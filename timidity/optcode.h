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

#ifndef OPTCODE_H_INCLUDED
#define OPTCODE_H_INCLUDED 1

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmacro-redefined"
#endif

#if defined(_M_IX86) || defined(__i386__) || defined(__i386) || defined(_X86_) || defined(__X86__) || defined(__I86__)
#define IX86CPU 1
#endif

#if defined(_M_X64) || defined(_AMD64_) || defined(_X64_) || defined(__X64__) || defined(__x86_64__)
#define IX64CPU 1
#undef IX86CPU
#undef IA64CPU
#endif

#if defined(_IA64_) || defined(__IA64__) || defined(__I64__)
#define IA64CPU 1
#undef IX86CPU
#undef IX64CPU
#endif

/* optimizing mode */
/* 0: none         */
/* 1: x86 asm      */
/* *: x86_64 asm   */
/* *: ia64 asm     */
/* *: arm asm      */
/* *: arm64 asm    */
#ifndef OPT_MODE
#define OPT_MODE 1
#endif

#if OPT_MODE == 1 && !defined(IX86CPU)
#undef  OPT_MODE
#define OPT_MODE 0
#endif

/*
#if OPT_MODE == * && !defined(AMD64CPU)
#undef  OPT_MODE
#define OPT_MODE 0
#endif
*/

/*
#if OPT_MODE == * && !defined(IA64CPU)
#undef  OPT_MODE
#define OPT_MODE 0
#endif
*/

/*
#if OPT_MODE == * && !defined(ARMCPU)
#undef  OPT_MODE
#define OPT_MODE 0
#endif
*/

/*
#if OPT_MODE == * && !defined(ARM64CPU)
#undef  OPT_MODE
#define OPT_MODE 0
#endif
*/




/*****************************************************************************/
/*
intrinsic��g�p���Ă݂�e�X�g gcc�ł�g����炵����
CPU�̊g���@�\�̑Ή��̈Ⴂ,�r���h����asm/intrin�Ή��̈Ⴂ������̂�
arch_ext_asm/intrin��ʂɎw��ł���悤�ɂ���
asm/intrin���Ή��̏ꍇ asm��D�悵�Ďg�p����
x86_ext/x86_AMD_ext���Ή��̏ꍇ x86_AMD_ext��D�悵�Ďg�p����
intrin�͈ꕔ������x86/x64���ʂȂ̂� USE_X86_EXT_INTRIN��x64�ł�L���� 
 x86/x64��p���߂� USE_X64_EXT_INTRIN/IX64CPU���ŋ�� (gather��

����̏�����
1 OPT_MODE or USE_X86_AMD_EXT_ASM or USE_X64_AMD_EXT_ASM
2 OPT_MODE or USE_X86_EXT_ASM or USE_X64_EXT_ASM
3 USE_X64_AMD_EXT_INTRIN
4 USE_X64_EXT_INTRIN
5 USE_X86_AMD_EXT_INTRIN
6 USE_X86_EXT_INTRIN


���_
AMD�킩���E�E���Ԃ�Ⴄ�̂ŗv�C�� (���̂Ƃ���K�v�Ȃ����g���\���Ȃ�����
�Ή��@�\�`�F�b�N ���낢������� (optcode.c is_x86ext_available() ���g�p
OPT_MODE�Ƃ̊֌W��ǂ����邩�E�E (���̂Ƃ���OPT_MODE�D��
 �܂Ƃ߂�Ȃ�, 1: x86 asm / no intrin �ɂ��Ĉȉ����炷, intrin��Ή��ɂȂ�����ύX, _EXT��_OPT�ɕύX �Ƃ�
AVX2�ȏ�̃r���h�����Ȃ��̂� ����͕s�� (VC2013?�ȍ~

*/
#define USE_PENTIUM_4 // for pentium 4 (northwood steppingA) float/double denormal fix

#if !defined(IX86CPU)
#undef USE_PENTIUM_4
#endif

//#define USE_SSE // �e�X�g�p
//#define USE_SSE2 // �e�X�g�p
//#define USE_SSE3 // �e�X�g�p
//#define USE_SSSE3 // �e�X�g�p
//#define USE_SSE41 // �e�X�g�p
//#define USE_SSE42 // �e�X�g�p
//#define USE_AVX // �e�X�g�p
//#define USE_AVX2 // �e�X�g�p
//#define USE_AVX512 // �e�X�g�p

/* x86 extension define */
/* 
  �g�p����g���@�\��w�肷�� (���ʂ̊g���@�\��܂�
  USE_MMX
  USE_MMX2
  USE_SSE // include MMX2
  USE_SSE2
  USE_SSE3
  USE_SSSE3
  USE_SSE41 (SSE4.1
  USE_SSE42 (SSE4.2 // include POPCNT
  USE_SSE4 (SSE4.1 +SSE4.2
  USE_AVX  // include PCLMULQDQ
  USE_AVX2 // include FMA BMI1 BMI2 F16C RDRAND
  USE_AVX512 // F, CD, VL, DQ, BW
*/
/* x86 AMD extension define */
/*	
  �g�p����g���@�\��w�肷�� (���ʂ̊g���@�\��܂�
  x86 extension����킹�Ďw�肷��
  USE_3DNOW
  USE_3DNOW_ENH (3DNow+
  USE_3DNOW_PRO (3DNow?
  USE_SSE4A
  USE_SSE5
*/

// x86 extension number
enum{
	X86_EXT_NONE = 0,
	X86_MMX,
	X86_SSE,
	X86_SSE2,
	X86_SSE3,
	X86_SSSE3,
	X86_SSE41,
	X86_SSE42,
	X86_AVX,
	X86_AVX2,
	X86_AVX512,
};
//x86 AMD extension number
enum{
	X86_AMD_EXT_NONE = 0,
	X86_MMX_EXT,
	X86_3DNOW,
	X86_3DNOW_EX,
	X86_3DNOW_ENH,
	X86_3DNOW_PRO,
	X86_SSE4A,
	X86_SSE5,
};

