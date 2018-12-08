/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2018 Masanao Izumo <iz@onicos.co.jp>
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
#ifndef __W32G_UT2_H__
#define __W32G_UT2_H__

extern int DlgChooseFontAndApply(HWND hwnd, HWND hwndFontChange, HFONT hFontPre, char *fontname, int *fontheight, int *fontwidth);
extern int DlgChooseFont(HWND hwnd, char *fontName, int *fontHeight, int *fontWidth);

extern void SetWindowPosSize ( HWND parent_hwnd, HWND hwnd, int x, int y );

// ini file of timidity window information
#define TIMIDITY_WINDOW_INI_FILE        timidity_window_inifile

#define FONT_FLAGS_NONE                 0x00
#define FONT_FLAGS_FIXED                0x01
#define FONT_FLAGS_ITALIC               0x02
#define FONT_FLAGS_BOLD                 0x04

// section of ini file
// [MainWnd]
// PosX =
// PosY =
// Width =
// Height =
typedef struct MAINWNDINFO_ {
        HWND hwnd;
        int init;
        int PosX;               // save parameter
        int PosY;               // save parameter
        int Width;              // save parameter
        int Height;             // save parameter
        int CanvasMode;
} MAINWNDINFO;
extern MAINWNDINFO MainWndInfo;

// section of ini file
// [ListWnd]
// PosX =
// PosY =
// Width =
// Height =
// fontName =
// fontWidth =
// fontHeight =
#define LISTWND_COLUM 6
typedef struct LISTWNDINFO_ {
	HWND hwnd;
	int PosX;		// save parameter
	int PosY;		// save parameter
	int Width;		// save parameter
	int Height;		// save parameter
	HMENU hPopupMenu;
	HWND hwndList; // ListBox , ListView
	HFONT hFontList; // ListBox , ListView
	char *fontName;
	char fontNameJA[LF_FULLFACESIZE + 1];			// save parameter
	char fontNameEN[LF_FULLFACESIZE + 1];			// save parameter
	int fontWidth;				// save parameter
	int fontHeight;				// save parameter
	int fontFlags;			// save parameter
	TCHAR ListName[PLAYLIST_MAX][LF_FULLFACESIZE + 1]; // save parameter
	int columWidth[LISTWND_COLUM];
} LISTWNDINFO;
extern LISTWNDINFO ListWndInfo;

// section of ini file
// [DocWnd]
// PosX =
// PosY =
// Width =
// Height =
// fontName =
// fontWidth =
// fontHeight =
#define DOCWND_DOCFILEMAX 10
typedef struct DOCWNDINFO_ {
        char DocFile[DOCWND_DOCFILEMAX][FILEPATH_MAX];
        int DocFileMax;
        int DocFileCur;
        char *Text;
        int TextSize;

        HWND hwnd;
        int PosX;               // save parameter
        int PosY;               // save parameter
        int Width;              // save parameter
        int Height;             // save parameter
        HMENU hPopupMenu;
        HWND hwndEdit;
        HFONT hFontEdit;
        char *fontName;
        char fontNameJA[LF_FULLFACESIZE + 4];                   // save parameter
        char fontNameEN[LF_FULLFACESIZE + 4];                   // save parameter
        int fontWidth;                          // save parameter
        int fontHeight;                         // save parameter
        int fontFlags;                  // save parameter
//      HANDLE hMutex;
} DOCWNDINFO;
extern DOCWNDINFO DocWndInfo;

// section of ini file
// [ConsoleWnd]
// PosX =
// PosY =
// Width =
// Height =
typedef struct CONSOLEWNDINFO_ {
        HWND hwnd;
        int PosX;               // save parameter
        int PosY;               // save parameter
        int Width;              // save parameter
        int Height;             // save parameter
} CONSOLEWNDINFO;
extern CONSOLEWNDINFO ConsoleWndInfo;

// section of ini file
// [SoundSpecWnd]
// PosX =
// PosY =
// Width =
// Height =
typedef struct SOUNDSPECWNDINFO_ {
        HWND hwnd;
        int PosX;               // save parameter
        int PosY;               // save parameter
        int Width;              // save parameter
        int Height;             // save parameter
} SOUNDSPECWNDINFO;
extern SOUNDSPECWNDINFO SoundSpecWndInfo;

extern int INISaveMainWnd(void);
extern int INILoadMainWnd(void);
extern int INISaveListWnd(void);
extern int INILoadListWnd(void);
extern int INISaveDocWnd(void);
extern int INILoadDocWnd(void);
extern int INISaveConsoleWnd(void);
extern int INILoadConsoleWnd(void);
extern int INISaveSoundSpecWnd(void);
extern int INILoadSoundSpecWnd(void);

extern int PauseOldTiMidity(void);

#endif /* __W32G_UT2_H__ */
