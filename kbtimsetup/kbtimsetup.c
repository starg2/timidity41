


#include "timidity.c"

#include <tchar.h>
#include "w32g.h"
#include "w32g_res.h"
#include "w32g_utl.h"
#include "w32g_pref.h"


// エラー回避用 (w32g_c.c, w32g_i.c, w32g_ut2.c 等の代替
int PlayerLanguage = LANGUAGE_JAPANESE; //LANGUAGE_ENGLISH;
int IniFileAutoSave = 1;
char *IniFile;
char *ConfigFile;
char *PlaylistFile;
char *PlaylistHistoryFile;
char *MidiFileOpenDir;
char *ConfigFileOpenDir;
char *PlaylistFileOpenDir;
int SecondMode = 0;
BOOL PosSizeSave = TRUE;
int DocMaxSize;
char *DocFileExt;
int AutoloadPlaylist = 0;
int AutosavePlaylist = 0;
int SeachDirRecursive = 0;
int DocWndIndependent = 0;
int DocWndAutoPopup = 0;
int TraceGraphicFlag;
int PlayerThreadPriority;
int MidiPlayerThreadPriority;
int MainThreadPriority;
int GUIThreadPriority;
int TracerThreadPriority;
int WrdThreadPriority;
int SubWindowMax = 6;
int InitMinimizeFlag = 0;
int main_panel_update_time = 10;
int DebugWndStartFlag = 1;
int ConsoleWndStartFlag = 0;
int ListWndStartFlag = 0;
int TracerWndStartFlag = 0;
int DocWndStartFlag = 0;
int WrdWndStartFlag = 0;
int SoundSpecWndStartFlag = 0;
int DebugWndFlag = 1;
int ConsoleWndFlag = 1;
int ListWndFlag = 1;
int TracerWndFlag = 0;
int DocWndFlag = 1;
int WrdWndFlag = 0;
int SoundSpecWndFlag = 0;
int WrdGraphicFlag;
int TraceGraphicFlag;
char *w32g_output_dir = NULL;
int w32g_auto_output_mode = 0;
int w32g_lock_open_file = 0;
WRDTracer w32g_wrdt_mode;
int playlist_max = 1;
int playlist_max_ini = 1;
int ConsoleClearFlag = 0;
// HWND
HWND hMainWnd = 0;
HWND hDebugWnd = 0;
HWND hConsoleWnd = 0;
HWND hTracerWnd = 0;
HWND hDocWnd = 0;
HWND hListWnd = 0;
HWND hWrdWnd = 0;
HWND hSoundSpecWnd = 0;
HWND hDebugEditWnd = 0;
HWND hDocEditWnd = 0;
HWND hUrlWnd = 0;
// Process.
HANDLE hProcess = 0;
// Main Thread.
HANDLE hMainThread = 0;
HANDLE hPlayerThread = 0;
HANDLE hMainThreadInfo = 0;
DWORD dwMainThreadID = 0;

void TracerWndApplyQuietChannel(ChannelBitMask quietchannels_){}
void w32g_restart(void){}
void w32g_send_rc(int rc, ptr_size_t value){}
void w32g_i_init(void){}
void w32g_ext_control_main_thread(int rc, ptr_size_t value){}

// Control funcitons
static int ctl_open(int using_stdin, int using_stdout){}
static void ctl_close(void){}
static int ctl_pass_playing_list(int number_of_files, char *list_of_files[]){}
static void ctl_event(CtlEvent *e){}
static int ctl_read(ptr_size_t *valp){}
static int cmsg(int type, int verbosity_level, char *fmt, ...){}
#define ctl w32gui_control_mode
#define CTL_STATUS_UPDATE -98
ControlMode ctl=
{
    "Win32 GUI interface", 'w',
    "w32gui",
    1,1,0,
    CTLF_AUTOSTART | CTLF_DRAG_START,
    ctl_open,
    ctl_close,
    ctl_pass_playing_list,
    ctl_read,
    NULL,
    cmsg,
    ctl_event
};


// ここからsetupのメイン部分

char IniPath[FILEPATH_MAX] = "";


