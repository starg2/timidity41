
	memset(&OverrideSample, 0, sizeof OverrideSample);
	MyIni_Load(&ini, fn);
	sec = MyIni_GetSection(&ini, "param", 0);
	
#ifdef MyIniParamRange
#undef MyIniParamRange
#endif
#define MyIniParamRange(MIPR_VAL, MIPR_MIN, MIPR_MAX) ((MIPR_VAL < MIPR_MIN) ? (MIPR_MIN) : ((MIPR_VAL > MIPR_MAX) ? (MIPR_MAX) : (MIPR_VAL)))

	OverrideSample.vibrato_delay = MyIni_GetInt32(sec, "VibratoDelay", 300);
	OverrideSample.vibrato_delay = MyIniParamRange(OverrideSample.vibrato_delay, 0, 2000);
	OverrideSample.vibrato_to_pitch = MyIni_GetInt32(sec, "VibratoDepth", 0);
	OverrideSample.vibrato_to_pitch = MyIniParamRange(OverrideSample.vibrato_to_pitch, 0, 600);
	OverrideSample.vibrato_sweep = MyIni_GetInt32(sec, "VibratoSweep", 0);
	OverrideSample.vibrato_sweep = MyIniParamRange(OverrideSample.vibrato_sweep, 0, 255);

	OverrideSample.vel_to_fc = MyIni_GetInt32(sec, "VelToFc", -2400);
	OverrideSample.vel_to_fc = MyIniParamRange(OverrideSample.vel_to_fc, -10000, 10000);
	OverrideSample.vel_to_fc_threshold = MyIni_GetInt32(sec, "VelToFcThr", 0);
	OverrideSample.vel_to_fc_threshold = MyIniParamRange(OverrideSample.vel_to_fc_threshold, 0, 127);
	OverrideSample.vel_to_resonance = MyIni_GetInt32(sec, "VelToRes", 0);
	OverrideSample.vel_to_resonance = MyIniParamRange(OverrideSample.vel_to_resonance, -100, 100);

	OverrideSample.tremolo_delay = MyIni_GetInt32(sec, "TremoloDelay", 0);
	OverrideSample.tremolo_delay = MyIniParamRange(OverrideSample.tremolo_delay, 0, 1000);
	OverrideSample.tremolo_to_amp = MyIni_GetInt32(sec, "TremoloDepth", 0);
	OverrideSample.tremolo_to_amp = MyIniParamRange(OverrideSample.tremolo_to_amp, 0, 256);
	OverrideSample.tremolo_to_fc = MyIni_GetInt32(sec, "TremoloToFc", 0);
	OverrideSample.tremolo_to_fc = MyIniParamRange(OverrideSample.tremolo_to_fc, -12000, 12000);
	OverrideSample.tremolo_to_pitch = MyIni_GetInt32(sec, "TremoloToPitch", 0);
	OverrideSample.tremolo_to_pitch = MyIniParamRange(OverrideSample.tremolo_to_pitch, -12000, 12000);

	OverrideSample.resonance = MyIni_GetInt32(sec, "Res", 0);
	OverrideSample.resonance = MyIniParamRange(OverrideSample.resonance, -200, 200);
	OverrideSample.cutoff_freq = MyIni_GetInt32(sec, "CutoffFreq", 10000);
	OverrideSample.cutoff_freq = MyIniParamRange(OverrideSample.cutoff_freq, 0, 20000);

	OverrideSample.envelope_delay = MyIni_GetInt32(sec, "EnvDelay", 1);
	OverrideSample.envelope_delay = MyIniParamRange(OverrideSample.envelope_delay, 1, 1000);

	OverrideSample.modenv_delay = MyIni_GetInt32(sec, "ModEnvDelay", 1);
	OverrideSample.modenv_delay = MyIniParamRange(OverrideSample.modenv_delay, 1, 2000);
	OverrideSample.modenv_to_fc = MyIni_GetInt32(sec, "ModEnvToFc", 100);
	OverrideSample.modenv_to_fc = MyIniParamRange(OverrideSample.modenv_to_fc, -12000, 12000);
	OverrideSample.modenv_to_pitch = MyIni_GetInt32(sec, "ModEnvToPitch", 0);
	OverrideSample.modenv_to_pitch = MyIniParamRange(OverrideSample.modenv_to_pitch, -12000, 12000);
	
	sf_attenuation_neg = MyIni_GetInt8(sec, "Attenuation_Neg", 0);
	sf_attenuation_neg = MyIniParamRange(sf_attenuation_neg, 0, 1); // 0 or 1
	sf_attenuation_pow = MyIni_GetFloat64(sec, "Attenuation_Pow", 10.0);
	sf_attenuation_pow = MyIniParamRange(sf_attenuation_pow, 0, 20.0); // 0 ~ 20.0
	sf_attenuation_mul = MyIni_GetFloat64(sec, "Attenuation_Mul", 0.002);
	sf_attenuation_mul = MyIniParamRange(sf_attenuation_mul, 0, 4.0); // 0 ~ 4.0
	sf_attenuation_add = MyIni_GetFloat64(sec, "Attenuation_Add", 0);
	sf_attenuation_add = MyIniParamRange(sf_attenuation_add, -1440.0, 1440.0); // -1440.0 ~ 1440.0
		
	sf_limit_volenv_attack = MyIni_GetInt32(sec, "Limit_VolEnv_Attack", 6); // 1ms
	sf_limit_volenv_attack = MyIniParamRange(sf_limit_volenv_attack, 0, 10); // 0 ~ 10
	sf_limit_modenv_attack = MyIni_GetInt32(sec, "Limit_ModEnv_Attack", 6); // 1ms
	sf_limit_modenv_attack = MyIniParamRange(sf_limit_modenv_attack, 0, 10); // 0 ~ 10
	sf_limit_modenv_fc = MyIni_GetInt32(sec, "Limit_ModEnv_Fc", 12000);
	sf_limit_modenv_fc = MyIniParamRange(sf_limit_modenv_fc, 0, 12000); // 0 ~ 12000
	sf_limit_modenv_pitch = MyIni_GetInt32(sec, "Limit_ModEnv_Pitch", 12000);
	sf_limit_modenv_pitch = MyIniParamRange(sf_limit_modenv_pitch, 0, 12000); // 0 ~ 12000
	sf_limit_modlfo_fc = MyIni_GetInt32(sec, "Limit_ModLfo_Fc", 12000);
	sf_limit_modlfo_fc = MyIniParamRange(sf_limit_modlfo_fc, 0, 12000); // 0 ~ 12000
	sf_limit_modlfo_pitch = MyIni_GetInt32(sec, "Limit_ModLfo_Pitch", 12000);
	sf_limit_modlfo_pitch = MyIniParamRange(sf_limit_modlfo_pitch, 0, 12000); // 0 ~ 12000
	sf_limit_viblfo_pitch = MyIni_GetInt32(sec, "Limit_VibLfo_Pitch", 12000);
	sf_limit_viblfo_pitch = MyIniParamRange(sf_limit_viblfo_pitch, 0, 12000); // 0 ~ 12000
	sf_limit_modlfo_freq = MyIni_GetInt32(sec, "Limit_ModLfo_Freq", 100000);
	sf_limit_modlfo_freq = MyIniParamRange(sf_limit_modlfo_freq, 1, 100000); // 1 ~ 100000
	sf_limit_viblfo_freq = MyIni_GetInt32(sec, "Limit_VibLfo_Freq", 100000);
	sf_limit_viblfo_freq = MyIniParamRange(sf_limit_viblfo_freq, 1, 100000); // 1 ~ 100000
		
	sf_default_modlfo_freq = MyIni_GetInt32(sec, "Default_ModLfo_Freq", 8176);
	sf_default_modlfo_freq = MyIniParamRange(sf_default_modlfo_freq, 1, 100000); // 1 ~ 100000
	sf_default_viblfo_freq = MyIni_GetInt32(sec, "Default_VibLfo_Freq", 8176);
	sf_default_viblfo_freq = MyIniParamRange(sf_default_viblfo_freq, 1, 100000); // 1 ~ 100000

	sf_config_lfo_swap = MyIni_GetInt8(sec, "Config_LFO_Swap", 0);
	sf_config_lfo_swap = MyIniParamRange(sf_config_lfo_swap, 0, 1); // 0 or 1
	sf_config_addrs_offset = MyIni_GetInt8(sec, "Config_Addrs_Offset", 0);
	sf_config_addrs_offset = MyIniParamRange(sf_config_addrs_offset, 0, 1); // 0 or 1

	otd.chorus_send = MyIni_GetInt8(sec, "_SF2ChorusSend", 0);
	otd.chorus_send = MyIniParamRange(otd.chorus_send, 0, 127);
	otd.reverb_send = MyIni_GetInt8(sec, "_SF2ReverbSend", 0);
	otd.reverb_send = MyIniParamRange(otd.reverb_send, 0, 127);
	
	otd.vibrato_rate = MyIni_GetInt32(sec, "_VibratoRate", 0); // cHz
	otd.vibrato_rate = MyIniParamRange(otd.vibrato_rate, 0, 2000.0);
	otd.vibrato_cent = MyIni_GetInt32(sec, "_VibratoCent", 0);
	otd.vibrato_cent = MyIniParamRange(otd.vibrato_cent, 0, 10000);
	otd.vibrato_delay = MyIni_GetInt32(sec, "_VibratoDelay", 0);
	otd.vibrato_delay = MyIniParamRange(otd.vibrato_delay, 0, 10000);
	otd.filter_freq = MyIni_GetInt32(sec, "_FilterFreq", 0);
	otd.filter_freq = MyIniParamRange(otd.filter_freq, 0.0, 20000.0);
	otd.filter_reso = MyIni_GetInt32(sec, "_FilterReso", 0); // cB
	otd.filter_reso = MyIniParamRange(otd.filter_reso, 0.0, 1000.0);

	voice_filter_reso = MyIni_GetFloat64(sec, "VoiceFilter_Reso", 1.0);
	voice_filter_reso = MyIniParamRange(voice_filter_reso, 0.0, 8.0);
	voice_filter_gain = MyIni_GetFloat64(sec, "VoiceFilter_Gain", 1.0);
	voice_filter_gain = MyIniParamRange(voice_filter_gain, 0.0, 8.0);
	
	sec = MyIni_GetSection(&ini, "envelope", 0);
	gs_env_attack_calc = MyIni_GetFloat64(sec, "GS_ENV_Attack", 1.0);
	gs_env_attack_calc = MyIniParamRange(gs_env_attack_calc, 0.0, 4.0);
	gs_env_decay_calc = MyIni_GetFloat64(sec, "GS_ENV_Decay", 1.0);
	gs_env_decay_calc = MyIniParamRange(gs_env_decay_calc, 0.0, 1.2);
	gs_env_release_calc = MyIni_GetFloat64(sec, "GS_ENV_Release", 1.0);
	gs_env_release_calc = MyIniParamRange(gs_env_release_calc, 0.0, 4.0);
	xg_env_attack_calc = MyIni_GetFloat64(sec, "XG_ENV_Attack", 1.0);
	xg_env_attack_calc = MyIniParamRange(xg_env_attack_calc, 0.0, 4.0);
	xg_env_decay_calc = MyIni_GetFloat64(sec, "XG_ENV_Decay", 1.0);
	xg_env_decay_calc = MyIniParamRange(xg_env_decay_calc, 0.0, 4.0);
	xg_env_release_calc = MyIni_GetFloat64(sec, "XG_ENV_Release", 1.0);
	xg_env_release_calc = MyIniParamRange(xg_env_release_calc, 0.0, 4.0);
	gm2_env_attack_calc = MyIni_GetFloat64(sec, "GM2_ENV_Attack", 1.0);
	gm2_env_attack_calc = MyIniParamRange(gm2_env_attack_calc, 0.0, 4.0);
	gm2_env_decay_calc = MyIni_GetFloat64(sec, "GM2_ENV_Decay", 1.0);
	gm2_env_decay_calc = MyIniParamRange(gm2_env_decay_calc, 0.0, 4.0);
	gm2_env_release_calc = MyIni_GetFloat64(sec, "GM2_ENV_Release", 1.0);
	gm2_env_release_calc = MyIniParamRange(gm2_env_release_calc, 0.0, 4.0);
	gm_env_attack_calc = MyIni_GetFloat64(sec, "GM_ENV_Attack", 1.0);
	gm_env_attack_calc = MyIniParamRange(gm_env_attack_calc, 0.0, 4.0);
	gm_env_decay_calc = MyIni_GetFloat64(sec, "GM_ENV_Decay", 1.0);
	gm_env_decay_calc = MyIniParamRange(gm_env_decay_calc, 0.0, 4.0);
	gm_env_release_calc = MyIni_GetFloat64(sec, "GM_ENV_Release", 1.0);
	gm_env_release_calc = MyIniParamRange(gm_env_release_calc, 0.0, 4.0);
	env_add_offdelay_time = MyIni_GetFloat64(sec, "ENV_Add_Offdelay", 0.0);
	env_add_offdelay_time = MyIniParamRange(env_add_offdelay_time, 0.0, 20.0);

	sec = MyIni_GetSection(&ini, "efx", 0);
