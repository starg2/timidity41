/*
    TiMidity -- MIDI to WAVE converter and player
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

    w32g_c.c: written by Daisuke Aoki <dai@y7.net>
                         Masanao Izumo <mo@goice.co.jp>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "timidity.h"
#include "common.h"
#include "output.h"
#include "instrum.h"
#include "playmidi.h"
#include "controls.h"
#include "miditrace.h"
#include "strtab.h"
#include "aq.h"


#include "w32g.h"
#include <process.h>

int w32g_play_active;
int w32g_current_volume[MAX_CHANNELS];
int w32g_current_expression[MAX_CHANNELS];
static int mark_apply_setting = 0;



//****************************************************************************/
// Control funcitons

static int ctl_open(int using_stdin, int using_stdout);
static void ctl_close(void);
static void ctl_pass_playing_list(int number_of_files, char *list_of_files[]);
static void ctl_event(CtlEvent *e);
static int ctl_read(int32 *valp);
static int cmsg(int type, int verbosity_level, char *fmt, ...);

#define ctl w32gui_control_mode

ControlMode ctl=
{
    "Win32 GUI interface", 'w',
    1,0,0,
    CTLF_AUTOSTART | CTLF_LIST_LOOP,
    ctl_open,
    ctl_close,
    ctl_pass_playing_list,
    ctl_read,
    cmsg,
    ctl_event
};

static int ctl_open(int using_stdin, int using_stdout)
{
    ctl.opened = 1;
    return w32g_open();
}

static void ctl_close(void)
{
    if(ctl.opened)
    {
	w32g_close();
	ctl.opened = 0;
    }
}


static int ctl_drop_file(HDROP hDrop)
{
    StringTable st;
    int i, n, len;
    char buffer[BUFSIZ];
    char **files;

    init_string_table(&st);
    n = DragQueryFile(hDrop,0xffffffffL, NULL, 0);
    for(i = 0; i < n; i++)
    {
	DragQueryFile(hDrop, i, buffer, sizeof(buffer));
	if(is_directory(buffer))
	    directory_form(buffer);
	len = strlen(buffer);
	put_string_table(&st, buffer, strlen(buffer));
    }
    DragFinish(hDrop);

    files = make_string_array(&st);
    if(files != NULL)
    {
	w32g_add_playlist(n, files, 1);
	free(files[0]);
	free(files);
    }
    w32g_update_playlist();
    if(n > 0)
	TmPanelUpdateList();
    return RC_NONE;
}

static int ctl_load_file(char *fileptr)
{
    StringTable st;
    int len, n;
    char **files;
    char buffer[BUFSIZ];
    char *basedir;

    for(n = 0; n < 100; n++) {
	if(fileptr[n] == '\0')
	    printf("<NIL>");
	else
	    printf("%c", fileptr[n]);
    }
    printf("\n");fflush(stdout);

    init_string_table(&st);
    n = 0;
    basedir = fileptr;
    fileptr += strlen(fileptr) + 1;
    while(*fileptr)
    {
	snprintf(buffer, sizeof(buffer), "%s\\%s", basedir, fileptr);
	if(is_directory(buffer))
	    directory_form(buffer);
	len = strlen(buffer);
	put_string_table(&st, buffer, len);
	n++;
	fileptr += strlen(fileptr) + 1;
    }

    if(n == 0)
    {
	put_string_table(&st, basedir, strlen(basedir));
	n++;
    }

    files = make_string_array(&st);
    if(files != NULL)
    {
	w32g_add_playlist(n, files, 1);
	free(files[0]);
	free(files);
    }
    w32g_update_playlist();
    if(n > 0)
	TmPanelUpdateList();
    w32g_lock_open_file = 0;
    return RC_NONE;
}

static int ctl_delete_playlist(int offset)
{
    int selected, nfiles, cur, pos;

    w32g_get_playlist_index(&selected, &nfiles, &cur);
    pos = cur + offset;
    if(pos < 0 || pos >= nfiles)
	return RC_NONE;
    w32g_delete_playlist(pos);
    if(w32g_play_active && selected == pos)
	return RC_LOAD_FILE;
    return RC_NONE;
}

