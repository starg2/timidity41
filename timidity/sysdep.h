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

#ifndef SYSDEP_H_INCLUDED
#define SYSDEP_H_INCLUDED 1

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <stdio.h>

/* Architectures */
#if defined(_M_IX86) || defined(__i386__) || defined(__i386) || \
	defined(_X86_) || defined(__X86__) || defined(__I86__)
#define IX86CPU 1
#endif

#if defined(_M_AMD64) || defined(__x86_64__) || defined(__amd64__)
#define AMD64CPU 1
#endif

#if defined(_M_IA64) || defined(__IA64__) || defined(__itanium__)
#define IA64CPU 1
#endif

#if defined(_M_ARM) || defined(_M_ARMT) || defined(__arm__)
#define ARMCPU 1
#endif

#if defined(__aarch64__)
#undef  ARMCPU
#define ARM64CPU 1
#endif

/* Platforms */
#if (defined(__WIN32__) || defined(_WIN32) || defined(_WIN64)) && !defined(__W32__)
#define __W32__ 1
#endif

#if !defined(_AMD64_) && defined(AMD64CPU) && \
	(((defined(WIN64) || defined(_WIN64))) || \
	 (defined(__GNUC__) && defined(__W32__)))
#define _AMD64_ 1
#endif

#if defined(__linux) && !defined(__linux__)
#define __linux__ 1
#endif

#if defined(__unix) && !defined(__unix__)
#define __unix__ 1
#endif

#if defined(sun) && !defined(SOLARIS)
#define __SUNOS__ 1
#endif

/* Fastest calling conversion */
#ifndef CALLINGCONV
#if defined(IX86CPU) && (defined(_MSC_VER) || defined(__POCC__) || \
	defined(__BORLANDC__) || defined(__WATCOMC__))
#define CALLINGCONV __fastcall
#elif defined(IX86CPU) && !defined(AMD64CPU) && defined(__GNUC__)
#define CALLINGCONV __attribute__((fastcall))
#else
#define CALLINGCONV /**/
#endif
#endif /* !CALLINGCONV */

#ifndef restrict
#define restrict /* not C99 */
#endif /* !restrict */

#ifndef TIMIDITY_FORCEINLINE
#ifdef __GNUC__
#define TIMIDITY_FORCEINLINE __attribute__((__always_inline__))
#elif defined(_MSC_VER)
#define TIMIDITY_FORCEINLINE __forceinline
#else
#define TIMIDITY_FORCEINLINE inline
#endif
#endif /* TIMIDITY_FORCEINLINE */

/* The size of the internal buffer is 2^AUDIO_BUFFER_BITS samples.
   This determines maximum number of samples ever computed in a row.

   For Linux and FreeBSD users:

   This also specifies the size of the buffer fragment.  A smaller
   fragment gives a faster response in interactive mode -- 10 or 11 is
   probably a good number. Unfortunately some sound cards emit a click
   when switching DMA buffers. If this happens to you, try increasing
   this number to reduce the frequency of the clicks.

   For other systems:

   You should probably use a larger number for improved performance.

*/
#ifndef DEFAULT_AUDIO_BUFFER_BITS
# ifdef __W32__
#  define DEFAULT_AUDIO_BUFFER_BITS 12
# else
#  define DEFAULT_AUDIO_BUFFER_BITS 11
# endif /* __W32__ */
#endif /* !DEFAULT_AUDIO_BUFFER_BITS */
///r
#ifndef DEFAULT_AUDIO_BUFFER_NUM
#define DEFAULT_AUDIO_BUFFER_NUM 32
#endif /* !DEFAULT_AUDIO_BUFFER_NUM */

#ifndef DEFAULT_COMPUTE_BUFFER_BITS
#define DEFAULT_COMPUTE_BUFFER_BITS (-1)
#endif /* !DEFAULT_COMPUTE_BUFFER_BITS */


#define SAMPLE_LENGTH_BITS 32 /* PAT/SF2:DWORD(unsined int 32bit */

#ifndef NO_VOLATILE
# define VOLATILE_TOUCH(val) /* do nothing */
# define VOLATILE volatile
#else
extern int volatile_touch(void *dmy);
# define VOLATILE_TOUCH(val) volatile_touch(&(val))
# define VOLATILE
#endif /* NO_VOLATILE */

/* Byte order */
#ifdef WORDS_BIGENDIAN
# ifndef BIG_ENDIAN
#  define BIG_ENDIAN 4321
# endif
# undef LITTLE_ENDIAN
#else
# undef BIG_ENDIAN
# ifndef LITTLE_ENDIAN
#  define LITTLE_ENDIAN 1234
# endif
#endif

/* Inline assembly (_asm and __asm__) */
#if defined(_MSC_VER) || defined(__WATCOMC__) || defined(__DMC__) || \
    (defined(__BORLANDC__) && (__BORLANDC__ >= 1380))
# define SUPPORT_ASM_INTEL 1
#elif defined(__GNUC__) && !defined(SUPPORT_ASM_INTEL)
# define SUPPORT_ASM_AT_AND_T 1
#endif


/* integer type definitions: ISO C now knows a better way */
#if defined(HAVE_STDINT_H) || \
    (defined(_MSC_VER) && _MSC_VER >= 1600 /* VC2010 */) || \
    (defined(__GNUC__) && __GNUC__ >= 3)
#include <stdint.h> // int types are defined here
typedef  int8_t   int8;
typedef uint8_t  uint8;
typedef  int16_t  int16;
typedef uint16_t uint16;
typedef  int32_t  int32;
typedef uint32_t uint32;
typedef  int64_t  int64;
typedef uint64_t uint64;
#define TIMIDITY_HAVE_INT64 1