#if defined(__GNUC__) || defined(__MINGW32__) /* target specific option mismatch... */
#ifndef __MMX__
#undef  USE_MMX
#endif
#ifndef __SSE__
#undef  USE_SSE
#undef  USE_MMX2
#endif
#ifndef __SSE2__
#undef  USE_SSE2
#endif
#ifndef __SSE3__
#undef  USE_SSE3
#endif
#ifndef __SSSE3__
#undef  USE_SSSE3
#endif
#ifndef __SSE4_1__
#undef  USE_SSE4
#undef  USE_SSE42
#endif
#ifndef __SSE4_2__
#undef  USE_SSE4
#undef  USE_SSE42
#endif
#ifndef __AVX__
#undef  USE_AVX
#endif
#ifndef __AVX2__
#undef  USE_AVX2
#endif
#if !defined(__AVX512F__) || !defined(__AVX512CD__) || !defined(__AVX512VL__) || !defined(__AVX512DQ__) || !defined(__AVX512BW__)
#undef  USE_AVX512
#endif
#endif /* __GNUC__ */

#if defined(USE_AVX512) // _MSC_VER >= 1910 VC2017?
#define USE_X86_EXT_INTRIN  10  // F, CD, VL, DQ, BW
#elif defined(USE_AVX2) // _MSC_VER >= 1700 VC2013?
#define USE_X86_EXT_INTRIN  9
#elif defined(USE_AVX) // _MSC_VER >= 1600 VC2010?
#define USE_X86_EXT_INTRIN  8
#elif defined(USE_SSE42) || defined(USE_SSE4)
#define USE_X86_EXT_INTRIN  7
#elif defined(USE_SSE41) // _MSC_VER >= 1500 VC2008?
#define USE_X86_EXT_INTRIN  6
#elif defined(USE_SSSE3)
#define USE_X86_EXT_INTRIN  5
#elif defined(USE_SSE3) // _MSC_VER >= 1400?? VC2005?
#define USE_X86_EXT_INTRIN  4
#elif defined(USE_SSE2)
#define USE_X86_EXT_INTRIN  3
#elif defined(USE_SSE) || defined(USE_MMX2) 
#define USE_X86_EXT_INTRIN  2 // include MMX2
#elif defined(USE_MMX) // _MSC_VER >= 1310 VC2003?
#define USE_X86_EXT_INTRIN  1
#else // not defined
#define USE_X86_EXT_INTRIN  0
#endif

#if (USE_X86_EXT_INTRIN >= 4)
#undef USE_PENTIUM_4
#endif

#if defined(USE_AVX512) // _MSC_VER >= 1910 VC2017?
#define USE_X64_EXT_INTRIN  10  // F, CD, VL, DQ, BW
#elif defined(USE_AVX2) // _MSC_VER >= 1700 VC2013?
#define USE_X64_EXT_INTRIN  9
#elif defined(USE_AVX) // _MSC_VER >= 1600 VC2010?
#define USE_X64_EXT_INTRIN  8
#elif defined(USE_SSE42) || defined(USE_SSE4)
#define USE_X64_EXT_INTRIN  7
#elif defined(USE_SSE41) // _MSC_VER >= 1500 VC2008?
#define USE_X64_EXT_INTRIN  6
#elif defined(USE_SSSE3)
#define USE_X64_EXT_INTRIN  5
#elif defined(USE_SSE3) // _MSC_VER >= 1400?? VC2005?
#define USE_X64_EXT_INTRIN  4
#elif defined(USE_SSE2)
#define USE_X64_EXT_INTRIN  3
#elif defined(USE_SSE) || defined(USE_MMX2) 
#define USE_X64_EXT_INTRIN  2 // include MMX2
#elif defined(USE_MMX) // _MSC_VER >= 1310 VC2003?
#define USE_X64_EXT_INTRIN  1
#else // not defined
#define USE_X64_EXT_INTRIN  0
#endif

#if defined(USE_SSE5) // _MSC_VER >= 1700 VC2012?
#define USE_X86_AMD_EXT_INTRIN  6
#elif defined(USE_SSE4A) // _MSC_VER >= 1600 VC2010?
#define USE_X86_AMD_EXT_INTRIN  5
#elif defined(USE_3DNOW_PRO)
#define USE_X86_AMD_EXT_INTRIN  4
#elif defined(USE_3DNOW_ENH)
#define USE_X86_AMD_EXT_INTRIN  3
#elif defined(USE_3DNOW)
#define USE_X86_AMD_EXT_INTRIN  2
#elif defined(USE_MMX_EXT)
#define USE_X86_AMD_EXT_INTRIN  1
#else // not defined
#define USE_X86_AMD_EXT_INTRIN  0
#endif

#if defined(USE_AVX512)
#define USE_X86_EXT_ASM     10  // F, CD, VL, DQ, BW
#elif defined(USE_AVX2)
#define USE_X86_EXT_ASM     9
#elif defined(USE_AVX)
#define USE_X86_EXT_ASM     8
#elif defined(USE_SSE42) || defined(USE_SSE4)
#define USE_X86_EXT_ASM     7
#elif defined(USE_SSE41)
#define USE_X86_EXT_ASM     6
#elif defined(USE_SSSE3)
#define USE_X86_EXT_ASM     5
#elif defined(USE_SSE3)
#define USE_X86_EXT_ASM     4
#elif defined(USE_SSE2)
#define USE_X86_EXT_ASM     3
#elif defined(USE_SSE) || defined(USE_MMX2) 
#define USE_X86_EXT_ASM     2 // include MMX2
#elif defined(USE_MMX)
#define USE_X86_EXT_ASM     1
#else // not defined
#define USE_X86_EXT_ASM     0
#endif

#if defined(USE_AVX512)
#define USE_X64_EXT_ASM     10  // F, CD, VL, DQ, BW
#elif defined(USE_AVX2)
#define USE_X64_EXT_ASM     9
#elif defined(USE_AVX)
#define USE_X64_EXT_ASM     8
#elif defined(USE_SSE42) || defined(USE_SSE4)
#define USE_X64_EXT_ASM     7
#elif defined(USE_SSE41)
#define USE_X64_EXT_ASM     6
#elif defined(USE_SSSE3)
#define USE_X64_EXT_ASM     5
#elif defined(USE_SSE3)
#define USE_X64_EXT_ASM     4
#elif defined(USE_SSE2)
#define USE_X64_EXT_ASM     3
#elif defined(USE_SSE) || defined(USE_MMX2) 
#define USE_X64_EXT_ASM     2 // include MMX2
#elif defined(USE_MMX)
#define USE_X64_EXT_ASM     1
#else // not defined
#define USE_X64_EXT_ASM     0
#endif

#if defined(USE_SSE4A)
#define USE_X86_AMD_EXT_ASM     5
#elif defined(USE_3DNOW_PRO)
#define USE_X86_AMD_EXT_ASM     4
#elif defined(USE_3DNOW_ENH)
#define USE_X86_AMD_EXT_ASM     3
#elif defined(USE_3DNOW)
#define USE_X86_AMD_EXT_ASM     2
#elif defined(USE_MMX_EXT)
#define USE_X86_AMD_EXT_ASM     1
#else // not defined
#define USE_X86_AMD_EXT_ASM     0
#endif

