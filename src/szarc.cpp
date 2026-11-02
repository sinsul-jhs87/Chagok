// szarc.cpp - 7z.dll 백엔드 + 형식 통합(AnyArchive)
// ZIP 계열은 자체 엔진(zipcore)을 그대로 쓰고, 나머지 형식만 7z.dll 을 필요할 때 불러와 처리한다.
#include "archive.h"
#include "sz_iface.h"
#include <algorithm>
#include <map>
#include <memory>
#include <cstring>

using namespace fmt;

// ============================================================ 7z.dll 불러오기
struct SzFormat {
    std::wstring name;
    GUID clsid{};
    std::vector<std::wstring> exts;
    bool update = false;
};
static HMODULE g_7z = nullptr;
static bool g_7zTried = false;
static Func_CreateObject g_create = nullptr;
static std::vector<SzFormat> g_formats;

static std::wstring ExeDir() {
    wchar_t b[MAX_PATH * 2];
    GetModuleFileNameW(nullptr, b, MAX_PATH * 2);
    std::wstring s = b;
    return s.substr(0, s.find_last_of(L'\\'));
}
static bool LoadSz() {
    if (g_7zTried) return g_7z != nullptr;
    g_7zTried = true;
    g_7z = LoadLibraryExW((ExeDir() + L"\\7z.dll").c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!g_7z) return false;
    g_create = (Func_CreateObject)GetProcAddress(g_7z, "CreateObject");
    auto num = (Func_GetNumberOfFormats)GetProcAddress(g_7z, "GetNumberOfFormats");
    auto prop = (Func_GetHandlerProperty2)GetProcAddress(g_7z, "GetHandlerProperty2");
    if (!g_create || !num || !prop) { FreeLibrary(g_7z); g_7z = nullptr; return false; }
    UInt32 n = 0;
    num(&n);
    for (UInt32 i = 0; i < n; i++) {
        SzFormat f;
        PROPVARIANT v;
        PropVariantInit(&v);
        if (prop(i, kHName, &v) == S_OK && v.vt == VT_BSTR) f.name = v.bstrVal;
        PropVariantClear(&v);
        if (prop(i, kHClassID, &v) == S_OK && v.vt == VT_BSTR && SysStringByteLen(v.bstrVal) >= sizeof(GUID))
            memcpy(&f.clsid, v.bstrVal, sizeof(GUID));
        PropVariantClear(&v);
        if (prop(i, kHExtension, &v) == S_OK && v.vt == VT_BSTR) {
            std::wstring e = Lower(v.bstrVal), cur;
            for (wchar_t c : e + L" ") { if (c == L' ') { if (!cur.empty()) f.exts.push_back(cur); cur.clear(); } else cur += c; }
        }
        PropVariantClear(&v);
        if (prop(i, kHUpdate, &v) == S_OK && v.vt == VT_BOOL) f.update = v.boolVal != VARIANT_FALSE;
        PropVariantClear(&v);
        g_formats.push_back(f);
    }
    return true;
}
bool SzAvailable() { return LoadSz(); }
static const SzFormat* FindFormat(const wchar_t* name) {
    for (auto& f : g_formats) if (!lstrcmpiW(f.name.c_str(), name)) return &f;
    return nullptr;
}

// ============================================================ COM 도우미
template <class T>
struct Unk : T {
    LONG ref = 1;
    virtual ~Unk() {}
    virtual bool Has(REFIID) { return false; }  // 추가로 지원하는 인터페이스
    virtual void* Cast(REFIID) { return nullptr; }
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (IsEqualIID(riid, IID_IUnknown)) { *ppv = static_cast<IUnknown*>(static_cast<T*>(this)); AddRef(); return S_OK; }
        if (void* p = Cast(riid)) { *ppv = p; AddRef(); return S_OK; }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return (ULONG)InterlockedIncrement(&ref); }
    STDMETHODIMP_(ULONG) Release() override { LONG r = InterlockedDecrement(&ref); if (!r) delete this; return (ULONG)r; }
};
static std::wstring PropStr(IInArchive_* a, UInt32 i, PROPID id) {
    PROPVARIANT v; PropVariantInit(&v);
    std::wstring s;
    if (a->GetProperty(i, id, &v) == S_OK && v.vt == VT_BSTR && v.bstrVal) s = v.bstrVal;
    PropVariantClear(&v);
    return s;
}
static uint64_t PropU64(IInArchive_* a, UInt32 i, PROPID id, bool* has = nullptr) {
    PROPVARIANT v; PropVariantInit(&v);
    uint64_t r = 0; bool h = true;
    if (a->GetProperty(i, id, &v) != S_OK) h = false;
    else if (v.vt == VT_UI8) r = v.uhVal.QuadPart;
    else if (v.vt == VT_UI4) r = v.ulVal;
    else if (v.vt == VT_UI2) r = v.uiVal;
    else if (v.vt == VT_I8) r = (uint64_t)v.hVal.QuadPart;
    else if (v.vt == VT_I4) r = (uint64_t)v.lVal;
    else h = false;
    PropVariantClear(&v);
    if (has) *has = h;
    return r;
}
static bool PropBool(IInArchive_* a, UInt32 i, PROPID id) {
    PROPVARIANT v; PropVariantInit(&v);
    bool r = false;
    if (a->GetProperty(i, id, &v) == S_OK && v.vt == VT_BOOL) r = v.boolVal != VARIANT_FALSE;
    PropVariantClear(&v);
    return r;
}
static bool PropTime(IInArchive_* a, UInt32 i, PROPID id, FILETIME& ft) {
    PROPVARIANT v; PropVariantInit(&v);
    bool r = false;
    if (a->GetProperty(i, id, &v) == S_OK && v.vt == VT_FILETIME) { ft = v.filetime; r = ft.dwHighDateTime || ft.dwLowDateTime; }
    PropVariantClear(&v);
    return r;
}

// ============================================================ 스트림
// 여러 볼륨(.001 …)을 이어 붙인 읽기 스트림
struct VolInStream : Unk<IInStream_> {
    std::shared_ptr<VolumeSet> vs;
    UInt64 pos = 0;
    IStreamGetSize_* sizeItf = nullptr;
    struct SizeImpl : IStreamGetSize_ {
        VolInStream* o;
        STDMETHODIMP QueryInterface(REFIID r, void** p) override { return o->QueryInterface(r, p); }
        STDMETHODIMP_(ULONG) AddRef() override { return o->AddRef(); }
        STDMETHODIMP_(ULONG) Release() override { return o->Release(); }
        STDMETHODIMP GetSize(UInt64* s) override { *s = o->vs->total; return S_OK; }
    } sz;
    VolInStream() { sz.o = this; }
    void* Cast(REFIID r) override {
        if (IsEqualIID(r, IID_IInStream_) || IsEqualIID(r, IID_ISequentialInStream_)) return static_cast<IInStream_*>(this);
        if (IsEqualIID(r, IID_IStreamGetSize_)) return static_cast<IStreamGetSize_*>(&sz);
        return nullptr;
    }
    STDMETHODIMP Read(void* data, UInt32 size, UInt32* processed) override {
        if (processed) *processed = 0;
        if (pos >= vs->total || !size) return S_OK;
        UInt32 k = (UInt32)std::min<UInt64>(size, vs->total - pos);
        if (!vs->Read(pos, data, k)) return E_FAIL;
        pos += k;
        if (processed) *processed = k;
        return S_OK;
    }
    STDMETHODIMP Seek(Int64 off, UInt32 origin, UInt64* np) override {
        Int64 base = origin == 0 ? 0 : origin == 1 ? (Int64)pos : (Int64)vs->total;
        if (base + off < 0) return HRESULT_FROM_WIN32(ERROR_NEGATIVE_SEEK);
        pos = (UInt64)(base + off);
        if (np) *np = pos;
        return S_OK;
    }
};
static IInStream_* OpenFileStream(const std::vector<std::wstring>& paths) {
    auto vs = std::make_shared<VolumeSet>();
    if (!vs->Open(paths)) return nullptr;
    VolInStream* s = new VolInStream();
    s->vs = vs;
    return s;
}

