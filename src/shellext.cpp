// shellext.cpp - 탐색기 우클릭 메뉴 확장 (ChagokShell.dll)
//  1) 클래식 IContextMenu: HKCU 에 등록. Windows 10, 그리고 Windows 11 의 '추가 옵션 표시' 안
//  2) IExplorerCommand: Windows 11 우클릭 첫 화면. 사내 인증서로 서명한 스파스 패키지(ChagokShell.msix)가
//     이 DLL 을 COM 서버로 등록한다 (installer/package/AppxManifest.xml)
//
//  선택이 압축 파일 1개   → 차곡 ▸ 알아서 풀기 / 현재 폴더에 압축 풀기 / 파일 이름 폴더에 풀기 / 풀 위치 선택… / ─ / 차곡으로 열기
//     (알아서 풀기: 맨 위 항목이 하나면 현재 폴더에, 여러 개면 파일 이름 폴더를 만들어 그 안에)
//     + 알아서 풀고 압축 파일 지우기 (휴지통으로)
//  그 밖(파일·폴더·여러 개) → 차곡 ▸ 자료.zip 으로 압축 / (여러 개면) 누른파일.zip 으로 압축 / 자료_20261103.zip 으로 압축
//                              / (여러 개면) 각각 압축 / 암호 걸어 압축… / ─ / 압축하기… / 분할 압축… / ─ / 차곡으로 열기
//  apk·iso 같은 '압축이 목표가 아닌' 형식(formats.h PACKAGE_EXTS) 하나 → 위 압축 메뉴 + ─ + 알아서 풀기 / 풀 위치 선택…
//     이름 규칙은 quickname.h (차곡과 함께 씀). 바로 압축 항목은 설정 창 없이 바로 만든다
#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include "shellext.h"
#include "formats.h"
#include "quickname.h"
#include "resource.h"

static HINSTANCE g_inst;
static volatile LONG g_refs = 0;
// {8D0C9DD2-90A8-492A-AEE8-45AF3C6D4EFD}
const CLSID CLSID_ChagokShell = {0x8d0c9dd2, 0x90a8, 0x492a, {0xae, 0xe8, 0x45, 0xaf, 0x3c, 0x6d, 0x4e, 0xfd}};
// {3BF35A03-70EC-4EEF-9CE7-FE2AD74D76A9} Windows 11 첫 화면 메뉴 (IExplorerCommand)
const CLSID CLSID_ChagokCommand = {0x3bf35a03, 0x70ec, 0x4eef, {0x9c, 0xe7, 0xfe, 0x2a, 0xd7, 0x4d, 0x76, 0xa9}};

// ------------------------------------------------------------ 메뉴 구성 (테스트에서도 사용)
bool ChagokIsArchive(const std::wstring& path) {
    DWORD a = GetFileAttributesW(path.c_str());
    if (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY)) return false;
    return fmt::IsArchivePath(path);
}
std::wstring ChagokArchiveStem(const std::wstring& path) { return fmt::Stem(path); }
static std::wstring ModuleDir();
// '자료.zip 으로' / '자료.7z 로' (받침에 따라)
static std::wstring AsName(const std::wstring& file) { return file + (file.size() > 3 && !lstrcmpiW(file.c_str() + file.size() - 3, L".7z") ? L" 로 압축" : L" 으로 압축"); }
std::vector<ChagokMenuItem> ChagokBuildMenu(const std::vector<std::wstring>& paths) {
    std::vector<ChagokMenuItem> m;
    if (paths.empty()) return m;
    bool package = paths.size() == 1 && ChagokIsArchive(paths[0]) && fmt::IsPackagePath(paths[0]);  // apk·iso 등: 압축 위주 + 풀기 일부
    if (paths.size() == 1 && ChagokIsArchive(paths[0]) && !package) {
        m.push_back({L"x-smart", L"알아서 풀기"});
        m.push_back({L"x-here", L"현재 폴더에 압축 풀기"});
        m.push_back({L"x-folder", L"파일 이름 폴더에 풀기"});
        m.push_back({L"x-to", L"풀 위치 선택…"});
        m.push_back({L"x-del", L"알아서 풀고 압축 파일 지우기"});
    } else {
        std::wstring ext = QnExt(ModuleDir()), f = QnFolderStem(paths), n = QnStem(paths[0]);
        m.push_back({L"c-folder", AsName(f + ext)});
        if (paths.size() > 1 && n != f) m.push_back({L"c-name", AsName(n + ext)});
        m.push_back({L"c-date", AsName(f + L"_" + QnToday() + ext)});
        if (paths.size() > 1) m.push_back({L"c-each", L"각각 압축 (" + std::to_wstring(paths.size()) + L"개 → 이름" + ext + L")"});
        m.push_back({L"c-pw", L"암호 걸어 압축…"});
        m.push_back({L"-1", L""});  // 구분선
        m.push_back({L"compress", L"압축하기…"});
        m.push_back({L"split", L"분할 압축…"});
        if (package) {
            m.push_back({L"-3", L""});  // 구분선
            // Windows 11 메뉴는 항목 순서가 고정이라, 압축 항목 아래에 놓을 풀기 항목은 별도 이름(…2)을 쓴다 (실행할 때는 같은 명령)
            m.push_back({L"x-smart2", L"알아서 풀기"});
            m.push_back({L"x-to2", L"풀 위치 선택…"});
        }
    }
    m.push_back({L"-2", L""});  // 구분선
    m.push_back({L"open", L"차곡으로 열기"});
    return m;
}

