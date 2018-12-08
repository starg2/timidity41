/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2018 Masanao Izumo <iz@onicos.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

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

    w32g_ini.c: written by Daisuke Aoki <dai@y7.net>
                           Masanao Izumo <iz@onicos.co.jp>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */


#include "timidity.h"
#include "common.h"
#include "output.h"
#include "instrum.h"
#include "playmidi.h"
#include "effect.h"
#include "w32g.h"
#include "w32g_utl.h"

///r
#if MAX_CHANNELS > 32
//#error "MAX_CHANNELS > 32 is not supported Windows GUI version"
#define CHANNEL_ARRAY_ENABLE ((MAX_CHANNELS + 31) >> 5)
#endif


//////////// elioncfg
#include "myini.h"
///r
#include "sndfontini.h"


#pragma comment(lib, "shlwapi.lib")
#include <shlwapi.h>

void OverrideSFSettingSave(void)
{
    INIDATA ini = { 0 };
    LPINISEC sec = NULL;
    const char sndfontini[] = SNDFNT_INITFILE_NAME;

    char fn[FILEPATH_MAX] = "";
    char *p = NULL;

#if defined(WINDRV)
    GetWindowsDirectoryA(fn, FILEPATH_MAX - strlen(sndfontini) - 1);
    if (is_last_path_sep(fn))
        fn[strlen(fn) - 1] = 0;
    strlcat(fn, PATH_STRING, FILEPATH_MAX);
#else
    if (GetModuleFileNameA(GetModuleHandle(0), fn, FILEPATH_MAX - strlen(sndfontini) - 1 - 1)) {
        PathRemoveFileSpecA(fn);
        strcat(fn, PATH_STRING);
    } else {
        fn[0] = '.';
        fn[1] = PATH_SEP;
        fn[2] = '\0';
    }
#endif
    strlcat(fn, sndfontini, FILEPATH_MAX);
///r
    //if (otd.vibrato_cent == 0 && otd.vibrato_delay == 0) {
    //      OverrideSFSettingLoad();
    //}

    MyIni_Load(&ini, fn);
    sec = MyIni_GetSection(&ini, "param", TRUE);

    MyIni_SetInt32(sec, "VibratoDelay", OverrideSample.vibrato_delay);
    MyIni_SetInt32(sec, "VibratoDepth", OverrideSample.vibrato_to_pitch);
    MyIni_SetInt32(sec, "VibratoSweep", OverrideSample.vibrato_sweep);

    MyIni_SetInt32(sec, "VelToFc", OverrideSample.vel_to_fc);
    MyIni_SetInt32(sec, "VelToFcThr", OverrideSample.vel_to_fc_threshold);
    MyIni_SetInt32(sec, "VelToRes", OverrideSample.vel_to_resonance);

    MyIni_SetInt32(sec, "TremoloDelay", OverrideSample.tremolo_delay);
    MyIni_SetInt32(sec, "TremoloDepth", OverrideSample.tremolo_to_amp);
    MyIni_SetInt32(sec, "TremoloToFc", OverrideSample.tremolo_to_fc);
    MyIni_SetInt32(sec, "TremoloToPitch", OverrideSample.tremolo_to_pitch);
    MyIni_SetInt32(sec, "Res", OverrideSample.resonance);
    MyIni_SetInt32(sec, "CutoffFreq", OverrideSample.cutoff_freq);

    MyIni_SetInt32(sec, "EnvDelay", OverrideSample.envelope_delay);

    MyIni_SetInt32(sec, "ModEnvDelay", OverrideSample.modenv_delay);
    MyIni_SetInt32(sec, "ModEnvToFc", OverrideSample.modenv_to_fc);
    MyIni_SetInt32(sec, "ModEnvToPitch", OverrideSample.modenv_to_pitch);

    MyIni_SetInt8(sec, "Attenuation_Neg", sf_attenuation_neg);
    MyIni_SetFloat64(sec, "Attenuation_Pow", sf_attenuation_pow);
    MyIni_SetFloat64(sec, "Attenuation_Mul", sf_attenuation_mul);
    MyIni_SetFloat64(sec, "Attenuation_Add", sf_attenuation_add);

    MyIni_SetInt32(sec, "Limit_VolEnv_Attack", sf_limit_volenv_attack);
    MyIni_SetInt32(sec, "Limit_ModEnv_Attack", sf_limit_modenv_attack);
    MyIni_SetInt32(sec, "Limit_ModEnv_Fc", sf_limit_modenv_fc);
    MyIni_SetInt32(sec, "Limit_ModEnv_Pitch", sf_limit_modenv_pitch);
    MyIni_SetInt32(sec, "Limit_ModLfo_Fc", sf_limit_modlfo_fc);
    MyIni_SetInt32(sec, "Limit_ModLfo_Pitch", sf_limit_modlfo_pitch);
    MyIni_SetInt32(sec, "Limit_VibLfo_Pitch", sf_limit_viblfo_pitch);
    MyIni_SetInt32(sec, "Limit_ModLfo_Freq", sf_limit_modlfo_freq);
    MyIni_SetInt32(sec, "Limit_VibLfo_Freq", sf_limit_viblfo_freq);

    MyIni_SetInt32(sec, "Default_ModLfo_Freq", sf_default_modlfo_freq);
    MyIni_SetInt32(sec, "Default_VibLfo_Freq", sf_default_viblfo_freq);

    MyIni_SetInt8(sec, "Config_LFO_Swap", sf_config_lfo_swap);
    MyIni_SetInt8(sec, "Config_Addrs_Offset", sf_config_addrs_offset);

    MyIni_SetInt8(sec, "_SF2ChorusSend", otd.chorus_send);
    MyIni_SetInt8(sec, "_SF2ReverbSend", otd.reverb_send);

    MyIni_SetInt32(sec, "_VibratoRate", otd.vibrato_rate);
    MyIni_SetInt32(sec, "_VibratoCent", otd.vibrato_cent);
    MyIni_SetInt32(sec, "_VibratoDelay", otd.vibrato_delay);
    MyIni_SetInt32(sec, "_FilterFreq", otd.filter_freq);
    MyIni_SetInt32(sec, "_FilterReso", otd.filter_reso);

    MyIni_SetFloat64(sec, "VoiceFilter_Reso", voice_filter_reso);
    MyIni_SetFloat64(sec, "VoiceFilter_Gain", voice_filter_gain);

    sec = MyIni_GetSection(&ini, "envelope", TRUE);
    MyIni_SetFloat64(sec, "GS_ENV_Attack", gs_env_attack_calc);
    MyIni_SetFloat64(sec, "GS_ENV_Decay", gs_env_decay_calc);
    MyIni_SetFloat64(sec, "GS_ENV_Release", gs_env_release_calc);
    MyIni_SetFloat64(sec, "XG_ENV_Attack", xg_env_attack_calc);
    MyIni_SetFloat64(sec, "XG_ENV_Decay", xg_env_decay_calc);
    MyIni_SetFloat64(sec, "XG_ENV_Release", xg_env_release_calc);
    MyIni_SetFloat64(sec, "GM2_ENV_Attack", gm2_env_attack_calc);
    MyIni_SetFloat64(sec, "GM2_ENV_Decay", gm2_env_decay_calc);
    MyIni_SetFloat64(sec, "GM2_ENV_Release", gm2_env_release_calc);
    MyIni_SetFloat64(sec, "GM_ENV_Attack", gm_env_attack_calc);
    MyIni_SetFloat64(sec, "GM_ENV_Decay", gm_env_decay_calc);
    MyIni_SetFloat64(sec, "GM_ENV_Release", gm_env_release_calc);
    MyIni_SetFloat64(sec, "ENV_Add_Offdelay", env_add_offdelay_time);

    sec = MyIni_GetSection(&ini, "main", TRUE);
    MyIni_SetUint32(sec, "ver", 101);
    MyIni_SetBool(sec, "OverWriteVibrato", otd.overwriteMode & EOWM_ENABLE_VIBRATO);
    MyIni_SetBool(sec, "OverWriteTremolo", otd.overwriteMode & EOWM_ENABLE_TREMOLO);
    MyIni_SetBool(sec, "OverWriteCutoff", otd.overwriteMode & EOWM_ENABLE_CUTOFF);
    MyIni_SetBool(sec, "OverWriteVel", otd.overwriteMode & EOWM_ENABLE_VEL);
    MyIni_SetBool(sec, "OverWriteModEnv", otd.overwriteMode & EOWM_ENABLE_MOD);
    MyIni_SetBool(sec, "OverWriteEnv", otd.overwriteMode & EOWM_ENABLE_ENV);

    sec = MyIni_GetSection(&ini, "chorus", TRUE);
    MyIni_SetFloat64(sec, "Ext_Level", ext_chorus_level);
    MyIni_SetFloat64(sec, "Ext_Feedback", ext_chorus_feedback);
    MyIni_SetFloat64(sec, "Ext_Depth", ext_chorus_depth);
    MyIni_SetInt32(sec, "Ext_EX_Phase", ext_chorus_ex_phase);
    MyIni_SetInt32(sec, "Ext_EX_Lite", ext_chorus_ex_lite);
    MyIni_SetInt32(sec, "Ext_EX_OV", ext_chorus_ex_ov);

#ifdef CUSTOMIZE_CHORUS_PARAM
    MyIni_SetInt8(sec, "PreLPF", otd.chorus_param.pre_lpf);
    MyIni_SetInt8(sec, "Level", otd.chorus_param.level);
    MyIni_SetInt8(sec, "Feedback", otd.chorus_param.feedback);
    MyIni_SetInt8(sec, "Delay", otd.chorus_param.delay);
    MyIni_SetInt8(sec, "Rate", otd.chorus_param.rate);
    MyIni_SetInt8(sec, "Depth", otd.chorus_param.depth);
    MyIni_SetInt8(sec, "SendReverb", otd.chorus_param.send_reverb);
    MyIni_SetInt8(sec, "SendDelay", otd.chorus_param.send_delay);
#endif

///r
    sec = MyIni_GetSection(&ini, "efx", 1);
    MyIni_SetFloat64(sec, "GS_OD_Level", otd.gsefx_CustomODLv);
    MyIni_SetFloat64(sec, "GS_OD_Drive", otd.gsefx_CustomODDrive);
    MyIni_SetFloat64(sec, "GS_OD_Freq", otd.gsefx_CustomODFreq);
    MyIni_SetFloat64(sec, "XG_OD_Drive", otd.xgefx_CustomODDrive);
    MyIni_SetFloat64(sec, "XG_OD_Level", otd.xgefx_CustomODLv);
    MyIni_SetFloat64(sec, "XG_OD_Freq", otd.xgefx_CustomODFreq);
    MyIni_SetFloat64(sec, "SD_OD_Drive", otd.sdefx_CustomODDrive);
    MyIni_SetFloat64(sec, "SD_OD_Level", otd.sdefx_CustomODLv);
    MyIni_SetFloat64(sec, "SD_OD_Freq", otd.sdefx_CustomODFreq);

    MyIni_SetFloat64(sec, "GS_LF_In_Level", otd.gsefx_CustomLFLvIn);
    MyIni_SetFloat64(sec, "GS_LF_Out_Level", otd.gsefx_CustomLFLvOut);
    MyIni_SetFloat64(sec, "XG_LF_In_Level", otd.xgefx_CustomLFLvIn);
    MyIni_SetFloat64(sec, "XG_LF_Out_Level", otd.xgefx_CustomLFLvOut);
    MyIni_SetFloat64(sec, "SD_LF_In_Level", otd.sdefx_CustomLFLvIn);
    MyIni_SetFloat64(sec, "SD_LF_Out_Level", otd.sdefx_CustomLFLvOut);

    MyIni_SetFloat64(sec, "Hmn_In_Level", otd.efx_CustomHmnLvIn);
    MyIni_SetFloat64(sec, "Hmn_Out_Level", otd.efx_CustomHmnLvOut);

    MyIni_SetFloat64(sec, "Lmt_In_Level", otd.efx_CustomLmtLvIn);
    MyIni_SetFloat64(sec, "Lmt_Out_Level", otd.efx_CustomLmtLvOut);

    MyIni_SetFloat64(sec, "Cmp_In_Level", otd.efx_CustomCmpLvIn);
    MyIni_SetFloat64(sec, "Cmp_Out_Level", otd.efx_CustomCmpLvOut);

    MyIni_SetFloat64(sec, "Wah_In_Level", otd.efx_CustomWahLvIn);
    MyIni_SetFloat64(sec, "Wah_Out_Level", otd.efx_CustomWahLvOut);

    MyIni_SetFloat64(sec, "GRev_In_Level", otd.efx_CustomGRevLvIn);
    MyIni_SetFloat64(sec, "GRev_Out_Level", otd.efx_CustomGRevLvOut);

    MyIni_SetFloat64(sec, "Enh_In_Level", otd.efx_CustomEnhLvIn);
    MyIni_SetFloat64(sec, "Enh_Out_Level", otd.efx_CustomEnhLvOut);

    MyIni_SetFloat64(sec, "Rot_Out_Level", otd.efx_CustomRotLvOut);

    MyIni_SetFloat64(sec, "PS_Out_Level", otd.efx_CustomPSLvOut);

    MyIni_SetFloat64(sec, "RM_Out_Level", otd.efx_CustomRMLvOut);

    MyIni_SetInt32(sec, "Rev_Type", otd.efx_CustomRevType);

    sec = MyIni_GetSection(&ini, "compressor", 1);
    MyIni_SetBool(sec, "enable", otd.timRunMode & EOWM_USE_COMPRESSOR);
    MyIni_SetFloat32(sec, "threshold", otd.compThr);
    MyIni_SetFloat32(sec, "slope", otd.compSlope);
    MyIni_SetFloat32(sec, "lookahead", otd.compLook);
    MyIni_SetFloat32(sec, "window", otd.compWTime);
    MyIni_SetFloat32(sec, "attack", otd.compATime);
    MyIni_SetFloat32(sec, "release", otd.compRTime);

///r
    sec = MyIni_GetSection(&ini, "reverb", 1);
    // freeverb
    MyIni_SetFloat32(sec, "scalewet", scalewet);
    MyIni_SetFloat32(sec, "scaleroom", freeverb_scaleroom);
    MyIni_SetFloat32(sec, "offsetroom", freeverb_offsetroom);
    MyIni_SetFloat32(sec, "fixedgain", fixedgain);
    MyIni_SetFloat32(sec, "combfbk", combfbk);
    MyIni_SetFloat32(sec, "timediff", time_rt_diff);
    MyIni_SetInt32(sec, "numcombs", numcombs);
    // reverb ex
    MyIni_SetFloat32(sec, "Ext_EX_Time", ext_reverb_ex_time);
    MyIni_SetFloat32(sec, "Ext_EX_Level", ext_reverb_ex_level);
    MyIni_SetFloat32(sec, "Ext_EX_ER_Level", ext_reverb_ex_er_level);
    MyIni_SetFloat32(sec, "Ext_EX_RV_Level", ext_reverb_ex_rv_level);
    MyIni_SetInt32(sec, "Ext_EX_RV_Num", ext_reverb_ex_rv_num);
    MyIni_SetInt32(sec, "Ext_EX_AP_Num", ext_reverb_ex_ap_num);
    //MyIni_SetInt32(sec, "Ext_EX_Lite", ext_reverb_ex_lite);
    //MyIni_SetInt32(sec, "Ext_EX_Mode", ext_reverb_ex_mode);
    //MyIni_SetInt32(sec, "Ext_EX_ER_Num", ext_reverb_ex_er_num);
    //MyIni_SetInt32(sec, "Ext_EX_RV_Type", ext_reverb_ex_rv_type);
    //MyIni_SetInt32(sec, "Ext_EX_AP_Type", ext_reverb_ex_ap_type);
    MyIni_SetInt32(sec, "Ext_EX_Mod", ext_reverb_ex_mod);
    // plate reverb
    MyIni_SetFloat32(sec, "Ext_Plate_Level", ext_plate_reverb_level);
    MyIni_SetFloat32(sec, "Ext_Plate_Time", ext_plate_reverb_time);

    sec = MyIni_GetSection(&ini, "delay", 1);
    MyIni_SetFloat32(sec, "delay", otd.delay_param.delay);
    MyIni_SetFloat64(sec, "level", otd.delay_param.level);
    MyIni_SetFloat64(sec, "feedback", otd.delay_param.feedback);

    sec = MyIni_GetSection(&ini, "filter", 1);
    MyIni_SetFloat64(sec, "Ext_Shelving_Gain", ext_filter_shelving_gain);
    MyIni_SetFloat64(sec, "Ext_Shelving_Reduce", ext_filter_shelving_reduce);
    MyIni_SetFloat64(sec, "Ext_Shelving_Q", ext_filter_shelving_q);
    MyIni_SetFloat64(sec, "Ext_Peaking_Gain", ext_filter_peaking_gain);
    MyIni_SetFloat64(sec, "Ext_Peaking_Reduce", ext_filter_peaking_reduce);
    MyIni_SetFloat64(sec, "Ext_Peaking_Q", ext_filter_peaking_q);

    sec = MyIni_GetSection(&ini, "effect", 1);
    //MyIni_SetFloat64(sec, "XG_System_Return_Level", xg_system_return_level);
    MyIni_SetFloat64(sec, "XG_Reverb_Return_Level", xg_reverb_return_level);
    MyIni_SetFloat64(sec, "XG_Chorus_Return_Level", xg_chorus_return_level);
    MyIni_SetFloat64(sec, "XG_Variation_Return_Level", xg_variation_return_level);
    MyIni_SetFloat64(sec, "XG_Chorus_Send_Reverb", xg_chorus_send_reverb);
    MyIni_SetFloat64(sec, "XG_Variation_Send_Reverb", xg_variation_send_reverb);
    MyIni_SetFloat64(sec, "XG_Variation_Send_Chorus", xg_variation_send_chorus);

    MyIni_Save(&ini, fn);
    MyIni_SectionAllClear(&ini);
}


