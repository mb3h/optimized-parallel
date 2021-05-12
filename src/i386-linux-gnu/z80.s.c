#ifdef Ei386
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
	(*1) only reserved bit (b5b3) keep newer on blocking.
	(*2) must be update 'reg_pc' when memory page is changed.
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
#define M  ".L" // local offset of member (.struct)

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
M "reg_f:" NL // (*1)
	".struct " M "reg_f +1 +" sizeofPAD LF

M "alt_bc:" LF
	".struct " M "alt_bc +2 +" sizeofPAD LF
M "alt_de:" LF
	".struct " M "alt_de +2 +" sizeofPAD LF
M "alt_hl:" LF
	".struct " M "alt_hl +2 +" sizeofPAD LF
M "alt_af:" LF
	".struct " M "alt_af +2 +" sizeofPAD LF

M "reg_pc:" NL
	".struct " M "reg_pc +2 +" sizeofPAD LF

M "iff1:" NL
	".struct " M "iff1 +1" LF
M "iff2:" NL
	".struct " M "iff2 +1 +" sizeofPAD LF

M "eapc2pc_neg:" LF
	".struct " M "eapc2pc_neg +" sizeofPTR LF
M "eapg_min:" LF
	".struct " M "eapg_min +" sizeofPTR LF
M "eapg_max:" LF
	".struct " M "eapg_max +" sizeofPTR LF

M "mem:" NL
	".struct " M "mem +(8 << " LS "log2of_memctl)" LF
);

#if 16 == RRm
# define AX           "%ax"
# define CX           "%cx"
# define DX           "%dx"
# define BX           "%bx"
#else //if 32 == RRm
# define AX           "%eax"
# define CX           "%ecx"
# define DX           "%edx"
# define BX           "%ebx"
#endif
#define EAX          AX
#define ECX          CX
#define EDX          DX
#define EBX          BX

#define CLK                        "%ebp"
#define VPC                        "%edi"
#define PC                          VPC
#define CPU                        "%esi"
#define A                          "%al"
#define B          M       "reg_b(" CPU ")"
#define C          M       "reg_c(" CPU ")"
#define D          M       "reg_d(" CPU ")"
#define E          M       "reg_e(" CPU ")"
#define H          M       "reg_h(" CPU ")"
#define L          M       "reg_l(" CPU ")"
#define UND_F      M       "reg_f(" CPU ")" // (*1)
#define SPH        M     "reg_sph(" CPU ")"
#define SPL        M     "reg_spl(" CPU ")"
#define BC         M      "reg_bc(" CPU ")"
#define DE         M      "reg_de(" CPU ")"
#define HL         M      "reg_hl(" CPU ")"
#define FA         M      "reg_fa(" CPU ")"
#define SP         M      "reg_sp(" CPU ")"
#define PC_ORIG    M      "reg_pc(" CPU ")"
#define IFF1       M        "iff1(" CPU ")"
#define IFF2       M        "iff2(" CPU ")"
#define ALTBC      M      "alt_bc(" CPU ")"
#define ALTDE      M      "alt_de(" CPU ")"
#define ALTHL      M      "alt_hl(" CPU ")"
#define ALTAF      M      "alt_af(" CPU ")"
#define EAPG_MIN   M    "eapg_min(" CPU ")"
#define EAPG_MAX   M    "eapg_max(" CPU ")"
#define VPC2PC_NEG M "eapc2pc_neg(" CPU ")"
#define PC2VA(reg) "sub " VPC2PC_NEG "," reg NL
#define VA2PC(reg) "add " VPC2PC_NEG "," reg NL

#define  CF "0x01"
#define  NF "0x02"
#define  VF "0x04"
//#define  PF VF
#define R1F "0x08" // b3 is reserved.
#define  HF "0x10"
#define R2F "0x20" // b5 is reserved.
#define  ZF "0x40"
#define  SF "0x80"
	/* cf)          SZ-H-PNC (Z80)
	                     /
	                     V
	       -NIIODIT SZ-A-P-C (i386)
	        TOO
	         PP
	         LL */

#define ADDn(dst, n) "add $" #n "," dst NL
#define SUBn(dst, n) "sub $" #n "," dst NL

#define CLK1(clocks, cycles) \
	"add $" #clocks " +" Tw ",%ebp" NL
#define al2ST(dst) \
	"mov " "%al" "," dst NL
#define dh2ST(dst) \
	"mov " "%dh" "," dst NL
#define dl2ST(dst) \
	"mov " "%dl" "," dst NL
#define LD2dl(src) \
	"mov " src "," "%dl" NL
#define dx2cx \
	"mov " DX "," CX NL
#define LD2cx(src) \
	"mov " src "," CX NL
#define cx2ST(dst) \
	"mov " CX "," dst NL
#define LD2dx(src) \
	"mov " src "," DX NL
#define dx2ST(dst) \
	"mov " DX "," dst NL
#define LD2bx(src) \
	"mov " src "," BX NL
#define bx2ST(dst) \
	"mov " BX "," dst NL
#define SWAPdx(src) \
	"xchg " src "," DX NL

#define JPNOTPGBRK(ofs, reg) "cmp " EAPG_MIN "," reg NL \
                        /*   "jc other_page_" #id NL*/ ".byte 0x72, 0x05" NL \
                             "cmp " EAPG_MAX "," reg NL /* 3B 7E xx */ \
                        /*   "jc same_page_" #id NL*/ ".byte 0x72, 0x00 +" #ofs NL \
                        /*"other_page_" #id ":" NL*/ \
                             /* xx ... (<ofs> bytes) */ \
                        /*"same_page_" #id ":" NL*/
