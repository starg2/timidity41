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
#include <string.h>
#include <windows.h>
#include "timidity.h"
#include "common.h"
#include "w32g.h"
#include "w32g_res.h"

#define PanelWndClassName "TiMidity Panel"

#define TM_PANEL_XMAX 350
#define TM_PANEL_YMAX 100

#define TMP_DONOT 1
#define TMP_PREPARE 2
#define TMP_DONE 3

#define TMP_SPACE 2
#define TMP_TITLE_CHAR_SIZE 12
#define TMP_FILE_CHAR_SIZE 10
#define TMP_MISCH_CHAR_SIZE 9
#define TMP_MISCW_CHAR_SIZE 7
#define TMP_MISC_CHAR_SIZE 12
#define TMP_3L_CSIZE TMP_MISC_CHAR_SIZE
#define TMP_3L_CSSIZE TMP_MISCH_CHAR_SIZE
#define TMP_3L_SPACE 1
#define TMP_4L_CSIZE TMP_MISC_CHAR_SIZE
#define TMP_4L_CSSIZE TMP_MISCH_CHAR_SIZE
#define TMP_4L_SPACE 1

static HFONT hTitleFont = NULL;
static HFONT hFileFont  = NULL;
static HFONT hMiscHFont = NULL;
static HFONT hMiscFont  = NULL;

extern char *PlayerFont;

#define TMP_FONT_SIZE 12
#define TMP_FONTSMALL_SIZE 10
static struct {
    HWND hwnd;
    HDC hdc;
    HDC hmdc;

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

    int TimeDone;
    int VoicesDone;
    int MasterVolumeDone;
    int ListDone;

    int cur_time;
    int total_time;
    int cur_voices;
    int master_volume;
} TmPanel;

static int tm_refresh_flag = 0;

static LRESULT CALLBACK PanelWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static void TmPanelInit(HWND hwnd);
static void TmPanelEnd(HWND hwnd);
static void TmPanelInitFont(void);
static void TmPanelClear(void);
static void TmPanelFullReset(void);
static void TmPanelRect(RECT *rc, int color);
static void TmPanelRepaint(HWND hwnd);
static void TmPanelUpdate(void);

void w32g_init_panel(HWND hwnd)
{
    WNDCLASS wndclass;
    HWND hPanelWnd;

    memset(&TmPanel, 0, sizeof(TmPanel));
    TmPanelInitFont();

    wndclass.style         = CS_HREDRAW | CS_VREDRAW;
    wndclass.lpfnWndProc   = PanelWndProc;
    wndclass.cbClsExtra    = 0;
    wndclass.cbWndExtra    = 0;
    wndclass.hInstance     = hInst;
    wndclass.hIcon         = NULL;
    wndclass.hCursor       = LoadCursor(0,IDC_ARROW);
    wndclass.hbrBackground = (HBRUSH)(COLOR_SCROLLBAR + 1);
    wndclass.lpszMenuName  = NULL;
    wndclass.lpszClassName = PanelWndClassName;
    RegisterClass(&wndclass);
    hPanelWnd = CreateWindowEx(0, PanelWndClassName, 0, WS_CHILD,
			       CW_USEDEFAULT, 0, TM_PANEL_XMAX, TM_PANEL_YMAX,
			       GetDlgItem(hwnd, IDC_RECT_PANEL), 0 ,hInst, 0);
    ShowWindow(hPanelWnd,SW_SHOW);
    UpdateWindow(hPanelWnd);
}

static LRESULT CALLBACK PanelWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
    switch (uMess)
    {
      case WM_CREATE:
	TmPanelInit(hwnd);
	break;
      case WM_DESTROY:
	TmPanelEnd(hwnd);
	break;

      case WM_PAINT:
	TmPanelRepaint(hwnd);
	break;
      default:
	return DefWindowProc(hwnd,uMess,wParam,lParam) ;
    }
    return 0L;
}

static int TmPanelTextLength(HFONT hfont, char *text)
{
    SIZE size;
    SelectObject(TmPanel.hmdc, hfont);    
    SetTextAlign(TmPanel.hmdc, TA_LEFT | TA_TOP | TA_NOUPDATECP);
    GetTextExtentPoint32(TmPanel.hmdc, text, strlen(text), &size);
    return size.cx;
}

