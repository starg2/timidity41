#ifndef _XAW_H_
#define _XAW_H_
/*

    TiMidity -- Experimental MIDI to WAVE converter
    Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

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

/*** Uncomment this to use libXaw3d ***/
/* #define XAW3D */

/*** Uncomment this to use Japanese and so on ***/
#define I18N

/*** Uncomment following not to use lyric widget ***/
#define MSGWINDOW

/*** Comment out following to use scrollable Text widget instead of Label widget ***/
#define WIDGET_IS_LABEL_WIDGET

/*** Comment following not to use short cut keys ***/
#define ENABLE_KEY_TRANSLATION

/*** Initial dot file name at home directory ***/
#define INITIAL_CONFIG ".xtimidity"

/*
 * CONSTANTS FOR XAW MENUS
 */
#define MAXVOLUME MAX_AMPLIFICATION

#define APP_CLASS "TiMidity"

#ifndef PATH_MAX
#define PATH_MAX 512
#endif
#define MAX_DIRECTORY_ENTRY BUFSIZ
#define LF 0x0a
#define SPACE 0x20
#define TAB 0x09

#endif /* _XAW_H_ */
