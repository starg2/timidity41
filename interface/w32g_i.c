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

    w32g_i.c: by Daisuke Aoki <dai@y7.net>
                 Masanao Izumo <mo@goice.co.jp>
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
#ifdef SUPPORT_SOUNDSPEC
#include "soundspec.h"
#endif /* SUPPORT_SOUNDSPEC */
#include "aq.h"
#include "w32g.h"
#include "w32g_res.h"

#include <windowsx.h>	/* There is no <windowsx.h> on CYGWIN.
			 * Edit_* and ListBox_* are defined in
			 * <windowsx.h>
			 */

// #include <commctrl.h> /* FIXME */
//#if defined(__CYGWIN32__) || defined(__MINGW32__)
//WINAPI void InitCommonControls(void);
//#endif

#define IDM_STOP	12501
#define IDM_PAUSE	12502
#define IDM_PREV	12503
#define IDM_FOREWARD	12504
#define IDM_PLAY	12505
#define IDM_BACKWARD	12506
#define IDM_NEXT	12507
#define IDM_CONSOLE	12511
#define IDM_LIST	12512
#define IDM_TRACER	12513
#define IDM_DOC		12514
#define IDM_WRD		12515
#define IDM_SOUNDSPEC	12516

#define STARTWND_XSIZE 100
#define STARTWND_YSIZE 100
static char StartWndClassName[] = "MainWindow";


HINSTANCE hInst;

// HWND
static HWND hMainWnd = 0;
static HWND hConsoleWnd = 0;
static HWND hTracerWnd = 0;
static HWND hDocWnd = 0;
HWND hListWnd = 0; // global
static HWND hWrdWnd = 0;
static HWND hSettingWnd = 0;

static struct {
    int id;
    HWND *hwnd;
    int status;
#define SWS_EXIST		0x0001
#define SWS_ICON		0x0002
#define SWS_HIDE		0x0004
} subwindow[] =
{
    {IDM_CONSOLE,&hConsoleWnd,	0},
    {IDM_LIST,	&hListWnd,	0},
    {IDM_TRACER,&hTracerWnd,	0},
    {IDM_DOC,	&hDocWnd,	0},
    {IDM_WRD,	&hWrdWnd,	0},
    {0,		NULL,		0}
};


static void GUIThread(void *arglist);
static void update_subwindow(int id);
static HWND ID2SubWindow(int id);
static int ID2SubWindowIdx(int id);
static void MainWndUpdateButton(int id);
static void InitToolbar(HWND hwnd);
static LRESULT CALLBACK StartWinProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static WINBOOL CALLBACK MainProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static WINBOOL CALLBACK ListWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static WINBOOL CALLBACK ConsoleWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static void ConsoleWndVerbosityApplyIncDec(int diff);
static void InitSettingWnd(HWND hParentWnd);
static void OpenSettingWnd(HWND hwnd);
static void SettingWndSetup(SETTING_TIMIDITY *st);

static HANDLE w32g_lock_sem = NULL;
static HANDLE w32g_empty_sem = NULL;
static int progress_jump = -1;
static HWND hMainWndScrollbarProgressWnd;
static HWND hMainWndScrollbarVolumeWnd;
#define W32G_VOLUME_MAX 200
static VOLATILE int w32g_wait_for_init;



static HANDLE hGUIThread = 0;
static HANDLE hPlayerThread = 0;
static DWORD dwGUIThreadID = 0;

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

int PlayerThreadPriority = THREAD_PRIORITY_NORMAL;
int GUIThreadPriority = THREAD_PRIORITY_NORMAL;

int WrdGraphicFlag;
int TraceGraphicFlag;

int DocMaxSize;
char *DocFileExt;
HWND hDebugEditWnd = 0;
HWND hDocEditWnd = 0;

int w32g_lock_open_file = 0;

#define RC_QUEUE_SIZE 8
static struct
{
    int rc;
    int32 value;
} rc_queue[RC_QUEUE_SIZE];
static VOLATILE int rc_queue_len, rc_queue_beg, rc_queue_end;


void w32g_lock(void)
{
    WaitForSingleObject(w32g_lock_sem, INFINITE);
}

void w32g_unlock(void)
{
    ReleaseSemaphore(w32g_lock_sem, 1, NULL);
}

void w32g_send_rc(int rc, int32 value)
{
    int full;

    w32g_lock();

    if(rc_queue_len == RC_QUEUE_SIZE)
    {
	/* Over flow.  Remove the oldest message */
	rc_queue_len--;
	rc_queue_beg = (rc_queue_beg + 1) % RC_QUEUE_SIZE;
	full = 1;
    }
    else
	full = 0;

    rc_queue_len++;
    rc_queue[rc_queue_end].rc = rc;
    rc_queue[rc_queue_end].value = value;
    rc_queue_end = (rc_queue_end + 1) % RC_QUEUE_SIZE;
    ReleaseSemaphore(w32g_empty_sem, 1, NULL);
    w32g_unlock();
}

