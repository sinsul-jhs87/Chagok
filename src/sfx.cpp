// sfx.cpp - 차곡 자동 실행 압축 파일(SFX)의 풀기 프로그램 (ChagokSFX.bin)
// 차곡이 [이 프로그램] + [ZIP 데이터] 를 이어 붙여 .exe 를 만든다.
// 실행하면 자기 자신 뒤쪽의 ZIP 을 찾아 고른 폴더에 푼다 (zipcore 는 앞에 데이터가 붙은 ZIP 을 읽을 수 있다).
#include "zipcore.h"
#include <commctrl.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <shlobj.h>
#include "resource.h"

static HINSTANCE g_hi;
static HWND g_win, g_edit, g_bar, g_status, g_ok, g_cancel, g_browse;
static HFONT g_font, g_bold;
static int g_dpi = 96;
static ZipArchive g_zip;
static Job g_job;
static HANDLE g_thread;
static bool g_running = false, g_done = false;
static std::wstring g_dest, g_err, g_firstErr;
static int g_failed = 0;
static bool g_okResult = false;
#define WM_APP_DONE (WM_APP + 1)
#define WM_APP_PW (WM_APP + 2)
#define WM_APP_ASK (WM_APP + 3)

static int S(int v) { return MulDiv(v, g_dpi, 96); }
static const wchar_t* TITLE = L"차곡 자동 압축 풀기";

// ---------------------------------------------------------------- 암호 입력 창
static bool g_pwDone, g_pwOk;
static HWND g_pwEdit;
static LRESULT CALLBACK PwProc(HWND h, UINT m, WPARAM w, LPARAM lp) {
    if (m == WM_COMMAND && (LOWORD(w) == IDOK || LOWORD(w) == IDCANCEL)) { g_pwOk = LOWORD(w) == IDOK; g_pwDone = true; return 0; }
    if (m == WM_CLOSE) { g_pwOk = false; g_pwDone = true; return 0; }
    return DefWindowProcW(h, m, w, lp);
}
static bool AskPassword(std::wstring& pw) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = PwProc; wc.hInstance = g_hi; wc.lpszClassName = L"ChagokSfxPw";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1); wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&wc);
    RECT r; GetWindowRect(g_win, &r);
    int W = S(360), H = S(170);
    HWND d = CreateWindowExW(WS_EX_DLGMODALFRAME, L"ChagokSfxPw", L"암호 입력", WS_POPUP | WS_CAPTION | WS_SYSMENU,
                             (r.left + r.right - W) / 2, (r.top + r.bottom - H) / 2, W, H, g_win, nullptr, g_hi, nullptr);
    auto mk = [&](const wchar_t* cls, const wchar_t* t, DWORD st, int x, int y, int w, int hh, int id) {
        HWND c = CreateWindowExW(!wcscmp(cls, L"EDIT") ? WS_EX_CLIENTEDGE : 0, cls, t, WS_CHILD | WS_VISIBLE | st, S(x), S(y), S(w), S(hh), d, (HMENU)(INT_PTR)id, g_hi, nullptr);
        SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
        return c;
    };
    mk(L"STATIC", L"암호로 보호된 압축 파일입니다. 암호를 입력하세요.", 0, 16, 14, 320, 20, 0);
    g_pwEdit = mk(L"EDIT", L"", ES_PASSWORD | ES_AUTOHSCROLL | WS_TABSTOP, 16, 42, 312, 26, 100);
    mk(L"BUTTON", L"확인", BS_DEFPUSHBUTTON | WS_TABSTOP, 156, 84, 84, 30, IDOK);
    mk(L"BUTTON", L"취소", WS_TABSTOP, 246, 84, 84, 30, IDCANCEL);
    EnableWindow(g_win, FALSE);
    ShowWindow(d, SW_SHOW);
    SetFocus(g_pwEdit);
    g_pwDone = false; g_pwOk = false;
    MSG msg;
    while (!g_pwDone && GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) { g_pwOk = true; g_pwDone = true; continue; }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) { g_pwDone = true; continue; }
        if (!IsDialogMessageW(d, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    wchar_t b[256] = {};
    GetWindowTextW(g_pwEdit, b, 256);
    pw = b;
    SecureZeroMemory(b, sizeof(b));
    EnableWindow(g_win, TRUE);
    DestroyWindow(d);
    SetForegroundWindow(g_win);
    return g_pwOk && !pw.empty();
}

// ---------------------------------------------------------------- 풀기
static DWORD WINAPI Worker(LPVOID) {
    std::vector<int> idx;
    for (size_t i = 0; i < g_zip.entries.size(); i++) idx.push_back((int)i);
    g_okResult = ExtractEntries(g_zip, idx, L"", g_dest, g_job, g_err, g_failed, g_firstErr);
    PostMessageW(g_win, WM_APP_DONE, 0, 0);
    return 0;
}

static std::wstring PickFolder(const std::wstring& init) {
    std::wstring out;
    IFileOpenDialog* d = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d)))) return out;
    DWORD o = 0;
    d->GetOptions(&o);
    d->SetOptions(o | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    d->SetTitle(L"압축을 풀 위치 선택");
    IShellItem* si = nullptr;
    std::wstring start = DirExists(init) ? init : ParentOf(init);
    if (SUCCEEDED(SHCreateItemFromParsingName(start.c_str(), nullptr, IID_PPV_ARGS(&si)))) { d->SetFolder(si); si->Release(); }
    if (SUCCEEDED(d->Show(g_win))) {
        IShellItem* it = nullptr;
        if (SUCCEEDED(d->GetResult(&it))) {
            PWSTR p = nullptr;
            if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p))) { out = p; CoTaskMemFree(p); }
            it->Release();
        }
    }
    d->Release();
    return out;
}

