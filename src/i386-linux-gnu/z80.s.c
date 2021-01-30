#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include "z80.h"
#include "z80priv.h"

#ifndef Tw
# define Tw "0"
#elif 1 == Tw
# undef Tw
# define Tw "1"
#else
# error "unexpected Tw macro."
#endif

#define LF "\n"
#define NL LF "\t"
#define LC ".L"
#define OP ".L"
#define M ".L"

__asm__ (
	".struct 0" LF
M "reg_bc:" LF
M "reg_c:" NL
	".struct " M "reg_c +1" LF
M "reg_b:" NL
	".struct " M "reg_b +1" LF
M "reg_de:" LF
M "reg_e:" NL
	".struct " M "reg_e +1" LF
M "reg_d:" NL
	".struct " M "reg_d +1" LF
M "reg_hl:" LF
M "reg_l:" NL
	".struct " M "reg_l +1" LF
M "reg_h:" NL
	".struct " M "reg_h +1" LF
M "reg_af:" LF
M "reg_f:" NL
	".struct " M "reg_f +1" LF
M "reg_a:" NL
	".struct " M "reg_a +1" LF
);

#define CPU           "%esi"
#define B   M "reg_b(" CPU ")"
#define C   M "reg_c(" CPU ")"
#define D   M "reg_d(" CPU ")"
#define E   M "reg_e(" CPU ")"
#define H   M "reg_h(" CPU ")"
#define L   M "reg_l(" CPU ")"
#define A   M "reg_a(" CPU ")"

#define EAPC          "%edi"
#define U             "%dh"
#define V             "%dl"
#define UV            "%edx"


#define R2M(src, dst) \
	"mov " src "," dst NL

#define CLK1(clocks, cycles) \
	"add $" #clocks " +" Tw ",%ebp" NL

#define OPFUNC(label) \
	__asm__ ( \
		".text" NL \
		".type " OP #label ",@function" LF \
	OP #label ":" NL \
		".cfi_startproc" NL
#define OPEND(label) \
		"ret" NL \
		".cfi_endproc" NL \
		".size " LC #label ",.-" LC #label NL \
	);

#define EAPC2V(label) \
	/* TODO: mem[] */ \
	"movzbl (" EAPC ")," UV NL \
	"inc " EAPC NL

OPFUNC(NOP) CLK1(4,1) OPEND(NOP)

OPFUNC(LD_B_N) EAPC2V(LD_B_N) R2M(V,B) CLK1(7,2) OPEND(LD_B_N)
OPFUNC(LD_C_N) EAPC2V(LD_C_N) R2M(V,C) CLK1(7,2) OPEND(LD_C_N)
OPFUNC(LD_D_N) EAPC2V(LD_D_N) R2M(V,D) CLK1(7,2) OPEND(LD_D_N)
OPFUNC(LD_E_N) EAPC2V(LD_E_N) R2M(V,E) CLK1(7,2) OPEND(LD_E_N)
OPFUNC(LD_H_N) EAPC2V(LD_H_N) R2M(V,H) CLK1(7,2) OPEND(LD_H_N)
OPFUNC(LD_L_N) EAPC2V(LD_L_N) R2M(V,L) CLK1(7,2) OPEND(LD_L_N)
OPFUNC(LD_A_N) EAPC2V(LD_A_N) R2M(V,A) CLK1(7,2) OPEND(LD_A_N)

__asm__ (
	".section .rodata" NL
	".type " LC "z80_opcode" ",@object" LF
LC "z80_opcode:" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "LD_B_N," OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "LD_C_N," OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "LD_D_N," OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "LD_E_N," OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "LD_H_N," OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "LD_L_N," OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP,"    OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "LD_A_N," OP "NOP" NL

	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP" NL

	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP" NL

	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP" NL
	".long " OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP," OP "NOP" NL

	".size " LC "z80_opcode" ",.-" LC "z80_opcode" NL
);