/* asm/intrin�s��� ���ɂ���Βǉ� */
#if !defined(IX64CPU)
#undef USE_X64_EXT_INTRIN
#define USE_X64_EXT_INTRIN   0
#undef USE_X64_AMD_EXT_INTRIN
#define USE_X64_AMD_EXT_INTRIN  0
#endif
#if !defined(IX86CPU) && !defined(IX64CPU)
#undef USE_X86_EXT_INTRIN
#define USE_X86_EXT_INTRIN      0
#undef USE_X86_AMD_EXT_INTRIN
#define USE_X86_AMD_EXT_INTRIN  0
#endif

/* Always disable inline asm */
#undef USE_X86_EXT_ASM
#define USE_X86_EXT_ASM      0
#undef USE_X86_AMD_EXT_ASM
#define USE_X86_AMD_EXT_ASM  0
#undef USE_X64_EXT_ASM
#define USE_X64_EXT_ASM      0
#undef USE_X64_AMD_EXT_ASM
#define USE_X64_AMD_EXT_ASM  0

#undef SUPPORT_ASM_INTEL

/*****************************************************************************/
/* PowerPC's AltiVec enhancement */
/* 0: none                       */
/* 1: use altivec                */
/*    (need -faltivec option)    */
#ifndef USE_ALTIVEC
#define USE_ALTIVEC 0
#endif



/*****************************************************************************/
/*****************************************************************************/

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif/* <sys/param.h> */
#ifdef HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif/* <sys/sysctl.h> */
#ifdef STDC_HEADERS
#include <string.h>
#elif defined(HAVE_STRINGS_H)
#include <strings.h>
#endif/* <string.h> */

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
# include <stdbool.h>
#endif

/*****************************************************************************/
#if OPT_MODE == 1 && USE_X86_EXT_ASM > 0

#ifdef LITTLE_ENDIAN
#define iman_ 0
#else
#define iman_ 1
#endif
#define _double2fixmagic 68719476736.0 * 1.5

#if defined(__BORLANDC__) && (__BORLANDC__ >= 1380)
extern int32 imuldiv8(int32 a, int32 b);
extern int32 imuldiv16(int32 a, int32 b);
extern int32 imuldiv24(int32 a, int32 b);
extern int32 imuldiv28(int32 a, int32 b);

#elif defined(SUPPORT_ASM_AT_AND_T) && defined(__ppc__)
static inline int32 imuldiv8(int32 a, int32 b)
{
    register int32 ret, rah, ral, rlh, rll;
    __asm__("mulhw %0,%7,%8\n\t"
	     "mullw %1,%7,%8\n\t"
	     "rlwinm %2,%0,24,0,7\n\t"
	     "rlwinm %3,%1,24,8,31\n\t"
	     "or %4,%2,%3"
	     :"=r"(rah),"=r"(ral),
	      "=r"(rlh),"=r"(rll),
	      "=r"(ret),
	      "=r"(a),"=r"(b)
	     :"5"(a),"6"(b));
    return ret;
}

static inline int32 imuldiv16(int32 a, int32 b)
{
    register int32 ret, rah, ral, rlh, rll;
    __asm__("mulhw %0,%7,%8\n\t"
	     "mullw %1,%7,%8\n\t"
	     "rlwinm %2,%0,16,0,15\n\t"
	     "rlwinm %3,%1,16,16,31\n\t"
	     "or %4,%2,%3"
	     :"=r"(rah),"=r"(ral),
	      "=r"(rlh),"=r"(rll),
	      "=r"(ret),
	      "=r"(a),"=r"(b)
	     :"5"(a),"6"(b));
    return ret;
}

static inline int32 imuldiv24(int32 a, int32 b)
{
    register int32 ret, rah, ral, rlh, rll;
    __asm__("mulhw %0,%7,%8\n\t"
	     "mullw %1,%7,%8\n\t"
	     "rlwinm %2,%0,8,0,23\n\t"
	     "rlwinm %3,%1,8,24,31\n\t"
	     "or %4,%2,%3"
	     :"=r"(rah),"=r"(ral),
	      "=r"(rlh),"=r"(rll),
	      "=r"(ret),
	      "=r"(a),"=r"(b)
	     :"5"(a),"6"(b));
    return ret;
}

static inline int32 imuldiv28(int32 a, int32 b)
{
    register int32 ret, rah, ral, rlh, rll;
    __asm__("mulhw %0,%7,%8\n\t"
	     "mullw %1,%7,%8\n\t"
	     "rlwinm %2,%0,4,0,27\n\t"
	     "rlwinm %3,%1,4,28,31\n\t"
	     "or %4,%2,%3"
	     :"=r"(rah),"=r"(ral),
	      "=r"(rlh),"=r"(rll),
	      "=r"(ret),
	      "=r"(a),"=r"(b)
	     :"5"(a),"6"(b));
    return ret;
}

#elif defined(SUPPORT_ASM_AT_AND_T)
static inline int32 imuldiv8(int32 a, int32 b)
{
    int32 result;
    __asm__("movl %1, %%eax\n\t"
	    "movl %2, %%edx\n\t"
	    "imull %%edx\n\t"
	    "shr $8, %%eax\n\t"
	    "shl $24, %%edx\n\t"
	    "or %%edx, %%eax\n\t"
	    "movl %%eax, %0\n\t"
	    : "=g"(result)
	    : "g"(a), "g"(b)
	    : "eax", "edx");
    return result;
}

static inline int32 imuldiv16(int32 a, int32 b)
{
    int32 result;
    __asm__("movl %1, %%eax\n\t"
	    "movl %2, %%edx\n\t"
	    "imull %%edx\n\t"
	    "shr $16, %%eax\n\t"
	    "shl $16, %%edx\n\t"
	    "or %%edx, %%eax\n\t"
	    "movl %%eax, %0\n\t"
	    : "=g"(result)
	    : "g"(a), "g"(b)
	    : "eax", "edx");
    return result;
}

static inline int32 imuldiv24(int32 a, int32 b)
{
    int32 result;
    __asm__("movl %1, %%eax\n\t"
	    "movl %2, %%edx\n\t"
	    "imull %%edx\n\t"
	    "shr $24, %%eax\n\t"
	    "shl $8, %%edx\n\t"
	    "or %%edx, %%eax\n\t"
	    "movl %%eax, %0\n\t"
	    : "=g"(result)
	    : "g"(a), "g"(b)
	    : "eax", "edx");
    return result;
}