static int w32g_ext_control(int rc, int32 value)
{
    switch(rc)
    {
      case RC_EXT_DROP:
	return ctl_drop_file((HDROP)value);
      case RC_EXT_LOAD_FILE:
	return ctl_load_file((char *)value);
      case RC_EXT_MODE_CHANGE:
	return TmCanvasChange();
      case RC_EXT_APPLY_SETTING:
	if(w32g_play_active) {
	    mark_apply_setting = 1;
	    return RC_STOP;
	}
	SettingWndApply();
	mark_apply_setting = 0;
	return RC_NONE;
      case RC_EXT_DELETE_PLAYLIST:
	rc = ctl_delete_playlist(value);
	TmPanelUpdateList();
	return rc;
      case RC_EXT_UPDATE_PLAYLIST:
	w32g_update_playlist();
	return rc;
    }
    return RC_NONE;
}

static int ctl_read(int32 *valp)
{
    int rc;

    rc = w32g_get_rc(valp, 0);
    if(rc >= RC_EXT_BASE)
	return w32g_ext_control(rc, *valp);
    return rc;
}

static int cmsg(int type, int verbosity_level, char *fmt, ...)
{
    char buffer[BUFSIZ];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    if((type==CMSG_TEXT || type==CMSG_INFO || type==CMSG_WARNING) &&
       ctl.verbosity<verbosity_level)
	return 0;
    if(type == CMSG_FATAL)
	w32g_msg_box(buffer, "TiMidity Error", MB_OK);
    PutsConsoleWnd(buffer);
    PutsConsoleWnd("\n");
    return 0;
}

static void ctl_pass_playing_list(int number_of_files, char *list_of_files[])
{
    int rc;
    int32 value;
    extern void timidity_init_aq_buff(void);
    int errcnt;

    w32g_add_playlist(number_of_files, list_of_files, 0);
    w32g_play_active = 0;
    errcnt = 0;

    if(play_mode->fd != -1 &&
       w32g_valid_playlist() && (ctl.flags & CTLF_AUTOSTART))
	rc = RC_LOAD_FILE;
    else
	rc = RC_NONE;

    while(1)
    {
	if(rc == RC_NONE)
	{
	    if(play_mode->fd != -1)
	    {
		aq_flush(1);
		play_mode->close_output();
		TmCanvasReset();
	    }
	    rc = w32g_get_rc(&value, 1);
	}

      redo:
	switch(rc)
	{
	  case RC_NONE:
	    Sleep(1000);
	    break;

	  case RC_LOAD_FILE: /* Play playlist.selected */
	    if(w32g_valid_playlist())
	    {
		w32g_play_active = 1;
		if(play_mode->fd == -1)
		{
		    int err;
		    if(play_mode->open_output() == -1)
		    {
			ctl.cmsg(CMSG_FATAL, VERB_NORMAL,
				 "Couldn't open %s (`%c') %s",
				 play_mode->id_name,
				 play_mode->id_character,
				 play_mode->name ? play_mode->name : "");
			break;
		    }
		    aq_setup();
		    timidity_init_aq_buff();
		}
		w32g_setcur_playlist();
		rc = play_midi_file(w32g_curr_playlist());
		if(rc == RC_ERROR)
		    errcnt++;
		else
		    errcnt = 0;
		if(errcnt == 10)
		{
		    w32g_msg_box("Too many MIDI files are error",
				 "TiMidity Warning", MB_OK);
		    errcnt = 0;
		    break;
		}
		w32g_play_active = 0;
		goto redo;
	    }
	    break;

	  case RC_ERROR:
	  case RC_TUNE_END:
	    if(play_mode->id_character != 'd')
		break;
	    /* FALLTHROUGH */
	  case RC_NEXT:
	    if(!w32g_valid_playlist())
		break;
	    if(w32g_next_playlist())
	    {
		rc = RC_LOAD_FILE;
		goto redo;
	    }
	    else
	    {
		/* end of list */
		if((ctl.flags & CTLF_AUTOEXIT) && w32g_valid_playlist())
		    return;
		if((ctl.flags & CTLF_LIST_LOOP) && w32g_valid_playlist())
		{
		    w32g_first_playlist();
		    rc = RC_LOAD_FILE;
		    goto redo;
		}
	    }
	    break;

	  case RC_REALLY_PREVIOUS:
	    if(w32g_prev_playlist())
	    {
		rc = RC_LOAD_FILE;
		goto redo;
	    }
	    break;

	  case RC_QUIT:
	    return;

	  case RC_CHANGE_VOLUME:
	    amplification += value;
	    TmPanelSetMasterVol(amplification);
	    TmPanelRefresh();
	    break;

	  case RC_TOGGLE_PAUSE:
	    play_pause_flag = !play_pause_flag;

	  default:
	    if(rc >= RC_EXT_BASE)
	    {
		int load;
		if((rc == RC_EXT_DROP || rc == RC_EXT_LOAD_FILE) &&
		   w32g_isempty_playlist() &&
		   (ctl.flags & CTLF_AUTOSTART))
		    load = 1;
		else
		    load = 0;
		w32g_ext_control(rc, value);
		if(load)
		{
		    rc = RC_LOAD_FILE;
		    goto redo;
		}
	    }
	    break;
	}

	if(mark_apply_setting)
	    SettingWndApply();
	rc = RC_NONE;
    }
}

