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

	w32g2_pref.c: Written by Daisuke Aoki <dai@y7.net>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <stddef.h>
#include <windows.h>
// #include <prsht.h>
#if defined(__CYGWIN32__) || defined(__MINGW32__)
#include <commdlg.h>
#else
#include <commctrl.h>
#endif /* __CYGWIN32__ */
#include <commctrl.h>
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
#include "w32g_res.h"
#include "w32g_utl.h"
#include "w32g_pref.h"

/* TiMidity Win32GUI preference / PropertySheet */

volatile int PrefWndDoing = 0;

static volatile int PrefWndSetOK = 0;
static HPROPSHEETPAGE hPrefWnd;
static int CALLBACK PrefWndPropSheetProc(HWND hwndDlg, UINT uMsg, LPARAM lParam);
static BOOL APIENTRY PrefPlayerDialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static BOOL APIENTRY PrefTiMidity1DialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static BOOL APIENTRY PrefTiMidity2DialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static BOOL APIENTRY PrefTiMidity3DialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static BOOL APIENTRY PrefTiMidity4DialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static int DlgOpenConfigFile(char *Filename, HWND hwnd);
static int DlgOpenOutputFile(char *Filename, HWND hwnd);

#if defined(__CYGWIN32__) || defined(__MINGW32__)
//#define pszTemplate	u1.pszTemplate
//#define pszIcon		u2.pszIcon
#define NONAMELESSUNION
#define DUMMYUNIONNAME	u1
#define DUMMYUNIONNAME2	u2
#define DUMMYUNIONNAME3	u3
#endif

#define PREFWND_NPAGES 5
void PrefWndCreate(HWND hwnd)
{
	int res;
	PROPSHEETPAGE psp[PREFWND_NPAGES];
	PROPSHEETHEADER psh;

	VOLATILE_TOUCH(PrefWndDoing);
	if(PrefWndDoing)
		return;
	PrefWndDoing = 1;
	PrefWndSetOK = 1;
//	Player page.
	psp[0].dwSize = sizeof(PROPSHEETPAGE);
	psp[0].dwFlags = PSP_USETITLE;
	psp[0].hInstance = hInst;
#if defined (__cplusplus)
	psp[0].pszTemplate = MAKEINTRESOURCE(IDD_PREF_PLAYER);
	psp[0].pszIcon = NULL;
#else
#ifdef NONAMELESSUNION
	psp[0].DUMMYUNIONNAME.pszTemplate = MAKEINTRESOURCE(IDD_PREF_PLAYER);
	psp[0].DUMMYUNIONNAME2.pszIcon = NULL;
#else
	psp[0].pszTemplate = MAKEINTRESOURCE(IDD_PREF_PLAYER);
	psp[0].pszIcon = NULL;
#endif
#endif
	psp[0].pfnDlgProc = PrefPlayerDialogProc;
	psp[0].pszTitle = (LPSTR)TEXT("Player");
	psp[0].lParam = 0;
	psp[0].pfnCallback = NULL;
// TiMidity page1.
	psp[1].dwSize = sizeof(PROPSHEETPAGE);
	psp[1].dwFlags = PSP_USETITLE;
	psp[1].hInstance = hInst;
#if defined (__cplusplus)
	psp[1].pszTemplate = MAKEINTRESOURCE(IDD_PREF_TIMIDITY1);
	psp[1].pszIcon = NULL;
#else
#ifdef NONAMELESSUNION
	psp[1].DUMMYUNIONNAME.pszTemplate = MAKEINTRESOURCE(IDD_PREF_TIMIDITY1);
	psp[1].DUMMYUNIONNAME2.pszIcon = NULL;
#else
	psp[1].pszTemplate = MAKEINTRESOURCE(IDD_PREF_TIMIDITY1);
	psp[1].pszIcon = NULL;
#endif
#endif
	psp[1].pfnDlgProc = PrefTiMidity1DialogProc;
	psp[1].pszTitle = (LPSTR)TEXT("Effect");
	psp[1].lParam = 0;
	psp[1].pfnCallback = NULL;
// TiMidity page2.
	psp[2].dwSize = sizeof(PROPSHEETPAGE);
	psp[2].dwFlags = PSP_USETITLE;
	psp[2].hInstance = hInst;
#if defined (__cplusplus)
	psp[2].pszTemplate = MAKEINTRESOURCE(IDD_PREF_TIMIDITY2);
	psp[2].pszIcon = NULL;
#else
#ifdef NONAMELESSUNION
	psp[2].DUMMYUNIONNAME.pszTemplate = MAKEINTRESOURCE(IDD_PREF_TIMIDITY2);
	psp[2].DUMMYUNIONNAME2.pszIcon = NULL;
#else
	psp[2].pszTemplate = MAKEINTRESOURCE(IDD_PREF_TIMIDITY2);
	psp[2].pszIcon = NULL;
#endif
#endif
	psp[2].pfnDlgProc = PrefTiMidity2DialogProc;
	psp[2].pszTitle = (LPSTR)TEXT("Misc");
	psp[2].lParam = 0;
	psp[2].pfnCallback = NULL;
// TiMidity page3.
	psp[3].dwSize = sizeof(PROPSHEETPAGE);
	psp[3].dwFlags = PSP_USETITLE;
	psp[3].hInstance = hInst;
#if defined (__cplusplus)
	psp[3].pszTemplate = MAKEINTRESOURCE(IDD_PREF_TIMIDITY3);
	psp[3].pszIcon = NULL;
#else
#ifdef NONAMELESSUNION
	psp[3].DUMMYUNIONNAME.pszTemplate = MAKEINTRESOURCE(IDD_PREF_TIMIDITY3);
	psp[3].DUMMYUNIONNAME2.pszIcon = NULL;
#else
	psp[3].pszTemplate = MAKEINTRESOURCE(IDD_PREF_TIMIDITY3);
	psp[3].pszIcon = NULL;
#endif
#endif
	psp[3].pfnDlgProc = PrefTiMidity3DialogProc;
	psp[3].pszTitle = (LPSTR)TEXT("Output");
	psp[3].lParam = 0;
	psp[3].pfnCallback = NULL;
// TiMidity page4.
	psp[4].dwSize = sizeof(PROPSHEETPAGE);
	psp[4].dwFlags = PSP_USETITLE;
	psp[4].hInstance = hInst;
#if defined (__cplusplus)
	psp[4].pszTemplate = MAKEINTRESOURCE(IDD_PREF_TIMIDITY4);
	psp[4].pszIcon = NULL;
#else
#ifdef NONAMELESSUNION
	psp[4].DUMMYUNIONNAME.pszTemplate = MAKEINTRESOURCE(IDD_PREF_TIMIDITY4);
	psp[4].DUMMYUNIONNAME2.pszIcon = NULL;
#else
	psp[4].pszTemplate = MAKEINTRESOURCE(IDD_PREF_TIMIDITY4);
	psp[4].pszIcon = NULL;
#endif
#endif
	psp[4].pfnDlgProc = PrefTiMidity4DialogProc;
	psp[4].pszTitle = (LPSTR)TEXT("Channel");
	psp[4].lParam = 0;
	psp[4].pfnCallback = NULL;
// Propsheetheader
	psh.dwSize = sizeof(PROPSHEETHEADER);
//	  psh.dwFlags = PSH_USEHICON | PSH_PROPSHEETPAGE | PSH_USECALLBACK | PSH_NOAPPLYNOW;
	psh.dwFlags = PSH_USEHICON | PSH_PROPSHEETPAGE | PSH_USECALLBACK;
	psh.hwndParent = hwnd;
	psh.hInstance = hInst;
	psh.pszCaption = (LPSTR)TEXT("TiMidity Win32GUI Preference");
	psh.nPages = sizeof(psp) / sizeof(PROPSHEETPAGE);
#if defined (__cplusplus)
	psh.nStartPage = 0;
	psh.ppsp = (LPCPROPSHEETPAGE)&psp;
#else
#ifdef NONAMELESSUNION
	psh.DUMMYUNIONNAME.hIcon = NULL;
	psh.DUMMYUNIONNAME2.nStartPage = 0;
	psh.DUMMYUNIONNAME3.ppsp = (LPCPROPSHEETPAGE)&psp;
#else
	psh.hIcon = NULL;
	psh.nStartPage = 0;
	psh.ppsp = (LPCPROPSHEETPAGE)&psp;
#endif
#endif
	psh.pfnCallback = PrefWndPropSheetProc;

	res = PropertySheet(&psh);

	PrefWndSetOK = 0;
	PrefWndDoing = 0;
	return;
}

