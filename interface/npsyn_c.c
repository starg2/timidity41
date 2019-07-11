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

    npsyn_c.c - Windows Named pipe synthesizer interface
        Copyright (c) 2007 Keishi Suenaga <s_keishi@yahoo.co.jp>

    I referenced following sources.
        alsaseq_c.c - ALSA sequencer server interface
            Copyright (c) 2000  Takashi Iwai <tiwai@suse.de>
        readmidi.c

    DESCRIPTION
    ===========

    This interface provides a Windows Named pipe interface which receives
    events and plays it in real-time.  On this mode, TiMidity works
    purely as software (real-time) MIDI render.

    For invoking Windows Named pipe synthesizer interface, run timidity as folows:
      % timidity -iN foo         (create pipe "\\.\pipe\foo")
    or
      % timidity -iN "foo bar" 1 (SampleTime Mode ON)

    TiMidity loads instruments dynamically at each time a PRM_CHANGE
    event is received.  It sometimes causes a noise.
    If you are using a low power machine, invoke timidity as follows:
      % timidity -s 11025 -iN       (set sampling freq. to 11025Hz)
    or
      % timidity -EFreverb=0 -iN    (disable MIDI reverb effect control)

    TiMidity keeps all loaded instruments during executing.
*/

//#define USE_PORTMIDI 1
//#define USE_GTK_GUI 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "interface.h"

#ifdef IA_NPSYN

#ifdef __POCC__
#include <sys/types.h>
#endif /* __POCC__ */

#include "rtsyn.h"

#ifdef USE_GTK_GUI
#include "wsgtk_main.h"
#endif /* USE_GTK_GUI */

#include <stdio.h>
#ifndef __W32__
//#include <termios.h>
//#include <term.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#endif /* !__W32__ */

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
#include <conio.h>
#define kbhit _kbhit
#define HAVE_DOS_KEYBOARD 1
#elif defined(__GNUC__)
#include <termios.h>
#endif /* _MSC_VER || __MINGW32__ || __MINGW64__ */

#ifndef HAVE_DOS_KEYBOARD
static struct termios initial_settings, new_settings;
static int peek_character = -1;
#endif /* !HAVE_DOS_KEYBOARD */

extern int volatile stream_max_compute; // play_event() ÇÃ compute_data() Ç≈åvéZÇãñÇ∑ç≈ëÂéûä‘
extern int seq_quit; // rtsyn_common.c

static int ctl_open(int using_stdin, int using_stdout);
static void ctl_close(void);
static int ctl_read(ptr_size_t *valp);
static int cmsg(int type, int verbosity_level, const char *fmt, ...);
static void ctl_event(CtlEvent *e);
static int ctl_pass_playing_list(int n, char *args[]);

#ifndef HAVE_DOS_KEYBOARD
static void init_keybord(void);
static void close_keybord(void);
static int kbhit(void);
static char readch(void);
#endif /* !HAVE_DOS_KEYBOARD */

/**********************************/
/* export the interface functions */

#define ctl npsyn_control_mode

ControlMode ctl =
{
    "Windows Named Pipe Synthesizer interface", 'N',
    "npsyn",
    1, 0, 0,
    0,
    ctl_open,
    ctl_close,
    ctl_pass_playing_list,
    ctl_read,
    NULL,
    cmsg,
    ctl_event
};

static int32 event_time_offset;
static FILE *outfp;

/*ARGSUSED*/

static int ctl_open(int using_stdin, int using_stdout)
{
    rtsyn_np_setup();

    ctl.opened = 1;
    ctl.flags &= ~(CTLF_LIST_RANDOM | CTLF_LIST_SORT);
    if (using_stdout)
        outfp = stderr;
    else
        outfp = stdout;
    return 0;
}

static void ctl_close(void)
{
    fflush(outfp);
    if (seq_quit == 0) {
        rtsyn_synth_stop();
        rtsyn_close();
        seq_quit = ~0;
    }
    ctl.opened = 0;
}

static int ctl_read(ptr_size_t *valp)
{
    return RC_NONE;
}

#ifdef IA_W32G_SYN
extern void PutsConsoleWnd(const char *str);
extern int ConsoleWndFlag;
#endif
static int cmsg(int type, int verbosity_level, const char *fmt, ...)
{
#ifndef WINDRV
#ifndef IA_W32G_SYN
    va_list ap;

    if ((type == CMSG_TEXT || type == CMSG_INFO || type == CMSG_WARNING) &&
            ctl.verbosity < verbosity_level)
        return 0;
    va_start(ap, fmt);
    if (type == CMSG_WARNING || type == CMSG_ERROR || type == CMSG_FATAL)
        dumb_error_count++;
    if (!ctl.opened) {
        vfprintf(stderr, fmt, ap);
        fputs(NLS, stderr);
    }
    else {
        vfprintf(outfp, fmt, ap);
        fputs(NLS, outfp);
        fflush(outfp);
    }
    va_end(ap);
#else
    if (!ConsoleWndFlag) return 0;

    {
        char buffer[1024];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, ap);
        va_end(ap);

        if ((type == CMSG_TEXT || type == CMSG_INFO || type == CMSG_WARNING) &&
                ctl.verbosity < verbosity_level)
            return 0;
//      if (type == CMSG_FATAL)
//          w32g_msg_box(buffer, "TiMidity Error", MB_OK);
        PutsConsoleWnd(buffer);
        PutsConsoleWnd("\n");
        return 0;
    }
