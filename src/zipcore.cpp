// zipcore.cpp - ZIP 엔진 구현. 압축/해제 알고리즘은 miniz(deflate)만 사용하고
// 컨테이너(분할 .z01/.zip, ZIP64, 한글 파일명)는 직접 처리한다.
#include "zipcore.h"
#include "miniz.h"
#include "zeng.h"
#include "zipcrypt.h"
#include "fastcrc.h"
#include <algorithm>
#include <deque>
#include <map>
#include <cstring>
#include <cwctype>

static const size_t BUF = 1 << 18;  // 256KB

// ---------------------------------------------------------------- 유틸
std::wstring Utf8ToW(const char* s, int n) {
    if (n <= 0) return L"";
    int m = MultiByteToWideChar(CP_UTF8, 0, s, n, nullptr, 0);
    std::wstring w(m, 0);
    MultiByteToWideChar(CP_UTF8, 0, s, n, &w[0], m);
    return w;
}
static std::wstring CpToW(UINT cp, const char* s, int n) {
    if (n <= 0) return L"";
    int m = MultiByteToWideChar(cp, 0, s, n, nullptr, 0);
    if (m <= 0) return Utf8ToW(s, n);
    std::wstring w(m, 0);
    MultiByteToWideChar(cp, 0, s, n, &w[0], m);
    return w;
}
static bool IsValidUtf8(const uint8_t* s, int n) {
    bool high = false;
    for (int i = 0; i < n;) {
        uint8_t c = s[i];
        if (c < 0x80) { i++; continue; }
        high = true;
        int k = (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 ? 3 : -1;
        if (k < 0) return false;
        for (int j = 1; j <= k; j++) if (i + j >= n || (s[i + j] & 0xC0) != 0x80) return false;
        i += k + 1;
    }
    (void)high;
    return true;  // ASCII 포함 유효한 UTF-8
}
std::string WToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int m = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(m, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], m, nullptr, nullptr);
    return s;
}
std::wstring LongPath(const std::wstring& p) {
    if (p.size() < 240 || p.compare(0, 4, L"\\\\?\\") == 0) return p;
    if (p.compare(0, 2, L"\\\\") == 0) return L"\\\\?\\UNC\\" + p.substr(2);
    return L"\\\\?\\" + p;
}
bool FileExists(const std::wstring& p) {
    DWORD a = GetFileAttributesW(LongPath(p).c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}
bool DirExists(const std::wstring& p) {
    DWORD a = GetFileAttributesW(LongPath(p).c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}
std::wstring FileNameOf(const std::wstring& p) {
    size_t s = p.find_last_of(L"\\/");
    return s == std::wstring::npos ? p : p.substr(s + 1);
}
std::wstring ParentOf(const std::wstring& p) {
    size_t s = p.find_last_of(L"\\/");
    return s == std::wstring::npos ? L"" : p.substr(0, s);
}
static std::wstring Lower(std::wstring s) {
    if (!s.empty()) CharLowerBuffW(&s[0], (DWORD)s.size());
    return s;
}

static inline uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static inline uint32_t rd32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static inline uint64_t rd64(const uint8_t* p) { return (uint64_t)rd32(p) | ((uint64_t)rd32(p + 4) << 32); }

struct Bytes : std::vector<uint8_t> {
    void u16(uint32_t v) { push_back(v & 0xFF); push_back((v >> 8) & 0xFF); }
    void u32(uint32_t v) { u16(v & 0xFFFF); u16(v >> 16); }
    void u64(uint64_t v) { u32((uint32_t)v); u32((uint32_t)(v >> 32)); }
    void str(const std::string& s) { insert(end(), s.begin(), s.end()); }
};

static void FileTimeToDos(const FILETIME& ft, uint16_t& d, uint16_t& t) {
    FILETIME lt;
    WORD dd = 0x21, tt = 0;
    if (FileTimeToLocalFileTime(&ft, &lt) && FileTimeToDosDateTime(&lt, &dd, &tt)) { d = dd; t = tt; }
    else { d = 0x21; t = 0; }
}
static bool DosToFileTime(uint16_t d, uint16_t t, FILETIME& ft) {
    FILETIME lt;
    return DosDateTimeToFileTime(d, t, &lt) && LocalFileTimeToFileTime(&lt, &ft);
}

// ---------------------------------------------------------------- VolumeSet
bool VolumeSet::Open(const std::vector<std::wstring>& ps) {
    Close();
    uint64_t s = 0;
    for (auto& p : ps) {
        HANDLE h = CreateFileW(LongPath(p).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (h == INVALID_HANDLE_VALUE) { Close(); return false; }
        LARGE_INTEGER sz; GetFileSizeEx(h, &sz);
        hs.push_back(h); starts.push_back(s); sizes.push_back((uint64_t)sz.QuadPart);
        s += (uint64_t)sz.QuadPart;
    }
    total = s;
    return true;
}
void VolumeSet::Close() {
    for (HANDLE h : hs) CloseHandle(h);
    hs.clear(); starts.clear(); sizes.clear(); total = 0;
}
// OVERLAPPED 오프셋 지정 읽기: 파일 포인터를 공유하지 않아 여러 스레드에서 동시에 호출해도 안전
bool VolumeSet::Read(uint64_t off, void* buf, size_t n) {
    uint8_t* d = (uint8_t*)buf;
    size_t i = 0;
    while (n) {
        while (i < hs.size() && !(off >= starts[i] && off < starts[i] + sizes[i])) i++;
        if (i >= hs.size()) return false;
        uint64_t in = off - starts[i];
        size_t k = (size_t)std::min<uint64_t>(n, sizes[i] - in);
        size_t left = k; uint8_t* q = d;
        while (left) {
            DWORD want = (DWORD)std::min<size_t>(left, 1u << 30), got = 0;
            OVERLAPPED ov{};
            ov.Offset = (DWORD)in; ov.OffsetHigh = (DWORD)(in >> 32);
            if (!ReadFile(hs[i], q, want, &got, &ov) || got != want) return false;
            q += got; left -= got; in += got;
        }
        d += k; off += k; n -= k;
        i = 0;
    }
    return true;
}

// ---------------------------------------------------------------- 읽기
static std::wstring ZExt(unsigned i) {
    wchar_t b[16];
    wsprintfW(b, i < 100 ? L".z%02u" : L".z%u", i);
    return b;
}

// 분할 볼륨 찾기. raw(.001) 형식도 지원
static std::vector<std::wstring> FindVolumes(const std::wstring& path, std::wstring& mainPath) {
    std::vector<std::wstring> v;
    size_t dot = path.find_last_of(L'.'), sl = path.find_last_of(L"\\/");
    mainPath = path;
    if (dot == std::wstring::npos || (sl != std::wstring::npos && dot < sl)) { v.push_back(path); return v; }
    std::wstring ext = Lower(path.substr(dot + 1)), stem = path.substr(0, dot);
    bool zN = ext.size() >= 3 && ext[0] == L'z' && std::all_of(ext.begin() + 1, ext.end(), iswdigit);
    bool num = ext.size() >= 3 && std::all_of(ext.begin(), ext.end(), iswdigit);
    if (num) {  // name.zip.001, .002 ...
        size_t w = ext.size();
        for (unsigned i = 1;; i++) {
            std::wstring e = std::to_wstring(i);
            while (e.size() < w) e = L"0" + e;
            std::wstring p = stem + L"." + e;
            if (!FileExists(p)) break;
            v.push_back(p);
        }
        if (v.empty()) v.push_back(path);
        mainPath = v[0];
        return v;
    }
    if (zN) mainPath = stem + L".zip";
    else if (ext != L"zip") { v.push_back(path); return v; }
    for (unsigned i = 1;; i++) {
        std::wstring p = stem + ZExt(i);
        if (!FileExists(p)) break;
        v.push_back(p);
    }
    v.push_back(mainPath);
    return v;
}

void ZipArchive::Close() {
    vs.Close();
    entries.clear();
    volumes.clear();
    path.clear();
    password.clear();
    isSplit = false; splitSize = 0;
}

bool ZipArchive::Open(const std::wstring& p, std::wstring& err) {
    Close();
    std::wstring mainP;
    volumes = FindVolumes(p, mainP);
    if (!FileExists(mainP)) { err = L"분할 압축의 마지막 파일(.zip)을 찾을 수 없습니다."; return false; }
    if (!vs.Open(volumes)) { err = L"파일을 열 수 없습니다."; return false; }
    path = mainP;
    isSplit = volumes.size() > 1;
    splitSize = isSplit ? vs.sizes[0] : 0;
    uint64_t total = vs.total;
    if (total < 22) { err = L"ZIP 형식의 파일이 아닙니다."; return false; }

    size_t tail = (size_t)std::min<uint64_t>(total, 65535 + 22);
    std::vector<uint8_t> t(tail);
    if (!vs.Read(total - tail, t.data(), tail)) { err = L"파일을 읽을 수 없습니다."; return false; }
    int64_t pos = -1;
    for (int64_t i = (int64_t)tail - 22; i >= 0; i--) {
        if (rd32(&t[(size_t)i]) == 0x06054b50) {
            uint16_t cl = rd16(&t[(size_t)i + 20]);
            if ((uint64_t)i + 22 + cl <= tail) { pos = i; break; }
        }
    }
    if (pos < 0) { err = L"ZIP 형식의 파일이 아니거나 손상되었습니다."; return false; }
    const uint8_t* e = &t[(size_t)pos];
    uint64_t eocdG = total - tail + (uint64_t)pos;
    uint32_t thisDisk = rd16(e + 4), cdDisk = rd16(e + 6);
    uint64_t nEnt = rd16(e + 10), cdSize = rd32(e + 12), cdOff = rd32(e + 16);
    uint64_t cdEndActual = eocdG;

    auto Global = [&](uint32_t d, uint64_t o, uint64_t delta, uint64_t& g) -> bool {
        if (thisDisk == 0) { g = o + delta; return g < total; }
        if (d >= vs.starts.size()) return false;
        g = vs.starts[d] + o;
        return g < total;
    };

    if (eocdG >= 20) {
        uint8_t l[20];
        if (vs.Read(eocdG - 20, l, 20) && rd32(l) == 0x07064b50) {
            uint32_t zd = rd32(l + 4); uint64_t zo = rd64(l + 8);
            uint64_t zg = 0;
            bool ok = false;
            if (thisDisk == 0xFFFF || rd32(l + 16) > 1) { // 다중 디스크
                if (zd < vs.starts.size()) { zg = vs.starts[zd] + zo; ok = true; }
            } else { zg = zo; ok = true; }
            uint8_t z[56];
            if (ok && zg + 56 <= total && vs.Read(zg, z, 56) && rd32(z) == 0x06064b50) {
                thisDisk = rd32(z + 16); cdDisk = rd32(z + 20);
                nEnt = rd64(z + 32); cdSize = rd64(z + 40); cdOff = rd64(z + 48);
                cdEndActual = zg;
            } else if (thisDisk == 0) {
                // 앞에 데이터가 붙은 경우(SFX): 직전 영역에서 zip64 EOCD 탐색
                if (eocdG >= 76 && vs.Read(eocdG - 76, z, 56) && rd32(z) == 0x06064b50) {
                    thisDisk = rd32(z + 16); cdDisk = rd32(z + 20);
                    nEnt = rd64(z + 32); cdSize = rd64(z + 40); cdOff = rd64(z + 48);
                    cdEndActual = eocdG - 76;
                }
            }
        }
    }
    if (thisDisk > 0 && thisDisk + 1 != volumes.size()) {
        wchar_t b[200];
        wsprintfW(b, L"분할 압축 파일 일부가 없습니다. (필요 %u개, 찾음 %u개)", thisDisk + 1, (unsigned)volumes.size());
        err = b;
        return false;
    }
    uint64_t delta = 0;
    if (thisDisk == 0 && cdEndActual > cdOff + cdSize) delta = cdEndActual - (cdOff + cdSize);

    uint64_t cdG = 0;
    if (!Global(cdDisk, cdOff, delta, cdG) && !(nEnt == 0)) { err = L"중앙 디렉터리 위치가 잘못되었습니다."; return false; }
    if (cdSize > (1ull << 31)) { err = L"중앙 디렉터리가 너무 큽니다."; return false; }
    std::vector<uint8_t> cd((size_t)cdSize);
    if (cdSize && !vs.Read(cdG, cd.data(), (size_t)cdSize)) { err = L"중앙 디렉터리를 읽을 수 없습니다."; return false; }

    size_t p2 = 0;
    entries.reserve((size_t)std::min<uint64_t>(nEnt, 1000000));
    while (p2 + 46 <= cd.size()) {
        const uint8_t* c = &cd[p2];
        if (rd32(c) != 0x02014b50) break;
        ZipEntry z;
        uint16_t madeBy = rd16(c + 4);
        z.flags = rd16(c + 8); z.method = rd16(c + 10);
        z.dosTime = rd16(c + 12); z.dosDate = rd16(c + 14);
        z.crc = rd32(c + 16);
        uint64_t cs = rd32(c + 20), us = rd32(c + 24);
        uint16_t n = rd16(c + 28), x = rd16(c + 30), m = rd16(c + 32);
        uint32_t disk = rd16(c + 34);
        z.extAttr = rd32(c + 38);
        uint64_t off = rd32(c + 42);
        if (p2 + 46 + n + x + m > cd.size()) break;
        const char* nm = (const char*)c + 46;
        const uint8_t* ex = c + 46 + n;
        std::wstring uname;
        for (size_t q = 0; q + 4 <= x;) {
            uint16_t id = rd16(ex + q), sz = rd16(ex + q + 2);
            const uint8_t* d = ex + q + 4;
            if (q + 4 + sz > x) break;
            if (id == 0x0001) {
                size_t k = 0;
                if (us == 0xFFFFFFFF && k + 8 <= sz) { us = rd64(d + k); k += 8; }
                if (cs == 0xFFFFFFFF && k + 8 <= sz) { cs = rd64(d + k); k += 8; }
                if (off == 0xFFFFFFFF && k + 8 <= sz) { off = rd64(d + k); k += 8; }
                if (disk == 0xFFFF && k + 4 <= sz) { disk = rd32(d + k); k += 4; }
            } else if (id == 0x9901 && sz >= 7) {
                z.aesVersion = (uint8_t)rd16(d);
                z.aesStrength = d[4];
                z.aesMethod = rd16(d + 5);
            } else if (id == 0x7075 && sz > 5 && d[0] == 1) {
                uname = Utf8ToW((const char*)d + 5, sz - 5);
            }
            q += 4 + sz;
        }
        if (g_zipNameCP && !(z.flags & 0x0800) && uname.empty())  // 사용자가 고른 글자 코드로 강제
            z.name = g_zipNameCP == 65001 ? Utf8ToW(nm, n) : CpToW((UINT)g_zipNameCP, nm, n);
        else if (!uname.empty()) z.name = uname;
        else if (IsValidUtf8((const uint8_t*)nm, n)) z.name = Utf8ToW(nm, n);  // UTF-8 플래그가 거짓인 파일 대비
        else {
            // 한국어 Windows는 OEM=949. 영문 Windows(437)에서도 한글 압축 파일이 깨지지 않도록 949 우선 시도
            UINT oem = GetOEMCP();
            if (oem == 437 && n > 0 && MultiByteToWideChar(949, MB_ERR_INVALID_CHARS, nm, n, nullptr, 0) > 0) oem = 949;
            z.name = CpToW(oem, nm, n);
        }
        for (auto& ch : z.name) if (ch == L'\\') ch = L'/';
        while (!z.name.empty() && z.name[0] == L'/') z.name.erase(0, 1);
        while (z.name.compare(0, 2, L"./") == 0) z.name.erase(0, 2);
        if (!z.name.empty() && z.name.back() == L'/') z.isDir = true;
        while (!z.name.empty() && z.name.back() == L'/') z.name.pop_back();
        if ((madeBy >> 8) == 0 && (z.extAttr & 0x10)) z.isDir = true;
        z.usize = us; z.csize = cs;
        if (!Global(disk, off, delta, z.localOffset)) { err = L"항목 위치가 잘못되었습니다: " + z.name; return false; }
        if (!z.name.empty()) entries.push_back(z);
        p2 += 46 + n + x + m;
    }
    return true;
}

bool ZipArchive::DataOffset(const ZipEntry& e, uint64_t& off) {
    uint8_t h[30];
    if (!vs.Read(e.localOffset, h, 30) || rd32(h) != 0x04034b50) return false;
    off = e.localOffset + 30 + rd16(h + 26) + rd16(h + 28);
    return true;
}

// ---------------------------------------------------------------- 쓰기
// 임시 볼륨 파일들을 최종 이름으로. numeric: name.ext.001, .002 … (조각이 하나면 name.ext)
bool FinalizeTempVolumes(const std::vector<std::wstring>& tmp, const std::wstring& target, bool numeric, std::wstring& err) {
    auto numName = [&](size_t i) { wchar_t b[16]; wsprintfW(b, L".%03u", (unsigned)i); return target + b; };
    for (unsigned i = 1; i < 100000; i++) {  // 이전 조각 정리
        std::wstring p = numName(i);
        if (!FileExists(p)) break;
        DeleteFileW(LongPath(p).c_str());
    }
    if (tmp.size() > 1) DeleteFileW(LongPath(target).c_str());
    for (size_t i = 0; i < tmp.size(); i++) {
        std::wstring dst = tmp.size() == 1 ? target : numName(i + 1);
        if (!MoveFileExW(LongPath(tmp[i]).c_str(), LongPath(dst).c_str(), MOVEFILE_REPLACE_EXISTING)) {
            err = L"파일 이름을 바꿀 수 없습니다: " + dst;
            return false;
        }
    }
    (void)numeric;
    return true;
}

class VolWriter {
public:
    std::wstring base;
    uint64_t split = 0, pos = 0;
    std::vector<std::wstring> vols;
    HANDLE h = INVALID_HANDLE_VALUE;
    ~VolWriter() { if (h != INVALID_HANDLE_VALUE) CloseHandle(h); }

    bool Begin(const std::wstring& b, uint64_t sp) { base = b; split = sp; return NewVol(); }
    std::wstring TmpName(size_t i) { return base + L".~tmp" + std::to_wstring(i + 1); }
    uint32_t Disk() const { return (uint32_t)(vols.size() - 1); }
    bool NewVol() {
        if (h != INVALID_HANDLE_VALUE) { CloseHandle(h); h = INVALID_HANDLE_VALUE; }
        std::wstring n = TmpName(vols.size());
        h = CreateFileW(LongPath(n).c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return false;
        vols.push_back(n);
        pos = 0;
        return true;
    }
    bool Raw(const uint8_t* d, size_t n) {
        while (n) {
            DWORD k = (DWORD)std::min<size_t>(n, 1u << 30), w = 0;
            if (!WriteFile(h, d, k, &w, nullptr) || w != k) return false;
            d += k; n -= k; pos += k;
        }
        return true;
    }
    bool Write(const void* p, size_t n) {
        const uint8_t* d = (const uint8_t*)p;
        while (n) {
            if (split && pos >= split && !NewVol()) return false;
            size_t k = split ? (size_t)std::min<uint64_t>(n, split - pos) : n;
            if (!Raw(d, k)) return false;
            d += k; n -= k;
        }
        return true;
    }
    bool Write(const Bytes& b) { return Write(b.data(), b.size()); }
    bool Room(size_t n) {  // 레코드가 볼륨 경계를 넘지 않도록
        if (split && pos > 0 && pos + n > split) return NewVol();
        return true;
    }
    bool Patch(uint32_t disk, uint64_t off, const void* p, size_t n) {
        LARGE_INTEGER li; li.QuadPart = (LONGLONG)off;
        DWORD w = 0;
        if (disk == Disk()) {
            LARGE_INTEGER z{};
            bool ok = SetFilePointerEx(h, li, nullptr, FILE_BEGIN) && WriteFile(h, p, (DWORD)n, &w, nullptr) && w == n;
            SetFilePointerEx(h, z, nullptr, FILE_END);
            return ok;
        }
        HANDLE f = CreateFileW(LongPath(vols[disk]).c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (f == INVALID_HANDLE_VALUE) return false;
        bool ok = SetFilePointerEx(f, li, nullptr, FILE_BEGIN) && WriteFile(f, p, (DWORD)n, &w, nullptr) && w == n;
        CloseHandle(f);
        return ok;
    }
    void Close() { if (h != INVALID_HANDLE_VALUE) { CloseHandle(h); h = INVALID_HANDLE_VALUE; } }
    void Abort() { Close(); for (auto& v : vols) DeleteFileW(LongPath(v).c_str()); vols.clear(); }
};

static std::wstring StemOf(const std::wstring& target) {
    if (target.size() > 4 && Lower(target.substr(target.size() - 4)) == L".zip") return target.substr(0, target.size() - 4);
    return target;
}

// 임시 볼륨을 최종 이름으로 (분할: name.z01 .. name.zip)
static bool FinalizeVolumes(VolWriter& w, const std::wstring& target, std::wstring& err) {
    std::wstring stem = StemOf(target);
    for (unsigned i = 1; i < 10000; i++) {  // 이전에 남아 있던 분할 조각 제거
        std::wstring p = stem + ZExt(i);
        if (!FileExists(p)) break;
        DeleteFileW(LongPath(p).c_str());
    }
    size_t k = w.vols.size();
    for (size_t i = 0; i < k; i++) {
        std::wstring dst = (i + 1 == k) ? target : stem + ZExt((unsigned)i + 1);
        if (!MoveFileExW(LongPath(w.vols[i]).c_str(), LongPath(dst).c_str(), MOVEFILE_REPLACE_EXISTING)) {
            err = L"파일 이름을 바꿀 수 없습니다: " + dst;
            return false;
        }
    }
    w.vols.clear();
    return true;
}

struct CdRec {
    std::string name;
    uint16_t flags, method, time, date;
    uint32_t crc, extAttr;
    uint64_t cs, us, off;
    uint32_t disk;
    bool z64;
    uint8_t aesStr, aesVer;
    uint16_t aesMeth;
};
static void AesExtra(Bytes& b, const CdRec& r) {
    if (!r.aesStr) return;
    b.u16(0x9901); b.u16(7); b.u16(r.aesVer ? r.aesVer : 2); b.push_back('A'); b.push_back('E');
    b.push_back(r.aesStr); b.u16(r.aesMeth);
}

// ---------------------------------------------------------------- 병렬 압축
// 파일을 1MB 블록으로 나눠 여러 코어에서 각각 독립된 raw deflate 스트림으로 압축하고
// (마지막 블록만 FINISH, 나머지는 SYNC_FLUSH 로 바이트 정렬) 순서대로 이어 붙인다. (pigz 방식)
// 블록마다 CRC 를 따로 구해 crc32_combine 으로 합친다.
int g_zipLevel = 3;
int g_zipThreads = 0;  // 쓸 코어 수 (0 = 자동)
static int ZipThreads(DWORD cores, int cap) {
    int n = g_zipThreads > 0 ? g_zipThreads : (int)cores;
    return std::max(1, std::min(n, cap));
}
int g_zipNameCP = 0;
static const size_t BLOCK = 1 << 20;

static uint32_t gf2_times(const uint32_t* mat, uint32_t vec) {
    uint32_t sum = 0;
    while (vec) { if (vec & 1) sum ^= *mat; vec >>= 1; mat++; }
    return sum;
}
static void gf2_square(uint32_t* sq, const uint32_t* mat) { for (int n = 0; n < 32; n++) sq[n] = gf2_times(mat, mat[n]); }
static uint32_t crc32_combine(uint32_t c1, uint32_t c2, uint64_t len2) {
    if (len2 == 0) return c1;
    uint32_t even[32], odd[32];
    odd[0] = 0xedb88320u;
    uint32_t row = 1;
    for (int n = 1; n < 32; n++) { odd[n] = row; row <<= 1; }
    gf2_square(even, odd);
    gf2_square(odd, even);
    do {
        gf2_square(even, odd);
        if (len2 & 1) c1 = gf2_times(even, c1);
        len2 >>= 1;
        if (!len2) break;
        gf2_square(odd, even);
        if (len2 & 1) c1 = gf2_times(odd, c1);
        len2 >>= 1;
    } while (len2);
    return c1 ^ c2;
}

// 이미 압축된 형식: 다시 압축해도 줄지 않으므로 저장만 한다
static bool IsPrecompressed(const std::wstring& name) {
    static const wchar_t* ext[] = {L"jpg", L"jpeg", L"png", L"gif", L"webp", L"heic", L"heif", L"avif", L"jxl",
        L"mp3", L"m4a", L"aac", L"ogg", L"opus", L"flac", L"wma", L"mp4", L"m4v", L"mkv", L"avi", L"mov", L"wmv",
        L"webm", L"flv", L"3gp", L"ts", L"zip", L"7z", L"rar", L"gz", L"tgz", L"bz2", L"xz", L"zst", L"lz", L"lzma",
        L"cab", L"alz", L"egg", L"jar", L"apk", L"ipa", L"msi", L"docx", L"xlsx", L"pptx", L"hwpx", L"odt", L"ods",
        L"odp", L"epub", L"woff", L"woff2", L"z01", L"z02", L"z03"};
    size_t d = name.find_last_of(L'.');
    if (d == std::wstring::npos) return false;
    std::wstring e = Lower(name.substr(d + 1));
    for (auto x : ext) if (e == x) return true;
    return false;
}

// 내용 검사: 64KB 표본을 가장 빠른 설정으로 압축해 보고 3% 도 안 줄면 '압축 안 되는 데이터'로 본다
// (확장자가 없거나 바뀐 동영상·사진·압축 파일, 암호화된 파일 등도 잡아낸다)
bool LooksIncompressible(const uint8_t* p, size_t n) {
    if (n > 65536) n = 65536;
    if (n < 4096) return false;
    std::vector<uint8_t> out(n);
    size_t got = tdefl_compress_mem_to_mem(out.data(), n, p, n, tdefl_create_comp_flags_from_zip_params(1, -15, MZ_DEFAULT_STRATEGY));
    return got == 0 || got > n * 97 / 100;
}
bool IsPrecompressedName(const std::wstring& name) { return IsPrecompressed(name); }
bool FileLooksIncompressible(const std::wstring& path, uint64_t size) {
    if (size < 256 * 1024) return false;  // 작은 파일은 그냥 압축
    HANDLE h = CreateFileW(LongPath(path).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    std::vector<uint8_t> b(65536);
    LARGE_INTEGER li; li.QuadPart = (LONGLONG)(size / 2);  // 머리글을 피해 가운데에서 표본
    DWORD got = 0;
    bool r = SetFilePointerEx(h, li, nullptr, FILE_BEGIN) && ReadFile(h, b.data(), 65536, &got, nullptr) && LooksIncompressible(b.data(), got);
    CloseHandle(h);
    return r;
}

struct Blk {
    size_t item = 0;
    bool first = false, last = false, store = false, work = false;
    bool decide = false;  // 블록 하나짜리 파일: '저장만' 판단을 작업 스레드에서 (파일이 많을 때 읽기 스레드가 막히지 않게)
    std::vector<uint8_t> in, out;
    uint32_t crc = 0;
    volatile LONG done = 0;
    bool fail = false;
};

class Pool {
public:
    SRWLOCK lk = SRWLOCK_INIT;
    CONDITION_VARIABLE cvWork = CONDITION_VARIABLE_INIT, cvDone = CONDITION_VARIABLE_INIT;
    std::deque<Blk*> q;
    std::vector<HANDLE> th;
    bool quit = false;
    int level = 3;
    static DWORD WINAPI Run(LPVOID p) { ((Pool*)p)->Loop(); return 0; }
    void Start(int n) { for (int i = 0; i < n; i++) th.push_back(CreateThread(nullptr, 0, Run, this, 0, nullptr)); }
    void Stop() {
        AcquireSRWLockExclusive(&lk); quit = true; ReleaseSRWLockExclusive(&lk);
        WakeAllConditionVariable(&cvWork);
        if (!th.empty()) WaitForMultipleObjects((DWORD)th.size(), th.data(), TRUE, INFINITE);
        for (HANDLE h : th) CloseHandle(h);
        th.clear();
    }
    void Push(Blk* b) {
        AcquireSRWLockExclusive(&lk); q.push_back(b); ReleaseSRWLockExclusive(&lk);
        WakeConditionVariable(&cvWork);
    }
    void Wait(Blk* b) {
        AcquireSRWLockExclusive(&lk);
        while (!b->done) SleepConditionVariableSRW(&cvDone, &lk, INFINITE, 0);
        ReleaseSRWLockExclusive(&lk);
    }
    void Loop() {
        ZEng* s = ZDefNew(level);
        bool init = s != nullptr;
        for (;;) {
            AcquireSRWLockExclusive(&lk);
            while (q.empty() && !quit) SleepConditionVariableSRW(&cvWork, &lk, INFINITE, 0);
            if (q.empty()) { ReleaseSRWLockExclusive(&lk); break; }
            Blk* b = q.front(); q.pop_front();
            ReleaseSRWLockExclusive(&lk);
            Compress(b, s, init);
            AcquireSRWLockExclusive(&lk); b->done = 1; ReleaseSRWLockExclusive(&lk);
            WakeAllConditionVariable(&cvDone);
        }
        ZDefFree(s);
    }
    // deflate '저장 블록'으로 그대로 감싸기 (압축 안 되는 블록: 거의 복사 속도)
    static void StoredDeflate(Blk* b) {
        const uint8_t* p = b->in.data();
        size_t n = b->in.size();
        b->out.clear();
        b->out.reserve(n + (n / 65535 + 1) * 5);
        do {
            size_t k = n > 65535 ? 65535 : n;
            bool fin = b->last && k == n;
            b->out.push_back(fin ? 1 : 0);
            b->out.push_back((uint8_t)k); b->out.push_back((uint8_t)(k >> 8));
            b->out.push_back((uint8_t)~k); b->out.push_back((uint8_t)(~k >> 8));
            b->out.insert(b->out.end(), p, p + k);
            p += k; n -= k;
        } while (n);
    }
    static void Compress(Blk* b, ZEng* s, bool init) {
        b->crc = FastCrc32(0, b->in.data(), b->in.size());
        if (b->decide && LooksIncompressible(b->in.data() + (b->in.size() - 65536) / 2, 65536)) b->store = true;
        if (b->store) return;
        if (b->in.size() >= 128 * 1024 && LooksIncompressible(b->in.data(), b->in.size())) { StoredDeflate(b); return; }
        if (!init || !ZDefReset(s)) { b->fail = true; return; }
        b->out.resize(b->in.size() + b->in.size() / 8 + 1024);
        const uint8_t* in = b->in.data();
        uint32_t ai = (uint32_t)b->in.size();
        size_t outPos = 0;
        for (;;) {
            uint8_t* out = b->out.data() + outPos;
            uint32_t ao = (uint32_t)(b->out.size() - outPos);
            int st = ZDefRun(s, in, ai, out, ao, b->last);
            outPos = b->out.size() - ao;
            if (st == ZE_END) break;
            if (st == ZE_ERR) { b->fail = true; return; }
            if (!b->last && ai == 0 && ao > 0) break;  // SYNC_FLUSH 완료
            if (ao == 0) b->out.resize(b->out.size() * 2);
        }
        b->out.resize(outPos);
    }
};

// 파일 하나를 gzip 으로 (멀티코어). 분할 시 target.001 …  진행률은 job.done 에 더하기만 한다
bool GzipCompress(const std::wstring& src, const std::wstring& target, const std::wstring& innerName,
                  uint64_t splitSize, Job& job, std::wstring& err) {
    HANDLE fh = CreateFileW(LongPath(src).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (fh == INVALID_HANDLE_VALUE) { err = L"임시 파일을 열 수 없습니다."; return false; }
    LARGE_INTEGER fsz; GetFileSizeEx(fh, &fsz);
    VolWriter w;
    if (!w.Begin(target, splitSize)) { CloseHandle(fh); err = L"압축 파일을 만들 수 없습니다: " + target; return false; }
    Bytes h;
    FILETIME now; GetSystemTimeAsFileTime(&now);
    uint64_t t = (((uint64_t)now.dwHighDateTime << 32) | now.dwLowDateTime) / 10000000ull - 11644473600ull;
    h.push_back(0x1f); h.push_back(0x8b); h.push_back(8); h.push_back(innerName.empty() ? 0 : 8);
    h.u32((uint32_t)t); h.push_back(0); h.push_back(11 /*NTFS*/);
    if (!innerName.empty()) { h.str(WToUtf8(innerName)); h.push_back(0); }
    bool ok = w.Write(h);
    SYSTEM_INFO si; GetSystemInfo(&si);
    int nth = ZipThreads(si.dwNumberOfProcessors, 16);
    Pool pool;
    pool.level = std::max(1, std::min(9, g_zipLevel));
    pool.Start(nth);
    std::deque<Blk*> fifo;
    uint64_t left = (uint64_t)fsz.QuadPart, total = 0;
    uint32_t crc = 0;
    bool eof = left == 0;
    if (eof) { Blk* b = new Blk(); b->last = true; b->work = true; fifo.push_back(b); pool.Push(b); }
    while (ok && (!eof || !fifo.empty())) {
        while (ok && !eof && fifo.size() < (size_t)nth * 3) {
            Blk* b = new Blk();
            b->work = true;
            size_t want = (size_t)std::min<uint64_t>(left, BLOCK);
            b->in.resize(want);
            DWORD got = 0;
            if (!ReadFile(fh, b->in.data(), (DWORD)want, &got, nullptr)) { delete b; ok = false; err = L"읽기 실패"; break; }
            b->in.resize(got);
            left -= got;
            if (got < want) left = 0;
            b->last = left == 0;
            eof = b->last;
            fifo.push_back(b);
            pool.Push(b);
        }
        if (!ok || fifo.empty()) break;
        if (job.cancel) { ok = false; err = L"취소되었습니다."; break; }
        Blk* b = fifo.front();
        pool.Wait(b);
        fifo.pop_front();
        if (b->fail || !w.Write(b->out.data(), b->out.size())) { ok = false; if (err.empty()) err = L"쓰기 실패 (디스크 공간을 확인하세요)"; }
        crc = crc32_combine(crc, b->crc, b->in.size());
        total += b->in.size();
        job.done += b->in.size();
        delete b;
    }
    pool.Stop();
    for (Blk* b : fifo) delete b;
    CloseHandle(fh);
    if (ok) { Bytes tr; tr.u32(crc); tr.u32((uint32_t)total); ok = w.Write(tr); if (!ok) err = L"쓰기 실패"; }
    w.Close();
    if (!ok) { w.Abort(); return false; }
    if (!FinalizeTempVolumes(w.vols, target, true, err)) { w.Abort(); return false; }
    w.vols.clear();
    return true;
}

static bool WriteArchiveBody(VolWriter& w, std::vector<AddItem>& items, ZipArchive* src,
                             Job& job, std::wstring& err, const std::wstring& password = L"") {
    std::vector<CdRec> cd(items.size());
    std::string pwBytes = zc::PwUtf8(password);
    zc::Aes aes;
    std::vector<uint8_t> inb(BUF);
    if (w.split) { Bytes m; m.u32(0x08074b50); if (!w.Write(m)) { err = L"쓰기 실패"; return false; } }

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    int nth = ZipThreads(si.dwNumberOfProcessors, 16);
    Pool pool;
    pool.level = std::max(1, std::min(9, g_zipLevel));
    pool.Start(nth);
    const size_t MAXQ = (size_t)nth * 3;

    // 읽기 상태
    size_t rItem = 0;
    HANDLE rh = INVALID_HANDLE_VALUE;
    uint64_t rLeft = 0;
    bool rFirst = true, rStore = false;
    std::deque<Blk*> fifo;
    bool ok = true;

    auto cleanup = [&]() {
        pool.Stop();
        for (Blk* b : fifo) delete b;
        fifo.clear();
        if (rh != INVALID_HANDLE_VALUE) CloseHandle(rh);
        rh = INVALID_HANDLE_VALUE;
    };
    auto fail = [&](const std::wstring& m) { err = m; ok = false; };

    // 다음 블록 하나를 읽어 큐에 넣는다. 더 읽을 것이 없으면 false
    auto produce = [&]() -> bool {
        while (rItem < items.size()) {
            AddItem& it = items[rItem];
            bool hasData = it.copyIdx < 0 && !it.isDir && it.size > 0;
            if (!hasData) {  // 폴더, 빈 파일, 기존 항목 복사: 표시용 블록 하나
                Blk* b = new Blk();
                b->item = rItem; b->first = b->last = true; b->done = 1;
                fifo.push_back(b);
                rItem++;
                return true;
            }
            if (rh == INVALID_HANDLE_VALUE) {
                rh = CreateFileW(LongPath(it.src).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                 nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
                if (rh == INVALID_HANDLE_VALUE) { fail(L"파일을 열 수 없습니다: " + it.src); return false; }
                rLeft = it.size;
                rFirst = true;
            }
            Blk* b = new Blk();
            b->item = rItem; b->first = rFirst; b->work = true;
            b->store = rStore;
            size_t want = (size_t)std::min<uint64_t>(rLeft, BLOCK);
            b->in.resize(want);
            DWORD got = 0;
            if (want && (!ReadFile(rh, b->in.data(), (DWORD)want, &got, nullptr))) { delete b; fail(L"파일 읽기 실패: " + it.src); return false; }
            b->in.resize(got);
            if (rFirst) {  // 파일 첫 블록: 확장자 또는 내용으로 '저장만' 할지 결정 (파일 전체에 적용)
                rStore = IsPrecompressed(it.arcName);
                bool sample = !rStore && it.size >= 256 * 1024 && got >= 65536;
                if (sample && got == rLeft) b->decide = true;  // 이 블록으로 끝나는 파일: 작업 스레드가 판단
                else if (sample) rStore = LooksIncompressible(b->in.data() + (got - 65536) / 2, 65536);
                b->store = rStore;
            }
            rLeft -= got;
            rFirst = false;
            if (got < want) rLeft = 0;  // 파일이 줄어든 경우
            b->last = rLeft == 0;
            if (b->last) { CloseHandle(rh); rh = INVALID_HANDLE_VALUE; rItem++; }
            fifo.push_back(b);
            pool.Push(b);
            return true;
        }
        return false;
    };

    uint32_t curCrc = 0;
    uint64_t curUs = 0, curCs = 0;
    while (ok) {
        while (ok && fifo.size() < MAXQ && produce()) {}
        if (!ok || fifo.empty()) break;
        if (job.cancel) { fail(L"취소되었습니다."); break; }
        Blk* b = fifo.front();
        pool.Wait(b);
        fifo.pop_front();
        AddItem& it = items[b->item];
        CdRec& r = cd[b->item];
        bool copy = it.copyIdx >= 0;
        if (b->fail) { delete b; fail(L"압축 오류: " + it.arcName); break; }

        if (b->first) {
            uint64_t dataOff = 0;
            if (copy) {
                const ZipEntry& e = src->entries[(size_t)it.copyIdx];
                r.name = WToUtf8(it.arcName.empty() ? e.name : it.arcName) + (e.isDir ? "/" : "");
                r.flags = (uint16_t)((e.flags & ~0x0800) | 0x0800);
                r.method = e.method; r.time = e.dosTime; r.date = e.dosDate;
                r.crc = e.crc; r.cs = e.csize; r.us = e.usize; r.extAttr = e.extAttr;
                r.aesStr = e.aesStrength; r.aesVer = e.aesVersion; r.aesMeth = e.aesMethod;
                if (!src->DataOffset(e, dataOff)) { delete b; fail(L"기존 항목을 읽을 수 없습니다: " + e.name); break; }
                r.z64 = r.cs >= 0xFFFFFFFFull || r.us >= 0xFFFFFFFFull;
            } else {
                r.name = WToUtf8(it.arcName) + (it.isDir ? "/" : "");
                r.flags = 0x0800;
                FileTimeToDos(it.mtime, r.date, r.time);
                r.extAttr = it.isDir ? 0x10 : 0x20;
                r.method = (it.isDir || it.size == 0 || b->store) ? 0 : 8;
                r.us = it.size;
                r.z64 = !it.isDir && it.size >= 0xFF000000ull;
                r.aesStr = 0; r.aesVer = 0; r.aesMeth = 0;
                if (!pwBytes.empty() && !it.isDir && it.size > 0) {  // AES-256 (AE-2: CRC 대신 HMAC 으로 검증)
                    r.flags |= 1;
                    r.aesStr = 3; r.aesVer = 2; r.aesMeth = r.method;
                    r.method = 99;
                }
            }
            if (job.onFile) job.onFile(copy ? std::wstring() : it.arcName);
            Bytes h;
            h.u32(0x04034b50); h.u16(r.aesStr ? 51 : r.z64 ? 45 : 20); h.u16(r.flags); h.u16(r.method);
            h.u16(r.time); h.u16(r.date);
            h.u32(copy ? r.crc : 0);
            if (r.z64) { h.u32(0xFFFFFFFF); h.u32(0xFFFFFFFF); }
            else { h.u32(copy ? (uint32_t)r.cs : 0); h.u32(copy ? (uint32_t)r.us : 0); }
            h.u16((uint32_t)r.name.size()); h.u16((r.z64 ? 20 : 0) + (r.aesStr ? 11 : 0));
            h.str(r.name);
            if (r.z64) { h.u16(1); h.u16(16); h.u64(copy ? r.us : 0); h.u64(copy ? r.cs : 0); }
            AesExtra(h, r);
            if (!copy && r.aesStr) {  // 소금값 + 암호 확인값
                uint8_t salt[16], pv[2];
                if (!zc::RandomBytes(salt, 16) || !aes.Init(pwBytes, salt, 16, 256, pv)) { delete b; fail(L"암호화 모듈을 쓸 수 없습니다."); break; }
                h.insert(h.end(), salt, salt + 16); h.insert(h.end(), pv, pv + 2);
            }
            if (!w.Room(h.size())) { delete b; fail(L"쓰기 실패"); break; }
            r.disk = w.Disk(); r.off = w.pos;
            if (!w.Write(h)) { delete b; fail(L"쓰기 실패 (디스크 공간을 확인하세요)"); break; }
            curCrc = 0; curUs = 0; curCs = (!copy && r.aesStr) ? 18 : 0;

            if (copy) {
                uint64_t left = r.cs, o = dataOff;
                while (left && ok) {
                    if (job.cancel) { fail(L"취소되었습니다."); break; }
                    size_t k = (size_t)std::min<uint64_t>(left, BUF);
                    if (!src->vs.Read(o, inb.data(), k)) { fail(L"기존 압축 파일을 읽을 수 없습니다."); break; }
                    if (!w.Write(inb.data(), k)) { fail(L"쓰기 실패 (디스크 공간을 확인하세요)"); break; }
                    left -= k; o += k; job.done += k;
                }
                if (ok && (r.flags & 0x08)) {
                    Bytes d; d.u32(0x08074b50); d.u32(r.crc);
                    if (r.z64) { d.u64(r.cs); d.u64(r.us); } else { d.u32((uint32_t)r.cs); d.u32((uint32_t)r.us); }
                    if (!w.Write(d)) fail(L"쓰기 실패");
                }
            }
        }
        if (ok && b->work) {
            std::vector<uint8_t>& data = b->store ? b->in : b->out;
            if (r.aesStr && !copy) {
                if (b->store) { b->out = b->in; }  // 원본 CRC 는 이미 계산됨, 암호화는 복사본에
                std::vector<uint8_t>& enc = b->out;
                aes.Crypt(enc.data(), enc.size());
                aes.MacUpdate(enc.data(), enc.size());
                if (!enc.empty() && !w.Write(enc.data(), enc.size())) { delete b; fail(L"쓰기 실패 (디스크 공간을 확인하세요)"); break; }
            } else if (!data.empty() && !w.Write(data.data(), data.size())) { delete b; fail(L"쓰기 실패 (디스크 공간을 확인하세요)"); break; }
            curCrc = crc32_combine(curCrc, b->crc, b->in.size());
            curUs += b->in.size();
            curCs += data.size();
            job.done += b->in.size();
        }
        if (ok && b->last && !copy && b->work) {
            if (curUs >= 0xFFFFFFFFull && !r.z64) { delete b; fail(L"압축 중 파일 크기가 바뀌었습니다: " + it.src); break; }
            if (r.aesStr) {
                uint8_t mac[10];
                aes.MacFinal(mac);
                if (!w.Write(mac, 10)) { delete b; fail(L"쓰기 실패"); break; }
                curCs += 10;
                curCrc = 0;  // AE-2
            }
            r.crc = curCrc; r.us = curUs; r.cs = curCs;
            uint8_t pb[12];
            memcpy(pb, &curCrc, 4);
            if (!r.z64) {
                uint32_t c32 = (uint32_t)curCs, u32 = (uint32_t)curUs;
                memcpy(pb + 4, &c32, 4); memcpy(pb + 8, &u32, 4);
                if (!w.Patch(r.disk, r.off + 14, pb, 12)) { delete b; fail(L"헤더 갱신 실패"); break; }
            } else {
                uint8_t z[16];
                memcpy(z, &curUs, 8); memcpy(z + 8, &curCs, 8);
                if (!w.Patch(r.disk, r.off + 14, pb, 4) ||
                    !w.Patch(r.disk, r.off + 30 + r.name.size() + 4, z, 16)) { delete b; fail(L"헤더 갱신 실패"); break; }
            }
        }
        delete b;
    }
    cleanup();
    if (!ok) return false;

    // 중앙 디렉터리
    std::vector<uint32_t> recDisk;
    recDisk.reserve(cd.size());
    uint32_t cdDisk = w.Disk();
    uint64_t cdOff = w.pos, cdSize = 0;
    bool first = true;
    for (auto& r : cd) {
        bool bu = r.z64 || r.us >= 0xFFFFFFFFull, bc = r.z64 || r.cs >= 0xFFFFFFFFull;
        bool bo = r.off >= 0xFFFFFFFFull, bd = r.disk >= 0xFFFF;
        Bytes x;
        if (bu || bc || bo || bd) {
            Bytes d;
            if (bu) d.u64(r.us);
            if (bc) d.u64(r.cs);
            if (bo) d.u64(r.off);
            if (bd) d.u32(r.disk);
            x.u16(1); x.u16((uint32_t)d.size()); x.insert(x.end(), d.begin(), d.end());
        }
        bool z = !x.empty();
        AesExtra(x, r);
        Bytes c;
        uint16_t ver = r.aesStr ? 51 : z ? 45 : 20;
        c.u32(0x02014b50); c.u16(ver); c.u16(ver); c.u16(r.flags); c.u16(r.method);
        c.u16(r.time); c.u16(r.date); c.u32(r.crc);
        c.u32(bc ? 0xFFFFFFFF : (uint32_t)r.cs); c.u32(bu ? 0xFFFFFFFF : (uint32_t)r.us);
        c.u16((uint32_t)r.name.size()); c.u16((uint32_t)x.size()); c.u16(0);
        c.u16(bd ? 0xFFFF : r.disk); c.u16(0); c.u32(r.extAttr);
        c.u32(bo ? 0xFFFFFFFF : (uint32_t)r.off);
        c.str(r.name); c.insert(c.end(), x.begin(), x.end());
        if (!w.Room(c.size())) { err = L"쓰기 실패"; return false; }
        if (first) { cdDisk = w.Disk(); cdOff = w.pos; first = false; }
        recDisk.push_back(w.Disk());
        if (!w.Write(c)) { err = L"쓰기 실패 (디스크 공간을 확인하세요)"; return false; }
        cdSize += c.size();
    }
    bool z64 = cd.size() >= 0xFFFF || cdSize >= 0xFFFFFFFFull || cdOff >= 0xFFFFFFFFull || w.Disk() >= 0xFFFF;
    if (!w.Room(z64 ? 98 : 22)) { err = L"쓰기 실패"; return false; }
    if (first) { cdDisk = w.Disk(); cdOff = w.pos; }
    uint32_t last = w.Disk();
    uint64_t onLast = 0;
    for (uint32_t d : recDisk) if (d == last) onLast++;
    Bytes e;
    if (z64) {
        uint64_t zOff = w.pos;
        e.u32(0x06064b50); e.u64(44); e.u16(45); e.u16(45);
        e.u32(last); e.u32(cdDisk); e.u64(onLast); e.u64(cd.size()); e.u64(cdSize); e.u64(cdOff);
        e.u32(0x07064b50); e.u32(last); e.u64(zOff); e.u32(last + 1);
    }
    e.u32(0x06054b50);
    e.u16(std::min<uint32_t>(last, 0xFFFF)); e.u16(std::min<uint32_t>(cdDisk, 0xFFFF));
    e.u16((uint32_t)std::min<uint64_t>(onLast, 0xFFFF)); e.u16((uint32_t)std::min<uint64_t>(cd.size(), 0xFFFF));
    e.u32((uint32_t)std::min<uint64_t>(cdSize, 0xFFFFFFFF)); e.u32((uint32_t)std::min<uint64_t>(cdOff, 0xFFFFFFFF));
    e.u16(0);
    if (!w.Write(e)) { err = L"쓰기 실패 (디스크 공간을 확인하세요)"; return false; }
    if (w.split && w.vols.size() == 1) {  // 결과가 한 조각이면 분할 표시를 '임시 표시(PK00)'로
        uint8_t pk00[4] = {'P', 'K', '0', '0'};
        w.Patch(0, 0, pk00, 4);
    }
    w.Close();
    return true;
}

void CollectItems(const std::vector<std::wstring>& paths, const std::wstring& prefix,
                  const std::vector<std::wstring>& exclude, std::vector<AddItem>& out) {
    auto excluded = [&](const std::wstring& p) {
        for (auto& x : exclude) if (lstrcmpiW(x.c_str(), p.c_str()) == 0) return true;
        return false;
    };
    std::function<void(const std::wstring&, const std::wstring&)> rec =
        [&](const std::wstring& dir, const std::wstring& arc) {
            WIN32_FIND_DATAW fd;
            HANDLE f = FindFirstFileExW(LongPath(dir + L"\\*").c_str(), FindExInfoBasic, &fd,
                                        FindExSearchNameMatch, nullptr, 2 /*FIND_FIRST_EX_LARGE_FETCH*/);
            if (f == INVALID_HANDLE_VALUE) return;
            std::vector<std::pair<std::wstring, WIN32_FIND_DATAW>> list;
            do {
                if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
                list.push_back({fd.cFileName, fd});
            } while (FindNextFileW(f, &fd));
            FindClose(f);
            for (auto& l : list) {
                std::wstring full = dir + L"\\" + l.first;
                AddItem it;
                it.src = full; it.arcName = arc + l.first; it.mtime = l.second.ftLastWriteTime;
                if (l.second.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    if (l.second.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;  // 정션 순환 방지
                    it.isDir = true;
                    out.push_back(it);
                    rec(full, it.arcName + L"/");
                } else {
                    if (excluded(full)) continue;
                    it.size = ((uint64_t)l.second.nFileSizeHigh << 32) | l.second.nFileSizeLow;
                    out.push_back(it);
                }
            }
        };
    for (auto p : paths) {
        while (p.size() > 3 && (p.back() == L'\\' || p.back() == L'/')) p.pop_back();
        WIN32_FILE_ATTRIBUTE_DATA a;
        if (!GetFileAttributesExW(LongPath(p).c_str(), GetFileExInfoStandard, &a)) continue;
        AddItem it;
        it.src = p; it.arcName = prefix + FileNameOf(p); it.mtime = a.ftLastWriteTime;
        if (a.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            it.isDir = true;
            if (it.arcName.empty() || it.arcName.back() == L':') it.arcName = prefix + L"드라이브";
            out.push_back(it);
            rec(p, it.arcName + L"/");
        } else {
            if (excluded(p)) continue;
            it.size = ((uint64_t)a.nFileSizeHigh << 32) | a.nFileSizeLow;
            out.push_back(it);
        }
    }
}

bool CreateZip(const std::wstring& target, std::vector<AddItem>& items, uint64_t splitSize,
               Job& job, std::wstring& err, const std::wstring& password) {
    uint64_t total = 0;
    for (auto& i : items) total += i.size;
    job.total = total; job.done = 0;
    VolWriter w;
    if (!w.Begin(target, splitSize)) { err = L"압축 파일을 만들 수 없습니다: " + target; return false; }
    if (!WriteArchiveBody(w, items, nullptr, job, err, password)) { w.Abort(); return false; }
    if (!FinalizeVolumes(w, target, err)) { w.Abort(); return false; }
    return true;
}

static bool RewriteZip(ZipArchive& arc, std::vector<AddItem>& all, Job& job, std::wstring& err, const std::wstring& password);
bool AddToZip(ZipArchive& arc, std::vector<AddItem>& items, Job& job, std::wstring& err, const std::wstring& password) {
    // 같은 이름(대소문자 무시)의 기존 항목은 교체
    std::vector<AddItem> all;
    auto key = [](const std::wstring& s) { return Lower(s); };
    std::vector<std::wstring> newKeys;
    for (auto& i : items) newKeys.push_back(key(i.arcName));
    std::sort(newKeys.begin(), newKeys.end());
    uint64_t total = 0;
    for (size_t i = 0; i < arc.entries.size(); i++) {
        auto& e = arc.entries[i];
        bool replaced = std::binary_search(newKeys.begin(), newKeys.end(), key(e.name));
        if (replaced && !e.isDir) continue;
        if (replaced && e.isDir) {  // 같은 폴더 항목은 새 쪽을 버린다
            for (auto& it : items) if (it.isDir && key(it.arcName) == key(e.name)) it.arcName.clear();
        }
        AddItem c; c.copyIdx = (int)i; c.arcName = e.name; c.isDir = e.isDir;
        all.push_back(c);
        total += e.csize;
    }
    for (auto& i : items) if (!i.arcName.empty()) { all.push_back(i); total += i.size; }
    job.total = total; job.done = 0;
    return RewriteZip(arc, all, job, err, password);
}

// 기존 항목 복사(+새 항목)로 압축 파일을 다시 쓰고 원래 파일과 바꾼다
static bool RewriteZip(ZipArchive& arc, std::vector<AddItem>& all, Job& job, std::wstring& err, const std::wstring& password) {
    std::wstring target = arc.path;
    uint64_t split = arc.isSplit ? arc.splitSize : 0;
    std::vector<std::wstring> oldVols = arc.volumes;
    VolWriter w;
    if (!w.Begin(target, split)) { err = L"임시 파일을 만들 수 없습니다. 쓰기 권한을 확인하세요."; return false; }
    if (!WriteArchiveBody(w, all, &arc, job, err, password)) { w.Abort(); return false; }
    arc.vs.Close();
    for (auto& v : oldVols) {
        if (!DeleteFileW(LongPath(v).c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND) {
            err = L"기존 압축 파일을 바꿀 수 없습니다(다른 프로그램이 사용 중일 수 있습니다): " + v;
            w.Abort();
            return false;
        }
    }
    if (!FinalizeVolumes(w, target, err)) { w.Abort(); return false; }
    return true;
}

bool ZipModify(ZipArchive& arc, const std::map<int, std::wstring>& rename, const std::vector<int>& del, Job& job, std::wstring& err) {
    std::vector<char> gone(arc.entries.size(), 0);
    for (int i : del) if (i >= 0 && (size_t)i < gone.size()) gone[(size_t)i] = 1;
    std::vector<AddItem> all;
    uint64_t total = 0;
    for (size_t i = 0; i < arc.entries.size(); i++) {
        if (gone[i]) continue;
        auto& e = arc.entries[i];
        AddItem c; c.copyIdx = (int)i; c.isDir = e.isDir;
        auto it = rename.find((int)i);
        c.arcName = it != rename.end() ? it->second : e.name;
        all.push_back(c);
        total += e.csize;
    }
    if (all.empty()) { err = L"압축 파일 안의 모든 항목을 지울 수는 없습니다."; return false; }
    job.total = total ? total : 1; job.done = 0;
    return RewriteZip(arc, all, job, err, L"");
}

// ---------------------------------------------------------------- 추출
std::wstring SanitizeRel(const std::wstring& rel) {
    static const wchar_t* reserved[] = {L"con", L"prn", L"aux", L"nul", L"com1", L"com2", L"com3", L"com4",
        L"com5", L"com6", L"com7", L"com8", L"com9", L"lpt1", L"lpt2", L"lpt3", L"lpt4", L"lpt5", L"lpt6",
        L"lpt7", L"lpt8", L"lpt9"};
    std::wstring out, seg;
    auto flush = [&]() {
        while (!seg.empty() && (seg.back() == L' ' || seg.back() == L'.')) seg.pop_back();
        if (!seg.empty() && seg != L"." && seg != L"..") {
            std::wstring base = Lower(seg.substr(0, seg.find(L'.')));
            for (auto r : reserved) if (base == r) { seg = L"_" + seg; break; }
            if (!out.empty()) out += L'\\';
            out += seg;
        }
        seg.clear();
    };
    for (wchar_t c : rel) {
        if (c == L'/' || c == L'\\') { flush(); continue; }
        if (c < 32 || wcschr(L"<>:\"|?*", c)) c = L'_';
        seg += c;
    }
    flush();
    return out;
}
bool MakeDirs(const std::wstring& p) {
    if (DirExists(p)) return true;
    std::wstring par = ParentOf(p);
    if (!par.empty() && par != p && par.back() != L':' && !DirExists(par)) MakeDirs(par);
    return CreateDirectoryW(LongPath(p).c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool ExtractEntries(ZipArchive& arc, const std::vector<int>& idxIn, const std::wstring& strip,
                    const std::wstring& dest, Job& job, std::wstring& err, int& failed,
                    std::wstring& firstError) {
    failed = 0;
    const bool test = job.testOnly;  // 검사: 파일을 쓰지 않고 풀어 보기만
    if (!test && !MakeDirs(dest)) { err = L"폴더를 만들 수 없습니다: " + dest; return false; }

    // 대상 경로 계산 + 같은 경로가 여러 번 나오면 마지막 항목만(덮어쓰기와 같은 결과)
    struct Task { int i; std::wstring full; };
    std::vector<Task> tasks;
    std::map<std::wstring, size_t> seen;
    uint64_t total = 0;
    for (int i : idxIn) {
        const ZipEntry& e = arc.entries[(size_t)i];
        std::wstring rel = e.name;
        if (!strip.empty() && rel.compare(0, strip.size(), strip) == 0) rel = rel.substr(strip.size());
        rel = SanitizeRel(rel);
        if (rel.empty()) continue;
        std::wstring full = dest + L"\\" + rel;
        if (e.isDir) { if (!test) MakeDirs(full); continue; }
        std::wstring key = Lower(full);
        auto it = seen.find(key);
        if (it != seen.end()) { tasks[it->second].i = -1; }
        seen[key] = tasks.size();
        tasks.push_back({i, full});
        total += e.usize;
    }
    job.total = total; job.done = 0;

    // 암호: 한 번 묻고 모든 스레드가 같이 쓴다. 틀리면 다시 묻기(최대 3번)
    SRWLOCK pwLk = SRWLOCK_INIT;
    std::wstring pw = arc.password;
    bool pwCancelled = false;
    int pwAsks = 0;
    auto getPw = [&](std::wstring& out, const std::wstring* rejected) -> bool {
        AcquireSRWLockExclusive(&pwLk);
        bool ok = false;
        if (!pwCancelled) {
            if (!pw.empty() && (!rejected || pw != *rejected)) { out = pw; ok = true; }
            else if (pwAsks < 3 && job.askPassword) {
                std::wstring np;
                pwAsks++;
                if (job.askPassword(np) && !np.empty()) { pw = np; out = pw; ok = true; }
                else pwCancelled = true;
            } else pwCancelled = true;
        }
        ReleaseSRWLockExclusive(&pwLk);
        return ok;
    };
    SRWLOCK lk = SRWLOCK_INIT;      // policy / 실패 기록 / 질문 직렬화
    int policy = -1;                // 1 모두 덮어쓰기, 3 모두 건너뛰기
    volatile LONG next = 0, abortAll = 0;
    auto fail = [&](const std::wstring& m) {
        AcquireSRWLockExclusive(&lk);
        failed++; if (firstError.empty()) firstError = m;
        ReleaseSRWLockExclusive(&lk);
    };

    auto worker = [&]() {
        std::vector<uint8_t> inb(BUF), outb(BUF);
        for (;;) {
            LONG t = InterlockedIncrement(&next) - 1;
            if (t >= (LONG)tasks.size() || abortAll || job.cancel) break;
            if (tasks[(size_t)t].i < 0) continue;
            const ZipEntry& e = arc.entries[(size_t)tasks[(size_t)t].i];
            const std::wstring& full = tasks[(size_t)t].full;
            if (job.onFile) job.onFile(e.name);
            if (!test) MakeDirs(ParentOf(full));
            bool isAes = e.Encrypted() && e.method == 99 && e.aesStrength >= 1 && e.aesStrength <= 3;
            uint16_t method = isAes ? e.aesMethod : e.method;
            if (e.Encrypted() && e.method == 99 && !isAes) { fail(L"지원하지 않는 암호 방식입니다: " + e.name); job.done += e.usize; continue; }
            if (e.Encrypted() && (e.flags & 0x40)) { fail(L"지원하지 않는 암호 방식(PKWARE 강력 암호)입니다: " + e.name); job.done += e.usize; continue; }
            if (method != 0 && method != 8) { fail(L"지원하지 않는 압축 방식입니다: " + e.name); job.done += e.usize; continue; }
            if (!test && FileExists(full)) {
                AcquireSRWLockExclusive(&lk);   // 질문 창은 한 번에 하나
                int a = policy;
                if (a < 0 && job.askOverwrite && !abortAll) a = job.askOverwrite(full);
                if (a == 1 || a == 3) policy = a;
                ReleaseSRWLockExclusive(&lk);
                if (a == 4) { abortAll = 1; break; }
                if (a == 2 || a == 3) { job.done += e.usize; continue; }
                SetFileAttributesW(LongPath(full).c_str(), FILE_ATTRIBUTE_NORMAL);
            }
            uint64_t off;
            if (!arc.DataOffset(e, off)) { fail(L"항목 헤더가 손상되었습니다: " + e.name); continue; }
            uint64_t dataLen = e.csize;
            zc::Aes aes;
            zc::ZipCrypto zcr;
            bool zipCrypto = e.Encrypted() && !isAes;
            if (e.Encrypted()) {  // 암호 확인
                int saltLen = isAes ? 4 + 4 * e.aesStrength : 0;
                uint8_t head[18];
                size_t hl = isAes ? (size_t)saltLen + 2 : 12;
                uint64_t trailer = isAes ? 10 : 0;
                if (e.csize < hl + trailer || !arc.vs.Read(off, head, hl)) { fail(L"항목이 손상되었습니다: " + e.name); continue; }
                bool okPw = false;
                std::wstring cand, rejected;
                bool first = true;
                while (getPw(cand, first ? nullptr : &rejected)) {
                    first = false;
                    for (int enc = 0; enc < 2 && !okPw; enc++) {
                        std::string pb = enc ? zc::PwAnsi(cand) : zc::PwUtf8(cand);
                        if (enc && pb == zc::PwUtf8(cand)) break;
                        if (isAes) {
                            uint8_t pv[2];
                            zc::Aes t;
                            if (t.Init(pb, head, saltLen, 64 * (e.aesStrength + 1), pv) && pv[0] == head[saltLen] && pv[1] == head[saltLen + 1]) {
                                aes.Init(pb, head, saltLen, 64 * (e.aesStrength + 1), pv);
                                okPw = true;
                            }
                        } else {
                            uint8_t h2[12];
                            memcpy(h2, head, 12);
                            zc::ZipCrypto t;
                            t.Init(pb);
                            uint8_t check = (e.flags & 8) ? (uint8_t)(e.dosTime >> 8) : (uint8_t)(e.crc >> 24);
                            if (t.CheckHeader(h2, check)) { zcr = t; okPw = true; }
                        }
                    }
                    if (okPw) break;
                    rejected = cand;
                }
                if (!okPw) {
                    AcquireSRWLockShared(&pwLk);
                    bool c = pwCancelled;
                    ReleaseSRWLockShared(&pwLk);
                    fail((c ? L"암호가 틀렸거나 입력하지 않았습니다: " : L"암호가 틀렸습니다: ") + e.name);
                    job.done += e.usize;
                    continue;
                }
                off += hl;
                dataLen = e.csize - hl - trailer;
            }
            auto decrypt = [&](uint8_t* d, size_t n) {
                if (isAes) { aes.MacUpdate(d, n); aes.Crypt(d, n); }
                else if (zipCrypto) zcr.Decrypt(d, n);
            };
            HANDLE fh = test ? INVALID_HANDLE_VALUE : CreateFileW(LongPath(full).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (!test && fh == INVALID_HANDLE_VALUE) { fail(L"파일을 만들 수 없습니다: " + full); continue; }
            uint32_t crc = 0;
            bool ok = true;
            std::wstring why;
            uint64_t left = dataLen;
            auto put = [&](const uint8_t* d, size_t n) {
                DWORD wr = 0;
                if (!test && (!WriteFile(fh, d, (DWORD)n, &wr, nullptr) || wr != n)) { why = L"쓰기 실패(디스크 공간 확인): "; return false; }
                crc = FastCrc32(crc, d, n);
                job.done += n;
                return true;
            };
            if (method == 0) {
                while (left && ok) {
                    if (job.cancel || abortAll) { ok = false; why = L"취소"; break; }
                    size_t k = (size_t)std::min<uint64_t>(left, BUF);
                    if (!arc.vs.Read(off, inb.data(), k)) { ok = false; why = L"읽기 실패: "; break; }
                    decrypt(inb.data(), k);
                    ok = put(inb.data(), k);
                    off += k; left -= k;
                }
            } else {
                ZEng* s = ZInfNew();
                const uint8_t* in = nullptr;
                uint32_t ai = 0;
                if (!s) { ok = false; why = L"메모리 부족: "; }
                while (s) {
                    if (job.cancel || abortAll) { ok = false; why = L"취소"; break; }
                    if (ai == 0 && left) {
                        size_t k = (size_t)std::min<uint64_t>(left, BUF);
                        if (!arc.vs.Read(off, inb.data(), k)) { ok = false; why = L"읽기 실패: "; break; }
                        decrypt(inb.data(), k);
                        in = inb.data(); ai = (uint32_t)k;
                        off += k; left -= k;
                    }
                    uint8_t* out = outb.data();
                    uint32_t ao = (uint32_t)BUF;
                    int st = ZInfRun(s, in, ai, out, ao);
                    size_t prod = BUF - ao;
                    if (prod && !(ok = put(outb.data(), prod))) break;
                    if (st == ZE_END) break;
                    if (st == ZE_ERR) { ok = false; why = L"데이터 손상: "; break; }
                    if (prod == 0 && ai == 0 && left == 0) { ok = false; why = L"데이터가 잘렸습니다: "; break; }
                }
                ZInfFree(s);
            }
            if (ok && isAes) {  // 인증 코드(HMAC) 확인: 암호가 틀렸거나 내용이 바뀌었으면 실패
                // 압축 스트림이 끝났어도 남은 암호문이 있으면 MAC 에 반영
                while (left && ok) {
                    size_t k = (size_t)std::min<uint64_t>(left, BUF);
                    if (!arc.vs.Read(off, inb.data(), k)) { ok = false; break; }
                    aes.MacUpdate(inb.data(), k);
                    off += k; left -= k;
                }
                uint8_t mac[10], want[10];
                aes.MacFinal(mac);
                if (!ok || !arc.vs.Read(off, want, 10) || memcmp(mac, want, 10) != 0) { ok = false; why = L"암호가 틀렸거나 파일이 변조되었습니다: "; }
            }
            bool checkCrc = !(isAes && e.aesVersion == 2);
            if (ok && checkCrc && crc != e.crc) { ok = false; why = e.Encrypted() ? L"암호가 틀렸거나 파일이 손상되었습니다: " : L"CRC 오류(손상된 파일): "; }
            if (ok && !test) {
                FILETIME ft;
                if (DosToFileTime(e.dosDate, e.dosTime, ft)) SetFileTime(fh, nullptr, nullptr, &ft);
            }
            if (!test) CloseHandle(fh);
            if (!ok) {
                if (!test) DeleteFileW(LongPath(full).c_str());
                if (job.cancel || abortAll) break;
                fail(why + e.name);
            }
        }
    };

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    int nth = ZipThreads(si.dwNumberOfProcessors, 8);
    nth = (int)std::min<size_t>((size_t)nth, std::max<size_t>(1, tasks.size()));
    struct Ctx { decltype(worker)* f; };
    Ctx ctx{&worker};
    auto thunk = [](LPVOID p) -> DWORD { (*((Ctx*)p)->f)(); return 0; };
    std::vector<HANDLE> th;
    for (int k = 1; k < nth; k++) th.push_back(CreateThread(nullptr, 0, thunk, &ctx, 0, nullptr));
    worker();
    if (!th.empty()) WaitForMultipleObjects((DWORD)th.size(), th.data(), TRUE, INFINITE);
    for (HANDLE h : th) CloseHandle(h);
    if (!pw.empty() && failed == 0) arc.password = pw;  // 다음 추출 때 다시 묻지 않도록
    if (job.cancel || abortAll) { err = L"취소되었습니다."; return false; }
    return true;
}

bool ZipHasEncrypted(const ZipArchive& arc) {
    for (auto& e : arc.entries) if (!e.isDir && e.Encrypted()) return true;
    return false;
}
bool ZipCheckPassword(ZipArchive& arc, const std::wstring& pwW) {
    for (auto& e : arc.entries) {
        if (e.isDir || !e.Encrypted()) continue;
        uint64_t off;
        if (!arc.DataOffset(e, off)) return false;
        bool isAes = e.method == 99 && e.aesStrength >= 1 && e.aesStrength <= 3;
        int saltLen = isAes ? 4 + 4 * e.aesStrength : 0;
        uint8_t head[18];
        if (!arc.vs.Read(off, head, isAes ? (size_t)saltLen + 2 : 12)) return false;
        for (int enc = 0; enc < 2; enc++) {
            std::string pb = enc ? zc::PwAnsi(pwW) : zc::PwUtf8(pwW);
            if (isAes) {
                zc::Aes t; uint8_t pv[2];
                if (t.Init(pb, head, saltLen, 64 * (e.aesStrength + 1), pv) && pv[0] == head[saltLen] && pv[1] == head[saltLen + 1]) return true;
            } else {
                uint8_t h2[12]; memcpy(h2, head, 12);
                zc::ZipCrypto t; t.Init(pb);
                if (t.CheckHeader(h2, (e.flags & 8) ? (uint8_t)(e.dosTime >> 8) : (uint8_t)(e.crc >> 24))) return true;
            }
        }
        return false;
    }
    return true;
}
