#include "fastcrc.h"
#include <immintrin.h>
#include <cpuid.h>

static uint32_t T[8][256];
static int g_mode = -1;  // 0 표, 1 PCLMUL

static void Init() {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
        T[0][i] = c;
    }
    for (uint32_t i = 0; i < 256; i++)
        for (int s = 1; s < 8; s++) T[s][i] = (T[s - 1][i] >> 8) ^ T[0][T[s - 1][i] & 0xFF];
    unsigned a, b, c, d;
    g_mode = (__get_cpuid(1, &a, &b, &c, &d) && (c & bit_PCLMUL) && (c & bit_SSE4_1)) ? 1 : 0;
}

// 원시 CRC(반전 안 한 값) 위에서 동작
static uint32_t Slice8(uint32_t c, const uint8_t* p, size_t n) {
    while (n && ((uintptr_t)p & 7)) { c = (c >> 8) ^ T[0][(c ^ *p++) & 0xFF]; n--; }
    while (n >= 8) {
        uint32_t lo = c ^ (uint32_t)(p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24);
        uint32_t hi = (uint32_t)(p[4] | p[5] << 8 | p[6] << 16 | (uint32_t)p[7] << 24);
        c = T[7][lo & 0xFF] ^ T[6][(lo >> 8) & 0xFF] ^ T[5][(lo >> 16) & 0xFF] ^ T[4][lo >> 24] ^
            T[3][hi & 0xFF] ^ T[2][(hi >> 8) & 0xFF] ^ T[1][(hi >> 16) & 0xFF] ^ T[0][hi >> 24];
        p += 8; n -= 8;
    }
    while (n--) c = (c >> 8) ^ T[0][(c ^ *p++) & 0xFF];
    return c;
}

// Intel "Fast CRC Computation Using PCLMULQDQ" 접기 (Chromium zlib crc32_simd 와 같은 상수)
// n >= 64, n % 16 == 0
__attribute__((target("pclmul,sse4.1")))
static uint32_t Clmul(uint32_t crc, const uint8_t* buf, size_t len) {
    alignas(16) static const uint64_t k1k2[2] = {0x0154442bd4ull, 0x01c6e41596ull};
    alignas(16) static const uint64_t k3k4[2] = {0x01751997d0ull, 0x00ccaa009eull};
    alignas(16) static const uint64_t k5k0[2] = {0x0163cd6124ull, 0x0000000000ull};
    alignas(16) static const uint64_t poly[2] = {0x01db710641ull, 0x01f7011641ull};
    __m128i x0, x1, x2, x3, x4, x5, x6, x7, x8;
    x1 = _mm_loadu_si128((const __m128i*)(buf + 0x00));
    x2 = _mm_loadu_si128((const __m128i*)(buf + 0x10));
    x3 = _mm_loadu_si128((const __m128i*)(buf + 0x20));
    x4 = _mm_loadu_si128((const __m128i*)(buf + 0x30));
    x1 = _mm_xor_si128(x1, _mm_cvtsi32_si128((int)crc));
    x0 = _mm_load_si128((const __m128i*)k1k2);
    buf += 64; len -= 64;
    while (len >= 64) {
        x5 = _mm_clmulepi64_si128(x1, x0, 0x00);
        x6 = _mm_clmulepi64_si128(x2, x0, 0x00);
        x7 = _mm_clmulepi64_si128(x3, x0, 0x00);
        x8 = _mm_clmulepi64_si128(x4, x0, 0x00);
        x1 = _mm_clmulepi64_si128(x1, x0, 0x11);
        x2 = _mm_clmulepi64_si128(x2, x0, 0x11);
        x3 = _mm_clmulepi64_si128(x3, x0, 0x11);
        x4 = _mm_clmulepi64_si128(x4, x0, 0x11);
        x1 = _mm_xor_si128(_mm_xor_si128(x1, x5), _mm_loadu_si128((const __m128i*)(buf + 0x00)));
        x2 = _mm_xor_si128(_mm_xor_si128(x2, x6), _mm_loadu_si128((const __m128i*)(buf + 0x10)));
        x3 = _mm_xor_si128(_mm_xor_si128(x3, x7), _mm_loadu_si128((const __m128i*)(buf + 0x20)));
        x4 = _mm_xor_si128(_mm_xor_si128(x4, x8), _mm_loadu_si128((const __m128i*)(buf + 0x30)));
        buf += 64; len -= 64;
    }
    x0 = _mm_load_si128((const __m128i*)k3k4);
    x5 = _mm_clmulepi64_si128(x1, x0, 0x00); x1 = _mm_clmulepi64_si128(x1, x0, 0x11);
    x1 = _mm_xor_si128(_mm_xor_si128(x1, x2), x5);
    x5 = _mm_clmulepi64_si128(x1, x0, 0x00); x1 = _mm_clmulepi64_si128(x1, x0, 0x11);
    x1 = _mm_xor_si128(_mm_xor_si128(x1, x3), x5);
    x5 = _mm_clmulepi64_si128(x1, x0, 0x00); x1 = _mm_clmulepi64_si128(x1, x0, 0x11);
    x1 = _mm_xor_si128(_mm_xor_si128(x1, x4), x5);
    while (len >= 16) {
        x2 = _mm_loadu_si128((const __m128i*)buf);
        x5 = _mm_clmulepi64_si128(x1, x0, 0x00); x1 = _mm_clmulepi64_si128(x1, x0, 0x11);
        x1 = _mm_xor_si128(_mm_xor_si128(x1, x2), x5);
        buf += 16; len -= 16;
    }
    x2 = _mm_clmulepi64_si128(x1, x0, 0x10);
    x3 = _mm_setr_epi32(~0, 0, ~0, 0);
    x1 = _mm_srli_si128(x1, 8);
    x1 = _mm_xor_si128(x1, x2);
    x0 = _mm_loadl_epi64((const __m128i*)k5k0);
    x2 = _mm_srli_si128(x1, 4);
    x1 = _mm_and_si128(x1, x3);
    x1 = _mm_clmulepi64_si128(x1, x0, 0x00);
    x1 = _mm_xor_si128(x1, x2);
    x0 = _mm_load_si128((const __m128i*)poly);
    x2 = _mm_and_si128(x1, x3);
    x2 = _mm_clmulepi64_si128(x2, x0, 0x10);
    x2 = _mm_and_si128(x2, x3);
    x2 = _mm_clmulepi64_si128(x2, x0, 0x00);
    x1 = _mm_xor_si128(x1, x2);
    return (uint32_t)_mm_extract_epi32(x1, 1);
}

uint32_t FastCrc32(uint32_t crc, const uint8_t* p, size_t n) {
    if (g_mode < 0) Init();  // 여러 스레드가 동시에 들어와도 같은 값을 쓰므로 무해
    uint32_t c = ~crc;
    if (g_mode == 1 && n >= 64) {
        size_t k = n & ~(size_t)15;
        c = Clmul(c, p, k);
        p += k; n -= k;
    }
    return ~Slice8(c, p, n);
}
