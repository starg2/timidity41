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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    w32g_main.c: Written by Daisuke Aoki <dai@y7.net>
                 Modified by Masanao Izumo <iz@onicos.co.jp>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <stddef.h>
#include <windows.h>
#include <mmsystem.h>
#undef RC_NONE
#include <shlobj.h>
// #include <prsht.h>
#include <commctrl.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <shlobj.h>

#include <windowsx.h>	/* There is no <windowsx.h> on CYGWIN.
			 * Edit_* and ListBox_* are defined in
			 * <windowsx.h>
			 */

#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "output.h"
#include "controls.h"

#include "wrd.h"

#include "w32g.h"
#include "w32g_res.h"
#include "w32g_utl.h"
#include "w32g_ut2.h"
#include "w32g_pref.h"
#include "w32g_subwin.h"
///r
#include "aq.h"
#ifdef INT_SYNTH
#include "w32g_int_synth_editor.h"	
#endif

#if defined(__CYGWIN32__) || defined(__MINGW32__)
#define WIN32GCC
WINAPI void InitCommonControls(void);
#endif

#if 0
#define GDI_LOCK() { \
	ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "GDI_LOCK(%s: %d)", __FILE__, __LINE__ ); \
	gdi_lock(); \
}
#define GDI_UNLOCK() { \
	ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "GDI_UNLOCK(%s: %d)", __FILE__, __LINE__ ); \
	gdi_unlock(); \
}
#else
#define GDI_LOCK() { gdi_lock(); }
#define GDI_UNLOCK() { gdi_unlock(); }
#endif

static void InitMainWnd(HWND hStartWnd);
static void MainWndItemResize(void);

static void ConsoleWndVerbosityApplySet(int num);
void ConsoleWndVerbosityApply(void);

void CanvasPaintAll(void);
void CanvasReset(void);
void CanvasClear(void);
void CanvasUpdate(int flag);
void CanvasReadPanelInfo(int flag);
void CanvasChange(int mode);
void MPanelResize(void);
void MPanelPaintAll(void);
void MPanelReadPanelInfo(int flag);
void MPanelReset(void);
void MPanelUpdateAll(void);
void ClearConsoleWnd(void);
void InitListWnd(HWND hParentWnd);
void InitTracerWnd(HWND hParentWnd);
void InitWrdWnd(HWND hParentWnd);
void InitDocWnd(HWND hParentWnd);
void InitListSearchWnd(HWND hParentWnd);
void PutsDocWnd(char *str);
void ClearDocWnd(void);
static void DlgPlaylistSave(HWND hwnd);
static void DlgPlaylistOpen(HWND hwnd);
static void DlgDirOpen(HWND hwnd);
static void DlgUrlOpen(HWND hwnd);
static void DlgMidiFileOpen(HWND hwnd);
void VprintfEditCtlWnd(HWND hwnd, char *fmt, va_list argList);
void PutsEditCtlWnd(HWND hwnd, char *str);
void ClearEditCtlWnd(HWND hwnd);

int	w32gSaveDefaultPlaylist(void);


#ifndef CLR_INVALID
#define CLR_INVALID 0xffffffff
#endif /* CLR_INVALID */
extern int optind;

HINSTANCE hInst;
static int progress_jump = -1;
static HWND hMainWndScrollbarProgressWnd;
static HWND hMainWndScrollbarVolumeWnd;
static HWND hMainWndScrollbarProgressTTipWnd;
static HWND hMainWndScrollbarVolumeTTipWnd;

//#define W32G_VOLUME_MAX 300
#define W32G_VOLUME_MAX MAX_AMPLIFICATION

// HWND
HWND hMainWnd = 0;
HWND hDebugWnd = 0;
HWND hConsoleWnd = 0;
HWND hTracerWnd = 0;
HWND hDocWnd = 0;
HWND hListWnd = 0;
HWND hWrdWnd = 0;
HWND hSoundSpecWnd = 0;
HWND hDebugEditWnd = 0;
HWND hDocEditWnd = 0;
HWND hUrlWnd = 0;

// Process.
HANDLE hProcess = 0;

// Main Thread.
HANDLE hMainThread = 0;
HANDLE hPlayerThread = 0;
HANDLE hMainThreadInfo = 0;
DWORD dwMainThreadID = 0;
static volatile int wait_thread_flag = 1;
typedef struct MAINTHREAD_ARGS_ {
	int *pArgc;
	char ***pArgv;
} MAINTHREAD_ARGS;
void WINAPI MainThread(void *arglist);

// Window Thread
HANDLE hWindowThread = 0;
HANDLE hWindowThreadInfo = 0;

// Thread
volatile int ThreadNumMax = 0;

// Debug Thread
volatile int DebugThreadExit = 1;
volatile HANDLE hDebugThread = 0;
void DebugThreadInit(void);
void PrintfDebugWnd(char *fmt, ...);
void ClearDebugWnd(void);
void InitDebugWnd(HWND hParentWnd);

// Flags
int InitMinimizeFlag = 0;
int DebugWndStartFlag = 1;
int ConsoleWndStartFlag = 0;
int ListWndStartFlag = 0;
int TracerWndStartFlag = 0;
int DocWndStartFlag = 0;
int WrdWndStartFlag = 0;
int SoundSpecWndStartFlag = 0;

int DebugWndFlag = 1;
int ConsoleWndFlag = 1;
int ListWndFlag = 1;
int TracerWndFlag = 0;
int DocWndFlag = 1;
int WrdWndFlag = 0;
int SoundSpecWndFlag = 0;
///r
int RestartTimidity = 0;
int ConsoleClearFlag = 0;

int WrdGraphicFlag;
int TraceGraphicFlag;

char *IniFile;
char *ConfigFile;
char *PlaylistFile;
char *PlaylistHistoryFile;
char *MidiFileOpenDir;
char *ConfigFileOpenDir;
char *PlaylistFileOpenDir;

///r
// Priority
//int PlayerThreadPriority;
int MidiPlayerThreadPriority;
int MainThreadPriority;
int GUIThreadPriority;
int TracerThreadPriority;
int WrdThreadPriority;

// dir
int SeachDirRecursive = 0;	// 再帰的ディレクトリ検索 
// Ini File
int IniFileAutoSave = 1;	// INI ファイルの自動セーブ

// misc
int DocMaxSize;
char *DocFileExt;

int AutoloadPlaylist = 0;
int AutosavePlaylist = 0;
int volatile save_playlist_once_before_exit_flag = 1;

static volatile int w32g_wait_for_init;
void w32g_send_rc(int rc, ptr_size_t value);
int w32g_lock_open_file = 0;

void TiMidityHeapCheck(void);

volatile DWORD dwWindowThreadID = -1;
void w32g_i_init(void)
{
    ThreadNumMax++;
    hProcess = GetCurrentProcess();
    hWindowThread = GetCurrentThread();
	dwWindowThreadID = GetCurrentThreadId();

    InitCommonControls();

#ifdef W32GUI_DEBUG
    DebugThreadInit();
#endif
}

int PlayerLanguage = LANGUAGE_ENGLISH;
//int PlayerLanguage = LANGUAGE_JAPANESE;
#define PInfoOK 1
long SetValue(int32 value, int32 min, int32 max)
{
  int32 v = value;
  if(v < min) v = min;
  else if( v > max) v = max;
  return v;
}

int w32gSecondTiMidity(int opt, int argc, char **argv, int nfiles, char **files);
int w32gSecondTiMidityExit(void);
int SecondMode = 1;

void FirstLoadIniFile(void);

extern int TracerWndDrawSkip;

#if (defined(__W32G__) && !defined(TWSYNG32))
extern void CmdLineToArgv(LPSTR lpCmdLine, int *argc, CHAR ***argv);
extern int win_main(int argc, char **argv); /* timidity.c */
int WINAPI
_tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
LPTSTR lpCmdLine, int nCmdShow)
{
	
	int argc = 0;
	CHAR **argv = NULL;
	int errcode;
	int i;
	static int first = 0;

	Sleep(100); // Restartで前プロセスの終了待機
#ifdef TIMIDITY_LEAK_CHECK
	_CrtSetDbgFlag(CRTDEBUGFLAGS);
#endif
	char *cmdline = tchar_to_char(lpCmdLine);
	CmdLineToArgv(cmdline,&argc,&argv);
	safe_free(cmdline);
#if 0
	FirstLoadIniFile();
	if(w32gSecondTiMidity(SecondMode,argc,argv)==TRUE){
		int res = win_main(argc, argv);
		w32gSecondTiMidityExit();
		for(i = 0; i < argc; i ++) {
			safe_free(argv[i]);
		}
		safe_free(argv);
		return res;
	} else {
		for(i = 0; i < argc; i ++) {
			safe_free(argv[i]);
		}
		safe_free(argv);
		return -1;
	}
#else
	wrdt=wrdt_list[0];
	errcode = win_main(argc, argv);
	for(i = 0; i < argc; i ++) {
		safe_free(argv[i]);
	}
	safe_free(argv);

	if(RestartTimidity){
		PROCESS_INFORMATION pi;
		STARTUPINFO si;
		TCHAR path[FILEPATH_MAX] = _T("");
		RestartTimidity = 0;
		memset(&si, 0, sizeof(si));
		si.cb  = sizeof(si);
		GetModuleFileName(hInstance, path, MAX_PATH);
		if(CreateProcess(path, lpCmdLine, NULL, NULL, TRUE, CREATE_DEFAULT_ERROR_MODE, NULL, NULL, &si, &pi) == FALSE)
			MessageBox(NULL, _T("Restart Error."), _T("TiMidity++ Win32GUI"), MB_OK | MB_ICONEXCLAMATION);
	}
	return errcode;
#endif
}
#endif /* (__W32G__ && !TWSYNG32) */

// ***************************************************************************
// System Function

void CALLBACK KillProcess(UINT IDEvent, UINT uReserved, DWORD_PTR dwUser,
	DWORD_PTR dwReserved1, DWORD_PTR dwReserved2)
{
	exit(0);
	//	ExitProcess(0);
}

void OnExit(void)
{
#ifdef W32GUI_DEBUG
	PrintfDebugWnd("PostQuitMessage\n");
	Sleep(200);
#endif
	PostQuitMessage(0);
}

static int OnExitReadyWait = 200;
void OnExitReady(void)
{
	int i;
#ifdef W32GUI_DEBUG
	PrintfDebugWnd("OnExitReady: Start.\n");
#endif
    w32g_send_rc(RC_STOP, 0);

	/* Exit after 10 sec. */
	
	timeSetEvent(10000, 1000, KillProcess, 0, TIME_ONESHOT);

	/* Wait really stopping to play */
	i = 1000/OnExitReadyWait; /* 1 sec. */
    while(w32g_play_active && i-- > 0)
	{
		Sleep(OnExitReadyWait);
		VOLATILE_TOUCH(w32g_play_active);
	}

#ifdef W32GUI_DEBUG
	PrintfDebugWnd("OnExitReady: End.\n");
#endif
}

void OnQuit(void)
{
	SendMessage(hMainWnd, WM_CLOSE, 0, 0);
}


// ***************************************************************************
// Start Window
// 大元のウィンドウの地位はMain Windowに譲り、今ではただの初期化関数

void InitStartWnd(int nCmdShow)
{
	InitMainWnd(NULL);
	InitConsoleWnd(hMainWnd);
	InitListWnd(hMainWnd);
	InitTracerWnd(hMainWnd);
	InitDocWnd(hMainWnd);
	InitWrdWnd(hMainWnd);
	InitSoundSpecWnd(hMainWnd);

    hMainWndScrollbarProgressWnd = GetDlgItem(hMainWnd, IDC_SCROLLBAR_PROGRESS);
	hMainWndScrollbarVolumeWnd = GetDlgItem(hMainWnd, IDC_SCROLLBAR_VOLUME);
	EnableScrollBar(hMainWndScrollbarVolumeWnd, SB_CTL,ESB_ENABLE_BOTH);
	SetScrollRange(hMainWndScrollbarVolumeWnd, SB_CTL,
		       0, W32G_VOLUME_MAX, TRUE);
///r
	SetScrollPos(hMainWndScrollbarVolumeWnd, SB_CTL,
		     W32G_VOLUME_MAX - output_amplification, TRUE);
	
    SetForegroundWindow(hMainWnd);
    MainWndItemResize();
}

/*****************************************************************************/
// Main Window

#define SWS_EXIST		0x0001
#define SWS_ICON		0x0002
#define SWS_HIDE		0x0004
typedef struct SUBWINDOW_ {
	HWND *hwnd;
	int status;
}	SUBWINDOW;
SUBWINDOW subwindow[] =
{
  {&hConsoleWnd,0},
  {&hListWnd,0},
  {&hTracerWnd,0},
  {&hDocWnd,0},
  {&hWrdWnd,0},
  {&hSoundSpecWnd,0},
  {NULL,0}
};

int SubWindowMax = 5;
SUBWINDOW SubWindowHistory[] =
{
  {&hConsoleWnd,0},
  {&hListWnd,0},
  {&hTracerWnd,0},
  {&hDocWnd,0},
  {&hWrdWnd,0},
  {&hSoundSpecWnd,0},
  {NULL,0}
};

MAINWNDINFO MainWndInfo;

static TOOLINFO SBVolumeTooltipInfo, SBProgressTooltipInfo;
static TCHAR SBVolumeTooltipText[8], // "0000 %\0"
	     SBProgressTooltipText[20]; // "000:00:00/000:00:00\0"

LRESULT CALLBACK MainProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
void update_subwindow(void);
void OnShow(void);
void OnHide(void);

static int MainWndInfoReset(HWND hwnd);
static int MainWndInfoApply(void);

extern void reload_cfg(void);


///r
/*
create top-level menu / system menu
override w32g_res.rc IDM_MENU_MAIN IDM_MENU_MAIN_EN
項目の変更が可能になったのでリソースメニューはイミなしに・・
*/
// MENU MODULE 
#define IDM_MODULE 42000 // MainProc() sort ID
static HMENU hMenuModule;
// MENU OUTPUT
#define IDM_OUTPUT 41000 // MainProc() sort ID
#define IDM_OUTPUT_OPTIONS 41099 // MainProc() sort ID
static const size_t outputItemStart = 2;
static HMENU hMenuOutput;

static void InitMainMenu(HWND hWnd)
{
	HMENU hMenu, hMenuFile, hMenuConfig, hMenuWindow, hMenuHelp;
	HMENU hSystemMenu;
	MENUITEMINFO mii;
	int i;

	// top-level menu
	hMenu = GetMenu(hWnd);
	if (hMenu != NULL)	{DestroyMenu (hMenu);}
	hMenu = CreateMenu();
	hMenuFile = CreateMenu();
	hMenuConfig = CreateMenu();
	hMenuWindow = CreateMenu();
	hMenuModule = CreateMenu();
	hMenuOutput = CreateMenu();
	hMenuHelp = CreateMenu();
	mii.cbSize = sizeof (MENUITEMINFO);
	mii.fMask = MIIM_TYPE | MIIM_SUBMENU;
	mii.fType = MFT_STRING;
	if (PlayerLanguage == LANGUAGE_JAPANESE) {
		mii.hSubMenu = hMenuFile;
		mii.dwTypeData = TEXT("ファイル(&F)");
		InsertMenuItem(hMenu, 0, TRUE, &mii);
		mii.hSubMenu = hMenuConfig;
		mii.dwTypeData = TEXT("設定(&C)");
		InsertMenuItem(hMenu, 1, TRUE, &mii);
		mii.hSubMenu = hMenuWindow;
		mii.dwTypeData = TEXT("ウィンドウ(&W)");
		InsertMenuItem(hMenu, 2, TRUE, &mii);
		mii.hSubMenu = hMenuModule;
		mii.dwTypeData = TEXT("モジュール(&M)");
		InsertMenuItem(hMenu, 3, TRUE, &mii);
		mii.hSubMenu = hMenuOutput;
		mii.dwTypeData = TEXT("出力(&O)");
		InsertMenuItem(hMenu, 4, TRUE, &mii);
		mii.hSubMenu = hMenuHelp;
		mii.dwTypeData = TEXT("ヘルプ(&H)");
		InsertMenuItem(hMenu, 5, TRUE, &mii);
		// File
		AppendMenu(hMenuFile, MF_STRING, IDM_MFOPENFILE, TEXT("ファイルを開く(&F)"));
		AppendMenu(hMenuFile, MF_STRING, IDM_MFOPENDIR, TEXT("フォルダを開く(&D)"));
#ifdef SUPPORT_SOCKET
		AppendMenu(hMenuFile, MF_STRING, IDM_MFOPENURL, TEXT("URL を開く(&U)"));
#endif
		AppendMenu(hMenuFile, MF_SEPARATOR, 0, 0);
		AppendMenu(hMenuFile, MF_STRING, IDM_MFLOADPLAYLIST, TEXT("プレイリストを開く(&P)"));
		AppendMenu(hMenuFile, MF_STRING, IDM_MFSAVEPLAYLISTAS, TEXT("プレイリストを保存(&S)"));
		AppendMenu(hMenuFile, MF_SEPARATOR, 0, 0);
		AppendMenu(hMenuFile, MF_STRING, IDM_MFRESTART, TEXT("再起動（&R)"));
		AppendMenu(hMenuFile, MF_STRING, IDM_MFEXIT, TEXT("終了(&X)"));
		// Config
		AppendMenu(hMenuConfig, MF_STRING, IDM_SETTING, TEXT("詳細設定(&P)"));
		AppendMenu(hMenuConfig, MF_SEPARATOR, 0, 0);
		AppendMenu(hMenuConfig, MF_STRING, IDM_MCLOADINIFILE, TEXT("設定読込(&L)"));
		AppendMenu(hMenuConfig, MF_STRING, IDM_MCSAVEINIFILE, TEXT("設定保存(&S)"));
		AppendMenu(hMenuConfig, MF_SEPARATOR, 0, 0);
		AppendMenu(hMenuConfig, MF_STRING, IDM_FORCE_RELOAD, TEXT("cfg強制再読込(&F)"));
		// Window
        AppendMenu(hMenuWindow, MF_STRING, IDM_MWPLAYLIST, TEXT("プレイリスト(&L)"));
        AppendMenu(hMenuWindow, MF_STRING, IDM_MWTRACER, TEXT("トレーサ(&T)"));
        AppendMenu(hMenuWindow, MF_STRING, IDM_MWDOCUMENT, TEXT("ドキュメント(&D)"));
        AppendMenu(hMenuWindow, MF_STRING, IDM_MWWRDTRACER, TEXT("WRD(&W)"));
        AppendMenu(hMenuWindow, MF_STRING, IDM_MWCONSOLE, TEXT("コンソール(&C)"));
#ifdef VST_LOADER_ENABLE
        AppendMenu(hMenuWindow, MF_STRING, IDM_MWVSTMGR, TEXT("VSTマネージャ(&V)"));
#endif /* VST_LOADER_ENABLE */
#ifdef SUPPORT_SOUNDSPEC
        AppendMenu(hMenuWindow, MF_STRING, IDM_MWSOUNDSPEC, TEXT("スペクトログラム(&S)"));
#endif
#ifdef INT_SYNTH
        AppendMenu(hMenuWindow, MF_STRING, IDM_MWISEDITOR, TEXT("内蔵シンセエディタ(&E)"));
#endif
		// Help
        AppendMenu(hMenuHelp, MF_STRING, IDM_MHONLINEHELP, TEXT("オンラインヘルプ(&O)"));
        AppendMenu(hMenuHelp, MF_STRING, IDM_MHONLINEHELPCFG, TEXT("コンフィグファイル詳解(&C)"));
        AppendMenu(hMenuHelp, MF_STRING, IDM_MHBTS, TEXT("バグ報告所(&B)"));
		AppendMenu(hMenuHelp, MF_SEPARATOR, 0, 0);
        AppendMenu(hMenuHelp, MF_STRING, IDM_MHTIMIDITY, TEXT("TiMidity++について(&T)"));
        AppendMenu(hMenuHelp, MF_STRING, IDM_MHVERSION, TEXT("バージョン情報(&V)"));
		AppendMenu(hMenuHelp, MF_SEPARATOR, 0, 0);
        AppendMenu(hMenuHelp, MF_STRING, IDM_MHSUPPLEMENT, TEXT("補足(&S)"));
	}else{
		mii.hSubMenu = hMenuFile;
		mii.dwTypeData = TEXT("File(&F)");
		InsertMenuItem(hMenu, 0, TRUE, &mii);
		mii.hSubMenu = hMenuConfig;
		mii.dwTypeData = TEXT("Config(&C)");
		InsertMenuItem(hMenu, 1, TRUE, &mii);
		mii.hSubMenu = hMenuWindow;
		mii.dwTypeData = TEXT("Window(&W)");
		InsertMenuItem(hMenu, 2, TRUE, &mii);
		mii.hSubMenu = hMenuModule;
		mii.dwTypeData = TEXT("Module(&M)");
		InsertMenuItem(hMenu, 3, TRUE, &mii);
		mii.hSubMenu = hMenuOutput;
		mii.dwTypeData = TEXT("Output(&O)");
		InsertMenuItem(hMenu, 4, TRUE, &mii);
		mii.hSubMenu = hMenuHelp;
		mii.dwTypeData = TEXT("Help(&H)");
		InsertMenuItem(hMenu, 5, TRUE, &mii);
		// File
		AppendMenu(hMenuFile, MF_STRING, IDM_MFOPENFILE, TEXT("Open File(&F)"));
		AppendMenu(hMenuFile, MF_STRING, IDM_MFOPENDIR, TEXT("Open Directory(&D)"));
#ifdef SUPPORT_SOCKET
		AppendMenu(hMenuFile, MF_STRING, IDM_MFOPENURL, TEXT("Open Internet URL(&U)"));
#endif
		AppendMenu(hMenuFile, MF_SEPARATOR, 0, 0);
		AppendMenu(hMenuFile, MF_STRING, IDM_MFLOADPLAYLIST, TEXT("Load Playlist(&P)"));
		AppendMenu(hMenuFile, MF_STRING, IDM_MFSAVEPLAYLISTAS, TEXT("Save Playlist as(&S)"));
		AppendMenu(hMenuFile, MF_SEPARATOR, 0, 0);
		AppendMenu(hMenuFile, MF_STRING, IDM_MFRESTART, TEXT("Restart(&R)"));
		AppendMenu(hMenuFile, MF_STRING, IDM_MFEXIT, TEXT("Exit(&X)"));
		// Config
		AppendMenu(hMenuConfig, MF_STRING, IDM_SETTING, TEXT("Preference(&P)"));
		AppendMenu(hMenuConfig, MF_SEPARATOR, 0, 0);
		AppendMenu(hMenuConfig, MF_STRING, IDM_MCLOADINIFILE, TEXT("Load ini file(&L)"));
		AppendMenu(hMenuConfig, MF_STRING, IDM_MCSAVEINIFILE, TEXT("Save ini file(&S)"));
		AppendMenu(hMenuConfig, MF_SEPARATOR, 0, 0);
		AppendMenu(hMenuConfig, MF_STRING, IDM_FORCE_RELOAD, TEXT("Reload cfg file(&F)"));
		// Window
        AppendMenu(hMenuWindow, MF_STRING, IDM_MWPLAYLIST, TEXT("Play List(&L)"));
        AppendMenu(hMenuWindow, MF_STRING, IDM_MWTRACER, TEXT("Tracer(&T)"));
        AppendMenu(hMenuWindow, MF_STRING, IDM_MWDOCUMENT, TEXT("Document(&D)"));
        AppendMenu(hMenuWindow, MF_STRING, IDM_MWWRDTRACER, TEXT("Wrd tracer(&W)"));
        AppendMenu(hMenuWindow, MF_STRING, IDM_MWCONSOLE, TEXT("Console(&C)"));
#ifdef VST_LOADER_ENABLE
        AppendMenu(hMenuWindow, MF_STRING, IDM_MWVSTMGR, TEXT("VST Manager(&V)"));
#endif /* VST_LOADER_ENABLE */
#ifdef HAVE_SOUNDSPEC
        AppendMenu(hMenuWindow, MF_STRING, IDM_MWSOUNDSPEC, TEXT("Sound Spectrogram(&S)"));
#endif
#ifdef INT_SYNTH
        AppendMenu(hMenuWindow, MF_STRING, IDM_MWISEDITOR, TEXT("Internal Synth Editor(&E)"));
#endif
		// Help
        AppendMenu(hMenuHelp, MF_STRING, IDM_MHONLINEHELP, TEXT("Online Help(&O)"));
        AppendMenu(hMenuHelp, MF_STRING, IDM_MHBTS, TEXT("Bug Tracking System(&B)"));
		AppendMenu(hMenuHelp, MF_SEPARATOR, 0, 0);
        AppendMenu(hMenuHelp, MF_STRING, IDM_MHTIMIDITY, TEXT("TiMidity++(&T)"));
        AppendMenu(hMenuHelp, MF_STRING, IDM_MHVERSION, TEXT("Version(&V)"));
		AppendMenu(hMenuHelp, MF_SEPARATOR, 0, 0);
        AppendMenu(hMenuHelp, MF_STRING, IDM_MHSUPPLEMENT, TEXT("Supplement(&S)"));
	}
	// Module
	for (i = 0; i < module_list_num; i++) {
		mii.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
		mii.wID = IDM_MODULE + i;
		mii.fType = MFT_STRING;
		if (st_temp->opt_default_module == module_list[i].num) {
			mii.fState = MFS_CHECKED;
		} else {
			mii.fState = MFS_UNCHECKED;
		}
		if (i > 0 && i % (module_list_num / 2) == 0)
			mii.fType |= MFT_MENUBARBREAK;
		TCHAR *t = char_to_tchar(module_list[i].name);
		mii.dwTypeData = t;
		InsertMenuItem(hMenuModule, i, TRUE, &mii);
		safe_free(t);
	}
	// Output
	mii.fMask = MIIM_TYPE | MIIM_ID;
	mii.wID = IDM_OUTPUT_OPTIONS;
	mii.fType = MFT_STRING;
	if (PlayerLanguage == LANGUAGE_JAPANESE) {
		mii.dwTypeData = _T("オプション(&O)");
	} else {
		mii.dwTypeData = _T("&Options");
	}
	InsertMenuItem(hMenuOutput, 0, TRUE, &mii);
	mii.fMask = MIIM_TYPE;
	mii.fType = MFT_SEPARATOR;
	InsertMenuItem(hMenuOutput, 1, TRUE, &mii);
	for (i = 0; play_mode_list[i] != 0; i++) {
		mii.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
		mii.wID = IDM_OUTPUT + i;
		mii.fType = MFT_STRING;
		if (st_temp->opt_playmode[0] == play_mode_list[i]->id_character) {
			mii.fState = MFS_CHECKED;
		} else {
			mii.fState = MFS_UNCHECKED;
		}
		TCHAR* t = char_to_tchar(play_mode_list[i]->id_name);
		mii.dwTypeData = t;
		InsertMenuItem(hMenuOutput, outputItemStart + i, TRUE, &mii);
		safe_free(t);
	}	
	SetMenu(hWnd , hMenu);
	// system menu
	hSystemMenu = GetSystemMenu(hWnd, FALSE);
	RemoveMenu(hSystemMenu,SC_MAXIMIZE,MF_BYCOMMAND);
	//RemoveMenu(hSystemMenu,SC_SIZE,MF_BYCOMMAND); // comment out for Resize MainWindow 
	EnableMenuItem(hSystemMenu, SC_MOVE, MF_BYCOMMAND | MF_GRAYED);
	InsertMenu(hSystemMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, 0); // 7
	InsertMenu(hSystemMenu, 0, MF_BYPOSITION, SC_SCREENSAVE, _T("Screen Saver")); // 6
	InsertMenu(hSystemMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, 0); // 5
	InsertMenu(hSystemMenu, 0, MF_BYPOSITION | MF_STRING, IDM_STOP, _T("Stop")); // 4
	InsertMenu(hSystemMenu, 0, MF_BYPOSITION | MF_STRING, IDM_PAUSE, _T("Pause")); // 3
	InsertMenu(hSystemMenu, 0, MF_BYPOSITION | MF_STRING, IDM_PREV, _T("Prev")); // 2
	InsertMenu(hSystemMenu, 0, MF_BYPOSITION | MF_STRING, IDM_NEXT, _T("Next")); // 1
	InsertMenu(hSystemMenu, 0, MF_BYPOSITION | MF_STRING, IDM_PLAY, _T("Play")); // 0	

	DrawMenuBar(hWnd);
}

static void UpdateModuleMenu(HWND hWnd, UINT wId)
{
	MENUITEMINFO mii;
	int i, num = -1, oldnum;

	for (i = 0; i < module_list_num; i++) {
		mii.cbSize = sizeof (MENUITEMINFO);
		mii.fMask = MIIM_STATE | MIIM_ID;
		GetMenuItemInfo(hMenuModule, i, TRUE, &mii);
		if (wId == mii.wID) {
			mii.fState = MFS_CHECKED;
			num = i;
		} else {
			mii.fState = MFS_UNCHECKED;
		}
		SetMenuItemInfo(hMenuModule, i, TRUE, &mii);
		if (st_temp->opt_default_module == module_list[i].num) {
			oldnum = i;
		}
	}
//	if (!w32g_play_active && num != oldnum) {
	if (num != oldnum) {
		if (num >= 0) {st_temp->opt_default_module = module_list[num].num;}
		else {st_temp->opt_default_module = MODULE_TIMIDITY_DEFAULT;}
		PrefSettingApplyReally();
	}
}

static void UpdateOutputMenu(HWND hWnd, UINT wId)
{
	MENUITEMINFO mii;
	int i, num = -1, oldnum;

	for (i = 0; play_mode_list[i] != 0; i++) {
		mii.cbSize = sizeof(MENUITEMINFO);
		mii.fMask = MIIM_STATE | MIIM_ID;
		GetMenuItemInfo(hMenuOutput, outputItemStart + i, TRUE, &mii);
		if (wId == mii.wID) {
			mii.fState = MFS_CHECKED;
			num = i;
		} else { mii.fState = MFS_UNCHECKED; }
		SetMenuItemInfo(hMenuOutput, outputItemStart + i, TRUE, &mii);
		if (st_temp->opt_playmode[0] == play_mode_list[i]->id_character) {
			oldnum = i;
		}
	}
//	if (!w32g_play_active && num != oldnum) {
	if (num != oldnum) {
		if (num >= 0) { st_temp->opt_playmode[0] = play_mode_list[num]->id_character; }
		else { st_temp->opt_playmode[0] = 'd'; }
		w32g_send_rc(RC_STOP, 0);
		PrefSettingApplyReally();
	}
}

static void RefreshModuleMenu(HWND hWnd)
{
    UINT flags;
    int i;

    for (i = 0; i < module_list_num; i++) {
        flags = MF_STRING;
        if (st_temp->opt_default_module == module_list[i].num) {
            flags |= MFS_CHECKED;
        }
        CheckMenuItem(hMenuModule, IDM_MODULE + i, MF_BYCOMMAND | flags);
    }
}

static void RefreshOutputMenu(HWND hWnd)
{
	MENUITEMINFO mii;
	int i;

	for (i = 0; play_mode_list[i] != 0; i++) {
		mii.cbSize = sizeof(MENUITEMINFO);
		mii.fMask = MIIM_STATE | MIIM_ID;
		GetMenuItemInfo(hMenuOutput, outputItemStart + i, TRUE, &mii);
		if (st_temp->opt_playmode[0] == play_mode_list[i]->id_character) {
			mii.fState = MFS_CHECKED;
		} else {
			mii.fState = MFS_UNCHECKED;
		}
		SetMenuItemInfo(hMenuOutput, outputItemStart + i, TRUE, &mii);
	}
}