static HFONT hFontPrefWnd = NULL;
#define PREFWND_XSIZE	240*3
#define PREFWND_YSIZE	180*4
static int CALLBACK PrefWndPropSheetProc(HWND hwnd, UINT uMsg, LPARAM lParam)
{
	if(uMsg==PSCB_INITIALIZED){
		PrefWndSetOK = 1;
		hPrefWnd = (HPROPSHEETPAGE)hwnd;
		PropSheet_Changed(hwnd,0);
		if(hFontPrefWnd==NULL){
			char FontLang[256];
			switch(PlayerLanguage){
			  case LANGUAGE_ENGLISH:
				strcpy(FontLang,"Times New Roman");
				break;
			  default:
			  case LANGUAGE_JAPANESE:
				strcpy(FontLang,"ＭＳ Ｐ明朝");
				break;
			}
			hFontPrefWnd = CreateFont(18,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,
									  DEFAULT_CHARSET,
									  OUT_DEFAULT_PRECIS,
									  CLIP_DEFAULT_PRECIS,
									  DEFAULT_QUALITY,
									  DEFAULT_PITCH | FF_DONTCARE,FontLang);
		}
	}
	return 0;
}

#define DLG_CHECKBUTTON_TO_FLAG(hwnd,ctlid,x)	\
	((SendDlgItemMessage((hwnd),(ctlid),BM_GETCHECK,0,0))?((x)=1):((x)=0))
#define DLG_FLAG_TO_CHECKBUTTON(hwnd,ctlid,x)	\
	((x)?(SendDlgItemMessage((hwnd),(ctlid),BM_SETCHECK,1,0)):\
	(SendDlgItemMessage((hwnd),(ctlid),BM_SETCHECK,0,0)))


/*
	プロパティシート
	WM_NOTIFY の PSN_KILLACTIVE
		ページが非アクティブになる時
		カレントページでプロパティシートが OK or CLOSE される時
	WM_NOTIFY の PSN_SETACTIVE
		ページがアクティブになる時
	WM_NOTIFY の PSN_RESET
		キャンセルされたとき
		PrefWndSetOK = 0 とする
*/

/* st_temp, sp_temp を適用する
 * 注意: MainThread からの呼び出し禁止、危険！
 */
void PrefSettingApplyReally(void)
{
	int restart;
	extern int IniFileAutoSave;

	free_instruments(1);
	if(play_mode->fd != -1)
		play_mode->close_output();

	restart = (PlayerLanguage != sp_temp->PlayerLanguage);
	if(sp_temp->PlayerLanguage == LANGUAGE_JAPANESE)
		strcpy(st_temp->output_text_code, "SJIS");
	else
		strcpy(st_temp->output_text_code, "ASCII");
	ApplySettingPlayer(sp_temp);
	ApplySettingTiMidity(st_temp);
	SaveSettingPlayer(sp_current);
	SaveSettingTiMidity(st_current);
	memcpy(sp_temp, sp_current, sizeof(SETTING_PLAYER));
	memcpy(st_temp, st_current, sizeof(SETTING_TIMIDITY));
	restore_voices(1);
	PrefWndSetOK = 0;
	if(IniFileAutoSave)
		SaveIniFile(sp_current, st_current);
	if(restart &&
	   MessageBox(hListWnd,"Restart TiMidity?", "TiMidity",
				  MB_YESNO)==IDYES)
	{
		if(hFontPrefWnd)
		{
			DeleteObject(hFontPrefWnd);
			hFontPrefWnd = 0;
		}

		w32g_restart();
		PrefWndDoing = 0;
	}
}

static void PrefSettingApply(void)
{
	 w32g_send_rc(RC_EXT_APPLY_SETTING, 0);
}

