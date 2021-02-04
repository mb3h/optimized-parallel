#ifndef Z80PRIV_H_INCLUDED__
#define Z80PRIV_H_INCLUDED__

#ifndef Tw // fetch wait clocks
# define Tw 0
#endif
#ifndef RRm // memoried size of 16-bits register. (16, 32 or 64 ...)
# define RRm 32
#endif

struct memctl {
	void *raw_or_rwfn;
	void *rwfn_priv;
};
typedef struct memctl memctl_s;

struct z80_ {
	union {
		struct {
#if 16 == RRm
			uint16_t bc, de, hl, sp, fa;
#else //if 32 == RRm
# if BYTE_ORDER == LITTLE_ENDIAN
			uint16_t bc, bc32, de, de32, hl, hl32, sp, sp32, fa, fa32;
# else
			uint16_t bc32, bc, de32, de, hl32, hl, sp32, sp, fa32, fa;
# endif
#endif
		} rr;
#if 16 == RRm
		uint16_t r16[5];
#else //if 32 == RRm
		uint16_t r16[5 * 2];
#endif
		struct {
#if 16 == RRm
# if BYTE_ORDER == LITTLE_ENDIAN
			uint8_t c, b, e, d, l, h, spl, sph, f, a;
# else
			uint8_t b, c, d, e, h, l, spl, sph, a, f;
# endif
#else //if 32 == RRm
# if BYTE_ORDER == LITTLE_ENDIAN
			uint8_t c, b, bc2, bc3, e, d, de2, de3,
			        l, h, hl2, hl3, spl, sph, sp2, sp3,
			        a, f, fa2, fa3;
# else
			uint8_t bc3, bc2, b, c, de3, de2, d, e,
			        hl3, hl2, h, l, sp3, sp2, spl, sph,
			        fa3, fa2, f, a;
# endif
#endif
		} r;
#if 16 == RRm
		uint8_t r8[5 * 2];
#else //if 32 == RRm
		uint8_t r8[5 * 4];
#endif
	};
#if 16 == RRm
	uint16_t pc;
#else //if 32 == RRm
# if BYTE_ORDER == LITTLE_ENDIAN
	uint16_t pc, pc32;
# else
	uint16_t pc32, pc;
# endif
#endif
	memctl_s mem[8];
};
typedef struct z80_ z80_s;

static const int8_t R2I[8] = {
#if 16 == RRm
# if BYTE_ORDER == LITTLE_ENDIAN
	1, 0, 3, 2, 5, 4, -1, 8
# else
	0, 1, 2, 3, 4, 5, -1, 9
# endif
#else //if 32 == RRm
# if BYTE_ORDER == LITTLE_ENDIAN
	1, 0, 5, 4,  9,  8, -1, 16
# else
	2, 3, 6, 7, 10, 11, -1, 19
# endif
#endif
};
static const unsigned RR2I[4] = {
#if 16 == RRm
	0,    1,    2,    3
#else //if 32 == RRm
# if BYTE_ORDER == LITTLE_ENDIAN
	0,    2,    4,    6
# else
	0 +2, 2 +2, 4 +2, 6 +2
# endif
#endif
};

#endif //def Z80PRIV_H_INCLUDED__
