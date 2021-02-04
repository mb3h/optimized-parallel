#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include "z80.h"
#include "z80priv.h"
#ifndef Tw
# define M1 4
#else
# define M1 (4 +Tw)
#endif

#include "assert.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

typedef uint32_t u32;
typedef  uint8_t u8;
typedef uint16_t u16;

#define arraycountof(x) (sizeof(x)/sizeof((x)[0]))

#define CF 0x01
#define NF 0x02
#define VF 0x04
#define PF VF
#define HF 0x10
#define ZF 0x40
#define SF 0x80

#ifndef Em // emulation not optimized.
# define PTR16(mmap8, addr) ((mmap8)[7 & (addr) >> 13].raw_or_rwfn)
# define PTR16OFS(addr) (0x1FFF & (addr))

// FETCH
static u8 fetch_to_n (z80_s *m_)
{
	++m_->pc;
u8 *src;
	src = (u8 *)PTR16(m_->mem, m_->pc);
BUG(src)
	src += PTR16OFS(m_->pc);
u8 n;
	n = src[0];
	return n;
}
static u16 fetch_to_nn (z80_s *m_)
{
	++m_->pc;
u8 *src;
	src = (u8 *)PTR16(m_->mem, m_->pc);
BUG(src)
	src += PTR16OFS(m_->pc);
u16 nn;
	nn = src[0];
	++m_->pc;
	nn |= src[1] << 8;
	return nn;
}

// 0X1
static unsigned ld_rr_nn (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned r16;
	r16 = 3 & p[0] >> 4;
u16 nn;
	nn = fetch_to_nn (m_);
u16 *dst;
	dst = m_->r16 + RR2I[r16];
	*dst = nn;
	return M1 +3 +3;
}
// TODO: flag register
static unsigned add_hl_rr (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned r16;
	r16 = 3 & p[0] >> 4;
const u16 *src;
	src = m_->r16 + RR2I[r16];
u16 *dst;
	dst = &m_->rr.hl;
	*dst += *src;
	return M1 +4 +3;
}

// 0X2
static unsigned ld_rr_a (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned r16;
	r16 = 3 & p[0] >> 4;
const u16 *rr;
	rr = m_->r16 + RR2I[r16];
u8 *dst;
	dst = (u8 *)PTR16(m_->mem, *rr);
BUG(dst)
	dst += PTR16OFS(*rr);
	*dst = m_->r.a;
	return M1 +3;
}
static unsigned ld_a_rr (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned r16;
	r16 = 3 & p[0] >> 4;
const u16 *rr;
	rr = m_->r16 + RR2I[r16];
const u8 *src;
	src = (const u8 *)PTR16(m_->mem, *rr);
BUG(src)
	src += PTR16OFS(*rr);
	m_->r.a = *src;
	return M1 +3;
}
static unsigned ld_pnn_hl (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
u16 nn;
	nn = fetch_to_nn (m_);
u16 *dst;
	dst = (u16 *)PTR16(m_->mem, nn);
BUG(dst)
	dst += PTR16OFS(nn);
	*dst = m_->rr.hl;
	return M1 +3 +3 +3 +3;
}
static unsigned ld_hl_pnn (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
u16 nn;
	nn = fetch_to_nn (m_);
const u16 *src;
	src = (const u16 *)PTR16(m_->mem, nn);
BUG(src)
	src += PTR16OFS(nn);
	m_->rr.hl = *src;
	return M1 +3 +3 +3 +3;
}
static unsigned ld_pnn_a (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
u16 nn;
	nn = fetch_to_nn (m_);
u8 *dst;
	dst = (u8 *)PTR16(m_->mem, nn);
BUG(dst)
	dst += PTR16OFS(nn);
	*dst = m_->r.a;
	return M1 +3 +3 +3 +3;
}
static unsigned ld_a_pnn (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
u16 nn;
	nn = fetch_to_nn (m_);
const u8 *src;
	src = (const u8 *)PTR16(m_->mem, nn);
BUG(src)
	src += PTR16OFS(nn);
	m_->r.a = *src;
	return M1 +3 +3 +3 +3;
}

// 0X3
static unsigned inc_rr (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned r16;
	r16 = 3 & p[0] >> 4;
u16 *dst;
	dst = m_->r16 + RR2I[r16];
	++*dst;
	return M1 +2;
}

static unsigned dec_rr (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned r16;
	r16 = 3 & p[0] >> 4;
u16 *dst;
	dst = m_->r16 + RR2I[r16];
	--*dst;
	return M1 +2;
}

// 0X4
static unsigned inc_r (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned r8;
	r8 = 7 & p[0] >> 3;
unsigned retval;
u8 *dst;
	switch (r8) {
	case 6:
		dst = (u8 *)PTR16(m_->mem, m_->rr.hl);
BUG(dst)
		dst += PTR16OFS(m_->rr.hl);
		retval = M1 +3 +3;
		break;
	default:
BUG(0 <= R2I[r8] && R2I[r8] < arraycountof(m_->r8))
		dst = m_->r8 + R2I[r8];
		retval = M1;
		break;
	}
	*dst = (u8)(*dst +1);
	m_->r.f &= ~(SF|ZF|HF|VF|NF);
	m_->r.f |= (0x7F < *dst) ? SF : 0;
	m_->r.f |= (0x00 == *dst) ? ZF : 0;
	m_->r.f |= (0x00 == (0x0F & *dst)) ? HF : 0;
	m_->r.f |= (0x80 == *dst) ? VF : 0;
	return retval;
}

