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

    output.c

    Audio output (to file / device) functions.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include "timidity.h"
#include "output.h"
#include "tables.h"
#include "audio_cnv.h"


/* These are very likely mutually exclusive.. */
#ifdef AU_AUDRIV
extern PlayMode audriv_play_mode;
#define DEFAULT_PLAY_MODE &audriv_play_mode
#endif

#ifdef AU_LINUX
extern PlayMode linux_play_mode;
#define DEFAULT_PLAY_MODE &linux_play_mode
#endif

#ifdef AU_ALSA
extern PlayMode alsa_play_mode;
#endif

#ifdef AU_HPUX
extern PlayMode hpux_play_mode;
extern PlayMode hpux_nplay_mode;
#define DEFAULT_PLAY_MODE &hpux_play_mode
#define NETWORK_PLAY_MODE &hpux_nplay_mode
#endif

#ifdef AU_WIN32
extern PlayMode win32_play_mode;
#define DEFAULT_PLAY_MODE &win32_play_mode
#endif

#ifdef AU_BSDI
extern PlayMode bsdi_play_mode;
#define DEFAULT_PLAY_MODE &bsdi_play_mode
#endif

#ifndef __MACOS__
/* These are always compiled in. */
extern PlayMode raw_play_mode, wave_play_mode, au_play_mode, aiff_play_mode;
extern PlayMode list_play_mode;
#else /* __MACOS__ */
extern PlayMode mac_play_mode;
#define DEFAULT_PLAY_MODE &mac_play_mode
#endif /* __MACOS__ */

PlayMode *play_mode_list[] = {
#ifdef DEFAULT_PLAY_MODE
  DEFAULT_PLAY_MODE,
#endif
#ifdef AU_ALSA
  &alsa_play_mode,
#endif
#ifdef NETWORK_PLAY_MODE
  NETWORK_PLAY_MODE,
#endif
#ifndef __MACOS__
  &wave_play_mode,
  &raw_play_mode,
  &au_play_mode,
  &aiff_play_mode,
  &list_play_mode,
#endif /* __MACOS__ */
  0
};

#ifdef DEFAULT_PLAY_MODE
  PlayMode *play_mode=DEFAULT_PLAY_MODE;
#else
  PlayMode *play_mode=&wave_play_mode;
#endif

/* for noise shaping */
int32 ns_tap[4] = { 0, 0, 0, 0};
static int32  ns_z0[4] = { 0, 0, 0, 0};
static int32  ns_z1[4] = { 0, 0, 0, 0};
int noise_shap_type = 0;

/*****************************************************************/
/* Some functions to convert signed 32-bit data to other formats */

void s32tos8(int32 *lp, int32 c)
{
    int8 *cp=(int8 *)(lp);
    int32 l, i, ll;

    if(!noise_shap_type)
    {
	for(i = 0; i < c; i++)
	{
	    l=(lp[i])>>(32-8-GUARD_BITS);
	    if (l>127) l=127;
	    else if (l<-128) l=-128;
	    cp[i] = (int8)(l);
	}
    }
    else
    {
	/* Noise Shaping filter from
	 * Kunihiko IMAI <imai@leo.ec.t.kanazawa-u.ac.jp>
	 */
	for(i = 0; i < c; i++)
	{
	    /* applied noise-shaping filter */
	    lp[i] += ns_tap[0]*ns_z0[0] + ns_tap[1]*ns_z0[1] + ns_tap[2]*ns_z0[2] + ns_tap[3]*ns_z0[3];
	    ll = lp[i];
	    l=(lp[i])>>(32-8-GUARD_BITS);
	    if (l>127) l=127;
	    else if (l<-128) l=-128;
	    cp[i] = (int8)(l);
	    ns_z0[3] = ns_z0[2]; ns_z0[2] = ns_z0[1]; ns_z0[1] = ns_z0[0];
	    ns_z0[0] = ll - l*(1U<<(32-8-GUARD_BITS));

	    if ( play_mode->encoding & PE_MONO ) continue;

	    ++i;
	    lp[i] += ns_tap[0]*ns_z1[0] + ns_tap[1]*ns_z1[1] + ns_tap[2]*ns_z1[2] + ns_tap[3]*ns_z1[3];
	    ll = lp[i];
	    l=(lp[i])>>(32-8-GUARD_BITS);
	    if (l>127) l=127;
	    else if (l<-128) l=-128;
	    cp[i] = (int8)(l);
	    ns_z1[3] = ns_z1[2]; ns_z1[2] = ns_z1[1]; ns_z1[1] = ns_z1[0];
	    ns_z1[0] = ll - l*(1U<<(32-8-GUARD_BITS));
	}
    }
}

