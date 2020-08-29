/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

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
*/

#ifndef ___W32G_UTL_H_
#define ___W32G_UTL_H_

#ifdef IA_W32G_SYN
#ifndef MAX_PORT
#define MAX_PORT 4
#endif
#endif

///r
#include "timidity.h"
#include "w32g.h"
#include <shtypes.h>

// ini & config
#define IniVersion "2.2"
typedef struct SETTING_PLAYER_ {
// Main Window
	int InitMinimizeFlag;
	int main_panel_update_time;
// SubWindow Starting Create Flag
	int DebugWndStartFlag;
	int ConsoleWndStartFlag;
	int ListWndStartFlag;
	int TracerWndStartFlag;
	int DocWndStartFlag;
	int WrdWndStartFlag;
	int SoundSpecWndStartFlag;
// SubWindow Starting Valid Flag
	int DebugWndFlag;
	int ConsoleWndFlag;
	int ListWndFlag;
	int TracerWndFlag;
	int DocWndFlag;
	int WrdWndFlag;
	int SoundSpecWndFlag;
// SubWindow Max Numer
	int SubWindowMax;
// Default File
	char ConfigFile[FILEPATH_MAX];
	char PlaylistFile[FILEPATH_MAX];
	char PlaylistHistoryFile[FILEPATH_MAX];
	int PlaylistMax;
// Default Dir
	char MidiFileOpenDir[FILEPATH_MAX];
	char ConfigFileOpenDir[FILEPATH_MAX];
	char PlaylistFileOpenDir[FILEPATH_MAX];
// Thread Priority
	int PlayerThreadPriority;
	int GUIThreadPriority;
// Font
	char SystemFont[LF_FULLFACESIZE + 1];
	char PlayerFont[LF_FULLFACESIZE + 1];
	char WrdFont[LF_FULLFACESIZE + 1];
	char DocFont[LF_FULLFACESIZE + 1];
	char ListFont[LF_FULLFACESIZE + 1];
	char TracerFont[LF_FULLFACESIZE + 1];
	int SystemFontSize;
	int PlayerFontSize;
	int WrdFontSize;
	int DocFontSize;
	int ListFontSize;
	int TracerFontSize;
// Misc.
	int WrdGraphicFlag;
	int TraceGraphicFlag;
	int DocMaxSize;
	char DocFileExt[256];
	int ConsoleClearFlag;
// End.
	int PlayerLanguage;
	int DocWndIndependent; 
	int DocWndAutoPopup; 
	int SeachDirRecursive;
	int IniFileAutoSave;
	int SecondMode;
  int AutoloadPlaylist;
  int AutosavePlaylist;
  int PosSizeSave;
// End.
} SETTING_PLAYER;

