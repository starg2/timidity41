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

    mix.c
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "interface.h"
#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "output.h"
#include "controls.h"
#include "tables.h"
#include "resample.h"
#include "mix.h"

#include "thread.h"
#include "int_synth.h"


///r 
#ifdef __BORLANDC__
#define inline
#endif

#define OFFSET_MAX (0x3FFFFFFFL)

#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)

#ifdef EFFECT_LEVEL_FLOAT // effect level float // not yet
#define MAX_AMP_VALUE ((1<<(AMP_BITS+1))-1)
#define MIN_AMP_VALUE (MAX_AMP_VALUE >> 9)
#define FINAL_VOLUME(v) (v)
#define MIXATION(a) *lp++ += (a) * s
#else // effect level int
#define MAX_AMP_VALUE ((1<<(AMP_BITS+1))-1)
#define MIN_AMP_VALUE (MAX_AMP_VALUE >> 9)
const FLOAT_T sample_level = ((double)(1 << (SAMPLE_BITS - 1)));
#define FINAL_VOLUME(v) (v * sample_level)
#define MIXATION(a) *lp++ += (a) * s
#endif
const final_volume_t mul_amp = 1U << AMP_BITS;
const final_volume_t max_amp_value = MAX_AMP_VALUE;

#else // DATA_T_INT32

#ifdef LOOKUP_HACK
#define MAX_AMP_VALUE 4095
#define MIN_AMP_VALUE (MAX_AMP_VALUE >> 9)
#define FINAL_VOLUME(v) ((final_volume_t)~_l2u[v])
#define MIXATION(a) *lp++ += mixup[(a << 8) | (uint8) s]
#else
#define MAX_AMP_VALUE ((1<<(AMP_BITS+1))-1)
#define MIN_AMP_VALUE (MAX_AMP_VALUE >> 9)
#define FINAL_VOLUME(v) (v)
#define MIXATION(a) *lp++ += (a) * s
#endif
const final_volume_t mul_amp = 1U << AMP_BITS;
int32 amp_bits = AMP_BITS;
const int32 max_amp_value = MAX_AMP_VALUE;

#endif // DATA_T_INT32


void mix_voice(DATA_T *, int, int32);


#if 0 // dim voice buffer
static ALIGN DATA_T voice_buffer[AUDIO_BUFFER_SIZE * RESAMPLE_OVER_SAMPLING_MAX];
#else // malloc voice buffer
static DATA_T *voice_buffer = NULL;
#endif // malloc voice buffer


/*************** mix.c initialize uninitialize *****************/

#ifdef MULTI_THREAD_COMPUTE
void init_thread_mix_c(void);
void free_thread_mix_c(void);
#endif

void init_mix_c(void)
{	
#ifdef ALIGN_SIZE
	const int min_compute_sample = 8;
	int byte = ((compute_buffer_size + min_compute_sample) * RESAMPLE_OVER_SAMPLING_MAX) * sizeof(DATA_T);
#else
	int byte = compute_buffer_size * RESAMPLE_OVER_SAMPLING_MAX * sizeof(DATA_T);
#endif

	if(byte < (AUDIO_BUFFER_SIZE * sizeof(DATA_T)))
		byte = AUDIO_BUFFER_SIZE * sizeof(DATA_T);
	
#if 0 // dim voice_buffer

#else // malloc voice_buffer
#ifdef ALIGN_SIZE
	if(voice_buffer){
		aligned_free(voice_buffer);
		voice_buffer = NULL;
	}
	voice_buffer = (DATA_T *)aligned_malloc(byte, ALIGN_SIZE);
#else
	if(voice_buffer){
		safe_free(voice_buffer);
		voice_buffer = NULL;
	}
	voice_buffer = (DATA_T *)safe_malloc(byte);
#endif
#endif // malloc voice_buffer
	memset(voice_buffer, 0, byte);

#ifdef MULTI_THREAD_COMPUTE
	init_thread_mix_c();
#endif		
}

