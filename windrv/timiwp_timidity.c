#include "config.h"
#include "timidity.h"
#include "common.h"
#include <windows.h>
#include <mmsystem.h>
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif /* HAVE_SYS_STAT_H */
#include <io.h>

#ifndef __bool_true_false_are_defined
# ifdef bool
#  undef bool
# endif
# ifdef ture
#  undef ture
# endif
# ifdef false
#  undef false
# endif
# define bool int
# define false ((bool)0)
# define true (!false)
# define __bool_true_false_are_defined true
#endif 
/* C99 _Bool hack */

#include "arc.h"
#include "tmdy_getopt.h"
#include "rtsyn.h"
#include "thread.h"


/* MAIN_INTERFACE */
extern void timidity_start_initialize(void);
extern int timidity_pre_load_configuration(void);
extern int timidity_post_load_configuration(void);
extern void timidity_init_player(void);
extern void timidity_init_aq_buff(void);
extern int set_tim_opt_long(int, char*, int);

extern void tmdy_free_config(void);

extern  const char *optcommands;
extern  const struct option longopts[];

#define INTERACTIVE_INTERFACE_IDS "kmqagrwAWP"

extern int got_a_configuration;

extern int def_prog;
extern char def_instr_name[];

extern CRITICAL_SECTION critSect;
extern int opt_evil_mode;

#include "timiwp_timidity.h"


static BOOL WINAPI handler(DWORD dw)
{
//  printf("***BREAK" NLS); fflush(stdout);
    intr++;
    return TRUE;
}

static RETSIGTYPE sigterm_exit(int sig)
{
    char s[4];

    /* NOTE: Here, fprintf is dangerous because it is not re-enterance
     * function.  It is possible coredump if the signal is called in printf's.
     */
/*
    write(2, "Terminated sig=0x", 17);
    s[0] = "0123456789abcdef"[(sig >> 4) & 0xf];
    s[1] = "0123456789abcdef"[sig & 0xf];
    s[2] = '\n';
    write(2, s, 3);
*/
    safe_exit(1);
    return 0;
}

static inline bool directory_p(const char *path)
{
    struct stat st;
    if (stat(path, &st) != -1) return S_ISDIR(st.st_mode);
    return false;
}

static inline void canonicalize_path(char *path)
{
    int len = strlen(path);
    if (!len || path[len - 1] == PATH_SEP) return;
    path[len] = PATH_SEP;
    path[len + 1] = '\0';
}

#include <shlwapi.h>
extern HINSTANCE hDllInstance;
extern void w32g_initialize(void);