extern void IniFlush(void);

int w32g_has_ini_file;

static int32 str2size(const char *str)
{
    int len;
    if (!str)
        return 0;
    len = strlen(str);
    if (!len)
        return 0;
    if (str[len - 1] == 'k' || str[len - 1] == 'K')
        return (int32)(1024. * atof(str));
    if (str[len - 1] == 'm' || str[len - 1] == 'M')
        return (int32)(1024. * 1024 * atof(str));
    if (str[len - 1] == 'g' || str[len - 1] == 'G')
        return (int32)(1024. * 1024 * 1024 * atof(str));
    return atoi(str);
}

void LoadIniFile(SETTING_PLAYER *sp,  SETTING_TIMIDITY *st)
{
    char buff[1024];
    FLOAT_T v_float = 0.0f;

    /* [PLAYER] */
    IniGetKeyInt(INI_SEC_PLAYER,"InitMinimizeFlag",&(sp->InitMinimizeFlag));
    IniGetKeyInt(INI_SEC_PLAYER,"DebugWndStartFlag",&(sp->DebugWndStartFlag));
    IniGetKeyInt(INI_SEC_PLAYER,"ConsoleWndStartFlag",&(sp->ConsoleWndStartFlag));
    IniGetKeyInt(INI_SEC_PLAYER,"ListWndStartFlag",&(sp->ListWndStartFlag));
    IniGetKeyInt(INI_SEC_PLAYER,"TracerWndStartFlag",&(sp->TracerWndStartFlag));
    IniGetKeyInt(INI_SEC_PLAYER,"DocWndStartFlag",&(sp->DocWndStartFlag));
    IniGetKeyInt(INI_SEC_PLAYER,"WrdWndStartFlag",&(sp->WrdWndStartFlag));
    IniGetKeyInt(INI_SEC_PLAYER,"DebugWndFlag",&(sp->DebugWndFlag));
    IniGetKeyInt(INI_SEC_PLAYER,"ConsoleWndFlag",&(sp->ConsoleWndFlag));
    IniGetKeyInt(INI_SEC_PLAYER,"ListWndFlag",&(sp->ListWndFlag));
    IniGetKeyInt(INI_SEC_PLAYER,"TracerWndFlag",&(sp->TracerWndFlag));
    IniGetKeyInt(INI_SEC_PLAYER,"DocWndFlag",&(sp->DocWndFlag));
    IniGetKeyInt(INI_SEC_PLAYER,"WrdWndFlag",&(sp->WrdWndFlag));
    IniGetKeyInt(INI_SEC_PLAYER,"SoundSpecWndFlag",&(sp->SoundSpecWndFlag));
    IniGetKeyInt(INI_SEC_PLAYER,"SubWindowMax",&(sp->SubWindowMax));
    IniGetKeyInt(INI_SEC_PLAYER,"PlaylistMax",&(sp->PlaylistMax));
    IniGetKeyInt(INI_SEC_PLAYER,"ConsoleClearFlag",&(sp->ConsoleClearFlag));
    IniGetKeyStringN(INI_SEC_PLAYER,"ConfigFile",sp->ConfigFile,FILEPATH_MAX);
    if (!sp->ConfigFile[0]) {
        const char timiditycfg[] = CONFIG_FILE_NAME_P;
        GetWindowsDirectory(sp->ConfigFile, sizeof(sp->ConfigFile) - 14);
        strlcat(sp->ConfigFile, timiditycfg, FILEPATH_MAX);
    }
    IniGetKeyStringN(INI_SEC_PLAYER,"PlaylistFile",sp->PlaylistFile,FILEPATH_MAX);
    IniGetKeyStringN(INI_SEC_PLAYER,"PlaylistHistoryFile",sp->PlaylistHistoryFile,FILEPATH_MAX);
    IniGetKeyStringN(INI_SEC_PLAYER,"MidiFileOpenDir",sp->MidiFileOpenDir,FILEPATH_MAX);
    IniGetKeyStringN(INI_SEC_PLAYER,"ConfigFileOpenDir",sp->ConfigFileOpenDir,FILEPATH_MAX);
    IniGetKeyStringN(INI_SEC_PLAYER,"PlaylistFileOpenDir",sp->PlaylistFileOpenDir,FILEPATH_MAX);
    IniGetKeyInt(INI_SEC_PLAYER,"PlayerThreadPriority",&(sp->PlayerThreadPriority));
    IniGetKeyInt(INI_SEC_PLAYER,"GUIThreadPriority",&(sp->GUIThreadPriority));
    IniGetKeyStringN(INI_SEC_PLAYER,"SystemFont",sp->SystemFont,LF_FULLFACESIZE + 1);
    IniGetKeyStringN(INI_SEC_PLAYER,"PlayerFont",sp->PlayerFont,LF_FULLFACESIZE + 1);
    IniGetKeyStringN(INI_SEC_PLAYER,"WrdFont",sp->WrdFont,LF_FULLFACESIZE + 1);
    IniGetKeyStringN(INI_SEC_PLAYER,"DocFont",sp->DocFont,LF_FULLFACESIZE + 1);
    IniGetKeyStringN(INI_SEC_PLAYER,"ListFont",sp->ListFont,LF_FULLFACESIZE + 1);
    IniGetKeyStringN(INI_SEC_PLAYER,"TracerFont",sp->TracerFont,LF_FULLFACESIZE + 1);
    IniGetKeyInt(INI_SEC_PLAYER,"SystemFontSize",&(sp->SystemFontSize));
    IniGetKeyInt(INI_SEC_PLAYER,"PlayerFontSize",&(sp->PlayerFontSize));
    IniGetKeyInt(INI_SEC_PLAYER,"WrdFontSize",&(sp->WrdFontSize));
    IniGetKeyInt(INI_SEC_PLAYER,"DocFontSize",&(sp->DocFontSize));
    IniGetKeyInt(INI_SEC_PLAYER,"ListFontSize",&(sp->ListFontSize));
    IniGetKeyInt(INI_SEC_PLAYER,"TracerFontSize",&(sp->TracerFontSize));
    IniGetKeyInt(INI_SEC_PLAYER,"WrdGraphicFlag",&(sp->WrdGraphicFlag));
    IniGetKeyInt(INI_SEC_PLAYER,"TraceGraphicFlag",&(sp->TraceGraphicFlag));
    IniGetKeyInt(INI_SEC_PLAYER,"DocMaxSize",&(sp->DocMaxSize));
    IniGetKeyStringN(INI_SEC_PLAYER,"DocFileExt",sp->DocFileExt,256);
    IniGetKeyInt(INI_SEC_PLAYER,"PlayerLanguage",&(sp->PlayerLanguage));
    IniGetKeyInt(INI_SEC_PLAYER,"DocWndIndependent",&(sp->DocWndIndependent));
    IniGetKeyInt(INI_SEC_PLAYER,"DocWndAutoPopup",&(sp->DocWndAutoPopup));
    IniGetKeyInt(INI_SEC_PLAYER,"SeachDirRecursive",&(sp->SeachDirRecursive));
    IniGetKeyInt(INI_SEC_PLAYER,"IniFileAutoSave",&(sp->IniFileAutoSave));
    IniGetKeyInt(INI_SEC_PLAYER,"SecondMode",&(sp->SecondMode));
    IniGetKeyInt(INI_SEC_PLAYER,"AutoloadPlaylist",&(sp->AutoloadPlaylist));
    IniGetKeyInt(INI_SEC_PLAYER,"AutosavePlaylist",&(sp->AutosavePlaylist));
    IniGetKeyInt(INI_SEC_PLAYER,"PosSizeSave",&(sp->PosSizeSave));
    IniGetKeyInt(INI_SEC_PLAYER,"main_panel_update_time",&(sp->main_panel_update_time));

    /* [TIMIDITY] */
    IniGetKeyInt32(INI_SEC_TIMIDITY,"amplification",&(st->amplification));
    IniGetKeyInt32(INI_SEC_TIMIDITY,"output_amplification",&(st->output_amplification));
    IniGetKeyInt(INI_SEC_TIMIDITY,"antialiasing_allowed",&(st->antialiasing_allowed));
///r
    IniGetKeyInt(INI_SEC_TIMIDITY,"buffer_fragments",&(st->buffer_fragments));
    if (st->buffer_fragments > 4096 || st->buffer_fragments < 2)
        st->buffer_fragments = 64;
    IniGetKeyInt(INI_SEC_TIMIDITY,"audio_buffer_bits",&(st->audio_buffer_bits));
    if (st->audio_buffer_bits > AUDIO_BUFFER_BITS || st->audio_buffer_bits < 5)
        st->audio_buffer_bits = AUDIO_BUFFER_BITS;
    IniGetKeyInt(INI_SEC_TIMIDITY,"compute_buffer_bits",&(st->compute_buffer_bits));
    if (st->compute_buffer_bits > 10 || st->compute_buffer_bits < -5)
        st->compute_buffer_bits = 6;
    IniGetKeyInt32(INI_SEC_TIMIDITY,"control_ratio",&(st->control_ratio));
///r
#if MAX_CHANNELS > 32
// ChannelBitMask int32[8]
    IniGetKeyInt32Array(INI_SEC_TIMIDITY,"default_drumchannels",(int32*) &(st->default_drumchannels), CHANNEL_ARRAY_ENABLE);
    IniGetKeyInt32Array(INI_SEC_TIMIDITY,"default_drumchannel_mask",(int32*) &(st->default_drumchannel_mask), CHANNEL_ARRAY_ENABLE);
    IniGetKeyInt32Array(INI_SEC_TIMIDITY,"quietchannels",(int32*) &(st->quietchannels), CHANNEL_ARRAY_ENABLE);
#else
// ChannelBitMask uint32
    IniGetKeyInt32(INI_SEC_TIMIDITY,"default_drumchannels",(int32*) &(st->default_drumchannels));
    IniGetKeyInt32(INI_SEC_TIMIDITY,"default_drumchannel_mask",(int32*) &(st->default_drumchannel_mask));
    IniGetKeyInt32(INI_SEC_TIMIDITY,"quietchannels",(int32*) &(st->quietchannels));
#endif
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_modulation_wheel",&(st->opt_modulation_wheel));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_portamento",&(st->opt_portamento));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_nrpn_vibrato",&(st->opt_nrpn_vibrato));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_channel_pressure",&(st->opt_channel_pressure));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_trace_text_meta_event",&(st->opt_trace_text_meta_event));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_overlap_voice_allow",&(st->opt_overlap_voice_allow));
///r
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_overlap_voice_count",&(st->opt_overlap_voice_count));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_max_channel_voices",&(st->opt_max_channel_voices));
///r
//    IniGetKeyStringN(INI_SEC_TIMIDITY,"opt_default_mid",buff,sizeof(buff) -1);
//    st->opt_default_mid = str2mID(buff);
//    st->opt_default_mid = atoi(buff);
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_default_mid",&(st->opt_default_mid));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_system_mid",&(st->opt_system_mid));
    IniGetKeyInt(INI_SEC_TIMIDITY,"default_tonebank",&(st->default_tonebank));
    IniGetKeyInt(INI_SEC_TIMIDITY,"special_tonebank",&(st->special_tonebank));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_reverb_control",&(st->opt_reverb_control));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_chorus_control",&(st->opt_chorus_control));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_surround_chorus",&(st->opt_surround_chorus));
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
    IniGetKeyInt(INI_SEC_TIMIDITY,"min_sustain_time",&(st->min_sustain_time));
