#ifndef ___CONTROLS_H_
#define ___CONTROLS_H_
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

    controls.h
*/

#define RC_IS_SKIP_FILE(rc) ((rc) == RC_QUIT || (rc) == RC_LOAD_FILE || \
			     (rc) == RC_NEXT || (rc) == RC_REALLY_PREVIOUS)

/* Return values for ControlMode.read */
#define RC_ERROR	-1
#define RC_NONE		0
#define RC_QUIT		1
#define RC_NEXT		2
#define RC_PREVIOUS	3 /* Restart this song at beginning, or the previous
			     song if we're less than a second into this one. */
#define RC_FORWARD	4
#define RC_BACK		5
#define RC_JUMP		6
#define RC_TOGGLE_PAUSE 7	/* Pause/continue */
#define RC_RESTART	8	/* Restart song at beginning */
#define RC_PAUSE	9	/* Really pause playing */
#define RC_CONTINUE	10	/* Continue if paused */
#define RC_REALLY_PREVIOUS 11	/* Really go to the previous song */
#define RC_CHANGE_VOLUME 12
#define RC_LOAD_FILE	13	/* Load a new midifile */
#define RC_TUNE_END	14	/* The tune is over, play it again sam? */
#define RC_KEYUP	15	/* Key up */
#define RC_KEYDOWN	16	/* Key down */
#define RC_SPEEDUP	17	/* Speed up */
#define RC_SPEEDDOWN	18	/* Speed down */
#define RC_VOICEINCR	19	/* Increase voices */
#define RC_VOICEDECR	20	/* Decrease voices */
#define RC_TOGGLE_DURMCHAN 21	/* Toggle drum channel */
#define RC_RELOAD	22	/* Reload & Play */
#define RC_TOGGLE_SNDSPEC 23	/* Open/Close Sound Spectrogram Window */
#define RC_CHANGE_REV_EFFB 24
#define RC_CHANGE_REV_TIME 25
#define RC_SYNC_RESTART 26
#define RC_TOGGLE_CTL_SPEANA 27

#define CMSG_INFO	0
#define CMSG_WARNING	1
#define CMSG_ERROR	2
#define CMSG_FATAL	3
#define CMSG_TRACE	4
#define CMSG_TIME	5
#define CMSG_TOTAL	6
#define CMSG_FILE	7
#define CMSG_TEXT	8

#define VERB_NORMAL	0
#define VERB_VERBOSE	1
#define VERB_NOISY	2
#define VERB_DEBUG	3
#define VERB_DEBUG_SILLY 4

#define CTLE_NOW_LOADING	1
#define CTLE_LOADING_DONE	2
#define CTLE_PLAY_START		3
#define CTLE_PLAY_END		4
#define CTLE_TEMPO		5
#define CTLE_METRONOME		6
#define CTLE_CURRENT_TIME	7
#define CTLE_NOTE		8
#define CTLE_MASTER_VOLUME	9
#define CTLE_PROGRAM		10
#define CTLE_VOLUME		11
#define CTLE_EXPRESSION		12
#define CTLE_PANNING		13
#define CTLE_SUSTAIN		14
#define CTLE_PITCH_BEND		15
#define CTLE_MOD_WHEEL		16
#define CTLE_CHORUS_EFFECT	17
#define CTLE_REVERB_EFFECT	18
#define CTLE_LYRIC		19
#define CTLE_REFRESH		20
#define CTLE_RESET		21
#define CTLE_SPEANA		22

typedef struct _CtlEvent {
    int type;		/* See above */
    long v1, v2, v3, v4;/* Event value */
} CtlEvent;

typedef struct {
  char *id_name, id_character;
  int verbosity, trace_playing, opened;

  int  (*open)(int using_stdin, int using_stdout);
  void (*close)(void);
  void (*pass_playing_list)(int number_of_files, char *list_of_files[]);
  int  (*read)(int32 *valp);
  int  (*cmsg)(int type, int verbosity_level, char *fmt, ...);
  void (*event)(CtlEvent *ev);	/* Control events */
} ControlMode;

extern ControlMode *ctl_list[], *ctl;
extern int dumb_error_count;

#endif /* ___CONTROLS_H_ */
