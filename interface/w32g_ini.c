#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include "timidity.h"
#include "w32g.h"
#include "w32g_utl.h"

void LoadIniFile(SETTING_PLAYER *sp,  SETTING_TIMIDITY *st)
{
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
    IniGetKeyStringN(INI_SEC_PLAYER,"PlaylistFile",sp->PlaylistFile,MAXPATH + 32);
    IniGetKeyStringN(INI_SEC_PLAYER,"PlaylistHistoryFile",sp->PlaylistHistoryFile,MAXPATH + 32);
    IniGetKeyStringN(INI_SEC_PLAYER,"MidiFileOpenDir",sp->MidiFileOpenDir,MAXPATH + 32);
    IniGetKeyStringN(INI_SEC_PLAYER,"ConfigFileOpenDir",sp->ConfigFileOpenDir,MAXPATH + 32);
    IniGetKeyStringN(INI_SEC_PLAYER,"PlaylistFileOpenDir",sp->PlaylistFileOpenDir,MAXPATH + 32);
    IniGetKeyInt(INI_SEC_PLAYER,"ProcessPriority",&(sp->ProcessPriority));
    IniGetKeyInt(INI_SEC_PLAYER,"PlayerThreadPriority",&(sp->PlayerThreadPriority));
    IniGetKeyInt(INI_SEC_PLAYER,"MidiPlayerThreadPriority",&(sp->MidiPlayerThreadPriority));
    IniGetKeyInt(INI_SEC_PLAYER,"MainThreadPriority",&(sp->MainThreadPriority));
    IniGetKeyInt(INI_SEC_PLAYER,"TracerThreadPriority",&(sp->TracerThreadPriority));
    IniGetKeyInt(INI_SEC_PLAYER,"WrdThreadPriority",&(sp->WrdThreadPriority));
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
    
    IniGetKeyInt(INI_SEC_TIMIDITY,"free_instruments_afterwards",&(st->free_instruments_afterwards));
    IniGetKeyInt(INI_SEC_TIMIDITY,"effect_lr_mode",&(st->effect_lr_mode));
    IniGetKeyInt(INI_SEC_TIMIDITY,"effect_lr_delay_msec",&(st->effect_lr_delay_msec));

#if MAX_CHANNELS <= 32
    IniGetKeyInt32(INI_SEC_TIMIDITY,"quietchannels",(int32 *)&(st->quietchannels));
    IniGetKeyInt32(INI_SEC_TIMIDITY,"drumchannels",(int32 *)&(st->drumchannels));
    IniGetKeyInt32(INI_SEC_TIMIDITY,"drumchannel_mask",(int32 *)&(st->drumchannel_mask));
    IniGetKeyInt32(INI_SEC_TIMIDITY,"default_drumchannels",(int32 *)&(st->default_drumchannels));
    IniGetKeyInt32(INI_SEC_TIMIDITY,"default_drumchannel_mask",(int32 *)&(st->default_drumchannel_mask));
#else /* FIXME */
#error "MAX_CHANNELS > 32 is not supported Windows GUI version"
#endif


    IniGetKeyChar(INI_SEC_TIMIDITY,"play_mode_id_character",&(st->play_mode_id_character));
    IniGetKeyInt(INI_SEC_TIMIDITY,"play_mode_encoding",&(st->play_mode_encoding));
    IniGetKeyChar(INI_SEC_TIMIDITY,"pmp_id_character",&(st->pmp_id_character));
    IniGetKeyStringN(INI_SEC_TIMIDITY,"OutputName",st->OutputName,MAXPATH + 32);
    IniGetKeyInt(INI_SEC_TIMIDITY,"antialiasing_allowed",&(st->antialiasing_allowed));
    IniGetKeyInt(INI_SEC_TIMIDITY,"fast_decay",&(st->fast_decay));
    IniGetKeyInt(INI_SEC_TIMIDITY,"adjust_panning_immediately",&(st->adjust_panning_immediately));
    IniGetKeyInt(INI_SEC_TIMIDITY,"output_rate",&(st->output_rate));
    IniGetKeyIntArray(INI_SEC_TIMIDITY,"default_program",st->default_program,MAX_CHANNELS);
    IniGetKeyInt32(INI_SEC_TIMIDITY,"amplification",&(st->amplification));
    IniGetKeyInt32(INI_SEC_TIMIDITY,"control_ratio",&(st->control_ratio));
    IniGetKeyInt(INI_SEC_TIMIDITY,"voices",&(st->voices));
    IniGetKeyInt32(INI_SEC_TIMIDITY,"modify_release",&(st->modify_release));
    IniGetKeyInt(INI_SEC_TIMIDITY,"view_soundspec_flag",&(st->view_soundspec_flag));
    IniGetKeyInt32(INI_SEC_TIMIDITY,"allocate_cache_size",&(st->allocate_cache_size));
    IniGetKeyStringN(INI_SEC_TIMIDITY,"allocate_chace_size_string",st->allocate_chace_size_string,64);
    IniGetKeyInt(INI_SEC_TIMIDITY,"ctl_verbosity",&(st->ctl_verbosity));
    IniGetKeyChar(INI_SEC_TIMIDITY,"ctl_id_character",&(st->ctl_id_character));
    IniGetKeyInt(INI_SEC_TIMIDITY,"ctl_trace_playing",&(st->ctl_trace_playing));
    IniGetKeyInt(INI_SEC_TIMIDITY,"buffer_fragments",&(st->buffer_fragments));
    IniGetKeyStringN(INI_SEC_TIMIDITY,"output_text_code",st->output_text_code,64);
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_modulation_wheel",&(st->opt_modulation_wheel));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_portamento",&(st->opt_portamento));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_nrpn_vibrato",&(st->opt_nrpn_vibrato));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_reverb_control",&(st->opt_reverb_control));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_chorus_control",&(st->opt_chorus_control));
    IniGetKeyIntArray(INI_SEC_TIMIDITY,"opt_chorus_control2",st->opt_chorus_control2,MAX_CHANNELS);
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_channel_pressure",&(st->opt_channel_pressure));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_trace_text_meta_event",&(st->opt_trace_text_meta_event));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_overlap_voice_allow",&(st->opt_overlap_voice_allow));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_default_mid",&(st->opt_default_mid));
    IniGetKeyIntArray(INI_SEC_TIMIDITY,"opt_default_mid_str",st->opt_default_mid_str,64);
    IniGetKeyInt(INI_SEC_TIMIDITY,"default_tonebank",&(st->default_tonebank));
    IniGetKeyInt(INI_SEC_TIMIDITY,"special_tonebank",&(st->special_tonebank));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_realtime_playing",&(st->opt_realtime_playing));
    IniGetKeyInt(INI_SEC_TIMIDITY,"opt_rcpcv_dll",&(st->opt_rcpcv_dll));
    IniGetKeyChar(INI_SEC_TIMIDITY,"wrdt_id",&(st->wrdt_id));
    IniGetKeyStringN(INI_SEC_TIMIDITY,"wrdt_open_opts",st->wrdt_open_opts,64);
    IniGetKeyInt(INI_SEC_TIMIDITY,"noise_sharp_type",&(st->noise_sharp_type));
}

