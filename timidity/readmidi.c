/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2009 Masanao Izumo <iz@onicos.co.jp>
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
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#ifdef __POCC__
#include <sys/types.h>
#endif //for off_t
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <math.h>
#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "output.h"
#include "controls.h"
#include "strtab.h"
#include "memb.h"
#include "zip.h"
#include "arc.h"
#include "mod.h"
#include "wrd.h"
#include "tables.h"
#include "effect.h"
#include "sndfontini.h"

/* rcp.c */
int read_rcp_file(struct timidity_file *tf, char *magic0, char *fn);

/* mld.c */
extern int read_mfi_file(struct timidity_file *tf);
extern char *get_mfi_file_title(struct timidity_file *tf);

#define MAX_MIDI_EVENT ((MAX_SAFE_MALLOC_SIZE / sizeof(MidiEvent)) - 1)
#define MARKER_START_CHAR	'('
#define MARKER_END_CHAR		')'

static uint8 rhythm_part[2];	/* for GS */
static uint8 drum_setup_xg[16] = { 9, 9, 9, 9, 9, 9, 9, 9,
				   9, 9, 9, 9, 9, 9, 9, 9 };	/* for XG */

enum
{
    CHORUS_ST_NOT_OK = 0,
    CHORUS_ST_OK
};

#ifdef ALWAYS_TRACE_TEXT_META_EVENT
int opt_trace_text_meta_event = 1;
#else
int opt_trace_text_meta_event = 0;
#endif /* ALWAYS_TRACE_TEXT_META_EVENT */

int opt_default_mid = 65;
int opt_system_mid = 0;
int ignore_midi_error = 1;
ChannelBitMask quietchannels;
struct midi_file_info *current_file_info = NULL;
int readmidi_error_flag = 0;
int readmidi_wrd_mode = 0;
int play_system_mode = DEFAULT_SYSTEM_MODE;
#ifdef SUPPORT_LOOPEVENT
int opt_use_midi_loop_repeat = 0;
int32 opt_midi_loop_repeat = 3;
#endif /* SUPPORT_LOOPEVENT */

/* Mingw gcc3 and Borland C hack */
/* If these are not NULL initialized cause Hang up */
/* why ?  I dont know. (Keishi Suenaga) */
static MidiEventList *evlist=NULL, *current_midi_point=NULL;
static int32 event_count;
static MBlockList mempool;
static StringTable string_event_strtab = { 0 };
static int current_read_track;
static int karaoke_format, karaoke_title_flag;
static struct midi_file_info *midi_file_info = NULL;
static char **string_event_table = NULL;
static int    string_event_table_size = 0;
int    default_channel_program[256];
static MidiEvent timesig[256];
TimeSegment *time_segments = NULL;

void init_delay_status_gs(void);
void init_chorus_status_gs(void);
void init_reverb_status_gs(void);
void init_eq_status_gs(void);
void init_insertion_effect_gs(void);
void init_mfx_effect_sd(void);
void init_multi_eq_xg(void);
void init_multi_eq_sd(void);
static void init_all_effect_xg(void);
static void init_all_effect_sd(void);

/* MIDI ports will be merged in several channels in the future. */
int midi_port_number;

/* These would both fit into 32 bits, but they are often added in
   large multiples, so it's simpler to have two roomy ints */
static int32 sample_increment, sample_correction; /*samples per MIDI delta-t*/

#define SETMIDIEVENT(e, at, t, ch, pa, pb) \
    { (e).time = (at); (e).type = (t); \
      (e).channel = (uint8)(ch); (e).a = (uint8)(pa); (e).b = (uint8)(pb); }

#define MIDIEVENT(at, t, ch, pa, pb) \
    { MidiEvent event; SETMIDIEVENT(event, at, t, ch, pa, pb); \
      readmidi_add_event(&event); }

#if MAX_CHANNELS <= 16
#define MERGE_CHANNEL_PORT(ch) ((int)(ch))
#define MERGE_CHANNEL_PORT2(ch, port) ((int)(ch))
#else
#define MERGE_CHANNEL_PORT(ch) ((int)(ch) | (midi_port_number << 4))
#define MERGE_CHANNEL_PORT2(ch, port) ((int)(ch) | ((int)port << 4))
#endif

#define alloc_midi_event() \
    (MidiEventList *)new_segment(&mempool, sizeof(MidiEventList))

typedef struct _UserDrumset {
	int8 bank;
	int8 prog;
	int8 play_note;
	int8 level;
	int8 assign_group;
	int8 pan;
	int8 reverb_send_level;
	int8 chorus_send_level;
	int8 rx_note_off;
	int8 rx_note_on;
	int8 delay_send_level;
	int8 source_map;
	int8 source_prog;
	int8 source_note;
	struct _UserDrumset *next;
} UserDrumset;

UserDrumset *userdrum_first = (UserDrumset *)NULL;
UserDrumset *userdrum_last = (UserDrumset *)NULL; 

void init_userdrum();
UserDrumset *get_userdrum(int bank, int prog);
void recompute_userdrum_altassign(int bank,int group);

typedef struct _UserInstrument {
	int8 bank;
	int8 prog;
	int8 source_map;
	int8 source_bank;
	int8 source_prog;
	int8 vibrato_rate;
	int8 vibrato_depth;
	int8 cutoff_freq;
	int8 resonance;
	int8 env_attack;
	int8 env_decay;
	int8 env_release;
	int8 vibrato_delay;
	struct _UserInstrument *next;
} UserInstrument;

UserInstrument *userinst_first = (UserInstrument *)NULL;
UserInstrument *userinst_last = (UserInstrument *)NULL; 

void init_userinst();
UserInstrument *get_userinst(int bank, int prog);
void recompute_userinst(int bank, int prog, int elm);
void recompute_userinst_altassign(int bank,int group);

int32 readmidi_set_track(int trackno, int rewindp)
{
    current_read_track = trackno;
    memset(&chorus_status_gs.text, 0, sizeof(struct chorus_text_gs_t));
    if(karaoke_format == 1 && current_read_track == 2)
	karaoke_format = 2; /* Start karaoke lyric */
    else if(karaoke_format == 2 && current_read_track == 3)
	karaoke_format = 3; /* End karaoke lyric */
    midi_port_number = 0;

    if(evlist == NULL)
	return 0;
    if(rewindp)
	current_midi_point = evlist;
    else
    {
	/* find the last event in the list */
	while(current_midi_point->next != NULL)
	    current_midi_point = current_midi_point->next;
    }
    return current_midi_point->event.time;
}

void readmidi_add_event(MidiEvent *a_event)
{
    MidiEventList *newev;
    int32 at;

    if(event_count == MAX_MIDI_EVENT)
    {
	if(!readmidi_error_flag)
	{
	    readmidi_error_flag = 1;
	    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		      "Maxmum number of events is exceeded");
	}
	return;
    }
    event_count++;

    at = a_event->time;
    newev = alloc_midi_event();
    newev->event = *a_event;	/* assign by value!!! */
    if(at < 0)	/* for safety */
	at = newev->event.time = 0;

    if(at >= current_midi_point->event.time)
    {
	/* Forward scan */
	MidiEventList *next = current_midi_point->next;
	while (next && (next->event.time <= at))
	{
	    current_midi_point = next;
	    next = current_midi_point->next;
	}
	newev->prev = current_midi_point;
	newev->next = next;
	current_midi_point->next = newev;
	if (next)
	    next->prev = newev;
    }
    else
    {
	/* Backward scan -- symmetrical to the one above */
	MidiEventList *prev = current_midi_point->prev;
	while (prev && (prev->event.time > at)) {
	    current_midi_point = prev;
	    prev = current_midi_point->prev;
	}
	newev->prev = prev;
	newev->next = current_midi_point;
	current_midi_point->prev = newev;
	if (prev)
	    prev->next = newev;
    }
    current_midi_point = newev;
}

void readmidi_add_ctl_event(int32 at, int ch, int a, int b)
{
    MidiEvent ev;

    if(convert_midi_control_change(ch, a, b, &ev))
    {
	ev.time = at;
	readmidi_add_event(&ev);
    }
    else
	ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(Control ch=%d %d: %d)", ch, a, b);
}

char *readmidi_make_string_event(int type, char *string, MidiEvent *ev,
				 int cnv)
{
    char *text;
    size_t len;
    StringTableNode *st;
    int a, b;

    if(string_event_strtab.nstring == 0)
	put_string_table(&string_event_strtab, "", 0);
    else if(string_event_strtab.nstring == 0x7FFE)
    {
	SETMIDIEVENT(*ev, 0, type, 0, 0, 0);
	return NULL; /* Over flow */
    }
    a = (string_event_strtab.nstring & 0xff);
    b = ((string_event_strtab.nstring >> 8) & 0xff);

    len = strlen(string);
    if(cnv)
    {
	text = (char *)new_segment(&tmpbuffer, SAFE_CONVERT_LENGTH(len) + 1);
	code_convert(string, text + 1, SAFE_CONVERT_LENGTH(len), NULL, NULL);
    }
    else
    {
	text = (char *)new_segment(&tmpbuffer, len + 1);
	memcpy(text + 1, string, len);
	text[len + 1] = '\0';
    }

    st = put_string_table(&string_event_strtab, text, strlen(text + 1) + 1);
    reuse_mblock(&tmpbuffer);

    text = st->string;
    *text = type;
    SETMIDIEVENT(*ev, 0, type, 0, a, b);
    return text;
}

static char *readmidi_make_lcd_event(int type, const uint8 *data, MidiEvent *ev)
{
    char *text;
    int len;
    StringTableNode *st;
    int a, b, i;

    if(string_event_strtab.nstring == 0)
	put_string_table(&string_event_strtab, "", 0);
    else if(string_event_strtab.nstring == 0x7FFE)
    {
	SETMIDIEVENT(*ev, 0, type, 0, 0, 0);
	return NULL; /* Over flow */
    }
    a = (string_event_strtab.nstring & 0xff);
    b = ((string_event_strtab.nstring >> 8) & 0xff);

    len = 128;
    
	text = (char *)new_segment(&tmpbuffer, len + 2);

    for( i=0; i<64; i++){
	const char tbl[]= "0123456789ABCDEF";
	text[1+i*2  ]=tbl[data[i]>>4];
	text[1+i*2+1]=tbl[data[i]&0xF];
    }
    text[len + 1] = '\0';
    
    
    st = put_string_table(&string_event_strtab, text, strlen(text + 1) + 1);
    reuse_mblock(&tmpbuffer);

    text = st->string;
    *text = type;
    SETMIDIEVENT(*ev, 0, type, 0, a, b);
    return text;
}

/* Computes how many (fractional) samples one MIDI delta-time unit contains */
static void compute_sample_increment(int32 tempo, int32 divisions)
{
  double a;
  a = (double) (tempo) * (double) (play_mode->rate) * (65536.0/1000000.0) /
    (double)(divisions);

  sample_correction = (int32)(a) & 0xFFFF;
  sample_increment = (int32)(a) >> 16;

  ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Samples per delta-t: %d (correction %d)",
       sample_increment, sample_correction);
}

/* Read variable-length number (7 bits per byte, MSB first) */
static int32 getvl(struct timidity_file *tf)
{
    int32 l;
    int c;

    _set_errno(0);
    l = 0;

    /* 1 */
    if((c = tf_getc(tf)) == EOF)
	goto eof;
    if(!(c & 0x80)) return l | c;
    l = (l | (c & 0x7f)) << 7;

    /* 2 */
    if((c = tf_getc(tf)) == EOF)
	goto eof;
    if(!(c & 0x80)) return l | c;
    l = (l | (c & 0x7f)) << 7;

    /* 3 */
    if((c = tf_getc(tf)) == EOF)
	goto eof;
    if(!(c & 0x80)) return l | c;
    l = (l | (c & 0x7f)) << 7;

    /* 4 */
    if((c = tf_getc(tf)) == EOF)
	goto eof;
    if(!(c & 0x80)) return l | c;

    /* Error */
    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
	      "%s: Illigal Variable-length quantity format.",
	      current_filename);
    return -2;

  eof:
    if(errno)
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "%s: read_midi_event: %s",
		  current_filename, strerror(errno));
    else
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "Warning: %s: Too shorten midi file.",
		  current_filename);
    return -1;
}

static char *add_karaoke_title(char *s1, const char *s2)
{
    char *ks;
    size_t k1, k2;

    if(s1 == NULL)
	return safe_strdup(s2);

    k1 = strlen(s1);
    k2 = strlen(s2);
    if(k2 == 0)
	return s1;
    ks = (char *)safe_malloc(k1 + k2 + 2);
    memcpy(ks, s1, k1);
    ks[k1++] = ' ';
    memcpy(ks + k1, s2, k2 + 1);
    safe_free(s1);
	s1 = NULL;

    return ks;
}


/* Print a string from the file, followed by a newline. Any non-ASCII
   or unprintable characters will be converted to periods. */
static char *dumpstring(int type, int32 len, const char *label, int allocp,
			struct timidity_file *tf)
{
    char *si, *so;
    size_t s_maxlen = SAFE_CONVERT_LENGTH(len);
    size_t llen, solen;

    if(len <= 0)
    {
	ctl->cmsg(CMSG_TEXT, VERB_VERBOSE, "%s", label);
	return NULL;
    }

    si = (char *)new_segment(&tmpbuffer, len + 1);
    so = (char *)new_segment(&tmpbuffer, s_maxlen);

    if(len != tf_read(si, 1, len, tf))
    {
	reuse_mblock(&tmpbuffer);
	return NULL;
    }
    si[len]='\0';

    if(type == 1 &&
       current_file_info->format == 1 &&
       (strncmp(si, "@K", 2) == 0)) 
/* Karaoke string should be "@KMIDI KARAOKE FILE" */
	karaoke_format = 1;

    code_convert(si, so, s_maxlen, NULL, NULL);

    llen = strlen(label);
    solen = strlen(so);
    if(llen + solen >= MIN_MBLOCK_SIZE)
	so[MIN_MBLOCK_SIZE - llen - 1] = '\0';

    ctl->cmsg(CMSG_TEXT, VERB_VERBOSE, "%s%s", label, so);

    if(allocp)
    {
	so = safe_strdup(so);
	reuse_mblock(&tmpbuffer);
	return so;
    }
    reuse_mblock(&tmpbuffer);
    return NULL;
}

static uint16 gs_convert_master_vol(int vol)
{
    double v;

    if(vol >= 0x7f)
	return 0xffff;
    v = (double)vol * (0xffff/127.0);
    if(v >= 0xffff)
	return 0xffff;
    return (uint16)v;
}

static uint16 gm_convert_master_vol(uint16 v1, uint16 v2)
{
    return (((v1 & 0x7f) | ((v2 & 0x7f) << 7)) << 2) | 3;
}

static void check_chorus_text_start(void)
{
	struct chorus_text_gs_t *p = &(chorus_status_gs.text);
    if(p->status != CHORUS_ST_OK && p->voice_reserve[17] &&
       p->macro[2] && p->pre_lpf[2] && p->level[2] &&
       p->feed_back[2] && p->delay[2] && p->rate[2] &&
       p->depth[2] && p->send_level[2])
    {
	ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Chorus text start");
	p->status = CHORUS_ST_OK;
    }
}

struct ctl_chg_types {
    unsigned char mtype;
    int ttype;
} ctl_chg_list[] = {
      { 0, ME_TONE_BANK_MSB },
      { 1, ME_MODULATION_WHEEL },
      { 2, ME_BREATH },
      { 4, ME_FOOT },
      { 5, ME_PORTAMENTO_TIME_MSB },
      { 6, ME_DATA_ENTRY_MSB },
      { 7, ME_MAINVOLUME },
      { 8, ME_BALANCE },
      { 10, ME_PAN },
      { 11, ME_EXPRESSION },
      { 32, ME_TONE_BANK_LSB },
      { 37, ME_PORTAMENTO_TIME_LSB },
      { 38, ME_DATA_ENTRY_LSB },
      { 64, ME_SUSTAIN },
      { 65, ME_PORTAMENTO },
      { 66, ME_SOSTENUTO },
      { 67, ME_SOFT_PEDAL },
      { 68, ME_LEGATO_FOOTSWITCH },
      { 69, ME_HOLD2 },
      { 71, ME_HARMONIC_CONTENT },
      { 72, ME_RELEASE_TIME },
      { 73, ME_ATTACK_TIME },
      { 74, ME_BRIGHTNESS },
      { 75, ME_DECAY_TIME },
      { 76, ME_VIBRATO_RATE },
      { 77, ME_VIBRATO_DEPTH },
      { 78, ME_VIBRATO_DELAY },
      { 84, ME_PORTAMENTO_CONTROL },
      { 91, ME_REVERB_EFFECT },
      { 92, ME_TREMOLO_EFFECT },
      { 93, ME_CHORUS_EFFECT },
      { 94, ME_CELESTE_EFFECT },
      { 95, ME_PHASER_EFFECT },
      { 96, ME_RPN_INC },
      { 97, ME_RPN_DEC },
      { 98, ME_NRPN_LSB },
      { 99, ME_NRPN_MSB },
      { 100, ME_RPN_LSB },
      { 101, ME_RPN_MSB },
      { 111, ME_LOOP_START },
      { 120, ME_ALL_SOUNDS_OFF },
      { 121, ME_RESET_CONTROLLERS },
      { 123, ME_ALL_NOTES_OFF },
      { 126, ME_MONO },
      { 127, ME_POLY },
};

int convert_midi_control_change(int chn, int type, int val, MidiEvent *ev_ret)
{
    int i, ttype;
	
    for (i = 0; i < ARRAY_SIZE(ctl_chg_list); i++) {
		if (ctl_chg_list[i].mtype == type) {
			ttype = ctl_chg_list[i].ttype;
			break;
		}
    }
    if (i >= ARRAY_SIZE(ctl_chg_list)){
		if(0 <= type && type <= 127)
			ttype = ME_UNDEF_CTRL_CHNG;
		else
			ttype = -1;
	}
    if(ttype != -1){
		if(val > 127)
			val = 127;
		ev_ret->type    = ttype;
		ev_ret->channel = chn;
		ev_ret->a       = val;
		ev_ret->b       = type; // original control_change number
		return 1;
    }
    return 0;
}

/* 
この関数がどこで使用されるのか不明 windows以外？
一応 ME_UNDEF_CTRL_CHNG 対応
*/
int unconvert_midi_control_change(MidiEvent *ev)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(ctl_chg_list); i++) {
	if (ctl_chg_list[i].ttype == ev->type)
	    break;
    }
    if (i >= ARRAY_SIZE(ctl_chg_list)){
		if(ev->type == ME_UNDEF_CTRL_CHNG)
			return ev->b; // original control_change number
		else
			return -1;
	}
    return ctl_chg_list[i].mtype;
}

static int block_to_part(int block, int port)
{
	int p;
	p = block & 0x0F;
	if(p == 0) {p = 9;}
	else if(p <= 9) {p--;}
	return MERGE_CHANNEL_PORT2(p, port);
}

///r
#if 0
/* Map XG types onto GS types.  XG should eventually have its own tables */
static int set_xg_reverb_type(int msb, int lsb)
{
	int type = 4;

	if ((msb == 0x00) ||
	    (msb >= 0x05 && msb <= 0x0F) ||
	    (msb >= 0x14))			/* NO EFFECT */
	{
		ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG Set Reverb Type (NO EFFECT %d %d)", msb, lsb);
		return -1;
	}

	switch(msb)
	{
	    case 0x01:
		type = 3;			/* Hall 1 */
		break;
	    case 0x02:
		type = 0;			/* Room 1 */
		break;
	    case 0x03:
		type = 3;			/* Stage 1 -> Hall 1 */
		break;
	    case 0x04:
		type = 5;			/* Plate */
		break;
	    default:
		type = 4;			/* unsupported -> Hall 2 */
	    break;
	}
	if (lsb == 0x01)
	{
	    switch(msb)
	    {
		case 0x01:
		    type = 4;			/* Hall 2 */
		    break;
		case 0x02:
		    type = 1;			/* Room 2 */
		    break;
		case 0x03:
		    type = 4;			/* Stage 2 -> Hall 2 */
		    break;
		default:
		    break;
	    }
	}
	if (lsb == 0x02 && msb == 0x02)
	    type = 2;				/* Room 3 */

	ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG Set Reverb Type (%d)", type);
	return type;
}

/* Map XG types onto GS types.  XG should eventually have its own tables */
static int set_xg_chorus_type(int msb, int lsb)
{
	int type = 2;

	if ((msb >= 0x00 && msb <= 0x40) ||
	    (msb >= 0x45 && msb <= 0x47) ||
	    (msb >= 0x49))			/* NO EFFECT */
	{
		ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG Set Chorus Type (NO EFFECT %d %d)", msb, lsb);
		return -1;
	}

	switch(msb)
	{
	    case 0x41:
		type = 0;			/* Chorus 1 */
		break;
	    case 0x42:
		type = 0;			/* Celeste 1 -> Chorus 1 */
		break;
	    case 0x43:
		type = 5;
		break;
	    default:
		type = 2;			/* unsupported -> Chorus 3 */
	    break;
	}
	if (lsb == 0x01)
	{
	    switch(msb)
	    {
		case 0x41:
		    type = 1;			/* Chorus 2 */
		    break;
		case 0x42:
		    type = 1;			/* Celeste 2 -> Chorus 2 */
		    break;
		default:
		    break;
	    }
	}
	else if (lsb == 0x02)
	{
	    switch(msb)
	    {
		case 0x41:
		    type = 2;			/* Chorus 3 */
		    break;
		case 0x42:
		    type = 2;			/* Celeste 3 -> Chorus 3 */
		    break;
		default:
		    break;
	    }
	}
	else if (lsb == 0x08)
	{
	    switch(msb)
	    {
		case 0x41:
		    type = 3;			/* Chorus 4 */
		    break;
		case 0x42:
		    type = 3;			/* Celeste 4 -> Chorus 4 */
		    break;
		default:
		    break;
	    }
	}

	ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG Set Chorus Type (%d)", type);
	return type;
}
#endif

/* XG SysEx parsing function by Eric A. Welsh
 * Also handles GS patch+bank changes
 *
 * This function provides basic support for XG Bulk Dump and Parameter
 * Change SysEx events
 */
int parse_sysex_event_multi(uint8 *val, int32 len, MidiEvent *evm)
{
    int num_events = 0;				/* Number of events added */
    uint32 channel_tt;
    int i, j;
    static uint8 xg_reverb_type_msb = 0x01, xg_reverb_type_lsb = 0x00;
    static uint8 xg_chorus_type_msb = 0x41, xg_chorus_type_lsb = 0x00;

    if(current_file_info->mid == 0 || current_file_info->mid >= 0x7e)
	current_file_info->mid = val[0];

    /* Effect 1 or Multi EQ */
    if(len >= 8 &&
       val[0] == 0x43 && /* Yamaha ID */
       val[2] == 0x4C && /* XG Model ID */
       ((val[1] <  0x10 && val[5] == 0x02) ||	/* Bulk Dump*/
        (val[1] >= 0x10 && val[3] == 0x02)))	/* Parameter Change */
    {
	uint8 addhigh, addmid, addlow;		/* Addresses */
	uint8 *body;				/* SysEx body */
	int ent, v;				/* Entry # of sub-event */
	uint8 *body_end;			/* End of SysEx body */

	if (val[1] < 0x10)	/* Bulk Dump */
	{
	    addhigh = val[5];
	    addmid = val[6];
	    addlow = val[7];
	    body = val + 8;
	    body_end = val + len - 3;
	}
	else			/* Parameter Change */
	{
	    addhigh = val[3];
	    addmid = val[4];
	    addlow = val[5];
	    body = val + 6;
	    body_end = val + len - 2;
	}

	/* set the SYSEX_XG_MSB info */
	SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_MSB, 0, addhigh, addmid);
	num_events++;

	for (ent = addlow; body <= body_end; body++, ent++) {
	  if(addmid == 0x01) {	/* Effect 1 */
	    switch(ent) {
		case 0x00:	/* Reverb Type MSB */
#if 1	/* XG specific reverb is supported, */
		    SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
			num_events++;
#else	/* XG specific reverb is not supported yet, use GS instead */
		    xg_reverb_type_msb = *body;

#endif
		    break;

		case 0x01:	/* Reverb Type LSB */
#if 1	/* XG specific reverb is supported, */
		    SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
			num_events++;
#else	/* XG specific reverb is not supported yet, use GS instead */
		    xg_reverb_type_lsb = *body;
		    v = set_xg_reverb_type(xg_reverb_type_msb, xg_reverb_type_lsb);
		    if (v >= 0) {
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_GS_LSB, 0, v, 0x05);
			num_events++;
		    }
#endif
		    break;

		case 0x0C:	/* Reverb Return */
		    SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
			num_events++;
		    break;

		case 0x20:	/* Chorus Type MSB */
#if 1	/* XG specific chorus is supported, */
		    SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
			num_events++;
#else	/* XG specific chorus is not supported yet, use GS instead */
		    xg_chorus_type_msb = *body;
#endif
		    break;

		case 0x21:	/* Chorus Type LSB */
#if 1	/* XG specific chorus is supported, */
		    SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
			num_events++;
#else	/* XG specific chorus is not supported yet, use GS instead */
		    xg_chorus_type_lsb = *body;
		    v = set_xg_chorus_type(xg_chorus_type_msb, xg_chorus_type_lsb);
		    if (v >= 0) {
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_GS_LSB, 0, v, 0x0D);
			num_events++;
		    }
#endif
		    break;

		case 0x2C:	/* Chorus Return */
		    SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
			num_events++;
		    break;

		default: /* Prameter Reverb 0x02~0x15 , Chorus 0x22~0x35 , Variation 0x40~0x75 */
		    SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
			num_events++;
		    break;
	    }
	  }
	  else if(addmid == 0x40) {	/* Multi EQ */
	    switch(ent) {
		case 0x00:	/* EQ type */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
			num_events++;
		    break;

		case 0x01:	/* EQ gain1 */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
			num_events++;
			break;

		case 0x02:	/* EQ frequency1 */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
			num_events++;
			break;

		case 0x03:	/* EQ Q1 */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
			num_events++;
			break;

		case 0x04:	/* EQ shape1 */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
			num_events++;
			break;

		case 0x05:	/* EQ gain2 */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
			num_events++;
			break;

		case 0x06:	/* EQ frequency2 */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
			num_events++;
			break;

		case 0x07:	/* EQ Q2 */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
			num_events++;
			break;

		case 0x09:	/* EQ gain3 */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
			num_events++;
			break;

		case 0x0A:	/* EQ frequency3 */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
			num_events++;
			break;

		case 0x0B:	/* EQ Q3 */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
			num_events++;
			break;

		case 0x0D:	/* EQ gain4 */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
			num_events++;
			break;

		case 0x0E:	/* EQ frequency4 */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
			num_events++;
			break;

		case 0x0F:	/* EQ Q4 */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
			num_events++;
			break;

		case 0x11:	/* EQ gain5 */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
			num_events++;
			break;

		case 0x12:	/* EQ frequency5 */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
			num_events++;
			break;

		case 0x13:	/* EQ Q5 */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
			num_events++;
			break;

		case 0x14:	/* EQ shape5 */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
			num_events++;
			break;

		default:
		    	break;
	    }
	  }
	}
    }

    /* Effect 2 (Insertion Effects) */
    else if(len >= 8 &&
       val[0] == 0x43 && /* Yamaha ID */
       val[2] == 0x4C && /* XG Model ID */
       ((val[1] <  0x10 && val[5] == 0x03) ||	/* Bulk Dump*/
        (val[1] >= 0x10 && val[3] == 0x03)))	/* Parameter Change */
    {
	uint8 addhigh, addmid, addlow;		/* Addresses */
	uint8 *body;				/* SysEx body */
	int ent;				/* Entry # of sub-event */
	uint8 *body_end;			/* End of SysEx body */

	if (val[1] < 0x10)	/* Bulk Dump */
	{
	    addhigh = val[5];
	    addmid = val[6];
	    addlow = val[7];
	    body = val + 8;
	    body_end = val + len - 3;
	}
	else			/* Parameter Change */
	{
	    addhigh = val[3];
	    addmid = val[4];
	    addlow = val[5];
	    body = val + 6;
	    body_end = val + len - 2;
	}

	/* set the SYSEX_XG_MSB info */
	SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_MSB, 0, addhigh, addmid);
	num_events++;

	for (ent = addlow; body <= body_end; body++, ent++) {
	    SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
			 num_events++;
	}
    }

    /* XG Multi Part Data parameter change */
    else if(len >= 8 &&
       val[0] == 0x43 && /* Yamaha ID */
       val[2] == 0x4C && /* XG Model ID */
       ((val[1] <  0x10 && val[5] == 0x08 &&	/* Bulk Dump */
         (val[4] == 0x29 || val[4] == 0x3F)) ||	/* Blocks 1 or 2 */
        (val[1] >= 0x10 && val[3] == 0x08)))	/* Parameter Change */
    {
	uint8 addhigh, addmid, addlow;		/* Addresses */
	uint8 *body;				/* SysEx body */
	uint8 p;				/* Channel part number [0..15] */
	int ent;				/* Entry # of sub-event */
	uint8 *body_end;			/* End of SysEx body */

	if (val[1] < 0x10)	/* Bulk Dump */
	{
	    addhigh = val[5];
	    addmid = val[6];
	    addlow = val[7];
	    body = val + 8;
	    p = addmid;
	    body_end = val + len - 3;
	}
	else			/* Parameter Change */
	{
	    addhigh = val[3];
	    addmid = val[4];
	    addlow = val[5];
	    body = val + 6;
	    p = addmid;
	    body_end = val + len - 2;
	}

	/* set the SYSEX_XG_MSB info */
	SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_MSB, p, addhigh, addmid);
	num_events++;

	for (ent = addlow; body <= body_end; body++, ent++) {
	    switch(ent) {
		case 0x00:	/* Element Reserve */
/*			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Element Reserve is not supported. (CH:%d VAL:%d)", p, *body); */
		    break;
///r
		case 0x01:	/* bank select MSB */
		    ctl->cmsg(CMSG_INFO, VERB_NOISY, "XG SysExMP ME_TONE_BANK_MSB %d %d",p , *body);
		    SETMIDIEVENT(evm[num_events], 0, ME_TONE_BANK_MSB, p, *body, SYSEX_TAG);
		    num_events++;
		    break;

		case 0x02:	/* bank select LSB */
		    ctl->cmsg(CMSG_INFO, VERB_NOISY, "XG SysExMP ME_TONE_BANK_LSB %d %d",p , *body);
		    SETMIDIEVENT(evm[num_events], 0, ME_TONE_BANK_LSB, p, *body, SYSEX_TAG);
		    num_events++;
		    break;

		case 0x03:	/* program number */
		    ctl->cmsg(CMSG_INFO, VERB_NOISY, "XG SysExMP ME_PROGRAM %d %d",p , *body);
		    SETMIDIEVENT(evm[num_events], 0, ME_PROGRAM, p, *body, SYSEX_TAG);
		    num_events++;
		    break;

		case 0x04:	/* Rcv CHANNEL */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, p, *body, 0x99);
			num_events++;
		    break;

		case 0x05:	/* mono/poly mode */
			if(*body == 0) {SETMIDIEVENT(evm[num_events], 0, ME_MONO, p, 0, SYSEX_TAG);}
			else {SETMIDIEVENT(evm[num_events], 0, ME_POLY, p, 0, SYSEX_TAG);}
			num_events++;
		    break;

		case 0x06:	/* Same Note Number Key On Assign */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, p, *body, ent);
			num_events++;
		    break;

		case 0x07:	/* Part Mode */
		    ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG SysExMP ME_DRUMPART %d %d",p , *body);
			drum_setup_xg[*body] = p;
			SETMIDIEVENT(evm[num_events], 0, ME_DRUMPART, p, *body, SYSEX_TAG);
			num_events++;
		    break;

		case 0x08:	/* note shift */
		    SETMIDIEVENT(evm[num_events], 0, ME_KEYSHIFT, p, *body, SYSEX_TAG);
		    num_events++;
		    break;

		case 0x09:	/* Detune 1st bit */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, p, *body, ent);
			num_events++;
		//	ctl->cmsg(CMSG_INFO, VERB_NOISY, "Detune 1st bit is not supported. (CH:%d VAL:%d)", p, *body); 
		    break;

		case 0x0A:	/* Detune 2nd bit */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, p, *body, ent);
			num_events++;
		//    ctl->cmsg(CMSG_INFO, VERB_NOISY, "Detune 2nd bit is not supported. (CH:%d VAL:%d)", p, *body); 
		    break;

		case 0x0B:	/* volume */
		    SETMIDIEVENT(evm[num_events], 0, ME_MAINVOLUME, p, *body, SYSEX_TAG);
		    num_events++;
		    break;

		case 0x0C:	/* Velocity Sense Depth */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_GS_LSB, p, *body, 0x21);
			num_events++;
			break;

		case 0x0D:	/* Velocity Sense Offset */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_GS_LSB, p, *body, 0x22);
			num_events++;
			break;

		case 0x0E:	/* pan */
		    if(*body == 0) {
			SETMIDIEVENT(evm[num_events], 0, ME_RANDOM_PAN, p, 0, SYSEX_TAG);
		    }
		    else {
			SETMIDIEVENT(evm[num_events], 0, ME_PAN, p, *body, SYSEX_TAG);
		    }
		    num_events++;
		    break;

		case 0x0F:	/* Note Limit Low */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x42);
			num_events++;
		    break;

		case 0x10:	/* Note Limit High */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x43);
			num_events++;
			break;

		case 0x11:	/* Dry Level */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, p, *body, ent);
			num_events++;
			break;

		case 0x12:	/* chorus send */
		    SETMIDIEVENT(evm[num_events], 0, ME_CHORUS_EFFECT, p, *body, SYSEX_TAG);
		    num_events++;
		    break;

		case 0x13:	/* reverb send */
		    SETMIDIEVENT(evm[num_events], 0, ME_REVERB_EFFECT, p, *body, SYSEX_TAG);
		    num_events++;
		    break;

		case 0x14:	/* Variation Send */
		    SETMIDIEVENT(evm[num_events], 0, ME_CELESTE_EFFECT, p, *body, SYSEX_TAG);
		    num_events++;
		    break;

		case 0x15:	/* Vibrato Rate */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, p, 0x08, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
			num_events += 3;
		    break;

		case 0x16:	/* Vibrato Depth */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, p, 0x09, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
			num_events += 3;
		    break;

		case 0x17:	/* Vibrato Delay */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, p, 0x0A, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
			num_events += 3;
		    break;

		case 0x18:	/* Filter Cutoff Frequency */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, p, 0x20, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
			num_events += 3;
		    break;

		case 0x19:	/* Filter Resonance */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, p, 0x21, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
			num_events += 3;
		    break;

		case 0x1A:	/* EG Attack Time */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, p, 0x63, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
			num_events += 3;
		    break;

		case 0x1B:	/* EG Decay Time */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, p, 0x64, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
			num_events += 3;
		    break;

		case 0x1C:	/* EG Release Time */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, p, 0x66, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
			num_events += 3;
		    break;

		case 0x1D:	/* MW Pitch Control */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x16);
			num_events++;
			break;

		case 0x1E:	/* MW Filter Control */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x17);
			num_events++;
			break;

		case 0x1F:	/* MW Amplitude Control */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x18);
			num_events++;
			break;

		case 0x20:	/* MW LFO PMod Depth */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x1A);
			num_events++;
			break;

		case 0x21:	/* MW LFO FMod Depth */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x1B);
			num_events++;
			break;

		case 0x22:	/* MW LFO AMod Depth */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x1C);
			num_events++;
			break;

		case 0x23:	/* bend pitch control */
		    SETMIDIEVENT(evm[num_events], 0, ME_RPN_MSB, p, 0, SYSEX_TAG);
		    SETMIDIEVENT(evm[num_events + 1], 0, ME_RPN_LSB, p, 0, SYSEX_TAG);
		    SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, p, ((int)*body - (int)0x40) & 0xFF, SYSEX_TAG);
		    num_events += 3;
		    break;

		case 0x24:	/* Bend Filter Control */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x22);
			num_events++;
			break;

		case 0x25:	/* Bend Amplitude Control */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x23);
			num_events++;
			break;

		case 0x26:	/* Bend LFO PMod Depth */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x25);
			num_events++;
			break;

		case 0x27:	/* Bend LFO FMod Depth */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x26);
			num_events++;
			break;

		case 0x28:	/* Bend LFO AMod Depth */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x27);
			num_events++;
			break;

		case 0x30:	/* Rcv Pitch Bend */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x48);
			num_events++;
			break;

		case 0x31:	/* Rcv Channel Pressure */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x49);
			num_events++;
			break;

		case 0x32:	/* Rcv Program Change */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x4A);
			num_events++;
			break;

		case 0x33:	/* Rcv Control Change */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x4B);
			num_events++;
			break;

		case 0x34:	/* Rcv Poly Pressure */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x4C);
			num_events++;
			break;

		case 0x35:	/* Rcv Note Message */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x4D);
			num_events++;
			break;

		case 0x36:	/* Rcv RPN */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x4E);
			num_events++;
			break;

		case 0x37:	/* Rcv NRPN */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x4F);
			num_events++;
			break;

		case 0x38:	/* Rcv Modulation */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x50);
			num_events++;
			break;

		case 0x39:	/* Rcv Volume */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x51);
			num_events++;
			break;

		case 0x3A:	/* Rcv Pan */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x52);
			num_events++;
			break;

		case 0x3B:	/* Rcv Expression */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x53);
			num_events++;
			break;

		case 0x3C:	/* Rcv Hold1 */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x54);
			num_events++;
			break;

		case 0x3D:	/* Rcv Portamento */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x55);
			num_events++;
			break;

		case 0x3E:	/* Rcv Sostenuto */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x56);
			num_events++;
			break;

		case 0x3F:	/* Rcv Soft */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x57);
			num_events++;
			break;

		case 0x40:	/* Rcv Bank Select */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x58);
			num_events++;
			break;

		case 0x41:	/* scale tuning */
		case 0x42:
		case 0x43:
		case 0x44:
		case 0x45:
		case 0x46:
		case 0x47:
		case 0x48:
		case 0x49:
		case 0x4a:
		case 0x4b:
		case 0x4c:
		    SETMIDIEVENT(evm[num_events], 0, ME_SCALE_TUNING, p, ent - 0x41, *body - 64);
		    num_events++;
		    ctl->cmsg(CMSG_INFO, VERB_NOISY, "Scale Tuning %s (CH:%d %d cent)",
			      note_name[ent - 0x41], p, *body - 64);
		    break;

		case 0x4D:	/* CAT Pitch Control */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x00);
			num_events++;
			break;

		case 0x4E:	/* CAT Filter Control */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x01);
			num_events++;
			break;

		case 0x4F:	/* CAT Amplitude Control */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x02);
			num_events++;
			break;

		case 0x50:	/* CAT LFO PMod Depth */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x04);
			num_events++;
			break;

		case 0x51:	/* CAT LFO FMod Depth */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x05);
			num_events++;
			break;

		case 0x52:	/* CAT LFO AMod Depth */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x06);
			num_events++;
			break;

		case 0x53:	/* PAT Pitch Control */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x0B);
			num_events++;
			break;

		case 0x54:	/* PAT Filter Control */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x0C);
			num_events++;
			break;

		case 0x55:	/* PAT Amplitude Control */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x0D);
			num_events++;
			break;

		case 0x56:	/* PAT LFO PMod Depth */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x0F);
			num_events++;
			break;

		case 0x57:	/* PAT LFO FMod Depth */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x10);
			num_events++;
			break;

		case 0x58:	/* PAT LFO AMod Depth */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x11);
			num_events++;
			break;
		
		case 0x59:	/* AC1 Controller Number */
			if(*body < 0 || *body > 95){
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, -1, 0x6D); // off
			}else{					
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x6D);
			}
			num_events++;
		//	ctl->cmsg(CMSG_INFO, VERB_NOISY, "AC1 Controller Number is not supported. (CH:%d VAL:%d)", p, *body); 
			break;

		case 0x5A:	/* AC1 Pitch Control */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x2C);
			num_events++;
			break;

		case 0x5B:	/* AC1 Filter Control */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x2D);
			num_events++;
			break;

		case 0x5C:	/* AC1 Amplitude Control */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x2E);
			num_events++;
			break;

		case 0x5D:	/* AC1 LFO PMod Depth */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x30);
			num_events++;
			break;

		case 0x5E:	/* AC1 LFO FMod Depth */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x31);
			num_events++;
			break;

		case 0x5F:	/* AC1 LFO AMod Depth */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x32);
			num_events++;
			break;

		case 0x60:	/* AC2 Controller Number */
			if(*body < 0 || *body > 95){
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, -1, 0x6E); // off
			}else{
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x6E);
			}
			num_events++;
		//	ctl->cmsg(CMSG_INFO, VERB_NOISY, "AC2 Controller Number is not supported. (CH:%d VAL:%d)", p, *body); 
			break;

		case 0x61:	/* AC2 Pitch Control */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x37);
			num_events++;
			break;

		case 0x62:	/* AC2 Filter Control */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x38);
			num_events++;
			break;

		case 0x63:	/* AC2 Amplitude Control */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x39);
			num_events++;
			break;

		case 0x64:	/* AC2 LFO PMod Depth */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x3B);
			num_events++;
			break;

		case 0x65:	/* AC2 LFO FMod Depth */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x3C);
			num_events++;
			break;

		case 0x66:	/* AC2 LFO AMod Depth */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x3D);
			num_events++;
			break;
///r
		case 0x67:	/* Portamento Switch */
			SETMIDIEVENT(evm[num_events], 0, ME_PORTAMENTO, p, *body ? 127 : 0, SYSEX_TAG);
		    num_events++;
		    break;

		case 0x68:	/* Portamento Time */
			SETMIDIEVENT(evm[num_events], 0, ME_PORTAMENTO_TIME_MSB, p, *body, SYSEX_TAG);
		    num_events++;
		    break;
///r
		case 0x69:	/* Pitch EG Initial Level */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x62);
			num_events++;
		//   ctl->cmsg(CMSG_INFO, VERB_NOISY, "Pitch EG Initial Level is not supported. (CH:%d VAL:%d)", p, *body); 
		    break;

		case 0x6A:	/* Pitch EG Attack Time */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x64);
			num_events++;
		//    ctl->cmsg(CMSG_INFO, VERB_NOISY, "Pitch EG Attack Time is not supported. (CH:%d VAL:%d)", p, *body); 
		    break;

		case 0x6B:	/* Pitch EG Release Level */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x69);
			num_events++;
		//    ctl->cmsg(CMSG_INFO, VERB_NOISY, "Pitch EG Release Level is not supported. (CH:%d VAL:%d)", p, *body); 
		    break;

		case 0x6C:	/* Pitch EG Release Time */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x6A);
			num_events++;
		//    ctl->cmsg(CMSG_INFO, VERB_NOISY, "Pitch EG Release Time is not supported. (CH:%d VAL:%d)", p, *body); 
		    break;

		case 0x6D:	/* Velocity Limit Low */
		    SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x44);
			num_events++;
			break;

		case 0x6E:	/* Velocity Limit High */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x45);
			num_events++;
		    break;

	//	case 0x6F:

		case 0x70:	/* Bend Pitch Low Control */
			/*
			RPN 0x00 0x40 に Pitch Bend Sensitivity Low Control を追加
			SysEx VALを符号反転してRPNに変換して反映 (RPNに変換するのはBend Pitch Controlと同じ
			通常のBend Pitch Controlの場合は Low Controlを同値で上書き , Low Controlが後の場合にだけ機能する
			*/
		    SETMIDIEVENT(evm[num_events], 0, ME_RPN_MSB, p, 0, SYSEX_TAG);
		    SETMIDIEVENT(evm[num_events + 1], 0, ME_RPN_LSB, p, 0x40, SYSEX_TAG); // low control
		    SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, p, ((int)0x40 - (int)*body) & 0xFF, SYSEX_TAG);
		    num_events += 3;
		    break;

		case 0x71:	/* Filter EG Depth */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x6B);
			num_events++;
		//    ctl->cmsg(CMSG_INFO, VERB_NOISY, "Filter EG Depth is not supported. (CH:%d VAL:%d)", p, *body); 
		    break;

		case 0x72:	/* EQ BASS */
			SETMIDIEVENT(evm[num_events], 0,ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0,ME_NRPN_LSB, p, 0x30, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0,ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x73:	/* EQ TREBLE */
			SETMIDIEVENT(evm[num_events], 0,ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0,ME_NRPN_LSB, p, 0x31, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0,ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x74:	/* EQ MID-BASS */
			SETMIDIEVENT(evm[num_events], 0,ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0,ME_NRPN_LSB, p, 0x32, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0,ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x75:	/* EQ MID-TREBLE */
			SETMIDIEVENT(evm[num_events], 0,ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0,ME_NRPN_LSB, p, 0x33, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0,ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x76:	/* EQ BASS frequency */
			SETMIDIEVENT(evm[num_events], 0,ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0,ME_NRPN_LSB, p, 0x34, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0,ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x77:	/* EQ TREBLE frequency */
			SETMIDIEVENT(evm[num_events], 0,ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0,ME_NRPN_LSB, p, 0x35, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0,ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x78:	/* EQ MID-BASS frequency */
			SETMIDIEVENT(evm[num_events], 0,ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0,ME_NRPN_LSB, p, 0x36, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0,ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x79:	/* EQ MID-TREBLE frequency */
			SETMIDIEVENT(evm[num_events], 0,ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0,ME_NRPN_LSB, p, 0x37, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0,ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x7A:	/* EQ BASS Q */
			SETMIDIEVENT(evm[num_events], 0,ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0,ME_NRPN_LSB, p, 0x38, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0,ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x7B:	/* EQ TREBLE Q */
			SETMIDIEVENT(evm[num_events], 0,ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0,ME_NRPN_LSB, p, 0x39, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0,ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x7C:	/* EQ MID-BASS Q */
			SETMIDIEVENT(evm[num_events], 0,ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0,ME_NRPN_LSB, p, 0x3A, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0,ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x7D:	/* EQ MID-TREBLE Q */
			SETMIDIEVENT(evm[num_events], 0,ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0,ME_NRPN_LSB, p, 0x3B, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0,ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x7E:	/* EQ BASS shape */
			SETMIDIEVENT(evm[num_events], 0,ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0,ME_NRPN_LSB, p, 0x3C, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0,ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x7F:	/* EQ TREBLE shape */
			SETMIDIEVENT(evm[num_events], 0,ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0,ME_NRPN_LSB, p, 0x3D, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0,ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
			num_events += 3;
			break;

		default:
		    ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported XG Bulk Dump SysEx. (ADDR:%02X %02X %02X VAL:%02X)",addhigh,addlow,ent,*body);
		    continue;
		    break;
	    }
	}
    }

    /* XG Drum Setup */
    else if(len >= 10 &&
       val[0] == 0x43 && /* Yamaha ID */
       val[2] == 0x4C && /* XG Model ID */
       ((val[1] <  0x10 && (val[5] & 0xF0) == 0x30) ||	/* Bulk Dump*/
        (val[1] >= 0x10 && (val[3] & 0xF0) == 0x30)))	/* Parameter Change */
    {
	uint8 addhigh, addmid, addlow;		/* Addresses */
	uint8 *body;				/* SysEx body */
	uint8 dp, note;				/* Channel part number [0..15] */
	int ent;				/* Entry # of sub-event */
	uint8 *body_end;			/* End of SysEx body */

	if (val[1] < 0x10)	/* Bulk Dump */
	{
	    addhigh = val[5];
	    addmid = val[6];
	    addlow = val[7];
	    body = val + 8;
	    body_end = val + len - 3;
	}
	else			/* Parameter Change */
	{
	    addhigh = val[3];
	    addmid = val[4];
	    addlow = val[5];
	    body = val + 6;
	    body_end = val + len - 2;
	}

	dp = drum_setup_xg[(addhigh & 0x0F) + 1];
	note = addmid;

	/* set the SYSEX_XG_MSB info */
	SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_MSB, dp, addhigh, addmid);
	num_events++;

	for (ent = addlow; body <= body_end; body++, ent++) {
	    switch(ent) {
		case 0x00:	/* Pitch Coarse */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x18, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x01:	/* Pitch Fine */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x19, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x02:	/* Level */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x1A, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x03:	/* Alternate Group */
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Alternate Group is not supported. (CH:%d NOTE:%d VAL:%d)", dp, note, *body);
			break;
		case 0x04:	/* Pan */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x1C, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x05:	/* Reverb Send */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x1D, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x06:	/* Chorus Send */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x1E, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x07:	/* Variation Send */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x1F, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x08:	/* Key Assign */
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Key Assign is not supported. (CH:%d NOTE:%d VAL:%d)", dp, note, *body);
			break;
		case 0x09:	/* Rcv Note Off */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_MSB, dp, note, 0);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_SYSEX_LSB, dp, *body, 0x46);
			num_events += 2;
			break;
		case 0x0A:	/* Rcv Note On */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_MSB, dp, note, 0);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_SYSEX_LSB, dp, *body, 0x47);
			num_events += 2;
			break;
		case 0x0B:	/* Filter Cutoff Frequency */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x14, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x0C:	/* Filter Resonance */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x15, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x0D:	/* EG Attack */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x16, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x0E:	/* EG Decay1 */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, dp, *body, ent);
			num_events++;
			break;
		case 0x0F:	/* EG Decay2 */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, dp, *body, ent);
			num_events++;
			break;
		case 0x20:	/* EQ BASS */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x30, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x21:	/* EQ TREBLE */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x31, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x22:	/* EQ MID-BASS */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x32, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x23:	/* EQ MID-TREBLE */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x33, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x24:	/* EQ BASS frequency */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x34, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x25:	/* EQ TREBLE frequency */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x35, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x26:	/* EQ MID-BASS frequency */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x36, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x27:	/* EQ MID-TREBLE frequency */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x37, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x28:	/* EQ BASS Q */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x38, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x29:	/* EQ TREBLE Q */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x39, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x2A:	/* EQ MID-BASS Q */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x3A, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x2B:	/* EQ MID-TREBLE Q */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x3B, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x2C:	/* EQ BASS shape */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x3C, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x2D:	/* EQ TREBLE shape */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x3D, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
			num_events += 3;
			break;
///r
		case 0x50:	/* High Pass Filter Cutoff Frequency */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x24, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x51:	/* High Pass Filter Resonace */
			SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x25, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
			SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
			num_events += 3;
			break;
		case 0x60:	/* Velocity Pitch Sense */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, dp, *body, ent);
			num_events++;
			break;
		case 0x61:	/* Velocity LPF Cutoff Sense */
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, dp, *body, ent);
			num_events++;
			break;
		default:
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported XG Bulk Dump SysEx. (ADDR:%02X %02X %02X VAL:%02X)",addhigh,addmid,ent,*body);
			break;
	    }
	}
    }

    /* parsing GS System Exclusive Message...
     *
     * val[4] == Parameter Address(High)
     * val[5] == Parameter Address(Middle)
     * val[6] == Parameter Address(Low)
     * val[7]... == Data...
     * val[last] == Checksum(== 128 - (sum of addresses&data bytes % 128)) 
     */
    else if(len >= 9 &&
       val[0] == 0x41 && /* Roland ID */
       val[1] == 0x10 && /* Device ID */
       val[2] == 0x42 && /* GS Model ID */
       val[3] == 0x12) /* Data Set Command */
    {
		uint8 p, dp, udn, gslen, port = 0;
		int i, addr, addr_h, addr_m, addr_l, checksum;
		p = block_to_part(val[5], midi_port_number);

		/* calculate checksum */
		checksum = 0;
		for(gslen = 9; gslen < len; gslen++)
			if(val[gslen] == 0xF7)
				break;
		for(i=4;i<gslen-1;i++) {
			checksum += val[i];
		}
		if(((128 - (checksum & 0x7F)) & 0x7F) != val[gslen-1]) {
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"GS SysEx: Checksum Error.");
			return num_events;
		}

		/* drum channel */
		dp = rhythm_part[(val[5] & 0xF0) >> 4];

		/* calculate user drumset number */
		udn = (val[5] & 0xF0) >> 4;

		addr_h = val[4];
		addr_m = val[5];
		addr_l = val[6];
		if(addr_h == 0x50) {	/* for double module mode */
			port = 1;
			p = block_to_part(val[5], port);
			addr_h = 0x40;
		} else if(addr_h == 0x51) {
			port = 1;
			p = block_to_part(val[5], port);
			addr_h = 0x41;
		}
		addr = (((int32)addr_h)<<16 | ((int32)addr_m)<<8 | (int32)addr_l);
		
		/* data error */
		if(val[7] > 0x7F){
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "GS SysEx: Data Error. (CH:%d ADDR:%02X %02X %02X VAL:%02X)", p, addr_h, addr_m, addr_l, val[7]);
		}

		switch(addr_h) {
		case 0x40:
			if((addr & 0xFFF000) == 0x401000) {
				switch(addr & 0xFF) {
				case 0x00:	/* Tone Number */
					SETMIDIEVENT(evm[0], 0, ME_TONE_BANK_MSB, p,val[7], SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_PROGRAM, p, val[8], SYSEX_TAG);
					num_events += 2;
					break;
				case 0x02:	/* Rx. Channel */
					if (val[7] == 0x10) {
						SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB,
								block_to_part(val[5],
								midi_port_number ^ port), 0x80, 0x45);
					} else {
						SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB,
								block_to_part(val[5],
								midi_port_number ^ port),
								MERGE_CHANNEL_PORT2(val[7],
								midi_port_number ^ port), 0x45);
					}
					num_events++;
					break;
				case 0x03:	/* Rx. Pitch Bend */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x48);
					num_events++;
					break;
				case 0x04:	/* Rx. Channel Pressure */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x49);
					num_events++;
					break;
				case 0x05:	/* Rx. Program Change */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x4A);
					num_events++;
					break;
				case 0x06:	/* Rx. Control Change */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x4B);
					num_events++;
					break;
				case 0x07:	/* Rx. Poly Pressure */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x4C);
					num_events++;
					break;
				case 0x08:	/* Rx. Note Message */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x4D);
					num_events++;
					break;
				case 0x09:	/* Rx. RPN */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x4E);
					num_events++;
					break;
				case 0x0A:	/* Rx. NRPN */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x4F);
					num_events++;
					break;
				case 0x0B:	/* Rx. Modulation */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x50);
					num_events++;
					break;
				case 0x0C:	/* Rx. Volume */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x51);
					num_events++;
					break;
				case 0x0D:	/* Rx. Panpot */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x52);
					num_events++;
					break;
				case 0x0E:	/* Rx. Expression */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x53);
					num_events++;
					break;
				case 0x0F:	/* Rx. Hold1 */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x54);
					num_events++;
					break;
				case 0x10:	/* Rx. Portamento */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x55);
					num_events++;
					break;
				case 0x11:	/* Rx. Sostenuto */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x56);
					num_events++;
					break;
				case 0x12:	/* Rx. Soft */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x57);
					num_events++;
					break;
				case 0x13:	/* MONO/POLY Mode */
					if(val[7] == 0) {SETMIDIEVENT(evm[0], 0, ME_MONO, p, val[7], SYSEX_TAG);}
					else {SETMIDIEVENT(evm[0], 0, ME_POLY, p, val[7], SYSEX_TAG);}
					num_events++;
					break;
				case 0x14:	/* Assign Mode */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x24);
					num_events++;
					break;
				case 0x15:	/* Use for Rhythm Part */
					if(val[7]) {
						rhythm_part[val[7] - 1] = p;
					}
					break;
				case 0x16:	/* Pitch Key Shift (dummy. see parse_sysex_event()) */
					break;
				case 0x17:	/* Pitch Offset Fine */
					{
						// 1stbit bit3-0 to bit7-4 , 2ndbit bit3-0 to bit3-0
						uint8 tmpi = (val[7] << 4) | (val[8] & 0x0F);
						if(tmpi < 0x08)
							tmpi = 0x08;
						else if(tmpi > 0xF8)
							tmpi = 0xF8;
						SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, tmpi, 0x26);
						num_events++;
					}
					break;
			//	case 0x18:	/* Pitch Offset Fine 2ndbit */
			//		break;
				case 0x19:	/* Part Level */
					SETMIDIEVENT(evm[0], 0, ME_MAINVOLUME, p, val[7], SYSEX_TAG);
					num_events++;
					break;
				case 0x1A:	/* Velocity Sense Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x21);
					num_events++;
					break;
				case 0x1B:	/* Velocity Sense Offset */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x22);
					num_events++;
					break;
				case 0x1C:	/* Part Panpot */
					if (val[7] == 0) {
						SETMIDIEVENT(evm[0], 0, ME_RANDOM_PAN, p, 0, SYSEX_TAG);
					} else {
						SETMIDIEVENT(evm[0], 0, ME_PAN, p, val[7], SYSEX_TAG);
					}
					num_events++;
					break;
				case 0x1D:	/* Keyboard Range Low */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x42);
					num_events++;
					break;
				case 0x1E:	/* Keyboard Range High */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x43);
					num_events++;
					break;
				case 0x1F:	/* CC1 Controller Number */
					if(val[7] < 1 || val[7] > 95){
						SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, -1, 0x6D); // off
					}else{					
						SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x6D);
					}
					num_events++;
				//	ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC2 Controller Number is not supported. (CH:%d VAL:%d)", p, val[7]);
					break;
				case 0x20:	/* CC2 Controller Number */
					if(val[7] < 1 || val[7] > 95){
						SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, -1, 0x6E); // off
					}else{
						SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x6E);
					}
					num_events++;
				//	ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC2 Controller Number is not supported. (CH:%d VAL:%d)", p, val[7]);
					break;
				case 0x21:	/* Chorus Send Level */
					SETMIDIEVENT(evm[0], 0, ME_CHORUS_EFFECT, p, val[7], SYSEX_TAG);
					num_events++;
					break;
				case 0x22:	/* Reverb Send Level */
					SETMIDIEVENT(evm[0], 0, ME_REVERB_EFFECT, p, val[7], SYSEX_TAG);
					num_events++;
					break;
				case 0x23:	/* Rx. Bank Select */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x58);
					num_events++;
					break;
				case 0x24:	/* Rx. Bank Select LSB */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x59);
					num_events++;
					break;
				case 0x2C:	/* Delay Send Level */
					SETMIDIEVENT(evm[0], 0, ME_CELESTE_EFFECT, p, val[7], SYSEX_TAG);
					num_events++;
					break;
				case 0x2A:	/* Pitch Fine Tune */
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, p, 0x00, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, p, 0x01, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, p, val[7], SYSEX_TAG);
					SETMIDIEVENT(evm[3], 0, ME_DATA_ENTRY_LSB, p, val[8], SYSEX_TAG);
					num_events += 4;
					break;
				case 0x30:	/* TONE MODIFY1: Vibrato Rate */
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, p, 0x08, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, p, val[7], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x31:	/* TONE MODIFY2: Vibrato Depth */
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, p, 0x09, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, p, val[7], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x32:	/* TONE MODIFY3: TVF Cutoff Freq */
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, p, 0x20, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, p, val[7], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x33:	/* TONE MODIFY4: TVF Resonance */
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, p, 0x21, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, p, val[7], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x34:	/* TONE MODIFY5: TVF&TVA Env.attack */
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, p, 0x63, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, p, val[7], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x35:	/* TONE MODIFY6: TVF&TVA Env.decay */
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, p, 0x64, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, p, val[7], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x36:	/* TONE MODIFY7: TVF&TVA Env.release */
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, p, 0x66, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, p, val[7], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x37:	/* TONE MODIFY8: Vibrato Delay */
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, p, 0x0A, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, p, val[7], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x40:	/* Scale Tuning */
					for (i = 0; i < 12; i++) {
						SETMIDIEVENT(evm[i],
								0, ME_SCALE_TUNING, p, i, val[i + 7] - 64);
						ctl->cmsg(CMSG_INFO, VERB_NOISY,
								"Scale Tuning %s (CH:%d %d cent)",
								note_name[i], p, val[i + 7] - 64);
					}
					num_events += 12;
					break;
				default:
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",addr_h,addr_m,addr_l,val[7],val[8]);
					break;
				}
			} else if((addr & 0xFFF000) == 0x402000) {
				switch(addr & 0xFF) {
				case 0x00:	/* MOD Pitch Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x16);
					num_events++;
					break;
				case 0x01:	/* MOD TVF Cutoff Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x17);
					num_events++;
					break;
				case 0x02:	/* MOD Amplitude Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x18);
					num_events++;
					break;
				case 0x03:	/* MOD LFO1 Rate Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x19);
					num_events++;
					break;
				case 0x04:	/* MOD LFO1 Pitch Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x1A);
					num_events++;
					break;
				case 0x05:	/* MOD LFO1 TVF Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x1B);
					num_events++;
					break;
				case 0x06:	/* MOD LFO1 TVA Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x1C);
					num_events++;
					break;
				case 0x07:	/* MOD LFO2 Rate Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x1D);
					num_events++;
					break;
				case 0x08:	/* MOD LFO2 Pitch Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x1E);
					num_events++;
					break;
				case 0x09:	/* MOD LFO2 TVF Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x1F);
					num_events++;
					break;
				case 0x0A:	/* MOD LFO2 TVA Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x20);
					num_events++;
					break;
				case 0x10:	/* !!!FIXME!!! Bend Pitch Control */
					SETMIDIEVENT(evm[0], 0, ME_RPN_MSB, p, 0, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_RPN_LSB, p, 0, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, p, (val[7] - 0x40) & 0x7F, SYSEX_TAG);
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x21);
					num_events += 4;
					break;
				case 0x11:	/* Bend TVF Cutoff Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x22);
					num_events++;
					break;
				case 0x12:	/* Bend Amplitude Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x23);
					num_events++;
					break;
				case 0x13:	/* Bend LFO1 Rate Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x24);
					num_events++;
					break;
				case 0x14:	/* Bend LFO1 Pitch Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x25);
					num_events++;
					break;
				case 0x15:	/* Bend LFO1 TVF Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x26);
					num_events++;
					break;
				case 0x16:	/* Bend LFO1 TVA Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x27);
					num_events++;
					break;
				case 0x17:	/* Bend LFO2 Rate Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x28);
					num_events++;
					break;
				case 0x18:	/* Bend LFO2 Pitch Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x29);
					num_events++;
					break;
				case 0x19:	/* Bend LFO2 TVF Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x2A);
					num_events++;
					break;
				case 0x1A:	/* Bend LFO2 TVA Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x2B);
					num_events++;
					break;
				case 0x20:	/* CAf Pitch Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x00);
					num_events++;
					break;
				case 0x21:	/* CAf TVF Cutoff Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x01);
					num_events++;
					break;
				case 0x22:	/* CAf Amplitude Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x02);
					num_events++;
					break;
				case 0x23:	/* CAf LFO1 Rate Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x03);
					num_events++;
					break;
				case 0x24:	/* CAf LFO1 Pitch Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x04);
					num_events++;
					break;
				case 0x25:	/* CAf LFO1 TVF Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x05);
					num_events++;
					break;
				case 0x26:	/* CAf LFO1 TVA Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x06);
					num_events++;
					break;
				case 0x27:	/* CAf LFO2 Rate Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x07);
					num_events++;
					break;
				case 0x28:	/* CAf LFO2 Pitch Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x08);
					num_events++;
					break;
				case 0x29:	/* CAf LFO2 TVF Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x09);
					num_events++;
					break;
				case 0x2A:	/* CAf LFO2 TVA Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x0A);
					num_events++;
					break;
				case 0x30:	/* PAf Pitch Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x0B);
					num_events++;
					break;
				case 0x31:	/* PAf TVF Cutoff Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x0C);
					num_events++;
					break;
				case 0x32:	/* PAf Amplitude Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x0D);
					num_events++;
					break;
				case 0x33:	/* PAf LFO1 Rate Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x0E);
					num_events++;
					break;
				case 0x34:	/* PAf LFO1 Pitch Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x0F);
					num_events++;
					break;
				case 0x35:	/* PAf LFO1 TVF Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x10);
					num_events++;
					break;
				case 0x36:	/* PAf LFO1 TVA Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x11);
					num_events++;
					break;
				case 0x37:	/* PAf LFO2 Rate Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x12);
					num_events++;
					break;
				case 0x38:	/* PAf LFO2 Pitch Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x13);
					num_events++;
					break;
				case 0x39:	/* PAf LFO2 TVF Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x14);
					num_events++;
					break;
				case 0x3A:	/* PAf LFO2 TVA Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x15);
					num_events++;
					break;
				case 0x40:	/* CC1 Pitch Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x2C);
					num_events++;
					break;
				case 0x41:	/* CC1 TVF Cutoff Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x2D);
					num_events++;
					break;
				case 0x42:	/* CC1 Amplitude Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x2E);
					num_events++;
					break;
				case 0x43:	/* CC1 LFO1 Rate Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x2F);
					num_events++;
					break;
				case 0x44:	/* CC1 LFO1 Pitch Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x30);
					num_events++;
					break;
				case 0x45:	/* CC1 LFO1 TVF Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x31);
					num_events++;
					break;
				case 0x46:	/* CC1 LFO1 TVA Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x32);
					num_events++;
					break;
				case 0x47:	/* CC1 LFO2 Rate Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x33);
					num_events++;
					break;
				case 0x48:	/* CC1 LFO2 Pitch Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x34);
					num_events++;
					break;
				case 0x49:	/* CC1 LFO2 TVF Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x35);
					num_events++;
					break;
				case 0x4A:	/* CC1 LFO2 TVA Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x36);
					num_events++;
					break;
				case 0x50:	/* CC2 Pitch Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x37);
					num_events++;
					break;
				case 0x51:	/* CC2 TVF Cutoff Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x38);
					num_events++;
					break;
				case 0x52:	/* CC2 Amplitude Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x39);
					num_events++;
					break;
				case 0x53:	/* CC2 LFO1 Rate Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x3A);
					num_events++;
					break;
				case 0x54:	/* CC2 LFO1 Pitch Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x3B);
					num_events++;
					break;
				case 0x55:	/* CC2 LFO1 TVF Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x3C);
					num_events++;
					break;
				case 0x56:	/* CC2 LFO1 TVA Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x3D);
					num_events++;
					break;
				case 0x57:	/* CC2 LFO2 Rate Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x3E);
					num_events++;
					break;
				case 0x58:	/* CC2 LFO2 Pitch Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x3F);
					num_events++;
					break;
				case 0x59:	/* CC2 LFO2 TVF Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x40);
					num_events++;
					break;
				case 0x5A:	/* CC2 LFO2 TVA Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x41);
					num_events++;
					break;
				default:
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",addr_h,addr_m,addr_l,val[7],val[8]);
					break;
				}
			} else if((addr & 0xFFFF00) == 0x400100) {
				switch(addr & 0xFF) {
				case 0x30:	/* Reverb Macro */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x05);
					num_events++;
					break;
				case 0x31:	/* Reverb Character */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x06);
					num_events++;
					break;
				case 0x32:	/* Reverb Pre-LPF */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x07);
					num_events++;
					break;
				case 0x33:	/* Reverb Level */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x08);
					num_events++;
					break;
				case 0x34:	/* Reverb Time */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x09);
					num_events++;
					break;
				case 0x35:	/* Reverb Delay Feedback */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x0A);
					num_events++;
					break;
				case 0x36:	/* Unknown Reverb Parameter */
					break;
				case 0x37:	/* Reverb Predelay Time */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x0C);
					num_events++;
					break;
				case 0x38:	/* Chorus Macro */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x0D);
					num_events++;
					break;
				case 0x39:	/* Chorus Pre-LPF */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x0E);
					num_events++;
					break;
				case 0x3A:	/* Chorus Level */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x0F);
					num_events++;
					break;
				case 0x3B:	/* Chorus Feedback */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x10);
					num_events++;
					break;
				case 0x3C:	/* Chorus Delay */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x11);
					num_events++;
					break;
				case 0x3D:	/* Chorus Rate */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x12);
					num_events++;
					break;
				case 0x3E:	/* Chorus Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x13);
					num_events++;
					break;
				case 0x3F:	/* Chorus Send Level to Reverb */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x14);
					num_events++;
					break;
				case 0x40:	/* Chorus Send Level to Delay */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x15);
					num_events++;
					break;
				case 0x50:	/* Delay Macro */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x16);
					num_events++;
					break;
				case 0x51:	/* Delay Pre-LPF */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x17);
					num_events++;
					break;
				case 0x52:	/* Delay Time Center */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x18);
					num_events++;
					break;
				case 0x53:	/* Delay Time Ratio Left */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x19);
					num_events++;
					break;
				case 0x54:	/* Delay Time Ratio Right */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x1A);
					num_events++;
					break;
				case 0x55:	/* Delay Level Center */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x1B);
					num_events++;
					break;
				case 0x56:	/* Delay Level Left */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x1C);
					num_events++;
					break;
				case 0x57:	/* Delay Level Right */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x1D);
					num_events++;
					break;
				case 0x58:	/* Delay Level */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x1E);
					num_events++;
					break;
				case 0x59:	/* Delay Feedback */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x1F);
					num_events++;
					break;
				case 0x5A:	/* Delay Send Level to Reverb */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x20);
					num_events++;
					break;
				default:
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",addr_h,addr_m,addr_l,val[7],val[8]);
					break;
				}
			} else if((addr & 0xFFFF00) == 0x400200) {
				switch(addr & 0xFF) {	/* EQ Parameter */
				case 0x00:	/* EQ LOW FREQ */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x01);
					num_events++;
					break;
				case 0x01:	/* EQ LOW GAIN */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x02);
					num_events++;
					break;
				case 0x02:	/* EQ HIGH FREQ */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x03);
					num_events++;
					break;
				case 0x03:	/* EQ HIGH GAIN */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x04);
					num_events++;
					break;
				default:
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",addr_h,addr_m,addr_l,val[7],val[8]);
					break;
				}
			} else if((addr & 0xFFFF00) == 0x400300) {
				int addrl = addr & 0xFF;
				switch(addrl) {	/* Insertion Effect Parameter */
				case 0x00: /* efx type */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x27);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_GS_LSB, p, val[8], 0x28);
					num_events += 2;
					break;
				case 0x03: /* efx parameter 1 */
				case 0x04: /* efx parameter 2 */
				case 0x05: /* efx parameter 3 */
				case 0x06: /* efx parameter 4 */
				case 0x07: /* efx parameter 5 */
				case 0x08: /* efx parameter 6 */
				case 0x09: /* efx parameter 7 */
				case 0x0A: /* efx parameter 8 */
				case 0x0B: /* efx parameter 9 */
				case 0x0C: /* efx parameter 10 */
				case 0x0D: /* efx parameter 11 */
				case 0x0E: /* efx parameter 12 */
				case 0x0F: /* efx parameter 13 */
				case 0x10: /* efx parameter 14 */
				case 0x11: /* efx parameter 15 */
				case 0x12: /* efx parameter 16 */
				case 0x13: /* efx parameter 17 */
				case 0x14: /* efx parameter 18 */
				case 0x15: /* efx parameter 19 */
				case 0x16: /* efx parameter 20 */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], addrl - 0x03 + 0x29); // 0x29-0x3C
					num_events++;
					break;
				case 0x17: /* efx send level to reverb */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x3D);
					num_events++;
					break;
				case 0x18: /* efx send level to chorus */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x3E);
					num_events++;
					break;
				case 0x19: /* efx send level to delay */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x3F);
					num_events++;
					break;
				case 0x1B: /* efx control source 1 */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x40);
					num_events++;
					break;
				case 0x1C: /* efx control depth 1 */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x41);
					num_events++;
					break;
				case 0x1D: /* efx control source 2 */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x42);
					num_events++;
					break;
				case 0x1E: /* efx control depth 2 */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x43);
					num_events++;
					break;
				case 0x1F: /* efx send eq swotch */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x44);
					num_events++;
					break;
				default:
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",addr_h,addr_m,addr_l,val[7],val[8]);
					break;
				}
			} else if((addr & 0xFFF000) == 0x404000) {
				switch(addr & 0xFF) {
				case 0x00:	/* TONE MAP NUMBER */
					SETMIDIEVENT(evm[0], 0, ME_TONE_BANK_LSB, p, val[7], SYSEX_TAG);
					num_events++;
					break;
				case 0x01:	/* TONE MAP-0 NUMBER */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x25);
					num_events++;
					break;
				case 0x20:	/* EQ ON/OFF */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x00);
					num_events++;
					break;
				case 0x22:	/* EFX ON/OFF */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x23);
					num_events++;
					break;
				default:
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",addr_h,addr_m,addr_l,val[7],val[8]);
					break;
				}
			}
			break;
		case 0x41:
			switch(addr & 0xF00) {
			case 0x000:	/* DRUM MAP NAME */
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx DRUM MAP NAME. (ADDR:%02X %02X %02X VAL:%02X %02X)",addr_h,addr_m,addr_l,val[7],val[8]);
				break;
			case 0x100:	/* Play Note Number */
				SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_MSB, dp, val[6], 0);
				SETMIDIEVENT(evm[1], 0, ME_SYSEX_GS_LSB, dp, val[7], 0x47);
				num_events += 2;
				break;
			case 0x200:
				SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, dp, 0x1A, SYSEX_TAG);
				SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, dp, val[6], SYSEX_TAG);
				SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, dp, val[7], SYSEX_TAG);
				num_events += 3;
				break;
			case 0x300:	/* ASSIGN GROUP NUMBER */
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx ASSIGN GROUP NUMBER. (ADDR:%02X %02X %02X VAL:%02X %02X)",addr_h,addr_m,addr_l,val[7],val[8]);
				break;
			case 0x400:
				SETMIDIEVENT(evm[0], 0,ME_NRPN_MSB, dp, 0x1C, SYSEX_TAG);
				SETMIDIEVENT(evm[1], 0,ME_NRPN_LSB, dp, val[6], SYSEX_TAG);
				SETMIDIEVENT(evm[2], 0,ME_DATA_ENTRY_MSB, dp, val[7], SYSEX_TAG);
				num_events += 3;
				break;
			case 0x500:
				SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, dp, 0x1D, SYSEX_TAG);
				SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, dp, val[6], SYSEX_TAG);
				SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, dp, val[7], SYSEX_TAG);
				num_events += 3;
				break;
			case 0x600:
				SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, dp, 0x1E, SYSEX_TAG);
				SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, dp, val[6], SYSEX_TAG);
				SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, dp, val[7], SYSEX_TAG);
				num_events += 3;
				break;
			case 0x700:	/* Rx. Note Off */
				SETMIDIEVENT(evm[0], 0, ME_SYSEX_MSB, dp, val[6], 0);
				SETMIDIEVENT(evm[1], 0, ME_SYSEX_LSB, dp, val[7], 0x46);
				num_events += 2;
				break;
			case 0x800:	/* Rx. Note On */
				SETMIDIEVENT(evm[0], 0, ME_SYSEX_MSB, dp, val[6], 0);
				SETMIDIEVENT(evm[1], 0, ME_SYSEX_LSB, dp, val[7], 0x47);
				num_events += 2;
				break;
			case 0x900:
				SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, dp, 0x1F, SYSEX_TAG);
				SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, dp, val[6], SYSEX_TAG);
				SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, dp, val[7], SYSEX_TAG);
				num_events += 3;
				break;
			default:
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",addr_h,addr_m,addr_l,val[7],val[8]);
				break;
			}
			break;
#if 0
		case 0x20:	/* User Instrument */
			switch(addr & 0xF00) {
				case 0x000:	/* Source Map */
					get_userinst(64 + udn, val[6])->source_map = val[7];
					break;
				case 0x100:	/* Source Bank */
					get_userinst(64 + udn, val[6])->source_bank = val[7];
					break;
#if !defined(TIMIDITY_TOOLS)
				case 0x200:	/* Source Prog */
					get_userinst(64 + udn, val[6])->source_prog = val[7];
					break;
#endif
				default:
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",addr_h,addr_m,addr_l,val[7],val[8]);
					break;
			}
			break;
#endif
		case 0x21:	/* User Drumset */
			switch(addr & 0xF00) {
				case 0x100:	/* Play Note */
					get_userdrum(64 + udn, val[6])->play_note = val[7];
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_MSB, dp, val[6], 0);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_GS_LSB, dp, val[7], 0x47);
					num_events += 2;
					break;
				case 0x200:	/* Level */
					get_userdrum(64 + udn, val[6])->level = val[7];
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, dp, 0x1A, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, dp, val[6], SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, dp, val[7], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x300:	/* Assign Group */
					get_userdrum(64 + udn, val[6])->assign_group = val[7];
					if(val[7] != 0) {recompute_userdrum_altassign(udn + 64, val[7]);}
					break;
				case 0x400:	/* Panpot */
					get_userdrum(64 + udn, val[6])->pan = val[7];
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, dp, 0x1C, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, dp, val[6], SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, dp, val[7], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x500:	/* Reverb Send Level */
					get_userdrum(64 + udn, val[6])->reverb_send_level = val[7];
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, dp, 0x1D, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, dp, val[6], SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, dp, val[7], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x600:	/* Chorus Send Level */
					get_userdrum(64 + udn, val[6])->chorus_send_level = val[7];
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, dp, 0x1E, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, dp, val[6], SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, dp, val[7], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x700:	/* Rx. Note Off */
					get_userdrum(64 + udn, val[6])->rx_note_off = val[7];
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_MSB, dp, val[6], 0);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_LSB, dp, val[7], 0x46);
					num_events += 2; 
					break;
				case 0x800:	/* Rx. Note On */
					get_userdrum(64 + udn, val[6])->rx_note_on = val[7];
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_MSB, dp, val[6], 0);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_LSB, dp, val[7], 0x47);
					num_events += 2; 
					break;
				case 0x900:	/* Delay Send Level */
					get_userdrum(64 + udn, val[6])->delay_send_level = val[7];
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, dp, 0x1F, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, dp, val[6], SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, dp, val[7], SYSEX_TAG);
					num_events += 3;
					break;
				case 0xA00:	/* Source Map */
					get_userdrum(64 + udn, val[6])->source_map = val[7];
					break;
				case 0xB00:	/* Source Prog */
					get_userdrum(64 + udn, val[6])->source_prog = val[7];
					break;
#if !defined(TIMIDITY_TOOLS)
				case 0xC00:	/* Source Note */
					get_userdrum(64 + udn, val[6])->source_note = val[7];
					break;
#endif
				default:
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",addr_h,addr_m,addr_l,val[7],val[8]);
					break;
			}
			break;
		case 0x00:	/* System */
			switch (addr & 0xfff0) {
			case 0x0100:	/* Channel Msg Rx Port (A) */
				SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB,
						block_to_part(addr & 0xf, 0), val[7], 0x46);
				num_events++;
				break;
			case 0x0110:	/* Channel Msg Rx Port (B) */
				SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB,
						block_to_part(addr & 0xf, 1), val[7], 0x46);
				num_events++;
				break;
			default:
			/*	ctl->cmsg(CMSG_INFO,VERB_NOISY, "Unsupported GS SysEx. "
						"(ADDR:%02X %02X %02X VAL:%02X %02X)",
						addr_h, addr_m, addr_l, val[7], val[8]);*/
				break;
			}
			break;
		}
    }

    /* parsing SD System Exclusive Message...
     * val[5] == Parameter Address(1
     * val[6] == Parameter Address(2
     * val[7] == Parameter Address(3
     * val[8] == Parameter Address(4
     * val[9]... == Data...
     * val[last] == Checksum(== 128 - (sum of addresses&data bytes % 128)) 
     */
    else if(len >= 10 &&
       val[0] == 0x41 && /* Roland ID */
       val[1] == 0x10 && /* Device ID */
       val[2] == 0x00 && /* Model ID */
       (val[3] >= 0x20 && val[3] <= 0x7F)&& /*  Model ID */
       val[4] == 0x12) /* Data Set Command */
    {
		uint8 sdlen;
		int i, checksum;
		int mfx, part, tmp1, tmp2;
		int16 dbyte, dbyte2, dbyte3;
#ifdef _DEBUG
		uint8 sysex[32];

		memset(sysex, 0, sizeof(sysex));
		for(i = 0; i < len; i++){
			sysex[i] = val[i];
		}
#endif

		/* calculate checksum */
#if 1 // 怪しい・・?
		checksum = 0;
		for(sdlen = 10; sdlen < len; sdlen++)
			if(val[sdlen] == 0xF7)
				break;
		for(i = 5; i < sdlen - 1; i++) {
			checksum += val[i];
		}
		if(((128 - (checksum & 0x7F)) & 0x7F) != val[sdlen - 1]) {
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"SD SysEx: Checksum Error.");
			return num_events;
		}
#endif


		switch(val[5]){
		case 0x00: // Setup
			break;
		case 0x01: // System
			if(val[6] != 0x00){
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. (ADDR:%02X %02X %02X %02X)",val[5],val[6],val[7],val[8]);
				break;
			}
			switch(val[7]){
			case 0x00: // System Common
				switch(val[8]){
				case 0x00: // Master Tune
				case 0x04: // Master key Shift
				case 0x05: // Master Level
				case 0x06: // System Control 1 source
				case 0x07: // System Control 2 source
				case 0x08: // System Control 3 source
				case 0x09: // System Control 4 source
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. (ADDR:%02X %02X %02X %02X)",val[5],val[6],val[7],val[8]);
					break;
				default:
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. (ADDR:%02X %02X %02X %02X)",val[5],val[6],val[7],val[8]);
					break;
				}
				break;
			case 0x20: // System EQ
				switch(val[8]){
				case 0x00: // EQ Switch
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x02);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x00);
					num_events += 3;
					break;
				case 0x01: // EQ Left Low Frequency
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x02);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x01);
					num_events += 3;
					break;
				case 0x02: // EQ Left Low Gain
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x02);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x02);
					num_events += 3;
					break;
				case 0x03: // EQ Left High Frequency
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x02);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x03);
					num_events += 3;
					break;
				case 0x04: // EQ Left High Gain
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x02);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x04);
					num_events += 3;
					break;
				case 0x05: // EQ Right Low Frequency
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x02);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x05);
					num_events += 3;
					break;
				case 0x06: // EQ Right Low Gain
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x02);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x06);
					num_events += 3;
					break;
				case 0x07: // EQ Right High Frequency
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x02);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x07);
					num_events += 3;
					break;
				case 0x08: // EQ Right High Gain
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x02);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x08);
					num_events += 3;
					break;
				default:
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. (ADDR:%02X %02X %02X %02X)",val[5],val[6],val[7],val[8]);
					break;
				}
				break;
			}
			break;
		case 0x02: // Audio
			break;
		case 0x10: // Temporary Multitimbre
			if(val[6] != 0x00){
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. (ADDR:%02X %02X %02X %02X)",val[5],val[6],val[7],val[8]);
				break;
			}
			switch(val[7]){
			case 0x00: // Multitimbre Common
				switch(val[8]){
				case 0x00: // Multitimbre Name 1
				case 0x01: // Multitimbre Name 2
				case 0x02: // Multitimbre Name 3
				case 0x03: // Multitimbre Name 4
				case 0x04: // Multitimbre Name 5
				case 0x05: // Multitimbre Name 6
				case 0x06: // Multitimbre Name 7
				case 0x07: // Multitimbre Name 8
				case 0x08: // Multitimbre Name 9
				case 0x09: // Multitimbre Name 10
				case 0x0A: // Multitimbre Name 11
				case 0x0B: // Multitimbre Name 12
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. (ADDR:%02X %02X %02X %02X)",val[5],val[6],val[7],val[8]);
					break;
				case 0x0C: // Solo Part Select
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. (ADDR:%02X %02X %02X %02X)",val[5],val[6],val[7],val[8]);
					break;
				case 0x10: // Voice Reserve 1
				case 0x11: // Voice Reserve 2
				case 0x12: // Voice Reserve 3
				case 0x13: // Voice Reserve 4
				case 0x14: // Voice Reserve 5
				case 0x15: // Voice Reserve 6
				case 0x16: // Voice Reserve 7
				case 0x17: // Voice Reserve 8
				case 0x18: // Voice Reserve 9
				case 0x19: // Voice Reserve 10
				case 0x1A: // Voice Reserve 11
				case 0x1B: // Voice Reserve 12
				case 0x1C: // Voice Reserve 13
				case 0x1D: // Voice Reserve 14
				case 0x1E: // Voice Reserve 15
				case 0x1F: // Voice Reserve 16
				case 0x20: // Voice Reserve 17
				case 0x21: // Voice Reserve 18
				case 0x22: // Voice Reserve 19
				case 0x23: // Voice Reserve 20
				case 0x24: // Voice Reserve 21
				case 0x25: // Voice Reserve 22
				case 0x26: // Voice Reserve 23
				case 0x27: // Voice Reserve 24
				case 0x28: // Voice Reserve 25
				case 0x29: // Voice Reserve 26
				case 0x2A: // Voice Reserve 27
				case 0x2B: // Voice Reserve 28
				case 0x2C: // Voice Reserve 29
				case 0x2D: // Voice Reserve 30
				case 0x2E: // Voice Reserve 31
				case 0x2F: // Voice Reserve 32
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. (ADDR:%02X %02X %02X %02X)",val[5],val[6],val[7],val[8]);
					break;
				case 0x30: // MFX1 Source
				case 0x31: // MFX2 Source
				case 0x32: // MFX3 Source
					tmp1 = (val[8] - 0x30);
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x07);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, tmp1, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x30);
					num_events += 3;
					break;
				case 0x33: // Chorus Source
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x07);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x33);
					num_events += 3;
					break;
				case 0x34: // Reverb Source
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x07);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x34);
					num_events += 3;
					break;
				case 0x35: // MFX1 Control Channel
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x07);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x35);
					num_events += 3;
					break;
				case 0x36: // MFX1 Control Port
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x07);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x36);
					num_events += 3;
					break;
				case 0x37: // MFX2 Control Channel
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x07);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x37);
					num_events += 3;
					break;
				case 0x38: // MFX2 Control Port
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x07);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x38);
					num_events += 3;
					break;
				case 0x39: // MFX3 Control Channel
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x07);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x39);
					num_events += 3;
					break;
				case 0x3A: // MFX3 Control Port
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x07);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x3A);
					num_events += 3;
					break;
				default:
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. (ADDR:%02X %02X %02X %02X)",val[5],val[6],val[7],val[8]);
					break;
				}
				break;
			case 0x02: // Multitimbre Common Chorus
				switch(val[8]){
				case 0x00: // Chorus Type
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x08);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x00);
					num_events += 3;
					break;
				case 0x01: // Chorus Level
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x08);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x01);
					num_events += 3;
					break;
				case 0x03: // Chorus Output Select
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x08);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x02);
					num_events += 3;
					break;
				case 0x04: // Chorus Parameter 1
				case 0x08: // Chorus Parameter 2
				case 0x0c: // Chorus Parameter 3
				case 0x10: // Chorus Parameter 4
				case 0x14: // Chorus Parameter 5
				case 0x18: // Chorus Parameter 6
				case 0x1C: // Chorus Parameter 7
				case 0x20: // Chorus Parameter 8
				case 0x24: // Chorus Parameter 9
				case 0x28: // Chorus Parameter 10
				case 0x2C: // Chorus Parameter 11
				case 0x30: // Chorus Parameter 12	
					{
					int num = (val[8] - 0x04) >> 2;
					int byteh = ((val[9] & 0x0F) << 4) | (val[10] & 0x0F);
					int bytel = ((val[11] & 0x0F) << 4) | (val[12] & 0x0F);
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, byteh, 0x08);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, bytel, 0x03 + num);
					num_events += 3;
					}
					break;
				default:
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. (ADDR:%02X %02X %02X %02X)",val[5],val[6],val[7],val[8]);
					break;
				}
				break;
			case 0x04: // Multitimbre Common Reverb
				switch(val[8]){
				case 0x00: // Reverb Type
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x09);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x00);
					num_events += 3;
					break;
				case 0x01: // Reverb Level
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x09);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x01);
					num_events += 3;
					break;
				case 0x03: // Reverb Parameter 1
				case 0x07: // Reverb Parameter 2
				case 0x0B: // Reverb Parameter 3
				case 0x0F: // Reverb Parameter 4
				case 0x13: // Reverb Parameter 5
				case 0x17: // Reverb Parameter 6
				case 0x1B: // Reverb Parameter 7
				case 0x1F: // Reverb Parameter 8
				case 0x23: // Reverb Parameter 9
				case 0x27: // Reverb Parameter 10
				case 0x2B: // Reverb Parameter 11
				case 0x2F: // Reverb Parameter 12	
				case 0x33: // Reverb Parameter 13
				case 0x37: // Reverb Parameter 14
				case 0x3B: // Reverb Parameter 15
				case 0x3F: // Reverb Parameter 16	
				case 0x43: // Reverb Parameter 17
				case 0x47: // Reverb Parameter 18
				case 0x4B: // Reverb Parameter 19
				case 0x4F: // Reverb Parameter 20	
					{
					int num = (val[8] - 0x03) >> 2;
					int byteh = ((val[9] & 0x0F) << 4) | (val[10] & 0x0F);
					int bytel = ((val[11] & 0x0F) << 4) | (val[12] & 0x0F);
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, byteh, 0x09);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, bytel, 0x02 + num);
					num_events += 3;
					}
					break;
				default:
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. (ADDR:%02X %02X %02X %02X)",val[5],val[6],val[7],val[8]);
					break;
				}
				break;
			case 0x06: // Multitimbre Common MFX1
			case 0x07: // Multitimbre Common MFX1
			case 0x08: // Multitimbre Common MFX2
			case 0x09: // Multitimbre Common MFX2
			case 0x0A: // Multitimbre Common MFX3
			case 0x0B: // Multitimbre Common MFX3
				mfx = (val[7] - 0x06) >> 1;
				dbyte = (((uint16)val[7] & 0x01) << 8) | val[8];
				switch(dbyte){
				case 0x0000: // MFX Type
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x0A);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, mfx, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x00);
					num_events += 3;
					break;
				case 0x0001: // MFX Dry Send Level
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x0A);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, mfx, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x01);
					num_events += 3;
					break;
				case 0x0002: // MFX Chorus Send Level
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x0A);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, mfx, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x02);
					num_events += 3;
					break;
				case 0x0003: // MFX Reverb Send Level
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x0A);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, mfx, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x03);
					num_events += 3;
					break;
				case 0x0005: // MFX Control 1 Source
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x0A);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, mfx, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x05);
					num_events += 3;
					break;
				case 0x0006: // MFX Control 1 Sens 
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x0A);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, mfx, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x06);
					num_events += 3;
					break;
				case 0x0007: // MFX Control 2 Source
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x0A);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, mfx, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x07);
					num_events += 3;
					break;
				case 0x0008: // MFX Control 2 Sens 
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x0A);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, mfx, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x08);
					num_events += 3;
					break;
				case 0x0009: // MFX Control 3 Source
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x0A);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, mfx, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x09);
					num_events += 3;
					break;
				case 0x000A: // MFX Control 3 Sens 
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x0A);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, mfx, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x0A);
					num_events += 3;
					break;
				case 0x000B: // MFX Control 4 Source
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x0A);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, mfx, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x0B);
					num_events += 3;
					break;
				case 0x000C: // MFX Control 4 Sens 
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x0A);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, mfx, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x0C);
					num_events += 3;
					break;
				case 0x000D: // MFX Control Assign 1
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x0A);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, mfx, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x0D);
					num_events += 3;
					break;
				case 0x000E: // MFX Control Assign 2
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x0A);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, mfx, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x0E);
					num_events += 3;
					break;
				case 0x000F: // MFX Control Assign 3
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x0A);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, mfx, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x0F);
					num_events += 3;
					break;
				case 0x0010: // MFX Control Assign 4
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x0A);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, mfx, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x10);
					num_events += 3;
					break;
				case 0x0011: // MFX Parameter 1
				case 0x0015: // MFX Parameter 2
				case 0x0019: // MFX Parameter 3
				case 0x001D: // MFX Parameter 4
				case 0x0021: // MFX Parameter 5
				case 0x0025: // MFX Parameter 6
				case 0x0029: // MFX Parameter 7
				case 0x002D: // MFX Parameter 8
				case 0x0031: // MFX Parameter 9
				case 0x0035: // MFX Parameter 10
				case 0x0039: // MFX Parameter 11
				case 0x003D: // MFX Parameter 12
				case 0x0041: // MFX Parameter 13
				case 0x0045: // MFX Parameter 14
				case 0x0049: // MFX Parameter 15
				case 0x004D: // MFX Parameter 16
				case 0x0051: // MFX Parameter 17
				case 0x0055: // MFX Parameter 18
				case 0x0059: // MFX Parameter 19
				case 0x005D: // MFX Parameter 20
				case 0x0061: // MFX Parameter 21
				case 0x0065: // MFX Parameter 22
				case 0x0069: // MFX Parameter 23
				case 0x006D: // MFX Parameter 24
				case 0x0071: // MFX Parameter 25
				case 0x0075: // MFX Parameter 26
				case 0x0079: // MFX Parameter 27
				case 0x007D: // MFX Parameter 28
				case 0x0101: // MFX Parameter 29
				case 0x0105: // MFX Parameter 30
				case 0x0109: // MFX Parameter 31
				case 0x010D: // MFX Parameter 32
					{
					int num = (dbyte > 0x0100) ? ((val[8] - 0x1) >> 2 + 28) : ((val[8] - 0x11) >> 2);
					int byteh = ((val[9] & 0x0F) << 4) | (val[10] & 0x0F);
					int bytel = ((val[11] & 0x0F) << 4) | (val[12] & 0x0F);
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, byteh, 0x0A);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, mfx, 0);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, bytel, 0x11 + num);
					num_events += 3;
					}
					break;
				default:
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. (ADDR:%02X %02X %02X %02X)",val[5],val[6],val[7],val[8]);
					break;
				}
				break;
			case 0x20: // Multitimbre Part (part 1
			case 0x21: // Multitimbre Part (part 2
			case 0x22: // Multitimbre Part (part 3
			case 0x23: // Multitimbre Part (part 4
			case 0x24: // Multitimbre Part (part 5
			case 0x25: // Multitimbre Part (part 6
			case 0x26: // Multitimbre Part (part 7
			case 0x27: // Multitimbre Part (part 8
			case 0x28: // Multitimbre Part (part 9
			case 0x29: // Multitimbre Part (part 10
			case 0x2A: // Multitimbre Part (part 11
			case 0x2B: // Multitimbre Part (part 12
			case 0x2C: // Multitimbre Part (part 13
			case 0x2D: // Multitimbre Part (part 14
			case 0x2E: // Multitimbre Part (part 15
			case 0x2F: // Multitimbre Part (part 16
			case 0x30: // Multitimbre Part (part 17
			case 0x31: // Multitimbre Part (part 18
			case 0x32: // Multitimbre Part (part 19
			case 0x33: // Multitimbre Part (part 20
			case 0x34: // Multitimbre Part (part 21
			case 0x35: // Multitimbre Part (part 22
			case 0x36: // Multitimbre Part (part 23
			case 0x37: // Multitimbre Part (part 24
			case 0x38: // Multitimbre Part (part 25
			case 0x39: // Multitimbre Part (part 26
			case 0x3A: // Multitimbre Part (part 27
			case 0x3B: // Multitimbre Part (part 28
			case 0x3C: // Multitimbre Part (part 29
			case 0x3D: // Multitimbre Part (part 30
			case 0x3E: // Multitimbre Part (part 31
			case 0x3F: // Multitimbre Part (part 32
				part = val[7] - 0x20;
				switch(val[8]){
				case 0x00: // Receive Channel
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_XG_LSB, part, val[9], 0x99);
					num_events++;
					break;
				case 0x01: // Receive Switch
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. Receive Switch");
					break;
				case 0x03: // Receive MIDI Port
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. Receive MIDI Port");
					break;
				case 0x04: // Patch Bank Select MSB (CC# 0
					SETMIDIEVENT(evm[0], 0, ME_TONE_BANK_MSB, part, val[9], SYSEX_TAG);
					num_events++;
					break;
				case 0x05: // Patch Bank Select LSB (CC# 32
					SETMIDIEVENT(evm[0], 0, ME_TONE_BANK_LSB, part, val[9], SYSEX_TAG);
					num_events++;
					break;
				case 0x06: // Patch Program Number (PC
					SETMIDIEVENT(evm[0], 0, ME_PROGRAM, part, val[9], SYSEX_TAG);
					num_events++;
					break;
				case 0x07: // Part Level (CC# 7
					SETMIDIEVENT(evm[0], 0, ME_MAINVOLUME, part, val[9], SYSEX_TAG);
					num_events++;
					break;
				case 0x08: // Part Pan (CC# 10
					SETMIDIEVENT(evm[0], 0, ME_PAN, part, val[9], SYSEX_TAG);
					num_events++;
					break;
				case 0x09: // Part Coarse Tune (RPN# 2
					SETMIDIEVENT(evm[0], 0, ME_RPN_MSB, part, 0, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_RPN_LSB, part, 2, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, part, val[9], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x0A: // Part Fine Tune (RPN# 1
					SETMIDIEVENT(evm[0], 0, ME_RPN_MSB, part, 0, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_RPN_LSB, part, 1, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, part, val[9], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x0B: // Part Mono/Poly (MONO ON/POLY ON
					if(val[9] == 0)
						{SETMIDIEVENT(evm[0], 0, ME_MONO, part, 0, SYSEX_TAG);}
					else 
						{SETMIDIEVENT(evm[0], 0, ME_POLY, part, 0, SYSEX_TAG);}
					num_events++;
					break;
				case 0x0C: // Part Legato Switch (CC# 68
					SETMIDIEVENT(evm[0], 0, ME_LEGATO_FOOTSWITCH, part, val[9], SYSEX_TAG);
					num_events++;
					break;
				case 0x0D: // Part Pitch Bend Range (RPN# 0
					if(val[9] >= 25){
						ctl->cmsg(CMSG_INFO,VERB_NOISY,"SD SysEx. Part Pitch Bend Range (Unsupported PATCH)");
					}else{
						SETMIDIEVENT(evm[0], 0, ME_RPN_MSB, part, 0, SYSEX_TAG);
						SETMIDIEVENT(evm[1], 0, ME_RPN_LSB, part, 0, SYSEX_TAG);
						SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, part, val[9], SYSEX_TAG); // range 0-24
						num_events += 3;
					}
					break;
				case 0x0E: // Part Portamento Switch (CC# 65
					SETMIDIEVENT(evm[0], 0, ME_PORTAMENTO, part, val[9], SYSEX_TAG);
					num_events++;
					break;
				case 0x0F: // Part Portamento Time (CC# 5
					SETMIDIEVENT(evm[0], 0, ME_PORTAMENTO_TIME_MSB, part, val[9], SYSEX_TAG);
					num_events++;
					break;
				case 0x11: // Part Cutoff Offset (CC# 74
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, part, 0x01, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, part, 0x20, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, part, val[9], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x12: // Part Resonance Offset (CC# 71
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, part, 0x01, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, part, 0x21, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, part, val[9], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x13: // Part Attack Time Offset (CC# 73
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, part, 0x01, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, part, 0x63, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, part, val[9], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x14: // Part Release Time Offset (CC# 72
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, part, 0x01, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, part, 0x66, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, part, val[9], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x15: // Part Octave 
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. Part Octave");
					break;
				case 0x16: // Part Velocity Sens Offset					
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_GS_LSB, part, val[9], 0x22);
					num_events++;
					break;
				case 0x17: // Part Keyboard Range Lower
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, part, val[9], 0x42);
					num_events++;
					break;
				case 0x18: // Part Keyboard Range Upper
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, part, val[9], 0x43);
					num_events++;
					break;
				case 0x19: // Part Keyboard Fade Width Lower
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. Part Keyboard Fade Width Lower");
					break;
				case 0x1A: // Part Keyboard Fade Width Upper
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. Part Keyboard Fade Width Upper");
					break;
				case 0x1B: // Part Mute Switch
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. Part Mute Switch");
					break;
				case 0x1C: // Part Dry Send Level
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x0B);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, part);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x1C);
					num_events += 3;		
					break;
				case 0x1D: // Part Chorus Send Level (CC# 93
					SETMIDIEVENT(evm[0], 0, ME_CHORUS_EFFECT, part, val[9], SYSEX_TAG);
					num_events++;
					break;
				case 0x1E: // Part Reverb Send Level (CC# 91
					SETMIDIEVENT(evm[0], 0, ME_REVERB_EFFECT, part, val[9], SYSEX_TAG);
					num_events++;
					break;
				case 0x1F: // Part Output Assign
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x0B);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, part);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x1F);
					num_events += 3;		
					break;
				case 0x20: // Part Output MFX Select
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x0B);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, part);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x20);
					num_events += 3;
					break;
				case 0x21: // Part Decay Time Offset (CC# 75
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, part, 0x01, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, part, 0x64, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, part, val[9], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x22: // Part Vibrato Rate (CC# 76
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, part, 0x01, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, part, 0x08, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, part, val[9], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x23: // Part Vibrato Depth (CC# 77
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, part, 0x01, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, part, 0x09, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, part, val[9], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x24: // Part Vibrato Decay (CC# 78
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, part, 0x01, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, part, 0x0A, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, part, val[9], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x25: // Mod LFO Pitch Depth
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, part, val[9], 0x1A);
				//	ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. (ADDR:%02X %02X %02X %02X)",val[5],val[6],val[7],val[8]);
					break;
				case 0x26: // CAf Pithch Control
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, part, val[9], 0x16);
					num_events++;
					break;
				case 0x27: // CAf TVF Cutoff Control
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, part, val[9], 0x17);
					num_events++;
					break;
				case 0x28: // CAf Amplitude Control
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, part, val[9], 0x18);
					num_events++;
					break;
				case 0x29: // CAf LFO Pitch Depth
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, part, val[9], 0x1A);
					num_events++;
					break;
				case 0x2A: // CAf LFO TVF Depth
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, part, val[9], 0x1B);
					num_events++;
					break;
				case 0x2B: // CAf LFO TVA Depth
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, part, val[9], 0x1C);
					num_events++;
					break;
				case 0x2C: // CC Control number
					if(val[9] < 0 || (val[9] > 31 && val[9] < 64) || val[9] > 95){
						SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, part, -1, 0x6D); // off
					}else{
						SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, part, val[9], 0x6D);
					}
					num_events++;
					break;
				case 0x2D: // CC Pithch Control
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, part, val[9], 0x2C);
					num_events++;
					break;
				case 0x2E: // CC TVF Cutoff Control
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, part, val[9], 0x2D);
					num_events++;
					break;
				case 0x2F: // CC Amplitude Control
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, part, val[9], 0x2E);
					num_events++;
					break;
				case 0x30: // CC LFO Pitch Depth
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, part, val[9], 0x30);
					num_events++;
					break;
				case 0x31: // CC LFO TVF Depth
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, part, val[9], 0x31);
					num_events++;
					break;
				case 0x32: // CC LFO TVA Depth
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, part, val[9], 0x32);
					num_events++;
					break;
				case 0x33: // Part Scale Tune for C
				case 0x34: // Part Scale Tune for C#
				case 0x35: // Part Scale Tune for D
				case 0x36: // Part Scale Tune for D#
				case 0x37: // Part Scale Tune for E
				case 0x38: // Part Scale Tune for F
				case 0x39: // Part Scale Tune for F#
				case 0x3A: // Part Scale Tune for G
				case 0x3B: // Part Scale Tune for G#
				case 0x3C: // Part Scale Tune for A
				case 0x3D: // Part Scale Tune for A#
				case 0x3E: // Part Scale Tune for B
					SETMIDIEVENT(evm[num_events], 0, ME_SCALE_TUNING, part, val[8] - 0x33, val[9]);
					num_events++;
					ctl->cmsg(CMSG_INFO, VERB_NOISY, "Part Scale Tuning %s (CH:%d %d cent)",
						  note_name[val[8] - 0x33], part, val[9]);
					break;
				case 0x3F: // GM2 Instrument Select 
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, part, val[9], 0x6C);
					num_events++;
					ctl->cmsg(CMSG_INFO, VERB_NOISY, "Part GM2 Instrument Select (CH:%d VAL:%d)", part, val[9]);
					break;
				}	
				break;
			case 0x40: // Multitimbre MIDI (port A / channel 1
			case 0x41: // Multitimbre MIDI (port A / channel 2
			case 0x42: // Multitimbre MIDI (port A / channel 3
			case 0x43: // Multitimbre MIDI (port A / channel 4
			case 0x44: // Multitimbre MIDI (port A / channel 5
			case 0x45: // Multitimbre MIDI (port A / channel 6
			case 0x46: // Multitimbre MIDI (port A / channel 7
			case 0x47: // Multitimbre MIDI (port A / channel 8
			case 0x48: // Multitimbre MIDI (port A / channel 9
			case 0x49: // Multitimbre MIDI (port A / channel 10
			case 0x4A: // Multitimbre MIDI (port A / channel 11
			case 0x4B: // Multitimbre MIDI (port A / channel 12
			case 0x4C: // Multitimbre MIDI (port A / channel 13
			case 0x4D: // Multitimbre MIDI (port A / channel 14
			case 0x4E: // Multitimbre MIDI (port A / channel 15
			case 0x4F: // Multitimbre MIDI (port A / channel 16
			case 0x50: // Multitimbre MIDI (port B / channel 1
			case 0x51: // Multitimbre MIDI (port B / channel 2
			case 0x52: // Multitimbre MIDI (port B / channel 3
			case 0x53: // Multitimbre MIDI (port B / channel 4
			case 0x54: // Multitimbre MIDI (port B / channel 5
			case 0x55: // Multitimbre MIDI (port B / channel 6
			case 0x56: // Multitimbre MIDI (port B / channel 7
			case 0x57: // Multitimbre MIDI (port B / channel 8
			case 0x58: // Multitimbre MIDI (port B / channel 9
			case 0x59: // Multitimbre MIDI (port B / channel 10
			case 0x5A: // Multitimbre MIDI (port B / channel 11
			case 0x5B: // Multitimbre MIDI (port B / channel 12
			case 0x5C: // Multitimbre MIDI (port B / channel 13
			case 0x5D: // Multitimbre MIDI (port B / channel 14
			case 0x5E: // Multitimbre MIDI (port B / channel 15
			case 0x5F: // Multitimbre MIDI (port B / channel 16				
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. Multitimbre MIDI (ADDR:%02X %02X %02X %02X)",val[5],val[6],val[7],val[8]);
				break;
			default:
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. (ADDR:%02X %02X %02X %02X)",val[5],val[6],val[7],val[8]);
				break;
			}
			break;
		case 0x11: // Temporary Patch/Rhythm part 1~32
		case 0x12: // 
		case 0x13: // 
		case 0x14: // 
		case 0x15: // 
		case 0x16: // 
		case 0x17: // 
		case 0x18: // 
			part = ((uint16)val[5] - 0x11) * 4 + val[6] >> 5;
			switch(((uint16)val[6] & 0x11) << 8 | val[7]){
			// Patch
			case 0x0000: // Patch Common
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. Patch Common (ADDR:%02X %02X %02X %02X)",val[5],val[6],val[7],val[8]);
				break;
			case 0x0002: // Patch Common Chorus
			case 0x1002: // Rhythm Common Chorus
				dbyte = (((uint16)val[7] & 0x01) << 8) | val[8];
				switch(dbyte){
				case 0x0000: // Chorus Type					
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x0E);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, part);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x00);
					num_events += 3;
					break;
				case 0x0001: // Chorus Level					
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x0E);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, part);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x01);
					num_events += 3;
					break;
				case 0x0003: // Chorus Output Select				
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x0E);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, part);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x02);
					num_events += 3;
					break;
				case 0x0004: // Chorus Parameter 1
				case 0x0008: // Chorus Parameter 2
				case 0x000C: // Chorus Parameter 3
				case 0x0010: // Chorus Parameter 4
				case 0x0014: // Chorus Parameter 5
				case 0x0018: // Chorus Parameter 6
				case 0x001C: // Chorus Parameter 7
				case 0x0020: // Chorus Parameter 8
				case 0x0024: // Chorus Parameter 9
				case 0x0028: // Chorus Parameter 10
				case 0x002C: // Chorus Parameter 11
				case 0x0030: // Chorus Parameter 12
					{
					int num = (val[7] & 0x01) ? ((val[8] - 0x1) >> 2 + 28) : ((val[8] - 0x0004) >> 2);
					int byteh = ((val[9] & 0x0F) << 4) | (val[10] & 0x0F);
					int bytel = ((val[11] & 0x0F) << 4) | (val[12] & 0x0F);
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, byteh, 0x0E);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, part);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, bytel, 0x03 + num);
					num_events += 3;
					}
					break;
				}
				break;
			case 0x0004: // Patch Common Reverb
			case 0x1004: // Rhythm Common Reverb
				dbyte = (((uint16)val[7] & 0x01) << 8) | val[8];
				switch(dbyte){
				case 0x0000: // Reverb Type					
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x0F);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, part);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x00);
					num_events += 3;
					break;
				case 0x0001: // Reverb Level					
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x0F);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, part);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x01);
					num_events += 3;
					break;
				case 0x0003: // Reverb Parameter 1
				case 0x0007: // Reverb Parameter 2
				case 0x000B: // Reverb Parameter 3
				case 0x000F: // Reverb Parameter 4
				case 0x0013: // Reverb Parameter 5
				case 0x0017: // Reverb Parameter 6
				case 0x001B: // Reverb Parameter 7
				case 0x001F: // Reverb Parameter 8
				case 0x0023: // Reverb Parameter 9
				case 0x0027: // Reverb Parameter 10
				case 0x002B: // Reverb Parameter 11
				case 0x002F: // Reverb Parameter 12
				case 0x0033: // Reverb Parameter 13
				case 0x0037: // Reverb Parameter 14
				case 0x003B: // Reverb Parameter 15
				case 0x003F: // Reverb Parameter 16
				case 0x0043: // Reverb Parameter 17
				case 0x0047: // Reverb Parameter 18
				case 0x004B: // Reverb Parameter 19
				case 0x004F: // Reverb Parameter 20
					{
					int num = (val[7] & 0x01) ? ((val[8] - 0x1) >> 2 + 28) : ((val[8] - 0x0003) >> 2);
					int byteh = ((val[9] & 0x0F) << 4) | (val[10] & 0x0F);
					int bytel = ((val[11] & 0x0F) << 4) | (val[12] & 0x0F);
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, byteh, 0x0F);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, part);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, bytel, 0x02 + num);
					num_events += 3;
					}
					break;
				}
				break;
			case 0x0006: // Patch Common MFX
			case 0x1006: // Rhythm Common MFX
				dbyte = (((uint16)val[7] & 0x01) << 8) | val[8];
				switch(dbyte){
				case 0x0000: // MFX Type					
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x10);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, part);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x00);
					num_events += 3;
					break;
				case 0x0001: // MFX Dry Send Level					
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x10);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, part);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x01);
					num_events += 3;
					break;
				case 0x0002: // MFX Chorus Send Level					
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x10);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, part);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x02);
					num_events += 3;
					break;
				case 0x0003: // MFX Reverb Send Level					
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x10);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, part);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x03);
					num_events += 3;
					break;
				case 0x0005: // MFX Control 1 Source				
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x10);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, part);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x05);
					num_events += 3;
					break;
				case 0x0006: // MFX Control 1 Sens					
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x10);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, part);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x06);
					num_events += 3;
					break;
				case 0x0007: // MFX Control 2 Source					
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x10);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, part);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x07);
					num_events += 3;
					break;
				case 0x0008: // MFX Control 2 Sens					
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x10);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, part);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x08);
					num_events += 3;
					break;
				case 0x0009: // MFX Control 3 Source						
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x10);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, part);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x09);
					num_events += 3;
					break;
				case 0x000A: // MFX Control 3 Sens							
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x10);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, part);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x0A);
					num_events += 3;
					break;
				case 0x000B: // MFX Control 4 Source					
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x10);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, part);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x0B);
					num_events += 3;
					break;
				case 0x000C: // MFX Control 4 Sens					
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x10);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, part);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x0C);
					num_events += 3;
					break;
				case 0x000D: // MFX Control Assign 1					
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x10);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, part);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x0D);
					num_events += 3;
					break;
				case 0x000E: // MFX Control Assign 2				
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x10);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, part);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x0E);
					num_events += 3;
					break;
				case 0x000F: // MFX Control Assign 3				
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x10);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, part);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x0F);
					num_events += 3;
					break;
				case 0x0010: // MFX Control Assign 4				
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, 0, 0x10);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, part);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, val[9], 0x10);
					num_events += 3;
					break;
				case 0x0011: // MFX Parameter 1
				case 0x0015: // MFX Parameter 2
				case 0x0019: // MFX Parameter 3
				case 0x001D: // MFX Parameter 4
				case 0x0021: // MFX Parameter 5
				case 0x0025: // MFX Parameter 6
				case 0x0029: // MFX Parameter 7
				case 0x002D: // MFX Parameter 8
				case 0x0031: // MFX Parameter 9
				case 0x0035: // MFX Parameter 10
				case 0x0039: // MFX Parameter 11
				case 0x003D: // MFX Parameter 12
				case 0x0041: // MFX Parameter 13
				case 0x0045: // MFX Parameter 14
				case 0x0049: // MFX Parameter 15
				case 0x004D: // MFX Parameter 16
				case 0x0051: // MFX Parameter 17
				case 0x0055: // MFX Parameter 18
				case 0x0059: // MFX Parameter 19
				case 0x005D: // MFX Parameter 20
				case 0x0061: // MFX Parameter 21
				case 0x0065: // MFX Parameter 22
				case 0x0069: // MFX Parameter 23
				case 0x006D: // MFX Parameter 24
				case 0x0071: // MFX Parameter 25
				case 0x0075: // MFX Parameter 26
				case 0x0079: // MFX Parameter 27
				case 0x007D: // MFX Parameter 28
				case 0x0101: // MFX Parameter 29
				case 0x0105: // MFX Parameter 30
				case 0x0109: // MFX Parameter 31
				case 0x010D: // MFX Parameter 32
					{
					int num = (val[7] & 0x01) ? ((val[8] - 0x1) >> 2 + 28) : ((val[8] - 0x11) >> 2);
					int byteh = ((val[9] & 0x0F) << 4) | (val[10] & 0x0F);
					int bytel = ((val[11] & 0x0F) << 4) | (val[12] & 0x0F);
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_SD_HSB, 0, byteh, 0x0A);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_SD_MSB, 0, 0, part);
					SETMIDIEVENT(evm[2], 0, ME_SYSEX_SD_LSB, 0, bytel, 0x11 + num);
					num_events += 3;
					}
					break;
				}
				break;
			case 0x0010: // Patch TMT (Tone Mix Table
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. Patch TMT (ADDR:%02X %02X %02X %02X)",val[5],val[6],val[7],val[8]);
				break;
			case 0x0020: // Patch Tone (tone 1
			case 0x0022: // Patch Tone (tone 2
			case 0x0024: // Patch Tone (tone 3
			case 0x0026: // Patch Tone (tone 4
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. Patch Tone (ADDR:%02X %02X %02X %02X)",val[5],val[6],val[7],val[8]);
				break;
			// Rhythm
			case 0x1000: // Rhythm Common
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. Rhythm Common (ADDR:%02X %02X %02X %02X)",val[5],val[6],val[7],val[8]);
				break;
			case 0x1010: // Rhythm Tone (key 21
			case 0x1012: // Rhythm Tone (key 22
			case 0x1014: // Rhythm Tone (key 23
			case 0x1016: // Rhythm Tone (key 24
			case 0x1018: // Rhythm Tone (key 25
			case 0x101A: // Rhythm Tone (key 26
			case 0x101C: // Rhythm Tone (key 27
			case 0x101E: // Rhythm Tone (key 28
			case 0x1020: // Rhythm Tone (key 29
			case 0x1022: // Rhythm Tone (key 30
			case 0x1024: // Rhythm Tone (key 31
			case 0x1026: // Rhythm Tone (key 32
			case 0x1028: // Rhythm Tone (key 33
			case 0x102A: // Rhythm Tone (key 34
			case 0x102C: // Rhythm Tone (key 35
			case 0x102E: // Rhythm Tone (key 36
			case 0x1030: // Rhythm Tone (key 37
			case 0x1032: // Rhythm Tone (key 38
			case 0x1034: // Rhythm Tone (key 39
			case 0x1036: // Rhythm Tone (key 40
			case 0x1038: // Rhythm Tone (key 41
			case 0x103A: // Rhythm Tone (key 42
			case 0x103C: // Rhythm Tone (key 43
			case 0x103E: // Rhythm Tone (key 44
			case 0x1040: // Rhythm Tone (key 45
			case 0x1042: // Rhythm Tone (key 46
			case 0x1044: // Rhythm Tone (key 47
			case 0x1046: // Rhythm Tone (key 48
			case 0x1048: // Rhythm Tone (key 49
			case 0x104A: // Rhythm Tone (key 50
			case 0x104C: // Rhythm Tone (key 51
			case 0x104E: // Rhythm Tone (key 52
			case 0x1050: // Rhythm Tone (key 53
			case 0x1052: // Rhythm Tone (key 54
			case 0x1054: // Rhythm Tone (key 55
			case 0x1056: // Rhythm Tone (key 56
			case 0x1058: // Rhythm Tone (key 57
			case 0x105A: // Rhythm Tone (key 58
			case 0x105C: // Rhythm Tone (key 59
			case 0x105E: // Rhythm Tone (key 60
			case 0x1060: // Rhythm Tone (key 61
			case 0x1062: // Rhythm Tone (key 62
			case 0x1064: // Rhythm Tone (key 63
			case 0x1066: // Rhythm Tone (key 64
			case 0x1068: // Rhythm Tone (key 65
			case 0x106A: // Rhythm Tone (key 66
			case 0x106C: // Rhythm Tone (key 67
			case 0x106E: // Rhythm Tone (key 68
			case 0x1070: // Rhythm Tone (key 69
			case 0x1072: // Rhythm Tone (key 70
			case 0x1074: // Rhythm Tone (key 71
			case 0x1076: // Rhythm Tone (key 72
			case 0x1078: // Rhythm Tone (key 73
			case 0x107A: // Rhythm Tone (key 74
			case 0x107C: // Rhythm Tone (key 75
			case 0x107E: // Rhythm Tone (key 76
			case 0x1100: // Rhythm Tone (key 77
			case 0x1102: // Rhythm Tone (key 78
			case 0x1104: // Rhythm Tone (key 79
			case 0x1106: // Rhythm Tone (key 80
			case 0x1108: // Rhythm Tone (key 81
			case 0x110A: // Rhythm Tone (key 82
			case 0x110C: // Rhythm Tone (key 83
			case 0x110E: // Rhythm Tone (key 84
			case 0x1110: // Rhythm Tone (key 85
			case 0x1112: // Rhythm Tone (key 86
			case 0x1114: // Rhythm Tone (key 87
			case 0x1116: // Rhythm Tone (key 88
			case 0x1118: // Rhythm Tone (key 89
			case 0x111A: // Rhythm Tone (key 90
			case 0x111C: // Rhythm Tone (key 91
			case 0x111E: // Rhythm Tone (key 92
			case 0x1120: // Rhythm Tone (key 93
			case 0x1122: // Rhythm Tone (key 94
			case 0x1124: // Rhythm Tone (key 95
			case 0x1126: // Rhythm Tone (key 96
			case 0x1128: // Rhythm Tone (key 97
			case 0x112A: // Rhythm Tone (key 98
			case 0x112C: // Rhythm Tone (key 99
			case 0x112E: // Rhythm Tone (key 100
			case 0x1130: // Rhythm Tone (key 101
			case 0x1132: // Rhythm Tone (key 102
			case 0x1134: // Rhythm Tone (key 103
			case 0x1136: // Rhythm Tone (key 104
			case 0x1138: // Rhythm Tone (key 105
			case 0x113A: // Rhythm Tone (key 106
			case 0x113C: // Rhythm Tone (key 107
			case 0x113E: // Rhythm Tone (key 108
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. Rhythm Tone (ADDR:%02X %02X %02X %02X)",val[5],val[6],val[7],val[8]);
				break;
			default:
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. (ADDR:%02X %02X %02X %02X)",val[5],val[6],val[7],val[8]);
				break;
			}
			break;
		default:
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD SysEx. (ADDR:%02X %02X %02X %02X)",val[5],val[6],val[7],val[8]);
			break;
		}		
	}

	
    /* parsing SD-50 System Exclusive Message...
     * val[6] == Parameter Address(1
     * val[7] == Parameter Address(2
     * val[8] == Parameter Address(3
     * val[9] == Parameter Address(4
     * val[9]... == Data...
     * val[last] == Checksum(== 128 - (sum of addresses&data bytes % 128)) 
     */
    else if(len >= 10 &&
       val[0] == 0x41 && /* Roland ID */
       val[1] == 0x10 && /* Device ID */
       val[2] == 0x00 && // Model ID (SD-50
	   val[3] == 0x00 && // Model ID (SD-50
       val[4] == 0x4A && // Model ID (SD-50
       val[5] == 0x12) /* Data Set Command */
    {
		uint8 sdlen;
		int i, checksum;
		int mfx, part, tmp1, tmp2;
		int16 dbyte, dbyte2, dbyte3;
#ifdef _DEBUG
		uint8 sysex[32];

		memset(sysex, 0, sizeof(sysex));
		for(i = 0; i < len; i++){
			sysex[i] = val[i];
		}
#endif

		/* calculate checksum */
#if 1 // 怪しい・・?
		checksum = 0;
		for(sdlen = 11; sdlen < len; sdlen++)
			if(val[sdlen] == 0xF7)
				break;
		for(i = 6; i < sdlen - 1; i++) {
			checksum += val[i];
		}
		if(((128 - (checksum & 0x7F)) & 0x7F) != val[sdlen - 1]) {
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"SD-50 SysEx: Checksum Error.");
			return num_events;
		}
#endif
		
		switch(val[6]){
		case 0x01: // Setup
			if(val[7]){
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"SD-50 SysEx. Addres Error. (ADDR:%02X %02X %02X %02X)",val[6],val[7],val[8],val[9]);
				break;	
			}
			switch((((uint16)val[8]) << 8) | val[9]){
			case 0x0012: // GM map
				// val[10] == 0
				SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, 0xFF, val[11] ? 1 : 0, 0x6C); // part=0xFF : all
				num_events++;
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "D-50 SysEx. GM map (CH:all VAL:%d)", val[11]);
				break;
			default:
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD-50 SysEx. (ADDR:%02X %02X %02X %02X)",val[6],val[7],val[8],val[9]);
				break;
			}
		default:
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported SD-50 SysEx. (ADDR:%02X %02X %02X %02X)",val[6],val[7],val[8],val[9]);
			break;
		}
	}

	/* Non-RealTime / RealTime Universal SysEx messages
	 * 0 0x7e(Non-RealTime) / 0x7f(RealTime)
	 * 1 SysEx device ID.  Could be from 0x00 to 0x7f.
	 *   0x7f means disregard device.
	 * 2 Sub ID
	 * ...
	 * E 0xf7
	 */
	else if (len > 4 && val[0] >= 0x7e){
		int slot;
		switch (val[2]) {
		case 0x01:	/* Sample Dump header */
		case 0x02:	/* Sample Dump packet */
		case 0x03:	/* Dump Request */
		case 0x04:	/* Device Control */
			switch(val[3]){
			case 0x05: /* Global Parameter Control */
				slot = (int)val[7] << 8 | val[8];
				switch(slot){
				case 0x0101: /* Reverb */
					for (i = 9; i < len && val[i] != 0xf7; i+= 2) {
						switch(val[i]) {
						case 0x00:	/* Reverb Type */
							SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, 0, val[i + 1], 0x60);
							num_events++;
							break;
						case 0x01:	/* Reverb Time */
							SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_GS_LSB, 0, val[i + 1], 0x09);
							num_events++;
							break;
						}
					}
					break;
				case 0x0102: /* Chorus */
					for (i = 9; i < len && val[i] != 0xf7; i+= 2) {
						switch(val[i]) {
						case 0x00:	/* Chorus Type */
							SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, 0, val[i + 1], 0x61);
							num_events++;
							break;
						case 0x01:	/* Modulation Rate */
							SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_GS_LSB, 0, val[i + 1], 0x12);
							num_events++;
							break;
						case 0x02:	/* Modulation Depth */
							SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_GS_LSB, 0, val[i + 1], 0x13);
							num_events++;
							break;
						case 0x03:	/* Feedback */
							SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_GS_LSB, 0, val[i + 1], 0x10);
							num_events++;
							break;
						case 0x04:	/* Send To Reverb */
							SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_GS_LSB, 0, val[i + 1], 0x14);
							num_events++;
							break;
						}
					}
					break;
				default:
					break;
				}
				break;
			default:
				break;
			}
			break;
		case 0x05:	/* Sample Dump extensions */
		case 0x06:	/* Inquiry Message */
		case 0x07:	/* File Dump */
			break;
		case 0x08:	/* MIDI Tuning Standard */
			switch (val[3]) {
			case 0x01:
				SETMIDIEVENT(evm[0], 0, ME_BULK_TUNING_DUMP, 0, val[4], 0);
				for (i = 0; i < 128; i++) {
					SETMIDIEVENT(evm[i * 2 + 1], 0, ME_BULK_TUNING_DUMP,
							1, i, val[i * 3 + 21]);
					SETMIDIEVENT(evm[i * 2 + 2], 0, ME_BULK_TUNING_DUMP,
							2, val[i * 3 + 22], val[i * 3 + 23]);
				}
				num_events += 257;
				break;
			case 0x02:
				SETMIDIEVENT(evm[0], 0, ME_SINGLE_NOTE_TUNING,
						0, val[4], 0);
				for (i = 0; i < val[5]; i++) {
					SETMIDIEVENT(evm[i * 2 + 1], 0, ME_SINGLE_NOTE_TUNING,
							1, val[i * 4 + 6], val[i * 4 + 7]);
					SETMIDIEVENT(evm[i * 2 + 2], 0, ME_SINGLE_NOTE_TUNING,
							2, val[i * 4 + 8], val[i * 4 + 9]);
				}
				num_events += val[5] * 2 + 1;
				break;
			case 0x0b:
				channel_tt = ((val[4] & 0x03) << 14 | val[5] << 7 | val[6])
						<< ((val[4] >> 2) * 16);
				if (val[1] == 0x7f) {
					SETMIDIEVENT(evm[0], 0, ME_MASTER_TEMPER_TYPE,
							0, val[7], (val[0] == 0x7f));
					num_events++;
				} else {
					for (i = j = 0; i < 32; i++)
						if (channel_tt & 1 << i) {
							SETMIDIEVENT(evm[j], 0, ME_TEMPER_TYPE,
									MERGE_CHANNEL_PORT(i),
									val[7], (val[0] == 0x7f));
							j++;
						}
					num_events += j;
				}
				break;
			case 0x0c:
				SETMIDIEVENT(evm[0], 0, ME_USER_TEMPER_ENTRY,
						0, val[4], val[21]);
				for (i = 0; i < val[21]; i++) {
					SETMIDIEVENT(evm[i * 5 + 1], 0, ME_USER_TEMPER_ENTRY,
							1, val[i * 10 + 22], val[i * 10 + 23]);
					SETMIDIEVENT(evm[i * 5 + 2], 0, ME_USER_TEMPER_ENTRY,
							2, val[i * 10 + 24], val[i * 10 + 25]);
					SETMIDIEVENT(evm[i * 5 + 3], 0, ME_USER_TEMPER_ENTRY,
							3, val[i * 10 + 26], val[i * 10 + 27]);
					SETMIDIEVENT(evm[i * 5 + 4], 0, ME_USER_TEMPER_ENTRY,
							4, val[i * 10 + 28], val[i * 10 + 29]);
					SETMIDIEVENT(evm[i * 5 + 5], 0, ME_USER_TEMPER_ENTRY,
							5, val[i * 10 + 30], val[i * 10 + 31]);
				}
				num_events += val[21] * 5 + 1;
				break;
			}
			break;
		case 0x09:	/* General MIDI Message , Controller Destination Setting */
			switch(val[3]) {
			case 0x01:	/* Channel Pressure */
				for (i = 5; i < len && val[i] != 0xf7; i+= 2) {
					switch(val[i]) {
					case 0x00:	/* Pitch Control */
						SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, val[4], val[i + 1], 0x00);
						num_events++;
						break;
					case 0x01:	/* Filter Cutoff Control */
						SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, val[4], val[i + 1], 0x01);
						num_events++;
						break;
					case 0x02:	/* Amplitude Control */
						SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, val[4], val[i + 1], 0x02);
						num_events++;
						break;
					case 0x03:	/* LFO Pitch Depth */
						SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, val[4], val[i + 1], 0x04);
						num_events++;
						break;
					case 0x04:	/* LFO Filter Depth */
						SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, val[4], val[i + 1], 0x05);
						num_events++;
						break;
					case 0x05:	/* LFO Amplitude Depth */
						SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, val[4], val[i + 1], 0x06);
						num_events++;
						break;
					}
				}
				break;
			case 0x03:	/* Control Change */
				if(val[5] <= 0 || (0x20 <= val[5] && val[5] <= 0x3F) || 0x60 <= val[5]){
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, val[4], -1, 0x6D); // CC1 number NULL
					num_events++;
				}else{
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, val[4], val[5], 0x6D); // CC1 number
					num_events++;
					for (i = 6; i < len && val[i] != 0xf7; i+= 2) {
						switch(val[i]) {
						case 0x00:	/* Pitch Control */
							SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, val[4], val[i + 1], 0x2C);
							num_events++;
							break;
						case 0x01:	/* Filter Cutoff Control */
							SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, val[4], val[i + 1], 0x2D);
							num_events++;
							break;
						case 0x02:	/* Amplitude Control */
							SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, val[4], val[i + 1], 0x2E);
							num_events++;
							break;
						case 0x03:	/* LFO Pitch Depth */
							SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, val[4], val[i + 1], 0x30);
							num_events++;
							break;
						case 0x04:	/* LFO Filter Depth */
							SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, val[4], val[i + 1], 0x31);
							num_events++;
							break;
						case 0x05:	/* LFO Amplitude Depth */
							SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, val[4], val[i + 1], 0x32);
							num_events++;
							break;
						}
					}
				}
				break;
			}
			break;
		case 0x0A:	/* Key-Based Instrument Control */
			switch(val[3]) {
			case 0x01:	/* Controller */
				for (i = 6; i < len && val[i] != 0xf7; i+= 2) {
					switch(val[i]) {
					case 0x07:	/* Level */
						SETMIDIEVENT(evm[num_events    ], 0, ME_NRPN_MSB      , val[4], 0x1A,       SYSEX_TAG);
						SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB      , val[4], val[5],     SYSEX_TAG);
						SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, val[4], val[i + 1], SYSEX_TAG);
						num_events += 3;
						break;
					case 0x0A:	/* Pan */
						SETMIDIEVENT(evm[num_events    ], 0, ME_NRPN_MSB      , val[4], 0x1C,       SYSEX_TAG);
						SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB      , val[4], val[5],     SYSEX_TAG);
						SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, val[4], val[i + 1], SYSEX_TAG);
						num_events += 3;
						break;
					case 0x5B:	/* Reverb Send */
						SETMIDIEVENT(evm[num_events    ], 0, ME_NRPN_MSB      , val[4], 0x1D,       SYSEX_TAG);
						SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB      , val[4], val[5],     SYSEX_TAG);
						SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, val[4], val[i + 1], SYSEX_TAG);
						num_events += 3;
						break;
					case 0x5D:	/* Chorus Send */
						SETMIDIEVENT(evm[num_events    ], 0, ME_NRPN_MSB      , val[4], 0x1E,       SYSEX_TAG);
						SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB      , val[4], val[5],     SYSEX_TAG);
						SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, val[4], val[i + 1], SYSEX_TAG);
						num_events += 3;
						break;
					}
				}
				break;
			}
			break;
		case 0x7b:	/* End of File */
		case 0x7c:	/* Handshaking Message: Wait */
		case 0x7d:	/* Handshaking Message: Cancel */
		case 0x7e:	/* Handshaking Message: NAK */
		case 0x7f:	/* Handshaking Message: ACK */
			break;
		}
	}

    return(num_events);
}

int parse_sysex_event(uint8 *val, int32 len, MidiEvent *ev)
{
	uint16 vol;
/*
    if(current_file_info->mid == 0 || current_file_info->mid >= 0x7e)
		current_file_info->mid = val[0];
*/
///r
// for SD native
    if(len >= 10 &&
       val[0] == 0x41 && // Roland ID
       val[1] == 0x10 && // Device ID
       val[2] == 0x00 && // Model ID
       (val[3] >= 0x20 || val[3] <= 0xFF) && // Model ID
       val[4] == 0x12) // Data Set 
    {
		if(current_file_info->mid == 0 || current_file_info->mid >= 0x7e)
			current_file_info->mid = 0x60;
		if( val[5] == 0x00 && // ad setup
			val[6] == 0x00 && // ad
			val[7] == 0x00 && // ad
			val[8] == 0x00 && // ad
			val[9] == 0x00 && // data
			val[10] == 0x00){ // data
			current_file_info->mid = 0x60;
			ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SysEx: SD Native Mode");
			SETMIDIEVENT(*ev, 0, ME_RESET, 0, SD_SYSTEM_MODE, SYSEX_TAG);
			return 1;
		}
	}

// for SD-50
    if(len >= 10 &&
       val[0] == 0x41 && // Roland ID
       val[1] == 0x10 && // Device ID
       val[2] == 0x00 && // Model ID (SD-50
	   val[3] == 0x00 && // Model ID (SD-50
       val[4] == 0x4A && // Model ID (SD-50
       val[5] == 0x12) // Data Set 
    {
		if(current_file_info->mid == 0 || current_file_info->mid >= 0x7e)
			current_file_info->mid = 0x60;
		if( val[6] == 0x01 && // ad setup
			val[7] == 0x00 && // ad
			val[8] == 0x00 && // ad sound mode
			val[9] == 0x00){ // ad
			switch( (((uint32)val[10]) << 7) | (uint32)val[11] ){ // data
			case 1:
				current_file_info->mid = 0x60;
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SysEx: SD-50 Studio Mode");
				SETMIDIEVENT(*ev, 0, ME_RESET, 0, SD_SYSTEM_MODE, SYSEX_TAG);
				break;		
			case 2:
				current_file_info->mid = 0x7e;
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SysEx: GM System On");
				SETMIDIEVENT(*ev, 0, ME_RESET, 0, GM_SYSTEM_MODE, SYSEX_TAG);
				break;
			case 3:
				current_file_info->mid = 0x7d;
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SysEx: GM2 System On");
				SETMIDIEVENT(*ev, 0, ME_RESET, 0, GM2_SYSTEM_MODE, SYSEX_TAG);
				break;		
			case 4:
				current_file_info->mid = 0x41;
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SysEx: GS System On");
				SETMIDIEVENT(*ev, 0, ME_RESET, 0, GS_SYSTEM_MODE, SYSEX_TAG);
				break;	
			}
			return 1;
		}
	}

// for KG
    if(len >= 4 &&
       val[0] == 0x42 && // KORG ID
       (val[1] >= 0x30 && val[1] <= 0x3F) && // Exclusive Channel 0:Global 1~F:Excl Ch
       (val[2] >= 0x00 && val[2] <= 0xFF)) // Machine ID
    {
		if(current_file_info->mid == 0 || current_file_info->mid >= 0x7e)
			current_file_info->mid = 42;
		if( val[3] == 0x00 &&
			val[4] == 0x00 || val[4] == 0x01){
			ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SysEx: KORG Mode");
			SETMIDIEVENT(*ev, 0, ME_RESET, 0, KG_SYSTEM_MODE, SYSEX_TAG);
			return 1;
		}
	}

// for GS
    if(len >= 10 &&
       val[0] == 0x41 && /* Roland ID */
       val[1] == 0x10 && /* Device ID */
       val[2] == 0x42 && /* GS Model ID */
       val[3] == 0x12)   /* Data Set Command */
		{
		/* Roland GS-Based Synthesizers.
		 * val[4..6] is address, val[7..len-2] is body.
		 *
		 * GS     Channel part number
		 * 0      10
		 * 1-9    1-9
		 * 10-15  11-16
		 */

		int32 addr,checksum,i;		/* SysEx address */
		uint8 *body;		/* SysEx body */
		uint8 p,gslen;		/* Channel part number [0..15] */
///r
		if(current_file_info->mid == 0 || current_file_info->mid >= 0x7e)
			current_file_info->mid = 0x41;

		/* check Checksum */
		checksum = 0;
		for(gslen = 9; gslen < len; gslen++)
			if(val[gslen] == 0xF7)
				break;
		for(i=4;i<gslen-1;i++) {
			checksum += val[i];
		}
		if(((128 - (checksum & 0x7F)) & 0x7F) != val[gslen-1]) {
			return 0;
		}

		addr = (((int32)val[4])<<16 |
			((int32)val[5])<<8 |
			(int32)val[6]);
		body = val + 7;
		p = (uint8)((addr >> 8) & 0xF);
		if(p == 0)
			p = 9;
		else if(p <= 9)
			p--;
		p = MERGE_CHANNEL_PORT(p);

		if(val[4] == 0x50) {	/* for double module mode */
			p += 16;
			addr = (((int32)0x40)<<16 |
				((int32)val[5])<<8 |
				(int32)val[6]);
		} else {	/* single module mode */
			addr = (((int32)val[4])<<16 |
				((int32)val[5])<<8 |
				(int32)val[6]);
		}

		if((addr & 0xFFF0FF) == 0x401015) /* Rhythm Parts */
		{
#ifdef GS_DRUMPART
/* GS drum part check from Masaaki Koyanagi's patch (GS_Drum_Part_Check()) */
/* Modified by Masanao Izumo */
			SETMIDIEVENT(*ev, 0, ME_DRUMPART, p, *body, SYSEX_TAG);
			return 1;
#else
			return 0;
#endif /* GS_DRUMPART */
		}

		if((addr & 0xFFF0FF) == 0x401016) /* Key Shift */
		{
			SETMIDIEVENT(*ev, 0, ME_KEYSHIFT, p, *body, SYSEX_TAG);
			return 1;
		}

		if(addr == 0x400000) /* Master Tune, not for SMF */
		{
			uint16 tune = ((body[1] & 0xF) << 8) | ((body[2] & 0xF) << 4) | (body[3] & 0xF);
		
			if (tune < 0x18)
				tune = 0x18;
			else if (tune > 0x7E8)
				tune = 0x7E8;
			SETMIDIEVENT(*ev, 0, ME_MASTER_TUNING, 0, tune & 0xFF, (tune >> 8) & 0x7F);
			return 1;
		}

		if(addr == 0x400004) /* Master Volume */
		{
			vol = gs_convert_master_vol(*body);
			SETMIDIEVENT(*ev, 0, ME_MASTER_VOLUME,
				 0, vol & 0xFF, (vol >> 8) & 0xFF);
			return 1;
		}

		if((addr & 0xFFF0FF) == 0x401019) /* Volume on/off */
		{
#if 0
			SETMIDIEVENT(*ev, 0, ME_VOLUME_ONOFF, p, *body >= 64, SYSEX_TAG);
#endif
			return 0;
		}

		if((addr & 0xFFF0FF) == 0x401002) /* Receive channel on/off */
		{
#if 0
			SETMIDIEVENT(*ev, 0, ME_RECEIVE_CHANNEL, (uint8)p, *body >= 64, SYSEX_TAG);
#endif
			return 0;
		}

		if(0x402000 <= addr && addr <= 0x402F5A) /* Controller Routing */
			return 0;

		if((addr & 0xFFF0FF) == 0x401040) /* Alternate Scale Tunings */
			return 0;

		if((addr & 0xFFFFF0) == 0x400130) /* Changing Effects */
		{
			struct chorus_text_gs_t *chorus_text = &(chorus_status_gs.text);
			switch(addr & 0xF)
			{
			  case 0x8: /* macro */
			memcpy(chorus_text->macro, body, 3);
			break;
			  case 0x9: /* PRE-LPF */
			memcpy(chorus_text->pre_lpf, body, 3);
			break;
			  case 0xa: /* level */
			memcpy(chorus_text->level, body, 3);
			break;
			  case 0xb: /* feed back */
			memcpy(chorus_text->feed_back, body, 3);
			break;
			  case 0xc: /* delay */
			memcpy(chorus_text->delay, body, 3);
			break;
			  case 0xd: /* rate */
			memcpy(chorus_text->rate, body, 3);
			break;
			  case 0xe: /* depth */
			memcpy(chorus_text->depth, body, 3);
			break;
			  case 0xf: /* send level */
			memcpy(chorus_text->send_level, body, 3);
			break;
			  default: break;
			}

			check_chorus_text_start();
			return 0;
		}

		if((addr & 0xFFF0FF) == 0x401003) /* Rx Pitch-Bend */
			return 0;

		if(addr == 0x400110) /* Voice Reserve */
		{
			if(len >= 25)
			memcpy(chorus_status_gs.text.voice_reserve, body, 18);
			check_chorus_text_start();
			return 0;
		}
///r
		if(addr == 0x40007F)	/* GS SYSTEM ON exCM */ 
		{
			if(is_cm_module() || opt_system_mid == 0x30 )
				return 0;
			ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SysEx: GS System On");
			SETMIDIEVENT(*ev, 0, ME_RESET, 0, GS_SYSTEM_MODE, SYSEX_TAG);
			return 1;
		}
		else if(addr == 0x00007F)	/* System Mode set */
		{
			if(is_cm_module() || opt_system_mid == 0x30 )
				return 0;
			ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SysEx: System Mode set (GS)");
			SETMIDIEVENT(*ev, 0, ME_RESET, 0, GS_SYSTEM_MODE, SYSEX_TAG);
			return 1;
		}
		return 0;
    }
     if(len >= 9 &&
       val[0] == 0x41 && /* Roland ID */
       val[1] == 0x10 && /* Device ID */
       val[2] == 0x45 && 
       val[3] == 0x12 && 
       val[4] == 0x10 && 
       val[5] == 0x00 && 
       val[6] == 0x00)
    {
		/* Text Insert for SC */
		uint8 save;

		len -= 2;
		save = val[len];
		val[len] = '\0';
		if(readmidi_make_string_event(ME_INSERT_TEXT, (char *)val + 7, ev, 1))
		{
			val[len] = save;
			return 1;
		}
		val[len] = save;
		return 0;
    }

    if(len > 9 &&                     /* GS lcd event. by T.Nogami*/
       val[0] == 0x41 && /* Roland ID */
       val[1] == 0x10 && /* Device ID */
       val[2] == 0x45 && 
       val[3] == 0x12 && 
       val[4] == 0x10 && 
       val[5] == 0x01 && 
       val[6] == 0x00)
    {
		/* Text Insert for SC */
		uint8 save;

		len -= 2;
		save = val[len];
		val[len] = '\0';
		if(readmidi_make_lcd_event(ME_GSLCD, (uint8 *)val + 7, ev))
		{
			val[len] = save;
			return 1;
		}
		val[len] = save;
		return 0;
    }

     /* val[1] can have values other than 0x10 for the XG ON event, which
     * work on real XG hardware.  I have several midi that use 0x1f instead
     * of 0x10.  playmidi.h lists 0x10 - 0x13 as MU50/80/90/100.  I don't
     * know what real world Device Number 0x1f would correspond to, but the
     * XG spec says the entire 0x1n range is valid, and 0x1f works on real
     * hardware, so I have modified the check below to accept the entire
     * 0x1n range.
     *
     * I think there are/were some hacks somewhere in playmidi.c (?) to work
     * around non- 0x10 values, but this fixes the root of the problem, which
     * allows the server mode to handle XG initialization properly as well.
     */
    if(len >= 8 &&
       val[0] == 0x43 &&
       (val[1] >= 0x10 && val[1] <= 0x1f) &&
       val[2] == 0x4C)
    {
		int addr = (val[3] << 16) | (val[4] << 8) | val[5];
///r
		if(current_file_info->mid == 0 || current_file_info->mid >= 0x7e)
			current_file_info->mid = 0x43;

		if (addr == 0x00007E)	/* XG SYSTEM ON */
		{
			current_file_info->mid = 0x43;
			ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SysEx: XG System On");
			SETMIDIEVENT(*ev, 0, ME_RESET, 0, XG_SYSTEM_MODE, SYSEX_TAG);
			return 1;
		}
		else if (addr == 0x000000 && len >= 12)	/* XG Master Tune */
		{
			uint16 tune = ((val[7] & 0xF) << 8) | ((val[8] & 0xF) << 4) | (val[9] & 0xF);
		
			if (tune > 0x7FF)
				tune = 0x7FF;
			SETMIDIEVENT(*ev, 0, ME_MASTER_TUNING, 0, tune & 0xFF, (tune >> 8) & 0x7F);
			return 1;
		}
		}

		if (len >= 7 && val[0] == 0x7F && val[1] == 0x7F)
		{
		if (val[2] == 0x04 && val[3] == 0x03)	/* GM2 Master Fine Tune */
		{
			uint16 tune = (val[4] & 0x7F) | (val[5] << 7) | 0x4000;
		
			SETMIDIEVENT(*ev, 0, ME_MASTER_TUNING, 0, tune & 0xFF, (tune >> 8) & 0x7F);
			return 1;
		}
		if (val[2] == 0x04 && val[3] == 0x04)	/* GM2 Master Coarse Tune */
		{
			uint8 tune = val[5];
		
			if (tune < 0x28)
				tune = 0x28;
			else if (tune > 0x58)
				tune = 0x58;
			SETMIDIEVENT(*ev, 0, ME_MASTER_TUNING, 0, tune, 0x80);
			return 1;
		}
    }
    
	/* Non-RealTime / RealTime Universal SysEx messages
	 * 0 0x7e(Non-RealTime) / 0x7f(RealTime)
	 * 1 SysEx device ID.  Could be from 0x00 to 0x7f.
	 *   0x7f means disregard device.
	 * 2 Sub ID
	 * ...
	 * E 0xf7
	 */
	if (len > 4 && val[0] >= 0x7e)
		switch (val[2]) {
		case 0x01:	/* Sample Dump header */
		case 0x02:	/* Sample Dump packet */
		case 0x03:	/* Dump Request */
			break;
		case 0x04:	/* MIDI Time Code Setup/Device Control */
			switch (val[3]) {
			case 0x01:	/* Master Volume */
				vol = gm_convert_master_vol(val[4], val[5]);
				if (val[1] == 0x7f) {
					SETMIDIEVENT(*ev, 0, ME_MASTER_VOLUME, 0,
							vol & 0xff, vol >> 8 & 0xff);
				} else {
					SETMIDIEVENT(*ev, 0, ME_MAINVOLUME,
							MERGE_CHANNEL_PORT(val[1]),
							vol >> 8 & 0xff, 0);
				}
				return 1;
			}
			break;
		case 0x05:	/* Sample Dump extensions */
		case 0x06:	/* Inquiry Message */
		case 0x07:	/* File Dump */
			break;
		case 0x08:	/* MIDI Tuning Standard */
			switch (val[3]) {
			case 0x0a:
				SETMIDIEVENT(*ev, 0, ME_TEMPER_KEYSIG, 0,
						val[4] - 0x40 + val[5] * 16, (val[0] == 0x7f));
				return 1;
			}
			break;
		case 0x09:	/* General MIDI Message */
			/* GM System Enable/Disable */
			switch(val[3]) {
			case 1:
				current_file_info->mid = 0x7e;
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SysEx: GM System On");
				SETMIDIEVENT(*ev, 0, ME_RESET, 0, GM_SYSTEM_MODE, SYSEX_TAG);
				break;
			case 2:
				current_file_info->mid = 0x60;
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SysEx: GM System Off");
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SysEx: SD Native Mode");
				SETMIDIEVENT(*ev, 0, ME_RESET, 0, SD_SYSTEM_MODE, SYSEX_TAG);
				break;				
			case 3:
				current_file_info->mid = 0x7d;
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SysEx: GM2 System On");
				SETMIDIEVENT(*ev, 0, ME_RESET, 0, GM2_SYSTEM_MODE, SYSEX_TAG);
				break;				
			default:
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SysEx: GM System Off");
				SETMIDIEVENT(*ev, 0, ME_RESET, 0, DEFAULT_SYSTEM_MODE, SYSEX_TAG);
				break;
			}
			return 1;
		case 0x7b:	/* End of File */
		case 0x7c:	/* Handshaking Message: Wait */
		case 0x7d:	/* Handshaking Message: Cancel */
		case 0x7e:	/* Handshaking Message: NAK */
		case 0x7f:	/* Handshaking Message: ACK */
			break;
		}
	return 0;
}

static int read_sysex_event(int32 at, int me, int32 len, struct timidity_file *tf)
{
    uint8 *val;
    MidiEvent ev, evm[260]; /* maximum number of XG bulk dump events */
    int ne, i;

    if(len == 0)
		return 0;
    if(me != 0xF0)
    {
		skip(tf, len);
		return 0;
    }

    val = (uint8 *)new_segment(&tmpbuffer, len);
    if(tf_read(val, 1, len, tf) != len)
    {
		reuse_mblock(&tmpbuffer);
		return -1;
    }
    if(parse_sysex_event(val, len, &ev))
    {
		ev.time = at;
		readmidi_add_event(&ev);
    }
    memset(evm, 0, sizeof(evm));//added by Kobarin
    if ((ne = parse_sysex_event_multi(val, len, evm)))
    {
		for (i = 0; i < ne; i++) {
			evm[i].time = at;
			readmidi_add_event(&evm[i]);
		}
    }
    reuse_mblock(&tmpbuffer);
    return 0;
}

static char *fix_string(char *s)
{
    int i, j, w;
    char c;

    if(s == NULL)
	return NULL;
    while(*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
	s++;

    /* s =~ tr/ \t\r\n/ /s; */
    w = 0;
    for(i = j = 0; (c = s[i]) != '\0'; i++)
    {
	if(c == '\t' || c == '\r' || c == '\n')
	    c = ' ';
	if(w)
	    w = (c == ' ');
	if(!w)
	{
	    s[j++] = c;
	    w = (c == ' ');
	}
    }

    /* s =~ s/ $//; */
    if(j > 0 && s[j - 1] == ' ')
	j--;

    s[j] = '\0';
    return s;
}

static void smf_time_signature(int32 at, struct timidity_file *tf, int len)
{
    int n, d, c, b;

    /* Time Signature (nn dd cc bb)
     * [0]: numerator
     * [1]: denominator
     * [2]: number of MIDI clocks in a metronome click
     * [3]: number of notated 32nd-notes in a MIDI
     *      quarter-note (24 MIDI Clocks).
     */

    if(len != 4)
    {
	ctl->cmsg(CMSG_WARNING, VERB_VERBOSE, "Invalid time signature");
	skip(tf, len);
	return;
    }

    n = tf_getc(tf);
    d = (1<<tf_getc(tf));
    c = tf_getc(tf);
    b = tf_getc(tf);

    if(n == 0 || d == 0)
    {
	ctl->cmsg(CMSG_WARNING, VERB_VERBOSE, "Invalid time signature");
	return;
    }

    MIDIEVENT(at, ME_TIMESIG, 0, n, d);
    MIDIEVENT(at, ME_TIMESIG, 1, c, b);
    ctl->cmsg(CMSG_INFO, VERB_NOISY,
	      "Time signature: %d/%d %d clock %d q.n.", n, d, c, b);
    if(current_file_info->time_sig_n == -1)
    {
	current_file_info->time_sig_n = n;
	current_file_info->time_sig_d = d;
	current_file_info->time_sig_c = c;
	current_file_info->time_sig_b = b;
    }
}

static void smf_key_signature(int32 at, struct timidity_file *tf, int len)
{
	int8 sf, mi;
	/* Key Signature (sf mi)
	 * sf = -7:  7 flats
	 * sf = -1:  1 flat
	 * sf = 0:   key of C
	 * sf = 1:   1 sharp
	 * sf = 7:   7 sharps
	 * mi = 0:  major key
	 * mi = 1:  minor key
	 */
	
	if (len != 2) {
		ctl->cmsg(CMSG_WARNING, VERB_VERBOSE, "Invalid key signature");
		skip(tf, len);
		return;
	}
	sf = tf_getc(tf);
	mi = tf_getc(tf);
	if (sf < -7 || sf > 7) {
		ctl->cmsg(CMSG_WARNING, VERB_VERBOSE, "Invalid key signature");
		return;
	}
	if (mi != 0 && mi != 1) {
		ctl->cmsg(CMSG_WARNING, VERB_VERBOSE, "Invalid key signature");
		return;
	}
	MIDIEVENT(at, ME_KEYSIG, 0, sf, mi);
	ctl->cmsg(CMSG_INFO, VERB_NOISY,
			"Key signature: %d %s %s", abs(sf),
			(sf < 0) ? "flat(s)" : "sharp(s)", (mi) ? "minor" : "major");
}

/* Used for WRD reader */
int dump_current_timesig(MidiEvent *codes, int maxlen)
{
    int i, n;
    MidiEventList *e;

    if(maxlen <= 0 || evlist == NULL)
	return 0;
    n = 0;
    for(i = 0, e = evlist; i < event_count; i++, e = e->next)
	if(e->event.type == ME_TIMESIG && e->event.channel == 0)
	{
	    if(n == 0 && e->event.time > 0)
	    {
		/* 4/4 is default */
		SETMIDIEVENT(codes[0], 0, ME_TIMESIG, 0, 4, 4);
		n++;
		if(maxlen == 1)
		    return 1;
	    }

	    if(n > 0)
	    {
		if(e->event.a == codes[n - 1].a &&
		   e->event.b == codes[n - 1].b)
		    continue; /* Unchanged */
		if(e->event.time == codes[n - 1].time)
		    n--; /* overwrite previous code */
	    }
	    codes[n++] = e->event;
	    if(n == maxlen)
		return n;
	}
    return n;
}

/* Read a SMF track */
static int read_smf_track(struct timidity_file *tf, int trackno, int rewindp)
{
    int32 len;
    size_t next_pos, pos;
    char tmp[4];
    int lastchan, laststatus;
    int me, type, a, b, c;
    int i;
    int32 smf_at_time;
    int note_seen = (! opt_preserve_silence);
    int hascc111;

    smf_at_time = readmidi_set_track(trackno, rewindp);

    /* Check the formalities */
    if((tf_read(tmp, 1, 4, tf) != 4) || (tf_read(&len, 4, 1, tf) != 1))
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "%s: Can't read track header.", current_filename);
	return -1;
    }
    len = BE_LONG(len);
    next_pos = tf_tell(tf) + len;
    if(strncmp(tmp, "MTrk", 4))
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "%s: Corrupt MIDI file.", current_filename);
	return -2;
    }

    lastchan = laststatus = 0;
    hascc111 = 0;

    for(;;)
    {
	if(readmidi_error_flag)
	    return -1;
	if((len = getvl(tf)) < 0)
	    return -1;
	smf_at_time += len;
	_set_errno(0);
	if((i = tf_getc(tf)) == EOF)
	{
	    if(errno)
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: read_midi_event: %s",
			  current_filename, strerror(errno));
	    else
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "Warning: %s: Too shorten midi file.",
			  current_filename);
	    return -1;
	}

	me = (uint8)i;
	if(me == 0xF0 || me == 0xF7) /* SysEx event */
	{
	    if((len = getvl(tf)) < 0)
		return -1;
	    if((i = read_sysex_event(smf_at_time, me, len, tf)) != 0)
		return i;
	}
	else if(me == 0xFF) /* Meta event */
	{
	    type = tf_getc(tf);
	    if((len = getvl(tf)) < 0)
		return -1;
	    if(type > 0 && type < 16)
	    {
		static char *label[] =
		{
		    "Text event: ", "Text: ", "Copyright: ", "Track name: ",
		    "Instrument: ", "Lyric: ", "Marker: ", "Cue point: "
		};

		if(type == 5 || /* Lyric */
		   (type == 1 && (opt_trace_text_meta_event ||
				  karaoke_format == 2 ||
				  chorus_status_gs.text.status == CHORUS_ST_OK)) ||
		   (type == 6 &&  (current_file_info->format == 0 ||
				   (current_file_info->format == 1 &&
				    current_read_track == 0))))
		{
		    char *str, *text;
		    MidiEvent ev;

		    str = (char *)new_segment(&tmpbuffer, len + 3);
		    if(type != 6)
		    {
			i = tf_read(str, 1, len, tf);
			str[len] = '\0';
		    }
		    else
		    {
			i = tf_read(str + 1, 1, len, tf);
			str[0] = MARKER_START_CHAR;
			str[len + 1] = MARKER_END_CHAR;
			str[len + 2] = '\0';
		    }

		    if(i != len)
		    {
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				  "Warning: %s: Too shorten midi file.",
				  current_filename);
			reuse_mblock(&tmpbuffer);
			return -1;
		    }

		    if((text = readmidi_make_string_event(1, str, &ev, 1))
		       == NULL)
		    {
			reuse_mblock(&tmpbuffer);
			continue;
		    }
		    ev.time = smf_at_time;

		    if(type == 6)
		    {
			if(strlen(fix_string(text + 1)) == 2)
			{
			    reuse_mblock(&tmpbuffer);
			    continue; /* Empty Marker */
			}
		    }

		    switch(type)
		    {
		      case 1:
			if(karaoke_format == 2)
			{
			    *text = ME_KARAOKE_LYRIC;
			    if(karaoke_title_flag == 0 &&
			       strncmp(str, "@T", 2) == 0)
				current_file_info->karaoke_title =
				    add_karaoke_title(current_file_info->
						      karaoke_title, str + 2);
			    ev.type = ME_KARAOKE_LYRIC;
			    readmidi_add_event(&ev);
			    continue;
			}
			if(chorus_status_gs.text.status == CHORUS_ST_OK)
			{
			    *text = ME_CHORUS_TEXT;
			    ev.type = ME_CHORUS_TEXT;
			    readmidi_add_event(&ev);
			    continue;
			}
			*text = ME_TEXT;
			ev.type = ME_TEXT;
			readmidi_add_event(&ev);
			continue;
		      case 5:
			*text = ME_LYRIC;
			ev.type = ME_LYRIC;
			readmidi_add_event(&ev);
			continue;
		      case 6:
			*text = ME_MARKER;
			ev.type = ME_MARKER;
			readmidi_add_event(&ev);
			continue;
		    }
		}

		if(type == 3 && /* Sequence or Track Name */
		   (current_file_info->format == 0 ||
		    (current_file_info->format == 1 &&
		     current_read_track == 0)))
		{
		  if(current_file_info->seq_name == NULL) {
		    char *name = dumpstring(3, len, "Sequence: ", 1, tf);
		    current_file_info->seq_name = safe_strdup(fix_string(name));
		    safe_free(name);
		  }
		    else
			dumpstring(3, len, "Sequence: ", 0, tf);
		}
		else if(type == 1 &&
			current_file_info->first_text == NULL &&
			(current_file_info->format == 0 ||
			 (current_file_info->format == 1 &&
			  current_read_track == 0))) {
		  char *name = dumpstring(1, len, "Text: ", 1, tf);
		  current_file_info->first_text = safe_strdup(fix_string(name));
		  safe_free(name);
		}
		else
		    dumpstring(type, len, label[(type>7) ? 0 : type], 0, tf);
	    }
	    else
	    {
		switch(type)
		{
		  case 0x00:
		    if(len == 2)
		    {
			a = tf_getc(tf);
			b = tf_getc(tf);
			ctl->cmsg(CMSG_INFO, VERB_DEBUG,
				  "(Sequence Number %02x %02x)", a, b);
		    }
		    else
			ctl->cmsg(CMSG_INFO, VERB_DEBUG,
				  "(Sequence Number len=%d)", len);
		    break;

		  case 0x2F: /* End of Track */
            if (hascc111 != 0)
                MIDIEVENT(smf_at_time, ME_NONE, 0, 0, 0);
		    pos = tf_tell(tf);
		    if(pos < next_pos)
			tf_seek(tf, next_pos - pos, SEEK_CUR);
		    return 0;

		  case 0x51: /* Tempo */
		    a = tf_getc(tf);
		    b = tf_getc(tf);
		    c = tf_getc(tf);
		    MIDIEVENT(smf_at_time, ME_TEMPO, c, a, b);
		    break;

		  case 0x54:
		    /* SMPTE Offset (hr mn se fr ff)
		     * hr: hours&type
		     *     0     1     2     3    4    5    6    7   bits
		     *     0  |<--type -->|<---- hours [0..23]---->|
		     * type: 00: 24 frames/second
		     *       01: 25 frames/second
		     *       10: 30 frames/second (drop frame)
		     *       11: 30 frames/second (non-drop frame)
		     * mn: minis [0..59]
		     * se: seconds [0..59]
		     * fr: frames [0..29]
		     * ff: fractional frames [0..99]
		     */
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(SMPTE Offset meta event)");
		    skip(tf, len);
		    break;

		  case 0x58: /* Time Signature */
		    smf_time_signature(smf_at_time, tf, len);
		    break;

		  case 0x59: /* Key Signature */
		    smf_key_signature(smf_at_time, tf, len);
		    break;

		  case 0x7f: /* Sequencer-Specific Meta-Event */
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(Sequencer-Specific meta event, length %ld)",
			      len);
		    skip(tf, len);
		    break;

		  case 0x20: /* MIDI channel prefix (SMF v1.0) */
		    if(len == 1)
		    {
			int midi_channel_prefix = tf_getc(tf);
			ctl->cmsg(CMSG_INFO, VERB_DEBUG,
				  "(MIDI channel prefix %d)",
				  midi_channel_prefix);
		    }
		    else
			skip(tf, len);
		    break;

		  case 0x21: /* MIDI port number */
		    if(len == 1)
		    {
			if((midi_port_number = tf_getc(tf))
			   == EOF)
			{
			    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				      "Warning: %s: Too shorten midi file.",
				      current_filename);
			    return -1;
			}
			midi_port_number &= 0xF;
			ctl->cmsg(CMSG_INFO, VERB_DEBUG,
				  "(MIDI port number %d)", midi_port_number);
		    }
		    else
			skip(tf, len);
		    break;

		  default:
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(Meta event type 0x%02x, length %ld)",
			      type, len);
		    skip(tf, len);
		    break;
		}
	    }
	}
	else /* MIDI event */
	{
	    a = me;
	    if(a & 0x80) /* status byte */
	    {
		lastchan = MERGE_CHANNEL_PORT(a & 0x0F);
		laststatus = (a >> 4) & 0x07;
		if(laststatus != 7)
		    a = tf_getc(tf) & 0x7F;
	    }
	    switch(laststatus)
	    {
	      case 0: /* Note off */
		b = tf_getc(tf) & 0x7F;
		MIDIEVENT(smf_at_time, ME_NOTEOFF, lastchan, a,b);
		break;

	      case 1: /* Note on */
		b = tf_getc(tf) & 0x7F;
		if(b)
		{
		     if (! note_seen && smf_at_time > 0)
		     {
			MIDIEVENT(0, ME_NOTEON, lastchan, a, 0);
			MIDIEVENT(0, ME_NOTEOFF, lastchan, a, 0);
			note_seen = 1;
		     }
		    MIDIEVENT(smf_at_time, ME_NOTEON, lastchan, a,b);
		}
		else /* b == 0 means Note Off */
		{
		    MIDIEVENT(smf_at_time, ME_NOTEOFF, lastchan, a, 0);
		}
		break;

	      case 2: /* Key Pressure */
		b = tf_getc(tf) & 0x7F;
		MIDIEVENT(smf_at_time, ME_KEYPRESSURE, lastchan, a, b);
		break;

	      case 3: /* Control change */
		b = tf_getc(tf);
		readmidi_add_ctl_event(smf_at_time, lastchan, a, b);
        if (a == 111) {
            if (hascc111 == 0)
                ctl->cmsg(CMSG_INFO, VERB_DEBUG,
                            "Detection loop start event CC#111");
            hascc111 = 1;
        }
		break;

	      case 4: /* Program change */
		MIDIEVENT(smf_at_time, ME_PROGRAM, lastchan, a, 0);
		break;

	      case 5: /* Channel pressure */
		MIDIEVENT(smf_at_time, ME_CHANNEL_PRESSURE, lastchan, a, 0);
		break;

	      case 6: /* Pitch wheel */
		b = tf_getc(tf) & 0x7F;
		MIDIEVENT(smf_at_time, ME_PITCHWHEEL, lastchan, a, b);
		break;

	      default: /* case 7: */
		/* Ignore this event */
		switch(lastchan & 0xF)
		{
		  case 2: /* Sys Com Song Position Pntr */
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(Sys Com Song Position Pntr)");
		    tf_getc(tf);
		    tf_getc(tf);
		    break;

		  case 3: /* Sys Com Song Select(Song #) */
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(Sys Com Song Select(Song #))");
		    tf_getc(tf);
		    break;

		  case 6: /* Sys Com tune request */
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(Sys Com tune request)");
		    break;
		  case 8: /* Sys real time timing clock */
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(Sys real time timing clock)");
		    break;
		  case 10: /* Sys real time start */
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(Sys real time start)");
		    break;
		  case 11: /* Sys real time continue */
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(Sys real time continue)");
		    break;
		  case 12: /* Sys real time stop */
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(Sys real time stop)");
		    break;
		  case 14: /* Sys real time active sensing */
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(Sys real time active sensing)");
		    break;
#if 0
		  case 15: /* Meta */
		  case 0: /* SysEx */
		  case 7: /* SysEx */
#endif
		  default: /* 1, 4, 5, 9, 13 */
		    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			      "*** Can't happen: status 0x%02X channel 0x%02X",
			      laststatus, lastchan & 0xF);
		    break;
		}
		}
	}
    }
    /*NOTREACHED*/
}

/* Free the linked event list from memory. */
static void free_midi_list(void)
{
    if(evlist != NULL)
    {
	reuse_mblock(&mempool);
	evlist = NULL;
    }
}

static void move_channels(int *chidx)
{
	int i, ch, maxch, newch;
	MidiEventList *e;
	
	for (i = 0; i < 256; i++)
		chidx[i] = -1;
	/* check channels */
	for (i = maxch = 0, e = evlist; i < event_count; i++, e = e->next)
		if (! GLOBAL_CHANNEL_EVENT_TYPE(e->event.type)) {
			if ((ch = e->event.channel) < REDUCE_CHANNELS)
				chidx[ch] = ch;
			if (maxch < ch)
				maxch = ch;
		}
	if (maxch >= REDUCE_CHANNELS)
		/* Move channel if enable */
		for (i = maxch = 0, e = evlist; i < event_count; i++, e = e->next)
			if (! GLOBAL_CHANNEL_EVENT_TYPE(e->event.type)) {
				if (chidx[ch = e->event.channel] != -1)
					ch = e->event.channel = chidx[ch];
				else {	/* -1 */
					if (ch >= MAX_CHANNELS) {
						newch = ch % REDUCE_CHANNELS;
						while (newch < ch && newch < MAX_CHANNELS) {
							if (chidx[newch] == -1) {
								ctl->cmsg(CMSG_INFO, VERB_VERBOSE,
										"channel %d => %d", ch, newch);
								ch = e->event.channel = chidx[ch] = newch;
								break;
							}
							newch += REDUCE_CHANNELS;
						}
					}
					if (chidx[ch] == -1) {
						if (ch < MAX_CHANNELS)
							chidx[ch] = ch;
						else {
							newch = (ch & (MAX_CHANNELS - 1));
							ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
									"channel %d => %d (mixed)", ch, newch);
							ch = e->event.channel = chidx[ch] = newch;
						}
					}
				}
				if (maxch < ch)
					maxch = ch;
			}
	for (i = 0, e = evlist; i < event_count; i++, e = e->next)
		if (e->event.type == ME_SYSEX_GS_LSB) {
			if (e->event.b == 0x45 || e->event.b == 0x46)
				if (maxch < e->event.channel)
					maxch = e->event.channel;
		} else if (e->event.type == ME_SYSEX_XG_LSB) {
			if (e->event.b == 0x99)
				if (maxch < e->event.channel)
					maxch = e->event.channel;
		}
	current_file_info->max_channel = maxch;
}

void change_system_mode(int mode)
{
    int mid;

    if(opt_system_mid)
	mid = mode = opt_system_mid; /* Always use opt_system_mid */
    else
	mid = current_file_info->mid;
    pan_table = sc_pan_table;
///r
    switch(mode) {
      case GM_SYSTEM_MODE:
		if(play_system_mode == DEFAULT_SYSTEM_MODE)
		{
			play_system_mode = GM_SYSTEM_MODE;
			vol_table = def_vol_table;
		}
		break;
      case GM2_SYSTEM_MODE:
		play_system_mode = GM2_SYSTEM_MODE;
		vol_table = def_vol_table;
		pan_table = gm2_pan_table;
		break;
      case GS_SYSTEM_MODE:
		play_system_mode = GS_SYSTEM_MODE;
		vol_table = gs_vol_table;
		break;
      case XG_SYSTEM_MODE:
		if (play_system_mode != XG_SYSTEM_MODE) {init_all_effect_xg();}
		play_system_mode = XG_SYSTEM_MODE;
		vol_table = xg_vol_table;
		break;
      case SD_SYSTEM_MODE:
		if (play_system_mode != SD_SYSTEM_MODE) {init_all_effect_sd();}
		play_system_mode = SD_SYSTEM_MODE;
		vol_table = def_vol_table;
		pan_table = gm2_pan_table;
		break;
      case KG_SYSTEM_MODE:
		play_system_mode = KG_SYSTEM_MODE;
		vol_table = def_vol_table;
		break;
      case CM_SYSTEM_MODE:
		play_system_mode = CM_SYSTEM_MODE;
		vol_table = gs_vol_table;
		break;
      default:
	/* --module option */
		if (is_gs_module()) {
			play_system_mode = GS_SYSTEM_MODE;
			break;
		} else if (is_xg_module()) {
			if (play_system_mode != XG_SYSTEM_MODE) {init_all_effect_xg();}
			play_system_mode = XG_SYSTEM_MODE;
			break;
		} else if (is_gm2_module()) {
			play_system_mode = GM2_SYSTEM_MODE;
			break;
		} else if (is_sd_module()) {
			if (play_system_mode != SD_SYSTEM_MODE) {init_all_effect_sd();}
			play_system_mode = SD_SYSTEM_MODE;
			break;
		} else if (is_kg_module()) {
			play_system_mode = KG_SYSTEM_MODE;
			break;
		} else if (is_cm_module()) {
			play_system_mode = CM_SYSTEM_MODE;
			break;
		}
		switch(mid)	{
		case 0x41:
			play_system_mode = GS_SYSTEM_MODE;
			vol_table = gs_vol_table;
			break;
		case 0x42:
			play_system_mode = KG_SYSTEM_MODE;
			vol_table = def_vol_table;
			break;
		case 0x43:
			if (play_system_mode != XG_SYSTEM_MODE) {init_all_effect_xg();}
			play_system_mode = XG_SYSTEM_MODE;
			vol_table = xg_vol_table;
			break;
		case 0x60:
			if (play_system_mode != SD_SYSTEM_MODE) {init_all_effect_sd();}
			play_system_mode = SD_SYSTEM_MODE;
			vol_table = def_vol_table;
			pan_table = gm2_pan_table;
			break;
		case 0x30:
			play_system_mode = CM_SYSTEM_MODE;
			vol_table = gs_vol_table;
			break;
		case 0x7d:
			play_system_mode = GM2_SYSTEM_MODE;
			vol_table = def_vol_table;
			pan_table = gm2_pan_table;
			break;
		case 0x7e:
			play_system_mode = GM_SYSTEM_MODE;
			vol_table = def_vol_table;
			break;
		default:
			play_system_mode = DEFAULT_SYSTEM_MODE;
			vol_table = def_vol_table;
			break;
		}
	break;
	}
}

///r
int get_default_mapID(int ch)
{
	switch(play_system_mode){
	case XG_SYSTEM_MODE:
		return ISDRUMCHANNEL(ch) ? XG_DRUM_KIT_MAP : XG_NORMAL_MAP;
	case GM2_SYSTEM_MODE:
		return ISDRUMCHANNEL(ch) ? GM2_DRUM_MAP : GM2_TONE_MAP;
	case SD_SYSTEM_MODE:
		return ISDRUMCHANNEL(ch) ? SDXX_TONE96_MAP : SDXX_DRUM104_MAP;
	case KG_SYSTEM_MODE:
		return ISDRUMCHANNEL(ch) ? K05RW_DRUM62_MAP : K05RW_TONE0_MAP;
	case CM_SYSTEM_MODE:
		return ISDRUMCHANNEL(ch) ? CM32_DRUM_MAP : CM32L_TONE_MAP;
	default:
		return INST_NO_MAP;
	}
}

/* Allocate an array of MidiEvents and fill it from the linked list of
   events, marking used instruments for loading. Convert event times to
   samples: handle tempo changes. Strip unnecessary events from the list.
   Free the linked list. */
static MidiEvent *groom_list(int32 divisions, int32 *eventsp, int32 *samplesp)
{
    MidiEvent *groomed_list, *lp;
    MidiEventList *meep;
    int32 i, j, our_event_count, tempo, skip_this_event;
    int32 sample_cum, samples_to_do, at, st, dt, counting_time;
    int ch, gch;
    uint8 current_set[MAX_CHANNELS],
	warn_tonebank[128 + MAP_BANK_COUNT], warn_drumset[128 + MAP_BANK_COUNT];
    int8 bank_lsb[MAX_CHANNELS], bank_msb[MAX_CHANNELS], mapID[MAX_CHANNELS];
    int current_program[MAX_CHANNELS];
    int wrd_args[WRD_MAXPARAM];
    int wrd_argc;
    int chidx[256];
    int newbank, newprog;
	int elm;
#ifdef SUPPORT_LOOPEVENT
    MidiEventList *loop_startmeep;
    int32 loop_add_at;
    int32 loop_repeat_counter;
    int32 loop_begin_time, loop_end_time;
    int32 loop_begin_event_count, loop_end_event_count;
    enum {
        LOOP_TYPE_UNKNOWN = 0,
        LOOP_TYPE_CC111_TO_EOT,
        LOOP_TYPE_MARK_A_TO_B,
        LOOP_TYPE_MARK_S_TO_E,
        LOOP_TYPE_CC2_TO_CC4,
    };
    int loop_type;
    const int loop_filter = opt_use_midi_loop_repeat;
    int loop_startflag;
#endif /* SUPPORT_LOOPEVENT */

    move_channels(chidx);

    COPY_CHANNELMASK(drumchannels, current_file_info->drumchannels);
    COPY_CHANNELMASK(drumchannel_mask, current_file_info->drumchannel_mask);

    /* Move drumchannels */
    for(ch = REDUCE_CHANNELS; ch < MAX_CHANNELS; ch++)
    {
	i = chidx[ch];
	if(i != -1 && i != ch && !IS_SET_CHANNELMASK(drumchannel_mask, i))
	{
	    if(IS_SET_CHANNELMASK(drumchannels, ch))
		SET_CHANNELMASK(drumchannels, i);
	    else
		UNSET_CHANNELMASK(drumchannels, i);
	}
    }

    memset(warn_tonebank, 0, sizeof(warn_tonebank));
    if (special_tonebank >= 0)
	newbank = special_tonebank;
    else
	newbank = default_tonebank;
    for(j = 0; j < MAX_CHANNELS; j++)
    {
	if(ISDRUMCHANNEL(j))
	    current_set[j] = 0;
	else
	{
	    if (tonebank[newbank] == NULL)
	    {
		if (warn_tonebank[newbank] == 0)
		{
		    ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
			      "Tone bank %d is undefined", newbank);
		    warn_tonebank[newbank] = 1;
		}
		newbank = 0;
	    }
	    current_set[j] = newbank;
	}
	bank_lsb[j] = bank_msb[j] = 0;
///r
	switch(current_file_info->mid){
	case XG_SYSTEM_MODE:
		bank_msb[j] = (j % 16 == 9) ? 127 : 0; /* Use MSB=127 for XG */
		mapID[j] = (j % 16 == 9) ? XG_DRUM_KIT_MAP : XG_NORMAL_MAP;
		break;
	case GM2_SYSTEM_MODE:
		bank_msb[j] = (j % 16 == 9) ? 120 : 121; /* Use MSB=120 for GM2 */
		mapID[j] = (j % 16 == 9) ? GM2_DRUM_MAP : GM2_TONE_MAP;
		break;
	case SD_SYSTEM_MODE:
		bank_msb[j] = (j % 16 == 9) ? 104 : 96; /* Use MSB=104 for SD */
		mapID[j] = (j % 16 == 9) ? SDXX_TONE96_MAP : SDXX_DRUM104_MAP;
		break;
	case KG_SYSTEM_MODE:
		bank_msb[j] = (j % 16 == 9) ? 62 : 0; /* Use MSB=62 for KG */
		mapID[j] = (j % 16 == 9) ? K05RW_DRUM62_MAP : K05RW_TONE0_MAP;
		break;
	case CM_SYSTEM_MODE:
		bank_msb[j] = (j % 16 == 9) ? 127 : 0; /* Use MSB=127 for CM */
		mapID[j] = (j % 16 == 9) ? CM32_DRUM_MAP : CM32L_TONE_MAP;
		break;
	default:
		mapID[j] = INST_NO_MAP;
		break;
	}	

	current_program[j] = default_program[j];
    }

    memset(warn_drumset, 0, sizeof(warn_drumset));
    tempo = 500000;
    compute_sample_increment(tempo, divisions);

    /* This may allocate a bit more than we need */
    groomed_list = lp =
	(MidiEvent *)safe_malloc(sizeof(MidiEvent) * (event_count + 1));
    meep = evlist;
	
#ifdef SUPPORT_LOOPEVENT
    loop_startmeep = NULL;
    loop_repeat_counter = opt_use_midi_loop_repeat ? opt_midi_loop_repeat : 0;
    loop_end_event_count = loop_begin_event_count = 0;
    loop_add_at = loop_end_time = loop_begin_time = 0;
    loop_type = LOOP_TYPE_UNKNOWN;
    loop_startflag = 0;
#endif /* SUPPORT_LOOPEVENT */

    our_event_count = 0;
    st = at = sample_cum = 0;
    counting_time = 2; /* We strip any silence before the first NOTE ON. */
#if defined(KBTIM) || defined(WINVSTI)
    counting_time = 0;
#endif
    wrd_argc = 0;
    change_system_mode(DEFAULT_SYSTEM_MODE);
	
///r
	//for(j = 0; j < MAX_CHANNELS; j++)
	//	mapID[j] = get_default_mapID(j);
	
//    for (i = 0; i < event_count || (loop_startmeep && meep); i++)
    for (i = 0; i < event_count || meep; i++)
    {
	skip_this_event = 0;
	ch = meep->event.channel;
	gch = GLOBAL_CHANNEL_EVENT_TYPE(meep->event.type);
	if(!gch && ch >= MAX_CHANNELS) /* For safety */
	    meep->event.channel = ch = (ch & (MAX_CHANNELS - 1));

	if(!gch && IS_SET_CHANNELMASK(quietchannels, ch))
	    skip_this_event = 1;
	else switch(meep->event.type)
	{
	  case ME_NONE:
	    skip_this_event = 1;
	    break;
	  case ME_RESET:
	    change_system_mode(meep->event.a);
	    ctl->cmsg(CMSG_INFO, VERB_NOISY, "MIDI reset at %d sec",
		      (int)((double)st / play_mode->rate + 0.5));
	    for(j = 0; j < MAX_CHANNELS; j++)
	    {
///r
/*
		if(play_system_mode == XG_SYSTEM_MODE && j % 16 == 9)
		    mapID[j] = XG_DRUM_KIT_MAP;
		else
*/
		mapID[j] = get_default_mapID(j);


		if(ISDRUMCHANNEL(j))
		    current_set[j] = 0;
		else
		{
		    if(special_tonebank >= 0)
			current_set[j] = special_tonebank;
		    else
			current_set[j] = default_tonebank;
		    if(tonebank[current_set[j]] == NULL)
			current_set[j] = 0;
		}
		bank_lsb[j] = bank_msb[j] = 0;
///
		switch(play_system_mode){
		case XG_SYSTEM_MODE:
			bank_msb[j] = (j % 16 == 9) ? 127 : 0; /* Use MSB=127 for XG */
			break;
		case GM2_SYSTEM_MODE:
			bank_msb[j] = (j % 16 == 9) ? 120 : 121; /* Use MSB=120 for GM2 */
			break;
		case SD_SYSTEM_MODE:
			bank_msb[j] = (j % 16 == 9) ? 104 : 96; /* Use MSB=104 for SD */
			break;
		case KG_SYSTEM_MODE:
			bank_msb[j] = (j % 16 == 9) ? 62 : 0; /* Use MSB=62 for KG */
			break;
		case CM_SYSTEM_MODE:
			bank_msb[j] = (j % 16 == 9) ? 127 : 0; /* Use MSB=127 for CM */
		}

		current_program[j] = default_program[j];
	    }
	    break;

			case ME_PROGRAM:
				if (ISDRUMCHANNEL(ch))
					newbank = current_program[ch];
				else
					newbank = current_set[ch];
				newprog = meep->event.a;
				switch (play_system_mode) {
				case GS_SYSTEM_MODE:	/* GS */
					switch (bank_lsb[ch]) {
					case 0:		/* No change */
						break;
					case 1:
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(GS ch=%d SC-55 MAP)", ch);
						mapID[ch] = (ISDRUMCHANNEL(ch)) ? SC_55_DRUM_MAP
								: SC_55_TONE_MAP;
						break;
					case 2:
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(GS ch=%d SC-88 MAP)", ch);
						mapID[ch] = (ISDRUMCHANNEL(ch)) ? SC_88_DRUM_MAP
								: SC_88_TONE_MAP;
						break;
					case 3:
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(GS ch=%d SC-88Pro MAP)", ch);
						mapID[ch] = (ISDRUMCHANNEL(ch)) ? SC_88PRO_DRUM_MAP
								: SC_88PRO_TONE_MAP;
						break;
					case 4:
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(GS ch=%d SC-8820/SC-8850 MAP)", ch);
						mapID[ch] = (ISDRUMCHANNEL(ch)) ? SC_8850_DRUM_MAP
								: SC_8850_TONE_MAP;
						break;
					default:
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(GS: ch=%d Strange bank LSB %d)",
								ch, bank_lsb[ch]);
						break;
					}
					newbank = bank_msb[ch];
					break;
				case XG_SYSTEM_MODE:	/* XG */
					switch (bank_msb[ch]) {
					case 0:		/* Normal */
#if 0
						if (ch == 9 && bank_lsb[ch] == 127 && mapID[ch] == XG_DRUM_KIT_MAP){
							/* FIXME: Why this part is drum?  Is this correct? */
							ctl->cmsg(CMSG_WARNING, VERB_NORMAL,
									"Warning: XG bank 0/127 is found. "
									"It may be not correctly played.");
						}else
#endif
						{
							ctl->cmsg(CMSG_INFO, VERB_DEBUG,
									"(XG ch=%d Normal voice)", ch);
							midi_drumpart_change(ch, 0);
							mapID[ch] = XG_NORMAL_MAP;
						}
						break;
					case 126:	/* SFX kit & MU2000 sampling kit */
						if(113 <= newprog && newprog <= 116){
							ctl->cmsg(CMSG_INFO, VERB_DEBUG,
									"(XG ch=%d Sampling kit)", ch);
							midi_drumpart_change(ch, 1);
							mapID[ch] = XG_SAMPLING126_MAP;
						}else{
							ctl->cmsg(CMSG_INFO, VERB_DEBUG,
									"(XG ch=%d SFX kit)", ch);
							midi_drumpart_change(ch, 1);
							mapID[ch] = XG_SFX_KIT_MAP;
						}
						break;
					case 127:	/* Drum kit */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(XG ch=%d Drum kit)", ch);
						midi_drumpart_change(ch, 1);
						mapID[ch] = XG_DRUM_KIT_MAP;
						break;
					case 63:	/* FREE voice */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(XG ch=%d FREE voice)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = XG_FREE_MAP;
						break;
					case 48:	/* MU100Exc voice */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(XG ch=%d MU100Exc voice)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = XG_MU100EXC_MAP;
						break;
					case 16:	/* Sampling16 voice */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(XG ch=%d Sampling16 voice)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = XG_SAMPLING16_MAP;
						break;
					// PCM
					case 32:	/* PCM-USER voice */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(XG ch=%d PCM-USER voice)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = XG_PCM_USER_MAP;
						break;
					case 64:	/* PCM-SFX voice */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(XG ch=%d PCM-SFX voice)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = XG_PCM_SFX_MAP;
						break;
					case 80:	/* PCM-A voice */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(XG ch=%d PCM-A voice)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = XG_PCM_A_MAP;
						break;
					case 96:	/* PCM-B voice */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(XG ch=%d PCM-B voice)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = XG_PCM_B_MAP;
						break;
					// VA
					case 33:	/* VA-USER voice */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(XG ch=%d VA-USER voice)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = XG_VA_USER_MAP;
						break;
					case 65:	/* VA-SFX voice */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(XG ch=%d VA-SFX voice)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = XG_VA_SFX_MAP;
						break;
					case 81:	/* VA-A voice */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(XG ch=%d VA-A voice)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = XG_VA_A_MAP;
						break;
					case 97:	/* VA-B voice */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(XG ch=%d VA-B voice)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = XG_VA_B_MAP;
						break;
					// SG
					case 34:	/* SG-USER voice */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(XG ch=%d SG-USER voice)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = XG_SG_USER_MAP;
						break;
					case 66:	/* SG-SFX voice */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(XG ch=%d SG-SFX voice)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = XG_SG_SFX_MAP;
						break;
					case 82:	/* SG-A voice */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(XG ch=%d SG-A voice)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = XG_SG_A_MAP;
						break;
					case 98:	/* SG-B voice */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(XG ch=%d SG-B voice)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = XG_SG_B_MAP;
						break;
					// FM
					case 35:	/* FM-USER voice */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(XG ch=%d FM-USER voice)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = XG_FM_USER_MAP;
						break;
					case 67:	/* FM-SFX voice */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(XG ch=%d FM-SFX voice)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = XG_FM_SFX_MAP;
						break;
					case 83:	/* FM-A voice */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(XG ch=%d FM-A voice)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = XG_FM_A_MAP;
						break;
					case 99:	/* FM-B voice */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(XG ch=%d FM-B voice)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = XG_FM_B_MAP;
						break;
					default:
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(XG: ch=%d Strange bank MSB %d)",
								ch, bank_msb[ch]);
						break;
					}
					newbank = bank_lsb[ch];
					break;
///r
				case SD_SYSTEM_MODE:	// SD //
				case GM2_SYSTEM_MODE:	/* GM2 */
					switch (bank_msb[ch]) {
					case 80:		/* Sp 1 */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(SD ch=%d Special 1)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = SDXX_TONE80_MAP;
						break;
					case 81:	/* Sp 2 */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(SD ch=%d Special 2)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = SDXX_TONE81_MAP;
						break;
					case 87:	/* Usr tone */ /* SD-50 Preset Tone */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(SD ch=%d Usr tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = SDXX_TONE87_MAP;
						break;
					case 89:	/* SD-50 Solo */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(SD ch=%d 50Solo tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = SDXX_TONE89_MAP;
						break;
					case 96:	/* Clas tone */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(SD ch=%d Clas tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = SDXX_TONE96_MAP;
						break;
					case 97:	/* Cont tone */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(SD ch=%d Cont tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = SDXX_TONE97_MAP;
						break;
					case 98:	/* Solo tone */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(SD ch=%d Solo tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = SDXX_TONE98_MAP;
						break;
					case 99:	/* Enha tone */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(SD ch=%d Enha tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = SDXX_TONE99_MAP;
						break;
					case 121:	/* GM2 tone */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(SD ch=%d GM2 tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = GM2_TONE_MAP;
						break;
					case 86:	/* Usr drum */ /* SD-50 Preset Rhythm */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(SD ch=%d Usr drum)", ch);
						midi_drumpart_change(ch, 1);
						mapID[ch] = SDXX_DRUM86_MAP;
						break;
					case 104:	/* Clas drum */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(SD ch=%d Clas drum)", ch);
						midi_drumpart_change(ch, 1);
						mapID[ch] = SDXX_DRUM104_MAP;
						break;
					case 105:	/* Cont drum */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(SD ch=%d Cont drum)", ch);
						midi_drumpart_change(ch, 1);
						mapID[ch] = SDXX_DRUM105_MAP;
						break;
					case 106:	/* Solo drum */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(SD ch=%d Solo drum)", ch);
						midi_drumpart_change(ch, 1);
						mapID[ch] = SDXX_DRUM106_MAP;
						break;
					case 107:	/* Clas drum */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(SD ch=%d Enha drum)", ch);
						midi_drumpart_change(ch, 1);
						mapID[ch] = SDXX_DRUM107_MAP;
						break;
					case 120:	/* GM2 drum */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(SD ch=%d GM2 drum)", ch);
						midi_drumpart_change(ch, 1);
						mapID[ch] = GM2_DRUM_MAP;
						break;
					default:
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(GM2: ch=%d Strange bank MSB %d)",
								ch, bank_msb[ch]);
						break;
					}
					newbank = bank_lsb[ch];
					break;
				case KG_SYSTEM_MODE:	/* AG NX */
					switch (bank_msb[ch]) {
					case 0:		/* NX 0 *//* 05RW 0 */
						midi_drumpart_change(ch, 0);
						if(opt_default_module == MODULE_AG10){
							ctl->cmsg(CMSG_INFO, VERB_DEBUG,"(NX ch=%d 0 GMa/y tone)", ch);
							mapID[ch] = NX5R_TONE0_MAP;
						}else if(opt_default_module == MODULE_05RW){
							ctl->cmsg(CMSG_INFO, VERB_DEBUG,"(05RW ch=%d 0 bankA tone)", ch);
							mapID[ch] = K05RW_TONE0_MAP;
						}else if(opt_default_module == MODULE_NX5R){
							ctl->cmsg(CMSG_INFO, VERB_DEBUG,"(NX ch=%d 0 GMa/y tone)", ch);
							mapID[ch] = NX5R_TONE0_MAP;
						}
						break;
					case 1:		/* NX 1 */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 1 tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE1_MAP;
						break;
					case 2:		/* NX 2 */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 2 tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE2_MAP;
						break;
					case 3:		/* NX 3 */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 3 tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE3_MAP;
						break;
					case 4:		/* NX 4 */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 4 tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE4_MAP;
						break;
					case 5:		/* NX 5 */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 5 tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE5_MAP;
						break;
					case 6:		/* NX 6 */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 6 tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE6_MAP;
						break;
					case 7:		/* NX 7 */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 7 tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE7_MAP;
						break;
					case 8:		/* NX 8 */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 8 tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE8_MAP;
						break;
					case 9:		/* NX 9 */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 9 tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE9_MAP;
						break;
					case 10:		/* NX 10 */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 10 tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE10_MAP;
						break;
					case 11:		/* NX 11 */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 11 tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE11_MAP;
						break;
					case 16:		/* NX 16 */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 16 tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE16_MAP;
						break;
					case 17:		/* NX 17 */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 17 tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE17_MAP;
						break;
					case 18:		/* NX 18 */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 18 tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE18_MAP;
						break;
					case 19:		/* NX 19 */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 19 tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE19_MAP;
						break;
					case 24:		/* NX 24 */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 24 tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE24_MAP;
						break;
					case 25:		/* NX 25 */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 25 tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE25_MAP;
						break;
					case 26:		/* NX 26 */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 26 tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE26_MAP;
						break;
					case 32:		/* NX 32 */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 32 tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE32_MAP;
						break;
					case 33:		/* NX 33 */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 33 tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE33_MAP;
						break;
					case 40:		/* NX 40 */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 40 tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE40_MAP;
						break;
					case 56:		/* NX GMb */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(05RW ch=%d 56 GMb tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = K05RW_TONE56_MAP;
						break;
					case 57:		/* NX GMb */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(05RW ch=%d 57 GMb tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = K05RW_TONE57_MAP;
						break;
					case 64:		/* NX GMb */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 64 ySFX tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE56_MAP;
						break;
					case 80:		/* NX PrgU */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 80 PrgU tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE80_MAP;
						break;
					case 81:		/* NX PrgA */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 81 PrgA tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE81_MAP;
						break;
					case 82:		/* NX PrgU */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 82 PrgB tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE82_MAP;
						break;
					case 83:		/* NX PrgC */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 83 PrgC tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE83_MAP;
						break;
					case 88:		/* NX CmbU */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 88 CmbU tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE88_MAP;
						break;
					case 89:		/* NX CmbA */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 89 CmbA tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE89_MAP;
						break;
					case 90:		/* NX CmbB */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 90 CmbB tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE90_MAP;
						break;
					case 91:		/* NX CmbC */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 91 CmbC tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE91_MAP;
						break;
					case 125:		/* NX CM */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d 125 CM tone)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = NX5R_TONE125_MAP;
						break;
					case 61:	/* NX r drum */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d NX r drum)", ch);
						midi_drumpart_change(ch, 1);
						mapID[ch] = NX5R_DRUM61_MAP;
						break;
					case 62:	/* 05RW k drum */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(05RW ch=%d NX k drum)", ch);
						midi_drumpart_change(ch, 1);
						mapID[ch] = K05RW_DRUM62_MAP;
						break;
					case 126:	/* NX y1 drum */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d NX y1 drum)", ch);
						midi_drumpart_change(ch, 1);
						mapID[ch] = NX5R_DRUM126_MAP;
						break;
					case 127:	/* NX y2 drum */
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX ch=%d NX y2 drum)", ch);
						midi_drumpart_change(ch, 1);
						mapID[ch] = NX5R_DRUM127_MAP;
						break;
					default:
						ctl->cmsg(CMSG_INFO, VERB_DEBUG,
								"(NX: ch=%d Strange bank MSB %d)",
								ch, bank_msb[ch]);
						break;
					}
					newbank = bank_lsb[ch];
					break;
				case CM_SYSTEM_MODE:	/* MT32 CMxx */
					if(opt_default_module == MODULE_CM500D){ // CM500D 1-9GS 10-15LA 
						if(ch < 10){
							ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(CM ch=%d CM300 MAP)", ch);
							mapID[ch] = (ISDRUMCHANNEL(ch)) ? SC_55_DRUM_MAP : SC_55_TONE_MAP;
						}else{
							ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(CM ch=%d CM32L MAP)", ch);
							mapID[ch] = CM32L_TONE_MAP;
						}
					}else if(ch > 15){
						ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(GS ch=%d SC-88Pro MAP)", ch);
						mapID[ch] = (ISDRUMCHANNEL(ch)) ? SC_88PRO_DRUM_MAP	: SC_88PRO_TONE_MAP;
					}else if((ch & (16 - 1)) == 9){
	/*					if(opt_default_module == MODULE_MT32){
							ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(CM ch=%d MT32 drum MAP)", ch);
							mapID[ch] = MT32_DRUM_MAP;
						}else*/
						{
							ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(CM ch=%d CM32L drum MAP)", ch);
							mapID[ch] = CM32_DRUM_MAP;
						}
					}else if((ch & (16 - 1)) < 9){
	/*					if(opt_default_module == MODULE_MT32){
							ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(CM ch=%d MT32 MAP)", ch);
							mapID[ch] = MT32_TONE_MAP;
						}else*/
						{
							ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(CM ch=%d CM32L MAP)", ch);
							mapID[ch] = CM32L_TONE_MAP;
						}
					}else if(current_program[ch] < 64){
						ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(CM ch=%d CM32P MAP)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = CM32P_TONE_MAP;
					}else switch (opt_default_module) {
					case MODULE_CM64_SN01:
						ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(CM ch=%d SN01 MAP)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = SN01_TONE_MAP;
						break;
					case MODULE_CM64_SN02:
						if(current_program[ch] < 71){
							ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(CM ch=%d SN02 drum MAP)", ch);
							midi_drumpart_change(ch, 1);
							mapID[ch] = SN02_DRUM_MAP;
						}else {
							ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(CM ch=%d SN02 MAP)", ch);
						midi_drumpart_change(ch, 0);
							mapID[ch] = SN02_TONE_MAP;
						}
						break;
					case MODULE_CM64_SN03:
						ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(CM ch=%d SN03 MAP)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = SN03_TONE_MAP;
						break;
					case MODULE_CM64_SN04:
						ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(CM ch=%d SN04 MAP)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = SN04_TONE_MAP;
						break;
					case MODULE_CM64_SN05:
						ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(CM ch=%d SN05 MAP)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = SN05_TONE_MAP;
						break;
					case MODULE_CM64_SN06:
						ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(CM ch=%d SN06 MAP)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = SN06_TONE_MAP;
						break;
					case MODULE_CM64_SN07:
						ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(CM ch=%d SN07 MAP)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = SN07_TONE_MAP;
						break;
					case MODULE_CM64_SN08:
						ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(CM ch=%d SN08 MAP)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = SN08_TONE_MAP;
						break;
					case MODULE_CM64_SN09:
						ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(CM ch=%d SN09 MAP)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = SN09_TONE_MAP;
						break;
					case MODULE_CM64_SN10:
						ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(CM ch=%d SN10 drum MAP)", ch);
						midi_drumpart_change(ch, 1);
						mapID[ch] = SN10_DRUM_MAP;
						break;
					case MODULE_CM64_SN11:
						ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(CM ch=%d SN11 MAP)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = SN11_TONE_MAP;
						break;
					case MODULE_CM64_SN12:
						ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(CM ch=%d SN12 MAP)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = SN12_TONE_MAP;
						break;
					case MODULE_CM64_SN13:
						ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(CM ch=%d SN13 MAP)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = SN13_TONE_MAP;
						break;
					case MODULE_CM64_SN14:
						ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(CM ch=%d SN14 MAP)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = SN14_TONE_MAP;
						break;
					case MODULE_CM64_SN15:
						ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(CM ch=%d SN15 MAP)", ch);
						midi_drumpart_change(ch, 0);
						mapID[ch] = SN15_TONE_MAP;
						break;
					default:
						break;
					}
					newbank = bank_msb[ch];
					break;
				default:
					newbank = bank_msb[ch];
					break;
				}
				if (ISDRUMCHANNEL(ch))
					current_set[ch] = newprog;
				else {
					if (special_tonebank >= 0)
						newbank = special_tonebank;
					if (current_program[ch] == SPECIAL_PROGRAM)
						skip_this_event = 1;
					current_set[ch] = newbank;
				}
				current_program[ch] = newprog;
				break;

	  case ME_NOTEON:
	    if(counting_time)
		counting_time = 1;
	    if(ISDRUMCHANNEL(ch))
	    {
			newbank = current_set[ch];
			newprog = meep->event.a;
			instrument_map(mapID[ch], &newbank, &newprog);
			if(!drumset[newbank]) /* Is this a defined drumset? */
			{
				if(warn_drumset[newbank] == 0)
				{
				ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
					  "Drum set %d is undefined", newbank);
				warn_drumset[newbank] = 1;
				}
				newbank = 0;
			}
		}
		else
		{
			if(current_program[ch] == SPECIAL_PROGRAM)
				break;
			newbank = current_set[ch];
			newprog = current_program[ch];
			instrument_map(mapID[ch], &newbank, &newprog);
			if(tonebank[newbank] == NULL)
			{
				if(warn_tonebank[newbank] == 0)
				{
				ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
						"Tone bank %d is undefined", newbank);
				warn_tonebank[newbank] = 1;
				}
				newbank = 0;
			}
		}
	    break;

	  case ME_TONE_BANK_MSB:
	    bank_msb[ch] = meep->event.a;
	    break;

	  case ME_TONE_BANK_LSB:
	    bank_lsb[ch] = meep->event.a;
	    break;
		
#ifdef SUPPORT_LOOPEVENT
          /* CC#111 - End of Track */
          case ME_LOOP_START: /* CC#111 */
            if ((loop_filter & LF_CC111_TO_EOT) != 0 &&
                (loop_type == LOOP_TYPE_UNKNOWN || loop_type == LOOP_TYPE_CC111_TO_EOT) &&
                loop_startmeep != meep)
            {
                loop_type = LOOP_TYPE_CC111_TO_EOT;
                loop_begin_time = meep->event.time;
                loop_startmeep = meep;
                loop_begin_event_count = i;
                loop_end_event_count = event_count + 1;
                ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
                          "ME_LOOP_START: %d", loop_begin_time);
                groomed_list = lp =
                    (MidiEvent*) safe_large_realloc(groomed_list, sizeof(MidiEvent) * (event_count +
                        (loop_end_event_count - loop_begin_event_count) *
                            loop_repeat_counter + 1));
                lp += our_event_count;
            }
            skip_this_event = 1;
            break;

          /* CC#2 - CC#4 */
          case ME_BREATH: /* CC#2 */
            if ((loop_filter & LF_CC2_TO_CC4) != 0) {
                ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
                          "ME_BREATH(LOOP BEGIN): %d", meep->event.time);
                if ((loop_type == LOOP_TYPE_UNKNOWN || loop_type == LOOP_TYPE_CC2_TO_CC4) &&
                    loop_startmeep != meep)
                {
                    loop_type = LOOP_TYPE_CC2_TO_CC4;
                    loop_begin_time = meep->event.time;
                    loop_startmeep = meep;
                    loop_begin_event_count = i;
                }
            }
            skip_this_event = 1;
            break;

          case ME_FOOT: /* CC#4 */
            if ((loop_filter & LF_CC2_TO_CC4) != 0) {
                ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
                          "ME_FOOT(LOOP END): %d", meep->event.time);
                if (loop_type == LOOP_TYPE_CC2_TO_CC4 &&
                        loop_startmeep && loop_repeat_counter > 0)
                {
                    if (loop_end_event_count == 0) {
                        loop_end_event_count = i;
                        groomed_list = lp =
                            (MidiEvent*) safe_large_realloc(groomed_list, sizeof(MidiEvent) * (event_count +
                                (loop_end_event_count - loop_begin_event_count) *
                                    loop_repeat_counter + 1));
                        lp += our_event_count;
                    }

                    loop_startflag = 1;
                }
            }
            skip_this_event = 1;
            break;

          /* Marker jump */
          case ME_MARKER: {
            int16 nstring = meep->event.a | ((int) meep->event.b << 8);
            const char *text = event2string(nstring);
            if (text) {
                /* A-B */
                if (strcmp(text + 1, "(A)") == 0) {
                    ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
                              "ME_MARKER(%c): %d", 'A', meep->event.time);
                    if ((loop_filter & LF_MARK_A_TO_B) != 0 &&
                        (loop_type == LOOP_TYPE_UNKNOWN || loop_type == LOOP_TYPE_MARK_A_TO_B) &&
                        loop_end_event_count == 0 &&
                        loop_startmeep != meep)
                    {
                        loop_type = LOOP_TYPE_MARK_A_TO_B;
                        loop_begin_time = meep->event.time;
                        loop_startmeep = meep;
                        loop_begin_event_count = i;
                    }
                    skip_this_event = 1;
                }
                if (strcmp(text + 1, "(B)") == 0) {
                    ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
                              "ME_MARKER(%c): %d", 'B', meep->event.time);
                    if (loop_type == LOOP_TYPE_MARK_A_TO_B &&
                            loop_startmeep && loop_repeat_counter > 0)
                    {
                        if (loop_end_event_count == 0) {
                            loop_end_event_count = i;
                            groomed_list = lp =
                                (MidiEvent*) safe_large_realloc(groomed_list, sizeof(MidiEvent) * (event_count +
                                    (loop_end_event_count - loop_begin_event_count) *
                                        loop_repeat_counter + 1));
                            lp += our_event_count;
                        }

                        loop_startflag = 1;
                    }
                    skip_this_event = 1;
                }

                /* Loop_Start-Loop_End */
                if (strcmp(text + 1, "(Loop_Start)") == 0) {
                    ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
                              "ME_MARKER(%c): %d", 'S', meep->event.time);
                    if ((loop_filter & LF_MARK_S_TO_E) != 0 &&
                        (loop_type == LOOP_TYPE_UNKNOWN || loop_type == LOOP_TYPE_MARK_S_TO_E) &&
                        loop_end_event_count == 0 &&
                        loop_startmeep != meep)
                    {
                        loop_type = LOOP_TYPE_MARK_S_TO_E;
                        loop_begin_time = meep->event.time;
                        loop_startmeep = meep;
                        loop_begin_event_count = i;
                    }
                    skip_this_event = 1;
                }
                if (strcmp(text + 1, "(Loop_End)") == 0) {
                    ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
                              "ME_MARKER(%c): %d", 'E', meep->event.time);
                    if (loop_type == LOOP_TYPE_MARK_S_TO_E &&
                            loop_startmeep && loop_repeat_counter > 0)
                    {
                        if (loop_end_event_count == 0) {
                            loop_end_event_count = i;
                            groomed_list = lp =
                                (MidiEvent*) safe_large_realloc(groomed_list, sizeof(MidiEvent) * (event_count +
                                    (loop_end_event_count - loop_begin_event_count) *
                                        loop_repeat_counter + 1));
                            lp += our_event_count;
                        }

                        loop_startflag = 1;
                    }
                    skip_this_event = 1;
                }
            }
            if (counting_time && ctl->trace_playing)
                counting_time = 1;
            break; }
#else
          case ME_BREATH:
            if (counting_time == 2)
                skip_this_event = 1;
            break;

          case ME_FOOT:
            if (counting_time == 2)
                skip_this_event = 1;
            break;

          case ME_MARKER:
            // thru
#endif /* SUPPORT_LOOPEVENT */

	  case ME_CHORUS_TEXT:
	  case ME_LYRIC:
	  case ME_INSERT_TEXT:
	  case ME_TEXT:
	  case ME_KARAOKE_LYRIC:
	    if((meep->event.a | meep->event.b) == 0)
		skip_this_event = 1;
	    else if(counting_time && ctl->trace_playing)
		counting_time = 1;
	    break;

	  case ME_GSLCD:
	    if (counting_time && ctl->trace_playing)
		counting_time = 1;
	    skip_this_event = !ctl->trace_playing;
	    break;

	  case ME_DRUMPART:
	    midi_drumpart_change(ch, meep->event.a);
	    break;

	  case ME_WRD:
	    if(readmidi_wrd_mode == WRD_TRACE_MIMPI)
	    {
		wrd_args[wrd_argc++] = meep->event.a | 256 * meep->event.b;
		if(ch != WRD_ARG)
		{
		    if(ch == WRD_MAG) {
			wrdt->apply(WRD_MAGPRELOAD, wrd_argc, wrd_args);
		    }
		    else if(ch == WRD_PLOAD)
			wrdt->apply(WRD_PHOPRELOAD, wrd_argc, wrd_args);
		    else if(ch == WRD_PATH)
			wrdt->apply(WRD_PATH, wrd_argc, wrd_args);
		    wrd_argc = 0;
		}
	    }
	    if(counting_time == 2 && readmidi_wrd_mode != WRD_TRACE_NOTHING)
		counting_time = 1;
	    break;

	  case ME_SHERRY:
	    if(counting_time == 2)
		counting_time = 1;
	    break;

	  case ME_NOTE_STEP:
	    if(counting_time == 2)
		skip_this_event = 1;
	    break;

	  case ME_CUEPOINT:
	    if (counting_time == 2)
		skip_this_event = 1;
	    break;
        }
		
        /* Recompute time in samples*/
        dt = meep->event.time - at;
#ifdef SUPPORT_LOOPEVENT
        dt += loop_add_at;
#endif /* SUPPORT_LOOPEVENT */
        if (dt != 0 && !counting_time) {
            samples_to_do = sample_increment * dt;
            sample_cum += sample_correction * dt;
            if (sample_cum & 0xFFFF0000) {
                samples_to_do += ((sample_cum >> 16) & 0xFFFF);
                sample_cum &= 0x0000FFFF;
            }
            st += samples_to_do;
            if (st < 0) {
                ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
                          "Overflow the sample counter");
                safe_free(groomed_list);
                return NULL;
            }
        }
        else if (counting_time == 1)
            counting_time = 0;

        if (meep->event.type == ME_TEMPO) {
            tempo = ch + meep->event.b * 256 + meep->event.a * 65536;
            compute_sample_increment(tempo, divisions);
        }

        if (!skip_this_event) {
            /* Add the event to the list */
            *lp = meep->event;
            lp->time = st;
            lp++;
            our_event_count++;
        }
        at = meep->event.time;
#ifdef SUPPORT_LOOPEVENT
        at += loop_add_at;
#endif /* SUPPORT_LOOPEVENT */
        meep = meep->next;

#ifdef SUPPORT_LOOPEVENT
        if (!meep && loop_type == LOOP_TYPE_CC111_TO_EOT &&
                loop_startmeep && loop_repeat_counter > 0)
        {
            // End of Track
            loop_startflag = 1;
            ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
                      "End of Track: %d", at);
        }

        if (loop_startflag) {
            if (loop_startmeep) {
                if (loop_end_time == 0)
                    loop_end_time = at;
                loop_add_at += (loop_end_time - loop_begin_time);
                meep = loop_startmeep->next;
                ctl->cmsg(CMSG_INFO, VERB_DEBUG,
                          "Loop jump: from %d, to %d",
                          loop_end_time,
                          meep->event.time);
            }
            loop_repeat_counter--;
            loop_startflag = 0;
        }
#endif /* SUPPORT_LOOPEVENT */
    }
    /* Add an End-of-Track event */
    lp->time = st;
    lp->type = ME_EOT;
    our_event_count++;
    free_midi_list();
    
    *eventsp = our_event_count;
    *samplesp = st;
    return groomed_list;
}

static int read_smf_file(struct timidity_file *tf)
{
    int32 len, divisions;
    int16 format, tracks, divisions_tmp;
    int i;

    if(current_file_info->file_type == IS_OTHER_FILE)
	current_file_info->file_type = IS_SMF_FILE;

    if(current_file_info->karaoke_title == NULL)
	karaoke_title_flag = 0;
    else
	karaoke_title_flag = 1;

    _set_errno(0);
    if(tf_read(&len, 4, 1, tf) != 1)
    {
	if(errno)
	    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s", current_filename,
		      strerror(errno));
	else
	    ctl->cmsg(CMSG_WARNING, VERB_NORMAL,
		      "%s: Not a MIDI file!", current_filename);
	return 1;
    }
    len = BE_LONG(len);

    tf_read(&format, 2, 1, tf);
    tf_read(&tracks, 2, 1, tf);
    tf_read(&divisions_tmp, 2, 1, tf);
    format = BE_SHORT(format);
    tracks = BE_SHORT(tracks);
    divisions_tmp = BE_SHORT(divisions_tmp);

    if(divisions_tmp < 0)
    {
	/* SMPTE time -- totally untested. Got a MIDI file that uses this? */
	divisions=
	    (int32)(-(divisions_tmp / 256)) * (int32)(divisions_tmp & 0xFF);
    }
    else
	divisions = (int32)divisions_tmp;

    if(play_mode->flag & PF_MIDI_EVENT)
	play_mode->acntl(PM_REQ_DIVISIONS, &divisions);

    if(len > 6)
    {
	ctl->cmsg(CMSG_WARNING, VERB_NORMAL,
		  "%s: MIDI file header size %ld bytes",
		  current_filename, len);
	skip(tf, len - 6); /* skip the excess */
    }
    if(format < 0 || format > 2)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "%s: Unknown MIDI file format %d", current_filename, format);
	return 1;
    }
    ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "Format: %d  Tracks: %d  Divisions: %d",
	      format, tracks, divisions);

    current_file_info->format = format;
    current_file_info->tracks = tracks;
    current_file_info->divisions = divisions;
    if(tf->url->url_tell != NULL)
	current_file_info->hdrsiz = (int16)tf_tell(tf);
    else
	current_file_info->hdrsiz = -1;

    switch(format)
    {
      case 0:
	if(read_smf_track(tf, 0, 1))
	{
	    if(ignore_midi_error)
		break;
	    return 1;
	}
	break;

      case 1:
	for(i = 0; i < tracks; i++)
	{
	    if(read_smf_track(tf, i, 1))
	    {
		if(ignore_midi_error)
		    break;
		return 1;
	    }
	}
	break;

      case 2: /* We simply play the tracks sequentially */
	for(i = 0; i < tracks; i++)
	{
	    if(read_smf_track(tf, i, 0))
	    {
		if(ignore_midi_error)
		    break;
		return 1;
	    }
	}
	break;
    }
    return 0;
}

void readmidi_read_init(void)
{
    int i;
	static int first = 1;

	/* initialize effect status */
	for (i = 0; i < MAX_CHANNELS; i++)
		init_channel_layer(i);
	if(!first)
		free_effect_buffers();
	switch(play_system_mode){
	case XG_SYSTEM_MODE:
		init_all_effect_xg();
		break;
	case SD_SYSTEM_MODE:
		init_all_effect_sd();
		break;
	case GM2_SYSTEM_MODE:
		init_reverb_status_gs(); // gm2
		init_delay_status_gs(); // gm2
		init_multi_eq_sd();
		break;
	default:
		init_reverb_status_gs();
		init_delay_status_gs();
		init_chorus_status_gs();
		init_eq_status_gs();
		init_insertion_effect_gs();
		break;
	
	}
	init_userdrum();
	init_userinst();
	rhythm_part[0] = rhythm_part[1] = 9;
	for(i = 0; i < 6; i++) {drum_setup_xg[i] = 9;}

    /* Put a do-nothing event first in the list for easier processing */
    evlist = current_midi_point = alloc_midi_event();
    evlist->event.time = 0;
    evlist->event.type = ME_NONE;
    evlist->event.channel = 0;
    evlist->event.a = 0;
    evlist->event.b = 0;
    evlist->prev = NULL;
    evlist->next = NULL;
    readmidi_error_flag = 0;
    event_count = 1;

    if(string_event_table != NULL)
    {
	safe_free(string_event_table[0]);
	safe_free(string_event_table);
	string_event_table = NULL;
	string_event_table_size = 0;
    }
	if (first != 1)
		if (string_event_strtab.nstring > 0)
			delete_string_table(&string_event_strtab);
    init_string_table(&string_event_strtab);
    karaoke_format = 0;

    for(i = 0; i < 256; i++)
	default_channel_program[i] = -1;
    readmidi_wrd_mode = WRD_TRACE_NOTHING;
	first = 0;
	
#ifdef VST_LOADER_ENABLE
	if (hVSTHost != NULL)
		((vst_processing_init)GetProcAddress(hVSTHost, "effectInit"))();
#endif
}

static void insert_note_steps(void)
{
	MidiEventList *e;
	int32 i, n, at, lasttime, meas, beat;
	uint8 num = 0, denom = 1, a, b;
	
	e = evlist;
	for (i = n = 0; i < event_count - 1 && n < 256 - 1; i++, e = e->next)
		if (e->event.type == ME_TIMESIG && e->event.channel == 0) {
			if (n == 0 && e->event.time > 0) {	/* 4/4 is default */
				SETMIDIEVENT(timesig[n], 0, ME_TIMESIG, 0, 4, 4);
				n++;
			}
			if (n > 0 && e->event.a == timesig[n - 1].a
					&& e->event.b == timesig[n - 1].b)
				continue;	/* unchanged */
			if (n > 0 && e->event.time == timesig[n - 1].time)
				n--;	/* overwrite previous timesig */
			timesig[n++] = e->event;
		}
	if (n == 0) {
		SETMIDIEVENT(timesig[n], 0, ME_TIMESIG, 0, 4, 4);
		n++;
	}
	timesig[n] = timesig[n - 1];
	timesig[n].time = 0x7fffffff;	/* stopper */
	lasttime = e->event.time;
	readmidi_set_track(0, 1);
	at = n = meas = beat = 0;
	while (at < lasttime && ! readmidi_error_flag) {
		if (at >= timesig[n].time) {
			if (beat != 0)
				meas++, beat = 0;
			num = timesig[n].a, denom = timesig[n].b, n++;

            if (denom == 0) {
                ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "warning: invalid denominator in timesig");
                denom = 1;
            }
		}
		a = (meas + 1) & 0xff;
		b = (((meas + 1) >> 8) & 0x0f) + ((beat + 1) << 4);
		MIDIEVENT(at, ME_NOTE_STEP, 0, a, b);
		if (++beat == num)
			meas++, beat = 0;
		at += current_file_info->divisions * 4 / denom;
	}
}

static int32 compute_smf_at_time(const int32, int32 *);
static int32 compute_smf_at_time2(const Measure, int32 *);

static void insert_cuepoints(void)
{
	TimeSegment *sp;
	int32 at, st, t;
	uint8 a0, b0, a1, b1;

	for (sp = time_segments; sp != NULL; sp = sp->next) {
		if (sp->type == 0) {
			if (sp->prev == NULL && sp->begin.s != 0) {
				t = sp->begin.s * play_mode->rate;
				a0 = t >> 24, b0 = t >> 16, a1 = t >> 8, b1 = t;
				MIDIEVENT(0, ME_NOTEON, 0, 0, 0);
				MIDIEVENT(0, ME_CUEPOINT, 0, a0, b0);
				MIDIEVENT(0, ME_CUEPOINT, 1, a1, b1);
			}
			if (sp->next != NULL) {
				at = compute_smf_at_time(sp->end.s * play_mode->rate, &st);
				if (sp->next->type == 0)
					t = sp->next->begin.s * play_mode->rate - st;
				else
					compute_smf_at_time2(sp->next->begin.m, &t), t -= st;
				a0 = t >> 24, b0 = t >> 16, a1 = t >> 8, b1 = t;
				MIDIEVENT(at, ME_CUEPOINT, 0, a0, b0);
				MIDIEVENT(at, ME_CUEPOINT, 1, a1, b1);
			} else if (sp->end.s != -1) {
				at = compute_smf_at_time(sp->end.s * play_mode->rate, &st);
				t = 0x7fffffff;		/* stopper */
				a0 = t >> 24, b0 = t >> 16, a1 = t >> 8, b1 = t;
				MIDIEVENT(at, ME_CUEPOINT, 0, a0, b0);
				MIDIEVENT(at, ME_CUEPOINT, 1, a1, b1);
			}
		} else {
			if (sp->prev == NULL
					&& (sp->begin.m.meas != 1 || sp->begin.m.beat != 1)) {
				compute_smf_at_time2(sp->begin.m, &t);
				a0 = t >> 24, b0 = t >> 16, a1 = t >> 8, b1 = t;
				MIDIEVENT(0, ME_NOTEON, 0, 0, 0);
				MIDIEVENT(0, ME_CUEPOINT, 0, a0, b0);
				MIDIEVENT(0, ME_CUEPOINT, 1, a1, b1);
			}
			if (sp->next != NULL) {
				at = compute_smf_at_time2(sp->end.m, &st);
				if (sp->next->type == 0)
					t = sp->next->begin.s * play_mode->rate - st;
				else
					compute_smf_at_time2(sp->next->begin.m, &t), t -= st;
				a0 = t >> 24, b0 = t >> 16, a1 = t >> 8, b1 = t;
				MIDIEVENT(at, ME_CUEPOINT, 0, a0, b0);
				MIDIEVENT(at, ME_CUEPOINT, 1, a1, b1);
			} else if (sp->end.m.meas != -1 || sp->end.m.beat != -1) {
				at = compute_smf_at_time2(sp->end.m, &st);
				t = 0x7fffffff;		/* stopper */
				a0 = t >> 24, b0 = t >> 16, a1 = t >> 8, b1 = t;
				MIDIEVENT(at, ME_CUEPOINT, 0, a0, b0);
				MIDIEVENT(at, ME_CUEPOINT, 1, a1, b1);
			}
		}
	}
}

static int32 compute_smf_at_time(const int32 sample, int32 *sample_adj)
{
	MidiEventList *e;
	int32 st = 0, tempo = 500000, prev_time = 0;
	int i;

	for (i = 0, e = evlist; i < event_count; i++, e = e->next) {
		st += (double) tempo * play_mode->rate / 1000000
				/ current_file_info->divisions
				* (e->event.time - prev_time) + 0.5;
		if (st >= sample && e->event.type == ME_NOTE_STEP) {
			*sample_adj = st;
			return e->event.time;
		}
		if (e->event.type == ME_TEMPO)
		    tempo = e->event.a * 65536 + e->event.b * 256 + e->event.channel;
		prev_time = e->event.time;
	}
	return -1;
}

static int32 compute_smf_at_time2(const Measure m, int32 *sample)
{
	MidiEventList *e;
	int32 st = 0, tempo = 500000, prev_time = 0;
	int i;

	for (i = 0, e = evlist; i < event_count; i++, e = e->next) {
		st += (double) tempo * play_mode->rate / 1000000
				/ current_file_info->divisions
				* (e->event.time - prev_time) + 0.5;
		if (e->event.type == ME_NOTE_STEP
				&& ((e->event.a + ((e->event.b & 0x0f) << 8)) * 16
				+ (e->event.b >> 4)) >= m.meas * 16 + m.beat) {
			*sample = st;
			return e->event.time;
		}
		if (e->event.type == ME_TEMPO)
		    tempo = e->event.a * 65536 + e->event.b * 256 + e->event.channel;
		prev_time = e->event.time;
	}
	return -1;
}

void free_time_segments(void)
{
	TimeSegment *sp, *next;

	for (sp = time_segments; sp != NULL; sp = next)
		next = sp->next, free(sp);
	time_segments = NULL;
}

#if !defined(KBTIM) && !defined(WINVSTI) //added by Kobarin(read_midi_file の不要なコードを除去)
MidiEvent *read_midi_file(struct timidity_file *tf, int32 *count, int32 *sp,
			  char *fn)
{
    char magic[4];
    MidiEvent *ev;
    int err, macbin_check, mtype, i;

    macbin_check = 1;
    current_file_info = get_midi_file_info(current_filename, 1);
    COPY_CHANNELMASK(drumchannels, current_file_info->drumchannels);
    COPY_CHANNELMASK(drumchannel_mask, current_file_info->drumchannel_mask);

    _set_errno(0);

    if((mtype = get_module_type(fn)) > 0)
    {
	readmidi_read_init();
	if(!IS_URL_SEEK_SAFE(tf->url))
	    tf->url = url_cache_open(tf->url, 1);
	err = load_module_file(tf, mtype);
	if(!err)
	{
	    current_file_info->format = 0;
	    memset(&drumchannels, 0, sizeof(drumchannels));
	    goto grooming;
	}
	free_midi_list();

	if(err == 2)
	    return NULL;
	url_rewind(tf->url);
	url_cache_disable(tf->url);
    }

#if MAX_CHANNELS > 16
    for(i = 16; i < MAX_CHANNELS; i++)
    {
	if(!IS_SET_CHANNELMASK(drumchannel_mask, i))
	{
	    if(IS_SET_CHANNELMASK(drumchannels, i & 0xF))
		SET_CHANNELMASK(drumchannels, i);
	    else
		UNSET_CHANNELMASK(drumchannels, i);
	}
    }
#endif

    if(opt_default_mid &&  (current_file_info->mid == 0 || current_file_info->mid >= 0x7e))
		current_file_info->mid = opt_default_mid;

  retry_read:
    if(tf_read(magic, 1, 4, tf) != 4)
    {
	if(errno)
	    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s", current_filename,
		      strerror(errno));
	else
	    ctl->cmsg(CMSG_WARNING, VERB_NORMAL,
		      "%s: Not a MIDI file!", current_filename);
	return NULL;
    }

    if(memcmp(magic, "MThd", 4) == 0)
    {
	readmidi_read_init();
	err = read_smf_file(tf);
    }
    else if(memcmp(magic, "RCM-", 4) == 0 || memcmp(magic, "COME", 4) == 0)
    {
	readmidi_read_init();
	err = read_rcp_file(tf, magic, fn);
    }
    else if (strncmp(magic, "RIFF", 4) == 0) {
       if (tf_read(magic, 1, 4, tf) == 4 &&
           tf_read(magic, 1, 4, tf) == 4 &&
           strncmp(magic, "RMID", 4) == 0 &&
           tf_read(magic, 1, 4, tf) == 4 &&
           strncmp(magic, "data", 4) == 0 &&
           tf_read(magic, 1, 4, tf) == 4) {
           goto retry_read;
       } else {
           err = 1;
           ctl->cmsg(CMSG_WARNING, VERB_NORMAL,
                     "%s: Not a MIDI file!", current_filename);
       }
    }
    else if(memcmp(magic, "melo", 4) == 0)
    {
	readmidi_read_init();
	err = read_mfi_file(tf);
    }
    else
    {
	if(macbin_check && magic[0] == 0)
	{
	    /* Mac Binary */
	    macbin_check = 0;
	    skip(tf, 128 - 4);
	    goto retry_read;
	}
	else if(memcmp(magic, "RIFF", 4) == 0)
	{
	    /* RIFF MIDI file */
	    skip(tf, 20 - 4);
	    goto retry_read;
	}
	err = 1;
	ctl->cmsg(CMSG_WARNING, VERB_NORMAL,
		  "%s: Not a MIDI file!", current_filename);
    }

    if(err)
    {
	free_midi_list();
	if(string_event_strtab.nstring > 0)
	    delete_string_table(&string_event_strtab);
	return NULL;
    }

    /* Read WRD file */
    if(!(play_mode->flag&PF_CAN_TRACE))
    {
	if(wrdt->start != NULL)
	    wrdt->start(WRD_TRACE_NOTHING);
	readmidi_wrd_mode = WRD_TRACE_NOTHING;
    }
    else if(wrdt->id != '-' && wrdt->opened)
    {
	readmidi_wrd_mode = import_wrd_file(fn);
	if(wrdt->start != NULL)
	    if(wrdt->start(readmidi_wrd_mode) == -1)
	    {
		/* strip all WRD events */
		MidiEventList *e;
		int32 i;
		for(i = 0, e = evlist; i < event_count; i++, e = e->next)
		    if (e->event.type == ME_WRD || e->event.type == ME_SHERRY)
			e->event.type = ME_NONE;
	    }
    }
    else
	readmidi_wrd_mode = WRD_TRACE_NOTHING;

    /* make lyric table */
    if(string_event_strtab.nstring > 0)
    {
	string_event_table_size = string_event_strtab.nstring;
	string_event_table = make_string_array(&string_event_strtab);
	if(string_event_table == NULL)
	{
	    delete_string_table(&string_event_strtab);
	    string_event_table_size = 0;
	}
    }

  grooming:
    insert_note_steps();
    ev = groom_list(current_file_info->divisions, count, sp);
    if(ev == NULL)
    {
	free_midi_list();
	if(string_event_strtab.nstring > 0)
	    delete_string_table(&string_event_strtab);
	return NULL;
    }
    current_file_info->samples = *sp;
    if(current_file_info->first_text == NULL)
	current_file_info->first_text = safe_strdup("");
    current_file_info->readflag = 1;
    return ev;
}
#else //KBTIM // WINVSTI
//read_midi_file 置き換え by Kobarin
MidiEvent *read_midi_file(struct timidity_file *tf, int32 *count, int32 *sp,
			  char *fn)
{//以下の対応部分を除去
 // SMF 以外の形式(RCP/着メロ等)
 // マックバイナリ
 // RMI(RIFF MIDI)
 // WRD
    char magic[4];
    MidiEvent *ev;
    int err, macbin_check, mtype, i;

    macbin_check = 1;
    current_file_info = get_midi_file_info(current_filename, 1);
    COPY_CHANNELMASK(drumchannels, current_file_info->drumchannels);
    COPY_CHANNELMASK(drumchannel_mask, current_file_info->drumchannel_mask);

    _set_errno(0);

#if MAX_CHANNELS > 16
    for(i = 16; i < MAX_CHANNELS; i++)
    {
	if(!IS_SET_CHANNELMASK(drumchannel_mask, i))
	{
	    if(IS_SET_CHANNELMASK(drumchannels, i & 0xF))
		SET_CHANNELMASK(drumchannels, i);
	    else
		UNSET_CHANNELMASK(drumchannels, i);
	}
    }
#endif

    if(opt_default_mid &&
        (current_file_info->mid == 0 || current_file_info->mid >= 0x7e)){
	    current_file_info->mid = opt_default_mid;
    }
  retry_read:
    if(tf_read(magic, 1, 4, tf) != 4){
	    return NULL;
    }

    if(memcmp(magic, "MThd", 4) == 0){
	    readmidi_read_init();
	    err = read_smf_file(tf);
    }
#ifdef IN_TIMIDITY // SMF以外対応 // for in_timidity
    else if(memcmp(magic, "RCM-", 4) == 0 || memcmp(magic, "COME", 4) == 0)
    {
	readmidi_read_init();
	err = read_rcp_file(tf, magic, fn);
    }
    else if (strncmp(magic, "RIFF", 4) == 0) {
       if (tf_read(magic, 1, 4, tf) == 4 &&
           tf_read(magic, 1, 4, tf) == 4 &&
           strncmp(magic, "RMID", 4) == 0 &&
           tf_read(magic, 1, 4, tf) == 4 &&
           strncmp(magic, "data", 4) == 0 &&
           tf_read(magic, 1, 4, tf) == 4) {
           goto retry_read;
       } else {
           err = 1;
           ctl->cmsg(CMSG_WARNING, VERB_NORMAL,
                     "%s: Not a MIDI file!", current_filename);
       }
    }
    else if(memcmp(magic, "melo", 4) == 0)
    {
	readmidi_read_init();
	err = read_mfi_file(tf);
    }
    else
    {
	if(macbin_check && magic[0] == 0)
	{
	    /* Mac Binary */
	    macbin_check = 0;
	    skip(tf, 128 - 4);
	    goto retry_read;
	}
	else if(memcmp(magic, "RIFF", 4) == 0)
	{
	    /* RIFF MIDI file */
	    skip(tf, 20 - 4);
	    goto retry_read;
	}
	err = 1;
	ctl->cmsg(CMSG_WARNING, VERB_NORMAL,
		  "%s: Not a MIDI file!", current_filename);
    }
#else
    else{//SMF 以外
        err = 1;
    }
#endif
    if(err){
	    free_midi_list();
	    if(string_event_strtab.nstring > 0)
	        delete_string_table(&string_event_strtab);
	    return NULL;
    }
    /* Read WRD file */
#if 0
    if(!(play_mode->flag&PF_CAN_TRACE)){
	    if(wrdt->start != NULL)
	        wrdt->start(WRD_TRACE_NOTHING);
	    readmidi_wrd_mode = WRD_TRACE_NOTHING;
    }
    else if(wrdt->id != '-' && wrdt->opened){
	    readmidi_wrd_mode = import_wrd_file(fn);
	    if(wrdt->start != NULL)
	        if(wrdt->start(readmidi_wrd_mode) == -1){
		        /* strip all WRD events */
		        MidiEventList *e;
		        int32 i;
		        for(i = 0, e = evlist; i < event_count; i++, e = e->next)
		            if (e->event.type == ME_WRD || e->event.type == ME_SHERRY)
			        e->event.type = ME_NONE;
	        }
    }
    else{
	    readmidi_wrd_mode = WRD_TRACE_NOTHING;
    }
#else
	readmidi_wrd_mode = WRD_TRACE_NOTHING;
#endif
    /* make lyric table */
    if(string_event_strtab.nstring > 0){
	    string_event_table_size = string_event_strtab.nstring;
	    string_event_table = make_string_array(&string_event_strtab);
	    if(string_event_table == NULL){
	        delete_string_table(&string_event_strtab);
	        string_event_table_size = 0;
	    }
    }

  grooming:
    insert_note_steps();
    ev = groom_list(current_file_info->divisions, count, sp);
    if(ev == NULL){
	    free_midi_list();
	    if(string_event_strtab.nstring > 0)
	        delete_string_table(&string_event_strtab);
	    return NULL;
    }
    current_file_info->samples = *sp;
    if(current_file_info->first_text == NULL)
	    current_file_info->first_text = safe_strdup("");
    current_file_info->readflag = 1;
    return ev;
}
#endif // ここまで by Kobarin

struct midi_file_info *new_midi_file_info(const char *filename)
{
    struct midi_file_info *p;
    p = (struct midi_file_info *)safe_malloc(sizeof(struct midi_file_info));

    /* Initialize default members */
    memset(p, 0, sizeof(struct midi_file_info));
    p->hdrsiz = -1;
    p->format = -1;
    p->tracks = -1;
    p->divisions = -1;
    p->time_sig_n = p->time_sig_d = -1;
    p->samples = -1;
    p->max_channel = -1;
    p->file_type = IS_OTHER_FILE;
    if(filename != NULL)
	p->filename = safe_strdup(filename);
    COPY_CHANNELMASK(p->drumchannels, default_drumchannels);
    COPY_CHANNELMASK(p->drumchannel_mask, default_drumchannel_mask);

    /* Append to midi_file_info */
    p->next = midi_file_info;
    midi_file_info = p;

    return p;
}

void free_all_midi_file_info(void)
{
  struct midi_file_info *info, *next;

  info = midi_file_info;
  while (info) {
    next = info->next;
    safe_free(info->filename);
    safe_free(info->seq_name);
    safe_free(info->artist_name);	
    if (info->karaoke_title != NULL && info->karaoke_title == info->first_text)
      safe_free(info->karaoke_title);
    else {
      safe_free(info->karaoke_title);
      safe_free(info->first_text);
    }
    safe_free(info->midi_data);
    safe_free(info->pcm_filename); /* Note: this memory is freed in playmidi.c*/
    safe_free(info);
    info = next;
  }
  midi_file_info = NULL;
  current_file_info = NULL;

///r
	if(string_event_table != NULL)//added by Kobarin
	{
		safe_free(string_event_table[0]);
		safe_free(string_event_table);
		string_event_table = NULL;
		string_event_table_size = 0;
	}
}

struct midi_file_info *get_midi_file_info(char *filename, int newp)
{
    struct midi_file_info *p;

    filename = (char *)url_expand_home_dir(filename);
    /* Linear search */
    for(p = midi_file_info; p; p = p->next)
	if(!strcmp(filename, p->filename))
	    return p;
    if(newp)
	return new_midi_file_info(filename);
    return NULL;
}

struct timidity_file *open_midi_file(char *fn,
				     int decompress, int noise_mode)
{
    struct midi_file_info *infop;
    struct timidity_file *tf;
#if defined(SMFCONV) && defined(__W32__)
    extern int opt_rcpcv_dll;
#endif

    infop = get_midi_file_info(fn, 0);
    if(infop == NULL || infop->midi_data == NULL)
	tf = open_file(fn, decompress, noise_mode);
    else
    {
	tf = open_with_mem(infop->midi_data, infop->midi_data_size,
			   noise_mode);
	if(infop->compressed)
	{
	    if((tf->url = url_inflate_open(tf->url, infop->midi_data_size, 1))
	       == NULL)
	    {
		close_file(tf);
		return NULL;
	    }
	}
    }

#if defined(SMFCONV) && defined(__W32__)
    /* smf convert */
    if(tf != NULL && opt_rcpcv_dll)
    {
	if(smfconv_w32(tf, fn))
	{
	    close_file(tf);
	    return NULL;
	}
    }
#endif

    return tf;
}

#ifndef NO_MIDI_CACHE
static ptr_size_t deflate_url_reader(char *buf, ptr_size_t size, void *user_val)
{
    return url_nread((URL)user_val, buf, size);
}

/*
 * URL data into deflated buffer.
 */
static void url_make_file_data(URL url, struct midi_file_info *infop)
{
    char buff[BUFSIZ];
    MemBuffer b;
    long n;
    DeflateHandler compressor;

    init_memb(&b);

    /* url => b */
    if((compressor = open_deflate_handler(deflate_url_reader, url,
					  ARC_DEFLATE_LEVEL)) == NULL)
	return;
    while((n = zip_deflate(compressor, buff, sizeof(buff))) > 0)
	push_memb(&b, buff, n);
    close_deflate_handler(compressor);
    infop->compressed = 1;

    /* b => mem */
    infop->midi_data_size = b.total_size;
    rewind_memb(&b);
    infop->midi_data = (void *)safe_malloc(infop->midi_data_size);
    read_memb(&b, infop->midi_data, infop->midi_data_size);
    delete_memb(&b);
}

static int check_need_cache(URL url, char *filename)
{
    int t1, t2;
    t1 = url_check_type(filename);
    t2 = url->type;
    return (t1 == URL_http_t || t1 == URL_ftp_t || t1 == URL_news_t)
	 && t2 != URL_arc_t;
}
#else
/*ARGSUSED*/
static void url_make_file_data(URL url, struct midi_file_info *infop)
{
}
/*ARGSUSED*/
static int check_need_cache(URL url, char *filename)
{
    return 0;
}
#endif /* NO_MIDI_CACHE */

int check_midi_file(char *filename)
{
    struct midi_file_info *p;
    struct timidity_file *tf;
    char tmp[4];
    int32 len;
    int16 format;
    int check_cache;

    if(filename == NULL)
    {
	if(current_file_info == NULL)
	    return -1;
	filename = current_file_info->filename;
    }

    p = get_midi_file_info(filename, 0);
    if(p != NULL)
	return p->format;
    p = get_midi_file_info(filename, 1);

    if(get_module_type(filename) > 0)
    {
	p->format = 0;
	return 0;
    }

    tf = open_file(filename, 1, OF_SILENT);
    if(tf == NULL)
	return -1;

    check_cache = check_need_cache(tf->url, filename);
    if(check_cache)
    {
	if(!IS_URL_SEEK_SAFE(tf->url))
	{
	    if((tf->url = url_cache_open(tf->url, 1)) == NULL)
	    {
		close_file(tf);
		return -1;
	    }
	}
    }

    /* Parse MIDI header */
    if(tf_read(tmp, 1, 4, tf) != 4)
    {
	close_file(tf);
	return -1;
    }

    if(tmp[0] == 0)
    {
	skip(tf, 128 - 4);
	if(tf_read(tmp, 1, 4, tf) != 4)
	{
	    close_file(tf);
	    return -1;
	}
    }

    if(strncmp(tmp, "RCM-", 4) == 0 ||
       strncmp(tmp, "COME", 4) == 0 ||
       strncmp(tmp, "RIFF", 4) == 0 ||
       strncmp(tmp, "melo", 4) == 0 ||
       strncmp(tmp, "M1", 2) == 0)
    {
	p->format = format = 1;
	goto end_of_header;
    }

    if(strncmp(tmp, "MThd", 4) != 0)
    {
	close_file(tf);
	return -1;
    }

    if(tf_read(&len, 4, 1, tf) != 1)
    {
	close_file(tf);
	return -1;
    }
    len = BE_LONG(len);

    tf_read(&format, 2, 1, tf);
    format = BE_SHORT(format);
    if(format < 0 || format > 2)
    {
	close_file(tf);
	return -1;
    }
    skip(tf, len - 2);

    p->format = format;
    p->hdrsiz = (int16)tf_tell(tf);

  end_of_header:
    if(check_cache)
    {
	url_rewind(tf->url);
	url_cache_disable(tf->url);
	url_make_file_data(tf->url, p);
    }
    close_file(tf);
    return format;
}

static char *get_midi_title1(struct midi_file_info *p)
{
    char *s;

    if(p->format != 0 && p->format != 1)
	return NULL;

    if((s = p->seq_name) == NULL)
	if((s = p->karaoke_title) == NULL)
	    s = p->first_text;
    if(s != NULL)
    {
	int all_space, i;

	all_space = 1;
	for(i = 0; s[i]; i++)
	    if(s[i] != ' ')
	    {
		all_space = 0;
		break;
	    }
	if(all_space)
	    s = NULL;
    }
    return s;
}

#if !defined(KBTIM) && !defined(WINVSTI) //added by Kobarin
char *get_midi_title(char *filename)
{
    struct midi_file_info *p;
    struct timidity_file *tf;
    char tmp[4];
    int32 len;
    int16 format, tracks, trk;
    int laststatus, check_cache;
    int mtype;

    if(filename == NULL)
    {
	if(current_file_info == NULL)
	    return NULL;
	filename = current_file_info->filename;
    }

    p = get_midi_file_info(filename, 0);
    if(p == NULL)
	p = get_midi_file_info(filename, 1);
    else 
    {
	if(p->seq_name != NULL || p->first_text != NULL || p->format < 0)
	    return get_midi_title1(p);
    }

    tf = open_file(filename, 1, OF_SILENT);
    if(tf == NULL)
	return NULL;

    mtype = get_module_type(filename);
    check_cache = check_need_cache(tf->url, filename);
    if(check_cache || mtype > 0)
    {
	if(!IS_URL_SEEK_SAFE(tf->url))
	{
	    if((tf->url = url_cache_open(tf->url, 1)) == NULL)
	    {
		close_file(tf);
		return NULL;
	    }
	}
    }

    if(mtype > 0)
    {
	char *title, *str;

	title = get_module_title(tf, mtype);
	if(title == NULL)
	{
	    /* No title */
	    p->artist_name = NULL; ///r
	    p->seq_name = NULL;
	    p->format = 0;
	    goto end_of_parse;
	}

	len = (int32)strlen(title);
	len = SAFE_CONVERT_LENGTH(len);
	str = (char *)new_segment(&tmpbuffer, len);
	code_convert(title, str, len, NULL, NULL);
	p->seq_name = (char *)safe_strdup(str);
	reuse_mblock(&tmpbuffer);
	p->format = 0;
	safe_free (title);
	goto end_of_parse;
    }

    /* Parse MIDI header */
    if(tf_read(tmp, 1, 4, tf) != 4)
    {
	close_file(tf);
	return NULL;
    }

    if(tmp[0] == 0)
    {
	skip(tf, 128 - 4);
	if(tf_read(tmp, 1, 4, tf) != 4)
	{
	    close_file(tf);
	    return NULL;
	}
    }

    if(memcmp(tmp, "RCM-", 4) == 0 || memcmp(tmp, "COME", 4) == 0)
    {
	int i;
	char local[0x40 + 1];
	char *str;

	p->format = 1;
	skip(tf, 0x20 - 4);
	tf_read(local, 1, 0x40, tf);
	local[0x40]='\0';

	for(i = 0x40 - 1; i >= 0; i--)
	{
	    if(local[i] == 0x20)
		local[i] = '\0';
	    else if(local[i] != '\0')
		break;
	}

	i = SAFE_CONVERT_LENGTH(i + 1);
	str = (char *)new_segment(&tmpbuffer, i);
	code_convert(local, str, i, NULL, NULL);
	p->seq_name = (char *)safe_strdup(str);
	reuse_mblock(&tmpbuffer);
	p->format = 1;
	goto end_of_parse;
    }
    if(memcmp(tmp, "melo", 4) == 0)
    {
	size_t i;
	char *master, *converted;
	
	master = get_mfi_file_title(tf);
	if (master != NULL)
	{
	    i = SAFE_CONVERT_LENGTH(strlen(master) + 1);
	    converted = (char *)new_segment(&tmpbuffer, i);
	    code_convert(master, converted, i, NULL, NULL);
	    p->seq_name = (char *)safe_strdup(converted);
	    reuse_mblock(&tmpbuffer);
	}
	else
	{
	    p->seq_name = (char *)safe_malloc(1);
	    p->seq_name[0] = '\0';
	}
	p->format = 0;
	goto end_of_parse;
    }

    if(strncmp(tmp, "M1", 2) == 0)
    {
	/* I don't know MPC file format */
	p->format = 1;
	goto end_of_parse;
    }

	  if(strncmp(tmp, "RIFF", 4) == 0)
	  {
	/* RIFF MIDI file */
	skip(tf, 20 - 4);
  if(tf_read(tmp, 1, 4, tf) != 4)
    {
	close_file(tf);
	return NULL;
    }
	  }

    if(strncmp(tmp, "MThd", 4) != 0)
    {
	close_file(tf);
	return NULL;
    }

    if(tf_read(&len, 4, 1, tf) != 1)
    {
	close_file(tf);
	return NULL;
    }

    len = BE_LONG(len);

    tf_read(&format, 2, 1, tf);
    tf_read(&tracks, 2, 1, tf);
    format = BE_SHORT(format);
    tracks = BE_SHORT(tracks);
    p->format = format;
    p->tracks = tracks;
    if(format < 0 || format > 2)
    {
	p->format = -1;
	close_file(tf);
	return NULL;
    }

    skip(tf, len - 4);
    p->hdrsiz = (int16)tf_tell(tf);

    if(format == 2)
	goto end_of_parse;

    if(tracks >= 3)
    {
	tracks = 3;
	karaoke_format = 0;
    }
    else
    {
	tracks = 1;
	karaoke_format = -1;
    }

    for(trk = 0; trk < tracks; trk++)
    {
	size_t next_pos, pos;

	if(trk >= 1 && karaoke_format == -1)
	    break;

	if((tf_read(tmp,1,4,tf) != 4) || (tf_read(&len,4,1,tf) != 1)) 
	    break;

	if(memcmp(tmp, "MTrk", 4))
	    break;

	next_pos = tf_tell(tf) + len;
	laststatus = -1;
	for(;;)
	{
	    int i, me, type;

	    /* skip Variable-length quantity */
	    do
	    {
		if((i = tf_getc(tf)) == EOF)
		    goto end_of_parse;
	    } while (i & 0x80);

	    if((me = tf_getc(tf)) == EOF)
		goto end_of_parse;

	    if(me == 0xF0 || me == 0xF7) /* SysEx */
	    {
		if((len = getvl(tf)) < 0)
		    goto end_of_parse;
		if((p->mid == 0 || p->mid >= 0x7e) && len > 0 && me == 0xF0)
		{
		    p->mid = tf_getc(tf);
		    len--;
		}
		skip(tf, len);
	    }
	    else if(me == 0xFF) /* Meta */
	    {
		type = tf_getc(tf);
		if((len = getvl(tf)) < 0)
		    goto end_of_parse;
///r
		if((type == 1 || type == 2 || type == 3) && len > 0 &&
		   (trk == 0 || karaoke_format != -1))
		{
		    char *si, *so;
		    int s_maxlen = SAFE_CONVERT_LENGTH(len);

		    si = (char *)new_segment(&tmpbuffer, len + 1);
		    so = (char *)new_segment(&tmpbuffer, s_maxlen);

		    if(len != tf_read(si, 1, len, tf))
		    {
			reuse_mblock(&tmpbuffer);
			goto end_of_parse;
		    }

		    si[len]='\0';
		    code_convert(si, so, s_maxlen, NULL, NULL);
///r
		    if(trk == 0 && type == 2)
		    {
		      if(p->artist_name == NULL) {
			char *name = safe_strdup(so);
			p->artist_name = safe_strdup(fix_string(name));
			safe_free(name);
		      }
		      reuse_mblock(&tmpbuffer);
			  continue;
		    }
		    if(trk == 0 && type == 3)
		    {
		      if(p->seq_name == NULL) {
			char *name = safe_strdup(so);
			p->seq_name = safe_strdup(fix_string(name));
			safe_free(name);
		      }
		      reuse_mblock(&tmpbuffer);
		      if(karaoke_format == -1)
			goto end_of_parse;
		    }
		    if(p->first_text == NULL) {
		      char *name;
		      name = safe_strdup(so);
		      p->first_text = safe_strdup(fix_string(name));
		      safe_free(name);
		    }
		    if(karaoke_format != -1)
		    {
			if(trk == 1 && strncmp(si, "@K", 2) == 0)
			    karaoke_format = 1;
			else if(karaoke_format == 1 && trk == 2)
			    karaoke_format = 2;
		    }
		    if(type == 1 && karaoke_format == 2)
		    {
			if(strncmp(si, "@T", 2) == 0)
			    p->karaoke_title =
				add_karaoke_title(p->karaoke_title, si + 2);
			else if(si[0] == '\\')
			    goto end_of_parse;
		    }
		    reuse_mblock(&tmpbuffer);
		}
		else if(type == 0x2F)
		{
		    pos = tf_tell(tf);
		    if(pos < next_pos)
///r
#ifdef _WIN64
			tf_seek_uint64(tf, next_pos - pos, SEEK_CUR);
#else
			tf_seek(tf, next_pos - pos, SEEK_CUR);
#endif

		    break; /* End of track */
		}
		else
		    skip(tf, len);
	    }
	    else /* MIDI event */
	    {
		/* skip MIDI event */
		karaoke_format = -1;
		if(trk != 0)
		    goto end_of_parse;

		if(me & 0x80) /* status byte */
		{
		    laststatus = (me >> 4) & 0x07;
		    if(laststatus != 7)
			tf_getc(tf);
		}

		switch(laststatus)
		{
		  case 0: case 1: case 2: case 3: case 6:
		    tf_getc(tf);
		    break;
		  case 7:
		    if(!(me & 0x80))
			break;
		    switch(me & 0x0F)
		    {
		      case 2:
			tf_getc(tf);
			tf_getc(tf);
			break;
		      case 3:
			tf_getc(tf);
			break;
		    }
		    break;
		}
	    }
	}
    }

  end_of_parse:
    if(check_cache)
    {
	url_rewind(tf->url);
	url_cache_disable(tf->url);
	url_make_file_data(tf->url, p);
    }
    close_file(tf);
    if(p->first_text == NULL)
	p->first_text = safe_strdup("");
    return get_midi_title1(p);
}
#else //KBTIM //WINVSTI by Kobarin

char *get_midi_title(char *filename)
{
    return NULL;
}

#endif //ここまで by Kobarin

int midi_file_save_as(char *in_name, char *out_name)
{
    struct timidity_file *tf;
    FILE* ofp;
    char buff[BUFSIZ];
    long n;

    if(in_name == NULL)
    {
	if(current_file_info == NULL)
	    return 0;
	in_name = current_file_info->filename;
    }
    out_name = (char *)url_expand_home_dir(out_name);

    ctl->cmsg(CMSG_INFO, VERB_NORMAL, "Save as %s...", out_name);

    _set_errno(0);
    if((tf = open_midi_file(in_name, 1, 0)) == NULL)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "%s: %s", out_name,
		  errno ? strerror(errno) : "Can't save file");
	return -1;
    }

    _set_errno(0);
    if((ofp = fopen(out_name, "wb")) == NULL)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "%s: %s", out_name,
		  errno ? strerror(errno) : "Can't save file");
	close_file(tf);
	return -1;
    }

    while((n = tf_read(buff, 1, sizeof(buff), tf)) > 0) {
	size_t dummy = fwrite(buff, 1, n, ofp); ++dummy;
	}
    ctl->cmsg(CMSG_INFO, VERB_NORMAL, "Save as %s...Done", out_name);

    fclose(ofp);
    close_file(tf);
    return 0;
}

char *event2string(int id)
{
    if(id == 0)
	return "";
#ifdef ABORT_AT_FATAL
    if(id >= string_event_table_size)
	abort();
#endif /* ABORT_AT_FATAL */
    if(string_event_table == NULL || id < 0 || id >= string_event_table_size)
	return NULL;
    return string_event_table[id];
}

/*! initialize Delay Effect (GS) */
void init_delay_status_gs(void)
{
	struct delay_status_gs_t *p = &delay_status_gs;
	p->type = 0;
	p->level = 0x40;
	p->level_center = 0x7F;
	p->level_left = 0;
	p->level_right = 0;
	p->time_c = 0x61;
	p->time_l = 0x01;
	p->time_r = 0x01;
	p->feedback = 0x50;
	p->pre_lpf = 0;
	recompute_delay_status_gs();
	init_ch_delay();
}

/*! recompute Delay Effect (GS) */
extern void init_pre_lpf(InfoPreFilter *info);
void recompute_delay_status_gs(void)
{
	struct delay_status_gs_t *p = &delay_status_gs;

#if 0 // move reverb.c setup_ch_delay()
	p->time_center = delay_time_center_table[p->time_c > 0x73 ? 0x73 : p->time_c];
	p->time_ratio_left = (double)p->time_l / 24.0;
	p->time_ratio_right = (double)p->time_r / 24.0;
	p->sample[0] = p->time_center * (double)play_mode->rate * DIV_1000;
	p->sample[1] = (double)p->sample[0] * p->time_ratio_left;
	p->sample[2] = (double)p->sample[0] * p->time_ratio_right;
	if (p->sample[1] > play_mode->rate)
		p->sample[1] = play_mode->rate;
	if (p->sample[2] > play_mode->rate)
		p->sample[2] = play_mode->rate;

	//elion
	// delay lv が 127 のものをすべて不正の値とみなす
///r
	if(p->level_center >= 127)
		p->level_center = 0; //?

	p->level_ratio[0] = p->level * p->level_center / (127.0f * 127.0f);
	p->level_ratio[1] = p->level * p->level_left / (127.0f * 127.0f);
	p->level_ratio[2] = p->level * p->level_right / (127.0f * 127.0f);
	p->feedback_ratio = (double)(p->feedback - 64) * (0.763f * 2.0f * DIV_100);
	p->send_reverb_ratio = (double)p->send_reverb * (0.787f * DIV_100);	

	if((p->level_left != 0 || p->level_right != 0) && p->type == 0) {//elionadd
		p->type = 1;	/* it needs 3-tap delay effect. */
	}
#endif

///r
	if(p->pre_lpf) {
		p->lpf.freqL = (double)(7 - p->pre_lpf) * DIV_7 * 16000.0f + 200.0f ;
		init_pre_lpf(&p->lpf);
	}
}

/*! Delay Macro (GS) */
void set_delay_macro_gs(int macro)
{
	struct delay_status_gs_t *p = &delay_status_gs;
//	if(macro >= 4) {p->type = 2;}	/* cross delay */
	macro *= 10;
	//p->time_center = delay_time_center_table[delay_macro_presets[macro + 1]];
	//p->time_ratio_left = (double)delay_macro_presets[macro + 2] / 24;
	//p->time_ratio_right = (double)delay_macro_presets[macro + 3] / 24;	
	p->time_c = delay_macro_presets[macro + 1];
	p->time_l = delay_macro_presets[macro + 2];
	p->time_r = delay_macro_presets[macro + 3];
	p->level_center = delay_macro_presets[macro + 4];
	p->level_left = delay_macro_presets[macro + 5];
	p->level_right = delay_macro_presets[macro + 6];
	p->level = delay_macro_presets[macro + 7];
	p->feedback = delay_macro_presets[macro + 8];
}

/*! initialize Reverb Effect (GS) */
void init_reverb_status_gs(void)
{
	struct reverb_status_gs_t *p = &reverb_status_gs;

	p->system_mode = 0;
	p->character = 0x04;
	p->pre_lpf = 0;
	p->level = 0x40;
	p->time = 0x40;
	p->delay_feedback = 0;
	p->pre_delay_time = 0;
	recompute_reverb_status_gs();
	init_ch_reverb();
}

/*! recompute Reverb Effect (GS) */
void recompute_reverb_status_gs(void)
{
	struct reverb_status_gs_t *p = &reverb_status_gs;
///r
	if(play_system_mode == SD_SYSTEM_MODE)
		p->system_mode = 1;
	else if(play_system_mode == GM2_SYSTEM_MODE)
		p->system_mode = 1;
	else
		p->system_mode = 0;

	if(p->pre_lpf) {
		p->lpf.freqL = (double)(7 - p->pre_lpf) * DIV_7 * 16000.0f + 200.0f ;		
		init_pre_lpf(&p->lpf);
	}
}

/*! Reverb Type (GM2) */
void set_reverb_macro_gm2(int macro)
{
	struct reverb_status_gs_t *p = &reverb_status_gs;
	int type = macro;

	p->system_mode = 1;
	if (macro == 8) {macro = 5;}
	macro *= 6;
	p->character = reverb_macro_presets[macro];
	p->pre_lpf = reverb_macro_presets[macro + 1];
	p->level = reverb_macro_presets[macro + 2];
	p->time = reverb_macro_presets[macro + 3];
	p->delay_feedback = reverb_macro_presets[macro + 4];
	p->pre_delay_time = reverb_macro_presets[macro + 5];

	switch(type) {	/* override GS macro's parameter */
	case 0:	/* Small Room */
		p->time = 44;
		break;
	case 1:	/* Medium Room */
	case 8:	/* Plate */
		p->time = 50;
		break;
	case 2:	/* Large Room */
		p->time = 56;
		break;
	case 3:	/* Medium Hall */
	case 4:	/* Large Hall */
		p->time = 64;
		break;
	}
}

/*! Reverb Macro (GS) */
void set_reverb_macro_gs(int macro)
{
	struct reverb_status_gs_t *p = &reverb_status_gs;
	macro *= 6;
	p->character = reverb_macro_presets[macro];
	p->pre_lpf = reverb_macro_presets[macro + 1];
	p->level = reverb_macro_presets[macro + 2];
	p->time = reverb_macro_presets[macro + 3];
	p->delay_feedback = reverb_macro_presets[macro + 4];
	p->pre_delay_time = reverb_macro_presets[macro + 5];
}

/*! initialize Chorus Effect (GS) */
void init_chorus_status_gs(void)
{
	struct chorus_status_gs_t *p = &chorus_status_gs;
#ifdef CUSTOMIZE_CHORUS_PARAM
	p->macro = 0x02;
	p->pre_lpf = otd.chorus_param.pre_lpf;
	p->level = otd.chorus_param.level;
	p->feedback = otd.chorus_param.feedback;
	p->delay = otd.chorus_param.delay;
	p->rate = otd.chorus_param.rate;
	p->depth = otd.chorus_param.depth;
	p->send_reverb = otd.chorus_param.send_reverb;
	p->send_delay = otd.chorus_param.send_delay;
	recompute_chorus_status_gs();
#else
	p->macro = 0;
	p->pre_lpf = 0;
	p->level = 0x40;
	p->feedback = 0x08;
	p->delay = 0x50;
	p->rate = 0x03;
	p->depth = 0x13;
	p->send_reverb = 0;
	p->send_delay = 0;
	recompute_chorus_status_gs();
#endif
	init_ch_chorus();
}

/*! recompute Chorus Effect (GS) */
void recompute_chorus_status_gs(void)
{
	struct chorus_status_gs_t *p = &chorus_status_gs;
///r
	if(play_system_mode == SD_SYSTEM_MODE)
		p->system_mode = 1;
	else if(play_system_mode == GM2_SYSTEM_MODE)
		p->system_mode = 1;
	else
		p->system_mode = 0;

	if(p->pre_lpf) {
		p->lpf.freqL = (double)(7 - p->pre_lpf) * DIV_7 * 16000.0f + 200.0f ;		
		init_pre_lpf(&p->lpf);
	}
}

/*! Chorus Macro (GS) */
void set_chorus_macro_gs(int macro)
{
	struct chorus_status_gs_t *p = &chorus_status_gs;

	p->system_mode = 0; // GS
	macro *= 8;
	p->pre_lpf = chorus_macro_presets[macro];
	p->level = chorus_macro_presets[macro + 1];
	p->feedback = chorus_macro_presets[macro + 2];
	p->delay = chorus_macro_presets[macro + 3];
	p->rate = chorus_macro_presets[macro + 4];
	p->depth = chorus_macro_presets[macro + 5];
	p->send_reverb = chorus_macro_presets[macro + 6];
	p->send_delay = chorus_macro_presets[macro + 7];
}

/*! Chorus Type (GM2) */
void set_chorus_macro_gm2(int macro)
{
	struct chorus_status_gs_t *p = &chorus_status_gs;

	p->system_mode = 1; // GM2
	macro *= 8;
	p->pre_lpf = chorus_macro_presets[macro];
	p->level = chorus_macro_presets[macro + 1];
	p->feedback = chorus_macro_presets[macro + 2];
	p->delay = chorus_macro_presets[macro + 3];
	p->rate = chorus_macro_presets[macro + 4];
	p->depth = chorus_macro_presets[macro + 5];
	p->send_reverb = chorus_macro_presets[macro + 6];
	p->send_delay = chorus_macro_presets[macro + 7];
}


/*! initialize EQ (GS) */
void init_eq_status_gs(void)
{
	struct eq_status_gs_t *p = &eq_status_gs;
	p->low_freq = 0;
	p->low_gain = 0x40;
	p->high_freq = 0;
	p->high_gain = 0x40;	
	memset(&(p->lsf), 0, sizeof(FilterCoefficients));
	memset(&(p->hsf), 0, sizeof(FilterCoefficients));
	recompute_eq_status_gs();
}

///r
// SC-88 spec max 12.0 dB 0x34~0x4C
static int clip_eq_gain(int in)
{
	if(in < - 12)
		return -12;
	else if(in > 12)
		return 12;
	else 
		return in;
}

/*! recompute EQ (GS) */
void recompute_eq_status_gs(void)
{
	struct eq_status_gs_t *p = &eq_status_gs;

	/* Lowpass Shelving Filter */
	if(p->low_gain != 0x40)
		init_sample_filter2(&(p->lsf), p->low_freq == 0 ? 200 : 400, clip_eq_gain(p->low_gain - 0x40), 0, FILTER_SHELVING_LOW);
	else
		init_sample_filter2(&(p->lsf), 0, 0, 0, FILTER_NONE);
	/* Highpass Shelving Filter */	
	if(p->high_gain != 0x40)
		init_sample_filter2(&(p->hsf), p->high_freq == 0 ? 3000 : 6000, clip_eq_gain(p->high_gain - 0x40), 0, FILTER_SHELVING_HI);
	else
		init_sample_filter2(&(p->hsf), 0, 0, 0, FILTER_NONE);
}

/*! initialize Multi EQ (XG) */
void init_multi_eq_xg(void)
{
	multi_eq_xg.valid = 0;
	set_multi_eq_type_xg(0);
	recompute_multi_eq_xg();
}

/*! set Multi EQ type (XG) */
void set_multi_eq_type_xg(int type)
{
	struct multi_eq_xg_t *p = &multi_eq_xg;
	type *= 20;
	p->gain1 = multi_eq_block_table_xg[type];
	p->freq1 = multi_eq_block_table_xg[type + 1];
	p->q1 = multi_eq_block_table_xg[type + 2];
	p->shape1 = multi_eq_block_table_xg[type + 3];
	p->gain2 = multi_eq_block_table_xg[type + 4];
	p->freq2 = multi_eq_block_table_xg[type + 5];
	p->q2 = multi_eq_block_table_xg[type + 6];
	p->gain3 = multi_eq_block_table_xg[type + 8];
	p->freq3 = multi_eq_block_table_xg[type + 9];
	p->q3 = multi_eq_block_table_xg[type + 10];
	p->gain4 = multi_eq_block_table_xg[type + 12];
	p->freq4 = multi_eq_block_table_xg[type + 13];
	p->q4 = multi_eq_block_table_xg[type + 14];
	p->gain5 = multi_eq_block_table_xg[type + 16];
	p->freq5 = multi_eq_block_table_xg[type + 17];
	p->q5 = multi_eq_block_table_xg[type + 18];
	p->shape5 = multi_eq_block_table_xg[type + 19];		
}

static FLOAT_T clip_eq_q(int in)
{
	if(in < 1)
		in = 1;
	else if(in > 120)
		in = 120;	
	return (FLOAT_T)in * DIV_10; // 0.1 ~ 12.0
}

/*! recompute Multi EQ (XG) */
void recompute_multi_eq_xg(void)
{
	struct multi_eq_xg_t *p = &multi_eq_xg;
	FLOAT_T freq, gain, q;
	int8 shape, valid = 0;
	
	if(p->freq1 != 0 && p->freq1 < 60 && p->gain1 != 0x40) {
		++valid;
		p->valid1 = 1;
		if     (p->freq1 < 0x04) p->freq1 = 0x04;
		else if(p->freq1 > 0x28) p->freq1 = 0x28;
		freq = eq_freq_table_xg[p->freq1];
		gain = clip_eq_gain(p->gain1 - 0x40);
		q = clip_eq_q(p->q1);
		shape = p->shape1 ? FILTER_PEAKING : FILTER_SHELVING_LOW;
		init_sample_filter2(&p->eq1, freq, gain, q, shape);
	} else {
		p->valid1 = 0;
		init_sample_filter2(&p->eq1, 0, 0, 0, FILTER_NONE);
	}
	if(p->freq2 != 0 && p->freq2 < 60 && p->gain2 != 0x40) {
		++valid;
		p->valid2 = 1;
		if     (p->freq2 < 0x0E) p->freq2 = 0x0E;
		else if(p->freq2 > 0x36) p->freq2 = 0x36;
		freq = eq_freq_table_xg[p->freq2];
		gain = clip_eq_gain(p->gain2 - 0x40);
		q = clip_eq_q(p->q2);
		init_sample_filter2(&p->eq2, freq, gain, q, FILTER_PEAKING);
	} else {
		p->valid2 = 0;
		init_sample_filter2(&p->eq2, 0, 0, 0, FILTER_NONE);
	}
	if(p->freq3 != 0 && p->freq3 < 60 && p->gain3 != 0x40) {
		++valid;
		p->valid3 = 1;
		if     (p->freq3 < 0x0E) p->freq3 = 0x0E;
		else if(p->freq3 > 0x36) p->freq3 = 0x36;
		freq = eq_freq_table_xg[p->freq3];
		gain = clip_eq_gain(p->gain3 - 0x40);
		q = clip_eq_q(p->q3);
		init_sample_filter2(&p->eq3, freq, gain, q, FILTER_PEAKING);
	} else {
		p->valid3 = 0;
		init_sample_filter2(&p->eq3, 0, 0, 0, FILTER_NONE);
	}
	if(p->freq4 != 0 && p->freq4 < 60 && p->gain4 != 0x40) {
		++valid;
		p->valid4 = 1;
		if     (p->freq4 < 0x0E) p->freq4 = 0x0E;
		else if(p->freq4 > 0x36) p->freq4 = 0x36;
		freq = eq_freq_table_xg[p->freq4];
		gain = clip_eq_gain(p->gain4 - 0x40);
		q = clip_eq_q(p->q4);
		init_sample_filter2(&p->eq4, freq, gain, q, FILTER_PEAKING);
	} else {
		p->valid4 = 0;
		init_sample_filter2(&p->eq4, 0, 0, 0, FILTER_NONE);
	}
	if(p->freq5 != 0 && p->freq5 < 60 && p->gain5 != 0x40) {
		++valid;
		p->valid5 = 1;
		if     (p->freq5 < 0x1C) p->freq5 = 0x1C;
		else if(p->freq5 > 0x3A) p->freq5 = 0x3A;
		freq = eq_freq_table_xg[p->freq5];
		gain = clip_eq_gain(p->gain5 - 0x40);
		q = clip_eq_q(p->q5);
		shape = p->shape5 ? FILTER_PEAKING : FILTER_SHELVING_HI;
		init_sample_filter2(&p->eq5, freq, gain, q, shape);
	} else {
		p->valid5 = 0;
		init_sample_filter2(&p->eq5, 0, 0, 0, FILTER_NONE);
	}
	p->valid = valid ? 1 : 0;
}


// SD-90 spec max 15.0 dB 0x00~0x1E
static int clip_eq_gain_sd(int in)
{
	in -= 15;
	if(in < - 15)
		return -15;
	else if(in > 15)
		return 15;
	else 
		return in;
}

/*! recompute Multi EQ (SD(GM2)) */
void recompute_multi_eq_sd(void)
{
	struct multi_eq_sd_t *p = &multi_eq_sd;
	int valid_ll, valid_lr, valid_hl, valid_hr;

	if(p->gain_ll != 0x40) {
		valid_ll = 1;
		init_sample_filter2(&p->eq_ll, p->freq_ll == 0 ? 200 : 400, clip_eq_gain_sd(p->gain_ll), 0, FILTER_SHELVING_LOW);
	} else {
		valid_ll = 0;
		init_sample_filter2(&p->eq_ll, 0, 0, 0, FILTER_NONE);
	}
	if(p->gain_lr != 0x40) {
		valid_lr = 1;
		init_sample_filter2(&p->eq_lr, p->freq_lr == 0 ? 200 : 400, clip_eq_gain_sd(p->gain_lr), 0, FILTER_SHELVING_LOW);
	} else {
		valid_lr = 0;
		init_sample_filter2(&p->eq_lr, 0, 0, 0, FILTER_NONE);
	}
	if(p->gain_hl != 0x40) {
		valid_hl = 1;
		init_sample_filter2(&p->eq_hl, p->freq_hl == 0 ? 2000 : p->freq_hl == 1 ? 4000 : 8000, clip_eq_gain_sd(p->gain_hl), 0, FILTER_SHELVING_HI);
	} else {
		valid_hl = 0;
		init_sample_filter2(&p->eq_hl, 0, 0, 0, FILTER_NONE);
	}
	if(p->gain_hr != 0x40) {
		valid_hr = 1;
		init_sample_filter2(&p->eq_hr, p->freq_hl == 0 ? 2000 : p->freq_hl == 1 ? 4000 : 8000, clip_eq_gain_sd(p->gain_hl), 0, FILTER_SHELVING_HI);
	} else {
		valid_hr = 0;
		init_sample_filter2(&p->eq_hl, 0, 0, 0, FILTER_NONE);
	}
	p->valid = p->sw && (valid_ll || valid_lr || valid_hl || valid_hr);
}

/*! initialize Multi EQ (SD(GM2)) */
void init_multi_eq_sd(void)
{
	struct multi_eq_sd_t *p = &multi_eq_sd;

	p->sw = 1; // on 
	p->freq_ll = 0x00; 
	p->gain_ll = 0x0F;
	p->freq_hl = 0x00; 
	p->gain_hl = 0x0F;
	p->freq_lr = 0x00; 
	p->gain_lr = 0x0F;	
	p->freq_hr = 0x00; 
	p->gain_hr = 0x0F;

	multi_eq_sd.valid = 0;
	recompute_multi_eq_sd();
}


/*! convert GS user drumset assign groups to internal "alternate assign". */
void recompute_userdrum_altassign(int bank, int group)
{
	int number = 0, i;
	char *params[131], param[10];
	ToneBank *bk;
	UserDrumset *p;
	
	for(p = userdrum_first; p != NULL; p = p->next) {
		if(p->assign_group == group) {
			sprintf(param, "%d", p->prog);
			params[number] = safe_strdup(param);
			number++;
		}
	}
	params[number] = NULL;

	alloc_instrument_bank(1, bank);
	bk = drumset[bank];
	bk->alt = add_altassign_string(bk->alt, params, number);
	for (i = number - 1; i >= 0; i--)
		safe_free(params[i]);
}

/*! initialize GS user drumset. */
void init_userdrum()
{
	int i;
	AlternateAssign *alt;

///r
	free_userdrum();
//	free_userdrum2(); // error altassign


	for(i=0;i<2;i++) {	/* allocate alternative assign */
		alt = (AlternateAssign *)safe_malloc(sizeof(AlternateAssign));
		memset(alt, 0, sizeof(AlternateAssign));
		alloc_instrument_bank(1, 64 + i);
		drumset[64 + i]->alt = alt;
	}
}

/*! recompute GS user drumset. */
Instrument *recompute_userdrum(int bank, int prog, int elm)
{
	UserDrumset *p;
	Instrument *ip = NULL;

	p = get_userdrum(bank, prog);
	if(drumset[bank]->tone[prog][elm])
		free_tone_bank_element(drumset[bank]->tone[prog][elm]);
	if(drumset[p->source_prog]) {
		ToneBankElement *source_tone;
		source_tone = drumset[p->source_prog]->tone[p->source_note][elm];
		if(source_tone){
			if(source_tone->name == NULL /* NULL if "soundfont" directive is used */
				  && source_tone->instrument == NULL) {
				if((ip = load_instrument(1, p->source_prog, p->source_note, elm)) == NULL) {
					ip = MAGIC_ERROR_INSTRUMENT;
				}
				source_tone->instrument = ip;
			}
			if(source_tone->name) {
				if(drumset[bank]->tone[prog][elm] == NULL){
					if(alloc_tone_bank_element(&drumset[bank]->tone[prog][elm])){
						ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "recompute_userdrum: ToneBankElement malloc error" "%d:%d:%d", bank, prog, elm);
						return NULL;
					}
				}
				copy_tone_bank_element(drumset[bank]->tone[prog][elm], source_tone);
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"User Drumset (%d %d -> %d %d)", p->source_prog, p->source_note, bank, prog);
				return ip;
			}
		}
		source_tone = drumset[0]->tone[p->source_note][elm];
		if(source_tone){
			if(source_tone->name) {
				if(drumset[bank]->tone[prog][elm] == NULL){
					if(alloc_tone_bank_element(&drumset[bank]->tone[prog][elm])){
						ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "recompute_userdrum: ToneBankElement malloc error" "%d:%d:%d", bank, prog, elm);
						return NULL;
					}
				}
				copy_tone_bank_element(drumset[bank]->tone[prog][elm], source_tone);
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"User Drumset (%d %d -> %d %d)", 0, p->source_note, bank, prog);
				return ip;
			}
		}
		ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "Referring user drum set %d, note %d not found - this instrument will not be heard as expected", bank, prog);
	}
	return ip;
}

/*! get pointer to requested GS user drumset.
   if it's not found, allocate a new item first. */
UserDrumset *get_userdrum(int bank, int prog)
{
	UserDrumset *p;

	for(p = userdrum_first; p != NULL; p = p->next) {
		if(p->bank == bank && p->prog == prog) {return p;}
	}

	p = (UserDrumset *)safe_malloc(sizeof(UserDrumset));
	memset(p, 0, sizeof(UserDrumset));
	p->next = NULL;
	if(userdrum_first == NULL) {
		userdrum_first = p;
		userdrum_last = p;
	} else {
		userdrum_last->next = p;
		userdrum_last = p;
	}
	p->bank = bank;
	p->prog = prog;

	return p;
}

/*! free GS user drumset. */
void free_userdrum()
{
	UserDrumset *p, *next;

///r
    //added by Kobarin
    int i;
    for(i = 0; i < 2; i++){
        if(drumset[64+i] && drumset[64+i]->alt){
            struct _AlternateAssign *alt = drumset[64+i]->alt;
            struct _AlternateAssign *del=alt;
            while(del){
                alt=del->next;
                safe_free(del);
                del=alt;
            }
            drumset[64+i]->alt = NULL;
        }
    }
    //ここまで

	for(p = userdrum_first; p != NULL; p = next){
		next = p->next;
		safe_free(p);
    }
	userdrum_first = userdrum_last = NULL;
}

///r
//added by Kobarin
void free_userdrum2()
{
    int i;
    for(i = 0; i < 128 + MAP_BANK_COUNT; i++){
        if(drumset[i] && drumset[i]->alt){
            struct _AlternateAssign *alt = drumset[i]->alt;
            struct _AlternateAssign *del=alt;
            while(del){
                alt=del->next;
                safe_free(del);
                del=alt;
            }
            drumset[i]->alt = NULL;
        }
    }
}

/*! initialize GS user instrument. */
void init_userinst()
{
	free_userinst();
}

///r
/*! recompute GS user instrument. */
void recompute_userinst(int bank, int prog, int elm)
{
	UserInstrument *p;

	p = get_userinst(bank, prog);
	if(tonebank[bank]->tone[prog][elm])		
		free_tone_bank_element(tonebank[bank]->tone[prog][elm]);
	if(tonebank[p->source_bank]) {
		if(tonebank[p->source_bank]->tone[p->source_prog][elm]){
			if(tonebank[p->source_bank]->tone[p->source_prog][elm]->name) {
				if(tonebank[bank]->tone[prog][elm] == NULL){		
					if(alloc_tone_bank_element(&tonebank[bank]->tone[prog][elm])){
						ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "recompute_userinst: ToneBankElement malloc error." "%d:%d:%d", bank, prog, elm);
						return;
					}
				}
				copy_tone_bank_element(tonebank[bank]->tone[prog][elm], tonebank[p->source_bank]->tone[p->source_prog][elm]);
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"User Instrument (%d %d -> %d %d)", p->source_bank, p->source_prog, bank, prog);
				return;
			}
		}
		if(tonebank[0]->tone[p->source_prog][elm]){
			if(tonebank[0]->tone[p->source_prog][elm]->name) {			
				if(tonebank[bank]->tone[prog][elm] == NULL){		
					if(alloc_tone_bank_element(&tonebank[bank]->tone[prog][elm])){
						ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "recompute_userinst: ToneBankElement malloc error." "%d:%d:%d", bank, prog, elm);
						return;
					}
				}
				copy_tone_bank_element(tonebank[bank]->tone[prog][elm], tonebank[0]->tone[p->source_prog][elm]);
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"User Instrument (%d %d -> %d %d)", 0, p->source_prog, bank, prog);
				return;
			}
		}		
	}
}

/*! get pointer to requested GS user instrument.
   if it's not found, allocate a new item first. */
UserInstrument *get_userinst(int bank, int prog)
{
	UserInstrument *p;

	for(p = userinst_first; p != NULL; p = p->next) {
		if(p->bank == bank && p->prog == prog) {return p;}
	}

	p = (UserInstrument *)safe_malloc(sizeof(UserInstrument));
	memset(p, 0, sizeof(UserInstrument));
	p->next = NULL;
	if(userinst_first == NULL) {
		userinst_first = p;
		userinst_last = p;
	} else {
		userinst_last->next = p;
		userinst_last = p;
	}
	p->bank = bank;
	p->prog = prog;

	p->source_bank = 0;
	p->source_prog = prog;
	return p;
}

/*! free GS user instrument. */
void free_userinst()
{
	UserInstrument *p, *next;

	for(p = userinst_first; p != NULL; p = next){
		next = p->next;
		safe_free(p);
    }
	userinst_first = userinst_last = NULL;
}



/*! recompute XG effect parameters. */
void recompute_effect_xg(struct effect_xg_t *st, int marge)
{
	EffectList *efc = st->ef;
	int j, flg = 0;

	calc_send_return_xg(st);
	if (efc == NULL) {return;}
	if(marge){
		for (j = 0; j < 16; j++) {
			st->param_lsb[j] = st->set_param_lsb[j];
		}
		for (j = 0; j < 10; j++) {
			st->param_msb[j] = st->set_param_msb[j];
		}
	}
	while (efc != NULL && efc->info != NULL)
	{
		(*efc->engine->conv_xg)(st, efc);
		(*efc->engine->do_effect)(NULL, MAGIC_INIT_EFFECT_INFO, efc);
		efc = efc->next_ef;
	}
}

/*! MIDI control XG variation/insertion effect parameters. */
// ctrl_type see effect.h control_var_effect_xg[]
void control_effect_xg(int ch)
{
	int i, tmp;

	if(play_system_mode != XG_SYSTEM_MODE || !opt_insertion_effect)
		return;
	for (i = 0; i < XG_VARIATION_EFFECT_NUM; i++) {
		struct effect_xg_t *st = &variation_effect_xg[i];
		if(ch != st->part) {continue;} // only insertion mode
		tmp = st->set_param_lsb[st->control_param];
		tmp += (FLOAT_T)channel[ch].mod.val * (FLOAT_T)(st->control_depth[0] - 0x40) * DIV_64 + 0.5;
		tmp += (FLOAT_T)channel[ch].bend.val * (FLOAT_T)(st->control_depth[1] - 0x40) * DIV_64 + 0.5;
		tmp += (FLOAT_T)channel[ch].caf.val * (FLOAT_T)(st->control_depth[2] - 0x40) * DIV_64 + 0.5;
		tmp += (FLOAT_T)channel[ch].cc1.val * (FLOAT_T)(st->control_depth[3] - 0x40) * DIV_64 + 0.5; // ac1
		tmp += (FLOAT_T)channel[ch].cc2.val * (FLOAT_T)(st->control_depth[4] - 0x40) * DIV_64 + 0.5; // ac2
		tmp += (FLOAT_T)channel[ch].cc3.val * (FLOAT_T)(st->control_depth[5] - 0x40) * DIV_64 + 0.5; // cbc1
		tmp += (FLOAT_T)channel[ch].cc4.val * (FLOAT_T)(st->control_depth[6] - 0x40) * DIV_64 + 0.5; // cbc2
		if(tmp > 127)
			tmp = 127;
		else if(tmp < 0)
			tmp = 0;
		if(tmp == st->param_lsb[st->control_param]){continue;} // no change
		st->param_lsb[st->control_param] = tmp;
		ctl->cmsg(CMSG_INFO, VERB_NOISY, "XG VARIATION:%d Control (ctrl * depth -> param:%d)", i, tmp);
		recompute_effect_xg(st, 0);
		
	}
	for (i = 0; i < XG_INSERTION_EFFECT_NUM; i++) {
		struct effect_xg_t *st = &insertion_effect_xg[i];
		if(ch != st->part) {continue;}
		tmp = st->set_param_lsb[st->control_param];
		tmp += (FLOAT_T)channel[ch].mod.val * (FLOAT_T)(st->control_depth[0] - 0x40) * DIV_64 + 0.5;
		tmp += (FLOAT_T)channel[ch].bend.val * (FLOAT_T)(st->control_depth[1] - 0x40) * DIV_64 + 0.5;
		tmp += (FLOAT_T)channel[ch].caf.val * (FLOAT_T)(st->control_depth[2] - 0x40) * DIV_64 + 0.5;
		tmp += (FLOAT_T)channel[ch].cc1.val * (FLOAT_T)(st->control_depth[3] - 0x40) * DIV_64 + 0.5; // ac1
		tmp += (FLOAT_T)channel[ch].cc2.val * (FLOAT_T)(st->control_depth[4] - 0x40) * DIV_64 + 0.5; // ac2
		tmp += (FLOAT_T)channel[ch].cc3.val * (FLOAT_T)(st->control_depth[5] - 0x40) * DIV_64 + 0.5; // cbc1
		tmp += (FLOAT_T)channel[ch].cc4.val * (FLOAT_T)(st->control_depth[6] - 0x40) * DIV_64 + 0.5; // cbc2
		if(tmp > 127)
			tmp = 127;
		else if(tmp < 0)
			tmp = 0;
		if(tmp == st->param_lsb[st->control_param]){continue;} // no change
		st->param_lsb[st->control_param] = tmp;
		ctl->cmsg(CMSG_INFO, VERB_NOISY, "XG INSERTION:%d Control (ctrl * depth -> param:%d)", i, tmp);
		recompute_effect_xg(st, 0);
	}
}

/*! initialize XG effect parameters */
static void init_effect_xg(struct effect_xg_t *st)
{
	int i;

	free_effect_list(st->ef);
	st->ef = NULL;

	st->use_msb = 0;
	st->type_msb = st->type_lsb	= 0;
	st->type = 0;		
	st->connection = 0;
	st->part = 0x7f;
	st->send_chorus = st->send_reverb = 0;
	st->ret = st->pan = 0x40;
	for (i = 0; i < 16; i++) {
		st->param_lsb[i] = 0;
		st->set_param_lsb[i] = 0;
	}
	for (i = 0; i < 10; i++) {
		st->param_msb[i] = 0;
		st->set_param_msb[i] = 0;
	}
	for (i = 0; i < 7; i++) {
		st->control_depth[i] = 0x40;
	}
	st->control_param = 0;
	recompute_effect_xg(st, 1);
}

///r
/*! initialize XG effect parameters */
static void init_all_effect_xg(void)
{
	int i;
 	init_effect_xg(&reverb_status_xg);

	reverb_status_xg.type_msb = 0x01;
	reverb_status_xg.type_lsb = 0x00;
	reverb_status_xg.type = 0x0100;
	reverb_status_xg.connection = XG_CONN_SYSTEM_REVERB;
	realloc_effect_xg(&reverb_status_xg);
	init_effect_xg(&chorus_status_xg);
	chorus_status_xg.type_msb = 0x41;
	chorus_status_xg.type_lsb = 0x00;
	chorus_status_xg.type = 0x4100;
	chorus_status_xg.connection = XG_CONN_SYSTEM_CHORUS;
	realloc_effect_xg(&chorus_status_xg);
	for (i = 0; i < XG_VARIATION_EFFECT_NUM; i++) {
		init_effect_xg(&variation_effect_xg[i]);
		variation_effect_xg[i].type_msb = 0x05;		
		variation_effect_xg[i].type = 0x0500;
		realloc_effect_xg(&variation_effect_xg[i]);
	}
	for (i = 0; i < XG_INSERTION_EFFECT_NUM; i++) {
		init_effect_xg(&insertion_effect_xg[i]);
		insertion_effect_xg[i].type_msb = 0x49;
		insertion_effect_xg[i].type = 0x4900;
		realloc_effect_xg(&insertion_effect_xg[i]);
	}
	init_multi_eq_xg();
	init_ch_effect_xg();
}

static void set_effect_param_xg(struct effect_xg_t *st, int type_msb, int type_lsb)
{
	int i, j;
	for (i = 0; effect_parameter_xg[i].type_msb != -1
		&& effect_parameter_xg[i].type_lsb != -1; i++) {
		if (type_msb == effect_parameter_xg[i].type_msb
			&& type_lsb == effect_parameter_xg[i].type_lsb) {
			for (j = 0; j < 16; j++) {
				st->set_param_lsb[j] = effect_parameter_xg[i].param_lsb[j];
			}
			for (j = 0; j < 10; j++) {
				st->set_param_msb[j] = effect_parameter_xg[i].param_msb[j];
			}
			st->control_param = effect_parameter_xg[i].control;
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "XG EFX: %s", effect_parameter_xg[i].name);
			return;
		}
	}
	if (type_msb != 0) {
		for (i = 0; effect_parameter_xg[i].type_msb != -1
			&& effect_parameter_xg[i].type_lsb != -1; i++) {
			if (type_lsb == effect_parameter_xg[i].type_lsb) {
				for (j = 0; j < 16; j++) {
					st->set_param_lsb[j] = effect_parameter_xg[i].param_lsb[j];
				}
				for (j = 0; j < 10; j++) {
					st->set_param_msb[j] = effect_parameter_xg[i].param_msb[j];
				}
				st->control_param = effect_parameter_xg[i].control;
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "XG EFX: %s", effect_parameter_xg[i].name);
				return;
			}
		}
	}
}

void realloc_effect_xg(struct effect_xg_t *st)
{
	int type_msb = st->type_msb, type_lsb = st->type_lsb;

	free_effect_list(st->ef);
	st->ef = NULL;
	st->use_msb = 0;

	switch(type_msb) {
	case 0x01:
		switch(type_lsb){
		default:
		case 0x00:
			st->ef = push_effect(st->ef, EFFECT_XG_HALL1);
			break;
		case 0x01:
			st->ef = push_effect(st->ef, EFFECT_XG_HALL2);
			break;
		case 0x06:
			st->ef = push_effect(st->ef, EFFECT_XG_HALL_M);
			break;
		case 0x07:
			st->ef = push_effect(st->ef, EFFECT_XG_HALL_L);
			break;
		}
		break;
	case 0x02:
		switch(type_lsb){
		default:
		case 0x00:
			st->ef = push_effect(st->ef, EFFECT_XG_ROOM1);
			break;
		case 0x01:
			st->ef = push_effect(st->ef, EFFECT_XG_ROOM2);
			break;
		case 0x02:
			st->ef = push_effect(st->ef, EFFECT_XG_ROOM3);
			break;
		case 0x05:
			st->ef = push_effect(st->ef, EFFECT_XG_ROOM_S);
			break;
		case 0x06:
			st->ef = push_effect(st->ef, EFFECT_XG_ROOM_M);
			break;
		case 0x07:
			st->ef = push_effect(st->ef, EFFECT_XG_ROOM_L);
			break;
		}
		break;
	case 0x03:
		switch(type_lsb){
		default:
		case 0x00:
			st->ef = push_effect(st->ef, EFFECT_XG_STAGE1);
			break;
		case 0x01:
			st->ef = push_effect(st->ef, EFFECT_XG_STAGE2);
			break;
		}
		break;
	case 0x04:
		switch(type_lsb){
		default:
		case 0x00:
			st->ef = push_effect(st->ef, EFFECT_XG_PLATE);
			break;
		case 0x07:
			st->ef = push_effect(st->ef, EFFECT_XG_GM_PLATE);
			break;
		}
		break;
	case 0x05:
		st->use_msb = 1;
		st->ef = push_effect(st->ef, EFFECT_XG_DELAY_LCR);
		break;
	case 0x06:
		st->use_msb = 1;
		st->ef = push_effect(st->ef, EFFECT_XG_DELAY_LR);
		break;
	case 0x07:
		st->use_msb = 1;
		st->ef = push_effect(st->ef, EFFECT_XG_ECHO);
		break;
	case 0x08:
		st->use_msb = 1;
		st->ef = push_effect(st->ef, EFFECT_XG_CROSS_DELAY);
		break;
	case 0x09:
		switch(type_lsb){
		default:
		case 0x00:
			st->ef = push_effect(st->ef, EFFECT_XG_EARLY_REF1);
			break;
		case 0x01:
			st->ef = push_effect(st->ef, EFFECT_XG_EARLY_REF2);
			break;
		}
		break;
	case 0x0A:
		st->ef = push_effect(st->ef, EFFECT_XG_GATE_REVERB);
		break;
	case 0x0B:
		st->ef = push_effect(st->ef, EFFECT_XG_REVERSE_GATE);
		break;
	case 0x10:
		st->ef = push_effect(st->ef, EFFECT_XG_WHITE_ROOM);
		break;
	case 0x11:
		st->ef = push_effect(st->ef, EFFECT_XG_TUNNEL);
		break;
	case 0x12:
		st->ef = push_effect(st->ef, EFFECT_XG_CANYON);
		break;
	case 0x13:
		st->ef = push_effect(st->ef, EFFECT_XG_BASEMENT);
		break;
	case 0x14:
		switch(type_lsb){
		default:
		case 0x00:
			st->ef = push_effect(st->ef, EFFECT_XG_KARAOKE1);
			break;
		case 0x01:
			st->ef = push_effect(st->ef, EFFECT_XG_KARAOKE1);
			break;
		case 0x02:
			st->ef = push_effect(st->ef, EFFECT_XG_KARAOKE3);
			break;
		}
		break;
	case 0x15:
		switch(type_lsb){
		default:
		case 0x00:
			st->ef = push_effect(st->ef, EFFECT_XG_TEMPO_DELAY);
			break;
		case 0x08:
			st->ef = push_effect(st->ef, EFFECT_XG_TEMPO_ECHO);
			break;
		}
		break;
	case 0x16:
		st->ef = push_effect(st->ef, EFFECT_XG_TEMPO_CROSS);
		break;
	case 0x41:
		switch(type_lsb){
		default:
		case 0x00:
			st->ef = push_effect(st->ef, EFFECT_XG_CHORUS1);
			break;
		case 0x01:
			st->ef = push_effect(st->ef, EFFECT_XG_CHORUS2);
			break;
		case 0x02:
			st->ef = push_effect(st->ef, EFFECT_XG_CHORUS3);
			break;
		case 0x03:
			st->ef = push_effect(st->ef, EFFECT_XG_GM_CHORUS1);
			break;
		case 0x04:
			st->ef = push_effect(st->ef, EFFECT_XG_GM_CHORUS2);
			break;
		case 0x05:
			st->ef = push_effect(st->ef, EFFECT_XG_GM_CHORUS3);
			break;
		case 0x06:
			st->ef = push_effect(st->ef, EFFECT_XG_GM_CHORUS4);
			break;
		case 0x07:
			st->ef = push_effect(st->ef, EFFECT_XG_FB_CHORUS);
			break;
		case 0x08:
			st->ef = push_effect(st->ef, EFFECT_XG_CHORUS4);
			break;
		}
		break;
	case 0x42:
		switch(type_lsb){
		default:
		case 0x00:
			st->ef = push_effect(st->ef, EFFECT_XG_CELESTE1);
			break;
		case 0x01:
			st->ef = push_effect(st->ef, EFFECT_XG_CELESTE2);
			break;
		case 0x02:
			st->ef = push_effect(st->ef, EFFECT_XG_CELESTE3);
			break;
		case 0x08:
			st->ef = push_effect(st->ef, EFFECT_XG_CELESTE4);
			break;
		}
		break;
	case 0x43:
		switch(type_lsb){
		default:
		case 0x00:
			st->ef = push_effect(st->ef, EFFECT_XG_FLANGER1);
			break;
		case 0x01:
			st->ef = push_effect(st->ef, EFFECT_XG_FLANGER2);
			break;
		case 0x07:
			st->ef = push_effect(st->ef, EFFECT_XG_GM_FLANGER);
			break;
		case 0x08:
			st->ef = push_effect(st->ef, EFFECT_XG_FLANGER3);
			break;
		}
		break;
	case 0x44:
		st->ef = push_effect(st->ef, EFFECT_XG_SYMPHONIC);
		break;
	case 0x45:
		switch(type_lsb){
		default:
		case 0x00:
			st->ef = push_effect(st->ef, EFFECT_XG_ROTARY_SPEAKER);
			break;
		case 0x01:
			st->ef = push_effect(st->ef, EFFECT_XG_DS_ROTARY_SPEAKER);
			break;
		case 0x02:
			st->ef = push_effect(st->ef, EFFECT_XG_OD_ROTARY_SPEAKER);
			break;
		case 0x03:
			st->ef = push_effect(st->ef, EFFECT_XG_AMP_ROTARY_SPEAKER);
			break;
		}
		break;
	case 0x46:
		st->ef = push_effect(st->ef, EFFECT_XG_TREMOLO);
		break;
	case 0x47:
		st->ef = push_effect(st->ef, EFFECT_XG_AUTO_PAN);
		break;
	case 0x48:
		switch(type_lsb){
		default:
		case 0x00:
			st->ef = push_effect(st->ef, EFFECT_XG_PHASER1);
			break;
		case 0x08:
			st->ef = push_effect(st->ef, EFFECT_XG_PHASER2);
			break;
		}
		break;
	case 0x49:
		switch(type_lsb){
		default:
		case 0x00:
			st->ef = push_effect(st->ef, EFFECT_XG_DISTORTION);
			break;
		case 0x01:
			st->ef = push_effect(st->ef, EFFECT_XG_COMP_DISTORTION);
			break;
		case 0x08:
			st->ef = push_effect(st->ef, EFFECT_XG_STEREO_DISTORTION);
			break;
		}
		break;
	case 0x4A:
		switch(type_lsb){
		default:
		case 0x00:
			st->ef = push_effect(st->ef, EFFECT_XG_OVERDRIVE);
			break;
		case 0x08:
			st->ef = push_effect(st->ef, EFFECT_XG_STEREO_OVERDRIVE);
			break;
		}
		break;
	case 0x4B:
		switch(type_lsb){
		default:
		case 0x00:
			st->ef = push_effect(st->ef, EFFECT_XG_AMP_SIMULATOR);
			break;
		case 0x01:
			st->ef = push_effect(st->ef, EFFECT_XG_AMP_SIMULATOR2);
			break;
		case 0x08:
			st->ef = push_effect(st->ef, EFFECT_XG_STEREO_AMP_SIMULATOR);
			break;
		}
		break;
	case 0x4C:
		st->ef = push_effect(st->ef, EFFECT_XG_EQ3);
		break;
	case 0x4D:
		st->ef = push_effect(st->ef, EFFECT_XG_EQ2);
		break;
	case 0x4E:
		switch(type_lsb){
		default:
		case 0x00:
			st->ef = push_effect(st->ef, EFFECT_XG_AUTO_WAH);
			break;
		case 0x01:
			st->ef = push_effect(st->ef, EFFECT_XG_AUTO_WAH_DS);
			break;
		case 0x02:
			st->ef = push_effect(st->ef, EFFECT_XG_AUTO_WAH_OD);
			break;
		}
		break;
	case 0x51:
		st->ef = push_effect(st->ef, EFFECT_XG_HARMONIC_ENHANCER);
		break;
	case 0x52:
		switch(type_lsb){
		default:
		case 0x00:
			st->ef = push_effect(st->ef, EFFECT_XG_TOUCH_WAH1);
			break;
		case 0x01:
			st->ef = push_effect(st->ef, EFFECT_XG_TOUCH_WAH_DS);
			break;
		case 0x02:
			st->ef = push_effect(st->ef, EFFECT_XG_TOUCH_WAH_OD);
			break;
		case 0x08:
			st->ef = push_effect(st->ef, EFFECT_XG_TOUCH_WAH2);
			break;
		}
		break;
	case 0x53:
		st->ef = push_effect(st->ef, EFFECT_XG_COMPRESSOR);
		break;
	case 0x54:
		st->ef = push_effect(st->ef, EFFECT_XG_NOISE_GATE);
		break;
	case 0x56:
		switch(type_lsb){
		default:
		case 0x00:
			st->ef = push_effect(st->ef, EFFECT_XG_2WAY_ROTARY_SPEAKER);
			break;
		case 0x01:
			st->ef = push_effect(st->ef, EFFECT_XG_DS_2WAY_ROTARY_SPEAKER);
			break;
		case 0x02:
			st->ef = push_effect(st->ef, EFFECT_XG_OD_2WAY_ROTARY_SPEAKER);
			break;
		case 0x03:
			st->ef = push_effect(st->ef, EFFECT_XG_AMP_2WAY_ROTARY_SPEAKER);
			break;
		}
		break;
	case 0x57:
		st->ef = push_effect(st->ef, EFFECT_XG_ENSEMBLE_DETUNE);
		break;
	case 0x58:
		st->ef = push_effect(st->ef, EFFECT_XG_AMBIENCE);
		break;
	case 0x5D:
		st->ef = push_effect(st->ef, EFFECT_XG_TALKING_MODULATOR);
		break;
	case 0x5E:
		st->ef = push_effect(st->ef, EFFECT_XG_LOFI);
		break;
	case 0x5F:
		switch(type_lsb){
		default:
		case 0x00:
			st->use_msb = 1;
			st->ef = push_effect(st->ef, EFFECT_XG_DS_DELAY);
			break;
		case 0x01:
			st->use_msb = 1;
			st->ef = push_effect(st->ef, EFFECT_XG_OD_DELAY);
			break;
		}
		break;
	case 0x60:
		switch(type_lsb){
		default:
		case 0x00:
			st->use_msb = 1;
			st->ef = push_effect(st->ef, EFFECT_XG_COMP_DS_DELAY);
			break;
		case 0x01:
			st->use_msb = 1;
			st->ef = push_effect(st->ef, EFFECT_XG_COMP_OD_DELAY);
			break;
		}
		break;
	case 0x61:
		switch(type_lsb){
		default:
		case 0x00:
			st->use_msb = 1;
			st->ef = push_effect(st->ef, EFFECT_XG_WAH_DS_DELAY);
			break;
		case 0x01:
			st->use_msb = 1;
			st->ef = push_effect(st->ef, EFFECT_XG_WAH_OD_DELAY);
			break;
		}
		break;
	case 0x62:
		switch(type_lsb){
		default:
		case 0x00:
			st->ef = push_effect(st->ef, EFFECT_XG_V_DIST_HARD);
			break;
		case 0x01:
			st->use_msb = 1;
			st->ef = push_effect(st->ef, EFFECT_XG_V_DIST_HARD_DELAY);
			break;
		case 0x02:
			st->ef = push_effect(st->ef, EFFECT_XG_V_DIST_SOFT);
			break;
		case 0x03:
			st->use_msb = 1;
			st->ef = push_effect(st->ef, EFFECT_XG_V_DIST_SOFT_DELAY);
			break;
		}
		break;
	case 0x63:
		switch(type_lsb){
		default:
		case 0x00:
			st->ef = push_effect(st->ef, EFFECT_XG_DUAL_ROTAR_SPEAKER1);
			break;
		case 0x01:
			st->ef = push_effect(st->ef, EFFECT_XG_DUAL_ROTAR_SPEAKER2);
			break;
		}
		break;
	case 0x64:
		switch(type_lsb){
		default:
		case 0x00:
			st->ef = push_effect(st->ef, EFFECT_XG_DS_TEMPO_DELAY);
			break;
		case 0x01:
			st->ef = push_effect(st->ef, EFFECT_XG_OD_TEMPO_DELAY);
			break;
		}
		break;
	case 0x65:
		switch(type_lsb){
		default:
		case 0x00:
			st->ef = push_effect(st->ef, EFFECT_XG_COMP_DS_TEMPO_DELAY);
			break;
		case 0x01:
			st->ef = push_effect(st->ef, EFFECT_XG_COMP_OD_TEMPO_DELAY);
			break;
		}
		break;
	case 0x66:
		switch(type_lsb){
		default:
		case 0x00:
			st->ef = push_effect(st->ef, EFFECT_XG_WAH_DS_TEMPO_DELAY);
			break;
		case 0x01:
			st->ef = push_effect(st->ef, EFFECT_XG_WAH_OD_TEMPO_DELAY);
			break;
		}
		break;
	case 0x67:
		switch(type_lsb){
		default:
		case 0x00:
			st->ef = push_effect(st->ef, EFFECT_XG_V_DIST_HARD_TEMPO_DELAY);
			break;
		case 0x01:
			st->ef = push_effect(st->ef, EFFECT_XG_V_DIST_SOFT_TEMPO_DELAY);
			break;
		}
		break;
	case 0x68:
		st->ef = push_effect(st->ef, EFFECT_XG_V_FLANGER);
		break;
	case 0x6B:
		st->ef = push_effect(st->ef, EFFECT_XG_TEMPO_FLANGER);
		break;
	case 0x6C:
		st->ef = push_effect(st->ef, EFFECT_XG_TEMPO_FLANGER);
		break;
	case 0x71:
		st->ef = push_effect(st->ef, EFFECT_XG_RING_MODULATOR);
		break;
	case 0x7F:
		switch(type_lsb){
		case 0x01:
			st->ef = push_effect(st->ef, EFFECT_XG_3D_MANUAL);
			break;
		case 0x02:
			st->ef = push_effect(st->ef, EFFECT_XG_3D_AUTO);
			break;
		}
		break;
	default:	/* Not Supported */
		type_msb = type_lsb = 0;
		break;
	}
	set_effect_param_xg(st, type_msb, type_lsb);
	recompute_effect_xg(st, 1);
}



/*! initialize GS insertion effect parameters */
void init_insertion_effect_gs(void)
{
	int i;
	struct insertion_effect_gs_t *st = &insertion_effect_gs;

	free_effect_list(st->ef);
	st->ef = NULL;

	for(i = 0; i < 20; i++) {
		st->parameter[i] = 0;
		st->set_param[i] = 0;
	}
	st->type = 0;
	st->type_lsb = 0;
	st->type_msb = 0;
	st->send_reverb = 0x28;
	st->send_chorus = 0;
	st->send_delay = 0;
	st->control_source1 = 0;
	st->control_depth1 = 0x40;
	st->control_source2 = 0;
	st->control_depth2 = 0x40;
	st->send_eq_switch = 0x01;
	st->control_param1 = -1; 
	st->control_param2 = -1;
}

static void set_effect_param_gs(struct insertion_effect_gs_t *st, int msb, int lsb)
{
	int i, j;
	for (i = 0; effect_parameter_gs[i].type_msb != -1
		&& effect_parameter_gs[i].type_lsb != -1; i++) {
		if (msb == effect_parameter_gs[i].type_msb
			&& lsb == effect_parameter_gs[i].type_lsb) {
			for (j = 0; j < 20; j++) {
				st->set_param[j] = effect_parameter_gs[i].param[j];
			}
			st->control_param1 = effect_parameter_gs[i].control1;
			st->control_param2 = effect_parameter_gs[i].control2;
#ifdef _DEBUG
			ctl->cmsg(CMSG_INFO, VERB_NORMAL, "GS EFX: %s", effect_parameter_gs[i].name);
#else
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "GS EFX: %s", effect_parameter_gs[i].name);
#endif
			break;
		}
	}
}

/*! recompute GS insertion effect parameters. */
void recompute_insertion_effect_gs(int marge)
{
	struct insertion_effect_gs_t *st = &insertion_effect_gs;
	EffectList *efc = st->ef;
	int j;

	if (st->ef == NULL) {return;}
	if(marge){
		for (j = 0; j < 20; j++){
			st->parameter[j] = st->set_param[j];
		}
	}
	while(efc != NULL && efc->info != NULL)
	{
		(*efc->engine->conv_gs)(st, efc);
		(*efc->engine->do_effect)(NULL, MAGIC_INIT_EFFECT_INFO, efc);
		efc = efc->next_ef;
	}
}

/*! EFX C.Src1/2 control GS insertion effect parameters. */
void control_effect_gs(MidiEvent *ev)
{
	struct insertion_effect_gs_t *st = &insertion_effect_gs;
	int update = 0;

	if (st->ef == NULL) {return;}
	if(ev->channel >= MAX_CHANNELS) {return;}
	if (!channel[ev->channel].insertion_effect) {return;}
	if(st->control_source1 && st->control_param1 != -1){
		if ((st->control_source1 <= 95 && st->control_source1 == ev->b
				&& ev->type >= ME_TONE_BANK_MSB && ev->type <= ME_UNDEF_CTRL_CHNG)
			|| (st->control_source1 == 96 && ev->type == ME_CHANNEL_PRESSURE)
			|| (st->control_source1 == 97 && ev->type == ME_PITCHWHEEL)	){
			int val = ev->type == ME_PITCHWHEEL ? calc_bend_val(ev->a + ev->b * 128) : ev->a;
			int var = val > 0x7F ? 127.0 : (FLOAT_T)val;
			int depth = st->control_depth1 - 0x40;
			int ctrl = st->control_param1;
			int tmp = st->set_param[ctrl];
			int base = tmp;
			tmp += (FLOAT_T)var * (FLOAT_T)depth * DIV_64 + 0.5;
			if(tmp > 127)
				tmp = 127;
			else if(tmp < 0)
				tmp = 0;
			st->parameter[ctrl] = tmp;
			update = 1;
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
				"GS INS Control1 (VAL:%d * depth:%d /64 + param:%d -> param(%d):%d)", 
					var, depth, base, ctrl, tmp);
		}
	}
	if(st->control_source2 && st->control_param2 != -1){
		if ((st->control_source2 <= 95 && st->control_source2 == ev->b
				&& ev->type >= ME_TONE_BANK_MSB && ev->type <= ME_UNDEF_CTRL_CHNG)
			|| (st->control_source2 == 96 && ev->type == ME_CHANNEL_PRESSURE)
			|| (st->control_source2 == 97 && ev->type == ME_PITCHWHEEL)	){	
			int val = ev->type == ME_PITCHWHEEL ? calc_bend_val(ev->a + ev->b * 128) : ev->a;
			int var = val > 0x7F ? 127.0 : (FLOAT_T)val;
			int depth = st->control_depth2 - 0x40;
			int ctrl = st->control_param2;
			int tmp = st->set_param[ctrl];
			int base = tmp;
			tmp += (FLOAT_T)var * (FLOAT_T)depth * DIV_64 + 0.5;
			if(tmp > 127)
				tmp = 127;
			else if(tmp < 0)
				tmp = 0;
			st->parameter[ctrl] = tmp;
			update = 1;
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
				"GS INS Control2 (VAL:%d * depth:%d /64 + param:%d -> param(%d):%d)", 
					var, depth, base, ctrl, tmp);
		}
	}
	if(update)
		recompute_insertion_effect_gs(0);
}

///r
/*! re-allocate GS insertion effect parameters. */
void realloc_insertion_effect_gs(void)
{
	struct insertion_effect_gs_t *st = &insertion_effect_gs;
	int type_msb = st->type_msb, type_lsb = st->type_lsb;

	free_effect_list(st->ef);
	st->ef = NULL;
	switch(type_msb) {
	case 0x01:
		switch(type_lsb) {
		case 0x00: /* Stereo-EQ */
			st->ef = push_effect(st->ef, EFFECT_GS_STEREO_EQ);
			break;
		case 0x01: /* Spectrum */
			st->ef = push_effect(st->ef, EFFECT_GS_SPECTRUM);
			break;
		case 0x02: /* ehnancer */
			st->ef = push_effect(st->ef, EFFECT_GS_ENHANCER);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x03: /* Humanizer */
			st->ef = push_effect(st->ef, EFFECT_GS_HUMANIZER);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x10: /* Overdrive */
			st->ef = push_effect(st->ef, EFFECT_GS_OVERDRIVE1);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x11: /* Distortion */
			st->ef = push_effect(st->ef, EFFECT_GS_DISTORTION1);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x20: /* Phaser */
			st->ef = push_effect(st->ef, EFFECT_GS_PHASER);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x21: /* Auto Wah */
			st->ef = push_effect(st->ef, EFFECT_GS_AUTO_WAH);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x22: /* Rotary */
			st->ef = push_effect(st->ef, EFFECT_GS_ROTARY);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x23: /* Stereo Flanger */
			st->ef = push_effect(st->ef, EFFECT_GS_STEREO_FLANGER);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x24: /* Step Flanger */
			st->ef = push_effect(st->ef, EFFECT_GS_STEP_FLANGER);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x25: /* Tremolo */
			st->ef = push_effect(st->ef, EFFECT_GS_TREMOLO);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x26: /* Auto Pan */
			st->ef = push_effect(st->ef, EFFECT_GS_AUTO_PAN);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x30: /* Compressor */
			st->ef = push_effect(st->ef, EFFECT_GS_COMPRESSOR);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x31: /* Limiter */
			st->ef = push_effect(st->ef, EFFECT_GS_LIMITER);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x40: /* Hexa Chorus */
			st->ef = push_effect(st->ef, EFFECT_GS_HEXA_CHORUS);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x41: /* Tremolo Chorus */
			st->ef = push_effect(st->ef, EFFECT_GS_TREMOLO_CHORUS);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x42: /* Stereo Chorus */
			st->ef = push_effect(st->ef, EFFECT_GS_STEREO_CHORUS);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x43: /* Space D */
			st->ef = push_effect(st->ef, EFFECT_GS_SPACE_D);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x44: /* 3D Chorus */
			st->ef = push_effect(st->ef, EFFECT_GS_3D_CHORUS);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x50: /* Stereo Delay */
			st->ef = push_effect(st->ef, EFFECT_GS_STEREO_DELAY);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x51: /* Mod Delay */
			st->ef = push_effect(st->ef, EFFECT_GS_MOD_DELAY);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x52: /* 3 Tap Delay */
			st->ef = push_effect(st->ef, EFFECT_GS_3TAP_DELAY);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x53: /* 4 Tap Delay */
			st->ef = push_effect(st->ef, EFFECT_GS_4TAP_DELAY);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x54: /* Tm Ctrl Delay */
			st->ef = push_effect(st->ef, EFFECT_GS_TM_CTRL_DELAY);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x55: /* Reverb */
			st->ef = push_effect(st->ef, EFFECT_GS_REVERB);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x56: /* Gate Reverb */
			st->ef = push_effect(st->ef, EFFECT_GS_GATE_REVERB);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x57: /* 3D Delay */
			st->ef = push_effect(st->ef, EFFECT_GS_3D_DELAY);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x60: /* 2 Pitch Shifter */
			st->ef = push_effect(st->ef, EFFECT_GS_2PITCH_SHIFTER);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x61: /* Fb P.Shifter */
			st->ef = push_effect(st->ef, EFFECT_GS_FB_P_SHIFTER);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x70: /* 3D Auto */
			st->ef = push_effect(st->ef, EFFECT_GS_3D_AUTO);
			break;
		case 0x71: /* 3D Manual */
			st->ef = push_effect(st->ef, EFFECT_GS_3D_MANUAL);
			break;
		case 0x72: /* Lo-Fi 1 */
			st->ef = push_effect(st->ef, EFFECT_GS_LOFI1);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x73: /* Lo-Fi 2 */
			st->ef = push_effect(st->ef, EFFECT_GS_LOFI2);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		default: break;
		}
		break;
	case 0x02:
		switch(type_lsb) {
		case 0x00: /* OD->Chorus */
			st->ef = push_effect(st->ef, EFFECT_GS_S_OD_CHORUS);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x01: /* OD->Flanger */
			st->ef = push_effect(st->ef, EFFECT_GS_S_OD_FLANGER);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x02: /* OD->Delay */
			st->ef = push_effect(st->ef, EFFECT_GS_S_OD_DELAY);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x03: /* DS->Chorus */
			st->ef = push_effect(st->ef, EFFECT_GS_S_DS_CHORUS);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x04: /* DS->Flanger */
			st->ef = push_effect(st->ef, EFFECT_GS_S_DS_FLANGER);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x05: /* DS->Delay */
			st->ef = push_effect(st->ef, EFFECT_GS_S_DS_DELAY);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x06: /* EH->Chorus */
			st->ef = push_effect(st->ef, EFFECT_GS_S_EH_CHORUS);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x07: /* EH->Flanger */
			st->ef = push_effect(st->ef, EFFECT_GS_S_EH_FLANGER);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x08: /* EH->Delay */
			st->ef = push_effect(st->ef, EFFECT_GS_S_EH_DELAY);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x09: /* Cho->Delay */
			st->ef = push_effect(st->ef, EFFECT_GS_S_CHO_DELAY);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x0A: /* FL->Delay */
			st->ef = push_effect(st->ef, EFFECT_GS_S_FL_DELAY);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		case 0x0B: /* Cho->Flanger */
			st->ef = push_effect(st->ef, EFFECT_GS_S_CHO_FLANGER);
			st->ef = push_effect(st->ef, EFFECT_GS_POST_EQ);
			break;
		default: break;
		}
		break;
	case 0x03:
		switch(type_lsb) {
		case 0x00: /* Rotary Multi */
			st->ef = push_effect(st->ef, EFFECT_GS_S_ROTARY_MULTI);
			break;
		default: break;
		}
		break;
	case 0x04:
		switch(type_lsb) {
		case 0x00: /* GTR Multi 1 */
			st->ef = push_effect(st->ef, EFFECT_GS_S_GTR_MULTI1);
			break;
		case 0x01: /* GTR Multi 2 */
			st->ef = push_effect(st->ef, EFFECT_GS_S_GTR_MULTI2);
			break;
		case 0x02: /* GTR Multi 3 */
			st->ef = push_effect(st->ef, EFFECT_GS_S_GTR_MULTI3);
			break;
		case 0x03: /* Clean Gt Multi 1 */
			st->ef = push_effect(st->ef, EFFECT_GS_S_CLEAN_GT_MULTI1);
			break;
		case 0x04: /* Clean Gt Multi 2 */
			st->ef = push_effect(st->ef, EFFECT_GS_S_CLEAN_GT_MULTI2);
			break;
		case 0x050: /* Base Multi */
			st->ef = push_effect(st->ef, EFFECT_GS_S_BASE_MULTI);
			break;
		case 0x06: /* Rhodes Multi */
			st->ef = push_effect(st->ef, EFFECT_GS_S_RHODES_MULTI);
			break;
		default: break;
		}
		break;
	case 0x05:
		switch(type_lsb) {
		case 0x00: /* Keyboard Multi */
			st->ef = push_effect(st->ef, EFFECT_GS_S_KEYBOARD_MULTI);
			break;
		default: break;
		}
		break;
	case 0x11:
		switch(type_lsb) {
		case 0x00: /* Cho/Delay */
			st->ef = push_effect(st->ef, EFFECT_GS_P_CHO_DELAY);
			break;
		case 0x01: /* FL/Delay */
			st->ef = push_effect(st->ef, EFFECT_GS_P_FL_DELAY);
			break;
		case 0x02: /* Cho/Flanger */
			st->ef = push_effect(st->ef, EFFECT_GS_P_CHO_FLANGER);
			break;
		case 0x03: /* OD1 / OD2 */
			st->ef = push_effect(st->ef, EFFECT_GS_P_OD1_OD2);
			break;
		case 0x04: /* OD/Rotary */
			st->ef = push_effect(st->ef, EFFECT_GS_P_OD_ROTARY);
			break;
		case 0x05: /* OD/Phaser */
			st->ef = push_effect(st->ef, EFFECT_GS_P_OD_PHASER);
			break;
		case 0x06: /* OD/AutoWah */
			st->ef = push_effect(st->ef, EFFECT_GS_P_OD_AUTOWAH);
			break;
		case 0x07: /* PH/Rotary */
			st->ef = push_effect(st->ef, EFFECT_GS_P_PH_ROTARY);
			break;
		case 0x08: /* PH/AutoWah */
			st->ef = push_effect(st->ef, EFFECT_GS_P_PH_AUTOWAH);
			break;
		default: break;
		}
		break;
	default: break;
	}

	set_effect_param_gs(st, type_msb, type_lsb);

	recompute_insertion_effect_gs(1); // 1: set_param to parame
}


/*! initialize SD MFX effect parameters */
void init_mfx_effect_sd(void)
{
	int h,i;
	
	for(h = 0; h < SD_MFX_EFFECT_NUM; h++){
		struct mfx_effect_sd_t *st = &mfx_effect_sd[h];

		free_effect_list(st->ef);
		st->ef = NULL;		
		st->efx_source = h; // =part
		st->common_type = 0;
		st->ctrl_channel = h; // mfx1:1ch, mfx2:2ch, mfx3:3ch
		st->ctrl_port = 0; // port A
		for(i = 0; i < 32; i++) {
			st->common_param[i] = 0;
			st->parameter[i] = 0;
		}
		st->common_dry_send = 127;
		st->common_send_reverb = 0;
		st->common_send_chorus = 0;
		for(i = 0; i < 4; i++) {
			st->common_ctrl_source[i] = 0;
			st->common_ctrl_sens[i] = 0x40;
			st->common_ctrl_assign[i] = 0;
		}		
		st->type = &channel[h].mfx_part_type;
		st->set_param = channel[h].mfx_part_param;
		st->ctrl_source = channel[h].mfx_part_ctrl_source;
		st->ctrl_sens = channel[h].mfx_part_ctrl_sens;
		st->ctrl_assign = channel[h].mfx_part_ctrl_assign;
		for (i = 0; i < 8; i++)
			st->ctrl_param[i] = -1;
		st->dry_level = 1.0;
		st->reverb_level = 0.0;
		st->chorus_level = 0.0;
		st->dry_leveli = TIM_FSCALE(1.0, 24);
		st->reverb_leveli = TIM_FSCALE(0.0, 24);
		st->chorus_leveli = TIM_FSCALE(0.0, 24);
	}
}

static void set_mfx_effect_param_sd(struct mfx_effect_sd_t *st, int type, int patch)
{
	int i, j;
	for (i = 0; effect_parameter_sd[i].type != -1; i++) {
		if (type == effect_parameter_sd[i].type) {
			if(patch >= 2){ // patch default
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "SD MFX patch default: %s", effect_parameter_sd[i].name);
			}else if(patch){ // patch change
				for (j = 0; j < 32; j++)
					st->set_param[j] = effect_parameter_sd[i].param[j];
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "SD MFX patch change: %s", effect_parameter_sd[i].name);
			}else{ // common change
				for (j = 0; j < 32; j++)
					st->set_param[j] = effect_parameter_sd[i].param[j];
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "SD MFX common change: %s", effect_parameter_sd[i].name);
			}
			for (j = 0; j < 8; j++)
				st->ctrl_param[j] = effect_parameter_sd[i].control[j];
			break;
		}
	}
}

/*! recompute SD MFX effect parameters. */
void recompute_mfx_effect_sd(struct mfx_effect_sd_t *st, int marge)
{
	int j;
	EffectList *efc = st->ef;
	
	calc_mfx_send_level_sd(st);
	if (st->ef == NULL) {return;}
	if(marge){
		for (j = 0; j < 32; j++) {
			st->parameter[j] = st->set_param[j];
		}
	}
	while(efc != NULL && efc->info != NULL)
	{
		(*efc->engine->conv_sd)(st, efc);
		(*efc->engine->do_effect)(NULL, MAGIC_INIT_EFFECT_INFO, efc);
		efc = efc->next_ef;
	}
}

/*! MIDI control SD MFX parameters. */
void control_effect_sd(MidiEvent *ev)
{
	int i, s;
	int update = 0;
	int ch = ev->channel;

	for (i = 0; i < SD_MFX_EFFECT_NUM; i++) {
		struct mfx_effect_sd_t *st = &mfx_effect_sd[i];		
		int update = 0;
		int ctrl_ch;
		if (st->ef == NULL) {continue;}	
		if(st->ctrl_channel == -1) {continue;}	
		ctrl_ch = st->ctrl_channel;
		if(st->ctrl_port)
			ctrl_ch += 16; // if portB 17ch~32ch
		if(ctrl_ch != ch) {continue;}
		for (s = 0; s < 4; s++) {
			int source = st->ctrl_source[s];
			if(!source) {continue;}	
			if(source > 32 && source < 64) {continue;}	
			if( (source <= 95 && source == ev->b && ev->type >= ME_TONE_BANK_MSB && ev->type <= ME_UNDEF_CTRL_CHNG)
				|| (source == 96 && ev->type == ME_CHANNEL_PRESSURE)
				|| (source == 97 && ev->type == ME_PITCHWHEEL)	
			){
				FLOAT_T var;
				FLOAT_T sens = st->ctrl_sens[s] - 0x40;
				int val, ctrl;
				int assign = st->ctrl_assign[s];
				if(assign <= 0) {continue;}
				if(assign > 8){
					ctl->cmsg(CMSG_INFO, VERB_NOISY, "SD MFX%d Control%d assign error (assign: %d)", i, s, assign);
					// コントロール可能なパラメータは最大7個まで
					// パラメータ単位だとするとパラメータ数は32個あるので足りないことになる
					// チャンネルなのか？ それだと MFX Control Channelは何なのか
					continue;
				}
				--assign;
				ctrl = st->ctrl_param[assign];
				if(ctrl == -1) {continue;}					
				val = ev->type == ME_PITCHWHEEL ? calc_bend_val(ev->a + ev->b * 128) : ev->a;
				var = val > 0x7F ? 127.0 : (FLOAT_T)val;
				if(ctrl >= 0){
					int tmp = st->set_param[ctrl];
					tmp += var * sens * DIV_64 + 0.5;
					if(tmp > 127)
						tmp = 127;
					else if(tmp < 0)
						tmp = 0;
					st->parameter[ctrl] = tmp;
					update = 1;
					ctl->cmsg(CMSG_INFO, VERB_NOISY,
						"SD MFX%d Control%d assign:%d (VAL:%d * sens:%d /64 + set_param:%d -> param:%d)", 
							i, s, ev->a, sens, st->set_param[ctrl], st->parameter[ctrl]);
				}else if(ctrl == -2){
					int tmp = st->set_param[1] * 100 + st->set_param[3];
					int tmp2 = tmp;
					tmp2 += var * sens * DIV_64 + 0.5;
					if(tmp2 == 0){
						st->set_param[1] = st->set_param[3] = 0;
					}else if(tmp2 < 0){
						int tmp3 = -tmp2 / 100;
						st->parameter[1] = -tmp3;
						st->parameter[3] = tmp2 + tmp3 * 100;
					}else{
						int tmp3 = tmp2 / 100;
						st->parameter[1] = tmp3;
						st->parameter[3] = tmp2 - tmp3 * 100;
					}
					update = 1;
					ctl->cmsg(CMSG_INFO, VERB_NOISY,
						"SD MFX%d Control%d assign:%d (VAL:%d * sens:%d /64 + set_param:%d[cent] -> param:%d[cent])", 
							i, s, ev->a, sens, tmp, tmp2);					
				}else if(ctrl == -3){
					int tmp = st->set_param[2] * 100 + st->set_param[4];
					int tmp2 = tmp;	
					tmp2 += var * sens * DIV_64 + 0.5;
					if(tmp2 == 0){
						st->set_param[2] = st->set_param[4] = 0;
					}else if(tmp2 < 0){
						int tmp3 = -tmp2 / 100;
						st->parameter[2] = -tmp3;
						st->parameter[4] = tmp2 + tmp3 * 100;
					}else{
						int tmp3 = tmp2 / 100;
						st->parameter[2] = tmp3;
						st->parameter[4] = tmp2 - tmp3 * 100;
					}
					update = 1;
					ctl->cmsg(CMSG_INFO, VERB_NOISY,
						"SD MFX%d Control%d assign:%d (VAL:%d * sens:%d /64 + set_param:%d[cent] -> param:%d[cent])", 
							i, s, ev->a, sens, tmp, tmp2);	
				}else if(ctrl == -4){
					int tmp = st->set_param[1] * 100 + st->set_param[2];	
					int tmp2 = tmp;
					tmp2 += var * sens * DIV_64 + 0.5;
					if(tmp2 == 0){
						st->set_param[1] = st->set_param[2] = 0;
					}else if(tmp2 < 0){
						int tmp3 = -tmp2 / 100;
						st->parameter[1] = -tmp3;
						st->parameter[2] = tmp2 + tmp3 * 100;
					}else{
						int tmp3 = tmp2 / 100;
						st->parameter[1] = tmp3;
						st->parameter[2] = tmp2 - tmp3 * 100;
					}
					update = 1;
					ctl->cmsg(CMSG_INFO, VERB_NOISY,
						"SD MFX%d Control%d assign:%d (VAL:%d * sens:%d /64 + set_param:%d[cent] -> param:%d[cent])", 
							i, s, ev->a, sens, tmp, tmp2);	
				}else if(ctrl == -5){
					int tmp = st->set_param[1] * 100 + st->set_param[4];	
					int tmp2 = tmp;
					tmp2 += var * sens * DIV_64 + 0.5;
					if(tmp2 == 0){
						st->set_param[1] = st->set_param[4] = 0;
					}else if(tmp2 < 0){
						int tmp3 = -tmp2 / 100;
						st->parameter[1] = -tmp3;
						st->parameter[4] = tmp2 + tmp3 * 100;
					}else{
						int tmp3 = tmp2 / 100;
						st->parameter[1] = tmp3;
						st->parameter[4] = tmp2 - tmp3 * 100;
					}
					update = 1;
					ctl->cmsg(CMSG_INFO, VERB_NOISY,
						"SD MFX%d Control%d assign:%d (VAL:%d * sens:%d /64 + set_param:%d[cent] -> param:%d[cent])", 
							i, s, ev->a, sens, tmp, tmp2);	
				}else if(ctrl == -6){
					int tmp = st->set_param[2] * 100 + st->set_param[5];	
					int tmp2 = tmp;
					tmp2 += var * sens * DIV_64 + 0.5;
					if(tmp2 == 0){
						st->set_param[2] = st->set_param[5] = 0;
					}else if(tmp2 < 0){
						int tmp3 = -tmp2 / 100;
						st->parameter[2] = -tmp3;
						st->parameter[5] = tmp2 + tmp3 * 100;
					}else{
						int tmp3 = tmp2 / 100;
						st->parameter[2] = tmp3;
						st->parameter[5] = tmp2 - tmp3 * 100;
					}
					update = 1;
					ctl->cmsg(CMSG_INFO, VERB_NOISY,
						"SD MFX%d Control%d assign:%d (VAL:%d * sens:%d /64 + set_param:%d[cent] -> param:%d[cent])", 
							i, s, ev->a, sens, tmp, tmp2);	
				}else if(ctrl == -7){
					int tmp = st->set_param[3] * 100 + st->set_param[6];	
					int tmp2 = tmp;
					tmp2 += var * sens * DIV_64 + 0.5;
					if(tmp2 == 0){
						st->set_param[3] = st->set_param[6] = 0;
					}else if(tmp2 < 0){
						int tmp3 = -tmp2 / 100;
						st->parameter[3] = -tmp3;
						st->parameter[6] = tmp2 + tmp3 * 100;
					}else{
						int tmp3 = tmp2 / 100;
						st->parameter[3] = tmp3;
						st->parameter[6] = tmp2 - tmp3 * 100;
					}
					update = 1;
					ctl->cmsg(CMSG_INFO, VERB_NOISY,
						"SD MFX%d Control%d assign:%d (VAL:%d * sens:%d /64 + set_param:%d[cent] -> param:%d[cent])", 
							i, s, ev->a, sens, tmp, tmp2);	
				}else if(ctrl == -8){
					int tmp = st->set_param[11] * 100 + st->set_param[12];	
					int tmp2 = tmp;
					tmp2 += var * sens * DIV_64 + 0.5;
					if(tmp2 == 0){
						st->set_param[11] = st->set_param[12] = 0;
					}else if(tmp2 < 0){
						int tmp3 = -tmp2 / 100;
						st->parameter[11] = -tmp3;
						st->parameter[12] = tmp2 + tmp3 * 100;
					}else{
						int tmp3 = tmp2 / 100;
						st->parameter[11] = tmp3;
						st->parameter[12] = tmp2 - tmp3 * 100;
					}
					update = 1;
					ctl->cmsg(CMSG_INFO, VERB_NOISY,
						"SD MFX%d Control%d assign:%d (VAL:%d * sens:%d /64 + set_param:%d[cent] -> param:%d[cent])", 
							i, s, ev->a, sens, tmp, tmp2);	
				}
			}
		}
		if(update)
			recompute_mfx_effect_sd(st, 0);
	}
}

/*! re-allocate SD MFX effect parameters. */
void realloc_mfx_effect_sd(struct mfx_effect_sd_t *st, int patch)
{
	int type = *st->type;

	free_effect_list(st->ef);
	st->ef = NULL;
	switch(type) {
	case 0x01: st->ef = push_effect(st->ef, EFFECT_SD_STEREO_EQ); break;
	case 0x02: st->ef = push_effect(st->ef, EFFECT_SD_OVERDRIVE); break;
	case 0x03: st->ef = push_effect(st->ef, EFFECT_SD_DISTORTION); break;
	case 0x04: st->ef = push_effect(st->ef, EFFECT_SD_PHASER); break;
	case 0x05: st->ef = push_effect(st->ef, EFFECT_SD_SPECTRUM); break;
	case 0x06: st->ef = push_effect(st->ef, EFFECT_SD_ENHANCER); break;
	case 0x07: st->ef = push_effect(st->ef, EFFECT_SD_AUTO_WAH); break;
	case 0x08: st->ef = push_effect(st->ef, EFFECT_SD_ROTARY); break;
	case 0x09: st->ef = push_effect(st->ef, EFFECT_SD_COMPRESSOR); break;
	case 0x0A: st->ef = push_effect(st->ef, EFFECT_SD_LIMITER); break;
	case 0x0B: st->ef = push_effect(st->ef, EFFECT_SD_HEXA_CHORUS); break;
	case 0x0C: st->ef = push_effect(st->ef, EFFECT_SD_TREMOLO_CHORUS); break;
	case 0x0D: st->ef = push_effect(st->ef, EFFECT_SD_SPACE_D); break;
	case 0x0E: st->ef = push_effect(st->ef, EFFECT_SD_STEREO_CHORUS); break;
	case 0x0F: st->ef = push_effect(st->ef, EFFECT_SD_STEREO_FLANGER); break;
	case 0x10: st->ef = push_effect(st->ef, EFFECT_SD_STEP_FLANGER); break;
	case 0x11: st->ef = push_effect(st->ef, EFFECT_SD_STEREO_DELAY); break;
	case 0x12: st->ef = push_effect(st->ef, EFFECT_SD_MOD_DELAY); break;
	case 0x13: st->ef = push_effect(st->ef, EFFECT_SD_3TAP_DELAY); break;
	case 0x14: st->ef = push_effect(st->ef, EFFECT_SD_4TAP_DELAY); break;
	case 0x15: st->ef = push_effect(st->ef, EFFECT_SD_TM_CTRL_DELAY); break;
	case 0x16: st->ef = push_effect(st->ef, EFFECT_SD_2PITCH_SHIFTER); break;
	case 0x17: st->ef = push_effect(st->ef, EFFECT_SD_FB_P_SHIFTER); break;
	case 0x18: st->ef = push_effect(st->ef, EFFECT_SD_REVERB); break;
	case 0x19: st->ef = push_effect(st->ef, EFFECT_SD_GATE_REVERB); break;
	case 0x1A: st->ef = push_effect(st->ef, EFFECT_SD_S_OD_CHORUS); break;
	case 0x1B: st->ef = push_effect(st->ef, EFFECT_SD_S_OD_FLANGER); break;
	case 0x1C: st->ef = push_effect(st->ef, EFFECT_SD_S_OD_DELAY); break;
	case 0x1D: st->ef = push_effect(st->ef, EFFECT_SD_S_DS_CHORUS); break;
	case 0x1E: st->ef = push_effect(st->ef, EFFECT_SD_S_DS_FLANGER); break;
	case 0x1F: st->ef = push_effect(st->ef, EFFECT_SD_S_DS_DELAY); break;
	case 0x20: st->ef = push_effect(st->ef, EFFECT_SD_S_EH_CHORUS); break;
	case 0x21: st->ef = push_effect(st->ef, EFFECT_SD_S_EH_FLANGER); break;
	case 0x22: st->ef = push_effect(st->ef, EFFECT_SD_S_EH_DELAY); break;
	case 0x23: st->ef = push_effect(st->ef, EFFECT_SD_S_CHO_DELAY); break;
	case 0x24: st->ef = push_effect(st->ef, EFFECT_SD_S_FL_DELAY); break;
	case 0x25: st->ef = push_effect(st->ef, EFFECT_SD_S_CHO_FLANGER); break;
	case 0x26: st->ef = push_effect(st->ef, EFFECT_SD_P_CHO_DELAY); break;
	case 0x27: st->ef = push_effect(st->ef, EFFECT_SD_P_FL_DELAY); break;
	case 0x28: st->ef = push_effect(st->ef, EFFECT_SD_P_CHO_FLANGER); break;
	case 0x29: st->ef = push_effect(st->ef, EFFECT_SD_STEREO_PHASER); break;
	case 0x2A: st->ef = push_effect(st->ef, EFFECT_SD_KEYSYNC_FLANGER); break;
	case 0x2B: st->ef = push_effect(st->ef, EFFECT_SD_FORMANT_FILTER); break;
	case 0x2C: st->ef = push_effect(st->ef, EFFECT_SD_RING_MODULATOR); break;
	case 0x2D: st->ef = push_effect(st->ef, EFFECT_SD_MULTITAP_DELAY); break;
	case 0x2E: st->ef = push_effect(st->ef, EFFECT_SD_REVERSE_DELAY); break;
	case 0x2F: st->ef = push_effect(st->ef, EFFECT_SD_SHUFFLE_DELAY); break;
	case 0x30: st->ef = push_effect(st->ef, EFFECT_SD_3D_DELAY); break;
	case 0x31: st->ef = push_effect(st->ef, EFFECT_SD_3PITCH_SHIFTER); break;
	case 0x32: st->ef = push_effect(st->ef, EFFECT_SD_LOFI_COMPRESS); break;
	case 0x33: st->ef = push_effect(st->ef, EFFECT_SD_LOFI_NOISE); break;
	case 0x34: st->ef = push_effect(st->ef, EFFECT_SD_SPEAKER_SIMULATOR); break;
	case 0x35: st->ef = push_effect(st->ef, EFFECT_SD_OVERDRIVE2); break;
	case 0x36: st->ef = push_effect(st->ef, EFFECT_SD_DISTORTION2); break;
	case 0x37: st->ef = push_effect(st->ef, EFFECT_SD_STEREO_COMPRESSOR); break;
	case 0x38: st->ef = push_effect(st->ef, EFFECT_SD_STEREO_LIMITER); break;
	case 0x39: st->ef = push_effect(st->ef, EFFECT_SD_GATE); break;
	case 0x3A: st->ef = push_effect(st->ef, EFFECT_SD_SLICER); break;
	case 0x3B: st->ef = push_effect(st->ef, EFFECT_SD_ISOLATOR); break;
	case 0x3C: st->ef = push_effect(st->ef, EFFECT_SD_3D_CHORUS); break;
	case 0x3D: st->ef = push_effect(st->ef, EFFECT_SD_3D_FLANGER); break;
	case 0x3E: st->ef = push_effect(st->ef, EFFECT_SD_TREMOLO); break;
	case 0x3F: st->ef = push_effect(st->ef, EFFECT_SD_AUTO_PAN); break;
	case 0x40: st->ef = push_effect(st->ef, EFFECT_SD_STEREO_PHASER2); break;
	case 0x41: st->ef = push_effect(st->ef, EFFECT_SD_STEREO_AUTO_WAH); break;
	case 0x42: st->ef = push_effect(st->ef, EFFECT_SD_STEREO_FORMANT_FILTER); break;
	case 0x43: st->ef = push_effect(st->ef, EFFECT_SD_MULTITAP_DELAY2); break;
	case 0x44: st->ef = push_effect(st->ef, EFFECT_SD_REVERSE_DELAY2); break;
	case 0x45: st->ef = push_effect(st->ef, EFFECT_SD_SHUFFLE_DELAY2); break;
	case 0x46: st->ef = push_effect(st->ef, EFFECT_SD_3D_DELAY2); break;
	case 0x47: st->ef = push_effect(st->ef, EFFECT_SD_ROTARY2); break;
	case 0x48: st->ef = push_effect(st->ef, EFFECT_SD_S_ROTARY_MULTI); break;
	case 0x49: st->ef = push_effect(st->ef, EFFECT_SD_S_KEYBOARD_MULTI); break;
	case 0x4A: st->ef = push_effect(st->ef, EFFECT_SD_S_RHODES_MULTI); break;
	case 0x4B: st->ef = push_effect(st->ef, EFFECT_SD_S_JD_MULTI); break;
	case 0x4C: st->ef = push_effect(st->ef, EFFECT_SD_STEREO_LOFI_COMPRESS); break;
	case 0x4D: st->ef = push_effect(st->ef, EFFECT_SD_STEREO_LOFI_NOISE); break;
	case 0x4E: st->ef = push_effect(st->ef, EFFECT_SD_GUITAR_AMP_SIMULATOR); break;
	case 0x4F: st->ef = push_effect(st->ef, EFFECT_SD_STEREO_OVERDRIVE); break;
	case 0x50: st->ef = push_effect(st->ef, EFFECT_SD_STEREO_DISTORTION); break;
	case 0x51: st->ef = push_effect(st->ef, EFFECT_SD_S_GUITAR_MULTI_A); break;
	case 0x52: st->ef = push_effect(st->ef, EFFECT_SD_S_GUITAR_MULTI_B); break;
	case 0x53: st->ef = push_effect(st->ef, EFFECT_SD_S_GUITAR_MULTI_C); break;
	case 0x54: st->ef = push_effect(st->ef, EFFECT_SD_S_CLEAN_GUITAR_MULTI_A); break;
	case 0x55: st->ef = push_effect(st->ef, EFFECT_SD_S_CLEAN_GUITAR_MULTI_B); break;
	case 0x56: st->ef = push_effect(st->ef, EFFECT_SD_S_BASE_MULTI); break;
	case 0x57: st->ef = push_effect(st->ef, EFFECT_SD_ISOLATOR2); break;
	case 0x58: st->ef = push_effect(st->ef, EFFECT_SD_STEREO_SPECTRUM); break;
	case 0x59: st->ef = push_effect(st->ef, EFFECT_SD_3D_AUTO_SPIN); break;
	case 0x5A: st->ef = push_effect(st->ef, EFFECT_SD_3D_MANUAL); break;
	// for effect test
	case 0x7E: st->ef = push_effect(st->ef, EFFECT_SD_TEST1); break;
	case 0x7F: st->ef = push_effect(st->ef, EFFECT_SD_TEST1); break;
	default: break;
	}	
	set_mfx_effect_param_sd(st, type, patch);
	recompute_mfx_effect_sd(st, 1);
}


/*! initialize SD chorus effect parameters */
void init_chorus_status_sd(void)
{
	struct mfx_effect_sd_t *st = &chorus_status_sd;

	free_effect_list(st->ef);
	st->ef = NULL;		
	st->efx_source = -1; // =common
	st->common_type = 1; // chorus
	st->common_dry_send = 127;
	st->common_send_reverb = 0;
	st->common_send_chorus = 0;
	st->common_efx_level = 127;
	st->common_output_select = 0; // main	
	st->type = &st->common_type;
	st->set_param = st->common_param;
	st->output_select= &st->common_output_select;
	st->chorus_level = 1.0;
	st->chorus_leveli = TIM_FSCALE(1.0, 24);
	st->reverb_level = 0;
	st->reverb_leveli = 0;
}

static void set_chorus_effect_param_sd(struct mfx_effect_sd_t *st, int type, int patch)
{
	int i, j;
	for (i = 0; effect_parameter_sd[i].type != -1; i++) {
		if (type == effect_parameter_sd[i].type) {
			if(patch >= 2){ // patch default
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "SD Chorus patch default: %s", effect_parameter_sd[i].name);
			}else if(patch){ // patch change
				for (j = 0; j < 12; j++)
					st->set_param[j] = effect_parameter_sd[i].param[j];
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "SD Chorus patch change: %s", effect_parameter_sd[i].name);
			}else{ // common change
				for (j = 0; j < 12; j++)
					st->set_param[j] = effect_parameter_sd[i].param[j];
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "SD Chorus common change: %s", effect_parameter_sd[i].name);
			}
			break;
		}
	}
}

/*! recompute SD chorus effect parameters. */
void recompute_chorus_status_sd(struct mfx_effect_sd_t *st, int marge)
{
	int j;
	EffectList *efc = st->ef;
	
	calc_chorus_level_sd(st);
	if (st->ef == NULL) {return;}
	if(marge){
		for (j = 0; j < 12; j++) {
			st->parameter[j] = st->set_param[j];
		}
	}
	while(efc != NULL && efc->info != NULL)
	{
		(*efc->engine->conv_sd)(st, efc);
		(*efc->engine->do_effect)(NULL, MAGIC_INIT_EFFECT_INFO, efc);
		efc = efc->next_ef;
	}
}

/*! re-allocate SD chorus effect parameters. */
void realloc_chorus_status_sd(struct mfx_effect_sd_t *st, int patch)
{
	int type = *st->type;
	int efx_param_num = 0x00; // effect.c effect_parameter_sd[]

	free_effect_list(st->ef);
	st->ef = NULL;
	switch(type) {
	case 0x01: st->ef = push_effect(st->ef, EFFECT_SD_CHO_CHORUS); efx_param_num = 0x60; break;
	case 0x02: st->ef = push_effect(st->ef, EFFECT_SD_CHO_DELAY); efx_param_num = 0x61; break;
	case 0x03: 
		switch(st->set_param[0]){			
		case 0x00: st->ef = push_effect(st->ef, EFFECT_SD_CHO_CHORUS1); efx_param_num = 0x62; break; // GM2_CHORUS
		case 0x01: st->ef = push_effect(st->ef, EFFECT_SD_CHO_CHORUS2); efx_param_num = 0x63; break; // GM2_CHORUS
		case 0x02: st->ef = push_effect(st->ef, EFFECT_SD_CHO_CHORUS3); efx_param_num = 0x64; break; // GM2_CHORUS
		case 0x03: st->ef = push_effect(st->ef, EFFECT_SD_CHO_CHORUS4); efx_param_num = 0x65; break; // GM2_CHORUS
		case 0x04: st->ef = push_effect(st->ef, EFFECT_SD_CHO_FB_CHORUS); efx_param_num = 0x66; break; // GM2_CHORUS
		case 0x05: st->ef = push_effect(st->ef, EFFECT_SD_CHO_FLANGER); efx_param_num = 0x67; break; // GM2_CHORUS
		case 0x7F: st->ef = push_effect(st->ef, EFFECT_SD_CHO_CHORUS1); efx_param_num = 0x62; break; // GM2_CHORUS
		default: break;
		}
		break;
	default: break;
	}	
	set_chorus_effect_param_sd(st, efx_param_num, patch);
	recompute_chorus_status_sd(st, 1);
}


/*! initialize SD reverb effect parameters */
void init_reverb_status_sd(void)
{
	struct mfx_effect_sd_t *st = &reverb_status_sd;

	free_effect_list(st->ef);
	st->ef = NULL;		
	st->efx_source = -1; // =common
	st->common_type = 3; // srv-hall
	st->common_dry_send = 127;
	st->common_send_reverb = 0;
	st->common_send_chorus = 0;
	st->common_efx_level = 100;
	st->common_output_select = 0; // main	
	st->type = &st->common_type;
	st->set_param = st->common_param;
	st->reverb_level = 100.0 * DIV_127;
	st->reverb_leveli = TIM_FSCALE(100.0 * DIV_127, 24);
	st->chorus_level = 0;
	st->chorus_leveli = 0;
}

static void set_reverb_effect_param_sd(struct mfx_effect_sd_t *st, int type, int patch)
{
	int i, j;
	for (i = 0; effect_parameter_sd[i].type != -1; i++) {
		if (type == effect_parameter_sd[i].type) {
			if(patch >= 2){ // patch default
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "SD Reverb patch default: %s", effect_parameter_sd[i].name);
			}else if(patch){ // patch change
				for (j = 0; j < 20; j++)
					st->set_param[j] = effect_parameter_sd[i].param[j];
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "SD Reverb patch change: %s", effect_parameter_sd[i].name);
			}else{ // common change
				for (j = 0; j < 20; j++)
					st->set_param[j] = effect_parameter_sd[i].param[j];
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "SD Reverb common change: %s", effect_parameter_sd[i].name);
			}
			break;
		}
	}
}

/*! recompute SD reverb effect parameters. */
void recompute_reverb_status_sd(struct mfx_effect_sd_t *st, int marge)
{
	int j;
	EffectList *efc = st->ef;
	
	calc_reverb_level_sd(st);
	if (st->ef == NULL) {return;}
	if(marge){
		for (j = 0; j < 20; j++) {
			st->parameter[j] = st->set_param[j];
		}
	}
	while(efc != NULL && efc->info != NULL)
	{
		(*efc->engine->conv_sd)(st, efc);
		(*efc->engine->do_effect)(NULL, MAGIC_INIT_EFFECT_INFO, efc);
		efc = efc->next_ef;
	}
}

/*! re-allocate SD reverb effect parameters. */
void realloc_reverb_status_sd(struct mfx_effect_sd_t *st, int patch)
{
	int type = *st->type;
	int efx_param_num = 0x00; // effect.c effect_parameter_sd[]

	free_effect_list(st->ef);
	st->ef = NULL;
	switch(type) {
	case 0x01:
		switch(st->set_param[0]){	
		case 0x00: st->ef = push_effect(st->ef, EFFECT_SD_REV_ROOM1); efx_param_num = 0x68; break;
		case 0x01: st->ef = push_effect(st->ef, EFFECT_SD_REV_ROOM2); efx_param_num = 0x69; break;
		case 0x03: st->ef = push_effect(st->ef, EFFECT_SD_REV_STAGE1); efx_param_num = 0x6A; break;
		case 0x04: st->ef = push_effect(st->ef, EFFECT_SD_REV_STAGE2); efx_param_num = 0x6B; break;
		case 0x05: st->ef = push_effect(st->ef, EFFECT_SD_REV_HALL1); efx_param_num = 0x6C; break;
		case 0x06: st->ef = push_effect(st->ef, EFFECT_SD_REV_HALL2); efx_param_num = 0x6D; break;
		case 0x07: st->ef = push_effect(st->ef, EFFECT_SD_REV_DELAY); efx_param_num = 0x6E; break;
		case 0x08: st->ef = push_effect(st->ef, EFFECT_SD_REV_PANDELAY); efx_param_num = 0x6F; break;
		case 0x7F: st->ef = push_effect(st->ef, EFFECT_SD_REV_HALL2); efx_param_num = 0x6D; break; // type1 default
		default: break;
		}
		break;
	case 0x02: st->ef = push_effect(st->ef, EFFECT_SD_REV_SRV_ROOM); efx_param_num = 0x70; break;
	case 0x03: st->ef = push_effect(st->ef, EFFECT_SD_REV_SRV_HALL); efx_param_num = 0x71; break;
	case 0x04: st->ef = push_effect(st->ef, EFFECT_SD_REV_SRV_PLATE); efx_param_num = 0x72; break;
	case 0x05:
		switch(st->set_param[0]){	
		case 0x00: st->ef = push_effect(st->ef, EFFECT_SD_REV_SMALL_ROOM); efx_param_num = 0x73; break;
		case 0x01: st->ef = push_effect(st->ef, EFFECT_SD_REV_MEDIUM_ROOM); efx_param_num = 0x74; break;
		case 0x03: st->ef = push_effect(st->ef, EFFECT_SD_REV_LARGE_ROOM); efx_param_num = 0x75; break;
		case 0x04: st->ef = push_effect(st->ef, EFFECT_SD_REV_MEDIUM_HALL); efx_param_num = 0x76; break;
		case 0x05: st->ef = push_effect(st->ef, EFFECT_SD_REV_LARGE_HALL); efx_param_num = 0x77; break;
		case 0x06: st->ef = push_effect(st->ef, EFFECT_SD_REV_PLATE); efx_param_num = 0x78; break;
		case 0x7F: st->ef = push_effect(st->ef, EFFECT_SD_REV_MEDIUM_HALL); efx_param_num = 0x76; break; // type5 default
		default: break;
		}
		break;
	default: break;
	}	
	set_reverb_effect_param_sd(st, efx_param_num, patch);
	recompute_reverb_status_sd(st, 1);
}

/*! initialize SD effect parameters */
static void init_all_effect_sd(void)
{
	int i;

	init_reverb_status_sd();
	realloc_reverb_status_sd(&reverb_status_sd, 0);
	init_chorus_status_sd();
	realloc_chorus_status_sd(&chorus_status_sd, 0);
	init_mfx_effect_sd();
	init_multi_eq_sd();
	init_ch_effect_sd();
}

/*! initialize channel layers. */
void init_channel_layer(int ch)
{
	if (ch >= MAX_CHANNELS)
		return;
	CLEAR_CHANNELMASK(channel[ch].channel_layer);
	SET_CHANNELMASK(channel[ch].channel_layer, ch);
	channel[ch].port_select = ch >> 4;
}

/*! add a new layer. */
void add_channel_layer(int to_ch, int from_ch)
{
	if (to_ch >= MAX_CHANNELS || from_ch >= MAX_CHANNELS)
		return;
	/* add a channel layer */
	UNSET_CHANNELMASK(channel[to_ch].channel_layer, to_ch);
	SET_CHANNELMASK(channel[to_ch].channel_layer, from_ch);
	ctl->cmsg(CMSG_INFO, VERB_NOISY,
			"Channel Layer (CH:%d -> CH:%d)", from_ch, to_ch);
}

/*! remove all layers for this channel. */
void remove_channel_layer(int ch)
{
	int i, offset;
	
	if (ch >= MAX_CHANNELS)
		return;
	/* remove channel layers */
	offset = ch & ~0xf;
	for (i = offset; i < offset + REDUCE_CHANNELS; i++)
		UNSET_CHANNELMASK(channel[i].channel_layer, ch);
	SET_CHANNELMASK(channel[ch].channel_layer, ch);
}


void free_readmidi(void)
{
	reuse_mblock(&mempool);
	free_time_segments();
	free_all_midi_file_info();
	free_userdrum();
	free_userdrum2();
	free_userinst();
	if (string_event_strtab.nstring > 0)
		delete_string_table(&string_event_strtab);
	if (string_event_table != NULL) {
		safe_free(string_event_table[0]);
		safe_free(string_event_table);
		string_event_table = NULL;
		string_event_table_size = 0;
	}
}



