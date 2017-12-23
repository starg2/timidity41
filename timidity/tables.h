/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2004 Masanao Izumo <iz@onicos.co.jp>
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

    tables.h
*/

#ifndef ___TABLES_H_
#define ___TABLES_H_

///r
// table
#ifdef LOOKUP_SINE
extern FLOAT_T lookup_sine(int x);
#else
#include <math.h>
#define lookup_sine(x) (sin((2*M_PI/1024.0) * (x)))
#endif
extern FLOAT_T lookup_triangular(int x);
extern FLOAT_T lookup_log(int x);
extern FLOAT_T lookup_saw1(int x);
extern FLOAT_T lookup_saw2(int x);
extern FLOAT_T lookup_square(int x);

// table2
// 1cycle = 1024 = fraction 10bit , precision 30bit
#define TABLE2_CYCLE_LENGTH (M_10BIT) // 2^n
extern FLOAT_T lookup2_sine(FLOAT_T in); // precision 30bit
extern FLOAT_T lookup2_sine_p(FLOAT_T in);
extern FLOAT_T lookup2_sine_linear(FLOAT_T in); // precision 30bit

// filter table
extern FLOAT_T filter_cb_p_table[];
extern FLOAT_T filter_cb_m_table[];
// noise
extern FLOAT_T lookup_noise_lowbit(FLOAT_T in, int32 cycle);
// nrpn param
enum {
	NRPN_PARAM_GM_DELAY = 0,
	NRPN_PARAM_GM_RATE,
	NRPN_PARAM_GM_CUTOFF,
	NRPN_PARAM_GM_CUTOFF_HPF,
//	NRPN_PARAM_GM_RESO,
	NRPN_PARAM_GM_ATTACK,
	NRPN_PARAM_GM_DECAY,
	NRPN_PARAM_GM_RELEASE,
	NRPN_PARAM_GS_DELAY,
	NRPN_PARAM_GS_RATE,
	NRPN_PARAM_GS_CUTOFF,
	NRPN_PARAM_GS_CUTOFF_HPF,
//	NRPN_PARAM_GS_RESO,
	NRPN_PARAM_GS_ATTACK,
	NRPN_PARAM_GS_DECAY,
	NRPN_PARAM_GS_RELEASE,
	NRPN_PARAM_XG_DELAY,
	NRPN_PARAM_XG_RATE,
	NRPN_PARAM_XG_CUTOFF,
	NRPN_PARAM_XG_CUTOFF_HPF,
//	NRPN_PARAM_XG_RESO,
	NRPN_PARAM_XG_ATTACK,
	NRPN_PARAM_XG_DECAY,
	NRPN_PARAM_XG_RELEASE,
//	NRPN_PARAM_SD_DELAY,
//	NRPN_PARAM_SD_RATE,
//	NRPN_PARAM_SD_CUTOFF,
//	NRPN_PARAM_SD_RESO,
//	NRPN_PARAM_SD_ATTACK,
//	NRPN_PARAM_SD_DECAY,
//	NRPN_PARAM_SD_RELEASE
	NRPN_PARAM_LIST_MAX,
};
extern FLOAT_T calc_nrpn_param(FLOAT_T in, int add, int mode);