#define JPIFPGBRK(ofs, reg) "cmp " EAPG_MIN "," reg NL \
                       /*   "jc other_page_" #id NL*/ ".byte 0x72, 0x05 +" #ofs NL \
                            "cmp " EAPG_MAX "," reg NL /* 3B 7E xx */ \
                       /*   "jnc other_page_" #id NL*/ ".byte 0x73, 0x00 +" #ofs NL \
                            /* xx ... (<ofs> bytes) */ \
                       /*"other_page_" #id ":" NL*/

#define cx2VPC     "mov " CX "," PC NL \
                   PC2VA(PC) \
                   JPNOTPGBRK(5, VPC) \
                   "call " LC "SetPC" NL /* E8 xx xx xx xx */
#define cx2VPC_ret "mov " CX "," PC NL \
                   PC2VA(PC) \
                   JPIFPGBRK(1, VPC) \
                   "ret" NL /* C3 */ \
                   "jmp " LC "SetPC" LF
#define dx2VPC_ret "mov " DX "," PC NL \
                   PC2VA(PC) \
                   JPIFPGBRK(1, VPC) \
                   "ret" NL /* C3 */ \
						 "mov " DX "," CX NL \
                   "jmp " LC "SetPC" LF

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
		OPEND0(label)
#define OPEND0(label) \
		".cfi_endproc" NL \
		".size " LC #label ",.-" LC #label NL \
	);

LCFUNC(SetPC)
	"mov " CX "," PC NL
	"and $7 << 13," CX NL // 7 = (1 << 16 -13) -1
	"shr $13 -2," CX NL
	"mov " M "raw_or_rwfn + " M "mem(" CPU "," CX ",2)," EBX NL // 2 = 1 << log2of(memctl_s) -2
	"mov " EBX "," EAPG_MIN NL
	"add $1 << 13," EBX NL
	"mov " EBX "," EAPG_MAX NL
	"sub $1 << 13," EBX NL

	"shl $13 -2," CX NL
	"and $0x1FFF," PC NL // 0x1FFF = (1 << 13) -1
	"sub " EBX "," CX NL
	"add " EBX "," PC NL
	"mov " ECX "," VPC2PC_NEG NL
	"dec " VPC NL
LCEND(SetPC)

LCFUNC(cxREAD2dl)
	"mov " CX "," BX NL
	"shr $13 -2," CX NL
	"and $7 << 2," CX NL // 7 = (1 << 16 -13) -1
	"mov " M "raw_or_rwfn + " M "mem(" CPU "," CX ",2)," ECX NL // 2 = 1 << log2of(memctl_s) -2
	"and $0x1FFF," BX NL // 0x1FFF = (1 << 13) -1
	"mov (" ECX "," BX ",1),%dl" NL
LCEND(cxREAD2dl)

LCFUNC(cxREAD2dx)
	/* TODO: rwfn / iofn */
	"lea 1(" CX ")," DX NL
	"test $0x1FFF," DX NL // 0x1FFF = (1 << 13) -1
	"jz " LC "cxREAD2dx" "_ladder_two_pages" NL
	"shr $13 -2," CX NL
	"and $7 << 2," CX NL // 7 = (1 << 16 -13) -1
	"mov " CX "," BX NL
	"mov " M "raw_or_rwfn + " M "mem(" CPU "," BX ",2)," ECX NL // 2 = 1 << log2of(memctl_s) -2
//LC "cxREAD2dx" "_raw:" NL
	"and $0x1FFF," DX NL // 0x1FFF = (1 << 13) -1
	"movzwl -1(" ECX "," DX ",1)," DX NL
	"ret" LF
LC "cxREAD2dx" "_ladder_two_pages:" NL
	"push " CX NL
	"call " LC "cxREAD2dl" NL
	"pop " CX NL
	"inc " CX NL
	"push " DX NL
	"call " LC "cxREAD2dl" NL
	"pop " BX NL
	"shl $8," DX NL
	"or " BX "," DX NL
LCEND(cxREAD2dx)

// broken: ebx
LCFUNC(dl2WRITEcx)
	"mov " CX "," BX NL
	"shr $13 -2," CX NL
	"and $7 << 2," CX NL // 7 = (1 << 16 -13) -1
	"mov " M "raw_or_rwfn + " M "mem(" CPU "," CX ",2)," ECX NL // 2 = 1 << log2of(memctl_s) -2
	"and $0x1FFF," BX NL // 0x1FFF = (1 << 13) -1
	"mov %dl,(" ECX "," BX ",1)" NL
LCEND(dl2WRITEcx)

// broken: ebx edx
LCFUNC(dx2WRITEcx)
	"jz " LC "dx2WRITEcx" "_ladder_two_pages" NL
	"lea 1(" CX ")," BX NL // b:addr+1(b15..b0)
	"test $0x1FFF," BX NL // 0x1FFF = (1 << 13) -1
	"jz " LC "dx2WRITEcx" "_ladder_two_pages" NL
	"shr $13 -2," CX NL
	"and $7 << 2," CX NL // 7 = (1 << 16 -13) -1
	"mov " M "raw_or_rwfn + " M "mem(" CPU "," CX ",2)," ECX NL // 2 = 1 << log2of(memctl_s) -2
//LC "dx2WRITEcx" "_raw:" NL
	"and $0x1FFF," BX NL // 0x1FFF = (1 << 13) -1
	"mov %dx,-1(" ECX "," BX ",1)" NL
	"ret" LF
LC "dx2WRITEcx" "_ladder_two_pages:" NL
	"push " DX NL
	"push " CX NL
	"call " LC "dl2WRITEcx" NL
	"pop " CX NL
	"inc " CX NL
	"pop " DX NL
	"shr $8," DX NL
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

