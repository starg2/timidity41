/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999,2000 Masanao Izumo <mo@goice.co.jp>
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

    w32g2_subwin.c: Written by Daisuke Aoki <dai@y7.net>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <stddef.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#if defined(__CYGWIN32__) || defined(__MINGW32__)
#define TPM_TOPALIGN	0x0000L
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
#include <shlobj.h>
#include <commctrl.h>
#include <windowsx.h>
#include "w32g_res.h"
#include "w32g_utl.h"
#include "w32g_pref.h"
#include "w32g_subwin.h"

// ***************************************************************************
//
// Console Window
//
// ***************************************************************************

// ---------------------------------------------------------------------------
// variables
static int ConsoleWndMaxSize = 64 * 1024;
static HFONT hFontConsoleWnd = NULL;

// ---------------------------------------------------------------------------
// prototypes of functions
static BOOL CALLBACK ConsoleWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static void ConsoleWndAllUpdate(void);
static void ConsoleWndVerbosityUpdate(void);
static void ConsoleWndVerbosityApply(void);
static void ConsoleWndValidUpdate(void);
static void ConsoleWndValidApply(void);
static void ConsoleWndVerbosityApplyIncDec(int num);

// ---------------------------------------------------------------------------
// Global Functions

// Initialization
void InitConsoleWnd(HWND hParentWnd)
{
	switch(PlayerLanguage){
  	case LANGUAGE_ENGLISH:
		hConsoleWnd = CreateDialog
  			(hInst,MAKEINTRESOURCE(IDD_DIALOG_CONSOLE_EN),hParentWnd,ConsoleWndProc);
		break;
 	default:
	case LANGUAGE_JAPANESE:
		hConsoleWnd = CreateDialog
  			(hInst,MAKEINTRESOURCE(IDD_DIALOG_CONSOLE),hParentWnd,ConsoleWndProc);
	break;
	}
	ShowWindow(hConsoleWnd,SW_HIDE);
	UpdateWindow(hConsoleWnd);
	ConsoleWndVerbosityApplyIncDec(0);
	CheckDlgButton(hConsoleWnd, IDC_CHECKBOX_VALID, ConsoleWndFlag);
	Edit_LimitText(GetDlgItem(hConsoleWnd,IDC_EDIT), ConsoleWndMaxSize);
}

// Window Procedure
static BOOL CALLBACK
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
	case WM_SIZE:
		ConsoleWndAllUpdate();
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
		ShowSubWindow(hConsoleWnd,0);
//		ShowWindow(hConsoleWnd, SW_HIDE);
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

// puts()
void PutsConsoleWnd(char *str)
{
	HWND hwnd;
	if(!IsWindow(hConsoleWnd) || !ConsoleWndFlag)
		return;
	hwnd = GetDlgItem(hConsoleWnd,IDC_EDIT);
	PutsEditCtlWnd(hwnd,str);
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
	VprintfEditCtlWnd(hwnd,fmt,ap);
	va_end(ap);
}

// Clear
void ClearConsoleWnd(void)
{
	HWND hwnd;
	if(!IsWindow(hConsoleWnd))
		return;
	hwnd = GetDlgItem(hConsoleWnd,IDC_EDIT);
	ClearEditCtlWnd(hwnd);
}

// ---------------------------------------------------------------------------
// Static Functions

static void ConsoleWndAllUpdate(void)
{
	ConsoleWndVerbosityUpdate();
	ConsoleWndValidUpdate();
	Edit_LimitText(GetDlgItem(hConsoleWnd,IDC_EDIT_VERBOSITY),3);
	Edit_LimitText(GetDlgItem(hConsoleWnd,IDC_EDIT),ConsoleWndMaxSize);
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
	char buffer[64];
	HWND hwnd;
	hwnd = GetDlgItem(hConsoleWnd,IDC_EDIT_VERBOSITY);
	if(!IsWindow(hConsoleWnd)) return;
	if(Edit_GetText(hwnd,buffer,60)<=0) return;
	ctl->verbosity = atoi(buffer);
	ConsoleWndVerbosityUpdate();
}

