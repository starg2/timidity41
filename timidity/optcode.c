/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef STDC_HEADERS
#  include <string.h>
#elif defined(HAVE_STRINGS_H)
#  include <strings.h>
#endif /* stdc */

#include "timidity.h"
#include "common.h"

const char *arch_string =
#ifdef IX64CPU
	#if USE_X64_EXT_INTRIN == 9
		"[x64 AVX2]"
	#elif USE_X64_EXT_INTRIN == 8
		"[x64 AVX]"
	#elif USE_X64_EXT_INTRIN == 7
		"[x64 SSE4.2]"
	#elif USE_X64_EXT_INTRIN == 6
		"[x64 SSE4.1]"
	#elif USE_X64_EXT_INTRIN == 5
		"[x64 SSSE3]"
	#elif USE_X64_EXT_INTRIN == 4
		"[x64 SSE3]"
	#elif USE_X64_EXT_INTRIN == 3
		"[x64 SSE2]"
	#elif USE_X64_EXT_INTRIN == 2
		"[x64 SSE]"
	#elif USE_X64_EXT_INTRIN == 1
		"[x64 MMX]"
	#else
		"[x64]"
	#endif
#elif defined(IX86CPU)
	#if USE_X86_EXT_INTRIN == 9
		"[x86 AVX2]"
	#elif USE_X86_EXT_INTRIN == 8
		"[x86 AVX]"
	#elif USE_X86_EXT_INTRIN == 7
		"[x86 SSE4.2]"
	#elif USE_X86_EXT_INTRIN == 6
		"[x86 SSE4.1]"
	#elif USE_X86_EXT_INTRIN == 5
		"[x86 SSSE3]"
	#elif USE_X86_EXT_INTRIN == 4
		"[x86 SSE3]"
	#elif USE_X86_EXT_INTRIN == 3
		"[x86 SSE2]"
	#elif USE_X86_EXT_INTRIN == 2
		"[x86 SSE]"
	#elif USE_X86_EXT_INTRIN == 1
		"[x86 MMX]"
	#else
		"[x86]"
	#endif
#else
	""
#endif
;


/*****************************************************************************/
#if USE_ALTIVEC
#define sizeof_vector 16

/* vector version of memset(). */
void v_memset(void *destp, int c, size_t len)
{
    register int32 dest = (int)destp;

    /* If it's worth using altivec code */
    if (len >= (sizeof_vector << 1)) { /* 32 byte */
        int i, xlen;
	int cccc[4] = { c, c, c, c, };
        vint32 *destv;
	vint32  pat = *(vint32*)cccc;

	/* first, write things to word boundary. */
	if ((xlen = (int)dest % sizeof_vector) != 0) {
	    libc_memset(destp, c, xlen);
	    len  -= xlen;
	    dest += xlen;
	}

	/* this is the maion loop. */
	destv = (vint32*) dest;
	xlen = len / sizeof_vector;
	for (i = 0; i < xlen; i++) {
  	    destv[i] = pat;
	}
	dest += i * sizeof_vector;
	len %= sizeof_vector;
    }

    /* write remaining few bytes. */
    libc_memset((void*)dest, 0, len);
}

/* a bit faster version. */
static vint32 vzero = (vint32)(0);
void v_memzero(void *destp, size_t len)
{
    register int32 dest = (int)destp;

    /* If it's worth using altivec code */
    if (len >= 2 * sizeof_vector) { /* 32 byte */
        int i, xlen;
        vint32 *destv;

	/* first, write things to word boundary. */
	if ((xlen = (int)dest % sizeof_vector) != 0) {
	    libc_memset(destp, 0, xlen);
	    len  -= xlen;
	    dest += xlen;
	}

	/* this is the maion loop. */
	destv = (vint32*) dest;
	xlen = len / sizeof_vector;
	for (i = 0; i < xlen; i++) {
  	    destv[i] = vzero;
	}
	dest += i * sizeof_vector;
	len %= sizeof_vector;
    }

    /* write remaining few bytes. */
    libc_memset((void*)dest, 0, len);
}


/* this is an alternate version of set_dry_signal() in reverb.c */
void v_set_dry_signal(void *destp, const int32 *buf, int32 n)
{
    int i = 0;
    if (n >= 8) {
        vint32 *dest = (vint32*) destp;
        const int32 n_div4 = divi_4(n);
	for (; i < n_div4; i++) {
            dest[i] = vec_add(dest[i], ((vint32*) buf)[i]);
	}
    }
    /* write remaining few bytes. */
    for (i *= 4; i < n; i++) {
        ((int32*) destp)[i] += buf[i];
    }
}
#endif /* USE_ALTIVEC */


/*****************************************************************************/
#if (defined(__BORLANDC__) && (__BORLANDC__ >= 1380))
int32 imuldiv8(int32 a, int32 b) {
	_asm {
		mov eax, a
		mov edx, b
		imul edx
		shr eax, 8
		shl edx, 24
		or  eax, edx
	}
}

int32 imuldiv16(int32 a, int32 b) {
	_asm {
		mov eax, a
		mov edx, b
		imul edx
		shr eax, 16
		shl edx, 16
		or  eax, edx
	}
}

