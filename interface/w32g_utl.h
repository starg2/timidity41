#ifndef ___W32G_UTL_H_
#define ___W32G_UTL_H_

// playlist
#define PLAYLIST_DATAMAX 512
typedef volatile struct PLAYLIST_ {
	volatile struct PLAYLIST_ *prev;
	volatile struct PLAYLIST_ *next;
	volatile char filename[PLAYLIST_DATAMAX+1];
	volatile char title[PLAYLIST_DATAMAX+1];
	volatile int opt;
} PLAYLIST;

#define prev_playlist(x) ((x)->prev)
#define next_playlist(x) ((x)->next)
#define proceed_playlist(x) { (x) = (x)->next; }
#define back_playlist(x) { (x) = (x)->prev; }

// misc tools
#define LotationBufferMAX 1024


// ini & config
typedef struct SETTING_PLAYER_ {
// Main Window
	int InitMinimizeFlag;
// SubWindow Starting Create Flag
	int DebugWndStartFlag;
	int ConsoleWndStartFlag;
	int ListWndStartFlag;
	int TracerWndStartFlag;
	int DocWndStartFlag;
	int WrdWndStartFlag;
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
	char ConfigFile[MAXPATH + 32];
	char PlaylistFile[MAXPATH + 32];
	char PlaylistHistoryFile[MAXPATH + 32];
// Default Dir
	char MidiFileOpenDir[MAXPATH + 32];
	char ConfigFileOpenDir[MAXPATH + 32];
	char PlaylistFileOpenDir[MAXPATH + 32];
// Thread Priority
	int ProcessPriority;
	int PlayerThreadPriority;
	int MidiPlayerThreadPriority;
	int MainThreadPriority;
	int TracerThreadPriority;
	int WrdThreadPriority;
// Font
	char SystemFont[256];
	char PlayerFont[256];
	char WrdFont[256];
	char DocFont[256];
	char ListFont[256];
	char TracerFont[256];
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
// End.
} SETTING_PLAYER;

