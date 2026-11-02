// formats.h - 압축 파일로 다룰 확장자 (Chagok.exe 와 ChagokShell.dll 공용, 헤더만)
#pragma once
#include <windows.h>
#include <string>

namespace fmt {

// 자체 ZIP 엔진으로 여는 형식 (ZIP 과 같은 구조·같은 deflate 알고리즘)
static const wchar_t* const ZIP_EXTS[] = {L"zip", L"jar", L"war", L"ear", L"apk", L"xpi", L"ipa", L"aab"};
// 7z.dll 로 여는 형식
static const wchar_t* const SZ_EXTS[] = {
    L"7z", L"rar", L"tar", L"gz", L"tgz", L"bz2", L"tbz", L"tbz2", L"xz", L"txz", L"lzma", L"tlz", L"z", L"taz",
    L"zst", L"tzst", L"cab", L"iso", L"img", L"wim", L"swm", L"lzh", L"lha", L"arj", L"cpio", L"rpm", L"deb",
    L"dmg", L"vhd", L"vhdx", L"vmdk", L"xar", L"squashfs"};
// 새로 만들 수 있는 형식 (저장 창의 '파일 형식')
//   zip  : 자체 엔진 (멀티코어)
//   7z   : 7z.dll LZMA2 (멀티스레드)
//   tar  : 7z.dll (압축 없음, 묶기만)
//   tgz  : tar + 자체 엔진 멀티코어 gzip

inline std::wstring Lower(std::wstring s) {
    if (!s.empty()) CharLowerBuffW(&s[0], (DWORD)s.size());
    return s;
}
inline std::wstring Ext(const std::wstring& path) {
    size_t d = path.find_last_of(L'.'), s = path.find_last_of(L"\\/");
    if (d == std::wstring::npos || (s != std::wstring::npos && d < s)) return L"";
    return Lower(path.substr(d + 1));
}
inline bool Digits(const std::wstring& s, size_t from = 0) {
    if (s.size() <= from) return false;
    for (size_t i = from; i < s.size(); i++) if (s[i] < L'0' || s[i] > L'9') return false;
    return true;
}
inline bool In(const std::wstring& e, const wchar_t* const* list, size_t n) {
    for (size_t i = 0; i < n; i++) if (e == list[i]) return true;
    return false;
}
#define FMT_COUNT(a) (sizeof(a) / sizeof(a[0]))
inline bool IsZipExt(const std::wstring& e) { return In(e, ZIP_EXTS, FMT_COUNT(ZIP_EXTS)) || (e.size() >= 3 && e[0] == L'z' && Digits(e, 1)); }
inline bool IsSzExt(const std::wstring& e) { return In(e, SZ_EXTS, FMT_COUNT(SZ_EXTS)); }

// 경로가 압축 파일인가: name.zip / name.z01 / name.7z / name.tar.gz / name.7z.001 …
inline bool IsArchivePath(const std::wstring& path) {
    std::wstring e = Ext(path);
    if (e.empty()) return false;
    if (IsZipExt(e) || IsSzExt(e)) return true;
    if (e.size() >= 3 && Digits(e)) {  // 분할 조각 .001: 앞 확장자가 압축 형식이어야 함
        std::wstring inner = Ext(path.substr(0, path.size() - e.size() - 1));
        return IsZipExt(inner) || IsSzExt(inner);
    }
    return false;
}
// 이 경로를 자체 ZIP 엔진으로 여는가
inline bool UsesZipEngine(const std::wstring& path) {
    std::wstring e = Ext(path);
    if (e.size() >= 3 && Digits(e)) e = Ext(path.substr(0, path.size() - e.size() - 1));
    return IsZipExt(e);
}
// 확장자를 뗀 이름: a.zip → a, a.tar.gz → a, a.7z.001 → a, a.z01 → a
inline std::wstring Stem(const std::wstring& path) {
    std::wstring n = path.substr(path.find_last_of(L"\\/") + 1);
    for (int k = 0; k < 3; k++) {
        size_t d = n.find_last_of(L'.');
        if (d == std::wstring::npos || d == 0) break;
        std::wstring e = Lower(n.substr(d + 1));
        bool strip = IsZipExt(e) || IsSzExt(e) || Digits(e);
        if (!strip) break;
        n = n.substr(0, d);
        if (!Digits(e) && e != L"gz" && e != L"bz2" && e != L"xz" && e != L"z" && e != L"zst" && e != L"lzma") break;  // a.tar.gz 는 두 번
    }
    // RAR 다중 볼륨: name.part1.rar → name
    size_t d = n.find_last_of(L'.');
    if (d != std::wstring::npos && d > 0) {
        std::wstring e = Lower(n.substr(d + 1));
        if (e.size() > 4 && e.compare(0, 4, L"part") == 0 && Digits(e, 4)) n = n.substr(0, d);
    }
    return n;
}

}  // namespace fmt
