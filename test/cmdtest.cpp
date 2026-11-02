// Windows 11 메뉴(IExplorerCommand) 동작 확인: cmdtest <dll> <경로...>
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <cstdio>
#include <string>
#include <vector>
static void P(const std::wstring& s) { int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, 0, 0, 0, 0); std::string u(n, 0); WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, &u[0], n, 0, 0); printf("%s\n", u.c_str()); }
int wmain(int argc, wchar_t** argv) {
    CoInitialize(nullptr);
    HMODULE m = LoadLibraryW(argv[1]);
    auto get = (HRESULT(__stdcall*)(REFCLSID, REFIID, void**))GetProcAddress(m, "DllGetClassObject");
    const CLSID c = {0x3bf35a03, 0x70ec, 0x4eef, {0x9c, 0xe7, 0xfe, 0x2a, 0xd7, 0x4d, 0x76, 0xa9}};
    IClassFactory* f = nullptr;
    if (FAILED(get(c, IID_IClassFactory, (void**)&f))) { P(L"no factory"); return 1; }
    IExplorerCommand* root = nullptr;
    f->CreateInstance(nullptr, IID_IExplorerCommand, (void**)&root);
    std::vector<PIDLIST_ABSOLUTE> ids;
    for (int i = 2; i < argc; i++) { PIDLIST_ABSOLUTE p = nullptr; if (SUCCEEDED(SHParseDisplayName(argv[i], nullptr, &p, 0, nullptr))) ids.push_back(p); }
    IShellItemArray* arr = nullptr;
    HRESULT hr = SHCreateShellItemArrayFromIDLists((UINT)ids.size(), (PCIDLIST_ABSOLUTE_ARRAY)ids.data(), &arr);
    if (FAILED(hr)) { P(L"no array"); return 1; }
    LPWSTR t = nullptr; EXPCMDSTATE st; EXPCMDFLAGS fl;
    root->GetTitle(arr, &t); root->GetState(arr, TRUE, &st); root->GetFlags(&fl);
    LPWSTR ic = nullptr; root->GetIcon(arr, &ic);
    P(std::wstring(L"ROOT ") + t + L" state=" + std::to_wstring(st) + L" flags=" + std::to_wstring(fl) + L" icon=" + (ic ? ic : L"-"));
    IEnumExplorerCommand* e = nullptr;
    root->EnumSubCommands(&e);
    IExplorerCommand* sc; ULONG got;
    while (e->Next(1, &sc, &got) == S_OK) {
        LPWSTR n = nullptr; sc->GetTitle(arr, &n); sc->GetState(arr, TRUE, &st); sc->GetFlags(&fl);
        if (st != ECS_HIDDEN) P(std::wstring(L"  ") + (fl & ECF_ISSEPARATOR ? L"────" : n));
        CoTaskMemFree(n); sc->Release();
    }
    e->Release(); root->Release(); f->Release();
    return 0;
}