void free_mix_c(void)
{
#if 0 // dim voice_buffer

#else // malloc voice_buffer
#ifdef ALIGN_SIZE
	if(voice_buffer){
		aligned_free(voice_buffer);
		voice_buffer = NULL;
	}
#else
	if(voice_buffer){
		safe_free(voice_buffer);
		voice_buffer = NULL;
	}
#endif
#endif
#ifdef MULTI_THREAD_COMPUTE
	free_thread_mix_c();
#endif		
}



/****************  ****************/


static void compute_portament(int v, int32 c)
{
    Voice *vp = &voice[v];
    int32 d;
	
	if(vp->porta_status != 1)
		return;
    d = (int32)(vp->porta_dpb * c); 
	if(d < 1)
		d = 1;
	if(vp->porta_pb == vp->porta_next_pb){
		vp->porta_status = 0; // stop portament
	}else if(vp->porta_pb < vp->porta_next_pb){
		vp->porta_pb += d;
		if(vp->porta_pb >= vp->porta_next_pb){
			vp->porta_pb = vp->porta_next_pb;
			vp->porta_status = 0; // stop portament
		}
	}else{ // vp->porta_pb > porta_last_note_fine
		vp->porta_pb -= d;
		if(vp->porta_pb <= vp->porta_next_pb){
			vp->porta_pb = vp->porta_next_pb;
			vp->porta_status = 0; // stop portament
		}
	}
	vp->porta_out = vp->porta_pb - vp->porta_note_fine;
}

static inline void update_tremolo(int v)
{
	Voice *vp = &voice[v];
//	const FLOAT_T tuning = TREMOLO_AMPLITUDE_TUNING; // * DIV_15BIT; // def DIV_17BIT

	FLOAT_T vol;	
	vol = (1.0 + vp->lfo2.out * vp->lfo_amp_depth[0])
		* (1.0 + vp->lfo1.out * vp->lfo_amp_depth[1]);
	vp->tremolo_volume = vol;
}


