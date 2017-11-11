/*
TiMidity++ -- MIDI to WAVE converter and player
Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

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
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

wasapi_a.c

Functions to play sound using WASAPI (Windows 7+).

Written by Starg.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#ifdef __W32__
#include "interface.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <windows.h>

#ifdef AU_WASAPI

#include "timidity.h"
#include "output.h"

int WASAPIOpenOutputShared(void)
{
    return -1;
}

int WASAPIOpenOutputExclusive(void)
{
    return -1;
}

void WASAPICloseOutput(void)
{

}

int WASAPIOutputData(char* pData, int32 size)
{
    return -1;
}

int WASAPIACntl(int request, void* pArg)
{
    return -1;
}

int WASAPIDetect(void)
{
    return 0;
}

PlayMode wasapi_shared_play_mode = {
    DEFAULT_RATE,
    PE_16BIT | PE_SIGNED,
    PF_PCM_STREAM | PF_CAN_TRACE | PF_BUFF_FRAGM_OPT,
    -1,
    {32},
    "WASAPI (Shared)", 'x',
    NULL,
    &WASAPIOpenOutputShared,
    &WASAPICloseOutput,
    &WASAPIOutputData,
    &WASAPIACntl,
    &WASAPIDetect
};

PlayMode wasapi_exclusive_play_mode = {
    DEFAULT_RATE,
    PE_16BIT | PE_SIGNED,
    PF_PCM_STREAM | PF_CAN_TRACE | PF_BUFF_FRAGM_OPT,
    -1,
    {32},
    "WASAPI (Exclusive)", 'X',
    NULL,
    &WASAPIOpenOutputExclusive,
    &WASAPICloseOutput,
    &WASAPIOutputData,
    &WASAPIACntl,
    &WASAPIDetect
};

#endif /* AU_WASAPI */