int timiwp_main_ini(int argc, char **argv)
{
    int c, err;
    int nfiles;
    char **files;
    int main_ret;
    int longind;
    int CoInitializeOK = 0;

#if defined(VST_LOADER_ENABLE)
	if (hVSTHost == NULL) {
		// 先にwindowsディレクトリから検索 
		char szWrapDll[FILEPATH_MAX];
		GetWindowsDirectory(szWrapDll, FILEPATH_MAX - 1);
		if (IS_PATH_SEP(szWrapDll[strlen(szWrapDll) - 1]))
			szWrapDll[strlen(szWrapDll) - 1] = 0;
		//char szWrapDll[FILEPATH_MAX];
		//GetModuleFileName(hDllInstance, szWrapDll, FILEPATH_MAX);
		//PathRemoveFileSpec(szWrapDll);
#ifdef _WIN64
		strcat(szWrapDll, "\\timvstwrap_x64.dll");
#else
		strcat(szWrapDll, "\\timvstwrap.dll");
#endif
		hVSTHost = LoadLibrary(szWrapDll);

		if (hVSTHost == NULL){
#ifdef _WIN64
			hVSTHost = LoadLibrary("timvstwrap_x64.dll");
#else
			hVSTHost = LoadLibrary("timvstwrap.dll");
#endif
		}
		if (hVSTHost != NULL) {
			((vst_open)GetProcAddress(hVSTHost, "vstOpen"))();
		}
	}
#endif

    program_name = argv[0];
    timidity_start_initialize();
    if (CoInitialize(NULL) == S_OK)
	CoInitializeOK = 1;
    w32g_initialize();

    for (c = 1; c < argc; c++)
    {
	if (directory_p(argv[c]))
	{
	    char *p;
	    p = (char*) safe_malloc(strlen(argv[c]) + 2);
	    strcpy(p, argv[c]);
	    canonicalize_path(p);
	    free(argv[c]);
	    argv[c] = p;
	}
    }

#if !defined(IA_WINSYN) && !defined(IA_PORTMIDISYN) && !defined(IA_W32G_SYN)
    if ((err = timidity_pre_load_configuration()) != 0)
	return err;
#else
    opt_sf_close_each_file = 0;
#endif

    optind = longind = 0;
    while ((c = getopt_long(argc, argv, optcommands, longopts, &longind)) > 0)
	if ((err = set_tim_opt_long(c, optarg, longind)) != 0)
	    break;

#if defined(IA_WINSYN) || defined(IA_PORTMIDISYN) || defined(IA_W32G_SYN)
    if (got_a_configuration != 1) {
	if ((err = timidity_pre_load_configuration()) != 0)
	    return err;
    }
#endif

    err += timidity_post_load_configuration();

    /* If there were problems, give up now */
    if (err || (optind >= argc &&
		!strchr(INTERACTIVE_INTERFACE_IDS, ctl->id_character)))
    {
	if (!got_a_configuration)
	{
	    char config1[FILEPATH_MAX];
	    char config2[FILEPATH_MAX];

	    ZeroMemory(config1, sizeof(config1));
	    ZeroMemory(config2, sizeof(config2));
	    GetWindowsDirectoryA(config1, FILEPATH_MAX - 1 - 13);
	    strlcat(config1, "\\TIMIDITY.CFG", FILEPATH_MAX);
	    if (GetModuleFileNameA(NULL, config2, FILEPATH_MAX - 1))
	    {
		char *strp;
		config2[FILEPATH_MAX - 1] = '\0';
		if (strp = strrchr(config2, '\\'))
		{
		    *(++strp) = '\0';
		    strncat(config2, "TIMIDITY.CFG", sizeof(config2) - strlen(config2) - 1);
		}
	    }

	    ctl->cmsg(CMSG_FATAL, VERB_NORMAL,
		      "%s: Can't read any configuration file.\nPlease check "
		      "%s or %s", program_name, config1, config2);

	}
	else
	{
	    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		      "Try %s -h for help", program_name);
	}
    }

    timidity_init_player();
///r
	load_all_instrument();
#ifdef MULTI_THREAD_COMPUTE
	begin_compute_thread();
#endif

    nfiles = argc - optind;
    files  = argv + optind;
    if (nfiles > 0 && ctl->id_character != 'r' && ctl->id_character != 'A' && ctl->id_character != 'W' && ctl->id_character != 'P')
	files = expand_file_archives(files, &nfiles);
    if (dumb_error_count)
	Sleep(1);

    main_ret = timiwp_play_main_ini(nfiles, files);

    w32g_uninitialize();
    if (CoInitializeOK)
	CoUninitialize();

    return main_ret;
}

