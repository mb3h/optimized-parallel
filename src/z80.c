#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include "z80.h"
#include "assert.h"

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

typedef uint32_t u32;
typedef  uint8_t u8;
typedef uint16_t u16;

struct z80_ {
	union {
		struct {
			u16 bc, de, hl, af, sp;
		} rr;
		u16 r16[5];
		struct {
#if BYTE_ORDER == LITTLE_ENDIAN
			u8 c, b, e, d, l, h, f, a, spl, sph;
#else
			u8 b, c, d, e, h, l, a, f, spl, sph;
#endif
		} r;
		u8 r8[8];
	};
	u16 pc;
	struct {
		void *ptr;
		u32 flags;
	} mem[8];
};
typedef struct z80_ z80_s;

static const unsigned R2I[8] = {
#if BYTE_ORDER == LITTLE_ENDIAN
	1, 0, 3, 2, 5, 4, 8, 7
#else
	0, 1, 2, 3, 4, 5, 7, 8
#endif
};
static const unsigned RR2I[4] = {
	0, 1, 2, 4
};

//#define M1 4
#define M1 (4 +1)

#define PTR16(mmap8, addr) ((mmap8)[7 & (addr) >> 13].ptr)
#define PTR16OFS(addr) (0x1FFF & (addr))


static unsigned ld_rr_nn (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned r16;
	r16 = 3 & p[0] >> 4;
u8 *src;
	++m_->pc;
	src = (u8 *)PTR16(m_->mem, m_->pc);
BUG(src)
	src += PTR16OFS(m_->pc);
u16 nn;
	nn = src[0];
	++m_->pc;
	nn |= src[1] << 8;
	++m_->pc;
u16 *dst;
	dst = m_->r16 + RR2I[r16];
	*dst = nn;
	return M1 +3 +3;
}

static unsigned inc_rr (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned r16;
	r16 = 3 & p[0] >> 4;
	++m_->pc;
u16 *dst;
	dst = m_->r16 + RR2I[r16];
	*dst = (u16)(*dst +1);
	return M1 +2;
}

static unsigned inc_r (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned r8;
	r8 = 7 & p[0] >> 3;
u8 *src;
	++m_->pc;
u8 *dst;
	switch (r8) {
	case 6:
		dst = (u8 *)PTR16(m_->mem, m_->rr.hl);
BUG(dst)
		dst += PTR16OFS(m_->rr.hl);
		*dst = (u8)(*dst +1);
		return M1 +3 +3;
	default:
		dst = m_->r8 + R2I[r8];
		*dst = (u8)(*dst +1);
		return M1;
	}
}

static unsigned ld_r_n (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned r8;
	r8 = 7 & p[0] >> 3;
u8 *src;
	++m_->pc;
	src = (u8 *)PTR16(m_->mem, m_->pc);
BUG(src)
	src += PTR16OFS(m_->pc);
	++m_->pc;
u8 *dst;
	switch (r8) {
	case 6:
		dst = (u8 *)PTR16(m_->mem, m_->rr.hl);
BUG(dst)
		dst += PTR16OFS(m_->rr.hl);
		*dst = *src;
		return M1 +3 +3;
	default:
		dst = m_->r8 + R2I[r8];
		*dst = *src;
		return M1 +3;
	}
}

static unsigned ld_r_r (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned r8;
	r8 = 7 & p[0];
u8 *src;
	switch (r8) {
	case 6:
		src = (u8 *)PTR16(m_->mem, m_->rr.hl);
BUG(src)
		src += PTR16OFS(m_->rr.hl);
		break;
	default:
		src = m_->r8 + R2I[r8];
		break;
	}
	r8 = 7 & p[0] >> 3;
u8 *dst;
	switch (r8) {
	case 6:
		dst = (u8 *)PTR16(m_->mem, m_->rr.hl);
BUG(dst)
		dst += PTR16OFS(m_->rr.hl);
		break;
	default:
		dst = m_->r8 + R2I[r8];
	}
	*dst = *src;
	++m_->pc;
	return M1 +3;
}

static unsigned xor_r (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned r8;
	r8 = 7 & p[0] >> 3;
u8 *src;
	++m_->pc;
u8 *dst;
	switch (r8) {
	case 6:
		dst = (u8 *)PTR16(m_->mem, m_->rr.hl);
BUG(dst)
		dst += PTR16OFS(m_->rr.hl);
		*dst = (u8)(*dst ^ m_->r.a);
		return M1 +3;
	default:
		dst = m_->r8 + R2I[r8];
		*dst = (u8)(*dst ^ m_->r.a);
		return M1;
	}
}

