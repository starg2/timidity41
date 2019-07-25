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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <windows.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */
#include <stdio.h>
#include <string.h>
#include <process.h>
#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "output.h"
#include "controls.h"
#include "recache.h"
#ifdef SUPPORT_SOUNDSPEC
#include "soundspec.h"
#endif /* SUPPORT_SOUNDSPEC */
#include "wrd.h"
#include "w32g.h"
#include "w32g_ut2.h"

extern char *timidity_window_inifile;

#define DEFAULT_CANVAS_MODE     CANVAS_MODE_MAP32

// ****************************************************************************
// DlgChooseFont
// hwnd: Owner Window of This Dialog
// hwndFontChange: Window to Change Font
// hFontPre: Previous Font of hwndFontChange (Call CloseHandle())
int DlgChooseFontAndApply(HWND hwnd, HWND hwndFontChange, HFONT hFontPre, char *fontname, int *fontheight, int *fontwidth)
{
    LOGFONT lf;
    CHOOSEFONT cf;
    HFONT hFont;
    ZeroMemory(&lf, sizeof(LOGFONT));
    ZeroMemory(&cf, sizeof(CHOOSEFONT));

//	lf.lfHeight = 16;
//	lf.lfWidth = 8;
	_tcscpy(lf.lfFaceName, _T("ＭＳ 明朝"));
    cf.lStructSize = sizeof(CHOOSEFONT);
    cf.hwndOwner = hwnd;
//  cf.hDC = NULL;
    cf.lpLogFont = &lf;
//  cf.iPointSize = 16;
//  cf.Flags = CF_ANSIONLY | CF_FORCEFONTEXIST;
    cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT;
//  cf.rgbColors = RGB(0, 0, 0);
//  cf.lCustData = NULL;
//  cf.lpfnHook = NULL;
//  cf.lpTemplateName = NULL;
//  cf.hInstance = 0;
//  cf.lpszStyle = NULL;
    cf.nFontType = SCREEN_FONTTYPE;
//  cf.nSizeMin = 4;
//  cf.nSizeMax = 72;
    ChooseFont(&cf);

//	if(ChooseFont(&cf)==TRUE)
//		return -1;
	if(hFontPre!=NULL)
		CloseHandle(hFontPre);
	hFont = CreateFontIndirect(&lf);
	SendMessage(hwndFontChange,WM_SETFONT,(WPARAM)hFont,(LPARAM)MAKELPARAM(TRUE,0));
	char *facename = tchar_to_char(lf.lfFaceName);
	if(fontname!=NULL) strcpy(fontname, facename);
	if(fontheight!=NULL) *fontheight = lf.lfHeight;
	if(fontwidth!=NULL) *fontwidth = lf.lfWidth;
	safe_free(facename);
	return 0;
}

int DlgChooseFont(HWND hwnd, char *fontName, int *fontHeight, int *fontWidth)
{
    LOGFONT lf;
    CHOOSEFONT cf;

	memset(&lf,0,sizeof(LOGFONT));
	if(fontHeight!=NULL) lf.lfHeight = *fontHeight;
	if(fontWidth!=NULL) lf.lfWidth = *fontWidth;
	TCHAR *tfontname = char_to_tchar(fontName);
	if(fontName!=NULL) _tcscpy(lf.lfFaceName, tfontname);
	safe_free(tfontname);

    ZeroMemory(&cf, sizeof(CHOOSEFONT));
    cf.lStructSize = sizeof(CHOOSEFONT);
    cf.hwndOwner = hwnd;
//  cf.hDC = NULL;
    cf.lpLogFont = &lf;
//  cf.iPointSize = 16;
//  cf.Flags = CF_ANSIONLY | CF_FORCEFONTEXIST;
    cf.Flags = CF_ANSIONLY | CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT;
//  cf.rgbColors = RGB(0,0,0);
//  cf.lCustData = NULL;
//  cf.lpfnHook = NULL;
//  cf.lpTemplateName = NULL;
//  cf.hInstance = 0;
//  cf.lpszStyle = NULL;
    cf.nFontType = SCREEN_FONTTYPE;
//  cf.nSizeMin = 4;
//  cf.nSizeMax = 72;
    if (ChooseFont(&cf) != TRUE)
        return -1;

	char *facename = tchar_to_char(lf.lfFaceName);
	if(fontName!=NULL) strcpy(fontName, facename);
	if(fontHeight!=NULL) *fontHeight = abs(lf.lfHeight);
	if(fontWidth!=NULL) *fontWidth = lf.lfWidth;
	safe_free(facename);
	return 0;
}

