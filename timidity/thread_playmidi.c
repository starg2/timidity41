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

    thread_playmidi.c -- random stuff in need of rearrangement
*/


#include "thread.h"




//extern
extern uint8 opt_normal_chorus_plus;

// thread_effect.c
extern void init_effect_buffer_thread(void);
// send effect
extern void set_ch_reverb_thread(DATA_T *buf, int32 count, int32 level);
extern void set_ch_delay_thread(DATA_T *buf, int32 count, int32 level);
extern void set_ch_chorus_thread(DATA_T *buf, int32 count, int32 level);
extern void set_ch_eq_gs_thread(DATA_T *buf, int32 count);
extern void set_ch_insertion_gs_thread(DATA_T *buf, int32 count);
extern void set_ch_mfx_sd_thread(DATA_T *buf, int32 count, int mfx_select, int32 level);
// GS Effect
extern void do_ch_reverb_thread(int32 count, int32 byte);
extern void mix_ch_reverb_thread(DATA_T *buf, int32 count, int32 byte);
extern void do_ch_delay_thread(int32 count, int32 byte);
extern void mix_ch_delay_thread(DATA_T *buf, int32 count, int32 byte);
extern void do_ch_chorus_thread(int32 count, int32 byte);
extern void mix_ch_chorus_thread(DATA_T *buf, int32 count, int32 byte);
extern void do_ch_eq_gs_thread(int32 count, int32 byte);
extern void mix_ch_eq_gs_thread(DATA_T* buf, int32 count, int32 byte);
extern void do_insertion_effect_gs_thread(int32 count, int32 byte);
extern void mix_insertion_effect_gs_thread(DATA_T *buf, int32 count, int32 byte, int eq_enable);
// XG Effect
extern void do_variation_effect1_xg_thread(int32 count, int32 byte);
extern void mix_variation_effect1_xg_thread(DATA_T *buf, int32 count, int32 byte);
extern void do_ch_chorus_xg_thread(int32 count, int32 byte);
extern void mix_ch_chorus_xg_thread(DATA_T *buf, int32 count, int32 byte);
extern void do_ch_reverb_xg_thread(int32 count, int32 byte);
extern void mix_ch_reverb_xg_thread(DATA_T *buf, int32 count, int32 byte);
extern void do_multi_eq_xg_thread(int32 count, int32 byte);
extern void mix_multi_eq_xg_thread(DATA_T *buf, int32 count, int32 byte);
// GM2,SD MFX/SysEffect
extern void do_ch_chorus_gm2_thread(int32 count, int32 byte);
extern void mix_ch_chorus_gm2_thread(DATA_T *buf, int32 count, int32 byte);
extern void do_ch_reverb_gm2_thread(int32 count, int32 byte);
extern void mix_ch_reverb_gm2_thread(DATA_T *buf, int32 count, int32 byte);
extern void do_mfx_effect_sd_thread(int32 count, int32 byte, int mfx_select);
extern void mix_mfx_effect_sd_thread(DATA_T *buf, int32 count, int32 byte, int mfx_select);
extern void do_ch_chorus_sd_thread(int32 count, int32 byte);
extern void mix_ch_chorus_sd_thread(DATA_T *buf, int32 count, int32 byte);
extern void do_ch_reverb_sd_thread(int32 count, int32 byte);
extern void mix_ch_reverb_sd_thread(DATA_T *buf, int32 count, int32 byte);
extern void do_multi_eq_sd_thread(int32 count, int32 byte);
extern void mix_multi_eq_sd_thread(DATA_T *buf, int32 count, int32 byte);
// 
extern void do_effect_thread(DATA_T *buf, int32 count, int32 byte);
extern void do_master_effect_thread(void);


// thread delay count // use thread_effect.c
// cdmt_ofs // cdmt_cv 0:voice_mix 1:ch_mix 2:send_effect 3:do_effect
int cdmt_ofs_0 = 3;
int cdmt_ofs_1 = 2;
int cdmt_ofs_2 = 1;
int cdmt_ofs_3 = 0;
// cdmt_buf in/outのみ
int cdmt_buf_i = 1;
int cdmt_buf_o = 0;



typedef struct _compute_data_midi_thread_var {
	int channel_effect, channel_reverb, channel_chorus, channel_delay, channel_eq, channel_ins, 
		channel_mfx[SD_MFX_EFFECT_NUM], multi_eq_xg_valid, multi_eq_sd_valid,
		channel_silent[MAX_CHANNELS], channel_delay_out[MAX_CHANNELS], channel_drumfx[MAX_CHANNELS], channel_send[MAX_CHANNELS], channel_flg[MAX_CHANNELS],
		uv, voice_free[MAX_VOICES], max_ch, flg_ch_mix, flg_ch_vst; //, flg_sys_vst;
	int32 count, cnt, n, current_sample, channel_lasttime[MAX_CHANNELS];
} compute_data_midi_thread_var;

compute_data_midi_thread_var cdmt_cv[4];

static void reset_compute_var(void)
{
	memset(cdmt_cv, 0, sizeof(cdmt_cv));
}

#if 0 // dim mix_voice_buffer // master_buffer
static ALIGN DATA_T mix_voice_buffer[MAX_VOICES][AUDIO_BUFFER_SIZE * 2]; // max:64MB
static ALIGN DATA_T master_buffer[4][AUDIO_BUFFER_SIZE * 2];
static ALIGN DATA_T ch_buffer_thread_s[4][MAX_CHANNELS][AUDIO_BUFFER_SIZE * 2]; // max:8MB
static DATA_T *ch_buffer_thread[4][MAX_CHANNELS];

static void clear_cdm_thread_buffer(void)
{
	memset(mix_voice_buffer, 0, sizeof(mix_voice_buffer));
	memset(master_buffer, 0, sizeof(master_buffer));
	memset(ch_buffer_thread_s, 0, sizeof(ch_buffer_thread_s));
}

static void uninit_cdm_thread_buffer(void){}

static void init_cdm_thread_buffer(void)
{
	int i, j;

	clear_cdm_thread_buffer();
	for (j = 0; j < 4; j++) {
		for(i = 0; i < MAX_CHANNELS; i++){
			ch_buffer_thread[j][i] = ch_buffer_thread_s[j][i];
		}
	}
}

#else // malloc mix_voice_buffer // master_buffer
DATA_T *master_buffer[4];
DATA_T *mix_voice_buffer[MAX_VOICES];
DATA_T *ch_buffer_thread[4][MAX_CHANNELS];

static void clear_cdm_thread_buffer(void)
{
	int i, j;
	
	if(mix_voice_buffer[0]){
		for (i = 0; i <= voices; i++) { // max voice
			memset(mix_voice_buffer[i], 0, synth_buffer_size);
		}
	}
	if(master_buffer[0]){
		for (i = 0; i < 4; i++) { // 
			memset(master_buffer[i], 0, synth_buffer_size);
		}
	}
	if(ch_buffer_thread[0][0]){
		for (j = 0; j < 4; j++) {
			for(i = 0; i < MAX_CHANNELS; i++){
				memset(ch_buffer_thread[j][i], 0, synth_buffer_size);
			}
		}
	}
}

static void uninit_cdm_thread_buffer(void)
{
	int i, j;

	for (i = 0; i < MAX_VOICES; i++) {
#ifdef ALIGN_SIZE
		aligned_free(mix_voice_buffer[i]);
#else
		safe_free(mix_voice_buffer[i]);
#endif
		mix_voice_buffer[i] = NULL;
	}
	for (i = 0; i < 4; i++) {
#ifdef ALIGN_SIZE
		aligned_free(master_buffer[i]);
#else
		safe_free(master_buffer[i]);
#endif
		master_buffer[i] = NULL;
	}
	for (j = 0; j < 4; j++) {
		for(i = 0; i < MAX_CHANNELS; i++){
#ifdef ALIGN_SIZE
			aligned_free(ch_buffer_thread[j][i]);
#else
			safe_free(ch_buffer_thread[j][i]);
#endif
			ch_buffer_thread[j][i] = NULL;
		}
	}
}