static void Start() {
    wchar_t b[2048];
    GetWindowTextW(g_edit, b, 2048);
    g_dest = b;
    while (!g_dest.empty() && (g_dest.back() == L'\\' || g_dest.back() == L' ')) g_dest.pop_back();
    if (g_dest.empty()) return;
    g_job.cancel = false;
    g_job.onFile = nullptr;
    g_job.askPassword = [](std::wstring& pw) { return SendMessageW(g_win, WM_APP_PW, 0, (LPARAM)&pw) != 0; };
    g_job.askOverwrite = [](const std::wstring& p) { return (int)SendMessageW(g_win, WM_APP_ASK, 0, (LPARAM)&p); };
    g_running = true;
    EnableWindow(g_ok, FALSE); EnableWindow(g_browse, FALSE); EnableWindow(g_edit, FALSE);
    SetWindowTextW(g_cancel, L"취소");
    SetTimer(g_win, 1, 100, nullptr);
    g_thread = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM lp) {
    switch (m) {
    case WM_COMMAND:
        switch (LOWORD(w)) {
        case 1: if (!g_running && !g_done) Start(); else if (g_done) DestroyWindow(h); return 0;
        case 2:
            if (g_running) { g_job.cancel = true; SetWindowTextW(g_status, L"취소하는 중…"); }
            else DestroyWindow(h);
            return 0;
        case 3: {
            wchar_t b[2048]; GetWindowTextW(g_edit, b, 2048);
            std::wstring p = PickFolder(b);
            if (!p.empty()) SetWindowTextW(g_edit, p.c_str());
            return 0;
        }
        }
        break;
    case WM_CTLCOLORSTATIC:  // 글자 배경을 창 배경(흰색)과 같게
        SetBkColor((HDC)w, GetSysColor(COLOR_WINDOW));
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    case WM_TIMER: {
        uint64_t d = g_job.done, t = g_job.total;
        SendMessageW(g_bar, PBM_SETPOS, t ? (WPARAM)(d * 1000 / t) : 0, 0);
        wchar_t b[64];
        swprintf(b, 64, L"%d%% 진행 중…", t ? (int)(d * 100 / t) : 0);
        SetWindowTextW(g_status, b);
        return 0;
    }
    case WM_APP_PW: return AskPassword(*(std::wstring*)lp) ? 1 : 0;
    case WM_APP_ASK: {
        const std::wstring& p = *(const std::wstring*)lp;
        int r = MessageBoxW(h, (L"같은 이름의 파일이 있습니다.\n" + p + L"\n\n[예] 모두 덮어쓰기   [아니요] 모두 건너뛰기   [취소] 중지").c_str(),
                            TITLE, MB_YESNOCANCEL | MB_ICONQUESTION);
        return r == IDYES ? 1 : r == IDNO ? 3 : 4;
    }
    case WM_APP_DONE: {
        KillTimer(h, 1);
        WaitForSingleObject(g_thread, INFINITE);
        CloseHandle(g_thread);
        g_running = false;
        if (!g_okResult) {
            if (!g_job.cancel) MessageBoxW(h, (L"압축을 풀지 못했습니다.\n" + g_err).c_str(), TITLE, MB_ICONERROR);
            EnableWindow(g_ok, TRUE); EnableWindow(g_browse, TRUE); EnableWindow(g_edit, TRUE);
            SetWindowTextW(g_cancel, L"닫기");
            SetWindowTextW(g_status, L"");
            SendMessageW(g_bar, PBM_SETPOS, 0, 0);
            return 0;
        }
        g_done = true;
        SendMessageW(g_bar, PBM_SETPOS, 1000, 0);
        std::wstring msg = g_failed ? std::to_wstring(g_failed) + L"개 항목을 풀지 못했습니다.\n" + g_firstErr + L"\n\n폴더를 열까요?"
                                    : L"압축을 모두 풀었습니다.\n" + g_dest + L"\n\n폴더를 열까요?";
        SetWindowTextW(g_status, g_failed ? L"일부 항목을 풀지 못했습니다." : L"완료");
        if (MessageBoxW(h, msg.c_str(), TITLE, MB_YESNO | (g_failed ? MB_ICONWARNING : MB_ICONINFORMATION)) == IDYES)
            ShellExecuteW(h, L"open", g_dest.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        DestroyWindow(h);
        return 0;
    }
    case WM_CLOSE:
        if (g_running) { g_job.cancel = true; return 0; }
        DestroyWindow(h);
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, lp);
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int show) {
    g_hi = hi;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX ic{sizeof(ic), ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&ic);
    wchar_t self[MAX_PATH * 4];
    GetModuleFileNameW(nullptr, self, MAX_PATH * 4);
    std::wstring err;
    if (!g_zip.Open(self, err) || g_zip.entries.empty()) {
        MessageBoxW(nullptr, L"이 파일에는 풀 압축 데이터가 없습니다.\n(차곡에서 '자동 실행 파일로 만들기'로 만든 파일만 풀 수 있습니다)", TITLE, MB_ICONERROR);
        return 1;
    }
    g_dpi = GetDpiForSystem();
    auto mkFont = [](int px, int wgt) {
        return CreateFontW(-S(px), 0, 0, 0, wgt, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Malgun Gothic");
    };
    g_font = mkFont(13, FW_NORMAL);
    g_bold = mkFont(15, FW_BOLD);
    std::wstring name = FileNameOf(self);
    std::wstring stem = name.size() > 4 && !lstrcmpiW(name.c_str() + name.size() - 4, L".exe") ? name.substr(0, name.size() - 4) : name;
    uint64_t total = 0;
    size_t files = 0;
    for (auto& e : g_zip.entries) if (!e.isDir) { total += e.usize; files++; }
    // 압축 파일 안이 폴더 하나뿐이면 그대로, 아니면 '이름' 폴더를 만들어 그 안에
    bool singleRoot = true;
    std::wstring root;
    for (auto& e : g_zip.entries) {
        std::wstring top = e.name.substr(0, e.name.find(L'/'));
        if (root.empty()) root = top;
        else if (top != root) singleRoot = false;
        if (e.name.find(L'/') == std::wstring::npos && !e.isDir) singleRoot = false;
    }
    std::wstring dest = ParentOf(self) + (singleRoot ? L"" : L"\\" + stem);

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc; wc.hInstance = hi; wc.lpszClassName = L"ChagokSfx";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(hi, MAKEINTRESOURCEW(IDI_APP));
    RegisterClassW(&wc);
    int W = S(520), H = S(260);
    RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    g_win = CreateWindowExW(0, L"ChagokSfx", (stem + L" - " + TITLE).c_str(), WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                            (wa.left + wa.right - W) / 2, (wa.top + wa.bottom - H) / 2, W, H, nullptr, nullptr, hi, nullptr);
    auto mk = [&](const wchar_t* cls, const std::wstring& t, DWORD st, int x, int y, int w, int h, int id, HFONT f) {
        HWND c = CreateWindowExW(!wcscmp(cls, L"EDIT") ? WS_EX_CLIENTEDGE : 0, cls, t.c_str(), WS_CHILD | WS_VISIBLE | st, S(x), S(y), S(w), S(h), g_win,
                                 (HMENU)(INT_PTR)id, hi, nullptr);
        SendMessageW(c, WM_SETFONT, (WPARAM)f, TRUE);
        return c;
    };
    wchar_t info[128];
    double mb = total / 1048576.0;
    swprintf(info, 128, L"파일 %u개 · %.1f MB", (unsigned)files, mb);
    mk(L"STATIC", stem, SS_ENDELLIPSIS, 20, 16, 470, 24, 0, g_bold);
    mk(L"STATIC", info, 0, 20, 42, 470, 20, 0, g_font);
    mk(L"STATIC", L"압축을 풀 위치", 0, 20, 74, 470, 20, 0, g_font);
    g_edit = mk(L"EDIT", dest, ES_AUTOHSCROLL | WS_TABSTOP, 20, 96, 380, 28, 10, g_font);
    g_browse = mk(L"BUTTON", L"찾아보기…", WS_TABSTOP, 406, 95, 90, 30, 3, g_font);
    g_bar = mk(PROGRESS_CLASSW, L"", 0, 20, 140, 476, 14, 0, g_font);
    SendMessageW(g_bar, PBM_SETRANGE32, 0, 1000);
    g_status = mk(L"STATIC", L"", 0, 20, 160, 300, 20, 0, g_font);
    g_ok = mk(L"BUTTON", L"압축 풀기", BS_DEFPUSHBUTTON | WS_TABSTOP, 310, 182, 92, 32, 1, g_font);
    g_cancel = mk(L"BUTTON", L"닫기", WS_TABSTOP, 406, 182, 90, 32, 2, g_font);
    ShowWindow(g_win, show);
    SetFocus(g_ok);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageW(g_win, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    CoUninitialize();
    return 0;
}
