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

    xaw_i.c - XAW Interface
	from Tomokazu Harada <harada@prince.pe.u-tokyo.ac.jp>
	modified by Yoshishige Arai <ryo2@on.rim.or.jp>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <sys/types.h>
#include <pwd.h>
#include <sys/stat.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Dialog.h>
#ifndef WIDGET_IS_LABEL_WIDGET
#include <X11/Xaw/AsciiText.h>
#endif
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeLine.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/Scrollbar.h>
#include <X11/Xaw/Cardinals.h>
#include <X11/Xaw/Simple.h>
#include <X11/Xaw/List.h>
#include <X11/Xaw/Viewport.h>
#include "check.xbm"
#include "timidity.h"
#include "common.h"
#include "controls.h"
#include "xaw.h"
#if HAVE_STRSTR
#define strstr(s,c)	index(s,c)
#endif
#ifndef HAVE_STRNCASECMP
int strncasecmp(char *s1, char *s2, unsigned int len);
#endif

typedef struct {
  int			id;
  String		name;
  Boolean		trap;
  Boolean		bmflag;
  Widget		widget;
} ButtonRec;

typedef struct {
  char *dirname;
  char *basename;
} DirPath;

void a_print_text(Widget,char *);
static void quitCB(),playCB(),pauseCB(),stopCB(),prevCB(),nextCB(),
  forwardCB(),backCB(),repeatCB(),randomCB(),menuCB(),sndspecCB(),
  volsetCB(),volupdownCB(),volupdownAction(),toggleMark(),filemenuCB(),
  popupLoad(),popdownLoad(),a_readconfig(),a_saveconfig(),
  saveconfigAction(),setDirAction(),setDirList();
static char *expandDir();
static void completeDir();
static int configcmp();
static char *strmatch();
#ifdef ENABLE_KEY_TRANSLATION
static void pauseAction(),soundkeyAction(),speedAction(),voiceAction();
#endif

extern void a_pipe_write(char *);
extern int a_pipe_read(char *,int);

static Widget title_mb,title_sm,time_l, popup_load, popup_load_f, load_d;
static Widget load_vport, load_flist, cwd_l, load_info;
#ifdef MSGWINDOW
static Widget lyric_t;
static Widget lyriclist[1];
static Dimension lyric_height;
static Dimension base_height;
#endif

static Widget toplevel, base_f, file_mb, file_sm, bsb, logo_label;
static Widget quit_b,play_b,pause_b,stop_b,prev_b,next_b,fwd_b,back_b;
static Widget random_b,repeat_b,b_box,v_box,vol_l,vol_bar;
static char local_buf[300];
static char window_title[300], *w_title;
int amplitude = DEFAULT_AMPLIFICATION;
String bitmapdir = DEFAULT_PATH "/bitmaps";
Boolean arrangetitle = TRUE;
typedef struct {
  Boolean		confirmexit;
  Boolean		repeat;
  Boolean		autostart;
  Boolean		hidetext;
  Boolean		shuffle;
} Config;

Config Cfg = { FALSE, FALSE, TRUE, FALSE, FALSE };
#define MAXBITMAP 8
static char *bmfname[] = {
  "back.xbm","fwrd.xbm","next.xbm","pause.xbm","play.xbm","prev.xbm",
  "quit.xbm","stop.xbm",(char *)NULL
};
static char *iconname = "timidity.xbm";
#define BM_BACK			0
#define BM_FWRD			1
#define BM_NEXT			2
#define BM_PAUSE		3
#define BM_PLAY			4
#define BM_PREV			5
#define BM_QUIT			6
#define BM_STOP			7
static char *dotfile = NULL;

char *cfg_items[]= {"Tracing", "ConfirmExit", "Disp:file", "Disp:volume",
		"Disp:button", "RepeatPlay", "AutoStart", "Disp:text",
		"Disp:time", "Disp:trace", "CurVol", "ShufflePlay",
		"AutoExit", "CurFileMode"};
#define S_Tracing 0
#define S_ConfirmExit 1
#define S_DispFile 2
#define S_DispVolume 3 
#define S_DispButton 4
#define S_RepeatPlay 5
#define S_AutoStart 6
#define S_DispText 7
#define S_DispTime 8
#define S_DispTrace 9
#define S_CurVol 10
#define S_ShufflePlay 11
#define S_AutoExit 12
#define S_CurFileMode 13
#define CFGITEMSNUMBER 14

static Display *disp;
static int screen;
static Pixmap check_mark;

static int bm_height[MAXBITMAP], bm_width[MAXBITMAP],
  x_hot,y_hot, root_height, root_width;
static Pixmap bm_Pixmap[MAXBITMAP];
static int max_files;
char basepath[PATH_MAX];
String *dirlist = NULL;

/*static struct _app_resources {
  String bitmap_dir;
  Boolean arrange_title;
} app_resources;

static XtResource resources[] ={
#define offset(entry) XtOffset(struct _app_resources*, entry)
  {"bitmapDir", "BitmapDir", XtRString, sizeof(String),
   offset(bitmap_dir), XtRString, DEFAULT_PATH "/bitmaps"},
  {"arrangeTitle", "ArrangeTitle", XtRBoolean, sizeof(Boolean),
   offset(arrange_title), XtRBoolean, (XtPointer)True},
#undef offset
};*/

static ButtonRec file_menu[] = {
#define ID_LOAD 100
  {ID_LOAD, "load", True, False},
#define ID_SAVECONFIG 101
  {ID_SAVECONFIG, "saveconfig", True, False},
#define ID_AUTOSTART 102
  {ID_AUTOSTART, "Auto Start", True, False},
#ifdef MSGWINDOW
#define ID_HIDETXT 103
  {ID_HIDETXT, "(Un)Hide Messages", True, False},
#endif
#define ID_DUMMY -1
  {ID_DUMMY, "line", False, False},
#define ID_QUIT 104
  {ID_QUIT, "quit", True, False},
};

static void offPauseButton(void) {
  Boolean s;

  XtVaGetValues(pause_b,XtNstate,&s,NULL);
  if (s == True) {
    s=False;
    XtVaSetValues(pause_b,XtNstate,&s,NULL);
	a_pipe_write("U");
  }
}

static void offPlayButton(void) {
  Boolean s;

  XtVaGetValues(play_b,XtNstate,&s,NULL);
  if (s == True) {
    s=False;
    XtVaSetValues(play_b,XtNstate,&s,NULL);
  }
}

static Boolean onPlayOffPause(void) {
  Boolean s;
  Boolean play_on;

  play_on = False;
  XtVaGetValues(play_b,XtNstate,&s,NULL);
  if (s == False) {
    s=True;
    XtVaSetValues(play_b,XtNstate,&s,NULL);
	play_on = True;
  }
  offPauseButton();
  return(play_on);
}

