#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include "z80.h"
#include "z80priv.h"

#ifndef Tw
# define Tw "0"
#else
# if 0 == Tw
#  undef Tw
#  define Tw "0"
# elif 1 == Tw
#  undef Tw
#  define Tw "1"
# else
#  error "unexpected Tw macro."
# endif
#endif

#define LF "\n"
#define NL LF "\t"
#define LC ".L"
#define OP ".L"
#define M ".L"

#if 16 == RRm
# define sizeofPAD "0"
#else //if 32 == RRm
# define sizeofPAD "2"
#endif

__asm__ (
	".struct 0" LF
M "reg_bc:" LF
M "reg_c:" NL
	".struct " M "reg_c +1" LF
M "reg_b:" NL
	".struct " M "reg_b +1 +" sizeofPAD LF
M "reg_de:" LF
M "reg_e:" NL
	".struct " M "reg_e +1" LF
M "reg_d:" NL
	".struct " M "reg_d +1 +" sizeofPAD LF
M "reg_hl:" LF
M "reg_l:" NL
	".struct " M "reg_l +1" LF
M "reg_h:" NL
	".struct " M "reg_h +1 +" sizeofPAD LF
M "reg_af:" LF
M "reg_f:" NL
	".struct " M "reg_f +1" LF
M "reg_a:" NL
	".struct " M "reg_a +1 +" sizeofPAD LF
M "reg_sp:" LF
	".struct " M "reg_sp +2 +" sizeofPAD LF
);

#if 16 == RRm
# define ECX          "%cx"
# define EDX          "%dx"
#else //if 32 == RRm
# define ECX          "%ecx"
# define EDX          "%edx"
#endif

#define EAPC          "%edi"
#define CPU           "%esi"
#define B   M "reg_b(" CPU ")"
#define C   M "reg_c(" CPU ")"
#define D   M "reg_d(" CPU ")"
#define E   M "reg_e(" CPU ")"
#define H   M "reg_h(" CPU ")"
#define L   M "reg_l(" CPU ")"
#define A   M "reg_a(" CPU ")"
#define BC  M "reg_bc(" CPU ")"
#define DE  M "reg_de(" CPU ")"
#define HL  M "reg_hl(" CPU ")"
#define SP  M "reg_sp(" CPU ")"

#define CF "0x01"
#define NF "0x02"
#define VF "0x04"
//#define PF VF
#define HF "0x10"
#define ZF "0x40"
#define SF "0x80"
	/* cf)          SZ-H-PNC (Z80)
	                     /
	                     V
	       -NIIODIT SZ-A-P-C (i386)
	        TOO
	         PP
	         LL */

#define CLK1(clocks, cycles) \
	"add $" #clocks " +" Tw ",%ebp" NL
#define dl2ST(dst) \
	"mov " "%dl" "," dst NL
#define LD2dl(src) \
	"mov " src "," "%dl" NL
#define LD2cx(src) \
	"mov " src "," ECX NL
#define LD2dx(src) \
	"mov " src "," EDX NL
#define dx2ST(dst) \
	"mov " EDX "," dst NL

#define LCFUNC(label) \
	__asm__ ( \
		".text" NL \
		".type " LC #label ",@function" LF \
	LC #label ":" NL \
		".cfi_startproc" NL
#define LCEND(label) \
		"ret" NL \
		".cfi_endproc" NL \
		".size " LC #label ",.-" LC #label NL \
	);

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

#define dl2WRITEcx \
	"call " LC "dl2WRITEcx" NL
#define cxREAD2dl \
	"call " LC "cxREAD2dl" NL

#define FETCH2dl(label) \
	/* TODO: mem[] */ \
	"movzbl (" EAPC ")," "%edx" NL \
	"inc " EAPC NL
#define FETCH2dx(label) \
	/* TODO: mem[] */ \
	"add $2," EAPC NL \
	"movzwl -2(" EAPC ")," "%edx" NL

LCFUNC(cxREAD2dl)
	/* TODO: */
LCEND(cxREAD2dl)

LCFUNC(dl2WRITEcx)
	/* TODO: */
LCEND(dl2WRITEcx)

OPFUNC(NOP) CLK1(4,1) OPEND(NOP) // (4+Tw)