int w32g_get_rc(int32 *value, int wait_if_empty)
{
    int rc;

    while(rc_queue_len == 0)
    {
	if(!wait_if_empty)
	    return RC_NONE;
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

static void NotImplemented(char *msg)
{
    char buff[BUFSIZ];
    strcpy(buff, msg);
    strcat(buff, " is not implemented.");
    w32g_msg_box(buff, "TiMidity Notice", MB_OK);
}

void PutsConsoleWnd(char *str)
{
    HWND hwnd;

// #### for debug
//    fputs(str, stdout);
//    fflush(stdout);

    if(!IsWindow(hConsoleWnd) ||
       !IsDlgButtonChecked(hConsoleWnd,IDC_CHECKBOX_VALID))
	return;

    hwnd = GetDlgItem(hConsoleWnd, IDC_EDIT_TERMINAL);
    if(!hwnd)
	return;
    w32g_lock();
    Edit_SetSel(hwnd,  0, -1);
    Edit_SetSel(hwnd, -1, -1);
    Edit_ReplaceSel(hwnd, str);
    w32g_unlock();
}

int SubWindowMax = 2;


void OnExit(void)
{
    PostQuitMessage(0);
}

void OnDestroy(void)
{
    TmFreeColor();
    OnExit();
}


static void ToggleSubWindow(int id)
{
    HWND hwnd = ID2SubWindow(id);

    if(!hwnd)
	return;
    if(IsWindowVisible(hwnd))
	ShowWindow(hwnd,SW_HIDE);
    else
    {
	ShowWindow(hwnd,SW_SHOW);
	switch(id)
	{
	  case IDM_LIST:
	    w32g_update_playlist();
	    break;
	}
    }
    update_subwindow(id);
    MainWndUpdateButton(id);
}

static void DlgMidiFileOpen(void)
{
    OPENFILENAME ofn;
    static char pFileName[16536];
    char *dir;
    static char *filter =
	"Midi file\0*.mid;*.smf;*.rcp;*.r36;*.g18;*.g36\0"
	"Archive file\0*.lzh;*.zip;*.gz;*.tgz\0"
	"All files\0*.*\0"
	"\0\0";
    if(w32g_lock_open_file)
	return;

    memset(pFileName, 0, sizeof(pFileName));
    memset(&ofn, 0, sizeof(OPENFILENAME));
    if(MidiFileOpenDir[0])
	dir = MidiFileOpenDir;
    else
	dir = NULL;

    ofn.lStructSize	= sizeof(OPENFILENAME);
    ofn.hwndOwner	= 0;
    ofn.hInstance	= hInst ;
    ofn.lpstrFilter	= filter;
    ofn.lpstrCustomFilter= 0;
    ofn.nMaxCustFilter	= 1;
    ofn.nFilterIndex	= 1 ;
    ofn.lpstrFile	= pFileName;
    ofn.nMaxFile	= sizeof(pFileName);
    ofn.lpstrFileTitle	= 0;
    ofn.nMaxFileTitle 	= 0;
    ofn.lpstrInitialDir	= dir;
    ofn.lpstrTitle	= 0;
    ofn.Flags		= OFN_FILEMUSTEXIST  | OFN_PATHMUSTEXIST
			| OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_READONLY;
    ofn.lpstrDefExt	= 0;
    ofn.lCustData	= 0;
    ofn.lpfnHook	= 0;
    ofn.lpTemplateName	= 0;

    if(!GetOpenFileName(&ofn))
	return;
    w32g_lock_open_file = 1;
    w32g_send_rc(RC_EXT_LOAD_FILE, (int32)pFileName);
}

static void VersionWnd()
{
    char VersionText[2048];

      sprintf(VersionText,
" TiMidity++ version %s -- MIDI to WAVE converter and player" NLS
" Copyright (C) 1999 Masanao Izumo <mo@goice.co.jp>" NLS
" Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>" NLS
NLS
" Windows version by" NLS
"       Davide Moretti <dmoretti@iper.net>." NLS
"       Nicolas Witczak." NLS
"       Daisuke Aoki <dai@y7.net>." NLS
"       Masanao Izumo <mo@goice.co.jp>." NLS
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
	      ,timidity_version);
    w32g_msg_box(VersionText, "Version", MB_OK);
}

void MainCmdProc(HWND hwnd, int wId, HWND hwndCtl, UINT wNotifyCode)
{
    switch(wId)
    {
      case IDM_STOP:
	w32g_send_rc(RC_STOP, 0);
	break;
      case IDM_PAUSE:
	w32g_send_rc(RC_TOGGLE_PAUSE, 0);
	break;
      case IDM_PREV:
	w32g_send_rc(RC_REALLY_PREVIOUS, 0);
	break;
      case IDM_BACKWARD:
	w32g_send_rc(RC_BACK, play_mode->rate * 1);
	break;
      case IDM_PLAY:
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
	w32g_send_rc(RC_FORWARD, play_mode->rate * 1);
	break;
      case IDM_NEXT:
	w32g_send_rc(RC_NEXT, 0);
	break;

      case IDM_CONSOLE:
      case IDM_MWCONSOLE:
	ToggleSubWindow(IDM_CONSOLE);
	break;
      case IDM_TRACER:
      case IDM_MWTRACER:
	NotImplemented("Tracer");
	SendDlgItemMessage(hMainWnd, IDC_TOOLBARWINDOW_SUBWND,
			   TB_CHECKBUTTON, IDM_TRACER,
			   (LPARAM)MAKELONG(FALSE, 0));
	break;
      case IDM_LIST:
      case IDM_MWPLAYLIST:
	ToggleSubWindow(IDM_LIST);
	break;
      case IDM_DOC:
      case IDM_MWDOCUMENT:
	NotImplemented("Document");
	SendDlgItemMessage(hMainWnd, IDC_TOOLBARWINDOW_SUBWND,
			   TB_CHECKBUTTON, IDM_DOC,
			   (LPARAM)MAKELONG(FALSE, 0));
	break;
      case IDM_WRD:
      case IDM_MWWRDTRACER:
	NotImplemented("WRD");
	SendDlgItemMessage(hMainWnd, IDC_TOOLBARWINDOW_SUBWND,
			   TB_CHECKBUTTON, IDM_WRD,
			   (LPARAM)MAKELONG(FALSE, 0));
	break;
      case IDM_SOUNDSPEC:
      case IDM_MWSOUNDSPEC:
	NotImplemented("Sound spectrogram");
	SendDlgItemMessage(hMainWnd, IDC_TOOLBARWINDOW_SUBWND,
			   TB_CHECKBUTTON, IDM_SOUNDSPEC,
			   (LPARAM)MAKELONG(FALSE, 0));
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
	NotImplemented("Open Directory");
	break;
      case IDM_MFLOADPLAYLIST:
	NotImplemented("Load Playlist");
	break;
      case IDM_MFSAVEPLAYLISTAS:
	NotImplemented("Save Playlist");
	break;
      case IDM_MFEXIT:
	OnExit();
	break;
      case IDM_SETTING:
	OpenSettingWnd(hwnd);
	break;
      case IDM_MCSAVEINIFILE:
	SaveIniFile(sp_current, st_current);
	w32g_has_ini_file = 1;
	break;
      case IDM_MCLOADINIFILE:
	if(w32g_has_ini_file)
	{
	    LoadIniFile(sp_current, st_current);
	    SettingWndSetup(st_current);
	    OpenSettingWnd(hwnd);
	}
	else
	    w32g_msg_box("Can't load ini file.",
			 "TiMidity Warning", MB_OK);
	break;
      case IDM_MHVERSION:
	VersionWnd();
	break;
    }
}

static WINBOOL CALLBACK
MainProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
    HMENU hMenu;

    switch (uMess)
    {
      case WM_INITDIALOG:
	hMenu = GetSystemMenu(hwnd, FALSE);
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
	break;

      HANDLE_MSG(hwnd,WM_COMMAND,MainCmdProc);

      HANDLE_MSG(hwnd,WM_CLOSE,DestroyWindow);

      case WM_DESTROY:
    	OnDestroy();
	break;

      case WM_SIZE:
    	if(wParam == SIZE_MINIMIZED) {
	    update_subwindow(-1);
	}
	break;

      case WM_DROPFILES:
	w32g_send_rc(RC_EXT_DROP, (int32)wParam);
	break;

      case WM_HSCROLL: {
	  int nScrollCode = (int)LOWORD(wParam);
	  int nPos = (int) HIWORD(wParam);
	  HWND bar = (HWND)lParam;

	  if(bar != hMainWndScrollbarProgressWnd)
	      break;

	  switch(nScrollCode)
	  {
	    case SB_THUMBTRACK:
	    case SB_THUMBPOSITION:
	      progress_jump = nPos;
	      break;
	    case SB_LINELEFT:
	      progress_jump = GetScrollPos(bar, SB_CTL) - 1;
	      if(progress_jump < 0)
		  progress_jump = 0;
	      break;
	    case SB_PAGELEFT:
	      progress_jump = GetScrollPos(bar, SB_CTL) - 10;
	      if(progress_jump < 0)
		  progress_jump = 0;
	      break;
	    case SB_LINERIGHT:
	      progress_jump = GetScrollPos(bar, SB_CTL) + 1;
	      break;
	    case SB_PAGERIGHT:
	      progress_jump = GetScrollPos(bar, SB_CTL) + 10;
	      break;
	    case SB_ENDSCROLL:
	      if(progress_jump != -1)
	      {
		  w32g_send_rc(RC_JUMP, progress_jump * play_mode->rate);
		  SetScrollPos(hMainWndScrollbarProgressWnd, SB_CTL,
			       progress_jump, TRUE);
		  progress_jump = -1;
	      }
	      break;
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
	      pos = GetScrollPos(bar, SB_CTL) - 5;
	      if(pos < 0)
		  pos = 0;
	      break;
	    case SB_LINEDOWN:
	    case SB_PAGEDOWN:
	      pos = GetScrollPos(bar, SB_CTL) + 5;
	      if(pos > W32G_VOLUME_MAX)
		  pos = W32G_VOLUME_MAX;
	      break;
	    case SB_ENDSCROLL:
	      if(pos != -1)
	      {
		  w32g_send_rc(RC_CHANGE_VOLUME,
			       (W32G_VOLUME_MAX - pos) - amplification);
		  SetScrollPos(bar, SB_CTL, pos, TRUE);
		  pos = -1;
	      }
	      break;
	  }
        }
	break;

      case WM_SYSCOMMAND:
	switch(wParam)
	{
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
	break;
    }
    return 0;
}

static void update_subwindow(int id)
{
    int i;

    if(id == -1)
    {
	for(i = 0; subwindow[i].hwnd != NULL; i++)
	    update_subwindow(subwindow[i].id);
    }
    else
    {
	i = ID2SubWindowIdx(id);
	if(i == -1)
	    return;

	if(IsWindow(*(subwindow[i].hwnd)))
	    subwindow[i].status |= SWS_EXIST;
	else
	{
	    subwindow[i].status = 0;
	    return;
	}
	if(IsIconic(*(subwindow[i].hwnd)))
	    subwindow[i].status |= SWS_ICON;
	else
	    subwindow[i].status &= ~SWS_ICON;
	if(IsWindowVisible(*(subwindow[i].hwnd)))
	    subwindow[i].status &= ~SWS_HIDE;
	else
	    subwindow[i].status |= SWS_HIDE;
    }
}

static LRESULT CALLBACK
StartWinProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
    switch(uMess)
    {
      HANDLE_MSG(hwnd, WM_CLOSE, DestroyWindow);
      case WM_DESTROY:
    	OnDestroy();
	break;
      default:
	return DefWindowProc(hwnd,uMess,wParam,lParam);
    }
    return 0L;
}

static void GUIThread(void *arglist)
{
    MSG msg;
    HWND hStartWnd;
    HICON hIcon;
    WNDCLASSEX wcl;

//    InitCommonControls();

    hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON_TIMIDITY));
    memset(&wcl, 0, sizeof(wcl));
    wcl.cbSize	      = sizeof(WNDCLASSEX);
    wcl.hInstance     = hInst;
    wcl.lpszClassName = StartWndClassName;
    wcl.lpfnWndProc   = StartWinProc;
    wcl.style         = 0;
    wcl.hIcon         = hIcon;
    wcl.hCursor       = LoadCursor(0,IDC_ARROW);
    wcl.lpszMenuName  = NULL;
    wcl.cbClsExtra    = 0;
    wcl.cbWndExtra    = 0;
    wcl.hbrBackground = (HBRUSH)(COLOR_SCROLLBAR + 1);
    RegisterClassEx(&wcl);

    hStartWnd = CreateWindow(StartWndClassName,
			     "TiMidity",
			     WS_OVERLAPPEDWINDOW,
			     CW_USEDEFAULT, CW_USEDEFAULT,
			     CW_USEDEFAULT, CW_USEDEFAULT,
			     HWND_DESKTOP,
			     NULL,
			     hInst,
			     NULL);
    ShowWindow(hStartWnd, SW_HIDE);
    UpdateWindow(hStartWnd);

    /* Main Window */
    hMainWnd = CreateDialog(hInst, MAKEINTRESOURCE(IDD_DIALOG_MAIN),
			    hStartWnd, MainProc);
    if(hIcon != NULL)
	SendMessage(hMainWnd, WM_SETICON, FALSE, (LPARAM)hIcon);
    UpdateWindow(hMainWnd);

    InitToolbar(hMainWnd);

    /* ListWnd */
    hListWnd = CreateDialog(hInst, MAKEINTRESOURCE(IDD_DIALOG_SIMPLE_LIST),
			    hStartWnd, ListWndProc);
    ShowWindow(hListWnd,SW_HIDE);
    UpdateWindow(hListWnd);

    /* ConsoleWnd */
    hConsoleWnd = CreateDialog(hInst, MAKEINTRESOURCE(IDD_DIALOG_CONSOLE),
			       hStartWnd, ConsoleWndProc);
    ConsoleWndVerbosityApplyIncDec(0);
    ShowWindow(hConsoleWnd, SW_HIDE);
    UpdateWindow(hConsoleWnd);
    CheckDlgButton(hConsoleWnd, IDC_CHECKBOX_VALID, 1);

    /* Scrollbar */
    hMainWndScrollbarProgressWnd = GetDlgItem(hMainWnd, IDC_SCROLLBAR_PROGRESS);
    hMainWndScrollbarVolumeWnd = GetDlgItem(hMainWnd, IDC_SCROLLBAR_VOLUME);
    EnableScrollBar(hMainWndScrollbarVolumeWnd, SB_CTL,ESB_ENABLE_BOTH);
    SetScrollRange(hMainWndScrollbarVolumeWnd, SB_CTL,
		   0, W32G_VOLUME_MAX, TRUE);
    SetScrollPos(hMainWndScrollbarVolumeWnd, SB_CTL,
		 W32G_VOLUME_MAX - amplification, TRUE);

    /* TiMidity Setting */
    InitSettingWnd(hStartWnd);

    update_subwindow(-1);

    TmInitColor();
    w32g_init_panel(hMainWnd);	/* Panel */
    w32g_init_canvas(hMainWnd);	/* Canvas */

    w32g_wait_for_init = 0;
    while(GetMessage(&msg,NULL,0,0))
    {
	TranslateMessage(&msg);
	DispatchMessage(&msg);
    }
    w32g_send_rc(RC_QUIT, 0);
    ExitThread(0);
}