static void InitMainWnd(HWND hParentWnd)
{
	HICON hIcon = LoadImage(hInst, MAKEINTRESOURCE(IDI_ICON_TIMIDITY), IMAGE_ICON, 16, 16, 0);
	if ( hMainWnd != NULL ) {
		DestroyWindow ( hMainWnd );
		hMainWnd = NULL;
	}
	MainWndInfoReset(NULL);
	INILoadMainWnd();
	if (PlayerLanguage == LANGUAGE_JAPANESE)
	  hMainWnd = CreateDialog(hInst,MAKEINTRESOURCE(IDD_DIALOG_MAIN),hParentWnd,MainProc);
	else
	  hMainWnd = CreateDialog(hInst,MAKEINTRESOURCE(IDD_DIALOG_MAIN_EN),hParentWnd,MainProc);
	//InithMenuModule(hMainWnd);
	//InithMenuOutput(hMainWnd);

	if (hIcon!=NULL) SendMessage(hMainWnd,WM_SETICON,FALSE,(LPARAM)hIcon);
	{  // Set the title of the main window again.
   	char buffer[256];
   	SendMessage( hMainWnd, WM_GETTEXT, (WPARAM)255, (LPARAM)buffer);
   	SendMessage( hMainWnd, WM_SETTEXT, (WPARAM)0, (LPARAM)buffer);
	}
	MainWndInfoReset(hMainWnd);
	INILoadMainWnd();
	MainWndInfoApply();
}

static int MainWndInfoReset(HWND hwnd)
{
	memset(&MainWndInfo,0,sizeof(MAINWNDINFO));
	MainWndInfo.PosX = - 1;
	MainWndInfo.PosY = - 1;	
	MainWndInfo.Width = - 1;
	MainWndInfo.Height = - 1;
	MainWndInfo.hwnd = hwnd;
	return 0;
}

static int MainWndInfoApply(void)
{	
	RECT d_rc, w_rc;

	GetClientRect ( GetDesktopWindow (), &d_rc );
	GetWindowRect ( MainWndInfo.hwnd, &w_rc );
	d_rc.right -= w_rc.right - w_rc.left;
	d_rc.bottom -= w_rc.bottom - w_rc.top;
	if ( MainWndInfo.PosX < d_rc.left ) MainWndInfo.PosX = d_rc.left; 
	if ( MainWndInfo.PosX > d_rc.right ) MainWndInfo.PosX = d_rc.right; 
	if ( MainWndInfo.PosY < d_rc.top ) MainWndInfo.PosY = d_rc.top; 
	if ( MainWndInfo.PosY > d_rc.bottom ) MainWndInfo.PosY = d_rc.bottom; 
	SetWindowPosSize(GetDesktopWindow(), MainWndInfo.hwnd, MainWndInfo.PosX, MainWndInfo.PosY );	
	if(MainWndInfo.Width <= 0) 
		MainWndInfo.Width = 480;
	if(MainWndInfo.Height <= 0) 
		MainWndInfo.Height = 160;
	MoveWindow(MainWndInfo.hwnd, MainWndInfo.PosX, MainWndInfo.PosY, MainWndInfo.Width, MainWndInfo.Height,TRUE);
	MainWndInfo.init = 1;
	return 0;
}

void MainCmdProc(HWND hwnd, int wId, HWND hwndCtl, UINT wNotifyCode);

void MainWndSetPauseButton(int flag);
void MainWndSetPlayButton(int flag);

void MainWndToggleConsoleButton(void);
void MainWndUpdateConsoleButton(void);
void MainWndToggleTracerButton(void);
void MainWndUpdateTracerButton(void);
void MainWndToggleListButton(void);
void MainWndUpdateListButton(void);
void MainWndToggleDocButton(void);
void MainWndUpdateDocButton(void);
void MainWndToggleWrdButton(void);
void MainWndUpdateWrdButton(void);
void MainWndToggleSoundSpecButton(void);
void MainWndUpdateSoundSpecButton(void);

void ShowSubWindow(HWND hwnd,int showflag);
void ToggleSubWindow(HWND hwnd);

static void VersionWnd(HWND hParentWnd);
static void TiMidityWnd(HWND hParentWnd);
static void SupplementWnd(HWND hParentWnd);

static void InitCanvasWnd(HWND hwnd);
static void CanvasInit(HWND hwnd);
static void InitPanelWnd(HWND hwnd);
static void MPanelInit(HWND hwnd);

static void InitMainToolbar(HWND hwnd);
static void InitSubWndToolbar(HWND hwnd);

static UINT PlayerForwardAndBackwardEventID = 0;
static void CALLBACK PlayerForward(UINT IDEvent, UINT uReserved, DWORD_PTR dwUser,
	DWORD_PTR dwReserved1, DWORD_PTR dwReserved2)
{
	w32g_send_rc(RC_FORWARD, play_mode->rate);
}

static void CALLBACK PlayerBackward(UINT IDEvent, UINT uReserved, DWORD_PTR dwUser,
	DWORD_PTR dwReserved1, DWORD_PTR dwReserved2)
{
	w32g_send_rc(RC_BACK, play_mode->rate);
}

static void CallPrefWnd(UINT_PTR cId);
extern void ShowPrefWnd ( void );
extern void HidePrefWnd ( void );
extern BOOL IsVisiblePrefWnd ( void );

extern int TracerWndDrawSkip;

#define WM_UPDATE_SCROLLBAR_PROGRESS  (WM_APP + 100)  // (int)lParam: sec

LRESULT CALLBACK
MainProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	static BOOL PrefWndShow;

	// PrintfDebugWnd("MainProc: Mess%lx WPARAM%lx LPARAM%lx\n",uMess,wParam,lParam);
	switch (uMess)
	{
      case WM_INITDIALOG:
#ifdef VST_LOADER_ENABLE
	if (hVSTHost == NULL) {
#ifdef _WIN64
		hVSTHost = LoadLibrary(_T("timvstwrap_x64.dll"));
#else
		hVSTHost = LoadLibrary(_T("timvstwrap.dll"));
#endif
		if (hVSTHost != NULL) {
			((vst_open)GetProcAddress(hVSTHost, "vstOpen"))();
	//		((vst_open_config_all)GetProcAddress(hVSTHost, "openEffectEditorAll"))(hwnd);
		}
	}
#endif
		PrefWndShow = FALSE;
		update_subwindow();
		MainWndUpdateConsoleButton();
		MainWndUpdateTracerButton();
		MainWndUpdateListButton();
		MainWndUpdateDocButton();
		MainWndUpdateWrdButton();
		MainWndUpdateSoundSpecButton();		
		InitMainMenu(hwnd); 
		InitPanelWnd(hwnd);
		InitCanvasWnd(hwnd);
		InitMainToolbar(hwnd);
		InitSubWndToolbar(hwnd);
		{
			hMainWndScrollbarVolumeTTipWnd = CreateWindow(TOOLTIPS_CLASS,
			NULL, TTS_ALWAYSTIP,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			hMainWndScrollbarVolumeWnd, NULL, hInst, NULL);
			SendMessage(hMainWndScrollbarVolumeTTipWnd, TTM_ACTIVATE, (WPARAM)TRUE, 0);
			hMainWndScrollbarProgressTTipWnd = CreateWindow(TOOLTIPS_CLASS,
			NULL, TTS_ALWAYSTIP,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			hMainWndScrollbarProgressWnd, NULL, hInst, NULL);
			SendMessage(hMainWndScrollbarProgressTTipWnd, TTM_ACTIVATE, (WPARAM)TRUE, 0);

			SBProgressTooltipInfo.cbSize = sizeof(TOOLINFO);
			SBProgressTooltipInfo.uFlags = TTF_SUBCLASS;
			SBProgressTooltipInfo.hwnd = hMainWndScrollbarProgressWnd;
			SBProgressTooltipInfo.lpszText = SBProgressTooltipText;
			GetClientRect(hMainWndScrollbarProgressWnd, &SBProgressTooltipInfo.rect);
			SendMessage(hMainWndScrollbarProgressTTipWnd, TTM_ADDTOOL,
				0, (LPARAM)&SBProgressTooltipInfo);

			SBVolumeTooltipInfo.cbSize = sizeof(TOOLINFO);
			SBVolumeTooltipInfo.uFlags = TTF_SUBCLASS;
			SBVolumeTooltipInfo.hwnd = hMainWndScrollbarVolumeWnd;
			SBVolumeTooltipInfo.lpszText = SBVolumeTooltipText;
			GetClientRect(hMainWndScrollbarVolumeWnd, &SBVolumeTooltipInfo.rect);
			SendMessage(hMainWndScrollbarVolumeTTipWnd, TTM_ADDTOOL,
				0, (LPARAM)&SBVolumeTooltipInfo);
		}
		return FALSE;
	  HANDLE_MSG(hwnd,WM_COMMAND,MainCmdProc);

	  case WM_DESTROY:
		{
			RECT rc;
			WINDOWPLACEMENT wp;
			GetWindowRect(hwnd,&rc);
			GetWindowPlacement(hwnd, &wp);
			if(wp.showCmd == SW_SHOWNORMAL){
				// 最小化状態などで終了すると正常にサイズ取得できない (メニューが含まれないサイズになる
				MainWndInfo.Width = rc.right - rc.left;
				MainWndInfo.Height = rc.bottom - rc.top;
			}
		}
		if(save_playlist_once_before_exit_flag) {
			save_playlist_once_before_exit_flag = 0;
			w32gSaveDefaultPlaylist();
		}
		INISaveMainWnd();
#ifdef VST_LOADER_ENABLE
	if (hVSTHost != NULL) {
		// 再生中終了の場合 まだシンセは動いている
		// vstCloseは時間かかるので先にNULLしておく
		HMODULE htemp = hVSTHost;
		hVSTHost = NULL; // VST使用をブロック
		Sleep(100); // 100msもあればブロック前に開始したVSTの処理は終ってるはず
		((vst_close)GetProcAddress(htemp,"vstClose"))();
		FreeLibrary(htemp);
	}
#endif
		PostQuitMessage(0);
		break;

	  case WM_CLOSE:
		if(save_playlist_once_before_exit_flag) {
			save_playlist_once_before_exit_flag = 0;
			w32gSaveDefaultPlaylist();
		}
		DestroyWindow(hwnd);
		break;
		
	case WM_GETMINMAXINFO:
		{
			LPMINMAXINFO lpmmi = (LPMINMAXINFO) lParam;
			lpmmi->ptMinTrackSize.x = max(345, lpmmi->ptMinTrackSize.x); // full:475
			lpmmi->ptMinTrackSize.y = max(158, lpmmi->ptMinTrackSize.y);
		//	lpmmi->ptMaxTrackSize.y = 158;
		}
		return 0;
      case WM_SIZE:
		switch(wParam){
		case SIZE_RESTORED:
			if ( PrefWndShow ) {
				ShowPrefWnd ();
			}
			if(MainWndInfo.init){
				RECT rc;
				GetWindowRect(hwnd,&rc);
				MainWndInfo.Width = rc.right - rc.left;
				MainWndInfo.Height = rc.bottom - rc.top;
			}
			break;
		case SIZE_MINIMIZED:
			if ( IsVisiblePrefWnd () )
				PrefWndShow = TRUE;
			else
				PrefWndShow = FALSE;
			HidePrefWnd ();
			update_subwindow();
			OnHide();
			break;
		default:
			break;
		}
		return FALSE;
	  case WM_MOVE:
		if ( ! IsIconic(hwnd) ) {
			RECT rc;
			GetWindowRect(hwnd,&rc);
			MainWndInfo.PosX = rc.left;
			MainWndInfo.PosY = rc.top;
		}
		break;
      case WM_QUERYOPEN:
		OnShow();
		return FALSE;
      case WM_DROPFILES:
#ifdef EXT_CONTROL_MAIN_THREAD
		w32g_ext_control_main_thread(RC_EXT_DROP, (ptr_size_t)wParam);
#else
		w32g_send_rc(RC_EXT_DROP, (ptr_size_t)wParam);
#endif
		return FALSE;
      case WM_HSCROLL: {
		int nScrollCode = (int)LOWORD(wParam);
		int nPos = (int) HIWORD(wParam);
		HWND bar = (HWND)lParam;

		if(bar != hMainWndScrollbarProgressWnd)
			break;

		switch(nScrollCode)	{
		case SB_THUMBTRACK:
		case SB_THUMBPOSITION:
			progress_jump = nPos;
			break;
		case SB_LINELEFT:
			progress_jump = GetScrollPos(bar, SB_CTL) - 1;
			if(progress_jump < 0)
				progress_jump = 0;
			SetScrollPos(hMainWndScrollbarProgressWnd, SB_CTL, progress_jump, TRUE);
			break;
		case SB_PAGELEFT:
			progress_jump = GetScrollPos(bar, SB_CTL) - 10;
			if(progress_jump < 0)
				progress_jump = 0;
			SetScrollPos(hMainWndScrollbarProgressWnd, SB_CTL, progress_jump, TRUE);
			break;
		case SB_LINERIGHT:
			progress_jump = GetScrollPos(bar, SB_CTL) + 1;
			SetScrollPos(hMainWndScrollbarProgressWnd, SB_CTL, progress_jump, TRUE);
			break;
		case SB_PAGERIGHT:
			progress_jump = GetScrollPos(bar, SB_CTL) + 10;
			SetScrollPos(hMainWndScrollbarProgressWnd, SB_CTL, progress_jump, TRUE);
			break;
		case SB_ENDSCROLL:
			if(progress_jump != -1)
			{
				w32g_send_rc(RC_JUMP, progress_jump * play_mode->rate);
				SetScrollPos(hMainWndScrollbarProgressWnd, SB_CTL, progress_jump, TRUE);
				progress_jump = -1;

			SendMessage(hMainWndScrollbarProgressTTipWnd, TTM_TRACKACTIVATE,
				FALSE, (LPARAM)&SBProgressTooltipInfo);
			SendMessage(hMainWndScrollbarProgressTTipWnd, TTM_ACTIVATE,
				(WPARAM)FALSE, 0);
			}
			break;
	    }

	    if (nScrollCode != SB_ENDSCROLL && progress_jump != -1)
	    {
		POINT point = { 0, 0 };
		uint32 CurTime_h, CurTime_m, CurTime_s;
		uint32 TotalTime_h, TotalTime_m, TotalTime_s;

		CurTime_s = progress_jump;
		CurTime_m = CurTime_s / 60;
		CurTime_s %= 60;
		CurTime_h = CurTime_m / 60;
		CurTime_m %= 60;

		TotalTime_s = 0;
		GetScrollRange(hMainWndScrollbarProgressWnd, SB_CTL, &TotalTime_m, &TotalTime_s);
		TotalTime_m = TotalTime_s / 60;
		TotalTime_s %= 60;
		TotalTime_h = TotalTime_m / 60;
		TotalTime_m %= 60;

		wsprintf(SBProgressTooltipText, TEXT("%02u:%02u:%02u/%02u:%02u:%02u\0"),
						CurTime_h, CurTime_m, CurTime_s,
						TotalTime_h, TotalTime_m, TotalTime_s);
		SendMessage(hMainWndScrollbarProgressTTipWnd, TTM_UPDATETIPTEXT,
			    0, (LPARAM)&SBProgressTooltipInfo);

		ClientToScreen(hMainWndScrollbarProgressWnd, &point);
		SendMessage(hMainWndScrollbarProgressTTipWnd, TTM_TRACKPOSITION,
			    0, MAKELPARAM(point.x, point.y));

		SendMessage(hMainWndScrollbarProgressTTipWnd, TTM_ACTIVATE,
			    (WPARAM)TRUE, 0);
		SendMessage(hMainWndScrollbarProgressTTipWnd, TTM_TRACKACTIVATE,
			    TRUE, (LPARAM)&SBProgressTooltipInfo);
	    }
	    break;
	  }
	  break;
	  case WM_VSCROLL: {
		int nScrollCode = (int) LOWORD(wParam);
		int nPos = (int) HIWORD(wParam);
		HWND bar = (HWND) lParam;
		static int pos = -1;

		if(bar != hMainWndScrollbarVolumeWnd)
			break;
		switch(nScrollCode)
		{
			case SB_THUMBTRACK:
			case SB_THUMBPOSITION:
			pos = nPos;
			break;
			case SB_LINEUP:
			case SB_PAGEUP:
			pos = GetScrollPos(bar, SB_CTL) - (nScrollCode == SB_LINEUP ? 5 : 20);
			if(pos < 0)
				pos = 0;
			SetScrollPos(bar, SB_CTL, pos, TRUE);
			break;
			case SB_LINEDOWN:
			case SB_PAGEDOWN:
			pos = GetScrollPos(bar, SB_CTL) + (nScrollCode == SB_LINEDOWN ? 5 : 20);
			if(pos > W32G_VOLUME_MAX)
				pos = W32G_VOLUME_MAX;
			SetScrollPos(bar, SB_CTL, pos, TRUE);
			break;
			case SB_ENDSCROLL:
			if(pos != -1)
			{
				w32g_send_rc(RC_CHANGE_VOLUME,
								(W32G_VOLUME_MAX - pos) - output_amplification);
				SetScrollPos(bar, SB_CTL, pos, TRUE);
				pos = -1;

				SendMessage(hMainWndScrollbarVolumeTTipWnd, TTM_TRACKACTIVATE,
					FALSE, (LPARAM)&SBVolumeTooltipInfo);
				SendMessage(hMainWndScrollbarVolumeTTipWnd, TTM_ACTIVATE,
					(WPARAM)FALSE, 0);
			}
			break;
	    }

	    if (nScrollCode != SB_ENDSCROLL && pos != -1)
	    {
		POINT point = { 0, 0 };

		wsprintf(SBVolumeTooltipText, TEXT("%u %%\0"), W32G_VOLUME_MAX - pos);
		SendMessage(hMainWndScrollbarVolumeTTipWnd, TTM_UPDATETIPTEXT,
			    0, (LPARAM)&SBVolumeTooltipInfo);

		ClientToScreen(hMainWndScrollbarVolumeWnd, &point);
		SendMessage(hMainWndScrollbarVolumeTTipWnd, TTM_TRACKPOSITION,
			    0, MAKELPARAM(point.x, point.y));

		SendMessage(hMainWndScrollbarVolumeTTipWnd, TTM_ACTIVATE,
			    (WPARAM)TRUE, 0);
		SendMessage(hMainWndScrollbarVolumeTTipWnd, TTM_TRACKACTIVATE,
			    TRUE, (LPARAM)&SBVolumeTooltipInfo);
	    }
	  }
	  break;
      case WM_MOUSEWHEEL:
      {
		static int16 wheel_delta = 0;
		int16 wheel_speed;
		int pos;

		wheel_delta += (int16)GET_WHEEL_DELTA_WPARAM(wParam);
		wheel_speed = wheel_delta / WHEEL_DELTA; // upper 16bit sined int // 1knoch = 120
		wheel_delta %= WHEEL_DELTA;
		pos = GetScrollPos(hMainWndScrollbarVolumeWnd, SB_CTL) - wheel_speed;
		if(pos < 0)
			pos = 0;
		if(pos > W32G_VOLUME_MAX)
			pos = W32G_VOLUME_MAX;
		w32g_send_rc(RC_CHANGE_VOLUME,
						(W32G_VOLUME_MAX - pos) - output_amplification);
		SetScrollPos(hMainWndScrollbarVolumeWnd, SB_CTL, pos, TRUE);
		break;
      }
	  case WM_APPCOMMAND:
		  switch (GET_APPCOMMAND_LPARAM(lParam)) {
		  case APPCOMMAND_MEDIA_PAUSE:
			  TracerWndDrawSkip = 1;
			  SendDlgItemMessage(hMainWnd, IDC_TOOLBARWINDOW_MAIN,
				  TB_CHECKBUTTON, IDM_PAUSE,
				  (LPARAM)MAKELONG(TRUE, 0));
			  w32g_send_rc(RC_PAUSE, 0);
			  return TRUE;

		  case APPCOMMAND_MEDIA_PLAY:
			  TracerWndDrawSkip = 0;
			  if (play_pause_flag)
			  {
				  SendDlgItemMessage(hMainWnd, IDC_TOOLBARWINDOW_MAIN,
					  TB_CHECKBUTTON, IDM_PAUSE,
					  (LPARAM)MAKELONG(FALSE, 0));
				  w32g_send_rc(RC_TOGGLE_PAUSE, 0);
			  }
			  if (!w32g_play_active)
				  w32g_send_rc(RC_LOAD_FILE, 0);
			  return TRUE;

		  case APPCOMMAND_MEDIA_PLAY_PAUSE:
			  if (!w32g_play_active) {
				  TracerWndDrawSkip = 0;
				  w32g_send_rc(RC_LOAD_FILE, 0);
			  } else {
				  TracerWndDrawSkip = !TracerWndDrawSkip;
				  SendDlgItemMessage(hMainWnd, IDC_TOOLBARWINDOW_MAIN,
					  TB_CHECKBUTTON, IDM_PAUSE,
					  (LPARAM)MAKELONG(!play_pause_flag, 0));
				  w32g_send_rc(RC_TOGGLE_PAUSE, 0);
			  }
			  return TRUE;

		  case APPCOMMAND_MEDIA_STOP:
			  TracerWndDrawSkip = 1;
			  w32g_send_rc(RC_STOP, 0);
			  return TRUE;

		  case APPCOMMAND_MEDIA_PREVIOUSTRACK:
			  TracerWndDrawSkip = 1;
			  w32g_send_rc(RC_REALLY_PREVIOUS, 0);
			  return TRUE;

		  case APPCOMMAND_MEDIA_NEXTTRACK:
			  TracerWndDrawSkip = 1;
			  w32g_send_rc(RC_NEXT, 0);
			  return TRUE;

		  default:
			  break;
		  }
		  return FALSE;

	  case WM_SYSCOMMAND:
		switch(wParam){
		  case IDM_STOP:
		  case IDM_PAUSE:
		  case IDM_PREV:
		  case IDM_PLAY:
		  case IDM_NEXT:
		  case IDM_FOREWARD:
		  case IDM_BACKWARD:
			SendMessage(hwnd,WM_COMMAND,wParam,(LPARAM)NULL);
			break;
		  default:
			break;
		}
		return FALSE;
      case WM_NOTIFY:
		switch(wParam) {
		  case IDC_TOOLBARWINDOW_MAIN:{
			  LPTBNOTIFY TbNotify = (LPTBNOTIFY)lParam;
			  switch(TbNotify->iItem){
				case IDM_BACKWARD:
				  if(TbNotify->hdr.code==TBN_BEGINDRAG){
#ifdef W32GUI_DEBUG
					  //PrintfDebugWnd("IDM_BACKWARD: BUTTON ON\n");
#endif
					  PlayerBackward(0,0,0,0,0);
					  PlayerForwardAndBackwardEventID =
						  timeSetEvent(100, 100, PlayerBackward, 0, TIME_PERIODIC);
				  }
				  if(PlayerForwardAndBackwardEventID != 0)
					  if(TbNotify->hdr.code==TBN_ENDDRAG){
#ifdef W32GUI_DEBUG
						  //PrintfDebugWnd("IDM_BACKWARD: BUTTON OFF\n");
#endif
						  timeKillEvent(PlayerForwardAndBackwardEventID);
						  PlayerForwardAndBackwardEventID = 0;
					  }
				  break;
				case IDM_FOREWARD:
				  if(TbNotify->hdr.code ==TBN_BEGINDRAG &&
					 PlayerForwardAndBackwardEventID == 0){
#ifdef W32GUI_DEBUG
					  //PrintfDebugWnd("IDM_FOREWARD: BUTTON ON\n");
#endif
					  PlayerForward(0,0,0,0,0);
					  PlayerForwardAndBackwardEventID =
						  timeSetEvent(100,300,PlayerForward,0,TIME_PERIODIC);
				  }
				  else if((TbNotify->hdr.code == TBN_ENDDRAG ||
						   TbNotify->hdr.code == NM_CLICK ||
						   TbNotify->hdr.code == NM_RCLICK) &&
						  PlayerForwardAndBackwardEventID != 0)
				  {
#ifdef W32GUI_DEBUG
					  //PrintfDebugWnd("IDM_FOREWARD: BUTTON OFF\n");
#endif
					  timeKillEvent(PlayerForwardAndBackwardEventID);
					  PlayerForwardAndBackwardEventID = 0;
				  }
				  break;
				default:
				  break;
			  }
		    }
			break; /* end of case IDC_TOOLBARWINDOW_MAIN */
		  default:
			break;
		}
		return FALSE;
	case WM_SHOWWINDOW:
	{
		BOOL fShow = (BOOL)wParam;
		if ( fShow ) {
			if ( PrefWndShow ) {
				ShowPrefWnd ();
			} else {
				HidePrefWnd ();
			}
		} else {
			if ( IsVisiblePrefWnd () )
				PrefWndShow = TRUE;
			else
				PrefWndShow = FALSE;
			HidePrefWnd ();
		}
		break;
	}
		break;
	case WM_UPDATE_SCROLLBAR_PROGRESS:
	{
		int sec = (int)lParam;

		if (sec == -1)
		{
			EnableWindow(hMainWndScrollbarProgressWnd, FALSE);
			progress_jump = -1;
		}
		else
		{
			EnableWindow(hMainWndScrollbarProgressWnd, TRUE);
			if (progress_jump == -1)
				SetScrollPos(hMainWndScrollbarProgressWnd, SB_CTL, sec, TRUE);
		}
		return TRUE;
	}

      default:
		  if (uMess == RegisterWindowMessage(_T("TaskbarCreated"))) {
			ShowWindow(hMainWnd, SW_HIDE);
			ShowWindow(hMainWnd, SW_SHOWNOACTIVATE);
			return 0;
		}
		return FALSE;
    }
	return FALSE;
}

static void MainWndItemResize(void)
{
    RECT rc;
    HWND hwnd = hMainWnd;
    HWND hPanel = GetDlgItem(hwnd, IDC_RECT_PANEL);
    HWND hCanvas = GetDlgItem(hwnd, IDC_RECT_CANVAS);
    HWND hProgress = hMainWndScrollbarProgressWnd;
    HWND hVolume = hMainWndScrollbarVolumeWnd;
    int w;
    int vx, vw;
    int cx, cw;
    int px, pw;
    GetClientRect(hwnd, &rc);
    w = rc.right - rc.left;

    GetWindowRect(hVolume, &rc);
    vw = rc.right - rc.left;
    vx = w - vw;
    SetWindowPos(hVolume, 0, vx, 0, rc.right - rc.left, rc.bottom - rc.top, 0);

    GetWindowRect(hCanvas, &rc);
    cw = rc.right - rc.left;
    cx = w - vw - cw;
    MoveWindow(hCanvas, cx, 0, rc.right - rc.left, rc.bottom - rc.top, FALSE);

    GetWindowRect(hPanel, &rc);
    MoveWindow(hPanel, 0, 0, w - vw - cw, rc.bottom - rc.top, FALSE);

    GetWindowRect(hProgress, &rc);
    ScreenToClient(hwnd, (LPPOINT) &rc);
    ScreenToClient(hwnd, (LPPOINT) &rc + 1);
    px = rc.left;
    pw = w - px - (rc.bottom - rc.top);
    SetWindowPos(hProgress, 0, px, rc.top, pw, rc.bottom - rc.top, 0);

    InvalidateRect(hwnd, 0, TRUE);
    MPanelResize();
    MPanelReadPanelInfo(1);
    MPanelUpdateAll();
    MPanelPaintAll();
    CanvasPaintAll();
}

void PrefWndCreate(HWND hwnd, UINT cid);

