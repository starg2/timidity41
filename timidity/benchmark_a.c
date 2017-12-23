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

    raw_audio.c

    Functions to output raw sound data to a file or stdout.

*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>

#ifdef __POCC__
#include <sys/types.h>
#endif //for off_t

#ifdef __W32__
#include <stdlib.h>
#include <io.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef STDC_HEADERS
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#elif HAVE_STRINGS_H
#include <strings.h>
#endif

#include <fcntl.h>

#ifdef __FreeBSD__
#include <stdio.h>
#endif

#include "timidity.h"
#include "common.h"
#include "output.h"
#include "controls.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "timer.h"

static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static int output_data(const uint8 *buf, size_t bytes);
static int acntl(int request, void *arg);

/* export the playback mode */

#define dpm benchmark_mode

PlayMode dpm = {
    DEFAULT_RATE, PE_16BIT|PE_SIGNED, PF_PCM_STREAM|PF_FILE_OUTPUT,
    -1,
    {0,0,0,0,0},
    "Benchmark", 'b',
    NULL,
    open_output,
    close_output,
    output_data,
    acntl
};

static double counter = 0.0, time_elapsed = 0.0;
static int first = 0;

/*************************************************************************/

static int silent_output_open(void)
{
	dpm.flag |= PF_AUTO_SPLIT_FILE;
	dpm.fd = 1;
	first = 0;
	return 0;
}

static int open_output(void)
{
	silent_output_open();
	return 0;
}

static int output_data(const uint8 *buf, size_t bytes)
{
	if(!bytes)
		return bytes;
	if(first){
		time_elapsed = get_current_calender_time() - counter;
		return bytes;
	}
	first = 1;
	counter = get_current_calender_time();
	time_elapsed = 0.0;
	ctl->cmsg(CMSG_INFO, VERB_NORMAL,
		"%s: First output", dpm.id_name);
	return bytes;
}

static void close_output(void)
{
    if (dpm.fd != -1) {
	ctl->cmsg(CMSG_INFO, VERB_NORMAL,
		  "%s: Time elapsed : %.2f sec", dpm.id_name,
		  (double)(time_elapsed));
    }
    dpm.fd = -1;
	first = 0;
}

static int acntl(int request, void *arg)
{
  switch(request) {
  case PM_REQ_PLAY_START:
    if (dpm.flag & PF_AUTO_SPLIT_FILE) {
      return silent_output_open();
    }
    return 0;
  case PM_REQ_PLAY_END:
    if(dpm.flag & PF_AUTO_SPLIT_FILE)
     close_output();
    return 0;
  case PM_REQ_DISCARD:
    return 0;
  }
  return -1;
}
