#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include "z80.h"
#include "z80priv.h"

#include "assert.h"
#include "portab.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

typedef uint32_t u32;
typedef  uint8_t u8;
typedef uint16_t u16;
typedef   int8_t s8;

///////////////////////////////////////////////////////////////////////////////
// recycle

static unsigned tohex (int c)
{
	if ('0' <= c && c <= '9')
		return (unsigned)(c - '0');
	else if ('A' <= c && c <= 'F')
		return (unsigned)(c - 'A' +10);
	else if ('a' <= c && c <= 'f')
		return (unsigned)(c - 'a' +10);
	return (unsigned)-1;
}

///////////////////////////////////////////////////////////////////////////////
// recycle reserved

static size_t z80a_disasm_tostr (const void *src_, u16 pc, char *dst, size_t cb)
{
static const char *cond[] = {
	"NZ", "Z", "NC", "C", "PO", "PE", "P", "M"
};
static const char *r8[] = {
	"B", "C", "D", "E", "H", "L", "(HL)", "A"
};
static const char *r16[] = {
	"BC", "DE", "HL", "SP"
};
static const char *r16b[] = {
	"BC", "DE", "HL", "AF"
};
static const char *regop[] = {
	"ADD", "ADC", "SUB", "SBC", "AND", "XOR", "OR", "CP"
};
static const char *regop8[] = {
	"A,", "A,", "", "A,", "", "", "", ""
};
static const char *op0X7[] = {
	"RLCA", "RRCA", "RLA", "RRA", "DAA", "CPL", "SCF", "CCF"
};
BUG(src_ && (NULL == dst || 4 +1 +9 +1 +9 < cb)) // 9 = len('[IX-0FFh]')
const u8 *src, *src0;
	src = src0 = (const u8 *)src_;
//char *dst_end;
//	dst_end = dst + cb;
	*dst = '\0';
int x, y, z;
	x = 3 & *src >> 6, y = 7 & *src >> 3, z = 7 & *src;
u8 n;
u16 nn;
	switch (x) {
	case 0:
		switch (z) {
		case 0: // 0X0
			switch (y) {
			case 0:
				if (dst)
					sprintf (dst, "%-4s", "NOP");
				break;
			case 1:
				if (dst)
					sprintf (dst, "%-4s %s,%s'", "EX", r16b[3], r16b[3]);
				break;
			case 2:
			case 3:
				n = *++src;
				nn = (u16)(pc +2 + (s8)n);
				if (dst)
					sprintf (dst, "%-4s %s%04Xh", (2 == y) ? "DJNZ" : "JR", (0x9FFF < nn) ? "0" : "", nn);
				break;
			default:
				n = *++src;
				nn = (u16)(pc +2 + (s8)n);
				if (dst)
					sprintf (dst, "%-4s %s,%s%04Xh", "JR", cond[y -4], (0x9FFF < nn) ? "0" : "", nn);
				break;
			}
			break;
		case 1: // 0X1
			if (! (1 & y)) {
				nn = load_le16 (++src);
				++src;
				if (dst)
					sprintf (dst, "%-4s %s,%s%04Xh", "LD", r16[y >> 1], (0x9FFF < nn) ? "0" : "", nn);
			}
			else {
				if (dst)
					sprintf (dst, "%-4s %s,%s", "ADD", "HL", r16[y >> 1]);
			}
			break;
		case 2: // 0X2
			if (! (4 & y)) {
				if (! (1 & y)) {
					if (dst)
						sprintf (dst, "%-4s (%s),%s", "LD", r16[y >> 1], "A");
				}
				else {
					if (dst)
						sprintf (dst, "%-4s %s,(%s)", "LD", "A", r16[y >> 1]);
				}
			}
			else {
				nn = load_le16 (++src);
				++src;
				if (! (1 & y)) {
					if (dst)
						sprintf (dst, "%-4s (%s%04Xh),%s", "LD", (0x9FFF < nn) ? "0" : "", nn, !(2 & y) ? "HL" : "A");
				}
				else {
					if (dst)
						sprintf (dst, "%-4s %s,(%s%04Xh)", "LD", !(2 & y) ? "HL" : "A", (0x9FFF < nn) ? "0" : "", nn);
				}
			}
			break;
		case 3: // 0X3
			if (dst)
				sprintf (dst, "%-4s %s", (1 & y) ? "DEC" : "INC", r16[y >> 1]);
			break;
		case 4: // 0X4
			if (dst)
				sprintf (dst, "%-4s %s", "INC", r8[y]);
			break;
		case 5: // 0X5
			if (dst)
				sprintf (dst, "%-4s %s", "DEC", r8[y]);
			break;
		case 6: // 0X6
			n = *++src;
			if (dst)
				sprintf (dst, "%-4s %s,%s%02Xh", "LD", r8[y], (0x9F < n) ? "0" : "", n);
			break;
		case 7: // 0X7
			if (dst)
				sprintf (dst, "%-4s", op0X7[y]);
			break;
		}
		break;
	case 1:
		if (6 == y && 6 == z) {
			if (dst)
				strcpy (dst, "HALT");
			break;
		}
		if (dst)
			sprintf (dst, "%-4s %s,%s", "LD", r8[y], r8[z]);
		break;
	case 2:
		if (dst)
			sprintf (dst, "%-4s %s%s", regop[y], regop8[y], r8[z]);
		break;
	case 3:
		switch (z) {
		case 1: // 3X1
			if (0 == (1 & y)) {
				if (dst)
					sprintf (dst, "%-4s %s", "POP", r16b[y >> 1]);
			}
			if (3 == y) {
				if (dst)
					strcpy (dst, "EXX");
			}
			else if (5 == y) {
				if (dst)
					sprintf (dst, "%-4s %s", "JP", "(HL)");
			}
			else if (7 == y) {
				if (dst)
					sprintf (dst, "%-4s %s,%s", "LD", r16[3], r16[2]);
			}
			break;
		case 2: // 3X2
			nn = load_le16 (++src);
			++src;
			if (dst)
				sprintf (dst, "%-4s %s,%s%04Xh", "JP", cond[y], (0x9FFF < nn) ? "0" : "", nn);
			break;
		case 3: // 3X3
			if (0 == y) {
				nn = load_le16 (++src);
				++src;
				if (dst)
					sprintf (dst, "%-4s %s%04Xh", "JP", (0x9FFF < nn) ? "0" : "", nn);
			}
			else if (5 == y) {
				if (dst)
					sprintf (dst, "%-4s %s,%s", "EX", r16[1], r16[2]);
			}
			break;
		case 6: // 3X6
			n = *++src;
			if (dst)
				sprintf (dst, "%-4s %s%s%02Xh", regop[y], regop8[y], (0x9F < n) ? "0" : "", n);
			break;
		case 0: // 3X0
		case 4: // 3X4
		case 5: // 3X5
		case 7: // 3X7
			break;
		}
		break;
	}
	return src +1 - src0;
}

