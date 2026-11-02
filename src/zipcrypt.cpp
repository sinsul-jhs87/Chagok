// zipcrypt.cpp - WinZip AES(AE-2) / ZipCrypto 구현
// AES: CPU 의 AES-NI 명령으로 직접 처리(없으면 소프트웨어). SHA-1/HMAC/PBKDF2 도 직접 구현.
// (Windows CNG 를 거치면 호출 비용 때문에 느려서 7-Zip 처럼 직접 구현했다. 난수만 시스템 RNG 사용)
#include "zipcrypt.h"
#include <bcrypt.h>
#include <cstring>
#include <vector>
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#include <wmmintrin.h>
#include <emmintrin.h>
#include <immintrin.h>

#ifndef NT_SUCCESS
#define NT_SUCCESS(s) (((NTSTATUS)(s)) >= 0)
#endif
#if defined(__GNUC__) || defined(__clang__)
#define AESNI_TARGET __attribute__((target("aes,sse2")))
#define SHANI_TARGET __attribute__((target("sha,sse4.1,ssse3")))
#else
#define AESNI_TARGET
#define SHANI_TARGET
#endif

namespace zc {

std::string PwUtf8(const std::wstring& pw) {
    int n = WideCharToMultiByte(CP_UTF8, 0, pw.data(), (int)pw.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    if (n) WideCharToMultiByte(CP_UTF8, 0, pw.data(), (int)pw.size(), &s[0], n, nullptr, nullptr);
    return s;
}
std::string PwAnsi(const std::wstring& pw) {
    int n = WideCharToMultiByte(CP_ACP, 0, pw.data(), (int)pw.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    if (n) WideCharToMultiByte(CP_ACP, 0, pw.data(), (int)pw.size(), &s[0], n, nullptr, nullptr);
    return s;
}
bool RandomBytes(uint8_t* p, size_t n) {
    return NT_SUCCESS(BCryptGenRandom(nullptr, p, (ULONG)n, BCRYPT_USE_SYSTEM_PREFERRED_RNG));
}

// ============================================================ SHA-1 / HMAC / PBKDF2
static bool HasShaNi() {
    static int v = -1;
    if (v < 0) {
        unsigned a = 0, b = 0, c = 0, d = 0, c1 = 0;
#if defined(_MSC_VER)
        int r[4]; __cpuid(r, 1); c1 = (unsigned)r[2];
        __cpuidex(r, 7, 0); b = (unsigned)r[1];
#else
        __get_cpuid(1, &a, &b, &c1, &d);
        b = 0;
        if (__get_cpuid_max(0, nullptr) >= 7) __cpuid_count(7, 0, a, b, c, d);
#endif
        v = ((b >> 29) & 1) && ((c1 >> 9) & 1) && ((c1 >> 19) & 1);
    }
    return v == 1;
}

// Intel SHA 확장 명령 (최근 Intel/AMD 대부분 지원)
SHANI_TARGET static void Sha1BlocksNi(uint32_t state[5], const uint8_t* data, size_t nblocks) {
    __m128i ABCD, ABCD_SAVE, E0, E0_SAVE, E1, MSG0, MSG1, MSG2, MSG3;
    const __m128i MASK = _mm_set_epi64x(0x0001020304050607ULL, 0x08090a0b0c0d0e0fULL);
    ABCD = _mm_loadu_si128((const __m128i*)state);
    E0 = _mm_set_epi32((int)state[4], 0, 0, 0);
    ABCD = _mm_shuffle_epi32(ABCD, 0x1B);
    while (nblocks--) {
        ABCD_SAVE = ABCD; E0_SAVE = E0;
        MSG0 = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i*)(data + 0)), MASK);
        E0 = _mm_add_epi32(E0, MSG0); E1 = ABCD; ABCD = _mm_sha1rnds4_epu32(ABCD, E0, 0);
        MSG1 = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i*)(data + 16)), MASK);
        E1 = _mm_sha1nexte_epu32(E1, MSG1); E0 = ABCD; ABCD = _mm_sha1rnds4_epu32(ABCD, E1, 0); MSG0 = _mm_sha1msg1_epu32(MSG0, MSG1);
        MSG2 = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i*)(data + 32)), MASK);
        E0 = _mm_sha1nexte_epu32(E0, MSG2); E1 = ABCD; ABCD = _mm_sha1rnds4_epu32(ABCD, E0, 0); MSG1 = _mm_sha1msg1_epu32(MSG1, MSG2); MSG0 = _mm_xor_si128(MSG0, MSG2);
        MSG3 = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i*)(data + 48)), MASK);
        E1 = _mm_sha1nexte_epu32(E1, MSG3); E0 = ABCD; MSG0 = _mm_sha1msg2_epu32(MSG0, MSG3); ABCD = _mm_sha1rnds4_epu32(ABCD, E1, 0); MSG2 = _mm_sha1msg1_epu32(MSG2, MSG3); MSG1 = _mm_xor_si128(MSG1, MSG3);