#else /* not C99 */
#ifdef HPUX
typedef          char   int8;
typedef unsigned char  uint8;
typedef          short  int16;
typedef unsigned short uint16;
#else
typedef   signed char   int8;
typedef unsigned char  uint8;
typedef   signed short  int16;
typedef unsigned short uint16;
#endif
/* DEC MMS has 64 bit long words */
/* Linux-Axp has also 64 bit long words */
#if defined(DEC) || defined(__alpha__) \
		|| defined(__ia64__) || defined(__x86_64__) \
		|| defined(__ppc64__) || defined(__s390x__) \
                || defined(__mips64__) || defined(__LP64__) \
                || defined(_LP64)
typedef          int   int32;
typedef unsigned int  uint32;
typedef          long  int64;
typedef unsigned long uint64;
#define TIMIDITY_HAVE_INT64 1

#else /* 32bit architectures */
typedef          long  int32;
typedef unsigned long uint32;

#ifdef __GNUC__
/* gcc version<3 (gcc3 has c99 support) */
typedef          long long  int64;
typedef unsigned long long uint64;
#define TIMIDITY_HAVE_INT64 1

#elif defined(_MSC_VER)
/* VC++. or PellesC */
# ifdef __POCC__
typedef          __int64  int64;
typedef unsigned __int64 uint64;
# else
typedef          _int64  int64;
typedef unsigned _int64 uint64;
# endif
#define TIMIDITY_HAVE_INT64 1

#elif defined(__BORLANDC__) || defined(__WATCOMC__)
typedef          __int64 int64;
typedef unsigned __int64 uint64;
#define TIMIDITY_HAVE_INT64 1

#elif defined(__MACOS__)
/* Mac's C compiler seems to have these types in common */
typedef SInt64  int64;
typedef UInt64 uint64;
#define TIMIDITY_HAVE_INT64 1

#else
/* typedef          long  int64; */
/* typedef unsigned long uint64; */
#undef TIMIDITY_HAVE_INT64
#endif
#endif /* 64bit arch */
#endif /* C99 */

#if (defined(SIZEOF_LONG_DOUBLE) && SIZEOF_LONG_DOUBLE > 8) || \
    (defined(__SIZEOF_LONG_DOUBLE__) && __SIZEOF_LONG_DOUBLE__ > 8) || \
    (defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))) || \
    defined(__BORLANDC__)
/* 80-bit (12~16 bytes) */
typedef long double LDBL_T;
#define HAVE_LONG_DOUBLE 1
#else
/* 64-bit (8 bytes) */
typedef double LDBL_T;
#undef HAVE_LONG_DOUBLE
#endif

/* pointer size is not long in WIN64 and x86_64 */
#if (defined(SIZEOF_POINTER) && SIZEOF_POINTER == 8) || \
    (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8) || \
    (defined(__W32__) && defined(_AMD64_))
/* 64bit arch */
typedef  int64   ptr_size_t;
typedef uint64 u_ptr_size_t;
#elif defined(__linux__) || defined(__unix__)
/* 32/64bit arch */
typedef          long   ptr_size_t;
typedef unsigned long u_ptr_size_t;
#elif defined(_MSC_VER) && !defined(__clang__) && !defined(__POCC__)
typedef _w64  int32   ptr_size_t;
typedef _w64 uint32 u_ptr_size_t;
#else
typedef  int32   ptr_size_t;
typedef uint32 u_ptr_size_t;
#endif

/* type of file offset/size */
#ifdef _MSC_VER
/* VC++ */
#  if _MSC_VER >= 1300 // VC2003 toolkit // def 1400 /* VC2005 */
#    include <io.h>
#    ifndef PSDKCRT
/* 64-bit offset/size (VCRT) */
#      define fseeko _fseeki64
#      define ftello _ftelli64
#      define lseek  _lseeki64
typedef int64 off_size_t;
#      define HAVE_OFF_SIZE_T_64BIT 1
#    else
/* 32-bit offset/size (PSDKCRT) */
#      define fseeko fseek
#      define ftello ftell
#      define lseek  _lseek
typedef long off_size_t;
#    endif /* PSDKCRT */
#  else
/* 32-bit offset/size (pseudo) */
#    define fseeko fseek
#    define ftello ftell
#    define lseek  _lseek
typedef off_t off_size_t; /* off_t is 32-bit signed */
#  endif /* _MSC_VER >= 1400 */
#elif defined(__MINGW32__)
#  include <io.h>
#  define fseeko _fseeki64
#  define ftello _ftelli64
#  define lseek  _lseeki64
typedef int64 off_size_t;
#  define HAVE_OFF_SIZE_T_64BIT 1
#elif defined(_LARGEFILE_SOURCE) || \
    (defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS == 64) || \
    (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L) || \
    (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 600)
/* 64-bit offset/size (o) */
typedef off_t off_size_t; /* off_t is 64-bit signed */
#  define HAVE_OFF_SIZE_T_64BIT 1
#elif defined(HAVE_FSEEKO) && defined(HAVE_FTELLO)
/* 32-bit offset/size (o) */
typedef off_t off_size_t; /* off_t is 32-bit signed */
#else
/* 32-bit offset/size (pseudo) */
#  define fseeko fseek
#  define ftello ftell
typedef off_t off_size_t; /* off_t is 32-bit signed */
#endif

