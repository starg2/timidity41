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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    rtsyn_common.c
        Copyright (c) 2003-2005  Keishi Suenaga <s_keishi@mutt.freemail.ne.jp>

    I referenced following sources.
        alsaseq_c.c - ALSA sequencer server interface
            Copyright (c) 2000  Takashi Iwai <tiwai@suse.de>
        readmidi.c
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "interface.h"

#if defined(IA_NPSYN) || defined(IA_PORTMIDISYN) || defined(IA_WINSYN) || defined(IA_W32G_SYN)

#ifdef __POCC__
#include <sys/types.h>
#endif //for off_t

#include <stdio.h>

#include <stdarg.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#endif
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <math.h>
#include <signal.h>

#include "server_defs.h"

#ifdef __W32__
#include <windows.h>
#endif

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
#include "miditrace.h"

#include "rtsyn.h"
#include "rtsyn_internal.h"

extern int32 current_sample;
extern void reset_midi(int playing);

int seq_quit = ~0;

int rtsyn_system_mode = DEFAULT_SYSTEM_MODE;
double rtsyn_latency = RTSYN_LATENCY;   //ratency (sec)
int32 rtsyn_start_sample;
int rtsyn_sample_time_mode = 0;
int rtsyn_skip_aq = 0;
double max_compute = RTSYN_LATENCY;     // play_event() ÇÃ compute_data() Ç≈åvéZÇãñÇ∑ç≈ëÂéûä‘

int rtsyn_portnumber = 1;
unsigned int portID[MAX_PORT];
char rtsyn_portlist[MAX_RTSYN_PORTLIST_NUM][MAX_RTSYN_PORTLIST_LEN];
int rtsyn_nportlist;

static int rtsyn_played = 0;
static double rtsyn_start_time;
static double last_event_time;
static double last_calc_time;
static int set_time_first = 2;

RTSynInternal rtsyn;

//acitive sensing
static int active_sensing_flag = 0;
static double active_sensing_time = 0;

//timer interrupt

/*
#define EX_RESET_NO 7
static uint8 sysex_resets[EX_RESET_NO][11] = {
        '\xf0','\x7e','\x7f','\x09','\x00','\xf7','\x00','\x00','\x00','\x00','\x00',
        '\xf0','\x7e','\x7f','\x09','\x01','\xf7','\x00','\x00','\x00','\x00','\x00',
        '\xf0','\x7e','\x7f','\x09','\x03','\xf7','\x00','\x00','\x00','\x00','\x00',
        '\xf0','\x41','\x10','\x42','\x12','\x40','\x00','\x7f','\x00','\x41','\xf7',
        '\xf0','\x41','\x10','\x42','\x12','\x00','\x00','\x7f','\x00','\x01','\xf7',
        '\xf0','\x41','\x10','\x42','\x12','\x00','\x00','\x7f','\x01','\x00','\xf7',
        '\xf0','\x43','\x10','\x4c','\x00','\x00','\x7E','\x00','\xf7','\x00','\x00' };
*/
/*
#define EX_RESET_NO 9
static uint8 sysex_resets[EX_RESET_NO][11] = {
        '\xf0','\x7e','\x7f','\x09','\x00','\xf7','\x00','\x00','\x00','\x00','\x00', //gm off
        '\xf0','\x7e','\x7f','\x09','\x01','\xf7','\x00','\x00','\x00','\x00','\x00', //gm1
        '\xf0','\x7e','\x7f','\x09','\x02','\xf7','\x00','\x00','\x00','\x00','\x00', //gm off
        '\xf0','\x7e','\x7f','\x09','\x03','\xf7','\x00','\x00','\x00','\x00','\x00', //gm2
        '\xf0','\x41','\x10','\x42','\x12','\x40','\x00','\x7f','\x00','\x41','\xf7', //GS
        '\xf0','\x41','\x10','\x42','\x12','\x40','\x00','\x7f','\x7f','\x41','\xf7', //GS off
        '\xf0','\x41','\x10','\x42','\x12','\x00','\x00','\x7f','\x00','\x01','\xf7', //88
        '\xf0','\x41','\x10','\x42','\x12','\x00','\x00','\x7f','\x01','\x00','\xf7', //88
        '\xf0','\x43','\x10','\x4c','\x00','\x00','\x7E','\x00','\xf7','\x00','\x00'  //XG on
        };
*/

