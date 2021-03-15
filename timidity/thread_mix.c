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

    thread_mix.c
*/



#ifdef LOOKUP_HACK
#define MIXATION_THREAD(a) *lp++ = mixup[(a << 8) | (uint8) s]
#else
#define MIXATION_THREAD(a) *lp++ = (a) * s
#endif


#if 0 // dim voice buffer
static ALIGN DATA_T voice_buffer_thread[CDM_JOB_NUM][AUDIO_BUFFER_SIZE * RESAMPLE_OVER_SAMPLING_MAX];
#else // malloc voice buffer
static DATA_T *voice_buffer_thread[CDM_JOB_NUM];
#endif // malloc voice buffer


/*************** thread_mix.c initialize uninitialize *****************/

void init_thread_mix_c(void)
{
	int i;
#ifdef ALIGN_SIZE
	const int min_compute_sample = 8;
	int byte = ((compute_buffer_size + min_compute_sample) * RESAMPLE_OVER_SAMPLING_MAX) * sizeof(DATA_T);
#else
	int byte = compute_buffer_size * RESAMPLE_OVER_SAMPLING_MAX * sizeof(DATA_T);
#endif

	if(byte < (AUDIO_BUFFER_SIZE * sizeof(DATA_T)))
		byte = AUDIO_BUFFER_SIZE * sizeof(DATA_T);
	
#if 0 // dim voice_buffer
	memset(voice_buffer_thread, 0, sizeof(voice_buffer_thread));
#else // malloc voice_buffer
	for(i = 0; i < CDM_JOB_NUM; i++){
#ifdef ALIGN_SIZE
		if(voice_buffer_thread[i]){
			aligned_free(voice_buffer_thread[i]);
			voice_buffer_thread[i] = NULL;
		}
		voice_buffer_thread[i] = (DATA_T *)aligned_malloc(byte, ALIGN_SIZE);
#else
		if(voice_buffer_thread[i]){
			safe_free(voice_buffer_thread[i]);
			voice_buffer_thread[i] = NULL;
		}
		voice_buffer_thread[i] = (DATA_T *)safe_malloc(byte);
#endif
		memset(voice_buffer_thread[i], 0, byte);
	}
#endif // malloc voice_buffer
		
}

void free_thread_mix_c(void)
{
	int i;

#if 0 // dim voice_buffer

#else // malloc voice_buffer
	for(i = 0; i < CDM_JOB_NUM; i++){
#ifdef ALIGN_SIZE
		if(voice_buffer_thread[i]){
			aligned_free(voice_buffer_thread[i]);
			voice_buffer_thread[i] = NULL;
		}
#else
		if(voice_buffer_thread[i]){
			safe_free(voice_buffer_thread[i]);
			voice_buffer_thread[i] = NULL;
		}
#endif
	}
#endif // malloc voice_buffer	
}


/**************** interface function ****************/
/**************** mix_voice ****************/




