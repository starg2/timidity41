/*
    TiMidity -- Experimental MIDI to WAVE converter
    Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <windowsx.h>
#undef RC_NONE
#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "controls.h"
#include "controls.h"
#include "output.h"
#include "w32g.h"
#include "w32g_res.h"
#ifdef LISTVIEW_PLAYLIST
#include <commctrl.h>
#endif

#define W32G_RANDOM_IS_SHUFFLE

void SetNumListWnd(int cursel, int nfiles);

// playlist
typedef struct _PlayListEntry {
    char *filepath;	// malloc
    char *filename;	// shared with filepath
    char *title;	// shared with midi_file_info
#ifdef LISTVIEW_PLAYLIST
	char *artist;	// shared with midi_file_info
	char *duration;	// malloc
	char *filetype;	// malloc
	char *system;	// malloc
#endif
    struct midi_file_info *info;
} PlayListEntry;

typedef struct {
    int nfiles;
    int selected; /* 0..nfiles-1 */
    int allocated; /* number of PlayListEntry is allocated */
    PlayListEntry *list;
} PlayList;

int playlist_max = 1;
int playlist_max_ini = 1;
static PlayList playlist[PLAYLIST_MAX] = {0, 0, 0, NULL};
static PlayList tmp_playlist = {0, 0, 0, NULL};
static int playlist_num_ctrl = 0;
static int playlist_num_play = 0;
static int playlist_num_ctrl_prev = 0;
static int playlist_reset = 0;
static PlayList *playlist_ctrl = &playlist[0];
static PlayList *playlist_play = &playlist[0];

int w32g_get_playlist_num_ctrl(void)
{
	return playlist_num_ctrl;
}

int w32g_is_playlist_ctrl_play(void)
{
	return (playlist_num_ctrl == playlist_num_play) ? 1 : 0;
}

void w32g_set_playlist_ctrl(int num)
{
	if(num < 0)
		num = 0;
	else if(num >= playlist_max)
		num = playlist_max - 1;
	if(num >= PLAYLIST_MAX)
		num = PLAYLIST_MAX - 1;
	playlist_num_ctrl_prev = playlist_num_ctrl;
	playlist_num_ctrl = num;
	if(playlist_num_ctrl_prev != playlist_num_ctrl)
		playlist_reset = 1;
	playlist_ctrl = &playlist[num];
}

void w32g_set_playlist_ctrl_play(void)
{
	playlist_num_play = playlist_num_ctrl;
	playlist_play = playlist_ctrl;
}

#ifdef LISTVIEW_PLAYLIST
void w32g_get_midi_file_info_post(PlayListEntry *entry)
{
	// duration
	if(entry->duration == NULL && entry->info->samples > 0){
		char buff[64];
		int32 sec, time_h, time_m;
		sec = entry->info->samples / play_mode->rate;
		time_h = sec / 60 / 60;
		sec %= 60 * 60;
		time_m = sec / 60;
		sec %= 60;	
		sprintf(buff,"%02d:%02d:%02d", time_h, time_m, sec);
		entry->duration = safe_strdup(buff);
	}
	// filetype
	if(entry->filetype == NULL && entry->info->file_type > 0){
		// see readmidi.h /* MIDI file types */
		switch(entry->info->file_type){
		case IS_SMF_FILE:
			entry->filetype = safe_strdup(TEXT("Standard MIDI File"));	break;
		case IS_MCP_FILE:
			entry->filetype = safe_strdup(TEXT("MCP File"));	break;
		case IS_RCP_FILE:
		case IS_R36_FILE:
		case IS_G18_FILE:
		case IS_G36_FILE:
			entry->filetype = safe_strdup(TEXT("Recomposer"));	break;
		case IS_SNG_FILE:
			entry->filetype = safe_strdup(TEXT("SNG File"));	break;
		case IS_MM2_FILE:
			entry->filetype = safe_strdup(TEXT("MM2 File"));	break;
		case IS_MML_FILE:
			entry->filetype = safe_strdup(TEXT("MML File"));	break;
		case IS_FM_FILE:
			entry->filetype = safe_strdup(TEXT("FM File"));	break;
		case IS_FPD_FILE:
			entry->filetype = safe_strdup(TEXT("FPD File"));	break;
		case IS_MOD_FILE:
			entry->filetype = safe_strdup(TEXT("Pro/Fast/Star/Noise Tracker"));	break;
		case IS_669_FILE:
			entry->filetype = safe_strdup(TEXT("Composer669/UNIS669"));	break;
		case IS_MTM_FILE:
			entry->filetype = safe_strdup(TEXT("MultiModuleEdit"));	break;
		case IS_STM_FILE:
			entry->filetype = safe_strdup(TEXT("ScreamTracker2"));	break;
		case IS_S3M_FILE:
			entry->filetype = safe_strdup(TEXT("ScreamTracker3"));	break;
		case IS_ULT_FILE:
			entry->filetype = safe_strdup(TEXT("UltraTracker"));	break;
		case IS_XM_FILE:
			entry->filetype = safe_strdup(TEXT("FastTracker2"));	break;
		case IS_FAR_FILE:
			entry->filetype = safe_strdup(TEXT("Farandole Composer"));	break;
		case IS_WOW_FILE:
			entry->filetype = safe_strdup(TEXT("Grave Composer"));	break;
		case IS_OKT_FILE:
			entry->filetype = safe_strdup(TEXT("Oktalyzer"));	break;
		case IS_DMF_FILE:
			entry->filetype = safe_strdup(TEXT("X-Tracker"));	break;
		case IS_MED_FILE:
			entry->filetype = safe_strdup(TEXT("MED/OctaMED"));	break;
		case IS_IT_FILE:
			entry->filetype = safe_strdup(TEXT("ImpulseTracker"));	break;
		case IS_PTM_FILE:
			entry->filetype = safe_strdup(TEXT("PolyTracker"));	break;
		case IS_MFI_FILE:
			entry->filetype = safe_strdup(TEXT("i-mode Melody Format"));	break;
		default:
			entry->filetype = safe_strdup(TEXT("Other File"));	break;
		}
	}
	// system
	if(entry->system == NULL && entry->info->mid > 0){		
		switch(entry->info->mid){
		case 0x41:
			entry->system = safe_strdup(TEXT("GS"));	break;
		case 0x42:
			entry->system = safe_strdup(TEXT("Korg"));	break;
		case 0x43:
			entry->system = safe_strdup(TEXT("XG"));	break;
		case 0x7d:
			entry->system = safe_strdup(TEXT("GM2"));	break;
		case 0x7e:
			entry->system = safe_strdup(TEXT("GM"));	break;
		case 0x60:
			entry->system = safe_strdup(TEXT("SD"));	break;
		case 0x30:
			entry->system = safe_strdup(TEXT("CM"));	break;
		default:
			entry->system = safe_strdup(TEXT("Other"));	break;
		}
	}
}
#endif

