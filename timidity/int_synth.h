/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

    In case you haven't heard, this program is free software;
    you can redistribute it and/or modify it under the terms of the
    GNU General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#ifndef ___INT_SYNTH_H_
#define ___INT_SYNTH_H_


#include "filter.h"


// resample data type
#define IS_RS_DATA_T_DOUBLE

#ifdef IS_RS_DATA_T_DOUBLE
typedef double IS_RS_DATA_T;
#else // IS_RS_DATA_T_FLOAT
typedef float IS_RS_DATA_T;
#define IS_RS_DATA_T_FLOAT
#endif

#define IS_INI_TYPE_ALL       (-1)
#define IS_INI_TYPE_SCC       (0)
#define IS_INI_TYPE_MMS       (1)
#define IS_INI_PRESET_ALL     (-1)
#define IS_INI_PRESET_NONE    (0)
#define IS_INI_PRESET_INIT    (1)

//#define IS_INI_LOAD_ALL 1 // 全部ロード , load_1_inst slowest , worst memory
//#define IS_INI_LOAD_TYPE 1 // type単位ロード
#define IS_INI_LOAD_BLOCK (32) // block単位ロード 1block=32/64/128preset
//#define IS_INI_LOAD_PRESET 1 // preset単位ロード , load_all_inst very slow , load_1_inst fast , best memory


// SCC
#define SCC_DATA_MAX 128
#define SCC_DATA_LENGTH 32
// LA
#define MT32_DATA_MAX 128
#define CM32L_DATA_MAX 256
// SCC
#define SCC_SETTING_MAX 1000 // <=1000
#define SCC_PARAM_MAX 1
#define SCC_OSC_MAX 6
#define SCC_AMP_MAX 4
#define SCC_PITCH_MAX 4
#define SCC_ENV_PARAM 14
#define SCC_LFO_PARAM 4
// MMS (OP : operator/partial
#define MMS_SETTING_MAX 1000 // <=1000
#define MMS_OP_MAX 16
#define MMS_OP_PARAM_MAX 4
#define MMS_OP_RANGE_MAX 4
#define MMS_OP_CON_MAX 4
#define MMS_OP_OSC_MAX 4
#define MMS_OP_WAVE_MAX 8
#define MMS_OP_SUB_MAX 4
#define MMS_OP_AMP_MAX 4
#define MMS_OP_PITCH_MAX 4
#define MMS_OP_WIDTH_MAX 4
#define MMS_OP_ENV_PARAM 14
#define MMS_OP_LFO_PARAM 4
#define MMS_OP_FLT_PARAM 4
#define MMS_OP_CUT_PARAM 4

// mt32emu
typedef struct _LA_Ctrl_ROM_PCM_Struct {
	uint8 pos;
	uint8 len;
	uint8 pitchLSB;
	uint8 pitchMSB;
} LA_Ctrl_ROM_PCM_Struct;

typedef struct _ControlROMMap {
	uint16 idPos;
	uint16 idLen;
	const uint8 *idBytes;
	uint16 pcmTable; // 4 * pcmCount bytes
	uint16 pcmCount;
	uint16 timbreAMap; // 128 bytes
	uint16 timbreAOffset;
	uint8 timbreACompressed;
	uint16 timbreBMap; // 128 bytes
	uint16 timbreBOffset;
	uint8 timbreBCompressed;
	uint16 timbreRMap; // 2 * timbreRCount bytes
	uint16 timbreRCount;
	uint16 rhythmSettings; // 4 * rhythmSettingsCount bytes
	uint16 rhythmSettingsCount;
	uint16 reserveSettings; // 9 bytes
	uint16 panSettings; // 8 bytes
	uint16 programSettings; // 8 bytes
	uint16 rhythmMaxTable; // 4 bytes
	uint16 patchMaxTable; // 16 bytes
	uint16 systemMaxTable; // 23 bytes
	uint16 timbreMaxTable; // 72 bytes
} LA_Ctrl_ROM_Map;

