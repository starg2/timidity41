/* 

    TiMidity -- Experimental MIDI to WAVE converter
    Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    ncurs_c.c
   
    */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>

#include <ncurses.h>

#include "config.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "output.h"
#include "controls.h"

static void ctl_refresh(void);
static void ctl_help_mode(void);
static void ctl_total_time(int tt);
static void ctl_master_volume(int mv);
static void ctl_file_name(char *name);
static void ctl_current_time(int ct);
static void ctl_note(int v);
static void ctl_program(int ch, int val);
static void ctl_volume(int channel, int val);
static void ctl_expression(int channel, int val);
static void ctl_panning(int channel, int val);
static void ctl_sustain(int channel, int val);
static void ctl_pitch_bend(int channel, int val);
static void ctl_reset(void);
static int ctl_open(int using_stdin, int using_stdout);
static void ctl_close(void);
static int ctl_read(int32 *valp);
static int cmsg(int type, int verbosity_level, char *fmt, ...);

/**********************************************/
/* export the interface functions */

#define ctl ncurses_control_mode

ControlMode ctl= 
{
  "ncurses interface", 'n',
  1,0,0,
  ctl_open,dumb_pass_playing_list, ctl_close, ctl_read, cmsg,
  ctl_refresh, ctl_reset, ctl_file_name, ctl_total_time, ctl_current_time, 
  ctl_note, 
  ctl_master_volume, ctl_program, ctl_volume, 
  ctl_expression, ctl_panning, ctl_sustain, ctl_pitch_bend
};


/***********************************************************************/
/* foreground/background checks disabled since switching to curses */
/* static int in_foreground=1; */
static int ctl_helpmode=0;

static WINDOW *dftwin=0, *msgwin=0;

static void _ctl_refresh(void)
{
  wmove(dftwin, 0,0);
  wrefresh(dftwin);
}

static void ctl_refresh(void)
{
  if (ctl.trace_playing)
    _ctl_refresh();
}

static void ctl_help_mode(void)
{
  static WINDOW *helpwin;
  if (ctl_helpmode)
    {
      ctl_helpmode=0;
      delwin(helpwin);
      touchwin(dftwin);
      _ctl_refresh();
    }
  else
    {
      ctl_helpmode=1;
      /* And here I thought the point of curses was that you could put
	 stuff on windows without having to worry about whether this
	 one is overlapping that or the other way round... */
      helpwin=newwin(2,COLS,0,0);
      wattron(helpwin, A_REVERSE);
      werase(helpwin); 
      waddstr(helpwin, 
	      "V/Up=Louder    b/Left=Skip back      "
	      "n/Next=Next file      r/Home=Restart file");
      wmove(helpwin, 1,0);
      waddstr(helpwin, 
	      "v/Down=Softer  f/Right=Skip forward  "
	      "p/Prev=Previous file  q/End=Quit program");
      wrefresh(helpwin);
    }
}

static void ctl_total_time(int tt)
{
  int mins, secs=tt/play_mode->rate;
  mins=secs/60;
  secs-=mins*60;

  wmove(dftwin, 4,6+6+3);
  wattron(dftwin, A_BOLD);
  wprintw(dftwin, "%3d:%02d", mins, secs);
  wattroff(dftwin, A_BOLD);
  _ctl_refresh();
}

static void ctl_master_volume(int mv)
{
  wmove(dftwin, 4,COLS-5);
  wattron(dftwin, A_BOLD);
  wprintw(dftwin, "%03d %%", mv);
  wattroff(dftwin, A_BOLD);
  _ctl_refresh();
}

static void ctl_file_name(char *name)
{
  wmove(dftwin, 3,6);
  wclrtoeol(dftwin );
  wattron(dftwin, A_BOLD);
  waddstr(dftwin, name);
  wattroff(dftwin, A_BOLD);
  _ctl_refresh();
}

static void ctl_current_time(int ct)
{
  int i,v;
  int mins, secs=ct/play_mode->rate;
  if (!ctl.trace_playing) 
    return;

  mins=secs/60;
  secs-=mins*60;
  wmove(dftwin, 4,6);
  wattron(dftwin, A_BOLD);
  wprintw(dftwin, "%3d:%02d", mins, secs);
  v=0;
  i=voices;
  while (i--)
    if (voice[i].status!=VOICE_FREE) v++;
  wmove(dftwin, 4,48);
  wprintw(dftwin, "%2d", v);
  wattroff(dftwin, A_BOLD);
  _ctl_refresh();
}

static void ctl_note(int v)
{
  int xl;
  if (!ctl.trace_playing) 
    return;
  xl=voice[v].note%(COLS-24);
  wmove(dftwin, 8+voice[v].channel,xl+3);
  switch(voice[v].status)
    {
    case VOICE_DIE:
      waddch(dftwin, ',');
      break;
    case VOICE_FREE: 
      waddch(dftwin, '.');
      break;
    case VOICE_ON:
      wattron(dftwin, A_BOLD);
      waddch(dftwin, '0'+(10*voice[v].velocity)/128); 
      wattroff(dftwin, A_BOLD);
      break;
    case VOICE_OFF:
    case VOICE_SUSTAINED:
      waddch(dftwin, '0'+(10*voice[v].velocity)/128);
      break;
    }
}