void w32g_close(void)
{
    TerminateThread(hGUIThread, 0);
    CloseHandle(w32g_lock_sem);
    CloseHandle(w32g_empty_sem);
}

int w32g_open(void)
{
    SaveSettingTiMidity(st_current);

    w32g_lock_sem = CreateSemaphore(NULL, 1, 1, "TiMidity Mutex Lock");
    w32g_empty_sem = CreateSemaphore(NULL, 0, 8, "TiMidity Empty Lock");

    hPlayerThread = GetCurrentThread();
    w32g_wait_for_init = 1;
    hGUIThread = CreateThread(NULL, 0,
			       (LPTHREAD_START_ROUTINE)GUIThread,
			       NULL, 0, &dwGUIThreadID);
    while(w32g_wait_for_init)
    {
	Sleep(0);
	VOLATILE_TOUCH(w32g_wait_for_init);
    }
    return 0;
}

static void InitToolbar(HWND hwnd)
{
    TBADDBITMAP MainTbab;
    TBADDBITMAP SubWndTbab;

    static TBBUTTON MainTbb[] =
    {
        {4, IDM_STOP,	TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0},
	{3, IDM_PAUSE,	TBSTATE_ENABLED, TBSTYLE_CHECK,  0, 0},
	{0, IDM_PREV,	TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0},
	{1, IDM_BACKWARD,TBSTATE_ENABLED,TBSTYLE_BUTTON, 0, 0},
	{2, IDM_PLAY,	TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0},
	{5, IDM_FOREWARD,TBSTATE_ENABLED,TBSTYLE_BUTTON, 0, 0},
	{6, IDM_NEXT,	TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0}
    };
    static TBBUTTON SubWndTbb[] = {
	{3, IDM_CONSOLE,TBSTATE_ENABLED, TBSTYLE_CHECK, 0, 0},
	{1, IDM_LIST,	TBSTATE_ENABLED, TBSTYLE_CHECK, 0, 0},
	{2, IDM_TRACER,	TBSTATE_ENABLED, TBSTYLE_CHECK, 0, 0},
	{0, IDM_DOC,	TBSTATE_ENABLED, TBSTYLE_CHECK, 0, 0},
	{4, IDM_WRD,	TBSTATE_ENABLED, TBSTYLE_CHECK, 0, 0},
    };

    SendDlgItemMessage(hwnd, IDC_TOOLBARWINDOW_MAIN,
		       TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
    SendDlgItemMessage(hwnd, IDC_TOOLBARWINDOW_MAIN,
		       TB_SETBUTTONSIZE, (WPARAM)0, (LPARAM)MAKELONG(16,16));
    MainTbab.hInst = hInst;
    MainTbab.nID = (int)IDB_BITMAP_MAIN_BUTTON;
    SendDlgItemMessage(hwnd, IDC_TOOLBARWINDOW_MAIN,
		       TB_ADDBITMAP, 7, (LPARAM)&MainTbab);
    SendDlgItemMessage(hwnd, IDC_TOOLBARWINDOW_MAIN,
		       TB_ADDBUTTONS, (WPARAM)7, (LPARAM)MainTbb);
    SendDlgItemMessage(hwnd, IDC_TOOLBARWINDOW_MAIN,
		       TB_AUTOSIZE, 0, 0);


    SendDlgItemMessage(hwnd, IDC_TOOLBARWINDOW_SUBWND,
		       TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
    SendDlgItemMessage(hwnd, IDC_TOOLBARWINDOW_SUBWND,
		       TB_SETBUTTONSIZE, (WPARAM)0, (LPARAM)MAKELONG(16,16));
    SubWndTbab.hInst = hInst;
    SubWndTbab.nID = (int)IDB_BITMAP_SUBWND_BUTTON;
    SendDlgItemMessage(hwnd, IDC_TOOLBARWINDOW_SUBWND,
		       TB_ADDBITMAP, 5, (LPARAM)&SubWndTbab);
    SendDlgItemMessage(hwnd, IDC_TOOLBARWINDOW_SUBWND,
		       TB_ADDBUTTONS, (WPARAM)5, (LPARAM)SubWndTbb);
    SendDlgItemMessage(hwnd, IDC_TOOLBARWINDOW_SUBWND,
		       TB_AUTOSIZE, 0, 0);
}

static int ID2SubWindowIdx(int id)
{
    int i;

    for(i = 0; subwindow[i].id; i++)
	if(subwindow[i].id == id)
	    return i;
    return -1;
}

static HWND ID2SubWindow(int id)
{
    int i;

    i = ID2SubWindowIdx(id);
    if(i == -1)
	return 0;
    return *subwindow[i].hwnd;
}

static void MainWndUpdateButton(int id)
{
    HWND hwnd = ID2SubWindow(id);

    if(!hwnd)
	return;
    if(IsWindowVisible(hwnd))
  	SendDlgItemMessage(hMainWnd, IDC_TOOLBARWINDOW_SUBWND,
			   TB_CHECKBUTTON, id, (LPARAM)MAKELONG(TRUE, 0));
    else
  	SendDlgItemMessage(hMainWnd, IDC_TOOLBARWINDOW_SUBWND,
			   TB_CHECKBUTTON, id, (LPARAM)MAKELONG(FALSE, 0));
}

static WINBOOL CALLBACK
ListWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
    switch(uMess)
    {
      case WM_INITDIALOG:
	w32g_update_playlist();
	break;
      case WM_COMMAND:
	switch(HIWORD(wParam))
	{
	  case IDCLOSE:
	    ShowWindow(hwnd, SW_HIDE);
	    MainWndUpdateButton(IDM_LIST);
	    break;
	  case LBN_DBLCLK:
	    if(LOWORD(wParam) == IDC_LISTBOX_PLAYLIST)
	    {
		HWND hListBox = (HWND)lParam;
		int num = ListBox_GetCurSel(hListBox);
		if(w32g_goto_playlist(num))
		    w32g_send_rc(RC_LOAD_FILE, 0);
	    }
	    break;
	}
	break;
      case WM_VKEYTOITEM: {
	  UINT vkey = (UINT)LOWORD(wParam);
	  int nCaretPos = (int)HIWORD(wParam);
	  int selected, nfiles;
	  switch(vkey)
	  {
	    case VK_RETURN:
	      if(w32g_goto_playlist(nCaretPos))
		  w32g_send_rc(RC_LOAD_FILE, 0);
	      break;
	    case VK_DELETE:
	      w32g_send_rc(RC_EXT_DELETE_PLAYLIST, 0);
	      break;
	    case VK_BACK:
	      w32g_send_rc(RC_EXT_DELETE_PLAYLIST, -1);
	      break;
	  }
	  return -1;
        }
        break;
      case WM_CLOSE:
	ShowWindow(hListWnd, SW_HIDE);
	MainWndUpdateButton(IDM_LIST);
	break;
      case WM_DROPFILES:
	SendMessage(hMainWnd, WM_DROPFILES, wParam, lParam);
	break;
      case WM_SIZE:
	if(wParam == SIZE_RESTORED)
	{
	    HWND hListBox = GetDlgItem(hListWnd, IDC_LISTBOX_PLAYLIST);
	    if(!hListBox)
		break;
	    MoveWindow(hListBox, 0, 0, LOWORD(lParam), HIWORD(lParam), FALSE);
	    ShowWindow(hListWnd, SW_HIDE);
	    ShowWindow(hListWnd, SW_SHOW);
	}
	break;
    }
    return 0;
}

static void ClearEditCtlWnd(HWND hwnd)
{
    char pszVoid[]="";
    if(!IsWindow(hwnd))
	return;
    Edit_SetText(hwnd, pszVoid);
}

static BOOL CALLBACK
ConsoleWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
    HWND w;

    switch(uMess)
    {
      case WM_COMMAND:
    	switch(LOWORD(wParam))
	{
	  case IDCLOSE:
	    ShowWindow(hwnd, SW_HIDE);
	    MainWndUpdateButton(IDM_CONSOLE);
	    break;
	  case IDCLEAR:
	    if((w = GetDlgItem(hConsoleWnd, IDC_EDIT_TERMINAL)) != 0)
		ClearEditCtlWnd(w);
	    break;
#if 0
	  case IDC_CHECKBOX_VALID:
	    ConsoleWndValidApply();
	    break;
	  case IDC_BUTTON_VERBOSITY:
	    ConsoleWndVerbosityApply();
	    break;
#endif
	  case IDC_BUTTON_INC:
	    ConsoleWndVerbosityApplyIncDec(1);
	    break;
	  case IDC_BUTTON_DEC:
	    ConsoleWndVerbosityApplyIncDec(-1);
	    break;
	}
	break;
      case WM_CLOSE:
	ShowWindow(hwnd, SW_HIDE);
	MainWndUpdateButton(IDM_CONSOLE);
	break;
      case WM_SETFOCUS:
    	HideCaret(hwnd);
	break;
      case WM_KILLFOCUS:
    	ShowCaret(hwnd);
	break;
    }
    return 0;
}

