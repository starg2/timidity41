/* 
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999 Masanao Izumo <mo@goice.co.jp>
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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	Macintosh interface for TiMidity
	by T.Nogami	<t-nogami@happy.email.ne.jp>
	
    mac_errno.h
*/

#ifndef	MAC_COM_H
#define	MAC_COM_H

#ifdef __POWERPC__
 #define TIMID_CPU "PPC"
#elif __MC68K__
 #if  __MC68881__
  #define TIMID_CPU "68k+FPU"
 #else
  #define TIMID_CPU "68k"
 #endif
#endif

#define	TIMID_VERSION	"1.0.0 " TIMID_CPU
#define	TIMID_VERSION_PASCAL	"\p" TIMID_VERSION


#define PI 3.14159265358979323846
#define REVERB_PATCH
#define SUPPORT_SOUNDSPEC
#undef  DECOMPRESSOR_LIST
#define DECOMPRESSOR_LIST { 0 }
#undef  PATCH_EXT_LIST
#define PATCH_EXT_LIST { ".pat", 0 }

#undef  DEFAULT_RATE
#define DEFAULT_RATE	22050

#define	AU_MACOS
#define BIG_ENDIAN
#undef  TILD_SCHEME_ENABLE
#undef  JAPANESE
#define ANOTHER_MAIN
#define DEFAULT_PATH	""
#undef  CONFIG_FILE
#define CONFIG_FILE DEFAULT_PATH "timidity.cfg"
#define ENABLE_SHERRY

#define MAC_SOUNDBUF_QLENGTH (stdQLength*4)

extern int presence_balance;

#include "mac_util.h"
#endif /*MAC_COM_H*/