static void ctl_program(int ch, int val)
{
  if (!ctl.trace_playing) 
    return;
  wmove(dftwin, 8+ch, COLS-20);
  if (ISDRUMCHANNEL(ch))
    {
      wattron(dftwin, A_BOLD);
      wprintw(dftwin, "%03d", val);
      wattroff(dftwin, A_BOLD);
    }
  else
    wprintw(dftwin, "%03d", val);
}

static void ctl_volume(int channel, int val)
{
  if (!ctl.trace_playing) 
    return;
  wmove(dftwin, 8+channel, COLS-16);
  wprintw(dftwin, "%3d", (val*100)/127);
}

static void ctl_expression(int channel, int val)
{
  if (!ctl.trace_playing) 
    return;
  wmove(dftwin, 8+channel, COLS-12);
  wprintw(dftwin, "%3d", (val*100)/127);
}

static void ctl_panning(int channel, int val)
{
  if (!ctl.trace_playing) 
    return;
  wmove(dftwin, 8+channel, COLS-8);
  if (val==NO_PANNING)
    waddstr(dftwin, "   ");
  else if (val<5)
    waddstr(dftwin, " L ");
  else if (val>123)
    waddstr(dftwin, " R ");
  else if (val>60 && val<68)
    waddstr(dftwin, " C ");
  else
    {
      /* wprintw(dftwin, "%+02d", (100*(val-64))/64); */
      val = (100*(val-64))/64; /* piss on curses */
      if (val<0)
	{
	  waddch(dftwin, '-');
	  val=-val;
	}
      else waddch(dftwin, '+');
      wprintw(dftwin, "%02d", val);
    }
}

static void ctl_sustain(int channel, int val)
{
  if (!ctl.trace_playing) 
    return;
  wmove(dftwin, 8+channel, COLS-4);
  if (val) waddch(dftwin, 'S');
  else waddch(dftwin, ' ');
}

static void ctl_pitch_bend(int channel, int val)
{
  if (!ctl.trace_playing) 
    return;
  wmove(dftwin, 8+channel, COLS-2);
  if (val>0x2000) waddch(dftwin, '+');
  else if (val<0x2000) waddch(dftwin, '-');
  else waddch(dftwin, ' ');
}

static void ctl_reset(void)
{
  int i,j;
  if (!ctl.trace_playing) 
    return;
  for (i=0; i<16; i++)
    {
      wmove(dftwin, 8+i, 3);
      for (j=0; j<COLS-24; j++)
	waddch(dftwin, '.');
      ctl_program(i, channel[i].program);
      ctl_volume(i, channel[i].volume);
      ctl_expression(i, channel[i].expression);
      ctl_panning(i, channel[i].panning);
      ctl_sustain(i, channel[i].sustain);
      ctl_pitch_bend(i, channel[i].pitchbend);
    }
  _ctl_refresh();
}

/***********************************************************************/

/* #define CURSED_REDIR_HACK */

#ifdef CURSED_REDIR_HACK
static SCREEN *oldscr;
#endif

static int ctl_open(int using_stdin, int using_stdout)
{
  int i;
#ifdef CURSED_REDIR_HACK
  FILE *infp=stdin, *outfp=stdout;
  SCREEN *dftscr;

  /* This doesn't work right */
  if (using_stdin && using_stdout)
    {
      infp=outfp=stderr;
      fflush(stderr);
      setvbuf(stderr, 0, _IOFBF, BUFSIZ);
    }
  else if (using_stdout)
    {
      outfp=stderr;
      fflush(stderr);
      setvbuf(stderr, 0, _IOFBF, BUFSIZ);
    }
  else if (using_stdin)
    {
      infp=stdout;
      fflush(stdout);
      setvbuf(stdout, 0, _IOFBF, BUFSIZ);
    }

  dftscr=newterm(0, outfp, infp);
  if (!dftscr)
    return -1;
  oldscr=set_term(dftscr);
  /* dftwin=stdscr; */
#else
  initscr();
#endif

  cbreak();
  noecho();
  nonl();
  nodelay(stdscr, 1);
  scrollok(stdscr, 0);
  idlok(stdscr, 1);
  keypad(stdscr, TRUE);
  ctl.opened=1;

  if (ctl.trace_playing)
    dftwin=stdscr;
  else
    dftwin=newwin(6,COLS,0,0);

  werase(dftwin);
  wmove(dftwin, 0,0);
  waddstr(dftwin, "TiMidity v" TIMID_VERSION);
  wmove(dftwin, 0,COLS-52);
  waddstr(dftwin, "(C) 1995 Tuukka Toivonen <toivonen@clinet.fi>");
  wmove(dftwin, 1,0);
  waddstr(dftwin, "Press 'h' for help with keys, or 'q' to quit.");
  wmove(dftwin, 3,0);
  waddstr(dftwin, "File:");
  wmove(dftwin, 4,0);
  if (ctl.trace_playing)
    {
      waddstr(dftwin, "Time:");
      wmove(dftwin, 4,6+6+1);
      waddch(dftwin, '/');
      wmove(dftwin, 4,40);
      wprintw(dftwin, "Voices:    / %d", voices);
    }
  else
    {
      waddstr(dftwin, "Playing time:");
    }
  wmove(dftwin, 4,COLS-20);
  waddstr(dftwin, "Master volume:");
  wmove(dftwin, 5,0);
  for (i=0; i<COLS; i++)
    waddch(dftwin, '_');
  if (ctl.trace_playing)
    {
      wmove(dftwin, 6,0);
      waddstr(dftwin, "Ch");
      wmove(dftwin, 6,COLS-20);
      waddstr(dftwin, "Prg Vol Exp Pan S B");
      wmove(dftwin, 7,0);
      for (i=0; i<COLS; i++)
	waddch(dftwin, '-');
      for (i=0; i<16; i++)
	{
	  wmove(dftwin, 8+i, 0);
	  wprintw(dftwin, "%02d", i+1);
	}
    }
  else
    {
      msgwin=newwin(LINES-6,COLS,6,0);
      werase(msgwin);
      scrollok(msgwin, 1);
      wrefresh(msgwin);
    }
  _ctl_refresh();
  
  return 0;
}

