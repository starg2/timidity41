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

    aiff_a.c

    Functions to output AIFF audio file (*.aiff).
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
#include <math.h>

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
static int write_u32(uint32 value);
static int write_u16(uint16 value);
static void ConvertToIeeeExtended(double num, char *bytes);

extern int default_play_event(void *);

/* export the playback mode */

#define dpm aiff_play_mode

PlayMode dpm = {
    44100, PE_SIGNED|PE_16BIT, PF_NEED_INSTRUMENTS,
    -1,
    {0,0,0,0,0},
    "AIFF file", 'a',
    "output.aiff",
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

/* Count the number of bytes output so the header can be fixed when
   closing the file */
static uint32 bytes_output;
static int32  play_counter;

static int write_u32(uint32 value)
{
    int n;
    value = BE_LONG(value);
    if((n = write(dpm.fd, (char *)&value, 4)) == -1)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: write: %s",
		  dpm.name, strerror(errno));
	close(dpm.fd);
	dpm.fd = -1;
	return -1;
    }
    return n;
}

static int write_u16(uint16 value)
{
    int n;
    value = BE_SHORT(value);
    if((n = write(dpm.fd, (char *)&value, 2)) == -1)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: write: %s",
		  dpm.name, strerror(errno));
	close(dpm.fd);
	dpm.fd = -1;
	return -1;
    }
    return n;
}

static int write_str(char *s)
{
    int n;
    if((n = write(dpm.fd, s, strlen(s))) == -1)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: write: %s",
		  dpm.name, strerror(errno));
	close(dpm.fd);
	dpm.fd = -1;
	return -1;
    }
    return n;
}

static int write_ieee_80bitfloat(double num)
{
    char bytes[10];
    int n;
    ConvertToIeeeExtended(num, bytes);

    if((n = write(dpm.fd, bytes, 10)) == -1)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: write: %s",
		  dpm.name, strerror(errno));
	close(dpm.fd);
	dpm.fd = -1;
	return -1;
    }
    return n;
}

static int chunk_start(char *id, uint32 chunk_len)
{
    int i, j;

    if((i = write_str(id)) == -1)
	return -1;
    if((j = write_u32(chunk_len)) == -1)
	return -1;
    return i + j;
}

static int open_output(void)
{
    dpm.encoding &= ~(PE_BYTESWAP|PE_ULAW|PE_ALAW);
    dpm.encoding |= PE_SIGNED;

    bytes_output = 0;

    if(dpm.name && dpm.name[0] == '-' && dpm.name[1] == '\0')
	dpm.fd=1; /* data to stdout */
    else
    {
	/* Open the audio file */
#ifdef __MACOS__
	dpm.fd = open(dpm.name, OPEN_MODE);
#else
	dpm.fd = open(dpm.name, OPEN_MODE, 0644);
#endif
	if(dpm.fd < 0)
	{
	    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s",
		      dpm.name, strerror(errno));
	    return -1;
	}
    }

    /* magic */
    if(write_str("FORM") == -1) return -1;

    /* file size - 8 (dmy) */
    if(write_u32((uint32)0xffffffff) == -1) return -1;

    /* chunk start tag */
    if(write_str("AIFF") == -1) return -1;

    /* COMM chunk */
    if(chunk_start("COMM", 18) == -1) return -1;

    /* number of channels */
    if(dpm.encoding & PE_MONO)
    {
	if(write_u16((uint16)1) == -1) return -1;
    }
    else
    {
	if(write_u16((uint16)2) == -1) return -1;
    }

    /* number of frames (dmy) */
    if(write_u32((uint32)0xffffffff) == -1) return -1;

    /* bits per sample */
    if(dpm.encoding & PE_16BIT)
    {
	if(write_u16((uint16)16) == -1) return -1;
    }
    else
    {
	if(write_u16((uint16)8) == -1) return -1;
    }

    /* sample rate */
    if(write_ieee_80bitfloat((double)dpm.rate) == -1) return -1;

    /* SSND chunk */
    if(chunk_start("SSND", (int32)0xffffffff) == -1) return -1;

    /* offset */
    if(write_u32((uint32)0) == -1) return -1;

    /* block size */
    if(write_u32((uint32)0) == -1) return -1;

    play_counter = 0;
    return 0;
}

static void output_data(int32 *buf, int32 count)
{
    play_counter += count;
    if(!(dpm.encoding & PE_MONO))
	count*=2; /* Stereo samples */

    if(dpm.encoding & PE_16BIT)
    {
	s32tos16b(buf, count); /* Big-endian data */
	while((-1==write(dpm.fd, (char *)buf, count * 2)) && errno==EINTR)
	    ;
	bytes_output += count * 2;
    }
    else
    {
	s32tos8(buf, count);
	while((-1==write(dpm.fd, (char *)buf, count)) && errno==EINTR)
	    ;
	bytes_output += count;
    }
}