// CAUTION: (*2)
#define FETCH2dl(label) \
	/* TODO: mem[] */ \
	"inc " VPC NL \
	"movzbl (" VPC ")," "%edx" NL
// CAUTION: (*2)
#define FETCH2dl2sdx \
	/* TODO: mem[] */ \
	"inc " VPC NL \
	"movsbl (" VPC ")," "%edx" NL
#define FETCH2dx(label) \
	/* TODO: mem[] */ \
	"add $2," VPC NL \
	"movzwl -1(" VPC ")," "%edx" NL

#define SET2ah(mask) \
	"or $(" mask "),%ah" NL
#define RES2ah(mask) \
	"and $(0xff - (" mask ")),%ah" NL

#define sdx2JR \
	/* TODO: mem[] */ \
	"add " EDX "," VPC NL \
	"ret" NL
//
#define F2ah \
	"xor " UND_F ",%ah" NL \
	"and $(0xff - (" R1F "+" R2F ")),%ah" NL \
	"xor " UND_F ",%ah" NL
#define AF2dx \
	"mov " UND_F ",%dl" NL \
	"mov " A ",%dh" NL \
	"xor %ah" ",%dl" NL \
	"and $(" R1F "+" R2F "),%dl" NL \
	"xor %ah" ",%dl" NL
#define dx2AF \
	"mov %dl,%ah" NL \
	"mov %dl," UND_F NL \
	"mov %dh," A NL
#define Vf2cl2ah \
	"seto %cl" NL RES2ah(VF "+" NF) "shl $2,%cl" NL "or %cl,%ah" NL
#define ah2fSZHNC \
	"sahf" NL
#define SZHNCf2ah \
	"lahf" NL
#define MADD_A(reg) \
	                 "add " reg "," A NL al2ST(UND_F) SZHNCf2ah Vf2cl2ah
#define MADC_A(reg) \
	"ror %ah" NL     "adc " reg "," A NL al2ST(UND_F) SZHNCf2ah Vf2cl2ah
#define MSUB(reg) \
	                 "sub " reg "," A NL al2ST(UND_F) SZHNCf2ah Vf2cl2ah SET2ah(NF)
#define MSBC_A(reg) \
	"ror %ah" NL     "sbb " reg "," A NL al2ST(UND_F) SZHNCf2ah Vf2cl2ah SET2ah(NF)
#define MAND(reg) \
	                 "and " reg "," A NL al2ST(UND_F) SZHNCf2ah RES2ah(NF) SET2ah(HF)
#define MXOR(reg) \
	                 "xor " reg "," A NL al2ST(UND_F) SZHNCf2ah RES2ah(NF "+" HF)
#define MOR(reg) \
	                 "or "  reg "," A NL al2ST(UND_F) SZHNCf2ah RES2ah(NF "+" HF)
#define MCP(reg) \
	"mov " reg ",%dh" NL "cmp %dh," A NL dh2ST(UND_F) SZHNCf2ah Vf2cl2ah SET2ah(NF)
#define ah2INCdl \
	ah2fSZHNC "inc %dl" NL dl2ST(UND_F) SZHNCf2ah Vf2cl2ah
#define ah2DECdl \
	ah2fSZHNC "dec %dl" NL dl2ST(UND_F) SZHNCf2ah Vf2cl2ah SET2ah(NF)
#define ah2ADDdx(h,l) \
	"mov %ah,%cl" NL "add " l ",%dl" NL "adc " h ",%dh" NL \
	SZHNCf2ah "and $(0xff - (" HF "+" NF "+" CF ")),%cl" NL "and $(" HF "+" CF "),%ah" NL "or %cl,%ah" NL

#define MENDIF(label) LC #label "_unmatch:" NL
#define MELSE0 MENDIF
#define MIFNZ(label) "test $" ZF ",%ah" NL "jnz " LC #label "_unmatch" NL
#define  MIFZ(label) "test $" ZF ",%ah" NL "jz  " LC #label "_unmatch" NL
#define MIFNC(label) "test $" CF ",%ah" NL "jnz " LC #label "_unmatch" NL
#define  MIFC(label) "test $" CF ",%ah" NL "jz  " LC #label "_unmatch" NL
#define MIFPO(label) "test $" VF ",%ah" NL "jnz " LC #label "_unmatch" NL
#define MIFPE(label) "test $" VF ",%ah" NL "jz  " LC #label "_unmatch" NL
#define  MIFP(label) "test $" SF ",%ah" NL "jnz " LC #label "_unmatch" NL
#define  MIFM(label) "test $" SF ",%ah" NL "jz  " LC #label "_unmatch" NL

OPFUNC(DAA)
	"mov %ah,%cl" NL
	"and $" NF ",%cl" NL
	"jnz " LC "DAA" "_after_sub" NL
//LC "DAA" "_after_add:" NL
	ah2fSZHNC
	"daa" NL
	"jmp " LC "DAA" "_NF_recovery" LF
LC "DAA" "_after_sub:" NL
	ah2fSZHNC
	"das" LF
LC "DAA" "_NF_recovery:" NL
	al2ST(UND_F)
	SZHNCf2ah
	"and $(0xff - " NF "),%ah" NL
	"or %cl,%ah" NL
	CLK1(4,1)
