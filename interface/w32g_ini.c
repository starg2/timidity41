/*

    TiMidity -- Experimental MIDI to WAVE converter
    Copyright (C) 1999 Masanao Izumo <mo@goice.co.jp>
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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    w32g_ini.c: written by Daisuke Aoki <dai@y7.net>
                           Masanao Izumo <mo@goice.co.jp>
*/

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include "timidity.h"
#include "common.h"
#include "output.h"
#include "instrum.h"
#include "playmidi.h"
#include "w32g.h"
#include "w32g_utl.h"

#if MAX_CHANNELS > 32 /* FIXME */
#error "MAX_CHANNELS > 32 is not supported Windows GUI version"
#endif

int w32g_has_ini_file;

static int32 str2size(char *str)
{
    int len = strlen(str);
    if(str[len - 1] == 'k' || str[len - 1] == 'K')
	return (int32)(1024.0 * atof(str));
    if(str[len - 1] == 'm' || str[len - 1] == 'M')
	return (int32)(1024 * 1024 * atof(str));
    return atoi(str);
}

void LoadIniFile(SETTING_PLAYER *sp,  SETTING_TIMIDITY *st)
{
    char buff[1024];

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
    IniGetKeyStringN(INI_SEC_PLAYER,"ConfigFile",sp->ConfigFile,MAXPATH + 32);
    if(!sp->ConfigFile[0])
	strcpy(sp->ConfigFile, W32G_TIMIDITY_CFG);
    IniGetKeyStringN(INI_SEC_PLAYER,"PlaylistFile",sp->PlaylistFile,MAXPATH + 32);
    IniGetKeyStringN(INI_SEC_PLAYER,"PlaylistHistoryFile",sp->PlaylistHistoryFile,MAXPATH + 32);
    IniGetKeyStringN(INI_SEC_PLAYER,"MidiFileOpenDir",sp->MidiFileOpenDir,MAXPATH + 32);
    IniGetKeyStringN(INI_SEC_PLAYER,"ConfigFileOpenDir",sp->ConfigFileOpenDir,MAXPATH + 32);
    IniGetKeyStringN(INI_SEC_PLAYER,"PlaylistFileOpenDir",sp->PlaylistFileOpenDir,MAXPATH + 32);
    IniGetKeyInt(INI_SEC_PLAYER,"PlayerThreadPriority",&(sp->PlayerThreadPriority));
    IniGetKeyInt(INI_SEC_PLAYER,"GUIThreadPriority",&(sp->GUIThreadPriority));
    IniGetKeyStringN(INI_SEC_PLAYER,"SystemFont",sp->SystemFont,256);
    IniGetKeyStringN(INI_SEC_PLAYER,"PlayerFont",sp->PlayerFont,256);
    IniGetKeyStringN(INI_SEC_PLAYER,"WrdFont",sp->WrdFont,256);
    IniGetKeyStringN(INI_SEC_PLAYER,"DocFont",sp->DocFont,256);
    IniGetKeyStringN(INI_SEC_PLAYER,"ListFont",sp->ListFont,256);
    IniGetKeyStringN(INI_SEC_PLAYER,"TracerFont",sp->TracerFont,256);
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
    IniGetKeyInt(INI_SEC_PLAYER,"SeachDirRecursive",&(sp->SeachDirRecursive));
    IniGetKeyInt(INI_SEC_PLAYER,"IniFileAutoSave",&(sp->IniFileAutoSave));

    /* [TIMIDITY] */
    IniGetKeyInt32(INI_SEC_TIMIDITY,"amplification",&(st->amplification));
    IniGetKeyInt(INI_SEC_TIMIDITY,"antialiasing_allowed",&(st->antialiasing_allowed));
    IniGetKeyInt(INI_SEC_TIMIDITY,"buffer_fragments",&(st->buffer_fragments));
    IniGetKeyInt32(INI_SEC_TIMIDITY,"control_ratio",&(st->control_ratio));
    IniGetKeyInt32(INI_SEC_TIMIDITY,"default_drumchannels",(int32 *)&(st->default_drumchannels));
    IniGetKeyInt32(INI_SEC_TIMIDITY,"default_drumchannel_mask",(int32 *)&(st->default_drumchannel_mask));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_modulation_wheel",&(st->opt_modulation_wheel));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_portamento",&(st->opt_portamento));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_nrpn_vibrato",&(st->opt_nrpn_vibrato));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_channel_pressure",&(st->opt_channel_pressure));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_trace_text_meta_event",&(st->opt_trace_text_meta_event));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_overlap_voice_allow",&(st->opt_overlap_voice_allow));
    IniGetKeyStringN(INI_SEC_TIMIDITY,"opt_default_mid",buff,sizeof(buff));
    st->opt_default_mid = str2mID(buff);
    IniGetKeyInt(INI_SEC_TIMIDITY,"default_tonebank",&(st->default_tonebank));
    IniGetKeyInt(INI_SEC_TIMIDITY,"special_tonebank",&(st->special_tonebank));
    IniGetKeyInt(INI_SEC_TIMIDITY,"effect_lr_mode",&(st->effect_lr_mode));
    IniGetKeyInt(INI_SEC_TIMIDITY,"effect_lr_delay_msec",&(st->effect_lr_delay_msec));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_reverb_control",&(st->opt_reverb_control));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_chorus_control",&(st->opt_chorus_control));
    IniGetKeyInt(INI_SEC_TIMIDITY,"noise_sharp_type",&(st->noise_sharp_type));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_evil_mode",&(st->opt_evil_mode));
    IniGetKeyInt(INI_SEC_TIMIDITY,"adjust_panning_immediately",&(st->adjust_panning_immediately));
    IniGetKeyInt(INI_SEC_TIMIDITY,"fast_decay",&(st->fast_decay));