/* type of audio data */
#ifdef DATA_T_DOUBLE
typedef double DATA_T;
#  undef DATA_T_INT32
#  undef DATA_T_FLOAT
#elif defined(DATA_T_FLOAT)
typedef float DATA_T;
#  undef DATA_T_INT32
#  undef DATA_T_DOUBLE
#else
typedef int32 DATA_T; // only 16bit 8bit sample
#  ifndef DATA_T_INT32
#    define DATA_T_INT32 1
#  endif
#  undef DATA_T_FLOAT
#  undef DATA_T_DOUBLE
#endif

/* type of floating point number */
#ifdef FLOAT_T_DOUBLE
typedef double FLOAT_T;
#else // FLOAT_T_FLOAT
typedef float FLOAT_T;
#define FLOAT_T_FLOAT
#endif


/* Instrument files are little-endian, MIDI files big-endian, so we
   need to do some conversions. */
#define XCHG_SHORT_CONST(x) ((((x) & 0xFF) << 8) | (((x) >> 8) & 0xFF))
#ifdef __GNUC__
# define XCHG_SHORT(_x) ( \
    { \
        unsigned short x = (_x); \
        ((((x) & 0xFF) << 8) | (((x) >> 8) & 0xFF)); \
    } \
    )
#else
# define XCHG_SHORT(x) XCHG_SHORT_CONST(x)
#endif

#define XCHG_LONG_CONST(x) ((((x) & 0xFFL) << 24) | \
		      (((x) & 0xFF00L) << 8) | \
		      (((x) & 0xFF0000L) >> 8) | \
		      (((x) >> 24) & 0xFFL))
#if defined(__i486__) && !defined(__i386__)
# define XCHG_LONG(x) \
    ({ long __value; \
        asm("bswap %1; movl %1,%0" : "=g" (__value) : "r" (x)); \
        __value; })
#elif defined(__GNUC__)
# define XCHG_LONG(_x) ( \
    { \
        uint32 x = (_x); /* 32-bit */ \
        ((((x) & 0xFFL) << 24) | \
		      (((x) & 0xFF00L) << 8) | \
		      (((x) & 0xFF0000L) >> 8) | \
		      (((x) >> 24) & 0xFFL)); \
    } \
    )
#else
# define XCHG_LONG(x) XCHG_LONG_CONST(x)
#endif

///r
#define XCHG_LONGLONG_CONST(x)(\
	(((x) & 0xFFL) << 56) | \
    (((x) & 0xFF00L) << 40) | \
    (((x) & 0xFF0000L) << 24) | \
    (((x) & 0xFF000000L) << 8) | \
    (((x) & 0xFF00000000LL) >> 8) | \
    (((x) & 0xFF0000000000LL) >> 24) | \
    (((x) & 0xFF000000000000LL) >> 40) | \
    (((x) >> 56) & 0xFFL))
#ifdef __GNUC__
# define XCHG_LONGLONG(_x) ( \
    { \
        unsigned long long x = (_x); \
        ( \
            (((x) & 0xFFL) << 56) | \
            (((x) & 0xFF00L) << 40) | \
            (((x) & 0xFF0000L) << 24) | \
            (((x) & 0xFF000000L) << 8) | \
            (((x) & 0xFF00000000LL) >> 8) | \
            (((x) & 0xFF0000000000LL) >> 24) | \
            (((x) & 0xFF000000000000LL) >> 40) | \
            (((x) >> 56) & 0xFFL)); \
    } \
    )
#else
# define XCHG_LONGLONG(x) XCHG_LONGLONG_CONST(x)
#endif


#ifdef LITTLE_ENDIAN
# define LE_SHORT(x)          (x)
# define LE_LONG(x)           (x)
# define LE_LONGLONG(x)       (x)
# define LE_SHORT_CONST(x)    (x)
# define LE_LONG_CONST(x)     (x)
# define LE_LONGLONG_CONST(x) (x)
# define BE_SHORT_CONST(x)    XCHG_SHORT_CONST(x)
# define BE_LONG_CONST(x)     XCHG_LONG_CONST(x)
# define BE_LONGLONG_CONST(x) XCHG_LONGLONG_CONST(x)
# ifdef __FreeBSD__
#  include <osreldate.h>
#  if __FreeBSD_version <= 500000
#    define BE_SHORT(x)    __byte_swap_word(x)
#    define BE_LONG(x)     __byte_swap_long(x)
#    define BE_LONGLONG(x) XCHG_LONGLONG(x)
#  else
#    if __FreeBSD_version <= 500028
#      define BE_SHORT(x)    __uint8_swap_uint16(x)
#      define BE_LONG(x)     __uint8_swap_uint32(x)
#      define BE_LONGLONG(x) XCHG_LONGLONG(x)
#    else
#      define BE_SHORT(x)    __bswap16(x)
#      define BE_LONG(x)     __bswap32(x)
#      define BE_LONGLONG(x) __bswap64(x)
#    endif
#  endif
# else
#  define BE_SHORT(x)    XCHG_SHORT(x)
#  define BE_LONG(x)     XCHG_LONG(x)
#  define BE_LONGLONG(x) XCHG_LONGLONG(x)
# endif
#else /* BIG_ENDIAN */
# define BE_SHORT(x)          (x)
# define BE_LONG(x)           (x)
# define BE_LONGLONG(x)       (x)
# define BE_SHORT_CONST(x)    (x)
# define BE_LONG_CONST(x)     (x)
# define BE_LONGLONG_CONST(x) (x)
# define LE_SHORT_CONST(x)    XCHG_SHORT_CONST(x)
# define LE_LONG_CONST(x)     XCHG_LONG_CONST(x)
# define LE_LONGLONG_CONST(x) XCHG_LONGLONG_CONST(x)
# ifdef __FreeBSD__
#  include <osreldate.h>
#  if __FreeBSD_version <= 500000
#    define LE_SHORT(x)    __byte_swap_word(x)
#    define LE_LONG(x)     __byte_swap_long(x)
#    define LE_LONGLONG(x) XCHG_LONGLONG(x)
#  else
#    if __FreeBSD_version <= 500028
#      define LE_SHORT(x)    __uint8_swap_uint16(x)
#      define LE_LONG(x)     __uint8_swap_uint32(x)
#      define LE_LONGLONG(x) XCHG_LONGLONG(x)
#    else
#      define LE_SHORT(x)    __bswap16(x)
#      define LE_LONG(x)     __bswap32(x)
#      define LE_LONGLONG(x) __bswap64(x)
#    endif
#  endif
# else
#  define LE_SHORT(x)    XCHG_SHORT(x)
#  define LE_LONG(x)     XCHG_LONG(x)
#  define LE_LONGLONG(x) XCHG_LONGLONG(x)
# endif /* __FreeBSD__ */
#endif /* LITTLE_ENDIAN */