OPEND(DAA)
OPFUNC(CPL) "not " A NL al2ST(UND_F) "or $(" HF "+" NF "),%ah" NL CLK1(4,1) OPEND(CPL)
OPFUNC(SCF) al2ST(UND_F) "or $" CF ",%ah" NL "and $(0xff - (" HF "+" NF ")),%ah" NL CLK1(4,1) OPEND(SCF)
OPFUNC(CCF)
	al2ST(UND_F)
	"mov %ah,%dl" NL
	"and $(0xff - (" NF "+" HF ")),%ah" NL
	"and $" CF ",%dl" NL
	"xor $" CF ",%ah" NL
	"shl $4,%dl" NL
	"or %dl,%ah" NL
	CLK1(4,1)
OPEND(CCF)

OPFUNC(RLCA) ah2fSZHNC "rol $1," A NL al2ST(UND_F) SZHNCf2ah "and $(0xff - (" HF "+" NF ")),%ah" NL CLK1(4,1) OPEND(RLCA)
OPFUNC(RRCA) ah2fSZHNC "ror $1," A NL al2ST(UND_F) SZHNCf2ah "and $(0xff - (" HF "+" NF ")),%ah" NL CLK1(4,1) OPEND(RRCA)
OPFUNC(RLA)  ah2fSZHNC "rcl $1," A NL al2ST(UND_F) SZHNCf2ah "and $(0xff - (" HF "+" NF ")),%ah" NL CLK1(4,1) OPEND(RLA) 
OPFUNC(RRA)  ah2fSZHNC "rcr $1," A NL al2ST(UND_F) SZHNCf2ah "and $(0xff - (" HF "+" NF ")),%ah" NL CLK1(4,1) OPEND(RRA) 

OPFUNC(JR)                 FETCH2dl2sdx CLK1(12,3) sdx2JR                                        OPEND(JR)    // (4+Tw,3,5)
OPFUNC(JR_NZ) MIFNZ(JR_NZ) FETCH2dl2sdx CLK1(12,3) sdx2JR MELSE0(JR_NZ) CLK1(7,2) "inc " VPC NL OPEND(JR_NZ) // (4+Tw,3[,5])
OPFUNC(JR_Z)  MIFZ (JR_Z)  FETCH2dl2sdx CLK1(12,3) sdx2JR MELSE0(JR_Z)  CLK1(7,2) "inc " VPC NL OPEND(JR_Z) 
OPFUNC(JR_NC) MIFNC(JR_NC) FETCH2dl2sdx CLK1(12,3) sdx2JR MELSE0(JR_NC) CLK1(7,2) "inc " VPC NL OPEND(JR_NC)
OPFUNC(JR_C)  MIFC (JR_C)  FETCH2dl2sdx CLK1(12,3) sdx2JR MELSE0(JR_C)  CLK1(7,2) "inc " VPC NL OPEND(JR_C) 


OPFUNC(JP)    FETCH2dx(JP) CLK1(10,3)                 dx2VPC_ret                                   OPEND(JP)    // (4+Tw,3,3)
OPFUNC(JP_NZ) CLK1(10,3) MIFNZ(JP_NZ) FETCH2dx(JP_NZ) dx2VPC_ret MELSE0(JP_NZ) ADDn(VPC,2) OPEND(JP_NZ) // (4+Tw,3,3)
OPFUNC(JP_Z)  CLK1(10,3) MIFZ (JP_Z)  FETCH2dx(JP_Z)  dx2VPC_ret MELSE0(JP_Z)  ADDn(VPC,2) OPEND(JP_Z) 
OPFUNC(JP_NC) CLK1(10,3) MIFNC(JP_NC) FETCH2dx(JP_NC) dx2VPC_ret MELSE0(JP_NC) ADDn(VPC,2) OPEND(JP_NC)
OPFUNC(JP_C)  CLK1(10,3) MIFC (JP_C)  FETCH2dx(JP_C)  dx2VPC_ret MELSE0(JP_C)  ADDn(VPC,2) OPEND(JP_C) 
OPFUNC(JP_PO) CLK1(10,3) MIFPO(JP_PO) FETCH2dx(JP_PO) dx2VPC_ret MELSE0(JP_PO) ADDn(VPC,2) OPEND(JP_PO)
OPFUNC(JP_PE) CLK1(10,3) MIFPE(JP_PE) FETCH2dx(JP_PE) dx2VPC_ret MELSE0(JP_PE) ADDn(VPC,2) OPEND(JP_PE)
OPFUNC(JP_P)  CLK1(10,3) MIFP (JP_P)  FETCH2dx(JP_P)  dx2VPC_ret MELSE0(JP_P)  ADDn(VPC,2) OPEND(JP_P) 
OPFUNC(JP_M)  CLK1(10,3) MIFM (JP_M)  FETCH2dx(JP_M)  dx2VPC_ret MELSE0(JP_M)  ADDn(VPC,2) OPEND(JP_M) 
OPFUNC(JP_HL) LD2dx(HL) CLK1(4,1) dx2VPC_ret OPEND(JP_HL) // (4+Tw)

OPFUNC(DJNZ)
	"mov " B ",%dl" NL
	"dec %dl" NL
	"jz " LC "DJNZ" "_unmatch" NL
	"mov %dl," B NL
	FETCH2dl2sdx CLK1(13,3) sdx2JR MELSE0(DJNZ) CLK1(8,2)
	"mov %dl," B NL
OPEND(DJNZ) // (5+Tw,3[,5])

