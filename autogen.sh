#!/bin/sh

mv INSTALL INSTALL.autogen.bak
# ChangeLog is required for automake
git log > ChangeLog
autoreconf -f -i
mv INSTALL.autogen.bak INSTALL

./configure "$@"