///r
	otd.gsefx_CustomODLv = MyIni_GetFloat64(sec, "GS_OD_Level", 1.0);
	otd.gsefx_CustomODLv = MyIniParamRange(otd.gsefx_CustomODLv, 0.001, 4.0);
	otd.gsefx_CustomODDrive = MyIni_GetFloat64(sec, "GS_OD_Drive", 1.0);
	otd.gsefx_CustomODDrive = MyIniParamRange(otd.gsefx_CustomODDrive, 0.001, 4.0);
	otd.gsefx_CustomODFreq = MyIni_GetFloat64(sec, "GS_OD_Freq", 1.0);
	otd.gsefx_CustomODFreq = MyIniParamRange(otd.gsefx_CustomODFreq, 0.001, 2.0);
	otd.xgefx_CustomODLv = MyIni_GetFloat64(sec, "XG_OD_Level", 1.0);
	otd.xgefx_CustomODLv = MyIniParamRange(otd.xgefx_CustomODLv, 0.001, 4.0);
	otd.xgefx_CustomODDrive = MyIni_GetFloat64(sec, "XG_OD_Drive", 1.0);
	otd.xgefx_CustomODDrive = MyIniParamRange(otd.xgefx_CustomODDrive, 0.001, 4.0);
	otd.xgefx_CustomODFreq = MyIni_GetFloat64(sec, "XG_OD_Freq", 1.0);
	otd.xgefx_CustomODFreq = MyIniParamRange(otd.xgefx_CustomODFreq, 0.001, 2.0);
	otd.sdefx_CustomODLv = MyIni_GetFloat64(sec, "SD_OD_Level", 1.0);
	otd.sdefx_CustomODLv = MyIniParamRange(otd.sdefx_CustomODLv, 0.001, 4.0);
	otd.sdefx_CustomODDrive = MyIni_GetFloat64(sec, "SD_OD_Drive", 1.0);
	otd.sdefx_CustomODDrive = MyIniParamRange(otd.sdefx_CustomODDrive, 0.001, 4.0);
	otd.sdefx_CustomODFreq = MyIni_GetFloat64(sec, "SD_OD_Freq", 1.0);
	otd.sdefx_CustomODFreq = MyIniParamRange(otd.sdefx_CustomODFreq, 0.001, 2.0);
	
	otd.gsefx_CustomLFLvIn = MyIni_GetFloat64(sec, "GS_LF_In_Level", 1.0);
	otd.gsefx_CustomLFLvIn = MyIniParamRange(otd.gsefx_CustomLFLvIn, 0.001, 4.0);
	otd.gsefx_CustomLFLvOut = MyIni_GetFloat64(sec, "GS_LF_Out_Level", 1.0);
	otd.gsefx_CustomLFLvOut = MyIniParamRange(otd.gsefx_CustomLFLvOut, 0.001, 4.0);
	otd.xgefx_CustomLFLvIn = MyIni_GetFloat64(sec, "XG_LF_In_Level", 1.0);
	otd.xgefx_CustomLFLvIn = MyIniParamRange(otd.xgefx_CustomLFLvIn, 0.001, 4.0);
	otd.xgefx_CustomLFLvOut = MyIni_GetFloat64(sec, "XG_LF_Out_Level", 1.0);
	otd.xgefx_CustomLFLvOut = MyIniParamRange(otd.xgefx_CustomLFLvOut, 0.001, 4.0);
	otd.sdefx_CustomLFLvIn = MyIni_GetFloat64(sec, "SD_LF_In_Level", 1.0);
	otd.sdefx_CustomLFLvIn = MyIniParamRange(otd.sdefx_CustomLFLvIn, 0.001, 4.0);
	otd.sdefx_CustomLFLvOut = MyIni_GetFloat64(sec, "SD_LF_Out_Level", 1.0);
	otd.sdefx_CustomLFLvOut = MyIniParamRange(otd.sdefx_CustomLFLvOut, 0.001, 4.0);
	
	otd.efx_CustomHmnLvIn = MyIni_GetFloat64(sec, "Hmn_In_Level", 1.0);
	otd.efx_CustomHmnLvIn = MyIniParamRange(otd.efx_CustomHmnLvIn, 0.001, 4.0);
	otd.efx_CustomHmnLvOut = MyIni_GetFloat64(sec, "Hmn_Out_Level", 1.0);
	otd.efx_CustomHmnLvOut = MyIniParamRange(otd.efx_CustomHmnLvOut, 0.001, 4.0);

	otd.efx_CustomLmtLvIn = MyIni_GetFloat64(sec, "Lmt_In_Level", 1.0);
	otd.efx_CustomLmtLvIn = MyIniParamRange(otd.efx_CustomLmtLvIn, 0.001, 4.0);
	otd.efx_CustomLmtLvOut = MyIni_GetFloat64(sec, "Lmt_Out_Level", 1.0);
	otd.efx_CustomLmtLvOut = MyIniParamRange(otd.efx_CustomLmtLvOut, 0.001, 4.0);

	otd.efx_CustomCmpLvIn = MyIni_GetFloat64(sec, "Cmp_In_Level", 1.0);
	otd.efx_CustomCmpLvIn = MyIniParamRange(otd.efx_CustomCmpLvIn, 0.001, 4.0);
	otd.efx_CustomCmpLvOut = MyIni_GetFloat64(sec, "Cmp_Out_Level", 1.0);
	otd.efx_CustomCmpLvOut = MyIniParamRange(otd.efx_CustomCmpLvOut, 0.001, 4.0);
	
	otd.efx_CustomWahLvIn = MyIni_GetFloat64(sec, "Wah_In_Level", 1.0);
	otd.efx_CustomWahLvIn = MyIniParamRange(otd.efx_CustomWahLvIn, 0.001, 4.0);
	otd.efx_CustomWahLvOut = MyIni_GetFloat64(sec, "Wah_Out_Level", 1.0);
	otd.efx_CustomWahLvOut = MyIniParamRange(otd.efx_CustomWahLvOut, 0.001, 4.0);
	
	otd.efx_CustomGRevLvIn = MyIni_GetFloat64(sec, "GRev_In_Level", 1.0);
	otd.efx_CustomGRevLvIn = MyIniParamRange(otd.efx_CustomGRevLvIn, 0.001, 4.0);
	otd.efx_CustomGRevLvOut = MyIni_GetFloat64(sec, "GRev_Out_Level", 1.0);
	otd.efx_CustomGRevLvOut = MyIniParamRange(otd.efx_CustomGRevLvOut, 0.001, 4.0);
	
	otd.efx_CustomEnhLvIn = MyIni_GetFloat64(sec, "Enh_In_Level", 1.0);
	otd.efx_CustomEnhLvIn = MyIniParamRange(otd.efx_CustomEnhLvIn, 0.001, 4.0);
	otd.efx_CustomEnhLvOut = MyIni_GetFloat64(sec, "Enh_Out_Level", 1.0);
	otd.efx_CustomEnhLvOut = MyIniParamRange(otd.efx_CustomEnhLvOut, 0.001, 4.0);
	
	otd.efx_CustomRotLvOut = MyIni_GetFloat64(sec, "Rot_Out_Level", 1.0);
	otd.efx_CustomRotLvOut = MyIniParamRange(otd.efx_CustomRotLvOut, 0.001, 4.0);
	
	otd.efx_CustomPSLvOut = MyIni_GetFloat64(sec, "PS_Out_Level", 1.0);
	otd.efx_CustomPSLvOut = MyIniParamRange(otd.efx_CustomPSLvOut, 0.001, 4.0);
	
	otd.efx_CustomRMLvOut = MyIni_GetFloat64(sec, "RM_Out_Level", 1.0);
	otd.efx_CustomRMLvOut = MyIniParamRange(otd.efx_CustomRMLvOut, 0.001, 4.0);

	otd.efx_CustomRevType = MyIni_GetInt32(sec, "Rev_Type", 0);

	sec = MyIni_GetSection(&ini, "chorus", 0);
	ext_chorus_level = MyIni_GetFloat64(sec, "Ext_Level", 1.0);
	ext_chorus_level = MyIniParamRange(ext_chorus_level, 0.001, 4.0);
	ext_chorus_feedback = MyIni_GetFloat64(sec, "Ext_Feedback", 1.0);
	ext_chorus_feedback = MyIniParamRange(ext_chorus_feedback, 0.001, 4.0);
	ext_chorus_depth = MyIni_GetFloat64(sec, "Ext_Depth", 1.0);
	ext_chorus_depth = MyIniParamRange(ext_chorus_depth, 0.001, 4.0);
	ext_chorus_ex_phase = MyIni_GetInt32(sec, "Ext_EX_Phase", 3);
	ext_chorus_ex_phase = MyIniParamRange(ext_chorus_ex_phase, 1, 8);
	ext_chorus_ex_lite = MyIni_GetInt32(sec, "Ext_EX_Lite", 0);
	ext_chorus_ex_lite = MyIniParamRange(ext_chorus_ex_lite, 0, 1);
	ext_chorus_ex_ov = MyIni_GetInt32(sec, "Ext_EX_OV", 0);
	ext_chorus_ex_ov = MyIniParamRange(ext_chorus_ex_ov, 0, 1);