// 차곡 실행. 경로가 많아 명령줄이 길면 목록 파일(@파일)로 넘긴다
bool ChagokLaunch(const std::wstring& exe, const std::wstring& verbIn, const std::vector<std::wstring>& paths) {
    std::wstring verb = verbIn;
    if (verb == L"x-smart2" || verb == L"x-to2") verb.pop_back();  // 패키지 메뉴의 풀기 항목
    std::wstring cmd = L"\"" + exe + L"\"";
    bool openArchive = verb == L"open" && paths.size() == 1 && ChagokIsArchive(paths[0]);
    if (!openArchive) cmd += L" /" + verb;
    std::wstring args;
    for (auto& p : paths) args += L" \"" + p + L"\"";
    if (cmd.size() + args.size() > 30000) {
        wchar_t tmp[MAX_PATH], file[MAX_PATH];
        GetTempPathW(MAX_PATH, tmp);
        GetTempFileNameW(tmp, L"chg", 0, file);
        HANDLE h = CreateFileW(file, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
        if (h == INVALID_HANDLE_VALUE) return false;
        std::wstring body = L"\xFEFF";
        for (auto& p : paths) body += p + L"\r\n";
        DWORD w;
        WriteFile(h, body.data(), (DWORD)(body.size() * sizeof(wchar_t)), &w, nullptr);
        CloseHandle(h);
        args = L" \"@" + std::wstring(file) + L"\"";
    }
    cmd += args;
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(), &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) return false;
    AllowSetForegroundWindow(pi.dwProcessId);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

// ------------------------------------------------------------ 아이콘 → 메뉴용 32bpp 비트맵
// 차곡 설정에서 고른 아이콘 (0 가지런한 더미, 1 갠 이불, 2 정리함)
static int SelectedIconId() {
    DWORD sel = 1, sz = sizeof(sel);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Sinsul\\Chagok", L"Icon", RRF_RT_REG_DWORD, nullptr, &sel, &sz);
    return sel == 0 ? IDI_APP_A : sel == 2 ? IDI_APP_C : IDI_APP;
}
static std::wstring ModuleDir() {
    wchar_t dll[MAX_PATH * 2];
    GetModuleFileNameW(g_inst, dll, MAX_PATH * 2);
    std::wstring p = dll;
    return p.substr(0, p.find_last_of(L'\\') + 1);
}
static HBITMAP IconBitmap() {
    int cx = GetSystemMetrics(SM_CXSMICON), cy = GetSystemMetrics(SM_CYSMICON);
    int id = SelectedIconId();
    HICON ic = (HICON)LoadImageW(g_inst, MAKEINTRESOURCEW(id), IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR);
    if (!ic) return nullptr;
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = cx; bi.bmiHeader.biHeight = -cy;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bm = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bm) {
        HGDIOBJ o = SelectObject(dc, bm);
        ZeroMemory(bits, (size_t)cx * cy * 4);
        DrawIconEx(dc, 0, 0, ic, cx, cy, 0, nullptr, DI_NORMAL);
        SelectObject(dc, o);
    }
    DeleteDC(dc);
    DestroyIcon(ic);
    return bm;
}