static inline void mix_mystery_signal(DATA_T *sp, DATA_T *lp, int v, int count)
{
	Voice *vp = voice + v;
	int i;
	DATA_T s;
	
	if (play_mode->encoding & PE_MONO) {
		/* Mono output. */
		if(opt_mix_envelope > 0){
			reset_envelope2(&vp->mix_env, vp->left_mix, vp->right_mix, ENVELOPE_KEEP);
			for (i = 0; i < count; i++) {
				s = *sp++;
				if(!(i & mix_env_mask))
					compute_envelope2(&vp->mix_env, opt_mix_envelope);
				MIXATION(vp->mix_env.vol[0]);
			}
			if(!check_envelope0(&vp->amp_env) && !check_envelope2(&vp->mix_env))
				 vp->finish_voice = 2; // set finish voice
		}else{
			for (i = 0; i < count; i++) {
				s = *sp++;
				MIXATION(vp->left_mix);
			}
			if(!check_envelope0(&vp->amp_env))
				 vp->finish_voice = 2; // set finish voice
		}
	}else{
		if(opt_mix_envelope >= 4){

// multi 4sample * 2ch	
#if (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
			__m128d vevol, vspx, vsp1, vsp2;
			reset_envelope2(&vp->mix_env, vp->left_mix, vp->right_mix, ENVELOPE_KEEP);
			for (i = 0; i < count; i += 4) {
				if(!(i & mix_env_mask)){
					compute_envelope2(&vp->mix_env, opt_mix_envelope);
#if defined(FLOAT_T_DOUBLE)	
					vevol = _mm_loadu_pd(vp->mix_env.vol);
#else
					vevol = _mm_cvtps_pd(_mm_load_ps(vp->mix_env.vol));
#endif
				}
				vspx = _mm_loadu_pd(sp);
				vsp1 = _mm_shuffle_pd(vspx, vspx, 0x0);
				vsp2 = _mm_shuffle_pd(vspx, vspx, 0x3);
				MM_LSU_FMA_PD(lp, vsp1, vevol);
				lp += 2;
				MM_LSU_FMA_PD(lp, vsp2, vevol);
				lp += 2;
				sp += 2;
				vspx = _mm_loadu_pd(sp);
				vsp1 = _mm_shuffle_pd(vspx, vspx, 0x0);
				vsp2 = _mm_shuffle_pd(vspx, vspx, 0x3);
				MM_LSU_FMA_PD(lp, vsp1, vevol);
				lp += 2;
				MM_LSU_FMA_PD(lp, vsp2, vevol);
				lp += 2;
				sp += 2;
			}	

#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
			__m128 vevol, vsp, vsp1, vsp2;
			reset_envelope2(&vp->mix_env, vp->left_mix, vp->right_mix, ENVELOPE_KEEP);
			for (i = 0; i < count; i += 4) {
				if(!(i & mix_env_mask)){
					compute_envelope2(&vp->mix_env, opt_mix_envelope);
#if defined(FLOAT_T_DOUBLE)	
#if (USE_X86_EXT_INTRIN >= 3)
					vevol = _mm_cvtpd_ps(_mm_loadu_pd(vp->mix_env.vol));
#else
					vevol = _mm_set_ps(0, 0, (float)vp->mix_env.vol[1], (float)vp->mix_env.vol[0]);
#endif
#else
					vevol = _mm_loadu_ps(vp->mix_env.vol);
#endif
					vevol = _mm_shuffle_ps(vevol, vevol, 0x44);
				}
				vsp = _mm_loadu_ps(sp);
				vsp1 = _mm_shuffle_ps(vsp, vsp, 0x50); // [0,1,2,3] to {0,0,1,1]
				vsp2 = _mm_shuffle_ps(vsp, vsp, 0xfa); // [0,1,2,3] to {2,2,3,3]
				MM_LSU_FMA_PS(lp, vsp1, vevol);
				lp += 4;
				MM_LSU_FMA_PS(lp, vsp2, vevol);
				lp += 4;
				sp += 4;
			}

#else // ! USE_X86_EXT_INTRIN
			DATA_T voll, volr;
			reset_envelope2(&vp->mix_env, vp->left_mix, vp->right_mix, ENVELOPE_KEEP);
			for (i = 0; i < count; i += 4) {
				if(!(i & mix_env_mask)){
					compute_envelope2(&vp->mix_env, opt_mix_envelope);
					voll = vp->mix_env.vol[0];
					volr = vp->mix_env.vol[1];
				}
				s = *sp++;
				MIXATION(voll); MIXATION(volr);
				s = *sp++;
				MIXATION(voll); MIXATION(volr);
				s = *sp++;
				MIXATION(voll); MIXATION(volr);
				s = *sp++;
				MIXATION(voll); MIXATION(volr);
			}
#endif // USE_X86_EXT_INTRIN
			
			if(!check_envelope0(&vp->amp_env) && !check_envelope2(&vp->mix_env))
				vp->finish_voice = 2; // set finish voice
		}else if(opt_mix_envelope > 0){

// single 1sample * 2ch
#if (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
			__m128d vevol;
			reset_envelope2(&vp->mix_env, vp->left_mix, vp->right_mix, ENVELOPE_KEEP);
			for (i = 0; i < count; i++) {
				if(!(i & mix_env_mask)){
					compute_envelope2(&vp->mix_env, opt_mix_envelope);
#if defined(FLOAT_T_DOUBLE)	
					vevol = _mm_loadu_pd(vp->mix_env.vol);
#else
					vevol = _mm_cvtps_pd(_mm_load_ps(vp->mix_env.vol));
#endif
				}
				MM_LSU_FMA_PD(lp, MM_LOAD1_PD(sp++), vevol);
				lp += 2;
			}

#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)			
			__m128 vevol;
			__m128 vsp;
			reset_envelope2(&vp->mix_env, vp->left_mix, vp->right_mix, ENVELOPE_KEEP);
			for (i = 0; i < count; i++) {
				if(!(i & mix_env_mask)){
					compute_envelope2(&vp->mix_env, opt_mix_envelope);
#if defined(FLOAT_T_DOUBLE)	
#if (USE_X86_EXT_INTRIN >= 3)
					vevol = _mm_cvtpd_ps(_mm_loadu_pd(vp->mix_env.vol));
#else
					vevol = _mm_set_ps(0, 0, (float)vp->mix_env.vol[1], (float)vp->mix_env.vol[0]);
#endif
#else
					vevol = _mm_loadu_ps(vp->mix_env.vol);
#endif
					vevol = _mm_shuffle_ps(vevol, vevol, 0x44);
				}
				vsp = _mm_loadu_ps(sp++);
				vsp = _mm_shuffle_ps(vsp, vsp, 0x50); // [0,1,2,3] to {0,0,1,1]
				vsp = MM_FMA_PS(vsp, vevol, _mm_loadu_ps(lp));
				_mm_storel_pi((__m64 *)lp, vsp);
				lp += 2;
			}

#else // ! USE_X86_EXT_INTRIN
			reset_envelope2(&vp->mix_env, vp->left_mix, vp->right_mix, ENVELOPE_KEEP);
			for (i = 0; i < count; i++) {
				s = *sp++;
				if(!(i & mix_env_mask))
					compute_envelope2(&vp->mix_env, opt_mix_envelope);
				MIXATION(vp->mix_env.vol[0]); MIXATION(vp->mix_env.vol[1]);
			}
#endif // USE_X86_EXT_INTRIN

			if(!check_envelope0(&vp->amp_env) && !check_envelope2(&vp->mix_env))
				vp->finish_voice = 2; // set finish voice
		}else{ // mix_env off

// single 2sample * 2ch
#if (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
			__m128d vevol = _mm_set_pd((double)vp->left_mix, (double)vp->left_mix);
			for (i = 0; i < count; i += 2) {
				MM_LSU_FMA_PD(lp, MM_LOAD1_PD(sp++), vevol);
				lp += 2;
				MM_LSU_FMA_PD(lp, MM_LOAD1_PD(sp++), vevol);
				lp += 2;
			}

#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)	
			__m128 vsp;		
			__m128 vevol = _mm_set_ps(0, 0, (float)vp->right_mix, (float)vp->left_mix);
			vevol = _mm_shuffle_ps(vevol, vevol, 0x44);
			for (i = 0; i < count; i += 2) {
				vsp = _mm_loadu_ps(sp);
				vsp = _mm_shuffle_ps(vsp, vsp, 0x50); // [0,1,2,3] to {0,0,1,1]
				MM_LSU_FMA_PS(lp, vevol, _mm_loadu_ps(lp));
				lp += 4;
				sp += 2;
			}
			
#else // ! USE_X86_EXT_INTRIN
			for (i = 0; i < count; i++) {
				s = *sp++;
				MIXATION(vp->left_mix); MIXATION(vp->right_mix);
			}
#endif // USE_X86_EXT_INTRIN

			if(!check_envelope0(&vp->amp_env))
				vp->finish_voice = 2; // set finish voice
		}
	}
}