typedef struct SETTING_TIMIDITY_ {
	int free_instruments_afterwards;	// U : 0 or 1
  // L : pathlist
	// c : configfile
  ChannelBitMask quietchannels;		// Q : set_channel_flag()
	int effect_lr_mode;		// b : 0,1,2,-1
	int effect_lr_delay_msec;	// > 0

  ChannelBitMask drumchannels;			// D : set_channel_flag()
	ChannelBitMask drumchannel_mask;
  ChannelBitMask default_drumchannels;			// D : set_channel_flag()
	ChannelBitMask default_drumchannel_mask;
  // d : dynamic lib
  // O : set_play_mode()
  char play_mode_id_character;
  int play_mode_encoding;
  // PE_ULAW, PE_ALAW, PE_16BIT, PE_MONO, PE_SIGNED, PE_BYTESWAP
 //		  case 'U': /* uLaw */
 //		    pmp->encoding |= PE_ULAW;
	//	    pmp->encoding &= ~(PE_ALAW|PE_16BIT);
	//	    break;
	//	  case 'A': /* aLaw */
	//	    pmp->encoding |= PE_ALAW;
	//	    pmp->encoding &= ~(PE_ULAW|PE_16BIT);
	//	    break;
	//	  case 'l': /* linear */
	//	    pmp->encoding &= ~(PE_ULAW|PE_ALAW);
	//	    break;
	//	  case '1': /* 1 for 16-bit */
	//	    pmp->encoding |= PE_16BIT;
	//	    pmp->encoding &= ~(PE_ULAW|PE_ALAW);
	//	    break;
	//	  case '8': pmp->encoding &= ~PE_16BIT; break;
	//	  case 'M': pmp->encoding |= PE_MONO; break;
	//	  case 'S': pmp->encoding &= ~PE_MONO; break; /* stereo */
	//	  case 's': pmp->encoding |= PE_SIGNED; break;
	//	  case 'u': pmp->encoding &= ~PE_SIGNED; break;
	//	  case 'x': pmp->encoding ^= PE_BYTESWAP; break; /* toggle */
	//---------------------------------
  //		8-bit sample width
  //		16-bit sample width
  //		8-bit sample & U-Law encoding
  //		8-bit sample & A-Law encoding
  //		8-bit sample & linear encoding
  //		16-bit sample & linear encoding
	//---------------------------------
	//   `8'    8-bit sample width
	//   `1'    16-bit sample width
	//---------------------------------
 	//  `U'    U-Law encoding
	//   `A'    A-Law encoding
	//   `l'    linear encoding
	//---------------------------------
	//   `M'    monophonic
	//   `S'    stereo
	//---------------------------------
	//   `s'    signed output
	//   `u'    unsigned output
	//   `x'    byte-swapped output
  //---------------------------------
  char pmp_id_character;		// id_character
	//  -Od     Win32 audio driver
	//  -Ow     RIFF WAVE file
	//  -Or     raw waveform data
	//  -Ou     Sun audio file
	//  -Oa     AIFF file
	// pmp->id_character, pmp->id_name
  char OutputName[MAXPATH + 32]; // o : string
  int antialiasing_allowed;		// a : 0,1
	int fast_decay;			// f : 0,1
  int adjust_panning_immediately;		// F : 0,1
  int output_rate;		// s :
// if x < 100 then x = x * 1000.0 + 0.5
// set_value(&opt_output_rate, x , MIN_OUTPUT_RATE,MAX_OUTPUT_RATE,
//	     "Resampling frequency");
// P : set overriding instrument
	int default_program[MAX_CHANNELS];
// I : set_default_prog()
  int32 amplification;  // A :
// set_value(&amplification, x , 0, MAX_AMPLIFICATION,
//				"Amplification");
	int32 control_ratio;		// C :
// set_value(&control_ratio, atoi(optarg), 1, MAX_CONTROL_RATIO,
//     "Control ratio");
	int voices;		// p :
// set_value(&tmpi32, atoi(optarg), 1, MAX_VOICES, "Polyphony");
	int32 modify_release;		// R :
// set_value(&modify_release, tmpi32, 0, MRELS - 1, "Modify Release");
// modify_release++;
	int view_soundspec_flag;		// g : 0,1
	double spectrogram_update_sec;
// atof(x)
  int32 allocate_cache_size;		// S :
	char allocate_chace_size_string[64];
// ???[kK] -> x * 1024.0
// ???[M] -> x * 1024.0 * 1024.0
	int ctl_verbosity;		// cmp->verbosity
	char ctl_id_character;		// cmp->id_character
	int ctl_trace_playing;		// cmp->trace_playing
// i : set_ctl()
	int buffer_fragments;		// B :
// set_value(&tmpi32, x, 0, 1000, "Buffer fragments");
// must be >=3
	char output_text_code[64];	// t : must be "noconv" or "sjis"
// E : set_extensin_modes()
  int opt_modulation_wheel;	// w/W :
  int opt_portamento;				// p/P
 	int opt_nrpn_vibrato;  		// v/V
  int opt_reverb_control;  	// r/R
	int opt_chorus_control;		// c/C
	int opt_chorus_control2[MAX_CHANNELS];
//  case 'c':
//    if('0' <= *(flag + 1) && *(flag + 1) <= '9')
//    {
//	opt_chorus_control = (atoi(flag + 1) & 0x7f);
//	while('0' <= *(flag + 1) && *(flag + 1) <= '9')
//	    flag++;
//    }
//    else
//	opt_chorus_control = 1;
//    break;
  int opt_channel_pressure;		// s/S
	int opt_trace_text_meta_event;		// t/T
	int opt_overlap_voice_allow;		// o/O
	int opt_default_mid;		// mXX

 /* FIXME?? int[]?? */
#if 1
	int opt_default_mid_str[64];
#else
	char opt_default_mid_str[64];
#endif

//	if(strcmp(flag + 1, "gs") == 0 ||
//	   strcmp(flag + 1, "GS") == 0)
//	    val = 0x41;
//	else if(strcmp(flag + 1, "xg") == 0 ||
//		strcmp(flag + 1, "XG") == 0)
//	    val = 0x43;
//	else if(strcmp(flag + 1, "gm") == 0 ||
//		strcmp(flag + 1, "GM") == 0)
//	    val = 0x7e;
	int default_tonebank;		// b
	int	special_tonebank;	//	B : -1 or XX
  int opt_realtime_playing;		// j : 0,1
	// w : set_win_modes()
	int opt_rcpcv_dll;
  // W : set_wrd()  must be '-' or 'W'
	char wrdt_id;
	char wrdt_open_opts[64];
  // h : help
	int noise_sharp_type;		// n : 0,1,2,3,4
} SETTING_TIMIDITY;

