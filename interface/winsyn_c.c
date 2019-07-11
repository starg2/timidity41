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

    winsyn_c.c - Windows synthesizer interface
        Copyright (c) 2002 2003 Keishi Suenaga <s_keishi@mutt.freemail.ne.jp>

    I referenced following sources.
        alsaseq_c.c - ALSA sequencer server interface
            Copyright (c) 2000  Takashi Iwai <tiwai@suse.de>
        readmidi.c

    DESCRIPTION
    ===========

    This interface provides a Windows MIDI device interface which receives
    events and plays it in real-time.  On this mode, TiMidity works
    purely as software (real-time) MIDI render.

    For invoking Windows synthesizer interface, run timidity as folows:
      % timidity -iW    (interactively select an Input MIDI device)
    or
      % timidity -iW 2  (connect to MIDI device No. 2)

    TiMidity loads instruments dynamically at each time a PRM_CHANGE
    event is received.  It sometimes causes a noise.
    If you are using a low power machine, invoke timidity as follows:
      % timidity -s 11025 -iW       (set sampling freq. to 11025Hz)
    or
      % timidity -EFreverb=0 -iW    (disable MIDI reverb effect control)

    TiMidity keeps all loaded instruments during executing.

    To use TiMidity as output device, you need a MIDI loopback device.
    I use MIDI Yoke. It can freely be obtained MIDI-OX site
    (http://www.midiox.com).
*/

//#define USE_PORTMIDI 1
//#define USE_GTK_GUI 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef __POCC__
#include <sys/types.h>
#endif /* __POCC__ */

#include "rtsyn.h"
#ifdef USE_GTK_GUI
#include "wsgtk_main.h"
#endif /* USE_GTK_GUI */

#include <stdio.h>
#ifndef __W32__
#include <termios.h>
//#include <term.h>
#include <unistd.h>
#endif /* __W32__ */

#if defined(__W32__)
#include <conio.h>
#define kbhit _kbhit
#define HAVE_DOS_KEYBOARD 1
#endif /* __W32__ */

#ifdef __W32__
#include "w32g_utl.h"
#endif /* __W32__ */

#include "sndfontini.h"

#ifndef HAVE_DOS_KEYBOARD
static struct termios initial_settings, new_settings;
static int peek_character = -1;
#endif /* !HAVE_DOS_KEYBOARD */

extern int volatile stream_max_compute; // play_event() の compute_data() で計算を許す最大時間
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

#define ctl winsyn_control_mode

ControlMode ctl =
{
    "Windows Synthesizer interface", 'W',
    "winsyn",
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
#if defined(__W32G__) || defined(TWSYNG32)
    SaveSettingTiMidity(st_current);
    CopyMemory(st_temp, st_current, sizeof(SETTING_TIMIDITY));
#endif

    rtsyn_ws_setup();

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

#ifndef WINDRV
static int ctl_read(ptr_size_t *valp)
{
    return RC_NONE;
}
#else
long saveVolume = -1;
long cfgMaxVolume = -1;
long curVolume;
#include "timidity.h"
void OpenMidiVolume(void)
{
	curVolume = 100;
}
void CloseMidiVolume(void)
{
}
int GetMidiVolume(void)
{
	return curVolume;
}
void SetMidiVolume(int newvolume)
{
	curVolume = newvolume;
}
void MidiMidiVolume(void) {};

static int ctl_read(ptr_size_t *valp)
{
  if (otd.EnableVolMidCtrl) {
    if (saveVolume == GetMidiVolume()) {
    } else {
      long newVolume;
      if (cfgMaxVolume < 0) {
        cfgMaxVolume = amplification;
      }

      saveVolume = GetMidiVolume();
      newVolume = cfgMaxVolume * saveVolume * DIV_100;
      *valp = newVolume - amplification;
      return RC_CHANGE_VOLUME;
    }
  }
  return RC_NONE;
}
#endif /* !WINDRV */

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
  if (!ctl.opened)
    {
      vfprintf(stderr, fmt, ap);
      fputs(NLS, stderr);
    }
  else
    {
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
//    if (type == CMSG_FATAL)
//      w32g_msg_box(buffer, "TiMidity Error", MB_OK);
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
#endif

#ifndef IA_W32G_SYN
static int ctl_pass_playing_list(int n, char *args[])
#else
// 0: OK, 2: Require to reset.
int ctl_pass_playing_list2(int n, char *args[])
#endif
{
  int i, j, devnum, devok;
  unsigned int port = 0;
  int started;
  char cbuf[80];
  TIMECAPS tcaps;

  rtsyn_get_port_list();

#ifndef IA_W32G_SYN
  if (n > MAX_PORT) {
    printf("Usage: timidity -iW [Midi interface No s]\n");
    return 1;
  }
#endif

  if (n > 0) {
    port = 0;
    while (port < n && n != 0) {
      if ((portID[port] = atoi(args[port])) == 0) {
        n = 0;
      } else {
        devok = 0;
        for (i = 0; i < rtsyn_nportlist; i++) {
          sscanf(rtsyn_portlist[i], "%d:%s", &devnum, cbuf);
          if (devnum == portID[port]) devok = 1;
        }
        if (devok == 0) {
          n = 0;
#ifdef IA_W32G_SYN
          {
            TCHAR buff[1024];
            _stprintf(buff, _T("MIDI IN Device ID %d is not available. So set a proper ID for the MIDI port %d and restart."), portID[port], port);
            MessageBox(NULL, buff, _T("Error"), MB_OK);
            return 2;
          }
#endif
        }
      }
    port++;
    }
  }
  if (n == 0) {
    rtsyn_portnumber = 0;
  } else {
    rtsyn_portnumber = port;
  }

#if !defined(IA_W32G_SYN) && !defined(USE_GTK_GUI)
  if (n == 0) {
    char cbuf[80];
    printf("Whow many ports do you use?(max %d)\n", MAX_PORT);
    do {
      if (0 == scanf("%u", &rtsyn_portnumber)) scanf("%s", cbuf);
    } while (rtsyn_portnumber == 0 || rtsyn_portnumber > MAX_PORT);
    printf("\n");
    printf("Opening Device drivers:");
    printf("Available Midi Input devices:\n");

    for (i = 0; i < rtsyn_nportlist; i++) {
      printf("%s\n", rtsyn_portlist[i]);
    }
    for (port = 0; port < rtsyn_portnumber; port++) {
      printf("Keyin Input Device Number of port%d\n", port + 1);
      do {
        devok = 0;
        if (0 == scanf("%u", &portID[port])) scanf("%s", cbuf);
        for (i = 0; i < rtsyn_nportlist; i++) {
          sscanf(rtsyn_portlist[i], "%d:%s", &devnum, cbuf);
          if (devnum == portID[port]) devok = 1;
        }
      } while (devok == 0);
      printf("\n");
    }
  }
#endif

  for (port = 0; port < rtsyn_portnumber; port++) {
    portID[port] = portID[port] - 1;
  }


#if !defined(IA_W32G_SYN) && !defined(USE_GTK_GUI)
  printf("TiMidity starting in Windows Synthesizer mode\n");
  printf("Usage: timidity -iW [Midi interface No]\n");
  printf("\n");
  printf("N (Normal mode) M(GM mode) S(GS mode) X(XG mode) G(GM2 mode) D(SD mode) K(KG mode) J(CM mode)\n");
  printf("(Only in Normal mode, Mode can be changed by MIDI data)\n");
  printf("m(GM reset) s(GS reset) x(XG reset) g(GM2 reset) d(SD reset) k(KG reset) j(CM reset)\n");
  printf("\n");
  printf("Press 'q' key to stop\n");
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
static void init_keybord(void) {
  tcgetattr(0, &initial_settings);
  tcgetattr(0, &new_settings);
  new_settings.c_lflag &= ~ICANON;
  new_settings.c_lflag &= ~ECHO;
  new_settings.c_lflag &= ~ISIG;
  new_settings.c_cc[VMIN] = 1;
  new_settings.c_cc[VTIME] = 0;
  tcsetattr(0, TCSANOW, &new_settings);
}

static void close_keybord(void) {
  tcsetattr(0, TCSANOW, &initial_settings);
}

static int kbhit(void) {
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


static char readch(void) {
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
ControlMode *interface_W_loader(void)
{
  return &ctl;
}


