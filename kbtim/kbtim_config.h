#pragma warning(disable : 4002)
//warning C4002: ϸ� 'cmsg' �Ɏw�肳�ꂽ�������̐����������܂��B
#pragma warning(disable : 4273)
//warning C4273: '_errno' : DLL �ݹ��ނ��������Ă��܂��BDLL �ɴ���߰Ă���Ă���Ɖ��肵�܂��B
#ifdef MSC_VER
#define cmsg(X) cmsg  //50KB �قǏ������Ȃ�
#else
#define cmsg(...) cmsg  //50KB �قǏ������Ȃ�
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
//playmidi.c �̐錾���� #define �����폜���Ȃ��Ƌ@�\���Ȃ�
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