typedef struct SETTING_TIMIDITY_ {
    // Parameter from command line options.

    int32 amplification;	// A
    int32 output_amplification; // --master-volume
    int antialiasing_allowed;	// a
    int buffer_fragments,	// B
        audio_buffer_bits;      // BXX,?
    int32 control_ratio;	// C
				// c (ignore)
    ChannelBitMask default_drumchannels, default_drumchannel_mask; // D
				// d (ignore)

				// E...
    int opt_modulation_wheel;	// E w/W
    int opt_portamento;		// E p/P
    int opt_nrpn_vibrato;  	// E v/V
    int opt_channel_pressure;	// E s/S
    int opt_trace_text_meta_event; // E t/T
    int opt_overlap_voice_allow;// E o/O
///r
	int opt_overlap_voice_count; //
	int opt_max_channel_voices;
    int opt_default_mid;	// E mXX
	int opt_system_mid;		// E MXX
    int default_tonebank;	// E b
    int special_tonebank;	// E B
    int opt_resample_type;  // E Fresamp
    int opt_reverb_control;	// E Freverb
    int opt_chorus_control;	// E Fchorus
    int noise_sharp_type;	// E Fns
	int opt_surround_chorus; // E ?
	int opt_normal_chorus_plus; // E ?
	int opt_tva_attack;			// E ?
	int opt_tva_decay;			// E ?
	int opt_tva_release;		// E ?
	int opt_delay_control;		// E ?
	int opt_default_module;		// --module
	int opt_lpf_def;			// E ?
	int opt_hpf_def;			// long opt
	int opt_drum_effect;			// E ?
	int opt_modulation_envelope;			// E ?
	int opt_eq_control;			// E ?
	int opt_insertion_effect;	// E ?
    int opt_evil_mode;		// e
    int adjust_panning_immediately; // F
    int opt_fast_decay;		// f
#ifdef SUPPORT_SOUNDSPEC
    int view_soundspec_flag;	// g
    FLOAT_T spectrogram_update_sec; // g
#endif
				// h (ignore)
    int default_program[MAX_CHANNELS]; // i
    int special_program[MAX_CHANNELS]; // I

    char opt_ctl[30];		// i ?
    int opt_realtime_playing;	// j
///r
	char reduce_voice_threshold[16]; // k
	char reduce_quality_threshold[16]; // l
				// L (ignore)
    int min_sustain_time;       // m
    int opt_resample_param;     // N
///r	
	char reduce_polyphony_threshold[16]; //n
    char opt_playmode[16];	// O
    char OutputName[FILEPATH_MAX]; // o : string
    char OutputDirName[FILEPATH_MAX]; // o : string
	int auto_output_mode;
				// P (ignore)
    int voices;			// p
    int auto_reduce_polyphony;  // pa
    ChannelBitMask quietchannels; // Q
    int temper_type_mute;	// Q
    char opt_qsize[16];		// q
    int32 modify_release;	// R
    int32 allocate_cache_size;	// S
	int32 opt_drum_power;	// ?
	int32 opt_amp_compensation;	// ?
	int key_adjust;		// K
	int8 opt_force_keysig;	// H
	int opt_pure_intonation;	// Z
	int8 opt_init_keysig;	// Z
    int output_rate;		// s
    char output_text_code[16];	// t
    int free_instruments_afterwards; // U
///r
	double opt_user_volume_curve; //V
    char opt_wrd[16];		// W
    int opt_print_fontname;
///r
	int compute_buffer_bits;	// Y

	int32 wmme_device_id;
	int wave_format_ext; 
	int32 wasapi_device_id;
	int32 wasapi_latency;
	int wasapi_format_ext;
	int wasapi_exclusive;
	int wasapi_polling;
	int wasapi_priority;
	int wasapi_stream_category;
	int wasapi_stream_option;
	int32 wdmks_device_id;
	int32 wdmks_latency;
	int wdmks_format_ext;
	int wdmks_polling;
	int wdmks_thread_priority;
	int wdmks_rt_priority;
	int wdmks_pin_priority;
	int32 pa_wmme_device_id;
	int32 pa_ds_device_id;
	int32 pa_asio_device_id;
	int32 pa_wdmks_device_id;
	int32 pa_wasapi_device_id;
	int pa_wasapi_flag;
	int pa_wasapi_stream_category;
	int pa_wasapi_stream_option;
	int add_play_time;		// --add-play-time
	int add_silent_time;	// --add-silent-time
	int emu_delay_time;		// --emu-delay-time
    int opt_resample_filter;     //
    int opt_resample_over_sampling;  // 
	int opt_pre_resamplation;

	int opt_mix_envelope;
	int opt_modulation_update;
	int opt_cut_short_time;
	int opt_limiter;
    int opt_use_midi_loop_repeat; //
    int opt_midi_loop_repeat;

#if defined(__W32__) && defined(SMFCONV)
    int opt_rcpcv_dll;		// wr, wR
#endif
				// x (ignore)
				// Z (ignore)
    /* for w32g_a.c */
    //int data_block_bits;
    //int data_block_num;
    int wmme_buffer_bits;
    int wmme_buffer_num;

//??    int waveout_data_block_size;
#if defined(WINDRV_SETUP)
		DWORD syn_ThreadPriority;
		int SynShTime;
		uint32 opt_rtsyn_latency;	// --rtsyn-latency
		int opt_rtsyn_skip_aq;	// --rtsyn-skip-aq
#elif defined(IA_W32G_SYN)
		int SynIDPort[MAX_PORT];
		int syn_AutoStart;
//		DWORD processPriority;
		DWORD syn_ThreadPriority;
		int SynPortNum;
		int SynShTime;
		uint32 opt_rtsyn_latency;	// --rtsyn-latency
		int opt_rtsyn_skip_aq;	// --rtsyn-skip-aq
		int opt_use_twsyn_bridge;
#endif
	int processPriority;		// --process-priority
	int compute_thread_num;
	
	int trace_mode_update_time;
	int opt_load_all_instrument;
	
    /* for INT_SYNTH */
	int32 opt_int_synth_sine;
	int32 opt_int_synth_rate;
	int32 opt_int_synth_update;

} SETTING_TIMIDITY;

