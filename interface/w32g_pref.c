/*
	TiMidity++ -- MIDI to WAVE converter and player
	Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
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

	w32g_pref.c: Written by Daisuke Aoki <dai@y7.net>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */
#include <process.h>
#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif /* HAVE_STDDEF_H */
#include <windows.h>
#include <shlobj.h>
#undef RC_NONE
#include <commctrl.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <math.h>

#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "output.h"
#include "controls.h"
#include "tables.h"
#include "miditrace.h"
#include "effect.h"
#ifdef SUPPORT_SOUNDSPEC
#include "soundspec.h"
#endif /* SUPPORT_SOUNDSPEC */
#include "recache.h"
#include "arc.h"
#include "strtab.h"
#include "wrd.h"
#include "mid.defs"
#include "timer2.h"
///r
#include "resample.h"
#include "mix.h"
#include "thread.h"
#include "dls.h"
#include "sfz.h"

#include <tchar.h>
#include "w32g.h"
#include "w32g_res.h"
#include "w32g_utl.h"
#include "w32g_ut2.h"
#include "w32g_pref.h"
///r
#ifdef AU_W32
#include "w32_a.h"
#endif
#ifdef AU_WASAPI
#include "wasapi_a.h"
#endif
#ifdef AU_WDMKS
#include "wdmks_a.h"
#endif
#ifdef AU_PORTAUDIO
#include "portaudio_a.h"
#ifdef AU_PORTAUDIO_DLL
#if defined(PORTAUDIO_V19) && defined(__W32__)
#include <pa_win_wasapi.h>
#endif
#endif
#endif
#include "sndfontini.h"

#ifdef AU_GOGO
/* #include <musenc.h>		/* for gogo */
#include <gogo/gogo.h>		/* for gogo */
#include "gogo_a.h"
#endif

#ifdef AU_FLAC
#include "flac_a.h"
#endif


/*****************************************************************************************************************************/

static char *CurrentConfigFile;

static int WinVer = -1;

static int get_winver(void)
{
    DWORD winver, major, minor;
    int ver = 0;

    if (WinVer != -1)
        return WinVer;
    winver = GetVersion();
    if (winver & 0x80000000) { // Win9x
        WinVer = 0;
        return 0;
    }
    major = (DWORD)(LOBYTE(LOWORD(winver)));
    minor = (DWORD)(HIBYTE(LOWORD(winver)));
    switch (major) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        ver = 0;
        break;
    case 6:
        switch (minor) {
        case 0: ver = 1; break; // vista
        case 1: ver = 2; break; // 7
        case 2: ver = 3; break; // 8
        case 3: ver = 4; break; // 8.1
        default: ver = 5; break; // 8.2?
        }
        break;
    case 10:
        switch (minor) {
        case 0: ver = 6; break; // 10
        default: ver = 7; break; // 10.1?
        }
        break;
    default:
        ver = 8; // 11?
        break;
    }
    WinVer = ver;
    return ver;
}

static int char_count(const char *s, int c)
{
    int n = 0;
    while (*s)
        n += (*s++ == c);
    return n;
}

static int get_verbosity_level(void)
{
    int lv = char_count(st_temp->opt_ctl + 1, 'v') + 1;
    lv -= char_count(st_temp->opt_ctl + 1, 'q');
    return lv;
}

///r
// for COMBOBOX
static int cb_find_item(int cb_num , const int *cb_info, int val, int miss)
{
    int i;
    for (i = 0; i < cb_num; i++)
        if (val == cb_info[i]) { return i; }
    return miss;
}

static void SetDlgItemFloat(HWND hwnd, UINT id, double v)
{
    char buf[128];
#ifdef _MSC_VER
    snprintf(buf, ARRAY_SIZE(buf) - 1, "%g", v);
#else
    snprintf(buf, ARRAY_SIZE(buf) - 1, "%#.6f", v);
#endif
    floatpoint_grooming(buf);
    SetDlgItemTextA(hwnd, id, buf); // A
}

static double GetDlgItemFloat(HWND hwnd, UINT id)
{
    char buf[128];
    buf[0] = '\0';
    GetDlgItemTextA(hwnd, id, buf, ARRAY_SIZE(buf)); // A
    return strtod(buf, NULL);
}


/*****************************************************************************************************************************/

extern void w32g_restart(void);

extern void set_gogo_opts_use_commandline_options(char *commandline);

extern void restore_voices(int save_voices);

extern void ConsoleWndApplyLevel(void);

extern void TracerWndApplyQuietChannel(ChannelBitMask quietchannels_);

volatile int PrefWndDoing = 0;

static int PrefInitialPage = 0;

static void PrefSettingApply(void);

enum pref_load_mode {
    pref_load_delay_mode = 0,
    pref_load_each_mode,
    pref_load_all_mode
};

static int prefLoadMode = pref_load_each_mode;
static int CurrentPlayerLanguage = -1;

#if defined(KBTIM_SETUP) || defined(WINDRV_SETUP)
extern void set_config_hwnd(HWND hwnd);
#endif

#if defined(IA_W32G_SYN) || defined(IA_W32GUI)
extern int RestartTimidity;
#endif

static int DlgOpenConfigFile(char *Filename, HWND hwnd);
static int DlgOpenOutputFile(char *Filename, HWND hwnd);
static int DlgOpenOutputDir(char *Dirname, HWND hwnd);

static HFONT hFixedPointFont;

static int w32_reset_exe_directory(void)
{
    char path[FILEPATH_MAX], *p;
    GetModuleFileNameA(NULL, path, FILEPATH_MAX - 1);
    p = pathsep_strrchr(path);
    if (p) {
        p++;
        *p = '\0';
    }
    else {
        GetWindowsDirectoryA(path, FILEPATH_MAX - 1);
    }
    return SetCurrentDirectoryA(path) != 0;
}

//#if defined(__CYGWIN32__) || defined(__MINGW32__)
#if 0 /* New version of mingw */
//#define pszTemplate   u1.pszTemplate
//#define pszIcon       u2.pszIcon
#ifndef NONAMELESSUNION
#define NONAMELESSUNION
#endif
#ifndef DUMMYUNIONNAME
#define DUMMYUNIONNAME  u1
#endif
#ifndef DUMMYUNIONNAME2
#define DUMMYUNIONNAME2 u2
#endif
#ifndef DUMMYUNIONNAME3
#define DUMMYUNIONNAME3 u3
#endif
#endif


extern int waveConfigDialog(void);
#ifdef AU_W32
extern void wmmeConfigDialog(HWND hwnd);
#endif
#ifdef AU_WASAPI
extern void wasapiConfigDialog(void);
#endif
#ifdef AU_WDMKS
extern void wdmksConfigDialog(void);
#endif
#ifdef AU_VORBIS
extern int vorbisConfigDialog(void);
#endif
#ifdef AU_GOGO
extern int gogoConfigDialog(void);
#endif
#ifdef AU_PORTAUDIO_DLL
extern int portaudioConfigDialog(void);
extern int asioConfigDialog(int deviceID);
#endif
#ifdef AU_LAME
extern int lameConfigDialog(void);
#endif
#ifdef AU_FLAC
extern int flacConfigDialog(void);
#endif
#ifdef AU_OPUS
extern int opusConfigDialog(void);
#endif
#ifdef AU_SPEEX
extern int speexConfigDialog(void);
#endif

#ifdef IA_W32G_SYN
static TCHAR **GetMidiINDrivers(void);
#endif

#define WM_MYSAVE    (WM_USER + 100)
#define WM_MYRESTORE (WM_USER + 101)

/* WindowsXP Theme API */
#ifndef ETDT_DISABLE
#define ETDT_DISABLE        (1)
#define ETDT_ENABLE         (2)
#define ETDT_USETABTEXTURE  (4)
#define ETDT_ENABLETAB      (ETDT_ENABLE | ETDT_USETABTEXTURE)
#endif


/*****************************************************************************************************************************/

/* TiMidity Win32GUI preference / PropertySheet */

static volatile int PrefWndSetOK = 0;
static HWND hPrefWnd = NULL;
static LRESULT APIENTRY CALLBACK PrefWndDialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
#if (defined(IA_W32G_SYN) || defined(WINDRV_SETUP))
static BOOL APIENTRY PrefSyn1DialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
#else
static LRESULT APIENTRY PrefPlayerDialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
#endif
static LRESULT APIENTRY PrefTiMidity1DialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static LRESULT APIENTRY PrefTiMidity2DialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static LRESULT APIENTRY PrefTiMidity3DialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);

static LRESULT APIENTRY PrefSFINI1DialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static LRESULT APIENTRY PrefSFINI2DialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static LRESULT APIENTRY PrefCustom1DialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static LRESULT APIENTRY PrefCustom2DialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static LRESULT APIENTRY PrefIntSynthDialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);

typedef struct pref_page_t_ {
	int index;
	TCHAR *title;
	HWND hwnd;
	UINT control;
	DLGPROC dlgproc;
	int opt;
} pref_page_t;

///r
static pref_page_t pref_pages_ja[] = {
#if defined(IA_W32G_SYN) || defined(WINDRV_SETUP)
	{ 0, TEXT("シンセサイザ"), (HWND)NULL, IDD_PREF_SYN1, (DLGPROC) PrefSyn1DialogProc, 0 },
	{ 1, TEXT("エフェクト"), (HWND)NULL, IDD_PREF_TIMIDITY1, (DLGPROC) PrefTiMidity1DialogProc, 0 },
	{ 2, TEXT("その他"), (HWND)NULL, IDD_PREF_TIMIDITY2, (DLGPROC) PrefTiMidity2DialogProc, 0 },
	{ 3, TEXT("出力"), (HWND)NULL, IDD_PREF_TIMIDITY3, (DLGPROC) PrefTiMidity3DialogProc, 0 },
	{ 4, TEXT("sf_ini1"), (HWND)NULL, IDD_PREF_SFINI1, (DLGPROC) PrefSFINI1DialogProc, 0 },
	{ 5, TEXT("sf_ini2"), (HWND)NULL, IDD_PREF_SFINI2, (DLGPROC) PrefSFINI2DialogProc, 0 },
	{ 6, TEXT("Custom1"), (HWND)NULL, IDD_PREF_CUSTOM1, (DLGPROC) PrefCustom1DialogProc, 0 },
	{ 7, TEXT("Custom2"), (HWND)NULL, IDD_PREF_CUSTOM2, (DLGPROC) PrefCustom2DialogProc, 0 },
	{ 8, TEXT("内蔵シンセ"), (HWND)NULL, IDD_PREF_INT_SYNTH, (DLGPROC) PrefIntSynthDialogProc, 0 },
#else
	{ 0, TEXT("プレイヤ"), (HWND)NULL, IDD_PREF_PLAYER, (DLGPROC) PrefPlayerDialogProc, 0 },
	{ 1, TEXT("エフェクト"), (HWND)NULL, IDD_PREF_TIMIDITY1, (DLGPROC) PrefTiMidity1DialogProc, 0 },
	{ 2, TEXT("その他"), (HWND)NULL, IDD_PREF_TIMIDITY2, (DLGPROC) PrefTiMidity2DialogProc, 0 },
	{ 3, TEXT("出力"), (HWND)NULL, IDD_PREF_TIMIDITY3, (DLGPROC) PrefTiMidity3DialogProc, 0 },
	{ 4, TEXT("sf_ini1"), (HWND)NULL, IDD_PREF_SFINI1, (DLGPROC) PrefSFINI1DialogProc, 0 },
	{ 5, TEXT("sf_ini2"), (HWND)NULL, IDD_PREF_SFINI2, (DLGPROC) PrefSFINI2DialogProc, 0 },
	{ 6, TEXT("custom1"), (HWND)NULL, IDD_PREF_CUSTOM1, (DLGPROC) PrefCustom1DialogProc, 0 },
	{ 7, TEXT("custom2"), (HWND)NULL, IDD_PREF_CUSTOM2, (DLGPROC) PrefCustom2DialogProc, 0 },
	{ 8, TEXT("内蔵シンセ"), (HWND)NULL, IDD_PREF_INT_SYNTH, (DLGPROC) PrefIntSynthDialogProc, 0 },
#endif
};
///r
static pref_page_t pref_pages_en[] = {
#if defined(IA_W32G_SYN) || defined(WINDRV_SETUP)
	{ 0, TEXT("Synthesizer"), (HWND)NULL, IDD_PREF_SYN1_EN, (DLGPROC) PrefSyn1DialogProc, 0 },
	{ 1, TEXT("Effect"), (HWND)NULL, IDD_PREF_TIMIDITY1_EN, (DLGPROC) PrefTiMidity1DialogProc, 0 },
	{ 2, TEXT("Misc"), (HWND)NULL, IDD_PREF_TIMIDITY2_EN, (DLGPROC) PrefTiMidity2DialogProc, 0 },
	{ 3, TEXT("Output"), (HWND)NULL, IDD_PREF_TIMIDITY3_EN, (DLGPROC) PrefTiMidity3DialogProc, 0 },
	{ 4, TEXT("sf_ini1"), (HWND)NULL, IDD_PREF_SFINI1_EN, (DLGPROC) PrefSFINI1DialogProc, 0 },
	{ 5, TEXT("sf_ini2"), (HWND)NULL, IDD_PREF_SFINI2_EN, (DLGPROC) PrefSFINI2DialogProc, 0 },
	{ 6, TEXT("Custom1"), (HWND)NULL, IDD_PREF_CUSTOM1, (DLGPROC) PrefCustom1DialogProc, 0 },
	{ 7, TEXT("Custom2"), (HWND)NULL, IDD_PREF_CUSTOM2, (DLGPROC) PrefCustom2DialogProc, 0 },
	{ 8, TEXT("InternalSynth"), (HWND)NULL, IDD_PREF_INT_SYNTH_EN, (DLGPROC) PrefIntSynthDialogProc, 0 },
#else
	{ 0, TEXT("Player"), (HWND)NULL, IDD_PREF_PLAYER_EN, (DLGPROC) PrefPlayerDialogProc, 0 },
	{ 1, TEXT("Effect"), (HWND)NULL, IDD_PREF_TIMIDITY1_EN, (DLGPROC) PrefTiMidity1DialogProc, 0 },
	{ 2, TEXT("Misc"), (HWND)NULL, IDD_PREF_TIMIDITY2_EN, (DLGPROC) PrefTiMidity2DialogProc, 0 },
	{ 3, TEXT("Output"), (HWND)NULL, IDD_PREF_TIMIDITY3_EN, (DLGPROC) PrefTiMidity3DialogProc, 0 },
	{ 4, TEXT("sf_ini1"), (HWND)NULL, IDD_PREF_SFINI1_EN, (DLGPROC) PrefSFINI1DialogProc, 0 },
	{ 5, TEXT("sf_ini2"), (HWND)NULL, IDD_PREF_SFINI2_EN, (DLGPROC) PrefSFINI2DialogProc, 0 },
	{ 6, TEXT("custom1"), (HWND)NULL, IDD_PREF_CUSTOM1_EN, (DLGPROC) PrefCustom1DialogProc, 0 },
	{ 7, TEXT("custom2"), (HWND)NULL, IDD_PREF_CUSTOM2_EN, (DLGPROC) PrefCustom2DialogProc, 0 },
	{ 8, TEXT("InternalSynth"), (HWND)NULL, IDD_PREF_INT_SYNTH_EN, (DLGPROC) PrefIntSynthDialogProc, 0 },
#endif
};

///r
#if !(defined(IA_W32G_SYN) || defined(WINDRV_SETUP))
#define PREF_PAGE_MAX 9
#else
#define PREF_PAGE_MAX 9
#endif

static pref_page_t *pref_pages;

static int prefWndLoadedPage;


static void PrefWndCreateTabItems(HWND hwnd)
{
    int i;
    HWND hwnd_tab;

    switch (CurrentPlayerLanguage) {
    case LANGUAGE_JAPANESE:
	pref_pages = pref_pages_ja;
	break;
    default:
    case LANGUAGE_ENGLISH:
	pref_pages = pref_pages_en;
	break;
    }

    hwnd_tab = GetDlgItem(hwnd, IDC_TAB_MAIN);
    for (i = 0; i < PREF_PAGE_MAX; i++) {
	TC_ITEM tci;
	tci.mask = TCIF_TEXT;
	tci.pszText = pref_pages[i].title;
	tci.cchTextMax = strlen(pref_pages[i].title);
	SendMessage(hwnd_tab, TCM_INSERTITEM, (WPARAM)i, (LPARAM)&tci);

	pref_pages[i].hwnd = NULL;
    }
}

static void PrefWndCreatePage(HWND hwnd, UINT page)
{
    typedef BOOL (WINAPI *IsThemeActiveFn)(void);
    typedef HRESULT (WINAPI *EnableThemeDialogTextureFn)(HWND hwnd, DWORD dwFlags);
    RECT rc;
    HWND hwnd_tab;
    HANDLE hUXTheme;
    IsThemeActiveFn pfnIsThemeActive;
    EnableThemeDialogTextureFn pfnEnableThemeDialogTexture;
    BOOL theme_active = FALSE;

    if (page >= PREF_PAGE_MAX || pref_pages[page].hwnd)
	return;

    switch (CurrentPlayerLanguage) {
    case LANGUAGE_JAPANESE:
	pref_pages = pref_pages_ja;
	break;
    default:
    case LANGUAGE_ENGLISH:
	pref_pages = pref_pages_en;
	break;
    }

    hwnd_tab = GetDlgItem(hwnd, IDC_TAB_MAIN);
    if (!hwnd_tab)
	return;

    GetClientRect(hwnd_tab, &rc);
    SendDlgItemMessage(hwnd, IDC_TAB_MAIN, TCM_ADJUSTRECT, (WPARAM)0, (LPARAM)&rc);
    {
	RECT rc_tab;
	POINT pt_wnd;
	GetWindowRect(hwnd_tab, &rc_tab);
	pt_wnd.x = rc_tab.left, pt_wnd.y = rc_tab.top;
	ScreenToClient(hwnd, &pt_wnd);
	rc.left   += pt_wnd.x;
	rc.top    += pt_wnd.y;
	rc.right  += pt_wnd.x;
	rc.bottom += pt_wnd.y;
    }

    hUXTheme = GetModuleHandle(TEXT("UXTHEME")); //LoadLibrary(TEXT("UXTHEME"));
    if (hUXTheme) {
	pfnIsThemeActive = (IsThemeActiveFn) GetProcAddress(hUXTheme, "IsThemeActive");
	pfnEnableThemeDialogTexture = (EnableThemeDialogTextureFn) GetProcAddress(hUXTheme, "EnableThemeDialogTexture");
	if (pfnIsThemeActive && pfnEnableThemeDialogTexture && (*pfnIsThemeActive)() != FALSE)
	    theme_active = TRUE;
    }

    pref_pages[page].hwnd = CreateDialog(hInst, MAKEINTRESOURCE(pref_pages[page].control),
	hwnd, pref_pages[page].dlgproc);
    //SetParent(pref_pages[page].hwnd, hwnd_tab); /* freeze */
    ShowWindow(pref_pages[page].hwnd, SW_HIDE);
    MoveWindow(pref_pages[page].hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
    if (theme_active)
	(*pfnEnableThemeDialogTexture)(pref_pages[page].hwnd, ETDT_ENABLETAB);

    if (hUXTheme) {
	//FreeLibrary(hUXTheme);
	hUXTheme = NULL;
    }
}

static UINT PrefSearchPageFromCID(UINT cid)
{
    int32 i;
    UINT num = 0;
    pref_page_t *page;

    switch (CurrentPlayerLanguage) {
    case LANGUAGE_JAPANESE:
	page = pref_pages_ja;
	break;
    default:
    case LANGUAGE_ENGLISH:
	page = pref_pages_en;
	break;
    }

    for (i = 0; i < PREF_PAGE_MAX; i++) {
	if (page[i].control == cid) {
	    num = i;
	}
    }

    return num;
}

static void PrefWndDelayLoad(void)
{
    static DWORD prevTime;
    if (prefWndLoadedPage == 0)
        prevTime = 0;
    if (GetTickCount() - prevTime <= 1.5 * 1000)
        return;
    PrefWndCreatePage(hPrefWnd, prefWndLoadedPage);
    prefWndLoadedPage++;
    prevTime = GetTickCount();
    if (prefWndLoadedPage >= PREF_PAGE_MAX)
        stop_timer(pref_timer);
    return;
}

void PrefWndCreate(HWND hwnd, UINT cid)
{
    CurrentPlayerLanguage = PlayerLanguage;
    UINT page = cid ? PrefSearchPageFromCID(cid) : PrefInitialPage;

    VOLATILE_TOUCH(PrefWndDoing);
    if (PrefWndDoing)
	return;
    PrefWndDoing = 1;
    PrefWndSetOK = 1;

    PrefInitialPage = page;
#if defined(KBTIM_SETUP) || defined(WINDRV_SETUP)
	switch(CurrentPlayerLanguage) {
		case LANGUAGE_JAPANESE:
			set_config_hwnd((HWND)CreateDialog ( hInst, MAKEINTRESOURCE(IDD_DIALOG_PREF), hwnd, PrefWndDialogProc ));
			break;
		default:
		case LANGUAGE_ENGLISH:
			set_config_hwnd((HWND)CreateDialog ( hInst, MAKEINTRESOURCE(IDD_DIALOG_PREF_EN), hwnd, PrefWndDialogProc ));
			break;
	}
#else
	switch(CurrentPlayerLanguage) {
		case LANGUAGE_JAPANESE:
			DialogBox ( hInst, MAKEINTRESOURCE(IDD_DIALOG_PREF), hwnd, PrefWndDialogProc );
			break;
		default:
		case LANGUAGE_ENGLISH:
			DialogBox ( hInst, MAKEINTRESOURCE(IDD_DIALOG_PREF_EN), hwnd, PrefWndDialogProc );
			break;
	}	
	hPrefWnd = NULL;
    CurrentPlayerLanguage = -1;
#endif
	PrefWndSetOK = 0;
	PrefWndDoing = 0;
	return;
}

#if defined(KBTIM_SETUP) || defined(WINDRV_SETUP)
extern void config_gui_main(void);
extern void config_gui_save(void);
extern void config_gui_main_close(void);
#endif

LRESULT APIENTRY CALLBACK PrefWndDialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	int i;

	switch (uMess){
	case WM_INITDIALOG:
	{
		int page = PrefInitialPage;
		if (page < 0 || page > PREF_PAGE_MAX) {
			page = 0; // default
		}

        hPrefWnd = hwnd;

        if (hFixedPointFont == NULL) {
            HDC hdc;
            int ptHeight = 9;
            hdc = GetDC(hwnd);
            switch (CurrentPlayerLanguage) {
            case LANGUAGE_ENGLISH: ptHeight = 8; break;
            default: break;
            }
            hFixedPointFont =
                CreateFont(-MulDiv(ptHeight, GetDeviceCaps(hdc, LOGPIXELSY), 72), 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                        FIXED_PITCH|FF_DONTCARE, NULL);
            ReleaseDC(hwnd, hdc);
        }

		// main
#if defined(IA_W32G_SYN) || defined(IA_W32GUI)
		EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_SAVE_RESTART), TRUE);
#else
		EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_SAVE_RESTART), FALSE);
#endif
#if defined(KBTIM_SETUP) || defined(WINDRV_SETUP)		
		config_gui_main(); // main()
		w32g_initialize();	
		EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_SAVE_RESTART), FALSE);
#endif
#if defined(KBTIM_SETUP)
		EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_INI_FILE), TRUE);
#else
		EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_INI_FILE), FALSE);
#endif
		SetDlgItemText(hwnd, IDC_EDIT_INI_FILE, TEXT(IniFile));
#ifdef VST_LOADER_ENABLE
		if (hVSTHost != NULL)
			EnableWindow(GetDlgItem(hwnd, IDC_OPENVSTMGR), TRUE);
		else
			EnableWindow(GetDlgItem(hwnd, IDC_OPENVSTMGR), FALSE);
#endif
		// table
		PrefWndCreateTabItems(hwnd);
		PrefWndCreatePage(hwnd, page);

        // preload
        if (prefLoadMode == pref_load_all_mode) {
            for (i = 0; i < PREF_PAGE_MAX; ++i)
                PrefWndCreatePage(hPrefWnd, i);
        }

		SetForegroundWindow(hwnd);
		SendDlgItemMessage ( hwnd, IDC_TAB_MAIN, TCM_SETCURSEL, (WPARAM)(page), (LPARAM)0 );
		ShowWindow ( pref_pages[page].hwnd, TRUE );	
        SetFocus(GetDlgItem(hwnd, IDC_TAB_MAIN));

        // background
        if (prefLoadMode == pref_load_delay_mode) {
            prefWndLoadedPage = 0;
            start_timer(pref_timer, PrefWndDelayLoad, 0.1 * 1000);
        }
		return TRUE;
	}
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_BUTTON_INI_FILE:
#if defined(KBTIM_SETUP)
			// 編集するiniを切り替える何か 
#endif
			break;
		case IDC_OPENVSTMGR:
#ifdef VST_LOADER_ENABLE
			if (hVSTHost != NULL)
				((open_vst_mgr)GetProcAddress(hVSTHost, "openVSTManager"))(hwnd);
#endif
			break;
		case IDC_BUTTON_SAVE_RESTART:
#if !defined(KBTIM_SETUP) && !defined(WINDRV_SETUP)
			RestartTimidity = 1;
#endif
			// thru
		case IDOK:
			for ( i = 0; i < PREF_PAGE_MAX; i++ ) {
				SendMessage ( pref_pages[i].hwnd, WM_MYSAVE, (WPARAM)0, (LPARAM)0 );
			}
#if defined(KBTIM_SETUP) || defined(WINDRV_SETUP)
			PrefSettingApply();
			w32g_uninitialize();
			SetWindowLongPtr(hwnd,	DWLP_MSGRESULT, TRUE);
			EndDialog ( hwnd, TRUE );
			config_gui_main_close();
			PostQuitMessage(0);
#else
			PrefSettingApply();
			SetWindowLongPtr(hwnd,	DWLP_MSGRESULT, TRUE);
			EndDialog ( hwnd, TRUE );
#endif
			return TRUE;
		case IDCANCEL:
#if defined(KBTIM_SETUP) || defined(WINDRV_SETUP)
			w32g_uninitialize();
			SetWindowLongPtr(hwnd,	DWLP_MSGRESULT, TRUE);
			EndDialog ( hwnd, TRUE );
			config_gui_main_close();
			PostQuitMessage(0);
#else
			SetWindowLongPtr(hwnd,	DWLP_MSGRESULT, FALSE);
			EndDialog ( hwnd, FALSE );
#endif
			return TRUE;
		case IDC_BUTTON_APPLY:
			for ( i = 0; i < PREF_PAGE_MAX; i++ ) {
				SendMessage ( pref_pages[i].hwnd, WM_MYSAVE, (WPARAM)0, (LPARAM)0 );
			}
			PrefSettingApply();
#if !defined(IA_W32G_SYN)
			TracerWndApplyQuietChannel(st_temp->quietchannels);
#endif
			SetWindowLongPtr(hwnd,	DWLP_MSGRESULT, TRUE);
			return TRUE;
		}
		break;

	case WM_NOTIFY:
      {
	LPNMHDR pnmh = (LPNMHDR) lParam;
	if (pnmh->idFrom == IDC_TAB_MAIN) {
	    switch (pnmh->code) {
	    case TCN_SELCHANGE:
	    {
		int nIndex = SendDlgItemMessage(hwnd, IDC_TAB_MAIN,
						TCM_GETCURSEL, (WPARAM)0, (LPARAM)0);
		for (i = 0; i < PREF_PAGE_MAX; i++) {
		    if (pref_pages[i].hwnd)
			ShowWindow(pref_pages[i].hwnd, SW_HIDE);
		}
		PrefWndCreatePage(hwnd, nIndex);
		ShowWindow(pref_pages[nIndex].hwnd, SW_SHOWNORMAL);
		PrefInitialPage = nIndex;
		return TRUE;
	    }

	    default:
		break;
	    }
	}
      }
      break;

	case WM_SIZE:
	{
		RECT rc;
		HWND hwnd_tab = GetDlgItem ( hwnd, IDC_TAB_MAIN );
		GetClientRect ( hwnd_tab, &rc );
		SendDlgItemMessage ( hwnd, IDC_TAB_MAIN, TCM_ADJUSTRECT, (WPARAM)TRUE, (LPARAM)&rc );
		for ( i = 0; i < PREF_PAGE_MAX; i++ ) {
			MoveWindow ( pref_pages[i].hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE );
		}
		return TRUE;
	}

	case WM_DESTROY:
		break;

	default:
	  break;
	}

	return FALSE;
}

//			SetWindowLong(hwnd,	DWL_MSGRESULT, FALSE);

#define DLG_CHECKBUTTON_TO_FLAG(hwnd,ctlid,x)	\
	((SendDlgItemMessage((hwnd),(ctlid),BM_GETCHECK,0,0))?((x)=1):((x)=0))
#define DLG_FLAG_TO_CHECKBUTTON(hwnd,ctlid,x)	\
	((x)?(SendDlgItemMessage((hwnd),(ctlid),BM_SETCHECK,1,0)):\
	(SendDlgItemMessage((hwnd),(ctlid),BM_SETCHECK,0,0)))


extern void TracerWndApplyQuietChannel( ChannelBitMask quietchannels_ );

/* st_temp, sp_temp を適用する
 * 注意: MainThread からの呼び出し禁止、危険！
 */
extern void OnQuit(void);
extern void timidity_init_player(void); /* timidity.c */

void PrefSettingApplyReally(void)
{
	int restart = 0;
	extern int IniFileAutoSave;
	
    if(opt_load_all_instrument)
		free_instruments(1);
	//if(play_mode->fd != -1)
	//	play_mode->close_output();
	if (play_mode->close_output)
		play_mode->close_output();
    if (CurrentPlayerLanguage == -1)
        CurrentPlayerLanguage = PlayerLanguage;

    restart = (CurrentPlayerLanguage != sp_temp->PlayerLanguage);
//	restart |= (strcmp(sp_temp->ConfigFile,ConfigFile) != 0);
	if(sp_temp->PlayerLanguage == LANGUAGE_JAPANESE)
		strcpy(st_temp->output_text_code, "SJIS");
	else
		strcpy(st_temp->output_text_code, "ASCII");
	uninitialize_resampler_coeffs();
#ifdef SUPPORT_SOUNDSPEC
//	if(view_soundspec_flag)
		close_soundspec();
#endif
	free_cache_data();	
	ApplySettingPlayer(sp_temp);
	ApplySettingTiMidity(st_temp);
	SaveSettingPlayer(sp_current);
	SaveSettingTiMidity(st_current);
	
#if !defined(KBTIM_SETUP) && !defined(WINDRV_SETUP)
	if(RestartTimidity){
		SaveIniFile(sp_current, st_current);
		OnQuit();
		return;
	}
#endif

	memcpy(sp_temp, sp_current, sizeof(SETTING_PLAYER));
	memcpy(st_temp, st_current, sizeof(SETTING_TIMIDITY));	

///r
	init_output(); // playmode rate
	init_playmidi();
	init_mix_c();
#ifdef INT_SYNTH
	init_int_synth();
#endif // INT_SYNTH
#ifdef ENABLE_DLS
	init_dls();
#endif
#ifdef ENABLE_SFZ
	init_sfz();
#endif
	initialize_resampler_coeffs();
    timidity_init_player();
	restore_voices(1);
#ifdef SUPPORT_SOUNDSPEC
	open_soundspec();
#endif
	
///r
	load_all_instrument();
#ifdef MULTI_THREAD_COMPUTE
	reset_compute_thread();
#endif

	PrefWndSetOK = 0;
	if(IniFileAutoSave)
		SaveIniFile(sp_current, st_current);
	if(restart &&
	   MessageBox(hListWnd,"Restart TiMidity?", "TiMidity",
				  MB_YESNO)==IDYES)
	{
		w32g_restart();
//		PrefWndDoing = 0;
	}
}

#if defined(IA_W32G_SYN)
extern int w32g_syn_do_before_pref_apply ( void );
extern int w32g_syn_do_after_pref_apply ( void );
extern int w32g_syn_do_after_pref_save_restart(void);
#endif

extern int IniFileAutoSave;
static void PrefSettingApply(void)
{
#if defined(KBTIM_SETUP) || defined(WINDRV_SETUP)
	ApplySettingPlayer(sp_temp);
	ApplySettingTiMidity(st_temp);
	SaveSettingPlayer(sp_current);
	SaveSettingTiMidity(st_current);
	memcpy(sp_temp, sp_current, sizeof(SETTING_PLAYER));
	memcpy(st_temp, st_current, sizeof(SETTING_TIMIDITY));
	SaveIniFile(sp_current, st_current);
#elif !defined(IA_W32G_SYN)
#ifdef EXT_CONTROL_MAIN_THREAD
	w32g_ext_control_main_thread(RC_EXT_APPLY_SETTING, 0);
#else
	w32g_send_rc(RC_EXT_APPLY_SETTING, 0);
#endif
#else
	int before_pref_apply_ok;
	before_pref_apply_ok = ( w32g_syn_do_before_pref_apply () == 0 );

	uninitialize_resampler_coeffs();
#ifdef SUPPORT_SOUNDSPEC
//	if(view_soundspec_flag)
		close_soundspec();
#endif
	ApplySettingPlayer(sp_temp);
	ApplySettingTiMidity(st_temp);
	SaveSettingPlayer(sp_current);
	SaveSettingTiMidity(st_current);
	
#if !defined(KBTIM_SETUP) && !defined(WINDRV_SETUP)
	if(RestartTimidity){
		SaveIniFile(sp_current, st_current);
		w32g_syn_do_after_pref_save_restart();
		return;
	}
#endif

	memcpy(sp_temp, sp_current, sizeof(SETTING_PLAYER));
	memcpy(st_temp, st_current, sizeof(SETTING_TIMIDITY));
	
	init_output(); // playmode rate
	init_playmidi();
	init_mix_c();
#ifdef INT_SYNTH
	init_int_synth();
#endif // INT_SYNTH
#ifdef ENABLE_DLS
	init_dls();
#endif
#ifdef ENABLE_SFZ
	init_sfz();
#endif
	initialize_resampler_coeffs();

#ifdef SUPPORT_SOUNDSPEC
	open_soundspec();
#endif
#ifdef MULTI_THREAD_COMPUTE
	reset_compute_thread();
#endif

	if(IniFileAutoSave)
		SaveIniFile(sp_current, st_current);
	if ( before_pref_apply_ok )
		w32g_syn_do_after_pref_apply ();
	PrefWndSetOK = 0;
//	PrefWndDoing = 0;
#endif
}


void reload_cfg(void)
{
#if defined(IA_W32GUI) && !defined(KBTIM_SETUP) && !defined(WINDRV_SETUP)
    PauseOldTiMidity();
    //while (w32g_play_active) { Sleep(10); } /* buggy */
    if (w32g_play_active)
    {
        const TCHAR *cfg_msg,
                cfg_msg_en[] = TEXT("Cannot reload between playing!"),
                cfg_msg_jp[] = TEXT("再生中にリロードできません!");
        if (CurrentPlayerLanguage == LANGUAGE_JAPANESE)
                cfg_msg = cfg_msg_jp;
        else
                cfg_msg = cfg_msg_en;
        MessageBox(hPrefWnd, cfg_msg, TEXT("TiMidity Warning"), MB_OK | MB_ICONWARNING);
        return;
    }
#endif
    free_instrument_map();
    clean_up_pathlist();
    free_instruments(0);
    free_special_patch(-1);
    tmdy_free_config();
    free_soundfonts();
#ifdef ENABLE_SFZ
	free_sfz();
#endif
#ifdef ENABLE_DLS
	free_dls();
#endif
#ifdef INT_SYNTH
	free_int_synth();
#endif // INT_SYNTH
    free_cache_data();
    timidity_start_initialize();
    if (!sp_temp->ConfigFile[0]) {
	strcpy(sp_temp->ConfigFile, ConfigFile);
    }
    if (read_config_file(sp_temp->ConfigFile, 0, 0)) {
	const char cfgname[] = "\\TIMIDITY.CFG";
	if (!sp_temp->ConfigFile[0]) {
	    GetWindowsDirectoryA(sp_temp->ConfigFile, FILEPATH_MAX - strlen(cfgname) - 1);
	    strlcat(sp_temp->ConfigFile, cfgname, FILEPATH_MAX);
	}
	read_config_file(sp_temp->ConfigFile, 0, 0);
    }
	safe_free(CurrentConfigFile);
	CurrentConfigFile = safe_strdup(sp_temp->ConfigFile);
	PrefSettingApply();

#if defined(WINDRV) || defined(WINDRV_SETUP)
	timdrvOverrideSFSettingLoad();
#else
    OverrideSFSettingLoad();
#endif
	init_load_soundfont();
#ifndef IA_W32G_SYN
	TracerWndApplyQuietChannel(st_temp->quietchannels);
#endif
}


// 設定値が不連続な場合などの処理の簡略化 nameリストに対応するnumリスト作成して使用 
// cb_info : int num_list[] , val : find value , miss : then not find value
// リスト数
#define CB_NUM(cb_info)				(sizeof(cb_info) / sizeof(int))
// numリストから一致する値を検索しコンボカウントを返す
#define CB_FIND(cb_info, val, miss)	(cb_find_item(CB_NUM(cb_info), cb_info, val, miss))

// 以下 コンボ関連メッセージ短縮形
// must WHND hwnd , cb_id : IDC_COMBO_hoge , error : then CB_ERROR value
// CB_SETCURSEL
#define CB_SET(cb_id, set_num)		(SendDlgItemMessage(hwnd, cb_id, CB_SETCURSEL, (WPARAM) set_num, (LPARAM) 0))
// CB_GETCURSEL
#define CB_GET(cb_id)				((int)SendDlgItemMessage(hwnd, cb_id, CB_GETCURSEL, (WPARAM) 0, (LPARAM) 0))
// CB_GETCURSEL check CB_ERROR(-1) , error : then CB_ERROR value
#define CB_GETS(cb_id, error)		(CB_GET(cb_id) == -1 ? error : CB_GET(cb_id))
// CB_RESETCONTENT
#define CB_RESET(cb_id)				(SendDlgItemMessage(hwnd, cb_id, CB_RESETCONTENT, (WPARAM) 0, (LPARAM) 0))
// CB_INSERTSTRING(ANSI)
#define CB_INSSTRA(cb_id, str)		(SendDlgItemMessageA(hwnd, cb_id, CB_INSERTSTRING, (WPARAM) -1, (LPARAM) str))
// CB_INSERTSTRING(UNICODE)
#define CB_INSSTRW(cb_id, str)		(SendDlgItemMessageW(hwnd, cb_id, CB_INSERTSTRING, (WPARAM) -1, (LPARAM) str))
#ifdef UNICODE
#define CB_INSSTR(cb_id, cstr)			CB_INSSTRW(cb_id, cstr)
#else
#define CB_INSSTR(cb_id, cstr)			CB_INSSTRA(cb_id, cstr)
#endif

// 以下 エディット関連関数短縮形
// must WHND hwnd , cb_id : IDC_EDIT_hoge value
#define EB_GETTEXTA(cb_id, cstr, len)	GetDlgItemTextA(hwnd, cb_id, cstr, len)
#define EB_GETTEXTW(cb_id, cstr, len)	GetDlgItemTextW(hwnd, cb_id, cstr, len)
#define EB_SETTEXTA(cb_id, cstr)		SetDlgItemTextA(hwnd, cb_id, cstr)
#define EB_SETTEXTW(cb_id, cstr)		SetDlgItemTextW(hwnd, cb_id, cstr)
#ifdef UNICODE
#define EB_GETTEXT(cb_id, cstr, len)	EB_GETTEXTW(cb_id, cstr, len)
#define EB_SETTEXT(cb_id, cstr)			EB_SETTEXTW(cb_id, cstr)
#else
#define EB_GETTEXT(cb_id, cstr, len)	EB_GETTEXTA(cb_id, cstr, len)
#define EB_SETTEXT(cb_id, cstr)			EB_SETTEXTA(cb_id, cstr)
#endif
#define EB_GET_INT(cb_id)			((INT)GetDlgItemInt(hwnd, cb_id, NULL, TRUE))
#define EB_SET_INT(cb_id, set_num)	((INT)SetDlgItemInt(hwnd, cb_id, set_num, TRUE))
#define EB_GET_UINT(cb_id)			((UINT)GetDlgItemInt(hwnd, cb_id, NULL, FALSE))
#define EB_SET_UINT(cb_id, set_num)	((UINT)SetDlgItemInt(hwnd, cb_id, set_num, FALSE))

// 以下 チェックボックス関連メッセージ短縮形
#define CH_GET(cb_id)			(SendDlgItemMessageA(hwnd, cb_id, BM_GETCHECK, 0, 0))
#define CH_SET(cb_id, flag)		(DLG_FLAG_TO_CHECKBUTTON(hwnd, cb_id, flag))

// 以下 ダイアログ関連関数短縮形
#define DI_GET(cb_id)			(GetDlgItem(hwnd, cb_id))
#define DI_ENABLE(cb_id)		(EnableWindow(DI_GET(cb_id), TRUE))
#define DI_DISABLE(cb_id)		(EnableWindow(DI_GET(cb_id), FALSE))
#define DI_ISENABLED(cb_id)		(IsWindowEnabled(DI_GET(cb_id)))

static int process_priority_num[] = {
	IDLE_PRIORITY_CLASS,			
	BELOW_NORMAL_PRIORITY_CLASS,
	NORMAL_PRIORITY_CLASS,
	ABOVE_NORMAL_PRIORITY_CLASS,
	HIGH_PRIORITY_CLASS,
	REALTIME_PRIORITY_CLASS,
};
static const TCHAR *process_priority_name_jp[] = {
	TEXT("低い"),
	TEXT("少し低い"),
	TEXT("普通"),
	TEXT("少し高い"),
	TEXT("高い"),
	TEXT("リアルタイム"),
};
static const TCHAR *process_priority_name_en[] = {
	TEXT("Lowest"),
	TEXT("Below Normal"),
	TEXT("Normal"),
	TEXT("Above Normal"),
	TEXT("Highest"),
	TEXT("Realtime"),
};

static int thread_priority_num[] = {
	THREAD_PRIORITY_IDLE,
	THREAD_PRIORITY_LOWEST,
	THREAD_PRIORITY_BELOW_NORMAL,
	THREAD_PRIORITY_NORMAL,
	THREAD_PRIORITY_ABOVE_NORMAL,
	THREAD_PRIORITY_HIGHEST,
	THREAD_PRIORITY_TIME_CRITICAL,
};
static const TCHAR *thread_priority_name_jp[] = {
	TEXT("アイドル"),
	TEXT("低い"),
	TEXT("少し低い"),
	TEXT("普通"),
	TEXT("少し高い"),
	TEXT("高い"),
	TEXT("タイムクリティカル"),
};
static const TCHAR *thread_priority_name_en[] = {
	TEXT("Idle"),
	TEXT("Lowest"),
	TEXT("Below Normal"),
	TEXT("Normal"),
	TEXT("Above Normal"),
	TEXT("Highest"),
	TEXT("Time Critical"),
};

// IDC_COMBO_COMPUTE_THREAD_NUM
#define cb_num_IDC_COMBO_COMPUTE_THREAD_NUM 15
static int cb_info_IDC_COMBO_COMPUTE_THREAD_NUM_num[] = {
	0,
	2,
	3,
	4,
	5,
	6,
	7,
	8,
	9,
	10,
	11,
	12,
	13,
	14,
	15,
	16,
}; 
static const TCHAR *cb_info_IDC_COMBO_COMPUTE_THREAD_NUM[] = {
	TEXT("OFF"),
	TEXT(" 2"),
	TEXT(" 3"),
	TEXT(" 4"),
	TEXT(" 5"),
	TEXT(" 6"),
	TEXT(" 7"),
	TEXT(" 8"),
	TEXT(" 9"),
	TEXT("10"),
	TEXT("11"),
	TEXT("12"),
	TEXT("13"),
	TEXT("14"),
	TEXT("15"),
	TEXT("16"),
};

// IDC_COMBO_CTL_VEBOSITY
static int cb_info_IDC_COMBO_CTL_VEBOSITY_num[] = {
	-1,
	0,
	1,
	2,
	3,
	4,
}; 
static const TCHAR *cb_info_IDC_COMBO_CTL_VEBOSITY[] = {
	TEXT("-1 ???"),
	TEXT("0 NORMAL"),
	TEXT("1 VERBOSE"),
	TEXT("2 NOISY"),
	TEXT("3 DEBUG"),
	TEXT("4 DEBUG_SILLY"),
}; 

#if !(defined(IA_W32G_SYN) || defined(WINDRV_SETUP))
extern int PlayerThreadPriority;

// IDC_COMBO_SUBWINDOW_MAX
#define cb_num_IDC_COMBO_SUBWINDOW_MAX 10
static const TCHAR *cb_info_IDC_COMBO_SUBWINDOW_MAX[] = {
	TEXT(" 0"),
	TEXT(" 1"),
	TEXT(" 2"),
	TEXT(" 3"),
	TEXT(" 4"),
	TEXT(" 5"),
	TEXT(" 6"),
	TEXT(" 7"),
	TEXT(" 8"),
	TEXT(" 9"),
	TEXT("10"),
};

// IDC_COMBO_TRACE_MODE_UPDATE
#define cb_num_IDC_COMBO_TRACE_MODE_UPDATE 17
static int cb_info_IDC_COMBO_TRACE_MODE_UPDATE_num[] = {
	0,
	10,
	15,
	20,
	25,
	30,
	40,
	60,
	80,
	100,
	125,
	150,
	175,
	200,
	250,
	300,
	400,
	500,
}; 
static const TCHAR *cb_info_IDC_COMBO_TRACE_MODE_UPDATE[] = {
	TEXT("OFF"),
	TEXT("10"),
	TEXT("15"),
	TEXT("20"),
	TEXT("25"),
	TEXT("30"),
	TEXT("40"),
	TEXT("60"),
	TEXT("80"),
	TEXT("100"),
	TEXT("125"),
	TEXT("150"),
	TEXT("175"),
	TEXT("200"),
	TEXT("250"),
	TEXT("300"),
	TEXT("400"),
	TEXT("500"),
}; 

// IDC_COMBO_PLAYLIST_MAX
#define cb_num_IDC_COMBO_PLAYLIST_MAX (PLAYLIST_MAX - 1)
static int cb_info_IDC_COMBO_PLAYLIST_MAX_num[] = {
	1,
	2,
	3,
	4,
	5,
	6,
	7,
	8,
	9,
	10,
	11,
	12,
	13,
	14,
	15,
	16,
	17,
	18,
	19,
	20,
}; 
static const TCHAR *cb_info_IDC_COMBO_PLAYLIST_MAX[] = {
	TEXT(" 1"),
	TEXT(" 2"),
	TEXT(" 3"),
	TEXT(" 4"),
	TEXT(" 5"),
	TEXT(" 6"),
	TEXT(" 7"),
	TEXT(" 8"),
	TEXT(" 9"),
	TEXT("10"),
	TEXT("11"),
	TEXT("12"),
	TEXT("13"),
	TEXT("14"),
	TEXT("15"),
	TEXT("16"),
	TEXT("17"),
	TEXT("18"),
	TEXT("19"),
	TEXT("20"),
}; 

// IDC_COMBO_SECOND_MODE
#define cb_num_IDC_COMBO_SECOND_MODE 5
static int cb_info_IDC_COMBO_SECOND_MODE_num[] = {
	0,
	1,
	2,
	3,
	4,
	5,
};
static const TCHAR *cb_info_IDC_COMBO_SECOND_MODE[] = {
	TEXT("0"),
	TEXT("1"),
	TEXT("2"),
	TEXT("3"),
	TEXT("4"),
	TEXT("5"),
};

extern DWORD processPriority;

static LRESULT APIENTRY
PrefPlayerDialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
    static int initflag = 1;
    int i, tmp;

	switch (uMess){
	case WM_INITDIALOG:		
#if defined(KBTIM_SETUP)
		DI_DISABLE(IDC_BUTTON_CFG_RELOAD);
		DI_DISABLE(IDC_CHECKBOX_AUTOQUIT);
		DI_DISABLE(IDC_CHECKBOX_AUTOUNIQ);
		DI_DISABLE(IDC_CHECKBOX_AUTOREFINE);
		DI_DISABLE(IDC_CHECKBOX_AUTOSTART);
		DI_DISABLE(IDC_CHECKBOX_NOT_CONTINUE);
		DI_DISABLE(IDC_CHECKBOX_NOT_DRAG_START);
		DI_DISABLE(IDC_CHECKBOX_NOT_LOOPING);
		DI_DISABLE(IDC_CHECKBOX_RANDOM);
		DI_DISABLE(IDC_CHECKBOX_CTL_TRACE_PLAYING);
		DI_DISABLE(IDC_COMBO_CTL_VEBOSITY);
		DI_DISABLE(IDC_CHECK_CONSOLE_CLEAR_FLG);
		DI_DISABLE(IDC_EDIT_MAIN_PANEL_UPDATE);
		DI_DISABLE(IDC_CHECKBOX_PRINT_FONTNAME);
		DI_DISABLE(IDC_EDIT_SPECTROGRAM_UPDATE);
		DI_DISABLE(IDC_CHECK_SEACHDIRRECURSIVE);
		DI_DISABLE(IDC_CHECK_DOCWNDINDEPENDENT);
		DI_DISABLE(IDC_CHECK_DOCWNDAUTOPOPUP);
		DI_DISABLE(IDC_CHECK_INIFILE_AUTOSAVE);
		DI_DISABLE(IDC_CHECK_AUTOLOAD_PLAYLIST);
		DI_DISABLE(IDC_CHECK_AUTOSAVE_PLAYLIST);
		DI_DISABLE(IDC_CHECK_POS_SIZE_SAVE);
		DI_DISABLE(IDC_COMBO_SUBWINDOW_MAX);
		DI_DISABLE(IDC_COMBO_PLAYLIST_MAX);
		DI_DISABLE(IDC_COMBO_SECOND_MODE);
        DI_DISABLE(IDC_CHECKBOX_LOOP_CC111);
        DI_DISABLE(IDC_CHECKBOX_LOOP_AB_MARK);
        DI_DISABLE(IDC_CHECKBOX_LOOP_SE_MARK);
        DI_DISABLE(IDC_CHECKBOX_LOOP_CC2);
        DI_DISABLE(IDC_EDIT_LOOP_REPEAT);
#endif

        if (!sp_temp->ConfigFile[0]) {
            strcpy(sp_temp->ConfigFile, ConfigFile);
        }
        EB_SETTEXTA(IDC_COMBO_CONFIG_FILE, sp_temp->ConfigFile);
        tmp = SendDlgItemMessage(hwnd, IDC_COMBO_CONFIG_FILE, WM_GETTEXTLENGTH, 0, 0); // A/W
        SendDlgItemMessage(hwnd, IDC_COMBO_CONFIG_FILE, CB_SETEDITSEL, 0, MAKELPARAM(tmp, tmp)); // A/W
        safe_free(CurrentConfigFile);
        CurrentConfigFile = safe_strdup(sp_temp->ConfigFile);

        switch (CurrentPlayerLanguage) {
        case LANGUAGE_ENGLISH:
            CheckRadioButton(hwnd, IDC_RADIOBUTTON_JAPANESE, IDC_RADIOBUTTON_ENGLISH,
                             IDC_RADIOBUTTON_ENGLISH);
            break;
        case LANGUAGE_JAPANESE:
        default:
            CheckRadioButton(hwnd, IDC_RADIOBUTTON_JAPANESE, IDC_RADIOBUTTON_ENGLISH,
                             IDC_RADIOBUTTON_JAPANESE);
            break;
        }
		
        // ctl
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_AUTOQUIT, strchr(st_temp->opt_ctl + 1, 'x'));
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_AUTOUNIQ, strchr(st_temp->opt_ctl + 1, 'u'));
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_AUTOREFINE, strchr(st_temp->opt_ctl + 1, 'R'));
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_AUTOSTART, strchr(st_temp->opt_ctl + 1, 'a'));
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_NOT_CONTINUE, !strchr(st_temp->opt_ctl + 1, 'C'));
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_NOT_DRAG_START, strchr(st_temp->opt_ctl + 1, 'd'));
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_NOT_LOOPING, strchr(st_temp->opt_ctl + 1, 'l'));
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_RANDOM, strchr(st_temp->opt_ctl + 1, 'r'));
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CTL_TRACE_PLAYING, strchr(st_temp->opt_ctl + 1, 't'));
		// console
		tmp = char_count(st_temp->opt_ctl + 1, 'v') - char_count(st_temp->opt_ctl + 1, 'q') + 1;
		for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_CTL_VEBOSITY_num); i++)
			CB_INSSTR(IDC_COMBO_CTL_VEBOSITY, cb_info_IDC_COMBO_CTL_VEBOSITY[i]);
		CB_SET(IDC_COMBO_CTL_VEBOSITY, CB_FIND(cb_info_IDC_COMBO_CTL_VEBOSITY_num, tmp, 1));
		DLG_FLAG_TO_CHECKBUTTON(hwnd, IDC_CHECK_CONSOLE_CLEAR_FLG, sp_temp->ConsoleClearFlag & 0x1);
		// main panel
		SetDlgItemInt(hwnd, IDC_EDIT_MAIN_PANEL_UPDATE, sp_temp->main_panel_update_time, FALSE);
		// tracer
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_PRINT_FONTNAME, st_temp->opt_print_fontname);

		// spectrogram
#ifdef SUPPORT_SOUNDSPEC
		SetDlgItemFloat(hwnd, IDC_EDIT_SPECTROGRAM_UPDATE, st_temp->spectrogram_update_sec);
#else
		DI_DISABLE(IDC_EDIT_SPECTROGRAM_UPDATE);
#endif
		
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECK_SEACHDIRRECURSIVE, sp_temp->SeachDirRecursive);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECK_DOCWNDINDEPENDENT, sp_temp->DocWndIndependent);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECK_DOCWNDAUTOPOPUP, sp_temp->DocWndAutoPopup);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECK_INIFILE_AUTOSAVE, sp_temp->IniFileAutoSave);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECK_AUTOLOAD_PLAYLIST, sp_temp->AutoloadPlaylist);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECK_AUTOSAVE_PLAYLIST, sp_temp->AutosavePlaylist);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECK_POS_SIZE_SAVE, sp_temp->PosSizeSave);
		
		for (i = 0; i <= cb_num_IDC_COMBO_SUBWINDOW_MAX; i++)
			CB_INSSTR(IDC_COMBO_SUBWINDOW_MAX, cb_info_IDC_COMBO_SUBWINDOW_MAX[i]);
		CB_SET(IDC_COMBO_SUBWINDOW_MAX, RANGE(sp_temp->SubWindowMax, 0, cb_num_IDC_COMBO_SUBWINDOW_MAX));

		if(CurrentPlayerLanguage == LANGUAGE_JAPANESE) {
			for (i = 0; i < CB_NUM(process_priority_num); i++)
				CB_INSSTR(IDC_COMBO_PROCESS_PRIORITY, process_priority_name_jp[i]);
			for (i = 0; i < CB_NUM(thread_priority_num); i++)		
				CB_INSSTR(IDC_COMBO_PLAYER_THREAD_PRIORITY, thread_priority_name_jp[i] );
		}else{
			for (i = 0; i < CB_NUM(process_priority_num); i++)
				CB_INSSTR(IDC_COMBO_PROCESS_PRIORITY, process_priority_name_en[i] );
			for (i = 0; i < CB_NUM(thread_priority_num); i++)		
				CB_INSSTR(IDC_COMBO_PLAYER_THREAD_PRIORITY, thread_priority_name_en[i] );
		}
		// Select process priority
		CB_SET(IDC_COMBO_PROCESS_PRIORITY, CB_FIND(process_priority_num, st_temp->processPriority, 2));
		// Select thread priority
		CB_SET(IDC_COMBO_PLAYER_THREAD_PRIORITY, CB_FIND(thread_priority_num, sp_temp->PlayerThreadPriority, 3));
		// compute_thread_num
		for (i = 0; i <= cb_num_IDC_COMBO_COMPUTE_THREAD_NUM; i++)
			CB_INSSTR(IDC_COMBO_COMPUTE_THREAD_NUM, cb_info_IDC_COMBO_COMPUTE_THREAD_NUM[i]);
		CB_SET(IDC_COMBO_COMPUTE_THREAD_NUM, CB_FIND(cb_info_IDC_COMBO_COMPUTE_THREAD_NUM_num, st_temp->compute_thread_num, 0));
		// playlist max
		for (i = 0; i <= cb_num_IDC_COMBO_PLAYLIST_MAX; i++)
			CB_INSSTR(IDC_COMBO_PLAYLIST_MAX, cb_info_IDC_COMBO_PLAYLIST_MAX[i]);
		CB_SET(IDC_COMBO_PLAYLIST_MAX, CB_FIND(cb_info_IDC_COMBO_PLAYLIST_MAX_num, sp_temp->PlaylistMax, 0));
		// second mode
		for (i = 0; i <= cb_num_IDC_COMBO_SECOND_MODE; i++)
			CB_INSSTR(IDC_COMBO_SECOND_MODE, cb_info_IDC_COMBO_SECOND_MODE[i]);
		CB_SET(IDC_COMBO_SECOND_MODE, CB_FIND(cb_info_IDC_COMBO_SECOND_MODE_num, sp_temp->SecondMode, 0));
		
        // CC/Mark loop repeat
        CH_SET(IDC_CHECKBOX_LOOP_CC111, (st_temp->opt_use_midi_loop_repeat & LF_CC111_TO_EOT) != 0);
        CH_SET(IDC_CHECKBOX_LOOP_AB_MARK, (st_temp->opt_use_midi_loop_repeat & LF_MARK_A_TO_B) != 0);
        CH_SET(IDC_CHECKBOX_LOOP_SE_MARK, (st_temp->opt_use_midi_loop_repeat & LF_MARK_S_TO_E) != 0);
        CH_SET(IDC_CHECKBOX_LOOP_CC2, (st_temp->opt_use_midi_loop_repeat & LF_CC2_TO_CC4) != 0);
        SendMessage(hwnd, WM_COMMAND, IDC_CHECKBOX_LOOP_CC111, 0);
        EB_SET_INT(IDC_EDIT_LOOP_REPEAT, st_temp->opt_midi_loop_repeat);

#ifndef SUPPORT_LOOPEVENT
        DI_DISABLE(IDC_CHECKBOX_LOOP_CC111);
        DI_DISABLE(IDC_CHECKBOX_LOOP_AB_MARK);
        DI_DISABLE(IDC_CHECKBOX_LOOP_SE_MARK);
        DI_DISABLE(IDC_CHECKBOX_LOOP_CC2);
        DI_DISABLE(IDC_EDIT_LOOP_REPEAT);
#endif

		initflag = 0;
		break;
	case WM_COMMAND:
	switch (LOWORD(wParam)) {
        case IDC_BUTTON_CONFIG_FILE: {
            char filename[FILEPATH_MAX];
            filename[0] = '\0';
            EB_GETTEXTA(IDC_COMBO_CONFIG_FILE, filename, FILEPATH_MAX - 1);
            if (!DlgOpenConfigFile(filename, hwnd))
                if (filename[0] != '\0') {
                    EB_SETTEXTA(IDC_COMBO_CONFIG_FILE, filename);
                    tmp = SendDlgItemMessage(hwnd, IDC_COMBO_CONFIG_FILE, WM_GETTEXTLENGTH, 0, 0); // A/W
                    SendDlgItemMessage(hwnd, IDC_COMBO_CONFIG_FILE, CB_SETEDITSEL, 0, MAKELPARAM(tmp, tmp)); // A/W
                }
            break; }

        case IDC_BUTTON_CFG_EDIT: {
            char filename[FILEPATH_MAX];
            const char *editor_path = getenv("TIMIDITY_CFG_EDITOR");
            const TCHAR *msg,
                   msg_en[] = TEXT("Please set TIMIDITY_CFG_EDITOR environment variable"),
                   msg_jp[] = TEXT("環境変数 TIMIDITY_CFG_EDITOR を設定してください");
            switch (CurrentPlayerLanguage) {
            case LANGUAGE_ENGLISH: msg = msg_en; break;
            case LANGUAGE_JAPANESE: default: msg = msg_jp; break;
            }

            filename[0] = '\0';
            EB_GETTEXTA(IDC_COMBO_CONFIG_FILE, filename, FILEPATH_MAX - 1);

            w32_reset_exe_directory();

            if (check_file_extension(filename, ".txt", 0) == 1 ||
                check_file_extension(filename, ".cfg", 0) == 1 ||
                check_file_extension(filename, ".config", 0) == 1)
            {
                if (editor_path != NULL) {
                    ShellExecuteA(NULL, "open", editor_path, filename, NULL, SW_SHOWNORMAL); // A
                } else {
                    ShellExecuteA(NULL, "open", "notepad.exe", filename, NULL, SW_SHOWNORMAL); // A
                    if (get_verbosity_level() >= VERB_NORMAL)
                        MessageBox(hwnd, msg, NULL, MB_OK | MB_ICONWARNING);
                }
            }
            break; }

		case IDC_BUTTON_CFG_RELOAD:
		{
			int i;
			for (i = 0; i < PREF_PAGE_MAX; i++ ) {
				SendMessage ( pref_pages[i].hwnd, WM_MYSAVE, (WPARAM)0, (LPARAM)0 );
			}
			reload_cfg();
			SetWindowLongPtr(hwnd,	DWLP_MSGRESULT, TRUE);
		}
			break;
		case IDC_RADIOBUTTON_JAPANESE:
		case IDC_RADIOBUTTON_ENGLISH:
			break;
#if 0 // 
		case IDC_CHECKBOX_CTL_TRACE_PLAYING:
			if(SendDlgItemMessage(hwnd, IDC_CHECKBOX_CTL_TRACE_PLAYING, BM_GETCHECK, 0, 0))
				EnableWindow(GetDlgItem(hwnd, IDC_COMBO_TRACE_MODE_UPDATE), TRUE);
			else
				EnableWindow(GetDlgItem(hwnd, IDC_COMBO_TRACE_MODE_UPDATE), FALSE);
			break;
#endif
			
#ifdef SUPPORT_LOOPEVENT
        case IDC_CHECKBOX_LOOP_CC111:
        case IDC_CHECKBOX_LOOP_AB_MARK:
        case IDC_CHECKBOX_LOOP_SE_MARK:
        case IDC_CHECKBOX_LOOP_CC2:
            if (CH_GET(IDC_CHECKBOX_LOOP_CC111) || CH_GET(IDC_CHECKBOX_LOOP_AB_MARK) ||
                    CH_GET(IDC_CHECKBOX_LOOP_SE_MARK) || CH_GET(IDC_CHECKBOX_LOOP_CC2))
                DI_ENABLE(IDC_EDIT_LOOP_REPEAT);
            else
                DI_DISABLE(IDC_EDIT_LOOP_REPEAT);
            break;
#endif

		default:
			break;
	  }
		PrefWndSetOK = 1;
		break;
		
    case WM_MYSAVE: {
        char *p;
        int flag;

        if (initflag)
            break;

        EB_GETTEXTA(IDC_COMBO_CONFIG_FILE, sp_temp->ConfigFile, FILEPATH_MAX - 1);

        if (CH_GET(IDC_RADIOBUTTON_ENGLISH))
            sp_temp->PlayerLanguage = LANGUAGE_ENGLISH;
        else if (CH_GET(IDC_RADIOBUTTON_JAPANESE))
            sp_temp->PlayerLanguage = LANGUAGE_JAPANESE;
		
        tmp = 0;
		SettingCtlFlag(st_temp, 'x', DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_AUTOQUIT,tmp));
		SettingCtlFlag(st_temp, 'u', DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_AUTOUNIQ,tmp));
		SettingCtlFlag(st_temp, 'R', DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_AUTOREFINE,tmp));
		SettingCtlFlag(st_temp, 'a', DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_AUTOSTART,tmp));
		SettingCtlFlag(st_temp, 'C', !DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_NOT_CONTINUE,tmp));
		SettingCtlFlag(st_temp, 'd', DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_NOT_DRAG_START,tmp));
		SettingCtlFlag(st_temp, 'l', DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_NOT_LOOPING,tmp));
		SettingCtlFlag(st_temp, 'r', DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_RANDOM,tmp));
		SettingCtlFlag(st_temp, 't', DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_CTL_TRACE_PLAYING,tmp));
				/* remove 'v' and 'q' from st_temp->opt_ctl */
		while(strchr(st_temp->opt_ctl + 1, 'v'))
			 SettingCtlFlag(st_temp, 'v', 0);
		while(strchr(st_temp->opt_ctl + 1, 'q'))
			 SettingCtlFlag(st_temp, 'q', 0);

		/* append 'v' or 'q' */
		p = st_temp->opt_ctl + strlen(st_temp->opt_ctl);
		tmp = cb_info_IDC_COMBO_CTL_VEBOSITY_num[CB_GETS(IDC_COMBO_CTL_VEBOSITY, 1)];
		tmp = RANGE(tmp, -1, 4); /* -1..4 */
		while(tmp > 1) { *p++ = 'v'; tmp--; }
		while(tmp < 1) { *p++ = 'q'; tmp++; }
		DLG_CHECKBUTTON_TO_FLAG(hwnd, IDC_CHECK_CONSOLE_CLEAR_FLG, tmp);
		sp_temp->ConsoleClearFlag = tmp ? 0x1 : 0x0;
		// main panel
		sp_temp->main_panel_update_time = GetDlgItemInt(hwnd, IDC_EDIT_MAIN_PANEL_UPDATE, NULL,FALSE);
		// tracer
		DLG_CHECKBUTTON_TO_FLAG(hwnd, IDC_CHECKBOX_PRINT_FONTNAME,st_temp->opt_print_fontname);

		// spectrogram
#ifdef SUPPORT_SOUNDSPEC
		st_temp->spectrogram_update_sec = GetDlgItemFloat(hwnd, IDC_EDIT_SPECTROGRAM_UPDATE);
#endif

		DLG_CHECKBUTTON_TO_FLAG(hwnd, IDC_CHECK_SEACHDIRRECURSIVE, sp_temp->SeachDirRecursive);
		DLG_CHECKBUTTON_TO_FLAG(hwnd, IDC_CHECK_DOCWNDINDEPENDENT, sp_temp->DocWndIndependent);
		DLG_CHECKBUTTON_TO_FLAG(hwnd, IDC_CHECK_DOCWNDAUTOPOPUP, sp_temp->DocWndAutoPopup);
		DLG_CHECKBUTTON_TO_FLAG(hwnd, IDC_CHECK_INIFILE_AUTOSAVE, sp_temp->IniFileAutoSave);
		DLG_CHECKBUTTON_TO_FLAG(hwnd, IDC_CHECK_AUTOLOAD_PLAYLIST, sp_temp->AutoloadPlaylist);
		DLG_CHECKBUTTON_TO_FLAG(hwnd, IDC_CHECK_AUTOSAVE_PLAYLIST, sp_temp->AutosavePlaylist);
		DLG_CHECKBUTTON_TO_FLAG(hwnd, IDC_CHECK_POS_SIZE_SAVE, sp_temp->PosSizeSave);
		
		sp_temp->SubWindowMax = CB_GETS(IDC_COMBO_SUBWINDOW_MAX, 5);

		// Set process priority
		st_temp->processPriority = process_priority_num[CB_GETS(IDC_COMBO_PROCESS_PRIORITY, 2)];
		// Set thread priority
		sp_temp->PlayerThreadPriority = thread_priority_num[CB_GETS(IDC_COMBO_PLAYER_THREAD_PRIORITY, 2)];
		// compute_thread_num
		st_temp->compute_thread_num = cb_info_IDC_COMBO_COMPUTE_THREAD_NUM_num[CB_GETS(IDC_COMBO_COMPUTE_THREAD_NUM, 0)];
		// playlist max
		sp_temp->PlaylistMax =  cb_info_IDC_COMBO_PLAYLIST_MAX_num[CB_GETS(IDC_COMBO_PLAYLIST_MAX, 0)];
		// second mode
		sp_temp->SecondMode =  cb_info_IDC_COMBO_SECOND_MODE_num[CB_GETS(IDC_COMBO_SECOND_MODE, 1)];

        // CC/Mark loop repeat
        st_temp->opt_use_midi_loop_repeat = 0;
#ifdef SUPPORT_LOOPEVENT
        flag = CH_GET(IDC_CHECKBOX_LOOP_CC111) ? 1 : 0;
        st_temp->opt_use_midi_loop_repeat |= LF_CC111_TO_EOT * flag;
        flag = CH_GET(IDC_CHECKBOX_LOOP_AB_MARK) ? 1 : 0;
        st_temp->opt_use_midi_loop_repeat |= LF_MARK_A_TO_B * flag;
        flag = CH_GET(IDC_CHECKBOX_LOOP_SE_MARK) ? 1 : 0;
        st_temp->opt_use_midi_loop_repeat |= LF_MARK_S_TO_E * flag;
        flag = CH_GET(IDC_CHECKBOX_LOOP_CC2) ? 1 : 0;
        st_temp->opt_use_midi_loop_repeat |= LF_CC2_TO_CC4 * flag;
        st_temp->opt_midi_loop_repeat = EB_GET_INT(IDC_EDIT_LOOP_REPEAT);
#endif

        break; }

    case WM_DESTROY:
        if (strcmp(sp_temp->ConfigFile, CurrentConfigFile) != 0) {
            const TCHAR *msg,
                   msg_en[] = TEXT("Press the Reload button to apply instruments"),
                   msg_jp[] = TEXT("音色情報は強制再読込ボタンを押すと反映されます");
            switch (CurrentPlayerLanguage) {
            case LANGUAGE_ENGLISH: msg = msg_en; break;
            case LANGUAGE_JAPANESE: default: msg = msg_jp; break;
            }
            if (get_verbosity_level() >= VERB_NORMAL)
                MessageBox(hMainWnd, msg, TEXT("TiMidity"), MB_OK | MB_ICONWARNING);
        }
        safe_free(CurrentConfigFile);
        CurrentConfigFile = 0;
        break;

    default:
        break;
	}
	return FALSE;
}
#else

#if defined(TWSYNG32) && !defined(TWSYNSRV) && defined(USE_TWSYN_BRIDGE)
#include "twsyn_bridge_common.h"
#include "twsyn_bridge_host.h"
#endif

extern int syn_ThreadPriority;
static TCHAR **MidiINDrivers = NULL;
static int midi_in_max = 0;	
// 0 MIDI Mapper -1
// 1 MIDI IN Driver 0
// 2 MIDI IN Driver 1
static TCHAR **GetMidiINDrivers( void )
{
	int i;
	
#if defined(TWSYNG32) && !defined(TWSYNSRV) && defined(USE_TWSYN_BRIDGE)
	if(st_temp->opt_use_twsyn_bridge){		
		midi_in_max = get_bridge_midi_devs();
		if ( MidiINDrivers != NULL ) {
			for ( i = 0; MidiINDrivers[i] != NULL; i ++ ) {
				safe_free ( MidiINDrivers[i] );
			}
			safe_free ( MidiINDrivers );
			MidiINDrivers = NULL;
		}
		MidiINDrivers = ( TCHAR ** ) malloc ( sizeof ( TCHAR * ) * ( midi_in_max + 2 ) );
		if ( MidiINDrivers == NULL ) return MidiINDrivers;
		for (i = 0; i <= midi_in_max; i ++ ) {
			MidiINDrivers[i] = strdup (get_bridge_midi_dev_name(i));
			if ( MidiINDrivers[i] == NULL )
				break;
		}
		MidiINDrivers[midi_in_max+1] = NULL;
	}else
#endif
	{
		midi_in_max = midiInGetNumDevs ();
		if ( MidiINDrivers != NULL ) {
			for ( i = 0; MidiINDrivers[i] != NULL; i ++ ) {
				safe_free ( MidiINDrivers[i] );
			}
			safe_free ( MidiINDrivers );
			MidiINDrivers = NULL;
		}
		MidiINDrivers = ( TCHAR ** ) malloc ( sizeof ( TCHAR * ) * ( midi_in_max + 2 ) );
		if ( MidiINDrivers == NULL ) return MidiINDrivers;
		MidiINDrivers[0] = safe_strdup ( "MIDI Mapper" );
		for ( i = 1; i <= midi_in_max; i ++ ) {
			MIDIINCAPS mic;
			if ( midiInGetDevCaps ( i - 1, &mic, sizeof ( MIDIINCAPS ) ) == 0 ) {
				MidiINDrivers[i] = strdup ( mic.szPname );
				if ( MidiINDrivers[i] == NULL )
					break;
			} else {
				MidiINDrivers[i] = NULL;
				break;
			}
		}
		MidiINDrivers[midi_in_max+1] = NULL;
	}
	return MidiINDrivers;
}

static BOOL APIENTRY
PrefSyn1DialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
    static int initflag = 1;
    int i, tmp;
    static const DWORD dwCtlPortIDs[] = {
          IDC_COMBO_IDPORT0, IDC_COMBO_IDPORT1,
          IDC_COMBO_IDPORT2, IDC_COMBO_IDPORT3,
          /*IDC_COMBO_IDPORT4, IDC_COMBO_IDPORT5,
          IDC_COMBO_IDPORT6, IDC_COMBO_IDPORT7*/ };
	switch (uMess){
    case WM_INITDIALOG:
        if (hFixedPointFont != NULL)
            SendDlgItemMessage(hwnd, IDC_COMBO_CONFIG_FILE, WM_SETFONT, (WPARAM) hFixedPointFont, MAKELPARAM(TRUE, 0));


        if (!sp_temp->ConfigFile[0]) {
            strcpy(sp_temp->ConfigFile, ConfigFile);
        }
        EB_SETTEXTA(IDC_COMBO_CONFIG_FILE, sp_temp->ConfigFile);
        tmp = SendDlgItemMessage(hwnd, IDC_COMBO_CONFIG_FILE, WM_GETTEXTLENGTH, 0, 0); // A/W
        SendDlgItemMessage(hwnd, IDC_COMBO_CONFIG_FILE, CB_SETEDITSEL, 0, MAKELPARAM(tmp, tmp)); // A/W
        safe_free(CurrentConfigFile);
        CurrentConfigFile = safe_strdup(sp_temp->ConfigFile);

#if defined(WINDRV_SETUP)
        DI_DISABLE(IDC_BUTTON_CFG_RELOAD);
        DI_DISABLE(IDC_BUTTON_CONFIG_FILE);
#endif

        switch (CurrentPlayerLanguage) {
        case LANGUAGE_ENGLISH:
            CheckRadioButton(hwnd, IDC_RADIOBUTTON_JAPANESE, IDC_RADIOBUTTON_ENGLISH,
                             IDC_RADIOBUTTON_ENGLISH);
            break;
        case LANGUAGE_JAPANESE:
        default:
            CheckRadioButton(hwnd, IDC_RADIOBUTTON_JAPANESE, IDC_RADIOBUTTON_ENGLISH,
                             IDC_RADIOBUTTON_JAPANESE);
            break;
        }
        DLG_FLAG_TO_CHECKBUTTON(hwnd, IDC_CHECK_INIFILE_AUTOSAVE,
                                sp_temp->IniFileAutoSave);

        // MIDI In
#if defined(WINDRV_SETUP)
        DI_DISABLE(IDC_COMBO_PORT_NUM);
        for (i = 0; i < ARRAY_SIZE(dwCtlPortIDs); i++) {
            DI_DISABLE(dwCtlPortIDs[i]);
        }
        DI_DISABLE(IDC_CHECK_USE_TWSYN_BRIDGE);
#else
#if defined(TWSYNG32) && !defined(TWSYNSRV) && defined(USE_TWSYN_BRIDGE)
        DLG_FLAG_TO_CHECKBUTTON(hwnd, IDC_CHECK_USE_TWSYN_BRIDGE, st_temp->opt_use_twsyn_bridge);
#else
        DI_DISABLE(IDC_CHECK_USE_TWSYN_BRIDGE);
#endif
        GetMidiINDrivers();

        CB_RESET(IDC_COMBO_PORT_NUM);
        for (i = 0; i <= MAX_PORT; i++) {
            char buff[32];
            sprintf(buff, "%d", i);
            CB_INSSTRA(IDC_COMBO_PORT_NUM, buff);
        }
        if (MidiINDrivers != NULL) {
            for (i = 0; i < ARRAY_SIZE(dwCtlPortIDs); i++) {
                CB_RESET(dwCtlPortIDs[i]);
            }
            for (i = 0; MidiINDrivers[i] != NULL; i ++) {
                const TCHAR *str = MidiINDrivers[i];
                int j;
                for (j = 0; j < ARRAY_SIZE(dwCtlPortIDs); j++) {
                    CB_INSSTR(dwCtlPortIDs[j], str ? str : TEXT(""));
                }
                safe_free(MidiINDrivers[i]);
            }
            safe_free(MidiINDrivers);
            MidiINDrivers = NULL;
        }
        CB_SET(IDC_COMBO_PORT_NUM, st_temp->SynPortNum);
        for (i = 0; i < MAX_PORT; i++) {
            if (st_temp->SynIDPort[i] > midi_in_max)
                st_temp->SynIDPort[i] = 0; // reset
        }
        for (i = 0; i < MAX_PORT && i < ARRAY_SIZE(dwCtlPortIDs); i++) {
            DI_ENABLE(dwCtlPortIDs[i]);
            CB_SET(dwCtlPortIDs[i], st_temp->SynIDPort[i]);
        }
#endif

#if defined(WINDRV_SETUP)
		DI_DISABLE(IDC_COMBO_CTL_VEBOSITY);
		DI_DISABLE(IDC_EDIT_SPECTROGRAM_UPDATE);
		DI_DISABLE(IDC_CHECK_SYN_AUTOSTART);
#else // ! defined(WINDRV_SETUP)
		// console
		tmp = char_count(st_temp->opt_ctl + 1, 'v') - char_count(st_temp->opt_ctl + 1, 'q') + 1;
		for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_CTL_VEBOSITY_num); i++)
			CB_INSSTR(IDC_COMBO_CTL_VEBOSITY, cb_info_IDC_COMBO_CTL_VEBOSITY[i]);
		CB_SET(IDC_COMBO_CTL_VEBOSITY, CB_FIND(cb_info_IDC_COMBO_CTL_VEBOSITY_num, tmp, 1));
		// spectrogram
#ifdef SUPPORT_SOUNDSPEC
		SetDlgItemFloat(hwnd, IDC_EDIT_SPECTROGRAM_UPDATE, st_temp->spectrogram_update_sec);
#else
		DI_DISABLE(IDC_EDIT_SPECTROGRAM_UPDATE);
#endif
		DLG_FLAG_TO_CHECKBUTTON(hwnd, IDC_CHECK_SYN_AUTOSTART, st_temp->syn_AutoStart);
#endif // defined(WINDRV_SETUP)
		SetDlgItemInt(hwnd,IDC_EDIT_SYN_SH_TIME,st_temp->SynShTime,FALSE);
		SetDlgItemInt(hwnd,IDC_EDIT_RTSYN_LATENCY,st_temp->opt_rtsyn_latency,FALSE);
		DLG_FLAG_TO_CHECKBUTTON(hwnd, IDC_CHECK_RTSYN_SKIP_AQ, st_temp->opt_rtsyn_skip_aq);

		if(PlayerLanguage == LANGUAGE_JAPANESE) {
			for (i = 0; i < CB_NUM(process_priority_num); i++)
				CB_INSSTR(IDC_COMBO_PROCESS_PRIORITY, process_priority_name_jp[i]);
			for (i = 0; i < CB_NUM(thread_priority_num); i++)		
				CB_INSSTR(IDC_COMBO_SYN_THREAD_PRIORITY, thread_priority_name_jp[i] );
		}else{
			for (i = 0; i < CB_NUM(process_priority_num); i++)
				CB_INSSTR(IDC_COMBO_PROCESS_PRIORITY, process_priority_name_en[i] );
			for (i = 0; i < CB_NUM(thread_priority_num); i++)		
				CB_INSSTR(IDC_COMBO_SYN_THREAD_PRIORITY, thread_priority_name_en[i] );
		}
		// Select process priority
		CB_SET(IDC_COMBO_PROCESS_PRIORITY, CB_FIND(process_priority_num, st_temp->processPriority, 2));
		// Select thread priority
		CB_SET(IDC_COMBO_SYN_THREAD_PRIORITY, CB_FIND(thread_priority_num, st_temp->syn_ThreadPriority, 3));
		// compute_thread_num
		for (i = 0; i <= cb_num_IDC_COMBO_COMPUTE_THREAD_NUM; i++)
			CB_INSSTR(IDC_COMBO_COMPUTE_THREAD_NUM, cb_info_IDC_COMBO_COMPUTE_THREAD_NUM[i]);
		CB_SET(IDC_COMBO_COMPUTE_THREAD_NUM, CB_FIND(cb_info_IDC_COMBO_COMPUTE_THREAD_NUM_num, st_temp->compute_thread_num, 0));

		initflag = 0;
		break;
	case WM_COMMAND:
	switch (LOWORD(wParam)) {
		
#if defined(TWSYNG32) && !defined(TWSYNSRV) && defined(USE_TWSYN_BRIDGE)
        case IDC_CHECK_USE_TWSYN_BRIDGE:
            for (i = 0; i < ARRAY_SIZE(dwCtlPortIDs); i++) {
                CB_RESET(dwCtlPortIDs[i]);
            }

            DLG_CHECKBUTTON_TO_FLAG(hwnd, IDC_CHECK_USE_TWSYN_BRIDGE, st_temp->opt_use_twsyn_bridge);
            tmp = CB_GET(IDC_COMBO_PORT_NUM);
            if (tmp != CB_ERR)
                st_temp->SynPortNum = tmp;

            GetMidiINDrivers();

            if (MidiINDrivers != NULL) {
                for (i = 0; MidiINDrivers[i] != NULL; i ++) {
                    const TCHAR *str = MidiINDrivers[i];
                    int j;
                    for (j = 0; j < ARRAY_SIZE(dwCtlPortIDs); j++) {
                        CB_INSSTR(dwCtlPortIDs[j], str ? str : TEXT(""));
                    }
                    safe_free(MidiINDrivers[i] );
                }
                safe_free(MidiINDrivers);
                MidiINDrivers = NULL;
            }
            for (i = 0; i < MAX_PORT; i++) {
                st_temp->SynIDPort[i] = 0; // reset
            }
            for (i = 0; i < MAX_PORT && i < ARRAY_SIZE(dwCtlPortIDs); i++) {
                CB_SET(dwCtlPortIDs[i], st_temp->SynIDPort[i]);
            }
            break;
#endif
        case IDC_BUTTON_CONFIG_FILE: {
            char filename[FILEPATH_MAX];
            filename[0] = '\0';
            EB_GETTEXTA(IDC_COMBO_CONFIG_FILE, filename, FILEPATH_MAX - 1);
            if (!DlgOpenConfigFile(filename, hwnd))
                if (filename[0] != '\0') {
                    EB_SETTEXTA(IDC_COMBO_CONFIG_FILE, filename); // A
                    tmp = SendDlgItemMessage(hwnd, IDC_COMBO_CONFIG_FILE, WM_GETTEXTLENGTH, 0, 0); // A/W
                    SendDlgItemMessage(hwnd, IDC_COMBO_CONFIG_FILE, CB_SETEDITSEL, 0, MAKELPARAM(tmp, tmp)); // A/W
                }
            break; }
        case IDC_BUTTON_CFG_EDIT: {
            char filename[FILEPATH_MAX];
            const char *editor_path = getenv("TIMIDITY_CFG_EDITOR");
            const TCHAR *msg,
                   msg_en[] = TEXT("Please set TIMIDITY_CFG_EDITOR environment variable"),
                   msg_jp[] = TEXT("環境変数 TIMIDITY_CFG_EDITOR を設定してください");
            switch (CurrentPlayerLanguage) {
            case LANGUAGE_ENGLISH: msg = msg_en; break;
            case LANGUAGE_JAPANESE: default: msg = msg_jp; break;
            }

            filename[0] = '\0';
            EB_GETTEXTA(IDC_COMBO_CONFIG_FILE, filename, FILEPATH_MAX - 1);

            w32_reset_exe_directory();

            if (check_file_extension(filename, ".txt", 0) == 1 ||
                check_file_extension(filename, ".cfg", 0) == 1 ||
                check_file_extension(filename, ".config", 0) == 1)
            {
                if (editor_path != NULL) {
                    ShellExecuteA(NULL, "open", editor_path, filename, NULL, SW_SHOWNORMAL); // A
                } else {
                    ShellExecuteA(NULL, "open", "notepad.exe", filename, NULL, SW_SHOWNORMAL); // A
                    if (get_verbosity_level() >= VERB_NORMAL)
                        MessageBox(hwnd, msg, NULL, MB_OK | MB_ICONWARNING);
                }
            }
            break; }	 
/*			ShellExecute(NULL, "open", "notepad.exe", ConfigFile, NULL, SW_SHOWNORMAL);
			break;
		case IDC_BUTTON_CFG_DIR:
			ShellExecute(NULL, "open", ConfigFileOpenDir, NULL, NULL, SW_SHOWNORMAL);
			break;*/
		case IDC_BUTTON_CFG_RELOAD:
		{
			int i;
			for (i = 0; i < PREF_PAGE_MAX; i++ ) {
				SendMessage ( pref_pages[i].hwnd, WM_MYSAVE, (WPARAM)0, (LPARAM)0 );
			}
			reload_cfg();
			SetWindowLongPtr(hwnd,	DWLP_MSGRESULT, TRUE);
		}
			break;
		case IDC_RADIOBUTTON_JAPANESE:
		case IDC_RADIOBUTTON_ENGLISH:
			break;
		default:
		break;
	  }
		PrefWndSetOK = 1;
		break;

    case WM_MYSAVE:
        if (initflag)
            break;

        EB_GETTEXTA(IDC_COMBO_CONFIG_FILE, sp_temp->ConfigFile, FILEPATH_MAX - 1);
        if (CH_GET(IDC_RADIOBUTTON_ENGLISH)) {
            sp_temp->PlayerLanguage = LANGUAGE_ENGLISH;
        } else if (CH_GET(IDC_RADIOBUTTON_JAPANESE)) {
            sp_temp->PlayerLanguage = LANGUAGE_JAPANESE;
        }

#if !defined(WINDRV_SETUP)
#if defined(TWSYNG32) && !defined(TWSYNSRV) && defined(USE_TWSYN_BRIDGE)
        DLG_CHECKBUTTON_TO_FLAG(hwnd, IDC_CHECK_USE_TWSYN_BRIDGE, st_temp->opt_use_twsyn_bridge);
#endif
        st_temp->syn_AutoStart = CH_GET(IDC_CHECK_SYN_AUTOSTART);
        tmp = CB_GET(IDC_COMBO_PORT_NUM);
        if (tmp != CB_ERR) st_temp->SynPortNum = tmp;
        for (i = 0; i < MAX_PORT && i < ARRAY_SIZE(dwCtlPortIDs); i++) {
            tmp = CB_GET(dwCtlPortIDs[i]);
            if (tmp != CB_ERR) st_temp->SynIDPort[i] = tmp;
        }

        // console
        {
            char *p;
            /* remove 'v' and 'q' from st_temp->opt_ctl */
            while (strchr(st_temp->opt_ctl + 1, 'v'))
                 SettingCtlFlag(st_temp, 'v', 0);
            while (strchr(st_temp->opt_ctl + 1, 'q'))
                 SettingCtlFlag(st_temp, 'q', 0);
            /* append 'v' or 'q' */
            p = st_temp->opt_ctl + strlen(st_temp->opt_ctl);
            tmp = cb_info_IDC_COMBO_CTL_VEBOSITY_num[CB_GETS(IDC_COMBO_CTL_VEBOSITY, 1)];
            RANGE(tmp, -1, 4); /* -1..4 */
            while (tmp > 1) { *p++ = 'v'; tmp--; }
            while (tmp < 1) { *p++ = 'q'; tmp++; }
        }

        // spectrogram
#ifdef SUPPORT_SOUNDSPEC
        st_temp->spectrogram_update_sec = GetDlgItemFloat(hwnd, IDC_EDIT_SPECTROGRAM_UPDATE);
#endif /* SUPPORT_SOUNDSPEC */
#endif // !defined(WINDRV_SETUP)

        // synthesize
        st_temp->SynShTime = EB_GET_UINT(IDC_EDIT_SYN_SH_TIME);
        RANGE(st_temp->SynShTime, 1, 1000);
        st_temp->opt_rtsyn_latency = EB_GET_UINT(IDC_EDIT_RTSYN_LATENCY);
        RANGE(st_temp->opt_rtsyn_latency, 1, 1000);
        DLG_CHECKBUTTON_TO_FLAG(hwnd, IDC_CHECK_RTSYN_SKIP_AQ, st_temp->opt_rtsyn_skip_aq);

        // Set process priority
        tmp = CB_GET(IDC_COMBO_PROCESS_PRIORITY);
        if (tmp != CB_ERR)
            st_temp->processPriority = process_priority_num[tmp];
        // Set thread priority
        tmp = CB_GET(IDC_COMBO_SYN_THREAD_PRIORITY);
        if (tmp != CB_ERR)
            st_temp->syn_ThreadPriority = thread_priority_num[tmp];
        // compute_thread_num
        st_temp->compute_thread_num = cb_info_IDC_COMBO_COMPUTE_THREAD_NUM_num[CB_GETS(IDC_COMBO_COMPUTE_THREAD_NUM, 0)];

        SetWindowLongPtr(hwnd, DWLP_MSGRESULT, FALSE);
        break;

    case WM_DESTROY:
        if (strcmp(sp_temp->ConfigFile, CurrentConfigFile) != 0) {
            const TCHAR *msg,
                   msg_en[] = TEXT("Press the Reload button to apply instruments"),
                   msg_jp[] = TEXT("音色情報は強制再読込ボタンを押すと反映されます");
            switch (CurrentPlayerLanguage) {
            case LANGUAGE_ENGLISH: msg = msg_en; break;
            case LANGUAGE_JAPANESE: default: msg = msg_jp; break;
            }
            if (get_verbosity_level() >= VERB_NORMAL)
                MessageBox(hwnd, msg, TEXT(""), MB_OK | MB_ICONWARNING);
        }
        safe_free(CurrentConfigFile);
        CurrentConfigFile = 0;
        break;

    default:
        break;
	}
	return FALSE;
}
#endif


///r
// IDC_COMBO_MIDI_TYPE
#define SYSTEM_MID_OFFSET (0x100)

#define cb_num_IDC_COMBO_MIDI_TYPE 14

static int cb_info_IDC_COMBO_MIDI_TYPE_num[] = {
//    DEFAULT_SYSTEM_MODE,
    GM_SYSTEM_MODE,
    GM2_SYSTEM_MODE,
    GS_SYSTEM_MODE,
    XG_SYSTEM_MODE,
    SD_SYSTEM_MODE,
	KG_SYSTEM_MODE,
	CM_SYSTEM_MODE,
//    DEFAULT_SYSTEM_MODE + SYSTEM_MID_OFFSET,
    GM_SYSTEM_MODE + SYSTEM_MID_OFFSET,
    GM2_SYSTEM_MODE + SYSTEM_MID_OFFSET,
    GS_SYSTEM_MODE + SYSTEM_MID_OFFSET,
    XG_SYSTEM_MODE + SYSTEM_MID_OFFSET,
    SD_SYSTEM_MODE + SYSTEM_MID_OFFSET,
	KG_SYSTEM_MODE + SYSTEM_MID_OFFSET,
	CM_SYSTEM_MODE + SYSTEM_MID_OFFSET,
};

static const TCHAR *cb_info_IDC_COMBO_MIDI_TYPE_en[] = {
//	TEXT("DEFAULT (default)"),
    TEXT("GM (default)"),
    TEXT("GM2 (default)"),
    TEXT("GS (default)"),
    TEXT("XG (default)"),
	TEXT("SD (default)"),
	TEXT("KORG (default)"),
	TEXT("CM/LA (default)"),
    TEXT("GM (system)"),
    TEXT("GM2 (system)"),
    TEXT("GS (system)"),
    TEXT("XG (system)"),
	TEXT("SD (system)"),
	TEXT("KORG (system)"),
	TEXT("CM/LA (system)"),
};

static const TCHAR *cb_info_IDC_COMBO_MIDI_TYPE_jp[] = {
//	TEXT("DEFAULT (デフォルト)"),
    TEXT("GM (デフォルト)"),
    TEXT("GM2 (デフォルト)"),
    TEXT("GS (デフォルト)"),
    TEXT("XG (デフォルト)"),
	TEXT("SD (デフォルト)"),
	TEXT("KORG (デフォルト)"),
	TEXT("CM/LA (デフォルト)"),
    TEXT("GM (固定)"),
    TEXT("GM2 (固定)"),
    TEXT("GS (固定)"),
    TEXT("XG (固定)"),
	TEXT("SD (固定)"),
	TEXT("KORG (固定)"),
	TEXT("CM/LA (固定)"),
};

// IDC_COMBO_MODULE
static int cb_find_module(int val)
{
	int i;
	for (i = 0; i < module_list_num; i++)
		if (val == module_list[i].num) {return i;}
	return 0;
}

// IDC_COMBO_REVERB
#if VSTWRAP_EXT
#define cb_num_IDC_COMBO_REVERB 12
#else
#define cb_num_IDC_COMBO_REVERB 9
#endif

static const TCHAR *cb_info_IDC_COMBO_REVERB_en[] = {
	TEXT("No Reverb"),
	TEXT("Standard Reverb"),
	TEXT("Global Old Reverb"),
	TEXT("New Reverb 1"),
	TEXT("Global New Reverb 1"),
	TEXT("Standard Reverb EX"),
	TEXT("Global Reverb EX"),
	TEXT("Sampling Reverb"),
	TEXT("Global Sampling Reverb"),
	TEXT("Reverb VST"),
	TEXT("Global Reverb VST"),
	TEXT("Channel VST"),
};

static const TCHAR *cb_info_IDC_COMBO_REVERB_jp[] = {
	TEXT("リバーブなし"),
	TEXT("標準リバーブ"),
	TEXT("標準リバーブ (グローバル)"),
	TEXT("新リバーブ"),
	TEXT("新リバーブ (グローバル)"),
	TEXT("標準リバーブ EX"),
	TEXT("標準リバーブ EX (グローバル)"),
	TEXT("サンプリングリバーブ"),
	TEXT("サンプリングリバーブ (グローバル)"),
	TEXT("リバーブ VST"),
	TEXT("リバーブ VST (グローバル)"),
	TEXT("チャンネル VST"),
};

// IDC_SLIDER_REVERB
#define sl_max_REVERB 127
#define sl_min_REVERB 0

// IDC_COMBO_CHORUS
#if VSTWRAP_EXT
#define cb_num_IDC_COMBO_CHORUS 8
#else
#define cb_num_IDC_COMBO_CHORUS 7
#endif

static const TCHAR *cb_info_IDC_COMBO_CHORUS_en[] = {
	TEXT("No Chorus"),
	TEXT("Standard Chorus"),
	TEXT("Standard Chorus 2"),
	TEXT("Standard Chorus 3 2phase"),
	TEXT("Standard Chorus 4 3phase"),
	TEXT("Standard Chorus 5 6phase"),
	TEXT("Standard Chorus EX"),
	TEXT("Chorus VST"),
	TEXT("Standard Chorus EX2"),
};

static const TCHAR *cb_info_IDC_COMBO_CHORUS_jp[] = {
	TEXT("コーラスなし"),
	TEXT("標準コーラス"),
	TEXT("標準コーラス2"),
	TEXT("標準コーラス3 2phase"),
	TEXT("標準コーラス4 3phase"),
	TEXT("標準コーラス5 6phase"),
	TEXT("標準コーラス EX"),
	TEXT("コーラス VST"),
	TEXT("標準コーラス EX2"),
};

// IDC_SLIDER_CHORUS
#define sl_max_CHORUS 127
#define sl_min_CHORUS 0

// IDC_COMBO_DELAY
#define cb_num_IDC_COMBO_DELAY 2

static const TCHAR *cb_info_IDC_COMBO_DELAY_en[] = {
	TEXT("No Delay"),
	TEXT("Standard Delay"),
	TEXT("Delay VST"),
};

static const TCHAR *cb_info_IDC_COMBO_DELAY_jp[] = {
	TEXT("ディレイなし"),
	TEXT("標準ディレイ"),
	TEXT("ディレイ VST"),
};

///r
// IDC_COMBO_LPF
#ifdef _DEBUG
#define cb_num_IDC_COMBO_LPF 35
#else
#define cb_num_IDC_COMBO_LPF 21
#endif

// sort filter.h
static const TCHAR *cb_info_IDC_COMBO_LPF_en[] = {
	TEXT("No Filter"),
	TEXT("Lowpass Filter (12dB/oct)"),
	TEXT("Lowpass Filter (24dB/oct)"),
	TEXT("Lowpass Filter (butterworth)"),
	TEXT("Lowpass Filter (12dB/oct)-2"),
	TEXT("Lowpass Filter (24dB/oct)-2"),
	TEXT("Lowpass Filter (6dB/oct)"),
	TEXT("Lowpass Filter (18dB/oct)"),
	TEXT("Lowpass Filter (two first order)"),
	TEXT("Highpass Filter (butterworth)"),
	TEXT("Bandpass Filter (butterworth)"),
	TEXT("Peak Filter"),
	TEXT("Notch Filter"),
	TEXT("Lowpass Filter (12dB/oct)-3"),
	TEXT("Highpass Filter (12dB/oct)-3"),
	TEXT("Bandpass Filter (12dB/oct)-3"),
	TEXT("Bandcut Filter (12dB/oct)-3"),
	TEXT("Highpass Filter (6dB/oct)"),
	TEXT("Highpass Filter (12dB/oct)-2"),
	TEXT("HBF (L6L12)"),
	TEXT("HBF (L12L6)"),
	// debug
	TEXT("HBF (L12H6)"),
	TEXT("HBF (L24H6)"),
	TEXT("HBF (L24H12)"),
	TEXT("HBF (L12OCT)"),
	TEXT("HBF (L24OCT)"),
	// debug
	TEXT("LPF (6dB/oct) x2"),
	TEXT("LPF (6dB/oct) x3"),
	TEXT("LPF (6dB/oct) x4"),
	TEXT("LPF (6dB/oct) x8"),
	TEXT("LPF (6dB/oct) x16"),
	TEXT("LPF (butterworth) x2"),
	TEXT("LPF (butterworth) x3"),
	TEXT("LPF (butterworth) x4"),
	TEXT("LPF (24dB/oct)-2 x2"),
	TEXT("LPF FIR"),
};

static const TCHAR *cb_info_IDC_COMBO_LPF_jp[] = {
	TEXT("フィルタなし"),
	TEXT("LPF (12dB/oct)"),
	TEXT("LPF (24dB/oct)"),
	TEXT("LPF (butterworth)"),
	TEXT("LPF (12dB/oct)-2"),
	TEXT("LPF (24dB/oct)-2"),
	TEXT("LPF (6dB/oct)"),
	TEXT("LPF (18dB/oct)"),
	TEXT("LPF (two first order)"),
	TEXT("HPF (butterworth)"),
	TEXT("BPF (butterworth)"),
	TEXT("Peak Filter"),
	TEXT("Notch Filter"),
	TEXT("LPF (12dB/oct)-3"),
	TEXT("HPF (12dB/oct)-3"),
	TEXT("BPF (12dB/oct)-3"),
	TEXT("BCF (12dB/oct)-3"),
	TEXT("HPF (6dB/oct)"),
	TEXT("HPF (12dB/oct)-2"),
	TEXT("HBF (L6L12)"),
	TEXT("HBF (L12L6)"),
	// debug
	TEXT("HBF (L12H6)"),
	TEXT("HBF (L24H6)"),
	TEXT("HBF (L24H12)"),
	TEXT("HBF (L12OCT)"),
	TEXT("HBF (L24OCT)"),
	// debug
	TEXT("LPF (6dB/oct) x2"),
	TEXT("LPF (6dB/oct) x3"),
	TEXT("LPF (6dB/oct) x4"),
	TEXT("LPF (6dB/oct) x8"),
	TEXT("LPF (6dB/oct) x16"),
	TEXT("LPF (butterworth) x2"),
	TEXT("LPF (butterworth) x3"),
	TEXT("LPF (butterworth) x4"),
	TEXT("LPF (24dB/oct)-2 x2"),
	TEXT("LPF FIR"),
};

// IDC_COMBO_HPF
#define cb_num_IDC_COMBO_HPF 5

static const TCHAR *cb_info_IDC_COMBO_HPF_en[] = {
	TEXT("No Filter"),
	TEXT("Highpass Filter (butterworth)"),
	TEXT("Highpass Filter (12dB/oct)-3"),
	TEXT("Highpass Filter (6dB/oct)"),
	TEXT("Highpass Filter (12dB/oct)-2"),
};

static const TCHAR *cb_info_IDC_COMBO_HPF_jp[] = {
	TEXT("フィルタなし"),
	TEXT("HPF (butterworth)"),
	TEXT("HPF (12dB/oct)-3"),
	TEXT("HPF (6dB/oct)"),
	TEXT("HPF (12dB/oct)-2"),
};


// IDC_COMBO_OVOICES
static int cb_info_IDC_COMBO_OVOICE_num[] = {
	-1,
	0,
	1,
	2,
	3,
	4,
	6,
	8,
	12,
	16,
	24,
	32,
	48,
	64,
	96,
	128,
	160,
	256,
	384,
	512,
};

static const TCHAR *cb_info_IDC_COMBO_OVOICE[] = {
	TEXT("OFF"),
	TEXT("0"),
	TEXT("1"),
	TEXT("2"),
	TEXT("3"),
	TEXT("4"),
	TEXT("6"),
	TEXT("8"),
	TEXT("12"),
	TEXT("16"),
	TEXT("24"),
	TEXT("32"),
	TEXT("48"),
	TEXT("64"),
	TEXT("96"),
	TEXT("128"),
	TEXT("160"),
	TEXT("256"),
	TEXT("384"),
	TEXT("512"),
};

// IDC_COMBO_CONTROL_RATIO
static int cb_info_IDC_COMBO_CONTROL_RATIO_num[] = {
	0,
	1,
	2,
	3,
	4,
	6,
	8,
	12,
	16,
	24,
	32,
	48,
	64,
	96,
	128,
	160,
	255,
};
static const TCHAR *cb_info_IDC_COMBO_CONTROL_RATIO[] = {
	TEXT("1ms"),
	TEXT("1"),
	TEXT("2"),
	TEXT("3"),
	TEXT("4"),
	TEXT("6"),
	TEXT("8"),
	TEXT("12"),
	TEXT("16"),
	TEXT("24"),
	TEXT("32"),
	TEXT("48"),
	TEXT("64"),
	TEXT("96"),
	TEXT("128"),
	TEXT("160"),
	TEXT("255"),
};

// IDC_COMBO_CUT_SHORT_TIME
static int cb_info_IDC_COMBO_CUT_SHORT_TIME_num[] = {
	0,
	10,
	15,
	20,
	25,
	30,
	35,
	40,
	45,
	50,
	60,
	70,
	80,
	90,
	100,
	120,
	140,
	160,
	180,
	200,
	220,
	240,
	260,
	280,
	300,
	350,
	400,
	450,
	500,
};

static const TCHAR *cb_info_IDC_COMBO_CUT_SHORT_TIME[] = {
	TEXT("OFF"),
	TEXT("10"),
	TEXT("15"),
	TEXT("20"),
	TEXT("25"),
	TEXT("30"),
	TEXT("35"),
	TEXT("40"),
	TEXT("45"),
	TEXT("50"),
	TEXT("60"),
	TEXT("70"),
	TEXT("80"),
	TEXT("90"),
	TEXT("100"),
	TEXT("120"),
	TEXT("140"),
	TEXT("160"),
	TEXT("180"),
	TEXT("200"),
	TEXT("220"),
	TEXT("240"),
	TEXT("260"),
	TEXT("280"),
	TEXT("300"),
	TEXT("350"),
	TEXT("400"),
	TEXT("450"),
	TEXT("500"),
};

// IDC_COMBO_CHANNEL_VOICES
static int cb_info_IDC_COMBO_CHANNEL_VOICES_num[] = {
	4,
	8,
	12,
	16,
	20,
	24,
	28,
	32,
	40,
	48,
	56,
	64,
	80,
	96,
	112,
	128,
	160,
	256,
	384,
	512,
};

static const TCHAR *cb_info_IDC_COMBO_CHANNEL_VOICES[] = {
	TEXT("4"),
	TEXT("8"),
	TEXT("12"),
	TEXT("16"),
	TEXT("20"),
	TEXT("24"),
	TEXT("28"),
	TEXT("32"),
	TEXT("40"),
	TEXT("48"),
	TEXT("56"),
	TEXT("64"),
	TEXT("80"),
	TEXT("96"),
	TEXT("112"),
	TEXT("128"),
	TEXT("160"),
	TEXT("256"),
	TEXT("384"),
	TEXT("512"),
};

// IDC_COMBO_MIX_ENV
static int cb_info_IDC_COMBO_MIX_ENV_num[] = {
	0,
	1,
	2,
	4,
	8,
	16,
	32,
	64,
	128,
	256,
};

static const TCHAR *cb_info_IDC_COMBO_MIX_ENV[] = {
	TEXT("OFF"),
	TEXT("1"),
	TEXT("2"),
	TEXT("4"),
	TEXT("8"),
	TEXT("16"),
	TEXT("32"),
	TEXT("64"),
	TEXT("128"),
	TEXT("256"),
};

// IDC_COMBO_MOD_UPDATE
static int cb_info_IDC_COMBO_MOD_UPDATE_num[] = {
	1,
	2,
	3,
	4,
	5,
	6,
	7,
	8,
	9,
	10,
	12,
	14,
	16,
	18,
	20,
	25,
	30,
	35,
	40,
	45,
	50,
	60,
	70,
	80,
	90,
	100,
};

static const TCHAR *cb_info_IDC_COMBO_MOD_UPDATE[] = {
	TEXT("1"),
	TEXT("2"),
	TEXT("3"),
	TEXT("4"),
	TEXT("5"),
	TEXT("6"),
	TEXT("7"),
	TEXT("8"),
	TEXT("9"),
	TEXT("10"),
	TEXT("12"),
	TEXT("14"),
	TEXT("16"),
	TEXT("18"),
	TEXT("20"),
	TEXT("25"),
	TEXT("30"),
	TEXT("35"),
	TEXT("40"),
	TEXT("45"),
	TEXT("50"),
	TEXT("60"),
	TEXT("70"),
	TEXT("80"),
	TEXT("90"),
	TEXT("100"),
};

// IDC_SLIDER_LIMITER
#define sl_max_LIMITER 1600
#define sl_min_LIMITER 1

// IDC_COMBO_VOICES
static int cb_info_IDC_COMBO_VOICES_num[] = {
	8,
	16,
	24,
	32,
	48,
	64,
	96,
	128,
	160,
	256,
	384,
	512,
	768,
	1000,
};

static const TCHAR *cb_info_IDC_COMBO_VOICES[] = {
	TEXT("8"),
	TEXT("16"),
	TEXT("24"),
	TEXT("32"),
	TEXT("48"),
	TEXT("64"),
	TEXT("96"),
	TEXT("128"),
	TEXT("160"),
	TEXT("256"),
	TEXT("384"),
	TEXT("512"),
	TEXT("768"),
	TEXT("1000"),
};

// IDC_SLIDER_AMPLIFICATION
#define sl_max_AMPLIFICATION 800
#define sl_min_AMPLIFICATION 0

// IDC_SLIDER_DRUM_POWER
#define sl_max_DRUM_POWER 200
#define sl_min_DRUM_POWER 0

// IDC_COMBO_ADD_PLAY_TIME
static int cb_info_IDC_COMBO_ADD_PLAY_TIME_num[] = {
    0,
    1,
    2,
    3,
    4,
    5,
	6,
	7,
	8,
	9,
	10,
	11,
	12,
	13,
	14,
	15,
	16,
	17,
	18,
	19,
	20,	
};

static const TCHAR *cb_info_IDC_COMBO_ADD_PLAY_TIME[] = {
	TEXT("OFF"),
	TEXT("1"),
	TEXT("2"),
	TEXT("3"),
	TEXT("4"),
	TEXT("5"),
	TEXT("6"),
	TEXT("7"),
	TEXT("8"),
	TEXT("9"),
	TEXT("10"),
	TEXT("11"),
	TEXT("12"),
	TEXT("13"),
	TEXT("14"),
	TEXT("15"),
	TEXT("16"),
	TEXT("17"),
	TEXT("18"),
	TEXT("19"),
	TEXT("20"),
};

// IDC_COMBO_ADD_SILENT_TIME
static int cb_info_IDC_COMBO_ADD_SILENT_TIME_num[] = {
    0,
    1,
    2,
    3,
    4,
    5,
	6,
	7,
	8,
	9,
	10,
	11,
	12,
	13,
	14,
	15,
	16,
	17,
	18,
	19,
	20,	
};

static const TCHAR *cb_info_IDC_COMBO_ADD_SILENT_TIME[] = {
	TEXT("OFF"),
	TEXT("1"),
	TEXT("2"),
	TEXT("3"),
	TEXT("4"),
	TEXT("5"),
	TEXT("6"),
	TEXT("7"),
	TEXT("8"),
	TEXT("9"),
	TEXT("10"),
	TEXT("11"),
	TEXT("12"),
	TEXT("13"),
	TEXT("14"),
	TEXT("15"),
	TEXT("16"),
	TEXT("17"),
	TEXT("18"),
	TEXT("19"),
	TEXT("20"),
};
// IDC_COMBO_EMU_DELAY_TIME
static int cb_info_IDC_COMBO_EMU_DELAY_TIME_num[] = {
    0,
    1,
    2,
    3,
    4,
    5,
	6,
	7,
	8,
	9,
	10,
	12,
	14,
	16,
	18,
	20,	
	22,	
	24,	
	26,	
	28,	
	30,	
	35,	
	40,	
	45,	
	50,	
};

static const TCHAR *cb_info_IDC_COMBO_EMU_DELAY_TIME[] = {
	TEXT("OFF"),
	TEXT("0.1"),
	TEXT("0.2"),
	TEXT("0.3"),
	TEXT("0.4"),
	TEXT("0.5"),
	TEXT("0.6"),
	TEXT("0.7"),
	TEXT("0.8"),
	TEXT("0.9"),
	TEXT("1.0"),
	TEXT("1.2"),
	TEXT("1.4"),
	TEXT("1.6"),
	TEXT("1.8"),
	TEXT("2.0"),
	TEXT("2.2"),
	TEXT("2.4"),
	TEXT("2.6"),
	TEXT("2.8"),
	TEXT("3.0"),
	TEXT("3.5"),
	TEXT("4.0"),
	TEXT("4.5"),
	TEXT("5.0"),
};

// IDC_COMBO_NOISESHARPING
#define cb_num_IDC_COMBO_NOISESHARPING 5

static const TCHAR *cb_info_IDC_COMBO_NOISESHARPING_en[] = {
	TEXT("No NS"),
	TEXT("Old NS"),
	TEXT("OD + New NS"),
	TEXT("Tube + New NS"),
	TEXT("New NS"),
}; 

static const TCHAR *cb_info_IDC_COMBO_NOISESHARPING_jp[] = {
	TEXT("NSなし"),
	TEXT("従来のNS"),
	TEXT("OD + 新NS"),
	TEXT("真空管 + 新NS"),
	TEXT("新NS"),
};

// IDC_COMBO_RESAMPLE
#define cb_num_IDC_COMBO_RESAMPLE 11

static const TCHAR *cb_info_IDC_COMBO_RESAMPLE_en[] = {
	TEXT("No resample"),
	TEXT("Linear"),
	TEXT("C spline"),
	TEXT("Lagrange"),
	TEXT("Newton"),
	TEXT("Gauss"),
	TEXT("Sharp"),
	TEXT("Linear %"),
	TEXT("Sine"),
	TEXT("Square"),
	TEXT("Lanczos"),
};

static const TCHAR *cb_info_IDC_COMBO_RESAMPLE_jp[] = {
	TEXT("補間なし"),
	TEXT("線形(リニア)"),
	TEXT("Cスプライン"),
	TEXT("ラグランジュ"),
	TEXT("ニュートン"),
	TEXT("ガウス風"),
	TEXT("シャープ"),
	TEXT("線形 %"),
	TEXT("サイン"),
	TEXT("スクエア"),
	TEXT("Lanczos"),
};

// IDC_COMBO_RESAMPLE_PARAM
static int cb_info_IDC_COMBO_RESAMPLE_PARAM_num[] = {
	0,
	1,
	2,
	3,
	4,
	6,
	8,
	12,
	16,
	20,
	24,
	28,
	32,
	36,
	40,
	45,
	48,
	50,
	60,
	64,
	70,
	80,
	90,
	96,
	100,
};

static const TCHAR *cb_info_IDC_COMBO_RESAMPLE_PARAM[] = {
	TEXT("DEF"),
	TEXT("1"),
	TEXT("2"),
	TEXT("3"),
	TEXT("4"),
	TEXT("6"),
	TEXT("8"),
	TEXT("12"),
	TEXT("16"),
	TEXT("20"),
	TEXT("24"),
	TEXT("28"),
	TEXT("32"),
	TEXT("36"),
	TEXT("40"),
	TEXT("45"),
	TEXT("48"),
	TEXT("50"),
	TEXT("60"),
	TEXT("64"),
	TEXT("70"),
	TEXT("80"),
	TEXT("90"),
	TEXT("96"),
	TEXT("100"),
};

// IDC_COMBO_RESAMPLE_FILTER
#define cb_num_IDC_COMBO_RESAMPLE_FILTER 10

static const TCHAR *cb_info_IDC_COMBO_RESAMPLE_FILTER_en[] = {
	TEXT("No Filter"),
	TEXT("LPFBW x1"),
	TEXT("LPFBW x2"),
	TEXT("LPFBW x3"),
	TEXT("LPFBW x4"),
	TEXT("LPF24-2 x1"),
	TEXT("LPF24-2 x2"),
	TEXT("LPF6 x8"),
	TEXT("LPF6 x16"),
	TEXT("LPF FIR"),
};

static const TCHAR *cb_info_IDC_COMBO_RESAMPLE_FILTER_jp[] = {
	TEXT("フィルタなし"),
	TEXT("LPFBW x1"),
	TEXT("LPFBW x2"),
	TEXT("LPFBW x3"),
	TEXT("LPFBW x4"),
	TEXT("LPF24-2 x1"),
	TEXT("LPF24-2 x2"),
	TEXT("LPF6 x8"),
	TEXT("LPF6 x16"),
	TEXT("LPF FIR"),
};

// IDC_COMBO_RESAMPLE_OVER_SAMPLING
// optimize
static int cb_info_IDC_COMBO_RESAMPLE_OVER_SAMPLING_num[] = {
	0,
	2,
	4,
	8,
	16,
};

static const TCHAR *cb_info_IDC_COMBO_RESAMPLE_OVER_SAMPLING[] = {
	TEXT("OFF"),
	TEXT("x2"),
	TEXT("x4"),
	TEXT("x8"),
	TEXT("x16"),
};

static LRESULT APIENTRY
PrefTiMidity1DialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	static int initflag = 1; 
	int i, tmp;
	const TCHAR **cb_info;
	switch (uMess){
	case WM_INITDIALOG:
		// MIDI effect
		// MIDI SYSTEM
		if (CurrentPlayerLanguage == LANGUAGE_JAPANESE)
			cb_info = cb_info_IDC_COMBO_MIDI_TYPE_jp;
		else 
			cb_info = cb_info_IDC_COMBO_MIDI_TYPE_en;
		for (i = 0; i < cb_num_IDC_COMBO_MIDI_TYPE; i++)
			CB_INSSTR(IDC_COMBO_MIDI_TYPE, cb_info[i]);
		if(st_temp->opt_system_mid)
			CB_SET(IDC_COMBO_MIDI_TYPE, CB_FIND(cb_info_IDC_COMBO_MIDI_TYPE_num, st_temp->opt_system_mid + SYSTEM_MID_OFFSET, 0));
		else
			CB_SET(IDC_COMBO_MIDI_TYPE, CB_FIND(cb_info_IDC_COMBO_MIDI_TYPE_num, st_temp->opt_default_mid, 0));

		// MODULE
		for (i = 0; i < module_list_num; i++)
			CB_INSSTR(IDC_COMBO_MODULE, module_list[i].name);
		CB_SET(IDC_COMBO_MODULE, cb_find_module(st_temp->opt_default_module));

		// REVERB
		if (CurrentPlayerLanguage == LANGUAGE_JAPANESE)
			cb_info = cb_info_IDC_COMBO_REVERB_jp;
		else 
			cb_info = cb_info_IDC_COMBO_REVERB_en;
		for (i = 0; i < cb_num_IDC_COMBO_REVERB; i++)
			SendDlgItemMessage(hwnd, IDC_COMBO_REVERB,CB_INSERTSTRING, (WPARAM) -1,	(LPARAM) cb_info[i]);

		SendDlgItemMessage(hwnd, IDC_SLIDER_REVERB, TBM_SETRANGEMAX, (WPARAM) 0, (LPARAM) sl_max_REVERB );
		SendDlgItemMessage(hwnd, IDC_SLIDER_REVERB, TBM_SETRANGEMIN, (WPARAM) 0, (LPARAM) sl_min_REVERB );

		if (st_temp->opt_reverb_control >= 0) {
			SendDlgItemMessage(hwnd, IDC_COMBO_REVERB, CB_SETCURSEL, (WPARAM) st_temp->opt_reverb_control, (LPARAM) 0);
			SendDlgItemMessage(hwnd, IDC_CHECKBOX_REVERB_LEVEL, BM_SETCHECK, 0, 0);
			SendDlgItemMessage(hwnd, IDC_SLIDER_REVERB, TBM_SETPOS, (WPARAM) 1, (LPARAM) 0 );
			SetDlgItemInt(hwnd, IDC_EDIT_REVERB, 0, FALSE);
			EnableWindow(GetDlgItem(hwnd, IDC_SLIDER_REVERB), FALSE);
			EnableWindow(GetDlgItem(hwnd, IDC_EDIT_REVERB), FALSE);
		} else {
			SendDlgItemMessage(hwnd, IDC_COMBO_REVERB, CB_SETCURSEL, (WPARAM) ((-st_temp->opt_reverb_control) / 128 + 1), (LPARAM) 0);
			SendDlgItemMessage(hwnd, IDC_CHECKBOX_REVERB_LEVEL, BM_SETCHECK, 1, 0);
			SendDlgItemMessage(hwnd, IDC_SLIDER_REVERB, TBM_SETPOS, (WPARAM) 1, (LPARAM) (-st_temp->opt_reverb_control) % 128 );
			SetDlgItemInt(hwnd, IDC_EDIT_REVERB, (-st_temp->opt_reverb_control) % 128, TRUE);
			EnableWindow(GetDlgItem(hwnd, IDC_SLIDER_REVERB), TRUE);
			EnableWindow(GetDlgItem(hwnd, IDC_EDIT_REVERB), TRUE);
		}

		// CHORUS
		if (CurrentPlayerLanguage == LANGUAGE_JAPANESE)
			cb_info = cb_info_IDC_COMBO_CHORUS_jp;
		else 
			cb_info = cb_info_IDC_COMBO_CHORUS_en;
		for (i = 0; i < cb_num_IDC_COMBO_CHORUS; i++)
			SendDlgItemMessage(hwnd, IDC_COMBO_CHORUS, CB_INSERTSTRING, (WPARAM) -1, (LPARAM) cb_info[i]);
		SendDlgItemMessage(hwnd, IDC_SLIDER_CHORUS, TBM_SETRANGEMAX, (WPARAM) 0, (LPARAM) sl_max_CHORUS );
		SendDlgItemMessage(hwnd, IDC_SLIDER_CHORUS, TBM_SETRANGEMIN, (WPARAM) 0, (LPARAM) sl_min_CHORUS );
		if (st_temp->opt_chorus_control >= 0) {
			if (st_temp->opt_normal_chorus_plus)
				i = st_temp->opt_normal_chorus_plus + 1;
			else 
				i = st_temp->opt_chorus_control;
			SendDlgItemMessage(hwnd, IDC_COMBO_CHORUS, CB_SETCURSEL, (WPARAM) i, (LPARAM) 0);
			SendDlgItemMessage(hwnd, IDC_CHECKBOX_CHORUS_LEVEL, BM_SETCHECK, 0, 0);
			SendDlgItemMessage(hwnd, IDC_SLIDER_CHORUS, TBM_SETPOS, (WPARAM) 1, (LPARAM) 0);
			SetDlgItemInt(hwnd,IDC_EDIT_CHORUS,0,FALSE);
			EnableWindow(GetDlgItem(hwnd, IDC_SLIDER_CHORUS), FALSE);
			EnableWindow(GetDlgItem(hwnd, IDC_EDIT_CHORUS), FALSE);
		} else {
			i = 1;
			if (st_temp->opt_normal_chorus_plus)
				i = st_temp->opt_normal_chorus_plus + 1;
			else
				i = 1;
			SendDlgItemMessage(hwnd, IDC_COMBO_CHORUS, CB_SETCURSEL, (WPARAM) i, (LPARAM) 0);
			SendDlgItemMessage(hwnd, IDC_CHECKBOX_CHORUS_LEVEL, BM_SETCHECK, 1, 0);
			SendDlgItemMessage(hwnd, IDC_SLIDER_CHORUS, TBM_SETPOS, (WPARAM) 1, (LPARAM) -i);
			SetDlgItemInt(hwnd, IDC_EDIT_CHORUS, -st_temp->opt_chorus_control, TRUE);
			EnableWindow(GetDlgItem(hwnd, IDC_SLIDER_CHORUS), TRUE);
			EnableWindow(GetDlgItem(hwnd, IDC_EDIT_CHORUS), TRUE);
		}

		// DELAY
		if (CurrentPlayerLanguage == LANGUAGE_JAPANESE)
			cb_info = cb_info_IDC_COMBO_DELAY_jp;
		else 
			cb_info = cb_info_IDC_COMBO_DELAY_en;

		for (i = 0; i < cb_num_IDC_COMBO_DELAY; i++)
			SendDlgItemMessage(hwnd, IDC_COMBO_DELAY,
					CB_INSERTSTRING, (WPARAM) -1,
					(LPARAM) cb_info[i]);

		SendDlgItemMessage(hwnd, IDC_COMBO_DELAY, CB_SETCURSEL,
				(WPARAM) st_temp->opt_delay_control, (LPARAM) 0);
		// LPF
		if (CurrentPlayerLanguage == LANGUAGE_JAPANESE)
			cb_info = cb_info_IDC_COMBO_LPF_jp;
		else 
			cb_info = cb_info_IDC_COMBO_LPF_en;
		for (i = 0; i < cb_num_IDC_COMBO_LPF; i++)
			CB_INSSTR(IDC_COMBO_LPF, (LPARAM) cb_info[i]);
		CB_SET(IDC_COMBO_LPF, (WPARAM) st_temp->opt_lpf_def);
		// HPF
		if (CurrentPlayerLanguage == LANGUAGE_JAPANESE)
			cb_info = cb_info_IDC_COMBO_HPF_jp;
		else 
			cb_info = cb_info_IDC_COMBO_HPF_en;
		for (i = 0; i < cb_num_IDC_COMBO_HPF; i++)
			CB_INSSTR(IDC_COMBO_HPF, (LPARAM) cb_info[i]);
		CB_SET(IDC_COMBO_HPF, (WPARAM) st_temp->opt_hpf_def);
		
		// MOD_ENV
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_MOD_ENV,st_temp->opt_modulation_envelope);
	 // Misc
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_MODWHEEL,st_temp->opt_modulation_wheel);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_PORTAMENTO,st_temp->opt_portamento);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CHPRESS,st_temp->opt_channel_pressure);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_DRUM_EFFECT,st_temp->opt_drum_effect);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_NRPNVIB,st_temp->opt_nrpn_vibrato);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_TVAA,st_temp->opt_tva_attack);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_TVAD,st_temp->opt_tva_decay);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_TVAR,st_temp->opt_tva_release);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_EQ,st_temp->opt_eq_control);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_IEFFECT,st_temp->opt_insertion_effect);
///r
		// OVOICE
		// CUT_SHORT_TIME
		for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_OVOICE_num); i++)
			CB_INSSTR(IDC_COMBO_OVOICE,	cb_info_IDC_COMBO_OVOICE[i]);
		for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_CUT_SHORT_TIME_num); i++)
			CB_INSSTR(IDC_COMBO_CUT_SHORT_TIME,cb_info_IDC_COMBO_CUT_SHORT_TIME[i]);

		if(!st_temp->opt_overlap_voice_allow){
			st_temp->opt_overlap_voice_count = 0;
			CB_SET(IDC_COMBO_OVOICE, 0);
			st_temp->opt_cut_short_time = 0;
			CB_SET(IDC_COMBO_CUT_SHORT_TIME, 0);
		}else{
			CB_SET(IDC_COMBO_OVOICE, CB_FIND(cb_info_IDC_COMBO_OVOICE_num, st_temp->opt_overlap_voice_count, 6));
			CB_SET(IDC_COMBO_CUT_SHORT_TIME, CB_FIND(cb_info_IDC_COMBO_CUT_SHORT_TIME_num, st_temp->opt_cut_short_time, 0));
		}	
		// MAX_CHANNEL_VOICES
		for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_CHANNEL_VOICES_num); i++)
			CB_INSSTR(IDC_COMBO_CHANNEL_VOICES, cb_info_IDC_COMBO_CHANNEL_VOICES[i]);
		CB_SET(IDC_COMBO_CHANNEL_VOICES, CB_FIND(cb_info_IDC_COMBO_CHANNEL_VOICES_num, st_temp->opt_max_channel_voices, 1));

		// MIX_ENV
	//	DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_MIX_ENV,st_temp->opt_mix_envelope);
		for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_MIX_ENV_num); i++)
			CB_INSSTR(IDC_COMBO_MIX_ENV, cb_info_IDC_COMBO_MIX_ENV[i]);
		CB_SET(IDC_COMBO_MIX_ENV, CB_FIND(cb_info_IDC_COMBO_MIX_ENV_num, st_temp->opt_mix_envelope, 1));
		
		// MOD_UPDATE
		for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_MOD_UPDATE_num); i++)
			CB_INSSTR(IDC_COMBO_MOD_UPDATE, cb_info_IDC_COMBO_MOD_UPDATE[i]);
		CB_SET(IDC_COMBO_MOD_UPDATE, CB_FIND(cb_info_IDC_COMBO_MOD_UPDATE_num, st_temp->opt_modulation_update, 0));
		
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_TRACETEXT,st_temp->opt_trace_text_meta_event);

		SetDlgItemInt(hwnd,IDC_EDIT_MODIFY_RELEASE,st_temp->modify_release,TRUE);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_FASTPAN,st_temp->adjust_panning_immediately);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_FASTDECAY,st_temp->opt_fast_decay);
///r
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_DECAY, st_temp->min_sustain_time);
		EnableWindow(GetDlgItem(hwnd, IDC_EDIT_DECAY), st_temp->min_sustain_time);
		SetDlgItemInt(hwnd,IDC_EDIT_DECAY, st_temp->min_sustain_time,TRUE);
		// CONTROL RATIO
		EnableWindow(GetDlgItem(hwnd, IDC_COMBO_CONTROL_RATIO), FALSE);
		// LIMITER
		SendDlgItemMessage(hwnd, IDC_SLIDER_LIMITER, TBM_SETRANGEMAX, (WPARAM) 0, (LPARAM) sl_max_LIMITER );
		SendDlgItemMessage(hwnd, IDC_SLIDER_LIMITER, TBM_SETRANGEMIN, (WPARAM) 0, (LPARAM) sl_min_LIMITER );
		if (st_temp->opt_limiter > 0) { // on
			SendDlgItemMessage(hwnd, IDC_CHECKBOX_LIMITER, BM_SETCHECK, 1, 0);
			SendDlgItemMessage(hwnd, IDC_SLIDER_LIMITER, TBM_SETPOS, (WPARAM) 1, (LPARAM)st_temp->opt_limiter);
			SetDlgItemInt(hwnd, IDC_EDIT_LIMITER, st_temp->opt_limiter, TRUE);
			EnableWindow(GetDlgItem(hwnd, IDC_SLIDER_LIMITER), TRUE);
			EnableWindow(GetDlgItem(hwnd, IDC_EDIT_LIMITER), TRUE);
		} else {
			SendDlgItemMessage(hwnd, IDC_CHECKBOX_LIMITER, BM_SETCHECK, 0, 0);
			SendDlgItemMessage(hwnd, IDC_SLIDER_LIMITER, TBM_SETPOS, (WPARAM) 1, (LPARAM) 100);
			SetDlgItemInt(hwnd, IDC_EDIT_LIMITER, 0, FALSE);
			EnableWindow(GetDlgItem(hwnd, IDC_SLIDER_LIMITER), FALSE);
			EnableWindow(GetDlgItem(hwnd, IDC_EDIT_LIMITER), FALSE);
		}
		

		// play
		// VOICES
		for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_VOICES_num); i++)
			CB_INSSTR(IDC_COMBO_VOICES,	cb_info_IDC_COMBO_VOICES[i]);
		CB_SET(IDC_COMBO_VOICES, CB_FIND(cb_info_IDC_COMBO_VOICES_num, st_temp->voices , 7));
		
		SendDlgItemMessage(hwnd, IDC_SLIDER_OUTPUT_AMP, TBM_SETRANGEMAX, (WPARAM) 0, (LPARAM) sl_max_AMPLIFICATION );
		SendDlgItemMessage(hwnd, IDC_SLIDER_OUTPUT_AMP, TBM_SETRANGEMIN, (WPARAM) 0, (LPARAM) sl_min_AMPLIFICATION );
		SendDlgItemMessage(hwnd, IDC_SLIDER_OUTPUT_AMP, TBM_SETPOS, (WPARAM) 1, (LPARAM) st_temp->output_amplification );
		SetDlgItemInt(hwnd,IDC_EDIT_OUTPUT_AMP,st_temp->output_amplification,FALSE);

		//SendDlgItemMessage(hwnd, IDC_SLIDER_AMPLIFICATION, TBM_SETRANGEMAX, (WPARAM) 0, (LPARAM) sl_max_AMPLIFICATION );
		//SendDlgItemMessage(hwnd, IDC_SLIDER_AMPLIFICATION, TBM_SETRANGEMIN, (WPARAM) 0, (LPARAM) sl_min_AMPLIFICATION );
		//SendDlgItemMessage(hwnd, IDC_SLIDER_AMPLIFICATION, TBM_SETPOS, (WPARAM) 1, (LPARAM) st_temp->amplification );
		SetDlgItemInt(hwnd,IDC_EDIT_AMPLIFICATION,st_temp->amplification,FALSE);
				
		//SendDlgItemMessage(hwnd, IDC_SLIDER_DRUM_POWER, TBM_SETRANGEMAX, (WPARAM) 0, (LPARAM) sl_max_DRUM_POWER );
		//SendDlgItemMessage(hwnd, IDC_SLIDER_DRUM_POWER, TBM_SETRANGEMIN, (WPARAM) 0, (LPARAM) sl_min_DRUM_POWER );
		//SendDlgItemMessage(hwnd, IDC_SLIDER_DRUM_POWER, TBM_SETPOS, (WPARAM) 1, (LPARAM) st_temp->opt_drum_power );
		SetDlgItemInt(hwnd,IDC_EDIT_DRUM_POWER,st_temp->opt_drum_power,FALSE);
		
		SetDlgItemFloat(hwnd, IDC_EDIT_VOLUME_CURVE, st_temp->opt_user_volume_curve);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_AMP_COMPENSATION,st_temp->opt_amp_compensation);
///r
#if defined(IA_W32G_SYN) || defined(WINDRV_SETUP)
		EnableWindow(GetDlgItem(hwnd, IDC_COMBO_ADD_PLAY_TIME), FALSE);
		EnableWindow(GetDlgItem(hwnd, IDC_COMBO_ADD_SILENT_TIME), FALSE);
		EnableWindow(GetDlgItem(hwnd, IDC_COMBO_EMU_DELAY_TIME), FALSE);
#else
		for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_ADD_PLAY_TIME_num); i++)
			CB_INSSTR(IDC_COMBO_ADD_PLAY_TIME,cb_info_IDC_COMBO_ADD_PLAY_TIME[i]);
		CB_SET(IDC_COMBO_ADD_PLAY_TIME, CB_FIND(cb_info_IDC_COMBO_ADD_PLAY_TIME_num, st_temp->add_play_time, 1));
		for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_ADD_SILENT_TIME_num); i++)
			CB_INSSTR(IDC_COMBO_ADD_SILENT_TIME,cb_info_IDC_COMBO_ADD_SILENT_TIME[i]);
		CB_SET(IDC_COMBO_ADD_SILENT_TIME, CB_FIND(cb_info_IDC_COMBO_ADD_SILENT_TIME_num, st_temp->add_silent_time, 1));
		for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_EMU_DELAY_TIME_num); i++)
			CB_INSSTR(IDC_COMBO_EMU_DELAY_TIME,cb_info_IDC_COMBO_EMU_DELAY_TIME[i]);
		CB_SET(IDC_COMBO_EMU_DELAY_TIME, CB_FIND(cb_info_IDC_COMBO_EMU_DELAY_TIME_num, st_temp->emu_delay_time, 5));
#endif
		// sample tuning
		// ANTIALIAS
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_ANTIALIAS,st_temp->antialiasing_allowed);
		// NOISESHAPING
		if (CurrentPlayerLanguage == LANGUAGE_JAPANESE)
			cb_info = cb_info_IDC_COMBO_NOISESHARPING_jp;
		else 
			cb_info = cb_info_IDC_COMBO_NOISESHARPING_en;
		for (i = 0; i < cb_num_IDC_COMBO_NOISESHARPING; i++)
			SendDlgItemMessage(hwnd, IDC_COMBO_NOISESHARPING, CB_INSERTSTRING, (WPARAM) -1, (LPARAM) cb_info[i]);
		SendDlgItemMessage(hwnd, IDC_COMBO_NOISESHARPING, CB_SETCURSEL, (WPARAM) st_temp->noise_sharp_type, (LPARAM) 0);
		// RESAMPLE
		if (CurrentPlayerLanguage == LANGUAGE_JAPANESE)
			cb_info = cb_info_IDC_COMBO_RESAMPLE_jp;
		else 
			cb_info = cb_info_IDC_COMBO_RESAMPLE_en;
		for (i = 0; i < cb_num_IDC_COMBO_RESAMPLE; i++)
			SendDlgItemMessage(hwnd, IDC_COMBO_RESAMPLE, CB_INSERTSTRING, (WPARAM) -1, (LPARAM) cb_info[i]);
		CB_SET(IDC_COMBO_RESAMPLE, (WPARAM) st_temp->opt_resample_type);
		for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_RESAMPLE_PARAM_num); i++)
			CB_INSSTR(IDC_COMBO_RESAMPLE_PARAM, cb_info_IDC_COMBO_RESAMPLE_PARAM[i]);
		CB_SET(IDC_COMBO_RESAMPLE_PARAM, CB_FIND(cb_info_IDC_COMBO_RESAMPLE_PARAM_num,st_temp->opt_resample_param ,0));

		if (CurrentPlayerLanguage == LANGUAGE_JAPANESE)
			cb_info = cb_info_IDC_COMBO_RESAMPLE_FILTER_jp;
		else 
			cb_info = cb_info_IDC_COMBO_RESAMPLE_FILTER_en;
		for (i = 0; i < cb_num_IDC_COMBO_RESAMPLE_FILTER; i++)
			SendDlgItemMessage(hwnd, IDC_COMBO_RESAMPLE_FILTER, CB_INSERTSTRING, (WPARAM) -1, (LPARAM) cb_info[i]);
		CB_SET(IDC_COMBO_RESAMPLE_FILTER, (WPARAM) st_temp->opt_resample_filter);
		
		for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_RESAMPLE_OVER_SAMPLING_num); i++)
			CB_INSSTR(IDC_COMBO_RESAMPLE_OVER_SAMPLING, cb_info_IDC_COMBO_RESAMPLE_OVER_SAMPLING[i]);
		CB_SET(IDC_COMBO_RESAMPLE_OVER_SAMPLING, CB_FIND(cb_info_IDC_COMBO_RESAMPLE_OVER_SAMPLING_num, st_temp->opt_resample_over_sampling, 1));

		initflag = 0;
		break;
	case WM_HSCROLL:
	case WM_VSCROLL:
			tmp = SendDlgItemMessage(hwnd, IDC_SLIDER_LIMITER, TBM_GETPOS, (WPARAM) 0, (LPARAM)0);
			SetDlgItemInt(hwnd, IDC_EDIT_LIMITER, tmp, FALSE);
			tmp = SendDlgItemMessage(hwnd, IDC_SLIDER_REVERB, TBM_GETPOS, (WPARAM) 0, (LPARAM)0);
			SetDlgItemInt(hwnd, IDC_EDIT_REVERB, tmp, FALSE);
			tmp = SendDlgItemMessage(hwnd, IDC_SLIDER_CHORUS, TBM_GETPOS, (WPARAM) 0, (LPARAM)0);
			SetDlgItemInt(hwnd, IDC_EDIT_CHORUS, tmp, FALSE);
			tmp = SendDlgItemMessage(hwnd, IDC_SLIDER_OUTPUT_AMP, TBM_GETPOS, (WPARAM) 0, (LPARAM)0);
			SetDlgItemInt(hwnd, IDC_EDIT_OUTPUT_AMP, tmp, FALSE);
			//tmp = SendDlgItemMessage(hwnd, IDC_SLIDER_AMPLIFICATION, TBM_GETPOS, (WPARAM) 0, (LPARAM)0);
			//SetDlgItemInt(hwnd, IDC_EDIT_AMPLIFICATION, tmp, FALSE);
			//tmp = SendDlgItemMessage(hwnd, IDC_SLIDER_DRUM_POWER, TBM_GETPOS, (WPARAM) 0, (LPARAM)0);
			//SetDlgItemInt(hwnd, IDC_EDIT_DRUM_POWER, tmp, FALSE);
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCLOSE:
			break;
		case IDC_CHECKBOX_CHORUS_LEVEL:
			if(SendDlgItemMessage(hwnd, IDC_CHECKBOX_CHORUS_LEVEL, BM_GETCHECK, 0, 0)){
				EnableWindow(GetDlgItem(hwnd, IDC_SLIDER_CHORUS), TRUE);
				EnableWindow(GetDlgItem(hwnd, IDC_EDIT_CHORUS), TRUE);
			} else {
				EnableWindow(GetDlgItem(hwnd, IDC_SLIDER_CHORUS), FALSE);
				EnableWindow(GetDlgItem(hwnd, IDC_EDIT_CHORUS), FALSE);
			}
			break;
		case IDC_CHECKBOX_REVERB_LEVEL:
			if(SendDlgItemMessage(hwnd, IDC_CHECKBOX_REVERB_LEVEL, BM_GETCHECK, 0, 0)){
				EnableWindow(GetDlgItem(hwnd, IDC_SLIDER_REVERB), TRUE);
				EnableWindow(GetDlgItem(hwnd, IDC_EDIT_REVERB), TRUE);
			} else {
				EnableWindow(GetDlgItem(hwnd, IDC_SLIDER_REVERB), FALSE);
				EnableWindow(GetDlgItem(hwnd, IDC_EDIT_REVERB), FALSE);
			}
			break;
		case IDC_COMBO_OVOICE:
			if(!(tmp = CB_GETS(IDC_COMBO_OVOICE, 0)))
				CB_SET(IDC_COMBO_CUT_SHORT_TIME, 0);
			break;
		case IDC_CHECKBOX_DECAY:
			if(SendDlgItemMessage(hwnd, IDC_CHECKBOX_DECAY, BM_GETCHECK, 0, 0)){
				SetDlgItemInt(hwnd,IDC_EDIT_DECAY,5000,TRUE);
				EnableWindow(GetDlgItem(hwnd, IDC_EDIT_DECAY), TRUE);
			}else{
				SetDlgItemInt(hwnd,IDC_EDIT_DECAY,0,TRUE);
				EnableWindow(GetDlgItem(hwnd, IDC_EDIT_DECAY), FALSE);
			}
			break;
		case IDC_CHECKBOX_LIMITER:
			if(SendDlgItemMessage(hwnd, IDC_CHECKBOX_LIMITER, BM_GETCHECK, 0, 0)){
				EnableWindow(GetDlgItem(hwnd, IDC_SLIDER_LIMITER), TRUE);
				EnableWindow(GetDlgItem(hwnd, IDC_EDIT_LIMITER), TRUE);
			} else {
				EnableWindow(GetDlgItem(hwnd, IDC_SLIDER_LIMITER), FALSE);
				EnableWindow(GetDlgItem(hwnd, IDC_EDIT_LIMITER), FALSE);
			}
			break;
		default:
			PrefWndSetOK = 1;
			return FALSE;
		break;	
		}
		PrefWndSetOK = 1;
		break;
	case WM_MYSAVE:
	{
		int flag;
		if ( initflag ) break;
		// MIDI effect
		// DEFAULT MIDI TYPE
		tmp = cb_info_IDC_COMBO_MIDI_TYPE_num[CB_GETS(IDC_COMBO_MIDI_TYPE, 0)];
		if(tmp >= SYSTEM_MID_OFFSET){
			tmp -= SYSTEM_MID_OFFSET;
			st_temp->opt_system_mid = tmp;
			st_temp->opt_default_mid = tmp;
		}else{
			st_temp->opt_system_mid = 0;
			st_temp->opt_default_mid = tmp;
		}

		// DEFAULT MODULE
		st_temp->opt_default_module = module_list[CB_GETS(IDC_COMBO_MODULE, 0)].num;

  		// REVERB
		st_temp->opt_reverb_control = (int)SendDlgItemMessage(hwnd, IDC_COMBO_REVERB, CB_GETCURSEL, 0, 0);
		if(st_temp->opt_reverb_control && SendDlgItemMessage(hwnd, IDC_CHECKBOX_REVERB_LEVEL, BM_GETCHECK, 0, 0)) {
			st_temp->opt_reverb_control = -(int)GetDlgItemInt(hwnd,IDC_EDIT_REVERB,NULL,TRUE) - (st_temp->opt_reverb_control - 1) * 128;
		}

		// CHORUS
		tmp = (int)SendDlgItemMessage(hwnd, IDC_COMBO_CHORUS, CB_GETCURSEL, 0, 0);
		if(!tmp || tmp < 0){
			st_temp->opt_normal_chorus_plus = 0;
			st_temp->opt_chorus_control = 0;
		}else{
			st_temp->opt_normal_chorus_plus = tmp - 1;
			st_temp->opt_chorus_control = 1;
		}
		if (st_temp->opt_chorus_control && SendDlgItemMessage(hwnd, IDC_CHECKBOX_CHORUS_LEVEL, BM_GETCHECK, 0, 0)) {
			st_temp->opt_chorus_control = -(int)GetDlgItemInt(hwnd,IDC_EDIT_CHORUS,NULL,TRUE);
 		}

		// DELAY
		st_temp->opt_delay_control = (int)SendDlgItemMessage(hwnd, IDC_COMBO_DELAY, CB_GETCURSEL, 0, 0);
		// LPF
		st_temp->opt_lpf_def = (int)SendDlgItemMessage(hwnd, IDC_COMBO_LPF, CB_GETCURSEL, 0, 0);		
		// HPF
		st_temp->opt_hpf_def = (int)SendDlgItemMessage(hwnd, IDC_COMBO_HPF, CB_GETCURSEL, 0, 0);		
		// MOD_ENV
		DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_MOD_ENV,st_temp->opt_modulation_envelope);
		// misc
		DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_MODWHEEL,st_temp->opt_modulation_wheel);
		DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_PORTAMENTO,st_temp->opt_portamento);
		DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_CHPRESS,st_temp->opt_channel_pressure);
		DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_DRUM_EFFECT,st_temp->opt_drum_effect);
		DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_NRPNVIB,st_temp->opt_nrpn_vibrato);
		DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_TVAA,st_temp->opt_tva_attack);
		DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_TVAD,st_temp->opt_tva_decay);
		DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_TVAR,st_temp->opt_tva_release);
		DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_EQ,st_temp->opt_eq_control);
		DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_IEFFECT,st_temp->opt_insertion_effect);
///r
		// special
		// OVOICE
		if(!(tmp = CB_GETS(IDC_COMBO_OVOICE, 6))){
			st_temp->opt_overlap_voice_allow = 0;
			st_temp->opt_overlap_voice_count = 0;
			st_temp->opt_cut_short_time = 0;
		}else{
			st_temp->opt_overlap_voice_allow = 1;
			st_temp->opt_overlap_voice_count = cb_info_IDC_COMBO_OVOICE_num[tmp];
			st_temp->opt_cut_short_time = cb_info_IDC_COMBO_CUT_SHORT_TIME_num[CB_GETS(IDC_COMBO_CUT_SHORT_TIME, 0)];
		}
		// MAX_CHANNEL_VOICES
		st_temp->opt_max_channel_voices = cb_info_IDC_COMBO_CHANNEL_VOICES_num[CB_GETS(IDC_COMBO_CHANNEL_VOICES, 1)];
		
		// MIX_ENV
	//	DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_MIX_ENV,st_temp->opt_mix_envelope);
		st_temp->opt_mix_envelope = cb_info_IDC_COMBO_MIX_ENV_num[CB_GETS(IDC_COMBO_MIX_ENV, 1)];
		
		// MOD_UPDATE
		st_temp->opt_modulation_update = cb_info_IDC_COMBO_MOD_UPDATE_num[CB_GETS(IDC_COMBO_MOD_UPDATE, 0)];

		DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_TRACETEXT,st_temp->opt_trace_text_meta_event);

		st_temp->modify_release = GetDlgItemInt(hwnd,IDC_EDIT_MODIFY_RELEASE,NULL,FALSE);
		DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_FASTPAN,st_temp->adjust_panning_immediately);
		DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_FASTDECAY,st_temp->opt_fast_decay);
 ///r
		DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_DECAY, flag);
		st_temp->min_sustain_time = !flag ? 0 : EB_GET_INT(IDC_EDIT_DECAY);
		// CONTROL RATIO
		st_temp->control_ratio = 0;
		// LIMITER
		DLG_CHECKBUTTON_TO_FLAG(hwnd, IDC_CHECKBOX_LIMITER, flag);
		st_temp->opt_limiter = !flag ? 0 : EB_GET_INT(IDC_EDIT_LIMITER);
	
		//play
		// Maximum voices
		st_temp->voices = GetDlgItemInt(hwnd,IDC_COMBO_VOICES,NULL,FALSE);

		RANGE(st_temp->voices, 1, MAX_VOICES); /* 1..1024 */
		if (st_temp->voices > max_voices) {
			free_voice_pointer();
			max_voices = st_temp->voices;
			init_voice_pointer();
		}
		
		st_temp->output_amplification = GetDlgItemInt(hwnd,IDC_EDIT_OUTPUT_AMP,NULL,FALSE);
		st_temp->amplification = GetDlgItemInt(hwnd,IDC_EDIT_AMPLIFICATION,NULL,FALSE);
		st_temp->opt_drum_power = GetDlgItemInt(hwnd,IDC_EDIT_DRUM_POWER,NULL,FALSE);
		st_temp->opt_user_volume_curve = GetDlgItemFloat(hwnd, IDC_EDIT_VOLUME_CURVE);
		DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_AMP_COMPENSATION,st_temp->opt_amp_compensation);

///r
#if !(defined(IA_W32G_SYN) || defined(WINDRV_SETUP))
		st_temp->add_play_time = cb_info_IDC_COMBO_ADD_PLAY_TIME_num[CB_GETS(IDC_COMBO_ADD_PLAY_TIME, 1)];
		st_temp->add_silent_time = cb_info_IDC_COMBO_ADD_SILENT_TIME_num[CB_GETS(IDC_COMBO_ADD_SILENT_TIME, 1)];
		st_temp->emu_delay_time = cb_info_IDC_COMBO_EMU_DELAY_TIME_num[CB_GETS(IDC_COMBO_EMU_DELAY_TIME, 5)];
#endif

		// hoge
		// ANTIALIAS
		DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_ANTIALIAS,st_temp->antialiasing_allowed);
		// NOISESHARPING
		st_temp->noise_sharp_type = SendDlgItemMessage(hwnd, IDC_COMBO_NOISESHARPING, CB_GETCURSEL, (WPARAM) 0, (LPARAM) 0);
		// RESAMPLE
		st_temp->opt_resample_type = SendDlgItemMessage(hwnd, IDC_COMBO_RESAMPLE, CB_GETCURSEL, (WPARAM) 0, (LPARAM) 0);
		st_temp->opt_resample_param = GetDlgItemInt(hwnd,IDC_COMBO_RESAMPLE_PARAM,NULL,TRUE);
		st_temp->opt_resample_filter = SendDlgItemMessage(hwnd, IDC_COMBO_RESAMPLE_FILTER, CB_GETCURSEL, (WPARAM) 0, (LPARAM) 0);
		st_temp->opt_resample_over_sampling = cb_info_IDC_COMBO_RESAMPLE_OVER_SAMPLING_num[CB_GETS(IDC_COMBO_RESAMPLE_OVER_SAMPLING, 1)];
	}
		SetWindowLongPtr(hwnd,DWLP_MSGRESULT,FALSE);
		break;
	case WM_SIZE:
		return FALSE;
	case WM_DESTROY:
		break;
	default:
	  break;
	}
	return FALSE;
}



// BANK // PROGRAM // CHANNEL
#define num_BANK_PROGRAM 129 // off 0-127
static const TCHAR *info_BANK_PROGRAM[] = {
	TEXT("OFF"),
	TEXT("0"),TEXT("1"),TEXT("2"),TEXT("3"),TEXT("4"),TEXT("5"),TEXT("6"),TEXT("7"),TEXT("8"),
	TEXT("9"),TEXT("10"),TEXT("11"),TEXT("12"),TEXT("13"),TEXT("14"),TEXT("15"),TEXT("16"),
	TEXT("17"),TEXT("18"),TEXT("19"),TEXT("20"),TEXT("21"),TEXT("22"),TEXT("23"),TEXT("24"),
	TEXT("25"),TEXT("26"),TEXT("27"),TEXT("28"),TEXT("29"),TEXT("30"),TEXT("31"),TEXT("32"),
	TEXT("33"),TEXT("34"),TEXT("35"),TEXT("36"),TEXT("37"),TEXT("38"),TEXT("39"),TEXT("40"),
	TEXT("41"),TEXT("42"),TEXT("43"),TEXT("44"),TEXT("45"),TEXT("46"),TEXT("47"),TEXT("48"),
	TEXT("49"),TEXT("50"),TEXT("51"),TEXT("52"),TEXT("53"),TEXT("54"),TEXT("55"),TEXT("56"),
	TEXT("57"),TEXT("58"),TEXT("59"),TEXT("60"),TEXT("61"),TEXT("62"),TEXT("63"),TEXT("64"),
	TEXT("65"),TEXT("66"),TEXT("67"),TEXT("68"),TEXT("69"),TEXT("70"),TEXT("71"),TEXT("72"),
	TEXT("73"),TEXT("74"),TEXT("75"),TEXT("76"),TEXT("77"),TEXT("78"),TEXT("79"),TEXT("80"),
	TEXT("81"),TEXT("82"),TEXT("83"),TEXT("84"),TEXT("85"),TEXT("86"),TEXT("87"),TEXT("88"),
	TEXT("89"),TEXT("90"),TEXT("91"),TEXT("92"),TEXT("93"),TEXT("94"),TEXT("95"),TEXT("96"),
	TEXT("97"),TEXT("98"),TEXT("99"),TEXT("100"),TEXT("101"),TEXT("102"),TEXT("103"),TEXT("104"),
	TEXT("105"),TEXT("106"),TEXT("107"),TEXT("108"),TEXT("109"),TEXT("110"),TEXT("111"),TEXT("112"),
	TEXT("113"),TEXT("114"),TEXT("115"),TEXT("116"),TEXT("117"),TEXT("118"),TEXT("119"),TEXT("120"),
	TEXT("121"),TEXT("122"),TEXT("123"),TEXT("124"),TEXT("125"),TEXT("126"),TEXT("127"),TEXT("128"),
};
static const TCHAR *info_CHANNEL[] = {
	TEXT("ch001"),TEXT("ch002"),TEXT("ch003"),TEXT("ch004"),TEXT("ch005"),TEXT("ch006"),TEXT("ch007"),TEXT("ch008"),
	TEXT("ch009"),TEXT("ch010"),TEXT("ch011"),TEXT("ch012"),TEXT("ch013"),TEXT("ch014"),TEXT("ch015"),TEXT("ch016"),
	TEXT("ch017"),TEXT("ch018"),TEXT("ch019"),TEXT("ch020"),TEXT("ch021"),TEXT("ch022"),TEXT("ch023"),TEXT("ch024"),
	TEXT("ch025"),TEXT("ch026"),TEXT("ch027"),TEXT("ch028"),TEXT("ch029"),TEXT("ch030"),TEXT("ch031"),TEXT("ch032"),
	TEXT("ch033"),TEXT("ch034"),TEXT("ch035"),TEXT("ch036"),TEXT("ch037"),TEXT("ch038"),TEXT("ch039"),TEXT("ch040"),
	TEXT("ch041"),TEXT("ch042"),TEXT("ch043"),TEXT("ch044"),TEXT("ch045"),TEXT("ch046"),TEXT("ch047"),TEXT("ch048"),
	TEXT("ch049"),TEXT("ch050"),TEXT("ch051"),TEXT("ch052"),TEXT("ch053"),TEXT("ch054"),TEXT("ch055"),TEXT("ch056"),
	TEXT("ch057"),TEXT("ch058"),TEXT("ch059"),TEXT("ch060"),TEXT("ch061"),TEXT("ch062"),TEXT("ch063"),TEXT("ch064"),
	TEXT("ch065"),TEXT("ch066"),TEXT("ch067"),TEXT("ch068"),TEXT("ch069"),TEXT("ch070"),TEXT("ch071"),TEXT("ch072"),
	TEXT("ch073"),TEXT("ch074"),TEXT("ch075"),TEXT("ch076"),TEXT("ch077"),TEXT("ch078"),TEXT("ch079"),TEXT("ch080"),
	TEXT("ch081"),TEXT("ch082"),TEXT("ch083"),TEXT("ch084"),TEXT("ch085"),TEXT("ch086"),TEXT("ch087"),TEXT("ch088"),
	TEXT("ch089"),TEXT("ch090"),TEXT("ch091"),TEXT("ch092"),TEXT("ch093"),TEXT("ch094"),TEXT("ch095"),TEXT("ch096"),
	TEXT("ch097"),TEXT("ch098"),TEXT("ch099"),TEXT("ch100"),TEXT("ch101"),TEXT("ch102"),TEXT("ch103"),TEXT("ch104"),
	TEXT("ch105"),TEXT("ch106"),TEXT("ch107"),TEXT("ch108"),TEXT("ch109"),TEXT("ch110"),TEXT("ch111"),TEXT("ch112"),
	TEXT("ch113"),TEXT("ch114"),TEXT("ch115"),TEXT("ch116"),TEXT("ch117"),TEXT("ch118"),TEXT("ch119"),TEXT("ch120"),
	TEXT("ch121"),TEXT("ch122"),TEXT("ch123"),TEXT("ch124"),TEXT("ch125"),TEXT("ch126"),TEXT("ch127"),TEXT("ch128"),
	TEXT("ch129"),TEXT("ch130"),TEXT("ch131"),TEXT("ch132"),TEXT("ch133"),TEXT("ch134"),TEXT("ch135"),TEXT("ch136"),
	TEXT("ch137"),TEXT("ch138"),TEXT("ch139"),TEXT("ch140"),TEXT("ch141"),TEXT("ch142"),TEXT("ch143"),TEXT("ch144"),
	TEXT("ch145"),TEXT("ch146"),TEXT("ch147"),TEXT("ch148"),TEXT("ch149"),TEXT("ch150"),TEXT("ch151"),TEXT("ch152"),
	TEXT("ch153"),TEXT("ch154"),TEXT("ch155"),TEXT("ch156"),TEXT("ch157"),TEXT("ch158"),TEXT("ch159"),TEXT("ch160"),
	TEXT("ch161"),TEXT("ch162"),TEXT("ch163"),TEXT("ch164"),TEXT("ch165"),TEXT("ch166"),TEXT("ch167"),TEXT("ch168"),
	TEXT("ch169"),TEXT("ch170"),TEXT("ch171"),TEXT("ch172"),TEXT("ch173"),TEXT("ch174"),TEXT("ch175"),TEXT("ch176"),
	TEXT("ch177"),TEXT("ch178"),TEXT("ch179"),TEXT("ch180"),TEXT("ch181"),TEXT("ch182"),TEXT("ch183"),TEXT("ch184"),
	TEXT("ch185"),TEXT("ch186"),TEXT("ch187"),TEXT("ch188"),TEXT("ch189"),TEXT("ch190"),TEXT("ch191"),TEXT("ch192"),
	TEXT("ch193"),TEXT("ch194"),TEXT("ch195"),TEXT("ch196"),TEXT("ch197"),TEXT("ch198"),TEXT("ch199"),TEXT("ch200"),
	TEXT("ch201"),TEXT("ch202"),TEXT("ch203"),TEXT("ch204"),TEXT("ch205"),TEXT("ch206"),TEXT("ch207"),TEXT("ch208"),
	TEXT("ch209"),TEXT("ch210"),TEXT("ch211"),TEXT("ch212"),TEXT("ch213"),TEXT("ch214"),TEXT("ch215"),TEXT("ch216"),
	TEXT("ch217"),TEXT("ch218"),TEXT("ch219"),TEXT("ch220"),TEXT("ch221"),TEXT("ch222"),TEXT("ch223"),TEXT("ch224"),
	TEXT("ch225"),TEXT("ch226"),TEXT("ch227"),TEXT("ch228"),TEXT("ch229"),TEXT("ch230"),TEXT("ch231"),TEXT("ch232"),
	TEXT("ch233"),TEXT("ch234"),TEXT("ch235"),TEXT("ch236"),TEXT("ch237"),TEXT("ch238"),TEXT("ch239"),TEXT("ch240"),
	TEXT("ch241"),TEXT("ch242"),TEXT("ch243"),TEXT("ch244"),TEXT("ch245"),TEXT("ch246"),TEXT("ch247"),TEXT("ch248"),
	TEXT("ch249"),TEXT("ch250"),TEXT("ch251"),TEXT("ch252"),TEXT("ch253"),TEXT("ch254"),TEXT("ch255"),TEXT("ch256"),
};

// BANK

// PROGRAM
#define RESTORE_PROGRAM                     0x00000000
#define PREF_PROGRAM_MODE_DEFAULT_PROGRAM	1
#define PREF_PROGRAM_MODE_SPECIAL_PROGRAM	2

#define PREF_PROGRAM_PAGE_NUM               ((MAX_CHANNELS + 15) >> 4) 
#define PREF_PROGRAM_PAGE_001_016           0
#define PREF_PROGRAM_PAGE_017_032           1
#define PREF_PROGRAM_PAGE_033_048           2
#define PREF_PROGRAM_PAGE_049_064           3
#define PREF_PROGRAM_PAGE_065_080           4
#define PREF_PROGRAM_PAGE_081_096           5
#define PREF_PROGRAM_PAGE_097_112           6
#define PREF_PROGRAM_PAGE_113_128           7
#define PREF_PROGRAM_PAGE_129_144           8
#define PREF_PROGRAM_PAGE_145_160           9
#define PREF_PROGRAM_PAGE_161_176           10
#define PREF_PROGRAM_PAGE_177_192           11
#define PREF_PROGRAM_PAGE_193_208           12
#define PREF_PROGRAM_PAGE_209_224           13
#define PREF_PROGRAM_PAGE_225_240           14
#define PREF_PROGRAM_PAGE_241_256           15
// IDC_COMBO_PROGRAM
static const TCHAR *cb_info_IDC_COMBO_PROGRAM[] = {
	TEXT("Port01 ch001-016"),
	TEXT("Port02 ch017-032"),
	TEXT("Port03 ch033-048"),
	TEXT("Port04 ch049-064"),
	TEXT("Port05 ch065-080"),
	TEXT("Port06 ch081-096"),
	TEXT("Port07 ch097-112"),
	TEXT("Port08 ch113-128"),
	TEXT("Port09 ch129-144"),
	TEXT("Port10 ch145-160"),
	TEXT("Port11 ch161-176"),
	TEXT("Port12 ch177-192"),
	TEXT("Port13 ch193-208"),
	TEXT("Port14 ch209-224"),
	TEXT("Port15 ch225-240"),
	TEXT("Port16 ch241-256"),
};

// CHANNEL
#define RESTORE_CHANNEL                     0x00000001
#define PREF_CHANNEL_MODE_DRUM_CHANNEL		1
#define PREF_CHANNEL_MODE_DRUM_CHANNEL_MASK	2
#define PREF_CHANNEL_MODE_QUIET_CHANNEL		3

#define PREF_CHANNEL_PAGE_NUM            ((MAX_CHANNELS + 31) >> 5)
#define PREF_CHANNEL_PAGE_001_032           0
#define PREF_CHANNEL_PAGE_033_064           1
#define PREF_CHANNEL_PAGE_065_096           2
#define PREF_CHANNEL_PAGE_097_128           3
#define PREF_CHANNEL_PAGE_129_160           4
#define PREF_CHANNEL_PAGE_161_192           5
#define PREF_CHANNEL_PAGE_193_224           6
#define PREF_CHANNEL_PAGE_225_256           7

// IDC_COMBO_CHANNEL
static const TCHAR *cb_info_IDC_COMBO_CHANNEL[] = {
	TEXT("Port01-02 ch001-032"),
	TEXT("Port03-04 ch033-064"),
	TEXT("Port05-06 ch065-096"),
	TEXT("Port07-08 ch097-128"),
	TEXT("Port09-10 ch129-160"),
	TEXT("Port11-12 ch161-192"),
	TEXT("Port13-14 ch193-224"),
	TEXT("Port15-16 ch225-256"),
};

// IDC_COMBO_(INIT|FORCE)_KEYSIG
static const TCHAR *cb_info_IDC_COMBO_KEYSIG[] = {
	TEXT("Cb Maj / Ab Min (b7)"),
	TEXT("Gb Maj / Eb Min (b6)"),
	TEXT("Db Maj / Bb Min (b5)"),
	TEXT("Ab Maj / F  Min (b4)"),
	TEXT("Eb Maj / C  Min (b3)"),
	TEXT("Bb Maj / G  Min (b2)"),
	TEXT("F  Maj / D  Min (b1)"),
	TEXT("C  Maj / A  Min (0)"),
	TEXT("G  Maj / E  Min (#1)"),
	TEXT("D  Maj / B  Min (#2)"),
	TEXT("A  Maj / F# Min (#3)"),
	TEXT("E  Maj / C# Min (#4)"),
	TEXT("B  Maj / G# Min (#5)"),
	TEXT("F# Maj / D# Min (#6)"),
	TEXT("C# Maj / A# Min (#7)"),
};

// IDC_COMBO_KEY_AJUST
static int cb_info_IDC_COMBO_KEY_ADJUST_num[] = {
	-12,
	-11,
	-10,
	-9,
	-8,
	-7,
	-6,
	-5,
	-4,
	-3,
	-2,
	-1,
	0,
	1,
	2,
	3,
	4,
	5,
	6,
	7,
	8,
	9,
	10,
	11,
	12,
};
static const TCHAR *cb_info_IDC_COMBO_KEY_ADJUST[] = {
	TEXT("-12"),
	TEXT("-11"),
	TEXT("-10"),
	TEXT("-9"),
	TEXT("-8"),
	TEXT("-7"),
	TEXT("-6"),
	TEXT("-5"),
	TEXT("-4"),
	TEXT("-3"),
	TEXT("-2"),
	TEXT("-1"),
	TEXT(" 0"),
	TEXT("+1"),
	TEXT("+2"),
	TEXT("+3"),
	TEXT("+4"),
	TEXT("+5"),
	TEXT("+6"),
	TEXT("+7"),
	TEXT("+8"),
	TEXT("+9"),
	TEXT("+10"),
	TEXT("+11"),
	TEXT("+12"),
};

static LRESULT APIENTRY
PrefTiMidity2DialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	static int initflag = 1;
	static int pref_program_mode;
	static int pref_program_page;
	static int pref_channel_mode;
	static int pref_channel_page;
	static ChannelBitMask channelbitmask;
    static const TCHAR **cache_info_BANK_PROGRAM = 0;
	int i, j, tmp;
	switch (uMess){
	case WM_INITDIALOG:
		// BANK
		for (i = 0; i < num_BANK_PROGRAM -1; i++)
			CB_INSSTR(IDC_COMBO_DEFAULT_TONEBANK, info_BANK_PROGRAM[i+1]);
		CB_SET(IDC_COMBO_DEFAULT_TONEBANK, RANGE(st_temp->default_tonebank, 0, 127));
		for (i = 0; i < num_BANK_PROGRAM; i++)
			CB_INSSTR(IDC_COMBO_SPECIAL_TONEBANK, info_BANK_PROGRAM[i]);
		CB_SET(IDC_COMBO_SPECIAL_TONEBANK, RANGE(st_temp->special_tonebank, -1, 127) + 1);

		// PROGRAM
		pref_program_mode = PREF_PROGRAM_MODE_DEFAULT_PROGRAM;
		pref_program_page = PREF_PROGRAM_PAGE_001_016;
		for (i = 0; i < PREF_PROGRAM_PAGE_NUM; i++)
			CB_INSSTR(IDC_COMBO_PROGRAM, cb_info_IDC_COMBO_PROGRAM[i]);
		CB_SET(IDC_COMBO_PROGRAM, pref_program_page);
		
        cache_info_BANK_PROGRAM = 0;
		SendMessage(hwnd,WM_MYRESTORE,(WPARAM)RESTORE_PROGRAM,(LPARAM)0);

		// CAHHNEL
		pref_channel_mode = PREF_CHANNEL_MODE_DRUM_CHANNEL;
		pref_channel_page = PREF_CHANNEL_PAGE_001_032;
		for (i = 0; i < PREF_CHANNEL_PAGE_NUM; i++)
			CB_INSSTR(IDC_COMBO_CHANNEL, cb_info_IDC_COMBO_CHANNEL[i]);
		CB_SET(IDC_COMBO_CHANNEL, pref_channel_page);
		SendMessage(hwnd,WM_MYRESTORE,(WPARAM)RESTORE_CHANNEL,(LPARAM)0);

		// MUSIC GRAMMER
		DLG_FLAG_TO_CHECKBUTTON(hwnd, IDC_CHECKBOX_PURE_INTONATION, st_temp->opt_pure_intonation);
		SendMessage(hwnd, WM_COMMAND, IDC_CHECKBOX_PURE_INTONATION, 0);
		DLG_FLAG_TO_CHECKBUTTON(hwnd, IDC_CHECKBOX_INIT_KEYSIG, (st_temp->opt_init_keysig != 8));
		SendMessage(hwnd, WM_COMMAND, IDC_CHECKBOX_PURE_INTONATION, 0);

		for (i = 0; i < 15; i++)
			CB_INSSTR(IDC_COMBO_INIT_KEYSIG, cb_info_IDC_COMBO_KEYSIG[i]);
		if (st_temp->opt_init_keysig == 8) {
			CB_SET(IDC_COMBO_INIT_KEYSIG, 7);
			SendDlgItemMessage(hwnd, IDC_CHECKBOX_INIT_MI, BM_SETCHECK, 0, 0);
		} else {
			CB_SET(IDC_COMBO_INIT_KEYSIG, (st_temp->opt_init_keysig + 7 & 0x0f));
			SendDlgItemMessage(hwnd, IDC_CHECKBOX_INIT_MI, BM_SETCHECK, (st_temp->opt_init_keysig + 7 & 0x10) ? 1 : 0, 0);
		}

		// KEY_ADJUST
		for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_KEY_ADJUST_num); i++)
			CB_INSSTR(IDC_COMBO_KEY_ADJUST, cb_info_IDC_COMBO_KEY_ADJUST[i]);
		CB_SET(IDC_COMBO_KEY_ADJUST, CB_FIND(cb_info_IDC_COMBO_KEY_ADJUST_num, st_temp->key_adjust, 12));

		DLG_FLAG_TO_CHECKBUTTON(hwnd, IDC_CHECKBOX_FORCE_KEYSIG, (st_temp->opt_force_keysig != 8));
		SendMessage(hwnd, WM_COMMAND, IDC_CHECKBOX_FORCE_KEYSIG, 0);
		for (i = 0; i < 15; i++)
			CB_INSSTR(IDC_COMBO_FORCE_KEYSIG, cb_info_IDC_COMBO_KEYSIG[i]);
		if (st_temp->opt_force_keysig == 8)
			CB_SET(IDC_COMBO_FORCE_KEYSIG, 7);
		else
			CB_SET(IDC_COMBO_FORCE_KEYSIG, st_temp->opt_force_keysig + 7);

		// TMPER MUTE
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_TEMPER_EQUAL,
				st_temp->temper_type_mute & 1 << 0);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_TEMPER_PYTHA,
				st_temp->temper_type_mute & 1 << 1);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_TEMPER_MEANTONE,
				st_temp->temper_type_mute & 1 << 2);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_TEMPER_PUREINT,
				st_temp->temper_type_mute & 1 << 3);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_TEMPER_USER0,
				st_temp->temper_type_mute & 1 << 4);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_TEMPER_USER1,
				st_temp->temper_type_mute & 1 << 5);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_TEMPER_USER2,
				st_temp->temper_type_mute & 1 << 6);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_TEMPER_USER3,
				st_temp->temper_type_mute & 1 << 7);

		initflag = 0;
		break;
	case WM_MYRESTORE:
		switch (wParam) {
		case RESTORE_PROGRAM:
            switch (pref_program_mode) {
            case PREF_PROGRAM_MODE_DEFAULT_PROGRAM:
                CH_SET(IDC_CHECKBOX_DEFAULT_PROGRAM, 1);
                CH_SET(IDC_CHECKBOX_SPECIAL_PROGRAM, 0);
                if (cache_info_BANK_PROGRAM != info_BANK_PROGRAM + 1) {
                    cache_info_BANK_PROGRAM = info_BANK_PROGRAM + 1;
                    for (i = 0; i < 16; i++) {
                        CB_RESET(IDC_COMBO_PROGRAM01 + i); // delete info
                        for (j = 0; j < num_BANK_PROGRAM -1; j++)
                            CB_INSSTR(IDC_COMBO_PROGRAM01 + i, cache_info_BANK_PROGRAM[j]);
                    }
                }
                for (i = 0; i < 16; i++) {
                    EB_SETTEXT(IDC_STATIC_PROGRAM01 + i, info_CHANNEL[pref_program_page * 16 + i]);
                    CB_SET(IDC_COMBO_PROGRAM01 + i, st_temp->default_program[pref_program_page * 16 + i]);
                }
                break;

            case PREF_PROGRAM_MODE_SPECIAL_PROGRAM:
                CH_SET(IDC_CHECKBOX_DEFAULT_PROGRAM, 0);
                CH_SET(IDC_CHECKBOX_SPECIAL_PROGRAM, 1);
                if (cache_info_BANK_PROGRAM != info_BANK_PROGRAM) {
                    cache_info_BANK_PROGRAM = info_BANK_PROGRAM;
                    for (i = 0; i < 16; i++) {
                        CB_RESET(IDC_COMBO_PROGRAM01 + i); // delete info
                        for (j = 0; j < num_BANK_PROGRAM; j++)
                            CB_INSSTR(IDC_COMBO_PROGRAM01 + i, cache_info_BANK_PROGRAM[j]);
                    }
                }
                for (i = 0; i < 16; i++) {
                    EB_SETTEXT(IDC_STATIC_PROGRAM01 + i, info_CHANNEL[pref_program_page * 16 + i]);
                    CB_SET(IDC_COMBO_PROGRAM01 + i, st_temp->special_program[pref_program_page * 16 + i] + 1);
                }
                break;
            }
            break;

		case RESTORE_CHANNEL:
			switch(pref_channel_mode){
			case PREF_CHANNEL_MODE_DRUM_CHANNEL_MASK:
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_DRUM_CHANNEL,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_DRUM_CHANNEL_MASK,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_QUIET_CHANNEL,BM_SETCHECK,0,0);
				channelbitmask = st_temp->default_drumchannel_mask;
				break;
			case PREF_CHANNEL_MODE_QUIET_CHANNEL:
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_DRUM_CHANNEL,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_DRUM_CHANNEL_MASK,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_QUIET_CHANNEL,BM_SETCHECK,1,0);
				channelbitmask = st_temp->quietchannels;
				break;
			default:
			case PREF_CHANNEL_MODE_DRUM_CHANNEL:
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_DRUM_CHANNEL,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_DRUM_CHANNEL_MASK,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_QUIET_CHANNEL,BM_SETCHECK,0,0);
				channelbitmask = st_temp->default_drumchannels;
				break;
			}
			for(i = 0; i < 32; i++){
				SetDlgItemText(hwnd, (IDC_CHECKBOX_CH01 + i), info_CHANNEL[pref_channel_page * 32 + i]);
				DLG_FLAG_TO_CHECKBUTTON(hwnd,(IDC_CHECKBOX_CH01 + i),IS_SET_CHANNELMASK(channelbitmask,(pref_channel_page * 32 + i)));
			}
			break;
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCLOSE:
			break;
		// PROGRAM
		case IDC_COMBO_PROGRAM:
			if(HIWORD(wParam) != 9 || pref_program_page == CB_GETS(IDC_COMBO_PROGRAM, 0))
				return 0;
			SendMessage(hwnd,WM_MYSAVE,(WPARAM)0,(LPARAM)0);
			pref_program_page = CB_GETS(IDC_COMBO_PROGRAM, 0);
			SendMessage(hwnd,WM_MYRESTORE,(WPARAM)RESTORE_PROGRAM,(LPARAM)0);
			break;
		case IDC_CHECKBOX_DEFAULT_PROGRAM:
			SendMessage(hwnd,WM_MYSAVE,(WPARAM)0,(LPARAM)0);
			pref_program_mode = PREF_PROGRAM_MODE_DEFAULT_PROGRAM;
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_DEFAULT_PROGRAM,BM_SETCHECK,1,0);
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_SPECIAL_PROGRAM,BM_SETCHECK,0,0);
			SendMessage(hwnd,WM_MYRESTORE,(WPARAM)RESTORE_PROGRAM,(LPARAM)0);
			break;
		case IDC_CHECKBOX_SPECIAL_PROGRAM:
			SendMessage(hwnd,WM_MYSAVE,(WPARAM)0,(LPARAM)0);
			pref_program_mode = PREF_PROGRAM_MODE_SPECIAL_PROGRAM;
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_DEFAULT_PROGRAM,BM_SETCHECK,0,0);
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_SPECIAL_PROGRAM,BM_SETCHECK,1,0);
			SendMessage(hwnd,WM_MYRESTORE,(WPARAM)RESTORE_PROGRAM,(LPARAM)0);
			break;

		// CHANNEL
		case IDC_COMBO_CHANNEL:
			if(HIWORD(wParam) != 9 || pref_channel_page == CB_GETS(IDC_COMBO_CHANNEL,0))
				return 0;
			SendMessage(hwnd,WM_MYSAVE,(WPARAM)0,(LPARAM)0);
			pref_channel_page = CB_GETS(IDC_COMBO_CHANNEL,0);
			SendMessage(hwnd,WM_MYRESTORE,(WPARAM)RESTORE_CHANNEL,(LPARAM)0);
			break;
		case IDC_BUTTON_REVERSE:
			SendMessage(hwnd,WM_MYSAVE,(WPARAM)0,(LPARAM)0);
			switch(pref_channel_mode){
			case PREF_CHANNEL_MODE_DRUM_CHANNEL_MASK:
				REVERSE_CHANNELMASK(st_temp->default_drumchannel_mask, pref_channel_page);
				break;
			case PREF_CHANNEL_MODE_QUIET_CHANNEL:
				REVERSE_CHANNELMASK(st_temp->quietchannels, pref_channel_page);
				break;
			default:
			case PREF_CHANNEL_MODE_DRUM_CHANNEL:
				REVERSE_CHANNELMASK(st_temp->default_drumchannels, pref_channel_page);
				break;
			}
			SendMessage(hwnd,WM_MYRESTORE,(WPARAM)RESTORE_CHANNEL,(LPARAM)0);
			break;
		case IDC_CHECKBOX_DRUM_CHANNEL:
			SendMessage(hwnd,WM_MYSAVE,(WPARAM)0,(LPARAM)0);
			pref_channel_mode = PREF_CHANNEL_MODE_DRUM_CHANNEL;
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_DRUM_CHANNEL,BM_SETCHECK,1,0);
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_DRUM_CHANNEL_MASK,BM_SETCHECK,0,0);
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_QUIET_CHANNEL,BM_SETCHECK,0,0);
			SendMessage(hwnd,WM_MYRESTORE,(WPARAM)RESTORE_CHANNEL,(LPARAM)0);
			break;
		case IDC_CHECKBOX_DRUM_CHANNEL_MASK:
			SendMessage(hwnd,WM_MYSAVE,(WPARAM)0,(LPARAM)0);
			pref_channel_mode = PREF_CHANNEL_MODE_DRUM_CHANNEL_MASK;
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_DRUM_CHANNEL,BM_SETCHECK,0,0);
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_DRUM_CHANNEL_MASK,BM_SETCHECK,1,0);
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_QUIET_CHANNEL,BM_SETCHECK,0,0);
			SendMessage(hwnd,WM_MYRESTORE,(WPARAM)RESTORE_CHANNEL,(LPARAM)0);
			break;
		case IDC_CHECKBOX_QUIET_CHANNEL:
			SendMessage(hwnd,WM_MYSAVE,(WPARAM)0,(LPARAM)0);
			pref_channel_mode = PREF_CHANNEL_MODE_QUIET_CHANNEL;
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_DRUM_CHANNEL,BM_SETCHECK,0,0);
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_DRUM_CHANNEL_MASK,BM_SETCHECK,0,0);
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_QUIET_CHANNEL,BM_SETCHECK,1,0);
			SendMessage(hwnd,WM_MYRESTORE,(WPARAM)RESTORE_CHANNEL,(LPARAM)0);
			break;
		// MUSIC GRAMMER
		case IDC_CHECKBOX_PURE_INTONATION:
			if (SendDlgItemMessage(hwnd, IDC_CHECKBOX_PURE_INTONATION,
					BM_GETCHECK, 0, 0)) {
				EnableWindow(GetDlgItem(hwnd,
						IDC_CHECKBOX_INIT_KEYSIG), TRUE);
				if (SendDlgItemMessage(hwnd,
						IDC_CHECKBOX_INIT_KEYSIG, BM_GETCHECK, 0, 0)) {
					EnableWindow(GetDlgItem(hwnd,
							IDC_COMBO_INIT_KEYSIG), TRUE);
					EnableWindow(GetDlgItem(hwnd,
							IDC_CHECKBOX_INIT_MI), TRUE);
				} else {
					EnableWindow(GetDlgItem(hwnd,
							IDC_COMBO_INIT_KEYSIG), FALSE);
					EnableWindow(GetDlgItem(hwnd,
							IDC_CHECKBOX_INIT_MI), FALSE);
				}
			} else {
				EnableWindow(GetDlgItem(hwnd,
						IDC_CHECKBOX_INIT_KEYSIG), FALSE);
				EnableWindow(GetDlgItem(hwnd, IDC_COMBO_INIT_KEYSIG), FALSE);
				EnableWindow(GetDlgItem(hwnd, IDC_CHECKBOX_INIT_MI), FALSE);
			}
			break;
		case IDC_CHECKBOX_INIT_KEYSIG:
			if (SendDlgItemMessage(hwnd,
					IDC_CHECKBOX_INIT_KEYSIG, BM_GETCHECK, 0, 0)) {
				EnableWindow(GetDlgItem(hwnd, IDC_COMBO_INIT_KEYSIG), TRUE);
				EnableWindow(GetDlgItem(hwnd, IDC_CHECKBOX_INIT_MI), TRUE);
			} else {
				EnableWindow(GetDlgItem(hwnd, IDC_COMBO_INIT_KEYSIG), FALSE);
				EnableWindow(GetDlgItem(hwnd, IDC_CHECKBOX_INIT_MI), FALSE);
			}
			break;
		case IDC_COMBO_INIT_KEYSIG:
		case IDC_CHECKBOX_INIT_MI:
			st_temp->opt_init_keysig = SendDlgItemMessage(hwnd,
					IDC_COMBO_INIT_KEYSIG, CB_GETCURSEL,
					(WPARAM) 0, (LPARAM) 0) + ((SendDlgItemMessage(hwnd,
					IDC_CHECKBOX_INIT_MI, BM_GETCHECK,
					0, 0)) ? 16 : 0) - 7;
			break;
		case IDC_CHECKBOX_FORCE_KEYSIG:
			if (SendDlgItemMessage(hwnd,
					IDC_CHECKBOX_FORCE_KEYSIG, BM_GETCHECK, 0, 0))
				EnableWindow(GetDlgItem(hwnd, IDC_COMBO_FORCE_KEYSIG), TRUE);
			else
				EnableWindow(GetDlgItem(hwnd, IDC_COMBO_FORCE_KEYSIG), FALSE);
			break;
		case IDC_COMBO_FORCE_KEYSIG:
			st_temp->opt_force_keysig = SendDlgItemMessage(hwnd,
					IDC_COMBO_FORCE_KEYSIG, CB_GETCURSEL,
					(WPARAM) 0, (LPARAM) 0) - 7;
			break;
		default:
			break;
		}
		PrefWndSetOK = 1;
		break;
	case WM_MYSAVE:
		if ( initflag ) break;
		// BANK
		st_temp->default_tonebank = CB_GETS(IDC_COMBO_DEFAULT_TONEBANK, 0);
		st_temp->special_tonebank = CB_GETS(IDC_COMBO_SPECIAL_TONEBANK, 0) - 1;

		// PROGRAM
		switch(pref_program_mode){
		case PREF_PROGRAM_MODE_DEFAULT_PROGRAM:
			for(i = 0; i < 16; i++)
				st_temp->default_program[pref_program_page *16 +i] = CB_GETS(IDC_COMBO_PROGRAM01 +i, 0);
			break;
		case PREF_PROGRAM_MODE_SPECIAL_PROGRAM:
			for(i = 0; i < 16; i++)
				st_temp->special_program[pref_program_page *16 +i] = CB_GETS(IDC_COMBO_PROGRAM01 +i, 0) - 1;
			break;
		}

		// CHANNEL
#define PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,ctlid,channelbitmask,ch,tmp) \
{	if(DLG_CHECKBUTTON_TO_FLAG((hwnd),(ctlid),(tmp))) SET_CHANNELMASK((channelbitmask),(ch)); \
else UNSET_CHANNELMASK((channelbitmask),(ch)); }

		for(i = 0; i < 32; i++)
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,(IDC_CHECKBOX_CH01 + i), channelbitmask, (pref_channel_page * 32 + i), tmp);
		switch(pref_channel_mode){
		case PREF_CHANNEL_MODE_DRUM_CHANNEL_MASK:
			st_temp->default_drumchannel_mask = channelbitmask;
			break;
		case PREF_CHANNEL_MODE_QUIET_CHANNEL:
			st_temp->quietchannels = channelbitmask;
			break;
		default:
		case PREF_CHANNEL_MODE_DRUM_CHANNEL:
			st_temp->default_drumchannels = channelbitmask;
			break;
		}

		// MUSIC GRAMMER
		DLG_CHECKBUTTON_TO_FLAG(hwnd,
				IDC_CHECKBOX_PURE_INTONATION, st_temp->opt_pure_intonation);
		if (SendDlgItemMessage(hwnd, IDC_CHECKBOX_PURE_INTONATION,
				BM_GETCHECK, 0, 0) && SendDlgItemMessage(hwnd,
				IDC_CHECKBOX_INIT_KEYSIG, BM_GETCHECK, 0, 0))
			st_temp->opt_init_keysig = SendDlgItemMessage(hwnd,
					IDC_COMBO_INIT_KEYSIG, CB_GETCURSEL,
					(WPARAM) 0, (LPARAM) 0) + ((SendDlgItemMessage(hwnd,
					IDC_CHECKBOX_INIT_MI, BM_GETCHECK,
					0, 0)) ? 16 : 0) - 7;
		else
			st_temp->opt_init_keysig = 8;
		// KEY_AJUST
		st_temp->key_adjust = cb_info_IDC_COMBO_KEY_ADJUST_num[CB_GETS(IDC_COMBO_KEY_ADJUST, 12)];

		if (SendDlgItemMessage(hwnd,
				IDC_CHECKBOX_FORCE_KEYSIG, BM_GETCHECK, 0, 0))
			st_temp->opt_force_keysig = SendDlgItemMessage(hwnd,
					IDC_COMBO_FORCE_KEYSIG, CB_GETCURSEL,
					(WPARAM) 0, (LPARAM) 0) - 7;
		else
			st_temp->opt_force_keysig = 8;

		// TMPER MUTE
		st_temp->temper_type_mute = 0;
		if (SendDlgItemMessage(hwnd, IDC_CHECKBOX_TEMPER_EQUAL,
				BM_GETCHECK, 0, 0))
			st_temp->temper_type_mute |= 1 << 0;
		if (SendDlgItemMessage(hwnd, IDC_CHECKBOX_TEMPER_PYTHA,
				BM_GETCHECK, 0, 0))
			st_temp->temper_type_mute |= 1 << 1;
		if (SendDlgItemMessage(hwnd, IDC_CHECKBOX_TEMPER_MEANTONE,
				BM_GETCHECK, 0, 0))
			st_temp->temper_type_mute |= 1 << 2;
		if (SendDlgItemMessage(hwnd, IDC_CHECKBOX_TEMPER_PUREINT,
				BM_GETCHECK, 0, 0))
			st_temp->temper_type_mute |= 1 << 3;
		if (SendDlgItemMessage(hwnd, IDC_CHECKBOX_TEMPER_USER0,
				BM_GETCHECK, 0, 0))
			st_temp->temper_type_mute |= 1 << 4;
		if (SendDlgItemMessage(hwnd, IDC_CHECKBOX_TEMPER_USER1,
				BM_GETCHECK, 0, 0))
			st_temp->temper_type_mute |= 1 << 5;
		if (SendDlgItemMessage(hwnd, IDC_CHECKBOX_TEMPER_USER2,
				BM_GETCHECK, 0, 0))
			st_temp->temper_type_mute |= 1 << 6;
		if (SendDlgItemMessage(hwnd, IDC_CHECKBOX_TEMPER_USER3,
				BM_GETCHECK, 0, 0))
			st_temp->temper_type_mute |= 1 << 7;
		SetWindowLongPtr(hwnd,DWLP_MSGRESULT,FALSE);
		break;
  case WM_SIZE:
		return FALSE;
	case WM_DESTROY:
		break;
	default:
	  break;
	}
	return FALSE;
}


///r

// IDC_COMBO_BANDWIDTH
#define cb_num_IDC_COMBO_BANDWIDTH 7
enum {
	BANDWIDTH_8BIT =	0,
	BANDWIDTH_16BIT =	1,
	BANDWIDTH_24BIT =	2,
	BANDWIDTH_32BIT =	3,
	BANDWIDTH_F32BIT =	4,
	BANDWIDTH_64BIT =	5,
	BANDWIDTH_F64BIT =	6,
};
static const TCHAR *cb_info_IDC_COMBO_BANDWIDTH_en[] = {
	TEXT("8-bit"),
	TEXT("16-bit"),
	TEXT("24-bit"),
	TEXT("32-bit"),
	TEXT("32-bit float"),
	TEXT("64-bit"),
	TEXT("64-bit float")
};
static const TCHAR *cb_info_IDC_COMBO_BANDWIDTH_jp[] = {
	TEXT("8ビット"),
	TEXT("16ビット"),
	TEXT("24ビット"),
	TEXT("32ビット"),
	TEXT("32ビットfloat"),
	TEXT("64ビット"),
	TEXT("64ビットfloat")
};
static const TCHAR **cb_info_IDC_COMBO_BANDWIDTH;

// IDC_COMBO_SAMPLE_RATE
static int cb_info_IDC_COMBO_SAMPLE_RATE_num[] = {
	4000,
	8000,
	11025,
	16000,
	22050,
	24000,
	32000,
	40000,
	44100,
	48000,
	64000,
	88200,
	96000,
	128000,
	132300,
	144000,
	176400,
	192000,
	256000,
	264600,
	288000,
	352800,
	384000,
};

static const TCHAR *cb_info_IDC_COMBO_SAMPLE_RATE[] = {
	TEXT("4000 Hz"),
	TEXT("8000 Hz"),
	TEXT("11025 Hz"),
	TEXT("16000 Hz"),
	TEXT("22050 Hz"),
	TEXT("24000 Hz"),
	TEXT("32000 Hz"),
	TEXT("40000 Hz"),
	TEXT("44100 Hz"),
	TEXT("48000 Hz"),
	TEXT("64000 Hz"),
	TEXT("88200 Hz"),
	TEXT("96000 Hz"),
	TEXT("128000 Hz"),
	TEXT("132300 Hz"),
	TEXT("144000 Hz"),
	TEXT("176400 Hz"),
	TEXT("192000 Hz"),
	TEXT("256000 Hz"),
	TEXT("264600 Hz"),
	TEXT("288000 Hz"),
	TEXT("352800 Hz"),
	TEXT("384000 Hz"),
};


// IDC_COMBO_COMPUTE_BUFFER_BITS
static int cb_info_IDC_COMBO_COMPUTE_BUFFER_BITS_num[] = {
	-1,
	-2,
	-3,
	-4,
	-5,
	0,
	1,
	2,
	3,
	4,
	5,
	6,
    7,
    8,
    9,
    10,
};

static const TCHAR *cb_info_IDC_COMBO_COMPUTE_BUFFER_BITS_en[] = {
	TEXT("Auto setting 1ms"),
	TEXT("Auto setting 2ms"),
	TEXT("Auto setting 3ms"),
	TEXT("Auto setting 4ms"),
	TEXT("Auto setting 5ms"),
	TEXT("1samples 0bit"),
	TEXT("2samples 1bit"),
	TEXT("4samples 2bit"),
	TEXT("8samples 3bit"),
	TEXT("16samples 4bit"),
	TEXT("32samples 5bit"),
	TEXT("64samples 6bit"),
	TEXT("128samples 7bit"),
	TEXT("256samples 8bit"),
	TEXT("512samples 9bit"),
	TEXT("1024samples 10bit"),
};

static const TCHAR *cb_info_IDC_COMBO_COMPUTE_BUFFER_BITS_jp[] = {
	TEXT("自動設定 約1ms"),
	TEXT("自動設定 約2ms"),
	TEXT("自動設定 約3ms"),
	TEXT("自動設定 約4ms"),
	TEXT("自動設定 約5ms"),
	TEXT("1サンプル 0bit"),
	TEXT("2サンプル 1bit"),
	TEXT("4サンプル 2bit"),
	TEXT("8サンプル 3bit"),
	TEXT("16サンプル 4bit"),
	TEXT("32サンプル 5bit"),
	TEXT("64サンプル 6bit"),
	TEXT("128サンプル 7bit"),
	TEXT("256サンプル 8bit"),
	TEXT("512サンプル 9bit"),
	TEXT("1024サンプル 10bit"),
};

// IDC_COMBO_BUFFER_BITS
static int cb_info_IDC_COMBO_BUFFER_BITS_num[] = {
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
};
static const TCHAR *cb_info_IDC_COMBO_BUFFER_BITS_en[] = {
	TEXT("32samples 5bit"),
	TEXT("64samples 6bit"),
	TEXT("128samples 7bit"),
	TEXT("256samples 8bit"),
	TEXT("512samples 9bit"),
	TEXT("1024samples 10bit"),
	TEXT("2048samples 11bit"),
	TEXT("4096samples 12bit"),
};
static const TCHAR *cb_info_IDC_COMBO_BUFFER_BITS_jp[] = {
	TEXT("32サンプル 5bit"),
	TEXT("64サンプル 6bit"),
	TEXT("128サンプル 7bit"),
	TEXT("256サンプル 8bit"),
	TEXT("512サンプル 9bit"),
	TEXT("1024サンプル 10bit"),
	TEXT("2048サンプル 11bit"),
	TEXT("4096サンプル 12bit"),
};

// IDC_COMBO_FRAGMENTS
static int cb_info_IDC_COMBO_FRAGMENTS_num[] = {
	2,
	4,
	6,
	8,
	12,
	16,
	24,
	32,
	48,
	64,
	96,
	128,
	160,
	256,
//	384,
//	512,
//	768,
//	1024,
//	2048,
//	4096,
};
static const TCHAR *cb_info_IDC_COMBO_FRAGMENTS_en[] = {
	TEXT("2blocks"),
	TEXT("4blocks"),
	TEXT("6blocks"),
	TEXT("8blocks"),
	TEXT("12blocks"),
	TEXT("16blocks"),
	TEXT("24blocks"),
	TEXT("32blocks"),
	TEXT("48blocks"),
	TEXT("64blocks"),
	TEXT("96blocks"),
	TEXT("128blocks"),
	TEXT("160blocks"),
	TEXT("256blocks"),
//	TEXT("384blocks"),
//	TEXT("512blocks"),
//	TEXT("768blocks"),
//	TEXT("1024blocks"),
//	TEXT("2048blocks"),
//	TEXT("4096blocks"),
};
static const TCHAR *cb_info_IDC_COMBO_FRAGMENTS_jp[] = {
	TEXT("2ブロック"),
	TEXT("4ブロック"),
	TEXT("6ブロック"),
	TEXT("8ブロック"),
	TEXT("12ブロック"),
	TEXT("16ブロック"),
	TEXT("24ブロック"),
	TEXT("32ブロック"),
	TEXT("48ブロック"),
	TEXT("64ブロック"),
	TEXT("96ブロック"),
	TEXT("128ブロック"),
	TEXT("160ブロック"),
	TEXT("256ブロック"),
//	TEXT("384ブロック"),
//	TEXT("512ブロック"),
//	TEXT("768ブロック"),
//	TEXT("1024ブロック"),
//	TEXT("2048ブロック"),
//	TEXT("4096ブロック"),
};

// IDC_COMBO_OUTPUT_MODE
static const TCHAR *cb_info_IDC_COMBO_OUTPUT_MODE_jp[] = {
	TEXT("以下のファイルに出力"), (TCHAR*)0,
#if defined(__CYGWIN32__) || defined(__MINGW32__)
	TEXT("ファイル名を自動で決定し、ソ\ースと同じフォルダに出力"), (TCHAR*)1,
#else
	TEXT("ファイル名を自動で決定し、ソースと同じフォルダに出力"), (TCHAR*)1,
#endif

	TEXT("ファイル名を自動で決定し、以下のフォルダに出力"), (TCHAR*)2,
	TEXT("ファイル名を自動で決定し、以下のフォルダに出力(フォルダ名付き)"), (TCHAR*)3,
	NULL,
};
static const TCHAR *cb_info_IDC_COMBO_OUTPUT_MODE_en[] = {
	TEXT("next output file"), (TCHAR*)0,
	TEXT("auto filename"), (TCHAR*)1,
	TEXT("auto filename and output in next dir"), (TCHAR*)2,
	TEXT("auto filename and output in next dir (with folder name)"), (TCHAR*)3,
	NULL,
};

static const TCHAR **cb_info_IDC_COMBO_OUTPUT_MODE = NULL;


// IDC_COMBO_CACHE_SIZE
static int cb_info_IDC_COMBO_CACHE_SIZE_num[] = {
    0,
    1048576, // 1
    2097152, // 2
    3145728, // 3
    4194304, // 4
    6291456, // 6
	8388608, // 8
	10485760, // 10
	12582912, // 12
	16777216, // 16
	20971520, // 20
	25165824, // 24
	33554432, // 32
	41943040, // 40
	50331648, // 48
	67108864, // 64 
	83886080, // 80 
	100663296, // 96
	134217728, // 128
	167772160, // 160
	201326592, // 192
	268435456, // 256
	335544320, // 320
	402653184, // 384
	//536870912, // 512
	//671088640, // 640
	//805306368, // 768
	//939524096, // 896
	//1073741824, // 1024
};
static const TCHAR *cb_info_IDC_COMBO_CACHE_SIZE[] = {
	TEXT("OFF"),
	TEXT("1MB"),
	TEXT("2MB"),
	TEXT("3MB"),
	TEXT("4MB"),
	TEXT("6MB"),
	TEXT("8MB"),
	TEXT("10MB"),
	TEXT("12MB"),
	TEXT("16MB"),
	TEXT("20MB"),
	TEXT("24MB"),
	TEXT("32MB"),
	TEXT("40MB"),
	TEXT("48MB"),
	TEXT("64MB"),
	TEXT("80MB"),
	TEXT("96MB"),
	TEXT("128MB"),
	TEXT("160MB"),
	TEXT("192MB"),
	TEXT("256MB"),
	TEXT("320MB"),
	TEXT("384MB"),
	//TEXT("512MB"),
	//TEXT("640MB"),
	//TEXT("768MB"),
	//TEXT("896MB"),
	//TEXT("1024MB"),
};



static LRESULT APIENTRY
PrefTiMidity3DialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	static int initflag = 1;
	int i, tmp;
	char *opt;
	switch (uMess){
   case WM_INITDIALOG:
#if defined(WINDRV_SETUP)
	//	EnableWindow(GetDlgItem(hwnd, IDC_COMBO_OUTPUT), FALSE);
	//	EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_OUTPUT_OPTIONS), FALSE);
		EnableWindow(GetDlgItem(hwnd, IDC_COMBO_OUTPUT_MODE), FALSE);
		EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_OUTPUT_FILE), FALSE);
		EnableWindow(GetDlgItem(hwnd, IDC_EDIT_OUTPUT_FILE), FALSE);
		EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_OUTPUT_FILE_DEL), FALSE);
		SendDlgItemMessage(hwnd,IDC_COMBO_OUTPUT,CB_RESETCONTENT,(WPARAM)0,(LPARAM)0);
		for(i=0;play_mode_list[i]!=0;i++){
			SendDlgItemMessage(hwnd,IDC_COMBO_OUTPUT,CB_INSERTSTRING,(WPARAM)-1,(LPARAM)play_mode_list[i]->id_name);
		}
		for(i=0;play_mode_list[i]!=0;i++){
			if(st_temp->opt_playmode[0]==play_mode_list[i]->id_character){
				tmp = i;
				break;
			}
		}
		SendDlgItemMessage(hwnd,IDC_COMBO_OUTPUT,CB_SETCURSEL,(WPARAM)tmp,(LPARAM)0);
#elif defined(KBTIM_SETUP) // || defined(WINDRV_SETUP)
		EnableWindow(GetDlgItem(hwnd, IDC_COMBO_OUTPUT), FALSE);
		EnableWindow(GetDlgItem(hwnd, IDC_COMBO_OUTPUT_MODE), FALSE);
		EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_OUTPUT_OPTIONS), FALSE);
		EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_OUTPUT_FILE), FALSE);
		EnableWindow(GetDlgItem(hwnd, IDC_EDIT_OUTPUT_FILE), FALSE);
		EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_OUTPUT_FILE_DEL), FALSE);
#else
		SendDlgItemMessage(hwnd,IDC_COMBO_OUTPUT,CB_RESETCONTENT,(WPARAM)0,(LPARAM)0);
		for(i=0;play_mode_list[i]!=0;i++){
			SendDlgItemMessage(hwnd,IDC_COMBO_OUTPUT,CB_INSERTSTRING,(WPARAM)-1,(LPARAM)play_mode_list[i]->id_name);
		}
		if (CurrentPlayerLanguage == LANGUAGE_JAPANESE)
			cb_info_IDC_COMBO_OUTPUT_MODE = cb_info_IDC_COMBO_OUTPUT_MODE_jp;
		else
			cb_info_IDC_COMBO_OUTPUT_MODE = cb_info_IDC_COMBO_OUTPUT_MODE_en;
		for(i=0;cb_info_IDC_COMBO_OUTPUT_MODE[i];i+=2){
			SendDlgItemMessage(hwnd,IDC_COMBO_OUTPUT_MODE,CB_INSERTSTRING,(WPARAM)-1,(LPARAM)cb_info_IDC_COMBO_OUTPUT_MODE[i]);
		}
		{
			int cb_num=0;
			for(cb_num=0;cb_info_IDC_COMBO_OUTPUT_MODE[cb_num] != NULL;cb_num+=2){
				SendDlgItemMessage(hwnd,IDC_COMBO_OUTPUT_MODE,CB_SETCURSEL,(WPARAM)0,(LPARAM)0);
				if(st_temp->auto_output_mode==(int)cb_info_IDC_COMBO_OUTPUT_MODE[cb_num+1]){
					SendDlgItemMessage(hwnd,IDC_COMBO_OUTPUT_MODE,CB_SETCURSEL,(WPARAM)cb_num/2,(LPARAM)0);
					break;
				}
			}
		}
		for(i=0;play_mode_list[i]!=0;i++){
			if(st_temp->opt_playmode[0]==play_mode_list[i]->id_character){
				tmp = i;
				break;
			}
		}
		SendDlgItemMessage(hwnd,IDC_COMBO_OUTPUT,CB_SETCURSEL,(WPARAM)tmp,(LPARAM)0);
		if(st_temp->auto_output_mode==0){
			if(st_temp->OutputName[0]=='\0')
				SetDlgItemText(hwnd,IDC_EDIT_OUTPUT_FILE,TEXT("output.wav"));
			else
				SetDlgItemText(hwnd,IDC_EDIT_OUTPUT_FILE,TEXT(st_temp->OutputName));
		} else
			SetDlgItemText(hwnd,IDC_EDIT_OUTPUT_FILE,st_temp->OutputDirName);	
		PostMessage(hwnd, WM_COMMAND, IDC_COMBO_OUTPUT_MODE, 0);	// force updating IDC_BUTTON_OUTPUT_FILE text
#endif
		opt = st_temp->opt_playmode + 1;	
		if(strchr(opt, 'U')){
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_ULAW,BM_SETCHECK,1,0);
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_ALAW,BM_SETCHECK,0,0);
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_LINEAR,BM_SETCHECK,0,0);
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_SIGNED,BM_SETCHECK,0,0);
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_UNSIGNED,BM_SETCHECK,1,0);
		} else if(strchr(opt, 'A')){
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_ULAW,BM_SETCHECK,0,0);
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_ALAW,BM_SETCHECK,1,0);
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_LINEAR,BM_SETCHECK,0,0);
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_SIGNED,BM_SETCHECK,0,0);
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_UNSIGNED,BM_SETCHECK,1,0);
		} else {
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_ULAW,BM_SETCHECK,0,0);
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_ALAW,BM_SETCHECK,0,0);
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_LINEAR,BM_SETCHECK,1,0);
		}
		// BANDWIDTH
		if (CurrentPlayerLanguage == LANGUAGE_JAPANESE)
		  cb_info_IDC_COMBO_BANDWIDTH = cb_info_IDC_COMBO_BANDWIDTH_jp;
		else
		  cb_info_IDC_COMBO_BANDWIDTH = cb_info_IDC_COMBO_BANDWIDTH_en;
		for (i = 0; i < cb_num_IDC_COMBO_BANDWIDTH; i++)
			SendDlgItemMessage(hwnd, IDC_COMBO_BANDWIDTH, CB_INSERTSTRING,
					(WPARAM) -1, (LPARAM) cb_info_IDC_COMBO_BANDWIDTH[i]);
///r
		if (strchr(opt, 'D')) {	// float 64-bit ?
			SendDlgItemMessage(hwnd, IDC_COMBO_BANDWIDTH, CB_SETCURSEL,
					(WPARAM) BANDWIDTH_F64BIT, (LPARAM) 0);
		} else if (strchr(opt, '6')) {	// 64-bit
			SendDlgItemMessage(hwnd, IDC_COMBO_BANDWIDTH, CB_SETCURSEL,
					(WPARAM) BANDWIDTH_64BIT, (LPARAM) 0);
		} else if (strchr(opt, 'f')) {	// float 32-bit ?
			SendDlgItemMessage(hwnd, IDC_COMBO_BANDWIDTH, CB_SETCURSEL,
					(WPARAM) BANDWIDTH_F32BIT, (LPARAM) 0);
		} else if (strchr(opt, '3')) {	// 32-bit
			SendDlgItemMessage(hwnd, IDC_COMBO_BANDWIDTH, CB_SETCURSEL,
					(WPARAM) BANDWIDTH_32BIT, (LPARAM) 0);
		} else if (strchr(opt, '2')) {	// 24-bit
			SendDlgItemMessage(hwnd, IDC_COMBO_BANDWIDTH, CB_SETCURSEL,
					(WPARAM) BANDWIDTH_24BIT, (LPARAM) 0);
		} else if (strchr(opt, '1')) {	// 16-bit
			SendDlgItemMessage(hwnd, IDC_COMBO_BANDWIDTH, CB_SETCURSEL,
					(WPARAM) BANDWIDTH_16BIT, (LPARAM) 0);
		} else {	// 8-bit
			SendDlgItemMessage(hwnd, IDC_COMBO_BANDWIDTH, CB_SETCURSEL,
					(WPARAM) BANDWIDTH_8BIT, (LPARAM) 0);
		}
		if(strchr(opt, 's')){
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_SIGNED,BM_SETCHECK,1,0);
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_UNSIGNED,BM_SETCHECK,0,0);
		} else {
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_SIGNED,BM_SETCHECK,0,0);
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_UNSIGNED,BM_SETCHECK,1,0);
		}
		if(strchr(opt, 'x')){
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_BYTESWAP,BM_SETCHECK,1,0);
		} else {
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_BYTESWAP,BM_SETCHECK,0,0);
		}
		if(strchr(opt, 'M')){
			SendDlgItemMessage(hwnd,IDC_RADIO_STEREO,BM_SETCHECK,0,0);
			SendDlgItemMessage(hwnd,IDC_RADIO_MONO,BM_SETCHECK,1,0);
		} else {
			SendDlgItemMessage(hwnd,IDC_RADIO_STEREO,BM_SETCHECK,1,0);
			SendDlgItemMessage(hwnd,IDC_RADIO_MONO,BM_SETCHECK,0,0);
		}

		// SAMPLE_RATE
		for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_SAMPLE_RATE_num); i++)
			CB_INSSTR(IDC_COMBO_SAMPLE_RATE,cb_info_IDC_COMBO_SAMPLE_RATE[i]);
		CB_SET(IDC_COMBO_SAMPLE_RATE, CB_FIND(cb_info_IDC_COMBO_SAMPLE_RATE_num, st_temp->output_rate, 9));

		// COMPUTE_BUFFER_BITS
		if (CurrentPlayerLanguage == LANGUAGE_JAPANESE){
			for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_COMPUTE_BUFFER_BITS_num); i++)
				CB_INSSTR(IDC_COMBO_COMPUTE_BUFFER_BITS, cb_info_IDC_COMBO_COMPUTE_BUFFER_BITS_jp[i]);
		}else{
			for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_COMPUTE_BUFFER_BITS_num); i++)
				CB_INSSTR(IDC_COMBO_COMPUTE_BUFFER_BITS, cb_info_IDC_COMBO_COMPUTE_BUFFER_BITS_en[i]);
		}
		CB_SET(IDC_COMBO_COMPUTE_BUFFER_BITS, CB_FIND(cb_info_IDC_COMBO_COMPUTE_BUFFER_BITS_num, st_temp->compute_buffer_bits, 0));

		// DATA_BLOCK
		if (CurrentPlayerLanguage == LANGUAGE_JAPANESE){
			for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_BUFFER_BITS_num); i++)
				CB_INSSTR(IDC_COMBO_BUFFER_BITS, cb_info_IDC_COMBO_BUFFER_BITS_jp[i]);
			for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_FRAGMENTS_num); i++)
				CB_INSSTR(IDC_COMBO_FRAGMENTS, cb_info_IDC_COMBO_FRAGMENTS_jp[i]);
		}else{
			for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_BUFFER_BITS_num); i++)
				CB_INSSTR(IDC_COMBO_BUFFER_BITS, cb_info_IDC_COMBO_BUFFER_BITS_en[i]);
			for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_FRAGMENTS_num); i++)
				CB_INSSTR(IDC_COMBO_FRAGMENTS, cb_info_IDC_COMBO_FRAGMENTS_en[i]);
		}
		CB_SET(IDC_COMBO_BUFFER_BITS, CB_FIND(cb_info_IDC_COMBO_BUFFER_BITS_num, st_temp->audio_buffer_bits, 5));
		CB_SET(IDC_COMBO_FRAGMENTS, CB_FIND(cb_info_IDC_COMBO_FRAGMENTS_num, st_temp->buffer_fragments, 9));

///r
#if defined(IA_W32G_SYN) || defined(WINDRV_SETUP)
		EnableWindow(GetDlgItem(hwnd, IDC_EDIT_AUDIO_BUFFER_MAX), FALSE);
		EnableWindow(GetDlgItem(hwnd, IDC_EDIT_AUDIO_BUFFER_FILL), FALSE);
		EnableWindow(GetDlgItem(hwnd, IDC_CHECKBOX_REDUCE_POLYPHONY), FALSE);
		EnableWindow(GetDlgItem(hwnd, IDC_EDIT_REDUCE_POLYPHONY), FALSE);
		EnableWindow(GetDlgItem(hwnd, IDC_CHECKBOX_REDUCE_VOICE), FALSE);
		EnableWindow(GetDlgItem(hwnd, IDC_EDIT_REDUCE_VOICE), FALSE);
		EnableWindow(GetDlgItem(hwnd, IDC_CHECKBOX_REDUCE_QUALITY), FALSE);
		EnableWindow(GetDlgItem(hwnd, IDC_EDIT_REDUCE_QUALITY), FALSE);
#else
		// AUDIO_BUFFER
		{
		char *max_buff = safe_strdup(st_temp->opt_qsize);
		char *fill_buff = strchr(max_buff, '/');
		if(fill_buff)
			*fill_buff = '\0', ++ fill_buff;
		SetDlgItemText(hwnd, IDC_EDIT_AUDIO_BUFFER_MAX, max_buff);
		if(fill_buff)
			SetDlgItemText(hwnd, IDC_EDIT_AUDIO_BUFFER_FILL, fill_buff);
		safe_free(max_buff);
		}
		// auto_reduce_polyphony
	//	DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_REDUCE_POLYPHONY,st_temp->auto_reduce_polyphony);
		// reduce_polyphony_threshold
		tmp = atof(st_temp->reduce_polyphony_threshold);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_REDUCE_POLYPHONY, (tmp!=0));
		SetDlgItemText(hwnd, IDC_EDIT_REDUCE_POLYPHONY, st_temp->reduce_polyphony_threshold);
		EnableWindow(GetDlgItem(hwnd, IDC_EDIT_REDUCE_POLYPHONY), tmp);
		// reduce_voice_threshold
		tmp = atof(st_temp->reduce_voice_threshold);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_REDUCE_VOICE, tmp);
		SetDlgItemText(hwnd, IDC_EDIT_REDUCE_VOICE, st_temp->reduce_voice_threshold);
		EnableWindow(GetDlgItem(hwnd, IDC_EDIT_REDUCE_VOICE), tmp);
		// reduce_quality_threshold
		tmp = atof(st_temp->reduce_quality_threshold);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_REDUCE_QUALITY, tmp);
		SetDlgItemText(hwnd,IDC_EDIT_REDUCE_QUALITY, st_temp->reduce_quality_threshold);
		EnableWindow(GetDlgItem(hwnd, IDC_EDIT_REDUCE_QUALITY), tmp);
#endif

		// others
		for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_CACHE_SIZE_num); i++)
			CB_INSSTR(IDC_COMBO_CACHE_SIZE, cb_info_IDC_COMBO_CACHE_SIZE[i]);
		CB_SET(IDC_COMBO_CACHE_SIZE, CB_FIND(cb_info_IDC_COMBO_CACHE_SIZE_num, st_temp->allocate_cache_size, 2));
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_PRE_RESAMPLE,st_temp->opt_pre_resamplation);
#if defined(IA_W32G_SYN) || defined(WINDRV_SETUP)
		EnableWindow(GetDlgItem(hwnd, IDC_CHECKBOX_LOADINST_PLAYING), FALSE);
#else
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_LOADINST_PLAYING,st_temp->opt_realtime_playing);
#endif		
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_FREE_INST,st_temp->free_instruments_afterwards);
		DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_LOAD_ALL_INST,st_temp->opt_load_all_instrument);

		SendMessage(hwnd,WM_MYRESTORE,(WPARAM)0,(LPARAM)0); // buffer time
		initflag = 0;
		break;
	case WM_MYRESTORE:
		{
			double tmp; //compute_buffer_time, device_buffer_time;
			int tmpi1, tmpi2;
#ifdef ALIGN_SIZE
			const int min_compute_sample = 8;
			uint32 min_compute_sample_mask = ~(min_compute_sample - 1);
#endif
			if(st_temp->compute_buffer_bits < 0){
#ifdef ALIGN_SIZE
				tmpi1 = tmpi2 = ((double)st_temp->output_rate / 1000.0) * abs(st_temp->compute_buffer_bits);
				tmpi1 &= min_compute_sample_mask;
				if(tmpi1 < tmpi2)
					tmpi1 += min_compute_sample; // >=1ms
#else	// ! ALIGN_SIZE		
				tmpi1 = (double)st_temp->output_rate / 1000.0;
#endif // ALIGN_SIZE
			}else{
				tmpi1 = 1<<st_temp->compute_buffer_bits;
			}			
			SetDlgItemInt(hwnd, IDC_EDIT_COMPUTE_BUFFER_SIZE, tmpi1, TRUE);
			tmp = 1000.0 * (double)tmpi1 / (double)st_temp->output_rate;
			SetDlgItemFloat(hwnd, IDC_EDIT_COMPUTE_BUFFER_TIME, tmp);
			tmp = 1000.0 * (double)(1<<st_temp->audio_buffer_bits) / (double)st_temp->output_rate;
			SetDlgItemFloat(hwnd, IDC_EDIT_DEVICE_BUFFER_TIME, tmp);
			tmp = tmp * (double)st_temp->buffer_fragments;
			SetDlgItemFloat(hwnd, IDC_EDIT_DEVICE_BUFFER_TOTAL_TIME, tmp);
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		 case IDCLOSE:
			break;
		 case IDC_COMBO_SAMPLE_RATE:
		 case IDC_COMBO_COMPUTE_BUFFER_BITS:
		 case IDC_COMBO_BUFFER_BITS:
		 case IDC_COMBO_FRAGMENTS:
		 case IDC_COMBO_CACHE_SIZE:
			SendMessage(hwnd,WM_MYSAVE,(WPARAM)0,(LPARAM)0);
			SendMessage(hwnd,WM_MYRESTORE,(WPARAM)0,(LPARAM)0); // buffer time
			break;
		case IDC_BUTTON_OUTPUT_FILE:
			{
			char filename[FILEPATH_MAX];
			filename[0] = filename[FILEPATH_MAX - 1] = '\0';
			EB_GETTEXTA(IDC_EDIT_OUTPUT_FILE, filename, FILEPATH_MAX - 1);
			if (st_temp->auto_output_mode > 0) {
			    if (!DlgOpenOutputDir(filename, hwnd) && filename[0] != '\0')
				EB_SETTEXTA(IDC_EDIT_OUTPUT_FILE, filename);
			}
			else {
			    if (!DlgOpenOutputFile(filename, hwnd) && filename[0] != '\0')
				EB_SETTEXTA(IDC_EDIT_OUTPUT_FILE, filename);
			}
			}
			break;
		case IDC_BUTTON_OUTPUT_FILE_DEL:
		    {
			char filename[FILEPATH_MAX];
			DWORD res;
			if (st_temp->auto_output_mode > 0) {
				break;
			}
			filename[0] = filename[FILEPATH_MAX - 1] = '\0';
			EB_GETTEXTA(IDC_EDIT_OUTPUT_FILE, filename, FILEPATH_MAX - 1);
			res = GetFileAttributesA(filename);
			if (res != 0xFFFFFFFF && !(res & FILE_ATTRIBUTE_DIRECTORY)) {
				if (DeleteFileA(filename) != TRUE) {
					char buffer[FILEPATH_MAX + 128];
					sprintf(buffer, "Can't delete file %s !", filename);
					MessageBoxA(NULL, buffer, "Error!", MB_OK);
				} else {
					char buffer[FILEPATH_MAX + 128];
					sprintf(buffer, "Delete file %s !", filename);
					MessageBoxA(NULL, buffer, "Delete!", MB_OK);
				}
			}
		    }
		    break;
		case IDC_CHECKBOX_ULAW:
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_ULAW,BM_GETCHECK,0,0)){
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ULAW,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ALAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_LINEAR,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd, IDC_COMBO_BANDWIDTH, CB_SETCURSEL,
					(WPARAM) BANDWIDTH_8BIT, (LPARAM) 0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_SIGNED,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_UNSIGNED,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_BYTESWAP,BM_SETCHECK,0,0);
			 } else {
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ULAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ALAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_LINEAR,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_SIGNED,BM_SETCHECK,1,0);
			 }
			break;
		case IDC_CHECKBOX_ALAW:
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_ALAW,BM_GETCHECK,0,0)){
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ULAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ALAW,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_LINEAR,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd, IDC_COMBO_BANDWIDTH, CB_SETCURSEL,
					(WPARAM) BANDWIDTH_8BIT, (LPARAM) 0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_SIGNED,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_UNSIGNED,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_BYTESWAP,BM_SETCHECK,0,0);
			 } else {
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ULAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ALAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_LINEAR,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_SIGNED,BM_SETCHECK,1,0);
			 }
			break;
		case IDC_CHECKBOX_LINEAR:
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_LINEAR,BM_GETCHECK,0,0)){
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ULAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ALAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_LINEAR,BM_SETCHECK,1,0);
			 } else {
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_LINEAR,BM_SETCHECK,1,0);
			 }
			break;
		case IDC_CHECKBOX_SIGNED:
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_SIGNED,BM_GETCHECK,0,0)){
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_SIGNED,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_UNSIGNED,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ULAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ALAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_LINEAR,BM_SETCHECK,1,0);
			 } else {
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_SIGNED,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_UNSIGNED,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ULAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ALAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_LINEAR,BM_SETCHECK,1,0);
			 }
			break;
		case IDC_CHECKBOX_UNSIGNED:
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_UNSIGNED,BM_GETCHECK,0,0)){
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_SIGNED,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_UNSIGNED,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ULAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ALAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_LINEAR,BM_SETCHECK,1,0);
			} else {
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_SIGNED,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_UNSIGNED,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ULAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ALAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_LINEAR,BM_SETCHECK,1,0);
			 }
			break;
		case IDC_CHECKBOX_BYTESWAP:
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_BYTESWAP,BM_GETCHECK,0,0)){
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_BYTESWAP,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ULAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ALAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_LINEAR,BM_SETCHECK,1,0);
			} else {
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_BYTESWAP,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ULAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ALAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_LINEAR,BM_SETCHECK,1,0);
			}
			break;
		case IDC_RADIO_STEREO:
			if(SendDlgItemMessage(hwnd,IDC_RADIO_STEREO,BM_GETCHECK,0,0)){
				SendDlgItemMessage(hwnd,IDC_RADIO_STEREO,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_RADIO_MONO,BM_SETCHECK,0,0);
			 } else {
				SendDlgItemMessage(hwnd,IDC_RADIO_STEREO,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_RADIO_MONO,BM_SETCHECK,1,0);
			 }
			break;
		case IDC_RADIO_MONO:
			if(SendDlgItemMessage(hwnd,IDC_RADIO_MONO,BM_GETCHECK,0,0)){
				SendDlgItemMessage(hwnd,IDC_RADIO_STEREO,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_RADIO_MONO,BM_SETCHECK,1,0);
		 } else {
				SendDlgItemMessage(hwnd,IDC_RADIO_STEREO,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_RADIO_MONO,BM_SETCHECK,0,0);
		 }
			break;
		case IDC_BUTTON_OUTPUT_OPTIONS:
			{
				int num;
				num = CB_GET(IDC_COMBO_OUTPUT);
				if (num >= 0) {
					st_temp->opt_playmode[0] = play_mode_list[num]->id_character;
				} else {
					st_temp->opt_playmode[0] = 'd';
				}
				if (st_temp->opt_playmode[0] == 'w') {
					waveConfigDialog();
					break;
				}
#ifdef AU_W32
				if (st_temp->opt_playmode[0] == 'd') {
					wmmeConfigDialog(hPrefWnd);
					break;
				}
#endif
				
#ifdef AU_WASAPI
				if (st_temp->opt_playmode[0] == 'x') {
					wasapiConfigDialog();
					break;
				}
#endif
				
#ifdef AU_WDMKS
				if (st_temp->opt_playmode[0] == 'k') {
					wdmksConfigDialog();
					break;
				}
#endif

#ifdef AU_VORBIS
				if (st_temp->opt_playmode[0] == 'v') {
					vorbisConfigDialog();
					break;
				}
#endif
#ifdef AU_GOGO
				if (st_temp->opt_playmode[0] == 'g') {
					gogoConfigDialog();
					break;
				}
#endif
#ifdef AU_PORTAUDIO_DLL
				if (st_temp->opt_playmode[0] == 'o'
					|| st_temp->opt_playmode[0] == 'P' || st_temp->opt_playmode[0] == 'p'
					|| st_temp->opt_playmode[0] == 'W' || st_temp->opt_playmode[0] == 'K')
				{
					portaudioConfigDialog();
					break;
				}
#endif
#ifdef AU_LAME
				if (st_temp->opt_playmode[0] == 'L') {
					lameConfigDialog();
					break;
				}
#endif
#ifdef AU_FLAC
				if (st_temp->opt_playmode[0] == 'F') {
					flacConfigDialog();
					break;
				}
#endif
				MessageBeep(-1); //Not Supported.
			}
			break;
		case IDC_COMBO_OUTPUT_MODE:
		{
			int cb_num1, cb_num2;
			cb_num1 = SendDlgItemMessage(hwnd,IDC_COMBO_OUTPUT_MODE,CB_GETCURSEL,(WPARAM)0,(LPARAM)0);
			if (CurrentPlayerLanguage == LANGUAGE_JAPANESE)
				cb_info_IDC_COMBO_OUTPUT_MODE = cb_info_IDC_COMBO_OUTPUT_MODE_jp;
			else
				cb_info_IDC_COMBO_OUTPUT_MODE = cb_info_IDC_COMBO_OUTPUT_MODE_en;
			for(cb_num2=0;(int)cb_info_IDC_COMBO_OUTPUT_MODE[cb_num2];cb_num2+=2){
				if(cb_num1*2==cb_num2){
					st_temp->auto_output_mode = (int)cb_info_IDC_COMBO_OUTPUT_MODE[cb_num2+1];
					break;
				}
			}
			if (CurrentPlayerLanguage == LANGUAGE_JAPANESE) {
				if(st_temp->auto_output_mode>0){
				SendDlgItemMessage(hwnd,IDC_BUTTON_OUTPUT_FILE,WM_SETTEXT,0,(LPARAM)"出力先");
				SetDlgItemText(hwnd,IDC_EDIT_OUTPUT_FILE,st_temp->OutputDirName);
				} else {
				SendDlgItemMessage(hwnd,IDC_BUTTON_OUTPUT_FILE,WM_SETTEXT,0,(LPARAM)"出力ファイル");
				SetDlgItemText(hwnd,IDC_EDIT_OUTPUT_FILE,st_temp->OutputName);
				}
			} else {
				if(st_temp->auto_output_mode>0){
				SendDlgItemMessage(hwnd,IDC_BUTTON_OUTPUT_FILE,WM_SETTEXT,0,(LPARAM)"Output Dir");
				SetDlgItemText(hwnd,IDC_EDIT_OUTPUT_FILE,st_temp->OutputDirName);
				} else {
				SendDlgItemMessage(hwnd,IDC_BUTTON_OUTPUT_FILE,WM_SETTEXT,0,(LPARAM)"Output File");
				SetDlgItemText(hwnd,IDC_EDIT_OUTPUT_FILE,st_temp->OutputName);
				}
			}
		}
			break;
///r
#if !(defined(IA_W32G_SYN) || defined(WINDRV_SETUP))
		case IDC_CHECKBOX_REDUCE_POLYPHONY:
			if(SendDlgItemMessage(hwnd, IDC_CHECKBOX_REDUCE_POLYPHONY, BM_GETCHECK, 0, 0)){
				SetDlgItemText(hwnd,IDC_EDIT_REDUCE_POLYPHONY, TEXT("85%"));
				EnableWindow(GetDlgItem(hwnd, IDC_EDIT_REDUCE_POLYPHONY), TRUE);
			}else{
				SetDlgItemInt(hwnd,IDC_EDIT_REDUCE_POLYPHONY, 0,TRUE);
				EnableWindow(GetDlgItem(hwnd, IDC_EDIT_REDUCE_POLYPHONY), FALSE);
			}
			break;
		case IDC_CHECKBOX_REDUCE_VOICE:
			if(SendDlgItemMessage(hwnd, IDC_CHECKBOX_REDUCE_VOICE, BM_GETCHECK, 0, 0)){
				SetDlgItemText(hwnd,IDC_EDIT_REDUCE_VOICE, TEXT("75%"));
				EnableWindow(GetDlgItem(hwnd, IDC_EDIT_REDUCE_VOICE), TRUE);
			}else{
				SetDlgItemInt(hwnd,IDC_EDIT_REDUCE_VOICE, 0,TRUE);
				EnableWindow(GetDlgItem(hwnd, IDC_EDIT_REDUCE_VOICE), FALSE);
			}
			break;
		case IDC_CHECKBOX_REDUCE_QUALITY:
			if(SendDlgItemMessage(hwnd, IDC_CHECKBOX_REDUCE_QUALITY, BM_GETCHECK, 0, 0)){
				SetDlgItemText(hwnd,IDC_EDIT_REDUCE_QUALITY, TEXT("99%"));
				EnableWindow(GetDlgItem(hwnd, IDC_EDIT_REDUCE_QUALITY), TRUE);
			}else{
				SetDlgItemInt(hwnd,IDC_EDIT_REDUCE_QUALITY, 0,TRUE);
				EnableWindow(GetDlgItem(hwnd, IDC_EDIT_REDUCE_QUALITY), FALSE);
			}
			break;
#endif
		default:
			break;
	  }
		PrefWndSetOK = 1;
		break;
	case WM_MYSAVE:
		if ( initflag ) break;
		// OUTPUT
		i = 0;
#if defined(KBTIM_SETUP) // || defined(TIMDRV_SETUP)
		st_temp->opt_playmode[i++]='d';
#else
		tmp = SendDlgItemMessage(hwnd,IDC_COMBO_OUTPUT,CB_GETCURSEL,(WPARAM)0,(LPARAM)0);
		if(tmp>=0){
			st_temp->opt_playmode[i++]=play_mode_list[tmp]->id_character;
		} else {
			st_temp->opt_playmode[i++]='d';
		}
#endif
///r
		tmp = SendDlgItemMessage(hwnd, IDC_COMBO_BANDWIDTH, CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
		if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_ULAW,BM_GETCHECK,0,0)){
			st_temp->opt_playmode[i++] = 'U';
			st_temp->opt_playmode[i++] = '8';
		}else if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_ALAW,BM_GETCHECK,0,0)){
			st_temp->opt_playmode[i++] = 'A';
			st_temp->opt_playmode[i++] = '8';
		}else if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_LINEAR,BM_GETCHECK,0,0)){
			st_temp->opt_playmode[i++] = 'l';
			if(tmp == BANDWIDTH_F64BIT)
				st_temp->opt_playmode[i++] = 'D';
			else if(tmp == BANDWIDTH_F32BIT)
				st_temp->opt_playmode[i++] = 'f';
			else if(tmp == BANDWIDTH_64BIT)
				st_temp->opt_playmode[i++] = '6';
			else if(tmp == BANDWIDTH_32BIT)
				st_temp->opt_playmode[i++] = '3';
			else if(tmp == BANDWIDTH_24BIT)
				st_temp->opt_playmode[i++] = '2';
			else if(tmp == BANDWIDTH_16BIT)
				st_temp->opt_playmode[i++] = '1';
			else if(tmp == BANDWIDTH_8BIT)
				st_temp->opt_playmode[i++] = '8';
			else	// 16-bit
				st_temp->opt_playmode[i++] = '1';
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_SIGNED,BM_GETCHECK,0,0))
				st_temp->opt_playmode[i++] = 's';
			else if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_UNSIGNED,BM_GETCHECK,0,0))
				st_temp->opt_playmode[i++] = 'u';
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_BYTESWAP,BM_GETCHECK,0,0))
				st_temp->opt_playmode[i++] = 'x';
		}
		if(SendDlgItemMessage(hwnd,IDC_RADIO_STEREO,BM_GETCHECK,0,0))
			st_temp->opt_playmode[i++] = 'S';
		else if(SendDlgItemMessage(hwnd,IDC_RADIO_MONO,BM_GETCHECK,0,0))
			st_temp->opt_playmode[i++] = 'M';
		st_temp->opt_playmode[i] = '\0';
		// SAMPLE_RATE
		st_temp->output_rate = cb_info_IDC_COMBO_SAMPLE_RATE_num[CB_GETS(IDC_COMBO_SAMPLE_RATE, 9)];

		// COMPUTE_BUFFER_BITS
		st_temp->compute_buffer_bits = cb_info_IDC_COMBO_COMPUTE_BUFFER_BITS_num[CB_GETS(IDC_COMBO_COMPUTE_BUFFER_BITS, 0)];

		// BUFFER  DATA_BLOCK
		st_temp->audio_buffer_bits = cb_info_IDC_COMBO_BUFFER_BITS_num[CB_GETS(IDC_COMBO_BUFFER_BITS, 5)];
		st_temp->buffer_fragments = cb_info_IDC_COMBO_FRAGMENTS_num[CB_GETS(IDC_COMBO_FRAGMENTS, 9)];

		if(st_temp->compute_buffer_bits >= st_temp->audio_buffer_bits){
			st_temp->compute_buffer_bits = 6;
			st_temp->audio_buffer_bits = 12;
		}

 		if(st_temp->auto_output_mode==0)
			GetDlgItemText(hwnd,IDC_EDIT_OUTPUT_FILE,st_temp->OutputName,(WPARAM)sizeof(st_temp->OutputName));
		else
			GetDlgItemText(hwnd,IDC_EDIT_OUTPUT_FILE,st_temp->OutputDirName,(WPARAM)sizeof(st_temp->OutputDirName));

///r
#if !(defined(IA_W32G_SYN) || defined(WINDRV_SETUP))
		// AUDIO_BUFFER
		{
		char max_buff[15], fill_buff[15];
		GetDlgItemText(hwnd,IDC_EDIT_AUDIO_BUFFER_MAX, max_buff, (WPARAM)sizeof(max_buff));
		if(strlen(max_buff) == 0)
			strcpy(max_buff, "5.0");
		GetDlgItemText(hwnd,IDC_EDIT_AUDIO_BUFFER_FILL, fill_buff, (WPARAM)sizeof(fill_buff));
		if(strlen(fill_buff) == 0)
			strcpy(fill_buff, "100%");
		snprintf(st_temp->opt_qsize,sizeof(st_temp->opt_qsize),"%s/%s", max_buff, fill_buff);
		}
		// reduce_polyphony_threshold
		DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_REDUCE_POLYPHONY,tmp);
		if(!tmp)
			snprintf(st_temp->reduce_polyphony_threshold,sizeof(st_temp->reduce_polyphony_threshold),"0");
		else
			GetDlgItemText(hwnd,IDC_EDIT_REDUCE_POLYPHONY, st_temp->reduce_polyphony_threshold, (WPARAM)sizeof(st_temp->reduce_polyphony_threshold));
		// reduce_voice_threshold
		DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_REDUCE_VOICE,tmp);
		if(!tmp)
			snprintf(st_temp->reduce_voice_threshold,sizeof(st_temp->reduce_voice_threshold),"0");
		else
			GetDlgItemText(hwnd,IDC_EDIT_REDUCE_VOICE, st_temp->reduce_voice_threshold, (WPARAM)sizeof(st_temp->reduce_voice_threshold));
		// reduce_quality_threshold
		DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_REDUCE_QUALITY,tmp);
		if(!tmp)
			snprintf(st_temp->reduce_quality_threshold,sizeof(st_temp->reduce_quality_threshold),"0");
		else
			GetDlgItemText(hwnd,IDC_EDIT_REDUCE_QUALITY, st_temp->reduce_quality_threshold, (WPARAM)sizeof(st_temp->reduce_quality_threshold));
#endif

		// others
		st_temp->allocate_cache_size = cb_info_IDC_COMBO_CACHE_SIZE_num[CB_GETS(IDC_COMBO_CACHE_SIZE, 2)];		
		DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_PRE_RESAMPLE,st_temp->opt_pre_resamplation);
#if !(defined(IA_W32G_SYN) || defined(WINDRV_SETUP))
		DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_LOADINST_PLAYING,st_temp->opt_realtime_playing);
#endif
		DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_FREE_INST,st_temp->free_instruments_afterwards);
		DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_LOAD_ALL_INST,st_temp->opt_load_all_instrument);

		SetWindowLongPtr(hwnd,DWLP_MSGRESULT,FALSE);
		break;
  case WM_SIZE:
		return FALSE;
	case WM_DESTROY:
		break;
	default:
	  break;
	}
	return FALSE;
}


// IDC_COMBO_REVC_EDNUMCOMBS
static int cb_info_IDC_COMBO_REVC_EDNUMCOMBS_num[] = {
	4,
	6,
	8,
	12,
	16,
	20,
	24,
	32,
	40,
	48,
	56,
	64,
};
static const TCHAR *cb_info_IDC_COMBO_REVC_EDNUMCOMBS[] = {
	TEXT("4"),
	TEXT("6"),
	TEXT("8"),
	TEXT("12"),
	TEXT("16"),
	TEXT("20"),
	TEXT("24"),
	TEXT("32"),
	TEXT("40"),
	TEXT("48"),
	TEXT("56"),
	TEXT("64"),
};

// IDC_COMBO_REVC_EX_RV_NUM
static int cb_info_IDC_COMBO_REVC_EX_RV_NUM_num[] = {
	8,
	12,
	16,
	20,
	24,
	32,
	40,
	48,
	56,
	64,
};
static const TCHAR *cb_info_IDC_COMBO_REVC_EX_RV_NUM[] = {
	TEXT("8"),
	TEXT("12"),
	TEXT("16"),
	TEXT("20"),
	TEXT("24"),
	TEXT("32"),
	TEXT("40"),
	TEXT("48"),
	TEXT("56"),
	TEXT("64"),
};

// IDC_COMBO_REVC_EX_AP_NUM
static int cb_info_IDC_COMBO_REVC_EX_AP_NUM_num[] = {
	0,
	4,
	8,
};
static const TCHAR *cb_info_IDC_COMBO_REVC_EX_AP_NUM[] = {
	TEXT("0"),
	TEXT("4"),
	TEXT("8"),
};

// IDC_SFOW_EFX_REV_TYPE
static int cb_info_IDC_SFOW_EFX_REV_TYPE_num[] = {
	0,
	1,
};

static const TCHAR *cb_info_IDC_SFOW_EFX_REV_TYPE[] = {
	TEXT("Freeverb"),
	TEXT("ReverbEX"),
};

// IDC_COMBO_REVC_SR_RS_MODE
static int cb_info_IDC_COMBO_REVC_SR_RS_MODE_num[] = {
	0,
	1,
	2,
	3,
};
static const TCHAR *cb_info_IDC_COMBO_REVC_SR_RS_MODE[] = {
	TEXT("OFF"),
	TEXT("SR1/2"),
	TEXT("SR1/4"),
	TEXT("SR1/8"),
};

// IDC_COMBO_REVC_SR_FFT_MODE
static int cb_info_IDC_COMBO_REVC_SR_FFT_MODE_num[] = {
	0,
	1,
	2,
};
static const TCHAR *cb_info_IDC_COMBO_REVC_SR_FFT_MODE[] = {
	TEXT("OFF"),
	TEXT("Zero Latency"),
	TEXT("Large Latency"),
};

// IDC_CHOC_EX_PHASE_NUM
static int cb_info_IDC_CHOC_EX_PHASE_NUM_num[] = {
	1,
	2,
	3,
	4,
	5,
	6,
	7,
	8,
};

static const TCHAR *cb_info_IDC_CHOC_EX_PHASE_NUM[] = {
	TEXT("1"),
	TEXT("2"),
	TEXT("3"),
	TEXT("4"),
	TEXT("5"),
	TEXT("6"),
	TEXT("7"),
	TEXT("8"),
};


static LRESULT APIENTRY CALLBACK PrefSFINI1DialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess) {
	case WM_INITDIALOG:

		DLG_FLAG_TO_CHECKBUTTON(hwnd, IDC_CHK_OWRITE_VIB, otd.overwriteMode & EOWM_ENABLE_VIBRATO);
		DLG_FLAG_TO_CHECKBUTTON(hwnd, IDC_CHK_OWRITE_TRM, otd.overwriteMode & EOWM_ENABLE_TREMOLO);
		DLG_FLAG_TO_CHECKBUTTON(hwnd, IDC_CHK_OWRITE_CUTRESO, otd.overwriteMode & EOWM_ENABLE_CUTOFF);
		DLG_FLAG_TO_CHECKBUTTON(hwnd, IDC_CHK_OWRITE_MODENV, otd.overwriteMode & EOWM_ENABLE_MOD);
		DLG_FLAG_TO_CHECKBUTTON(hwnd, IDC_CHK_OWRITE_ENV, otd.overwriteMode & EOWM_ENABLE_ENV);
		DLG_FLAG_TO_CHECKBUTTON(hwnd, IDC_CHK_OWRITE_VEL, otd.overwriteMode & EOWM_ENABLE_VEL);
			
		// soundfont override
		SetDlgItemInt(hwnd, IDC_SFOW_VIBDELAY, OverrideSample.vibrato_delay, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_VIBDEPTH, OverrideSample.vibrato_to_pitch, TRUE);

		SetDlgItemInt(hwnd, IDC_SFOW_TRMDELAY, OverrideSample.tremolo_delay, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_TRMDEPTH, OverrideSample.tremolo_to_amp, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_TRMFC, OverrideSample.tremolo_to_fc, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_TRMPITCH, OverrideSample.tremolo_to_pitch, TRUE);

		SetDlgItemInt(hwnd, IDC_SFOW_VELFC, OverrideSample.vel_to_fc, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_VELTHR, OverrideSample.vel_to_fc_threshold, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_VELRES, OverrideSample.vel_to_resonance, TRUE);

		SetDlgItemInt(hwnd, IDC_SFOW_MODENVFC, OverrideSample.modenv_to_fc, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_MODENVDELAY, OverrideSample.modenv_delay, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_MODENVPITCH, OverrideSample.modenv_to_pitch, TRUE);

		SetDlgItemInt(hwnd, IDC_SFOW_CUTOFF, OverrideSample.cutoff_freq, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_RESONANCE, OverrideSample.resonance, TRUE);

		SetDlgItemInt(hwnd, IDC_SFOW_ENVDELAY, OverrideSample.envelope_delay, TRUE);

		// soundfont attenuation
		DLG_FLAG_TO_CHECKBUTTON(hwnd, IDC_SFATT_NEG, sf_attenuation_neg);
		SetDlgItemFloat(hwnd, IDC_SFATT_POW, sf_attenuation_pow);
		SetDlgItemFloat(hwnd, IDC_SFATT_MUL, sf_attenuation_mul);
		SetDlgItemFloat(hwnd, IDC_SFATT_ADD, sf_attenuation_add);

		// soundfont limit
		SetDlgItemInt(hwnd, IDC_SFL_VOLENV_ATK, sf_limit_volenv_attack, TRUE);
		SetDlgItemInt(hwnd, IDC_SFL_MODENV_ATK, sf_limit_modenv_attack, TRUE);
		SetDlgItemInt(hwnd, IDC_SFL_MODENV_FC, sf_limit_modenv_fc, TRUE);
		SetDlgItemInt(hwnd, IDC_SFL_MODENV_PIT, sf_limit_modenv_pitch, TRUE);
		SetDlgItemInt(hwnd, IDC_SFL_MODLFO_FC, sf_limit_modlfo_fc, TRUE);
		SetDlgItemInt(hwnd, IDC_SFL_MODLFO_PIT, sf_limit_modlfo_pitch, TRUE);
		SetDlgItemInt(hwnd, IDC_SFL_VIBLFO_PIT, sf_limit_viblfo_pitch, TRUE);
		SetDlgItemInt(hwnd, IDC_SFL_MODLFO_FREQ, sf_limit_modlfo_freq, TRUE);
		SetDlgItemInt(hwnd, IDC_SFL_VIBLFO_FREQ, sf_limit_viblfo_freq, TRUE);
		
		// soundfont default
		SetDlgItemInt(hwnd, IDC_SFD_MODLFO_FREQ, sf_default_modlfo_freq, TRUE);
		SetDlgItemInt(hwnd, IDC_SFD_VIBLFO_FREQ, sf_default_viblfo_freq, TRUE);
		
		// soundfont config
		DLG_FLAG_TO_CHECKBUTTON(hwnd, IDC_SFC_LFO_SWAP, sf_config_lfo_swap);
		DLG_FLAG_TO_CHECKBUTTON(hwnd, IDC_SFC_ADRS_OFFSET, sf_config_addrs_offset);
		
		return FALSE;
	case WM_COMMAND:
		break;
	case WM_MYSAVE:
#ifdef CHECKRANGE_SFINI_PARAM
#undef CHECKRANGE_SFINI_PARAM
#endif
#define CHECKRANGE_SFINI_PARAM(SV, SMIN,SMAX) SV = ((SV < SMIN) ?SMIN : ((SV > SMAX) ? SMAX : SV))

		otd.overwriteMode = 0;
		otd.overwriteMode |= (SendDlgItemMessage(hwnd, IDC_CHK_OWRITE_VIB, BM_GETCHECK, 0, 0)?1:0) * EOWM_ENABLE_VIBRATO;
		otd.overwriteMode |= (SendDlgItemMessage(hwnd, IDC_CHK_OWRITE_TRM, BM_GETCHECK, 0, 0)?1:0) * EOWM_ENABLE_TREMOLO;
		otd.overwriteMode |= (SendDlgItemMessage(hwnd, IDC_CHK_OWRITE_ENV, BM_GETCHECK, 0, 0)?1:0) * EOWM_ENABLE_ENV;
		otd.overwriteMode |= (SendDlgItemMessage(hwnd, IDC_CHK_OWRITE_MODENV, BM_GETCHECK, 0, 0)?1:0) * EOWM_ENABLE_MOD;
		otd.overwriteMode |= (SendDlgItemMessage(hwnd, IDC_CHK_OWRITE_CUTRESO, BM_GETCHECK, 0, 0)?1:0) * EOWM_ENABLE_CUTOFF;
		otd.overwriteMode |= (SendDlgItemMessage(hwnd, IDC_CHK_OWRITE_VEL, BM_GETCHECK, 0, 0)?1:0) * EOWM_ENABLE_VEL;
		
		// soundfont override
		OverrideSample.vibrato_delay = (int)GetDlgItemInt(hwnd, IDC_SFOW_VIBDELAY, NULL, TRUE);
		OverrideSample.vibrato_to_pitch = (int)GetDlgItemInt(hwnd, IDC_SFOW_VIBDEPTH, NULL, TRUE);
		CHECKRANGE_SFINI_PARAM(OverrideSample.vibrato_delay, 0, 2000);
		CHECKRANGE_SFINI_PARAM(OverrideSample.vibrato_to_pitch, 0, 600);

		OverrideSample.tremolo_delay = (int)GetDlgItemInt(hwnd, IDC_SFOW_TRMDELAY, NULL, TRUE);
		OverrideSample.tremolo_to_amp = (int)GetDlgItemInt(hwnd, IDC_SFOW_TRMDEPTH, NULL, TRUE);
		OverrideSample.tremolo_to_fc = (int)GetDlgItemInt(hwnd, IDC_SFOW_TRMFC, NULL, TRUE);
		OverrideSample.tremolo_to_pitch = (int)GetDlgItemInt(hwnd, IDC_SFOW_TRMPITCH, NULL, TRUE);
		CHECKRANGE_SFINI_PARAM(OverrideSample.tremolo_delay, 0, 1000);
		CHECKRANGE_SFINI_PARAM(OverrideSample.tremolo_to_amp, 0, 256);
		CHECKRANGE_SFINI_PARAM(OverrideSample.tremolo_to_fc, -12000, 12000);
		CHECKRANGE_SFINI_PARAM(OverrideSample.tremolo_to_pitch, -12000, 12000);

		OverrideSample.vel_to_fc = (int)GetDlgItemInt(hwnd, IDC_SFOW_VELFC, NULL, TRUE);
		OverrideSample.vel_to_fc_threshold = (int)GetDlgItemInt(hwnd, IDC_SFOW_VELTHR, NULL, TRUE);
		OverrideSample.vel_to_resonance = (int)GetDlgItemInt(hwnd, IDC_SFOW_VELRES, NULL, TRUE);
		CHECKRANGE_SFINI_PARAM(OverrideSample.vel_to_fc, -10000, 10000);
		CHECKRANGE_SFINI_PARAM(OverrideSample.vel_to_fc_threshold, 0, 127);
		CHECKRANGE_SFINI_PARAM(OverrideSample.vel_to_resonance, -100, 100);

		OverrideSample.modenv_to_fc = (int)GetDlgItemInt(hwnd, IDC_SFOW_MODENVFC, NULL, TRUE);
		OverrideSample.modenv_delay = (int)GetDlgItemInt(hwnd, IDC_SFOW_MODENVDELAY, NULL, TRUE);
		OverrideSample.modenv_to_pitch = (int)GetDlgItemInt(hwnd, IDC_SFOW_MODENVPITCH, NULL, TRUE);
		CHECKRANGE_SFINI_PARAM(OverrideSample.modenv_delay, 1, 2000);
		CHECKRANGE_SFINI_PARAM(OverrideSample.modenv_to_fc, -12000, 12000);
		CHECKRANGE_SFINI_PARAM(OverrideSample.modenv_to_pitch, -12000, 12000);

		OverrideSample.cutoff_freq = (int)GetDlgItemInt(hwnd, IDC_SFOW_CUTOFF, NULL, TRUE);
		OverrideSample.resonance = (int)GetDlgItemInt(hwnd, IDC_SFOW_RESONANCE, NULL, TRUE);
		CHECKRANGE_SFINI_PARAM(OverrideSample.cutoff_freq, 0, 20000);
		CHECKRANGE_SFINI_PARAM(OverrideSample.resonance, -200, 200);

		OverrideSample.envelope_delay = (int)GetDlgItemInt(hwnd, IDC_SFOW_ENVDELAY, NULL, TRUE);
		CHECKRANGE_SFINI_PARAM(OverrideSample.envelope_delay, 1, 1000);
		
		// soundfont attenuation
		DLG_CHECKBUTTON_TO_FLAG(hwnd, IDC_SFATT_NEG, sf_attenuation_neg);
		sf_attenuation_pow = GetDlgItemFloat(hwnd, IDC_SFATT_POW);
		sf_attenuation_mul = GetDlgItemFloat(hwnd, IDC_SFATT_MUL);	
		sf_attenuation_add = GetDlgItemFloat(hwnd, IDC_SFATT_ADD);	
		CHECKRANGE_SFINI_PARAM(sf_attenuation_pow, 0.0, 20.0);
		CHECKRANGE_SFINI_PARAM(sf_attenuation_mul, 0.0, 4.0);
		CHECKRANGE_SFINI_PARAM(sf_attenuation_add, -1440.0, 1440.0);
		
		// soundfont limit
		sf_limit_volenv_attack = (int)GetDlgItemInt(hwnd, IDC_SFL_VOLENV_ATK, NULL, TRUE);
		sf_limit_modenv_attack = (int)GetDlgItemInt(hwnd, IDC_SFL_MODENV_ATK, NULL, TRUE);
		sf_limit_modenv_fc = (int)GetDlgItemInt(hwnd, IDC_SFL_MODENV_FC, NULL, TRUE);
		sf_limit_modenv_pitch = (int)GetDlgItemInt(hwnd, IDC_SFL_MODENV_PIT, NULL, TRUE);
		sf_limit_modlfo_fc = (int)GetDlgItemInt(hwnd, IDC_SFL_MODLFO_FC, NULL, TRUE);
		sf_limit_modlfo_pitch = (int)GetDlgItemInt(hwnd, IDC_SFL_MODLFO_PIT, NULL, TRUE);
		sf_limit_viblfo_pitch = (int)GetDlgItemInt(hwnd, IDC_SFL_VIBLFO_PIT, NULL, TRUE);
		sf_limit_modlfo_freq = (int)GetDlgItemInt(hwnd, IDC_SFL_MODLFO_FREQ, NULL, TRUE);
		sf_limit_viblfo_freq = (int)GetDlgItemInt(hwnd, IDC_SFL_VIBLFO_FREQ, NULL, TRUE);
		CHECKRANGE_SFINI_PARAM(sf_limit_volenv_attack, 0, 10);
		CHECKRANGE_SFINI_PARAM(sf_limit_modenv_attack, 0, 10);
		CHECKRANGE_SFINI_PARAM(sf_limit_modenv_fc, 0, 12000);
		CHECKRANGE_SFINI_PARAM(sf_limit_modenv_pitch, 0, 12000);
		CHECKRANGE_SFINI_PARAM(sf_limit_modlfo_fc, 0, 12000);
		CHECKRANGE_SFINI_PARAM(sf_limit_modlfo_pitch, 0, 12000);
		CHECKRANGE_SFINI_PARAM(sf_limit_viblfo_pitch, 0, 12000);
		CHECKRANGE_SFINI_PARAM(sf_limit_modlfo_freq, 1, 100000);
		CHECKRANGE_SFINI_PARAM(sf_limit_viblfo_freq, 1, 100000);
		
		// soundfont default
		sf_default_modlfo_freq = (int)GetDlgItemInt(hwnd, IDC_SFD_MODLFO_FREQ, NULL, TRUE);
		sf_default_viblfo_freq = (int)GetDlgItemInt(hwnd, IDC_SFD_VIBLFO_FREQ, NULL, TRUE);
		CHECKRANGE_SFINI_PARAM(sf_default_modlfo_freq, 1, 100000);
		CHECKRANGE_SFINI_PARAM(sf_default_viblfo_freq, 1, 100000);
		
		// soundfont config
		DLG_CHECKBUTTON_TO_FLAG(hwnd, IDC_SFC_LFO_SWAP, sf_config_lfo_swap);
		DLG_CHECKBUTTON_TO_FLAG(hwnd, IDC_SFC_ADRS_OFFSET, sf_config_addrs_offset);

#undef CHECKRANGE_SFINI_PARAM
		break;
	case WM_SIZE:
		return FALSE;
	case WM_DESTROY:
		break;
	default:
		break;
	}
	return FALSE;
}


static LRESULT APIENTRY CALLBACK PrefSFINI2DialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess) {
	case WM_INITDIALOG:

		// envelope
		SetDlgItemFloat(hwnd, IDC_GSENV_ATTACK_CALC, gs_env_attack_calc);
		SetDlgItemFloat(hwnd, IDC_GSENV_DECAY_CALC, gs_env_decay_calc);
		SetDlgItemFloat(hwnd, IDC_GSENV_RELEASE_CALC, gs_env_release_calc);
		SetDlgItemFloat(hwnd, IDC_XGENV_ATTACK_CALC, xg_env_attack_calc);
		SetDlgItemFloat(hwnd, IDC_XGENV_DECAY_CALC, xg_env_decay_calc);
		SetDlgItemFloat(hwnd, IDC_XGENV_RELEASE_CALC, xg_env_release_calc);
		SetDlgItemFloat(hwnd, IDC_GM2ENV_ATTACK_CALC, gm2_env_attack_calc);
		SetDlgItemFloat(hwnd, IDC_GM2ENV_DECAY_CALC, gm2_env_decay_calc);
		SetDlgItemFloat(hwnd, IDC_GM2ENV_RELEASE_CALC, gm2_env_release_calc);
		SetDlgItemFloat(hwnd, IDC_GMENV_ATTACK_CALC, gm_env_attack_calc);
		SetDlgItemFloat(hwnd, IDC_GMENV_DECAY_CALC, gm_env_decay_calc);
		SetDlgItemFloat(hwnd, IDC_GMENV_RELEASE_CALC, gm_env_release_calc);
		SetDlgItemFloat(hwnd, IDC_ENV_ADD_OFFDELAY_TIME, env_add_offdelay_time);
		// nrpn vibrato
		SetDlgItemInt(hwnd, IDC_VIBRATE_CALC, otd.vibrato_rate, TRUE);
		SetDlgItemInt(hwnd, IDC_VIBDEPTH_CALC, otd.vibrato_cent, TRUE);
		SetDlgItemInt(hwnd, IDC_VIBDELAY_CALC, otd.vibrato_delay, TRUE);		
		// nrpn filter	
		SetDlgItemInt(hwnd, IDC_FILTER_FREQ_CALC, otd.filter_freq, TRUE);
		SetDlgItemInt(hwnd, IDC_FILTER_RESO_CALC, otd.filter_reso, TRUE);
		// voice filter
		SetDlgItemFloat(hwnd, IDC_VOICE_FILTER_RESO_CALC, voice_filter_reso);
		SetDlgItemFloat(hwnd, IDC_VOICE_FILTER_GAIN_CALC, voice_filter_gain);
		// compressor
		DLG_FLAG_TO_CHECKBUTTON(hwnd, IDC_COMPC_CHKENABLE, otd.timRunMode & EOWM_USE_COMPRESSOR);
		SetDlgItemFloat(hwnd, IDC_COMPC_EDTHR, otd.compThr);
		SetDlgItemFloat(hwnd, IDC_COMPC_EDSLOPE, otd.compSlope);
		SetDlgItemFloat(hwnd, IDC_COMPC_EDLOOK, otd.compLook);
		SetDlgItemFloat(hwnd, IDC_COMPC_EDWTIME, otd.compWTime);
		SetDlgItemFloat(hwnd, IDC_COMPC_EDATIME, otd.compATime);
		SetDlgItemFloat(hwnd, IDC_COMPC_EDRTIME, otd.compRTime);
		
		return FALSE;
	case WM_COMMAND:
		break;
	case WM_MYSAVE:
#ifdef CHECKRANGE_SFINI_PARAM
#undef CHECKRANGE_SFINI_PARAM
#endif
#define CHECKRANGE_SFINI_PARAM(SV, SMIN,SMAX) SV = ((SV < SMIN) ?SMIN : ((SV > SMAX) ? SMAX : SV))

		// envelope
		gs_env_attack_calc = GetDlgItemFloat(hwnd, IDC_GSENV_ATTACK_CALC);
		gs_env_decay_calc = GetDlgItemFloat(hwnd, IDC_GSENV_DECAY_CALC);
		gs_env_release_calc = GetDlgItemFloat(hwnd, IDC_GSENV_RELEASE_CALC);
		xg_env_attack_calc = GetDlgItemFloat(hwnd, IDC_XGENV_ATTACK_CALC);
		xg_env_decay_calc = GetDlgItemFloat(hwnd, IDC_XGENV_DECAY_CALC);
		xg_env_release_calc = GetDlgItemFloat(hwnd, IDC_XGENV_RELEASE_CALC);
		gm2_env_attack_calc = GetDlgItemFloat(hwnd, IDC_GM2ENV_ATTACK_CALC);
		gm2_env_decay_calc = GetDlgItemFloat(hwnd, IDC_GM2ENV_DECAY_CALC);
		gm2_env_release_calc = GetDlgItemFloat(hwnd, IDC_GM2ENV_RELEASE_CALC);
		gm_env_attack_calc = GetDlgItemFloat(hwnd, IDC_GMENV_ATTACK_CALC);
		gm_env_decay_calc = GetDlgItemFloat(hwnd, IDC_GMENV_DECAY_CALC);
		gm_env_release_calc = GetDlgItemFloat(hwnd, IDC_GMENV_RELEASE_CALC);
		CHECKRANGE_SFINI_PARAM(gs_env_attack_calc, 0, 4.0);
		CHECKRANGE_SFINI_PARAM(gs_env_decay_calc, 0, 4.0);
		CHECKRANGE_SFINI_PARAM(gs_env_release_calc, 0, 4.0);
		CHECKRANGE_SFINI_PARAM(xg_env_attack_calc, 0, 4.0);
		CHECKRANGE_SFINI_PARAM(xg_env_decay_calc, 0, 4.0);
		CHECKRANGE_SFINI_PARAM(xg_env_release_calc, 0, 4.0);
		CHECKRANGE_SFINI_PARAM(gm2_env_attack_calc, 0, 4.0);
		CHECKRANGE_SFINI_PARAM(gm2_env_decay_calc, 0, 4.0);
		CHECKRANGE_SFINI_PARAM(gm2_env_release_calc, 0, 4.0);
		CHECKRANGE_SFINI_PARAM(gm_env_attack_calc, 0, 4.0);
		CHECKRANGE_SFINI_PARAM(gm_env_decay_calc, 0, 4.0);
		CHECKRANGE_SFINI_PARAM(gm_env_release_calc, 0, 4.0);
		env_add_offdelay_time = GetDlgItemFloat(hwnd, IDC_ENV_ADD_OFFDELAY_TIME);
		CHECKRANGE_SFINI_PARAM(env_add_offdelay_time, 0, 20.0);
		// vibrato
		otd.vibrato_rate  = (double)GetDlgItemInt(hwnd, IDC_VIBRATE_CALC, NULL, TRUE);
		otd.vibrato_cent  = (double)GetDlgItemInt(hwnd, IDC_VIBDEPTH_CALC, NULL, TRUE);
		otd.vibrato_delay = (double)GetDlgItemInt(hwnd, IDC_VIBDELAY_CALC, NULL, TRUE);
		CHECKRANGE_SFINI_PARAM(otd.vibrato_rate, 0, 2000);
		CHECKRANGE_SFINI_PARAM(otd.vibrato_cent, 0, 10000);
		CHECKRANGE_SFINI_PARAM(otd.vibrato_delay, 0, 10000);		
		// nrpn filter	
		otd.filter_freq  = (double)GetDlgItemInt(hwnd, IDC_FILTER_FREQ_CALC, NULL, TRUE);
		otd.filter_reso  = (double)GetDlgItemInt(hwnd, IDC_FILTER_RESO_CALC, NULL, TRUE);
		CHECKRANGE_SFINI_PARAM(otd.filter_freq, 0, 20000);
		CHECKRANGE_SFINI_PARAM(otd.filter_reso, 0, 1000);
		// voice filter
		voice_filter_reso = GetDlgItemFloat(hwnd, IDC_VOICE_FILTER_RESO_CALC);
		voice_filter_gain = GetDlgItemFloat(hwnd, IDC_VOICE_FILTER_GAIN_CALC);
		CHECKRANGE_SFINI_PARAM(voice_filter_reso, 0, 8.0);
		CHECKRANGE_SFINI_PARAM(voice_filter_gain, 0, 8.0);
		
		// compressor
		otd.timRunMode &= ~EOWM_USE_COMPRESSOR;
		otd.timRunMode += (SendDlgItemMessage(hwnd, IDC_COMPC_CHKENABLE, BM_GETCHECK, 0, 0)?1:0) * EOWM_USE_COMPRESSOR;
		otd.compThr = GetDlgItemFloat(hwnd, IDC_COMPC_EDTHR);
		otd.compSlope = GetDlgItemFloat(hwnd, IDC_COMPC_EDSLOPE);
		otd.compLook = GetDlgItemFloat(hwnd, IDC_COMPC_EDLOOK);
		otd.compWTime = GetDlgItemFloat(hwnd, IDC_COMPC_EDWTIME);
		otd.compATime = GetDlgItemFloat(hwnd, IDC_COMPC_EDATIME);
		otd.compRTime = GetDlgItemFloat(hwnd, IDC_COMPC_EDRTIME);
#undef CHECKRANGE_SFINI_PARAM
		break;
	case WM_SIZE:
		return FALSE;
	case WM_DESTROY:
		break;
	default:
		break;
	}
	return FALSE;
}


static LRESULT APIENTRY CALLBACK PrefCustom1DialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	int i;
	switch (uMess) {
	case WM_INITDIALOG:
///r
		// freeverb
		SetDlgItemFloat(hwnd, IDC_REVC_EDWET, scalewet);
		SetDlgItemFloat(hwnd, IDC_REVC_EDSCALEROOM, freeverb_scaleroom);
		SetDlgItemFloat(hwnd, IDC_REVC_EDOFFSETROOM, freeverb_offsetroom);
		SetDlgItemFloat(hwnd, IDC_REVC_EDFIXGAIN, fixedgain);
		SetDlgItemFloat(hwnd, IDC_REVC_EDCOMBFBK, combfbk);
		SetDlgItemFloat(hwnd, IDC_REVC_EDTIME, time_rt_diff);
		// NUMCOMBS
		for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_REVC_EDNUMCOMBS_num); i++)
			CB_INSSTR(IDC_COMBO_REVC_EDNUMCOMBS, cb_info_IDC_COMBO_REVC_EDNUMCOMBS[i]);
		CB_SET(IDC_COMBO_REVC_EDNUMCOMBS, CB_FIND(cb_info_IDC_COMBO_REVC_EDNUMCOMBS_num, numcombs, 2));
		// reverb ex
		SetDlgItemFloat(hwnd, IDC_REVC_EX_TIME, ext_reverb_ex_time);
		SetDlgItemFloat(hwnd, IDC_REVC_EX_LEVEL, ext_reverb_ex_level);
		SetDlgItemFloat(hwnd, IDC_REVC_EX_ER_LEVEL, ext_reverb_ex_er_level);
		SetDlgItemFloat(hwnd, IDC_REVC_EX_RV_LEVEL, ext_reverb_ex_rv_level);
	//	DLG_FLAG_TO_CHECKBUTTON(hwnd, IDC_REVC_EX_LITE_MODE, ext_reverb_ex_lite);	
		for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_REVC_EX_RV_NUM_num); i++)
			CB_INSSTR(IDC_COMBO_REVC_EX_RV_NUM, cb_info_IDC_COMBO_REVC_EX_RV_NUM[i]);
		CB_SET(IDC_COMBO_REVC_EX_RV_NUM, CB_FIND(cb_info_IDC_COMBO_REVC_EX_RV_NUM_num, ext_reverb_ex_rv_num, 5));
		for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_REVC_EX_AP_NUM_num); i++)
			CB_INSSTR(IDC_COMBO_REVC_EX_AP_NUM, cb_info_IDC_COMBO_REVC_EX_AP_NUM[i]);
		CB_SET(IDC_COMBO_REVC_EX_AP_NUM, CB_FIND(cb_info_IDC_COMBO_REVC_EX_AP_NUM_num, ext_reverb_ex_ap_num, 8));
		//for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_REVC_EX_ER_NUM_num); i++)
		//	CB_INSSTR(IDC_COMBO_REVC_EX_ER_NUM, cb_info_IDC_COMBO_REVC_EX_ER_NUM[i]);
		//CB_SET(IDC_COMBO_REVC_EX_ER_NUM, CB_FIND(cb_info_IDC_COMBO_REVC_EX_ER_NUM_num, ext_reverb_ex_er_num, 8));
		//for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_REVC_EX_RV_TYPE_num); i++)
		//	CB_INSSTR(IDC_COMBO_REVC_EX_RV_TYPE, cb_info_IDC_COMBO_REVC_EX_RV_TYPE[i]);
		//CB_SET(IDC_COMBO_REVC_EX_RV_TYPE, CB_FIND(cb_info_IDC_COMBO_REVC_EX_RV_TYPE_num, ext_reverb_ex_rv_type, 2));
		//for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_REVC_EX_AP_TYPE_num); i++)
		//	CB_INSSTR(IDC_COMBO_REVC_EX_AP_TYPE, cb_info_IDC_COMBO_REVC_EX_AP_TYPE[i]);
		//CB_SET(IDC_COMBO_REVC_EX_AP_TYPE, CB_FIND(cb_info_IDC_COMBO_REVC_EX_AP_TYPE_num, ext_reverb_ex_ap_type, 0));
		//for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_REVC_EX_MODE_num); i++)
		//	CB_INSSTR(IDC_COMBO_REVC_EX_MODE, cb_info_IDC_COMBO_REVC_EX_MODE[i]);
		//CB_SET(IDC_COMBO_REVC_EX_MODE, CB_FIND(cb_info_IDC_COMBO_REVC_EX_MODE_num, ext_reverb_ex_mode, 0));
		DLG_FLAG_TO_CHECKBUTTON(hwnd, IDC_REVC_EX_MOD, ext_reverb_ex_mod);	
		// plate reverb
		SetDlgItemFloat(hwnd, IDC_REVC_PLATE_LEVEL, ext_plate_reverb_level);
		SetDlgItemFloat(hwnd, IDC_REVC_PLATE_TIME, ext_plate_reverb_time);
		// efx reverb 
		for (i = 0; i < CB_NUM(cb_info_IDC_SFOW_EFX_REV_TYPE_num); i++)
			CB_INSSTR(IDC_SFOW_EFX_REV_TYPE, cb_info_IDC_SFOW_EFX_REV_TYPE[i]);
		CB_SET(IDC_SFOW_EFX_REV_TYPE, CB_FIND(cb_info_IDC_SFOW_EFX_REV_TYPE_num, otd.efx_CustomRevType, 5));
		// reverb ex2
		SetDlgItemFloat(hwnd, IDC_REVC_SR_LEVEL, ext_reverb_ex2_level);
		for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_REVC_SR_RS_MODE_num); i++)
			CB_INSSTR(IDC_COMBO_REVC_SR_RS_MODE, cb_info_IDC_COMBO_REVC_SR_RS_MODE[i]);
		CB_SET(IDC_COMBO_REVC_SR_RS_MODE, CB_FIND(cb_info_IDC_COMBO_REVC_SR_RS_MODE_num, ext_reverb_ex2_rsmode, 3));
		for (i = 0; i < CB_NUM(cb_info_IDC_COMBO_REVC_SR_FFT_MODE_num); i++)
			CB_INSSTR(IDC_COMBO_REVC_SR_FFT_MODE, cb_info_IDC_COMBO_REVC_SR_FFT_MODE[i]);
		CB_SET(IDC_COMBO_REVC_SR_FFT_MODE, CB_FIND(cb_info_IDC_COMBO_REVC_SR_FFT_MODE_num, ext_reverb_ex2_fftmode, 0));
		// ch chorus
		SetDlgItemFloat(hwnd, IDC_CHOC_EXT_LEVEL, ext_chorus_level);
		SetDlgItemFloat(hwnd, IDC_CHOC_EXT_FEEDBACK, ext_chorus_feedback);
		SetDlgItemFloat(hwnd, IDC_CHOC_EXT_DEPTH, ext_chorus_depth);
		// ch chorus ex phase
		for (i = 0; i < CB_NUM(cb_info_IDC_CHOC_EX_PHASE_NUM_num); i++)
			CB_INSSTR(IDC_CHOC_EX_PHASE_NUM, cb_info_IDC_CHOC_EX_PHASE_NUM[i]);
		CB_SET(IDC_CHOC_EX_PHASE_NUM, CB_FIND(cb_info_IDC_CHOC_EX_PHASE_NUM_num, ext_chorus_ex_phase, 2));
	//	DLG_FLAG_TO_CHECKBUTTON(hwnd, IDC_CHOC_EX_LITE_MODE, ext_chorus_ex_lite);	
		DLG_FLAG_TO_CHECKBUTTON(hwnd, IDC_CHOC_EX_OV, ext_chorus_ex_ov);					
		// ch delay
		SetDlgItemFloat(hwnd, IDC_DELAY_EDDELAY, otd.delay_param.delay);
		SetDlgItemFloat(hwnd, IDC_DELAY_EDLEVEL, otd.delay_param.level);
		SetDlgItemFloat(hwnd, IDC_DELAY_EDFEEDBACK, otd.delay_param.feedback);
		// filter
		SetDlgItemFloat(hwnd, IDC_FLTC_EXT_SHELVING_GAIN, ext_filter_shelving_gain);
		SetDlgItemFloat(hwnd, IDC_FLTC_EXT_SHELVING_REDUCE, ext_filter_shelving_reduce);
		SetDlgItemFloat(hwnd, IDC_FLTC_EXT_SHELVING_Q, ext_filter_shelving_q);
		SetDlgItemFloat(hwnd, IDC_FLTC_EXT_PEAKING_GAIN, ext_filter_peaking_gain);
		SetDlgItemFloat(hwnd, IDC_FLTC_EXT_PEAKING_REDUCE, ext_filter_peaking_reduce);		
		SetDlgItemFloat(hwnd, IDC_FLTC_EXT_PEAKING_Q, ext_filter_peaking_q);
		// effect
		//SetDlgItemFloat(hwnd, IDC_EFX_XG_SYS_RETURN_LEVEL, xg_system_return_level);
		SetDlgItemFloat(hwnd, IDC_EFX_XG_REV_RETURN_LEVEL, xg_reverb_return_level);
		SetDlgItemFloat(hwnd, IDC_EFX_XG_CHO_RETURN_LEVEL, xg_chorus_return_level);
		SetDlgItemFloat(hwnd, IDC_EFX_XG_VAR_RETURN_LEVEL, xg_variation_return_level);		
		SetDlgItemFloat(hwnd, IDC_EFX_XG_CHO_SEND_REVERB, xg_chorus_send_reverb);
		SetDlgItemFloat(hwnd, IDC_EFX_XG_VAR_SEND_REVERB, xg_variation_send_reverb);
		SetDlgItemFloat(hwnd, IDC_EFX_XG_VAR_SEND_CHORUS, xg_variation_send_chorus);

		return FALSE;
	case WM_COMMAND:
//		switch (LOWORD(wParam)) {
//#ifdef VST_LOADER_ENABLE
//			case IDC_OPENVSTMGR:
//				if (hVSTHost != NULL) {
//					((open_vst_mgr)GetProcAddress(hVSTHost, "openVSTManager"))(hwnd);
//				}
//				break;
//#endif
//		}
		break;
	case WM_MYSAVE:
#ifdef CHECKRANGE_SFINI_PARAM
#undef CHECKRANGE_SFINI_PARAM
#endif
#define CHECKRANGE_SFINI_PARAM(SV, SMIN,SMAX) SV = ((SV < SMIN) ?SMIN : ((SV > SMAX) ? SMAX : SV))
		
///r
		// ch freeverb
		scalewet = GetDlgItemFloat(hwnd, IDC_REVC_EDWET);
		freeverb_scaleroom = GetDlgItemFloat(hwnd, IDC_REVC_EDSCALEROOM);
		freeverb_offsetroom = GetDlgItemFloat(hwnd, IDC_REVC_EDOFFSETROOM);
		fixedgain = GetDlgItemFloat(hwnd, IDC_REVC_EDFIXGAIN);
		combfbk = GetDlgItemFloat(hwnd, IDC_REVC_EDCOMBFBK);
		time_rt_diff = GetDlgItemFloat(hwnd, IDC_REVC_EDTIME);
		numcombs = cb_info_IDC_COMBO_REVC_EDNUMCOMBS_num[CB_GETS(IDC_COMBO_REVC_EDNUMCOMBS, 2)];
		// reverb ex
		ext_reverb_ex_time = GetDlgItemFloat(hwnd, IDC_REVC_EX_TIME);
		ext_reverb_ex_level = GetDlgItemFloat(hwnd, IDC_REVC_EX_LEVEL);
		ext_reverb_ex_er_level = GetDlgItemFloat(hwnd, IDC_REVC_EX_ER_LEVEL);
		ext_reverb_ex_rv_level = GetDlgItemFloat(hwnd, IDC_REVC_EX_RV_LEVEL);
		CHECKRANGE_SFINI_PARAM(ext_reverb_ex_time, 0.001, 4.000);	
		CHECKRANGE_SFINI_PARAM(ext_reverb_ex_level, 0.001, 4.000);
		CHECKRANGE_SFINI_PARAM(ext_reverb_ex_er_level, 0.001, 4.000);
		CHECKRANGE_SFINI_PARAM(ext_reverb_ex_rv_level, 0.001, 4.000);
	//	DLG_CHECKBUTTON_TO_FLAG(hwnd, IDC_REVC_EX_LITE_MODE, ext_reverb_ex_lite);
		ext_reverb_ex_rv_num = cb_info_IDC_COMBO_REVC_EX_RV_NUM_num[CB_GETS(IDC_COMBO_REVC_EX_RV_NUM, 5)];
		ext_reverb_ex_ap_num = cb_info_IDC_COMBO_REVC_EX_AP_NUM_num[CB_GETS(IDC_COMBO_REVC_EX_AP_NUM, 1)];
		//ext_reverb_ex_er_num = cb_info_IDC_COMBO_REVC_EX_ER_NUM_num[CB_GETS(IDC_COMBO_REVC_EX_ER_NUM, 8)];
		//ext_reverb_ex_rv_num = cb_info_IDC_COMBO_REVC_EX_RV_NUM_num[CB_GETS(IDC_COMBO_REVC_EX_RV_NUM, 6)];
		//ext_reverb_ex_rv_type = cb_info_IDC_COMBO_REVC_EX_RV_TYPE_num[CB_GETS(IDC_COMBO_REVC_EX_RV_TYPE, 2)];
		//ext_reverb_ex_ap_type = cb_info_IDC_COMBO_REVC_EX_AP_TYPE_num[CB_GETS(IDC_COMBO_REVC_EX_AP_TYPE, 0)];
		//ext_reverb_ex_mode = cb_info_IDC_COMBO_REVC_EX_MODE_num[CB_GETS(IDC_COMBO_REVC_EX_MODE, 0)];
		DLG_CHECKBUTTON_TO_FLAG(hwnd, IDC_REVC_EX_MOD, ext_reverb_ex_mod);
		// reverb ex2
		ext_reverb_ex2_level = GetDlgItemFloat(hwnd, IDC_REVC_SR_LEVEL);
		CHECKRANGE_SFINI_PARAM(ext_reverb_ex2_level, 0.001, 16.000);	
		ext_reverb_ex2_rsmode = cb_info_IDC_COMBO_REVC_SR_RS_MODE_num[CB_GETS(IDC_COMBO_REVC_SR_RS_MODE, 3)];
		ext_reverb_ex2_fftmode = cb_info_IDC_COMBO_REVC_SR_FFT_MODE_num[CB_GETS(IDC_COMBO_REVC_SR_FFT_MODE, 0)];
		// plate reverb
		ext_plate_reverb_level = GetDlgItemFloat(hwnd, IDC_REVC_PLATE_LEVEL);
		ext_plate_reverb_time = GetDlgItemFloat(hwnd, IDC_REVC_PLATE_TIME);
		CHECKRANGE_SFINI_PARAM(ext_plate_reverb_level, 0.001, 4.000);
		CHECKRANGE_SFINI_PARAM(ext_plate_reverb_time, 0.001, 4.000);	
		// efx reverb 
		otd.efx_CustomRevType = cb_info_IDC_SFOW_EFX_REV_TYPE_num[CB_GETS(IDC_SFOW_EFX_REV_TYPE, 0)];
		// ch chorus
		ext_chorus_level = GetDlgItemFloat(hwnd, IDC_CHOC_EXT_LEVEL);
		ext_chorus_feedback = GetDlgItemFloat(hwnd, IDC_CHOC_EXT_FEEDBACK);
		ext_chorus_depth = GetDlgItemFloat(hwnd, IDC_CHOC_EXT_DEPTH);		
		CHECKRANGE_SFINI_PARAM(ext_chorus_level, 0.001, 4.000);
		CHECKRANGE_SFINI_PARAM(ext_chorus_feedback, 0.001, 4.000);	
		CHECKRANGE_SFINI_PARAM(ext_chorus_depth, 0.001, 4.000);
		// ch chorus ex
		ext_chorus_ex_phase = cb_info_IDC_CHOC_EX_PHASE_NUM_num[CB_GETS(IDC_CHOC_EX_PHASE_NUM, 2)];
	//	DLG_CHECKBUTTON_TO_FLAG(hwnd, IDC_CHOC_EX_LITE_MODE, ext_chorus_ex_lite);
		DLG_CHECKBUTTON_TO_FLAG(hwnd, IDC_CHOC_EX_OV, ext_chorus_ex_ov);
		// ch delay
		otd.delay_param.delay = GetDlgItemFloat(hwnd, IDC_DELAY_EDDELAY);
		if (otd.delay_param.delay < 0.01) {
			MessageBox(hwnd, "delay point is range [0.01 - 2.0]\r\n1.0 = 100%", "err", MB_OK);
			otd.delay_param.delay = 0.01;
		}
		if (otd.delay_param.delay > 2.0) {
			MessageBox(hwnd, "delay point is range [0.01 - 2.0]\r\n1.0 = 100%", "err", MB_OK);
			otd.delay_param.delay = 2.00;
		}
		otd.delay_param.level = GetDlgItemFloat(hwnd, IDC_DELAY_EDLEVEL);
		otd.delay_param.feedback = GetDlgItemFloat(hwnd, IDC_DELAY_EDFEEDBACK);
		CHECKRANGE_SFINI_PARAM(otd.delay_param.level, 0.001, 8.000);
		CHECKRANGE_SFINI_PARAM(otd.delay_param.feedback, 0.001, 4.000);	
		// filter
		ext_filter_shelving_gain = GetDlgItemFloat(hwnd, IDC_FLTC_EXT_SHELVING_GAIN);
		ext_filter_shelving_reduce = GetDlgItemFloat(hwnd, IDC_FLTC_EXT_SHELVING_REDUCE);
		ext_filter_shelving_q = GetDlgItemFloat(hwnd, IDC_FLTC_EXT_SHELVING_Q);
		ext_filter_peaking_gain = GetDlgItemFloat(hwnd, IDC_FLTC_EXT_PEAKING_GAIN);
		ext_filter_peaking_reduce = GetDlgItemFloat(hwnd, IDC_FLTC_EXT_PEAKING_REDUCE);
		ext_filter_peaking_q = GetDlgItemFloat(hwnd, IDC_FLTC_EXT_PEAKING_Q);
		CHECKRANGE_SFINI_PARAM(ext_filter_shelving_gain, 0.001, 8.000);
		CHECKRANGE_SFINI_PARAM(ext_filter_shelving_reduce, 0.0, 8.000);
		CHECKRANGE_SFINI_PARAM(ext_filter_shelving_q, 0.25, 8.000);
		CHECKRANGE_SFINI_PARAM(ext_filter_peaking_gain, 0.001, 8.000);
		CHECKRANGE_SFINI_PARAM(ext_filter_peaking_reduce, 0.0, 8.000);	
		CHECKRANGE_SFINI_PARAM(ext_filter_peaking_q, 0.25, 8.000);
		// effect
		//xg_system_return_level = GetDlgItemFloat(hwnd, IDC_EFX_XG_SYS_RETURN_LEVEL);
		//CHECKRANGE_SFINI_PARAM(xg_system_return_level, 0.001, 8.000);	
		xg_reverb_return_level = GetDlgItemFloat(hwnd, IDC_EFX_XG_REV_RETURN_LEVEL);
		CHECKRANGE_SFINI_PARAM(xg_reverb_return_level, 0.001, 8.000);	
		xg_chorus_return_level = GetDlgItemFloat(hwnd, IDC_EFX_XG_CHO_RETURN_LEVEL);
		CHECKRANGE_SFINI_PARAM(xg_chorus_return_level, 0.001, 8.000);	
		xg_variation_return_level = GetDlgItemFloat(hwnd, IDC_EFX_XG_VAR_RETURN_LEVEL);
		CHECKRANGE_SFINI_PARAM(xg_variation_return_level, 0.001, 8.000);			
		xg_chorus_send_reverb = GetDlgItemFloat(hwnd, IDC_EFX_XG_CHO_SEND_REVERB);
		CHECKRANGE_SFINI_PARAM(xg_chorus_send_reverb, 0.001, 8.000);	
		xg_variation_send_reverb = GetDlgItemFloat(hwnd, IDC_EFX_XG_VAR_SEND_REVERB);
		CHECKRANGE_SFINI_PARAM(xg_variation_send_reverb, 0.001, 8.000);	
		xg_variation_send_chorus = GetDlgItemFloat(hwnd, IDC_EFX_XG_VAR_SEND_CHORUS);
		CHECKRANGE_SFINI_PARAM(xg_variation_send_chorus, 0.001, 8.000);	

#undef CHECKRANGE_SFINI_PARAM
		break;
	case WM_SIZE:
		return FALSE;
	case WM_DESTROY:
		break;
	default:
		break;
	}
	return FALSE;
}

static LRESULT APIENTRY CALLBACK PrefCustom2DialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess) {
	case WM_INITDIALOG:
///r
		// efx OD/DS
		SetDlgItemInt(hwnd, IDC_SFOW_EFXGS_ODD_CALC, otd.gsefx_CustomODDrive * 1000, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_EFXGS_ODLV_CALC, otd.gsefx_CustomODLv * 1000, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_EFXGS_ODF_CALC, otd.gsefx_CustomODFreq * 1000, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_EFXXG_ODD_CALC, otd.xgefx_CustomODDrive * 1000, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_EFXXG_ODLV_CALC, otd.xgefx_CustomODLv * 1000, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_EFXXG_ODF_CALC, otd.xgefx_CustomODFreq * 1000, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_EFXSD_ODD_CALC, otd.sdefx_CustomODDrive * 1000, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_EFXSD_ODLV_CALC, otd.sdefx_CustomODLv * 1000, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_EFXSD_ODF_CALC, otd.sdefx_CustomODFreq * 1000, TRUE);
		// efx lofi
		SetDlgItemInt(hwnd, IDC_SFOW_EFXGS_LFIN_CALC, otd.gsefx_CustomLFLvIn * 1000, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_EFXGS_LFOUT_CALC, otd.gsefx_CustomLFLvOut * 1000, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_EFXXG_LFIN_CALC, otd.xgefx_CustomLFLvIn * 1000, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_EFXXG_LFOUT_CALC, otd.xgefx_CustomLFLvOut * 1000, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_EFXSD_LFIN_CALC, otd.sdefx_CustomLFLvIn * 1000, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_EFXSD_LFOUT_CALC, otd.sdefx_CustomLFLvOut * 1000, TRUE);
		// efx humanizer
		SetDlgItemInt(hwnd, IDC_SFOW_EFX_HMNIN_CALC, otd.efx_CustomHmnLvIn * 1000, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_EFX_HMNOUT_CALC, otd.efx_CustomHmnLvOut * 1000, TRUE);
		// efx compressor
		SetDlgItemInt(hwnd, IDC_SFOW_EFX_CMPIN_CALC, otd.efx_CustomCmpLvIn * 1000, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_EFX_CMPOUT_CALC, otd.efx_CustomCmpLvOut * 1000, TRUE);
		// efx limiter
		SetDlgItemInt(hwnd, IDC_SFOW_EFX_LMTIN_CALC, otd.efx_CustomLmtLvIn * 1000, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_EFX_LMTOUT_CALC, otd.efx_CustomLmtLvOut * 1000, TRUE);	
		// efx wah
		SetDlgItemInt(hwnd, IDC_SFOW_EFX_WAHIN_CALC, otd.efx_CustomWahLvIn * 1000, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_EFX_WAHOUT_CALC, otd.efx_CustomWahLvOut * 1000, TRUE);	
		// efx gate reverb
		SetDlgItemInt(hwnd, IDC_SFOW_EFX_GREVIN_CALC, otd.efx_CustomGRevLvIn * 1000, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_EFX_GREVOUT_CALC, otd.efx_CustomGRevLvOut * 1000, TRUE);	
		// efx enhancer
		SetDlgItemInt(hwnd, IDC_SFOW_EFX_ENHIN_CALC, otd.efx_CustomEnhLvIn * 1000, TRUE);
		SetDlgItemInt(hwnd, IDC_SFOW_EFX_ENHOUT_CALC, otd.efx_CustomEnhLvOut * 1000, TRUE);	
		// efx rotary
		SetDlgItemInt(hwnd, IDC_SFOW_EFX_ROTOUT_CALC, otd.efx_CustomRotLvOut * 1000, TRUE);	
		// efx pitch shifter
		SetDlgItemInt(hwnd, IDC_SFOW_EFX_PSOUT_CALC, otd.efx_CustomPSLvOut * 1000, TRUE);	
		// efx ring modulator
		SetDlgItemInt(hwnd, IDC_SFOW_EFX_RMOUT_CALC, otd.efx_CustomRMLvOut * 1000, TRUE);	

		return FALSE;
	case WM_COMMAND:
//		switch (LOWORD(wParam)) {
//#ifdef VST_LOADER_ENABLE
//			case IDC_OPENVSTMGR:
//				if (hVSTHost != NULL) {
//					((open_vst_mgr)GetProcAddress(hVSTHost, "openVSTManager"))(hwnd);
//				}
//				break;
//#endif
//		}
		break;
	case WM_MYSAVE:
#ifdef CHECKRANGE_SFINI_PARAM
#undef CHECKRANGE_SFINI_PARAM
#endif
#define CHECKRANGE_SFINI_PARAM(SV, SMIN,SMAX) SV = ((SV < SMIN) ?SMIN : ((SV > SMAX) ? SMAX : SV))


///r
		// efx OD/DS
		otd.gsefx_CustomODDrive = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFXGS_ODD_CALC, NULL, TRUE) / 1000;
		otd.gsefx_CustomODLv = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFXGS_ODLV_CALC, NULL, TRUE) / 1000;
		otd.gsefx_CustomODFreq = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFXGS_ODF_CALC, NULL, TRUE) / 1000;
		otd.xgefx_CustomODDrive = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFXXG_ODD_CALC, NULL, TRUE) / 1000;
		otd.xgefx_CustomODLv = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFXXG_ODLV_CALC, NULL, TRUE) / 1000;
		otd.xgefx_CustomODFreq = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFXXG_ODF_CALC, NULL, TRUE) / 1000;
		otd.sdefx_CustomODDrive = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFXSD_ODD_CALC, NULL, TRUE) / 1000;
		otd.sdefx_CustomODLv = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFXSD_ODLV_CALC, NULL, TRUE) / 1000;
		otd.sdefx_CustomODFreq = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFXSD_ODF_CALC, NULL, TRUE) / 1000;
		CHECKRANGE_SFINI_PARAM(otd.gsefx_CustomODDrive, 0.001, 4.000);
		CHECKRANGE_SFINI_PARAM(otd.gsefx_CustomODLv, 0.001, 4.000);
		CHECKRANGE_SFINI_PARAM(otd.gsefx_CustomODFreq, 0.001, 2.000);
		CHECKRANGE_SFINI_PARAM(otd.xgefx_CustomODDrive, 0.001, 4.000);
		CHECKRANGE_SFINI_PARAM(otd.xgefx_CustomODLv, 0.001, 4.000);
		CHECKRANGE_SFINI_PARAM(otd.xgefx_CustomODFreq, 0.001, 2.000);	
		CHECKRANGE_SFINI_PARAM(otd.sdefx_CustomODDrive, 0.001, 4.000);
		CHECKRANGE_SFINI_PARAM(otd.sdefx_CustomODLv, 0.001, 4.000);
		CHECKRANGE_SFINI_PARAM(otd.sdefx_CustomODFreq, 0.001, 2.000);		
		// efx LoFi
		otd.gsefx_CustomLFLvIn = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFXGS_LFIN_CALC, NULL, TRUE) / 1000;
		otd.gsefx_CustomLFLvOut = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFXGS_LFOUT_CALC, NULL, TRUE) / 1000;
		otd.xgefx_CustomLFLvIn = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFXXG_LFIN_CALC, NULL, TRUE) / 1000;
		otd.xgefx_CustomLFLvOut = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFXXG_LFOUT_CALC, NULL, TRUE) / 1000;
		otd.sdefx_CustomLFLvIn = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFXSD_LFIN_CALC, NULL, TRUE) / 1000;
		otd.sdefx_CustomLFLvOut = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFXSD_LFOUT_CALC, NULL, TRUE) / 1000;
		CHECKRANGE_SFINI_PARAM(otd.gsefx_CustomLFLvIn, 0.001, 4.000);
		CHECKRANGE_SFINI_PARAM(otd.gsefx_CustomLFLvOut, 0.001, 4.000);
		CHECKRANGE_SFINI_PARAM(otd.xgefx_CustomLFLvIn, 0.001, 4.000);
		CHECKRANGE_SFINI_PARAM(otd.xgefx_CustomLFLvOut, 0.001, 4.000);	
		CHECKRANGE_SFINI_PARAM(otd.sdefx_CustomLFLvIn, 0.001, 4.000);
		CHECKRANGE_SFINI_PARAM(otd.sdefx_CustomLFLvOut, 0.001, 4.000);	
		// efx humanizer
		otd.efx_CustomHmnLvIn = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFX_HMNIN_CALC, NULL, TRUE) / 1000;
		otd.efx_CustomHmnLvOut = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFX_HMNOUT_CALC, NULL, TRUE) / 1000;
		CHECKRANGE_SFINI_PARAM(otd.efx_CustomHmnLvIn, 0.001, 4.000);
		CHECKRANGE_SFINI_PARAM(otd.efx_CustomHmnLvOut, 0.001, 4.000);
		// efx compressor
		otd.efx_CustomCmpLvIn = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFX_CMPIN_CALC, NULL, TRUE) / 1000;
		otd.efx_CustomCmpLvOut = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFX_CMPOUT_CALC, NULL, TRUE) / 1000;
		CHECKRANGE_SFINI_PARAM(otd.efx_CustomCmpLvIn, 0.001, 4.000);
		CHECKRANGE_SFINI_PARAM(otd.efx_CustomCmpLvOut, 0.001, 4.000);
		// efx limiter
		otd.efx_CustomLmtLvIn = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFX_LMTIN_CALC, NULL, TRUE) / 1000;
		otd.efx_CustomLmtLvOut = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFX_LMTOUT_CALC, NULL, TRUE) / 1000;	
		CHECKRANGE_SFINI_PARAM(otd.efx_CustomLmtLvIn, 0.001, 4.000);
		CHECKRANGE_SFINI_PARAM(otd.efx_CustomLmtLvOut, 0.001, 4.000);
		// efx wah
		otd.efx_CustomWahLvIn = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFX_WAHIN_CALC, NULL, TRUE) / 1000;
		otd.efx_CustomWahLvOut = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFX_WAHOUT_CALC, NULL, TRUE) / 1000;	
		CHECKRANGE_SFINI_PARAM(otd.efx_CustomWahLvIn, 0.001, 4.000);
		CHECKRANGE_SFINI_PARAM(otd.efx_CustomWahLvOut, 0.001, 4.000);
		// efx gate reverb
		otd.efx_CustomGRevLvIn = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFX_GREVIN_CALC, NULL, TRUE) / 1000;
		otd.efx_CustomGRevLvOut = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFX_GREVOUT_CALC, NULL, TRUE) / 1000;	
		CHECKRANGE_SFINI_PARAM(otd.efx_CustomGRevLvIn, 0.001, 4.000);
		CHECKRANGE_SFINI_PARAM(otd.efx_CustomGRevLvOut, 0.001, 4.000);
		// efx enhancer
		otd.efx_CustomEnhLvIn = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFX_ENHIN_CALC, NULL, TRUE) / 1000;
		otd.efx_CustomEnhLvOut = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFX_ENHOUT_CALC, NULL, TRUE) / 1000;	
		CHECKRANGE_SFINI_PARAM(otd.efx_CustomEnhLvIn, 0.001, 4.000);
		CHECKRANGE_SFINI_PARAM(otd.efx_CustomEnhLvOut, 0.001, 4.000);
		// efx rotary
		otd.efx_CustomRotLvOut = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFX_ROTOUT_CALC, NULL, TRUE) / 1000;	
		CHECKRANGE_SFINI_PARAM(otd.efx_CustomRotLvOut, 0.001, 4.000);
		// efx pitch shifter
		otd.efx_CustomPSLvOut = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFX_PSOUT_CALC, NULL, TRUE) / 1000;	
		CHECKRANGE_SFINI_PARAM(otd.efx_CustomPSLvOut, 0.001, 4.000);
		// efx ring modulator
		otd.efx_CustomRMLvOut = (double)GetDlgItemInt(hwnd, IDC_SFOW_EFX_RMOUT_CALC, NULL, TRUE) / 1000;	
		CHECKRANGE_SFINI_PARAM(otd.efx_CustomRMLvOut, 0.001, 4.000);

		
#undef CHECKRANGE_SFINI_PARAM
		break;
	case WM_SIZE:
		return FALSE;
	case WM_DESTROY:
		break;
	default:
		break;
	}
	return FALSE;
}


// IDC_COMBO_INT_SYNTH_SINE
#define cb_num_IDC_COMBO_INT_SYNTH_SINE 3

static const TCHAR *cb_info_IDC_COMBO_INT_SYNTH_SINE_en[] = {
	TEXT("math function"),
	TEXT("10bit table"),
	TEXT("10bit table (linear interporation)"),
};

static const TCHAR *cb_info_IDC_COMBO_INT_SYNTH_SINE_jp[] = {
	TEXT("math関数 (精度重視)"),
	TEXT("10bitテーブル 補間なし"),
	TEXT("10bitテーブル リニア補間"),
};

static LRESULT APIENTRY CALLBACK PrefIntSynthDialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	int i;
	switch (uMess) {
	case WM_INITDIALOG:
		SetDlgItemInt(hwnd, IDC_EDIT_INT_SYNTH_RATE, st_temp->opt_int_synth_rate, TRUE);
		SetDlgItemInt(hwnd, IDC_EDIT_INT_SYNTH_UPDATE, st_temp->opt_int_synth_update, TRUE);
	//	SetDlgItemInt(hwnd, IDC_EDIT_INT_SYNTH_SINE, st_temp->opt_int_synth_sine, TRUE);
		// INT_SYNTH_SINE
		if (CurrentPlayerLanguage == LANGUAGE_JAPANESE){
			for (i = 0; i < cb_num_IDC_COMBO_INT_SYNTH_SINE; i++)
				CB_INSSTR(IDC_COMBO_INT_SYNTH_SINE, cb_info_IDC_COMBO_INT_SYNTH_SINE_jp[i]);
		}else{
			for (i = 0; i < cb_num_IDC_COMBO_INT_SYNTH_SINE; i++)
				CB_INSSTR(IDC_COMBO_INT_SYNTH_SINE, cb_info_IDC_COMBO_INT_SYNTH_SINE_en[i]);
		}
		CB_SET(IDC_COMBO_INT_SYNTH_SINE, (WPARAM) st_temp->opt_int_synth_sine);
		return FALSE;
	case WM_COMMAND:
		break;
	case WM_MYSAVE:
		st_temp->opt_int_synth_rate = GetDlgItemInt(hwnd, IDC_EDIT_INT_SYNTH_RATE, NULL, TRUE);
		st_temp->opt_int_synth_update = GetDlgItemInt(hwnd, IDC_EDIT_INT_SYNTH_UPDATE, NULL, TRUE);		
	//	st_temp->opt_int_synth_sine = GetDlgItemInt(hwnd, IDC_EDIT_INT_SYNTH_SINE, NULL, TRUE);
		// INT_SYNTH_SINE
		st_temp->opt_int_synth_sine = SendDlgItemMessage(hwnd, IDC_COMBO_INT_SYNTH_SINE, CB_GETCURSEL, (WPARAM) 0, (LPARAM) 0);
		break;
	case WM_SIZE:
		return FALSE;
	case WM_DESTROY:
		break;
	default:
		break;
	}
	return FALSE;
}

void ShowPrefWnd ( void )
{
	if ( IsWindow ( hPrefWnd ) )
		ShowWindow ( hPrefWnd, SW_SHOW );
}
void HidePrefWnd ( void )
{
	if ( IsWindow ( hPrefWnd ) )
		ShowWindow ( hPrefWnd, SW_HIDE );
}
BOOL IsVisiblePrefWnd ( void )
{
	return IsWindowVisible ( hPrefWnd );
}

static int DlgOpenConfigFile(char *Filename, HWND hwnd)
{
	OPENFILENAMEA ofn;
	char filename[FILEPATH_MAX],
	     dir[FILEPATH_MAX];
    int i, res;
    const char *filter,
           filter_en[] = "All Supported files (*.cfg;*.config;*.sf2;*.sf3)\0*.cfg;*.config;*.sf2;*.sf3\0"
                "SoundFont file (*.sf2)\0*.sf2;*.sf3\0"
                "Config file (*.cfg;*.config)\0*.cfg;*.config\0"
                "All files (*.*)\0*.*\0"
                "\0\0",
           filter_jp[] = "すべての対応ファイル (*.cfg;*.config;*.sf2;*.sf3)\0*.cfg;*.config;*.sf2;*.sf3\0"
                "SoundFont ファイル (*.sf2)\0*.sf2;*.sf3\0"
                "Config ファイル (*.cfg;*.config)\0*.cfg;*.config\0"
                "すべてのファイル (*.*)\0*.*\0"
                "\0\0";
    const char *title,
           title_en[] = "Open Config File",
           title_jp[] = "Config ファイルを開く";

	if (CurrentPlayerLanguage == LANGUAGE_JAPANESE) {
		filter = filter_jp;
		title = title_jp;
	}
	else {
		filter = filter_en;
		title = title_en;
	}

	strncpy(dir, ConfigFileOpenDir, FILEPATH_MAX);
	dir[FILEPATH_MAX - 1] = '\0';
	strncpy(filename, Filename, FILEPATH_MAX);
	filename[FILEPATH_MAX - 1] = '\0';
    i = (int) strlen(filename);
    if (i > 0 && IS_PATH_SEP(filename[i - 1]))
        strlcat(filename, CONFIG_FILE_NAME, FILEPATH_MAX);
    if (i > 0 && strchr(filename, '\"') != 0)
        filename[0] = '\0';

	ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = hwnd;
	ofn.hInstance = hInst;
	ofn.lpstrFilter = filter;
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = filename;
	ofn.nMaxFile = FILEPATH_MAX;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	if (dir[0] != '\0')
		ofn.lpstrInitialDir	= dir;
	else
		ofn.lpstrInitialDir	= 0;
	ofn.lpstrTitle	= title;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER
	| OFN_READONLY | OFN_HIDEREADONLY;
	ofn.lpstrDefExt = 0;
	ofn.lCustData = 0;
	ofn.lpfnHook = 0;
	ofn.lpTemplateName = 0;

	res = SafeGetOpenFileName(&ofn);
	strncpy(ConfigFileOpenDir, dir, FILEPATH_MAX);
	ConfigFileOpenDir[FILEPATH_MAX - 1] = '\0';
	if (res != FALSE) {
		strncpy(Filename, filename, FILEPATH_MAX);
		Filename[FILEPATH_MAX - 1] = '\0';
		return 0;
	}
	else {
		Filename[0] = '\0';
		return -1;
	}
}

static int DlgOpenOutputFile(char *Filename, HWND hwnd)
{
	OPENFILENAMEA ofn;
	char filename[FILEPATH_MAX],
	     dir[FILEPATH_MAX];
	int res;
	static char OutputFileOpenDir[FILEPATH_MAX];
	static int initflag = 1;
	const char *filter,
		   *filter_en = "wave file\0*.wav;*.wave;*.aif;*.aiff;*.aifc;*.au;*.snd;*.audio\0"
				"all files\0*.*\0"
				"\0\0",
		   *filter_jp = "波形ファイル (*.wav;*.aif)\0*.wav;*.wave;*.aif;*.aiff;*.aifc;*.au;*.snd;*.audio\0"
				"すべてのファイル (*.*)\0*.*\0"
				"\0\0";
	const char *title,
		   *title_en = "Output File",
		   *title_jp = "出力ファイルを選ぶ";

	if (CurrentPlayerLanguage == LANGUAGE_JAPANESE) {
		filter = filter_jp;
		title = title_jp;
	}
	else {
		filter = filter_en;
		title = title_en;
	}

	if (initflag) {
		OutputFileOpenDir[0] = '\0';
		initflag = 0;
	}
	strncpy(dir, OutputFileOpenDir, FILEPATH_MAX);
	dir[FILEPATH_MAX - 1] = '\0';
	strncpy(filename, Filename, FILEPATH_MAX);
	filename[FILEPATH_MAX - 1] = '\0';
	if (strlen(filename) > 0 && IS_PATH_SEP(filename[strlen(filename) - 1])) {
		strlcat(filename, "output.wav", FILEPATH_MAX);
	}

	ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = hwnd;
	ofn.hInstance = hInst;
	ofn.lpstrFilter = filter;
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = filename;
	ofn.nMaxFile = FILEPATH_MAX;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	if (dir[0] != '\0')
		ofn.lpstrInitialDir	= dir;
	else
		ofn.lpstrInitialDir	= 0;
	ofn.lpstrTitle	= title;
	ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY;
	ofn.lpstrDefExt = 0;
	ofn.lCustData = 0;
	ofn.lpfnHook = 0;
	ofn.lpTemplateName = 0;

	res = SafeGetSaveFileName(&ofn);
	strncpy(OutputFileOpenDir, dir, FILEPATH_MAX);
	OutputFileOpenDir[FILEPATH_MAX - 1] = '\0';
	if (res != FALSE) {
		strncpy(Filename, filename, FILEPATH_MAX);
		Filename[FILEPATH_MAX - 1] = '\0';
		return 0;
	} else {
		Filename[0] = '\0';
		return -1;
	}
}

static volatile LPITEMIDLIST itemidlist_pre;
int CALLBACK
DlgOpenOutputDirBrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
    switch (uMsg) {
    case BFFM_INITIALIZED:
	if (itemidlist_pre)
	    SendMessage(hwnd, BFFM_SETSELECTION, (WPARAM)0, (LPARAM)itemidlist_pre);
	break;
    default:
	break;
    }
    return 0;
}

static int DlgOpenOutputDir(char *Dirname, HWND hwnd)
{
	static int initflag = 1;
	static char biBuffer[FILEPATH_MAX];
	char Buffer[FILEPATH_MAX];
	BROWSEINFOA bi;
	LPITEMIDLIST itemidlist;
	const char *title,
		   *title_en = "Select output directory.",
		   *title_jp = "出力先のディレクトリを選択してください。";

	if (CurrentPlayerLanguage == LANGUAGE_JAPANESE)
		title = title_jp;
	else
		title = title_en;

	if (initflag == 1) {
		biBuffer[0] = '\0';
		initflag = 0;
	}
	ZeroMemory(&bi, sizeof(bi));
	bi.hwndOwner = hwnd;
	bi.pidlRoot = NULL;
	bi.pszDisplayName = biBuffer;
	bi.lpszTitle = title;
	bi.ulFlags = 0;
	bi.lpfn = DlgOpenOutputDirBrowseCallbackProc;
	bi.lParam = 0;
	bi.iImage = 0;
	itemidlist = SHBrowseForFolderA(&bi);

	if (!itemidlist)
		return -1; /* Cancel */

	SHGetPathFromIDList(itemidlist, Buffer);
	strncpy(biBuffer, Buffer, sizeof(Buffer) - 1);

	if (itemidlist_pre)
		CoTaskMemFree(itemidlist_pre);
	itemidlist_pre = itemidlist;

	directory_form(Buffer);
	strcpy(Dirname, Buffer);
	return 0;
}

volatile int w32g_interactive_id3_tag_set = 0;
int w32g_gogo_id3_tag_dialog(void)
{
	return 0;
}


//
//
// RIFF WAVE ConfigDialog
//
//

volatile wave_ConfigDialogInfo_t wave_ConfigDialogInfo;

// チェックされているか。
#define IS_CHECK(id) CH_GET(id)
// チェックする。
#define CHECK(id) SendDlgItemMessage(hwnd, id, BM_SETCHECK, 1, 0)
// チェックをはずす。
#define UNCHECK(id) SendDlgItemMessage(hwnd, id, BM_SETCHECK, 0, 0)
// id のチェックボックスを設定する。
#define CHECKBOX_SET(id) \
	if (wave_ConfigDialogInfo.opt##id > 0) \
		SendDlgItemMessage(hwnd, id, BM_SETCHECK, 1, 0); \
	else \
		SendDlgItemMessage(hwnd, id, BM_SETCHECK, 0, 0); \
// id のチェックボックスを変数に代入する。
#define CHECKBOX_GET(id) \
	if (CH_GET(id)) \
		wave_ConfigDialogInfo.opt##id = 1; \
	else \
		wave_ConfigDialogInfo.opt##id = 0; \
// id のエディットを設定する。
#define EDIT_SET(id) SendDlgItemMessageA(hwnd, id, WM_SETTEXT, 0, (LPARAM)wave_ConfigDialogInfo.opt##id);
// id のエディットを変数に代入する。
#define EDIT_GET(id, size) SendDlgItemMessageA(hwnd, id, WM_GETTEXT, (WPARAM)size, (LPARAM)wave_ConfigDialogInfo.opt##id);
#define EDIT_GET_RANGE(id, size, min, max) \
{ \
	char tmpbuf[1024]; \
	int value; \
	SendDlgItemMessageA(hwnd, id, WM_GETTEXT, (WPARAM)size, (LPARAM)wave_ConfigDialogInfo.opt##id); \
	value = atoi((char*)wave_ConfigDialogInfo.opt##id); \
	if (value < min) value = min; \
	if (value > max) value = max; \
	sprintf(tmpbuf, "%d", value); \
	strncpy((char*)wave_ConfigDialogInfo.opt##id, tmpbuf, size); \
	(wave_ConfigDialogInfo.opt##id)[size] = '\0'; \
}
// コントロールの有効化
#define ENABLE_CONTROL(id) EnableWindow(GetDlgItem(hwnd, id), TRUE);
// コントロールの無効化
#define DISABLE_CONTROL(id) EnableWindow(GetDlgItem(hwnd, id), FALSE);

static void waveConfigDialogProcControlEnableDisable(HWND hwnd);
static void waveConfigDialogProcControlApply(HWND hwnd);
static void waveConfigDialogProcControlReset(HWND hwnd);
static int wave_ConfigDialogInfoLock();
static int wave_ConfigDialogInfoUnLock();
static LRESULT APIENTRY CALLBACK waveConfigDialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess) {
	case WM_INITDIALOG:
	{
		// 設定
		waveConfigDialogProcControlReset(hwnd);

		SetFocus(DI_GET(IDOK));
	}
		break;
	case WM_KEYUP:
		if (wParam == VK_ESCAPE) {
			PostMessage(hwnd, WM_COMMAND, MAKEWPARAM(0, 0), MAKELPARAM(IDCLOSE, 0));
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCLOSE:
			PostMessage(hwnd, WM_CLOSE, (WPARAM)0, (LPARAM)0);
			break;
		case IDOK:
			waveConfigDialogProcControlApply(hwnd);
			wave_ConfigDialogInfoApply();
			wave_ConfigDialogInfoSaveINI();
			EndDialog(hwnd, TRUE);
			break;
		case IDCANCEL:
			PostMessage(hwnd, WM_CLOSE, (WPARAM)0, (LPARAM)0);
			break;
		default:
			break;
		}
		break;
	case WM_NOTIFY:
		break;
	case WM_CLOSE:
		EndDialog(hwnd, FALSE);
		break;
	default:
		break;
	}
	return FALSE;
}

// コントロールの有効 / 無効化
static void waveConfigDialogProcControlEnableDisable(HWND hwnd)
{
}

static void waveConfigDialogProcControlReset(HWND hwnd)
{
	// エディットの設定
	EDIT_SET(IDC_EDIT_RIFFWAVE_UPDATE_STEP)
	// チェックボックスの設定
	CHECKBOX_SET(IDC_CHECKBOX_RIFFWAVE_EXTENSIBLE)
	// コントロールの有効 / 無効化
	waveConfigDialogProcControlEnableDisable(hwnd);
}

static void waveConfigDialogProcControlApply(HWND hwnd)
{
	// エディットの設定
	EDIT_GET_RANGE(IDC_EDIT_RIFFWAVE_UPDATE_STEP, 6, 0, 99999)
	// チェックボックスの設定
	CHECKBOX_GET(IDC_CHECKBOX_RIFFWAVE_EXTENSIBLE)
	// リセット
	waveConfigDialogProcControlReset(hwnd);
}

#undef CHECKBOX_SET
#undef CHECKBOX_GET
#undef EDIT_SET
#undef EDIT_GET
#undef EDIT_GET_RANGE

extern void wave_set_option_extensible(int);
extern void wave_set_option_update_step(int);

int waveConfigDialog(void)
{
	int changed = 0;
	if (CurrentPlayerLanguage == LANGUAGE_JAPANESE)
		changed = DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG_RIFFWAVE), hPrefWnd, (DLGPROC)waveConfigDialogProc);
	else
		changed = DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG_RIFFWAVE_EN), hPrefWnd, (DLGPROC)waveConfigDialogProc);
	return changed;
}

static int wave_ConfigDialogInfoLock()
{
	return 0;
}

static int wave_ConfigDialogInfoUnLock()
{
	return 0;
}

int wave_ConfigDialogInfoInit(void)
{
	wave_ConfigDialogInfo.optIDC_CHECKBOX_RIFFWAVE_EXTENSIBLE = 0;
	strcpy((char*)wave_ConfigDialogInfo.optIDC_EDIT_RIFFWAVE_UPDATE_STEP, "512");
	return 0;
}

int wave_ConfigDialogInfoApply(void)
{
	wave_ConfigDialogInfoLock();
	wave_set_option_extensible(wave_ConfigDialogInfo.optIDC_CHECKBOX_RIFFWAVE_EXTENSIBLE);
	wave_set_option_update_step(atoi((char*)wave_ConfigDialogInfo.optIDC_EDIT_RIFFWAVE_UPDATE_STEP));
	wave_ConfigDialogInfoUnLock();
	return 0;
}

#define SEC_WAVE	"WAVE"
int wave_ConfigDialogInfoSaveINI(void)
{
	const char *section = SEC_WAVE;
	const char *inifile = timidity_output_inifile;
	char buffer[1024];

#define NUMSAVE(name) \
		sprintf(buffer, "%d", wave_ConfigDialogInfo.name); \
		WritePrivateProfileStringA(section, #name, buffer, inifile);
#define STRSAVE(name, len) \
		WritePrivateProfileStringA(section, (char*)#name, (char*)wave_ConfigDialogInfo.name, inifile);
	STRSAVE(optIDC_EDIT_RIFFWAVE_UPDATE_STEP, 1024)
	NUMSAVE(optIDC_CHECKBOX_RIFFWAVE_EXTENSIBLE)
	WritePrivateProfileStringA(NULL, NULL, NULL, inifile);		// Write Flush
#undef NUMSAVE
#undef STRSAVE
	return 0;
}
int wave_ConfigDialogInfoLoadINI(void)
{
	const char *section = SEC_WAVE;
	const char *inifile = timidity_output_inifile;
	int num;
	char buffer[1024];
#define NUMLOAD(name) \
		num = GetPrivateProfileIntA(section, #name, -1, inifile); \
		if (num != -1) wave_ConfigDialogInfo.name = num;
#define STRLOAD(name, len) \
		GetPrivateProfileStringA(section, (char*)#name, "", buffer, len, inifile); \
		buffer[len - 1] = '\0'; \
		if (buffer[0] != 0) \
			strcpy((char*)wave_ConfigDialogInfo.name, buffer);
	wave_ConfigDialogInfoLock();
	STRLOAD(optIDC_EDIT_RIFFWAVE_UPDATE_STEP, 1024)
	NUMLOAD(optIDC_CHECKBOX_RIFFWAVE_EXTENSIBLE)
#undef NUMLOAD
#undef STRLOAD
	wave_set_option_extensible(wave_ConfigDialogInfo.optIDC_CHECKBOX_RIFFWAVE_EXTENSIBLE);
	wave_set_option_update_step(atoi((char*)wave_ConfigDialogInfo.optIDC_EDIT_RIFFWAVE_UPDATE_STEP));
	wave_ConfigDialogInfoUnLock();
	return 0;
}


#ifdef AU_GOGO
//
//
// gogo ConfigDialog
//
//

// id のコンボボックスの情報の定義
#define CB_INFO_TYPE1_BEGIN(id) static int cb_info_ ## id [] = {
#define CB_INFO_TYPE1_END };
#define CB_INFO_TYPE2_BEGIN(id) static const TCHAR * cb_info_ ## id [] = {
#define CB_INFO_TYPE2_END };

// cb_info_type1_ＩＤ  cb_info_type2_ＩＤ というふうになる。

// IDC_COMBO_OUTPUT_FORMAT
CB_INFO_TYPE2_BEGIN(IDC_COMBO_OUTPUT_FORMAT)
	TEXT("MP3+TAG"), (TCHAR*)MC_OUTPUT_NORMAL,
	TEXT("RIFF/WAVE"), (TCHAR*)MC_OUTPUT_RIFF_WAVE,
	TEXT("RIFF/RMP"), (TCHAR*)MC_OUTPUT_RIFF_RMP,
	NULL
CB_INFO_TYPE2_END

// IDC_COMBO_MPEG1_AUDIO_BITRATE
CB_INFO_TYPE1_BEGIN(IDC_COMBO_MPEG1_AUDIO_BITRATE)
	32,40,48,56,64,80,96,112,128,160,192,224,256,320,-1
CB_INFO_TYPE1_END

// IDC_COMBO_MPEG2_AUDIO_BITRATE
CB_INFO_TYPE1_BEGIN(IDC_COMBO_MPEG2_AUDIO_BITRATE)
	8,16,24,32,40,48,56,64,80,96,112,128,144,160,-1
CB_INFO_TYPE1_END

// IDC_COMBO_ENCODE_MODE
CB_INFO_TYPE2_BEGIN(IDC_COMBO_ENCODE_MODE)
	TEXT("monoral"), (TCHAR*)MC_MODE_MONO,
	TEXT("stereo"), (TCHAR*)MC_MODE_STEREO,
	TEXT("joint stereo"), (TCHAR*)MC_MODE_JOINT,
	TEXT("mid/side stereo"), (TCHAR*)MC_MODE_MSSTEREO,
	TEXT("dual channel"), (TCHAR*)MC_MODE_DUALCHANNEL,
	NULL
CB_INFO_TYPE2_END

// IDC_COMBO_EMPHASIS_TYPE
CB_INFO_TYPE2_BEGIN(IDC_COMBO_EMPHASIS_TYPE)
	TEXT("NONE"), (TCHAR*)MC_EMP_NONE,
	TEXT("50/15ms (normal CD-DA emphasis)"), (TCHAR*)MC_EMP_5015MS,
	TEXT("CCITT"), (TCHAR*)MC_EMP_CCITT,
	NULL
CB_INFO_TYPE2_END

// IDC_COMBO_VBR_BITRATE_LOW
CB_INFO_TYPE1_BEGIN(IDC_COMBO_VBR_BITRATE_LOW)
	32,40,48,56,64,80,96,112,128,160,192,224,256,320,-1
CB_INFO_TYPE1_END

// IDC_COMBO_VBR_BITRATE_HIGH
CB_INFO_TYPE1_BEGIN(IDC_COMBO_VBR_BITRATE_HIGH)
	32,40,48,56,64,80,96,112,128,160,192,224,256,320,-1
CB_INFO_TYPE1_END

// IDC_COMBO_VBR
CB_INFO_TYPE2_BEGIN(IDC_COMBO_VBR)
	TEXT("Quality 0 (320 - 32 kbps)"), (TCHAR*)0,
	TEXT("Quality 1 (256 - 32 kbps)"), (TCHAR*)1,
	TEXT("Quality 2 (256 - 32 kbps)"), (TCHAR*)2,
	TEXT("Quality 3 (256 - 32 kbps)"), (TCHAR*)3,
	TEXT("Quality 4 (256 - 32 kbps)"), (TCHAR*)4,
	TEXT("Quality 5 (224 - 32 kbps)"), (TCHAR*)5,
	TEXT("Quality 6 (192 - 32 kbps)"), (TCHAR*)6,
	TEXT("Quality 7 (160 - 32 kbps)"), (TCHAR*)7,
	TEXT("Quality 8 (128 - 32 kbps)"), (TCHAR*)8,
	TEXT("Quality 9 (128 - 32 kbps)"), (TCHAR*)9,
	NULL
CB_INFO_TYPE2_END

// id のコンボボックスを選択の設定する。
#define CB_SETCURSEL_TYPE1(id) \
{ \
	int cb_num; \
	for(cb_num=0;(int)cb_info_ ## id [cb_num]>=0;cb_num++){ \
		SendDlgItemMessage(hwnd,id,CB_SETCURSEL,(WPARAM)0,(LPARAM)0); \
		if(gogo_ConfigDialogInfo.opt ## id == (int) cb_info_ ## id [cb_num]){ \
			SendDlgItemMessage(hwnd,id,CB_SETCURSEL,(WPARAM)cb_num,(LPARAM)0); \
			break; \
		} \
	} \
}
#define CB_SETCURSEL_TYPE2(id) \
{ \
	int cb_num; \
	for(cb_num=0;(int)cb_info_ ## id [cb_num];cb_num+=2){ \
		SendDlgItemMessage(hwnd,id,CB_SETCURSEL,(WPARAM)0,(LPARAM)0); \
	    if(gogo_ConfigDialogInfo.opt ## id == (int) cb_info_ ## id [cb_num+1]){ \
			SendDlgItemMessage(hwnd,id,CB_SETCURSEL,(WPARAM)cb_num/2,(LPARAM)0); \
			break; \
		} \
	} \
}
// id のコンボボックスの選択を変数に代入する。
#define CB_GETCURSEL_TYPE1(id) \
{ \
	int cb_num1, cb_num2; \
	cb_num1 = SendDlgItemMessage(hwnd,id,CB_GETCURSEL,(WPARAM)0,(LPARAM)0); \
	for(cb_num2=0;(int)cb_info_ ## id [cb_num2]>=0;cb_num2++) \
		if(cb_num1==cb_num2){ \
			gogo_ConfigDialogInfo.opt ## id = (int)cb_info_ ## id [cb_num2]; \
			break; \
		} \
}
#define CB_GETCURSEL_TYPE2(id) \
{ \
	int cb_num1, cb_num2; \
	cb_num1 = SendDlgItemMessage(hwnd,id,CB_GETCURSEL,(WPARAM)0,(LPARAM)0); \
	for(cb_num2=0;(int)cb_info_ ## id [cb_num2];cb_num2+=2) \
		if(cb_num1*2==cb_num2){ \
			gogo_ConfigDialogInfo.opt ## id = (int)cb_info_ ## id [cb_num2+1]; \
			break; \
		} \
}
// チェックされているか。
#define IS_CHECK(id) SendDlgItemMessage(hwnd,id,BM_GETCHECK,0,0)
// チェックする。
#define CHECK(id) SendDlgItemMessage(hwnd,id,BM_SETCHECK,1,0)
// チェックをはずす。
#define UNCHECK(id) SendDlgItemMessage(hwnd,id,BM_SETCHECK,0,0)
// id のチェックボックスを設定する。
#define CHECKBOX_SET(id) \
	if(gogo_ConfigDialogInfo.opt ## id>0) \
		SendDlgItemMessage(hwnd,id,BM_SETCHECK,1,0); \
	else \
		SendDlgItemMessage(hwnd,id,BM_SETCHECK,0,0); \
// id のチェックボックスを変数に代入する。
#define CHECKBOX_GET(id) \
	if(SendDlgItemMessage(hwnd,id,BM_GETCHECK,0,0)) \
		gogo_ConfigDialogInfo.opt ## id = 1; \
	else \
		gogo_ConfigDialogInfo.opt ## id = 0; \
// id のエディットを設定する。
#define EDIT_SET(id) SendDlgItemMessage(hwnd,id,WM_SETTEXT,0,(LPARAM)gogo_ConfigDialogInfo.opt ## id);
// id のエディットを変数に代入する。
#define EDIT_GET(id,size) SendDlgItemMessage(hwnd,id,WM_GETTEXT,(WPARAM)size,(LPARAM)gogo_ConfigDialogInfo.opt ## id);
#define EDIT_GET_RANGE(id,size,min,max) \
{ \
	char tmpbuf[1024]; \
	int value; \
	SendDlgItemMessage(hwnd,id,WM_GETTEXT,(WPARAM)size,(LPARAM)gogo_ConfigDialogInfo.opt ## id); \
	value = atoi((char *)gogo_ConfigDialogInfo.opt ## id); \
	if(value<min) value = min; \
	if(value>max) value = max; \
	sprintf(tmpbuf,"%d",value); \
	strncpy((char *)gogo_ConfigDialogInfo.opt ## id,tmpbuf,size); \
	(gogo_ConfigDialogInfo.opt ## id)[size] = '\0'; \
}
// コントロールの有効化
#define ENABLE_CONTROL(id) EnableWindow(GetDlgItem(hwnd,id),TRUE);
// コントロールの無効化
#define DISABLE_CONTROL(id) EnableWindow(GetDlgItem(hwnd,id),FALSE);

static void gogoConfigDialogProcControlEnableDisable(HWND hwnd);
static void gogoConfigDialogProcControlApply(HWND hwnd);
static void gogoConfigDialogProcControlReset(HWND hwnd);
static int gogo_ConfigDialogInfoLock();
static int gogo_ConfigDialogInfoUnLock();
static LRESULT APIENTRY CALLBACK gogoConfigDialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	char buff[1024];
	switch (uMess){
	case WM_INITDIALOG:
	{
		int i;
		// コンボボックスの初期化
		for(i=0;cb_info_IDC_COMBO_OUTPUT_FORMAT[i];i+=2){
			SendDlgItemMessage(hwnd,IDC_COMBO_OUTPUT_FORMAT,CB_INSERTSTRING,(WPARAM)-1,(LPARAM)cb_info_IDC_COMBO_OUTPUT_FORMAT[i]);
		}
		for(i=0;cb_info_IDC_COMBO_MPEG1_AUDIO_BITRATE[i]>=0;i++){
			sprintf(buff,"%d kbit/sec",cb_info_IDC_COMBO_MPEG1_AUDIO_BITRATE[i]);
			SendDlgItemMessage(hwnd,IDC_COMBO_MPEG1_AUDIO_BITRATE,CB_INSERTSTRING,(WPARAM)-1,(LPARAM)buff);
		}
		for(i=0;cb_info_IDC_COMBO_MPEG2_AUDIO_BITRATE[i]>=0;i++){
			sprintf(buff,"%d kbit/sec",cb_info_IDC_COMBO_MPEG2_AUDIO_BITRATE[i]);
			SendDlgItemMessage(hwnd,IDC_COMBO_MPEG2_AUDIO_BITRATE,CB_INSERTSTRING,(WPARAM)-1,(LPARAM)buff);
		}
		for(i=0;cb_info_IDC_COMBO_ENCODE_MODE[i];i+=2){
			SendDlgItemMessage(hwnd,IDC_COMBO_ENCODE_MODE,CB_INSERTSTRING,(WPARAM)-1,(LPARAM)cb_info_IDC_COMBO_ENCODE_MODE[i]);
		}
		for(i=0;cb_info_IDC_COMBO_EMPHASIS_TYPE[i];i+=2){
			SendDlgItemMessage(hwnd,IDC_COMBO_EMPHASIS_TYPE,CB_INSERTSTRING,(WPARAM)-1,(LPARAM)cb_info_IDC_COMBO_EMPHASIS_TYPE[i]);
		}
		for(i=0;cb_info_IDC_COMBO_VBR_BITRATE_LOW[i]>=0;i++){
			sprintf(buff,"%d kbit/sec",cb_info_IDC_COMBO_VBR_BITRATE_LOW[i]);
			SendDlgItemMessage(hwnd,IDC_COMBO_VBR_BITRATE_LOW,CB_INSERTSTRING,(WPARAM)-1,(LPARAM)buff);
		}
		for(i=0;cb_info_IDC_COMBO_VBR_BITRATE_HIGH[i]>=0;i++){
			sprintf(buff,"%d kbit/sec",cb_info_IDC_COMBO_VBR_BITRATE_HIGH[i]);
			SendDlgItemMessage(hwnd,IDC_COMBO_VBR_BITRATE_HIGH,CB_INSERTSTRING,(WPARAM)-1,(LPARAM)buff);
		}
		for(i=0;cb_info_IDC_COMBO_VBR[i];i+=2){
			SendDlgItemMessage(hwnd,IDC_COMBO_VBR,CB_INSERTSTRING,(WPARAM)-1,(LPARAM)cb_info_IDC_COMBO_VBR[i]);
		}
		// 設定
		gogoConfigDialogProcControlReset(hwnd);

		SetFocus(GetDlgItem(hwnd, IDOK));
	}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCLOSE:
			PostMessage(hwnd,WM_CLOSE,(WPARAM)0,(LPARAM)0);
			break;
		case IDOK:
			gogoConfigDialogProcControlApply(hwnd);
			gogo_ConfigDialogInfoSaveINI();
			PostMessage(hwnd,WM_CLOSE,(WPARAM)0,(LPARAM)0);
			break;
		case IDCANCEL:
			PostMessage(hwnd,WM_CLOSE,(WPARAM)0,(LPARAM)0);
			break;
		case IDC_BUTTON_APPLY:
			gogoConfigDialogProcControlApply(hwnd);
			break;
		case IDC_CHECK_DEFAULT:
			gogoConfigDialogProcControlEnableDisable(hwnd);
			break;
		case IDC_CHECK_COMMANDLINE_OPTS:
			gogoConfigDialogProcControlEnableDisable(hwnd);
			break;
		case IDC_EDIT_COMMANDLINE_OPTION:
			break;
		case IDC_CHECK_OUTPUT_FORMAT:
			gogoConfigDialogProcControlEnableDisable(hwnd);
			break;
		case IDC_COMBO_OUTPUT_FORMAT:
			break;
		case IDC_CHECK_MPEG1AUDIOBITRATE:
			if(IS_CHECK(IDC_CHECK_MPEG1AUDIOBITRATE)){
				CHECK(IDC_CHECK_MPEG2AUDIOBITRATE);
				UNCHECK(IDC_CHECK_VBR);
				UNCHECK(IDC_CHECK_VBR_BITRATE);
			} else {
				UNCHECK(IDC_CHECK_MPEG2AUDIOBITRATE);
			}
			gogoConfigDialogProcControlEnableDisable(hwnd);
			break;
		case IDC_COMBO_MPEG1_AUDIO_BITRATE:
			break;
		case IDC_CHECK_MPEG2AUDIOBITRATE:
			if(IS_CHECK(IDC_CHECK_MPEG2AUDIOBITRATE)){
				CHECK(IDC_CHECK_MPEG1AUDIOBITRATE);
				UNCHECK(IDC_CHECK_VBR);
				UNCHECK(IDC_CHECK_VBR_BITRATE);
			} else {
				UNCHECK(IDC_CHECK_MPEG1AUDIOBITRATE);
			}
			gogoConfigDialogProcControlEnableDisable(hwnd);
			break;
		case IDC_COMBO_MPEG2_AUDIO_BITRATE:
			break;
		case IDC_CHECK_ENHANCED_LOW_PASS_FILTER:
			if(IS_CHECK(IDC_CHECK_ENHANCED_LOW_PASS_FILTER)){
				UNCHECK(IDC_CHECK_16KHZ_LOW_PASS_FILTER);
			}
			gogoConfigDialogProcControlEnableDisable(hwnd);
			break;
		case IDC_EDIT_LPF_PARA1:
			break;
		case IDC_EDIT_LPF_PARA2:
			break;
		case IDC_CHECK_ENCODE_MODE:
			gogoConfigDialogProcControlEnableDisable(hwnd);
			break;
		case IDC_COMBO_ENCODE_MODE:
			break;
		case IDC_CHECK_EMPHASIS_TYPE:
			gogoConfigDialogProcControlEnableDisable(hwnd);
			break;
		case IDC_COMBO_EMPHASIS_TYPE:
			break;
		case IDC_CHECK_OUTFREQ:
			gogoConfigDialogProcControlEnableDisable(hwnd);
			break;
		case IDC_EDIT_OUTFREQ:
			break;
		case IDC_CHECK_MSTHRESHOLD:
			gogoConfigDialogProcControlEnableDisable(hwnd);
			break;
		case IDC_EDIT_MSTHRESHOLD_THRESHOLD:
			break;
		case IDC_EDIT_MSTHRESHOLD_MSPOWER:
			break;
		case IDC_CHECK_USE_CPU_OPTS:
			gogoConfigDialogProcControlEnableDisable(hwnd);
			break;
		case IDC_CHECK_CPUMMX:
			break;
		case IDC_CHECK_CPUSSE:
			break;
		case IDC_CHECK_CPU3DNOW:
			break;
		case IDC_CHECK_CPUE3DNOW:
			break;
		case IDC_CHECK_CPUCMOV:
			break;
		case IDC_CHECK_CPUEMMX:
			break;
		case IDC_CHECK_CPUSSE2:
			break;
		case IDC_CHECK_VBR:
			if(IS_CHECK(IDC_CHECK_VBR)){
				UNCHECK(IDC_COMBO_MPEG1_AUDIO_BITRATE);
				UNCHECK(IDC_COMBO_MPEG2_AUDIO_BITRATE);
			}
			gogoConfigDialogProcControlEnableDisable(hwnd);
			break;
		case IDC_COMBO_VBR:
			break;
		case IDC_CHECK_VBR_BITRATE:
			if(IS_CHECK(IDC_CHECK_VBR_BITRATE)){
				CHECK(IDC_CHECK_VBR);
				UNCHECK(IDC_COMBO_MPEG1_AUDIO_BITRATE);
				UNCHECK(IDC_COMBO_MPEG2_AUDIO_BITRATE);
			}
			gogoConfigDialogProcControlEnableDisable(hwnd);
			break;
		case IDC_COMBO_VBR_BITRATE_LOW:
			break;
		case IDC_COMBO_VBR_BITRATE_HIGH:
			break;
		case IDC_CHECK_USEPSY:
			break;
		case IDC_CHECK_VERIFY:
			break;
		case IDC_CHECK_16KHZ_LOW_PASS_FILTER:
			if(IS_CHECK(IDC_CHECK_16KHZ_LOW_PASS_FILTER)){
				UNCHECK(IDC_CHECK_ENHANCED_LOW_PASS_FILTER);
			}
			gogoConfigDialogProcControlEnableDisable(hwnd);
			break;
		default:
			break;
		}
		break;
	case WM_NOTIFY:
		break;
	case WM_SIZE:
		return FALSE;
	case WM_CLOSE:
//MessageBox(NULL,"CLOSE","CLOSE",MB_OK);
		EndDialog(hwnd, FALSE);
		break;
	case WM_DESTROY:
//MessageBox(NULL,"DESTROY","DESTROY",MB_OK);
		break;
	default:
		break;
	}
	return FALSE;
}

// コントロールの有効 / 無効化
static void gogoConfigDialogProcControlEnableDisable(HWND hwnd)
{
	ENABLE_CONTROL(IDC_CHECK_DEFAULT);
	if(IS_CHECK(IDC_CHECK_DEFAULT)){
		DISABLE_CONTROL(IDC_CHECK_COMMANDLINE_OPTS);
		DISABLE_CONTROL(IDC_EDIT_COMMANDLINE_OPTION);
		DISABLE_CONTROL(IDC_CHECK_OUTPUT_FORMAT);
		DISABLE_CONTROL(IDC_COMBO_OUTPUT_FORMAT);
		DISABLE_CONTROL(IDC_CHECK_MPEG1AUDIOBITRATE);
		DISABLE_CONTROL(IDC_COMBO_MPEG1_AUDIO_BITRATE);
		DISABLE_CONTROL(IDC_CHECK_MPEG2AUDIOBITRATE);
		DISABLE_CONTROL(IDC_COMBO_MPEG2_AUDIO_BITRATE);
		DISABLE_CONTROL(IDC_CHECK_ENHANCED_LOW_PASS_FILTER);
		DISABLE_CONTROL(IDC_EDIT_LPF_PARA1);
		DISABLE_CONTROL(IDC_EDIT_LPF_PARA2);
		DISABLE_CONTROL(IDC_CHECK_ENCODE_MODE);
		DISABLE_CONTROL(IDC_COMBO_ENCODE_MODE);
		DISABLE_CONTROL(IDC_CHECK_EMPHASIS_TYPE);
		DISABLE_CONTROL(IDC_COMBO_EMPHASIS_TYPE);
		DISABLE_CONTROL(IDC_CHECK_OUTFREQ);
		DISABLE_CONTROL(IDC_EDIT_OUTFREQ);
		DISABLE_CONTROL(IDC_CHECK_MSTHRESHOLD);
		DISABLE_CONTROL(IDC_EDIT_MSTHRESHOLD_THRESHOLD);
		DISABLE_CONTROL(IDC_EDIT_MSTHRESHOLD_MSPOWER);
		DISABLE_CONTROL(IDC_CHECK_USE_CPU_OPTS);
		DISABLE_CONTROL(IDC_CHECK_CPUMMX);
		DISABLE_CONTROL(IDC_CHECK_CPUSSE);
		DISABLE_CONTROL(IDC_CHECK_CPU3DNOW);
		DISABLE_CONTROL(IDC_CHECK_CPUE3DNOW);
		DISABLE_CONTROL(IDC_CHECK_CPUCMOV);
		DISABLE_CONTROL(IDC_CHECK_CPUEMMX);
		DISABLE_CONTROL(IDC_CHECK_CPUSSE2);
		DISABLE_CONTROL(IDC_CHECK_VBR);
		DISABLE_CONTROL(IDC_COMBO_VBR);
		DISABLE_CONTROL(IDC_CHECK_VBR_BITRATE);
		DISABLE_CONTROL(IDC_COMBO_VBR_BITRATE_LOW);
		DISABLE_CONTROL(IDC_COMBO_VBR_BITRATE_HIGH);
		DISABLE_CONTROL(IDC_CHECK_USEPSY);
		DISABLE_CONTROL(IDC_CHECK_VERIFY);
		DISABLE_CONTROL(IDC_CHECK_16KHZ_LOW_PASS_FILTER);
	} else {
		ENABLE_CONTROL(IDC_CHECK_COMMANDLINE_OPTS);
		if(IS_CHECK(IDC_CHECK_COMMANDLINE_OPTS)){
			ENABLE_CONTROL(IDC_EDIT_COMMANDLINE_OPTION);
			DISABLE_CONTROL(IDC_CHECK_OUTPUT_FORMAT);
			DISABLE_CONTROL(IDC_COMBO_OUTPUT_FORMAT);
			DISABLE_CONTROL(IDC_CHECK_MPEG1AUDIOBITRATE);
			DISABLE_CONTROL(IDC_COMBO_MPEG1_AUDIO_BITRATE);
			DISABLE_CONTROL(IDC_CHECK_MPEG2AUDIOBITRATE);
			DISABLE_CONTROL(IDC_COMBO_MPEG2_AUDIO_BITRATE);
			DISABLE_CONTROL(IDC_CHECK_ENHANCED_LOW_PASS_FILTER);
			DISABLE_CONTROL(IDC_EDIT_LPF_PARA1);
			DISABLE_CONTROL(IDC_EDIT_LPF_PARA2);
			DISABLE_CONTROL(IDC_CHECK_ENCODE_MODE);
			DISABLE_CONTROL(IDC_COMBO_ENCODE_MODE);
			DISABLE_CONTROL(IDC_CHECK_EMPHASIS_TYPE);
			DISABLE_CONTROL(IDC_COMBO_EMPHASIS_TYPE);
			DISABLE_CONTROL(IDC_CHECK_OUTFREQ);
			DISABLE_CONTROL(IDC_EDIT_OUTFREQ);
			DISABLE_CONTROL(IDC_CHECK_MSTHRESHOLD);
			DISABLE_CONTROL(IDC_EDIT_MSTHRESHOLD_THRESHOLD);
			DISABLE_CONTROL(IDC_EDIT_MSTHRESHOLD_MSPOWER);
			DISABLE_CONTROL(IDC_CHECK_USE_CPU_OPTS);
			DISABLE_CONTROL(IDC_CHECK_CPUMMX);
			DISABLE_CONTROL(IDC_CHECK_CPUSSE);
			DISABLE_CONTROL(IDC_CHECK_CPU3DNOW);
			DISABLE_CONTROL(IDC_CHECK_CPUE3DNOW);
			DISABLE_CONTROL(IDC_CHECK_CPUCMOV);
			DISABLE_CONTROL(IDC_CHECK_CPUEMMX);
			DISABLE_CONTROL(IDC_CHECK_CPUSSE2);
			DISABLE_CONTROL(IDC_CHECK_VBR);
			DISABLE_CONTROL(IDC_COMBO_VBR);
			DISABLE_CONTROL(IDC_CHECK_VBR_BITRATE);
			DISABLE_CONTROL(IDC_COMBO_VBR_BITRATE_LOW);
			DISABLE_CONTROL(IDC_COMBO_VBR_BITRATE_HIGH);
			DISABLE_CONTROL(IDC_CHECK_USEPSY);
			DISABLE_CONTROL(IDC_CHECK_VERIFY);
			DISABLE_CONTROL(IDC_CHECK_16KHZ_LOW_PASS_FILTER);
		} else {
			DISABLE_CONTROL(IDC_EDIT_COMMANDLINE_OPTION);
			ENABLE_CONTROL(IDC_CHECK_OUTPUT_FORMAT);
			if(IS_CHECK(IDC_CHECK_OUTPUT_FORMAT)){
				ENABLE_CONTROL(IDC_COMBO_OUTPUT_FORMAT);
			} else {
				DISABLE_CONTROL(IDC_COMBO_OUTPUT_FORMAT);
			}
			ENABLE_CONTROL(IDC_CHECK_16KHZ_LOW_PASS_FILTER);
			ENABLE_CONTROL(IDC_CHECK_ENHANCED_LOW_PASS_FILTER);
			if(IS_CHECK(IDC_CHECK_16KHZ_LOW_PASS_FILTER)){
				UNCHECK(IDC_CHECK_ENHANCED_LOW_PASS_FILTER);
				DISABLE_CONTROL(IDC_EDIT_LPF_PARA1);
				DISABLE_CONTROL(IDC_EDIT_LPF_PARA2);
			} else {
				if(IS_CHECK(IDC_CHECK_ENHANCED_LOW_PASS_FILTER)){
					UNCHECK(IDC_CHECK_16KHZ_LOW_PASS_FILTER);
					ENABLE_CONTROL(IDC_EDIT_LPF_PARA1);
					ENABLE_CONTROL(IDC_EDIT_LPF_PARA2);
				} else {
					DISABLE_CONTROL(IDC_EDIT_LPF_PARA1);
					DISABLE_CONTROL(IDC_EDIT_LPF_PARA2);
				}
			}
			ENABLE_CONTROL(IDC_CHECK_ENCODE_MODE);
			if(IS_CHECK(IDC_CHECK_ENCODE_MODE)){
				ENABLE_CONTROL(IDC_COMBO_ENCODE_MODE);
			} else {
				DISABLE_CONTROL(IDC_COMBO_ENCODE_MODE);
			}
			ENABLE_CONTROL(IDC_CHECK_EMPHASIS_TYPE);
			if(IS_CHECK(IDC_CHECK_EMPHASIS_TYPE)){
				ENABLE_CONTROL(IDC_COMBO_EMPHASIS_TYPE);
			} else {
				DISABLE_CONTROL(IDC_COMBO_EMPHASIS_TYPE);
			}
			ENABLE_CONTROL(IDC_CHECK_OUTFREQ);
			if(IS_CHECK(IDC_CHECK_OUTFREQ)){
				ENABLE_CONTROL(IDC_EDIT_OUTFREQ);
			} else {
				DISABLE_CONTROL(IDC_EDIT_OUTFREQ);
			}
			ENABLE_CONTROL(IDC_CHECK_MSTHRESHOLD);
			if(IS_CHECK(IDC_CHECK_MSTHRESHOLD)){
				ENABLE_CONTROL(IDC_EDIT_MSTHRESHOLD_THRESHOLD);
				ENABLE_CONTROL(IDC_EDIT_MSTHRESHOLD_MSPOWER);
			} else {
				DISABLE_CONTROL(IDC_EDIT_MSTHRESHOLD_THRESHOLD);
				DISABLE_CONTROL(IDC_EDIT_MSTHRESHOLD_MSPOWER);
			}
			ENABLE_CONTROL(IDC_CHECK_USE_CPU_OPTS);
			if(IS_CHECK(IDC_CHECK_USE_CPU_OPTS)){
				ENABLE_CONTROL(IDC_CHECK_CPUMMX);
				ENABLE_CONTROL(IDC_CHECK_CPUSSE);
				ENABLE_CONTROL(IDC_CHECK_CPU3DNOW);
				ENABLE_CONTROL(IDC_CHECK_CPUE3DNOW);
				ENABLE_CONTROL(IDC_CHECK_CPUCMOV);
				ENABLE_CONTROL(IDC_CHECK_CPUEMMX);
				ENABLE_CONTROL(IDC_CHECK_CPUSSE2);
			} else {
				DISABLE_CONTROL(IDC_CHECK_CPUMMX);
				DISABLE_CONTROL(IDC_CHECK_CPUSSE);
				DISABLE_CONTROL(IDC_CHECK_CPU3DNOW);
				DISABLE_CONTROL(IDC_CHECK_CPUE3DNOW);
				DISABLE_CONTROL(IDC_CHECK_CPUCMOV);
				DISABLE_CONTROL(IDC_CHECK_CPUEMMX);
				DISABLE_CONTROL(IDC_CHECK_CPUSSE2);
			}
			ENABLE_CONTROL(IDC_CHECK_VBR);
			ENABLE_CONTROL(IDC_CHECK_MPEG1AUDIOBITRATE);
			ENABLE_CONTROL(IDC_CHECK_MPEG2AUDIOBITRATE);
			if(IS_CHECK(IDC_CHECK_VBR)){
				ENABLE_CONTROL(IDC_COMBO_VBR);
				ENABLE_CONTROL(IDC_CHECK_VBR_BITRATE);
				if(IS_CHECK(IDC_CHECK_VBR_BITRATE)){
					ENABLE_CONTROL(IDC_COMBO_VBR_BITRATE_LOW);
					ENABLE_CONTROL(IDC_COMBO_VBR_BITRATE_HIGH);
				} else {
					DISABLE_CONTROL(IDC_COMBO_VBR_BITRATE_LOW);
					DISABLE_CONTROL(IDC_COMBO_VBR_BITRATE_HIGH);
				}
				UNCHECK(IDC_CHECK_MPEG1AUDIOBITRATE);
				UNCHECK(IDC_CHECK_MPEG2AUDIOBITRATE);
				DISABLE_CONTROL(IDC_COMBO_MPEG1_AUDIO_BITRATE);
				DISABLE_CONTROL(IDC_COMBO_MPEG2_AUDIO_BITRATE);
			} else {
				UNCHECK(IDC_CHECK_VBR_BITRATE);
				DISABLE_CONTROL(IDC_COMBO_VBR);
				DISABLE_CONTROL(IDC_CHECK_VBR_BITRATE);
				DISABLE_CONTROL(IDC_COMBO_VBR_BITRATE_LOW);
				DISABLE_CONTROL(IDC_COMBO_VBR_BITRATE_HIGH);
				if(IS_CHECK(IDC_CHECK_MPEG1AUDIOBITRATE)){
					ENABLE_CONTROL(IDC_COMBO_MPEG1_AUDIO_BITRATE);
				} else {
					DISABLE_CONTROL(IDC_COMBO_MPEG1_AUDIO_BITRATE);
				}
				if(IS_CHECK(IDC_CHECK_MPEG2AUDIOBITRATE)){
					ENABLE_CONTROL(IDC_COMBO_MPEG2_AUDIO_BITRATE);
				} else {
					DISABLE_CONTROL(IDC_COMBO_MPEG2_AUDIO_BITRATE);
				}
			}
			ENABLE_CONTROL(IDC_CHECK_USEPSY);
			ENABLE_CONTROL(IDC_CHECK_VERIFY);
		}
	}
}

static void gogoConfigDialogProcControlReset(HWND hwnd)
{
	// コンボボックスの選択設定
	CB_SETCURSEL_TYPE2(IDC_COMBO_OUTPUT_FORMAT)
	CB_SETCURSEL_TYPE1(IDC_COMBO_MPEG1_AUDIO_BITRATE)
	CB_SETCURSEL_TYPE1(IDC_COMBO_MPEG2_AUDIO_BITRATE)
	CB_SETCURSEL_TYPE2(IDC_COMBO_ENCODE_MODE)
	CB_SETCURSEL_TYPE2(IDC_COMBO_EMPHASIS_TYPE)
	CB_SETCURSEL_TYPE1(IDC_COMBO_VBR_BITRATE_LOW)
	CB_SETCURSEL_TYPE1(IDC_COMBO_VBR_BITRATE_HIGH)
	CB_SETCURSEL_TYPE2(IDC_COMBO_VBR)
	// チェックボックスの設定
	CHECKBOX_SET(IDC_CHECK_DEFAULT)
	CHECKBOX_SET(IDC_CHECK_COMMANDLINE_OPTS)
	CHECKBOX_SET(IDC_CHECK_OUTPUT_FORMAT)
	CHECKBOX_SET(IDC_CHECK_MPEG1AUDIOBITRATE)
	CHECKBOX_SET(IDC_CHECK_MPEG2AUDIOBITRATE)
	CHECKBOX_SET(IDC_CHECK_ENHANCED_LOW_PASS_FILTER)
	CHECKBOX_SET(IDC_CHECK_ENCODE_MODE)
	CHECKBOX_SET(IDC_CHECK_EMPHASIS_TYPE)
	CHECKBOX_SET(IDC_CHECK_OUTFREQ)
	CHECKBOX_SET(IDC_CHECK_MSTHRESHOLD)
	CHECKBOX_SET(IDC_CHECK_USE_CPU_OPTS)
	CHECKBOX_SET(IDC_CHECK_CPUMMX)
	CHECKBOX_SET(IDC_CHECK_CPUSSE)
	CHECKBOX_SET(IDC_CHECK_CPU3DNOW)
	CHECKBOX_SET(IDC_CHECK_CPUE3DNOW)
	CHECKBOX_SET(IDC_CHECK_CPUCMOV)
	CHECKBOX_SET(IDC_CHECK_CPUEMMX)
	CHECKBOX_SET(IDC_CHECK_CPUSSE2)
	CHECKBOX_SET(IDC_CHECK_VBR)
	CHECKBOX_SET(IDC_CHECK_VBR_BITRATE)
	CHECKBOX_SET(IDC_CHECK_USEPSY)
	CHECKBOX_SET(IDC_CHECK_VERIFY)
	CHECKBOX_SET(IDC_CHECK_16KHZ_LOW_PASS_FILTER)
	// エディットの設定
	EDIT_SET(IDC_EDIT_OUTFREQ)
	EDIT_SET(IDC_EDIT_MSTHRESHOLD_THRESHOLD)
	EDIT_SET(IDC_EDIT_MSTHRESHOLD_MSPOWER)
	EDIT_SET(IDC_EDIT_COMMANDLINE_OPTION)
	EDIT_SET(IDC_EDIT_LPF_PARA1)
	EDIT_SET(IDC_EDIT_LPF_PARA2)
	// コントロールの有効 / 無効化
	gogoConfigDialogProcControlEnableDisable(hwnd);
}

static void gogoConfigDialogProcControlApply(HWND hwnd)
{
	// コンボボックスの選択設定
	CB_GETCURSEL_TYPE2(IDC_COMBO_OUTPUT_FORMAT)
	CB_GETCURSEL_TYPE1(IDC_COMBO_MPEG1_AUDIO_BITRATE)
	CB_GETCURSEL_TYPE1(IDC_COMBO_MPEG2_AUDIO_BITRATE)
	CB_GETCURSEL_TYPE2(IDC_COMBO_ENCODE_MODE)
	CB_GETCURSEL_TYPE2(IDC_COMBO_EMPHASIS_TYPE)
	CB_GETCURSEL_TYPE1(IDC_COMBO_VBR_BITRATE_LOW)
	CB_GETCURSEL_TYPE1(IDC_COMBO_VBR_BITRATE_HIGH)
	CB_GETCURSEL_TYPE2(IDC_COMBO_VBR)
	// チェックボックスの設定
	CHECKBOX_GET(IDC_CHECK_DEFAULT)
	CHECKBOX_GET(IDC_CHECK_COMMANDLINE_OPTS)
	CHECKBOX_GET(IDC_CHECK_OUTPUT_FORMAT)
	CHECKBOX_GET(IDC_CHECK_MPEG1AUDIOBITRATE)
	CHECKBOX_GET(IDC_CHECK_MPEG2AUDIOBITRATE)
	CHECKBOX_GET(IDC_CHECK_ENHANCED_LOW_PASS_FILTER)
	CHECKBOX_GET(IDC_CHECK_ENCODE_MODE)
	CHECKBOX_GET(IDC_CHECK_EMPHASIS_TYPE)
	CHECKBOX_GET(IDC_CHECK_OUTFREQ)
	CHECKBOX_GET(IDC_CHECK_MSTHRESHOLD)
	CHECKBOX_GET(IDC_CHECK_USE_CPU_OPTS)
	CHECKBOX_GET(IDC_CHECK_CPUMMX)
	CHECKBOX_GET(IDC_CHECK_CPUSSE)
	CHECKBOX_GET(IDC_CHECK_CPU3DNOW)
	CHECKBOX_GET(IDC_CHECK_CPUE3DNOW)
	CHECKBOX_GET(IDC_CHECK_CPUCMOV)
	CHECKBOX_GET(IDC_CHECK_CPUEMMX)
	CHECKBOX_GET(IDC_CHECK_CPUSSE2)
	CHECKBOX_GET(IDC_CHECK_VBR)
	CHECKBOX_GET(IDC_CHECK_VBR_BITRATE)
	CHECKBOX_GET(IDC_CHECK_USEPSY)
	CHECKBOX_GET(IDC_CHECK_VERIFY)
	CHECKBOX_GET(IDC_CHECK_16KHZ_LOW_PASS_FILTER)
	// エディットの設定
	EDIT_GET_RANGE(IDC_EDIT_OUTFREQ,6,MIN_OUTPUT_RATE,MAX_OUTPUT_RATE)
	EDIT_GET_RANGE(IDC_EDIT_MSTHRESHOLD_THRESHOLD,4,0,100)
	EDIT_GET_RANGE(IDC_EDIT_MSTHRESHOLD_MSPOWER,4,0,100)
	EDIT_GET(IDC_EDIT_COMMANDLINE_OPTION,1024)
	EDIT_GET_RANGE(IDC_EDIT_LPF_PARA1,4,0,100)
	EDIT_GET_RANGE(IDC_EDIT_LPF_PARA2,4,0,100)
	// コントロールの有効 / 無効化
	gogoConfigDialogProcControlEnableDisable(hwnd);
	// リセット
	gogoConfigDialogProcControlReset(hwnd);
}

#undef CB_INFO_TYPE1_BEGIN
#undef CB_INFO_TYPE1_END
#undef CB_INFO_TYPE2_BEGIN
#undef CB_INFO_TYPE2_END
#undef CB_SETCURSEL_TYPE1
#undef CB_SETCURSEL_TYPE2
#undef CB_GETCURSEL_TYPE1
#undef CB_GETCURSEL_TYPE2
#undef CHECKBOX_SET
#undef CHECKBOX_GET
#undef EDIT_SET
#undef EDIT_GET
#undef EDIT_GET_RANGE

#endif

int gogoConfigDialog(void)
{
	int changed = 0;
#ifdef AU_GOGO
	if (CurrentPlayerLanguage == LANGUAGE_JAPANESE)
		changed = DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG_GOGO), hPrefWnd, (DLGPROC)gogoConfigDialogProc);
	else
		changed = DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG_GOGO_EN), hPrefWnd, (DLGPROC)gogoConfigDialogProc);
#endif
	return changed;
}

#ifdef AU_GOGO

static int gogo_ConfigDialogInfoLock()
{
	return 0;
}
static int gogo_ConfigDialogInfoUnLock()
{
	return 0;
}

volatile gogo_ConfigDialogInfo_t gogo_ConfigDialogInfo;

int gogo_ConfigDialogInfoInit(void)
{
	gogo_ConfigDialogInfo.optIDC_CHECK_DEFAULT = 1;
	gogo_ConfigDialogInfo.optIDC_CHECK_COMMANDLINE_OPTS = 0;
	gogo_ConfigDialogInfo.optIDC_EDIT_COMMANDLINE_OPTION[0] = '\0';
	gogo_ConfigDialogInfo.optIDC_CHECK_OUTPUT_FORMAT = 1;
	gogo_ConfigDialogInfo.optIDC_COMBO_OUTPUT_FORMAT = MC_OUTPUT_NORMAL;
	gogo_ConfigDialogInfo.optIDC_CHECK_MPEG1AUDIOBITRATE = 1;
	gogo_ConfigDialogInfo.optIDC_COMBO_MPEG1_AUDIO_BITRATE = 160;
	gogo_ConfigDialogInfo.optIDC_CHECK_MPEG2AUDIOBITRATE = 1;
	gogo_ConfigDialogInfo.optIDC_COMBO_MPEG2_AUDIO_BITRATE = 80;
	gogo_ConfigDialogInfo.optIDC_CHECK_ENHANCED_LOW_PASS_FILTER = 0;
	strcpy((char *)gogo_ConfigDialogInfo.optIDC_EDIT_LPF_PARA1,"55");
	strcpy((char *)gogo_ConfigDialogInfo.optIDC_EDIT_LPF_PARA2,"70");
	gogo_ConfigDialogInfo.optIDC_CHECK_ENCODE_MODE = 1;
	gogo_ConfigDialogInfo.optIDC_COMBO_ENCODE_MODE = MC_MODE_STEREO;
	gogo_ConfigDialogInfo.optIDC_CHECK_EMPHASIS_TYPE = 1;
	gogo_ConfigDialogInfo.optIDC_COMBO_EMPHASIS_TYPE = MC_EMP_NONE;
	gogo_ConfigDialogInfo.optIDC_CHECK_OUTFREQ = 0;
	strcpy((char *)gogo_ConfigDialogInfo.optIDC_EDIT_OUTFREQ,"44100");
	gogo_ConfigDialogInfo.optIDC_CHECK_MSTHRESHOLD = 0;
	strcpy((char *)gogo_ConfigDialogInfo.optIDC_EDIT_MSTHRESHOLD_THRESHOLD,"75");
	strcpy((char *)gogo_ConfigDialogInfo.optIDC_EDIT_MSTHRESHOLD_MSPOWER,"66");
	gogo_ConfigDialogInfo.optIDC_CHECK_USE_CPU_OPTS = 0;
	gogo_ConfigDialogInfo.optIDC_CHECK_CPUMMX = 0;
	gogo_ConfigDialogInfo.optIDC_CHECK_CPUSSE = 0;
	gogo_ConfigDialogInfo.optIDC_CHECK_CPU3DNOW = 0;
	gogo_ConfigDialogInfo.optIDC_CHECK_CPUE3DNOW = 0;
	gogo_ConfigDialogInfo.optIDC_CHECK_CPUCMOV = 0;
	gogo_ConfigDialogInfo.optIDC_CHECK_CPUEMMX = 0;
	gogo_ConfigDialogInfo.optIDC_CHECK_CPUSSE2 = 0;
	gogo_ConfigDialogInfo.optIDC_CHECK_VBR = 0;
	gogo_ConfigDialogInfo.optIDC_COMBO_VBR = 0;
	gogo_ConfigDialogInfo.optIDC_CHECK_VBR_BITRATE = 0;
	gogo_ConfigDialogInfo.optIDC_COMBO_VBR_BITRATE_LOW = 32;
	gogo_ConfigDialogInfo.optIDC_COMBO_VBR_BITRATE_HIGH = 320;
	gogo_ConfigDialogInfo.optIDC_CHECK_USEPSY = 1;
	gogo_ConfigDialogInfo.optIDC_CHECK_VERIFY = 0;
	gogo_ConfigDialogInfo.optIDC_CHECK_16KHZ_LOW_PASS_FILTER = 1;
	return 0;
}

int gogo_ConfigDialogInfoApply(void)
{
	gogo_ConfigDialogInfoLock();
	if(gogo_ConfigDialogInfo.optIDC_CHECK_DEFAULT>0){
		gogo_opts_reset();
		gogo_ConfigDialogInfoUnLock();
		return 0;
	}
	if(gogo_ConfigDialogInfo.optIDC_CHECK_COMMANDLINE_OPTS>0){
		gogo_opts_reset();
		set_gogo_opts_use_commandline_options((char *)gogo_ConfigDialogInfo.optIDC_EDIT_COMMANDLINE_OPTION);
		gogo_ConfigDialogInfoUnLock();
		return 0;
	}
	if(gogo_ConfigDialogInfo.optIDC_CHECK_OUTPUT_FORMAT>0){
		gogo_opts.optOUTPUT_FORMAT = gogo_ConfigDialogInfo.optIDC_COMBO_OUTPUT_FORMAT;
	}
	if(gogo_ConfigDialogInfo.optIDC_CHECK_MPEG1AUDIOBITRATE>0){
		gogo_opts.optBITRATE1 = gogo_ConfigDialogInfo.optIDC_COMBO_MPEG1_AUDIO_BITRATE;
	}
	if(gogo_ConfigDialogInfo.optIDC_CHECK_MPEG2AUDIOBITRATE>0){
		gogo_opts.optBITRATE2 = gogo_ConfigDialogInfo.optIDC_COMBO_MPEG2_AUDIO_BITRATE;
	}
	if(gogo_ConfigDialogInfo.optIDC_CHECK_ENHANCED_LOW_PASS_FILTER>0){
		gogo_opts.optENHANCEDFILTER_A = atoi((char *)gogo_ConfigDialogInfo.optIDC_EDIT_LPF_PARA1);
		gogo_opts.optENHANCEDFILTER_B = atoi((char *)gogo_ConfigDialogInfo.optIDC_EDIT_LPF_PARA2);
	}
	if(gogo_ConfigDialogInfo.optIDC_CHECK_ENCODE_MODE>0){
		gogo_opts.optENCODEMODE = gogo_ConfigDialogInfo.optIDC_COMBO_ENCODE_MODE;
	}
	if(gogo_ConfigDialogInfo.optIDC_CHECK_EMPHASIS_TYPE>0){
		gogo_opts.optEMPHASIS = gogo_ConfigDialogInfo.optIDC_COMBO_EMPHASIS_TYPE;
	}
	if(gogo_ConfigDialogInfo.optIDC_CHECK_OUTFREQ>0){
		gogo_opts.optOUTFREQ = atoi((char *)gogo_ConfigDialogInfo.optIDC_EDIT_OUTFREQ);
	}
	if(gogo_ConfigDialogInfo.optIDC_CHECK_MSTHRESHOLD>0){
		gogo_opts.optMSTHRESHOLD_threshold = atoi((char *)gogo_ConfigDialogInfo.optIDC_EDIT_MSTHRESHOLD_THRESHOLD);
		gogo_opts.optMSTHRESHOLD_mspower = atoi((char *)gogo_ConfigDialogInfo.optIDC_EDIT_MSTHRESHOLD_MSPOWER);
	}
	if(gogo_ConfigDialogInfo.optIDC_CHECK_USE_CPU_OPTS>0){
		gogo_opts.optUSECPUOPT = 1;
		gogo_opts.optUSEMMX = gogo_ConfigDialogInfo.optIDC_CHECK_CPUMMX;
		gogo_opts.optUSEKNI = gogo_ConfigDialogInfo.optIDC_CHECK_CPUSSE;
		gogo_opts.optUSE3DNOW = gogo_ConfigDialogInfo.optIDC_CHECK_CPU3DNOW;
		gogo_opts.optUSEE3DNOW = gogo_ConfigDialogInfo.optIDC_CHECK_CPUE3DNOW;
		gogo_opts.optUSESSE = gogo_ConfigDialogInfo.optIDC_CHECK_CPUSSE;
		gogo_opts.optUSECMOV = gogo_ConfigDialogInfo.optIDC_CHECK_CPUCMOV;
		gogo_opts.optUSEEMMX = gogo_ConfigDialogInfo.optIDC_CHECK_CPUEMMX;
		gogo_opts.optUSESSE2 = gogo_ConfigDialogInfo.optIDC_CHECK_CPUSSE2;
	} else {
		gogo_opts.optUSECPUOPT = 0;
	}
	if(gogo_ConfigDialogInfo.optIDC_CHECK_VBR>0){
		gogo_opts.optVBR = gogo_ConfigDialogInfo.optIDC_COMBO_VBR;
	}
	if(gogo_ConfigDialogInfo.optIDC_CHECK_VBR_BITRATE>0){
		gogo_opts.optVBRBITRATE_low = gogo_ConfigDialogInfo.optIDC_COMBO_VBR_BITRATE_LOW;
		gogo_opts.optVBRBITRATE_high = gogo_ConfigDialogInfo.optIDC_COMBO_VBR_BITRATE_HIGH;
	}
	if(gogo_ConfigDialogInfo.optIDC_CHECK_USEPSY>0){
		gogo_opts.optUSEPSY = TRUE;
	}
	if(gogo_ConfigDialogInfo.optIDC_CHECK_VERIFY>0){
		gogo_opts.optVERIFY = TRUE;
	}
	if(gogo_ConfigDialogInfo.optIDC_CHECK_16KHZ_LOW_PASS_FILTER>0){
		gogo_opts.optUSELPF16 = TRUE;
	}
//	gogo_opts.optINPFREQ;			// SYSTEM USE(システムで使用するから指定できない)
//	gogo_opts.optSTARTOFFSET;	// SYSTEM USE
//	gogo_opts.optADDTAGnum;		// SYSTEM USE
//	gogo_opts.optADDTAG_len[64];	// SYSTEM USE
//	gogo_opts.optADDTAG_buf[64];	// SYSTEM USE
//	gogo_opts.optCPU;					// PREPAIRING(準備中)
//	gogo_opts.optBYTE_SWAP;			// SYSTEM USE
//	gogo_opts.opt8BIT_PCM;			// SYSTEM USE
//	gogo_opts.optMONO_PCM;		// SYSTEM USE
//	gogo_opts.optTOWNS_SND;			// SYSTEM USE
//	gogo_opts.optTHREAD_PRIORITY;	// PREPARING
//	gogo_opts.optREADTHREAD_PRIORITY;	// PREPARING
//	gogo_opts.optOUTPUTDIR[FILEPATH_MAX];			// SYSTEM USE
//	gogo_opts.output_name[FILEPATH_MAX];				// SYSTEM USE
	gogo_ConfigDialogInfoUnLock();
	return 0;
}

#define SEC_GOGO	"gogo"
int gogo_ConfigDialogInfoSaveINI(void)
{
	char *section = SEC_GOGO;
	char *inifile = timidity_output_inifile;
	char buffer[1024];
#define NUMSAVE(name) \
		sprintf(buffer,"%d",gogo_ConfigDialogInfo.name ); \
		WritePrivateProfileString(section, #name ,buffer,inifile);
#define STRSAVE(name) \
		WritePrivateProfileString(section,(char *) #name ,(char *)gogo_ConfigDialogInfo.name ,inifile);
	NUMSAVE(optIDC_CHECK_DEFAULT)
	NUMSAVE(optIDC_CHECK_COMMANDLINE_OPTS)
	STRSAVE(optIDC_EDIT_COMMANDLINE_OPTION)
	NUMSAVE(optIDC_CHECK_OUTPUT_FORMAT)
	NUMSAVE(optIDC_COMBO_OUTPUT_FORMAT)
	NUMSAVE(optIDC_CHECK_MPEG1AUDIOBITRATE)
	NUMSAVE(optIDC_COMBO_MPEG1_AUDIO_BITRATE)
	NUMSAVE(optIDC_CHECK_MPEG2AUDIOBITRATE)
	NUMSAVE(optIDC_COMBO_MPEG2_AUDIO_BITRATE)
	NUMSAVE(optIDC_CHECK_ENHANCED_LOW_PASS_FILTER)
	STRSAVE(optIDC_EDIT_LPF_PARA1)
	STRSAVE(optIDC_EDIT_LPF_PARA2)
	NUMSAVE(optIDC_CHECK_ENCODE_MODE)
	NUMSAVE(optIDC_COMBO_ENCODE_MODE)
	NUMSAVE(optIDC_CHECK_EMPHASIS_TYPE)
	NUMSAVE(optIDC_COMBO_EMPHASIS_TYPE)
	NUMSAVE(optIDC_CHECK_OUTFREQ)
	STRSAVE(optIDC_EDIT_OUTFREQ)
	NUMSAVE(optIDC_CHECK_MSTHRESHOLD)
	STRSAVE(optIDC_EDIT_MSTHRESHOLD_THRESHOLD)
	STRSAVE(optIDC_EDIT_MSTHRESHOLD_MSPOWER)
	NUMSAVE(optIDC_CHECK_USE_CPU_OPTS)
	NUMSAVE(optIDC_CHECK_CPUMMX)
	NUMSAVE(optIDC_CHECK_CPUSSE)
	NUMSAVE(optIDC_CHECK_CPU3DNOW)
	NUMSAVE(optIDC_CHECK_CPUE3DNOW)
	NUMSAVE(optIDC_CHECK_CPUCMOV)
	NUMSAVE(optIDC_CHECK_CPUEMMX)
	NUMSAVE(optIDC_CHECK_CPUSSE2)
	NUMSAVE(optIDC_CHECK_VBR)
	NUMSAVE(optIDC_COMBO_VBR)
	NUMSAVE(optIDC_CHECK_VBR_BITRATE)
	NUMSAVE(optIDC_COMBO_VBR_BITRATE_LOW)
	NUMSAVE(optIDC_COMBO_VBR_BITRATE_HIGH)
	NUMSAVE(optIDC_CHECK_USEPSY)
	NUMSAVE(optIDC_CHECK_VERIFY)
	NUMSAVE(optIDC_CHECK_16KHZ_LOW_PASS_FILTER)
	WritePrivateProfileString(NULL,NULL,NULL,inifile);		// Write Flush
#undef NUMSAVE
#undef STRSAVE
	return 0;
}
int gogo_ConfigDialogInfoLoadINI(void)
{
	char *section = SEC_GOGO;
	char *inifile = timidity_output_inifile;
	int num;
	char buffer[1024];
#define NUMLOAD(name) \
		num = GetPrivateProfileInt(section, #name ,-1,inifile); \
		if(num!=-1) gogo_ConfigDialogInfo.name = num;
#define STRLOAD(name,len) \
		GetPrivateProfileString(section,(char *) #name ,"",buffer,len,inifile); \
		buffer[len-1] = '\0'; \
		if(buffer[0]!=0) \
			strcpy((char *)gogo_ConfigDialogInfo.name ,buffer);
	gogo_ConfigDialogInfoLock();
	NUMLOAD(optIDC_CHECK_DEFAULT)
	NUMLOAD(optIDC_CHECK_COMMANDLINE_OPTS)
	STRLOAD(optIDC_EDIT_COMMANDLINE_OPTION,1024)
	NUMLOAD(optIDC_CHECK_OUTPUT_FORMAT)
	NUMLOAD(optIDC_COMBO_OUTPUT_FORMAT)
	NUMLOAD(optIDC_CHECK_MPEG1AUDIOBITRATE)
	NUMLOAD(optIDC_COMBO_MPEG1_AUDIO_BITRATE)
	NUMLOAD(optIDC_CHECK_MPEG2AUDIOBITRATE)
	NUMLOAD(optIDC_COMBO_MPEG2_AUDIO_BITRATE)
	NUMLOAD(optIDC_CHECK_ENHANCED_LOW_PASS_FILTER)
	STRLOAD(optIDC_EDIT_LPF_PARA1,4)
	STRLOAD(optIDC_EDIT_LPF_PARA2,4)
	NUMLOAD(optIDC_CHECK_ENCODE_MODE)
	NUMLOAD(optIDC_COMBO_ENCODE_MODE)
	NUMLOAD(optIDC_CHECK_EMPHASIS_TYPE)
	NUMLOAD(optIDC_COMBO_EMPHASIS_TYPE)
	NUMLOAD(optIDC_CHECK_OUTFREQ)
	STRLOAD(optIDC_EDIT_OUTFREQ,6)
	NUMLOAD(optIDC_CHECK_MSTHRESHOLD)
	STRLOAD(optIDC_EDIT_MSTHRESHOLD_THRESHOLD,4)
	STRLOAD(optIDC_EDIT_MSTHRESHOLD_MSPOWER,4)
	NUMLOAD(optIDC_CHECK_USE_CPU_OPTS)
	NUMLOAD(optIDC_CHECK_CPUMMX)
	NUMLOAD(optIDC_CHECK_CPUSSE)
	NUMLOAD(optIDC_CHECK_CPU3DNOW)
	NUMLOAD(optIDC_CHECK_CPUE3DNOW)
	NUMLOAD(optIDC_CHECK_CPUCMOV)
	NUMLOAD(optIDC_CHECK_CPUEMMX)
	NUMLOAD(optIDC_CHECK_CPUSSE2)
	NUMLOAD(optIDC_CHECK_VBR)
	NUMLOAD(optIDC_COMBO_VBR)
	NUMLOAD(optIDC_CHECK_VBR_BITRATE)
	NUMLOAD(optIDC_COMBO_VBR_BITRATE_LOW)
	NUMLOAD(optIDC_COMBO_VBR_BITRATE_HIGH)
	NUMLOAD(optIDC_CHECK_USEPSY)
	NUMLOAD(optIDC_CHECK_VERIFY)
	NUMLOAD(optIDC_CHECK_16KHZ_LOW_PASS_FILTER)
#undef NUMLOAD
#undef STRLOAD
	gogo_ConfigDialogInfoUnLock();
	return 0;
}

#endif	// AU_GOGO


#ifdef AU_VORBIS
//
//
// vorbis ConfigDialog
//
//

volatile vorbis_ConfigDialogInfo_t vorbis_ConfigDialogInfo;

// id のコンボボックスの情報の定義
#define CB_INFO_TYPE1_BEGIN(id) static int cb_info_ ## id [] = {
#define CB_INFO_TYPE1_END };
#define CB_INFO_TYPE2_BEGIN(id) static const TCHAR * cb_info_ ## id [] = {
#define CB_INFO_TYPE2_END };

// cb_info_type1_ＩＤ  cb_info_type2_ＩＤ というふうになる。

// IDC_COMBO_MODE_jp
CB_INFO_TYPE2_BEGIN(IDC_COMBO_MODE_jp)
	TEXT("VBR 品質 1 (低)"), (TCHAR*)1,
	TEXT("VBR 品質 2"), (TCHAR*)2,
	TEXT("VBR 品質 3"), (TCHAR*)3,
	TEXT("VBR 品質 4"), (TCHAR*)4,
	TEXT("VBR 品質 4.99"), (TCHAR*)499,
	TEXT("VBR 品質 5"), (TCHAR*)5,
	TEXT("VBR 品質 6"), (TCHAR*)6,
	TEXT("VBR 品質 7"), (TCHAR*)7,
	TEXT("VBR 品質 8 (デフォルト)"), (TCHAR*)8,
	TEXT("VBR 品質 9"), (TCHAR*)9,
	TEXT("VBR 品質 10 (高)"), (TCHAR*)10,
#if 0
	"デフォルト(約128kbps VBR)",(char *)0,
	"約112kbps VBR",(char *)1,
	"約128kbps VBR",(char *)2,
	"約160kbps VBR",(char *)3,
	"約192kbps VBR",(char *)4,
	"約256kbps VBR",(char *)5,
	"約350kbps VBR",(char *)6,
#endif
	NULL
CB_INFO_TYPE2_END

// IDC_COMBO_MODE_en
CB_INFO_TYPE2_BEGIN(IDC_COMBO_MODE_en)
	TEXT("VBR Quality 1 (low)"), (TCHAR*)1,
	TEXT("VBR Quality 2"), (TCHAR*)2,
	TEXT("VBR Quality 3"), (TCHAR*)3,
	TEXT("VBR Quality 4"), (TCHAR*)4,
	TEXT("VBR Quality 4.99"), (TCHAR*)499,
	TEXT("VBR Quality 5"), (TCHAR*)5,
	TEXT("VBR Quality 6"), (TCHAR*)6,
	TEXT("VBR Quality 7"), (TCHAR*)7,
	TEXT("VBR Quality 8 (default)"), (TCHAR*)8,
	TEXT("VBR Quality 9"), (TCHAR*)9,
	TEXT("VBR Quality 10 (high)"), (TCHAR*)10,
#if 0
	"Default (About 128kbps VBR)",(char *)0,
	"About 112kbps VBR",(char *)1,
	"About 128kbps VBR",(char *)2,
	"About 160kbps VBR",(char *)3,
	"About 192kbps VBR",(char *)4,
	"About 256kbps VBR",(char *)5,
	"About 350kbps VBR",(char *)6,
#endif
	NULL
CB_INFO_TYPE2_END

static const TCHAR **cb_info_IDC_COMBO_MODE;

// id のコンボボックスを選択の設定する。
#define CB_SETCURSEL_TYPE1(id) \
{ \
	int cb_num; \
	for(cb_num=0;(int)cb_info_ ## id [cb_num]>=0;cb_num++){ \
		SendDlgItemMessage(hwnd,id,CB_SETCURSEL,(WPARAM)0,(LPARAM)0); \
		if(vorbis_ConfigDialogInfo.opt ## id == (int) cb_info_ ## id [cb_num]){ \
			SendDlgItemMessage(hwnd,id,CB_SETCURSEL,(WPARAM)cb_num,(LPARAM)0); \
			break; \
		} \
	} \
}
#define CB_SETCURSEL_TYPE2(id) \
{ \
	int cb_num; \
	for(cb_num=0;(int)cb_info_ ## id [cb_num];cb_num+=2){ \
		SendDlgItemMessage(hwnd,id,CB_SETCURSEL,(WPARAM)0,(LPARAM)0); \
	    if(vorbis_ConfigDialogInfo.opt ## id == (int) cb_info_ ## id [cb_num+1]){ \
			SendDlgItemMessage(hwnd,id,CB_SETCURSEL,(WPARAM)cb_num/2,(LPARAM)0); \
			break; \
		} \
	} \
}
// id のコンボボックスの選択を変数に代入する。
#define CB_GETCURSEL_TYPE1(id) \
{ \
	int cb_num1, cb_num2; \
	cb_num1 = SendDlgItemMessage(hwnd,id,CB_GETCURSEL,(WPARAM)0,(LPARAM)0); \
	for(cb_num2=0;(int)cb_info_ ## id [cb_num2]>=0;cb_num2++) \
		if(cb_num1==cb_num2){ \
			vorbis_ConfigDialogInfo.opt ## id = (int)cb_info_ ## id [cb_num2]; \
			break; \
		} \
}
#define CB_GETCURSEL_TYPE2(id) \
{ \
	int cb_num1, cb_num2; \
	cb_num1 = SendDlgItemMessage(hwnd,id,CB_GETCURSEL,(WPARAM)0,(LPARAM)0); \
	for(cb_num2=0;(int)cb_info_ ## id [cb_num2];cb_num2+=2) \
		if(cb_num1*2==cb_num2){ \
			vorbis_ConfigDialogInfo.opt ## id = (int)cb_info_ ## id [cb_num2+1]; \
			break; \
		} \
}
// チェックされているか。
#define IS_CHECK(id) SendDlgItemMessage(hwnd,id,BM_GETCHECK,0,0)
// チェックする。
#define CHECK(id) SendDlgItemMessage(hwnd,id,BM_SETCHECK,1,0)
// チェックをはずす。
#define UNCHECK(id) SendDlgItemMessage(hwnd,id,BM_SETCHECK,0,0)
// id のチェックボックスを設定する。
#define CHECKBOX_SET(id) \
	if(vorbis_ConfigDialogInfo.opt ## id>0) \
		SendDlgItemMessage(hwnd,id,BM_SETCHECK,1,0); \
	else \
		SendDlgItemMessage(hwnd,id,BM_SETCHECK,0,0); \
// id のチェックボックスを変数に代入する。
#define CHECKBOX_GET(id) \
	if(SendDlgItemMessage(hwnd,id,BM_GETCHECK,0,0)) \
		vorbis_ConfigDialogInfo.opt ## id = 1; \
	else \
		vorbis_ConfigDialogInfo.opt ## id = 0; \
// id のエディットを設定する。
#define EDIT_SET(id) SendDlgItemMessage(hwnd,id,WM_SETTEXT,0,(LPARAM)vorbis_ConfigDialogInfo.opt ## id);
// id のエディットを変数に代入する。
#define EDIT_GET(id,size) SendDlgItemMessage(hwnd,id,WM_GETTEXT,(WPARAM)size,(LPARAM)vorbis_ConfigDialogInfo.opt ## id);
#define EDIT_GET_RANGE(id,size,min,max) \
{ \
	char tmpbuf[1024]; \
	int value; \
	SendDlgItemMessage(hwnd,id,WM_GETTEXT,(WPARAM)size,(LPARAM)vorbis_ConfigDialogInfo.opt ## id); \
	value = atoi((char *)vorbis_ConfigDialogInfo.opt ## id); \
	if(value<min) value = min; \
	if(value>max) value = max; \
	sprintf(tmpbuf,"%d",value); \
	strncpy((char *)vorbis_ConfigDialogInfo.opt ## id,tmpbuf,size); \
	(vorbis_ConfigDialogInfo.opt ## id)[size] = '\0'; \
}
// コントロールの有効化
#define ENABLE_CONTROL(id) EnableWindow(GetDlgItem(hwnd,id),TRUE);
// コントロールの無効化
#define DISABLE_CONTROL(id) EnableWindow(GetDlgItem(hwnd,id),FALSE);


static void vorbisConfigDialogProcControlEnableDisable(HWND hwnd);
static void vorbisConfigDialogProcControlApply(HWND hwnd);
static void vorbisConfigDialogProcControlReset(HWND hwnd);
static int vorbis_ConfigDialogInfoLock();
static int vorbis_ConfigDialogInfoUnLock();
static LRESULT APIENTRY CALLBACK vorbisConfigDialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess){
	case WM_INITDIALOG:
	{
		int i;
		// コンボボックスの初期化
		if (CurrentPlayerLanguage == LANGUAGE_JAPANESE)
		  cb_info_IDC_COMBO_MODE = cb_info_IDC_COMBO_MODE_jp;
		else
		  cb_info_IDC_COMBO_MODE = cb_info_IDC_COMBO_MODE_en;

		for(i=0;cb_info_IDC_COMBO_MODE[i];i+=2){
			SendDlgItemMessage(hwnd,IDC_COMBO_MODE,CB_INSERTSTRING,(WPARAM)-1,(LPARAM)cb_info_IDC_COMBO_MODE[i]);
		}
		// 設定
		vorbisConfigDialogProcControlReset(hwnd);

		SetFocus(GetDlgItem(hwnd, IDOK));
	}
		break;
	case WM_KEYUP:
		if(wParam == VK_ESCAPE) {
			PostMessage(hwnd,WM_COMMAND,MAKEWPARAM(0,0),MAKELPARAM(IDCLOSE,0));
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCLOSE:
			PostMessage(hwnd,WM_CLOSE,(WPARAM)0,(LPARAM)0);
			break;
		case IDOK:
			vorbisConfigDialogProcControlApply(hwnd);
			PostMessage(hwnd,WM_CLOSE,(WPARAM)0,(LPARAM)0);
			break;
		case IDCANCEL:
			PostMessage(hwnd,WM_CLOSE,(WPARAM)0,(LPARAM)0);
			break;
		case IDC_BUTTON_APPLY:
			vorbisConfigDialogProcControlApply(hwnd);
			break;
		case IDC_CHECK_DEFAULT:
			vorbisConfigDialogProcControlEnableDisable(hwnd);
			break;
		case IDC_COMBO_MODE:
			break;
		case IDC_CHECK_USE_TAG:
			vorbisConfigDialogProcControlEnableDisable(hwnd);
			break;
		default:
			break;
		}
		break;
	case WM_NOTIFY:
		break;
	case WM_CLOSE:
		vorbis_ConfigDialogInfoSaveINI();
		EndDialog(hwnd, FALSE);
		break;
	default:
		break;
	}
	return FALSE;
}

// コントロールの有効 / 無効化
static void vorbisConfigDialogProcControlEnableDisable(HWND hwnd)
{
	ENABLE_CONTROL(IDC_CHECK_DEFAULT);
	if(IS_CHECK(IDC_CHECK_DEFAULT)){
		DISABLE_CONTROL(IDC_COMBO_MODE);
	} else {
		ENABLE_CONTROL(IDC_COMBO_MODE);
	}
	if(IS_CHECK(IDC_CHECK_USE_TAG)){
		ENABLE_CONTROL(IDC_EDIT1);
		ENABLE_CONTROL(IDC_EDIT2);
		ENABLE_CONTROL(IDC_EDIT3);
	} else {
		DISABLE_CONTROL(IDC_EDIT1);
		DISABLE_CONTROL(IDC_EDIT2);
		DISABLE_CONTROL(IDC_EDIT3);
	}
}

static void vorbisConfigDialogProcControlReset(HWND hwnd)
{
	// コンボボックスの選択設定
	CB_SETCURSEL_TYPE2(IDC_COMBO_MODE)
	// チェックボックスの設定
	CHECKBOX_SET(IDC_CHECK_DEFAULT)
	// エディットの設定
	EDIT_SET(IDC_EDIT1);
	EDIT_SET(IDC_EDIT2);
	EDIT_SET(IDC_EDIT3);
	// コントロールの有効 / 無効化
	vorbisConfigDialogProcControlEnableDisable(hwnd);
}

static void vorbisConfigDialogProcControlApply(HWND hwnd)
{
	// コンボボックスの選択設定
	CB_GETCURSEL_TYPE2(IDC_COMBO_MODE)
	// チェックボックスの設定
	CHECKBOX_GET(IDC_CHECK_DEFAULT)
	// エディットの設定
	EDIT_GET(IDC_EDIT1,256-1);
	EDIT_GET(IDC_EDIT2,256-1);
	EDIT_GET(IDC_EDIT3,256-1);
	// コントロールの有効 / 無効化
	vorbisConfigDialogProcControlEnableDisable(hwnd);
	// リセット
	vorbisConfigDialogProcControlReset(hwnd);
}

#undef CB_INFO_TYPE1_BEGIN
#undef CB_INFO_TYPE1_END
#undef CB_INFO_TYPE2_BEGIN
#undef CB_INFO_TYPE2_END
#undef CB_SETCURSEL_TYPE1
#undef CB_SETCURSEL_TYPE2
#undef CB_GETCURSEL_TYPE1
#undef CB_GETCURSEL_TYPE2
#undef CHECKBOX_SET
#undef CHECKBOX_GET
#undef EDIT_SET
#undef EDIT_GET
#undef EDIT_GET_RANGE

#endif

int vorbisConfigDialog(void)
{
	int changed = 0;
#ifdef AU_VORBIS
	if (CurrentPlayerLanguage == LANGUAGE_JAPANESE)
		changed = DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG_VORBIS), hPrefWnd, (DLGPROC)vorbisConfigDialogProc);
	else
		changed = DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG_VORBIS_EN), hPrefWnd, (DLGPROC)vorbisConfigDialogProc);
#endif
	return changed;
}

#ifdef AU_VORBIS

static int vorbis_ConfigDialogInfoLock()
{
	return 0;
}
static int vorbis_ConfigDialogInfoUnLock()
{
	return 0;
}

int vorbis_ConfigDialogInfoInit(void)
{
	vorbis_ConfigDialogInfo.optIDC_CHECK_DEFAULT = 1;
	vorbis_ConfigDialogInfo.optIDC_COMBO_MODE = 0;
	vorbis_ConfigDialogInfo.optIDC_CHECK_USE_TAG = 0;
	vorbis_ConfigDialogInfo.optIDC_EDIT1[0] = '\0';
	vorbis_ConfigDialogInfo.optIDC_EDIT2[0] = '\0';
	vorbis_ConfigDialogInfo.optIDC_EDIT3[0] = '\0';
	return 0;
}

extern volatile int ogg_vorbis_mode;
int vorbis_ConfigDialogInfoApply(void)
{
	vorbis_ConfigDialogInfoLock();
	if(vorbis_ConfigDialogInfo.optIDC_CHECK_DEFAULT>0){
//		vorbis_opts_reset();
		vorbis_ConfigDialogInfoUnLock();
		return 0;
	}
	ogg_vorbis_mode = vorbis_ConfigDialogInfo.optIDC_COMBO_MODE;
	vorbis_ConfigDialogInfoUnLock();
	return 0;
}

#define SEC_VORBIS	"vorbis"
int vorbis_ConfigDialogInfoSaveINI(void)
{
	char *section = SEC_VORBIS;
	char *inifile = timidity_output_inifile;
	char buffer[1024];
//	int len;
#define NUMSAVE(name) \
		sprintf(buffer,"%d",vorbis_ConfigDialogInfo.name ); \
		WritePrivateProfileString(section, #name ,buffer,inifile);
#define STRSAVE(name,len) \
		WritePrivateProfileString(section,(char *) #name ,(char *)vorbis_ConfigDialogInfo.name ,inifile);
	NUMSAVE(optIDC_CHECK_DEFAULT)
	NUMSAVE(optIDC_COMBO_MODE)
	NUMSAVE(optIDC_CHECK_USE_TAG)
	STRSAVE(optIDC_EDIT1,256)
	STRSAVE(optIDC_EDIT2,256)
	STRSAVE(optIDC_EDIT3,256)
	WritePrivateProfileString(NULL,NULL,NULL,inifile);		// Write Flush
#undef NUMSAVE
#undef STRSAVE
	return 0;
}
int vorbis_ConfigDialogInfoLoadINI(void)
{
	char *section = SEC_VORBIS;
	char *inifile = timidity_output_inifile;
	int num;
	char buffer[1024];
#define NUMLOAD(name) \
		num = GetPrivateProfileInt(section, #name ,-1,inifile); \
		if(num!=-1) vorbis_ConfigDialogInfo.name = num;
#define STRLOAD(name,len) \
		GetPrivateProfileString(section,(char *) #name ,"",buffer,len,inifile); \
		buffer[len-1] = '\0'; \
		if(buffer[0]!=0) \
			strcpy((char *)vorbis_ConfigDialogInfo.name ,buffer);
	vorbis_ConfigDialogInfoLock();
	NUMLOAD(optIDC_CHECK_DEFAULT)
	NUMLOAD(optIDC_COMBO_MODE)
	NUMLOAD(optIDC_CHECK_USE_TAG)
	STRLOAD(optIDC_EDIT1,256)
	STRLOAD(optIDC_EDIT2,256)
	STRLOAD(optIDC_EDIT3,256)
#undef NUMLOAD
#undef STRLOAD
	vorbis_ConfigDialogInfoUnLock();
	return 0;
}

#endif	// AU_VORBIS


#ifdef AU_LAME
//
//
// LAME ConfigDialog
//
//

volatile lame_ConfigDialogInfo_t lame_ConfigDialogInfo;

// id のコンボボックスの情報の定義
#define CB_INFO_TYPE2_BEGIN(id) static const TCHAR * cb_info_lame_ ## id [] = {
#define CB_INFO_TYPE2_END };

// cb_info_type1_ＩＤ  cb_info_type2_ＩＤ というふうになる。

// IDC_LAME_CBPRESET_en
CB_INFO_TYPE2_BEGIN(IDC_LAME_CBPRESET_en)
	TEXT("preset: insane"), (TCHAR*)0,
	TEXT("preset: fast extreme"), (TCHAR*)1,
	TEXT("preset: extreme"), (TCHAR*)2,
	TEXT("preset: fast standard"), (TCHAR*)3,
	TEXT("preset: standard"), (TCHAR*)4,
	TEXT("preset: fast medium"), (TCHAR*)5,
	TEXT("preset: medium"), (TCHAR*)6,
	TEXT("CBR 320 kbps"), (TCHAR*)7,
	TEXT("CBR 256 kbps"), (TCHAR*)8,
	TEXT("CBR 224 kbps"), (TCHAR*)9,
	TEXT("CBR 192 kbps"), (TCHAR*)10,
	TEXT("CBR 160 kbps"), (TCHAR*)11,
	TEXT("CBR 128 kbps"), (TCHAR*)12,
	TEXT("CBR 112 kbps"), (TCHAR*)13,
	TEXT("CBR 96 kbps"), (TCHAR*)14,
	TEXT("CBR 80 kbps"), (TCHAR*)15,
	TEXT("CBR 64 kbps"), (TCHAR*)16,
	NULL
CB_INFO_TYPE2_END

static const TCHAR **cb_info_lame_IDC_LAME_CBPRESET;

// id のコンボボックスを選択の設定する。
#define CB_SETCURSEL_TYPE2(id) \
{ \
	int cb_num; \
	for(cb_num=0;(int)cb_info_lame_ ## id [cb_num];cb_num+=2){ \
		SendDlgItemMessage(hwnd,id,CB_SETCURSEL,(WPARAM)0,(LPARAM)0); \
	    if(lame_ConfigDialogInfo.opt ## id == (int) cb_info_lame_ ## id [cb_num+1]){ \
			SendDlgItemMessage(hwnd,id,CB_SETCURSEL,(WPARAM)cb_num/2,(LPARAM)0); \
			break; \
		} \
	} \
}
// id のコンボボックスの選択を変数に代入する。
#define CB_GETCURSEL_TYPE2(id) \
{ \
	int cb_num1, cb_num2; \
	cb_num1 = SendDlgItemMessage(hwnd,id,CB_GETCURSEL,(WPARAM)0,(LPARAM)0); \
	for(cb_num2=0;(int)cb_info_lame_ ## id [cb_num2];cb_num2+=2) \
		if(cb_num1*2==cb_num2){ \
			lame_ConfigDialogInfo.opt ## id = (int)cb_info_lame_ ## id [cb_num2+1]; \
			break; \
		} \
}
// コントロールの有効化
#define ENABLE_CONTROL(id) EnableWindow(GetDlgItem(hwnd,id),TRUE);
// コントロールの無効化
#define DISABLE_CONTROL(id) EnableWindow(GetDlgItem(hwnd,id),FALSE);


static void lameConfigDialogProcControlApply(HWND hwnd);
static void lameConfigDialogProcControlReset(HWND hwnd);
static int lame_ConfigDialogInfoLock();
static int lame_ConfigDialogInfoUnLock();
static LRESULT APIENTRY CALLBACK lameConfigDialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess){
	case WM_INITDIALOG:
	{
		int i;
		// コンボボックスの初期化
		cb_info_lame_IDC_LAME_CBPRESET = cb_info_lame_IDC_LAME_CBPRESET_en;

		for(i=0;cb_info_lame_IDC_LAME_CBPRESET[i];i+=2){
			SendDlgItemMessage(hwnd,IDC_LAME_CBPRESET,CB_INSERTSTRING,(WPARAM)-1,(LPARAM)cb_info_lame_IDC_LAME_CBPRESET[i]);
		}
		// 設定
		lameConfigDialogProcControlReset(hwnd);

		SetFocus(GetDlgItem(hwnd, IDOK));
	}
		break;
	case WM_KEYUP:
		if(wParam == VK_ESCAPE) {
			PostMessage(hwnd,WM_COMMAND,MAKEWPARAM(0,0),MAKELPARAM(IDCLOSE,0));
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCLOSE:
			PostMessage(hwnd,WM_CLOSE,(WPARAM)0,(LPARAM)0);
			break;
		case IDOK:
			lameConfigDialogProcControlApply(hwnd);
			lame_ConfigDialogInfoApply();
			PostMessage(hwnd,WM_CLOSE,(WPARAM)0,(LPARAM)0);
			break;
		case IDCANCEL:
			PostMessage(hwnd,WM_CLOSE,(WPARAM)0,(LPARAM)0);
			break;
		case IDC_LAME_CBPRESET:
			break;
		default:
			break;
		}
		break;
	case WM_NOTIFY:
		break;
	case WM_CLOSE:
		lame_ConfigDialogInfoSaveINI();
		EndDialog(hwnd, FALSE);
		break;
	default:
		break;
	}
	return FALSE;
}

static void lameConfigDialogProcControlReset(HWND hwnd)
{
	// コンボボックスの選択設定
	CB_SETCURSEL_TYPE2(IDC_LAME_CBPRESET)
}

static void lameConfigDialogProcControlApply(HWND hwnd)
{
	// コンボボックスの選択設定
	CB_GETCURSEL_TYPE2(IDC_LAME_CBPRESET)
	// リセット
	lameConfigDialogProcControlReset(hwnd);
}

#undef CB_INFO_TYPE2_BEGIN
#undef CB_INFO_TYPE2_END
#undef CB_SETCURSEL_TYPE2
#undef CB_GETCURSEL_TYPE2

#endif

int lameConfigDialog(void)
{
	int changed = 0;
#ifdef AU_LAME
	changed = DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG_LAME), hPrefWnd, (DLGPROC)lameConfigDialogProc);
#endif
	return changed;
}

#ifdef AU_LAME

static int lame_ConfigDialogInfoLock()
{
	return 0;
}
static int lame_ConfigDialogInfoUnLock()
{
	return 0;
}

int lame_ConfigDialogInfoInit(void)
{
	lame_ConfigDialogInfo.optIDC_LAME_CBPRESET = 4;
	return 0;
}

int lame_ConfigDialogInfoApply(void)
{
	lame_ConfigDialogInfoLock();
	lame_encode_preset = lame_ConfigDialogInfo.optIDC_LAME_CBPRESET;
	lame_ConfigDialogInfoUnLock();
	return 0;
}

#define SEC_LAME	"lame"
int lame_ConfigDialogInfoSaveINI(void)
{
	char *section = SEC_LAME;
	char *inifile = timidity_output_inifile;
	char buffer[1024];
//	int len;
#define NUMSAVE(name) \
		sprintf(buffer,"%d",lame_ConfigDialogInfo.name ); \
		WritePrivateProfileString(section, #name ,buffer,inifile);
//#define STRSAVE(name,len) \
//		WritePrivateProfileString(section,(char *) #name ,(char *)lame_ConfigDialogInfo.name ,inifile);
	NUMSAVE(optIDC_LAME_CBPRESET)
	WritePrivateProfileString(NULL,NULL,NULL,inifile);		// Write Flush
#undef NUMSAVE
//#undef STRSAVE
	return 0;
}
int lame_ConfigDialogInfoLoadINI(void)
{
	char *section = SEC_LAME;
	char *inifile = timidity_output_inifile;
	int num;
//	char buffer[1024];
#define NUMLOAD(name) \
		num = GetPrivateProfileInt(section, #name ,-1,inifile); \
		if(num!=-1) lame_ConfigDialogInfo.name = num;
//#define STRLOAD(name,len) \
//		GetPrivateProfileString(section,(char *) #name ,"",buffer,len,inifile); \
//		buffer[len-1] = '\0'; \
//		if(buffer[0]!=0) \
//			strcpy((char *)lame_ConfigDialogInfo.name ,buffer);
	lame_ConfigDialogInfoLock();
	NUMLOAD(optIDC_LAME_CBPRESET)
#undef NUMLOAD
//#undef STRLOAD
	lame_encode_preset = lame_ConfigDialogInfo.optIDC_LAME_CBPRESET;
	lame_ConfigDialogInfoUnLock();
	return 0;
}

#endif	// AU_LAME


#ifdef AU_FLAC
//
//
// libFLAC ConfigDialog
//
//

volatile flac_ConfigDialogInfo_t flac_ConfigDialogInfo;

// id のコンボボックスの情報の定義
#define CB_INFO_TYPE2_BEGIN(id) static const TCHAR * cb_info_flac_ ## id [] = {
#define CB_INFO_TYPE2_END };

// cb_info_type1_ＩＤ  cb_info_type2_ＩＤ というふうになる。

// IDC_COMBO_ENCODE_MODE_jp
CB_INFO_TYPE2_BEGIN(IDC_COMBO_ENCODE_MODE_jp)
	TEXT("デフォルト"), (TCHAR*)5,
	TEXT("レベル 0 (低)"), (TCHAR*)0,
	TEXT("レベル 1"), (TCHAR*)1,
	TEXT("レベル 2"), (TCHAR*)2,
	TEXT("レベル 3"), (TCHAR*)3,
	TEXT("レベル 4"), (TCHAR*)4,
	TEXT("レベル 5 (デフォルト)"), (TCHAR*)5,
	TEXT("レベル 6"), (TCHAR*)6,
	TEXT("レベル 7"), (TCHAR*)7,
	TEXT("レベル 8 (高)"), (TCHAR*)8,
	NULL
CB_INFO_TYPE2_END

// IDC_COMBO_ENCODE_MODE_en
CB_INFO_TYPE2_BEGIN(IDC_COMBO_ENCODE_MODE_en)
	TEXT("Default"), (TCHAR*)5,
	TEXT("Level 0 (low)"), (TCHAR*)0,
	TEXT("Level 1"), (TCHAR*)1,
	TEXT("Level 2"), (TCHAR*)2,
	TEXT("Level 3"), (TCHAR*)3,
	TEXT("Level 4"), (TCHAR*)4,
	TEXT("Level 5 (default)"), (TCHAR*)5,
	TEXT("Level 6"), (TCHAR*)6,
	TEXT("Level 7"), (TCHAR*)7,
	TEXT("Level 8 (high)"), (TCHAR*)8,
	NULL
CB_INFO_TYPE2_END

static const TCHAR **cb_info_flac_IDC_COMBO_ENCODE_MODE;

// id のコンボボックスを選択の設定する。
#define CB_SETCURSEL_TYPE2(id) \
{ \
	int cb_num; \
	for(cb_num=0;(int)cb_info_flac_ ## id [cb_num];cb_num+=2){ \
		SendDlgItemMessage(hwnd,id,CB_SETCURSEL,(WPARAM)0,(LPARAM)0); \
	    if(flac_ConfigDialogInfo.opt ## id == (int) cb_info_flac_ ## id [cb_num+1]){ \
			SendDlgItemMessage(hwnd,id,CB_SETCURSEL,(WPARAM)cb_num/2,(LPARAM)0); \
			break; \
		} \
	} \
}
// id のコンボボックスの選択を変数に代入する。
#define CB_GETCURSEL_TYPE2(id) \
{ \
	int cb_num1, cb_num2; \
	cb_num1 = SendDlgItemMessage(hwnd,id,CB_GETCURSEL,(WPARAM)0,(LPARAM)0); \
	for(cb_num2=0;(int)cb_info_flac_ ## id [cb_num2];cb_num2+=2) \
		if(cb_num1*2==cb_num2){ \
			flac_ConfigDialogInfo.opt ## id = (int)cb_info_flac_ ## id [cb_num2+1]; \
			break; \
		} \
}
// チェックされているか。
#define IS_CHECK(id) SendDlgItemMessage(hwnd,id,BM_GETCHECK,0,0)
// チェックする。
#define CHECK(id) SendDlgItemMessage(hwnd,id,BM_SETCHECK,1,0)
// チェックをはずす。
#define UNCHECK(id) SendDlgItemMessage(hwnd,id,BM_SETCHECK,0,0)
// id のチェックボックスを設定する。
#define CHECKBOX_SET(id) \
	if(flac_ConfigDialogInfo.opt ## id>0) \
		SendDlgItemMessage(hwnd,id,BM_SETCHECK,1,0); \
	else \
		SendDlgItemMessage(hwnd,id,BM_SETCHECK,0,0); \
// id のチェックボックスを変数に代入する。
#define CHECKBOX_GET(id) \
	if(SendDlgItemMessage(hwnd,id,BM_GETCHECK,0,0)) \
		flac_ConfigDialogInfo.opt ## id = 1; \
	else \
		flac_ConfigDialogInfo.opt ## id = 0; \
// コントロールの有効化
#define ENABLE_CONTROL(id) EnableWindow(GetDlgItem(hwnd,id),TRUE);
// コントロールの無効化
#define DISABLE_CONTROL(id) EnableWindow(GetDlgItem(hwnd,id),FALSE);

static void flacConfigDialogProcControlEnableDisable(HWND hwnd);
static void flacConfigDialogProcControlApply(HWND hwnd);
static void flacConfigDialogProcControlReset(HWND hwnd);
static int flac_ConfigDialogInfoLock();
static int flac_ConfigDialogInfoUnLock();
static LRESULT APIENTRY CALLBACK flacConfigDialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess){
	case WM_INITDIALOG:
	{
		int i;
		// コンボボックスの初期化
		if (CurrentPlayerLanguage == LANGUAGE_JAPANESE)
		  cb_info_flac_IDC_COMBO_ENCODE_MODE = cb_info_flac_IDC_COMBO_ENCODE_MODE_jp;
		else
		  cb_info_flac_IDC_COMBO_ENCODE_MODE = cb_info_flac_IDC_COMBO_ENCODE_MODE_en;

		for(i=0;cb_info_flac_IDC_COMBO_ENCODE_MODE[i];i+=2){
			SendDlgItemMessage(hwnd,IDC_COMBO_ENCODE_MODE,CB_INSERTSTRING,(WPARAM)-1,(LPARAM)cb_info_flac_IDC_COMBO_ENCODE_MODE[i]);
		}
		// 設定
		flacConfigDialogProcControlReset(hwnd);

		SetFocus(GetDlgItem(hwnd, IDOK));
	}
		break;
	case WM_KEYUP:
		if(wParam == VK_ESCAPE) {
			PostMessage(hwnd,WM_COMMAND,MAKEWPARAM(0,0),MAKELPARAM(IDCLOSE,0));
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCLOSE:
			PostMessage(hwnd,WM_CLOSE,(WPARAM)0,(LPARAM)0);
			break;
		case IDOK:
			flacConfigDialogProcControlApply(hwnd);
			flac_ConfigDialogInfoApply();
			PostMessage(hwnd,WM_CLOSE,(WPARAM)0,(LPARAM)0);
			break;
		case IDCANCEL:
			PostMessage(hwnd,WM_CLOSE,(WPARAM)0,(LPARAM)0);
			break;
		case IDC_COMBO_ENCODE_MODE:
			break;
		case IDC_CHECKBOX_OGGFLAC_CONTAINER:
			break;
		default:
			break;
		}
		break;
	case WM_NOTIFY:
		break;
	case WM_CLOSE:
		flac_ConfigDialogInfoSaveINI();
		EndDialog(hwnd, FALSE);
		break;
	default:
		break;
	}
	return FALSE;
}

// コントロールの有効 / 無効化
static void flacConfigDialogProcControlEnableDisable(HWND hwnd)
{
#if !defined(AU_OGGFLAC)
	DISABLE_CONTROL(IDC_CHECKBOX_OGGFLAC_CONTAINER);
#endif
}

static void flacConfigDialogProcControlReset(HWND hwnd)
{
	// コンボボックスの選択設定
	CB_SETCURSEL_TYPE2(IDC_COMBO_ENCODE_MODE)
	// チェックボックスの設定
	CHECKBOX_SET(IDC_CHECKBOX_OGGFLAC_CONTAINER)
	// コントロールの有効 / 無効化
	flacConfigDialogProcControlEnableDisable(hwnd);
}

static void flacConfigDialogProcControlApply(HWND hwnd)
{
	// コンボボックスの選択設定
	CB_GETCURSEL_TYPE2(IDC_COMBO_ENCODE_MODE)
	// チェックボックスの設定
	CHECKBOX_GET(IDC_CHECKBOX_OGGFLAC_CONTAINER)
	// リセット
	flacConfigDialogProcControlReset(hwnd);
}

#undef CB_INFO_TYPE2_BEGIN
#undef CB_INFO_TYPE2_END
#undef CB_SETCURSEL_TYPE2
#undef CB_GETCURSEL_TYPE2
#undef CHECKBOX_SET
#undef CHECKBOX_GET

#endif

int flacConfigDialog(void)
{
	int changed = 0;
#ifdef AU_FLAC
	if (CurrentPlayerLanguage == LANGUAGE_JAPANESE)
		changed = DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG_FLAC), hPrefWnd, (DLGPROC)flacConfigDialogProc);
	else
		changed = DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG_FLAC_EN), hPrefWnd, (DLGPROC)flacConfigDialogProc);
#endif
	return changed;
}

#ifdef AU_FLAC

static int flac_ConfigDialogInfoLock()
{
	return 0;
}
static int flac_ConfigDialogInfoUnLock()
{
	return 0;
}

int flac_ConfigDialogInfoInit(void)
{
	flac_ConfigDialogInfo.optIDC_COMBO_ENCODE_MODE = 5;
	flac_ConfigDialogInfo.optIDC_CHECKBOX_OGGFLAC_CONTAINER = 0;
	return 0;
}

int flac_ConfigDialogInfoApply(void)
{
	flac_ConfigDialogInfoLock();
	flac_set_compression_level(flac_ConfigDialogInfo.optIDC_COMBO_ENCODE_MODE);
#if defined(AU_OGGFLAC)
	flac_set_option_oggflac(flac_ConfigDialogInfo.optIDC_CHECKBOX_OGGFLAC_CONTAINER);
#endif
	flac_ConfigDialogInfoUnLock();
	return 0;
}

#define SEC_FLAC	"FLAC"
int flac_ConfigDialogInfoSaveINI(void)
{
	char *section = SEC_FLAC;
	char *inifile = timidity_output_inifile;
	char buffer[1024];
//	int len;
#define NUMSAVE(name) \
		sprintf(buffer,"%d",flac_ConfigDialogInfo.name ); \
		WritePrivateProfileString(section, #name ,buffer,inifile);
//#define STRSAVE(name,len) \
//		WritePrivateProfileString(section,(char *) #name ,(char *)flac_ConfigDialogInfo.name ,inifile);
	NUMSAVE(optIDC_COMBO_ENCODE_MODE)
	NUMSAVE(optIDC_CHECKBOX_OGGFLAC_CONTAINER)
	WritePrivateProfileString(NULL,NULL,NULL,inifile);		// Write Flush
#undef NUMSAVE
//#undef STRSAVE
	return 0;
}
int flac_ConfigDialogInfoLoadINI(void)
{
	char *section = SEC_FLAC;
	char *inifile = timidity_output_inifile;
	int num;
//	char buffer[1024];
#define NUMLOAD(name) \
		num = GetPrivateProfileInt(section, #name ,-1,inifile); \
		if(num!=-1) flac_ConfigDialogInfo.name = num;
//#define STRLOAD(name,len) \
//		GetPrivateProfileString(section,(char *) #name ,"",buffer,len,inifile); \
//		buffer[len-1] = '\0'; \
//		if(buffer[0]!=0) \
//			strcpy((char *)flac_ConfigDialogInfo.name ,buffer);
	flac_ConfigDialogInfoLock();
	NUMLOAD(optIDC_COMBO_ENCODE_MODE)
	NUMLOAD(optIDC_CHECKBOX_OGGFLAC_CONTAINER)
#undef NUMLOAD
//#undef STRLOAD
	flac_set_compression_level(flac_ConfigDialogInfo.optIDC_COMBO_ENCODE_MODE);
	flac_ConfigDialogInfoUnLock();
	return 0;
}

#endif	// AU_FLAC


#ifdef AU_OPUS
// Opus

int opus_ConfigDialogInfoInit(void)
{
	return 0;
}

int opus_ConfigDialogInfoSaveINI(void)
{
	return 0;
}

int opus_ConfigDialogInfoLoadINI(void)
{
	return 0;
}

#endif

#ifdef AU_SPEEX
// Speex

int speex_ConfigDialogInfoInit(void)
{
	return 0;
}

int speex_ConfigDialogInfoSaveINI(void)
{
	return 0;
}

int speex_ConfigDialogInfoLoadINI(void)
{
	return 0;
}

#endif

#ifdef AU_PORTAUDIO_DLL
///////////////////////////////////////////////////////////////////////
//
// asioConfigDialog
//
///////////////////////////////////////////////////////////////////////
#include <portaudio.h>
#include <pa_asio.h>
#include "w32_portaudio.h"

///r
PA_DEVICELIST cb_info_IDC_COMBO_PA_ASIO_NAME[PA_DEVLIST_MAX];
PA_DEVICELIST cb_info_IDC_COMBO_PA_DS_NAME[PA_DEVLIST_MAX];
PA_DEVICELIST cb_info_IDC_COMBO_PA_WMME_NAME[PA_DEVLIST_MAX];
PA_DEVICELIST cb_info_IDC_COMBO_PA_WDMKS_NAME[PA_DEVLIST_MAX];
PA_DEVICELIST cb_info_IDC_COMBO_PA_WASAPI_NAME[PA_DEVLIST_MAX];

#define cb_num_IDC_COMBO_PA_WASAPI_PRIORITY 8
static const TCHAR *cb_info_IDC_COMBO_PA_WASAPI_PRIORITY[] = {
    TEXT("None"),
    TEXT("Audio (Shared Mode)"),
    TEXT("Capture"),
    TEXT("Distribution"),
    TEXT("Games"),
    TEXT("Playback"),
    TEXT("ProAudio (Exclusive Mode)"),
    TEXT("WindowManager"),
};

#define cb_num_IDC_COMBO_PA_WASAPI_STREAM_CATEGORY 12
static const TCHAR *cb_info_IDC_COMBO_PA_WASAPI_STREAM_CATEGORY[] = {
    TEXT("Other"),
    TEXT("None"),
    TEXT("None"),
    TEXT("Communications"),
    TEXT("Alerts"),
    TEXT("SoundEffects"),
    TEXT("GameEffects"),
    TEXT("GameMedia"),
    TEXT("GameChat"),
    TEXT("Speech"),
    TEXT("Movie"),
    TEXT("Media"),
};


LRESULT WINAPI portaudioConfigDialogProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	int i = 0, cb_num = 0, cb_sel = 0, flag;

	switch (msg) {
		case WM_INITDIALOG:
		{
			int max, winver = get_winver();
			// WASAPI Options
			if (st_temp->pa_wasapi_flag & paWinWasapiExclusive)
				SendDlgItemMessage(hwnd, IDC_CHECKBOX_PA_WASAPI_EXCLUSIVE, BM_SETCHECK, 1, 0);
			else
				SendDlgItemMessage(hwnd, IDC_CHECKBOX_PA_WASAPI_EXCLUSIVE, BM_SETCHECK, 0, 0);
			if (st_temp->pa_wasapi_flag & paWinWasapiRedirectHostProcessor)
				SendDlgItemMessage(hwnd, IDC_CHECKBOX_PA_WASAPI_REDIRECT, BM_SETCHECK, 1, 0);
			else
				SendDlgItemMessage(hwnd, IDC_CHECKBOX_PA_WASAPI_REDIRECT, BM_SETCHECK, 0, 0);
			if (st_temp->pa_wasapi_flag & paWinWasapiUseChannelMask)
				SendDlgItemMessage(hwnd, IDC_CHECKBOX_PA_WASAPI_CH_MASK, BM_SETCHECK, 1, 0);
			else
				SendDlgItemMessage(hwnd, IDC_CHECKBOX_PA_WASAPI_CH_MASK, BM_SETCHECK, 0, 0);
			if (st_temp->pa_wasapi_flag & paWinWasapiPolling)
				SendDlgItemMessage(hwnd, IDC_CHECKBOX_PA_WASAPI_POLLING, BM_SETCHECK, 1, 0);
			else
				SendDlgItemMessage(hwnd, IDC_CHECKBOX_PA_WASAPI_POLLING, BM_SETCHECK, 0, 0);
			for (i = 0; i < cb_num_IDC_COMBO_PA_WASAPI_PRIORITY; i++)
				CB_INSSTR(IDC_COMBO_PA_WASAPI_PRIORITY, cb_info_IDC_COMBO_PA_WASAPI_PRIORITY[i]);
			CB_SET(IDC_COMBO_PA_WASAPI_PRIORITY, (st_temp->pa_wasapi_flag >> 4));

			DI_DISABLE(IDC_CHECKBOX_PA_WASAPI_CH_MASK); // 作ってないのでOFF
			// WASAPI StreamCategory
			max = winver >= 3 ? cb_num_IDC_COMBO_PA_WASAPI_STREAM_CATEGORY : 1;
			for (i = 0; i < max; i++)
				CB_INSSTR(IDC_COMBO_PA_WASAPI_STREAM_CATEGORY, cb_info_IDC_COMBO_PA_WASAPI_STREAM_CATEGORY[i]);
			if(winver >= 3) { // win8
				CB_SET(IDC_COMBO_PA_WASAPI_STREAM_CATEGORY, (st_temp->pa_wasapi_stream_category));
			}else{
				CB_SET(IDC_COMBO_PA_WASAPI_STREAM_CATEGORY, 0);
				DI_DISABLE(IDC_COMBO_PA_WASAPI_STREAM_CATEGORY);
			}
			// WASAPI StreamOption
			if (winver >= 6) { // win10
				SendDlgItemMessage(hwnd, IDC_CHECKBOX_PA_WASAPI_STREAM_OPTIONS_RAW, BM_SETCHECK, (st_temp->pa_wasapi_stream_option & 1) ? BST_CHECKED : BST_UNCHECKED, 0);
				SendDlgItemMessage(hwnd, IDC_CHECKBOX_PA_WASAPI_STREAM_OPTIONS_MATCH_FORMAT, BM_SETCHECK, (st_temp->pa_wasapi_stream_option & 2) ? BST_CHECKED : BST_UNCHECKED, 0);
				SendDlgItemMessage(hwnd, IDC_CHECKBOX_PA_WASAPI_STREAM_OPTIONS_AMBISONICS, BM_SETCHECK, (st_temp->pa_wasapi_stream_option & 4) ? BST_CHECKED : BST_UNCHECKED, 0);
			}
			else if (winver >= 4) { // win8.1
				SendDlgItemMessage(hwnd, IDC_CHECKBOX_PA_WASAPI_STREAM_OPTIONS_RAW, BM_SETCHECK, (st_temp->pa_wasapi_stream_option & 1) ? BST_CHECKED : BST_UNCHECKED, 0);
				DI_DISABLE(IDC_CHECKBOX_PA_WASAPI_STREAM_OPTIONS_MATCH_FORMAT);
				DI_DISABLE(IDC_CHECKBOX_PA_WASAPI_STREAM_OPTIONS_AMBISONICS);
			}
			else {
				DI_DISABLE(IDC_CHECKBOX_PA_WASAPI_STREAM_OPTIONS_RAW);
				DI_DISABLE(IDC_CHECKBOX_PA_WASAPI_STREAM_OPTIONS_MATCH_FORMAT);
				DI_DISABLE(IDC_CHECKBOX_PA_WASAPI_STREAM_OPTIONS_AMBISONICS);
			}


			// asio
			cb_num = pa_device_list(cb_info_IDC_COMBO_PA_ASIO_NAME, paASIO);
			if (cb_num <= -1) {
				// Unsupported
				DI_DISABLE(IDC_COMBO_PA_ASIO_DEV);
				DI_DISABLE(IDC_BUTTON_PA_ASIO_CONFIG);
				DI_DISABLE(IDC_COMBO_PA_DS_DEV);
				DI_DISABLE(IDC_COMBO_PA_WMME_DEV);
				DI_DISABLE(IDC_COMBO_PA_WDMKS_DEV);
				DI_DISABLE(IDC_COMBO_PA_WASAPI_DEV);
			}
			if (cb_num == 0) {
				DI_DISABLE(IDC_COMBO_PA_ASIO_DEV);
				DI_DISABLE(IDC_BUTTON_PA_ASIO_CONFIG);
			}
			for (i = 0; i < cb_num && i < 100; i++) {
				CB_INSSTRA(IDC_COMBO_PA_ASIO_DEV, &cb_info_IDC_COMBO_PA_ASIO_NAME[i].name);
				if (st_temp->pa_asio_device_id == cb_info_IDC_COMBO_PA_ASIO_NAME[i].deviceID)
					cb_sel = i;
			}
			CB_SET(IDC_COMBO_PA_ASIO_DEV, (cb_sel));
			// ds
			cb_num = pa_device_list(cb_info_IDC_COMBO_PA_DS_NAME, paDirectSound);
			if (cb_num == 0)
				DI_DISABLE(IDC_COMBO_PA_DS_DEV);
			for (i = 0; i < cb_num && i < 100; i++) {
				CB_INSSTRA(IDC_COMBO_PA_DS_DEV, &cb_info_IDC_COMBO_PA_DS_NAME[i].name);
				if (st_temp->pa_ds_device_id == cb_info_IDC_COMBO_PA_DS_NAME[i].deviceID)
					cb_sel = i;
			}
			CB_SET(IDC_COMBO_PA_DS_DEV, (cb_sel));
			// WMME
			cb_num = pa_device_list(cb_info_IDC_COMBO_PA_WMME_NAME, paMME);
			if (cb_num == 0)
				DI_DISABLE(IDC_COMBO_PA_WMME_DEV);
			for (i = 0; i < cb_num && i < 100; i++) {
				CB_INSSTRA(IDC_COMBO_PA_WMME_DEV, &cb_info_IDC_COMBO_PA_WMME_NAME[i].name);
				if (st_temp->pa_wmme_device_id == cb_info_IDC_COMBO_PA_WMME_NAME[i].deviceID)
					cb_sel = i;
			}
			CB_SET(IDC_COMBO_PA_WMME_DEV, (cb_sel));
			// WDMKS
			cb_num = pa_device_list(cb_info_IDC_COMBO_PA_WDMKS_NAME, paWDMKS);
			if (cb_num == 0)
				DI_DISABLE(IDC_COMBO_PA_WDMKS_DEV);
			for (i = 0; i < cb_num && i < 100; i++) {
				CB_INSSTRA(IDC_COMBO_PA_WDMKS_DEV, &cb_info_IDC_COMBO_PA_WDMKS_NAME[i].name);
				if (st_temp->pa_wdmks_device_id == cb_info_IDC_COMBO_PA_WDMKS_NAME[i].deviceID)
					cb_sel = i;
			}
			CB_SET(IDC_COMBO_PA_WDMKS_DEV, (cb_sel));
			// WASAPI
			cb_num = pa_device_list(cb_info_IDC_COMBO_PA_WASAPI_NAME, paWASAPI);
			if (cb_num == 0)
				DI_DISABLE(IDC_COMBO_PA_WASAPI_DEV);
			for (i = 0; i < cb_num && i < 100; i++) {
				CB_INSSTRA(IDC_COMBO_PA_WASAPI_DEV, &cb_info_IDC_COMBO_PA_WASAPI_NAME[i].name);
				if (st_temp->pa_wasapi_device_id == cb_info_IDC_COMBO_PA_WASAPI_NAME[i].deviceID)
					cb_sel = i;
			}
			CB_SET(IDC_COMBO_PA_WASAPI_DEV, (cb_sel));

			// error check
			cb_num = pa_device_list(cb_info_IDC_COMBO_PA_ASIO_NAME, paASIO);
			if (cb_num == -2) {	
#ifdef _WIN64
				const TCHAR *pa_msg,
					pa_msg_en[] = TEXT("Cannot load portaudio_x64.dll"),
					pa_msg_jp[] = TEXT("portaudio_x64.dll をロードしていません。");
#else
				const TCHAR *pa_msg,
					pa_msg_en[] = TEXT("Cannot load portaudio_x86.dll"),
					pa_msg_jp[] = TEXT("portaudio_x86.dll をロードしていません。");
#endif
				if (CurrentPlayerLanguage == LANGUAGE_JAPANESE)
					pa_msg = pa_msg_jp;
				else
					pa_msg = pa_msg_en;
				MessageBox(hwnd, pa_msg, TEXT("TiMidity Warning"), MB_OK | MB_ICONWARNING);
			}
			else if (cb_num == -3) {
				const TCHAR *pa_msg,
					pa_msg_en[] = TEXT("Couldn't close device"),
					pa_msg_jp[] = TEXT("出力デバイスを閉じれません。");
				if (CurrentPlayerLanguage == LANGUAGE_JAPANESE)
					pa_msg = pa_msg_jp;
				else
					pa_msg = pa_msg_en;
				MessageBox(hwnd, pa_msg, TEXT("TiMidity Warning"), MB_OK | MB_ICONWARNING);
			}
			else if (cb_num <= -1) {
#ifdef _WIN64
				const TCHAR *pa_msg,
					pa_msg_en[] = TEXT("Failed initialize portaudio_x64.dll"),
					pa_msg_jp[] = TEXT("portaudio_x86.dll を使用できません。");
#else
				const TCHAR *pa_msg,
					pa_msg_en[] = TEXT("Failed initialize portaudio_x86.dll"),
					pa_msg_jp[] = TEXT("portaudio_x86.dll を使用できません。");
#endif
				if (CurrentPlayerLanguage == LANGUAGE_JAPANESE)
					pa_msg = pa_msg_jp;
				else
					pa_msg = pa_msg_en;
				MessageBox(hwnd, pa_msg, TEXT("TiMidity Warning"), MB_OK | MB_ICONWARNING);
			}

			SetFocus(DI_GET(IDOK));
			return TRUE;
		}
		case WM_CLOSE:
			EndDialog(hwnd,FALSE);
			break;
		case WM_COMMAND:
			switch (LOWORD(wp)) {
			case IDC_BUTTON_PA_ASIO_CONFIG:
				cb_sel = SendDlgItemMessage(hwnd, IDC_COMBO_PA_ASIO_DEV, CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
				asioConfigDialog(cb_info_IDC_COMBO_PA_ASIO_NAME[cb_sel].deviceID);
				break;
			case IDCANCEL:
				PostMessage(hwnd,WM_CLOSE,(WPARAM)0,(LPARAM)0);
				break;
			case IDOK:
				// asio
				if(IsWindowEnabled(GetDlgItem(hwnd, IDC_COMBO_PA_ASIO_DEV))) {
					cb_sel = SendDlgItemMessage(hwnd, IDC_COMBO_PA_ASIO_DEV, CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
					st_temp->pa_asio_device_id = cb_info_IDC_COMBO_PA_ASIO_NAME[cb_sel].deviceID;
				}
				// ds
				if(IsWindowEnabled(GetDlgItem(hwnd, IDC_COMBO_PA_DS_DEV))) {
					cb_sel = SendDlgItemMessage(hwnd, IDC_COMBO_PA_DS_DEV, CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
					st_temp->pa_ds_device_id = cb_info_IDC_COMBO_PA_DS_NAME[cb_sel].deviceID;
				}
				// WMME
				if(IsWindowEnabled(GetDlgItem(hwnd, IDC_COMBO_PA_WMME_DEV))) {
					cb_sel = SendDlgItemMessage(hwnd, IDC_COMBO_PA_WMME_DEV, CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
					st_temp->pa_wmme_device_id = cb_info_IDC_COMBO_PA_WMME_NAME[cb_sel].deviceID;
				}
				// WDMKS
				if(IsWindowEnabled(GetDlgItem(hwnd, IDC_COMBO_PA_WDMKS_DEV))) {
					cb_sel = SendDlgItemMessage(hwnd, IDC_COMBO_PA_WDMKS_DEV, CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
					st_temp->pa_wdmks_device_id = cb_info_IDC_COMBO_PA_WDMKS_NAME[cb_sel].deviceID;
				}
				// WASAPI
				if(IsWindowEnabled(GetDlgItem(hwnd, IDC_COMBO_PA_WASAPI_DEV))) {
					cb_sel = SendDlgItemMessage(hwnd, IDC_COMBO_PA_WASAPI_DEV, CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
					st_temp->pa_wasapi_device_id = cb_info_IDC_COMBO_PA_WASAPI_NAME[cb_sel].deviceID;
				}
				// WASAPI Flag
				flag = 0;
				if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_PA_WASAPI_EXCLUSIVE,BM_GETCHECK,0,0))
					flag |= paWinWasapiExclusive;
				if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_PA_WASAPI_REDIRECT,BM_GETCHECK,0,0))
					flag |= paWinWasapiRedirectHostProcessor;
				if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_PA_WASAPI_CH_MASK,BM_GETCHECK,0,0))
					flag |= paWinWasapiUseChannelMask;
				if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_PA_WASAPI_POLLING,BM_GETCHECK,0,0))
					flag |= paWinWasapiPolling;
				cb_sel = SendDlgItemMessage(hwnd, IDC_COMBO_PA_WASAPI_PRIORITY, CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
				flag |= cb_sel << 4;
				st_temp->pa_wasapi_flag = flag;
				// WASAPI StreamCategory
				st_temp->pa_wasapi_stream_category = CB_GET(IDC_COMBO_PA_WASAPI_STREAM_CATEGORY);
				// WASAPI StreamOption
				st_temp->pa_wasapi_stream_option = 0;
				if (SendDlgItemMessage(hwnd, IDC_CHECKBOX_PA_WASAPI_STREAM_OPTIONS_RAW, BM_GETCHECK, 0, 0))
					st_temp->pa_wasapi_stream_option |= 1;
				if (SendDlgItemMessage(hwnd, IDC_CHECKBOX_PA_WASAPI_STREAM_OPTIONS_MATCH_FORMAT, BM_GETCHECK, 0, 0))
					st_temp->pa_wasapi_stream_option |= 2;
				if (SendDlgItemMessage(hwnd, IDC_CHECKBOX_PA_WASAPI_STREAM_OPTIONS_AMBISONICS, BM_GETCHECK, 0, 0))
					st_temp->pa_wasapi_stream_option |= 4;

				EndDialog(hwnd,TRUE);
				break;
			}
			break;
	}
	return FALSE;
}

int portaudioConfigDialog(void)
{
	int changed = 0;
	changed = DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_PORTAUDIO), hPrefWnd, (DLGPROC)portaudioConfigDialogProc);
	return changed;
}
///r
int asioConfigDialog(int deviceID)
{
	extern HWND hMainWnd;

	PaHostApiTypeId HostApiTypeId;
	const PaHostApiInfo  *HostApiInfo;
	PaDeviceIndex DeviceIndex;
	PaError err;
	HWND hWnd;
	int buffered_data = 0;

	PaHostApiIndex i, ApiCount;
	
	
#ifdef AU_PORTAUDIO_DLL
#ifdef PORTAUDIO_V19
	if(load_portaudio_dll(0))
		return -1;
#else
	if(load_portaudio_dll(PA_DLL_ASIO))
		return -1;
#endif
#endif
	
#ifdef AU_PORTAUDIO_DLL
	if(play_mode == portaudio_play_mode
#ifdef PORTAUDIO_V19
	    || play_mode == &portaudio_win_wasapi_play_mode
	    || play_mode == &portaudio_win_wdmks_play_mode
#endif
	    || play_mode == &portaudio_win_ds_play_mode
	    || play_mode == &portaudio_win_wmme_play_mode
	    || play_mode == &portaudio_asio_play_mode)
	{
		play_mode->acntl(PM_REQ_GETFILLED, &buffered_data);
		if (buffered_data != 0) return -1;
		play_mode->close_output();
	}
#else
	if(play_mode == &portaudio_play_mode) {
		play_mode->acntl(PM_REQ_GETFILLED, &buffered_data);
		if (buffered_data != 0) return -1;
		play_mode->close_output();
	}
#endif
	err = Pa_Initialize();
	if( err != paNoError ) goto error1;


	HostApiTypeId = paASIO;
	i = 0;
	hWnd = hPrefWnd;
	ApiCount = Pa_GetHostApiCount();
	do{
		HostApiInfo=Pa_GetHostApiInfo(i);
		if( HostApiInfo->type == HostApiTypeId ) break;
	    i++;
	}while ( i < ApiCount );
	if ( i == ApiCount ) goto error2;

	if(deviceID <0)
		DeviceIndex = HostApiInfo->defaultOutputDevice;
	else
		DeviceIndex = deviceID;

	if(DeviceIndex==paNoDevice) goto error2;

	if (HostApiTypeId ==  paASIO){
    	err = PaAsio_ShowControlPanel( DeviceIndex, (void*) hWnd);
		if( err != paNoError ) goto error1;
	}
	Pa_Terminate();
	play_mode->open_output();
//  	free_portaudio_dll();
	return 0;
	
error1:
//  	free_portaudio_dll();
	MessageBox(NULL, Pa_GetErrorText( err ), "Port Audio (asio) error", MB_OK | MB_ICONEXCLAMATION);
error2:
	Pa_Terminate();
	return -1;
}

#endif //AU_PORTAUDIO_DLL



///r
#ifdef AU_W32

DEVICELIST cb_info_IDC_COMBO_WMME_NAME[DEVLIST_MAX];

LRESULT WINAPI wmmeConfigDialogProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	int i = 0, cb_num = 0;

	switch (msg) {
		case WM_INITDIALOG:
			// WMME_DEV
			cb_num = wmme_device_list(cb_info_IDC_COMBO_WMME_NAME);
			for (i = 0; i < (cb_num + 1); i++){
				SendDlgItemMessage(hwnd, IDC_COMBO_WMME_DEV, CB_INSERTSTRING, (WPARAM) -1, (LPARAM) &cb_info_IDC_COMBO_WMME_NAME[i].name);
			}
			SendDlgItemMessage(hwnd, IDC_COMBO_WMME_DEV, CB_SETCURSEL, (WPARAM) (st_temp->wmme_device_id + 1), (LPARAM) 0);

			// WAVEFORMATEX
			if(st_temp->wave_format_ext == 0){
				CheckRadioButton(hwnd,IDC_RADIOBUTTON_WAVE_FORMAT_EX,IDC_RADIOBUTTON_WAVE_FORMAT_EXT,IDC_RADIOBUTTON_WAVE_FORMAT_EX);
			}else{
				CheckRadioButton(hwnd,IDC_RADIOBUTTON_WAVE_FORMAT_EX,IDC_RADIOBUTTON_WAVE_FORMAT_EXT,IDC_RADIOBUTTON_WAVE_FORMAT_EXT);
			}
			// WMME_BUFFER (data_block
			SetDlgItemInt(hwnd, IDC_EDIT_WMME_BUFFER_BIT, st_temp->wmme_buffer_bits, FALSE);
			SetDlgItemInt(hwnd, IDC_EDIT_WMME_BUFFER_NUM, st_temp->wmme_buffer_num, FALSE);
			EnableWindow(GetDlgItem(hwnd, IDC_EDIT_WMME_BUFFER_BIT), FALSE); // バッファ設定共有なので 不使用
			EnableWindow(GetDlgItem(hwnd, IDC_EDIT_WMME_BUFFER_NUM), FALSE); // バッファ設定共有なので 不使用
			return TRUE;
		case WM_CLOSE:
			EndDialog(hwnd,TRUE);
			break;
		case WM_COMMAND:
			switch (LOWORD(wp)) {
			case IDCANCEL:
				PostMessage(hwnd,WM_CLOSE,(WPARAM)0,(LPARAM)0);
				break;
			case IDOK:
				// WMME_DEV
				st_temp->wmme_device_id = SendDlgItemMessage(hwnd, IDC_COMBO_WMME_DEV, CB_GETCURSEL, (WPARAM)0, (LPARAM)0) - 1;
				// WAVEFORMATEX
				if(SendDlgItemMessage(hwnd,IDC_RADIOBUTTON_WAVE_FORMAT_EX,BM_GETCHECK,0,0))
					st_temp->wave_format_ext = 0;
				else
					st_temp->wave_format_ext = 1;
				// WMME_BUFFER (data_block
				st_temp->wmme_buffer_bits = GetDlgItemInt(hwnd, IDC_EDIT_WMME_BUFFER_BIT, NULL, FALSE);
				st_temp->wmme_buffer_num = GetDlgItemInt(hwnd, IDC_EDIT_WMME_BUFFER_NUM, NULL, FALSE);
				PostMessage(hwnd,WM_CLOSE,(WPARAM)0,(LPARAM)0);
				break;
			}
			break;

	}
	return FALSE;
}

void wmmeConfigDialog(HWND hwnd)
{
	DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_WMME), hwnd, (DLGPROC)wmmeConfigDialogProc);
}
#endif



#ifdef AU_WASAPI
///////////////////////////////////////////////////////////////////////
//
// WASAPI Config Dialog
//
///////////////////////////////////////////////////////////////////////

WASAPI_DEVICELIST cb_info_IDC_COMBO_WASAPI_NAME[WASAPI_DEVLIST_MAX];

#define cb_num_IDC_COMBO_WASAPI_PRIORITY 8
static const TCHAR *cb_info_IDC_COMBO_WASAPI_PRIORITY[] = {
    TEXT("Auto"),
    TEXT("Audio (Shared Mode)"),
    TEXT("Capture"),
    TEXT("Distribution"),
    TEXT("Games"),
    TEXT("Playback"),
    TEXT("ProAudio (Exclusive Mode)"),
    TEXT("WindowManager"),
};

#define cb_num_IDC_COMBO_WASAPI_STREAM_CATEGORY 12
static const TCHAR *cb_info_IDC_COMBO_WASAPI_STREAM_CATEGORY[] = {
    TEXT("Other"),
    TEXT("None"),
    TEXT("None"),
    TEXT("Communications"),
    TEXT("Alerts"),
    TEXT("SoundEffects"),
    TEXT("GameEffects"),
    TEXT("GameMedia"),
    TEXT("GameChat"),
    TEXT("Speech"),
    TEXT("Movie"),
    TEXT("Media"),
};

LRESULT WINAPI wasapiConfigDialogProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	int i = 0, cb_num = 0, cb_sel = 0, flag;

	switch (msg) {
		case WM_INITDIALOG:
		{
			int max, winver = get_winver();
			// WASAPI device
			cb_num = wasapi_device_list(cb_info_IDC_COMBO_WASAPI_NAME);
			if (cb_num == 0)
				DI_DISABLE(IDC_COMBO_WASAPI_DEV);
			else
				DI_ENABLE(IDC_COMBO_WASAPI_DEV);
			for (i = 0; i < cb_num && i < WASAPI_DEVLIST_MAX; i++) {
				CB_INSSTRA(IDC_COMBO_WASAPI_DEV, &cb_info_IDC_COMBO_WASAPI_NAME[i].name);
				if (st_temp->wasapi_device_id == cb_info_IDC_COMBO_WASAPI_NAME[i].deviceID)
					cb_sel = i;
			}
			CB_SET(IDC_COMBO_WASAPI_DEV, (cb_sel));		
			// Latency
			SetDlgItemInt(hwnd, IDC_EDIT_WASAPI_LATENCY, st_temp->wasapi_latency, FALSE);
			SetDlgItemInt(hwnd, IDC_STATIC_WASAPI_LATENCY_MIN, cb_info_IDC_COMBO_WASAPI_NAME[cb_sel].LatencyMin, FALSE);
			SetDlgItemInt(hwnd, IDC_STATIC_WASAPI_LATENCY_MAX, cb_info_IDC_COMBO_WASAPI_NAME[cb_sel].LatencyMax, FALSE);
			// WASAPI WAVEFORMATEX
			if(st_temp->wasapi_format_ext == 0){
				CheckRadioButton(hwnd,IDC_RADIOBUTTON_WASAPI_FORMAT_EX,IDC_RADIOBUTTON_WASAPI_FORMAT_EXT,IDC_RADIOBUTTON_WASAPI_FORMAT_EX);
			}else{
				CheckRadioButton(hwnd,IDC_RADIOBUTTON_WASAPI_FORMAT_EX,IDC_RADIOBUTTON_WASAPI_FORMAT_EXT,IDC_RADIOBUTTON_WASAPI_FORMAT_EXT);
			}
			// WASAPI Share Mode
		//	CH_SET(IDC_CHECKBOX_WASAPI_EXCLUSIVE, st_temp->wasapi_exclusive);
			if(st_temp->wasapi_exclusive == 0){
				CheckRadioButton(hwnd, IDC_RADIOBUTTON_WASAPI_SHARE, IDC_RADIOBUTTON_WASAPI_EXCLUSIVE, IDC_RADIOBUTTON_WASAPI_SHARE);
			}else{
				CheckRadioButton(hwnd, IDC_RADIOBUTTON_WASAPI_SHARE, IDC_RADIOBUTTON_WASAPI_EXCLUSIVE, IDC_RADIOBUTTON_WASAPI_EXCLUSIVE);
			}
			// WASAPI Flags
			if(st_temp->wasapi_polling == 0){
				CheckRadioButton(hwnd, IDC_RADIOBUTTON_WASAPI_EVENT, IDC_RADIOBUTTON_WASAPI_POLLING, IDC_RADIOBUTTON_WASAPI_EVENT);
			}else{
				CheckRadioButton(hwnd, IDC_RADIOBUTTON_WASAPI_EVENT, IDC_RADIOBUTTON_WASAPI_POLLING, IDC_RADIOBUTTON_WASAPI_POLLING);
			}
			// WASAPI Thread Priority
			for (i = 0; i < cb_num_IDC_COMBO_WASAPI_PRIORITY; i++)
				CB_INSSTR(IDC_COMBO_WASAPI_PRIORITY, cb_info_IDC_COMBO_WASAPI_PRIORITY[i]);
			CB_SET(IDC_COMBO_WASAPI_PRIORITY, (st_temp->wasapi_priority));		
			// WASAPI Stream Category
			max = winver >= 3 ? cb_num_IDC_COMBO_WASAPI_STREAM_CATEGORY : 1;
			for (i = 0; i < max; i++)
				CB_INSSTR(IDC_COMBO_WASAPI_STREAM_CATEGORY, cb_info_IDC_COMBO_WASAPI_STREAM_CATEGORY[i]);
			if(winver >= 3) { // win8
				CB_SET(IDC_COMBO_WASAPI_STREAM_CATEGORY, (st_temp->wasapi_stream_category));
			}else{
				CB_SET(IDC_COMBO_WASAPI_STREAM_CATEGORY, 0);
				DI_DISABLE(IDC_COMBO_WASAPI_STREAM_CATEGORY);
			}
			// WASAPI Stream Option
			if(winver >= 6){ // win10
				SendDlgItemMessage(hwnd, IDC_CHECKBOX_WASAPI_STREAM_OPTIONS_RAW, BM_SETCHECK, (st_temp->wasapi_stream_option & 1) ? BST_CHECKED : BST_UNCHECKED, 0);
				SendDlgItemMessage(hwnd, IDC_CHECKBOX_WASAPI_STREAM_OPTIONS_MATCH_FORMAT, BM_SETCHECK, (st_temp->wasapi_stream_option & 2) ? BST_CHECKED : BST_UNCHECKED, 0);
				SendDlgItemMessage(hwnd, IDC_CHECKBOX_WASAPI_STREAM_OPTIONS_AMBISONICS, BM_SETCHECK, (st_temp->wasapi_stream_option & 4) ? BST_CHECKED : BST_UNCHECKED, 0);
			}else if(winver >= 4){ // win8.1
				SendDlgItemMessage(hwnd, IDC_CHECKBOX_WASAPI_STREAM_OPTIONS_RAW, BM_SETCHECK, (st_temp->wasapi_stream_option & 1) ? BST_CHECKED : BST_UNCHECKED, 0);
				DI_DISABLE(IDC_CHECKBOX_WASAPI_STREAM_OPTIONS_MATCH_FORMAT);
				DI_DISABLE(IDC_CHECKBOX_WASAPI_STREAM_OPTIONS_AMBISONICS);
			}else{
				DI_DISABLE(IDC_CHECKBOX_WASAPI_STREAM_OPTIONS_RAW);
				DI_DISABLE(IDC_CHECKBOX_WASAPI_STREAM_OPTIONS_MATCH_FORMAT);
				DI_DISABLE(IDC_CHECKBOX_WASAPI_STREAM_OPTIONS_AMBISONICS);
			}

			SetFocus(DI_GET(IDOK));
			return TRUE;
		}
		case WM_CLOSE:
			EndDialog(hwnd,FALSE);
			break;
		case WM_COMMAND:
			switch (LOWORD(wp)) {
			case IDC_COMBO_WASAPI_DEV:				
				if(IsWindowEnabled(GetDlgItem(hwnd, IDC_COMBO_WASAPI_DEV))) {
					cb_sel = SendDlgItemMessage(hwnd, IDC_COMBO_WASAPI_DEV, CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
					SetDlgItemInt(hwnd, IDC_STATIC_WASAPI_LATENCY_MIN, cb_info_IDC_COMBO_WASAPI_NAME[cb_sel].LatencyMin, FALSE);
					SetDlgItemInt(hwnd, IDC_STATIC_WASAPI_LATENCY_MAX, cb_info_IDC_COMBO_WASAPI_NAME[cb_sel].LatencyMax, FALSE);
				}	
				break;
			case IDCANCEL:
				PostMessage(hwnd,WM_CLOSE,(WPARAM)0,(LPARAM)0);
				break;
			case IDOK:
				// WASAPI device
				if(IsWindowEnabled(GetDlgItem(hwnd, IDC_COMBO_WASAPI_DEV))) {
					cb_sel = SendDlgItemMessage(hwnd, IDC_COMBO_WASAPI_DEV, CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
					st_temp->wasapi_device_id = cb_info_IDC_COMBO_WASAPI_NAME[cb_sel].deviceID;
				}	
				// Latency
				st_temp->wasapi_latency = GetDlgItemInt(hwnd, IDC_EDIT_WASAPI_LATENCY, NULL, FALSE);
				// WASAPI WAVEFORMATEX
				if(SendDlgItemMessage(hwnd,IDC_RADIOBUTTON_WASAPI_FORMAT_EX,BM_GETCHECK,0,0))
					st_temp->wasapi_format_ext = 0;
				else
					st_temp->wasapi_format_ext = 1;	
				// WASAPI Share Mode
			//	st_temp->wasapi_exclusive = CH_GET(IDC_CHECKBOX_WASAPI_EXCLUSIVE);
			//	st_temp->wasapi_exclusive = CH_GET(IDC_RADIOBUTTON_WASAPI_SHARE) ? 0 : 1;
				if(SendDlgItemMessage(hwnd, IDC_RADIOBUTTON_WASAPI_SHARE, BM_GETCHECK, 0, 0))
					st_temp->wasapi_exclusive = 0;
				else
					st_temp->wasapi_exclusive = 1;
				// WASAPI Flags
				if(SendDlgItemMessage(hwnd, IDC_RADIOBUTTON_WASAPI_EVENT, BM_GETCHECK, 0, 0))
					st_temp->wasapi_polling = 0;
				else
					st_temp->wasapi_polling = 1;
				// WASAPI Thread Priority
				cb_sel = SendDlgItemMessage(hwnd, IDC_COMBO_WASAPI_PRIORITY, CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
				st_temp->wasapi_priority = cb_sel;
				// WASAPI Stream Category
				st_temp->wasapi_stream_category = CB_GET(IDC_COMBO_WASAPI_STREAM_CATEGORY);
				// WASAPI Stream Option
				st_temp->wasapi_stream_option = 0;
				if (SendDlgItemMessage(hwnd, IDC_CHECKBOX_WASAPI_STREAM_OPTIONS_RAW, BM_GETCHECK, 0, 0))
					st_temp->wasapi_stream_option |= 1;
				if (SendDlgItemMessage(hwnd, IDC_CHECKBOX_WASAPI_STREAM_OPTIONS_MATCH_FORMAT, BM_GETCHECK, 0, 0))
					st_temp->wasapi_stream_option |= 2;
				if (SendDlgItemMessage(hwnd, IDC_CHECKBOX_WASAPI_STREAM_OPTIONS_AMBISONICS, BM_GETCHECK, 0, 0))
					st_temp->wasapi_stream_option |= 4;

				EndDialog(hwnd,TRUE);
				break;
			}
			break;
	}
	return FALSE;
}

void wasapiConfigDialog(void)
{
	DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_WASAPI), hPrefWnd, (DLGPROC)wasapiConfigDialogProc);
}

#endif // AU_WASAPI


#ifdef AU_WDMKS
///////////////////////////////////////////////////////////////////////
//
// WDMKS Config Dialog
//
///////////////////////////////////////////////////////////////////////

WDMKS_DEVICELIST cb_info_IDC_COMBO_WDMKS_INFO[WDMKS_DEVLIST_MAX];

#define cb_num_IDC_COMBO_WDMKS_PIN_PRIORITY 4
static const TCHAR *cb_info_IDC_COMBO_WDMKS_PIN_PRIORITY[] = {
    TEXT("Low"),
    TEXT("Normal"),
    TEXT("High"),
    TEXT("Exclusive"),
};

#define cb_num_IDC_COMBO_WDMKS_RT_PRIORITY 8
static const TCHAR *cb_info_IDC_COMBO_WDMKS_RT_PRIORITY[] = {
    TEXT("None"),
    TEXT("Audio"),
    TEXT("Capture"),
    TEXT("Distribution"),
    TEXT("Games"),
    TEXT("Playback"),
    TEXT("ProAudio"),
    TEXT("WindowManager"),
};

LRESULT WINAPI wdmksConfigDialogProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	int i = 0, cb_num = 0, cb_sel = 0, flag;

	switch (msg) {
		case WM_INITDIALOG:
		{
			// WDMKS device
			cb_num = wdmks_device_list(cb_info_IDC_COMBO_WDMKS_INFO);
			if (cb_num == 0)
				DI_DISABLE(IDC_COMBO_WDMKS_DEV);
			else
				DI_ENABLE(IDC_COMBO_WDMKS_DEV);
			for (i = 0; i < cb_num && i < WDMKS_DEVLIST_MAX; i++) {
				CB_INSSTRA(IDC_COMBO_WDMKS_DEV, &cb_info_IDC_COMBO_WDMKS_INFO[i].name);
				if (st_temp->wdmks_device_id == cb_info_IDC_COMBO_WDMKS_INFO[i].deviceID)
					cb_sel = i;
			}
			CB_SET(IDC_COMBO_WDMKS_DEV, (cb_sel));	
			// Device Info 			
			SetDlgItemInt(hwnd, IDC_STATIC_WDMKS_DEVICE_ID, cb_info_IDC_COMBO_WDMKS_INFO[cb_sel].deviceID, FALSE);
			if(cb_info_IDC_COMBO_WDMKS_INFO[cb_sel].isWaveRT)
				SendDlgItemMessage(hwnd,IDC_STATIC_WDMKS_STREAM_TYPE,WM_SETTEXT,0,(LPARAM)"WaveRT");
			else
				SendDlgItemMessage(hwnd,IDC_STATIC_WDMKS_STREAM_TYPE,WM_SETTEXT,0,(LPARAM)"WaveCyclic");
			SetDlgItemInt(hwnd, IDC_STATIC_WDMKS_RATE_MIN, cb_info_IDC_COMBO_WDMKS_INFO[cb_sel].minSampleRate, FALSE);
			SetDlgItemInt(hwnd, IDC_STATIC_WDMKS_RATE_MAX, cb_info_IDC_COMBO_WDMKS_INFO[cb_sel].maxSampleRate, FALSE);
			SetDlgItemInt(hwnd, IDC_STATIC_WDMKS_BITS_MIN, cb_info_IDC_COMBO_WDMKS_INFO[cb_sel].minBits, FALSE);
			SetDlgItemInt(hwnd, IDC_STATIC_WDMKS_BITS_MAX, cb_info_IDC_COMBO_WDMKS_INFO[cb_sel].maxBits, FALSE);
			if(cb_info_IDC_COMBO_WDMKS_INFO[cb_sel].isFloat)
				SendDlgItemMessage(hwnd,IDC_STATIC_WDMKS_FLOAT,WM_SETTEXT,0,(LPARAM)"Support");
			else
				SendDlgItemMessage(hwnd,IDC_STATIC_WDMKS_FLOAT,WM_SETTEXT,0,(LPARAM)"Not Support");
			SetDlgItemInt(hwnd, IDC_STATIC_WDMKS_LATENCY_MIN, cb_info_IDC_COMBO_WDMKS_INFO[cb_sel].minLatency, FALSE);
			SetDlgItemInt(hwnd, IDC_STATIC_WDMKS_LATENCY_MAX, cb_info_IDC_COMBO_WDMKS_INFO[cb_sel].maxLatency, FALSE);
			// Latency
			SetDlgItemInt(hwnd, IDC_EDIT_WDMKS_LATENCY, st_temp->wdmks_latency, FALSE);
			// WDMKS WAVEFORMATEX
			if(st_temp->wdmks_format_ext == 0){
				CheckRadioButton(hwnd,IDC_RADIOBUTTON_WDMKS_FORMAT_EX,IDC_RADIOBUTTON_WDMKS_FORMAT_EXT,IDC_RADIOBUTTON_WDMKS_FORMAT_EX);
			}else{
				CheckRadioButton(hwnd,IDC_RADIOBUTTON_WDMKS_FORMAT_EX,IDC_RADIOBUTTON_WDMKS_FORMAT_EXT,IDC_RADIOBUTTON_WDMKS_FORMAT_EXT);
			}
			// WDMKS Flags
			if(st_temp->wdmks_polling == 0){
				CheckRadioButton(hwnd, IDC_RADIOBUTTON_WDMKS_EVENT, IDC_RADIOBUTTON_WDMKS_POLLING, IDC_RADIOBUTTON_WDMKS_EVENT);
			}else{
				CheckRadioButton(hwnd, IDC_RADIOBUTTON_WDMKS_EVENT, IDC_RADIOBUTTON_WDMKS_POLLING, IDC_RADIOBUTTON_WDMKS_POLLING);
			}
			// WASAPI Thread Priority
			for (i = 0; i < CB_NUM(thread_priority_num); i++)		
				CB_INSSTR(IDC_COMBO_WDMKS_THREAD_PRIORITY, thread_priority_name_en[i] );
			CB_SET(IDC_COMBO_WDMKS_THREAD_PRIORITY, CB_FIND(thread_priority_num, st_temp->wdmks_thread_priority, 3));	
			// WDMKS Thread Priority
			for (i = 0; i < cb_num_IDC_COMBO_WDMKS_PIN_PRIORITY; i++)
				CB_INSSTR(IDC_COMBO_WDMKS_PIN_PRIORITY, cb_info_IDC_COMBO_WDMKS_PIN_PRIORITY[i]);
			CB_SET(IDC_COMBO_WDMKS_PIN_PRIORITY, (st_temp->wdmks_pin_priority));		
			// WDMKS Thread Priority RT
			for (i = 0; i < cb_num_IDC_COMBO_WDMKS_RT_PRIORITY; i++)
				CB_INSSTR(IDC_COMBO_WDMKS_RT_PRIORITY, cb_info_IDC_COMBO_WDMKS_RT_PRIORITY[i]);
			CB_SET(IDC_COMBO_WDMKS_RT_PRIORITY, (st_temp->wdmks_rt_priority));		

			SetFocus(DI_GET(IDOK));
			return TRUE;
		}
		case WM_CLOSE:
			EndDialog(hwnd,FALSE);
			break;
		case WM_COMMAND:
			switch (LOWORD(wp)) {
			case IDC_COMBO_WDMKS_DEV:				
				if(IsWindowEnabled(GetDlgItem(hwnd, IDC_COMBO_WDMKS_DEV))) {
					cb_sel = SendDlgItemMessage(hwnd, IDC_COMBO_WDMKS_DEV, CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
					// Device Info 	
					SetDlgItemInt(hwnd, IDC_STATIC_WDMKS_DEVICE_ID, cb_info_IDC_COMBO_WDMKS_INFO[cb_sel].deviceID, FALSE);
					if(cb_info_IDC_COMBO_WDMKS_INFO[cb_sel].isWaveRT)
						SendDlgItemMessage(hwnd,IDC_STATIC_WDMKS_STREAM_TYPE,WM_SETTEXT,0,(LPARAM)"WaveRT");
					else
						SendDlgItemMessage(hwnd,IDC_STATIC_WDMKS_STREAM_TYPE,WM_SETTEXT,0,(LPARAM)"WaveCyclic");
					SetDlgItemInt(hwnd, IDC_STATIC_WDMKS_RATE_MIN, cb_info_IDC_COMBO_WDMKS_INFO[cb_sel].minSampleRate, FALSE);
					SetDlgItemInt(hwnd, IDC_STATIC_WDMKS_RATE_MAX, cb_info_IDC_COMBO_WDMKS_INFO[cb_sel].maxSampleRate, FALSE);
					SetDlgItemInt(hwnd, IDC_STATIC_WDMKS_BITS_MIN, cb_info_IDC_COMBO_WDMKS_INFO[cb_sel].minBits, FALSE);
					SetDlgItemInt(hwnd, IDC_STATIC_WDMKS_BITS_MAX, cb_info_IDC_COMBO_WDMKS_INFO[cb_sel].maxBits, FALSE);
					if(cb_info_IDC_COMBO_WDMKS_INFO[cb_sel].isFloat)
						SendDlgItemMessage(hwnd,IDC_STATIC_WDMKS_FLOAT,WM_SETTEXT,0,(LPARAM)"Support");
					else
						SendDlgItemMessage(hwnd,IDC_STATIC_WDMKS_FLOAT,WM_SETTEXT,0,(LPARAM)"Not Support");
					SetDlgItemInt(hwnd, IDC_STATIC_WDMKS_LATENCY_MIN, cb_info_IDC_COMBO_WDMKS_INFO[cb_sel].minLatency, FALSE);
					SetDlgItemInt(hwnd, IDC_STATIC_WDMKS_LATENCY_MAX, cb_info_IDC_COMBO_WDMKS_INFO[cb_sel].maxLatency, FALSE);
				}	
				break;
			case IDCANCEL:
				PostMessage(hwnd,WM_CLOSE,(WPARAM)0,(LPARAM)0);
				break;
			case IDOK:
				// WDMKS device
				if(IsWindowEnabled(GetDlgItem(hwnd, IDC_COMBO_WDMKS_DEV))) {
					cb_sel = SendDlgItemMessage(hwnd, IDC_COMBO_WDMKS_DEV, CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
					st_temp->wdmks_device_id = cb_info_IDC_COMBO_WDMKS_INFO[cb_sel].deviceID;
				}	
				// Latency
				st_temp->wdmks_latency = GetDlgItemInt(hwnd, IDC_EDIT_WDMKS_LATENCY, NULL, FALSE);
				// WDMKS WAVEFORMATEX
				if(SendDlgItemMessage(hwnd,IDC_RADIOBUTTON_WDMKS_FORMAT_EX,BM_GETCHECK,0,0))
					st_temp->wdmks_format_ext = 0;
				else
					st_temp->wdmks_format_ext = 1;	
				// WDMKS Flags
				if(SendDlgItemMessage(hwnd, IDC_RADIOBUTTON_WDMKS_EVENT, BM_GETCHECK, 0, 0))
					st_temp->wdmks_polling = 0;
				else
					st_temp->wdmks_polling = 1;
				// WDMKS Thread Priority
				st_temp->wdmks_thread_priority = thread_priority_num[CB_GETS(IDC_COMBO_WDMKS_THREAD_PRIORITY, 2)];
				// WDMKS KS Priority
				cb_sel = SendDlgItemMessage(hwnd, IDC_COMBO_WDMKS_PIN_PRIORITY, CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
				st_temp->wdmks_pin_priority = cb_sel;
				// WDMKS RT Priority
				cb_sel = SendDlgItemMessage(hwnd, IDC_COMBO_WDMKS_RT_PRIORITY, CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
				st_temp->wdmks_rt_priority = cb_sel;

				EndDialog(hwnd,TRUE);
				break;
			}
			break;
	}
	return FALSE;
}

void wdmksConfigDialog(void)
{
	DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_WDMKS), hPrefWnd, (DLGPROC)wdmksConfigDialogProc);
}

#endif // AU_WDMKS