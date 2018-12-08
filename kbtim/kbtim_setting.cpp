#include <windows.h>

extern "C"{
///r
#include "config.h"

#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "effect.h"
#include "output.h"
#include "controls.h"
#include "recache.h"
#include "tables.h"
#include "resample.h"
#include "wrd.h"
#include "w32g.h"
#include "w32g_utl.h"
#include <sys/stat.h>
#include "strtab.h"
#include "url.h"
#include "sndfontini.h"
#include "myini.h"
#include "mix.h"
#include "thread.h"
#include "miditrace.h"

///r
extern int opt_default_mid;
extern char def_instr_name[];
extern int opt_control_ratio;
extern char *opt_aq_max_buff;
extern char *opt_aq_fill_buff;
extern int opt_evil_mode;
extern int opt_buffer_fragments;
extern int32 opt_output_rate;
#if defined(__W32__) && defined(SMFCONV)
extern int opt_rcpcv_dll;
#endif /* SMFCONV */
//extern volatile int data_block_bits;
//extern volatile int data_block_num;
//extern double voice_filter_reso;
//extern double voice_filter_gain;
//extern double scalewet;
//extern double freeverb_scaleroom;
//extern double freeverb_offsetroom;
//extern double fixedgain;
//extern double combfbk;
//extern double time_rt_diff;
//extern int numcombs;
//extern double ext_chorus_level;
//extern double ext_chorus_feedback;
//extern Sample OverrideSample;
//extern OVERRIDETIMIDITYDATA otd;
}

#include "kbtim_setting.h"

//*****************************************************************************/
// Setting
static SETTING_TIMIDITY g_st_default;

#define SetFlag(flag) (!!(flag))

static long SetValue(int32 value, int32 min, int32 max)
{
    int32 v = value;
    if (v < min) v = min;
    else if ( v > max) v = max;
    return v;
}

static int32 str2size(char *str)
{
    int len = strlen(str);
    if (str[len - 1] == 'k' || str[len - 1] == 'K')
        return (int32)(1024.0 * atof(str));
    if (str[len - 1] == 'm' || str[len - 1] == 'M')
        return (int32)(1024 * 1024 * atof(str));
    return atoi(str);
}

extern int opt_normal_chorus_plus; // E ?
extern int opt_compute_buffer_bits;
#define INI_MAXLEN 1024