/**********************************************************************/
void SetWindowPosSize(HWND parent_hwnd, HWND hwnd, int x, int y)
{
    RECT rc1, rc2;
    int width1, height1;
    int width2, height2;
    if (GetWindowRect(hwnd, &rc1)) {
        width1 = rc1.right - rc1.left;
        height1 = rc1.bottom - rc1.top;
        if (x >= 0) {
            rc1.right = rc1.right - rc1.left + x;
            rc1.left = x;
        } else {
//          rc1.right = rc1.right - rc1.left;
//          rc1.left = 0;
        }
        if (y >= 0) {
            rc1.bottom = rc1.bottom - rc1.top + y;
            rc1.top = y;
        } else {
//          rc1.bottom = rc1.bottom - rc1.top;
//          rc1.top = 0;
        }
        if (GetClientRect(parent_hwnd, &rc2)) {
            width2 = rc2.right - rc2.left;
            height2 = rc2.bottom - rc2.top;
            if (rc1.left < rc2.left) rc1.left = rc2.left;
            if (rc1.left > rc2.right) rc1.left = rc2.right;
            if (rc1.top < rc2.top) rc1.top = rc2.top;
            if (rc1.top > rc2.bottom) rc1.top = rc2.bottom;
            if (width1 > width2) width1 = width2;
            if (height1 > height2) height1 = height2;
            MoveWindow(hwnd, rc1.left, rc1.top, width1, height1, TRUE);
        }
    }
}

/**********************************************************************/
BOOL PosSizeSave = TRUE;

#define SEC_MAINWND _T("MainWnd")
int INISaveMainWnd(void)
{
	TCHAR *section = SEC_MAINWND;
	TCHAR *inifile = char_to_tchar(TIMIDITY_WINDOW_INI_FILE);
	TCHAR buffer[256];
	if ( PosSizeSave ) {
		_stprintf(buffer, _T("%d"), MainWndInfo.PosX);
		if ( MainWndInfo.PosX >= 0 || MainWndInfo.PosY >= 0 ) {
			if ( MainWndInfo.PosX < 0 )
				MainWndInfo.PosX = 0;
			if ( MainWndInfo.PosY < 0 )
				MainWndInfo.PosY = 0;
		}
		if ( MainWndInfo.PosX >= 0 )
			WritePrivateProfileString(section, _T("PosX"), buffer, inifile);
		_stprintf(buffer, _T("%d"), MainWndInfo.PosY);
		if ( MainWndInfo.PosY >= 0 )
			WritePrivateProfileString(section, _T("PosY"), buffer, inifile);
		_stprintf(buffer, _T("%d"), MainWndInfo.Width);
		WritePrivateProfileString(section, _T("Width"), buffer, inifile);
		_stprintf(buffer, _T("%d"), MainWndInfo.Height);
		WritePrivateProfileString(section, _T("Height"), buffer, inifile);
	}
	_stprintf(buffer, _T("%d"), MainWndInfo.CanvasMode);
	WritePrivateProfileString(section, _T("CanvasMode"), buffer, inifile);
	WritePrivateProfileString(NULL,NULL,NULL,inifile);		// Write Flush
	safe_free(inifile);
	return 0;
}

int INILoadMainWnd(void)
{
	TCHAR *section = SEC_MAINWND;
	TCHAR *inifile = char_to_tchar(TIMIDITY_WINDOW_INI_FILE);
	int num;
	num = GetPrivateProfileInt(section, _T("PosX"), -1, inifile);
	MainWndInfo.PosX = num;
	num = GetPrivateProfileInt(section, _T("PosY"), -1, inifile);
	MainWndInfo.PosY = num;
	num = GetPrivateProfileInt(section, _T("Width"), -1, inifile);
	MainWndInfo.Width = num;
	num = GetPrivateProfileInt(section, _T("Height"), -1, inifile);
	MainWndInfo.Height = num;
	num = GetPrivateProfileInt(section, _T("CanvasMode"), -1, inifile);
	MainWndInfo.CanvasMode = num;
	safe_free(inifile);
	return 0;
}

#define SEC_LISTWND _T("ListWnd")
int INISaveListWnd(void)
{
	TCHAR *section = SEC_LISTWND;
	TCHAR *inifile = char_to_tchar(TIMIDITY_WINDOW_INI_FILE);
	TCHAR buffer[256];
	int i;
	TCHAR key[30] = _T("");

	if ( PosSizeSave ) {
		if ( ListWndInfo.PosX >= 0 || ListWndInfo.PosY >= 0 ) {
			if ( ListWndInfo.PosX < 0 )
				ListWndInfo.PosX = 0;
			if ( ListWndInfo.PosY < 0 )
				ListWndInfo.PosY = 0;
		}
		_stprintf(buffer, _T("%d"), ListWndInfo.PosX);
		if ( ListWndInfo.PosX >= 0 )
			WritePrivateProfileString(section, _T("PosX"), buffer, inifile);
		_stprintf(buffer, _T("%d"), ListWndInfo.PosY);
		if ( ListWndInfo.PosY >= 0 )
			WritePrivateProfileString(section, _T("PosY"), buffer, inifile);
		_stprintf(buffer, _T("%d"), ListWndInfo.Width);
		WritePrivateProfileString(section, _T("Width"), buffer, inifile);
		_stprintf(buffer, _T("%d"), ListWndInfo.Height);
		WritePrivateProfileString(section, _T("Height"), buffer, inifile);
	}
	TCHAR *tstr = char_to_tchar(ListWndInfo.fontNameEN);
	WritePrivateProfileString(section, _T("fontNameEN"), tstr, inifile);
	safe_free(tstr);
	tstr = char_to_tchar(ListWndInfo.fontNameJA);
	WritePrivateProfileString(section, _T("fontNameJA"), tstr, inifile);
	safe_free(tstr);
	_stprintf(buffer, _T("%d"), ListWndInfo.fontWidth);
	WritePrivateProfileString(section, _T("fontWidth"), buffer, inifile);
	_stprintf(buffer, _T("%d"), ListWndInfo.fontHeight);
	WritePrivateProfileString(section, _T("fontHeight"), buffer, inifile);
	_stprintf(buffer, _T("%d"), ListWndInfo.fontFlags);
	WritePrivateProfileString(section, _T("fontFlags"), buffer, inifile);
	// ListName
	for(i = 0; i < playlist_max; i++){
		_sntprintf(key, sizeof(key), _T("ListName%d"), i);
		WritePrivateProfileString(section, key, ListWndInfo.ListName[i], inifile);
	}
	// columWidth
	for(i = 0; i < LISTWND_COLUM; i++){
		_sntprintf(key, sizeof(key), _T("ColumWidth%d"), i);
		_stprintf(buffer, _T("%d"), ListWndInfo.columWidth[i]);
		WritePrivateProfileString(section, key, buffer, inifile);
	}
	//
	WritePrivateProfileString(NULL,NULL,NULL,inifile);		// Write Flush
	safe_free(inifile);
	return 0;
}


