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

    w32g_main.c: Written by Daisuke Aoki <dai@y7.net>
                 Modified by Masanao Izumo <mo@goice.co.jp>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <stddef.h>
#include <commdlg.h>

#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
//#include <dir.h>

#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "output.h"
#include "controls.h"
#include "tables.h"
#include "miditrace.h"
#include "reverb.h"
#ifdef SUPPORT_SOUNDSPEC
#include "soundspec.h"
#endif /* SUPPORT_SOUNDSPEC */
#include "recache.h"
#include "arc.h"
#include "strtab.h"
#include "wrd.h"
#include "mid.defs"
#include "w32g.h"
#include "w32g_main.h"
#include "w32g_res.h"
#include "w32g_utl.h"
#include "w32g_c.h"
// #include <commctrl.h> /* FIXME */
#if defined(__CYGWIN32__) || defined(__MINGW32__)
WINAPI void InitCommonControls(void);
#endif

#define W32ENABLE_OPTIONS

extern int read_config_file(char *name, int self);
extern void timidity_init_player(void);
extern void timidity_start_initialize(void);
static void OpenSettingWnd(HWND hParentWnd);
static void SettingWndSetup(void);
static void SettingWndDefault(void);

//static void CmdLineToArgv(LPSTR lpCmdLine, int *argc, CHAR ***argv);


#ifndef CLR_INVALID
#define CLR_INVALID 0xffffffff
#endif /* CLR_INVALID */
extern int optind;

#define TMS_REPEAT 10
#define SAFE_SET(x,y) while(((x)=(y))==0);
#define SAFE_NOT(x,y) while((x)==(y));
#define SAFE_SET_NOT_NULL_REPEAT(x,y,n) \
	{ int i521342 = n; for(i521342=(z);i521342>0&&((x)=(y))==NULL;i521342--);}
#define SAFE_SET_NOT_NULL_REPEATX(x,y) \
	{ int i521342 = TMS_REPEAT; for(i521342=(z);i521342>0&&((x)=(y))==NULL;i521342--);}
#define SAFE_NOT_NULL_REPEAT(x,n) \
	{ int i521342 = n; for(i521342=(z);i521342>0&&(x)==NULL;i521342--);}
#define SAFE_NOT_NULL_REPEATX(x) \
	{ int i521342 = TMS_REPEAT; for(i521342=(z);i521342>0&&(x)==NULL;i521342--);}

#ifdef W32GUI_DEBUG
	#define SAFE_NOT(x,y) \
		while((x)==(y)) PrintfDebugWnd("Error : %lu",GetLastError());
	#define SAFE_NOT_TYPE(x,y,typestr) \
		while((x)==(y)) PrintfDebugWnd("Error(%s) : %lu",typestr,GetLastError());
	#define SAFE_SET_NOT(x,y,z) \
		while(((x)=(y))==(z)) PrintfDebugWnd("Error : %lu",GetLastError());
	#define SAFE_SET_NOT_TYPE(x,y,z,typestr) \
		while(((x)=(y))==(z)) PrintfDebugWnd("Error(%s) : %lu",typestr,GetLastError());
	#define SAFE_SET_NOT2(x,y,z1,z2) \
		while((x)=(y),(x)==(z1)||(x)==(z2)) PrintfDebugWnd("Error : %lu",GetLastError());
	#define SAFE_SET_NOT2_TYPE(x,y,z1,z2,typestr) \
  	while(((x)=(y))==0,(x)==(z1)||(x)==(z2)) PrintfDebugWnd("Error(%s) : %lu",typestr,GetLastError());
#else
	#define SAFE_NOT(x,y) while((x)==(y));
	#define SAFE_NOT_TYPE(x,y,typestr) while((x)==(y));
	#define SAFE_SET_NOT(x,y,z) while(((x)=(y))==(z));
	#define SAFE_SET_NOT4(x,y,z,typestr) while(((x)=(y))==(z));
	#define SAFE_SET_NOT2(x,y,z1,z2) while((x)=(y),(x)==(z1)||(x)==(z2));
	#define SAFE_SET_NOT2_TYPE(x,y,z1,z2,typestr) while((x)=(y),(x)==(z1)||(x)==(z2));
#endif
	#define SAFE_SET_NOT_NULL(x,y) SAFE_SET_NOT(x,y,NULL)
	#define SAFE_SET_NOT_NULL_TYPE(x,y,typestr)  SAFE_SET_NOT_TYPE(x,y,NULL,typestr)

HINSTANCE hInst ;

// HWND
HWND hStartWnd = 0;
HWND hMainWnd = 0;
HWND hDebugWnd = 0;
HWND hConsoleWnd = 0;
HWND hTracerWnd = 0;
HWND hDocWnd = 0;
HWND hListWnd = 0;
HWND hWrdWnd = 0;
HWND hSoundSpecWnd = 0;
static HWND hSettingWnd = 0;

// Main Thread.
HANDLE hMainThread = 0;
DWORD dwMainThreadID = 0;
static volatile int wait_thread_flag = 1;
typedef struct MAINTHREAD_ARGS_ {
	int *pArgc;
	char ***pArgv;
} MAINTHREAD_ARGS;
void MainThread(void *arglist);

int InitMinimizeFlag = 0;

int DebugWndStartFlag = 1;
int ConsoleWndStartFlag = 0;
int ListWndStartFlag = 0;
int TracerWndStartFlag = 0;
int DocWndStartFlag = 0;
int WrdWndStartFlag = 0;

int DebugWndFlag = 1;
int ConsoleWndFlag = 1;
int ListWndFlag = 1;
int TracerWndFlag = 0;
int DocWndFlag = 0;
int WrdWndFlag = 0;
int SoundSpecWndFlag = 0;

char *IniFile;
char *ConfigFile;
char *PlaylistFile;
char *PlaylistHistoryFile;
char *MidiFileOpenDir;
char *ConfigFileOpenDir;
char *PlaylistFileOpenDir;

int ProcessPriority;
int PlayerThreadPriority;
int MidiPlayerThreadPriority;
int MainThreadPriority;
int TracerThreadPriority;
int WrdThreadPriority;

int WrdGraphicFlag;
int TraceGraphicFlag;

int DocMaxSize;
char *DocFileExt;

HWND hDebugEditWnd = 0;
HWND hDocEditWnd = 0;
/* WinMain */
int w32gui_main(int argc, char **argv)
{
    MSG msg;
//    int argc;
//    CHAR **argv = NULL;
//    CmdLineToArgv(lpCmdLine,&argc,&argv);

// prevent several instances from being launched
//	if( (hPrevWnd = FindWindow(StartWndClassName,0)) != 0 ){
//		BringWindowToTop( hPrevWnd );
//		return 0;
//	}

    hInst = GetModuleHandle(0);
    InitCommonControls();
    w32g_initialize();

    SaveSettingPlayer(sp_temp);			// Initialize sp_temp
	LoadIniFile(sp_temp,st_temp);
	ApplySettingPlayer(sp_temp);		// At first, apply only player setting

// Main thread (1)
	{
	MAINTHREAD_ARGS arglist;
#if !(defined(__CYGWIN32__) || defined(__MINGW32__))
	SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES),NULL,TRUE};
#endif
	arglist.pArgc = &argc;
	arglist.pArgv = &argv;

#if defined(__CYGWIN32__) || defined(__MINGW32__)
	hMainThread =
	    CreateThread(NULL, 0,
			 (LPTHREAD_START_ROUTINE)MainThread,
			 &arglist, 0, &dwMainThreadID);
#else
	hMainThread = (HANDLE)_beginthreadNT(MainThread,0,(void *)&arglist,
			&sa,CREATE_SUSPENDED,&dwMainThreadID);
#endif
	}

    while(wait_thread_flag)
    {
	VOLATILE_TOUCH(wait_thread_flag);
	Sleep(100);
    }

// Window thread (2)
	InitStartWnd(SW_HIDE);

// message loop for the application
	while( GetMessage(&msg,NULL,0,0) ){
//		HandleFastSearch(msg);
//PrintfDebugWnd("H%lu M%lu WP%lu LP%lu T%lu x%d y%d\n",
//	msg.hwnd, msg.message, msg.wParam, msg.lParam, msg.time, msg.pt.x, msg.pt.y);
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (msg.wParam); // Returns the value from PostQuitMessage
}

// ***************************************************************************
// System Function

void OnExit(void)
{
	PostQuitMessage(0) ;
}

void OnDestroy(void)
{
	PlayerOnStop();
	w32g_finish_ctl();
	PlayerOnKill();
	OnExit();
}

// ***************************************************************************
// Start Window

#define STARTWND_XSIZE 100
#define STARTWND_YSIZE 100
static char StartWndClassName[] = "TiMidity_Win32GUI";

static LRESULT CALLBACK StartWinProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
void InitStartWnd(int nCmdShow)
{
   	WNDCLASS wndclass ;

	wndclass.style         = CS_HREDRAW | CS_VREDRAW ;
	wndclass.lpfnWndProc   = StartWinProc ;
	wndclass.cbClsExtra    = 0 ;
	wndclass.cbWndExtra    = 0 ;
	wndclass.hInstance     = hInst ;
	wndclass.hIcon         = LoadIcon(hInst,MAKEINTRESOURCE(IDI_ICON_TIMIDITY)) ;
	wndclass.hCursor       = LoadCursor(0,IDC_ARROW) ;
	wndclass.hbrBackground = (HBRUSH)(COLOR_SCROLLBAR + 1);
	wndclass.lpszMenuName  = NULL;
	wndclass.lpszClassName =  StartWndClassName;

	RegisterClass(&wndclass);
	hStartWnd = CreateWindowEx(WS_EX_DLGMODALFRAME,StartWndClassName,0,
		WS_OVERLAPPED  | WS_MINIMIZEBOX | WS_SYSMENU | WS_CLIPCHILDREN ,
		CW_USEDEFAULT,0,STARTWND_XSIZE,STARTWND_YSIZE,0,0,hInst,0);
	ShowWindow(hStartWnd,SW_HIDE);
//	ShowWindow(hStartWnd,nCmdShow);
	UpdateWindow(hStartWnd);
}

LRESULT CALLBACK
StartWinProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
    LRESULT res;

    switch (uMess)
    {
	HANDLE_MSG(hwnd,WM_CLOSE,DestroyWindow);
      case WM_DESTROY:
    	OnDestroy();
	break;
      case WM_CREATE:
#ifdef W32GUI_DEBUG
	InitDebugWnd(hwnd);
	while(!IsWindow(hDebugEditWnd));
#endif
	InitMainWnd(hwnd);
	InitConsoleWnd(hwnd);
	InitListWnd(hwnd);
	InitTracerWnd(hwnd);
	InitDocWnd(hwnd);
	InitWrdWnd(hwnd);
	InitSoundSpecWnd(hwnd);
	while(!IsWindow(hConsoleWnd));
	ResumeThread(hMainThread);
	break;
      default:
	if(uMess == WM_ACTIVATEAPP)
	    break;
	res = DefWindowProc(hwnd,uMess,wParam,lParam);
	return res;
    }
    return 0L;
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

int SubWindowMax = 2;
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

BOOL CALLBACK MainProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);

void update_subwindow(void);
void OnShow(void);
void OnHide(void);

void InitMainWnd(HWND hParentWnd)
{
// FIXME
	HICON hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON_TIMIDITY));
//	HICON hIcon = LoadIcon(NULL, IDI_APPLICATION);
	hMainWnd = CreateDialog(hInst,MAKEINTRESOURCE(IDD_DIALOG_MAIN),hParentWnd,MainProc);
  if ( hIcon != NULL) SendMessage( hMainWnd, WM_SETICON, FALSE, (LPARAM)hIcon);
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

static void InitCanvasWnd(HWND hwnd);
static void TmCanvasInit(HWND hwnd);
static void InitPanelWnd(HWND hwnd);
static void TmPanelInit(HWND hwnd);

static void InitMainToolbar(HWND hwnd);
#define IDM_STOP			12501
#define IDM_PAUSE			12502
#define IDM_PREV			12503
#define IDM_FOREWARD	12504
#define IDM_PLAY			12505
#define IDM_BACKWARD	12506
#define IDM_NEXT			12507
static void InitSubWndToolbar(HWND hwnd);
#define IDM_CONSOLE		12511
#define IDM_LIST			12512
#define IDM_TRACER	 	12513
#define IDM_DOC				12514
#define IDM_WRD				12515
#define IDM_SOUNDSPEC	12516

HICON hiconstop = 0;

BOOL CALLBACK
MainProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
    HDROP hDrop;
    switch (uMess)
    {
      case WM_INITDIALOG:
	update_subwindow();
	MainWndUpdateConsoleButton();
	MainWndUpdateTracerButton();
	MainWndUpdateListButton();
	MainWndUpdateDocButton();
	MainWndUpdateWrdButton();
	MainWndUpdateSoundSpecButton();
	MainWndScrollbarProgressInit();
	InitPanelWnd(hwnd);
	InitCanvasWnd(hwnd);
	InitMainToolbar(hwnd);
	InitSubWndToolbar(hwnd);
	{
	    HMENU hMenu = GetSystemMenu(hwnd, FALSE);
	    EnableMenuItem(hMenu, SC_MAXIMIZE, MF_BYCOMMAND	| MF_GRAYED);
	    EnableMenuItem(hMenu, SC_SIZE, MF_BYCOMMAND | MF_GRAYED);
	    EnableMenuItem(hMenu, SC_MOVE, MF_BYCOMMAND | MF_GRAYED);
	    InsertMenu(hMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, 0);
	    InsertMenu(hMenu, 0, MF_BYPOSITION, SC_SCREENSAVE, "Screen Saver");
	    InsertMenu(hMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, 0);
	    InsertMenu(hMenu, 0, MF_BYPOSITION | MF_STRING, IDM_STOP, "Stop");
	    InsertMenu(hMenu, 0, MF_BYPOSITION | MF_STRING, IDM_PAUSE, "Pause");
	    InsertMenu(hMenu, 0, MF_BYPOSITION | MF_STRING, IDM_PREV, "Prev");
	    InsertMenu(hMenu, 0, MF_BYPOSITION | MF_STRING, IDM_NEXT, "Next");
	    InsertMenu(hMenu, 0, MF_BYPOSITION | MF_STRING, IDM_PLAY, "Play");
	    DrawMenuBar(hwnd);
    	}
	return FALSE;
	HANDLE_MSG(hwnd,WM_COMMAND,MainCmdProc);
	HANDLE_MSG(hwnd,WM_CLOSE,DestroyWindow);
      case WM_DESTROY:
    	OnDestroy();
	return FALSE;
      case WM_SIZE:
    	if(wParam == SIZE_MINIMIZED){
	    update_subwindow();
	    OnHide();
	}
	return FALSE;
      case WM_QUERYOPEN:
	OnShow();
	return FALSE;
      case WM_DROPFILES:
	hDrop = (HDROP)wParam;
	PlayerPlaylistAddDropfiles(hDrop);
	DragFinish(hDrop);
	PlayerOnPlayLoadFile();
	MainWndSetPlayButton(1);
	MainWndSetPauseButton(0);
	TmPanelUpdate();
	UpdateListWnd();
	return FALSE;
      case WM_HSCROLL:
    {
	int nScrollCode = (int) LOWORD(wParam);
	int nPos = (int) HIWORD(wParam);
	HWND hwndScrollBar = (HWND) lParam;
	if(hwndScrollBar == hMainWndScrollbarProgressWnd){
	    if(nScrollCode == SB_THUMBPOSITION){
           	MainWndScrollbarProgressApply(nPos);
	    } else if(nScrollCode == SB_LINELEFT){
		nPos = GetScrollPos(hwndScrollBar,SB_CTL);
		nPos--;
		MainWndScrollbarProgressApply(nPos);
		MainWndScrollbarProgressNotUpdate = 1;
	    } else if(nScrollCode == SB_LINERIGHT){
		nPos = GetScrollPos(hwndScrollBar,SB_CTL);
		nPos++;
		MainWndScrollbarProgressApply(nPos);
		MainWndScrollbarProgressNotUpdate = 1;
	    }
	    if(nScrollCode == SB_THUMBTRACK)
		MainWndScrollbarProgressNotUpdate = 1;
    	    else
		MainWndScrollbarProgressNotUpdate = 0;
	}
    }
	return FALSE;
      case WM_VSCROLL:
	{
	    int nScrollCode = (int) LOWORD(wParam);
	    int nPos = (int) HIWORD(wParam);
	    HWND hwndScrollBar = (HWND) lParam;
	    if(hwndScrollBar == hMainWndScrollbarVolumeWnd){
		if(nScrollCode == SB_THUMBPOSITION){
		    MainWndScrollbarVolumeApply(nPos);
		} else if(nScrollCode == SB_LINEUP){
		    nPos = GetScrollPos(hwndScrollBar,SB_CTL);
		    nPos--;
		    MainWndScrollbarVolumeApply(nPos);
		    MainWndScrollbarVolumeNotUpdate = 1;
		} else if(nScrollCode == SB_LINEDOWN){
		    nPos = GetScrollPos(hwndScrollBar,SB_CTL);
		    nPos++;
		    MainWndScrollbarVolumeApply(nPos);
		    MainWndScrollbarVolumeNotUpdate = 1;
		}
		if(nScrollCode == SB_THUMBTRACK)
		    MainWndScrollbarVolumeNotUpdate = 1;
		else
		    MainWndScrollbarVolumeNotUpdate = 0;
	    }
	}
	return FALSE;
      case WM_SYSCOMMAND:
	switch(wParam){
	  case IDM_STOP:
	  case IDM_PAUSE:
	  case IDM_PREV:
	  case IDM_PLAY:
	  case IDM_NEXT:
	    SendMessage(hwnd,WM_COMMAND,wParam,NULL);
	    break;
	  default:
	    break;
	}
	return FALSE;
	/*
	   case WM_SETFOCUS:
	   HideCaret((HWND)wParam);
	   break;
	   case WM_KILLFOCUS:
	   ShowCaret((HWND)wParam);
	   break;
	   */
      default:
	return FALSE;
    }
}

