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

    motif_ctl.c: written by Vincent Pagel (pagel@loria.fr) 10/4/95
   
    A motif interface for TIMIDITY : to prevent X redrawings from 
    interfering with the audio computation, I don't use the XtAppAddWorkProc

    I create a pipe between the timidity process and a Motif interface
    process forked from the 1st one

    */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>

#include "config.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "output.h"
#include "controls.h"
#include "motif.h"

static void ctl_refresh(void);
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
static void ctl_pass_playing_list(int number_of_files, char *list_of_files[]);

/**********************************************/
/* export the interface functions */

#define ctl motif_control_mode

ControlMode ctl= 
{
  "motif interface", 'm',
  1,0,0,
  ctl_open, ctl_pass_playing_list, ctl_close, ctl_read, cmsg,
  ctl_refresh, ctl_reset, ctl_file_name, ctl_total_time, ctl_current_time, 
  ctl_note, 
  ctl_master_volume, ctl_program, ctl_volume, 
  ctl_expression, ctl_panning, ctl_sustain, ctl_pitch_bend
};


/***********************************************************************/
/* Put controls on the pipe                                            */
/***********************************************************************/
static int cmsg(int type, int verbosity_level, char *fmt, ...)
{
    char local[255];

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
	    vsprintf(local, fmt, ap);
	    pipe_int_write(CMSG_MESSAGE);
	    pipe_int_write(type);
	    pipe_string_write(local);
	}
    va_end(ap);
    return 0;
}


static void _ctl_refresh(void)
{
    /* pipe_int_write(REFRESH_MESSAGE); */
}

static void ctl_refresh(void)
{
  if (ctl.trace_playing)
    _ctl_refresh();
}

static void ctl_total_time(int tt)
{
  int centisecs=tt/(play_mode->rate/100);

  pipe_int_write(TOTALTIME_MESSAGE);
  pipe_int_write(centisecs);
}

static void ctl_master_volume(int mv)
{
    pipe_int_write(MASTERVOL_MESSAGE);
    pipe_int_write(mv);
}

static void ctl_file_name(char *name)
{
    pipe_int_write(FILENAME_MESSAGE);
    pipe_string_write(name);
}

static void ctl_current_time(int ct)
{
    int i,v;
    int centisecs=ct/(play_mode->rate/100);

    if (!ctl.trace_playing) 
	return;
           
    v=0;
    i=voices;
    while (i--)
	if (voice[i].status!=VOICE_FREE) v++;
    
    pipe_int_write(CURTIME_MESSAGE);
    pipe_int_write(centisecs);
    pipe_int_write(v);
}

static void ctl_note(int v)
{
    /*   int xl;
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
	*/
}

static void ctl_program(int ch, int val)
{
/*  if (!ctl.trace_playing) 
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
    */
}

static void ctl_volume(int channel, int val)
{
    /*
      if (!ctl.trace_playing) 
    return;
  wmove(dftwin, 8+channel, COLS-16);
  wprintw(dftwin, "%3d", (val*100)/127);
  */
}

static void ctl_expression(int channel, int val)
{
/*  if (!ctl.trace_playing) 
    return;
  wmove(dftwin, 8+channel, COLS-12);
  wprintw(dftwin, "%3d", (val*100)/127);
  */
}

static void ctl_panning(int channel, int val)
{
/*  if (!ctl.trace_playing) 
    return;
  
  if (val==NO_PANNING)
    waddstr(dftwin, "   ");
  else if (val<5)
    waddstr(dftwin, " L ");
  else if (val>123)
    waddstr(dftwin, " R ");
  else if (val>60 && val<68)
    waddstr(dftwin, " C ");
    */
}

static void ctl_sustain(int channel, int val)
{
/*
  if (!ctl.trace_playing) 
    return;

  if (val) waddch(dftwin, 'S');
  else waddch(dftwin, ' ');
  */
}

static void ctl_pitch_bend(int channel, int val)
{
/*  if (!ctl.trace_playing) 
    return;

  if (val>0x2000) waddch(dftwin, '+');
  else if (val<0x2000) waddch(dftwin, '-');
  else waddch(dftwin, ' ');
  */
}

static void ctl_reset(void)
{
/*  int i,j;
  if (!ctl.trace_playing) 
    return;
  for (i=0; i<16; i++)
    {
	ctl_program(i, channel[i].program);
	ctl_volume(i, channel[i].volume);
	ctl_expression(i, channel[i].expression);
	ctl_panning(i, channel[i].panning);
	ctl_sustain(i, channel[i].sustain);
	ctl_pitch_bend(i, channel[i].pitchbend);
    }
  _ctl_refresh();
  */
}

