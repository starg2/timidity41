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
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "controls.h"
#include "timer.h"
#include "bitset.h"

#include "w32g.h"
#include "w32g_res.h"

#define TCTM_MAX_CHANNELS 16
#define BITMAP_SLEEP_SIZEX 96
#define BITMAP_SLEEP_SIZEY 64

#define TM_CANVAS_XMAX 160
#define TM_CANVAS_YMAX 160
#define TM_CANVASMAP_XMAX 16
#define TM_CANVASMAP_YMAX 16
#define TM_SEC_PER_BOX 0.05

#define TM_DISPLAY_SKEYS 24 /* Display Start KEY */
#define TM_DISPLAY_NKEYS 96 /* Display Number of KEY */

static struct
{
    HWND hwnd;
    HWND hParentWnd;
    HDC hdc;
    HDC hmdc;
    HBITMAP hbitmap;
    HBITMAP hSleepBitmap;

    RECT rcMe;	// Whole window region
    RECT rcDr;	// Update rectangle region

    // margin
    int left;
    int top;
    int right;
    int bottom;

    // bar box size.
    int	rectx;
    int recty;
    int spacex;
    int spacey;

    char bar[TCTM_MAX_CHANNELS];
    char bar_old[TCTM_MAX_CHANNELS];
    double bar_alive_time[TCTM_MAX_CHANNELS]; /* 0..0.8 */
    double last_bar_time;
    uint8 xnote[TCTM_MAX_CHANNELS][TM_DISPLAY_NKEYS/8];
    Bitset channel_on_flags[TCTM_MAX_CHANNELS];
} TmCanvas;

int TmCanvasMode = TMCM_SLEEP;

static HWND hCanvasWnd;
static char CanvasWndClassName[] = "TiMidity Canvas";
static LRESULT CALLBACK CanvasWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static void TmCanvasInit(HWND hwnd);
static void TmCanvasEnd(HWND hwnd);
static void TmCanvasRepaint(HWND hwnd);
static void TmCanvasFillRect(RECT *r, COLORREF c);
static HGDIOBJ hgdiobj_hpen, hgdiobj_hbrush;

void w32g_init_canvas(HWND hwnd)
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
      case WM_DESTROY:
	TmCanvasEnd(hwnd);
	break;
      case WM_PAINT:
	TmCanvasRepaint(hwnd);
    	return 0;
      case WM_LBUTTONDBLCLK:
	w32g_send_rc(RC_EXT_MODE_CHANGE, 0);
	break;
      default:
	return DefWindowProc(hwnd,uMess,wParam,lParam) ;
    }
    return 0L;
}

static void TmCanvasInit(HWND hwnd)
{
    RECT rc;
    int i;

    TmCanvas.hwnd = hwnd;
    TmCanvas.hParentWnd = GetParent(TmCanvas.hwnd);
    GetClientRect(TmCanvas.hParentWnd,&rc);
    TmCanvas.rcMe = rc;
    MoveWindow(TmCanvas.hwnd,0,0,rc.right-rc.left,rc.bottom-rc.top,FALSE);
    TmCanvas.left = 3;
    TmCanvas.top = 2;
    TmCanvas.right = 64;
    TmCanvas.bottom = 64;
    TmCanvas.rectx = 5;
    TmCanvas.recty = 2;
    TmCanvas.spacex = 1;
    TmCanvas.spacey = 1;
    TmCanvas.hdc = GetDC(TmCanvas.hwnd);
    TmCanvas.hbitmap = CreateCompatibleBitmap(TmCanvas.hdc,
					      TM_CANVAS_XMAX, TM_CANVAS_YMAX);
    TmCanvas.hmdc = CreateCompatibleDC(TmCanvas.hdc);
    SelectObject(TmCanvas.hmdc,TmCanvas.hbitmap);
    TmCanvas.hSleepBitmap = LoadBitmap(hInst,MAKEINTRESOURCE(IDB_BITMAP_SLEEP));
    for(i = 0; i < TCTM_MAX_CHANNELS; i++)
	init_bitset(&TmCanvas.channel_on_flags[i], 128);
    TmCanvasReset();
}

static void TmCanvasEnd(HWND hwnd)
{
    ReleaseDC(TmCanvas.hwnd, TmCanvas.hdc);
    TmCanvas.hdc = 0;
}

static void TmCanvasRepaint(HWND hwnd)
{
    HDC hdc;
    PAINTSTRUCT ps;

    if(!TmCanvas.hdc)
	return;

    hdc = BeginPaint(TmCanvas.hwnd, &ps);
    BitBltRect(hdc, TmCanvas.hmdc, &ps.rcPaint);
    EndPaint(TmCanvas.hwnd, &ps);
}