#define SET2ah(mask) \
	"or $(" mask "),%ah" NL
#define RES2ah(mask) \
	"and $(0xff - (" mask ")),%ah" NL
#define eflagsOF2cl2ah \
	"seto %cl" NL RES2ah(VF "+" NF) "shl $2,%cl" NL "or %cl,%ah" NL
#define ah2eflags \
	"sahf" NL
#define eflags2ah \
	"lahf" NL
#define ah2INCdl \
	ah2eflags "inc %dl" NL eflags2ah eflagsOF2cl2ah
#define ah2DECdl \
	ah2eflags "dec %dl" NL eflags2ah eflagsOF2cl2ah SET2ah(NF)

OPFUNC(INC_BC) LD2dx(BC) "inc " EDX NL CLK1(6,1) dx2ST(BC) OPEND(INC_BC) // (6+Tw)
OPFUNC(INC_DE) LD2dx(DE) "inc " EDX NL CLK1(6,1) dx2ST(DE) OPEND(INC_DE)
OPFUNC(INC_HL) LD2dx(HL) "inc " EDX NL CLK1(6,1) dx2ST(HL) OPEND(INC_HL)
OPFUNC(INC_SP) LD2dx(SP) "inc " EDX NL CLK1(6,1) dx2ST(SP) OPEND(INC_SP)

OPFUNC(INC_B) LD2dl(B) ah2INCdl dl2ST(B) CLK1(4,1) OPEND(INC_B) // (4+Tw)
OPFUNC(INC_C) LD2dl(C) ah2INCdl dl2ST(C) CLK1(4,1) OPEND(INC_C)
OPFUNC(INC_D) LD2dl(D) ah2INCdl dl2ST(D) CLK1(4,1) OPEND(INC_D)
OPFUNC(INC_E) LD2dl(E) ah2INCdl dl2ST(E) CLK1(4,1) OPEND(INC_E)
OPFUNC(INC_H) LD2dl(H) ah2INCdl dl2ST(H) CLK1(4,1) OPEND(INC_H)
OPFUNC(INC_L) LD2dl(L) ah2INCdl dl2ST(L) CLK1(4,1) OPEND(INC_L)
OPFUNC(INC_p) LD2cx(HL) "push " ECX NL cxREAD2dl ah2INCdl "pop " ECX NL dl2WRITEcx CLK1(11,1) OPEND(INC_p) // (4+Tw,4,3)
OPFUNC(INC_A) LD2dl(A) ah2INCdl dl2ST(A) CLK1(4,1) OPEND(INC_A)

OPFUNC(DEC_B) LD2dl(B) ah2DECdl dl2ST(B) CLK1(4,1) OPEND(DEC_B) // (4+Tw)
OPFUNC(DEC_C) LD2dl(C) ah2DECdl dl2ST(C) CLK1(4,1) OPEND(DEC_C)
OPFUNC(DEC_D) LD2dl(D) ah2DECdl dl2ST(D) CLK1(4,1) OPEND(DEC_D)
OPFUNC(DEC_E) LD2dl(E) ah2DECdl dl2ST(E) CLK1(4,1) OPEND(DEC_E)
OPFUNC(DEC_H) LD2dl(H) ah2DECdl dl2ST(H) CLK1(4,1) OPEND(DEC_H)
OPFUNC(DEC_L) LD2dl(L) ah2DECdl dl2ST(L) CLK1(4,1) OPEND(DEC_L)
OPFUNC(DEC_p) LD2cx(HL) "push " ECX NL cxREAD2dl ah2DECdl "pop " ECX NL dl2WRITEcx CLK1(11,1) OPEND(DEC_p) // (4+Tw,4,3)
OPFUNC(DEC_A) LD2dl(A) ah2DECdl dl2ST(A) CLK1(4,1) OPEND(DEC_A)

OPFUNC(LD_BC_NN) FETCH2dx(LD_BC_NN) dx2ST(BC) CLK1(10,1) OPEND(LD_BC_NN) // (4+Tw,3,3)
OPFUNC(LD_DE_NN) FETCH2dx(LD_DE_NN) dx2ST(DE) CLK1(10,1) OPEND(LD_DE_NN)
OPFUNC(LD_HL_NN) FETCH2dx(LD_HL_NN) dx2ST(HL) CLK1(10,1) OPEND(LD_HL_NN)
OPFUNC(LD_SP_NN) FETCH2dx(LD_SP_NN) dx2ST(SP) CLK1(10,1) OPEND(LD_SP_NN)