// w32_utl.c で使用
void get_ini_path(char *ini)
{
	if(ini)
		strcpy(ini, (const char *)&IniPath);
}


// timidity_start_initialize()
void config_gui_start_initialize(void)
{
	int i;
    static int drums[] = DEFAULT_DRUMCHANNELS;
    static int is_first = 1;

#if defined(__FreeBSD__) && !defined(__alpha__)
    fp_except_t fpexp;
#elif defined(__NetBSD__) || defined(__OpenBSD__)
    fp_except fpexp;
#endif
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    fpexp = fpgetmask();
    fpsetmask(fpexp & ~(FP_X_INV|FP_X_DZ));
#endif
    if(!output_text_code)
		output_text_code = safe_strdup(OUTPUT_TEXT_CODE);
    if(!opt_aq_max_buff)
		opt_aq_max_buff = safe_strdup("5.0");
    if(!opt_aq_fill_buff)
		opt_aq_fill_buff = safe_strdup("100%");	
    if(!opt_reduce_voice_threshold)
		opt_reduce_voice_threshold = safe_strdup("75%");
    if(!opt_reduce_quality_threshold)
		opt_reduce_quality_threshold = safe_strdup("99%");
    if(!opt_reduce_polyphony_threshold)
		opt_reduce_polyphony_threshold = safe_strdup("85%");

    /* Check the byte order */
    i = 1;
#ifdef LITTLE_ENDIAN
    if(*(char *)&i != 1)
#else
    if(*(char *)&i == 1)
#endif
    {
		fprintf(stderr, "Byte order is miss configured.\n");
		exit(1);
    }
    for(i = 0; i < MAX_CHANNELS; i++)
		memset(&(channel[i]), 0, sizeof(Channel));
    CLEAR_CHANNELMASK(quietchannels);
    CLEAR_CHANNELMASK(default_drumchannels);
    for(i = 0; drums[i] > 0; i++)
		SET_CHANNELMASK(default_drumchannels, drums[i] - 1);
#if MAX_CHANNELS > 16
    for(i = 16; i < MAX_CHANNELS; i++)
		if(IS_SET_CHANNELMASK(default_drumchannels, i & 0xF))
			SET_CHANNELMASK(default_drumchannels, i);
#endif
    if(program_name == NULL)
		program_name = "TiMidity";
    uudecode_unquote_html = 1;
    for(i = 0; i < MAX_CHANNELS; i++){
		default_program[i] = DEFAULT_PROGRAM;
		special_program[i] = -1;
		memset(channel[i].drums, 0, sizeof(channel[i].drums));
    }
    if(play_mode == NULL)
		play_mode = &null_play_mode;

}