int32 imuldiv24(int32 a, int32 b) {
	_asm {
		mov eax, a
		mov edx, b
		imul edx
		shr eax, 24
		shl edx, 8
		or  eax, edx
	}
}

int32 imuldiv28(int32 a, int32 b) {
	_asm {
		mov eax, a
		mov edx, b
		imul edx
		shr eax, 28
		shl edx, 4
		or  eax, edx
	}
}
#endif


/*****************************************************************************/
#if (USE_X86_EXT_ASM || USE_X86_EXT_INTRIN || USE_X86_AMD_EXT_ASM || USE_X86_AMD_EXT_INTRIN)

enum{
	X86_VENDER_INTEL=0,
	X86_VENDER_AMD,
	X86_VENDER_UNKNOWN,
};

static const char* x86_vendors[] = 
{
	"GenuineIntel",
	"AuthenticAMD",
	"Unknown     ",
};

// 拡張フラグ取得
static inline int64	xgetbv(int index)
{
#if (USE_X86_EXT_ASM || USE_X86_AMD_EXT_ASM)
	uint64 flg = 0;
	//_asm {
	//	xgetbv
	//	mov dword ptr [now], eax
	//	mov dword ptr [now+4], edx
	//}
	//return flg;
	return 0xFFFFFFFFFFFFFFFF; // asmでxgetbv index どこ・・わからんのでスルー
#elif (USE_X86_EXT_INTRIN || USE_X86_AMD_EXT_INTRIN)
	return _xgetbv(index);
#endif

}


// if x86ext available TRUE, else FALSE
int is_x86ext_available(void)
{
	const int ext_intel = (USE_X86_EXT_ASM > USE_X86_EXT_INTRIN) ? USE_X86_EXT_ASM : USE_X86_EXT_INTRIN;
	const int ext_amd = (USE_X86_AMD_EXT_ASM > USE_X86_AMD_EXT_INTRIN) ? USE_X86_AMD_EXT_ASM : USE_X86_AMD_EXT_INTRIN;
	int flg_intel = 0, flg_amd = 0; // available flg
	int vid;
	char vendor[16];
	int32 reg[4];
	uint32 cmd;
	uint32 flg1; // feature flg
	uint32 flg2; // more feature flg
	uint32 flg3; // extended feature flg
	uint32 flg4; // extended feature flg pg2

	memset(vendor, 0, sizeof(vendor));
	__cpuid(reg, 0);
	cmd = reg[0];
	((uint32*)vendor)[0] = reg[1];
	((uint32*)vendor)[1] = reg[3];
	((uint32*)vendor)[2] = reg[2];
	for(vid = 0; vid < X86_VENDER_UNKNOWN; ++vid){
		if(!memcmp(vendor, x86_vendors[vid], 12))
			break;
	}
	if(cmd >= 0x00000001){
		__cpuid(reg, 0x00000001);
		flg1 = reg[3];
		flg2 = reg[2];
	}
	__cpuid(reg, 0x80000000);
	cmd = reg[ 0 ];
	if(cmd >= 0x80000001){
		__cpuid(reg, 0x80000001);
		flg4 = reg[2];
		flg3 = reg[3];
	}
	switch(ext_intel){
	case X86_EXT_NONE:
		flg_intel = 1;
		break;
	case X86_MMX:
		flg_intel = (flg1 >> 23) & 1;
		break;
	case X86_SSE:
		flg_intel = (flg1 >> 25) & 1;
		break;
	case X86_SSE2:
		flg_intel = (flg1 >> 26) & 1;
		break;
	case X86_SSE3:
		flg_intel = (flg2 >> 0) & 1;
		break;
	case X86_SSSE3:
		flg_intel = (flg2 >> 9) & 1;
		break;
	case X86_SSE41:
		flg_intel = (flg2 >> 19) & 1;
		break;
	case X86_SSE42:
		flg_intel = (flg2 >> 20) & 1;
		break;
	case X86_AVX:
		if((xgetbv(0) & 6) == 6)
			flg_intel = (flg2 >> 28 ) & 1;
		else
			flg_intel = 0;
		break;
	case X86_AVX2:
		if((xgetbv(0) & 6) == 6)
			flg_intel = (flg2 >> 29 ) & 1;
		else
			flg_intel = 0;
		break;
	default:
		flg_intel = 0;
		break;
	}
	switch(ext_amd){
	case X86_AMD_EXT_NONE:
		flg_amd = 1;
		break;
	case X86_MMX_EXT:
		flg_amd = (flg3 >> 22) & 1;
		break;
	case X86_3DNOW:
		flg_amd = (flg3 >> 31) & 1;
		break;
	case X86_3DNOW_EX:
		flg_amd = (flg3 >> 30) & 1;
		break;
	case X86_3DNOW_PRO:
		flg_amd = 1; // ・・わからんのでスルー
		break;
	case X86_SSE4A:
		flg_amd = (flg4 >> 6) & 1;
		break;
	default:
		flg_amd = 0;
		break;
	}
	return (flg_intel && flg_amd) ? 1 : 0;
}


#endif // IX86CPU