#ifndef MAX_PORT
#define MAX_PORT (MAX_CHANNELS / 16)
#endif /* !MAX_PORT */
#if MAX_PORT > MAXMIDIPORT
#undef  MAX_CHANNELS
#undef  MAX_PORT
#define MAX_CHANNELS (MAXMIDIPORT * 16)
#define MAX_PORT     (MAXMIDIPORT)
#endif

#if ((MAX_CHANNELS) & 0xF) == 0
#define MIDIPORT_MASK(ch) ((ch) & (MAX_CHANNELS - 1u))
#else
#define MIDIPORT_MASK(ch) ((ch) % MAX_CHANNELS)
#endif

///r
/* max_channels is defined in "timidity.h" */
#if MAX_CHANNELS > 32
typedef struct _ChannelBitMask
{
    uint32 b[8];		/* 256-bit bitvector */
} ChannelBitMask;
#define CLEAR_CHANNELMASK(bits) \
	memset((bits).b, 0, sizeof(ChannelBitMask))
#define FILL_CHANNELMASK(bits) \
	memset((bits).b, 0xFF, sizeof(ChannelBitMask))
#define IS_SET_CHANNELMASK(bits, c) \
	((bits).b[((c) >> 5) & 0x7] &   (1u << ((c) & 0x1F)))
#define SET_CHANNELMASK(bits, c) \
	((bits).b[((c) >> 5) & 0x7] |=  (1u << ((c) & 0x1F)))
#define UNSET_CHANNELMASK(bits, c) \
	((bits).b[((c) >> 5) & 0x7] &= ~(1u << ((c) & 0x1F)))
#define TOGGLE_CHANNELMASK(bits, c) \
	((bits).b[((c) >> 5) & 0x7] ^=  (1u << ((c) & 0x1F)))
#define COPY_CHANNELMASK(dest, src) \
	memcpy(&(dest), &(src), sizeof(ChannelBitMask))
#define REVERSE_CHANNELMASK(bits, a) \
	((bits).b[a] = ~(bits).b[a])
#define COMPARE_CHANNELMASK(bitsA, bitsB) \
	(memcmp((bitsA).b, (bitsB).b, sizeof ((bitsA).b)) == 0)
#else
typedef struct _ChannelBitMask
{
    uint32 b; /* 32-bit bitvector */
} ChannelBitMask;
#define CLEAR_CHANNELMASK(bits)		((bits).b = 0)
#define FILL_CHANNELMASK(bits)		((bits).b = ~0)
#define IS_SET_CHANNELMASK(bits, c) ((bits).b &   (1u << (c)))
#define SET_CHANNELMASK(bits, c)    ((bits).b |=  (1u << (c)))
#define UNSET_CHANNELMASK(bits, c)  ((bits).b &= ~(1u << (c)))
#define TOGGLE_CHANNELMASK(bits, c) ((bits).b ^=  (1u << (c)))
#define COPY_CHANNELMASK(dest, src)	((dest).b = (src).b)
#define REVERSE_CHANNELMASK(bits,a)	((bits).b = ~(bits).b) // not use a = NULL
#define COMPARE_CHANNELMASK(bitsA, bitsB)	((bitsA).b == (bitsB).b)
#endif

#ifdef LOOKUP_HACK
   typedef int8 sample_t;
   typedef uint8 final_volume_t;
#  define MIXUP_SHIFT 5
#else
   typedef int16 sample_t;
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
   typedef FLOAT_T final_volume_t;
#else
   typedef int32 final_volume_t;
#endif
#endif
   
// use signed int 
#if ((SAMPLE_LENGTH_BITS + FRACTION_BITS) >= 32) && defined(TIMIDITY_HAVE_INT64)
typedef int64 splen_t;
#define SPLEN_T_MAX (splen_t)((((uint64)1ULL) << 63) - 1)
#define INTEGER_BITS (64 - FRACTION_BITS) // 64bit
#define INTEGER_MASK (0xFFFFFFFFFFFFFFFF << FRACTION_BITS) // 64bit
#else // BITS <= 31
typedef int32 splen_t;
#define SPLEN_T_MAX (splen_t)((((uint32)1UL) << 31) - 1)
#define INTEGER_BITS (32 - FRACTION_BITS) // 32bit
#define INTEGER_MASK (0xFFFFFFFF << FRACTION_BITS) // 32bit
#endif	/* SAMPLE_LENGTH_BITS */

#define FRACTION_MASK (~ INTEGER_MASK)

