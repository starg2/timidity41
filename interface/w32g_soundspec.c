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

    w32g_soundspec.c

	Windows interface for TiMidity++
	written by yta

*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */
#include <math.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#ifdef __W32__

#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "output.h"
#include "controls.h"
#include "soundspec.h"
#include "fft.h"
#include "miditrace.h"
#include "timer.h"
#include "w32g.h"

#if defined(SUPPORT_SOUNDSPEC)

#include <windows.h>
#include <wingdi.h>


#define FFTSIZE 1024		/* Power of 2 */
#define SCOPE_HEIGHT 512	/* You can specified any positive value */
#define SCOPE_WIDTH  512	/* You can specified any positive value */
#define SCROLL_THRESHOLD 256	/* 1 <= SCROLL_THRESHOLD <= SCOPE_WIDTH */
#define NCOLOR 32768		/* 1 <= NCOLOR <= 65536 */
#define AMP 1.0
#define DEFAULT_ZOOM (44100.0 / 1024.0 * 2.0) /* ~86Hz */
#define MIN_ZOOM 15.0		/* 15Hz is lower bound that human can be heard. */
#define MAX_ZOOM 440.0
#define DEFAULT_UPDATE 0.05

static int initflag;
///r
static DATA_T *ring_buffer;
#define ring_buffer_len (8 * AUDIO_BUFFER_SIZE)
static int ring_index;
static int32 outcnt;
static FLOAT_T exp_hz_table[SCOPE_HEIGHT + 1];
static FLOAT_T *w_table;

static int32 call_cnt;

int view_soundspec_flag;
int ctl_speana_flag;
int32 soundspec_update_interval;
static int32 next_wakeup_samples;
static FLOAT_T soundspec_zoom = DEFAULT_ZOOM;

static RGBTRIPLE *spec_pixels;

static DWORD dwWidth;
static DWORD dwHeight;

static HWND win;

static BOOL disp;
static BITMAPINFO bmi;

static RGBTRIPLE color_ring[NCOLOR];

typedef struct _rgb_t {
	double r, g, b;
} rgb_t;
static void hsv_to_rgb(double h, double s, double v, rgb_t *rgb)
{
	double f;
	double i;
	double p1, p2, p3;

	if (s < 0)
		s = 0;
	if (v < 0)
		v = 0;

	if (s > 1)
		s = 1;
	if (v > 1)
		v = 1;

	h = fmod(h, 360.0);
	if (h < 0)
		h += 360;

	h *= DIV_60;
	f = modf(h, &i);
	p1 = v * (1 - s);
	p2 = v * (1 - s * f);
	p3 = v * (1 - s * (1 - f));

	switch ((int)i) {
	case 0:
		rgb->r = v;
		rgb->g = p3;
		rgb->b = p1;
		return;
	case 1:
		rgb->r = p2;
		rgb->g = v;
		rgb->b = p1;
		return;
	case 2:
		rgb->r = p1;
		rgb->g = v;
		rgb->b = p3;
		return;
	case 3:
		rgb->r = p1;
		rgb->g = p2;
		rgb->b = v;
		return;
	case 4:
		rgb->r = p3;
		rgb->g = p1;
		rgb->b = v;
		return;
	case 5:
		rgb->r = v;
		rgb->g = p1;
		rgb->b = p2;
		return;
	}
	return;
}

static double calc_color_diff(int r1, int g1, int b1,
			      int r2, int g2, int b2)
{
	double rd, gd, bd;

	rd = r2 - r1;
	gd = g2 - g1;
	bd = b2 - b1;

	return rd * rd + gd * gd + bd * bd;
}

static RGBTRIPLE AllocRGBColor(
	double red,		/* [0, 1] */
	double green,		/* [0, 1] */
	double blue)		/* [0, 1] */
{
	RGBTRIPLE c;

	c.rgbtRed = (uint8)(red * 255.999);
	c.rgbtGreen = (uint8)(green * 255.999);
	c.rgbtBlue = (uint8)(blue * 255.999);

	return c;
}

static void set_color_ring(void)
{
	int i;
	const double div_ncolor = 1.0 / NCOLOR;

	/*i = 0          ...        NCOLOR
	 *  Blue -> Green -> Red -> White
	 */
	for (i = 0; i < NCOLOR; i++)
	{
		rgb_t rgb;
		double h = 240 - 360 * sqrt(div_ncolor * i);
		double s, v;

		if (h >= 0)
			s = 0.9;
		else
		{
			s = 0.9 + h * DIV_120 * 0.9;
			h = 0.0;
		}
/*		v = sqrt(i * div_ncolor * 0.6 + 0.4); */
		v = 1.0;
		hsv_to_rgb(h, s, v, &rgb);
		color_ring[i] = AllocRGBColor(rgb.r, rgb.g, rgb.b);
	}
}