static void init_cdm_thread_buffer(void)
{
	int i, j;

	if(mix_voice_buffer[0] == NULL){
		for (i = 0; i <= voices; i++) { // max voice
#ifdef ALIGN_SIZE
			mix_voice_buffer[i] = (DATA_T *)aligned_malloc(synth_buffer_size, ALIGN_SIZE);
#else
			mix_voice_buffer[i] = (DATA_T *)safe_malloc(synth_buffer_size);
#endif
			memset(mix_voice_buffer[i], 0, synth_buffer_size);
		}
	}
	if(master_buffer[0] == NULL){
		for (i = 0; i < 4; i++) { // 
#ifdef ALIGN_SIZE
			master_buffer[i] = (DATA_T *)aligned_malloc(synth_buffer_size, ALIGN_SIZE);
#else
			master_buffer[i] = (DATA_T *)safe_malloc(synth_buffer_size);
#endif
			memset(master_buffer[i], 0, synth_buffer_size);
		}
	}
	if(ch_buffer_thread[0][0] == NULL){
		for (j = 0; j < 4; j++) {
			for(i = 0; i < MAX_CHANNELS; i++){
#ifdef ALIGN_SIZE
				ch_buffer_thread[j][i] = (DATA_T *)aligned_malloc(synth_buffer_size, ALIGN_SIZE);
#else
				ch_buffer_thread[j][i] = (DATA_T *)safe_malloc(synth_buffer_size);
#endif
				memset(ch_buffer_thread[j][i], 0, synth_buffer_size);
			}
		}
	}
}

#endif // malloc mix_voice_buffer

// thread.c terminate_compute_thread()
void uninit_compute_data_midi_thread(void)
{
	uninit_cdm_thread_buffer();
}

// thread.c begin_compute_thread()
void init_compute_data_midi_thread(void)
{
	init_cdm_thread_buffer();
	reset_compute_var();
	init_effect_buffer_thread();
}

// playmidi.c reset_midi()
static void reset_midi_thread(void)
{
	clear_cdm_thread_buffer();
	reset_compute_var();
	init_effect_buffer_thread();
}





/*
do_compute_data_midi_thread以下を仮スレッド数16で分割

job_num
 0; // do_master_effect_thread
 1; // do effect // sd_rev // gm2_rev // gs_rev // xg_rev
 2; // do effect // sd_mx0 // gm2_cho // gs_ins // xg_dly
 3; // do effect // sd_mx1 // gm2_mx0 // gs_cho // xg_cho
 4; // do effect // sd_mx2 // gm2_mx1 // gs_dly // xg_meq
 5; // do effect // sd_cho // gm2_mx2 // gs_peq // 空    
 6; // do effect // sd_meq // gm2_meq // 空     // 空    
 7; // send effect // master,eq                          
 8; // send effect // reverb                             
 9; // send effect // chorus							 
10; // send effect // gs_dly // xg_var // sd_mfx         
11; // send effect // gs_ins // XG 空 // SD 空
12; // compute_var
4-20:ボイス/チャンネルは16分割
これとは別にエフェクト用スレッドは1;2;以降の空いたところに追加される (RevExMod使用の場合

effectは(一番分割数が多いものに合わせてるのでMIDIシステムによって空がある
同じjob_numは1セットのジョブ (effectは数が少ないのと空きあるのでボイス/チャンネルだけのもある
(全部をバラにすると割り当てオーバーヘッドが大きくなる？
(effectは一部除いて軽負荷 OFFになってることもある 空のムダ？
(全CPUコアに同じ処理(ボイス/チャンネル)を割り当てると速い (キャッシュ効率？
(XG_INS*4(別CH)の場合は16が最適
負荷の高い可能性のある 3以下は ボイス/チャンネルと分離
などいろいろあって 20分割	


*/
#define CDM_JOB_VC_OFFSET 4
const int cdm_job_num = CDM_JOB_NUM; // 13 <= num // for voice/channel
const int cdm_job_num_total = CDM_JOB_NUM + CDM_JOB_VC_OFFSET;

// XG EQ/INS dfx_buf_mix ch_mix
static void do_compute_data_midi_thread2(int thread_num)
{
	int i, j;
	int32 efx_delay_out, subtime, delay_out;
	DATA_T *p;

	if(!cdmt_cv[cdmt_ofs_1].flg_ch_mix)
		return;
	for(i = thread_num; i <= cdmt_cv[cdmt_ofs_1].max_ch; i += cdm_job_num) {
		if(!cdmt_cv[cdmt_ofs_1].channel_silent[i])
			continue;
		p = ch_buffer_thread[cdmt_ofs_1][i];
		subtime = current_sample - channel[i].lasttime;
		delay_out = 0;
		if(subtime < delay_out_count[1]){
			int eq_flg = 0;
			if(!ISDRUMCHANNEL(i)){
				if(cdmt_cv[cdmt_ofs_1].channel_eq && channel[i].eq_xg.valid){ /* part EQ*/
					do_ch_eq_xg(p, cdmt_cv[cdmt_ofs_1].cnt, &(channel[i].eq_xg));
					eq_flg += 1;
				}
			}else if(cdmt_cv[cdmt_ofs_1].channel_drumfx[i]){
				for (j = 0; j < channel[i].drum_effect_num; j++) {
					struct DrumPartEffect *de = &(channel[i].drum_effect[j]);
					if(cdmt_cv[cdmt_ofs_1].channel_eq && de->eq_xg->valid){	/* drum effect part EQ */
						do_ch_eq_xg(de->buf_ptr[cdmt_ofs_1], cdmt_cv[cdmt_ofs_1].cnt, de->eq_xg);
						eq_flg += 1;
					}
					mix_signal(p, de->buf_ptr[cdmt_ofs_1], cdmt_cv[cdmt_ofs_1].cnt); // drum_efx_buf mix to ch_buff
				}
			}		
			if(eq_flg)
				delay_out += delay_out_count[1];
		}
		if(opt_insertion_effect){
		/* insertion effect */
			for (j = 0; j < XG_INSERTION_EFFECT_NUM; j++) {
				int ch = insertion_effect_xg[j].part;
				if(i != ch)
					continue;
				efx_delay_out = delay_out_count[delay_out_type_xg[insertion_effect_xg[j].type_msb][insertion_effect_xg[j].type_lsb]];
				if(subtime < (delay_out + efx_delay_out)){
					do_insertion_effect_xg(p, cdmt_cv[cdmt_ofs_1].cnt, &insertion_effect_xg[j]);
					cdmt_cv[cdmt_ofs_1].channel_delay_out[i] += efx_delay_out;
				}
			}
		}
		/* variation effect (ins mode) */
		if(opt_delay_control){	
			for (j = 0; j < XG_VARIATION_EFFECT_NUM; j++) {
				int ch = variation_effect_xg[j].part;
				if(i != ch || variation_effect_xg[j].connection != XG_CONN_INSERTION)
					continue;
				efx_delay_out = delay_out_count[delay_out_type_xg[variation_effect_xg[j].type_msb][variation_effect_xg[j].type_lsb]];
				if(subtime < (delay_out + efx_delay_out)){
					do_insertion_effect_xg(p, cdmt_cv[cdmt_ofs_1].cnt, &variation_effect_xg[j]);
					delay_out += efx_delay_out;
				}
			}
		}
		cdmt_cv[cdmt_ofs_1].channel_delay_out[i] = delay_out;
		// ch mixer
		if(adjust_panning_immediately){
			Channel *cp = channel + i;
			reset_envelope2(&cp->vol_env, cp->ch_amp[0], cp->ch_amp[1], ENVELOPE_KEEP);
			compute_envelope2(&cp->vol_env, cdmt_cv[cdmt_ofs_1].count);
			mix_ch_signal_source(p, i, cdmt_cv[cdmt_ofs_1].count);
			if(cdmt_cv[cdmt_ofs_1].channel_drumfx[i]){
				for (j = 0; j < channel[i].drum_effect_num; j++) {
					mix_dfx_signal_source(channel[i].drum_effect[j].buf_ptr[cdmt_ofs_1], i, cdmt_cv[cdmt_ofs_1].cnt); // drum_efx_buf mix level
				}
			}
		}
	}
}

