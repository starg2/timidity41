/*

    TiMidity -- Experimental MIDI to WAVE converter
    Copyright (C) 1999 Masanao Izumo <mo@goice.co.jp>
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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <windowsx.h>
#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "controls.h"
#include "w32g.h"
#include "w32g_res.h"

// playlist
typedef struct _PlayListEntry {
    char *filename;	// malloc
    char *title;	// shared with midi_file_info
    struct midi_file_info *info;
} PlayListEntry;
static struct {
    int nfiles;
    int selected; /* 0..nfiles-1 */
    int allocated; /* number of PlayListEntry is allocated */
    PlayListEntry *list;
} playlist = {0, 0, 0, NULL};

static HWND playlist_box(void)
{
    extern HWND hListWnd;
    if(!hListWnd)
	return 0;
    return GetDlgItem(hListWnd, IDC_LISTBOX_PLAYLIST);
}

static void w32g_set_playlist(PlayListEntry *entry, char *filename)
{
    entry->filename = safe_strdup(filename);
    entry->title = get_midi_title(filename);
    entry->info = get_midi_file_info(filename, 1);
}

static void w32g_add_playlist1(char *filename)
{
    if(playlist.allocated == 0)
    {
	playlist.allocated = 32;
	playlist.list = (PlayListEntry *)safe_malloc(playlist.allocated *
						     sizeof(PlayListEntry));
    }
    else if(playlist.nfiles == playlist.allocated)
    {
	playlist.allocated *= 2;
	playlist.list = (PlayListEntry *)safe_realloc(playlist.list,
						      playlist.allocated *
						      sizeof(PlayListEntry));
    }
    w32g_set_playlist(&playlist.list[playlist.nfiles++], filename);
}

void w32g_add_playlist(int nfiles, char **files, int expand_flag)
{
    char **new_files;
    int i, new_nfiles;

    if(nfiles == 0)
	return;
    new_nfiles = nfiles;
    if(!expand_flag)
	new_files = files;
    else
    {
	new_files = expand_file_archives(files, &new_nfiles);
	if(new_nfiles == 0)
	    return;
    }

    for(i = 0; i < new_nfiles; i++)
	w32g_add_playlist1(new_files[i]);

    if(new_files != files)
    {
	free(new_files[0]);
	free(new_files);
    }

    w32g_update_playlist();
}

int w32g_next_playlist(void)
{
    while(playlist.selected + 1 < playlist.nfiles)
    {
	playlist.selected++;
	if(playlist.list[playlist.selected].info->file_type != IS_ERROR_FILE)
	{
	    w32g_update_playlist();
	    return 1;
	}
    }
    return 0;
}

int w32g_prev_playlist(void)
{
    while(playlist.selected > 0)
    {
	playlist.selected--;
	if(playlist.list[playlist.selected].info->file_type != IS_ERROR_FILE)
	{
	    w32g_update_playlist();
	    return 1;
	}
    }
    return 0;
}

void w32g_first_playlist(void)
{
    playlist.selected = 0;
    while(playlist.selected < playlist.nfiles &&
	  playlist.list[playlist.selected].info->file_type == IS_ERROR_FILE)
	playlist.selected++;
    if(playlist.selected == playlist.nfiles)
	playlist.selected = 0;
    w32g_update_playlist();
}

int w32g_goto_playlist(int num)
{
    if(0 <= num && num < playlist.nfiles)
    {
	playlist.selected = num;
	while(playlist.selected < playlist.nfiles &&
	      playlist.list[playlist.selected].info->file_type == IS_ERROR_FILE)
	    playlist.selected++;
	if(playlist.selected == playlist.nfiles)
	    playlist.selected = num;
	w32g_update_playlist();
	return 1;
    }
    return 0;
}

int w32g_isempty_playlist(void)
{
    return playlist.nfiles == 0;
}

char *w32g_curr_playlist(void)
{
    if(!playlist.nfiles)
	return NULL;
    return playlist.list[playlist.selected].filename;
}

void w32g_update_playlist(void)
{
    int i, top_index, cur;
    HWND hListBox;

    hListBox = playlist_box();
    if(!hListBox)
	return;

    top_index = ListBox_GetTopIndex(hListBox);
    if(top_index != 0)
	ListBox_SetTopIndex(hListBox, 0);

    cur = ListBox_GetCurSel(hListBox);
    for(i = 0; i < playlist.nfiles; i++)
    {
	char *filename, *title, *item1, *item2;
	int maxlen, item2_len;

	filename = playlist.list[i].filename;
	title = playlist.list[i].title;
	if(title == NULL || title[0] == '\0')
	{
	    if(playlist.list[i].info->file_type == IS_ERROR_FILE)
		title = " --SKIP-- ";
	    else
		title = " -------- ";
	}
	maxlen = strlen(filename) + strlen(title) + 32;
	item1 = (char *)new_segment(&tmpbuffer, maxlen);
	if(i == playlist.selected)
	    snprintf(item1, maxlen, "==>%04d %s (%s)", i + 1, title, filename);
	else
	    snprintf(item1, maxlen, "    %04d %s (%s)", i + 1, title, filename);
	item2_len = ListBox_GetTextLen(hListBox, i);
	item2 = (char *)new_segment(&tmpbuffer, item2_len + 1);
	ListBox_GetText(hListBox, i, item2);
	if(strcmp(item1, item2) != 0)
	{
	    ListBox_DeleteString(hListBox, i);
	    ListBox_InsertString(hListBox, i, item1);
	}
	reuse_mblock(&tmpbuffer);
    }

    if(cur < 0)
	cur = 0;
    else if(cur >= playlist.nfiles - 1)
	cur = playlist.nfiles - 1;
    ListBox_SetCurSel(hListBox, cur);
}

void w32g_get_playlist_index(int *selected, int *nfiles, int *cursel)
{
    if(selected != NULL)
	*selected = playlist.selected;
    if(nfiles != NULL)
	*nfiles = playlist.nfiles;
    if(cursel != NULL)
    {
	HWND hListBox;
	hListBox = playlist_box();
	if(hListBox)
	    *cursel = ListBox_GetCurSel(hListBox);
	else
	    *cursel = 0;
    }
}

void w32g_delete_playlist(int pos)
{
    int i;
    HWND hListBox;

    hListBox = playlist_box();
    if(!hListBox)
	return;

    ListBox_DeleteString(hListBox, pos);
    free(playlist.list[pos].filename);
    playlist.nfiles--;
    for(i = pos; i < playlist.nfiles; i++)
	playlist.list[i] = playlist.list[i + 1];
    if(pos < playlist.selected || pos == playlist.nfiles)
    {
	playlist.selected--;
	if(playlist.selected < 0)
	    playlist.selected = 0;
    }
    w32g_update_playlist();
    if(playlist.nfiles > 0)
    {
	if(pos == playlist.nfiles)
	    pos--;
	ListBox_SetCurSel(hListBox, pos);
    }
}

int w32g_valid_playlist(void)
{
    int i, n;

    n = 0;
    for(i = 0; i < playlist.nfiles; i++)
	if(playlist.list[i].info->file_type != IS_ERROR_FILE)
	    n++;
    return n;
}

void w32g_setcur_playlist(void)
{
    HWND hListBox;
    hListBox = playlist_box();
    if(!hListBox)
	return;
    ListBox_SetCurSel(hListBox, playlist.selected);
}
