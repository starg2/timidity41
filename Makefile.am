# TiMidity++ -- MIDI to WAVE converter and player
# Copyright (C) 1999 Masanao Izumo <mo@goice.co.jp>
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
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
# This Makefile is known to work with GNU make.

SUBDIRS = \
	utils \
	libarc \
	interface \
	timidity \
	doc

EXTRA_DIST = \
	AUTHORS \
	COPYING \
	ChangeLog \
	INSTALL \
	INSTALL.jp \
	NEWS \
	README \
	README.jp \
	timidity.ide \
	timidity.mak

dynamics:
	cd interface; $(MAKE) $@

install.man install.manj:
	cd doc; $(MAKE) $@

install.tk:
	cd interface; $(MAKE) $@

install.el:
	cd interface; $(MAKE) $@

install.bin:
	cd timidity; $(MAKE) install

dist-hook:
	chmod -R a+r $(distdir)
	$(TAR) chof - $(distdir) | bzip2 -9 --repetitive-best - > $(distdir).tar.bz2