void ConsoleWndVerbosityApplyIncDec(int num)
{
    char buff[32];
    HWND hwnd;

    hwnd = GetDlgItem(hConsoleWnd, IDC_EDIT_VERBOSITY);
    if(!hwnd)
	return;
    ctl->verbosity += num;
    sprintf(buff, "%d", ctl->verbosity);
    SetWindowText(hwnd, buff);
}

void w32g_ctle_play_start(int sec)
{
    if(sec > 0)
    {
	SetScrollRange(hMainWndScrollbarProgressWnd, SB_CTL, 0, sec, TRUE);
	MainWndScrollbarProgressUpdate(0);
    }
    else
	MainWndScrollbarProgressUpdate(-1);
}

void MainWndScrollbarProgressUpdate(int sec)
{
    static int lastsec = -1, enabled = 0;

    if(sec == lastsec)
	return;

    if(sec == -1)
    {
  	EnableWindow(hMainWndScrollbarProgressWnd, FALSE);
	enabled = 0;
	progress_jump = -1;
    }
    else
    {
	if(!enabled)
	{
	    EnableWindow(hMainWndScrollbarProgressWnd, TRUE);
	    enabled = 1;
	}
	if(progress_jump == -1)
	    SetScrollPos(hMainWndScrollbarProgressWnd, SB_CTL, sec, TRUE);
    }
    lastsec = sec;
}