static void set_draw_pixel(double *val, RGBTRIPLE pixels[])
{
	int i;
	for (i = 0; i < SCOPE_HEIGHT; i++)
	{
		unsigned v;
		v = (unsigned)val[SCOPE_HEIGHT - i - 1];
		if (v >= NCOLOR)
			v = NCOLOR - 1;
		pixels[i] = color_ring[v % NCOLOR];
	}
}

static void make_logspectrogram(double *from, double *to)
{
	double px;
	int i;

	to[0] = from[0];
	px = 0.0;
	for (i = 1; i < SCOPE_HEIGHT - 1; i++)
	{
		double tx, s;
		int x1, n;

		tx = exp_hz_table[i];
		x1 = (int)px;
		n = 0;
		s = 0.0;
		do
		{
			double a;
			a = from[x1];
			s += a + (tx - x1) * (from[x1 + 1] - a);
			n++;
			x1++;
		} while (x1 < tx);
		to[i] = s / n;
		px = tx;
	}
	to[SCOPE_HEIGHT - 1] = from[divi_2(FFTSIZE) - 1];

	for (i = 0; i < SCOPE_HEIGHT; i++)
		if (to[i] <= -0.0)
			to[i] = 0.0;
}

static void initialize_exp_hz_table(double zoom)
{
	int i;
	double r, x, w;

	if (zoom < MIN_ZOOM)
		soundspec_zoom = MIN_ZOOM;
	else if (zoom > MAX_ZOOM)
		soundspec_zoom = MAX_ZOOM;
	else
		soundspec_zoom = zoom;

	w = (double)play_mode->rate * 0.5 / zoom;
	r = exp(log(w) * (1.0 / SCOPE_HEIGHT));
	w = (FFTSIZE / 2.0) / (w - 1.0);
	for (i = 0, x = 1.0; i <= SCOPE_HEIGHT; i++, x *= r)
		exp_hz_table[i] = (x - 1.0) * w;
}

void TargetSpectrogramCanvas(HWND hwnd)
{
	win = hwnd;
}

void UpdateSpectrogramCanvas(void)
{
	HDC context;
	RECT rect;

	if (win && IsWindowVisible(win)) {
		GetClientRect(win, &rect);
		dwWidth = rect.right;
		dwHeight = rect.bottom;

		context = GetDC(win);
		StretchDIBits(context,
			      0, 0, dwWidth, dwHeight,
			      0, 0, SCOPE_WIDTH, SCOPE_HEIGHT,
			      spec_pixels,
			      &bmi, DIB_RGB_COLORS, SRCCOPY);
		ReleaseDC(win, context);
		context = NULL;
	}
}

void HandleSpecKeydownEvent(long message, short modifiers)
{
	int32 nze = 0;

	switch (message & 0x7F)
	{
	case VK_UP:	nze++;	break;	//up
	case VK_DOWN:	nze--;	break;	//down

	case VK_LEFT:	//left
		soundspec_update_interval =
			(int32)(soundspec_update_interval * 1.1);
		if (soundspec_update_interval > 0.5 * play_mode->rate)
			soundspec_update_interval =
				(int32)(0.5 * play_mode->rate);
		break;

	case VK_RIGHT:	//right
		soundspec_update_interval =
			(int32)(soundspec_update_interval / 1.1);
		if (soundspec_update_interval < 0.01 * play_mode->rate)
			soundspec_update_interval =
				(int32)(0.01 * play_mode->rate);
		break;
	}

	if (nze)
		initialize_exp_hz_table(soundspec_zoom - 4 * nze);
}

static void draw_scope(double *values)
{
	int32 offset, expose, i;
	RGBTRIPLE pixels[SCOPE_HEIGHT];
	double work[SCOPE_HEIGHT];
	int32 nze;

	nze = 0;
	make_logspectrogram(values, work);

	if (win && disp) {
		set_draw_pixel(work, pixels);

		expose = 0;

		offset = call_cnt % SCROLL_THRESHOLD;
		if (offset == 0)
		{	//scroll the window
			const int scope_width_half = divi_2(SCOPE_WIDTH);
			const int scope_scroll_body = SCOPE_WIDTH - SCROLL_THRESHOLD;
			for (i = 0; i < SCOPE_HEIGHT; i++) {
				CopyMemory(spec_pixels + i * SCOPE_WIDTH,
					   spec_pixels + i * SCOPE_WIDTH + SCROLL_THRESHOLD,
					   scope_scroll_body * sizeof(RGBTRIPLE));
				FillMemory(spec_pixels + i * SCOPE_WIDTH + scope_scroll_body,
					   SCROLL_THRESHOLD * sizeof(RGBTRIPLE),
					   0);
			}
		}

		//draw
		for (i = 0; i < SCOPE_HEIGHT; i++) {
			spec_pixels[i * SCOPE_WIDTH + SCOPE_WIDTH
				      - SCROLL_THRESHOLD + offset] = pixels[i];
		}
		UpdateSpectrogramCanvas();
	}

	if (nze)
		initialize_exp_hz_table(soundspec_zoom - 4 * nze);

	call_cnt++;
}

