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

    w32g_subwin.c: Written by Daisuke Aoki <dai@y7.net>
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
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

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

#include "w32g.h"
#include <shlobj.h>
#include <commctrl.h>
#include <windowsx.h>
#include "w32g_res.h"
#include "w32g_utl.h"
#include "w32g_pref.h"
#include "w32g_subwin.h"
#include "w32g_ut2.h"

#ifdef TIMW32G_USE_NEW_CONSOLE
#include "w32g_new_console.h"
#endif

#if defined(__CYGWIN32__) || defined(__MINGW32__)
#ifndef TPM_TOPALIGN
#define TPM_TOPALIGN	0x0000L
#endif
#endif

extern void MainWndToggleConsoleButton(void);
extern void MainWndUpdateConsoleButton(void);
extern void MainWndToggleTracerButton(void);
extern void MainWndUpdateTracerButton(void);
extern void MainWndToggleListButton(void);
extern void MainWndUpdateListButton(void);
extern void MainWndToggleDocButton(void);
extern void MainWndUpdateDocButton(void);
extern void MainWndToggleWrdButton(void);
extern void MainWndUpdateWrdButton(void);
extern void MainWndToggleSoundSpecButton(void);
extern void MainWndUpdateSoundSpecButton(void);
extern void ShowSubWindow(HWND hwnd,int showflag);
extern void ToggleSubWindow(HWND hwnd);

extern void VprintfEditCtlWnd(HWND hwnd, char *fmt, va_list argList);
extern void PrintfEditCtlWnd(HWND hwnd, char *fmt, ...);
extern void PutsEditCtlWnd(HWND hwnd, char *str);
extern void ClearEditCtlWnd(HWND hwnd);

extern char *nkf_convert(char *si,char *so,int maxsize,char *in_mode,char *out_mode);

void InitListSearchWnd(HWND hParentWnd);
void ShowListSearch(void);
void HideListSearch(void);

// ***************************************************************************
//
// Console Window
//
// ***************************************************************************

// ---------------------------------------------------------------------------
// variables
static int ConsoleWndMaxSize = 64 * 1024;
static HFONT hFontConsoleWnd = NULL;
CONSOLEWNDINFO ConsoleWndInfo;

// ---------------------------------------------------------------------------
// prototypes of functions
static LRESULT CALLBACK ConsoleWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static void ConsoleWndAllUpdate(void);
static void ConsoleWndVerbosityUpdate(void);
static void ConsoleWndVerbosityApply(void);
static void ConsoleWndValidUpdate(void);
static void ConsoleWndValidApply(void);
static void ConsoleWndVerbosityApplySet(int num);
static int ConsoleWndInfoReset(HWND hwnd);
static int ConsoleWndInfoApply(void);

// ---------------------------------------------------------------------------
// Global Functions

// Initialization
void InitConsoleWnd(HWND hParentWnd)
{
	HICON hIcon;
	if (hConsoleWnd != NULL) {
		DestroyWindow(hConsoleWnd);
		hConsoleWnd = NULL;
	}
///r
	ConsoleWndInfoReset(hConsoleWnd);
	INILoadConsoleWnd();
	
#ifdef TIMW32G_USE_NEW_CONSOLE
	InitializeNewConsole();
#endif
	
#ifdef TIMW32G_USE_NEW_CONSOLE
	InitializeNewConsole();
#endif

#ifdef TIMW32G_USE_NEW_CONSOLE
	InitializeNewConsole();
#endif

	switch(PlayerLanguage){
  	case LANGUAGE_ENGLISH:
		hConsoleWnd = CreateDialog
  			(hInst,MAKEINTRESOURCE(IDD_DIALOG_CONSOLE_EN),hParentWnd,ConsoleWndProc);
		break;
	case LANGUAGE_JAPANESE:
 	default:
		hConsoleWnd = CreateDialog
  			(hInst,MAKEINTRESOURCE(IDD_DIALOG_CONSOLE),hParentWnd,ConsoleWndProc);
	break;
	}
	hIcon = LoadImage(hInst, MAKEINTRESOURCE(IDI_ICON_TIMIDITY), IMAGE_ICON, 16, 16, 0);
	if (hIcon!=NULL) SendMessage(hConsoleWnd,WM_SETICON,FALSE,(LPARAM)hIcon);
	ConsoleWndInfoReset(hConsoleWnd);
	ShowWindow(hConsoleWnd,ConsoleWndStartFlag ? SW_SHOW : SW_HIDE);
///r
	INILoadConsoleWnd();
	ConsoleWndInfoApply();
	UpdateWindow(hConsoleWnd);
	ConsoleWndVerbosityApply();
	CheckDlgButton(hConsoleWnd, IDC_CHECKBOX_VALID, ConsoleWndFlag);
#ifndef TIMW32G_USE_NEW_CONSOLE
	Edit_LimitText(GetDlgItem(hConsoleWnd,IDC_EDIT), ConsoleWndMaxSize);
#endif
}

// Window Procedure
static LRESULT CALLBACK
ConsoleWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess){
	case WM_INITDIALOG:
		PutsConsoleWnd("Console Window\n");
		ConsoleWndAllUpdate();
		SetWindowPosSize(GetDesktopWindow(),hwnd,ConsoleWndInfo.PosX, ConsoleWndInfo.PosY );
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
			{
				int n = (int)GetDlgItemInt(hwnd, IDC_EDIT_VERBOSITY, NULL, TRUE);
				n++;
				ConsoleWndVerbosityApplySet(n);
			}
			break;
		case IDC_BUTTON_DEC:
			{
				int n = (int)GetDlgItemInt(hwnd, IDC_EDIT_VERBOSITY, NULL, TRUE);
				n--;
				ConsoleWndVerbosityApplySet(n);
			}
			break;
		default:
			break;
		}
		switch (HIWORD(wParam)) {
		case EN_ERRSPACE:
			ClearConsoleWnd();
			PutsConsoleWnd("### EN_ERRSPACE -> Clear! ###\n");
			break;
		default:
			break;
		}
		break;
///r
	case WM_GETMINMAXINFO:
		{
			LPMINMAXINFO lpmmi = (LPMINMAXINFO) lParam;
			lpmmi->ptMinTrackSize.x = max(380, lpmmi->ptMinTrackSize.x);
			lpmmi->ptMinTrackSize.y = max(150, lpmmi->ptMinTrackSize.y);
		}
		return 0;
	case WM_SIZE:		
		ConsoleWndAllUpdate();
		switch(wParam){
		case SIZE_MAXIMIZED:
		case SIZE_RESTORED:
			{	// くそめんどーー
			int x,y,cx,cy;
			int max = 0;
			RECT rcParent;
			RECT rcBUTTON_VERBOSITY, rcEDIT_VERBOSITY, rcBUTTON_DEC, rcBUTTON_INC, rcCHECKBOX_VALID, rcCLEAR, rcEDIT;
			HWND hwndBUTTON_VERBOSITY, hwndEDIT_VERBOSITY, hwndBUTTON_DEC, hwndBUTTON_INC, hwndCHECKBOX_VALID, hwndCLEAR, hwndEDIT;
			hwndEDIT = GetDlgItem(hwnd,IDC_EDIT);
			hwndBUTTON_VERBOSITY = GetDlgItem(hwnd,IDC_BUTTON_VERBOSITY);
			hwndEDIT_VERBOSITY = GetDlgItem(hwnd,IDC_EDIT_VERBOSITY);
			hwndBUTTON_DEC = GetDlgItem(hwnd,IDC_BUTTON_DEC);
			hwndBUTTON_INC = GetDlgItem(hwnd,IDC_BUTTON_INC);
			hwndCHECKBOX_VALID = GetDlgItem(hwnd,IDC_CHECKBOX_VALID);
			hwndCLEAR = GetDlgItem(hwnd,IDCLEAR);
			GetWindowRect(hwndEDIT,&rcEDIT); // x0y0 基準
			GetWindowRect(hwndBUTTON_VERBOSITY,&rcBUTTON_VERBOSITY);
			GetWindowRect(hwndEDIT_VERBOSITY,&rcEDIT_VERBOSITY);
			GetWindowRect(hwndBUTTON_DEC,&rcBUTTON_DEC);
			GetWindowRect(hwndBUTTON_INC,&rcBUTTON_INC);
			GetWindowRect(hwndCHECKBOX_VALID,&rcCHECKBOX_VALID);
			GetWindowRect(hwndCLEAR,&rcCLEAR);					
			GetClientRect(hwnd,&rcParent);
			// IDC_BUTTON_VERBOSITY
			cx = rcBUTTON_VERBOSITY.right-rcBUTTON_VERBOSITY.left;
			cy = rcBUTTON_VERBOSITY.bottom-rcBUTTON_VERBOSITY.top;
			x = rcBUTTON_VERBOSITY.left - rcEDIT.left;
			y = rcParent.bottom - cy - 4;
			MoveWindow(hwndBUTTON_VERBOSITY,x,y,cx,cy,TRUE);
			if(cy>max) max = cy;
			// IDC_EDIT_VERBOSITY
			cx = rcEDIT_VERBOSITY.right-rcEDIT_VERBOSITY.left;
			cy = rcEDIT_VERBOSITY.bottom-rcEDIT_VERBOSITY.top;
			x = rcEDIT_VERBOSITY.left - rcEDIT.left;
			y = rcParent.bottom - cy - 4;
			MoveWindow(hwndEDIT_VERBOSITY,x,y,cx,cy,TRUE);
			if(cy>max) max = cy;
			// IDC_BUTTON_DEC
			cx = rcBUTTON_DEC.right-rcBUTTON_DEC.left;
			cy = rcBUTTON_DEC.bottom-rcBUTTON_DEC.top;
			x = rcBUTTON_DEC.left - rcEDIT.left;
			y = rcParent.bottom - cy - 4;
			MoveWindow(hwndBUTTON_DEC,x,y,cx,cy,TRUE);
			if(cy>max) max = cy;
			// IDC_BUTTON_INC
			cx = rcBUTTON_INC.right-rcBUTTON_INC.left;
			cy = rcBUTTON_INC.bottom-rcBUTTON_INC.top;
			x = rcBUTTON_INC.left - rcEDIT.left;
			y = rcParent.bottom - cy - 4;
			MoveWindow(hwndBUTTON_INC,x,y,cx,cy,TRUE);
			if(cy>max) max = cy;
			// IDC_CHECKBOX_VALID
			cx = rcCHECKBOX_VALID.right-rcCHECKBOX_VALID.left;
			cy = rcCHECKBOX_VALID.bottom-rcCHECKBOX_VALID.top;
			x = rcCHECKBOX_VALID.left - rcEDIT.left;
			y = rcParent.bottom - cy - 4;
			MoveWindow(hwndCHECKBOX_VALID,x,y,cx,cy,TRUE);
			if(cy>max) max = cy;
			// IDCLEAR
			cx = rcCLEAR.right-rcCLEAR.left;
			cy = rcCLEAR.bottom-rcCLEAR.top;
			x = rcCLEAR.left - rcEDIT.left;
			y = rcParent.bottom - cy - 4;
			MoveWindow(hwndCLEAR,x,y,cx,cy,TRUE);
			if(cy>max) max = cy;
			// IDC_EDIT
			cx = rcParent.right - rcParent.left;
			cy = rcParent.bottom - rcParent.top - max - 8;
			x  = rcParent.left;
			y = rcParent.top;
			MoveWindow(hwndEDIT,x,y,cx,cy,TRUE);
			// 
			InvalidateRect(hwnd,&rcParent,FALSE);
			UpdateWindow(hwnd);
			GetWindowRect(hwnd,&rcParent);
			ConsoleWndInfo.Width = rcParent.right - rcParent.left;
			ConsoleWndInfo.Height = rcParent.bottom - rcParent.top;
			break;
			}
		case SIZE_MINIMIZED:
		case SIZE_MAXHIDE:
		case SIZE_MAXSHOW:
		default:
			break;
		}
		break;
	case WM_MOVE:
//		ConsoleWndInfo.PosX = (int) LOWORD(lParam);
//		ConsoleWndInfo.PosY = (int) HIWORD(lParam);
		{
			RECT rc;
			GetWindowRect(hwnd,&rc);
			ConsoleWndInfo.PosX = rc.left;
			ConsoleWndInfo.PosY = rc.top;
		}
		break;
	// See PreDispatchMessage() in w32g2_main.c
	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
	{
		int nVirtKey = (int)wParam;
		switch(nVirtKey){
			case VK_ESCAPE:
				SendMessage(hwnd,WM_CLOSE,0,0);
				break;
		}
	}
		break;
	case WM_DROPFILES:
#ifdef EXT_CONTROL_MAIN_THREAD
		w32g_ext_control_main_thread(RC_EXT_DROP, (ptr_size_t)wParam);
#else
		w32g_send_rc(RC_EXT_DROP, (ptr_size_t)wParam);
#endif
		return FALSE;
	case WM_DESTROY:		
		{
		RECT rc;
		GetWindowRect(hwnd,&rc);
		ConsoleWndInfo.Width = rc.right - rc.left;
		ConsoleWndInfo.Height = rc.bottom - rc.top;
		}
		INISaveConsoleWnd();
		break;
	case WM_CLOSE:
		ShowSubWindow(hConsoleWnd,0);
//		ShowWindow(hConsoleWnd, SW_HIDE);
		MainWndUpdateConsoleButton();
		break;
#ifdef TIMW32G_USE_NEW_CONSOLE
	case WM_ACTIVATE:
		if (LOWORD(wParam) != WA_INACTIVE) {
			SetFocus(GetDlgItem(hConsoleWnd, IDC_EDIT));
			return TRUE;
		}
		break;