// main() start
void config_gui_main(void)
{
#if defined(TIMIDITY_LEAK_CHECK)
	_CrtSetDbgFlag(CRTDEBUGFLAGS);
#endif
	OverrideSFSettingLoad();
#if defined(VST_LOADER_ENABLE)
	if (hVSTHost == NULL) {		
		// ini(=kpi)のディレクトリにある timvstwrap.dll
		int i, last = 0;
		char WrapPath[FILEPATH_MAX] = "";
		for(i = 0; i < FILEPATH_MAX; i++){
			if(IniPath[i] == '\0')
				break;
			else if(IniPath[i] == '\\')
				last = i;
		}
		if(last){
			int j = 0;
			for(i = 0; i < last; i++){
				WrapPath[j++] = IniPath[i];
			}
			WrapPath[j] = '\0';
#ifdef _WIN64
			strcat(WrapPath, "\\timvstwrap_x64.dll\0");
#else
			strcat(WrapPath, "\\timvstwrap.dll\0");
#endif
		}
		if(WrapPath[0] != 0)
			hVSTHost = LoadLibrary(WrapPath);
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
#if defined(__W32__) && !defined(WINDRV)
	(void)w32_reset_dll_directory();
#endif
	// timidity_start_initialize()
	config_gui_start_initialize();
}

// main() end
void config_gui_main_close(void)
{
	int i;
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
#ifdef IA_DYNAMIC
	safe_free(dynamic_lib_root);
	dynamic_lib_root = NULL;
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
	free_freq_data();
	free_wrd();
	free_readmidi();
	free_playmidi();
	free_mix_c();
	free_global_mblock();
	tmdy_free_config();
	//free_reverb_buffer();
	free_effect_buffers();
	free_int_synth();
	free_voices();
	uninitialize_resampler_coeffs();
	for (i = 0; i < MAX_CHANNELS; i++)
		free_drum_effect(i);
#ifdef VST_LOADER_ENABLE
	if (hVSTHost != NULL) {
		// only load , no save
		((vst_close)GetProcAddress(hVSTHost,"vstClose"))();
		FreeLibrary(hVSTHost);
		hVSTHost = NULL;
	}
#endif
}




HINSTANCE hInst = NULL;
HWND hConfigWnd = NULL;

void set_config_hwnd(HWND hwnd)
{
	if(hwnd)
		hConfigWnd = hwnd;
}

// Create Window
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	HICON *hIcon;

	hInst = hInstance;
	hIcon = (HICON *)LoadImage(hInst, MAKEINTRESOURCE(IDI_ICON_TIMIDITY), IMAGE_ICON, 16, 16, 0);
	if ( hConfigWnd != NULL ) {
		DestroyWindow ( hConfigWnd );
		hConfigWnd = NULL;
	}	
	// Create Window
	PrefWndCreate(NULL, 0); // set hConfigWnd	
	if (hIcon!=NULL)
		SendMessage(hConfigWnd, WM_SETICON,FALSE, (LPARAM)hIcon);
	{  // Set the title of the main window again.
   		char buffer[256];
   		SendMessage( hConfigWnd, WM_GETTEXT, (WPARAM)255, (LPARAM)buffer);
   		SendMessage( hConfigWnd, WM_SETTEXT, (WPARAM)0, (LPARAM)buffer);
	}
	if (!hConfigWnd)
		return FALSE;
	ShowWindow(hConfigWnd, nCmdShow);
	UpdateWindow(hConfigWnd);
	return TRUE;
}

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	static int init_flg = 0;
	MSG msg;
	HACCEL hAccelTable;
	
	if(init_flg)
		return FALSE;
	init_flg = 1;
	if(lpCmdLine[0] == 0){
		// 空の場合 kbtimsetup.exeのディレクトリにある kbtim.ini
		char buffer[FILEPATH_MAX] = {0};
		char *p;
		HMODULE hInst = GetModuleHandle(0);		
		if(GetModuleFileName(hInst, buffer, FILEPATH_MAX - 1)){
			if((p = pathsep_strrchr(buffer)) != NULL){
				p++;
				*p = '\0';
			}else{
				buffer[0] = '.';
				buffer[1] = PATH_SEP;
				buffer[2] = '\0';
			}
		}else{
			buffer[0] = '.';
			buffer[1] = PATH_SEP;
			buffer[2] = '\0';
		}
		strncpy(IniPath, buffer, FILEPATH_MAX);
		IniPath[FILEPATH_MAX - 1] = '\0';
		strcat(IniPath,"kbtim.ini");
	}else{
		// ini取得 プラグインとファイル名が同じで拡張子が INI	
		int i, last = 0;
		for(i = 0; i < FILEPATH_MAX; i++){
			if(lpCmdLine[i] == '\0')
				break;
			else if(lpCmdLine[i] == '.')
				last = i;
		}
		if(last){
			int j = 0;
			for(i = 0; i < last; i++){
				if(lpCmdLine[i] != '"')
					IniPath[j++] = lpCmdLine[i];
			}
			IniPath[j] = '\0';
			strcat(IniPath, ".ini\0");
		}
	}
	if (!InitInstance (hInstance, nCmdShow)) // Create Window
		return FALSE;
#if 0 // accel
	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CONFIG_GUI));	
	while (GetMessage(&msg, NULL, 0, 0)){ // message loop
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)){
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
#else
	while( GetMessage(&msg,NULL,0,0) ){ // message loop
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
#endif
	return (int) msg.wParam;
}