static void TmPanelInit(HWND hwnd)
{
    int i, top, bottom;
    RECT rc;
    HBITMAP hbitmap;
    HWND hParentWnd;

    TmPanel.hwnd = hwnd;
    hParentWnd = GetParent(TmPanel.hwnd);
    GetClientRect(hParentWnd, &rc);
    MoveWindow(TmPanel.hwnd, 0, 0, rc.right-rc.left, rc.bottom-rc.top, FALSE);
    TmPanel.hdc = GetDC(TmPanel.hwnd);
    hbitmap = CreateCompatibleBitmap(TmPanel.hdc,
				     TM_PANEL_XMAX, TM_PANEL_YMAX);
    TmPanel.hmdc = CreateCompatibleDC(TmPanel.hdc);
    SelectObject(TmPanel.hmdc, hbitmap);
    GetClientRect(TmPanel.hwnd, &rc);
    TmPanel.rcMe = rc;
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
    TmPanel.rcTimeH.right = TmPanel.rcTimeH.left + TmPanelTextLength(hMiscHFont, "TIME");
    TmPanel.rcTimeH.bottom = bottom - i;
    TmPanel.rcTime.left = TmPanel.rcTimeH.right + 3;
    TmPanel.rcTime.top = top;
    TmPanel.rcTime.right = TmPanel.rcTime.left + TmPanelTextLength(hMiscFont, "00:00:00/00:00:00");
    TmPanel.rcTime.bottom = bottom;
    TmPanel.rcVoicesH.left = TmPanel.rcTime.right + 4;
    TmPanel.rcVoicesH.top = top + i;
    TmPanel.rcVoicesH.right = TmPanel.rcVoicesH.left + TmPanelTextLength(hMiscHFont, "VOICES");
    TmPanel.rcVoicesH.bottom = bottom - i;
    TmPanel.rcVoices.left = TmPanel.rcVoicesH.right + TMP_3L_SPACE;
    TmPanel.rcVoices.top = top;
    TmPanel.rcVoices.right = TmPanel.rcVoices.left + TmPanelTextLength(hMiscFont, "000/000");
    TmPanel.rcVoices.bottom = bottom;
    TmPanel.rcMasterVolumeH.left = TmPanel.rcVoices.right + 4;
    TmPanel.rcMasterVolumeH.top = top + i;
    TmPanel.rcMasterVolumeH.right = TmPanel.rcMasterVolumeH.left + TMP_MISCW_CHAR_SIZE*4;
    TmPanel.rcMasterVolumeH.bottom = bottom - i;
    TmPanel.rcMasterVolume.left = TmPanel.rcMasterVolumeH.right + 3;
    TmPanel.rcMasterVolume.top = top;
    TmPanel.rcMasterVolume.right = TmPanel.rcMasterVolume.left + TmPanelTextLength(hMiscFont, "000%");
    TmPanel.rcMasterVolume.bottom = bottom;
    top = TmPanel.rcTime.bottom + TMP_SPACE;
    bottom = top + TMP_4L_CSIZE;
    i = (TMP_4L_CSIZE - TMP_4L_CSSIZE)/2;
    TmPanel.rcListH.left = TmPanel.rcMe.left + TMP_SPACE;
    TmPanel.rcListH.top = top + i;
    TmPanel.rcListH.right = TmPanel.rcListH.left + TmPanelTextLength(hMiscHFont, "LIST");
    TmPanel.rcListH.bottom = bottom - i;
    TmPanel.rcList.left = TmPanel.rcListH.right + 3;
    TmPanel.rcList.top = top;
    TmPanel.rcList.right = TmPanel.rcList.left + TmPanelTextLength(hMiscFont, "0000/0000");
    TmPanel.rcList.bottom = bottom;
    TmPanelFullReset();
    TmPanelUpdate();
}

static void TmPanelEnd(HWND hwnd)
{
    ReleaseDC(TmPanel.hwnd, TmPanel.hdc);
    TmPanel.hdc = 0;
}

static void TmPanelFullReset(void)
{
    TmPanelClear();
    TmPanel.TimeDone = TMP_DONOT;
    TmPanel.VoicesDone = TMP_DONOT;
    TmPanel.MasterVolumeDone = TMP_DONOT;
    TmPanelRect(&TmPanel.rcTitle, TMCC_FORE_HALF);
    TmPanelRect(&TmPanel.rcFile, TMCC_FORE_HALF);
    TmPanel.cur_time = -1;
    TmPanel.total_time = -1;
    TmPanel.cur_voices = -1;
    TmPanel.master_volume = -1;
}