void MainCmdProc(HWND hwnd, int wId, HWND hwndCtl, UINT wNotifyCode)
{
	 // PrintfDebugWnd("WM_COMMAND: ID%lx HWND%lx CODE%lx\n",wId,hwndCtl,wNotifyCode);
    switch(wId)
    {
      case IDM_STOP:
			TracerWndDrawSkip = 1;
		  w32g_send_rc(RC_STOP, 0);
		  break;
      case IDM_PAUSE:
			TracerWndDrawSkip = !TracerWndDrawSkip;
		  SendDlgItemMessage(hMainWnd, IDC_TOOLBARWINDOW_MAIN,
									TB_CHECKBUTTON, IDM_PAUSE,
									(LPARAM)MAKELONG(!play_pause_flag, 0));
		  w32g_send_rc(RC_TOGGLE_PAUSE, 0);
		  break;
      case IDM_PREV:
			TracerWndDrawSkip = 1;
		  w32g_send_rc(RC_REALLY_PREVIOUS, 0);
		  break;
      case IDM_BACKWARD:
		  /* Do nothing here. See WM_NOTIFY in MainProc() */
		  break;
      case IDM_PLAY:
			TracerWndDrawSkip = 0;
		  if(play_pause_flag)
		  {
				SendDlgItemMessage(hMainWnd, IDC_TOOLBARWINDOW_MAIN,
										 TB_CHECKBUTTON, IDM_PAUSE,
										 (LPARAM)MAKELONG(FALSE, 0));
				w32g_send_rc(RC_TOGGLE_PAUSE, 0);
		  }
		  if(!w32g_play_active)
				w32g_send_rc(RC_LOAD_FILE, 0);
		  break;
      case IDM_FOREWARD:
		  /* Do nothing here. See WM_NOTIFY in MainProc() */
		  break;
      case IDM_NEXT:
			TracerWndDrawSkip = 1;
		  w32g_send_rc(RC_NEXT, 0);
		  break;
      case IDM_CONSOLE:
      case IDM_MWCONSOLE:
		  ToggleSubWindow(hConsoleWnd);
		  break;
      case IDM_TRACER:
      case IDM_MWTRACER:
		  ToggleSubWindow(hTracerWnd);
//		  MainWndUpdateTracerButton();
//		  MessageBox(hwnd, "Not Supported.","Warning!",MB_OK);
		  break;
      case IDM_LIST:
      case IDM_MWPLAYLIST:
		  ToggleSubWindow(hListWnd);
		  if(IsWindowVisible(hListWnd))
#ifdef EXT_CONTROL_MAIN_THREAD
			w32g_ext_control_main_thread(RC_EXT_UPDATE_PLAYLIST, 0);
#else
			w32g_send_rc(RC_EXT_UPDATE_PLAYLIST, 0);
#endif
		  break;
      case IDM_DOC:
      case IDM_MWDOCUMENT:
		  ToggleSubWindow(hDocWnd);
//		  if(IsWindowVisible(hDocWnd))
//			  w32g_send_rc(RC_EXT_DOC, 0);
		  break;
      case IDM_WRD:
      case IDM_MWWRDTRACER:
		  ToggleSubWindow(hWrdWnd);
//		  MainWndUpdateWrdButton();
//		  MessageBox(hwnd, "Not Supported.","Warning!",MB_OK);
		  break;
      case IDM_SOUNDSPEC:
      case IDM_MWSOUNDSPEC:
		  ToggleSubWindow(hSoundSpecWnd);
//		  MainWndUpdateSoundSpecButton();
//		  MessageBox(hwnd, "Not Supported.","Warning!",MB_OK);
		  break;
///r
      case IDM_VSTMGR:
      case IDM_MWVSTMGR:
#ifdef VST_LOADER_ENABLE
	if (!hVSTHost) {
	    w32_reset_dll_directory();
#ifdef _WIN64
	    hVSTHost = LoadLibrary(TEXT("timvstwrap_x64.dll"));
#else
	    hVSTHost = LoadLibrary(TEXT("timvstwrap.dll"));
#endif
	    if (hVSTHost && GetProcAddress(hVSTHost, "vstOpen")) {
		((vst_open) GetProcAddress(hVSTHost, "vstOpen"))();
	    }
	}

	if (hVSTHost) {
	    ((open_vst_mgr) GetProcAddress(hVSTHost, "openVSTManager"))(hwnd);
	}
	else if (hVSTHost) {
#ifdef _WIN64
	    const TCHAR *vst_nosupport,
			 vst_nosupport_en[] = TEXT("openVSTManager could not be found in timvstwrap_x64.dll"),
			 vst_nosupport_jp[] = TEXT("openVSTManager が timvstwrap_x64.dll から見つかりませんでした。");
#else
	    const TCHAR *vst_nosupport,
			 vst_nosupport_en[] = TEXT("openVSTManager could not be found in timvstwrap.dll"),
			 vst_nosupport_jp[] = TEXT("openVSTManager が timvstwrap.dll から見つかりませんでした。");
#endif
	    if (PlayerLanguage == LANGUAGE_JAPANESE)
		vst_nosupport = vst_nosupport_jp;
	    else
		vst_nosupport = vst_nosupport_en;
	    MessageBox(hwnd, vst_nosupport, TEXT("TiMidity Warning"), MB_OK | MB_ICONWARNING);
	}
	else {
#ifdef _WIN64
	    const TCHAR *vst_nosupport,
			 vst_nosupport_en[] = TEXT("Cannot load timvstwrap_x64.dll"),
			 vst_nosupport_jp[] = TEXT("timvstwrap_x64.dll をロードしていません。");
#else
	    const TCHAR *vst_nosupport,
			 vst_nosupport_en[] = TEXT("Cannot load timvstwrap.dll"),
			 vst_nosupport_jp[] = TEXT("timvstwrap.dll をロードしていません。");
#endif
	    if (PlayerLanguage == LANGUAGE_JAPANESE)
		vst_nosupport = vst_nosupport_jp;
	    else
		vst_nosupport = vst_nosupport_en;
	    MessageBox(hwnd, vst_nosupport, TEXT("TiMidity Warning"), MB_OK | MB_ICONWARNING);
	}
#endif
		  break;
	  case IDM_MWISEDITOR:
#ifdef INT_SYNTH
		  ISEditorWndCreate(hwnd);	
#endif
		  break;
      case IDOK:
		  break;
      case IDCANCEL:
		  OnQuit();
		  break;
      case IDM_MFOPENFILE:
		  DlgMidiFileOpen(hwnd);
		  break;
      case IDM_MFOPENDIR:
		  DlgDirOpen(hwnd);
		  break;
      case IDM_MFOPENURL:
		  DlgUrlOpen(hwnd);
		  break;
      case IDM_MFLOADPLAYLIST:
		  DlgPlaylistOpen(hwnd);
		  break;
      case IDM_MFSAVEPLAYLISTAS:
		  DlgPlaylistSave(hwnd);
		  break;
      case IDM_MFRESTART:
		  RestartTimidity = 1; // WinMain()
		  // thru exit
      case IDM_MFEXIT:
		  OnQuit();
		  break;

      case IDM_SETTING:
		  CallPrefWnd(0);
		  break;

      case IDM_MCSAVEINIFILE:
		  VOLATILE_TOUCH(PrefWndDoing);
		  if(PrefWndDoing){
			  MessageBox(hMainWnd, _T("Can't Save Ini file while preference dialog."),
				  _T("Warning"), MB_OK);
				break;
		  }
		  SaveIniFile(sp_current, st_current);
		  break;
      case IDM_MCLOADINIFILE:
		 if(!w32g_has_ini_file) {
			 MessageBox(hMainWnd, _T("Can't load Ini file."),
				 _T("Warning"), MB_OK);
		     break;
		 }

		  VOLATILE_TOUCH(PrefWndDoing);
		  if(PrefWndDoing){
			  MessageBox(hMainWnd, _T("Can't load Ini file while preference dialog."),
				  _T("Warning"), MB_OK);
				break;
		  }
		  LoadIniFile(sp_temp,st_temp);
		  CallPrefWnd(0);
		  break;
      case IDM_MWDEBUG:
#ifdef W32GUI_DEBUG
		  if(IsWindowVisible(hDebugWnd))
				ShowWindow(hDebugWnd,SW_HIDE);
		  else
				ShowWindow(hDebugWnd,SW_SHOW);
#endif
		  break;
      case IDM_MHTOPIC:
		  MessageBox(hwnd, _T("Not Supported."), _T("Warning!"), MB_OK);
		  break;
      case IDM_MHHELP:
		  MessageBox(hwnd, _T("Not Supported."), _T("Warning!"), MB_OK);
		  break;
      case IDM_MHONLINEHELP:
		  if (PlayerLanguage == LANGUAGE_JAPANESE) {
			  ShellExecute(HWND_DESKTOP, NULL, _T("http://timidity-docs.sourceforge.jp/"), NULL, NULL, SW_SHOWNORMAL);
		  } else {
			  ShellExecute(HWND_DESKTOP, NULL, _T("http://timidity.sourceforge.net/index.html.en"), NULL, NULL, SW_SHOWNORMAL);
		  }
		  break;
      case IDM_MHBTS:
		  if (PlayerLanguage == LANGUAGE_JAPANESE) {
			  ShellExecute(HWND_DESKTOP, NULL, _T("http://timidity-docs.sourceforge.jp/cgi-bin/kagemai-ja/guest.cgi"), NULL, NULL, SW_SHOWNORMAL);
		  } else {
			  ShellExecute(HWND_DESKTOP, NULL, _T("http://timidity-docs.sourceforge.jp/cgi-bin/kagemai-en/guest.cgi"), NULL, NULL, SW_SHOWNORMAL);
		  }
		  break;
      case IDM_MHONLINEHELPCFG:
		  ShellExecute(HWND_DESKTOP, NULL, _T("http://timidity-docs.sourceforge.jp/cgi-bin/hiki/hiki.cgi?%28ja%29timidity.cfg"), NULL, NULL, SW_SHOWNORMAL);
		  break;
      case IDM_MHVERSION:
		  VersionWnd(hwnd);
		  break;
      case IDM_MHTIMIDITY:
		  TiMidityWnd(hwnd);
		  break;
      case IDM_FORCE_RELOAD:
		  if (!w32g_play_active) {reload_cfg();}
		  break;
      case IDM_MHSUPPLEMENT:
		  SupplementWnd(hwnd);
		  break;
	  default:
///r
	// sort large ID
	if (IDM_MODULE <= wId)
	    UpdateModuleMenu(hwnd, wId);
	else if (IDM_OUTPUT_OPTIONS == wId) {
	    UINT id;
	    if (PlayerLanguage == LANGUAGE_JAPANESE) {
		id = IDD_PREF_TIMIDITY3;
	    }
	    else {
		id = IDD_PREF_TIMIDITY3_EN;
	    }
	    CallPrefWnd(id);
	}
	else if (IDM_OUTPUT <= wId)
	    UpdateOutputMenu(hwnd, wId);
	break;
    }
}

static void CallPrefWnd(UINT_PTR cId)
{
    PrefWndCreate(hMainWnd, cId);

    MPanelReadPanelInfo(1);
    MPanelUpdateAll();
    MPanelPaintAll();

    SetScrollPos(hMainWndScrollbarVolumeWnd, SB_CTL,
		 W32G_VOLUME_MAX - output_amplification, TRUE);

    RefreshOutputMenu(hMainWnd);
    RefreshModuleMenu(hMainWnd);
}


void update_subwindow(void)
{
  SUBWINDOW *s = subwindow;
  int i;
  for(i=0;s[i].hwnd!=NULL;i++){
		if(IsWindow(*(s[i].hwnd)))
  		s[i].status |= SWS_EXIST;
		else {
  		s[i].status = 0;
    	continue;
    }
		if(IsIconic(*(s[i].hwnd)))
  		s[i].status |= SWS_ICON;
		else
  		s[i].status &= ~ SWS_ICON;
		if(IsWindowVisible(*(s[i].hwnd)))
  		s[i].status &= ~ SWS_HIDE;
		else
  		s[i].status |= SWS_HIDE;
	}
}

void MainWndSetPauseButton(int flag)
{
	if(flag)
  	SendDlgItemMessage(hMainWnd, IDC_TOOLBARWINDOW_MAIN,
    	TB_CHECKBUTTON, IDM_PAUSE, (LPARAM)MAKELONG(TRUE, 0));
	else
  	SendDlgItemMessage(hMainWnd, IDC_TOOLBARWINDOW_MAIN,
    	TB_CHECKBUTTON, IDM_PAUSE, (LPARAM)MAKELONG(FALSE, 0));
}

void MainWndSetPlayButton(int flag)
{
	return;
}

void MainWndUpdateConsoleButton(void)
{
	if(IsWindowVisible(hConsoleWnd))
  	SendDlgItemMessage(hMainWnd, IDC_TOOLBARWINDOW_SUBWND,
    	TB_CHECKBUTTON, IDM_CONSOLE, (LPARAM)MAKELONG(TRUE, 0));
	else
  	SendDlgItemMessage(hMainWnd, IDC_TOOLBARWINDOW_SUBWND,
    	TB_CHECKBUTTON, IDM_CONSOLE, (LPARAM)MAKELONG(FALSE, 0));
}

void MainWndUpdateListButton(void)
{
	if(IsWindowVisible(hListWnd))
  	SendDlgItemMessage(hMainWnd, IDC_TOOLBARWINDOW_SUBWND,
    	TB_CHECKBUTTON, IDM_LIST, (LPARAM)MAKELONG(TRUE, 0));
	else
  	SendDlgItemMessage(hMainWnd, IDC_TOOLBARWINDOW_SUBWND,
    	TB_CHECKBUTTON, IDM_LIST, (LPARAM)MAKELONG(FALSE, 0));
}

void MainWndUpdateDocButton(void)
{
	if(IsWindowVisible(hDocWnd))
  	SendDlgItemMessage(hMainWnd, IDC_TOOLBARWINDOW_SUBWND,
    	TB_CHECKBUTTON, IDM_DOC, (LPARAM)MAKELONG(TRUE, 0));
	else
  	SendDlgItemMessage(hMainWnd, IDC_TOOLBARWINDOW_SUBWND,
    	TB_CHECKBUTTON, IDM_DOC, (LPARAM)MAKELONG(FALSE, 0));
}

void MainWndUpdateTracerButton(void)
{
	if(IsWindowVisible(hTracerWnd))
		SendDlgItemMessage(hMainWnd, IDC_TOOLBARWINDOW_SUBWND,
			TB_CHECKBUTTON, IDM_TRACER,
			(LPARAM)MAKELONG(TRUE, 0));
   else
  		SendDlgItemMessage(hMainWnd, IDC_TOOLBARWINDOW_SUBWND,
			TB_CHECKBUTTON, IDM_TRACER,
			(LPARAM)MAKELONG(FALSE, 0));
}

void MainWndUpdateWrdButton(void)
{
	if(IsWindowVisible(hWrdWnd))
  	SendDlgItemMessage(hMainWnd, IDC_TOOLBARWINDOW_SUBWND,
    	TB_CHECKBUTTON, IDM_WRD, (LPARAM)MAKELONG(TRUE, 0));
	else
  	SendDlgItemMessage(hMainWnd, IDC_TOOLBARWINDOW_SUBWND,
    	TB_CHECKBUTTON, IDM_WRD, (LPARAM)MAKELONG(FALSE, 0));
}


void MainWndUpdateSoundSpecButton(void)
{
	if(IsWindowVisible(hSoundSpecWnd))
		SendDlgItemMessage(hMainWnd, IDC_TOOLBARWINDOW_SUBWND,
			TB_CHECKBUTTON, IDM_SOUNDSPEC,
			(LPARAM)MAKELONG(TRUE, 0));
	else
  		SendDlgItemMessage(hMainWnd, IDC_TOOLBARWINDOW_SUBWND,
			TB_CHECKBUTTON, IDM_SOUNDSPEC,
			(LPARAM)MAKELONG(FALSE, 0));
}

#undef SUBWINDOW_POS_IS_OLD_CLOSED_WINDOW
void ShowSubWindow(HWND hwnd,int showflag)
{
	int i, num = 0;
	RECT rc;
#ifdef SUBWINDOW_POS_IS_OLD_CLOSED_WINDOW
	RECT rc2;
#endif
	int max = 0;
	if(showflag){
		if(IsWindowVisible(hwnd))
  		return;
		for(i=0;SubWindowHistory[i].hwnd!=NULL;i++)
  		if(*(SubWindowHistory[i].hwnd)==hwnd)
      	num = i;
		for(i=0;SubWindowHistory[i].hwnd!=NULL;i++)
  		if(*(SubWindowHistory[i].hwnd)!=hwnd){
	  		if(SubWindowHistory[i].status > 0)
					SubWindowHistory[i].status += 1;
	  		if(SubWindowHistory[i].status>SubWindowMax){
					if(SubWindowHistory[i].status>max){
    				GetWindowRect(*(SubWindowHistory[i].hwnd), &rc);
						max = SubWindowHistory[i].status;
					}
      		ShowWindow(*(SubWindowHistory[i].hwnd),SW_HIDE);
					SubWindowHistory[i].status = 0;
				}
			}
#ifdef SUBWINDOW_POS_IS_OLD_CLOSED_WINDOW
		// サブウインドウを最大数を越えて閉じられる古いウインドウに合わせる仕様は止めることにした。
		if(max>0){
			GetWindowRect(hwnd, &rc2);
			MoveWindow(hwnd,rc.left,rc.top,rc2.right-rc2.left,rc2.bottom-rc2.top,TRUE);
		}
#endif
		ShowWindow(hwnd,SW_SHOW);
		SubWindowHistory[num].status = 1;
	} else {
		if(!IsWindowVisible(hwnd))
  		return;
		for(i=0;SubWindowHistory[i].hwnd!=NULL;i++)
  		if(*(SubWindowHistory[i].hwnd)==hwnd)
      	num = i;
		for(i=0;SubWindowHistory[i].hwnd!=NULL;i++)
    	if(i!=num)
	  		if(SubWindowHistory[i].status>=SubWindowHistory[num].status)
					SubWindowHistory[i].status -= 1;
    ShowWindow(hwnd,SW_HIDE);
		SubWindowHistory[num].status = 0;
	}
	MainWndUpdateConsoleButton();
	MainWndUpdateListButton();
	MainWndUpdateTracerButton();
	MainWndUpdateDocButton();
	MainWndUpdateWrdButton();
	MainWndUpdateSoundSpecButton();
}

void ToggleSubWindow(HWND hwnd)
{
	if(IsWindowVisible(hwnd))
		ShowSubWindow(hwnd,0);
  else
		ShowSubWindow(hwnd,1);
}

void OnShow(void)
{
  SUBWINDOW *s = subwindow;
	int i;
  for(i=0;s[i].hwnd!=NULL;i++)
		if(s[i].status & SWS_EXIST) {
			if(s[i].status & SWS_HIDE)
    		ShowWindow(*(s[i].hwnd),SW_HIDE);
			else
    		ShowWindow(*(s[i].hwnd),SW_SHOW);
		}
}

void OnHide(void)
{
  SUBWINDOW *s = subwindow;
	int i;
  for(i=0;s[i].hwnd!=NULL;i++){
		if(s[i].status & SWS_EXIST)
    	ShowWindow(*(s[i].hwnd),SW_HIDE);
  }
}

#ifdef W32GUI_DEBUG
void WINAPI DebugThread(void *args)
{
	MSG msg;
	DebugThreadExit = 0;
//	InitDebugWnd(NULL);
	ShowWindow(hDebugWnd,SW_SHOW);
	AttachThreadInput(GetWindowThreadProcessId(hDebugThread,NULL),
   	GetWindowThreadProcessId(hWindowThread,NULL),TRUE);
	AttachThreadInput(GetWindowThreadProcessId(hWindowThread,NULL),
   	GetWindowThreadProcessId(hDebugThread,NULL),TRUE);
	while( GetMessage(&msg,NULL,0,0) ){
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	DebugThreadExit = 1;
	crt_endthread();
}

void DebugThreadInit(void)
{
	DWORD dwThreadID;
	if(!DebugThreadExit)
   	return;
	hDebugThread = crt_beginthreadex(NULL,0,(LPTHREAD_START_ROUTINE)DebugThread,0,0,&dwThreadID);
	set_thread_description((ptr_size_t)hDebugThread, "W32G Debug Thread");
}
#endif





//-----------------------------------------------------------------------------
// Toolbar Main

static TBBUTTON MainTbb[] = {
    {4, IDM_STOP, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0},
    {3, IDM_PAUSE, TBSTATE_ENABLED, TBSTYLE_CHECK, 0, 0},
    {0, IDM_PREV, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0},
    {1, IDM_BACKWARD, TBSTATE_ENABLED,TBSTYLE_BUTTON, 0, 0},
    {2, IDM_PLAY, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0},
    {5, IDM_FOREWARD, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0},
    {6, IDM_NEXT, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0}
};

static void InitMainToolbar(HWND hwnd)
{
	TBADDBITMAP MainTbab;
  SendDlgItemMessage(hwnd, IDC_TOOLBARWINDOW_MAIN,
  		TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
  SendDlgItemMessage(hwnd, IDC_TOOLBARWINDOW_MAIN,
  		TB_SETBUTTONSIZE, (WPARAM)0, (LPARAM)MAKELONG(16,16));
	MainTbab.hInst = hInst;
	MainTbab.nID =(int)IDB_BITMAP_MAIN_BUTTON;
  SendDlgItemMessage(hwnd, IDC_TOOLBARWINDOW_MAIN,
  	TB_ADDBITMAP, 7, (LPARAM)&MainTbab);
  SendDlgItemMessage(hwnd, IDC_TOOLBARWINDOW_MAIN,
  	TB_ADDBUTTONS, (WPARAM)7,(LPARAM)&MainTbb);
  SendDlgItemMessage(hwnd, IDC_TOOLBARWINDOW_MAIN,
		TB_AUTOSIZE, 0, 0);
}

//-----------------------------------------------------------------------------
// Toolbar SubWnd

#define SUBWNDTOOLBAR_BITMAPITEMS 7
static TBBUTTON SubWndTbb[] = {
    { 3, IDM_CONSOLE, TBSTATE_ENABLED, TBSTYLE_CHECK, 0, 0 },
    { 1, IDM_LIST, TBSTATE_ENABLED, TBSTYLE_CHECK, 0, 0 },
    { 2, IDM_TRACER, TBSTATE_ENABLED, TBSTYLE_CHECK, 0, 0 },
    { 0, IDM_DOC, TBSTATE_ENABLED, TBSTYLE_CHECK, 0, 0 },
    { 4, IDM_WRD, TBSTATE_ENABLED, TBSTYLE_CHECK, 0, 0 },
    { 5, IDM_VSTMGR,
#ifdef VST_LOADER_ENABLE
	 TBSTATE_ENABLED,
#else
	 0, /* disabled */
#endif
	 TBSTYLE_BUTTON, 0, 0 },
    { 6, IDM_SOUNDSPEC,
#ifdef SUPPORT_SOUNDSPEC
	 TBSTATE_ENABLED,
#else
	 0, /* disabled */
#endif
	 TBSTYLE_CHECK, 0, 0 },
};

static void InitSubWndToolbar(HWND hwnd)
{
	TBADDBITMAP SubWndTbab;
  SendDlgItemMessage(hwnd, IDC_TOOLBARWINDOW_SUBWND,
  		TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
  SendDlgItemMessage(hwnd, IDC_TOOLBARWINDOW_SUBWND,
  		TB_SETBUTTONSIZE, (WPARAM)0, (LPARAM)MAKELONG(16,16));
  SendDlgItemMessage(hwnd, IDC_TOOLBARWINDOW_SUBWND,
  	TB_ADDBUTTONS, (WPARAM)sizeof(SubWndTbb) / sizeof(TBBUTTON),(LPARAM)&SubWndTbb);
	SubWndTbab.hInst = hInst;
	SubWndTbab.nID =(int)IDB_BITMAP_SUBWND_BUTTON;
  SendDlgItemMessage(hwnd, IDC_TOOLBARWINDOW_SUBWND,
  	TB_ADDBITMAP, (WPARAM)SUBWNDTOOLBAR_BITMAPITEMS, (LPARAM)&SubWndTbab);
  SendDlgItemMessage(hwnd, IDC_TOOLBARWINDOW_SUBWND,
		TB_AUTOSIZE, 0, 0);
}


//-----------------------------------------------------------------------------
// Canvas Window

#define MAPBAR_LIKE_TMIDI	// note bar view like tmidi
#define TM_CANVAS_XMAX 160
#define TM_CANVAS_YMAX 160
#define TM_CANVASMAP_XMAX 16
#define TM_CANVASMAP_YMAX 16
#define CANVAS_XMAX 160
#define CANVAS_YMAX 160
#define CMAP_XMAX 16
#define CMAP_YMAX 16
#define CK_MAX_CHANNELS 16
#define CMAP_MODE_16		1
#define CMAP_MODE_32		2
#define CMAP_MODE_64		3
#define CMAP_MAX_CHANNELS	64
struct Canvas_ {
	HWND hwnd;
	HWND hParentWnd;
	RECT rcMe;
	int Mode;
	HDC hdc;
	HDC hmdc;
	HGDIOBJ hgdiobj_hmdcprev;
	int UpdateAll;
	int PaintDone;
	HANDLE hPopupMenu;
	HANDLE hPopupMenuKeyboard;
// Sleep mode
	RECT rcSleep;
	HBITMAP hbitmap;
	HBITMAP hBitmapSleep;
	int SleepUpdateFlag;
// Map mode
	RECT rcMap;
	char MapMap[CMAP_MAX_CHANNELS][CMAP_YMAX];
	char MapMapOld[CMAP_MAX_CHANNELS][CMAP_YMAX];
	char MapBar[CMAP_MAX_CHANNELS];
	char MapBarOld[CMAP_MAX_CHANNELS];
	int MapDelay;
	int MapResidual;
	int MapUpdateFlag;
	int MapMode;
	int MapBarWidth;
	int MapBarMax;
   int MapCh;
   int MapPan[CMAP_MAX_CHANNELS];
   int MapPanOld[CMAP_MAX_CHANNELS];
   int MapSustain[CMAP_MAX_CHANNELS];
   int MapSustainOld[CMAP_MAX_CHANNELS];
   int MapExpression[CMAP_MAX_CHANNELS];
   int MapExpressionOld[CMAP_MAX_CHANNELS];
   int MapVolume[CMAP_MAX_CHANNELS];
   int MapVolumeOld[CMAP_MAX_CHANNELS];
   int MapPitchbend[CMAP_MAX_CHANNELS];
   int MapPitchbendOld[CMAP_MAX_CHANNELS];
   int MapChChanged;
	ChannelBitMask DrumChannel;
   RECT rcMapMap;
   RECT rcMapSub;
// Keyboard mode
	RECT rcKeyboard;
	uint32 CKxnote[MAX_W32G_MIDI_CHANNELS][4];
	uint32 CKxnote_old[MAX_W32G_MIDI_CHANNELS][4];
	int CKNoteFrom;
   int CKNoteTo;
	int CKCh;
	int CKPart;
	RECT rcGSLCD;
	int8 GSLCD[16][16];
	int8 GSLCD_old[16][16];
	int KeyboardUpdateFlag;
  	int xnote_reset;
// misc
   Channel channel[MAX_W32G_MIDI_CHANNELS];
} Canvas;

#define IDC_CANVAS 4242

static HWND hCanvasWnd;
static TCHAR CanvasWndClassName[] = _T("TiMidity Canvas");
static LRESULT CALLBACK CanvasWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static void CanvasPaintDo(void);

#define IDM_CANVAS_SLEEP		2321
#define IDM_CANVAS_MAP        2322
#define IDM_CANVAS_KEYBOARD   2323
#define IDM_CANVAS_REDRAW     2324
#define IDM_CANVAS_MAP16      2325
#define IDM_CANVAS_MAP32      2326
#define IDM_CANVAS_MAP64      2327
#define IDM_CANVAS_KEYBOARD_A   2328
#define IDM_CANVAS_KEYBOARD_B   2329
#define IDM_CANVAS_KEYBOARD_C   2330
#define IDM_CANVAS_KEYBOARD_D   2331
#define IDM_CANVAS_GSLCD   2332
static void InitCanvasWnd(HWND hwnd)
{
	WNDCLASS wndclass ;
	hCanvasWnd = 0;
	wndclass.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS | CS_CLASSDC;
	wndclass.lpfnWndProc   = CanvasWndProc ;
	wndclass.cbClsExtra    = 0 ;
	wndclass.cbWndExtra    = 0 ;
	wndclass.hInstance     = hInst ;
	wndclass.hIcon         = NULL;
	wndclass.hCursor       = LoadCursor(0,IDC_ARROW) ;
	wndclass.hbrBackground = (HBRUSH)(COLOR_SCROLLBAR + 1);
	wndclass.lpszMenuName  = NULL;
	wndclass.lpszClassName = CanvasWndClassName;
	RegisterClass(&wndclass);
   hCanvasWnd = CreateWindowEx(0,CanvasWndClassName,0,WS_CHILD,CW_USEDEFAULT,
   	0,CANVAS_XMAX,CANVAS_YMAX,GetDlgItem(hwnd,IDC_RECT_CANVAS),0,hInst,0);
	CanvasInit(hCanvasWnd);
	CanvasReset();
	CanvasClear();
	CanvasReadPanelInfo(1);
	CanvasUpdate(1);
	CanvasPaintAll();
	ShowWindow(hCanvasWnd,SW_SHOW);
	UpdateWindow(hCanvasWnd);
}

static LRESULT CALLBACK
CanvasWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess)
	{
		case WM_CREATE:
			Canvas.hPopupMenuKeyboard = CreatePopupMenu();
			AppendMenu(Canvas.hPopupMenuKeyboard,MF_STRING,IDM_CANVAS_KEYBOARD_A,_T("A Part"));
			AppendMenu(Canvas.hPopupMenuKeyboard,MF_STRING,IDM_CANVAS_KEYBOARD_B,_T("B Part"));
			AppendMenu(Canvas.hPopupMenuKeyboard,MF_STRING,IDM_CANVAS_KEYBOARD_C,_T("C Part"));
			AppendMenu(Canvas.hPopupMenuKeyboard,MF_STRING,IDM_CANVAS_KEYBOARD_D,_T("D Part"));
			Canvas.hPopupMenu = CreatePopupMenu();
			AppendMenu(Canvas.hPopupMenu,MF_STRING,IDM_CANVAS_GSLCD,_T("LCD Mode"));
			AppendMenu(Canvas.hPopupMenu,MF_STRING,IDM_CANVAS_MAP16,_T("Map16 Mode"));
			AppendMenu(Canvas.hPopupMenu,MF_STRING,IDM_CANVAS_MAP32,_T("Map32 Mode"));
			AppendMenu(Canvas.hPopupMenu,MF_STRING,IDM_CANVAS_MAP64,_T("Map64 Mode"));
			//	AppendMenu(Canvas.hPopupMenu,MF_STRING,IDM_CANVAS_KEYBOARD,_T("Keyboard Mode"));
			AppendMenu(Canvas.hPopupMenu,MF_POPUP,(UINT)Canvas.hPopupMenuKeyboard,_T("Keyboard Mode"));
			AppendMenu(Canvas.hPopupMenu,MF_STRING,IDM_CANVAS_SLEEP,_T("Sleep Mode"));
			AppendMenu(Canvas.hPopupMenu,MF_SEPARATOR,0,0);
			AppendMenu(Canvas.hPopupMenu,MF_STRING,IDM_CANVAS_REDRAW,_T("Redraw"));
			break;
		case WM_DESTROY:
			DestroyMenu ( Canvas.hPopupMenuKeyboard );
			Canvas.hPopupMenuKeyboard = NULL;
			break;
		case WM_PAINT:
			CanvasPaintDo();
    		return 0;
		case WM_LBUTTONDBLCLK:
#ifdef EXT_CONTROL_MAIN_THREAD
			w32g_ext_control_main_thread(RC_EXT_MODE_CHANGE, 0);
#else
			w32g_send_rc(RC_EXT_MODE_CHANGE, 0);
#endif
			break;
      case WM_RBUTTONDOWN:
      {
      	RECT rc;
      	GetWindowRect(Canvas.hwnd,(RECT *)&rc);
				SetForegroundWindow ( Canvas.hwnd );
			TrackPopupMenu(Canvas.hPopupMenu,TPM_TOPALIGN|TPM_LEFTALIGN,
         	rc.left+(int)LOWORD(lParam),rc.top+(int)HIWORD(lParam),
            0,Canvas.hwnd,NULL);
				PostMessage ( Canvas.hwnd, WM_NULL, 0, 0 );
      }
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
			case IDM_CANVAS_GSLCD:
				CanvasChange(CANVAS_MODE_GSLCD);
				break;
			case IDM_CANVAS_MAP16:
				Canvas.MapMode = CMAP_MODE_16;
				CanvasChange(CANVAS_MODE_MAP16);
				break;
			case IDM_CANVAS_MAP32:
				Canvas.MapMode = CMAP_MODE_32;
				CanvasChange(CANVAS_MODE_MAP32);
				break;
			case IDM_CANVAS_MAP64:
				Canvas.MapMode = CMAP_MODE_64;
				CanvasChange(CANVAS_MODE_MAP64);
				break;
			case IDM_CANVAS_KEYBOARD:
				CanvasChange(CANVAS_MODE_KBD_A);
				break;
			case IDM_CANVAS_KEYBOARD_A:
				Canvas.CKPart = 1;
				CanvasChange(CANVAS_MODE_KBD_A);
				break;
			case IDM_CANVAS_KEYBOARD_B:
				Canvas.CKPart = 2;
				CanvasChange(CANVAS_MODE_KBD_B);
				break;
			case IDM_CANVAS_KEYBOARD_C:
				Canvas.CKPart = 3;
				CanvasChange(CANVAS_MODE_KBD_C);
				break;
			case IDM_CANVAS_KEYBOARD_D:
				Canvas.CKPart = 4;
				CanvasChange(CANVAS_MODE_KBD_D);
				break;
			case IDM_CANVAS_SLEEP:
				CanvasChange(CANVAS_MODE_SLEEP);
				break;
			case IDM_CANVAS_REDRAW:
//				PanelResetPart(PANELRESET_CHANNEL);
				CanvasReset();
				CanvasClear();
				CanvasReadPanelInfo(1);
				CanvasUpdate(1);
				CanvasPaintAll();
				break;
			}
			break;
		default:
			return DefWindowProc(hwnd,uMess,wParam,lParam) ;
	}
	return 0L;
}

