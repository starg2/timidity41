/*

    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999 Masanao Izumo <mo@goice.co.jp>
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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    xaw_c.c - XAW Interface from
	Tomokazu Harada <harada@prince.pe.u-tokyo.ac.jp>
	Yoshishige Arai <ryo2@on.rim.or.jp>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#ifndef __WIN32__
#include <unistd.h>
#endif /* __WIN32__ */

#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "output.h"
#include "controls.h"
#include "miditrace.h"
#include "xaw.h"
static void ctl_current_time(int secs, int v);
static void ctl_lyric(int lyricid);
static int ctl_open(int using_stdin, int using_stdout);
static void ctl_close(void);
static int ctl_read(int32 *valp);
static int cmsg(int type, int verbosity_level, char *fmt, ...);
static void ctl_pass_playing_list(int number_of_files, char *list_of_files[]);
static void ctl_event(CtlEvent *e);

static void a_pipe_open(void);
static int a_pipe_ready(void);
static void ctl_master_volume(int);
static void ctl_total_time(int);
void a_pipe_write(char *);
int a_pipe_read(char *,int);
#ifndef MSGWINDOW
static void a_pipe_int_write(int);
static void a_pipe_error(char *);
#endif

static int exitflag=0,randomflag=0,repeatflag=0,selectflag=0,amp_diff=0;
static int xaw_ready=0;
static int number_of_files;
static char **list_of_files;
static char **titles;
static int *file_table;
extern int amplitude;

/**********************************************/
/* export the interface functions */

#define ctl xaw_control_mode

ControlMode ctl=
{
    "XAW interface", 'a',
    1,0,0,
    ctl_open,
    ctl_close,
    ctl_pass_playing_list,
    ctl_read,
    cmsg,
    ctl_event
};

static char local_buf[300];

/***********************************************************************/
/* Put controls on the pipe                                            */
/***********************************************************************/
#define CMSG_MESSAGE 16

static int cmsg(int type, int verbosity_level, char *fmt, ...) {
#ifndef MSGWINDOW
  char local[255];
#endif
  va_list ap;

  if ((type==CMSG_TEXT || type==CMSG_INFO || type==CMSG_WARNING) &&
      ctl.verbosity<verbosity_level)
    return 0;

  va_start(ap, fmt);

  if(!xaw_ready) {
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, NLS);
    va_end(ap);
    return 0;
  }

  if (1000 != ctl.opened) {
#ifndef MSGWINDOW
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, NLS);
#else
#ifdef HAVE_VSNPRINTF
	vsnprintf(local_buf+2,100,fmt,ap);
#else
	{
	  char *buff;
	  MBlockList pool;
	  init_mblock(&pool);
	  buff = (char *)new_segment(&pool, MIN_MBLOCK_SIZE);
	  vsprintf(buff, fmt, ap);
	  buff[MIN_MBLOCK_SIZE - 1] = '\0';
	  strncpy(local_buf + 2, buff, 100);
	  local_buf[97] = '\0';
	  reuse_mblock(&pool);
	}
#endif /* HAVE_VSNPRINTF */
	local_buf[0]='L';
	local_buf[1]=' ';
	a_pipe_write(local_buf);
#endif /* MSGWINDOW */
  } else {
#ifdef MSGWINDOW
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, NLS);
#else
	a_pipe_int_write(CMSG_MESSAGE);
	a_pipe_int_write(type);
	a_pipe_int_write(strlen(local));
	a_pipe_write(local);
#endif /* MSGWINDOW */
  }
  va_end(ap);
  return 0;
}

/*ARGSUSED*/
static char tt_str[16];

static void ctl_current_time(int sec, int v) {

  static int previous_sec=-1;
  if (sec!=previous_sec) {
    previous_sec=sec;
    sprintf(local_buf,"T %02d:%02d%s",sec / 60,sec % 60,tt_str);
    a_pipe_write(local_buf);
  }
}

static void ctl_total_time(int tt)
{
    int mins, secs=tt / play_mode->rate;
    mins=secs / 60;
    secs-=mins * 60;

    sprintf(tt_str, " /%3d:%02d", mins, secs);
    ctl_current_time(0, 0);
}

static void ctl_master_volume(int mv)
{
  sprintf(local_buf,"V %03d", mv);
  amplitude=atoi(local_buf+2);
  if (amplitude < 0) amplitude = 0;
  if (amplitude > MAXVOLUME) amplitude = MAXVOLUME;
  a_pipe_write(local_buf);
}