#ifdef SUPPORT_SOUNDSPEC
    IniGetKeyInt(INI_SEC_TIMIDITY,"view_soundspec_flag",&(st->view_soundspec_flag));
    IniGetKeyFloat(INI_SEC_TIMIDITY,"spectrogram_update_sec",&v_float);
    st->spectrogram_update_sec = v_float;
#endif
    IniGetKeyIntArray(INI_SEC_TIMIDITY,"default_program",st->default_program,MAX_CHANNELS);
    IniGetKeyStringN(INI_SEC_TIMIDITY,"opt_ctl",st->opt_ctl,sizeof(st->opt_ctl));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_realtime_playing",&(st->opt_realtime_playing));
    IniGetKeyInt(INI_SEC_TIMIDITY,"reduce_voice_threshold",&(st->reduce_voice_threshold));
    IniGetKeyStringN(INI_SEC_TIMIDITY,"opt_playmode",st->opt_playmode,sizeof(st->opt_playmode));
    IniGetKeyStringN(INI_SEC_TIMIDITY,"OutputName",st->OutputName,sizeof(st->OutputName));
    IniGetKeyInt(INI_SEC_TIMIDITY,"voices",&(st->voices));
    IniGetKeyInt(INI_SEC_TIMIDITY,"auto_reduce_polyphony",&(st->auto_reduce_polyphony));
    IniGetKeyInt32(INI_SEC_TIMIDITY,"quietchannels",(int32 *)&(st->quietchannels));
    IniGetKeyStringN(INI_SEC_TIMIDITY,"opt_qsize",st->opt_qsize,sizeof(st->opt_qsize));
    IniGetKeyInt32(INI_SEC_TIMIDITY,"modify_release",&(st->modify_release));
    IniGetKeyStringN(INI_SEC_TIMIDITY,"allocate_cache_size",buff,sizeof(buff));
    st->allocate_cache_size = str2size(buff);
    IniGetKeyInt(INI_SEC_TIMIDITY,"output_rate",&(st->output_rate));
    IniGetKeyStringN(INI_SEC_TIMIDITY,"output_text_code",st->output_text_code,sizeof(st->output_text_code));
    IniGetKeyInt(INI_SEC_TIMIDITY,"free_instruments_afterwards",&(st->free_instruments_afterwards));
    IniGetKeyStringN(INI_SEC_TIMIDITY,"out_wrd",st->opt_wrd,sizeof(st->opt_wrd));
