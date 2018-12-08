#pragma warning(disable : 4002)
//warning C4002: ﾏｸﾛ 'cmsg' に指定された実引数の数が多すぎます。
#pragma warning(disable : 4273)
//warning C4273: '_errno' : DLL ﾘﾝｹｰｼﾞが矛盾しています。DLL にｴｸｽﾎﾟｰﾄされていると仮定します。
#ifdef MSC_VER
#define cmsg(X) cmsg  //50KB ほど小さくなる
#else
#define cmsg(...) cmsg  //50KB ほど小さくなる
#endif

///r
#if 0
#ifdef DEFAULT_RESAMPLATION
#undef DEFAULT_RESAMPLATION
#endif

//#define DEFAULT_RESAMPLATION resample_inline
#endif

#ifndef NO_MIDI_CACHE
#define NO_MIDI_CACHE 1
#endif

#ifndef HAVE_MKSTEMP
#define HAVE_MKSTEMP 1
#endif

#ifdef HAVE_POPEN
#undef HAVE_POPEN
#endif

#ifdef REDUCE_VOICE_TIME_TUNING
#undef REDUCE_VOICE_TIME_TUNING
//playmidi.c の宣言部の #define 部を削除しないと機能しない
#endif

#ifdef AU_W32
#undef AU_W32
#endif

#ifdef AU_VORBIS
#undef AU_VORBIS
#endif

#ifdef AU_GOGO
#undef AU_GOGO
#endif

#ifdef AU_PORTAUDIO
#undef AU_PORTAUDIO
#endif

#ifdef URL_DIR_CACHE_ENABLE
#undef URL_DIR_CACHE_ENABLE
#endif

#ifdef SUPPORT_SOCKET
#undef SUPPORT_SOCKET
#endif

#ifndef HAVE_SYS_STAT_H
#define HAVE_SYS_STAT_H 1
#endif

#ifndef S_ISDIR
#define S_ISDIR(mode)   (((mode) & 0xF000) == 0x4000)
#endif /* S_ISDIR */


#ifndef HAVE_VSNPRINTF
#define HAVE_VSNPRINTF 1
#endif

#ifndef HAVE_SNPRINTF
#define HAVE_SNPRINTF 1
#endif

#ifdef MSC_VER
typedef __int64 int64;
#endif

#ifdef HAVE_SIGNAL
#undef HAVE_SIGNAL
#endif