// Color
#define CCR_FORE	 	RGB(0x00,0x00,0x00)
#define CCR_BACK 		RGB(0x00,0xf0,0x00)
#define CCR_DFORE	 	RGB(0x70,0x00,0x00)
#define CCR_DBACK		RGB(0x00,0xf0,0x00)
#define CCR_LOW		RGB(0x80,0xd0,0x00)
#define CCR_MIDDLE	RGB(0xb0,0xb0,0x00)
#define CCR_HIGH		RGB(0xe0,0x00,0x00)
// 色を m : n で混ぜる
static COLORREF HalfColorMN(COLORREF fc, COLORREF bc, int m, int n)
{
	return fc*m/(m+n) + bc*n/(m+n);
}
static COLORREF HalfColor23(COLORREF fc, COLORREF bc)
{
	return HalfColorMN(fc,bc,2,3);
}
static COLORREF HalfColor15(COLORREF fc, COLORREF bc)
{
	return HalfColorMN(fc,bc,1,5);
}
#define CC_BACK				0
#define CC_FORE				1
#define CC_LOW					2
#define CC_MIDDLE				3
#define CC_HIGH				4
#define CC_FORE_HALF			11
#define CC_LOW_HALF			12
#define CC_MIDDLE_HALF		13
#define CC_HIGH_HALF			14
#define CC_FORE_WEAKHALF	15
#define CC_DBACK				21
#define CC_DFORE				22
#define CC_DFORE_HALF		23
#define CC_DFORE_WEAKHALF	24
static COLORREF CanvasColor(int c)
{
  switch(c){
  	case CC_BACK:
			return CCR_BACK;
  	case CC_FORE:
			return CCR_FORE;
  	case CC_FORE_HALF:
			return HalfColor15(CCR_FORE,CCR_BACK);
  	case CC_FORE_WEAKHALF:
			return HalfColor23(CCR_FORE,CCR_BACK);
  	case CC_DBACK:
			return CCR_DBACK;
  	case CC_DFORE:
			return CCR_DFORE;
  	case CC_DFORE_HALF:
			return HalfColor15(CCR_DFORE,CCR_DBACK);
  	case CC_DFORE_WEAKHALF:
			return HalfColor23(CCR_DFORE,CCR_DBACK);
  	case CC_LOW:
			return CCR_LOW;
  	case CC_MIDDLE:
			return CCR_MIDDLE;
  	case CC_HIGH:
			return CCR_HIGH;
  	case CC_LOW_HALF:
			return HalfColor23(CCR_LOW,CCR_BACK);
  	case CC_MIDDLE_HALF:
			return HalfColor23(CCR_MIDDLE,CCR_BACK);
  	case CC_HIGH_HALF:
			return HalfColor23(CCR_HIGH,CCR_BACK);
		default:
			return CCR_BACK;
	}
}

static int CanvasOK = 0;
static void CanvasInit(HWND hwnd)
{
	RECT rc;

	GDI_LOCK(); // gdi_lock
	Canvas.hwnd = hwnd;
	Canvas.hParentWnd = GetParent(Canvas.hwnd);
	GetClientRect(Canvas.hParentWnd,&rc);
	MoveWindow(Canvas.hwnd,0,0,rc.right-rc.left,rc.bottom-rc.top,FALSE);
	GetClientRect(Canvas.hwnd,(RECT *)&(Canvas.rcMe));
	Canvas.hdc = GetDC(Canvas.hwnd);
	Canvas.hbitmap = CreateCompatibleBitmap(Canvas.hdc,CANVAS_XMAX,CANVAS_YMAX);
	Canvas.hmdc = CreateCompatibleDC(Canvas.hdc);
	ReleaseDC(Canvas.hwnd,Canvas.hdc);
	Canvas.hBitmapSleep = LoadBitmap(hInst,MAKEINTRESOURCE(IDB_BITMAP_SLEEP));
	Canvas.hgdiobj_hmdcprev = SelectObject(Canvas.hmdc,Canvas.hbitmap);

	SetRect((RECT *)&(Canvas.rcSleep),0,0,96,64);
	SetRect((RECT *)&(Canvas.rcMap),3,2+2,0,0);
	SetRect((RECT *)&(Canvas.rcKeyboard),1,1,0,0);
	SetRect((RECT *)&(Canvas.rcGSLCD),3,4,99,68);
   Canvas.rcMapMap.left = Canvas.rcMap.left;
   Canvas.rcMapMap.top = Canvas.rcMap.top;
   Canvas.rcMapMap.right = Canvas.rcMapMap.left + 6*16 - 1;
   Canvas.rcMapMap.bottom = Canvas.rcMapMap.top + 3*16 - 1;
   Canvas.rcMapSub.left = Canvas.rcMapMap.left;
   Canvas.rcMapSub.top = Canvas.rcMapMap.bottom + 2;
   Canvas.rcMapSub.right = Canvas.rcMapSub.left + 6*16 - 1;
   Canvas.rcMapSub.bottom = Canvas.rcMapSub.top +4+3+3+3+4 - 1;
	Canvas.MapDelay = 1;
	Canvas.MapResidual = 0;
	Canvas.MapMode = (MainWndInfo.CanvasMode == CANVAS_MODE_MAP32)
			? CMAP_MODE_32 : (MainWndInfo.CanvasMode == CANVAS_MODE_MAP64)
			? CMAP_MODE_64 : CMAP_MODE_16;
	Canvas.MapBarMax = 16;
	//	Canvas.CKNoteFrom = 24;
	//   Canvas.CKNoteTo = 24 + 96;
	Canvas.CKNoteFrom = 12;
   Canvas.CKNoteTo = Canvas.CKNoteFrom + 96 + 3;
	Canvas.CKCh = 16;
	Canvas.CKPart = (MainWndInfo.CanvasMode == CANVAS_MODE_KBD_B)
		? 2 : (MainWndInfo.CanvasMode == CANVAS_MODE_KBD_C)
		? 3 : (MainWndInfo.CanvasMode == CANVAS_MODE_KBD_D)
		? 4 : 1;
	Canvas.UpdateAll = 0;
	Canvas.Mode = (MainWndInfo.CanvasMode < CANVAS_MODE_GSLCD
			|| MainWndInfo.CanvasMode > CANVAS_MODE_SLEEP)
			? 1 : MainWndInfo.CanvasMode;
	Canvas.PaintDone = 0;
	GDI_UNLOCK(); // gdi_lock
	CanvasReset();
	CanvasOK = 1;
}

// Canvas Map

static void CanvasMapClear(void)
{
	HPEN hPen;
	HBRUSH hBrush;
	HGDIOBJ hgdiobj_hpen, hgdiobj_hbrush;
	if(!CanvasOK)
   	return;
	GDI_LOCK(); // gdi_lock
	hPen = CreatePen(PS_SOLID,1,CanvasColor(CC_BACK));
	hBrush = CreateSolidBrush(CanvasColor(CC_BACK));
	hgdiobj_hpen = SelectObject(Canvas.hmdc, hPen);
	hgdiobj_hbrush = SelectObject(Canvas.hmdc, hBrush);
	Rectangle(Canvas.hmdc,
   	Canvas.rcMe.left,Canvas.rcMe.top,Canvas.rcMe.right,Canvas.rcMe.bottom);
	SelectObject(Canvas.hmdc, hgdiobj_hpen);
	DeleteObject(hPen);
	SelectObject(Canvas.hmdc, hgdiobj_hbrush);
	DeleteObject(hBrush);
	GDI_UNLOCK(); // gdi_lock
	InvalidateRect(hCanvasWnd, NULL, FALSE);
}

static void CanvasMapReset(void)
{
	int i,j,ch;
	if(!CanvasOK)
   	return;
	switch(Canvas.MapMode){
   case CMAP_MODE_64:
		Canvas.MapCh = 64;
#ifdef MAPBAR_LIKE_TMIDI
		Canvas.MapBarWidth = 5;
		Canvas.MapBarMax = 4+1;
		Canvas.rcMapMap.bottom = Canvas.rcMapMap.top + 3*Canvas.MapBarMax*4 + 6 - 1;
#else
  		Canvas.MapBarWidth = 1;
		Canvas.MapBarMax = 16;
		Canvas.rcMapMap.bottom = Canvas.rcMapMap.top + 3*Canvas.MapBarMax - 1;
#endif
		break;
   case CMAP_MODE_32:
		Canvas.MapCh = 32;
#ifdef MAPBAR_LIKE_TMIDI
		Canvas.MapBarWidth = 5;
		Canvas.MapBarMax = 10+1;
		Canvas.rcMapMap.bottom = Canvas.rcMapMap.top + 3*Canvas.MapBarMax*2 + 6 - 1;
#else
  		Canvas.MapBarWidth = 2;
		Canvas.MapBarMax = 16;
		Canvas.rcMapMap.bottom = Canvas.rcMapMap.top + 3*Canvas.MapBarMax - 1;
#endif
		break;
   default:
   case CMAP_MODE_16:
		Canvas.MapCh = 16;
		Canvas.MapBarWidth = 5;
		Canvas.MapBarMax = 16;
		Canvas.rcMapMap.bottom = Canvas.rcMapMap.top + 3*Canvas.MapBarMax - 1;
		break;
	}
	for(i=0;i<Canvas.MapCh;i++){
		for(j=0;j<Canvas.MapBarMax;j++){
    		Canvas.MapMap[i][j] = CC_FORE_HALF;
    		Canvas.MapMapOld[i][j] = -1;
		}
   	Canvas.MapBar[i] = -1;
    	Canvas.MapBarOld[i] = -1;
	}
	for(ch=0;ch<Canvas.MapCh;ch++){
		Canvas.MapPan[ch] = Canvas.MapPanOld[ch] = 2;
		Canvas.MapSustain[ch] = Canvas.MapSustainOld[ch] = 0;
		Canvas.MapExpression[ch] = Canvas.MapExpressionOld[ch] = 0;
		Canvas.MapVolume[ch] = Canvas.MapVolumeOld[ch] = 0;
		Canvas.MapPitchbend[ch] = Canvas.MapPitchbendOld[ch] = 0x2000;
	}
	Canvas.MapResidual = -1;
	Canvas.MapUpdateFlag = 1;
	Canvas.PaintDone = 0;
}

static void CanvasMapReadPanelInfo(int flag)
{
	int ch,v;

	if(!CanvasOK)
		return;
	if(!PInfoOK)
    	return;
   if(Canvas.UpdateAll)
   	flag = 1;
	if(!Panel->changed && !flag)
   	return;
	// Bar
	Canvas.DrumChannel = drumchannels;
	for(ch=0;ch<Canvas.MapCh;ch++){
		Canvas.MapBarOld[ch] = Canvas.MapBar[ch];
		if(Panel->v_flags[ch] == FLAG_NOTE_ON){
#if 0
  			v = Panel->ctotal[ch]/8;
#else
			v = (int) Panel->ctotal[ch] * Canvas.MapBarMax / 128;
#endif
		} else {
      	v = 0;
		}
		if(v<0) v = 0; else if(v>Canvas.MapBarMax-1) v = Canvas.MapBarMax-1;
		if(v != Canvas.MapBar[ch]){
			// 遅延
	    	if(Canvas.MapDelay){
   	  		int old = Canvas.MapBar[ch];
				if(Canvas.MapBar[ch]<0)
					Canvas.MapBar[ch] = v;
				else
					Canvas.MapBar[ch] = (old*10*1/3 + v*10*2/3)/10;
				if(old == Canvas.MapBar[ch]){
     				if(v>old)
						Canvas.MapBar[ch] = old + 1;
     				else if(v<old)
						Canvas.MapBar[ch] = old - 1;
      		}
			} else
				Canvas.MapBar[ch] = v;
		}
		if(Canvas.MapBarOld[ch] != Canvas.MapBar[ch])
			Canvas.MapResidual = -1;
	}
	// Sub
	if(Canvas.MapMode==CMAP_MODE_16){
		Canvas.MapChChanged = 0;
		for(ch=0;ch<Canvas.MapCh;ch++){
			int changed = 0;
			Canvas.MapPanOld[ch] = Canvas.MapPan[ch];
			Canvas.MapSustainOld[ch] = Canvas.MapSustain[ch];
			Canvas.MapExpressionOld[ch] = Canvas.MapExpression[ch];
			Canvas.MapVolumeOld[ch] = Canvas.MapVolume[ch];
			Canvas.MapPitchbendOld[ch] = Canvas.MapPitchbend[ch];
			if(Panel->channel[ch].panning==NO_PANNING)
         	Canvas.MapPan[ch] = -1;
			else {
				Canvas.MapPan[ch] = ( Panel->channel[ch].panning - 64 ) * 3 /128;
//				Canvas.MapPan[ch] = ( 64 - Panel->channel[ch].panning ) * 3 /128;
				Canvas.MapPan[ch] = SetValue(Canvas.MapPan[ch],-2,2) + 2;
			}
			if(Panel->channel[ch].sustain)
				Canvas.MapSustain[ch] = 5;
         else
				Canvas.MapSustain[ch] = 0;
			//Canvas.MapSustain[ch] = SetValue(Canvas.MapSustain[ch],0,10);
#if 0
			Canvas.MapExpression[ch] = (Panel->channel[ch].expression * 11) >>8;
#else
		    Canvas.MapExpression[ch] = (Panel->channel[ch].expression * 11) >>7;
#endif
			Canvas.MapExpression[ch] = SetValue(Canvas.MapExpression[ch],0,10);
#if 0
			Canvas.MapVolume[ch] = (Panel->channel[ch].volume * 11) >>8;
#else
			Canvas.MapVolume[ch] = (Panel->channel[ch].volume * 11) >>7;
#endif
			Canvas.MapVolume[ch] = SetValue(Canvas.MapVolume[ch],0,10);
			Canvas.MapPitchbend[ch] = Panel->channel[ch].pitchbend;
			if(Canvas.MapPanOld[ch]!=Canvas.MapPan[ch]) changed = 1;
			if(Canvas.MapSustainOld[ch]!=Canvas.MapSustain[ch]) changed = 1;
			if(Canvas.MapExpressionOld[ch]!=Canvas.MapExpression[ch]) changed = 1;
			if(Canvas.MapVolumeOld[ch]!=Canvas.MapVolume[ch]) changed = 1;
			if(Canvas.MapPitchbendOld[ch]!=Canvas.MapPitchbend[ch]) changed = 1;
			if(changed)
  		    	Canvas.MapChChanged |= 1 << ch;
		}
	}
}

static void CanvasMapDrawMapBar(int flag)
{
	int i,ch;

	if(!CanvasOK)
		return;
   if(Canvas.UpdateAll)
   	flag = 1;
	if(!Canvas.MapResidual && !flag)
   	return;
	Canvas.MapResidual = 0;
	for(ch=0;ch<Canvas.MapCh;ch++){
		int drumflag = IS_SET_CHANNELMASK(Canvas.DrumChannel,ch);
 		char color1,color2,color3;
		if(drumflag){
			color1 = CC_DFORE;
			color2 = CC_DFORE_WEAKHALF;
         color3 = CC_DFORE_HALF;
      } else {
			color1 = CC_FORE;
			color2 = CC_FORE_WEAKHALF;
         color3 = CC_FORE_HALF;
      }
		for(i=0;i<Canvas.MapBarMax;i++){
			int y = Canvas.MapBarMax-1-i;
			Canvas.MapMapOld[ch][y] = Canvas.MapMap[ch][y];
			if(i<=Canvas.MapBar[ch]){
				Canvas.MapMap[ch][y] = color1;
				Canvas.MapResidual = 1;
#if 1		// 残像
			} else if(i<=Canvas.MapBarOld[ch]){
				Canvas.MapMap[ch][y] = color2;
				Canvas.MapResidual = 1;
#endif
    		} else {
				Canvas.MapMap[ch][y] = color3;
			}
		}
	}
	if(Canvas.MapResidual==0)
		Canvas.MapResidual = -1;
}

// #define CMAP_ALL_UPDATE
static void CanvasMapUpdate(int flag)
{
	RECT rc;
	int i,j;
	int change_flag = 0;
	if(!CanvasOK)
   	return;
	CanvasMapDrawMapBar(flag);
   if(Canvas.UpdateAll)
   	flag = 1;
	if(PInfoOK && Canvas.MapMode==CMAP_MODE_16){
   if(flag || Panel->changed){
		int ch;
		GDI_LOCK();
		for(ch=0;ch<Canvas.MapCh;ch++){
			int x,y;
         COLORREF color;
			COLORREF colorFG, colorBG;
			int drumflag = IS_SET_CHANNELMASK(Canvas.DrumChannel,ch);
			if(!flag && !(Canvas.MapChChanged & (1<<ch)))
         	continue;
			if(drumflag){
				colorFG = CanvasColor(CC_DFORE);
				colorBG = CanvasColor(CC_DFORE_HALF);
      	} else {
				colorFG = CanvasColor(CC_FORE);
				colorBG = CanvasColor(CC_FORE_HALF);
      	}
         rc.left = Canvas.rcMapSub.left + (5 + 1) * ch;
         rc.right = rc.left + 5 - 1;
			// PAN
         rc.top = Canvas.rcMapSub.top;
         rc.bottom = rc.top + 3 - 1;
				for(x=rc.left;x<=rc.right;x++){
				for(y=rc.top;y<=rc.bottom;y++)
        			SetPixelV(Canvas.hmdc,x,y,colorBG);
				}
			if(Canvas.MapPan[ch]>=0){
				x = rc.left + Canvas.MapPan[ch];
				for(y=rc.top;y<=rc.bottom;y++)
     				SetPixelV(Canvas.hmdc,x,y,colorFG);
			}
         // SUSTAIN
         rc.top = rc.bottom + 2;
         rc.bottom = rc.top + 2 - 1;
				if(Canvas.MapSustain[ch]){
					for(x=rc.left;x<=rc.right;x++){
					for(y=rc.top;y<=rc.bottom;y++)
        				SetPixelV(Canvas.hmdc,x,y,colorFG);
					}
				} else {
					for(x=rc.left;x<=rc.right;x++){
					for(y=rc.top;y<=rc.bottom;y++)
        				SetPixelV(Canvas.hmdc,x,y,colorBG);
					}
         // EXPRESSION
         rc.top = rc.bottom + 2;
         rc.bottom = rc.top + 2 - 1;
			for(i=1;i<=10;i++){
         		if(i <= Canvas.MapExpression[ch])
            	color = colorFG;
            else
            	color = colorBG;
			x = rc.left + (i-1)/2;
			y = rc.top + (i+1)%2;
  				SetPixelV(Canvas.hmdc,x,y,color);
			}
         // VOLUME
         rc.top = rc.bottom + 2;
         rc.bottom = rc.top + 2 - 1;
			for(i=1;i<=10;i++){
        		if(i <= Canvas.MapVolume[ch])
            	color = colorFG;
            else
            	color = colorBG;
            x = rc.left + (i-1)/2;
				y = rc.top + (i+1)%2;
  				SetPixelV(Canvas.hmdc,x,y,color);
			}
         // PITCH_BEND
         rc.top = rc.bottom + 2;
         rc.bottom = rc.top + 3 - 1;
					for(x=rc.left;x<=rc.right;x++){
				for(y=rc.top;y<=rc.bottom;y++)
        			SetPixelV(Canvas.hmdc,x,y,colorBG);
					}
         if(Canvas.MapPitchbend[ch]==-2){
         	y = rc.top + 1;
				for(x=rc.left;x<=rc.right;x++)
  					SetPixelV(Canvas.hmdc,x,y,colorFG);
				y++;
				for(x=rc.left;x<=rc.right;x++)
  					SetPixelV(Canvas.hmdc,x,y,colorFG);
			} else if(Canvas.MapPitchbend[ch]>0x2000){
         	y = rc.top;
				for(x=rc.left;x<=rc.left;x++)
  					SetPixelV(Canvas.hmdc,x,y,colorFG);
				y++;
				for(x=rc.left;x<=rc.left+2;x++)
  					SetPixelV(Canvas.hmdc,x,y,colorFG);
				y++;
				for(x=rc.left;x<=rc.left+4;x++)
		 			SetPixelV(Canvas.hmdc,x,y,colorFG);
			} else if(Canvas.MapPitchbend[ch]<0x2000){
         	y = rc.top;
				for(x=rc.right;x<=rc.right;x++)
  					SetPixelV(Canvas.hmdc,x,y,colorFG);
				y++;
				for(x=rc.right-2;x<=rc.right;x++)
  					SetPixelV(Canvas.hmdc,x,y,colorFG);
				y++;
				for(x=rc.right-4;x<=rc.right;x++)
		 			SetPixelV(Canvas.hmdc,x,y,colorFG);
         }
      }
      }
	 		InvalidateRect(hCanvasWnd,(RECT *)&(Canvas.rcMapSub),FALSE);
			GDI_UNLOCK();
   }
   }
	if(!Canvas.MapResidual && !flag)
   	return;
	change_flag = 0;
#ifndef MAPBAR_LIKE_TMIDI
	GDI_LOCK();
	for(i=0;i<Canvas.MapCh;i++){
		for(j=0;j<Canvas.MapBarMax;j++){
			if(Canvas.MapMap[i][j]!=Canvas.MapMapOld[i][j] || flag){
				int x,y;
				COLORREF color = CanvasColor(Canvas.MapMap[i][j]);
            rc.left = Canvas.rcMap.left + (Canvas.MapBarWidth + 1) * i;
            rc.right = rc.left -1 + Canvas.MapBarWidth;
            rc.top = Canvas.rcMap.top + (2 + 1) * j;
            rc.bottom = rc.top -1 + 2;
				for(x=rc.left;x<=rc.right;x++)
					for(y=rc.top;y<=rc.bottom;y++)
          			SetPixelV(Canvas.hmdc,x,y,color);
				change_flag = 1;
    		}
   	}
	}
	GDI_UNLOCK();
#else
	GDI_LOCK();
	if(Canvas.MapMode==CMAP_MODE_16){
	for(i=0;i<Canvas.MapCh;i++){
		for(j=0;j<Canvas.MapBarMax;j++){
			if(Canvas.MapMap[i][j]!=Canvas.MapMapOld[i][j] || flag){
				int x,y;
				COLORREF color = CanvasColor(Canvas.MapMap[i][j]);
				rc.left = Canvas.rcMap.left + (Canvas.MapBarWidth + 1) * i;
				rc.right = rc.left -1 + Canvas.MapBarWidth;
				rc.top = Canvas.rcMap.top + (2 + 1) * j;
				rc.bottom = rc.top -1 + 2;
					for(x=rc.left;x<=rc.right;x++){
					for(y=rc.top;y<=rc.bottom;y++)
          			SetPixelV(Canvas.hmdc,x,y,color);
					}
				change_flag = 1;
    		}
   		}
	}
	} else if(Canvas.MapMode==CMAP_MODE_32){
	for(i=0;i<Canvas.MapCh;i++){
		for(j=0;j<Canvas.MapBarMax;j++){
			if(Canvas.MapMap[i][j]!=Canvas.MapMapOld[i][j] || flag){
				int x,y;
				COLORREF color = CanvasColor(Canvas.MapMap[i][j]);
				if(i<=15){
					rc.left = Canvas.rcMap.left + (Canvas.MapBarWidth + 1) * i;
					rc.right = rc.left -1 + Canvas.MapBarWidth;
						rc.top = -1 + Canvas.rcMap.top + (2 + 1) * j;
					rc.bottom = rc.top -1 + 2;
				} else {
					rc.left = Canvas.rcMap.left + (Canvas.MapBarWidth + 1) * (i-16);
					rc.right = rc.left -1 + Canvas.MapBarWidth;
						rc.top = -1 + Canvas.rcMap.top + (2 + 1) * j + Canvas.MapBarMax*(2+1) + 2 ;
					rc.bottom = rc.top -1 + 2;
				}
				for(x=rc.left;x<=rc.right;x++)
					for(y=rc.top;y<=rc.bottom;y++)
	          			SetPixelV(Canvas.hmdc,x,y,color);
				change_flag = 1;
    		}
   		}
	}
	} else if(Canvas.MapMode==CMAP_MODE_64){
	for(i=0;i<Canvas.MapCh;i++){
		for(j=0;j<Canvas.MapBarMax;j++){
			if(Canvas.MapMap[i][j]!=Canvas.MapMapOld[i][j] || flag){
				int x,y;
				COLORREF color = CanvasColor(Canvas.MapMap[i][j]);
				if(i<=15){
					rc.left = Canvas.rcMap.left + (Canvas.MapBarWidth + 1) * i;
					rc.right = rc.left -1 + Canvas.MapBarWidth;
						rc.top = -1 + Canvas.rcMap.top + (2 + 1) * j;
					rc.bottom = rc.top -1 + 2;
				} else if(i<=31){
					rc.left = Canvas.rcMap.left + (Canvas.MapBarWidth + 1) * (i-16);
					rc.right = rc.left -1 + Canvas.MapBarWidth;
						rc.top = -1 + Canvas.rcMap.top + (2 + 1) * j + Canvas.MapBarMax*(2+1) + 2 ;
					rc.bottom = rc.top -1 + 2;
				} else if(i<=47){
					rc.left = Canvas.rcMap.left + (Canvas.MapBarWidth + 1) * (i-32);
					rc.right = rc.left -1 + Canvas.MapBarWidth;
						rc.top = -1 + Canvas.rcMap.top + (2 + 1) * j + 2*Canvas.MapBarMax*(2+1) + 2*2 ;
					rc.bottom = rc.top -1 + 2;
				} else if(i<=63){
					rc.left = Canvas.rcMap.left + (Canvas.MapBarWidth + 1) * (i-48);
					rc.right = rc.left -1 + Canvas.MapBarWidth;
						rc.top = -1 + Canvas.rcMap.top + (2 + 1) * j + 3*Canvas.MapBarMax*(2+1) + 3*2 ;
					rc.bottom = rc.top -1 + 2;
				}
				for(x=rc.left;x<=rc.right;x++)
					for(y=rc.top;y<=rc.bottom;y++)
	          			SetPixelV(Canvas.hmdc,x,y,color);
				change_flag = 1;
    		}
   		}
	}
	}
	GDI_UNLOCK();
#endif
#ifdef CMAP_ALL_UPDATE
	if(change_flag)
		InvalidateRect(hCanvasWnd,NULL,FALSE);
#else
	if(change_flag)
		InvalidateRect(hCanvasWnd,(RECT *)&(Canvas.rcMapMap),FALSE);
#endif
	if(Canvas.UpdateAll)
		InvalidateRect(hCanvasWnd,NULL,FALSE);
	if(Canvas.MapResidual<0)
		Canvas.MapResidual = 0;
}

static void CanvasSleepClear(void)
{
	RECT rc;
	HPEN hPen;
	HBRUSH hBrush;
	HGDIOBJ hgdiobj_hpen, hgdiobj_hbrush;
	GDI_LOCK(); // gdi_lock
	hPen = CreatePen(PS_SOLID,1,GetSysColor(COLOR_SCROLLBAR));
	hBrush = CreateSolidBrush(GetSysColor(COLOR_SCROLLBAR));
	hgdiobj_hpen = SelectObject(Canvas.hmdc, hPen);
	hgdiobj_hbrush = SelectObject(Canvas.hmdc, hBrush);
	GetClientRect(Canvas.hwnd, &rc);
	Rectangle(Canvas.hmdc,rc.left,rc.top,rc.right,rc.bottom);
	SelectObject(Canvas.hmdc, hgdiobj_hpen);
	DeleteObject(hPen);
	SelectObject(Canvas.hmdc, hgdiobj_hbrush);
	DeleteObject(hBrush);
	GDI_UNLOCK(); // gdi_lock
	InvalidateRect(hCanvasWnd, NULL, FALSE);
}

static void CanvasSleepReset(void)
{
	Canvas.PaintDone = 0;
}

static void CanvasSleepReadPanelInfo(int flag)
{
}

static int CanvasSleepDone = 0;
static void CanvasSleepUpdate(int flag)
{
	HDC hdc;
	RECT rc;
	int x,y;
	CanvasSleepReset();
	GDI_LOCK(); // gdi_lock
   hdc = CreateCompatibleDC(Canvas.hmdc);
	SelectObject(hdc,Canvas.hBitmapSleep);
	GetClientRect(Canvas.hwnd, &rc);
	x = (rc.right - rc.left - Canvas.rcSleep.right)/2;
	y = (rc.bottom - rc.top - Canvas.rcSleep.bottom)/2;
	if(x<0) x = 0;
	if(y<0) y = 0;
	BitBlt(Canvas.hmdc,x,y,Canvas.rcSleep.right,Canvas.rcSleep.bottom,hdc,0,0,SRCCOPY);
	DeleteDC(hdc);
	GDI_UNLOCK(); // gdi_lock
	if(flag) {InvalidateRect(hCanvasWnd, NULL, FALSE);}
}

// Canvas GSLCD

static void CanvasGSLCDReset(void)
{
	int i, j;
	for(i = 0; i < 16; i++) {
		for(j = 0; j < 16; j++) {
			Canvas.GSLCD[i][j] = 0;
			Canvas.GSLCD_old[i][j] = 0;
		}
	}
	Canvas.PaintDone = 0;
}

static void CanvasGSLCDReadPanelInfo(int flag)
{
	int i, j;
	if (!CanvasOK) {return;}
	if (!PInfoOK) {return;}
	if (Canvas.UpdateAll) {flag = 1;}
	if (!Panel->changed && !flag) {return;}
	for(i = 0; i < 16; i++) {
		for(j = 0; j < 16; j++) {
			Canvas.GSLCD_old[i][j] = Canvas.GSLCD[i][j];
			Canvas.GSLCD[i][j] = Panel->GSLCD[i][j];
		}
	}
}

#define CGSLCD_BG RGB(0x00,0xf0,0x00)
#define CGSLCD_ON RGB(0x00,0x00,0x00)
#define CGSLCD_OFF RGB(0x00,0xc0,0x00)
static void CanvasGSLCDUpdate(int flag)
{
	int i, j, x, y, changed = 0;
	COLORREF colorON = CGSLCD_ON, colorOFF = CGSLCD_OFF;
	HPEN hPen;
	HBRUSH hBrush;
	HGDIOBJ hgdiobj_hpen, hgdiobj_hbrush;

	if(!PInfoOK) {return;}
	if(Canvas.UpdateAll) {flag = 1;}
	if(!Panel->changed && !flag) {return;}
	GDI_LOCK(); // gdi_lock
	for (i = 0; i < 16; i++) {
		for (j = 0; j < 16; j++) {
     		x = Canvas.rcGSLCD.left + i * 6;
   			y = Canvas.rcGSLCD.top + j * 4;
			if (flag || Canvas.GSLCD[i][j] != Canvas.GSLCD_old[i][j]) {
				changed = 1;
				if (Canvas.GSLCD[i][j] == 1) {
					hPen = CreatePen(PS_SOLID, 1, colorON);
					hBrush = CreateSolidBrush(colorON);
				}
				else {
					hPen = CreatePen(PS_SOLID, 1, colorOFF);
					hBrush = CreateSolidBrush(colorOFF);
				}
				hgdiobj_hpen = SelectObject(Canvas.hmdc, hPen);
				hgdiobj_hbrush = SelectObject(Canvas.hmdc, hBrush);
				Rectangle(Canvas.hmdc, x, y, x + 5, y + 3);
				SelectObject(Canvas.hmdc, hgdiobj_hpen);
				DeleteObject(hPen);
				SelectObject(Canvas.hmdc, hgdiobj_hbrush);
				DeleteObject(hBrush);
			}
		}
	}
	if (changed) {
		GDI_UNLOCK();
       	InvalidateRect(Canvas.hwnd, (RECT *)&Canvas.rcGSLCD, FALSE);
		GDI_LOCK();
	}
	GDI_UNLOCK(); // gdi_lock
	if(flag) {InvalidateRect(hCanvasWnd, NULL, FALSE);}
}

