

#ifndef INTERFACE_H_INCLUDED
#define INTERFACE_H_INCLUDED 1

/* interface.h.  Generated from interface.h.in by configure.  */
/* Define if you have EMACS interface. */
/* #undef IA_EMACS */

/* Define if you have GTK interface. */
/* #undef IA_GTK */

/* Define if you have KMIDI interface. */
/* #undef IA_KMIDI */

/* Define if you have MOTIF interface. */
/* #undef IA_MOTIF */

/* Define if you have NCURSES interface. */
#define IA_NCURSES 1

/* Define if you have PDCURSES library. */
#define USE_PDCURSES 1

/* Define if you have PLUGIN interface. */
/* #undef IA_PLUGIN */

/* Define if you have SLANG interface. */
/* #undef IA_SLANG */

/* Define if you have TCLTK interface. */
/* #undef IA_TCLTK */

/* Define if you have VT100 interface. */
#define IA_VT100 1

/* Define if you have XAW interface. */
/* #undef IA_XAW */

/* Define if you have XSKIN interface. */
/* #undef IA_XSKIN */

/* Define if you have DYNAMIC interface. */
#define IA_DYNAMIC 1

/* Define if you have Windows32 GUI interface. */
/* #undef IA_W32GUI */

/* Define if you have Windows GUI synthesizer mode interface. */
/* #undef IA_W32G_SYN */

/* Define if you have Remote MIDI interface. */
/* #undef IA_SERVER */

/* Define if you have Remote MIDI interface. */
/* #undef IA_ALSASEQ */

/* Define if you have Windows synthesizer mode interface. */
#define IA_WINSYN 1

/* Define if you have PortMIDI synthesizer mode interface. */
#define IA_PORTMIDISYN 1

/* Define if you have Windows named pipe synthesizer mode interface. */
#define IA_NPSYN 1

/* Define if you have Windows32 GUI synthesizer mode interface. */
#ifdef TWSYNG32
#define IA_W32G_SYN 1
#endif


/* Define if you have Windows32 GUI interface. */
#ifdef TWSYNG32
#undef IA_W32GUI
#undef IA_NCURSES
#undef IA_VT100
#undef IA_DYNAMIC
//#undef IA_WINSYN
#undef IA_PORTMIDISYN
#undef IA_NPSYN
#elif defined(__W32G__)
#define IA_W32GUI 1
#endif

#if defined(IA_W32GUI) || defined(IA_W32G_SYN)
#ifndef __W32READDIR__
#define __W32READDIR__
#endif
#ifndef URL_DIR_CACHE_ENABLE
#define URL_DIR_CACHE_ENABLE
#endif
#define __W32G__        /* for Win32 GUI */
//#define IA_XSKIN
#endif

/* Define if you have Windows32 CUI interface. */
#ifdef TIM_CUI
#undef __W32G__	/* for Win32 GUI */
#undef IA_W32G_SYN	/* for Win32 GUI */
#undef IA_W32GUI
#undef IA_DYNAMIC // need GNU DLD ?
//#undef IA_NCURSES
//#undef IA_VT100
//#undef IA_WINSYN
//#undef IA_PORTMIDISYN
//#undef IA_NPSYN
#endif

/* Define if you have Windows32 Service mode interface. */
#ifdef TWSYNSRV
#define IA_W32G_SYN 1
#undef __W32G__	/* for Win32 GUI */
#undef IA_W32GUI
#undef IA_NCURSES
#undef IA_VT100
#undef IA_DYNAMIC
//#undef IA_WINSYN
#undef IA_PORTMIDISYN
#undef IA_NPSYN
#endif

/* Define if you have Windows32 Driver mode interface. */
#ifdef WINDRV
#undef __W32G__	/* for Win32 GUI */
#undef IA_W32GUI
#undef IA_NCURSES
#undef IA_VT100
#undef IA_DYNAMIC
//#undef IA_WINSYN
#undef IA_PORTMIDISYN
#undef IA_NPSYN
#endif

/* Define if you have Windows32 GUI interface. */
#if defined(__W32G__) && !defined(TWSYNG32) && !defined(KBTIM)
#undef IA_NCURSES
#undef IA_VT100
#undef IA_DYNAMIC
#undef IA_WINSYN
#undef IA_PORTMIDISYN
#undef IA_NPSYN
#endif

/* Define if you have cfgforsf interface. */
#ifdef CFG_FOR_SF
#undef IA_NCURSES
#undef IA_VT100
#undef IA_DYNAMIC
#undef IA_WINSYN
#undef IA_PORTMIDISYN
#undef IA_NPSYN
#endif


///r
#ifdef KBTIM /*added by Kobarin*/
#undef IA_W32GUI
#undef IA_W32G_SYN
#undef TIM_CUI
#undef IA_NCURSES
#undef IA_VT100
#undef IA_DYNAMIC
#undef IA_WINSYN
#undef IA_PORTMIDISYN
#undef IA_NPSYN
#endif /*KBTIM*/

#ifdef KBTIM_SETUP
#define IA_W32GUI 1
#undef IA_W32G_SYN

#endif /*KBTIM_SETUP*/


#ifdef WINVSTI
#undef IA_W32GUI
#undef IA_W32G_SYN
#undef TIM_CUI
#undef IA_NCURSES
#undef IA_VT100
#undef IA_DYNAMIC
#undef IA_WINSYN
#undef IA_PORTMIDISYN
#undef IA_NPSYN
#endif /*KBTIM*/



#endif /* INTERFACE_H_INCLUDED */
