// 엔진 테스트용 콘솔 도구 (배포 대상 아님)
//  zipcli c <out.zip> <splitBytes> <src...>
//  zipcli a <arc.zip> <src...>
//  zipcli x <arc> <destDir>
//  zipcli l <arc>
//  zipcli t <arc>                       검사
//  zipcli r <arc> <이름> <새 이름>       이름 바꾸기 (폴더면 안쪽 전체)
//  zipcli d <arc> <이름>                 삭제 (폴더면 안쪽 전체)
#include "../src/archive.h"
#include <cstdio>
#include <cwchar>

static void P(const std::wstring& s) {
    std::string u = WToUtf8(s);
    fwrite(u.data(), 1, u.size(), stdout);
    fputc('\n', stdout);
}
static std::wstring Full(const wchar_t* p) {
    wchar_t b[4096]; GetFullPathNameW(p, 4096, b, nullptr); return b;
}

#include <psapi.h>
// ZSTATS=1 이면 끝날 때 CPU 시간·최대 메모리를 stderr 로 (속도 측정용)
static void PrintStats() {
    FILETIME a, b, k, u;
    PROCESS_MEMORY_COUNTERS pm{sizeof(pm)};
    GetProcessTimes(GetCurrentProcess(), &a, &b, &k, &u);
    GetProcessMemoryInfo(GetCurrentProcess(), &pm, sizeof(pm));
    auto sec = [](FILETIME f) { return (((uint64_t)f.dwHighDateTime << 32) | f.dwLowDateTime) / 1e7; };
    fprintf(stderr, "ZSTATS cpu=%.2f peak=%.1f\n", sec(k) + sec(u), pm.PeakWorkingSetSize / 1048576.0);
}
int wmain(int argc, wchar_t** argv) {
    if (_wgetenv(L"ZSTATS")) atexit(PrintStats);
    if (argc < 3) return 2;
    std::wstring cmd = argv[1], err;
    if (const wchar_t* lv = _wgetenv(L"ZLEVEL")) g_zipLevel = _wtoi(lv);
    if (const wchar_t* t = _wgetenv(L"ZTHREADS")) g_zipThreads = _wtoi(t);
    Job job;
    if (_wgetenv(L"PROGMON")) {  // 진행 값 감시 (0.5초마다 출력)
        static Job* pj = &job;
        CreateThread(nullptr, 0, [](LPVOID) -> DWORD { for (;;) { Sleep(500); fprintf(stderr, "done=%llu total=%llu\n", (unsigned long long)pj->done, (unsigned long long)pj->total); } }, nullptr, 0, nullptr);
    }
    job.askOverwrite = [](const std::wstring&) { return 1; };
    std::function<bool(std::wstring&)> askPw = [](std::wstring& p) {
        const wchar_t* e = _wgetenv(L"PW"); if (!e) return false; p = e; return true; };
    job.askPassword = askPw;
    if (cmd == L"c") {
        std::vector<std::wstring> src;
        for (int i = 4; i < argc; i++) src.push_back(Full(argv[i]));
        std::vector<AddItem> items;
        std::wstring tgt = Full(argv[2]);
        CollectItems(src, L"", {tgt}, items);
        const wchar_t* cpw = _wgetenv(L"CPW");  // 만들 때 걸 암호
        if (!AnyCreate(tgt, items, _wcstoui64(argv[3], nullptr, 10), job, err, cpw ? cpw : L"")) { P(L"ERR " + err); return 1; }
        P(L"OK"); return 0;
    }
    AnyArchive a;
    if (!a.Open(Full(argv[2]), err, askPw)) { P(L"ERR " + err); return 1; }
    P(L"format=" + a.format + (a.readOnly ? L" (read-only)" : L""));
    if (cmd == L"l") {
        for (auto& e : a.entries) {
            wchar_t b[64]; swprintf(b, 64, L" %llu %llu %08x", (unsigned long long)e.usize, (unsigned long long)e.csize, e.crc);
            P((e.isDir ? L"D " : L"F ") + e.name + b);
        }
        P(L"volumes=" + std::to_wstring(a.volumes.size()));
        return 0;
    }
    if (cmd == L"a") {
        std::vector<std::wstring> src;
        for (int i = 3; i < argc; i++) src.push_back(Full(argv[i]));
        std::vector<AddItem> items;
        CollectItems(src, L"", {}, items);
        if (!AnyAdd(a, items, job, err)) { P(L"ERR " + err); return 1; }
        P(L"OK"); return 0;
    }
    if (cmd == L"t") {
        std::vector<int> idx;
        for (size_t i = 0; i < a.entries.size(); i++) idx.push_back((int)i);
        int failed = 0; std::wstring fe;
        job.testOnly = true;
        if (!AnyExtract(a, idx, L"", L"", job, err, failed, fe)) { P(L"ERR " + err); return 1; }
        if (failed) { P(L"FAILED " + std::to_wstring(failed) + L" " + fe); return 1; }
        P(L"OK"); return 0;
    }
    if (cmd == L"r" || cmd == L"d") {
        std::wstring nm = argv[3];
        std::map<int, std::wstring> ren; std::vector<int> del;
        for (size_t i = 0; i < a.entries.size(); i++) {
            const std::wstring& n = a.entries[i].name;
            bool hit = n == nm || n.compare(0, nm.size() + 1, nm + L"/") == 0;
            if (!hit) continue;
            if (cmd == L"d") del.push_back((int)i);
            else ren[(int)i] = std::wstring(argv[4]) + n.substr(nm.size());
        }
        if (ren.empty() && del.empty()) { P(L"ERR not found"); return 1; }
        if (!AnyModify(a, ren, del, job, err)) { P(L"ERR " + err); return 1; }
        P(L"OK"); return 0;
    }
    if (cmd == L"x") {
        std::vector<int> idx;
        for (size_t i = 0; i < a.entries.size(); i++) idx.push_back((int)i);
        int failed = 0; std::wstring fe;
        if (!AnyExtract(a, idx, L"", Full(argv[3]), job, err, failed, fe)) { P(L"ERR " + err); return 1; }
        if (failed) { P(L"FAILED " + std::to_wstring(failed) + L" " + fe); return 1; }
        P(L"OK"); return 0;
    }
    return 2;
}
