

#include "timidity.c"


//timidity.c で static なグローバル変数にアクセスする必要がある
//timidity.c を一切書き換えないようにするためには、こうするしかないと
//思われる

void kbtim_initialize(char *szCfgFile);
void kbtim_uninitialize(void);
void kbtim_play_midi_file(char *szFileName);

static void kbtim_start_initialize(void);//timidity_start_initialize 相当
static void kbtim_init_player(void);//timidity_init_player 相当

extern void uninitialize_resampler_coeffs(void);
//extern void free_reverb_buffer(void);
extern void free_resamp_cache_data(void);
extern void free_audio_bucket(void);
extern void free_userdrum(void);
//extern void free_userdrum2(void);
extern void free_soundfonts(void);


void kbtim_start_initialize(void)//timidity_start_initialize 相当
{
    int i;
    static int drums[] = DEFAULT_DRUMCHANNELS;

    if(!output_text_code){
	    output_text_code = safe_strdup(OUTPUT_TEXT_CODE);
    }
    if(!opt_aq_max_buff){
	    opt_aq_max_buff = safe_strdup("5.0");
    }
    if(!opt_aq_fill_buff){
	    opt_aq_fill_buff = safe_strdup("100%");
    }
    for(i = 0; i < MAX_CHANNELS; i++){
	    memset(&(channel[i]), 0, sizeof(Channel));
    }

    CLEAR_CHANNELMASK(quietchannels);
    CLEAR_CHANNELMASK(default_drumchannels);

    for(i = 0; drums[i] > 0; i++){
	    SET_CHANNELMASK(default_drumchannels, drums[i] - 1);
    }
    for(i = 16; i < MAX_CHANNELS; i++){
        if(IS_SET_CHANNELMASK(default_drumchannels, i & 0xF)){
	        SET_CHANNELMASK(default_drumchannels, i);
        }
    }
    for(i = 0; i < MAX_CHANNELS; i++){
	    default_program[i] = DEFAULT_PROGRAM;
	    memset(channel[i].drums, 0, sizeof(channel[i].drums));
    }
    {
        for(i = 0; url_module_list[i]; i++){
	        url_add_module(url_module_list[i]);
        }
    	init_string_table(&opt_config_string);
	    init_freq_table();
	    init_freq_table_tuning();
    	init_freq_table_pytha();
	    init_freq_table_meantone();
	    init_freq_table_pureint();
	    init_freq_table_user();
	    init_bend_fine();
	    init_bend_coarse();
	    init_tables();
	    init_gm2_pan_table();
	    init_attack_vol_table();
	    init_sb_vol_table();
	    init_modenv_vol_table();
	    init_def_vol_table();
	    init_gs_vol_table();
	    init_perceived_vol_table();
	    init_gm2_vol_table();
        for(i = 0; i < NSPECIAL_PATCH; i++){
	        special_patch[i] = NULL;
        }
	    init_midi_trace();
	    int_rand(-1);	// initialize random seed
	    int_rand(42);	// the 1st number generated is not very random
    }
}

///r
void kbtim_init_player(void)//timidity_init_player 相当
{
    // Allocate voice[]
    free_voices();
    safe_free(voice);
    voice = (Voice*) safe_calloc(max_voices, sizeof(Voice));

    // save defaults 
    COPY_CHANNELMASK(drumchannels, default_drumchannels);
    COPY_CHANNELMASK(drumchannel_mask, default_drumchannel_mask);
	
	init_output(); // div_playmode_rate
	init_playmidi();
	init_mix_c();
#ifdef INT_SYNTH
	init_int_synth();
#endif // INT_SYNTH
}