// *****************************************************************************
// TiMidity settings

static BOOL CALLBACK SettingWndProc(HWND hwnd, UINT uMess,
				    WPARAM wParam, LPARAM lParam);
static HWND hComboOutput, hOutputEdit, hOutputRefBtn;


#define MAX_PLAY_MODE_LIST 8
extern PlayMode w32_play_mode, wave_play_mode, raw_play_mode, au_play_mode,
	aiff_play_mode;
static struct
{
    PlayMode *play_mode;
    int32 orig_encoding;
} w32g_play_mode_list[] =
{
{&w32_play_mode,0},
{&wave_play_mode,0},
{&au_play_mode,0},
{&aiff_play_mode,0},
{&raw_play_mode,0},
{NULL,0}
};

static void InitSettingWnd(HWND hParentWnd)
{
    int i, selected, output_enabled;

    hSettingWnd = CreateDialog(hInst, MAKEINTRESOURCE(IDD_DIALOG_SETTING),
			       hParentWnd, SettingWndProc);
    ShowWindow(hSettingWnd,SW_HIDE);
    UpdateWindow(hSettingWnd);

    hComboOutput = GetDlgItem(hSettingWnd, IDC_COMBO_OUTPUT_MODE);
    hOutputEdit = GetDlgItem(hSettingWnd, IDC_EDIT_OUTPUT_FILE);
    hOutputRefBtn = GetDlgItem(hSettingWnd, IDC_BUTTON_REF);

    selected = 0;
    for(i = 0; w32g_play_mode_list[i].play_mode != NULL; i++)
    {
	w32g_play_mode_list[i].orig_encoding =
	    w32g_play_mode_list[i].play_mode->encoding;
	ComboBox_InsertString(hComboOutput, i,
			      w32g_play_mode_list[i].play_mode->id_name);
	if(w32g_play_mode_list[i].play_mode->id_character ==
	   play_mode->id_character)
	    selected = i;
    }
    ComboBox_SetCurSel(hComboOutput, selected);
    SetDlgItemText(hSettingWnd, IDC_EDIT_OUTPUT_FILE,
		   play_mode->name ? play_mode->name : "");
    output_enabled = w32g_play_mode_list[selected].play_mode->id_character != 'd';
    EnableWindow(hOutputEdit, output_enabled);
    EnableWindow(hOutputRefBtn, output_enabled);
}

