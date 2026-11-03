// zeng.cpp - zeng.h 구현
#include "zeng.h"
#include <cstring>
#ifdef CHAGOK_ZNG
#include "zlib-ng.h"
struct ZEng { zng_stream s; };
static int Map(int st) { return st == Z_STREAM_END ? ZE_END : (st == Z_OK || st == Z_BUF_ERROR) ? ZE_OK : ZE_ERR; }
ZEng* ZDefNew(int level) {
    if (level == 3) level = 4;  // 차곡 '빠름'(3): zlib-ng 3 은 miniz 3 보다 1~2% 커서, 크기가 같은 4 를 쓴다 (그래도 CPU 시간 약 20~25% 적음)
    ZEng* e = new ZEng; memset(&e->s, 0, sizeof(e->s));
    if (zng_deflateInit2(&e->s, level, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK) { delete e; return nullptr; }
    return e;
}
bool ZDefReset(ZEng* e) { return zng_deflateReset(&e->s) == Z_OK; }
int ZDefRun(ZEng* e, const uint8_t*& in, uint32_t& ai, uint8_t*& out, uint32_t& ao, bool fin) {
    e->s.next_in = in; e->s.avail_in = ai; e->s.next_out = out; e->s.avail_out = ao;
    int st = zng_deflate(&e->s, fin ? Z_FINISH : Z_SYNC_FLUSH);
    in = e->s.next_in; ai = e->s.avail_in; out = e->s.next_out; ao = e->s.avail_out;
    return Map(st);
}
void ZDefFree(ZEng* e) { if (e) { zng_deflateEnd(&e->s); delete e; } }
ZEng* ZInfNew() {
    ZEng* e = new ZEng; memset(&e->s, 0, sizeof(e->s));
    if (zng_inflateInit2(&e->s, -15) != Z_OK) { delete e; return nullptr; }
    return e;
}
int ZInfRun(ZEng* e, const uint8_t*& in, uint32_t& ai, uint8_t*& out, uint32_t& ao) {
    e->s.next_in = in; e->s.avail_in = ai; e->s.next_out = out; e->s.avail_out = ao;
    int st = zng_inflate(&e->s, Z_NO_FLUSH);
    in = e->s.next_in; ai = e->s.avail_in; out = e->s.next_out; ao = e->s.avail_out;
    return Map(st);
}
void ZInfFree(ZEng* e) { if (e) { zng_inflateEnd(&e->s); delete e; } }
#else
#include "miniz.h"
struct ZEng { mz_stream s; };
static int Map(int st) { return st == MZ_STREAM_END ? ZE_END : (st == MZ_OK || st == MZ_BUF_ERROR) ? ZE_OK : ZE_ERR; }
ZEng* ZDefNew(int level) {
    ZEng* e = new ZEng; memset(&e->s, 0, sizeof(e->s));
    if (mz_deflateInit2(&e->s, level, MZ_DEFLATED, -MZ_DEFAULT_WINDOW_BITS, 9, MZ_DEFAULT_STRATEGY) != MZ_OK) { delete e; return nullptr; }
    return e;
}
bool ZDefReset(ZEng* e) { return mz_deflateReset(&e->s) == MZ_OK; }
int ZDefRun(ZEng* e, const uint8_t*& in, uint32_t& ai, uint8_t*& out, uint32_t& ao, bool fin) {
    e->s.next_in = in; e->s.avail_in = ai; e->s.next_out = out; e->s.avail_out = ao;
    int st = mz_deflate(&e->s, fin ? MZ_FINISH : MZ_SYNC_FLUSH);
    in = e->s.next_in; ai = e->s.avail_in; out = e->s.next_out; ao = e->s.avail_out;
    return Map(st);
}
void ZDefFree(ZEng* e) { if (e) { mz_deflateEnd(&e->s); delete e; } }
ZEng* ZInfNew() {
    ZEng* e = new ZEng; memset(&e->s, 0, sizeof(e->s));
    if (mz_inflateInit2(&e->s, -MZ_DEFAULT_WINDOW_BITS) != MZ_OK) { delete e; return nullptr; }
    return e;
}
int ZInfRun(ZEng* e, const uint8_t*& in, uint32_t& ai, uint8_t*& out, uint32_t& ao) {
    e->s.next_in = in; e->s.avail_in = ai; e->s.next_out = out; e->s.avail_out = ao;
    int st = mz_inflate(&e->s, MZ_NO_FLUSH);
    in = e->s.next_in; ai = e->s.avail_in; out = e->s.next_out; ao = e->s.avail_out;
    return Map(st);
}
void ZInfFree(ZEng* e) { if (e) { mz_inflateEnd(&e->s); delete e; } }
#endif
