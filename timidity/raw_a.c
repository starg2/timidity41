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

#define dpm raw_play_mode

PlayMode dpm = {
    DEFAULT_RATE, PE_16BIT|PE_SIGNED, PF_PCM_STREAM|PF_FILE_OUTPUT,
    -1,
    {0,0,0,0,0},
    "Raw waveform data", 'r',
    NULL,
    open_output,
    close_output,
    output_data,
    acntl
};

static double counter = 0.0;

/*************************************************************************/

/*
 * Get the filename extention from the encoding
 * This extension is available for sox.
 */
static const char *encoding_ext(int32 encoding) {
  static char ext[5], *p;

  if(encoding & PE_ULAW) {
    return ".ul";
  }
  if(encoding & PE_ALAW) {
    return ".al"; /* ?? */
  }

  p = ext;
  *p++ = '.';
  if(encoding & PE_SIGNED)
    *p++ = 's';
  else
    *p++ = 'u';
  if(encoding & PE_16BIT)
    *p++ = 'w';
  else if(encoding & PE_24BIT)
    *p++ = '2', *p++ = '4'; /* is there any common extension? */
///r ?
  else if(encoding & PE_32BIT)
    *p++ = '3', *p++ = '2'; /* is there any common extension? */
  else if(encoding & PE_F32BIT)
    *p++ = '3', *p++ = '2'; /* is there any common extension? */
  else if(encoding & PE_64BIT)
    *p++ = '6', *p++ = '4'; /* is there any common extension? */
  else if(encoding & PE_F64BIT)
    *p++ = '6', *p++ = '4'; /* is there any common extension? */
  else
    *p++ = 'b';
  *p = '\0';
  return ext;
}

static int raw_output_open(const char *fname)
{
  int fd;

  counter = get_current_calender_time();

  if(strcmp(fname, "-") == 0)
    return 1; /* data to stdout */
#ifdef __W32__
  TCHAR *t = char_to_tchar(fname);
  fd = _topen(t, FILE_OUTPUT_MODE);
  safe_free(t);
#else
  fd = open(fname, FILE_OUTPUT_MODE);
#endif
  if(fd < 0)
    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s",
	      fname, strerror(errno));
  return fd;
}

static int auto_raw_output_open(const char *input_filename)
{
  char *output_filename = create_auto_output_name(input_filename, encoding_ext(dpm.encoding), NULL, 0);

  if (!output_filename) {
    return -1;
  }

  if((dpm.fd = raw_output_open(output_filename)) == -1) {
    free(output_filename);
    return -1;
  }
  if(dpm.name != NULL)
    free(dpm.name);
  dpm.name = output_filename;
  ctl->cmsg(CMSG_INFO, VERB_NORMAL, "Output %s", dpm.name);
  return 0;
}

static int open_output(void)
{
  dpm.encoding = validate_encoding(dpm.encoding, 0, 0);

  if(dpm.name == NULL) {
    dpm.flag |= PF_AUTO_SPLIT_FILE;
  } else {
    dpm.flag &= ~PF_AUTO_SPLIT_FILE;
    if((dpm.fd = raw_output_open(dpm.name)) == -1)
      return -1;
  }

  return 0;
}

static int output_data(const uint8 *buf, size_t bytes)
{
    int n;

    if(dpm.fd == -1)
      return -1;

    while(((n = std_write(dpm.fd, buf, bytes)) == -1) && errno == EINTR)
	;
    if(n == -1)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s",
		  dpm.name, strerror(errno));
	return -1;
    }
    return n;
}

static void close_output(void)
{
    if (dpm.fd != -1) {
	ctl->cmsg(CMSG_INFO, VERB_DEBUG,
		  "%s: Finished output (real time=%.2f)", dpm.id_name,
		  (double)(get_current_calender_time() - counter));
    }
    if(dpm.fd != 1 && dpm.fd != -1) /* We don't close stdout */
	close(dpm.fd);
    dpm.fd = -1;
}

static int acntl(int request, void *arg)
{
  switch(request) {
  case PM_REQ_PLAY_START:
    if (dpm.flag & PF_AUTO_SPLIT_FILE) {
      const char *filename = (current_file_info && current_file_info->filename) ?
			     current_file_info->filename : "Output.mid";
      return auto_raw_output_open(filename);
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