#ifdef USE_LDEXP
#  define TIM_FSCALE(a, b) ldexp((double)(a), (b))
#  define TIM_FSCALENEG(a, b) ldexp((double)(a), -(b))
#  include <math.h>
#else
///r
#  define TIM_FSCALE(a, b) ((a) * (double)(1L << (b)))
#  define TIM_FSCALENEG(a, b) ((a) * (1.0 / (double)(1L << (b))))
#endif

#ifdef HPUX
#undef mono
#endif

#ifdef SOLARIS
/* Solaris */
int usleep(unsigned int useconds); /* shut gcc warning up */
#elif defined(__SUNOS__)
/* SunOS 4.x */
#include <sys/stdtypes.h>
#include <memory.h>
#define memcpy(x, y, n) bcopy(y, x, n)
#endif /* sun */


#ifdef __W32__
#undef PATCH_EXT_LIST
#define PATCH_EXT_LIST { ".pat", 0 }

#define URL_DIR_CACHE_DISABLE 1
#endif

/* The path separator (D.M.) */
/* Windows: "\"
 * Cygwin:  both "\" and "/"
 * Macintosh: ":"
 * Unix: "/"
 */
#if defined(__W32__)
#  define PATH_SEP '\\'
#  define PATH_STRING "\\"
#  define PATH_SEP2 '/'
#elif defined(__MACOS__)
#  define PATH_SEP ':'
#  define PATH_STRING ":"
#else
#  define PATH_SEP '/'
#  define PATH_STRING "/"
#endif

#ifdef PATH_SEP2
#define IS_PATH_SEP(c) ((c) == PATH_SEP || (c) == PATH_SEP2)
#else
#define IS_PATH_SEP(c) ((c) == PATH_SEP)
#endif

/* new line code */
#ifndef NLS
#ifdef __W32G__
#  define NLS "\r\n"
#else /* !__W32G__ */
#  define NLS "\n"
#endif
#endif /* NLS */

///r
// _MSC_VER < 1600 VC2010(VC10)  stdint.h‚ª‚È‚¢ê‡
#ifndef INT8_MAX
#define INT8_MAX 127i8
#endif /*INT8_MAX */
#ifndef INT16_MAX
#define INT16_MAX 32767i16
#endif /*INT16_MAX */
#ifndef INT32_MAX
#define INT32_MAX 2147483647i32
#endif /*INT32_MAX */
#ifndef INT64_MAX
#define INT64_MAX 9223372036854775807i64
#endif /*INT64_MAX */
#ifndef INT8_MIN
#define INT8_MIN (-127i8 - 1)
#endif /*INT8_MIN */
#ifndef INT16_MIN
#define INT16_MIN (-32767i16 - 1)
#endif /*INT16_MIN */
#ifndef INT32_MIN
#define INT32_MIN (-2147483647i32 - 1)
#endif /*INT32_MIN */
#ifndef INT64_MIN
#define INT64_MIN (-9223372036854775807i64 - 1)
#endif /*INT64_MIN */
#ifndef UINT8_MAX
#define UINT8_MAX 0xffui8
#endif /*UINT8_MAX */
#ifndef UINT16_MAX
#define UINT16_MAX 0xffffui16
#endif /*UINT16_MAX */
#ifndef UINT32_MAX
#define UINT32_MAX 0xffffffffui32
#endif /*UINT32_MAX */
#ifndef UINT64_MAX
#define UINT64_MAX 0xffffffffffffffffui64
#endif /*UINT64_MAX */

#ifndef M_E
#define M_E ((double)2.7182818284590452353602874713527)
#endif /* M_E */
#ifndef M_PI
#define M_PI ((double)3.1415926535897932384626433832795)
#endif /* M_PI */
#ifndef M_LN2
#define M_LN2		((double)0.69314718055994530941723212145818) // log(2)
#endif /* M_LN2 */
#ifndef M_LN10
#define M_LN10		((double)2.3025850929940456840179914546844) // log(10)
#endif /* M_LN10 */

#define M_LOG2		((double)0.30102999566398119521373889472449) // log10(2)
#define M_PI2   ((double)6.283185307179586476925286766559) // pi * 2
#define M_PI_DIV2   ((double)1.5707963267948966192313216916398) // pi / 2


#define M_1BIT (2) // 1U<<1
#define M_2BIT (4) // 1U<<2
#define M_3BIT (8) // 1U<<3
#define M_4BIT (16) // 1U<<4
#define M_5BIT (32) // 1U<<5
#define M_6BIT (64) // 1U<<6
#define M_7BIT (128) // 1U<<7
#define M_8BIT (256) // 1U<<8
#define M_9BIT (512) // 1U<<9
#define M_10BIT (1024) // 1U<<10
#define M_11BIT (2048) // 1U<<11
#define M_12BIT (4096) // 1U<<12
#define M_13BIT (8192) // 1U<<13
#define M_14BIT (16384) // 1U<<14
#define M_15BIT (32768L) // 1U<<15
#define M_16BIT (65536L) // 1U<<16
#define M_20BIT (1048576L) // 1U<<20
#define M_23BIT (8388608L) // 1U<<23
#define M_24BIT (16777216L) // 1U<<24
#define M_28BIT (268435456L) // 1U<<28
#define M_30BIT (1073741824UL) // 1U<<30
#define M_31BIT (2147483648UL) // 1U<<31
#define M_32BIT (4294967296LL) // 1U<<32
#define M_63BIT (9223372036854775808ULL) // 1U<<63
#define M_64BIT (18446744073709551616ULL) // 1U<<64

#define SQRT_2 ((double)1.4142135623730950488016887242097) // sqrt(2)