static inline void mix_mystery_signal_thread(DATA_T *sp, DATA_T *lp, int v, int count)
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
				MIXATION_THREAD(vp->mix_env.vol[0]);
			}
			if(!check_envelope0(&vp->amp_env) && !check_envelope2(&vp->mix_env))
				vp->finish_voice = 2; // set finish voice
		}else{
			for (i = 0; i < count; i++) {
				s = *sp++;
				MIXATION_THREAD(vp->left_mix);
			}
			if(!check_envelope0(&vp->amp_env))
				vp->finish_voice = 2; // set finish voice
		}
	}else{

		if(opt_mix_envelope >= 4){
	
// multi 4sample * 2ch	
#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
			__m128d vevolx;
			__m256d vevol, vsp, vsp1, vsp2;
			reset_envelope2(&vp->mix_env, vp->left_mix, vp->right_mix, ENVELOPE_KEEP);
			for (i = 0; i < count; i += 4) {
				if(!(i & mix_env_mask)){
					compute_envelope2(&vp->mix_env, opt_mix_envelope);
#if defined(FLOAT_T_DOUBLE)	
					vevolx = _mm_loadu_pd(vp->mix_env.vol);
#else
					vevolx = _mm_cvtps_pd(_mm_load_ps(vp->mix_env.vol));
#endif
					vevol = MM256_SET2X_PD(vevolx, vevolx);
				}
				vsp = _mm256_loadu_pd(sp);
#if (USE_X86_EXT_INTRIN >= 9)
				vsp1 = _mm256_permute4x64_pd(vsp, 0x50);
				vsp2 = _mm256_permute4x64_pd(vsp, 0xfa);
#else
				vsp1 = _mm256_permute2f128_pd(vsp, vsp, 0x0);
				vsp2 = _mm256_permute2f128_pd(vsp, vsp, 0x3);
				vsp1 = _mm256_permute_pd(vsp1, 0xc);
				vsp2 = _mm256_permute_pd(vsp1, 0xc);
#endif
				_mm256_storeu_pd(lp, _mm256_mul_pd(vsp1, vevol));
				lp += 4;
				_mm256_storeu_pd(lp, _mm256_mul_pd(vsp2, vevol));
				lp += 4;
				sp += 4;
			}	
	
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
			__m128 vevolx, vspx;
			__m256 vevol, vsp;
			reset_envelope2(&vp->mix_env, vp->left_mix, vp->right_mix, ENVELOPE_KEEP);
			for (i = 0; i < count; i += 4) {
				if(!(i & mix_env_mask)){
					compute_envelope2(&vp->mix_env, opt_mix_envelope);
#if defined(FLOAT_T_DOUBLE)	
					vevolx = _mm_cvtpd_ps(_mm_loadu_pd(vp->mix_env.vol));
#else
					vevolx = _mm_loadu_ps(vp->mix_env.vol);
#endif
					vevolx = _mm_shuffle_ps(vevolx, vevolx, 0x44);
					vevol = MM256_SET2X_PS(vevolx, vevolx);
				}
#if (USE_X86_EXT_INTRIN >= 9)
				vsp = _mm_loadu_ps(sp);
				vsp = _mm256_permute8x32_ps(vsp, _mm256_set_epi32(3, 3, 2, 2, 1, 1, 0, 0));
#else
				vspx = _mm_loadu_ps(sp);
				vsp = MM256_SET2X_PS(_mm_permute_ps(vspx, 0x50), _mm_permute_ps(vspx, 0xfa));
#endif
				_mm256_storeu_ps(lp, _mm256_mul_ps(vsp, vevol));
				lp += 8;
				sp += 4;
			}

#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
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
				_mm_storeu_pd(lp, _mm_mul_pd(vsp1, vevol));
				lp += 2;
				_mm_storeu_pd(lp, _mm_mul_pd(vsp2, vevol));
				lp += 2;
				sp += 2;
				vspx = _mm_loadu_pd(sp);
				vsp1 = _mm_shuffle_pd(vspx, vspx, 0x0);
				vsp2 = _mm_shuffle_pd(vspx, vspx, 0x3);
				_mm_storeu_pd(lp, _mm_mul_pd(vsp1, vevol));
				lp += 2;
				_mm_storeu_pd(lp, _mm_mul_pd(vsp2, vevol));
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
				_mm_storeu_ps(lp, _mm_mul_ps(vsp1, vevol));
				lp += 4;
				_mm_storeu_ps(lp, _mm_mul_ps(vsp2, vevol));
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
				MIXATION_THREAD(voll); MIXATION_THREAD(volr);
				s = *sp++;
				MIXATION_THREAD(voll); MIXATION_THREAD(volr);
				s = *sp++;
				MIXATION_THREAD(voll); MIXATION_THREAD(volr);
				s = *sp++;
				MIXATION_THREAD(voll); MIXATION_THREAD(volr);
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
				_mm_storeu_pd(lp, _mm_mul_pd(MM_LOAD1_PD(sp++), vevol));
				lp += 2;
			}

#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)			
			__m128 vevol;
			__m128 vsp;
			reset_envelope2(&vp->mix_env, vp->left_mix, vp->right_mix, ENVELOPE_KEEP);
			for (i = 0; i < count; i++) {
				if(!(i & mix_env_mask)){
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
				vsp = _mm_mul_ps(vsp, vevol);
				*(lp++) = MM_EXTRACT_F32(vsp,0);
				*(lp++) = MM_EXTRACT_F32(vsp,1);
			}

#else // ! USE_X86_EXT_INTRIN
			reset_envelope2(&vp->mix_env, vp->left_mix, vp->right_mix, ENVELOPE_KEEP);
			for (i = 0; i < count; i++) {
				s = *sp++;
				if(!(i & mix_env_mask))
					compute_envelope2(&vp->mix_env, opt_mix_envelope);
				MIXATION_THREAD(vp->mix_env.vol[0]); MIXATION_THREAD(vp->mix_env.vol[1]);

			}
#endif // USE_X86_EXT_INTRIN

			if(!check_envelope0(&vp->amp_env) && !check_envelope2(&vp->mix_env))
				vp->finish_voice = 2; // set finish voice

		}else{ // mix_env off

// single 1sample * 2ch
#if (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
			__m128d vevol = _mm_set_pd((double)vp->left_mix, (double)vp->left_mix);
			for (i = 0; i < count; i += 2) {
				_mm_storeu_pd(lp, _mm_mul_pd(MM_LOAD1_PD(sp++), vevol));
				lp += 2;
				_mm_storeu_pd(lp, _mm_mul_pd(MM_LOAD1_PD(sp++), vevol));
				lp += 2;
			}

#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)	
			__m128 vsp;		
			__m128 vevol = _mm_set_ps(0, 0, (float)vp->right_mix, (float)vp->left_mix);
			vevol = _mm_shuffle_ps(vevol, vevol, 0x44);
			for (i = 0; i < count; i += 2) {
				vsp = _mm_loadu_ps(sp);
				vsp = _mm_shuffle_ps(vsp, vsp, 0x50); // [0,1,2,3] to {0,0,1,1]
				_mm_storeu_ps(lp, _mm_mul_ps(vsp, vevol));
				lp += 4;
				sp += 2;
			}

#else // ! USE_X86_EXT_INTRIN
			for (i = 0; i < count; i++) {
				s = *sp++;
				MIXATION_THREAD(vp->left_mix); MIXATION_THREAD(vp->right_mix);
			}
#endif // USE_X86_EXT_INTRIN

			if(!check_envelope0(&vp->amp_env))
				vp->finish_voice = 2; // set finish voice
		}
	}
}


