#ifndef ___OUTPUT_H_
#define ___OUTPUT_H_
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

    output.h

*/

/* Data format encoding bits */

#define PE_MONO 	(1u<<0)  /* versus stereo */
#define PE_SIGNED	(1u<<1)  /* versus unsigned */
#define PE_16BIT 	(1u<<2)  /* versus 8-bit */
#define PE_ULAW 	(1u<<3)  /* versus linear */
#define PE_ALAW 	(1u<<4)  /* versus linear */
#define PE_BYTESWAP	(1u<<5)  /* versus the other way */

/* Flag bits */

#define PF_NEED_INSTRUMENTS 	(1u<<0)
#define PF_CAN_TRACE		(1u<<1)

typedef struct {
  int32 rate, encoding, flag;
  int fd; /* file descriptor for the audio device */
  int32 extra_param[5]; /* e.g. buffer fragments, output channel, ... */
  char *id_name, id_character;
  char *name; /* default device or file name */

  int (*play_event)(void *);
  int (*open_output)(void); /* 0=success, 1=warning, -1=fatal error */
  void (*close_output)(void);
  void (*output_data)(int32 *buf, int32 count);
  int (*flush_output)(void);
  void (*purge_output)(void);
  int32 (*current_samples)(void);
  int (*play_loop)(void);
} PlayMode;

extern PlayMode *play_mode_list[], *play_mode;

/* Conversion functions -- These overwrite the int32 data in *lp with
   data in another format */

/* 8-bit signed and unsigned*/
extern void s32tos8(int32 *lp, int32 c);
extern void s32tou8(int32 *lp, int32 c);

/* 16-bit */
extern void s32tos16(int32 *lp, int32 c);
extern void s32tou16(int32 *lp, int32 c);

/* byte-exchanged 16-bit */
extern void s32tos16x(int32 *lp, int32 c);
extern void s32tou16x(int32 *lp, int32 c);

/* uLaw (8 bits) */
extern void s32toulaw(int32 *lp, int32 c);

/* aLaw (8 bits) */
extern void s32toalaw(int32 *lp, int32 c);

extern int32 dumb_current_samples(void);
extern int   dumb_play_loop(void);

/* little-endian and big-endian specific */
#ifdef LITTLE_ENDIAN
#define s32tou16l s32tou16
#define s32tou16b s32tou16x
#define s32tos16l s32tos16
#define s32tos16b s32tos16x
#else
#define s32tou16l s32tou16x
#define s32tou16b s32tou16
#define s32tos16l s32tos16x
#define s32tos16b s32tos16
#endif

#endif /* ___OUTPUT_H_ */
