#include <stdint.h>
#include <stddef.h>
#include <endian.h>

typedef uint32_t u32;
typedef  uint8_t u8;
typedef uint16_t u16;

#include "portab.h"

u16 load_le16 (const void *src)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return *(const u16 *)src;
#else
const u8 *p;
	p = (const u8 *)src;
	return p[0]|p[1]<<8;
#endif
}

size_t store_le16 (void *dst, u16 val)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	*(u16 *)dst = val;
#else
	*(u16 *)dst = (u16)(0xff & val >> 8 | 0xff00 & val << 8);
#endif
	return sizeof(u16);
}