#if defined(__W32__) && defined(SMFCONV)
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_rcpcv_dll",&(st->opt_rcpcv_dll));
#endif
    IniGetKeyInt(INI_SEC_TIMIDITY,"data_block_time",&(st->data_block_time));
    IniGetKeyInt(INI_SEC_TIMIDITY,"data_block_num",&(st->data_block_num));
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
    IniPutKeyStringN(INI_SEC_PLAYER,"ConfigFile",sp->ConfigFile,MAXPATH + 32);
    IniPutKeyStringN(INI_SEC_PLAYER,"PlaylistFile",sp->PlaylistFile,MAXPATH + 32);
    IniPutKeyStringN(INI_SEC_PLAYER,"PlaylistHistoryFile",sp->PlaylistHistoryFile,MAXPATH + 32);
    IniPutKeyStringN(INI_SEC_PLAYER,"MidiFileOpenDir",sp->MidiFileOpenDir,MAXPATH + 32);
    IniPutKeyStringN(INI_SEC_PLAYER,"ConfigFileOpenDir",sp->ConfigFileOpenDir,MAXPATH + 32);
    IniPutKeyStringN(INI_SEC_PLAYER,"PlaylistFileOpenDir",sp->PlaylistFileOpenDir,MAXPATH + 32);
    IniPutKeyInt(INI_SEC_PLAYER,"PlayerThreadPriority",&(sp->PlayerThreadPriority));
    IniPutKeyInt(INI_SEC_PLAYER,"GUIThreadPriority",&(sp->GUIThreadPriority));
    IniPutKeyStringN(INI_SEC_PLAYER,"SystemFont",sp->SystemFont,256);
    IniPutKeyStringN(INI_SEC_PLAYER,"PlayerFont",sp->PlayerFont,256);
    IniPutKeyStringN(INI_SEC_PLAYER,"WrdFont",sp->WrdFont,256);
    IniPutKeyStringN(INI_SEC_PLAYER,"DocFont",sp->DocFont,256);
    IniPutKeyStringN(INI_SEC_PLAYER,"ListFont",sp->ListFont,256);
    IniPutKeyStringN(INI_SEC_PLAYER,"TracerFont",sp->TracerFont,256);
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
    IniPutKeyInt(INI_SEC_PLAYER,"SeachDirRecursive",&(sp->SeachDirRecursive));
    IniPutKeyInt(INI_SEC_PLAYER,"IniFileAutoSave",&(sp->IniFileAutoSave));


    /* [TIMIDITY] */
    IniPutKeyInt32(INI_SEC_TIMIDITY,"amplification",&(st->amplification));
    IniPutKeyInt(INI_SEC_TIMIDITY,"antialiasing_allowed",&(st->antialiasing_allowed));
    IniPutKeyInt(INI_SEC_TIMIDITY,"buffer_fragments",&(st->buffer_fragments));
    IniPutKeyInt32(INI_SEC_TIMIDITY,"control_ratio",&(st->control_ratio));
    IniPutKeyInt32(INI_SEC_TIMIDITY,"default_drumchannels",(int32 *)&(st->default_drumchannels));
    IniPutKeyInt32(INI_SEC_TIMIDITY,"default_drumchannel_mask",(int32 *)&(st->default_drumchannel_mask));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_modulation_wheel",&(st->opt_modulation_wheel));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_portamento",&(st->opt_portamento));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_nrpn_vibrato",&(st->opt_nrpn_vibrato));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_channel_pressure",&(st->opt_channel_pressure));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_trace_text_meta_event",&(st->opt_trace_text_meta_event));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_overlap_voice_allow",&(st->opt_overlap_voice_allow));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_default_mid",&(st->opt_default_mid));
    IniPutKeyInt(INI_SEC_TIMIDITY,"default_tonebank",&(st->default_tonebank));
    IniPutKeyInt(INI_SEC_TIMIDITY,"special_tonebank",&(st->special_tonebank));
    IniPutKeyInt(INI_SEC_TIMIDITY,"effect_lr_mode",&(st->effect_lr_mode));
    IniPutKeyInt(INI_SEC_TIMIDITY,"effect_lr_delay_msec",&(st->effect_lr_delay_msec));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_reverb_control",&(st->opt_reverb_control));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_chorus_control",&(st->opt_chorus_control));
    IniPutKeyInt(INI_SEC_TIMIDITY,"noise_sharp_type",&(st->noise_sharp_type));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_evil_mode",&(st->opt_evil_mode));
    IniPutKeyInt(INI_SEC_TIMIDITY,"adjust_panning_immediately",&(st->adjust_panning_immediately));
    IniPutKeyInt(INI_SEC_TIMIDITY,"fast_decay",&(st->fast_decay));