#ifdef CUSTOMIZE_CHORUS_PARAM
	otd.chorus_param.pre_lpf = MyIni_GetInt8(sec, "PreLPF", 0);// 0;
	otd.chorus_param.pre_lpf = MyIniParamRange(otd.chorus_param.pre_lpf,			0, 6);
	otd.chorus_param.level = MyIni_GetInt8(sec, "Level", 0x40);//0x40;
	otd.chorus_param.level = MyIniParamRange(otd.chorus_param.level,			0, 127);
	otd.chorus_param.feedback = MyIni_GetInt8(sec, "Feedback", 0x08);//0x08;
	otd.chorus_param.feedback = MyIniParamRange(otd.chorus_param.feedback,	0, 127);
	otd.chorus_param.delay = MyIni_GetInt8(sec, "Delay", 0x50);//0x50;
	otd.chorus_param.delay = MyIniParamRange(otd.chorus_param.delay,			0, 127);
	otd.chorus_param.rate = MyIni_GetInt8(sec, "Rate", 0x03);//0x03;
	otd.chorus_param.rate = MyIniParamRange(otd.chorus_param.rate,			0, 127);
	otd.chorus_param.depth = MyIni_GetInt8(sec, "Depth", 0x13);//0x13;
	otd.chorus_param.depth = MyIniParamRange(otd.chorus_param.depth,			0, 127);
	otd.chorus_param.send_reverb = MyIni_GetInt8(sec, "SendReverb",	0);//0;
	otd.chorus_param.send_reverb = MyIniParamRange(otd.chorus_param.send_reverb, 0, 127);
	otd.chorus_param.send_delay = MyIni_GetInt8(sec, "SendDelay", 0);//0;
	otd.chorus_param.send_delay = MyIniParamRange(otd.chorus_param.send_delay,	0, 127);