#define S4(EA, EB, MA, MB, MC, MD, F) \
        EA = _mm_sha1nexte_epu32(EA, MA); EB = ABCD; MB = _mm_sha1msg2_epu32(MB, MA); ABCD = _mm_sha1rnds4_epu32(ABCD, EA, F); MD = _mm_sha1msg1_epu32(MD, MA); MC = _mm_xor_si128(MC, MA);
        S4(E0, E1, MSG0, MSG1, MSG2, MSG3, 0)   // 16-19
        S4(E1, E0, MSG1, MSG2, MSG3, MSG0, 1)   // 20-23
        S4(E0, E1, MSG2, MSG3, MSG0, MSG1, 1)   // 24-27
        S4(E1, E0, MSG3, MSG0, MSG1, MSG2, 1)   // 28-31
        S4(E0, E1, MSG0, MSG1, MSG2, MSG3, 1)   // 32-35
        S4(E1, E0, MSG1, MSG2, MSG3, MSG0, 1)   // 36-39
        S4(E0, E1, MSG2, MSG3, MSG0, MSG1, 2)   // 40-43
        S4(E1, E0, MSG3, MSG0, MSG1, MSG2, 2)   // 44-47
        S4(E0, E1, MSG0, MSG1, MSG2, MSG3, 2)   // 48-51
        S4(E1, E0, MSG1, MSG2, MSG3, MSG0, 2)   // 52-55
        S4(E0, E1, MSG2, MSG3, MSG0, MSG1, 2)   // 56-59
        S4(E1, E0, MSG3, MSG0, MSG1, MSG2, 3)   // 60-63
#undef S4
        E0 = _mm_sha1nexte_epu32(E0, MSG0); E1 = ABCD; MSG1 = _mm_sha1msg2_epu32(MSG1, MSG0); ABCD = _mm_sha1rnds4_epu32(ABCD, E0, 3); MSG3 = _mm_sha1msg1_epu32(MSG3, MSG0); MSG2 = _mm_xor_si128(MSG2, MSG0);  // 64-67
        E1 = _mm_sha1nexte_epu32(E1, MSG1); E0 = ABCD; MSG2 = _mm_sha1msg2_epu32(MSG2, MSG1); ABCD = _mm_sha1rnds4_epu32(ABCD, E1, 3); MSG3 = _mm_xor_si128(MSG3, MSG1);  // 68-71
        E0 = _mm_sha1nexte_epu32(E0, MSG2); E1 = ABCD; MSG3 = _mm_sha1msg2_epu32(MSG3, MSG2); ABCD = _mm_sha1rnds4_epu32(ABCD, E0, 3);  // 72-75
        E1 = _mm_sha1nexte_epu32(E1, MSG3); E0 = ABCD; ABCD = _mm_sha1rnds4_epu32(ABCD, E1, 3);  // 76-79
        E0 = _mm_sha1nexte_epu32(E0, E0_SAVE);
        ABCD = _mm_add_epi32(ABCD, ABCD_SAVE);
        data += 64;
    }
    ABCD = _mm_shuffle_epi32(ABCD, 0x1B);
    _mm_storeu_si128((__m128i*)state, ABCD);
    state[4] = (uint32_t)_mm_extract_epi32(E0, 3);
}