/*ARGSUSED*/
static void quitCB(Widget w,XtPointer data,XtPointer dummy) {
  a_pipe_write("Q");
}

/*ARGSUSED*/
static void sndspecCB(Widget w,XtPointer data,XtPointer dummy) {
#ifdef SUPPORT_SOUNDSPEC
  a_pipe_write("g");
#else
  ;
#endif
}

/*ARGSUSED*/
static void playCB(Widget w,XtPointer data,XtPointer dummy) {
  onPlayOffPause();
  XtVaGetValues(title_mb,XtNlabel,&local_buf+2,NULL);
  a_pipe_write("P");
}

#ifdef ENABLE_KEY_TRANSLATION
/* ARGSUSED */
static void pauseAction(Widget w,XEvent *e,String *vector,Cardinal *n) {
  Boolean s;

  XtVaGetValues(play_b,XtNstate,&s,NULL);
  if (e->type == KeyPress && s == True) {
	XtVaGetValues(pause_b,XtNstate,&s,NULL);
	s ^= True;
	/*  s = ((s)? False:True);*/
	XtVaSetValues(pause_b,XtNstate,&s,NULL);
	a_pipe_write("U");
  }
}

static void soundkeyAction(Widget w,XEvent *e,String *vector,Cardinal *n) {
  a_pipe_write(*(int *)n == 0 ? "+\n":"-\n");
}

static void speedAction(Widget w,XEvent *e,String *vector,Cardinal *n) {
  a_pipe_write(*(int *)n == 0 ? ">\n":"<\n");
}

static void voiceAction(Widget w,XEvent *e,String *vector,Cardinal *n) {
  a_pipe_write(*(int *)n == 0 ? "O\n":"o\n");
}
#endif

/*ARGSUSED*/
static void pauseCB() {
  Boolean s;

  XtVaGetValues(play_b,XtNstate,&s,NULL);
  if (s == True) {
	a_pipe_write("U");
  } else {
	s = True;
	XtVaSetValues(pause_b,XtNstate,&s,NULL);
  }
}

/*ARGSUSED*/
static void stopCB(Widget w,XtPointer data,XtPointer dummy) {
  offPlayButton();
  offPauseButton();
  a_pipe_write("S");
}

/*ARGSUSED*/
static void nextCB(Widget w,XtPointer data,XtPointer dummy) {
  onPlayOffPause();
  a_pipe_write("N");
}

/*ARGSUSED*/
static void prevCB(Widget w,XtPointer data,XtPointer dummy) {
  onPlayOffPause();
  a_pipe_write("B");
}

/*ARGSUSED*/
static void forwardCB(Widget w,XtPointer data,XtPointer dummy) {
  if (onPlayOffPause()) a_pipe_write("P");
  a_pipe_write("f");
}

/*ARGSUSED*/
static void backCB(Widget w,XtPointer data,XtPointer dummy) {
  if (onPlayOffPause()) a_pipe_write("P");
  a_pipe_write("b");
}

/*ARGSUSED*/
static void repeatCB(Widget w,XtPointer data,XtPointer dummy) {
  Boolean s;
  Boolean *set= (Boolean *)data;

  if (set != NULL && *set) {
	s = *set;
	XtVaSetValues(repeat_b,XtNstate,set,NULL);
  } else {
	XtVaGetValues(repeat_b,XtNstate,&s,NULL);
  }
  if (s == True) {
    a_pipe_write("R 1");
  } else a_pipe_write("R 0");
}

/*ARGSUSED*/
static void randomCB(Widget w,XtPointer data,XtPointer dummy) {
  Boolean s;
  Boolean *set= (Boolean *)data;

  onPlayOffPause();
  if (set != NULL && *set) {
	s = *set;
	XtVaSetValues(random_b,XtNstate,set,NULL);
  } else {
	XtVaGetValues(random_b,XtNstate,&s,NULL);
  }
  if (s == True) {
    onPlayOffPause();
    a_pipe_write("D 1");
  } else {
    offPlayButton();
    offPauseButton();
    a_pipe_write("D 2");
  }
}

static void menuCB(Widget w,XtPointer data,XtPointer dummy) {
  onPlayOffPause();
  sprintf(local_buf,"L %d",((int)data)+1);
  a_pipe_write(local_buf);
}

static void volsetCB(Widget w,XtPointer data,XtPointer call_data)
{
  static int tmpval;
  char s[16];
  float percent = *(float*)call_data;
  int value = (float)(MAXVOLUME * percent);
  float thumb, l_thumb;

  if (tmpval == value) return;
  if (amplitude > MAXVOLUME) amplitude = MAXVOLUME;
  tmpval = value;
  l_thumb = thumb = (float)amplitude / (float)MAXVOLUME;
  sprintf(s, "Volume %d", tmpval);
  XtVaSetValues(vol_l, XtNlabel, s, NULL);
  sprintf(s, "V %03d\n", tmpval);
  a_pipe_write(s);
}

static void volupdownCB(Widget w,XtPointer data,XtPointer diff) {
  char s[16];
  sprintf(s, "v %03d\n", (((int)diff > 0)? (-10):10));
  a_pipe_write(s);
}

static void volupdownAction(Widget w,XEvent *e,String *v,Cardinal *n) {
  char s[16];
  sprintf(s, "v %03d\n", atoi(*v));
  a_pipe_write(s);
}

#ifndef WIDGET_IS_LABEL_WIDGET
void a_print_text(Widget w, char *st) {
  XawTextPosition pos;
  XawTextBlock tb;

  st = strcat(st, "\n");
  pos = XawTextGetInsertionPoint(w);
  tb.firstPos = 0;
  tb.length = strlen(st);
  tb.ptr = st;
  tb.format = FMT8BIT;
  XawTextReplace(w, pos, pos, &tb);
  XawTextSetInsertionPoint(w, pos + tb.length);
}
#else
void a_print_text(Widget w, char *st) {
  XtVaSetValues(w,XtNlabel,st,NULL);
}
#endif /*WIDGET_IS_LABEL_WIDGET*/

