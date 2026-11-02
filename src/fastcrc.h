// 빠른 CRC-32 (ZIP 과 같은 다항식 0xEDB88320, mz_crc32 와 같은 결과)
//  - CPU 가 PCLMULQDQ 를 지원하면(2010년 이후 x86-64 거의 전부) 캐리리스 곱셈 접기 방식: 수 GB/s
//  - 아니면 slice-by-8 표 방식
#pragma once
#include <stdint.h>
#include <stddef.h>
uint32_t FastCrc32(uint32_t crc, const uint8_t* p, size_t n);
