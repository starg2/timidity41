#ifndef _XAW_H_
#define _XAW_H_
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

    xaw.h: written by Yoshishige Arai (ryo2@on.rim.or.jp) 12/8/98

    */

/*
 * XAW configurations
 */

/* Define to use libXaw3d */
/* #define XAW3D */

/* Define to use Japanese and so on */
#define I18N

/* Define to use scrollable Text widget instead of Label widget */
/* #define WIDGET_IS_LABEL_WIDGET */

/* Define to use short cut keys */
#define ENABLE_KEY_TRANSLATION

/*** Initial dot file name at home directory ***/
#define INITIAL_CONFIG ".xtimidity"


/*
 * CONSTANTS FOR XAW MENUS
 */
#define MAXVOLUME MAX_AMPLIFICATION
#define MAX_XAW_MIDI_CHANNELS 16

#define APP_CLASS "TiMidity"

#ifndef PATH_MAX
#define PATH_MAX 512
#endif
#define MAX_DIRECTORY_ENTRY BUFSIZ
#define LF 0x0a
#define SPACE 0x20
#define TAB 0x09

#endif /* _XAW_H_ */