static inline DATA_T *silent_signal_thread(DATA_T *buf, int32 c)
{
	int i;
	
	if (! (play_mode->encoding & PE_MONO))
		for (i = 0; i < c; i++) 
			*(buf++) = *(buf++) = 0;
	else /* Mono output. */
		for (i = 0; i < c; i++)
			*(buf++) = 0;
	return buf;
}


/**************** interface function ****************/
void mix_voice_thread(DATA_T *buf, int v, int32 c, int thread)
{
	Voice *vp = voice + v;
	DATA_T *sp = voice_buffer_thread[thread];
	int delay_cnt = 0, env = vp->sample->modes & MODES_ENVELOPE;

	vp->elapsed_count += c;

	if(vp->status & (VOICE_FREE | VOICE_PENDING))
		return;

	if(!vp->init_voice)
		init_voice(v);
	else if(vp->update_voice)
		update_voice(v);
	reset_envelope2(&vp->vol_env, vp->left_amp, vp->right_amp, ENVELOPE_KEEP);
	compute_envelope2(&vp->vol_env, c);	
	if(env)
		delay_cnt = compute_envelope0(&vp->amp_env, c); // prev resample_voice()
	else
		delay_cnt = compute_envelope0_delay(&vp->amp_env, c); // prev resample_voice()
	if (delay_cnt) {
		if(delay_cnt == c)
			return;
#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE) && defined(FLOAT_T_DOUBLE)
		delay_cnt &= ~(0x3); // for filter SIMD optimaize (filter.c buffer_filter()
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE) && defined(FLOAT_T_DOUBLE)
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
#ifdef ENABLE_ECW
	case INST_ECW:
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
		compute_voice_mms_thread(v, sp, c, thread);
		break;
	case INST_SCC:
		compute_voice_scc_thread(v, sp, c, thread);
		break;
#endif
	}
#ifdef VOICE_EFFECT
	voice_effect(v, sp, c);
#endif
	voice_filter(v, sp, c);
	mix_mystery_signal_thread(sp, buf, v, c);
	if(vp->finish_voice >= 2 || vp->finish_voice && !vp->sample->keep_voice)
		free_voice(v);
}