static void kbtim_ApplySettingTiMidity(SETTING_TIMIDITY *st)
{
    int i;
    char buffer[INI_MAXLEN];

    amplification = SetValue(st->amplification, 0, MAX_AMPLIFICATION);
    output_amplification = SetValue(st->output_amplification, 0, MAX_AMPLIFICATION);
    antialiasing_allowed = SetFlag(st->antialiasing_allowed);
    opt_buffer_fragments = -1;
    if (st->compute_buffer_bits == -128)
        opt_compute_buffer_bits = -128;
    else
        opt_compute_buffer_bits = SetValue(st->compute_buffer_bits, -5, 10);
    default_drumchannels = st->default_drumchannels;
    default_drumchannel_mask = st->default_drumchannel_mask;
    opt_modulation_wheel = SetFlag(st->opt_modulation_wheel);
    opt_portamento = SetFlag(st->opt_portamento);
    opt_nrpn_vibrato = SetFlag(st->opt_nrpn_vibrato);
    opt_channel_pressure = SetFlag(st->opt_channel_pressure);
    opt_trace_text_meta_event = SetFlag(st->opt_trace_text_meta_event);
    opt_overlap_voice_allow = SetFlag(st->opt_overlap_voice_allow);
///r
    opt_overlap_voice_count = SetValue(st->opt_overlap_voice_count, 0, 512);
    opt_max_channel_voices = SetValue(st->opt_max_channel_voices, 4, 512);

    opt_default_mid = st->opt_default_mid;
    opt_system_mid = st->opt_system_mid;
    default_tonebank = st->default_tonebank;
    special_tonebank = st->special_tonebank;
    opt_reverb_control = st->opt_reverb_control;
    opt_chorus_control = st->opt_chorus_control;
    opt_surround_chorus = st->opt_surround_chorus;
///r
    opt_normal_chorus_plus = st->opt_normal_chorus_plus;

    noise_sharp_type = st->noise_sharp_type;
    opt_evil_mode = st->opt_evil_mode;
    opt_tva_attack = st->opt_tva_attack;
    opt_tva_decay = st->opt_tva_decay;
    opt_tva_release = st->opt_tva_release;
    opt_delay_control = st->opt_delay_control;
    opt_default_module = st->opt_default_module;
    opt_lpf_def = st->opt_lpf_def;
    opt_hpf_def = st->opt_hpf_def;
    opt_drum_effect = st->opt_drum_effect;
    opt_modulation_envelope = st->opt_modulation_envelope;
    opt_eq_control = st->opt_eq_control;
    opt_insertion_effect = st->opt_insertion_effect;
    adjust_panning_immediately = SetFlag(st->adjust_panning_immediately);
    fast_decay = SetFlag(st->opt_fast_decay);

    for (i = 0; i < MAX_CHANNELS; i++)
        default_program[i] = st->default_program[i];
    max_voices = voices = st->voices;
        auto_reduce_polyphony = st->auto_reduce_polyphony;
    quietchannels = st->quietchannels;
    temper_type_mute = st->temper_type_mute;

    safe_free(opt_aq_max_buff);
    opt_aq_max_buff = NULL;
    safe_free(opt_aq_fill_buff);
    opt_aq_fill_buff = NULL;
    //safe_free(opt_reduce_voice_threshold);
    //opt_reduce_voice_threshold = NULL;
    //safe_free(opt_reduce_quality_threshold);
    //opt_reduce_quality_threshold = NULL;
    //safe_free(opt_reduce_polyphony_threshold);
    //opt_reduce_polyphony_threshold = NULL;
    strcpy(buffer, st->opt_qsize);
    opt_aq_max_buff = buffer;
    opt_aq_fill_buff = strchr(opt_aq_max_buff, '/');
    *opt_aq_fill_buff++ = '\0';
    opt_aq_max_buff = safe_strdup(opt_aq_max_buff);
    opt_aq_fill_buff = safe_strdup(opt_aq_fill_buff);

    modify_release = SetValue(st->modify_release, 0, MAX_MREL);
    allocate_cache_size = st->allocate_cache_size;
    key_adjust = st->key_adjust;
    opt_force_keysig = st->opt_force_keysig;
    opt_pure_intonation = st->opt_pure_intonation;
    opt_init_keysig = st->opt_init_keysig;
    free_instruments_afterwards = st->free_instruments_afterwards;
    opt_realtime_playing = st->opt_realtime_playing;

    opt_control_ratio = st->control_ratio;
    control_ratio = SetValue(opt_control_ratio, 0, MAX_CONTROL_RATIO);
    //opt_control_ratio == 0 の場合の control_ratio の値の設定は
    //kbtim_play_midi_file で行う
    opt_drum_power = SetValue(st->opt_drum_power, 0, MAX_AMPLIFICATION);
    opt_amp_compensation = st->opt_amp_compensation;
    opt_user_volume_curve = st->opt_user_volume_curve;
///r
    set_current_resampler(st->opt_resample_type);
    set_resampler_parm(st->opt_resample_param);
    opt_resample_filter = st->opt_resample_filter;
    set_resampler_over_sampling(st->opt_resample_over_sampling);
    opt_pre_resamplation = st->opt_pre_resamplation;

    add_play_time = st->add_play_time;
    add_silent_time = st->add_silent_time;
    emu_delay_time = st->emu_delay_time;

    opt_limiter = st->opt_limiter;
    opt_mix_envelope = st->opt_mix_envelope;
    opt_modulation_update = st->opt_modulation_update;
    opt_cut_short_time = st->opt_cut_short_time;
    opt_use_midi_loop_repeat = SetFlag(st->opt_use_midi_loop_repeat);
    opt_midi_loop_repeat = SetValue(st->opt_midi_loop_repeat, 0, 99);

    compute_thread_num = st->compute_thread_num;

#if defined(__W32__) && defined(SMFCONV)
    opt_rcpcv_dll = st->opt_rcpcv_dll;
#endif /* SMFCONV */

    opt_load_all_instrument = st->opt_load_all_instrument;

    /* for INT_SYNTH */
    opt_int_synth_sine = st->opt_int_synth_sine;
    opt_int_synth_rate = st->opt_int_synth_rate;
    opt_int_synth_update = st->opt_int_synth_update;
}

