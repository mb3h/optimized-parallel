/* cf)
	Z80 Family CPU User Manual (Rev.05 - Feb'05)
	Intel IA-32(R) Architectures Software Developer's Manual,
	- Volume 1: Basic Architecture
	- Volume 2: Instruction Set Reference
	- Volume 3: System Programming Guide
 */
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
#include "portab.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

typedef uint32_t u32;
typedef  uint8_t u8;
typedef uint16_t u16;
typedef   int8_t s8;

#define arraycountof(x) (sizeof(x)/sizeof((x)[0]))
#define swap(a, b, t) t = a, a = b, b = t;

#define  CF 0x01
#define  NF 0x02
#define  VF 0x04
#define  PF VF
#define R1F 0x08 // b3 is reserved.
#define  HF 0x10
#define R2F 0x20 // b5 is reserved.
#define  ZF 0x40
#define  SF 0x80

#ifndef Ei386 // emulation not optimized.
# define PTR16(mmap8, addr) ((mmap8)[7 & (addr) >> 13].raw_or_rwfn)
# define PTR16OFS(addr) (0x1FFF & (addr))

///////////////////////////////////////////////////////////////////////////////

// RELATIVE ADDRESSING
static u16 rel_add16 (u16 nn, u8 e, unsigned *clocks)
{
BUG(clocks)
u16 alu;
	alu = (u16)(nn + (s8)e);
	*clocks += 5;
	return alu;
}

// MEMORY (READ)
static u8 read_to_n (const memctl_s *mem, u16 addr, unsigned *clocks)
{
BUG(clocks)
u8 *src;
	src = (u8 *)PTR16(mem, addr);
BUG(src)
	src += PTR16OFS(addr);
u8 n;
	n = *src;
	*clocks += 3;
	return n;
}
static u16 read_to_nn (const memctl_s *mem, u16 addr, unsigned *clocks)
{
u16 nn;
	nn  = read_to_n (mem, addr, clocks);
	nn |= read_to_n (mem, addr +1, clocks) << 8;
	return nn;
}

// MEMORY (WRITE)
static void n_to_write (memctl_s *mem, u8 n, u16 addr, unsigned *clocks)
{
BUG(clocks)
	*clocks += 3;
u8 *dst;
	dst = (u8 *)PTR16(mem, addr);
BUG(dst)
	dst += PTR16OFS(addr);
	*dst = n;
}
static void nn_to_write (memctl_s *mem, u16 nn, u16 addr, unsigned *clocks)
{
BUG(clocks)
	n_to_write (mem, 255 & nn, addr, clocks);
	n_to_write (mem, 255 & nn >> 8, addr +1, clocks);
}

// FETCH
static u8 fetch_to_n (z80_s *m_, unsigned *clocks)
{
BUG(clocks)
	++m_->pc;
	return read_to_n (m_->mem, m_->pc, clocks);
}
static u16 fetch_to_nn (z80_s *m_, unsigned *clocks)
{
BUG(clocks)
	m_->pc += 2;
	return read_to_nn (m_->mem, m_->pc -1, clocks);
}
static unsigned r8_read_to_n (z80_s *m_, unsigned r8, unsigned *clocks)
{
BUG(clocks)
u8 n;
	if (6 == r8)
		n = read_to_n (m_->mem, m_->rr.hl, clocks);
	else {
BUG(0 <= R2I[r8] && R2I[r8] < arraycountof(m_->r8))
		n = *(m_->r8 + R2I[r8]);
	}
	return n;
}
static void n_to_write_r8 (z80_s *m_, u8 n, unsigned r8, unsigned *clocks)
{
	if (6 == r8)
		n_to_write (m_->mem, n, m_->rr.hl, clocks);
	else {
BUG(0 <= R2I[r8] && R2I[r8] < arraycountof(m_->r8))
		*(m_->r8 + R2I[r8]) = n;
	}
}

