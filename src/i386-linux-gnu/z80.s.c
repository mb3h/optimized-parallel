#if i386 == Em
/* cf)
	Z80 Family CPU User Manual (Rev.05 - Feb'05)
	Intel IA-32(R) Architectures Software Developer's Manual,
	- Volume 1: Basic Architecture
	- Volume 2: Instruction Set Reference
	- Volume 3: System Programming Guide
 */
/* NOTE:
	This emulation module (z80.s.c) got many hints from Z80_x86.cpp written by cisc.
	In Z80 emulation code I know, his x86 native module was most lightweight, most
	optimized and most suitable bit layout. I expand memory pager and fetch state,
	fixed I/O switch timing resolution and modified other several for generality,
	however his great achievments never fade.
   CAUTION:
	(*1)must be update 'reg_pc' when memory page is changed.
 */
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
#define LC ".L" // local address (.text/.rodata)
#define LS ".L" // local symbol (.equ)
#define OP ".L" // local address of opcode (.text)
#define M ".L" // local offset of member (.struct)

#if 16 == RRm
# define sizeofPAD "0"
#else //if 32 == RRm
# define sizeofPAD "2"
#endif
# define sizeofPTR "4"

// memctl_s
__asm__ (
	".equ " LS "log2of_memctl" ",3" NL // 1 << 3 == sizeof(memctl_s)

	".struct 0" LF
M "raw_or_rwfn:" LF
	".struct " M "raw_or_rwfn +" sizeofPTR LF
M "rwfn_priv:" LF
	".struct " M "rwfn_priv +" sizeofPTR LF
);

// z80_s
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
M "reg_sp:" LF
M "reg_spl:" NL
	".struct " M "reg_spl +1" LF
M "reg_sph:" NL
	".struct " M "reg_sph +1 +" sizeofPAD LF
M "reg_fa:" LF
M "reg_a:" NL
	".struct " M "reg_a +1" LF
M "reg_f:" NL
	".struct " M "reg_f +1 +" sizeofPAD LF
M "reg_pc:" NL
	".struct " M "reg_pc +2 +" sizeofPAD LF
M "mem:" NL
	".struct " M "mem +(8 << " LS "log2of_memctl)" LF
);

#if 16 == RRm
# define EAX          "%ax"
# define ECX          "%cx"
# define EDX          "%dx"
# define EBX          "%bx"
#else //if 32 == RRm
# define EAX          "%eax"
# define ECX          "%ecx"
# define EDX          "%edx"
# define EBX          "%ebx"
#endif

#define CLK             "%ebp"
#define EAPC            "%edi"
#define CPU             "%esi"
#define A               "%al"
#define B   M   "reg_b(" CPU ")"
#define C   M   "reg_c(" CPU ")"
#define D   M   "reg_d(" CPU ")"
#define E   M   "reg_e(" CPU ")"
#define H   M   "reg_h(" CPU ")"
#define L   M   "reg_l(" CPU ")"
#define SPH M "reg_sph(" CPU ")"
#define SPL M "reg_spl(" CPU ")"
#define BC  M  "reg_bc(" CPU ")"
#define DE  M  "reg_de(" CPU ")"
#define HL  M  "reg_hl(" CPU ")"
#define FA  M  "reg_fa(" CPU ")"
#define SP  M  "reg_sp(" CPU ")"
#define PC  M  "reg_pc(" CPU ")"

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
#define dh2ST(dst) \
	"mov " "%dh" "," dst NL
#define dl2ST(dst) \
	"mov " "%dl" "," dst NL
#define LD2dl(src) \
	"mov " src "," "%dl" NL
#define dx2cx \
	"mov " EDX "," ECX NL
#define LD2cx(src) \
	"mov " src "," ECX NL
#define LD2dx(src) \
	"mov " src "," EDX NL
#define dx2ST(dst) \
	"mov " EDX "," dst NL

