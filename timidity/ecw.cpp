// ECW Support Routines for TiMidity++
// Copyright (c) 2018-2021 Starg <https://osdn.net/projects/timidity41>

extern "C"
{
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "timidity.h"
#include "common.h"
#include "controls.h"
#include "output.h"
#include "instrum.h"
#include "playmidi.h"
#include "tables.h"

#include "ecw.h"
}

extern "C" void init_ecw(void)
{
}

extern "C" void free_ecw(void)
{
}

extern "C" Instrument *extract_ecw_file(char *sample_file, uint8 font_bank, int8 font_preset, int8 font_keynote)
{
    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: error: ECW support is not implemented.", sample_file);
    return 0;
}

extern "C" void free_ecw_file(Instrument *ip)
{
}