static void CanvasGSLCDClear(void)
{
	HPEN hPen;
	HBRUSH hBrush;
	HGDIOBJ hgdiobj_hpen, hgdiobj_hbrush;
	COLORREF BGcolor;
	if(!CanvasOK)
   	return;
 	GDI_LOCK(); // gdi_lock
	BGcolor = RGB(0,0,0);
	hPen = CreatePen(PS_SOLID,1,BGcolor);
	hBrush = CreateSolidBrush(BGcolor);
	hgdiobj_hpen = SelectObject(Canvas.hmdc, hPen);
	hgdiobj_hbrush = SelectObject(Canvas.hmdc, hBrush);
	Rectangle(Canvas.hmdc,
   	Canvas.rcMe.left,Canvas.rcMe.top,Canvas.rcMe.right,Canvas.rcMe.bottom);
	SelectObject(Canvas.hmdc, hgdiobj_hpen);
	DeleteObject(hPen);
	SelectObject(Canvas.hmdc, hgdiobj_hbrush);
	DeleteObject(hBrush);
	BGcolor = CGSLCD_BG;
	hPen = CreatePen(PS_SOLID,1,BGcolor);
	hBrush = CreateSolidBrush(BGcolor);
	hgdiobj_hpen = SelectObject(Canvas.hmdc, hPen);
	hgdiobj_hbrush = SelectObject(Canvas.hmdc, hBrush);
	Rectangle(Canvas.hmdc,
   	Canvas.rcMe.left + 1,Canvas.rcMe.top + 1,Canvas.rcMe.right - 1,Canvas.rcMe.bottom - 1);
	SelectObject(Canvas.hmdc, hgdiobj_hpen);
	DeleteObject(hPen);
	SelectObject(Canvas.hmdc, hgdiobj_hbrush);
	DeleteObject(hBrush);
	GDI_UNLOCK(); // gdi_lock

	CanvasGSLCDReset();
	CanvasGSLCDReadPanelInfo(1);
	CanvasGSLCDUpdate(1);
	InvalidateRect(hCanvasWnd, NULL, FALSE);
}

// Canvas Keyboard

static void CanvasKeyboardReset(void)
{
	int i,j;
	int ChFrom = (Canvas.CKPart - 1) * Canvas.CKCh;
	int ChTo = Canvas.CKPart * Canvas.CKCh - 1;
	for(i=ChFrom;i<=ChTo;i++){
		for(j=0;j<4;j++){
			Canvas.CKxnote[i][j] = 0;
			Canvas.CKxnote_old[i][j] = 0;
		}
	}
	Canvas.PaintDone = 0;
}

static void CanvasKeyboardReadPanelInfo(int flag)
{
	int i,j;
	int ChFrom, ChTo;
	if(!CanvasOK)
		return;
	if(!PInfoOK)
    	return;
	ChFrom = (Canvas.CKPart - 1) * Canvas.CKCh;
	ChTo = Canvas.CKPart * Canvas.CKCh - 1;
   if(Canvas.UpdateAll)
   	flag = 1;
	if(!Panel->changed && !flag)
   	return;
	for(i=ChFrom;i<=ChTo;i++)
		for(j=0;j<4;j++){
      	Canvas.CKxnote_old[i][j] = Canvas.CKxnote[i][j];
			Canvas.CKxnote[i][j] = Panel->xnote[i][j];
      }
}

#define CK_KEY_BLACK	1
#define CK_KEY_WHITE	2
#define CK_ON RGB(0xff,0x00,0x00)
#define CK_OFF_WHITE RGB(0xff,0xff,0xff)
#define CK_OFF_BLACK RGB(0x00,0x00,0x00)
#define CK_DOFF_WHITE RGB(0xcc,0xcc,0xcc)
#define CK_DOFF_BLACK RGB(0x00,0x00,0x00)
static void CanvasKeyboardUpdate(int flag)
{
	int j,k,l;
   int channel;
	int ChFrom, ChTo;

	if(!PInfoOK)
		return;
   if(Canvas.UpdateAll)
   	flag = 1;
	if(!COMPARE_CHANNELMASK(Canvas.DrumChannel,drumchannels))
   	flag = 1;
	if(!Panel->changed && !flag)
   	return;
	ChFrom = (Canvas.CKPart - 1) * Canvas.CKCh;
	ChTo = Canvas.CKPart * Canvas.CKCh - 1;
	GDI_LOCK(); // gdi_lock
	for(channel=ChFrom;channel<=ChTo;channel++){
		int change_flag = 0;
		int drumflag = IS_SET_CHANNELMASK(drumchannels,channel);
		COLORREF colorON, colorOFF_WHITE, colorOFF_BLACK;
		if(drumflag){
			colorON = CK_ON;
         colorOFF_WHITE = CK_DOFF_WHITE;
         colorOFF_BLACK = CK_OFF_BLACK;
      } else {
			colorON = CK_ON;
         colorOFF_WHITE = CK_OFF_WHITE;
         colorOFF_BLACK = CK_OFF_BLACK;
      }
		for(j=0;j<4;j++){
			int32 xnote, xnote_diff;
			xnote = Canvas.CKxnote[channel][j];
        	xnote_diff = Canvas.CKxnote[channel][j] ^ Canvas.CKxnote_old[channel][j];
			if(!flag && xnote_diff == 0)
         	continue;
			for(k=0;k<32;k++){
				int key = 0;
				int KeyOn = 0;
  	   		int note = j*32+k;
            int reff = (int32)1 << k;
				int x,y;
				if(note < Canvas.CKNoteFrom || note > Canvas.CKNoteTo)
      			continue;
            if(!flag && !(xnote_diff & reff))
              	continue;
            if(xnote & reff)
					KeyOn = 1;
				note = note % 12;
            if(note == 1 || note == 3 || note == 6 || note == 8 || note == 10)
			 		key = CK_KEY_BLACK;
            else
			 		key = CK_KEY_WHITE;
     			x = Canvas.rcKeyboard.left + j * 32 + k - Canvas.CKNoteFrom;
   			y = Canvas.rcKeyboard.top + (channel - ChFrom) * 4;
				switch(key){
            case CK_KEY_BLACK:
	            if(KeyOn){
						for(l=0;l<2;l++)
      	  				SetPixelV(Canvas.hmdc,x,y+l,colorON);
  						SetPixelV(Canvas.hmdc,x,y+2,colorOFF_WHITE);
	            } else {
						for(l=0;l<2;l++)
      	  				SetPixelV(Canvas.hmdc,x,y+l,colorOFF_BLACK);
  						SetPixelV(Canvas.hmdc,x,y+2,colorOFF_WHITE);
         	   }
					break;
            case CK_KEY_WHITE:
	            if(KeyOn){
  						SetPixelV(Canvas.hmdc,x,y,colorOFF_WHITE);
						for(l=1;l<3;l++)
      	  				SetPixelV(Canvas.hmdc,x,y+l,colorON);
	            } else {
  						SetPixelV(Canvas.hmdc,x,y,colorOFF_WHITE);
						for(l=1;l<3;l++)
      	  				SetPixelV(Canvas.hmdc,x,y+l,colorOFF_WHITE);
         	   }
					break;
				default:
            	break;
	  			}
            change_flag = 1;
      	}
		}
		if(change_flag){
         RECT rc;
			GDI_UNLOCK();
			GetClientRect(Canvas.hwnd,&rc);
         rc.top = Canvas.rcKeyboard.top + (channel - ChFrom) * 4;
			rc.bottom = rc.top + 4;
       	InvalidateRect(Canvas.hwnd, &rc, FALSE);
			GDI_LOCK();
		}
	}
	GDI_UNLOCK(); // gdi_lock
	if(flag)
		InvalidateRect(hCanvasWnd, NULL, FALSE);
	Canvas.DrumChannel = drumchannels;
}

static void CanvasKeyboardClear(void)
{
	int i;
	HPEN hPen;
	HBRUSH hBrush;
	HGDIOBJ hgdiobj_hpen, hgdiobj_hbrush;
	COLORREF FGcolor, BGcolor;
	HFONT hfont;
   HGDIOBJ hgdiobj;
	RECT rc;
	TCHAR buffer[16];
	if(!CanvasOK)
   	return;
 	GDI_LOCK(); // gdi_lock
#if 0
	hPen = CreatePen(PS_SOLID,1,CanvasColor(CC_BACK));
	hBrush = CreateSolidBrush(CanvasColor(CC_BACK));
#else
	FGcolor = RGB(0xff,0xff,0xff);
   BGcolor = RGB(0x00,0x00,0x00);
	hPen = CreatePen(PS_SOLID,1,BGcolor);
	hBrush = CreateSolidBrush(BGcolor);
#endif
	hgdiobj_hpen = SelectObject(Canvas.hmdc, hPen);
	hgdiobj_hbrush = SelectObject(Canvas.hmdc, hBrush);
	Rectangle(Canvas.hmdc,
   	Canvas.rcMe.left,Canvas.rcMe.top,Canvas.rcMe.right,Canvas.rcMe.bottom);
	SelectObject(Canvas.hmdc, hgdiobj_hpen);
	DeleteObject(hPen);
	SelectObject(Canvas.hmdc, hgdiobj_hbrush);
	DeleteObject(hBrush);

	hfont = CreateFont(7,7,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,
		DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
     	DEFAULT_PITCH | FF_DONTCARE,_T("Arial Bold"));
	hgdiobj = SelectObject(Canvas.hmdc,hfont);
   SetTextAlign(Canvas.hmdc, TA_LEFT | TA_TOP | TA_NOUPDATECP);
	rc.left =	Canvas.rcMe.left+1 ;
	rc.top =		Canvas.rcMe.bottom-7;
	rc.right =	Canvas.rcMe.left+1 + 40;
	rc.bottom =	Canvas.rcMe.bottom;
	SetTextColor(Canvas.hmdc,RGB(0xff,0xff,0xff));
	SetBkColor(Canvas.hmdc,RGB(0x00,0x00,0x00));
	_tcscpy(buffer,_T(" "));
 	ExtTextOut(Canvas.hmdc,rc.left,rc.top,ETO_CLIPPED|ETO_OPAQUE,&rc,
 		buffer,_tcslen(buffer),NULL);
	for(i=1;i<=(MAX_CHANNELS>>4);i++){
		if(i==Canvas.CKPart){
   		SetTextColor(Canvas.hmdc,RGB(0xff,0xff,0xff));
   		SetBkColor(Canvas.hmdc,RGB(0x00,0x00,0x00));
      } else {
   		SetTextColor(Canvas.hmdc,RGB(0x80,0x80,0x80));
   		SetBkColor(Canvas.hmdc,RGB(0x00,0x00,0x00));
		}
		rc.left =	Canvas.rcMe.left+1 + 0 + (i-1)*24;
		rc.top =		Canvas.rcMe.bottom-7;
		rc.right =	Canvas.rcMe.left+1 + 0 + (i)*24 - 1;
		rc.bottom =	Canvas.rcMe.bottom;
		_stprintf(buffer,_T("[%c]"),i+_T('A')-1);
 	 	ExtTextOut(Canvas.hmdc,rc.left,rc.top,ETO_CLIPPED|ETO_OPAQUE,&rc,
   		buffer,_tcslen(buffer),NULL);
	}
   if((HGDIOBJ)hgdiobj!=(HGDIOBJ)NULL && (HGDIOBJ)hgdiobj!=(HGDIOBJ)GDI_ERROR)
   	SelectObject(Canvas.hmdc,hgdiobj);
	DeleteObject(hfont);
	GDI_UNLOCK(); // gdi_lock

	CanvasKeyboardReset();
	CanvasKeyboardReadPanelInfo(1);
	CanvasKeyboardUpdate(1);
	InvalidateRect(hCanvasWnd, NULL, FALSE);
}

// Canvas All

static void CanvasPaintDo(void)
{
	RECT rc;
	if ( GetUpdateRect(Canvas.hwnd, &rc, FALSE) ) {
		PAINTSTRUCT ps;
		GDI_LOCK(); // gdi_lock
		Canvas.hdc = BeginPaint(Canvas.hwnd, &ps);
		BitBlt(Canvas.hdc,rc.left,rc.top,rc.right,rc.bottom,Canvas.hmdc,rc.left,rc.top,SRCCOPY);
		EndPaint(Canvas.hwnd, &ps);
		GDI_UNLOCK(); // gdi_lock
	}
}
void CanvasPaint(void)
{
	Canvas.PaintDone = 0;
	//UpdateWindow(hCanvasWnd);
}
void CanvasPaintAll(void)
{
	InvalidateRect(hCanvasWnd, NULL, FALSE);
	CanvasPaint();
}

void CanvasReset(void)
{
	if (! CanvasOK)
		return;
	switch (Canvas.Mode) {
	case CANVAS_MODE_GSLCD:
		CanvasGSLCDReset();
		break;
	case CANVAS_MODE_MAP16:
	case CANVAS_MODE_MAP32:
	case CANVAS_MODE_MAP64:
		CanvasMapReset();
		break;
	case CANVAS_MODE_KBD_A:
	case CANVAS_MODE_KBD_B:
	case CANVAS_MODE_KBD_C:
	case CANVAS_MODE_KBD_D:
		CanvasKeyboardReset();
		break;
	case CANVAS_MODE_SLEEP:
		CanvasSleepReset();
		break;
	}
}

void CanvasClear(void)
{
	if (! CanvasOK)
		return;
	switch (Canvas.Mode) {
	case CANVAS_MODE_GSLCD:
		CanvasGSLCDClear();
		break;
	case CANVAS_MODE_MAP16:
	case CANVAS_MODE_MAP32:
	case CANVAS_MODE_MAP64:
		CanvasMapClear();
		break;
	case CANVAS_MODE_KBD_A:
	case CANVAS_MODE_KBD_B:
	case CANVAS_MODE_KBD_C:
	case CANVAS_MODE_KBD_D:
		CanvasKeyboardClear();
		break;
	case CANVAS_MODE_SLEEP:
		CanvasSleepClear();
		break;
	}
}

void CanvasUpdate(int flag)
{
	if (! CanvasOK)
		return;
	switch (Canvas.Mode) {
	case CANVAS_MODE_GSLCD:
		CanvasGSLCDUpdate(flag);
		break;
	case CANVAS_MODE_MAP16:
	case CANVAS_MODE_MAP32:
	case CANVAS_MODE_MAP64:
		CanvasMapUpdate(flag);
		break;
	case CANVAS_MODE_KBD_A:
	case CANVAS_MODE_KBD_B:
	case CANVAS_MODE_KBD_C:
	case CANVAS_MODE_KBD_D:
		CanvasKeyboardUpdate(flag);
		break;
	case CANVAS_MODE_SLEEP:
		CanvasSleepUpdate(flag);
		break;
	}
}

void CanvasReadPanelInfo(int flag)
{
	if (! CanvasOK)
		return;
	switch (Canvas.Mode) {
	case CANVAS_MODE_GSLCD:
		CanvasGSLCDReadPanelInfo(flag);
		break;
	case CANVAS_MODE_MAP16:
	case CANVAS_MODE_MAP32:
	case CANVAS_MODE_MAP64:
		CanvasMapReadPanelInfo(flag);
		break;
	case CANVAS_MODE_KBD_A:
	case CANVAS_MODE_KBD_B:
	case CANVAS_MODE_KBD_C:
	case CANVAS_MODE_KBD_D:
		CanvasKeyboardReadPanelInfo(flag);
		break;
	case CANVAS_MODE_SLEEP:
//		CanvasSleepReadPanelInfo(flag);
		break;
	}
}

void CanvasChange(int mode)
{
	if (mode != 0)
		Canvas.Mode = mode;
	else {
		if (Canvas.Mode == CANVAS_MODE_SLEEP)
			Canvas.Mode = CANVAS_MODE_GSLCD;
		else if (Canvas.Mode == CANVAS_MODE_GSLCD) {
			Canvas.MapMode = CMAP_MODE_16;
			Canvas.Mode = CANVAS_MODE_MAP16;
		} else if (Canvas.Mode == CANVAS_MODE_MAP16) {
			Canvas.MapMode = CMAP_MODE_32;
			Canvas.Mode = CANVAS_MODE_MAP32;
		} else if (Canvas.Mode == CANVAS_MODE_MAP32) {
			Canvas.MapMode = CMAP_MODE_64;
			Canvas.Mode = CANVAS_MODE_MAP64;
		} else if (Canvas.Mode == CANVAS_MODE_MAP64) {
			Canvas.CKPart = 1;
			Canvas.Mode = CANVAS_MODE_KBD_A;
		} else if (Canvas.Mode == CANVAS_MODE_KBD_A) {
			Canvas.CKPart = 2;
			Canvas.Mode = CANVAS_MODE_KBD_B;
		} else if (Canvas.Mode == CANVAS_MODE_KBD_B) {
			Canvas.CKPart = 3;
			Canvas.Mode = CANVAS_MODE_KBD_C;
		} else if (Canvas.Mode == CANVAS_MODE_KBD_C) {
			Canvas.CKPart = 4;
			Canvas.Mode = CANVAS_MODE_KBD_D;
		} else if (Canvas.Mode == CANVAS_MODE_KBD_D)
			Canvas.Mode = CANVAS_MODE_SLEEP;
	}
	MainWndInfo.CanvasMode = Canvas.Mode;
	CanvasReset();
	CanvasClear();
	CanvasReadPanelInfo(1);
	CanvasUpdate(1);
	CanvasPaintAll();
}

int CanvasGetMode(void)
{
	return Canvas.Mode;
}




























//-----------------------------------------------------------------------------
// Main Panel
//  メインパネルウインドウ関連
//
//
//
//
//
//
//

#define MPANEL_XMAX 440
#define MPANEL_YMAX 88

// update flag.
#define MP_UPDATE_ALL		    0xffffL
#define MP_UPDATE_NONE		    0x0000L
#define MP_UPDATE_TITLE		    0x0001L
#define MP_UPDATE_FILE		    0x0002L
#define MP_UPDATE_TIME			0x0004L
#define MP_UPDATE_METRONOME		0x0008L
#define MP_UPDATE_VOICES		0x0010L
#define MP_UPDATE_MVOLUME		0x0020L
#define MP_UPDATE_RATE			0x0040L
#define MP_UPDATE_PLAYLIST		0x0080L
///r
#define MP_UPDATE_AQ_RATIO		0x0100L

#define MP_UPDATE_MISC		    0x0200L
#define MP_UPDATE_MESSAGE	    0x0400L
#define MP_UPDATE_BACKGROUND	0x0800L
#define MP_UPDATE_KEYSIG		0x1000L
#define MP_UPDATE_TEMPO			0x2000L

#define MP_TITLE_MAX	256
#define MP_FILE_MAX		256
struct MPanel_ {
	HWND hwnd;
	HWND hParentWnd;
	HDC hdc;
   HDC hmdc;
	HGDIOBJ hgdiobj_hmdcprev;
	HBITMAP hbitmap;
	HBITMAP hbitmapBG;			/* the background bitmap */
	HBITMAP hbitmapBGFilter;	/* the background bitmap filter */
	HFONT hfont;
	TCHAR Font[256];
	TCHAR FontLang[256];
	TCHAR FontLangFixed[256];
	RECT rcMe;
	RECT rcTitle;
	RECT rcFile;
	RECT rcTime;
	RECT rcVoices;
	RECT rcMVolume;
	RECT rcRate;
	RECT rcMetronome;
	RECT rcKeysig;
	RECT rcTempo;
	RECT rcList;
	RECT rcMisc;
	RECT rcMessage;
	char Title[MP_TITLE_MAX+1];
	char File[MP_FILE_MAX+1];
	int CurTime_h; int CurTime_m; int CurTime_s; int CurTime_ss;
	int TotalTime_h; int TotalTime_m; int TotalTime_s; int TotalTime_ss;
	int CurVoices;
	int MaxVoices;
   int MVolume;
	int Rate;
	int Meas;
	int Beat;
	char Keysig[7];
	int Key_offset;
	int Tempo;
	int Tempo_ratio;
	int PlaylistNum;
	int PlaylistMax;
	HFONT hFontTitle;
	HFONT hFontFile;
	HFONT hFontTime;
	HFONT hFontVoices;
	HFONT hFontMVolume;
	HFONT hFontRate;
	HFONT hFontMetronome;
	HFONT hFontKeysig;
	HFONT hFontTempo;
	HFONT hFontList;
	HFONT hFontMisc;
	HFONT hFontMessage;
	long UpdateFlag;
	COLORREF FGColor;
	COLORREF BGColor;
	COLORREF BGBGColor;
	enum play_system_modes play_system_mode;
	int current_file_info_file_type;
	int current_file_info_max_channel;
///r
	HFONT hFontAQ_RATIO;
	RECT rcAQ_RATIO;
	int aq_ratio;
	char rq_flag[2];
	char rv_flag[2];
	char rp_flag[2];

} MPanel;
extern volatile int MPanelOK;

static struct MPanelMessageData_ {
	int len;	// メッセージボックスの長さ。
	char buff[1024];	// 実バッファ。
	DWORD prevtime;
	int msec;	// 実残り秒。
	int pointer;		// 現在のポインタ。

	char curbuff[1024];
	int curbuffsize;
	int curmode;	// 現在メッセージのモード。
	int curmsec;		// 現在メッセージの残り秒。
	char nextbuff[1024];
	int nextmode;	// 現在メッセージのモード。
	int nextmsec;		// 現在メッセージの残り秒。
} MPanelMessageData;
void MPanelMessageInit(void);
void MPanelMessageAdd(char *message, int msec, int mode);
void MPanelMessageClearAll(void);
void MPanelMessageClear(void);
void MPanelMessageNext(void);
void MPanelMessageUpdate(void);

static HWND hPanelWnd;
static TCHAR PanelWndClassName[] = _T("TiMidity Main Panel");
static LRESULT CALLBACK PanelWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
int MPanelMode = 0;

static void InitPanelWnd(HWND hwnd)
{
	WNDCLASS wndclass ;
	hPanelWnd = 0;
	wndclass.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS | CS_CLASSDC;
	wndclass.lpfnWndProc   = PanelWndProc ;
	wndclass.cbClsExtra    = 0 ;
	wndclass.cbWndExtra    = 0 ;
	wndclass.hInstance     = hInst ;
	wndclass.hIcon         = NULL;
	wndclass.hCursor       = LoadCursor(0,IDC_ARROW) ;
	wndclass.hbrBackground = (HBRUSH)(COLOR_SCROLLBAR + 1);
	wndclass.lpszMenuName  = NULL;
	wndclass.lpszClassName = PanelWndClassName;
	RegisterClass(&wndclass);
  	hPanelWnd =
  	CreateWindowEx(0,PanelWndClassName,0,WS_CHILD,
    	CW_USEDEFAULT,0,MPANEL_XMAX,MPANEL_YMAX,
      GetDlgItem(hwnd,IDC_RECT_PANEL),0,hInst,0);
	MPanelInit(hPanelWnd);
	MPanelReset();
	MPanelReadPanelInfo(1);
	MPanelUpdateAll();
  	MPanelPaintAll();
	UpdateWindow(hPanelWnd);
	ShowWindow(hPanelWnd,SW_SHOW);
}

static void MPanelPaintDo(void);
static LRESULT CALLBACK
PanelWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess)
	{
		case WM_CREATE:
			break;
		case WM_PAINT:
      	MPanelPaintDo();
	    	return 0;
		case WM_LBUTTONDBLCLK:
//			MPanelReset();
		  	MPanelReadPanelInfo(1);
			MPanelUpdateAll();
		  	MPanelPaintAll();
			break;
		case WM_DESTROY:
			if(MPanel.hFontTitle!=NULL)
				DeleteObject(MPanel.hFontTitle);
			if(MPanel.hFontFile!=NULL)
				DeleteObject(MPanel.hFontFile);
			if(MPanel.hFontTime!=NULL)
				DeleteObject(MPanel.hFontTime);
			if(MPanel.hFontVoices!=NULL)
				DeleteObject(MPanel.hFontVoices);
			if(MPanel.hFontMVolume!=NULL)
				DeleteObject(MPanel.hFontMVolume);
			if(MPanel.hFontRate!=NULL)
				DeleteObject(MPanel.hFontRate);
			if (MPanel.hFontMetronome != NULL)
				DeleteObject(MPanel.hFontMetronome);
			if (MPanel.hFontKeysig != NULL)
				DeleteObject(MPanel.hFontKeysig);
			if (MPanel.hFontTempo != NULL)
				DeleteObject(MPanel.hFontTempo);
///r
			if(MPanel.hFontAQ_RATIO!=NULL)
				DeleteObject(MPanel.hFontAQ_RATIO);
	//		if(MPanel.hFontList!=NULL)
	//			DeleteObject(MPanel.hFontList);

			if(MPanel.hFontMisc!=NULL)
				DeleteObject(MPanel.hFontMisc);
			if(MPanel.hFontMessage!=NULL)
				DeleteObject(MPanel.hFontMessage);
			break;
		default:
			return DefWindowProc(hwnd,uMess,wParam,lParam) ;
	}
	return 0L;
}

// Initialization of MPanel strucuter at once.
volatile int MPanelOK = 0;
static void MPanelInit(HWND hwnd)
{
	RECT rc;
	int tmp;
	GDI_LOCK(); // gdi_lock
	MPanel.hwnd = hwnd;
	MPanel.hParentWnd = GetParent(MPanel.hwnd);
	GetClientRect(MPanel.hParentWnd,&rc);
	MoveWindow(MPanel.hwnd,0,0,rc.right-rc.left,rc.bottom-rc.top,FALSE);
	MPanel.hdc = GetDC(MPanel.hwnd);
	MPanel.hbitmap =
  		CreateCompatibleBitmap(MPanel.hdc,MPANEL_XMAX,MPANEL_YMAX);
	MPanel.hmdc =
    	CreateCompatibleDC(MPanel.hdc);
	MPanel.hgdiobj_hmdcprev = SelectObject(MPanel.hmdc,MPanel.hbitmap);
	ReleaseDC(MPanel.hwnd,MPanel.hdc);

	GetClientRect(MPanel.hwnd,&rc);
	// RECT reft,top,right,bottom
	SetRect(&(MPanel.rcMe),rc.left,rc.top,rc.right,rc.bottom);
	rc = MPanel.rcMe;
	SetRect(&(MPanel.rcTitle),	rc.left+2,	rc.top+2,				rc.right-2,				rc.top+2+14);
	SetRect(&(MPanel.rcFile),	rc.left+2,	rc.top+2+14+1,		rc.right-2,				rc.top+2+14+1+12);
#if 0
	SetRect(&(MPanel.rcTime),	rc.left+2,	rc.top+2+14+1+12+1,	rc.left+2+180,		rc.top+2+14+1+12+1+25);
	SetRect(&(MPanel.rcVoices), rc.right-2-36-1-24-48,	rc.top+2+14+1+12+1,	rc.right-2-36-1-24,	rc.top+2+14+1+12+1+12);
	SetRect(&(MPanel.rcMVolume),rc.right-2-36,			rc.top+2+14+1+12+1,	rc.right-2,				rc.top+2+14+1+12+1+12);
	SetRect(&(MPanel.rcRate), 	rc.right-2-60-1-18-48,	rc.top+2+14+1+12+1+12+1,	rc.right-2-36-1,	rc.top+2+14+1+12+1+12+1+12);
	SetRect(&(MPanel.rcList),	rc.right-2-60,				rc.top+2+14+1+12+1+12+1,	rc.right-2,				rc.top+2+14+1+12+1+12+1+12);
	SetRect(&(MPanel.rcMisc),	rc.left+2,	rc.top+2+14+1+12+1+25+1,rc.right-2,rc.top+2+14+1+12+1+25+1+12);
	SetRect(&(MPanel.rcMessage),rc.left,rc.top,rc.right,rc.bottom);
#else
	SetRect(&(MPanel.rcTime),
			rc.left + 2,
			rc.top + 2 + 14 + 1 + 12 + 1,
			rc.left + 2 + 176,
			rc.top + 2 + 14 + 1 + 12 + 1 + 25);
	SetRect(&(MPanel.rcVoices),
			rc.left + 2 + 176 + 2,
			rc.top + 2 + 14 + 1 + 12 + 1,
			rc.left + 2 + 176 + 2 + 50,
			rc.top + 2 + 14 + 1 + 12 + 1 + 12);
	SetRect(&(MPanel.rcRate),
			rc.left + 2 + 176 + 2 + 50 + 2,
			rc.top + 2 + 14 + 1 + 12 + 1,
			rc.right - 2 - 38 - 2,
			rc.top + 2 + 14 + 1 + 12 + 1 + 12);
	SetRect(&(MPanel.rcMVolume),
			rc.right - 2 - 38,
			rc.top + 2 + 14 + 1 + 12 + 1,
			rc.right - 2,
			rc.top + 2 + 14 + 1 + 12 + 1 + 12);
	SetRect(&(MPanel.rcMetronome),
			rc.left + 2 + 176 - 40,
			rc.top + 2 + 14 + 1 + 12 + 1 + 12 + 1 + 12 + 1,
			rc.left + 2 + 176,
			rc.top + 2 + 14 + 1 + 12 + 1 + 12 + 1 + 12 + 1 + 12);
	SetRect(&(MPanel.rcKeysig),
			rc.left + 2 + 176 + 2,
			rc.top + 2 + 14 + 1 + 12 + 1 + 12 + 1,
			rc.right - 2 - 69 - 2,
			rc.top + 2 + 14 + 1 + 12 + 1 + 12 + 1 + 12);
	SetRect(&(MPanel.rcTempo),
			rc.right - 2 - 69,
			rc.top + 2 + 14 + 1 + 12 + 1 + 12 + 1,
			rc.right - 2,
			rc.top + 2 + 14 + 1 + 12 + 1 + 12 + 1 + 12);
	SetRect(&(MPanel.rcMisc),
			rc.left + 2 + 176 + 2,
			rc.top + 2 + 14 + 1 + 12 + 1 + 12 + 1 + 12 + 1,
			rc.right - 2 - 54 - 2,
			rc.top + 2 + 14 + 1 + 12 + 1 + 12 + 1 + 12 + 1 + 12);
///r
// test rcAQ_RATIO replace rcList
	SetRect(&(MPanel.rcAQ_RATIO),
			rc.right - 2 - 54,
			rc.top + 2 + 14 + 1 + 12 + 1 + 12 + 1 + 12 + 1,
			rc.right - 2,
			rc.top + 2 + 14 + 1 + 12 + 1 + 12 + 1 + 12 + 1 + 12);
	//SetRect(&(MPanel.rcList),
	//		rc.right - 2 - 54,
	//		rc.top + 2 + 14 + 1 + 12 + 1 + 12 + 1 + 12 + 1,
	//		rc.right - 2,
	//		rc.top + 2 + 14 + 1 + 12 + 1 + 12 + 1 + 12 + 1 + 12);

	SetRect(&(MPanel.rcMessage),
			rc.left + 2,
			rc.top + 2 + 14 + 1 + 12 + 1 + 25 + 1,
			rc.left + 2 + 176 - 40 - 2,
			rc.top + 2 + 14 + 1 + 12 + 1 + 25 + 1 + 12);
#endif
	MPanel.hFontTitle = NULL;
	MPanel.hFontFile = NULL;
	MPanel.hFontTime = NULL;
	MPanel.hFontVoices = NULL;
	MPanel.hFontMVolume = NULL;
	MPanel.hFontRate = NULL;
	MPanel.hFontMetronome = NULL;
	MPanel.hFontKeysig = NULL;
	MPanel.hFontTempo = NULL;
///r
	MPanel.hFontAQ_RATIO = NULL;
//	MPanel.hFontList = NULL;


//	_tcscpy(MPanel.Font,_T("Times New Roman"));
	_tcscpy(MPanel.Font,_T("Arial Bold"));
	switch(PlayerLanguage){
   case LANGUAGE_ENGLISH:
		_tcscpy(MPanel.FontLang,_T("Times New Roman"));
		_tcscpy(MPanel.FontLangFixed,_T("Times New Roman"));
     	break;
	default:
	case LANGUAGE_JAPANESE:
		_tcscpy(MPanel.FontLang,_T("ＭＳ Ｐ明朝"));
		_tcscpy(MPanel.FontLangFixed,_T("ＭＳ 明朝"));
//		strcpy(MPanel.FontLang,"ＭＳ Ｐゴシック");
//		strcpy(MPanel.FontLangFixed,"ＭＳ ゴシック");
		break;
	}
	rc = MPanel.rcTitle;
	MPanel.hFontTitle =
		CreateFont(rc.bottom-rc.top+1,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,
			DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
      	DEFAULT_PITCH | FF_DONTCARE,MPanel.FontLang);
	rc = MPanel.rcFile;
	MPanel.hFontFile =
		CreateFont(rc.bottom-rc.top+1,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,
			DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
      	DEFAULT_PITCH | FF_DONTCARE,MPanel.FontLang);
	rc = MPanel.rcTime;
	MPanel.hFontTime =
		CreateFont(24,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,
			DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
      	DEFAULT_PITCH | FF_DONTCARE,MPanel.Font);
	rc = MPanel.rcVoices;
	MPanel.hFontVoices =
		CreateFont(rc.bottom-rc.top+1,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,
			DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
      	DEFAULT_PITCH | FF_DONTCARE,MPanel.Font);
	rc = MPanel.rcMVolume;
	MPanel.hFontMVolume =
		CreateFont(rc.bottom-rc.top+1,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,
			DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
      	DEFAULT_PITCH | FF_DONTCARE,MPanel.Font);
	rc = MPanel.rcRate;
	MPanel.hFontRate =
		CreateFont(rc.bottom-rc.top+1,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,
			DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
      	DEFAULT_PITCH | FF_DONTCARE,MPanel.Font);
	rc = MPanel.rcMetronome;
	MPanel.hFontMetronome =
			CreateFont(rc.bottom - rc.top + 1, 0, 0, 0, FW_DONTCARE,
			FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
			DEFAULT_PITCH | FF_DONTCARE, MPanel.Font);
	rc = MPanel.rcKeysig;
	tmp = (rc.bottom - rc.top + 1) / 2;
	MPanel.hFontKeysig =
			CreateFont(tmp * 2, tmp, 0, 0, FW_DONTCARE,
			FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
			FIXED_PITCH | FF_DONTCARE, MPanel.Font);
	rc = MPanel.rcTempo;
	tmp = (rc.bottom - rc.top + 1) / 2;
	MPanel.hFontTempo =
			CreateFont(tmp * 2, tmp, 0, 0, FW_DONTCARE,
			FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
			FIXED_PITCH | FF_DONTCARE, MPanel.Font);
///r
    rc = MPanel.rcAQ_RATIO;
    MPanel.hFontAQ_RATIO =
        CreateFont(rc.bottom-rc.top+1,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,MPanel.Font);
    //rc = MPanel.rcList;
    //MPanel.hFontList =
    //    CreateFontA(rc.bottom-rc.top+1,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,
    //        DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
    //        DEFAULT_PITCH | FF_DONTCARE,MPanel.Font);

    rc = MPanel.rcMisc;
    tmp = (rc.bottom-rc.top+1)/2;
    MPanel.hFontMisc =
        CreateFont(tmp*2,tmp,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
            FIXED_PITCH | FF_DONTCARE,MPanel.Font);
    rc = MPanel.rcMessage;
    tmp = (rc.bottom-rc.top+1)/2;
    MPanel.hFontMessage =
        CreateFont(tmp*2,tmp,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
            FIXED_PITCH | FF_DONTCARE,MPanel.FontLangFixed);

	MPanelOK = 1;
	GDI_UNLOCK(); // gdi_lock
	MPanelMessageInit();
}