int INILoadListWnd(void)
{
	TCHAR *section = SEC_LISTWND;
	TCHAR *inifile = char_to_tchar(TIMIDITY_WINDOW_INI_FILE);
	int num;
	TCHAR buffer[LF_FULLFACESIZE + 1];
	int i;
	TCHAR key[30] = _T(""), name[30] = _T("");
	const int def_columWidth[LISTWND_COLUM] = {400, 150, 60, 50, 150, 600};

	num = GetPrivateProfileInt(section, _T("PosX"), -1, inifile);
	ListWndInfo.PosX = num;
	num = GetPrivateProfileInt(section, _T("PosY"), -1, inifile);
	ListWndInfo.PosY = num;
	num = GetPrivateProfileInt(section, _T("Width"), -1, inifile);
	if(num!=-1) ListWndInfo.Width = num;
	num = GetPrivateProfileInt(section, _T("Height"), -1, inifile);
	if(num!=-1) ListWndInfo.Height = num;
	GetPrivateProfileString(section, _T("fontNameEN"), _T(""), buffer, LF_FULLFACESIZE + 1, inifile);
	if (buffer[0] != 0) {
		char *text = tchar_to_char(buffer);
		strcpy(ListWndInfo.fontNameEN, text);
		safe_free(text);
	}
	GetPrivateProfileString(section, _T("fontNameJA"), _T(""), buffer, LF_FULLFACESIZE + 1, inifile);
	if (buffer[0] != 0) {
		char *text = tchar_to_char(buffer);
		strcpy(ListWndInfo.fontNameJA, text);
		safe_free(text);
	}
	num = GetPrivateProfileInt(section, _T("fontWidth"), -1, inifile);
	if(num!=-1) ListWndInfo.fontWidth = num;
	num = GetPrivateProfileInt(section, _T("fontHeight"), -1, inifile);
	if(num!=-1) ListWndInfo.fontHeight = num;
	num = GetPrivateProfileInt(section, _T("fontFlags"), -1, inifile);
	if(num!=-1) ListWndInfo.fontFlags = num;
	// ListName
	for(i = 0; i < PLAYLIST_MAX; i++){
		_sntprintf(key, sizeof(key), _T("ListName%d"), i);
		_sntprintf(name, sizeof(name), _T("default%d"), i);
		GetPrivateProfileString(section,key,name,buffer,LF_FULLFACESIZE + 1,inifile);
		if (buffer[0] != 0) {
			_tcsncpy(ListWndInfo.ListName[i], buffer, LF_FULLFACESIZE);
			ListWndInfo.ListName[i][LF_FULLFACESIZE] = _T('\0');
		}
	}
	// columWidth
	for(i = 0; i < LISTWND_COLUM; i++){
		_sntprintf(key, sizeof(key), _T("ColumWidth%d"), i);
		num = GetPrivateProfileInt(section,key, def_columWidth[i],inifile);
		ListWndInfo.columWidth[i] = num;
	}
	safe_free(inifile);
	return 0;
}

#define SEC_DOCWND _T("DocWnd")
int INISaveDocWnd(void)
{
	TCHAR *section = SEC_DOCWND;
	TCHAR *inifile = char_to_tchar(TIMIDITY_WINDOW_INI_FILE);
	TCHAR buffer[256];
	if ( PosSizeSave ) {
		if ( DocWndInfo.PosX >= 0 || DocWndInfo.PosY >= 0 ) {
			if ( DocWndInfo.PosX < 0 )
				DocWndInfo.PosX = 0;
			if ( DocWndInfo.PosY < 0 )
				DocWndInfo.PosY = 0;
		}
		_stprintf(buffer,_T("%d"),DocWndInfo.PosX);
		if ( DocWndInfo.PosX >= 0 )
		WritePrivateProfileString(section,_T("PosX"),buffer,inifile);
		_stprintf(buffer,_T("%d"),DocWndInfo.PosY);
		if ( DocWndInfo.PosY >= 0 )
		WritePrivateProfileString(section,_T("PosY"),buffer,inifile);
		_stprintf(buffer,_T("%d"),DocWndInfo.Width);
		WritePrivateProfileString(section,_T("Width"),buffer,inifile);
		_stprintf(buffer,_T("%d"),DocWndInfo.Height);
		WritePrivateProfileString(section,_T("Height"),buffer,inifile);
	}
	TCHAR *tstr = char_to_tchar(DocWndInfo.fontNameEN);
	WritePrivateProfileString(section,_T("fontNameEN"),tstr,inifile);
	safe_free(tstr);
	tstr = char_to_tchar(DocWndInfo.fontNameJA);
	WritePrivateProfileString(section,_T("fontNameJA"),tstr,inifile);
	safe_free(tstr);
	_stprintf(buffer,_T("%d"),DocWndInfo.fontWidth);
	WritePrivateProfileString(section,_T("fontWidth"),buffer,inifile);
	_stprintf(buffer,_T("%d"),DocWndInfo.fontHeight);
	WritePrivateProfileString(section,_T("fontHeight"),buffer,inifile);
	_stprintf(buffer,_T("%d"),DocWndInfo.fontFlags);
	WritePrivateProfileString(section,_T("fontFlags"),buffer,inifile);
	WritePrivateProfileString(NULL,NULL,NULL,inifile);		// Write Flush
	safe_free(inifile);
	return 0;
}

