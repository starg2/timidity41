/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2001 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>
*/

/* This code from awesfx
 * Modified by Masanao Izumo <mo@goice.co.jp>
 */

/*================================================================
 * parsesf.c
 *	parse SoundFonr layers and convert it to AWE driver patch
 *
 * Copyright (C) 1996,1997 Takashi Iwai
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *================================================================*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <stdlib.h>
#include <math.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include "timidity.h"
#include "common.h"
#include "tables.h"
#include "instrum.h"
#include "playmidi.h"
#include "controls.h"
#include "sffile.h"
#include "sflayer.h"
#include "sfitem.h"
#include "output.h"
#include "filter.h"
#include "resample.h"

#define FILENAME_NORMALIZE(fname) url_expand_home_dir(fname)
#define FILENAME_REDUCED(fname)   url_unexpand_home_dir(fname)
#define SFMalloc(rec, count)      new_segment(&(rec)->pool, count)
#define SFStrdup(rec, s)          strdup_mblock(&(rec)->pool, s)

/*----------------------------------------------------------------
 * compile flags
 *----------------------------------------------------------------*/

/*#define SF_CLOSE_EACH_FILE*/
/*#define SF_SUPPRESS_ENVELOPE*/
/*#define SF_SUPPRESS_TREMOLO*/
/*#define SF_SUPPRESS_VIBRATO*/
#define CUTOFF_AMPTUNING 0.6


/* return value */
#define AWE_RET_OK		0	/* successfully loaded */
#define AWE_RET_ERR		1	/* some fatal error occurs */
#define AWE_RET_SKIP		2	/* some fonts are skipped */
#define AWE_RET_NOMEM		3	/* out or memory; not all fonts loaded */
#define AWE_RET_NOT_FOUND	4	/* the file is not found */

/*----------------------------------------------------------------
 * local parameters
 *----------------------------------------------------------------*/

typedef struct _SFPatchRec {
	int preset, bank, keynote; /* -1 = matches all */
} SFPatchRec;

typedef struct _SampleList {
	Sample v;
	struct _SampleList *next;
	int32 start, len;
	int32 cutoff_freq;
	FLOAT_T resonance;
	int16 scaleTuning;	/* pitch scale tuning(%), normally 100 */
	int16 root, tune;
	char low, high;		/* key note range */

	/* Depend on play_mode->rate */
	int32 vibrato_freq;
	double attack;
	double hold;
	int sustain;
	double decay;
	double release;
} SampleList;

typedef struct _InstList {
	SFPatchRec pat;
	int pr_idx;
	int samples;
	int order;
	SampleList *slist;
	struct _InstList *next;
} InstList;

typedef struct _SFExclude {
	SFPatchRec pat;
	struct _SFExclude *next;
} SFExclude;

typedef struct _SFOrder {
	SFPatchRec pat;
	int order;
	struct _SFOrder *next;
} SFOrder;

#define INSTHASHSIZE 127
#define INSTHASH(bank, preset, keynote) \
	((int)(((unsigned)bank ^ (unsigned)preset ^ (unsigned)keynote) % INSTHASHSIZE))

typedef struct _SFInsts {
	struct timidity_file *tf;
	char *fname;
	int8 def_order, def_cutoff_allowed, def_resonance_allowed;
	uint16 version, minorversion;
	int32 samplepos, samplesize;
	InstList *instlist[INSTHASHSIZE];
	char **inst_namebuf;
	SFExclude *sfexclude;
	SFOrder *sforder;
	struct _SFInsts *next;
	FLOAT_T amptune;
	MBlockList pool;
} SFInsts;

/*----------------------------------------------------------------*/

/* prototypes */

#define P_GLOBAL	1
#define P_LAYER		2
#ifndef FALSE
#define FALSE 0
#endif /* FALSE */
#ifndef TRUE
#define TRUE 1
#endif /* TRUE */


static SFInsts *find_soundfont(char *sf_file);
static SFInsts *new_soundfont(char *sf_file);
static void init_sf(SFInsts *rec);
static void end_soundfont(SFInsts *rec);
static Instrument *try_load_soundfont(SFInsts *rec, int order, int bank,
				      int preset, int keynote);
static Instrument *load_from_file(SFInsts *rec, InstList *ip);
static int is_excluded(SFInsts *rec, int bank, int preset, int keynote);
static int is_ordered(SFInsts *rec, int bank, int preset, int keynote);
static int load_font(SFInfo *sf, int pridx);
static int parse_layer(SFInfo *sf, int pridx, LayerTable *tbl, int level);
static int is_global(SFGenLayer *layer);
static void clear_table(LayerTable *tbl);
static void set_to_table(SFInfo *sf, LayerTable *tbl, SFGenLayer *lay, int level);
static void add_item_to_table(LayerTable *tbl, int oper, int amount, int level);
static void merge_table(SFInfo *sf, LayerTable *dst, LayerTable *src);
static void init_and_merge_table(SFInfo *sf, LayerTable *dst, LayerTable *src);
static int sanity_range(LayerTable *tbl);
static int make_patch(SFInfo *sf, int pridx, LayerTable *tbl);
static void make_info(SFInfo *sf, SampleList *vp, LayerTable *tbl);
static FLOAT_T calc_volume(LayerTable *tbl);
static void set_sample_info(SFInfo *sf, SampleList *vp, LayerTable *tbl);
static void set_init_info(SFInfo *sf, SampleList *vp, LayerTable *tbl);
static int abscent_to_Hz(int abscents);
static void set_rootkey(SFInfo *sf, SampleList *vp, LayerTable *tbl);
static void set_rootfreq(SampleList *vp);
static double to_msec(int timecent);
static int32 to_offset(int offset);
static int32 calc_rate(int diff, double msec);
static int32 calc_sustain(int sust_cB);
static void convert_volume_envelope(SampleList *vp, LayerTable *tbl);
static void convert_tremolo(SampleList *vp, LayerTable *tbl);
static void convert_vibrato(SampleList *vp, LayerTable *tbl);
static void do_lowpass(Sample *sp, int32 freq, FLOAT_T resonance);

/*----------------------------------------------------------------*/

static SFInsts *sfrecs = NULL;
static SFInsts *current_sfrec = NULL;
#define def_drum_inst 0

static SFInsts *find_soundfont(char *sf_file)
{
    SFInsts *sf;

    sf_file = FILENAME_NORMALIZE(sf_file);
    for(sf = sfrecs; sf != NULL; sf = sf->next)
	if(sf->fname != NULL && strcmp(sf->fname, sf_file) == 0)
	    return sf;
    return NULL;
}