static void ConsoleWndVerbosityApplyIncDec(int num)
{
	if(!IsWindow(hConsoleWnd)) return;
	ctl->verbosity += num;
	RANGE(ctl->verbosity, -1, 4);
	ConsoleWndVerbosityUpdate();
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

// ---------------------------------------------------------------------------
// Variables
static HANDLE hListWndPopupMenu;

// ---------------------------------------------------------------------------
// Prototypes
static BOOL CALLBACK ListWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static int ResetListWnd(void);
static int ClearListWnd(void);
static int UniqListWnd(void);
static int RefineListWnd(void);
static int DelListWnd(int nth);

// ---------------------------------------------------------------------------
// Grobal Functions

void InitListWnd(HWND hParentWnd)
{
	switch(PlayerLanguage){
	case LANGUAGE_ENGLISH:
		hListWnd = CreateDialog
			(hInst,MAKEINTRESOURCE(IDD_DIALOG_SIMPLE_LIST_EN),hParentWnd,ListWndProc);
		break;
	default:
	case LANGUAGE_JAPANESE:
		hListWnd = CreateDialog
			(hInst,MAKEINTRESOURCE(IDD_DIALOG_SIMPLE_LIST),hParentWnd,ListWndProc);
		break;
	}
	hListWndPopupMenu = CreatePopupMenu();
	AppendMenu(hListWndPopupMenu,MF_STRING,IDM_LISTWND_PLAY,"Play");
	AppendMenu(hListWndPopupMenu,MF_STRING,IDM_LISTWND_REMOVE,"Remove");
	ShowWindow(hListWnd,SW_HIDE);
	UpdateWindow(hListWnd);
	w32g_send_rc(RC_EXT_UPDATE_PLAYLIST, 0);
}

// ---------------------------------------------------------------------------
// Static Functions

static BOOL CALLBACK
ListWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess){
	case WM_INITDIALOG:
		SendDlgItemMessage(hwnd,IDC_LISTBOX_PLAYLIST,
			LB_SETHORIZONTALEXTENT,(WPARAM)1024,0);
		w32g_send_rc(RC_EXT_UPDATE_PLAYLIST, 0);
		return FALSE;
		/* マウス入力がキャプチャされていないための処理 */
	case WM_SETCURSOR:
		switch(HIWORD(lParam)){
		case WM_RBUTTONDOWN:
			{
				POINT point;
				GetCursorPos(&point);
				TrackPopupMenu(hListWndPopupMenu,TPM_TOPALIGN|TPM_LEFTALIGN,
					point.x,point.y,0,hwnd,NULL);
			}
			break;
		default:
			break;
		}
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
			default:
				break;
			}
			switch (LOWORD(wParam)) {
			case IDC_BUTTON_CLEAR:
				if(MessageBox(hListWnd,"Clear playlist?","Playlist",
							  MB_YESNO)==IDYES)
					w32g_send_rc(RC_EXT_CLEAR_PLAYLIST, 0);
				return FALSE;
			case IDC_BUTTON_REFINE:
				if(MessageBox(hListWnd,
							  "Remove unsupported file types from the playlist?",
							  "Playlist",MB_YESNO) == IDYES)
					w32g_send_rc(RC_EXT_REFINE_PLAYLIST, 0);
				return FALSE;
			case IDC_BUTTON_UNIQ:
				if(MessageBox(hListWnd,
							  "Remove the same files from the playlist and make files of the playlist unique?",
							  "Playlist",MB_YESNO)==IDYES)
					w32g_send_rc(RC_EXT_UNIQ_PLAYLIST, 0);
				return FALSE;
			case IDM_LISTWND_REMOVE:
				w32g_send_rc(RC_EXT_DELETE_PLAYLIST, 0);
				break;
			case IDC_BUTTON_DOC: {
					int cursel;
					w32g_get_playlist_index(NULL, NULL, &cursel);
					w32g_send_rc(RC_EXT_OPEN_DOC, cursel);
				}
				break;
			case IDM_LISTWND_PLAY:
				{
					HWND hListBox = GetDlgItem(hListWnd, IDC_LISTBOX_PLAYLIST);
					if(hListBox)
						w32g_send_rc(RC_EXT_JUMP_FILE, ListBox_GetCurSel(hListBox));
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
					int num;
					switch(vkey){
					case VK_SPACE:
					case VK_RETURN:
						w32g_send_rc(RC_EXT_JUMP_FILE, nCaretPos);
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
						if(MessageBox(hListWnd,"Quit TiMidity?","TiMidity",MB_ICONQUESTION|MB_YESNO)==IDYES)
							SendMessage(hMainWnd,WM_CLOSE,0,0);
						return -2;
					case VK_BACK:
						w32g_send_rc(RC_EXT_DELETE_PLAYLIST, -1);
					case 0x44:	// VK_D
						w32g_send_rc(RC_EXT_DELETE_PLAYLIST, 0);
						return -2;
					case VK_DELETE:
						w32g_send_rc(RC_EXT_ROTATE_PLAYLIST, -1);
						return -2;
					case VK_INSERT:
						w32g_send_rc(RC_EXT_ROTATE_PLAYLIST, 1);
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
					case VK_F1:
					case 0x48:	// VK_H
						MessageBox(hListWnd,
							"Usage of key.\n"
							"List window command.\n"
							"  ESC: Close Help      H: Help\n"
							"Player command.\n"
							"  SPACE/ENTER: PLAY    E: Stop    S: Pause\n"
							"  P: Prev    N: Next\n"
							"Playlist command.\n"
							"  M: Refine playlist    U: Uniq playlist\n"
							"  C: Clear playlist\n"
							"  D: Remove playlist    BS: Remove previous playlist\n"
							"  INS: Push Playlist    DEL: Pop Playlist\n"
							"TiMidity command.\n"
							"  Q: Quit\n"
							,"Help", MB_OK);
						return -2;
					default:
						break;
			}
			return -1;
		}
		case WM_SIZE:
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
		ShowSubWindow(hListWnd,0);
//		ShowWindow(hListWnd, SW_HIDE);
		MainWndUpdateListButton();
		break;
	case WM_DROPFILES:
		SendMessage(hMainWnd,WM_DROPFILES,wParam,lParam);
		return 0;
	default:
		return FALSE;
	}
	return FALSE;
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

