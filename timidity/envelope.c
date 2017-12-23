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

*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "output.h"
#include "controls.h"
#include "tables.h"
#include "mix.h"
#include "envelope.h"



////////// new amp_env , mod_env //////////

// vol max ENV0_OFFSET_MAX
static double calc_envelope0_curve(double vol, int curve){
#if 1 // use table 
	FLOAT_T *table;
	switch(curve){
	default:
	case LINEAR_CURVE:
		return vol * DIV_ENV0_OFFSET_MAX;
	case SF2_CONCAVE:
		table = sf2_concave_table;
		break;
	case SF2_CONVEX:
		table = sf2_convex_table;
		break;
	case DEF_VOL_CURVE:
		table = def_vol_table;
		break;
	case GS_VOL_CURVE:
		table = gs_vol_table;
		break;
	case XG_VOL_CURVE:
		table = xg_vol_table;
		break;
	case SF2_VOL_CURVE:
		table = sb_vol_table;
		break;		
	case MODENV_VOL_CURVE:
		table = modenv_vol_table;
		break;
	}
#if 1 // table interporation
	{
		double fp = vol * DIV_ENV0_OFFSET_MAX * 1023.0;
		double fp2 = floor(fp);
		int32 index = fp2;
		double v1 = table[index];
		double v2 = table[index + 1];
		return (v1 + (v2 - v1) * (fp - fp2));
	}
#else // no interporation
	{
		return table[(int)(vol * DIV_ENV0_OFFSET_MAX * 1023.0)];;
	}
#endif
#else // calc
	double tmp;
	if(vol > DIV_ENV0_OFFSET_MAX)
		return 1.0;
	else(vol <= 0.0)
		return 0.0;
	else switch(curve){
	default:
	case LINEAR_CURVE:
		return vol * DIV_ENV0_OFFSET_MAX;
	case SF2_CONCAVE:
		vol *= DIV_ENV0_OFFSET_MAX;
		return ((vol + 1.0 - sqrt(1.0 - vol * vol)) * DIV_2);
	case SF2_CONVEX:
		vol *= DIV_ENV0_OFFSET_MAX;
		tmp = 1.0 - vol;
		return ((vol + sqrt(1.0 - tmp * tmp)) * DIV_2);
	case DEF_VOL_CURVE:
		return pow(2.0, (vol * DIV_ENV0_OFFSET_MAX - 1.0) * 6);
	case GS_VOL_CURVE:
		return pow(2.0, (vol * DIV_ENV0_OFFSET_MAX - 1.0) * 8);
	case XG_VOL_CURVE:
		return pow(2.0, (vol * DIV_ENV0_OFFSET_MAX - 1.0) * 8);
	case SF2_VOL_CURVE:
		return pow(10.0, (1.0 - vol * DIV_ENV0_OFFSET_MAX) * 960.0 / (-200.0));
	case MODENV_VOL_CURVE:
		vol *= DIV_ENV0_OFFSET_MAX;
		tmp = (1.0 - (-20.0 * DIV_96 * log((vol * vol) * DIV_LN10)));
		if (tmp < 0) {tmp = 0;}
		return log(tmp + 1.0) * DIV_LN2;
	}
#endif
}

// in volume : 0.0~1.0
// out vol : 0.0 ~ ENV0_OFFSET_MAX
static double calc_rev_envelope0_curve(double volume, int curve){
// rev table
	const double rev_offset = ENV0_OFFSET_MAX / 1023.0;
	FLOAT_T *table;
	int i = 0;
	double v1, v2;
	
	if(volume >= 1.0)
		return ENV0_OFFSET_MAX;
	else if(volume <= 0.0)
		return 0.0;
	else switch(curve){
	default:
	case LINEAR_CURVE:
		return volume * ENV0_OFFSET_MAX;
	case SF2_CONCAVE:
		table = sf2_concave_table;
		break;
	case SF2_CONVEX:
		table = sf2_convex_table;
		break;
	case DEF_VOL_CURVE:
		table = def_vol_table;
		break;
	case GS_VOL_CURVE:
		table = gs_vol_table;
		break;
	case XG_VOL_CURVE:
		table = xg_vol_table;
		break;
	case SF2_VOL_CURVE:
		table = sb_vol_table;
		break;		
	case MODENV_VOL_CURVE:
		table = modenv_vol_table;
		break;
	}
#if 1 // max 20cycle
	while(i < 1024 && volume >= table[i]){ i += 256; } // max 4cycle
	i -= 256;
	while(i < 1024 && volume >= table[i]){ i += 64; } // max 4cycle
	i -= 64;
	while(i < 1024 && volume >= table[i]){ i += 16; } // max 4cycle
	i -= 16;
	while(i < 1024 && volume >= table[i]){ i += 4; } // max 4cycle
	i -= 4;
	while(i < 1024 && volume >= table[i]){ i += 1; } // max 4cycle
#else // max 1024 cycle
	while(i < 1024 && volume >= table[i]){ i += 1; } // max 1024cycle
#endif
#if 1 // table interporation
	v2 = table[i--];		
	v1 = table[i];
//	volume = (v1 + (v2 - v1) * ((vol * DIV_ENV0_OFFSET_MAX * 1023.0) - i));
//	vol = ((volume - v1) / (v2 - v1) + i) / (DIV_ENV0_OFFSET_MAX * 1023.0);
	if(v1 != v2)
		return ((volume - v1) / (v2 - v1) + i) * rev_offset;
	else
		return (double)i * rev_offset;
#else // no interporation
	return (double)i * rev_offset;
#endif
}