#define GFNSTART(label) \
	__asm__ ( \
		".text" NL \
		".globl " "z80_" #label NL \
		".type " "z80_" #label ",@function" LF \
	"z80_" #label ":" NL \
		".cfi_startproc" NL
#define GFNEND(label) \
		"ret" NL \
		".cfi_endproc" NL \
		".size " "z80_" #label ",.-" "z80_" #label NL \
	);

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

LCFUNC(cxREAD2dl)
	"mov " ECX "," EBX NL
	"shr $13 -2," ECX NL
	"and $7 << 2," ECX NL // 7 = (1 << 16 -13) -1
	"mov " M "raw_or_rwfn + " M "mem(" CPU "," ECX ",2)," ECX NL // 2 = 1 << log2of(memctl_s) -2
	"and $0x1FFF," EBX NL // 0x1FFF = (1 << 13) -1
	"mov (" ECX "," EBX ",1),%dl" NL
LCEND(cxREAD2dl)

LCFUNC(cxREAD2dx)
	/* TODO: */
	"mov " ECX "," EBX NL
	"lea 1(" EBX ")," EDX NL
	"test $0x1FFF," EDX NL // 0x1FFF = (1 << 13) -1
	"jz " LC "cxREAD2dx" "_ladder_two_pages" NL
	"shr $13 -2," EBX NL
	"and $7 << 2," EBX NL // 7 = (1 << 16 -13) -1
	"mov " M "raw_or_rwfn + " M "mem(" CPU "," EBX ",2)," ECX NL // 2 = 1 << log2of(memctl_s) -2
//LC "cxREAD2dx" "_raw:" NL
	"and $0x1FFF," EDX NL // 0x1FFF = (1 << 13) -1
	"movzwl -1(" ECX "," EDX ",1)," EDX NL
	"ret" LF
LC "cxREAD2dx" "_ladder_two_pages:" NL
	"push " EBX NL
	"call " LC "cxREAD2dl" NL
	"pop " EBX NL
	"inc " EBX NL
	"push " EDX NL
	"call " LC "cxREAD2dl" NL
	"pop " EBX NL
	"shl $8," EDX NL
	"or " EBX "," EDX NL
LCEND(cxREAD2dx)

// broken: ebx
LCFUNC(dl2WRITEcx)
	"mov " ECX "," EBX NL
	"shr $13 -2," ECX NL
	"and $7 << 2," ECX NL // 7 = (1 << 16 -13) -1
	"mov " M "raw_or_rwfn + " M "mem(" CPU "," ECX ",2)," ECX NL // 2 = 1 << log2of(memctl_s) -2
	"and $0x1FFF," EBX NL // 0x1FFF = (1 << 13) -1
	"mov %dl,(" ECX "," EBX ",1)" NL
LCEND(dl2WRITEcx)

// broken: ebx edx
LCFUNC(dx2WRITEcx)
	"jz " LC "dx2WRITEcx" "_ladder_two_pages" NL
	"lea 1(" ECX ")," EBX NL // b:addr+1(b15..b0)
	"test $0x1FFF," EBX NL // 0x1FFF = (1 << 13) -1
	"jz " LC "dx2WRITEcx" "_ladder_two_pages" NL
	"shr $13 -2," ECX NL
	"and $7 << 2," ECX NL // 7 = (1 << 16 -13) -1
	"mov " M "raw_or_rwfn + " M "mem(" CPU "," ECX ",2)," ECX NL // 2 = 1 << log2of(memctl_s) -2
//LC "dx2WRITEcx" "_raw:" NL
	"and $0x1FFF," EBX NL // 0x1FFF = (1 << 13) -1
	"mov %dx,-1(" ECX "," EBX ",1)" NL
	"ret" LF
LC "dx2WRITEcx" "_ladder_two_pages:" NL
	"push " EDX NL
	"push " ECX NL
	"call " LC "dl2WRITEcx" NL
	"pop " ECX NL
	"inc " ECX NL
	"pop " EDX NL
	"shr $8," EDX NL
	"jmp " LC "dl2WRITEcx" NL