// ALU core
static u8 inc8 (u8 lhs, u8 *f)
{
BUG(f)
u8 alu;
	alu = (u8)(lhs +1);
	*f &= ~(SF|ZF|HF|VF|NF|R1F|R2F);
	*f |= (R1F|R2F) & alu;
	*f |= (0x7F < alu) ? SF : 0;
	*f |= (0x00 == alu) ? ZF : 0;
	*f |= (0x00 == (0x0F & alu)) ? HF : 0;
	*f |= (0x80 == alu) ? VF : 0;
	return alu;
}
static u8 dec8 (u8 lhs, u8 *f)
{
BUG(f)
u8 alu;
	alu = (u8)(lhs +0xFF);
	*f &= ~(SF|ZF|HF|VF|NF|R1F|R2F);
	*f |= (R1F|R2F) & alu;
	*f |= (0x7F < alu) ? SF : 0;
	*f |= (0x00 == alu) ? ZF : 0;
	*f |= (0x0F == (0x0F & alu)) ? HF : 0;
	*f |= (0x7F == alu) ? VF : 0;
	*f |= NF;
	return alu;
}
static u8 add8 (u8 lhs, u8 rhs, u8 *f)
{
BUG(f)
u8 alu;
	alu = (u8)(lhs + rhs);
	*f &= 0; // ~(SF|ZF|HF|VF|NF|CF|R1F|R2F)
	*f |= (R1F|R2F) & alu;
	*f |= (15 < (15 & lhs) + (15 & rhs)) ? HF : 0;
	*f |= (0x7F < alu) ? SF : 0;
	*f |= (0x00 == alu) ? ZF : 0;
	*f |= ((s8)lhs + (s8)rhs < -128 || 127 < (s8)lhs + (s8)rhs) ? VF : 0;
	*f |= (alu < rhs) ? CF : 0;
	return alu;
}
static u8 adc8 (u8 lhs, u8 rhs, u8 *f)
{
BUG(f)
u8 cy;
	cy = (CF & *f) ? 1 : 0;
u8 alu;
	alu = (u8)(lhs + rhs + cy);
	*f &= 0; // ~(SF|ZF|HF|VF|NF|CF|R1F|R2F)
	*f |= (R1F|R2F) & alu;
	*f |= (15 < (15 & lhs) + (15 & rhs) + cy) ? HF : 0;
	*f |= (0x7F < alu) ? SF : 0;
	*f |= (0x00 == alu) ? ZF : 0;
	*f |= ((s8)lhs + (s8)rhs + cy < -128 || 127 < (s8)lhs + (s8)rhs + cy) ? VF : 0;
	*f |= (alu < rhs + cy) ? CF : 0;
	return alu;
}
static u8 sub8 (u8 lhs, u8 rhs, u8 *f)
{
BUG(f)
u8 alu;
	alu = (u8)(lhs +0x100 - rhs);
	*f &= 0; // ~(SF|ZF|HF|VF|NF|CF|R1F|R2F)
	*f |= (R1F|R2F) & alu;
	*f |= ((15 & lhs) - (15 & rhs) < 0) ? HF : 0;
	*f |= (0x7F < alu) ? SF : 0;
	*f |= (0x00 == alu) ? ZF : 0;
	*f |= ((s8)lhs - (s8)rhs < -128 || 127 < (s8)lhs - (s8)rhs) ? VF : 0;
	*f |= NF;
	*f |= (0xFF < alu + rhs) ? CF : 0;
	return alu;
}
static u8 sbc8 (u8 lhs, u8 rhs, u8 *f)
{
BUG(f)
u8 cy;
	cy = (CF & *f) ? 1 : 0;
u8 alu;
	alu = (u8)(lhs +0x100 - cy - rhs);
	*f &= 0; // ~(SF|ZF|HF|VF|NF|CF|R1F|R2F)
	*f |= (R1F|R2F) & alu;
	*f |= ((15 & lhs) - (15 & rhs) - cy < 0) ? HF : 0;
	*f |= (0x7F < alu) ? SF : 0;
	*f |= (0x00 == alu) ? ZF : 0;
	*f |= ((s8)lhs - (s8)rhs - cy < -128 || 127 < (s8)lhs - (s8)rhs - cy) ? VF : 0;
	*f |= NF;
	*f |= (0xFF < alu + rhs + cy) ? CF : 0;
	return alu;
}
static u8 odd_even[16] = {
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0
};
static u8 and8 (u8 lhs, u8 rhs, u8 *f)
{
BUG(f)
u8 alu;
	alu = (u8)(lhs & rhs);
	*f &= 0; // ~(SF|ZF|HF|VF|NF|CF|R1F|R2F)
	*f |= (R1F|R2F) & alu;
	*f |= HF;
	*f |= (0x7F < alu) ? SF : 0;
	*f |= (0x00 == alu) ? ZF : 0;
	*f |= !(odd_even[15 & alu] ^ odd_even[15 & alu >> 4]) ? PF : 0;
	return alu;
}
static u8 xor8 (u8 lhs, u8 rhs, u8 *f)
{
BUG(f)
u8 alu;
	alu = (u8)(lhs ^ rhs);
	*f &= 0; // ~(SF|ZF|HF|VF|NF|CF|R1F|R2F)
	*f |= (R1F|R2F) & alu;
	*f |= (0x7F < alu) ? SF : 0;
	*f |= (0x00 == alu) ? ZF : 0;
	*f |= !(odd_even[15 & alu] ^ odd_even[15 & alu >> 4]) ? PF : 0;
	return alu;
}
static u8 or8 (u8 lhs, u8 rhs, u8 *f)
{
BUG(f)
u8 alu;
	alu = (u8)(lhs | rhs);
	*f &= 0; // ~(SF|ZF|HF|VF|NF|CF|R1F|R2F)
	*f |= (R1F|R2F) & alu;
	*f |= (0x7F < alu) ? SF : 0;
	*f |= (0x00 == alu) ? ZF : 0;
	*f |= !(odd_even[15 & alu] ^ odd_even[15 & alu >> 4]) ? PF : 0;
	return alu;
}
static u8 cp8 (u8 lhs, u8 rhs, u8 *f)
{
BUG(f)
	sub8 (lhs, rhs, f);
	*f &= ~(R1F|R2F);
	*f |= (R1F|R2F) & rhs;
	return lhs;
}

