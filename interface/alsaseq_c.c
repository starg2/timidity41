/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2001 Masanao Izumo <mo@goice.co.jp>
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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


    alsaseq_c.c - ALSA sequencer server interface
        Copyright (c) 2000  Takashi Iwai <tiwai@suse.de>


    DESCRIPTION
    ===========

    This interface provides an ALSA sequencer interface which receives 
    events and plays it in real-time.  On this mode, TiMidity works
    purely as software (real-time) MIDI render.  There is no
    scheduling routine in this interface, since all scheduling is done 
    by ALSA seqeuncer core.

    For invoking ALSA sequencer interface, run timidity as folows:
      % timidity -iA -B2,8
    The fragment size can be adjustable.  The smaller number gives
    better real-time response.  Then timidity shows new port numbers
    which were newly created (128:0 and 128:1 below).
    ---------------------------------------
      % timidity -iA -B2,8
      TiMidity starting in ALSA server mode
      Opening sequencer port 128:0 128:1
    ---------------------------------------
    These ports can be connected with any other sequencer ports.
    For example, playing a MIDI file via pmidi (what's an overkill :-),
      % pmidi -p128:0 foo.mid
    If a midi file needs two ports, you may connect like this:
      % pmidi -p128:0,128:1 bar.mid
    Connecting from external MIDI keyboard may become like this:
      % aconnect 64:0 128:0
     
    The interface tries to reset process scheduling as SCHED_FIFO
    and as high priority as possible.  For enabling this feature,
    timidity must be invoked by root or installed with set-uid root.
    The SCHED_FIFO'd program shows much better real-time response.
    For example, without rescheduled, timidity may cause pauses at
    every time /proc is accessed.

    Timidity loads instruments dynamically at each time a PRM_CHANGE
    event is received.  This causes sometimes pauses during playback.
    It occurs often in the playback via pmidi.
    Furthermore, timidity resets the loaded instruments when the all
    subscriptions are disconnected.  Thus for keeping all loaded
    instruments also after playback is finished, you need to connect a
    dummy port (e.g. midi input port) to timidity port via aconnect:
      % aconnect 64:0 128:0

    If you prefer a bit more fancy visual output, use my tiny program, 
    aseqview.
      % aseqview -p2 &amp;
    Then connect two ports to timidity ports:
      % aconnect 129:0 128:0
      % aconnect 129:1 128:1
    The outputs ought to be redirected to 129:0,1 instead of 128:0,1.

    You may access to timidity also via OSS MIDI emulation on ALSA
    sequencer.  Take a look at /proc/asound/seq/oss for checking the
    device number to be accessed.
    ---------------------------------------
      % cat /proc/asound/seq/oss
      OSS sequencer emulation version 0.1.8
      ALSA client number 63
      ALSA receiver port 0
      ...
      midi 1: [TiMidity port 0] ALSA port 128:0
        capability write / opened none

      midi 2: [TiMidity port 1] ALSA port 128:1
        capability write / opened none
    ---------------------------------------
    In the case above, the MIDI devices 1 and 2 are assigned to
    timidity.  Now, play with playmidi:
      % playmidi -e -D1 foo.mid


    BUGS
    ====

    Well, well, they must be there..


    */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <signal.h>

#ifdef HAVE_SYS_SOUNDCARD_H
#include <sys/asoundlib.h>
#else
#include "server_defs.h"
#endif /* HAVE_SYS_SOUNDCARD_H */

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

#define NUM_PORTS	2	/* number of ports;
				 * this should be configurable via command line..
				 */

#define TICKTIME_HZ	100

struct seq_context {
	snd_seq_t *handle;	/* The snd_seq handle to /dev/snd/seq */
	int client;		/* The client associated with this context */
	int port[NUM_PORTS];	/* created sequencer ports */
	int fd;			/* The file descriptor */
	int used;		/* number of current connection */
	int active;		/* */
};

static int ctl_open(int using_stdin, int using_stdout);
static void ctl_close(void);
static int ctl_read(int32 *valp);
static int cmsg(int type, int verbosity_level, char *fmt, ...);
static void ctl_event(CtlEvent *e);
static void ctl_pass_playing_list(int n, char *args[]);

/**********************************/
/* export the interface functions */

#define ctl alsaseq_control_mode

ControlMode ctl=
{
    "ALSA sequencer interface", 'A',
    1,0,0,
    0,
    ctl_open,
    ctl_close,
    ctl_pass_playing_list,
    ctl_read,
    cmsg,
    ctl_event
};

static int32 event_time_offset;
static FILE *outfp;
static struct seq_context alsactx;
static struct seq_context *ctxp = &alsactx;

/*ARGSUSED*/
static int ctl_open(int using_stdin, int using_stdout)
{
	ctl.opened = 1;
	ctl.flags &= ~(CTLF_LIST_RANDOM|CTLF_LIST_SORT);
	if (using_stdout)
		outfp = stderr;
	else
		outfp = stdout;
	return 0;
}

static void ctl_close(void)
{
	if (!ctl.opened)
		return;
}

static int ctl_read(int32 *valp)
{
    return RC_NONE;
}

static int cmsg(int type, int verbosity_level, char *fmt, ...)
{
    va_list ap;

    if((type==CMSG_TEXT || type==CMSG_INFO || type==CMSG_WARNING) &&
       ctl.verbosity < verbosity_level)
	return 0;

    if(outfp == NULL)
	outfp = stderr;

    va_start(ap, fmt);
    vfprintf(outfp, fmt, ap);
    fputs(NLS, outfp);
    fflush(outfp);
    va_end(ap);

    return 0;
}

static void ctl_event(CtlEvent *e)
{
}

static RETSIGTYPE sig_timeout(int sig)
{
    signal(SIGALRM, sig_timeout); /* For SysV base */
    /* Expect EINTR */
}

static void doit(void);
static int do_sequencer(void);
static void server_reset(void);

static int set_realtime_priority(void)
{
	struct sched_param schp;

        /*
         * set the process to realtime privs
         */
        memset(&schp, 0, sizeof(schp));
        schp.sched_priority = sched_get_priority_max(SCHED_FIFO);

        if (sched_setscheduler(0, SCHED_FIFO, &schp) != 0) {
		printf("can't set sched_setscheduler - using normal priority\n");
                return -1;
        }
	printf("set SCHED_FIFO\n");
        return 0;
}

static void ctl_pass_playing_list(int n, char *args[])
{
	snd_seq_port_info_t pinfo;
	int i;

#ifdef SIGPIPE
	signal(SIGPIPE, SIG_IGN);    /* Handle broken pipe */
#endif /* SIGPIPE */

	printf("TiMidity starting in ALSA server mode\n");

	set_realtime_priority();

	if (snd_seq_open(&ctxp->handle, SND_SEQ_OPEN) < 0) {
		fprintf(stderr, "error in snd_seq_open\n");
		return;
	}
	ctxp->client = snd_seq_client_id(ctxp->handle);
	ctxp->fd = snd_seq_file_descriptor(ctxp->handle);

	printf("Opening sequencer port:");
	for (i = 0; i < NUM_PORTS; i++) {
		snd_seq_port_info_t pinfo;
		memset(&pinfo, 0, sizeof(pinfo));
		sprintf(pinfo.name, "TiMidity port %d", i);
		pinfo.capability = SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE;
		pinfo.type = SND_SEQ_PORT_TYPE_MIDI_GENERIC;
		strcpy(pinfo.group, SND_SEQ_GROUP_DEVICE);
		if (snd_seq_create_port(ctxp->handle, &pinfo) < 0) {
			fprintf(stderr, "error in snd_seq_create_simple_port\n");
			return;
		}
		ctxp->port[i] = pinfo.port;
		printf(" %d:%d", ctxp->client, ctxp->port[i]);
	}
	printf("\n");

	ctxp->used = 0;
	ctxp->active = 0;

	opt_realtime_playing = 2; /* Enable loading patch while playing */
	allocate_cache_size = 0; /* Don't use pre-calclated samples */
	/* aq_set_soft_queue(-1.0, 0.0); */
	alarm(0);
	signal(SIGALRM, sig_timeout);
	signal(SIGINT, safe_exit);
	signal(SIGTERM, safe_exit);

	play_mode->close_output();
	for (;;) {
		server_reset();
		doit();
	}
}

static void seq_play_event(MidiEvent *ev)
{
	ev->time = event_time_offset;
	play_event(ev);
}

static void stop_playing(void)
{
	if(upper_voices) {
		MidiEvent ev;
		ev.type = ME_EOT;
		ev.a = 0;
		ev.b = 0;
		seq_play_event(&ev);
		aq_flush(0);
	}
}

static void doit(void)
{
	for (;;) {
		while (snd_seq_event_input_pending(ctxp->handle, 1)) {
			if (do_sequencer())
				goto __done;
		}
		if (ctxp->active) {
			double fill_time;
			MidiEvent ev;

			aq_add(NULL, 0);
#if 0
			fill_time = high_time_at - (double)aq_filled() / play_mode->rate;
			if (fill_time <= 0)
				continue;
			event_time_offset += (int32)(fill_time * play_mode->rate);
#endif
			event_time_offset += play_mode->rate / TICKTIME_HZ;
			ev.time = event_time_offset;
			ev.type = ME_NONE;
			play_event(&ev);
		} else {
			fd_set rfds;
			FD_ZERO(&rfds);
			FD_SET(ctxp->fd, &rfds);
			if (select(ctxp->fd + 1, &rfds, NULL, NULL, NULL) < 0)
				goto __done;
		}
	}

__done:
	if (ctxp->active) {
		stop_playing();
		play_mode->close_output();
		free_instruments(0);
		free_global_mblock();
		ctxp->active = 0;
	}
}

static void server_reset(void)
{
	playmidi_stream_init();
	if (free_instruments_afterwards)
		free_instruments(0);
	reduce_voice_threshold = 0; /* Disable auto reduction voice */
	event_time_offset = 0;
}

#define NOTE_CHAN(ev)	((ev)->dest.port * 16 + (ev)->data.note.channel)
#define CTRL_CHAN(ev)	((ev)->dest.port * 16 + (ev)->data.control.channel)

static int do_sequencer(void)
{
	int n;
	MidiEvent ev;
	snd_seq_event_t *aevp;

	n = snd_seq_event_input(ctxp->handle, &aevp);
	if (n < 0 || aevp == NULL)
		return 0;

	switch(aevp->type) {
	case SND_SEQ_EVENT_NOTEON:
		ev.channel = NOTE_CHAN(aevp);
		ev.a       = aevp->data.note.note;
		ev.b       = aevp->data.note.velocity;
		if (ev.b == 0)
			ev.type = ME_NOTEOFF;
		else
			ev.type = ME_NOTEON;
		seq_play_event(&ev);
		break;

	case SND_SEQ_EVENT_NOTEOFF:
		ev.channel = NOTE_CHAN(aevp);
		ev.a       = aevp->data.note.note;
		ev.b       = aevp->data.note.velocity;
		ev.type = ME_NOTEOFF;
		seq_play_event(&ev);
		break;

	case SND_SEQ_EVENT_KEYPRESS:
		ev.channel = NOTE_CHAN(aevp);
		ev.a       = aevp->data.note.note;
		ev.b       = aevp->data.note.velocity;
		ev.type = ME_KEYPRESSURE;
		seq_play_event(&ev);
		break;

	case SND_SEQ_EVENT_PGMCHANGE:
		ev.channel = CTRL_CHAN(aevp);
		ev.a = aevp->data.control.value;
		ev.type = ME_PROGRAM;
		seq_play_event(&ev);
		break;

	case SND_SEQ_EVENT_CONTROLLER:
		if(convert_midi_control_change(CTRL_CHAN(aevp),
					       aevp->data.control.param,
					       aevp->data.control.value,
					       &ev))
			seq_play_event(&ev);
		break;

	case SND_SEQ_EVENT_PITCHBEND:
		ev.type    = ME_PITCHWHEEL;
		ev.channel = CTRL_CHAN(aevp);
		aevp->data.control.value += 0x2000;
		ev.a       = (aevp->data.control.value) & 0x7f;
		ev.b       = (aevp->data.control.value>>7) & 0x7f;
		seq_play_event(&ev);
		break;

	case SND_SEQ_EVENT_CHANPRESS:
		ev.type    = ME_CHANNEL_PRESSURE;
		ev.channel = CTRL_CHAN(aevp);
		ev.a       = aevp->data.control.value;
		seq_play_event(&ev);
		break;
		
	case SND_SEQ_EVENT_SYSEX:
		if (parse_sysex_event(aevp->data.ext.ptr + 1, aevp->data.ext.len - 1, &ev))
			seq_play_event(&ev);
		break;

	case SND_SEQ_EVENT_PORT_USED:
		if (ctxp->used == 0) {
			if (play_mode->open_output() < 0) {
				ctl.cmsg(CMSG_FATAL, VERB_NORMAL,
					 "Couldn't open %s (`%c')",
					 play_mode->id_name, play_mode->id_character);
				snd_seq_free_event(aevp);
				return 0;
			}
			ctxp->active = 1;
		}
		ctxp->used++;
		break;

	case SND_SEQ_EVENT_PORT_UNUSED:
		ctxp->used--;
		if (ctxp->used <= 0) {
			snd_seq_free_event(aevp);
			return 1; /* quit now */
		}
		break;
		
	default:
		/*printf("Unsupported event %d\n", aevp->type);*/
		break;
	}
	snd_seq_free_event(aevp);
	return 0;
}

/*
 * interface_<id>_loader();
 */
ControlMode *interface_A_loader(void)
{
    return &ctl;
}
