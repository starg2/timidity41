/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2018 Masanao Izumo <iz@onicos.co.jp>
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


    rtsyn_internal.h
*/

#ifndef ___RTSYN_INTERNAL_H_
#define ___RTSYN_INTERNAL_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

typedef struct {
    char id_character;

    void (*get_port_list)(void);
    int  (*synth_start)(void);
    void (*synth_stop)(void);
    int  (*play_some_data)(void);
    void (*midiports_close)(void);
    int  (*buf_check)(void);
} RTSynInternal;

extern RTSynInternal rtsyn;

extern int rtsyn_portnumber;
extern unsigned int portID[MAX_PORT];
extern char rtsyn_portlist[MAX_RTSYN_PORTLIST_NUM][MAX_RTSYN_PORTLIST_LEN];
extern int rtsyn_nportlist;

#endif /* ___RTSYN_INTERNAL_H_ */
