#ifndef Z80_H_INCLUDED__
#define Z80_H_INCLUDED__

struct z80 {
	uint8_t priv[112]; // gcc's value on i386
};
bool z80_init (struct z80 *this_, size_t cb);
unsigned __attribute__((stdcall)) z80_exec (struct z80 *this_, unsigned min_clocks);

#endif //def Z80_H_INCLUDED__
