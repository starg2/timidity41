/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2018 Masanao Izumo <iz@onicos.co.jp>
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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    w32g_c.c: written by Daisuke Aoki <dai@y7.net>
                         Masanao Izumo <iz@onicos.co.jp>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */
#include <stdarg.h>
#include "timidity.h"
#include "common.h"
#include "output.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "controls.h"
#include "miditrace.h"
#include "strtab.h"
#include "aq.h"
#include "timer.h"

#include "w32g.h"
#include "w32g_subwin.h"
#include "w32g_utl.h"

#ifdef TIMW32G_USE_NEW_CONSOLE
#include "w32g_new_console.h"
#endif

extern int CanvasGetMode(void);
extern void CanvasUpdate(int flag);
extern void CanvasReadPanelInfo(int flag);
extern void CanvasPaint(void);
extern void CanvasPaintAll(void);
extern void CanvasReset(void);
extern void CanvasClear(void);
extern void MPanelPaintAll(void);
extern void MPanelReadPanelInfo(int flag);
extern void MPanelReset(void);
extern void MPanelUpdate(void);
extern void MPanelUpdateAll(void);
extern void MPanelPaint(void);
extern int is_directory(const char *path);
extern int directory_form(char *buffer);
extern int TracerWndDrawSkip;

volatile int w32g_play_active;
volatile int w32g_restart_gui_flag = 0;
int w32g_current_volume[MAX_CHANNELS];
int w32g_current_expression[MAX_CHANNELS];
static int mark_apply_setting = 0;
PanelInfo *Panel = NULL;
static void CanvasUpdateInterval(void);
static void ctl_panel_refresh(int);
static void AddStartupMessage(const char *message);
static void ShowStartupMessage(void);

char *w32g_output_dir = NULL;
int w32g_auto_output_mode = 0;

int main_panel_update_time = 10; // ms
static double update_interval_time = 0.01;

extern void MPanelMessageAdd(const char *message, int msec, int mode);
extern void MPanelMessageClearAll(void);

extern int w32g_msg_box(char *message, char *title, int type);


//****************************************************************************/
// EXT_CONTROL_THREAD funcitons

#ifdef EXT_CONTROL_THREAD
static int thread_exit = 0, thread_finish = 1, thread_init = 0;
static HANDLE hEventTcv;
static HANDLE hThread = NULL;
static int rc_thread = 0;
static ptr_size_t value_thread = 0;
static void w32g_ext_control_sub_thread(int rc, ptr_size_t value);

static unsigned __stdcall w32g_ext_control_thread(void *param)
{
    for (;;) {
        WaitForSingleObject(hEventTcv, INFINITE); // スレッド開始イベント待機
        if (thread_exit) break;
        w32g_ext_control_sub_thread(rc_thread, value_thread);
        ResetEvent(hEventTcv); // スレッド開始イベントリセット
        thread_finish = 1; // スレッド終了フラグセット
    }

    return 0;
}

static void w32g_uninit_ext_control_thread(void)
{
    DWORD status;

    thread_exit = 1;
    if (hThread == NULL)
        return;
    SetEvent(hEventTcv);
    switch (WaitForSingleObject(hThread, 10)) {
    case WAIT_OBJECT_0:
        break;
    case WAIT_TIMEOUT:
        status = WaitForSingleObject(hThread, 1000);
        if (status == WAIT_TIMEOUT)
            TerminateThread(hThread, 0);
        break;
    default:
        TerminateThread(hThread, 0);
        break;
    }
    CloseHandle(hThread);
    hThread = NULL;
    if (hEventTcv != NULL) {
        CloseHandle(hEventTcv);
        hEventTcv = NULL;
    }
    thread_exit = 0;
    thread_finish = 1;
    thread_init = 0;
}

static void w32g_init_ext_control_thread(void)
{
    DWORD ThreadID;

    if (thread_init)
        w32g_uninit_ext_control_thread();
    thread_init = 1;
    hEventTcv = CreateEvent(NULL,FALSE,FALSE,NULL); // reset manual
    if (hEventTcv == NULL)
        return;
    hThread = crt_beginthreadex(NULL, 0, w32g_ext_control_thread, 0, 0, &ThreadID);
    if (hThread == NULL)
        return;
}

static int w32g_go_ext_control_thread(int rc, ptr_size_t value)
{
    if (hThread == NULL) {
        w32g_ext_control_sub_thread(rc, value);
        return RC_NONE;
    }
    if (!thread_finish)
        return RC_NONE;
    rc_thread = rc;
    value_thread = value;
    thread_finish = 0; // スレッド終了フラグリセット
    SetEvent(hEventTcv); // スレッド開始イベントセット (再開)
    return RC_NONE;
}

int w32g_working_ext_control_thread(void)
{
    return !thread_finish;
}
#endif

//****************************************************************************/
// Control funcitons

static int ctl_open(int using_stdin, int using_stdout);
static void ctl_close(void);
static int ctl_pass_playing_list(int number_of_files, char *list_of_files[]);
static void ctl_event(CtlEvent *e);
static int ctl_read(ptr_size_t *valp);
static int cmsg(int type, int verbosity_level, const char *fmt, ...);

#define ctl w32gui_control_mode

#define CTL_STATUS_UPDATE -98

ControlMode ctl =
{
    "Win32 GUI interface", 'w',
    "w32gui",
    1, 1, 0,
    CTLF_AUTOSTART | CTLF_DRAG_START,
    ctl_open,
    ctl_close,
    ctl_pass_playing_list,
    ctl_read,
    NULL,
    cmsg,
    ctl_event
};

static uint32 cuepoint = 0;
static int cuepoint_pending = 0;

#ifdef FORCE_TIME_PERIOD
static TIMECAPS tcaps;
#endif

static int ctl_open(int using_stdin, int using_stdout)
{
    (void) using_stdin;
    (void) using_stdout;

    if (ctl.opened)
        return 0;
    ctl.opened = 1;
    set_trace_loop_hook(CanvasUpdateInterval);
    ShowStartupMessage();

    /* Initialize Panel */
    Panel = (PanelInfo*) safe_malloc(sizeof(PanelInfo));
    ZeroMemory(Panel, sizeof(PanelInfo));
    Panel->changed = 1;

//#ifdef FORCE_TIME_PERIOD
//  timeGetDevCaps(&tcaps, sizeof(TIMECAPS));
//  timeBeginPeriod(tcaps.wPeriodMin);
//#endif

#ifdef EXT_CONTROL_THREAD
    w32g_init_ext_control_thread();
#endif
    return w32g_open();
}