static void OpenSettingWnd(HWND hParentWnd)
{
    ShowWindow(hSettingWnd,SW_SHOW);
    UpdateWindow(hSettingWnd);
    SettingWndSetup(st_current);
}

#define IS_CHECKED(hwnd, id) (SendDlgItemMessage(hwnd, id, BM_GETCHECK, 0, 0) == BST_CHECKED)
#define SET_CHECKED(hwnd, id, test) \
    if(test) \
	SendDlgItemMessage(hwnd, id, BM_SETCHECK, BST_CHECKED, 0); \
    else \
	SendDlgItemMessage(hwnd, id, BM_SETCHECK, BST_UNCHECKED, 0)

static void SettingWndSetup(SETTING_TIMIDITY *st)
{
    char numstr[16];
    int flag[4];
    int i;

    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_CHORUS, st->opt_chorus_control);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_REVERB, st->opt_reverb_control);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_DELAY_EFFECT, st->effect_lr_mode != -1);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_FREE_INST,
		st->free_instruments_afterwards);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_ANTIALIAS, st->antialiasing_allowed);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_MODWHEEL, st->opt_modulation_wheel);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_PORTAMENT, st->opt_portamento);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_NRPNVIB, st->opt_nrpn_vibrato);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_CHPRESS, st->opt_channel_pressure);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_OVOICE, st->opt_overlap_voice_allow);
    SET_CHECKED(hSettingWnd, IDC_CHECKBOX_LOADINST_PLAYING,
		st->opt_realtime_playing);

    /* Mono/Stereo */
    memset(flag, 0, sizeof(flag));
    if(strchr(st->opt_playmode + 1, 'M'))
	flag[0] = 1;
    else
	flag[1] = 1;
    SET_CHECKED(hSettingWnd, IDC_RADIO_MONO, flag[0]);
    SET_CHECKED(hSettingWnd, IDC_RADIO_STEREO, flag[1]);

    /* 16/8/U/A */
    memset(flag, 0, sizeof(flag));
    if(strchr(st->opt_playmode + 1, '1'))
	flag[0] = 1;
    else if(strchr(st->opt_playmode + 1, 'U'))
	flag[2] = 1;
    else if(strchr(st->opt_playmode + 1, 'A'))
	flag[3] = 1;
    else
	flag[1] = 1;
    SET_CHECKED(hSettingWnd, IDC_RADIO_16BITS, flag[0]);
    SET_CHECKED(hSettingWnd, IDC_RADIO_8BITS, flag[1]);
    SET_CHECKED(hSettingWnd, IDC_RADIO_ULAW, flag[2]);
    SET_CHECKED(hSettingWnd, IDC_RADIO_ALAW, flag[3]);

    /* Output Mode */
    for(i = 0; w32g_play_mode_list[i].play_mode != NULL; i++)
	if(st->opt_playmode[0] == w32g_play_mode_list[i].play_mode->id_character)
	    break;
    if(w32g_play_mode_list[i].play_mode == NULL)
	i = 0;
    ComboBox_SetCurSel(hComboOutput, i);
    i = (w32g_play_mode_list[i].play_mode->id_character != 'd');
    EnableWindow(hOutputEdit, i);
    EnableWindow(hOutputRefBtn, i);

    /* sample rate */
    sprintf(numstr, "%d", st->output_rate);
    SetDlgItemText(hSettingWnd, IDC_EDIT_SAMPLE_RATE, numstr);

    /* number of voices */
    sprintf(numstr, "%d", st->voices);
    SetDlgItemText(hSettingWnd, IDC_EDIT_VOICES, numstr);

    /* Noise sharping */
    sprintf(numstr, "%d", st->noise_sharp_type);
    SetDlgItemText(hSettingWnd, IDC_EDIT_NOISESHARPING, numstr);
}

