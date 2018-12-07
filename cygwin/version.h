#ifndef ___VERSION_H_
#define ___VERSION_H_

#ifndef VERSION

#define BUILD_DATE "181027"
#define BUILD_BASE "c220"
#define VERSION_DATESTR "-tim" BUILD_DATE "-" BUILD_BASE

#undef VERSION_DATA_T

#ifdef DATA_T_DOUBLE
#define VERSION_TYPESTR "-f64"
#elif defined(DATA_T_FLOAT)
#define VERSION_TYPESTR "-f32"
#else
#define VERSION_TYPESTR "-i32"
#endif

#if defined(OPT_MODE) && (OPT_MODE != 0) && !defined(DATA_T_DOUBLE) && !defined(DATA_T_FLOAT)
#  define VERSION_TYPEEXT "fxd"
#elif defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
# if defined(USE_AVX2)
#  define VERSION_TYPEEXT "avx2"
# elif defined(USE_AVX)
#  define VERSION_TYPEEXT "avx"
# elif defined(USE_SSE4A)
#  define VERSION_TYPEEXT "sse4a"
# elif defined(USE_SSE41)
#  define VERSION_TYPEEXT "sse41"
# elif defined(USE_SSE42)
#  define VERSION_TYPEEXT "sse42"
# elif defined(USE_SSE4)
#  define VERSION_TYPEEXT "sse41+42"
# elif defined(USE_SSSE3)
#  define VERSION_TYPEEXT "ssse3"
# elif defined(USE_SSE3)
#  define VERSION_TYPEEXT "sse3"
# elif defined(USE_SSE2)
#  define VERSION_TYPEEXT "sse2"
# elif defined(USE_SSE)
#  define VERSION_TYPEEXT "sse1"
# elif defined(USE_MMX2)
#  define VERSION_TYPEEXT "mmx2"
# elif defined(USE_MMX)
#  define VERSION_TYPEEXT "mmx"
# elif defined(USE_3DNOW_ENH)
#  define VERSION_TYPEEXT "3dnowplus"
# elif defined(USE_3DNOW)
#  define VERSION_TYPEEXT "3dnow"
# endif /* USE_* */
#endif /* OPT_MODE, DATA_T_DOUBLE || DATA_T_FLOAT */

#ifndef VERSION_TYPEEXT
# define VERSION_TYPEEXT ""
#endif

#ifndef VERSION_TYPEARCH
# if defined(AMD64CPU) || defined(_AMD64_)
#  define VERSION_TYPEARCH "-x64"
# elif defined(IA64CPU)
#  define VERSION_TYPEARCH "-ia64"
# elif defined(ARMCPU)
#  define VERSION_TYPEARCH "-arm"
# elif defined(ARM64CPU)
#  define VERSION_TYPEARCH "-arm64"
# else
#  define VERSION_TYPEARCH ""
# endif
#endif

#define VERSION "current" VERSION_DATESTR VERSION_TYPESTR VERSION_TYPEEXT VERSION_TYPEARCH

#endif /* !VERSION */

#ifndef TIMID_VERSION
#define TIMID_VERSION "current"
#endif

#ifndef PACKAGE_NAME
#define PACKAGE_NAME "TiMidity++"
#endif

#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION TIMID_VERSION
#endif

#ifndef PACKAGE_STRING
#define PACKAGE_STRING PACKAGE_NAME " " VERSION
#endif

#endif /* !___VERSION_H_ */