static void ctl_close(void)
{
    if (ctl.opened) {
        w32g_close();
        ctl.opened = 0;
        safe_free(Panel);
        Panel = NULL;

#ifdef TIMW32G_USE_NEW_CONSOLE
        ClearNewConsoleBuffer();
#endif

#ifdef TIMW32G_USE_NEW_CONSOLE
	ClearNewConsoleBuffer();
#endif

//#ifdef FORCE_TIME_PERIOD
//      timeEndPeriod(tcaps.wPeriodMin);
//#endif
#ifdef EXT_CONTROL_THREAD
        w32g_uninit_ext_control_thread();
#endif
    }
}

static void PanelReset(void)
{
    int i, j;

    if (main_panel_update_time > 2000)
        main_panel_update_time = 2000;
    else if (main_panel_update_time < 0)
        main_panel_update_time  = 0;
    update_interval_time = (double) main_panel_update_time * DIV_1000;

    Panel->reset_panel = 0;
    Panel->multi_part = 0;
    Panel->wait_reset = 0;
    Panel->cur_time = 0;
    Panel->cur_time_h = 0;
    Panel->cur_time_m = 0;
    Panel->cur_time_s = 0;
    Panel->cur_time_ss = 0;
    for (i = 0; i < MAX_W32G_MIDI_CHANNELS; i++) {
        Panel->v_flags[i] = 0;
        Panel->cnote[i] = 0;
        Panel->cvel[i] = 0;
        Panel->ctotal[i] = 0;
        Panel->c_flags[i] = 0;
        for (j = 0; j < 4; j++)
            Panel->xnote[i][j] = 0;
//      Panel->channel[i].panning = 64;
        Panel->channel[i].panning = -1;
        Panel->channel[i].sustain = 0;
        Panel->channel[i].expression = 0;
        Panel->channel[i].volume = 0;
//      Panel->channel[i].pitchbend = 0x2000;
        Panel->channel[i].pitchbend = -2;
    }
    Panel->titlename[0] = '\0';
    Panel->filename[0] = '\0';
    Panel->titlename_setflag = 0;
    Panel->filename_setflag = 0;
    Panel->cur_voices = 0;
    Panel->voices = voices;
    Panel->upper_voices = 0;
  //Panel->master_volume = 0;
    Panel->meas = 0;
    Panel->beat = 0;
    Panel->keysig[0] = '\0';
    Panel->key_offset = 0;
    Panel->tempo = 0;
    Panel->tempo_ratio = 0;
    Panel->aq_ratio = 0;
    for (i = 0; i < 16; i++) {
        for (j = 0; j < 16; j++) {
            Panel->GSLCD[i][j] = 0;
        }
    }
    Panel->gslcd_displayed_flag = 0;
    Panel->changed = 1;
}

#define GS_LCD_CLEAR_TIME 2.88

static void ctl_gslcd_update(void)
{
    double t;
    int i, j;
    t = get_current_calender_time();
    if (t - Panel->gslcd_last_display_time > GS_LCD_CLEAR_TIME && Panel->gslcd_displayed_flag) {
        for (i = 0; i < 16; i++) {
            for (j = 0; j < 16; j++) {
                Panel->GSLCD[i][j] = 0;
            }
        }
        CanvasClear();
        Panel->gslcd_displayed_flag = 0;
    }
}

static void CanvasUpdateInterval(void)
{
    static double lasttime = 0.;
    double t;

    if (CanvasGetMode() == CANVAS_MODE_MAP16
            || CanvasGetMode() == CANVAS_MODE_MAP32
            || CanvasGetMode() == CANVAS_MODE_MAP64) {
        t = get_current_calender_time();
        if (t - lasttime > 0.01) {
            CanvasReadPanelInfo(0);
            CanvasUpdate(0);
            CanvasPaint();
            lasttime = t;
        }
    } else if (CanvasGetMode() == CANVAS_MODE_GSLCD)
        ctl_gslcd_update();
}

static int ctl_drop_file(HDROP hDrop)
{
    StringTable st;
    int i, n, len;
    TCHAR tbuffer[FILEPATH_MAX];
    char buffer[FILEPATH_MAX];
    char **files;
    int prevnfiles;

    w32g_get_playlist_index(NULL, &prevnfiles, NULL);

    init_string_table(&st);
    n = DragQueryFile(hDrop,0xffffffffL, NULL, 0);
    for(i = 0; i < n; i++)
    {
	DragQueryFile(hDrop, i, tbuffer, ARRAY_SIZE(tbuffer));
	char *s = tchar_to_char(tbuffer);
	strcpy(buffer, s);
	safe_free(s);
	if(is_directory(buffer))
	    directory_form(buffer);
	len = strlen(buffer);
	put_string_table(&st, buffer, strlen(buffer));
    }
    DragFinish(hDrop);

    if ((files = make_string_array(&st)) == NULL)
        n = 0;
    else {
        n = w32g_add_playlist(n, files, 1,
                              ctl.flags & CTLF_AUTOUNIQ,
                              ctl.flags & CTLF_AUTOREFINE);
        safe_free(files[0]);
        safe_free(files);
    }
    if (n > 0) {
        ctl_panel_refresh(1);
        if (ctl.flags & CTLF_DRAG_START) {
            w32g_goto_playlist(prevnfiles, !(ctl.flags & CTLF_NOT_CONTINUE));
            return RC_LOAD_FILE;
        }
    }
    return RC_NONE;
}

static int ctl_load_file(const char *fileptr)
{
    StringTable st;
    int len, n;
    char **files;
    char buffer[FILEPATH_MAX];
    const char *basedir;

    init_string_table(&st);
    n = 0;
    basedir = fileptr;
    fileptr += strlen(fileptr) + 1;
    while (*fileptr) {
        buffer[FILEPATH_MAX - 1] = '\0';
        snprintf(buffer, ARRAY_SIZE(buffer), "%s\\%s", basedir, fileptr);
        if (is_directory(buffer))
            directory_form(buffer);
        len = strlen(buffer);
        put_string_table(&st, buffer, len);
        n++;
        fileptr += strlen(fileptr) + 1;
    }

    if (n == 0) {
        put_string_table(&st, basedir, strlen(basedir));
        n++;
    }

    files = make_string_array(&st);
    n = w32g_add_playlist(n, files, 1,
                          ctl.flags & CTLF_AUTOUNIQ,
                          ctl.flags & CTLF_AUTOREFINE);
    safe_free(files[0]);
    safe_free(files);

    if (n > 0)
        ctl_panel_refresh(1);
    w32g_lock_open_file = 0;
    return RC_NONE;
}