static void ctl_lyric(int lyricid)
{
    char *lyric;
    static int lyric_col = 2;
    static char lyric_buf[300];

    lyric = event2string(lyricid);
    if(lyric != NULL)
    {
	if(lyric[0] == ME_KARAOKE_LYRIC)
	{
	    if(lyric[1] == '/' || lyric[1] == '\\')
	    {
		lyric_buf[0] = 'L';
		lyric_buf[1] = ' ';
		sprintf(lyric_buf + 2, "%s", lyric + 2);
		a_pipe_write(lyric_buf);
		lyric_col = strlen(lyric + 2) + 2;
	    }
	    else if(lyric[1] == '@')
	    {
		lyric_buf[0] = 'L';
		lyric_buf[1] = ' ';
		if(lyric[2] == 'L')
		    sprintf(lyric_buf + 2, "Language: %s", lyric + 3);
		else if(lyric[2] == 'T')
		    sprintf(lyric_buf + 2, "Title: %s", lyric + 3);
		else
		    sprintf(lyric_buf + 2, "%s", lyric + 1);
		a_pipe_write(lyric_buf);
	    }
	    else
	    {
		lyric_buf[0] = 'L';
		lyric_buf[1] = ' ';
		sprintf(lyric_buf + lyric_col, lyric + 1);
		a_pipe_write(lyric_buf);
		lyric_col += strlen(lyric + 1);
	    }
	}
	else
	{
	    if(lyric[0] == ME_CHORUS_TEXT || lyric[0] == ME_INSERT_TEXT)
		lyric_col = 2;
	    lyric_buf[0] = 'L';
	    lyric_buf[1] = ' ';
	    sprintf(lyric_buf + lyric_col, lyric + 1);
	    a_pipe_write(lyric_buf);
	}
    }
}

/*ARGSUSED*/
static int ctl_open(int using_stdin, int using_stdout) {
  ctl.opened=1;

  /* The child process won't come back from this call  */
  a_pipe_open();

  return 0;
}

static void ctl_close(void)
{
  if (ctl.opened) {
    a_pipe_write("Q");
    ctl.opened=0;
    xaw_ready=0;
  }
}

static void xaw_add_midi_file(char *additional_path)
{
    char *files[1], **ret, **tmp;
    int i, nfiles;
    char *p;

    files[0] = additional_path;
    nfiles = 1;
    ret = expand_file_archives(files, &nfiles);
    tmp = list_of_files;
    titles=(char **)safe_realloc(titles,(number_of_files+nfiles)*sizeof(char *));
    list_of_files=(char **)safe_malloc((number_of_files+nfiles)*sizeof(char *));
    for (i=0;i<number_of_files;i++)
	list_of_files[i]=safe_strdup(tmp[i]);
    for (i=0;i<nfiles;i++) {
	p=strrchr(ret[i],'/');
	if (p==NULL) p=ret[i]; else p++;
	titles[number_of_files+i]=(char *)safe_malloc(sizeof(char)*(strlen(p)+ 9));
	list_of_files[number_of_files+i]=safe_strdup(ret[i]);
	sprintf(titles[number_of_files+i],"%d. %s",number_of_files+i+1,p);
    }

    file_table=(int *)safe_realloc(file_table,
				   (number_of_files+nfiles)*sizeof(int));
    for(i = number_of_files; i < number_of_files + nfiles; i++)
	file_table[i] = i;
    number_of_files+=nfiles;

    if(ret != files) free(ret);
    sprintf(local_buf, "X %d", nfiles);
    a_pipe_write(local_buf);
    for (i=0;i<nfiles;i++)
	a_pipe_write(titles[number_of_files-nfiles+i]);
}

