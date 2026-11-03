// quickname.h - 우클릭 '바로 압축'의 이름 규칙
// 메뉴에 보이는 이름과 차곡이 실제로 만드는 파일 이름이 같아야 하므로 셸 확장(ChagokShell.dll)과 차곡(Chagok.exe)이 함께 쓴다.
//   하나만 골랐을 때      : 그 항목 이름 (파일은 확장자 뺌)      예) 사진 → 사진.zip, 보고서.hwp → 보고서.zip
//   여러 개 골랐을 때     : 들어 있는 폴더 이름                  예) 자료\a.hwp, 자료\b.xlsx → 자료.zip
//   '누른 파일 이름으로'   : 우클릭한(첫) 항목 이름
//   날짜 붙이기           : 이름_20261103
//   확장자                : 설정의 프리셋 (빠름 = .zip, 보통·최대 = .7z — 7z.dll 이 있을 때만)
#pragma once
#include <windows.h>
#include <string>
#include <vector>

inline bool QnIsDir(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}
inline std::wstring QnTrim(std::wstring p) {
    while (p.size() > 3 && (p.back() == L'\\' || p.back() == L'/')) p.pop_back();
    return p;
}
inline std::wstring QnLeaf(const std::wstring& path) {
    std::wstring p = QnTrim(path);
    size_t s = p.find_last_of(L"\\/");
    return s == std::wstring::npos ? p : p.substr(s + 1);
}
inline std::wstring QnParent(const std::wstring& path) {
    std::wstring p = QnTrim(path);
    size_t s = p.find_last_of(L"\\/");
    if (s == std::wstring::npos) return L"";
    if (s == 2 && p[1] == L':') return p.substr(0, 3);  // 드라이브 맨 위는 C:\ 그대로
    return p.substr(0, s);
}
inline std::wstring QnSafe(std::wstring n) {
    if (n.empty() || n.back() == L':' || n == L"\\") return L"압축";
    return n;
}
// 항목 하나의 이름 (파일이면 확장자 뺌. 'a.tar.gz' 는 'a.tar')
inline std::wstring QnStem(const std::wstring& path) {
    std::wstring n = QnLeaf(path);
    if (!QnIsDir(path)) { size_t d = n.find_last_of(L'.'); if (d != std::wstring::npos && d > 0) n = n.substr(0, d); }
    return QnSafe(n);
}
// 폴더 이름 규칙: 하나면 그 항목, 여러 개면 들어 있는 폴더
inline std::wstring QnFolderStem(const std::vector<std::wstring>& paths) {
    if (paths.empty()) return L"압축";
    if (paths.size() == 1) return QnStem(paths[0]);
    return QnSafe(QnLeaf(QnParent(paths[0])));
}
inline std::wstring QnToday() {
    SYSTEMTIME t; GetLocalTime(&t);
    wchar_t b[16]; wsprintfW(b, L"%04d%02d%02d", t.wYear, t.wMonth, t.wDay);
    return b;
}
// 프리셋에 맞는 확장자. exeDir: 7z.dll 이 있는 차곡 폴더 (끝에 \ 있어도 됨)
inline std::wstring QnExt(const std::wstring& exeDir) {
    DWORD preset = 0, sz = sizeof(preset);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Sinsul\\Chagok", L"Preset", RRF_RT_REG_DWORD, nullptr, &preset, &sz);
    if (preset == 0) return L".zip";
    std::wstring d = exeDir;
    if (!d.empty() && d.back() != L'\\') d += L'\\';
    DWORD a = GetFileAttributesW((d + L"7z.dll").c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY) ? L".7z" : L".zip";
}