//#define WAVE_TABLE // sine other wave_table

enum { // see compute_osc[]
	OSC_SINE = 0,             // 0:SINE
	OSC_TRIANGULAR,           // 1:TRIANGULAR
	OSC_SAW1,                 // 2:SAW1
	OSC_SAW2,                 // 3:SAW2
	OSC_SQUARE,               // 4:SQUARE
	OSC_NOISE,                // 5:NOISE
	OSC_NOISE_LOWBIT,         // 6:NOISE LOWBIT
	OSC_SINE_TABLE,          // SINE (10bit lookup table, no interpolation)
	OSC_SINE_TABLE_LINEAR,   // SINE (10bit lookup table, linear interpolation)
	OSC_LIST_MAX, // last
};

enum {
	OSC_SCC,           // 6:SCC no interpolation
	OSC_SCC_INT_LINEAR,  // 7:SCC linear interpolation
	OSC_SCC_INT_SINE,  // 8:SCC sine interpolation
};

#define LFO_OSC_MAX (OSC_SQUARE)  // normal wave

enum {
	OP_OSC = 0,
	OP_FM,
	OP_AM,
	OP_PM,
	OP_AMPM,
	OP_RM,
	OP_SYNC,
	OP_CLIP, // test
	OP_LOWBIT, // test
	OP_LIST_MAX, // last
};

enum {
	MMS_OSC_WAVE = 0,
	MMS_OSC_SCC,
	MMS_OSC_MT32,
	MMS_OSC_CM32L,
	MMS_OSC_LIST_MAX, // last
};

typedef struct _Preset_SCC {    
	char *inst_name;
    int16 param[SCC_PARAM_MAX];
    int32 osc[SCC_OSC_MAX];
    int16 amp[SCC_AMP_MAX];
    int16 pitch[SCC_PITCH_MAX];
    int32 ampenv[SCC_ENV_PARAM];
    int32 pitenv[SCC_ENV_PARAM];
    int16 lfo1[SCC_LFO_PARAM];
    int16 lfo2[SCC_LFO_PARAM];
} Preset_SCC;

typedef struct _Preset_MMS {
	char *inst_name;
	char op_max;
    int16 op_param[MMS_OP_MAX][MMS_OP_PARAM_MAX];
    int8 op_connect[MMS_OP_MAX][MMS_OP_CON_MAX];
    int8 op_range[MMS_OP_MAX][MMS_OP_RANGE_MAX];
    int32 op_osc[MMS_OP_MAX][MMS_OP_OSC_MAX];
    int16 op_wave[MMS_OP_MAX][MMS_OP_WAVE_MAX];
    int16 op_sub[MMS_OP_MAX][MMS_OP_SUB_MAX];
	int16 op_amp[MMS_OP_MAX][MMS_OP_AMP_MAX];
	int16 op_pitch[MMS_OP_MAX][MMS_OP_PITCH_MAX];
	int16 op_width[MMS_OP_MAX][MMS_OP_WIDTH_MAX];
    int32 op_ampenv[MMS_OP_MAX][MMS_OP_ENV_PARAM];
    int32 op_modenv[MMS_OP_MAX][MMS_OP_ENV_PARAM];
    int32 op_widenv[MMS_OP_MAX][MMS_OP_ENV_PARAM];
    int32 op_pitenv[MMS_OP_MAX][MMS_OP_ENV_PARAM];
    int16 op_ampenv_keyf[MMS_OP_MAX][MMS_OP_ENV_PARAM];
    int16 op_modenv_keyf[MMS_OP_MAX][MMS_OP_ENV_PARAM];
    int16 op_widenv_keyf[MMS_OP_MAX][MMS_OP_ENV_PARAM];
    int16 op_pitenv_keyf[MMS_OP_MAX][MMS_OP_ENV_PARAM];
    int16 op_ampenv_velf[MMS_OP_MAX][MMS_OP_ENV_PARAM];
    int16 op_modenv_velf[MMS_OP_MAX][MMS_OP_ENV_PARAM];
    int16 op_widenv_velf[MMS_OP_MAX][MMS_OP_ENV_PARAM];
    int16 op_pitenv_velf[MMS_OP_MAX][MMS_OP_ENV_PARAM];
    int16 op_lfo1[MMS_OP_MAX][MMS_OP_LFO_PARAM];
    int16 op_lfo2[MMS_OP_MAX][MMS_OP_LFO_PARAM];
    int16 op_lfo3[MMS_OP_MAX][MMS_OP_LFO_PARAM];
    int16 op_lfo4[MMS_OP_MAX][MMS_OP_LFO_PARAM];
	int16 op_filter[MMS_OP_MAX][MMS_OP_FLT_PARAM];
	int16 op_cutoff[MMS_OP_MAX][MMS_OP_CUT_PARAM];	
} Preset_MMS;

