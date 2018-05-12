// SFZ Support Routines for TiMidity++
// Copyright (c) 2018 Starg <https://osdn.net/projects/timidity41>

#pragma once

#ifdef ENABLE_SFZ

#include "instrum.h"

void init_sfz(void);
void free_sfz(void);
Instrument *extract_sfz_file(char *sample_file);
void free_sfz_file(Instrument *ip);

#endif /* ENABLE_SFZ */