static void adjust_envelope0_stage(Envelope0 *env){
	if(env->stage < ENV0_RELEASE1_STAGE){ // noteon stage
		if(env->rate[ENV0_SUSTAIN_STAGE] < 0
			&& env->vol <= env->init_vol[ENV0_SUSTAIN_STAGE]
			&& env->vol > env->target_vol[ENV0_SUSTAIN_STAGE])
			env->stage = ENV0_SUSTAIN_STAGE;
		else if(env->rate[ENV0_DECAY_STAGE] < 0
			&& env->vol <= env->init_vol[ENV0_DECAY_STAGE]
			&& env->vol > env->target_vol[ENV0_DECAY_STAGE])
			env->stage = ENV0_DECAY_STAGE;
		else if(env->rate[ENV0_HOLD_STAGE] < 0
			&& env->vol <= env->init_vol[ENV0_HOLD_STAGE]
			&& env->vol > env->target_vol[ENV0_HOLD_STAGE])
			env->stage = ENV0_HOLD_STAGE;
		else
			env->stage = ENV0_SUSTAIN_STAGE; // bug? 
	}else{ // noteoff stage
		if(env->rate[ENV0_RELEASE3_STAGE] < 0
			&& env->vol <= env->init_vol[ENV0_RELEASE3_STAGE]
			&& env->vol > env->target_vol[ENV0_RELEASE3_STAGE])
			env->stage = ENV0_RELEASE3_STAGE;
		else if(env->rate[ENV0_RELEASE2_STAGE] < 0
			&& env->vol <= env->init_vol[ENV0_RELEASE2_STAGE]
			&& env->vol > env->target_vol[ENV0_RELEASE2_STAGE])
			env->stage = ENV0_RELEASE2_STAGE;
		else if(env->rate[ENV0_RELEASE1_STAGE] < 0
			&& env->vol <= env->init_vol[ENV0_RELEASE1_STAGE]
			&& env->vol > env->target_vol[ENV0_RELEASE1_STAGE])
			env->stage = ENV0_RELEASE1_STAGE;
		else
			env->stage = ENV0_RELEASE4_STAGE; // bug? 
	}
}

 // note on
void init_envelope0(Envelope0 *env){
	int i;
	memset(env, 0, sizeof(Envelope0));
	env->stage = ENV0_ATTACK_STAGE;
}

void apply_envelope0_param(Envelope0 *env){
	int i; 
	//int special_curve = 0;	
	for(i = 0; i < ENV0_STAGE_LIST_MAX; i++){
		if(i <= ENV0_ATTACK_STAGE){
			env->init_vol[i] = 0;
			env->target_vol[i] = env->offset[i];	
		}else if(i == ENV0_RELEASE1_STAGE){
			env->init_vol[i] = ENV0_OFFSET_MAX;	
			env->target_vol[i] = env->offset[i];	
			//if(env->target_vol[i] > 0)
			//	special_curve = 1;
		}else if(env->offset[i - 1] < env->offset[i]){
			env->init_vol[i] = env->offset[i - 1];
			env->target_vol[i] = env->offset[i];	
			//special_curve = 1;
		}else{
			env->init_vol[i] = env->offset[i - 1];
			env->target_vol[i] = env->offset[i];	
			env->rate[i] *= -1.0;
		}
		if(env->follow[i] <= 0)
			env->follow[i] = 1.0;
		env->add_vol[i] = env->rate[i] * env->follow[i];
	}
	//if(special_curve){ // ADDSR以外の場合 LINEAR_CURVEのみ (stage変更が複雑になるので
	//	for(i = 0; i < ENV0_STAGE_LIST_MAX; i++)
	//		env->curve[i] = LINEAR_CURVE;
	//}
	env->target_vol[ENV0_SUSTAIN_STAGE] = 0;	
	env->target_vol[ENV0_RELEASE4_STAGE] = 0;	
	env->status = env->delay ? ENV0_NOTE_ON_WAIT : ENV0_NOTE_ON;	
}


// stage変更はこの関数の後にする (変更前のstageが必要なので
static inline void set_noteoff_recalc_release_rate(Envelope0 *env){	
	int i;
	if(env->status < ENV0_NOTE_OFF || env->status == ENV0_NOTE_CUT_WAIT){
		if(env->status == ENV0_NOTE_CUT_WAIT)
			env->status = ENV0_NOTE_CUT;
		else
			env->status = ENV0_NOTE_OFF;	
		// カーブが変わるとvolは同じでも出力volumeは違うので 次ステージのカーブでvolumeが一致するvol値に変更
		if(env->curve[env->stage] != env->curve[ENV0_RELEASE1_STAGE])
			env->vol = calc_rev_envelope0_curve(env->volume, env->curve[ENV0_RELEASE1_STAGE]);		
		// RELEASE1の入力rateはoffset差がOFFSET_MAXで計算されてるので volとの差分でrate調整
		if(env->vol == env->target_vol[ENV0_RELEASE1_STAGE])
			env->rate[ENV0_RELEASE1_STAGE] *= -DIV_ENV0_OFFSET_MAX;
		else
			env->rate[ENV0_RELEASE1_STAGE] *= (env->target_vol[ENV0_RELEASE1_STAGE] - env->vol) * DIV_ENV0_OFFSET_MAX;
		env->add_vol[ENV0_RELEASE1_STAGE] = env->rate[ENV0_RELEASE1_STAGE] * env->follow[ENV0_RELEASE1_STAGE];
	}
	env->stage = ENV0_RELEASE1_STAGE; // set noteoff stage
}

 // noteoff cutnote killnote
void reset_envelope0_release(Envelope0 *env, int32 rate){
	int i;
	if(!rate){
		env->status = ENV0_NOTE_OFF_WAIT; // set noteoff
	}else if(env->status >= ENV0_NOTE_CUT){
		return;
	}else{ // if set cut_rate 
		if(env->status == ENV0_NOTE_OFF && env->stage >= ENV0_RELEASE1_STAGE) // すでにノートオフ後リリースの場合
			env->status = ENV0_NOTE_CUT; // そのままカットノートに移行
		else
			env->status = ENV0_NOTE_CUT_WAIT;
		for(i = ENV0_RELEASE1_STAGE; i < ENV0_STAGE_LIST_MAX; i++){
			double rate2 = -rate * env->vol * DIV_ENV0_OFFSET_MAX;
			if(env->rate[i] < rate)
				env->rate[i] = rate;
			if(env->add_vol[i] > rate2)
				env->add_vol[i] = rate2;
			env->target_vol[i] = 0;
			env->follow[i] = 1.0;
		}
	}
}