#else
	case WM_SETFOCUS:
		HideCaret(hwnd);
		break;
	case WM_KILLFOCUS:
		ShowCaret(hwnd);
		break;
#endif
	default:
		return FALSE;
	}
	return FALSE;
}

// puts()
void PutsConsoleWnd(const char *str)
{
	HWND hwnd;
	if(!IsWindow(hConsoleWnd) || !ConsoleWndFlag)
		return;
	hwnd = GetDlgItem(hConsoleWnd,IDC_EDIT);
#ifdef TIMW32G_USE_NEW_CONSOLE
	NewConsoleWrite(hwnd, str);
#else
	PutsEditCtlWnd(hwnd,str);
#endif
}

// printf()
void PrintfConsoleWnd(char *fmt, ...)
{
	HWND hwnd;
	va_list ap;
	if(!IsWindow(hConsoleWnd) || !ConsoleWndFlag)
		return;
	hwnd = GetDlgItem(hConsoleWnd,IDC_EDIT);
	va_start(ap, fmt);
#ifdef TIMW32G_USE_NEW_CONSOLE
	NewConsoleWriteV(hwnd, fmt, ap);
#else
	VprintfEditCtlWnd(hwnd,fmt,ap);
#endif
	va_end(ap);
}

// Clear
void ClearConsoleWnd(void)
{
	HWND hwnd;
	if(!IsWindow(hConsoleWnd))
		return;
	hwnd = GetDlgItem(hConsoleWnd,IDC_EDIT);
#ifdef TIMW32G_USE_NEW_CONSOLE
	NewConsoleClear(hwnd);
#else
	ClearEditCtlWnd(hwnd);
#endif
}

// ---------------------------------------------------------------------------
// Static Functions

static void ConsoleWndAllUpdate(void)
{
	ConsoleWndVerbosityUpdate();
	ConsoleWndValidUpdate();
	Edit_LimitText(GetDlgItem(hConsoleWnd,IDC_EDIT_VERBOSITY),3);
#ifndef TIMW32G_USE_NEW_CONSOLE
	Edit_LimitText(GetDlgItem(hConsoleWnd,IDC_EDIT),ConsoleWndMaxSize);
#endif
}

static void ConsoleWndValidUpdate(void)
{
	if(ConsoleWndFlag)
		CheckDlgButton(hConsoleWnd, IDC_CHECKBOX_VALID, 1);
	else
		CheckDlgButton(hConsoleWnd, IDC_CHECKBOX_VALID, 0);
}

static void ConsoleWndValidApply(void)
{
	if(IsDlgButtonChecked(hConsoleWnd,IDC_CHECKBOX_VALID))
		ConsoleWndFlag = 1;
	else
		ConsoleWndFlag = 0;
}

static void ConsoleWndVerbosityUpdate(void)
{
	SetDlgItemInt(hConsoleWnd,IDC_EDIT_VERBOSITY,(UINT)ctl->verbosity, TRUE);
}

static void ConsoleWndVerbosityApply(void)
{
	TCHAR buffer[64];
	HWND hwnd;
	hwnd = GetDlgItem(hConsoleWnd,IDC_EDIT_VERBOSITY);
	if(!IsWindow(hConsoleWnd)) return;
	if(Edit_GetText(hwnd,buffer,60)<=0) return;
	ctl->verbosity = _ttoi(buffer);
	ConsoleWndVerbosityUpdate();
}

static void ConsoleWndVerbosityApplySet(int num)
{
	if(!IsWindow(hConsoleWnd)) return;
	ctl->verbosity = num;
	RANGE(ctl->verbosity, -1, 4);
	ConsoleWndVerbosityUpdate();
}

static int ConsoleWndInfoReset(HWND hwnd)
{
	memset(&ConsoleWndInfo,0,sizeof(CONSOLEWNDINFO));
	ConsoleWndInfo.PosX = - 1;
	ConsoleWndInfo.PosY = - 1;
	ConsoleWndInfo.Width = 454;
	ConsoleWndInfo.Height = 337;
	ConsoleWndInfo.hwnd = hwnd;
	return 0;
}

static int ConsoleWndInfoApply(void)
{
	
	RECT rc;

	GetWindowRect(ConsoleWndInfo.hwnd,&rc);
	MoveWindow(ConsoleWndInfo.hwnd,rc.left,rc.top,ConsoleWndInfo.Width,ConsoleWndInfo.Height,TRUE);
	INISaveConsoleWnd();
	return 0;
}



// ****************************************************************************
//
// List Window
//
// ****************************************************************************

// ---------------------------------------------------------------------------
// Macros
#define IDM_LISTWND_REMOVE		4101
#define IDM_LISTWND_PLAY  		4102
#define IDM_LISTWND_REFINE 		4103
#define IDM_LISTWND_UNIQ 		4104
#define IDM_LISTWND_CLEAR 		4105
#define IDM_LISTWND_CHOOSEFONT	4106
#define IDM_LISTWND_CURRENT     4107
#define IDM_LISTWND_SEARCH      4108
#define IDM_LISTWND_LISTNAME    4109
#define IDM_LISTWND_COPY        4110
#define IDM_LISTWND_CUT         4111
#define IDM_LISTWND_PASTE       4112


// ---------------------------------------------------------------------------
// Variables
LISTWNDINFO ListWndInfo;
HWND hListSearchWnd = NULL;
#ifdef LISTVIEW_PLAYLIST
HIMAGELIST hImageList = NULL;
#endif

// ---------------------------------------------------------------------------
// Prototypes
static LRESULT CALLBACK ListWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static int ListWndInfoReset(HWND hwnd);
static int ListWndInfoApply(void);
static int ListWndSetFontListBox(char *fontName, int fontWidth, int fontHeght);
static int ResetListWnd(void);
static int ClearListWnd(void);
static int UniqListWnd(void);
static int RefineListWnd(void);
static int DelListWnd(int nth);

// ---------------------------------------------------------------------------
// Grobal Functions
void InitListWnd(HWND hParentWnd)
{
	HICON hIcon;
	if (hListWnd != NULL) {
		DestroyWindow(hListWnd);
		hListWnd = NULL;
	}
	ListWndInfoReset(hListWnd);
	INILoadListWnd();
#ifdef LISTVIEW_PLAYLIST
	switch(PlayerLanguage){
	case LANGUAGE_ENGLISH:
		hListWnd = CreateDialog(hInst,MAKEINTRESOURCE(IDD_DIALOG_PLAYLIST_EN),hParentWnd,ListWndProc);
		break;
	case LANGUAGE_JAPANESE:
	default:
		hListWnd = CreateDialog(hInst,MAKEINTRESOURCE(IDD_DIALOG_PLAYLIST),hParentWnd,ListWndProc);
		break;
	}
#else
	switch(PlayerLanguage){
	case LANGUAGE_ENGLISH:
		hListWnd = CreateDialog(hInst,MAKEINTRESOURCE(IDD_DIALOG_SIMPLE_LIST_EN),hParentWnd,ListWndProc);
		break;
	case LANGUAGE_JAPANESE:
	default:
		hListWnd = CreateDialog(hInst,MAKEINTRESOURCE(IDD_DIALOG_SIMPLE_LIST),hParentWnd,ListWndProc);
		break;
	}
#endif
	ListWndInfoReset(hListWnd);
	ListWndInfo.hPopupMenu = CreatePopupMenu();
	switch(PlayerLanguage){
	case LANGUAGE_JAPANESE:
		AppendMenu(ListWndInfo.hPopupMenu,MF_STRING,IDM_LISTWND_PLAY,_T("演奏(&P)"));
		AppendMenu(ListWndInfo.hPopupMenu,MF_STRING,IDC_BUTTON_DOC,_T("ドキュメント(&D)..."));
		AppendMenu(ListWndInfo.hPopupMenu,MF_SEPARATOR,0,0);
		AppendMenu(ListWndInfo.hPopupMenu,MF_STRING,IDM_LISTWND_CURRENT,_T("現在位置(&C)"));
		AppendMenu(ListWndInfo.hPopupMenu,MF_STRING,IDM_LISTWND_SEARCH,_T("検索(&S)..."));
#ifdef LISTVIEW_PLAYLIST
		AppendMenu(ListWndInfo.hPopupMenu,MF_SEPARATOR,0,0);
		AppendMenu(ListWndInfo.hPopupMenu,MF_STRING,IDM_LISTWND_CUT,_T("カット(&X)"));
		AppendMenu(ListWndInfo.hPopupMenu,MF_STRING,IDM_LISTWND_COPY,_T("コピー(&Z)"));
		AppendMenu(ListWndInfo.hPopupMenu,MF_STRING,IDM_LISTWND_PASTE,_T("ペースト/挿入(&A)"));
#endif
		AppendMenu(ListWndInfo.hPopupMenu,MF_SEPARATOR,0,0);
		AppendMenu(ListWndInfo.hPopupMenu,MF_STRING,IDM_LISTWND_REMOVE,_T("削除(&R)"));
		AppendMenu(ListWndInfo.hPopupMenu,MF_SEPARATOR,0,0);
		AppendMenu(ListWndInfo.hPopupMenu,MF_STRING,IDM_LISTWND_CHOOSEFONT,_T("フォントの選択(&H)..."));
		AppendMenu(ListWndInfo.hPopupMenu,MF_STRING,IDM_LISTWND_LISTNAME,_T("リスト名変更(&L)..."));		
		break;
  	case LANGUAGE_ENGLISH:
 	default:
		AppendMenu(ListWndInfo.hPopupMenu,MF_STRING,IDM_LISTWND_PLAY,_T("&Play"));
		AppendMenu(ListWndInfo.hPopupMenu,MF_STRING,IDC_BUTTON_DOC,_T("&Doc..."));
		AppendMenu(ListWndInfo.hPopupMenu,MF_SEPARATOR,0,0);
		AppendMenu(ListWndInfo.hPopupMenu,MF_STRING,IDM_LISTWND_CURRENT,_T("&Current item"));
		AppendMenu(ListWndInfo.hPopupMenu,MF_STRING,IDM_LISTWND_SEARCH,_T("&Search..."));
#ifdef LISTVIEW_PLAYLIST
		AppendMenu(ListWndInfo.hPopupMenu,MF_SEPARATOR,0,0);
		AppendMenu(ListWndInfo.hPopupMenu,MF_STRING,IDM_LISTWND_CUT,_T("Cut(&X)"));
		AppendMenu(ListWndInfo.hPopupMenu,MF_STRING,IDM_LISTWND_COPY,_T("Copy(&Z)"));
		AppendMenu(ListWndInfo.hPopupMenu,MF_STRING,IDM_LISTWND_PASTE,_T("Paste/Insert(&A)"));
#endif
		AppendMenu(ListWndInfo.hPopupMenu,MF_SEPARATOR,0,0);
		AppendMenu(ListWndInfo.hPopupMenu,MF_STRING,IDM_LISTWND_REMOVE,_T("&Remove"));
		AppendMenu(ListWndInfo.hPopupMenu,MF_SEPARATOR,0,0);
		AppendMenu(ListWndInfo.hPopupMenu,MF_STRING,IDM_LISTWND_CHOOSEFONT,_T("C&hoose Font..."));
		AppendMenu(ListWndInfo.hPopupMenu,MF_STRING,IDM_LISTWND_LISTNAME,_T("Rename L&ist..."));
		break;
	}
	hIcon = LoadImage(hInst, MAKEINTRESOURCE(IDI_ICON_TIMIDITY), IMAGE_ICON, 16, 16, 0);
	if (hIcon!=NULL) SendMessage(hListWnd,WM_SETICON,FALSE,(LPARAM)hIcon);

	INILoadListWnd();
	ListWndInfoApply();
	ShowWindow(ListWndInfo.hwnd,ListWndStartFlag ? SW_SHOW : SW_HIDE);
	UpdateWindow(ListWndInfo.hwnd);
	
#ifdef EXT_CONTROL_MAIN_THREAD
	w32g_ext_control_main_thread(RC_EXT_PLAYLIST_CTRL, 0);
	w32g_ext_control_main_thread(RC_EXT_UPDATE_PLAYLIST, 0);
#else
	w32g_send_rc(RC_EXT_PLAYLIST_CTRL, 0);
	w32g_send_rc(RC_EXT_UPDATE_PLAYLIST, 0);
#endif
}

#ifdef LISTVIEW_PLAYLIST
void init_imagelist(HWND hlv)
{
	HICON hIcon = NULL;
	if(hImageList)
		return;
	if(!hImageList)
		hImageList = ImageList_Create(12, 12, ILC_COLOR, 2, 5);
	if(!hImageList)
		return;
	hIcon = (HICON)LoadImage(hInst, MAKEINTRESOURCE(IDI_ICON_PLAYLIST_PLAY), IMAGE_ICON, 12, 12, 0);
	if(!hIcon)
		return;
	ImageList_AddIcon(hImageList, (HICON)hIcon);
	ListView_SetImageList(hlv, hImageList, LVSIL_SMALL);
}

void uninit_imagelist()
{
	// hImageList will be destroyed by the parent list view
	hImageList = NULL;
}
#endif

// ---------------------------------------------------------------------------
// Static Functions

void SetNumListWnd(int cursel, int nfiles);

#define WM_CHOOSEFONT_DIAG	(WM_APP+100)
#define WM_LIST_SEARCH_DIAG	(WM_APP+101)
#define WM_LISTNAME_DIAG	(WM_APP+102)
#define LISTWND_HORIZONTALEXTENT 1600