static void close_output(void)
{
    if(dpm.fd != 1) /* We don't close stdout */
    {
	/* It's not stdout, so it's probably a file, and we can try
	   fixing the block lengths in the header before closing. */
	if(lseek(dpm.fd, 4, SEEK_SET) >= 0)
	{
	    uint32 f;

	    /* file size - 8 */
	    if(write_u32((uint32)(4		/* "AIFF" */
				  + 26		/* COMM chunk */
				  + 16 + bytes_output/* SSND chunk */
				  )) == -1) return;

	    /* COMM chunk */
	    /* number of frames */
	    lseek(dpm.fd, 12+10, SEEK_SET);
	    f = bytes_output;
	    if(!(dpm.encoding & PE_MONO))
		f /= 2;
	    if(dpm.encoding & PE_16BIT)
		f /= 2;
	    if(write_u32(f) == -1) return;

	    /* SSND chunk */
	    lseek(dpm.fd, 12+26+4, SEEK_SET);
	    if(write_u32((uint32)(8 + bytes_output)) == -1) return;
	}
	close(dpm.fd);
    }
    dpm.fd = -1;
}

/* Dummies */
static int flush_output(void)	{ return RC_NONE; }
static void purge_output(void)	{ }
static int play_loop(void)	{ return 0; }
static int32 current_samples(void) { return play_counter; }

/*
 * C O N V E R T   T O   I E E E   E X T E N D E D
 */

/* Copyright (C) 1988-1991 Apple Computer, Inc.
 * All rights reserved.
 *
 * Machine-independent I/O routines for IEEE floating-point numbers.
 *
 * NaN's and infinities are converted to HUGE_VAL or HUGE, which
 * happens to be infinity on IEEE machines.  Unfortunately, it is
 * impossible to preserve NaN's in a machine-independent way.
 * Infinities are, however, preserved on IEEE machines.
 *
 * These routines have been tested on the following machines:
 *    Apple Macintosh, MPW 3.1 C compiler
 *    Apple Macintosh, THINK C compiler
 *    Silicon Graphics IRIS, MIPS compiler
 *    Cray X/MP and Y/MP
 *    Digital Equipment VAX
 *
 *
 * Implemented by Malcolm Slaney and Ken Turkowski.
 *
 * Malcolm Slaney contributions during 1988-1990 include big- and little-
 * endian file I/O, conversion to and from Motorola's extended 80-bit
 * floating-point format, and conversions to and from IEEE single-
 * precision floating-point format.
 *
 * In 1991, Ken Turkowski implemented the conversions to and from
 * IEEE double-precision format, added more precision to the extended
 * conversions, and accommodated conversions involving +/- infinity,
 * NaN's, and denormalized numbers.
 */

#ifndef HUGE_VAL
# define HUGE_VAL HUGE
#endif /*HUGE_VAL*/

# define FloatToUnsigned(f)      ((unsigned long)(((long)(f - 2147483648.0)) + 2147483647L) + 1)

static void ConvertToIeeeExtended(double num, char *bytes)
{
    int    sign;
    int expon;
    double fMant, fsMant;
    unsigned long hiMant, loMant;

    if (num < 0) {
        sign = 0x8000;
        num *= -1;
    } else {
        sign = 0;
    }

    if (num == 0) {
        expon = 0; hiMant = 0; loMant = 0;
    }
    else {
        fMant = frexp(num, &expon);
        if ((expon > 16384) || !(fMant < 1)) {    /* Infinity or NaN */
            expon = sign|0x7FFF; hiMant = 0; loMant = 0; /* infinity */
        }
        else {    /* Finite */
            expon += 16382;
            if (expon < 0) {    /* denormalized */
                fMant = ldexp(fMant, expon);
                expon = 0;
            }
            expon |= sign;
            fMant = ldexp(fMant, 32);
            fsMant = floor(fMant);
            hiMant = FloatToUnsigned(fsMant);
            fMant = ldexp(fMant - fsMant, 32);
            fsMant = floor(fMant);
            loMant = FloatToUnsigned(fsMant);
        }
    }

    bytes[0] = expon >> 8;
    bytes[1] = expon;
    bytes[2] = (char)(hiMant >> 24);
    bytes[3] = (char)(hiMant >> 16);
    bytes[4] = (char)(hiMant >> 8);
    bytes[5] = (char)hiMant;
    bytes[6] = (char)(loMant >> 24);
    bytes[7] = (char)(loMant >> 16);
    bytes[8] = (char)(loMant >> 8);
    bytes[9] = (char)loMant;
}