///r
static PlayMode kbtim_play_mode = {
    DEFAULT_RATE,         //rate
    PE_16BIT | PE_SIGNED, //encoding
    PF_PCM_STREAM,        //flag
    -1,                   //fd: file descriptor for the audio device
    {0,0,0,0,0},          //extra_param[5]: System depended parameters
                            //e.g. buffer fragments, ...
    "kbtim_play_mode",    //id_name
    0,                    //id_character
    NULL,                       /* open_output */
    NULL,                       /* close_output */
    NULL,                       /* output_data */
    NULL,                       /* acntl */
    NULL                        /* detect */
};

///r
static void kbtim_SaveSettingTiMidity(SETTING_TIMIDITY *st)
{
    int i,j;

    ZeroMemory(st, sizeof(SETTING_TIMIDITY));

    // from timidity.c timidity_start_initialize()
    if (play_mode == NULL)
        play_mode = &kbtim_play_mode;
    if (!opt_aq_max_buff)
        opt_aq_max_buff = safe_strdup("5.0");
    if (!opt_aq_fill_buff)
        opt_aq_fill_buff = safe_strdup("100%");
    if (!output_text_code)
        output_text_code = safe_strdup(OUTPUT_TEXT_CODE);

    st->amplification = SetValue(amplification, 0, MAX_AMPLIFICATION);
    st->output_amplification = SetValue(output_amplification, 0, MAX_AMPLIFICATION);
    st->antialiasing_allowed = SetFlag(antialiasing_allowed);
    st->buffer_fragments = opt_buffer_fragments;
    st->compute_buffer_bits = opt_compute_buffer_bits;
    st->control_ratio = SetValue(opt_control_ratio, 0, MAX_CONTROL_RATIO);
    st->default_drumchannels = default_drumchannels;
    st->default_drumchannel_mask = default_drumchannel_mask;
    st->opt_modulation_wheel = SetFlag(opt_modulation_wheel);
    st->opt_portamento = SetFlag(opt_portamento);
    st->opt_nrpn_vibrato = SetFlag(opt_nrpn_vibrato);
    st->opt_channel_pressure = SetFlag(opt_channel_pressure);
    st->opt_trace_text_meta_event = SetFlag(opt_trace_text_meta_event);
    st->opt_overlap_voice_allow = SetFlag(opt_overlap_voice_allow);
    st->opt_overlap_voice_count = SetValue(opt_overlap_voice_count, 0, 512);
    st->opt_max_channel_voices = SetValue(opt_max_channel_voices, 4, 512);

    st->opt_default_mid = opt_default_mid;
    st->opt_system_mid = opt_system_mid;
    st->default_tonebank = default_tonebank;
    st->special_tonebank = special_tonebank;
    st->opt_reverb_control = opt_reverb_control;
    st->opt_chorus_control = opt_chorus_control;
    st->opt_surround_chorus = opt_surround_chorus;
    st->opt_normal_chorus_plus = opt_normal_chorus_plus;
    st->opt_tva_attack = opt_tva_attack;
    st->opt_tva_decay = opt_tva_decay;
    st->opt_tva_release = opt_tva_release;
    st->opt_delay_control = opt_delay_control;
    st->opt_default_module = opt_default_module;
    st->opt_lpf_def = opt_lpf_def;
    st->opt_hpf_def = opt_hpf_def;
    st->opt_drum_effect = opt_drum_effect;
    st->opt_modulation_envelope = opt_modulation_envelope;
    st->opt_eq_control = opt_eq_control;
    st->opt_insertion_effect = opt_insertion_effect;
    st->noise_sharp_type = noise_sharp_type;
    st->opt_evil_mode = SetFlag(opt_evil_mode);
    st->adjust_panning_immediately = SetFlag(adjust_panning_immediately);
    st->opt_fast_decay = SetFlag(fast_decay);
    for (i = 0; i < MAX_CHANNELS; i++) {
        if (def_instr_name[0])
            st->default_program[i] = SPECIAL_PROGRAM;
        else
            st->default_program[i] = default_program[i];
    }
    j = 0;
    st->opt_playmode[j++] = play_mode->id_character;
    st->opt_playmode[j++] = ((play_mode->encoding & PE_MONO) ? 'M' : 'S');
    st->opt_playmode[j++] = ((play_mode->encoding & PE_SIGNED) ? 's' : 'u');
    if (play_mode->encoding & PE_F64BIT) {st->opt_playmode[j++] = 'D';}
    else if (play_mode->encoding & PE_F32BIT) {st->opt_playmode[j++] = 'f';}
    else if (play_mode->encoding & PE_64BIT) {st->opt_playmode[j++] = '6';}
    else if (play_mode->encoding & PE_32BIT) {st->opt_playmode[j++] = '3';}
    else if (play_mode->encoding & PE_24BIT) {st->opt_playmode[j++] = '2';}
    else if (play_mode->encoding & PE_16BIT) {st->opt_playmode[j++] = '1';}
    else {st->opt_playmode[j++] = '8';}
    if (play_mode->encoding & PE_ULAW)
        st->opt_playmode[j++] = 'U';
    else if (play_mode->encoding & PE_ALAW)
        st->opt_playmode[j++] = 'A';
    else
        st->opt_playmode[j++] = 'l';
    if (play_mode->encoding & PE_BYTESWAP)
        st->opt_playmode[j++] = 'x';
    st->opt_playmode[j] = '\0';
    st->voices = voices;
    st->auto_reduce_polyphony = auto_reduce_polyphony;
    st->quietchannels = quietchannels;
    st->temper_type_mute = temper_type_mute;
    snprintf(st->opt_qsize,sizeof(st->opt_qsize),"%s/%s", opt_aq_max_buff,opt_aq_fill_buff);
    st->modify_release = SetValue(modify_release, 0, MAX_MREL);
    st->allocate_cache_size = allocate_cache_size;
    st->opt_drum_power = SetValue(opt_drum_power, 0, MAX_AMPLIFICATION);
    st->opt_amp_compensation = opt_amp_compensation;
    st->key_adjust = key_adjust;
    st->opt_force_keysig = opt_force_keysig;
    st->opt_pure_intonation = opt_pure_intonation;
    st->opt_init_keysig = opt_init_keysig;
    st->output_rate = opt_output_rate;
    if (st->output_rate == 0)
    {
        st->output_rate = play_mode->rate;
        if (st->output_rate == 0)
            st->output_rate = DEFAULT_RATE;
    }
    st->output_rate = SetValue(st->output_rate,MIN_OUTPUT_RATE,MAX_OUTPUT_RATE);
    strncpy(st->output_text_code,output_text_code,sizeof(st->output_text_code) -1);
    st->free_instruments_afterwards = free_instruments_afterwards;
    st->opt_user_volume_curve = opt_user_volume_curve;
///r
    st->opt_resample_type = SetValue(opt_resample_type, 0, RESAMPLE_MAX - 1);
    st->opt_resample_param = opt_resample_param;
    st->opt_resample_filter = opt_resample_filter;
    st->opt_resample_over_sampling = opt_resample_over_sampling;
    st->opt_pre_resamplation = opt_pre_resamplation;

    st->add_play_time = add_play_time;
    st->add_silent_time = add_silent_time;
    st->emu_delay_time = emu_delay_time;
    st->opt_limiter = opt_limiter;
    st->opt_use_midi_loop_repeat = opt_use_midi_loop_repeat;
    st->opt_midi_loop_repeat = opt_midi_loop_repeat;

    st->opt_mix_envelope = opt_mix_envelope;
    st->opt_modulation_update = opt_modulation_update;
    st->opt_cut_short_time = opt_cut_short_time;

    st->compute_thread_num = compute_thread_num;

#if defined(__W32__) && defined(SMFCONV)
    st->opt_rcpcv_dll = opt_rcpcv_dll;
#endif /* SMFCONV */

    st->opt_load_all_instrument = opt_load_all_instrument;

    /* for INT_SYNTH */
    st->opt_int_synth_sine = opt_int_synth_sine;
    st->opt_int_synth_rate = opt_int_synth_rate;
    st->opt_int_synth_update = opt_int_synth_update;
}

