#ifndef ___W32G_H_
#define ___W32G_H_

#include <windows.h>

#ifndef MAXPATH
#define MAXPATH 256
#endif /* MAXPATH */

#define RC_EXT_BASE 1000
enum {
    RC_EXT_DROP = RC_EXT_BASE,
    RC_EXT_LOAD_FILE,
    RC_EXT_MODE_CHANGE,
    RC_EXT_APPLY_SETTING,
    RC_EXT_DELETE_PLAYLIST,
    RC_EXT_UPDATE_PLAYLIST,
};

#define W32G_TIMIDITY_CFG "C:\\WINDOWS\\TIMIDITY.CFG"
#define MAX_W32G_MIDI_CHANNELS	32

#define FLAG_NOTE_OFF	1
#define FLAG_NOTE_ON	2

#define FLAG_BANK	0x0001
#define FLAG_PROG	0x0002
#define FLAG_PAN	0x0004
#define FLAG_SUST	0x0008


#define TMCCC_BLACK	RGB(0x00,0x00,0x00)
#define TMCCC_WHITE	RGB(0xff,0xff,0xff)
#define TMCCC_RED	RGB(0xff,0x00,0x00)

#define TMCCC_FORE	TMCCC_BLACK // Aliased
#define TMCCC_BACK 	RGB(0x00, 0xf0, 0x00)
#define TMCCC_LOW	RGB(0x80, 0xd0, 0x00)
#define TMCCC_MIDDLE	RGB(0xb0, 0xb0, 0x00)
#define TMCCC_HIGH	RGB(0xe0, 0x00, 0x00)

enum {
    TMCC_BLACK, // Aliased FORE
    TMCC_WHITE,
    TMCC_RED,
    TMCC_BACK,
    TMCC_LOW,
    TMCC_MIDDLE,
    TMCC_HIGH,
    TMCC_FORE_HALF,
    TMCC_LOW_HALF,
    TMCC_MIDDLE_HALF,
    TMCC_HIGH_HALF,
    TMCC_FORE_WEAKHALF,
    TMCC_SIZE
};
#define TMCC_FORE TMCC_BLACK // Aliased

typedef struct _TmColors {
    COLORREF color;
    HPEN pen;
    HBRUSH brush;
} TmColors;

// Canvas Modes
enum {
    TMCM_SLEEP,
    TMCM_CHANNEL,
    TMCM_TRACER
// TMCM_32CHANNEL
// TMCM_FREQUENCY
};


#include "w32g_utl.h"


/* w32g_i.c */
extern int w32g_open(void);
extern void w32g_close(void);
extern void w32g_send_rc(int rc, int32 value);
extern int w32g_get_rc(int32 *value, int wait_if_empty);
extern void w32g_lock(void);
extern void w32g_unlock(void);
extern void MainWndScrollbarProgressUpdate(int sec);
extern void PutsConsoleWnd(char *str);
extern void w32g_ctle_play_start(int sec);
extern void SettingWndApply(void);
extern int w32g_lock_open_file;
extern void w32g_i_init();
extern HINSTANCE hInst;


/* w32g_utl.c */

/* w32g_playlist.c */
void w32g_add_playlist(int nfiles, char **files, int expand_flag);
char **w32g_get_playlist(int *nfiles);
extern int w32g_next_playlist(void);
extern int w32g_prev_playlist(void);
extern void w32g_first_playlist(void);
extern int w32g_isempty_playlist(void);
extern char *w32g_curr_playlist(void);
extern void w32g_update_playlist(void);
extern void w32g_get_playlist_index(int *selected, int *nfiles, int *cursel);
extern int w32g_goto_playlist(int num);
extern void w32g_delete_playlist(int pos);
extern int w32g_valid_playlist(void);
extern void w32g_setcur_playlist(void);

/* w32g_panel.c */
extern void w32g_init_panel(HWND hwnd);
extern void TmPanelStartToLoad(char *filename);
extern void TmPanelStartToPlay(int total_sec);
extern void TmPanelSetVoices(int v);
//extern void TmPanelInit(HWND hwnd);
extern void TmPanelRefresh(void);
extern void TmPanelSetTime(int sec);
extern void TmPanelSetMasterVol(int v);
extern void TmPanelUpdateList(void);

/* w32g_canvas.c */
extern void w32g_init_canvas(HWND hwnd);
extern void TmCanvasRefresh(void);
extern void TmCanvasReset(void);
extern void TmCanvasNote(int status, int ch, int note, int vel);
extern int TmCanvasChange(void);
extern int TmCanvasMode;

/* w32g_c.c */
extern int w32g_play_active;
extern int w32g_current_volume[/* MAX_CHANNELS */];
extern int w32g_current_expression[/* MAX_CHANNELS */];





// flags
extern int InitMinimizeFlag;
extern int DebugWndStartFlag;
extern int ConsoleWndStartFlag;
extern int ListWndStartFlag;
extern int TracerWndStartFlag;
extern int DocWndStartFlag;
extern int WrdWndStartFlag;
extern int DebugWndFlag;
extern int ConsoleWndFlag;
extern int ListWndFlag;
extern int TracerWndFlag;
extern int DocWndFlag;
extern int WrdWndFlag;
extern int SoundSpecWndFlag;

extern int SubWindowMax;

extern char *IniFile;
extern char *ConfigFile;
extern char *PlaylistFile;
extern char *PlaylistHistoryFile;
extern char *MidiFileOpenDir;
extern char *ConfigFileOpenDir;
extern char *PlaylistFileOpenDir;

extern int PlayerThreadPriority;
extern int GUIThreadPriority;

extern int WrdGraphicFlag;
extern int TraceGraphicFlag;
extern int DocMaxSize;
extern char *DocFileExt;

extern int w32g_has_ini_file;

#endif