#ifdef SUPPORT_SOUNDSPEC
    IniGetKeyInt(INI_SEC_TIMIDITY,"view_soundspec_flag",&(st->view_soundspec_flag));
    v_float = 0.0;
    IniGetKeyFloat(INI_SEC_TIMIDITY,"spectrogram_update_sec",&v_float);
    st->spectrogram_update_sec = v_float;
#endif
    IniGetKeyIntArray(INI_SEC_TIMIDITY,"default_program",st->default_program,MAX_CHANNELS);
    IniGetKeyIntArray(INI_SEC_TIMIDITY,"special_program",st->special_program,MAX_CHANNELS);
    IniGetKeyStringN(INI_SEC_TIMIDITY,"opt_ctl",st->opt_ctl,sizeof(st->opt_ctl) -1);
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_print_fontname",&(st->opt_print_fontname));
    IniGetKeyInt32(INI_SEC_TIMIDITY,"opt_drum_power",&(st->opt_drum_power));
    IniGetKeyInt32(INI_SEC_TIMIDITY,"opt_amp_compensation",&(st->opt_amp_compensation));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_realtime_playing",&(st->opt_realtime_playing));
///r
    IniGetKeyStringN(INI_SEC_TIMIDITY,"reduce_voice_threshold",st->reduce_voice_threshold,sizeof(st->reduce_voice_threshold) -1);
    IniGetKeyStringN(INI_SEC_TIMIDITY,"reduce_quality_threshold",st->reduce_quality_threshold,sizeof(st->reduce_quality_threshold) -1);
    IniGetKeyStringN(INI_SEC_TIMIDITY,"reduce_polyphony_threshold",st->reduce_polyphony_threshold,sizeof(st->reduce_polyphony_threshold) -1);

    IniGetKeyStringN(INI_SEC_TIMIDITY,"opt_playmode",st->opt_playmode,sizeof(st->opt_playmode) -1);
    IniGetKeyStringN(INI_SEC_TIMIDITY,"OutputName",st->OutputName,sizeof(st->OutputName) -1);
    IniGetKeyStringN(INI_SEC_TIMIDITY,"OutputDirName",st->OutputDirName,sizeof(st->OutputDirName) -1);
    IniGetKeyInt(INI_SEC_TIMIDITY,"auto_output_mode",&(st->auto_output_mode));
    IniGetKeyInt(INI_SEC_TIMIDITY,"voices",&(st->voices));
    IniGetKeyInt(INI_SEC_TIMIDITY,"auto_reduce_polyphony",&(st->auto_reduce_polyphony));
    IniGetKeyInt(INI_SEC_TIMIDITY,"temper_type_mute",&(st->temper_type_mute));