// Sinsul.Chagok 패키지(Windows 11 첫 화면 메뉴)가 이 사용자에게 등록되어 있는지
// 패키지 패밀리 이름 = 이름 + '_' + 게시자 ID(CN=Sinsul Chagok 의 SHA-256 앞 8바이트를 base32 로)
#define CHAGOK_FAMILY L"Sinsul.Chagok_q1acyza95e29j"
static bool ModernMenuRegistered() {
    typedef LONG(WINAPI * Fn)(PCWSTR, UINT32*, PWSTR*, UINT32*, WCHAR*);
    static Fn fn = (Fn)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetPackagesByPackageFamily");
    if (!fn) return false;
    UINT32 count = 0, len = 0;
    LONG r = fn(CHAGOK_FAMILY, &count, nullptr, &len, nullptr);
    return (r == ERROR_SUCCESS || r == ERROR_INSUFFICIENT_BUFFER) && count > 0;
}

// ------------------------------------------------------------ COM 객체
class ShellExt : public IShellExtInit, public IContextMenu {
    LONG ref = 1;
    std::vector<std::wstring> paths;
    std::vector<ChagokMenuItem> items;
    std::vector<int> cmdOfs;  // 메뉴 오프셋 → items 인덱스
    HBITMAP bmp = nullptr;
public:
    ShellExt() { InterlockedIncrement(&g_refs); }
    virtual ~ShellExt() { if (bmp) DeleteObject(bmp); InterlockedDecrement(&g_refs); }
    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IShellExtInit)) *ppv = static_cast<IShellExtInit*>(this);
        else if (IsEqualIID(riid, IID_IContextMenu)) *ppv = static_cast<IContextMenu*>(this);
        else { *ppv = nullptr; return E_NOINTERFACE; }
        AddRef();
        return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return (ULONG)InterlockedIncrement(&ref); }
    STDMETHODIMP_(ULONG) Release() override { LONG r = InterlockedDecrement(&ref); if (!r) delete this; return (ULONG)r; }
    // IShellExtInit
    STDMETHODIMP Initialize(PCIDLIST_ABSOLUTE, IDataObject* dobj, HKEY) override {
        paths.clear();
        if (!dobj) return E_INVALIDARG;
        FORMATETC fe = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        STGMEDIUM sm;
        if (FAILED(dobj->GetData(&fe, &sm))) return E_INVALIDARG;
        HDROP hd = (HDROP)GlobalLock(sm.hGlobal);
        if (hd) {
            UINT n = DragQueryFileW(hd, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < n; i++) {
                UINT len = DragQueryFileW(hd, i, nullptr, 0);
                std::wstring s(len, 0);
                DragQueryFileW(hd, i, &s[0], len + 1);
                paths.push_back(s);
            }
            GlobalUnlock(sm.hGlobal);
        }
        ReleaseStgMedium(&sm);
        return paths.empty() ? E_INVALIDARG : S_OK;
    }
    // IContextMenu
    STDMETHODIMP QueryContextMenu(HMENU hmenu, UINT index, UINT first, UINT last, UINT flags) override {
        if (flags & CMF_DEFAULTONLY) return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 0);
        // Windows 11 새 메뉴(패키지)가 등록되어 있으면 그쪽이 '추가 옵션 표시'에도 나오므로 여기서는 숨는다 (중복 방지)
        if (ModernMenuRegistered()) return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 0);
        items = ChagokBuildMenu(paths);
        if (items.empty()) return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 0);
        HMENU sub = CreatePopupMenu();
        UINT id = first;
        cmdOfs.clear();
        for (size_t i = 0; i < items.size(); i++) {
            if (items[i].verb.empty() || items[i].verb[0] == L'-') { AppendMenuW(sub, MF_SEPARATOR, 0, nullptr); continue; }
            if (id > last) break;
            AppendMenuW(sub, MF_STRING, id++, items[i].label.c_str());
            cmdOfs.push_back((int)i);
        }
        MENUITEMINFOW mi{sizeof(mi)};
        mi.fMask = MIIM_SUBMENU | MIIM_STRING | MIIM_ID;
        mi.wID = id++;
        mi.hSubMenu = sub;
        mi.dwTypeData = (LPWSTR)L"차곡";
        if (!bmp) bmp = IconBitmap();
        if (bmp) { mi.fMask |= MIIM_BITMAP; mi.hbmpItem = bmp; }
        InsertMenuW(hmenu, index, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
        InsertMenuItemW(hmenu, index + 1, TRUE, &mi);
        InsertMenuW(hmenu, index + 2, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
        return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, id - first);
    }
    int FindVerb(LPCSTR verbA, LPCWSTR verbW) {
        for (size_t k = 0; k < cmdOfs.size(); k++) {
            const std::wstring& v = items[(size_t)cmdOfs[k]].verb;
            if (verbW && !lstrcmpiW(verbW, (L"chagok." + v).c_str())) return (int)k;
            if (verbA) {
                std::string a = "chagok.";
                for (wchar_t c : v) a += (char)c;
                if (!lstrcmpiA(verbA, a.c_str())) return (int)k;
            }
        }
        return -1;
    }
    STDMETHODIMP InvokeCommand(CMINVOKECOMMANDINFO* ci) override {
        int k = -1;
        bool unicode = ci->cbSize >= sizeof(CMINVOKECOMMANDINFOEX) && (ci->fMask & CMIC_MASK_UNICODE);
        CMINVOKECOMMANDINFOEX* cx = (CMINVOKECOMMANDINFOEX*)ci;
        if (unicode && !IS_INTRESOURCE(cx->lpVerbW)) k = FindVerb(nullptr, cx->lpVerbW);
        else if (!IS_INTRESOURCE(ci->lpVerb)) k = FindVerb(ci->lpVerb, nullptr);
        else k = LOWORD((UINT_PTR)ci->lpVerb);
        if (k < 0 || k >= (int)cmdOfs.size()) return E_INVALIDARG;
        wchar_t dll[MAX_PATH * 2];
        GetModuleFileNameW(g_inst, dll, MAX_PATH * 2);
        std::wstring exe = dll;
        exe = exe.substr(0, exe.find_last_of(L'\\') + 1) + L"Chagok.exe";
        if (!ChagokLaunch(exe, items[(size_t)cmdOfs[(size_t)k]].verb, paths)) {
            MessageBoxW(ci->hwnd, (L"차곡을 실행할 수 없습니다.\n" + exe).c_str(), L"차곡", MB_ICONERROR);
            return E_FAIL;
        }
        return S_OK;
    }
    STDMETHODIMP GetCommandString(UINT_PTR idCmd, UINT type, UINT*, LPSTR name, UINT cch) override {
        if (idCmd >= cmdOfs.size()) return E_INVALIDARG;
        const ChagokMenuItem& it = items[(size_t)cmdOfs[idCmd]];
        std::wstring verb = L"chagok." + it.verb;
        switch (type) {
        case GCS_VERBW: lstrcpynW((LPWSTR)name, verb.c_str(), (int)cch); return S_OK;
        case GCS_VERBA: { std::string a; for (wchar_t c : verb) a += (char)c; lstrcpynA(name, a.c_str(), (int)cch); return S_OK; }
        case GCS_HELPTEXTW: lstrcpynW((LPWSTR)name, it.label.c_str(), (int)cch); return S_OK;
        case GCS_VALIDATEW: case GCS_VALIDATEA: return S_OK;
        }
        return E_NOTIMPL;
    }
};