/*ARGSUSED*/
static void popupLoad(Widget w,XtPointer client_data,XtPointer call_data) {
#define DIALOG_HEIGHT 400
  int n = 0;
  Arg wargs[4];
  Position popup_x, popup_y, top_x, top_y;
  Dimension top_width;

  XtSetArg(wargs[n], XtNx, &top_x); n++;
  XtSetArg(wargs[n], XtNy, &top_y); n++;
  XtSetArg(wargs[n], XtNwidth, &top_width); n++;
  XtGetValues(toplevel, wargs, n);

  popup_x=top_x+ 20;
  popup_y=top_y+ 72;
  top_width += 100;
  if(popup_x+top_width > root_width) popup_x = root_width -top_width -20;
  if(popup_y+DIALOG_HEIGHT > root_height) popup_y = root_height -DIALOG_HEIGHT -20;

  n= 0;
  XtSetArg(wargs[n], XtNx, popup_x); n++;
  XtSetArg(wargs[n], XtNy, popup_y); n++;
  XtSetArg(wargs[n], XtNwidth, top_width); n++;
  XtSetArg(wargs[n], XtNheight, DIALOG_HEIGHT); n++;
  XtSetValues(popup_load, wargs, n);
  XtVaSetValues(load_d,XtNvalue,basepath,NULL);
  XtRealizeWidget(popup_load);
  XtPopup(popup_load,(XtGrabKind)XtGrabNone);

  n= 0;
  top_width -= 4;
  XtSetArg(wargs[n], XtNwidth, &top_width); n++;
  XtSetValues(load_vport, wargs, n);
}

static void popdownLoad(Widget w,XtPointer s,XtPointer data) {
  char *p, *p2;
  DirPath full;
  char local_buf[300];
  struct stat st;

  /* tricky way for both use of action and callback */
  if (s != NULL && data == NULL){
    p = XawDialogGetValueString(load_d);
	if (NULL != (p2 = expandDir(p, &full)))
	  p = p2;
	if(stat(p, &st) != -1) {
	  if (st.st_mode & S_IFMT & (S_IFDIR|S_IFREG|S_IFLNK)) {
		sprintf(local_buf,"X %s\n",p);
		a_pipe_write(local_buf);
	  }
	}
  }
  XtPopdown(popup_load);
}

static void toggleMark(Widget w,int id) {
  file_menu[id-100].bmflag ^= True;
  XtVaSetValues(w,XtNleftBitmap,
				((file_menu[id-100].bmflag)? check_mark:None),NULL);
}

static void filemenuCB(Widget w,XtPointer id_data, XtPointer data) {
  int *id = (int *)id_data;
#ifdef MSGWINDOW
  Dimension w1,h1;
  Dimension w2,h2;
#endif

  switch (*id) {
	case ID_LOAD:
	  popupLoad(w,NULL,NULL);
	  break;
	case ID_AUTOSTART:
	  toggleMark(w,*id);
	  break;
#ifdef MSGWINDOW
	case ID_HIDETXT:
	  XtVaGetValues(toplevel,XtNheight,&h1,NULL);
	  XtVaGetValues(toplevel,XtNwidth,&w1,NULL);
	  if (XtIsManaged(lyric_t)) {
		XtUnmanageChildren(lyriclist, XtNumber(lyriclist));
		XtMakeResizeRequest(toplevel,w1,base_height-lyric_height,&w2,&h2);
	  } else {
		XtManageChildren(lyriclist, XtNumber(lyriclist));
		XtMakeResizeRequest(toplevel,w1,h1+lyric_height,&w2,&h2);
		XtVaSetValues(lyric_t,XtNheight,&lyric_height,NULL);
	  }
	  toggleMark(w,*id);
	  break;
#endif
	case ID_SAVECONFIG:
	  a_saveconfig(dotfile);
	  break;
	case ID_QUIT:
      quitCB(w, NULL, NULL);
      break;    
  }
}

/*ARGSUSED*/
static void handle_input(XtPointer data,int *source,XtInputId *id) {
  char s[16];
  int i = 0, n;
  float thumb;

  a_pipe_read(local_buf,sizeof(local_buf));
  switch (local_buf[0]) {
    case 'T' : XtVaSetValues(time_l,XtNlabel,local_buf+2,NULL);break;
    case 'E' :
	  XtVaSetValues(title_mb,XtNlabel,(char *)strrchr(local_buf+2,' ')+1,NULL);
	  if (arrangetitle) {
		sprintf(window_title, "%s : %s", APP_CLASS, local_buf+2);
		XtVaSetValues(toplevel,XtNtitle,window_title,NULL);
	  }
	  break;
    case 'O' : offPlayButton();break;
#ifdef MSGWINDOW
    case 'L' :
	  if (XtIsManaged(lyric_t)) a_print_text(lyric_t, local_buf+2);
	  break;
#endif
    case 'Q' : exit(0);
    case 'V':
	  i=atoi(local_buf+2);
	  thumb = (float)i / (float)MAXVOLUME;
	  sprintf(s, "Volume %3d", i);
	  XtVaSetValues(vol_l, XtNlabel, s, NULL);
	  if (sizeof(thumb) > sizeof(XtArgVal)) {
		XtVaSetValues(vol_bar, XtNtopOfThumb, &thumb, NULL);
	  } else {
		XtArgVal *l_thumb = (XtArgVal *) &thumb;
		XtVaSetValues(vol_bar, XtNtopOfThumb,*l_thumb, NULL);
	  }
	  break;
    case 'v':
    case 'g':
    case '\0' :
	  break;
    case 'X':
	  n=max_files;
	  max_files+=atoi(local_buf+2);
	  for (i=n;i<max_files;i++) {
		a_pipe_read(local_buf,sizeof(local_buf));
		bsb=XtVaCreateManagedWidget(local_buf,smeBSBObjectClass,title_sm,NULL);
		XtAddCallback(bsb,XtNcallback,menuCB,(XtPointer)i);
	  }
	  break;
    default : 
	  fprintf(stderr,"Unkown message '%s' from CONTROL" NLS,local_buf);
  }
}

#ifndef HAVE_STRNCASECMP
int strncasecmp(char *s1, char *s2, unsigned int len) {
  int dif;
  while (len-- > 0) {
	if ((dif =
		 (unsigned char)tolower(*s1) - (unsigned char)tolower(*s2++)) == 0)
	  return(dif);
	if (*s1++ == '\0')
	  break;
  }
  return (0);
}
#endif

static int configcmp(char *s, int *num) {
  int i;
  char *p;
  for (i= 0; i < CFGITEMSNUMBER; i++) {
	if (0 == strncasecmp(s, cfg_items[i], strlen(cfg_items[i]))) {
	  p = s + strlen(cfg_items[i]);
	  while (*p == SPACE || *p == TAB) p++;
	  *num = atoi(p);
	  return i;
	}
  }
  return(-1);
}

static char *strmatch(char *s1, char *s2) {
  char *p = s1;

  while (*p != '\0' && *p == *s2++) p++;
  *p = '\0';
  return(s1);
}

