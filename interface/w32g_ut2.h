#ifndef __W32G_UT2_H__
#define __W32G_UT2_H__

extern int DlgChooseFontAndApply(HWND hwnd, HWND hwndFontChange, HFONT hFontPre, char *fontname, int *fontheight, int *fontwidth);
extern int DlgChooseFont(HWND hwnd, char *fontName, int *fontHeight, int *fontWidth);

extern int INILoadAll(void);
extern int INISaveAll(void);

// ini file of timidity window information
#define TIMIDITY_WINDOW_INI_FILE	timidity_window_inifile

#define FONT_FLAGS_NONE			0x00
#define FONT_FLAGS_FIXED		0x01
#define FONT_FLAGS_ITALIC		0x02
#define FONT_FLAGS_BOLD			0x04

// section of ini file
// [ListWnd]
// Width =
// Height =
// fontName =
// fontWidth =
// fontHeight =
typedef struct LISTWNDINFO_ {
	HWND hwnd;
	int Width;		// save parameter
	int Height;		// save parameter
	HMENU hPopupMenu;
	HWND hwndListBox;
	HFONT hFontListBox;
	char fontName[64];			// save parameter
	int fontWidth;				// save parameter
	int fontHeight;				// save parameter
	int fontFlags;			// save parameter
} LISTWNDINFO;
extern LISTWNDINFO ListWndInfo;

// section of ini file
// [DocWnd]
// Width =
// Height =
// fontName =
// fontWidth =
// fontHeight =
#define DOCWND_DOCFILEMAX 10
typedef struct DOCWNDINFO_ {
	char DocFile[DOCWND_DOCFILEMAX][512];
	int DocFileMax;
	int DocFileCur;
	char *Text;
	int TextSize;

	HWND hwnd;
	int Width;		// save parameter
	int Height;		// save parameter
	HMENU hPopupMenu;
	HWND hwndEdit;
	HFONT hFontEdit;
	char fontName[64];			// save parameter
	int fontWidth;				// save parameter
	int fontHeight;				// save parameter
	int fontFlags;			// save parameter
//	HANDLE hMutex;
} DOCWNDINFO;
extern DOCWNDINFO DocWndInfo;

extern int INISaveListWnd(void);
extern int INILoadListWnd(void);
extern int INISaveDocWnd(void);
extern int INILoadDocWnd(void);

#endif /* __W32G_UT2_H__ */