// Font Setting
static void TmPanelInitFont(void)
{
    if(hTitleFont != NULL)
	DeleteObject(hTitleFont);
    hTitleFont = CreateFont(TMP_TITLE_CHAR_SIZE, 0, 0, 0, FW_DONTCARE,
			    FALSE, FALSE, FALSE, DEFAULT_CHARSET,
			    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			    DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
			    PlayerFont);
    if(hFileFont != NULL)
	DeleteObject(hFileFont);
    hFileFont = CreateFont(TMP_FILE_CHAR_SIZE, 0, 0, 0, FW_DONTCARE,
			   FALSE, FALSE, FALSE, DEFAULT_CHARSET,
			   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			   DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
			   PlayerFont);
    if(hMiscHFont != NULL)
	DeleteObject(hMiscHFont);
    hMiscHFont = CreateFont(TMP_MISCH_CHAR_SIZE, 0, 0, 0, FW_BOLD, FALSE,
			    FALSE,FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
			    CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
			    DEFAULT_PITCH | FF_DONTCARE, PlayerFont);
    if(hMiscFont != NULL)
	DeleteObject(hMiscFont);
    hMiscFont = CreateFont(TMP_MISC_CHAR_SIZE, 0, 0, 0, FW_BOLD, FALSE, FALSE,
			   FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
			   CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
			   DEFAULT_PITCH | FF_DONTCARE, PlayerFont);
}

static void TmPanelClear(void)
{
    TmPanelRect(&TmPanel.rcMe, TMCC_BACK);
}

static void TmPanelRepaint(HWND hwnd)
{
    HDC hdc;
    PAINTSTRUCT ps;

    hdc = BeginPaint(TmPanel.hwnd, &ps);
    BitBltRect(hdc, TmPanel.hmdc, &ps.rcPaint);
    EndPaint(TmPanel.hwnd, &ps);
}

static void TmPanelText(RECT *r, HFONT hfont, int fg, int bg,
			char *buffer, BOOL bitblt)
{
    HGDIOBJ hgdiobj;

    if(!TmPanel.hdc)
	return;

    hgdiobj = SelectObject(TmPanel.hmdc, hfont);
    SetTextColor(TmPanel.hmdc, TmCc(fg));
    SetBkColor(TmPanel.hmdc, TmCc(bg));
    SetTextAlign(TmPanel.hmdc, TA_LEFT | TA_TOP | TA_NOUPDATECP);
    ExtTextOut(TmPanel.hmdc, r->left, r->top, ETO_CLIPPED, r,
	       buffer, strlen(buffer), NULL);
    if(bitblt)
	BitBltRect(TmPanel.hdc, TmPanel.hmdc, r);
}

static void TmPanelUpdate(void)
{
    char buffer[32];
    extern int voices;

    if(TmPanel.VoicesDone == TMP_PREPARE)
    {
	sprintf(buffer,"%03d/%03d", TmPanel.cur_voices, voices);
	TmPanelText(&TmPanel.rcVoices, hMiscFont, TMCC_FORE, TMCC_FORE_HALF,
		    buffer, TRUE);
	TmPanel.VoicesDone = TMP_DONE;
    }

    if(TmPanel.TimeDone == TMP_PREPARE)
    {
	int ch, cm, cs;
	int th, tm, ts;
	int t;

	t = TmPanel.cur_time;
	ch = t / 60 / 60;
	t %= 60*60;
	cm = t / 60;
	t %= 60;
	cs = t;

	t = TmPanel.total_time;
	th = t / 60 / 60;
	t %= 60*60;
	tm = t / 60;
	t %= 60;
	ts = t;

	sprintf(buffer,"%02d:%02d:%02d/%02d:%02d:%02d",
		ch, cm, cs, th, tm, ts);
	TmPanelText(&TmPanel.rcTime, hMiscFont, TMCC_FORE, TMCC_FORE_HALF,
		    buffer, TRUE);
	TmPanel.TimeDone = TMP_DONE;
    }

    if(TmPanel.MasterVolumeDone == TMP_PREPARE)
    {
	sprintf(buffer, "%03d%%", TmPanel.master_volume);
	TmPanelText(&TmPanel.rcMasterVolume, hMiscFont,
		    TMCC_FORE, TMCC_FORE_HALF, buffer, TRUE);
	TmPanel.MasterVolumeDone = TMP_DONE;
    }

    if(TmPanel.ListDone == TMP_PREPARE)
    {
	int playlist_selected, playlist_nfiles;
	w32g_get_playlist_index(&playlist_selected, &playlist_nfiles, NULL);
	if(playlist_nfiles == 0)
	    strcpy(buffer,"0000/0000");
	else
	    sprintf(buffer,"%04d/%04d",
		    playlist_selected + 1, playlist_nfiles);
	TmPanelText(&TmPanel.rcList, hMiscFont, TMCC_FORE, TMCC_FORE_HALF,
		    buffer, TRUE);
	TmPanel.ListDone = TMP_DONE;
    }
}

