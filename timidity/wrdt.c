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

*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "timidity.h"
#include "common.h"
#include "wrd.h"
#include "strtab.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"

/*
 * Remap WRD @COLOR(16)-@COLOR(23) to RGB plain number.
 * Usage: rgb = wrd_color_remap[color-16];
 *
 * R G B
 * 1 2 4
 */
int wrd_color_remap[8] =
{
    0, /* 16 secret?, I don't know this code meaning. */
    1, /* 17 red */
    4, /* 18 blue */
    5, /* 19 magenta */
    2, /* 20 green */
    3, /* 21 yellow */
    6, /* 22 cyan */
    7  /* 23 white */
};

/* Map RGB plane from MIMPI plane.
 *
 * {0,1,2,3,4,5,6,7} -- No conversion
 * {0,4,1,5,2,6,3,7} -- BRG to RGB
 * {0,1,4,5,2,3,6,7} -- GRB to RGB
 */
int wrd_plane_remap[8] = {0,1,2,3,4,5,6,7};

extern WRDTracer dumb_wrdt_mode;

#ifdef __MACOS__
extern WRDTracer mac_wrdt_mode;
#endif

/*ARGSUSED*/
static int null_wrdt_open(char *wrdt_opts) { return 0; }
/*ARGSUSED*/
static void null_wrdt_apply(int cmd, int argc, int args[]) { }
static void null_wrdt_update_events(void) { }
static void null_wrdt_end(void) { }
static void null_wrdt_close(void) { }

WRDTracer null_wrdt_mode =
{
    "No WRD trace", '-',
    0,
    null_wrdt_open,
    null_wrdt_apply,
    null_wrdt_update_events,
    NULL,
    null_wrdt_end,
    null_wrdt_close
};
extern WRDTracer tty_wrdt_mode;


#ifdef WRDT_X
extern WRDTracer x_wrdt_mode;
#endif /* WRDT_X */

#ifdef __WIN32__
extern WRDTracer wincon_wrdt_mode;
#endif /* __WIN32__ */

WRDTracer *wrdt_list[] =
{
#ifdef WRDT_X
    &x_wrdt_mode,
#endif /* WRDT_X */
#ifdef __WIN32__
	&wincon_wrdt_mode,
#endif /* __WIN32__ */
#ifndef __MACOS__
    &tty_wrdt_mode,
#endif /* __MACOS__ */
#ifdef __MACOS__
    &mac_wrdt_mode,
#endif
    &dumb_wrdt_mode,
    &null_wrdt_mode,
    0
};

WRDTracer *wrdt = &null_wrdt_mode;


static StringTable path_list;
static StringTable default_path_list;

void wrd_init_path(void)
{
    StringTableNode *p;
    delete_string_table(&path_list);
    for(p = default_path_list.head; p; p = p->next)
	wrd_add_path(p->string, 0);
    if(current_file_info && strchr(current_file_info->filename, '#') != NULL)
	wrd_add_path(current_file_info->filename,
		     strchr(current_file_info->filename, '#') -
		     current_file_info->filename + 1);
    if(current_file_info &&
       strrchr(current_file_info->filename, PATH_SEP) != NULL)
	wrd_add_path(current_file_info->filename,
		     strrchr(current_file_info->filename, PATH_SEP) -
		     current_file_info->filename + 1);
}

void wrd_add_path(char *path, int pathlen)
{
    if(pathlen == 0)
	pathlen = strlen(path);
    if(pathlen > 0)
    {
	int exists;
	StringTableNode *p;

	exists = 0;
	for(p = path_list.head; p; p = p->next)
	    if(strncmp(p->string, path, pathlen) == 0)
	    {
		exists = 1;
		break;
	    }
	if(!exists)
	{
	    put_string_table(&path_list, path, pathlen);
	    if(current_file_info &&
	       strchr(current_file_info->filename, '#') != NULL &&
	       path[pathlen - 1] != '#')
	    {
		MBlockList buf;
		char *arc_path;
		int baselen;

		init_mblock(&buf);
		baselen = strchr(current_file_info->filename, '#') -
		    current_file_info->filename + 1;
		arc_path = new_segment(&buf, baselen + pathlen + 1);
		strncpy(arc_path, current_file_info->filename, baselen);
		strncpy(arc_path + baselen, path, pathlen);
		arc_path[baselen + pathlen] = '\0';
		put_string_table(&path_list, arc_path, strlen(arc_path));
		reuse_mblock(&buf);
	    }
	}
    }
}

void wrd_add_default_path(char *path)
{
    put_string_table(&default_path_list, path, strlen(path));
}

static struct timidity_file *try_wrd_open_file(char *prefix, char *fn)
{
    MBlockList buf;
    char *path;
    int len1, len2;
    struct timidity_file *tf;

    init_mblock(&buf);
    len1 = strlen(prefix);
    len2 = strlen(fn);
    path = (char *)new_segment(&buf, len1 + len2 + 2);
    strcpy(path, prefix);
    if( len1>0 && path[len1 - 1] != '#' && path[len1 - 1] != PATH_SEP)
    {
	path[len1++] = PATH_SEP;
	path[len1] = '\0';
    }
    strcat(path, fn);
    tf = open_file(path, 0, OF_SILENT);
    reuse_mblock(&buf);
    return tf;
}

#define CUR_DIR_PATH ""

struct timidity_file *wrd_open_file(char *filename)
{
    StringTableNode *path;
    struct timidity_file *tf;
    for(path = path_list.head; path; path = path->next){
	if((tf = try_wrd_open_file(path->string, filename)) != NULL)
	    return tf;
    }
    return try_wrd_open_file(CUR_DIR_PATH, filename);
}

void wrd_midi_event(int cmd, int arg)
{
    static int wrd_argc = 0;
    static int wrd_args[WRD_MAXPARAM];

    if(!wrdt->opened) /* Ignore any WRD command if WRD is closed */
	return;

    if(cmd == -1)
    {
	wrd_argc = 0;
	return;
    }

    wrd_args[wrd_argc++] = arg;
    if(cmd != WRD_ARG)
    {
	wrdt->apply(cmd, wrd_argc, wrd_args);
	wrd_argc = 0;
    }
}
