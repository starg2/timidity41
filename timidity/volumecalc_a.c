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
    along with this program; nclude <sys/stat.h>if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    volumecalc_a.c
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <math.h>
#ifdef __GNUC__
#include <stdio.h>
#endif /* __GNUC__ */

#ifdef __POCC__
#include <sys/types.h>
#endif //for off_t

#ifdef __W32__
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */
#include <io.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef STDC_HEADERS
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#elif defined(HAVE_STRINGS_H)
#include <strings.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif /* HAVE_FCNTL_H */

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

#ifdef AU_VOLUME_CALC


static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static int32 output_data(const uint8 *buf, size_t bytes);
static int acntl(int request, void *arg);

static void calc_rms(const uint8 *buf, int32 bytes);
static void calc_max(const uint8 *buf, int32 bytes);
static void (*p_calc_func)(const uint8 *buf, int32 bytes) = calc_max;

static FILE *fp;
static double now_max_volume;
static double total_rms;
static off_size_t samples;
static off_size_t trim_front, trim_tail;

extern int opt_volume_calc_rms; /* 0=Maximum Amplitude, 1=RMS Amplitude */
extern int opt_volume_calc_trim; /* 1=Trim silence samples */

/* use PE_F64BIT format */
#define USE_F64BIT

/* export the playback mode */

#define dpm soundfont_vol_calc

PlayMode dpm = {
    DEFAULT_RATE, PE_F64BIT | PE_SIGNED,
    PF_PCM_STREAM | PF_FILE_OUTPUT,
    -1,
    { 0 },
    "Soundfont Volume Calc", 'V',
    NULL,
    open_output,
    close_output,
    output_data,
    acntl
};

int open_output(void)
{
	const char default_output[] = "volume.txt";
	uint32 include_enc, exclude_enc;

	if (fp && fp != stdout) {
		fclose(fp);
		fp = NULL;
	}

	if (!dpm.name) fp = fopen(default_output, "w");
	else if (!strcmp(dpm.name, "")) fp = fopen(default_output, "w");
	else if (!strcmp(dpm.name, "-")) fp = stdout;
	else {
		fp = fopen(dpm.name, "w");
		if ((fp = fopen(dpm.name, "w")) == NULL)
			fp = fopen(default_output, "w");
	}

	include_enc = exclude_enc = 0;

	exclude_enc |= PE_BYTESWAP | PE_24BIT | PE_32BIT | PE_64BIT | PE_F32BIT;

#ifdef USE_F64BIT
	include_enc |= PE_F64BIT | PE_SIGNED;
	exclude_enc |= PE_16BIT;
#else
	include_enc |= PE_16BIT | PE_SIGNED;
	exclude_enc |= PE_F64BIT;
#endif
	dpm.encoding = validate_encoding(dpm.encoding, include_enc, exclude_enc);

	if (!dpm.name) {
		dpm.flag |= PF_AUTO_SPLIT_FILE;
	} else {
		dpm.flag &= ~PF_AUTO_SPLIT_FILE;
	}

	p_calc_func = (opt_volume_calc_rms) ? calc_rms : calc_max;

	now_max_volume = 0.0;
	total_rms = 0.0;
	samples = 0;
	trim_front = trim_tail = 0;

	return 0;
}

static int32 output_data(const uint8 *buf, size_t bytes)
{
	(*p_calc_func)(buf, bytes);
	return 0;
}

static void calc_rms(const uint8 *buf, int32 bytes)
{
	int i;
	double tmp;
#ifdef USE_F64BIT
	const int length = divi_8(bytes);
	const double * const shb = (double*) buf;
#else
	const int length = divi_2(bytes);
	const int16 * const shb = (int16*) buf;
#endif
	static const double silence = 1.0 / 65536;

	for (i = 0; i < length; ++i) {
		/* RMS Amplitude */
#ifdef USE_F64BIT
		tmp = shb[i];
#else
		tmp = (double)(shb[i]) * DIV_15BIT;
#endif
		total_rms += tmp * tmp;
		/* Count silence samples */
		if (tmp < silence && tmp > -silence) {
			++trim_tail;
		}
		else {
			if (trim_front == 0) {
				trim_front = trim_tail;
			}
			trim_tail = 0;
		}
	}
	samples += length;
}

static void calc_max(const uint8 *buf, int32 bytes)
{
	int i;
	double tmp;
#ifdef USE_F64BIT
	const int length = divi_8(bytes);
	const double * const shb = (double*) buf;
#else
	const int length = divi_2(bytes);
	const int16 * const shb = (int16*) buf;
#endif

	for (i = 0; i < length; ++i) {
		/* Maximum Amplitude */
#ifdef USE_F64BIT
		tmp = fabs(shb[i]) * M_15BIT;
#else
		tmp = (double)(abs(shb[i]));
#endif
		if (now_max_volume < tmp) {
			now_max_volume = tmp;
		}
	}
	samples += length;
}

static void close_output(void)
{
	double result;
	off_size_t total_samples = samples;
	const FLOAT_T div_ch = (dpm.encoding & PE_MONO) ? 1.0 : DIV_2;

	if (!fp) return;

	if (opt_volume_calc_trim) {
		/* Trim silence samples */
		total_samples -= trim_front + trim_tail;
	}

	if (opt_volume_calc_rms) {
		/* RMS Amplitude */
		if (total_samples > 0) {
			result = sqrt(total_rms / total_samples);
		}
		else {
			result = 0.0;
		}
		fprintf(fp, "%.6f\n", result); /* RMS Amplitude */
	}
	else {
		/* Maximum Amplitude */
		if (now_max_volume > 0.0) {
			result = 1.0 / now_max_volume;
		}
		else {
			result = 0.0;
		}
		fprintf(fp, "%.3f\n", result); /* Volume adjustment */
	}

	/* Log */
	if (fp != stdout) {
		ctl->cmsg(CMSG_INFO, VERB_NORMAL,
			  "Samples read:     %13d", (int) samples);
		ctl->cmsg(CMSG_INFO, VERB_NORMAL,
			  "Length (seconds): %13.6f", (double) samples * div_ch * div_playmode_rate);

		if (opt_volume_calc_rms) {
			if (opt_volume_calc_trim) {
				/* Trim silence samples */
				ctl->cmsg(CMSG_INFO, VERB_NORMAL,
					  "Silence samples:  %13d", (int) (trim_front + trim_tail));
				ctl->cmsg(CMSG_INFO, VERB_NORMAL,
					  "Valid samples:    %13d", (int) total_samples);
			}
			/* RMS Amplitude */
			ctl->cmsg(CMSG_INFO, VERB_NORMAL,
				  "RMS     amplitude:%13.6f", result);
		}
		else {
			/* Maximum Amplitude */
			ctl->cmsg(CMSG_INFO, VERB_NORMAL,
				  "Maximum Amplitude:%13.6f",
				  (result > 0.0) ? (1.0 / result) : 0.0);
			ctl->cmsg(CMSG_INFO, VERB_NORMAL,
				  "Volume adjustment:%13.3f", result);
		}
	}

	if (fp && fp != stdout) {
		fclose(fp);
	}
	fp = NULL;

	dpm.fd = -1;
}

static int acntl(int request, void *arg)
{
  switch (request) {
  case PM_REQ_PLAY_START:
    return 0;
  case PM_REQ_PLAY_END:
    if (dpm.flag & PF_AUTO_SPLIT_FILE)
      close_output();
    return 0;
  case PM_REQ_DISCARD:
    return 0;
  }
  return -1;
}

#endif /* AU_VOLUME_CALC */