// 압축 해제 대상 파일
struct FileOutStream : Unk<ISequentialOutStream_> {
    HANDLE h = INVALID_HANDLE_VALUE;
    Job* job = nullptr;
    bool failed = false;
    ~FileOutStream() override { if (h != INVALID_HANDLE_VALUE) CloseHandle(h); }
    void* Cast(REFIID r) override { return IsEqualIID(r, IID_ISequentialOutStream_) ? static_cast<ISequentialOutStream_*>(this) : nullptr; }
    STDMETHODIMP Write(const void* d, UInt32 n, UInt32* done) override {
        DWORD w = 0;
        if (done) *done = 0;
        if (job && job->cancel) return E_ABORT;
        if (!WriteFile(h, d, n, &w, nullptr) || w != n) { failed = true; return HRESULT_FROM_WIN32(GetLastError()); }
        if (done) *done = n;
        return S_OK;
    }
};

// 새 압축 파일 (분할 + 앞으로 되돌아가 쓰기 지원: 7z 는 끝에 머리글을 다시 씀)
struct VolOutStream : Unk<IOutStream_> {
    std::wstring base;
    UInt64 split = 0, pos = 0, size = 0;
    std::vector<HANDLE> hs;
    std::vector<std::wstring> names;
    Job* job = nullptr;
    ~VolOutStream() override { CloseAll(); }
    void CloseAll() { for (HANDLE h : hs) if (h != INVALID_HANDLE_VALUE) CloseHandle(h); hs.clear(); }
    void Abort() { CloseAll(); for (auto& n : names) DeleteFileW(LongPath(n).c_str()); names.clear(); }
    void* Cast(REFIID r) override {
        if (IsEqualIID(r, IID_IOutStream_) || IsEqualIID(r, IID_ISequentialOutStream_)) return static_cast<IOutStream_*>(this);
        return nullptr;
    }
    HANDLE Vol(size_t i) {
        while (hs.size() <= i) {
            std::wstring n = base + L".~tmp" + std::to_wstring(hs.size() + 1);
            HANDLE h = CreateFileW(LongPath(n).c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (h == INVALID_HANDLE_VALUE) return h;
            hs.push_back(h); names.push_back(n);
        }
        return hs[i];
    }
    STDMETHODIMP Write(const void* d, UInt32 n, UInt32* done) override {
        if (done) *done = 0;
        if (job && job->cancel) return E_ABORT;
        const uint8_t* p = (const uint8_t*)d;
        UInt32 total = 0;
        while (n) {
            size_t vi = split ? (size_t)(pos / split) : 0;
            UInt64 in = split ? pos % split : pos;
            UInt32 k = split ? (UInt32)std::min<UInt64>(n, split - in) : n;
            HANDLE h = Vol(vi);
            if (h == INVALID_HANDLE_VALUE) return E_FAIL;
            LARGE_INTEGER li; li.QuadPart = (LONGLONG)in;
            DWORD w = 0;
            if (!SetFilePointerEx(h, li, nullptr, FILE_BEGIN) || !WriteFile(h, p, k, &w, nullptr) || w != k)
                return HRESULT_FROM_WIN32(GetLastError());
            p += k; n -= k; pos += k; total += k;
            if (pos > size) size = pos;
        }
        if (done) *done = total;
        return S_OK;
    }
    STDMETHODIMP Seek(Int64 off, UInt32 origin, UInt64* np) override {
        Int64 b = origin == 0 ? 0 : origin == 1 ? (Int64)pos : (Int64)size;
        if (b + off < 0) return HRESULT_FROM_WIN32(ERROR_NEGATIVE_SEEK);
        pos = (UInt64)(b + off);
        if (np) *np = pos;
        return S_OK;
    }
    STDMETHODIMP SetSize(UInt64 s) override {
        size = s;
        if (!split) {
            HANDLE h = Vol(0);
            LARGE_INTEGER li; li.QuadPart = (LONGLONG)s;
            SetFilePointerEx(h, li, nullptr, FILE_BEGIN);
            SetEndOfFile(h);
        }
        return S_OK;
    }
    bool Finish(const std::wstring& target, std::wstring& err) {
        if (split) {  // 마지막 조각 길이 정리
            size_t last = size ? (size_t)((size - 1) / split) : 0;
            Vol(last);
            for (size_t i = last + 1; i < hs.size(); i++) { CloseHandle(hs[i]); hs[i] = INVALID_HANDLE_VALUE; DeleteFileW(LongPath(names[i]).c_str()); }
            hs.resize(last + 1); names.resize(last + 1);
            LARGE_INTEGER li; li.QuadPart = (LONGLONG)(size - (UInt64)last * split);
            SetFilePointerEx(hs[last], li, nullptr, FILE_BEGIN);
            SetEndOfFile(hs[last]);
        } else if (hs.empty()) Vol(0);
        CloseAll();
        bool ok = FinalizeTempVolumes(names, target, true, err);
        names.clear();
        return ok;
    }
};

// 압축할 원본 파일
struct FileInStream : Unk<ISequentialInStream_> {
    HANDLE h = INVALID_HANDLE_VALUE;
    Job* job = nullptr;
    ~FileInStream() override { if (h != INVALID_HANDLE_VALUE) CloseHandle(h); }
    void* Cast(REFIID r) override { return IsEqualIID(r, IID_ISequentialInStream_) ? static_cast<ISequentialInStream_*>(this) : nullptr; }
    STDMETHODIMP Read(void* d, UInt32 n, UInt32* got) override {
        DWORD g = 0;
        if (job && job->cancel) return E_ABORT;
        if (!ReadFile(h, d, n, &g, nullptr)) return HRESULT_FROM_WIN32(GetLastError());
        if (got) *got = g;
        return S_OK;
    }
};

static HRESULT MakeBstr(const std::wstring& s, BSTR* out) {
    *out = SysAllocStringLen(s.c_str(), (UINT)s.size());
    return *out ? S_OK : E_OUTOFMEMORY;
}

// ============================================================ 열기 콜백 (암호, rar 다중 볼륨)
struct OpenCb : Unk<IArchiveOpenCallback_> {
    std::wstring dir, first;
    std::wstring curName;
    std::wstring* password = nullptr;
    const std::function<bool(std::wstring&)>* ask = nullptr;
    bool asked = false, cancelled = false;
    struct Pw : ICryptoGetTextPassword_ {
        OpenCb* o;
        STDMETHODIMP QueryInterface(REFIID r, void** p) override { return o->QueryInterface(r, p); }
        STDMETHODIMP_(ULONG) AddRef() override { return o->AddRef(); }
        STDMETHODIMP_(ULONG) Release() override { return o->Release(); }
        STDMETHODIMP CryptoGetTextPassword(BSTR* pw) override {
            if (o->password->empty()) {
                o->asked = true;
                if (!o->ask || !*o->ask || !(*o->ask)(*o->password)) { o->cancelled = true; return E_ABORT; }
            }
            return MakeBstr(*o->password, pw);
        }
    } pw;
    struct Vol : IUnknown {
        OpenCb* o;
        STDMETHODIMP QueryInterface(REFIID r, void** p) override { return o->QueryInterface(r, p); }
        STDMETHODIMP_(ULONG) AddRef() override { return o->AddRef(); }
        STDMETHODIMP_(ULONG) Release() override { return o->Release(); }
        // IArchiveOpenVolumeCallback
        virtual HRESULT STDMETHODCALLTYPE GetProperty(PROPID id, PROPVARIANT* v) {
            PropVariantInit(v);
            if (id == kpidName) { v->vt = VT_BSTR; v->bstrVal = SysAllocString(o->curName.c_str()); }
            return S_OK;
        }
        virtual HRESULT STDMETHODCALLTYPE GetStream(const wchar_t* name, IInStream_** s) {
            *s = nullptr;
            std::wstring p = o->dir + L"\\" + name;
            if (!FileExists(p)) return S_FALSE;
            *s = OpenFileStream({p});
            o->curName = name;
            return *s ? S_OK : S_FALSE;
        }
    } vol;
    OpenCb() { pw.o = this; vol.o = this; }
    void* Cast(REFIID r) override {
        if (IsEqualIID(r, IID_IArchiveOpenCallback_)) return static_cast<IArchiveOpenCallback_*>(this);
        if (IsEqualIID(r, IID_ICryptoGetTextPassword_)) return static_cast<ICryptoGetTextPassword_*>(&pw);
        if (IsEqualIID(r, IID_IArchiveOpenVolumeCallback_)) return static_cast<IUnknown*>(&vol);
        return nullptr;
    }
    STDMETHODIMP SetTotal(const UInt64*, const UInt64*) override { return S_OK; }
    STDMETHODIMP SetCompleted(const UInt64*, const UInt64*) override { return S_OK; }
};

// ============================================================ SzArchive
struct SzArchive {
    IInArchive_* arc = nullptr;
    const SzFormat* fmtInfo = nullptr;
    std::vector<UInt32> map;           // entries 순번 → 7z.dll 항목 번호
    std::wstring password;
    std::wstring nestedTemp;           // tar.gz 등: 안쪽 tar 를 풀어 둔 임시 파일
    uint64_t firstVolSize = 0;
    ~SzArchive() { Close(); }
    void Close() {
        if (arc) { arc->Close(); arc->Release(); arc = nullptr; }
        if (!nestedTemp.empty()) { DeleteFileW(LongPath(nestedTemp).c_str()); nestedTemp.clear(); }
        map.clear();
    }
};

static IInArchive_* TryOpen(const SzFormat& f, const std::vector<std::wstring>& vols, OpenCb* cb) {
    IInArchive_* a = nullptr;
    if (g_create(&f.clsid, &IID_IInArchive_, (void**)&a) != S_OK || !a) return nullptr;
    IInStream_* s = OpenFileStream(vols);
    if (!s) { a->Release(); return nullptr; }
    UInt64 maxCheck = 1 << 22;
    HRESULT hr = a->Open(s, &maxCheck, cb);
    s->Release();
    if (hr != S_OK) { a->Close(); a->Release(); return nullptr; }
    return a;
}

// 확장자에 맞는 형식부터, 그다음 압축 컨테이너 형식 전체를 시도
static const wchar_t* const FALLBACK[] = {L"7z", L"Rar5", L"Rar", L"tar", L"gzip", L"bzip2", L"xz", L"Iso", L"Udf",
    L"FAT", L"NTFS", L"MBR", L"GPT", L"VHD", L"VHDX", L"VMDK", L"VDI", L"QCOW", L"Cab", L"wim", L"Lzh", L"Arj",
    L"Cpio", L"Rpm", L"Ar", L"Dmg", L"HFS", L"APFS", L"SquashFS", L"CramFS", L"Ext", L"Xar", L"Z", L"lzma", L"zstd"};

static std::wstring TempPath(const wchar_t* ext) {
    wchar_t d[MAX_PATH], f[MAX_PATH];
    GetTempPathW(MAX_PATH, d);
    GetTempFileNameW(d, L"chg", 0, f);
    std::wstring p = f;
    DeleteFileW(f);
    return p + ext;
}

// 단일 항목 하나를 파일로 풀기 (tar.gz 의 안쪽 tar)
struct OneFileExtractCb : Unk<IArchiveExtractCallback_> {
    std::wstring target;
    Int32 result = -1;
    void* Cast(REFIID r) override { return IsEqualIID(r, IID_IArchiveExtractCallback_) ? static_cast<IArchiveExtractCallback_*>(this) : nullptr; }
    STDMETHODIMP SetTotal(UInt64) override { return S_OK; }
    STDMETHODIMP SetCompleted(const UInt64*) override { return S_OK; }
    STDMETHODIMP GetStream(UInt32, ISequentialOutStream_** out, Int32 mode) override {
        *out = nullptr;
        if (mode != kAskExtract) return S_OK;
        FileOutStream* f = new FileOutStream();
        f->h = CreateFileW(LongPath(target).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
        if (f->h == INVALID_HANDLE_VALUE) { f->Release(); return E_FAIL; }
        *out = f;
        return S_OK;
    }
    STDMETHODIMP PrepareOperation(Int32) override { return S_OK; }
    STDMETHODIMP SetOperationResult(Int32 r) override { result = r; return S_OK; }
};

// 바이트로 이름을 저장하는 형식(gzip, tar 등)에서 UTF-8 이름이 ANSI 로 잘못 읽힌 경우 되돌린다
static std::wstring FixLatin1Utf8(const std::wstring& s) {
    bool high = false;
    for (wchar_t c : s) if (c >= 0x80) high = true;
    if (!high) return s;
    std::string b;
    bool latin = true;
    for (wchar_t c : s) { if (c > 0xFF) { latin = false; break; } b += (char)c; }
    if (!latin) {
        BOOL usedDef = FALSE;
        int m = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, s.data(), (int)s.size(), nullptr, 0, nullptr, &usedDef);
        if (m <= 0 || usedDef) return s;
        b.assign(m, 0);
        WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, s.data(), (int)s.size(), &b[0], m, nullptr, &usedDef);
        if (usedDef) return s;
    }
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, b.data(), (int)b.size(), nullptr, 0);
    if (n <= 0) return s;
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, b.data(), (int)b.size(), &w[0], n);
    return w;
}
static void FillEntries(AnyArchive& a, const std::wstring& singleName) {
    SzArchive& z = *a.sz;
    std::wstring fn = Lower(z.fmtInfo->name);
    bool byteNames = fn == L"gzip" || fn == L"tar" || fn == L"cpio" || fn == L"ar" || fn == L"rpm" || fn == L"lzh" || fn == L"arj";
    UInt32 n = 0;
    z.arc->GetNumberOfItems(&n);
    a.entries.clear();
    z.map.clear();
    for (UInt32 i = 0; i < n; i++) {
        if (PropBool(z.arc, i, kpidIsAltStream)) continue;
        ZipEntry e;
        e.name = PropStr(z.arc, i, kpidPath);
        if (byteNames) e.name = FixLatin1Utf8(e.name);
        for (auto& c : e.name) if (c == L'\\') c = L'/';
        while (!e.name.empty() && e.name[0] == L'/') e.name.erase(0, 1);
        while (!e.name.empty() && e.name.back() == L'/') e.name.pop_back();
        if (e.name.empty()) e.name = n == 1 ? singleName : L"[" + std::to_wstring(i) + L"]";
        e.isDir = PropBool(z.arc, i, kpidIsDir);
        e.usize = PropU64(z.arc, i, kpidSize);
        bool hasPack = false;
        e.csize = PropU64(z.arc, i, kpidPackSize, &hasPack);
        if (!hasPack || (e.csize == 0 && e.usize > 0)) e.csize = UINT64_MAX;  // 알 수 없음 (솔리드 등)
        e.method = 8;
        if (PropBool(z.arc, i, kpidEncrypted)) e.flags |= 1;
        FILETIME ft, lt;
        WORD d = 0x21, t = 0;
        if (PropTime(z.arc, i, kpidMTime, ft) && FileTimeToLocalFileTime(&ft, &lt)) FileTimeToDosDateTime(&lt, &d, &t);
        e.dosDate = d; e.dosTime = t;
        e.crc = (uint32_t)PropU64(z.arc, i, kpidCRC);
        e.localOffset = i;
        a.entries.push_back(e);
        z.map.push_back(i);
    }
    // 하나라도 압축 크기를 모르면(솔리드 7z 등) 항목별 크기는 표시하지 않는다
    bool unknown = false;
    for (auto& e : a.entries) if (!e.isDir && e.csize == UINT64_MAX) unknown = true;
    if (unknown) for (auto& e : a.entries) e.csize = UINT64_MAX;
}