/*ARGSUSED*/
static int ctl_blocking_read(int32 *valp) {
  int n;

  a_pipe_read(local_buf,sizeof(local_buf));
  for (;;) {
    switch (local_buf[0]) {
      case 'P' : return RC_LOAD_FILE;
      case 'U' : return RC_TOGGLE_PAUSE;
      case 'f': *valp=(int32)(play_mode->rate * 10);
        return RC_FORWARD;
      case 'b': *valp=(int32)(play_mode->rate * 10);
        return RC_BACK;
      case 'S' : return RC_QUIT;
      case 'N' : return RC_NEXT;
      case 'B' : return RC_REALLY_PREVIOUS;
      case 'R' : repeatflag=atoi(local_buf+2);return RC_NONE;
      case 'D' : randomflag=atoi(local_buf+2);return RC_QUIT;
      case 'F' : 
      case 'L' : selectflag=atoi(local_buf+2);return RC_QUIT;
      case 'l' : a_pipe_read(local_buf,sizeof(local_buf));
		n=atoi(local_buf+2); *valp= n * play_mode->rate;
		*valp=(int32)amp_diff; return RC_JUMP;
      case 'V' : a_pipe_read(local_buf,sizeof(local_buf));
		n=atoi(local_buf+2); amp_diff=n - amplitude;
		*valp=(int32)amp_diff; return RC_CHANGE_VOLUME;
      case 'v' : a_pipe_read(local_buf,sizeof(local_buf));
		n=atoi(local_buf+2);
		*valp=(int32)n; return RC_CHANGE_VOLUME;
	  case '+': a_pipe_read(local_buf,sizeof(local_buf));
		*valp = (int32)1; return RC_KEYUP;
	  case '-': a_pipe_read(local_buf,sizeof(local_buf));
		*valp = (int32)-1; return RC_KEYDOWN;
      case '>': a_pipe_read(local_buf,sizeof(local_buf));
		*valp = (int32)1; return RC_SPEEDUP;
      case '<': a_pipe_read(local_buf,sizeof(local_buf));
		*valp = (int32)1; return RC_SPEEDDOWN;
      case 'o': a_pipe_read(local_buf,sizeof(local_buf));
		*valp = (int32)1; return RC_VOICEINCR;
      case 'O': a_pipe_read(local_buf,sizeof(local_buf));
		*valp = (int32)1; return RC_VOICEDECR;
      case 'X': a_pipe_read(local_buf,sizeof(local_buf));
		xaw_add_midi_file(local_buf + 2);
		return RC_NONE;
      case 'g': return RC_TOGGLE_SNDSPEC;
      case 'Q' :
      default  : exitflag=1;return RC_QUIT;
    }
  }
}

static int ctl_read(int32 *valp) {
  if (a_pipe_ready()<=0) return RC_NONE;
  return ctl_blocking_read(valp);
}

static void shuffle(int n,int *a) {
  int i,j,tmp;

  for (i=0;i<n;i++) {
    j=int_rand(n);
    tmp=a[i];
    a[i]=a[j];
    a[j]=tmp;
  }
}

static void ctl_pass_playing_list(int init_number_of_files,
				  char *init_list_of_files[]) {
  int current_no,command,i;
  int32 val;
  char *p;

  /* Wait prepare 'interface' */
  a_pipe_read(local_buf,sizeof(local_buf));
  if (strcmp("READY",local_buf)) return;
  xaw_ready=1;

  number_of_files = init_number_of_files;
  list_of_files = init_list_of_files;

  /* Make title string */
  titles=(char **)safe_malloc(number_of_files*sizeof(char *));
  for (i=0;i<number_of_files;i++) {
    p=strrchr(list_of_files[i],'/');
    if (p==NULL) {
      p=list_of_files[i];
    } else p++;
    titles[i]=(char *)safe_malloc(sizeof(char)*(strlen(p)+ 9));
	sprintf(titles[i],"%d. %s",i+1,p);
  }

  /* Send title string */
  sprintf(local_buf,"%d",number_of_files);
  a_pipe_write(local_buf);
  for (i=0;i<number_of_files;i++)
	a_pipe_write(titles[i]);

  /* Make the table of play sequence */
  file_table=(int *)safe_malloc(number_of_files*sizeof(int));
  for (i=0;i<number_of_files;i++) file_table[i]=i;

  /* Draw the title of the first file */
  current_no=0;
  sprintf(local_buf,"E %s",titles[file_table[0]]);
  a_pipe_write(local_buf);

  command=ctl_blocking_read(&val);

  /* Main loop */
  for (;;) {
    /* Play file */
    if (command==RC_LOAD_FILE) {
      sprintf(local_buf,"E %s",titles[file_table[current_no]]);
      a_pipe_write(local_buf);
      command=play_midi_file(list_of_files[file_table[current_no]]);
    } else {
	  if (command==RC_CHANGE_VOLUME) amplitude+=val;
	  if (command==RC_TOGGLE_SNDSPEC) ;
      /* Quit timidity*/
	  if (exitflag) return;
	  /* Stop playing */
      if (command==RC_QUIT) {
		sprintf(local_buf,"T 00:00");
		a_pipe_write(local_buf);
		/* Shuffle the table */
		if (randomflag) {
		  current_no=0;
		  if (randomflag==1) {
			shuffle(number_of_files,file_table);
			randomflag=0;
			command=RC_LOAD_FILE;
			continue;
		  }
		  randomflag=0;
		  for (i=0;i<number_of_files;i++) file_table[i]=i;
		  sprintf(local_buf,"E %s",titles[file_table[current_no]]);
		  a_pipe_write(local_buf);
		}
		/* Play the selected file */
		if (selectflag) {
		  for (i=0;i<number_of_files;i++)
			if (file_table[i]==selectflag-1) break;
		  if (i!=number_of_files) current_no=i;
		  selectflag=0;
		  command=RC_LOAD_FILE;
		  continue;
		}
		/* After the all file played */
      } else if (command==RC_TUNE_END || command==RC_ERROR) {
		if (current_no+1<number_of_files) {
		  current_no++;
		  command=RC_LOAD_FILE;
		  continue;
		  /* Repeat */
		} else if (repeatflag) {
		  current_no=0;
		  command=RC_LOAD_FILE;
		  continue;
		  /* Off the play button */
		} else {
		  a_pipe_write("O");
		}
		/* Play the next */
      } else if (command==RC_NEXT) {
		if (current_no+1<number_of_files) current_no++;
		command=RC_LOAD_FILE;
		continue;
		/* Play the previous */
      } else if (command==RC_REALLY_PREVIOUS) {
		if (current_no>0) current_no--;
		command=RC_LOAD_FILE;
		continue;
      }
      command=ctl_blocking_read(&val);
    }
  }
}