// #### obsoleted
#define PLAYERMODE_AUTOQUIT				0x0001
#define PLAYERMODE_AUTOREFINE				0x0002
#define PLAYERMODE_AUTOUNIQ				0x0004
#define PLAYERMODE_NOT_CONTINUE			0x0008
#define PLAYERMODE_NOT_DRAG_START	0x0010


extern char *OutputName;

extern void LoadIniFile(SETTING_PLAYER *sp,  SETTING_TIMIDITY *st);
extern void SaveIniFile(SETTING_PLAYER *sp,  SETTING_TIMIDITY *st);

extern SETTING_PLAYER *sp_default, *sp_current, *sp_temp;
extern SETTING_TIMIDITY *st_default, *st_current, *st_temp;
extern CHAR *INI_INVALID;
extern CHAR *INI_SEC_PLAYER;
extern CHAR *INI_SEC_TIMIDITY;
extern char *SystemFont;
extern char *PlayerFont;
extern char *WrdFont;
extern char *DocFont;
extern char *ListFont;
extern char *TracerFont;
extern HFONT hSystemFont;
extern HFONT hPlayerFont;
extern HFONT hWrdFont;
extern HFONT hDocFont;
extern HFONT hListFont;
extern HFONT hTracerFont;
extern int SystemFontSize;
extern int PlayerFontSize;
extern int WrdFontSize;
extern int DocFontSize;
extern int ListFontSize;
extern int TracerFontSize;
///r
extern int IniGetKeyInt64(char *section, char *key,int64 *n);
extern int IniGetKeyInt32(char *section, char *key,int32 *n);
extern int IniGetKeyInt32Array(char *section, char *key, int32 *n, int arraysize);
extern int IniGetKeyInt(char *section, char *key, int *n);
extern int IniGetKeyInt8(char *section, char *key, int8 *n);
extern int IniGetKeyChar(char *section, char *key, char *c);
extern int IniGetKeyIntArray(char *section, char *key, int *n, int arraysize);
extern int IniGetKeyString(char *section, char *key,char *str);
extern int IniGetKeyStringN(char *section, char *key,char *str, int size);
extern int IniGetKeyFloat(char *section, char *key, FLOAT_T *n);
extern int IniPutKeyInt64(char *section, char *key,int64 *n);
extern int IniPutKeyInt32(char *section, char *key,int32 *n);
extern int IniPutKeyInt32Array(char *section, char *key, int32 *n, int arraysize);
extern int IniPutKeyInt(char *section, char *key, int *n);
extern int IniPutKeyInt8(char *section, char *key, int8 *n);
extern int IniPutKeyChar(char *section, char *key, char *c);
extern int IniPutKeyIntArray(char *section, char *key, int *n, int arraysize);
extern int IniPutKeyString(char *section, char *key, char *str);
extern int IniPutKeyStringN(char *section, char *key, char *str, int size);
extern int IniPutKeyFloat(char *section, char *key, FLOAT_T n);
extern void ApplySettingPlayer(SETTING_PLAYER *sp);
extern void SaveSettingPlayer(SETTING_PLAYER *sp);
extern void ApplySettingTiMidity(SETTING_TIMIDITY *st);
extern void SaveSettingTiMidity(SETTING_TIMIDITY *st);
extern void SettingCtlFlag(SETTING_TIMIDITY *st, int opt_id, int onoff);
extern int IniVersionCheck(void);
extern void BitBltRect(HDC dst, HDC src, const RECT *rc);

// mode option for ShowFileDialog()
enum {
	FILEDIALOG_OPEN_FILE,
	FILEDIALOG_OPEN_MULTIPLE_FILES,
	FILEDIALOG_OPEN_FOLDER,
	FILEDIALOG_SAVE_FILE
};

// pDirectory and pResult must point to an array of at least FILEPATH_MAX length!
extern BOOL ShowFileDialog(
	int mode,
	HWND hParentWindow,
	LPCWSTR pTitle,
	char *pDirectory,
	char *pResult,
	UINT filterCount,
	const COMDLG_FILTERSPEC *pFilters
);

#if 0
extern TmColors tm_colors[ /* TMCC_SIZE */ ];
#define TmCc(c) (tm_colors[c].color)
extern void TmInitColor(void);
extern void TmFreeColor(void);
extern void TmFillRect(HDC hdc, RECT *rc, int color);
#endif
extern void w32g_uninitialize(void);
extern void w32g_initialize(void);
extern int is_directory(const char *path);
extern int directory_form(char *path_in_out);
extern int is_last_path_sep(const char *path);

extern char *timidity_window_inifile;
extern char *timidity_output_inifile;
extern char *timidity_history_inifile;

#endif /* ___W32G_UTL_H_ */