static bool SzOpen(AnyArchive& a, const std::vector<std::wstring>& vols, std::wstring& err,
                   const std::function<bool(std::wstring&)>& ask) {
    if (!LoadSz()) { err = L"이 형식을 열려면 7z.dll 이 필요합니다. 차곡을 다시 설치해 주세요."; return false; }
    a.sz = new SzArchive();
    SzArchive& z = *a.sz;
    std::wstring ext = Ext(a.path);
    if (Digits(ext)) ext = Ext(a.path.substr(0, a.path.size() - ext.size() - 1));  // name.7z.001 → 7z
    std::vector<const SzFormat*> order;
    for (auto& f : g_formats) {
        if (!lstrcmpiW(f.name.c_str(), L"Split") || !lstrcmpiW(f.name.c_str(), L"Hash")) continue;
        for (auto& e : f.exts) if (e == ext) { order.push_back(&f); break; }
    }
    for (auto n : FALLBACK) {
        const SzFormat* f = FindFormat(n);
        if (f && std::find(order.begin(), order.end(), f) == order.end()) order.push_back(f);
    }
    bool badPw = false;
    OpenCb* cb = new OpenCb();
    cb->dir = ParentOf(vols[0]);
    cb->curName = FileNameOf(vols[0]);
    cb->password = &z.password;
    cb->ask = &ask;
    for (auto f : order) {
        z.arc = TryOpen(*f, vols, cb);
        if (z.arc) { z.fmtInfo = f; break; }
        if (cb->cancelled) break;
        bool wrong = false;
        for (int retry = 0; retry < 3 && cb->asked && !cb->cancelled && !z.arc; retry++) {  // 암호가 틀림 → 다시 묻기
            wrong = true;
            z.password.clear();
            z.arc = TryOpen(*f, vols, cb);
        }
        if (z.arc) { z.fmtInfo = f; break; }
        if (cb->cancelled || wrong) { badPw = wrong && !cb->cancelled; break; }
    }
    bool cancelled = cb->cancelled;
    cb->Release();
    if (!z.arc) {
        err = cancelled ? L"암호 입력을 취소했습니다." : badPw ? L"암호가 틀렸습니다." : L"지원하지 않는 형식이거나 손상된 파일입니다.";
        return false;
    }
    z.firstVolSize = vols.size() > 1 ? (uint64_t)0 : 0;
    a.format = z.fmtInfo->name;
    std::wstring stem = fmt::Stem(a.path);
    FillEntries(a, stem);

    // gzip/bzip2/xz 안에 tar 가 하나 있으면 풀어서 tar 로 연다 (tar.gz, tgz …)
    std::wstring fn = Lower(z.fmtInfo->name);
    bool single = fn == L"gzip" || fn == L"bzip2" || fn == L"xz" || fn == L"lzma" || fn == L"z" || fn == L"zstd";
    if (single && a.entries.size() == 1) {
        std::wstring inner = Lower(a.entries[0].name);
        std::wstring outerNoExt = Lower(a.path.substr(0, a.path.find_last_of(L'.')));
        bool tarInside = (inner.size() > 4 && inner.substr(inner.size() - 4) == L".tar") ||
                         (outerNoExt.size() > 4 && outerNoExt.substr(outerNoExt.size() - 4) == L".tar") ||
                         ext == L"tgz" || ext == L"tbz" || ext == L"tbz2" || ext == L"txz" || ext == L"tlz" || ext == L"taz" || ext == L"tzst";
        const SzFormat* tar = FindFormat(L"tar");
        if (tarInside && tar) {
            OneFileExtractCb* x = new OneFileExtractCb();
            x->target = TempPath(L".tar");
            UInt32 idx = z.map[0];
            HRESULT hr = z.arc->Extract(&idx, 1, 0, x);
            bool ok = hr == S_OK && x->result == kOpOK;
            std::wstring tmp = x->target;
            x->Release();
            if (ok) {
                OpenCb* c2 = new OpenCb();
                c2->password = &z.password; c2->ask = &ask; c2->dir = ParentOf(tmp); c2->curName = FileNameOf(tmp);
                IInArchive_* t = TryOpen(*tar, {tmp}, c2);
                c2->Release();
                if (t) {
                    z.arc->Close(); z.arc->Release();
                    z.arc = t;
                    z.nestedTemp = tmp;
                    a.format = L"tar." + (fn == L"gzip" ? std::wstring(L"gz") : fn == L"bzip2" ? std::wstring(L"bz2") : fn);
                    FillEntries(a, stem);
                } else DeleteFileW(tmp.c_str());
            } else DeleteFileW(tmp.c_str());
        }
    }
    // 추가 가능 여부: 7z, tar 만 (분할 tar 는 제외)
    std::wstring f2 = Lower(z.fmtInfo->name);
    a.readOnly = !(z.nestedTemp.empty() && (f2 == L"7z" || f2 == L"tar") && z.fmtInfo->update);
    if (a.readOnly) a.readOnlyWhy = a.format + L" 형식은 파일 추가를 지원하지 않습니다. (열기·압축 풀기만 가능)";
    return true;
}