int timiwp_main_close(void)
{
    int i;
///r
#ifdef MULTI_THREAD_COMPUTE
	terminate_compute_thread();
#endif
#if 0
    timiwp_play_main_close();

    free_instruments(0);
    free_global_mblock();
    free_all_midi_file_info();
    free_userdrum();
    free_userinst();
    tmdy_free_config();
    free_effect_buffers();
    for (i = 0; i < MAX_CHANNELS; i++) { free_drum_effect(i); }
#endif
#ifdef SUPPORT_SOCKET
    safe_free(url_user_agent);
    url_user_agent = NULL;
    safe_free(url_http_proxy_host);
    url_http_proxy_host = NULL;
    safe_free(url_ftp_proxy_host);
    url_ftp_proxy_host = NULL;
    safe_free(user_mailaddr);
    user_mailaddr = NULL;
#endif
#if 0
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
    if (nfiles > 0
	&& ctl->id_character != 'r' && ctl->id_character != 'A'
	&& ctl->id_character != 'W' && ctl->id_character != 'N'  && ctl->id_character != 'P')
    {
	safe_free(files_nbuf);
	safe_free(files);
    }
#endif
    free_soft_queue();
    free_audio_bucket();
    free_instruments(0);
    free_soundfonts();
    free_cache_data();
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
#endif
    free_voices();
    safe_free(voice);
    voice = NULL;
    uninitialize_resampler_coeffs();
    for (i = 0; i < MAX_CHANNELS; i++)
	free_drum_effect(i);
#ifdef VST_LOADER_ENABLE
	if (hVSTHost) {
		((vst_close) GetProcAddress(hVSTHost, "vstClose"))();
		FreeLibrary(hVSTHost);
		hVSTHost = NULL;
	}
#endif
    return 0;
}

///r
static inline int set_default_program(int prog)
{
    int bank;
    Instrument *ip;

    bank = (special_tonebank >= 0) ? special_tonebank : default_tonebank;
    if ((ip = play_midi_load_instrument(0, bank, prog, 0, NULL)) == NULL) // elm=0
	return 1;

    default_instrument = ip;
    return 0;
}

int timiwp_play_main_ini(int nfiles, char **files)
{
    int output_fail = 0;

    if (ctl->open(0, 0))
    {
/*	fprintf(stderr, "Couldn't open %s (`%c')" NLS,
		ctl->id_name, ctl->id_character);
*/
	play_mode->close_output();
	return 3;
    }

#ifdef HAVE_SIGNAL
    signal(SIGTERM, sigterm_exit);
#endif /* HAVE_SIGNAL */
    SetConsoleCtrlHandler(handler, TRUE);

    ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
	      "Initialize for Critical Section");
    InitializeCriticalSection(&critSect);
    if (opt_evil_mode)
	if (!SetThreadPriority(GetCurrentThread(),
			       THREAD_PRIORITY_ABOVE_NORMAL))
	    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		      "Error raising process priority");

    /* Open output device */
    ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
	      "Open output: %c, %s",
	      play_mode->id_character,
	      play_mode->id_name);

    if (play_mode->flag & PF_PCM_STREAM) {
	play_mode->extra_param[1] = aq_calc_fragsize();
	ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
		  "requesting fragment size: %d",
		  play_mode->extra_param[1]);
    }
    if (play_mode->open_output() < 0)
    {
	ctl->cmsg(CMSG_FATAL, VERB_NORMAL,
		  "Couldn't open %s (`%c')",
		  play_mode->id_name, play_mode->id_character);
	output_fail = 1;
	ctl->close();
	return 2;
    }

    if (!control_ratio)
    {
	control_ratio = play_mode->rate / CONTROLS_PER_SECOND;
	if (control_ratio < 1)
	    control_ratio = 1;
	else if (control_ratio > MAX_CONTROL_RATIO)
	    control_ratio = MAX_CONTROL_RATIO;
    }

    init_load_soundfont();
    if (!output_fail)
    {
	aq_setup();
	timidity_init_aq_buff();
    }
    if (allocate_cache_size > 0)
	resamp_cache_reset();

    if (def_prog >= 0)
	set_default_program(def_prog);
    if (*def_instr_name)
	set_default_instrument(def_instr_name);

 return 0;
}

int timiwp_play_main_close(void)
{
    if (intr)
	aq_flush(1);
    play_mode->close_output();
    ctl->close();
    DeleteCriticalSection(&critSect);
    free_archive_files();

    return 0;
}



