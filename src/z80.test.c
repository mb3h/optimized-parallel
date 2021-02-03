#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include "z80.h"
#include "z80priv.h"

#include "assert.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

typedef uint32_t u32;
typedef  uint8_t u8;
typedef uint16_t u16;

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
static void each_opcode_test (z80_s *m_)
{
FILE *fp;
	if (NULL == (fp = fopen ("./z80.c" ".test.in", "r")))
		return;
size_t line;
	line = 0;
	while (!feof (fp)) {
char text[128];
	// test-code input
		if (NULL == (fgets (text, sizeof(text), fp)))
			break;
		text[sizeof(text) -1] = '\0';
char *tail;
		tail = strchr (text, '\0');
		while (text < tail && strchr ("\r\n", tail[-1]))
			*--tail = '\0';
		if ('#' == *text)
			continue;
	// caption output
		if (1 == ++line % 32)
	puts ("# " "SZ-H-PNC CLK CODE     CHANGED");
		// SZ-H-PNC
char *p;
		p = text;
		while (strchr (" \t", *p))
			++p;
		m_->r.f = 0;
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
		// TEST DATA [A000-A0FF] <- 00..FF
		for (addr = 0x0000; addr < 0x0100; ++addr)
			mem5[addr] = 255 - (u8)addr;
		// BC/DE/HL/A
		while (p < tail) {
			while (strchr (" \t", *p))
				++p;
			if (memchr ("\0" "#", *p, 2))
				break;
			if ('=' == p[2]) {
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
			else if ('=' == p[1]) {
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
			else
				break;
		}
		m_->mem[5].raw_or_rwfn = mem5, m_->mem[5].rwfn_priv = 0;
		m_->pc = 0xB000;
z80_s old;
		memcpy (&old, m_, sizeof(z80_s));
unsigned executed;
		executed = z80_exec ((struct z80 *)m_, 1);
	// result output
char *dst;
		dst = text;
		// SZ-H-PNC
		strcpy (dst, "  ");
		dst = strchr (dst, '\0');
u8 mask;
		for (mask = 0x80; 0 < mask; mask >>= 1) {
			if (mask & (old.r.f ^ m_->r.f))
				*dst++ = (mask & m_->r.f) ? '1' : '0';
			else
				*dst++ = '-';
		}
		// CLK
		*dst++ = ' ';
		sprintf (dst, "%3d", executed);
		dst = strchr (dst, '\0');
		// CODE
		*dst++ = ' ';
		p = q = &mem5[0x1000];
size_t cb;
		cb = (0xB000 < m_->pc && m_->pc < 0xB004) ? m_->pc - 0xB000 : 4;
size_t i;
		for (i = 0; i < 4; ++i) {
			if (i < cb)
				sprintf (dst, "%02X", mem5[0x1000 +i]);
			else
				strcpy (dst, "  ");
			dst += 2;
		}
		// PC
		if (! (0xB000 < m_->pc && m_->pc <= 0xB004)) {
			sprintf (dst, " PC=%04X", m_->pc);
			dst = strchr (dst, '\0');
		}
		// A
		if (! (old.r.a == m_->r.a)) {
			sprintf (dst, " A=%02X", m_->r.a);
			dst = strchr (dst, '\0');
		}
		// BC
		if (!(old.r.b == m_->r.b || old.r.c == m_->r.c)) {
			sprintf (dst, " BC=%04X", m_->rr.bc);
			dst = strchr (dst, '\0');
		}
		else {
			if (! (old.r.b == m_->r.b)) {
				sprintf (dst, " B=%02X", m_->r.b);
				dst = strchr (dst, '\0');
			}
			if (! (old.r.c == m_->r.c)) {
				sprintf (dst, " C=%02X", m_->r.c);
				dst = strchr (dst, '\0');
			}
		}
		// DE
		if (!(old.r.d == m_->r.d || old.r.e == m_->r.e)) {
			sprintf (dst, " DE=%04X", m_->rr.de);
			dst = strchr (dst, '\0');
		}
		else {
			if (! (old.r.d == m_->r.d)) {
				sprintf (dst, " D=%02X", m_->r.d);
				dst = strchr (dst, '\0');
			}
			if (! (old.r.e == m_->r.e)) {
				sprintf (dst, " E=%02X", m_->r.e);
				dst = strchr (dst, '\0');
			}
		}
		// HL
		if (!(old.r.h == m_->r.h || old.r.l == m_->r.l)) {
			sprintf (dst, " HL=%04X", m_->rr.hl);
			dst = strchr (dst, '\0');
		}
		else {
			if (! (old.r.h == m_->r.h)) {
				sprintf (dst, " H=%02X", m_->r.h);
				dst = strchr (dst, '\0');
			}
			if (! (old.r.l == m_->r.l)) {
				sprintf (dst, " L=%02X", m_->r.l);
				dst = strchr (dst, '\0');
			}
		}
		// SP
		if (! (old.rr.sp == m_->rr.sp)) {
			sprintf (dst, " SP=%04X", m_->rr.sp);
			dst = strchr (dst, '\0');
		}
		// TEST DATA [A000-A0FF]
		for (addr = 0x0000; addr < 0x0100; ++addr) {
			if (255 - (u8)addr == mem5[addr])
				continue;
			if (addr +1 < 0x0100 && !(255 - (u8)(addr +1) == mem5[addr +1]))
				sprintf (dst, " [%04X,2]=%04X", 0xA000 + addr, mem5[addr +1] << 8 | mem5[addr]);
			else
				sprintf (dst, " [%04X]=%02X", 0xA000 + addr, mem5[addr]);
			dst = strchr (dst, '\0');
		}
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
		each_opcode_test (m_);
		break;
	}
	return 0;
}