static BOOL APIENTRY
PrefPlayerDialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess){
   case WM_INITDIALOG:
//		SendMessage(hwnd,WM_SETFONT,(WPARAM)hFontPrefWnd,MAKELPARAM(TRUE,0));
		SendDlgItemMessage(hwnd,IDC_EDIT_CONFIG_FILE,
		WM_SETFONT,(WPARAM)hFontPrefWnd,MAKELPARAM(TRUE,0));
		break;
	case WM_COMMAND:
	switch (LOWORD(wParam)) {
		case IDC_BUTTON_CONFIG_FILE:
			{
				char filename[MAXPATH+1];
				filename[0] = '\0';
				SendDlgItemMessage(hwnd,IDC_EDIT_CONFIG_FILE,WM_GETTEXT,
					(WPARAM)MAX_PATH,(LPARAM)TEXT(filename));
				if(!DlgOpenConfigFile(filename,hwnd))
				if(filename[0]!='\0')
						SetDlgItemText(hwnd,IDC_EDIT_CONFIG_FILE,TEXT(filename));
	   }
			break;
		case IDC_RADIOBUTTON_JAPANESE:
		case IDC_RADIOBUTTON_ENGLISH:
			break;
		default:
		break;
	  }
		PrefWndSetOK = 1;
		PropSheet_Changed((HWND)hPrefWnd,hwnd);
		break;
	case WM_NOTIFY:
		switch (((NMHDR FAR *) lParam)->code){
		case PSN_KILLACTIVE:
		SendDlgItemMessage(hwnd,IDC_EDIT_CONFIG_FILE,WM_GETTEXT,
			(WPARAM)MAX_PATH,(LPARAM)TEXT(sp_temp->ConfigFile));
			if(SendDlgItemMessage(hwnd,IDC_RADIOBUTTON_ENGLISH,BM_GETCHECK,0,0)){
				sp_temp->PlayerLanguage = LANGUAGE_ENGLISH;
			} else if(SendDlgItemMessage(hwnd,IDC_RADIOBUTTON_JAPANESE,BM_GETCHECK,0,0)){
				sp_temp->PlayerLanguage = LANGUAGE_JAPANESE;
			}
		 {
		 int flag;

			SettingCtlFlag(st_temp, 'x',
								DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_AUTOQUIT,flag));
			SettingCtlFlag(st_temp, 'u',
								DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_AUTOUNIQ,flag));
			SettingCtlFlag(st_temp, 'R',
								DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_AUTOREFINE,flag));
			SettingCtlFlag(st_temp, 'C',
								DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_NOT_CONTINUE,flag));
			SettingCtlFlag(st_temp, 'd',
								!DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_NOT_DRAG_START,flag));
			SettingCtlFlag(st_temp, 'l',
								!DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_NOT_LOOPING,flag));
			}
			DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECK_SEACHDIRRECURSIVE,
				sp_temp->SeachDirRecursive);
			DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECK_DOCWNDINDEPENDENT,
				sp_temp->DocWndIndependent);
			DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECK_INIFILE_AUTOSAVE,
				sp_temp->IniFileAutoSave);
			SetWindowLong(hwnd,DWL_MSGRESULT,FALSE);
			return TRUE;
		case PSN_RESET:
			PrefWndSetOK = 0;
			SetWindowLong(hwnd,	DWL_MSGRESULT, FALSE);
			break;
		case PSN_APPLY:
			PrefSettingApply();
			PropSheet_UnChanged(hPrefWnd,hwnd);
			break;
		case PSN_SETACTIVE:
			SetDlgItemText(hwnd,IDC_EDIT_CONFIG_FILE,TEXT(sp_temp->ConfigFile));
			switch(sp_temp->PlayerLanguage){
		 case LANGUAGE_ENGLISH:
				CheckRadioButton(hwnd,IDC_RADIOBUTTON_JAPANESE,IDC_RADIOBUTTON_ENGLISH,
				IDC_RADIOBUTTON_ENGLISH);
				break;
			default:
		 case LANGUAGE_JAPANESE:
				CheckRadioButton(hwnd,IDC_RADIOBUTTON_JAPANESE,IDC_RADIOBUTTON_ENGLISH,
				IDC_RADIOBUTTON_JAPANESE);
				break;
			}
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_AUTOQUIT,
									strchr(st_temp->opt_ctl + 1, 'x'));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_AUTOUNIQ,
									strchr(st_temp->opt_ctl + 1, 'u'));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_AUTOREFINE,
									strchr(st_temp->opt_ctl + 1, 'R'));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_NOT_CONTINUE,
									strchr(st_temp->opt_ctl + 1, 'C'));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_NOT_DRAG_START,
									!strchr(st_temp->opt_ctl + 1, 'd'));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_NOT_LOOPING,
									!strchr(st_temp->opt_ctl + 1, 'l'));

			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECK_SEACHDIRRECURSIVE,
									sp_temp->SeachDirRecursive);
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECK_DOCWNDINDEPENDENT,
									sp_temp->DocWndIndependent);
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECK_INIFILE_AUTOSAVE,
									sp_temp->IniFileAutoSave);
			break;
		default:
			return FALSE;
		}
   case WM_SIZE:
		return FALSE;
	case WM_CLOSE:
		break;
	default:
	  break;
	}
	return FALSE;
}