void rtsyn_seq_set_time(MidiEvent *ev, double event_time);

void rtsyn_gm_reset(void)
{
    MidiEvent ev;

    rtsyn_server_reset();
    ev.type = ME_RESET;
    ev.a = GM_SYSTEM_MODE;
    rtsyn_play_event(&ev);
}

void rtsyn_gs_reset(void)
{
    MidiEvent ev;

    rtsyn_server_reset();
    ev.type = ME_RESET;
    ev.a = GS_SYSTEM_MODE;
    rtsyn_play_event(&ev);
}

void rtsyn_xg_reset(void)
{
    MidiEvent ev;

    rtsyn_server_reset();
    ev.type = ME_RESET;
    ev.a = XG_SYSTEM_MODE;
    ev.time = 0;
    rtsyn_play_event(&ev);
}

void rtsyn_gm2_reset(void)
{
    MidiEvent ev;

    rtsyn_server_reset();
    ev.type = ME_RESET;
    ev.a = GM2_SYSTEM_MODE;
    ev.time = 0;
    rtsyn_play_event(&ev);
}

void rtsyn_sd_reset(void)
{
    MidiEvent ev;

    rtsyn_server_reset();
    ev.type = ME_RESET;
    ev.a = SD_SYSTEM_MODE;
    ev.time = 0;
    rtsyn_play_event(&ev);
}

void rtsyn_kg_reset(void)
{
    MidiEvent ev;

    rtsyn_server_reset();
    ev.type = ME_RESET;
    ev.a = KG_SYSTEM_MODE;
    ev.time = 0;
    rtsyn_play_event(&ev);
}

void rtsyn_cm_reset(void)
{
    MidiEvent ev;

    rtsyn_server_reset();
    ev.type = ME_RESET;
    ev.a = CM_SYSTEM_MODE;
    ev.time = 0;
    rtsyn_play_event(&ev);
}

void rtsyn_normal_reset(void)
{
    MidiEvent ev;

    rtsyn_server_reset();
    ev.type = ME_RESET;
    ev.a = rtsyn_system_mode;
    rtsyn_play_event(&ev);
}

void rtsyn_gm_modeset(void)
{
    MidiEvent ev;

    rtsyn_server_reset();
    rtsyn_system_mode = GM_SYSTEM_MODE;
    ev.type = ME_RESET;
    ev.a = rtsyn_system_mode;
    rtsyn_play_event(&ev);
    change_system_mode(rtsyn_system_mode);
    reset_midi(1);
}

void rtsyn_gs_modeset(void)
{
    MidiEvent ev;

    rtsyn_server_reset();
    rtsyn_system_mode = GS_SYSTEM_MODE;
    ev.type = ME_RESET;
    ev.a = rtsyn_system_mode;
    rtsyn_play_event(&ev);
    change_system_mode(rtsyn_system_mode);
    reset_midi(1);
}

void rtsyn_xg_modeset(void)
{
    MidiEvent ev;

    rtsyn_server_reset();
    rtsyn_system_mode = XG_SYSTEM_MODE;
    ev.type = ME_RESET;
    ev.a = rtsyn_system_mode;
    rtsyn_play_event(&ev);
    change_system_mode(rtsyn_system_mode);
    reset_midi(1);
}

void rtsyn_gm2_modeset(void)
{
    MidiEvent ev;

    rtsyn_server_reset();
    rtsyn_system_mode = GM2_SYSTEM_MODE;
    ev.type = ME_RESET;
    ev.a = rtsyn_system_mode;
    rtsyn_play_event(&ev);
    change_system_mode(rtsyn_system_mode);
    reset_midi(1);
}

void rtsyn_sd_modeset(void)
{
    MidiEvent ev;

    rtsyn_server_reset();
    rtsyn_system_mode = SD_SYSTEM_MODE;
    ev.type = ME_RESET;
    ev.a = rtsyn_system_mode;
    rtsyn_play_event(&ev);
    change_system_mode(rtsyn_system_mode);
    reset_midi(1);
}