// ============================================================ AnyArchive
bool AnyArchive::Open(const std::wstring& p, std::wstring& err, const std::function<bool(std::wstring&)>& ask) {
    Close();
    if (fmt::UsesZipEngine(p)) {
        if (!zip.Open(p, err)) return false;
        path = zip.path; volumes = zip.volumes; isSplit = zip.isSplit; entries = zip.entries;
        format = L"ZIP"; isZip = true; readOnly = false;
        return true;
    }
    // ZIP 확장자가 아니어도 내용이 ZIP 이면 자체 엔진 사용 (예: 이름만 바뀐 zip)
    isZip = false;
    path = p;
    // 분할 조각(.001) 모으기
    volumes.clear();
    std::wstring e = Ext(p);
    if (e.size() >= 3 && Digits(e)) {
        std::wstring stem = p.substr(0, p.size() - e.size() - 1);
        for (unsigned i = 1;; i++) {
            wchar_t b[24]; swprintf(b, 24, L".%0*u", (int)e.size(), i);
            if (!FileExists(stem + b)) break;
            volumes.push_back(stem + b);
        }
        if (volumes.empty()) volumes.push_back(p);
        path = volumes[0];
    } else volumes.push_back(p);
    isSplit = volumes.size() > 1;
    if (!SzOpen(*this, volumes, err, ask)) { Close(); return false; }
    if (format == L"zip") {  // 7z.dll 이 zip 으로 판정 → 자체 엔진으로 다시
        std::wstring e2;
        delete sz; sz = nullptr;
        if (zip.Open(p, e2)) { path = zip.path; volumes = zip.volumes; isSplit = zip.isSplit; entries = zip.entries; format = L"ZIP"; isZip = true; readOnly = false; return true; }
    }
    return true;
}
void AnyArchive::Close() {
    zip.Close();
    if (sz) { delete sz; sz = nullptr; }
    entries.clear();
    volumes.clear();
    path.clear();
    isSplit = false; readOnly = false; isZip = true; format = L"ZIP";
}