// 0X0
static unsigned nop (z80_s *m_, const u8 *p)
{
	return M1;
}
static unsigned ex_af_af (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u16 nn;
	nn = m_->rr.alt_af;
	m_->rr.alt_af = m_->r.a << 8 | m_->r.f;
	m_->rr.fa = (0xff & nn >> 8) | (0xff00 & nn << 8);
	return clocks;
}
static unsigned djnz (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1 +1;
u8 e;
	e = fetch_to_n (m_, &clocks);
	if (0 == --m_->r.b)
		return clocks;
	m_->pc = rel_add16 (m_->pc, e, &clocks);
	return clocks;
}
static unsigned jr (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u8 e;
	e = fetch_to_n (m_, &clocks);
	m_->pc = rel_add16 (m_->pc, e, &clocks);
	return clocks;
}
static unsigned cond_jr (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u8 e;
	e = fetch_to_n (m_, &clocks);
	switch (1 & p[0] >> 4) {
	case 0:
		if (! ((ZF & m_->r.f) == ZF * (1 & p[0] >> 3)))
			return clocks;
		break;
	case 1:
		if (! ((CF & m_->r.f) == CF * (1 & p[0] >> 3)))
			return clocks;
		break;
	}
	m_->pc = rel_add16 (m_->pc, e, &clocks);
	return clocks;
}

// 0X1
static unsigned ld_rr_nn (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u16 *rr;
	rr = m_->r16 + RR2I[3 & p[0] >> 4];
	*rr = fetch_to_nn (m_, &clocks);
	return clocks;
}
// TODO: flag register
static unsigned add_hl_rr (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
const u16 *rr;
	rr = m_->r16 + RR2I[3 & p[0] >> 4];
	m_->rr.hl += *rr;
	return M1 +4 +3;
}

// 0X2
static unsigned ld_rr_a (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
const u16 *rr;
	rr = m_->r16 + RR2I[3 & p[0] >> 4];
	n_to_write (m_->mem, m_->r.a, *rr, &clocks);
	return clocks;
}
static unsigned ld_a_rr (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
const u16 *rr;
	rr = m_->r16 + RR2I[3 & p[0] >> 4];
	m_->r.a = read_to_n (m_->mem, *rr, &clocks);
	return clocks;
}
static unsigned ld_pnn_hl (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u16 nn;
	nn = fetch_to_nn (m_, &clocks);
	nn_to_write (m_->mem, m_->rr.hl, nn, &clocks);
	return clocks;
}
static unsigned ld_hl_pnn (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u16 nn;
	nn = fetch_to_nn (m_, &clocks);
	m_->rr.hl = read_to_nn (m_->mem, nn, &clocks);
	return clocks;
}
static unsigned ld_pnn_a (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u16 nn;
	nn = fetch_to_nn (m_, &clocks);
	n_to_write (m_->mem, m_->r.a, nn, &clocks);
	return clocks;
}
static unsigned ld_a_pnn (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u16 nn;
	nn = fetch_to_nn (m_, &clocks);
	m_->r.a = read_to_n (m_->mem, nn, &clocks);
	return clocks;
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
unsigned clocks;
	clocks = M1;
unsigned r8;
	r8 = 7 & p[0] >> 3;
u8 n;
	n = r8_read_to_n (m_, r8, &clocks);
	n = inc8 (n, &m_->r.f);
	if (6 == r8)
		++clocks; // (M1,4,3)
	n_to_write_r8 (m_, n, r8, &clocks);
	return clocks;
}