OPFUNC(LD_B_N) FETCH2dl(LD_B_N) dl2ST(B) CLK1(7,2) OPEND(LD_B_N) // (4+Tw,3)
OPFUNC(LD_C_N) FETCH2dl(LD_C_N) dl2ST(C) CLK1(7,2) OPEND(LD_C_N)
OPFUNC(LD_D_N) FETCH2dl(LD_D_N) dl2ST(D) CLK1(7,2) OPEND(LD_D_N)
OPFUNC(LD_E_N) FETCH2dl(LD_E_N) dl2ST(E) CLK1(7,2) OPEND(LD_E_N)
OPFUNC(LD_H_N) FETCH2dl(LD_H_N) dl2ST(H) CLK1(7,2) OPEND(LD_H_N)
OPFUNC(LD_L_N) FETCH2dl(LD_L_N) dl2ST(L) CLK1(7,2) OPEND(LD_L_N)
OPFUNC(LD_A_N) FETCH2dl(LD_A_N) dl2ST(A) CLK1(7,2) OPEND(LD_A_N)

OPFUNC(LD_C_B) LD2dl(B) dl2ST(C) CLK1(4,1) OPEND(LD_C_B) // (4+Tw)
OPFUNC(LD_D_B) LD2dl(B) dl2ST(D) CLK1(4,1) OPEND(LD_D_B)
OPFUNC(LD_E_B) LD2dl(B) dl2ST(E) CLK1(4,1) OPEND(LD_E_B)
OPFUNC(LD_H_B) LD2dl(B) dl2ST(H) CLK1(4,1) OPEND(LD_H_B)
OPFUNC(LD_L_B) LD2dl(B) dl2ST(L) CLK1(4,1) OPEND(LD_L_B)
OPFUNC(LD_HL_B) LD2cx(HL) LD2dl(B) dl2WRITEcx CLK1(7,2) OPEND(LD_HL_B) // (4+Tw,3)
OPFUNC(LD_A_B) LD2dl(B) dl2ST(A) CLK1(4,1) OPEND(LD_A_B)

OPFUNC(LD_B_C) LD2dl(C) dl2ST(B) CLK1(4,1) OPEND(LD_B_C)
OPFUNC(LD_D_C) LD2dl(C) dl2ST(D) CLK1(4,1) OPEND(LD_D_C)
OPFUNC(LD_E_C) LD2dl(C) dl2ST(E) CLK1(4,1) OPEND(LD_E_C)
OPFUNC(LD_H_C) LD2dl(C) dl2ST(H) CLK1(4,1) OPEND(LD_H_C)
OPFUNC(LD_L_C) LD2dl(C) dl2ST(L) CLK1(4,1) OPEND(LD_L_C)
OPFUNC(LD_HL_C) LD2cx(HL) LD2dl(C) dl2WRITEcx CLK1(7,2) OPEND(LD_HL_C)
OPFUNC(LD_A_C) LD2dl(C) dl2ST(A) CLK1(4,1) OPEND(LD_A_C)

OPFUNC(LD_B_D) LD2dl(D) dl2ST(B) CLK1(4,1) OPEND(LD_B_D)
OPFUNC(LD_C_D) LD2dl(D) dl2ST(C) CLK1(4,1) OPEND(LD_C_D)
OPFUNC(LD_E_D) LD2dl(D) dl2ST(E) CLK1(4,1) OPEND(LD_E_D)
OPFUNC(LD_H_D) LD2dl(D) dl2ST(H) CLK1(4,1) OPEND(LD_H_D)
OPFUNC(LD_L_D) LD2dl(D) dl2ST(L) CLK1(4,1) OPEND(LD_L_D)
OPFUNC(LD_HL_D) LD2cx(HL) LD2dl(D) dl2WRITEcx CLK1(7,2) OPEND(LD_HL_D)
OPFUNC(LD_A_D) LD2dl(D) dl2ST(A) CLK1(4,1) OPEND(LD_A_D)

