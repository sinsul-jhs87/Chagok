// ChagokShell.dll 테스트: 선택한 경로들로 메뉴를 만들어 항목을 출력하고, 지정한 항목을 실행해 본다.
//  menutest.exe <dll> <invoke-index|-1> <path...>
#include <windows.h>
#include <shlobj.h>
#include <cstdio>
#include <string>
#include <vector>

static const CLSID CLSID_ChagokShell = {0x8d0c9dd2, 0x90a8, 0x492a, {0xae, 0xe8, 0x45, 0xaf, 0x3c, 0x6d, 0x4e, 0xfd}};

class DataObj : public IDataObject {
    LONG ref = 1;
    std::vector<std::wstring> paths;
public:
    DataObj(const std::vector<std::wstring>& p) : paths(p) {}
    virtual ~DataObj() {}
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDataObject) { *ppv = this; AddRef(); return S_OK; }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ++ref; }
    STDMETHODIMP_(ULONG) Release() override { LONG r = --ref; if (!r) delete this; return r; }
    STDMETHODIMP GetData(FORMATETC* fe, STGMEDIUM* sm) override {
        if (fe->cfFormat != CF_HDROP) return DV_E_FORMATETC;
        size_t chars = 1;
        for (auto& p : paths) chars += p.size() + 1;
        HGLOBAL g = GlobalAlloc(GHND, sizeof(DROPFILES) + chars * sizeof(wchar_t));
        DROPFILES* df = (DROPFILES*)GlobalLock(g);
        df->pFiles = sizeof(DROPFILES); df->fWide = TRUE;
        wchar_t* d = (wchar_t*)(df + 1);
        for (auto& p : paths) { memcpy(d, p.c_str(), (p.size() + 1) * sizeof(wchar_t)); d += p.size() + 1; }
        *d = 0;
        GlobalUnlock(g);
        sm->tymed = TYMED_HGLOBAL; sm->hGlobal = g; sm->pUnkForRelease = nullptr;
        return S_OK;
    }
    STDMETHODIMP GetDataHere(FORMATETC*, STGMEDIUM*) override { return E_NOTIMPL; }
    STDMETHODIMP QueryGetData(FORMATETC* fe) override { return fe->cfFormat == CF_HDROP ? S_OK : DV_E_FORMATETC; }
    STDMETHODIMP GetCanonicalFormatEtc(FORMATETC*, FORMATETC*) override { return E_NOTIMPL; }
    STDMETHODIMP SetData(FORMATETC*, STGMEDIUM*, BOOL) override { return E_NOTIMPL; }
    STDMETHODIMP EnumFormatEtc(DWORD, IEnumFORMATETC**) override { return E_NOTIMPL; }
    STDMETHODIMP DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override { return E_NOTIMPL; }
    STDMETHODIMP DUnadvise(DWORD) override { return E_NOTIMPL; }
    STDMETHODIMP EnumDAdvise(IEnumSTATDATA**) override { return E_NOTIMPL; }
};

static void Out(const std::wstring& s) {
    int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string u(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, &u[0], n, nullptr, nullptr);
    printf("%s\n", u.c_str());
}

int wmain(int argc, wchar_t** argv) {
    CoInitialize(nullptr);
    HMODULE m = LoadLibraryW(argv[1]);
    if (!m) { printf("LoadLibrary 실패\n"); return 1; }
    auto get = (HRESULT(__stdcall*)(REFCLSID, REFIID, void**))GetProcAddress(m, "DllGetClassObject");
    IClassFactory* cf = nullptr;
    if (!get || FAILED(get(CLSID_ChagokShell, IID_IClassFactory, (void**)&cf))) { printf("팩토리 실패\n"); return 1; }
    IShellExtInit* init = nullptr;
    cf->CreateInstance(nullptr, IID_IShellExtInit, (void**)&init);
    std::vector<std::wstring> paths;
    for (int i = 3; i < argc; i++) { wchar_t f[4096]; GetFullPathNameW(argv[i], 4096, f, nullptr); paths.push_back(f); }
    DataObj* d = new DataObj(paths);
    if (FAILED(init->Initialize(nullptr, d, nullptr))) { printf("Initialize 실패\n"); return 1; }
    IContextMenu* cm = nullptr;
    init->QueryInterface(IID_IContextMenu, (void**)&cm);
    HMENU h = CreatePopupMenu();
    AppendMenuW(h, MF_STRING, 1, L"(기존 항목)");
    HRESULT hr = cm->QueryContextMenu(h, 1, 100, 200, CMF_NORMAL);
    printf("사용한 ID 수: %d\n", HRESULT_CODE(hr));
    for (int i = 0; i < GetMenuItemCount(h); i++) {
        wchar_t t[256] = {};
        MENUITEMINFOW mi{sizeof(mi)};
        mi.fMask = MIIM_STRING | MIIM_SUBMENU | MIIM_FTYPE | MIIM_BITMAP;
        mi.dwTypeData = t; mi.cch = 256;
        GetMenuItemInfoW(h, i, TRUE, &mi);
        if (mi.fType & MFT_SEPARATOR) { Out(L"  ──"); continue; }
        Out(std::wstring(L"  ") + t + (mi.hSubMenu ? L" ▸" : L"") + (mi.hbmpItem ? L"  [아이콘]" : L""));
        if (mi.hSubMenu) {
            for (int j = 0; j < GetMenuItemCount(mi.hSubMenu); j++) {
                wchar_t s[256] = {};
                MENUITEMINFOW si{sizeof(si)};
                si.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_ID | MIIM_SUBMENU;
                si.dwTypeData = s; si.cch = 256;
                GetMenuItemInfoW(mi.hSubMenu, j, TRUE, &si);
                if (si.fType & MFT_SEPARATOR) Out(L"      ──");
                else if (si.hSubMenu) {
                    Out(std::wstring(L"      ") + s + L" ▸");
                    for (int k = 0; k < GetMenuItemCount(si.hSubMenu); k++) {
                        wchar_t c[256] = {}; MENUITEMINFOW ci2{sizeof(ci2)};
                        ci2.fMask = MIIM_STRING | MIIM_ID; ci2.dwTypeData = c; ci2.cch = 256;
                        GetMenuItemInfoW(si.hSubMenu, k, TRUE, &ci2);
                        wchar_t verb[64] = {};
                        cm->GetCommandString(ci2.wID - 100, GCS_VERBW, nullptr, (LPSTR)verb, 64);
                        Out(std::wstring(L"          ") + std::to_wstring(ci2.wID - 100) + L". " + c + L"   (" + verb + L")");
                    }
                } else {
                    wchar_t verb[64] = {};
                    cm->GetCommandString(si.wID - 100, GCS_VERBW, nullptr, (LPSTR)verb, 64);
                    Out(std::wstring(L"      ") + std::to_wstring(si.wID - 100) + L". " + s + L"   (" + verb + L")");
                }
            }
        }
    }
    int inv = _wtoi(argv[2]);
    if (inv >= 0) {
        CMINVOKECOMMANDINFO ci{sizeof(ci)};
        ci.lpVerb = MAKEINTRESOURCEA(inv);
        ci.nShow = SW_SHOWNORMAL;
        printf("실행 결과: %s\n", SUCCEEDED(cm->InvokeCommand(&ci)) ? "OK" : "실패");
    }
    cm->Release(); init->Release(); cf->Release();
    return 0;
}