// float div to mult 1/X
#define DIV_2 ((double)0.5)
#define DIV_3 ((double)0.33333333333333333333333333333333)
#define DIV_4 ((double)0.25)
#define DIV_5 ((double)0.2)
#define DIV_6 ((double)0.16666666666666666666666666666667)
#define DIV_7 ((double)0.14285714285714285714285714285714)
#define DIV_8 ((double)0.125)
#define DIV_9 ((double)0.11111111111111111111111111111111)
#define DIV_10 ((double)0.1)
#define DIV_11 ((double)0.090909090909090909090909090909091)
#define DIV_12 ((double)0.083333333333333333333333333333333)
#define DIV_13 ((double)0.076923076923076923076923076923077)
#define DIV_14 ((double)0.071428571428571428571428571428571)
#define DIV_15 ((double)0.066666666666666666666666666666667) // 1/15
#define DIV_16 ((double)0.0625) // 1/16
#define DIV_19 ((double)0.052631578947368421052631578947368) // 1/19
#define DIV_20 ((double)0.05)
#define DIV_24 ((double)0.041666666666666666666666666666667) // 1/24
#define DIV_30 ((double)0.033333333333333333333333333333333) // 1/30
#define DIV_31 ((double)0.032258064516129032258064516129032) // 1/31
#define DIV_32 ((double)0.03125) // 1/32
#define DIV_36 ((double)0.027777777777777777777777777777778) // 1/36
#define DIV_40 ((double)0.025) // 1/40
#define DIV_48 ((double)0.020833333333333333333333333333333) // 1/48
#define DIV_50 ((double)0.02) // 1/50
#define DIV_60 ((double)0.016666666666666666666666666666667) // 1/60
#define DIV_63 ((double)0.015873015873015873015873015873016) // 1/63
#define DIV_64 ((double)0.015625) // 1/64
#define DIV_65 ((double)0.015384615384615384615384615384615) // 1/65
#define DIV_80 ((double)0.0125) // 1/80
#define DIV_96 ((double)0.010416666666666666666666666666) // 1/96
#define DIV_100 (double)(0.01) // 1/100
#define DIV_120 ((double)0.0083333333333333333333333333333333) // 1/120
#define DIV_127 (double)(0.007874015748031496062992125984252) // 1/127
#define DIV_128 (double)(0.0078125) // 1/128
#define DIV_140 ((double)0.0071428571428571428571428571428571) // 1/140
#define DIV_150 ((double)0.0066666666666666666666666666666667) // 1/150
#define DIV_160 ((double)0.00625) // 1/160
#define DIV_180 (double)(0.0055555555555555555555555555555556)
#define DIV_200 (double)(0.005) // 1/200
#define DIV_255 (double)(0.003921568627450980392156862745098)
#define DIV_256 (double)(0.00390625) // 1/256
#define DIV_360 (double)(0.0027777777777777777777777777777778)
#define DIV_440 (double)(0.0022727272727272727272727272727273)
#define DIV_600 (double)(0.0016666666666666666666666666666667) // 1/600
#define DIV_1000 (double)(0.001) // 1/1000
#define DIV_1023 (double)(9.7751710654936461388074291300098e-4) // 1/1023
#define DIV_1024 (double)(0.0009765625)
#define DIV_1200 (double)(8.3333333333333333333333333333333e-4) // 1/1200
#define DIV_1600 (double)(0.000625) // 1/1600
#define DIV_2400 (double)(4.1666666666666666666666666666667e-4) // 1/2400
#define DIV_4800 (double)(2.0833333333333333333333333333333e-4) // 1/4800
#define DIV_9600 (double)(1.0416666666666666666666666666667e-4) // 1/9600
#define DIV_10000 (double)(0.0001) // 1/10000
#define DIV_12288 (double)(8.1380208333333333333333333333333e-5) // 1/12288
#define DIV_22000 (double)(4.5454545454545454545454545454545e-5) // 1/22000
#define DIV_44100 (double)(2.2675736961451247165532879818594e-5) // 1/44100
#define DIV_1000000 (double)(0.000001) // 1/1000000

#define DIV_3_2 ((double)0.6666666666666666666666666666666) // 1/(3/2)
#define DIV_32_6 ((double)0.1875) // 1/(32/6)