// 0X5
// TODO: flag register
static unsigned dec_r (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned r8;
	r8 = 7 & p[0] >> 3;
u8 *dst;
	switch (r8) {
	case 6:
		dst = (u8 *)PTR16(m_->mem, m_->rr.hl);
BUG(dst)
		dst += PTR16OFS(m_->rr.hl);
		*dst = (u8)(*dst +0xFF);
		return M1 +3 +3;
	default:
BUG(0 <= R2I[r8] && R2I[r8] < arraycountof(m_->r8))
		dst = m_->r8 + R2I[r8];
		*dst = (u8)(*dst +0xFF);
		return M1;
	}
}

// 0X6
static unsigned ld_r_n (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned r8;
	r8 = 7 & p[0] >> 3;
u8 n;
	n = fetch_to_n (m_);
u8 *dst;
	switch (r8) {
	case 6:
		dst = (u8 *)PTR16(m_->mem, m_->rr.hl);
BUG(dst)
		dst += PTR16OFS(m_->rr.hl);
		*dst = n;
		return M1 +3 +3;
	default:
BUG(0 <= R2I[r8] && R2I[r8] < arraycountof(m_->r8))
		dst = m_->r8 + R2I[r8];
		*dst = n;
		return M1 +3;
	}
}

static unsigned ld_r_r (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
unsigned r8;
	r8 = 7 & p[0];
u8 *src;
	switch (r8) {
	case 6:
		src = (u8 *)PTR16(m_->mem, m_->rr.hl);
BUG(src)
		src += PTR16OFS(m_->rr.hl);
		clocks += 3;
		break;
	default:
BUG(0 <= R2I[r8] && R2I[r8] < arraycountof(m_->r8))
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
		clocks += 3;
		break;
	default:
BUG(0 <= R2I[r8] && R2I[r8] < arraycountof(m_->r8))
		dst = m_->r8 + R2I[r8];
	}
	*dst = *src;
	return clocks;
}

static unsigned xor_r (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned r8;
	r8 = 7 & p[0];
unsigned retval;
u8 *dst;
	switch (r8) {
	case 6:
		dst = (u8 *)PTR16(m_->mem, m_->rr.hl);
BUG(dst)
		dst += PTR16OFS(m_->rr.hl);
		retval = M1 +3;
		break;
	default:
BUG(0 <= R2I[r8] && R2I[r8] < arraycountof(m_->r8))
		dst = m_->r8 + R2I[r8];
		retval = M1;
		break;
	}
	*dst = (u8)(*dst ^ m_->r.a);
	m_->r.f &= ~(SF|ZF|HF|VF|NF|CF);
	m_->r.f |= (0x7F < *dst) ? SF : 0;
	m_->r.f |= (0x00 == *dst) ? ZF : 0;
static u8 pv[16] = {
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0
};
	m_->r.f |= !(pv[15 & *dst] ^ pv[15 & *dst >> 4]) ? PF : 0;
	return retval;
}

static unsigned (*z80_opcode[256]) (z80_s *m_, const u8 *p) = {
	  NULL  , ld_rr_nn , ld_rr_a  , inc_rr, inc_r , dec_r , ld_r_n, NULL  
	, NULL  , add_hl_rr, ld_a_rr  , dec_rr, inc_r , dec_r , ld_r_n, NULL  
	, NULL  , ld_rr_nn , ld_rr_a  , inc_rr, inc_r , dec_r , ld_r_n, NULL  
	, NULL  , add_hl_rr, ld_a_rr  , dec_rr, inc_r , dec_r , ld_r_n, NULL  
	, NULL  , ld_rr_nn , ld_pnn_hl, inc_rr, inc_r , dec_r , ld_r_n, NULL  
	, NULL  , add_hl_rr, ld_hl_pnn, dec_rr, inc_r , dec_r , ld_r_n, NULL  
	, NULL  , ld_rr_nn , ld_pnn_a , inc_rr, inc_r , dec_r , ld_r_n, NULL  
	, NULL  , add_hl_rr, ld_a_pnn , dec_rr, inc_r , dec_r , ld_r_n, NULL  

	, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r
	, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r
	, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r
	, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r
	, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r
	, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r
	, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, NULL  , ld_r_r
	, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r

	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL 
	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL 
	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL 
	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL 
	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL 
	, xor_r , xor_r , xor_r , xor_r , xor_r , xor_r , xor_r , xor_r 
	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL 
	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL 

	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  
	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  
	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  
	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  
	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  
	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  
	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  
	, NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  , NULL  
};

unsigned __attribute__((stdcall)) z80_exec (struct z80 *this_, unsigned min_clocks)
{
BUG(this_ && 0 < min_clocks)
z80_s *m_;
	m_ = (z80_s *)this_;
	--m_->pc;
unsigned clocks;
	for (clocks = 0; clocks < min_clocks;) {
		++m_->pc;
u8 *p;
		p = (u8 *)PTR16(m_->mem, m_->pc);
BUG(p)
		p += PTR16OFS(m_->pc);
BUG(z80_opcode[*p])
		clocks += z80_opcode[*p] (m_, p);
	}
	++m_->pc;
	return clocks;
}
#endif //ndef Em

bool z80_init (struct z80 *this_, size_t cb)
{
BUG(this_)
BUG(sizeof(z80_s) <= cb)
	memset (this_, 0, sizeof(z80_s));
}
