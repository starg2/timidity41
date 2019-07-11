## TiMidity++ -- MIDI to WAVE converter and player
## Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
## Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

# Define follows if you want to change.
# Note that the definition of beginning with just one `#' implies
# default value from configure.

SHELL=/bin/bash


## Package compilations
## Please modify and uncomment if you want to change.
#CC= i686-w64-mingw32-gcc
#CFLAGS =  -DWINVER=0x0400 -D_WIN32_WINDOWS=0x0400
# For pentium gcc
##CFLAGS = -O3 -mpentium -march=pentium -fomit-frame-pointer \
##         -funroll-all-loops -malign-double -ffast-math
# For PGCC
##CFLAGS = -O6 -mpentium -fomit-frame-pointer -funroll-all-loops -ffast-math \
##         -fno-peep-spills \
##         -fno-exceptions -malign-jumps=0 -malign-loops=0 -malign-functions=0
#CPPFLAGS =  -I. -I../cygwin/ -D__W32__ -DAU_W32 -DAU_BENCHMARK
WINDRES = i686-w64-mingw32-windres

#DEFS = -DHAVE_CONFIG_H
#LDFLAGS = 
#LIBS = -lm        -lwinmm
#SHLD = gcc -mdll 
#SHCFLAGS =  -fPIC
#


## Package installations:
#prefix = /usr/local
#exec_prefix = ${prefix}
#bindir = ${exec_prefix}/bin
#libdir = ${exec_prefix}/lib
#datadir = ${prefix}/share
#mandir = ${prefix}/share/man
pkglibdir = /usr/local/lib/timidity
pkgdatadir = /usr/local/share/timidity
#INSTALL = /usr/bin/install -c

# Where to install the patches, config files.
PKGDATADIR = $(pkgdatadir)

# Where to install the Tcl code and the bitmaps.
# It also contains bitmaps which are shared with XAW interface.
PKGLIBDIR = $(pkglibdir)

# Where to install the dynamic link interface.
SHLIB_DIR = $(pkglibdir)

# Where to install timidity.el
ELISP_DIR = $(lispdir)

# Where to install TiMidity.ad
XAWRES = ${xawresdir}/app-defaults
XAWRES_JA = ${xawresdir}/ja_JP.eucJP/app-defaults

# If you want to change TCL_DIR, please do follows.
# * Add -DTKPROGPATH=\"$(TCL_DIR)/tkmidity.tcl\" to CPPFLAGS.
# * Make a symbolic link $(PKGLIBDIR)/bitmaps to $(TCL_DIR)/bitmaps
TCL_DIR = $(PKGLIBDIR)
##CPPFLAGS += -DTKPROGPATH=\"$(TCL_DIR)/tkmidity.tcl\"

# Define the timidity default file search path.
DEF_DEFAULT_PATH = -DDEFAULT_PATH=\"$(PKGDATADIR)\"

# You sould not change follows definitions. 
DEF_PKGDATADIR = -DPKGDATADIR=\"$(PKGDATADIR)\"
DEF_PKGLIBDIR = -DPKGLIBDIR=\"$(PKGLIBDIR)\"
DEF_SHLIB_DIR = -DSHLIB_DIR=\"$(SHLIB_DIR)\"
BITMAP_DIR = $(TCL_DIR)/bitmaps