static SFInsts *new_soundfont(char *sf_file)
{
    SFInsts *sf;

    sf_file = FILENAME_NORMALIZE(sf_file);
    for(sf = sfrecs; sf != NULL; sf = sf->next)
	if(sf->fname == NULL)
	    break;
    if(sf == NULL)
	sf = (SFInsts *)safe_malloc(sizeof(SFInsts));
    memset(sf, 0, sizeof(SFInsts));
    init_mblock(&sf->pool);
    sf->fname = SFStrdup(sf, FILENAME_NORMALIZE(sf_file));
    sf->def_order = DEFAULT_SOUNDFONT_ORDER;
    sf->amptune = 1.0;
    return sf;
}

void add_soundfont(char *sf_file,
		   int sf_order, int sf_cutoff, int sf_resonance,
		   int amp)
{
    SFInsts *sf;

    if((sf = find_soundfont(sf_file)) != NULL)
    {
	if(sf_order >= 0)
	    sf->def_order = sf_order;
	if(sf_cutoff >= 0)
	    sf->def_cutoff_allowed = sf_cutoff;
	if(sf_resonance >= 0)
	    sf->def_resonance_allowed = sf_resonance;
	if(amp >= 0)
	    sf->amptune = (FLOAT_T)amp * 0.01;
	current_sfrec = sf;
	return;
    }

    sf = new_soundfont(sf_file);
    if(sf_order >= 0)
	sf->def_order = sf_order;
    if(sf_cutoff > 0)
	sf->def_cutoff_allowed = 1;
    if(amp >= 0)
	sf->amptune = (FLOAT_T)amp * 0.01;
    sf->next = sfrecs;
    current_sfrec = sfrecs = sf;
}

void remove_soundfont(char *sf_file)
{
    SFInsts *sf;

    if((sf = find_soundfont(sf_file)) != NULL)
	end_soundfont(sf);
}

char *soundfont_preset_name(int bank, int preset, int keynote,
			    char **sndfile)
{
    SFInsts *rec;
    if(sndfile != NULL)
	*sndfile = NULL;
    for(rec = sfrecs; rec != NULL; rec = rec->next)
	if(rec->fname != NULL)
	{
	    int addr;
	    InstList *ip;

	    addr = INSTHASH(bank, preset, keynote);
	    for(ip = rec->instlist[addr]; ip; ip = ip->next)
		if(ip->pat.bank == bank && ip->pat.preset == preset &&
		   (keynote < 0 || keynote == ip->pat.keynote))
		    break;
	    if(ip != NULL)
	    {
		if(sndfile != NULL)
		    *sndfile = rec->fname;
		return rec->inst_namebuf[ip->pr_idx];
	    }
	}
    return NULL;
}

static void init_sf(SFInsts *rec)
{
	SFInfo sfinfo;
	int i;

	ctl->cmsg(CMSG_INFO, VERB_NOISY, "Init soundfonts `%s'",
		  FILENAME_REDUCED(rec->fname));

	if ((rec->tf = open_file(rec->fname, 1, OF_VERBOSE)) == NULL) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "Can't open soundfont file %s",
			  FILENAME_REDUCED(rec->fname));
		end_soundfont(rec);
		return;
	}

	if(load_soundfont(&sfinfo, rec->tf))
	{
	    end_soundfont(rec);
	    return;
	}

	correct_samples(&sfinfo);
	current_sfrec = rec;
	for (i = 0; i < sfinfo.npresets; i++) {
		int bank = sfinfo.preset[i].bank;
		int preset = sfinfo.preset[i].preset;

		if (bank == 128)
		    alloc_instrument_bank(1, preset);
		else {
			if (is_excluded(rec, bank, preset, -1))
				continue;
			alloc_instrument_bank(0, bank);
		}
		load_font(&sfinfo, i);
	}

	/* copy header info */
	rec->version = sfinfo.version;
	rec->minorversion = sfinfo.minorversion;
	rec->samplepos = sfinfo.samplepos;
	rec->samplesize = sfinfo.samplesize;
	rec->inst_namebuf =
	    (char **)SFMalloc(rec, sfinfo.npresets * sizeof(char *));
	for(i = 0; i < sfinfo.npresets; i++)
	    rec->inst_namebuf[i] =
		(char *)SFStrdup(rec, sfinfo.preset[i].hdr.name);
	free_soundfont(&sfinfo);

#ifndef SF_CLOSE_EACH_FILE
	if(!IS_URL_SEEK_SAFE(rec->tf->url))
#endif
	{
	    close_file(rec->tf);
	    rec->tf = NULL;
	}
}

void init_load_soundfont(void)
{
    SFInsts *rec;
    for(rec = sfrecs; rec != NULL; rec = rec->next)
	if(rec->fname != NULL)
	    init_sf(rec);
}

static void end_soundfont(SFInsts *rec)
{
	if (rec->tf) {
		close_file(rec->tf);
		rec->tf = NULL;
	}

	rec->fname = NULL;
	rec->inst_namebuf = NULL;
	rec->sfexclude = NULL;
	rec->sforder = NULL;
	reuse_mblock(&rec->pool);
}

Instrument *extract_soundfont(char *sf_file, int bank, int preset,
			      int keynote)
{
    SFInsts *sf;

    if((sf = find_soundfont(sf_file)) != NULL)
	return try_load_soundfont(sf, -1, bank, preset, keynote);
    sf = new_soundfont(sf_file);
    sf->next = sfrecs;
    sf->def_order = 2;
    sfrecs = sf;
    init_sf(sf);
    return try_load_soundfont(sf, -1, bank, preset, keynote);
}

/*----------------------------------------------------------------
 * get converted instrument info and load the wave data from file
 *----------------------------------------------------------------*/

static Instrument *try_load_soundfont(SFInsts *rec, int order, int bank,
				      int preset, int keynote)
{
	InstList *ip;
	Instrument *inst = NULL;
	int addr;

	if (rec->tf == NULL) {
		if (rec->fname == NULL)
			return NULL;
		if ((rec->tf = open_file(rec->fname, 1, OF_VERBOSE)) == NULL) {
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				  "Can't open soundfont file %s",
				  FILENAME_REDUCED(rec->fname));
			end_soundfont(rec);
			return NULL;
		}
#ifndef SF_CLOSE_EACH_FILE
		if(!IS_URL_SEEK_SAFE(rec->tf->url))
		    rec->tf->url = url_cache_open(rec->tf->url, 1);
#endif /* SF_CLOSE_EACH_FILE */
	}

	addr = INSTHASH(bank, preset, keynote);
	for (ip = rec->instlist[addr]; ip; ip = ip->next) {
		if (ip->pat.bank == bank && ip->pat.preset == preset &&
		    (keynote < 0 || ip->pat.keynote == keynote) &&
		    (order < 0 || ip->order == order))
			break;
	}

	if (ip && ip->samples)
		inst = load_from_file(rec, ip);