void reset_envelope0_damper(Envelope0 *env, int8 damper){
	if(env->status >= ENV0_NOTE_CUT_WAIT)
		return;
	if(damper == 127){
		env->stage = ENV0_SUSTAIN_STAGE; // set noteon stage	
		adjust_envelope0_stage(env);
		env->add_vol[ENV0_HOLD_STAGE] = env->rate[ENV0_HOLD_STAGE] * env->follow[ENV0_HOLD_STAGE];		
		env->add_vol[ENV0_DECAY_STAGE] = env->rate[ENV0_DECAY_STAGE] * env->follow[ENV0_DECAY_STAGE];		
		env->add_vol[ENV0_SUSTAIN_STAGE] = env->rate[ENV0_SUSTAIN_STAGE];
#if 1 // half damper
	}else if(damper > 0){
		int i, count = 0;
		double rate = 0;
		double ratio1 = (double)damper * DIV_127;
		double ratio2 = 1.0 - ratio1;
		set_noteoff_recalc_release_rate(env); // release rateが必要なので
		env->stage = ENV0_SUSTAIN_STAGE; // set noteon stage		
		adjust_envelope0_stage(env);	
		for(i = ENV0_RELEASE1_STAGE; i < ENV0_RELEASE4_STAGE; i++){
			if(env->add_vol[i] < 0){
				count++;
				rate += env->add_vol[i];
			}
		}
		if(count)
			rate /= count;
		else
			rate = env->rate[ENV0_RELEASE4_STAGE];
		env->add_vol[ENV0_HOLD_STAGE] = rate * ratio2 + env->rate[ENV0_HOLD_STAGE] * env->follow[ENV0_HOLD_STAGE] * ratio1;
		env->add_vol[ENV0_DECAY_STAGE] = rate * ratio2 + env->rate[ENV0_DECAY_STAGE] * env->follow[ENV0_DECAY_STAGE] * ratio1;
		env->add_vol[ENV0_SUSTAIN_STAGE] = rate * ratio2 + env->rate[ENV0_SUSTAIN_STAGE] * ratio1;
#endif
	}else{
		set_noteoff_recalc_release_rate(env); // set noteoff , release stage
		adjust_envelope0_stage(env);
	}
}

static int compute_envelope0_stage(Envelope0 *env, int cnt){
	double add_vol = env->add_vol[env->stage] * (double)cnt;
#if 0 // precision limit ( 30bit(max_value 0x3FFFFFFF) - 52bit(precision) = -22bit(lowlimit)
	if(env->add_vol[env->stage] < 0){
		if(add_vol > -DIV_21BIT)
			add_vol = -DIV_21BIT;
	}else{
		if(add_vol < DIV_21BIT)
			add_vol = DIV_21BIT;
	}
#endif
	env->vol += add_vol;
	if ((env->add_vol[env->stage] < 0) ^ (env->vol > env->target_vol[env->stage])) {
		if(env->target_vol[env->stage]){ // target_vol>0 : next stage 
			int next_stage = env->stage + 1;
			env->vol = env->target_vol[env->stage];	
			env->volume = calc_envelope0_curve(env->vol, env->curve[env->stage]);
			// カーブが変わるとvolは同じでも出力volumeは違うので 次ステージのカーブでvolumeが一致するvolに変更
			if(env->curve[env->stage] != env->curve[next_stage]){
				env->vol = calc_rev_envelope0_curve(env->volume, env->curve[next_stage]);	
				// add_volはoffset差で計算されてるので init_volとvol差分をadd_volへ反映
				if(env->init_vol[next_stage] != env->vol && env->init_vol[next_stage] != env->target_vol[next_stage]){
					if(env->init_vol[next_stage] > env->target_vol[next_stage])
						env->add_vol[next_stage] *= (env->vol - env->target_vol[next_stage]) / (env->init_vol[next_stage] - env->target_vol[next_stage]);
					else
						env->add_vol[next_stage] *= (env->target_vol[next_stage] - env->vol) / (env->init_vol[next_stage] - env->target_vol[next_stage]);
				}
			}
			env->stage++; // stage <= ENV0_RELEASE3_STAGE
			return 0;
		}else{ // target_vol=0 : stage end
			env->stage = ENV0_END_STAGE;
			env->vol = 0; env->volume = 0;
			return 1;
		}
	}
	env->volume = calc_envelope0_curve(env->vol, env->curve[env->stage]);
	return 0;
}

int compute_envelope0(Envelope0 *env, int32 cnt){
	int delay_cnt = 0;

	if(env->stage == ENV0_END_STAGE){
		env->vol = 0; env->volume = 0;
		return delay_cnt;
	}else if(env->status == ENV0_NOTE_ON_WAIT){
		if(cnt <= env->delay){
			env->delay -= cnt;
			return cnt;
		}
		delay_cnt = env->delay;
		cnt -= env->delay;
		env->delay = 0;
		env->status = ENV0_NOTE_ON;
	}else if(env->status == ENV0_NOTE_OFF_WAIT || env->status == ENV0_NOTE_CUT_WAIT){
#if 1 // ノート時間を最低1バッファ分確保 かわりにoffdelay処理はバッファ単位になる
		if(env->offdelay < 0)
			set_noteoff_recalc_release_rate(env); // set noteoff / release stage
		else{
			env->offdelay -= cnt;
			if(cnt <= env->delay){
				env->delay -= cnt;
				return cnt;
			}
			delay_cnt = env->delay;
			cnt -= env->delay;
			env->delay = 0;
		}		
#else // offdelay処理はサンプル単位になる ノート時間<バッファのとき音抜けするかも
		if(cnt < env->offdelay){
			env->offdelay -= cnt;			
			if(cnt <= env->delay){
				env->delay -= cnt;
				return cnt;
			}
			delay_cnt = env->delay;
			cnt -= env->delay;
			env->delay = 0;
		}else{			
			if(env->offdelay <= env->delay){ 
				env->stage = ENV0_END_STAGE;
				env->vol = 0; env->volume = 0;
				return env->offdelay;
			}
			delay_cnt = env->delay;
			if(compute_envelope0_stage(env, env->offdelay - env->delay))
				return 0;
			env->delay = 0;
			cnt -= env->offdelay;
			set_noteoff_recalc_release_rate(env); // set noteoff / release stage
		}
#endif
	}else if(env->count[env->stage]){ // sustain limit timeout
		if(cnt < env->count[env->stage]){
			env->count[env->stage] -= cnt;
		}else{
			if(compute_envelope0_stage(env, env->count[env->stage]))	
				return 0;
			cnt -= env->count[env->stage];
			env->count[env->stage] = 0;
			if(env->stage == ENV0_SUSTAIN_STAGE)
				set_noteoff_recalc_release_rate(env); // set noteoff / release stage
		}
	}			
	compute_envelope0_stage(env, cnt); // cnt : recompute flag
	return delay_cnt;
}

