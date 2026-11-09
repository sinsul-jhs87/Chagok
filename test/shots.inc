// 화면 검사용 자동 캡처 (배포 빌드에는 들어가지 않음: -DCHAGOK_SHOTS 로만 빌드)
// 테마·색상은 레지스트리(Theme / ColorMode)로 정해 두고 실행하면, 모든 화면을 차례로 띄워 PNG 로 저장하고 끝난다.
// 환경 변수: SHOT_DIR(저장 폴더) SHOT_ARC(압축 파일) SHOT_BROWSE(둘러볼 폴더) SHOT_TXT SHOT_IMG(미리보기용 파일)
static std::wstring g_shotDir, g_shotName;
static std::wstring EnvW(const wchar_t* n) { wchar_t b[1024]; DWORD k = GetEnvironmentVariableW(n, b, 1024); return k && k < 1024 ? b : L""; }
static BOOL CALLBACK ShotEnum(HWND w, LPARAM lp) {
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    RECT r;
    if (pid == GetCurrentProcessId() && IsWindowVisible(w) && GetWindowRect(w, &r) && r.right - r.left > 60 && r.bottom - r.top > 30) {
        RECT* u = (RECT*)lp;
        if (IsRectEmpty(u)) *u = r; else UnionRect(u, u, &r);
    }
    return TRUE;
}
static void ShotSave(const std::wstring& name) {
    RECT u{};
    EnumWindows(ShotEnum, (LPARAM)&u);
    int w = u.right - u.left, h = u.bottom - u.top;
    if (w <= 0 || h <= 0) return;
    HDC sdc = GetDC(nullptr), mdc = CreateCompatibleDC(sdc);
    HBITMAP bm = CreateCompatibleBitmap(sdc, w, h);
    HGDIOBJ old = SelectObject(mdc, bm);
    BitBlt(mdc, 0, 0, w, h, sdc, u.left, u.top, SRCCOPY);
    SelectObject(mdc, old);
    UINT n = 0, sz = 0;
    GetImageEncodersSize(&n, &sz);
    std::vector<BYTE> buf(sz);
    ImageCodecInfo* ci = (ImageCodecInfo*)buf.data();
    GetImageEncoders(n, sz, ci);
    for (UINT i = 0; i < n; i++)
        if (!wcscmp(ci[i].MimeType, L"image/png")) { Bitmap b(bm, nullptr); b.Save((g_shotDir + L"\\" + name + L".png").c_str(), &ci[i].Clsid, nullptr); }
    DeleteObject(bm); DeleteDC(mdc); ReleaseDC(nullptr, sdc);
}
static void Pump(int ms) {
    DWORD t0 = GetTickCount();
    MSG m;
    while (GetTickCount() - t0 < (DWORD)ms) {
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
        Sleep(15);
    }
}
static void Snap(const wchar_t* name) { Relayout(); Pump(700); ShotSave(name); }
// 대화상자·메뉴처럼 모달로 도는 화면: 잠시 뒤 찍고 닫는다
static void CALLBACK ShotTimer(HWND, UINT, UINT_PTR id, DWORD) {
    KillTimer(nullptr, id);
    ShotSave(g_shotName);
    if (HWND p = FindWindowW(L"ChagokPopup", nullptr)) if (IsWindowVisible(p)) { PostMessageW(p, WM_KEYDOWN, VK_ESCAPE, 0); return; }
    for (const wchar_t* c : {L"ChagokPreview", L"ChagokDialog", L"ChagokSettings"})
        if (HWND d = FindWindowW(c, nullptr)) if (IsWindowVisible(d)) { PostMessageW(d, WM_CLOSE, 0, 0); return; }
}
static void Arm(const wchar_t* name) { g_shotName = name; SetTimer(nullptr, 0, 1300, ShotTimer); }
static POINT ListPoint(int row) {
    RECT r = ItemRect(row);
    POINT p{(r.left + r.right) / 2, (r.top + r.bottom) / 2};
    ClientToScreen(g_hwnd, &p);
    return p;
}
static int FirstFileRow() { for (size_t i = 0; i < g_rows.size(); i++) if (g_rows[i].kind == 2) return (int)i; return -1; }