int INILoadDocWnd(void)
{
	TCHAR *section = SEC_DOCWND;
	TCHAR *inifile = char_to_tchar(TIMIDITY_WINDOW_INI_FILE);
	int num;
	TCHAR buffer[LF_FULLFACESIZE + 1];
	num = GetPrivateProfileInt(section,_T("PosX"),-1,inifile);
	DocWndInfo.PosX = num;
	num = GetPrivateProfileInt(section,_T("PosY"),-1,inifile);
	DocWndInfo.PosY = num;
	num = GetPrivateProfileInt(section,_T("Width"),-1,inifile);
	if(num!=-1) DocWndInfo.Width = num;
	num = GetPrivateProfileInt(section,_T("Height"),-1,inifile);
	if(num!=-1) DocWndInfo.Height = num;
	GetPrivateProfileString(section,_T("fontNameEN"),_T(""),buffer,LF_FULLFACESIZE + 1,inifile);
	if (buffer[0] != 0) {
		char *text = tchar_to_char(buffer);
		strcpy(DocWndInfo.fontNameEN, text);
		safe_free(text);
	}
	GetPrivateProfileString(section,_T("fontNameJA"),_T(""),buffer,LF_FULLFACESIZE + 1,inifile);
	if (buffer[0] != 0) {
		char *text = tchar_to_char(buffer);
		strcpy(DocWndInfo.fontNameJA, text);
		safe_free(text);
	}
	num = GetPrivateProfileInt(section,_T("fontWidth"),-1,inifile);
	if(num!=-1) DocWndInfo.fontWidth = num;
	num = GetPrivateProfileInt(section,_T("fontHeight"),-1,inifile);
	if(num!=-1) DocWndInfo.fontHeight = num;
	num = GetPrivateProfileInt(section,_T("fontFlags"),-1,inifile);
	if(num!=-1) DocWndInfo.fontFlags = num;
	safe_free(inifile);
	return 0;
}

#define SEC_CONSOLEWND _T("ConsoleWnd")
int INISaveConsoleWnd(void)
{
	TCHAR *section = SEC_CONSOLEWND;
	TCHAR *inifile = char_to_tchar(TIMIDITY_WINDOW_INI_FILE);
	TCHAR buffer[256];
	if ( PosSizeSave ) {
		if ( ConsoleWndInfo.PosX >= 0 || ConsoleWndInfo.PosY >= 0 ) {
			if ( ConsoleWndInfo.PosX < 0 )
				ConsoleWndInfo.PosX = 0;
			if ( ConsoleWndInfo.PosY < 0 )
				ConsoleWndInfo.PosY = 0;
		}
		_stprintf(buffer,_T("%d"),ConsoleWndInfo.PosX);
		if ( ConsoleWndInfo.PosX >= 0 )
		WritePrivateProfileString(section,_T("PosX"),buffer,inifile);
		_stprintf(buffer,_T("%d"),ConsoleWndInfo.PosY);
		if ( ConsoleWndInfo.PosY >= 0 )
		WritePrivateProfileString(section,_T("PosY"),buffer,inifile);
		///r
		_stprintf(buffer,_T("%d"),ConsoleWndInfo.Width);
		WritePrivateProfileString(section,_T("Width"),buffer,inifile);
		_stprintf(buffer,_T("%d"),ConsoleWndInfo.Height);
		WritePrivateProfileString(section,_T("Height"),buffer,inifile);
	}
	WritePrivateProfileString(NULL,NULL,NULL,inifile);		// Write Flush
	safe_free(inifile);
	return 0;
}

int INILoadConsoleWnd(void)
{
	TCHAR *section = SEC_CONSOLEWND;
	TCHAR *inifile = char_to_tchar(TIMIDITY_WINDOW_INI_FILE);
	int num;
	num = GetPrivateProfileInt(section,_T("PosX"),-1,inifile);
	ConsoleWndInfo.PosX = num;
	num = GetPrivateProfileInt(section,_T("PosY"),-1,inifile);
	ConsoleWndInfo.PosY = num;
	///r
	num = GetPrivateProfileInt(section,_T("Width"),-1,inifile);
	if(num!=-1) ConsoleWndInfo.Width = num;
	num = GetPrivateProfileInt(section,_T("Height"),-1,inifile);
	if(num!=-1) ConsoleWndInfo.Height = num;
	safe_free(inifile);
	return 0;
}