void
SaveIniFile(SETTING_PLAYER *sp,  SETTING_TIMIDITY *st)
{
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
    IniPutKeyInt(INI_SEC_PLAYER,"ProcessPriority",&(sp->ProcessPriority));
    IniPutKeyInt(INI_SEC_PLAYER,"PlayerThreadPriority",&(sp->PlayerThreadPriority));
    IniPutKeyInt(INI_SEC_PLAYER,"MidiPlayerThreadPriority",&(sp->MidiPlayerThreadPriority));
    IniPutKeyInt(INI_SEC_PLAYER,"MainThreadPriority",&(sp->MainThreadPriority));
    IniPutKeyInt(INI_SEC_PLAYER,"TracerThreadPriority",&(sp->TracerThreadPriority));
    IniPutKeyInt(INI_SEC_PLAYER,"WrdThreadPriority",&(sp->WrdThreadPriority));
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
    
    IniPutKeyInt(INI_SEC_TIMIDITY,"free_instruments_afterwards",&(st->free_instruments_afterwards));

    IniPutKeyInt(INI_SEC_TIMIDITY,"effect_lr_mode",&(st->effect_lr_mode));
    IniPutKeyInt(INI_SEC_TIMIDITY,"effect_lr_delay_msec",&(st->effect_lr_delay_msec));


#if MAX_CHANNELS <= 32
    IniPutKeyInt32(INI_SEC_TIMIDITY,"quietchannels",(int32 *)&(st->quietchannels));
    IniPutKeyInt32(INI_SEC_TIMIDITY,"drumchannels",(int32 *)&(st->drumchannels));
    IniPutKeyInt32(INI_SEC_TIMIDITY,"drumchannel_mask",(int32 *)&(st->drumchannel_mask));
    IniPutKeyInt32(INI_SEC_TIMIDITY,"default_drumchannels",(int32 *)&(st->default_drumchannels));
    IniPutKeyInt32(INI_SEC_TIMIDITY,"default_drumchannel_mask",(int32 *)&(st->default_drumchannel_mask));
#else /* FIXME */
#error "MAX_CHANNELS > 32 is not supported Windows GUI version"
#endif

    IniPutKeyChar(INI_SEC_TIMIDITY,"play_mode_id_character",&(st->play_mode_id_character));
    IniPutKeyInt(INI_SEC_TIMIDITY,"play_mode_encoding",&(st->play_mode_encoding));
    IniPutKeyChar(INI_SEC_TIMIDITY,"pmp_id_character",&(st->pmp_id_character));
    IniPutKeyStringN(INI_SEC_TIMIDITY,"OutputName",st->OutputName,MAXPATH + 32);
    IniPutKeyInt(INI_SEC_TIMIDITY,"antialiasing_allowed",&(st->antialiasing_allowed));
    IniPutKeyInt(INI_SEC_TIMIDITY,"fast_decay",&(st->fast_decay));
    IniPutKeyInt(INI_SEC_TIMIDITY,"adjust_panning_immediately",&(st->adjust_panning_immediately));
    IniPutKeyInt(INI_SEC_TIMIDITY,"output_rate",&(st->output_rate));
    IniPutKeyIntArray(INI_SEC_TIMIDITY,"default_program",st->default_program,MAX_CHANNELS);
    IniPutKeyInt32(INI_SEC_TIMIDITY,"amplification",&(st->amplification));
    IniPutKeyInt32(INI_SEC_TIMIDITY,"control_ratio",&(st->control_ratio));
    IniPutKeyInt(INI_SEC_TIMIDITY,"voices",&(st->voices));
    IniPutKeyInt32(INI_SEC_TIMIDITY,"modify_release",&(st->modify_release));
    IniPutKeyInt(INI_SEC_TIMIDITY,"view_soundspec_flag",&(st->view_soundspec_flag));
    IniPutKeyInt32(INI_SEC_TIMIDITY,"allocate_cache_size",&(st->allocate_cache_size));
    IniPutKeyStringN(INI_SEC_TIMIDITY,"allocate_chace_size_string",st->allocate_chace_size_string,64);
    IniPutKeyInt(INI_SEC_TIMIDITY,"ctl_verbosity",&(st->ctl_verbosity));
    IniPutKeyChar(INI_SEC_TIMIDITY,"ctl_id_character",&(st->ctl_id_character));
    IniPutKeyInt(INI_SEC_TIMIDITY,"ctl_trace_playing",&(st->ctl_trace_playing));
    IniPutKeyInt(INI_SEC_TIMIDITY,"buffer_fragments",&(st->buffer_fragments));
    IniPutKeyStringN(INI_SEC_TIMIDITY,"output_text_code",st->output_text_code,64);
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_modulation_wheel",&(st->opt_modulation_wheel));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_portamento",&(st->opt_portamento));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_nrpn_vibrato",&(st->opt_nrpn_vibrato));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_reverb_control",&(st->opt_reverb_control));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_chorus_control",&(st->opt_chorus_control));
    IniPutKeyIntArray(INI_SEC_TIMIDITY,"opt_chorus_control2",st->opt_chorus_control2,MAX_CHANNELS);
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_channel_pressure",&(st->opt_channel_pressure));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_trace_text_meta_event",&(st->opt_trace_text_meta_event));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_overlap_voice_allow",&(st->opt_overlap_voice_allow));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_default_mid",&(st->opt_default_mid));
    IniPutKeyIntArray(INI_SEC_TIMIDITY,"opt_default_mid_str",st->opt_default_mid_str,64);
    IniPutKeyInt(INI_SEC_TIMIDITY,"default_tonebank",&(st->default_tonebank));
    IniPutKeyInt(INI_SEC_TIMIDITY,"special_tonebank",&(st->special_tonebank));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_realtime_playing",&(st->opt_realtime_playing));
    IniPutKeyInt(INI_SEC_TIMIDITY,"opt_rcpcv_dll",&(st->opt_rcpcv_dll));
    IniPutKeyChar(INI_SEC_TIMIDITY,"wrdt_id",&(st->wrdt_id));
    IniPutKeyStringN(INI_SEC_TIMIDITY,"wrdt_open_opts",st->wrdt_open_opts,64);
    IniPutKeyInt(INI_SEC_TIMIDITY,"noise_sharp_type",&(st->noise_sharp_type));
}