#ifdef SF_CLOSE_EACH_FILE
	close_file(rec->tf);
	rec->tf = NULL;
#endif

	return inst;
}

Instrument *load_soundfont_inst(int order,
				int bank, int preset, int keynote)
{
    SFInsts *rec;
    Instrument *ip;
    for(rec = sfrecs; rec != NULL; rec = rec->next)
    {
	if(rec->fname != NULL)
	{
	    ip = try_load_soundfont(rec, order, bank, preset, keynote);
	    if(ip != NULL)
		return ip;
	}
    }
    return NULL;
}

/*----------------------------------------------------------------*/
#define TO_MHZ(abscents) (int32)(8176.0 * pow(2.0,(double)(abscents)/1200.0))
#if 0
#ifndef M_LN2
#define M_LN2		0.69314718055994530942
#endif /* M_LN2 */
#ifndef M_LN10
#define M_LN10		2.30258509299404568402
#endif /* M_LN10 */
#define TO_VOLUME(centibel) (uint8)(255 * (1.0 - \
				(centibel) * (M_LN10 / 1200.0 / M_LN2)))
#else
#define TO_VOLUME(level)  (uint8)(255.0 - (level) * (255.0/1000.0))
#endif


static FLOAT_T calc_volume(LayerTable *tbl)
{
    int v;
    if(!tbl->set[SF_initAtten] || tbl->val[SF_initAtten] == 0)
	return (FLOAT_T)1.0;
    v = tbl->val[SF_initAtten];
    if(v < 0)
	return (FLOAT_T)1.0;
    if(v > 956)
	return (FLOAT_T)0.0;

    v = v * 127 / 956;		/* 0..127 */

    return vol_table[127 - v];
}

/*
 * convert timecents to sec
 */
static double to_msec(int timecent)
{
    return 1000.0 * pow(2.0, (double)timecent / 1200.0);
}

/* convert from 8bit value to fractional offset (15.15) */
static int32 to_offset(int offset)
{
	return (int32)offset << (7+15);
}

/* calculate ramp rate in fractional unit;
 * diff = 8bit, time = msec
 */
static int32 calc_rate(int diff, double msec)
{
    double rate;

    if(msec < 6)
	msec = 6;
    if(diff == 0)
	diff = 255;
    diff <<= (7+15);
    rate = ((double)diff / play_mode->rate) * control_ratio * 1000.0 / msec;
    if(fast_decay)
	rate *= 2;
    return (int32)rate;
}

/*
 * Sustain level
 * sf: centibels
 * parm: 0x7f - sustain_level(dB) * 0.75
 */
static int32 calc_sustain(int sust_cB)
{
    double level;
    if(sust_cB <= 0)
	return 255;
    level = (double)sust_cB;
    if(level >= 1000)
	return 1;
    return TO_VOLUME(level);
}

static Instrument *load_from_file(SFInsts *rec, InstList *ip)
{
	SampleList *sp;
	Instrument *inst;
	int i;

	if(ip->pat.bank == 128)
	    ctl->cmsg(CMSG_INFO, VERB_NOISY,
		      "Loading SF Drumset %d %d: %s",
		      ip->pat.preset + progbase, ip->pat.keynote,
		      rec->inst_namebuf[ip->pr_idx]);
	else
	    ctl->cmsg(CMSG_INFO, VERB_NOISY,
		      "Loading SF Tonebank %d %d: %s",
		      ip->pat.bank, ip->pat.preset + progbase,
		      rec->inst_namebuf[ip->pr_idx]);
	inst = (Instrument *)safe_malloc(sizeof(Instrument));
	inst->type = INST_SF2;
	inst->samples = ip->samples;
	inst->sample = (Sample *)safe_malloc(sizeof(Sample) * ip->samples);
	memset(inst->sample, 0, sizeof(Sample) * ip->samples);
	for (i = 0, sp = ip->slist; i < ip->samples && sp; i++, sp = sp->next) {
		Sample *sample = inst->sample + i;
		int32 j;
#ifndef LITTLE_ENDIAN
		int32 k;
		int16 *tmp, s;
#endif
		ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			  "[%d] Rate=%d LV=%d HV=%d "
			  "Low=%d Hi=%d Root=%d Pan=%d",
			  sp->start, sp->v.sample_rate, 
			  sp->v.low_vel, sp->v.high_vel, 
			  sp->v.low_freq, sp->v.high_freq, sp->v.root_freq,
			  sp->v.panning);
		memcpy(sample, &sp->v, sizeof(Sample));

		/* convert mHz to control ratio */
		sample->vibrato_control_ratio = sp->vibrato_freq *
		    (VIBRATO_RATE_TUNING * play_mode->rate) /
			(2 * VIBRATO_SAMPLE_INCREMENTS);

		/* convert envelop parameters */
		sample->envelope_offset[0] = to_offset(255);
		sample->envelope_rate[0] = calc_rate(255, sp->attack);

		sample->envelope_offset[1] = to_offset(250);
		sample->envelope_rate[1] = calc_rate(5, sp->hold);

		sample->envelope_offset[2] = to_offset(sp->sustain);
		sample->envelope_rate[2] = calc_rate(250 - sp->sustain, sp->decay);

		sample->envelope_offset[3] = to_offset(5);
		sample->envelope_rate[3] = calc_rate(255, sp->release);

		sample->envelope_offset[4] = to_offset(4);
		sample->envelope_rate[4] = to_offset(200);

		sample->envelope_offset[5] = to_offset(4);
		sample->envelope_rate[5] = to_offset(200);

#if 0
		sample->envelope_offset[3] = to_offset(1);
		sample->envelope_rate[3] = calc_rate(sp->sustain, sp->release);

		sample->envelope_offset[4] = sp->v.envelope_offset[3];
		sample->envelope_rate[4] = sp->v.envelope_rate[3];

		sample->envelope_offset[5] = sp->v.envelope_offset[4];
		sample->envelope_rate[5] = sp->v.envelope_rate[4];
#endif

		if(i > 0 && (sample->note_to_use ||
			     (sample->modes & MODES_LOOPING)))
		{
		    SampleList *sps;
		    Sample *found, *s;

		    found = NULL;
		    for(j = 0, sps = ip->slist, s = inst->sample; j < i && sps;
			j++, sps = sps->next, s++)
		    {
			if(s->data == NULL)
			    break;
			if(sp->start == sps->start)
			{
			    if(sp->cutoff_freq != sps->cutoff_freq ||
			       sp->resonance != sps->cutoff_freq)
				continue;
			    if(antialiasing_allowed)
			    {
				 if(sample->data_length != s->data_length ||
				    sample->sample_rate != s->sample_rate)
				     continue;
			    }
			    if(s->note_to_use && !(s->modes & MODES_LOOPING))
				continue;
			    found = s;
			    break;
			}
		    }
		    if(found)
		    {
			sample->data = found->data;
			sample->data_alloced = 0;
			ctl->cmsg(CMSG_INFO, VERB_DEBUG, " * Cached");
			continue;
		    }
		}

		sample->data = (sample_t *)safe_malloc(sp->len);
		sample->data_alloced = 1;

		ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
			  "Data: %d %d V=%g, envofs: %d %d %d %d %d %d",
			  sp->start, sp->start + sp->len,
			  sample->volume,
			  sample->envelope_offset[0] >> 22,
			  sample->envelope_offset[1] >> 22,
			  sample->envelope_offset[2] >> 22,
			  sample->envelope_offset[3] >> 22,
			  sample->envelope_offset[4] >> 22,
			  sample->envelope_offset[5] >> 22);

		tf_seek(rec->tf, sp->start, SEEK_SET);
		tf_read(sample->data, sp->len, 1, rec->tf);
