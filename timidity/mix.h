/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

    In case you haven't heard, this program is free software;
    you can redistribute it and/or modify it under the terms of the
    GNU General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    mix.h

*/

#ifndef ___MIX_H_
#define ___MIX_H_

///r
extern void mix_voice(DATA_T *, int, int32);

extern int recompute_envelope(int);
extern int apply_envelope_to_amp(int);
extern int recompute_modulation_envelope(int);


extern void init_mix_c(void);
extern void free_mix_c(void);

#endif /* ___MIX_H_ */