/* ------ Pipe handlers ----- */

static int pipe_in_fd,pipe_out_fd;

extern void a_start_interface(int);

static void a_pipe_open(void) {
  int cont_inter[2],inter_cont[2];

  if (pipe(cont_inter)<0 || pipe(inter_cont)<0) exit(1);

  if (fork()==0) {
    close(cont_inter[1]);
    close(inter_cont[0]);
    pipe_in_fd=cont_inter[0];
    pipe_out_fd=inter_cont[1];
    a_start_interface(pipe_in_fd);
  }
  close(cont_inter[0]);
  close(inter_cont[1]);
  pipe_in_fd=inter_cont[0];
  pipe_out_fd=cont_inter[1];
}

void a_pipe_write(char *buf) {
  write(pipe_out_fd,buf,strlen(buf));
  write(pipe_out_fd,"\n",1);
}

#ifndef MSGWINDOW
void a_pipe_int_write(int c) {
    int len;
    len = write(pipe_out_fd,&c,sizeof(c));
    if (len!=sizeof(int))
	  a_pipe_error("PIPE_INT_WRITE");
}
#endif

static int a_pipe_ready(void) {
  fd_set fds;
  static struct timeval tv;
  int cnt;

  FD_ZERO(&fds);
  FD_SET(pipe_in_fd,&fds);
  tv.tv_sec=0;
  tv.tv_usec=0;
  if((cnt=select(pipe_in_fd+1,&fds,NULL,NULL,&tv))<0)
    return -1;
  return cnt > 0 && FD_ISSET(pipe_in_fd, &fds) != 0;
}

int a_pipe_read(char *buf,int bufsize) {
  int i;

  bufsize--;
  for (i=0;i<bufsize;i++) {
    read(pipe_in_fd,buf+i,1);
    if (buf[i]=='\n') break;
  }
  buf[i]=0;
  return 0;
}

static void ctl_event(CtlEvent *e)
{
    switch(e->type)
    {
	case CTLE_LOADING_DONE:
	  break;
	case CTLE_CURRENT_TIME:
	  ctl_current_time((int)e->v1, (int)e->v2);
	  break;
	case CTLE_PLAY_START:
	  ctl_total_time((int)e->v1);
	  break;
	case CTLE_LYRIC:
	  ctl_lyric((int)e->v1);
	  break;
	case CTLE_MASTER_VOLUME:
	  ctl_master_volume((int)e->v1);
	  break;
    }
}

/*
 * interface_<id>_loader();
 */
ControlMode *interface_a_loader(void)
{
    return &ctl;
}

#ifndef MSGWINDOW
/*
 * error message
 */
static void a_pipe_error(char *s)
{
#ifdef HAVE_STRERROR
    fprintf(stderr,"connection problem in %s because:%s" NLS, s,
			strerror(errno));
#else
    fprintf(stderr,"connection problem in %s because:%d" NLS, s, errno);
#endif
    exit(1);
}
#endif /* MSGWINDOW */
