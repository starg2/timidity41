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
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "timidity.h"
#include "output.h"
#include "tables.h"
#include "controls.h"
#include "audio_cnv.h"



extern PlayMode alsa_play_mode;


extern PlayMode hpux_nplay_mode;



/* These are very likely mutually exclusive.. */
#if defined(AU_AUDRIV)
extern PlayMode audriv_play_mode;
#define DEV_PLAY_MODE &audriv_play_mode

#elif defined(AU_SUN)
extern PlayMode sun_play_mode;
#define DEV_PLAY_MODE &sun_play_mode

#elif defined(AU_OSS)
extern PlayMode oss_play_mode;
#define DEV_PLAY_MODE &oss_play_mode

#elif defined(AU_HPUX_AUDIO)
extern PlayMode hpux_play_mode;
#define DEV_PLAY_MODE &hpux_play_mode

#elif defined(AU_W32)
extern PlayMode w32_play_mode;
#define DEV_PLAY_MODE &w32_play_mode

#elif defined(AU_BSDI)
extern PlayMode bsdi_play_mode;
#define DEV_PLAY_MODE &bsdi_play_mode

#elif defined(__MACOS__)
extern PlayMode mac_play_mode;
#define DEV_PLAY_MODE &mac_play_mode
#endif

#ifdef AU_ALSA
extern PlayMode alsa_play_mode;
#endif /* AU_ALSA */

#ifdef AU_HPUX_ALIB
extern PlayMode hpux_nplay_mode
#endif /* AU_HPUX_ALIB */

#ifdef AU_ESD
extern PlayMode esd_play_mode;
#endif /* AU_ESD */

#ifdef AU_NAS
extern PlayMode nas_play_mode;
#endif /* AU_NAS */

#ifndef __MACOS__
/* These are always compiled in. */
extern PlayMode raw_play_mode, wave_play_mode, au_play_mode, aiff_play_mode;
extern PlayMode list_play_mode;
#endif /* !__MACOS__ */


PlayMode *play_mode_list[] = {
#ifdef DEV_PLAY_MODE
  DEV_PLAY_MODE,
#endif

#ifdef AU_ALSA
  &alsa_play_mode,
#endif /* AU_ALSA */

#ifdef AU_HPUX_ALIB
  &hpux_nplay_mode,
#endif /* AU_HPUX_ALIB */

#if defined(AU_ESD)
  &esd_play_mode,
#endif /* AU_ESD */

#if defined(AU_NAS)
  &nas_play_mode,
#endif /* AU_NAS */

#ifndef __MACOS__
  &wave_play_mode,
  &raw_play_mode,
  &au_play_mode,
  &aiff_play_mode,
  &list_play_mode,
#endif /* __MACOS__ */
  0
};

PlayMode *play_mode = NULL;
PlayMode *target_play_mode = NULL;

/*****************************************************************/
/* Some functions to convert signed 32-bit data to other formats */

void s32tos8(int32 *lp, int32 c)
{
    int8 *cp=(int8 *)(lp);
    int32 l, i;

    for(i = 0; i < c; i++)
    {
	l=(lp[i])>>(32-8-GUARD_BITS);
	if (l>127) l=127;
	else if (l<-128) l=-128;
	cp[i] = (int8)(l);
    }
}

void s32tou8(int32 *lp, int32 c)
{
    uint8 *cp=(uint8 *)(lp);
    int32 l, i;

    for(i = 0; i < c; i++)
    {
	l=(lp[i])>>(32-8-GUARD_BITS);
	if (l>127) l=127;
	else if (l<-128) l=-128;
	cp[i] = 0x80 ^ ((uint8) l);
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

/* return: number of bytes */
int32 general_output_convert(int32 *buf, int32 count)
{
    int32 bytes;

    if(!(play_mode->encoding & PE_MONO))
	count *= 2; /* Stereo samples */
    bytes = count;
    if(play_mode->encoding & PE_16BIT)
    {
	bytes *= 2;
	if(play_mode->encoding & PE_BYTESWAP)
	{
	    if(play_mode->encoding & PE_SIGNED)
		s32tos16x(buf, count);
	    else
		s32tou16x(buf, count);
	}
	else if(play_mode->encoding & PE_SIGNED)
	    s32tos16(buf, count);
	else
	    s32tou16(buf, count);
    }
    else if(play_mode->encoding & PE_ULAW)
	s32toulaw(buf, count);
    else if(play_mode->encoding & PE_ALAW)
	s32toalaw(buf, count);
    else if(play_mode->encoding & PE_SIGNED)
	s32tos8(buf, count);
    else
	s32tou8(buf, count);
    return bytes;
}

int validate_encoding(int enc, int include_enc, int exclude_enc)
{
    const char *orig_enc_name, *enc_name;
    int orig_enc;

    orig_enc = enc;
    orig_enc_name = output_encoding_string(enc);
    enc |= include_enc;
    enc &= ~exclude_enc;
    if(enc & (PE_ULAW|PE_ALAW))
	enc &= ~(PE_16BIT|PE_SIGNED|PE_BYTESWAP);
    if(!(enc & PE_16BIT))
	enc &= ~PE_BYTESWAP;
    enc_name = output_encoding_string(enc);
    if(strcmp(orig_enc_name, enc_name) != 0)
	ctl->cmsg(CMSG_WARNING, VERB_NOISY,
		  "Notice: Audio encoding is changed `%s' to `%s'",
		  orig_enc_name, enc_name);
    return enc;
}

const char *output_encoding_string(int enc)
{
    if(enc & PE_MONO)
    {
	if(enc & PE_16BIT)
	{
	    if(enc & PE_SIGNED)
		return "16bit (mono)";
	    else
		return "unsigned 16bit (mono)";
	}
	else
	{
	    if(enc & PE_ULAW)
		return "U-law (mono)";
	    else if(enc & PE_ALAW)
		return "A-law (mono)";
	    else if(enc & PE_SIGNED)
		return "8bit (mono)";
	    else
		return "unsigned 8bit (mono)";
	}
    }
    else if(enc & PE_16BIT)
    {
	if(enc & PE_BYTESWAP)
	{
	    if(enc & PE_SIGNED)
		return "16bit (swap)";
	    else
		return "unsigned 16bit (swap)";
	}
	else if(enc & PE_SIGNED)
	    return "16bit";
	else
	    return "unsigned 16bit";
    }
    else
	if(enc & PE_ULAW)
	    return "U-law";
	else if(enc & PE_ALAW)
	    return "A-law";
	else if(enc & PE_SIGNED)
	    return "8bit";
	else
	    return "unsigned 8bit";
    /*NOTREACHED*/
}