static unsigned (*z80_opcode[256]) (z80_s *m_, const u8 *p) = {
	  NULL  , ld_rr_nn, NULL  , inc_rr, inc_r , NULL  , ld_r_n, NULL  
	, NULL  , NULL    , NULL  , NULL  , inc_r , NULL  , ld_r_n, NULL  
	, NULL  , ld_rr_nn, NULL  , inc_rr, inc_r , NULL  , ld_r_n, NULL  
	, NULL  , NULL    , NULL  , NULL  , inc_r , NULL  , ld_r_n, NULL  
	, NULL  , ld_rr_nn, NULL  , inc_rr, inc_r , NULL  , ld_r_n, NULL  
	, NULL  , NULL    , NULL  , NULL  , inc_r , NULL  , ld_r_n, NULL  
	, NULL  , ld_rr_nn, NULL  , inc_rr, inc_r , NULL  , ld_r_n, NULL  
	, NULL  , NULL    , NULL  , NULL  , inc_r , NULL  , ld_r_n, NULL  

	, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r
	, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r
	, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r
	, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r
	, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r
	, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r
	, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, NULL  , ld_r_r
	, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r

	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , xor_r 
	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , xor_r 
	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , xor_r 
	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , xor_r 
	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , xor_r 
	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , xor_r 
	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , xor_r 
	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , xor_r 

	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  
	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  
	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  
	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  
	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  
	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  
	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  
	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  
};

unsigned z80_exec (struct z80 *this_, unsigned min_clocks)
{
BUG(this_ && 0 < min_clocks)
z80_s *m_;
	m_ = (z80_s *)this_;
unsigned clocks;
	for (clocks = 0; clocks < min_clocks;) {
u8 *p;
		p = (u8 *)PTR16(m_->mem, m_->pc);
BUG(p)
		p += PTR16OFS(m_->pc);
BUG(z80_opcode[*p])
		clocks += z80_opcode[*p] (m_, p);
	}
	return clocks;
}

bool z80_init (struct z80 *this_, size_t cb)
{
BUG(this_)
BUG(sizeof(z80_s) <= cb)
	memset (this_, 0, sizeof(z80_s));
}

#if 1
int main (int argc, char *argv[])
{
static u8 test_code[] = {
	  0x21, 0x00, 0xA0, 0x3E, 0x48, 0x77, 0x2C, 0x36 // 533 53 53 5 5-
	, 0x65, 0x23, 0x3E, 0x6C, 0x77, 0x2C, 0x36, 0x6C // 33 7 53 53 5 533
	, 0x23, 0x3E, 0x6F, 0x77, 0x2C, 0x36, 0x20, 0x23 // 7 53 53 5 533 7
	, 0x3E, 0x77, 0x77, 0x2C, 0x36, 0x6F, 0x23, 0x3E // 53 53 5 533 7 5-
	, 0x72, 0x77, 0x2C, 0x36, 0x6C, 0x23, 0x3E, 0x64 // 3 53 5 533 7 53
	, 0x77, 0x2C, 0x36, 0x21, 0x23, 0xAF, 0x77       // 53 5 533 7 5 53
}; // (258 states)
struct z80 z80;
	z80_init (&z80, sizeof(z80));
z80_s *m_;
	m_ = (z80_s *)&z80;

	// memory layout debug
BUG(&m_->rr.de == &m_->r16[1])
BUG(&m_->rr.sp == &m_->r16[4])
#if BYTE_ORDER == LITTLE_ENDIAN
BUG(&m_->rr.de == (u16 *)&m_->r.e)
#else
BUG(&m_->rr.de == (u16 *)&m_->r.d)
#endif

u8 mem0[0x30], mem5[0x2000];
BUG(sizeof(test_code) <= sizeof(mem0))
	memcpy (mem0, test_code, sizeof(test_code));
	memset (mem5, 0, sizeof(mem5));
unsigned executed;
	m_->mem[0].ptr = mem0, m_->mem[0].flags = 0;
	m_->mem[5].ptr = mem5, m_->mem[5].flags = 0;
	m_->pc = 0x0000;
	executed = z80_exec (&z80, 251);
BUG(258 == executed)
BUG(0x002F == m_->pc)
BUG(0 == strcmp ("Hello world!", (const char *)mem5))
	fprintf (stderr, VTGG "%s" VTO "\n", (const char *)mem5);
	return 0;
}
#endif