static inline uint32_t rol32(uint32_t x, int c) { return (x << c) | (x >> (32 - c)); }
static void Sha1BlocksC(uint32_t h[5], const uint8_t* p, size_t nblocks) {
    for (; nblocks--; p += 64) {
        uint32_t w[16];
        for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; i++) {
            uint32_t wi;
            if (i < 16) wi = w[i];
            else { wi = rol32(w[(i + 13) & 15] ^ w[(i + 8) & 15] ^ w[(i + 2) & 15] ^ w[i & 15], 1); w[i & 15] = wi; }
            uint32_t f, k;
            if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6; }
            uint32_t t = rol32(a, 5) + f + e + k + wi;
            e = d; d = c; c = rol32(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
}
static void Sha1Blocks(uint32_t h[5], const uint8_t* p, size_t n) {
    if (HasShaNi()) Sha1BlocksNi(h, p, n); else Sha1BlocksC(h, p, n);
}

struct Sha1 {
    uint32_t h[5];
    uint8_t buf[64];
    uint64_t len = 0;
    size_t n = 0;
    void Init() { h[0] = 0x67452301; h[1] = 0xEFCDAB89; h[2] = 0x98BADCFE; h[3] = 0x10325476; h[4] = 0xC3D2E1F0; len = 0; n = 0; }
    void Update(const uint8_t* p, size_t k) {
        len += k;
        if (n) {
            size_t t = 64 - n < k ? 64 - n : k;
            memcpy(buf + n, p, t); n += t; p += t; k -= t;
            if (n == 64) { Sha1Blocks(h, buf, 1); n = 0; }
        }
        if (k >= 64) { size_t nb = k / 64; Sha1Blocks(h, p, nb); p += nb * 64; k -= nb * 64; }
        if (k) { memcpy(buf, p, k); n = k; }
    }
    void Final(uint8_t out[20]) {
        uint64_t bits = len * 8;
        uint8_t pad[72] = {0x80};
        size_t padLen = (n < 56) ? 56 - n : 120 - n;
        uint8_t l[8];
        for (int i = 0; i < 8; i++) l[i] = (uint8_t)(bits >> (56 - 8 * i));
        Update(pad, padLen);
        Update(l, 8);
        for (int i = 0; i < 5; i++) { out[4 * i] = (uint8_t)(h[i] >> 24); out[4 * i + 1] = (uint8_t)(h[i] >> 16); out[4 * i + 2] = (uint8_t)(h[i] >> 8); out[4 * i + 3] = (uint8_t)h[i]; }
    }
};
struct Hmac {
    Sha1 in, out;
    void Init(const uint8_t* key, size_t kl) {
        uint8_t k[64] = {0}, ipad[64], opad[64];
        if (kl > 64) { Sha1 t; t.Init(); t.Update(key, kl); t.Final(k); }
        else memcpy(k, key, kl);
        for (int i = 0; i < 64; i++) { ipad[i] = k[i] ^ 0x36; opad[i] = k[i] ^ 0x5c; }
        in.Init(); in.Update(ipad, 64);
        out.Init(); out.Update(opad, 64);
        SecureZeroMemory(k, sizeof(k));
    }
    void Update(const uint8_t* p, size_t n) { in.Update(p, n); }
    void Final(uint8_t mac[20]) {
        uint8_t ih[20];
        in.Final(ih);
        Sha1 o = out;
        o.Update(ih, 20);
        o.Final(mac);
    }
};
static void Pbkdf2Sha1(const std::string& pw, const uint8_t* salt, size_t sl, int iter, uint8_t* out, size_t outLen) {
    Hmac base;
    base.Init((const uint8_t*)pw.data(), pw.size());
    for (uint32_t blk = 1; outLen; blk++) {
        uint8_t u[20], t[20];
        Hmac h = base;
        h.Update(salt, sl);
        uint8_t be[4] = {(uint8_t)(blk >> 24), (uint8_t)(blk >> 16), (uint8_t)(blk >> 8), (uint8_t)blk};
        h.Update(be, 4);
        h.Final(u);
        memcpy(t, u, 20);
        for (int i = 1; i < iter; i++) {
            Hmac h2 = base;
            h2.Update(u, 20);
            h2.Final(u);
            for (int j = 0; j < 20; j++) t[j] ^= u[j];
        }
        size_t k = outLen < 20 ? outLen : 20;
        memcpy(out, t, k);
        out += k; outLen -= k;
    }
}

// ============================================================ AES (암호화 방향만: CTR 은 암·복호화 같음)
static const uint8_t SBOX[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16};

static bool HasAesNi() {
    static int v = -1;
    if (v < 0) {
        unsigned a = 0, b = 0, c = 0, d = 0;
#if defined(_MSC_VER)
        int r[4]; __cpuid(r, 1); c = (unsigned)r[2];
#else
        __get_cpuid(1, &a, &b, &c, &d);
#endif
        v = (c >> 25) & 1;
    }
    return v == 1;
}

struct AesKey {
    int rounds = 0;
    alignas(16) uint8_t rk[15 * 16];
    void Expand(const uint8_t* key, int bits) {
        int nk = bits / 32;
        rounds = nk + 6;
        int total = 4 * (rounds + 1);
        uint32_t w[60];
        for (int i = 0; i < nk; i++) w[i] = (uint32_t)key[4 * i] << 24 | (uint32_t)key[4 * i + 1] << 16 | (uint32_t)key[4 * i + 2] << 8 | key[4 * i + 3];
        uint32_t rcon = 0x01;
        for (int i = nk; i < total; i++) {
            uint32_t t = w[i - 1];
            if (i % nk == 0) {
                t = (t << 8) | (t >> 24);
                t = (uint32_t)SBOX[t >> 24] << 24 | (uint32_t)SBOX[(t >> 16) & 255] << 16 | (uint32_t)SBOX[(t >> 8) & 255] << 8 | SBOX[t & 255];
                t ^= rcon << 24;
                rcon = (rcon << 1) ^ ((rcon & 0x80) ? 0x11b : 0);
            } else if (nk > 6 && i % nk == 4) {
                t = (uint32_t)SBOX[t >> 24] << 24 | (uint32_t)SBOX[(t >> 16) & 255] << 16 | (uint32_t)SBOX[(t >> 8) & 255] << 8 | SBOX[t & 255];
            }
            w[i] = w[i - nk] ^ t;
        }
        for (int i = 0; i < total; i++) { rk[4 * i] = (uint8_t)(w[i] >> 24); rk[4 * i + 1] = (uint8_t)(w[i] >> 16); rk[4 * i + 2] = (uint8_t)(w[i] >> 8); rk[4 * i + 3] = (uint8_t)w[i]; }
        SecureZeroMemory(w, sizeof(w));
    }
    // 소프트웨어 (AES-NI 없는 CPU 용): T-테이블 방식
    static const uint32_t* Te() {
        static uint32_t T[4][256];
        static volatile LONG ready = 0;
        if (!ready) {
            for (int i = 0; i < 256; i++) {
                uint8_t s = SBOX[i], s2 = (uint8_t)((s << 1) ^ ((s & 0x80) ? 0x1b : 0)), s3 = (uint8_t)(s2 ^ s);
                uint32_t t = (uint32_t)s2 << 24 | (uint32_t)s << 16 | (uint32_t)s << 8 | s3;
                for (int k = 0; k < 4; k++) { T[k][i] = t; t = (t >> 8) | (t << 24); }
            }
            ready = 1;
        }
        return &T[0][0];
    }
    void EncryptSoft(uint8_t* io) const {
        const uint32_t* T = Te();
        const uint32_t *T0 = T, *T1 = T + 256, *T2 = T + 512, *T3 = T + 768;
        auto ld = [](const uint8_t* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; };
        uint32_t s0 = ld(io) ^ ld(rk), s1 = ld(io + 4) ^ ld(rk + 4), s2 = ld(io + 8) ^ ld(rk + 8), s3 = ld(io + 12) ^ ld(rk + 12);
        for (int r = 1; r < rounds; r++) {
            const uint8_t* k = rk + 16 * r;
            uint32_t t0 = T0[s0 >> 24] ^ T1[(s1 >> 16) & 255] ^ T2[(s2 >> 8) & 255] ^ T3[s3 & 255] ^ ld(k);
            uint32_t t1 = T0[s1 >> 24] ^ T1[(s2 >> 16) & 255] ^ T2[(s3 >> 8) & 255] ^ T3[s0 & 255] ^ ld(k + 4);
            uint32_t t2 = T0[s2 >> 24] ^ T1[(s3 >> 16) & 255] ^ T2[(s0 >> 8) & 255] ^ T3[s1 & 255] ^ ld(k + 8);
            uint32_t t3 = T0[s3 >> 24] ^ T1[(s0 >> 16) & 255] ^ T2[(s1 >> 8) & 255] ^ T3[s2 & 255] ^ ld(k + 12);
            s0 = t0; s1 = t1; s2 = t2; s3 = t3;
        }
        const uint8_t* k = rk + 16 * rounds;
        uint32_t o[4];
        o[0] = ((uint32_t)SBOX[s0 >> 24] << 24 | (uint32_t)SBOX[(s1 >> 16) & 255] << 16 | (uint32_t)SBOX[(s2 >> 8) & 255] << 8 | SBOX[s3 & 255]) ^ ld(k);
        o[1] = ((uint32_t)SBOX[s1 >> 24] << 24 | (uint32_t)SBOX[(s2 >> 16) & 255] << 16 | (uint32_t)SBOX[(s3 >> 8) & 255] << 8 | SBOX[s0 & 255]) ^ ld(k + 4);
        o[2] = ((uint32_t)SBOX[s2 >> 24] << 24 | (uint32_t)SBOX[(s3 >> 16) & 255] << 16 | (uint32_t)SBOX[(s0 >> 8) & 255] << 8 | SBOX[s1 & 255]) ^ ld(k + 8);
        o[3] = ((uint32_t)SBOX[s3 >> 24] << 24 | (uint32_t)SBOX[(s0 >> 16) & 255] << 16 | (uint32_t)SBOX[(s1 >> 8) & 255] << 8 | SBOX[s2 & 255]) ^ ld(k + 12);
        for (int i = 0; i < 4; i++) { io[4 * i] = (uint8_t)(o[i] >> 24); io[4 * i + 1] = (uint8_t)(o[i] >> 16); io[4 * i + 2] = (uint8_t)(o[i] >> 8); io[4 * i + 3] = (uint8_t)o[i]; }
    }
};

AESNI_TARGET __attribute__((noinline)) static void CtrBlocksNi(const AesKey& k, uint64_t& counter, uint8_t* out, size_t nblocks) {
    __m128i rk[15];
    for (int i = 0; i <= k.rounds; i++) rk[i] = _mm_load_si128((const __m128i*)(k.rk + 16 * i));
    size_t b = 0;
    for (; b + 4 <= nblocks; b += 4) {
        __m128i x0 = _mm_set_epi64x(0, (long long)(counter + 1)), x1 = _mm_set_epi64x(0, (long long)(counter + 2));
        __m128i x2 = _mm_set_epi64x(0, (long long)(counter + 3)), x3 = _mm_set_epi64x(0, (long long)(counter + 4));
        counter += 4;
        x0 = _mm_xor_si128(x0, rk[0]); x1 = _mm_xor_si128(x1, rk[0]); x2 = _mm_xor_si128(x2, rk[0]); x3 = _mm_xor_si128(x3, rk[0]);
        for (int r = 1; r < k.rounds; r++) {
            x0 = _mm_aesenc_si128(x0, rk[r]); x1 = _mm_aesenc_si128(x1, rk[r]);
            x2 = _mm_aesenc_si128(x2, rk[r]); x3 = _mm_aesenc_si128(x3, rk[r]);
        }
        x0 = _mm_aesenclast_si128(x0, rk[k.rounds]); x1 = _mm_aesenclast_si128(x1, rk[k.rounds]);
        x2 = _mm_aesenclast_si128(x2, rk[k.rounds]); x3 = _mm_aesenclast_si128(x3, rk[k.rounds]);
        _mm_storeu_si128((__m128i*)(out + 16 * b), x0); _mm_storeu_si128((__m128i*)(out + 16 * b + 16), x1);
        _mm_storeu_si128((__m128i*)(out + 16 * b + 32), x2); _mm_storeu_si128((__m128i*)(out + 16 * b + 48), x3);
    }
    for (uint8_t* p = out + 16 * b, *end = out + 16 * nblocks; p < end; p += 16) {  // 4개 단위 나머지
        __m128i x = _mm_xor_si128(_mm_set_epi64x(0, (long long)(++counter)), rk[0]);
        for (int r = 1; r < k.rounds; r++) x = _mm_aesenc_si128(x, rk[r]);
        x = _mm_aesenclast_si128(x, rk[k.rounds]);
        _mm_storeu_si128((__m128i*)p, x);
    }
}

// ============================================================ Aes (WinZip)
struct AesState {
    AesKey key;
    Hmac mac;
};
Aes::~Aes() {
    if (key) { SecureZeroMemory(key, sizeof(AesState)); delete (AesState*)key; }
}
bool Aes::Init(const std::string& pw, const uint8_t* salt, int saltLen, int keyBits, uint8_t verifier[2]) {
    int kl = keyBits / 8;
    uint8_t dk[66];
    Pbkdf2Sha1(pw, salt, (size_t)saltLen, 1000, dk, (size_t)kl * 2 + 2);  // WinZip AES 규격: 1000회
    if (!key) key = new AesState();
    AesState* s = (AesState*)key;
    s->key.Expand(dk, keyBits);
    s->mac.Init(dk + kl, (size_t)kl);
    verifier[0] = dk[kl * 2];
    verifier[1] = dk[kl * 2 + 1];
    SecureZeroMemory(dk, sizeof(dk));
    counter = 0;
    ksPos = sizeof(ks);
    return true;
}
void Aes::Refill() {
    AesState* s = (AesState*)key;
    if (HasAesNi()) CtrBlocksNi(s->key, counter, ks, sizeof(ks) / 16);
    else {
        for (size_t i = 0; i < sizeof(ks); i += 16) {
            uint64_t c = ++counter;
            memcpy(ks + i, &c, 8);
            memset(ks + i + 8, 0, 8);
            s->key.EncryptSoft(ks + i);
        }
    }
    ksPos = 0;
}
void Aes::Crypt(uint8_t* d, size_t n) {
    while (n) {
        if (ksPos == sizeof(ks)) Refill();
        size_t k = sizeof(ks) - ksPos;
        if (k > n) k = n;
        const uint8_t* s = ks + ksPos;
        size_t i = 0;
        for (; i + 16 <= k; i += 16) {
            __m128i a = _mm_loadu_si128((const __m128i*)(d + i)), b = _mm_loadu_si128((const __m128i*)(s + i));
            _mm_storeu_si128((__m128i*)(d + i), _mm_xor_si128(a, b));
        }
        for (; i < k; i++) d[i] ^= s[i];
        d += k; n -= k; ksPos += k;
    }
}
void Aes::MacUpdate(const uint8_t* d, size_t n) { ((AesState*)key)->mac.Update(d, n); }
void Aes::MacFinal(uint8_t out10[10]) {
    uint8_t full[20];
    ((AesState*)key)->mac.Final(full);
    memcpy(out10, full, 10);
}

// ============================================================ ZipCrypto
static uint32_t Crc32Byte(uint32_t crc, uint8_t b) {
    static uint32_t table[256];
    static volatile LONG init = 0;
    if (!init) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        init = 1;
    }
    return table[(crc ^ b) & 0xFF] ^ (crc >> 8);
}
void ZipCrypto::Update(uint8_t c) {
    k0 = Crc32Byte(k0, c);
    k1 = (k1 + (k0 & 0xFF)) * 134775813u + 1;
    k2 = Crc32Byte(k2, (uint8_t)(k1 >> 24));
}
uint8_t ZipCrypto::Byte() const {
    uint16_t t = (uint16_t)((k2 & 0xFFFF) | 2);
    return (uint8_t)((t * (t ^ 1)) >> 8);
}
void ZipCrypto::Init(const std::string& pw) {
    k0 = 0x12345678; k1 = 0x23456789; k2 = 0x34567890;
    for (unsigned char c : pw) Update(c);
}
void ZipCrypto::Decrypt(uint8_t* p, size_t n) {
    for (size_t i = 0; i < n; i++) { p[i] ^= Byte(); Update(p[i]); }
}
bool ZipCrypto::CheckHeader(uint8_t hdr[12], uint8_t check) {
    Decrypt(hdr, 12);
    return hdr[11] == check;
}

}  // namespace zc
