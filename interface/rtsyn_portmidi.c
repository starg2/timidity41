/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2018 Masanao Izumo <iz@onicos.co.jp>
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


    rtsyn_portmidi.c
        Copyright (c) 2003 Keishi Suenaga <s_keishi@mutt.freemail.ne.jp>

    I referenced following sources.
        alsaseq_c.c - ALSA sequencer server interface
            Copyright (c) 2000  Takashi Iwai <tiwai@suse.de>
        readmidi.c

*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "interface.h"

#ifdef IA_PORTMIDISYN

#include <portmidi.h>
#include <porttime.h>

#include "timidity.h"
#include "common.h"
#include "controls.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "recache.h"
#include "output.h"
#include "aq.h"
#include "timer.h"

#include "rtsyn.h"
#include "rtsyn_internal.h"

static double pm_start_time;
static PmError pmerr;
static unsigned int InNum;
struct midistream_t {
    PortMidiStream *stream;
};
static struct midistream_t midistream[MAX_PORT];
#define PMBUFF_SIZE 8192
#define EXBUFF_SIZE 512
static PmEvent pmbuffer[PMBUFF_SIZE];
static uint8   sysexbuffer[EXBUFF_SIZE];

static void rtsyn_pm_get_port_list(void);
static int  rtsyn_pm_synth_start(void);
static void rtsyn_pm_synth_stop(void);
static int  rtsyn_pm_play_some_data(void);
static void rtsyn_pm_midiports_close(void);
static int  rtsyn_pm_buf_check(void);

void rtsyn_pm_setup(void)
{
    rtsyn.id_character    = 'P';
    rtsyn.get_port_list   = rtsyn_pm_get_port_list;
    rtsyn.synth_start     = rtsyn_pm_synth_start;
    rtsyn.synth_stop      = rtsyn_pm_synth_stop;
    rtsyn.play_some_data  = rtsyn_pm_play_some_data;
    rtsyn.midiports_close = rtsyn_pm_midiports_close;
    rtsyn.buf_check       = rtsyn_pm_buf_check;
}

static void rtsyn_pm_get_port_list(void)
{
    int i, j;
    char fmt[64];
    PmDeviceInfo *deviceinfo;

    snprintf(fmt, sizeof(fmt), "%%d:%%.%ds", MAX_RTSYN_PORTLIST_LEN - 1);

    pmerr = Pm_Initialize();
    if (pmerr != pmNoError) goto pmerror;

    InNum = Pm_CountDevices();
    j = 0;
    for (i = 1; i <=InNum && i <= MAX_RTSYN_PORTLIST_NUM; i++) {
        deviceinfo = (PmDeviceInfo*) Pm_GetDeviceInfo(i - 1);
        if (TRUE == deviceinfo->input) {
            snprintf(rtsyn_portlist[j], MAX_RTSYN_PORTLIST_LEN, fmt, i, deviceinfo->name);
            rtsyn_portlist[j][MAX_RTSYN_PORTLIST_LEN - 1] = '\0';
            j++;
        }
    }
    rtsyn_nportlist = j;
    Pm_Terminate();
    return;

pmerror:
    Pm_Terminate();
    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
              "PortMIDI error: %s", Pm_GetErrorText(pmerr));
    return;
}

static int rtsyn_pm_synth_start(void)
{
    int i;
    unsigned int port;

    port = 0;
    Pt_Start(1, NULL, NULL);
    pm_start_time = get_current_calender_time();
    pmerr = Pm_Initialize();
    if (pmerr != pmNoError) goto pmerror;
    for (port = 0; port < rtsyn_portnumber; port++) {
        PortMidiStream *stream;
        void *timeinfo;

        pmerr = Pm_OpenInput(&stream,
                             portID[port],
                             NULL,
                             (PMBUFF_SIZE),
                             Pt_Time,
                             NULL);
        midistream[port].stream = stream;
        if (pmerr != pmNoError) goto pmerror;
        pmerr = Pm_SetFilter(midistream[port].stream, PM_FILT_CLOCK);
        if (pmerr != pmNoError) goto pmerror;
    }
    return ~0;

pmerror:
    Pm_Terminate();
    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
              "PortMIDI error: %s", Pm_GetErrorText(pmerr));
    return 0;
}

static void rtsyn_pm_synth_stop(void)
{
    rtsyn_stop_playing();
//  play_mode->close_output();
    rtsyn_midiports_close();
    return;

pmerror:
    Pm_Terminate();
    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
              "PortMIDI error: %s", Pm_GetErrorText(pmerr));
    return;
}

static void rtsyn_pm_midiports_close(void)
{
    unsigned int port;

    for (port = 0; port < rtsyn_portnumber; port++) {
        pmerr = Pm_Abort(midistream[port].stream);
//      if (pmerr != pmNoError) goto pmerror;
    }
    Pm_Terminate();
    Pt_Stop();
}

static int rtsyn_pm_play_some_data(void)
{
    PmMessage pmmsg;
    int played;
    int j, port, exlen, data, shift;
    int32 pmlength, pmbpoint;
    double event_time;

    played = 0;
    sleep(0);
    for (port = 0; port < rtsyn_portnumber; port++) {
        pmerr = Pm_Read(midistream[port].stream, pmbuffer, PMBUFF_SIZE);
        if (pmerr < 0) goto pmerror;
        pmlength = pmerr;
        pmbpoint = 0;
        while (pmbpoint < pmlength) {
            played = ~0;
            pmmsg = pmbuffer[pmbpoint].message;
            event_time = ((double) pmbuffer[pmbpoint].timestamp) / 1000.0 + pm_start_time;
            pmbpoint++;
            if (1 == rtsyn_play_one_data(port, pmmsg, event_time)) {
                j = 0;
                sysexbuffer[j++] = 0xf0;
                for (shift = 8, data = 0; shift < 32 && (data != 0x0f7); shift += 8) {
                    data = (pmmsg >> shift) & 0x0FF;
                    sysexbuffer[j++] = data;
                }
                if (data != 0x0f7) {
                    if (pmbpoint >= pmlength) {
                        do {
                            pmerr = Pm_Read(midistream[port].stream, pmbuffer, PMBUFF_SIZE);
                            if (pmerr < 0) { goto pmerror; }
                            sleep(0);
                        } while (pmerr == 8);
                        pmlength = pmerr;
                        pmbpoint = 0;
                    }
                    while (j < EXBUFF_SIZE - 4) {
                        for (shift = 0, data = 0; shift < 32 && (data != 0x0f7); shift += 8) {
                            data = (pmbuffer[pmbpoint].message >> shift) & 0x0FF;
                            sysexbuffer[j++] = data;
                        }
                        pmbpoint++;
                        if (data == 0x0f7) break;
                        if (pmbpoint >= pmlength) {
                            do {
                                pmerr = Pm_Read(midistream[port].stream, pmbuffer, PMBUFF_SIZE);
                                if (pmerr < 0) { goto pmerror; }
                                sleep(0);
                            } while (pmerr == 0);
                            pmlength = pmerr;
                            pmbpoint = 0;
                        }
                    }
                }
                exlen = j;
                rtsyn_play_one_sysex(sysexbuffer, exlen, event_time);
            }
        }
    }
    return played;

pmerror:
    Pm_Terminate();
    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
              "PortMIDI error: %s", Pm_GetErrorText(pmerr));
    return 0;
}

static int rtsyn_pm_buf_check(void)
{
    return 0;
}

#endif /* IA_PORTMIDISYN */

