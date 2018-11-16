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

    w32g_int_synth_editor.c
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
#include "mid.defs"
#include "support.h"

#include "w32g.h"
#include "w32g_utl.h"
#include <shlobj.h>
#include <commctrl.h>
#include <windowsx.h>
#include "w32g_res.h"

#ifdef INT_SYNTH
#include "int_synth.h"
#include "w32g_int_synth_editor.h"

static int focus_wnd = 0;

static int scc_data_num = 0;
static int scc_preset_num = 0;
static int mms_preset_num = 0;
static int mms_op_num = 0;
static char ISIniFileOpenDir[FILEPATH_MAX] = "";

#define WM_ISE_RESTORE (WM_USER + 102)

#ifdef DLG_CHECKBUTTON_TO_FLAG
#undef DLG_CHECKBUTTON_TO_FLAG
#endif
#define DLG_CHECKBUTTON_TO_FLAG(hwnd,ctlid,x)	\
	((SendDlgItemMessage((hwnd),(ctlid),BM_GETCHECK,0,0))?((x)=1):((x)=0))

#ifdef DLG_FLAG_TO_CHECKBUTTON
#undef DLG_FLAG_TO_CHECKBUTTON
#endif
#define DLG_FLAG_TO_CHECKBUTTON(hwnd,ctlid,x)	\
	((x)?(SendDlgItemMessage((hwnd),(ctlid),BM_SETCHECK,1,0)):\
	(SendDlgItemMessage((hwnd),(ctlid),BM_SETCHECK,0,0)))

#ifdef CHECKRANGE_ISEDITOR_PARAM
#undef CHECKRANGE_ISEDITOR_PARAM
#endif
#define CHECKRANGE_ISEDITOR_PARAM(SV, SMIN,SMAX) SV = ((SV < SMIN) ?SMIN : ((SV > SMAX) ? SMAX : SV))