// GS/GM2 dfx_buf_mix ch_mix
static void do_compute_data_midi_thread3(int thread_num)
{
	int i, j;
	DATA_T *p;
	
	if(!cdmt_cv[cdmt_ofs_1].flg_ch_mix)
		return;
	for(i = thread_num; i <= cdmt_cv[cdmt_ofs_1].max_ch; i += cdm_job_num) {
		if(!cdmt_cv[cdmt_ofs_1].channel_silent[i])
			continue;
		p = ch_buffer_thread[cdmt_ofs_1][i];
		// drum effect buffer mix
		if(cdmt_cv[cdmt_ofs_1].channel_drumfx[i])
			for (j = 0; j < channel[i].drum_effect_num; j++)
				mix_signal(p, channel[i].drum_effect[j].buf_ptr[cdmt_ofs_1], cdmt_cv[cdmt_ofs_1].cnt);
		// ch mixer
		//if(adjust_panning_immediately){
		//	Channel *cp = channel + i;
		//	reset_envelope2(&cp->vol_env, cp->ch_amp[0], cp->ch_amp[1], ENVELOPE_KEEP);
		//	compute_envelope2(&cp->vol_env, cdmt_cv[cdmt_ofs_1].count);
		//	mix_ch_signal_source(p, i, cdmt_cv[cdmt_ofs_1].count);
		//	 // drum_efx_buf mix level
		//	if(cdmt_cv[cdmt_ofs_1].channel_drumfx[i]){
		//		for (j = 0; j < channel[i].drum_effect_num; j++) {
		//			mix_dfx_signal_source(channel[i].drum_effect[j].buf_ptr[cdmt_ofs_1], i, cdmt_cv[cdmt_ofs_1].cnt);
		//		}
		//	}
		//}
	}
}

// effect_off/ch_vst ch_mix
//static void do_compute_data_midi_thread4(int thread_num)
//{
//	int i, j;
//	DATA_T *p;
//	
//	for(i = thread_num; i <= cdmt_cv[cdmt_ofs_1].max_ch; i += cdm_job_num) {
//		if(!cdmt_cv[cdmt_ofs_1].channel_silent[i])
//			continue;
//		p = ch_buffer_thread[cdmt_ofs_1][i];
//		// ch mixer
//		//if(adjust_panning_immediately){
//		//	Channel *cp = channel + i
//		//	reset_envelope2(&cp->vol_env, cp->ch_amp[0], cp->ch_amp[1], ENVELOPE_KEEP);
//		//	compute_envelope2(&cp->vol_env, cdmt_cv[cdmt_ofs_1].count);
//		//	mix_ch_signal_source(p, i, cdmt_cv[cdmt_ofs_1].count);
//		//}
//	}
//}