void MainCmdProc(HWND hwnd, int wId, HWND hwndCtl, UINT wNotifyCode)
{
    int iRes;
    switch(wId)
    {
      case IDM_STOP:
	PlayerOnStop();
	MainWndSetPlayButton(0);
	MainWndSetPauseButton(0);
	break;
      case IDM_PAUSE:
  	iRes = PlayerOnPause();
	MainWndSetPauseButton(iRes);
	break;
      case IDM_PREV:
   	PlayerOnPrevPlay();
	MainWndSetPlayButton(1);
	MainWndSetPauseButton(0);
	break;
      case IDM_BACKWARD:
	break;
      case IDM_PLAY:
  	PlayerOnPlay();
	MainWndSetPlayButton(1);
	MainWndSetPauseButton(0);
	break;
      case IDM_FOREWARD:
	break;
      case IDM_NEXT:
   	PlayerOnNextPlay();
	MainWndSetPlayButton(1);
	MainWndSetPauseButton(0);
	break;
      case IDM_CONSOLE:
      case IDM_MWCONSOLE:
	ToggleSubWindow(hConsoleWnd);
	break;
      case IDM_TRACER:
      case IDM_MWTRACER:
/*		ToggleSubWindow(hTracerWnd); */
	MainWndUpdateTracerButton();
	break;
      case IDM_LIST:
      case IDM_MWPLAYLIST:
	ToggleSubWindow(hListWnd);
	break;
      case IDM_DOC:
      case IDM_MWDOCUMENT:
/*		ToggleSubWindow(hDocWnd); */
	MainWndUpdateDocButton();
	break;
      case IDM_WRD:
      case IDM_MWWRDTRACER:
/*		ToggleSubWindow(hWrdWnd); */
	MainWndUpdateWrdButton();
	break;
      case IDM_SOUNDSPEC:
      case IDM_MWSOUNDSPEC:
/*		ToggleSubWindow(hSoundSpecWnd); */
	MainWndUpdateSoundSpecButton();
	break;
      case IDOK:
	break;
      case IDCANCEL:
	OnExit();
	break;
      case IDM_MFOPENFILE:
	DlgMidiFileOpen();
	break;
      case IDM_MFOPENDIR:
	break;
      case IDM_MFLOADPLAYLIST:
	break;
      case IDM_MFSAVEPLAYLISTAS:
	break;
      case IDM_MFEXIT:
	OnExit();
	break;

      case IDM_SETTING:
	OpenSettingWnd(hwnd);
	break;

      case IDM_MCSAVEINIFILE:
	SaveIniFile(sp_current, st_current);
	break;
      case IDM_MCLOADINIFILE:
	if(player_status != PLAYERSTATUS_STOP)
	{
	    MessageBox(hMainWnd, "Can't load Ini file while playing",
		       "Warning", MB_OK);
	    break;
	}
	LoadIniFile(sp_current,st_current);
	ApplySettingPlayer(sp_current);
	ApplySettingTimidity(st_current);
	SettingWndSetup();
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
	break;
      case IDM_MHHELP:
	break;
      case IDM_MHVERSION:
	VersionWnd(hwnd);
	break;
      case IDM_MHTIMIDITY:
	TiMidityWnd(hwnd);
	break;
    }
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
}