/***********************************************************************/
/* OPEN THE CONNECTION                                                */
/***********************************************************************/
static int ctl_open(int using_stdin, int using_stdout)
{
  ctl.opened=1;
  ctl.trace_playing=1;	/* Default mode with Motif interface */
  
  /* The child process won't come back from this call  */
  pipe_open();

  return 0;
}

/* Tells the window to disapear */
static void ctl_close(void)
{
  if (ctl.opened)
    {
	pipe_int_write(CLOSE_MESSAGE);
	ctl.opened=0;
    }
}


/* 
 * Read information coming from the window in a BLOCKING way
 */
static int ctl_blocking_read(int32 *valp)
{
  int command;
  int new_volume;
  int new_centiseconds;

  pipe_int_read(&command);
  
  while (1)    /* Loop after pause sleeping to treat other buttons! */
      {

	  switch(command)
	      {
	      case MOTIF_CHANGE_VOLUME:
		  pipe_int_read(&new_volume);
		  *valp= new_volume - amplification ;
		  return RC_CHANGE_VOLUME;
		  
	      case MOTIF_CHANGE_LOCATOR:
		  pipe_int_read(&new_centiseconds);
		  *valp= new_centiseconds*(play_mode->rate / 100) ;
		  return RC_JUMP;
		  
	      case MOTIF_QUIT:
		  return RC_QUIT;
		
	      case MOTIF_PLAY_FILE:
		  return RC_LOAD_FILE;		  
		  
	      case MOTIF_NEXT:
		  return RC_NEXT;
		  
	      case MOTIF_PREV:
		  return RC_REALLY_PREVIOUS;
		  
	      case MOTIF_RESTART:
		  return RC_RESTART;
		  
	      case MOTIF_FWD:
		  *valp=play_mode->rate;
		  return RC_FORWARD;
		  
	      case MOTIF_RWD:
		  *valp=play_mode->rate;
		  return RC_BACK;
	      }
	  
	  
	  if (command==MOTIF_PAUSE)
	      {
		  pipe_int_read(&command); /* Blocking reading => Sleep ! */
		  if (command==MOTIF_PAUSE)
		      return RC_NONE; /* Resume where we stopped */
	      }
	  else 
	      {
		  fprintf(stderr,"UNKNOWN RC_MESSAGE %i\n",command);
		  return RC_NONE;
	      }
      }

}

/* 
 * Read information coming from the window in a non blocking way
 */
static int ctl_read(int32 *valp)
{
  int num;

  /* We don't wan't to lock on reading  */
  num=pipe_read_ready(); 

  if (num==0)
      return RC_NONE;
  
  return(ctl_blocking_read(valp));
}

static void ctl_pass_playing_list(int number_of_files, char *list_of_files[])
{
    int i=0;
    char file_to_play[1000];
    int command;
    int32 val;

    /* Pass the list to the interface */
    pipe_int_write(FILE_LIST_MESSAGE);
    pipe_int_write(number_of_files);
    for (i=0;i<number_of_files;i++)
	pipe_string_write(list_of_files[i]);
    
    /* Ask the interface for a filename to play -> begin to play automatically */
    pipe_int_write(NEXT_FILE_MESSAGE);
    
    command = ctl_blocking_read(&val);

    /* Main Loop */
    for (;;)
	{ 
	    if (command==RC_LOAD_FILE)
		{
		    /* Read a LoadFile command */
		    pipe_string_read(file_to_play);
		    command=play_midi_file(file_to_play);
		}
	    else
		{
		    if (command==RC_QUIT)
			return;
		    if (command==RC_ERROR)
			command=RC_TUNE_END; /* Launch next file */
	    

		    switch(command)
			{
			case RC_NEXT:
			    pipe_int_write(NEXT_FILE_MESSAGE);
			    break;
			case RC_REALLY_PREVIOUS:
			    pipe_int_write(PREV_FILE_MESSAGE);
			    break;
			case RC_TUNE_END:
			    pipe_int_write(TUNE_END_MESSAGE);
			    break;
			default:
			    printf("PANIC !!! OTHER COMMAND ERROR ?!?! %i\n",command);
			}
		    
		    command = ctl_blocking_read(&val);
		}
	}
}
