// zipcore.h - ZIP 읽기/쓰기 엔진 (일반 / 분할(.z01) / ZIP64)
#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <atomic>
#include <functional>
#include <map>

std::wstring Utf8ToW(const char* s, int n);
std::string  WToUtf8(const std::wstring& w);
std::wstring LongPath(const std::wstring& p);
bool         FileExists(const std::wstring& p);
bool         DirExists(const std::wstring& p);
std::wstring FileNameOf(const std::wstring& p);
std::wstring ParentOf(const std::wstring& p);

struct ZipEntry {
    std::wstring name;      // '/' 구분, 끝 '/' 없음
    bool     isDir = false;
    uint16_t flags = 0, method = 0, dosTime = 0, dosDate = 0;
    uint32_t crc = 0, extAttr = 0;
    uint64_t usize = 0, csize = 0;
    uint64_t localOffset = 0;   // 볼륨 연결 기준 전역 오프셋
    uint8_t  aesStrength = 0;   // WinZip AES: 1=128 2=192 3=256 (0 이면 AES 아님)
    uint8_t  aesVersion = 0;    // 1=AE-1(CRC 있음) 2=AE-2
    uint16_t aesMethod = 0;     // AES 안쪽의 실제 압축 방식
    bool Encrypted() const { return (flags & 1) != 0; }
};

// 여러 볼륨 파일을 하나의 연속 스트림처럼 읽는다
class VolumeSet {
public:
    ~VolumeSet() { Close(); }
    bool Open(const std::vector<std::wstring>& paths);
    void Close();
    bool Read(uint64_t off, void* buf, size_t n);
    std::vector<HANDLE>   hs;
    std::vector<uint64_t> starts, sizes;
    uint64_t total = 0;
};

class ZipArchive {
public:
    std::wstring path;                  // 대표 경로(.zip)
    std::vector<std::wstring> volumes;  // 실제 파일들(.z01.. + .zip)
    bool     isSplit = false;
    uint64_t splitSize = 0;             // 첫 볼륨 크기
    std::vector<ZipEntry> entries;
    VolumeSet vs;
    std::wstring password;              // 확인된 암호 (다시 묻지 않도록)

    bool Open(const std::wstring& p, std::wstring& err);
    void Close();
    bool DataOffset(const ZipEntry& e, uint64_t& off);
};

// 작업 진행/취소/질문 인터페이스 (작업 스레드에서 호출됨)
struct Job {
    std::atomic<bool>     cancel{false};
    std::atomic<uint64_t> done{0}, total{0};
    uint64_t pBase = 0, pSpan = 0;   // 여러 단계 작업의 진행 환산 (0 이면 사용 안 함)
    bool testOnly = false;           // 압축 파일 검사: 풀어서 CRC·암호 검증만 하고 파일은 쓰지 않음
    std::function<void(const std::wstring&)> onFile;       // 현재 처리 중인 이름
    std::function<bool(std::wstring&)>       askPassword;   // 암호 입력 (false = 취소)
    std::function<int(const std::wstring&)>  askOverwrite;  // 0 덮어쓰기 1 모두 2 건너뛰기 3 모두건너뛰기 4 취소
};

struct AddItem {
    std::wstring src;       // 디스크 경로 (복사 항목이면 비어 있음)
    std::wstring arcName;   // 압축 파일 내 경로 ('/' 구분)
    bool     isDir = false;
    int      copyIdx = -1;  // 기존 압축 파일의 항목 번호
    uint64_t size = 0;
    FILETIME mtime{};
};

// 압축 수준 1(빠름) ~ 9(최대). 기본 3
extern int g_zipLevel;
extern int g_zipThreads;  // 압축·해제에 쓸 코어 수 (0 = 자동)
// ZIP 파일 이름 글자 코드 강제 (0 = 자동, 949 한국어, 65001 UTF-8, 932 일본어, 936 중국어 간체, 437 영어 DOS)
extern int g_zipNameCP;

// 경로 목록(파일/폴더)을 재귀적으로 AddItem 으로 변환
void CollectItems(const std::vector<std::wstring>& paths, const std::wstring& prefix,
                  const std::vector<std::wstring>& exclude, std::vector<AddItem>& out);

// 새 압축 파일 생성 (splitSize=0 이면 일반)
// password 가 비어 있지 않으면 AES-256 으로 암호화 (WinZip AE-2)
bool CreateZip(const std::wstring& target, std::vector<AddItem>& items, uint64_t splitSize,
               Job& job, std::wstring& err, const std::wstring& password = L"");

// 기존 압축 파일에 항목 추가 (같은 이름은 교체). 분할 압축이면 같은 크기로 다시 분할
bool AddToZip(ZipArchive& arc, std::vector<AddItem>& items, Job& job, std::wstring& err, const std::wstring& password = L"");
// 암호가 맞는지 확인 (암호 걸린 항목 하나로 검사). 맞는 암호 바이트 형식도 돌려준다
bool ZipCheckPassword(ZipArchive& arc, const std::wstring& pw);
// 이름 바꾸기·삭제: 다시 압축하지 않고 데이터를 그대로 복사해 새로 쓴다
// rename: 항목 번호 → 새 전체 이름('/' 구분), del: 지울 항목 번호
bool ZipModify(ZipArchive& arc, const std::map<int, std::wstring>& rename, const std::vector<int>& del, Job& job, std::wstring& err);
bool ZipHasEncrypted(const ZipArchive& arc);

// 항목 추출. stripPrefix("a/b/")를 잘라낸 상대 경로로 dest 아래에 만든다
bool ExtractEntries(ZipArchive& arc, const std::vector<int>& idx, const std::wstring& stripPrefix,
                    const std::wstring& dest, Job& job, std::wstring& err, int& failed,
                    std::wstring& firstError);

// 공용 도우미 (szarc.cpp 에서도 사용)
std::wstring SanitizeRel(const std::wstring& rel);
bool MakeDirs(const std::wstring& p);
bool FinalizeTempVolumes(const std::vector<std::wstring>& tmp, const std::wstring& target, bool numeric, std::wstring& err);
bool GzipCompress(const std::wstring& src, const std::wstring& target, const std::wstring& innerName,
                  uint64_t splitSize, Job& job, std::wstring& err);
bool LooksIncompressible(const uint8_t* p, size_t n);
bool IsPrecompressedName(const std::wstring& name);
bool FileLooksIncompressible(const std::wstring& path, uint64_t size);
