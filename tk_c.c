/*================================================================
 *
 * The Tcl/Tk interface for Timidity
 * written by Takashi Iwai (iwai@dragon.mm.t.u-tokyo.ac.jp)
 *
 * Most of the following codes are derived from both motif_ctl.c
 * and motif_pipe.c.  The communication between Tk program and
 * timidity is established by a pipe stream as in Motif interface.
 * On the contrast to motif, the stdin and stdout are assigned
 * as pipe i/o in Tk interface.
 *
 *================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <sys/ioctl.h>

#include "config.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "output.h"
#include "controls.h"


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

static void pipe_printf(char *fmt, ...);
static void pipe_puts(char *str);
static int pipe_gets(char *str, int maxlen);
static void pipe_open();
static void pipe_error(char *st);
static int pipe_read_ready();

/**********************************************/

#define ctl tk_control_mode

ControlMode ctl= 
{
  "Tcl/Tk interface", 'k',
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
	if (!ctl.opened) {
		vfprintf(stderr, fmt, ap);
		fprintf(stderr, "\n");
	} else if (ctl.trace_playing) {
		vsprintf(local, fmt, ap);
		pipe_printf("CMSG %d", type);
		pipe_puts(local);
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
	pipe_printf("TIME %d", centisecs);
}

static void ctl_master_volume(int mv)
{
	pipe_printf("MVOL %d", mv);
}

static void ctl_file_name(char *name)
{
	pipe_printf("FILE %s", name);
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
    
	pipe_printf("CURT %d %d", centisecs, v);
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
	ctl.trace_playing=1;	/* Default mode with Tcl/Tk interface */
  
	/* The child process won't come back from this call  */
	pipe_open();

	return 0;
}

/* Tells the window to disapear */
static void ctl_close(void)
{
	if (ctl.opened) {
		pipe_puts("QUIT");
		ctl.opened=0;
	}
}


/* 
 * Read information coming from the window in a BLOCKING way
 */

/* commands are: PREV, NEXT, QUIT, STOP, LOAD, JUMP, VOLM */

static int ctl_blocking_read(int32 *valp)
{
	char buf[256], *tok, *arg;
	int rc;
	int new_volume;
	int new_centiseconds;

	rc = pipe_gets(buf, sizeof(buf)-1);
	tok = strtok(buf, " ");

	while (1)/* Loop after pause sleeping to treat other buttons! */
	{
		switch (*tok) {
		case 'V':
			if ((arg = strtok(NULL, " ")) != NULL) {
				new_volume = atoi(arg);
				*valp= new_volume - amplification ;
				return RC_CHANGE_VOLUME;
			}
			return RC_NONE;
		  
		case 'J':
			if ((arg = strtok(NULL, " ")) != NULL) {
				new_centiseconds = atoi(arg);
				*valp= new_centiseconds*(play_mode->rate / 100) ;
				return RC_JUMP;
			}
			return RC_NONE;
		  
		case 'Q':
			return RC_QUIT;
		
		case 'L':
			return RC_LOAD_FILE;		  
		  
		case 'N':
			return RC_NEXT;
		  
		case 'P':
			/*return RC_REALLY_PREVIOUS;*/
			return RC_PREVIOUS;
		  
		case 'R':
			return RC_RESTART;
		  
		case 'F':
			*valp=play_mode->rate;
			return RC_FORWARD;
		  
		case 'B':
			*valp=play_mode->rate;
			return RC_BACK;
		}
	  
	  
		if (*tok == 'S') {
			pipe_gets(buf, sizeof(buf)-1);
			tok = strtok(buf, " ");
			if (*tok == 'S')
				return RC_NONE; /* Resume where we stopped */
		}
		else {
			fprintf(stderr,"UNKNOWN RC_MESSAGE %s\n", tok);
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
	char local[1000];
	int command;
	int32 val;

	/* Pass the list to the interface */
	pipe_printf("LIST %d", number_of_files);
	for (i=0;i<number_of_files;i++)
		pipe_puts(list_of_files[i]);
    
	/* Ask the interface for a filename to play -> begin to play automatically */
	/*pipe_puts("NEXT");*/
	command = ctl_blocking_read(&val);

	/* Main Loop */
	for (;;)
	{ 
		if (command==RC_LOAD_FILE)
		{
			/* Read a LoadFile command */
			pipe_gets(local, sizeof(local)-1);
			command=play_midi_file(local);
		}
		else
		{
			if (command==RC_QUIT) {
				/* if really QUIT */
				pipe_gets(local, sizeof(local)-1);
				if (*local == 'Z')
					return;
				/* only stop playing..*/
			}
			else if (command==RC_ERROR)
				command=RC_TUNE_END; /* Launch next file */
	    

			switch(command)
			{
			case RC_NEXT:
				pipe_puts("NEXT");
				break;
			case RC_REALLY_PREVIOUS:
				pipe_puts("PREV");
				break;
			case RC_TUNE_END:
				pipe_puts("TEND");
				break;
			case RC_RESTART:
				pipe_puts("RSTA");
				break;
			}
		    
			command = ctl_blocking_read(&val);
		}
	}
}


/*
 */

static int pipeAppli[2],pipePanel[2]; /* Pipe for communication with Tcl/Tk process   */
static int fpip_in, fpip_out;	/* in and out depends in which process we are */
static int pid;	               /* Pid for child process */

static void pipe_open()
{
	int res;
    
	res=pipe(pipeAppli);
	if (res!=0) pipe_error("PIPE_APPLI CREATION");
    
	res=pipe(pipePanel);
	if (res!=0) pipe_error("PIPE_PANEL CREATION");
    
	if ((pid=fork())==0)	/*child*/
	{
		close(pipePanel[1]); 
		close(pipeAppli[0]);
	    
		dup2(pipePanel[0], fileno(stdin));
		close(pipePanel[0]);
		dup2(pipeAppli[1], fileno(stdout));
		close(pipeAppli[1]);
	    
		execlp(WISH, WISH, "-f", TKPROGPATH, NULL);
		/* Won't come back from here */
		fprintf(stderr,"WARNING: come back from TkMidity\n");
		exit(0);
	}
    
	close(pipePanel[0]);
	close(pipeAppli[1]);
    
	fpip_in= pipeAppli[0];
	fpip_out= pipePanel[1];
}


#if defined(sgi)
#include <sys/time.h>
#endif

#if defined(SOLARIS)
#include <sys/filio.h>
#endif

int pipe_read_ready(void)
{
#if defined(sgi)
    fd_set fds;
    int cnt;
    struct timeval timeout;

    FD_ZERO(&fds);
    FD_SET(fpip_in, &fds);
    timeout.tv_sec = timeout.tv_usec = 0;
    if((cnt = select(fpip_in + 1, &fds, NULL, NULL, &timeout)) < 0)
    {
	perror("select");
	return -1;
    }

    return cnt > 0 && FD_ISSET(fpip_in, &fds) != 0;
#else
    int num;

    if(ioctl(fpip_in,FIONREAD,&num) < 0) /* see how many chars in buffer. */
    {
	perror("ioctl: FIONREAD");
	return -1;
    }
    return num;
#endif
}


/***********************************************************************/
/* PIPE COMUNICATION                                                   */
/***********************************************************************/

static void pipe_error(char *st)
{
    fprintf(stderr,"CONNECTION PROBLEM WITH TCL/TK PROCESS IN %s BECAUSE:%s\n",
	    st,
	    sys_errlist[errno]);
    exit(1);
}


static void pipe_printf(char *fmt, ...)
{
	char buf[256];
	va_list ap;
	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	pipe_puts(buf);
}

static void pipe_puts(char *str)
{
	int len;
	char lf = '\n';
	len = strlen(str);
	write(fpip_out, str, len);
	write(fpip_out, &lf, 1);
}


int pipe_gets(char *str, int maxlen)
{
/* blocking reading */
	char *p;
	int len;

	/* at least 5 letters (4+\n) command */
	len = 0;
	for (p = str; ; p++) {
		read(fpip_in, p, 1);
		if (*p == '\n')
			break;
		len++;
	}
	*p = 0;
	return len;
}