static BOOL APIENTRY
PrefTiMidity1DialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess){
	case WM_INITDIALOG:
		break;
	case WM_COMMAND:
	switch (LOWORD(wParam)) {
	  case IDCLOSE:
		break;
		case IDC_CHECKBOX_CHORUS:
		case IDC_CHECKBOX_CHORUS_LEVEL:
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_CHORUS,BM_GETCHECK,0,0)){
				EnableWindow(GetDlgItem(hwnd,IDC_CHECKBOX_CHORUS_LEVEL),TRUE);
				if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_CHORUS_LEVEL,BM_GETCHECK,0,0)){
					EnableWindow(GetDlgItem(hwnd,IDC_EDIT_CHORUS),TRUE);
			} else {
					EnableWindow(GetDlgItem(hwnd,IDC_EDIT_CHORUS),FALSE);
				}
			} else {
				EnableWindow(GetDlgItem(hwnd,IDC_CHECKBOX_CHORUS_LEVEL),FALSE);
				EnableWindow(GetDlgItem(hwnd,IDC_EDIT_CHORUS),FALSE);
			}
			break;
		case IDC_CHECKBOX_REVERB:
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_REVERB,BM_GETCHECK,0,0)){
				EnableWindow(GetDlgItem(hwnd,IDC_CHECKBOX_GLOBAL_REVERB),TRUE);
				EnableWindow(GetDlgItem(hwnd,IDC_CHECKBOX_REVERB_LEVEL),TRUE);
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_REVERB_LEVEL,BM_GETCHECK,0,0)){
					EnableWindow(GetDlgItem(hwnd,IDC_EDIT_REVERB),TRUE);
			} else {
					EnableWindow(GetDlgItem(hwnd,IDC_EDIT_REVERB),FALSE);
				}
			} else {
				EnableWindow(GetDlgItem(hwnd,IDC_CHECKBOX_GLOBAL_REVERB),FALSE);
				EnableWindow(GetDlgItem(hwnd,IDC_CHECKBOX_REVERB_LEVEL),FALSE);
				EnableWindow(GetDlgItem(hwnd,IDC_EDIT_REVERB),FALSE);
			}
			break;
		case IDC_CHECKBOX_GLOBAL_REVERB:
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_GLOBAL_REVERB,BM_GETCHECK,0,0)){
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_REVERB_LEVEL,BM_SETCHECK,0,0);
			}
			SendMessage(hwnd,WM_COMMAND,IDC_CHECKBOX_REVERB,0);
			break;
		case IDC_CHECKBOX_REVERB_LEVEL:
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_REVERB_LEVEL,BM_GETCHECK,0,0)){
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_GLOBAL_REVERB,BM_SETCHECK,0,0);
			}
			SendMessage(hwnd,WM_COMMAND,IDC_CHECKBOX_REVERB,0);
			break;
	 case IDC_CHECKBOX_DELAY:
		case IDC_RADIOBUTTON_DELAY_LEFT:
		case IDC_RADIOBUTTON_DELAY_RIGHT:
		case IDC_RADIOBUTTON_DELAY_CENTER:
			if(!SendDlgItemMessage(hwnd,IDC_RADIOBUTTON_DELAY_LEFT,BM_GETCHECK,0,0))
			if(!SendDlgItemMessage(hwnd,IDC_RADIOBUTTON_DELAY_RIGHT,BM_GETCHECK,0,0))
			if(!SendDlgItemMessage(hwnd,IDC_RADIOBUTTON_DELAY_CENTER,BM_GETCHECK,0,0))
				CheckRadioButton(hwnd,IDC_RADIOBUTTON_DELAY_LEFT,IDC_RADIOBUTTON_DELAY_CENTER,IDC_RADIOBUTTON_DELAY_CENTER);
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_DELAY,BM_GETCHECK,0,0)){
				EnableWindow(GetDlgItem(hwnd,IDC_RADIOBUTTON_DELAY_LEFT),TRUE);
				EnableWindow(GetDlgItem(hwnd,IDC_RADIOBUTTON_DELAY_RIGHT),TRUE);
				EnableWindow(GetDlgItem(hwnd,IDC_RADIOBUTTON_DELAY_CENTER),TRUE);
				EnableWindow(GetDlgItem(hwnd,IDC_EDIT_DELAY),TRUE);
		 } else {
				EnableWindow(GetDlgItem(hwnd,IDC_RADIOBUTTON_DELAY_LEFT),FALSE);
				EnableWindow(GetDlgItem(hwnd,IDC_RADIOBUTTON_DELAY_RIGHT),FALSE);
				EnableWindow(GetDlgItem(hwnd,IDC_RADIOBUTTON_DELAY_CENTER),FALSE);
				EnableWindow(GetDlgItem(hwnd,IDC_EDIT_DELAY),FALSE);
		 }
			break;
		default:
		PrefWndSetOK = 1;
		return FALSE;
		break;
	  }
		PrefWndSetOK = 1;
		PropSheet_Changed((HWND)hPrefWnd,hwnd);
		break;
	case WM_NOTIFY:
		switch (((NMHDR FAR *) lParam)->code){
		case PSN_KILLACTIVE:
			// CHORUS
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_CHORUS,BM_GETCHECK,0,0)){
				if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_CHORUS_LEVEL,BM_GETCHECK,0,0)){
					st_temp->opt_chorus_control = -GetDlgItemInt(hwnd,IDC_EDIT_CHORUS,NULL,TRUE);
				} else {
					st_temp->opt_chorus_control = 1;
				}
			} else {
				st_temp->opt_chorus_control = 0;
			}
		 // REVERB
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_REVERB,BM_GETCHECK,0,0)){
				if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_GLOBAL_REVERB,BM_GETCHECK,0,0)){
					st_temp->opt_reverb_control = 2;
				} else if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_REVERB_LEVEL,BM_GETCHECK,0,0)){
					st_temp->opt_reverb_control = -GetDlgItemInt(hwnd,IDC_EDIT_REVERB,NULL,TRUE);
				} else {
					st_temp->opt_reverb_control = 1;
				}
			} else {
				st_temp->opt_reverb_control = 0;
			}
		 // DELAY
			st_temp->effect_lr_delay_msec = GetDlgItemInt(hwnd,IDC_EDIT_DELAY,NULL,FALSE);
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_DELAY,BM_GETCHECK,0,0)){
				if(SendDlgItemMessage(hwnd,IDC_RADIOBUTTON_DELAY_LEFT,BM_GETCHECK,0,0)){
					st_temp->effect_lr_mode = 0;
				} else if(SendDlgItemMessage(hwnd,IDC_RADIOBUTTON_DELAY_RIGHT,BM_GETCHECK,0,0)){
					st_temp->effect_lr_mode = 1;
				} else {
					st_temp->effect_lr_mode = 2;
			}
			} else {
				st_temp->effect_lr_mode = -1;
		 }
			// NOISESHARPING
		 st_temp->noise_sharp_type = GetDlgItemInt(hwnd,IDC_EDIT_NOISESHARPING,NULL,FALSE);
			// Misc
			DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_MODWHEEL,st_temp->opt_modulation_wheel);
			DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_PORTAMENTO,st_temp->opt_portamento);
			DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_NRPNVIB,st_temp->opt_nrpn_vibrato);
			DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_CHPRESS,st_temp->opt_channel_pressure);
			DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_OVOICE,st_temp->opt_overlap_voice_allow);
			DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_TRACETEXT,st_temp->opt_trace_text_meta_event);
		 st_temp->modify_release = GetDlgItemInt(hwnd,IDC_EDIT_MODIFY_RELEASE,NULL,FALSE);
			SetWindowLong(hwnd,DWL_MSGRESULT,FALSE);
			return TRUE;
		case PSN_RESET:
			PrefWndSetOK = 0;
			SetWindowLong(hwnd,	DWL_MSGRESULT, FALSE);
			break;
		case PSN_APPLY:
			PrefSettingApply();
			break;
		case PSN_SETACTIVE:
			// CHORUS
			if(GetDlgItemInt(hwnd,IDC_EDIT_CHORUS,NULL,FALSE)==0)
				SetDlgItemInt(hwnd,IDC_EDIT_CHORUS,1,TRUE);
			if(st_temp->opt_chorus_control==0){
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_CHORUS,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_CHORUS_LEVEL,BM_SETCHECK,0,0);
			} else if(st_temp->opt_chorus_control>0){
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_CHORUS,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_CHORUS_LEVEL,BM_SETCHECK,0,0);
			} else {
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_CHORUS,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_CHORUS_LEVEL,BM_SETCHECK,1,0);
				SetDlgItemInt(hwnd,IDC_EDIT_CHORUS,-st_temp->opt_chorus_control,TRUE);
			}
			SendMessage(hwnd,WM_COMMAND,IDC_CHECKBOX_CHORUS,0);
			// REVERB
			if(GetDlgItemInt(hwnd,IDC_EDIT_REVERB,NULL,FALSE)==0)
				SetDlgItemInt(hwnd,IDC_EDIT_REVERB,1,TRUE);
			if(st_temp->opt_reverb_control==0){
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_REVERB,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_GLOBAL_REVERB,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_REVERB_LEVEL,BM_SETCHECK,0,0);
			} else if(st_temp->opt_reverb_control==1){
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_REVERB,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_GLOBAL_REVERB,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_REVERB_LEVEL,BM_SETCHECK,0,0);
			} else if(st_temp->opt_reverb_control>=2){
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_REVERB,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_GLOBAL_REVERB,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_REVERB_LEVEL,BM_SETCHECK,0,0);
			} else {
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_REVERB,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_GLOBAL_REVERB,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_REVERB_LEVEL,BM_SETCHECK,1,0);
				SetDlgItemInt(hwnd,IDC_EDIT_REVERB,-st_temp->opt_reverb_control,TRUE);
		 }
			SendMessage(hwnd,WM_COMMAND,IDC_CHECKBOX_REVERB,0);
			// DELAY
			SetDlgItemInt(hwnd,IDC_EDIT_DELAY,st_temp->effect_lr_delay_msec,TRUE);
			if(st_temp->effect_lr_mode<0){
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_DELAY,BM_SETCHECK,0,0);
			} else {
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_DELAY,BM_SETCHECK,1,0);
				switch(st_temp->effect_lr_mode){
			case 0:
					CheckRadioButton(hwnd,IDC_RADIOBUTTON_DELAY_LEFT,IDC_RADIOBUTTON_DELAY_CENTER,
				IDC_RADIOBUTTON_DELAY_LEFT);
					break;
				case 1:
					CheckRadioButton(hwnd,IDC_RADIOBUTTON_DELAY_LEFT,IDC_RADIOBUTTON_DELAY_CENTER,
				IDC_RADIOBUTTON_DELAY_RIGHT);
					break;
				case 2:
			default:
					CheckRadioButton(hwnd,IDC_RADIOBUTTON_DELAY_LEFT,IDC_RADIOBUTTON_DELAY_CENTER,
				IDC_RADIOBUTTON_DELAY_CENTER);
					break;
			 }
		 }
			SendMessage(hwnd,WM_COMMAND,IDC_CHECKBOX_DELAY,0);
			// NOISESHARPING
		 SetDlgItemInt(hwnd,IDC_EDIT_NOISESHARPING,st_temp->noise_sharp_type,TRUE);
			// Misc
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_MODWHEEL,st_temp->opt_modulation_wheel);
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_PORTAMENTO,st_temp->opt_portamento);
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_NRPNVIB,st_temp->opt_nrpn_vibrato);
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CHPRESS,st_temp->opt_channel_pressure);
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_OVOICE,st_temp->opt_overlap_voice_allow);
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_TRACETEXT,st_temp->opt_trace_text_meta_event);
		 SetDlgItemInt(hwnd,IDC_EDIT_MODIFY_RELEASE,st_temp->modify_release,TRUE);
			break;
		default:
			return FALSE;
		}
   case WM_SIZE:
		return FALSE;
	case WM_CLOSE:
		break;
	default:
	  break;
	}
	return FALSE;
}