void ShowSubWindow(HWND hwnd,int showflag)
{
	int i, num;
  RECT rc,rc2;
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
		if(max>0){
			GetWindowRect(hwnd, &rc2);
			MoveWindow(hwnd,rc.left,rc.top,rc2.right-rc2.left,rc2.bottom-rc2.top,TRUE);
		}
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

//-----------------------------------------------------------------------------
// Toolbar Main

static TBBUTTON MainTbb[] = {
    {4, IDM_STOP, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0},
    {3, IDM_PAUSE, TBSTATE_ENABLED, TBSTYLE_CHECK, 0, 0},
    {0, IDM_PREV, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0},
    {1, IDM_FOREWARD, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0},
    {2, IDM_PLAY, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0},
    {5, IDM_BACKWARD, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0},
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

static TBBUTTON SubWndTbb[] = {
    {3, IDM_CONSOLE, TBSTATE_ENABLED, TBSTYLE_CHECK, 0, 0},
    {1, IDM_LIST, TBSTATE_ENABLED, TBSTYLE_CHECK, 0, 0},
    {2, IDM_TRACER, TBSTATE_ENABLED, TBSTYLE_CHECK, 0, 0},
    {0, IDM_DOC, TBSTATE_ENABLED, TBSTYLE_CHECK, 0, 0},
    {4, IDM_WRD, TBSTATE_ENABLED, TBSTYLE_CHECK, 0, 0},
};

static void InitSubWndToolbar(HWND hwnd)
{
	TBADDBITMAP SubWndTbab;
  SendDlgItemMessage(hwnd, IDC_TOOLBARWINDOW_SUBWND,
  		TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
  SendDlgItemMessage(hwnd, IDC_TOOLBARWINDOW_SUBWND,
  		TB_SETBUTTONSIZE, (WPARAM)0, (LPARAM)MAKELONG(16,16));
	SubWndTbab.hInst = hInst;
	SubWndTbab.nID =(int)IDB_BITMAP_SUBWND_BUTTON;
  SendDlgItemMessage(hwnd, IDC_TOOLBARWINDOW_SUBWND,
  	TB_ADDBITMAP, 5, (LPARAM)&SubWndTbab);
  SendDlgItemMessage(hwnd, IDC_TOOLBARWINDOW_SUBWND,
  	TB_ADDBUTTONS, (WPARAM)5,(LPARAM)&SubWndTbb);
  SendDlgItemMessage(hwnd, IDC_TOOLBARWINDOW_SUBWND,
		TB_AUTOSIZE, 0, 0);
}










//-----------------------------------------------------------------------------
// Canvas Window

#define TM_CANVAS_XMAX 160
#define TM_CANVAS_YMAX 160
// #define TM_CANVAS_XMAX 128
// #define TM_CANVAS_YMAX 108
#define TM_CANVASMAP_XMAX 16
#define TM_CANVASMAP_YMAX 16
struct TmCanvas_ {
	HWND hwnd;
	HWND hParentWnd;
	int left;
  int top;
	int right;
  int bottom;
	int	rectx;
  int recty;
  int spacex;
  int spacey;
	char map[TM_CANVASMAP_XMAX][TM_CANVASMAP_YMAX];
	char map2[TM_CANVASMAP_XMAX][TM_CANVASMAP_YMAX];
	char bar[TM_CANVASMAP_XMAX];
	char bar_old[TM_CANVASMAP_XMAX];
	int bar_delay;
	uint32 xnote[MAX_W32G_MIDI_CHANNELS][4];
	uint32 xnote_old[MAX_W32G_MIDI_CHANNELS][4];
	int xnote_reset;
	HDC hdc;
	HDC hmdc;
	HGDIOBJ hgdiobj_hmdcprev;
  HBITMAP hbitmap;
  HBITMAP hSleepBitmap;
} TmCanvas;

// int TmCanvasMode = TMCM_CHANNEL;
int TmCanvasMode = TMCM_SLEEP;

#define IDC_CANVAS 4242

static HWND hCanvasWnd;
static char CanvasWndClassName[] = "TiMidity Canvas";
static LRESULT CALLBACK CanvasWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static void TmCanvasClear(void);
static void TmCanvasChange(void);

static void InitCanvasWnd(HWND hwnd)
{
	WNDCLASS wndclass ;
	hCanvasWnd = 0;
	wndclass.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
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
	while(hCanvasWnd==0){
		hCanvasWnd = CreateWindowEx(0,CanvasWndClassName,0,
  		WS_CHILD,
			CW_USEDEFAULT,0,TM_CANVAS_XMAX,TM_CANVAS_YMAX,GetDlgItem(hwnd,IDC_RECT_CANVAS),0,hInst,0);
  }
	ShowWindow(hCanvasWnd,SW_SHOW);
	UpdateWindow(hCanvasWnd);
}

static LRESULT CALLBACK
CanvasWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess)
	{
		case WM_CREATE:
			TmCanvasInit(hwnd);
			break;
		case WM_PAINT:
			TmCanvasUpdate();
    	return 0;
		case WM_TM_CANVAS_UPDATE:
			TmCanvasUpdate();
    	break;
		case WM_LBUTTONDBLCLK:
			TmCanvasChange();
			break;
		default:
			return DefWindowProc(hwnd,uMess,wParam,lParam) ;
	}
	return 0L;
}

#define TMCCC_FORE	 	RGB(0x00,0x00,0x00)
#define TMCCC_BACK 		RGB(0x00,0xf0,0x00)
#define TMCCC_LOW			RGB(0x80,0xd0,0x00)
#define TMCCC_MIDDLE	RGB(0xb0,0xb0,0x00)
#define TMCCC_HIGH		RGB(0xe0,0x00,0x00)

#define TMCCF_NONE		1
#define TMCCF_HALF		1

#define TMCC_BACK					0
#define TMCC_FORE					1
#define TMCC_LOW					2
#define TMCC_MIDDLE				3
#define TMCC_HIGH					4
#define TMCC_FORE_HALF		11
#define TMCC_LOW_HALF			12
#define TMCC_MIDDLE_HALF	13
#define TMCC_HIGH_HALF		14
#define TMCC_FORE_WEAKHALF		15

static COLORREF WeakHalfColor(COLORREF fc, COLORREF bc)
{
	return fc*1/3 + bc*2/3;
}

static COLORREF HalfColor(COLORREF fc, COLORREF bc)
{
	return fc*1/6 + bc*5/6;
}

static COLORREF TmCc(int c)
{
  switch(c){
  	case TMCC_BACK:
			return TMCCC_BACK;
  	case TMCC_FORE:
			return TMCCC_FORE;
  	case TMCC_FORE_HALF:
			return HalfColor(TMCCC_FORE,TMCCC_BACK);
  	case TMCC_FORE_WEAKHALF:
			return WeakHalfColor(TMCCC_FORE,TMCCC_BACK);
  	case TMCC_LOW:
			return TMCCC_LOW;
  	case TMCC_MIDDLE:
			return TMCCC_MIDDLE;
  	case TMCC_HIGH:
			return TMCCC_HIGH;
  	case TMCC_LOW_HALF:
			return HalfColor(TMCCC_LOW,TMCCC_BACK);
  	case TMCC_MIDDLE_HALF:
			return HalfColor(TMCCC_MIDDLE,TMCCC_BACK);
  	case TMCC_HIGH_HALF:
			return HalfColor(TMCCC_HIGH,TMCCC_BACK);
		default:
			return TMCCC_BACK;
	}
}

static HANDLE hCanvasSemaphore = NULL;
// #define CANVAS_BEGIN() WaitForSingleObject(hCanvasSemaphore, INFINITE)
// #define CANVAS_END() ReleaseSemaphore(hCanvasSemaphore, 1, NULL)
#define CANVAS_BEGIN()
#define CANVAS_END()

static int TmCanvasStart = 0;
static void TmCanvasInit(HWND hwnd)
{
  RECT rc;
	if(hCanvasSemaphore==NULL)
		hCanvasSemaphore = CreateSemaphore(NULL, 1, 1, "TiMidityCanvasSemaphore");
	CANVAS_BEGIN();
	TmCanvas.hwnd = hwnd;
	TmCanvas.hParentWnd = GetParent(TmCanvas.hwnd);
	GetClientRect(TmCanvas.hParentWnd,&rc);
  MoveWindow(TmCanvas.hwnd,0,0,rc.right-rc.left,rc.bottom-rc.top,FALSE);
	TmCanvas.left = 3;
	TmCanvas.top = 2;
	TmCanvas.right = 64;
	TmCanvas.bottom = 64;
	TmCanvas.rectx = 5;
	TmCanvas.recty = 2;
	TmCanvas.spacex = 1;
	TmCanvas.spacey = 1;
	TmCanvas.bar_delay = 1;
	TmCanvas.hdc = GetDC(TmCanvas.hwnd);
  SAFE_SET_NOT_NULL(TmCanvas.hbitmap,
  	CreateCompatibleBitmap(TmCanvas.hdc,TM_CANVAS_XMAX,TM_CANVAS_YMAX));
  SAFE_SET_NOT_NULL(TmCanvas.hmdc,CreateCompatibleDC(TmCanvas.hdc));
	TmCanvas.hgdiobj_hmdcprev = SelectObject(TmCanvas.hmdc,TmCanvas.hbitmap);
	ReleaseDC(TmCanvas.hwnd,TmCanvas.hdc);
  TmCanvas.hSleepBitmap = LoadBitmap(hInst,MAKEINTRESOURCE(IDB_BITMAP_SLEEP));
	CANVAS_END();
	TmCanvasReset();
	TmCanvasStart = 1;
	TmCanvasClear();
	TmCanvasSet();
	TmCanvasUpdate();
}

static void TmCanvasClearChannelMode(void)
{
	HPEN hPen;
  HBRUSH hBrush;
	HGDIOBJ hgdiobj_hpen, hgdiobj_hbrush;
	SAFE_SET_NOT_NULL(hPen,CreatePen(PS_SOLID,1,TmCc(TMCC_BACK)));
	SAFE_SET_NOT_NULL(hBrush,CreateSolidBrush(TmCc(TMCC_BACK)));
	hgdiobj_hpen = SelectObject(TmCanvas.hmdc, hPen);
	hgdiobj_hbrush = SelectObject(TmCanvas.hmdc, hBrush);
	Rectangle(TmCanvas.hmdc,0,0,TM_CANVAS_XMAX,TM_CANVAS_YMAX);
	SelectObject(TmCanvas.hmdc, hgdiobj_hpen);
  DeleteObject(hPen);
  SelectObject(TmCanvas.hmdc, hgdiobj_hbrush);
  DeleteObject(hBrush);
	InvalidateRect(hCanvasWnd, NULL, FALSE);
}

static void TmCanvasResetChannelMode(void)
{
	int i,j;
	for(i=0;i<TM_CANVASMAP_XMAX;i++){
		for(j=0;j<TM_CANVASMAP_YMAX;j++){
    	TmCanvas.map[i][j] = TMCC_FORE_HALF;
    	TmCanvas.map2[i][j] = -1;
		}
    TmCanvas.bar[i] = -1;
    TmCanvas.bar_old[i] = -1;
	}
	TmCanvas.bar_delay = 1;
	InvalidateRect(hCanvasWnd, NULL, FALSE);
}

static void TmCanvasUpdateChannelMode(void)
{
	PAINTSTRUCT ps;
	RECT rc;
	int i,j,k,l;
	int change_flag = 0;
	if(!TmCanvasStart)
  	return;
	for(i=0;i<TM_CANVASMAP_XMAX;i++)
		for(j=0;j<TM_CANVASMAP_YMAX;j++){
			if(TmCanvas.map[i][j]!=TmCanvas.map2[i][j]){
				int x,y;
				COLORREF color = TmCc(TmCanvas.map[i][j]);
				for(k=0;k<TmCanvas.rectx;k++)
					for(l=0;l<TmCanvas.recty;l++){
          	x = TmCanvas.left + (TmCanvas.rectx + TmCanvas.spacex) * i + k;
        		y = TmCanvas.top + (TmCanvas.recty + TmCanvas.spacey) * j + l;
          	SetPixelV(TmCanvas.hmdc,x,y,color);
					}
				TmCanvas.map2[i][j] = TmCanvas.map[i][j];
				change_flag = 1;
    	}
		}
	TmCanvas.hdc = BeginPaint(TmCanvas.hwnd, &ps);
	GetClientRect(TmCanvas.hwnd, &rc);
//	BitBlt(TmCanvas.hdc,0,0,TM_CANVAS_XMAX,TM_CANVAS_YMAX,TmCanvas.hmdc,0,0,SRCCOPY);
	BitBlt(TmCanvas.hdc,rc.left,rc.top,rc.right,rc.bottom,TmCanvas.hmdc,0,0,SRCCOPY);
	EndPaint(TmCanvas.hwnd, &ps);
	if(change_flag)
		InvalidateRect(hCanvasWnd, NULL, FALSE);
}

static void TmCanvasSetChannelMode(void)
{
	int i,ch,v;
	if(Panel==NULL)
  	return;
	for(ch=0;ch<TM_CANVASMAP_XMAX;ch++){
		if(Panel->v_flags[ch] == FLAG_NOTE_ON)
			v = Panel->ctotal[ch]/8;
    else
    	v = 0;
		if(v<0) v = 0; else if(v>15) v = 15;
// Vibrate bar.
#if 1
		if(v != TmCanvas.bar[ch]){
#else
		if(v == TmCanvas.bar[ch]){
			v = v * (rand()%7 + 7) / 10;
      if(v<0)
      	v = 0;
    }
    {
#endif
    	if(TmCanvas.bar_delay){
     		int old = TmCanvas.bar[ch];
				if(TmCanvas.bar[ch]<0)
					TmCanvas.bar[ch] = v;
				else
					TmCanvas.bar[ch] = (TmCanvas.bar[ch]*10*1/3 + v*10*2/3)/10;
				if(old == TmCanvas.bar[ch]){
     			if(v>TmCanvas.bar[ch])
						TmCanvas.bar[ch] = old + 1;
     			else if(v<TmCanvas.bar[ch])
						TmCanvas.bar[ch] = old - 1;
      	}
			} else
				TmCanvas.bar[ch] = v;
		}
		for(i=0;i<=15;i++){
  		if(i<TmCanvas.bar[ch])
				TmCanvas.map[ch][TM_CANVASMAP_XMAX-1-i] = TMCC_FORE;
#if 1
  		else if(i<TmCanvas.bar_old[ch])
				TmCanvas.map[ch][TM_CANVASMAP_XMAX-1-i] = TMCC_FORE_WEAKHALF;
#endif
    	else
				TmCanvas.map[ch][TM_CANVASMAP_XMAX-1-i] = TMCC_FORE_HALF;
		}
		TmCanvas.bar_old[ch] = TmCanvas.bar[ch];
  }
	InvalidateRect(hCanvasWnd, NULL, FALSE);
}

static void TmCanvasClearSleepMode(void)
{
	RECT rc;
	HPEN hPen;
  HBRUSH hBrush;
	HGDIOBJ hgdiobj_hpen, hgdiobj_hbrush;
	SAFE_SET_NOT_NULL(hPen,CreatePen(PS_SOLID,1,GetSysColor(COLOR_SCROLLBAR)));
	SAFE_SET_NOT_NULL(hBrush,CreateSolidBrush(GetSysColor(COLOR_SCROLLBAR)));
	hgdiobj_hpen = SelectObject(TmCanvas.hmdc, hPen);
	hgdiobj_hbrush = SelectObject(TmCanvas.hmdc, hBrush);
	GetClientRect(TmCanvas.hwnd, &rc);
	Rectangle(TmCanvas.hmdc,rc.left,rc.top,rc.right,rc.bottom);
	SelectObject(TmCanvas.hmdc, hgdiobj_hpen);
  DeleteObject(hPen);
  SelectObject(TmCanvas.hmdc, hgdiobj_hbrush);
  DeleteObject(hBrush);
	InvalidateRect(hCanvasWnd, NULL, FALSE);
}

#define BITMAP_SLEEP_SIZEX 96
#define BITMAP_SLEEP_SIZEY 64
static int TmCanvasSleepModeDone = 0;
static void TmCanvasResetSleepMode(void)
{
	HDC hdc;
	RECT rc;
	int x,y;
	TmCanvasClearSleepMode();
  SAFE_SET_NOT_NULL(hdc,CreateCompatibleDC(TmCanvas.hmdc));
	SelectObject(hdc,TmCanvas.hSleepBitmap);
	GetClientRect(TmCanvas.hwnd, &rc);
	x = (rc.right - rc.left - BITMAP_SLEEP_SIZEX)/2;
	y = (rc.bottom - rc.top - BITMAP_SLEEP_SIZEY)/2;
	if(x<0) x = 0;
	if(y<0) y = 0;
	BitBlt(TmCanvas.hmdc,x,y,BITMAP_SLEEP_SIZEX,BITMAP_SLEEP_SIZEY,hdc,0,0,SRCCOPY);
	DeleteDC(hdc);
	TmCanvasSleepModeDone = 0;
}

static void TmCanvasSetSleepMode(void)
{
}

static void TmCanvasUpdateSleepMode(void)
{
	PAINTSTRUCT ps;
	RECT rc;
	TmCanvas.hdc = BeginPaint(TmCanvas.hwnd, &ps);
	GetClientRect(TmCanvas.hwnd, &rc);
	BitBlt(TmCanvas.hdc,rc.left,rc.top,rc.right,rc.bottom,TmCanvas.hmdc,0,0,SRCCOPY);
	EndPaint(TmCanvas.hwnd, &ps);
	if(!TmCanvasSleepModeDone){
		InvalidateRect(hCanvasWnd, NULL, FALSE);
		TmCanvasSleepModeDone = 1;
	}
}






#define TCTM_MAX_CHANNELS 16
static void TmCanvasResetTracerMode(void)
{
	int i,j;
  for(i=0;i<TCTM_MAX_CHANNELS;i++){
		for(j=0;j<4;j++){
			TmCanvas.xnote[i][j] = 0;
			TmCanvas.xnote_old[i][j] = 0;
		}
  }
	TmCanvas.xnote_reset = 1;
}

#define TCTM_ON RGB(0xff,0x00,0x00)
#define TCTM_OFF_WHITE RGB(0xff,0xff,0xff)
#define TCTM_OFF_BRACK RGB(0x00,0x00,0x00)
static void TmCanvasUpdateTracerMode(void)
{
	PAINTSTRUCT ps;
	RECT rc;
	int i,j;
	int change_flag = 0;
	if(!TmCanvasStart)
  	return;

  for(i=0;i<TCTM_MAX_CHANNELS;i++){
		if(change_flag)
    	break;
		for(j=0;j<4;j++){
			if(TmCanvas.xnote[i][j] != TmCanvas.xnote_old[i][j]){
				change_flag = 1;
				break;
			}
		}
  }

/*
	if(change_flag || TmCanvas.xnote_reset){
	  for(i=0;i<TCTM_MAX_CHANNELS;i++){
			for(j=0;j<4;j++){
				int32 diff = TmCanvas.xnote[i][j] ^ TmCanvas.xnote_old[i][j];
				for(k=0;k<32;k++){
  	      int32 reff = (int32)1 << k;
    	   	int note = j*32+k;
					COLORREF color;
					if(note < 24 || note >= 120)
  	      	continue;
					note = note % 12;
					if(TmCanvas.xnote[i][j] & reff)
						color = TCTM_ON;
  				else if(note == 1 || note == 3 || note == 6 || note == 8 || note == 10)
						color = TCTM_OFF_BRACK;
	        else
  	      	color = TCTM_OFF_WHITE;
					if(TmCanvas.xnote_reset || diff & reff){
						int x,y;
  	      	x = TmCanvas.left + j * 32 + k - 24;
    	   		y = TmCanvas.top + i * 4;
	          if(note == 1 || note == 3 || note == 6 || note == 8 || note == 10){
							for(l=0;l<2;l++)
	  	        	SetPixelV(TmCanvas.hmdc,x,y+l,color);
						} else {
							for(l=0;l<3;l++)
	  	        	SetPixelV(TmCanvas.hmdc,x,y+l,color);
						}
					}
				}
		  }
		}
	}
*/
	TmCanvas.hdc = BeginPaint(TmCanvas.hwnd, &ps);
	GetClientRect(TmCanvas.hwnd, &rc);
	BitBlt(TmCanvas.hdc,rc.left,rc.top,rc.right,rc.bottom,TmCanvas.hmdc,0,0,SRCCOPY);
	EndPaint(TmCanvas.hwnd, &ps);
 	if(change_flag || TmCanvas.xnote_reset)
		InvalidateRect(hCanvasWnd, NULL, FALSE);
  TmCanvas.xnote_reset = 0;
}

static void TmCanvasSetTracerMode(void)
{
	int i,j,k,l;
	int change_flag = 0;
	int32 diff;
 	int32 reff;
	int note;
	COLORREF color;
	int x,y;

	if(Panel!=NULL){
  	for(i=0;i<TCTM_MAX_CHANNELS;i++){
			for(j=0;j<4;j++){
				TmCanvas.xnote_old[i][j] = TmCanvas.xnote[i][j];
				TmCanvas.xnote[i][j] = Panel->xnote[i][j];
				if(TmCanvas.xnote[i][j] != TmCanvas.xnote_old[i][j]){
					change_flag = 1;
				}
    	}
  	}
	}
	if(change_flag || TmCanvas.xnote_reset){
	  for(i=0;i<TCTM_MAX_CHANNELS;i++){
			for(j=0;j<4;j++){
				diff = TmCanvas.xnote[i][j] ^ TmCanvas.xnote_old[i][j];
				for(k=0;k<32;k++){
  	      reff = (int32)1 << k;
    	   	note = j*32+k;
					if(note < 24 || note >= 120)
  	      	continue;
					note = note % 12;
					if(TmCanvas.xnote[i][j] & reff)
						color = TCTM_ON;
  				else if(note == 1 || note == 3 || note == 6 || note == 8 || note == 10)
						color = TCTM_OFF_BRACK;
	        else
  	      	color = TCTM_OFF_WHITE;
					if(TmCanvas.xnote_reset || diff & reff){
  	      	x = TmCanvas.left + j * 32 + k - 24;
    	   		y = TmCanvas.top + i * 4;
	          if(note == 1 || note == 3 || note == 6 || note == 8 || note == 10){
							for(l=0;l<2;l++)
	  	        	SetPixelV(TmCanvas.hmdc,x,y+l,color);
  	        	SetPixelV(TmCanvas.hmdc,x,y+2,TCTM_OFF_WHITE);
						} else {
							for(l=0;l<3;l++)
	  	        	SetPixelV(TmCanvas.hmdc,x,y+l,color);
						}
					}
				}
		  }
		}
		InvalidateRect(hCanvasWnd, NULL, FALSE);
	}
}

static void TmCanvasClearTracerMode(void)
{
	TmCanvasClearChannelMode();
}














volatile static int TmCanvasClearFlag = 0;
static void TmCanvasClear(void)
{
	if(TmCanvasClearFlag)
		return;
	CANVAS_BEGIN();
	TmCanvasClearFlag = 1;
	switch(TmCanvasMode){
		case TMCM_CHANNEL:
			TmCanvasClearChannelMode();
      break;
		case TMCM_TRACER:
			TmCanvasClearTracerMode();
			break;
		case TMCM_SLEEP:
		default:
			TmCanvasClearSleepMode();
			break;
  }
	TmCanvasClearFlag = 0;
	CANVAS_END();
}

volatile static int TmCanvasUpdateFlag = 0;
void TmCanvasUpdate(void)
{
	if(!TmCanvasStart)
  	return;
	if(TmCanvasUpdateFlag)
		return;
	CANVAS_BEGIN();
	TmCanvasClearFlag = 1;
	switch(TmCanvasMode){
		case TMCM_CHANNEL:
			TmCanvasUpdateChannelMode();
      break;
		case TMCM_TRACER:
			TmCanvasUpdateTracerMode();
			break;
		case TMCM_SLEEP:
		default:
			TmCanvasUpdateSleepMode();
			break;
  }
	TmCanvasClearFlag = 0;
	CANVAS_END();
}

volatile static int TmCanvasResetFlag = 0;
void TmCanvasReset(void)
{
	if(!TmCanvasStart)
  	return;
	if(TmCanvasResetFlag)
		return;
	CANVAS_BEGIN();
	TmCanvasResetFlag = 1;
	switch(TmCanvasMode){
		case TMCM_CHANNEL:
			TmCanvasResetChannelMode();
      break;
		case TMCM_TRACER:
			TmCanvasResetTracerMode();
      break;
		case TMCM_SLEEP:
		default:
			TmCanvasResetSleepMode();
      break;
  }
	TmCanvasResetFlag = 0;
	CANVAS_END();
}

volatile static int TmCanvasSetFlag = 0;
void TmCanvasSet(void)
{
	if(!TmCanvasStart)
  	return;
	if(TmCanvasSetFlag)
		return;
	CANVAS_BEGIN();
	TmCanvasSetFlag = 1;
	switch(TmCanvasMode){
		case TMCM_CHANNEL:
			TmCanvasSetChannelMode();
      break;
		case TMCM_TRACER:
			TmCanvasSetTracerMode();
			break;
		case TMCM_SLEEP:
		default:
			TmCanvasSetSleepMode();
			break;
  }
	TmCanvasSetFlag = 0;
	CANVAS_END();
}

static void TmCanvasChange(void)
{
    TmCanvasClear();
    TmCanvasReset();
    TmCanvasSet();
    TmCanvasUpdate();
    switch(TmCanvasMode){
      case TMCM_SLEEP:
	TmCanvasMode = TMCM_CHANNEL;
	break;
      case TMCM_CHANNEL:
	TmCanvasMode = TMCM_TRACER;
	break;
      case TMCM_TRACER:
	TmCanvasMode = TMCM_SLEEP;
	break;
      default:
	TmCanvasMode = TMCM_SLEEP;
	break;
    }
    TmCanvasClear();
    TmCanvasReset();
    TmCanvasSet();
    TmCanvasUpdate();
}








































//-----------------------------------------------------------------------------
// Main Panel

#define TM_PANEL_XMAX 350
#define TM_PANEL_YMAX 100

#define TMP_SPACE 2
#define TMP_TITLE_CHAR_SIZE 12
#define TMP_FILE_CHAR_SIZE 10
#define TMP_MISCH_CHAR_SIZE 9
#define TMP_MISC_CHAR_SIZE 12
#define TMP_3L_CSIZE TMP_MISC_CHAR_SIZE
#define TMP_3L_CSSIZE TMP_MISCH_CHAR_SIZE
#define TMP_3L_SPACE 1
#define TMP_4L_CSIZE TMP_MISC_CHAR_SIZE
#define TMP_4L_CSSIZE TMP_MISCH_CHAR_SIZE
#define TMP_4L_SPACE 1

static HFONT hTitleFont = NULL;
static HFONT hFileFont = NULL;
static HFONT hMiscHFont = NULL;
static HFONT hMiscFont = NULL;

#define TMP_FONT_SIZE 12
#define TMP_FONTSMALL_SIZE 10
#define TMP_TITLE_MAX 256
#define TMP_FILE_MAX 256
struct TmPanel_ {
	HWND hwnd;
	HWND hParentWnd;
	RECT rcMe;
	RECT rcTitleH;
	RECT rcTitle;
	RECT rcFileH;
	RECT rcFile;
  RECT rcTimeH;
  RECT rcTime;
	RECT rcVoicesH;
	RECT rcVoices;
	RECT rcMasterVolume;
	RECT rcMasterVolumeH;
	RECT rcListH;
	RECT rcList;
	int TitleDone;
  int FileDone;
  int TimeDone;
  int VoicesDone;
	int MasterVolumeDone;
	int ListDone;
	int TitleHDone;
  int FileHDone;
  int TimeHDone;
  int VoicesHDone;
	int MasterVolumeHDone;
	int ListHDone;
	char Title[TMP_TITLE_MAX+1];
	char File[TMP_FILE_MAX+1];
	int cur_time_h;
	int cur_time_m;
	int cur_time_s;
	int cur_time_ss;
	int total_time_h;
	int total_time_m;
	int total_time_s;
	int total_time_ss;
	int cur_voices;
	int voices;
#if 0 /* Not used */
	int upper_voices;
#endif
  int master_volume;
	int cur_pl_num;
  int playlist_num;
	HDC hdc;
	HDC hmdc;
	HGDIOBJ hgdiobj_hmdcprev;
  HBITMAP hbitmap;
	HFONT hfont;
	HFONT hfontsmall;
  char font[256];
} TmPanel;

static HWND hPanelWnd;
static char PanelWndClassName[] = "TiMidity Panel";
static LRESULT CALLBACK PanelWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static void TmPanelClear(void);
int TmPanelMode = 0;
static int TmPanelAllUpdate = 1;

static void InitPanelWnd(HWND hwnd)
{
	WNDCLASS wndclass ;
	hPanelWnd = 0;
	wndclass.style         = CS_HREDRAW | CS_VREDRAW ;
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
  SAFE_SET_NOT_NULL(
  	hPanelWnd,
  	CreateWindowEx(0,PanelWndClassName,0,WS_CHILD,
    	CW_USEDEFAULT,0,TM_PANEL_XMAX,TM_PANEL_YMAX,
      GetDlgItem(hwnd,IDC_RECT_PANEL),0,hInst,0)
	);
	ShowWindow(hPanelWnd,SW_SHOW);
	UpdateWindow(hPanelWnd);
}

static LRESULT CALLBACK
PanelWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess)
	{
		case WM_CREATE:
			TmPanelInit(hwnd);
			break;
		case WM_PAINT:
			TmPanelUpdate();
//			SendMessage(GetParent(hwnd),WM_PAINT,0,0);
    	return 0;
		default:
			return DefWindowProc(hwnd,uMess,wParam,lParam) ;
	}
	return 0L;
}