static char *expandDir(char *path, DirPath *full) {
  static char tmp[PATH_MAX];
  static char newfull[PATH_MAX];
  char *p, *tail;

  p = path;
  if (path == NULL) {
	strcpy(tmp, "/");
	full->dirname = tmp;
	full->basename = NULL;
	strcpy(newfull, tmp); return newfull;
  } else if (NULL == (tail = strrchr(path, '/'))) {
	p = tmp;
	strcpy(p, basepath);
	full->dirname = p;
	while (*p++ != '\0') ;
	strcpy(p, path);
	sprintf(newfull,"%s/%s", basepath, path);
	full->basename = p; return newfull;
  }
  if (*p  == '/') {
	strcpy(tmp, path);
  } else {
	if (*p == '~') {
	  struct passwd *pw;

	  p++;
	  if (*p == '/' || *p == '\0') {
		pw = getpwuid(getuid());
	  } else {
		char buf[80], *bp = buf;

		while (*p != '/' && *p != '\0') *bp++ = *p++;
		*bp = '\0';
		pw = getpwnam(buf);
	  }
	  if (pw == NULL) {
		ctl->cmsg(CMSG_WARNING, VERB_NORMAL,
				  "something wrong with getting path."); return NULL;
	  }
	  while (*p == '/')	p++;
	  sprintf(tmp, "%s/%s", pw->pw_dir, p);
	} else {	/* *p != '~' */
	  sprintf(tmp, "%s/%s", basepath, path);
	}
  }
  p = tmp;
  tail = strrchr(p, '/'); *tail++ = '\0';
  full->dirname = p;
  full->basename = tail;
  sprintf(newfull,"%s/%s", p, tail);
  return newfull;
}

/*ARGSUSED*/
static void setDirAction(Widget w,XEvent *e,String *v,Cardinal *n) {
  char *p, *p2;
  struct stat st;
  DirPath full;
  XawListReturnStruct lrs;

  p = XawDialogGetValueString(load_d);
  if (NULL != (p2 = expandDir(p, &full)))
	p = p2;
  if(stat(p, &st) == -1) return;
  if(S_ISDIR(st.st_mode)) {
	strcpy(basepath,p);
	p = strrchr(basepath, '/');
	if (*(p+1) == '\0') *p = '\0';
	lrs.string = ""; dirlist[0] = (char *)NULL;
	setDirList(load_flist, cwd_l, &lrs);
  }
}

static void setDirList(Widget list, Widget label, XawListReturnStruct *lrs) {
  URL dirp;
  struct stat st;
  char *p, currdir[PATH_MAX], filename[PATH_MAX];
  int i, d_num, f_num;

  strcpy(currdir, basepath);

  if(!strncmp("../", lrs->string, 3)) {
	if(currdir != (p = strrchr(currdir, '/')))
	  *p = '\0';
	else
	  currdir[1] = '\0';
  } else {
	if(currdir[1] != '\0') strcat(currdir, "/");
	strcat(currdir, lrs->string);
	if(stat(currdir, &st) == -1) return;
	if(!S_ISDIR(st.st_mode)) {
	  XtVaSetValues(load_d,XtNvalue,currdir,NULL); return;
	}
	currdir[strlen(currdir)-1] = '\0';
  }	
  if (NULL != (dirp=url_dir_open(currdir))) {
	char *fullpath;
	MBlockList pool;
	init_mblock(&pool);

	for(i= 0; dirlist[i] != NULL; i++)
	  XtFree(dirlist[i]);
	i = 0; d_num = 0; f_num = 0;
	while (url_gets(dirp, filename, sizeof(filename)) != NULL
		   && i < MAX_DIRECTORY_ENTRY) {
	  fullpath = (char *)new_segment(&pool,strlen(currdir) +strlen(filename) +2);
	  sprintf(fullpath, "%s/%s", currdir, filename);
	  if(stat(fullpath, &st) == -1) continue;
	  if(filename[0] == '.' && filename[1] == '\0') continue;
	  if (currdir[0] == '/' && currdir[1] == '\0' && filename[0] == '.'
		  && filename[1] == '.' && filename[2] == '\0') continue;
	  if(S_ISDIR(st.st_mode)) {
		strcat(filename, "/"); d_num++;
	  } else {
		f_num++;
	  }
	  dirlist[i] = XtMalloc(sizeof(char) * strlen(filename) + 1);
	  strcpy(dirlist[i++], filename);
	}
	dirlist[i] = NULL;
	sprintf(local_buf, "%d Directories, %d Files", d_num, f_num);
	XawListChange(list, dirlist, 0,0,True);
	XtVaSetValues(label,XtNlabel,currdir,NULL);
	XtVaSetValues(load_info,XtNlabel,local_buf,NULL);
	strcpy(basepath, currdir);
  }
}

/*ARGSUSED*/
static void completeDir(Widget w,XEvent *e, XtPointer data)
{
  char *p;
  DirPath full;
  
  p = XawDialogGetValueString(load_d);
  if (!expandDir(p, &full))
	ctl->cmsg(CMSG_WARNING,VERB_NORMAL,"something wrong with getting path.");
  if(full.basename != NULL) {
	int len, match = 0;
	char filename[PATH_MAX], matchstr[PATH_MAX];
	char *fullpath;
	URL dirp;

	len = strlen(full.basename);
	if (NULL != (dirp=url_dir_open(full.dirname))) {
	  MBlockList pool;
	  init_mblock(&pool);

	  while (url_gets(dirp, filename, sizeof(filename)) != NULL) {
		if (!strncmp(full.basename, filename, len)) {
		  struct stat st;

		  fullpath = (char *)new_segment(&pool,
							strlen(full.dirname) + strlen(filename) + 2);
		  sprintf(fullpath, "%s/%s", full.dirname, filename);

		  if (stat(fullpath, &st) == -1)
			continue;
		  if (!match)
			strcpy(matchstr, filename);
		  else
			strmatch(matchstr, filename);
		  match++;
		  if(S_ISDIR(st.st_mode) && (!strcmp(filename,full.basename))) {
			strcpy(matchstr, filename);
			strcat(matchstr, "/"); match = 1; break;
		  }
		}
	  }
	  if (match) {
		sprintf(fullpath, "%s/%s", full.dirname, matchstr);
		XtVaSetValues(load_d,XtNvalue,fullpath,NULL);
		XawTextSetInsertionPoint(w,strlen(fullpath));
	  }
	  url_close(dirp);
	  reuse_mblock(&pool);
	}
  }
}