void rtsyn_kg_modeset(void)
{
    MidiEvent ev;

    rtsyn_server_reset();
    rtsyn_system_mode = KG_SYSTEM_MODE;
    ev.type = ME_RESET;
    ev.a = rtsyn_system_mode;
    rtsyn_play_event(&ev);
    change_system_mode(rtsyn_system_mode);
    reset_midi(1);
}

void rtsyn_cm_modeset(void)
{
    MidiEvent ev;

    rtsyn_server_reset();
    rtsyn_system_mode = CM_SYSTEM_MODE;
    ev.type = ME_RESET;
    ev.a = rtsyn_system_mode;
    rtsyn_play_event(&ev);
    change_system_mode(rtsyn_system_mode);
    reset_midi(1);
}

void rtsyn_normal_modeset(void)
{
    MidiEvent ev;

    rtsyn_server_reset();
    rtsyn_system_mode = DEFAULT_SYSTEM_MODE;
    ev.type = ME_RESET;
    ev.a = rtsyn_system_mode;
    rtsyn_play_event(&ev);
    change_system_mode(rtsyn_system_mode);
    reset_midi(1);
}

double rtsyn_set_latency(double latency)
{
    if (latency < 1.0 / TICKTIME_HZ * 4.0)
        latency = 1.0 / TICKTIME_HZ * 4.0;
    rtsyn_latency = latency;
    return latency;
}

void rtsyn_set_skip_aq(int flg)
{
    rtsyn_skip_aq = flg ? 1 : 0;
}

void rtsyn_init(void)
{
    int i, j;
    MidiEvent ev;
    /* set constants */
    opt_realtime_playing = 1; /* Enable loading patch while playing */
    allocate_cache_size = 0; /* Don't use pre-calclated samples */
    auto_reduce_polyphony = 0;
    opt_sf_close_each_file = 0;

    if (play_mode->flag & PF_PCM_STREAM) {
        play_mode->extra_param[1] = aq_calc_fragsize();
        ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
                  "requesting fragment size: %d",
                  play_mode->extra_param[1]);
    }
    if (ctl->id_character != 'N')
    {
        if (rtsyn_skip_aq) // c212 add
            aq_set_soft_queue(0.0, 0.0); // skip audio queue
        else
            aq_set_soft_queue(rtsyn_latency, 0.0);
    }
    max_compute = (double) stream_max_compute * DIV_1000; //rtsyn_latency * 1000.0;
    max_compute = (rtsyn_latency > max_compute) ? rtsyn_latency : max_compute;
    i = current_keysig + ((current_keysig < 8) ? 7 : -9), j = 0;
    while (i != 7)
        i += (i < 7) ? 5 : -7, j++;
    j += note_key_offset, j -= floor(j * DIV_12) * 12;
    current_freq_table = j;

    if (play_mode && play_mode->acntl)
        play_mode->acntl(PM_REQ_PLAY_END, NULL);
    if (play_mode && play_mode->close_output)
        play_mode->close_output();

    current_file_info = get_midi_file_info("Output.mid", 1);

    if (play_mode && play_mode->open_output)
        play_mode->open_output();

    aq_setup();

    if (play_mode && play_mode->acntl)
        play_mode->acntl(PM_REQ_PLAY_START, NULL);

    rtsyn_reset();
    rtsyn_system_mode = DEFAULT_SYSTEM_MODE;
    change_system_mode(rtsyn_system_mode);

    reset_midi(0);
}

void rtsyn_close(void)
{
    rtsyn_stop_playing();
    if (play_mode && play_mode->acntl)
        play_mode->acntl(PM_REQ_PLAY_END, NULL);
    free_instruments(0);
    playmidi_stream_free();
    free_global_mblock();
}

void rtsyn_play_event_sample(MidiEvent *ev, int32 event_sample_time)
{
    ev->time = event_sample_time;
    play_event(ev);
    if (rtsyn_sample_time_mode != 1)
        aq_fill_nonblocking();
}