#ifdef SUPPORT_SOUNDSPEC
    IniPutKeyInt(INI_SEC_TIMIDITY,"view_soundspec_flag",&(st->view_soundspec_flag));
    v_float = st->spectrogram_update_sec;
    IniPutKeyFloat(INI_SEC_TIMIDITY,"spectrogram_update_sec",&v_float);
#endif
    IniPutKeyIntArray(INI_SEC_TIMIDITY,"default_program",st->default_program,MAX_CHANNELS);
    IniPutKeyStringN(INI_SEC_TIMIDITY,"opt_ctl",st->opt_ctl,sizeof(st->opt_ctl));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_realtime_playing",&(st->opt_realtime_playing));
    IniPutKeyInt(INI_SEC_TIMIDITY,"reduce_voice_threshold",&(st->reduce_voice_threshold));
    IniPutKeyStringN(INI_SEC_TIMIDITY,"opt_playmode",st->opt_playmode,sizeof(st->opt_playmode));
    IniPutKeyStringN(INI_SEC_TIMIDITY,"OutputName",st->OutputName,sizeof(st->OutputName));
    IniPutKeyInt(INI_SEC_TIMIDITY,"voices",&(st->voices));
    IniPutKeyInt(INI_SEC_TIMIDITY,"auto_reduce_polyphony",&(st->auto_reduce_polyphony));
    IniPutKeyInt32(INI_SEC_TIMIDITY,"quietchannels",(int32 *)&(st->quietchannels));
    IniPutKeyStringN(INI_SEC_TIMIDITY,"opt_qsize",st->opt_qsize,sizeof(st->opt_qsize));
    IniPutKeyInt32(INI_SEC_TIMIDITY,"modify_release",&(st->modify_release));
    IniPutKeyInt32(INI_SEC_TIMIDITY,"allocate_cache_size",&(st->allocate_cache_size));
    IniPutKeyInt(INI_SEC_TIMIDITY,"output_rate",&(st->output_rate));
    if(st->output_rate == 0)
	st->output_rate = play_mode->rate;
    IniPutKeyStringN(INI_SEC_TIMIDITY,"output_text_code",st->output_text_code,sizeof(st->output_text_code));
    IniPutKeyInt(INI_SEC_TIMIDITY,"free_instruments_afterwards",&(st->free_instruments_afterwards));
    IniPutKeyStringN(INI_SEC_TIMIDITY,"out_wrd",st->opt_wrd,sizeof(st->opt_wrd));
#if defined(__W32__) && defined(SMFCONV)
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_rcpcv_dll",&(st->opt_rcpcv_dll));
#endif
    IniPutKeyInt(INI_SEC_TIMIDITY,"data_block_time",&(st->data_block_time));
    IniPutKeyInt(INI_SEC_TIMIDITY,"data_block_num",&(st->data_block_num));
    w32g_has_ini_file = 1;
}