OPFUNC(RST_00) CLK1(11,3) "mov " VPC "," EDX NL LD2cx(SP) "inc " EDX NL SUBn(CX,2) VA2PC(EDX) cx2ST(SP) dx2WRITEcx "mov $0x0000," CX NL cx2VPC_ret OPEND0(RST_00) // (5+Tw,3,3)
OPFUNC(RST_08) CLK1(11,3) "mov " VPC "," EDX NL LD2cx(SP) "inc " EDX NL SUBn(CX,2) VA2PC(EDX) cx2ST(SP) dx2WRITEcx "mov $0x0008," CX NL cx2VPC_ret OPEND0(RST_08)
OPFUNC(RST_10) CLK1(11,3) "mov " VPC "," EDX NL LD2cx(SP) "inc " EDX NL SUBn(CX,2) VA2PC(EDX) cx2ST(SP) dx2WRITEcx "mov $0x0010," CX NL cx2VPC_ret OPEND0(RST_10)
OPFUNC(RST_18) CLK1(11,3) "mov " VPC "," EDX NL LD2cx(SP) "inc " EDX NL SUBn(CX,2) VA2PC(EDX) cx2ST(SP) dx2WRITEcx "mov $0x0018," CX NL cx2VPC_ret OPEND0(RST_18)
OPFUNC(RST_20) CLK1(11,3) "mov " VPC "," EDX NL LD2cx(SP) "inc " EDX NL SUBn(CX,2) VA2PC(EDX) cx2ST(SP) dx2WRITEcx "mov $0x0020," CX NL cx2VPC_ret OPEND0(RST_20)
OPFUNC(RST_28) CLK1(11,3) "mov " VPC "," EDX NL LD2cx(SP) "inc " EDX NL SUBn(CX,2) VA2PC(EDX) cx2ST(SP) dx2WRITEcx "mov $0x0028," CX NL cx2VPC_ret OPEND0(RST_28)
OPFUNC(RST_30) CLK1(11,3) "mov " VPC "," EDX NL LD2cx(SP) "inc " EDX NL SUBn(CX,2) VA2PC(EDX) cx2ST(SP) dx2WRITEcx "mov $0x0030," CX NL cx2VPC_ret OPEND0(RST_30)
OPFUNC(RST_38) CLK1(11,3) "mov " VPC "," EDX NL LD2cx(SP) "inc " EDX NL SUBn(CX,2) VA2PC(EDX) cx2ST(SP) dx2WRITEcx "mov $0x0038," CX NL cx2VPC_ret OPEND0(RST_38)

OPFUNC(CALL) FETCH2dx(CALL) CLK1(17,5) SWAPdx(VPC) LD2cx(SP) "inc " EDX NL SUBn(CX,2) VA2PC(EDX) cx2ST(SP) dx2WRITEcx SWAPdx(PC) dx2VPC_ret OPEND(CALL) // (4+Tw,3,4,3,3)
OPFUNC(CALL_NZ) MIFNZ(CALL_NZ) FETCH2dx(CALL_NZ) CLK1(17,5) SWAPdx(VPC) LD2cx(SP) "inc " EDX NL SUBn(CX,2) VA2PC(EDX) cx2ST(SP) dx2WRITEcx SWAPdx(PC) dx2VPC_ret MELSE0(CALL_NZ) CLK1(10,3) ADDn(VPC,2) OPEND(CALL_NZ) // (4+Tw,3,4,3,3) (4+Tw,3,3)
OPFUNC(CALL_Z)  MIFZ (CALL_Z)  FETCH2dx(CALL_Z)  CLK1(17,5) SWAPdx(VPC) LD2cx(SP) "inc " EDX NL SUBn(CX,2) VA2PC(EDX) cx2ST(SP) dx2WRITEcx SWAPdx(PC) dx2VPC_ret MELSE0(CALL_Z)  CLK1(10,3) ADDn(VPC,2) OPEND(CALL_Z)
OPFUNC(CALL_NC) MIFNC(CALL_NC) FETCH2dx(CALL_NC) CLK1(17,5) SWAPdx(VPC) LD2cx(SP) "inc " EDX NL SUBn(CX,2) VA2PC(EDX) cx2ST(SP) dx2WRITEcx SWAPdx(PC) dx2VPC_ret MELSE0(CALL_NC) CLK1(10,3) ADDn(VPC,2) OPEND(CALL_NC)
OPFUNC(CALL_C)  MIFC (CALL_C)  FETCH2dx(CALL_C)  CLK1(17,5) SWAPdx(VPC) LD2cx(SP) "inc " EDX NL SUBn(CX,2) VA2PC(EDX) cx2ST(SP) dx2WRITEcx SWAPdx(PC) dx2VPC_ret MELSE0(CALL_C)  CLK1(10,3) ADDn(VPC,2) OPEND(CALL_C)
OPFUNC(CALL_PO) MIFPO(CALL_PO) FETCH2dx(CALL_PO) CLK1(17,5) SWAPdx(VPC) LD2cx(SP) "inc " EDX NL SUBn(CX,2) VA2PC(EDX) cx2ST(SP) dx2WRITEcx SWAPdx(PC) dx2VPC_ret MELSE0(CALL_PO) CLK1(10,3) ADDn(VPC,2) OPEND(CALL_PO)
OPFUNC(CALL_PE) MIFPE(CALL_PE) FETCH2dx(CALL_PE) CLK1(17,5) SWAPdx(VPC) LD2cx(SP) "inc " EDX NL SUBn(CX,2) VA2PC(EDX) cx2ST(SP) dx2WRITEcx SWAPdx(PC) dx2VPC_ret MELSE0(CALL_PE) CLK1(10,3) ADDn(VPC,2) OPEND(CALL_PE)
OPFUNC(CALL_P)  MIFP (CALL_P)  FETCH2dx(CALL_P)  CLK1(17,5) SWAPdx(VPC) LD2cx(SP) "inc " EDX NL SUBn(CX,2) VA2PC(EDX) cx2ST(SP) dx2WRITEcx SWAPdx(PC) dx2VPC_ret MELSE0(CALL_P)  CLK1(10,3) ADDn(VPC,2) OPEND(CALL_P) 
OPFUNC(CALL_M)  MIFM (CALL_M)  FETCH2dx(CALL_M)  CLK1(17,5) SWAPdx(VPC) LD2cx(SP) "inc " EDX NL SUBn(CX,2) VA2PC(EDX) cx2ST(SP) dx2WRITEcx SWAPdx(PC) dx2VPC_ret MELSE0(CALL_M)  CLK1(10,3) ADDn(VPC,2) OPEND(CALL_M) 