struct drawing_queue
{
	double values[FFTSIZE / 2 + 1];
	struct drawing_queue *next;
};
static struct drawing_queue *free_queue_list = NULL;

static struct drawing_queue *new_queue(void)
{
	struct drawing_queue *p;

	if (free_queue_list)
	{
		p = free_queue_list;
		free_queue_list = free_queue_list->next;
	}
	else
		p = (struct drawing_queue*) safe_malloc(sizeof(struct drawing_queue));
	p->next = NULL;
	return p;
}
static void free_queue(struct drawing_queue *p)
{
	p->next = free_queue_list;
	free_queue_list = p;
}

static void trace_draw_scope(void *vp)
{
	struct drawing_queue *q;
	q = (struct drawing_queue*)vp;
	if (!midi_trace.flush_flag)
	{
		if (!midi_trace.flush_flag && win)
			draw_scope(q->values);
		if (ctl_speana_flag)
		{
			CtlEvent e;
			e.type = CTLE_SPEANA;
			e.v1 = (u_ptr_size_t)(q->values);
			e.v2 = divi_2(FFTSIZE);
			ctl->event(&e);
		}
	}
	free_queue(q);
}

static void decibelspec(double *from, double *to)
{
	double p, hr;
	int i, j;
	const double div_fftsize = 1.0 / FFTSIZE;
	const int half_fftsize = divi_2(FFTSIZE);

	if (!w_table)
	{
		double t;
		const double add_t_inc = 2.0 * M_PI * div_fftsize;

		w_table = (FLOAT_T*) safe_malloc(FFTSIZE * sizeof(FLOAT_T));
		t = -M_PI;
		for (i = 0; i < FFTSIZE; i++)
		{
			w_table[i] = 0.5f + 0.5f * cos(t);
			t += add_t_inc;
		}
	}
	for (i = 0; i < FFTSIZE; i++)
		from[i] *= w_table[i];
	realfft(from, FFTSIZE);

	hr = AMP * NCOLOR;
	if (from[0] >= 0)
		p = from[0];
	else
		p = -from[0];
	to[0] = log(1.0 + (128.0 * div_fftsize) * p) * hr;
	for (i = 1, j = FFTSIZE - 1; i < half_fftsize; i++, j--)
	{
		double t, u;

		t = from[i];
		u = from[j];
		to[i] = log(1.0 + (128.0 * div_fftsize) * sqrt(t*t + u*u)) * hr;
	}
	p = from[half_fftsize];
	to[half_fftsize] = log(1.0 + (128.0 * div_fftsize) * sqrt(2 * p*p)) * hr;
}

void close_soundspec(void)
{
	disp = FALSE;

	while (free_queue_list) {
		struct drawing_queue *p = free_queue_list->next;
		safe_free(free_queue_list);
		free_queue_list = p;
	}

	realfft(NULL, 0);

	safe_free(ring_buffer);
	ring_buffer = NULL;
	safe_free(w_table);
	w_table = NULL;
	safe_free(spec_pixels);
	spec_pixels = NULL;
	
    view_soundspec_flag = 0;

	initflag = 0;
}

void open_soundspec(void)
{
	if (disp != FALSE)
	{	//already opened
		ring_index = 0;
		next_wakeup_samples = outcnt;
		ZeroMemory(ring_buffer, ring_buffer_len * sizeof(DATA_T));
		view_soundspec_flag = 1;
		return;
	}

	if (soundspec_update_interval == 0)
		soundspec_update_interval = (int32)(DEFAULT_UPDATE * play_mode->rate);

	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = SCOPE_WIDTH;
	bmi.bmiHeader.biHeight = -SCOPE_HEIGHT;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 8 * sizeof(RGBTRIPLE);
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biSizeImage = SCOPE_WIDTH * sizeof(RGBTRIPLE) * SCOPE_HEIGHT;

	disp = TRUE;

	if (!initflag)
	{
///r
		ring_buffer = (DATA_T*) safe_large_malloc(ring_buffer_len * sizeof(DATA_T)); // def safe_malloc
		realfft(NULL, FFTSIZE);
		initialize_exp_hz_table(DEFAULT_ZOOM);
		spec_pixels = (RGBTRIPLE*) safe_large_malloc(SCOPE_WIDTH * sizeof(RGBTRIPLE) * SCOPE_HEIGHT);
		initflag = 1;
	}

	ZeroMemory(spec_pixels, SCOPE_WIDTH * sizeof(RGBTRIPLE) * SCOPE_HEIGHT);

	set_color_ring();

	call_cnt = 0;
	ring_index = 0;
	next_wakeup_samples = outcnt;
	ZeroMemory(ring_buffer, ring_buffer_len * sizeof(DATA_T));
	view_soundspec_flag = 1;
}