void kbtim_initialize(char *szCfgFile)
{
    char local[1024];
    kbtim_start_initialize();
    initialize_resampler_coeffs();
    //timidity_pre_load_configuration 相当
    lstrcpyn(local, szCfgFile, sizeof(local));
///r
  	read_config_file(local, 0, READ_CONFIG_SUCCESS);

    //timidity_post_load_configuration 相当
    if(opt_config_string.nstring > 0){
	    char **config_string_list;
	    config_string_list = make_string_array(&opt_config_string);
	    if(config_string_list != NULL){
    	    int i;
    	    for(i = 0; config_string_list[i]; i++){
                read_config_file(config_string_list[i], 1, READ_CONFIG_SUCCESS); ///r
	        }
	        free(config_string_list[0]);
	        free(config_string_list);
	    }
    }
    //
    kbtim_init_player();
	init_load_soundfont();	
///r
	load_all_instrument();
#ifdef MULTI_THREAD_COMPUTE
	begin_compute_thread();
#endif
}

void kbtim_uninitialize(void)
{
    int i;
	
	//timidity_play_main の後始末
	free_archive_files();
	
	//win_main の後始末
#ifdef MULTI_THREAD_COMPUTE
	terminate_compute_thread();
#endif
	safe_free(pcm_alternate_file);
	pcm_alternate_file = NULL;
	safe_free(opt_output_name);
	opt_output_name = NULL;
	safe_free(opt_aq_max_buff);
	opt_aq_max_buff = NULL;
	safe_free(opt_aq_fill_buff);
	opt_aq_fill_buff = NULL;
	safe_free(opt_reduce_voice_threshold);
	opt_reduce_voice_threshold = NULL;
	safe_free(opt_reduce_quality_threshold);
	opt_reduce_quality_threshold = NULL;
	safe_free(opt_reduce_polyphony_threshold);
	opt_reduce_polyphony_threshold = NULL;
	safe_free(output_text_code);
	output_text_code = NULL;
	safe_free(wrdt_open_opts);
	wrdt_open_opts = NULL;
		
	free_soft_queue();
	free_audio_bucket();
	free_instruments(0);
	free_soundfonts();
	free_cache_data();
	//freq.c::freq_initialize_fft_arrays で確保したメモリの解放
	free_freq_data();
	free_wrd();
	free_readmidi();
	free_playmidi();
	free_mix_c();
	free_global_mblock();
	tmdy_free_config();
	//free_reverb_buffer();
	free_effect_buffers();
#ifdef INT_SYNTH
	free_int_synth();
#endif // INT_SYNTH
	free_voices();//free_voice_by_Kobarin() の代わり
    //initialize_resampler_coeffs で確保したメモリの解放
    uninitialize_resampler_coeffs();

	for (i = 0; i < MAX_CHANNELS; i++)
		free_drum_effect(i);
}

void kbtim_play_midi_file(char *szFileName)
{
	 // KbTimDecoder::Open()のレート反映
	init_output(); // div_playmode_rate
	free_playmidi();
	init_playmidi();
	init_mix_c();
#ifdef INT_SYNTH
	init_int_synth();
#endif // INT_SYNTH
    if(!opt_control_ratio){
	    control_ratio = play_mode->rate / CONTROLS_PER_SECOND;
        if(control_ratio < 1){
            control_ratio = 1;
        }
        else if(control_ratio > MAX_CONTROL_RATIO){
            control_ratio = MAX_CONTROL_RATIO;
        }
    }
    aq_setup();
	timidity_init_aq_buff();

    if(allocate_cache_size > 0)
	   resamp_cache_reset();

	if (def_prog >= 0)
		set_default_program(def_prog);
	if (*def_instr_name)
		set_default_instrument(def_instr_name);

    play_mode->open_output();
    play_midi_file(szFileName);
    aq_flush(1);
	play_mode->close_output();
	ctl->close();
	wrdt->close();
    if(free_instruments_afterwards){
        free_instruments(0);
    }
    free_all_midi_file_info();//これがないと曲切り替え時の演奏がおかしくなる（？）
    free_userdrum();          //init_userdrum() で確保したメモリの解放
    free_archive_files();
}
