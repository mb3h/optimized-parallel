#ifndef Z80PRIV_H_INCLUDED__
#define Z80PRIV_H_INCLUDED__

struct z80_ {
	union {
		struct {
			uint16_t bc, de, hl, af, sp;
		} rr;
		uint16_t r16[5];
		struct {
#if BYTE_ORDER == LITTLE_ENDIAN
			uint8_t c, b, e, d, l, h, f, a, spl, sph;
#else
			uint8_t b, c, d, e, h, l, a, f, spl, sph;
#endif
		} r;
		uint8_t r8[8];
	};
	uint16_t pc;
	struct {
		void *ptr;
		uint32_t flags;
	} mem[8];
};
typedef struct z80_ z80_s;

//#define M1 4
#define M1 (4 +1)

#endif //def Z80PRIV_H_INCLUDED__