void MPanelResize(void)
{
    RECT rc;

    GetClientRect(MPanel.hParentWnd,&rc);
    MoveWindow(MPanel.hwnd,0,0,rc.right-rc.left,rc.bottom-rc.top,FALSE);

    //GetClientRect(MPanel.hwnd,&rc);
    //RECT reft,top,right,bottom
    SetRect(&(MPanel.rcMe),rc.left,rc.top,rc.right,rc.bottom);
    rc = MPanel.rcMe;
    SetRect(&(MPanel.rcTitle),  rc.left+2,              rc.top+2,               rc.right-2,         rc.top+2+14);
    SetRect(&(MPanel.rcFile),   rc.left+2,              rc.top+2+14+1,          rc.right-2,         rc.top+2+14+1+12);

    // 3-4
    SetRect(&(MPanel.rcTime),//176
            rc.left + 2,
            rc.top + 2 + 14 + 1 + 12 + 1,
            rc.left + 2 + 176,
            rc.top + 2 + 14 + 1 + 12 + 1 + 25);
    // 3
    SetRect(&(MPanel.rcMVolume),//38
            rc.right - 2 - 38,
            rc.top + 2 + 14 + 1 + 12 + 1,
            rc.right - 2,
            rc.top + 2 + 14 + 1 + 12 + 1 + 12);
    SetRect(&(MPanel.rcRate),//expand
            rc.left + 2 + 176 + 2 + 50 + 2,
            rc.top + 2 + 14 + 1 + 12 + 1,
            rc.right - 2 - 38 - 2,
            rc.top + 2 + 14 + 1 + 12 + 1 + 12);
    SetRect(&(MPanel.rcVoices),//50
            rc.left + 2 + 176 + 2,
            rc.top + 2 + 14 + 1 + 12 + 1,
            rc.left + 2 + 176 + 2 + 50,
            rc.top + 2 + 14 + 1 + 12 + 1 + 12);
    // 4
    SetRect(&(MPanel.rcTempo),//69
            rc.right - 2 - 69,
            rc.top + 2 + 14 + 1 + 12 + 1 + 12 + 1,
            rc.right - 2,
            rc.top + 2 + 14 + 1 + 12 + 1 + 12 + 1 + 12);
    SetRect(&(MPanel.rcKeysig),//expand
            rc.left + 2 + 176 + 2,
            rc.top + 2 + 14 + 1 + 12 + 1 + 12 + 1,
            rc.right - 2 - 69 - 2,
            rc.top + 2 + 14 + 1 + 12 + 1 + 12 + 1 + 12);
    // 5
///r
// test rcAQ_RATIO replace rcList
    SetRect(&(MPanel.rcAQ_RATIO),//54
            rc.right - 2 - 54,
            rc.top + 2 + 14 + 1 + 12 + 1 + 12 + 1 + 12 + 1,
            rc.right - 2,
            rc.top + 2 + 14 + 1 + 12 + 1 + 12 + 1 + 12 + 1 + 12);
    //SetRect(&(MPanel.rcList),
    //        rc.right - 2 - 54,
    //        rc.top + 2 + 14 + 1 + 12 + 1 + 12 + 1 + 12 + 1,
    //        rc.right - 2,
    //        rc.top + 2 + 14 + 1 + 12 + 1 + 12 + 1 + 12 + 1 + 12);
    SetRect(&(MPanel.rcMisc),//100
            rc.right - 2 - 54 - 2 - 100,
            rc.top + 2 + 14 + 1 + 12 + 1 + 12 + 1 + 12 + 1,
            rc.right - 2 - 54 - 2,
            rc.top + 2 + 14 + 1 + 12 + 1 + 12 + 1 + 12 + 1 + 12);
    SetRect(&(MPanel.rcMetronome),//40
            rc.right - 2 - 54 - 2 - 100 - 2 - 40,
            rc.top + 2 + 14 + 1 + 12 + 1 + 12 + 1 + 12 + 1,
            rc.right - 2 - 54 - 2 - 100 - 2,
            rc.top + 2 + 14 + 1 + 12 + 1 + 12 + 1 + 12 + 1 + 12);
    SetRect(&(MPanel.rcMessage),//expand
            rc.left + 2,
            rc.top + 2 + 14 + 1 + 12 + 1 + 12 + 1 + 12 + 1,
            rc.right - 2 - 54 - 2 - 100 - 2 - 40 - 2,
            rc.top + 2 + 14 + 1 + 12 + 1 + 12 + 1 + 12 + 1 + 12);
}

// パネル構造体をリセットする。
void MPanelReset(void)
{
	if(!MPanelOK)
		return;
	MPanel.Title[0] = '\0';
	MPanel.File[0] = '\0';
	strcpy(MPanel.Title,"No title.");
	strcpy(MPanel.File,"No file.");
	MPanel.CurTime_h = 0;
	MPanel.CurTime_m = 0;
	MPanel.CurTime_s = 0;
	MPanel.CurTime_ss = 0;
	MPanel.TotalTime_h = 0;
	MPanel.TotalTime_m = 0;
	MPanel.TotalTime_s = 0;
	MPanel.TotalTime_ss = 0;
	MPanel.CurVoices = 0;
	MPanel.MaxVoices = 0;
	MPanel.MVolume = 0;
	MPanel.Rate = 0;
	MPanel.Meas = 0;
	MPanel.Beat = 0;
	MPanel.Keysig[0] = '\0';
	MPanel.Key_offset = 0;
	MPanel.Tempo = 0;
	MPanel.Tempo_ratio = 0;
	MPanel.PlaylistNum = 0;
	MPanel.PlaylistMax = 0;
	MPanel.UpdateFlag = MP_UPDATE_ALL;
//	MPanel.FGColor = RGB(0x00,0x00,0x00);
//	MPanel.BGColor = RGB(0xff,0xff,0xff);
	MPanel.FGColor = RGB(0x00,0x00,0x00);
//	MPanel.BGColor = RGB(0xc0,0xc0,0xc0);
	MPanel.BGColor = RGB(0xc0,0xc5,0xc3);
	MPanel.BGBGColor = RGB(0x60,0x60,0x60);
#if 0
	if(MPanel.hFontTitle!=NULL)
   	DeleteObject(MPanel.hFontTitle);
	if(MPanel.hFontFile!=NULL)
   	DeleteObject(MPanel.hFontFile);
	if(MPanel.hFontTime!=NULL)
   	DeleteObject(MPanel.hFontTime);
	if(MPanel.hFontVoices!=NULL)
   	DeleteObject(MPanel.hFontVoices);
	if(MPanel.hFontMVolume!=NULL)
   	DeleteObject(MPanel.hFontMVolume);
	if(MPanel.hFontRate!=NULL)
   	DeleteObject(MPanel.hFontRate);
	if (MPanel.hFontMetronome != NULL)
		DeleteObject(MPanel.hFontMetronome);
	if (MPanel.hFontKeysig != NULL)
		DeleteObject(MPanel.hFontKeysig);
	if (MPanel.hFontTempo != NULL)
		DeleteObject(MPanel.hFontTempo);
	if(MPanel.hFontList!=NULL)
   	DeleteObject(MPanel.hFontList);
#endif
	MPanel.play_system_mode = DEFAULT_SYSTEM_MODE;
	MPanel.current_file_info_file_type = IS_OTHER_FILE;
	MPanel.current_file_info_max_channel = -1;
	MPanelMessageClearAll();
///r
	MPanel.aq_ratio = 0;
	MPanel.rv_flag[0] = ' ';
	MPanel.rv_flag[1] = '\0';
	MPanel.rq_flag[0] = ' ';
	MPanel.rq_flag[1] = '\0';
	MPanel.rp_flag[0] = ' ';
	MPanel.rp_flag[1] = '\0';
}

// パネル構造体を元に更新する。
void MPanelUpdate(void)
{
	if(!MPanelOK)
		return;
	MPanelMessageUpdate();
	if(MPanel.UpdateFlag==MP_UPDATE_NONE)
	   	return;
	if(MPanel.UpdateFlag & MP_UPDATE_BACKGROUND){
		// ビットマップを貼り付けるが今は塗りつぶし。
		HPEN hPen;
      HBRUSH hBrush;
//		COLORREF color = MPanel.FGColor;
		COLORREF color = MPanel.BGBGColor;
		RECT rc = MPanel.rcMe;
		HGDIOBJ hgdiobj_hpen, hgdiobj_hbrush;
		GDI_LOCK(); // gdi_lock
		hPen = CreatePen(PS_SOLID,1,color);
		hBrush = CreateSolidBrush(color);
		hgdiobj_hpen = SelectObject(MPanel.hmdc, hPen);
		hgdiobj_hbrush = SelectObject(MPanel.hmdc, hBrush);
		Rectangle(MPanel.hmdc,rc.left,rc.top,rc.right,rc.bottom);
		SelectObject(MPanel.hmdc, hgdiobj_hpen);
		DeleteObject(hPen);
		SelectObject(MPanel.hmdc, hgdiobj_hbrush);
		DeleteObject(hBrush);
		GDI_UNLOCK(); // gdi_lock
		InvalidateRect(MPanel.hwnd,&rc, FALSE);
   }
	if(MPanel.UpdateFlag & MP_UPDATE_TITLE){
		HGDIOBJ hgdiobj = SelectObject(MPanel.hmdc,MPanel.hFontTitle);
		GDI_LOCK(); // gdi_lock
		SetTextColor(MPanel.hmdc,MPanel.FGColor);
		SetBkColor(MPanel.hmdc,MPanel.BGColor);
	//#include "w32g2_c.h"
	SetTextAlign(MPanel.hmdc, TA_LEFT | TA_TOP | TA_NOUPDATECP);
	TCHAR *ttitle = char_to_tchar(MPanel.Title);
		ExtTextOut(MPanel.hmdc,MPanel.rcTitle.left,MPanel.rcTitle.top,
    		ETO_CLIPPED	| ETO_OPAQUE,&(MPanel.rcTitle),
			ttitle,_tcslen(ttitle),NULL);
		safe_free(ttitle);
		if((HGDIOBJ)hgdiobj!=(HGDIOBJ)NULL && (HGDIOBJ)hgdiobj!=(HGDIOBJ)GDI_ERROR)
			SelectObject(MPanel.hmdc,hgdiobj);
		GDI_UNLOCK(); // gdi_lock
		InvalidateRect(hPanelWnd, &(MPanel.rcTitle), FALSE);
	}
	if(MPanel.UpdateFlag & MP_UPDATE_FILE){
		HGDIOBJ hgdiobj;
		GDI_LOCK(); // gdi_lock
		hgdiobj = SelectObject(MPanel.hmdc,MPanel.hFontFile);
		SetTextColor(MPanel.hmdc,MPanel.FGColor);
		SetBkColor(MPanel.hmdc,MPanel.BGColor);
		SetTextAlign(MPanel.hmdc, TA_LEFT | TA_TOP | TA_NOUPDATECP);
		TCHAR *t = char_to_tchar(MPanel.File);
		ExtTextOut(MPanel.hmdc,MPanel.rcFile.left,MPanel.rcFile.top,
    		ETO_CLIPPED	| ETO_OPAQUE,&(MPanel.rcFile),
    		t,_tcslen(t),NULL);
		safe_free(t);
		if((HGDIOBJ)hgdiobj!=(HGDIOBJ)NULL && (HGDIOBJ)hgdiobj!=(HGDIOBJ)GDI_ERROR)
			SelectObject(MPanel.hmdc,hgdiobj);
		GDI_UNLOCK(); // gdi_lock
		InvalidateRect(hPanelWnd, &(MPanel.rcFile), FALSE);
   }
	if(MPanel.UpdateFlag & MP_UPDATE_TIME){
		char buffer[256];
		HGDIOBJ hgdiobj;
		GDI_LOCK(); // gdi_lock
		hgdiobj = SelectObject(MPanel.hmdc,MPanel.hFontTime);
   	sprintf(buffer," %02d:%02d:%02d/%02d:%02d:%02d",
			MPanel.CurTime_h,MPanel.CurTime_m,MPanel.CurTime_s,
    		MPanel.TotalTime_h,MPanel.TotalTime_m,MPanel.TotalTime_s);
		SetTextColor(MPanel.hmdc,MPanel.FGColor);
		SetBkColor(MPanel.hmdc,MPanel.BGColor);
		SetTextAlign(MPanel.hmdc, TA_LEFT | TA_TOP | TA_NOUPDATECP);
		ExtTextOutA(MPanel.hmdc,MPanel.rcTime.left,MPanel.rcTime.top,
    		ETO_CLIPPED	| ETO_OPAQUE,&(MPanel.rcTime),
    		buffer,strlen(buffer),NULL);
		if((HGDIOBJ)hgdiobj!=(HGDIOBJ)NULL && (HGDIOBJ)hgdiobj!=(HGDIOBJ)GDI_ERROR)
			SelectObject(MPanel.hmdc,hgdiobj);
		GDI_UNLOCK(); // gdi_lock
		InvalidateRect(hPanelWnd, &(MPanel.rcTime), FALSE);
   }
	if(MPanel.UpdateFlag & MP_UPDATE_VOICES){
		char buffer[256];
		HGDIOBJ hgdiobj;
		GDI_LOCK(); // gdi_lock
		hgdiobj = SelectObject(MPanel.hmdc,MPanel.hFontVoices);
   	sprintf(buffer," %03d/%03d",MPanel.CurVoices,MPanel.MaxVoices);
		SetTextColor(MPanel.hmdc,MPanel.FGColor);
		SetBkColor(MPanel.hmdc,MPanel.BGColor);
		SetTextAlign(MPanel.hmdc, TA_LEFT | TA_TOP | TA_NOUPDATECP);
		ExtTextOutA(MPanel.hmdc,MPanel.rcVoices.left,MPanel.rcVoices.top,
    		ETO_CLIPPED	| ETO_OPAQUE,&(MPanel.rcVoices),
    		buffer,strlen(buffer),NULL);
		if((HGDIOBJ)hgdiobj!=(HGDIOBJ)NULL && (HGDIOBJ)hgdiobj!=(HGDIOBJ)GDI_ERROR)
			SelectObject(MPanel.hmdc,hgdiobj);
		GDI_UNLOCK(); // gdi_lock
		InvalidateRect(hPanelWnd, &(MPanel.rcVoices), FALSE);
   }
	if(MPanel.UpdateFlag & MP_UPDATE_MVOLUME){
		char buffer[256];
		HGDIOBJ hgdiobj;
		GDI_LOCK(); // gdi_lock
		hgdiobj = SelectObject(MPanel.hmdc,MPanel.hFontVoices);
   	sprintf(buffer," %03d%%",MPanel.MVolume);
		SetTextColor(MPanel.hmdc,MPanel.FGColor);
		SetBkColor(MPanel.hmdc,MPanel.BGColor);
		SetTextAlign(MPanel.hmdc, TA_LEFT | TA_TOP | TA_NOUPDATECP);
		ExtTextOutA(MPanel.hmdc,MPanel.rcMVolume.left,MPanel.rcMVolume.top,
    		ETO_CLIPPED	| ETO_OPAQUE,&(MPanel.rcMVolume),
    		buffer,strlen(buffer),NULL);
		if((HGDIOBJ)hgdiobj!=(HGDIOBJ)NULL && (HGDIOBJ)hgdiobj!=(HGDIOBJ)GDI_ERROR)
			SelectObject(MPanel.hmdc,hgdiobj);
		GDI_UNLOCK(); // gdi_lock
		InvalidateRect(hPanelWnd, &(MPanel.rcMVolume), FALSE);
   }
	if(MPanel.UpdateFlag & MP_UPDATE_RATE){
		char buffer[256];
		HGDIOBJ hgdiobj;
		GDI_LOCK(); // gdi_lock
		hgdiobj = SelectObject(MPanel.hmdc,MPanel.hFontRate);
   	sprintf(buffer," %05dHz",MPanel.Rate);
		SetTextColor(MPanel.hmdc,MPanel.FGColor);
		SetBkColor(MPanel.hmdc,MPanel.BGColor);
		SetTextAlign(MPanel.hmdc, TA_LEFT | TA_TOP | TA_NOUPDATECP);
		ExtTextOutA(MPanel.hmdc,MPanel.rcRate.left,MPanel.rcRate.top,
    		ETO_CLIPPED	| ETO_OPAQUE,&(MPanel.rcRate),
    		buffer,strlen(buffer),NULL);
		if((HGDIOBJ)hgdiobj!=(HGDIOBJ)NULL && (HGDIOBJ)hgdiobj!=(HGDIOBJ)GDI_ERROR)
			SelectObject(MPanel.hmdc,hgdiobj);
		GDI_UNLOCK(); // gdi_lock
		InvalidateRect(hPanelWnd, &(MPanel.rcRate), FALSE);
   }
	if (MPanel.UpdateFlag & MP_UPDATE_METRONOME) {
		char buffer[256];
		HGDIOBJ hgdiobj;
		
		GDI_LOCK(); // gdi_lock
		hgdiobj = SelectObject(MPanel.hmdc, MPanel.hFontMetronome);
		sprintf(buffer, " %03d.%02d", MPanel.Meas, MPanel.Beat);
		SetTextColor(MPanel.hmdc, MPanel.FGColor);
		SetBkColor(MPanel.hmdc, MPanel.BGColor);
		SetTextAlign(MPanel.hmdc, TA_LEFT | TA_TOP | TA_NOUPDATECP);
		ExtTextOutA(MPanel.hmdc,
				MPanel.rcMetronome.left, MPanel.rcMetronome.top,
				ETO_CLIPPED | ETO_OPAQUE, &(MPanel.rcMetronome),
				buffer, strlen(buffer), NULL);
		if ((HGDIOBJ) hgdiobj != (HGDIOBJ) NULL
				&& (HGDIOBJ) hgdiobj != (HGDIOBJ) GDI_ERROR)
			SelectObject(MPanel.hmdc, hgdiobj);
		GDI_UNLOCK(); // gdi_lock
		InvalidateRect(hPanelWnd, &(MPanel.rcMetronome), FALSE);
	}
	if (MPanel.UpdateFlag & MP_UPDATE_KEYSIG) {
		char buffer[256];
		HGDIOBJ hgdiobj;
		
		GDI_LOCK(); // gdi_lock
		hgdiobj = SelectObject(MPanel.hmdc, MPanel.hFontKeysig);
		if (MPanel.Keysig[0] == '\0')
			strcpy(MPanel.Keysig, "-- ---");
		sprintf(buffer, "%s (%+03d)", MPanel.Keysig, MPanel.Key_offset);
		SetTextColor(MPanel.hmdc, MPanel.FGColor);
		SetBkColor(MPanel.hmdc, MPanel.BGColor);
		SetTextAlign(MPanel.hmdc, TA_LEFT | TA_TOP | TA_NOUPDATECP);
		ExtTextOutA(MPanel.hmdc,
				MPanel.rcKeysig.left + 2, MPanel.rcKeysig.top,
				ETO_CLIPPED | ETO_OPAQUE, &(MPanel.rcKeysig),
				buffer, strlen(buffer), NULL);
		if ((HGDIOBJ) hgdiobj != (HGDIOBJ) NULL
				&& (HGDIOBJ) hgdiobj != (HGDIOBJ) GDI_ERROR)
			SelectObject(MPanel.hmdc, hgdiobj);
		GDI_UNLOCK(); // gdi_lock
		InvalidateRect(hPanelWnd, &(MPanel.rcKeysig), FALSE);
	}
	if (MPanel.UpdateFlag & MP_UPDATE_TEMPO) {
		char buffer[256];
		HGDIOBJ hgdiobj;
		
		GDI_LOCK(); // gdi_lock
		hgdiobj = SelectObject(MPanel.hmdc, MPanel.hFontTempo);
		sprintf(buffer, "%3d (%03d %%)", MPanel.Tempo, MPanel.Tempo_ratio);
		SetTextColor(MPanel.hmdc, MPanel.FGColor);
		SetBkColor(MPanel.hmdc, MPanel.BGColor);
		SetTextAlign(MPanel.hmdc, TA_LEFT | TA_TOP | TA_NOUPDATECP);
		ExtTextOutA(MPanel.hmdc,
				MPanel.rcTempo.left + 2, MPanel.rcTempo.top,
				ETO_CLIPPED | ETO_OPAQUE, &(MPanel.rcTempo),
				buffer, strlen(buffer), NULL);
		if ((HGDIOBJ) hgdiobj != (HGDIOBJ) NULL
				&& (HGDIOBJ) hgdiobj != (HGDIOBJ) GDI_ERROR)
			SelectObject(MPanel.hmdc, hgdiobj);
		GDI_UNLOCK(); // gdi_lock
		InvalidateRect(hPanelWnd, &(MPanel.rcTempo), FALSE);
	}
///r
	if(MPanel.UpdateFlag & MP_UPDATE_AQ_RATIO){
		char buffer[256];
		HGDIOBJ hgdiobj;
		GDI_LOCK(); // gdi_lock
		hgdiobj = SelectObject(MPanel.hmdc,MPanel.hFontAQ_RATIO);
   		sprintf(buffer,"%04d%%%s%s%s",MPanel.aq_ratio,MPanel.rq_flag,MPanel.rv_flag,MPanel.rp_flag);
		SetTextColor(MPanel.hmdc,MPanel.FGColor);
		SetBkColor(MPanel.hmdc,MPanel.BGColor);
		SetTextAlign(MPanel.hmdc, TA_LEFT | TA_TOP | TA_NOUPDATECP);
		ExtTextOutA(MPanel.hmdc,MPanel.rcAQ_RATIO.left,MPanel.rcAQ_RATIO.top,
    		ETO_CLIPPED	| ETO_OPAQUE,&(MPanel.rcAQ_RATIO),
    		buffer,strlen(buffer),NULL);
		if((HGDIOBJ)hgdiobj!=(HGDIOBJ)NULL && (HGDIOBJ)hgdiobj!=(HGDIOBJ)GDI_ERROR)
			SelectObject(MPanel.hmdc,hgdiobj);
		GDI_UNLOCK(); // gdi_lock
		InvalidateRect(hPanelWnd, &(MPanel.rcAQ_RATIO), FALSE);
   }
	//if(MPanel.UpdateFlag & MP_UPDATE_PLAYLIST){
	//	char buffer[256];
	//	HGDIOBJ hgdiobj;
	//	GDI_LOCK(); // gdi_lock
	//	hgdiobj = SelectObject(MPanel.hmdc,MPanel.hFontList);
 //  	sprintf(buffer," %04d/%04d",MPanel.PlaylistNum,MPanel.PlaylistMax);
	//	SetTextColor(MPanel.hmdc,MPanel.FGColor);
	//	SetBkColor(MPanel.hmdc,MPanel.BGColor);
	//	SetTextAlign(MPanel.hmdc, TA_LEFT | TA_TOP | TA_NOUPDATECP);
	//	ExtTextOut(MPanel.hmdc,MPanel.rcList.left,MPanel.rcList.top,
 //   		ETO_CLIPPED	| ETO_OPAQUE,&(MPanel.rcList),
 //   		buffer,strlen(buffer),NULL);
	//	if((HGDIOBJ)hgdiobj!=(HGDIOBJ)NULL && (HGDIOBJ)hgdiobj!=(HGDIOBJ)GDI_ERROR)
	//		SelectObject(MPanel.hmdc,hgdiobj);
	//	GDI_UNLOCK(); // gdi_lock
	//	InvalidateRect(hPanelWnd, &(MPanel.rcList), FALSE);
 //  }
	if(MPanel.UpdateFlag & MP_UPDATE_MISC){
		char buffer[256];
		HGDIOBJ hgdiobj;
		GDI_LOCK(); // gdi_lock
		hgdiobj = SelectObject(MPanel.hmdc,MPanel.hFontMisc);
		buffer[0] = '\0';
		switch(MPanel.play_system_mode){
    	case GM_SYSTEM_MODE:
			strcat(buffer,"[GM]");
			break;
    	case GM2_SYSTEM_MODE:
			strcat(buffer,"[G2]");
			break;
    	case GS_SYSTEM_MODE:
			strcat(buffer,"[GS]");
			break;
    	case XG_SYSTEM_MODE:
			strcat(buffer,"[XG]");
			break;
    	case SD_SYSTEM_MODE:
			strcat(buffer,"[SD]");
			break;
    	case KG_SYSTEM_MODE:
			strcat(buffer,"[KG]");
			break;
    	case CM_SYSTEM_MODE:
			strcat(buffer,"[CM]");
			break;
		default:
		case DEFAULT_SYSTEM_MODE:
			strcat(buffer,"[--]");
			break;
		}
		switch(MPanel.current_file_info_file_type){
		case  IS_SMF_FILE:
			strcat(buffer,"[SMF]");
			break;
		case  IS_MCP_FILE:
			strcat(buffer,"[MCP]");
			break;
		case  IS_RCP_FILE:
			strcat(buffer,"[RCP]");
			break;
		case  IS_R36_FILE:
			strcat(buffer,"[R36]");
			break;
		case  IS_G18_FILE:
			strcat(buffer,"[G18]");
			break;
		case  IS_G36_FILE:
			strcat(buffer,"[G36]");
			break;
		case  IS_SNG_FILE:
			strcat(buffer,"[SNG]");
			break;
		case  IS_MM2_FILE:
			strcat(buffer,"[MM2]");
			break;
		case  IS_MML_FILE:
			strcat(buffer,"[MML]");
			break;
		case  IS_FM_FILE:
			strcat(buffer,"[FM ]");
			break;
		case  IS_FPD_FILE:
			strcat(buffer,"[FPD]");
			break;
		case  IS_MOD_FILE:
			strcat(buffer,"[MOD]");
			break;
		case  IS_669_FILE:
			strcat(buffer,"[669]");
			break;
		case  IS_MTM_FILE:
			strcat(buffer,"[MTM]");
			break;
		case  IS_STM_FILE:
			strcat(buffer,"[STM]");
			break;
		case  IS_S3M_FILE:
			strcat(buffer,"[S3M]");
			break;
		case  IS_ULT_FILE:
			strcat(buffer,"[ULT]");
			break;
		case  IS_XM_FILE:
			strcat(buffer,"[XM ]");
			break;
		case  IS_FAR_FILE:
			strcat(buffer,"[FAR]");
			break;
		case  IS_WOW_FILE:
			strcat(buffer,"[WOW]");
			break;
		case  IS_OKT_FILE:
			strcat(buffer,"[OKT]");
			break;
		case  IS_DMF_FILE:
			strcat(buffer,"[DMF]");
			break;
		case  IS_MED_FILE:
			strcat(buffer,"[MED]");
			break;
		case  IS_IT_FILE:
			strcat(buffer,"[IT ]");
			break;
		case  IS_PTM_FILE:
			strcat(buffer,"[PTM]");
			break;
		case  IS_MFI_FILE:
			strcat(buffer,"[MFI]");
			break;
		default:
		case  IS_OTHER_FILE:
			strcat(buffer,"[---]");
			break;
		}
		if(MPanel.current_file_info_max_channel>=0){
      	char local[16];
         sprintf(local,"[%02dch]",MPanel.current_file_info_max_channel+1);
			strcat(buffer,local);
      }else
			strcat(buffer,"[--ch]");
		SetTextColor(MPanel.hmdc,MPanel.FGColor);
		SetBkColor(MPanel.hmdc,MPanel.BGColor);
		SetTextAlign(MPanel.hmdc, TA_LEFT | TA_TOP | TA_NOUPDATECP);
		ExtTextOutA(MPanel.hmdc,MPanel.rcMisc.left,MPanel.rcMisc.top,
    		ETO_CLIPPED	| ETO_OPAQUE,&(MPanel.rcMisc),
    		buffer,strlen(buffer),NULL);
		if((HGDIOBJ)hgdiobj!=(HGDIOBJ)NULL && (HGDIOBJ)hgdiobj!=(HGDIOBJ)GDI_ERROR)
			SelectObject(MPanel.hmdc,hgdiobj);
		GDI_UNLOCK(); // gdi_lock
		InvalidateRect(hPanelWnd, &(MPanel.rcMisc), FALSE);
   }
   if(MPanel.UpdateFlag & MP_UPDATE_MESSAGE){
		HGDIOBJ hgdiobj;
		GDI_LOCK(); // gdi_lock
		hgdiobj = SelectObject(MPanel.hmdc,MPanel.hFontMessage);
		SetTextColor(MPanel.hmdc,MPanel.FGColor);
		SetBkColor(MPanel.hmdc,MPanel.BGColor);
		SetTextAlign(MPanel.hmdc, TA_LEFT | TA_TOP | TA_NOUPDATECP);
		TCHAR *t = char_to_tchar(MPanelMessageData.buff);
		switch ( MPanelMessageData.curmode ) {
		case 1:
			ExtTextOut(MPanel.hmdc, MPanel.rcMessage.left - (MPanelMessageData.pointer - MPanelMessageData.len) * (MPanel.rcMessage.bottom - MPanel.rcMessage.top + 1) / 2, MPanel.rcMessage.top,
				ETO_CLIPPED	| ETO_OPAQUE,&(MPanel.rcMessage),
    			t,_tcslen(t),NULL);
			break;
		case 3:
		case 4:
			// FIXME: implement this

		case 2:
		case -1:
		default:
			ExtTextOut(MPanel.hmdc,MPanel.rcMessage.left,MPanel.rcMessage.top,
				ETO_CLIPPED	| ETO_OPAQUE,&(MPanel.rcMessage),
    			t,_tcslen(t),NULL);
			break;
		}
		safe_free(t);
		if((HGDIOBJ)hgdiobj!=(HGDIOBJ)NULL && (HGDIOBJ)hgdiobj!=(HGDIOBJ)GDI_ERROR)
			SelectObject(MPanel.hmdc,hgdiobj);
		GDI_UNLOCK(); // gdi_lock
		InvalidateRect(hPanelWnd, &(MPanel.rcMessage), FALSE);
   }
	if(MPanel.UpdateFlag==MP_UPDATE_ALL)
		InvalidateRect(hPanelWnd, NULL, FALSE);
	MPanel.UpdateFlag = MP_UPDATE_NONE;
}