// ------------------------------------------------------------ Windows 11 첫 화면 메뉴 (IExplorerCommand)
// 루트 '차곡' 아래에 하위 명령을 모두 두고, 선택한 항목에 맞지 않는 명령은 GetState 에서 숨긴다.
static std::vector<std::wstring> ItemPaths(IShellItemArray* arr) {
    std::vector<std::wstring> v;
    DWORD n = 0;
    if (!arr || FAILED(arr->GetCount(&n))) return v;
    for (DWORD i = 0; i < n; i++) {
        IShellItem* it = nullptr;
        if (FAILED(arr->GetItemAt(i, &it))) continue;
        PWSTR p = nullptr;
        if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p))) { v.push_back(p); CoTaskMemFree(p); }
        it->Release();
    }
    return v;
}
static HRESULT DupStr(const std::wstring& s, LPWSTR* out) { return SHStrDupW(s.c_str(), out); }
static const wchar_t* SUB_VERBS[] = {L"x-smart", L"x-here", L"x-folder", L"x-to", L"x-del",
                                     L"c-folder", L"c-name", L"c-date", L"c-each", L"c-pw", L"-1", L"compress", L"split", L"-3", L"x-smart2", L"x-to2", L"-2", L"open"};

class CmdBase : public IExplorerCommand {
protected:
    LONG ref = 1;
public:
    CmdBase() { InterlockedIncrement(&g_refs); }
    virtual ~CmdBase() { InterlockedDecrement(&g_refs); }
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IExplorerCommand)) { *ppv = static_cast<IExplorerCommand*>(this); AddRef(); return S_OK; }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return (ULONG)InterlockedIncrement(&ref); }
    STDMETHODIMP_(ULONG) Release() override { LONG r = InterlockedDecrement(&ref); if (!r) delete this; return (ULONG)r; }
    STDMETHODIMP GetToolTip(IShellItemArray*, LPWSTR* tip) override { *tip = nullptr; return E_NOTIMPL; }
    STDMETHODIMP GetIcon(IShellItemArray*, LPWSTR* icon) override { *icon = nullptr; return E_NOTIMPL; }
    STDMETHODIMP GetCanonicalName(GUID* g) override { *g = GUID_NULL; return E_NOTIMPL; }
    STDMETHODIMP EnumSubCommands(IEnumExplorerCommand** e) override { *e = nullptr; return E_NOTIMPL; }
};