void SettingWndApply(void)
{
    char buff[BUFSIZ];
    extern void timidity_init_aq_buff(void);
    int i, selected;

    st_current->opt_chorus_control =
	IS_CHECKED(hSettingWnd, IDC_CHECKBOX_CHORUS);
    st_current->opt_reverb_control =
	IS_CHECKED(hSettingWnd, IDC_CHECKBOX_REVERB);
    if(IS_CHECKED(hSettingWnd, IDC_CHECKBOX_DELAY_EFFECT))
	st_current->effect_lr_mode = 2;
    else
	st_current->effect_lr_mode = -1;
    st_current->free_instruments_afterwards =
	IS_CHECKED(hSettingWnd, IDC_CHECKBOX_FREE_INST);
    st_current->antialiasing_allowed =
	IS_CHECKED(hSettingWnd, IDC_CHECKBOX_ANTIALIAS);
    st_current->opt_modulation_wheel =
	IS_CHECKED(hSettingWnd, IDC_CHECKBOX_MODWHEEL);
    st_current->opt_portamento =
	IS_CHECKED(hSettingWnd, IDC_CHECKBOX_PORTAMENT);
    st_current->opt_nrpn_vibrato =
	IS_CHECKED(hSettingWnd, IDC_CHECKBOX_NRPNVIB);
    st_current->opt_channel_pressure =
	IS_CHECKED(hSettingWnd, IDC_CHECKBOX_CHPRESS);
    st_current->opt_overlap_voice_allow =
	IS_CHECKED(hSettingWnd, IDC_CHECKBOX_OVOICE);
    st_current->opt_realtime_playing =
	IS_CHECKED(hSettingWnd, IDC_CHECKBOX_LOADINST_PLAYING);
    GetDlgItemText(hSettingWnd, IDC_EDIT_NOISESHARPING, buff, sizeof(buff));
    st_current->noise_sharp_type = atoi(buff);
    GetDlgItemText(hSettingWnd, IDC_EDIT_SAMPLE_RATE, buff, sizeof(buff));
    st_current->output_rate = atoi(buff);
    GetDlgItemText(hSettingWnd, IDC_EDIT_VOICES, buff, sizeof(buff));
    st_current->voices = atoi(buff);

    selected = ComboBox_GetCurSel(hComboOutput);
    i = 0;
    st_current->opt_playmode[i++] =
	w32g_play_mode_list[selected].play_mode->id_character;
    if(IS_CHECKED(hSettingWnd, IDC_RADIO_16BITS))
	st_current->opt_playmode[i++] = '1';
    else if(IS_CHECKED(hSettingWnd, IDC_RADIO_8BITS))
	st_current->opt_playmode[i++] = '8';
    else if(IS_CHECKED(hSettingWnd, IDC_RADIO_ULAW))
	st_current->opt_playmode[i++] = 'U';
    else if(IS_CHECKED(hSettingWnd, IDC_RADIO_ALAW))
	st_current->opt_playmode[i++] = 'A';
    if(IS_CHECKED(hSettingWnd, IDC_RADIO_MONO))
	st_current->opt_playmode[i++] = 'M';
    else
	st_current->opt_playmode[i++] = 'S';
    if(w32g_play_mode_list[selected].orig_encoding & PE_SIGNED)
	st_current->opt_playmode[i++] = 's';
    else
	st_current->opt_playmode[i++] = 'u';
    if(w32g_play_mode_list[selected].orig_encoding & PE_BYTESWAP)
	st_current->opt_playmode[i++] = 'x';
    st_current->opt_playmode[i++] = '\0';

    if(st_current->opt_playmode[0] == 'd')
	st_current->OutputName[0] = '\0';
    else
    {
	GetDlgItemText(hSettingWnd, IDC_EDIT_OUTPUT_FILE, buff, sizeof(buff));
	strncpy(st_current->OutputName, buff, sizeof(st_current->OutputName));
    }

    free_instruments(1);
    play_mode->close_output();

    ApplySettingTiMidity(st_current);
}