static char *get_filename(char *filepath)
{
	int i, last = 0, len = strlen(filepath);
	char *buf = filepath;
	for(i = 0; i < len; i++){
		if(buf[i] == '\\' || buf[i] == '/')
			last = i;
	}
	if(last)
		last++;
	return (buf + last);
}

static HWND playlist_box(void)
{
    if(!hListWnd)
	return 0;
#ifdef LISTVIEW_PLAYLIST
    return GetDlgItem(hListWnd, IDC_LV_PLAYLIST);
#else
    return GetDlgItem(hListWnd, IDC_LISTBOX_PLAYLIST);
#endif
}

static int w32g_add_playlist1(char *filename, int uniq, int refine)
{
    PlayListEntry *entry;
    char *title;
    struct midi_file_info *info;

    if(uniq)
    {
	int i;
	for(i = 0; i < playlist_ctrl->nfiles; i++)
	    if(pathcmp(filename, playlist_ctrl->list[i].filepath, 0) == 0)
		return 0;
    }

    title = get_midi_title(filename); // set artist to info
    info = get_midi_file_info(filename, 1);

	if(refine && info->format < 0)
	return 0;

    if(playlist_ctrl->allocated == 0)
    {
	playlist_ctrl->allocated = 32;
	playlist_ctrl->list = (PlayListEntry *)safe_malloc(playlist_ctrl->allocated *
						     sizeof(PlayListEntry));
    }
    else if(playlist_ctrl->nfiles == playlist_ctrl->allocated)
    {
	playlist_ctrl->allocated *= 2;
	playlist_ctrl->list = (PlayListEntry *)safe_realloc(playlist_ctrl->list,
						      playlist_ctrl->allocated *
						      sizeof(PlayListEntry));
    }

    entry = &playlist_ctrl->list[playlist_ctrl->nfiles];
    entry->filepath = safe_strdup(filename);
	entry->filename = get_filename(entry->filepath);
    entry->title = title;
#ifdef LISTVIEW_PLAYLIST
	entry->artist = info->artist_name;
	entry->duration = NULL;
	entry->filetype = NULL;
	entry->system = NULL;
#endif
    entry->info = info;
    playlist_ctrl->nfiles++;
	w32g_shuffle_playlist_reset(1);
    return 1;
}

int w32g_add_playlist(int nfiles, char **files, int expand_flag,
		      int uniq, int refine)
{
    char **new_files1;
    char **new_files2;
    int i, n;
    extern int SeachDirRecursive;
    extern char **FilesExpandDir(int *, char **);

    if(nfiles == 0)
	return 0;

    if(SeachDirRecursive)
    {
	new_files1 = FilesExpandDir(&nfiles, files);
	if(new_files1 == NULL)
	    return 0;
	expand_flag = 1;
    }
    else
	new_files1 = files;

    if(!expand_flag)
	new_files2 = new_files1;
    else
    {
	new_files2 = expand_file_archives(new_files1, &nfiles);
	if(new_files2 == NULL)
	{
	    if(new_files1 != files)
	    {
		free(new_files1[0]);
		free(new_files1);
	    }
	    return 0;
	}
    }

    n = 0;
    for(i = 0; i < nfiles; i++)
	n += w32g_add_playlist1(new_files2[i], uniq, refine);

    if(new_files2 != new_files1)
    {
	free(new_files2[0]);
	free(new_files2);
    }
    if(new_files1 != files)
    {
	free(new_files1[0]);
	free(new_files1);
    }

    if(n > 0)
	w32g_update_playlist();
    return n;
}

int w32g_next_playlist(int skip_invalid_file)
{
    while(playlist_play->selected + 1 < playlist_play->nfiles)
    {
	playlist_play->selected++;
	if(!skip_invalid_file ||
	   playlist_play->list[playlist_play->selected].info->file_type != IS_ERROR_FILE)
	{
		if(w32g_is_playlist_ctrl_play())
			w32g_update_playlist();
	    return 1;
	}
    }
    return 0;
}

int w32g_prev_playlist(int skip_invalid_file)
{
    while(playlist_play->selected > 0)
    {
	playlist_play->selected--;
	if(!skip_invalid_file ||
	   playlist_play->list[playlist_play->selected].info->file_type != IS_ERROR_FILE)
	{
		if(w32g_is_playlist_ctrl_play())
			w32g_update_playlist();
	    return 1;
	}
    }
    return 0;
}

int w32g_random_playlist(int skip_invalid_file)
{
	int old_selected_index = playlist_play->selected;
	int select;
	int err = 0;
	for(;;) {
		if ( playlist_play->nfiles == 1) {
			select = old_selected_index;
		} else {
			if ( playlist_play->nfiles <= 1 )
				select = 0;
			else if ( playlist_play->nfiles == 2 )
				select = 1;
			else
				select = int_rand(playlist_play->nfiles - 1);
			select += old_selected_index;
			if ( select >= playlist_play->nfiles )
				select -= playlist_play->nfiles;
			if ( select < 0 )
				select = 0;
		}
		playlist_play->selected = select; 
		if(!skip_invalid_file || playlist_play->list[playlist_play->selected].info->file_type != IS_ERROR_FILE) {
			if(w32g_is_playlist_ctrl_play())
				w32g_update_playlist();
			return 1;
		}
		if ( playlist_play->nfiles == 2 ) {
			playlist_play->selected = old_selected_index; 
			if(!skip_invalid_file || playlist_play->list[playlist_play->selected].info->file_type != IS_ERROR_FILE) {
				if(w32g_is_playlist_ctrl_play())
					w32g_update_playlist();
				return 1;
			}
		}
		// for safety.
		if (playlist_play->selected == old_selected_index)
			break;
		err++;
		if (err > playlist_play->nfiles + 10)
			break;
	}
  return 0;
}