static void ctl_event(CtlEvent *e)
{
    switch(e->type)
    {
      case CTLE_NOW_LOADING:
	TmPanelStartToLoad((char *)e->v1);
	break;
      case CTLE_LOADING_DONE:
	break;
      case CTLE_PLAY_START:
	TmPanelStartToPlay((int)e->v1 / play_mode->rate);
	w32g_ctle_play_start((int)e->v1 / play_mode->rate);
	break;
      case CTLE_PLAY_END:
	MainWndScrollbarProgressUpdate(-1);
	break;
      case CTLE_TEMPO:
	break;
      case CTLE_METRONOME:
	break;
      case CTLE_CURRENT_TIME: {
	  int sec;
	  if(midi_trace.flush_flag)
	      return;
	  if(ctl.trace_playing)
	      sec = (int)e->v1;
	  else
	  {
	      sec = current_trace_samples();
	      if(sec < 0)
		  sec = (int)e->v1;
	      else
		  sec = sec / play_mode->rate;
	  }
	  MainWndScrollbarProgressUpdate(sec);
	  TmPanelSetVoices((int)e->v2);
	  TmPanelSetTime(sec);
	}
	break;
      case CTLE_NOTE:
	TmCanvasNote((int)e->v1, (int)e->v2, (int)e->v3, (int)e->v4);
	break;
      case CTLE_MASTER_VOLUME:
	TmPanelSetMasterVol((int)e->v1);
	if(play_pause_flag)
	    TmPanelRefresh();
	break;
      case CTLE_PROGRAM:
	break;
      case CTLE_VOLUME:
	if(e->v1 < MAX_CHANNELS)
	    w32g_current_volume[e->v1] = e->v2;
	break;
      case CTLE_EXPRESSION:
	if(e->v1 < MAX_CHANNELS)
	    w32g_current_expression[e->v1] = e->v2;
	break;
      case CTLE_PANNING:
	break;
      case CTLE_SUSTAIN:
	break;
      case CTLE_PITCH_BEND:
	break;
      case CTLE_MOD_WHEEL:
	break;
      case CTLE_CHORUS_EFFECT:
	break;
      case CTLE_REVERB_EFFECT:
	break;
      case CTLE_LYRIC:
	default_ctl_lyric((uint16)e->v1);
	break;
      case CTLE_REFRESH:
	TmPanelRefresh();
	TmCanvasRefresh();
	break;
      case CTLE_RESET:
	TmCanvasReset();
	break;
      case CTLE_SPEANA:
	break;
      case CTLE_PAUSE:
	if(w32g_play_active)
	{
	    MainWndScrollbarProgressUpdate((int)e->v2);
	    TmPanelSetTime((int)e->v2);
	    TmPanelRefresh();
	}
    }
}