#define DIV_1BIT ((double)0.5) // 1/(1<<1)
#define DIV_2BIT ((double)0.25) // 1/(1<<2)
#define DIV_3BIT ((double)0.125) // 1/(1<<3)
#define DIV_4BIT ((double)0.0625) // 1/(1<<4)
#define DIV_5BIT ((double)0.03125) // 1/(1<<5)
#define DIV_6BIT ((double)0.015625) // 1/(1<<6)
#define DIV_7BIT ((double)0.0078125) // 1/(1<<7)
#define DIV_8BIT ((double)0.00390625) // 1/(1<<8)
#define DIV_9BIT ((double)0.001953125) // 1/(1<<9)
#define DIV_10BIT ((double)0.0009765625) // 1/(1<<10)
#define DIV_11BIT ((double)0.00048828125) // 1/(1<<11)
#define DIV_12BIT ((double)0.000244140625) // 1/(1<<12)
#define DIV_13BIT ((double)0.0001220703125) // 1/(1<<13)
#define DIV_14BIT ((double)0.00006103515625) // 1/(1<<14)
#define DIV_15BIT ((double)0.000030517578125) // 1/(1<<15)
#define DIV_16BIT ((double)0.0000152587890625) // 1/(1<<16)
#define DIV_17BIT ((double)0.00000762939453125) // 1/(1<<17)
#define DIV_18BIT ((double)0.000003814697265625) // 1/(1<<18)
#define DIV_19BIT ((double)0.0000019073486328125) // 1/(1<<19)
#define DIV_20BIT ((double)0.00000095367431640625) // 1/(1<<20)
#define DIV_21BIT ((double)0.000000476837158203125) // 1/(1<<21)
#define DIV_22BIT ((double)0.0000002384185791015625) // 1/(1<<22)
#define DIV_23BIT ((double)0.00000011920928955078125) // 1/(1<<23)
#define DIV_24BIT ((double)0.000000059604644775390625) // 1/(1<<24)
#define DIV_25BIT ((double)0.0000000298023223876953125) // 1/(1<<25)
#define DIV_26BIT ((double)0.00000001490116119384765625) // 1/(1<<26)
#define DIV_27BIT ((double)0.000000007450580596923828125) // 1/(1<<27)
#define DIV_28BIT ((double)0.0000000037252902984619140625) // 1/(1<<28)
#define DIV_29BIT ((double)0.00000000186264514923095703125) // 1/(1<<29)
#define DIV_30BIT ((double)0.000000000931322574615478515625) // 1/(1<<30)
#define DIV_31BIT ((double)0.0000000004656612873077392578125) // 1/(1<<31)
#define DIV_32BIT ((double)0.00000000023283064365386962890625) // 1/(1<<32)
#define DIV_33BIT ((double)0.000000000116415321826934814453125) // 1/(1<<33)
#define DIV_34BIT ((double)0.0000000000582076609134674072265625) // 1/(1<<34)
#define DIV_35BIT ((double)(2.910383045673370361328125e-11)) // 1/(1<<35)
#define DIV_36BIT ((double)(1.4551915228366851806640625e-11)) // 1/(1<<36)
#define DIV_37BIT ((double)(7.2759576141834259033203125e-12)) // 1/(1<<37)
#define DIV_38BIT ((double)(3.63797880709171295166015625e-12)) // 1/(1<<38)
#define DIV_39BIT ((double)(1.818989403545856475830078125e-12)) // 1/(1<<39)
#define DIV_40BIT ((double)(9.094947017729282379150390625e-13)) // 1/(1<<40)
#define DIV_48BIT ((double)(3.5527136788005009293556213378906e-15)) // 1/(1<<48)
#define DIV_50BIT ((double)(8.8817841970012523233890533447266e-16)) // 1/(1<<50)
#define DIV_52BIT ((double)(2.2204460492503130808472633361816e-16)) // 1/(1<<52)
#define DIV_63BIT ((double)(1.0842021724855044340074528008699e-19)) // 1/(1<<63)
#define DIV_64BIT ((double)(5.4210108624275221700372640043497e-20)) // 1/(1<<64)

#define DIV_PI ((double)0.31830988618379067153776752674503) // 1/pi
#define DIV_PI2 ((double)0.15915494309189533576888376337251) // 1/(pi*2)
#define DIV_LN2 ((double)1.4426950408889634073599246810019) // 1/log(2)
#define DIV_LN10 ((double)0.43429448190325182765112891891661) // 1/log(10)
#define DIV_LOG2 ((double)3.3219280948873623478703194294894) // 1/log10(2)
#define DIV_SQRT2 ((double)0.70710678118654752440084436210485) // 1/sqrt(2)


#define USE_SHIFT_DIV 1 /* (n / 2) -> (n >> 1) */
#define USE_RECIP_DIV 1 /* (n / 2.0) -> (n * 0.5) */

#if -1L == (-1L >> 1)
#define USE_ARITHMETIC_SHIHT 1
#endif

#ifdef USE_SHIFTDIV
#define divi_2(i) ((i) >> 1)
#define divi_4(i) ((i) >> 2)
#define divi_8(i) ((i) >> 3)
#define divi_16(i) ((i) >> 4)
#define divi_32(i) ((i) >> 5)
#define divi_64(i) ((i) >> 6)
#define divi_128(i) ((i) >> 7)
#define divi_256(i) ((i) >> 8)
#define divi_512(i) ((i) >> 9)
#define divi_1024(i) ((i) >> 10)
#define divi_2048(i) ((i) >> 11)
#define divi_4096(i) ((i) >> 12)
#else
#define divi_2(i) ((i) / 2)
#define divi_4(i) ((i) / 4)
#define divi_8(i) ((i) / 8)
#define divi_16(i) ((i) / 16)
#define divi_32(i) ((i) / 32)
#define divi_64(i) ((i) / 64)
#define divi_128(i) ((i) / 128)
#define divi_256(i) ((i) / 256)
#define divi_512(i) ((i) / 512)
#define divi_1024(i) ((i) / 1024)
#define divi_2048(i) ((i) / 2048)
#define divi_4096(i) ((i) / 4096)
#endif

#ifdef USE_RECIPDIV
#define divf_2(f) ((f) * DIV_2)
#define divf_4(f) ((f) * DIV_4)
#define divf_8(f) ((f) * DIV_8)
#define divf_16(f) ((f) * DIV_16)
#define divf_32(f) ((f) * DIV_32)
#define divf_64(f) ((f) * DIV_64)
#define divf_128(f) ((f) * DIV_128)
#define divf_256(f) ((f) * DIV_256)
#define divf_512(f) ((f) * DIV_9BIT)
#define divf_1024(f) ((f) * DIV_10BIT)
#define divf_2048(f) ((f) * DIV_11BIT)
#define divf_4096(f) ((f) * DIV_12BIT)
#else
#define divf_2(flt) ((flt) / 2.0f)
#define divf_4(flt) ((flt) / 4.0f)
#define divf_8(flt) ((flt) / 8.0f)
#define divf_16(flt) ((flt) / 16.0f)
#define divf_32(flt) ((flt) / 32.0f)
#define divf_64(flt) ((flt) / 64.0f)
#define divf_128(flt) ((flt) / 128.0f)
#define divf_256(flt) ((flt) / 256.0f)
#define divf_512(flt) ((flt) / 512.0f)
#define divf_1024(flt) ((flt) / 1024.0f)
#define divf_2048(flt) ((flt) / 2048.0f)
#define divf_4096(flt) ((flt) / 4096.0f)
#endif