///////////////////////////////////////////////////////////////////////////////
// cannot recycle

static void hello_world_test (z80_s *m_)
{
static u8 hello_world_z80[] = {
	  0x21, 0x00, 0xA0, 0x3E, 0x48, 0x77, 0x2C, 0x36 // 533 53 53 5 5-
	, 0x65, 0x23, 0x3E, 0x6C, 0x77, 0x2C, 0x36, 0x6C // 33 7 53 53 5 533
	, 0x23, 0x3E, 0x6F, 0x77, 0x2C, 0x36, 0x20, 0x23 // 7 53 53 5 533 7
	, 0x3E, 0x77, 0x77, 0x2C, 0x36, 0x6F, 0x23, 0x3E // 53 53 5 533 7 5-
	, 0x72, 0x77, 0x2C, 0x36, 0x6C, 0x23, 0x3E, 0x64 // 3 53 5 533 7 53
	, 0x77, 0x2C, 0x36, 0x21, 0x23, 0xAF, 0x77       // 53 5 533 7 5 53
}; // (258 states)

u8 mem0[0x30], mem5[0x2000];
BUG(sizeof(hello_world_z80) <= sizeof(mem0))
	memcpy (mem0, hello_world_z80, sizeof(hello_world_z80));
	memset (mem5, 0, sizeof(mem5));
unsigned executed;
	m_->mem[0].raw_or_rwfn = mem0, m_->mem[0].rwfn_priv = 0;
	m_->mem[5].raw_or_rwfn = mem5, m_->mem[5].rwfn_priv = 0;
	m_->pc = 0x0000;
	executed = z80_exec ((struct z80 *)m_, 218 +Tw * 32 +1); // CLK(0000-002E) +1
BUG(225 +Tw * 33 == executed)
BUG(0x002F == m_->pc)
BUG(0 == strcmp ("Hello world!", (const char *)mem5))
	fprintf (stderr, VTGG "%s" VTO "\n", (const char *)mem5);
}