static int ctl_load_files_and_play(argc_argv_t *argc_argv, int playflag)
{
    StringTable st;
    int i, n, len;
    char buffer[FILEPATH_MAX];
    char **files;
    int prevnfiles;

    if (argc_argv==NULL)
        return RC_NONE;

    w32g_get_playlist_index(NULL, &prevnfiles, NULL);

    init_string_table(&st);
    n = argc_argv->argc;
    for (i = 0; i < n; i++) {
        strncpy(buffer, (argc_argv->argv)[i], FILEPATH_MAX - 1);
        buffer[FILEPATH_MAX - 1] = '\0';
        if (is_directory(buffer))
            directory_form(buffer);
        len = strlen(buffer);
        put_string_table(&st, buffer, strlen(buffer));
    }
#if 1
    for (i = 0; i < argc_argv->argc; i++) {
        safe_free(argc_argv->argv[i]);
    }
    safe_free(argc_argv->argv);
    argc_argv->argv = NULL;
    argc_argv->argc = 0;
#endif
    if ((files = make_string_array(&st)) == NULL)
        n = 0;
    else {
        n = w32g_add_playlist(n, files, 1,
                              ctl.flags & CTLF_AUTOUNIQ,
                              ctl.flags & CTLF_AUTOREFINE);
        safe_free(files[0]);
        safe_free(files);
    }
    if (n > 0) {
        ctl_panel_refresh(1);
        if (playflag) {
            w32g_goto_playlist(prevnfiles, !(ctl.flags & CTLF_NOT_CONTINUE));
            return RC_LOAD_FILE;
        }
    }
    return RC_NONE;
}

static int ctl_load_playlist(const char *fileptr)
{
    StringTable st;
    int n;
    char **files;
    char buffer[FILEPATH_MAX];
    const char *basedir;

    init_string_table(&st);
    n = 0;
    basedir = fileptr;
    fileptr += strlen(fileptr) + 1;
    while (*fileptr) {
        buffer[FILEPATH_MAX - 1] = '\0';
        snprintf(buffer, ARRAY_SIZE(buffer), "@%s\\%s", basedir, fileptr);
        put_string_table(&st, buffer, strlen(buffer));
        n++;
        fileptr += strlen(fileptr) + 1;
    }

    if (n == 0) {
        buffer[0] = '@';
        strncpy(buffer + 1, basedir, ARRAY_SIZE(buffer) - 1);
        put_string_table(&st, buffer, strlen(buffer));
        n++;
    }

    files = make_string_array(&st);
    n = w32g_add_playlist(n, files, 1,
                          ctl.flags & CTLF_AUTOUNIQ,
                          ctl.flags & CTLF_AUTOREFINE);
    safe_free(files[0]);
    safe_free(files);

    if (n > 0)
        ctl_panel_refresh(1);
    w32g_lock_open_file = 0;
    return RC_NONE;
}

static int ctl_save_playlist(const char *fileptr)
{
    FILE *fp;
    int i, nfiles;

    if ((fp = fopen(fileptr, "w")) == NULL) {
        w32g_lock_open_file = 0;
        cmsg(CMSG_FATAL, VERB_NORMAL, "%s: %s", fileptr, strerror(errno));
        w32g_lock_open_file = 0;
        return RC_NONE;
    }

    w32g_get_playlist_index(NULL, &nfiles, NULL);
    for (i = 0; i < nfiles; i++) {
        fputs(w32g_get_playlist(i), fp);
        fputs("\n", fp);
    }

    fclose(fp);
    w32g_lock_open_file = 0;
    return RC_NONE;
}

static int ctl_delete_playlist(int offset)
{
    int selected, nfiles, cur, pos;

#ifdef LISTVIEW_PLAYLIST
    if (offset == 0) {
        int flg = w32g_delete_playlist(-1); // select multi
        if (flg) {
            ctl_panel_refresh(1);
            if (w32g_play_active && flg > 1) { // flg>1 delete selected
                ctl_panel_refresh(1);
                return RC_LOAD_FILE;
            }
        }
        return RC_NONE;
    }
#endif
    w32g_get_playlist_index(&selected, &nfiles, &cur);
    pos = cur + offset;
    if (pos < 0 || pos >= nfiles)
        return RC_NONE;
    if (w32g_delete_playlist(pos)) {
        w32g_update_playlist();
        if (w32g_play_active && selected == pos) {
            ctl_panel_refresh(1);
            return RC_LOAD_FILE;
        }
    }
    return RC_NONE;
}

static int ctl_copycut_playlist(int mode)
{
    int flg = 0;

#ifdef LISTVIEW_PLAYLIST
    w32g_copy_playlist();
    if (mode)
        flg = w32g_delete_playlist(-1); // select multi
    if (flg) {
        ctl_panel_refresh(1);
        if (w32g_play_active && flg > 1) { // flg>1 delete selected
            ctl_panel_refresh(1);
            return RC_LOAD_FILE;
        }
    }
    return RC_NONE;
#endif
}

static int ctl_paste_playlist(void)
{
#ifdef LISTVIEW_PLAYLIST
    w32g_paste_playlist(ctl.flags & CTLF_AUTOUNIQ, ctl.flags & CTLF_AUTOREFINE);
    ctl_panel_refresh(1);
    return RC_NONE;
#endif
}

static int ctl_uniq_playlist(void)
{
    int n, stop;
    n = w32g_uniq_playlist(&stop);
    if (n > 0) {
        ctl_panel_refresh(1);
        if (stop)
            return RC_STOP;
    }
    return RC_NONE;
}

static int ctl_refine_playlist(void)
{
    int n, stop;
    n = w32g_refine_playlist(&stop);
    if (n > 0) {
        ctl_panel_refresh(1);
        if (stop)
            return RC_STOP;
    }
    return RC_NONE;
}

/*
w32g_ext_ext_controlの問題
再生中のプレイリスト操作等で音切れになる
プレイリスト操作のときw32g_ext_ext_control()はバッファ生成compute_data()/バッファ転送aq_soft_flush()に関わる部分で呼び出される
プレイリスト操作は処理時間が長いこともあるので その場合音切れになる
そこでプレイヤスレッドから切り離して 最小のプレイコントロールだけ発行する
EXT_CONTROL_MAIN_THREAD GUIのメインスレッドで処理する  (変更箇所多い 処理時間が長いときMAIN_THREADが固まるけど・・
EXT_CONTROL_THREAD コントロール用スレッドを追加し処理する (変更箇所は少ない スレッドのムダ？
*/