#if defined(DATA_T_INT32) && defined(USE_ARITHMETIC_SHIHT)
#define divt_2(t) divi_2(t)
#define divt_4(t) divi_4(t)
#define divt_8(t) divi_8(t)
#define divt_16(t) divi_16(t)
#define divt_32(t) divi_32(t)
#define divt_64(t) divi_64(t)
#define divt_128(t) divi_128(t)
#define divt_256(t) divi_256(t)
#define divt_512(t) divi_512(t)
#define divt_1024(t) divi_1024(t)
#define divt_2048(t) divi_2048(t)
#define divt_4096(t) divi_4096(t)
#elif defined(DATA_T_INT32)
#define divt_2(i) ((i) / 2)
#define divt_4(i) ((i) / 4)
#define divt_8(i) ((i) / 8)
#define divt_16(i) ((i) / 16)
#define divt_32(i) ((i) / 32)
#define divt_64(i) ((i) / 64)
#define divt_128(i) ((i) / 128)
#define divt_256(i) ((i) / 256)
#define divt_512(i) ((i) / 512)
#define divt_1024(i) ((i) / 1024)
#define divt_2048(i) ((i) / 2048)
#define divt_4096(i) ((i) / 4096)
#else
#define divt_2(t) divf_2(t)
#define divt_4(t) divf_4(t)
#define divt_8(t) divf_8(t)
#define divt_16(t) divf_16(t)
#define divt_32(t) divf_32(t)
#define divt_64(t) divf_64(t)
#define divt_128(t) divf_128(t)
#define divt_256(t) divf_256(t)
#define divt_512(t) divf_512(t)
#define divt_1024(t) divf_1024(t)
#define divt_2048(t) divf_2048(t)
#define divt_4096(t) divf_4096(t)
#endif

#ifndef __W32__
#undef MAIL_NAME
#endif /* __W32__ */

#ifdef __MINGW32__
#define aligned_malloc __mingw_aligned_malloc
#define aligned_free   __mingw_aligned_free
/* aligned_malloc is unsafe because s must be a multiple of a */
//#elif __STDC_VERSION__ >= 201112L
//#define aligned_malloc(s,a) aligned_malloc(a,s)
//#define aligned_free   free
#elif defined(__GNUC__) && _POSIX_VERSION >= 200112L
#define aligned_malloc(s,a) ({void *ptr; if(!s || posix_memalign(&ptr,a,s)) ptr = NULL; ptr;})
#define aligned_free   free
#elif _MSC_VER
#define aligned_malloc _aligned_malloc
#define aligned_free   _aligned_free
#endif

#if defined(__BORLANDC__) || defined(__WATCOMC__) || defined(__DMC__)
/* strncasecmp() -> strncmpi(char*, char*, size_t) */
//#define strncasecmp(a, b, c) strncmpi(a, b, c)
//#define strcasecmp(a, b) strcmpi(a, b)
#ifndef strncasecmp
#define strncasecmp(a, b, c) strnicmp(a, b, c)
#endif
#ifndef strcasecmp
#define strcasecmp(a, b) stricmp(a, b)
#endif
#endif /* __BORLANDC__ */

#ifdef _MSC_VER
#ifndef strncasecmp
#define strncasecmp(a, b, c)	_strnicmp((a), (b), (c))
#endif
#ifndef strcasecmp
#define strcasecmp(a, b)		_stricmp((a), (b))
#endif
#ifndef __POCC__
#define open _open
#define close _close
//#define write _write
//#define lseek _lseek
#define unlink _unlink
#if _MSC_VER < 1500    /* 1500(VC9)  */
#define write _write
#ifdef HAVE_VSNPRINTF
#define vsnprintf _vsnprintf
#endif
#endif
#pragma warning(4 : 4305 4244)
#else
#ifndef EPERM
#define EPERM 1
#endif
#ifndef EINTR
#define EINTR 4
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#ifndef _MAX_PATH
#define _MAX_PATH 260
#endif
#undef strncasecmp
#endif
#endif /* _MSC_VER */

#if defined(__CYGWIN32__) || defined(__MINGW32__)
#ifndef WIN32GCC
#define WIN32GCC 1
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#ifndef _MAX_PATH
#define _MAX_PATH 260
#endif
#endif /* __GNUC__ */

#define SAFE_CONVERT_LENGTH(len) (6 * (len) + 1)

#ifdef __MACOS__
#include "mac_com.h"
#endif

#ifdef __W32__
# undef DECOMPRESSOR_LIST
# undef PATCH_CONVERTERS
#endif

#ifndef HAVE_POPEN
# undef DECOMPRESSOR_LIST
# undef PATCH_CONVERTERS
#endif

// from timidity.c
#ifdef _MSC_VER
typedef ptrdiff_t ssize_t; 
#endif


#ifndef _WIN32
#define _set_errno(val) (errno = val)
#endif

#endif /* SYSDEP_H_INCUDED */