LCEND(dx2WRITEcx)

#define cxREAD2dl \
	"call " LC "cxREAD2dl" NL
#define cxREAD2dx \
	"call " LC "cxREAD2dx" NL
#define dl2WRITEcx \
	"call " LC "dl2WRITEcx" NL
#define dx2WRITEcx \
	"call " LC "dx2WRITEcx" NL

// CAUTION: (*1)
#define FETCH2dl(label) \
	/* TODO: mem[] */ \
	"inc " EAPC NL \
	"movzbl (" EAPC ")," "%edx" NL
#define FETCH2dx(label) \
	/* TODO: mem[] */ \
	"add $2," EAPC NL \
	"movzwl -1(" EAPC ")," "%edx" NL

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
#define MADD_A(reg) \
	"add " reg "," A NL eflags2ah eflagsOF2cl2ah
#define MADC_A(reg) \
	"ror %ah" NL "adc " reg "," A NL eflags2ah eflagsOF2cl2ah
#define MSUB(reg) \
	"sub " reg "," A NL eflags2ah eflagsOF2cl2ah SET2ah(NF)
#define MSBC_A(reg) \
	"ror %ah" NL "sbb " reg "," A NL eflags2ah eflagsOF2cl2ah SET2ah(NF)
#define MAND(reg) \
	"and " reg "," A NL eflags2ah RES2ah(NF) SET2ah(HF)
#define MXOR(reg) \
	"xor " reg "," A NL eflags2ah RES2ah(NF "+" HF)
#define MOR(reg) \
	"or " reg "," A NL eflags2ah RES2ah(NF "+" HF)
#define MCP(reg) \
	"mov " reg ",%dh" NL "cmp %dh," A NL eflags2ah eflagsOF2cl2ah SET2ah(NF)
#define ah2INCdl \
	ah2eflags "inc %dl" NL eflags2ah eflagsOF2cl2ah
#define ah2DECdl \
	ah2eflags "dec %dl" NL eflags2ah eflagsOF2cl2ah SET2ah(NF)
#define ah2ADDdx(h,l) \
	"mov %ah,%cl" NL "add " l ",%dl" NL "adc " h ",%dh" NL \
	eflags2ah "and $(0xff - (" HF "+" NF "+" CF ")),%cl" NL "and $(" HF "+" CF "),%ah" NL "or %cl,%ah" NL

OPFUNC(ADD_HL_BC) LD2dx(HL) ah2ADDdx(B,C)     dl2ST(L) CLK1(11,3) dh2ST(H) OPEND(ADD_HL_BC) // (4+Tw,4,3)
OPFUNC(ADD_HL_DE) LD2dx(HL) ah2ADDdx(D,E)     dl2ST(L) CLK1(11,3) dh2ST(H) OPEND(ADD_HL_DE)
OPFUNC(ADD_HL_HL) LD2dx(HL) ah2ADDdx(H,L)     dl2ST(L) CLK1(11,3) dh2ST(H) OPEND(ADD_HL_HL)
OPFUNC(ADD_HL_SP) LD2dx(HL) ah2ADDdx(SPH,SPL) dl2ST(L) CLK1(11,3) dh2ST(H) OPEND(ADD_HL_SP)

OPFUNC(INC_BC) LD2dx(BC) "inc " EDX NL CLK1(6,1) dx2ST(BC) OPEND(INC_BC) // (6+Tw)
OPFUNC(INC_DE) LD2dx(DE) "inc " EDX NL CLK1(6,1) dx2ST(DE) OPEND(INC_DE)
OPFUNC(INC_HL) LD2dx(HL) "inc " EDX NL CLK1(6,1) dx2ST(HL) OPEND(INC_HL)
OPFUNC(INC_SP) LD2dx(SP) "inc " EDX NL CLK1(6,1) dx2ST(SP) OPEND(INC_SP)