static void TmAddRedrawRect(int x, int y, int width, int height)
{
    int x2, y2;

    x2 = x + width;
    y2 = y + height;
    if(TmCanvas.rcDr.left == ~0L)
    {
	TmCanvas.rcDr.left = x;
	TmCanvas.rcDr.top = y;
	TmCanvas.rcDr.right = x2;
	TmCanvas.rcDr.bottom = y2;
    }
    else
    {
	if(TmCanvas.rcDr.left > x)
	    TmCanvas.rcDr.left = x;
	if(TmCanvas.rcDr.top > y)
	    TmCanvas.rcDr.top = y;
	if(TmCanvas.rcDr.right < x2)
	    TmCanvas.rcDr.right = x2;
	if(TmCanvas.rcDr.bottom < y2)
	    TmCanvas.rcDr.bottom = y2;
    }
}

static void TmCanvasResetChannelMode(void)
{
    int i, j, x, y;
    HPEN pen;
    HBRUSH brush;

    pen = tm_colors[TMCC_BACK].pen;
    brush = tm_colors[TMCC_BACK].brush;
    hgdiobj_hpen = SelectObject(TmCanvas.hmdc, pen);
    hgdiobj_hbrush = SelectObject(TmCanvas.hmdc, brush);

    Rectangle(TmCanvas.hmdc,
	      TmCanvas.rcMe.left, TmCanvas.rcMe.top,
	      TmCanvas.rcMe.right, TmCanvas.rcMe.bottom);

    pen = tm_colors[TMCC_FORE_HALF].pen;
    brush = tm_colors[TMCC_FORE_HALF].brush;
    SelectObject(TmCanvas.hmdc, pen);
    SelectObject(TmCanvas.hmdc, brush);

    for(j = 0; j < TM_CANVASMAP_YMAX; j++)
    {
	for(i = 0; i < TM_CANVASMAP_XMAX; i++)
	{
	    x = TmCanvas.left + (TmCanvas.rectx + TmCanvas.spacex) * i;
	    y = TmCanvas.top  + (TmCanvas.recty + TmCanvas.spacey) * j;
	    Rectangle(TmCanvas.hmdc, x, y,
		      x + TmCanvas.rectx, y + TmCanvas.recty);
	}
    }
    
    SelectObject(TmCanvas.hmdc, hgdiobj_hpen);
    SelectObject(TmCanvas.hmdc, hgdiobj_hbrush);
}

static void TmDrawBar(int x, int y, int len, int c)
{
    HPEN pen;
    HBRUSH brush;
    static int last_c;
    int i;

    if(hgdiobj_hpen == NULL)
    {
	pen = tm_colors[c].pen;
	brush = tm_colors[c].brush;
	hgdiobj_hpen = SelectObject(TmCanvas.hmdc, pen);
	hgdiobj_hbrush = SelectObject(TmCanvas.hmdc, hgdiobj_hbrush);
    }
    else if(last_c != c)
    {
	pen = tm_colors[c].pen;
	brush = tm_colors[c].brush;
	SelectObject(TmCanvas.hmdc, pen);
	SelectObject(TmCanvas.hmdc, hgdiobj_hbrush);
    }
    last_c = c;

    y = TM_CANVASMAP_YMAX - y - len; // Reverse up/down

    // Set up start position.
    x = TmCanvas.left + x * (TmCanvas.rectx + TmCanvas.spacex);
    y = TmCanvas.top  + y * (TmCanvas.recty + TmCanvas.spacey);

    for(i = 0; i < len; i++, y += (TmCanvas.recty + TmCanvas.spacey))
    {
	Rectangle(TmCanvas.hmdc, x, y,
		  x + TmCanvas.rectx, y + TmCanvas.recty);
	TmAddRedrawRect(x, y, TmCanvas.rectx, TmCanvas.recty);
    }
}