static void TmPanelRect(RECT *rc, int color)
{
    TmFillRect(TmPanel.hmdc, rc, color);
}

void TmPanelRefresh(void)
{
    if(!TmPanel.hdc)
	return;
    if(tm_refresh_flag)
    {
	TmPanelUpdate();
	tm_refresh_flag = 0;
    }
}

void TmPanelStartToLoad(char *filename)
{
    char *title;
    extern char *get_midi_title(char *);
    extern int amplification;

    title = get_midi_title(filename);
    TmPanelFullReset();
    /* Title */
    if(title)
	TmPanelText(&TmPanel.rcTitle, hTitleFont, TMCC_FORE, TMCC_FORE_HALF,
		    title, FALSE);

    /* File name */
    TmPanelText(&TmPanel.rcFile, hFileFont, TMCC_FORE, TMCC_FORE_HALF,
		filename, FALSE);

    /*
     * Labels
     */

    /* "TIME" */
    TmPanelText(&TmPanel.rcTimeH, hMiscHFont, TMCC_FORE, TMCC_BACK,
		"TIME", FALSE);

    /* "VOICES" */
    TmPanelText(&TmPanel.rcVoicesH, hMiscHFont, TMCC_FORE, TMCC_BACK,
		"VOICES", FALSE);

    /* "M.Vol" */
    TmPanelText(&TmPanel.rcMasterVolumeH, hMiscHFont,
		TMCC_FORE, TMCC_BACK,
		"M.Vol", FALSE);

    /* "LIST" */
    TmPanelText(&TmPanel.rcListH, hMiscHFont, TMCC_FORE, TMCC_BACK,
		"LIST", FALSE);

    BitBltRect(TmPanel.hdc, TmPanel.hmdc, &TmPanel.rcMe);

    TmPanel.cur_voices = 0;
    TmPanel.master_volume = amplification;
    TmPanel.VoicesDone = TMP_PREPARE;
    TmPanel.MasterVolumeDone = TMP_PREPARE;
    TmPanel.ListDone = TMP_PREPARE;
    TmPanelRefresh();
    tm_refresh_flag = 0;
}

void TmPanelStartToPlay(int total_sec)
{
    TmPanel.cur_time = 0;
    TmPanel.total_time = total_sec;
    TmPanel.TimeDone = TMP_PREPARE;
    tm_refresh_flag = 1;
}

void TmPanelSetVoices(int v)
{
    if(TmPanel.cur_voices != v)
    {
	TmPanel.cur_voices = v;
        TmPanel.VoicesDone = TMP_PREPARE;
	tm_refresh_flag = 1;
    }
}

void TmPanelSetTime(int sec)
{
    if(TmPanel.cur_time != sec)
    {
	TmPanel.cur_time = sec;
	TmPanel.TimeDone = TMP_PREPARE;
	tm_refresh_flag = 1;
    }
}

void TmPanelSetMasterVol(int v)
{
    if(TmPanel.master_volume != v)
    {
	TmPanel.master_volume = v;
	TmPanel.MasterVolumeDone = TMP_PREPARE;
	tm_refresh_flag = 1;
	if(!w32g_play_active)
	    TmPanelRefresh();
    }
}

void TmPanelUpdateList(void)
{
    TmPanel.ListDone = TMP_PREPARE;
    tm_refresh_flag = 1;
    if(!w32g_play_active)
	TmPanelRefresh();
}
