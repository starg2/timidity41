# 
# TiMidity -- Experimental MIDI to WAVE converter
# Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>
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

########### Installation options
#
# This looks like a job for Autoconf Repairman!
# But where to change without revealing my secret identity?

########### Compiler and flags.
CC = gcc
DEBUGFLAGS = -Wall -O2

########### Install.
INSTALL = /usr/bin/install

# Where to install the executable
BIN_DIR = /usr/local/bin

# Where to install the manual pages
MAN_DIR = /usr/local/man/man1

# Where to install the patches, config files, and MIDI files.
# If you change this, it's a good idea to recompile the binary,
# or you'll need to invoke timidity with the -L option.
TIMID_DIR = /usr/local/lib/timidity

# Where to install the Tcl code, if you use the Tcl code that is.
TCL_DIR = $(TIMID_DIR)

########### Compilation options -- there are more in config.h.  
#
# Uncomment the lines you need, changing library paths as required.

########### Audio device selection
#
# Select one only. If your make doesn't understand +=, you may have to
# do some axe work.

# Select the Linux/FreeBSD audio driver
SYSTEM += -DAU_LINUX
SYSEXTRAS += linux_a.c
#EXTRAINCS +=
#EXTRALIBS += 

## Select the HP-UX network audio server
#SYSTEM += -DHPUX -DAU_HPUX
#SYSEXTRAS += hpux_a.c
##EXTRAINCS +=
#EXTRALIBS += -lAlib

## Select the Sun audio driver (for SunOS)
#SYSTEM += -DSUN -D__USE_GNU -DAU_SUN
#SYSEXTRAS += sun_a.c
#EXTRAINCS += -I/usr/demo/SOUND
#EXTRALIBS += -L/usr/demo/SOUND -laudio

## Select the Sun audio driver (for Solaris)
#SYSTEM += -DSUN -DSOLARIS -DAU_SUN
#SYSEXTRAS += sun_a.c
#EXTRAINCS += -I/usr/demo/SOUND/include

## Select the DEC MMS audio server
#SYSTEM += -DDEC -DAU_DEC
#SYSEXTRAS += dec_a.c
#EXTRAINCS += -I/usr/include/mme
#EXTRALIBS += -lmme

########### User interface selection
#
# Select any combination by uncommenting and editing the appropriate
# lines.  If you don't have ncurses, slang, motif, _or_ Tcl/Tk, you'll
# have to make do with a non-interactive interface that just prints out
# messages.

# Select the ncurses full-screen interface
SYSTEM += -DIA_NCURSES
SYSEXTRAS += ncurs_c.c
EXTRAINCS += -I/usr/include/ncurses
EXTRALIBS += -lncurses

## Select the S-Lang full-screen interface
#SYSTEM += -DIA_SLANG
#SYSEXTRAS += slang_c.c
#EXTRAINCS += -I/usr/include/slang
#EXTRALIBS += -lslang

## Select the MOTIF interface
#SYSTEM += -DMOTIF
#SYSEXTRAS += motif_c.c motif_i.c motif_p.c
#EXTRAINCS += -I/usr/local/X11R5/include -L/usr/local/X11R5/lib
#EXTRAINCS += -I/usr/include/X11
#EXTRALIBS += -lXm -lXt -lX11
## Solaris needs libgen?
#EXTRALIBS += -lgen

# Select the Tcl/Tk interface
SYSTEM += -DTCLTK -DWISH=\"wishx\" -DTKPROGPATH=\"$(TCL_DIR)/tkmidity.tcl\"
SYSEXTRAS += tk_c.c
INST_TK = install.tk
#EXTRAINCS +=
#EXTRALIBS +=

########### Now check out the options in config.h

#########################################################################
# You shouldn't need to change anything beyond this line

.SUFFIXES: .c .h .ptcl .tcl .o .1 .txt .ps

VERSION = 0.2i
FNVERSION = 02i
SUPPVERSION = 0.2
FNSUPPVERSION = 02

PROJ = timidity
DIST = timidity-$(VERSION).tar.gz
DISTZIP = timid$(FNVERSION).zip
SDIST = timidity-lib-$(SUPPVERSION).tar.gz
SDISTZIP = tilib$(FNSUPPVERSION).zip