int __fastcall KbTimSetting::IniGetKeyInt(char *section, char *key,int *n)
{
    int ret = GetPrivateProfileInt(section, key, 0, m_szIniFile);
    *n = ret;
    return 0;
}

//int __fastcall KbTimSetting::IniGetKeyFloat(char *section, char *key,int *n)
//{
//    int ret = GetPrivateProfileInt(section, key, 0, m_szIniFile);
//    *n = ret;
//    return 0;
//}


#define INI_MAXLEN 1024
int __fastcall KbTimSetting::IniGetKeyFloat(char *section, char *key, FLOAT_T *n)
{
    CHAR *INI_INVALID = "INVALID PARAMETER";
    CHAR buffer[INI_MAXLEN];
    GetPrivateProfileString(section, key, INI_INVALID, buffer, INI_MAXLEN-1, m_szIniFile);
    if (strcasecmp(buffer, INI_INVALID))
    {
        *n = (FLOAT_T) atof(buffer);
        return 0;
    }
    else
        return 1;
}

int __fastcall KbTimSetting::IniGetKeyInt8(char *section, char *key,int8 *n)
{
    int ret;
    IniGetKeyInt(section, key, &ret);
    *n = ret;
    return 0;
}
int __fastcall KbTimSetting::IniGetKeyIntArray(char *section, char *key, int *n, int arraysize)
{
    int i;
    int ret = 0;
    char keybuffer[1024];
    for (i = 0; i < arraysize; i++) {
        sprintf(keybuffer,"%s%d",key,i);
        n[i] = GetPrivateProfileInt(section, keybuffer, 0, m_szIniFile);
    }
    return ret;
}
int __fastcall KbTimSetting::IniGetKeyStringN(char *section, char *key, char *str, int size)
{
    GetPrivateProfileString(section, key, "", str, size-1, m_szIniFile);
    str[size-1] = 0;
    return 0;
}