// ------------------------------------------------------------ 압축 풀기
struct ExtractCb : Unk<IArchiveExtractCallback_> {
    AnyArchive* a;
    Job* job;
    std::map<UInt32, std::wstring> target;   // 7z.dll 항목 번호 → 저장 경로
    std::map<UInt32, size_t> entryOf;
    int policy = -1;
    bool abortAll = false;
    int failed = 0;
    std::wstring firstError;
    UInt32 curIndex = 0;
    std::wstring curPath;
    FileOutStream* cur = nullptr;
    bool wrongPw = false;
    struct Pw : ICryptoGetTextPassword_ {
        ExtractCb* o;
        STDMETHODIMP QueryInterface(REFIID r, void** p) override { return o->QueryInterface(r, p); }
        STDMETHODIMP_(ULONG) AddRef() override { return o->AddRef(); }
        STDMETHODIMP_(ULONG) Release() override { return o->Release(); }
        STDMETHODIMP CryptoGetTextPassword(BSTR* pw) override {
            std::wstring& p = o->a->sz->password;
            if (p.empty() && (!o->job->askPassword || !o->job->askPassword(p))) { o->abortAll = true; return E_ABORT; }
            return MakeBstr(p, pw);
        }
    } pw;
    ExtractCb() { pw.o = this; }
    void* Cast(REFIID r) override {
        if (IsEqualIID(r, IID_IArchiveExtractCallback_) || IsEqualIID(r, IID_IProgress_)) return static_cast<IArchiveExtractCallback_*>(this);
        if (IsEqualIID(r, IID_ICryptoGetTextPassword_)) return static_cast<ICryptoGetTextPassword_*>(&pw);
        return nullptr;
    }
    void Fail(const std::wstring& m) { failed++; if (firstError.empty()) firstError = m; }
    STDMETHODIMP SetTotal(UInt64 t) override { job->total = t; return S_OK; }
    STDMETHODIMP SetCompleted(const UInt64* c) override {
        if (c) job->done = *c;
        return (job->cancel || abortAll) ? E_ABORT : S_OK;
    }
    STDMETHODIMP GetStream(UInt32 index, ISequentialOutStream_** out, Int32 mode) override {
        *out = nullptr;
        cur = nullptr;
        curIndex = index;
        if (mode != kAskExtract) return S_OK;
        auto it = target.find(index);
        if (it == target.end()) return S_OK;
        const ZipEntry& e = a->entries[entryOf[index]];
        curPath = it->second;
        if (job->onFile) job->onFile(e.name);
        if (e.isDir) { MakeDirs(curPath); return S_OK; }
        MakeDirs(ParentOf(curPath));
        if (FileExists(curPath)) {
            int r = policy;
            if (r < 0 && job->askOverwrite) r = job->askOverwrite(curPath);
            if (r == 1 || r == 3) policy = r;
            if (r == 4) { abortAll = true; return E_ABORT; }
            if (r == 2 || r == 3) return S_OK;
            SetFileAttributesW(LongPath(curPath).c_str(), FILE_ATTRIBUTE_NORMAL);
        }
        FileOutStream* f = new FileOutStream();
        f->job = job;
        f->h = CreateFileW(LongPath(curPath).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f->h == INVALID_HANDLE_VALUE) { f->Release(); Fail(L"파일을 만들 수 없습니다: " + curPath); return S_OK; }
        FILETIME ft;
        if (PropTime(a->sz->arc, index, kpidMTime, ft)) SetFileTime(f->h, nullptr, nullptr, &ft);
        cur = f;
        f->AddRef();  // 결과 처리까지 붙잡아 둠
        *out = f;
        return S_OK;
    }
    STDMETHODIMP PrepareOperation(Int32) override { return S_OK; }
    STDMETHODIMP SetOperationResult(Int32 r) override {
        if (cur) {
            if (r == kOpOK) {
                FILETIME ft;
                if (PropTime(a->sz->arc, curIndex, kpidMTime, ft)) SetFileTime(cur->h, nullptr, nullptr, &ft);
            }
            CloseHandle(cur->h);
            cur->h = INVALID_HANDLE_VALUE;
            if (r != kOpOK) DeleteFileW(LongPath(curPath).c_str());
            cur->Release();
            cur = nullptr;
        }
        if (r != kOpOK && target.count(curIndex)) {
            const ZipEntry& e = a->entries[entryOf[curIndex]];
            const wchar_t* why = r == kOpWrongPassword ? L"암호가 틀렸습니다: " :
                                 r == kOpUnsupportedMethod ? L"지원하지 않는 압축 방식입니다: " :
                                 r == kOpCRCError ? L"CRC 오류(손상된 파일): " :
                                 r == kOpUnexpectedEnd ? L"데이터가 잘렸습니다(분할 조각이 빠졌는지 확인): " : L"데이터 오류: ";
            if (r == kOpWrongPassword || ((e.flags & 1) && (r == kOpDataError || r == kOpCRCError))) {
                why = L"암호가 틀렸습니다: ";
                wrongPw = true;
            }
            Fail(why + e.name);
        }
        return S_OK;
    }
};

bool AnyExtract(AnyArchive& a, const std::vector<int>& idx, const std::wstring& strip, const std::wstring& dest,
                Job& job, std::wstring& err, int& failed, std::wstring& firstError) {
    if (a.isZip) return ExtractEntries(a.zip, idx, strip, dest, job, err, failed, firstError);
    failed = 0;
    if (!job.testOnly && !MakeDirs(dest)) { err = L"폴더를 만들 수 없습니다: " + dest; return false; }
    ExtractCb* cb = new ExtractCb();
    cb->a = &a; cb->job = &job;
    std::vector<UInt32> list;
    uint64_t total = 0;
    for (int i : idx) {
        const ZipEntry& e = a.entries[(size_t)i];
        std::wstring rel = e.name;
        if (!strip.empty() && rel.compare(0, strip.size(), strip) == 0) rel = rel.substr(strip.size());
        rel = SanitizeRel(rel);
        if (rel.empty()) continue;
        UInt32 ai = a.sz->map[(size_t)i];
        cb->target[ai] = dest + L"\\" + rel;
        cb->entryOf[ai] = (size_t)i;
        list.push_back(ai);
        total += e.usize;
    }
    std::sort(list.begin(), list.end());
    job.total = total; job.done = 0;
    HRESULT hr = list.empty() ? S_OK : a.sz->arc->Extract(list.data(), (UInt32)list.size(), job.testOnly ? 1 : 0, cb);
    bool cancelled = job.cancel || cb->abortAll;
    failed = cb->failed;
    firstError = cb->firstError;
    if (cb->wrongPw) a.sz->password.clear();  // 다음에 다시 묻기
    cb->Release();
    if (cancelled) { err = L"취소되었습니다."; return false; }
    if (hr != S_OK && !failed) { err = L"압축을 풀 수 없습니다. 파일이 손상되었을 수 있습니다."; return false; }
    return true;
}