static void TmCanvasChannelNote(int status, int ch, int note, int vel)
{
    int v;
    double t, past_time;
    unsigned int onoff;
    int i;

    if(ch >= TCTM_MAX_CHANNELS)
	return;

    onoff = (status == VOICE_ON || status == VOICE_SUSTAINED);
    onoff <<= (8 * sizeof(onoff) - 1);
    set_bitset(&TmCanvas.channel_on_flags[ch], &onoff, note, 1);

    t = get_current_calender_time();
    past_time = t - TmCanvas.last_bar_time;
    TmCanvas.last_bar_time = t;

    /* decrease alive time of bar */
    for(i = 0; i < TCTM_MAX_CHANNELS; i++)
	if(!has_bitset(&TmCanvas.channel_on_flags[i]))
	    TmCanvas.bar_alive_time[i] -= past_time;

    /* increase alive time of NoteON bar */
    if(status == VOICE_ON)
    {
	v = vel * w32g_current_volume[ch] * w32g_current_volume[ch]; // 0..2^21
	t = v / (127.0 * 127.0 * 8.0); // 0..16
	t *= TM_SEC_PER_BOX;
	if(TmCanvas.bar_alive_time[ch] < t)
	    TmCanvas.bar_alive_time[ch] = t;
    }

    /* Update bar[] */
    for(i = 0; i < TCTM_MAX_CHANNELS; i++)
    {
	if(TmCanvas.bar_alive_time[i] <= 0)
	{
	    TmCanvas.bar_alive_time[i] = -1.0;
	    v = -1;
	}
	else
	{
	    v = (int)(TmCanvas.bar_alive_time[i] / TM_SEC_PER_BOX);
	    if(v >= TM_CANVASMAP_YMAX)
		v = TM_CANVASMAP_YMAX - 1;
	}
	TmCanvas.bar[i] = v;
    }

    /* Update the display */
    hgdiobj_hpen = hgdiobj_hbrush = 0;
    for(i = 0; i < TCTM_MAX_CHANNELS; i++)
    {
	if(TmCanvas.bar_old[i] < TmCanvas.bar[i])
	    TmDrawBar(i, TmCanvas.bar_old[i] + 1,
		      TmCanvas.bar[i] - TmCanvas.bar_old[i],
		      TMCC_FORE); // Draw bar boxes
	else if(TmCanvas.bar_old[i] > TmCanvas.bar[i])
	    TmDrawBar(i, TmCanvas.bar[i] + 1,
		      TmCanvas.bar_old[i] - TmCanvas.bar[i],
		      TMCC_FORE_HALF); // Clear bar boxes
	TmCanvas.bar_old[i] = TmCanvas.bar[i]; // Save old bar length
    }

    if(hgdiobj_hpen)
    {
	SelectObject(TmCanvas.hmdc, hgdiobj_hpen);
	SelectObject(TmCanvas.hmdc, hgdiobj_hbrush);
    }
}

static void TmCanvasFillRect(RECT *r, COLORREF c)
{
    HPEN hPen;
    HBRUSH hBrush;
    HGDIOBJ hgdiobj_hpen, hgdiobj_hbrush;

    hPen = CreatePen(PS_SOLID, 1, c);
    hBrush = CreateSolidBrush(c);
    hgdiobj_hpen = SelectObject(TmCanvas.hmdc, hPen);
    hgdiobj_hbrush = SelectObject(TmCanvas.hmdc, hBrush);
    Rectangle(TmCanvas.hmdc, r->left, r->top, r->right, r->bottom);
    SelectObject(TmCanvas.hmdc, hgdiobj_hpen);
    DeleteObject(hPen);
    SelectObject(TmCanvas.hmdc, hgdiobj_hbrush);
    DeleteObject(hBrush);
}

#define BITMAP_SLEEP_SIZEX 96
#define BITMAP_SLEEP_SIZEY 64
static void TmCanvasResetSleepMode(void)
{
    HDC hdc;
    int x, y;

    TmCanvasFillRect(&TmCanvas.rcMe, GetSysColor(COLOR_SCROLLBAR));

    hdc = CreateCompatibleDC(TmCanvas.hmdc);
    SelectObject(hdc, TmCanvas.hSleepBitmap);
    x = (TmCanvas.rcMe.right - TmCanvas.rcMe.left - BITMAP_SLEEP_SIZEX)/2;
    y = (TmCanvas.rcMe.bottom - TmCanvas.rcMe.top - BITMAP_SLEEP_SIZEY)/2;
    if(x<0) x = 0;
    if(y<0) y = 0;
    BitBlt(TmCanvas.hmdc, x, y, BITMAP_SLEEP_SIZEX, BITMAP_SLEEP_SIZEY,
	   hdc, 0, 0, SRCCOPY);
    DeleteDC(hdc);
}

static const int black_key[12] = {0,1,0,1,0,0,1,0,1,0,1,0};
static void TmCanvasResetTracerMode(void)
{
    int ch, note, x, y;
    RECT rc;

    TmFillRect(TmCanvas.hmdc, &TmCanvas.rcMe, TMCC_BACK);
    for(ch = 0; ch < 16; ch++)
    {
	rc.left = TmCanvas.left;
	rc.top = TmCanvas.top + ch * 4;
	rc.right = TmCanvas.left + 8 * 12;
	rc.bottom = TmCanvas.top + ch * 4 + 3;
	TmFillRect(TmCanvas.hmdc, &rc, TMCC_WHITE);
	for(note = 0; note < TM_DISPLAY_NKEYS; note++)
	{
	    if(!black_key[(note+TM_DISPLAY_SKEYS)%12])
		continue;
	    x = TmCanvas.left + note;
	    y = TmCanvas.top + ch * 4;
	    SetPixelV(TmCanvas.hmdc, x, y, TmCc(TMCC_BLACK));
	    SetPixelV(TmCanvas.hmdc, x, y+1, TmCc(TMCC_BLACK));
	}
    }
}

