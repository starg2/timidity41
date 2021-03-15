// ECW Support Routines for TiMidity++
// Copyright (c) 2018 Starg <https://osdn.net/projects/timidity41>

#pragma once

#ifdef ENABLE_ECW

#include "instrum.h"

void init_ecw(void);
void free_ecw(void);
Instrument *extract_ecw_file(char *sample_file, uint8 font_bank, int8 font_preset, int8 font_keynote);
void free_ecw_file(Instrument *ip);

#endif