#endif /* !IA_W32G_SYN */
#endif /* !WINDRV */

    return 0;
}

static void ctl_event(CtlEvent *e)
{
}

static void doit(void);

#ifdef IA_W32G_SYN
extern void w32g_syn_doit(void);
extern int w32g_syn_ctl_pass_playing_list(int n_, char *args_[]);

static int ctl_pass_playing_list(int n, char *args[])
{
    return w32g_syn_ctl_pass_playing_list(n, args);
}
#endif /* IA_W32G_SYN */

#ifndef IA_W32G_SYN
static int ctl_pass_playing_list(int n, char *args[])
#else
// 0: OK, 2: Require to reset.
int ctl_pass_playing_list2(int n, char *args[])
#endif
{
#ifdef FORCE_TIME_PERIOD
    TIMECAPS tcaps;
#endif /* FORCE_TIME_PERIOD */

    if ((n < 1) || (n > 2)) {
        ctl.cmsg(CMSG_WARNING, VERB_NORMAL,
                 "Usage: timidity -i%c [Named Pipe Name] SampleTimeMode(1 or 0)", ctl.id_character);
        return 1;
    }

    rtsyn_np_set_pipe_name(args[0]);
    if (n == 1) {
        rtsyn_sample_time_mode = 0;
    } else {
        rtsyn_sample_time_mode = atoi(args[1]);
    }

#if !defined(IA_W32G_SYN) && !defined(USE_GTK_GUI)
    ctl.cmsg(CMSG_WARNING, VERB_NORMAL,
             "\nOpening Device drivers:" "Available Midi Input Pipe:" "" NLS);

    ctl.cmsg(CMSG_WARNING, VERB_NORMAL,
             "PATH");
    {
        LPCSTR PipeName;
        PipeName = rtsyn_np_get_pipe_path();
        ctl.cmsg(CMSG_WARNING, VERB_NORMAL,
                 "%s", PipeName);
        ctl.cmsg(CMSG_WARNING, VERB_NORMAL,
                 "");
    }

    ctl.cmsg(CMSG_WARNING, VERB_NORMAL,
             "TiMidity starting in Windows Named Pipe Synthesizer mode");
    ctl.cmsg(CMSG_WARNING, VERB_NORMAL,
             "Usage: timidity -i%c [Named Pipe Name] SampleTimeMode(1 or 0) " NLS, ctl.id_character);
    ctl.cmsg(CMSG_WARNING, VERB_NORMAL, "c(Reset)");
    ctl.cmsg(CMSG_WARNING, VERB_NORMAL,
             "N(Normal mode) M(GM mode) S(GS mode) X(XG mode) G(GM2 mode) D(SD mode) K(KG mode) J(CM mode)");
    ctl.cmsg(CMSG_WARNING, VERB_NORMAL,
             "(Only in Normal mode, Mode can be changed by MIDI data)");
    ctl.cmsg(CMSG_WARNING, VERB_NORMAL,
             "m(GM reset) s(GS reset) x(XG reset) g(GM2 reset) d(SD reset) k(KG reset) j(CM reset)" NLS);
    ctl.cmsg(CMSG_WARNING, VERB_NORMAL,
             "Press 'q' key to stop");
#endif

    rtsyn_init();

#ifdef FORCE_TIME_PERIOD
    if (timeGetDevCaps(&tcaps, sizeof(TIMECAPS)) != TIMERR_NOERROR)
        tcaps.wPeriodMin = 10;
    timeBeginPeriod(tcaps.wPeriodMin);
#endif /* FORCE_TIME_PERIOD */

#ifdef USE_GTK_GUI
    twgtk_main();
#else
#ifdef IA_W32G_SYN
    if (0 != rtsyn_synth_start()) {
        seq_quit = 0;
        while (seq_quit == 0) {
            w32g_syn_doit();
        }
        rtsyn_synth_stop();
    }
#else
    if (0 != rtsyn_synth_start()) {
        seq_quit = 0;
        while (seq_quit == 0) {
            doit();
        }
        rtsyn_synth_stop();
    }
#endif /* IA_W32G_SYN */
#endif /* USE_GTK_GUI */

#ifdef FORCE_TIME_PERIOD
    timeEndPeriod(tcaps.wPeriodMin);
#endif /* FORCE_TIME_PERIOD */
    rtsyn_close();

    return 0;
}

#ifndef IA_W32G_SYN

