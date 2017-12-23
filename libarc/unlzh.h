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
*/

#ifndef ___LZH_H_
#define ___LZH_H_

typedef struct _UNLZHHandler *UNLZHHandler;

extern UNLZHHandler open_unlzh_handler(ptr_size_t (*read_func)(char*, ptr_size_t, void*),
				       const char *method,
				       ptr_size_t compsize, ptr_size_t origsize,
				       void *user_val);
extern ptr_size_t unlzh(UNLZHHandler decoder, char *buff, ptr_size_t buff_size);
extern void close_unlzh_handler(UNLZHHandler decoder);

extern char *lzh_methods[];

#endif /* ___LZH_H_ */