static int char_count(char *s, int c)
{
	 int n = 0;
	 while(*s)
		  n += (*s++ == c);
	 return n;
}

static BOOL APIENTRY
PrefTiMidity2DialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess){
   case WM_INITDIALOG:
		break;
	case WM_COMMAND:
	switch (LOWORD(wParam)) {
	  case IDCLOSE:
		break;
		case IDC_CHECKBOX_SPECIAL_TONEBANK:
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_SPECIAL_TONEBANK,BM_GETCHECK,0,0)){
				EnableWindow(GetDlgItem(hwnd,IDC_EDIT_SPECIAL_TONEBANK),TRUE);
			} else {
				EnableWindow(GetDlgItem(hwnd,IDC_EDIT_SPECIAL_TONEBANK),FALSE);
			}
		default:
		break;
	  }
		PrefWndSetOK = 1;
		PropSheet_Changed((HWND)hPrefWnd,hwnd);
		break;
	case WM_NOTIFY:
		switch (((NMHDR FAR *) lParam)->code){
		case PSN_KILLACTIVE: {
			int i;
			char *p;
			st_temp->voices = GetDlgItemInt(hwnd,IDC_EDIT_VOICES,NULL,FALSE);
			st_temp->amplification = GetDlgItemInt(hwnd,IDC_EDIT_AMPLIFICATION,NULL,FALSE);
			DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_FREE_INST,st_temp->free_instruments_afterwards);
			DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_ANTIALIAS,st_temp->antialiasing_allowed);
			DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_LOADINST_PLAYING,st_temp->opt_realtime_playing);
			st_temp->allocate_cache_size = GetDlgItemInt(hwnd,IDC_EDIT_CACHE_SIZE,NULL,FALSE);
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_REDUCE_VOICE,BM_GETCHECK,0,0))
			{
				st_temp->reduce_voice_threshold = -1;
				st_temp->auto_reduce_polyphony = 1;
			}
			else
			{
				st_temp->reduce_voice_threshold = 0;
				st_temp->auto_reduce_polyphony = 0;
			}

			st_temp->default_tonebank = GetDlgItemInt(hwnd,IDC_EDIT_DEFAULT_TONEBANK,NULL,FALSE);
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_SPECIAL_TONEBANK,BM_GETCHECK,0,0)){
				st_temp->special_tonebank = GetDlgItemInt(hwnd,IDC_EDIT_SPECIAL_TONEBANK,NULL,TRUE);
		 } else {
				st_temp->special_tonebank = -1;
		 }
			if(SendDlgItemMessage(hwnd,IDC_RADIOBUTTON_GS,BM_GETCHECK,0,0)){
			st_temp->opt_default_mid = 0x41;
			} else if(SendDlgItemMessage(hwnd,IDC_RADIOBUTTON_XG,BM_GETCHECK,0,0)){
			st_temp->opt_default_mid = 0x43;
		 } else
			st_temp->opt_default_mid = 0x7e;

			SettingCtlFlag(st_temp, 't',
								DLG_CHECKBUTTON_TO_FLAG(hwnd,IDC_CHECKBOX_CTL_TRACE_PLAYING,i));

			/* remove 'v' and 'q' from st_temp->opt_ctl */
			while(strchr(st_temp->opt_ctl + 1, 'v'))
				 SettingCtlFlag(st_temp, 'v', 0);
			while(strchr(st_temp->opt_ctl + 1, 'q'))
				 SettingCtlFlag(st_temp, 'q', 0);

			/* append 'v' or 'q' */
			p = st_temp->opt_ctl + strlen(st_temp->opt_ctl);
			i = GetDlgItemInt(hwnd,IDC_EDIT_CTL_VEBOSITY,NULL,TRUE);
			while(i > 1) { *p++ = 'v'; i--; }
			while(i < 1) { *p++ = 'q'; i++; }

			st_temp->control_ratio = GetDlgItemInt(hwnd,IDC_EDIT_CONTROL_RATIO,NULL,FALSE);
			SetWindowLong(hwnd,DWL_MSGRESULT,FALSE);
			}
			return TRUE;
		case PSN_RESET:
			PrefWndSetOK = 0;
			SetWindowLong(hwnd,	DWL_MSGRESULT, FALSE);
			break;
		case PSN_APPLY:
			PrefSettingApply();
			break;
		case PSN_SETACTIVE:
			SetDlgItemInt(hwnd,IDC_EDIT_VOICES,st_temp->voices,FALSE);
			SetDlgItemInt(hwnd,IDC_EDIT_AMPLIFICATION,st_temp->amplification,FALSE);
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_FREE_INST,st_temp->free_instruments_afterwards);
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_ANTIALIAS,st_temp->antialiasing_allowed);
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_LOADINST_PLAYING,st_temp->opt_realtime_playing);
			SetDlgItemInt(hwnd,IDC_EDIT_CACHE_SIZE,st_temp->allocate_cache_size,FALSE);

			SetDlgItemInt(hwnd,IDC_EDIT_REDUCE_VOICE,st_temp->reduce_voice_threshold,TRUE);
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_REDUCE_VOICE,BM_SETCHECK,st_temp->reduce_voice_threshold,0);
			SetDlgItemInt(hwnd,IDC_EDIT_DEFAULT_TONEBANK,st_temp->default_tonebank,FALSE);
			SetDlgItemInt(hwnd,IDC_EDIT_SPECIAL_TONEBANK,st_temp->special_tonebank,TRUE);
			if(st_temp->special_tonebank<0){
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_SPECIAL_TONEBANK,BM_SETCHECK,0,0);
			} else {
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_SPECIAL_TONEBANK,BM_SETCHECK,1,0);
			}
			SendMessage(hwnd,WM_COMMAND,IDC_CHECKBOX_SPECIAL_TONEBANK,0);
			switch(st_temp->opt_default_mid){
		 case 0x41:
				CheckRadioButton(hwnd,IDC_RADIOBUTTON_GM,IDC_RADIOBUTTON_XG,IDC_RADIOBUTTON_GS);
				break;
		 case 0x43:
				CheckRadioButton(hwnd,IDC_RADIOBUTTON_GM,IDC_RADIOBUTTON_XG,IDC_RADIOBUTTON_XG);
				break;
			default:
		 case 0x7e:
				CheckRadioButton(hwnd,IDC_RADIOBUTTON_GM,IDC_RADIOBUTTON_XG,IDC_RADIOBUTTON_GM);
				break;
			}

			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CTL_TRACE_PLAYING,
											strchr(st_temp->opt_ctl + 1, 't'));
			SetDlgItemInt(hwnd,IDC_EDIT_CTL_VEBOSITY,
							  char_count(st_temp->opt_ctl + 1, 'v') -
							  char_count(st_temp->opt_ctl + 1, 'q') + 1, TRUE);
			SetDlgItemInt(hwnd,IDC_EDIT_CONTROL_RATIO,st_temp->control_ratio,FALSE);
			break;
		default:
			return FALSE;
		}
   case WM_SIZE:
		return FALSE;
	case WM_CLOSE:
		break;
	default:
	  break;
	}
	return FALSE;
}