static void TmCanvasTracerNote(int status, int ch, int note, int vel)
{
    int prev_key, bitidx, bitmask, is_black, x, y;

    if(note < TM_DISPLAY_SKEYS ||
       note >= TM_DISPLAY_SKEYS + TM_DISPLAY_NKEYS ||
       ch >= TCTM_MAX_CHANNELS)
	return;

    vel = (status == VOICE_ON || status == VOICE_SUSTAINED);
    is_black = black_key[note % 12];
    note -= TM_DISPLAY_SKEYS;
    bitidx = note / 8;
    bitmask = 1<<(note % 8);
    prev_key = !!(TmCanvas.xnote[ch][bitidx] & bitmask);
    if(vel == prev_key)
	return;
    x = TmCanvas.left + note;
    y = TmCanvas.top + ch * 4;
    if(vel)
    {
	/* Note On */
	TmCanvas.xnote[ch][bitidx] |= bitmask;
	SetPixelV(TmCanvas.hmdc, x, y, TmCc(TMCC_RED));
	SetPixelV(TmCanvas.hmdc, x, y+1, TmCc(TMCC_RED));
	if(!is_black)
	    SetPixelV(TmCanvas.hmdc, x, y+2, TmCc(TMCC_RED));
    }
    else
    {
	/* Note Off */
	TmCanvas.xnote[ch][bitidx] &= ~bitmask;
	if(is_black)
	{
	    SetPixelV(TmCanvas.hmdc, x, y, TmCc(TMCC_BLACK));
	    SetPixelV(TmCanvas.hmdc, x, y+1, TmCc(TMCC_BLACK));
	}
	else
	{
	    SetPixelV(TmCanvas.hmdc, x, y, TmCc(TMCC_WHITE));
	    SetPixelV(TmCanvas.hmdc, x, y+1, TmCc(TMCC_WHITE));
	    SetPixelV(TmCanvas.hmdc, x, y+2, TmCc(TMCC_WHITE));
	}
    }
    TmAddRedrawRect(x, y, 1, 3);
}

void TmCanvasReset(void)
{
    int i;

    memset(TmCanvas.xnote, 0, sizeof(TmCanvas.xnote));
    TmCanvas.last_bar_time = get_current_calender_time();
    for(i = 0; i < TCTM_MAX_CHANNELS; i++)
    {
	TmCanvas.bar_alive_time[i] = -1.0;
	clear_bitset(&TmCanvas.channel_on_flags[i], 0, 128);
	TmCanvas.bar[i] = -1;
	TmCanvas.bar_old[i] = -1;
    }
    switch(TmCanvasMode)
    {
      case TMCM_SLEEP:
      default:
	TmCanvasResetSleepMode();
	break;
      case TMCM_CHANNEL:
	TmCanvasResetChannelMode();
	break;
      case TMCM_TRACER:
	TmCanvasResetTracerMode();
	break;
    }

    TmCanvas.rcDr = TmCanvas.rcMe;
    TmCanvasRefresh();
}

int TmCanvasChange(void)
{
    int rc = RC_NONE;

    switch(TmCanvasMode)
    {
      case TMCM_SLEEP:
	TmCanvasMode = TMCM_CHANNEL;
	if(!ctl->trace_playing)
	    rc = RC_SYNC_RESTART;
	ctl->trace_playing = 1;
	break;
      case TMCM_CHANNEL:
	TmCanvasMode = TMCM_TRACER;
	if(!ctl->trace_playing)
	    rc = RC_SYNC_RESTART;
	ctl->trace_playing = 1;
	break;
      case TMCM_TRACER:
	TmCanvasMode = TMCM_SLEEP;
	ctl->trace_playing = 0;
	break;
      default:
	TmCanvasMode = TMCM_SLEEP;
	ctl->trace_playing = 0;
	break;
    }
    TmCanvasReset();
    return rc;
}

void TmCanvasRefresh(void)
{
    if(!TmCanvas.hdc)
	return;
    if(TmCanvas.rcDr.left != ~0L)
    {
	BitBltRect(TmCanvas.hdc, TmCanvas.hmdc, &TmCanvas.rcDr);
	TmCanvas.rcDr.left = ~0L;
    }
}

void TmCanvasNote(int status, int ch, int note, int vel)
{
    if(ch >= TCTM_MAX_CHANNELS || !TmCanvas.hdc)
	return;
    switch(TmCanvasMode)
    {
      default:
      case TMCM_SLEEP:
	break;
      case TMCM_CHANNEL:
	TmCanvasChannelNote(status, ch, note, vel);
	break;
      case TMCM_TRACER:
	TmCanvasTracerNote(status, ch, note, vel);
	break;
    }
}