#define TMP_DONOT 1
#define TMP_PREPARE 2
#define TMP_DONE 3

static int TmPanelStart = 0;
static void TmPanelInit(HWND hwnd)
{
	int i,top,bottom;
	RECT rc;
	TmPanel.hwnd = hwnd;
	TmPanel.hParentWnd = GetParent(TmPanel.hwnd);
	GetClientRect(TmPanel.hParentWnd,&rc);
  MoveWindow(TmPanel.hwnd,0,0,rc.right-rc.left,rc.bottom-rc.top,FALSE);
	TmPanel.hdc = GetDC(TmPanel.hwnd);
	SAFE_SET_NOT_NULL(TmPanel.hbitmap,
  	CreateCompatibleBitmap(TmPanel.hdc,TM_PANEL_XMAX,TM_PANEL_YMAX));
	SAFE_SET_NOT_NULL(TmPanel.hmdc,CreateCompatibleDC(TmPanel.hdc));
	TmPanel.hgdiobj_hmdcprev = SelectObject(TmPanel.hmdc,TmPanel.hbitmap);
	ReleaseDC(TmPanel.hwnd,TmPanel.hdc);
	GetClientRect(TmPanel.hwnd,&rc);
	TmPanel.rcMe.left = rc.left;
	TmPanel.rcMe.top = rc.top;
	TmPanel.rcMe.right = rc.right;
	TmPanel.rcMe.bottom = rc.bottom;
	TmPanel.rcTitle.left = TmPanel.rcMe.left+TMP_SPACE;
	TmPanel.rcTitle.top = TmPanel.rcMe.top+TMP_SPACE;
	TmPanel.rcTitle.right = TmPanel.rcMe.right-TMP_SPACE;
	TmPanel.rcTitle.bottom = TmPanel.rcTitle.top+TMP_TITLE_CHAR_SIZE+TMP_SPACE;
	TmPanel.rcFile.left = TmPanel.rcMe.left+TMP_SPACE;
	TmPanel.rcFile.top = TmPanel.rcTitle.bottom+TMP_SPACE;
	TmPanel.rcFile.right = TmPanel.rcMe.right-TMP_SPACE;
	TmPanel.rcFile.bottom = TmPanel.rcFile.top+TMP_FILE_CHAR_SIZE+TMP_SPACE;
	top = TmPanel.rcFile.bottom+TMP_SPACE;
  bottom = top + TMP_3L_CSIZE;
	i = (TMP_3L_CSIZE - TMP_3L_CSSIZE)/2;
	TmPanel.rcTimeH.left = TmPanel.rcMe.left + TMP_SPACE;
	TmPanel.rcTimeH.top = top + i;
	TmPanel.rcTimeH.right = TmPanel.rcTimeH.left + TMP_3L_CSSIZE*4;
	TmPanel.rcTimeH.bottom = bottom - i;
	TmPanel.rcTime.left = TmPanel.rcTimeH.right + TMP_3L_SPACE;
	TmPanel.rcTime.top = top;
	TmPanel.rcTime.right = TmPanel.rcTime.left + TMP_3L_CSIZE*12;
	TmPanel.rcTime.bottom = bottom;
	TmPanel.rcVoicesH.left = TmPanel.rcTime.right + TMP_3L_SPACE;
	TmPanel.rcVoicesH.top = top + i;
	TmPanel.rcVoicesH.right = TmPanel.rcVoicesH.left + TMP_3L_CSSIZE*6;
	TmPanel.rcVoicesH.bottom = bottom - i;
	TmPanel.rcVoices.left = TmPanel.rcVoicesH.right + TMP_3L_SPACE;
	TmPanel.rcVoices.top = top;
	TmPanel.rcVoices.right = TmPanel.rcVoices.left + TMP_3L_CSIZE*7;
	TmPanel.rcVoices.bottom = bottom;
	TmPanel.rcMasterVolumeH.left = TmPanel.rcVoices.right + TMP_3L_SPACE;
	TmPanel.rcMasterVolumeH.top = top + i;
	TmPanel.rcMasterVolumeH.right = TmPanel.rcMasterVolumeH.left + TMP_3L_CSSIZE*4;
	TmPanel.rcMasterVolumeH.bottom = bottom - i;
	TmPanel.rcMasterVolume.left = TmPanel.rcMasterVolumeH.right + TMP_3L_SPACE;
	TmPanel.rcMasterVolume.top = top;
	TmPanel.rcMasterVolume.right = TmPanel.rcMasterVolume.left + TMP_3L_CSIZE*4;
	TmPanel.rcMasterVolume.bottom = bottom;
	top = TmPanel.rcTime.bottom + TMP_SPACE;
  bottom = top + TMP_4L_CSIZE;
	i = (TMP_4L_CSIZE - TMP_4L_CSSIZE)/2;
	TmPanel.rcListH.left = TmPanel.rcMe.left + TMP_SPACE;
	TmPanel.rcListH.top = top + i;
	TmPanel.rcListH.right = TmPanel.rcListH.left + TMP_4L_CSSIZE*4;
	TmPanel.rcListH.bottom = bottom - i;
	TmPanel.rcList.left = TmPanel.rcListH.right + TMP_4L_SPACE;
	TmPanel.rcList.top = top;
	TmPanel.rcList.right = TmPanel.rcList.left + TMP_4L_CSIZE*9;
	TmPanel.rcList.bottom = bottom;
	TmPanelStart = 1;
	TmPanelClear();
	TmPanelFullReset();
	TmPanelAllUpdate = 1;
	TmPanelUpdate();
}

static void TmPanelRect(RECT rc, COLORREF color)
{
	HPEN hPen;
  HBRUSH hBrush;
	HGDIOBJ hgdiobj_hpen, hgdiobj_hbrush;
	SAFE_SET_NOT_NULL(hPen,CreatePen(PS_SOLID,1,color));
	SAFE_SET_NOT_NULL(hBrush,CreateSolidBrush(color));
	hgdiobj_hpen = SelectObject(TmPanel.hmdc, hPen);
	hgdiobj_hbrush = SelectObject(TmPanel.hmdc, hBrush);
	Rectangle(TmPanel.hmdc,rc.left,rc.top,rc.right,rc.bottom);
	SelectObject(TmPanel.hmdc, hgdiobj_hpen);
  DeleteObject(hPen);
  SelectObject(TmPanel.hmdc, hgdiobj_hbrush);
  DeleteObject(hBrush);
	InvalidateRect(TmPanel.hwnd,&rc, FALSE);
}

static void TmPanelClear(void)
{
	if(!TmPanelStart)
  	return;
	TmPanelRect(TmPanel.rcMe,TmCc(TMCC_BACK));
}

void TmPanelPartReset(void)
{
	if(!TmPanelStart)
  	return;
  TmPanel.TimeDone = TMP_DONOT;
  TmPanel.VoicesDone = TMP_DONOT;
	TmPanel.MasterVolumeDone = TMP_DONOT;
  TmPanel.ListDone = TMP_DONOT;
	TmPanel.cur_time_h = -1;
	TmPanel.cur_time_m = -1;
	TmPanel.cur_time_s = -1;
	TmPanel.cur_time_ss = -1;
	TmPanel.total_time_h = -1;
	TmPanel.total_time_m = -1;
	TmPanel.total_time_s = -1;
	TmPanel.total_time_ss = -1;
	TmPanel.cur_voices = -1;
//	TmPanel.voices = -1;
	TmPanel.cur_pl_num = -1;
  TmPanel.playlist_num = -1;
}

void TmPanelReset(void)
{
	if(!TmPanelStart)
  	return;
	TmPanelClear();
	TmPanel.TitleDone = TMP_DONOT;
  TmPanel.FileDone = TMP_DONOT;
  TmPanel.TimeDone = TMP_DONOT;
  TmPanel.VoicesDone = TMP_DONOT;
	TmPanel.MasterVolumeDone = TMP_DONOT;
  TmPanel.ListDone = TMP_DONOT;
	TmPanel.TitleHDone = TMP_DONOT;
  TmPanel.FileHDone = TMP_DONOT;
  TmPanel.TimeHDone = TMP_DONOT;
  TmPanel.VoicesHDone = TMP_DONOT;
	TmPanel.MasterVolumeHDone = TMP_DONOT;
  TmPanel.ListHDone = TMP_DONOT;
	TmPanel.Title[0] = '\0';
	TmPanel.File[0] = '\0';
	TmPanelRect(TmPanel.rcTitle,TmCc(TMCC_FORE_HALF));
	TmPanelRect(TmPanel.rcFile,TmCc(TMCC_FORE_HALF));
	TmPanel.cur_time_h = -1;
	TmPanel.cur_time_m = -1;
	TmPanel.cur_time_s = -1;
	TmPanel.cur_time_ss = -1;
	TmPanel.total_time_h = -1;
	TmPanel.total_time_m = -1;
	TmPanel.total_time_s = -1;
	TmPanel.total_time_ss = -1;
	TmPanel.cur_voices = -1;
	TmPanel.cur_pl_num = -1;
  TmPanel.playlist_num = -1;
	TmPanel.voices = -1;
  TmPanel.master_volume = -1;
	TmPanelAllUpdate = 1;
}

void TmPanelFullReset(void)
{
	if(!TmPanelStart)
  	return;
	TmPanelClear();
	TmPanel.TitleDone = TMP_DONOT;
  TmPanel.FileDone = TMP_DONOT;
  TmPanel.TimeDone = TMP_DONOT;
  TmPanel.VoicesDone = TMP_DONOT;
	TmPanel.MasterVolumeDone = TMP_DONOT;
  TmPanel.ListDone = TMP_DONOT;
	TmPanel.TitleHDone = TMP_DONOT;
  TmPanel.FileHDone = TMP_DONOT;
  TmPanel.TimeHDone = TMP_DONOT;
  TmPanel.VoicesHDone = TMP_DONOT;
	TmPanel.MasterVolumeHDone = TMP_DONOT;
  TmPanel.ListHDone = TMP_DONOT;
	TmPanel.Title[0] = '\0';
	TmPanel.File[0] = '\0';
	TmPanelRect(TmPanel.rcTitle,TmCc(TMCC_FORE_HALF));
	TmPanelRect(TmPanel.rcFile,TmCc(TMCC_FORE_HALF));
	TmPanel.cur_time_h = -1;
	TmPanel.cur_time_m = -1;
	TmPanel.cur_time_s = -1;
	TmPanel.cur_time_ss = -1;
	TmPanel.total_time_h = -1;
	TmPanel.total_time_m = -1;
	TmPanel.total_time_s = -1;
	TmPanel.total_time_ss = -1;
	TmPanel.cur_voices = -1;
	TmPanel.cur_pl_num = -1;
  TmPanel.playlist_num = -1;
	TmPanel.voices = -1;
  TmPanel.master_volume = -1;
// Font Setting
	if(hTitleFont!=NULL)
		DeleteObject(hTitleFont);
	SAFE_SET_NOT_NULL(
  	hTitleFont,
		CreateFont(TMP_TITLE_CHAR_SIZE,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,
			DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
      DEFAULT_PITCH | FF_DONTCARE,PlayerFont)
  );
	if(hFileFont!=NULL)
		DeleteObject(hFileFont);
	SAFE_SET_NOT_NULL(
		hFileFont,
		CreateFont(TMP_FILE_CHAR_SIZE,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,
			DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
      DEFAULT_PITCH | FF_DONTCARE,PlayerFont)
  );
	if(hMiscHFont!=NULL)
		DeleteObject(hMiscHFont);
	SAFE_SET_NOT_NULL(
		hMiscHFont,
		CreateFont(TMP_MISCH_CHAR_SIZE,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,
     	DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
     	DEFAULT_PITCH | FF_DONTCARE,PlayerFont)
  );
	if(hMiscFont!=NULL)
		DeleteObject(hMiscFont);
	SAFE_SET_NOT_NULL(
		hMiscFont,
		CreateFont(TMP_MISC_CHAR_SIZE,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,
     	DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
     	DEFAULT_PITCH | FF_DONTCARE,PlayerFont)
  );
	TmPanelAllUpdate = 1;
}

#define RANGE(x,min,max) { if((x)<min) (x) = min; else if((x)>max) (x) = max; }