static BOOL APIENTRY
PrefTiMidity3DialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess){
   case WM_INITDIALOG:
		SendDlgItemMessage(hwnd,IDC_EDIT_OUTPUT_FILE,
		WM_SETFONT,(WPARAM)hFontPrefWnd,MAKELPARAM(TRUE,0));
		break;
	case WM_COMMAND:
	switch (LOWORD(wParam)) {
	  case IDCLOSE:
		break;
		case IDC_BUTTON_OUTPUT_FILE:
			{
				char filename[MAXPATH+1];
				filename[0] = '\0';
				SendDlgItemMessage(hwnd,IDC_EDIT_OUTPUT_FILE,WM_GETTEXT,
					(WPARAM)MAX_PATH,(LPARAM)TEXT(filename));
				if(!DlgOpenOutputFile(filename,hwnd))
				if(filename[0]!='\0')
						SetDlgItemText(hwnd,IDC_EDIT_OUTPUT_FILE,TEXT(filename));
	   }
		break;
		case IDC_BUTTON_OUTPUT_FILE_DEL:
			{
			char filename[MAXPATH+1];
			DWORD res;
			GetDlgItemText(hwnd,IDC_EDIT_OUTPUT_FILE,filename,(WPARAM)MAX_PATH);
		 res = GetFileAttributes(filename);
		 if(res!=0xFFFFFFFF && !(res & FILE_ATTRIBUTE_DIRECTORY)){
				if(DeleteFile(filename)!=TRUE){
				char buffer[MAXPATH + 1024];
			   sprintf(buffer,"Can't delete file %s !",filename);
					MessageBox(NULL,buffer,"Error!", MB_OK);
				} else {
				char buffer[MAXPATH + 1024];
			   sprintf(buffer,"Delete file %s !",filename);
					MessageBox(NULL,buffer,"Delete!", MB_OK);
			}
			}
		 }
			break;
		case IDC_BUTTON_LOW:
			SetDlgItemInt(hwnd,IDC_EDIT_SAMPLE_RATE,11025,FALSE);
		break;
		case IDC_BUTTON_MIDDLE:
			SetDlgItemInt(hwnd,IDC_EDIT_SAMPLE_RATE,22050,FALSE);
		break;
		case IDC_BUTTON_HIGH:
			SetDlgItemInt(hwnd,IDC_EDIT_SAMPLE_RATE,44100,FALSE);
		break;
		case IDC_BUTTON_4:
			SetDlgItemInt(hwnd,IDC_EDIT_SAMPLE_RATE,4000,FALSE);
		break;
		case IDC_BUTTON_8:
			SetDlgItemInt(hwnd,IDC_EDIT_SAMPLE_RATE,8000,FALSE);
		break;
		case IDC_BUTTON_16:
			SetDlgItemInt(hwnd,IDC_EDIT_SAMPLE_RATE,16000,FALSE);
		break;
		case IDC_BUTTON_24:
			SetDlgItemInt(hwnd,IDC_EDIT_SAMPLE_RATE,24000,FALSE);
		break;
		case IDC_BUTTON_32:
			SetDlgItemInt(hwnd,IDC_EDIT_SAMPLE_RATE,32000,FALSE);
		break;
		case IDC_BUTTON_40:
			SetDlgItemInt(hwnd,IDC_EDIT_SAMPLE_RATE,40000,FALSE);
		break;
		case IDC_BUTTON_48:
			SetDlgItemInt(hwnd,IDC_EDIT_SAMPLE_RATE,48000,FALSE);
		break;
		case IDC_CHECKBOX_ULAW:
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_ULAW,BM_GETCHECK,0,0)){
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ULAW,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ALAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_LINEAR,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_8BITS,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_16BITS,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_SIGNED,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_UNSIGNED,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_BYTESWAP,BM_SETCHECK,0,0);
		 } else {
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ULAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ALAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_LINEAR,BM_SETCHECK,1,0);
		 }
			break;
		case IDC_CHECKBOX_ALAW:
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_ALAW,BM_GETCHECK,0,0)){
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ULAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ALAW,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_LINEAR,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_8BITS,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_16BITS,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_SIGNED,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_UNSIGNED,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_BYTESWAP,BM_SETCHECK,0,0);
		 } else {
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ULAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ALAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_LINEAR,BM_SETCHECK,1,0);
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
		case IDC_CHECKBOX_8BITS:
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_8BITS,BM_GETCHECK,0,0)){
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_8BITS,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_16BITS,BM_SETCHECK,0,0);
		 } else {
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_8BITS,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_16BITS,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ULAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ALAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_LINEAR,BM_SETCHECK,1,0);
		 }
			break;
		case IDC_CHECKBOX_16BITS:
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_16BITS,BM_GETCHECK,0,0)){
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_8BITS,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_16BITS,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ULAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ALAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_LINEAR,BM_SETCHECK,1,0);
		 } else {
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_8BITS,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_16BITS,BM_SETCHECK,0,0);
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
		default:
		break;
	  }
		PrefWndSetOK = 1;
		PropSheet_Changed((HWND)hPrefWnd,hwnd);
		break;
	case WM_NOTIFY:
		switch (((NMHDR FAR *) lParam)->code){
		case PSN_KILLACTIVE: {
			int i = 0;
			if(SendDlgItemMessage(hwnd,IDC_RADIOBUTTON_RIFF_WAVE,BM_GETCHECK,0,0)){
			 st_temp->opt_playmode[i] = 'w';
		 } else
			if(SendDlgItemMessage(hwnd,IDC_RADIOBUTTON_LIST_MIDI_EVENT,BM_GETCHECK,0,0)){
			 st_temp->opt_playmode[i] = 'l';
		 } else
			if(SendDlgItemMessage(hwnd,IDC_RADIOBUTTON_RAW_WAVEFORM,BM_GETCHECK,0,0)){
		  st_temp->opt_playmode[i] = 'r';
		 } else
			if(SendDlgItemMessage(hwnd,IDC_RADIOBUTTON_SUN_AUDIO,BM_GETCHECK,0,0)){
		   st_temp->opt_playmode[i] = 'u';
		 } else
			if(SendDlgItemMessage(hwnd,IDC_RADIOBUTTON_AIFF,BM_GETCHECK,0,0)){
			st_temp->opt_playmode[i] = 'a';
		 } else
			st_temp->opt_playmode[i] = 'd';
			i++;
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_ULAW,BM_GETCHECK,0,0))
				st_temp->opt_playmode[i++] = 'U';
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_ALAW,BM_GETCHECK,0,0))
				st_temp->opt_playmode[i++] = 'A';
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_LINEAR,BM_GETCHECK,0,0))
				st_temp->opt_playmode[i++] = 'l';
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_8BITS,BM_GETCHECK,0,0))
				st_temp->opt_playmode[i++] = '8';
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_16BITS,BM_GETCHECK,0,0))
				st_temp->opt_playmode[i++] = '1';
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_SIGNED,BM_GETCHECK,0,0))
				st_temp->opt_playmode[i++] = 's';
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_UNSIGNED,BM_GETCHECK,0,0))
				st_temp->opt_playmode[i++] = 'u';
			if(SendDlgItemMessage(hwnd,IDC_CHECKBOX_BYTESWAP,BM_GETCHECK,0,0))
				st_temp->opt_playmode[i++] = 'x';
			if(SendDlgItemMessage(hwnd,IDC_RADIO_STEREO,BM_GETCHECK,0,0))
				st_temp->opt_playmode[i++] = 'S';
			if(SendDlgItemMessage(hwnd,IDC_RADIO_MONO,BM_GETCHECK,0,0))
				st_temp->opt_playmode[i++] = 'M';
			st_temp->opt_playmode[i] = '\0';
			st_temp->output_rate = GetDlgItemInt(hwnd,IDC_EDIT_SAMPLE_RATE,NULL,FALSE);
			GetDlgItemText(hwnd,IDC_EDIT_OUTPUT_FILE,st_temp->OutputName,(WPARAM)sizeof(st_temp->OutputName));
			SetWindowLong(hwnd,DWL_MSGRESULT,FALSE);
			}
			return TRUE;
		case PSN_RESET:
			PrefWndSetOK = 0;
			SetWindowLong(hwnd,	DWL_MSGRESULT, FALSE);
			break;
		case PSN_APPLY:
			PrefSettingApply();
			break;
		case PSN_SETACTIVE: {
			char *opt;
			switch(st_temp->opt_playmode[0]){
		 case 'w':
				CheckRadioButton(hwnd,IDC_RADIOBUTTON_WIN32AUDIO,IDC_RADIOBUTTON_AIFF,
				IDC_RADIOBUTTON_RIFF_WAVE);
				break;
		 case 'r':
				CheckRadioButton(hwnd,IDC_RADIOBUTTON_WIN32AUDIO,IDC_RADIOBUTTON_AIFF,
				IDC_RADIOBUTTON_RAW_WAVEFORM);
				break;
		 case 'u':
				CheckRadioButton(hwnd,IDC_RADIOBUTTON_WIN32AUDIO,IDC_RADIOBUTTON_AIFF,
				IDC_RADIOBUTTON_SUN_AUDIO);
				break;
		 case 'a':
				CheckRadioButton(hwnd,IDC_RADIOBUTTON_WIN32AUDIO,IDC_RADIOBUTTON_AIFF,
				IDC_RADIOBUTTON_AIFF);
				break;
		 case 'l':
				CheckRadioButton(hwnd,IDC_RADIOBUTTON_WIN32AUDIO,IDC_RADIOBUTTON_AIFF,
				IDC_RADIOBUTTON_LIST_MIDI_EVENT);
				break;
		 case 'd':
			default:
				CheckRadioButton(hwnd,IDC_RADIOBUTTON_WIN32AUDIO,IDC_RADIOBUTTON_AIFF,
				IDC_RADIOBUTTON_WIN32AUDIO);
				break;
			}
			if(st_temp->OutputName[0]=='\0')
				SetDlgItemText(hwnd,IDC_EDIT_OUTPUT_FILE,TEXT("output.wav"));
			else
				SetDlgItemText(hwnd,IDC_EDIT_OUTPUT_FILE,TEXT(st_temp->OutputName));

			opt = st_temp->opt_playmode + 1;
			if(strchr(opt, 'U')){
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ULAW,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ALAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_LINEAR,BM_SETCHECK,0,0);
		 } else if(strchr(opt, 'A')){
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ULAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ALAW,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_LINEAR,BM_SETCHECK,0,0);
		 } else {
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ULAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_ALAW,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_LINEAR,BM_SETCHECK,1,0);
		 }
			if(strchr(opt, '1')){
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_8BITS,BM_SETCHECK,0,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_16BITS,BM_SETCHECK,1,0);
			} else {
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_8BITS,BM_SETCHECK,1,0);
				SendDlgItemMessage(hwnd,IDC_CHECKBOX_16BITS,BM_SETCHECK,0,0);
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
			SetDlgItemInt(hwnd,IDC_EDIT_SAMPLE_RATE,st_temp->output_rate,FALSE);