#if !(defined(IA_W32G_SYN) || defined(WINDRV_SETUP))
    IniGetKeyStringN(INI_SEC_TIMIDITY,"opt_qsize",st->opt_qsize,sizeof(st->opt_qsize) -1);
#endif
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
    IniGetKeyStringN(INI_SEC_TIMIDITY,"out_wrd",st->opt_wrd,sizeof(st->opt_wrd) -1);
///r
#ifdef AU_W32
    IniGetKeyInt32(INI_SEC_TIMIDITY,"wmme_device_id",&st->wmme_device_id);
    IniGetKeyInt(INI_SEC_TIMIDITY,"wave_format_ext",&st->wave_format_ext);
#endif
#ifdef AU_WASAPI
    IniGetKeyInt32(INI_SEC_TIMIDITY,"opt_wasapi_device_id",&st->wasapi_device_id);
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_wasapi_latency",&st->wasapi_latency);
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_wasapi_format_ext",&st->wasapi_format_ext);
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_wasapi_exclusive",&st->wasapi_exclusive);
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_wasapi_polling",&st->wasapi_polling);
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_wasapi_priority",&st->wasapi_priority);
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_wasapi_stream_category",&st->wasapi_stream_category);
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_wasapi_stream_option",&st->wasapi_stream_option);
#endif
#ifdef AU_WDMKS
    IniGetKeyInt32(INI_SEC_TIMIDITY,"opt_wdmks_device_id",&st->wdmks_device_id);
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_wdmks_latency",&st->wdmks_latency);
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_wdmks_format_ext",&st->wdmks_format_ext);
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_wdmks_polling",&st->wdmks_polling);
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_wdmks_thread__priority",&st->wdmks_thread_priority);
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_wdmks_pin_priority",&st->wdmks_pin_priority);
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_wdmks_rt_priority",&st->wdmks_rt_priority);
#endif
#ifdef AU_PORTAUDIO
    IniGetKeyInt32(INI_SEC_TIMIDITY,"pa_wmme_device_id",&st->pa_wmme_device_id);
    IniGetKeyInt32(INI_SEC_TIMIDITY,"pa_ds_device_id",&st->pa_ds_device_id);
    IniGetKeyInt32(INI_SEC_TIMIDITY,"pa_asio_device_id",&st->pa_asio_device_id);
    IniGetKeyInt32(INI_SEC_TIMIDITY,"pa_wdmks_device_id",&st->pa_wdmks_device_id);
    IniGetKeyInt32(INI_SEC_TIMIDITY,"pa_wasapi_device_id",&st->pa_wasapi_device_id);
    IniGetKeyInt(INI_SEC_TIMIDITY,"pa_wasapi_flag",&st->pa_wasapi_flag);
    IniGetKeyInt(INI_SEC_TIMIDITY,"pa_wasapi_stream_category",&st->pa_wasapi_stream_category);
    IniGetKeyInt(INI_SEC_TIMIDITY,"pa_wasapi_stream_option",&st->pa_wasapi_stream_option);