OPFUNC(DEC_BC) LD2dx(BC) "dec " EDX NL CLK1(6,1) dx2ST(BC) OPEND(DEC_BC) // (6+Tw)
OPFUNC(DEC_DE) LD2dx(DE) "dec " EDX NL CLK1(6,1) dx2ST(DE) OPEND(DEC_DE)
OPFUNC(DEC_HL) LD2dx(HL) "dec " EDX NL CLK1(6,1) dx2ST(HL) OPEND(DEC_HL)
OPFUNC(DEC_SP) LD2dx(SP) "dec " EDX NL CLK1(6,1) dx2ST(SP) OPEND(DEC_SP)

#define OPALU(fn) \
	OPFUNC(fn##_B) M##fn(B) CLK1(4,1) OPEND(fn##_B) /* (4+Tw) */ \
	OPFUNC(fn##_C) M##fn(C) CLK1(4,1) OPEND(fn##_C) \
	OPFUNC(fn##_D) M##fn(D) CLK1(4,1) OPEND(fn##_D) \
	OPFUNC(fn##_E) M##fn(E) CLK1(4,1) OPEND(fn##_E) \
	OPFUNC(fn##_H) M##fn(H) CLK1(4,1) OPEND(fn##_H) \
	OPFUNC(fn##_L) M##fn(L) CLK1(4,1) OPEND(fn##_L) \
	OPFUNC(fn##_p) LD2cx(HL) cxREAD2dl M##fn("%dl") CLK1(7,2) OPEND(fn##_p) /* (4+Tw,3) */ \
	OPFUNC(fn##_A) M##fn(A) CLK1(4,1) OPEND(fn##_A)

OPALU(ADD_A) OPALU(ADC_A) OPALU(SUB) OPALU(SBC_A)
OPALU(AND)   OPALU(XOR)   OPALU(OR)  OPALU(CP)

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

OPFUNC(LD_C_B) LD2dl(B) dl2ST(C) CLK1(4,1) OPEND(LD_C_B) // (4+Tw)
OPFUNC(LD_D_B) LD2dl(B) dl2ST(D) CLK1(4,1) OPEND(LD_D_B)
OPFUNC(LD_E_B) LD2dl(B) dl2ST(E) CLK1(4,1) OPEND(LD_E_B)
OPFUNC(LD_H_B) LD2dl(B) dl2ST(H) CLK1(4,1) OPEND(LD_H_B)
OPFUNC(LD_L_B) LD2dl(B) dl2ST(L) CLK1(4,1) OPEND(LD_L_B)
OPFUNC(LD_p_B) LD2cx(HL) LD2dl(B) dl2WRITEcx CLK1(7,2) OPEND(LD_p_B) // (4+Tw,3)
OPFUNC(LD_A_B) LD2dl(B) dl2ST(A) CLK1(4,1) OPEND(LD_A_B)

OPFUNC(LD_B_C) LD2dl(C) dl2ST(B) CLK1(4,1) OPEND(LD_B_C)
OPFUNC(LD_D_C) LD2dl(C) dl2ST(D) CLK1(4,1) OPEND(LD_D_C)
OPFUNC(LD_E_C) LD2dl(C) dl2ST(E) CLK1(4,1) OPEND(LD_E_C)
OPFUNC(LD_H_C) LD2dl(C) dl2ST(H) CLK1(4,1) OPEND(LD_H_C)
OPFUNC(LD_L_C) LD2dl(C) dl2ST(L) CLK1(4,1) OPEND(LD_L_C)
OPFUNC(LD_p_C) LD2cx(HL) LD2dl(C) dl2WRITEcx CLK1(7,2) OPEND(LD_p_C)
OPFUNC(LD_A_C) LD2dl(C) dl2ST(A) CLK1(4,1) OPEND(LD_A_C)

OPFUNC(LD_B_D) LD2dl(D) dl2ST(B) CLK1(4,1) OPEND(LD_B_D)
OPFUNC(LD_C_D) LD2dl(D) dl2ST(C) CLK1(4,1) OPEND(LD_C_D)
OPFUNC(LD_E_D) LD2dl(D) dl2ST(E) CLK1(4,1) OPEND(LD_E_D)
OPFUNC(LD_H_D) LD2dl(D) dl2ST(H) CLK1(4,1) OPEND(LD_H_D)
OPFUNC(LD_L_D) LD2dl(D) dl2ST(L) CLK1(4,1) OPEND(LD_L_D)
OPFUNC(LD_p_D) LD2cx(HL) LD2dl(D) dl2WRITEcx CLK1(7,2) OPEND(LD_p_D)
OPFUNC(LD_A_D) LD2dl(D) dl2ST(A) CLK1(4,1) OPEND(LD_A_D)

OPFUNC(LD_B_E) LD2dl(E) dl2ST(B) CLK1(4,1) OPEND(LD_B_E)
OPFUNC(LD_C_E) LD2dl(E) dl2ST(C) CLK1(4,1) OPEND(LD_C_E)
OPFUNC(LD_D_E) LD2dl(E) dl2ST(D) CLK1(4,1) OPEND(LD_D_E)
OPFUNC(LD_H_E) LD2dl(E) dl2ST(H) CLK1(4,1) OPEND(LD_H_E)
OPFUNC(LD_L_E) LD2dl(E) dl2ST(L) CLK1(4,1) OPEND(LD_L_E)
OPFUNC(LD_p_E) LD2cx(HL) LD2dl(E) dl2WRITEcx CLK1(7,2) OPEND(LD_p_E)
OPFUNC(LD_A_E) LD2dl(E) dl2ST(A) CLK1(4,1) OPEND(LD_A_E)

OPFUNC(LD_B_H) LD2dl(H) dl2ST(B) CLK1(4,1) OPEND(LD_B_H)
OPFUNC(LD_C_H) LD2dl(H) dl2ST(C) CLK1(4,1) OPEND(LD_C_H)
OPFUNC(LD_D_H) LD2dl(H) dl2ST(D) CLK1(4,1) OPEND(LD_D_H)
OPFUNC(LD_E_H) LD2dl(H) dl2ST(E) CLK1(4,1) OPEND(LD_E_H)
OPFUNC(LD_L_H) LD2dl(H) dl2ST(L) CLK1(4,1) OPEND(LD_L_H)
OPFUNC(LD_p_H) LD2cx(HL) LD2dl(H) dl2WRITEcx CLK1(7,2) OPEND(LD_p_H)
OPFUNC(LD_A_H) LD2dl(H) dl2ST(A) CLK1(4,1) OPEND(LD_A_H)

OPFUNC(LD_B_L) LD2dl(L) dl2ST(B) CLK1(4,1) OPEND(LD_B_L)
OPFUNC(LD_C_L) LD2dl(L) dl2ST(C) CLK1(4,1) OPEND(LD_C_L)
OPFUNC(LD_D_L) LD2dl(L) dl2ST(D) CLK1(4,1) OPEND(LD_D_L)
OPFUNC(LD_E_L) LD2dl(L) dl2ST(E) CLK1(4,1) OPEND(LD_E_L)
OPFUNC(LD_H_L) LD2dl(L) dl2ST(H) CLK1(4,1) OPEND(LD_H_L)
OPFUNC(LD_p_L) LD2cx(HL) LD2dl(L) dl2WRITEcx CLK1(7,2) OPEND(LD_p_L)
OPFUNC(LD_A_L) LD2dl(L) dl2ST(A) CLK1(4,1) OPEND(LD_A_L)

OPFUNC(LD_B_p) LD2cx(HL) cxREAD2dl dl2ST(B) CLK1(7,2) OPEND(LD_B_p)
OPFUNC(LD_C_p) LD2cx(HL) cxREAD2dl dl2ST(C) CLK1(7,2) OPEND(LD_C_p)
OPFUNC(LD_D_p) LD2cx(HL) cxREAD2dl dl2ST(D) CLK1(7,2) OPEND(LD_D_p)
OPFUNC(LD_E_p) LD2cx(HL) cxREAD2dl dl2ST(E) CLK1(7,2) OPEND(LD_E_p)
OPFUNC(LD_H_p) LD2cx(HL) cxREAD2dl dl2ST(H) CLK1(7,2) OPEND(LD_H_p)
OPFUNC(LD_L_p) LD2cx(HL) cxREAD2dl dl2ST(L) CLK1(7,2) OPEND(LD_L_p)
OPFUNC(LD_A_p) LD2cx(HL) cxREAD2dl dl2ST(A) CLK1(7,2) OPEND(LD_A_p)

OPFUNC(LD_B_A) LD2dl(A) dl2ST(B) CLK1(4,1) OPEND(LD_B_A)
OPFUNC(LD_C_A) LD2dl(A) dl2ST(C) CLK1(4,1) OPEND(LD_C_A)
OPFUNC(LD_D_A) LD2dl(A) dl2ST(D) CLK1(4,1) OPEND(LD_D_A)
OPFUNC(LD_E_A) LD2dl(A) dl2ST(E) CLK1(4,1) OPEND(LD_E_A)
OPFUNC(LD_H_A) LD2dl(A) dl2ST(H) CLK1(4,1) OPEND(LD_H_A)
OPFUNC(LD_L_A) LD2dl(A) dl2ST(L) CLK1(4,1) OPEND(LD_L_A)
OPFUNC(LD_p_A) LD2cx(HL) LD2dl(A) dl2WRITEcx CLK1(7,2) OPEND(LD_p_A)

OPFUNC(LD_B_N) FETCH2dl(LD_B_N) dl2ST(B) CLK1(7,2) OPEND(LD_B_N) // (4+Tw,3)
OPFUNC(LD_C_N) FETCH2dl(LD_C_N) dl2ST(C) CLK1(7,2) OPEND(LD_C_N)
OPFUNC(LD_D_N) FETCH2dl(LD_D_N) dl2ST(D) CLK1(7,2) OPEND(LD_D_N)
OPFUNC(LD_E_N) FETCH2dl(LD_E_N) dl2ST(E) CLK1(7,2) OPEND(LD_E_N)
OPFUNC(LD_H_N) FETCH2dl(LD_H_N) dl2ST(H) CLK1(7,2) OPEND(LD_H_N)
OPFUNC(LD_L_N) FETCH2dl(LD_L_N) dl2ST(L) CLK1(7,2) OPEND(LD_L_N)
OPFUNC(LD_p_N) FETCH2dl(LD_p_N) LD2cx(HL) CLK1(10,3) dl2WRITEcx OPEND(LD_p_N) // (4+Tw,3,3)
OPFUNC(LD_A_N) FETCH2dl(LD_A_N) dl2ST(A) CLK1(7,2) OPEND(LD_A_N)

OPFUNC(LD_BC_A) LD2cx(BC) LD2dl(A) CLK1(7,2) dl2WRITEcx OPEND(LD_BC_A) // (4+Tw,3)
OPFUNC(LD_A_BC) LD2cx(BC) cxREAD2dl dl2ST(A) CLK1(7,2) OPEND(LD_A_BC)
OPFUNC(LD_DE_A) LD2cx(DE) LD2dl(A) CLK1(7,2) dl2WRITEcx OPEND(LD_DE_A)
OPFUNC(LD_A_DE) LD2cx(DE) cxREAD2dl dl2ST(A) CLK1(7,2) OPEND(LD_A_DE)
OPFUNC(LD_pNN_HL) FETCH2dx(LD_pNN_HL) dx2cx CLK1(16,5) LD2dx(HL) dx2WRITEcx OPEND(LD_pNN_HL) // (4+Tw,3,3,3,3)
OPFUNC(LD_HL_pNN) FETCH2dx(LD_HL_pNN) dx2cx CLK1(16,5) cxREAD2dx dx2ST(HL) OPEND(LD_HL_pNN)
OPFUNC(LD_pNN_A) FETCH2dx(LD_pNN_A) dx2cx LD2dl(A) CLK1(13,4) dl2WRITEcx OPEND(LD_pNN_A) // (4+Tw,3,3,3)
OPFUNC(LD_A_pNN) FETCH2dx(LD_A_pNN) dx2cx cxREAD2dl CLK1(13,4) dl2ST(A) OPEND(LD_A_pNN)

__asm__ (
	".section .rodata" NL
	".type " LC "z80_opcode" ",@object" LF
LC "z80_opcode:" NL
	".long " OP "NOP," OP "LD_BC_NN,"  OP "LD_BC_A,"   OP "INC_BC," OP "INC_B," OP "DEC_B," OP "LD_B_N," OP "NOP" NL
	".long " OP "NOP," OP "ADD_HL_BC," OP "LD_A_BC,"   OP "DEC_BC," OP "INC_C," OP "DEC_C," OP "LD_C_N," OP "NOP" NL
	".long " OP "NOP," OP "LD_DE_NN,"  OP "LD_DE_A,"   OP "INC_DE," OP "INC_D," OP "DEC_D," OP "LD_D_N," OP "NOP" NL
	".long " OP "NOP," OP "ADD_HL_DE," OP "LD_A_DE,"   OP "DEC_DE," OP "INC_E," OP "DEC_E," OP "LD_E_N," OP "NOP" NL
	".long " OP "NOP," OP "LD_HL_NN,"  OP "LD_pNN_HL," OP "INC_HL," OP "INC_H," OP "DEC_H," OP "LD_H_N," OP "NOP" NL
	".long " OP "NOP," OP "ADD_HL_HL," OP "LD_HL_pNN," OP "DEC_HL," OP "INC_L," OP "DEC_L," OP "LD_L_N," OP "NOP" NL
	".long " OP "NOP," OP "LD_SP_NN,"  OP "LD_pNN_A,"  OP "INC_SP," OP "INC_p," OP "DEC_p," OP "LD_p_N," OP "NOP" NL
	".long " OP "NOP," OP "ADD_HL_SP," OP "LD_A_pNN,"  OP "DEC_SP," OP "INC_A," OP "DEC_A," OP "LD_A_N," OP "NOP" NL

	".long " OP    "NOP," OP "LD_B_C," OP "LD_B_D," OP "LD_B_E," OP "LD_B_H," OP "LD_B_L," OP "LD_B_p," OP "LD_B_A" NL
	".long " OP "LD_C_B," OP    "NOP," OP "LD_C_D," OP "LD_C_E," OP "LD_C_H," OP "LD_C_L," OP "LD_C_p," OP "LD_C_A" NL
	".long " OP "LD_D_B," OP "LD_D_C," OP    "NOP," OP "LD_D_E," OP "LD_D_H," OP "LD_D_L," OP "LD_D_p," OP "LD_D_A" NL
	".long " OP "LD_E_B," OP "LD_E_C," OP "LD_E_D," OP    "NOP," OP "LD_E_H," OP "LD_E_L," OP "LD_E_p," OP "LD_E_A" NL
	".long " OP "LD_H_B," OP "LD_H_C," OP "LD_H_D," OP "LD_H_E," OP    "NOP," OP "LD_H_L," OP "LD_H_p," OP "LD_H_A" NL
	".long " OP "LD_L_B," OP "LD_L_C," OP "LD_L_D," OP "LD_L_E," OP "LD_L_H," OP    "NOP," OP "LD_L_p," OP "LD_L_A" NL
	".long " OP "LD_p_B," OP "LD_p_C," OP "LD_p_D," OP "LD_p_E," OP "LD_p_H," OP "LD_p_L," OP "NOP,"    OP "LD_p_A" NL
	".long " OP "LD_A_B," OP "LD_A_C," OP "LD_A_D," OP "LD_A_E," OP "LD_A_H," OP "LD_A_L," OP "LD_A_p," OP    "NOP" NL

	".long " OP "ADD_A_B," OP "ADD_A_C," OP "ADD_A_D," OP "ADD_A_E," OP "ADD_A_H," OP "ADD_A_L," OP "ADD_A_p," OP "ADD_A_A" NL
	".long " OP "ADC_A_B," OP "ADC_A_C," OP "ADC_A_D," OP "ADC_A_E," OP "ADC_A_H," OP "ADC_A_L," OP "ADC_A_p," OP "ADC_A_A" NL
	".long " OP   "SUB_B," OP   "SUB_C," OP   "SUB_D," OP   "SUB_E," OP   "SUB_H," OP   "SUB_L," OP   "SUB_p," OP   "SUB_A" NL
	".long " OP "SBC_A_B," OP "SBC_A_C," OP "SBC_A_D," OP "SBC_A_E," OP "SBC_A_H," OP "SBC_A_L," OP "SBC_A_p," OP "SBC_A_A" NL
	".long " OP   "AND_B," OP   "AND_C," OP   "AND_D," OP   "AND_E," OP   "AND_H," OP   "AND_L," OP   "AND_p," OP   "AND_A" NL
	".long " OP   "XOR_B," OP   "XOR_C," OP   "XOR_D," OP   "XOR_E," OP   "XOR_H," OP   "XOR_L," OP   "XOR_p," OP   "XOR_A" NL
	".long " OP    "OR_B," OP    "OR_C," OP    "OR_D," OP    "OR_E," OP    "OR_H," OP    "OR_L," OP    "OR_p," OP    "OR_A" NL
	".long " OP    "CP_B," OP    "CP_C," OP    "CP_D," OP    "CP_E," OP    "CP_H," OP    "CP_L," OP    "CP_p," OP    "CP_A" NL

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

GFNSTART(exec)
	"push " CPU NL
	"push " ECX NL
	"push " CLK NL
	".cfi_def_cfa_offset 16" NL
	".cfi_offset " CLK ",-16" NL // ebp
	"push " EBX NL
	".cfi_def_cfa_offset 20" NL
	"push " EAPC NL
	".cfi_def_cfa_offset 24" NL



	"mov 24+0(%esp)," CPU NL
	"mov " PC "," ECX NL
	"mov " ECX "," EAPC NL
	"shr $13 -2," ECX NL
	"and $7 << 2," ECX NL // 7 = (1 << 16 -13) -1
	"mov " M "raw_or_rwfn + " M "mem(" CPU "," ECX ",2)," ECX NL // 2 = 1 << log2of(memctl_s) -2
	"and $0x1FFF," EAPC NL // 0x1FFF = (1 << 13) -1
	"add " ECX "," EAPC NL
	"mov " FA "," EAX NL

	"dec " EAPC NL
	"xor " CLK "," CLK NL
LC "z80_exec_loop:"
	FETCH2dl(z80_exec)
	"call *" LC "z80_opcode(," EDX ",4)" NL

	"cmp 24+4(%esp)," CLK NL
	"jc " LC "z80_exec_loop" NL
	"mov " EAX "," FA NL
	"inc " EAPC NL
	"mov " PC "," EAX NL // (*1)
	"and $7 << 13," EAX NL // 7 = (1 << 16 -13) -1
	"add " EAX "," EAPC NL
	"shr $13 -2," EAX NL
	"sub " M "raw_or_rwfn + " M "mem(" CPU "," EAX ",2)," EAPC NL // 2 = 1 << log2of(memctl_s) -2
	"mov " EAPC "," PC NL
	"mov " CLK "," EAX NL
	
	"pop " EAPC NL
	".cfi_def_cfa esp,20" NL
	"pop " EBX NL
	".cfi_def_cfa_offset 16" NL
	"pop " CLK NL
	".cfi_restore " CLK NL // ebp
	".cfi_def_cfa_offset 12" NL
	"pop " ECX NL
	".cfi_def_cfa_offset 8" NL
	"pop " CPU NL
	".cfi_def_cfa_offset 4" NL
GFNEND(exec)

#endif // i386 == Em