static size_t changed_flags_tostr (z80_s *m_, const z80_s *old, char *dst, size_t cb)
{
BUG(m_ && dst && 1 +8 < cb)
char *begin/*, *end*/;
	begin = dst/*, end = dst + cb*/;
u8 old_f;
	old_f = (NULL == old) ? 0x00 : old->r.f;

u8 mask;
	for (mask = 0x80; 0 < mask; mask >>= 1) {
		if (mask & (old_f ^ m_->r.f))
			*dst++ = (mask & m_->r.f) ? '1' : '0';
		else
			*dst++ = '-';
	}
	return dst - begin;
}
#define SP_DEFAULT 0xA0F0
static size_t changed_reg_tostr (z80_s *m_, const z80_s *old, const char *sep, char *dst, size_t cb)
{
BUG(m_ && sep && dst && 0 < cb)
char *begin, *end;
	begin = dst, end = dst + cb;
	*dst = '\0';
const char *prefix;
int changed;
	// SP
BUG(dst +4 +4 < end)
	changed  = (old && old->rr.sp == m_->rr.sp || NULL == old && SP_DEFAULT == m_->rr.sp) ? 0 : 3;
	changed |= (old && (u16)(m_->rr.sp +8 - old->rr.sp) < 8) ? 4 : 0;
	changed |= (old && (u16)(m_->rr.sp    - old->rr.sp) < 8) ? 8 : 0;
	prefix = ('\0' == *begin) ? "" : sep;
	if (11 == changed)
		sprintf (dst, "%s" "SP=" "+%d", prefix, m_->rr.sp - old->rr.sp);
	else if (7 == changed)
		sprintf (dst, "%s" "SP=" "-%d", prefix, old->rr.sp - m_->rr.sp);
	else if (3 == changed)
		sprintf (dst, "%s" "SP=" "%04X", prefix, m_->rr.sp);
	dst = strchr (dst, '\0');
	// BC'
BUG(dst +5 +4 < end)
	changed  = (old && old->rr.alt_bc == m_->rr.alt_bc || NULL == old && 0 == m_->rr.alt_bc) ? 0 : 3;
	prefix = ('\0' == *begin) ? "" : sep;
	if (3 == changed)
		sprintf (dst, "%s" "BC'=" "%04X", prefix, m_->rr.alt_bc);
	dst = strchr (dst, '\0');
	// DE'
BUG(dst +5 +4 < end)
	changed  = (old && old->rr.alt_de == m_->rr.alt_de || NULL == old && 0 == m_->rr.alt_de) ? 0 : 3;
	prefix = ('\0' == *begin) ? "" : sep;
	if (3 == changed)
		sprintf (dst, "%s" "DE'=" "%04X", prefix, m_->rr.alt_de);
	dst = strchr (dst, '\0');
	// HL'
BUG(dst +5 +4 < end)
	changed  = (old && old->rr.alt_hl == m_->rr.alt_hl || NULL == old && 0 == m_->rr.alt_hl) ? 0 : 3;
	prefix = ('\0' == *begin) ? "" : sep;
	if (3 == changed)
		sprintf (dst, "%s" "HL'=" "%04X", prefix, m_->rr.alt_hl);
	dst = strchr (dst, '\0');
	// AF'
BUG(dst +5 +4 < end)
	changed  = (old && old->rr.alt_af == m_->rr.alt_af || NULL == old && 0 == m_->rr.alt_af) ? 0 : 3;
	prefix = ('\0' == *begin) ? "" : sep;
	if (3 == changed)
		sprintf (dst, "%s" "AF'=" "%04X", prefix, m_->rr.alt_af);
	dst = strchr (dst, '\0');
	// BC
BUG(dst +3 +2 +3 +2 < end)
	changed  = (old && old->r.c == m_->r.c || NULL == old && 0 == m_->r.c) ? 0 : 1;
	changed |= (old && old->r.b == m_->r.b || NULL == old && 0 == m_->r.b) ? 0 : 2;
	prefix = ('\0' == *begin) ? "" : sep;
	if (3 == changed)
		sprintf (dst, "%s" "BC=" "%04X", prefix, m_->rr.bc);
	else if (2 == changed)
		sprintf (dst, "%s" "B=" "%02X", prefix, m_->r.b);
	else if (1 == changed)
		sprintf (dst, "%s" "C=" "%02X", prefix, m_->r.c);
	dst = strchr (dst, '\0');
	// DE
BUG(dst +3 +2 +3 +2 < end)
	changed  = (old && old->r.e == m_->r.e || NULL == old && 0 == m_->r.e) ? 0 : 1;
	changed |= (old && old->r.d == m_->r.d || NULL == old && 0 == m_->r.d) ? 0 : 2;
	prefix = ('\0' == *begin) ? "" : sep;
	if (3 == changed)
		sprintf (dst, "%s" "DE=" "%04X", prefix, m_->rr.de);
	else if (2 == changed)
		sprintf (dst, "%s" "D=" "%02X", prefix, m_->r.d);
	else if (1 == changed)
		sprintf (dst, "%s" "E=" "%02X", prefix, m_->r.e);
	dst = strchr (dst, '\0');
	// HL
BUG(dst +3 +2 +3 +2 < end)
	changed  = (old && old->r.l == m_->r.l || NULL == old && 0 == m_->r.l) ? 0 : 1;
	changed |= (old && old->r.h == m_->r.h || NULL == old && 0 == m_->r.h) ? 0 : 2;
	prefix = ('\0' == *begin) ? "" : sep;
	if (3 == changed)
		sprintf (dst, "%s" "HL=" "%04X", prefix, m_->rr.hl);
	else if (2 == changed)
		sprintf (dst, "%s" "H=" "%02X", prefix, m_->r.h);
	else if (1 == changed)
		sprintf (dst, "%s" "L=" "%02X", prefix, m_->r.l);
	dst = strchr (dst, '\0');
	// A
BUG(dst +3 +2 < end)
	changed  = (old && old->r.a == m_->r.a || NULL == old && 0 == m_->r.a) ? 0 : 1;
	prefix = ('\0' == *begin) ? "" : sep;
	if (1 == changed)
		sprintf (dst, "%s" "A=" "%02X", prefix, m_->r.a);
	dst = strchr (dst, '\0');
	return dst - begin;
}
#define SEP "." // for 'sort -kN'
static void each_opcode_test (z80_s *m_, const char *test_text_path)
{
FILE *fp;
	if (NULL == (fp = fopen (test_text_path, "r")))
		return;
#ifdef Ei386
	// native emulation tag
puts ("i386 native emulation.");
#endif //def Ei386
size_t line;
	line = 0;
	while (!feof (fp)) {
char text[256], *text_end;
	text_end = text + sizeof(text);
	// test-code input
		if (NULL == (fgets (text, text_end - text, fp)))
			break;
		text_end[-1] = '\0';
char *tail;
		tail = strchr (text, '\0');
		while (text < tail && strchr ("\r\n", tail[-1]))
			*--tail = '\0';
		if ('#' == *text)
			continue;
		memset (m_->r16, 0, sizeof(m_->r16));
		m_->pc = 0;
	// caption output
		if (1 == ++line % 32)
printf ("# %13s SZ-H-PNC %4s SZ-H-PNC CLK  %-19s %-8s (disasm)" "\n", "(INITIAL)", "", "(RESULT)", "CODE");
		// SZ-H-PNC
char *p;
		p = text;
		while (strchr (" \t", *p))
			++p;
		while (strchr ("01", *p))
			m_->r.f = m_->r.f << 1 | (u8)(*p++ - '0');
		// INIT [A000-BFFF] <- AA
u8 mem5[0x2000];
		memset (mem5, 0xAA, sizeof(mem5));
		// CODE [B000-]
		while (strchr (" \t", *p))
			++p;
char *q;
		q = p + strspn (p, "0123456789ABCDEFabcdef");
u16 addr;
		for (addr = 0x1000; p +1 < q; p += 2, ++addr)
			mem5[addr] = tohex (p[0]) << 4 | tohex (p[1]);
		p = q;
		m_->rr.sp = SP_DEFAULT;
		// TEST DATA [A000-A0FF] <- 00..FF
		for (addr = 0x0000; addr < 0x0100; ++addr)
			mem5[addr] = 255 - (u8)addr;
		// BC/DE/HL/SP/A
		while (p < tail) {
			while (strchr (" \t", *p))
				++p;
			if (memchr ("\0" "#", *p, 2))
				break;
			// B/C/D/E/H/L/A
			if ('=' == p[1]) {
u8 n;
				n = (u8)strtol (p +2, &q, 16);
				switch (*p) {
				case 'B': m_->r.b = n; break;
				case 'C': m_->r.c = n; break;
				case 'D': m_->r.d = n; break;
				case 'E': m_->r.e = n; break;
				case 'H': m_->r.h = n; break;
				case 'L': m_->r.l = n; break;
				case 'A': m_->r.a = n; break;
				}
				p = q;
			}
			// BC/DE/HL/SP
			else if ('=' == p[2]) {
u16 nn;
				nn = (u16)strtol (p +3, &q, 16);
				if (0 == memcmp ("BC", p, 2))
					m_->rr.bc = nn;
				else if (0 == memcmp ("DE", p, 2))
					m_->rr.de = nn;
				else if (0 == memcmp ("HL", p, 2))
					m_->rr.hl = nn;
				else if (0 == memcmp ("SP", p, 2))
					m_->rr.sp = nn;
				p = q;
			}
			// BC'/DE'/HL'/AF'
			else if ('=' == p[3]) {
u16 nn;
				nn = (u16)strtol (p +4, &q, 16);
				if (0 == memcmp ("BC'", p, 3))
					m_->rr.alt_bc = nn;
				else if (0 == memcmp ("DE'", p, 3))
					m_->rr.alt_de = nn;
				else if (0 == memcmp ("HL'", p, 3))
					m_->rr.alt_hl = nn;
				else if (0 == memcmp ("AF'", p, 3))
					m_->rr.alt_af = nn;
				p = q;
			}
			else
				break;
		}
		m_->mem[5].raw_or_rwfn = mem5, m_->mem[5].rwfn_priv = 0;
		m_->pc = 0xB000;
z80_s old;
		memcpy (&old, m_, sizeof(z80_s));
u16 pc_before_exec;
		pc_before_exec = m_->pc;
unsigned executed;
		executed = z80_exec ((struct z80 *)m_, 1);
	// (executed bytes)
u8 *opcode;
		opcode = &mem5[0x1000];
size_t cb_executed;
		cb_executed = 4;
		if (0x00 == (0xc7 & opcode[0]) && 0x08 < opcode[0]) // DJNZ JR
			cb_executed = 2;
		else if (0xc0 == (0xc7 & opcode[0]) || 0xc9 == opcode[0]) // RET
			cb_executed = 1;
		else if (0xc2 == (0xc7 & opcode[0]) || 0xc3 == opcode[0]) // JP
			cb_executed = 3;
		else if (0xc4 == (0xc7 & opcode[0]) || 0xcd == opcode[0]) // CALL
			cb_executed = 3;
		else if (0xB000 < m_->pc && m_->pc < 0xB004)
			cb_executed = m_->pc - 0xB000;
	// result output
char *dst;
		dst = text;
size_t cb;
		// A BC DE HL SP ... [before]
		cb = changed_reg_tostr (&old, NULL, SEP, dst, text_end - dst);
		if (cb < 2) {
			strcpy (dst, " ."); // for 'sort -kN'
			cb = strlen (dst);
		}
		if (cb < 15) {
			memmove (dst +15 - cb, dst, cb);
			memset (dst, ' ', 15 - cb);
			cb = 15;
		}
		dst += cb;
		// SZ-H-PNC [before]
		*dst++ = ' ';
		dst += changed_flags_tostr (&old, NULL, dst, text_end - dst);
		// 
		strcpy (dst, "  => ");
		dst = strchr (dst, '\0');
		// SZ-H-PNC
		*dst++ = ' ';
		dst += changed_flags_tostr (m_, &old, dst, text_end - dst);
		// CLK
		*dst++ = ' ';
		sprintf (dst, "%3d", executed);
		dst = strchr (dst, '\0');
		cb = 0;
		strcpy (dst, "  ");
		dst = strchr (dst, '\0');
		// A BC DE HL SP ...
		cb += changed_reg_tostr (m_, &old, SEP, dst +cb, text_end - (dst +cb));
		// PC
		if (! (0xB000 < m_->pc && m_->pc <= 0xB004 && 0xB000 + cb_executed == m_->pc)) {
			if (0 < cb)
				dst[cb++] = SEP[0];
			sprintf (dst +cb, "PC=%04X", m_->pc);
			cb += strlen (dst +cb);
		}
		// TEST DATA [A000-A0FF]
		for (addr = 0x0000; addr < 0x0100; ++addr) {
			if (255 - (u8)addr == mem5[addr])
				continue;
			if (0 < cb)
				dst[cb++] = SEP[0];
			if (addr +1 < 0x0100 && !(255 - (u8)(addr +1) == mem5[addr +1])) {
				sprintf (dst +cb, "[%04X,2]=%02X%02X->%04X", 0xA000 + addr, 255 - (u8)(addr +1), 255 - (u8)addr, load_le16 (mem5 + addr));
				++addr;
			}
			else
				sprintf (dst +cb, "[%04X]=%02X->%02X", 0xA000 + addr, 255 - (u8)addr, *(mem5 + addr));
			cb += strlen (dst +cb);
		}
		if (cb < 2) {
			cb = 0;
			dst[cb++] = '.'; // for 'sort -kN'
		}
		if (cb < 19) {
			memset (dst +cb, ' ', 19 - cb);
			cb = 19;
		}
		dst += cb;
		// CODE
		*dst++ = ' ';
size_t i;
		for (i = 0; i < 4; ++i) {
			if (i < cb_executed)
				sprintf (dst, "%02X", opcode[i]);
			else
				strcpy (dst, "  ");
			dst += 2;
		}
		// mnemonic
		*dst++ = ' ';
		z80a_disasm_tostr (mem5 +0x1000, pc_before_exec, dst, text_end - dst);
		dst = strchr (dst, '\0');
		//
		*dst = '\0';
puts (text);
	}
	fclose (fp);
}

int main (int argc, char *argv[])
{
int test_number;
	test_number = (1 < argc) ? atoi (argv[1]) : 0;

struct z80 z80;
	z80_init (&z80, sizeof(z80));
z80_s *m_;
	m_ = (z80_s *)&z80;

	// memory layout debug
BUG(&m_->rr.de == &m_->r16[RR2I[1]])
BUG(&m_->rr.sp == &m_->r16[RR2I[3]])
#if BYTE_ORDER == LITTLE_ENDIAN
BUG(&m_->rr.de == (u16 *)&m_->r.e)
#else
BUG(&m_->rr.de == (u16 *)&m_->r.d)
#endif
	switch (test_number) {
	case 0:
		hello_world_test (m_);
		break;
	case 1:
		each_opcode_test (m_, "./z80.c" ".test.in");
		break;
	}
	return 0;
}