#endif
    IniGetKeyInt(INI_SEC_TIMIDITY,"add_play_time",&(st->add_play_time));
    IniGetKeyInt(INI_SEC_TIMIDITY,"add_silent_time",&(st->add_silent_time));
    IniGetKeyInt(INI_SEC_TIMIDITY,"emu_delay_time",&(st->emu_delay_time));

    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_mix_envelope",&st->opt_mix_envelope);
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_modulation_update",&(st->opt_modulation_update));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_cut_short_time",&st->opt_cut_short_time);
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_limiter",&st->opt_limiter);
    IniGetKeyInt(INI_SEC_TIMIDITY, "opt_use_midi_loop_repeat", &st->opt_use_midi_loop_repeat);
    IniGetKeyInt(INI_SEC_TIMIDITY, "opt_midi_loop_repeat", &st->opt_midi_loop_repeat);

#if defined(__W32__) && defined(SMFCONV)
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_rcpcv_dll",&(st->opt_rcpcv_dll));
#endif
//  IniGetKeyInt(INI_SEC_TIMIDITY,"data_block_bits",&(st->data_block_bits));
//  if (st->data_block_bits > AUDIO_BUFFER_BITS)
//      st->data_block_bits = AUDIO_BUFFER_BITS;
//  IniGetKeyInt(INI_SEC_TIMIDITY,"data_block_num",&(st->data_block_num));

#if defined(WINDRV_SETUP)
    IniGetKeyInt(INI_SEC_TIMIDITY,"processPriority",&(st->processPriority));
    IniGetKeyInt(INI_SEC_TIMIDITY,"syn_ThreadPriority",&(st->syn_ThreadPriority));
    IniGetKeyInt(INI_SEC_TIMIDITY,"SynShTime",&(st->SynShTime));
    if ( st->SynShTime < 0 ) st->SynShTime = 0;
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_rtsyn_latency",&(st->opt_rtsyn_latency));
    if ( st->opt_rtsyn_latency < 1 ) st->opt_rtsyn_latency = 1;
    if ( st->opt_rtsyn_latency > 1000 ) st->opt_rtsyn_latency = 1000;
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_rtsyn_skip_aq",&(st->opt_rtsyn_skip_aq));
#elif defined(IA_W32G_SYN)
    IniGetKeyIntArray(INI_SEC_TIMIDITY,"SynIDPort",st->SynIDPort,MAX_PORT);
    IniGetKeyInt(INI_SEC_TIMIDITY,"syn_AutoStart",&(st->syn_AutoStart));
    IniGetKeyInt(INI_SEC_TIMIDITY,"processPriority",&(st->processPriority));
    IniGetKeyInt(INI_SEC_TIMIDITY,"syn_ThreadPriority",&(st->syn_ThreadPriority));
    IniGetKeyInt(INI_SEC_TIMIDITY,"SynPortNum",&(st->SynPortNum));
    IniGetKeyInt(INI_SEC_TIMIDITY,"SynShTime",&(st->SynShTime));
    if ( st->SynShTime < 0 ) st->SynShTime = 0;
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_rtsyn_latency",&(st->opt_rtsyn_latency));
    if ( st->opt_rtsyn_latency < 1 ) st->opt_rtsyn_latency = 1;
    if ( st->opt_rtsyn_latency > 1000 ) st->opt_rtsyn_latency = 1000;
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_rtsyn_skip_aq",&(st->opt_rtsyn_skip_aq));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_use_twsyn_bridge",&(st->opt_use_twsyn_bridge));
#else
    IniGetKeyInt(INI_SEC_TIMIDITY,"processPriority",&(st->processPriority));