#ifndef LITTLE_ENDIAN
		tmp = (int16*)sample->data;
		k = sp->len/2;
		for (j = 0; j < k; j++) {
			s = LE_SHORT(*tmp);
			*tmp++ = s;
		}
#endif

		/* do some filtering if necessary */
		if (sp->cutoff_freq > 0) {
			/* restore the normal value */
			sample->data_length >>= FRACTION_BITS;
			ctl->cmsg(CMSG_INFO, VERB_DEBUG,
				  " * Filter: cutoff=%dHz resonance=%g",
				  ip->pat.bank, ip->pat.preset, ip->pat.keynote, sp->cutoff_freq, sp->resonance);
			do_lowpass(sample, sp->cutoff_freq, sp->resonance);
			/* convert again to the fractional value */
			sample->data_length <<= FRACTION_BITS;
		}

		if (antialiasing_allowed)
		    antialiasing((int16 *)sample->data,
				 sample->data_length >> FRACTION_BITS,
				 sample->sample_rate,
				 play_mode->rate);

		/* resample it if possible */
		if (sample->note_to_use && !(sample->modes & MODES_LOOPING))
			pre_resample(sample);

#ifdef LOOKUP_HACK
		/* squash the 16-bit data into 8 bits. */
		{
			uint8 *gulp,*ulp;
			int16 *swp;
			int l = sample->data_length >> FRACTION_BITS;
			gulp = ulp = (uint8 *)safe_malloc(l + 1);
			swp = (int16 *)sample->data;
			while (l--)
				*ulp++ = (*swp++ >> 8) & 0xFF;
			free(sample->data);
			sample->data=(sample_t *)gulp;
		}
#endif
	}
	return inst;
}


/*----------------------------------------------------------------
 * excluded samples
 *----------------------------------------------------------------*/

int exclude_soundfont(int bank, int preset, int keynote)
{
	SFExclude *exc;
	if(current_sfrec == NULL)
	    return 1;
	exc = (SFExclude*)SFMalloc(current_sfrec , sizeof(SFExclude));
	exc->pat.bank = bank;
	exc->pat.preset = preset;
	exc->pat.keynote = keynote;
	exc->next = current_sfrec->sfexclude;
	current_sfrec->sfexclude = exc;
	return 0;
}

/* check the instrument is specified to be excluded */
static int is_excluded(SFInsts *rec, int bank, int preset, int keynote)
{
	SFExclude *p;
	for (p = rec->sfexclude; p; p = p->next) {
		if (p->pat.bank == bank &&
		    (p->pat.preset < 0 || p->pat.preset == preset) &&
		    (p->pat.keynote < 0 || p->pat.keynote == keynote))
			return 1;
	}
	return 0;
}


/*----------------------------------------------------------------
 * ordered samples
 *----------------------------------------------------------------*/

int order_soundfont(int bank, int preset, int keynote, int order)
{
	SFOrder *p;
	if(current_sfrec == NULL)
	    return 1;
	p = (SFOrder*)SFMalloc(current_sfrec, sizeof(SFOrder));
	p->pat.bank = bank;
	p->pat.preset = preset;
	p->pat.keynote = keynote;
	p->order = order;
	p->next = current_sfrec->sforder;
	current_sfrec->sforder = p;
	return 0;
}

/* check the instrument is specified to be ordered */
static int is_ordered(SFInsts *rec, int bank, int preset, int keynote)
{
	SFOrder *p;
	for (p = rec->sforder; p; p = p->next) {
		if (p->pat.bank == bank &&
		    (p->pat.preset < 0 || p->pat.preset == preset) &&
		    (p->pat.keynote < 0 || p->pat.keynote == keynote))
			return p->order;
	}
	return -1;
}


/*----------------------------------------------------------------*/

static int load_font(SFInfo *sf, int pridx)
{
	SFPresetHdr *preset = &sf->preset[pridx];
	int rc, j, nlayers;
	SFGenLayer *layp, *globalp;

	/* if layer is empty, skip it */
	if ((nlayers = preset->hdr.nlayers) <= 0 ||
	    (layp = preset->hdr.layer) == NULL)
		return AWE_RET_SKIP;
	/* check global layer */
	globalp = NULL;
	if (is_global(layp)) {
		globalp = layp;
		layp++;
		nlayers--;
	}
	/* parse for each preset layer */
	for (j = 0; j < nlayers; j++, layp++) {
		LayerTable tbl;

		/* set up table */
		clear_table(&tbl);
		if (globalp)
			set_to_table(sf, &tbl, globalp, P_GLOBAL);
		set_to_table(sf, &tbl, layp, P_LAYER);
		
		/* parse the instrument */
		rc = parse_layer(sf, pridx, &tbl, 0);
		if(rc == AWE_RET_ERR || rc == AWE_RET_NOMEM)
			return rc;
	}

	return AWE_RET_OK;
}


/*----------------------------------------------------------------*/