KbTimSetting::KbTimSetting(void)
{
    static int init_done = 0;
    if (!init_done) {
        init_done = 1;
        kbtim_SaveSettingTiMidity(&g_st_default);
    }
    memcpy(&m_st, &g_st_default, sizeof(m_st));
    m_szIniFile[0] = 0;
    m_szCfgFile[0] = 0;
    m_ftIni.dwHighDateTime = m_ftIni.dwLowDateTime = 0;
    m_ftCfg.dwHighDateTime = m_ftCfg.dwLowDateTime = 0;
    m_hNotification = INVALID_HANDLE_VALUE;
}
KbTimSetting::~KbTimSetting(void)
{
    Close();
}

///r
void KbTimSetting::KbTimOverrideSFSettingLoad(void)
{
    OverrideSFSettingLoad(m_szIniFile, strlen(m_szIniFile) + 1);
}


BOOL __fastcall KbTimSetting::LoadIniFile(const char *cszIniFile)
{
    char buff[1024];
    int32 temp32;
    FLOAT_T tempD;
    char tempC;
    FLOAT_T v_float = 0.0f;

    Close();

    static char INI_SEC_PLAYER[] = "PLAYER";
    static char INI_SEC_TIMIDITY[] = "TIMIDITY";
///r
    static char INI_SEC_KBTIM[] = "KBTIM";
    static char INI_SEC_PARAM[] = "param";
    static char INI_SEC_MAIN[] = "main";
    static char INI_SEC_CHORUS[] = "chorus";
    static char INI_SEC_COMP[] = "compressor";
    static char INI_SEC_REVERB[] = "reverb";
    static char INI_SEC_DELAY[] = "delay";
    static char INI_SEC_VST[] = "VST";
    static char INI_SEC_EFX[] = "efx";
    static char INI_SEC_DRIVER[] = "driver";

    lstrcpyn(m_szIniFile, cszIniFile, sizeof(m_szIniFile));
    SETTING_TIMIDITY *st = &m_st;
    memcpy(st, &g_st_default, sizeof(SETTING_TIMIDITY));
    // [PLAYER]
    IniGetKeyStringN(INI_SEC_PLAYER,"ConfigFile", m_szCfgFile, sizeof(m_szCfgFile));
    // [TIMIDITY]
    IniGetKeyInt32(INI_SEC_TIMIDITY,"amplification",&(st->amplification));
    IniGetKeyInt32(INI_SEC_TIMIDITY,"output_amplification",&(st->output_amplification));
    IniGetKeyInt(INI_SEC_TIMIDITY,"antialiasing_allowed",&(st->antialiasing_allowed));
    IniGetKeyInt(INI_SEC_TIMIDITY,"buffer_fragments",&(st->buffer_fragments));
    IniGetKeyInt(INI_SEC_TIMIDITY,"compute_buffer_bits",&(st->compute_buffer_bits));
    IniGetKeyInt32(INI_SEC_TIMIDITY,"control_ratio",&(st->control_ratio));
    IniGetKeyInt32(INI_SEC_TIMIDITY,"default_drumchannels",(int32*) &(st->default_drumchannels));
    IniGetKeyInt32(INI_SEC_TIMIDITY,"default_drumchannel_mask",(int32*) &(st->default_drumchannel_mask));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_modulation_wheel",&(st->opt_modulation_wheel));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_portamento",&(st->opt_portamento));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_nrpn_vibrato",&(st->opt_nrpn_vibrato));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_channel_pressure",&(st->opt_channel_pressure));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_trace_text_meta_event",&(st->opt_trace_text_meta_event));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_overlap_voice_allow",&(st->opt_overlap_voice_allow));