#if 0		// Buggy
			EnableWindow(GetDlgItem(hwnd,IDC_RADIOBUTTON_LIST_MIDI_EVENT),FALSE);
#endif
			break;
			}
		default:
			return FALSE;
		}
   case WM_SIZE:
		return FALSE;
	case WM_CLOSE:
		break;
	default:
	  break;
	}
	return FALSE;
}

#define PREF_CHANNEL_MODE_DRUM_CHANNEL		1
#define PREF_CHANNEL_MODE_DRUM_CHANNEL_MASK	2
#define PREF_CHANNEL_MODE_QUIET_CHANNEL		3
static BOOL APIENTRY
PrefTiMidity4DialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	static int pref_channel_mode;
	switch (uMess){
   case WM_INITDIALOG:
		pref_channel_mode = PREF_CHANNEL_MODE_DRUM_CHANNEL;
		break;
	case WM_COMMAND:
	switch (LOWORD(wParam)) {
	  case IDCLOSE:
		break;
		case IDC_CHECKBOX_DRUM_CHANNEL:
			{
			NMHDR nmhdr;
		 nmhdr.code = PSN_KILLACTIVE;
			SendMessage(hwnd,WM_NOTIFY,0,(LPARAM)&nmhdr);
			pref_channel_mode = PREF_CHANNEL_MODE_DRUM_CHANNEL;
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_DRUM_CHANNEL,BM_SETCHECK,1,0);
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_DRUM_CHANNEL_MASK,BM_SETCHECK,0,0);
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_QUIET_CHANNEL,BM_SETCHECK,0,0);
		 nmhdr.code = PSN_SETACTIVE;
			SendMessage(hwnd,WM_NOTIFY,0,(LPARAM)&nmhdr);
			}
		break;
		case IDC_CHECKBOX_DRUM_CHANNEL_MASK:
			{
			NMHDR nmhdr;
		 nmhdr.code = PSN_KILLACTIVE;
			SendMessage(hwnd,WM_NOTIFY,0,(LPARAM)&nmhdr);
			pref_channel_mode = PREF_CHANNEL_MODE_DRUM_CHANNEL_MASK;
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_DRUM_CHANNEL,BM_SETCHECK,0,0);
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_DRUM_CHANNEL_MASK,BM_SETCHECK,1,0);
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_QUIET_CHANNEL,BM_SETCHECK,0,0);
		 nmhdr.code = PSN_SETACTIVE;
			SendMessage(hwnd,WM_NOTIFY,0,(LPARAM)&nmhdr);
			}
		break;
		case IDC_CHECKBOX_QUIET_CHANNEL:
			{
			NMHDR nmhdr;
		 nmhdr.code = PSN_KILLACTIVE;
			SendMessage(hwnd,WM_NOTIFY,0,(LPARAM)&nmhdr);
			pref_channel_mode = PREF_CHANNEL_MODE_QUIET_CHANNEL;
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_DRUM_CHANNEL,BM_SETCHECK,0,0);
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_DRUM_CHANNEL_MASK,BM_SETCHECK,0,0);
			SendDlgItemMessage(hwnd,IDC_CHECKBOX_QUIET_CHANNEL,BM_SETCHECK,1,0);
		 nmhdr.code = PSN_SETACTIVE;
			SendMessage(hwnd,WM_NOTIFY,0,(LPARAM)&nmhdr);
			}
		break;
		default:
		break;
	  }
		PrefWndSetOK = 1;
		PropSheet_Changed((HWND)hPrefWnd,hwnd);
		break;
	case WM_NOTIFY:
		switch (((NMHDR FAR *) lParam)->code){
		case PSN_KILLACTIVE:
			{
			ChannelBitMask channelbitmask;
			int tmp;
#define PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,ctlid,channelbitmask,ch,tmp) \
{	if(DLG_CHECKBUTTON_TO_FLAG((hwnd),(ctlid),(tmp))) SET_CHANNELMASK((channelbitmask),(ch)); \
	else UNSET_CHANNELMASK((channelbitmask),(ch)); }
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH01,channelbitmask,0,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH02,channelbitmask,1,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH03,channelbitmask,2,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH04,channelbitmask,3,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH05,channelbitmask,4,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH06,channelbitmask,5,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH07,channelbitmask,6,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH08,channelbitmask,7,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH09,channelbitmask,8,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH10,channelbitmask,9,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH11,channelbitmask,10,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH12,channelbitmask,11,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH13,channelbitmask,12,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH14,channelbitmask,13,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH15,channelbitmask,14,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH16,channelbitmask,15,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH17,channelbitmask,16,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH18,channelbitmask,17,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH19,channelbitmask,18,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH20,channelbitmask,19,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH21,channelbitmask,20,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH22,channelbitmask,21,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH23,channelbitmask,22,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH24,channelbitmask,23,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH25,channelbitmask,24,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH26,channelbitmask,25,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH27,channelbitmask,26,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH28,channelbitmask,27,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH29,channelbitmask,28,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH30,channelbitmask,29,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH31,channelbitmask,30,tmp);
			PREF_CHECKBUTTON_SET_CHANNELMASK(hwnd,IDC_CHECKBOX_CH32,channelbitmask,31,tmp);
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
			}
			SetWindowLong(hwnd,DWL_MSGRESULT,FALSE);
			return TRUE;
		case PSN_RESET:
			PrefWndSetOK = 0;
			SetWindowLong(hwnd,	DWL_MSGRESULT, FALSE);
			break;
		case PSN_APPLY:
			PrefSettingApply();
			break;
		case PSN_SETACTIVE:
			{
			ChannelBitMask channelbitmask;
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
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH01,IS_SET_CHANNELMASK(channelbitmask,0));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH02,IS_SET_CHANNELMASK(channelbitmask,1));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH03,IS_SET_CHANNELMASK(channelbitmask,2));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH04,IS_SET_CHANNELMASK(channelbitmask,3));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH05,IS_SET_CHANNELMASK(channelbitmask,4));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH06,IS_SET_CHANNELMASK(channelbitmask,5));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH07,IS_SET_CHANNELMASK(channelbitmask,6));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH08,IS_SET_CHANNELMASK(channelbitmask,7));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH09,IS_SET_CHANNELMASK(channelbitmask,8));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH10,IS_SET_CHANNELMASK(channelbitmask,9));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH11,IS_SET_CHANNELMASK(channelbitmask,10));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH12,IS_SET_CHANNELMASK(channelbitmask,11));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH13,IS_SET_CHANNELMASK(channelbitmask,12));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH14,IS_SET_CHANNELMASK(channelbitmask,13));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH15,IS_SET_CHANNELMASK(channelbitmask,14));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH16,IS_SET_CHANNELMASK(channelbitmask,15));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH17,IS_SET_CHANNELMASK(channelbitmask,16));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH18,IS_SET_CHANNELMASK(channelbitmask,17));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH19,IS_SET_CHANNELMASK(channelbitmask,18));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH20,IS_SET_CHANNELMASK(channelbitmask,19));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH21,IS_SET_CHANNELMASK(channelbitmask,20));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH22,IS_SET_CHANNELMASK(channelbitmask,21));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH23,IS_SET_CHANNELMASK(channelbitmask,22));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH24,IS_SET_CHANNELMASK(channelbitmask,23));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH25,IS_SET_CHANNELMASK(channelbitmask,24));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH26,IS_SET_CHANNELMASK(channelbitmask,25));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH27,IS_SET_CHANNELMASK(channelbitmask,26));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH28,IS_SET_CHANNELMASK(channelbitmask,27));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH29,IS_SET_CHANNELMASK(channelbitmask,28));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH30,IS_SET_CHANNELMASK(channelbitmask,29));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH31,IS_SET_CHANNELMASK(channelbitmask,30));
			DLG_FLAG_TO_CHECKBUTTON(hwnd,IDC_CHECKBOX_CH32,IS_SET_CHANNELMASK(channelbitmask,31));
			}
		 break;
		default:
			return FALSE;
		}
   case WM_SIZE:
		return FALSE;
	case WM_CLOSE:
		break;
	default:
	  break;
	}
	return FALSE;
}