#define SEC_SOUNDSPECWND _T("SoundSpecWnd")
int INISaveSoundSpecWnd(void)
{
	TCHAR *section = SEC_SOUNDSPECWND;
	TCHAR *inifile = char_to_tchar(TIMIDITY_WINDOW_INI_FILE);
	TCHAR buffer[256];
	if ( PosSizeSave ) {
		if ( SoundSpecWndInfo.PosX >= 0 || SoundSpecWndInfo.PosY >= 0 ) {
			if ( SoundSpecWndInfo.PosX < 0 )
				SoundSpecWndInfo.PosX = 0;
			if ( SoundSpecWndInfo.PosY < 0 )
				SoundSpecWndInfo.PosY = 0;
		}
		_stprintf(buffer,_T("%d"),SoundSpecWndInfo.PosX);
		if ( SoundSpecWndInfo.PosX >= 0 )
		WritePrivateProfileString(section,_T("PosX"),buffer,inifile);
		_stprintf(buffer,_T("%d"),SoundSpecWndInfo.PosY);
		if ( SoundSpecWndInfo.PosY >= 0 )
		WritePrivateProfileString(section,_T("PosY"),buffer,inifile);
		_stprintf(buffer,_T("%d"),SoundSpecWndInfo.Width);
		WritePrivateProfileString(section,_T("Width"),buffer,inifile);
		_stprintf(buffer,_T("%d"),SoundSpecWndInfo.Height);
		WritePrivateProfileString(section,_T("Height"),buffer,inifile);
	}
	WritePrivateProfileString(NULL,NULL,NULL,inifile);		// Write Flush
	safe_free(inifile);
	return 0;
}

int INILoadSoundSpecWnd(void)
{
	TCHAR *section = SEC_SOUNDSPECWND;
	TCHAR *inifile = char_to_tchar(TIMIDITY_WINDOW_INI_FILE);
	int num;

	num = GetPrivateProfileInt(section,_T("PosX"),-1,inifile);
	SoundSpecWndInfo.PosX = num;
	num = GetPrivateProfileInt(section,_T("PosY"),-1,inifile);
	SoundSpecWndInfo.PosY = num;
	num = GetPrivateProfileInt(section,_T("Width"),-1,inifile);
	if(num!=-1) SoundSpecWndInfo.Width = num;
	num = GetPrivateProfileInt(section,_T("Height"),-1,inifile);
	if(num!=-1) SoundSpecWndInfo.Height = num;
	safe_free(inifile);
	return 0;
}


/**********************************************************************/
// プロセス間通信用にメールスロットのサーバースレッドを用意する

#define TIMIDITY_MAILSLOT _T("\\\\.\\mailslot\\timiditypp_mailslot_ver_1_0")

// メールスロットに渡される形式
// ヘッダ
// コマンド名
// オプション数
// オプション１
// オプション２
//   ...

// ヘッダ
#define MC_HEADER       "TiMidity++Win32GUI Mailslot-1.0"
// コマンド名
// TiMidity の終了
#define MC_TERMINATE    "Terminate"
// ファイルを指定
#define MC_FILES "Files Argc Argv"
// オプション１ : ファイル名１
//   ...
// プレイリストのクリア
#define MC_PLAYLIST_CLEAR       "Playlist Clear"
// 演奏開始
#define MC_PLAY                 "Play"
// 次のファイルの演奏
#define MC_PLAY_NEXT    "Play Next"
// 前のファイルの演奏
#define MC_PLAY_PREV    "Play Prev"
// 演奏停止
#define MC_STOP "Stop"
// 演奏一時停止
#define MC_PAUSE        "Pause"
// TiMidity の状態を指定メールスロットに送信
#define MC_SEND_TIMIDITY_INFO   "Send TiMidity Info"
// オプション１ : メールスロット名
// オプション２ : 状態１
//   ...
// 状態
// "PlayFileName:～" : 演奏ファイル名
// "PlayTile:～"                : 演奏タイトル名
// "PlayStatus:～"              : 演奏状態(～:PLAY,STOP,PAUSE)

static HANDLE hMailslot = NULL;

void w32gMailslotThread(void);

int w32gStartMailslotThread(void)
{
    DWORD dwThreadID;
    HANDLE hThread;
    hThread = (HANDLE) crt_beginthreadex(NULL,0,(LPTHREAD_START_ROUTINE) w32gMailslotThread,NULL,0,&dwThreadID);
    if ((ULONG_PTR) hThread == -1) {
        return FALSE;   // Error!
    }
	set_thread_description((ptr_size_t)hThread, "W32G Mailslot Thread");
	return TRUE;
}