void TmPanelUpdate(void)
{
	PAINTSTRUCT ps;

	if(!TmPanelStart)
  	return;

// Panel : 1st line
	if(TmPanel.TitleDone==TMP_PREPARE){
		HGDIOBJ hgdiobj;
		HFONT hfont = hTitleFont;
		SAFE_SET_NOT2_TYPE(hgdiobj,SelectObject(TmPanel.hmdc, hfont),NULL,(HGDIOBJ)GDI_ERROR,"SelectObject");
		SAFE_NOT_TYPE(SetTextColor(TmPanel.hmdc,TmCc(TMCC_FORE)),(COLORREF)CLR_INVALID,"SetTextColor");
		SAFE_NOT_TYPE(SetBkColor(TmPanel.hmdc, TmCc(TMCC_FORE_HALF)),(COLORREF)CLR_INVALID,"SetBkColor");
		SAFE_NOT_TYPE(SetTextAlign(TmPanel.hmdc, TA_LEFT | TA_TOP | TA_NOUPDATECP),(UINT)GDI_ERROR,"SetTextAlign");
		ExtTextOut(TmPanel.hmdc,TmPanel.rcTitle.left,TmPanel.rcTitle.top,
    	ETO_CLIPPED,&(TmPanel.rcTitle),
    	TmPanel.Title,strlen(TmPanel.Title),NULL);
		SelectObject(TmPanel.hmdc, hgdiobj);
    InvalidateRect(hPanelWnd, &(TmPanel.rcTitle), FALSE);
		TmPanel.TitleDone=TMP_DONE;
  }

// Panel : 2nd line
	if(TmPanel.FileDone==TMP_PREPARE){
		HGDIOBJ hgdiobj;
		HFONT hfont = hFileFont;
		SAFE_SET_NOT2_TYPE(hgdiobj,SelectObject(TmPanel.hmdc, hfont),NULL,(HGDIOBJ)GDI_ERROR,"SelectObject");
		SAFE_NOT_TYPE(SetTextColor(TmPanel.hmdc,TmCc(TMCC_FORE)),(COLORREF)CLR_INVALID,"SetTextColor");
		SAFE_NOT_TYPE(SetBkColor(TmPanel.hmdc, TmCc(TMCC_FORE_HALF)),(COLORREF)CLR_INVALID,"SetBkColor");
		SAFE_NOT_TYPE(SetTextAlign(TmPanel.hmdc, TA_LEFT | TA_TOP | TA_NOUPDATECP),(UINT)GDI_ERROR,"SetTextAlign");
		ExtTextOut(TmPanel.hmdc,TmPanel.rcFile.left,TmPanel.rcFile.top,
    	ETO_CLIPPED,&(TmPanel.rcFile),
    	TmPanel.File,strlen(TmPanel.File),NULL);
		SelectObject(TmPanel.hmdc, hgdiobj);
    InvalidateRect(hPanelWnd, &(TmPanel.rcFile), FALSE);
		TmPanel.FileDone=TMP_DONE;
  }

// Panel : 3rd line
	if(TmPanel.TimeHDone==TMP_PREPARE){
		char buffer[] = "TIME";
		SIZE size;
		HGDIOBJ hgdiobj;
		HFONT hfont = hMiscHFont;
		SAFE_SET_NOT2_TYPE(hgdiobj,SelectObject(TmPanel.hmdc, hfont),NULL,(HGDIOBJ)GDI_ERROR,"SelectObject");
		SAFE_NOT_TYPE(SetTextColor(TmPanel.hmdc,TmCc(TMCC_FORE)),(COLORREF)CLR_INVALID,"SetTextColor");
		SAFE_NOT_TYPE(SetBkColor(TmPanel.hmdc, TmCc(TMCC_BACK)),(COLORREF)CLR_INVALID,"SetBkColor");
		SAFE_NOT_TYPE(SetTextAlign(TmPanel.hmdc, TA_LEFT | TA_TOP | TA_NOUPDATECP),(UINT)GDI_ERROR,"SetTextAlign");
		GetTextExtentPoint32(TmPanel.hmdc,buffer,strlen(buffer),&size);
		TmPanel.rcTimeH.right = TmPanel.rcTimeH.left + size.cx;
		ExtTextOut(TmPanel.hmdc,TmPanel.rcTimeH.left,TmPanel.rcTimeH.top,
    	ETO_CLIPPED,&(TmPanel.rcTimeH),
    	buffer,strlen(buffer),NULL);
		SelectObject(TmPanel.hmdc, hgdiobj);
    InvalidateRect(hPanelWnd, &(TmPanel.rcTimeH), FALSE);
		TmPanel.TimeHDone=TMP_DONE;
  }
	if(TmPanel.TimeDone==TMP_PREPARE){
		char buffer[32];
		SIZE size;
		HGDIOBJ hgdiobj;
		HFONT hfont = hMiscFont;
#if 1
    sprintf(buffer,"%02d:%02d:%02d/%02d:%02d:%02d",
			TmPanel.cur_time_h,TmPanel.cur_time_m,TmPanel.cur_time_s,
    	TmPanel.total_time_h,TmPanel.total_time_m,TmPanel.total_time_s);
#else
    sprintf(buffer,"%02.2d:%02.2d:%02.2d.%02.2d/%02.2d:%02.2d:%02.2d.%02.2d",
			TmPanel.cur_time_h,TmPanel.cur_time_m,
    	TmPanel.cur_time_s,TmPanel.cur_time_ss,
    	TmPanel.total_time_h,TmPanel.total_time_m,
    	TmPanel.total_time_s,TmPanel.total_time_ss);
#endif
		SAFE_SET_NOT2_TYPE(hgdiobj,SelectObject(TmPanel.hmdc, hfont),NULL,(HGDIOBJ)GDI_ERROR,"SelectObject");
		SAFE_NOT_TYPE(SetTextColor(TmPanel.hmdc,TmCc(TMCC_FORE)),(COLORREF)CLR_INVALID,"SetTextColor");
		SAFE_NOT_TYPE(SetBkColor(TmPanel.hmdc, TmCc(TMCC_FORE_HALF)),(COLORREF)CLR_INVALID,"SetBkColor");
		SAFE_NOT_TYPE(SetTextAlign(TmPanel.hmdc, TA_LEFT | TA_TOP | TA_NOUPDATECP),(UINT)GDI_ERROR,"SetTextAlign");
		GetTextExtentPoint32(TmPanel.hmdc,buffer,strlen(buffer),&size);
		TmPanel.rcTime.left = TmPanel.rcTimeH.right + TMP_3L_SPACE;
    TmPanel.rcTime.right = TmPanel.rcTime.left + size.cx;
		ExtTextOut(TmPanel.hmdc,TmPanel.rcTime.left,TmPanel.rcTime.top,
    	ETO_CLIPPED,&(TmPanel.rcTime),
    	buffer,strlen(buffer),NULL);
		SelectObject(TmPanel.hmdc, hgdiobj);
    InvalidateRect(hPanelWnd, &(TmPanel.rcTime), FALSE);
		TmPanel.TimeDone=TMP_DONOT;
  }

	if(TmPanel.VoicesHDone==TMP_PREPARE){
		char buffer[] = "VOICES";
		SIZE size;
		HGDIOBJ hgdiobj;
		HFONT hfont = hMiscHFont;
		SAFE_SET_NOT2_TYPE(hgdiobj,SelectObject(TmPanel.hmdc, hfont),NULL,(HGDIOBJ)GDI_ERROR,"SelectObject");
		SAFE_NOT_TYPE(SetTextColor(TmPanel.hmdc,TmCc(TMCC_FORE)),(COLORREF)CLR_INVALID,"SetTextColor");
		SAFE_NOT_TYPE(SetBkColor(TmPanel.hmdc, TmCc(TMCC_BACK)),(COLORREF)CLR_INVALID,"SetBkColor");
		SAFE_NOT_TYPE(SetTextAlign(TmPanel.hmdc, TA_LEFT | TA_TOP | TA_NOUPDATECP),(UINT)GDI_ERROR,"SetTextAlign");
		GetTextExtentPoint32(TmPanel.hmdc,buffer,strlen(buffer),&size);
		TmPanel.rcVoicesH.left = TmPanel.rcTime.right + TMP_3L_SPACE;
		TmPanel.rcVoicesH.right = TmPanel.rcVoicesH.left + size.cx;
		ExtTextOut(TmPanel.hmdc,TmPanel.rcVoicesH.left,TmPanel.rcVoicesH.top,
    	ETO_CLIPPED,&(TmPanel.rcVoicesH),
    	buffer,strlen(buffer),NULL);
		SelectObject(TmPanel.hmdc, hgdiobj);
    InvalidateRect(hPanelWnd, &(TmPanel.rcVoicesH), FALSE);
		TmPanel.VoicesHDone=TMP_DONE;
  }
	if(TmPanel.VoicesDone==TMP_PREPARE){
		char buffer[32];
		SIZE size;
		HGDIOBJ hgdiobj;
		HFONT hfont = hMiscFont;
// FIXME: Why does TmPanel.voices need?
//    sprintf(buffer,"%03d/%03d", TmPanel.cur_voices, TmPanel.voices);
    sprintf(buffer,"%03d/%03d", TmPanel.cur_voices, voices);
		SAFE_SET_NOT2_TYPE(hgdiobj,SelectObject(TmPanel.hmdc, hfont),NULL,(HGDIOBJ)GDI_ERROR,"SelectObject");
		SAFE_NOT_TYPE(SetTextColor(TmPanel.hmdc,TmCc(TMCC_FORE)),(COLORREF)CLR_INVALID,"SetTextColor");
		SAFE_NOT_TYPE(SetBkColor(TmPanel.hmdc, TmCc(TMCC_FORE_HALF)),(COLORREF)CLR_INVALID,"SetBkColor");
		SAFE_NOT_TYPE(SetTextAlign(TmPanel.hmdc, TA_LEFT | TA_TOP | TA_NOUPDATECP),(UINT)GDI_ERROR,"SetTextAlign");
		GetTextExtentPoint32(TmPanel.hmdc,buffer,strlen(buffer),&size);
		TmPanel.rcVoices.left = TmPanel.rcVoicesH.right + TMP_3L_SPACE;
		TmPanel.rcVoices.right = TmPanel.rcVoices.left + size.cx;
		ExtTextOut(TmPanel.hmdc,TmPanel.rcVoices.left,TmPanel.rcVoices.top,
    	ETO_CLIPPED,&(TmPanel.rcVoices),
    	buffer,strlen(buffer),NULL);
		SelectObject(TmPanel.hmdc, hgdiobj);
    InvalidateRect(hPanelWnd, &(TmPanel.rcVoices), FALSE);
		TmPanel.VoicesDone=TMP_DONOT;
  }

	if(TmPanel.MasterVolumeHDone==TMP_PREPARE){
		char buffer[] = "M.Vol";
		SIZE size;
		HGDIOBJ hgdiobj;
    HFONT hfont = hMiscHFont;
		SAFE_SET_NOT2_TYPE(hgdiobj,SelectObject(TmPanel.hmdc, hfont),NULL,(HGDIOBJ)GDI_ERROR,"SelectObject");
		SAFE_NOT_TYPE(SetTextColor(TmPanel.hmdc,TmCc(TMCC_FORE)),(COLORREF)CLR_INVALID,"SetTextColor");
		SAFE_NOT_TYPE(SetBkColor(TmPanel.hmdc, TmCc(TMCC_BACK)),(COLORREF)CLR_INVALID,"SetBkColor");
		SAFE_NOT_TYPE(SetTextAlign(TmPanel.hmdc, TA_LEFT | TA_TOP | TA_NOUPDATECP),(UINT)GDI_ERROR,"SetTextAlign");
		GetTextExtentPoint32(TmPanel.hmdc,buffer,strlen(buffer),&size);
		TmPanel.rcMasterVolumeH.left = TmPanel.rcVoices.right + TMP_3L_SPACE;
		TmPanel.rcMasterVolumeH.right = TmPanel.rcMasterVolumeH.left + size.cx;
		ExtTextOut(TmPanel.hmdc,TmPanel.rcMasterVolumeH.left,TmPanel.rcMasterVolumeH.top,
    	ETO_CLIPPED,&(TmPanel.rcMasterVolumeH),
    	buffer,strlen(buffer),NULL);
		SelectObject(TmPanel.hmdc, hgdiobj);
    InvalidateRect(hPanelWnd, &(TmPanel.rcMasterVolumeH), FALSE);
		TmPanel.MasterVolumeHDone=TMP_DONE;
  }
	if(TmPanel.MasterVolumeDone==TMP_PREPARE){
		char buffer[32];
		SIZE size;
		HGDIOBJ hgdiobj;
		HFONT hfont = hMiscFont;
    sprintf(buffer,"%03d%%", TmPanel.master_volume);
		SAFE_SET_NOT2_TYPE(hgdiobj,SelectObject(TmPanel.hmdc, hfont),NULL,(HGDIOBJ)GDI_ERROR,"SelectObject");
		SAFE_NOT_TYPE(SetTextColor(TmPanel.hmdc,TmCc(TMCC_FORE)),(COLORREF)CLR_INVALID,"SetTextColor");
		SAFE_NOT_TYPE(SetBkColor(TmPanel.hmdc, TmCc(TMCC_FORE_HALF)),(COLORREF)CLR_INVALID,"SetBkColor");
		SAFE_NOT_TYPE(SetTextAlign(TmPanel.hmdc, TA_LEFT | TA_TOP | TA_NOUPDATECP),(UINT)GDI_ERROR,"SetTextAlign");
		GetTextExtentPoint32(TmPanel.hmdc,buffer,strlen(buffer),&size);
		TmPanel.rcMasterVolume.left = TmPanel.rcMasterVolumeH.right + TMP_3L_SPACE;
		TmPanel.rcMasterVolume.right = TmPanel.rcMasterVolume.left + size.cx;
		ExtTextOut(TmPanel.hmdc,TmPanel.rcMasterVolume.left,TmPanel.rcMasterVolume.top,
    	ETO_CLIPPED,&(TmPanel.rcMasterVolume),
    	buffer,strlen(buffer),NULL);
		SelectObject(TmPanel.hmdc, hgdiobj);
    InvalidateRect(hPanelWnd, &(TmPanel.rcMasterVolume), FALSE);
		TmPanel.MasterVolumeDone=TMP_DONOT;
  }


// Panel : 4th line
	if(TmPanel.ListHDone==TMP_PREPARE){
		char buffer[] = "LIST";
		SIZE size;
		HGDIOBJ hgdiobj;
    HFONT hfont = hMiscHFont;
		SAFE_SET_NOT2_TYPE(hgdiobj,SelectObject(TmPanel.hmdc, hfont),NULL,(HGDIOBJ)GDI_ERROR,"SelectObject");
		SAFE_NOT_TYPE(SetTextColor(TmPanel.hmdc,TmCc(TMCC_FORE)),(COLORREF)CLR_INVALID,"SetTextColor");
		SAFE_NOT_TYPE(SetBkColor(TmPanel.hmdc, TmCc(TMCC_BACK)),(COLORREF)CLR_INVALID,"SetBkColor");
		SAFE_NOT_TYPE(SetTextAlign(TmPanel.hmdc, TA_LEFT | TA_TOP | TA_NOUPDATECP),(UINT)GDI_ERROR,"SetTextAlign");
		GetTextExtentPoint32(TmPanel.hmdc,buffer,strlen(buffer),&size);
		TmPanel.rcListH.left = TmPanel.rcMe.left + TMP_SPACE;
		TmPanel.rcListH.right = TmPanel.rcListH.left + size.cx;
		ExtTextOut(TmPanel.hmdc,TmPanel.rcListH.left,TmPanel.rcListH.top,
    	ETO_CLIPPED,&(TmPanel.rcListH),
    	buffer,strlen(buffer),NULL);
		SelectObject(TmPanel.hmdc, hgdiobj);
    InvalidateRect(hPanelWnd, &(TmPanel.rcListH), FALSE);
		TmPanel.ListHDone=TMP_DONE;
  }
	if(TmPanel.ListDone==TMP_PREPARE){
		char buffer[32];
		SIZE size;
		HGDIOBJ hgdiobj;
		HFONT hfont = hMiscFont;
    sprintf(buffer,"%04d/%04d", TmPanel.cur_pl_num, TmPanel.playlist_num);
		SAFE_SET_NOT2_TYPE(hgdiobj,SelectObject(TmPanel.hmdc, hfont),NULL,(HGDIOBJ)GDI_ERROR,"SelectObject");
		SAFE_NOT_TYPE(SetTextColor(TmPanel.hmdc,TmCc(TMCC_FORE)),(COLORREF)CLR_INVALID,"SetTextColor");
		SAFE_NOT_TYPE(SetBkColor(TmPanel.hmdc, TmCc(TMCC_FORE_HALF)),(COLORREF)CLR_INVALID,"SetBkColor");
		SAFE_NOT_TYPE(SetTextAlign(TmPanel.hmdc, TA_LEFT | TA_TOP | TA_NOUPDATECP),(UINT)GDI_ERROR,"SetTextAlign");
		GetTextExtentPoint32(TmPanel.hmdc,buffer,strlen(buffer),&size);
		TmPanel.rcList.left = TmPanel.rcListH.right + TMP_4L_SPACE;
		TmPanel.rcList.right = TmPanel.rcList.left + size.cx;
		ExtTextOut(TmPanel.hmdc,TmPanel.rcList.left,TmPanel.rcList.top,
    	ETO_CLIPPED,&(TmPanel.rcList),
    	buffer,strlen(buffer),NULL);
		SelectObject(TmPanel.hmdc, hgdiobj);
    InvalidateRect(hPanelWnd, &(TmPanel.rcList), FALSE);
		TmPanel.ListDone=TMP_DONOT;
  }

	TmPanel.hdc = BeginPaint(TmPanel.hwnd, &ps);
	BitBlt(TmPanel.hdc,
  	TmPanel.rcMe.left,TmPanel.rcMe.top,TmPanel.rcMe.right,TmPanel.rcMe.bottom,
    TmPanel.hmdc,0,0,SRCCOPY);

	if(TmPanelAllUpdate){
		InvalidateRect(hPanelWnd, NULL, FALSE);
		TmPanelAllUpdate = 0;
  }
	EndPaint(TmPanel.hwnd, &ps);
}

