// preview.cpp - 탐색기 미리 보기 창(Alt+P)용 압축 파일 미리 보기 (ChagokPreview.dll)
//  탐색기가 아닌 Windows 미리 보기 호스트(prevhost.exe) 안에서 돈다. 압축 파일을 열어 목록만 보여 준다(풀지 않음).
//  ZIP 계열은 차곡 자체 엔진, 그 밖(7z·rar·tar·iso …)은 같은 폴더의 7z.dll — main 과 같은 archive 계층(szarc)을 쓴다.
//  등록(HKCU): CLSID + 확장자별 ShellEx\{8895B1C6-…}(IPreviewHandler) — Chagok.exe /register-assoc 가 한다.
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <propsys.h>
#include <shlwapi.h>
#include <uxtheme.h>
#include <algorithm>
#include <string>
#include <vector>
#include "archive.h"

// {6A1C2D0E-5B7F-4C3A-9E21-8F4D2B7A1C55}
static const CLSID CLSID_ChagokPreview = {0x6a1c2d0e, 0x5b7f, 0x4c3a, {0x9e, 0x21, 0x8f, 0x4d, 0x2b, 0x7a, 0x1c, 0x55}};
static HINSTANCE g_inst;
static volatile LONG g_refs = 0;

static std::wstring Size(uint64_t b) {
    wchar_t s[64];
    if (b < 1024) swprintf(s, 64, L"%llu B", (unsigned long long)b);
    else if (b < 1024ull * 1024) swprintf(s, 64, L"%.0f KB", b / 1024.0);
    else if (b < 1024ull * 1024 * 1024) swprintf(s, 64, L"%.1f MB", b / 1048576.0);
    else swprintf(s, 64, L"%.2f GB", b / 1073741824.0);
    return s;
}
static std::wstring DosDate(uint16_t d, uint16_t t) {
    if (!d) return L"";
    wchar_t s[32];
    swprintf(s, 32, L"%04d-%02d-%02d %02d:%02d", 1980 + (d >> 9), (d >> 5) & 15, d & 31, t >> 11, (t >> 5) & 63);
    return s;
}

// ---------------------------------------------------------------- 미리 보기 창 (머리글 + 목록)
struct View {
    HWND h = nullptr, list = nullptr;
    HFONT font = nullptr, bold = nullptr;
    std::wstring title, summary, error;
    struct Row { std::wstring name, size, date; bool dir; };
    std::vector<Row> rows;
};
static const int HEAD_H = 54;
static LRESULT CALLBACK ViewProc(HWND h, UINT m, WPARAM w, LPARAM lp) {
    View* v = (View*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (m) {
    case WM_NCCREATE: SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTW*)lp)->lpCreateParams); break;
    case WM_SIZE:
        if (v && v->list) {
            int cw = LOWORD(lp), ch = HIWORD(lp);
            MoveWindow(v->list, 0, HEAD_H, cw, std::max(0, ch - HEAD_H), TRUE);
            int nameW = std::max(120, cw - 230);
            ListView_SetColumnWidth(v->list, 0, nameW);
        }
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT cr; GetClientRect(h, &cr);
        RECT hr{0, 0, cr.right, v && v->list ? HEAD_H : cr.bottom};
        FillRect(dc, &hr, GetSysColorBrush(COLOR_WINDOW));
        if (v) {
            SetBkMode(dc, TRANSPARENT);
            HGDIOBJ of = SelectObject(dc, v->bold);
            SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
            RECT t{12, 8, cr.right - 12, 30};
            DrawTextW(dc, v->title.c_str(), -1, &t, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            SelectObject(dc, v->font);
            SetTextColor(dc, GetSysColor(COLOR_GRAYTEXT));
            RECT t2{12, 30, cr.right - 12, v->list ? 50 : cr.bottom - 8};
            DrawTextW(dc, (v->error.empty() ? v->summary : v->error).c_str(), -1, &t2, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
            SelectObject(dc, of);
        }
        EndPaint(h, &ps);
        return 0;
    }
    case WM_NOTIFY: {
        NMHDR* nh = (NMHDR*)lp;
        if (v && nh->hwndFrom == v->list && nh->code == LVN_GETDISPINFOW) {
            NMLVDISPINFOW* di = (NMLVDISPINFOW*)lp;
            if ((di->item.mask & LVIF_TEXT) && di->item.iItem >= 0 && (size_t)di->item.iItem < v->rows.size()) {
                const View::Row& r = v->rows[(size_t)di->item.iItem];
                const std::wstring& t = di->item.iSubItem == 0 ? r.name : di->item.iSubItem == 1 ? r.size : r.date;
                lstrcpynW(di->item.pszText, t.c_str(), di->item.cchTextMax);
            }
            return 0;
        }
        break;
    }
    }
    return DefWindowProcW(h, m, w, lp);
}