#endif

	sec = MyIni_GetSection(&ini, "main", 1);
	if (MyIni_GetUint32(sec, "ver", 100) == 100) {
		INISEC *old_sec;
		old_sec = MyIni_GetSection(&ini, "param", 0);

		MyIni_SetUint32(sec, "ver", 101);
		MyIni_SetBool(sec, "OverWriteVibrato",	MyIni_GetBool(old_sec, "_OverWriteVibrato", 0));
		MyIni_SetBool(sec, "OverWriteTremolo",	MyIni_GetBool(old_sec, "_OverWriteTremolo", 0));
		MyIni_SetBool(sec, "OverWriteVel",		0);
		MyIni_SetBool(sec, "OverWriteEnv",		0);
		MyIni_SetBool(sec, "OverWriteModEnv",	0);
		MyIni_SetBool(sec, "OverWriteCutoff",	MyIni_GetBool(old_sec, "_OverWriteCutoff", 0));

		MyIni_DeleteKey(&ini, "param", "_OverWriteVibrato");
		MyIni_DeleteKey(&ini, "param", "_OverWriteTremolo");
		MyIni_DeleteKey(&ini, "param", "_OverWriteVel");
		MyIni_DeleteKey(&ini, "param", "_OverWriteEnv");
		MyIni_DeleteKey(&ini, "param", "_OverWriteModEnv");
		MyIni_DeleteKey(&ini, "param", "_OverWriteCutoff");

		MyIni_Save(&ini, fn);
	}
	otd.overwriteMode = 0;
	otd.overwriteMode |= MyIni_GetBool(sec, "OverWriteVibrato", 0) * EOWM_ENABLE_VIBRATO;
	otd.overwriteMode |= MyIni_GetBool(sec, "OverWriteTremolo", 0) * EOWM_ENABLE_TREMOLO;
	otd.overwriteMode |= MyIni_GetBool(sec, "OverWriteCutoff", 0) * EOWM_ENABLE_CUTOFF;
	otd.overwriteMode |= MyIni_GetBool(sec, "OverWriteVel", 0) * EOWM_ENABLE_VEL;
	otd.overwriteMode |= MyIni_GetBool(sec, "OverWriteModEnv", 0) * EOWM_ENABLE_MOD;
	otd.overwriteMode |= MyIni_GetBool(sec, "OverWriteEnv", 0) * EOWM_ENABLE_ENV;

	otd.timRunMode = 0;

	sec = MyIni_GetSection(&ini, "compressor", 0);
	otd.timRunMode |= MyIni_GetBool(sec, "enable", 0) * EOWM_USE_COMPRESSOR;
	otd.compThr = MyIni_GetFloat32(sec, "threshold", 50);
	otd.compSlope = MyIni_GetFloat32(sec, "slope", 50);
	otd.compLook = MyIni_GetFloat32(sec, "lookahead", 3);
	otd.compWTime = MyIni_GetFloat32(sec, "window", 1);
	otd.compATime = MyIni_GetFloat32(sec, "attack", 0.01);
	otd.compRTime = MyIni_GetFloat32(sec, "release", 300);

	sec = MyIni_GetSection(&ini, "reverb", 0);
	// freeverb
	scalewet = MyIni_GetFloat32(sec, "scalewet", 0.06);
	freeverb_scaleroom = MyIni_GetFloat32(sec, "scaleroom", 0.28);
	freeverb_offsetroom = MyIni_GetFloat32(sec, "offsetroom", 0.7);
	fixedgain = MyIni_GetFloat32(sec, "fixedgain", 0.025);
	combfbk = MyIni_GetFloat32(sec, "combfbk", 3.0);
	time_rt_diff = MyIni_GetFloat32(sec, "timediff", 1.0);
	numcombs = MyIni_GetInt32(sec, "numcombs", 8);
	// reverb ex
	ext_reverb_ex_time = MyIni_GetFloat32(sec, "Ext_EX_Time", 1.0);
	ext_reverb_ex_level = MyIni_GetFloat32(sec, "Ext_EX_Level", 1.0);
	ext_reverb_ex_er_level = MyIni_GetFloat32(sec, "Ext_EX_ER_Level", 1.0);
	ext_reverb_ex_rv_level = MyIni_GetFloat32(sec, "Ext_EX_RV_Level", 1.0);
	ext_reverb_ex_rv_num = MyIni_GetInt32(sec, "Ext_EX_RV_Num", 16);
	ext_reverb_ex_ap_num = MyIni_GetInt32(sec, "Ext_EX_AP_Num", 8);
	//ext_reverb_ex_lite = MyIni_GetInt32(sec, "Ext_EX_Lite", 0);
	//ext_reverb_ex_lite = MyIniParamRange(ext_reverb_ex_lite, 0, 1);
	//ext_reverb_ex_mode = MyIni_GetInt32(sec, "Ext_EX_Mode", 0);
	//ext_reverb_ex_er_num = MyIni_GetInt32(sec, "Ext_EX_ER_Num", 32);
	//ext_reverb_ex_rv_type = MyIni_GetInt32(sec, "Ext_EX_RV_Type", 1);
	//ext_reverb_ex_ap_type = MyIni_GetInt32(sec, "Ext_EX_AP_Type", 0);
	ext_reverb_ex_mod = MyIni_GetInt32(sec, "Ext_EX_Mod", 0);
	// reverb ex2	
	ext_reverb_ex2_level = MyIni_GetFloat32(sec, "Ext_SR_Level", 1.0);
	ext_reverb_ex2_rsmode = MyIni_GetInt32(sec, "Ext_SR_RS_Mode", 3);
	ext_reverb_ex2_fftmode = MyIni_GetInt32(sec, "Ext_SR_FFT_Mode", 0);
	// plate reverb
	ext_plate_reverb_level = MyIni_GetFloat32(sec, "Ext_Plate_Level", 1.0);
	ext_plate_reverb_level = MyIniParamRange(ext_plate_reverb_level, 0.001, 8.0);
	ext_plate_reverb_time = MyIni_GetFloat32(sec, "Ext_Plate_Time", 1.0);
	ext_plate_reverb_time = MyIniParamRange(ext_plate_reverb_time, 0.01, 2.0);
	
	sec = MyIni_GetSection(&ini, "delay", 0);
	otd.delay_param.delay = MyIni_GetFloat32(sec, "delay", 1.0);
	otd.delay_param.delay = MyIniParamRange(otd.delay_param.delay, 0.01, 2.0);
	otd.delay_param.level = MyIni_GetFloat64(sec, "level", 1.0);
	otd.delay_param.level = MyIniParamRange(otd.delay_param.level, 0.001, 8.0);
	otd.delay_param.feedback = MyIni_GetFloat64(sec, "feedback", 1.0);
	otd.delay_param.feedback = MyIniParamRange(otd.delay_param.feedback, 0.001, 4.0);
		
	sec = MyIni_GetSection(&ini, "filter", 0);
	ext_filter_shelving_gain = MyIni_GetFloat64(sec, "Ext_Shelving_Gain", 1.0);
	ext_filter_shelving_gain = MyIniParamRange(ext_filter_shelving_gain, 0.001, 8.0);
	ext_filter_shelving_reduce = MyIni_GetFloat64(sec, "Ext_Shelving_Reduce", 1.0);
	ext_filter_shelving_reduce = MyIniParamRange(ext_filter_shelving_reduce, 0.0, 8.0);
	ext_filter_shelving_q = MyIni_GetFloat64(sec, "Ext_Shelving_Q", 1.0);
	ext_filter_shelving_q = MyIniParamRange(ext_filter_shelving_q, 0.25, 8.0);
	ext_filter_peaking_gain = MyIni_GetFloat64(sec, "Ext_Peaking_Gain", 1.0);
	ext_filter_peaking_gain = MyIniParamRange(ext_filter_peaking_gain, 0.001, 8.0);
	ext_filter_peaking_reduce = MyIni_GetFloat64(sec, "Ext_Peaking_Reduce", 1.0);
	ext_filter_peaking_reduce = MyIniParamRange(ext_filter_peaking_reduce, 0.0, 8.0);
	ext_filter_peaking_q = MyIni_GetFloat64(sec, "Ext_Peaking_Q", 1.0);
	ext_filter_peaking_q = MyIniParamRange(ext_filter_peaking_q, 0.25, 8.0);
			
	sec = MyIni_GetSection(&ini, "effect", 0);
	//xg_system_return_level = MyIni_GetFloat64(sec, "XG_System_Return_Level", 1.0);
	//xg_system_return_level = MyIniParamRange(xg_system_return_level, 0.001, 8.0);
	xg_reverb_return_level = MyIni_GetFloat64(sec, "XG_Reverb_Return_Level", 1.0);
	xg_reverb_return_level = MyIniParamRange(xg_reverb_return_level, 0.001, 8.0);
	xg_chorus_return_level = MyIni_GetFloat64(sec, "XG_Chorus_Return_Level", 1.0);
	xg_chorus_return_level = MyIniParamRange(xg_chorus_return_level, 0.001, 8.0);
	xg_variation_return_level = MyIni_GetFloat64(sec, "XG_Variation_Return_Level", 1.0);
	xg_variation_return_level = MyIniParamRange(xg_variation_return_level, 0.001, 8.0);	
	xg_chorus_send_reverb = MyIni_GetFloat64(sec, "XG_Chorus_Send_Reverb", 1.0);
	xg_chorus_send_reverb = MyIniParamRange(xg_chorus_send_reverb, 0.001, 8.0);
	xg_variation_send_reverb = MyIni_GetFloat64(sec, "XG_Variation_Send_Reverb", 1.0);
	xg_variation_send_reverb = MyIniParamRange(xg_variation_send_reverb, 0.001, 8.0);
	xg_variation_send_chorus = MyIni_GetFloat64(sec, "XG_Variation_Send_Chorus", 1.0);
	xg_variation_send_chorus = MyIniParamRange(xg_variation_send_chorus, 0.001, 8.0);

	sec = MyIni_GetSection(&ini, "driver", 0);
	otd.EnableVolMidCtrl = MyIni_GetInt32(sec, "volctrl", 1);
	otd.DriverRVolume = MyIni_GetInt32(sec, "nowvol", 100);
	otd.DriverRVolume = MyIniParamRange(otd.DriverRVolume, 0, 200);

	MyIni_SectionAllClear(&ini);

#undef MyIniParamRange
