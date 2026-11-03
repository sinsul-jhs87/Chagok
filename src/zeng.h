// zeng.h - deflate/inflate 엔진 감싸기 (miniz 또는 zlib-ng, zeng.cpp 를 CHAGOK_ZNG 로 빌드하면 zlib-ng)
#pragma once
#include <cstdint>
struct ZEng;
enum { ZE_OK = 0, ZE_END = 1, ZE_ERR = -1 };
// 압축: 블록마다 Reset 후 Run. fin=false 면 SYNC_FLUSH(바이트 정렬, 이어 붙이기용), true 면 FINISH
ZEng* ZDefNew(int level);
bool ZDefReset(ZEng* e);
int ZDefRun(ZEng* e, const uint8_t*& in, uint32_t& availIn, uint8_t*& out, uint32_t& availOut, bool fin);
void ZDefFree(ZEng* e);
// 풀기 (raw deflate)
ZEng* ZInfNew();
int ZInfRun(ZEng* e, const uint8_t*& in, uint32_t& availIn, uint8_t*& out, uint32_t& availOut);
void ZInfFree(ZEng* e);