#define SSIZE 256
static void a_readconfig (Config *Cfg) {
  char s[SSIZE];
  char *home, c = ' ', *p;
  FILE *fp;
  int i, k;

  if (NULL == (home=getenv("HOME"))) home=getenv("home");
  if (home != NULL) {
	dotfile = (char *)XtMalloc(sizeof(char) * PATH_MAX);
	sprintf(dotfile, "%s/%s", home, INITIAL_CONFIG);
	if (NULL != (fp=fopen(dotfile, "r"))) {
	  while (c != EOF) {
		p = s;
		for(i=0;;i++) {
		  c = fgetc(fp);
		  if (c == LF || c == EOF || i > SSIZE) break;
		  *p++ = c;
		}
		*p = (char)NULL;
		if (0 != strncasecmp(s, "set ", 4)) continue;
		switch (configcmp(s+4, &k)) {
		case S_RepeatPlay:
		  Cfg->repeat = (Boolean)k; break;
		case S_AutoStart:
		  Cfg->autostart = (Boolean)k; break;
#ifdef MSGWINDOW
		case S_DispText:
		  Cfg->hidetext = (Boolean)(k ^ 1);
		  break;
#endif
		case S_ShufflePlay:
		  Cfg->shuffle = (Boolean)k; break;
		}
	  }

	  fclose(fp);
	}
  }
}

static void saveconfigAction(Widget w,XEvent e,XtPointer data) {
  a_saveconfig(dotfile);
}

static void a_saveconfig (char *file) {
  FILE *fp;
  Boolean s1, s2;

  if ('\0' != *file) {
	if (NULL != (fp=fopen(file, "w"))) {
	  XtVaGetValues(repeat_b,XtNstate,&s1,NULL);
	  XtVaGetValues(random_b,XtNstate,&s2,NULL);
	  fprintf(fp,"set %s %d\n",cfg_items[S_ConfirmExit],(int)Cfg.confirmexit);
	  fprintf(fp,"set %s %d\n",cfg_items[S_RepeatPlay],(int)s1);
	  fprintf(fp,"set %s %d\n",cfg_items[S_AutoStart],(int)file_menu[ID_AUTOSTART-100].bmflag);
#ifdef MSGWINDOW
	  fprintf(fp,"set %s %d\n",cfg_items[S_DispText],(int)(file_menu[ID_HIDETXT-100].bmflag ^ TRUE));
#endif
	  fprintf(fp,"set %s %d\n",cfg_items[S_ShufflePlay],(int)s2);
	  fclose(fp);
	} else {
	  fprintf(stderr, "cannot open initializing file '%s'.\n", file);
	}
  }
}