OPFUNC(LD_B_E) LD2dl(E) dl2ST(B) CLK1(4,1) OPEND(LD_B_E)
OPFUNC(LD_C_E) LD2dl(E) dl2ST(C) CLK1(4,1) OPEND(LD_C_E)
OPFUNC(LD_D_E) LD2dl(E) dl2ST(D) CLK1(4,1) OPEND(LD_D_E)
OPFUNC(LD_H_E) LD2dl(E) dl2ST(H) CLK1(4,1) OPEND(LD_H_E)
OPFUNC(LD_L_E) LD2dl(E) dl2ST(L) CLK1(4,1) OPEND(LD_L_E)
OPFUNC(LD_HL_E) LD2cx(HL) LD2dl(E) dl2WRITEcx CLK1(7,2) OPEND(LD_HL_E)
OPFUNC(LD_A_E) LD2dl(E) dl2ST(A) CLK1(4,1) OPEND(LD_A_E)

OPFUNC(LD_B_H) LD2dl(H) dl2ST(B) CLK1(4,1) OPEND(LD_B_H)
OPFUNC(LD_C_H) LD2dl(H) dl2ST(C) CLK1(4,1) OPEND(LD_C_H)
OPFUNC(LD_D_H) LD2dl(H) dl2ST(D) CLK1(4,1) OPEND(LD_D_H)
OPFUNC(LD_E_H) LD2dl(H) dl2ST(E) CLK1(4,1) OPEND(LD_E_H)
OPFUNC(LD_L_H) LD2dl(H) dl2ST(L) CLK1(4,1) OPEND(LD_L_H)
OPFUNC(LD_HL_H) LD2cx(HL) LD2dl(H) dl2WRITEcx CLK1(7,2) OPEND(LD_HL_H)
OPFUNC(LD_A_H) LD2dl(H) dl2ST(A) CLK1(4,1) OPEND(LD_A_H)

OPFUNC(LD_B_L) LD2dl(L) dl2ST(B) CLK1(4,1) OPEND(LD_B_L)
OPFUNC(LD_C_L) LD2dl(L) dl2ST(C) CLK1(4,1) OPEND(LD_C_L)
OPFUNC(LD_D_L) LD2dl(L) dl2ST(D) CLK1(4,1) OPEND(LD_D_L)
OPFUNC(LD_E_L) LD2dl(L) dl2ST(E) CLK1(4,1) OPEND(LD_E_L)
OPFUNC(LD_H_L) LD2dl(L) dl2ST(H) CLK1(4,1) OPEND(LD_H_L)
OPFUNC(LD_HL_L) LD2cx(HL) LD2dl(L) dl2WRITEcx CLK1(7,2) OPEND(LD_HL_L)
OPFUNC(LD_A_L) LD2dl(L) dl2ST(A) CLK1(4,1) OPEND(LD_A_L)

OPFUNC(LD_B_HL) LD2cx(HL) cxREAD2dl dl2ST(B) CLK1(4,1) OPEND(LD_B_HL)
OPFUNC(LD_C_HL) LD2cx(HL) cxREAD2dl dl2ST(C) CLK1(4,1) OPEND(LD_C_HL)
OPFUNC(LD_D_HL) LD2cx(HL) cxREAD2dl dl2ST(D) CLK1(4,1) OPEND(LD_D_HL)
OPFUNC(LD_E_HL) LD2cx(HL) cxREAD2dl dl2ST(E) CLK1(4,1) OPEND(LD_E_HL)
OPFUNC(LD_H_HL) LD2cx(HL) cxREAD2dl dl2ST(H) CLK1(4,1) OPEND(LD_H_HL)
OPFUNC(LD_L_HL) LD2cx(HL) cxREAD2dl dl2ST(L) CLK1(4,1) OPEND(LD_L_HL)
OPFUNC(LD_A_HL) LD2cx(HL) cxREAD2dl dl2ST(A) CLK1(4,1) OPEND(LD_A_HL)