static void ctl_close(void)
{
  if (ctl.opened)
    {
      endwin();
      ctl.opened=0;
    }
}

static int ctl_read(int32 *valp)
{
  int c;
  while ((c=getch())!=ERR)
    {
      switch(c)
	{
	case 'h':
	case '?':
	case KEY_F(1):
	  ctl_help_mode();
	  return RC_NONE;
	  
	case 'V':
	case KEY_UP:
	  *valp=10;
	  return RC_CHANGE_VOLUME;
	case 'v':
	case KEY_DOWN:
	  *valp=-10;
	  return RC_CHANGE_VOLUME;
	case 'q':
	case KEY_END:
	  return RC_QUIT;
	case 'n':
	case KEY_NPAGE:
	  return RC_NEXT;
	case 'p':
	case KEY_PPAGE:
	  return RC_REALLY_PREVIOUS;
	case 'r':
	case KEY_HOME:
	  return RC_RESTART;

	case 'f':
	case KEY_RIGHT:
	  *valp=play_mode->rate;
	  return RC_FORWARD;
	case 'b':
	case KEY_LEFT:
	  *valp=play_mode->rate;
	  return RC_BACK;
	  /* case ' ':
	     return RC_TOGGLE_PAUSE; */
	}
    }
  return RC_NONE;
}

static int cmsg(int type, int verbosity_level, char *fmt, ...)
{
  va_list ap;
  if ((type==CMSG_TEXT || type==CMSG_INFO || type==CMSG_WARNING) &&
      ctl.verbosity<verbosity_level)
    return 0;
  va_start(ap, fmt);
  if (!ctl.opened)
    {
      vfprintf(stderr, fmt, ap);
      fprintf(stderr, "\n");
    }
  else if (ctl.trace_playing)
    {
      switch(type)
	{
	  /* Pretty pointless to only have one line for messages, but... */
	case CMSG_WARNING:
	case CMSG_ERROR:
	case CMSG_FATAL:
	  wmove(dftwin, 2,0);
	  wclrtoeol(dftwin);
	  wattron(dftwin, A_REVERSE);
	  vwprintw(dftwin, fmt, ap);
	  wattroff(dftwin, A_REVERSE);
	  _ctl_refresh();
	  if (type==CMSG_WARNING)
	    sleep(1); /* Don't you just _HATE_ it when programs do this... */
	  else
	    sleep(2);
	  wmove(dftwin, 2,0);
	  wclrtoeol(dftwin);
	  _ctl_refresh();
	  break;
	}
    }
  else
    {
      switch(type)
	{
	default:
	  vwprintw(msgwin, fmt, ap);
	  wprintw(msgwin, "\n");
	  wrefresh(msgwin);
	  break;

	case CMSG_WARNING:
	  wattron(msgwin, A_BOLD);
	  vwprintw(msgwin, fmt, ap);
	  wprintw(msgwin, "\n");
	  wattroff(msgwin, A_BOLD);
	  wrefresh(msgwin);
	  break;
	  
	case CMSG_ERROR:
	case CMSG_FATAL:
	  wattron(msgwin, A_REVERSE);
	  vwprintw(msgwin, fmt, ap);
	  wprintw(msgwin, "\n");
	  wattroff(msgwin, A_REVERSE);
	  wrefresh(msgwin);
	  if (type==CMSG_FATAL)
	    sleep(2);
	  break;
	}
    }

  va_end(ap);
  return 0;
}