#if defined(EXT_CONTROL_THREAD) || defined(EXT_CONTROL_MAIN_THREAD)
#if defined(EXT_CONTROL_THREAD)
static void w32g_ext_control_sub_thread(int rc, ptr_size_t value)
#elif defined(EXT_CONTROL_MAIN_THREAD)
void w32g_ext_control_main_thread(int rc, ptr_size_t value)
#endif
{
    int rrc = RC_NONE;

    switch (rc) {
    case RC_EXT_PLAYLIST_CTRL:
        w32g_set_playlist_ctrl(value);
        break;
    case RC_EXT_DROP:
        if ((ctl.flags & CTLF_DRAG_START))
            w32g_set_playlist_ctrl_play();
        rrc = ctl_drop_file((HDROP) value);
        break;
    case RC_EXT_LOAD_FILE:
        rrc = ctl_load_file((char*) value);
        break;
    case RC_EXT_LOAD_FILES_AND_PLAY:
        w32g_set_playlist_ctrl_play();
        rrc = ctl_load_files_and_play((argc_argv_t*) value, 1);
        break;
    case RC_EXT_LOAD_PLAYLIST:
        rrc = ctl_load_playlist((char*) value);
        break;
    case RC_EXT_SAVE_PLAYLIST:
        rrc = ctl_save_playlist((char*) value);
        break;
    case RC_EXT_MODE_CHANGE:
        CanvasChange(value);
        break;
    case RC_EXT_APPLY_SETTING:
        if (w32g_play_active) {
            mark_apply_setting = 1;
            rrc = RC_STOP;
            break;
        }
        PrefSettingApplyReally();
        mark_apply_setting = 0;
        break;
    case RC_EXT_DELETE_PLAYLIST:
        rrc = ctl_delete_playlist(value);
        break;
    case RC_EXT_COPYCUT_PLAYLIST:
        rrc = ctl_copycut_playlist(value);
        break;
    case RC_EXT_PASTE_PLAYLIST:
        rrc = ctl_paste_playlist();
        break;
    case RC_EXT_UPDATE_PLAYLIST:
        w32g_update_playlist();
        break;
    case RC_EXT_UNIQ_PLAYLIST:
        rrc = ctl_uniq_playlist();
        break;
    case RC_EXT_REFINE_PLAYLIST:
        rrc = ctl_refine_playlist();
        break;
    case RC_EXT_JUMP_FILE:
        TracerWndDrawSkip = 1;
        if (w32g_goto_playlist(value, !(ctl.flags & CTLF_NOT_CONTINUE)))
            rrc = RC_LOAD_FILE;
        break;
    case RC_EXT_ROTATE_PLAYLIST:
        w32g_rotate_playlist(value);
        ctl_panel_refresh(1);
        break;
    case RC_EXT_CLEAR_PLAYLIST:
        w32g_clear_playlist();
        if (w32g_is_playlist_ctrl_play()) { // 再生中のプレイリストなら停止
            ctl_panel_refresh(1);
            rrc = RC_STOP;
        }
        break;
    case RC_EXT_OPEN_DOC:
        w32g_setup_doc(value);
        w32g_open_doc(0);
        break;
    }
    //
    switch (rrc) {
    case RC_LOAD_FILE:
        w32g_send_rc(RC_LOAD_FILE, 0);
        break;
    case RC_STOP:
        w32g_send_rc(RC_STOP, 0);
        break;
    }
}
#endif

#if defined(EXT_CONTROL_THREAD)
#if defined(EXT_CONTROL_MAIN_THREAD)
void w32g_ext_control_main_thread(int rc, ptr_size_t value)
{
    if (w32g_working_ext_control_thread())
        return;
    switch (rc) {
    case RC_EXT_PLAYLIST_CTRL:
        w32g_set_playlist_ctrl(value);
        break;
    case RC_EXT_LOAD_PLAYLIST:
        ctl_load_playlist((char*) value);
        break;
    case RC_EXT_SAVE_PLAYLIST:
        ctl_save_playlist((char*) value);
        break;
    default:
        w32g_go_ext_control_thread(rc, value);
        break;
    }
}
#else
static int w32g_ext_control(int rc, ptr_size_t value)
{
    if (w32g_working_ext_control_thread())
        return;
    switch (rc) {
    case RC_EXT_PLAYLIST_CTRL:
        w32g_set_playlist_ctrl(value);
        break;
    case RC_EXT_LOAD_PLAYLIST:
        ctl_load_playlist((char*) value);
        break;
    case RC_EXT_SAVE_PLAYLIST:
        ctl_save_playlist((char*) value);
        break;
    default:
        return w32g_go_ext_control_thread(rc, value);
    }
    return RC_NONE;
}
#endif

#elif defined(EXT_CONTROL_MAIN_THREAD)
static int w32g_ext_control(int rc, ptr_size_t value)
{
    return RC_NONE;
}

#else
static int w32g_ext_control(int rc, ptr_size_t value)
{
    switch (rc) {
    case RC_EXT_PLAYLIST_CTRL:
        w32g_set_playlist_ctrl(value);
        break;
    case RC_EXT_DROP:
        if ((ctl.flags & CTLF_DRAG_START))
            w32g_set_playlist_ctrl_play();
        return ctl_drop_file((HDROP)value);
    case RC_EXT_LOAD_FILE:
        return ctl_load_file((char*) value);
    case RC_EXT_LOAD_FILES_AND_PLAY:
        w32g_set_playlist_ctrl_play();
        return ctl_load_files_and_play((argc_argv_t*) value, 1);
    case RC_EXT_LOAD_PLAYLIST:
        return ctl_load_playlist((char*) value);
    case RC_EXT_SAVE_PLAYLIST:
        return ctl_save_playlist((char*) value);
    case RC_EXT_MODE_CHANGE:
        CanvasChange(value);
        break;
    case RC_EXT_APPLY_SETTING:
        if (w32g_play_active) {
            mark_apply_setting = 1;
            return RC_STOP;
        }
        PrefSettingApplyReally();
        mark_apply_setting = 0;
        break;
    case RC_EXT_DELETE_PLAYLIST:
        return ctl_delete_playlist(value);
    case RC_EXT_COPYCUT_PLAYLIST:
        rrc = ctl_copycut_playlist(value);
        break;
    case RC_EXT_PASTE_PLAYLIST:
        rrc = ctl_paste_playlist();
        break;
    case RC_EXT_UPDATE_PLAYLIST:
        w32g_update_playlist();
        break;
    case RC_EXT_UNIQ_PLAYLIST:
        return ctl_uniq_playlist();
    case RC_EXT_REFINE_PLAYLIST:
        return ctl_refine_playlist();
    case RC_EXT_JUMP_FILE:
        TracerWndDrawSkip = 1;
        if (w32g_goto_playlist(value, !(ctl.flags & CTLF_NOT_CONTINUE)))
        return RC_LOAD_FILE;
        break;
    case RC_EXT_ROTATE_PLAYLIST:
        w32g_rotate_playlist(value);
        ctl_panel_refresh(1);
        break;
    case RC_EXT_CLEAR_PLAYLIST:
        w32g_clear_playlist();
        if (w32g_is_playlist_ctrl_play()) { // 再生中のプレイリストなら停止
            ctl_panel_refresh(1);
            return RC_STOP;
        }
        break;
    case RC_EXT_OPEN_DOC:
        w32g_setup_doc(value);
        w32g_open_doc(0);
        break;
    }
    return RC_NONE;
}
#endif // EXT_CONTROL_