///r
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_overlap_voice_count",&(st->opt_overlap_voice_count));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_max_channel_voices",&(st->opt_max_channel_voices));

    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_reverb_control",&(st->opt_reverb_control));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_chorus_control",&(st->opt_chorus_control));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_surround_chorus",&(st->opt_surround_chorus));
///r
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_normal_chorus_plus",&(st->opt_normal_chorus_plus));

    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_tva_attack",&(st->opt_tva_attack));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_tva_decay",&(st->opt_tva_decay));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_tva_release",&(st->opt_tva_release));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_delay_control",&(st->opt_delay_control));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_default_module",&(st->opt_default_module));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_lpf_def",&(st->opt_lpf_def));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_hpf_def",&(st->opt_hpf_def));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_drum_effect",&(st->opt_drum_effect));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_modulation_envelope",&(st->opt_modulation_envelope));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_eq_control",&(st->opt_eq_control));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_insertion_effect",&(st->opt_insertion_effect));
    IniGetKeyInt(INI_SEC_TIMIDITY,"noise_sharp_type",&(st->noise_sharp_type));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_evil_mode",&(st->opt_evil_mode));
    IniGetKeyInt(INI_SEC_TIMIDITY,"adjust_panning_immediately",&(st->adjust_panning_immediately));
    IniGetKeyInt(INI_SEC_TIMIDITY,"fast_decay",&(st->opt_fast_decay));

    IniGetKeyIntArray(INI_SEC_TIMIDITY,"default_program",st->default_program,MAX_CHANNELS);
    IniGetKeyStringN(INI_SEC_TIMIDITY,"opt_ctl",st->opt_ctl,sizeof(st->opt_ctl) -1);
    IniGetKeyInt32(INI_SEC_TIMIDITY,"opt_drum_power",&(st->opt_drum_power));
    IniGetKeyInt32(INI_SEC_TIMIDITY,"opt_amp_compensation",&(st->opt_amp_compensation));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_realtime_playing",&(st->opt_realtime_playing));
