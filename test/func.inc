// 내 PC 둘러보기 동작 검사 (-DCHAGOK_SHOTS 빌드, 환경 변수 FUNC_LOG / SHOT_BROWSE)
static FILE* g_flog;
static int g_fail = 0;
static void Check(bool ok, const wchar_t* what) {
    if (!ok) g_fail++;
    fwprintf(g_flog, L"%ls %ls\n", ok ? L"PASS" : L"FAIL", what);
    fflush(g_flog);
}
static int RowOf(const wchar_t* name) {
    for (size_t i = 0; i < g_rows.size(); i++) if (!lstrcmpiW(g_rows[i].name.c_str(), name)) return (int)i;
    return -1;
}
// 자동 업데이트 출처 검사: UPD_GH(가짜 GitHub 주소, 꺼진 주소) UPD_LOCAL(사내 폴더)
static void UpdTest() {
    std::wstring gh = EnvW(L"UPD_GH"), down = EnvW(L"UPD_GH_DOWN"), local = EnvW(L"UPD_LOCAL");
    { UpdInfo u; u.base = local; bool ok = UpdFetch(&u, gh.c_str());
      Check(ok && u.from == L"GitHub" && u.version == L"9.9.9" && u.base == gh + L"/releases/download/v9.9.9/", L"GitHub 먼저 (사내 위치가 있어도)");
      std::string data; std::wstring err;
      Check(FetchAll(UpdJoin(u.base, u.file), data, err) && Sha256Hex(data) == u.sha256, L"GitHub 설치 파일 받기 + SHA-256"); }
    { UpdInfo u; u.base = local; bool ok = UpdFetch(&u, down.c_str());
      Check(ok && u.from == L"사내 위치" && u.version == L"8.8.8" && u.base == local, L"GitHub 안 되면 사내 위치"); }
    { UpdInfo u; bool ok = UpdFetch(&u, down.c_str());
      Check(!ok && u.err.find(L"GitHub:") == 0 && u.err.find(L"사내 위치는 정해져") != std::wstring::npos, L"사내 위치 없음 + GitHub 안 됨 → 오류"); }
    { UpdInfo u; u.base = L"Z:\\nope\\x"; bool ok = UpdFetch(&u, down.c_str());
      Check(!ok && u.err.find(L"사내 위치:") != std::wstring::npos, L"둘 다 안 됨 → 두 오류 모두"); }
}
// 새 폴더: NF_ARC(쓰기 가능한 압축 파일 복사본)에 '새 폴더' 를 만들고 목록·선택 확인
static void NfTest(const std::wstring& arc) {
    Check(OpenArchive(arc), L"압축 파일 열기");
    size_t before = g_arc.entries.size();
    NewFolderCreate(L"새 폴더");
    for (int i = 0; i < 300 && Busy(); i++) Pump(100);
    Pump(500);
    bool found = false;
    for (auto& e : g_arc.entries) if (e.isDir && (e.name == L"새 폴더" || e.name == L"새 폴더/")) found = true;
    Check(found && g_arc.entries.size() == before + 1, L"새 폴더 항목이 생김");
    int one = SingleSel();
    Check(one >= 0 && g_rows[(size_t)one].kind == 1 && g_dirs[(size_t)g_rows[(size_t)one].idx].name == L"새 폴더", L"만든 폴더가 선택됨");
}
static void FuncRun() {
    std::wstring dir = EnvW(L"SHOT_BROWSE");
    g_flog = _wfopen(EnvW(L"FUNC_LOG").c_str(), L"w, ccs=UTF-8");
    if (!EnvW(L"UPD_GH").empty()) { UpdTest(); fclose(g_flog); DestroyWindow(g_hwnd); return; }
    if (!EnvW(L"NF_ARC").empty()) { NfTest(EnvW(L"NF_ARC")); fclose(g_flog); DestroyWindow(g_hwnd); return; }
    Pump(500);
    EnterBrowse(dir);
    Check(g_fs && !g_hasArc && g_fsPath == FsNorm(dir), L"폴더 열기");
    Check(g_rows.size() == 8, L"항목 8개 (숨김 제외)");
    Check(g_rows.size() >= 2 && g_rows[0].kind == 1 && g_rows[1].kind == 1, L"폴더가 먼저");
    Activate(RowOf(L"도면"));
    Check(FileNameOf(g_fsPath) == L"도면", L"폴더 들어가기");
    GoUp();
    Check(g_fsPath == FsNorm(dir) && g_focus >= 0 && g_rows[(size_t)g_focus].name == L"도면", L"뒤로: 원래 폴더 + 나온 폴더 선택");
    // 검색
    SetWindowTextW(g_searchEdit, L"zip");
    Check(g_rows.size() == 2, L"이 폴더에서 찾기");
    SetWindowTextW(g_searchEdit, L"");
    Check(g_rows.size() == 8, L"찾기 지우기");
    // 압축 파일 열기 → 맨 위에서 뒤로 → 폴더
    Activate(RowOf(L"제안서_자료.zip"));
    Check(g_hasArc && !g_fs, L"압축 파일 두 번 눌러 열기");
    ShowDir(1);
    GoUp();
    Check(g_hasArc && g_cur == 0, L"압축 파일 안에서 뒤로 = 상위 폴더");
    GoUp();
    Check(g_fs && !g_hasArc && g_focus >= 0 && g_rows[(size_t)g_focus].name == L"제안서_자료.zip", L"압축 파일 맨 위에서 뒤로 = 그 파일이 있는 폴더");
    // 경로 단추
    Relayout();
    OnCommand(ID_FSCRUMB + 0);
    Check(g_fs && g_fsPath.empty(), L"경로 단추 '내 PC'");
    Check(!g_rows.empty() && g_fsItems[(size_t)g_rows[0].idx].name == L"바탕 화면", L"내 PC: 자주 쓰는 폴더가 먼저");
    bool drive = false;
    for (auto& r : g_rows) if (g_fsItems[(size_t)r.idx].drive) drive = true;
    Check(drive, L"내 PC: 드라이브 표시");
    GoUp();
    Check(!g_fs && !g_hasArc, L"내 PC 에서 뒤로 = 처음 화면");
    // 압축 → 끝나면 그 폴더에서 새 파일 선택
    EnterBrowse(dir);
    SelectRow(RowOf(L"readme.txt"), false, false);
    SelectRow(RowOf(L"견적서.xlsx"), true, false);
    std::vector<std::wstring> src = FsSelPaths();
    Check(src.size() == 2, L"고른 항목 경로");
    JobState* j = new JobState();
    j->kind = JK_COMPRESS; j->sources = src; j->target = FsJoin(g_fsPath, L"묶음.zip");
    StartJob(j);
    for (int i = 0; i < 100 && Busy(); i++) Pump(100);
    Pump(1500);
    Check(!Busy() && g_fs && FileExists(FsJoin(g_fsPath, L"묶음.zip")), L"고른 항목 압축");
    int nr = RowOf(L"묶음.zip");
    Check(nr >= 0 && g_sel[(size_t)nr], L"압축 후 새 파일이 목록에 선택됨");
    // 밖에서 파일이 생기면 목록이 저절로 바뀜
    std::wstring nf = FsJoin(g_fsPath, L"새파일.txt");
    HANDLE h = CreateFileW(nf.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    CloseHandle(h);
    Pump(2500);
    Check(RowOf(L"새파일.txt") >= 0, L"폴더 변화 자동 반영");
    DeleteFileW(nf.c_str());
    Pump(2500);
    Check(RowOf(L"새파일.txt") < 0, L"지운 파일 자동 반영");
    // 풀기 (차곡을 하나 더 띄워 새 폴더에 풀기)
    FsExtract({FsJoin(g_fsPath, L"묶음.zip")}, XT_FOLDER);
    std::wstring out = FsJoin(g_fsPath, L"묶음");
    for (int i = 0; i < 150 && !FileExists(out + L"\\readme.txt"); i++) Pump(100);
    Pump(1500);
    Check(FileExists(out + L"\\readme.txt") && FileExists(out + L"\\견적서.xlsx"), L"새 폴더에 풀기");
    Check(RowOf(L"묶음") >= 0, L"푼 폴더가 목록에 보임");
    // 정렬: 종류
    g_sortCol = 2; g_sortDesc = false; FsShow();
    Check(g_rows.size() > 3, L"종류 정렬");
    g_sortCol = 0; FsShow();
    fwprintf(g_flog, L"FAIL %d\n", g_fail);
    fclose(g_flog);
    DestroyWindow(g_hwnd);
}
// 덮어쓰기 질문이 진행 창 위에 뜨는지 (일반 창 모드): OW_ARC 압축 파일을 그 자리에 풀어 질문을 띄우고 화면을 찍는다
static void OwRun() {
    g_shotDir = EnvW(L"SHOT_DIR");
    Pump(500);
    OpenArchive(EnvW(L"OW_ARC"));
    ExtractAllTo(XT_HERE);
    Arm(L"ow_normal");
    for (int i = 0; i < 200 && Busy(); i++) Pump(100);
    DestroyWindow(g_hwnd);
}