static struct playlist_shuffle_ {
	int * volatile list;
	int volatile cur;
	int volatile allocated;
	int volatile max;
} playlist_shuffle;
static int playlist_shuffle_init = 0;

#define PLAYLIST_SHUFFLE_LIST_SIZE 1024

int w32g_shuffle_playlist_reset(int preserve )
{
	int i;
	int cur_old = -1;
	int max_old = 0;
	int max = playlist_play->nfiles;
	int allocate_min;
	if ( max < 0 ) max = 0;
	if ( playlist_shuffle_init == 0 ){
		playlist_shuffle.list = NULL;
		playlist_shuffle.allocated = 0;
		playlist_shuffle.cur = -1;
		playlist_shuffle.max = 0;
		playlist_shuffle_init = 1;
	}
	if ( preserve ) {
		cur_old = playlist_shuffle.cur;
		max_old = playlist_shuffle.max;
	}
	allocate_min = playlist_shuffle.allocated - PLAYLIST_SHUFFLE_LIST_SIZE;
	if ( allocate_min < 0 ) allocate_min = 0;
	if ( playlist_shuffle.list == NULL || max < allocate_min || playlist_shuffle.allocated < max ) {
		playlist_shuffle.allocated = (max/PLAYLIST_SHUFFLE_LIST_SIZE + 1) * PLAYLIST_SHUFFLE_LIST_SIZE;
		playlist_shuffle.list = (int *) realloc ( playlist_shuffle.list, (playlist_shuffle.allocated + 1) * sizeof(int) );
		if ( playlist_shuffle.list == NULL ) {
			playlist_shuffle_init = 0;
			playlist_shuffle.cur = -1;
			playlist_shuffle.max = 0;
			return 0;
		}
	}
	for ( i = max_old; i < max; i ++ ){
		playlist_shuffle.list[i] = i;
	}
	playlist_shuffle.list[max] = -1;
	playlist_shuffle.cur = cur_old;
	playlist_shuffle.max = max;
	return 1;
}

int w32g_shuffle_playlist_next(int skip_invalid_file)
{
	if ( !playlist_shuffle_init ) {
		if ( !w32g_shuffle_playlist_reset(0) )
			return 0;
	}
	for ( playlist_shuffle.cur ++ ; playlist_shuffle.cur < playlist_shuffle.max; playlist_shuffle.cur ++ ) {
		int n = int_rand(playlist_shuffle.max - playlist_shuffle.cur) + playlist_shuffle.cur;
		int temp = playlist_shuffle.list[playlist_shuffle.cur];
		if ( n > playlist_shuffle.max ) n = playlist_shuffle.max;
		playlist_shuffle.list[playlist_shuffle.cur] = playlist_shuffle.list[n];
		playlist_shuffle.list[n] = temp;
		if ( playlist_shuffle.list[playlist_shuffle.cur] < playlist_play->nfiles ) {
			playlist_play->selected = playlist_shuffle.list[playlist_shuffle.cur];
			if(!skip_invalid_file ||
				playlist_play->list[playlist_play->selected].info->file_type != IS_ERROR_FILE) {
				w32g_update_playlist();
				return 1;
			}
		}
	}
    return 0;
}

// void w32g_rotate_playlist(int dest) 用
static int w32g_shuffle_playlist_rotate(int dest, int i1, int i2)
{
    int i, save;
	
	if ( i2 >= playlist_shuffle.max )
		i2 = playlist_shuffle.max - 1;
    if(i1 >= i2)
		return 1;
	
    if(dest > 0) {
		save = playlist_shuffle.list[i2];
		for(i = i2; i > i1; i--) /* i: i2 -> i1 */
			playlist_shuffle.list[i] = playlist_shuffle.list[i - 1];
		playlist_shuffle.list[i] = save;
		
	} else {
		save = playlist_shuffle.list[i1];
		for(i = i1; i < i2; i++) /* i: i1 -> i2 */
			playlist_shuffle.list[i] = playlist_shuffle.list[i + 1];
		playlist_shuffle.list[i] = save;
    }
	return 0;
}

// int w32g_delete_playlist(int pos) 用
static int w32g_shuffle_playlist_delete(int n)
{
	int i;
	int delete_flag = 0;
	for ( i = 0; i < playlist_shuffle.max; i++ ) {
		if ( playlist_shuffle.list[i] == n ) {
			delete_flag = 1;
			break;
		}
	}
	for ( ; i < playlist_shuffle.max; i++ ) {
		playlist_shuffle.list[i-1] = playlist_shuffle.list[i];
	}
	for ( i = 0; i < playlist_shuffle.max; i++ ) {
		if ( playlist_shuffle.list[i] >= n )
			playlist_shuffle.list[i]--;
	}
	if ( delete_flag )
		playlist_shuffle.max--;
	return 0;
}

void w32g_first_playlist(int skip_invalid_file)
{
    playlist_ctrl->selected = 0;
    if(skip_invalid_file)
    {
	while(playlist_ctrl->selected < playlist_ctrl->nfiles &&
	      playlist_ctrl->list[playlist_ctrl->selected].info->file_type == IS_ERROR_FILE)
	    playlist_ctrl->selected++;
	if(playlist_ctrl->selected == playlist_ctrl->nfiles)
	    playlist_ctrl->selected = 0;
    }
    w32g_update_playlist();
}

int w32g_goto_playlist(int num, int skip_invalid_file)
{
    if(0 <= num && num < playlist_ctrl->nfiles)
    {
	playlist_ctrl->selected = num;
	if(skip_invalid_file)
	{
	    while(playlist_ctrl->selected < playlist_ctrl->nfiles &&
		  playlist_ctrl->list[playlist_ctrl->selected].info->file_type == IS_ERROR_FILE)
		playlist_ctrl->selected++;
	    if(playlist_ctrl->selected == playlist_ctrl->nfiles)
		playlist_ctrl->selected = num;
	}
	w32g_update_playlist();
	return 1;
    }
    return 0;
}

int w32g_isempty_playlist(void)
{
    return playlist_ctrl->nfiles == 0;
}

