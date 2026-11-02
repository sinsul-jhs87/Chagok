// archive.h - 형식 통합 계층
//  ZIP 계열 → 자체 엔진(zipcore, 멀티코어 deflate)  : 기존 속도 그대로
//  그 밖(7z, rar, tar, gz, iso, img …) → 7z.dll (필요할 때만 불러옴)
#pragma once
#include "zipcore.h"
#include "formats.h"

struct SzArchive;  // szarc.cpp

struct AnyArchive {
    std::wstring path;                  // 화면에 보이는 대표 경로
    std::vector<std::wstring> volumes;
    bool isSplit = false;
    std::vector<ZipEntry> entries;      // 항목 목록 (두 엔진 공통 형식)
    std::wstring format = L"ZIP";       // 표시용 형식 이름
    bool isZip = true;
    bool readOnly = false;              // 파일 추가 불가 형식
    std::wstring readOnlyWhy;
    ZipArchive zip;
    SzArchive* sz = nullptr;

    ~AnyArchive() { Close(); }
    // askPassword: 머리글까지 암호화된 7z 등에서 호출 (false 반환 시 취소)
    bool Open(const std::wstring& p, std::wstring& err, const std::function<bool(std::wstring&)>& askPassword);
    void Close();
};

bool AnyExtract(AnyArchive& a, const std::vector<int>& idx, const std::wstring& strip, const std::wstring& dest,
                Job& job, std::wstring& err, int& failed, std::wstring& firstError);
bool AnyAdd(AnyArchive& a, std::vector<AddItem>& items, Job& job, std::wstring& err);
// 이름 바꾸기·삭제 (항목 번호 기준). 데이터는 다시 압축하지 않고 복사
bool AnyModify(AnyArchive& a, const std::map<int, std::wstring>& rename, const std::vector<int>& del, Job& job, std::wstring& err);

// 만들기: 형식은 대상 확장자로 결정 (.zip / .7z / .tar / .tar.gz·.tgz)
// password 가 있으면 암호화: ZIP = AES-256(WinZip AE-2), 7z = AES-256 + 파일 이름 목록 암호화
bool AnyCreate(const std::wstring& target, std::vector<AddItem>& items, uint64_t splitSize, Job& job, std::wstring& err,
               const std::wstring& password = L"");
bool AnyCanEncrypt(const std::wstring& target);   // .zip / .7z
bool AnyHasPassword(const AnyArchive& a);

// 7z.dll 사용 가능 여부 (Chagok.exe 옆에 있어야 함)
bool SzAvailable();