void rtsyn_play_event_time(MidiEvent *ev, double event_time)
{
    int gch;
    double current_event_time, buf_time;
    MidiEvent nev;

    if ((event_time - last_event_time) > max_compute) {
        kill_all_voices();
        current_sample = (double)(play_mode->rate) * get_current_calender_time() + 0.5;
        rtsyn_start_time = get_current_calender_time();
        rtsyn_start_sample = current_sample;
        last_event_time = rtsyn_start_time;
    } else {
        nev.type = ME_NONE;
        if ((event_time - last_event_time) > 1.0 / (double) TICKTIME_HZ) {
            buf_time = last_event_time + 1.0 / (double) TICKTIME_HZ;
            rtsyn_seq_set_time(&nev, buf_time);
            play_event(&nev);
            aq_fill_nonblocking();

            while (event_time > buf_time + 1.0 / (double) TICKTIME_HZ) {
                buf_time = buf_time + 1.0 / (double) TICKTIME_HZ;
                rtsyn_seq_set_time(&nev, buf_time);
                play_event(&nev);
                aq_fill_nonblocking();
            }
        }
        gch = GLOBAL_CHANNEL_EVENT_TYPE(ev->type);
        if (gch || !IS_SET_CHANNELMASK(quietchannels, ev->channel)) {
            rtsyn_seq_set_time(ev, event_time);
            play_event(ev);
            aq_fill_nonblocking();
            last_event_time = (event_time  > last_event_time) ? event_time : last_event_time;
        }
    }
    rtsyn_played = 1;
}

void rtsyn_play_event(MidiEvent *ev)
{
    rtsyn_play_event_time(ev, get_current_calender_time());
}

void rtsyn_wot_reset(void)
{
    int i;
    kill_all_voices();
    if (free_instruments_afterwards) {
        free_instruments(0);
    }
    aq_flush(1);
//  play_mode->close_output();      // PM_REQ_PLAY_START wlll called in playmidi_stream_init()
//  play_mode->open_output();       // but w32_a.c does not have it.
    play_mode->acntl(PM_REQ_FLUSH, NULL);

    readmidi_read_init();
    playmidi_stream_init();
    change_system_mode(rtsyn_system_mode);
    reset_midi(1);
    reduce_voice_threshold = 0; // * Disable auto reduction voice *
///r
    reduce_quality_threshold = 0;
    reduce_polyphony_threshold = 0;
    auto_reduce_polyphony = 0;
}

void rtsyn_tmr_reset(void)
{
    if (ctl->id_character == 'N')
        current_sample = 0;
    rtsyn_start_time = get_current_calender_time();
    rtsyn_start_sample = current_sample;
    last_event_time = rtsyn_start_time + rtsyn_latency;
    last_calc_time = rtsyn_start_time;
}

void rtsyn_reset(void)
{
    rtsyn_wot_reset();
    rtsyn_tmr_reset();
}

void rtsyn_server_reset(void)
{
    rtsyn_wot_reset();
    if (rtsyn_sample_time_mode != 1) {
        rtsyn_tmr_reset();
    }
}

void rtsyn_stop_playing(void)
{
    if (upper_voices) {
        const double tail = (rtsyn_latency > 0.1) ? 0.1 : rtsyn_latency;
        MidiEvent ev;
        ev.type = ME_EOT;
        ev.time = 0;
        ev.a = 0;
        ev.b = 0;
        rtsyn_play_event_time(&ev, tail + get_current_calender_time());
        sleep(tail * 10.0);
//      rtsyn_play_event_time(&ev, rtsyn_latency + get_current_calender_time());
//      sleep(rtsyn_latency * 1000.0);
        aq_flush(0);
    }
    trace_flush();
}

void rtsyn_seq_set_time(MidiEvent *ev, double event_time)
{
    double currenttime, time_div;

    time_div = event_time  - rtsyn_start_time;
    ev->time = rtsyn_start_sample
             + (int32)((double)(play_mode->rate) * time_div + 0.5);
}