#if 0
char *w32g_curr_playlist(void)
{
    if(!playlist_ctrl->nfiles)
	return NULL;
    return playlist_ctrl->list[playlist_ctrl->selected].filepath;
}
#endif

///r
// Update an only list at the position.
void w32g_update_playlist_pos(int pos)
{
#ifdef LISTVIEW_PLAYLIST
	

// LVITEM volatile がないとReleaseビルドでpszTextがバグる

    int i, cur, modified;
    HWND hList;

    if(!(hList = playlist_box()))
	return;
	
    cur = ListView_GetNextItem(hList, -1, LVNI_FOCUSED);
    modified = 0;
	i = pos;
	if(i >= 0 && i < playlist_ctrl->nfiles)
    {
		PlayListEntry *entry = &playlist_ctrl->list[i];
		w32g_get_midi_file_info_post(entry);
		{
			volatile LVITEM lvi0;		
			lvi0.iItem = i;
			lvi0.iSubItem = 0;
			lvi0.mask = LVIF_TEXT;
			if(entry->title == NULL || entry->title[0] == '\0'){
				if(entry->info->file_type == IS_ERROR_FILE)
					lvi0.pszText = " --SKIP-- ";
				else
					lvi0.pszText = entry->filename;
			}else
				lvi0.pszText = entry->title;
			if(! ListView_SetItem(hList, (LPARAM)&lvi0))
				ListView_InsertItem(hList, (LPARAM)&lvi0);
		}
		{
			volatile LVITEM lvi0;	
			lvi0.iItem = i;
			lvi0.iSubItem = 0;	
			lvi0.mask = LVIF_IMAGE;		
			if(i == playlist_ctrl->selected && w32g_is_playlist_ctrl_play()){
				lvi0.iImage = 0; // playing icon
			}else{
				lvi0.iImage = -1; // no icon	
			}
			ListView_SetItem(hList, (LPARAM)&lvi0);
		}
		ListView_SetItemText(hList, i, 1, entry->artist ? entry->artist : TEXT("----"));
		ListView_SetItemText(hList, i, 2, entry->duration ? entry->duration : TEXT("--:--:--"));
		ListView_SetItemText(hList, i, 3, entry->system ? entry->system : TEXT("----"));
		ListView_SetItemText(hList, i, 4, entry->filetype ? entry->filetype : TEXT("----"));
		ListView_SetItemText(hList, i, 5, entry->filepath ? entry->filepath : TEXT("----"));
    }
    if(cur==pos) {
		if(cur < 0)
			cur = playlist_ctrl->selected;
		else if(cur >= playlist_ctrl->nfiles - 1)
			cur = playlist_ctrl->nfiles - 1;
		ListView_SetItemState(hList, cur, LVIS_FOCUSED, LVIS_FOCUSED);
		SetNumListWnd(cur,playlist_ctrl->nfiles);
    }

#else
    int i, cur, modified;
    HWND hList;

    if(!(hList = playlist_box()))
	return;
	
    cur = ListBox_GetCurSel(hList);
    modified = 0;
	i = pos;
	if(i >= 0 && i < playlist_ctrl->nfiles)
    {
	char *filename, *title, *item1, *item2;
	int maxlen, item2_len;
	int notitle = 0;

	filename = playlist_ctrl->list[i].filepath;
	title = playlist_ctrl->list[i].title;
	if(title == NULL || title[0] == '\0')
	{
	    if(playlist_ctrl->list[i].info->file_type == IS_ERROR_FILE)
		title = " --SKIP-- ";
		else
		{
//		title = " -------- ";
		title = playlist_ctrl->list[i].filename;
		notitle = 1;
		}
	}
	maxlen = strlen(filename) + strlen(title) + 32 + 80;
	item1 = (char *)new_segment(&tmpbuffer, maxlen);
	if(!notitle)
	{
	if(i == playlist_ctrl->selected)
	    snprintf(item1, maxlen, "==>%-80s   ==>(%s)", title, filename);
	else
	    snprintf(item1, maxlen, "   %-80s      (%s)", title, filename);
	} else
	{
	if(i == playlist_ctrl->selected)
	    snprintf(item1, maxlen, "==>%-80s   ==>(%s)", title, filename);
	else
	    snprintf(item1, maxlen, "   %-80s      (%s)", title, filename);
	}
	item2_len = ListBox_GetTextLen(hList, i); // 前回空プレイリスト対策
	if(item2_len > 0){
		item2 = (char *)new_segment(&tmpbuffer, item2_len + 1);
		ListBox_GetText(hList, i, item2);
	}else{
		item2 = (char *)new_segment(&tmpbuffer, 1);
		item2[0] = '\0';
	}
	if(strcmp(item1, item2) != 0){
	    ListBox_DeleteString(hList, i);
	    ListBox_InsertString(hList, i, item1);
	    modified = 1;
	}
	reuse_mblock(&tmpbuffer);
    }	
    if(modified && cur==pos)
    {
	if(cur < 0)
	    cur = playlist_ctrl->selected;
	else if(cur >= playlist_ctrl->nfiles - 1)
	    cur = playlist_ctrl->nfiles - 1;
	ListBox_SetCurSel(hList, cur);
	SetNumListWnd(cur,playlist_ctrl->nfiles);
    }
#endif
}