int ReadFromMailslot(HANDLE hmailslot, char *buffer, int *size)
{
    DWORD dwMessageSize, dwMessageNum, dwMessageReadSize;
    BOOL bRes;
    int i;
    bRes = GetMailslotInfo(hmailslot,NULL,&dwMessageSize,&dwMessageNum,(LPDWORD) NULL);
    if (bRes==FALSE || dwMessageSize==MAILSLOT_NO_MESSAGE)
        return FALSE;
    for (i=0;i<10;i++) {
        bRes = ReadFile(hMailslot,buffer,dwMessageSize,&dwMessageReadSize,(LPOVERLAPPED) NULL);
#ifdef W32GUI_DEBUG
PrintfDebugWnd("[%s]\n",buffer);
#endif
        if (bRes==TRUE) {
            break;
        }
        Sleep(300);
    }
    if (bRes==TRUE) {
        *size = (int) dwMessageSize;
        return TRUE;
    } else
        return FALSE;
}
// 無視する版
void ReadFromMailslotIgnore(HANDLE hmailslot, int num)
{
    int i;
    char buffer[10240];
    int size;
    for (i=0;i<num;i++) {
        if (ReadFromMailslot(hmailslot,buffer,&size)==FALSE)
            return;
    }
    return;
}
// メールスロットに書き込む
HANDLE *OpenMailslot(void)
{
    HANDLE hFile;
    hFile = CreateFile(TIMIDITY_MAILSLOT,GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,(HANDLE) NULL);
    if (hFile==INVALID_HANDLE_VALUE)
        return NULL;
    else
        return hFile;
}
void CloseMailslot(HANDLE hmailslot)
{
    CloseHandle(hmailslot);
}
int WriteMailslot(HANDLE hmailslot, char *buffer, int size)
{
	DWORD dwWrittenSize;
	BOOL bRes;
	bRes = WriteFile(hmailslot,buffer,(DWORD)strlen(buffer) + 1,&dwWrittenSize,(LPOVERLAPPED)NULL);
	if(bRes==FALSE){
		return FALSE;
	}
	return TRUE;
}

int isURLFile(const char *filename);
extern volatile DWORD dwWindowThreadID;
volatile argc_argv_t MailslotArgcArgv;
volatile int MailslotThreadTeminateFlag = FALSE;
void w32gMailslotThread(void)
{
    int i;
    char buffer[1024];
    int size;
    MailslotArgcArgv.argc = 0;
    MailslotArgcArgv.argv = NULL;
    for (i=0;i<10;i++) {
        hMailslot = CreateMailslot(TIMIDITY_MAILSLOT,0,MAILSLOT_WAIT_FOREVER,(LPSECURITY_ATTRIBUTES) NULL);
        if (hMailslot != INVALID_HANDLE_VALUE) {
            break;
        }
        hMailslot = NULL;
        Sleep(300);
    }
    if (hMailslot==NULL) {
        return;
    }
    for (;;) {
        Sleep(1000);
        if (MailslotThreadTeminateFlag==TRUE) {
            if (hMailslot!=NULL)
                CloseHandle(hMailslot);
            break;
        }
        for (;;) {
            Sleep(200);
            if (ReadFromMailslot(hMailslot,buffer,&size)==FALSE) {
                Sleep(1000);
                continue;
            }
            if (strcasecmp(buffer,MC_HEADER) !=0) {
                continue;
            }
            if (ReadFromMailslot(hMailslot,buffer,&size)==FALSE) {
                Sleep(1000);
                continue;
            }
            if (strcasecmp(buffer,MC_TERMINATE)==0) {
                CloseHandle(hMailslot);
                w32g_send_rc(RC_STOP, 0);
                w32g_send_rc(RC_QUIT, 0);
//                  PostThreadMessage(dwWindowThreadID,WM_CLOSE,0,0);
//                  PostThreadMessage(dwWindowThreadID,WM_QUIT,0,0);
                Sleep(500);
                return;
            }
            if (strcasecmp(buffer,MC_FILES)==0) {
                char **files;
                int nfiles;
                int flag = TRUE;
                if (ReadFromMailslot(hMailslot,buffer,&size)==FALSE) {
                    continue;
                }
                nfiles = atoi(buffer);
                // MailslotArgcArgv　が初期化されていなかったら処理途中として無視
                if (MailslotArgcArgv.argc!=0 || MailslotArgcArgv.argv!=NULL) {
                    ReadFromMailslotIgnore(hMailslot,nfiles);
                    continue;
                }
                files = (char**) malloc(sizeof(char*) *nfiles);
                if (files==NULL) {
                    ReadFromMailslotIgnore(hMailslot,nfiles);
                    continue;
                }
                for (i=0;i<nfiles;i++) {
                    if (ReadFromMailslot(hMailslot,buffer,&size)==FALSE) {
                        flag = FALSE;
                        break;
                    }
                    files[i] = (char*) malloc(sizeof(char) *(size+1));
                    if (files[i]==NULL) {
                        int j;
                        ReadFromMailslotIgnore(hMailslot,nfiles-i-1);
                        for (j=0;j<i;j++) {
                            safe_free(files[j]);
                        }
                        flag = FALSE;
                        break;
                    }
                    strncpy(files[i],buffer,size);
                    files[i][size] = 0;
                }
                if (flag==FALSE) {
                    safe_free(files);
                    continue;
                }
                MailslotArgcArgv.argc = nfiles;
                MailslotArgcArgv.argv = files;
                // files は別のところで解放してくれる
#ifdef EXT_CONTROL_MAIN_THREAD
                w32g_ext_control_main_thread(RC_EXT_LOAD_FILES_AND_PLAY,(ptr_size_t) &MailslotArgcArgv);
#else
                w32g_send_rc(RC_EXT_LOAD_FILES_AND_PLAY,(ptr_size_t) &MailslotArgcArgv);
//              w32g_send_rc(RC_EXT_LOAD_FILE,(ptr_size_t)) files[0]);
#endif
                continue;
            }
            if (strcasecmp(buffer,MC_PLAYLIST_CLEAR)==0) {
                int param_num;
                if (ReadFromMailslot(hMailslot,buffer,&size)==FALSE) {
                    continue;
                }
                param_num = atoi(buffer);
#ifdef EXT_CONTROL_MAIN_THREAD
                w32g_ext_control_main_thread(RC_EXT_CLEAR_PLAYLIST,0);
#else
                w32g_send_rc(RC_EXT_CLEAR_PLAYLIST,0);
#endif
                ReadFromMailslotIgnore(hMailslot,param_num);
                continue;
            }
            if (strcasecmp(buffer,MC_PLAY)==0) {
                int param_num;
                if (ReadFromMailslot(hMailslot,buffer,&size)==FALSE) {
                    continue;
                }
                param_num = atoi(buffer);
                w32g_send_rc(RC_LOAD_FILE,0);
                ReadFromMailslotIgnore(hMailslot,param_num);
                continue;
            }
            if (strcasecmp(buffer,MC_PLAY_NEXT)==0) {
                int param_num;
                if (ReadFromMailslot(hMailslot,buffer,&size)==FALSE) {
                    continue;
                }
                param_num = atoi(buffer);
                w32g_send_rc(RC_NEXT,0);
                ReadFromMailslotIgnore(hMailslot,param_num);
                continue;
            }
            if (strcasecmp(buffer,MC_PLAY_PREV)==0) {
                int param_num;
                if (ReadFromMailslot(hMailslot,buffer,&size)==FALSE) {
                    continue;
                }
                param_num = atoi(buffer);
                w32g_send_rc(RC_REALLY_PREVIOUS,0);
                ReadFromMailslotIgnore(hMailslot,param_num);
                continue;
            }
            if (strcasecmp(buffer,MC_STOP)==0) {
                int param_num;
                if (ReadFromMailslot(hMailslot,buffer,&size)==FALSE) {
                    continue;
                }
                param_num = atoi(buffer);
                w32g_send_rc(RC_STOP,0);
                ReadFromMailslotIgnore(hMailslot,param_num);
                continue;
            }
            if (strcasecmp(buffer,MC_PAUSE)==0) {
                int param_num;
                if (ReadFromMailslot(hMailslot,buffer,&size)==FALSE) {
                    continue;
                }
                param_num = atoi(buffer);
                w32g_send_rc(RC_PAUSE,0);
                ReadFromMailslotIgnore(hMailslot,param_num);
                continue;
            }
            if (strcasecmp(buffer,MC_SEND_TIMIDITY_INFO)==0) {
                int param_num;
                if (ReadFromMailslot(hMailslot,buffer,&size)==FALSE) {
                    continue;
                }
                param_num = atoi(buffer);
                ReadFromMailslotIgnore(hMailslot,param_num);
                // 何もしない
                continue;
            }
        }
    }
}