static inline int32 imuldiv28(int32 a, int32 b)
{
    int32 result;
    __asm__("movl %1, %%eax\n\t"
	    "movl %2, %%edx\n\t"
	    "imull %%edx\n\t"
	    "shr $28, %%eax\n\t"
	    "shl $4, %%edx\n\t"
	    "or %%edx, %%eax\n\t"
	    "movl %%eax, %0\n\t"
	    : "=g"(result)
	    : "g"(a), "g"(b)
	    : "eax", "edx");
    return result;
}

#elif defined(SUPPORT_ASM_INTEL)
inline int32 imuldiv8(int32 a, int32 b) {
	_asm {
		mov eax, a
		mov edx, b
		imul edx
		shr eax, 8
		shl edx, 24
		or  eax, edx
	}
}

inline int32 imuldiv16(int32 a, int32 b) {
	_asm {
		mov eax, a
		mov edx, b
		imul edx
		shr eax, 16
		shl edx, 16
		or  eax, edx
	}
}

inline int32 imuldiv24(int32 a, int32 b) {
	_asm {
		mov eax, a
		mov edx, b
		imul edx
		shr eax, 24
		shl edx, 8
		or  eax, edx
	}
}

inline int32 imuldiv28(int32 a, int32 b) {
	_asm {
		mov eax, a
		mov edx, b
		imul edx
		shr eax, 28
		shl edx, 4
		or  eax, edx
	}
}

inline int64 imuldiv24_64bit(int64 a, int64 b) {
	return ((int64)(a) * (int64)(b)) >> 24;
}

inline int64 int64_imuldiv24(int64 a, int64 b)
{
	return ((int64)(a) * (int64)(b)) >> 24;
}

#else
/* Generic version of imuldiv. */
#define imuldiv8(a, b) \
    (int32)(((int64)(a) * (int64)(b)) >> 8)

#define imuldiv16(a, b) \
    (int32)(((int64)(a) * (int64)(b)) >> 16)

#define imuldiv24(a, b) \
    (int32)(((int64)(a) * (int64)(b)) >> 24)

#define imuldiv28(a, b) \
    (int32)(((int64)(a) * (int64)(b)) >> 28)

#endif /* architectures */

#define ifloor_internal(a, b) \
    ((a) & ~((1L << (b)) - 1))

#define ifloor8(a) \
    ifloor_internal(a, 8)

#define ifloor16(a) \
    ifloor_internal(a, 16)

#define ifloor24(a) \
    ifloor_internal(a, 24)

#define ifloor28(a) \
    ifloor_internal(a, 28)

static inline int32 signlong(int32 a)
{
	return ((a | 0x7fffffff) >> 30);
}

#else
/* Generic version of imuldiv. */
#define imuldiv8(a, b) \
    (int32)(((int64)(a) * (int64)(b)) >> 8)

#define imuldiv16(a, b) \
    (int32)(((int64)(a) * (int64)(b)) >> 16)

#define imuldiv24(a, b) \
    (int32)(((int64)(a) * (int64)(b)) >> 24)

#define imuldiv28(a, b) \
    (int32)(((int64)(a) * (int64)(b)) >> 28)

#endif /* OPT_MODE != 0 */



/*****************************************************************************/
#if (USE_X86_EXT_ASM || USE_X86_EXT_INTRIN || USE_X86_AMD_EXT_ASM || USE_X86_AMD_EXT_INTRIN)

#if (USE_X86_EXT_INTRIN || USE_X86_AMD_EXT_INTRIN)
#ifdef __GNUC__
#include <x86intrin.h>
#elif (_MSC_VER >= 1600) // VC2010(VC10)
#include <intrin.h>
#else // VC2003(VC7) VC2005(VC8) VC2008(VC9)
#include <emmintrin.h>
#if defined(USE_X86_AMD_EXT_INTRIN) && (USE_X86_AMD_EXT_INTRIN >= 2)
#include <mm3dnow.h>
#endif
#endif
#endif


#ifdef __GNUC__

#if ((USE_X86_EXT_ASM >= 10) || (USE_X86_EXT_INTRIN >= 10)) // AVX512 64byte
#define ALIGN_SIZE 64
#define ALIGN __attribute__((aligned(ALIGN_SIZE)))
#define ALIGN32 __attribute__((aligned(32)))
#define ALIGN16 __attribute__((aligned(16)))
#define ALIGN8 __attribute__((aligned(8)))
#elif ((USE_X86_EXT_ASM >= 8) || (USE_X86_EXT_INTRIN >= 8)) // AVX 32byte
#define ALIGN_SIZE 32
#define ALIGN __attribute__((aligned(ALIGN_SIZE)))
#define ALIGN32 __attribute__((aligned(32)))
#define ALIGN16 __attribute__((aligned(16)))
#define ALIGN8 __attribute__((aligned(8)))
#elif ((USE_X86_EXT_ASM >= 2) || (USE_X86_EXT_INTRIN >= 2)) // SSE 16byte // AMD??
#define ALIGN_SIZE 16
#define ALIGN __attribute__((aligned(ALIGN_SIZE)))
#define ALIGN32 __attribute__((aligned(32)))
#define ALIGN16 __attribute__((aligned(16)))
#define ALIGN8 __attribute__((aligned(8)))
#elif ((USE_X86_EXT_ASM >= 1) || (USE_X86_EXT_INTRIN >= 1)) // MMX 8byte // AMD??
#define ALIGN_SIZE 8
#define ALIGN __attribute__((aligned(ALIGN_SIZE)))
#define ALIGN32 __attribute__((aligned(32)))
#define ALIGN16 __attribute__((aligned(16)))
#define ALIGN8 __attribute__((aligned(8)))
#endif // ALIGN size

#elif defined(_MSC_VER) || defined(MSC_VER)