static void ListWndCreateTabItems(HWND hwnd)
{
    int i;
    HWND hwnd_tab;

    hwnd_tab = GetDlgItem(hwnd, IDC_TAB_PLAYLIST);
    for (i = 0; i < playlist_max; i++) {
        TC_ITEM tci;
        tci.mask = TCIF_TEXT;
        tci.pszText = ListWndInfo.ListName[i];
        tci.cchTextMax = _tcslen(ListWndInfo.ListName[i]);
        SendMessage(hwnd_tab, TCM_INSERTITEM, (WPARAM) i, (LPARAM) &tci);
    }
}

LRESULT CALLBACK
ListNameWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	int num;

	switch (uMess){
	case WM_INITDIALOG:
		SendMessage(hwnd,WM_SETTEXT,0,(LPARAM)_T("ListName"));
		num = w32g_get_playlist_num_ctrl();
		SetDlgItemText(hwnd, IDC_EDIT1, ListWndInfo.ListName[num]);
		SetFocus(GetDlgItem(hwnd,IDC_EDIT1));
		return FALSE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			num = w32g_get_playlist_num_ctrl();
			GetDlgItemText(hwnd, IDC_EDIT1, ListWndInfo.ListName[num], LF_FULLFACESIZE + 1);
			{
			TC_ITEM tci;
			HWND hwnd_tab;
			tci.mask = TCIF_TEXT;
			tci.pszText = ListWndInfo.ListName[num];
			tci.cchTextMax = _tcslen(ListWndInfo.ListName[num]);
			hwnd_tab = GetDlgItem(hListWnd, IDC_TAB_PLAYLIST);
			SendMessage(hwnd_tab, TCM_SETITEM, (WPARAM)num, (LPARAM)&tci);
			}
#ifdef EXT_CONTROL_MAIN_THREAD
			w32g_ext_control_main_thread(RC_EXT_PLAYLIST_CTRL, num);
			w32g_ext_control_main_thread(RC_EXT_UPDATE_PLAYLIST, 0);
#else
			w32g_send_rc(RC_EXT_PLAYLIST_CTRL, num);
			w32g_send_rc(RC_EXT_UPDATE_PLAYLIST, 0);
#endif
			// thru close
		case IDCLOSE:
		case IDCANCEL:
			EndDialog(hwnd, TRUE);
			break;
		default:
			return FALSE;
		}
		break;
	case WM_CLOSE:
		EndDialog(hwnd, TRUE);
		break;
	default:
		return FALSE;
	}
	return FALSE;
}