// XG Effect
static void do_compute_data_midi_thread5(int thread_num)
{		
	switch(thread_num){
	case 0:
		if(cdmt_cv[cdmt_ofs_3].channel_reverb)
		{do_ch_reverb_xg_thread(cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
		break;
	case 1:
		if(cdmt_cv[cdmt_ofs_3].channel_delay)
		{do_variation_effect1_xg_thread(cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
		break;
	case 2:
		if(cdmt_cv[cdmt_ofs_3].channel_chorus)
		{do_ch_chorus_xg_thread(cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
		break;
	case 3:
		if(cdmt_cv[cdmt_ofs_3].multi_eq_xg_valid)
		{do_multi_eq_xg_thread(cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
		break;
	}
}

// GS Effect
static void do_compute_data_midi_thread6(int thread_num)
{	
	switch(thread_num){
	case 0:
		if(cdmt_cv[cdmt_ofs_3].channel_reverb)
		{do_ch_reverb_thread(cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
		break;
	case 1:
		if(cdmt_cv[cdmt_ofs_3].channel_ins)
		{do_insertion_effect_gs_thread(cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
		break;
	case 2:
		if(cdmt_cv[cdmt_ofs_3].channel_chorus) 
		{do_ch_chorus_thread(cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
		break;
	case 3:
		if(cdmt_cv[cdmt_ofs_3].channel_delay)
		{do_ch_delay_thread(cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
		break;
	case 4:
		if(cdmt_cv[cdmt_ofs_3].channel_eq)
		{do_ch_eq_gs_thread(cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
		break;
	}
}

// SD MFX/SysEffect
static void do_compute_data_midi_thread7(int thread_num)
{
	switch(thread_num){
	case 0:
		if(cdmt_cv[cdmt_ofs_3].channel_reverb)
		{do_ch_reverb_sd_thread(cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
		break;
	case 1:
		if(cdmt_cv[cdmt_ofs_3].channel_mfx[0])
		{do_mfx_effect_sd_thread(cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n, 0);}
		break;
	case 2:
		if(cdmt_cv[cdmt_ofs_3].channel_mfx[1])
		{do_mfx_effect_sd_thread(cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n, 1);}
		break;
	case 3:
		if(cdmt_cv[cdmt_ofs_3].channel_mfx[2])
		{do_mfx_effect_sd_thread(cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n, 2);}
		break;
	case 4:
		if(cdmt_cv[cdmt_ofs_3].channel_chorus)
		{do_ch_chorus_sd_thread(cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
		break;
	case 5:
		if(cdmt_cv[cdmt_ofs_3].multi_eq_sd_valid)
		{do_multi_eq_sd_thread(cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
		break;
	}
}

// GM2 SysEffect (use PartMFX
static void do_compute_data_midi_thread8(int thread_num)
{
	switch(thread_num){
	case 0:
		if(cdmt_cv[cdmt_ofs_3].channel_reverb)
		{do_ch_reverb_gm2_thread(cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
		break;
	case 1:
		if(cdmt_cv[cdmt_ofs_3].channel_chorus)
		{do_ch_chorus_gm2_thread(cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
		break;
	case 2:
		if(cdmt_cv[cdmt_ofs_3].channel_mfx[0])
		{do_mfx_effect_sd_thread(cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n, 0);}
		break;
	case 3:
		if(cdmt_cv[cdmt_ofs_3].channel_mfx[1])
		{do_mfx_effect_sd_thread(cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n, 1);}
		break;
	case 4:
		if(cdmt_cv[cdmt_ofs_3].channel_mfx[2])
		{do_mfx_effect_sd_thread(cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n, 2);}
		break;
	case 5:
		if(cdmt_cv[cdmt_ofs_3].multi_eq_sd_valid)
		{do_multi_eq_sd_thread(cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
		break;
	}
}



// send effect buffer XG // master
static void do_compute_data_midi_thread10(int thread_num)
{
	int i;
	// master
	for(i = 0; i <= cdmt_cv[cdmt_ofs_2].max_ch; i++) {	/* system effects */
		DATA_T *p = ch_buffer_thread[cdmt_ofs_2][i];
		Channel *cp = channel + i;

		switch(cdmt_cv[cdmt_ofs_2].channel_send[i]){
		case SEND_EFFECT:
			if(cdmt_cv[cdmt_ofs_2].channel_silent[i]
				&& ((cdmt_cv[cdmt_ofs_2].current_sample - cdmt_cv[cdmt_ofs_2].channel_lasttime[i])
				< (cdmt_cv[cdmt_ofs_2].channel_delay_out[i] + delay_out_count[1]))) {
				if(cp->dry_level == 127)
					mix_signal(master_buffer[cdmt_ofs_2], p, cdmt_cv[cdmt_ofs_2].cnt);
				else
					mix_signal_level(master_buffer[cdmt_ofs_2], p, cdmt_cv[cdmt_ofs_2].cnt, cp->dry_level);
			}
			break;
		case SEND_OUTPUT:
			mix_signal(master_buffer[cdmt_ofs_2], p, cdmt_cv[cdmt_ofs_2].cnt);
			break;
		}
	}
}

// send effect buffer XG // reverb
static void do_compute_data_midi_thread11(int thread_num)
{
	int i, j;

	for(i = 0; i <= cdmt_cv[cdmt_ofs_2].max_ch; i++) {	/* system effects */
		DATA_T *p = ch_buffer_thread[cdmt_ofs_2][i];
		Channel *cp = channel + i;

		if(cdmt_cv[cdmt_ofs_2].channel_send[i] != SEND_EFFECT)
			continue;
		if(cdmt_cv[cdmt_ofs_2].channel_silent[i]
			&& ((cdmt_cv[cdmt_ofs_2].current_sample - cdmt_cv[cdmt_ofs_2].channel_lasttime[i])
			< (cdmt_cv[cdmt_ofs_2].channel_delay_out[i] + delay_out_count[1]))) {
			if (!is_insertion_effect_xg(i) && opt_drum_effect && ISDRUMCHANNEL(i) && cp->drum_effect_num) {
				for (j = 0; j < cp->drum_effect_num; j++) {
					struct DrumPartEffect *de = &(cp->drum_effect[j]);
					if (de->reverb_send > 0)
						set_ch_reverb_thread(de->buf_ptr[cdmt_ofs_2], cdmt_cv[cdmt_ofs_2].cnt, de->reverb_send);
				}
			} else {
				if(cdmt_cv[cdmt_ofs_2].channel_reverb && cp->reverb_level > 0)
					set_ch_reverb_thread(p, cdmt_cv[cdmt_ofs_2].cnt, cp->reverb_level);
			}
		}
	}
}

// send effect buffer XG // chorus
static void do_compute_data_midi_thread12(int thread_num)
{
	int i, j;

	for(i = 0; i <= cdmt_cv[cdmt_ofs_2].max_ch; i++) {	/* system effects */
		DATA_T *p = ch_buffer_thread[cdmt_ofs_2][i];
		Channel *cp = channel + i;
		
		if(cdmt_cv[cdmt_ofs_2].channel_send[i] != SEND_EFFECT)
			continue;
		if(cdmt_cv[cdmt_ofs_2].channel_silent[i]
			&& ((cdmt_cv[cdmt_ofs_2].current_sample - cdmt_cv[cdmt_ofs_2].channel_lasttime[i])
			< (cdmt_cv[cdmt_ofs_2].channel_delay_out[i] + delay_out_count[1]))) {
			if (!is_insertion_effect_xg(i) && opt_drum_effect && ISDRUMCHANNEL(i) && cp->drum_effect_num) {
				for (j = 0; j < cp->drum_effect_num; j++) {
					struct DrumPartEffect *de = &(cp->drum_effect[j]);
					if (de->chorus_send > 0)
						set_ch_chorus_thread(de->buf_ptr[cdmt_ofs_2], cdmt_cv[cdmt_ofs_2].cnt, de->chorus_send);
				}
			} else {
				if(cdmt_cv[cdmt_ofs_2].channel_chorus && cp->chorus_level > 0)
					set_ch_chorus_thread(p, cdmt_cv[cdmt_ofs_2].cnt, cp->chorus_level);
			}
		}
	}
}

// send effect buffer XG // variation
static void do_compute_data_midi_thread13(int thread_num)
{
	int i, j;

	for(i = 0; i <= cdmt_cv[cdmt_ofs_2].max_ch; i++) {	/* system effects */
		DATA_T *p = ch_buffer_thread[cdmt_ofs_2][i];
		Channel *cp = channel + i;
		
		if(cdmt_cv[cdmt_ofs_2].channel_send[i] != SEND_EFFECT)
			continue;
		if(cdmt_cv[cdmt_ofs_2].channel_silent[i]
			&& ((cdmt_cv[cdmt_ofs_2].current_sample - cdmt_cv[cdmt_ofs_2].channel_lasttime[i])
			< (cdmt_cv[cdmt_ofs_2].channel_delay_out[i] + delay_out_count[1]))) {
			if (!is_insertion_effect_xg(i) && opt_drum_effect && ISDRUMCHANNEL(i) && cp->drum_effect_num) {
				for (j = 0; j < cp->drum_effect_num; j++) {
					struct DrumPartEffect *de = &(cp->drum_effect[j]);
					if (de->delay_send > 0)
						set_ch_delay_thread(de->buf_ptr[cdmt_ofs_2], cdmt_cv[cdmt_ofs_2].cnt, de->delay_send);
				}
			} else {
				if(cdmt_cv[cdmt_ofs_2].channel_delay && cp->delay_level > 0)
					set_ch_delay_thread(p, cdmt_cv[cdmt_ofs_2].cnt, cp->delay_level);
			}
		}
	}
}

// send effect buffer GS/GM // master/eq
static void do_compute_data_midi_thread14(int thread_num)
{
	int i;
	int32 cs = cdmt_cv[cdmt_ofs_2].current_sample;

	for(i = 0; i <= cdmt_cv[cdmt_ofs_2].max_ch; i++) {	/* system effects */
		DATA_T *p = ch_buffer_thread[cdmt_ofs_2][i];		
		Channel *cp = channel + i;
				
		switch(cdmt_cv[cdmt_ofs_2].channel_send[i]){
		case SEND_EFFECT:
			if(cdmt_cv[cdmt_ofs_2].channel_silent[i] && ((cs - cdmt_cv[cdmt_ofs_2].channel_lasttime[i]) < delay_out_count[0])) {
				if(cdmt_cv[cdmt_ofs_2].channel_eq && cp->eq_gs)	
					set_ch_eq_gs_thread(p, cdmt_cv[cdmt_ofs_2].cnt);
				else
					mix_signal(master_buffer[cdmt_ofs_2], p, cdmt_cv[cdmt_ofs_2].cnt);
			}
			break;
		case SEND_OUTPUT:
			mix_signal(master_buffer[cdmt_ofs_2], p, cdmt_cv[cdmt_ofs_2].cnt);
			break;
		}
	}
}

// send effect buffer GS/GM // reverb
static void do_compute_data_midi_thread15(int thread_num)
{
	int i, j;
	int32 cs = cdmt_cv[cdmt_ofs_2].current_sample;

	for(i = 0; i <= cdmt_cv[cdmt_ofs_2].max_ch; i++) {	/* system effects */
		DATA_T *p = ch_buffer_thread[cdmt_ofs_2][i];		
		Channel *cp = channel + i;
				
		if(cdmt_cv[cdmt_ofs_2].channel_send[i] != SEND_EFFECT)
			continue;
		if(cdmt_cv[cdmt_ofs_2].channel_silent[i] && ((cs - cdmt_cv[cdmt_ofs_2].channel_lasttime[i]) < delay_out_count[0])) {
			if (opt_drum_effect && ISDRUMCHANNEL(i) && cp->drum_effect_num) {
				for (j = 0; j < cp->drum_effect_num; j++) {
					struct DrumPartEffect *de = &(cp->drum_effect[j]);
					if (de->reverb_send > 0)
						set_ch_reverb_thread(de->buf_ptr[cdmt_ofs_2], cdmt_cv[cdmt_ofs_2].cnt, de->reverb_send);
				}
			} else {
				if(cdmt_cv[cdmt_ofs_2].channel_reverb && cp->reverb_level > 0)
					set_ch_reverb_thread(p, cdmt_cv[cdmt_ofs_2].cnt, cp->reverb_level);
			}
		}
	}
}

// send effect buffer GS/GM // chorus
static void do_compute_data_midi_thread16(int thread_num)
{
	int i, j;
	int32 cs = cdmt_cv[cdmt_ofs_2].current_sample;

	for(i = 0; i <= cdmt_cv[cdmt_ofs_2].max_ch; i++) {	/* system effects */
		DATA_T *p = ch_buffer_thread[cdmt_ofs_2][i];		
		Channel *cp = channel + i;
				
		if(cdmt_cv[cdmt_ofs_2].channel_send[i] != SEND_EFFECT)
			continue;
		if(cdmt_cv[cdmt_ofs_2].channel_silent[i] && ((cs - cdmt_cv[cdmt_ofs_2].channel_lasttime[i]) < delay_out_count[0])) {
			if (opt_drum_effect && ISDRUMCHANNEL(i) && cp->drum_effect_num) {
				for (j = 0; j < cp->drum_effect_num; j++) {
					struct DrumPartEffect *de = &(cp->drum_effect[j]);
					if (de->chorus_send > 0)
						set_ch_chorus_thread(de->buf_ptr[cdmt_ofs_2], cdmt_cv[cdmt_ofs_2].cnt, de->chorus_send);
				}
			} else {
				if(cdmt_cv[cdmt_ofs_2].channel_chorus && cp->chorus_level > 0)
					set_ch_chorus_thread(p, cdmt_cv[cdmt_ofs_2].cnt, cp->chorus_level);
			}
		}
	}
}

// send effect buffer GS/GM // delay
static void do_compute_data_midi_thread17(int thread_num)
{
	int i, j;
	int32 cs = cdmt_cv[cdmt_ofs_2].current_sample;

	for(i = 0; i <= cdmt_cv[cdmt_ofs_2].max_ch; i++) {	/* system effects */
		DATA_T *p = ch_buffer_thread[cdmt_ofs_2][i];		
		Channel *cp = channel + i;
				
		if(cdmt_cv[cdmt_ofs_2].channel_send[i] != SEND_EFFECT)
			continue;
		if(cdmt_cv[cdmt_ofs_2].channel_silent[i] && ((cs - cdmt_cv[cdmt_ofs_2].channel_lasttime[i]) < delay_out_count[0])) {
			if (opt_drum_effect && ISDRUMCHANNEL(i) && cp->drum_effect_num) {
				for (j = 0; j < cp->drum_effect_num; j++) {
					struct DrumPartEffect *de = &(cp->drum_effect[j]);
					if (de->delay_send > 0)
						set_ch_delay_thread(de->buf_ptr[cdmt_ofs_2], cdmt_cv[cdmt_ofs_2].cnt, de->delay_send);
				}
			} else {
				if(cdmt_cv[cdmt_ofs_2].channel_delay && cp->delay_level > 0)
					set_ch_delay_thread(p, cdmt_cv[cdmt_ofs_2].cnt, cp->delay_level);
			}
		}
	}
}

// send effect buffer GS/GM // ins
static void do_compute_data_midi_thread18(int thread_num)
{
	int i;
	int insertion_lasttime = INT_MIN / 2;
	int insertion_silent = 0;
	int32 cs = cdmt_cv[cdmt_ofs_2].current_sample;

	for(i = 0; i <= cdmt_cv[cdmt_ofs_2].max_ch; i++) {	/* system effects */
		DATA_T *p = ch_buffer_thread[cdmt_ofs_2][i];		
		Channel *cp = channel + i;
			
		if(cdmt_cv[cdmt_ofs_2].channel_send[i] != SEND_GS_INS)
			continue;	
		if(cdmt_cv[cdmt_ofs_2].channel_lasttime[i] > insertion_lasttime)
			insertion_lasttime = cdmt_cv[cdmt_ofs_2].channel_lasttime[i];
		insertion_silent += cdmt_cv[cdmt_ofs_2].channel_silent[i];
		set_ch_insertion_gs_thread(p, cdmt_cv[cdmt_ofs_2].cnt);
	}
	if(opt_insertion_effect && insertion_silent
		&& ((cs - insertion_lasttime) < delay_out_count[delay_out_type_gs[insertion_effect_gs.type_msb][insertion_effect_gs.type_lsb]]) ){
		cdmt_cv[cdmt_ofs_2].channel_ins = 1;
	}else{
		cdmt_cv[cdmt_ofs_2].channel_ins = 0;
	}
}

// send effect buffer GM2 // master
static void do_compute_data_midi_thread19(int thread_num)
{
	int i;
	int32 cs = cdmt_cv[cdmt_ofs_2].current_sample;

	for(i = 0; i <= cdmt_cv[cdmt_ofs_2].max_ch; i++) {	/* system effects */
		DATA_T *p = ch_buffer_thread[cdmt_ofs_2][i];
		Channel *cp = channel + i;
				
		switch(cdmt_cv[cdmt_ofs_2].channel_send[i]){
		case SEND_EFFECT:
			if(cdmt_cv[cdmt_ofs_2].channel_silent[i] && ((cs - cdmt_cv[cdmt_ofs_2].channel_lasttime[i]) < delay_out_count[0])) {
				mix_signal(master_buffer[cdmt_ofs_2], p, cdmt_cv[cdmt_ofs_2].cnt);
			}
			break;
		case SEND_OUTPUT:
			mix_signal(master_buffer[cdmt_ofs_2], p, cdmt_cv[cdmt_ofs_2].cnt);
			break;
		}
	}
}

// send effect buffer GM2 // reverb
static void do_compute_data_midi_thread20(int thread_num)
{
	int i, j;
	int32 cs = cdmt_cv[cdmt_ofs_2].current_sample;

	for(i = 0; i <= cdmt_cv[cdmt_ofs_2].max_ch; i++) {	/* system effects */
		DATA_T *p = ch_buffer_thread[cdmt_ofs_2][i];
		Channel *cp = channel + i;
				
		if(cdmt_cv[cdmt_ofs_2].channel_send[i] != SEND_EFFECT)
			continue;
		if(cdmt_cv[cdmt_ofs_2].channel_silent[i] && ((cs - cdmt_cv[cdmt_ofs_2].channel_lasttime[i]) < delay_out_count[0])) {
			if (opt_drum_effect && ISDRUMCHANNEL(i) && cp->drum_effect_num) {
				for (j = 0; j < cp->drum_effect_num; j++) {
					struct DrumPartEffect *de = &(cp->drum_effect[j]);
					if (de->reverb_send > 0)
						set_ch_reverb_thread(de->buf_ptr[cdmt_ofs_2], cdmt_cv[cdmt_ofs_2].cnt, de->reverb_send);
				}
			} else {
				if(cdmt_cv[cdmt_ofs_2].channel_reverb && cp->reverb_level > 0)
					set_ch_reverb_thread(p, cdmt_cv[cdmt_ofs_2].cnt, cp->reverb_level);
			}
		}
	}
}

// send effect buffer GM2 // chorus
static void do_compute_data_midi_thread21(int thread_num)
{
	int i, j;
	int32 cs = cdmt_cv[cdmt_ofs_2].current_sample;

	for(i = 0; i <= cdmt_cv[cdmt_ofs_2].max_ch; i++) {	/* system effects */
		DATA_T *p = ch_buffer_thread[cdmt_ofs_2][i];
		Channel *cp = channel + i;
				
		if(cdmt_cv[cdmt_ofs_2].channel_send[i] != SEND_EFFECT)
			continue;
		if(cdmt_cv[cdmt_ofs_2].channel_silent[i] && ((cs - cdmt_cv[cdmt_ofs_2].channel_lasttime[i]) < delay_out_count[0])) {
			if (opt_drum_effect && ISDRUMCHANNEL(i) && cp->drum_effect_num) {
				for (j = 0; j < cp->drum_effect_num; j++) {
					struct DrumPartEffect *de = &(cp->drum_effect[j]);
					if (de->chorus_send > 0)
						set_ch_chorus_thread(de->buf_ptr[cdmt_ofs_2], cdmt_cv[cdmt_ofs_2].cnt, de->chorus_send);
				}
			} else {
				if(cdmt_cv[cdmt_ofs_2].channel_chorus && cp->chorus_level > 0)
					set_ch_chorus_thread(p, cdmt_cv[cdmt_ofs_2].cnt, cp->chorus_level);
			}
		}
	}
}

// send effect buffer GM2 // mfx
static void do_compute_data_midi_thread22(int thread_num)
{
	int i, j;
	int mfx_lasttime[SD_MFX_EFFECT_NUM] = {INT_MIN / 2, INT_MIN / 2, INT_MIN / 2,};
	int mfx_silent[SD_MFX_EFFECT_NUM] = {0, 0, 0,};
	int32 cs = cdmt_cv[cdmt_ofs_2].current_sample;

	for(i = 0; i <= cdmt_cv[cdmt_ofs_2].max_ch; i++) {	/* system effects */
		DATA_T *p = ch_buffer_thread[cdmt_ofs_2][i];
		Channel *cp = channel + i;
				
		if(cdmt_cv[cdmt_ofs_2].channel_send[i] != SEND_SD_MFX)
			continue;
		if(cp->lasttime > mfx_lasttime[cp->sd_output_mfx_select])
			mfx_lasttime[cp->sd_output_mfx_select] = cp->lasttime;
		mfx_silent[cp->sd_output_mfx_select] += cdmt_cv[cdmt_ofs_2].channel_silent[i];
		set_ch_mfx_sd_thread(p, cdmt_cv[cdmt_ofs_2].cnt, cp->sd_output_mfx_select, cp->sd_dry_send_level);
	}
	for(j = 0; j < SD_MFX_EFFECT_NUM; j++){
		if(opt_insertion_effect && mfx_silent[j] && ((cs - mfx_lasttime[j]) < delay_out_count[delay_out_type_sd[*mfx_effect_sd->type]]) ){
			cdmt_cv[cdmt_ofs_2].channel_mfx[j] = 1;
		}else{
			cdmt_cv[cdmt_ofs_2].channel_mfx[j] = 0;
		}
	}
}

// send effect XG
static void do_compute_data_midi_thread30(int thread_num)
{	
		switch(thread_num){
		case 0: do_compute_data_midi_thread10(thread_num); break; // master
		case 1: do_compute_data_midi_thread11(thread_num); break; // reverb
		case 2: do_compute_data_midi_thread12(thread_num); break; // chorus
		case 3: do_compute_data_midi_thread13(thread_num); break; // var
		}
}

// send effect GS/GM
static void do_compute_data_midi_thread31(int thread_num)
{
		switch(thread_num){
		case 0: do_compute_data_midi_thread14(thread_num); break; // master
		case 1: do_compute_data_midi_thread15(thread_num); break; // reverb
		case 2: do_compute_data_midi_thread16(thread_num); break; // chorus
		case 3: do_compute_data_midi_thread17(thread_num); break; // delay
		case 4: do_compute_data_midi_thread18(thread_num); break; // ins				
		}	
}

// send effect SD/GM2
static void do_compute_data_midi_thread32(int thread_num)
{
		switch(thread_num){
		case 0: do_compute_data_midi_thread19(thread_num); break; // master
		case 1: do_compute_data_midi_thread20(thread_num); break; // reverb
		case 2: do_compute_data_midi_thread21(thread_num); break; // chorus
		case 3: do_compute_data_midi_thread22(thread_num); break; // mfx
		}
}


static void thread_mix_voice_common(int i, int thread_num)
{
	if (voice[i].status != VOICE_FREE){
		int ch = voice[i].channel;
		cdmt_cv[cdmt_ofs_0].voice_free[i] = 1;
		if(cdmt_cv[cdmt_ofs_0].channel_silent[ch]) {
			memset(mix_voice_buffer[i], 0, cdmt_cv[cdmt_ofs_0].n);
			mix_voice_thread(mix_voice_buffer[i], i, cdmt_cv[cdmt_ofs_0].count, thread_num);
		}else{
			free_voice(i);
		}
	}else{
		cdmt_cv[cdmt_ofs_0].voice_free[i] = 0;
	}
}


// mix voice // ch mix // system effect
void do_compute_data_midi_thread_block1(int thread_num)
{
	int i, j;
	
	// master effect	
	if(thread_num == 0) 
		do_master_effect_thread();
	// do effect	
	else if(thread_num >= 1 && thread_num <= 6) {
		if (cdmt_cv[cdmt_ofs_3].channel_effect){
			int num = thread_num - 1;
			switch(play_system_mode){
			case XG_SYSTEM_MODE: 	/* XG */
				do_compute_data_midi_thread5(num);
				break;
			case SD_SYSTEM_MODE:	/* SD */
				do_compute_data_midi_thread7(num);
				break;
			case GM2_SYSTEM_MODE:	/* GM2 */
				do_compute_data_midi_thread8(num);
				break;
			default:	/* GM & GS */
				do_compute_data_midi_thread6(num);
				break;
			}
		}
	}	
	// send effect
	else if(thread_num >= 7 && thread_num <= 11) {
		if (cdmt_cv[cdmt_ofs_2].channel_effect){
			int num = thread_num - 7;
			switch(play_system_mode){
			case XG_SYSTEM_MODE: 	/* XG */
				switch(num){
				case 0: do_compute_data_midi_thread10(num); break; // master
				case 1: do_compute_data_midi_thread11(num); break; // reverb
				case 2: do_compute_data_midi_thread12(num); break; // chorus
				case 3: do_compute_data_midi_thread13(num); break; // var
				}
				break;
			case SD_SYSTEM_MODE:	/* SD */
			case GM2_SYSTEM_MODE:	/* GM2 */
				switch(num){
				case 0: do_compute_data_midi_thread19(num); break; // master
				case 1: do_compute_data_midi_thread20(num); break; // reverb
				case 2: do_compute_data_midi_thread21(num); break; // chorus
				case 3: do_compute_data_midi_thread22(num); break; // mfx
				}
				break;
			default:	/* GM & GS */
				switch(num){
				case 0: do_compute_data_midi_thread14(num); break; // master
				case 1: do_compute_data_midi_thread15(num); break; // reverb
				case 2: do_compute_data_midi_thread16(num); break; // chorus
				case 3: do_compute_data_midi_thread17(num); break; // delay
				case 4: do_compute_data_midi_thread18(num); break; // ins				
				}
				break;
			}
		}
	}
	// compute_var		
	else if(thread_num == 12) {
		for(i = 0; i <= cdmt_cv[cdmt_ofs_0].max_ch; i++)
			cdmt_cv[cdmt_ofs_0].channel_lasttime[i] = channel[i].lasttime;

		if(!cdmt_cv[cdmt_ofs_0].channel_effect){ // channel VST // all effect off
			//if(adjust_panning_immediately)
			//	cdmt_cv[cdmt_ofs_0].flg_ch_mix = 1;
		}else switch(play_system_mode){
		case XG_SYSTEM_MODE:	/* XG */
			if(adjust_panning_immediately || opt_drum_effect || cdmt_cv[cdmt_ofs_0].channel_eq || opt_insertion_effect)
				cdmt_cv[cdmt_ofs_0].flg_ch_mix = 1;
			cdmt_cv[cdmt_ofs_0].multi_eq_xg_valid = multi_eq_xg.valid;
			break;
		case GM2_SYSTEM_MODE:	/* GM2 */
		case SD_SYSTEM_MODE:	/* SD */
			if(opt_drum_effect)
				cdmt_cv[cdmt_ofs_0].flg_ch_mix = 1;
			cdmt_cv[cdmt_ofs_0].multi_eq_sd_valid = multi_eq_sd.valid;
			break;
		default:	/* GM & GS */
			if(opt_drum_effect)	// adjust_panning_immediately || 
				cdmt_cv[cdmt_ofs_0].flg_ch_mix = 1;
			break;
		}
	}	
	if(thread_num >= CDM_JOB_VC_OFFSET){
		int num = thread_num - CDM_JOB_VC_OFFSET;
		// clear buffers of channnel buffer
		for(i = num; i <= cdmt_cv[cdmt_ofs_0].max_ch; i += cdm_job_num)
				memset(ch_buffer_thread[cdmt_ofs_0][i], 0, cdmt_cv[cdmt_ofs_0].n); // clear

		/* clear buffers of drum-part effect */
		if(cdmt_cv[cdmt_ofs_0].channel_effect) {
			for(i = num; i <= cdmt_cv[cdmt_ofs_0].max_ch; i += cdm_job_num){
				if (!opt_drum_effect || !ISDRUMCHANNEL(i))
					continue;
				for (j = 0; j < channel[i].drum_effect_num; j++) 
					if (channel[i].drum_effect[j].buf_ptr[cdmt_ofs_0] != NULL)
						memset(channel[i].drum_effect[j].buf_ptr[cdmt_ofs_0], 0, cdmt_cv[cdmt_ofs_0].n);			
			}
		}
		// mix voice
		for (i = num; i < cdmt_cv[cdmt_ofs_0].uv; i += cdm_job_num) 
			thread_mix_voice_common(i, num);

		// ch mix
		if (cdmt_cv[cdmt_ofs_1].channel_effect){
			switch(play_system_mode){
			case XG_SYSTEM_MODE: 	/* XG */
				do_compute_data_midi_thread2(num);
				break;
			default:	/* GM & GS & GM2 & SD */
				do_compute_data_midi_thread3(num);
				break;
			}
		}
		//else{ // all effect off // ch vst
		//	if(cdmt_cv[cdmt_ofs_0].flg_ch_mix)
		//		do_compute_data_midi_thread4(thread_num);
		//}
	}

}



/* do_compute_data_midi() with DSP Effect */
// countが変化しないこと
static void do_compute_data_midi_thread(int32 count)
{
	int i, j, uv = upper_voices, max_ch = current_file_info->max_channel;
	int stereo = ! (play_mode->encoding & PE_MONO);
	int32 cnt = count * ((stereo) ? 2 : 1);
	int32 n = cnt * sizeof(DATA_T); /* in bytes */	
	struct DrumPartEffect *de;
	
	/* output buffer clear */
	memset(buffer_pointer, 0, n); // must

	if(opt_realtime_playing)
		if(compute_data_midi_skip(count, cnt)){	// silent skip
			current_sample += count;
			return;
		}

	// calc thread delay count
	cdmt_ofs_0 = (cdmt_ofs_0 + 1) & 0x3;
	cdmt_ofs_1 = (cdmt_ofs_1 + 1) & 0x3;
	cdmt_ofs_2 = (cdmt_ofs_2 + 1) & 0x3;
	cdmt_ofs_3 = (cdmt_ofs_3 + 1) & 0x3;
	cdmt_buf_i = (cdmt_buf_i + 1) & 0x1;
	cdmt_buf_o = (cdmt_buf_o + 1) & 0x1;	
	// clear flag
	memset(&cdmt_cv[cdmt_ofs_0], 0, sizeof(compute_data_midi_thread_var));	
	if(max_ch >= MAX_CHANNELS) max_ch = MAX_CHANNELS - 1;	
	/* buffer clear */
	memset(master_buffer[cdmt_ofs_0], 0, n);		
	// compute_var
	cdmt_cv[cdmt_ofs_0].count = count;
	cdmt_cv[cdmt_ofs_0].cnt = cnt;
	cdmt_cv[cdmt_ofs_0].n = n;
	cdmt_cv[cdmt_ofs_0].uv = uv;
	cdmt_cv[cdmt_ofs_0].max_ch = max_ch;
	cdmt_cv[cdmt_ofs_0].current_sample = current_sample;

	/* are effects valid? / don't supported in mono */
	if(!stereo){
		cdmt_cv[cdmt_ofs_0].channel_reverb = 0;
		cdmt_cv[cdmt_ofs_0].channel_chorus = 0;
		cdmt_cv[cdmt_ofs_0].channel_delay = 0;
	}else{
		cdmt_cv[cdmt_ofs_0].channel_reverb = ((opt_reverb_control > 0 && opt_reverb_control & 0x1)
				|| (opt_reverb_control < 0 && opt_reverb_control & 0x80));
		cdmt_cv[cdmt_ofs_0].channel_chorus = opt_chorus_control;
		cdmt_cv[cdmt_ofs_0].channel_delay = opt_delay_control > 0;
	}
	/* is EQ valid? */
	cdmt_cv[cdmt_ofs_0].channel_eq = opt_eq_control && (eq_status_gs.low_gain != 0x40 || eq_status_gs.high_gain != 0x40 ||
		play_system_mode == XG_SYSTEM_MODE);	
	/* is ch vst valid? */
#ifdef VST_LOADER_ENABLE		// elion add.
	cdmt_cv[cdmt_ofs_0].flg_ch_vst = (hVSTHost != NULL && use_vst_channel) ? 1 : 0;
#else
	cdmt_cv[cdmt_ofs_0].flg_ch_vst = 0;
#endif // VST_LOADER_ENABLE
	cdmt_cv[cdmt_ofs_0].channel_effect = !cdmt_cv[cdmt_ofs_0].flg_ch_vst && (stereo && (cdmt_cv[cdmt_ofs_0].channel_reverb || cdmt_cv[cdmt_ofs_0].channel_chorus
			|| cdmt_cv[cdmt_ofs_0].channel_delay || cdmt_cv[cdmt_ofs_0].channel_eq || opt_insertion_effect));
	/* is rev/cho vst both valid? */
//	if (opt_normal_chorus_plus == 6 && (opt_reverb_control >= 9 || opt_reverb_control <= -1024))
//		cdmt_cv[cdmt_ofs_0].flg_sys_vst = 1;
//	else
//		cdmt_cv[cdmt_ofs_0].flg_sys_vst = 0;
	// serach channel.lasttime
	for(i = 0; i < uv; i++)
		if(voice[i].status != VOICE_FREE)	{channel[voice[i].channel].lasttime = current_sample + count;}

	/* appropriate buffers for channels */
	if(cdmt_cv[cdmt_ofs_0].channel_effect) {
		for(i = 0; i <= max_ch; i++) {
			if(!IS_SET_CHANNELMASK(channel_mute, i))
				cdmt_cv[cdmt_ofs_0].channel_silent[i] = 1;
			if(cdmt_cv[cdmt_ofs_0].flg_ch_vst){
				cdmt_cv[cdmt_ofs_0].channel_send[i] = SEND_OUTPUT;
			} else if(opt_insertion_effect && channel[i].insertion_effect) { // GS INS
				cdmt_cv[cdmt_ofs_0].channel_send[i] = SEND_GS_INS;
			} else if(opt_insertion_effect && (play_system_mode == SD_SYSTEM_MODE) && is_mfx_effect_sd(i)) { // SD MFX
				cdmt_cv[cdmt_ofs_0].channel_send[i] = SEND_SD_MFX;
			} else  if(!opt_drum_effect && ISDRUMCHANNEL(i)	) {
				make_drum_effect(i);
				cdmt_cv[cdmt_ofs_0].channel_drumfx[i] = 1;
				cdmt_cv[cdmt_ofs_0].channel_send[i] = SEND_EFFECT;
			} else if( (play_system_mode == XG_SYSTEM_MODE && is_insertion_effect_xg(i))
				|| channel[i].eq_gs
				|| channel[i].reverb_level > 0
				|| channel[i].chorus_level > 0
				|| channel[i].delay_level > 0
				|| channel[i].eq_xg.valid
				|| channel[i].dry_level != 127	) {
				cdmt_cv[cdmt_ofs_0].channel_send[i] = SEND_EFFECT;
			} else
				cdmt_cv[cdmt_ofs_0].channel_send[i] = SEND_OUTPUT;
		}
	}else{ // all effect off // ch vst
		for(i = 0; i <= max_ch; i++)
			if(!IS_SET_CHANNELMASK(channel_mute, i))
				cdmt_cv[cdmt_ofs_0].channel_silent[i] = 1;
	}
		
	// mt_compute1
	go_compute_thread(do_compute_data_midi_thread_block1, cdm_job_num_total);
	
	// voice to ch/drumfx
	for (i = 0; i < uv; i++) {
		int	ch;
		if (!cdmt_cv[cdmt_ofs_0].voice_free[i])
			continue;
		ch = voice[i].channel;
		if(cdmt_cv[cdmt_ofs_0].channel_silent[ch]){
			if(cdmt_cv[cdmt_ofs_0].channel_drumfx[ch]){
				int dx_flg = 0, dx_num = 0;
				for (j = 0; j < channel[ch].drum_effect_num; j++){
					if (channel[ch].drum_effect[j].note == voice[i].note){
						dx_flg = 1;	dx_num = j;
					}
				}
				if(dx_flg)
					mix_signal_clear(channel[ch].drum_effect[dx_num].buf_ptr[cdmt_ofs_0], mix_voice_buffer[i], cnt);
				else
					mix_signal_clear(ch_buffer_thread[cdmt_ofs_0][ch], mix_voice_buffer[i], cnt);
			}else
				mix_signal_clear(ch_buffer_thread[cdmt_ofs_0][ch], mix_voice_buffer[i], cnt); // ch
		}
		if(voice[i].status == VOICE_FREE){ // inc timeout
			ctl_note_event(i);	
#ifdef VOICE_EFFECT
			uninit_voice_effect(i);
#endif
		}
	}
	while(uv > 0 && voice[uv - 1].status == VOICE_FREE)	{uv--;}
	upper_voices = uv;
	
	// send effect
	if(!cdmt_cv[cdmt_ofs_1].channel_effect){
		// channel VST
#ifdef VST_LOADER_ENABLE		// elion add.
		if (cdmt_cv[cdmt_ofs_1].flg_ch_vst)
			do_channel_vst(buffer_pointer, ch_buffer_thread[cdmt_ofs_1], cnt, max_ch);
		else
#endif // VST_LOADER_ENABLE
		// all effect off
		for(i = 0; i <= max_ch; i++)
			mix_signal(buffer_pointer, ch_buffer_thread[cdmt_ofs_1][i], cnt);
#ifndef MASTER_VST_EFFECT2	
		do_effect_thread(buffer_pointer, count, n);
#endif
		if(opt_realtime_playing)
			compute_data_midi_skip(0, cnt); // silent skip , buffer check
		current_sample += count;
		return;
	}

	// return effect
	if(cdmt_cv[cdmt_ofs_3].channel_effect){
		switch(play_system_mode) {
		case XG_SYSTEM_MODE: /* XG */
			/* mixing signal and applying system effects */ 
			if(cdmt_cv[cdmt_ofs_3].channel_delay) {mix_variation_effect1_xg_thread(master_buffer[cdmt_ofs_3], cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
			if(cdmt_cv[cdmt_ofs_3].channel_chorus) {mix_ch_chorus_xg_thread(master_buffer[cdmt_ofs_3], cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
			if(cdmt_cv[cdmt_ofs_3].channel_reverb) {mix_ch_reverb_xg_thread(master_buffer[cdmt_ofs_3], cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
			if(cdmt_cv[cdmt_ofs_3].multi_eq_xg_valid) {mix_multi_eq_xg_thread(master_buffer[cdmt_ofs_3], cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
			memcpy(buffer_pointer, master_buffer[cdmt_ofs_3], cdmt_cv[cdmt_ofs_3].n);
			break;
		case GM2_SYSTEM_MODE:	/* GM2 */
			/* mfx effect */
			for(j = 0; j < SD_MFX_EFFECT_NUM; j++){
				if(cdmt_cv[cdmt_ofs_3].channel_mfx[j]) {mix_mfx_effect_sd_thread(master_buffer[cdmt_ofs_3], cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n, j);}
			}
			if(cdmt_cv[cdmt_ofs_3].channel_chorus) {mix_ch_chorus_gm2_thread(master_buffer[cdmt_ofs_3], cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
			if(cdmt_cv[cdmt_ofs_3].channel_reverb) {mix_ch_reverb_gm2_thread(master_buffer[cdmt_ofs_3], cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
			if(cdmt_cv[cdmt_ofs_3].multi_eq_sd_valid) {mix_multi_eq_sd_thread(master_buffer[cdmt_ofs_3], cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
			memcpy(buffer_pointer, master_buffer[cdmt_ofs_3], cdmt_cv[cdmt_ofs_3].n);
			break;
		case SD_SYSTEM_MODE:	/* SD */
			/* mfx effect */
			for(j = 0; j < SD_MFX_EFFECT_NUM; j++){
				if(cdmt_cv[cdmt_ofs_3].channel_mfx[j]) {mix_mfx_effect_sd_thread(master_buffer[cdmt_ofs_3], cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n, j);}
			}
			if(cdmt_cv[cdmt_ofs_3].channel_chorus) {mix_ch_chorus_sd_thread(master_buffer[cdmt_ofs_3], cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
			if(cdmt_cv[cdmt_ofs_3].channel_reverb) {mix_ch_reverb_sd_thread(master_buffer[cdmt_ofs_3], cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
			if(cdmt_cv[cdmt_ofs_3].multi_eq_sd_valid) {mix_multi_eq_sd_thread(master_buffer[cdmt_ofs_3], cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
			memcpy(buffer_pointer, master_buffer[cdmt_ofs_3], cdmt_cv[cdmt_ofs_3].n);
			break;
		default:	/* GM & GS */
			/* insertion effect */ /* sending insertion effect voice to channel effect, eq, dry */
			/* mixing signal and applying system effects */ 
			if(cdmt_cv[cdmt_ofs_3].channel_ins) {mix_insertion_effect_gs_thread(master_buffer[cdmt_ofs_3], cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n, cdmt_cv[cdmt_ofs_3].channel_eq);}
			if(cdmt_cv[cdmt_ofs_3].channel_eq) {mix_ch_eq_gs_thread(master_buffer[cdmt_ofs_3], cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
			if(cdmt_cv[cdmt_ofs_3].channel_chorus) {mix_ch_chorus_thread(master_buffer[cdmt_ofs_3], cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
			if(cdmt_cv[cdmt_ofs_3].channel_delay) {mix_ch_delay_thread(master_buffer[cdmt_ofs_3], cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
			if(cdmt_cv[cdmt_ofs_3].channel_reverb) {mix_ch_reverb_thread(master_buffer[cdmt_ofs_3], cdmt_cv[cdmt_ofs_3].cnt, cdmt_cv[cdmt_ofs_3].n);}
			memcpy(buffer_pointer, master_buffer[cdmt_ofs_3], cdmt_cv[cdmt_ofs_3].n);
			break;
		}
	}
	// elion add.
	// move effect.c do_effect()
	//mix_compressor(buffer_pointer, cnt);
#ifndef MASTER_VST_EFFECT2	
    do_effect_thread(buffer_pointer, count, n);
#endif
	if(opt_realtime_playing)
		compute_data_midi_skip(0, cnt); // silent skip , buffer check
	current_sample += count;	
}

