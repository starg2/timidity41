#ifndef SNDFONTINI_H
#define SNDFONTINI_H


enum EnumOverWriteMode {//elion add
	EOWM_ENABLE_VIBRATO = 1, 
	EOWM_ENABLE_TREMOLO = 2,
	EOWM_ENABLE_CUTOFF = 4, 
	EOWM_ENABLE_VEL = 8, 
	EOWM_ENABLE_MOD = 16, 
	EOWM_ENABLE_ENV = 32, 
};

enum EnumTimRunMode {//elion add
	EOWM_USE_COMPRESSOR = 1, 
};

#define CUSTOMIZE_CHORUS_PARAM
typedef struct OverrideTiMidityData {
	short overwriteMode;
	short timRunMode;
	/* nrpn */
	double vibrato_rate;
	double vibrato_cent;
	double vibrato_delay;
	double filter_freq;
	double filter_reso;
	/* effect */
	char chorus_send, reverb_send;
	double gsefx_CustomODLv;
	double gsefx_CustomODDrive;
	double gsefx_CustomODFreq;
	double xgefx_CustomODLv;
	double xgefx_CustomODDrive;
	double xgefx_CustomODFreq;
	double sdefx_CustomODLv;
	double sdefx_CustomODDrive;
	double sdefx_CustomODFreq;

	double gsefx_CustomLFLvIn;
	double gsefx_CustomLFLvOut;
	double xgefx_CustomLFLvIn;
	double xgefx_CustomLFLvOut;
	double sdefx_CustomLFLvIn;
	double sdefx_CustomLFLvOut;
	
	double efx_CustomHmnLvIn;
	double efx_CustomHmnLvOut;

	double efx_CustomLmtLvIn;
	double efx_CustomLmtLvOut;

	double efx_CustomCmpLvIn;
	double efx_CustomCmpLvOut;

	double efx_CustomWahLvIn;
	double efx_CustomWahLvOut;
	
	double efx_CustomGRevLvIn;
	double efx_CustomGRevLvOut;

	double efx_CustomEnhLvIn;
	double efx_CustomEnhLvOut;

	double efx_CustomRotLvOut;

	double efx_CustomPSLvOut;

	double efx_CustomRMLvOut;

	int efx_CustomRevType;

#ifdef CUSTOMIZE_CHORUS_PARAM
	struct tag_chorus {
		char pre_lpf;
		char level;
		char feedback;
		char delay;
		char rate;
		char depth;
		char send_reverb;
		char send_delay;
	}chorus_param;
#endif

	struct tag_delay
	{
		float delay;
		double level, feedback;
	}delay_param;

	double compThr, compSlope, compLook, compWTime, compATime, compRTime;

	unsigned char EnableVolMidCtrl;
	short DriverRVolume;
}OVERRIDETIMIDITYDATA ;

extern char sfini_path[FILEPATH_MAX];


#if defined(WINDRV) || defined(WINDRV_SETUP)
extern void timdrvOverrideSFSettingLoad(void);
#elif defined(KBTIM) || defined(WINVSTI)
extern void OverrideSFSettingLoad(const char*, int);
#else
extern void OverrideSFSettingLoad(void);
#endif


extern Sample OverrideSample;
extern OVERRIDETIMIDITYDATA otd;

extern int8 sf_attenuation_neg;
extern double sf_attenuation_pow;
extern double sf_attenuation_mul;
extern double sf_attenuation_add;

extern int32 sf_limit_volenv_attack;
extern int32 sf_limit_modenv_attack;
extern int32 sf_limit_modenv_fc;
extern int32 sf_limit_modenv_pitch;
extern int32 sf_limit_modlfo_fc;
extern int32 sf_limit_modlfo_pitch;
extern int32 sf_limit_viblfo_pitch;
extern int32 sf_limit_modlfo_freq;
extern int32 sf_limit_viblfo_freq;

extern int32 sf_default_modlfo_freq;
extern int32 sf_default_viblfo_freq;

extern int8 sf_config_lfo_swap;
extern int8 sf_config_addrs_offset;

extern double gs_env_attack_calc;
extern double gs_env_decay_calc;
extern double gs_env_release_calc;
extern double xg_env_attack_calc;
extern double xg_env_decay_calc;
extern double xg_env_release_calc;
extern double gm2_env_attack_calc;
extern double gm2_env_decay_calc;
extern double gm2_env_release_calc;
extern double gm_env_attack_calc;
extern double gm_env_decay_calc;
extern double gm_env_release_calc;
extern double env_add_offdelay_time;

extern double voice_filter_reso;
extern double voice_filter_gain;

extern double scalewet;
extern double freeverb_scaleroom;
extern double freeverb_offsetroom;
extern double fixedgain;
extern double combfbk;
extern double time_rt_diff;
extern int numcombs;

extern double ext_reverb_ex_time;
extern double ext_reverb_ex_level;
extern double ext_reverb_ex_er_level;
extern double ext_reverb_ex_rv_level;
extern int ext_reverb_ex_rv_num;
extern int ext_reverb_ex_ap_num;
//extern int ext_reverb_ex_lite;
//extern int ext_reverb_ex_mode;
//extern int ext_reverb_ex_er_num;
//extern int ext_reverb_ex_ap_num;
//extern int ext_reverb_ex_rv_type;
//extern int ext_reverb_ex_ap_type;
extern int ext_reverb_ex_mod;

extern double ext_reverb_ex2_level;
extern int ext_reverb_ex2_rsmode;
extern int ext_reverb_ex2_fftmode;

extern double ext_plate_reverb_level;
extern double ext_plate_reverb_time;

extern double ext_chorus_level;
extern double ext_chorus_feedback;
extern double ext_chorus_depth;
extern int ext_chorus_ex_phase;
extern int ext_chorus_ex_lite;
extern int ext_chorus_ex_ov;

extern double ext_filter_shelving_gain;
extern double ext_filter_shelving_reduce;
extern double ext_filter_shelving_q;
extern double ext_filter_peaking_gain;
extern double ext_filter_peaking_reduce;
extern double ext_filter_peaking_q;

//extern double xg_system_return_level;
extern double xg_reverb_return_level;
extern double xg_chorus_return_level;
extern double xg_variation_return_level;
extern double xg_chorus_send_reverb;
extern double xg_variation_send_reverb;
extern double xg_variation_send_chorus;

#endif /* SNDFONTINI_H */