static void FillView(View& v, const std::wstring& path) {
    v.title = path.substr(path.find_last_of(L"\\/") + 1);
    AnyArchive a;
    std::wstring err;
    // 머리글까지 암호화된 7z 등은 암호를 묻지 않고 '목록을 볼 수 없음'으로
    if (!a.Open(path, err, [](std::wstring&) { return false; })) {
        v.error = L"목록을 볼 수 없습니다." + (err.empty() ? std::wstring() : L"\n" + err);
        return;
    }
    uint64_t us = 0, cs = 0;
    size_t files = 0, dirs = 0;
    bool locked = false;
    std::vector<const ZipEntry*> es;
    for (auto& e : a.entries) {
        es.push_back(&e);
        if (e.isDir) dirs++; else { files++; us += e.usize; if (e.csize != UINT64_MAX) cs += e.csize; }
        if (e.Encrypted()) locked = true;
    }
    std::sort(es.begin(), es.end(), [](const ZipEntry* x, const ZipEntry* y) { return StrCmpLogicalW(x->name.c_str(), y->name.c_str()) < 0; });
    const size_t MAXROWS = 20000;  // 아주 큰 목록은 앞부분만
    for (auto* e : es) {
        if (v.rows.size() >= MAXROWS) break;
        std::wstring n = e->name;
        for (auto& c : n) if (c == L'/') c = L'\\';
        v.rows.push_back({e->isDir ? n + L"\\" : n, e->isDir ? L"" : Size(e->usize), DosDate(e->dosDate, e->dosTime)});
    }
    v.summary = a.format + L" · 파일 " + std::to_wstring(files) + L"개" + (dirs ? L" · 폴더 " + std::to_wstring(dirs) + L"개" : L"") +
                L" · " + Size(us) + (cs && cs < us ? L" → " + Size(cs) : L"") + (locked ? L" · 암호" : L"") +
                (es.size() > MAXROWS ? L" · 앞 " + std::to_wstring(MAXROWS) + L"개만 표시" : L"");
}