static LRESULT CALLBACK
ListWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	static BOOL ListSearchWndShow;
	switch (uMess){
	case WM_INITDIALOG:
		ListSearchWndShow = 0;
		InitListSearchWnd(hwnd);
		ListWndCreateTabItems(hwnd);
#ifdef LISTVIEW_PLAYLIST
		{
			HWND hlv = GetDlgItem(hwnd, IDC_LV_PLAYLIST);
			volatile LVCOLUMN lvc;
			lvc.mask = LVCF_TEXT | LVCF_WIDTH;
			lvc.fmt = LVCFMT_LEFT;
			lvc.cx = ListWndInfo.columWidth[0];
			lvc.pszText = TEXT("Title");
			lvc.cchTextMax = 256 - 1;
			lvc.iSubItem = 0;
			ListView_InsertColumn(hlv, 0, &lvc);
			lvc.cx = ListWndInfo.columWidth[1];
			lvc.pszText = TEXT("Artist");
			ListView_InsertColumn(hlv, 1, &lvc);
			ListWndInfo.hwndList = hlv;		
			lvc.cx = ListWndInfo.columWidth[2];
			lvc.pszText = TEXT("Duration");
			ListView_InsertColumn(hlv, 2, &lvc);
			ListWndInfo.hwndList = hlv;		
			lvc.cx = ListWndInfo.columWidth[3];
			lvc.pszText = TEXT("System");
			ListView_InsertColumn(hlv, 3, &lvc);
			ListWndInfo.hwndList = hlv;		
			lvc.cx = ListWndInfo.columWidth[4];
			lvc.pszText = TEXT("File Type");
			ListView_InsertColumn(hlv, 4, &lvc);
			ListWndInfo.hwndList = hlv;		
			lvc.cx = ListWndInfo.columWidth[5];
			lvc.pszText = TEXT("File Path");
			ListView_InsertColumn(hlv, 5, &lvc);			
			ListView_SetExtendedListViewStyle(hlv, LVS_NOSORTHEADER);
			ListView_SetExtendedListViewStyleEx(hlv, 0xFFFFFFFF, LVS_EX_FULLROWSELECT | LVS_EX_SUBITEMIMAGES | LVS_EX_GRIDLINES | LVS_EX_UNDERLINEHOT);
		//	ListView_SetExtendedListViewStyleEx(hlv, LVS_EX_CHECKBOXES, LVS_EX_CHECKBOXES);
			init_imagelist(hlv);
		}
#else
		ListWndInfo.hwndList = GetDlgItem(hwnd, IDC_LISTBOX_PLAYLIST);
		SendDlgItemMessage(hwnd,IDC_LISTBOX_PLAYLIST,
			LB_SETHORIZONTALEXTENT,(WPARAM)LISTWND_HORIZONTALEXTENT,0);
#endif
#ifdef EXT_CONTROL_MAIN_THREAD
		w32g_ext_control_main_thread(RC_EXT_PLAYLIST_CTRL, 0);
		w32g_ext_control_main_thread(RC_EXT_UPDATE_PLAYLIST, 0);
#else
		w32g_send_rc(RC_EXT_PLAYLIST_CTRL, 0);
		w32g_send_rc(RC_EXT_UPDATE_PLAYLIST, 0);
#endif
		SetWindowPosSize(GetDesktopWindow(),hwnd,ListWndInfo.PosX, ListWndInfo.PosY );
		return FALSE;
	case WM_NOTIFY:
		{
		LPNMHDR pnmh = (LPNMHDR) lParam;
		switch(pnmh->idFrom){
#ifdef LISTVIEW_PLAYLIST
		case IDC_LV_PLAYLIST:
			switch (pnmh->code) {
			case NM_DBLCLK:
				SendMessage(hwnd,WM_COMMAND,(WPARAM)IDM_LISTWND_PLAY,0);
				break;
			case NM_CLICK:
			case NM_RCLICK:	
			case NM_HOVER:
				break;
			case LVN_MARQUEEBEGIN:
				break;
			case LVN_BEGINDRAG:
			case LVN_BEGINRDRAG:
				break;
			case LVN_KEYDOWN:
				{
					UINT vkey = ((LPNMLVKEYDOWN)lParam)->wVKey;					
					switch(vkey){
					case VK_SPACE:
					case VK_RETURN:						
						SendMessage(hwnd,WM_COMMAND,(WPARAM)IDM_LISTWND_PLAY,0);
						break;
					case 0x50:	// VK_P
						SendMessage(hMainWnd,WM_COMMAND,MAKEWPARAM(IDM_PREV,0),0);
						break;
					case 0x4e:	// VK_N
						SendMessage(hMainWnd,WM_COMMAND,MAKEWPARAM(IDM_NEXT,0),0);
						break;
					case 0x45:	// VK_E
						SendMessage(hMainWnd,WM_COMMAND,MAKEWPARAM(IDM_STOP,0),0);
						return -2;
					case 0x53:	// VK_S
						SendMessage(hMainWnd,WM_COMMAND,MAKEWPARAM(IDM_PAUSE,0),0);
						break;
					case VK_ESCAPE:
						SendMessage(hListWnd,WM_COMMAND,MAKEWPARAM(0,IDCLOSE),0);
						break;
					case 0x51:	// VK_Q
						if (MessageBox(hListWnd, _T("Quit TiMidity?"), _T("TiMidity"), MB_ICONQUESTION | MB_YESNO) == IDYES)
							SendMessage(hMainWnd,WM_CLOSE,0,0);
						break;
					case VK_BACK:
#ifdef EXT_CONTROL_MAIN_THREAD
						w32g_ext_control_main_thread(RC_EXT_DELETE_PLAYLIST, -1);
#else
						w32g_send_rc(RC_EXT_DELETE_PLAYLIST, -1);
#endif
						break;
					case 0x44:	// VK_D
#ifdef EXT_CONTROL_MAIN_THREAD
						w32g_ext_control_main_thread(RC_EXT_DELETE_PLAYLIST, 0);
#else
						w32g_send_rc(RC_EXT_DELETE_PLAYLIST, 0);
#endif
						break;
					case VK_DELETE:
#ifdef EXT_CONTROL_MAIN_THREAD
						w32g_ext_control_main_thread(RC_EXT_ROTATE_PLAYLIST, -1);
#else
						w32g_send_rc(RC_EXT_ROTATE_PLAYLIST, -1);
#endif
						break;
					case VK_INSERT:
#ifdef EXT_CONTROL_MAIN_THREAD
						w32g_ext_control_main_thread(RC_EXT_ROTATE_PLAYLIST, 1);
#else
						w32g_send_rc(RC_EXT_ROTATE_PLAYLIST, 1);
#endif
						break;
					case 0x5A:	// VK_Z
#ifdef EXT_CONTROL_MAIN_THREAD
						w32g_ext_control_main_thread(RC_EXT_COPYCUT_PLAYLIST, 0);
#endif					
						break;
					case 0x58:	// VK_X
#ifdef EXT_CONTROL_MAIN_THREAD
						w32g_ext_control_main_thread(RC_EXT_COPYCUT_PLAYLIST, 1);
#endif					
						break;
					case 0x41:	// VK_A
#ifdef EXT_CONTROL_MAIN_THREAD
						w32g_ext_control_main_thread(RC_EXT_PASTE_PLAYLIST, 0);
#endif					
						break;
					case 0x46:	// VK_F
						break;
					case 0x42:	// VK_B
						break;
					case 0x4D:	// VK_M
						SendMessage(hListWnd,WM_COMMAND,MAKEWPARAM(IDC_BUTTON_REFINE,0),0);
						break;
					case 0x43:	// VK_C
						SendMessage(hListWnd,WM_COMMAND,MAKEWPARAM(IDC_BUTTON_CLEAR,0),0);
						break;
					case 0x55:	// VK_U
						SendMessage(hListWnd,WM_COMMAND,MAKEWPARAM(IDC_BUTTON_UNIQ,0),0);
						break;
					case 0x56:	// VK_V
						SendMessage(hListWnd,WM_COMMAND,MAKEWPARAM(IDC_BUTTON_DOC,0),0);
						break;
					case 0x57:	// VK_W
						SendMessage(hMainWnd,WM_COMMAND,MAKEWPARAM(IDM_WRD,0),0);
						break;
					case 0x49:// VK_I
						{	
						int nIndex = SendDlgItemMessage(hwnd, IDC_TAB_PLAYLIST, TCM_GETCURSEL, (WPARAM)0, (LPARAM)0);
						--nIndex;
						if(nIndex < 0)
							nIndex = playlist_max - 1;
						SendDlgItemMessage(hwnd, IDC_TAB_PLAYLIST, TCM_SETCURSEL, (WPARAM)nIndex, (LPARAM)0);
#ifdef EXT_CONTROL_MAIN_THREAD
						w32g_ext_control_main_thread(RC_EXT_PLAYLIST_CTRL, nIndex);
						w32g_ext_control_main_thread(RC_EXT_UPDATE_PLAYLIST, 0);
#else
						w32g_send_rc(RC_EXT_PLAYLIST_CTRL, nIndex);
						w32g_send_rc(RC_EXT_UPDATE_PLAYLIST, 0);
#endif
						}
						break;
					case 0x4F:// VK_O
						{	
						int nIndex = SendDlgItemMessage(hwnd, IDC_TAB_PLAYLIST, TCM_GETCURSEL, (WPARAM)0, (LPARAM)0);
						++nIndex;
						if(nIndex >= playlist_max)
							nIndex = 0;
						SendDlgItemMessage(hwnd, IDC_TAB_PLAYLIST, TCM_SETCURSEL, (WPARAM)nIndex, (LPARAM)0);
#ifdef EXT_CONTROL_MAIN_THREAD
						w32g_ext_control_main_thread(RC_EXT_PLAYLIST_CTRL, nIndex);
						w32g_ext_control_main_thread(RC_EXT_UPDATE_PLAYLIST, 0);
#else
						w32g_send_rc(RC_EXT_PLAYLIST_CTRL, nIndex);
						w32g_send_rc(RC_EXT_UPDATE_PLAYLIST, 0);
#endif
						}
						break;
					}

				}
				break;
			case LVN_COLUMNCLICK:
				break;
			default:
				break;
			}
			break;
#endif
		case IDC_TAB_PLAYLIST:
			switch (pnmh->code) {
			case TCN_SELCHANGE:
			{
			int nIndex = SendDlgItemMessage(hwnd, IDC_TAB_PLAYLIST,
							TCM_GETCURSEL, (WPARAM)0, (LPARAM)0);
#ifdef EXT_CONTROL_MAIN_THREAD
			w32g_ext_control_main_thread(RC_EXT_PLAYLIST_CTRL, nIndex);
			w32g_ext_control_main_thread(RC_EXT_UPDATE_PLAYLIST, 0);
#else
			w32g_send_rc(RC_EXT_PLAYLIST_CTRL, nIndex);
			w32g_send_rc(RC_EXT_UPDATE_PLAYLIST, 0);
#endif
			return TRUE;
			}
			default:
				break;
			}
			break;
		}
		}
		break;
	case WM_DESTROY:
#ifdef LISTVIEW_PLAYLIST
		{
			HWND hlv = GetDlgItem(hwnd, IDC_LV_PLAYLIST);
			int i;
			for(i = 0; i < LISTWND_COLUM; i++){
				ListWndInfo.columWidth[i] = ListView_GetColumnWidth(hlv, i);
			}
		}
		uninit_imagelist();
#endif
		{
		RECT rc;
		GetWindowRect(hwnd,&rc);
		ListWndInfo.Width = rc.right - rc.left;
		ListWndInfo.Height = rc.bottom - rc.top;
		}
		DestroyMenu(ListWndInfo.hPopupMenu);
		ListWndInfo.hPopupMenu = NULL;
		DeleteObject(ListWndInfo.hFontList);
		ListWndInfo.hFontList = NULL;
		INISaveListWnd();
		break;
		/* マウス入力がキャプチャされていないための処理 */
	case WM_SETCURSOR:
		switch(HIWORD(lParam)){
		case WM_RBUTTONDOWN:
			if(LOWORD(lParam)!=HTCAPTION){	// タイトルバーにないとき
				POINT point;
				int res;
				GetCursorPos(&point);
				SetForegroundWindow ( hwnd );				
#if 1 // menu position
				{
				RECT rc;
				int mw = 205 + 5, mh = 258 + 5; // menu固定 W:205 H:258 (margin+5
			//	GetWindowRect(GetDesktopWindow(), &rc);
				GetWindowRect(hwnd, &rc);
				if((point.y + mh) > rc.bottom)
					point.y = rc.bottom - mh;
				if(point.y < 0)
					point.y = 0;
				if((point.x + mw) > rc.right)
					point.x = rc.right - mw;
				if(point.x < 0)
					point.x = 0;
				}
#endif
				res = TrackPopupMenu(ListWndInfo.hPopupMenu,TPM_TOPALIGN|TPM_LEFTALIGN,
					point.x,point.y,0,hwnd,NULL);				
				PostMessage ( hwnd, WM_NULL, 0, 0 );
				return TRUE;
			}
			break;
		default:
			break;
		}
		break;
	case WM_CHOOSEFONT_DIAG:
		{
			char fontName[LF_FULLFACESIZE + 1];
			int fontHeight;
			int fontWidth;
			strncpy(fontName,ListWndInfo.fontName,LF_FULLFACESIZE + 1);
			fontHeight = ListWndInfo.fontHeight;
			fontWidth = ListWndInfo.fontWidth;
			if(DlgChooseFont(hwnd,fontName,&fontHeight,&fontWidth)==0){
				ListWndSetFontListBox(fontName,fontWidth,fontHeight);
			}
		}
		break;
	case WM_LIST_SEARCH_DIAG:
		ShowListSearch();
		break;
	case WM_LISTNAME_DIAG:
		DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG_SEARCHBOX), hwnd, ListNameWndProc);
		break;
	case WM_COMMAND:
			switch (HIWORD(wParam)) {
			case IDCLOSE:
				ShowWindow(hwnd, SW_HIDE);
				MainWndUpdateListButton();
				break;
			case LBN_DBLCLK:
				SendMessage(hwnd,WM_COMMAND,(WPARAM)IDM_LISTWND_PLAY,0);
				return FALSE;
			case LBN_SELCHANGE:
				{
				int selected, nfiles, cursel;
				w32g_get_playlist_index(&selected,&nfiles,&cursel);
				SetNumListWnd(cursel,nfiles);
				return FALSE;
				}
			default:
				break;
			}
			switch (LOWORD(wParam)) {
			case IDC_BUTTON_CLEAR:
				if (MessageBox(hListWnd, _T("Clear playlist?"), _T("Playlist"),
							  MB_YESNO)==IDYES)
#ifdef EXT_CONTROL_MAIN_THREAD
					w32g_ext_control_main_thread(RC_EXT_CLEAR_PLAYLIST, 0);
#else
					w32g_send_rc(RC_EXT_CLEAR_PLAYLIST, 0);
#endif
				return FALSE;
			case IDC_BUTTON_REFINE:
				if(MessageBox(hListWnd,
							  _T("Remove unsupported file types from the playlist?"),
							  _T("Playlist"),MB_YESNO) == IDYES)
#ifdef EXT_CONTROL_MAIN_THREAD
					w32g_ext_control_main_thread(RC_EXT_REFINE_PLAYLIST, 0);
#else
					w32g_send_rc(RC_EXT_REFINE_PLAYLIST, 0);
#endif
				return FALSE;
			case IDC_BUTTON_UNIQ:
				if(MessageBox(hListWnd,
							  _T("Remove the same files from the playlist and make files of the playlist unique?"),
							  _T("Playlist"),MB_YESNO)==IDYES)
#ifdef EXT_CONTROL_MAIN_THREAD
					w32g_ext_control_main_thread(RC_EXT_UNIQ_PLAYLIST, 0);
#else
					w32g_send_rc(RC_EXT_UNIQ_PLAYLIST, 0);
#endif
				return FALSE;
			case IDM_LISTWND_REMOVE:
#ifdef EXT_CONTROL_MAIN_THREAD
				w32g_ext_control_main_thread(RC_EXT_DELETE_PLAYLIST, 0);
#else
				w32g_send_rc(RC_EXT_DELETE_PLAYLIST, 0);
#endif
				break;
			case IDM_LISTWND_CUT:
#ifdef EXT_CONTROL_MAIN_THREAD
				w32g_ext_control_main_thread(RC_EXT_COPYCUT_PLAYLIST, 1);
#endif
				break;
			case IDM_LISTWND_COPY:
#ifdef EXT_CONTROL_MAIN_THREAD
				w32g_ext_control_main_thread(RC_EXT_COPYCUT_PLAYLIST, 0);
#endif
				break;
			case IDM_LISTWND_PASTE:
#ifdef EXT_CONTROL_MAIN_THREAD
				w32g_ext_control_main_thread(RC_EXT_PASTE_PLAYLIST, 0);
#endif
				break;		

			case IDC_BUTTON_DOC: {
					int cursel;
					w32g_get_playlist_index(NULL, NULL, &cursel);
#ifdef EXT_CONTROL_MAIN_THREAD
					w32g_ext_control_main_thread(RC_EXT_OPEN_DOC, cursel);
#else
					w32g_send_rc(RC_EXT_OPEN_DOC, cursel);
#endif
				}
				break;
			case IDM_LISTWND_PLAY:
				{
#ifdef LISTVIEW_PLAYLIST
					int new_cursel =  ListView_GetNextItem(GetDlgItem(hwnd, IDC_LV_PLAYLIST), -1, LVNI_FOCUSED);
#else
					int new_cursel =  SendDlgItemMessage(hwnd,IDC_LISTBOX_PLAYLIST,LB_GETCURSEL,0,0);
#endif
					int selected, nfiles, cursel;
					w32g_set_playlist_ctrl_play();
					w32g_get_playlist_index(&selected, &nfiles, &cursel);
					if ( nfiles <= new_cursel ) new_cursel = nfiles - 1;
					if ( new_cursel >= 0 )
#ifdef EXT_CONTROL_MAIN_THREAD
						w32g_ext_control_main_thread(RC_EXT_JUMP_FILE, new_cursel );
#else
						w32g_send_rc(RC_EXT_JUMP_FILE, new_cursel );
#endif
				}
				return FALSE;
			case IDM_LISTWND_CHOOSEFONT:
				{
 					SendMessage(hwnd,WM_CHOOSEFONT_DIAG,0,0);
				}
				return FALSE;
			case IDM_LISTWND_LISTNAME:
				{
 					SendMessage(hwnd,WM_LISTNAME_DIAG,0,0);
				}
				return FALSE;
			case IDM_LISTWND_CURRENT:
				if(w32g_is_playlist_ctrl_play())
				{
					int selected, nfiles, cursel;
					w32g_get_playlist_index(&selected, &nfiles, &cursel);
#ifdef LISTVIEW_PLAYLIST
					ListView_SetItemState(ListWndInfo.hwndList, selected, LVIS_FOCUSED, LVIS_FOCUSED);
#else
					SendDlgItemMessage(hwnd,IDC_LISTBOX_PLAYLIST, LB_SETCURSEL,(WPARAM)selected,0);
#endif
					SetNumListWnd(selected,nfiles);
				}
				return FALSE;
			case IDM_LISTWND_SEARCH:
				{
					SendMessage(hwnd,WM_LIST_SEARCH_DIAG,0,0);
				}
				return FALSE;
			default:
				break;
			}
			break;
			case WM_VKEYTOITEM:
				{
					UINT vkey = (UINT)LOWORD(wParam);
					int nCaretPos = (int)HIWORD(wParam);
					switch(vkey){
					case VK_SPACE:
					case VK_RETURN:
#ifdef EXT_CONTROL_MAIN_THREAD
						w32g_ext_control_main_thread(RC_EXT_JUMP_FILE, nCaretPos);
#else
						w32g_send_rc(RC_EXT_JUMP_FILE, nCaretPos);
#endif
						return -2;
					case 0x50:	// VK_P
						SendMessage(hMainWnd,WM_COMMAND,MAKEWPARAM(IDM_PREV,0),0);
						return -2;
					case 0x4e:	// VK_N
						SendMessage(hMainWnd,WM_COMMAND,MAKEWPARAM(IDM_NEXT,0),0);
						return -2;
					case 0x45:	// VK_E
						SendMessage(hMainWnd,WM_COMMAND,MAKEWPARAM(IDM_STOP,0),0);
						return -2;
					case 0x53:	// VK_S
						SendMessage(hMainWnd,WM_COMMAND,MAKEWPARAM(IDM_PAUSE,0),0);
						return -2;
					case VK_ESCAPE:
						SendMessage(hListWnd,WM_COMMAND,MAKEWPARAM(0,IDCLOSE),0);
						return -2;
					case 0x51:	// VK_Q
						if (MessageBox(hListWnd, _T("Quit TiMidity?"), _T("TiMidity"), MB_ICONQUESTION | MB_YESNO) == IDYES)
							SendMessage(hMainWnd,WM_CLOSE,0,0);
						return -2;
					case VK_BACK:
#ifdef EXT_CONTROL_MAIN_THREAD
						w32g_ext_control_main_thread(RC_EXT_DELETE_PLAYLIST, -1);
#else
						w32g_send_rc(RC_EXT_DELETE_PLAYLIST, -1);
#endif
						return -2;
					case 0x44:	// VK_D
#ifdef EXT_CONTROL_MAIN_THREAD
						w32g_ext_control_main_thread(RC_EXT_DELETE_PLAYLIST, 0);
#else
						w32g_send_rc(RC_EXT_DELETE_PLAYLIST, 0);
#endif
						return -2;
					case VK_DELETE:
#ifdef EXT_CONTROL_MAIN_THREAD
						w32g_ext_control_main_thread(RC_EXT_ROTATE_PLAYLIST, -1);
#else
						w32g_send_rc(RC_EXT_ROTATE_PLAYLIST, -1);
#endif
						return -2;
					case VK_INSERT:
#ifdef EXT_CONTROL_MAIN_THREAD
						w32g_ext_control_main_thread(RC_EXT_ROTATE_PLAYLIST, 1);
#else
						w32g_send_rc(RC_EXT_ROTATE_PLAYLIST, 1);
#endif
						return -2;
					case 0x46:	// VK_F
						return -2;
					case 0x42:	// VK_B
						return -2;
					case 0x4D:	// VK_M
						SendMessage(hListWnd,WM_COMMAND,MAKEWPARAM(IDC_BUTTON_REFINE,0),0);
						return -2;
					case 0x43:	// VK_C
						SendMessage(hListWnd,WM_COMMAND,MAKEWPARAM(IDC_BUTTON_CLEAR,0),0);
						return -2;
					case 0x55:	// VK_U
						SendMessage(hListWnd,WM_COMMAND,MAKEWPARAM(IDC_BUTTON_UNIQ,0),0);
						return -2;
					case 0x56:	// VK_V
						SendMessage(hListWnd,WM_COMMAND,MAKEWPARAM(IDC_BUTTON_DOC,0),0);
						return -2;
					case 0x57:	// VK_W
						SendMessage(hMainWnd,WM_COMMAND,MAKEWPARAM(IDM_WRD,0),0);
						return -2;
					case 0x49:// VK_I
						{	
						int nIndex = SendDlgItemMessage(hwnd, IDC_TAB_PLAYLIST, TCM_GETCURSEL, (WPARAM)0, (LPARAM)0);
						--nIndex;
						if(nIndex < 0)
							nIndex = playlist_max - 1;
						SendDlgItemMessage(hwnd, IDC_TAB_PLAYLIST, TCM_SETCURSEL, (WPARAM)nIndex, (LPARAM)0);
#ifdef EXT_CONTROL_MAIN_THREAD
						w32g_ext_control_main_thread(RC_EXT_PLAYLIST_CTRL, nIndex);
						w32g_ext_control_main_thread(RC_EXT_UPDATE_PLAYLIST, 0);
#else
						w32g_send_rc(RC_EXT_PLAYLIST_CTRL, nIndex);
						w32g_send_rc(RC_EXT_UPDATE_PLAYLIST, 0);
#endif
						}
						return -2;
					case 0x4F:// VK_O
						{	
						int nIndex = SendDlgItemMessage(hwnd, IDC_TAB_PLAYLIST, TCM_GETCURSEL, (WPARAM)0, (LPARAM)0);
						++nIndex;
						if(nIndex >= playlist_max)
							nIndex = 0;
						SendDlgItemMessage(hwnd, IDC_TAB_PLAYLIST, TCM_SETCURSEL, (WPARAM)nIndex, (LPARAM)0);
#ifdef EXT_CONTROL_MAIN_THREAD
						w32g_ext_control_main_thread(RC_EXT_PLAYLIST_CTRL, nIndex);
						w32g_ext_control_main_thread(RC_EXT_UPDATE_PLAYLIST, 0);
#else
						w32g_send_rc(RC_EXT_PLAYLIST_CTRL, nIndex);
						w32g_send_rc(RC_EXT_UPDATE_PLAYLIST, 0);
#endif
						}
						return -2;
					case VK_F1:
					case 0x48:	// VK_H
						if ( PlayerLanguage == LANGUAGE_JAPANESE ){
						MessageBox(hListWnd,
							_T("キーコマンド\n")
							_T("リストウインドウコマンド\n")
							_T("  ESC: ヘルプを閉じる      H: ヘルプを出す\n")
							_T("  V: ドキュメントを見る      W: WRD ウインドウを開く\n")
							_T("プレイヤーコマンド\n")
							_T("  SPACE/ENTER: 演奏開始    E: 停止    S: 一時停止\n")
							_T("  P: 前の曲    N: 次の曲\n")
							_T("プレイリスト操作コマンド\n")
							_T("  M: MIDIファイル以外を削除    U: 重複ファイルを削除\n")
							_T("  C: プレイリストのクリア\n")
#ifdef LISTVIEW_PLAYLIST
							_T("  Z: 選択した曲をコピー    X: 選択した曲をカット\n")
							_T("  A: フォーカス位置へをペースト(挿入)\n")
#endif
							_T("  D: カーソルの曲を削除    BS: カーソルの前の曲を削除\n")
							_T("  INS: カーソルの曲をリストの最後に移す (Push)\n")
							_T("  DEL: リストの最後の曲をカーソルの前に挿入 (Pop)\n")
							_T("プレイリストタブ操作コマンド\n")
							_T("  I: 次プレイリストタブ    O: 前プレイリストタブ\n")
							_T("TiMidity コマンド\n")
							_T("  Q: 終了\n")
							, _T("ヘルプ"), MB_OK);
						} else {
						MessageBox(hListWnd,
							_T("Usage of key.\n")
							_T("List window command.\n")
							_T("  ESC: Close Help      H: Help\n")
							_T("  V: View Document   W: Open WRD window\n")
							_T("Player command.\n")
							_T("  SPACE/ENTER: PLAY    E: Stop    S: Pause\n")
							_T("  P: Prev    N: Next\n")
							_T("Playlist command.\n")
							_T("  M: Refine playlist    U: Uniq playlist\n")
							_T("  C: Clear playlist\n")
#ifdef LISTVIEW_PLAYLIST
							_T("  Z: Copy    X: Cut    A: Paste(Insert)\n")
#endif
							_T("  D: Remove playlist    BS: Remove previous playlist\n")
							_T("  INS: Push Playlist    DEL: Pop Playlist\n")
							_T("Playlist tab command.\n")
							_T("  I: next playlist tab    O: prev playlist tab\n")
							_T("TiMidity command.\n")
							_T("  Q: Quit\n")
							, _T("Help"),  MB_OK);
						}
						return -2;
					default:
						break;
			}
			return -1;
		}
	case WM_GETMINMAXINFO:
		{
			LPMINMAXINFO lpmmi = (LPMINMAXINFO) lParam;
			lpmmi->ptMinTrackSize.x = max(350, lpmmi->ptMinTrackSize.x);
			lpmmi->ptMinTrackSize.y = max(100, lpmmi->ptMinTrackSize.y);
		}
		return 0;
	case WM_SIZE:
		switch(wParam){
		case SIZE_MAXIMIZED:
		case SIZE_RESTORED:
			{		// なんか意味なく面倒(^^;;
				int x,y,cx,cy;
				int maxHeight = 0;
				int center, idControl;
				HWND hwndChild;
				RECT rcParent, rcChild, rcRest;
				GetWindowRect(hwnd,&rcParent);
				cx = rcParent.right-rcParent.left;
				cy  = rcParent.bottom-rcParent.top;
				GetClientRect(hwnd,&rcParent);
				rcRest.left = rcParent.left; rcRest.right = rcParent.right;

				// IDC_EDIT_NUM
				idControl = IDC_EDIT_NUM;
				hwndChild = GetDlgItem(hwnd,idControl);
				GetWindowRect(hwndChild,&rcChild);
				cx = rcChild.right-rcChild.left;
				cy = rcChild.bottom-rcChild.top;
				x = rcParent.left + 1;
				y = rcParent.bottom - cy - 3;
				MoveWindow(hwndChild,x,y,cx,cy,TRUE);
				if(cy>maxHeight) maxHeight = cy;
				rcRest.left += cx;
				// IDC_BUTTON_DOC
				idControl = IDC_BUTTON_DOC;
				hwndChild = GetDlgItem(hwnd,idControl);
				GetWindowRect(hwndChild,&rcChild);
				cx = rcChild.right-rcChild.left;
				cy = rcChild.bottom-rcChild.top;
				x = rcRest.left + 10;
				y = rcParent.bottom - cy - 1;
				MoveWindow(hwndChild,x,y,cx,cy,TRUE);
				if(cy>maxHeight) maxHeight = cy;
				rcRest.left += cx;
				// IDC_BUTTON_CLEAR
				idControl = IDC_BUTTON_CLEAR;
				hwndChild = GetDlgItem(hwnd,idControl);
				GetWindowRect(hwndChild,&rcChild);
				cx = rcChild.right-rcChild.left;
				cy = rcChild.bottom-rcChild.top;
				x = rcParent.right - cx - 1;
				y = rcParent.bottom - cy - 1;
				MoveWindow(hwndChild,x,y,cx,cy,TRUE);
				if(cy>maxHeight) maxHeight = cy;
				rcRest.right -= cx + 5;
				// IDC_BUTTON_UNIQ
				center = rcRest.left + (int)((rcRest.right - rcRest.left)*0.52);
				idControl = IDC_BUTTON_UNIQ;
				hwndChild = GetDlgItem(hwnd,idControl);
				GetWindowRect(hwndChild,&rcChild);
				cx = rcChild.right-rcChild.left;
				cy = rcChild.bottom-rcChild.top;
				x = center - cx;
				y = rcParent.bottom - cy - 1;
				MoveWindow(hwndChild,x,y,cx,cy,TRUE);
				if(cy>maxHeight) maxHeight = cy;
				// IDC_BUTTON_REFINE
				idControl = IDC_BUTTON_REFINE;
				hwndChild = GetDlgItem(hwnd,idControl);
				GetWindowRect(hwndChild,&rcChild);
				cx = rcChild.right-rcChild.left;
				cy = rcChild.bottom-rcChild.top;
				x = center + 3;
				y = rcParent.bottom - cy - 1;
				MoveWindow(hwndChild,x,y,cx,cy,TRUE);
				if(cy>maxHeight) maxHeight = cy;				
				// IDC_TAB_PLAYLIST
				idControl = IDC_TAB_PLAYLIST;
				hwndChild = GetDlgItem(hwnd,idControl);
				cx = rcParent.right - rcParent.left - 2;
				cy = rcParent.bottom - rcParent.top - maxHeight - 3;
				x  = rcParent.left + 1;
				y = rcParent.top + 1;
				MoveWindow(hwndChild,x,y,cx,cy,TRUE);
				// PLAYLIST
#ifdef LISTVIEW_PLAYLIST
				idControl = IDC_LV_PLAYLIST;
#else
				idControl = IDC_LISTBOX_PLAYLIST;
#endif
				hwndChild = GetDlgItem(hwnd,idControl);
				cx = rcParent.right - rcParent.left - 2;
				cy = rcParent.bottom - rcParent.top - maxHeight - 3 - 20; // -20: tab hight
				x  = rcParent.left + 1;
				y = rcParent.top + 1 + 20; // +20: tab hight
				MoveWindow(hwndChild,x,y,cx,cy,TRUE);
				// 
				InvalidateRect(hwnd,&rcParent,FALSE);
				UpdateWindow(hwnd);
				GetWindowRect(hwnd,&rcParent);
				ListWndInfo.Width = rcParent.right - rcParent.left;
				ListWndInfo.Height = rcParent.bottom - rcParent.top;
				break;
			}
		case SIZE_MINIMIZED:
		case SIZE_MAXHIDE:
		case SIZE_MAXSHOW:
		default:
			break;
		}
		break;
	case WM_MOVE:
//		ListWndInfo.PosX = (int) LOWORD(lParam);
//		ListWndInfo.PosY = (int) HIWORD(lParam);
		{
			RECT rc;
			GetWindowRect(hwnd,&rc);
			ListWndInfo.PosX = rc.left;
			ListWndInfo.PosY = rc.top;
		}
		break;
	// See PreDispatchMessage() in w32g2_main.c
	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
	{
		int nVirtKey = (int)wParam;
		switch(nVirtKey){
			case VK_ESCAPE:
				SendMessage(hwnd,WM_CLOSE,0,0);
				break;
		}
	}
		break;
	case WM_CLOSE:
		ShowSubWindow(hListWnd,0);
//		ShowWindow(hListWnd, SW_HIDE);
		MainWndUpdateListButton();
		break;
	case WM_SHOWWINDOW:
	{
		BOOL fShow = (BOOL)wParam;
		if ( fShow ) {
			if ( ListSearchWndShow ) {
				ShowListSearch();
			} else {
				HideListSearch();
			}
		} else {
			if ( IsWindowVisible ( hListSearchWnd ) )
				ListSearchWndShow = TRUE;
			else
				ListSearchWndShow = FALSE;
			HideListSearch();
		}
	}
		break;
	case WM_DROPFILES:
		SendMessage(hMainWnd,WM_DROPFILES,wParam,lParam);
		return 0;
	case WM_HELP:
		PostMessage(hListWnd, WM_VKEYTOITEM, MAKEWPARAM('H', 0), 0);
		break;
	default:
		return FALSE;
	}
	return FALSE;
}

static int ListWndInfoReset(HWND hwnd)
{
	memset(&ListWndInfo,0,sizeof(LISTWNDINFO));
	ListWndInfo.PosX = - 1;
	ListWndInfo.PosY = - 1;
	ListWndInfo.Height = 400;
	ListWndInfo.Width = 400;
	ListWndInfo.hPopupMenu = NULL;
	ListWndInfo.hwnd = hwnd;
	if ( hwnd != NULL )
#ifdef LISTVIEW_PLAYLIST
		ListWndInfo.hwndList = GetDlgItem(hwnd,IDC_LV_PLAYLIST);
#else
		ListWndInfo.hwndList = GetDlgItem(hwnd,IDC_LISTBOX_PLAYLIST);
#endif
	strcpy(ListWndInfo.fontNameEN,"Times New Roman");
	char *s = tchar_to_char(_T("ＭＳ 明朝"));
	strcpy(ListWndInfo.fontNameJA,s);
	safe_free(s);
	ListWndInfo.fontHeight = 12;
	ListWndInfo.fontWidth = 6;
	ListWndInfo.fontFlags = FONT_FLAGS_FIXED;
	switch(PlayerLanguage){
	case LANGUAGE_ENGLISH:
		ListWndInfo.fontName = ListWndInfo.fontNameEN;
		break;
	case LANGUAGE_JAPANESE:
	default:
		ListWndInfo.fontName = ListWndInfo.fontNameJA;
		break;
	}
	return 0;
}
static int ListWndInfoApply(void)
{
	RECT rc;
	HFONT hFontPre = NULL;
	DWORD fdwPitch = (ListWndInfo.fontFlags&FONT_FLAGS_FIXED)?FIXED_PITCH:VARIABLE_PITCH;	
	DWORD fdwItalic = (ListWndInfo.fontFlags&FONT_FLAGS_ITALIC)?TRUE:FALSE;
	TCHAR *tfontname = char_to_tchar(ListWndInfo.fontName);
	HFONT hFont =
		CreateFont(ListWndInfo.fontHeight,ListWndInfo.fontWidth,0,0,FW_DONTCARE,fdwItalic,FALSE,FALSE,
			DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
	      	fdwPitch | FF_DONTCARE, tfontname);
	safe_free(tfontname);
	if(hFont != NULL){
		hFontPre = ListWndInfo.hFontList;
		ListWndInfo.hFontList = hFont;
		SendMessage(ListWndInfo.hwndList,WM_SETFONT,(WPARAM)ListWndInfo.hFontList,(LPARAM)MAKELPARAM(TRUE,0));
	}
	GetWindowRect(ListWndInfo.hwnd,&rc);
	MoveWindow(ListWndInfo.hwnd,rc.left,rc.top,ListWndInfo.Width,ListWndInfo.Height,TRUE);
//	InvalidateRect(hwnd,&rc,FALSE);
//	UpdateWindow(hwnd);
	if(hFontPre!=NULL)
		DeleteObject(hFontPre);
	INISaveListWnd();
	return 0;
}

static int ListWndSetFontListBox(char *fontName, int fontWidth, int fontHeight)
{
	strcpy(ListWndInfo.fontName,fontName);
	ListWndInfo.fontWidth = fontWidth;
	ListWndInfo.fontHeight = fontHeight;
	ListWndInfoApply();
	return 0;
}

void SetNumListWnd(int cursel, int nfiles)
{
	TCHAR buff[64];
	_stprintf(buff, _T("%04d/%04d"), cursel + 1, nfiles);
	SetDlgItemText(hListWnd,IDC_EDIT_NUM,buff);
}