// 0X5
static unsigned dec_r (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
unsigned r8;
	r8 = 7 & p[0] >> 3;
u8 n;
	n = r8_read_to_n (m_, r8, &clocks);
	n = dec8 (n, &m_->r.f);
	if (6 == r8)
		++clocks; // (M1,4,3)
	n_to_write_r8 (m_, n, r8, &clocks);
	return clocks;
}

// 0X6
static unsigned ld_r_n (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u8 n;
	n = fetch_to_n (m_, &clocks);
	n_to_write_r8 (m_, n, 7 & p[0] >> 3, &clocks);
	return clocks;
}

// 1XX
static unsigned ld_r_r (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u8 n;
	n = r8_read_to_n (m_, 7 & p[0], &clocks);
	n_to_write_r8 (m_, n, 7 & p[0] >> 3, &clocks);
	return clocks;
}

// 20X
static unsigned add_r (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u8 n;
	n = r8_read_to_n (m_, 7 & p[0], &clocks);
	m_->r.a = add8 (m_->r.a, n, &m_->r.f);
	return clocks;
}

// 21X
static unsigned adc_r (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u8 n;
	n = r8_read_to_n (m_, 7 & p[0], &clocks);
	m_->r.a = adc8 (m_->r.a, n, &m_->r.f);
	return clocks;
}

// 22X
static unsigned sub_r (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u8 n;
	n = r8_read_to_n (m_, 7 & p[0], &clocks);
	m_->r.a = sub8 (m_->r.a, n, &m_->r.f);
	return clocks;
}

// 23X
static unsigned sbc_r (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u8 n;
	n = r8_read_to_n (m_, 7 & p[0], &clocks);
	m_->r.a = sbc8 (m_->r.a, n, &m_->r.f);
	return clocks;
}

// 24X
static unsigned and_r (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u8 n;
	n = r8_read_to_n (m_, 7 & p[0], &clocks);
	m_->r.a = and8 (m_->r.a, n, &m_->r.f);
	return clocks;
}

// 25X
static unsigned xor_r (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u8 n;
	n = r8_read_to_n (m_, 7 & p[0], &clocks);
	m_->r.a = xor8 (m_->r.a, n, &m_->r.f);
	return clocks;
}

// 26X
static unsigned or_r (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u8 n;
	n = r8_read_to_n (m_, 7 & p[0], &clocks);
	m_->r.a = or8 (m_->r.a, n, &m_->r.f);
	return clocks;
}

// 27X
static unsigned cp_r (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u8 n;
	n = r8_read_to_n (m_, 7 & p[0], &clocks);
	cp8 (m_->r.a, n, &m_->r.f);
	return clocks;
}

// 3X1
static unsigned exx (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u16 t;
	swap(m_->rr.alt_hl, m_->rr.hl, t);
	swap(m_->rr.alt_de, m_->rr.de, t);
	swap(m_->rr.alt_bc, m_->rr.bc, t);
	return clocks;
}

// 3X3
static unsigned ex_de_hl (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u16 t;
	swap(m_->rr.de, m_->rr.hl, t);
	return clocks;
}