static int DlgOpenConfigFile(char *Filename, HWND hwnd)
{
	OPENFILENAME ofn;
   char filename[MAXPATH + 256];
   char dir[MAXPATH + 256];
   int res;

   strncpy(dir,ConfigFileOpenDir,MAXPATH);
   dir[MAXPATH-1] = '\0';
   strncpy(filename,Filename,MAXPATH);
   filename[MAXPATH-1] = '\0';

	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.hInstance = hInst ;
	ofn.lpstrFilter =
		"config file\0*.cfg;*.config\0"
		"all files\0*.*\0"
		"\0\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 1 ;
	ofn.lpstrFile = filename;
	ofn.nMaxFile = 256;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	if(dir[0] != '\0')
		ofn.lpstrInitialDir	= dir;
	else
		ofn.lpstrInitialDir	= 0;
	ofn.lpstrTitle	= "Open Config File";
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER
	| OFN_READONLY | OFN_HIDEREADONLY;
	ofn.lpstrDefExt = 0;
	ofn.lCustData = 0;
	ofn.lpfnHook = 0;
	ofn.lpTemplateName= 0;

	res = GetOpenFileName(&ofn);
   strncpy(ConfigFileOpenDir,dir,MAXPATH);
   ConfigFileOpenDir[MAXPATH-1] = '\0';
   if(res==TRUE){
	strncpy(Filename,filename,MAXPATH);
	Filename[MAXPATH-1] = '\0';
	return 0;
	}
   else {
	Filename[0] = '\0';
	return -1;
	}
}

static int DlgOpenOutputFile(char *Filename, HWND hwnd)
{
	OPENFILENAME ofn;
   char filename[MAXPATH + 256];
   char dir[MAXPATH + 256];
   int res;
	static char OutputFileOpenDir[MAXPATH+256];
   static int initflag = 1;

   if(initflag){
	OutputFileOpenDir[0] = '\0';
	  initflag = 0;
   }
   strncpy(dir,OutputFileOpenDir,MAXPATH);
   dir[MAXPATH-1] = '\0';
   strncpy(filename,Filename,MAXPATH);
   filename[MAXPATH-1] = '\0';

	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.hInstance = hInst ;
	ofn.lpstrFilter =
		"wave file\0*.wav;*.wave;*.aif;*.aiff;*.aifc;*.au;*.snd;*.audio\0"
		"all files\0*.*\0"
		"\0\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 1 ;
	ofn.lpstrFile = filename;
	ofn.nMaxFile = 256;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	if(dir[0] != '\0')
		ofn.lpstrInitialDir	= dir;
	else
		ofn.lpstrInitialDir	= 0;
	ofn.lpstrTitle	= "Output File";
	ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY;
	ofn.lpstrDefExt = 0;
	ofn.lCustData = 0;
	ofn.lpfnHook = 0;
	ofn.lpTemplateName= 0;

	res = GetSaveFileName(&ofn);
   strncpy(OutputFileOpenDir,dir,MAXPATH);
   OutputFileOpenDir[MAXPATH-1] = '\0';
   if(res==TRUE){
	strncpy(Filename,filename,MAXPATH);
	Filename[MAXPATH-1] = '\0';
	return 0;
	} else {
	Filename[0] = '\0';
	return -1;
	}
}