void TmPanelSet(void)
{
	if(!TmPanelStart)
  	return;
	if(Panel==NULL)
  	return;
	if(TmPanel.TitleDone==TMP_DONOT){
		if(cur_pl!=NULL){
  		strncpy((char *)TmPanel.Title,(char *)cur_pl->title,TMP_TITLE_MAX);
			TmPanel.Title[TMP_TITLE_MAX] = '\0';
			TmPanel.TitleDone = TMP_PREPARE;
    } else {
			TmPanel.Title[0] = '\0';
			TmPanel.TitleDone = TMP_PREPARE;
//			TmPanel.TitleDone = TMP_DONOT;
		}
  }
	if(TmPanel.FileDone==TMP_DONOT){
		if(cur_pl!=NULL){
  		strncpy((char *)TmPanel.File,(char *)cur_pl->filename,TMP_FILE_MAX);
			TmPanel.File[TMP_FILE_MAX] = '\0';
			TmPanel.FileDone = TMP_PREPARE;
    } else {
			TmPanel.File[0] = '\0';
			TmPanel.FileDone = TMP_PREPARE;
//			TmPanel.FileDone = TMP_DONOT;
		}
  }
	if(TmPanel.TimeHDone==TMP_DONOT)
		TmPanel.TimeHDone = TMP_PREPARE;
	if(TmPanel.TimeDone==TMP_DONOT){
		if(TmPanel.cur_time_s == Panel->cur_time_s
//    		&& TmPanel.cur_time_ss == Panel->cur_time_ss
				&& TmPanel.cur_time_m == Panel->cur_time_m
				&& TmPanel.cur_time_h == Panel->cur_time_h
				&& TmPanel.total_time_h == Panel->total_time_h
				&& TmPanel.total_time_m == Panel->total_time_m
				&& TmPanel.total_time_s == Panel->total_time_s
//				&& TmPanel.total_time_ss == Panel->total_time_ss
		)
    	TmPanel.TimeDone = TMP_DONOT;
    else {
			TmPanel.cur_time_h = Panel->cur_time_h;
			TmPanel.cur_time_m = Panel->cur_time_m;
			TmPanel.cur_time_s = Panel->cur_time_s;
			TmPanel.cur_time_ss = Panel->cur_time_ss;
			TmPanel.total_time_h = Panel->total_time_h;
			TmPanel.total_time_m = Panel->total_time_m;
			TmPanel.total_time_s = Panel->total_time_s;
			TmPanel.total_time_ss = Panel->total_time_ss;
			RANGE(TmPanel.cur_time_h,0,99);
			RANGE(TmPanel.total_time_h,0,99);
			TmPanel.TimeDone = TMP_PREPARE;
		}
  }
	if(TmPanel.VoicesHDone==TMP_DONOT)
		TmPanel.VoicesHDone = TMP_PREPARE;
	if(TmPanel.VoicesDone==TMP_DONOT){
		if(player_status != PLAYERSTATUS_PLAY){
			Panel->voices = st_current->voices;
    }
		if(TmPanel.cur_voices == Panel->cur_voices
    		&& TmPanel.voices == Panel->voices)
			TmPanel.VoicesDone = TMP_DONOT;
    else {
      TmPanel.cur_voices = Panel->cur_voices;
			TmPanel.voices = Panel->voices;
			TmPanel.VoicesDone = TMP_PREPARE;
		}
  }
	if(TmPanel.MasterVolumeHDone==TMP_DONOT)
		TmPanel.MasterVolumeHDone = TMP_PREPARE;
	if(TmPanel.MasterVolumeDone==TMP_DONOT){
		if(player_status != PLAYERSTATUS_PLAY){
			Panel->master_volume = st_current->amplification;
			MainWndScrollbarVolumeUpdate();
    }
		if(TmPanel.master_volume == Panel->master_volume)
			TmPanel.MasterVolumeDone = TMP_DONOT;
    else {
    	TmPanel.master_volume = Panel->master_volume;
			TmPanel.MasterVolumeDone = TMP_PREPARE;
		}
  }
	if(TmPanel.ListHDone==TMP_DONOT)
		TmPanel.ListHDone = TMP_PREPARE;
	if(TmPanel.ListDone==TMP_DONOT){
		if(TmPanel.cur_pl_num == cur_pl_num && TmPanel.playlist_num == playlist_num)
			TmPanel.ListDone = TMP_DONOT;
    else {
    	TmPanel.cur_pl_num = cur_pl_num;
      TmPanel.playlist_num = playlist_num;
			TmPanel.ListDone = TMP_PREPARE;
    }
	}
	TmPanelUpdate();
}











// ----------------------------------------------------------------------------
// Progress Scrollbar

int MainWndScrollbarProgressPos = 0;
int MainWndScrollbarProgressPosMin = 0;
int MainWndScrollbarProgressPosMax = 1000;
int MainWndScrollbarProgressNotUpdate = 0;
int MainWndScrollbarProgressApplyFlag = 0;

HWND hMainWndScrollbarProgressWnd = 0;

void MainWndScrollbarProgressReset(void)
{
	HWND hwnd = 0;
	if(hMainWndScrollbarProgressWnd==0){
		hwnd = GetDlgItem(hMainWnd,IDC_SCROLLBAR_PROGRESS);
		hMainWndScrollbarProgressWnd = hwnd;
		if(hwnd==0)
  		return;
  } else
  	hwnd = hMainWndScrollbarProgressWnd;

	EnableScrollBar(hwnd,SB_CTL,ESB_ENABLE_BOTH);
	SetScrollRange(hwnd,SB_CTL,
  	MainWndScrollbarProgressPosMin,MainWndScrollbarProgressPosMax,TRUE);
	MainWndScrollbarProgressPos = MainWndScrollbarProgressPosMin;
  SetScrollPos(hwnd,SB_CTL,MainWndScrollbarProgressPosMin,TRUE);
	MainWndScrollbarProgressNotUpdate = 0;
	MainWndScrollbarProgressApplyFlag = 0;
}

void MainWndScrollbarProgressInit(void)
{
	MainWndScrollbarProgressReset();
}

static int Time2Pos(int32 time)
{
	if(MainWndScrollbarProgressPosMax==MainWndScrollbarProgressPosMin)
  	return 0;
	if(Panel->total_time<=0)
  	return 0;
	return ((int32)(MainWndScrollbarProgressPosMax-MainWndScrollbarProgressPosMin))
  	* time / Panel->total_time;
}

static int32 Pos2Time(int pos)
{
	if(MainWndScrollbarProgressPosMax==MainWndScrollbarProgressPosMin)
  	return MainWndScrollbarProgressPosMin;
	return (Panel->total_time)*((int32)pos)
  	/(MainWndScrollbarProgressPosMax-MainWndScrollbarProgressPosMin);
}

void MainWndScrollbarProgressApply(int pos)
{
	int min,max;
	HWND hwnd = 0;
	int32 val;
	if(hMainWndScrollbarProgressWnd==0){
		hwnd = GetDlgItem(hMainWnd,IDC_SCROLLBAR_PROGRESS);
		hMainWndScrollbarProgressWnd = hwnd;
		if(hwnd==0)
  		return;
		MainWndScrollbarProgressReset();
  } else
  	hwnd = hMainWndScrollbarProgressWnd;

	if(!IsWindowEnabled(hwnd))
  	return;
	if(pos == GetScrollPos(hwnd,SB_CTL))
  	return;
	val = Pos2Time(pos);

	if(val/100 != Panel->cur_time/100){
  	val *= (st_current->output_rate)/100;
    PutCrbLoopBuffer(RC_JUMP,val);
		GetScrollRange(hwnd,SB_CTL,&min,&max);
  	RANGE(pos,min,max);
		MainWndScrollbarProgressPos = pos;
		SetScrollPos(hwnd,SB_CTL,pos,TRUE);
   	MainWndScrollbarProgressApplyFlag = 10;
		PanelPartReset();
  	return;
	}
}

void MainWndScrollbarProgressUpdate(void)
{
	int min,max,pos;
	HWND hwnd = 0;

	if(hMainWndScrollbarProgressWnd==0){
		hwnd = GetDlgItem(hMainWnd,IDC_SCROLLBAR_PROGRESS);
		hMainWndScrollbarProgressWnd = hwnd;
		if(hwnd==0)
  		return;
		MainWndScrollbarProgressReset();
  } else
  	hwnd = hMainWndScrollbarProgressWnd;

  if(player_status == PLAYERSTATUS_PLAY)
  	EnableWindow(hwnd,TRUE);
  else {
		MainWndScrollbarProgressReset();
  	EnableWindow(hwnd,FALSE);
		return;
  }
	if(!IsWindowEnabled(hwnd))
  	return;

	if(MainWndScrollbarProgressNotUpdate)
  	return;

	pos = Time2Pos(Panel->cur_time);
	if(MainWndScrollbarProgressPos == pos)
  	return;
  else {
		if(MainWndScrollbarProgressApplyFlag){
			MainWndScrollbarProgressApplyFlag--;
			if(MainWndScrollbarProgressApplyFlag<0)
     		MainWndScrollbarProgressApplyFlag = 0;
 	  	return;
		}
		GetScrollRange(hwnd,SB_CTL,&min,&max);
  	RANGE(pos,min,max);
		MainWndScrollbarProgressPos = pos;
		SetScrollPos(hwnd,SB_CTL,pos,TRUE);
	}
}










// ----------------------------------------------------------------------------
// Volume Scrollbar

int MainWndScrollbarVolumePos = 0;
int MainWndScrollbarVolumePosMin = 0;
int MainWndScrollbarVolumePosMax = 200;
int MainWndScrollbarVolumeNotUpdate = 0;
int MainWndScrollbarVolumeApplyFlag = 0;

HWND hMainWndScrollbarVolumeWnd = 0;

void MainWndScrollbarVolumeReset(void)
{
	HWND hwnd = 0;
	if(hMainWndScrollbarVolumeWnd==0){
		hwnd = GetDlgItem(hMainWnd,IDC_SCROLLBAR_VOLUME);
		hMainWndScrollbarVolumeWnd = hwnd;
		if(hwnd==0)
  		return;
  } else
  	hwnd = hMainWndScrollbarVolumeWnd;

	EnableScrollBar(hwnd,SB_CTL,ESB_ENABLE_BOTH);
	SetScrollRange(hwnd,SB_CTL,
  	MainWndScrollbarVolumePosMin,MainWndScrollbarVolumePosMax,TRUE);
	MainWndScrollbarVolumePos = MainWndScrollbarVolumePosMin;
  SetScrollPos(hwnd,SB_CTL,MainWndScrollbarVolumePosMin,TRUE);
	MainWndScrollbarVolumeNotUpdate = 0;
	MainWndScrollbarVolumeApplyFlag = 0;
}

void MainWndScrollbarVolumeInit(void)
{
	MainWndScrollbarVolumeReset();
}

static int Volume2Pos(int vol)
{
	int pos = MainWndScrollbarVolumePosMax-vol;
	RANGE(pos,MainWndScrollbarVolumePosMin,MainWndScrollbarVolumePosMax);
	return pos;
}


static int32 Pos2Volume(int pos)
{
	int vol = MainWndScrollbarVolumePosMax-pos;
	RANGE(vol,MainWndScrollbarVolumePosMin,MainWndScrollbarVolumePosMax);
	return vol;
}

void MainWndScrollbarVolumeApply(int pos)
{
	int min,max;
	HWND hwnd = 0;
	int32 val;
	if(hMainWndScrollbarVolumeWnd==0){
		hwnd = GetDlgItem(hMainWnd,IDC_SCROLLBAR_VOLUME);
		hMainWndScrollbarVolumeWnd = hwnd;
		if(hwnd==0)
  		return;
		MainWndScrollbarVolumeReset();
  } else
  	hwnd = hMainWndScrollbarVolumeWnd;

	if(!IsWindowEnabled(hwnd))
  	return;
	if(pos == GetScrollPos(hwnd,SB_CTL))
 		return;

	val = Pos2Volume(pos);
	if(player_status == PLAYERSTATUS_PLAY){
		if(val != Panel->master_volume){
			if(st_current->amplification != val)
				st_current->amplification = val;
			val -= Panel->master_volume;
    	PutCrbLoopBuffer(RC_CHANGE_VOLUME,val);
			GetScrollRange(hwnd,SB_CTL,&min,&max);
  		RANGE(pos,min,max);
			MainWndScrollbarVolumePos = pos;
			SetScrollPos(hwnd,SB_CTL,pos,TRUE);
   		MainWndScrollbarVolumeApplyFlag = 3;
  		return;
		}
	} else {
		if(st_current->amplification != val){
			st_current->amplification = val;
    	ApplySettingTimidity(st_current);
  	}
		GetScrollRange(hwnd,SB_CTL,&min,&max);
 		RANGE(pos,min,max);
		MainWndScrollbarVolumePos = pos;
		SetScrollPos(hwnd,SB_CTL,pos,TRUE);
		TmPanelSet();
		return;
	}
}

void MainWndScrollbarVolumeUpdate(void)
{
	int min,max,pos;
	HWND hwnd = 0;
	if(hMainWndScrollbarVolumeWnd==0){
		hwnd = GetDlgItem(hMainWnd,IDC_SCROLLBAR_VOLUME);
		hMainWndScrollbarVolumeWnd = hwnd;
		if(hwnd==0)
  		return;
		MainWndScrollbarVolumeReset();
  } else
  	hwnd = hMainWndScrollbarVolumeWnd;

	if(!IsWindowEnabled(hwnd))
  	return;

	if(player_status != PLAYERSTATUS_PLAY){
		pos = Volume2Pos(st_current->amplification);
		if(pos != GetScrollPos(hwnd,SB_CTL)){
			GetScrollRange(hwnd,SB_CTL,&min,&max);
  		RANGE(pos,min,max);
			MainWndScrollbarVolumePos = pos;
			SetScrollPos(hwnd,SB_CTL,pos,TRUE);
		}
		return;
 	}

	if(MainWndScrollbarVolumeNotUpdate)
  	return;

	pos = Volume2Pos(Panel->master_volume);
	if(MainWndScrollbarVolumePos == pos)
  	return;
  else {
		if(MainWndScrollbarVolumeApplyFlag){
			MainWndScrollbarVolumeApplyFlag--;
			if(MainWndScrollbarVolumeApplyFlag<0)
     		MainWndScrollbarVolumeApplyFlag = 0;
 	  	return;
		}
		GetScrollRange(hwnd,SB_CTL,&min,&max);
  	RANGE(pos,min,max);
		MainWndScrollbarVolumePos = pos;
		SetScrollPos(hwnd,SB_CTL,pos,TRUE);
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
  sprintf(VersionText,
"TiMidity-0.2i by Tuukka Toivonen <tt@cgs.fi>." NLS
"TiMidity Win32 version by Davide Moretti <dmoretti@iper.net>." NLS
"TiMidity Windows 95 port by Nicolas Witczak." NLS
"TiMidity Win32 GUI by Daisuke Aoki <dai@y7.net>." NLS
"Win32GUI alpha version (Z1.W32G.0.02)" NLS
"TiMidity++ by Masanao Izumo <mo@goice.co.jp>." NLS
	);
	MessageBox(hParentWnd, VersionText, "Version", MB_OK);
}

static void TiMidityWnd(HWND hParentWnd)
{
	char TiMidityText[2024];
  sprintf(TiMidityText,
" TiMidity++ version %s -- Experimental MIDI to WAVE converter" NLS
" Copyright (C) 1999 Masanao Izumo <mo@goice.co.jp>" NLS
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
" Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA." NLS
,
timidity_version
	);
	MessageBox(hParentWnd, TiMidityText, "TiMidity", MB_OK);
}


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
  RECT rc;
	switch (uMess){
		case WM_INITDIALOG:
			InitDebugEditWnd(hwnd);
			if(DebugWndFlag)
				CheckDlgButton(hwnd, IDC_CHECKBOX_DEBUG_WND_VALID, 1);
      else
				CheckDlgButton(hwnd, IDC_CHECKBOX_DEBUG_WND_VALID, 0);
			return FALSE;
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
	      default:
        	return FALSE;
      }
		case WM_SIZE:
      GetClientRect(hDebugWnd, &rc);
      MoveWindow(hDebugEditWnd, rc.left, rc.top,rc.right, rc.bottom - 30,TRUE);
	    return FALSE;
		case WM_CLOSE:
					ShowWindow(hDebugWnd, SW_HIDE);
          break;
    default:
      	return FALSE;
	}
	return FALSE;
}

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
  SendMessage(hDebugEditWnd, EM_SETLIMITTEXT, (WPARAM)1024*64, 0);
//  SendMessage(hDebugEditWnd, WM_PAINT, 0, 0);
	GetClientRect(hParentWnd, &rc);
  MoveWindow(hDebugEditWnd,rc.left,rc.top,rc.right,rc.bottom-30,TRUE);
 	ClearDebugWnd();
	ShowWindow(hDebugEditWnd,SW_SHOW);
	UpdateWindow(hDebugEditWnd);
}

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





// ***************************************************************************
// Console Window

BOOL CALLBACK ConsoleWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);

void InitConsoleWnd(HWND hParentWnd)
{
	hConsoleWnd = CreateDialog
  	(hInst,MAKEINTRESOURCE(IDD_DIALOG_CONSOLE),hParentWnd,ConsoleWndProc);
	ShowWindow(hConsoleWnd,SW_HIDE);
	UpdateWindow(hConsoleWnd);
}

int ConsoleWndMaxSize = 64 * 1024;