// 3X6
static unsigned add_n (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u8 n;
	n = fetch_to_n (m_, &clocks);
	m_->r.a = add8 (m_->r.a, n, &m_->r.f);
	return clocks;
}
static unsigned adc_n (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u8 n;
	n = fetch_to_n (m_, &clocks);
	m_->r.a = adc8 (m_->r.a, n, &m_->r.f);
	return clocks;
}
static unsigned sub_n (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u8 n;
	n = fetch_to_n (m_, &clocks);
	m_->r.a = sub8 (m_->r.a, n, &m_->r.f);
	return clocks;
}
static unsigned sbc_n (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u8 n;
	n = fetch_to_n (m_, &clocks);
	m_->r.a = sbc8 (m_->r.a, n, &m_->r.f);
	return clocks;
}
static unsigned and_n (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u8 n;
	n = fetch_to_n (m_, &clocks);
	m_->r.a = and8 (m_->r.a, n, &m_->r.f);
	return clocks;
}
static unsigned xor_n (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u8 n;
	n = fetch_to_n (m_, &clocks);
	m_->r.a = xor8 (m_->r.a, n, &m_->r.f);
	return clocks;
}
static unsigned or_n (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u8 n;
	n = fetch_to_n (m_, &clocks);
	m_->r.a = or8 (m_->r.a, n, &m_->r.f);
	return clocks;
}
static unsigned cp_n (z80_s *m_, const u8 *p)
{
BUG(m_ && p)
unsigned clocks;
	clocks = M1;
u8 n;
	n = fetch_to_n (m_, &clocks);
	cp8 (m_->r.a, n, &m_->r.f);
	return clocks;
}

static unsigned (*z80_opcode[256]) (z80_s *m_, const u8 *p) = {
	  nop     , ld_rr_nn , ld_rr_a  , inc_rr, inc_r , dec_r , ld_r_n, NULL  
	, ex_af_af, add_hl_rr, ld_a_rr  , dec_rr, inc_r , dec_r , ld_r_n, NULL  
	, djnz    , ld_rr_nn , ld_rr_a  , inc_rr, inc_r , dec_r , ld_r_n, NULL  
	, jr      , add_hl_rr, ld_a_rr  , dec_rr, inc_r , dec_r , ld_r_n, NULL  
	, cond_jr , ld_rr_nn , ld_pnn_hl, inc_rr, inc_r , dec_r , ld_r_n, NULL  
	, cond_jr , add_hl_rr, ld_hl_pnn, dec_rr, inc_r , dec_r , ld_r_n, NULL  
	, cond_jr , ld_rr_nn , ld_pnn_a , inc_rr, inc_r , dec_r , ld_r_n, NULL  
	, cond_jr , add_hl_rr, ld_a_pnn , dec_rr, inc_r , dec_r , ld_r_n, NULL  

	, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r
	, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r
	, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r
	, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r
	, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r
	, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r
	, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, NULL  , ld_r_r
	, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r

	, add_r , add_r , add_r , add_r , add_r , add_r , add_r , add_r
	, adc_r , adc_r , adc_r , adc_r , adc_r , adc_r , adc_r , adc_r
	, sub_r , sub_r , sub_r , sub_r , sub_r , sub_r , sub_r , sub_r
	, sbc_r , sbc_r , sbc_r , sbc_r , sbc_r , sbc_r , sbc_r , sbc_r
	, and_r , and_r , and_r , and_r , and_r , and_r , and_r , and_r 
	, xor_r , xor_r , xor_r , xor_r , xor_r , xor_r , xor_r , xor_r 
	,  or_r ,  or_r ,  or_r ,  or_r ,  or_r ,  or_r ,  or_r ,  or_r 
	,  cp_r ,  cp_r ,  cp_r ,  cp_r ,  cp_r ,  cp_r ,  cp_r ,  cp_r 

	, NULL  , NULL  , NULL  , NULL    , NULL  , NULL  , add_n , NULL  
	, NULL  , NULL  , NULL  , NULL    , NULL  , NULL  , adc_n , NULL  
	, NULL  , NULL  , NULL  , NULL    , NULL  , NULL  , sub_n , NULL  
	, NULL  , exx   , NULL  , NULL    , NULL  , NULL  , sbc_n , NULL  
	, NULL  , NULL  , NULL  , NULL    , NULL  , NULL  , and_n , NULL  
	, NULL  , NULL  , NULL  , ex_de_hl, NULL  , NULL  , xor_n , NULL  
	, NULL  , NULL  , NULL  , NULL    , NULL  , NULL  ,  or_n , NULL  
	, NULL  , NULL  , NULL  , NULL    , NULL  , NULL  ,  cp_n , NULL  
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
#endif //ndef Ei386

bool z80_init (struct z80 *this_, size_t cb)
{
BUG(this_)
BUG(sizeof(z80_s) <= cb)
	memset (this_, 0, sizeof(z80_s));
}
