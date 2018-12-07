/* interface.h.  Generated from interface.h.in by configure.  */
/* Define if you have EMACS interface. */
#define IA_EMACS 1

/* Define if you have GTK interface. */
#define IA_GTK 1

/* Define if you have KMIDI interface. */
/* #undef IA_KMIDI */

/* Define if you have MOTIF interface. */
#define IA_MOTIF 1

/* Define if you have NCURSES interface. */
#define IA_NCURSES 1

/* Define if you have PLUGIN interface. */
/* #undef IA_PLUGIN */

/* Define if you have SLANG interface. */
#define IA_SLANG 1

/* Define if you have TCLTK interface. */
#define IA_TCLTK 1

/* Define if you have VT100 interface. */
#define IA_VT100 1

/* Define if you have XAW interface. */
#define IA_XAW 1

/* Define if you have XSKIN interface. */
#define IA_XSKIN 1

/* Define if you have DYNAMIC interface. */
/* #undef IA_DYNAMIC */

/* Define if you have Windows32 GUI interface. */
/* #undef IA_W32GUI */

/* Define if you have Windows GUI synthesizer mode interface. */
/* #undef IA_W32G_SYN */

/* Define if you have Remote MIDI interface. */
#define IA_SERVER 1

/* Define if you have Remote MIDI interface. */
#define IA_ALSASEQ 1

/* Define if you have Windows synthesizer mode interface. */
/* #undef IA_WINSYN */

/* Define if you have PortMIDI synthesizer mode interface. */
/* #undef IA_PORTMIDISYN */

/* Define if you have Windows named pipe synthesizer mode interface. */
/* #undef IA_NPSYN */

#if defined(IA_W32GUI) || defined(IA_W32G_SYN)
#ifndef __W32READDIR__
#define __W32READDIR__ 1
#endif
#define URL_DIR_CACHE_ENABLE 1
#define __W32G__ 1        /* for Win32 GUI */
#endif