static void DlgSelectOutputFile(void)
{
    OPENFILENAME ofn;
    static char pFileName[16536];
    char *dir;
    static char *filter = NULL;
//	"Midi file\0*.mid;*.smf;*.rcp;*.r36;*.g18;*.g36\0"
//	"Archive file\0*.lzh;*.zip;*.gz;*.tgz\0"
//	"All files\0*.*\0"
//	"\0\0";
    if(w32g_lock_open_file)
	return;

    memset(pFileName, 0, sizeof(pFileName));
    memset(&ofn, 0, sizeof(OPENFILENAME));
    if(MidiFileOpenDir[0])
	dir = MidiFileOpenDir;
    else
	dir = NULL;

    ofn.lStructSize	= sizeof(OPENFILENAME);
    ofn.hwndOwner	= 0;
    ofn.hInstance	= hInst;
    ofn.lpstrFilter	= filter;
    ofn.lpstrCustomFilter= 0;
    ofn.nMaxCustFilter	= 1;
    ofn.nFilterIndex	= 1 ;
    ofn.lpstrFile	= pFileName;
    ofn.nMaxFile	= sizeof(pFileName);
    ofn.lpstrFileTitle	= 0;
    ofn.nMaxFileTitle 	= 0;
    ofn.lpstrInitialDir	= dir;
    ofn.lpstrTitle	= 0;
    ofn.Flags		= OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_HIDEREADONLY;
    ofn.lpstrDefExt	= 0;
    ofn.lCustData	= 0;
    ofn.lpfnHook	= 0;
    ofn.lpTemplateName	= 0;

    if(!GetSaveFileName(&ofn))
	return;
    SetDlgItemText(hSettingWnd, IDC_EDIT_OUTPUT_FILE, pFileName);
}

static BOOL CALLBACK SettingWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
    static int setting_warning_displayed = 0;

    switch(uMess)
    {
      case WM_SHOWWINDOW:
	if(wParam)
	{
	    if(w32g_play_active &&
	       !setting_warning_displayed)
		w32g_msg_box("Don't change parameter while playing",
			     "Warning", MB_OK);
	    setting_warning_displayed = 1;
	    SettingWndSetup(st_current);
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
	    w32g_send_rc(RC_EXT_APPLY_SETTING, 0);
	    ShowWindow(hSettingWnd, SW_HIDE);
	    break;
	  case IDDEFAULT:
	    SettingWndSetup(st_default);
	    break;
	  case IDC_COMBO_OUTPUT_MODE: {
	      int selected, output_enabled;
	      selected = ComboBox_GetCurSel(hComboOutput);
	      output_enabled = w32g_play_mode_list[selected].play_mode->id_character != 'd';
	    EnableWindow(hOutputEdit, output_enabled);
	    EnableWindow(hOutputRefBtn, output_enabled);
	    }
	    break;
	  case IDC_BUTTON_REF:
	    DlgSelectOutputFile();
	    break;
	}
	break;

      case WM_CLOSE:
	ShowWindow(hSettingWnd, SW_HIDE);
	break;
    }
    return 0;
}

int w32g_msg_box(char *message, char *title, int type)
{
    return MessageBox(hMainWnd, message, title, type);
}