void soundspec_setinterval(double sec)
{
	soundspec_update_interval = (int32)(sec * play_mode->rate);

	if (soundspec_update_interval == 0)
		soundspec_update_interval = (int32)(DEFAULT_UPDATE * play_mode->rate);
	if (soundspec_update_interval > 0.5 * play_mode->rate)
		soundspec_update_interval = (int32)(0.5 * play_mode->rate);
	if (soundspec_update_interval < 0.01 * play_mode->rate)
		soundspec_update_interval = (int32)(0.01 * play_mode->rate);
}

static void ringsamples(double *x, int pos, int n)
{
	int i, upper;
	double r;

	upper = ring_buffer_len;
#ifdef EFFECT_LEVEL_FLOAT
	r = 1.0 / 2.0;
#else
	r = 1.0 / pow(2.0, 32.0);
#endif
	for (i = 0; i < n; i++, pos++)
	{
		if (pos >= upper)
			pos = 0;
		x[i] = (double)ring_buffer[pos] * r;
	}
}

void soundspec_update_wave(DATA_T *buff, int samples)
{
	int i;

	if (!win || !disp || !view_soundspec_flag)
	{
		outcnt += samples;
		return;
	}

	if (!buff) /* Initialize */
	{
		ring_index = 0;
		if (samples == 0)
		{
			outcnt = 0;
			next_wakeup_samples = 0;
		}
		if (ring_buffer)
			ZeroMemory(ring_buffer, ring_buffer_len * sizeof(DATA_T));
		return;
	}

	if (!ring_buffer)
	{
		ring_buffer = (DATA_T*) safe_large_calloc(ring_buffer_len, sizeof(DATA_T));
		if (soundspec_update_interval == 0)
			soundspec_update_interval =
				(int32)(DEFAULT_UPDATE * play_mode->rate);
		realfft(NULL, FFTSIZE);
		initialize_exp_hz_table(soundspec_zoom);
	}

	if (ring_index + samples > ring_buffer_len)
	{
		int d;

		d = ring_buffer_len - ring_index;
		if (play_mode->encoding & PE_MONO)
			CopyMemory(ring_buffer + ring_index, buff, d * sizeof(DATA_T));
		else
		{
			DATA_T *p;
			int n;

			p = ring_buffer + ring_index;
			n = d * 2;
			for (i = 0; i < n; i += 2)
				*p++ = divt_2(buff[i] + buff[i + 1]);
		}
		ring_index = 0;
		outcnt += d;
		samples -= d;
	}

	if (play_mode->encoding & PE_MONO)
		CopyMemory(ring_buffer + ring_index, buff, samples * sizeof(DATA_T));
	else
	{
		DATA_T *p;
		int n;

		p = ring_buffer + ring_index;
		n = samples * 2;
		for (i = 0; i < n; i += 2)
			*p++ = divt_2(buff[i] + buff[i + 1]);
	}

	ring_index += samples;
	outcnt += samples;
	if (ring_index == ring_buffer_len)
		ring_index = 0;

	if (next_wakeup_samples < outcnt - (ring_buffer_len - FFTSIZE))
	{
		/* next_wakeup_samples is too small */
		next_wakeup_samples = outcnt - (ring_buffer_len - FFTSIZE);
	}

	while (next_wakeup_samples < outcnt - FFTSIZE)
	{
		double x[FFTSIZE];
		struct drawing_queue *q;

		ringsamples(x, next_wakeup_samples % ring_buffer_len, FFTSIZE);
		q = new_queue();
		decibelspec(x, q->values);
		push_midi_time_vp(midi_trace.offset + next_wakeup_samples,
				  trace_draw_scope,
				  q);
		next_wakeup_samples += soundspec_update_interval;
	}
}

/* Re-initialize something */
void soundspec_reinit(void)
{
	initialize_exp_hz_table(soundspec_zoom);
}

#endif /* SUPPORT_SOUNDSPEC */

#endif /* __W32__ */