///r
    IniGetKeyStringN(INI_SEC_TIMIDITY,"reduce_voice_threshold",st->reduce_voice_threshold,sizeof(st->reduce_voice_threshold));
    IniGetKeyStringN(INI_SEC_TIMIDITY,"reduce_quality_threshold",st->reduce_quality_threshold,sizeof(st->reduce_quality_threshold));
    IniGetKeyStringN(INI_SEC_TIMIDITY,"reduce_polyphony_threshold",st->reduce_polyphony_threshold,sizeof(st->reduce_polyphony_threshold));

    IniGetKeyStringN(INI_SEC_TIMIDITY,"opt_playmode",st->opt_playmode,sizeof(st->opt_playmode) -1);
    IniGetKeyStringN(INI_SEC_TIMIDITY,"OutputName",st->OutputName,sizeof(st->OutputName) -1);
    IniGetKeyStringN(INI_SEC_TIMIDITY,"OutputDirName",st->OutputDirName,sizeof(st->OutputDirName) -1);
    IniGetKeyInt(INI_SEC_TIMIDITY,"auto_output_mode",&(st->auto_output_mode));
    IniGetKeyInt(INI_SEC_TIMIDITY,"voices",&(st->voices));
    IniGetKeyInt(INI_SEC_TIMIDITY,"auto_reduce_polyphony",&(st->auto_reduce_polyphony));
    IniGetKeyInt32(INI_SEC_TIMIDITY,"quietchannels",(int32*) &(st->quietchannels));
    IniGetKeyInt(INI_SEC_TIMIDITY,"temper_type_mute",&(st->temper_type_mute));
    IniGetKeyStringN(INI_SEC_TIMIDITY,"opt_qsize",st->opt_qsize,sizeof(st->opt_qsize) -1);
    IniGetKeyInt32(INI_SEC_TIMIDITY,"modify_release",&(st->modify_release));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_resample_type",&(st->opt_resample_type));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_resample_param",&(st->opt_resample_param));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_resample_filter",&(st->opt_resample_filter));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_resample_over_sampling",&(st->opt_resample_over_sampling));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_pre_resamplation",&(st->opt_pre_resamplation));
    IniGetKeyStringN(INI_SEC_TIMIDITY,"allocate_cache_size",buff,sizeof(buff) -1);
    st->allocate_cache_size = str2size(buff);
    IniGetKeyInt(INI_SEC_TIMIDITY,"key_adjust",&(st->key_adjust));
    IniGetKeyInt8(INI_SEC_TIMIDITY,"opt_force_keysig",&(st->opt_force_keysig));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_pure_intonation",&(st->opt_pure_intonation));
    IniGetKeyInt8(INI_SEC_TIMIDITY,"opt_init_keysig",&(st->opt_init_keysig));
    IniGetKeyInt(INI_SEC_TIMIDITY,"output_rate",&(st->output_rate));
    IniGetKeyStringN(INI_SEC_TIMIDITY,"output_text_code",st->output_text_code,sizeof(st->output_text_code) -1);
    IniGetKeyInt(INI_SEC_TIMIDITY,"free_instruments_afterwards",&(st->free_instruments_afterwards));
///r
    v_float = 0.0;
    IniGetKeyFloat(INI_SEC_TIMIDITY,"opt_user_volume_curve",&v_float);
    st->opt_user_volume_curve = v_float;

    IniGetKeyInt(INI_SEC_TIMIDITY,"add_play_time",&(st->add_play_time));
    IniGetKeyInt(INI_SEC_TIMIDITY,"add_silent_time",&(st->add_silent_time));
    IniGetKeyInt(INI_SEC_TIMIDITY,"emu_delay_time",&(st->emu_delay_time));

    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_mix_envelope",&st->opt_mix_envelope);
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_modulation_update",&(st->opt_modulation_update));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_cut_short_time",&st->opt_cut_short_time);
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_limiter",&st->opt_limiter);
    IniGetKeyInt(INI_SEC_TIMIDITY, "opt_use_midi_loop_repeat", &st->opt_use_midi_loop_repeat);
    IniGetKeyInt(INI_SEC_TIMIDITY, "opt_midi_loop_repeat", &st->opt_midi_loop_repeat);

    IniGetKeyInt(INI_SEC_TIMIDITY,"compute_thread_num",&(st->compute_thread_num));

    IniGetKeyInt(INI_SEC_TIMIDITY,"trace_mode_update_time",&(st->trace_mode_update_time));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_load_all_instrument",&(st->opt_load_all_instrument));

#if defined(__W32__) && defined(SMFCONV)
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_rcpcv_dll",&(st->opt_rcpcv_dll));
#endif

    /* for INT_SYNTH */
    IniGetKeyInt32(INI_SEC_TIMIDITY,"opt_int_synth_sine",&(st->opt_int_synth_sine));
    IniGetKeyInt32(INI_SEC_TIMIDITY,"opt_int_synth_rate",&(st->opt_int_synth_rate));
    IniGetKeyInt32(INI_SEC_TIMIDITY,"opt_int_synth_update",&(st->opt_int_synth_update));

    // [KBTIM]