// only delay count
int compute_envelope0_delay(Envelope0 *env, int32 cnt){
	int delay_cnt = 0;

	if(env->stage == ENV0_END_STAGE){
		env->vol = 0; env->volume = 0;
		return delay_cnt;
	}else if(env->status == ENV0_NOTE_ON_WAIT){
		if(cnt <= env->delay){
			env->delay -= cnt;
			return cnt;
		}else{
			delay_cnt = env->delay;
			cnt -= env->delay;
			env->delay = 0;
			env->status = ENV0_NOTE_ON;
		}
	}else if(env->status == ENV0_NOTE_OFF_WAIT){
		if(cnt < env->offdelay){
			env->offdelay -= cnt;
		}else{
			cnt -= env->offdelay;
			env->stage = ENV0_RELEASE1_STAGE; // note off
		}
	}		
	return delay_cnt;
}

int check_envelope0(Envelope0 *env){
	return env->stage;
}



////////// amp_env , mod_env //////////

static void compute_envelope1_stage(Envelope1 *env, Envelope1Stage *stage){
	double tmp, tmp_init, tmp_target;
	FLOAT_T *table;

	env->vol = stage->init_vol + (stage->target_vol - stage->init_vol) * (((FLOAT_T)env->count - stage->start) * stage->div_length);
#if 0 // no curve
	env->volume = env->vol;
#elif  1 // use table 
	switch(stage->curve){
	default:
	case LINEAR_CURVE:
		env->volume = env->vol;
		return;
	case SF2_CONCAVE:
		table = sf2_concave_table;
		break;
	case SF2_CONVEX:
		table = sf2_convex_table;
		break;
	case DEF_VOL_CURVE:
		table = def_vol_table;
		break;
	case GS_VOL_CURVE:
		table = gs_vol_table;
		break;
	case XG_VOL_CURVE:
		table = xg_vol_table;
		break;
	case SF2_VOL_CURVE:
		table = sb_vol_table;
		break;		
	case MODENV_VOL_CURVE:
		table = modenv_vol_table;
		break;
	}
#if 1 // table interporation
	{
		double fp1 = env->vol * 1023.0;
		double fp2 = floor(fp1);
		int32 index = fp2;
		double v1 = table[index];
		double v2 = table[index + 1];
		env->volume = (v1 + (v2 - v1) * (fp1 - fp2));
		return;
	}
#else // no interporation
	{
		env->volume = table[(int)(env->vol * 1023.0)];
		return;
	}
#endif
#else // calc
	double tmp;
	if(env->vol > 1.0)
		return 1.0;
	else(env->vol <= 0.0)
		return 0.0;
	else switch(env->curve){
	default:
	case LINEAR_CURVE:
		env->volume = env->vol;
		break;
	case SF2_CONCAVE:
		env->volume = ((env->vol + 1.0 - sqrt(1.0 - env->vol * env->vol)) * DIV_2);
		break;
	case SF2_CONVEX:
		tmp = 1.0 - env->vol;
		env->volume = ((env->vol + sqrt(1.0 - tmp * tmp)) * DIV_2);
		break;
	case DEF_VOL_CURVE:
		env->volume = pow(2.0, (env->vol - 1.0) * 6);
		break;
	case GS_VOL_CURVE:
		env->volume = pow(2.0, (env->vol - 1.0) * 8);
		break;
	case XG_VOL_CURVE:
		env->volume = pow(2.0, (env->vol - 1.0) * 8);
		break;
	case SF2_VOL_CURVE:
		env->volume = pow(10.0, (1.0 - env->vol) * 960.0 / (-200.0));
		break;
	case MODENV_VOL_CURVE:
		tmp = (1.0 - (-20.0 * DIV_96 * log((env->vol * env->vol) * DIV_LN10)));
		if (tmp < 0) {tmp = 0;}
		env->volume = log(tmp + 1.0) * DIV_LN2;
		break;
	}
#endif
}