#if ((USE_X86_EXT_ASM >= 10) || (USE_X86_EXT_INTRIN >= 10)) // AVX512 64byte
#define ALIGN_SIZE 64
#define ALIGN _declspec(align(ALIGN_SIZE))
#define ALIGN32 _declspec(align(32))
#define ALIGN16 _declspec(align(16))
#define ALIGN8 _declspec(align(8))
#elif ((USE_X86_EXT_ASM >= 8) || (USE_X86_EXT_INTRIN >= 8)) // AVX 32byte
#define ALIGN_SIZE 32
#define ALIGN _declspec(align(ALIGN_SIZE))
#define ALIGN32 _declspec(align(32))
#define ALIGN16 _declspec(align(16))
#define ALIGN8 _declspec(align(8))
#elif ((USE_X86_EXT_ASM >= 2) || (USE_X86_EXT_INTRIN >= 2)) // SSE 16byte // AMD??
#define ALIGN_SIZE 16
#define ALIGN _declspec(align(ALIGN_SIZE))
#define ALIGN32 _declspec(align(32))
#define ALIGN16 _declspec(align(16))
#define ALIGN8 _declspec(align(8))
#elif ((USE_X86_EXT_ASM >= 1) || (USE_X86_EXT_INTRIN >= 1)) // MMX 8byte // AMD??
#define ALIGN_SIZE 8
#define ALIGN _declspec(align(ALIGN_SIZE))
#define ALIGN32 _declspec(align(32))
#define ALIGN16 _declspec(align(16))
#define ALIGN8 _declspec(align(8))
#endif // ALIGN size

#endif /* __GNUC__, MSC_VER */

/*
�ȉ���FMA�̃}�N���� CPU��FMA�̎������Ȃ��ꍇ��MADD (�ۂߗL���̐��x�̈Ⴂ�͍l�����ĂȂ� �~�X�����E�E
FMA(vec_a, vec_b, vec_c) : vec_a * vec_b + vec_c
FMA2(vec_a, vec_b, vec_c, vec_d) : vec_a * vec_b + vec_c * vec_d
LS_FMA(ptr, vec_a, vec_b) : store(ptr, load(ptr) + vec_a * vec_b) // *ptr += vec_a * vec_b
LS_ADD(ptr, vec_a) : store(ptr, load(ptr) + vec_a) // *ptr += vec_a
LS_MUL(ptr, vec_a) : store(ptr, load(ptr) * vec_a) // *ptr *= vec_a
LSU : Unalignment (use loadu/storeu
*/

#if (USE_X86_EXT_INTRIN >= 9)
#define MM256_SET2X_SI256(vec_a, vec_b) \
	_mm256_inserti128_si256(_mm256_inserti128_si256(_mm256_setzero_si256(), vec_a, 0x0), vec_b, 0x1)
#endif

#if (USE_X86_EXT_INTRIN >= 8)
#if (USE_X86_EXT_INTRIN >= 9)
#define MM256_FMA_PD(vec_a, vec_b, vec_c) _mm256_fmadd_pd(vec_a, vec_b, vec_c)
#define MM256_FMA2_PD(vec_a, vec_b, vec_c, vec_d) _mm256_fmadd_pd(vec_a, vec_b, _mm256_mul_pd(vec_c, vec_d))
#define MM256_FMA3_PD(v00, v01, v10, v11, v20, v21) _mm256_fmadd_pd(v20, v21, _mm256_fmadd_pd(v10, v11, _mm256_mul_pd(v00, v01)))
#define MM256_FMA4_PD(v00, v01, v10, v11, v20, v21, v30, v31) _mm256_add_pd(\
	_mm256_fmadd_pd(v30, v31, _mm256_mul_pd(v20, v21)), _mm256_fmadd_pd(v10, v11, _mm256_mul_pd(v00, v01)) )
#define MM256_LS_FMA_PD(ptr, vec_a, vec_b) _mm256_store_pd(ptr, _mm256_fmadd_pd(vec_a, vec_b, _mm256_load_pd(ptr)))
#define MM256_LSU_FMA_PD(ptr, vec_a, vec_b) _mm256_storeu_pd(ptr, _mm256_fmadd_pd(vec_a, vec_b, _mm256_loadu_pd(ptr)))
#define MM256_MSUB_PD(vec_a, vec_b, vec_c) _mm256_fmsub_pd(vec_a, vec_b, vec_c)
#define MM256_FMA_PS(vec_a, vec_b, vec_c) _mm256_fmadd_ps(vec_a, vec_b, vec_c)
#define MM256_FMA2_PS(vec_a, vec_b, vec_c, vec_d) _mm256_fmadd_ps(vec_a, vec_b, _mm256_mul_ps(vec_c, vec_d))
#define MM256_FMA3_PS(v00, v01, v10, v11, v20, v21) _mm256_fmadd_ps(v20, v21, _mm256_fmadd_ps(v10, v11, _mm256_mul_ps(v00, v01))
#define MM256_LS_FMA_PS(ptr, vec_a, vec_b) _mm256_store_ps(ptr, _mm256_fmadd_ps(vec_a, vec_b, _mm256_load_ps(ptr)))
#define MM256_LSU_FMA_PS(ptr, vec_a, vec_b) _mm256_storeu_ps(ptr, _mm256_fmadd_ps(vec_a, vec_b, _mm256_loadu_ps(ptr)))
#define MM256_MSUB_PS(vec_a, vec_b, vec_c) _mm256_fmsub_ps(vec_a, vec_b, vec_c)
#else // ! (USE_X86_EXT_INTRIN >= 9)
#define MM256_FMA_PD(vec_a, vec_b, vec_c) _mm256_add_pd(_mm256_mul_pd(vec_a, vec_b), vec_c)
#define MM256_FMA2_PD(vec_a, vec_b, vec_c, vec_d) _mm256_add_pd(_mm256_mul_pd(vec_a, vec_b), _mm256_mul_pd(vec_c, vec_d))
#define MM256_FMA3_PD(v00, v01, v10, v11, v20, v21) _mm256_add_pd(\
	_mm256_add_pd(_mm256_mul_pd(v00, v01),_mm256_mul_pd(v10, v11)), _mm256_mul_pd(v20, v21))
#define MM256_FMA4_PD(v00, v01, v10, v11, v20, v21, v30, v31) _mm256_add_pd(\
	_mm256_add_pd(_mm256_mul_pd(v00, v01),_mm256_mul_pd(v10, v11)), _mm256_add_pd(_mm256_mul_pd(v20, v21),_mm256_mul_pd(v30, v31)))