void w32g_update_playlist(void)
{
#if 0
    int i, cur, modified;
    HWND hList;

    if(!(hList = playlist_box()))
	return;

    cur = ListBox_GetCurSel(hList);
    modified = 0;
    for(i = 0; i < playlist_ctrl->nfiles; i++)
    {
	char *filename, *title, *item1, *item2;
	int maxlen, item2_len;

	filename = playlist_ctrl->list[i].filepath;
	title = playlist_ctrl->list[i].title;
	if(title == NULL || title[0] == '\0')
	{
	    if(playlist_ctrl->list[i].info->file_type == IS_ERROR_FILE)
		title = " --SKIP-- ";
	    else
		title = " -------- ";
	}
	maxlen = strlen(filename) + strlen(title) + 32;
	item1 = (char *)new_segment(&tmpbuffer, maxlen);
	if(i == playlist_ctrl->selected)
	    snprintf(item1, maxlen, "==>%04d %s (%s)", i + 1, title, filename);
	else
	    snprintf(item1, maxlen, "   %04d %s (%s)", i + 1, title, filename);
	item2_len = ListBox_GetTextLen(hList, i);
	item2 = (char *)new_segment(&tmpbuffer, item2_len + 1);
	ListBox_GetText(hList, i, item2);
	if(strcmp(item1, item2) != 0)
	{
	    ListBox_DeleteString(hList, i);
	    ListBox_InsertString(hList, i, item1);
	    modified = 1;
	}
	reuse_mblock(&tmpbuffer);
    }

    if(modified)
    {
	if(cur < 0)
	    cur = playlist_ctrl->selected;
	else if(cur >= playlist_ctrl->nfiles - 1)
	    cur = playlist_ctrl->nfiles - 1;
	ListBox_SetCurSel(hList, cur);
	SetNumListWnd(cur,playlist_ctrl->nfiles);
    }
#else
    int i, cur, modified;
    HWND hList;

    if(!(hList = playlist_box()))
	return;	
	SendMessage(hList, WM_SETREDRAW , 0, 0);

#ifdef LISTVIEW_PLAYLIST
    cur = ListView_GetNextItem(hList, -1, LVNI_FOCUSED);
#else
    cur = ListBox_GetCurSel(hList);
#endif
    modified = 0;
	if(playlist_reset)
#ifdef LISTVIEW_PLAYLIST
		ListView_DeleteAllItems(hList);
#else
		ListBox_ResetContent(hList);
#endif
	playlist_reset = 0;
    for(i = 0; i < playlist_ctrl->nfiles; i++)
    {
		w32g_update_playlist_pos(i);
    }
//	if(modified)
//	{
	if(cur < 0)
		cur = playlist_ctrl->selected;
	else if(cur >= playlist_ctrl->nfiles - 1)
		cur = playlist_ctrl->nfiles - 1;
#ifdef LISTVIEW_PLAYLIST
	ListView_SetItemState(hList, cur, LVIS_FOCUSED, LVIS_FOCUSED);
#else
	ListBox_SetCurSel(hList, cur);
#endif
	SetNumListWnd(cur,playlist_ctrl->nfiles);
//	}
	SendMessage(hList, WM_SETREDRAW, 1, 0);

#endif
}

void w32g_get_playlist_index(int *selected, int *nfiles, int *cursel)
{
    if(selected != NULL)
	*selected = playlist_ctrl->selected;
    if(nfiles != NULL)
	*nfiles = playlist_ctrl->nfiles;
    if(cursel != NULL)
    {
	HWND hList;
	hList = playlist_box();
	if(hList)
#ifdef LISTVIEW_PLAYLIST		
		*cursel = ListView_GetNextItem(hList, -1, LVNI_FOCUSED);
#else
	    *cursel = ListBox_GetCurSel(hList);
#endif
	else
	    *cursel = 0;
    }
}

void w32g_get_playlist_play_index(int *selected)
{
    if(selected != NULL)
	*selected = playlist_play->selected;
}

#ifdef LISTVIEW_PLAYLIST
static int w32g_delete_playlist_multi(void)
{
    int i, num, pos = 0, selnum = 0, flg = 0;
    HWND hList;

    if(!(hList = playlist_box()))
		return 0;	
	selnum = ListView_GetSelectedCount(hList);
	if(selnum < 1) // no select 
		selnum = 1;	// for focus
	for(num = 0; num < selnum; num++){
		if(selnum > 1) // multi
			pos = ListView_GetNextItem(hList, -1, LVNI_SELECTED);	
		else // single
			pos = ListView_GetNextItem(hList, -1, LVNI_FOCUSED);
		if(pos < 0)
			return 0;
#ifdef W32G_RANDOM_IS_SHUFFLE
		if(w32g_is_playlist_ctrl_play())
			w32g_shuffle_playlist_delete(pos);
#endif
		if(pos == playlist_ctrl->selected)
			flg++; // delete selected
		ListView_DeleteItem(hList, pos);
		free(playlist_ctrl->list[pos].filepath);
		if(playlist_ctrl->list[pos].duration != NULL) free(playlist_ctrl->list[pos].duration);
		if(playlist_ctrl->list[pos].filetype != NULL) free(playlist_ctrl->list[pos].filetype);
		if(playlist_ctrl->list[pos].system != NULL) free(playlist_ctrl->list[pos].system);	
		playlist_ctrl->nfiles--;	
		for(i = pos; i < playlist_ctrl->nfiles; i++)
			playlist_ctrl->list[i] = playlist_ctrl->list[i + 1];
		if(pos < playlist_ctrl->selected || pos == playlist_ctrl->nfiles) {
			if(--playlist_ctrl->selected < 0)
				playlist_ctrl->selected = 0;
		}
	}
    if(playlist_ctrl->nfiles > 0) {
		Sleep(10);
		w32g_update_playlist();
		if(pos >= playlist_ctrl->nfiles)
			pos = playlist_ctrl->nfiles - 1;
		ListView_SetItemState(hList, pos, LVIS_FOCUSED, LVIS_FOCUSED);
		SetNumListWnd(pos,playlist_ctrl->nfiles);
	}
	return 1 + flg;
}
#endif