// in volume : 0.0~1.0
// out vol : 0.0~1.0
static double calc_rev_envelope1_curve(double volume, int curve){
// rev table
	const double rev_offset = 1.0 / 1023.0;
	FLOAT_T *table;
	int i = 0;
	double v1, v2;
	
	if(volume >= 1.0)
		return 1.0;
	else if(volume <= 0.0)
		return 0.0;
	else switch(curve){
	default:
	case LINEAR_CURVE:
		return volume;
	case SF2_CONCAVE:
		table = sf2_concave_table;
		break;
	case SF2_CONVEX:
		table = sf2_convex_table;
		break;
	case DEF_VOL_CURVE:
		table = def_vol_table;
		break;
	case GS_VOL_CURVE:
		table = gs_vol_table;
		break;
	case XG_VOL_CURVE:
		table = xg_vol_table;
		break;
	case SF2_VOL_CURVE:
		table = sb_vol_table;
		break;		
	case MODENV_VOL_CURVE:
		table = modenv_vol_table;
		break;
	}
#if 1 // max 20cycle
	while(i < 1024 && volume >= table[i]){ i += 256; } // max 4cycle
	i -= 256;
	while(i < 1024 && volume >= table[i]){ i += 64; } // max 4cycle
	i -= 64;
	while(i < 1024 && volume >= table[i]){ i += 16; } // max 4cycle
	i -= 16;
	while(i < 1024 && volume >= table[i]){ i += 4; } // max 4cycle
	i -= 4;
	while(i < 1024 && volume >= table[i]){ i += 1; } // max 4cycle
#elif 0 // max 23cycle
	while(i < 1024 && volume >= table[i]){ i += 180; } // max 6cycle
	i -= 180;
	while(i < 1024 && volume >= table[i]){ i += 30; } // max 6cycle
	i -= 30;
	while(i < 1024 && volume >= table[i]){ i += 5; } // max 6cycle
	i -= 5;
	while(i < 1024 && volume >= table[i]){ i += 1; } // max 5cycle
#elif 0 // max 31cycle
	while(i < 1024 && volume >= table[i]){ i += 100; } // max 11cycle
	i -= 100;
	while(i < 1024 && volume >= table[i]){ i += 10; } // max 10cycle
	i -= 10;
	while(i < 1024 && volume >= table[i]){ i += 1; } // max 10cycle
#elif 0 // max 64cycle
	while(i < 1024 && volume >= table[i]){ i += 32; } // max 32cycle
	i -= 32;
	while(i < 1024 && volume >= table[i]){ i += 1; } // max 32cycle
#else // max 1024 cycle
	while(i < 1024 && volume >= table[i]){ i += 1; } // max 1024cycle
#endif
#if 1 // table interporation
	v2 = table[i--];		
	v1 = table[i];
//	volume = (v1 + (v2 - v1) * ((vol * 1023.0) - i));
//	vol = ((volume - v1) / (v2 - v1) + i) / (1023.0);
	if(v1 != v2)
		return ((volume - v1) / (v2 - v1) + i) * rev_offset;
	else
		return (double)i * rev_offset;
#else // no interporation
	return (double)i * rev_offset;
#endif
}

 // note on
void init_envelope1(Envelope1 *env, FLOAT_T delay_cnt, FLOAT_T attack_cnt, FLOAT_T hold_cnt, FLOAT_T decay_cnt,
	FLOAT_T sustain_vol,FLOAT_T sustain_cnt, FLOAT_T release_cnt, int up_curve, int curve){
FLOAT_T tmp1, tmp2;
	env->stage = ENV1_DELAY_STAGE;
	env->volume = 0.0;
	env->vol = 0.0;
	env->count = 0;
	env->stage_p[1].curve = up_curve;
	env->stage_p[1].length = delay_cnt;
	env->stage_p[1].div_length = env->stage_p[1].length ? (1.0 / env->stage_p[1].length) : MIN_ENV1_LENGTH;
	env->stage_p[1].start = 0;
	env->stage_p[1].end = env->stage_p[1].start + env->stage_p[1].length;
	env->stage_p[1].init_vol = 0.0;
	env->stage_p[1].target_vol = env->stage_p[1].init_vol;
	env->stage_p[2].curve = up_curve;
	env->stage_p[2].length = attack_cnt;
	env->stage_p[2].div_length = env->stage_p[2].length ? (1.0 / env->stage_p[2].length) : MIN_ENV1_LENGTH;
	env->stage_p[2].start = env->stage_p[1].end;
	env->stage_p[2].end = env->stage_p[2].start + env->stage_p[2].length;
	env->stage_p[2].init_vol = env->stage_p[1].target_vol;
	env->stage_p[2].target_vol = 1.0;
	env->stage_p[3].curve = curve;
	env->stage_p[3].length = hold_cnt;
	env->stage_p[3].div_length = env->stage_p[3].length ? (1.0 / env->stage_p[3].length) : MIN_ENV1_LENGTH;
	env->stage_p[3].start = env->stage_p[2].end;
	env->stage_p[3].end = env->stage_p[3].start + env->stage_p[3].length;
	env->stage_p[3].init_vol = env->stage_p[2].target_vol;
	env->stage_p[3].target_vol = env->stage_p[3].init_vol;
	env->stage_p[4].curve = curve;
	env->stage_p[4].length_b = env->stage_p[4].length = decay_cnt;
	env->stage_p[4].div_length = env->stage_p[4].length ? (1.0 / env->stage_p[4].length) : MIN_ENV1_LENGTH;
	env->stage_p[4].start = env->stage_p[3].end;
	env->stage_p[4].end = env->stage_p[4].start + env->stage_p[4].length;
	env->stage_p[4].init_vol = env->stage_p[3].target_vol;
	env->stage_p[4].target_vol = sustain_vol > 1.0 ? 1.0 : sustain_vol;
	env->stage_p[5].curve = curve;
	env->stage_p[5].length_b = env->stage_p[5].length = sustain_cnt;
	env->stage_p[5].div_length = env->stage_p[5].length ? (1.0 / env->stage_p[5].length) : MIN_ENV1_LENGTH;
	env->stage_p[5].start = env->stage_p[4].end;
	env->stage_p[5].end = env->stage_p[5].start + env->stage_p[5].length;
	env->stage_p[5].init_vol = env->stage_p[4].target_vol;
	env->stage_p[5].target_vol = env->stage_p[5].init_vol;
	env->stage_p[6].curve = LINEAR_CURVE;
	env->stage_p[6].length = delay_cnt;
	env->stage_p[6].div_length = env->stage_p[6].length ? (1.0 / env->stage_p[6].length) : MIN_ENV1_LENGTH;
	env->stage_p[6].start = MAX_ENV1_LENGTH;
	env->stage_p[6].end = env->stage_p[6].start + env->stage_p[6].length; 
	env->stage_p[6].init_vol = 1.0;
	env->stage_p[6].target_vol = env->stage_p[6].init_vol;
	env->stage_p[7].curve = curve;
	env->stage_p[7].length_b = env->stage_p[7].length = release_cnt;
	env->stage_p[7].div_length = env->stage_p[7].length ? (1.0 / env->stage_p[7].length) : MIN_ENV1_LENGTH;
	env->stage_p[7].start = env->stage_p[6].end;
	env->stage_p[7].end = env->stage_p[7].start + env->stage_p[7].length;
	env->stage_p[7].init_vol = env->stage_p[6].target_vol;
	env->stage_p[7].target_vol = 0.0;
}

 // cutnote killnote