#endif
    IniGetKeyInt(INI_SEC_TIMIDITY,"compute_thread_num",&(st->compute_thread_num));

    IniGetKeyInt(INI_SEC_TIMIDITY,"trace_mode_update_time",&(st->trace_mode_update_time));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_load_all_instrument",&(st->opt_load_all_instrument));

    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_load_all_instrument",&(st->opt_load_all_instrument));

    /* for INT_SYNTH */
    IniGetKeyInt32(INI_SEC_TIMIDITY,"opt_int_synth_sine",&(st->opt_int_synth_sine));
    IniGetKeyInt32(INI_SEC_TIMIDITY,"opt_int_synth_rate",&(st->opt_int_synth_rate));
    IniGetKeyInt32(INI_SEC_TIMIDITY,"opt_int_synth_update",&(st->opt_int_synth_update));
}

void
SaveIniFile(SETTING_PLAYER *sp,  SETTING_TIMIDITY *st)
{
    /* [PLAYER] */
    IniPutKeyString(INI_SEC_PLAYER,"IniVersion",IniVersion);
    IniPutKeyInt(INI_SEC_PLAYER,"InitMinimizeFlag",&(sp->InitMinimizeFlag));
    IniPutKeyInt(INI_SEC_PLAYER,"DebugWndStartFlag",&(sp->DebugWndStartFlag));
    IniPutKeyInt(INI_SEC_PLAYER,"ConsoleWndStartFlag",&(sp->ConsoleWndStartFlag));
    IniPutKeyInt(INI_SEC_PLAYER,"ListWndStartFlag",&(sp->ListWndStartFlag));
    IniPutKeyInt(INI_SEC_PLAYER,"TracerWndStartFlag",&(sp->TracerWndStartFlag));
    IniPutKeyInt(INI_SEC_PLAYER,"DocWndStartFlag",&(sp->DocWndStartFlag));
    IniPutKeyInt(INI_SEC_PLAYER,"WrdWndStartFlag",&(sp->WrdWndStartFlag));
    IniPutKeyInt(INI_SEC_PLAYER,"DebugWndFlag",&(sp->DebugWndFlag));
    IniPutKeyInt(INI_SEC_PLAYER,"ConsoleWndFlag",&(sp->ConsoleWndFlag));
    IniPutKeyInt(INI_SEC_PLAYER,"ListWndFlag",&(sp->ListWndFlag));
    IniPutKeyInt(INI_SEC_PLAYER,"TracerWndFlag",&(sp->TracerWndFlag));
    IniPutKeyInt(INI_SEC_PLAYER,"DocWndFlag",&(sp->DocWndFlag));
    IniPutKeyInt(INI_SEC_PLAYER,"WrdWndFlag",&(sp->WrdWndFlag));
    IniPutKeyInt(INI_SEC_PLAYER,"SoundSpecWndFlag",&(sp->SoundSpecWndFlag));
    IniPutKeyInt(INI_SEC_PLAYER,"SubWindowMax",&(sp->SubWindowMax));
    IniPutKeyInt(INI_SEC_PLAYER,"PlaylistMax",&(sp->PlaylistMax));
    IniPutKeyInt(INI_SEC_PLAYER,"ConsoleClearFlag",&(sp->ConsoleClearFlag));
    IniPutKeyStringN(INI_SEC_PLAYER,"ConfigFile",sp->ConfigFile,FILEPATH_MAX);
    IniPutKeyStringN(INI_SEC_PLAYER,"PlaylistFile",sp->PlaylistFile,FILEPATH_MAX);
    IniPutKeyStringN(INI_SEC_PLAYER,"PlaylistHistoryFile",sp->PlaylistHistoryFile,FILEPATH_MAX);
    IniPutKeyStringN(INI_SEC_PLAYER,"MidiFileOpenDir",sp->MidiFileOpenDir,FILEPATH_MAX);
    IniPutKeyStringN(INI_SEC_PLAYER,"ConfigFileOpenDir",sp->ConfigFileOpenDir,FILEPATH_MAX);
    IniPutKeyStringN(INI_SEC_PLAYER,"PlaylistFileOpenDir",sp->PlaylistFileOpenDir,FILEPATH_MAX);
    IniPutKeyInt(INI_SEC_PLAYER,"PlayerThreadPriority",&(sp->PlayerThreadPriority));
    IniPutKeyInt(INI_SEC_PLAYER,"GUIThreadPriority",&(sp->GUIThreadPriority));
    IniPutKeyStringN(INI_SEC_PLAYER,"SystemFont",sp->SystemFont,LF_FULLFACESIZE + 1);
    IniPutKeyStringN(INI_SEC_PLAYER,"PlayerFont",sp->PlayerFont,LF_FULLFACESIZE + 1);
    IniPutKeyStringN(INI_SEC_PLAYER,"WrdFont",sp->WrdFont,LF_FULLFACESIZE + 1);
    IniPutKeyStringN(INI_SEC_PLAYER,"DocFont",sp->DocFont,LF_FULLFACESIZE + 1);
    IniPutKeyStringN(INI_SEC_PLAYER,"ListFont",sp->ListFont,LF_FULLFACESIZE + 1);
    IniPutKeyStringN(INI_SEC_PLAYER,"TracerFont",sp->TracerFont,LF_FULLFACESIZE + 1);
    IniPutKeyInt(INI_SEC_PLAYER,"SystemFontSize",&(sp->SystemFontSize));
    IniPutKeyInt(INI_SEC_PLAYER,"PlayerFontSize",&(sp->PlayerFontSize));
    IniPutKeyInt(INI_SEC_PLAYER,"WrdFontSize",&(sp->WrdFontSize));
    IniPutKeyInt(INI_SEC_PLAYER,"DocFontSize",&(sp->DocFontSize));
    IniPutKeyInt(INI_SEC_PLAYER,"ListFontSize",&(sp->ListFontSize));
    IniPutKeyInt(INI_SEC_PLAYER,"TracerFontSize",&(sp->TracerFontSize));
    IniPutKeyInt(INI_SEC_PLAYER,"WrdGraphicFlag",&(sp->WrdGraphicFlag));
    IniPutKeyInt(INI_SEC_PLAYER,"TraceGraphicFlag",&(sp->TraceGraphicFlag));
    IniPutKeyInt(INI_SEC_PLAYER,"DocMaxSize",&(sp->DocMaxSize));
    IniPutKeyStringN(INI_SEC_PLAYER,"DocFileExt",sp->DocFileExt,256);
    IniPutKeyInt(INI_SEC_PLAYER,"PlayerLanguage",&(sp->PlayerLanguage));
    IniPutKeyInt(INI_SEC_PLAYER,"DocWndIndependent",&(sp->DocWndIndependent));
    IniPutKeyInt(INI_SEC_PLAYER,"DocWndAutoPopup",&(sp->DocWndAutoPopup));
    IniPutKeyInt(INI_SEC_PLAYER,"SeachDirRecursive",&(sp->SeachDirRecursive));
    IniPutKeyInt(INI_SEC_PLAYER,"IniFileAutoSave",&(sp->IniFileAutoSave));
    IniPutKeyInt(INI_SEC_PLAYER,"SecondMode",&(sp->SecondMode));
    IniPutKeyInt(INI_SEC_PLAYER,"AutoloadPlaylist",&(sp->AutoloadPlaylist));
    IniPutKeyInt(INI_SEC_PLAYER,"AutosavePlaylist",&(sp->AutosavePlaylist));
    IniPutKeyInt(INI_SEC_PLAYER,"PosSizeSave",&(sp->PosSizeSave));
    IniPutKeyInt(INI_SEC_PLAYER,"main_panel_update_time",&(sp->main_panel_update_time));

    /* [TIMIDITY] */
    IniPutKeyInt32(INI_SEC_TIMIDITY,"amplification",&(st->amplification));
    IniPutKeyInt32(INI_SEC_TIMIDITY,"output_amplification",&(st->output_amplification));
    IniPutKeyInt(INI_SEC_TIMIDITY,"antialiasing_allowed",&(st->antialiasing_allowed));
    IniPutKeyInt(INI_SEC_TIMIDITY,"buffer_fragments",&(st->buffer_fragments));
    IniPutKeyInt(INI_SEC_TIMIDITY,"audio_buffer_bits",&(st->audio_buffer_bits));
///r
    IniPutKeyInt(INI_SEC_TIMIDITY,"compute_buffer_bits",&(st->compute_buffer_bits));
    IniPutKeyInt32(INI_SEC_TIMIDITY,"control_ratio",&(st->control_ratio));
///r
#if MAX_CHANNELS > 32
// ChannelBitMask uint32[8]
    IniPutKeyInt32Array(INI_SEC_TIMIDITY,"default_drumchannels",(int32*) &st->default_drumchannels, CHANNEL_ARRAY_ENABLE);
    IniPutKeyInt32Array(INI_SEC_TIMIDITY,"default_drumchannel_mask",(int32*) &st->default_drumchannel_mask, CHANNEL_ARRAY_ENABLE);
    IniPutKeyInt32Array(INI_SEC_TIMIDITY,"quietchannels",(int32*) &(st->quietchannels), CHANNEL_ARRAY_ENABLE);
#else
// ChannelBitMask uint32
    IniPutKeyInt32(INI_SEC_TIMIDITY,"default_drumchannels",(int32*) &st->default_drumchannels);
    IniPutKeyInt32(INI_SEC_TIMIDITY,"default_drumchannel_mask",(int32*) &st->default_drumchannel_mask);
    IniPutKeyInt32(INI_SEC_TIMIDITY,"quietchannels",(int32*) &(st->quietchannels));
#endif
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_modulation_wheel",&(st->opt_modulation_wheel));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_portamento",&(st->opt_portamento));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_nrpn_vibrato",&(st->opt_nrpn_vibrato));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_channel_pressure",&(st->opt_channel_pressure));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_trace_text_meta_event",&(st->opt_trace_text_meta_event));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_overlap_voice_allow",&(st->opt_overlap_voice_allow));
///r
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_overlap_voice_count",&(st->opt_overlap_voice_count));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_max_channel_voices",&(st->opt_max_channel_voices));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_default_mid",&(st->opt_default_mid));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_system_mid",&(st->opt_system_mid));
    IniPutKeyInt(INI_SEC_TIMIDITY,"default_tonebank",&(st->default_tonebank));
    IniPutKeyInt(INI_SEC_TIMIDITY,"special_tonebank",&(st->special_tonebank));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_reverb_control",&(st->opt_reverb_control));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_chorus_control",&(st->opt_chorus_control));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_surround_chorus",&(st->opt_surround_chorus));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_normal_chorus_plus",&(st->opt_normal_chorus_plus));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_tva_attack",&(st->opt_tva_attack));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_tva_decay",&(st->opt_tva_decay));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_tva_release",&(st->opt_tva_release));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_delay_control",&(st->opt_delay_control));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_default_module",&(st->opt_default_module));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_lpf_def",&(st->opt_lpf_def));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_hpf_def",&(st->opt_hpf_def));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_drum_effect",&(st->opt_drum_effect));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_modulation_envelope",&(st->opt_modulation_envelope));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_eq_control",&(st->opt_eq_control));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_insertion_effect",&(st->opt_insertion_effect));
    IniPutKeyInt(INI_SEC_TIMIDITY,"noise_sharp_type",&(st->noise_sharp_type));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_evil_mode",&(st->opt_evil_mode));
    IniPutKeyInt(INI_SEC_TIMIDITY,"adjust_panning_immediately",&(st->adjust_panning_immediately));
    IniPutKeyInt(INI_SEC_TIMIDITY,"fast_decay",&(st->opt_fast_decay));
    IniPutKeyInt(INI_SEC_TIMIDITY,"min_sustain_time",&(st->min_sustain_time));