int w32g_delete_playlist(int pos)
{
    int i;
    HWND hList;
	
#ifdef LISTVIEW_PLAYLIST
	if(pos == -1) // select multi 
		return w32g_delete_playlist_multi();
#endif
    if(!(hList = playlist_box()))
	return 0;

    if(pos >= playlist_ctrl->nfiles)
	return 0;

#ifdef W32G_RANDOM_IS_SHUFFLE
	if(w32g_is_playlist_ctrl_play())
		w32g_shuffle_playlist_delete(pos);
#endif
#ifdef LISTVIEW_PLAYLIST
	ListView_DeleteItem(hList, pos);
#else
    ListBox_DeleteString(hList, pos);
#endif
    free(playlist_ctrl->list[pos].filepath);
#ifdef LISTVIEW_PLAYLIST
	if(playlist_ctrl->list[pos].duration != NULL) free(playlist_ctrl->list[pos].duration);
	if(playlist_ctrl->list[pos].filetype != NULL) free(playlist_ctrl->list[pos].filetype);
	if(playlist_ctrl->list[pos].system != NULL) free(playlist_ctrl->list[pos].system);
#endif
    playlist_ctrl->nfiles--;
    for(i = pos; i < playlist_ctrl->nfiles; i++)
		playlist_ctrl->list[i] = playlist_ctrl->list[i + 1];
    if(pos < playlist_ctrl->selected || pos == playlist_ctrl->nfiles)
    {
	playlist_ctrl->selected--;
	if(playlist_ctrl->selected < 0){
	    playlist_ctrl->selected = 0;
		SetNumListWnd(playlist_ctrl->selected,playlist_ctrl->nfiles);
	} else
		w32g_update_playlist_pos(playlist_ctrl->selected);
    }

    if(playlist_ctrl->nfiles > 0)
    {
	if(pos == playlist_ctrl->nfiles)
	    pos--;
#ifdef LISTVIEW_PLAYLIST
	ListView_SetItemState(hList, pos, LVIS_FOCUSED, LVIS_FOCUSED);
#else
	ListBox_SetCurSel(hList, pos);
#endif
	SetNumListWnd(pos,playlist_ctrl->nfiles);
    }
    return 1;
}
int w32g_ismidi_playlist(int n)
{
    if(n < 0 || n >= playlist_play->nfiles)
	return 0;
    return playlist_play->list[n].info->format >= 0;
}

int w32g_nvalid_playlist(void)
{
    int i, n;

    n = 0;
    for(i = 0; i < playlist_play->nfiles; i++)
	if(w32g_ismidi_playlist(i))
	    n++;
    return n;
}

void w32g_setcur_playlist(void)
{
    HWND hList;
    if(!(hList = playlist_box()))
	return;
#ifdef LISTVIEW_PLAYLIST
	ListView_SetItemState(hList, playlist_ctrl->selected, LVIS_FOCUSED, LVIS_FOCUSED);
#else
    ListBox_SetCurSel(hList, playlist_ctrl->selected);
#endif
	SetNumListWnd(playlist_ctrl->selected,playlist_ctrl->nfiles);
}

int w32g_uniq_playlist(int *is_selected_removed)
{
    int nremoved;
    int i, n, j1, j2, cursel;
    HWND hList;

    hList = playlist_box();
    if(hList)
#ifdef LISTVIEW_PLAYLIST		
		cursel = ListView_GetNextItem(hList, -1, LVNI_FOCUSED);
#else
		cursel = ListBox_GetCurSel(hList);
#endif
    else
	cursel = -1;

    if(is_selected_removed != NULL)
	*is_selected_removed = 0;
    nremoved = 0;
    n = playlist_ctrl->nfiles;
    for(i = 0; i < n - 1; i++)
    {
	int save_n;

	/* remove list[i] from list[i+1 .. n-1] */
	j1 = j2 = i + 1;
	save_n = n;
	while(j2 < save_n) /* j1 <= j2 */
	{
	    if(pathcmp(playlist_ctrl->list[i].filepath,
		       playlist_ctrl->list[j2].filepath, 0) == 0)
	    {
		nremoved++;
		n--;
		free(playlist_ctrl->list[j2].filepath);
#ifdef LISTVIEW_PLAYLIST
		if(playlist_ctrl->list[j2].duration != NULL) free(playlist_ctrl->list[j2].duration);
		if(playlist_ctrl->list[j2].filetype != NULL) free(playlist_ctrl->list[j2].filetype);
		if(playlist_ctrl->list[j2].system != NULL) free(playlist_ctrl->list[j2].system);
#endif
		if(j2 == playlist_ctrl->selected &&
		   is_selected_removed != NULL &&
		   !*is_selected_removed)
		{
		    *is_selected_removed = 1;
		    playlist_ctrl->selected = j1;
		}
		if(j2 < playlist_ctrl->selected)
		    playlist_ctrl->selected--;
		if(j2 < cursel)
		    cursel--;
	    }
	    else
	    {
		playlist_ctrl->list[j1] = playlist_ctrl->list[j2];
		j1++;
	    }
	    j2++;
	}
    }
    if(nremoved)
    {
	for(i = 0; i < nremoved; i++)
#ifdef LISTVIEW_PLAYLIST
	    ListView_DeleteItem(hList, --playlist_ctrl->nfiles);
#else
	    ListBox_DeleteString(hList, --playlist_ctrl->nfiles);
#endif
	if(cursel >= 0){
#ifdef LISTVIEW_PLAYLIST
		ListView_SetItemState(hList, cursel, LVIS_FOCUSED, LVIS_FOCUSED);
#else
	    ListBox_SetCurSel(hList, cursel);
#endif
		SetNumListWnd(cursel,playlist_ctrl->nfiles);
	}
	w32g_update_playlist();
    }
    return nremoved;
}

int w32g_refine_playlist(int *is_selected_removed)
{
    int nremoved;
    int i, j1, j2, cursel;
    HWND hList;

    hList = playlist_box();
    if(hList)
#ifdef LISTVIEW_PLAYLIST		
		cursel = ListView_GetNextItem(hList, -1, LVNI_FOCUSED);
#else
		cursel = ListBox_GetCurSel(hList);
#endif
    else
	cursel = -1;

    if(is_selected_removed != NULL)
	*is_selected_removed = 0;
    nremoved = 0;
    j1 = j2 = 0;
    while(j2 < playlist_ctrl->nfiles) /* j1 <= j2 */
    {
	if(playlist_ctrl->list[j2].info->format < 0)
	{
	    nremoved++;
	    free(playlist_ctrl->list[j2].filepath);
#ifdef LISTVIEW_PLAYLIST
		if(playlist_ctrl->list[j2].duration != NULL) free(playlist_ctrl->list[j2].duration);
		if(playlist_ctrl->list[j2].filetype != NULL) free(playlist_ctrl->list[j2].filetype);
		if(playlist_ctrl->list[j2].system != NULL) free(playlist_ctrl->list[j2].system);
#endif
		if(j2 == playlist_ctrl->selected &&
		   is_selected_removed != NULL &&
		   !*is_selected_removed)
		{
		    *is_selected_removed = 1;
		    playlist_ctrl->selected = j1;
		}
		if(j2 < playlist_ctrl->selected)
		    playlist_ctrl->selected--;
		if(j2 < cursel)
		    cursel--;
	}
	else
	{
	    playlist_ctrl->list[j1] = playlist_ctrl->list[j2];
	    j1++;
	}
	j2++;
    }
    if(nremoved)
    {
	for(i = 0; i < nremoved; i++)
#ifdef LISTVIEW_PLAYLIST
	    ListView_DeleteItem(hList, --playlist_ctrl->nfiles);
#else
	    ListBox_DeleteString(hList, --playlist_ctrl->nfiles);
#endif
	if(cursel >= playlist_ctrl->nfiles)
	    cursel = playlist_ctrl->nfiles - 1;
	if(cursel >= 0){
#ifdef LISTVIEW_PLAYLIST
		ListView_SetItemState(hList, cursel, LVIS_FOCUSED, LVIS_FOCUSED);
#else
	    ListBox_SetCurSel(hList, cursel);
#endif
		SetNumListWnd(cursel,playlist_ctrl->nfiles);
	}
	w32g_update_playlist();
    }
    return nremoved;
}