void reset_envelope1_release(Envelope1 *env, double release_length){
	//if(env->vol <= 0.0){
	//	env->stage_p[6].end = env->stage_p[7].end = -MAX_ENV1_LENGTH; // to END_STAGE
	//	env->stage = END_STAGE;
	//	env->volume = env->vol = 0.0;
	//	return;
	//}
	if(env->stage == ENV1_END_STAGE)
		return;
	if(env->stage_p[env->stage].curve != env->stage_p[ENV1_RELEASE_STAGE].curve)
		env->vol = calc_rev_envelope1_curve(env->volume, env->stage_p[ENV1_RELEASE_STAGE].curve);		
	env->stage = ENV1_RELEASE_STAGE;
	env->stage_p[6].length = 0;
	env->stage_p[6].div_length = MAX_ENV1_LENGTH;		
	env->stage_p[6].start = env->stage_p[6].end = -MAX_ENV1_LENGTH; 
	env->stage_p[6].init_vol = env->stage_p[6].target_vol = env->vol;
	if(release_length >= 0 && release_length < env->stage_p[7].length){
		env->stage_p[7].length = (release_length < 0) ? env->stage_p[7].length : release_length;
		env->stage_p[7].div_length = env->stage_p[7].length ? (1.0 / env->stage_p[7].length) : MIN_ENV1_LENGTH;
	}
#if 1 // recompute opt
	env->stage_p[7].start = env->count - (1.0 - env->vol) * env->stage_p[7].length;
	env->stage_p[7].end = env->stage_p[7].start + env->stage_p[7].length;
#else // recompute
	env->stage_p[7].start = env->count - (env->stage_p[7].init_vol - env->stage_p[6].target_vol) / (env->stage_p[7].init_vol - env->stage_p[7].target_vol) * env->stage_p[7].length;
	env->stage_p[7].end = env->stage_p[7].start + env->stage_p[7].length;
#endif
	env->stage = ENV1_RELEASE_STAGE;
}

static void reset_envelope1_offdelay(Envelope1 *env, double offdelay_length){
	//if(env->vol <= 0.0){
	//	env->stage_p[6].end = env->stage_p[7].end = -MAX_ENV1_LENGTH; // to END_STAGE
	//	env->stage = END_STAGE;
	//	env->volume = env->vol = 0.0;
	//	return;
	//}	
	if(offdelay_length >= 0 && offdelay_length < env->stage_p[6].length){
		env->stage_p[6].length = (offdelay_length < 0) ? env->stage_p[6].length : offdelay_length;
		env->stage_p[6].div_length = env->stage_p[6].length ? (1.0 / env->stage_p[6].length) : MIN_ENV1_LENGTH;
	}
	env->stage = ENV1_OFFDELAY_STAGE;
	env->stage_p[6].start = env->count;
	env->stage_p[6].end = env->stage_p[6].start + env->stage_p[6].length; 
	env->stage_p[6].init_vol = env->stage_p[6].target_vol = env->vol;
	env->stage_p[7].start = env->stage_p[6].end;
	env->stage_p[7].end = env->stage_p[7].start + env->stage_p[7].length;
}

static void reset_envelope1_noteoff(Envelope1 *env){
	if(env->stage >= ENV1_OFFDELAY_STAGE)
		return;
	if(env->stage_p[6].length){
		reset_envelope1_offdelay(env, ENVELOPE_KEEP);
	}else{
		reset_envelope1_release(env, ENVELOPE_KEEP);
	}
}

void reset_envelope1_decay(Envelope1 *env){
	env->stage = ENV1_DECAY_STAGE;
	env->stage_p[4].start = env->count - (env->stage_p[4].init_vol - env->vol) / (env->stage_p[4].init_vol - env->stage_p[4].target_vol) * env->stage_p[4].length;
	env->stage_p[4].end = env->stage_p[4].start + env->stage_p[4].length;
	env->stage_p[5].start = env->stage_p[4].end;
	env->stage_p[5].end = env->stage_p[5].start + env->stage_p[5].length;
}

void reset_envelope1_sustain(Envelope1 *env){
	env->stage = ENV1_SUSTAIN_STAGE;
	env->stage_p[5].start = env->count - (env->stage_p[5].init_vol - env->vol) / (env->stage_p[5].init_vol - env->stage_p[5].target_vol) * env->stage_p[5].length;
	env->stage_p[5].end = env->stage_p[5].start + env->stage_p[5].length;
}

void reset_envelope1_half_damper(Envelope1 *env, int8 damper){
	if(damper == 127){
		env->stage_p[4].length = env->stage_p[4].length_b;
		env->stage_p[5].length = env->stage_p[5].length_b;
	}else{
		env->stage_p[4].length = env->stage_p[7].length_b + (env->stage_p[4].length_b - env->stage_p[7].length_b) * damper * DIV_127;
		env->stage_p[5].length = env->stage_p[7].length_b + (playmode_rate_ms * DAMPER_SUSTAIN_TIME - env->stage_p[7].length_b) * damper * DIV_127;
	}
	if(env->vol > env->stage_p[4].target_vol)
		reset_envelope1_decay(env);
	else
		reset_envelope1_sustain(env);
}