#define DOCWND_DOCFILEMAX 10
static struct DocWndInfo_ {
	char DocFile[DOCWND_DOCFILEMAX][MAXPATH];
	int DocFileMax;
	int DocFileCur;
	char *Text;
	int TextSize;
//	HANDLE hMutex;
} DocWndInfo;
int DocWndIndependent = 0; /* Independent document viewer mode.(独立ドキュメントビュワーモード) */

static BOOL CALLBACK DocWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
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

void InitDocWnd(HWND hParentWnd)
{
	switch(PlayerLanguage){
  	case LANGUAGE_ENGLISH:
		hDocWnd = CreateDialog
			(hInst,MAKEINTRESOURCE(IDD_DIALOG_DOC_EN),hParentWnd,DocWndProc);
		break;
 	default:
	case LANGUAGE_JAPANESE:
		hDocWnd = CreateDialog
			(hInst,MAKEINTRESOURCE(IDD_DIALOG_DOC),hParentWnd,DocWndProc);
	break;
	}
//	hDocWnd = CreateDialog
//		(hInst,MAKEINTRESOURCE(IDD_DIALOG_DOC),hParentWnd,DocWndProc);
	ShowWindow(hDocWnd,SW_HIDE);
	UpdateWindow(hDocWnd);
}

static BOOL CALLBACK
DocWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess){
	case WM_INITDIALOG:
		PutsDocWnd("Doc Window\n");
		DocWndInfoInit();
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
	default:
		return FALSE;
	}
	return FALSE;
}

static char ControlCode[] = "@ABCDEFGHIJKLMNOPQRS";
static void DocWndConvertText(char *in, int in_size, char *out, int out_size)
{
	char *buffer = (char *)malloc(sizeof(char)*out_size);
	int buffer_size = out_size;
	int i=0, j=0;
	int nl = 0;

// Convert Return Code CR, LF -> CR+LF ,
//         Control Code -> ^? (^@, ^A, ^B, ...).
stage1:
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
stage2:
#ifndef MAX2
#define MAX2(x,y) ((x)>=(y)?(x):(y))
#endif
	switch(PlayerLanguage){
  	case LANGUAGE_ENGLISH:
 	default:
		strncpy(out,buffer,MAX2(buffer_size-1,out_size-1));
		out[out_size-1] = '\0';
		break;
	case LANGUAGE_JAPANESE:
		strncpy(out,buffer,MAX2(buffer_size-1,out_size-1));
		nkf_convert(buffer,out,out_size-1,NULL,"SJIS");
		out[out_size-1] = '\0';
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
	Edit_SetText(GetDlgItem(hDocWnd,IDC_EDIT),buffer);
	DocWndInfoUnLock();
}

static void DocWndSetInfo(char *info, char *filename)
{
	char buffer[BUFFER_SIZE];
	int buffer_size = BUFFER_SIZE;
	if(!IsWindow(hDocWnd) || !DocWndFlag)
		return;
	if(DocWndInfoLock()==FALSE)
		return;
	Edit_SetText(GetDlgItem(hDocWnd,IDC_EDIT_INFO),info);
	Edit_SetText(GetDlgItem(hDocWnd,IDC_EDIT_FILENAME),filename);
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
	if(DocWndInfo.Text!=NULL){
		free(DocWndInfo.Text);
		DocWndInfo.Text = NULL;
	}
	DocWndInfo.TextSize = 0;
	DocWndSetInfo("","");
	DocWndSetText("",0);
end:
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
	strncpy(DocWndInfo.DocFile[DocWndInfo.DocFileMax-1],filename,MAXPATH);
	DocWndInfo.DocFile[DocWndInfo.DocFileMax-1][MAXPATH-1] = '\0';
#ifdef W32GUI_DEBUG
PrintfDebugWnd("DocWndAddDocFile -> (%d)[%s]\n",DocWndInfo.DocFileMax-1,DocWndInfo.DocFile[DocWndInfo.DocFileMax-1]);
#endif
end:
	DocWndInfoUnLock();
}

void DocWndSetMidifile(char *filename)
{
	char buffer[MAXPATH+10];
	char *p;
	if(DocWndInfoLock()==FALSE)
		return;
	strncpy(buffer,filename,MAXPATH-1);
	buffer[MAXPATH-1] = '\0';
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
	if(DocWndInfo.Text!=NULL){
		free(DocWndInfo.Text);
		DocWndInfo.Text = NULL;
	}
	DocWndInfo.Text = (char *)malloc(sizeof(char)*DOCWNDDOCSIZEMAX);
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

void w32g_open_doc(int close_if_no_doc)
{
	if(close_if_no_doc && DocWndInfo.DocFileMax <= 0)
		ShowSubWindow(hDocWnd, 0);
	else
	{
		DocWndReadDoc(1);
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
