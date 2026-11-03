#pragma once
#include <string>
#include <vector>
#include <guiddef.h>
struct ChagokMenuItem { std::wstring verb, label; };  // verb 가 - 로 시작하면 구분선
extern const CLSID CLSID_ChagokShell;
extern const CLSID CLSID_ChagokCommand;
#define CHAGOK_COMMAND_CLSID_STR L"{3BF35A03-70EC-4EEF-9CE7-FE2AD74D76A9}"
#define CHAGOK_CLSID_STR L"{8D0C9DD2-90A8-492A-AEE8-45AF3C6D4EFD}"
bool ChagokIsArchive(const std::wstring& path);
std::wstring ChagokArchiveStem(const std::wstring& path);
std::vector<ChagokMenuItem> ChagokBuildMenu(const std::vector<std::wstring>& paths);
bool ChagokLaunch(const std::wstring& exe, const std::wstring& verb, const std::vector<std::wstring>& paths);