void w32g_clear_playlist(void)
{
    HWND hList;

    hList = playlist_box();
    while(playlist_ctrl->nfiles > 0)
    {
	playlist_ctrl->nfiles--;
	free(playlist_ctrl->list[playlist_ctrl->nfiles].filepath);
#ifdef LISTVIEW_PLAYLIST
	if(playlist_ctrl->list[playlist_ctrl->nfiles].duration != NULL) free(playlist_ctrl->list[playlist_ctrl->nfiles].duration);
	if(playlist_ctrl->list[playlist_ctrl->nfiles].filetype != NULL) free(playlist_ctrl->list[playlist_ctrl->nfiles].filetype);
	if(playlist_ctrl->list[playlist_ctrl->nfiles].system != NULL) free(playlist_ctrl->list[playlist_ctrl->nfiles].system);
#endif
#if 0
	if(hList)
	    ListBox_DeleteString(hList, playlist_ctrl->nfiles);
#endif
    }
//	LB_RESETCONTENT
	if(hList)
#ifdef LISTVIEW_PLAYLIST
		ListView_DeleteAllItems(hList);
#else
	    ListBox_ResetContent(hList);
#endif
	playlist_ctrl->selected = 0;
	SetNumListWnd(0,0);
}

void w32g_rotate_playlist(int dest)
{
    int i, i1, i2;
    HWND hList;
    PlayListEntry save;
#ifdef LISTVIEW_PLAYLIST
#else
	char temp[1024];
#endif

    if(playlist_ctrl->nfiles == 0)
	return;
    if(!(hList = playlist_box()))
	return;
	
#ifdef LISTVIEW_PLAYLIST
	i1 = ListView_GetNextItem(hList, -1, LVNI_FOCUSED);
#else
    i1 = ListBox_GetCurSel(hList);
#endif
    i2 = playlist_ctrl->nfiles - 1;
    if(i1 >= i2)
	return;

#ifdef W32G_RANDOM_IS_SHUFFLE
	if(w32g_is_playlist_ctrl_play())
		w32g_shuffle_playlist_rotate(dest,i1,i2);
#endif
    if(dest > 0)
    {
	save = playlist_ctrl->list[i2];
	for(i = i2; i > i1; i--) /* i: i2 -> i1 */
	    playlist_ctrl->list[i] = playlist_ctrl->list[i - 1];
	playlist_ctrl->list[i] = save;
#ifdef LISTVIEW_PLAYLIST
	ListView_DeleteItem(hList,i2);
	for(i = i1; i < playlist_ctrl->nfiles; i++)
		w32g_update_playlist_pos(i);
	ListView_SetItemState(hList, i1, LVIS_FOCUSED, LVIS_FOCUSED);
#else
	ListBox_GetText(hList,i2,temp);
    ListBox_DeleteString(hList,i2);
    ListBox_InsertString(hList,i1,temp);
	ListBox_SetCurSel(hList,i1);
#endif
	if(playlist_ctrl->selected == i2){
	    playlist_ctrl->selected = i1;
		w32g_update_playlist_pos(playlist_ctrl->selected);
	} else if(i1 <= playlist_ctrl->selected && playlist_ctrl->selected < i2){
	    playlist_ctrl->selected++;
		w32g_update_playlist_pos(playlist_ctrl->selected);
	}
    }
    else
    {
	save = playlist_ctrl->list[i1];
	for(i = i1; i < i2; i++) /* i: i1 -> i2 */
	    playlist_ctrl->list[i] = playlist_ctrl->list[i + 1];
	playlist_ctrl->list[i] = save;
#ifdef LISTVIEW_PLAYLIST
	ListView_DeleteItem(hList,i1);
	w32g_update_playlist_pos(playlist_ctrl->nfiles - 1);
	ListView_SetItemState(hList, i1, LVIS_FOCUSED, LVIS_FOCUSED);
#else
	ListBox_GetText(hList,i1,temp);
    ListBox_DeleteString(hList,i1);
    ListBox_InsertString(hList,-1,temp);
	ListBox_SetCurSel(hList,i1);
#endif
	if(playlist_ctrl->selected == i1){
	    playlist_ctrl->selected = i2;
		w32g_update_playlist_pos(playlist_ctrl->selected);
	} else if(i1 < playlist_ctrl->selected && playlist_ctrl->selected <= i2){
	    playlist_ctrl->selected--;    
		w32g_update_playlist_pos(playlist_ctrl->selected);
	}
    }
}

char *w32g_get_playlist(int idx)
{
    if(idx < 0 || idx >= playlist_ctrl->nfiles)
	return NULL;
    return playlist_ctrl->list[idx].filepath;
}

char *w32g_get_playlist_play(int idx)
{
    if(idx < 0 || idx >= playlist_play->nfiles)
	return NULL;
    return playlist_play->list[idx].filepath;
}