#ifndef HAVE_DOS_KEYBOARD
static void init_keybord(void)
{
    tcgetattr(0, &initial_settings);
    tcgetattr(0, &new_settings);
    new_settings.c_lflag &= ~ICANON;
    new_settings.c_lflag &= ~ECHO;
    new_settings.c_lflag &= ~ISIG;
    new_settings.c_cc[VMIN] = 1;
    new_settings.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &new_settings);
}

static void close_keybord(void)
{
    tcsetattr(0, TCSANOW, &initial_settings);
}

static int kbhit(void)
{
    char ch;
    int nread;

    if (peek_character != -1)
        return 1;
    new_settings.c_cc[VMIN] = 0;
    tcsetattr(0, TCSANOW, &new_settings);
    nread = read(0, &ch, 1);
    new_settings.c_cc[VMIN] = 1;
    tcsetattr(0, TCSANOW, &new_settings);

    if (nread == 1) {
        peek_character = ch;
        return 1;
    }
    return 0;
}

static char readch(void)
{
    char ch;
    if (peek_character != -1) {
        ch = peek_character;
        peek_character = -1;
        return ch;
    }
    read(0, &ch, 1);
    return ch;
}
#endif /* !HAVE_DOS_KEYBOARD */

static void doit(void)
{
#ifndef HAVE_DOS_KEYBOARD
    init_keybord();
#endif

    while (seq_quit == 0) {
#ifdef HAVE_DOS_KEYBOARD
        if (kbhit()) {
            switch (getch()) {
#else
        if (kbhit()) {
            switch (readch()) {
#endif
            case 'Q':
            case 'q':
                ctl.cmsg(CMSG_INFO, VERB_NORMAL, "quit");
                seq_quit = ~0;
                break;

            case 'm':
                ctl.cmsg(CMSG_INFO, VERB_VERBOSE, "GM System Reset");
                rtsyn_gm_reset();
                break;
            case 's':
                ctl.cmsg(CMSG_INFO, VERB_VERBOSE, "GS System Reset");
                rtsyn_gs_reset();
                break;
            case 'x':
                ctl.cmsg(CMSG_INFO, VERB_VERBOSE, "XG System Reset");
                rtsyn_xg_reset();
                break;
            case 'g':
                ctl.cmsg(CMSG_INFO, VERB_VERBOSE, "GM2 System Reset");
                rtsyn_gm2_reset();
                break;
            case 'd':
                ctl.cmsg(CMSG_INFO, VERB_VERBOSE, "SD Native Reset");
                rtsyn_sd_reset();
                break;
            case 'k':
                ctl.cmsg(CMSG_INFO, VERB_VERBOSE, "KG System Reset");
                rtsyn_kg_reset();
                break;
            case 'j':
                ctl.cmsg(CMSG_INFO, VERB_VERBOSE, "CM System Reset");
                rtsyn_cm_reset();
                break;
            case 'c':
                ctl.cmsg(CMSG_INFO, VERB_VERBOSE, "Reset");
                rtsyn_normal_reset();
                break;

            case 'M':
                ctl.cmsg(CMSG_INFO, VERB_VERBOSE, "GM System is DefaultMode");
                rtsyn_gm_modeset();
                break;
            case 'S':
                ctl.cmsg(CMSG_INFO, VERB_VERBOSE, "GS System is DefaultMode");
                rtsyn_gs_modeset();
                break;
            case 'X':
                ctl.cmsg(CMSG_INFO, VERB_VERBOSE, "XG System is DefaultMode");
                rtsyn_xg_modeset();
                break;
            case 'G':
                ctl.cmsg(CMSG_INFO, VERB_VERBOSE, "GM2 System is DefaultMode");
                rtsyn_gm2_modeset();
                break;
            case 'D':
                ctl.cmsg(CMSG_INFO, VERB_VERBOSE, "SD Native is DefaultMode");
                rtsyn_sd_modeset();
                break;
            case 'K':
                ctl.cmsg(CMSG_INFO, VERB_VERBOSE, "KG System is DefaultMode");
                rtsyn_kg_modeset();
                break;
            case 'J':
                ctl.cmsg(CMSG_INFO, VERB_VERBOSE, "CM System is DefaultMode");
                rtsyn_cm_modeset();
                break;
            case 'N':
                ctl.cmsg(CMSG_INFO, VERB_VERBOSE, "Restore NormalMode");
                rtsyn_normal_modeset();
                break;
            }
        }
        rtsyn_play_some_data();
        if (rtsyn_sample_time_mode == 0)
            rtsyn_play_calculate();
        if (intr) seq_quit = ~0;
        sleep(0.001);
    }
#ifndef HAVE_DOS_KEYBOARD
    close_keybord();
#endif
}

#endif /* !IA_W32G_SYN */

/*
 * interface_<id>_loader();
 */
ControlMode *interface_N_loader(void)
{
    return &ctl;
}

#endif /* IA_NPSYN */