void ConsoleWndAllUpdate(void);
void ConsoleWndVerbosityUpdate(void);
void ConsoleWndVerbosityApply(void);
void ConsoleWndValidUpdate(void);
void ConsoleWndValidApply(void);
void ConsoleWndVerbosityApplyIncDec(int num);

BOOL CALLBACK
ConsoleWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess){
		case WM_INITDIALOG:
			PutsConsoleWnd("Console Window\n");
			ConsoleWndAllUpdate();
			return FALSE;
    case WM_COMMAND:
    	switch (LOWORD(wParam)) {
      	case IDCLOSE:
				ShowWindow(hwnd, SW_HIDE);
				MainWndUpdateConsoleButton();
          break;
        case IDCLEAR:
					ClearConsoleWnd();
          break;
				case IDC_CHECKBOX_VALID:
					ConsoleWndValidApply();
          break;
				case IDC_BUTTON_VERBOSITY:
					ConsoleWndVerbosityApply();
					break;
				case IDC_BUTTON_INC:
					ConsoleWndVerbosityApplyIncDec(1);
					break;
				case IDC_BUTTON_DEC:
					ConsoleWndVerbosityApplyIncDec(-1);
					break;
	      default:
        	return FALSE;
      }
		case WM_SIZE:
				ConsoleWndAllUpdate();
		    return FALSE;
		case WM_CLOSE:
			ShowWindow(hConsoleWnd, SW_HIDE);
			MainWndUpdateConsoleButton();
			break;
		case WM_SETFOCUS:
    	HideCaret(hwnd);
      break;
		case WM_KILLFOCUS:
    	ShowCaret(hwnd);
      break;
    default:
      return FALSE;
	}
	return FALSE;
}

void PutsConsoleWnd(char *str)
{
	HWND hwnd;
	if(!IsWindow(hConsoleWnd) || !ConsoleWndFlag)
		return;
	hwnd = GetDlgItem(hConsoleWnd,IDC_EDIT);
	PutsEditCtlWnd(hwnd,str);
}

void PrintfConsoleWnd(char *fmt, ...)
{
	HWND hwnd;
  va_list ap;
	if(!IsWindow(hConsoleWnd) || !ConsoleWndFlag)
		return;
	hwnd = GetDlgItem(hConsoleWnd,IDC_EDIT);
  va_start(ap, fmt);
  VprintfEditCtlWnd(hwnd,fmt,ap);
  va_end(ap);
}

void ClearConsoleWnd(void)
{
	HWND hwnd;
	if(!IsWindow(hConsoleWnd))
		return;
	hwnd = GetDlgItem(hConsoleWnd,IDC_EDIT);
	ClearEditCtlWnd(hwnd);
}

void ConsoleWndAllUpdate(void)
{
	ConsoleWndVerbosityUpdate();
	ConsoleWndValidUpdate();
	Edit_LimitText(GetDlgItem(hConsoleWnd,IDC_EDIT_VERBOSITY),3);
	Edit_LimitText(GetDlgItem(hConsoleWnd,IDC_EDIT),ConsoleWndMaxSize);
}

void ConsoleWndValidUpdate(void)
{
	if(ConsoleWndFlag)
		CheckDlgButton(hConsoleWnd, IDC_CHECKBOX_VALID, 1);
  else
  	CheckDlgButton(hConsoleWnd, IDC_CHECKBOX_VALID, 0);
}

void ConsoleWndValidApply(void)
{
	if(IsDlgButtonChecked(hConsoleWnd,IDC_CHECKBOX_VALID))
		ConsoleWndFlag = 1;
	else
		ConsoleWndFlag = 0;
}

void ConsoleWndVerbosityUpdate(void)
{
	SetDlgItemInt(hConsoleWnd,IDC_EDIT_VERBOSITY,(UINT)ctl->verbosity, TRUE);
}

void ConsoleWndVerbosityApply(void)
{
	char buffer[64];
	HWND hwnd;
	hwnd = GetDlgItem(hConsoleWnd,IDC_EDIT_VERBOSITY);
	if(!IsWindow(hConsoleWnd)) return;
  if(Edit_GetText(hwnd,buffer,60)<=0) return;
	ctl->verbosity = atoi(buffer);
	ConsoleWndVerbosityUpdate();
}

void ConsoleWndVerbosityApplyIncDec(int num)
{
	int verbosity = 0;
	if(!IsWindow(hConsoleWnd)) return;
	verbosity = ctl->verbosity;
  if(num > 0)
		verbosity++;
  else if(num < 0)
		verbosity--;
  else
 		verbosity = 1;
	ctl->verbosity = verbosity;
	ConsoleWndVerbosityUpdate();
}

// ****************************************************************************
// List Window

BOOL CALLBACK ListWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
void InitListWnd(HWND hParentWnd)
{
	hListWnd = CreateDialog
//  	(hInst,MAKEINTRESOURCE(IDD_DIALOG_LIST),hParentWnd,ListWndProc);
  	(hInst,MAKEINTRESOURCE(IDD_DIALOG_SIMPLE_LIST),hParentWnd,ListWndProc);
	ShowWindow(hListWnd,SW_HIDE);
	UpdateWindow(hListWnd);
	UpdateListWnd();
}

BOOL CALLBACK
ListWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess){
		case WM_INITDIALOG:
			UpdateListWnd();
			return FALSE;
    case WM_COMMAND:
   		switch (HIWORD(wParam)) {
	  		case IDCLOSE:
					ShowWindow(hwnd, SW_HIDE);
					MainWndUpdateListButton();
					break;
 				case LBN_DBLCLK:
        	{
						if(LOWORD(wParam)==IDC_LISTBOX_PLAYLIST){
							HWND hListBox = (HWND)lParam;
          		int num = ListBox_GetCurSel(hListBox);
							if(num>=0){
        				SetCur_plNum(num);
					      PlayerOnPlayEx(RC_LOAD_FILE);
							}
						}
					}
					return FALSE;
				default:
  	 			return FALSE;
				}
	    return FALSE;
		case WM_VKEYTOITEM:
    {
    	UINT vkey = (UINT)LOWORD(wParam);
			int nCaretPos = (int)HIWORD(wParam);
			int num;
			switch(vkey){
				case VK_RETURN:
					num = nCaretPos;
					if(num>=0){
          	SetCur_plNum(num);
          	PlayerOnPlayEx(RC_LOAD_FILE);
          }
					return -2;
				default:
        	break;
			}
			return -1;
		}
		case WM_SIZE:
		    return FALSE;
		case WM_CLOSE:
			ShowWindow(hListWnd, SW_HIDE);
			MainWndUpdateListButton();
          break;
		case WM_DROPFILES:
			SendMessage(hMainWnd,WM_DROPFILES,wParam,lParam);
			return FALSE;
    default:
      	return FALSE;
	}
	return FALSE;
}

volatile static int ResetListWndDoing = 0;
#if 0 /* not used */
static void ResetListWnd(void)
{
	HWND hListBox = GetDlgItem(hListWnd, IDC_LISTBOX_PLAYLIST);
	PLAYLIST *pl;
	char local[1024];
	int top_index;
	if(!hListBox)
  	return;
	if(ResetListWndDoing)
  	return;
	ResetListWndDoing = 1;
	top_index = ListBox_GetTopIndex(hListBox);
	ListBox_ResetContent(hListBox);
	if(playlist!=NULL){
		PLAYLIST *current_playlist;
		LockPlaylist();
		current_playlist = cur_pl;
		for(pl=playlist;pl!=NULL;pl=pl->next){
			if(pl==current_playlist)
				sprintf(local,"* %s (%s)",pl->title,pl->filename);
			else
				sprintf(local,"  %s (%s)",pl->title,pl->filename);
			ListBox_AddString(hListBox,local);
		}
		UnLockPlaylist();
	}
	ListBox_SetTopIndex(hListBox,top_index);
	ListBox_SetCurSel(hListBox,cur_pl_num-1);
	ResetListWndDoing = 0;
}
#endif

volatile static int UpdateListWndDoing = 0;
void UpdateListWnd(void)
{
	HWND hListBox;
	PLAYLIST *pl;
	char local[1024],local2[1024];
	int top_index;
	if(!hListBox)
  	return;
	if(UpdateListWndDoing)
  	return;
	UpdateListWndDoing = 1;
	hListBox = GetDlgItem(hListWnd, IDC_LISTBOX_PLAYLIST);
	top_index = ListBox_GetTopIndex(hListBox);
//	ListBox_ResetContent(hListBox);
	if(playlist!=NULL){
		PLAYLIST *current_playlist;
		volatile char *p1,*p2;
		int i = -1;
		LockPlaylist();
		current_playlist = cur_pl;
		for(pl=playlist;pl!=NULL;pl=pl->next){
			i++;
			p1 = pl->title;
      if(p1==NULL || p1[0] == '\0')
      	p1 = " -------- ";
      p2 = pl->filename;
			if(pl==current_playlist)
				sprintf(local,"%04d*%s (%s)",i+1,p1,p2);
			else
				sprintf(local,"%04d %s (%s)",i+1,p1,p2);
			ListBox_GetText(hListBox,i,local2);
			if(strcmp(local,local2)){
				ListBox_DeleteString(hListBox,i);
				ListBox_InsertString(hListBox,i,local);
      }
//			ListBox_AddString(hListBox,local);
		}
		UnLockPlaylist();
	}
	ListBox_SetTopIndex(hListBox,top_index);
	ListBox_SetCurSel(hListBox,cur_pl_num-1);
	UpdateListWndDoing = 0;
}






// ***************************************************************************
// Tracer Window

BOOL CALLBACK TracerWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
void InitTracerWnd(HWND hParentWnd)
{
	hTracerWnd = CreateDialog
  	(hInst,MAKEINTRESOURCE(IDD_DIALOG_TRACER),hParentWnd,TracerWndProc);
	ShowWindow(hTracerWnd,SW_HIDE);
	UpdateWindow(hTracerWnd);
}

BOOL CALLBACK
TracerWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess){
		case WM_INITDIALOG:
			return FALSE;
	    case WM_COMMAND:
    		switch (LOWORD(wParam)) {
		  		case IDCLOSE:
					ShowWindow(hwnd, SW_HIDE);
					MainWndUpdateTracerButton();
					break;
				default:
        			return FALSE;
			}
		case WM_SIZE:
		    return FALSE;
		case WM_CLOSE:
			ShowWindow(hTracerWnd, SW_HIDE);
			MainWndUpdateTracerButton();
			break;
    default:
      	return FALSE;
	}
	return FALSE;
}




// ****************************************************************************
// Wrd Window

BOOL CALLBACK WrdWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
void InitWrdWnd(HWND hParentWnd)
{
	hWrdWnd = CreateDialog
  	(hInst,MAKEINTRESOURCE(IDD_DIALOG_WRD),hParentWnd,WrdWndProc);
	ShowWindow(hWrdWnd,SW_HIDE);
	UpdateWindow(hWrdWnd);
}

BOOL CALLBACK
WrdWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess){
		case WM_INITDIALOG:
			return FALSE;
	    case WM_COMMAND:
    		switch (LOWORD(wParam)) {
		  		case IDCLOSE:
					ShowWindow(hwnd, SW_HIDE);
					MainWndUpdateWrdButton();
					break;
				default:
        			return FALSE;
			}
		case WM_SIZE:
		    return FALSE;
		case WM_CLOSE:
			ShowWindow(hWrdWnd, SW_HIDE);
			MainWndUpdateWrdButton();
	        break;
    default:
      	return FALSE;
	}
	return FALSE;
}




//****************************************************************************
// Doc Window

BOOL CALLBACK DocWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
void InitDocEditWnd(HWND hParentWnd);

void InitDocWnd(HWND hParentWnd)
{
	hDocWnd = CreateDialog
  	(hInst,MAKEINTRESOURCE(IDD_DIALOG_DOC),hParentWnd,DocWndProc);
	ShowWindow(hDocWnd,SW_HIDE);
	UpdateWindow(hDocWnd);
}

BOOL CALLBACK
DocWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess){
		case WM_INITDIALOG:
			PutsDocWnd("Doc Window\n");
			return FALSE;
    case WM_COMMAND:
    	switch (LOWORD(wParam)) {
      	case IDCLOSE:
					ShowWindow(hwnd, SW_HIDE);
					MainWndUpdateDocButton();
          break;
        case IDCLEAR:
					ClearDocWnd();
          break;
	      default:
        	return FALSE;
      }
		case WM_CLOSE:
			ShowWindow(hDocWnd, SW_HIDE);
			MainWndUpdateDocButton();
	        break;
    default:
      	return FALSE;
	}
	return FALSE;
}

void PutsDocWnd(char *str)
{
	HWND hwnd;
	if(!IsWindow(hDocWnd) || !DocWndFlag)
		return;
	hwnd = GetDlgItem(hDocWnd,IDC_EDIT);
	PutsEditCtlWnd(hwnd,str);
}

void PrintfDocWnd(char *fmt, ...)
{
	HWND hwnd;
  va_list ap;
	if(!IsWindow(hDocWnd) || !DocWndFlag)
		return;
	hwnd = GetDlgItem(hDocWnd,IDC_EDIT);
  va_start(ap, fmt);
  VprintfEditCtlWnd(hwnd,fmt,ap);
  va_end(ap);
}

void ClearDocWnd(void)
{
	HWND hwnd;
	if(!IsWindow(hDocWnd))
		return;
	hwnd = GetDlgItem(hDocWnd,IDC_EDIT);
	ClearEditCtlWnd(hwnd);
}

//****************************************************************************
// SoundSpec Window

BOOL CALLBACK SoundSpecWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
void InitSoundSpecWnd(HWND hParentWnd)
{
	hSoundSpecWnd = CreateDialog
  	(hInst,MAKEINTRESOURCE(IDD_DIALOG_SOUNDSPEC),hParentWnd,SoundSpecWndProc);
	ShowWindow(hSoundSpecWnd,SW_HIDE);
	UpdateWindow(hSoundSpecWnd);
}

BOOL CALLBACK
SoundSpecWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess){
		case WM_INITDIALOG:
			return FALSE;
	    case WM_COMMAND:
    		switch (LOWORD(wParam)) {
		  		case IDCLOSE:
					ShowWindow(hwnd, SW_HIDE);
					MainWndUpdateSoundSpecButton();
					break;
				default:
        			return FALSE;
			}
		case WM_SIZE:
		    return FALSE;
		case WM_CLOSE:
			ShowWindow(hSoundSpecWnd, SW_HIDE);
			MainWndUpdateSoundSpecButton();
          break;
    default:
      	return FALSE;
	}
	return FALSE;
}


































// ****************************************************************************
// Main Thread

#ifdef W32ENABLE_OPTIONS
#define OPTCOMMANDS "A:aB:b:C:c:D:d:eE:Ffg:hI:i:jL:n:O:o:P:p:Q:R:rS:s:t:UW:w:x:"
extern int optind;
extern char *optarg;
extern int getopt(int, char **, char *);
extern int set_tim_opt(int c, char *optarg);
#endif /* W32ENABLE_OPTIONS */

void MainThread(void *arglist)
{
    int argc = *(((MAINTHREAD_ARGS *)arglist)->pArgc);
    char **argv = *(((MAINTHREAD_ARGS *)arglist)->pArgv);
    int err = 0;
    int nfiles = argc;
    char **files = argv;
    char c;

    if((program_name=strrchr(argv[0], PATH_SEP))!=NULL)
	program_name++;
    else
	program_name=argv[0];

    timidity_start_initialize();
    timidity_init_player();

// commindline option
#ifdef W32ENABLE_OPTIONS
    while((c = getopt(argc, argv, OPTCOMMANDS)) > 0)
	if((err += set_tim_opt(c, optarg)) != 0)
	    break;
#endif /* W32ENABLE_OPTIONS */

    if(read_config_file(ConfigFile,0))
    {
	char str[256];

	ctl->cmsg(CMSG_INFO, VERB_NOISY,
		  "Warning: Cannot read %s correctly",
		  ConfigFile);
	snprintf(str, sizeof(str),
		 "Warning: Cannot read %s correctly", ConfigFile);
	MessageBox(hMainWnd, str, "Error", MB_OK);
    }
    SaveSettingPlayer(sp_current);
    SaveSettingTimidity(st_current); /* Initialize st_current */
    LoadIniFile(sp_current,st_current);
    ApplySettingPlayer(sp_current);
    ApplySettingTimidity(st_current);
    SaveSettingPlayer(sp_default);
    SaveSettingTimidity(st_default);
    SaveIniFile(sp_current,st_current);

    w32g_init_ctl();

    nfiles = argc - optind;
    files  = argv + optind;
    if(ctl->flags & CTLF_LIST_RANDOM)
	randomize_string_list(files, nfiles);
    else if(ctl->flags & CTLF_LIST_SORT)
	sort_pathname(files, nfiles);
    PlayerPlaylistAddFiles(nfiles,files,1);
    wait_thread_flag = 0;
    Player_loop(1);

  done:
#if defined(__CYGWIN32__) || defined(__MINGW32__)
    ExitThread(0);
#else
    _endthread();
#endif
}




