static void MPanelPaintDo(void)
{
	RECT rc;
	if ( GetUpdateRect(MPanel.hwnd, &rc, FALSE) ) {
		PAINTSTRUCT ps;
		HDC hdc;
		GDI_LOCK(); // gdi_lock
		hdc = BeginPaint(MPanel.hwnd, &ps);
		BitBlt(hdc,rc.left,rc.top,rc.right,rc.bottom,MPanel.hmdc,rc.left,rc.top,SRCCOPY);
		EndPaint(MPanel.hwnd, &ps);
		GDI_UNLOCK(); // gdi_lock
	}
}

// 描画
void MPanelPaint(void)
{
	//UpdateWindow(hPanelWnd);
}

// 完全描画
void MPanelPaintAll(void)
{
	InvalidateRect(hPanelWnd, NULL, FALSE);
	MPanelPaint();
}

// パネル構造体を元に完全更新をする。
void MPanelUpdateAll(void)
{
	if(!MPanelOK)
		return;
	MPanel.UpdateFlag = MP_UPDATE_ALL;
	MPanelUpdate();
}

// PanelInfo 構造体を読み込んでパネル構造体へ適用する。
// flag は強制更新する。
void MPanelReadPanelInfo(int flag)
{
	if(!MPanelOK)
		return;
	if(!PInfoOK)
    	return;

	if(!Panel->changed && !flag)
   	return;

	if(flag
   	||	MPanel.CurTime_s != Panel->cur_time_s
//    || MPanel.CurTime_ss != Panel->cur_time_ss
		|| MPanel.CurTime_m != Panel->cur_time_m
		|| MPanel.CurTime_h != Panel->cur_time_h
		|| MPanel.TotalTime_s != Panel->total_time_s
//		|| MPanel.TotalTime_ss != Panel->total_time_ss
		|| MPanel.TotalTime_m != Panel->total_time_m
		|| MPanel.TotalTime_h != Panel->total_time_h
	){
		MPanel.CurTime_h = Panel->cur_time_h;
		MPanel.CurTime_m = Panel->cur_time_m;
		MPanel.CurTime_s = Panel->cur_time_s;
		MPanel.CurTime_ss = Panel->cur_time_ss;
		MPanel.TotalTime_h = Panel->total_time_h;
		MPanel.TotalTime_m = Panel->total_time_m;
		MPanel.TotalTime_s = Panel->total_time_s;
//		MPanel.TotalTime_ss = Panel->total_time_ss;
		RANGE(MPanel.CurTime_h,0,99);
      RANGE(MPanel.TotalTime_h,0,99);
     	MPanel.UpdateFlag |=	MP_UPDATE_TIME;
	}
	if(flag || MPanel.MaxVoices != Panel->voices){
		MPanel.MaxVoices = Panel->voices;
     	MPanel.UpdateFlag |=	MP_UPDATE_VOICES;
   }
	if(flag || MPanel.CurVoices != Panel->cur_voices){
		MPanel.CurVoices = Panel->cur_voices;
     	MPanel.UpdateFlag |=	MP_UPDATE_VOICES;
   }
///r
	if(flag || MPanel.MVolume != output_amplification){
		MPanel.MVolume = output_amplification;
     	MPanel.UpdateFlag |=	MP_UPDATE_MVOLUME;
   }
	if(flag || MPanel.Rate != play_mode->rate){
		MPanel.Rate = play_mode->rate;
     	MPanel.UpdateFlag |=	MP_UPDATE_RATE;
   }
	if (flag || MPanel.Meas != Panel->meas) {
		MPanel.Meas = Panel->meas;
		MPanel.UpdateFlag |= MP_UPDATE_METRONOME;
	}
	if (flag || MPanel.Beat != Panel->beat) {
		MPanel.Beat = Panel->beat;
		MPanel.UpdateFlag |= MP_UPDATE_METRONOME;
	}
	if (flag || strcmp(MPanel.Keysig, Panel->keysig) != 0) {
		strcpy(MPanel.Keysig, Panel->keysig);
		MPanel.UpdateFlag |= MP_UPDATE_KEYSIG;
	}
	if (flag || MPanel.Key_offset != Panel->key_offset) {
		MPanel.Key_offset = Panel->key_offset;
		MPanel.UpdateFlag |= MP_UPDATE_KEYSIG;
	}
	if (flag || MPanel.Tempo != Panel->tempo) {
		MPanel.Tempo = Panel->tempo;
		MPanel.UpdateFlag |= MP_UPDATE_TEMPO;
	}
	if (flag || MPanel.Tempo_ratio != Panel->tempo_ratio) {
		MPanel.Tempo_ratio = Panel->tempo_ratio;
		MPanel.UpdateFlag |= MP_UPDATE_TEMPO;
	}
///r
/*
	w32g_get_playlist_index(&cur_pl_num, &playlist_num, NULL);
	if(playlist_num > 0)
		cur_pl_num++;
	if(flag || MPanel.PlaylistNum != cur_pl_num){
		MPanel.PlaylistNum = cur_pl_num;
     	MPanel.UpdateFlag |=	MP_UPDATE_PLAYLIST;
   }
	if(flag || MPanel.PlaylistMax != playlist_num){
		MPanel.PlaylistMax = playlist_num;
     	MPanel.UpdateFlag |=	MP_UPDATE_PLAYLIST;
   }
*/
	{
		int rate;
		float devsiz,filled;
		devsiz = aq_get_dev_queuesize();
		filled = aq_filled() + aq_soft_filled();
		rate = devsiz <= 0 ?  0 : (int)(filled / devsiz * 100 + 0.5); // 
		if(rate > 9999)
			rate = 9999;
		if(flag || MPanel.aq_ratio != rate){
			MPanel.rq_flag[0] = reduce_quality_flag ? 'Q' : ' ';
			MPanel.rv_flag[0] = reduce_voice_flag ? 'V' : ' ';
			MPanel.rp_flag[0] = reduce_polyphony_flag ? 'P' : ' ';
			MPanel.aq_ratio = rate;
     		MPanel.UpdateFlag |=	MP_UPDATE_AQ_RATIO;
		}
	}

	if(flag || MPanel.play_system_mode != play_system_mode){
		MPanel.play_system_mode = play_system_mode;
     	MPanel.UpdateFlag |=	MP_UPDATE_MISC;
   }
	if(current_file_info!=NULL){
	if(flag || MPanel.current_file_info_file_type != current_file_info->file_type){
      MPanel.current_file_info_file_type = current_file_info->file_type;
     	MPanel.UpdateFlag |=	MP_UPDATE_MISC;
   }
	if(flag || MPanel.current_file_info_max_channel != current_file_info->max_channel){
      MPanel.current_file_info_max_channel = current_file_info->max_channel;
     	MPanel.UpdateFlag |=	MP_UPDATE_MISC;
   }
   }
}

void MPanelStartLoad(const char *filename)
{
    strncpy((char *)MPanel.File, filename, MP_FILE_MAX);
    MPanel.UpdateFlag |= MP_UPDATE_FILE;
    MPanelUpdate();
}

void MPanelMessageInit(void)
{
	int width = (MPanel.rcMessage.bottom - MPanel.rcMessage.top + 1) / 2;
	MPanelMessageData.len = (MPanel.rcMessage.right - MPanel.rcMessage.left) / width;
	MPanelMessageClearAll();
}

// sec 秒で message を流す。
// mode 0: sec 秒だけ message を表示。デフォルト。
// mode 1: sec 秒の間に message を右から左に流す。
// mode 2: sec 秒の間に messege を表示。ポインタを左から右に移す。ポインタを境界に色を変える。
void MPanelMessageAdd(char *message, int msec, int mode)
{
	if ( MPanelMessageData.nextmode >= 0 ) {
		MPanelMessageNext();
		strncpy(MPanelMessageData.nextbuff,message,sizeof(MPanelMessageData.nextbuff)-1);
		MPanelMessageData.nextmode = mode;
		MPanelMessageData.nextmsec = msec;
	} else if ( MPanelMessageData.curmode >= 0 ){
		strncpy(MPanelMessageData.nextbuff,message,sizeof(MPanelMessageData.nextbuff)-1);
		MPanelMessageData.nextmode = mode;
		MPanelMessageData.nextmsec = msec;
	} else {
		strncpy(MPanelMessageData.nextbuff,message,sizeof(MPanelMessageData.nextbuff)-1);
		MPanelMessageData.nextmode = mode;
		MPanelMessageData.nextmsec = msec;
		MPanelMessageNext();
	}
}
int MPanelMessageHaveMesssage(void)
{
	if ( MPanelMessageData.curmode >= 0 || MPanelMessageData.nextmode >= 0 )
		return 1;
	else
		return 0;
}
void MPanelMessageClearAll(void)
{
	MPanelMessageData.buff[0] = '\0';
	MPanelMessageData.curmode = -1;
	MPanelMessageData.nextmode = -1;
   	MPanel.UpdateFlag |= MP_UPDATE_MESSAGE;
}
void MPanelMessageClear(void)
{
	MPanelMessageData.buff[0] = '\0';
   	MPanel.UpdateFlag |= MP_UPDATE_MESSAGE;
}
void MPanelMessageNext(void)
{
	MPanelMessageClear();
	if ( MPanelMessageData.nextmode >= 0 ) {
		strcpy(MPanelMessageData.curbuff,MPanelMessageData.nextbuff);
		MPanelMessageData.curbuffsize = strlen(MPanelMessageData.curbuff);
		MPanelMessageData.curmode = MPanelMessageData.nextmode;
		MPanelMessageData.curmsec = MPanelMessageData.nextmsec;
		MPanelMessageData.pointer = -1;
		MPanelMessageData.nextmode = -1;
		MPanelMessageData.prevtime = -1;
	} else {
		MPanelMessageData.curmode = -1;
		MPanelMessageData.prevtime = -1;
	}
   	MPanel.UpdateFlag |= MP_UPDATE_MESSAGE;
}
void MPanelMessageUpdate(void)
{
//	DWORD curtime = GetCurrentTime();
	DWORD curtime = 0;
	int pointer; 

	if ( MPanelMessageData.curmode >= 0 ) {
		curtime += Panel->cur_time_h; 
		curtime *= 24;
		curtime += Panel->cur_time_m;
		curtime *= 60;
		curtime += Panel->cur_time_s;
		curtime *= 1000;
		curtime += Panel->cur_time_ss;
	}
	switch ( MPanelMessageData.curmode ) {
	case 0:
		if ( MPanelMessageData.prevtime == -1 ){
			strcpy( MPanelMessageData.buff, MPanelMessageData.curbuff );
			MPanelMessageData.prevtime = curtime;
			MPanelMessageData.msec = MPanelMessageData.curmsec;
	     	MPanel.UpdateFlag |= MP_UPDATE_MESSAGE;
		} else {
			MPanelMessageData.msec -= curtime - MPanelMessageData.prevtime;
			MPanelMessageData.prevtime = curtime;
		}
		if ( MPanelMessageData.msec <= 0 || curtime < MPanelMessageData.prevtime ) {
			MPanelMessageNext();
			MPanelMessageUpdate();
	     	MPanel.UpdateFlag |= MP_UPDATE_MESSAGE;
			break;
		}
		break;
	case 1:
		if ( MPanelMessageData.prevtime == -1 ){
			strcpy(MPanelMessageData.buff, MPanelMessageData.curbuff);
			MPanelMessageData.prevtime = curtime;
			MPanelMessageData.msec = MPanelMessageData.curmsec;
	     	MPanel.UpdateFlag |= MP_UPDATE_MESSAGE;
		} else {
			MPanelMessageData.msec -= curtime - MPanelMessageData.prevtime;
			MPanelMessageData.prevtime = curtime;
		}
		if ( MPanelMessageData.msec <= 0 || curtime < MPanelMessageData.prevtime ) {
			MPanelMessageNext();
			MPanelMessageUpdate();
	     	MPanel.UpdateFlag |= MP_UPDATE_MESSAGE;
			return;
		}
//		pointer = MPanelMessageData.len * 4 / 5 + ( MPanelMessageData.curmsec - MPanelMessageData.msec ) / 1000 * 2;
		pointer = MPanelMessageData.len - 8 + ( MPanelMessageData.curmsec - MPanelMessageData.msec ) / 1000 * 2;
		pointer = (int)( pointer / 2 ) * 2;
		if ( MPanelMessageData.pointer != pointer ) {
			MPanelMessageData.pointer = pointer;
	     	MPanel.UpdateFlag |= MP_UPDATE_MESSAGE;
		}
		break;
	case 2:
		if ( MPanelMessageData.prevtime == -1 ){
			strcpy( MPanelMessageData.buff, MPanelMessageData.curbuff );
			MPanelMessageData.prevtime = curtime;
			MPanelMessageData.msec = MPanelMessageData.curmsec;
	     	MPanel.UpdateFlag |= MP_UPDATE_MESSAGE;
		} else {
			MPanelMessageData.msec -= curtime - MPanelMessageData.prevtime;
			MPanelMessageData.prevtime = curtime;
		}
		if ( MPanelMessageData.msec <= 0 || curtime < MPanelMessageData.prevtime ) {
			MPanelMessageNext();
			MPanelMessageUpdate();
	     	MPanel.UpdateFlag |= MP_UPDATE_MESSAGE;
			break;
		}
		pointer = ( MPanelMessageData.len + MPanelMessageData.curbuffsize ) * ( MPanelMessageData.curmsec - MPanelMessageData.msec ) / MPanelMessageData.curmsec;
		if ( MPanelMessageData.pointer != pointer ) {
	     	MPanel.UpdateFlag |= MP_UPDATE_MESSAGE;
		}
		break;
	case 3:
		if ( MPanelMessageData.prevtime == -1 ){
			strcpy(MPanelMessageData.buff, MPanelMessageData.curbuff);
			MPanelMessageData.prevtime = curtime;
			MPanelMessageData.msec = MPanelMessageData.curmsec;
	     	MPanel.UpdateFlag |= MP_UPDATE_MESSAGE;
		} else {
			MPanelMessageData.msec -= curtime - MPanelMessageData.prevtime;
			MPanelMessageData.prevtime = curtime;
		}
		if ( MPanelMessageData.msec <= 0 || curtime < MPanelMessageData.prevtime ) {
			MPanelMessageNext();
			MPanelMessageUpdate();
	     	MPanel.UpdateFlag |= MP_UPDATE_MESSAGE;
			return;
		}
		pointer = MPanelMessageData.len * 3 / 4 + ( MPanelMessageData.len / 4 + MPanelMessageData.curbuffsize ) * ( MPanelMessageData.curmsec - MPanelMessageData.msec ) / MPanelMessageData.curmsec;
		pointer = ((int)(pointer / 2)) * 2;
		if ( MPanelMessageData.pointer != pointer ) {
			MPanelMessageData.pointer = pointer;
	     	MPanel.UpdateFlag |= MP_UPDATE_MESSAGE;
		}
		break;
	case 4:
		if ( MPanelMessageData.prevtime == -1 ){
			strcpy(MPanelMessageData.buff, MPanelMessageData.curbuff);
			MPanelMessageData.prevtime = curtime;
#define MPANELMESSAGE_MODE2_SLEEPMSEC 1000
			if ( MPanelMessageData.curmsec < MPANELMESSAGE_MODE2_SLEEPMSEC * 2 ) {
				MPanelMessageData.curmsec = MPANELMESSAGE_MODE2_SLEEPMSEC * 2;
			}
			MPanelMessageData.msec = MPanelMessageData.curmsec;
	     	MPanel.UpdateFlag |= MP_UPDATE_MESSAGE;
		} else {
			MPanelMessageData.msec -= curtime - MPanelMessageData.prevtime;
			MPanelMessageData.prevtime = curtime;
		}
		if ( MPanelMessageData.msec <= 0 || curtime < MPanelMessageData.prevtime ) {
			MPanelMessageNext();
			MPanelMessageUpdate();
	     	MPanel.UpdateFlag |= MP_UPDATE_MESSAGE;
			return;
		}
		if ( MPanelMessageData.curmsec - MPanelMessageData.msec <= MPANELMESSAGE_MODE2_SLEEPMSEC ) {
			pointer = 0;
		} else {
			pointer = MPanelMessageData.curbuffsize * ( MPanelMessageData.curmsec - MPanelMessageData.msec - MPANELMESSAGE_MODE2_SLEEPMSEC ) / ( MPanelMessageData.curmsec - MPANELMESSAGE_MODE2_SLEEPMSEC );
		}
		pointer = ((int)(pointer / 2)) * 2;
		if ( MPanelMessageData.pointer != pointer ) {
			MPanelMessageData.pointer = pointer;
	     	MPanel.UpdateFlag |= MP_UPDATE_MESSAGE;
		}
		break;
	case -1:
	default:
//		MPanelMessageData.buff[0] = '\0';
		break;
	}
}




// ----------------------------------------------------------------------------
// Misc. Controls











// ----------------------------------------------------------------------------









// ****************************************************************************
// Version Window

static void VersionWnd(HWND hParentWnd)
{
	char VersionText[2024];
  snprintf(VersionText, sizeof(VersionText) / sizeof(char),
"TiMidity++ %s%s %s"
#ifdef UNICODE
" [Unicode]"
#endif
NLS NLS
"TiMidity-0.2i by Tuukka Toivonen <tt@cgs.fi>." NLS
"TiMidity Win32 version by Davide Moretti <dave@rimini.com>." NLS
"TiMidity Windows 95 port by Nicolas Witczak." NLS
"TiMidity Win32 GUI by Daisuke Aoki <dai@y7.net>." NLS
" Japanese menu, dialog, etc by Saito <timidity@flashmail.com>." NLS
"TiMidity++ by Masanao Izumo <mo@goice.co.jp>." NLS
,(strcmp(timidity_version, "current")) ? "version " : "", timidity_version, arch_string
);
  TCHAR *t = char_to_tchar(VersionText);
	MessageBox(hParentWnd, t, _T("Version"), MB_OK);
	safe_free(t);
}

static void TiMidityWnd(HWND hParentWnd)
{
	char TiMidityText[2024];
  snprintf(TiMidityText, sizeof(TiMidityText) / sizeof(char),
" TiMidity++ %s%s %s"
#ifdef UNICODE
" [Unicode]"
#endif
" -- MIDI to WAVE converter and player" NLS
" Copyright (C) 1999-2018 Masanao Izumo <iz@onicos.co.jp>" NLS
" Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>" NLS
NLS
" Win32 version by Davide Moretti <dmoretti@iper.net>" NLS
" GUI by Daisuke Aoki <dai@y7.net>." NLS
" Modified by Masanao Izumo <mo@goice.co.jp>." NLS
NLS
" This program is free software; you can redistribute it and/or modify" NLS
" it under the terms of the GNU General Public License as published by" NLS
" the Free Software Foundation; either version 2 of the License, or" NLS
" (at your option) any later version." NLS
NLS
" This program is distributed in the hope that it will be useful," NLS
" but WITHOUT ANY WARRANTY; without even the implied warranty of"NLS
" MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the" NLS
" GNU General Public License for more details." NLS
NLS
" You should have received a copy of the GNU General Public License" NLS
" along with this program; if not, write to the Free Software" NLS
" Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA" NLS
,
(strcmp(timidity_version, "current")) ? "version " : "", timidity_version, arch_string
	);
  TCHAR *t = char_to_tchar(TiMidityText);
	MessageBox(hParentWnd, t, _T("TiMidity++"), MB_OK);
	safe_free(t);
}

static void SupplementWnd(HWND hParentWnd)
{
	char SupplementText[2024];
  snprintf(SupplementText, sizeof(SupplementText) / sizeof(char),
"GS is a registered trademark of Roland Corporation. " NLS
"XG is a registered trademark of Yamaha Corporation. " NLS
"KG is a registered trademark of KORG Incorporated. " NLS
"GM2 specs are published by the MIDI Manufacturers Association" NLS
" and Association of Musical Electronics Industry. " NLS);
  TCHAR *t = char_to_tchar(SupplementText);
	  MessageBox(hParentWnd, t, _T("Supplement"), MB_OK);
	  safe_free(t);
}


// ****************************************************************************

#ifdef W32GUI_DEBUG
void TiMidityHeapCheck(void)
{
	HANDLE *ProcessHeaps = NULL;
   DWORD dwNumberOfHeaps;
	DWORD dw = 10;
   int i;
   PrintfDebugWnd("\n[Heaps Check Start]\n");
	if(GetProcessHeap()!=NULL)
   	if(HeapValidate(GetProcessHeap(),0,NULL)==TRUE)
   		PrintfDebugWnd("Process Heap is Valid\n");
     	else
   		PrintfDebugWnd("Process Heap is Invalid\n");
   ProcessHeaps = (HANDLE *)realloc(ProcessHeaps,sizeof(HANDLE)*dw);
   dwNumberOfHeaps = GetProcessHeaps(dw, ProcessHeaps);
   if(dw<dwNumberOfHeaps){
		dw = dwNumberOfHeaps;
	   ProcessHeaps = (HANDLE *)realloc(ProcessHeaps,sizeof(HANDLE)*dw);
   	dwNumberOfHeaps = GetProcessHeaps(dw, ProcessHeaps);
   }
   PrintfDebugWnd("NumberOfHeaps=%ld\n",(int)dwNumberOfHeaps);
	for(i=0;i<(int)dwNumberOfHeaps;i++){
     	if(HeapValidate(ProcessHeaps[i],0,NULL)==TRUE)
    		PrintfDebugWnd("Heap %d is Valid\n",i+1);
      else
     		PrintfDebugWnd("Heap %d is Invalid\n",i+1);
   }
   PrintfDebugWnd("[Heaps Check End]\n\n");
	safe_free(ProcessHeaps);
}
#endif

void TiMidityVariablesCheck(void)
{
#if 0
// player_status
	PrintfDebugWnd("[player_status]\n");
	PrintfDebugWnd("player_status=%ld\n",player_status);
	switch(player_status){
	case PLAYERSTATUS_NONE:
	PrintfDebugWnd("player_status=PLAYERSTATUS_NONE\n");
	break;
	case PLAYERSTATUS_STOP:
	PrintfDebugWnd("player_status=PLAYERSTATUS_STOP\n");
	break;
	case PLAYERSTATUS_PAUSE:
	PrintfDebugWnd("player_status=PLAYERSTATUS_PAUSE\n");
	break;
	case PLAYERSTATUS_PLAY:
	PrintfDebugWnd("player_status=PLAYERSTATUS_PLAY\n");
	break;
	case PLAYERSTATUS_PLAYSTART:
	PrintfDebugWnd("player_status=PLAYERSTATUS_PLAYSTART\n");
	break;
	case PLAYERSTATUS_DEMANDPLAY:
	PrintfDebugWnd("player_status=PLAYERSTATUS_DEMANDPLAY\n");
	break;
	case PLAYERSTATUS_PLAYEND:
	PrintfDebugWnd("player_status=PLAYERSTATUS_PLAYEND\n");
	break;
	case PLAYERSTATUS_PLAYERROREND:
	PrintfDebugWnd("player_status=PLAYERSTATUS_PLAYERROREND\n");
	break;
	case PLAYERSTATUS_QUIT:
	PrintfDebugWnd("player_status=PLAYERSTATUS_QUIT\n");
	break;
	case PLAYERSTATUS_ERROR:
	PrintfDebugWnd("player_status=PLAYERSTATUS_ERROR\n");
	break;
	case PLAYERSTATUS_FORCED_EXIT:
	PrintfDebugWnd("player_status=PLAYERSTATUS_FORCED_EXIT\n");
	break;
	default:
   break;
   }
#endif
}

extern int32 test_var[10];
int32 test_var[10] = {0};


// ****************************************************************************
// Debug Window
#ifdef W32GUI_DEBUG

BOOL CALLBACK DebugWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
void InitDebugEditWnd(HWND hParentWnd);

void InitDebugWnd(HWND hParentWnd)
{
	hDebugWnd = CreateDialog
  			(hInst,MAKEINTRESOURCE(IDD_DIALOG_DEBUG),hParentWnd,DebugWndProc);
	ShowWindow(hDebugWnd,SW_HIDE);
	UpdateWindow(hDebugWnd);
}

BOOL CALLBACK
DebugWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess){
		case WM_INITDIALOG:
//			InitDebugEditWnd(hwnd);
		hDebugEditWnd = GetDlgItem(hwnd,IDC_EDIT);
		if(DebugWndFlag)
			CheckDlgButton(hwnd, IDC_CHECKBOX_DEBUG_WND_VALID, 1);
      else
			CheckDlgButton(hwnd, IDC_CHECKBOX_DEBUG_WND_VALID, 0);
		TiMidityHeapCheck();
			return FALSE;
		case WM_DESTROY:
      	PostQuitMessage(0);
			return 0;
    case WM_COMMAND:
    	switch (LOWORD(wParam)) {
      case IDCLOSE:
      	ShowWindow(hwnd, SW_HIDE);
         break;
      case IDCLEAR:
			ClearDebugWnd();
         break;
			case IDC_CHECKBOX_DEBUG_WND_VALID:
				if(IsDlgButtonChecked(hwnd,IDC_CHECKBOX_DEBUG_WND_VALID))
					DebugWndFlag = 1;
				else
					DebugWndFlag = 0;
         break;
      case IDC_BUTTON_EXITPROCESS:
        	ExitProcess(0);
			return 0;
      case IDC_BUTTON_EXIT:
        	return DestroyWindow(hwnd);
      case IDC_BUTTON_HEAP_CHECK:
			TiMidityHeapCheck();
        	break;
      case IDC_BUTTON_VARIABLES_CHECK:
			TiMidityVariablesCheck();
        	break;
	case IDC_EDIT_VAR0:
	case IDC_EDIT_VAR1:
	case IDC_EDIT_VAR2:
	case IDC_EDIT_VAR3:
	case IDC_EDIT_VAR4:
	case IDC_EDIT_VAR5:
	case IDC_EDIT_VAR6:
	case IDC_EDIT_VAR7:
	case IDC_EDIT_VAR8:
	case IDC_EDIT_VAR9:
     	break;
	case IDC_BUTTON_VAR_ENTER:
	{
		int i;		
		for(i=0; i<10;i++)
			test_var[i] = GetDlgItemInt(hwnd, IDC_EDIT_VAR0 + i, NULL, TRUE);
	}
        	break;
      default:
        		break;
      }
   	switch (HIWORD(wParam)) {
		case EN_ERRSPACE:
      	ClearConsoleWnd();
//      	PutsConsoleWnd("### EN_ERRSPACE -> Clear! ###\n");
			break;
		default:
      	break;
      }
    	break;
    case WM_SIZE:
//      GetClientRect(hDebugWnd, &rc);
//      MoveWindow(hDebugEditWnd, rc.left, rc.top,rc.right, rc.bottom - 30,TRUE);
	    return FALSE;
		case WM_CLOSE:
					ShowWindow(hDebugWnd, SW_HIDE);
          break;
    default:
      	return FALSE;
	}
	return FALSE;
}

#if 0
void InitDebugEditWnd(HWND hParentWnd)
{
  RECT rc;
	GetClientRect(hParentWnd, &rc);
	hDebugEditWnd = CreateWindowEx(
  	WS_EX_CLIENTEDGE|WS_EX_TOOLWINDOW|WS_EX_DLGMODALFRAME,
  	"EDIT","",
		WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_AUTOHSCROLL | WS_HSCROLL
    |ES_READONLY | ES_WANTRETURN | ES_MULTILINE | ES_AUTOVSCROLL ,
//      	0,0,rc.right, rc.bottom - 30,hParentWnd,NULL,hInst,NULL);
      	0,0,100,100,hParentWnd,NULL,hInst,NULL);
	SendMessage(hDebugEditWnd, EM_SETLIMITTEXT, (WPARAM)1024*640, 0);
//  SendMessage(hDebugEditWnd, WM_PAINT, 0, 0);
	GetClientRect(hParentWnd, &rc);
	MoveWindow(hDebugEditWnd,rc.left,rc.top,rc.right,rc.bottom-30,TRUE);
 	ClearDebugWnd();
	ShowWindow(hDebugEditWnd,SW_SHOW);
	UpdateWindow(hDebugEditWnd);
}
#endif

void PutsDebugWnd(char *str)
{
	if(!IsWindow(hDebugEditWnd) || !DebugWndFlag)
		return;
	PutsEditCtlWnd(hDebugEditWnd,str);
}

void PrintfDebugWnd(char *fmt, ...)
{
	va_list ap;
	if(!IsWindow(hDebugEditWnd) || !DebugWndFlag)
		return;
	va_start(ap, fmt);
	VprintfEditCtlWnd(hDebugEditWnd,fmt,ap);
	va_end(ap);
}

void ClearDebugWnd(void)
{
	if(!IsWindow(hDebugEditWnd))
		return;
	ClearEditCtlWnd(hDebugEditWnd);
}
#endif






















// ****************************************************************************
// Main Thread

extern HWND hListSearchWnd;
extern void HideListSearch(void);

#ifndef __BORLANDC__
DWORD volatile dwMainThreadId = 0;
#endif