void s32tou8(int32 *lp, int32 c)
{
    uint8 *cp=(uint8 *)(lp);
    int32 l, i, ll;

    if(!noise_shap_type)
    {
	for(i = 0; i < c; i++)
	{
	    l=(lp[i])>>(32-8-GUARD_BITS);
	    if (l>127) l=127;
	    else if (l<-128) l=-128;
	    cp[i] = 0x80 ^ ((uint8) l);
	}
    }
    else
    {
	/* Noise Shaping filter from
	 * Kunihiko IMAI <imai@leo.ec.t.kanazawa-u.ac.jp>
	 */
	for(i = 0; i < c; i++)
	{
	    /* applied noise-shaping filter */
	    lp[i] += ns_tap[0]*ns_z0[0] + ns_tap[1]*ns_z0[1] + ns_tap[2]*ns_z0[2] + ns_tap[3]*ns_z0[3];
	    ll = lp[i];
	    l=(lp[i])>>(32-8-GUARD_BITS);
	    if (l>127) l=127;
	    else if (l<-128) l=-128;
	    cp[i] = 0x80 ^ ((uint8) l);
	    ns_z0[3] = ns_z0[2]; ns_z0[2] = ns_z0[1]; ns_z0[1] = ns_z0[0];
	    ns_z0[0] = ll - l*(1U<<(32-8-GUARD_BITS));
      
	    if ( play_mode->encoding & PE_MONO ) continue;

	    ++i;
	    lp[i] += ns_tap[0]*ns_z1[0] + ns_tap[1]*ns_z1[1] + ns_tap[2]*ns_z1[2] + ns_tap[3]*ns_z1[3];
	    ll = lp[i];
	    l=(lp[i])>>(32-8-GUARD_BITS);
	    if (l>127) l=127;
	    else if (l<-128) l=-128;
	    cp[i] = 0x80 ^ ((uint8) l);
	    ns_z1[3] = ns_z1[2]; ns_z1[2] = ns_z1[1]; ns_z1[1] = ns_z1[0];
	    ns_z1[0] = ll - l*(1U<<(32-8-GUARD_BITS));
	}
    }
}

void s32tos16(int32 *lp, int32 c)
{
  int16 *sp=(int16 *)(lp);
  int32 l, i;

  for(i = 0; i < c; i++)
    {
      l=(lp[i])>>(32-16-GUARD_BITS);
      if (l > 32767) l=32767;
      else if (l<-32768) l=-32768;
      sp[i] = (int16)(l);
    }
}

void s32tou16(int32 *lp, int32 c)
{
  uint16 *sp=(uint16 *)(lp);
  int32 l, i;

  for(i = 0; i < c; i++)
    {
      l=(lp[i])>>(32-16-GUARD_BITS);
      if (l > 32767) l=32767;
      else if (l<-32768) l=-32768;
      sp[i] = 0x8000 ^ (uint16)(l);
    }
}

void s32tos16x(int32 *lp, int32 c)
{
  int16 *sp=(int16 *)(lp);
  int32 l, i;

  for(i = 0; i < c; i++)
    {
      l=(lp[i])>>(32-16-GUARD_BITS);
      if (l > 32767) l=32767;
      else if (l<-32768) l=-32768;
      sp[i] = XCHG_SHORT((int16)(l));
    }
}

void s32tou16x(int32 *lp, int32 c)
{
  uint16 *sp=(uint16 *)(lp);
  int32 l, i;

  for(i = 0; i < c; i++)
    {
      l=(lp[i])>>(32-16-GUARD_BITS);
      if (l > 32767) l=32767;
      else if (l<-32768) l=-32768;
      sp[i] = XCHG_SHORT(0x8000 ^ (uint16)(l));
    }
}

void s32toulaw(int32 *lp, int32 c)
{
    int8 *up=(int8 *)(lp);
    int32 l, i;

    for(i = 0; i < c; i++)
    {
	l=(lp[i])>>(32-16-GUARD_BITS);
	if (l > 32767) l=32767;
	else if (l<-32768) l=-32768;
	up[i] = AUDIO_S2U(l);
    }
}

void s32toalaw(int32 *lp, int32 c)
{
    int8 *up=(int8 *)(lp);
    int32 l, i;

    for(i = 0; i < c; i++)
    {
	l=(lp[i])>>(32-16-GUARD_BITS);
	if (l > 32767) l=32767;
	else if (l<-32768) l=-32768;
	up[i] = AUDIO_S2A(l);
    }
}

char *output_encoding_string(int enc)
{
    if(enc & PE_MONO)
	if(enc & PE_16BIT)
	    if(enc & PE_SIGNED)
		return "16bit (mono)";
	    else
		return "unsigned 16bit (mono)";
	else
	    if(enc & PE_ULAW)
		return "U-law (mono)";
	    else if(enc & PE_ALAW)
		return "A-law (mono)";
	    else
		if(enc & PE_SIGNED)
		    return "8bit (mono)";
		else
		    return "unsigned 8bit (mono)";
    else
	if(enc & PE_16BIT)
	    if(enc & PE_SIGNED)
		return "16bit";
	    else
		return "unsigned 16bit";
	else
	    if(enc & PE_ULAW)
		return "U-law";
	    else if(enc & PE_ALAW)
		return "A-law";
	    else
		if(enc & PE_SIGNED)
		    return "8bit";
		else
		    return "unsigned 8bit";
    /*NOTREACHED*/
}

int32 dumb_current_samples(void)
{
    return -1;
}

int dumb_play_loop(void)
{
    return 0;
}