#ifdef LISTVIEW_PLAYLIST
void w32g_copy_playlist(void)
{
    int i, num, pos, selnum = 0;
    HWND hList;
	PlayListEntry *entry;

    if(!(hList = playlist_box()))
		return;		
	// clear tmp_playlist
	for(i = 0; i < tmp_playlist.nfiles; i++){
		entry = &tmp_playlist.list[i];
		if(entry->filepath != NULL) {
			free(entry->filepath);
			entry->filepath = NULL;
		}
#ifdef LISTVIEW_PLAYLIST
		if(entry->duration != NULL) {
			free(entry->duration);
			entry->duration = NULL;
		}
		if(entry->filetype != NULL) {
			free(entry->filetype);
			entry->filetype = NULL;
		}
		if(entry->system != NULL) {
			free(entry->system);
			entry->system = NULL;
		}
#endif
	}
	if(tmp_playlist.list != NULL) {
		free(tmp_playlist.list);
		tmp_playlist.list = NULL;
	}
	tmp_playlist.allocated = 0;
	tmp_playlist.nfiles = 0;
	tmp_playlist.selected = 0;	
	// get select
	selnum = ListView_GetSelectedCount(hList);
	if(selnum < 1)
		return;	
	tmp_playlist.nfiles = selnum;
	// alloc list    
	tmp_playlist.allocated = tmp_playlist.nfiles;
	tmp_playlist.list = (PlayListEntry *)safe_malloc(tmp_playlist.allocated * sizeof(PlayListEntry));
	// copy to tmp_playlist
	pos = -1;
	for(num = 0; num < selnum; num++){
		pos = ListView_GetNextItem(hList, pos, LVNI_SELECTED);
		if(playlist_ctrl->list[pos].filepath != NULL) 
			tmp_playlist.list[num].filepath = safe_strdup(playlist_ctrl->list[pos].filepath);
		else
			tmp_playlist.list[num].filepath = NULL;
		if(playlist_ctrl->list[pos].duration != NULL)
			tmp_playlist.list[num].duration = safe_strdup(playlist_ctrl->list[pos].duration);
		else
			tmp_playlist.list[num].duration = NULL;
		if(playlist_ctrl->list[pos].filetype != NULL)
			tmp_playlist.list[num].filetype = safe_strdup(playlist_ctrl->list[pos].filetype);
		else
			tmp_playlist.list[num].filetype = NULL;
		if(playlist_ctrl->list[pos].system != NULL)
			tmp_playlist.list[num].system = safe_strdup(playlist_ctrl->list[pos].system);
		else
			tmp_playlist.list[num].system = NULL;
		tmp_playlist.list[num].filename = NULL;
		tmp_playlist.list[num].title = NULL;
		tmp_playlist.list[num].artist = NULL;
		tmp_playlist.list[num].info = playlist_ctrl->list[pos].info;
	}
}

void w32g_paste_playlist(int uniq, int refine)
{
    int i, num, pos, select = 0, skip = 0;
    HWND hList;

    PlayListEntry *entry;
    struct midi_file_info *info;
	
    if(!(hList = playlist_box()))
		return;	
	
	select = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
	if(select < 0 || select >= playlist_ctrl->nfiles)
		select = playlist_ctrl->nfiles;	
	for(num = 0; num < tmp_playlist.nfiles ; num++){
		pos = select + num;		
		if(uniq) {
			skip = 0;
			for(i = 0; i < playlist_ctrl->nfiles; i++)
				if(pathcmp(tmp_playlist.list[num].filepath , playlist_ctrl->list[i].filepath, 0) == 0){
					skip = 1;
					break;
				}
		}
		if(skip) continue;
		info = tmp_playlist.list[num].info;
		if(refine && info->format < 0)
			continue;
		if(playlist_ctrl->allocated == 0) {
			playlist_ctrl->allocated = 32;
			playlist_ctrl->list = (PlayListEntry *)safe_malloc(playlist_ctrl->allocated * sizeof(PlayListEntry));
		}else if(playlist_ctrl->nfiles == playlist_ctrl->allocated) {
			playlist_ctrl->allocated *= 2;
			playlist_ctrl->list = (PlayListEntry *)safe_realloc(playlist_ctrl->list,playlist_ctrl->allocated * sizeof(PlayListEntry));
		}
		// insert 1個分スペース作る
		if(playlist_ctrl->nfiles && pos < playlist_ctrl->nfiles)
			for(i = playlist_ctrl->nfiles - 1; i >= pos; i--)
				playlist_ctrl->list[i + 1] = playlist_ctrl->list[i];
		// paste tmp_playlist
		entry = &playlist_ctrl->list[pos];
		entry->filepath = safe_strdup(tmp_playlist.list[num].filepath);
		entry->filename = get_filename(entry->filepath);
		entry->title = info->seq_name;
		entry->artist = info->artist_name;
		if(tmp_playlist.list[num].duration != NULL)	
			entry->duration = safe_strdup(tmp_playlist.list[num].duration);
		else
			entry->duration = NULL;
		if(tmp_playlist.list[num].filetype != NULL)	
			entry->filetype = safe_strdup(tmp_playlist.list[num].filetype);
		else
			entry->filetype = NULL;
		if(tmp_playlist.list[num].system != NULL)	
			entry->system = safe_strdup(tmp_playlist.list[num].system);
		else
			entry->system = NULL;
		entry->info = info;
		playlist_ctrl->nfiles++;
	}	
	w32g_shuffle_playlist_reset(1);
	Sleep(10);
    if(playlist_ctrl->nfiles > 0)
		w32g_update_playlist();
    return;
}
#endif




void w32g_free_playlist(void)
{
	PlayListEntry *entry;
	int i, j;
	
	for(j=0; j < PLAYLIST_MAX; j++){
		for(i=0; i < playlist[j].nfiles; i++){
			entry = &playlist[j].list[i];
			if(entry->filepath != NULL) free(entry->filepath);
#ifdef LISTVIEW_PLAYLIST
			if(entry->duration != NULL) free(entry->duration);
			if(entry->filetype != NULL) free(entry->filetype);
			if(entry->system != NULL) free(entry->system);
#endif
		}
		if(playlist[j].list != NULL) free(playlist[j].list);
	}
	if(playlist_shuffle.list != NULL) free(playlist_shuffle.list);
	
#ifdef LISTVIEW_PLAYLIST
	// clear tmp_playlist
	for(i = 0; i < tmp_playlist.nfiles; i++){
		entry = &tmp_playlist.list[i];
		if(entry->filepath != NULL) free(entry->filepath);
		if(entry->duration != NULL) free(entry->duration);
		if(entry->filetype != NULL) free(entry->filetype);
		if(entry->system != NULL) free(entry->system);
	}
	if(tmp_playlist.list != NULL) free(tmp_playlist.list);
#endif
}