void rtsyn_play_calculate(void)
{
    MidiEvent ev;
    double currenet_event_time, current_time;

    current_time = get_current_calender_time();
    currenet_event_time = current_time + rtsyn_latency;

    if ((rtsyn_played == 0 && currenet_event_time > last_calc_time + 1.0 / (double) TICKTIME_HZ) /* event buffer is empty */
        ||  (current_time + 1.0 / (double) TICKTIME_HZ * 2.0 > last_event_time) /* near miss */
    ) {
        ev.type = ME_NONE;
        rtsyn_play_event_time(&ev, currenet_event_time);
        last_calc_time = currenet_event_time;
    }
    rtsyn_played = 0;

    if (active_sensing_flag==~0 && (get_current_calender_time() > active_sensing_time + 0.5)) {
        //normaly acitive sensing expiering time is 330ms(>300ms) but this loop is heavy
        play_mode->close_output();
        play_mode->open_output();
        ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Active Sensing Expired");
        rtsyn_server_reset();
        active_sensing_flag = 0;
    }
}

int rtsyn_play_one_data(int port, uint32 dwParam1, double event_time)
{
    MidiEvent ev;

    if (rtsyn_sample_time_mode != 1) {
        event_time += rtsyn_latency;
    }
    ev.type = ME_NONE;
    ev.channel = dwParam1 & 0x0000000f;
    ev.channel = ev.channel + (port << 4);
    ev.a = (dwParam1 >> 8) & 0xff;
    ev.b = (dwParam1 >> 16) & 0xff;
    switch ((int)(dwParam1 & 0x000000f0)) {
    case 0x80:
        ev.type = ME_NOTEOFF;
//      rtsyn_play_event(&ev);
        break;
    case 0x90:
        ev.type = (ev.b) ? ME_NOTEON : ME_NOTEOFF;
//      rtsyn_play_event(&ev);
        break;
    case 0xa0:
        ev.type = ME_KEYPRESSURE;
//      rtsyn_play_event(&ev);
        break;
    case 0xb0:
        if (!convert_midi_control_change(ev.channel, ev.a, ev.b, &ev))
        ev.type = ME_NONE;
        break;
    case 0xc0:
        ev.type = ME_PROGRAM;
//      rtsyn_play_event(&ev);
        break;
    case 0xd0:
        ev.type = ME_CHANNEL_PRESSURE;
//      rtsyn_play_event(&ev);
        break;
    case 0xe0:
        ev.type = ME_PITCHWHEEL;
//      rtsyn_play_event(&ev);
        break;
    case 0xf0:
#ifdef IA_PORTMIDISYN
        if ((dwParam1 & 0x000000ff) == 0xf0) {
            //SysEx
            return 1;
        }
#endif
        if ((dwParam1 & 0x000000ff) == 0xf2) {
            ev.type = ME_PROGRAM;
//          rtsyn_play_event(&ev);
        }
#if 0
        if ((dwParam1 & 0x000000ff) == 0xf1)
            //MIDI Time Code Qtr. Frame (not need)
            ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY, "MIDI Time Code Qtr");
        if ((dwParam1 & 0x000000ff) == 0xf3)
            //Song Select(Song #) (not need)
#endif
        if ((dwParam1 & 0x000000ff) == 0xf6) {
            //Tune request  but use to make TiMidity++  to calculate.
            if (rtsyn_sample_time_mode == 1) {
                ev.type = ME_NONE;
                rtsyn_play_event_sample(&ev, event_time);
                aq_fill_nonblocking();
                //aq_soft_flush();
            } else {
                //ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY, "Tune request");
            }
        }
#if 0
        if ((dwParam1 & 0x000000ff) == 0xf8)
            //Timing Clock (not need)
            ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY, "Timing Clock");
        if ((dwParam1 & 0x000000ff) == 0xfa)
            {}//Start
        if ((dwParam1 & 0x000000ff) == 0xfb)
            {}//Continue
        if ((dwParam1 & 0x000000ff) == 0xfc) {
            //Stop
            ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY, "Stop");
        }
#endif
        if ((dwParam1 & 0x000000ff) == 0xfe) {
            //Active Sensing
//          ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY, "Active Sensing");
            active_sensing_flag = ~0;
            active_sensing_time = get_current_calender_time();
        }
        if ((dwParam1 & 0x000000ff) == 0xff) {
            //System Reset  use for TiMidity++  timer  reset
            if (rtsyn_sample_time_mode == 1) {
                rtsyn_tmr_reset();
            } else {
                //ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY, "System Reset");
            }
        }
        break;
    default:
//      ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY, "Unsupported event %d", aevp->type);
        break;
    }
    if (ev.type != ME_NONE) {
        if (rtsyn_sample_time_mode != 1) {
            rtsyn_play_event_time(&ev, event_time);
        } else {
            rtsyn_play_event_sample(&ev, event_time);
        }
    }
    return 0;
}

void rtsyn_play_one_sysex(uint8 *sysexbuffer, int exlen, double event_time)
{
    int i, j, chk, ne;
    MidiEvent ev;
    MidiEvent evm[260];

    if (rtsyn_sample_time_mode != 1) {
        event_time += rtsyn_latency;
    }

    if (((int8) sysexbuffer[0] != '\xf0') && ((int8)sysexbuffer[0] != '\xf7')) return;

/* // this is bad check  someone send SysEx f0xxxxxxxxxxx without xf7 format.
    if ((((int8) sysexbuffer[0] != '\xf0') && ((int8)sysexbuffer[0] != '\xf7')) ||
    (((int8) sysexbuffer[0] == '\xf0') && ((int8)sysexbuffer[exlen - 1] != '\xf7'))) return;
*/

/*
    for (i = 0; i < EX_RESET_NO; i++) {
        chk = 0;
        for (j = 0; (j < exlen) && (j < 11); j++) {
            if (chk == 0 && sysex_resets[i][j] != sysexbuffer[j]) {
                chk = ~0;
            }
        }
        if (chk == 0) {
            rtsyn_server_reset();
        }
    }
*/
/*
        ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY, "SyeEx length=%x bytes ", exlen);
        for (i = 0; i < exlen; i++) {
            ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY, "%x ", sysexbuffer[i]);
        }
        ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY, "\n");
*/
    if (parse_sysex_event(sysexbuffer + 1, exlen - 1, &ev)) {
        if (ev.type == ME_RESET) rtsyn_server_reset();
        if (ev.type == ME_RESET && rtsyn_system_mode != DEFAULT_SYSTEM_MODE) {
            ev.a = rtsyn_system_mode;
            change_system_mode(rtsyn_system_mode);
            if (rtsyn_sample_time_mode != 1) {
                rtsyn_play_event_time(&ev, event_time);
            } else {
                rtsyn_play_event_sample(&ev, event_time);
            }
        } else {
            if (rtsyn_sample_time_mode != 1) {
                rtsyn_play_event_time(&ev, event_time);
            } else {
                rtsyn_play_event_sample(&ev, event_time);
            }
        }
    }
    if ((ne = parse_sysex_event_multi(sysexbuffer + 1, exlen - 1, evm)) != 0) {
        for (i = 0; i < ne; i++) {
            if (rtsyn_sample_time_mode != 1) {
                rtsyn_play_event_time(&(evm[i]), event_time);
            } else {
                rtsyn_play_event_sample(&(evm[i]), event_time);
            }
        }
    }
}

void rtsyn_print_port_list(void)
{
    int i;
    char fmt[64];

    snprintf(fmt, sizeof(fmt), "%%.%ds", MAX_RTSYN_PORTLIST_LEN - 1);

    ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
              "Listing Device drivers:" "Available Midi Input devices:" "%d", rtsyn_nportlist);

    for (i = 0; i < rtsyn_nportlist; i++) {
        ctl->cmsg(CMSG_WARNING, VERB_NORMAL,
                  fmt, rtsyn_portlist[i]);
    }
}

void rtsyn_get_port_list(void)
{
    rtsyn.get_port_list();
}

int rtsyn_synth_start(void)
{
    return rtsyn.synth_start();
}

void rtsyn_synth_stop(void)
{
    rtsyn.synth_stop();
}

int rtsyn_play_some_data(void)
{
    return rtsyn.play_some_data();
}

void rtsyn_midiports_close(void)
{
    rtsyn.midiports_close();
}

int rtsyn_buf_check(void)
{
    return rtsyn.buf_check();
}

#endif /* IA_NPSYN || IA_PORTMIDISYN || IA_WINSYN || IA_W32G_SYN */