static int ctl_read(ptr_size_t *valp)
{
    int rc;
    ptr_size_t buf;
    if (cuepoint_pending) {
        *valp = cuepoint;
        cuepoint_pending = 0;
        return RC_FORWARD;
    }
    rc = w32g_get_rc(&buf, play_pause_flag);
    *valp = (ptr_size_t) buf;
#ifndef EXT_CONTROL_MAIN_THREAD
    if (rc >= RC_EXT_BASE)
        return w32g_ext_control(rc, buf);
#endif
    return rc;
}

static int cmsg(int type, int verbosity_level, const char *fmt, ...)
{
    char buffer[BUFSIZ];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buffer, ARRAY_SIZE(buffer), fmt, ap);
    va_end(ap);

    if (type == CMSG_TEXT) {
        MPanelMessageClearAll();
        MPanelMessageAdd(buffer, 2000, 0);
    }

    if ((type == CMSG_TEXT || type == CMSG_INFO || type == CMSG_WARNING) &&
       ctl.verbosity < verbosity_level)
        return 0;
    if (type == CMSG_ERROR)
        AddStartupMessage(buffer);
    else if (type == CMSG_FATAL) {
        ShowStartupMessage();
        w32g_msg_box(buffer, "TiMidity Error", MB_OK);
    }
#ifdef TIMW32G_USE_NEW_CONSOLE
    NewConsoleBufferWriteCMsg(type, verbosity_level, buffer);
#else
    PutsConsoleWnd(buffer);
    PutsConsoleWnd("\n");
#endif
    return 0;
}

static char *startup_message = NULL, *startup_message_tail;

static void AddStartupMessage(const char *message)
{
    static int remaining;
    int length;
    if (ctl.opened)
        return;
    length = strlen(message);
    if (startup_message == NULL) {
        remaining = 2048;   /* simple */
        startup_message = malloc(remaining);
        if (startup_message == NULL)
            return;
        startup_message_tail = startup_message;
        remaining--;
    }
    length = min(length, remaining);
    strncpy(startup_message_tail, message, length);
    remaining -= length;
    startup_message_tail += length;
    if (remaining > 0) {
        *startup_message_tail++ = '\n';
        remaining--;
    }
}

static void ShowStartupMessage(void)
{
    if (startup_message == NULL)
        return;
    if (startup_message != startup_message_tail) {
        *startup_message_tail = '\0';
        w32g_msg_box(startup_message, "TiMidity Error", MB_OK);
    }
    safe_free(startup_message);
    startup_message = NULL;
}

static void ctl_panel_refresh(int flag)
{
    static double lasttime = 0.;
    const double t = get_current_calender_time();

    if (!flag && (t - lasttime < update_interval_time))
        return;
    lasttime = t;
    MPanelReadPanelInfo(0);
    MPanelUpdate();
    MPanelPaint();
}

static void ctl_master_volume(int mv)
{
    Panel->master_volume = mv;
    Panel->changed = 1;
    ctl_panel_refresh(0);
}

static void ctl_metronome(int meas, int beat)
{
    static int lastmeas = CTL_STATUS_UPDATE;
    static int lastbeat = CTL_STATUS_UPDATE;

    if (meas == CTL_STATUS_UPDATE)
        meas = lastmeas;
    else
        lastmeas = meas;
    if (beat == CTL_STATUS_UPDATE)
        beat = lastbeat;
    else
        lastbeat = beat;
    Panel->meas = meas;
    Panel->beat = beat;
    Panel->changed = 1;

    ctl_panel_refresh(0);
}

static void ctl_keysig(int8 k, int ko)
{
    static int8 lastkeysig = CTL_STATUS_UPDATE;
    static int lastoffset = CTL_STATUS_UPDATE;
    static const char *keysig_name[] = {
        "Cb", "Gb", "Db", "Ab", "Eb", "Bb", "F ", "C ",
        "G ", "D ", "A ", "E ", "B ", "F#", "C#", "G#",
        "D#", "A#"
    };
    int i, j;

    if (k == CTL_STATUS_UPDATE)
        k = lastkeysig;
    else
        lastkeysig = k;
    if (ko == CTL_STATUS_UPDATE)
        ko = lastoffset;
    else
        lastoffset = ko;
    i = k + ((k < 8) ? 7 : -6);
    if (ko > 0)
        for (j = 0; j < ko; j++)
            i += (i > 10) ? -5 : 7;
    else
        for (j = 0; j < abs(ko); j++)
            i += (i < 7) ? 5 : -7;
    sprintf(Panel->keysig, "%s %s", keysig_name[i], (k < 8) ? "Maj" : "Min");
    Panel->key_offset = ko;
    Panel->changed = 1;
    ctl_panel_refresh(0);
}

static void ctl_tempo(int t, int tr)
{
    static int lasttempo = CTL_STATUS_UPDATE;
    static int lastratio = CTL_STATUS_UPDATE;

    if (t == CTL_STATUS_UPDATE)
        t = lasttempo;
    else
        lasttempo = t;
    if (tr == CTL_STATUS_UPDATE)
        tr = lastratio;
    else
        lastratio = tr;
    t = (int) (500000 / (double) t * 120 * (double) tr / 100 + 0.5);
    Panel->tempo = t;
    Panel->tempo_ratio = tr;
    Panel->changed = 1;
    ctl_panel_refresh(0);
}