/* parse a preset layer and convert it to the patch structure */
static int parse_layer(SFInfo *sf, int pridx, LayerTable *tbl, int level)
{
	SFInstHdr *inst;
	int rc, i, nlayers;
	SFGenLayer *lay, *globalp;
#if 0
	SFPresetHdr *preset = &sf->preset[pridx];
#endif

	if (level >= 2) {
		fprintf(stderr, "parse_layer: too deep instrument level\n");
		return AWE_RET_ERR;
	}

	/* instrument must be defined */
	if (!tbl->set[SF_instrument])
		return AWE_RET_SKIP;

	inst = &sf->inst[tbl->val[SF_instrument]];

	/* Here, TiMidity makes the reference of the data.  The real data
	 * is loaded after.  So, duplicated data is allowed */
#if 0
	/* if non-standard drumset includes standard drum instruments,
	   skip it to avoid duplicate the data */
	if (def_drum_inst >= 0 && preset->bank == 128 && preset->preset != 0 &&
	    tbl->val[SF_instrument] == def_drum_inst)
			return AWE_RET_SKIP;
#endif

	/* if layer is empty, skip it */
	if ((nlayers = inst->hdr.nlayers) <= 0 ||
	    (lay = inst->hdr.layer) == NULL)
		return AWE_RET_SKIP;

	/* check global layer */
	globalp = NULL;
	if (is_global(lay)) {
		globalp = lay;
		lay++;
		nlayers--;
	}

	/* parse for each layer */
	for (i = 0; i < nlayers; i++, lay++) {
		LayerTable ctbl;
		clear_table(&ctbl);
		if (globalp)
			set_to_table(sf, &ctbl, globalp, P_GLOBAL);
		set_to_table(sf, &ctbl, lay, P_LAYER);

		if (!ctbl.set[SF_sampleId]) {
			/* recursive loading */
			merge_table(sf, &ctbl, tbl);
			if (! sanity_range(&ctbl))
				continue;
			rc = parse_layer(sf, pridx, &ctbl, level+1);
			if (rc != AWE_RET_OK && rc != AWE_RET_SKIP)
				return rc;
		} else {
			init_and_merge_table(sf, &ctbl, tbl);
			if (! sanity_range(&ctbl))
				continue;

			/* load the info data */
			if ((rc = make_patch(sf, pridx, &ctbl)) == AWE_RET_ERR)
				return rc;
		}
	}
	return AWE_RET_OK;
}


static int is_global(SFGenLayer *layer)
{
	int i;
	for (i = 0; i < layer->nlists; i++) {
		if (layer->list[i].oper == SF_instrument ||
		    layer->list[i].oper == SF_sampleId)
			return 0;
	}
	return 1;
}


/*----------------------------------------------------------------
 * layer table handlers
 *----------------------------------------------------------------*/

/* initialize layer table */
static void clear_table(LayerTable *tbl)
{
	memset(tbl->val, 0, sizeof(tbl->val));
	memset(tbl->set, 0, sizeof(tbl->set));
}

/* set items in a layer to the table */
static void set_to_table(SFInfo *sf, LayerTable *tbl, SFGenLayer *lay, int level)
{
	int i;
	for (i = 0; i < lay->nlists; i++) {
		SFGenRec *gen = &lay->list[i];
		/* copy the value regardless of its copy policy */
		tbl->val[gen->oper] = gen->amount;
		tbl->set[gen->oper] = level;
	}
}

/* add an item to the table */
static void add_item_to_table(LayerTable *tbl, int oper, int amount, int level)
{
	LayerItem *item = &layer_items[oper];
	int o_lo, o_hi, lo, hi;

	switch (item->copy) {
	case L_INHRT:
		tbl->val[oper] += amount;
		break;
	case L_OVWRT:
		tbl->val[oper] = amount;
		break;
	case L_PRSET:
	case L_INSTR:
		/* do not overwrite */
		if (!tbl->set[oper])
			tbl->val[oper] = amount;
		break;
	case L_RANGE:
		if (!tbl->set[oper]) {
			tbl->val[oper] = amount;
		} else {
			o_lo = LOWNUM(tbl->val[oper]);
			o_hi = HIGHNUM(tbl->val[oper]);
			lo = LOWNUM(amount);
			hi = HIGHNUM(amount);
			if (lo < o_lo) lo = o_lo;
			if (hi > o_hi) hi = o_hi;
			tbl->val[oper] = RANGE(lo, hi);
		}
		break;
	}
}

/* merge two tables */
static void merge_table(SFInfo *sf, LayerTable *dst, LayerTable *src)
{
	int i;
	for (i = 0; i < SF_EOF; i++) {
		if (src->set[i]) {
			if (sf->version == 1) {
				if (!dst->set[i] ||
				    i == SF_keyRange || i == SF_velRange)
					/* just copy it */
					dst->val[i] = src->val[i];
			}
			else
				add_item_to_table(dst, i, src->val[i], P_GLOBAL);
			dst->set[i] = P_GLOBAL;
		}
	}
}

/* merge and set default values */
static void init_and_merge_table(SFInfo *sf, LayerTable *dst, LayerTable *src)
{
	int i;

	/* default value is not zero */
	if (sf->version == 1) {
		layer_items[SF_sustainEnv1].defv = 1000;
		layer_items[SF_sustainEnv2].defv = 1000;
		layer_items[SF_freqLfo1].defv = -725;
		layer_items[SF_freqLfo2].defv = -15600;
	} else {
		layer_items[SF_sustainEnv1].defv = 0;
		layer_items[SF_sustainEnv2].defv = 0;
		layer_items[SF_freqLfo1].defv = 0;
		layer_items[SF_freqLfo2].defv = 0;
	}

	/* set default */
	for (i = 0; i < SF_EOF; i++) {
		if (!dst->set[i])
			dst->val[i] = layer_items[i].defv;
	}
	merge_table(sf, dst, src);
	/* convert from SBK to SF2 */
	if (sf->version == 1) {
		for (i = 0; i < SF_EOF; i++) {
			if (dst->set[i])
				dst->val[i] = sbk_to_sf2(i, dst->val[i]);
		}
	}
}


/*----------------------------------------------------------------
 * check key and velocity range
 *----------------------------------------------------------------*/

static int sanity_range(LayerTable *tbl)
{
	int lo, hi;

	lo = LOWNUM(tbl->val[SF_keyRange]);
	hi = HIGHNUM(tbl->val[SF_keyRange]);
	if (lo < 0 || lo > 127 || hi < 0 || hi > 127 || hi < lo)
		return 0;

	lo = LOWNUM(tbl->val[SF_velRange]);
	hi = HIGHNUM(tbl->val[SF_velRange]);
	if (lo < 0 || lo > 127 || hi < 0 || hi > 127 || hi < lo)
		return 0;

	return 1;
}


/*----------------------------------------------------------------
 * create patch record from the stored data table
 *----------------------------------------------------------------*/