// ------------------------------------------------------------ 만들기 / 추가 (7z.dll)
struct UpdateCb : Unk<IArchiveUpdateCallback_> {
    std::vector<AddItem>* items = nullptr;   // 새 항목
    std::vector<UInt32> keep;                 // 유지할 기존 항목 번호
    std::map<size_t, std::wstring> rename;    // keep 의 위치 → 새 이름
    std::vector<const ZipEntry*> keepEntry;   // 이름 바꿀 때 속성을 채우는 데 사용
    Job* job = nullptr;
    std::wstring password;
    bool useWinSep = true;
    std::wstring failMsg;
    struct Pw2 : ICryptoGetTextPassword2_ {
        UpdateCb* o;
        STDMETHODIMP QueryInterface(REFIID r, void** p) override { return o->QueryInterface(r, p); }
        STDMETHODIMP_(ULONG) AddRef() override { return o->AddRef(); }
        STDMETHODIMP_(ULONG) Release() override { return o->Release(); }
        STDMETHODIMP CryptoGetTextPassword2(Int32* defined, BSTR* pw) override {
            *defined = o->password.empty() ? 0 : 1;
            return MakeBstr(o->password, pw);
        }
    } pw2;
    UpdateCb() { pw2.o = this; }
    void* Cast(REFIID r) override {
        if (IsEqualIID(r, IID_IArchiveUpdateCallback_) || IsEqualIID(r, IID_IProgress_)) return static_cast<IArchiveUpdateCallback_*>(this);
        if (IsEqualIID(r, IID_ICryptoGetTextPassword2_)) return static_cast<ICryptoGetTextPassword2_*>(&pw2);
        return nullptr;
    }
    uint64_t tot = 0;
    // 7z 두 단계 만들기: 단계별 진행을 전체 진행(pBase + 비율 × pSpan)으로 바꿔 보고
    STDMETHODIMP SetTotal(UInt64 t) override { tot = t; if (!job->pSpan) job->total = t; return S_OK; }
    STDMETHODIMP SetCompleted(const UInt64* c) override {
        if (c) job->done = job->pSpan ? job->pBase + (tot ? (uint64_t)((double)*c / tot * job->pSpan) : 0) : *c;
        return job->cancel ? E_ABORT : S_OK;
    }
    STDMETHODIMP GetUpdateItemInfo(UInt32 index, Int32* newData, Int32* newProps, UInt32* inArc) override {
        bool old = index < keep.size();
        if (newData) *newData = old ? 0 : 1;
        if (newProps) *newProps = old ? (rename.count(index) ? 1 : 0) : 1;
        if (inArc) *inArc = old ? keep[index] : (UInt32)-1;
        return S_OK;
    }
    STDMETHODIMP GetProperty(UInt32 index, PROPID id, PROPVARIANT* v) override {
        PropVariantInit(v);
        if (id == kpidIsAnti) { v->vt = VT_BOOL; v->boolVal = VARIANT_FALSE; return S_OK; }
        if (index < keep.size()) {
            auto r = rename.find(index);
            if (r == rename.end() || index >= keepEntry.size() || !keepEntry[index]) return S_OK;
            const ZipEntry& e = *keepEntry[index];
            switch (id) {
            case kpidPath: {
                std::wstring p = r->second;
                if (useWinSep) for (auto& c : p) if (c == L'/') c = L'\\';
                v->vt = VT_BSTR; v->bstrVal = SysAllocString(p.c_str());
                break;
            }
            case kpidIsDir: v->vt = VT_BOOL; v->boolVal = e.isDir ? VARIANT_TRUE : VARIANT_FALSE; break;
            case kpidSize: v->vt = VT_UI8; v->uhVal.QuadPart = e.usize; break;
            case kpidAttrib: v->vt = VT_UI4; v->ulVal = e.extAttr ? e.extAttr : (e.isDir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_ARCHIVE); break;
            case kpidMTime: {
                FILETIME ft;
                if (DosDateTimeToFileTime(e.dosDate, e.dosTime, &ft)) { FILETIME u; LocalFileTimeToFileTime(&ft, &u); v->vt = VT_FILETIME; v->filetime = u; }
                break;
            }
            }
            return S_OK;
        }
        const AddItem& it = (*items)[index - keep.size()];
        switch (id) {
        case kpidPath: {
            std::wstring p = it.arcName;
            if (useWinSep) for (auto& c : p) if (c == L'/') c = L'\\';
            v->vt = VT_BSTR; v->bstrVal = SysAllocString(p.c_str());
            break;
        }
        case kpidIsDir: v->vt = VT_BOOL; v->boolVal = it.isDir ? VARIANT_TRUE : VARIANT_FALSE; break;
        case kpidSize: v->vt = VT_UI8; v->uhVal.QuadPart = it.isDir ? 0 : it.size; break;
        case kpidAttrib: v->vt = VT_UI4; v->ulVal = it.isDir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_ARCHIVE; break;
        case kpidMTime: case kpidCTime: case kpidATime: v->vt = VT_FILETIME; v->filetime = it.mtime; break;
        }
        return S_OK;
    }
    STDMETHODIMP GetStream(UInt32 index, ISequentialInStream_** in) override {
        *in = nullptr;
        if (index < keep.size()) return S_OK;
        const AddItem& it = (*items)[index - keep.size()];
        if (it.isDir) return S_OK;
        if (job->onFile) job->onFile(it.arcName);
        FileInStream* f = new FileInStream();
        f->job = job;
        f->h = CreateFileW(LongPath(it.src).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (f->h == INVALID_HANDLE_VALUE) { f->Release(); failMsg = L"파일을 열 수 없습니다: " + it.src; return E_FAIL; }
        *in = f;
        return S_OK;
    }
    STDMETHODIMP SetOperationResult(Int32) override { return S_OK; }
};

static void SetLevel(IUnknown* handler, const std::wstring& fmtName, bool encrypt, bool copyOnly = false) {
    ISetProperties_* sp = nullptr;
    if (handler->QueryInterface(IID_ISetProperties_, (void**)&sp) != S_OK || !sp) return;
    std::vector<const wchar_t*> names;
    std::vector<PROPVARIANT> vals;
    PROPVARIANT v;
    if (copyOnly) {  // 압축 안 되는 파일: 저장만 (7z 'Copy' 방식)
        PropVariantInit(&v); v.vt = VT_BSTR; v.bstrVal = SysAllocString(L"Copy");
        names.push_back(L"0"); vals.push_back(v);
    } else if (fmtName != L"tar") {
        // 7z(LZMA2) 속도는 '파서 방식'이 좌우한다 (사전 크기·fb 는 영향이 작음). 측정 결과에 따라:
        //   빠름: 7-Zip 빠름(x1, 빠른 파서)   → ZIP 과 비슷한 속도
        //   보통: 일반 파서 + HC4 매치 탐색  → 7-Zip 기본(BT4)보다 약 3배 빠르고 크기는 조금 큼
        //   최대: 7-Zip 최대(x9)
        int lv = std::max(1, std::min(9, g_zipLevel));
        PropVariantInit(&v); v.vt = VT_UI4; v.ulVal = lv >= 8 ? 9 : lv <= 3 ? 1 : 5;
        names.push_back(L"x"); vals.push_back(v);
        if (fmtName == L"7z" && lv > 3 && lv < 8) {
            PropVariantInit(&v); v.vt = VT_BSTR; v.bstrVal = SysAllocString(L"HC4");
            names.push_back(L"mf"); vals.push_back(v);
        }
        SYSTEM_INFO si; GetSystemInfo(&si);
        PropVariantInit(&v); v.vt = VT_UI4; v.ulVal = si.dwNumberOfProcessors;  // 모든 코어 사용
        names.push_back(L"mt"); vals.push_back(v);
    }
    if (encrypt && fmtName == L"7z") {  // 파일 이름 목록까지 암호화 (암호 없이는 이름도 볼 수 없음)
        PropVariantInit(&v); v.vt = VT_BOOL; v.boolVal = VARIANT_TRUE;
        names.push_back(L"he"); vals.push_back(v);
    }
    if (!names.empty() && sp->SetProperties(names.data(), vals.data(), (UInt32)names.size()) != S_OK)
        OutputDebugStringW(L"Chagok: 7z SetProperties 실패");
    for (auto& x : vals) PropVariantClear(&x);
    sp->Release();
}

// pending 이 있으면(기존 파일 갱신) 임시 볼륨 이름만 돌려주고 확정은 호출 측에서 한다
static bool SzWrite(const wchar_t* fmtName, IInArchive_* existing, std::vector<UInt32> keep, std::vector<AddItem>& items,
                    const std::wstring& target, uint64_t split, Job& job, std::wstring& err,
                    std::vector<std::wstring>* pending = nullptr, const std::wstring& password = L"", bool copyOnly = false,
                    const std::map<size_t, std::wstring>* rename = nullptr, const std::vector<const ZipEntry*>* keepEntry = nullptr) {
    if (!LoadSz()) { err = L"7z.dll 이 없어 이 형식으로 만들 수 없습니다."; return false; }
    const SzFormat* f = FindFormat(fmtName);
    if (!f) { err = L"7z.dll 에 해당 형식이 없습니다."; return false; }
    IOutArchive_* out = nullptr;
    if (existing) existing->QueryInterface(IID_IOutArchive_, (void**)&out);
    else g_create(&f->clsid, &IID_IOutArchive_, (void**)&out);
    if (!out) { err = L"이 형식은 쓰기를 지원하지 않습니다."; return false; }
    SetLevel(out, Lower(fmtName), !password.empty(), copyOnly);
    VolOutStream* os = new VolOutStream();
    os->base = target; os->split = split; os->job = &job;
    UpdateCb* cb = new UpdateCb();
    cb->items = &items; cb->keep = keep; cb->job = &job; cb->password = password;
    if (rename) cb->rename = *rename;
    if (keepEntry) cb->keepEntry = *keepEntry;
    HRESULT hr = out->UpdateItems(os, (UInt32)(keep.size() + items.size()), cb);
    out->Release();
    bool ok = hr == S_OK && !job.cancel;
    if (!ok) {
        err = job.cancel ? L"취소되었습니다." : !cb->failMsg.empty() ? cb->failMsg : L"압축 중 오류가 발생했습니다.";
        os->Abort();
    }
    cb->Release();
    if (ok) {
        if (pending) {
            // 분할 길이 정리까지는 Finish 와 같게, 이름 바꾸기만 미룬다
            if (os->split) {
                size_t last = os->size ? (size_t)((os->size - 1) / os->split) : 0;
                os->Vol(last);
                LARGE_INTEGER li; li.QuadPart = (LONGLONG)(os->size - (UInt64)last * os->split);
                SetFilePointerEx(os->hs[last], li, nullptr, FILE_BEGIN);
                SetEndOfFile(os->hs[last]);
            } else if (os->hs.empty()) os->Vol(0);
            os->CloseAll();
            *pending = os->names;
            os->names.clear();
        } else ok = os->Finish(target, err);
    }
    os->Release();
    return ok;
}

// 7z: 암호가 맞는지 암호 걸린 항목 하나를 시험 해제해 본다
struct TestCb : Unk<IArchiveExtractCallback_> {
    std::wstring pw;
    Int32 result = -1;
    struct Pw : ICryptoGetTextPassword_ {
        TestCb* o;
        STDMETHODIMP QueryInterface(REFIID r, void** p) override { return o->QueryInterface(r, p); }
        STDMETHODIMP_(ULONG) AddRef() override { return o->AddRef(); }
        STDMETHODIMP_(ULONG) Release() override { return o->Release(); }
        STDMETHODIMP CryptoGetTextPassword(BSTR* p) override { return MakeBstr(o->pw, p); }
    } pwi;
    TestCb() { pwi.o = this; }
    void* Cast(REFIID r) override {
        if (IsEqualIID(r, IID_IArchiveExtractCallback_) || IsEqualIID(r, IID_IProgress_)) return static_cast<IArchiveExtractCallback_*>(this);
        if (IsEqualIID(r, IID_ICryptoGetTextPassword_)) return static_cast<ICryptoGetTextPassword_*>(&pwi);
        return nullptr;
    }
    STDMETHODIMP SetTotal(UInt64) override { return S_OK; }
    STDMETHODIMP SetCompleted(const UInt64*) override { return S_OK; }
    STDMETHODIMP GetStream(UInt32, ISequentialOutStream_** o, Int32) override { *o = nullptr; return S_OK; }
    STDMETHODIMP PrepareOperation(Int32) override { return S_OK; }
    STDMETHODIMP SetOperationResult(Int32 r) override { result = r; return S_OK; }
};
static bool SzTestPassword(AnyArchive& a, const std::wstring& pw) {
    // 가장 작은 암호 항목으로 시험
    int best = -1;
    for (size_t i = 0; i < a.entries.size(); i++)
        if (!a.entries[i].isDir && a.entries[i].Encrypted() && (best < 0 || a.entries[i].usize < a.entries[(size_t)best].usize)) best = (int)i;
    if (best < 0) return true;
    TestCb* cb = new TestCb();
    cb->pw = pw;
    UInt32 idx = a.sz->map[(size_t)best];
    HRESULT hr = a.sz->arc->Extract(&idx, 1, 1, cb);
    bool ok = hr == S_OK && cb->result == kOpOK;
    cb->Release();
    return ok;
}
bool AnyHasPassword(const AnyArchive& a) {
    if (a.isZip) return ZipHasEncrypted(a.zip);
    if (a.sz && !a.sz->password.empty()) return true;
    for (auto& e : a.entries) if (!e.isDir && e.Encrypted()) return true;
    return false;
}

// 암호가 걸린 압축 파일을 고쳐 쓸 때 쓸 암호 (맞는지 확인, 필요하면 묻기)
static bool NeedPassword(AnyArchive& a, Job& job, std::wstring& pw, std::wstring& err, const wchar_t* what) {
    pw.clear();
    if (!AnyHasPassword(a) || a.readOnly) return true;
    std::wstring known = a.isZip ? a.zip.password : a.sz->password;
    bool ok = !known.empty() && (a.isZip ? ZipCheckPassword(a.zip, known) : SzTestPassword(a, known));
    if (ok) pw = known;
    for (int t = 0; !ok && t < 3; t++) {
        std::wstring cand;
        if (!job.askPassword || !job.askPassword(cand) || cand.empty()) { err = std::wstring(L"암호를 입력하지 않아 ") + what; return false; }
        ok = a.isZip ? ZipCheckPassword(a.zip, cand) : SzTestPassword(a, cand);
        if (ok) pw = cand;
    }
    if (!ok) { err = std::wstring(L"암호가 틀려 ") + what; return false; }
    return true;
}

bool AnyModify(AnyArchive& a, const std::map<int, std::wstring>& rename, const std::vector<int>& del, Job& job, std::wstring& err) {
    if (a.readOnly) { err = a.readOnlyWhy; return false; }
    if (a.isZip) return ZipModify(a.zip, rename, del, job, err);  // ZIP 은 암호 없이도 데이터를 그대로 복사
    std::wstring pw;
    if (!NeedPassword(a, job, pw, err, L"바꾸지 않았습니다.")) return false;  // 암호 7z 는 같은 암호로 다시 써야 함
    std::vector<char> gone(a.entries.size(), 0);
    for (int i : del) if (i >= 0 && (size_t)i < gone.size()) gone[(size_t)i] = 1;
    std::vector<UInt32> keep;
    std::vector<const ZipEntry*> keepEntry;
    std::map<size_t, std::wstring> ren;
    uint64_t total = 0;
    for (size_t i = 0; i < a.entries.size(); i++) {
        if (gone[i]) continue;
        auto r = rename.find((int)i);
        if (r != rename.end()) ren[keep.size()] = r->second;
        keep.push_back(a.sz->map[i]);
        keepEntry.push_back(&a.entries[i]);
        total += a.entries[i].usize;
    }
    if (keep.empty()) { err = L"압축 파일 안의 모든 항목을 지울 수는 없습니다."; return false; }
    uint64_t split = 0;
    if (a.isSplit) {
        WIN32_FILE_ATTRIBUTE_DATA d;
        if (GetFileAttributesExW(LongPath(a.volumes[0]).c_str(), GetFileExInfoStandard, &d))
            split = ((uint64_t)d.nFileSizeHigh << 32) | d.nFileSizeLow;
    }
    std::wstring target = a.isSplit ? a.path.substr(0, a.path.find_last_of(L'.')) : a.path;
    std::wstring fmtName = a.sz->fmtInfo->name;
    std::vector<std::wstring> tmp;
    std::vector<AddItem> none;
    job.total = total ? total : 1;
    if (!SzWrite(fmtName.c_str(), a.sz->arc, keep, none, target, split, job, err, &tmp, pw, false, &ren, &keepEntry)) return false;
    std::vector<std::wstring> oldVols = a.volumes;
    a.Close();
    for (auto& v : oldVols) {
        if (!DeleteFileW(LongPath(v).c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND) {
            err = L"기존 압축 파일을 바꿀 수 없습니다(다른 프로그램이 사용 중일 수 있습니다): " + v;
            for (auto& t : tmp) DeleteFileW(LongPath(t).c_str());
            return false;
        }
    }
    return FinalizeTempVolumes(tmp, target, true, err);
}

bool AnyAdd(AnyArchive& a, std::vector<AddItem>& items, Job& job, std::wstring& err) {
    // 암호가 걸린 압축 파일이면 같은 암호로 새 파일도 암호화한다 (맞는 암호인지 먼저 확인)
    std::wstring pw;
    if (AnyHasPassword(a) && !a.readOnly) {
        std::wstring known = a.isZip ? a.zip.password : a.sz->password;
        bool ok = !known.empty() && (a.isZip ? ZipCheckPassword(a.zip, known) : SzTestPassword(a, known));
        if (ok) pw = known;
        for (int t = 0; !ok && t < 3; t++) {
            std::wstring cand;
            if (!job.askPassword || !job.askPassword(cand) || cand.empty()) { err = L"암호를 입력하지 않아 파일을 추가하지 않았습니다."; return false; }
            ok = a.isZip ? ZipCheckPassword(a.zip, cand) : SzTestPassword(a, cand);
            if (ok) pw = cand;
        }
        if (!ok) { err = L"암호가 틀려 파일을 추가하지 않았습니다."; return false; }
    }
    if (a.isZip) {
        bool ok = AddToZip(a.zip, items, job, err, pw);
        return ok;
    }
    if (a.readOnly) { err = a.readOnlyWhy; return false; }
    // 같은 이름(대소문자 무시)은 교체
    std::vector<std::wstring> newKeys;
    for (auto& i : items) newKeys.push_back(Lower(i.arcName));
    std::sort(newKeys.begin(), newKeys.end());
    std::vector<UInt32> keep;
    std::vector<AddItem> add;
    for (size_t i = 0; i < a.entries.size(); i++) {
        bool rep = std::binary_search(newKeys.begin(), newKeys.end(), Lower(a.entries[i].name));
        if (rep && !a.entries[i].isDir) continue;
        keep.push_back(a.sz->map[i]);
    }
    for (auto& it : items) {
        bool dirExists = false;
        if (it.isDir) for (auto& e : a.entries) if (e.isDir && !lstrcmpiW(e.name.c_str(), it.arcName.c_str())) dirExists = true;
        if (!dirExists) add.push_back(it);
    }
    uint64_t split = 0;
    if (a.isSplit) {
        WIN32_FILE_ATTRIBUTE_DATA d;
        if (GetFileAttributesExW(LongPath(a.volumes[0]).c_str(), GetFileExInfoStandard, &d))
            split = ((uint64_t)d.nFileSizeHigh << 32) | d.nFileSizeLow;
    }
    std::wstring target = a.isSplit ? a.path.substr(0, a.path.find_last_of(L'.')) : a.path;
    std::wstring fmtName = a.sz->fmtInfo->name;
    std::vector<std::wstring> tmp;
    if (!SzWrite(fmtName.c_str(), a.sz->arc, keep, add, target, split, job, err, &tmp, pw)) return false;
    std::vector<std::wstring> oldVols = a.volumes;
    a.Close();
    for (auto& v : oldVols) {
        if (!DeleteFileW(LongPath(v).c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND) {
            err = L"기존 압축 파일을 바꿀 수 없습니다(다른 프로그램이 사용 중일 수 있습니다): " + v;
            for (auto& t : tmp) DeleteFileW(LongPath(t).c_str());
            return false;
        }
    }
    return FinalizeTempVolumes(tmp, target, true, err);
}

// ------------------------------------------------------------ 만들기
// 7z 만들기: 동영상·사진·압축 파일처럼 줄어들지 않는 파일은 LZMA2 에 넣지 않고 'Copy'(저장)로 따로 담는다.
// (LZMA2 는 줄지 않는 데이터에도 일치 탐색을 끝까지 해서 매우 느리다)
//  1단계: 압축되는 파일 → 임시 7z (LZMA2, 멀티스레드)
//  2단계: 임시 7z 를 열어 줄지 않는 파일을 Copy 방식으로 추가 → 최종 파일 (기존 데이터는 다시 압축하지 않고 그대로 복사)
static bool Create7z(const std::wstring& target, std::vector<AddItem>& items, uint64_t splitSize, Job& job,
                     std::wstring& err, const std::wstring& password) {
    std::vector<AddItem> comp, raw;
    for (auto& it : items) {
        bool r = !it.isDir && (IsPrecompressedName(it.arcName) || FileLooksIncompressible(it.src, it.size));
        (r ? raw : comp).push_back(it);
    }
    bool anyComp = false;
    for (auto& it : comp) if (!it.isDir) anyComp = true;
    if (raw.empty()) return SzWrite(L"7z", nullptr, {}, items, target, splitSize, job, err, nullptr, password);
    if (!anyComp) return SzWrite(L"7z", nullptr, {}, items, target, splitSize, job, err, nullptr, password, true);
    std::wstring tmp = target + L".~stage1.7z";
    uint64_t compSize = 0, rawSize = 0;
    for (auto& it : comp) compSize += it.size;
    for (auto& it : raw) rawSize += it.size;
    job.total = compSize + rawSize ? compSize + rawSize : 1;
    job.pBase = 0; job.pSpan = compSize ? compSize : 1;
    bool ok1 = SzWrite(L"7z", nullptr, {}, comp, tmp, 0, job, err, nullptr, password);
    job.pBase = compSize; job.pSpan = rawSize ? rawSize : 1;
    if (!ok1) { job.pSpan = 0; DeleteFileW(LongPath(tmp).c_str()); return false; }
    const SzFormat* f = FindFormat(L"7z");
    std::wstring pw = password;
    OpenCb* cb = new OpenCb();
    cb->password = &pw; cb->dir = ParentOf(tmp); cb->curName = FileNameOf(tmp);
    IInArchive_* a = TryOpen(*f, {tmp}, cb);
    cb->Release();
    if (!a) { job.pSpan = 0; DeleteFileW(LongPath(tmp).c_str()); err = L"임시 파일을 다시 열 수 없습니다."; return false; }
    UInt32 n = 0;
    a->GetNumberOfItems(&n);
    std::vector<UInt32> keep(n);
    for (UInt32 i = 0; i < n; i++) keep[i] = i;
    bool ok = SzWrite(L"7z", a, keep, raw, target, splitSize, job, err, nullptr, password, true);
    job.pSpan = 0;
    a->Close(); a->Release();
    DeleteFileW(LongPath(tmp).c_str());
    return ok;
}

bool AnyCanEncrypt(const std::wstring& target) {
    std::wstring low = Lower(target);
    auto ends = [&](const wchar_t* s) { size_t n = wcslen(s); return low.size() >= n && low.compare(low.size() - n, n, s) == 0; };
    return ends(L".zip") || ends(L".7z");
}
bool AnyCreate(const std::wstring& target, std::vector<AddItem>& items, uint64_t splitSize, Job& job, std::wstring& err,
               const std::wstring& password) {
    std::wstring low = Lower(target);
    auto ends = [&](const wchar_t* s) { size_t n = wcslen(s); return low.size() >= n && low.compare(low.size() - n, n, s) == 0; };
    if (!password.empty() && !AnyCanEncrypt(target)) { err = L"TAR / TAR.GZ 형식은 암호를 지원하지 않습니다. ZIP 이나 7z 를 고르세요."; return false; }
    if (ends(L".7z")) return Create7z(target, items, splitSize, job, err, password);
    if (ends(L".tar")) return SzWrite(L"tar", nullptr, {}, items, target, splitSize, job, err);
    if (ends(L".tar.gz") || ends(L".tgz")) {
        // tar 로 묶은 뒤 자체 엔진(멀티코어)으로 gzip
        uint64_t total = 0;
        for (auto& i : items) total += i.size;
        std::wstring tmp = TempPath(L".tar");
        Job inner;
        inner.onFile = job.onFile;
        // 진행률: tar 단계 0~50%, gzip 단계 50~100%
        bool ok;
        {
            std::atomic<bool> stop{false};
            HANDLE th = nullptr;
            struct Ctx { Job* o; Job* i; std::atomic<bool>* stop; };
            Ctx ctx{&job, &inner, &stop};
            th = CreateThread(nullptr, 0, [](LPVOID p) -> DWORD {
                Ctx* c = (Ctx*)p;
                while (!*c->stop) {
                    uint64_t t = c->i->total, d = c->i->done;
                    c->o->done = t ? d * c->o->total / (2 * t) : 0;
                    if (c->o->cancel) c->i->cancel = true;
                    Sleep(100);
                }
                return 0;
            }, &ctx, 0, nullptr);
            job.total = total ? total : 1;
            ok = SzWrite(L"tar", nullptr, {}, items, tmp, 0, inner, err);
            stop = true;
            WaitForSingleObject(th, INFINITE);
            CloseHandle(th);
        }
        if (!ok) { DeleteFileW(tmp.c_str()); if (job.cancel) err = L"취소되었습니다."; return false; }
        WIN32_FILE_ATTRIBUTE_DATA d;
        uint64_t tarSize = 0;
        if (GetFileAttributesExW(tmp.c_str(), GetFileExInfoStandard, &d)) tarSize = ((uint64_t)d.nFileSizeHigh << 32) | d.nFileSizeLow;
        job.total = tarSize * 2; job.done = tarSize;
        std::wstring innerName = fmt::Stem(target) + L".tar";
        innerName = innerName.substr(innerName.find_last_of(L"\\/") + 1);
        ok = GzipCompress(tmp, target, innerName, splitSize, job, err);
        DeleteFileW(tmp.c_str());
        return ok;
    }
    return CreateZip(target, items, splitSize, job, err, password);
}