typedef struct _Preset_IS {
	char *ini_file;
	int8 scc_data_load;
	uint32 scc_load, mms_load;
	char *scc_data_name[SCC_DATA_MAX];
	int16 scc_data_int[SCC_DATA_MAX][SCC_DATA_LENGTH];
	FLOAT_T scc_data[SCC_DATA_MAX][SCC_DATA_LENGTH + 1];
	Preset_SCC *scc_setting[SCC_SETTING_MAX];
	Preset_MMS *mms_setting[MMS_SETTING_MAX];
	struct _Preset_IS *next;
} Preset_IS;



typedef FLOAT_T (*compute_osc_t)(FLOAT_T in, int32 var);
typedef FLOAT_T (*compute_scc_t)(FLOAT_T in, FLOAT_T *data);

typedef struct {
	int32 loop, ofs_start, ofs_end, loop_start, loop_end, loop_length;
	FLOAT_T sample_rate, root_freq, div_root_freq;
} Info_PCM;

typedef struct {
	int8 mode;
	int32 delay;
	int32 cycle;
	FLOAT_T freq, rate, out;
	Envelope3 env;
	compute_osc_t osc_ptr;
} Info_LFO;

typedef struct {
	int32 rs_count;
	int32 offset;
	IS_RS_DATA_T data[2];
	IS_RS_DATA_T db[17];
} Info_Resample;

typedef struct {
	int8 init, mode, scc_flag;
	int32 thru_count;
	int32 cycle;
	FLOAT_T output_level;
	FLOAT_T freq_mlt, freq, rate;
	FLOAT_T env_pitch, pitch, lfo_amp, lfo_pitch, amp_vol;
	FLOAT_T *data_ptr; // scc_data
	Envelope0 amp_env, pit_env;
	Info_LFO lfo1, lfo2;
	Info_Resample rs;
} InfoIS_SCC;

typedef struct _Info_OP{
	int8 mode, mod_type, osc_type, wave_type, update_width;
	int32 cycle;
	FLOAT_T op_level, wave_width; 
	FLOAT_T wave_width1, rate_width1, rate_width2, req_wave_width1, req_rate_width1, req_rate_width2;
	FLOAT_T freq_mlt, freq, mod_level, cent, rate, pcm_rate, efx_var1, efx_var2, amp_vol;
	FLOAT_T env_pitch, lfo_amp, lfo_pitch, env_width, lfo_width;
	int32 ofs_start, ofs_end, loop_length;
	FLOAT_T freq_coef, pcm_cycle;
	FLOAT_T *data_ptr; // pcm_data scc_data
	int8 op_flag, amp_env_flg, mod_env_flg, wid_env_flg, pit_env_flg, lfo1_flg, lfo2_flg, lfo3_flg, lfo4_flg;
	FLOAT_T *ptr_connect[MMS_OP_CON_MAX], in, sync;
	compute_osc_t osc_ptr;
	compute_scc_t scc_ptr;
	Envelope0 amp_env, mod_env, wid_env, pit_env;
	Info_LFO lfo1, lfo2, lfo3, lfo4;
	FLOAT_T flt_freq, flt_reso, env_cutoff, lfo_cutoff;
	FilterCoefficients fc;
} Info_OP;