#if 0
static int make_patch(SFInfo *sf, int pridx, LayerTable *tbl)
{
    int bank, preset, keynote;
    int addr, order;
    InstList *ip;
    SFSampleInfo *sample;
    SampleList *sp;

    sample = &sf->sample[tbl->val[SF_sampleId]];
    if(sample->sampletype & 0x8000) /* is ROM sample? */
    {
	ctl->cmsg(CMSG_INFO, VERB_DEBUG, "preset %d is ROM sample: 0x%x",
		  pridx, sample->sampletype);
	return AWE_RET_SKIP;
    }

    bank = sf->preset[pridx].bank;
    preset = sf->preset[pridx].preset;
    if(bank == 128)
	keynote = LOWNUM(tbl->val[SF_keyRange]);
    else
	keynote = -1;

    ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
	      "SF make inst pridx=%d bank=%d preset=%d keynote=%d",
	      pridx, bank, preset, keynote);

    if(is_excluded(current_sfrec, bank, preset, keynote))
    {
	ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY, " * Excluded");
	return AWE_RET_SKIP;
    }

    order = is_ordered(current_sfrec, bank, preset, keynote);
    if(order < 0)
	order = current_sfrec->def_order;

    addr = INSTHASH(bank, preset, keynote);

    for(ip = current_sfrec->instlist[addr]; ip; ip = ip->next)
    {
	if(ip->pat.bank == bank && ip->pat.preset == preset &&
	   (keynote < 0 || keynote == ip->pat.keynote))
	    break;
    }

    if(ip == NULL)
    {
	ip = (InstList*)SFMalloc(current_sfrec, sizeof(InstList));
	memset(ip, 0, sizeof(InstList));
	ip->pr_idx = pridx;
	ip->pat.bank = bank;
	ip->pat.preset = preset;
	ip->pat.keynote = keynote;
	ip->order = order;
	ip->samples = 0;
	ip->slist = NULL;
	ip->next = current_sfrec->instlist[addr];
	current_sfrec->instlist[addr] = ip;
    }

    /* new sample */
    sp = (SampleList *)SFMalloc(current_sfrec, sizeof(SampleList));
    memset(sp, 0, sizeof(SampleList));

    if(bank == 128)
	sp->v.note_to_use = keynote;
    sp->v.high_vel = 127;
    make_info(sf, sp, tbl);

    /* add a sample */
    if(ip->slist == NULL)
	ip->slist = sp;
    else
    {
	SampleList *cur, *prev;
	int32 start;

	/* Insert sample */
	start = sp->start;
	cur = ip->slist;
	prev = NULL;
	while(cur && cur->start <= start)
	{
	    prev = cur;
	    cur = cur->next;
	}
	if(prev == NULL)
	{
	    sp->next = ip->slist;
	    ip->slist = sp;
	}
	else
	{
	    prev->next = sp;
	    sp->next = cur;
	}
    }
    ip->samples++;

    return AWE_RET_OK;
}
#else
static int make_patch(SFInfo *sf, int pridx, LayerTable *tbl)
{
    int bank, preset, keynote;
    int keynote_from, keynote_to, done;
    int addr, order;
    InstList *ip;
    SFSampleInfo *sample;
    SampleList *sp;

    sample = &sf->sample[tbl->val[SF_sampleId]];
    if(sample->sampletype & 0x8000) /* is ROM sample? */
    {
	ctl->cmsg(CMSG_INFO, VERB_DEBUG, "preset %d is ROM sample: 0x%x",
		  pridx, sample->sampletype);
	return AWE_RET_SKIP;
    }

    bank = sf->preset[pridx].bank;
    preset = sf->preset[pridx].preset;
    if(bank == 128){
		keynote_from = LOWNUM(tbl->val[SF_keyRange]);
		keynote_to = HIGHNUM(tbl->val[SF_keyRange]);
    } else
	keynote_from = keynote_to = -1;

	done = 0;
	for(keynote=keynote_from;keynote<=keynote_to;keynote++){

    ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
	      "SF make inst pridx=%d bank=%d preset=%d keynote=%d",
	      pridx, bank, preset, keynote);

    if(is_excluded(current_sfrec, bank, preset, keynote))
    {
	ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY, " * Excluded");
	continue;
    } else
	done++;

    order = is_ordered(current_sfrec, bank, preset, keynote);
    if(order < 0)
	order = current_sfrec->def_order;

    addr = INSTHASH(bank, preset, keynote);

    for(ip = current_sfrec->instlist[addr]; ip; ip = ip->next)
    {
	if(ip->pat.bank == bank && ip->pat.preset == preset &&
	   (keynote < 0 || keynote == ip->pat.keynote))
	    break;
    }

    if(ip == NULL)
    {
	ip = (InstList*)SFMalloc(current_sfrec, sizeof(InstList));
	memset(ip, 0, sizeof(InstList));
	ip->pr_idx = pridx;
	ip->pat.bank = bank;
	ip->pat.preset = preset;
	ip->pat.keynote = keynote;
	ip->order = order;
	ip->samples = 0;
	ip->slist = NULL;
	ip->next = current_sfrec->instlist[addr];
	current_sfrec->instlist[addr] = ip;
    }

    /* new sample */
    sp = (SampleList *)SFMalloc(current_sfrec, sizeof(SampleList));
    memset(sp, 0, sizeof(SampleList));

    if(bank == 128)
	sp->v.note_to_use = keynote;
    sp->v.high_vel = 127;
    make_info(sf, sp, tbl);

    /* add a sample */
    if(ip->slist == NULL)
	ip->slist = sp;
    else
    {
	SampleList *cur, *prev;
	int32 start;

	/* Insert sample */
	start = sp->start;
	cur = ip->slist;
	prev = NULL;
	while(cur && cur->start <= start)
	{
	    prev = cur;
	    cur = cur->next;
	}
	if(prev == NULL)
	{
	    sp->next = ip->slist;
	    ip->slist = sp;
	}
	else
	{
	    prev->next = sp;
	    sp->next = cur;
	}
    }
    ip->samples++;

	} /* for(;;) */

	if(done==0)
	return AWE_RET_SKIP;
	else
	return AWE_RET_OK;
}
#endif

/*----------------------------------------------------------------
 *
 * Modified for TiMidity
 */

/* conver to Sample parameter */
static void make_info(SFInfo *sf, SampleList *vp, LayerTable *tbl)
{
	set_sample_info(sf, vp, tbl);
	set_init_info(sf, vp, tbl);
	set_rootkey(sf, vp, tbl);
	set_rootfreq(vp);

	/* tremolo & vibrato */
#ifndef SF_SUPPRESS_TREMOLO
	convert_tremolo(vp, tbl);
#endif /* SF_SUPPRESS_TREMOLO */

#ifndef SF_SUPPRESS_VIBRATO
	convert_vibrato(vp, tbl);
#endif /* SF_SUPPRESS_VIBRATO */
}