#define MM256_LS_FMA_PD(ptr, vec_a, vec_b) _mm256_store_pd(ptr, _mm256_add_pd(_mm256_load_pd(ptr), _mm256_mul_pd(vec_a, vec_b)))
#define MM256_LSU_FMA_PD(ptr, vec_a, vec_b) _mm256_storeu_pd(ptr, _mm256_add_pd(_mm256_loadu_pd(ptr), _mm256_mul_pd(vec_a, vec_b)))
#define MM256_MSUB_PD(vec_a, vec_b, vec_c) _mm256_sub_pd(_mm256_mul_pd(vec_a, vec_b), vec_c)
#define MM256_FMA_PS(vec_a, vec_b, vec_c) _mm256_add_ps(_mm256_mul_ps(vec_a, vec_b), vec_c)
#define MM256_FMA2_PS(vec_a, vec_b, vec_c, vec_d) _mm256_add_ps(_mm256_mul_ps(vec_a, vec_b), _mm256_mul_ps(vec_c, vec_d))
#define MM256_FMA3_PS(v00, v01, v10, v11, v20, v21) _mm256_add_ps(\
	_mm256_add_ps(_mm256_mul_ps(v00, v01),_mm256_mul_ps(v10, v11)), _mm256_mul_ps(v20, v21)))
#define MM256_LS_FMA_PS(ptr, vec_a, vec_b) _mm256_store_ps(ptr, _mm256_add_ps(_mm256_load_ps(ptr), _mm256_mul_ps(vec_a, vec_b)))
#define MM256_LSU_FMA_PS(ptr, vec_a, vec_b) _mm256_storeu_ps(ptr, _mm256_add_ps(_mm256_loadu_ps(ptr), _mm256_mul_ps(vec_a, vec_b)))
#define MM256_MSUB_PS(vec_a, vec_b, vec_c) _mm256_sub_ps(_mm256_mul_ps(vec_a, vec_b), vec_c)
#endif // (USE_X86_EXT_INTRIN >= 9)
#define MM256_LS_ADD_PD(ptr, vec_a) _mm256_store_pd(ptr, _mm256_add_pd(_mm256_load_pd(ptr), vec_a))
#define MM256_LSU_ADD_PD(ptr, vec_a) _mm256_storeu_pd(ptr, _mm256_add_pd(_mm256_loadu_pd(ptr), vec_a))
#define MM256_LS_MUL_PD(ptr, vec_a) _mm256_store_pd(ptr, _mm256_mul_pd(_mm256_load_pd(ptr), vec_a))
#define MM256_LSU_MUL_PD(ptr, vec_a) _mm256_storeu_pd(ptr, _mm256_mul_pd(_mm256_loadu_pd(ptr), vec_a))
#define MM256_LS_ADD_PS(ptr, vec_a) _mm256_store_ps(ptr, _mm256_add_ps(_mm256_load_ps(ptr), vec_a))
#define MM256_LSU_ADD_PS(ptr, vec_a) _mm256_storeu_ps(ptr, _mm256_add_ps(_mm256_loadu_ps(ptr), vec_a))
#define MM256_LS_MUL_PS(ptr, vec_a) _mm256_store_ps(ptr, _mm256_mul_ps(_mm256_load_ps(ptr), vec_a))
#define MM256_LSU_MUL_PS(ptr, vec_a) _mm256_storeu_ps(ptr, _mm256_mul_ps(_mm256_loadu_ps(ptr), vec_a))
#define MM256_SET2X_PS(vec_a, vec_b) \
	_mm256_insertf128_ps(_mm256_insertf128_ps(_mm256_setzero_ps(), vec_a, 0x0), vec_b, 0x1)
#define MM256_SET2X_PD(vec_a, vec_b) \
	_mm256_insertf128_pd(_mm256_insertf128_pd(_mm256_setzero_pd(), vec_a, 0x0), vec_b, 0x1)
#endif // (USE_X86_EXT_INTRIN >= 8)

#if (USE_X86_EXT_INTRIN >= 3)
#if (USE_X86_EXT_INTRIN >= 9)
#define MM_FMA_PD(vec_a, vec_b, vec_c) _mm_fmadd_pd(vec_a, vec_b, vec_c)
#define MM_FMA2_PD(vec_a, vec_b, vec_c, vec_d) _mm_fmadd_pd(vec_a, vec_b, _mm_mul_pd(vec_c, vec_d))
#define MM_FMA3_PD(v00, v01, v10, v11, v20, v21) _mm_fmadd_pd(v20, v21, _mm_fmadd_pd(v10, v11, _mm_mul_pd(v00, v01)) )
#define MM_FMA4_PD(v00, v01, v10, v11, v20, v21, v30, v31) _mm_add_pd(\
	_mm_fmadd_pd(v30, v31, _mm_mul_pd(v20, v21)), _mm_fmadd_pd(v10, v11, _mm_mul_pd(v00, v01)) )
#define MM_FMA5_PD(v00, v01, v10, v11, v20, v21, v30, v31, v40, v41) _mm_add_pd(_mm_fmadd_pd(v40, v41, \
	_mm_fmadd_pd(v30, v31, _mm_mul_pd(v20, v21))), _mm_fmadd_pd(v10, v11, _mm_mul_pd(v00, v01)) )
#define MM_FMA6_PD(v00, v01, v10, v11, v20, v21, v30, v31, v40, v41, v50, v51) _mm_add_pd(\
	_mm_fmadd_pd(v50, v51, _mm_fmadd_pd(v40, v41, _mm_mul_pd(v30, v31))), \
	_mm_fmadd_pd(v20, v21, _mm_fmadd_pd(v10, v11, _mm_mul_pd(v00, v01))) )
#define MM_MSUB_PD(vec_a, vec_b, vec_c) _mm_fmsub_pd(vec_a, vec_b, vec_c)
#define MM_LS_FMA_PD(ptr, vec_a, vec_b) _mm_store_pd(ptr, _mm_fmadd_pd(vec_a, vec_b, _mm_load_pd(ptr)))
#define MM_LSU_FMA_PD(ptr, vec_a, vec_b) _mm_storeu_pd(ptr, _mm_fmadd_pd(vec_a, vec_b, _mm_loadu_pd(ptr)))
#define MM_MSUB_PD(vec_a, vec_b, vec_c) _mm_fmsub_pd(vec_a, vec_b, vec_c)
#else // !(USE_X86_EXT_INTRIN >= 9)
#define MM_FMA_PD(vec_a, vec_b, vec_c) _mm_add_pd(_mm_mul_pd(vec_a, vec_b), vec_c)
#define MM_FMA2_PD(vec_a, vec_b, vec_c, vec_d) _mm_add_pd(_mm_mul_pd(vec_a, vec_b), _mm_mul_pd(vec_c, vec_d))
#define MM_FMA3_PD(v00, v01, v10, v11, v20, v21) _mm_add_pd(\
	_mm_add_pd(_mm_mul_pd(v00, v01),_mm_mul_pd(v10, v11)), _mm_mul_pd(v20, v21) )