///r
int apply_envelope_to_amp(int v)
{
	Voice *vp = &voice[v];
	double lamp, ramp;
	int32 la, ra;

	if (vp->panned == PANNED_MYSTERY) {

		lamp = vp->vol_env.vol[0];
		ramp = vp->vol_env.vol[1];
		lamp *= vp->tremolo_volume;
		ramp *= vp->tremolo_volume;
		if (vp->sample->modes & MODES_ENVELOPE) {
			lamp *= vp->amp_env.volume;
			ramp *= vp->amp_env.volume;
		}
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
		lamp *= mul_amp;
		ramp *= mul_amp;
		if (lamp > max_amp_value){
			lamp = max_amp_value;}
		else if (lamp < 0){lamp = 0;}
		if (ramp > max_amp_value){
			ramp = max_amp_value;}
		else if (ramp < 0){ramp = 0;}
		vp->left_mix = FINAL_VOLUME(lamp);
		vp->right_mix = FINAL_VOLUME(ramp);
#else
		la = TIM_FSCALE(lamp, amp_bits);
		if (la > max_amp_value){la = max_amp_value;}
		else if (la < 0){la = 0;}
		ra = TIM_FSCALE(ramp, amp_bits);
		if (ra > max_amp_value){ra = max_amp_value;}
		else if (ra < 0){ra = 0;}
#if 0
		if ((vp->status & (VOICE_OFF | VOICE_SUSTAINED)) && (la | ra) <= 0) {
			vp->finish_voice = 2; // set finish voice
			return 1;
		}
#endif
		vp->left_mix = FINAL_VOLUME(la);
		vp->right_mix = FINAL_VOLUME(ra);
#endif

	} else {
		lamp = vp->vol_env.vol[0];
		lamp *= vp->tremolo_volume;
		if (vp->sample->modes & MODES_ENVELOPE)
			lamp *= vp->amp_env.volume;
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
		lamp *= mul_amp;
		if (lamp > max_amp_value){lamp = max_amp_value;}
		else if (lamp < 0){lamp = 0;}
		vp->left_mix = FINAL_VOLUME(lamp);
		vp->right_mix = vp->left_mix;
#else
		la = TIM_FSCALE(lamp, amp_bits);
		if (la > max_amp_value){la = max_amp_value;}
		else if (la < 0){la = 0;}
#if 0
		if ((vp->status & (VOICE_OFF | VOICE_SUSTAINED)) && la <= 0) {
			vp->finish_voice = 2; // set finish voice
			return 1;
		}
#endif
		vp->left_mix = FINAL_VOLUME(la);
#endif
	}
	return 0;
}