/* set sample address */
static void set_sample_info(SFInfo *sf, SampleList *vp, LayerTable *tbl)
{
    SFSampleInfo *sp = &sf->sample[tbl->val[SF_sampleId]];

    /* set sample position */
    vp->start = (tbl->val[SF_startAddrsHi] << 15)
	+ tbl->val[SF_startAddrs]
	+ sp->startsample;
    vp->len = (tbl->val[SF_endAddrsHi] << 15)
	+ tbl->val[SF_endAddrs]
	+ sp->endsample - vp->start;

    /* set loop position */
    vp->v.loop_start = (tbl->val[SF_startloopAddrsHi] << 15)
	+ tbl->val[SF_startloopAddrs]
	+ sp->startloop - vp->start;
    vp->v.loop_end = (tbl->val[SF_endloopAddrsHi] << 15)
	+ tbl->val[SF_endloopAddrs]
	+ sp->endloop - vp->start;

    /* set data length */
    vp->v.data_length = vp->len;
    if(vp->v.loop_end > vp->len)
	vp->v.loop_end = vp->len;

    /* Sample rate */
    vp->v.sample_rate = sp->samplerate;

    /* sample mode */
    vp->v.modes = MODES_16BIT;

    /* volume envelope & total volume */
    vp->v.volume = calc_volume(tbl) * current_sfrec->amptune;
    if(tbl->val[SF_sampleFlags] == 1 || tbl->val[SF_sampleFlags] == 3)
    {
	/* looping */
	vp->v.modes |= MODES_LOOPING|MODES_SUSTAIN;
#ifndef SF_SUPPRESS_ENVELOPE
	convert_volume_envelope(vp, tbl);
#endif /* SF_SUPPRESS_ENVELOPE */
	if(tbl->val[SF_sampleFlags] == 3)
	    vp->v.data_length = vp->v.loop_end; /* strip the tail */
    }
    else
    {
#if 0 /* What?? */
	/* short-shot; set a small blank loop at the tail */
	if (sp->loopshot > 8) {
	    vp->loopstart = sp->endsample + 8 - sp->startloop;
	    vp->loopend = sp->endsample + sp->loopshot - 8 - sp->endloop;
	} else {
	    fprintf(stderr, "loop size is too short: %d\n", sp->loopshot);
	    exit(1);
	}
#endif
    }

    /* convert to fractional samples */
    vp->v.data_length <<= FRACTION_BITS;
    vp->v.loop_start <<= FRACTION_BITS;
    vp->v.loop_end <<= FRACTION_BITS;

    /* point to the file position */
    vp->start = vp->start * 2 + sf->samplepos;
    vp->len *= 2;
}

/*----------------------------------------------------------------*/

/* set global information */
static void set_init_info(SFInfo *sf, SampleList *vp, LayerTable *tbl)
{
    int val;

    /* key range */
    if(tbl->set[SF_keyRange])
    {
	vp->low = LOWNUM(tbl->val[SF_keyRange]);
	vp->high = HIGHNUM(tbl->val[SF_keyRange]);
    }
    else
    {
	vp->low = 0;
	vp->high = 127;
    }
    vp->v.low_freq = freq_table[(int)vp->low];
    vp->v.high_freq = freq_table[(int)vp->high];

    /* velocity range */
    if(tbl->set[SF_velRange])
    {
	vp->v.low_vel = LOWNUM(tbl->val[SF_velRange]);
	vp->v.high_vel = HIGHNUM(tbl->val[SF_velRange]);
    }

    /* fixed key & velocity */
    if(tbl->set[SF_keynum])
	vp->v.note_to_use = tbl->val[SF_keynum];
#if 0 /* Not supported */
    vp->fixvel = tbl->val[SF_velocity];
#endif

    /* panning position: 0 to 127 */
    val = (int)tbl->val[SF_panEffectsSend];
    if(val < -500)
	vp->v.panning = 0;
    else if(val > 500)
	vp->v.panning = 127;
    else
	vp->v.panning = (int8)((val + 500) * 127 / 1000);
    /* vp->fixpan = -1; */

#if 0 /* Not supported */

    /* initial volume */

    vp->amplitude = awe_option.default_volume * 127 / 100;
    /* this is not a centibel? */
    vp->attenuation = awe_calc_attenuation((int)(tbl->val[SF_initAtten] / awe_option.atten_sense));
	
    /* chorus & reverb effects */
    if (tbl->set[SF_chorusEffectsSend])
	vp->parm.chorus = awe_calc_chorus(tbl->val[SF_chorusEffectsSend]);
    else
	vp->parm.chorus = awe_option.default_chorus * 255 / 100;
    if (tbl->set[SF_reverbEffectsSend])
	vp->parm.reverb = awe_calc_reverb(tbl->val[SF_chorusEffectsSend]);
    else
	vp->parm.reverb = awe_option.default_reverb * 255 / 100;
#endif

    /* initial cutoff & resonance */
    vp->cutoff_freq = 0;
    if(tbl->val[SF_initialFilterFc] < 0)
	tbl->set[SF_initialFilterFc] = tbl->val[SF_initialFilterFc] = 0;
    if(current_sfrec->def_cutoff_allowed &&
       (tbl->set[SF_initialFilterFc] || tbl->set[SF_env1ToFilterFc]))
    {
	if(!tbl->set[SF_initialFilterFc])
	    val = 13500;
	else
	    val = tbl->val[SF_initialFilterFc];

	if(tbl->set[SF_env1ToFilterFc])
	    val += tbl->val[SF_env1ToFilterFc];
	vp->cutoff_freq = abscent_to_Hz(val);
    }

    vp->resonance = 0;
    if(current_sfrec->def_resonance_allowed && tbl->set[SF_initialFilterQ])
    {
	val = tbl->val[SF_initialFilterQ];
	vp->resonance = pow(10.0, (double)val / 2.0 / 200.0) - 1;
	if(vp->resonance < 0)
	    vp->resonance = 0;
    }

#if 0 /* Not supported */
    /* exclusive class key */
    vp->exclusiveClass = tbl->val[SF_keyExclusiveClass];
#endif
}

static int abscent_to_Hz(int abscents)
{
	return (int)(8.176 * pow(2.0, (double)abscents / 1200.0));
}

/*----------------------------------------------------------------*/

/* calculate root key & fine tune */
static void set_rootkey(SFInfo *sf, SampleList *vp, LayerTable *tbl)
{
    SFSampleInfo *sp = &sf->sample[tbl->val[SF_sampleId]];

    /* scale tuning */
    vp->scaleTuning = tbl->val[SF_scaleTuning];

    /* set initial root key & fine tune */
    if(sf->version == 1 && tbl->set[SF_samplePitch])
    {
	/* set from sample pitch */
	vp->root = tbl->val[SF_samplePitch] / 100;
	vp->tune = -tbl->val[SF_samplePitch] % 100;
	if(vp->tune <= -50)
	{
	    vp->root++;
	    vp->tune = 100 + vp->tune;
	}
	if(vp->scaleTuning == 50)
	    vp->tune /= 2;
    }
    else
    {
	/* from sample info */
	vp->root = sp->originalPitch;
	vp->tune = sp->pitchCorrection;
    }

    /* orverride root key */
    if(tbl->set[SF_rootKey])
	vp->root += tbl->val[SF_rootKey] - sp->originalPitch;

    vp->tune += tbl->val[SF_coarseTune] * vp->scaleTuning +
	(int)tbl->val[SF_fineTune] * (int)vp->scaleTuning / 100;

    /* correct too high pitch */
    if(vp->root >= vp->high + 60)
	vp->root -= 60;
}