//  IniGetKeyInt(INI_SEC_KBTIM,"resample_type",&(st->opt_resample_type));
//  IniGetKeyInt(INI_SEC_KBTIM,"resample_param",&(st->opt_resample_param));

    KbTimOverrideSFSettingLoad();

    if (st->opt_chorus_control < 0) {
        st->opt_surround_chorus = 0;
    }
    //INI の内容を無視
    st->voices = 512;
    st->auto_reduce_polyphony = 0;

    //INI/CFG のファイル更新日時を取得
    GetIniCfgFileTime(&m_ftIni, &m_ftCfg);

    //CFG ファイルが置かれているフォルダの更新監視
    char szCfgDir[MAX_PATH*2];//CFG が置かれているフォルダ
    strcpy(szCfgDir, m_szCfgFile);
    char *p = szCfgDir;
    char *path = NULL;
    while (*p) {
        if (IsDBCSLeadByte((BYTE) *p)) {
            if (!p[1]) {
                *p = 0;
                break;
            }
            p += 2;
            continue;
        }
        if (*p == '\\') {
            path = p;
        }
        p++;
    }
    if (path) {
        path[1] = 0;
        if (path-szCfgDir > 3) {//ルートディレクトリ以外の場合は末尾の \ を除く
            path[0] = 0;
        }
    }
    if (szCfgDir[0]) {
        m_hNotification =
            FindFirstChangeNotification(szCfgDir,
                                        TRUE,
                                        FILE_NOTIFY_CHANGE_FILE_NAME |
                                        FILE_NOTIFY_CHANGE_DIR_NAME  |
                                        FILE_NOTIFY_CHANGE_LAST_WRITE);
    }
    else {
        m_hNotification = INVALID_HANDLE_VALUE;
    }

   return TRUE;
}

void __fastcall KbTimSetting::Close(void)
{
    memcpy(&m_st, &g_st_default, sizeof(m_st));
    m_szIniFile[0] = 0;
    m_szCfgFile[0] = 0;
    m_ftIni.dwHighDateTime = m_ftIni.dwLowDateTime = 0;
    m_ftCfg.dwHighDateTime = m_ftCfg.dwLowDateTime = 0;
    if (m_hNotification != INVALID_HANDLE_VALUE) {
        FindCloseChangeNotification(m_hNotification);
        m_hNotification = INVALID_HANDLE_VALUE;
    }
}

void __fastcall KbTimSetting::Apply(void)
{
    kbtim_ApplySettingTiMidity(&m_st);
}

void __fastcall KbTimSetting::GetIniCfgFileTime(FILETIME* pftIni, FILETIME* pftCfg)
{//INI/CFG ファイルの更新日時を取得
    HANDLE hFile;
    char     *filenames[] = {m_szIniFile, m_szCfgFile, NULL};
    FILETIME *filetimes[] = {pftIni, pftCfg, NULL};
    int i = 0;
    while (filenames[i]) {
        hFile = CreateFile(filenames[i],
                           GENERIC_READ,
                           FILE_SHARE_READ|FILE_SHARE_WRITE,
                           NULL,
                           OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL,
                           NULL);
        filetimes[i]->dwHighDateTime = filetimes[i]->dwLowDateTime = 0;
        if (hFile != INVALID_HANDLE_VALUE) {
            GetFileTime(hFile, NULL, NULL, filetimes[i]);
            CloseHandle(hFile);
        }
        i++;
    }
}
BOOL __fastcall KbTimSetting::IsUpdated(void)
{//
    FILETIME ftIni, ftCfg;
    GetIniCfgFileTime(&ftIni, &ftCfg);
    if (CompareFileTime(&ftIni, &m_ftIni) != 0 ||
        CompareFileTime(&ftCfg, &m_ftCfg) != 0) {
        //INI or CFG のファイル更新日時が変わっている
        //OutputDebugString("IniFile or CfgFile is updated\n");
        return TRUE;
    }
    else if ((ftIni.dwHighDateTime == 0 && ftIni.dwLowDateTime == 0) ||
             (ftCfg.dwHighDateTime == 0 && ftCfg.dwLowDateTime == 0)) {
        //INI or CFG ファイルが存在しない
        //OutputDebugString("IniFile or CfgFile does not exist\n");
        return TRUE;
    }
    else if (m_hNotification != INVALID_HANDLE_VALUE &&
             WaitForSingleObject(m_hNotification, 0) == WAIT_OBJECT_0) {
        //CFG が置かれているフォルダが更新されている
        //OutputDebugString("CfgFolder is updated\n");
        return TRUE;
    }
    else {
        return FALSE;
    }
}