#ifdef SUPPORT_SOUNDSPEC
    IniPutKeyInt(INI_SEC_TIMIDITY,"view_soundspec_flag",&(st->view_soundspec_flag));
    IniPutKeyFloat(INI_SEC_TIMIDITY,"spectrogram_update_sec",st->spectrogram_update_sec);
#endif
    IniPutKeyIntArray(INI_SEC_TIMIDITY,"default_program",st->default_program,MAX_CHANNELS);
//    IniPutKeyIntArray(INI_SEC_TIMIDITY,"special_program",st->special_program,MAX_CHANNELS);
    IniPutKeyStringN(INI_SEC_TIMIDITY,"opt_ctl",st->opt_ctl,sizeof(st->opt_ctl));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_print_fontname",&(st->opt_print_fontname));
    IniPutKeyInt32(INI_SEC_TIMIDITY,"opt_drum_power",&(st->opt_drum_power));
    IniPutKeyInt32(INI_SEC_TIMIDITY,"opt_amp_compensation",&(st->opt_amp_compensation));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_realtime_playing",&(st->opt_realtime_playing));
///r
    IniPutKeyStringN(INI_SEC_TIMIDITY,"reduce_voice_threshold",st->reduce_voice_threshold,sizeof(st->reduce_voice_threshold));
    IniPutKeyStringN(INI_SEC_TIMIDITY,"reduce_quality_threshold",st->reduce_quality_threshold,sizeof(st->reduce_quality_threshold));
    IniPutKeyStringN(INI_SEC_TIMIDITY,"reduce_polyphony_threshold",st->reduce_polyphony_threshold,sizeof(st->reduce_polyphony_threshold));

    IniPutKeyStringN(INI_SEC_TIMIDITY,"opt_playmode",st->opt_playmode,sizeof(st->opt_playmode));
    IniPutKeyStringN(INI_SEC_TIMIDITY,"OutputName",st->OutputName,sizeof(st->OutputName));
    IniPutKeyStringN(INI_SEC_TIMIDITY,"OutputDirName",st->OutputDirName,sizeof(st->OutputDirName));
    IniPutKeyInt(INI_SEC_TIMIDITY,"auto_output_mode",&(st->auto_output_mode));
    IniPutKeyInt(INI_SEC_TIMIDITY,"voices",&(st->voices));
    IniPutKeyInt(INI_SEC_TIMIDITY,"auto_reduce_polyphony",&(st->auto_reduce_polyphony));
    IniPutKeyInt(INI_SEC_TIMIDITY,"temper_type_mute",&(st->temper_type_mute));
#if !(defined(IA_W32G_SYN) || defined(WINDRV_SETUP))
    IniPutKeyStringN(INI_SEC_TIMIDITY,"opt_qsize",st->opt_qsize,sizeof(st->opt_qsize));
#endif
    IniPutKeyInt32(INI_SEC_TIMIDITY,"modify_release",&(st->modify_release));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_resample_type",&(st->opt_resample_type));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_resample_param",&(st->opt_resample_param));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_resample_filter",&(st->opt_resample_filter));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_resample_over_sampling",&(st->opt_resample_over_sampling));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_pre_resamplation",&(st->opt_pre_resamplation));

    IniPutKeyInt32(INI_SEC_TIMIDITY,"allocate_cache_size",&(st->allocate_cache_size));
    IniPutKeyInt(INI_SEC_TIMIDITY,"key_adjust",&(st->key_adjust));
    IniPutKeyInt8(INI_SEC_TIMIDITY,"opt_force_keysig",&(st->opt_force_keysig));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_pure_intonation",&(st->opt_pure_intonation));
    IniPutKeyInt8(INI_SEC_TIMIDITY,"opt_init_keysig",&(st->opt_init_keysig));
    IniPutKeyInt(INI_SEC_TIMIDITY,"output_rate",&(st->output_rate));
    if (st->output_rate == 0)
        st->output_rate = play_mode->rate;
    IniPutKeyStringN(INI_SEC_TIMIDITY,"output_text_code",st->output_text_code,sizeof(st->output_text_code));
///r
    IniPutKeyFloat(INI_SEC_TIMIDITY,"opt_user_volume_curve",st->opt_user_volume_curve);
    IniPutKeyInt(INI_SEC_TIMIDITY,"free_instruments_afterwards",&(st->free_instruments_afterwards));
    IniPutKeyStringN(INI_SEC_TIMIDITY,"out_wrd",st->opt_wrd,sizeof(st->opt_wrd));