OPFUNC(RET)                  CLK1(10,3) LD2cx(SP) cxREAD2dx ADDn(SP,2) dx2VPC_ret OPEND(RET) // (4+Tw,3,3)
OPFUNC(RET_NZ) MIFNZ(RET_NZ) CLK1(11,3) LD2cx(SP) cxREAD2dx ADDn(SP,2) dx2VPC_ret MELSE0(RET_NZ) CLK1(5,3) OPEND(RET_NZ) // (5+Tw,3,3)
OPFUNC(RET_Z)  MIFZ (RET_Z)  CLK1(11,3) LD2cx(SP) cxREAD2dx ADDn(SP,2) dx2VPC_ret MELSE0(RET_Z)  CLK1(5,3) OPEND(RET_Z)
OPFUNC(RET_NC) MIFNC(RET_NC) CLK1(11,3) LD2cx(SP) cxREAD2dx ADDn(SP,2) dx2VPC_ret MELSE0(RET_NC) CLK1(5,3) OPEND(RET_NC)
OPFUNC(RET_C)  MIFC (RET_C)  CLK1(11,3) LD2cx(SP) cxREAD2dx ADDn(SP,2) dx2VPC_ret MELSE0(RET_C)  CLK1(5,3) OPEND(RET_C)
OPFUNC(RET_PO) MIFPO(RET_PO) CLK1(11,3) LD2cx(SP) cxREAD2dx ADDn(SP,2) dx2VPC_ret MELSE0(RET_PO) CLK1(5,3) OPEND(RET_PO)
OPFUNC(RET_PE) MIFPE(RET_PE) CLK1(11,3) LD2cx(SP) cxREAD2dx ADDn(SP,2) dx2VPC_ret MELSE0(RET_PE) CLK1(5,3) OPEND(RET_PE)
OPFUNC(RET_P)  MIFP (RET_P)  CLK1(11,3) LD2cx(SP) cxREAD2dx ADDn(SP,2) dx2VPC_ret MELSE0(RET_P)  CLK1(5,3) OPEND(RET_P)
OPFUNC(RET_M)  MIFM (RET_M)  CLK1(11,3) LD2cx(SP) cxREAD2dx ADDn(SP,2) dx2VPC_ret MELSE0(RET_M)  CLK1(5,3) OPEND(RET_M)

OPFUNC(ADD_HL_BC) LD2dx(HL) ah2ADDdx(B,C)     dl2ST(L) CLK1(11,3) dh2ST(H) OPEND(ADD_HL_BC) // (4+Tw,4,3)
OPFUNC(ADD_HL_DE) LD2dx(HL) ah2ADDdx(D,E)     dl2ST(L) CLK1(11,3) dh2ST(H) OPEND(ADD_HL_DE)
OPFUNC(ADD_HL_HL) LD2dx(HL) ah2ADDdx(H,L)     dl2ST(L) CLK1(11,3) dh2ST(H) OPEND(ADD_HL_HL)
OPFUNC(ADD_HL_SP) LD2dx(HL) ah2ADDdx(SPH,SPL) dl2ST(L) CLK1(11,3) dh2ST(H) OPEND(ADD_HL_SP)

OPFUNC(INC_BC) LD2dx(BC) "inc " DX NL CLK1(6,1) dx2ST(BC) OPEND(INC_BC) // (6+Tw)
OPFUNC(INC_DE) LD2dx(DE) "inc " DX NL CLK1(6,1) dx2ST(DE) OPEND(INC_DE)
OPFUNC(INC_HL) LD2dx(HL) "inc " DX NL CLK1(6,1) dx2ST(HL) OPEND(INC_HL)
OPFUNC(INC_SP) LD2dx(SP) "inc " DX NL CLK1(6,1) dx2ST(SP) OPEND(INC_SP)

OPFUNC(DEC_BC) LD2dx(BC) "dec " DX NL CLK1(6,1) dx2ST(BC) OPEND(DEC_BC) // (6+Tw)
OPFUNC(DEC_DE) LD2dx(DE) "dec " DX NL CLK1(6,1) dx2ST(DE) OPEND(DEC_DE)
OPFUNC(DEC_HL) LD2dx(HL) "dec " DX NL CLK1(6,1) dx2ST(HL) OPEND(DEC_HL)
OPFUNC(DEC_SP) LD2dx(SP) "dec " DX NL CLK1(6,1) dx2ST(SP) OPEND(DEC_SP)

OPFUNC(EX_AF_AF) AF2dx SWAPdx(ALTAF) NL dx2AF CLK1(4,1) OPEND(EX_AF_AF) // (4+Tw)
OPFUNC(EX_DE_HL) LD2dx(DE) LD2bx(HL) CLK1(4,1) dx2ST(HL) bx2ST(DE) OPEND(EX_DE_HL)
OPFUNC(EXX)
	LD2dx(ALTBC) SWAPdx(BC) dx2ST(ALTBC)
	LD2dx(ALTDE) SWAPdx(DE) dx2ST(ALTDE)
	LD2dx(ALTHL) SWAPdx(HL) dx2ST(ALTHL)
	CLK1(4,1)