void WINAPI MainThread(void *arglist)
{
#ifdef __BORLANDC__
	DWORD volatile dwMainThreadId = 0;
#endif
    MSG msg;

	ThreadNumMax++;

	dwMainThreadId = GetCurrentThreadId ();
#ifdef W32GUI_DEBUG
	PrintfDebugWnd("(*/%d)MainThread : Start.\n",ThreadNumMax);
#endif
#ifdef USE_THREADTIMES
	ThreadTimesAddThread(hMainThread,"MainThread");
#endif
// Thread priority
//	SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
	SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_NORMAL);
//	SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_ABOVE_NORMAL);


    InitStartWnd(SW_HIDE);

    w32g_wait_for_init = 0;

// message loop for the application
	while( GetMessage(&msg,NULL,0,0) ){
//		HandleFastSearch(msg);
//PrintfDebugWnd("H%lu M%lu WP%lu LP%lu T%lu x%d y%d\n",
//	msg.hwnd, msg.message, msg.wParam, msg.lParam, msg.time, msg.pt.x, msg.pt.y);
#if 1
		// ESC で窓を閉じる。 
		if ( msg.message == WM_KEYDOWN && (int)msg.wParam == VK_ESCAPE ) {
			if ( msg.hwnd == hConsoleWnd || IsChild ( hConsoleWnd, msg.hwnd ) ) {
				 ToggleSubWindow(hConsoleWnd);
			} else if ( msg.hwnd == hDocWnd || IsChild ( hDocWnd, msg.hwnd ) ) {
				 ToggleSubWindow(hDocWnd);
			} else if ( msg.hwnd == hWrdWnd || IsChild ( hWrdWnd, msg.hwnd ) ) {
				 ToggleSubWindow(hWrdWnd);
			} else if ( msg.hwnd == hListWnd || IsChild ( hListWnd, msg.hwnd ) ) {
				 ToggleSubWindow(hListWnd);
			} else if ( msg.hwnd == hListSearchWnd || IsChild ( hListSearchWnd, msg.hwnd ) ) {
				HideListSearch();
			} else if ( msg.hwnd == hTracerWnd || IsChild ( hTracerWnd, msg.hwnd ) ) {
				ToggleSubWindow(hTracerWnd);
			} else if ( msg.hwnd == hSoundSpecWnd || IsChild ( hSoundSpecWnd, msg.hwnd ) ) {
				ToggleSubWindow(hSoundSpecWnd);
			}
		}
#endif
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

#ifdef W32GUI_DEBUG
	Sleep(200);
	PrintfDebugWnd("w32gui_main: DebugWndExit\n");
//   MessageBox(NULL, "Exit!","Exit!",MB_OK);
	if(hDebugWnd!=NULL)
		for(;;){
			if(!DebugThreadExit){
   			SendMessage(hDebugWnd,WM_COMMAND,(WPARAM)IDC_BUTTON_EXIT,0);
				Sleep(100);
         } else
         	break;
      }
#endif
	if(!w32g_restart_gui_flag)
	{
		OnExitReady();
		w32g_send_rc(RC_QUIT, 0);
	}
	crt_endthread();
}




// **************************************************************************
// Misc Dialog
#if FILEPATH_MAX < 16536
#define DialogMaxFileName 16536
#else
#define DialogMaxFileName FILEPATH_MAX
#endif
static char DialogFileNameBuff[DialogMaxFileName];
static char *DlgFileOpen(HWND hwnd, const TCHAR *title, const TCHAR *filter, const TCHAR *dir)
{
	TCHAR tfile[DialogMaxFileName] = _T("");
	OPENFILENAME ofn;
    memset(DialogFileNameBuff, 0, sizeof(DialogFileNameBuff));
	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.hInstance = hInst ;
	ofn.lpstrFilter = filter;
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 1 ;
	ofn.lpstrFile = tfile;
	ofn.nMaxFile = sizeof(tfile) / sizeof(tfile[0]);
	ofn.lpstrFileTitle = 0;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir	= dir;
	ofn.lpstrTitle = title;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST
		| OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_READONLY;
	ofn.lpstrDefExt = 0;
	ofn.lCustData = 0;
	ofn.lpfnHook = 0;
	ofn.lpTemplateName= 0;

	if(GetOpenFileName(&ofn)==TRUE) {
		// tfile may contain null characters
#ifdef UNICODE
		// "DialogMaxFileName - 2" to ensure the buffer ends with two null characters
		WideCharToMultiByte(CP_UTF8, 0, tfile, DialogMaxFileName, DialogFileNameBuff, DialogMaxFileName - 2, NULL, NULL);
#else
		memcpy(DialogFileNameBuff, tfile, sizeof(DialogFileNameBuff));
#endif
		return DialogFileNameBuff;
	} else
		return NULL;
}

static void DlgMidiFileOpen(HWND hwnd)
{
    TCHAR* dir;
    char *file;
    const TCHAR *filter,
        filter_en[] = _T("timidity file\0*.mid;*.smf;*.rcp;*.r36;*.g18;*.g36;*.rmi;*.mld;*.mod;*.xm;*.s3m;*.it;*.669;*.amf;*.dsm;*.far;*.gdm;*.imf;*.med;*.mtm;*.stm;*.stx;*.ult;*.uni;*.lzh;*.zip;*.gz;*.pls;*.m3u;*.asx\0")
            _T("midi file\0*.mid;*.midi;*.smf;*.rmi\0")
            _T("rcp file\0*.rcp;*.r36;*.g18;*.g36\0")
            _T("mfi file\0*.mld\0")
            _T("mod file\0*.mod;*.xm;*.s3m;*.it;*.669;*.amf;*.dsm;*.far;*.gdm;*.imf;*.med;*.mtm;*.stm;*.stx;*.ult;*.uni\0")
            _T("archive file\0*.lzh;*.zip;*.gz\0")
            _T("playlist file\0*.pls;*.m3u;*.asx\0")
            _T("all files\0*.*\0")
            _T("\0\0"),
        filter_jp[] = _T("Timidity サポート済みファイル\0*.mid;*.smf;*.rcp;*.r36;*.g18;*.g36;*.rmi;*.mld;*.mod;*.xm;*.s3m;*.it;*.669;*.amf;*.dsm;*.far;*.gdm;*.imf;*.med;*.mtm;*.stm;*.stx;*.ult;*.uni;*.lzh;*.zip;*.gz;*.pls;*.m3u;*.asx\0")
            _T("SMF/RMID (*.mid;*.midi;*.smf;*.rmi)\0*.mid;*.midi;*.smf;*.rmi\0")
            _T("RCP (*.rcp;*.r36;*.g18;*.g36)\0*.rcp;*.r36;*.g18;*.g36\0")
            _T("MFi (*.mld)\0*.mld\0")
            _T("MOD (*.mod;*.xm;*.s3m;*.it;*.669;*.amf;*.dsm;*.far;*.gdm;*.imf;*.med;*.mtm;*.stm;*.stx;*.ult;*.uni)\0*.mod;*.xm;*.s3m;*.it;*.669;*.amf;*.dsm;*.far;*.gdm;*.imf;*.med;*.mtm;*.stm;*.stx;*.ult;*.uni\0")
            _T("圧縮済みアーカイブ (*.lzh;*.zip;*.gz)\0*.lzh;*.zip;*.gz\0")
            _T("プレイリストファイル (*.pls;*.m3u;*.asx)\0*.pls;*.m3u;*.asx\0")
            _T("すべてのファイル (*.*)\0*.*\0")
            _T("\0\0");

    if ( PlayerLanguage == LANGUAGE_JAPANESE ) 
		filter = filter_jp;
    else
		filter = filter_en;

    if(w32g_lock_open_file)
		return;

    if(MidiFileOpenDir[0])
		dir = char_to_tchar(MidiFileOpenDir);
    else
		dir = NULL;

	file = DlgFileOpen(hwnd, NULL, filter, dir);
	safe_free(dir);
	if(file == NULL)
		return;

    w32g_lock_open_file = 1;
#ifdef EXT_CONTROL_MAIN_THREAD
	w32g_ext_control_main_thread(RC_EXT_LOAD_FILE, (ptr_size_t)file);
#else
	w32g_send_rc(RC_EXT_LOAD_FILE, (ptr_size_t)file);
#endif
}

static volatile LPITEMIDLIST itemidlist_pre = NULL;
int CALLBACK DlgDirOpenBrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	switch(uMsg){
	case BFFM_INITIALIZED:
		if(itemidlist_pre)
			SendMessage(hwnd,BFFM_SETSELECTION,(WPARAM)0,(LPARAM)itemidlist_pre);
		break;
	default:
		break;
	}
	return 0;
}

static void DlgDirOpen(HWND hwnd)
{
	static int initflag = 1;
	static TCHAR biBuffer[FILEPATH_MAX];
	static TCHAR tBuffer[FILEPATH_MAX];
	static char Buffer[FILEPATH_MAX];
	BROWSEINFO bi;
	LPITEMIDLIST itemidlist;

	if(w32g_lock_open_file)
	    return;

	if(initflag==1){
		biBuffer[0] = _T('\0');
		initflag = 0;
	}
	memset(&bi, 0, sizeof(bi));
	bi.hwndOwner = hwnd;
	bi.pidlRoot = NULL;
    bi.pszDisplayName = biBuffer;
	if ( PlayerLanguage == LANGUAGE_JAPANESE ) 
		bi.lpszTitle = _T("MIDI ファイルのあるディレクトリを御選択なされますよう。");
	else
		bi.lpszTitle = _T("Select a directory with MIDI files.");
	bi.ulFlags = 0;
	bi.lpfn = DlgDirOpenBrowseCallbackProc;
    bi.lParam = 0;
    bi.iImage = 0;
	itemidlist = SHBrowseForFolder(&bi);
	if(!itemidlist)
		return; /* Cancel */
	memset(tBuffer, 0, sizeof(tBuffer));
	SHGetPathFromIDList(itemidlist, tBuffer);
	_tcsncpy(biBuffer, tBuffer, sizeof(tBuffer) / sizeof(TCHAR) - 1);
	if(itemidlist_pre)
		CoTaskMemFree(itemidlist_pre);
	itemidlist_pre = itemidlist;
    w32g_lock_open_file = 1;
	char *s = tchar_to_char(tBuffer);
	strncpy(Buffer, s, sizeof(Buffer) / sizeof(char) - 1);
	safe_free(s);
	Buffer[sizeof(Buffer) / sizeof(char) - 1] = '\0';
	directory_form(Buffer);
#ifdef EXT_CONTROL_MAIN_THREAD
	w32g_ext_control_main_thread(RC_EXT_LOAD_FILE, (ptr_size_t)Buffer);
#else
	w32g_send_rc(RC_EXT_LOAD_FILE, (ptr_size_t)Buffer);
#endif
}

LRESULT CALLBACK UrlOpenWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static void DlgUrlOpen(HWND hwnd)
{
    if(w32g_lock_open_file)
		return;

    switch(PlayerLanguage) {
    case LANGUAGE_JAPANESE:
        DialogBox(hInst,MAKEINTRESOURCE(IDD_DIALOG_ONE_LINE),hwnd,UrlOpenWndProc);
        break;
    case LANGUAGE_ENGLISH:
    default:
        DialogBox(hInst,MAKEINTRESOURCE(IDD_DIALOG_ONE_LINE_EN),hwnd,UrlOpenWndProc);
        break;
    }
}

#if FILEPATH_MAX < 8192
#define UrlOpenStringMax 8192
#else
#define UrlOpenStringMax FILEPATH_MAX
#endif

volatile argc_argv_t UrlArgcArgv;
LRESULT CALLBACK
UrlOpenWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	static volatile argc_argv_t UrlArgcArgv;

    switch (uMess) {
    case WM_INITDIALOG:
        switch (PlayerLanguage) {
        case LANGUAGE_ENGLISH:
            SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)_T("Open Internet URL"));
            SendMessage(GetDlgItem(hwnd, IDC_STATIC_HEAD), WM_SETTEXT, 0, (LPARAM)_T("Type the address of a file (on the Internet) and the player will open it for you."));
            SendMessage(GetDlgItem(hwnd, IDC_STATIC_TAIL), WM_SETTEXT, 0, (LPARAM)_T("(ex. http, ftp, news protocols)"));
            SendMessage(GetDlgItem(hwnd, IDC_BUTTON_1), WM_SETTEXT, 0, (LPARAM)_T("PLAY"));
            SendMessage(GetDlgItem(hwnd, IDC_BUTTON_2), WM_SETTEXT, 0, (LPARAM)_T("ADD LIST"));
            SendMessage(GetDlgItem(hwnd, IDC_BUTTON_3), WM_SETTEXT, 0, (LPARAM)_T("CLOSE"));
            break;
        case LANGUAGE_JAPANESE:
        default:
            SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)_T("URL を開く"));
            SendMessage(GetDlgItem(hwnd, IDC_STATIC_HEAD), WM_SETTEXT, 0, (LPARAM)_T("インターネットにあるファイルのアドレスを入れてください。"));
            SendMessage(GetDlgItem(hwnd, IDC_STATIC_TAIL), WM_SETTEXT, 0, (LPARAM)_T("(例: http, ftp, news プロトコル)"));
            SendMessage(GetDlgItem(hwnd, IDC_BUTTON_1), WM_SETTEXT, 0, (LPARAM)_T("演奏"));
            SendMessage(GetDlgItem(hwnd, IDC_BUTTON_2), WM_SETTEXT, 0, (LPARAM)_T("リスト追加"));
            SendMessage(GetDlgItem(hwnd, IDC_BUTTON_3), WM_SETTEXT, 0, (LPARAM)_T("閉じる"));
            break;
        }
        SendDlgItemMessage(hwnd, IDC_BUTTON_1, BM_SETSTYLE, BS_DEFPUSHBUTTON, (LONG)TRUE);
        SendDlgItemMessage(hwnd, IDC_BUTTON_2, BM_SETSTYLE, BS_PUSHBUTTON, (LONG)TRUE);
        SendDlgItemMessage(hwnd, IDC_BUTTON_3, BM_SETSTYLE, BS_PUSHBUTTON, (LONG)TRUE);
        SetFocus(GetDlgItem(hwnd, IDC_EDIT_ONE_LINE));
        SendDlgItemMessage(hwnd, IDC_EDIT_ONE_LINE, EM_SETSEL, (WPARAM)0, (LPARAM)0);
        SendDlgItemMessage(hwnd, IDC_EDIT_ONE_LINE, EM_REPLACESEL, (WPARAM)FALSE, (LPARAM)_T("http://"));
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDCLOSE:
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            break;

        case IDC_BUTTON_1: {
            TCHAR UrlOpenString[UrlOpenStringMax];
            char **argv;
            ZeroMemory(UrlOpenString, UrlOpenStringMax * sizeof(TCHAR));
            SendDlgItemMessage(hwnd, IDC_EDIT_ONE_LINE,
                WM_GETTEXT, (WPARAM)(UrlOpenStringMax - 1), (LPARAM)UrlOpenString);
            w32g_lock_open_file = 1;
            UrlArgcArgv.argc = 1;
            argv = (char**) safe_malloc((UrlArgcArgv.argc + 1) * sizeof(char*));
            argv[0] = tchar_to_char(UrlOpenString);
            argv[1] = NULL;
            UrlArgcArgv.argv = argv;
            // argv, argv[0] は別のところで解放してくれる
#ifdef EXT_CONTROL_MAIN_THREAD
			w32g_ext_control_main_thread(RC_EXT_LOAD_FILES_AND_PLAY, (ptr_size_t)&UrlArgcArgv);
#else
			w32g_send_rc(RC_EXT_LOAD_FILES_AND_PLAY, (ptr_size_t)&UrlArgcArgv);
#endif
            w32g_lock_open_file = 0;
            SetWindowLongPtr(hwnd, DWLP_MSGRESULT, TRUE);
            EndDialog(hwnd, TRUE);
            break; }

        case IDC_BUTTON_2: {
            static char UrlOpenString[UrlOpenStringMax];
            TCHAR tUrlOpenString[UrlOpenStringMax];
            ZeroMemory(UrlOpenString, UrlOpenStringMax * sizeof(char));
            ZeroMemory(tUrlOpenString, UrlOpenStringMax * sizeof(TCHAR));
            SendDlgItemMessage(hwnd, IDC_EDIT_ONE_LINE,
                WM_GETTEXT, (WPARAM)(UrlOpenStringMax - 1), (LPARAM)tUrlOpenString);
            w32g_lock_open_file = 1;
			char *s = tchar_to_char(tUrlOpenString);
			strlcpy(UrlOpenString, s, UrlOpenStringMax - 1);
			safe_free(s);
#ifdef EXT_CONTROL_MAIN_THREAD
			w32g_ext_control_main_thread(RC_EXT_LOAD_FILE, (ptr_size_t)UrlOpenString);
#else
			w32g_send_rc(RC_EXT_LOAD_FILE, (ptr_size_t)UrlOpenString);
#endif
			SetWindowLongPtr(hwnd,DWLP_MSGRESULT,TRUE);
			EndDialog(hwnd,TRUE);
		}
			break;
		case IDC_BUTTON_3:
			PostMessage(hwnd,WM_CLOSE,0,0);
			break;
		default:
			return FALSE;
	}
		break;
	case WM_CLOSE:
		SetWindowLongPtr(hwnd,DWLP_MSGRESULT,FALSE);
		EndDialog(hwnd,FALSE);
		break;
	default:
		return FALSE;
	}
	return FALSE;
}

static void DlgPlaylistOpen(HWND hwnd)
{
    TCHAR *dir;
    char *file;
    const TCHAR *filter,
        filter_en[] =
           _T("playlist file\0*.pls;*.m3u;*.asx\0")
           _T("all files\0*.*\0")
           _T("\0\0"),
        filter_jp[] =
            _T("プレイリストファイル (*.pls;*.m3u;*.asx)\0*.pls;*.m3u;*.asx\0")
            _T("すべてのファイル (*.*)\0*.*\0")
            _T("\0\0");

	if (PlayerLanguage == LANGUAGE_JAPANESE)
		filter = filter_jp;
	else
		filter = filter_en;

	if(w32g_lock_open_file)
		return;

    if(MidiFileOpenDir[0])
		dir = char_to_tchar(MidiFileOpenDir);
    else
		dir = NULL;

	file = DlgFileOpen(hwnd, NULL, filter, dir);
	safe_free(dir);
	if(file == NULL)
		return;

    w32g_lock_open_file = 1;
#ifdef EXT_CONTROL_MAIN_THREAD
	w32g_ext_control_main_thread(RC_EXT_LOAD_PLAYLIST, (ptr_size_t)file);
#else
	w32g_send_rc(RC_EXT_LOAD_PLAYLIST, (ptr_size_t)file);
#endif
}

#include <sys/stat.h> /* for stat() */
static int CheckOverWrite(HWND hwnd, char *filename)
{
	char buff[BUFSIZ];
	int exists;

#if 0
	FILE *fp;
    if((fp = fopen(filename, "r")) == NULL)
		exists = 0;
	else
	{
		fclose(fp);
		exists = 1;
	}
#else
	struct stat st;
	exists = (stat(filename, &st) != -1);
#endif

	if(!exists)
		return 1;
	snprintf(buff, sizeof(buff), "%s exists. Overwrite it?", filename);
	TCHAR *t = char_to_tchar(buff);
	int ret = MessageBox(hwnd, t, _T("Warning"), MB_YESNO) == IDYES;
	safe_free(t);
	return ret;
}

static void DlgPlaylistSave(HWND hwnd)
{
    OPENFILENAME ofn;
    static TCHAR *dir;
    const TCHAR *filter,
        filter_en[] =
           _T("playlist file\0*.pls;*.m3u;*.asx\0")
           _T("all files\0*.*\0")
           _T("\0\0"),
        filter_jp[] =
            _T("プレイリストファイル (*.pls;*.m3u;*.asx)\0*.pls;*.m3u;*.asx\0")
            _T("すべてのファイル (*.*)\0*.*\0")
            _T("\0\0");
	TCHAR tfilename[DialogMaxFileName] = _T("");

	if (PlayerLanguage == LANGUAGE_JAPANESE)
		filter = filter_jp;
	else
		filter = filter_en;

    if(w32g_lock_open_file)
		return;

    if(MidiFileOpenDir[0])
		dir = char_to_tchar(MidiFileOpenDir);
    else
		dir = NULL;

	memset(DialogFileNameBuff, 0, sizeof(DialogFileNameBuff));
	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.hInstance = hInst;
	ofn.lpstrFilter = filter;
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 1 ;
	ofn.lpstrFile = tfilename;
	ofn.nMaxFile = sizeof(tfilename) / sizeof(TCHAR);
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir	= dir;
	ofn.lpstrTitle	= _T("Save Playlist File");
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_HIDEREADONLY;
// ofn.lpstrDefExt = 0;
	ofn.lpstrDefExt = _T("pls");
	ofn.lCustData = 0;
	ofn.lpfnHook = 0;
	ofn.lpTemplateName= 0;

	BOOL ret = GetSaveFileName(&ofn);
	safe_free(dir);
	char *sfilename = tchar_to_char(tfilename);
	strncpy(DialogFileNameBuff, sfilename, sizeof(DialogFileNameBuff) / sizeof(char));
	safe_free(sfilename);
	DialogFileNameBuff[sizeof(DialogFileNameBuff) / sizeof(char) - 1] = '\0';
	if(ret != TRUE)
		return;
	if(!CheckOverWrite(hwnd, DialogFileNameBuff))
		return;
    w32g_lock_open_file = 1;
#ifdef EXT_CONTROL_MAIN_THREAD
	w32g_ext_control_main_thread(RC_EXT_SAVE_PLAYLIST, (ptr_size_t)DialogFileNameBuff);
#else
	w32g_send_rc(RC_EXT_SAVE_PLAYLIST, (ptr_size_t)DialogFileNameBuff);
#endif
}

// ****************************************************************************
// Edit Ctl.

void VprintfEditCtlWnd(HWND hwnd, char *fmt, va_list argList)
{
	 char buffer[BUFSIZ], out[BUFSIZ];
	 char *in;
	 int i;

	 if(!IsWindow(hwnd))
		  return;

	 vsnprintf(buffer, sizeof(buffer), fmt, argList);
	 in = buffer;
	 i = 0;
	 for(;;){
		  if(*in == '\0' || i>sizeof(out)-3){
				out[i] = '\0';
				break;
		  }
		  if(*in=='\n'){
				out[i] = 13;
				out[i+1] = 10;
				in++;
				i += 2;
				continue;
		  }
		  out[i] = *in;
		  in++;
		  i++;
	 }

     {
         int len = GetWindowTextLength(hwnd);
         Edit_SetSel(hwnd, len, len);
         Edit_ReplaceSel(hwnd, out);
     }
}

void PrintfEditCtlWnd(HWND hwnd, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    VprintfEditCtlWnd(hwnd,fmt,ap);
    va_end(ap);
}

#if 1
void PutsEditCtlWnd(HWND hwnd, char *str)
{
	char *in = str;
	int i;
	char out[BUFSIZ];
	i = 0;
	for(;;){
		if(*in == '\0' || i>sizeof(out)-3){
			out[i] = '\0';
			break;
    }
  	if(*in=='\n'){
    	out[i] = 13;
    	out[i+1] = 10;
			in++;
      i += 2;
      continue;
    }
    out[i] = *in;
		in++;
    i++;
  }
	if(IsWindow(hwnd)){
        int len = GetWindowTextLength(hwnd);
 		Edit_SetSel(hwnd, len, len);
 		Edit_ReplaceSel(hwnd,out);
	}
}
#else
void PutsEditCtlWnd(HWND hwnd, char *str)
{
	if(!IsWindow(hwnd))
		return;
	PrintfEditCtlWnd(hwnd,"%s",str);
}
#endif

void ClearEditCtlWnd(HWND hwnd)
{
	TCHAR pszVoid[] = _T("");
	if(!IsWindow(hwnd))
		return;
	if(IsWindow(hwnd)){
//		Edit_SetSel(hwnd,0,-1);
		Edit_SetSel(hwnd,-1,-1);
	}
	Edit_SetText(hwnd,pszVoid);
}


// ****************************************************************************
// Misc funciton.

int w32g_msg_box(char *message, char *title, int type)
{
	TCHAR *tmsg = char_to_tchar(message);
	TCHAR *ttitle = char_to_tchar(title);
    int ret = MessageBox(hMainWnd, tmsg, ttitle, type);
	safe_free(ttitle);
	safe_free(tmsg);
	return ret;
}


//#define RC_QUEUE_SIZE 8
#define RC_QUEUE_SIZE 48
static struct
{
    int rc;
    ptr_size_t value;
} rc_queue[RC_QUEUE_SIZE];
static volatile int rc_queue_len, rc_queue_beg, rc_queue_end;

static HANDLE w32g_lock_sem = NULL;
static HANDLE w32g_empty_sem = NULL;

void w32g_lock(void)
{
	if(w32g_lock_sem)
	    WaitForSingleObject(w32g_lock_sem, INFINITE);
}

void w32g_unlock(void)
{
	if(w32g_lock_sem)
	    ReleaseSemaphore(w32g_lock_sem, 1, NULL);
}

void w32g_send_rc(int rc, ptr_size_t value)
{
    w32g_lock();

    if(rc_queue_len == RC_QUEUE_SIZE)
    {
	/* Over flow.  Remove the oldest message */
	rc_queue_len--;
	rc_queue_beg = (rc_queue_beg + 1) % RC_QUEUE_SIZE;
    }

    rc_queue_len++;
    rc_queue[rc_queue_end].rc = rc;
    rc_queue[rc_queue_end].value = value;
    rc_queue_end = (rc_queue_end + 1) % RC_QUEUE_SIZE;
	if(w32g_empty_sem)
	    ReleaseSemaphore(w32g_empty_sem, 1, NULL);
    w32g_unlock();
}

int w32g_get_rc(ptr_size_t *value, int wait_if_empty)
{
    int rc;

    while(rc_queue_len == 0)
    {
	if(!wait_if_empty)
	    return RC_NONE;
	if(w32g_empty_sem)
		WaitForSingleObject(w32g_empty_sem, INFINITE);
	VOLATILE_TOUCH(rc_queue_len);
    } 

    w32g_lock();
    rc = rc_queue[rc_queue_beg].rc;
    *value = rc_queue[rc_queue_beg].value;
    rc_queue_len--;
    rc_queue_beg = (rc_queue_beg + 1) % RC_QUEUE_SIZE;
    w32g_unlock();

    return rc;
}

int w32g_open(void)
{
    SaveSettingTiMidity(st_current);
    memcpy(st_temp, st_current, sizeof(SETTING_TIMIDITY));

    w32g_lock_sem = CreateSemaphore(NULL, 1, 1, _T("TiMidity Mutex Lock"));
    w32g_empty_sem = CreateSemaphore(NULL, 0, 8, _T("TiMidity Empty Lock"));

    hPlayerThread = GetCurrentThread();
    w32g_wait_for_init = 1;
    hMainThread = crt_beginthreadex(NULL, 0, (LPTHREAD_START_ROUTINE)MainThread, NULL, 0, &dwMainThreadID);
	set_thread_description((ptr_size_t)hMainThread, "W32G Main Thread");

    while(w32g_wait_for_init)
    {
		Sleep(0);
		VOLATILE_TOUCH(w32g_wait_for_init);
    }
    return 0;
}

static void terminate_main_thread(void)
{
	DWORD status;

	switch(WaitForSingleObject(hMainThread, 0))
	{
	  case WAIT_OBJECT_0:
		break;
	  case WAIT_TIMEOUT:
		OnQuit();
		status = WaitForSingleObject(hMainThread, 5000);
		if(status == WAIT_TIMEOUT)
			TerminateThread(hMainThread, 0);
		break;
	  default:
		TerminateThread(hMainThread, 0);
		break;
	}
}

void w32g_close(void)
{	
	terminate_main_thread();
	if(w32g_lock_sem){
	    CloseHandle(w32g_lock_sem);
		w32g_lock_sem = NULL;
	}
	if(w32g_empty_sem){
	    CloseHandle(w32g_empty_sem);
		w32g_empty_sem = NULL;
	}
}

void w32g_restart(void)
{
	w32g_restart_gui_flag = 1;

	terminate_main_thread();
	if(w32g_lock_sem){
	    CloseHandle(w32g_lock_sem);
		w32g_lock_sem = NULL;
	}
	if(w32g_empty_sem){
	    CloseHandle(w32g_empty_sem);
		w32g_empty_sem = NULL;
	}

	/* Reset variable */
    hDebugEditWnd = 0;

	/* Now ready to start */
	w32g_open();
	w32g_restart_gui_flag = 0;
}

void w32g_ctle_play_start(int sec)
{
    char *title;

    if(sec >= 0)
    {
	SetScrollRange(hMainWndScrollbarProgressWnd, SB_CTL, 0, sec, TRUE);
	MainWndScrollbarProgressUpdate(0);
    }
    else
	MainWndScrollbarProgressUpdate(-1);

    Panel->cur_time_h = MPanel.CurTime_h = 0;
    Panel->cur_time_m = MPanel.CurTime_m = 0;
    Panel->cur_time_s = MPanel.CurTime_s = 0;
    Panel->cur_time_ss = MPanel.CurTime_ss = 0;

    MPanel.TotalTime_h = sec / 60 / 60;
    RANGE(MPanel.TotalTime_h, 0, 99);
    Panel->total_time_h = MPanel.TotalTime_h;

    sec %= 60 * 60;
    Panel->total_time_m = MPanel.TotalTime_m = sec / 60;
    Panel->total_time_s = MPanel.TotalTime_s = sec % 60;
    Panel->total_time_ss = MPanel.TotalTime_ss = 0;

    MPanel.UpdateFlag |= MP_UPDATE_TIME;

    /* Now, ready to get the title of MIDI */
    if((title = get_midi_title(MPanel.File)) != NULL)
    {
	strncpy(MPanel.Title, title, MP_TITLE_MAX);
	MPanel.UpdateFlag |= MP_UPDATE_TITLE;
    }
    MPanelUpdate();


}

void MainWndScrollbarProgressUpdate(int sec)
{
    static int lastsec = -1;

    if(sec == lastsec)
	return;

	PostMessage(hMainWnd, WM_UPDATE_SCROLLBAR_PROGRESS, 0, (LPARAM)sec);
    lastsec = sec;
}

void w32g_show_console(void)
{
	ShowWindow(hConsoleWnd, SW_SHOW);
	SendDlgItemMessage(hMainWnd, IDC_TOOLBARWINDOW_SUBWND,
					   TB_CHECKBUTTON, IDM_CONSOLE, (LPARAM)MAKELONG(TRUE, 0));
}

//
// GDI アクセスを単一スレッドに限定するためのロック機構

static HANDLE volatile hMutexGDI = NULL;
// static int volatile lock_num = 0;
int gdi_lock_ex ( DWORD timeout )
{
// lock_num++;
// ctl->cmsg(CMSG_INFO, VERB_VERBOSE,
// 		  "gdi_lock<%d %d>", GetCurrentThreadId(),lock_num );
	if(hMutexGDI==NULL){
		hMutexGDI = CreateMutex(NULL,FALSE,NULL);
		if(hMutexGDI==NULL)
			return -1;
	}
	if(WaitForSingleObject(hMutexGDI,timeout)==WAIT_FAILED){
		return -1;
	}
	return 0;
}
int gdi_lock(void)
{
	return gdi_lock_ex ( INFINITE );
}

extern int gdi_unlock(void)
{
//lock_num--;
//ctl->cmsg(CMSG_INFO, VERB_VERBOSE,
//		  "gdi_unlock<%d %d>", GetCurrentThreadId(),lock_num );
	if(hMutexGDI!=NULL){
		ReleaseMutex(hMutexGDI);
	}
	return 0;
}