CFLAGS= $(DEBUGFLAGS) -DDEFAULT_PATH=\"$(TIMID_DIR)\" \
	-DTIMID_VERSION=\"$(VERSION)\" $(SYSTEM) $(EXTRAINCS)

########### All relevant files.. Anybody know autoconf?

# Sources needed for every installation
CSRCS = timidity.c common.c readmidi.c playmidi.c resample.c mix.c instrum.c \
        tables.c controls.c output.c filter.c \
        wave_a.c raw_a.c dumb_c.c

# Optional installation-specific sources
OPTSRCS = hpux_a.c linux_a.c sun_a.c dec_a.c win_a.c \
	ncurs_c.c slang_c.c tk_c.c \
	motif_c.c motif_i.c motif_p.c

# External utility sources
TOOLSRCS = bag.c wav2pat.c

# Tcl interface sources
TCLSRCS = tkmidity.ptcl tkpanel.tcl browser.tcl misc.tcl

# Things used by MSDOS/Windows
GETOPTSRCS = getopt.c timidity.ide timidity.mak

# Miscellaneous things to distribute in the source archive
MAKEFILES = Makefile my my.pl
BITMAPS = tkbitmaps/prev.xbm tkbitmaps/next.xbm tkbitmaps/play.xbm \
	tkbitmaps/stop.xbm tkbitmaps/pause.xbm tkbitmaps/quit.xbm \
	BITMAPS/back.xbm BITMAPS/fwd.xbm BITMAPS/next.xbm BITMAPS/pause.xbm \
	BITMAPS/prev.xbm BITMAPS/quit.xbm BITMAPS/restart.xbm \
	BITMAPS/timidity.xbm

# All headers
CHDRS = config.h common.h readmidi.h playmidi.h resample.h mix.h instrum.h \
        tables.h controls.h output.h filter.h motif.h

# Used in making the distribution archive
ALLSRCS = $(CSRCS) $(OPTSRCS) $(CHDRS) $(TOOLSRCS) $(TCLSRCS) $(MAKEFILES) \
	$(BITMAPS) $(GETOPTSRCS)

TCLF = tkmidity.tcl tkpanel.tcl browser.tcl misc.tcl
ALLTCLF = $(TCLF) tclIndex
BITMAPF = tkbitmaps/prev.xbm tkbitmaps/next.xbm tkbitmaps/play.xbm \
	tkbitmaps/stop.xbm tkbitmaps/pause.xbm tkbitmaps/quit.xbm \
	BITMAPS/timidity.xbm

MANPAGES = timidity.1
TXTPAGES = $(MANPAGES:.1=.txt)
PSPAGES = $(MANPAGES:.1=.ps)
DOCS = README README.tk README.DEC README.W32 \
	COPYING INSTALL CHANGES TODO FAQ $(MANPAGES) $(TXTPAGES) $(PSPAGES)

CONFIGF = timidity.cfg gsdrum.cfg gravis.cfg midia.cfg wowpats.cfg mt32.cfg

PATCHF = patch/acpiano.pat patch/nylongt2.pat
SAMPLEF = midi/cavatina.mid midi/impromptu.mid midi/malaguena.mid \
	  midi/test-decay.mid midi/test-panning.mid midi/test-scale.mid

# These get stuffed in the distribution archive
ALLDIST = $(DOCS) $(ALLSRCS) $(CONFIGF)

# These go in the support archive
SUPPDIST = $(PATCHF) $(SAMPLEF) README.lib

########### Rules

COBJS = $(CSRCS:.c=.o) $(SYSEXTRAS:.c=.o)

TOOLS = $(TOOLSRCS:.c=)

all: $(PROJ) $(TOOLS)

clean:
	rm -f $(COBJS) core

spotless: clean
	rm -f $(PROJ) $(TOOLS) $(TXTPAGES) $(PSPAGES) \
		$(DIST) $(SDIST) $(DIST).CONTENTS $(SDIST).CONTENTS \
		$(DISTZIP) $(SDISTZIP) $(DISTZIP).CONTENTS $(SDISTZIP).CONTENTS

.c.o:
	$(CC) $(CFLAGS) -c $<

$(PROJ): $(COBJS)
	$(CC) $(CFLAGS) -o $(PROJ) $(COBJS) $(EXTRALIBS) -lm