static void set_rootfreq(SampleList *vp)
{
    int root, tune;

    root = vp->root;
    tune = vp->tune;

    while(tune <= -100)
    {
	root++;
	tune += 100;
    }
    while(tune > 0)
    {
	root--;
	tune -= 100;
    }

    /* -100 < tune <= 0 */
    tune = (-tune * 256) / 100;

    if(root > 127)
	vp->v.root_freq = (int32)((FLOAT_T)freq_table[127] *
				  bend_coarse[root - 127] * bend_fine[tune]);
				  
    else if(root < 0)
	vp->v.root_freq = (int32)((FLOAT_T)freq_table[0] /
				  bend_coarse[-root] * bend_fine[tune]);
    else
	vp->v.root_freq = (int32)((FLOAT_T)freq_table[root] * bend_fine[tune]);
}

/*----------------------------------------------------------------*/


/*Pseudo Reverb*/
extern int32 modify_release;


/* volume envelope parameters */
static void convert_volume_envelope(SampleList *vp, LayerTable *tbl)
{
    vp->attack  = to_msec(tbl->val[SF_attackEnv2]);
    vp->hold    = to_msec(tbl->val[SF_holdEnv2]);
    vp->sustain = calc_sustain(tbl->val[SF_sustainEnv2]);
    if(vp->sustain > 250)
	vp->sustain = 250;
    vp->decay   = to_msec(tbl->val[SF_decayEnv2]);
    if(modify_release)
	vp->release = modify_release;
    else
	vp->release = to_msec(tbl->val[SF_releaseEnv2]); /* Pseudo Reverb */

#if 0 /* Not supported */
    /* key hold/decay */
    vp->parm.volkeyhold = tbl->val[SF_autoHoldEnv2];
    vp->parm.volkeydecay = tbl->val[SF_autoDecayEnv2];
#endif

    vp->v.modes |= MODES_ENVELOPE;
}


#ifndef SF_SUPPRESS_TREMOLO
/*----------------------------------------------------------------
 * tremolo (LFO1) conversion
 *----------------------------------------------------------------*/

static void convert_tremolo(SampleList *vp, LayerTable *tbl)
{
    int32 level, freq;

    if(!tbl->set[SF_lfo1ToVolume])
	return;

    level = tbl->val[SF_lfo1ToVolume];
    level = (level * 0x80) / 120;
    if(level < -128)
	level = -128;
    if(level > 127)
	level = 127;
    if(level < 0)
	level += 0x100;
    vp->v.tremolo_depth = (uint8)level;

    /* frequency in mHz */
    if(!tbl->set[SF_freqLfo1])
	freq = 0;
    else
    {
	freq = tbl->val[SF_freqLfo1];
	freq = TO_MHZ(freq);
    }

    /* convert mHz to sine table increment; 1024<<rate_shift=1wave */
    vp->v.tremolo_phase_increment = (freq * 1024) << RATE_SHIFT;
    vp->v.tremolo_sweep_increment = 0;
}
#endif

#ifndef SF_SUPPRESS_VIBRATO
/*----------------------------------------------------------------
 * vibrato (LFO2) conversion
 *----------------------------------------------------------------*/

static void convert_vibrato(SampleList *vp, LayerTable *tbl)
{
    int32 shift, freq;

    if(!tbl->set[SF_lfo2ToPitch])
	return;

    shift = tbl->val[SF_lfo2ToPitch];

    /* cents to linear; 400cents = 256 */
    shift = shift * 256 / 400;
    if(shift < 0)
	shift += 256;
    vp->v.vibrato_depth = (uint8)shift;

    /* frequency in mHz */
    if(!tbl->set[SF_freqLfo2])
	freq = 0;
    else
    {
	freq = tbl->val[SF_freqLfo2];
	freq = TO_MHZ(freq);
    }
    vp->vibrato_freq = freq;
    vp->v.vibrato_sweep_increment = 0;
}
#endif


/*----------------------------------------------------------------
 * low-pass filter:
 * 	y(n) = A * x(n) + B * y(n-1)
 * 	A = 2.0 * pi * center
 * 	B = exp(-A / frequency)
 *----------------------------------------------------------------
 * resonance filter:
 *	y(n) = a * x(n) - b * y(n-1) - c * y(n-2)
 *	c = exp(-2 * pi * width / rate)
 *	b = -4 * c / (1+c) * cos(2 * pi * center / rate)
 *	a = sqt(1-b*b/(4 * c)) * (1-c)
 *----------------------------------------------------------------*/

#ifdef LOOKUP_HACK
#define MAX_DATAVAL 127
#define MIN_DATAVAL -128
#else
#define MAX_DATAVAL 32767
#define MIN_DATAVAL -32768
#endif

/* ## FIXME */
static void do_lowpass(Sample *sp, int32 freq, FLOAT_T resonance)
{
	double A, B, C;
	sample_t *buf, pv1, pv2;
	int32 i;

	if (freq > sp->sample_rate * 2) {
		ctl->cmsg(CMSG_WARNING, VERB_NORMAL,
			  "Lowpass: center must be < data rate*2");
		return;
	}
	A = 2.0 * M_PI * freq * 2.5 / sp->sample_rate;
	B = exp(-A / sp->sample_rate);
	A *= 0.8;
	B *= 0.8;
	C = 0;

	if (resonance) {
		double a, b, c;
		int32 width;
		width = freq / 5;
		c = exp(-2.0 * M_PI * width / sp->sample_rate);
		b = -4.0 * c / (1+c) * cos(2.0 * M_PI * freq / sp->sample_rate);
		a = sqrt(1 - b * b / (4 * c)) * (1 - c);
		b = -b; c = -c;

		A += a * resonance;
		B += b;
		C = c;
	}

	pv1 = 0;
	pv2 = 0;
	buf = sp->data;

	for (i = 0; i < sp->data_length; i++) {
		sample_t l = *buf;
		double d = A * l + B * pv1 + C * pv2;
		if (d > MAX_DATAVAL)
			d = MAX_DATAVAL;
		else if (d < MIN_DATAVAL)
			d = MIN_DATAVAL;
		pv2 = pv1;
#ifndef CUTOFF_AMPTUNING
		pv1 = *buf++ = (sample_t)d;
#else
		pv1 = (sample_t)d;
		*buf++ = (sample_t)(d * CUTOFF_AMPTUNING);
#endif
	}
}