#define SINE_CYCLE_LENGTH 1024
extern int32 freq_table[];
extern int32 freq_table_zapped[];
extern int32 freq_table_tuning[][128];
extern int32 freq_table_pytha[][128];
extern int32 freq_table_meantone[][128];
extern int32 freq_table_pureint[][128];
extern int32 freq_table_user[][48][128];
extern FLOAT_T *vol_table;
extern FLOAT_T def_vol_table[];
extern FLOAT_T gs_vol_table[];
extern FLOAT_T *xg_vol_table; /* == gs_vol_table */
extern FLOAT_T *pan_table;
extern FLOAT_T bend_fine[];
extern FLOAT_T bend_coarse[];
extern FLOAT_T midi_time_table[], midi_time_table2[];
extern FLOAT_T portament_time_table_xg[];
#ifdef LOOKUP_HACK
extern uint8 *_l2u; /* 13-bit PCM to 8-bit u-law */
extern uint8 _l2u_[]; /* used in LOOKUP_HACK */
extern int16 _u2l[];
extern int32 *mixup;
#ifdef LOOKUP_INTERPOLATION
extern int8 *iplookup;
#endif
#endif
extern uint8 reverb_macro_presets[];
extern uint8 chorus_macro_presets[];
extern uint8 delay_macro_presets[];
extern float delay_time_center_table[];
extern float chorus_delay_time_table[];
///r
extern FLOAT_T attack_vol_table[];
extern FLOAT_T perceived_vol_table[];
extern FLOAT_T gm2_vol_table[];
extern FLOAT_T user_vol_table[];
extern float sc_eg_attack_table[];
extern float sc_eg_decay_table[];
extern float sc_eg_release_table[];
extern FLOAT_T sc_vel_table[];
extern FLOAT_T sc_vol_table[];
extern FLOAT_T sc_pan_table[], gm2_pan_table[];
extern FLOAT_T sc_drum_level_table[];
extern FLOAT_T sb_vol_table[];
extern FLOAT_T modenv_vol_table[], sf2_concave_table[], sf2_convex_table[];
extern float cb_to_amp_table[];
extern float reverb_time_table[];
extern float pan_delay_table[];
extern float chamberlin_filter_db_to_q_table[];

// GS
extern float pre_delay_time_table[];
extern int16 delay_time_1_table[];
extern int16 delay_time_2_table[];
extern float delay_time_3_table[];
extern float delay_time_4_table[];
extern float rate1_table[];
extern float rate2_table[];
extern int16 HF_damp_freq_table_gs[];
extern int16 cutoff_freq_table_gs[];
extern int16 eq_freq_table_gs[];
extern int16 lpf_table_gs[];
extern int16 manual_table[];
extern int16 azimuth_table[];
//extern float accel_table[];

// XG
extern float lfo_freq_table_xg[];
extern float mod_delay_offset_table_xg[];
extern float eq_freq_table_xg[];
extern float reverb_time_table_xg[];
extern float delay_time_table_xg[];
extern float room_size_table_xg[];
extern float delay_time_2_table_xg[];
extern float compressor_attack_time_table_xg[];
extern float compressor_release_time_table_xg[];
extern float compressor_ratio_table_xg[];
extern float reverb_width_height_table_xg[];
extern float wah_release_time_table_xg[];
extern float lofi_sampling_freq_table_xg[];
extern float tempo_table_xg[];
//extern float dyna_attack_time_table_xg[];
//extern float dyna_release_time_table_xg[];
extern float ring_mod_carrier_freq_course_xg[];
//extern float v_flanger_delay_offset_table_xg[];
extern uint8 multi_eq_block_table_xg[];

// SD
extern float note1_table_sd[10];
extern float note2_table_sd[22];


///r
extern int8 delay_out_type_gs[18][128];
extern int8 delay_out_type_xg[128][9];
extern int8 delay_out_type_sd[];

extern void init_freq_table(void);
extern void init_freq_table_tuning(void);
extern void init_freq_table_pytha(void);
extern void init_freq_table_meantone(void);
extern void init_freq_table_pureint(void);
extern void init_freq_table_user(void);
extern void init_bend_fine(void);
extern void init_bend_coarse(void);
extern void init_tables(void);
extern void init_gm2_pan_table(void);
extern void init_attack_vol_table(void);
extern void init_sb_vol_table(void);
extern void init_modenv_vol_table(void);
extern void init_def_vol_table(void);
extern void init_gs_vol_table(void);
extern void init_perceived_vol_table(void);
extern void init_gm2_vol_table(void);
extern void init_user_vol_table(FLOAT_T power);


extern void init_filter_table(void);

#endif /* ___TABLES_H_ */