///r
#ifdef AU_W32
    IniPutKeyInt32(INI_SEC_TIMIDITY,"wmme_device_id",&st->wmme_device_id);
    IniPutKeyInt(INI_SEC_TIMIDITY,"wave_format_ext",&st->wave_format_ext);
#endif
#ifdef AU_WASAPI
    IniPutKeyInt32(INI_SEC_TIMIDITY,"opt_wasapi_device_id",&st->wasapi_device_id);
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_wasapi_latency",&st->wasapi_latency);
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_wasapi_format_ext",&st->wasapi_format_ext);
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_wasapi_exclusive",&st->wasapi_exclusive);
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_wasapi_polling",&st->wasapi_polling);
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_wasapi_priority",&st->wasapi_priority);
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_wasapi_stream_category",&st->wasapi_stream_category);
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_wasapi_stream_option",&st->wasapi_stream_option);
#endif
#ifdef AU_WDMKS
    IniPutKeyInt32(INI_SEC_TIMIDITY,"opt_wdmks_device_id",&st->wdmks_device_id);
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_wdmks_latency",&st->wdmks_latency);
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_wdmks_format_ext",&st->wdmks_format_ext);
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_wdmks_polling",&st->wdmks_polling);
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_wdmks_thread_priority",&st->wdmks_thread_priority);
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_wdmks_pin_priority",&st->wdmks_pin_priority);
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_wdmks_rt_priority",&st->wdmks_rt_priority);
#endif
#ifdef AU_PORTAUDIO
    IniPutKeyInt32(INI_SEC_TIMIDITY,"pa_wmme_device_id",&st->pa_wmme_device_id);
    IniPutKeyInt32(INI_SEC_TIMIDITY,"pa_ds_device_id",&st->pa_ds_device_id);
    IniPutKeyInt32(INI_SEC_TIMIDITY,"pa_asio_device_id",&st->pa_asio_device_id);
    IniPutKeyInt32(INI_SEC_TIMIDITY,"pa_wdmks_device_id",&st->pa_wdmks_device_id);
    IniPutKeyInt32(INI_SEC_TIMIDITY,"pa_wasapi_device_id",&st->pa_wasapi_device_id);
    IniPutKeyInt(INI_SEC_TIMIDITY,"pa_wasapi_flag",&st->pa_wasapi_flag);
    IniPutKeyInt(INI_SEC_TIMIDITY,"pa_wasapi_stream_category",&st->pa_wasapi_stream_category);
    IniPutKeyInt(INI_SEC_TIMIDITY,"pa_wasapi_stream_option",&st->pa_wasapi_stream_option);
#endif

    IniPutKeyInt(INI_SEC_TIMIDITY,"add_play_time",&(st->add_play_time));
    IniPutKeyInt(INI_SEC_TIMIDITY,"add_silent_time",&(st->add_silent_time));
    IniPutKeyInt(INI_SEC_TIMIDITY,"emu_delay_time",&(st->emu_delay_time));

    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_mix_envelope",&st->opt_mix_envelope);
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_modulation_update",&(st->opt_modulation_update));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_cut_short_time",&st->opt_cut_short_time);
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_limiter",&st->opt_limiter);
    IniPutKeyInt(INI_SEC_TIMIDITY, "opt_use_midi_loop_repeat", &st->opt_use_midi_loop_repeat);
    IniPutKeyInt(INI_SEC_TIMIDITY, "opt_midi_loop_repeat", &st->opt_midi_loop_repeat);

#if defined(__W32__) && defined(SMFCONV)
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_rcpcv_dll",&(st->opt_rcpcv_dll));
#endif
//    IniPutKeyInt(INI_SEC_TIMIDITY,"data_block_bits",&(st->data_block_bits));
//    IniPutKeyInt(INI_SEC_TIMIDITY,"data_block_num",&(st->data_block_num));
//    IniPutKeyInt(INI_SEC_TIMIDITY,"wmme_buffer_bits",&(st->wmme_buffer_bits));
//    IniPutKeyInt(INI_SEC_TIMIDITY,"wmme_buffer_num",&(st->wmme_buffer_num));

#if defined(WINDRV_SETUP)
    IniPutKeyInt(INI_SEC_TIMIDITY,"processPriority",&(st->processPriority));
    IniPutKeyInt(INI_SEC_TIMIDITY,"syn_ThreadPriority",&(st->syn_ThreadPriority));
    IniPutKeyInt(INI_SEC_TIMIDITY,"SynShTime",&(st->SynShTime));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_rtsyn_latency",&(st->opt_rtsyn_latency));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_rtsyn_skip_aq",&(st->opt_rtsyn_skip_aq));
#elif defined(IA_W32G_SYN)
    IniPutKeyIntArray(INI_SEC_TIMIDITY,"SynIDPort",st->SynIDPort,MAX_PORT);
    IniPutKeyInt(INI_SEC_TIMIDITY,"syn_AutoStart",&(st->syn_AutoStart));
    IniPutKeyInt(INI_SEC_TIMIDITY,"processPriority",&(st->processPriority));
    IniPutKeyInt(INI_SEC_TIMIDITY,"syn_ThreadPriority",&(st->syn_ThreadPriority));
    IniPutKeyInt(INI_SEC_TIMIDITY,"SynPortNum",&(st->SynPortNum));
    IniPutKeyInt(INI_SEC_TIMIDITY,"SynShTime",&(st->SynShTime));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_rtsyn_latency",&(st->opt_rtsyn_latency));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_rtsyn_skip_aq",&(st->opt_rtsyn_skip_aq));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_use_twsyn_bridge",&(st->opt_use_twsyn_bridge));
#else
    IniPutKeyInt(INI_SEC_TIMIDITY,"processPriority",&(st->processPriority));
#endif
    IniPutKeyInt(INI_SEC_TIMIDITY,"compute_thread_num",&(st->compute_thread_num));

    IniPutKeyInt(INI_SEC_TIMIDITY,"trace_mode_update_time",&(st->trace_mode_update_time));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_load_all_instrument",&(st->opt_load_all_instrument));

    /* for INT_SYNTH */
    IniPutKeyInt32(INI_SEC_TIMIDITY,"opt_int_synth_sine",&(st->opt_int_synth_sine));
    IniPutKeyInt32(INI_SEC_TIMIDITY,"opt_int_synth_rate",&(st->opt_int_synth_rate));
    IniPutKeyInt32(INI_SEC_TIMIDITY,"opt_int_synth_update",&(st->opt_int_synth_update));

    IniFlush();
    w32g_has_ini_file = 1;

    OverrideSFSettingSave();
}

// When Start TiMidity in WinMain()
extern int SecondMode;
static char S_IniFile[FILEPATH_MAX];
void FirstLoadIniFile(void)
{
    const char timini[] = TIMW32_INITFILE_NAME;
    char buffer[FILEPATH_MAX];
    char *prevIniFile = IniFile;
    char *p;
    IniFile = S_IniFile;
    if (GetModuleFileNameA(GetModuleHandle(NULL), buffer, FILEPATH_MAX))
    {
        if ((p = pathsep_strrchr(buffer)) != NULL)
        {
            p++;
            *p = '\0';
        }
        else
        {
            buffer[0] = '.';
            buffer[1] = PATH_SEP;
            buffer[2] = '\0';
        }
    }
    else
    {
        buffer[0] = '.';
        buffer[1] = PATH_SEP;
        buffer[2] = '\0';
    }
    strncpy(IniFile, buffer, FILEPATH_MAX);
    IniFile[FILEPATH_MAX - 1] = '\0';
    strlcat(IniFile, timini, FILEPATH_MAX);
    IniGetKeyInt(INI_SEC_PLAYER, "SecondMode", &(SecondMode));
    IniFile = prevIniFile;
}
