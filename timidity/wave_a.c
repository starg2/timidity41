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

    wave_audio.c

    Functions to output RIFF WAVE format data to a file or stdout.

*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#ifdef __WIN32__
#include <stdlib.h>
#include <io.h>
#else
#include <unistd.h>
#endif
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <fcntl.h>

#ifdef __FreeBSD__
#include <stdio.h>
#endif

#include "timidity.h"
#include "output.h"
#include "controls.h"

#ifdef __WIN32__
#define OPEN_MODE O_WRONLY | O_CREAT | O_TRUNC | O_BINARY
#else
#define OPEN_MODE O_WRONLY | O_CREAT | O_TRUNC
#endif

static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static void output_data(int32 *buf, int32 count);
static int flush_output(void);
static void purge_output(void);
static int32 current_samples(void);
static int play_loop(void);

extern int default_play_event(void *);

/* export the playback mode */

#define dpm wave_play_mode

PlayMode dpm = {
  DEFAULT_RATE, PE_16BIT|PE_SIGNED, PF_NEED_INSTRUMENTS,
  -1,
  {0,0,0,0,0},
  "RIFF WAVE file", 'w',
  "output.wav",
  default_play_event,
  open_output,
  close_output,
  output_data,
  flush_output,
  purge_output,
  current_samples,
  play_loop
};

/*************************************************************************/

static char *orig_RIFFheader=
  "RIFF" "\377\377\377\377"
  "WAVE" "fmt " "\020\000\000\000" "\001\000"
  /* 22: channels */ "\001\000"
  /* 24: frequency */ "xxxx"
  /* 28: bytes/second */ "xxxx"
  /* 32: bytes/sample */ "\004\000"
  /* 34: bits/sample */ "\020\000"
  "data" "\377\377\377\377"
;

/* Count the number of bytes output so the header can be fixed when
   closing the file */
static int32 bytes_output;

/* We only support 16-bit signed and 8-bit unsigned data -- WAVEs have
   to be supported because TiMidity is a "MIDI-to-WAVE converter"...

   uLaw WAVEs might be useful and not too hard to implement. I just
   don't know what should go in the "fmt " block. */

static int open_output(void)
{
  char RIFFheader[44];
  int t;

  dpm.encoding &= ~PE_BYTESWAP;
  if (dpm.encoding & (PE_ULAW|PE_ALAW))
      dpm.encoding &= ~PE_16BIT;

  if (dpm.encoding & PE_16BIT)
      dpm.encoding |= PE_SIGNED;
  else
      dpm.encoding &= ~PE_SIGNED;

  if (dpm.name && dpm.name[0]=='-' && dpm.name[1]=='\0')
    dpm.fd=1; /* data to stdout */
  else
    {
      /* Open the audio file */
#ifdef __MACOS__
      dpm.fd=open(dpm.name, OPEN_MODE);
#else
      dpm.fd=open(dpm.name, OPEN_MODE, 0644);
#endif
      if (dpm.fd<0)
	{
	  ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s",
	       dpm.name, strerror(errno));
	  return -1;
	}
    }

  /* Generate a (rather non-standard) RIFF header. We don't know yet
     what the block lengths will be. We'll fix that at close if this
     is a seekable file. */

  memcpy(RIFFheader, orig_RIFFheader, 44);

  if(dpm.encoding & PE_ALAW)
      RIFFheader[20]='\006';
  else if(dpm.encoding & PE_ULAW)
      RIFFheader[20]='\007';

  RIFFheader[32]='\001';
  if (!(dpm.encoding & PE_MONO)) RIFFheader[22]='\002';

  *((int *)(RIFFheader+24))=LE_LONG(dpm.rate);

  t=dpm.rate;
  if (!(dpm.encoding & PE_MONO)) t*=2;
  if (dpm.encoding & PE_16BIT) t*=2;
  *((int *)(RIFFheader+28))=LE_LONG(t);

  /* Bug fixed from Masaaki Koyanagi <koya@k2.t.u-tokyo.ac.jp> */
  if ((dpm.encoding & (PE_MONO | PE_16BIT)) == PE_MONO)
    RIFFheader[32]='\001';
  else if (!(dpm.encoding & PE_MONO) && (dpm.encoding & PE_16BIT))
    RIFFheader[32]='\004';
  else 
    RIFFheader[32]='\002';

  if (!(dpm.encoding & PE_16BIT)) RIFFheader[34]='\010';

  if(write(dpm.fd, RIFFheader, 44) == -1)
  {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: write: %s",
		dpm.name, strerror(errno));
      close(dpm.fd);
      dpm.fd = -1;
      return -1;
  }

  /* Reset the length counter */
  bytes_output=0;

  return 0;
}

static void output_data(int32 *buf, int32 count)
{
  if (!(dpm.encoding & PE_MONO)) count*=2; /* Stereo samples */

  if (dpm.encoding & PE_16BIT)
    {
      s32tos16l(buf, count); /* Little-endian data */

      while ((-1==write(dpm.fd, buf, count * 2)) && errno==EINTR)
	;
      bytes_output += count*2;
    }
  else
  {
      if(dpm.encoding & PE_ULAW)
	  s32toulaw(buf, count);
      else if(dpm.encoding & PE_ALAW)
	  s32toalaw(buf, count);
      else
	  s32tou8(buf, count);

      while ((-1==write(dpm.fd, buf, count)) && errno==EINTR)
	;
      bytes_output += count;
    }
}

static void close_output(void)
{
  if (dpm.fd != 1) /* We don't close stdout */
    {
      /* It's not stdout, so it's probably a file, and we can try
         fixing the block lengths in the header before closing. */
      if (lseek(dpm.fd, 4, SEEK_SET)>=0)
	{
	  int32 tmp;
	  tmp=LE_LONG(bytes_output + 44 - 8);
	  write(dpm.fd, &tmp, 4);
	  lseek(dpm.fd, 40, SEEK_SET);
	  tmp=LE_LONG(bytes_output);
	  write(dpm.fd, &tmp, 4);
	}
      close(dpm.fd);
    }
  dpm.fd = -1;
}

/* Dummies */
static int flush_output(void) { return RC_NONE; }
static void purge_output(void) { }

static int play_loop(void)
{
    return 0;
}

static int32 current_samples(void)
{
    return -1;
}