#if 0
// ***************************************************************************
// Tracer Window

BOOL CALLBACK TracerWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
void InitTracerWnd(HWND hParentWnd)
{
	if (hTracerWnd != NULL) {
		DestroyWindow(hTracerWnd);
		hTracerWnd = NULL;
	}
	hTracerWnd = CreateDialog
		(hInst,MAKEINTRESOURCE(IDD_DIALOG_TRACER),hParentWnd,TracerWndProc);
	ShowWindow(hTracerWnd,TracerWndStartFlag ? SW_SHOW : SW_HIDE);
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
#endif



//****************************************************************************
// Doc Window

#define IDM_DOCWND_CHOOSEFONT 4232
int DocWndIndependent = 0; /* Independent document viewer mode.(独立ドキュメントビュワーモード) */
int DocWndAutoPopup = 0;
DOCWNDINFO DocWndInfo;

static LRESULT CALLBACK DocWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static void InitDocEditWnd(HWND hParentWnd);
static void DocWndConvertText(char *in, int in_size, char *out, int out_size);
static void DocWndSetText(char *text, int text_size);
static void DocWndSetInfo(char *info, char *filename);
static void DocWndInfoInit(void);
static int DocWndInfoLock(void);
static void DocWndInfoUnLock(void);

void InitDocWnd(HWND hParentWnd);
void DocWndInfoReset(void);
void DocWndAddDocFile(char *filename);
void DocWndSetMidifile(char *filename);
void DocWndReadDoc(int num);
void DocWndReadDocNext(void);
void DocWndReadDocPrev(void);

static int DocWndInfoReset2(HWND hwnd);
static int DocWndInfoApply(void);
static int DocWndSetFontEdit(char *fontName, int fontWidth, int fontHeight);

void InitDocWnd(HWND hParentWnd)
{
	HMENU hMenu;
	HICON hIcon;
	if (hDocWnd != NULL) {
		DestroyWindow(hDocWnd);
		hDocWnd = NULL;
	}
	DocWndInfoReset2(hDocWnd);
	INILoadDocWnd();
	switch(PlayerLanguage){
  	case LANGUAGE_ENGLISH:
		hDocWnd = CreateDialog
			(hInst,MAKEINTRESOURCE(IDD_DIALOG_DOC_EN),hParentWnd,DocWndProc);
		break;
	case LANGUAGE_JAPANESE:
 	default:
		hDocWnd = CreateDialog
			(hInst,MAKEINTRESOURCE(IDD_DIALOG_DOC),hParentWnd,DocWndProc);
	break;
	}
	hIcon = LoadImage(hInst, MAKEINTRESOURCE(IDI_ICON_TIMIDITY), IMAGE_ICON, 16, 16, 0);
	if (hIcon!=NULL) SendMessage(hDocWnd,WM_SETICON,FALSE,(LPARAM)hIcon);
	DocWndInfoReset2(hDocWnd);
	hMenu = GetSystemMenu(DocWndInfo.hwnd,FALSE);
	switch(PlayerLanguage){
	case LANGUAGE_JAPANESE:
		AppendMenu(hMenu,MF_SEPARATOR,0,0);
		AppendMenu(hMenu, MF_STRING, IDM_DOCWND_CHOOSEFONT, _T("フォントの選択"));
		break;
  	case LANGUAGE_ENGLISH:
 	default:
		AppendMenu(hMenu,MF_SEPARATOR,0,0);
		AppendMenu(hMenu, MF_STRING, IDM_DOCWND_CHOOSEFONT, _T("Choose Font"));
		break;
	}
	DocWndInfoReset2(hDocWnd);
	INILoadDocWnd();
	DocWndInfoApply();
	ShowWindow(hDocWnd,DocWndStartFlag ? SW_SHOW : SW_HIDE);
	UpdateWindow(hDocWnd);
	EnableWindow(GetDlgItem(hDocWnd,IDC_BUTTON_PREV),FALSE);
	EnableWindow(GetDlgItem(hDocWnd,IDC_BUTTON_NEXT),FALSE);
}

static LRESULT CALLBACK
DocWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess){
	case WM_INITDIALOG:
		PutsDocWnd("Doc Window\n");
		DocWndInfoInit();
		SetWindowPosSize(GetDesktopWindow(),hwnd,DocWndInfo.PosX, DocWndInfo.PosY );
		return FALSE;
	case WM_DESTROY:
		{
		RECT rc;
		GetWindowRect(hwnd,&rc);
		DocWndInfo.Width = rc.right - rc.left;
		DocWndInfo.Height = rc.bottom - rc.top;
		}
		INISaveDocWnd();
		break;
	case WM_SYSCOMMAND:
		switch(wParam){
		case IDM_DOCWND_CHOOSEFONT:
		{
			char fontName[LF_FULLFACESIZE + 1];
			int fontHeight;
			int fontWidth;
			strcpy(fontName,DocWndInfo.fontName);
			fontHeight = DocWndInfo.fontHeight;
			fontWidth = DocWndInfo.fontWidth;
			if(DlgChooseFont(hwnd,fontName,&fontHeight,&fontWidth)==0){
				DocWndSetFontEdit(fontName,fontWidth,fontHeight);
			}
			break;
		}
			break;
		default:
			break;
		}
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
			break;
		}
		switch (LOWORD(wParam)) {
		case IDC_BUTTON_NEXT:
			DocWndReadDocNext();
			break;
		case IDC_BUTTON_PREV:
			DocWndReadDocPrev();
			break;
		default:
			break;
		}
		return FALSE;
	// See PreDispatchMessage() in w32g2_main.c
	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
	{
		int nVirtKey = (int)wParam;
		switch(nVirtKey){
			case VK_ESCAPE:
				SendMessage(hwnd,WM_CLOSE,0,0);
				break;
		}
	}
		break;
	case WM_CLOSE:
		ShowSubWindow(hDocWnd,0);
//		ShowWindow(hDocWnd, SW_HIDE);
		MainWndUpdateDocButton();
		break;
	case WM_GETMINMAXINFO:
		{
			LPMINMAXINFO lpmmi = (LPMINMAXINFO) lParam;
			lpmmi->ptMinTrackSize.x = max(300, lpmmi->ptMinTrackSize.x);
			lpmmi->ptMinTrackSize.y = max(100, lpmmi->ptMinTrackSize.y);
		}
		return 0;
	case WM_SIZE:
		switch(wParam){
		case SIZE_MAXIMIZED:
		case SIZE_RESTORED:
			{		// なんか意味なく面倒(^^;;
				int x,y,cx,cy;
				int max = 0;
				int width;
				RECT rcParent;
				RECT rcEDIT_INFO, rcEDIT_FILENAME, rcBUTTON_PREV, rcBUTTON_NEXT, rcEDIT;
				HWND hwndEDIT_INFO, hwndEDIT_FILENAME, hwndBUTTON_PREV, hwndBUTTON_NEXT, hwndEDIT;
				GetWindowRect(hwnd,&rcParent);
				cx = rcParent.right-rcParent.left;
				cy  = rcParent.bottom-rcParent.top;
				GetClientRect(hwnd,&rcParent);
				hwndEDIT = GetDlgItem(hwnd,IDC_EDIT);
				hwndEDIT_INFO = GetDlgItem(hwnd,IDC_EDIT_INFO);
				hwndEDIT_FILENAME = GetDlgItem(hwnd,IDC_EDIT_FILENAME);
				hwndBUTTON_PREV = GetDlgItem(hwnd,IDC_BUTTON_PREV);
				hwndBUTTON_NEXT = GetDlgItem(hwnd,IDC_BUTTON_NEXT);
				GetWindowRect(hwndEDIT,&rcEDIT);
				GetWindowRect(hwndEDIT_INFO,&rcEDIT_INFO);
				GetWindowRect(hwndEDIT_FILENAME,&rcEDIT_FILENAME);
				GetWindowRect(hwndBUTTON_PREV,&rcBUTTON_PREV);
				GetWindowRect(hwndBUTTON_NEXT,&rcBUTTON_NEXT);
				width = rcParent.right - rcParent.left;

				cx = rcBUTTON_NEXT.right-rcBUTTON_NEXT.left;
				cy = rcBUTTON_NEXT.bottom-rcBUTTON_NEXT.top;
				x = rcParent.right - cx - 5;
				y = rcParent.bottom - cy;
				MoveWindow(hwndBUTTON_NEXT,x,y,cx,cy,TRUE);
				width -= cx + 5;
				if(cy>max) max = cy;
				cx = rcBUTTON_PREV.right-rcBUTTON_PREV.left;
				cy = rcBUTTON_PREV.bottom-rcBUTTON_PREV.top;
				x  -= cx + 5;
				y = rcParent.bottom - cy;
				MoveWindow(hwndBUTTON_PREV,x,y,cx,cy,TRUE);
				width -= cx;
				if(cy>max) max = cy;
				width -= 5;
//				cx = rcEDIT_INFO.right-rcEDIT_INFO.left;
				cx = (int)(width * 0.36);
				cy = rcEDIT_INFO.bottom-rcEDIT_INFO.top;
				x = rcParent.left;
				y = rcParent.bottom - cy;
				MoveWindow(hwndEDIT_INFO,x,y,cx,cy,TRUE);
				if(cy>max) max = cy;
				x += cx + 5;
//				cx = rcEDIT_FILENAME.right-rcEDIT_FILENAME.left;
				cx = (int)(width * 0.56);
				cy = rcEDIT_FILENAME.bottom-rcEDIT_FILENAME.top;
				y = rcParent.bottom - cy;
				MoveWindow(hwndEDIT_FILENAME,x,y,cx,cy,TRUE);
				if(cy>max) max = cy;
				cx = rcParent.right - rcParent.left;
				cy = rcParent.bottom - rcParent.top - max - 5;
				x  = rcParent.left;
				y = rcParent.top;
				MoveWindow(hwndEDIT,x,y,cx,cy,TRUE);
				InvalidateRect(hwnd,&rcParent,FALSE);
				UpdateWindow(hwnd);
				GetWindowRect(hwnd,&rcParent);
				DocWndInfo.Width = rcParent.right - rcParent.left;
				DocWndInfo.Height = rcParent.bottom - rcParent.top;
				break;
			}
		case SIZE_MINIMIZED:
		case SIZE_MAXHIDE:
		case SIZE_MAXSHOW:
		default:
			break;
		}
		break;
	case WM_MOVE:
//		DocWndInfo.PosX = (int) LOWORD(lParam);
//		DocWndInfo.PosY = (int) HIWORD(lParam);
		{
			RECT rc;
			GetWindowRect(hwnd,&rc);
			DocWndInfo.PosX = rc.left;
			DocWndInfo.PosY = rc.top;
		}
		break;
	case WM_DROPFILES:
#ifdef EXT_CONTROL_MAIN_THREAD
		w32g_ext_control_main_thread(RC_EXT_DROP, (ptr_size_t)wParam);
#else
		w32g_send_rc(RC_EXT_DROP, (ptr_size_t)wParam);
#endif
		return FALSE;
	default:
		return FALSE;
	}
	return FALSE;
}

static int DocWndInfoReset2(HWND hwnd)
{
//      ZeroMemory(&DocWndInfo,sizeof(DOCWNDINFO));
    DocWndInfo.PosX = - 1;
    DocWndInfo.PosY = - 1;
    DocWndInfo.Height = 400;
    DocWndInfo.Width = 400;
    DocWndInfo.hPopupMenu = NULL;
    DocWndInfo.hwnd = hwnd;
    if ( hwnd != NULL )
        DocWndInfo.hwndEdit = GetDlgItem(hwnd,IDC_EDIT);
    strcpy(DocWndInfo.fontNameEN,"Times New Roman");
	char* s = tchar_to_char(_T("ＭＳ 明朝"));
    strcpy(DocWndInfo.fontNameJA,s);
	safe_free(s);
    DocWndInfo.fontHeight = 12;
    DocWndInfo.fontWidth = 6;
    DocWndInfo.fontFlags = FONT_FLAGS_FIXED;
    switch (PlayerLanguage) {
    case LANGUAGE_ENGLISH:
        DocWndInfo.fontName = DocWndInfo.fontNameEN;
        break;
    case LANGUAGE_JAPANESE:
    default:
        DocWndInfo.fontName = DocWndInfo.fontNameJA;
        break;
    }
    return 0;
}
static int DocWndInfoApply(void)
{
	RECT rc;
	HFONT hFontPre = NULL;
	DWORD fdwPitch = (DocWndInfo.fontFlags&FONT_FLAGS_FIXED)?FIXED_PITCH:VARIABLE_PITCH;	
	DWORD fdwItalic = (DocWndInfo.fontFlags&FONT_FLAGS_ITALIC)?TRUE:FALSE;
	TCHAR *tfontname = char_to_tchar(DocWndInfo.fontName);
	HFONT hFont =
		CreateFont(DocWndInfo.fontHeight,DocWndInfo.fontWidth,0,0,FW_DONTCARE,fdwItalic,FALSE,FALSE,
			DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
	      	fdwPitch | FF_DONTCARE,tfontname);
	safe_free(tfontname);
	if(hFont != NULL){
		hFontPre = DocWndInfo.hFontEdit;
		DocWndInfo.hFontEdit = hFont;
		SendMessage(DocWndInfo.hwndEdit,WM_SETFONT,(WPARAM)DocWndInfo.hFontEdit,(LPARAM)MAKELPARAM(TRUE,0));
	}
	GetWindowRect(DocWndInfo.hwnd,&rc);
	MoveWindow(DocWndInfo.hwnd,rc.left,rc.top,DocWndInfo.Width,DocWndInfo.Height,TRUE);
//	InvalidateRect(hwnd,&rc,FALSE);
//	UpdateWindow(hwnd);
	if(hFontPre!=NULL) DeleteObject(hFontPre);
	INISaveDocWnd();
	return 0;
}