int compute_envelope1(Envelope1 *env, int32 cnt){
	int delay_cnt = 0;

	if(env->stage == ENV1_END_STAGE)
		return 0;
	if(env->stage == ENV1_DELAY_STAGE){
		if((env->count + cnt) <= env->stage_p[1].end)
			delay_cnt = cnt;
		else{
			delay_cnt = env->stage_p[1].end - env->count;
			env->stage++; // next stage
		}
	}
	env->count += cnt;
	if(env->stage >= ENV1_OFFDELAY_STAGE){ // noteoff
		if(env->stage_p[6].length && env->count <= env->stage_p[6].end){
			if(env->stage_p[1].length && env->count <= env->stage_p[1].end){
				compute_envelope1_stage(env, &(env->stage_p[1]));
			}else if(env->stage_p[2].length && env->count <= env->stage_p[2].end){
				compute_envelope1_stage(env, &(env->stage_p[2]));
			}else if(env->stage_p[3].length && env->count <= env->stage_p[3].end){
				compute_envelope1_stage(env, &(env->stage_p[3]));
			}else if(env->stage_p[4].length && env->count <= env->stage_p[4].end){
				compute_envelope1_stage(env, &(env->stage_p[4]));
			}else if(env->stage_p[4].target_vol <= 0.0){
				env->stage = ENV1_END_STAGE;
				env->volume = env->vol = 0.0;
			}else if(env->stage_p[5].length && env->count <= env->stage_p[5].end){
				compute_envelope1_stage(env, &(env->stage_p[5]));
			}else{
				reset_envelope1_release(env, ENVELOPE_KEEP); // min_sustain_time end
			}
		}else if(env->stage_p[7].length && env->count <= env->stage_p[7].end){
			if(env->stage == ENV1_OFFDELAY_STAGE)
				reset_envelope1_release(env, ENVELOPE_KEEP); // exist OFFDELAY stage
			compute_envelope1_stage(env, &(env->stage_p[7]));
		}else{
			env->stage = ENV1_END_STAGE;
			env->volume = env->vol = 0.0;
		}
	}else{  // noteon
		if(env->stage_p[1].length && env->count <= env->stage_p[1].end){
			env->stage = ENV1_DELAY_STAGE;
			compute_envelope1_stage(env, &(env->stage_p[1]));
		}else if(env->stage_p[2].length && env->count <= env->stage_p[2].end){
			env->stage = ENV1_ATTACK_STAGE;
			compute_envelope1_stage(env, &(env->stage_p[2]));
		}else if(env->stage_p[3].length && env->count <= env->stage_p[3].end){
			env->stage = ENV1_HOLD_STAGE;
			compute_envelope1_stage(env, &(env->stage_p[3]));
		}else if(env->stage_p[4].length && env->count <= env->stage_p[4].end){
			env->stage = ENV1_DECAY_STAGE;
			compute_envelope1_stage(env, &(env->stage_p[4]));
		}else if(env->stage_p[4].target_vol <= 0.0){
			env->stage = ENV1_END_STAGE;
			env->volume = env->vol = 0.0;
		}else if(env->stage_p[5].length && env->count <= env->stage_p[5].end){
			env->stage = ENV1_SUSTAIN_STAGE;
			compute_envelope1_stage(env, &(env->stage_p[5]));
		}else{
			reset_envelope1_release(env, ENVELOPE_KEEP); // min_sustain_time end
		}
	}
	return delay_cnt;
}

int check_envelope1(Envelope1 *env){
	if(env->stage > ENV1_ATTACK_STAGE && env->vol <= 0.0)
		env->stage = ENV1_END_STAGE;
	return env->stage;
}



////////// vol_env , mix_env ////////////

void init_envelope2(Envelope2 *env, FLOAT_T vol1, FLOAT_T vol2, FLOAT_T time_cnt){
	env->stage = ENV1_END_STAGE;
	env->count = 0;
	env->vol[0] = env->target_vol[0] = env->init_vol[0] = vol1;
	env->vol[1] = env->target_vol[1] = env->init_vol[1] = vol2;
	env->length = time_cnt;
	env->div_length = time_cnt ? (1.0 / env->length) : MIN_ENV1_LENGTH;
}

void reset_envelope2(Envelope2 *env, FLOAT_T target_vol1, FLOAT_T target_vol2, FLOAT_T time_cnt){
	if(env->target_vol[0] == target_vol1 && env->target_vol[1] == target_vol2 && env->length == time_cnt)
		return;
	env->stage = ENV1_DELAY_STAGE; // env_on
	env->count = 0;
#if (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE) && defined(FLOAT_T_DOUBLE)
	_mm_storeu_pd(env->init_vol, _mm_loadu_pd(env->vol));
	env->target_vol[0] = target_vol1;
	env->target_vol[1] = target_vol2;
	if(time_cnt < 0 || env->length == time_cnt){		
		_mm_storeu_pd(env->mlt_vol, _mm_mul_pd(
			_mm_sub_pd(_mm_loadu_pd(env->target_vol), _mm_loadu_pd(env->init_vol)), MM_LOAD1_PD(&env->div_length)));
		return;
	}
	env->length = time_cnt;
	env->div_length = time_cnt ? (1.0 / env->length) : MIN_ENV1_LENGTH;		
	_mm_storeu_pd(env->mlt_vol, _mm_mul_pd(
		_mm_sub_pd(_mm_loadu_pd(env->target_vol), _mm_loadu_pd(env->init_vol)), MM_LOAD1_PD(&env->div_length)));
#else
	env->init_vol[0] = env->vol[0];
	env->init_vol[1] = env->vol[1];
	env->target_vol[0] = target_vol1;
	env->target_vol[1] = target_vol2;
	if(time_cnt < 0 || env->length == time_cnt){
		env->mlt_vol[0] = (env->target_vol[0] - env->init_vol[0]) * env->div_length;
		env->mlt_vol[1] = (env->target_vol[1] - env->init_vol[1]) * env->div_length;
		return;
	}
	env->length = time_cnt;
	env->div_length = time_cnt ? (1.0 / env->length) : MIN_ENV1_LENGTH;
	env->mlt_vol[0] = (env->target_vol[0] - env->init_vol[0]) * env->div_length;
	env->mlt_vol[1] = (env->target_vol[1] - env->init_vol[1]) * env->div_length;
#endif
}

