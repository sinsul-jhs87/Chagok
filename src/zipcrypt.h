// zipcrypt.h - ZIP 암호화 (WinZip AES-256, AE-2) + 기존 ZipCrypto 해제
// Windows 기본 암호화 모듈(CNG, bcrypt.dll) 사용: 추가 라이브러리 없음, AES-NI 하드웨어 가속
#pragma once
#include <windows.h>
#include <cstdint>
#include <string>

namespace zc {

// 비밀번호 → 바이트. AES 는 UTF-8 이 표준이지만, 예전 프로그램이 ANSI(CP949)로 만든 파일도 있어 둘 다 시도한다
std::string PwUtf8(const std::wstring& pw);
std::string PwAnsi(const std::wstring& pw);

// AES-256 CTR(WinZip 방식, 리틀엔디언 카운터) + HMAC-SHA1
class Aes {
public:
    ~Aes();
    // salt(16바이트)로 키를 만든다. verifier(2바이트)를 돌려준다
    bool Init(const std::string& pw, const uint8_t* salt, int saltLen, int keyBits, uint8_t verifier[2]);
    void Crypt(uint8_t* data, size_t n);           // 암호화·복호화 동일 (CTR)
    void MacUpdate(const uint8_t* data, size_t n);  // 암호문에 대해
    void MacFinal(uint8_t out10[10]);
private:
    void* key = nullptr;      // BCRYPT_KEY_HANDLE
    void* hash = nullptr;     // BCRYPT_HASH_HANDLE
    uint8_t ks[4096];         // 키스트림 버퍼
    size_t ksPos = sizeof(ks);
    uint64_t counter = 0;
    void Refill();
};

bool RandomBytes(uint8_t* p, size_t n);

// 기존 PKWARE 암호(ZipCrypto) — 읽기 전용(약한 방식이라 새로 만들 때는 쓰지 않음)
class ZipCrypto {
public:
    void Init(const std::string& pw);
    // 12바이트 머리글을 풀고 확인 바이트가 맞는지
    bool CheckHeader(uint8_t hdr[12], uint8_t check);
    void Decrypt(uint8_t* p, size_t n);
private:
    uint32_t k0 = 0, k1 = 0, k2 = 0;
    void Update(uint8_t c);
    uint8_t Byte() const;
};

}  // namespace zc