#define TIMIDTY_MUTEX_NAME _T("TiMidity_pp_Win32GUI_ver_1_0_0")
static HANDLE hMutexTiMidity = NULL;
// TiMidity が唯一なることを主張します
// その証拠の Mutex を hMutexTiMidity に保持します
int UniqTiMidity(void)
{
    hMutexTiMidity = CreateMutex(NULL,TRUE,TIMIDTY_MUTEX_NAME);
    if (hMutexTiMidity!=NULL && GetLastError()==0) {
        return TRUE;
    }
    if (GetLastError()==ERROR_ALREADY_EXISTS) {
        ;
    }
    if (hMutexTiMidity!=NULL) {
        CloseHandle(hMutexTiMidity);
    }
    hMutexTiMidity = NULL;
    return FALSE;
}

// すでに TiMidity が存在するか
int ExistOldTiMidity(void)
{
    HANDLE hMutex = CreateMutex(NULL,TRUE,TIMIDTY_MUTEX_NAME);
    if (GetLastError()==ERROR_ALREADY_EXISTS) {
        if (hMutex!=NULL)
            CloseHandle(hMutex);
        return TRUE;
    }
    if (hMutex!=NULL)
        CloseHandle(hMutex);
    return FALSE;
}

// 何回か唯一の TiMidity になろうとします
int TryUniqTiMidity(int num)
{
    int i;
    for (i=0;i<num;i++) {
        if (UniqTiMidity()==TRUE) {
            return TRUE;
        }
        Sleep(1000);
    }
    return FALSE;
}