static int DocWndSetFontEdit(char *fontName, int fontWidth, int fontHeight)
{
	strcpy(DocWndInfo.fontName,fontName);
	DocWndInfo.fontWidth = fontWidth;
	DocWndInfo.fontHeight = fontHeight;
	DocWndInfoApply();
	return 0;
}

static char ControlCode[] = "@ABCDEFGHIJKLMNOPQRS";
static void DocWndConvertText(char *in, int in_size, char *out, int out_size)
{
	char *buffer = (char *)safe_malloc(sizeof(char)*out_size);
	int buffer_size = out_size;
	int i=0, j=0;
	int nl = 0;

// Convert Return Code CR, LF -> CR+LF ,
//         Control Code -> ^? (^@, ^A, ^B, ...).
// stage1:
	for(;;){
		if(i>=in_size || j>=buffer_size-1)
			goto stage1_end;
		if(nl==13){
			if(in[i]==13){
				if(j>=buffer_size-2)
					goto stage1_end;
				buffer[j++] = 13;
				buffer[j++] = 10;
				i++;
				nl = 13;
				continue;
			}
			if(in[i]==10){
				if(j>=buffer_size-2)
					goto stage1_end;
				buffer[j++] = 13;
				buffer[j++] = 10;
				i++;
				nl = 0;
				continue;
			}
			if(j>=buffer_size-2)
				goto stage1_end;
			buffer[j++] = 13;
			buffer[j++] = 10;
			if(in[i]>=0 && in[i]<=0x1f && in[i]!='\t'){
				if(j>=buffer_size-2)
					goto stage1_end;
				buffer[j++] = '^';
				buffer[j++] = ControlCode[in[i]];
			} else {
				if(j>=buffer_size-1)
					goto stage1_end;
				buffer[j++] = in[i];
			}
			i++;
			nl = 0;
			continue;
		}
		if(nl==10){
			if(in[i]==13||in[i]==10){
				if(j>=buffer_size-2)
					goto stage1_end;
				buffer[j++] = 13;
				buffer[j++] = 10;
				nl = in[i];
				i++;
				continue;
			}
			if(j>=buffer_size-2)
				goto stage1_end;
			buffer[j++] = 13;
			buffer[j++] = 10;
			if(in[i]>=0 && in[i]<=0x1f && in[i]!='\t'){
				if(j>=buffer_size-2)
					goto stage1_end;
				buffer[j++] = '^';
				buffer[j++] = ControlCode[in[i]];
			} else {
				if(j>=buffer_size-1)
					goto stage1_end;
				buffer[j++] = in[i];
			}
			i++;
			nl = 0;
			continue;
		}
		if(in[i]==13||in[i]==10){
			nl = in[i];
			i++;
			continue;
		}
		if(in[i]>=0 && in[i]<=0x1f && in[i]!='\t'){
			if(j>=buffer_size-2)
				goto stage1_end;
			buffer[j++] = '^';
			buffer[j++] = ControlCode[in[i]];
		} else {
			if(j>=buffer_size-1)
				goto stage1_end;
			buffer[j++] = in[i];
		}
		i++;
		nl = 0;
		continue;
	}
stage1_end:
	buffer[j] = '\0';
// Convert KANJI Code.
// stage2:
#ifndef MAX2
#define MAX2(x,y) ((x)>=(y)?(x):(y))
#endif
    switch (PlayerLanguage) {
    case LANGUAGE_ENGLISH:
        strncpy(out,buffer,MAX2(buffer_size-1,out_size-1));
        out[out_size-1] = '\0';
        safe_free(buffer);
        break;
    case LANGUAGE_JAPANESE:
    default:
        strncpy(out,buffer,MAX2(buffer_size-1,out_size-1));
        code_convert(buffer,out,out_size-1,NULL,NULL);
        out[out_size-1] = '\0';
        safe_free(buffer);
        break;
    }
}

#define BUFFER_SIZE (1024*64)
static void DocWndSetText(char *text, int text_size)
{
	char buffer[BUFFER_SIZE];
	int buffer_size = BUFFER_SIZE;
	if(!IsWindow(hDocWnd) || !DocWndFlag)
		return;
	if(DocWndInfoLock()==FALSE)
		return;
//	Edit_SetText(GetDlgItem(hDocWnd,IDC_EDIT),text);
	DocWndConvertText(text,text_size,buffer,buffer_size);
	TCHAR *t = char_to_tchar(buffer);
	Edit_SetText(GetDlgItem(hDocWnd,IDC_EDIT),t);
	safe_free(t);
	DocWndInfoUnLock();
}

static void DocWndSetInfo(char *info, char *filename)
{
	if(!IsWindow(hDocWnd) || !DocWndFlag)
		return;
	if(DocWndInfoLock()==FALSE)
		return;
	TCHAR *t = char_to_tchar(info);
	Edit_SetText(GetDlgItem(hDocWnd,IDC_EDIT_INFO),t);
	safe_free(t);
	t = char_to_tchar(filename);
	Edit_SetText(GetDlgItem(hDocWnd,IDC_EDIT_FILENAME),t);
	safe_free(t);
	DocWndInfoUnLock();
}

// *.doc *.txt *.hed archive#*.doc archive#*.txt archive#*.hed 

static void DocWndInfoInit(void)
{
//	DocWndInfo.hMutex = NULL;
//	DocWndInfo.hMutex = CreateMutex(NULL,TRUE,NULL);
	DocWndInfo.DocFileCur = 0;
	DocWndInfo.DocFileMax = 0;
	DocWndInfo.Text = NULL;
	DocWndInfo.TextSize = 0;
	EnableWindow(GetDlgItem(hDocWnd,IDC_BUTTON_PREV),FALSE);
	EnableWindow(GetDlgItem(hDocWnd,IDC_BUTTON_NEXT),FALSE);
//	if(DocWndInfo.hMutex!=NULL)
//		DocWndInfoUnLock();
}

// Success -> TRUE   Failure -> FALSE 
static int DocWndInfoLock(void)
{
#if 0
	DWORD dwRes;
	if(DocWndInfo.hMutex==NULL)
		return FALSE;
	dwRes = WaitForSingleObject(DocWndInfo.hMutex,10000);
	if(dwRes==WAIT_OBJECT_0	|| dwRes==WAIT_ABANDONED)
		return TRUE;
	else
		return FALSE;
#else
	return TRUE;
#endif
}

static void DocWndInfoUnLock(void)
{
//	ReleaseMutex(DocWndInfo.hMutex);
}

void DocWndInfoReset(void)
{
	if(DocWndInfoLock()==FALSE)
		return;
	DocWndInfo.DocFileCur = 0;
	DocWndInfo.DocFileMax = 0;
	safe_free(DocWndInfo.Text);
	DocWndInfo.Text = NULL;
	DocWndInfo.TextSize = 0;
	DocWndSetInfo("","");
	DocWndSetText("",0);
// end:
	DocWndInfoUnLock();
}

void DocWndAddDocFile(char *filename)
{
	struct timidity_file *tf = open_file(filename,0,0);
#ifdef W32GUI_DEBUG
PrintfDebugWnd("DocWndAddDocFile <- [%s]\n",filename);
#endif
	if(tf==NULL)
		return;
	close_file(tf);
	if(DocWndInfoLock()==FALSE)
		return;
	if(DocWndInfo.DocFileMax>=DOCWND_DOCFILEMAX-1)
		goto end;
	DocWndInfo.DocFileMax++;
	strncpy(DocWndInfo.DocFile[DocWndInfo.DocFileMax-1],filename,FILEPATH_MAX);
	DocWndInfo.DocFile[DocWndInfo.DocFileMax-1][FILEPATH_MAX-1] = '\0';
	if(DocWndInfo.DocFileCur==1)
		EnableWindow(GetDlgItem(hDocWnd,IDC_BUTTON_PREV),FALSE);
	else
		EnableWindow(GetDlgItem(hDocWnd,IDC_BUTTON_PREV),TRUE);
	if(DocWndInfo.DocFileCur==DocWndInfo.DocFileMax)
		EnableWindow(GetDlgItem(hDocWnd,IDC_BUTTON_NEXT),FALSE);
	else
		EnableWindow(GetDlgItem(hDocWnd,IDC_BUTTON_NEXT),TRUE);
#ifdef W32GUI_DEBUG
PrintfDebugWnd("DocWndAddDocFile -> (%d)[%s]\n",DocWndInfo.DocFileMax-1,DocWndInfo.DocFile[DocWndInfo.DocFileMax-1]);
#endif
end:
	DocWndInfoUnLock();
}

void DocWndSetMidifile(char *filename)
{
	char buffer[FILEPATH_MAX];
	char *p;
	if(DocWndInfoLock()==FALSE)
		return;
	strncpy(buffer,filename,FILEPATH_MAX-1);
	buffer[FILEPATH_MAX-1] = '\0';
	p = strrchr(buffer,'.');
	if(p==NULL)
		goto end;
	*p = '\0';
	strcat(buffer,".txt");
	DocWndAddDocFile(buffer);
	*p = '\0';
	strcat(buffer,".doc");
	DocWndAddDocFile(buffer);
	*p = '\0';
	strcat(buffer,".hed");
	DocWndAddDocFile(buffer);
	p = strrchr(buffer,'#');
	if(p==NULL)
		goto end;
	p ++;
	*p = '\0';
	strcat(buffer,"readme.txt");
	DocWndAddDocFile(buffer);
	*p = '\0';
	strcat(buffer,"readme.1st");
	DocWndAddDocFile(buffer);
	*p = '\0';
	strcat(buffer,"歌詞.txt");
	DocWndAddDocFile(buffer);
	p = strrchr(buffer,'\\');
	if(p==NULL)
		goto end;
	p ++;
	*p = '\0';
	strcat(buffer,"readme.txt");
	DocWndAddDocFile(buffer);
	*p = '\0';
	strcat(buffer,"readme.1st");
	DocWndAddDocFile(buffer);
	*p = '\0';
	strcat(buffer,"歌詞.txt");
	DocWndAddDocFile(buffer);
end:
	DocWndInfoUnLock();
}

#define DOCWNDDOCSIZEMAX (64*1024)
void DocWndReadDoc(int num)
{
	struct timidity_file *tf;
	if(DocWndInfoLock()==FALSE)
		return;
	if(num<1)
		num = 1;
	if(num>DocWndInfo.DocFileMax)
		num = DocWndInfo.DocFileMax;
	if(num==DocWndInfo.DocFileCur)
		goto end;
	DocWndInfo.DocFileCur = num;
	tf = open_file(DocWndInfo.DocFile[DocWndInfo.DocFileCur-1],1,10);
	if(tf==NULL)
		goto end;
	safe_free(DocWndInfo.Text);
	DocWndInfo.Text = NULL;
	DocWndInfo.Text = (char *)safe_malloc(sizeof(char)*DOCWNDDOCSIZEMAX);
	DocWndInfo.Text[0] = '\0';
	DocWndInfo.TextSize = tf_read(DocWndInfo.Text,1,DOCWNDDOCSIZEMAX-1,tf);
	DocWndInfo.Text[DocWndInfo.TextSize] = '\0';
	close_file(tf);
	{
		char info[1024];
		char *filename;
		char *p1, *p2, *p3;
		p1 = DocWndInfo.DocFile[DocWndInfo.DocFileCur-1];
		p2 = pathsep_strrchr(p1);
		p3 = strrchr(p1,'#');
		if(p3!=NULL){
			sprintf(info,"(%02d/%02d) %s",DocWndInfo.DocFileCur,DocWndInfo.DocFileMax,p3+1);
			filename = p2 + 1;
		} else if(p2!=NULL){
			sprintf(info,"(%02d/%02d) %s",DocWndInfo.DocFileCur,DocWndInfo.DocFileMax,p2+1);
			filename = p2 + 1;
		} else {
			sprintf(info,"(%02d/%02d) %s",DocWndInfo.DocFileCur,DocWndInfo.DocFileMax,p1+1);
			filename = p1;
		}
		DocWndSetInfo(info,filename);
	}
	DocWndSetText(DocWndInfo.Text,DocWndInfo.TextSize);
end:
	if(DocWndInfo.DocFileCur==1)
		EnableWindow(GetDlgItem(hDocWnd,IDC_BUTTON_PREV),FALSE);
	else
		EnableWindow(GetDlgItem(hDocWnd,IDC_BUTTON_PREV),TRUE);
	if(DocWndInfo.DocFileCur==DocWndInfo.DocFileMax)
		EnableWindow(GetDlgItem(hDocWnd,IDC_BUTTON_NEXT),FALSE);
	else
		EnableWindow(GetDlgItem(hDocWnd,IDC_BUTTON_NEXT),TRUE);
	DocWndInfoUnLock();
}

void DocWndReadDocNext(void)
{
	int num;
	if(DocWndInfoLock()==FALSE)
		return;
	num = DocWndInfo.DocFileCur + 1;
	if(num>DocWndInfo.DocFileMax)
		num = DocWndInfo.DocFileMax;
	DocWndReadDoc(num);
	DocWndInfoUnLock();
}