class SubCmd : public CmdBase {
    std::wstring verb;
    const ChagokMenuItem* Find(const std::vector<ChagokMenuItem>& m) const {
        for (auto& it : m) if (it.verb == verb) return &it;
        return nullptr;
    }
public:
    explicit SubCmd(const wchar_t* v) : verb(v) {}
    static std::wstring DefaultLabel(const std::wstring& v) {
        if (v == L"x-smart") return L"알아서 풀기";
        if (v == L"x-here") return L"현재 폴더에 압축 풀기";
        if (v == L"x-folder") return L"파일 이름 폴더에 풀기";
        if (v == L"x-to") return L"풀 위치 선택…";
        if (v == L"compress") return L"압축하기…";
        if (v == L"split") return L"분할 압축…";
        if (v == L"x-del") return L"알아서 풀고 압축 파일 지우기";
        if (v == L"x-smart2") return L"알아서 풀기";
        if (v == L"x-to2") return L"풀 위치 선택…";
        if (v == L"c-folder") return L"폴더 이름으로 압축";
        if (v == L"c-name") return L"파일 이름으로 압축";
        if (v == L"c-date") return L"날짜 붙여 압축";
        if (v == L"c-each") return L"각각 압축";
        if (v == L"c-pw") return L"암호 걸어 압축…";
        if (!v.empty() && v[0] == L'-') return L"";
        return L"차곡으로 열기";
    }
    STDMETHODIMP GetTitle(IShellItemArray* arr, LPWSTR* name) override {
        auto paths = ItemPaths(arr);
        if (paths.empty()) return DupStr(DefaultLabel(verb), name);
        auto m = ChagokBuildMenu(paths);
        const ChagokMenuItem* it = Find(m);
        return DupStr(it ? it->label : DefaultLabel(verb), name);
    }
    STDMETHODIMP GetState(IShellItemArray* arr, BOOL, EXPCMDSTATE* st) override {
        *st = ECS_ENABLED;
        auto paths = ItemPaths(arr);
        if (!paths.empty() && !Find(ChagokBuildMenu(paths))) *st = ECS_HIDDEN;  // 선택에 맞지 않는 명령만 숨김
        return S_OK;
    }
    STDMETHODIMP GetFlags(EXPCMDFLAGS* f) override { *f = verb[0] == L'-' ? ECF_ISSEPARATOR : ECF_DEFAULT; return S_OK; }
    STDMETHODIMP Invoke(IShellItemArray* arr, IBindCtx*) override {
        auto paths = ItemPaths(arr);
        if (paths.empty()) return E_INVALIDARG;
        std::wstring exe = ModuleDir() + L"Chagok.exe";
        if (!ChagokLaunch(exe, verb, paths)) {
            MessageBoxW(nullptr, (L"차곡을 실행할 수 없습니다.\n" + exe).c_str(), L"차곡", MB_ICONERROR);
            return E_FAIL;
        }
        return S_OK;
    }
};