extern char *OutputName;

void LoadIniFile(SETTING_PLAYER *sp,  SETTING_TIMIDITY *st);
void SaveIniFile(SETTING_PLAYER *sp,  SETTING_TIMIDITY *st);

extern SETTING_PLAYER *sp_default, *sp_current, *sp_temp;
extern SETTING_TIMIDITY *st_default, *st_current, *st_temp;

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


/* Auto-generated by hh2h.rb script from this line. */

PLAYLIST * new_playlist(void);
PLAYLIST * del_playlist(PLAYLIST *playlist);
void del_all_playlist(PLAYLIST *playlist);
PLAYLIST * first_playlist(PLAYLIST *playlist);
PLAYLIST * last_playlist(PLAYLIST *playlist);
PLAYLIST * insert_playlist(PLAYLIST *playlist, PLAYLIST *prev, PLAYLIST *next);
void prev_insert_playlist(PLAYLIST *p1, PLAYLIST *p2);
void next_insert_playlist(PLAYLIST *p1, PLAYLIST *p2);
void first_insert_playlist(PLAYLIST *p1, PLAYLIST *p2);
void last_insert_playlist(PLAYLIST *p1, PLAYLIST *p2);
PLAYLIST * last_insert_playlist_filename(PLAYLIST *playlist, char *filename);
PLAYLIST * first_insert_playlist_filename(PLAYLIST *playlist, char *filename);
PLAYLIST * dup_a_playlist_merely(PLAYLIST *playlist);
PLAYLIST * dup_a_playlist(PLAYLIST *playlist);
PLAYLIST * dup_playlist(PLAYLIST *playlist);
void reverse_playlist(PLAYLIST *playlist);
int num_before_playlist(PLAYLIST *pl);
int num_after_playlist(PLAYLIST *pl);
int num_with_playlist(PLAYLIST *pl);
int isinteger(char *str);
char * GetLotationBuffer(void);
int IniGetKeyInt32(char *section, char *key,int32 *n);
int IniGetKeyInt32Array(char *section, char *key, int32 *n, int arraysize);
int IniGetKeyInt(char *section, char *key, int *n);
int IniGetKeyChar(char *section, char *key, char *c);
int IniGetKeyIntArray(char *section, char *key, int *n, int arraysize);
int IniGetKeyString(char *section, char *key,char *str);
int IniGetKeyStringN(char *section, char *key,char *str, int size);
int IniGetKeyFloat(char *section, char *key, FLOAT_T *n);
int IniPutKeyInt32(char *section, char *key,int32 *n);
int IniPutKeyInt32Array(char *section, char *key, int32 *n, int arraysize);
int IniPutKeyInt(char *section, char *key, int *n);
int IniPutKeyChar(char *section, char *key, char *c);
int IniPutKeyIntArray(char *section, char *key, int *n, int arraysize);
int IniPutKeyString(char *section, char *key, char *str);
int IniPutKeyStringN(char *section, char *key, char *str, int size);
int IniPutKeyFloat(char *section, char *key, FLOAT_T n);
int SetFlag(int flag);
long SetValue(int32 value, int32 min, int32 max);
void ApplySettingPlayer(SETTING_PLAYER *sp);
void SaveSettingPlayer(SETTING_PLAYER *sp);
void ApplySettingTimidity(SETTING_TIMIDITY *st);
void SaveSettingTimidity(SETTING_TIMIDITY *st);
void w32g_initialize(void);
void w32g_init_ctl(void);
void w32g_reset_ctl(void);
void w32g_finish_ctl(void);

/* Auto-generated by hh2h.rb script to this line. */


#endif /* ___W32G_UTL_H_ */