/**************** interface function ****************/
void mix_voice(DATA_T *buf, int v, int32 c)
{
	Voice *vp = voice + v;
	DATA_T *sp = voice_buffer;
	int delay_cnt, env = vp->sample->modes & MODES_ENVELOPE;
			
	if(!vp->init_voice)
		init_voice(v);
	else if(vp->update_voice)
		update_voice(v);
	reset_envelope2(&vp->vol_env, vp->left_amp, vp->right_amp, ENVELOPE_KEEP);
	compute_envelope2(&vp->vol_env, c);
	if(env)
		delay_cnt = compute_envelope0(&vp->amp_env, c); // prev resample_voice()
	else
		delay_cnt = compute_envelope0_delay(&vp->amp_env, c); // only delay count
	if (delay_cnt) {
		if(delay_cnt == c)
			return;
#if (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE) && defined(FLOAT_T_DOUBLE)
		delay_cnt &= ~(0x1); // for filter SIMD optimaize (filter.c buffer_filter()
#endif
		if (play_mode->encoding & PE_MONO)
			buf += delay_cnt;
		else
			buf += delay_cnt * 2;
		c -= delay_cnt;
	}		
	if((++vp->mod_update_count) >= opt_modulation_update)
		vp->mod_update_count = 0;
	if(!vp->mod_update_count){	
		int scnt = c * opt_modulation_update;		
		if (opt_modulation_envelope && env){
			compute_envelope0(&vp->mod_env, scnt);
			compute_envelope4(&vp->pit_env, scnt);
		}
		compute_oscillator(&vp->lfo1, scnt);
		compute_oscillator(&vp->lfo2, scnt);
		update_tremolo(v);	
		compute_portament(v, scnt);
		recompute_voice_lfo(v);
		recompute_voice_amp(v);
		recompute_voice_pitch(v);
		recompute_voice_filter(v);
		recompute_resample_filter(v);
	}
	apply_envelope_to_amp(v);
	switch(vp->sample->inst_type){
	case INST_GUS:
	case INST_SF2:
	case INST_MOD:
	case INST_PCM:
#ifdef ENABLE_SFZ
	case INST_SFZ:
#endif
#ifdef ENABLE_DLS
	case INST_DLS:
#endif
		if(opt_resample_over_sampling){
			int32 c2 = c * opt_resample_over_sampling;
			resample_voice(v, sp, c2);
			resample_filter(v, sp, c2);
			resample_down_sampling(sp, c);
		}else{
			resample_voice(v, sp, c);
			resample_filter(v, sp, c);
		}
		break;
#ifdef INT_SYNTH
	case INST_MMS:
		compute_voice_mms(v, sp, c);
		break;
	case INST_SCC:
		compute_voice_scc(v, sp, c);
		break;
#endif
	}
#ifdef VOICE_EFFECT
	voice_effect(v, sp, c);
#endif
	voice_filter(v, sp, c);
	mix_mystery_signal(sp, buf, v, c);	
	if(vp->finish_voice >= 2 || vp->finish_voice && !vp->sample->keep_voice)
		free_voice(v);
}





#ifdef MULTI_THREAD_COMPUTE
#include "thread_mix.c"
#endif