#define MM_FMA4_PD(v00, v01, v10, v11, v20, v21, v30, v31) _mm_add_pd(\
	_mm_add_pd(_mm_mul_pd(v00, v01),_mm_mul_pd(v10, v11)), _mm_add_pd(_mm_mul_pd(v20, v21),_mm_mul_pd(v30, v31)))
#define MM_FMA5_PD(v00, v01, v10, v11, v20, v21, v30, v31, v40, v41) _mm_add_pd(_mm_add_pd(\
	_mm_add_pd(_mm_mul_pd(v00, v01),_mm_mul_pd(v10, v11)), _mm_add_pd(_mm_mul_pd(v20, v21),_mm_mul_pd(v30, v31)))\
	, _mm_mul_pd(v40, v41))
#define MM_FMA6_PD(v00, v01, v10, v11, v20, v21, v30, v31, v40, v41, v50, v51) _mm_add_pd(_mm_add_pd(\
	_mm_add_pd(_mm_mul_pd(v00, v01),_mm_mul_pd(v10, v11)), _mm_add_pd(_mm_mul_pd(v20, v21),_mm_mul_pd(v30, v31)))\
	, _mm_add_pd(_mm_mul_pd(v40, v41),_mm_mul_pd(v50, v51)))
#define MM_LS_FMA_PD(ptr, vec_a, vec_b) _mm_store_pd(ptr, _mm_add_pd(_mm_load_pd(ptr), _mm_mul_pd(vec_a, vec_b)))
#define MM_LSU_FMA_PD(ptr, vec_a, vec_b) _mm_storeu_pd(ptr, _mm_add_pd(_mm_loadu_pd(ptr), _mm_mul_pd(vec_a, vec_b)))
#define MM_MSUB_PD(vec_a, vec_b, vec_c) _mm_sub_pd(_mm_mul_pd(vec_a, vec_b), vec_c)
#endif // (USE_X86_EXT_INTRIN >= 9)

#define MM_LS_ADD_PD(ptr, vec_a) _mm_store_pd(ptr, _mm_add_pd(_mm_load_pd(ptr), vec_a))
#define MM_LSU_ADD_PD(ptr, vec_a) _mm_storeu_pd(ptr, _mm_add_pd(_mm_loadu_pd(ptr), vec_a))
#define MM_LS_MUL_PD(ptr, vec_a) _mm_store_pd(ptr, _mm_mul_pd(_mm_load_pd(ptr), vec_a))
#define MM_LSU_MUL_PD(ptr, vec_a) _mm_storeu_pd(ptr, _mm_mul_pd(_mm_loadu_pd(ptr), vec_a))

#if 0//(USE_X86_EXT_INTRIN >= 4) // sse3
#define MM_LOAD1_PD(ptr) _mm_loaddup_pd(ptr) // slow!
#else // !(USE_X86_EXT_INTRIN >= 4)
#define MM_LOAD1_PD(ptr) _mm_load1_pd(ptr)
#endif // (USE_X86_EXT_INTRIN >= 4)

#if (USE_X86_EXT_INTRIN >= 6) // sse4.1
#define MM_EXTRACT_EPI32(vec,num) _mm_extract_epi32(vec,num) // num:0~3
#else // ! (USE_X86_EXT_INTRIN >= 6)
#define MM_EXTRACT_EPI32(vec,num) _mm_cvtsi128_si32(_mm_shuffle_epi32(vec, num)) // num:0~3
#endif // (USE_X86_EXT_INTRIN >= 6)

#endif // (USE_X86_EXT_INTRIN >= 3)

#if (USE_X86_EXT_INTRIN >= 2)
#if (USE_X86_EXT_INTRIN >= 9)
#define MM_FMA_PS(vec_a, vec_b, vec_c) _mm_fmadd_ps(vec_a, vec_b, vec_c)
#define MM_FMA2_PS(vec_a, vec_b, vec_c, vec_d) _mm_fmadd_ps(vec_a, vec_b, _mm_mul_ps(vec_c, vec_d))
#define MM_FMA3_PS(v00, v01, v10, v11, v20, v21) _mm_fmadd_ps(v20, v21, _mm_fmadd_ps(v10, v11, _mm_mul_ps(v00, v01))
#define MM_FMA4_PS(v00, v01, v10, v11, v20, v21, v30, v31) _mm_add_ps(\
	_mm_fmadd_ps(v30, v31, _mm_mul_ps(v20, v21)), _mm_fmadd_ps(v10, v11, _mm_mul_ps(v00, v01)) )
#define MM_FMA5_PS(v00, v01, v10, v11, v20, v21, v30, v31, v40, v41) _mm_fmadd_ps(v40, v41, \
	_mm_fmadd_ps(v30, v31, _mm_mul_ps(v20, v21)), _mm_fmadd_ps(v10, v11, _mm_mul_ps(v00, v01)) )
#define MM_FMA6_PS(v00, v01, v10, v11, v20, v21, v30, v31, v40, v41, v50, v51) _mm_add_ps(\
	_mm_fmadd_ps(v50, v51, _mm_fmadd_ps(v40, v41, _mm_mul_ps(v30, v31))), \
	_mm_fmadd_ps(v20, v21, _mm_fmadd_ps(v10, v11, _mm_mul_ps(v00, v01))) )
#define MM_LS_FMA_PS(ptr, vec_a, vec_b) _mm_store_ps(ptr, _mm_fmadd_ps(vec_a, vec_b, _mm_load_ps(ptr)))
#define MM_LSU_FMA_PS(ptr, vec_a, vec_b) _mm_storeu_ps(ptr, _mm_fmadd_ps(vec_a, vec_b, _mm_loadu_ps(ptr)))
#define MM_MSUB_PS(vec_a, vec_b, vec_c) _mm_fmsub_ps(vec_a, vec_b, vec_c)
#else
#define MM_FMA_PS(vec_a, vec_b, vec_c) _mm_add_ps(_mm_mul_ps(vec_a, vec_b), vec_c)
#define MM_FMA2_PS(vec_a, vec_b, vec_c, vec_d) _mm_add_ps(_mm_mul_ps(vec_a, vec_b), _mm_mul_ps(vec_c, vec_d))
#define MM_FMA3_PS(v00, v01, v10, v11, v20, v21) _mm_add_ps(\
	_mm_add_ps(_mm_mul_ps(v00, v01),_mm_mul_ps(v10, v11)), _mm_mul_ps(v20, v21))