static LRESULT APIENTRY CALLBACK ISEditorSCCDATAProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	int i, tmp, num;
	WORD clid = 0;
	static WORD focus_clid = 0;
	int16 wheel_speed = 0;
	static HWND hISEScrollbarValWnd[32];

	switch (uMess) {
	case WM_INITDIALOG:
		DLG_FLAG_TO_CHECKBUTTON(hwnd, IDC_CHK_SCC_DATA_OVERRIDE, scc_data_editor_override);		
		SendDlgItemMessage(hwnd, IDC_SLIDER_SCC_DATA_NUM, TBM_SETRANGEMAX, (WPARAM) 0, (LPARAM) SCC_DATA_MAX - 1);
		SendDlgItemMessage(hwnd, IDC_SLIDER_SCC_DATA_NUM, TBM_SETRANGEMIN, (WPARAM) 0, (LPARAM) 0);
		TCHAR *tname = char_to_tchar(scc_data_editor_load_name(scc_data_num));
		SetDlgItemText(hwnd,IDC_EDIT_SCC_DATA_NAME, tname);
		safe_free(tname);
		for(i = 0; i < SCC_DATA_LENGTH; i++){
			int sb_id = IDC_SCROLLBAR_SCC_DATA_VAL1 + i;
			hISEScrollbarValWnd[i] = GetDlgItem(hwnd, sb_id);
			EnableScrollBar(hISEScrollbarValWnd[i], SB_CTL, ESB_ENABLE_BOTH);
			SetScrollRange(hISEScrollbarValWnd[i], SB_CTL, 0, 256, TRUE); // 256 -128 ~ 128
		}
	case WM_ISE_RESTORE:
		DLG_FLAG_TO_CHECKBUTTON(hwnd, IDC_CHK_SCC_DATA_OVERRIDE, scc_data_editor_override);		
		SendDlgItemMessage(hwnd, IDC_SLIDER_SCC_DATA_NUM, TBM_SETPOS, (WPARAM) 1, (LPARAM) scc_data_num);
		SetDlgItemInt(hwnd, IDC_EDIT_SCC_DATA_NUM, scc_data_num, TRUE);
		for(i = 0; i < SCC_DATA_LENGTH; i++){
			int ed_id = IDC_EDIT_SCC_DATA_VAL1 + i;
			tmp = scc_data_editor_get_param(i);
			SetDlgItemInt(hwnd, ed_id, tmp, TRUE);
			SetScrollPos(hISEScrollbarValWnd[i], SB_CTL, 128 - tmp, TRUE);
		}		
		break;
	case WM_HSCROLL:
	case WM_VSCROLL:
		tmp = SendDlgItemMessage(hwnd, IDC_SLIDER_SCC_DATA_NUM, TBM_GETPOS, (WPARAM) 0, (LPARAM)0);
		if(tmp != scc_data_num){
			scc_data_num = tmp;
			SetDlgItemInt(hwnd, IDC_EDIT_SCC_DATA_NUM, tmp, TRUE);	
			break;
		}else{
			int nScrollCode = (int) LOWORD(wParam);
			int nPos = (int) HIWORD(wParam);
			HWND bar = (HWND) lParam;
			static int pos = -1;

			num = -1;
			for(i = 0; i < SCC_DATA_LENGTH; i++){	
				if(bar == hISEScrollbarValWnd[i]){
					num = i;
					break;
				}
			}
			if(num == -1)
				break;
			switch(nScrollCode){
			case SB_THUMBTRACK:
			case SB_THUMBPOSITION:
				pos = nPos;
				break;
			case SB_LINEUP:
			case SB_PAGEUP:
				pos = GetScrollPos(bar, SB_CTL) - (nScrollCode == SB_LINEUP ? 1 : 10);
				if(pos < 0)
					pos = 0;
				SetScrollPos(bar, SB_CTL, pos, TRUE);
				break;
			case SB_LINEDOWN:
			case SB_PAGEDOWN:
				pos = GetScrollPos(bar, SB_CTL) + (nScrollCode == SB_LINEDOWN ? 1 : 10);
				if(pos > 256)
					pos = 256;
				SetScrollPos(bar, SB_CTL, pos, TRUE);
				break;
			case SB_ENDSCROLL:
				if(pos != -1){
					SetScrollPos(bar, SB_CTL, pos, TRUE);
					pos = -1;
				}
				break;
			}
			if (pos == -1)
				break;
			tmp = 128 - pos;
			if(tmp != scc_data_editor_get_param(num)){	
				scc_data_editor_set_param(num, tmp);
				SetDlgItemInt(hwnd, IDC_EDIT_SCC_DATA_VAL1 + num, tmp, TRUE);
				break;
			}
		}
		break;
	case WM_MOUSEWHEEL:		
		if(focus_wnd != 1)
			break;
		wheel_speed = (int16)GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA; // upper 16bit sined int // 1knoch = 120
		wheel_speed *= (LOWORD(wParam) & MK_SHIFT) ? 10 : 1;
		if(focus_clid == IDC_EDIT_SCC_DATA_NUM){
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;	
			if(tmp < 0) 
				tmp = 0;
			else if(tmp > (SCC_DATA_MAX - 1))
				tmp = SCC_DATA_MAX - 1;
			scc_data_num = tmp;			
			SetDlgItemInt(hwnd, IDC_EDIT_SCC_DATA_NUM, scc_data_num, TRUE);
			SendDlgItemMessage(hwnd, IDC_SLIDER_SCC_DATA_NUM, TBM_SETPOS, (WPARAM) 1, (LPARAM) scc_data_num);
		}else if(focus_clid >= IDC_EDIT_SCC_DATA_VAL1 && focus_clid <= IDC_EDIT_SCC_DATA_VAL32){
			num = focus_clid - IDC_EDIT_SCC_DATA_VAL1;
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;			
			if(tmp < -128) 
				tmp = -128;
			else if(tmp > 128)
				tmp = 128;		
			if(tmp != scc_data_editor_get_param(num)){	
				scc_data_editor_set_param(num, tmp);	
				SetScrollPos(hISEScrollbarValWnd[num], SB_CTL, 128 - tmp, TRUE);
				SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			}
		}
		break;	
	case WM_COMMAND:
		clid = LOWORD(wParam);
		focus_clid = clid;
		focus_wnd = 1;
		switch (clid) {
		case IDC_CHK_SCC_DATA_OVERRIDE:			
			DLG_CHECKBUTTON_TO_FLAG(hwnd, IDC_CHK_SCC_DATA_OVERRIDE, scc_data_editor_override);
			break;
		case IDC_BUTTON_SCC_DATA_LOAD_PRESET:
			SetDlgItemText(hwnd,IDC_EDIT_SCC_DATA_NAME, scc_data_editor_load_name(scc_data_num));
			scc_data_editor_load_preset(scc_data_num);
			SendMessage(hwnd, WM_ISE_RESTORE, (WPARAM)0, (LPARAM)0 );
			break;
		case IDC_BUTTON_SCC_DATA_SAVE_PRESET:	
			{
				char buff[256];
				GetDlgItemText(hwnd, IDC_EDIT_SCC_DATA_NAME, buff, (WPARAM)sizeof(buff));
				scc_data_editor_store_name(scc_data_num, (const char *)buff);
				scc_data_editor_store_preset(scc_data_num);
			}		
			break;
		case IDC_BUTTON_SCC_DATA_LOAD_TEMP:					
			scc_data_editor_load_preset(-1);
			SendMessage(hwnd, WM_ISE_RESTORE, (WPARAM)0, (LPARAM)0 );
			break;
		case IDC_BUTTON_SCC_DATA_SAVE_TEMP:			
			scc_data_editor_store_preset(-1);
			break;
		case IDC_EDIT_SCC_DATA_NUM:		
			tmp = (int)GetDlgItemInt(hwnd, IDC_EDIT_SCC_DATA_NUM, NULL, TRUE);
			if(tmp < 0) 
				tmp = 0;
			else if(tmp > (SCC_DATA_MAX - 1))
				tmp = SCC_DATA_MAX - 1;
			scc_data_num = tmp;				
			SendDlgItemMessage(hwnd, IDC_SLIDER_SCC_DATA_NUM, TBM_SETPOS, (WPARAM) 1, (LPARAM) scc_data_num);
			break;
		case IDC_BUTTON_SCC_DATA_CLEAR:
			scc_data_editor_clear_param();
			SendMessage(hwnd, WM_ISE_RESTORE, (WPARAM)0, (LPARAM)0 );		
			break;
		default:	
			if(clid >= IDC_EDIT_SCC_DATA_VAL1 && clid <= IDC_EDIT_SCC_DATA_VAL32){
				num = clid - IDC_EDIT_SCC_DATA_VAL1;			
				tmp = (int)GetDlgItemInt(hwnd, clid, NULL, TRUE);
				if(tmp < -128) 
					tmp = -128;
				else if(tmp > 128)
					tmp = 128;		
				if(tmp != scc_data_editor_get_param(num)){	
					scc_data_editor_set_param(num, tmp);	
					SetScrollPos(hISEScrollbarValWnd[num], SB_CTL, 128 - tmp, TRUE);	
					SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
				}
			}
			break;
		}
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


static LRESULT APIENTRY CALLBACK ISEditorSCCProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	int i, tmp;
	WORD clid = 0;
	static WORD focus_clid = 0;
	int16 wheel_speed = 0;

	switch (uMess) {
	case WM_INITDIALOG:		
		SendDlgItemMessage(hwnd, IDC_SLIDER_SCC_NUM, TBM_SETRANGEMAX, (WPARAM) 0, (LPARAM) SCC_SETTING_MAX - 1);
		SendDlgItemMessage(hwnd, IDC_SLIDER_SCC_NUM, TBM_SETRANGEMIN, (WPARAM) 0, (LPARAM) 0);
		SetDlgItemText(hwnd,IDC_EDIT_SCC_NAME, scc_editor_load_name(scc_preset_num));
	case WM_ISE_RESTORE:
		DLG_FLAG_TO_CHECKBUTTON(hwnd, IDC_CHK_SCC_OVERRIDE, scc_editor_override);		
		SendDlgItemMessage(hwnd, IDC_SLIDER_SCC_NUM, TBM_SETPOS, (WPARAM) 1, (LPARAM) scc_preset_num);
		SetDlgItemInt(hwnd, IDC_EDIT_SCC_NUM, scc_preset_num, TRUE);
		SetDlgItemInt(hwnd, IDC_EDIT_SCC_G1_P1, scc_editor_get_param(0, 0), TRUE); // pram
		for(i = 0; i < SCC_OSC_MAX; i++)
			SetDlgItemInt(hwnd, IDC_EDIT_SCC_G2_P1 + i, scc_editor_get_param(1, i), TRUE); // osc
		SetDlgItemInt(hwnd, IDC_EDIT_SCC_G3_P1, scc_editor_get_param(2, 0), TRUE); // amp
		SetDlgItemInt(hwnd, IDC_EDIT_SCC_G4_P1, scc_editor_get_param(3, 0), TRUE); // pitch
		SetDlgItemInt(hwnd, IDC_EDIT_SCC_G4_P2, scc_editor_get_param(3, 1), TRUE);
		SetDlgItemText(hwnd, IDC_EDIT_SCC_WAVE1_DATA_NAME, scc_editor_load_wave_name(0));
		SetDlgItemText(hwnd, IDC_EDIT_SCC_WAVE2_DATA_NAME, scc_editor_load_wave_name(1));
		for(i = 0; i < SCC_ENV_PARAM; i++)
			SetDlgItemInt(hwnd, IDC_EDIT_SCC_G5_P1 + i, scc_editor_get_param(4, i), TRUE);
		for(i = 0; i < SCC_ENV_PARAM; i++)
			SetDlgItemInt(hwnd, IDC_EDIT_SCC_G6_P1 + i, scc_editor_get_param(5, i), TRUE);
		for(i = 0; i < SCC_LFO_PARAM; i++)
			SetDlgItemInt(hwnd, IDC_EDIT_SCC_G7_P1 + i, scc_editor_get_param(6, i), TRUE);
		for(i = 0; i < SCC_LFO_PARAM; i++)
			SetDlgItemInt(hwnd, IDC_EDIT_SCC_G8_P1 + i, scc_editor_get_param(7, i), TRUE);
		SetDlgItemInt(hwnd, IDC_EDIT_SCC_G9_P1, scc_editor_get_param(8, 0), TRUE); // loop
		SetDlgItemText(hwnd, IDC_EDIT_SCC_LFO1_WAVE_NAME, scc_editor_load_wave_name(8));
		SetDlgItemText(hwnd, IDC_EDIT_SCC_LFO2_WAVE_NAME, scc_editor_load_wave_name(9));
		break;
	case WM_HSCROLL:
	case WM_VSCROLL:
			tmp = SendDlgItemMessage(hwnd, IDC_SLIDER_SCC_NUM, TBM_GETPOS, (WPARAM) 0, (LPARAM)0);
			if(tmp != scc_preset_num){
				scc_preset_num = tmp;
				SetDlgItemInt(hwnd, IDC_EDIT_SCC_NUM, tmp, TRUE);	
			}
			break;
	case WM_MOUSEWHEEL:	
		if(focus_wnd != 2)
			break;	
		wheel_speed = (int16)GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA; // upper 16bit sined int // 1knoch = 120		
		wheel_speed *= (LOWORD(wParam) & MK_SHIFT) ? 10 : 1;
		switch (focus_clid) {
		case IDC_EDIT_SCC_NUM:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;	
			if(tmp < 0) 
				tmp = 0;
			else if(tmp > (SCC_SETTING_MAX - 1))
				tmp = SCC_SETTING_MAX - 1;
			scc_preset_num = tmp;		
			SetDlgItemInt(hwnd, IDC_EDIT_SCC_NUM, scc_preset_num, TRUE);		
			SendDlgItemMessage(hwnd, IDC_SLIDER_SCC_NUM, TBM_SETPOS, (WPARAM) 1, (LPARAM) scc_preset_num);	
			break;
		case IDC_EDIT_SCC_G1_P1:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;			
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			scc_editor_set_param(0, 0, tmp); // param
			break;
		case IDC_EDIT_SCC_G2_P1:
		case IDC_EDIT_SCC_G2_P2:
		case IDC_EDIT_SCC_G2_P3:
		case IDC_EDIT_SCC_G2_P4:
		case IDC_EDIT_SCC_G2_P5:
		case IDC_EDIT_SCC_G2_P6:
		case IDC_EDIT_SCC_G2_P7:
		case IDC_EDIT_SCC_G2_P8:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);	
			scc_editor_set_param(1, focus_clid - IDC_EDIT_SCC_G2_P1, tmp); // osc
			if(focus_clid == IDC_EDIT_SCC_G2_P4)
				SetDlgItemText(hwnd, IDC_EDIT_SCC_WAVE1_DATA_NAME, scc_editor_load_wave_name(0));	
			else if(focus_clid == IDC_EDIT_SCC_G2_P6)
				SetDlgItemText(hwnd, IDC_EDIT_SCC_WAVE2_DATA_NAME, scc_editor_load_wave_name(1));	
			else if(focus_clid == IDC_EDIT_SCC_G2_P8)
				SetDlgItemText(hwnd, IDC_EDIT_SCC_WAVE3_DATA_NAME, scc_editor_load_wave_name(2));	
			break;
		case IDC_EDIT_SCC_G3_P1:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;			
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			scc_editor_set_param(2, focus_clid - IDC_EDIT_SCC_G3_P1, tmp); // amp
			break;
		case IDC_EDIT_SCC_G4_P1:
		case IDC_EDIT_SCC_G4_P2:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;			
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			scc_editor_set_param(3, focus_clid - IDC_EDIT_SCC_G4_P1, tmp); // pitch
			break;
		case IDC_EDIT_SCC_G5_P1:
		case IDC_EDIT_SCC_G5_P2:
		case IDC_EDIT_SCC_G5_P3:
		case IDC_EDIT_SCC_G5_P4:
		case IDC_EDIT_SCC_G5_P5:
		case IDC_EDIT_SCC_G5_P6:
		case IDC_EDIT_SCC_G5_P7:
		case IDC_EDIT_SCC_G5_P8:
		case IDC_EDIT_SCC_G5_P9:
		case IDC_EDIT_SCC_G5_P10:
		case IDC_EDIT_SCC_G5_P11:
		case IDC_EDIT_SCC_G5_P12:
		case IDC_EDIT_SCC_G5_P13:
		case IDC_EDIT_SCC_G5_P14:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;		
			if(tmp < 0) tmp = 0;			
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			scc_editor_set_param(4, focus_clid - IDC_EDIT_SCC_G5_P1, tmp); // ampenv
			break;
		case IDC_EDIT_SCC_G6_P1:
		case IDC_EDIT_SCC_G6_P2:
		case IDC_EDIT_SCC_G6_P3:
		case IDC_EDIT_SCC_G6_P4:
		case IDC_EDIT_SCC_G6_P5:
		case IDC_EDIT_SCC_G6_P6:
		case IDC_EDIT_SCC_G6_P7:
		case IDC_EDIT_SCC_G6_P8:
		case IDC_EDIT_SCC_G6_P9:
		case IDC_EDIT_SCC_G6_P10:
		case IDC_EDIT_SCC_G6_P11:
		case IDC_EDIT_SCC_G6_P12:
		case IDC_EDIT_SCC_G6_P13:
		case IDC_EDIT_SCC_G6_P14:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;			
			if(tmp < 0) tmp = 0;		
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			scc_editor_set_param(5, focus_clid - IDC_EDIT_SCC_G6_P1, tmp); // pitenv
			break;
		case IDC_EDIT_SCC_G7_P1:
		case IDC_EDIT_SCC_G7_P2:
		case IDC_EDIT_SCC_G7_P3:
		case IDC_EDIT_SCC_G7_P4:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;			
			if(tmp < 0) tmp = 0;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			scc_editor_set_param(6, focus_clid - IDC_EDIT_SCC_G7_P1, tmp); // lfo1
			if(focus_clid == IDC_EDIT_SCC_G7_P3)
				SetDlgItemText(hwnd, IDC_EDIT_SCC_LFO1_WAVE_NAME, scc_editor_load_wave_name(8));
			break;
		case IDC_EDIT_SCC_G8_P1:
		case IDC_EDIT_SCC_G8_P2:
		case IDC_EDIT_SCC_G8_P3:
		case IDC_EDIT_SCC_G8_P4:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;			
			if(tmp < 0) tmp = 0;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			scc_editor_set_param(7, focus_clid - IDC_EDIT_SCC_G8_P1, tmp); // lfo2
			if(focus_clid == IDC_EDIT_SCC_G8_P3)
				SetDlgItemText(hwnd, IDC_EDIT_SCC_LFO2_WAVE_NAME, scc_editor_load_wave_name(9));
			break;
		case IDC_EDIT_SCC_G9_P1:
		case IDC_EDIT_SCC_G9_P2:
		case IDC_EDIT_SCC_G9_P3:
		case IDC_EDIT_SCC_G9_P4:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;			
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			scc_editor_set_param(8, focus_clid - IDC_EDIT_SCC_G9_P1, tmp); // loop
			break;
		}
		break;	
	case WM_COMMAND:
		clid = LOWORD(wParam);
		focus_clid = clid;
		focus_wnd = 2;
		switch (clid) {
		case IDC_CHK_SCC_OVERRIDE:			
			DLG_CHECKBUTTON_TO_FLAG(hwnd, IDC_CHK_SCC_OVERRIDE, scc_editor_override);
			break;
		case IDC_BUTTON_SCC_DELETE_PRESET:
			scc_editor_delete_preset(scc_preset_num);
			break;
		case IDC_BUTTON_SCC_LOAD_PRESET:
			SetDlgItemText(hwnd,IDC_EDIT_SCC_NAME, scc_editor_load_name(scc_preset_num));
			scc_editor_load_preset(scc_preset_num);
			SendMessage(hwnd, WM_ISE_RESTORE, (WPARAM)0, (LPARAM)0 );
			break;
		case IDC_BUTTON_SCC_SAVE_PRESET:	
			{
				char buff[256];
				GetDlgItemText(hwnd, IDC_EDIT_SCC_NAME, buff, (WPARAM)sizeof(buff));
				scc_editor_store_name(scc_preset_num, (const char *)buff);
				scc_editor_store_preset(scc_preset_num);
			}		
			break;
		case IDC_BUTTON_SCC_LOAD_TEMP:					
			scc_editor_load_preset(-1);
			SendMessage(hwnd, WM_ISE_RESTORE, (WPARAM)0, (LPARAM)0 );
			break;
		case IDC_BUTTON_SCC_SAVE_TEMP:			
			scc_editor_store_preset(-1);
			break;
		case IDC_EDIT_SCC_NUM:								
			tmp = (int)GetDlgItemInt(hwnd, IDC_EDIT_SCC_NUM, NULL, TRUE);
			if(tmp < 0) 
				tmp = 0;
			else if(tmp > (SCC_SETTING_MAX - 1))
				tmp = SCC_SETTING_MAX - 1;
			scc_preset_num = tmp;				
			SendDlgItemMessage(hwnd, IDC_SLIDER_SCC_NUM, TBM_SETPOS, (WPARAM) 1, (LPARAM) scc_preset_num);
			break;
		case IDC_BUTTON_SCC_CLEAR:
			scc_editor_set_default_param(-1);
			SendMessage(hwnd, WM_ISE_RESTORE, (WPARAM)0, (LPARAM)0 );		
			break;
		case IDC_EDIT_SCC_G1_P1:
			scc_editor_set_param(0, 0, (int)GetDlgItemInt(hwnd, IDC_EDIT_SCC_G1_P1, NULL, TRUE)); // param
			break;
		case IDC_EDIT_SCC_G2_P1:
		case IDC_EDIT_SCC_G2_P2:
		case IDC_EDIT_SCC_G2_P3:
		case IDC_EDIT_SCC_G2_P4:
		case IDC_EDIT_SCC_G2_P5:
		case IDC_EDIT_SCC_G2_P6:
		case IDC_EDIT_SCC_G2_P7:
		case IDC_EDIT_SCC_G2_P8:
			tmp = (int)GetDlgItemInt(hwnd, clid, NULL, TRUE);
			scc_editor_set_param(1, clid - IDC_EDIT_SCC_G2_P1, tmp); // osc
			if(clid == IDC_EDIT_SCC_G2_P4)
				SetDlgItemText(hwnd, IDC_EDIT_SCC_WAVE1_DATA_NAME, scc_editor_load_wave_name(0));
			else if(clid == IDC_EDIT_SCC_G2_P6)
				SetDlgItemText(hwnd, IDC_EDIT_SCC_WAVE2_DATA_NAME, scc_editor_load_wave_name(1));
			else if(clid == IDC_EDIT_SCC_G2_P8)
				SetDlgItemText(hwnd, IDC_EDIT_SCC_WAVE3_DATA_NAME, scc_editor_load_wave_name(2));
			break;
		case IDC_EDIT_SCC_G3_P1:
			scc_editor_set_param(2, clid - IDC_EDIT_SCC_G3_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // amp
			break;
		case IDC_EDIT_SCC_G4_P1:
		case IDC_EDIT_SCC_G4_P2:
			scc_editor_set_param(3, clid - IDC_EDIT_SCC_G4_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // pitch
			break;
		case IDC_EDIT_SCC_G5_P1:
		case IDC_EDIT_SCC_G5_P2:
		case IDC_EDIT_SCC_G5_P3:
		case IDC_EDIT_SCC_G5_P4:
		case IDC_EDIT_SCC_G5_P5:
		case IDC_EDIT_SCC_G5_P6:
		case IDC_EDIT_SCC_G5_P7:
		case IDC_EDIT_SCC_G5_P8:
		case IDC_EDIT_SCC_G5_P9:
		case IDC_EDIT_SCC_G5_P10:
		case IDC_EDIT_SCC_G5_P11:
		case IDC_EDIT_SCC_G5_P12:
		case IDC_EDIT_SCC_G5_P13:
		case IDC_EDIT_SCC_G5_P14:
			scc_editor_set_param(4, clid - IDC_EDIT_SCC_G5_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // ampenv
			break;
		case IDC_EDIT_SCC_G6_P1:
		case IDC_EDIT_SCC_G6_P2:
		case IDC_EDIT_SCC_G6_P3:
		case IDC_EDIT_SCC_G6_P4:
		case IDC_EDIT_SCC_G6_P5:
		case IDC_EDIT_SCC_G6_P6:
		case IDC_EDIT_SCC_G6_P7:
		case IDC_EDIT_SCC_G6_P8:
		case IDC_EDIT_SCC_G6_P9:
		case IDC_EDIT_SCC_G6_P10:
		case IDC_EDIT_SCC_G6_P11:
		case IDC_EDIT_SCC_G6_P12:
		case IDC_EDIT_SCC_G6_P13:
		case IDC_EDIT_SCC_G6_P14:
			scc_editor_set_param(5, clid - IDC_EDIT_SCC_G6_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // pitenv
			break;
		case IDC_EDIT_SCC_G7_P1:
		case IDC_EDIT_SCC_G7_P2:
		case IDC_EDIT_SCC_G7_P3:
		case IDC_EDIT_SCC_G7_P4:
			scc_editor_set_param(6, clid - IDC_EDIT_SCC_G7_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // lfo1
			if(clid == IDC_EDIT_SCC_G7_P3)
				SetDlgItemText(hwnd, IDC_EDIT_SCC_LFO1_WAVE_NAME, scc_editor_load_wave_name(8));
			break;
		case IDC_EDIT_SCC_G8_P1:
		case IDC_EDIT_SCC_G8_P2:
		case IDC_EDIT_SCC_G8_P3:
		case IDC_EDIT_SCC_G8_P4:
			scc_editor_set_param(7, clid - IDC_EDIT_SCC_G8_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // lfo2
			if(clid == IDC_EDIT_SCC_G8_P3)
				SetDlgItemText(hwnd, IDC_EDIT_SCC_LFO2_WAVE_NAME, scc_editor_load_wave_name(9));
			break;
		case IDC_EDIT_SCC_G9_P1:
		case IDC_EDIT_SCC_G9_P2:
		case IDC_EDIT_SCC_G9_P3:
		case IDC_EDIT_SCC_G9_P4:
			scc_editor_set_param(8, clid - IDC_EDIT_SCC_G9_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // loop
			break;
		case IDC_EDIT_SCC_WAVE1_DATA_NAME:
			focus_clid = IDC_EDIT_SCC_G2_P4;
			break;
		case IDC_EDIT_SCC_WAVE2_DATA_NAME:
			focus_clid = IDC_EDIT_SCC_G2_P6;
			break;
		case IDC_EDIT_SCC_WAVE3_DATA_NAME:
			focus_clid = IDC_EDIT_SCC_G2_P8;
			break;
		case IDC_EDIT_SCC_LFO1_WAVE_NAME:
			focus_clid = IDC_EDIT_SCC_G7_P3;
			break;
		case IDC_EDIT_SCC_LFO2_WAVE_NAME:
			focus_clid = IDC_EDIT_SCC_G8_P3;
			break;
		}
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

static LRESULT APIENTRY CALLBACK ISEditorMMSProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	int i, tmp;
	WORD clid = 0;
	static WORD focus_clid = 0, prv_focus_clid = 0;
	int16 wheel_speed = 0;

	switch (uMess) {
	case WM_INITDIALOG:
		SendDlgItemMessage(hwnd, IDC_SLIDER_MMS_NUM, TBM_SETRANGEMAX, (WPARAM) 0, (LPARAM) MMS_SETTING_MAX - 1);
		SendDlgItemMessage(hwnd, IDC_SLIDER_MMS_NUM, TBM_SETRANGEMIN, (WPARAM) 0, (LPARAM) 0);
		SetDlgItemInt(hwnd, IDC_EDIT_MMS_NUM, mms_preset_num, TRUE);
		SetDlgItemText(hwnd,IDC_EDIT_MMS_NAME, mms_editor_load_name(mms_preset_num));	
	case WM_ISE_RESTORE:
		DLG_FLAG_TO_CHECKBUTTON(hwnd, IDC_CHK_MMS_OVERRIDE, mms_editor_override);		
		SendDlgItemMessage(hwnd, IDC_SLIDER_MMS_NUM, TBM_SETPOS, (WPARAM) 1, (LPARAM) mms_preset_num);
		SetDlgItemInt(hwnd, IDC_EDIT_MMS_OP_NUM, mms_editor_get_param(0, 0, 0), TRUE); // op max
		for(i = 0; i < MMS_OP_MAX; i++)
			DLG_FLAG_TO_CHECKBUTTON(hwnd, IDC_BUTTON_MMS_OP0 + i, (i == mms_op_num) ? 1 : 0);			
		for(i = 0; i < MMS_OP_RANGE_MAX; i++)
			SetDlgItemInt(hwnd, IDC_EDIT_MMS_G1_P1 + i, mms_editor_get_param(1, mms_op_num, i), TRUE); // range		
		SetDlgItemInt(hwnd, IDC_EDIT_MMS_G2_P1, mms_editor_get_param(2, mms_op_num, 0), TRUE); // param
		SetDlgItemInt(hwnd, IDC_EDIT_MMS_G2_P2, mms_editor_get_param(2, mms_op_num, 1), TRUE); // param
		SetDlgItemInt(hwnd, IDC_EDIT_MMS_G2_P3, mms_editor_get_param(2, mms_op_num, 2), TRUE); // param
		for(i = 0; i < MMS_OP_CON_MAX; i++)
			SetDlgItemInt(hwnd, IDC_EDIT_MMS_G3_P1 + i, mms_editor_get_param(3, mms_op_num, i), TRUE); // con	
		SetDlgItemInt(hwnd, IDC_EDIT_MMS_G4_P1, mms_editor_get_param(4, mms_op_num, 0), TRUE); // osc
		SetDlgItemInt(hwnd, IDC_EDIT_MMS_G4_P2, mms_editor_get_param(4, mms_op_num, 1), TRUE); // osc
		SetDlgItemInt(hwnd, IDC_EDIT_MMS_G4_P3, mms_editor_get_param(4, mms_op_num, 2), TRUE); // osc	
		for(i = 0; i < MMS_OP_WAVE_MAX; i++)
			SetDlgItemInt(hwnd, IDC_EDIT_MMS_G5_P1 + i, mms_editor_get_param(5, mms_op_num, i), TRUE); // wave	
		SetDlgItemText(hwnd, IDC_EDIT_MMS_WAVE_DATA_NAME, mms_editor_load_wave_name(mms_op_num, -1));
		for(i = 0; i < MMS_OP_SUB_MAX; i++)
			SetDlgItemInt(hwnd, IDC_EDIT_MMS_G6_P1 + i, mms_editor_get_param(6, mms_op_num, i), TRUE); // sub
		SetDlgItemInt(hwnd, IDC_EDIT_MMS_G7_P1, mms_editor_get_param(7, mms_op_num, 0), TRUE); // amp
		SetDlgItemInt(hwnd, IDC_EDIT_MMS_G8_P1, mms_editor_get_param(8, mms_op_num, 0), TRUE); // pit
		SetDlgItemInt(hwnd, IDC_EDIT_MMS_G8_P2, mms_editor_get_param(8, mms_op_num, 1), TRUE); // pit
		SetDlgItemInt(hwnd, IDC_EDIT_MMS_G8_P3, mms_editor_get_param(8, mms_op_num, 2), TRUE); // pit
		SetDlgItemInt(hwnd, IDC_EDIT_MMS_G9_P1, mms_editor_get_param(9, mms_op_num, 0), TRUE); // wid
		SetDlgItemInt(hwnd, IDC_EDIT_MMS_G9_P2, mms_editor_get_param(9, mms_op_num, 1), TRUE); // wid
		SetDlgItemInt(hwnd, IDC_EDIT_MMS_G9_P3, mms_editor_get_param(9, mms_op_num, 2), TRUE); // wid
		SetDlgItemInt(hwnd, IDC_EDIT_MMS_G10_P1, mms_editor_get_param(10, mms_op_num, 0), TRUE); // flt
		SetDlgItemText(hwnd, IDC_EDIT_MMS_FILTER_NAME, mms_editor_load_filter_name(mms_op_num)); // flt type
		SetDlgItemInt(hwnd, IDC_EDIT_MMS_G10_P2, mms_editor_get_param(10, mms_op_num, 1), TRUE); // flt
		SetDlgItemInt(hwnd, IDC_EDIT_MMS_G10_P3, mms_editor_get_param(10, mms_op_num, 2), TRUE); // flt
		for(i = 0; i < MMS_OP_CUT_PARAM; i++)
			SetDlgItemInt(hwnd, IDC_EDIT_MMS_G11_P1 + i, mms_editor_get_param(11, mms_op_num, i), TRUE); // cutoff
		for(i = 0; i < MMS_OP_ENV_PARAM; i++){
			SetDlgItemInt(hwnd, IDC_EDIT_MMS_G12_P1 + i, mms_editor_get_param(12, mms_op_num, i), TRUE); // ampenv
			SetDlgItemInt(hwnd, IDC_EDIT_MMS_G13_P1 + i, mms_editor_get_param(13, mms_op_num, i), TRUE); // pitenv
			SetDlgItemInt(hwnd, IDC_EDIT_MMS_G14_P1 + i, mms_editor_get_param(14, mms_op_num, i), TRUE); // widenv
			SetDlgItemInt(hwnd, IDC_EDIT_MMS_G15_P1 + i, mms_editor_get_param(15, mms_op_num, i), TRUE); // modenv
			if(i >= 12) continue;
			SetDlgItemInt(hwnd, IDC_EDIT_MMS_G16_P1 + i, mms_editor_get_param(16, mms_op_num, i), TRUE); // ampenv_keyf
			SetDlgItemInt(hwnd, IDC_EDIT_MMS_G17_P1 + i, mms_editor_get_param(17, mms_op_num, i), TRUE); // pitenv_keyf
			SetDlgItemInt(hwnd, IDC_EDIT_MMS_G18_P1 + i, mms_editor_get_param(18, mms_op_num, i), TRUE); // widenv_keyf
			SetDlgItemInt(hwnd, IDC_EDIT_MMS_G19_P1 + i, mms_editor_get_param(19, mms_op_num, i), TRUE); // modenv_keyf
			SetDlgItemInt(hwnd, IDC_EDIT_MMS_G20_P1 + i, mms_editor_get_param(20, mms_op_num, i), TRUE); // ampenv_velf
			SetDlgItemInt(hwnd, IDC_EDIT_MMS_G21_P1 + i, mms_editor_get_param(21, mms_op_num, i), TRUE); // pitenv_velf
			SetDlgItemInt(hwnd, IDC_EDIT_MMS_G22_P1 + i, mms_editor_get_param(22, mms_op_num, i), TRUE); // widenv_velf
			SetDlgItemInt(hwnd, IDC_EDIT_MMS_G23_P1 + i, mms_editor_get_param(23, mms_op_num, i), TRUE); // modenv_velf
		}
		for(i = 0; i < MMS_OP_LFO_PARAM; i++){
			SetDlgItemInt(hwnd, IDC_EDIT_MMS_G24_P1 + i, mms_editor_get_param(24, mms_op_num, i), TRUE); // lfo1
			SetDlgItemInt(hwnd, IDC_EDIT_MMS_G25_P1 + i, mms_editor_get_param(25, mms_op_num, i), TRUE); // lfo2
			SetDlgItemInt(hwnd, IDC_EDIT_MMS_G26_P1 + i, mms_editor_get_param(26, mms_op_num, i), TRUE); // lfo3
			SetDlgItemInt(hwnd, IDC_EDIT_MMS_G27_P1 + i, mms_editor_get_param(27, mms_op_num, i), TRUE); // lfo4
		}		
		for(i = 0; i < MMS_OP_LOOP_MAX; i++)
			SetDlgItemInt(hwnd, IDC_EDIT_MMS_G28_P1 + i, mms_editor_get_param(28, mms_op_num, i), TRUE); // loop
		SetDlgItemText(hwnd, IDC_EDIT_MMS_LFO1_WAVE_NAME, mms_editor_load_wave_name(mms_op_num, 0));
		SetDlgItemText(hwnd, IDC_EDIT_MMS_LFO2_WAVE_NAME, mms_editor_load_wave_name(mms_op_num, 1));
		SetDlgItemText(hwnd, IDC_EDIT_MMS_LFO3_WAVE_NAME, mms_editor_load_wave_name(mms_op_num, 2));
		SetDlgItemText(hwnd, IDC_EDIT_MMS_LFO4_WAVE_NAME, mms_editor_load_wave_name(mms_op_num, 3));
		break;
	case WM_HSCROLL:
	case WM_VSCROLL:
		tmp = SendDlgItemMessage(hwnd, IDC_SLIDER_MMS_NUM, TBM_GETPOS, (WPARAM) 0, (LPARAM)0);
		if(tmp != mms_preset_num){
			mms_preset_num = tmp;
			SetDlgItemInt(hwnd, IDC_EDIT_MMS_NUM, tmp, TRUE);	
		}
		break;
	case WM_MOUSEWHEEL:	
		if(focus_wnd != 3)
			break;	
		wheel_speed = (int16)GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA; // upper 16bit sined int // 1knoch = 120	
		wheel_speed *= (LOWORD(wParam) & MK_SHIFT) ? 10 : 1;
		switch (focus_clid) {
		case IDC_EDIT_MMS_NUM:		
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;	
			if(tmp < 0) 
				tmp = 0;
			else if(tmp > (MMS_SETTING_MAX - 1))
				tmp = MMS_SETTING_MAX - 1;
			mms_preset_num = tmp;	
			SetDlgItemInt(hwnd, IDC_EDIT_MMS_NUM, mms_preset_num, TRUE);					
			SendDlgItemMessage(hwnd, IDC_SLIDER_MMS_NUM, TBM_SETPOS, (WPARAM) 1, (LPARAM) mms_preset_num);
			break;
		case IDC_EDIT_MMS_OP_NUM:	
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			if(tmp < 0) 
				tmp = 0;
			else if(tmp > MMS_OP_MAX)
				tmp = MMS_OP_MAX;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(0, 0, 0, tmp); // op max
			break;
		case IDC_EDIT_MMS_G1_P1:
		case IDC_EDIT_MMS_G1_P2:
		case IDC_EDIT_MMS_G1_P3:
		case IDC_EDIT_MMS_G1_P4:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			if(tmp < 0) tmp = 0;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(1, mms_op_num, focus_clid - IDC_EDIT_MMS_G1_P1, tmp); // range
			break;			
		case IDC_EDIT_MMS_G2_P1:
		case IDC_EDIT_MMS_G2_P2:
		case IDC_EDIT_MMS_G2_P3:
		case IDC_EDIT_MMS_G2_P4:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			if(tmp < 0) tmp = 0;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(2, mms_op_num, focus_clid - IDC_EDIT_MMS_G2_P1, tmp); // param
			break;			
		case IDC_EDIT_MMS_G3_P1:
		case IDC_EDIT_MMS_G3_P2:
		case IDC_EDIT_MMS_G3_P3:
		case IDC_EDIT_MMS_G3_P4:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(3, mms_op_num, focus_clid - IDC_EDIT_MMS_G3_P1, tmp); // connect
			break;			
		case IDC_EDIT_MMS_G4_P1:
		case IDC_EDIT_MMS_G4_P2:
		case IDC_EDIT_MMS_G4_P3:
		case IDC_EDIT_MMS_G4_P4:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(4, mms_op_num, focus_clid - IDC_EDIT_MMS_G4_P1, tmp); // osc			
			break;			
		case IDC_EDIT_MMS_G5_P1:
		case IDC_EDIT_MMS_G5_P2:
		case IDC_EDIT_MMS_G5_P3:
		case IDC_EDIT_MMS_G5_P4:
		case IDC_EDIT_MMS_G5_P5:
		case IDC_EDIT_MMS_G5_P6:
		case IDC_EDIT_MMS_G5_P7:
		case IDC_EDIT_MMS_G5_P8:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			if(tmp < 0) tmp = 0;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(5, mms_op_num, focus_clid - IDC_EDIT_MMS_G5_P1, tmp); // wave
			if(focus_clid == IDC_EDIT_MMS_G5_P1 || focus_clid == IDC_EDIT_MMS_G5_P2)
				SetDlgItemText(hwnd, IDC_EDIT_MMS_WAVE_DATA_NAME, mms_editor_load_wave_name(mms_op_num, -1));
			break;		
		case IDC_EDIT_MMS_G6_P1:
		case IDC_EDIT_MMS_G6_P2:
		case IDC_EDIT_MMS_G6_P3:
		case IDC_EDIT_MMS_G6_P4:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(6, mms_op_num, focus_clid - IDC_EDIT_MMS_G6_P1, tmp); // sub
			break;	
		case IDC_EDIT_MMS_G7_P1:
		case IDC_EDIT_MMS_G7_P2:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(7, mms_op_num, focus_clid - IDC_EDIT_MMS_G7_P1, tmp); // amp
			break;	
		case IDC_EDIT_MMS_G8_P1:
		case IDC_EDIT_MMS_G8_P2:
		case IDC_EDIT_MMS_G8_P3:
		case IDC_EDIT_MMS_G8_P4:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(8, mms_op_num, focus_clid - IDC_EDIT_MMS_G8_P1, tmp); // pitch
			break;	
		case IDC_EDIT_MMS_G9_P1:
		case IDC_EDIT_MMS_G9_P2:
		case IDC_EDIT_MMS_G9_P3:
		case IDC_EDIT_MMS_G9_P4:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(9, mms_op_num, focus_clid - IDC_EDIT_MMS_G9_P1, tmp); // width
			break;	
		case IDC_EDIT_MMS_G10_P1:
		case IDC_EDIT_MMS_G10_P2:
		case IDC_EDIT_MMS_G10_P3:
		case IDC_EDIT_MMS_G10_P4:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			if(tmp < 0) tmp = 0;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(10, mms_op_num, focus_clid - IDC_EDIT_MMS_G10_P1, tmp); // filter
			if(focus_clid == IDC_EDIT_MMS_G10_P1)
				SetDlgItemText(hwnd, IDC_EDIT_MMS_FILTER_NAME, mms_editor_load_filter_name(mms_op_num));
			break;	
		case IDC_EDIT_MMS_G11_P1:
		case IDC_EDIT_MMS_G11_P2:
		case IDC_EDIT_MMS_G11_P3:
		case IDC_EDIT_MMS_G11_P4:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(11, mms_op_num, focus_clid - IDC_EDIT_MMS_G11_P1, tmp); // cutoff
			break;				
		case IDC_EDIT_MMS_G12_P1:
		case IDC_EDIT_MMS_G12_P2:
		case IDC_EDIT_MMS_G12_P3:
		case IDC_EDIT_MMS_G12_P4:
		case IDC_EDIT_MMS_G12_P5:
		case IDC_EDIT_MMS_G12_P6:
		case IDC_EDIT_MMS_G12_P7:
		case IDC_EDIT_MMS_G12_P8:
		case IDC_EDIT_MMS_G12_P9:
		case IDC_EDIT_MMS_G12_P10:
		case IDC_EDIT_MMS_G12_P11:
		case IDC_EDIT_MMS_G12_P12:
		case IDC_EDIT_MMS_G12_P13:
		case IDC_EDIT_MMS_G12_P14:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			if(tmp < 0) tmp = 0;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(12, mms_op_num, focus_clid - IDC_EDIT_MMS_G12_P1, tmp); // ampenv
			break;
		case IDC_EDIT_MMS_G13_P1:
		case IDC_EDIT_MMS_G13_P2:
		case IDC_EDIT_MMS_G13_P3:
		case IDC_EDIT_MMS_G13_P4:
		case IDC_EDIT_MMS_G13_P5:
		case IDC_EDIT_MMS_G13_P6:
		case IDC_EDIT_MMS_G13_P7:
		case IDC_EDIT_MMS_G13_P8:
		case IDC_EDIT_MMS_G13_P9:
		case IDC_EDIT_MMS_G13_P10:
		case IDC_EDIT_MMS_G13_P11:
		case IDC_EDIT_MMS_G13_P12:
		case IDC_EDIT_MMS_G13_P13:
		case IDC_EDIT_MMS_G13_P14:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			if(tmp < 0) tmp = 0;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(13, mms_op_num, focus_clid - IDC_EDIT_MMS_G13_P1, tmp); // pitenv
			break;
		case IDC_EDIT_MMS_G14_P1:
		case IDC_EDIT_MMS_G14_P2:
		case IDC_EDIT_MMS_G14_P3:
		case IDC_EDIT_MMS_G14_P4:
		case IDC_EDIT_MMS_G14_P5:
		case IDC_EDIT_MMS_G14_P6:
		case IDC_EDIT_MMS_G14_P7:
		case IDC_EDIT_MMS_G14_P8:
		case IDC_EDIT_MMS_G14_P9:
		case IDC_EDIT_MMS_G14_P10:
		case IDC_EDIT_MMS_G14_P11:
		case IDC_EDIT_MMS_G14_P12:
		case IDC_EDIT_MMS_G14_P13:
		case IDC_EDIT_MMS_G14_P14:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			if(tmp < 0) tmp = 0;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(14, mms_op_num, focus_clid - IDC_EDIT_MMS_G14_P1, tmp); // widenv
			break;
		case IDC_EDIT_MMS_G15_P1:
		case IDC_EDIT_MMS_G15_P2:
		case IDC_EDIT_MMS_G15_P3:
		case IDC_EDIT_MMS_G15_P4:
		case IDC_EDIT_MMS_G15_P5:
		case IDC_EDIT_MMS_G15_P6:
		case IDC_EDIT_MMS_G15_P7:
		case IDC_EDIT_MMS_G15_P8:
		case IDC_EDIT_MMS_G15_P9:
		case IDC_EDIT_MMS_G15_P10:
		case IDC_EDIT_MMS_G15_P11:
		case IDC_EDIT_MMS_G15_P12:
		case IDC_EDIT_MMS_G15_P13:
		case IDC_EDIT_MMS_G15_P14:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			if(tmp < 0) tmp = 0;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(15, mms_op_num, focus_clid - IDC_EDIT_MMS_G15_P1, tmp); // modenv
			break;			
		case IDC_EDIT_MMS_G16_P1:
		case IDC_EDIT_MMS_G16_P2:
		case IDC_EDIT_MMS_G16_P3:
		case IDC_EDIT_MMS_G16_P4:
		case IDC_EDIT_MMS_G16_P5:
		case IDC_EDIT_MMS_G16_P6:
		case IDC_EDIT_MMS_G16_P7:
		case IDC_EDIT_MMS_G16_P8:
		case IDC_EDIT_MMS_G16_P9:
		case IDC_EDIT_MMS_G16_P10:
		case IDC_EDIT_MMS_G16_P11:
		case IDC_EDIT_MMS_G16_P12:
		case IDC_EDIT_MMS_G16_P13:
		case IDC_EDIT_MMS_G16_P14:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(16, mms_op_num, focus_clid - IDC_EDIT_MMS_G16_P1, tmp); // ampenv_keyf
			break;
		case IDC_EDIT_MMS_G17_P1:
		case IDC_EDIT_MMS_G17_P2:
		case IDC_EDIT_MMS_G17_P3:
		case IDC_EDIT_MMS_G17_P4:
		case IDC_EDIT_MMS_G17_P5:
		case IDC_EDIT_MMS_G17_P6:
		case IDC_EDIT_MMS_G17_P7:
		case IDC_EDIT_MMS_G17_P8:
		case IDC_EDIT_MMS_G17_P9:
		case IDC_EDIT_MMS_G17_P10:
		case IDC_EDIT_MMS_G17_P11:
		case IDC_EDIT_MMS_G17_P12:
		case IDC_EDIT_MMS_G17_P13:
		case IDC_EDIT_MMS_G17_P14:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(17, mms_op_num, focus_clid - IDC_EDIT_MMS_G17_P1, tmp); // pitenv_keyf
			break;
		case IDC_EDIT_MMS_G18_P1:
		case IDC_EDIT_MMS_G18_P2:
		case IDC_EDIT_MMS_G18_P3:
		case IDC_EDIT_MMS_G18_P4:
		case IDC_EDIT_MMS_G18_P5:
		case IDC_EDIT_MMS_G18_P6:
		case IDC_EDIT_MMS_G18_P7:
		case IDC_EDIT_MMS_G18_P8:
		case IDC_EDIT_MMS_G18_P9:
		case IDC_EDIT_MMS_G18_P10:
		case IDC_EDIT_MMS_G18_P11:
		case IDC_EDIT_MMS_G18_P12:
		case IDC_EDIT_MMS_G18_P13:
		case IDC_EDIT_MMS_G18_P14:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(18, mms_op_num, focus_clid - IDC_EDIT_MMS_G18_P1, tmp); // widenv_keyf
			break;
		case IDC_EDIT_MMS_G19_P1:
		case IDC_EDIT_MMS_G19_P2:
		case IDC_EDIT_MMS_G19_P3:
		case IDC_EDIT_MMS_G19_P4:
		case IDC_EDIT_MMS_G19_P5:
		case IDC_EDIT_MMS_G19_P6:
		case IDC_EDIT_MMS_G19_P7:
		case IDC_EDIT_MMS_G19_P8:
		case IDC_EDIT_MMS_G19_P9:
		case IDC_EDIT_MMS_G19_P10:
		case IDC_EDIT_MMS_G19_P11:
		case IDC_EDIT_MMS_G19_P12:
		case IDC_EDIT_MMS_G19_P13:
		case IDC_EDIT_MMS_G19_P14:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(19, mms_op_num, focus_clid - IDC_EDIT_MMS_G19_P1, tmp); // modenv_keyf
			break;			
		case IDC_EDIT_MMS_G20_P1:
		case IDC_EDIT_MMS_G20_P2:
		case IDC_EDIT_MMS_G20_P3:
		case IDC_EDIT_MMS_G20_P4:
		case IDC_EDIT_MMS_G20_P5:
		case IDC_EDIT_MMS_G20_P6:
		case IDC_EDIT_MMS_G20_P7:
		case IDC_EDIT_MMS_G20_P8:
		case IDC_EDIT_MMS_G20_P9:
		case IDC_EDIT_MMS_G20_P10:
		case IDC_EDIT_MMS_G20_P11:
		case IDC_EDIT_MMS_G20_P12:
		case IDC_EDIT_MMS_G20_P13:
		case IDC_EDIT_MMS_G20_P14:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(20, mms_op_num, focus_clid - IDC_EDIT_MMS_G20_P1, tmp); // ampenv_velf
			break;	
		case IDC_EDIT_MMS_G21_P1:
		case IDC_EDIT_MMS_G21_P2:
		case IDC_EDIT_MMS_G21_P3:
		case IDC_EDIT_MMS_G21_P4:
		case IDC_EDIT_MMS_G21_P5:
		case IDC_EDIT_MMS_G21_P6:
		case IDC_EDIT_MMS_G21_P7:
		case IDC_EDIT_MMS_G21_P8:
		case IDC_EDIT_MMS_G21_P9:
		case IDC_EDIT_MMS_G21_P10:
		case IDC_EDIT_MMS_G21_P11:
		case IDC_EDIT_MMS_G21_P12:
		case IDC_EDIT_MMS_G21_P13:
		case IDC_EDIT_MMS_G21_P14:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(21, mms_op_num, focus_clid - IDC_EDIT_MMS_G21_P1, tmp); // pitenv_velf
			break;	
		case IDC_EDIT_MMS_G22_P1:
		case IDC_EDIT_MMS_G22_P2:
		case IDC_EDIT_MMS_G22_P3:
		case IDC_EDIT_MMS_G22_P4:
		case IDC_EDIT_MMS_G22_P5:
		case IDC_EDIT_MMS_G22_P6:
		case IDC_EDIT_MMS_G22_P7:
		case IDC_EDIT_MMS_G22_P8:
		case IDC_EDIT_MMS_G22_P9:
		case IDC_EDIT_MMS_G22_P10:
		case IDC_EDIT_MMS_G22_P11:
		case IDC_EDIT_MMS_G22_P12:
		case IDC_EDIT_MMS_G22_P13:
		case IDC_EDIT_MMS_G22_P14:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(22, mms_op_num, focus_clid - IDC_EDIT_MMS_G22_P1, tmp); // widenv_velf
			break;	
		case IDC_EDIT_MMS_G23_P1:
		case IDC_EDIT_MMS_G23_P2:
		case IDC_EDIT_MMS_G23_P3:
		case IDC_EDIT_MMS_G23_P4:
		case IDC_EDIT_MMS_G23_P5:
		case IDC_EDIT_MMS_G23_P6:
		case IDC_EDIT_MMS_G23_P7:
		case IDC_EDIT_MMS_G23_P8:
		case IDC_EDIT_MMS_G23_P9:
		case IDC_EDIT_MMS_G23_P10:
		case IDC_EDIT_MMS_G23_P11:
		case IDC_EDIT_MMS_G23_P12:
		case IDC_EDIT_MMS_G23_P13:
		case IDC_EDIT_MMS_G23_P14:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(23, mms_op_num, focus_clid - IDC_EDIT_MMS_G23_P1, tmp); // modenv_velf
			break;			
		case IDC_EDIT_MMS_G24_P1:
		case IDC_EDIT_MMS_G24_P2:
		case IDC_EDIT_MMS_G24_P3:
		case IDC_EDIT_MMS_G24_P4:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			if(tmp < 0) tmp = 0;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(24, mms_op_num, focus_clid - IDC_EDIT_MMS_G24_P1, tmp); // lfo1
			if(focus_clid == IDC_EDIT_MMS_G24_P3)
				SetDlgItemText(hwnd, IDC_EDIT_MMS_LFO1_WAVE_NAME, mms_editor_load_wave_name(mms_op_num, 0));
			break;
		case IDC_EDIT_MMS_G25_P1:
		case IDC_EDIT_MMS_G25_P2:
		case IDC_EDIT_MMS_G25_P3:
		case IDC_EDIT_MMS_G25_P4:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			if(tmp < 0) tmp = 0;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(25, mms_op_num, focus_clid - IDC_EDIT_MMS_G25_P1, tmp); // lfo2
			if(focus_clid == IDC_EDIT_MMS_G25_P3)
				SetDlgItemText(hwnd, IDC_EDIT_MMS_LFO2_WAVE_NAME, mms_editor_load_wave_name(mms_op_num, 1));
			break;
		case IDC_EDIT_MMS_G26_P1:
		case IDC_EDIT_MMS_G26_P2:
		case IDC_EDIT_MMS_G26_P3:
		case IDC_EDIT_MMS_G26_P4:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			if(tmp < 0) tmp = 0;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(26, mms_op_num, focus_clid - IDC_EDIT_MMS_G26_P1, tmp); // lfo3
			if(focus_clid == IDC_EDIT_MMS_G26_P3)
				SetDlgItemText(hwnd, IDC_EDIT_MMS_LFO3_WAVE_NAME, mms_editor_load_wave_name(mms_op_num, 2));
			break;
		case IDC_EDIT_MMS_G27_P1:
		case IDC_EDIT_MMS_G27_P2:
		case IDC_EDIT_MMS_G27_P3:
		case IDC_EDIT_MMS_G27_P4:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			if(tmp < 0) tmp = 0;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(27, mms_op_num, focus_clid - IDC_EDIT_MMS_G27_P1, tmp); // lfo4
			if(focus_clid == IDC_EDIT_MMS_G27_P3)
				SetDlgItemText(hwnd, IDC_EDIT_MMS_LFO4_WAVE_NAME, mms_editor_load_wave_name(mms_op_num, 3));
			break;		
		case IDC_EDIT_MMS_G28_P1:
		case IDC_EDIT_MMS_G28_P2:
		case IDC_EDIT_MMS_G28_P3:
		case IDC_EDIT_MMS_G28_P4:
			tmp = (int)GetDlgItemInt(hwnd, focus_clid, NULL, TRUE) + wheel_speed;
			if(tmp < 0) tmp = 0;
			SetDlgItemInt(hwnd, focus_clid, tmp, TRUE);
			mms_editor_set_param(28, mms_op_num, focus_clid - IDC_EDIT_MMS_G28_P1, tmp); // loop
			break;	
		}
		break;
	case WM_COMMAND:
		clid = LOWORD(wParam);
		prv_focus_clid = focus_clid;
		focus_clid = clid;
		focus_wnd = 3;
		switch (clid) {
		case IDC_CHK_MMS_OVERRIDE:			
			DLG_CHECKBUTTON_TO_FLAG(hwnd, IDC_CHK_MMS_OVERRIDE, mms_editor_override);
			break;
		case IDC_BUTTON_MMS_DELETE_PRESET:
			mms_editor_delete_preset(mms_preset_num);
			break;
		case IDC_BUTTON_MMS_LOAD_PRESET:
			SetDlgItemText(hwnd,IDC_EDIT_MMS_NAME, mms_editor_load_name(mms_preset_num));
			mms_editor_load_preset(mms_preset_num);
			SendMessage(hwnd, WM_ISE_RESTORE, (WPARAM)0, (LPARAM)0 );
			break;
		case IDC_BUTTON_MMS_SAVE_PRESET:	
			{
				char buff[256];
				GetDlgItemText(hwnd, IDC_EDIT_MMS_NAME, buff, (WPARAM)sizeof(buff));
				mms_editor_store_name(mms_preset_num, (const char *)buff);
				mms_editor_store_preset(mms_preset_num);
			}		
			break;
		case IDC_BUTTON_MMS_LOAD_TEMP:					
			mms_editor_load_preset(-1);
			SendMessage(hwnd, WM_ISE_RESTORE, (WPARAM)0, (LPARAM)0 );
			break;
		case IDC_BUTTON_MMS_SAVE_TEMP:			
			mms_editor_store_preset(-1);
			break;
		case IDC_BUTTON_MMS_MAGIC_PARAM:		
			mms_editor_set_magic_param();
			SendMessage(hwnd, WM_ISE_RESTORE, (WPARAM)0, (LPARAM)0 );
			break;
		case IDC_EDIT_MMS_NUM:		
			tmp = (int)GetDlgItemInt(hwnd, IDC_EDIT_MMS_NUM, NULL, TRUE);
			if(tmp < 0) 
				tmp = 0;
			else if(tmp > (MMS_SETTING_MAX - 1))
				tmp = MMS_SETTING_MAX - 1;
			mms_preset_num = tmp;				
			SendDlgItemMessage(hwnd, IDC_SLIDER_MMS_NUM, TBM_SETPOS, (WPARAM) 1, (LPARAM) mms_preset_num);
			break;
		case IDC_BUTTON_MMS_OP0:
		case IDC_BUTTON_MMS_OP1:
		case IDC_BUTTON_MMS_OP2:
		case IDC_BUTTON_MMS_OP3:
		case IDC_BUTTON_MMS_OP4:
		case IDC_BUTTON_MMS_OP5:
		case IDC_BUTTON_MMS_OP6:
		case IDC_BUTTON_MMS_OP7:
		case IDC_BUTTON_MMS_OP8:
		case IDC_BUTTON_MMS_OP9:
		case IDC_BUTTON_MMS_OP10:
		case IDC_BUTTON_MMS_OP11:
		case IDC_BUTTON_MMS_OP12:
		case IDC_BUTTON_MMS_OP13:
		case IDC_BUTTON_MMS_OP14:
		case IDC_BUTTON_MMS_OP15:
			tmp = clid - IDC_BUTTON_MMS_OP0;
			if(tmp != mms_op_num){
				mms_op_num = tmp; // op change
				SendMessage(hwnd, WM_ISE_RESTORE, (WPARAM)0, (LPARAM)0 );
			}			
			break;
		case IDC_EDIT_MMS_OP_NUM:	
			mms_editor_set_param(0, 0, 0, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // op max
			break;
		case IDC_BUTTON_MMS_OP_ALL_CLEAR:
			mms_editor_set_default_param(-1, -1);
			SendMessage(hwnd, WM_ISE_RESTORE, (WPARAM)0, (LPARAM)0 );		
			break;
		case IDC_BUTTON_MMS_OP_CLEAR:
			mms_editor_set_default_param(-1, mms_op_num);
			SendMessage(hwnd, WM_ISE_RESTORE, (WPARAM)0, (LPARAM)0 );		
			break;
		case IDC_EDIT_MMS_G1_P1:
		case IDC_EDIT_MMS_G1_P2:
		case IDC_EDIT_MMS_G1_P3:
		case IDC_EDIT_MMS_G1_P4:
			mms_editor_set_param(1, mms_op_num, clid - IDC_EDIT_MMS_G1_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // range
			break;			
		case IDC_EDIT_MMS_G2_P1:
		case IDC_EDIT_MMS_G2_P2:
		case IDC_EDIT_MMS_G2_P3:
		case IDC_EDIT_MMS_G2_P4:
			mms_editor_set_param(2, mms_op_num, clid - IDC_EDIT_MMS_G2_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // param
			break;			
		case IDC_EDIT_MMS_G3_P1:
		case IDC_EDIT_MMS_G3_P2:
		case IDC_EDIT_MMS_G3_P3:
		case IDC_EDIT_MMS_G3_P4:
			mms_editor_set_param(3, mms_op_num, clid - IDC_EDIT_MMS_G3_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // connect
			break;			
		case IDC_EDIT_MMS_G4_P1:
		case IDC_EDIT_MMS_G4_P2:
		case IDC_EDIT_MMS_G4_P3:
		case IDC_EDIT_MMS_G4_P4:
			mms_editor_set_param(4, mms_op_num, clid - IDC_EDIT_MMS_G4_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // osc
			break;			
		case IDC_EDIT_MMS_G5_P1:
		case IDC_EDIT_MMS_G5_P2:
		case IDC_EDIT_MMS_G5_P3:
		case IDC_EDIT_MMS_G5_P4:
		case IDC_EDIT_MMS_G5_P5:
		case IDC_EDIT_MMS_G5_P6:
		case IDC_EDIT_MMS_G5_P7:
		case IDC_EDIT_MMS_G5_P8:
			mms_editor_set_param(5, mms_op_num, clid - IDC_EDIT_MMS_G5_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // wave
			if(clid == IDC_EDIT_MMS_G5_P1 || clid == IDC_EDIT_MMS_G5_P2)
				SetDlgItemText(hwnd, IDC_EDIT_MMS_WAVE_DATA_NAME, mms_editor_load_wave_name(mms_op_num, -1));
			break;		
		case IDC_EDIT_MMS_G6_P1:
		case IDC_EDIT_MMS_G6_P2:
		case IDC_EDIT_MMS_G6_P3:
		case IDC_EDIT_MMS_G6_P4:
			mms_editor_set_param(6, mms_op_num, clid - IDC_EDIT_MMS_G6_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // sub
			break;	
		case IDC_EDIT_MMS_G7_P1:
		case IDC_EDIT_MMS_G7_P2:
			mms_editor_set_param(7, mms_op_num, clid - IDC_EDIT_MMS_G7_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // amp
			break;	
		case IDC_EDIT_MMS_G8_P1:
		case IDC_EDIT_MMS_G8_P2:
		case IDC_EDIT_MMS_G8_P3:
		case IDC_EDIT_MMS_G8_P4:
			mms_editor_set_param(8, mms_op_num, clid - IDC_EDIT_MMS_G8_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // pitch
			break;	
		case IDC_EDIT_MMS_G9_P1:
		case IDC_EDIT_MMS_G9_P2:
		case IDC_EDIT_MMS_G9_P3:
		case IDC_EDIT_MMS_G9_P4:
			mms_editor_set_param(9, mms_op_num, clid - IDC_EDIT_MMS_G9_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // width
			break;	
		case IDC_EDIT_MMS_G10_P1:
		case IDC_EDIT_MMS_G10_P2:
		case IDC_EDIT_MMS_G10_P3:
		case IDC_EDIT_MMS_G10_P4:
			mms_editor_set_param(10, mms_op_num, clid - IDC_EDIT_MMS_G10_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // filter
			if(clid == IDC_EDIT_MMS_G10_P1)
				SetDlgItemText(hwnd, IDC_EDIT_MMS_FILTER_NAME, mms_editor_load_filter_name(mms_op_num));
			break;	
		case IDC_EDIT_MMS_G11_P1:
		case IDC_EDIT_MMS_G11_P2:
		case IDC_EDIT_MMS_G11_P3:
		case IDC_EDIT_MMS_G11_P4:
			mms_editor_set_param(11, mms_op_num, clid - IDC_EDIT_MMS_G11_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // cutoff
			break;				
		case IDC_EDIT_MMS_G12_P1:
		case IDC_EDIT_MMS_G12_P2:
		case IDC_EDIT_MMS_G12_P3:
		case IDC_EDIT_MMS_G12_P4:
		case IDC_EDIT_MMS_G12_P5:
		case IDC_EDIT_MMS_G12_P6:
		case IDC_EDIT_MMS_G12_P7:
		case IDC_EDIT_MMS_G12_P8:
		case IDC_EDIT_MMS_G12_P9:
		case IDC_EDIT_MMS_G12_P10:
		case IDC_EDIT_MMS_G12_P11:
		case IDC_EDIT_MMS_G12_P12:
		case IDC_EDIT_MMS_G12_P13:
		case IDC_EDIT_MMS_G12_P14:
			mms_editor_set_param(12, mms_op_num, clid - IDC_EDIT_MMS_G12_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // ampenv
			break;
		case IDC_EDIT_MMS_G13_P1:
		case IDC_EDIT_MMS_G13_P2:
		case IDC_EDIT_MMS_G13_P3:
		case IDC_EDIT_MMS_G13_P4:
		case IDC_EDIT_MMS_G13_P5:
		case IDC_EDIT_MMS_G13_P6:
		case IDC_EDIT_MMS_G13_P7:
		case IDC_EDIT_MMS_G13_P8:
		case IDC_EDIT_MMS_G13_P9:
		case IDC_EDIT_MMS_G13_P10:
		case IDC_EDIT_MMS_G13_P11:
		case IDC_EDIT_MMS_G13_P12:
		case IDC_EDIT_MMS_G13_P13:
		case IDC_EDIT_MMS_G13_P14:
			mms_editor_set_param(13, mms_op_num, clid - IDC_EDIT_MMS_G13_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // pitenv
			break;
		case IDC_EDIT_MMS_G14_P1:
		case IDC_EDIT_MMS_G14_P2:
		case IDC_EDIT_MMS_G14_P3:
		case IDC_EDIT_MMS_G14_P4:
		case IDC_EDIT_MMS_G14_P5:
		case IDC_EDIT_MMS_G14_P6:
		case IDC_EDIT_MMS_G14_P7:
		case IDC_EDIT_MMS_G14_P8:
		case IDC_EDIT_MMS_G14_P9:
		case IDC_EDIT_MMS_G14_P10:
		case IDC_EDIT_MMS_G14_P11:
		case IDC_EDIT_MMS_G14_P12:
		case IDC_EDIT_MMS_G14_P13:
		case IDC_EDIT_MMS_G14_P14:
			mms_editor_set_param(14, mms_op_num, clid - IDC_EDIT_MMS_G14_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // widenv
			break;
		case IDC_EDIT_MMS_G15_P1:
		case IDC_EDIT_MMS_G15_P2:
		case IDC_EDIT_MMS_G15_P3:
		case IDC_EDIT_MMS_G15_P4:
		case IDC_EDIT_MMS_G15_P5:
		case IDC_EDIT_MMS_G15_P6:
		case IDC_EDIT_MMS_G15_P7:
		case IDC_EDIT_MMS_G15_P8:
		case IDC_EDIT_MMS_G15_P9:
		case IDC_EDIT_MMS_G15_P10:
		case IDC_EDIT_MMS_G15_P11:
		case IDC_EDIT_MMS_G15_P12:
		case IDC_EDIT_MMS_G15_P13:
		case IDC_EDIT_MMS_G15_P14:
			mms_editor_set_param(15, mms_op_num, clid - IDC_EDIT_MMS_G15_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // modenv
			break;			
		case IDC_EDIT_MMS_G16_P1:
		case IDC_EDIT_MMS_G16_P2:
		case IDC_EDIT_MMS_G16_P3:
		case IDC_EDIT_MMS_G16_P4:
		case IDC_EDIT_MMS_G16_P5:
		case IDC_EDIT_MMS_G16_P6:
		case IDC_EDIT_MMS_G16_P7:
		case IDC_EDIT_MMS_G16_P8:
		case IDC_EDIT_MMS_G16_P9:
		case IDC_EDIT_MMS_G16_P10:
		case IDC_EDIT_MMS_G16_P11:
		case IDC_EDIT_MMS_G16_P12:
		case IDC_EDIT_MMS_G16_P13:
		case IDC_EDIT_MMS_G16_P14:
			mms_editor_set_param(16, mms_op_num, clid - IDC_EDIT_MMS_G16_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // ampenv_keyf
			break;
		case IDC_EDIT_MMS_G17_P1:
		case IDC_EDIT_MMS_G17_P2:
		case IDC_EDIT_MMS_G17_P3:
		case IDC_EDIT_MMS_G17_P4:
		case IDC_EDIT_MMS_G17_P5:
		case IDC_EDIT_MMS_G17_P6:
		case IDC_EDIT_MMS_G17_P7:
		case IDC_EDIT_MMS_G17_P8:
		case IDC_EDIT_MMS_G17_P9:
		case IDC_EDIT_MMS_G17_P10:
		case IDC_EDIT_MMS_G17_P11:
		case IDC_EDIT_MMS_G17_P12:
		case IDC_EDIT_MMS_G17_P13:
		case IDC_EDIT_MMS_G17_P14:
			mms_editor_set_param(17, mms_op_num, clid - IDC_EDIT_MMS_G17_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // pitenv_keyf
			break;
		case IDC_EDIT_MMS_G18_P1:
		case IDC_EDIT_MMS_G18_P2:
		case IDC_EDIT_MMS_G18_P3:
		case IDC_EDIT_MMS_G18_P4:
		case IDC_EDIT_MMS_G18_P5:
		case IDC_EDIT_MMS_G18_P6:
		case IDC_EDIT_MMS_G18_P7:
		case IDC_EDIT_MMS_G18_P8:
		case IDC_EDIT_MMS_G18_P9:
		case IDC_EDIT_MMS_G18_P10:
		case IDC_EDIT_MMS_G18_P11:
		case IDC_EDIT_MMS_G18_P12:
		case IDC_EDIT_MMS_G18_P13:
		case IDC_EDIT_MMS_G18_P14:
			mms_editor_set_param(18, mms_op_num, clid - IDC_EDIT_MMS_G18_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // widenv_keyf
			break;
		case IDC_EDIT_MMS_G19_P1:
		case IDC_EDIT_MMS_G19_P2:
		case IDC_EDIT_MMS_G19_P3:
		case IDC_EDIT_MMS_G19_P4:
		case IDC_EDIT_MMS_G19_P5:
		case IDC_EDIT_MMS_G19_P6:
		case IDC_EDIT_MMS_G19_P7:
		case IDC_EDIT_MMS_G19_P8:
		case IDC_EDIT_MMS_G19_P9:
		case IDC_EDIT_MMS_G19_P10:
		case IDC_EDIT_MMS_G19_P11:
		case IDC_EDIT_MMS_G19_P12:
		case IDC_EDIT_MMS_G19_P13:
		case IDC_EDIT_MMS_G19_P14:
			mms_editor_set_param(19, mms_op_num, clid - IDC_EDIT_MMS_G19_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // modenv_keyf
			break;			
		case IDC_EDIT_MMS_G20_P1:
		case IDC_EDIT_MMS_G20_P2:
		case IDC_EDIT_MMS_G20_P3:
		case IDC_EDIT_MMS_G20_P4:
		case IDC_EDIT_MMS_G20_P5:
		case IDC_EDIT_MMS_G20_P6:
		case IDC_EDIT_MMS_G20_P7:
		case IDC_EDIT_MMS_G20_P8:
		case IDC_EDIT_MMS_G20_P9:
		case IDC_EDIT_MMS_G20_P10:
		case IDC_EDIT_MMS_G20_P11:
		case IDC_EDIT_MMS_G20_P12:
		case IDC_EDIT_MMS_G20_P13:
		case IDC_EDIT_MMS_G20_P14:
			mms_editor_set_param(20, mms_op_num, clid - IDC_EDIT_MMS_G20_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // ampenv_velf
			break;	
		case IDC_EDIT_MMS_G21_P1:
		case IDC_EDIT_MMS_G21_P2:
		case IDC_EDIT_MMS_G21_P3:
		case IDC_EDIT_MMS_G21_P4:
		case IDC_EDIT_MMS_G21_P5:
		case IDC_EDIT_MMS_G21_P6:
		case IDC_EDIT_MMS_G21_P7:
		case IDC_EDIT_MMS_G21_P8:
		case IDC_EDIT_MMS_G21_P9:
		case IDC_EDIT_MMS_G21_P10:
		case IDC_EDIT_MMS_G21_P11:
		case IDC_EDIT_MMS_G21_P12:
		case IDC_EDIT_MMS_G21_P13:
		case IDC_EDIT_MMS_G21_P14:
			mms_editor_set_param(21, mms_op_num, clid - IDC_EDIT_MMS_G21_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // pitenv_velf
			break;	
		case IDC_EDIT_MMS_G22_P1:
		case IDC_EDIT_MMS_G22_P2:
		case IDC_EDIT_MMS_G22_P3:
		case IDC_EDIT_MMS_G22_P4:
		case IDC_EDIT_MMS_G22_P5:
		case IDC_EDIT_MMS_G22_P6:
		case IDC_EDIT_MMS_G22_P7:
		case IDC_EDIT_MMS_G22_P8:
		case IDC_EDIT_MMS_G22_P9:
		case IDC_EDIT_MMS_G22_P10:
		case IDC_EDIT_MMS_G22_P11:
		case IDC_EDIT_MMS_G22_P12:
		case IDC_EDIT_MMS_G22_P13:
		case IDC_EDIT_MMS_G22_P14:
			mms_editor_set_param(22, mms_op_num, clid - IDC_EDIT_MMS_G22_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // widenv_velf
			break;	
		case IDC_EDIT_MMS_G23_P1:
		case IDC_EDIT_MMS_G23_P2:
		case IDC_EDIT_MMS_G23_P3:
		case IDC_EDIT_MMS_G23_P4:
		case IDC_EDIT_MMS_G23_P5:
		case IDC_EDIT_MMS_G23_P6:
		case IDC_EDIT_MMS_G23_P7:
		case IDC_EDIT_MMS_G23_P8:
		case IDC_EDIT_MMS_G23_P9:
		case IDC_EDIT_MMS_G23_P10:
		case IDC_EDIT_MMS_G23_P11:
		case IDC_EDIT_MMS_G23_P12:
		case IDC_EDIT_MMS_G23_P13:
		case IDC_EDIT_MMS_G23_P14:
			mms_editor_set_param(23, mms_op_num, clid - IDC_EDIT_MMS_G23_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // modenv_velf
			break;			
		case IDC_EDIT_MMS_G24_P1:
		case IDC_EDIT_MMS_G24_P2:
		case IDC_EDIT_MMS_G24_P3:
		case IDC_EDIT_MMS_G24_P4:
			mms_editor_set_param(24, mms_op_num, clid - IDC_EDIT_MMS_G24_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // lfo1
			if(clid == IDC_EDIT_MMS_G24_P3)
				SetDlgItemText(hwnd, IDC_EDIT_MMS_LFO1_WAVE_NAME, mms_editor_load_wave_name(mms_op_num, 0));
			break;
		case IDC_EDIT_MMS_G25_P1:
		case IDC_EDIT_MMS_G25_P2:
		case IDC_EDIT_MMS_G25_P3:
		case IDC_EDIT_MMS_G25_P4:
			mms_editor_set_param(25, mms_op_num, clid - IDC_EDIT_MMS_G25_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // lfo2
			if(clid == IDC_EDIT_MMS_G25_P3)
				SetDlgItemText(hwnd, IDC_EDIT_MMS_LFO2_WAVE_NAME, mms_editor_load_wave_name(mms_op_num, 1));
			break;
		case IDC_EDIT_MMS_G26_P1:
		case IDC_EDIT_MMS_G26_P2:
		case IDC_EDIT_MMS_G26_P3:
		case IDC_EDIT_MMS_G26_P4:
			mms_editor_set_param(26, mms_op_num, clid - IDC_EDIT_MMS_G26_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // lfo3
			if(clid == IDC_EDIT_MMS_G26_P3)
				SetDlgItemText(hwnd, IDC_EDIT_MMS_LFO3_WAVE_NAME, mms_editor_load_wave_name(mms_op_num, 2));
			break;
		case IDC_EDIT_MMS_G27_P1:
		case IDC_EDIT_MMS_G27_P2:
		case IDC_EDIT_MMS_G27_P3:
		case IDC_EDIT_MMS_G27_P4:
			mms_editor_set_param(27, mms_op_num, clid - IDC_EDIT_MMS_G27_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // lfo4
			if(clid == IDC_EDIT_MMS_G27_P3)
				SetDlgItemText(hwnd, IDC_EDIT_MMS_LFO4_WAVE_NAME, mms_editor_load_wave_name(mms_op_num, 3));
			break;
		case IDC_EDIT_MMS_G28_P1:
		case IDC_EDIT_MMS_G28_P2:
		case IDC_EDIT_MMS_G28_P3:
		case IDC_EDIT_MMS_G28_P4:
			mms_editor_set_param(28, mms_op_num, clid - IDC_EDIT_MMS_G28_P1, (int)GetDlgItemInt(hwnd, clid, NULL, TRUE)); // loop
			break;
		case IDC_EDIT_MMS_WAVE_DATA_NAME:
			focus_clid = prv_focus_clid;
			break;
		case IDC_EDIT_MMS_FILTER_NAME:
			focus_clid = IDC_EDIT_MMS_G10_P1;
			break;
		case IDC_EDIT_MMS_LFO1_WAVE_NAME:
			focus_clid = IDC_EDIT_MMS_G24_P3;
			break;
		case IDC_EDIT_MMS_LFO2_WAVE_NAME:
			focus_clid = IDC_EDIT_MMS_G25_P3;
			break;
		case IDC_EDIT_MMS_LFO3_WAVE_NAME:
			focus_clid = IDC_EDIT_MMS_G26_P3;
			break;
		case IDC_EDIT_MMS_LFO4_WAVE_NAME:
			focus_clid = IDC_EDIT_MMS_G27_P3;
			break;
		}
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

typedef struct is_editor_page_t_ {
	int index;
	TCHAR *title;
	HWND hwnd;
	UINT control;
	DLGPROC dlgproc;
	int opt;
} is_editor_page_t;

static is_editor_page_t is_editor_pages_ja[] = {
	{ 0, TEXT("SCC_DATA"), (HWND)NULL, IDD_ISEDITOR_SCC_DATA, (DLGPROC) ISEditorSCCDATAProc, 0 },
	{ 1, TEXT("SCC"), (HWND)NULL, IDD_ISEDITOR_SCC, (DLGPROC) ISEditorSCCProc, 0 },
	{ 2, TEXT("MMS"), (HWND)NULL, IDD_ISEDITOR_MMS, (DLGPROC) ISEditorMMSProc, 0 },
};

static is_editor_page_t is_editor_pages_en[] = {
	{ 0, TEXT("SCC_DATA"), (HWND)NULL, IDD_ISEDITOR_SCC_DATA_EN, (DLGPROC) ISEditorSCCDATAProc, 0 },
	{ 1, TEXT("SCC"), (HWND)NULL, IDD_ISEDITOR_SCC_EN, (DLGPROC) ISEditorSCCProc, 0 },
	{ 2, TEXT("MMS"), (HWND)NULL, IDD_ISEDITOR_MMS_EN, (DLGPROC) ISEditorMMSProc, 0 },
};

#define ISEDITOR_PAGE_MAX 3

static is_editor_page_t *is_editor_pages;
volatile int ISEditorWndDoing = 0;
static volatile int ISEditorWndSetOK = 0;
static int ISEditorInitialPage = 0;
static HWND hISEditorWnd = NULL;

#define ETDT_DISABLE (1)
#define ETDT_ENABLE (2)
#define ETDT_USETABTEXTURE (4)
#define ETDT_ENABLETAB (ETDT_ENABLE|ETDT_USETABTEXTURE)
typedef BOOL (WINAPI *IsThemeActiveFn)(void);
typedef HRESULT (WINAPI *EnableThemeDialogTextureFn)(HWND hwnd, DWORD dwFlags);

static void ISEditorWndCreateTabItems(HWND hwnd)
{
    int i;
    HWND hwnd_tab;

    switch (PlayerLanguage) {
    case LANGUAGE_JAPANESE:
	is_editor_pages = is_editor_pages_ja;
	break;
    default:
    case LANGUAGE_ENGLISH:
	is_editor_pages = is_editor_pages_en;
	break;
    }

    hwnd_tab = GetDlgItem(hwnd, IDC_TAB_ISEDITOR);
    for (i = 0; i < ISEDITOR_PAGE_MAX; i++) {
	TC_ITEM tci;
	tci.mask = TCIF_TEXT;
	tci.pszText = is_editor_pages[i].title;
	tci.cchTextMax = strlen(is_editor_pages[i].title);
	SendMessage(hwnd_tab, TCM_INSERTITEM, (WPARAM)i, (LPARAM)&tci);

	is_editor_pages[i].hwnd = NULL;
    }
}

static void ISEditorWndCreatePage(HWND hwnd, UINT page)
{
    RECT rc;
    HWND hwnd_tab;
    HANDLE hUXTheme;
    IsThemeActiveFn pfnIsThemeActive;
    EnableThemeDialogTextureFn pfnEnableThemeDialogTexture;
    BOOL theme_active = FALSE;

    if (page >= ISEDITOR_PAGE_MAX || is_editor_pages[page].hwnd)
		return;
    switch (PlayerLanguage) {
    case LANGUAGE_JAPANESE:
		is_editor_pages = is_editor_pages_ja;
		break;
    default:
    case LANGUAGE_ENGLISH:
		is_editor_pages = is_editor_pages_en;
		break;
    }
    hwnd_tab = GetDlgItem(hwnd, IDC_TAB_ISEDITOR);
    if (!hwnd_tab)
		return;

    GetClientRect(hwnd_tab, &rc);
    SendDlgItemMessage(hwnd, IDC_TAB_ISEDITOR, TCM_ADJUSTRECT, (WPARAM)0, (LPARAM)&rc);
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
    is_editor_pages[page].hwnd = CreateDialog(hInst, MAKEINTRESOURCE(is_editor_pages[page].control),
	hwnd, is_editor_pages[page].dlgproc);
    ShowWindow(is_editor_pages[page].hwnd, SW_HIDE);
    MoveWindow(is_editor_pages[page].hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
    if (theme_active)
	(*pfnEnableThemeDialogTexture)(is_editor_pages[page].hwnd, ETDT_ENABLETAB);
    if (hUXTheme) {
		hUXTheme = NULL;
    }
}

static int DlgOpenISIniFile(char *Filename, HWND hwnd)
{
	OPENFILENAMEA ofn;
	char filename[FILEPATH_MAX];
	static char dir[FILEPATH_MAX];
	int res;
	const char *filter,
		   *filter_en = "Ini file (*.ini)\0*.ini\0"
				"All files (*.*)\0*.*\0"
				"\0\0",
		   *filter_jp = "Ini ファイル (*.ini)\0*.ini\0"
				"すべてのファイル (*.*)\0*.*\0"
				"\0\0";
	const char *title,
		   *title_en = "Open Ini File",
		   *title_jp = "Iniファイルを開く";

	if (PlayerLanguage == LANGUAGE_JAPANESE) {
		filter = filter_jp;
		title = title_jp;
	}
	else {
		filter = filter_en;
		title = title_en;
	}
	if(ISIniFileOpenDir[0] == '\0')
		strncpy(ISIniFileOpenDir, ConfigFileOpenDir, FILEPATH_MAX);
	strncpy(dir, ISIniFileOpenDir, FILEPATH_MAX);
	dir[FILEPATH_MAX - 1] = '\0';
	strncpy(filename, Filename, FILEPATH_MAX);
	filename[FILEPATH_MAX - 1] = '\0';
	if (strlen(filename) > 0 && IS_PATH_SEP(filename[strlen(filename) - 1])) {
		strlcat(filename, "int_synth.ini", FILEPATH_MAX);
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
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER
	| OFN_READONLY | OFN_HIDEREADONLY;
	ofn.lpstrDefExt = 0;
	ofn.lCustData = 0;
	ofn.lpfnHook = 0;
	ofn.lpTemplateName = 0;

	res = SafeGetOpenFileName(&ofn);
	strncpy(ISIniFileOpenDir, dir, FILEPATH_MAX);
	ISIniFileOpenDir[FILEPATH_MAX - 1] = '\0';
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

LRESULT APIENTRY CALLBACK ISEditorWndDialogProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	int i;
	WORD clid = 0;
	static WORD focus_clid = 0, prv_focus_clid = 0;
	int16 wheel_speed = 0;
	char filename[FILEPATH_MAX];

	switch (uMess){
	case WM_INITDIALOG:
	{
		hISEditorWnd = hwnd;
		// main		
		SetDlgItemText(hwnd, IDC_EDIT_IS_INI_FILE, is_editor_get_ini_path());
		// table
		ISEditorWndCreateTabItems(hwnd);
		ISEditorWndCreatePage(hwnd, 0);
		SetForegroundWindow(hwnd);
		SendDlgItemMessage ( hwnd, IDC_TAB_ISEDITOR, TCM_SETCURSEL, (WPARAM)0, (LPARAM)0 );
		ShowWindow ( is_editor_pages[0].hwnd, TRUE );
		return TRUE;
	}
	case WM_MOUSEWHEEL:		
		if(focus_wnd != 0)
			break;
		wheel_speed = (int16)GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA; // upper 16bit sined int // 1knoch = 120		
		wheel_speed *= (LOWORD(wParam) & MK_SHIFT) ? 10 : 1;
		break;
	case WM_COMMAND:
		clid = LOWORD(wParam);
		prv_focus_clid = focus_clid;
		focus_clid = clid;
		focus_wnd = 0;
		switch (clid) {
		case IDC_BUTTON_IS_INI_FILE:
			{
				filename[0] = '\0';
				GetDlgItemText(hwnd, IDC_EDIT_IS_INI_FILE, filename, (WPARAM)sizeof(filename));
				if(!DlgOpenISIniFile(filename, hwnd))
				if(filename[0] != '\0'){
					SetDlgItemText(hwnd, IDC_EDIT_IS_INI_FILE, TEXT(filename));
					is_editor_set_ini_path((const char *)filename);
					is_editor_load_ini();
					SendMessage(hwnd, WM_ISE_RESTORE, (WPARAM)0, (LPARAM)0 );
				}
			}
			break;
		case IDC_BUTTON_IS_LOAD_INI_FILE:
			{
				filename[0] = '\0';
				GetDlgItemText(hwnd, IDC_EDIT_IS_INI_FILE, filename, (WPARAM)sizeof(filename));
				is_editor_set_ini_path((const char *)filename);
				is_editor_load_ini();
				SendMessage(hwnd, WM_ISE_RESTORE, (WPARAM)0, (LPARAM)0 );
			}
			break;
		}
		break;

	case WM_NOTIFY:
      {
	LPNMHDR pnmh = (LPNMHDR) lParam;
	if (pnmh->idFrom == IDC_TAB_ISEDITOR) {
	    switch (pnmh->code) {
	    case TCN_SELCHANGE:
	    {
		int nIndex = SendDlgItemMessage(hwnd, IDC_TAB_ISEDITOR, TCM_GETCURSEL, (WPARAM)0, (LPARAM)0);
		for (i = 0; i < ISEDITOR_PAGE_MAX; i++) {
		    if (is_editor_pages[i].hwnd)
			ShowWindow(is_editor_pages[i].hwnd, SW_HIDE);
		}
		ISEditorWndCreatePage(hwnd, nIndex);
		ShowWindow(is_editor_pages[nIndex].hwnd, SW_SHOWNORMAL);
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
		HWND hwnd_tab = GetDlgItem ( hwnd, IDC_TAB_ISEDITOR );
		GetClientRect ( hwnd_tab, &rc );
		SendDlgItemMessage ( hwnd, IDC_TAB_ISEDITOR, TCM_ADJUSTRECT, (WPARAM)TRUE, (LPARAM)&rc );
		for ( i = 0; i < ISEDITOR_PAGE_MAX; i++ ) {
			MoveWindow ( is_editor_pages[i].hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE );
		}
		return TRUE;
	}
	case WM_DESTROY:
	case WM_CLOSE:			
		EndDialog ( hwnd, FALSE );
		uninit_is_editor_param();
		break;
	default:
	  break;
	}
	return FALSE;
}

static UINT ISEditorSearchPageFromCID(UINT cid)
{
    int32 i;
    UINT num = 0;
    is_editor_page_t *page;

    switch (PlayerLanguage) {
    case LANGUAGE_JAPANESE:
		page = is_editor_pages_ja;
		break;
    default:
		case LANGUAGE_ENGLISH:
		page = is_editor_pages_en;
		break;
    }
    for (i = 0; i < ISEDITOR_PAGE_MAX; i++) {
		if (page[i].control == cid) {
			num = i;
		}
    }
    return num;
}

void ISEditorWndCreate(HWND hwnd)
{
    UINT page = ISEditorSearchPageFromCID(0);
	HICON hIcon;

    VOLATILE_TOUCH(ISEditorWndDoing);
    if (ISEditorWndDoing)
	return;
	init_is_editor_param();
	scc_data_num = 0;
	scc_preset_num = 0;
	mms_preset_num = 0;
	mms_op_num = 0;
	focus_wnd = 0;
    ISEditorWndDoing = 1;
    ISEditorWndSetOK = 1;
    ISEditorInitialPage = page;
	switch(PlayerLanguage) {
		case LANGUAGE_JAPANESE:
		//	DialogBox ( hInst, MAKEINTRESOURCE(IDD_DIALOG_ISEDITOR), hwnd, ISEditorWndDialogProc );
			hISEditorWnd = CreateDialog(hInst, MAKEINTRESOURCE(IDD_DIALOG_ISEDITOR), hwnd, ISEditorWndDialogProc);
			break;
		default:
		case LANGUAGE_ENGLISH:
		//	DialogBox ( hInst, MAKEINTRESOURCE(IDD_DIALOG_ISEDITOR_EN), hwnd, ISEditorWndDialogProc );
			hISEditorWnd = CreateDialog(hInst, MAKEINTRESOURCE(IDD_DIALOG_ISEDITOR), hwnd, ISEditorWndDialogProc);
			break;
	}
	ISEditorWndSetOK = 0;
	ISEditorWndDoing = 0;		
	ShowWindow(hISEditorWnd, SW_HIDE);
	hIcon = LoadImage(hInst, MAKEINTRESOURCE(IDI_ICON_TIMIDITY), IMAGE_ICON, 16, 16, 0);
	if (hIcon) SendMessage(hISEditorWnd, WM_SETICON, FALSE, (LPARAM)hIcon);
	UpdateWindow(hISEditorWnd);
	ShowWindow(hISEditorWnd, SW_SHOW);
	return;
}








#undef DLG_CHECKBUTTON_TO_FLAG
#undef DLG_FLAG_TO_CHECKBUTTON
#undef CHECKRANGE_ISEDITOR_PARAM

#endif // INT_SYNTH