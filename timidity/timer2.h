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

    timer2.h
*/


#ifndef ___TIMER2_H_
#define ___TIMER2_H_

// timer ID list
enum timer_event_t {
        trace_timer = 0,
        pref_timer,
        MAX_TIMER_ID, // last
};

typedef void (*timer_func_t)(void);

extern timer_func_t timer_func[MAX_TIMER_ID];
extern int start_timer(uint32 id, timer_func_t fnc, uint32 ms);
extern void stop_timer(uint32 id);


#endif /* ___TIMER2_H_ */