OPFUNC(LD_B_A) LD2dl(A) dl2ST(B) CLK1(4,1) OPEND(LD_B_A)
OPFUNC(LD_C_A) LD2dl(A) dl2ST(C) CLK1(4,1) OPEND(LD_C_A)
OPFUNC(LD_D_A) LD2dl(A) dl2ST(D) CLK1(4,1) OPEND(LD_D_A)
OPFUNC(LD_E_A) LD2dl(A) dl2ST(E) CLK1(4,1) OPEND(LD_E_A)
OPFUNC(LD_H_A) LD2dl(A) dl2ST(H) CLK1(4,1) OPEND(LD_H_A)
OPFUNC(LD_L_A) LD2dl(A) dl2ST(L) CLK1(4,1) OPEND(LD_L_A)
OPFUNC(LD_HL_A) LD2cx(HL) LD2dl(A) dl2WRITEcx CLK1(7,2) OPEND(LD_HL_A)

__asm__ (
	".section .rodata" NL
	".type " LC "z80_opcode" ",@object" LF
LC "z80_opcode:" NL
	".long " OP "NOP," OP "LD_BC_NN," OP "NOP," OP "INC_BC," OP "INC_B," OP "DEC_B," OP "LD_B_N," OP "NOP" NL
	".long " OP "NOP," OP      "NOP," OP "NOP," OP    "NOP," OP "INC_C," OP "DEC_C," OP "LD_C_N," OP "NOP" NL
	".long " OP "NOP," OP "LD_DE_NN," OP "NOP," OP "INC_DE," OP "INC_D," OP "DEC_D," OP "LD_D_N," OP "NOP" NL
	".long " OP "NOP," OP      "NOP," OP "NOP," OP    "NOP," OP "INC_E," OP "DEC_E," OP "LD_E_N," OP "NOP" NL
	".long " OP "NOP," OP "LD_HL_NN," OP "NOP," OP "INC_HL," OP "INC_H," OP "DEC_H," OP "LD_H_N," OP "NOP" NL
	".long " OP "NOP," OP      "NOP," OP "NOP," OP    "NOP," OP "INC_L," OP "DEC_L," OP "LD_L_N," OP "NOP" NL
	".long " OP "NOP," OP "LD_SP_NN," OP "NOP," OP "INC_SP," OP "INC_p," OP "DEC_p," OP "NOP,"    OP "NOP" NL
	".long " OP "NOP," OP      "NOP," OP "NOP," OP    "NOP," OP "INC_A," OP "DEC_A," OP "LD_A_N," OP "NOP" NL

	".long " OP    "NOP,"  OP "LD_B_C,"  OP "LD_B_D,"  OP "LD_B_E,"  OP "LD_B_H,"  OP "LD_B_L,"  OP "LD_B_HL," OP "LD_B_A" NL
	".long " OP "LD_C_B,"  OP    "NOP,"  OP "LD_C_D,"  OP "LD_C_E,"  OP "LD_C_H,"  OP "LD_C_L,"  OP "LD_C_HL," OP "LD_C_A" NL
	".long " OP "LD_D_B,"  OP "LD_D_C,"  OP    "NOP,"  OP "LD_D_E,"  OP "LD_D_H,"  OP "LD_D_L,"  OP "LD_D_HL," OP "LD_D_A" NL
	".long " OP "LD_E_B,"  OP "LD_E_C,"  OP "LD_E_D,"  OP    "NOP,"  OP "LD_E_H,"  OP "LD_E_L,"  OP "LD_E_HL," OP "LD_E_A" NL
	".long " OP "LD_H_B,"  OP "LD_H_C,"  OP "LD_H_D,"  OP "LD_H_E,"  OP    "NOP,"  OP "LD_H_L,"  OP "LD_H_HL," OP "LD_H_A" NL
	".long " OP "LD_L_B,"  OP "LD_L_C,"  OP "LD_L_D,"  OP "LD_L_E,"  OP "LD_L_H,"  OP    "NOP,"  OP "LD_L_HL," OP "LD_L_A" NL
	".long " OP "LD_HL_B," OP "LD_HL_C," OP "LD_HL_D," OP "LD_HL_E," OP "LD_HL_H," OP "LD_HL_L," OP "NOP,"     OP "LD_HL_A" NL
	".long " OP "LD_A_B,"  OP "LD_A_C,"  OP "LD_A_D,"  OP "LD_A_E,"  OP "LD_A_H,"  OP "LD_A_L,"  OP "LD_A_HL," OP    "NOP" NL

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