// ---------------------------------------------------------------- COM 객체
class Preview : public IPreviewHandler, public IInitializeWithFile, public IObjectWithSite, public IOleWindow {
    LONG ref = 1;
    std::wstring path;
    HWND parent = nullptr;
    RECT rc{};
    IUnknown* site = nullptr;
    View* view = nullptr;
public:
    Preview() { InterlockedIncrement(&g_refs); }
    virtual ~Preview() { Unload(); if (site) site->Release(); InterlockedDecrement(&g_refs); }
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IPreviewHandler)) *ppv = static_cast<IPreviewHandler*>(this);
        else if (IsEqualIID(riid, IID_IInitializeWithFile)) *ppv = static_cast<IInitializeWithFile*>(this);
        else if (IsEqualIID(riid, IID_IObjectWithSite)) *ppv = static_cast<IObjectWithSite*>(this);
        else if (IsEqualIID(riid, IID_IOleWindow)) *ppv = static_cast<IOleWindow*>(this);
        else { *ppv = nullptr; return E_NOINTERFACE; }
        AddRef();
        return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return (ULONG)InterlockedIncrement(&ref); }
    STDMETHODIMP_(ULONG) Release() override { LONG r = InterlockedDecrement(&ref); if (!r) delete this; return (ULONG)r; }
    // IInitializeWithFile
    STDMETHODIMP Initialize(LPCWSTR p, DWORD) override { path = p ? p : L""; return path.empty() ? E_INVALIDARG : S_OK; }
    // IObjectWithSite
    STDMETHODIMP SetSite(IUnknown* s) override { if (site) site->Release(); site = s; if (site) site->AddRef(); return S_OK; }
    STDMETHODIMP GetSite(REFIID riid, void** ppv) override { if (!site) { *ppv = nullptr; return E_FAIL; } return site->QueryInterface(riid, ppv); }
    // IOleWindow
    STDMETHODIMP GetWindow(HWND* h) override { *h = view ? view->h : parent; return *h ? S_OK : E_FAIL; }
    STDMETHODIMP ContextSensitiveHelp(BOOL) override { return E_NOTIMPL; }
    // IPreviewHandler
    STDMETHODIMP SetWindow(HWND h, const RECT* r) override {
        parent = h; if (r) rc = *r;
        if (view && view->h) { SetParent(view->h, parent); MoveWindow(view->h, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE); }
        return S_OK;
    }
    STDMETHODIMP SetRect(const RECT* r) override {
        if (!r) return E_INVALIDARG;
        rc = *r;
        if (view && view->h) MoveWindow(view->h, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
        return S_OK;
    }
    STDMETHODIMP DoPreview() override {
        if (view || !parent) return view ? S_OK : E_FAIL;
        INITCOMMONCONTROLSEX ic{sizeof(ic), ICC_LISTVIEW_CLASSES};
        InitCommonControlsEx(&ic);
        static bool reg = false;
        if (!reg) {
            WNDCLASSW wc{};
            wc.lpfnWndProc = ViewProc; wc.hInstance = g_inst; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc.lpszClassName = L"ChagokPreviewView";
            RegisterClassW(&wc);
            reg = true;
        }
        view = new View();
        FillView(*view, path);
        NONCLIENTMETRICSW nm{sizeof(nm)};
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(nm), &nm, 0);
        view->font = CreateFontIndirectW(&nm.lfMessageFont);
        LOGFONTW lf = nm.lfMessageFont; lf.lfWeight = FW_BOLD;
        view->bold = CreateFontIndirectW(&lf);
        view->h = CreateWindowExW(0, L"ChagokPreviewView", L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, rc.left, rc.top,
                                  rc.right - rc.left, rc.bottom - rc.top, parent, nullptr, g_inst, view);
        if (view->error.empty()) {
            view->list = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_OWNERDATA | LVS_NOSORTHEADER | LVS_SHOWSELALWAYS,
                                         0, HEAD_H, rc.right - rc.left, std::max(0L, rc.bottom - rc.top - HEAD_H), view->h, nullptr, g_inst, nullptr);
            SendMessageW(view->list, WM_SETFONT, (WPARAM)view->font, TRUE);
            ListView_SetExtendedListViewStyle(view->list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
            SetWindowTheme(view->list, L"Explorer", nullptr);
            LVCOLUMNW c{};
            c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
            c.pszText = (LPWSTR)L"이름"; c.cx = std::max(120L, rc.right - rc.left - 230); c.fmt = LVCFMT_LEFT;
            ListView_InsertColumn(view->list, 0, &c);
            c.pszText = (LPWSTR)L"크기"; c.cx = 80; c.fmt = LVCFMT_RIGHT;
            ListView_InsertColumn(view->list, 1, &c);
            c.pszText = (LPWSTR)L"수정한 날짜"; c.cx = 130; c.fmt = LVCFMT_LEFT;
            ListView_InsertColumn(view->list, 2, &c);
            ListView_SetItemCountEx(view->list, (int)view->rows.size(), LVSICF_NOINVALIDATEALL);
        }
        return S_OK;
    }
    STDMETHODIMP Unload() override {
        if (view) {
            if (view->h) DestroyWindow(view->h);
            if (view->font) DeleteObject(view->font);
            if (view->bold) DeleteObject(view->bold);
            delete view;
            view = nullptr;
        }
        return S_OK;
    }
    STDMETHODIMP SetFocus() override { if (view && view->list) ::SetFocus(view->list); return S_OK; }
    STDMETHODIMP QueryFocus(HWND* h) override { *h = GetFocus(); return S_OK; }
    STDMETHODIMP TranslateAccelerator(MSG* msg) override {
        IPreviewHandlerFrame* fr = nullptr;
        HRESULT hr = S_FALSE;
        if (site && SUCCEEDED(site->QueryInterface(IID_IPreviewHandlerFrame, (void**)&fr)) && fr) { hr = fr->TranslateAccelerator(msg); fr->Release(); }
        return hr;
    }
};

class Factory : public IClassFactory {
    LONG ref = 1;
public:
    virtual ~Factory() {}
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) { *ppv = this; AddRef(); return S_OK; }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return (ULONG)InterlockedIncrement(&ref); }
    STDMETHODIMP_(ULONG) Release() override { LONG r = InterlockedDecrement(&ref); if (!r) delete this; return (ULONG)r; }
    STDMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override {
        if (outer) return CLASS_E_NOAGGREGATION;
        Preview* p = new Preview();
        HRESULT hr = p->QueryInterface(riid, ppv);
        p->Release();
        return hr;
    }
    STDMETHODIMP LockServer(BOOL lock) override { if (lock) InterlockedIncrement(&g_refs); else InterlockedDecrement(&g_refs); return S_OK; }
};

extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID clsid, REFIID riid, void** ppv) {
    if (!IsEqualCLSID(clsid, CLSID_ChagokPreview)) return CLASS_E_CLASSNOTAVAILABLE;
    Factory* f = new Factory();
    HRESULT hr = f->QueryInterface(riid, ppv);
    f->Release();
    return hr;
}
extern "C" HRESULT __stdcall DllCanUnloadNow() { return g_refs == 0 ? S_OK : S_FALSE; }
extern "C" BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { g_inst = h; DisableThreadLibraryCalls(h); }
    return TRUE;
}
