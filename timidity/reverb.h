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

*/


/*****************************************************************************
 * REVERB EFFECT FOR TIMIDITY-0.2i+X (Version 0.06a  1998/2/15)
 *                      Copyright (C) 1997,1998  Masaki Kiryu <mkiryu@usa.net>
 *                                   (http://www.netq.or.jp/~user/hid/masaki/)
 *
 * reverb.h
 *****************************************************************************/
#ifndef ___REVERB_H_
#define ___REVERB_H_

#define DEFAULT_REVERB_SEND_LEVEL   54

extern void do_reverb(int32 *, int32);
extern void do_ch_reverb(int32 *, int32);
extern void set_ch_reverb(register int32 *, int32, int);
extern void do_mono_reverb(int32 *, int32);
extern void init_reverb(int32);
extern void reverb_rc_event(int, int32);
extern int do_reverb_flag; /* from command line option */

#endif /* ___REVERB_H_ */