void compute_envelope2(Envelope2 *env, int32 cnt){
	if(env->stage == ENV1_END_STAGE) // END_STAGE
		return;
	env->count += cnt;

#if (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE) && defined(FLOAT_T_DOUBLE)
	if(env->count < env->length){
		_mm_storeu_pd(env->vol, 
			MM_FMA_PD(_mm_loadu_pd(env->mlt_vol), _mm_set1_pd((double)env->count), _mm_loadu_pd(env->init_vol)));
	}else{
		_mm_storeu_pd(env->vol, _mm_loadu_pd(env->target_vol));
		env->stage = ENV1_END_STAGE; // env_off
	}
#else
	if(env->count < env->length){
		env->vol[0] = env->init_vol[0] + env->mlt_vol[0] * env->count;
		env->vol[1] = env->init_vol[1] + env->mlt_vol[1] * env->count;
	}else{
		env->vol[0] = env->target_vol[0];
		env->vol[1] = env->target_vol[1];
		env->stage = ENV1_END_STAGE; // env_off
	}
#endif
}

int check_envelope2(Envelope2 *env){
	 return env->stage;
 }


////////// other_env ////////////

void init_envelope3(Envelope3 *env, FLOAT_T vol, FLOAT_T time_cnt){
	env->stage = ENV1_END_STAGE;
	env->count = 0;
	env->vol = env->target_vol = env->init_vol = vol;
	env->length = time_cnt;
	env->div_length = time_cnt ? (1.0 / env->length) : MIN_ENV1_LENGTH;
}

void reset_envelope3(Envelope3 *env, FLOAT_T target_vol, FLOAT_T time_cnt){
	if(env->target_vol == target_vol && env->length == time_cnt)
		return;
	env->stage = ENV1_DELAY_STAGE; // env_on
	env->count = 0;
	env->init_vol = env->vol;
	env->target_vol = target_vol;
	if(time_cnt < 0 || env->length == time_cnt){
		env->mlt_vol = (env->target_vol - env->init_vol) * env->div_length;
		return;
	}
	env->length = time_cnt;
	env->div_length = time_cnt ? (1.0 / env->length) : MIN_ENV1_LENGTH;
	env->mlt_vol = (env->target_vol - env->init_vol) * env->div_length;
}

void compute_envelope3(Envelope3 *env, int32 count){
	if(env->stage == ENV1_END_STAGE) // END_STAGE
		return;
	env->count += count;
	if(env->count < env->length){
		env->vol = env->init_vol + env->mlt_vol * env->count;
	}else{
		env->vol = env->target_vol;
		env->stage = ENV1_END_STAGE; // env_off
	}
}

int check_envelope3(Envelope3 *env){
	 return env->stage;
}


////////// pitch_env ////////////


static void reset_envelope4(Envelope4 *env){
	if(env->stage == ENV4_END_STAGE) // END_STAGE
		return;
	env->count = 0;
	env->init_vol = env->vol;
	env->target_vol = env->stage_vol[env->stage];	
	env->length = env->stage_len[env->stage];	
	env->mlt_vol = (env->target_vol - env->init_vol) / env->length;
}

void init_envelope4(Envelope4 *env, 
	FLOAT_T delay_cnt, FLOAT_T offdelay_cnt,
	FLOAT_T ini_lv, 
	FLOAT_T atk_lv, FLOAT_T atk_cnt, 
	FLOAT_T dcy_lv, FLOAT_T dcy_cnt, 
	FLOAT_T sus_lv, FLOAT_T sus_cnt, 
	FLOAT_T rls_lv, FLOAT_T rls_cnt )
{
	memset(env, 0, sizeof(Envelope4));
	env->status = 0;
	env->delay = delay_cnt;
	env->offdelay = offdelay_cnt;
	env->vol = ini_lv;	
	env->stage_vol[ENV4_ATTACK_STAGE] = atk_lv;
	env->stage_len[ENV4_ATTACK_STAGE] = atk_cnt < 1.0 ? 1.0 : atk_cnt;
	env->stage_vol[ENV4_DECAY_STAGE] = dcy_lv;
	env->stage_len[ENV4_DECAY_STAGE] = dcy_cnt < 1.0 ? 1.0 : dcy_cnt;
	env->stage_vol[ENV4_SUSTAIN_STAGE] = sus_lv;
	env->stage_len[ENV4_SUSTAIN_STAGE] = sus_cnt < 1.0 ? 1.0 : sus_cnt;
	env->stage_vol[ENV4_RELEASE_STAGE] = rls_lv;
	env->stage_len[ENV4_RELEASE_STAGE] = rls_cnt < 1.0 ? 1.0 : rls_cnt;
	env->stage = ENV4_ATTACK_STAGE;
	reset_envelope4(env);
}

void reset_envelope4_noteoff(Envelope4 *env){
	if(env->stage >= ENV4_END_STAGE)
		return;
	if(env->status > 1)
		return;	
	env->status = 1; // notoff
}

void compute_envelope4(Envelope4 *env, int32 count){
	if(env->stage == ENV4_END_STAGE) // END_STAGE
		return;
	if(env->delay >= 0){
		env->delay -= count;
		return;
	}else if(env->status == 1){
		if(env->offdelay >= 0)
			env->offdelay -= count;
		else{
			FLOAT_T div_length;
			env->status = 2;
			env->stage = ENV4_RELEASE_STAGE;
			reset_envelope4(env);
			return;
		}
	}		
	env->count += count;
	if(env->count < env->length){
		env->vol = env->init_vol + env->mlt_vol * env->count;
	}else if(env->stage >= ENV4_RELEASE_STAGE){
		env->stage = ENV4_END_STAGE;
		env->vol = env->target_vol;
	}else if(env->stage == ENV4_SUSTAIN_STAGE){	
		env->vol = env->target_vol;
	}else{ // < ENV4_SUSTAIN_STAGE
		env->vol = env->target_vol;
		env->stage++;
		reset_envelope4(env);
	}
}

int check_envelope4(Envelope4 *env){
	 return env->stage;
 }