#define MM_FMA4_PS(v00, v01, v10, v11, v20, v21, v30, v31) _mm_add_ps(\
	_mm_add_ps(_mm_mul_ps(v00, v01),_mm_mul_ps(v10, v11)), _mm_add_ps(_mm_mul_ps(v20, v21),_mm_mul_ps(v30, v31))))
#define MM_FMA5_PS(v00, v01, v10, v11, v20, v21, v30, v31, v40, v41) _mm_add_ps(_mm_add_ps(\
	_mm_add_ps(_mm_mul_ps(v00, v01),_mm_mul_ps(v10, v11)), _mm_add_ps(_mm_mul_ps(v20, v21),_mm_mul_ps(v30, v31)))\
	, _mm_mul_ps(v40, v41))
#define MM_FMA6_PS(v00, v01, v10, v11, v20, v21, v30, v31, v40, v41, v50, v51) _mm_add_ps(_mm_add_ps(\
	_mm_add_ps(_mm_mul_ps(v00, v01),_mm_mul_ps(v10, v11)), _mm_add_ps(_mm_mul_ps(v20, v21),_mm_mul_ps(v30, v31)))\
	, _mm_add_ps(_mm_mul_ps(v40, v41),_mm_mul_ps(v50, v51)))
#define MM_LS_FMA_PS(ptr, vec_a, vec_b) _mm_store_ps(ptr, _mm_add_ps(_mm_load_ps(ptr), _mm_mul_ps(vec_a, vec_b)))
#define MM_LSU_FMA_PS(ptr, vec_a, vec_b) _mm_storeu_ps(ptr, _mm_add_ps(_mm_loadu_ps(ptr), _mm_mul_ps(vec_a, vec_b)))
#define MM_MSUB_PS(vec_a, vec_b, vec_c) _mm_sub_ps(_mm_mul_ps(vec_a, vec_b), vec_c)
#endif
#define MM_LS_ADD_PS(ptr, vec_a) _mm_store_ps(ptr, _mm_add_ps(_mm_load_ps(ptr), vec_a))
#define MM_LSU_ADD_PS(ptr, vec_a) _mm_storeu_ps(ptr, _mm_add_ps(_mm_loadu_ps(ptr), vec_a))
#define MM_LS_MUL_PS(ptr, vec_a) _mm_store_ps(ptr, _mm_mul_ps(_mm_load_ps(ptr), vec_a))
#define MM_LSU_MUL_PS(ptr, vec_a) _mm_storeu_ps(ptr, _mm_mul_ps(_mm_loadu_ps(ptr), vec_a))
#endif

#if (USE_X86_EXT_INTRIN >= 1)
#if !defined(_MSC_VER) || defined(__clang__)
#define MM_EXTRACT_F32(reg,idx) _mm_cvtss_f32(_mm_shuffle_ps(reg,reg,idx))
#define MM_EXTRACT_F64(reg,idx) _mm_cvtsd_f64(_mm_shuffle_pd(reg,reg,idx))
#define MM_EXTRACT_I32(reg,idx) _mm_cvtsi128_si32(_mm_shuffle_epi32(reg,idx))
#if (USE_X86_EXT_INTRIN >= 9)
#define MM256_EXTRACT_F32(reg,idx) _mm256_cvtss_f32(_mm256_permutevar8x32_ps(reg,idx))
#define MM256_EXTRACT_F64(reg,idx) _mm256_cvtsd_f64(_mm256_permute4x64_pd(reg,idx))
#else
#define MM256_EXTRACT_F32(reg,idx) _mm_cvtss_f32(_mm_permute_ps(_mm256_extractf128_ps(reg, idx >= 4), idx % 4))
#define MM256_EXTRACT_F64(reg,idx) _mm_cvtsd_f64(_mm_permute_pd(_mm256_extractf128_ps(reg, idx >= 2), idx % 2))
#endif
#define MM256_EXTRACT_I32(reg,idx) _mm256_extract_epi32(reg,idx)
#else
#define MM_EXTRACT_F32(reg,idx) reg.m128_f32[idx]
#define MM_EXTRACT_F64(reg,idx) reg.m128d_f64[idx]
#define MM_EXTRACT_I32(reg,idx) reg.m128i_i32[idx]
#define MM256_EXTRACT_F32(reg,idx) reg.m256_f32[idx]
#define MM256_EXTRACT_F64(reg,idx) reg.m256d_f64[idx]
#define MM256_EXTRACT_I32(reg,idx) reg.m256i_i32[idx]
#endif
#endif // (USE_X86_EXT_INTRIN >= 1)

#define IS_ALIGN(ptr) (!((int32)ptr & (ALIGN_SIZE - 1)))
extern int is_x86ext_available(void);

#else // USE_EXT 0

#define ALIGN 
#define ALIGN8 
#define ALIGN16 
#define ALIGN32 
#define ALIGNED_MALLOC(size) malloc(size)
#define ALIGNED_FREE(ptr) free(ptr)

#ifndef aligned_malloc
#define aligned_malloc(size_byte, align_size) malloc(size_byte)
#endif
#ifndef aligned_free
#define aligned_free(ptr) free(ptr)
#endif
#endif // USE_EXT



/*****************************************************************************/
#if USE_ALTIVEC

#ifndef __bool_true_false_are_defined
#define bool _Bool
typedef enum { false = 0, true = 1 } bool;
#endif /* C99 Hack */

/* typedefs */
typedef vector signed int  vint32;
typedef vector signed char vint8;

/* prototypes */
void v_memset(void *dest, int c, size_t len);
void v_memzero(void *dest, size_t len);
void v_set_dry_signal(void *dest, const int32 *buf, int32 n);

/* inline functions */
extern inline bool is_altivec_available(void)
{
  int sel[2] = { CTL_HW, HW_VECTORUNIT };
  int has_altivec = false;
  size_t len = sizeof(has_altivec);
  int error = sysctl(sel, 2, &has_altivec, &len, NULL, 0);
  if (!error) {
    return (bool)!!has_altivec;
  } else {
    return false;
  }
}

extern inline void libc_memset(void *destp, int c, size_t len)
{
    memset(destp, c, len);
}

static inline void *switch_memset(void *destp, int c, size_t len)
{
    void *keepdestp = destp;
    if (!is_altivec_available()) {
        libc_memset(destp, c, len);
    } else if (c) {
        v_memset(destp, c, len);
    } else {
        v_memzero(destp, len);
    }
    return keepdestp;
}

#define memset switch_memset
#endif /* altivec */

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#endif /* OPTCODE_H_INCLUDED */