class SubEnum : public IEnumExplorerCommand {
    LONG ref = 1;
    ULONG pos = 0;
public:
    SubEnum() { InterlockedIncrement(&g_refs); }
    virtual ~SubEnum() { InterlockedDecrement(&g_refs); }
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IEnumExplorerCommand)) { *ppv = this; AddRef(); return S_OK; }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return (ULONG)InterlockedIncrement(&ref); }
    STDMETHODIMP_(ULONG) Release() override { LONG r = InterlockedDecrement(&ref); if (!r) delete this; return (ULONG)r; }
    STDMETHODIMP Next(ULONG n, IExplorerCommand** out, ULONG* fetched) override {
        ULONG k = 0;
        const ULONG total = sizeof(SUB_VERBS) / sizeof(SUB_VERBS[0]);
        while (k < n && pos < total) out[k++] = new SubCmd(SUB_VERBS[pos++]);
        if (fetched) *fetched = k;
        return k == n ? S_OK : S_FALSE;
    }
    STDMETHODIMP Skip(ULONG n) override { pos += n; return S_OK; }
    STDMETHODIMP Reset() override { pos = 0; return S_OK; }
    STDMETHODIMP Clone(IEnumExplorerCommand** e) override { *e = nullptr; return E_NOTIMPL; }
};

class RootCmd : public CmdBase {
public:
    STDMETHODIMP GetTitle(IShellItemArray*, LPWSTR* name) override { return DupStr(L"차곡", name); }
    STDMETHODIMP GetIcon(IShellItemArray*, LPWSTR* icon) override {
        wchar_t dll[MAX_PATH * 2];
        GetModuleFileNameW(g_inst, dll, MAX_PATH * 2);
        return DupStr(std::wstring(dll) + L",-" + std::to_wstring(SelectedIconId()), icon);
    }
    STDMETHODIMP GetState(IShellItemArray*, BOOL, EXPCMDSTATE* st) override {
        // 새 메뉴는 선택 목록 없이 물어볼 때가 있어 항상 보이게 한다 (파일·폴더 메뉴에만 등록되어 있음)
        *st = ECS_ENABLED;
        return S_OK;
    }
    STDMETHODIMP GetFlags(EXPCMDFLAGS* f) override { *f = ECF_HASSUBCOMMANDS; return S_OK; }
    STDMETHODIMP Invoke(IShellItemArray*, IBindCtx*) override { return E_NOTIMPL; }
    STDMETHODIMP EnumSubCommands(IEnumExplorerCommand** e) override { *e = new SubEnum(); return S_OK; }
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
    bool command = false;
    STDMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override {
        if (outer) return CLASS_E_NOAGGREGATION;
        if (command) {
            RootCmd* c = new RootCmd();
            HRESULT hr = c->QueryInterface(riid, ppv);
            c->Release();
            return hr;
        }
        ShellExt* s = new ShellExt();
        HRESULT hr = s->QueryInterface(riid, ppv);
        s->Release();
        return hr;
    }
    STDMETHODIMP LockServer(BOOL lock) override { if (lock) InterlockedIncrement(&g_refs); else InterlockedDecrement(&g_refs); return S_OK; }
};

extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID clsid, REFIID riid, void** ppv) {
    bool cmd = IsEqualCLSID(clsid, CLSID_ChagokCommand);
    if (!cmd && !IsEqualCLSID(clsid, CLSID_ChagokShell)) return CLASS_E_CLASSNOTAVAILABLE;
    Factory* f = new Factory();
    f->command = cmd;
    HRESULT hr = f->QueryInterface(riid, ppv);
    f->Release();
    return hr;
}
extern "C" HRESULT __stdcall DllCanUnloadNow() { return g_refs == 0 ? S_OK : S_FALSE; }
extern "C" BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { g_inst = h; DisableThreadLibraryCalls(h); }
    return TRUE;
}