void DocWndReadDocPrev(void)
{
	int num;
	if(DocWndInfoLock()==FALSE)
		return;
	num = DocWndInfo.DocFileCur - 1;
	if(num<1)
		num = 1;
	DocWndReadDoc(num);
	DocWndInfoUnLock();
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
// List Search Dialog
#define ListSearchStringMax 256
static char ListSearchString[ListSearchStringMax];

LRESULT CALLBACK ListSearchWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
void InitListSearchWnd(HWND hParentWnd)
{
	strcpy(ListSearchString,"");
	if (hListSearchWnd != NULL) {
		DestroyWindow(hListSearchWnd);
		hListSearchWnd = NULL;
	}
	switch(PlayerLanguage){
	case LANGUAGE_JAPANESE:
		hListSearchWnd = CreateDialog
			(hInst,MAKEINTRESOURCE(IDD_DIALOG_ONE_LINE),hParentWnd,ListSearchWndProc);
		break;
	case LANGUAGE_ENGLISH:
	default:
		hListSearchWnd = CreateDialog
			(hInst,MAKEINTRESOURCE(IDD_DIALOG_ONE_LINE_EN),hParentWnd,ListSearchWndProc);
		break;
	}
	ShowWindow(hListSearchWnd,SW_HIDE);
	UpdateWindow(hListSearchWnd);
}

#define ListSearchStringBuffSize 1024*2

LRESULT CALLBACK
ListSearchWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess){
	case WM_INITDIALOG:
		switch(PlayerLanguage){
		case LANGUAGE_JAPANESE:
			SendMessage(hwnd,WM_SETTEXT,0,(LPARAM)_T("プレイリストの検索"));
			SendMessage(GetDlgItem(hwnd,IDC_STATIC_HEAD),WM_SETTEXT,0,(LPARAM)_T("検索キーワードを入れてください。"));
			SendMessage(GetDlgItem(hwnd,IDC_STATIC_TAIL),WM_SETTEXT,0,(LPARAM)_T(""));
			SendMessage(GetDlgItem(hwnd,IDC_BUTTON_1),WM_SETTEXT,0,(LPARAM)_T("検索"));
			SendMessage(GetDlgItem(hwnd,IDC_BUTTON_2),WM_SETTEXT,0,(LPARAM)_T("次を検索"));
			SendMessage(GetDlgItem(hwnd,IDC_BUTTON_3),WM_SETTEXT,0,(LPARAM)_T("閉じる"));
			break;
		case LANGUAGE_ENGLISH:
		default:
			SendMessage(hwnd,WM_SETTEXT,0,(LPARAM)_T("Playlist Search"));
			SendMessage(GetDlgItem(hwnd,IDC_STATIC_HEAD),WM_SETTEXT,0,(LPARAM)_T("Enter search keyword."));
			SendMessage(GetDlgItem(hwnd,IDC_STATIC_TAIL),WM_SETTEXT,0,(LPARAM)_T(""));
			SendMessage(GetDlgItem(hwnd,IDC_BUTTON_1),WM_SETTEXT,0,(LPARAM)_T("SEACH"));
			SendMessage(GetDlgItem(hwnd,IDC_BUTTON_2),WM_SETTEXT,0,(LPARAM)_T("NEXT SEARCH"));
			SendMessage(GetDlgItem(hwnd,IDC_BUTTON_3),WM_SETTEXT,0,(LPARAM)_T("CLOSE"));
			break;
		}
		SetFocus(GetDlgItem(hwnd,IDC_EDIT_ONE_LINE));
		return FALSE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCLOSE:
			ShowWindow(hwnd, SW_HIDE);
			break;
		case IDC_BUTTON_1:
		case IDC_BUTTON_2:
		{
			int selected, nfiles, cursel;
			TCHAR tListSearchString[ListSearchStringMax] = _T("");
			SendDlgItemMessage(hwnd,IDC_EDIT_ONE_LINE,
				WM_GETTEXT,(WPARAM)250,(LPARAM)tListSearchString);
			char *s = tchar_to_char(tListSearchString);
			strncpy(ListSearchString, s, ListSearchStringMax - 1);
			safe_free(s);
			ListSearchString[ListSearchStringMax - 1] = '\0';
			w32g_get_playlist_index(&selected, &nfiles, &cursel);
			if ( LOWORD(wParam) == IDC_BUTTON_2 )
				cursel++;
			if ( strlen ( ListSearchString ) > 0 ) {
				char buff[ListSearchStringBuffSize];
				for ( ; cursel < nfiles; cursel ++ ) {
					int result = SendDlgItemMessage(hListWnd,IDC_LISTBOX_PLAYLIST,
						LB_GETTEXTLEN,(WPARAM)cursel, 0 );
					if ( result < ListSearchStringBuffSize ) {
						result = SendDlgItemMessage(hListWnd,IDC_LISTBOX_PLAYLIST,
							LB_GETTEXT,(WPARAM)cursel,(LPARAM)buff);
						if ( result == LB_ERR ) {
							cursel = LB_ERR;
							break;
						}
						if ( strstr ( buff, ListSearchString ) != NULL ) {
							break;
						}
					} else if ( result == LB_ERR ) {
						cursel = LB_ERR;
						break;
					}
				}
				if ( cursel >= nfiles ) {
					cursel = LB_ERR;
				}
			} else {
				cursel = LB_ERR;
			}
			if ( cursel != LB_ERR ) {
				SendDlgItemMessage(hListWnd,IDC_LISTBOX_PLAYLIST,
					LB_SETCURSEL,(WPARAM)cursel,0);
				SetNumListWnd(cursel,nfiles);
				if ( LOWORD(wParam) == IDC_BUTTON_1 )
					HideListSearch();
			}
		}
			break;
		case IDC_BUTTON_3:
			HideListSearch();
			break;
		default:
			return FALSE;
	}
		break;
	case WM_CLOSE:
		ShowWindow(hListSearchWnd, SW_HIDE);
		break;
	default:
		return FALSE;
	}
	return FALSE;
}

void ShowListSearch(void)
{
	ShowWindow(hListSearchWnd, SW_SHOW);
}
void HideListSearch(void)
{
	ShowWindow(hListSearchWnd, SW_HIDE);
}

//****************************************************************************
// SoundSpec Window

// ---------------------------------------------------------------------------
// variables
SOUNDSPECWNDINFO SoundSpecWndInfo;

// ---------------------------------------------------------------------------
// prototypes of functions
LRESULT CALLBACK SoundSpecWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
#ifdef SUPPORT_SOUNDSPEC
void TargetSpectrogramCanvas(HWND hwnd);
void HandleSpecKeydownEvent(long message, short modifiers);
void UpdateSpectrogramCanvas(void);
#endif

static int SoundSpecWndInfoApply(void);
static int SoundSpecWndInfoReset(HWND hwnd);

// ---------------------------------------------------------------------------
// Global Functions
void InitSoundSpecWnd(HWND hParentWnd)
{
	HICON hIcon;
	if (hSoundSpecWnd != NULL) {
		DestroyWindow(hSoundSpecWnd);
		hSoundSpecWnd = NULL;
	}
	SoundSpecWndInfoReset(NULL);
	INILoadSoundSpecWnd();
	switch(PlayerLanguage){
	case LANGUAGE_JAPANESE:
		hSoundSpecWnd = CreateDialog
			(hInst,MAKEINTRESOURCE(IDD_DIALOG_SOUNDSPEC),hParentWnd,SoundSpecWndProc);
		break;
  	case LANGUAGE_ENGLISH:
 	default:
		hSoundSpecWnd = CreateDialog
			(hInst,MAKEINTRESOURCE(IDD_DIALOG_SOUNDSPEC_EN),hParentWnd,SoundSpecWndProc);
		break;
	}
	SoundSpecWndInfoReset(hSoundSpecWnd);
	ShowWindow(hSoundSpecWnd,SoundSpecWndStartFlag ? SW_SHOW : SW_HIDE);
	UpdateWindow(hSoundSpecWnd);
	hIcon = LoadImage(hInst, MAKEINTRESOURCE(IDI_ICON_TIMIDITY), IMAGE_ICON, 16, 16, 0);
	if (hIcon!=NULL) SendMessage(hSoundSpecWnd,WM_SETICON,FALSE,(LPARAM)hIcon);
	SoundSpecWndInfoApply();
}

LRESULT CALLBACK
SoundSpecWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess){
	case WM_INITDIALOG:
	{
		RECT rc;
#ifdef SUPPORT_SOUNDSPEC
		open_soundspec();
		TargetSpectrogramCanvas(hwnd);
	   	soundspec_update_wave(NULL, 0);
#endif
		SetWindowPosSize(GetDesktopWindow(),hwnd,SoundSpecWndInfo.PosX, SoundSpecWndInfo.PosY );
		GetWindowRect(hwnd,&rc);
		MoveWindow(hwnd,rc.left,rc.top,SoundSpecWndInfo.Width,SoundSpecWndInfo.Height,TRUE);
	}
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
		break;
	case WM_ERASEBKGND:
		return 0;
	case WM_PAINT:
#ifdef SUPPORT_SOUNDSPEC
		UpdateSpectrogramCanvas();
#endif
		break;
	case WM_SIZE:
		InvalidateRect(hwnd, NULL, FALSE);
		return FALSE;
	case WM_MOVE:
	{
		RECT rc;
		GetWindowRect(hwnd,&rc);
		SoundSpecWndInfo.PosX = rc.left;
		SoundSpecWndInfo.PosY = rc.top;
	}
		return FALSE;
	case WM_DESTROY:
	{
		RECT rc;
#ifdef SUPPORT_SOUNDSPEC
		close_soundspec();
#endif
		GetWindowRect(hwnd,&rc);
		SoundSpecWndInfo.Width = rc.right - rc.left;
		SoundSpecWndInfo.Height = rc.bottom - rc.top;
		INISaveSoundSpecWnd();
	}
		break;
	case WM_CLOSE:
		ShowWindow(hSoundSpecWnd, SW_HIDE);
		MainWndUpdateSoundSpecButton();
		break;
	case WM_CHAR:
	{
		int nVirtKey = (int)wParam;
		switch(nVirtKey){
			case 0x48:	// H
			case 0x68:	// h
				if ( PlayerLanguage == LANGUAGE_JAPANESE ){
				MessageBox(hSoundSpecWnd,
					_T("キーコマンド\n")
					_T("スペクトログラムウインドウコマンド\n")
					_T("  ESC: ヘルプを閉じる      H: ヘルプを出す\n")
					_T("  UP: 縦ズームアウト    DOWN: 縦ズームイン\n")
					_T("  LEFT: 横ズームアウト    RIGHT: 横ズームイン\n")
					,_T("ヘルプ"), MB_OK);
				}
				else {
				MessageBox(hSoundSpecWnd,
					_T("Usage of key.\n")
					_T("Sound Spec window command.\n")
					_T("  ESC: Close Help      H: Help\n")
					_T("  UP: Horizontal zoom out    DOWN: Horizontal zoom in\n")
					_T("  LEFT: Vertical zoom out    RIGHT: Vertical zoom in\n")
					,_T("Help"), MB_OK);
				}
				break;
		}
	}
		break;
	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
	{
		int nVirtKey = (int)wParam;
		short nModifiers = (int)lParam & 0xFFFF;
		switch(nVirtKey){
			case VK_ESCAPE:
				SendMessage(hwnd,WM_CLOSE,0,0);
				break;
			default:
#ifdef SUPPORT_SOUNDSPEC
				HandleSpecKeydownEvent(nVirtKey, nModifiers);
#endif
				break;
		}
	}
		break;
	case WM_HELP:
		PostMessage(hwnd, WM_CHAR, 'H', 0);
		break;
	case WM_GETMINMAXINFO:
	{
		LPMINMAXINFO lpmmi = (LPMINMAXINFO) lParam;
		lpmmi->ptMinTrackSize.x = max(192, lpmmi->ptMinTrackSize.x);
		lpmmi->ptMinTrackSize.y = max(100, lpmmi->ptMinTrackSize.y);
	}
		return 0;
	case WM_DROPFILES:
		SendMessage(hMainWnd,WM_DROPFILES,wParam,lParam);
		return 0;
	default:
		return FALSE;
	}
	return FALSE;
}
static int SoundSpecWndInfoReset(HWND hwnd)
{
	if(hwnd==NULL) {
		SoundSpecWndInfo.PosX = - 1;
		SoundSpecWndInfo.PosY = - 1;
		SoundSpecWndInfo.Height = 400;
		SoundSpecWndInfo.Width = 400;
	}
	SoundSpecWndInfo.hwnd = hwnd;
	return 0;
}
static int SoundSpecWndInfoApply(void)
{
	RECT rc;
	GetWindowRect(SoundSpecWndInfo.hwnd,&rc);
	MoveWindow(SoundSpecWndInfo.hwnd,rc.left,rc.top,SoundSpecWndInfo.Width,SoundSpecWndInfo.Height,TRUE);
//	InvalidateRect(hwnd,&rc,FALSE);
//	UpdateWindow(hwnd);
	INISaveSoundSpecWnd();
	return 0;
}

void w32g_open_doc(int close_if_no_doc)
{
	if(close_if_no_doc==1 && DocWndInfo.DocFileMax <= 0)
		ShowSubWindow(hDocWnd, 0);
	else
	{
		DocWndReadDoc(1);
		if(close_if_no_doc!=2)
			ShowSubWindow(hDocWnd, 1);
	}
}

void w32g_setup_doc(int idx)
{
	char *filename;

	DocWndInfoReset();
	if((filename = w32g_get_playlist(idx)) == NULL)
		return;
	DocWndSetMidifile(filename);
}

void w32g_free_doc(void)
{
	safe_free(DocWndInfo.Text);
	DocWndInfo.Text = NULL;
}