typedef void (*compute_op_t)(Info_OP *info);

typedef struct _InfoIS_MMS{

	int8 init, op_max;
	int32 thru_count;
	Info_OP op[MMS_OP_MAX];
	compute_op_t op_ptr[MMS_OP_MAX];
	FLOAT_T (*op_num_ptr)(struct _InfoIS_MMS *info);
	FLOAT_T out, dummy;
	Info_Resample rs;
} InfoIS_MMS;

typedef FLOAT_T (*compute_op_num_t)(InfoIS_MMS *info);


#ifdef INT_SYNTH

extern compute_osc_t compute_osc[];

/**************** interface function is_editor ****************/
extern char is_ini_filepath[];
extern void is_editor_store_conf(void);
extern int is_editor_get_param(int num);
extern void is_editor_set_param(int num, int val);
extern int scc_data_editor_override;
extern int scc_editor_override;
extern int mms_editor_override; 
extern const char *scc_data_editor_load_name(int num);
extern void scc_data_editor_store_name(int num, const char *name);
extern void scc_data_editor_clear_param(void);
extern void scc_data_editor_set_default_param(int set_num);
extern int scc_data_editor_get_param(int num);
extern void scc_data_editor_set_param(int num, int val);
extern void scc_data_editor_load_preset(int num);
extern void scc_data_editor_store_preset(int num);
extern const char *scc_editor_load_name(int num);
extern void scc_editor_store_name(int num, const char *name);
extern const char *scc_editor_load_wave_name(int fnc);
extern void scc_editor_set_default_param(int set_num);
extern int scc_editor_get_param(int type, int num);
extern void scc_editor_set_param(int type, int num, int val);
extern void scc_editor_delete_preset(int num);
extern void scc_editor_load_preset(int num);
extern void scc_editor_store_preset(int num);
extern const char *mms_editor_load_name(int num);
extern void mms_editor_store_name(int num, const char *name);
extern const char *mms_editor_load_wave_name(int op, int fnc);
extern const char *mms_editor_load_filter_name(int op);
extern void mms_editor_set_default_param(int set_num, int op_num);
extern void mms_editor_set_magic_param(void);
extern int mms_editor_get_param(int type, int op, int num);
extern void mms_editor_set_param(int type, int op, int num, int val);
extern void mms_editor_delete_preset(int num);
extern void mms_editor_load_preset(int num);
extern void mms_editor_store_preset(int num);
extern const char *is_editor_get_ini_path(void);
extern void is_editor_set_ini_path(const char *name);
extern void is_editor_load_ini(void);
extern void init_is_editor_param(void);
extern void uninit_is_editor_param(void);

/**************** interface function SCC MMS ****************/
// SCC
extern void init_int_synth_scc(int v);
extern void noteoff_int_synth_scc(int v);
extern void damper_int_synth_scc(int v, int8 damper);
extern void compute_voice_scc(int v, DATA_T *ptr, int32 count);
// MMS
extern void init_int_synth_mms(int v);
extern void noteoff_int_synth_mms(int v);
extern void damper_int_synth_mms(int v, int8 damper);
extern void compute_voice_mms(int v, DATA_T *ptr, int32 count);

/**************** interface function internal synth ****************/
extern void free_int_synth_preset(void);
extern void free_int_synth(void);
extern void init_int_synth(void);

#endif /* INT_SYNTH */

extern int32 opt_int_synth_sine;
extern int32 opt_int_synth_rate;
extern int32 opt_int_synth_update;



#endif /* ___INT_SYNTH_H_ */