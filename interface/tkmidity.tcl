#!/usr/bin/wish -f
#
# TiMidity++ -- MIDI to WAVE converter and player
# Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
# Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
#
# TkMidity -- Tcl/Tk Interface for TiMidity
#	written by Takashi IWAI
#
#  Version 1.3; Dec. 5, 1995
#


# system path and configuration file path
set TclRoot /usr/lib/timidity
set bitmap_path /usr/lib/timidity/bitmaps
set ConfigFile "~/.tkmidity"

# append library path
lappend auto_path $TclRoot

#
# window fonts
#

option add *menu*font "-adobe-helvetica-bold-r-normal--12-*"
option add *body.curfile.font "-adobe-helvetica-bold-o-normal--18-*"
option add *body.file*font "-adobe-helvetica-medium-r-normal--14-*"
option add *body*label.font "-adobe-times-bold-i-normal--18-*"
option add *body*Scale.font "-adobe-times-medium-i-normal--14-*"
option add *body.text*font "-adobe-helvetica-medium-r-normal--14-*"

#
# main routine
#

InitGlobal
LoadConfig
InitCmdLine $argc $argv

CreateWindow
AppendMsg "TiMidity Tcl/Tk Version\n  written by Takashi Iwai\n"

init-random [pid]

if {$Stat(new_tcltk)} {
    fileevent stdin readable HandleInput
} else {
    addinput -read stdin HandleInput
}

if {$Config(AutoStart)} { PlayCmd }
tkwait window .

# send quit message to the main process before die...
WriteMsg "QUIT"
WriteMsg "ZAPP"
exit 0

# end of file