OPEND(EXX)

OPFUNC(NOP) CLK1(4,1) OPEND(NOP) // (4+Tw)

#define OPALU(fn) \
	OPFUNC(fn##_B) M##fn(B) CLK1(4,1) OPEND(fn##_B) /* (4+Tw) */ \
	OPFUNC(fn##_C) M##fn(C) CLK1(4,1) OPEND(fn##_C) \
	OPFUNC(fn##_D) M##fn(D) CLK1(4,1) OPEND(fn##_D) \
	OPFUNC(fn##_E) M##fn(E) CLK1(4,1) OPEND(fn##_E) \
	OPFUNC(fn##_H) M##fn(H) CLK1(4,1) OPEND(fn##_H) \
	OPFUNC(fn##_L) M##fn(L) CLK1(4,1) OPEND(fn##_L) \
	OPFUNC(fn##_p) LD2cx(HL) cxREAD2dl M##fn("%dl") CLK1(7,2) OPEND(fn##_p) /* (4+Tw,3) */ \
	OPFUNC(fn##_A) M##fn(A) CLK1(4,1) OPEND(fn##_A) \
	OPFUNC(fn##_N) FETCH2dl(fn##_N) M##fn("%dl") CLK1(7,2) OPEND(fn##_N) /* (4+Tw,3) */

OPALU(ADD_A) OPALU(ADC_A) OPALU(SUB) OPALU(SBC_A)
OPALU(AND)   OPALU(XOR)   OPALU(OR)  OPALU(CP)

OPFUNC(INC_B) LD2dl(B) ah2INCdl dl2ST(B) CLK1(4,1) OPEND(INC_B) // (4+Tw)
OPFUNC(INC_C) LD2dl(C) ah2INCdl dl2ST(C) CLK1(4,1) OPEND(INC_C)
OPFUNC(INC_D) LD2dl(D) ah2INCdl dl2ST(D) CLK1(4,1) OPEND(INC_D)
OPFUNC(INC_E) LD2dl(E) ah2INCdl dl2ST(E) CLK1(4,1) OPEND(INC_E)
OPFUNC(INC_H) LD2dl(H) ah2INCdl dl2ST(H) CLK1(4,1) OPEND(INC_H)
OPFUNC(INC_L) LD2dl(L) ah2INCdl dl2ST(L) CLK1(4,1) OPEND(INC_L)
OPFUNC(INC_p) LD2cx(HL) "push " CX NL cxREAD2dl ah2INCdl "pop " CX NL dl2WRITEcx CLK1(11,1) OPEND(INC_p) // (4+Tw,4,3)
OPFUNC(INC_A) LD2dl(A) ah2INCdl dl2ST(A) CLK1(4,1) OPEND(INC_A)

OPFUNC(DEC_B) LD2dl(B) ah2DECdl dl2ST(B) CLK1(4,1) OPEND(DEC_B) // (4+Tw)
OPFUNC(DEC_C) LD2dl(C) ah2DECdl dl2ST(C) CLK1(4,1) OPEND(DEC_C)
OPFUNC(DEC_D) LD2dl(D) ah2DECdl dl2ST(D) CLK1(4,1) OPEND(DEC_D)
OPFUNC(DEC_E) LD2dl(E) ah2DECdl dl2ST(E) CLK1(4,1) OPEND(DEC_E)
OPFUNC(DEC_H) LD2dl(H) ah2DECdl dl2ST(H) CLK1(4,1) OPEND(DEC_H)
OPFUNC(DEC_L) LD2dl(L) ah2DECdl dl2ST(L) CLK1(4,1) OPEND(DEC_L)
OPFUNC(DEC_p) LD2cx(HL) "push " CX NL cxREAD2dl ah2DECdl "pop " CX NL dl2WRITEcx CLK1(11,1) OPEND(DEC_p) // (4+Tw,4,3)
OPFUNC(DEC_A) LD2dl(A) ah2DECdl dl2ST(A) CLK1(4,1) OPEND(DEC_A)

OPFUNC(PUSH_BC) LD2dx(BC) LD2cx(SP) SUBn(CX,2) cx2ST(SP) dx2WRITEcx CLK1(11,3) OPEND(PUSH_BC) // (5+Tw,3,3)
OPFUNC(PUSH_DE) LD2dx(DE) LD2cx(SP) SUBn(CX,2) cx2ST(SP) dx2WRITEcx CLK1(11,3) OPEND(PUSH_DE)
OPFUNC(PUSH_HL) LD2dx(HL) LD2cx(SP) SUBn(CX,2) cx2ST(SP) dx2WRITEcx CLK1(11,3) OPEND(PUSH_HL)
OPFUNC(PUSH_AF) AF2dx     LD2cx(SP) SUBn(CX,2) cx2ST(SP) dx2WRITEcx CLK1(11,3) OPEND(PUSH_AF)

OPFUNC(POP_BC) LD2cx(SP) cxREAD2dx ADDn(SP,2) dx2ST(BC) CLK1(10,3) OPEND(POP_BC) // (4+Tw,3,3)
OPFUNC(POP_DE) LD2cx(SP) cxREAD2dx ADDn(SP,2) dx2ST(DE) CLK1(10,3) OPEND(POP_DE)
OPFUNC(POP_HL) LD2cx(SP) cxREAD2dx ADDn(SP,2) dx2ST(HL) CLK1(10,3) OPEND(POP_HL)
OPFUNC(POP_AF) LD2cx(SP) cxREAD2dx ADDn(SP,2) dx2AF     CLK1(10,3) OPEND(POP_AF)

OPFUNC(LD_BC_NN) FETCH2dx(LD_BC_NN) dx2ST(BC) CLK1(10,1) OPEND(LD_BC_NN) // (4+Tw,3,3)
OPFUNC(LD_DE_NN) FETCH2dx(LD_DE_NN) dx2ST(DE) CLK1(10,1) OPEND(LD_DE_NN)
OPFUNC(LD_HL_NN) FETCH2dx(LD_HL_NN) dx2ST(HL) CLK1(10,1) OPEND(LD_HL_NN)
OPFUNC(LD_SP_NN) FETCH2dx(LD_SP_NN) dx2ST(SP) CLK1(10,1) OPEND(LD_SP_NN)

OPFUNC(LD_SP_HL) LD2dx(HL) CLK1(6,1) dx2ST(SP) OPEND(LD_SP_HL) // (6+Tw)

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

OPFUNC(DI)
	CLK1(4,1)
	"xor %cl,%cl" NL
	"mov %cl," IFF1 NL
	"mov %cl," IFF2 NL
OPEND(DI) // (4+Tw)

OPFUNC(EI)
	CLK1(4,1)
	"orb $0x80," IFF1 NL
OPEND(EI) // (4+Tw)

__asm__ (
	".section .rodata" NL
	".type " LC "z80_opcode" ",@object" LF
LC "z80_opcode:" NL
	".long " OP     "NOP,"  OP  "LD_BC_NN," OP "LD_BC_A,"   OP "INC_BC," OP "INC_B," OP "DEC_B," OP "LD_B_N," OP "RLCA" NL
	".long " OP "EX_AF_AF," OP "ADD_HL_BC," OP "LD_A_BC,"   OP "DEC_BC," OP "INC_C," OP "DEC_C," OP "LD_C_N," OP "RRCA" NL
	".long " OP    "DJNZ,"  OP  "LD_DE_NN," OP "LD_DE_A,"   OP "INC_DE," OP "INC_D," OP "DEC_D," OP "LD_D_N," OP "RLA" NL
	".long " OP    "JR,"    OP "ADD_HL_DE," OP "LD_A_DE,"   OP "DEC_DE," OP "INC_E," OP "DEC_E," OP "LD_E_N," OP "RRA" NL
	".long " OP    "JR_NZ," OP  "LD_HL_NN," OP "LD_pNN_HL," OP "INC_HL," OP "INC_H," OP "DEC_H," OP "LD_H_N," OP "DAA" NL
	".long " OP    "JR_Z,"  OP "ADD_HL_HL," OP "LD_HL_pNN," OP "DEC_HL," OP "INC_L," OP "DEC_L," OP "LD_L_N," OP "CPL" NL
	".long " OP    "JR_NC," OP  "LD_SP_NN," OP "LD_pNN_A,"  OP "INC_SP," OP "INC_p," OP "DEC_p," OP "LD_p_N," OP "SCF" NL
	".long " OP    "JR_C,"  OP "ADD_HL_SP," OP "LD_A_pNN,"  OP "DEC_SP," OP "INC_A," OP "DEC_A," OP "LD_A_N," OP "CCF" NL

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

	".long " OP "RET_NZ," OP   "POP_BC," OP "JP_NZ," OP       "JP," OP "CALL_NZ," OP "PUSH_BC," OP "ADD_A_N," OP "RST_00" NL
	".long " OP "RET_Z,"  OP      "RET," OP "JP_Z,"  OP      "NOP," OP "CALL_Z,"  OP    "CALL," OP "ADC_A_N," OP "RST_08" NL
	".long " OP "RET_NC," OP   "POP_DE," OP "JP_NC," OP      "NOP," OP "CALL_NC," OP "PUSH_DE," OP   "SUB_N," OP "RST_10" NL
	".long " OP "RET_C,"  OP      "EXX," OP "JP_C,"  OP      "NOP," OP "CALL_C,"  OP     "NOP," OP "SBC_A_N," OP "RST_18" NL
	".long " OP "RET_PO," OP   "POP_HL," OP "JP_PO," OP      "NOP," OP "CALL_PO," OP "PUSH_HL," OP   "AND_N," OP "RST_20" NL
	".long " OP "RET_PE," OP    "JP_HL," OP "JP_PE," OP "EX_DE_HL," OP "CALL_PE," OP     "NOP," OP   "XOR_N," OP "RST_28" NL
	".long " OP "RET_P,"  OP   "POP_AF," OP "JP_P,"  OP       "DI," OP  "CALL_P," OP "PUSH_AF," OP    "OR_N," OP "RST_30" NL
	".long " OP "RET_M,"  OP "LD_SP_HL," OP "JP_M,"  OP       "EI," OP  "CALL_M," OP     "NOP," OP    "CP_N," OP "RST_38" NL

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
	"push " VPC NL
	".cfi_def_cfa_offset 24" NL


	"mov 24+0(%esp)," CPU NL
	"mov " PC_ORIG "," CX NL
	cx2VPC
	"mov " FA "," AX NL
	"xor " CLK "," CLK LF
LC "z80_exec_loop:"
	FETCH2dl(z80_exec)
	"call *" LC "z80_opcode(," DX ",4)" NL

	"cmp 24+4(%esp)," CLK NL
	"jc " LC "z80_exec_loop" NL
	F2ah
	"mov " AX "," FA NL
	"inc " VPC NL
	VA2PC(VPC)
	"mov " PC "," PC_ORIG NL
	"mov " CLK "," AX NL
	
	"pop " VPC NL
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

#endif //def Ei386