// **************************************************************************
// Misc Dialog

void DlgMultiFileOpen
		(char*** ppszFileList,int* icbCount,char *filter, char *dir)
{
#define cbszFileName 16536
	OPENFILENAME ofn;
	int iIndex;
	char* pFileName  ;
	pFileName = (char*)calloc(cbszFileName, sizeof(char)) ;
	pFileName[0] = '\0' ;
	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize	= sizeof(OPENFILENAME);
	ofn.hwndOwner		= 0;
	ofn.hInstance		= hInst ;
	ofn.lpstrFilter	= filter;
	ofn.lpstrCustomFilter  = 0;
	ofn.nMaxCustFilter	  = 1 ;
	ofn.nFilterIndex	= 1 ;
	ofn.lpstrFile		= pFileName;
	ofn.nMaxFile		= cbszFileName;
	ofn.lpstrFileTitle	= 0;
	ofn.nMaxFileTitle 	= 0;
  if(dir[0] != '\0')
		ofn.lpstrInitialDir	= dir;
	else
		ofn.lpstrInitialDir	= 0;
	ofn.lpstrTitle		= 0;
	ofn.Flags			= OFN_FILEMUSTEXIST  | OFN_PATHMUSTEXIST
		| OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_READONLY;
	ofn.lpstrDefExt	= 0;
	ofn.lCustData		= 0;
	ofn.lpfnHook		= 0;
	ofn.lpTemplateName= 0;

	if(!GetOpenFileName(&ofn))
		return;

// count files
	for(iIndex=1 , (*icbCount)=-1 ;(pFileName[iIndex-1] != '\0') || (pFileName[iIndex] != '\0') ;iIndex++ )
	{
		if(pFileName[iIndex] == '\0')
			(*icbCount)++;
	}
	// extract file list
		if( (*icbCount) > 0 )
	{
		(*ppszFileList) = (char**)calloc(*icbCount, sizeof(char*) );
		for(iIndex=0;iIndex<(*icbCount);iIndex++)
		{
			while( ( *(ofn.lpstrFile++) )!= '\0')
			{ (void)0 ; }
			(*ppszFileList)[iIndex] = (char*)calloc(strlen(ofn.lpstrFile)
				+ strlen(pFileName) + 2 , sizeof(char) ) ;
			strcpy((*ppszFileList)[iIndex],pFileName);
			strcat((*ppszFileList)[iIndex],"\\");
			strcat( (*ppszFileList)[iIndex] , ofn.lpstrFile );
		}
	}
	else
	{
		(*icbCount) = 1 ;
		(*ppszFileList) = (char**)calloc(*icbCount,sizeof(char*));
		(*ppszFileList)[0] = (char*)calloc( strlen(pFileName) + 1 , sizeof(char) );
		strcpy((*ppszFileList)[0],pFileName);
	}
	free(pFileName);
	return ;
}

char MidiFileExtFilter[] =
	"midi file\0*.mid;*.smf;*.rcp;*.r36;*.g18;*.g36\0"
	"archive file\0*.lzh;*.zip;*.gz\0"
	"all files\0*.*\0"
	"\0\0";

void DlgMidiFileOpen(void)
{
	char **files = NULL;
	int nfiles = 0, i;
	PLAYLIST *cur_pl_old;
	DlgMultiFileOpen(&files,&nfiles,MidiFileExtFilter,MidiFileOpenDir);
	if(nfiles>0){
		cur_pl_old = cur_pl;
		PlayerPlaylistAddFiles(nfiles,files,1);
		for(i=0;i<nfiles;i++)
  		free(files[i]);
  	free(files);
		cur_pl = cur_pl_old;
		PlayerPlaylistNum();
	}
	if(cur_pl==NULL && playlist != NULL){
  	PLAYLIST *pl = first_playlist(playlist);
    if(cur_pl==NULL){
    	cur_pl = pl;
			PlayerPlaylistNum();
			TmPanelReset();
    }
  }
	TmPanelSet();
  TmPanelUpdate();
  UpdateListWnd();
}



// ****************************************************************************
// Edit Ctl.

#define USE_LOTATION_BUFFER

void VprintfEditCtlWnd(HWND hwnd, char *fmt, va_list argList)
{
	char buffer[LotationBufferMAX];
	char *in = buffer;
	int i;
#ifndef USE_LOTATION_BUFFER
	char out[LotationBufferMAX];
#else
	char *out = GetLotationBuffer();
#endif
	vsprintf(in, fmt, argList);
  i = 0;
	for(;;){
		if(*in == '\0' || i>LotationBufferMAX-3){
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
		Edit_SetSel(hwnd,0,-1);
		Edit_SetSel(hwnd,-1,-1);
		Edit_ReplaceSel(hwnd,out);
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
#ifndef USE_LOTATION_BUFFER
	char out[LotationBufferMAX];
#else
	char *out = GetLotationBuffer();
#endif
	i = 0;
	for(;;){
		if(*in == '\0' || i>LotationBufferMAX-3){
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
		Edit_SetSel(hwnd,0,-1);
		Edit_SetSel(hwnd,-1,-1);
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
	char pszVoid[]="";
	if(!IsWindow(hwnd))
		return;
	Edit_SetText(hwnd,pszVoid);
}

// *****************************************************************************
// TiMidity settings

static BOOL CALLBACK SettingWndProc(HWND hwnd, UINT uMess,
				    WPARAM wParam, LPARAM lParam);
static void OpenSettingWnd(HWND hParentWnd)
{
    static int setting_warning_displayed = 0;

    if(hSettingWnd == 0)
	hSettingWnd = CreateDialog(hInst,
				   MAKEINTRESOURCE(IDD_DIALOG_SETTING),
				   hParentWnd,SettingWndProc);
    ShowWindow(hSettingWnd,SW_SHOW);
    UpdateWindow(hSoundSpecWnd);
    SettingWndSetup();
}

#define IS_CHECKED(hwnd, id) (SendDlgItemMessage(hwnd, id, BM_GETCHECK, 0, 0) == BST_CHECKED)
#define SET_CHECKED(hwnd, id, test) \
    if(test) \
	SendDlgItemMessage(hwnd, id, BM_SETCHECK, BST_CHECKED, 0); \
    else \
	SendDlgItemMessage(hwnd, id, BM_SETCHECK, BST_UNCHECKED, 0)

static void SettingWndSetup(void)
{
    char numstr[16];

    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_CHORUS, opt_chorus_control);

    SendDlgItemMessage(hSettingWnd, IDC_CHECKBOX_CHORUS,
		       BM_SETSTYLE,
		       BS_AUTOCHECKBOX | BS_LEFT | BS_TOP | WS_TABSTOP | WS_DISABLED,
		       0);

    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_REVERB, opt_reverb_control);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_DELAY_EFFECT, effect_lr_mode != -1);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_FREE_INST,
		free_instruments_afterwards);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_ANTIALIAS, antialiasing_allowed);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_MODWHEEL, opt_modulation_wheel);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_PORTAMENT, opt_portamento);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_NRPNVIB, opt_nrpn_vibrato);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_CHPRESS, opt_channel_pressure);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_OVOICE, opt_overlap_voice_allow);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_LOADINST_PLAYING,
		opt_realtime_playing);
    SET_CHECKED(hSettingWnd, IDC_RADIO_16BITS, play_mode->encoding & PE_16BIT);
    SET_CHECKED(hSettingWnd, IDC_RADIO_8BITS,
		!(play_mode->encoding & PE_16BIT));
    SET_CHECKED(hSettingWnd, IDC_RADIO_MONO, play_mode->encoding & PE_MONO);
    SET_CHECKED(hSettingWnd, IDC_RADIO_STEREO,
		!(play_mode->encoding & PE_MONO));

    /* sample rate */
    sprintf(numstr, "%d", play_mode->rate);
    SetDlgItemText(hSettingWnd, IDC_EDIT_SAMPLE_RATE, numstr);

    
    SetDlgItemText(hSettingWnd, IDC_EDIT_SAMPLE_RATE, numstr);
//    SendDlgItemMessage(hSettingWnd, IDC_EDIT_SAMPLE_RATE,
//		       EN_CHANGE, WS_DISABLED, 0);

{static int i;
i = !i;
//    SendDlgItemMessage(hSettingWnd, IDC_EDIT_SAMPLE_RATE,
//		       EM_SETREADONLY, i, 0);

//    SendDlgItemMessage(hSettingWnd, IDC_EDIT_SAMPLE_RATE,
//		       WM_ENABLE, 0, 0);
}


    /* number of voices */
    sprintf(numstr, "%d", voices);
    SetDlgItemText(hSettingWnd, IDC_EDIT_VOICES, numstr);

    /* Noise sharping */
    sprintf(numstr, "%d", noise_sharp_type);
    SetDlgItemText(hSettingWnd, IDC_EDIT_NOISESHARPING, numstr);
}

static void SettingWndApply(void)
{
    int is_checked, enc, rate, nv, ns, antialias;
    char numstr[16];

    if(player_status != PLAYERSTATUS_STOP)
    {
	ShowWindow(hSettingWnd, SW_HIDE);
	MessageBox(hMainWnd, "Can't change parameter while playing",
		   "Warning", MB_OK);
	return;
    }

    opt_chorus_control = IS_CHECKED(hSettingWnd, IDC_CHECKBOX_CHORUS);
    opt_reverb_control = IS_CHECKED(hSettingWnd, IDC_CHECKBOX_REVERB);
    if(IS_CHECKED(hSettingWnd, IDC_CHECKBOX_DELAY_EFFECT))
	effect_lr_mode = 2;
    else
	effect_lr_mode = -1;
    free_instruments_afterwards = IS_CHECKED(hSettingWnd,
					     IDC_CHECKBOX_FREE_INST);
    antialias = IS_CHECKED(hSettingWnd, IDC_CHECKBOX_ANTIALIAS);
    opt_modulation_wheel = IS_CHECKED(hSettingWnd, IDC_CHECKBOX_MODWHEEL);
    opt_portamento = IS_CHECKED(hSettingWnd, IDC_CHECKBOX_PORTAMENT);
    opt_nrpn_vibrato = IS_CHECKED(hSettingWnd, IDC_CHECKBOX_NRPNVIB);
    opt_channel_pressure = IS_CHECKED(hSettingWnd, IDC_CHECKBOX_CHPRESS);
    opt_overlap_voice_allow = IS_CHECKED(hSettingWnd, IDC_CHECKBOX_OVOICE);
    opt_realtime_playing = IS_CHECKED(hSettingWnd,
				      IDC_CHECKBOX_LOADINST_PLAYING);

    if(antialiasing_allowed != antialias)
    {
	antialiasing_allowed = antialias;
	free_instruments(1);
    }

    enc = play_mode->encoding;
    rate = play_mode->rate;
    nv = voices;

    /* 16 bit or 8 bit */
    is_checked = IS_CHECKED(hSettingWnd, IDC_RADIO_16BITS);
    if(is_checked)
	enc |= PE_16BIT;
    else
	enc &= ~PE_16BIT;

    /* stereo or mono */
    is_checked = IS_CHECKED(hSettingWnd, IDC_RADIO_MONO);
    if(is_checked)
	enc |= PE_MONO;
    else
	enc &= ~PE_MONO;

    GetDlgItemText(hSettingWnd, IDC_EDIT_SAMPLE_RATE, numstr, sizeof(numstr));
    rate = atoi(numstr);

    GetDlgItemText(hSettingWnd, IDC_EDIT_NOISESHARPING, numstr, sizeof(numstr));
    ns = atoi(numstr);

    GetDlgItemText(hSettingWnd, IDC_EDIT_VOICES, numstr, sizeof(numstr));
    nv = atoi(numstr);

    if(play_mode->encoding != enc || play_mode->rate != rate)
	target_play_mode = play_mode;
    play_mode->encoding = enc;
    play_mode->rate = rate;
    voices = nv;
    noise_sharp_type = ns;
    SaveSettingTimidity(st_current);
}

static void SettingWndDefault(void)
{
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_CHORUS, 1);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_REVERB, 1);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_DELAY_EFFECT, 0);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_FREE_INST, 0);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_ANTIALIAS, 0);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_MODWHEEL, 1);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_PORTAMENT, 1);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_NRPNVIB, 1);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_CHPRESS, 0);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_OVOICE, 1);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_LOADINST_PLAYING, 0);
    SET_CHECKED(hSettingWnd, IDC_RADIO_16BITS, 1);
    SET_CHECKED(hSettingWnd, IDC_RADIO_8BITS, 0);
    SET_CHECKED(hSettingWnd, IDC_RADIO_STEREO, 1);
    SET_CHECKED(hSettingWnd, IDC_RADIO_MONO, 0);

    SetDlgItemText(hSettingWnd, IDC_EDIT_SAMPLE_RATE, "33075");
    SetDlgItemText(hSettingWnd, IDC_EDIT_VOICES, "128");
    SetDlgItemText(hSettingWnd, IDC_EDIT_NOISESHARPING, "0");
}

static BOOL CALLBACK SettingWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
    static int setting_warning_displayed = 0;
    switch(uMess)
    {
      case WM_SHOWWINDOW:
	if(wParam)
	{
	    if(player_status != PLAYERSTATUS_STOP &&
	       !setting_warning_displayed)
		MessageBox(hMainWnd, "Don't change parameter while playing",
			   "Warning", MB_OK);
	    setting_warning_displayed = 1;
	    SettingWndSetup();
	}
	else
	    setting_warning_displayed = 0;
	break;
      case WM_COMMAND:
    	switch (LOWORD(wParam))
	{
	  case IDCANCEL:
	    ShowWindow(hSettingWnd, SW_HIDE);
	    break;
	  case IDOK:
	    SettingWndApply();
	    ShowWindow(hSettingWnd, SW_HIDE);
	    break;
	  case IDDEFAULT:
	    SettingWndDefault();
	    break;
	}
	break;

      case WM_CLOSE:
	ShowWindow(hSettingWnd, SW_HIDE);
	break;
    }
    return 0;
}

// ****************************************************************************
// Misc funciton.

#if 0 /* not used */
static char *get_filename(char *src, char *dest)
{
	char *p = src;
	char *start = NULL;
	int quot_flag = 0;
	if(p == NULL)
		return NULL;
	for(;;){
		if(*p != ' ' && *p != '\0' && start == NULL)
			start = p;
		if(*p == '\'' || *p == '\"'){
			if(quot_flag){
				if(p - start != 0)
					strncpy(dest, start, p - start);
				dest[p-start] = '\0';
				p++;
				return p;
			} else {
				quot_flag = !quot_flag;
				p++;
				start = p;
				continue;
			}
		}
		if(*p == '\0' || (*p == ' ' && !quot_flag)){
			if(start == NULL)
				return NULL;
			if(p - start != 0)
				strncpy(dest, start, p - start);
			dest[p-start] = '\0';
			if(*p != '\0')
				p++;
			return p;
		}
		p++;
	}
}

static void CmdLineToArgv(LPSTR lpCmdLine, int *pArgc, CHAR ***pArgv)
{
	LPSTR p = lpCmdLine , buffer, lpsRes;
	int i, max = -1, inc = 16;
	int buffer_size;

	buffer_size = strlen(lpCmdLine) + 1024;
	buffer = safe_malloc(sizeof(CHAR) * buffer_size + 1);
	strcpy(buffer, lpCmdLine);

	for(i=0;;i++)
	{
	if(i > max){
		max += inc;
		*pArgv = (CHAR **)safe_realloc(*pArgv, sizeof(CHAR *) * (max + 1));
	}
	if(i==0){
		GetModuleFileName(NULL,buffer,buffer_size);
		lpsRes = p;
	} else
		lpsRes = get_filename(p,buffer);
	if(lpsRes != NULL){
		(*pArgv)[i] = (CHAR *)safe_malloc(sizeof(CHAR) * strlen(buffer) + 1);
		strcpy((*pArgv)[i],buffer);
		p = lpsRes;
	} else {
		*pArgc = i;
		free(buffer);
		return;
	}
	}
}
#endif
