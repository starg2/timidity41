#!/bin/sh

echo "Building pdcurses (libpdcurses.a)"
make -C pdcurses/win32/ -f xc-mingw-w64-i686-win32.mak libs