void a_start_interface(int pipe_in) {
  static XtActionsRec actions[] ={
	{"do-quit",(XtActionProc)quitCB},
	{"fix-menu", (XtActionProc)filemenuCB},
	{"do-complete", (XtActionProc)completeDir},
	{"do-chgdir", (XtActionProc)setDirAction},
#ifdef ENABLE_KEY_TRANSLATION
	{"do-dialog-button",(XtActionProc)popdownLoad},
	{"do-load",(XtActionProc)popupLoad},
	{"do-save",(XtActionProc)saveconfigAction},
	{"do-play",(XtActionProc)playCB},
	{"do-sndspec",(XtActionProc)sndspecCB},
	{"do-pause",(XtActionProc)pauseAction},
	{"do-stop",(XtActionProc)stopCB},
	{"do-next",(XtActionProc)nextCB},
	{"do-prev",(XtActionProc)prevCB},
	{"do-forward",(XtActionProc)forwardCB},
	{"do-back",(XtActionProc)backCB},
	{"do-key",(XtActionProc)soundkeyAction},
	{"do-speed",(XtActionProc)speedAction},
	{"do-voice",(XtActionProc)voiceAction},
#endif
	{"do-volset",(XtActionProc)volsetCB},
	{"do-volupdown",(XtActionProc)volupdownAction},
  };

  static String fallback_resources[]={
	"*Label.font: -adobe-helvetica-bold-o-*-*-14-*-75-75-*-*-*-*",
	"*Text*fontSet: -misc-fixed-medium-r-normal--14-*-*-*-*-*-*-*",
	"*Text*background: gray85",
	"*Text*scrollbar*background: gray75",
	"*Scrollbar*background: gray85",
	"*Label.foreground: black",
	"*Label.background: #CCFF33",
	"*MenuButton.background: #CCFF33",
	"*Command.background: gray85",
	"*Dialog*background: gray75",
	"*load_dialog.OK.background: gray85",
	"*load_dialog.Cancel.background: gray85",
	"*files.background: gray85",
	"*clip.background: gray85",
	"*Command.foreground: MediumBlue",
	"*Toggle.background: gray85",
	"*Toggle.foreground: MediumBlue",
	"*Command.font: -adobe-helvetica-medium-o-*-*-12-*-75-75-*-*-*-*",
	"*Toggle.font: -adobe-helvetica-medium-o-*-*-12-*-75-75-*-*-*-*",
	"*MenuButton.translations:<EnterWindow>:	highlight()\\n\
		<LeaveWindow>:	reset()\\n\
		Any<BtnDown>:	reset() fix-menu() PopupMenu()",
	"*base_form.background: gray75",
	"*button_box.background: gray75",
	"*button_box.borderWidth: 0",
	"*button_box.horizDistance: 4",
	"*button_box.resizable: False",
	"*file_menubutton.menuName: file_simplemenu",
	"*file_menubutton.label: file...",
	"*MenuButton.font: -adobe-helvetica-bold-o-*-*-12-*-75-75-*-*-*-*",
	"*file_menubutton.width: 60",
	"*file_menubutton.height: 28",
	"*file_menubutton.horizDistance: 6",
	"*file_menubutton.vertDistance: 4",
	"*file_simplemenu.SmeBSB.font: -adobe-helvetica-medium-r-*-*-12-*-75-75-*-*-*-*",
	"*title_simplemenu.SmeBSB.font: -adobe-helvetica-medium-r-*-*-12-*-75-75-*-*-*-*",
	"*file_simplemenu.background: Gray85",
	"*title_simplemenu.background: Gray85",
	"*file_simplemenu.width: 180",
	"*title_simplemenu.width: 200",
	"*title_menubutton.menuName: title_simplemenu",
	"*title_menubutton.label: ------",
	"*title_menubutton.width: 200",
	"*title_menubutton.height: 28",
	"*title_menubutton.horizDistance: 6",
	"*title_menubutton.vertDistance: 4",
	"*title_menubutton.fromHoriz: file_menubutton",
	"*time_label.width: 108",
	"*time_label.height: 28",
	"*time_label.fromHoriz: title_menubutton",
	"*time_label.horizDistance: 1",
	"*time_label.vertDistance: 4",
	"*time_label.label: 00:00 / -----",
	"*button_box.height: 42",
	"*play_button.width: 32",
	"*play_button.height: 32",
	"*play_button.horizDistance: 1",
	"*play_button.vertDistance: 9",
	"*pause_button.width: 32",
	"*pause_button.height: 32",
	"*pause_button.horizDistance: 1",
	"*pause_button.vertDistance: 1",
	"*stop_button.width: 32",
	"*stop_button.height: 32",
	"*stop_button.horizDistance: 1",
	"*stop_button.vertDistance: 1",
	"*prev_button.width: 32",
	"*prev_button.height: 32",
	"*prev_button.horizDistance: 1",
	"*prev_button.vertDistance: 1",
	"*back_button.width: 32",
	"*back_button.height: 32",
	"*back_button.horizDistance: 1",
	"*back_button.vertDistance: 1",
	"*fwd_button.width: 32",
	"*fwd_button.height: 32",
	"*fwd_button.horizDistance: 1",
	"*fwd_button.vertDistance: 1",
	"*next_button.width: 32",
	"*next_button.height: 32",
	"*next_button.horizDistance: 1",
	"*next_button.vertDistance: 1",
	"*quit_button.width: 32",
	"*quit_button.height: 32",
	"*quit_button.horizDistance: 1",
	"*quit_button.vertDistance: 1",
	"*random_button.width: 60",
	"*random_button.height: 16",
	"*random_button.horizDistance: 12",
	"*random_button.vertDistance: 6",
	"*random_button.label: random",
	"*repeat_button.width: 60",
	"*repeat_button.height: 16",
	"*repeat_button.horizDistance: 12",
	"*repeat_button.vertDistance: 2",
	"*repeat_button.label: repeat",
#ifdef MSGWINDOW
	"*lyric_text.fromVert: volume_box",
	"*lyric_text.width: 380",
	"*lyric_text.vertDistance: 4",
#ifndef WIDGET_IS_LABEL_WIDGET
	"*lyric_text.horizDistance: 6",
	"*lyric_text.height: 240",
	"*lyric_text.background: gray85",
	"*lyric_text.foreground: black",
	"*lyric_text.scrollVertical: WhenNeeded",
#else
	"*lyric_text.height: 30",
	"*lyric_text.fontSet: -*-*-medium-r-normal--14-*",
	"*lyric_text.label: MessageWindow",
#endif
#ifdef I18N
	"*lyric_text.international: True",
#else
	"*lyric_text.international: False",
#endif
#endif
	"*logo_label.font: -adobe-helvetica-bold-o-*-*-14-*-75-75-*-*-*-*",
	"*logo_label.label: Timidity",
	"*logo_label.foreground: MediumBlue",
	"*logo_label.background: gray75",
	"*logo_label.borderWidth: 0",
	"*logo_label.width: 72",
	"*logo_label.height: 24",
	"*logo_label.vertDistance: 8",
	"*logo_label.horizDistance: 2",
	"*volume_box.width: 320",
	"*volume_box.height: 100",
	"*volume_box.background: gray75",
	"*volume_label.background: gray75",
	"*volume_label.vertDistance: 0",
	"*volume_box.vertDistance: 2",
	"*volume_box.borderWidth: 0",
	"*volume_label.font: -adobe-helvetica-bold-r-*-*-12-*-75-75-*-*-*-*",
	"*volume_label.borderWidth: 0",
	"*volume_label.label: Volume 70",
	"*volume_label.width: 260",
	"*volume_bar.length: 300",
	"*popup_load.title: Timidity <Load File>",
	"*popup_loadform.height: 400",
	"*popup_loadform.background: gray75",
	"*load_dialog.label: File Name",
	"*load_dialog*background: gray75",
	"*load_dialog.borderWidth: 0",
	"*load_dialog.height: 132",
	"*cwd_label.background: gray85",
	"*load_dialog.label.font: -adobe-helvetica-bold-r-*-*-14-*-75-75-*-*-*-*",
	"*cwd_label.font: -adobe-helvetica-medium-r-*-*-12-*-75-75-*-*-*-*",
	"*cwd_info.font: -adobe-helvetica-medium-r-*-*-12-*-75-75-*-*-*-*",
	"*cwd_info.background: gray75",
	"*BitmapDir: " DEFAULT_PATH "/bitmaps/",
#ifndef XAW3D
	"*volume_bar.translations: #override\\n\
		~Ctrl Shift<Btn1Down>: do-volupdown(-50)\\n\
		~Ctrl Shift<Btn3Down>: do-volupdown(50)\\n\
		Ctrl ~Shift<Btn1Down>: do-volupdown(-5)\\n\
		Ctrl ~Shift<Btn3Down>: do-volupdown(5)\\n\
		<Btn1Down>: MoveThumb()\\n\
		<BtnUp>: NotifyScroll(FullLength) EndScroll()",
#else
	"*volume_bar.translations: #override\\n\
		~Ctrl Shift<Btn1Down>: do-volupdown(-50)\\n\
		~Ctrl Shift<Btn3Down>: do-volupdown(50)\\n\
		Ctrl ~Shift<Btn1Down>: do-volupdown(-5)\\n\
		Ctrl ~Shift<Btn3Down>: do-volupdown(5)\\n\
		<Btn1Down>: StartScroll(Forward) MoveThumb()\\n\
		<Btn3Down>: StartScroll(Backward) MoveThumb()\\n\
		<BtnUp>: NotifyScroll(FullLength) EndScroll()",
#endif

#ifdef ENABLE_KEY_TRANSLATION
	"*file_simplemenu.load.label:	Load (Meta-N)",
	"*file_simplemenu.saveconfig.label:	Save Config (Meta-S)",
	"*file_simplemenu.quit.label:	Quit (Meta-Q, Q)",

	"*base_form.translations: #override\\n\
		~Ctrl Meta<Key>n:	do-load()\\n\
		~Ctrl Meta<Key>s:	do-save()\\n\
		~Ctrl<Key>q:		do-quit()\\n\
		~Ctrl<Key>r:		do-play()\\n\
		<Key>Return:		do-play()\\n\
		<Key>KP_Enter:		do-play()\\n\
		~Ctrl<Key>g:		do-sndspec()\\n\
		~Ctrl<Key>space:	do-pause()\\n\
		~Ctrl<Key>s:		do-stop()\\n\
		<Key>p:			do-prev()\\n\
		~Meta<Key>n:		do-next()\\n\
		<Key>f:			do-forward()\\n\
		<Key>b:			do-back()\\n\
		~Ctrl<Key>plus:		do-key()\\n\
		~Shift<Key>-:		do-key(1)\\n\
		<Key>KP_Add:		do-key()\\n\
		<Key>KP_Subtract:	do-key(1)\\n\
		~Ctrl<Key>greater:	do-speed()\\n\
		~Ctrl<Key>less:		do-speed(1)\\n\
		~Shift<Key>o:		do-voice()\\n\
		~Ctrl Shift<Key>o:	do-voice(1)\\n\
		~Ctrl ~Shift<Key>v:	do-volupdown(-10)\\n\
		~Ctrl Shift<Key>v:	do-volupdown(10)",

	"*load_dialog*translations: #override\\n\
		~Ctrl<Key>Return:	do-chgdir()\\n\
		~Ctrl<Key>KP_Enter:	do-chgdir()\\n\
		~Ctrl ~Meta<Key>Tab:	do-complete()\\n\
		Ctrl ~Shift<Key>g:	do-dialog-button(1)\\n\
		<Key>Escape:		do-dialog-button(1)",
#else
	"*file_simplemenu.load.label:	Load",
	"*file_simplemenu.saveconfig.label:	Save Config",
	"*file_simplemenu.quit.label:	Quit",
#endif
    NULL,
  };
  XtAppContext app_con;
  char cbuf[PATH_MAX];
  Pixmap bmPixmap;
  int bmwidth, bmheight;
  int i;
  int argc=1;
  float thumb, l_thumb;
  char *argv="timidity";

  XawListReturnStruct lrs;

#ifdef I18N
  XtSetLanguageProc(NULL,NULL,NULL);
#endif
  toplevel=XtVaAppInitialize(&app_con,APP_CLASS,NULL,ZERO,&argc,&argv,
						 fallback_resources,NULL);
  /*XtVaGetApplicationResources(toplevel,(caddr_t)&app_resources,resources,
						  XtNumber(resources),NULL);
  bitmapdir = app_resources.bitmap_dir;
  arrangetitle = app_resources.arrange_title;*/
  a_readconfig(&Cfg);
  disp = XtDisplay(toplevel);
  screen = DefaultScreen(disp);
  root_height = DisplayHeight(disp, screen);
  root_width = DisplayWidth(disp, screen);
  check_mark = XCreateBitmapFromData(XtDisplay(toplevel),
                                      RootWindowOfScreen(XtScreen(toplevel)),
                                      (char *) check_bits,
                                      check_width,check_height);
  for(i= 0; i < MAXBITMAP; i++) {
	sprintf(cbuf,"%s/%s",bitmapdir,bmfname[i]);
	XReadBitmapFile(disp,RootWindow(disp,screen),cbuf,&bm_width[i],&bm_height[i],
					&bm_Pixmap[i],&x_hot,&y_hot);
  }
#ifndef STDC_HEADERS
  getwd(basepath);
#else
  getcwd(basepath, sizeof(basepath));
#endif
  if (!strlen(basepath)) strcat(basepath, "/");

  XtAppAddActions(app_con, actions, XtNumber(actions));
  base_f=XtVaCreateManagedWidget("base_form",formWidgetClass,toplevel,NULL);
  file_mb=XtVaCreateManagedWidget("file_menubutton",menuButtonWidgetClass,base_f,NULL);
  file_sm=XtVaCreatePopupShell("file_simplemenu",simpleMenuWidgetClass,file_mb,NULL);
  title_mb=XtVaCreateManagedWidget("title_menubutton",menuButtonWidgetClass,base_f,NULL);
  title_sm=XtVaCreatePopupShell("title_simplemenu",simpleMenuWidgetClass,title_mb,NULL);
  time_l=XtVaCreateManagedWidget("time_label",labelWidgetClass,base_f,NULL);
  b_box=XtVaCreateManagedWidget("button_box",boxWidgetClass,base_f,
								XtNorientation, XtorientHorizontal,
								XtNfromVert,title_mb,NULL);
  v_box=XtVaCreateManagedWidget("volume_box",boxWidgetClass,base_f,
								XtNfromVert,b_box,NULL);
  vol_l=XtVaCreateManagedWidget("volume_label",labelWidgetClass,v_box,
								NULL);
  vol_bar=XtVaCreateManagedWidget("volume_bar",scrollbarWidgetClass,v_box,
								  XtNorientation, XtorientHorizontal,
								  XtNfromVert,vol_l,
								  XtNtopOfThumb, &l_thumb,
								  NULL);
  l_thumb = thumb = (float)amplitude / (float)MAXVOLUME;
  if (sizeof(thumb) > sizeof(XtArgVal)) {
	XtVaSetValues(vol_bar,XtNtopOfThumb,&thumb,NULL);
  } else {
	XtArgVal * l_thumb = (XtArgVal *) &thumb;
	XtVaSetValues(vol_bar,XtNtopOfThumb,*l_thumb,NULL);
  }
  play_b=XtVaCreateManagedWidget("play_button",toggleWidgetClass,b_box,
								 XtNbitmap,bm_Pixmap[BM_PLAY],
								 NULL);
  pause_b=XtVaCreateManagedWidget("pause_button",toggleWidgetClass,b_box,
								 XtNbitmap,bm_Pixmap[BM_PAUSE],
								 XtNfromHoriz, play_b,
								 NULL);
  stop_b=XtVaCreateManagedWidget("stop_button",commandWidgetClass,b_box,
								 XtNbitmap,bm_Pixmap[BM_STOP],
								 XtNfromHoriz,pause_b,NULL);
  prev_b=XtVaCreateManagedWidget("prev_button",commandWidgetClass,b_box,
								 XtNbitmap,bm_Pixmap[BM_PREV],
								 XtNfromHoriz,stop_b,NULL);
  back_b=XtVaCreateManagedWidget("back_button",commandWidgetClass,b_box,
								 XtNbitmap,bm_Pixmap[BM_BACK],
								 XtNfromHoriz,prev_b,NULL);
  fwd_b=XtVaCreateManagedWidget("fwd_button",commandWidgetClass,b_box,
								 XtNbitmap,bm_Pixmap[BM_FWRD],
								 XtNfromHoriz,back_b,NULL);
  next_b=XtVaCreateManagedWidget("next_button",commandWidgetClass,b_box,
								 XtNbitmap,bm_Pixmap[BM_NEXT],
								 XtNfromHoriz,fwd_b,NULL);
  quit_b=XtVaCreateManagedWidget("quit_button",commandWidgetClass,b_box,
								 XtNbitmap,bm_Pixmap[BM_QUIT],
								 XtNfromHoriz,next_b,NULL);
  random_b=XtVaCreateManagedWidget("random_button",toggleWidgetClass,base_f,
								   XtNfromHoriz,b_box,
								   XtNfromVert,time_l,NULL);
  repeat_b=XtVaCreateManagedWidget("repeat_button",toggleWidgetClass,base_f,
								   XtNfromHoriz,b_box,
								   XtNfromVert,random_b,NULL);
  logo_label=XtVaCreateManagedWidget("logo_label",labelWidgetClass,base_f,
								   XtNfromHoriz,v_box,
								   XtNfromVert,repeat_b,NULL);

  popup_load=XtVaCreatePopupShell("popup_load",transientShellWidgetClass,toplevel,
								  NULL);  
  popup_load_f= XtVaCreateManagedWidget("popup_loadform",formWidgetClass,popup_load,
								 NULL);
  load_d=XtVaCreateManagedWidget("load_dialog",dialogWidgetClass,popup_load_f,
								 XtNresizable, True,
								 NULL);
  cwd_l = XtVaCreateManagedWidget("cwd_label",labelWidgetClass,popup_load_f,
				XtNlabel,basepath, XtNborderWidth,0, XtNfromVert,load_d,
				XtNresizable,True, NULL);
  load_vport = XtVaCreateManagedWidget("vport",viewportWidgetClass, popup_load_f,
				XtNfromVert,cwd_l,	XtNallowHoriz,True, XtNallowVert,True,
				XtNwidth,250, XtNheight,200, NULL);
  load_flist = XtVaCreateManagedWidget("files",listWidgetClass,load_vport,
				XtNverticalList,True, XtNforceColumns,False,
				XtNdefaultColumns, 3, NULL);
  load_info = XtVaCreateManagedWidget("cwd_info",labelWidgetClass,popup_load_f,
				XtNborderWidth,0, XtNwidth,250, XtNheight,32,
				XtNfromVert,load_vport, NULL);
  XawDialogAddButton(load_d, "OK", popdownLoad,"Y");
  XawDialogAddButton(load_d, "Cancel", popdownLoad,NULL);
#ifdef MSGWINDOW
#ifndef WIDGET_IS_LABEL_WIDGET
  lyric_t=XtVaCreateManagedWidget("lyric_text",asciiTextWidgetClass,base_f,
				XtNwrap,XawtextWrapWord, XtNeditType,XawtextAppend,
				XtNfromVert,v_box, NULL);
#else
  lyric_t=XtVaCreateManagedWidget("lyric_text",labelWidgetClass,base_f,NULL);
#endif
  lyriclist[0] = lyric_t;
#endif

  XtAddCallback(quit_b,XtNcallback,quitCB,NULL);
  XtAddCallback(play_b,XtNcallback,playCB,NULL);
  XtAddCallback(pause_b,XtNcallback,pauseCB,NULL);
  XtAddCallback(stop_b,XtNcallback,stopCB,NULL);
  XtAddCallback(prev_b,XtNcallback,prevCB,NULL);
  XtAddCallback(next_b,XtNcallback,nextCB,NULL);
  XtAddCallback(fwd_b,XtNcallback,forwardCB,NULL);
  XtAddCallback(back_b,XtNcallback,backCB,NULL);
  XtAddCallback(random_b,XtNcallback,randomCB,NULL);
  XtAddCallback(repeat_b,XtNcallback,repeatCB,NULL);
  XtAddCallback(vol_bar,XtNjumpProc,volsetCB,NULL);
  XtAddCallback(vol_bar,XtNscrollProc,volupdownCB,NULL);
  XtAppAddInput(app_con,pipe_in,(XtPointer)XtInputReadMask,handle_input,NULL);
  XtAddCallback(load_flist,XtNcallback,(XtCallbackProc)setDirList,cwd_l);

  XtRealizeWidget(toplevel);

  dirlist =(String *)malloc(sizeof(String)* MAX_DIRECTORY_ENTRY);
  dirlist[0] = (char *)NULL;
  lrs.string = "";
  setDirList(load_flist, cwd_l, &lrs);
  XtSetKeyboardFocus(base_f, base_f);
#ifdef MSGWINDOW
  XtSetKeyboardFocus(lyric_t, base_f);
#endif
  XtSetKeyboardFocus(popup_load, load_d);
  XtOverrideTranslations (toplevel,
			XtParseTranslationTable ("<Message>WM_PROTOCOLS: do-quit()"));
  sprintf(cbuf,"%s/%s",bitmapdir,iconname);
  XReadBitmapFile(disp,RootWindow(disp,screen),cbuf,
				  &bmwidth,&bmheight,&bmPixmap,&x_hot,&y_hot);
  XtVaSetValues(toplevel,XtNiconPixmap,bmPixmap,NULL);
  strcpy(window_title,APP_CLASS);
  w_title = strncat(window_title," : ",3);
  w_title += sizeof(APP_CLASS)+ 2;
#ifdef MSGWINDOW
  XtVaGetValues(toplevel,XtNheight,&base_height,NULL);
  XtVaGetValues(lyric_t,XtNheight,&lyric_height,NULL);
  a_print_text(lyric_t,strcpy(local_buf,"<< Timidity Messages >>"));
#endif

  a_pipe_write("READY");

  while (1) {
	a_pipe_read(local_buf,sizeof(local_buf));
	if (local_buf[0] < 'A') break;
#ifdef MSGWINDOW
	a_print_text(lyric_t,local_buf+2);
#endif
  }
  max_files=atoi(local_buf);
  for (i=0;i<max_files;i++) {
    a_pipe_read(local_buf,sizeof(local_buf));
    bsb=XtVaCreateManagedWidget(local_buf,smeBSBObjectClass,title_sm,NULL);
	XtAddCallback(bsb,XtNcallback,menuCB,(XtPointer)i);
  }

  for (i = 0; i < XtNumber(file_menu); i++) {
	bsb=XtVaCreateManagedWidget(file_menu[i].name,
					(file_menu[i].trap ? smeBSBObjectClass:smeLineObjectClass),
					file_sm,XtNleftBitmap, None,XtNleftMargin,24,NULL),
	  XtAddCallback(bsb,XtNcallback,filemenuCB,(XtPointer)&file_menu[i].id);
	file_menu[i].widget = bsb;
  }

  if (Cfg.autostart) filemenuCB(file_menu[ID_AUTOSTART-100].widget,
							&file_menu[ID_AUTOSTART-100].id,NULL);
#ifdef MSGWINDOW
  if (Cfg.hidetext) filemenuCB(file_menu[ID_HIDETXT-100].widget,
						   &file_menu[ID_HIDETXT-100].id,NULL);
#endif
  if (Cfg.repeat) repeatCB(NULL,&Cfg.repeat,NULL);
  if (Cfg.shuffle) randomCB(NULL,&Cfg.shuffle,NULL);
  if (Cfg.autostart)
	playCB(NULL,NULL,NULL);
  else
	stopCB(NULL,NULL,NULL);
  XtAppMainLoop(app_con);
}
