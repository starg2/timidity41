/* 

    TiMidity -- Experimental MIDI to WAVE converter
    Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

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

   controls.c
   
   */

#include "config.h"
#include "controls.h"

#ifdef MOTIF
  extern ControlMode motif_control_mode;
# ifndef DEFAULT_CONTROL_MODE
#  define DEFAULT_CONTROL_MODE &motif_control_mode
# endif
#endif

#ifdef TCLTK
  extern ControlMode tk_control_mode;
# ifndef DEFAULT_CONTROL_MODE
#  define DEFAULT_CONTROL_MODE &tk_control_mode
# endif
#endif

#ifdef IA_NCURSES
  extern ControlMode ncurses_control_mode;
# ifndef DEFAULT_CONTROL_MODE
#  define DEFAULT_CONTROL_MODE &ncurses_control_mode
# endif
#endif

#ifdef IA_SLANG
  extern ControlMode slang_control_mode;
# ifndef DEFAULT_CONTROL_MODE
#  define DEFAULT_CONTROL_MODE &slang_control_mode
# endif
#endif

/* Minimal control mode */
extern ControlMode dumb_control_mode;
#ifndef DEFAULT_CONTROL_MODE
# define DEFAULT_CONTROL_MODE &dumb_control_mode
#endif

ControlMode *ctl_list[]={
#ifdef IA_NCURSES
  &ncurses_control_mode,
#endif
#ifdef IA_SLANG
  &slang_control_mode,
#endif
#ifdef MOTIF
  &motif_control_mode,
#endif
#ifdef TCLTK
  &tk_control_mode,
#endif
  &dumb_control_mode,
  0
};

ControlMode *ctl=DEFAULT_CONTROL_MODE;