bag: bag.c
	$(CC) $(CFLAGS) -o bag bag.c

wav2pat: wav2pat.c
	$(CC) $(CFLAGS) -o wav2pat wav2pat.c

#depends depend dep:
#	$(CC) $(CFLAGS) -MM $(CSRCS) $(OPTSRCS) $(TOOLSRCS) > depends
#
#include depends

########### Installation targets

install:
	@echo
	@echo Please enter one of the commands below:
	@echo 
	@echo \"make install.bin\" - Install the executable in $(BIN_DIR).
	@echo \"make install.man\" - Install the manual page in $(MAN_DIR).
	@echo \"make install.lib\" - Install the library files in $(TIMID_DIR).
	@echo \"make install.all\" - All of the above.
	@echo

install.all: install.bin install.man install.lib

# install.bin: $(PROJ) Dumb make thinks it has to have $(COBJS) to install...
install.bin:
	mkdir -p $(BIN_DIR)
	$(INSTALL) -s -m 755 $(PROJ) $(TOOLS) $(BIN_DIR)

install.man:
	mkdir -p $(MAN_DIR)
	$(INSTALL) -m 644 $(MANPAGES) $(MAN_DIR)

install.lib: install.config install.patch $(INST_TK)

install.config: $(CONFIGF)
	mkdir -p $(TIMID_DIR)
	$(INSTALL) -m 644 $(CONFIGF) $(TIMID_DIR)

install.patch: $(PATCHF)
	mkdir -p $(TIMID_DIR)/patch
	$(INSTALL) -m 644 $(PATCHF) $(TIMID_DIR)/patch

install.tk: $(ALLTCLF)
	$(INSTALL) -m 644 $(ALLTCLF) $(TCL_DIR)
	mkdir -p $(TCL_DIR)/BITMAPS
	$(INSTALL) -m 644 $(BITMAPF) $(TCL_DIR)/BITMAPS

.ptcl.tcl:
	sed -e s@%TCL_DIR%@$(TCL_DIR)@g $< > $@

.1.txt:
	groff -man -Tlatin1 -P-b -P-u $< >$@

.1.ps:
	groff -man $< >$@

tclIndex: $(TCLF)
	echo 'auto_mkindex . *.tcl; exit' | wish

########## Some special targets

time:
	time ./$(PROJ) -idq -s32 -p32 -Or -o/dev/null \
	-c /usr/local/lib/timidity/timidity.cfg \
	$(HOME)/midi/jazz/jzdwing.mid.gz

dist: $(DIST) $(DISTZIP)

sdist: $(SDIST) $(SDISTZIP)

putup: $(DIST) $(DISTZIP)
	changes2html < CHANGES > changes.html
	faq2html < FAQ > faq.html
	cp /home/titoivon/public_html/timidity/index.html index.html
	tar czvf /home/titoivon/putup.tgz $(DIST) $(DIST).CONTENTS \
		changes.html faq.html timidity.txt index.html \
		INSTALL COPYING
	rm changes.html faq.html timidity.txt index.html

$(DIST) $(DISTZIP): $(ALLDIST)
	-rm -rf /tmp/timidity-$(VERSION) /tmp/timid$(FNVERSION)
	mkdir /tmp/timidity-$(VERSION)
	cp -aP $(ALLDIST) /tmp/timidity-$(VERSION)
	(cd /tmp && tar czvf $(DIST) timidity-$(VERSION))
	(cd /tmp && mv timidity-$(VERSION) timid$(FNVERSION) && \
		zip -r $(DISTZIP) timid$(FNVERSION))
	mv /tmp/$(DIST) /tmp/$(DISTZIP) .
	rm -rf /tmp/timid$(FNVERSION)
	tar tzvf $(DIST) >$(DIST).CONTENTS
	unzip -lv $(DISTZIP) >$(DISTZIP).CONTENTS

$(SDIST): $(SUPPDIST)
	tar czf $(SDIST) $(SUPPDIST)
	tar tzvf $(SDIST) >$(SDIST).CONTENTS
	zip $(SDISTZIP) $(SUPPDIST)
	unzip -lv $(SDISTZIP) >$(SDISTZIP).CONTENTS


########## End of Makefile