int SendFilesToOldTiMidity(int nfiles, char **files)
{
	int i;
	HANDLE hmailslot;
	char buffer[1024];
	int size;
	hmailslot = OpenMailslot();
	if(hmailslot==NULL)
		return FALSE;
	strcpy(buffer,MC_HEADER);
	size = strlen(buffer); WriteMailslot(hmailslot,buffer,size);
	strcpy(buffer,MC_FILES);
	size = strlen(buffer); WriteMailslot(hmailslot,buffer,size);
	sprintf(buffer,"%d",nfiles);
	size = strlen(buffer); WriteMailslot(hmailslot,buffer,size);
	for(i=0;i<nfiles;i++){
		TCHAR tfilepath[FILEPATH_MAX];
		TCHAR *tfile = char_to_tchar(files[i]);
		TCHAR *p;
//		if(url_check_type(files[i])==-1 && GetFullPathName(files[i],1000,filepath,&p)!=0){
		if(isURLFile(files[i])==FALSE && GetFullPathName(tfile,FILEPATH_MAX-1,tfilepath,&p)!=0){
			char *filepath = tchar_to_char(tfilepath);
			size = strlen(filepath); WriteMailslot(hmailslot,filepath,size);
			safe_free(filepath);
		} else {
			size = strlen(files[i]); WriteMailslot(hmailslot,files[i],size);
		}
	}
	CloseMailslot(hmailslot);
	return TRUE;
}

int SendCommandNoParamOldTiMidity(char *command)
{
    HANDLE hmailslot;
    char buffer[1024];
    int size;
    hmailslot = OpenMailslot();
    if (hmailslot==NULL)
        return FALSE;
    strcpy(buffer,MC_HEADER);
    size = strlen(buffer); WriteMailslot(hmailslot,buffer,size);
    strcpy(buffer,command);
    size = strlen(buffer); WriteMailslot(hmailslot,buffer,size);
    strcpy(buffer,"0");
    size = strlen(buffer); WriteMailslot(hmailslot,buffer,size);
    CloseMailslot(hmailslot);
    return TRUE;
}

int TerminateOldTiMidity(void)
{
    return SendCommandNoParamOldTiMidity(MC_TERMINATE);
}
int ClearPlaylistOldTiMidity(void)
{
    return SendCommandNoParamOldTiMidity(MC_PLAYLIST_CLEAR);
}
int PlayOldTiMidity(void)
{
    return SendCommandNoParamOldTiMidity(MC_PLAY);
}

int PlayNextOldTiMidity(void)
{
    return SendCommandNoParamOldTiMidity(MC_PLAY_NEXT);
}
int PlayPrevOldTiMidity(void)
{
    return SendCommandNoParamOldTiMidity(MC_PLAY_PREV);
}
int StopOldTiMidity(void)
{
    return SendCommandNoParamOldTiMidity(MC_STOP);
}
int PauseOldTiMidity(void)
{
    return SendCommandNoParamOldTiMidity(MC_PAUSE);
}

// ２重起動時の処理
// opt==0 : ファイルを古い TiMidity に渡して自分は終了。古い TiMidity がないときは自分が起動。
//          古いプレイリストはクリアする。
// opt==1 : ファイルを古い TiMidity に渡して自分は終了。古い TiMidity がないときは自分が起動。
//          古いプレイリストはクリアしない。
// opt==2 : 古い TiMidity を終了して、自分が演奏する
// opt==3 : 自分は何もせず終了。古い TiMidity がないときは自分が起動。
// opt==4 : 古い TiMidity を終了して、自分は何もせず終了
// opt==5 : ２重に起動する
// 自分が終了するべきときは FALSE を返す
// 自分が終了するべきでないときは TRUE を返す
int w32gSecondTiMidity(int opt, int argc, char **argv, int nfiles, char **files)
{
    int i;
    switch (opt) {
    case 0:
    case 1:
    case 3:
        if (ExistOldTiMidity()==TRUE) {
            if (opt==3)
                return FALSE;
            if (opt==0)
                ClearPlaylistOldTiMidity();
            SendFilesToOldTiMidity(nfiles, files);
            return FALSE;
        } else {
            if (TryUniqTiMidity(20)==TRUE) {
                w32gStartMailslotThread();
                return TRUE;
            }
            return FALSE;
        }
    case 2:
        if (ExistOldTiMidity()==TRUE) {
            for (i=0;i<=20;i++) {
                TerminateOldTiMidity();
                if (UniqTiMidity()==TRUE) {
                    w32gStartMailslotThread();
                    return TRUE;
                }
                Sleep(1000);
            }
        } else {
            if (TryUniqTiMidity(20)==TRUE) {
                w32gStartMailslotThread();
                return TRUE;
            }
        }
        return FALSE;
    case 4:
        if (ExistOldTiMidity()==TRUE) {
            for (i=0;i<=20;i++) {
                TerminateOldTiMidity();
                if (ExistOldTiMidity()==FALSE) {
                    return FALSE;
                }
                Sleep(1000);
            }
        }
        return FALSE;
    case 5:
        return TRUE;
    default:
        return FALSE;
    }
}

// w32gSecondTiMidity() の後処理
int w32gSecondTiMidityExit(void)
{
    MailslotThreadTeminateFlag = TRUE;
    Sleep(300);
    if (hMailslot!=NULL)
        CloseHandle(hMailslot);
    ReleaseMutex(hMutexTiMidity);
    CloseHandle(hMutexTiMidity);
    return 0;
}

// Before it call timidity_start_initialize()
int isURLFile(const char *filename)
{
    if (strncasecmp(filename,"http://",7)==0
        //|| strncasecmp(filename,"https://",8)==0
        || strncasecmp(filename,"ftp://",6)==0
        || strncasecmp(filename,"news://",7)==0
        || strncasecmp(filename,"file:",5)==0
        || strncasecmp(filename,"dir:",4)==0) {
        return TRUE;
    } else {
        return FALSE;
    }
}
