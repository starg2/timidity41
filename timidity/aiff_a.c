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
static int write_u32(int fd, uint32 value);
static int write_u16(int fd, uint16 value);
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

static int write_u32(int fd, uint32 value)
{
    value = BE_LONG(value);
    return write(fd, (char *)&value, 4);
}

static int write_u16(int fd, uint16 value)
{
    value = BE_SHORT(value);
    return write(fd, (char *)&value, 2);
}

static int write_ieee_80bitfloat(int fd, double num)
{
    char bytes[10];
    ConvertToIeeeExtended(num, bytes);
    return write(fd, bytes, 10);
}

static int chunk_start(int fd, char *id, uint32 chunk_len)
{
    int i, j;

    i = write(fd, id, 4);
    if(i < 4)
	return i;

    j = write_u32(fd, chunk_len);
    if(j == -1)
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
		      dpm.name, sys_errlist[errno]);
	    return -1;
	}
    }

    /* magic */
    write(dpm.fd, "FORM", 4);

    /* file size - 8 (dmy) */
    write_u32(dpm.fd, (uint32)0xffffffff);

    /* chunk start tag */
    write(dpm.fd, "AIFF", (int32)4);

    /* COMM chunk */
    chunk_start(dpm.fd, "COMM", 18);

    /* number of channels */
    if(dpm.encoding & PE_MONO)
	write_u16(dpm.fd, (uint16)1);
    else
	write_u16(dpm.fd, (uint16)2);

    /* number of frames (dmy) */
    write_u32(dpm.fd, (uint32)0xffffffff);

    /* bits per sample */
    if(dpm.encoding & PE_16BIT)
	write_u16(dpm.fd, (uint16)16);
    else
	write_u16(dpm.fd, (uint16)8);

    /* sample rate */
    write_ieee_80bitfloat(dpm.fd, (double)dpm.rate);

    /* SSND chunk */
    chunk_start(dpm.fd, "SSND", (int32)0xffffffff);

    /* offset */
    write_u32(dpm.fd, (uint32)0);

    /* block size */
    write_u32(dpm.fd, (uint32)0);

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
	    write_u32(dpm.fd, (uint32)(4		/* "AIFF" */
				       + 26		/* COMM chunk */
				       + 16 + bytes_output/* SSND chunk */
				       ));
	    /* COMM chunk */
	    /* number of frames */
	    lseek(dpm.fd, 12+10, SEEK_SET);
	    f = bytes_output;
	    if(!(dpm.encoding & PE_MONO))
		f /= 2;
	    if(dpm.encoding & PE_16BIT)
		f /= 2;
	    write_u32(dpm.fd, f);

	    /* SSND chunk */
	    lseek(dpm.fd, 12+26+4, SEEK_SET);
	    write_u32(dpm.fd, (uint32)(8 + bytes_output));
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