static void ShotsRun() {
    g_shotDir = EnvW(L"SHOT_DIR");
    std::wstring arc = EnvW(L"SHOT_ARC"), dir = EnvW(L"SHOT_BROWSE"), txt = EnvW(L"SHOT_TXT"), img = EnvW(L"SHOT_IMG");
    Pump(800);
    Snap(L"01_home");
    OpenArchive(arc);
    g_view = 1;
    Snap(L"02_archive");
    SetWindowTextW(g_searchEdit, L"서");
    Snap(L"03_search");
    SetWindowTextW(g_searchEdit, L"");
    SetFocus(g_hwnd);
    g_view = 0; Snap(L"04_tiles"); g_view = 1;
    int fr = FirstFileRow();
    SelectRow(fr, false, false);
    Snap(L"05_selected");
    Arm(L"06_ctxmenu"); CmdContextMenu(ListPoint(fr));
    Arm(L"07_rename"); CmdRename();
    Arm(L"07b_newfolder"); CmdNewFolder();
    AlbumOpen(); Pump(700); ShotSave(L"08_album"); AlbumClose();
    for (int i = 0; i < 4; i++) { wchar_t n[32]; wsprintfW(n, L"09_settings%d", i); Arm(n); RunSettings(false, nullptr, i); }
    for (int i = 0; i < 3; i++) {
        wchar_t n[32]; wsprintfW(n, L"10_compress%d", i);
        CompressOpts o; o.target = L"C:\\자료\\제안서_자료.zip"; o.preset = 0; o.sfx = i == 2;
        Arm(n); RunSettings(true, &o, i);
    }
    {   // 진행 창 (가짜 작업)
        JobState* j = new JobState();
        j->kind = JK_COMPRESS; j->target = L"C:\\자료\\제안서_자료.zip";
        j->job.total = 100 << 20; j->job.done = 42 << 20; j->filesTotal = 37; j->filesDone = 15; j->cur = L"현장사진/IMG_0412.jpg";
        g_job = j;
        ProgOpen();
        Snap(L"11_progress");
        ProgClose();
        g_job = nullptr;
        delete j;
        Relayout();
    }
    if (!txt.empty()) { bool o = false; Arm(L"12_preview_text"); RunPreview(FileNameOf(txt), txt, o); }
    if (!img.empty()) { bool o = false; Arm(L"13_preview_image"); RunPreview(FileNameOf(img), img, o); }
    {
        JobState* j = new JobState();
        j->hashes = {{L"견적서.xlsx", L"3f1c0e8b2a9d47c6e5b1f0a2d3c4b5a69788f7e6d5c4b3a2918070605040302f"}};
        Arm(L"14_hash"); ShowHashResult(j);
        delete j;
    }
    Arm(L"15_message"); MsgBox(L"압축 해제 완료", L"다음 위치에 압축을 풀었습니다.\nC:\\자료\\제안서_자료");
    { std::wstring p = L"C:\\자료\\견적서.xlsx"; Arm(L"16_overwrite"); SendMessageW(g_hwnd, WM_APP_ASK, 0, (LPARAM)&p); }
    // 내 PC 둘러보기
    GoUp();  // 압축 파일 맨 위에서 뒤로 → 그 압축 파일이 있는 폴더
    Snap(L"17_back_to_folder");
    EnterBrowse(L"");
    Snap(L"18_mypc");
    EnterBrowse(dir);
    int ar = -1;
    for (size_t i = 0; i < g_rows.size(); i++) if (g_fsItems[(size_t)g_rows[i].idx].arc) ar = (int)i;
    SelectRow(ar >= 0 ? ar : 0, false, false);
    Snap(L"19_browse");
    Arm(L"20_browse_ctx"); CmdContextMenu(ListPoint(g_focus));
    g_view = 0; Snap(L"21_browse_tiles"); g_view = 1;
    SetWindowTextW(g_searchEdit, L"zip");
    Snap(L"22_browse_search");
    SetWindowTextW(g_searchEdit, L"");
    ExitBrowse();
    Snap(L"23_home_again");
    DestroyWindow(g_hwnd);
}