///r
extern BOOL SetWrdWndActive(void);
static int ctl_pass_playing_list(int number_of_files, char *list_of_files[])
{
    static int init_flag = 1;
    int rc;
    ptr_size_t value;
    extern void timidity_init_aq_buff(void);
    int errcnt;

    w32g_add_playlist(number_of_files, list_of_files, 0,
                      ctl.flags & CTLF_AUTOUNIQ,
                      ctl.flags & CTLF_AUTOREFINE);
    w32g_play_active = 0;
    errcnt = 0;

    if (init_flag && w32g_nvalid_playlist() && (ctl.flags & CTLF_AUTOSTART))
//  if (play_mode->fd != -1 &&
//          w32g_nvalid_playlist() && (ctl.flags & CTLF_AUTOSTART))
        rc = RC_LOAD_FILE;
    else
        rc = RC_NONE;
    init_flag = 0;

#ifdef W32G_RANDOM_IS_SHUFFLE
    w32g_shuffle_playlist_reset(0);
#endif
    while (1) {
        if (rc == RC_NONE) {
            if (play_mode->fd != -1) {
                aq_flush(1);
                play_mode->close_output();
                safe_free(play_mode->name);
                play_mode->name = NULL;
            }
            rc = w32g_get_rc(&value, 1);
        }

      redo:
        switch (rc) {
          case RC_NONE:
            Sleep(100);
            break;

          case RC_LOAD_FILE: /* Play playlist.selected */
            TracerWndDrawSkip = 0;
            if (w32g_nvalid_playlist()) {
                int selected;
                w32g_get_playlist_play_index(&selected);
                w32g_play_active = 1;
                if (play_mode->fd == -1) {
                    safe_free(play_mode->name);
        //          play_mode->name = strdup(OutputName);
                    if (w32g_auto_output_mode > 0)
                        play_mode->name = NULL;
                    else
                        play_mode->name = strdup(OutputName);
                    if (play_mode->flag & PF_PCM_STREAM) {
                        play_mode->extra_param[1] = aq_calc_fragsize();
                        ctl.cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
                                 "requesting fragment size: %d",
                                 play_mode->extra_param[1]);
                    }
                    if (play_mode->close_output)
                        play_mode->close_output();
                    if (play_mode->open_output() == -1) {
                        ctl.cmsg(CMSG_FATAL, VERB_NORMAL,
                                 "Couldn't open %s (`%c') %s",
                                 play_mode->id_name,
                                 play_mode->id_character,
                                 play_mode->name ? play_mode->name : "");
                        break;
                    }
                    aq_setup();
                    timidity_init_aq_buff();
                }
                if (play_mode->id_character == 'l')
                    w32g_show_console();
                if (ConsoleClearFlag == 0x1)
                    ClearConsoleWnd();
                if (!DocWndIndependent) {
                    w32g_setup_doc(selected);
                    if (DocWndAutoPopup)
                        w32g_open_doc(1);
                    else
                        w32g_open_doc(2);
                }
                {
                    char *p = w32g_get_playlist_play(selected);
                    if (Panel!=NULL && p!=NULL)
                        strncpy(Panel->filename, p, ARRAY_SIZE(Panel->filename));
                }

                SetWrdWndActive();

#ifdef FORCE_TIME_PERIOD
                if (timeGetDevCaps(&tcaps, sizeof(TIMECAPS)) != TIMERR_NOERROR)
                    tcaps.wPeriodMin = 10;
                timeBeginPeriod(tcaps.wPeriodMin);
#endif
                rc = play_midi_file(w32g_get_playlist_play(selected));

#ifdef FORCE_TIME_PERIOD
                timeEndPeriod(tcaps.wPeriodMin);
#endif
                if (ctl.flags & CTLF_NOT_CONTINUE)
                    w32g_update_playlist(); /* Update mark of error */
                if (rc == RC_ERROR) {
                    int nfiles;
                    errcnt++;
                    w32g_get_playlist_index(NULL, &nfiles, NULL);
                    if (errcnt >= nfiles)
                        w32g_msg_box("No MIDI file to play",
                                     "TiMidity Warning", MB_OK);
                }
                else
                    errcnt = 0;
                w32g_play_active = 0;
                goto redo;
            }
            break;

          case RC_ERROR:
          case RC_TUNE_END:
            if (play_mode->close_output)
                play_mode->close_output();
#if 0
            if (play_mode->id_character != 'd' ||
                    (ctl.flags & CTLF_NOT_CONTINUE)) {
#else
            if (ctl.flags & CTLF_NOT_CONTINUE) {
                if (ctl.flags & CTLF_LIST_LOOP) {
                    rc = RC_LOAD_FILE;
                    goto redo;
                } else
#endif
                break;
            }
            /* FALLTHROUGH */
          case RC_NEXT:
            TracerWndDrawSkip = 1;
            if (!w32g_nvalid_playlist()) {
                if (ctl.flags & CTLF_AUTOEXIT) {
                    if (play_mode->fd != -1)
                        aq_flush(0);
                    return 0;
                }
                break;
            }
            if (ctl.flags & CTLF_LIST_RANDOM) {
#ifdef W32G_RANDOM_IS_SHUFFLE
                if (w32g_shuffle_playlist_next(!(ctl.flags & CTLF_NOT_CONTINUE))) {
#else
                if (w32g_random_playlist(!(ctl.flags & CTLF_NOT_CONTINUE))) {
#endif
                    rc = RC_LOAD_FILE;
                    goto redo;
                }
            } else {
                if (w32g_next_playlist(!(ctl.flags & CTLF_NOT_CONTINUE))) {
                    rc = RC_LOAD_FILE;
                    goto redo;
                }
            }
            {
                /* end of list */
                if (ctl.flags & CTLF_AUTOEXIT) {
                    if (play_mode->fd != -1)
                        aq_flush(0);
                    return 0;
                }
                if ((ctl.flags & CTLF_LIST_LOOP) && w32g_nvalid_playlist()) {
#ifdef W32G_RANDOM_IS_SHUFFLE
                    if (ctl.flags & CTLF_LIST_RANDOM) {
                        w32g_shuffle_playlist_reset(0);
                        w32g_shuffle_playlist_next(!(ctl.flags & CTLF_NOT_CONTINUE));
                    } else {
#endif
                        w32g_first_playlist(!(ctl.flags & CTLF_NOT_CONTINUE));
#ifdef W32G_RANDOM_IS_SHUFFLE
                    }
#endif
                    rc = RC_LOAD_FILE;
                    goto redo;
                }
                if ((ctl.flags & CTLF_LIST_RANDOM) && w32g_nvalid_playlist())
                    w32g_shuffle_playlist_reset(0);
            }
            break;

          case RC_REALLY_PREVIOUS:
            TracerWndDrawSkip = 1;
#ifdef W32G_RANDOM_IS_SHUFFLE
            w32g_shuffle_playlist_reset(0);
#endif
            if (w32g_prev_playlist(!(ctl.flags & CTLF_NOT_CONTINUE))) {
                rc = RC_LOAD_FILE;
                goto redo;
            }
            break;

          case RC_QUIT:
            TracerWndDrawSkip = 1;
            if (play_mode->fd != -1)
                aq_flush(1);
            return 0;
///r
          case RC_CHANGE_VOLUME:
            output_amplification += value;
            ctl_master_volume(output_amplification);
            break;

          case RC_TOGGLE_PAUSE:
            play_pause_flag = !play_pause_flag;
            break;

          case RC_STOP:
            TracerWndDrawSkip = 1;
#ifdef W32G_RANDOM_IS_SHUFFLE
            w32g_shuffle_playlist_reset(0);
#endif
            if (play_mode->close_output)
                play_mode->close_output();
            break;

          default:
#ifndef EXT_CONTROL_MAIN_THREAD
            if (rc >= RC_EXT_BASE) {
                rc = w32g_ext_control(rc, value);
                if (rc != RC_NONE)
                    goto redo;
            }
#endif
            break;
        }

        if (mark_apply_setting) {
            PrefSettingApplyReally();
            mark_apply_setting = 0;
        }
        rc = RC_NONE;
    }
    return 0;
}

static void ctl_lcd_mark(int flag, int x, int y)
{
    Panel->GSLCD[x][y] = flag;
}

static void ctl_gslcd(int id)
{
    const char *lcd;
    int i, j, k, data, mask;
    char tmp[3];

    if ((lcd = event2string(id)) == NULL)
        return;
    if (lcd[0] != ME_GSLCD)
        return;
    lcd++;
    for (i = 0; i < 16; i++) {
        for (j = 0; j < 4; j++) {
            tmp[0] = lcd[2 * (j * 16 + i)];
            tmp[1] = lcd[2 * (j * 16 + i) + 1];
            if (sscanf(tmp, "%02X", &data) != 1) {
                /* Invalid format */
                return;
            }
            mask = 0x10;
            for (k = 0; k < 5; k++) {
                if (data & mask) { ctl_lcd_mark(1, j * 5 + k, i); }
                else { ctl_lcd_mark(0, j * 5 + k, i); }
                mask >>= 1;
            }
        }
    }
    Panel->gslcd_displayed_flag = 1;
    Panel->gslcd_last_display_time = get_current_calender_time();
    Panel->changed = 1;
}

static void ctl_channel_note(int ch, int note, int vel)
{
    if (vel == 0) {
        if (note == Panel->cnote[ch])
            Panel->v_flags[ch] = FLAG_NOTE_OFF;
        Panel->cvel[ch] = 0;
    } else if (vel > Panel->cvel[ch]) {
        Panel->cvel[ch] = vel;
        Panel->cnote[ch] = note;
        Panel->ctotal[ch] = (vel * Panel->channel[ch].volume *
                             Panel->channel[ch].expression) >> 14;
//                           Panel->channel[ch].expression / (127 * 127);
        Panel->v_flags[ch] = FLAG_NOTE_ON;
    }
    Panel->changed = 1;
}

static void ctl_note(int status, int ch, int note, int vel)
{
    int32 i, n;

    if (!ctl.trace_playing)
        return;
    if (ch < 0 || ch >= MAX_W32G_MIDI_CHANNELS)
        return;

    if (status != VOICE_ON)
        vel = 0;

    switch (status) {
    case VOICE_SUSTAINED:
    case VOICE_DIE:
    case VOICE_FREE:
    case VOICE_OFF:
        n = note;
        i = 0;
        if (n < 0) n = 0;
        if (n > 127) n = 127;
        while (n >= 32) {
            n -= 32;
            i++;
        }
        Panel->xnote[ch][i] &= ~(((int32)1) << n);
        break;
    case VOICE_ON:
        n = note;
        i = 0;
        if (n < 0) n = 0;
        if (n > 127) n = 127;
        while (n >= 32) {
            n -= 32;
            i++;
        }
        Panel->xnote[ch][i] |= ((int32)1) << n;
        break;
    }
    ctl_channel_note(ch, note, vel);
}

static void ctl_volume(int ch, int val)
{
    if (ch >= MAX_W32G_MIDI_CHANNELS)
        return;
    if (!ctl.trace_playing)
        return;

    Panel->channel[ch].volume = val;
    ctl_channel_note(ch, Panel->cnote[ch], Panel->cvel[ch]);
}

static void ctl_expression(int ch, int val)
{
    if (ch >= MAX_W32G_MIDI_CHANNELS)
        return;
    if (!ctl.trace_playing)
        return;

    Panel->channel[ch].expression = val;
    ctl_channel_note(ch, Panel->cnote[ch], Panel->cvel[ch]);
}

static void ctl_current_time(int secs, int nvoices)
{
    int32 centisecs = secs * 100;

    Panel->cur_time = centisecs;
    Panel->cur_time_h = centisecs / 100 / 60 / 60;
    centisecs %= 100 * 60 * 60;
    Panel->cur_time_m = centisecs / 100 / 60;
    centisecs %= 100 * 60;
    Panel->cur_time_s = centisecs / 100;
    centisecs %= 100;
    Panel->cur_time_ss = centisecs;
    Panel->cur_voices = nvoices;
    Panel->changed = 1;
}

static void display_aq_ratio(void)
{
    static int last_rate = -1;
    int rate, devsiz;

    if ((devsiz = aq_get_dev_queuesize()) == 0)
        return;

    rate = (int)(((double)(aq_filled() + aq_soft_filled()) / devsiz)
                 * 100 + 0.5);
    if (rate > 999)
        rate = 1000;
    Panel->aq_ratio = rate;
    if (last_rate != rate) {
        last_rate = Panel->aq_ratio = rate;
        Panel->changed = 1;
    }
}

static void ctl_total_time(int tt)
{
    int32 centisecs = tt / (play_mode->rate / 100);

    Panel->total_time = centisecs;
    Panel->total_time_h = centisecs / 100 / 60 / 60;
    centisecs %= 100 * 60 * 60;
    Panel->total_time_m = centisecs / 100 / 60;
    centisecs %= 100 * 60;
    Panel->total_time_s = centisecs / 100;
    centisecs %= 100;
    Panel->total_time_ss = centisecs;
    Panel->changed = 1;
    ctl_current_time(0, 0);
}

static void ctl_program(int ch, int val)
{
    if (ch < 0 || ch >= MAX_W32G_MIDI_CHANNELS)
        return;
    if (!ctl.trace_playing)
        return;
    if (!IS_CURRENT_MOD_FILE)
        val += progbase;

    Panel->channel[ch].program = val;
    Panel->c_flags[ch] |= FLAG_PROG;
    Panel->changed = 1;
}

static void ctl_panning(int ch, int val)
{
    if (ch >= MAX_W32G_MIDI_CHANNELS)
        return;
    if (!ctl.trace_playing)
        return;
    Panel->channel[ch].panning = val;
    Panel->c_flags[ch] |= FLAG_PAN;
    Panel->changed = 1;
}

static void ctl_sustain(int ch, int val)
{
    if (ch >= MAX_W32G_MIDI_CHANNELS)
        return;
    if (!ctl.trace_playing)
        return;
    Panel->channel[ch].sustain = val;
    Panel->c_flags[ch] |= FLAG_SUST;
    Panel->changed = 1;
}

static void ctl_pitch_bend(int ch, int val)
{
    if (ch >= MAX_W32G_MIDI_CHANNELS)
        return;
    if (!ctl.trace_playing)
        return;

    Panel->channel[ch].pitchbend = val;
//  Panel->c_flags[ch] |= FLAG_BENDT;
    Panel->changed = 1;
}

static void ctl_reset(void)
{
    int i;

    if (!ctl.trace_playing)
        return;

    PanelReset();
    CanvasReadPanelInfo(0);
    CanvasUpdate(0);
    CanvasPaint();

    for (i = 0; i < MAX_W32G_MIDI_CHANNELS; i++) {
        if (ISDRUMCHANNEL(i))
            ctl_program(i, channel[i].bank);
        else
            ctl_program(i, channel[i].program);
        ctl_volume(i, channel[i].volume);
        ctl_expression(i, channel[i].expression);
        ctl_panning(i, channel[i].panning);
        ctl_sustain(i, channel[i].sustain);
        if (channel[i].pitchbend == 0x2000 &&
            channel[i].mod.val > 0)
            ctl_pitch_bend(i, -1);
        else
            ctl_pitch_bend(i, channel[i].pitchbend);
        ctl_channel_note(i, Panel->cnote[i], 0);
    }
    Panel->changed = 1;
}

static void ctl_maxvoices(int v)
{
    Panel->voices = v;
    Panel->changed = 1;
}

extern void w32_wrd_ctl_event(CtlEvent *e);
extern void w32_tracer_ctl_event(CtlEvent *e);
static void ctl_event(CtlEvent *e)
{
    int flg = 0;

    w32_wrd_ctl_event(e);
    w32_tracer_ctl_event(e);
    switch (e->type) {
    case CTLE_NOW_LOADING:
        PanelReset();
        CanvasReset();
        CanvasClear();
        CanvasReadPanelInfo(1);
        CanvasUpdate(1);
        CanvasPaintAll();
        MPanelReset();
        MPanelReadPanelInfo(1);
        MPanelUpdateAll();
        MPanelPaintAll();
        MPanelStartLoad((char*) e->v1);
        break;
    case CTLE_LOADING_DONE:
        break;
    case CTLE_PLAY_START:
        w32g_ctle_play_start((int) e->v1 / play_mode->rate);
        break;
    case CTLE_PLAY_END:
        MainWndScrollbarProgressUpdate(-1);
        break;
    case CTLE_CUEPOINT:
        cuepoint = e->v1;
        cuepoint_pending = 1;
        break;
    case CTLE_CURRENT_TIME_END:
        flg = 1;
        // thru
    case CTLE_CURRENT_TIME: {
        int sec;
        if (midi_trace.flush_flag)
            return;
        if (ctl.trace_playing)
            sec = (int) e->v1;
        else {
            sec = current_trace_samples();
            if (sec < 0)
            sec = (int) e->v1;
            else
            sec = sec / play_mode->rate;
        }
        ctl_current_time(sec, (int) e->v2);
        display_aq_ratio();
        MainWndScrollbarProgressUpdate(sec);
        ctl_panel_refresh(flg);
        break; }
    case CTLE_NOTE:
        ctl_note((int) e->v1, (int) e->v2, (int) e->v3, (int) e->v4);
        break;
    case CTLE_GSLCD:
        ctl_gslcd((int) e->v1);
        CanvasReadPanelInfo(0);
        CanvasUpdate(0);
        CanvasPaint();
        break;
    case CTLE_MASTER_VOLUME:
        ctl_master_volume((int) e->v1);
        break;
    case CTLE_METRONOME:
        ctl_metronome((int) e->v1, (int) e->v2);
        break;
    case CTLE_KEYSIG:
        ctl_keysig((int8) e->v1, CTL_STATUS_UPDATE);
        break;
    case CTLE_KEY_OFFSET:
        ctl_keysig(CTL_STATUS_UPDATE, (int) e->v1);
        break;
    case CTLE_TEMPO:
        ctl_tempo((int) e->v1, CTL_STATUS_UPDATE);
        break;
    case CTLE_TIME_RATIO:
        ctl_tempo(CTL_STATUS_UPDATE, (int) e->v1);
        break;
    case CTLE_PROGRAM:
    //  ctl_program((int) e->v1, (int) e->v2, (char*) e->v3);
        ctl_program((int) e->v1, (int) e->v2);
        break;
    case CTLE_VOLUME:
        ctl_volume((int) e->v1, (int) e->v2);
        break;
    case CTLE_EXPRESSION:
        ctl_expression((int) e->v1, (int) e->v2);
        break;
    case CTLE_PANNING:
        ctl_panning((int) e->v1, (int) e->v2);
        break;
    case CTLE_SUSTAIN:
        ctl_sustain((int) e->v1, (int) e->v2);
        break;
    case CTLE_PITCH_BEND:
        ctl_pitch_bend((int) e->v1, (int) e->v2);
        break;
    case CTLE_MOD_WHEEL:
        ctl_pitch_bend((int) e->v1, e->v2 ? -2 : 0x2000);
        break;
    case CTLE_CHORUS_EFFECT:
        break;
    case CTLE_REVERB_EFFECT:
        break;
    case CTLE_LYRIC: {
#if 1
        const char *lyric;
        lyric = event2string((uint16) e->v1);
        if (lyric != NULL) {
            MPanelMessageClearAll();
            MPanelMessageAdd(lyric+1,20000,1);
        }
#else
        default_ctl_lyric((uint16) e->v1);
#endif
        break; }
    case CTLE_REFRESH:
        if (CanvasGetMode() == CANVAS_MODE_KBD_A
                || CanvasGetMode() == CANVAS_MODE_KBD_B
                || CanvasGetMode() == CANVAS_MODE_KBD_C
                || CanvasGetMode() == CANVAS_MODE_KBD_D) {
            CanvasReadPanelInfo(0);
            CanvasUpdate(0);
            CanvasPaint();
        }
        break;
    case CTLE_RESET:
        ctl_reset();
        break;
    case CTLE_SPEANA:
        break;
    case CTLE_PAUSE:
        if (w32g_play_active) {
            MainWndScrollbarProgressUpdate((int) e->v2);
            if (!(int) e->v1)
            ctl_reset();
            ctl_current_time((int) e->v2, 0);
            ctl_panel_refresh(0);
        }
        break;
    case CTLE_MAXVOICES:
        ctl_maxvoices((int) e->v1);
        break;
    }
}

/*
 * interface_<id>_loader();
 */
ControlMode *interface_d_loader(void)
{
    return &ctl;
}


