/* 
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999 Masanao Izumo <mo@goice.co.jp>
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

	Macintosh interface for TiMidity
	by T.Nogami	<t-nogami@happy.email.ne.jp>
	KINOSHITA, K.   <kino@krhm.jvc-victor.co.jp>
		
    mac_qt_a.c
    Macintosh QuickTime audio driver
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>

#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <Threads.h>
#include <QuickTimeComponents.h>

#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "output.h"
#include "controls.h"
#include "miditrace.h"
#include "wrd.h"

#include "mac_main.h"

static int do_event(void *);
static int open_output(void);
static void close_output(void);
static int32 current_samples(void);
static int acntl(int request, void *arg);

static unsigned long	start_tic;
static NoteAllocator	gNoteAllocator;

#define dmp mac_quicktime_play_mode

PlayMode dmp =
{
    DEFAULT_RATE, 0, PF_CAN_TRACE|PF_MIDI_EVENT,
    -1,
    {0,0,0,0,0},
    "QuickTime MIDI mode", 'q',
    "-",
    open_output,
    close_output,
    NULL,
    acntl
};

static void init_variable(void)
{
	start_tic = TickCount();
}

static int open_output(void)
{
	init_variable();
	// open the note allocator component
	gNoteAllocator = OpenDefaultComponent(kNoteAllocatorComponentType, 0);
	if(gNoteAllocator == NULL){
		close_output();
		return -1;
	}
	dmp.fd = 1; //normaly opened flag
	return 0;
}

static unsigned long current_tick(void)
{
	return TickCount() - start_tic;
}

static int32 current_samples(void)
{
    return (current_tick()*play_mode->rate + 30)/60;
}

static void close_output(void)
{
	if(dmp.fd == -1) return;
	if(gNoteAllocator != NULL)
		CloseComponent(gNoteAllocator);
	dmp.fd = -1;	//disabled
}

static void ctl_timestamp(void)
{
	long secs;
	CtlEvent ce;
	static int last_secs = -1;

	secs = (long)(current_tick() / 60);
	if(secs == last_secs)
		return;
	ce.type = CTLE_CURRENT_TIME;
	ce.v1 = last_secs = secs;
	ctl->event(&ce);
}

static int do_event(void *p)
{
	MidiEvent *ev = (MidiEvent *)p;
	int ch, rc = RC_NONE, i;
	static NoteChannel	note_channel[MAX_CHANNELS];
	static uint8 bank_lsb[MAX_CHANNELS], bank_msb[MAX_CHANNELS];
	static uint8 bend_sense[MAX_CHANNELS], master_tune[MAX_CHANNELS];
	static uint8 rpn_flag, rpn_addr[MAX_CHANNELS], nrpn_lsb[MAX_CHANNELS], nrpn_msb[MAX_CHANNELS];
	static Boolean prescan = 0;

	if(!prescan){
		for(ch = 0; ch < MAX_CHANNELS; ch++){
			if(note_channel[ch] != NULL){
				NADisposeNoteChannel(gNoteAllocator, note_channel[ch]);
				note_channel[ch] = NULL;
			}
			bank_lsb[ch] = 0;
			bank_msb[ch] = 0;
		}
		for(;; ev++){
			ch = ev->channel;
			if(ev->type == ME_PROGRAM){
				if(note_channel[ch] == NULL){
					long instrumentNumber;
					NoteRequest	myNoteRequest;

					if(ISDRUMCHANNEL(ch))
						instrumentNumber = kFirstDrumkit + ev->a + 1;
					else if(play_system_mode == GS_SYSTEM_MODE)
						instrumentNumber = kFirstGSInstrument + ((bank_msb[ch]+1)<<7) + ev->a;
					else
						instrumentNumber = kFirstGMInstrument + ev->a;
					myNoteRequest.info.flags = 0;
					myNoteRequest.info.reserved = 0;
					*(short *)(&myNoteRequest.info.polyphony) = EndianS16_NtoB(8);			// 8 voices poliphonic
					*(Fixed *)(&myNoteRequest.info.typicalPolyphony) = EndianU32_NtoB(0x00010000);
					NAStuffToneDescription(gNoteAllocator, instrumentNumber, &myNoteRequest.tone);
					NANewNoteChannel(gNoteAllocator, &myNoteRequest, &note_channel[ch]);
				}
			}
			else if(ev->type == ME_TONE_BANK_LSB){
				bank_lsb[ch] = ev->a;
			}
			else if(ev->type == ME_TONE_BANK_MSB){
				bank_msb[ch] = ev->a;
			}
			else if(ev->type == ME_RESET){
				play_system_mode = ev->a;
			}
			else if(ev->type == ME_EOT){
				prescan = 1;
				for(ch = 0; ch < MAX_CHANNELS; ch++){
					bank_lsb[ch] = 0;
					bank_msb[ch] = 0;
				}
				init_variable();
				break;
			}
		}
		return RC_NONE;
	}
	for(;;){
		static int timestamp = 1;
		long myDelay;
		
		if( (myDelay = ev->time - (current_tick()*play_mode->rate+30)/60) < 0 ){
			timestamp = 1;
			break;
		}
		if(timestamp && myDelay > (play_mode->rate>>2)){
			timestamp = 0;
			ctl_timestamp();
		}
		Delay(1, &myDelay);
		rc = check_apply_control();
		if(RC_IS_SKIP_FILE(rc)){
			prescan = 0;
			for(ch = 0; ch < MAX_CHANNELS; ch++){
				if(note_channel[ch] != NULL){
					NADisposeNoteChannel(gNoteAllocator, note_channel[ch]);
					note_channel[ch] = NULL;
				}
				bank_lsb[ch] = 0;
				bank_msb[ch] = 0;
			}
			return rc;
		}
	}
	ch = ev->channel;
	switch(ev->type)
	{
	case ME_NOTEON:
		NAPlayNote(gNoteAllocator, note_channel[ch], ev->a, ev->b);
		break;
	case ME_NOTEOFF:
		NAPlayNote(gNoteAllocator, note_channel[ch], ev->a, 0);
		break;
	case ME_PROGRAM:
		if(note_channel[ch] == NULL){
			long instrumentNumber;
			NoteRequest	myNoteRequest;

			if(ISDRUMCHANNEL(ch))
				instrumentNumber = kFirstDrumkit + ev->a + 1;
			else if(play_system_mode == GS_SYSTEM_MODE)
				instrumentNumber = kFirstGSInstrument + ((bank_msb[ch]+1)<<7) + ev->a;
			else
				instrumentNumber = kFirstGMInstrument + ev->a;
			myNoteRequest.info.flags = 0;
			myNoteRequest.info.reserved = 0;
			*(short *)(&myNoteRequest.info.polyphony) = EndianS16_NtoB(8);			// 8 voices poliphonic
			*(Fixed *)(&myNoteRequest.info.typicalPolyphony) = EndianU32_NtoB(0x00010000);
			NAStuffToneDescription(gNoteAllocator, instrumentNumber, &myNoteRequest.tone);
			NANewNoteChannel(gNoteAllocator, &myNoteRequest, &note_channel[ch]);
		}
		break;
	/* MIDI Events */
	case ME_KEYPRESSURE:
	case ME_CHANNEL_PRESSURE:
		NASetController(gNoteAllocator, note_channel[ch], kControllerAfterTouch, ev->a<<8);
		break;
	case ME_PITCHWHEEL:
		if(bend_sense[ch])
			NASetController(gNoteAllocator, note_channel[ch], kControllerPitchBend,
							((ev->a + (ev->b<<7) - 0x2000 + 2)>>2)/bend_sense[ch]*bend_sense[ch]);
		else
			NASetController(gNoteAllocator, note_channel[ch], kControllerPitchBend,
							((ev->a + (ev->b<<7) - 0x2000 + 2)>>2));
		break;
	/* Controls */
	case ME_TONE_BANK_LSB:
		bank_lsb[ch] = ev->a;
		break;
	case ME_TONE_BANK_MSB:
		bank_msb[ch] = ev->a;
		break;
	case ME_MODULATION_WHEEL:
		NASetController(gNoteAllocator, note_channel[ch], kControllerModulationWheel, ev->a<<8);
		break;
	case ME_BREATH:
		NASetController(gNoteAllocator, note_channel[ch], kControllerBreath, ev->a<<8);
		break;
	case ME_FOOT:
		NASetController(gNoteAllocator, note_channel[ch], kControllerFoot, ev->a<<8);
		break;
	case ME_BALANCE:
		NASetController(gNoteAllocator, note_channel[ch], kControllerBalance, ev->a<<8);
		break;
	case ME_MAINVOLUME:
		NASetController(gNoteAllocator, note_channel[ch], kControllerVolume, ev->a<<8);
		break;
	case ME_PAN:
		// kControllerPan 256-512
		NASetController(gNoteAllocator, note_channel[ch], kControllerPan, (ev->a<<1) + 256);
		ctl_mode_event(CTLE_PANNING, 1, ch, ev->a);
		break;
	case ME_EXPRESSION:
		NASetController(gNoteAllocator, note_channel[ch], kControllerExpression, ev->a<<8);
		ctl_mode_event(CTLE_EXPRESSION, 1, ch, ev->a);
		break;
	case ME_SUSTAIN:
		// kControllerSustain on/off only
		NASetController(gNoteAllocator, note_channel[ch], kControllerSustain, ev->a);
		ctl_mode_event(CTLE_SUSTAIN, 1, ch, ev->a >= 64);
		break;
	case ME_PORTAMENTO_TIME_MSB:
		// kControllerPortamentoTime
		NASetController(gNoteAllocator, note_channel[ch], kControllerPortamentoTime, ev->a);
		break;
	case ME_PORTAMENTO:
		NASetController(gNoteAllocator, note_channel[ch], kControllerPortamentoTime, ev->a);
		break;
	case ME_DATA_ENTRY_MSB:
		if(rpn_flag){
			if(rpn_addr[ch] == 0)			// pitchbend sensitivity
				bend_sense[ch] = ev->a;
			else if(rpn_addr[ch] == 1)		// master tuning (fine)
				master_tune[ch] |= ev->a;
			else if(rpn_addr[ch] == 2)		// master tuning (coarse)
				master_tune[ch] |= (ev->a<<7);
		}
		else {
		}
		break;
	case ME_REVERB_EFFECT:
		NASetController(gNoteAllocator, note_channel[ch], kControllerReverb, ev->a<<8);
		break;
	case ME_TREMOLO_EFFECT:
		NASetController(gNoteAllocator, note_channel[ch], kControllerTremolo, ev->a<<8);
		break;
	case ME_CHORUS_EFFECT:
		NASetController(gNoteAllocator, note_channel[ch], kControllerChorus, ev->a<<8);
		ctl_mode_event(CTLE_CHORUS_EFFECT, 1, ch, channel[ch].chorus_level);
		break;
	case ME_CELESTE_EFFECT:
		NASetController(gNoteAllocator, note_channel[ch], kControllerCeleste, ev->a<<8);
		break;
	case ME_PHASER_EFFECT:
		NASetController(gNoteAllocator, note_channel[ch], kControllerPhaser, ev->a<<8);
		break;
	case ME_RPN_INC:
		rpn_flag = 1;
		rpn_addr[ch]++;
		break;
	case ME_RPN_DEC:
		rpn_flag = 1;
		rpn_addr[ch]--;
		break;
	case ME_NRPN_LSB:
		rpn_flag = 0;
		nrpn_lsb[ch] = ev->a;
		break;
	case ME_NRPN_MSB:
		rpn_flag = 0;
		nrpn_msb[ch] = ev->a;
		break;
	case ME_RPN_LSB:
		rpn_flag = 1;
		rpn_addr[ch] = ev->a;
		break;
	case ME_RPN_MSB:
		break;
	case ME_ALL_SOUNDS_OFF:
		for(i = 0; i < 128; i++)
			NAPlayNote(gNoteAllocator, note_channel[ch], i, 0);
		break;
	case ME_RESET_CONTROLLERS:
		NAResetNoteChannel(gNoteAllocator, note_channel[ch]);
		break;
	case ME_ALL_NOTES_OFF:
		for(i = 0; i < 128; i++)
			NAPlayNote(gNoteAllocator, note_channel[ch], i, 0);
		break;
	case ME_MONO:
		break;
	case ME_POLY:
		break;
	case ME_SOSTENUTO:
		NASetController(gNoteAllocator, note_channel[ch], kControllerSostenuto, ev->a<<8);
		break;
	case ME_SOFT_PEDAL:
		NASetController(gNoteAllocator, note_channel[ch], kControllerSoftPedal, ev->a<<8);
		break;
	/* TiMidity Extensionals */
	case ME_RANDOM_PAN:
		break;
	case ME_SET_PATCH:
		break;
	case ME_TEMPO:
		break;
	case ME_CHORUS_TEXT:
	case ME_LYRIC:
	case ME_MARKER:
	case ME_INSERT_TEXT:
	case ME_TEXT:
	case ME_KARAOKE_LYRIC:
		i = ev->a | ((int)ev->b << 8);
		ctl_mode_event(CTLE_LYRIC, 1, i, 0);
		break;
	case ME_MASTER_VOLUME:
		NASetController(gNoteAllocator, NULL, kControllerMasterVolume, ev->a + (ev->b<<7));
		break;
	case ME_PATCH_OFFS:
		break;
	case ME_RESET:
		play_system_mode = ev->a;
		break;
	case ME_WRD:
		push_midi_trace2(wrd_midi_event,
						 ch, ev->a | (ev->b << 8));
		break;
	case ME_DRUMPART:
		break;
	case ME_KEYSHIFT:
		NASetController(gNoteAllocator, NULL, kControllerMasterTune, ev->a);
		break;
	case ME_NOTE_STEP:
		break;
	case ME_EOT:
		prescan = 0;
		for(ch = 0; ch < MAX_CHANNELS; ch++){
			if(note_channel[ch] != NULL){
				NADisposeNoteChannel(gNoteAllocator, note_channel[ch]);
				note_channel[ch] = NULL;
			}
			bank_lsb[ch] = 0;
			bank_msb[ch] = 0;
		}
		return RC_NONE;
	}

	return rc;
}

static int acntl(int request, void *arg)
{
    switch(request)
    {
      case PM_REQ_MIDI:
	return do_event(arg);
      case PM_REQ_GETSAMPLES:
	*(int32 *)arg = current_samples();
	return 0;
      case PM_REQ_DISCARD:
      case PM_REQ_FLUSH:
      case PM_REQ_PLAY_START:
	init_variable();
	return 0;
    }
    return -1;
}
