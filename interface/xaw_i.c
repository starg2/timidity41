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
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif /* HAVE_FCNTL_H */

#include "xaw.h"
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Dialog.h>
#include <X11/Xaw/AsciiText.h>

#ifdef OFFIX
#include <OffiX/DragAndDrop.h>
#include <OffiX/DragAndDropTypes.h>
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
#include "arrow.xbm"
#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "controls.h"

#ifndef S_ISDIR
#define S_ISDIR(mode)   (((mode)&0xF000) == 0x4000)
#endif /* S_ISDIR */

#define TRACE_WIDTH 627			/* default height of trace_vport */
#define TRACEVPORT_WIDTH (TRACE_WIDTH+12)
#define TRACE_WIDTH_SHORT 388
#define TRACEV_OFS 22
#define TRACE_FOOT 16
#define TRACEH_OFS 0
#define BAR_SPACE 20
#define BAR_HEIGHT 16
static int VOLUME_LABEL_WIDTH = 80;
#define MAX_TRACE_HEIGHT 560
#if (MAX_XAW_MIDI_CHANNELS*BAR_SPACE+TRACE_FOOT+14 > MAX_TRACE_HEIGHT)
#define TRACE_HEIGHT MAX_TRACE_HEIGHT
#else
#define TRACE_HEIGHT (MAX_XAW_MIDI_CHANNELS*BAR_SPACE+TRACE_FOOT+14)
#endif
#define VOICES_NUM_OFS 6
#define TTITLE_OFS 120

#define BARSCALE2 0.31111	/* velocity scale   (60-4)/180 */
#define BARSCALE3 0.28125	/* volume scale     (40-4)/128 */
#define BARSCALE4 0.25		/* expression scale (36-4)/128 */
#define BARSCALE5 0.385827	/* expression scale (53-4)/128 */
static int currwidth = 2;
static int rotatewidth[] = {TRACE_WIDTH_SHORT, 592, TRACE_WIDTH+8};

typedef struct {
  int			col;	/* column number */
  char			**cap;	/* caption strings array */
  int			*w;  	/* column width array */
  int			*ofs;	/* column offset array */
} Tplane;
static int plane = 0;

#define TCOLUMN 9
#define T2COLUMN 10
static char *caption[TCOLUMN] =
{"ch","  vel"," vol","expr","prog","pan","pit"," instrument",
 "          keyboard"};
static char *caption2[T2COLUMN] =
{"ch","  vel"," vol","expr","prog","pan","bnk","reverb","chorus",
 "          keyboard"};

static int BARH_SPACE[TCOLUMN]= {22,60,40,36,36,36,30,106,304};
#define BARH_OFS0	(TRACEH_OFS)
#define BARH_OFS1	(BARH_OFS0+22)
#define BARH_OFS2	(BARH_OFS1+60)
#define BARH_OFS3	(BARH_OFS2+40)
#define BARH_OFS4	(BARH_OFS3+36)
#define BARH_OFS5	(BARH_OFS4+36)
#define BARH_OFS6	(BARH_OFS5+36)
#define BARH_OFS7	(BARH_OFS6+30)
#define BARH_OFS8	(BARH_OFS7+106)
static int bar0ofs[] = {BARH_OFS0,BARH_OFS1,BARH_OFS2,BARH_OFS3,
	  BARH_OFS4,BARH_OFS5,BARH_OFS6,BARH_OFS7,BARH_OFS8};

static int BARH2_SPACE[T2COLUMN]= {22,60,40,36,36,36,30,53,53,304};
#define BARH2_OFS0	(TRACEH_OFS)
#define BARH2_OFS1	(BARH2_OFS0+22)
#define BARH2_OFS2	(BARH2_OFS1+60)
#define BARH2_OFS3	(BARH2_OFS2+40)
#define BARH2_OFS4	(BARH2_OFS3+36)
#define BARH2_OFS5	(BARH2_OFS4+36)
#define BARH2_OFS6	(BARH2_OFS5+36)
#define BARH2_OFS7	(BARH2_OFS6+30)
#define BARH2_OFS8	(BARH2_OFS7+53)
#define BARH2_OFS9	(BARH2_OFS8+53)
static int bar1ofs[] = {BARH2_OFS0,BARH2_OFS1,BARH2_OFS2,BARH2_OFS3,
	  BARH2_OFS4,BARH2_OFS5,BARH2_OFS6,BARH2_OFS7,BARH2_OFS8,BARH2_OFS9};

static Tplane pl[] = {
  {TCOLUMN, caption, BARH_SPACE, bar0ofs},
  {T2COLUMN, caption2, BARH2_SPACE, bar1ofs},
};

#define CL_C 0	/* column 0 = channel */
#define CL_VE 1	/* column 1 = velocity */
#define CL_VO 2	/* column 2 = volume */
#define CL_EX 3	/* column 3 = expression */
#define CL_PR 4	/* column 4 = program */
#define CL_PA 5	/* column 5 = panning */
#define CL_PI 6	/* column 6 = pitch bend */
#define CL_IN 7	/* column 7 = instrument name */

#define CL_BA  6	/* column 6 = bank */
#define CL_RE  7	/* column 7 = reverb */
#define CL_CH  8	/* column 8 = chorus */

#define INST_NAME_SIZE 16
static char *inst_name[MAX_XAW_MIDI_CHANNELS];
static int disp_inst_name_len = 13;
#define UNTITLED_STR "<No Title>"

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
static void drawProg(int,int,int,int,Boolean),drawPan(int,int,Boolean),
  draw1Chan(int,int,char),drawVol(int,int),drawExp(int,int),drawPitch(int,int),
  drawInstname(int,char *),drawBank(int,int),
  drawReverb(int,int),drawChorus(int,int),drawVoices(void),drawTitle(char *),
  quitCB(),playCB(),pauseCB(),stopCB(),prevCB(),nextCB(),
  forwardCB(),backCB(),repeatCB(),randomCB(),menuCB(),sndspecCB(),
  volsetCB(),volupdownCB(),tunesetCB(),tuneslideCB(),filemenuCB(),
  toggleMark(),popupLoad(),popdownLoad(),volupdownAction(),tuneslideAction(),
  tunesetAction(),a_readconfig(),a_saveconfig(),filemenuAction(),
  setDirAction(),setDirList(),
  drawBar(),redrawTrace(Boolean),completeDir(),ctl_channel_note(),
  drawKeyboardAll(),draw1Note(),redrawAction(),redrawCaption(),
  exchgWidth(),toggletrace(),
  checkRightAndPopupSubmenu(),popdownSubmenuCB(),popdownSubmenu(),leaveSubmenu(),
  addOneFile();
static char *expandDir(),*strmatch();
static int configcmp();
static void pauseAction(),soundkeyAction(),speedAction(),voiceAction();

extern void a_pipe_write(char *);
extern int a_pipe_read(char *,int);
extern int a_pipe_nread(char *buf, int n);
static void initStatus(void);

static Widget title_mb,title_sm,time_l,popup_load,popup_load_f,load_d;
static Widget load_vport,load_flist,cwd_l,load_info, lyric_t;
static Dimension lyric_height, base_height, text_height;
static GC gc,gcs,gct;
static Pixel bgcolor,menubcolor,textcolor,textbgcolor,text2bgcolor,buttonbgcolor,
  buttoncolor,togglecolor,tracecolor,volcolor,expcolor,pancolor,capcolor,rimcolor,
  boxcolor,suscolor,playcolor,revcolor,chocolor;
static Pixel black,white;
static long barcol[MAX_XAW_MIDI_CHANNELS];

static Widget toplevel,m_box,base_f,file_mb,file_sm,bsb,
  quit_b,play_b,pause_b,stop_b,prev_b,next_b,fwd_b,back_b,
  random_b,repeat_b,b_box,v_box,t_box,vol_l0,vol_l,vol_bar,tune_l0,tune_l,tune_bar,
  trace_vport,trace,pbox,pbsb;
static Widget *psmenu = NULL;
static char local_buf[300];
static char window_title[300], *w_title;
int amplitude = DEFAULT_AMPLIFICATION;
String bitmapdir = DEFAULT_PATH "/bitmaps";
Boolean arrangetitle = TRUE;
static int voices = 0, last_voice = 0, voices_num_width;
static int maxentry_on_a_menu = 0,submenu_n = 0;

typedef struct {
  Boolean		confirmexit;
  Boolean		repeat;
  Boolean		autostart;
  Boolean		autoexit;
  Boolean		hidetext;
  Boolean		shuffle;
  Boolean		disptrace;
} Config;

#define FLAG_NOTE_OFF   1
#define FLAG_NOTE_ON    2
#define FLAG_BANK       1
#define FLAG_PROG       2
#define FLAG_PROG_ON    4
#define FLAG_PAN        8
#define FLAG_SUST       16
#define FLAG_BENDT		32
typedef struct {
  int reset_panel;
  int multi_part;
  char v_flags[MAX_XAW_MIDI_CHANNELS];
  int16 cnote[MAX_XAW_MIDI_CHANNELS];
  int16 cvel[MAX_XAW_MIDI_CHANNELS];
  int16 ctotal[MAX_XAW_MIDI_CHANNELS];
  int16 bank[MAX_XAW_MIDI_CHANNELS];
  int16 reverb[MAX_XAW_MIDI_CHANNELS];  
  char c_flags[MAX_XAW_MIDI_CHANNELS];
  Channel channel[MAX_XAW_MIDI_CHANNELS];
} PanelInfo;

/* Default configuration to execute Xaw interface */
/* (confirmexit repeat autostart hidetext shuffle disptrace) */
Config Cfg = { False, False, True, False, False, True };
static PanelInfo *Panel;

typedef struct {
  int y;
  int l;
} KeyL;

typedef struct {
  KeyL k[3];
  int xofs;
  Pixel col;
} ThreeL;
static ThreeL *keyG;
#define KEY_NUM 111

#define MAXBITMAP 10
static char *bmfname[] = {
  "back.xbm","fwrd.xbm","next.xbm","pause.xbm","play.xbm","prev.xbm",
  "quit.xbm","stop.xbm","random.xbm","repeat.xbm",(char *)NULL
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
#define BM_RANDOM		8
#define BM_REPEAT		9
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
static Pixmap check_mark, arrow_mark, layer[2];

static int bm_height[MAXBITMAP], bm_width[MAXBITMAP],
  x_hot,y_hot, root_height, root_width;
static Pixmap bm_Pixmap[MAXBITMAP];
static int max_files;
char basepath[PATH_MAX];
String *dirlist = NULL;
Dimension trace_width, trace_height, menu_width;
static int total_time = 0, curr_time;
static float thumbj;
#ifdef I18N
XFontSet ttitlefont;
XFontStruct *ttitlefont0;
char **ml;
XFontStruct **fs_list;
#else
XFontStruct *ttitlefont;
#define ttitlefont0 ttitlefont
#endif

static struct _app_resources {
  String bitmap_dir;
  Boolean arrange_title;
  Dimension text_height,trace_width,trace_height,menu_width;
  Pixel common_fgcolor,common_bgcolor,menub_bgcolor,text_bgcolor,text2_bgcolor,
	toggle_fgcolor,button_fgcolor,button_bgcolor,
	velocity_color,drumvelocity_color,volume_color,expr_color,pan_color,
	trace_bgcolor,rim_color,box_color,caption_color,sus_color,play_color,
	rev_color,cho_color;
  XFontStruct *label_font,*volume_font,*trace_font;
  String more_text,file_text;
#ifdef I18N
  XFontSet text_font, ttitle_font;
#else
  XFontStruct *text_font, *ttitle_font;
#endif
} app_resources;

static XtResource resources[] ={
#define offset(entry) XtOffset(struct _app_resources*, entry)
  {"bitmapDir", "BitmapDir", XtRString, sizeof(String),
   offset(bitmap_dir), XtRString, DEFAULT_PATH "/bitmaps"},
  {"arrangeTitle", "ArrangeTitle", XtRBoolean, sizeof(Boolean),
   offset(arrange_title), XtRImmediate, (XtPointer)False},
#ifdef WIDGET_IS_LABEL_WIDGET
  {"textLHeight", "TextLHeight", XtRShort, sizeof(short),
   offset(text_height), XtRImmediate, (XtPointer)30},
#else
  {"textHeight", "TextHeight", XtRShort, sizeof(short),
   offset(text_height), XtRImmediate, (XtPointer)120},
#endif
  {"traceWidth", "TraceWidth", XtRShort, sizeof(short),
   offset(trace_width), XtRImmediate, (XtPointer)TRACE_WIDTH},
  {"traceHeight", "TraceHeight", XtRShort, sizeof(short),
   offset(trace_height), XtRImmediate, (XtPointer)TRACE_HEIGHT},
  {"menuWidth", "MenuWidth", XtRShort, sizeof(int),
   offset(menu_width), XtRImmediate, (XtPointer)200},
  {"foreground", XtCForeground, XtRPixel, sizeof(Pixel),
   offset(common_fgcolor), XtRString, "black"},
  {"background", XtCBackground, XtRPixel, sizeof(Pixel),
   offset(common_bgcolor), XtRString, "gray65"},
  {"menubutton", "MenuButtonBackground", XtRPixel, sizeof(Pixel),
   offset(menub_bgcolor), XtRString, "#CCFF33"},
  {"textbackground", "TextBackground", XtRPixel, sizeof(Pixel),
   offset(text_bgcolor), XtRString, "gray85"},
  {"text2background", "Text2Background", XtRPixel, sizeof(Pixel),
   offset(text2_bgcolor), XtRString, "gray80"},
  {"toggleforeground", "ToggleForeground", XtRPixel, sizeof(Pixel),
   offset(toggle_fgcolor), XtRString, "MediumBlue"},
  {"buttonforeground", "ButtonForeground", XtRPixel, sizeof(Pixel),
   offset(button_fgcolor), XtRString, "blue"},
  {"buttonbackground", "ButtonBackground", XtRPixel, sizeof(Pixel),
   offset(button_bgcolor), XtRString, "gray76"},
  {"velforeground", "VelForeground", XtRPixel, sizeof(Pixel),
   offset(velocity_color), XtRString, "orange"},
  {"veldrumforeground", "VelDrumForeground", XtRPixel, sizeof(Pixel),
   offset(drumvelocity_color), XtRString, "red"},
  {"volforeground", "VolForeground", XtRPixel, sizeof(Pixel),
   offset(volume_color), XtRString, "LightPink"},
  {"expforeground", "ExpForeground", XtRPixel, sizeof(Pixel),
   offset(expr_color), XtRString, "aquamarine"},
  {"panforeground", "PanForeground", XtRPixel, sizeof(Pixel),
   offset(pan_color), XtRString, "blue"},
  {"tracebackground", "TraceBackground", XtRPixel, sizeof(Pixel),
   offset(trace_bgcolor), XtRString, "gray90"},
  {"rimcolor", "RimColor", XtRPixel, sizeof(Pixel),
   offset(rim_color), XtRString, "gray20"},
  {"boxcolor", "BoxColor", XtRPixel, sizeof(Pixel),
   offset(box_color), XtRString, "gray76"},
  {"captioncolor", "CaptionColor", XtRPixel, sizeof(Pixel),
   offset(caption_color), XtRString, "DarkSlateGrey"},
  {"sustainedkeycolor", "SustainedKeyColor", XtRPixel, sizeof(Pixel),
   offset(sus_color), XtRString, "red4"},
  {"playingkeycolor", "PlayingKeyColor", XtRPixel, sizeof(Pixel),
   offset(play_color), XtRString, "maroon1"},
  {"reverbcolor", "ReverbColor", XtRPixel, sizeof(Pixel),
   offset(rev_color), XtRString, "PaleGoldenrod"},
  {"choruscolor", "ChorusColor", XtRPixel, sizeof(Pixel),
   offset(cho_color), XtRString, "yellow"},
  {"labelfont", "LabelFont", XtRFontStruct, sizeof(XFontStruct *),
   offset(label_font), XtRString, "-adobe-helvetica-bold-r-*-*-14-*-75-75-*-*-*-*"},
  {"volumefont", "VolumeFont", XtRFontStruct, sizeof(XFontStruct *),
   offset(volume_font), XtRString, "-adobe-helvetica-bold-r-*-*-12-*-75-75-*-*-*-*"},
#ifdef I18N
  {"textfontset", "TextFontSet", XtRFontSet, sizeof(XFontSet),
   offset(text_font), XtRString, "-*-*-medium-r-normal--14-*-*-*-*-*-*-*"},
  {"ttitlefont", "Ttitlefont", XtRFontSet, sizeof(XFontSet),
   offset(ttitle_font), XtRString, "-misc-fixed-medium-r-normal--14-*-*-*-*-*-*-*"},
#else
  {"textfont", XtCFont, XtRFontStruct, sizeof(XFontStruct *),
   offset(text_font), XtRString, "-*-fixed-medium-r-normal--14-*-*-*-*-*-*-*"},
  {"ttitlefont", "Ttitlefont", XtRFontStruct, sizeof(XFontStruct *),
   offset(ttitle_font), XtRString, "-adobe-helvetica-bold-o-*-*-14-*-75-75-*-*-*-*"},
#endif
  {"tracefont", "TraceFont", XtRFontStruct, sizeof(XFontStruct *),
   offset(trace_font), XtRString, "7x14"},
  {"labelfile", "LabelFile", XtRString, sizeof(String),
   offset(file_text), XtRString, "file..."},
#undef offset
};

enum {
    ID_LOAD = 100,
    ID_SAVECONFIG,
    ID_HIDETXT,
    ID_HIDETRACE,
    ID_SHUFFLE,
    ID_REPEAT,
    ID_AUTOSTART,
    ID_AUTOQUIT,
    ID_DUMMY,
    ID_QUIT,
};
#define IDS_SAVECONFIG  "101"
#define IDS_HIDETXT     "102"
#define IDS_HIDETRACE   "103"
#define IDS_SHUFFLE     "104"
#define IDS_REPEAT      "105"
#define IDS_AUTOSTART   "106"
#define IDS_AUTOQUIT    "107"


static ButtonRec file_menu[] = {
  {ID_LOAD, "load", True, False},
  {ID_SAVECONFIG, "saveconfig", True, False},
  {ID_HIDETXT, "(Un)Hide Messages (Ctrl-M)", True, False},
  {ID_HIDETRACE, "(Un)Hide Trace (Ctrl-T)", True, False},
  {ID_SHUFFLE, "Shuffle (Ctrl-S)", True, False},
  {ID_REPEAT, "Repeat (Ctrl-R)", True, False},
  {ID_AUTOSTART, "Auto Start", True, False},
  {ID_AUTOQUIT, "Auto Exit", True, False},
  {ID_DUMMY, "line", False, False},
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
	a_pipe_write("T 0\n");
  }
}

static Boolean IsTracePlaying(void) {
  Boolean s;

  if (!ctl->trace_playing) return False;
  XtVaGetValues(play_b,XtNstate,&s,NULL);
  return(s);
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
#endif
}

/*ARGSUSED*/
static void playCB(Widget w,XtPointer data,XtPointer dummy) {
  onPlayOffPause();
  XtVaGetValues(title_mb,XtNlabel,&local_buf+2,NULL);
  a_pipe_write("P");
}

/*ARGSUSED*/
static void pauseAction(Widget w,XEvent *e,String *v,Cardinal *n) {
  Boolean s;

  XtVaGetValues(play_b,XtNstate,&s,NULL);
  if (e->type == KeyPress && s == True) {
	XtVaGetValues(pause_b,XtNstate,&s,NULL);
	s ^= True;
	XtVaSetValues(pause_b,XtNstate,&s,NULL);
	a_pipe_write("U");
  }
}

static void soundkeyAction(Widget w,XEvent *e,String *v,Cardinal *n) {
  a_pipe_write(*(int *)n == 0 ? "+\n":"-\n");
}

static void speedAction(Widget w,XEvent *e,String *v,Cardinal *n) {
  a_pipe_write(*(int *)n == 0 ? ">\n":"<\n");
}

static void voiceAction(Widget w,XEvent *e,String *v,Cardinal *n) {
  a_pipe_write(*(int *)n == 0 ? "O\n":"o\n");
}

static void randomAction(Widget w,XEvent *e,String *v,Cardinal *n) {
  Boolean s;
  XtVaGetValues(random_b,XtNstate,&s,NULL);
  s ^= True;
  if(!s) XtVaSetValues(random_b,XtNstate,s,NULL);
	randomCB(NULL,&s,NULL);
}

static void repeatAction(Widget w,XEvent *e,String *v,Cardinal *n) {
  Boolean s;
  XtVaGetValues(repeat_b,XtNstate,&s,NULL);
  s ^= True;
  if(!s) XtVaSetValues(repeat_b,XtNstate,s,NULL);
  repeatCB(NULL,&s,NULL);
}

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
  initStatus();
  redrawTrace(False);
}

/*ARGSUSED*/
static void nextCB(Widget w,XtPointer data,XtPointer dummy) {
  onPlayOffPause();
  a_pipe_write("N");
  initStatus();
  redrawTrace(True);
}

/*ARGSUSED*/
static void prevCB(Widget w,XtPointer data,XtPointer dummy) {
  onPlayOffPause();
  a_pipe_write("B");
  initStatus();
  redrawTrace(True);
}

/*ARGSUSED*/
static void forwardCB(Widget w,XtPointer data,XtPointer dummy) {
  if (onPlayOffPause()) a_pipe_write("P");
  a_pipe_write("f");
  initStatus();
}

/*ARGSUSED*/
static void backCB(Widget w,XtPointer data,XtPointer dummy) {
  if (onPlayOffPause()) a_pipe_write("P");
  a_pipe_write("b");
  initStatus();
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
  toggleMark(file_menu[ID_REPEAT-100].widget, file_menu[ID_REPEAT-100].id);
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
  toggleMark(file_menu[ID_SHUFFLE-100].widget, file_menu[ID_SHUFFLE-100].id);
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
  sprintf(s, "%d", tmpval);
  XtVaSetValues(vol_l, XtNlabel, s, NULL);
  sprintf(s, "V %03d\n", tmpval);
  a_pipe_write(s);
}

static void volupdownCB(Widget w,XtPointer data,XtPointer diff) {
  char s[8];
  sprintf(s, "v %03d\n", (((int)diff > 0)? (-10):10));
  a_pipe_write(s);
}

static void volupdownAction(Widget w,XEvent *e,String *v,Cardinal *n) {
  char s[8];

  sprintf(s, "v %03d\n", atoi(*v));
  a_pipe_write(s);
}

static void tunesetCB(Widget w,XtPointer data,XtPointer call_data)
{
  static int tmpval;
  char s[16];
  float percent = *(float*)call_data;
  int value = (float)(total_time * percent);
  float thumb, l_thumb;

  if (tmpval == value) return;
  if (curr_time > total_time) curr_time = total_time;
  curr_time = tmpval = value;
  l_thumb = thumb = (float)curr_time / (float)total_time;
  snprintf(s,sizeof(s), "%2d:%02d", tmpval / 60, tmpval % 60);
  XtVaSetValues(tune_l0, XtNlabel, s, NULL);
  sprintf(s, "T %d\n", tmpval);
  a_pipe_write(s);
}

static void tunesetAction(Widget w,XEvent *e,String *v,Cardinal *n) {
  static float tmpval;
  char s[16];
  int value;
  float thumb, l_thumb;

  XtVaGetValues(tune_bar, XtNtopOfThumb, &l_thumb, NULL);
  if (tmpval == l_thumb) return;
  tmpval = l_thumb;
  value = (int)(l_thumb * total_time);
  snprintf(s,sizeof(s), "%2d:%02d", curr_time / 60, curr_time % 60);
  XtVaSetValues(tune_l0, XtNlabel, s, NULL);
  XtVaSetValues(tune_bar, XtNtopOfThumb, &l_thumb, NULL);
  sprintf(s, "T %d\n", value);
  a_pipe_write(s);
}

static void tuneslideCB(Widget w,XtPointer data,XtPointer diff) {
  char s[16];

  sprintf(s, "T %d\n", curr_time+ (int)diff);
  a_pipe_write(s);
}

static void tuneslideAction(Widget w,XEvent *e,String *v,Cardinal *n) {
  char s[16];
  float l_thumb;

  XtVaGetValues(tune_bar, XtNtopOfThumb, &l_thumb, NULL);
  sprintf(s, "T %d\n", (int)(total_time * l_thumb));
  a_pipe_write(s);
}

/*ARGSUSED*/
static void resizeAction(Widget w,XEvent *e,String *v,Cardinal *n) {
  Dimension w1,w2,h1,h2;
  Position y1;
  int i,tmp,tmp2,tmp3;

  XtVaGetValues(toplevel,XtNwidth,&w1,NULL);
  XtVaGetValues(toplevel,XtNheight,&h1,NULL);
  w2 = w1 -8;
  if(w2>TRACEVPORT_WIDTH) w2 = TRACEVPORT_WIDTH;
  XtVaGetValues(lyric_t,XtNheight,&h2,NULL);
  XtResizeWidget(lyric_t,w2-2,h2,0);
  i= 0; tmp = 10;
  while (tmp > 0) {
	i++; tmp -= (int)(w2) / 36;
  }
  XtVaSetValues(lyric_t,XtNborderWidth,1,NULL);
  XtVaSetValues(b_box,XtNheight,i*40,NULL);
  XtResizeWidget(b_box,w2,i*40,0);

  if(ctl->trace_playing) {
	XtVaGetValues(trace_vport,XtNy,&y1,NULL);
	XtResizeWidget(trace_vport,w2,((h1-y1>TRACE_HEIGHT+12)? TRACE_HEIGHT+12:h1-y1),0);
  }
  XtVaGetValues(v_box,XtNheight,&h2,NULL);
  w2 = ((w1 < TRACE_WIDTH_SHORT)? w1:TRACE_WIDTH_SHORT);	/* new v_box width */
  tmp = XTextWidth(app_resources.volume_font,"Volume ",7)+8; /* vol_l width */
  XtVaSetValues(vol_l0,XtNwidth,tmp,NULL);
  XtVaSetValues(v_box,XtNwidth,w2,NULL);
  tmp2 = w2 -tmp - XTextWidth(app_resources.volume_font,"000",3) -38;
  tmp3 = w2 -XTextWidth(app_resources.volume_font,"/ 99:59",7)
	- XTextWidth(app_resources.volume_font,"000",3) -45;
  XtResizeWidget(v_box,w2,h2,0);
  XtVaGetValues(vol_bar,XtNheight,&h2,NULL);
  XtVaSetValues(vol_bar,XtNwidth,tmp2,NULL);
  XtVaSetValues(tune_bar,XtNwidth,tmp3,NULL);
  XtResizeWidget(vol_bar,tmp2,h2,0);
  XtResizeWidget(tune_bar,tmp3,h2,0);
  XSync(disp, False);
  usleep(10000);
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
#endif /* !WIDGET_IS_LABEL_WIDGET */

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
		snprintf(local_buf,sizeof(local_buf),"X %s\n",p);
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

static void filemenuAction(Widget w,XEvent *e,String *v,Cardinal *n) {
  int i;

  if(e == NULL)
	i= ID_HIDETXT;
  else
	i= atoi(*v);
  if(!(ctl->trace_playing) && i == ID_HIDETRACE) i= ID_HIDETXT;
  filemenuCB(file_menu[i-100].widget,&file_menu[i-100].id,NULL);
}

static void filemenuCB(Widget w,XtPointer id_data, XtPointer data) {
  int *id = (int *)id_data;
  Dimension w1,h1,w2,h2,tmp;

  switch (*id) {
	case ID_LOAD:
	  popupLoad(w,NULL,NULL);
	  break;
	case ID_AUTOSTART:
	  toggleMark(w,*id);
	  break;
	case ID_AUTOQUIT:
	  toggleMark(w,*id);
	  a_pipe_write("q");
	  break;
	case ID_HIDETRACE:
	  if(ctl->trace_playing) {
		XtVaGetValues(toplevel,XtNheight,&h1,NULL);
		XtVaGetValues(toplevel,XtNwidth,&w1,NULL);
		if(XtIsManaged(trace_vport)) {
		  tmp = trace_height + (XtIsManaged(lyric_t) ? 0:lyric_height);
		  XtUnmanageChild(trace_vport);
		  XtMakeResizeRequest(toplevel,w1,base_height-tmp,&w2,&h2);
		} else {
		  XtManageChild(trace_vport);
		  XtVaSetValues(trace_vport,XtNfromVert,
						(XtIsManaged(lyric_t) ? lyric_t:t_box),NULL);
		  XtMakeResizeRequest(toplevel,w1,h1+trace_height,&w2,&h2);
		  XtVaSetValues(trace_vport,XtNheight,trace_height,NULL);
		}
		toggleMark(w,*id);
	  }
	  break;
	case ID_HIDETXT:
	  XtVaGetValues(toplevel,XtNheight,&h1,NULL);
	  XtVaGetValues(toplevel,XtNwidth,&w1,NULL);
	  if(XtIsManaged(lyric_t)) {
		if(ctl->trace_playing) {
		  tmp = lyric_height + (XtIsManaged(trace_vport) ? 0:trace_height);
		} else {
		  tmp = lyric_height;
		}
		XtUnmanageChild(lyric_t);
		if(ctl->trace_playing && XtIsManaged(trace_vport))
		  XtVaSetValues(trace_vport,XtNfromVert,t_box,NULL);
		XtMakeResizeRequest(toplevel,w1,base_height-tmp,&w2,&h2);
	  } else {
		XtManageChild(lyric_t);
		if(ctl->trace_playing && XtIsManaged(trace_vport)) {
		  XtVaSetValues(trace_vport,XtNfromVert,lyric_t,NULL);
		}
		XtVaSetValues(lyric_t,XtNheight,lyric_height,NULL);
		XtMakeResizeRequest(toplevel,w1,h1+lyric_height,&w2,&h2);
	  }
	  toggleMark(w,*id);
	  break;
	case ID_SAVECONFIG:
	  a_saveconfig(dotfile);
	  break;
	case ID_SHUFFLE:
	  randomAction(NULL,NULL,NULL,NULL);
	  break;
	case ID_REPEAT:
	  repeatAction(NULL,NULL,NULL,NULL);
	  break;
	case ID_QUIT:
      quitCB(w, NULL, NULL);
      break;    
  }
}

#ifdef WIDGET_IS_LABEL_WIDGET
static void a_print_msg(Widget w)
{
    int i, msglen;
    a_pipe_nread((char *)&msglen, sizeof(int));
    while(msglen > 0)
    {
	i = msglen;
	if(i > sizeof(local_buf)-1)
	    i = sizeof(local_buf)-1;
	a_pipe_nread(local_buf, i);
	local_buf[i] = '\0';
	XtVaSetValues(w,XtNlabel,local_buf,NULL);
	msglen -= i;
    }
}
#else
static void a_print_msg(Widget w)
{
    int i, msglen;
    XawTextPosition pos;
    XawTextBlock tb;

    tb.firstPos = 0;
    tb.ptr = local_buf;
    tb.format = FMT8BIT;
    pos = XawTextGetInsertionPoint(w);

    a_pipe_nread((char *)&msglen, sizeof(int));
    while(msglen > 0)
    {
        i = msglen;
        if(i > sizeof(local_buf))
            i = sizeof(local_buf);
        a_pipe_nread(local_buf, i);
        tb.length = i;
        XawTextReplace(w, pos, pos, &tb);
        pos += i;
        XawTextSetInsertionPoint(w, pos);
        msglen -= i;
    }
}
#endif /* WIDGET_IS_LABEL_WIDGET */

#define DELTA_VEL       32

static void ctl_channel_note(int ch, int note, int velocity) {
  
  if(!ctl->trace_playing) return;
  if (velocity == 0) {
	if (note == Panel->cnote[ch])	  
	  Panel->v_flags[ch] = FLAG_NOTE_OFF;
	Panel->cvel[ch] = 0;
  } else if (velocity > Panel->cvel[ch]) {
	Panel->cvel[ch] = velocity;
	Panel->cnote[ch] = note;
	Panel->ctotal[ch] = velocity * Panel->channel[ch].volume *
	  Panel->channel[ch].expression / (127*127);
	Panel->v_flags[ch] = FLAG_NOTE_ON;
  }
}

/*ARGSUSED*/
static void handle_input(XtPointer data,int *source,XtInputId *id) {
  char s[16], c;
  int i=0, n, ch, note;
  float thumb;

  a_pipe_read(local_buf,sizeof(local_buf));
  switch (local_buf[0]) {
  case 't' :
	curr_time = n = atoi(local_buf+2); i= n % 60; n /= 60;
	sprintf(s, "%d:%02d", n,i);
	XtVaSetValues(tune_l0, XtNlabel, s, NULL);
	if (total_time >0) {
	  thumbj = (float)curr_time / (float)total_time;
	  if (sizeof(thumbj) > sizeof(XtArgVal)) {
		XtVaSetValues(tune_bar,XtNtopOfThumb,&thumbj,NULL);
	  } else {
		XtArgVal *l_thumbj = (XtArgVal *) &thumbj;
		XtVaSetValues(tune_bar,XtNtopOfThumb,*l_thumbj,NULL);
	  }
	}
	break;
  case 'T' :
	n= atoi(local_buf+2);
	if(n > 0) {
	  total_time = n;
	  snprintf(s,sizeof(s), "/%2d:%02d", n/60, n%60);
	  XtVaSetValues(tune_l,XtNlabel,s,NULL);
	}
	break;
  case 'E' :
    {
	  char *name;
	  name=strrchr(local_buf+2,' ');
	  if(name==NULL)
		break;
	  name++;
	  XtVaSetValues(title_mb,XtNlabel,name,NULL);
	  snprintf(window_title, sizeof(window_title), "%s : %s", APP_CLASS, local_buf+2);
	  XtVaSetValues(toplevel,XtNtitle,window_title,NULL);
	  *window_title = '\0';
    }
	break;
  case 'e' :
	if (arrangetitle) {
	  char *p= local_buf+2;
	  if (!strcmp(p, "(null)")) p = UNTITLED_STR;
	  snprintf(window_title, sizeof(window_title), "%s : %s", APP_CLASS, p);
	  XtVaSetValues(toplevel,XtNtitle,window_title,NULL);
	}
	snprintf(window_title, sizeof(window_title), "%s", local_buf+2);
	break;
  case 'O' : offPlayButton();break;
  case 'L' :
	a_print_msg(lyric_t);
	break;
  case 'Q' : exit(0);
  case 'V':
	i=atoi(local_buf+2);
	thumb = (float)i / (float)MAXVOLUME;
	sprintf(s, "%d", i);
	XtVaSetValues(vol_l, XtNlabel, s, NULL);
	if (sizeof(thumb) > sizeof(XtArgVal)) {
	  XtVaSetValues(vol_bar, XtNtopOfThumb, &thumb, NULL);
	} else {
	  XtArgVal *l_thumb = (XtArgVal *) &thumb;
	  XtVaSetValues(vol_bar, XtNtopOfThumb,*l_thumb, NULL);
	}
	break;
  case 'v':
	c= *(local_buf+1);
	n= atoi(local_buf+2);
	if(c == 'L')
	  voices = n;
	else
	  last_voice = n;
	if(IsTracePlaying()) drawVoices();
	break;
  case 'g':
  case '\0' :
	break;
  case 'X':
	n=max_files;
	max_files+=atoi(local_buf+2);
	for (i=n;i<max_files;i++) {
	  a_pipe_read(local_buf,sizeof(local_buf));
	  addOneFile(max_files,i,local_buf);
	}
	break;
  case 'Y':
	if(ctl->trace_playing) {
	  ch= *(local_buf+1) - 'A';
	  c= *(local_buf+2);
	  note= (*(local_buf+3)-'0')*100 + (*(local_buf+4)-'0')*10 + *(local_buf+5)-'0';
	  n= atoi(local_buf+6);
	  if (c == '*' || c == '&') {
		Panel->c_flags[ch] |= FLAG_PROG_ON;
	  } else {
		Panel->c_flags[ch] &= ~FLAG_PROG_ON; n= 0;
	  }
	  ctl_channel_note(ch, note, n);
	  draw1Note(ch,note,c);
	  draw1Chan(ch,Panel->ctotal[ch],c);
	}
	break;
  case 'I':
	if(IsTracePlaying()) {
	  ch= *(local_buf+1) - 'A';
	  strncpy(inst_name[ch], (char *)&local_buf[2], INST_NAME_SIZE);
	  drawInstname(ch, inst_name[ch]);
	}
	break;
  case 'P':
	if(IsTracePlaying()) {
	  c= *(local_buf+1);
	  ch= *(local_buf+2)-'A';
	  n= atoi(local_buf+3);
	  switch(c) {
	  case  'A':		/* panning */
		Panel->channel[ch].panning = n;
		Panel->c_flags[ch] |= FLAG_PAN;
		drawPan(ch,n,True);
		break;
	  case  'B':		/* pitch_bend */
		Panel->channel[ch].pitchbend = n;
		Panel->c_flags[ch] |= FLAG_BENDT;
		if (!plane) drawPitch(ch,n);
		break;
	  case  'b':		/* tonebank */
		Panel->channel[ch].bank = n;
		if (plane) drawBank(ch,n);
		break;
	  case  'r':		/* reverb */
		Panel->reverb[ch] = n;
		if (plane) drawReverb(ch,n);
		break;
	  case  'c':		/* chorus */
		Panel->channel[ch].chorus_level = n;
		if (plane) drawChorus(ch,n);
		break;
	  case  'S':		/* sustain */
		Panel->channel[ch].sustain = n;
		Panel->c_flags[ch] |= FLAG_SUST;
		break;
	  case  'P':		/* program */
		Panel->channel[ch].program = n;
		Panel->c_flags[ch] |= FLAG_PROG;
		drawProg(ch,n,4,pl[plane].ofs[CL_PR],True);
		break;
	  case  'E':		/* expression */
		Panel->channel[ch].expression = n;
		ctl_channel_note(ch, Panel->cnote[ch], Panel->cvel[ch]);
		drawExp(ch,n);
		break;
	  case  'V':		/* volume */
		Panel->channel[ch].volume = n;
		ctl_channel_note(ch, Panel->cnote[ch], Panel->cvel[ch]);
		drawVol(ch,n);
		break;
	  }
	}
	break;
  case 'R':
	redrawTrace(True);	break;
  case 'U':			/* update timer */
	if(ctl->trace_playing) {
	  for(i=0; i<MAX_XAW_MIDI_CHANNELS; i++)
		if (Panel->v_flags[i]) {
		  if (Panel->v_flags[i] == FLAG_NOTE_OFF) {
			Panel->ctotal[i] -= DELTA_VEL;
			if (Panel->ctotal[i] <= 0) {
			  Panel->ctotal[i] = 0;
			  Panel->v_flags[i] = 0;
			}
		  } else {
			Panel->v_flags[i] = 0;
		  }
		}
	}
	break;
  case 'm':
	n= atoi(local_buf+1);
	switch(n) {
	case GM_SYSTEM_MODE:
	  sprintf(s,"%d:%02d / GM",total_time/60,total_time%60); break;
	case GS_SYSTEM_MODE:
	  sprintf(s,"%d:%02d / GS",total_time/60,total_time%60); break;
	case XG_SYSTEM_MODE:
	  sprintf(s,"%d:%02d / XG",total_time/60,total_time%60); break;
	default:
	  sprintf(s,"%d:%02d",total_time/60,total_time%60); break;
	}
	XtVaSetValues(time_l,XtNlabel,s,NULL);
	break;
  default : 
	fprintf(stderr,"Unkown message '%s' from CONTROL" NLS,local_buf);
  }
}


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
	snprintf(newfull,sizeof(newfull),"%s/%s", basepath, path);
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
	  snprintf(tmp, sizeof(tmp), "%s/%s", pw->pw_dir, p);
	} else {	/* *p != '~' */
	  snprintf(tmp, sizeof(tmp), "%s/%s", basepath, path);
	}
  }
  p = tmp;
  tail = strrchr(p, '/'); *tail++ = '\0';
  full->dirname = p;
  full->basename = tail;
  snprintf(newfull,sizeof(newfull),"%s/%s", p, tail);
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

/*
 * sort algorithm for DirList:
 * - directories before files
 */
static int dirlist_cmp (const void *p1, const void *p2)
{
    int i1, i2;
    char *s1, *s2;

    s1 = *((char **)p1);
    s2 = *((char **)p2);
    i1 = strlen (s1) - 1;
    i2 = strlen (s2) - 1;
    if (i1 >= 0 && i2 >= 0) {
	if (s1 [i1] == '/' && s2 [i2] != '/')
	    return -1;
	if (s1 [i1] != '/' && s2 [i2] == '/')
	    return  1;
    }
    return strcmp (s1, s2);
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
        qsort (dirlist, i, sizeof (char *), dirlist_cmp);
	dirlist[i] = NULL;
	snprintf(local_buf, sizeof(local_buf), "%d Directories, %d Files", d_num, f_num);
	XawListChange(list, dirlist, 0,0,True);
	XtVaSetValues(label,XtNlabel,currdir,NULL);
	XtVaSetValues(load_info,XtNlabel,local_buf,NULL);
	strcpy(basepath, currdir);
  }
}

static void drawBar(int ch,int len, int xofs, int column, Pixel color) {
  XSetForeground(disp, gct, bgcolor);
  XSetForeground(disp, gct, boxcolor);
  XFillRectangle(XtDisplay(trace),XtWindow(trace),gct,
				 xofs+len+2,TRACEV_OFS+BAR_SPACE*ch+2,
				 pl[plane].w[column] -len -4,BAR_HEIGHT);
  XSetForeground(disp, gct, color);
  XFillRectangle(XtDisplay(trace),XtWindow(trace),gct,
				 xofs+2,TRACEV_OFS+BAR_SPACE*ch+2,
				 len,BAR_HEIGHT);
}

static void drawProg(int ch,int val,int column,int xofs, Boolean do_clean) {
  char s[4];

  if(do_clean) {
	XSetForeground(disp, gct, boxcolor);
	XFillRectangle(XtDisplay(trace),XtWindow(trace),gct,
				   xofs+2,TRACEV_OFS+BAR_SPACE*ch+2,
				   pl[plane].w[4]-4,BAR_HEIGHT);
  }
  XSetForeground(disp, gct, black);
  sprintf(s, "%3d", val);
  XDrawString(XtDisplay(trace), XtWindow(trace), gct,
			  xofs+5,TRACEV_OFS+BAR_SPACE*ch+16,s,3);
}

static void drawPan(int ch,int val,Boolean setcolor) {
  int ap,bp;
  int x;
  static XPoint pp[3];

  if (val < 0) return;
  if (setcolor) {
	XSetForeground(disp, gct, boxcolor);
	XFillRectangle(XtDisplay(trace),XtWindow(trace),gct,
				   pl[plane].ofs[CL_PA]+2,TRACEV_OFS+BAR_SPACE*ch+2,
				   pl[plane].w[CL_PA]-4,BAR_HEIGHT);
	XSetForeground(disp, gct, pancolor);
  }
  x= pl[plane].ofs[CL_PA]+3;
  ap= 31 * val/127;
  bp= 31 -ap -1;
  pp[0].x= ap+ x; pp[0].y= 12 +BAR_SPACE*(ch+1);
  pp[1].x= bp+ x; pp[1].y= 8 +BAR_SPACE*(ch+1);
  pp[2].x= bp+ x; pp[2].y= 16 +BAR_SPACE*(ch+1);
  XFillPolygon(XtDisplay(trace),XtWindow(trace),gct,pp,3,
			   (int)Nonconvex,(int)CoordModeOrigin);
}

static void draw1Chan(int ch,int val,char cmd) {
  if (cmd == '*' || cmd == '&') {
	drawBar(ch, (int)(val*BARSCALE2), pl[plane].ofs[CL_VE], CL_VE, barcol[ch]);
  } else {
	XSetForeground(disp, gct, boxcolor);
	XFillRectangle(XtDisplay(trace),XtWindow(trace),gct,
				   pl[plane].ofs[CL_VE]+2,TRACEV_OFS+BAR_SPACE*ch+2,
				   pl[plane].w[CL_VE]-4,BAR_HEIGHT);
  }
}

static void drawVol(int ch,int val) {
  drawBar(ch, (int)(val*BARSCALE3), pl[plane].ofs[CL_VO], CL_VO, volcolor);
}

static void drawExp(int ch,int val) {
  drawBar(ch, (int)(val*BARSCALE4), pl[plane].ofs[CL_EX], CL_EX, expcolor);
}

static void drawReverb(int ch,int val) {
  drawBar(ch, (int)(val*BARSCALE5), pl[plane].ofs[CL_RE], CL_RE, revcolor);
}

static void drawChorus(int ch,int val) {
  drawBar(ch, (int)(val*BARSCALE5), pl[plane].ofs[CL_CH], CL_CH, chocolor);
}

static void drawPitch(int ch,int val) {
  char s[3];

  XSetForeground(disp, gct, boxcolor);
  XFillRectangle(XtDisplay(trace),XtWindow(trace),gct,
				 pl[plane].ofs[CL_PI]+2,TRACEV_OFS+BAR_SPACE*ch+2,
				 pl[plane].w[CL_PI] -4,BAR_HEIGHT);
  XSetForeground(disp, gct, barcol[9]);
  if (val != 0) {
	if (val<0) {
	  sprintf(s, "=");
	} else {
	  if (val == 0x2000) sprintf(s, "*");
	  else if (val>0x3000) sprintf(s, ">>");
	  else if (val>0x2000) sprintf(s, ">");
	  else if (val>0x1000) sprintf(s, "<");
	  else sprintf(s, "<<");
	}
	XDrawString(XtDisplay(trace), XtWindow(trace), gct,
				pl[plane].ofs[CL_PI]+4,TRACEV_OFS+BAR_SPACE*ch+16,s,strlen(s));
  }
}

static void drawInstname(int ch, char *name) {
  int len;
  if(!ctl->trace_playing) return;
  if(plane!=0) return;

  XSetForeground(disp, gct, boxcolor);
  XFillRectangle(XtDisplay(trace),XtWindow(trace),gct,
				 pl[plane].ofs[CL_IN]+2,TRACEV_OFS+BAR_SPACE*ch+2,
				 pl[plane].w[CL_IN] -4,BAR_HEIGHT);
  XSetForeground(disp, gct, ((ISDRUMCHANNEL(ch))? capcolor:black));
  len = strlen(name);
  XDrawString(XtDisplay(trace), XtWindow(trace), gct,
			  pl[plane].ofs[CL_IN]+4,TRACEV_OFS+BAR_SPACE*ch+15,
			  name,(len>disp_inst_name_len)? disp_inst_name_len:len);
}

static void draw1Note(int ch,int note,int flag) {
  int i, j;
  XSegment dot[3];

  j = note -9;
  if (j<0) return;
  if (flag == '*') {
	XSetForeground(disp, gct, playcolor);
  } else if (flag == '&') {
	XSetForeground(disp, gct,
	               ((keyG[j].col == black)? suscolor:barcol[0]));
  }  else {
	XSetForeground(disp, gct, keyG[j].col);
  }
  for(i= 0; i<3; i++) {
	dot[i].x1 = keyG[j].xofs +i;
	dot[i].y1 = TRACEV_OFS+ keyG[j].k[i].y+ ch*BAR_SPACE;
	dot[i].x2 = dot[i].x1;
	dot[i].y2 = dot[i].y1 + keyG[j].k[i].l;
  }
  XDrawSegments(XtDisplay(trace),XtWindow(trace),gct,dot,3);
}

static void drawKeyboardAll(Display *disp,Drawable pix) {
  int i, j;
  XSegment dot[3];

  XSetForeground(disp, gc, tracecolor);
  XFillRectangle(disp,pix,gc,0,0,BARH_OFS8,BAR_SPACE);
  XSetForeground(disp, gc, boxcolor);
  XFillRectangle(disp,pix,gc,BARH_OFS8,0,TRACE_WIDTH-BARH_OFS8+1,BAR_SPACE);
  for(i= 0; i<KEY_NUM; i++) {
	XSetForeground(disp, gc, keyG[i].col);
	for(j= 0; j<3; j++) {
	  dot[j].x1 = keyG[i].xofs +j;
	  dot[j].y1 = keyG[i].k[j].y;
	  dot[j].x2 = dot[j].x1;
	  dot[j].y2 = dot[j].y1 + keyG[i].k[j].l;
	}
	XDrawSegments(disp,pix,gc,dot,3);
  }
}

static void drawBank(int ch,int val) {
  char s[4];

  XSetForeground(disp, gct, black);
  sprintf(s, "%3d", (int)val);
  XDrawString(disp,XtWindow(trace),gct,
              pl[plane].ofs[CL_BA],TRACEV_OFS+BAR_SPACE*ch+15, s,strlen(s));
}

#define VOICENUM_WIDTH 56
static void drawVoices(void) {
  XSetForeground(disp, gct, tracecolor);
  XFillRectangle(disp,XtWindow(trace),gct,voices_num_width +4,
                 MAX_XAW_MIDI_CHANNELS*BAR_SPACE+TRACEV_OFS+1,VOICENUM_WIDTH,TRACE_FOOT);  
  sprintf(local_buf, "%3d/%d", last_voice, voices);
  XSetForeground(disp, gct, capcolor);  
  XDrawString(disp, XtWindow(trace),gct,voices_num_width+6,
              MAX_XAW_MIDI_CHANNELS*BAR_SPACE+TRACEV_OFS+16,local_buf,strlen(local_buf));
}

static void drawTitle(char *str) {
  char *p = str;

  if(ctl->trace_playing) {
	if (!strcmp(p, "(null)")) p = UNTITLED_STR;
	XSetForeground(disp, gcs, capcolor);
#ifdef I18N
	XmbDrawString(XtDisplay(trace), XtWindow(trace),ttitlefont,gcs,
	              VOICES_NUM_OFS+TTITLE_OFS,
	              MAX_XAW_MIDI_CHANNELS*BAR_SPACE+TRACEV_OFS+ ttitlefont0->ascent +3,
	              p,strlen(p));
#else
	XDrawString(XtDisplay(trace), XtWindow(trace),gcs,
	            VOICES_NUM_OFS+TTITLE_OFS,
	            MAX_XAW_MIDI_CHANNELS*BAR_SPACE+TRACEV_OFS+ ttitlefont->ascent +3,
	            p,strlen(p));
#endif
	}
}

static void toggletrace(Widget w,XEvent *e,String *v,Cardinal *n) {
  if((e->xbutton.button == 1) || e->type == KeyPress) {
	plane ^= 1;
	redrawTrace(True);
  }
}

/*ARGSUSED*/
static void exchgWidth(Widget w,XEvent *e,String *v,Cardinal *n) {
  Dimension w1,h1,w2,h2;

  XtVaGetValues(toplevel,XtNheight,&h1,NULL);
  ++currwidth;
  currwidth %= 3;		/* number of rotatewidth */
  w1 = rotatewidth[currwidth];
  XtMakeResizeRequest(toplevel,w1,h1,&w2,&h2);
  resizeAction(w,NULL,NULL,NULL);  
}

/*ARGSUSED*/
static void redrawAction(Widget w,XEvent *e,String *v,Cardinal *n) {
  redrawTrace(True);
}

/*ARGSUSED*/
static Boolean cursor_is_in = False;
static void redrawCaption(Widget w,XEvent *e,String *v,Cardinal *n) {
  char *p;
  int i;

  if(e->type == EnterNotify) {
	cursor_is_in = True;
	XSetForeground(disp, gct, capcolor);
  } else {
	cursor_is_in = False;
	XSetForeground(disp, gct, tracecolor);
  }
  XFillRectangle(disp,XtWindow(trace),gct, 0,0,TRACE_WIDTH,TRACEV_OFS);
  XSetBackground(disp, gct, (e->type == EnterNotify)? expcolor:tracecolor);
  XSetForeground(disp, gct, (e->type == EnterNotify)? tracecolor:capcolor);
  for(i=0; i<pl[plane].col; i++) {
	p = pl[plane].cap[i];
	XDrawString(disp,XtWindow(trace),gct,pl[plane].ofs[i]+4,16,p,strlen(p));
  }
}

static void redrawTrace(Boolean draw) {
  int i;
  Dimension w1, h1;
  char s[3];

  if(!ctl->trace_playing) return;
  if(!XtIsRealized(trace)) return;

  XtVaGetValues(trace,XtNheight,&h1,NULL);
  XtVaGetValues(trace,XtNwidth,&w1,NULL);
  XSetForeground(disp, gct, tracecolor);
  XFillRectangle(disp,XtWindow(trace),gct, 0,0,w1,h1);
  XSetForeground(disp, gct, boxcolor);
  XFillRectangle(disp,XtWindow(trace),gct,
				 BARH_OFS8 -1,TRACEV_OFS, TRACE_WIDTH-BARH_OFS8+1,
				 BAR_SPACE*MAX_XAW_MIDI_CHANNELS);
  for(i= 0; i<MAX_XAW_MIDI_CHANNELS; i++) {
	XCopyArea(disp, layer[plane], XtWindow(trace), gct, 0,0,
			  TRACE_WIDTH,BAR_SPACE, 0, TRACEV_OFS+i*BAR_SPACE);
  }
  XSetForeground(disp, gct, capcolor);
  XDrawLine(disp,XtWindow(trace),gct,BARH_OFS0,TRACEV_OFS+BAR_SPACE*MAX_XAW_MIDI_CHANNELS,
			TRACE_WIDTH-1,TRACEV_OFS+BAR_SPACE*MAX_XAW_MIDI_CHANNELS);

  XSetForeground(disp, gct, black);
  for(i= 1; i<MAX_XAW_MIDI_CHANNELS+1; i++) {
	sprintf(s, "%2d", i);
	XDrawString(disp, XtWindow(trace), gct,
				pl[plane].ofs[CL_C]+2,TRACEV_OFS+BAR_SPACE*i-5,s,2);
  }

  if(cursor_is_in) {
	XSetForeground(disp, gct, capcolor);
	XFillRectangle(disp,XtWindow(trace),gct, 0,0,TRACE_WIDTH,TRACEV_OFS);
  }
  XSetForeground(disp, gct, (cursor_is_in)? tracecolor:capcolor);
  for(i=0; i<pl[plane].col; i++) {
	char *p;
	p = pl[plane].cap[i];
	XDrawString(disp,XtWindow(trace),gct,pl[plane].ofs[i]+4,16,p,strlen(p));
  }
  XSetForeground(disp, gct, tracecolor);
  XFillRectangle(disp,XtWindow(trace),gct,0,MAX_XAW_MIDI_CHANNELS*BAR_SPACE+TRACEV_OFS+1,
                 TRACE_WIDTH,TRACE_FOOT);
  XSetForeground(disp, gct, capcolor);  
  XDrawString(disp, XtWindow(trace),gct,VOICES_NUM_OFS,
              MAX_XAW_MIDI_CHANNELS*BAR_SPACE+TRACEV_OFS+16,"Voices",6);
  drawVoices();
  drawTitle(window_title);
  if(draw) {
	for(i=0; i<MAX_XAW_MIDI_CHANNELS; i++)
	  if (Panel->ctotal[i] != 0 && Panel->c_flags[i] & FLAG_PROG_ON)
		draw1Chan(i,Panel->ctotal[i],'*');
	XSetForeground(disp, gct, pancolor);
	for(i=0; i<MAX_XAW_MIDI_CHANNELS; i++) {
	  if (Panel->c_flags[i] & FLAG_PAN)
		drawPan(i,Panel->channel[i].panning,False);
	}
	XSetForeground(disp, gct, black);
	for(i=0; i<MAX_XAW_MIDI_CHANNELS; i++) {
	  drawProg(i,Panel->channel[i].program,CL_PR,pl[plane].ofs[4],False);
	  drawVol(i,Panel->channel[i].volume);
	  drawExp(i,Panel->channel[i].expression);
	  if (plane) {
		drawBank(i,Panel->channel[i].bank);
		drawReverb(i,Panel->reverb[i]);
		drawChorus(i,Panel->channel[i].chorus_level);
	  } else {
		drawPitch(i,Panel->channel[i].pitchbend);
		drawInstname(i, inst_name[i]);
	  }
	}
  }
}

static void initStatus(void) {
  int i;

  if(!ctl->trace_playing) return;
  for(i=0; i<MAX_XAW_MIDI_CHANNELS; i++) {
	Panel->channel[i].program= 0;
	Panel->channel[i].volume= 0;
	Panel->channel[i].sustain= 0;
	Panel->channel[i].expression= 0;
	Panel->channel[i].pitchbend= 0;
	Panel->channel[i].panning= -1;
	Panel->ctotal[i] = 0;
	Panel->cvel[i] = 0;
	Panel->bank[i] = 0;
	Panel->reverb[i] = 0;
	Panel->channel[i].chorus_level = 0;
	Panel->v_flags[i] = 0;
	Panel->c_flags[i] = 0;
	*inst_name[i] = '\0';
  }
  last_voice = 0;
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
	char *fullpath = filename;
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
	snprintf(dotfile, PATH_MAX, "%s/%s", home, INITIAL_CONFIG);
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
		case S_AutoExit:
		  Cfg->autoexit = (Boolean)k; break;
		case S_DispText:
		  Cfg->hidetext = (Boolean)(k ^ 1);
		  break;
		case S_ShufflePlay:
		  Cfg->shuffle = (Boolean)k; break;
		case S_DispTrace:
		  Cfg->disptrace = (Boolean)k; break;
		}
	  }

	  fclose(fp);
	}
  }
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
	  fprintf(fp,"set %s %d\n",cfg_items[S_DispText],(int)(file_menu[ID_HIDETXT-100].bmflag ^ TRUE));
	  fprintf(fp,"set %s %d\n",cfg_items[S_ShufflePlay],(int)s2);
	  fprintf(fp,"set %s %d\n",cfg_items[S_DispTrace],((int)file_menu[ID_HIDETRACE-100].bmflag ? 0:1));
	  fprintf(fp,"set %s %d\n",cfg_items[S_AutoExit],(int)file_menu[ID_AUTOQUIT-100].bmflag);
	  fclose(fp);
	} else {
	  fprintf(stderr, "cannot open initializing file '%s'.\n", file);
	}
  }
}

#ifdef OFFIX
static void FileDropedHandler(Widget widget ,XtPointer data,XEvent *event,Boolean *b)
{
  char *filename;
  unsigned char *Data;
  unsigned long Size;
  char local_buffer[PATH_MAX];
  int i;
  static const int AcceptType[]={DndFile,DndFiles,DndDir,DndLink,DndExe,DndURL,
				 DndNotDnd};
  int Type;
  Type=DndDataType(event);
  for(i=0;AcceptType[i]!=DndNotDnd;i++){
    if(AcceptType[i]==Type)
      goto OK;
  }
  fprintf(stderr,"NOT ACCEPT\n");
  /*Not Acceptable,so Do Nothing*/
  return;
OK:
  DndGetData(&Data,&Size);
  if(Type==DndFiles){
    filename = Data;
    while (filename[0] != '\0'){
	  snprintf(local_buffer,sizeof(local_buffer),"X %s\n",filename);
      a_pipe_write(local_buffer);
      filename = filename + strlen(filename) + 1;
    }       
  }
  else{
	snprintf(local_buffer,sizeof(local_buffer),"X %s%s\n",Data,(Type==DndDir)?"/":"");
    a_pipe_write(local_buffer);
  }
  return;
}
#endif

/*ARGSUSED*/
static void leaveSubmenu(Widget w, XEvent *e, String *v, Cardinal *n) {
  XLeaveWindowEvent *leave_event = (XLeaveWindowEvent *)e;
  Dimension height;

  XtVaGetValues(w,XtNheight,&height,NULL);
  if (leave_event->x <= 0 || leave_event->y <= 0 || leave_event->y >= height)
	XtPopdown(w);
}

/*ARGSUSED*/
static void checkRightAndPopupSubmenu(Widget w, XEvent *e, String *v, Cardinal *n) {
  XLeaveWindowEvent *leave_ev = (XLeaveWindowEvent *)e;
  Dimension nheight,height,width;
  Position y;
  int i;

  if(!maxentry_on_a_menu) return;

  if(e == NULL)
	i= *(int *)n;
  else
	i= atoi(*v);
  if(w != title_sm) {
	if(leave_ev->x <= 0 || leave_ev->y < 0) {
		XtPopdown(w); return;
	}
  } else {
	if(leave_ev->x <= 0 || leave_ev->y <= 0) return;
  }
  if(psmenu[i] == NULL) return;
  XtVaGetValues(psmenu[i],XtNheight,&height,NULL);
  
  /* neighbor menu height */
  XtVaGetValues((i>0)? psmenu[i-1]:title_sm,
                XtNwidth,&width,XtNheight,&nheight,XtNy,&y,NULL);
  if(leave_ev->x > 0 && leave_ev->y > nheight - 22) {
	XtVaSetValues(psmenu[i],XtNx,leave_ev->x_root-60,
				  XtNy,y +((height)? nheight-height:0),NULL);
	XtPopup(psmenu[i],XtGrabNone);  
  }
}

/*ARGSUSED*/
static void popdownSubmenuCB(Widget w,XtPointer data,XtPointer dummy) {
  int i = (int)data;

  if (i < 0) i = submenu_n -1;
  while(i >= 0) XtPopdown(psmenu[i--]);
}

/*ARGSUSED*/
static void popdownSubmenu(Widget w, XEvent *e, String *v, Cardinal *n) {
  int i = atoi(*v);

  while(i >= 0) XtPopdown(psmenu[i--]);
}

static void addOneFile(int max_files,int curr_num,char *fname) {
  static Dimension tmpi = 0;
  static int menu_height = 0;
  static Widget tmpw;
  static int j = 0;
  char sbuf[256];

  if (!maxentry_on_a_menu) tmpw = title_sm;
  if(menu_height + tmpi*2 > root_height) {
	if(!maxentry_on_a_menu) {
	  maxentry_on_a_menu = j = curr_num;
	  XtAddCallback(title_sm,XtNpopdownCallback,popdownSubmenuCB,(XtPointer)(submenu_n -1));
	}
	if(j >= maxentry_on_a_menu) {
	  if (psmenu == NULL)
		psmenu = (Widget *)safe_malloc(sizeof(Widget) * ((int)(max_files / curr_num)+ 2));
	  else
		psmenu = (Widget *)safe_realloc((char *)psmenu, sizeof(Widget)*(submenu_n + 2));
	  sprintf(sbuf, "morebox%d", submenu_n);
	  pbox=XtVaCreateManagedWidget(sbuf,smeBSBObjectClass,tmpw,XtNlabel,"  More...",
	                               XtNbackground,textbgcolor,XtNforeground,capcolor,
	                               XtNrightBitmap,arrow_mark,
	                               XtNfont,app_resources.label_font, NULL);
	  snprintf(sbuf,sizeof(sbuf),
	          "<LeaveWindow>: unhighlight() checkRightAndPopupSubmenu(%d)",submenu_n);
	  XtOverrideTranslations(tmpw, XtParseTranslationTable(sbuf));

	  sprintf(sbuf, "psmenu%d", submenu_n);
	  psmenu[submenu_n] = XtVaCreatePopupShell(sbuf,simpleMenuWidgetClass,title_sm,
	                               XtNforeground,textcolor, XtNbackground,textbgcolor,
	                               XtNbackingStore,NotUseful,XtNsaveUnder,False,
	                               XtNwidth,menu_width,
	                               NULL);
	  snprintf(sbuf,sizeof(sbuf), "<BtnUp>: popdownSubmenu(%d) notify() unhighlight()\n\
		<EnterWindow>: unhighlight()\n\
		<LeaveWindow>: leaveSubmenu(%d) unhighlight()",submenu_n,submenu_n);
	  XtOverrideTranslations(psmenu[submenu_n],XtParseTranslationTable(sbuf));
	  tmpw = psmenu[submenu_n++]; psmenu[submenu_n] = NULL;
	  j = 0;
	}
  }
  if(maxentry_on_a_menu) j++;
  bsb=XtVaCreateManagedWidget(fname,smeBSBObjectClass,tmpw,NULL);
  XtAddCallback(bsb,XtNcallback,menuCB,(XtPointer)curr_num);
  XtVaGetValues(bsb, XtNheight, &tmpi, NULL);
  if(!maxentry_on_a_menu) menu_height += tmpi;
  else psmenu[submenu_n] = NULL;
}

void a_start_interface(int pipe_in) {
  static XtActionsRec actions[] ={
	{"do-quit",(XtActionProc)quitCB},
	{"fix-menu", (XtActionProc)filemenuCB},
	{"do-menu", (XtActionProc)filemenuAction},
	{"do-complete", (XtActionProc)completeDir},
	{"do-chgdir", (XtActionProc)setDirAction},
	{"draw-trace",(XtActionProc)redrawAction},
	{"do-exchange",(XtActionProc)exchgWidth},
	{"do-toggletrace",(XtActionProc)toggletrace},
	{"do-revcaption",(XtActionProc)redrawCaption},
	{"do-dialog-button",(XtActionProc)popdownLoad},
	{"do-load",(XtActionProc)popupLoad},
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
	{"do-volset",(XtActionProc)volsetCB},
	{"do-volupdown",(XtActionProc)volupdownAction},
	{"do-tuneset",(XtActionProc)tunesetAction},
	{"do-tuneslide",(XtActionProc)tuneslideAction},
	{"do-resize",(XtActionProc)resizeAction},
	{"checkRightAndPopupSubmenu",checkRightAndPopupSubmenu},
	{"leaveSubmenu",leaveSubmenu},
	{"popdownSubmenu",popdownSubmenu},
  };

  static String fallback_resources[]={
	"*Label.font: -adobe-helvetica-bold-o-*-*-14-*-75-75-*-*-*-*",
	"*Text*fontSet: -misc-fixed-medium-r-normal--14-*-*-*-*-*-*-*",
	"*Text*background: gray85",
	"*Text*scrollbar*background: gray76",
	"*Scrollbar*background: gray85",
	"*Label.foreground: black",
	"*Label.background: #CCFF33",
	"*Dialog*background: gray90",
	"*Command.background: gray85",
	"*Command.font: -adobe-helvetica-medium-o-*-*-12-*-75-75-*-*-*-*",
	"*Toggle.font: -adobe-helvetica-medium-o-*-*-12-*-75-75-*-*-*-*",
	"*MenuButton.translations:<EnterWindow>:	highlight()\\n\
		<LeaveWindow>:	reset()\\n\
		Any<BtnDown>:	reset() fix-menu() PopupMenu()",
	"*menu_box.borderWidth: 0",
	"*button_box.borderWidth: 0",
	"*button_box.horizDistance: 4",
	"*file_menubutton.menuName: file_simplemenu",
	"*file_menubutton.width: 60",
	"*file_menubutton.height: 28",
	"*file_menubutton.horizDistance: 6",
	"*file_menubutton.vertDistance: 4",
	"*SmeBSB.font: -adobe-helvetica-medium-r-*-*-12-*-75-75-*-*-*-*",
	"*title_menubutton.menuName: title_simplemenu",
	"*title_menubutton.width: 210",
	"*title_menubutton.height: 28",
	"*title_menubutton.resize: false",
	"*title_menubutton.horizDistance: 6",
	"*title_menubutton.vertDistance: 4",
	"*title_menubutton.fromHoriz: file_menubutton",
	"*time_label.width: 92",
	"*time_label.height: 26",
	"*time_label.resize: false",
	"*time_label.fromHoriz: title_menubutton",
	"*time_label.horizDistance: 1",
	"*time_label.vertDistance: 4",
	"*time_label.label: time / mode",
	"*button_box.height: 40",
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
	"*random_button.width: 32",
	"*random_button.height: 32",
	"*random_button.horizDistance: 4",
	"*random_button.vertDistance: 1",
	"*repeat_button.width: 32",
	"*repeat_button.height: 32",
	"*repeat_button.horizDistance: 1",
	"*repeat_button.vertDistance: 1",
	"*lyric_text.fromVert: tune_box",
	"*lyric_text.borderWidth: 1" ,
	"*lyric_text.vertDistance: 4",
	"*lyric_text.horizDistance: 6",
#ifndef WIDGET_IS_LABEL_WIDGET
	"*lyric_text.height: 120",
	"*lyric_text.scrollVertical: WhenNeeded",
#else
	"*lyric_text.height: 30",
	"*lyric_text.label: MessageWindow",
	"*lyric_text.resize: true",
#endif
#ifdef I18N
	"*lyric_text.international: True",
#else
	"*lyric_text.international: False",
#endif
	"*volume_box.height: 36",
	"*volume_box.background: gray76",
	"*volume_label.background: gray76",
	"*volume_label.vertDistance: 0",
	"*volume_box.vertDistance: 2",
	"*volume_box.borderWidth: 0",
	"*volume_label.borderWidth: 0",
	"*volume_label.label: 70",
	"*volume_bar.length: 330",
	"*tune_box.height: 36",
	"*tune_box.background: gray76",
	"*tune_box.borderWidth: 0",
	"*tune_label.label: ----",
	"*tune_label.background: gray76",
	"*tune_label.vertDistance: 0",
	"*tune_label.horizDistance: 0",
	"*tune_label0.horizDistance: 0",
	"*tune_box.vertDistance: 2",
	"*tune_bar.length: 330",
	"*popup_load.title: Timidity <Load File>",
	"*popup_loadform.height: 400",
	"*load_dialog.label: File Name",
	"*load_dialog.label.background: gray85",
	"*load_dialog*background: gray76",
	"*load_dialog.borderWidth: 0",
	"*load_dialog.height: 132",
	"*trace.vertDistance: 2",
	"*trace.borderWidth: 1",
	"*trace_vport.borderWidth: 1",
	"*trace_vport.background: gray76",
	"*load_dialog.label.font: -adobe-helvetica-bold-r-*-*-14-*-75-75-*-*-*-*",
	"*cwd_label.font: -adobe-helvetica-medium-r-*-*-12-*-75-75-*-*-*-*",
	"*cwd_info.font: -adobe-helvetica-medium-r-*-*-12-*-75-75-*-*-*-*",
	"*cwd_info.background: gray76",
	"*BitmapDir: " DEFAULT_PATH "/bitmaps/",
#ifdef XAW3D
	"*volume_bar.translations: #override\\n\
		~Ctrl Shift<Btn1Down>: do-volupdown(-50)\\n\
		~Ctrl Shift<Btn3Down>: do-volupdown(50)\\n\
		Ctrl ~Shift<Btn1Down>: do-volupdown(-5)\\n\
		Ctrl ~Shift<Btn3Down>: do-volupdown(5)\\n\
		<Btn1Down>: MoveThumb()\\n\
		<BtnUp>: NotifyScroll(FullLength) EndScroll()",
	"*tune_bar.translations: #override\\n\
		<Btn1Up>: do-tuneset()\\n\
		<Btn3Up>: do-tuneslide()\\n\
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
	"*tune_bar.translations: #override\\n\
		<Btn1Up>: do-tuneset()\\n\
		<Btn3Up>: do-tuneslide()\\n\
		<Btn1Down>: StartScroll(Forward) MoveThumb()\\n\
		<Btn3Down>: StartScroll(Backward) MoveThumb()\\n\
		<BtnUp>: NotifyScroll(FullLength) EndScroll()",
#endif
	"*file_simplemenu.load.label:	Load (Meta-N)",
	"*file_simplemenu.saveconfig.label:	Save Config (Meta-S)",
	"*file_simplemenu.quit.label:	Quit (Meta-Q, Q)",

	"*base_form.translations: #override\\n\
		~Ctrl Meta<Key>n:	do-load()\\n\
		~Ctrl Meta<Key>s:	do-menu(" IDS_SAVECONFIG ")\\n\
		Ctrl <Key>r:		do-menu(" IDS_REPEAT ")\\n\
		Ctrl <Key>s:		do-menu(" IDS_SHUFFLE ")\\n\
		Ctrl<Key>t:			do-menu(" IDS_HIDETRACE ")\\n\
		Ctrl<Key>m:			do-menu(" IDS_HIDETXT ")\\n\
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
		~Ctrl Shift<Key>v:	do-volupdown(10)\\n\
		~Ctrl<Key>x:		do-exchange()\\n\
		~Ctrl<Key>t:		do-toggletrace()\\n\
		<ConfigureNotify>:	do-resize()",

	"*load_dialog.value.translations: #override\\n\
		~Ctrl<Key>Return:	do-chgdir()\\n\
		~Ctrl<Key>KP_Enter:	do-chgdir()\\n\
		~Ctrl ~Meta<Key>Tab:	do-complete() end-of-line()\\n\
		Ctrl ~Shift<Key>g:	do-dialog-button(1)\\n\
		<Key>Escape:		do-dialog-button(1)",
	"*trace.translations: #override\\n\
		<Btn1Down>: do-toggletrace()\\n\
		<EnterNotify>: do-revcaption()\\n\
		<LeaveNotify>: do-revcaption()\\n\
		<Expose>: draw-trace()",
	"*title_simplemenu.translations: #override\\n\
		<EnterWindow>: unset()",
	"*time_label.translations: #override\\n\
		<Btn2Down>: do-menu(" IDS_HIDETRACE ")\\n\
		<Btn3Down>: do-exchange()",
    NULL,
  };
  XtAppContext app_con;
  char cbuf[PATH_MAX];
  Pixmap bmPixmap;
  int bmwidth, bmheight;
  int i, j, k, tmpi;
  int argc=1;
  float thumb, l_thumb, l_thumbj;
  char *argv="timidity", *moretext, *filetext;
  XFontStruct *labelfont,*volumefont,*tracefont;
#ifdef I18N
  #define XtNfontDEF XtNfontSet
  XFontSet textfont;
#else
  #define XtNfontDEF XtNfont
  XFontStruct *textfont;
#endif
  XawListReturnStruct lrs;
  unsigned long gcmask;
  XGCValues gcval;

#ifdef DEBUG_PRINT_RESOURCE
  for(i=0; fallback_resources[i] != NULL; i++) {
	fprintf(stderr, "%s\n", fallback_resources[i++]);
  }
  exit(0);
#endif
#ifdef I18N
  XtSetLanguageProc(NULL,NULL,NULL);
#endif
  toplevel=XtVaAppInitialize(&app_con,APP_CLASS,NULL,ZERO,&argc,&argv,
						 fallback_resources,NULL);
  XtGetApplicationResources(toplevel,(caddr_t)&app_resources,resources,
						  XtNumber(resources),NULL,0);
  bitmapdir = app_resources.bitmap_dir;
  arrangetitle = app_resources.arrange_title;
  text_height = (Dimension)app_resources.text_height;
  trace_width = (Dimension)app_resources.trace_width;
  trace_height = (Dimension)app_resources.trace_height;
  menu_width = (Dimension)app_resources.menu_width;
  labelfont = app_resources.label_font;
  volumefont = app_resources.volume_font;
  textfont = app_resources.text_font;
  tracefont = app_resources.trace_font;
  ttitlefont = app_resources.ttitle_font;
  a_readconfig(&Cfg);
  disp = XtDisplay(toplevel);
  screen = DefaultScreen(disp);
  root_height = DisplayHeight(disp, screen);
  root_width = DisplayWidth(disp, screen);
  check_mark = XCreateBitmapFromData(XtDisplay(toplevel),
                                     RootWindowOfScreen(XtScreen(toplevel)),
                                     (char *)check_bits,
                                     check_width, check_height);
  arrow_mark = XCreateBitmapFromData(XtDisplay(toplevel),
                                      RootWindowOfScreen(XtScreen(toplevel)),
                                      (char *)arrow_bits,
                                      (unsigned int)arrow_width,arrow_height);
  for(i= 0; i < MAXBITMAP; i++) {
	snprintf(cbuf,sizeof(cbuf),"%s/%s",bitmapdir,bmfname[i]);
	XReadBitmapFile(disp,RootWindow(disp,screen),cbuf,&bm_width[i],&bm_height[i],
	                &bm_Pixmap[i],&x_hot,&y_hot);
  }
#ifndef STDC_HEADERS
  getwd(basepath);
#else
  getcwd(basepath, sizeof(basepath));
#endif
  if (!strlen(basepath)) strcat(basepath, "/");
#ifdef OFFIX
  DndInitialize(toplevel);
  DndRegisterOtherDrop(FileDropedHandler);
  DndRegisterIconDrop(FileDropedHandler);
#endif
  XtAppAddActions(app_con, actions, XtNumber(actions));

  bgcolor = app_resources.common_bgcolor;
  menubcolor = app_resources.menub_bgcolor;
  textcolor = app_resources.common_fgcolor;
  textbgcolor = app_resources.text_bgcolor;
  text2bgcolor = app_resources.text2_bgcolor;
  buttonbgcolor = app_resources.button_bgcolor;
  buttoncolor = app_resources.button_fgcolor;
  togglecolor = app_resources.toggle_fgcolor;
  if(ctl->trace_playing) {
	volcolor = app_resources.volume_color;
	expcolor = app_resources.expr_color;
	pancolor = app_resources.pan_color;
	tracecolor = app_resources.trace_bgcolor;
  }
  if(ctl->trace_playing) {
	gcmask = GCForeground | GCBackground | GCFont;
	gcval.foreground = 1;
	gcval.background = 1;
	gcval.plane_mask = 1;
#ifdef I18N
	XFontsOfFontSet(ttitlefont,&fs_list,&ml);
	ttitlefont0 = fs_list[0];
	gcval.font = ttitlefont0->fid;
#else
	gcval.font = ttitlefont->fid;
#endif
	gcs = XCreateGC(disp, RootWindow(disp, screen), gcmask, &gcval);
  }
  base_f=XtVaCreateManagedWidget("base_form",boxWidgetClass,toplevel,
			XtNbackground,bgcolor,
			XtNwidth,rotatewidth[currwidth], NULL);
  m_box=XtVaCreateManagedWidget("menu_box",boxWidgetClass,base_f,
			XtNorientation,XtorientHorizontal,
			XtNbackground,bgcolor, NULL);
  filetext = app_resources.file_text;
  file_mb=XtVaCreateManagedWidget("file_menubutton",menuButtonWidgetClass,m_box,
			XtNforeground,textcolor, XtNbackground,menubcolor,
			XtNfont,labelfont, XtNlabel,filetext, NULL);
  file_sm=XtVaCreatePopupShell("file_simplemenu",simpleMenuWidgetClass,file_mb,
			XtNforeground,textcolor, XtNbackground,textbgcolor,
			XtNbackingStore,NotUseful, XtNsaveUnder,False, XtNwidth,menu_width,
			NULL);
  snprintf(cbuf,sizeof(cbuf),"TiMidity++ %s",timidity_version);
  title_mb=XtVaCreateManagedWidget("title_menubutton",menuButtonWidgetClass,m_box,
			XtNforeground,textcolor, XtNbackground,menubcolor,XtNlabel,cbuf,
			XtNfont,labelfont, NULL);
  title_sm=XtVaCreatePopupShell("title_simplemenu",simpleMenuWidgetClass,title_mb,
			XtNforeground,textcolor, XtNbackground,textbgcolor,
			XtNbackingStore,NotUseful, XtNsaveUnder,False, NULL);
  time_l=XtVaCreateManagedWidget("time_label",commandWidgetClass,m_box,
			XtNfont,labelfont,
			XtNbackground,menubcolor,NULL);
  b_box=XtVaCreateManagedWidget("button_box",boxWidgetClass,base_f,
			XtNorientation,XtorientHorizontal,
			XtNwidth,rotatewidth[currwidth]-10,
			XtNbackground,bgcolor,XtNfromVert,m_box, NULL);
  v_box=XtVaCreateManagedWidget("volume_box",boxWidgetClass,base_f,
			XtNorientation,XtorientHorizontal,
			XtNwidth, TRACE_WIDTH_SHORT,
			XtNfromVert,b_box,XtNbackground,bgcolor, NULL);
  t_box=XtVaCreateManagedWidget("tune_box",boxWidgetClass,base_f,
			XtNorientation,XtorientHorizontal,
			XtNwidth, TRACE_WIDTH_SHORT,
			XtNfromVert,v_box,XtNbackground,bgcolor, NULL);
  i = XTextWidth(volumefont,"Volume ",7)+8;
  vol_l0=XtVaCreateManagedWidget("volume_label0",labelWidgetClass,v_box,
			XtNwidth,i, XtNresize,False,
			XtNfont,volumefont, XtNlabel, "Volume", XtNborderWidth,0,
			XtNforeground,textcolor, XtNbackground,bgcolor, NULL);
  VOLUME_LABEL_WIDTH = i+30;
  j =	XTextWidth(volumefont,"000",3)+8;
  VOLUME_LABEL_WIDTH += j;
  vol_l=XtVaCreateManagedWidget("volume_label",labelWidgetClass,v_box,
			XtNwidth,j, XtNresize,False,XtNborderWidth,0,
			XtNfont,volumefont, XtNorientation, XtorientHorizontal,
			XtNforeground,textcolor, XtNbackground,bgcolor, NULL);
  vol_bar=XtVaCreateManagedWidget("volume_bar",scrollbarWidgetClass,v_box,
			XtNorientation, XtorientHorizontal,
			XtNwidth, TRACE_WIDTH_SHORT -VOLUME_LABEL_WIDTH,
			XtNbackground,textbgcolor,
			XtNfromVert,vol_l, XtNtopOfThumb,&l_thumb, NULL);
  i = XTextWidth(volumefont," 00:00",6);
  tune_l0=XtVaCreateManagedWidget("tune_label0",labelWidgetClass,t_box,
			XtNwidth,i, XtNresize,False, XtNlabel, " 0:00",
			XtNfont,volumefont, XtNfromVert,vol_l,XtNborderWidth,0,
			XtNforeground,textcolor, XtNbackground,bgcolor, NULL);
  j = XTextWidth(volumefont,"/ 00:00",7);
  tune_l=XtVaCreateManagedWidget("tune_label",labelWidgetClass,t_box,
			XtNwidth,j, XtNresize,False,
			XtNfont,volumefont, XtNfromVert,vol_l,XtNborderWidth,0,
			XtNforeground,textcolor, XtNbackground,bgcolor, NULL);
  tune_bar=XtVaCreateManagedWidget("tune_bar",scrollbarWidgetClass,t_box,
			XtNwidth, TRACE_WIDTH_SHORT -i -j -30,
			XtNbackground,textbgcolor,XtNorientation, XtorientHorizontal,
			XtNfromVert,tune_l, XtNtopOfThumb,&l_thumbj, NULL);
  l_thumb = thumb = (float)amplitude / (float)MAXVOLUME;
  if (sizeof(thumb) > sizeof(XtArgVal)) {
	XtVaSetValues(vol_bar,XtNtopOfThumb,&thumb,NULL);
  } else {
	XtArgVal * l_thumb = (XtArgVal *) &thumb;
	XtVaSetValues(vol_bar,XtNtopOfThumb,*l_thumb,NULL);
  }
  play_b=XtVaCreateManagedWidget("play_button",toggleWidgetClass,b_box,
			XtNbitmap,bm_Pixmap[BM_PLAY],
			XtNforeground,buttoncolor, XtNbackground,buttonbgcolor,
			NULL);
  pause_b=XtVaCreateManagedWidget("pause_button",toggleWidgetClass,b_box,
			XtNbitmap,bm_Pixmap[BM_PAUSE],
			XtNfromHoriz, play_b,
			XtNforeground,togglecolor, XtNbackground,buttonbgcolor,
			NULL);
  stop_b=XtVaCreateManagedWidget("stop_button",commandWidgetClass,b_box,
			XtNbitmap,bm_Pixmap[BM_STOP],
			XtNforeground,buttoncolor, XtNbackground,buttonbgcolor,
			XtNfromHoriz,pause_b,NULL);
  prev_b=XtVaCreateManagedWidget("prev_button",commandWidgetClass,b_box,
			XtNbitmap,bm_Pixmap[BM_PREV],
			XtNforeground,buttoncolor, XtNbackground,buttonbgcolor,
			XtNfromHoriz,stop_b,NULL);
  back_b=XtVaCreateManagedWidget("back_button",commandWidgetClass,b_box,
			XtNbitmap,bm_Pixmap[BM_BACK],
			XtNforeground,buttoncolor, XtNbackground,buttonbgcolor,
			XtNfromHoriz,prev_b,NULL);
  fwd_b=XtVaCreateManagedWidget("fwd_button",commandWidgetClass,b_box,
			XtNbitmap,bm_Pixmap[BM_FWRD],
			XtNforeground,buttoncolor, XtNbackground,buttonbgcolor,
			XtNfromHoriz,back_b,NULL);
  next_b=XtVaCreateManagedWidget("next_button",commandWidgetClass,b_box,
			XtNbitmap,bm_Pixmap[BM_NEXT],
			XtNforeground,buttoncolor, XtNbackground,buttonbgcolor,
			XtNfromHoriz,fwd_b,NULL);
  quit_b=XtVaCreateManagedWidget("quit_button",commandWidgetClass,b_box,
			XtNbitmap,bm_Pixmap[BM_QUIT],
			XtNforeground,buttoncolor, XtNbackground,buttonbgcolor,
			XtNfromHoriz,next_b,NULL);
  random_b=XtVaCreateManagedWidget("random_button",toggleWidgetClass,b_box,
			XtNbitmap,bm_Pixmap[BM_RANDOM],
			XtNfromHoriz,quit_b,
			XtNforeground,togglecolor, XtNbackground,buttonbgcolor,
			NULL);
  repeat_b=XtVaCreateManagedWidget("repeat_button",toggleWidgetClass,b_box,
			XtNbitmap,bm_Pixmap[BM_REPEAT],
			XtNfromHoriz,random_b,
			XtNforeground,togglecolor, XtNbackground,buttonbgcolor,
			NULL);
  popup_load=XtVaCreatePopupShell("popup_load",transientShellWidgetClass,toplevel,
			NULL);  
  popup_load_f= XtVaCreateManagedWidget("popup_loadform",formWidgetClass,popup_load,
			XtNbackground,textbgcolor, NULL);
  load_d=XtVaCreateManagedWidget("load_dialog",dialogWidgetClass,popup_load_f,
			XtNbackground,textbgcolor, XtNresizable,True, NULL);
  cwd_l = XtVaCreateManagedWidget("cwd_label",labelWidgetClass,popup_load_f,
			XtNlabel,basepath, XtNborderWidth,0, XtNfromVert,load_d,
			XtNbackground,text2bgcolor, XtNresizable,True, NULL);
  load_vport = XtVaCreateManagedWidget("vport",viewportWidgetClass, popup_load_f,
			XtNfromVert,cwd_l,	XtNallowHoriz,True, XtNallowVert,True,
			XtNuseBottom,True, XtNwidth,250, XtNheight,200, NULL);
  load_flist = XtVaCreateManagedWidget("files",listWidgetClass,load_vport,
			XtNverticalList,True, XtNforceColumns,False,
			XtNdefaultColumns, 3, NULL);
  load_info = XtVaCreateManagedWidget("cwd_info",labelWidgetClass,popup_load_f,
			XtNborderWidth,0, XtNwidth,250, XtNheight,32,
			XtNbackground,text2bgcolor, XtNfromVert,load_vport, NULL);
  XawDialogAddButton(load_d, "OK", popdownLoad,"Y");
  XawDialogAddButton(load_d, "Cancel", popdownLoad,NULL);
#ifndef WIDGET_IS_LABEL_WIDGET
  lyric_t=XtVaCreateManagedWidget("lyric_text",asciiTextWidgetClass,base_f,
			XtNwrap,XawtextWrapWord, XtNeditType,XawtextAppend,
			XtNwidth, rotatewidth[currwidth]-10,
#else
  lyric_t=XtVaCreateManagedWidget("lyric_text",labelWidgetClass,base_f,
			XtNresize,False,
			XtNforeground,textcolor, XtNbackground,menubcolor,
			XtNwidth,rotatewidth[currwidth]-10,
#endif
			XtNfontDEF,textfont, XtNheight,text_height,
			XtNfromVert,t_box, NULL);
  if(ctl->trace_playing) {
	trace_vport = XtVaCreateManagedWidget("trace_vport",viewportWidgetClass, base_f,
			XtNallowHoriz,True, XtNallowVert,True,
			XtNuseBottom,True, XtNfromVert,lyric_t,
#ifdef WIDGET_IS_LABEL_WIDGET
			XtNuseRight,True,
#endif
			XtNwidth,trace_width, XtNheight,trace_height+12, NULL);
	trace = XtVaCreateManagedWidget("trace",widgetClass,trace_vport,
			XtNwidth,trace_width,
			XtNheight,BAR_SPACE*MAX_XAW_MIDI_CHANNELS+TRACEV_OFS, NULL);
  }
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
  XtAddCallback(time_l,XtNcallback,(XtCallbackProc)filemenuAction,NULL);

  XtRealizeWidget(toplevel);
  dirlist =(String *)malloc(sizeof(String)* MAX_DIRECTORY_ENTRY);
  dirlist[0] = (char *)NULL;
  lrs.string = "";
  setDirList(load_flist, cwd_l, &lrs);
  XtSetKeyboardFocus(base_f, base_f);
  XtSetKeyboardFocus(lyric_t, base_f);
  if(ctl->trace_playing) 
	XtSetKeyboardFocus(trace, base_f);
  XtSetKeyboardFocus(popup_load, load_d);
  XtOverrideTranslations(toplevel,
            XtParseTranslationTable("<Message>WM_PROTOCOLS: do-quit()"));
  snprintf(cbuf,sizeof(cbuf),"%s/%s",bitmapdir,iconname);
  XReadBitmapFile(disp,RootWindow(disp,screen),cbuf,
                  &bmwidth,&bmheight,&bmPixmap,&x_hot,&y_hot);
  XtVaSetValues(toplevel,XtNiconPixmap,bmPixmap,NULL);
  strcpy(window_title,APP_CLASS);
  w_title = strncat(window_title," : ",3);
  w_title += sizeof(APP_CLASS)+ 2;
  XtVaGetValues(toplevel,XtNheight,&base_height,NULL);
  XtVaGetValues(lyric_t,XtNheight,&lyric_height,NULL);
  a_print_text(lyric_t,strcpy(local_buf,"<< Timidity Messages >>"));
  a_pipe_write("READY");

  if(ctl->trace_playing) {
	Panel = (PanelInfo *)safe_malloc(sizeof(PanelInfo));
	gct = XCreateGC(disp, RootWindow(disp, screen), 0, NULL);
	gc = XCreateGC(disp, RootWindow(disp, screen), 0, NULL);
	for(i=0; i<MAX_XAW_MIDI_CHANNELS; i++) {
	  if(ISDRUMCHANNEL(i))
		barcol[i]=app_resources.drumvelocity_color;
	  else
		barcol[i]=app_resources.velocity_color;
	  inst_name[i] = (char *)safe_malloc(sizeof(char) * INST_NAME_SIZE);
	}
	rimcolor = app_resources.rim_color;
	boxcolor = app_resources.box_color;
	capcolor = app_resources.caption_color;
	black = BlackPixel(disp, screen);
	white = WhitePixel(disp, screen);
	suscolor = app_resources.sus_color;
	playcolor = app_resources.play_color;
	revcolor = app_resources.rev_color;
	chocolor = app_resources.cho_color;
	XSetFont(disp, gct, tracefont->fid);

	keyG = (ThreeL *)safe_malloc(sizeof(ThreeL) * KEY_NUM);
	for(i=0, j= BARH_OFS8+1; i<KEY_NUM; i++) {
	  tmpi = i%12;
	  switch(tmpi) {
	  case 0:
	  case 5:
	  case 10:
		keyG[i].k[0].y = 11; keyG[i].k[0].l = 7;
		keyG[i].k[1].y = 2; keyG[i].k[1].l = 16;
		keyG[i].k[2].y = 11; keyG[i].k[2].l = 7;
		keyG[i].col = white;
		break;
	  case 2:
	  case 7:
		keyG[i].k[0].y = 11; keyG[i].k[0].l = 7;
		keyG[i].k[1].y = 2; keyG[i].k[1].l = 16;
		keyG[i].k[2].y = 2; keyG[i].k[2].l = 16;		
		keyG[i].col = white; break;
	  case 3:
	  case 8:
		j += 2;
		keyG[i].k[0].y = 2; keyG[i].k[0].l = 16;
		keyG[i].k[1].y = 2; keyG[i].k[1].l = 16;
		keyG[i].k[2].y = 11; keyG[i].k[2].l = 7;
		keyG[i].col = white; break;
	  default:	/* black key */
		keyG[i].k[0].y = 2; keyG[i].k[0].l = 8;
		keyG[i].k[1].y = 2; keyG[i].k[1].l = 8;
		keyG[i].k[2].y = 2; keyG[i].k[2].l = 8;
		keyG[i].col = black; break;
	  }
	  keyG[i].xofs = j; j += 2;
	}

	/* draw on template pixmaps that includes one channel row */
	for(i=0; i<2; i++) {
	  layer[i] = XCreatePixmap(disp,XtWindow(trace),TRACE_WIDTH,BAR_SPACE,
	                           DefaultDepth(disp,screen));
	  drawKeyboardAll(disp, layer[i]);
	  XSetForeground(disp, gc, capcolor);
	  XDrawLine(disp,layer[i],gc,0,0,TRACE_WIDTH,0);
	  XDrawLine(disp,layer[i],gc,0,0,0,BAR_SPACE);
	  XDrawLine(disp,layer[i],gc,TRACE_WIDTH-1,0,TRACE_WIDTH-1,BAR_SPACE);

	  for(j=0; j<pl[i].col -1; j++) {
		int w;
		tmpi= TRACEH_OFS; w= pl[i].w[j];
		for(k= 0; k<j; k++) tmpi += pl[i].w[k];
		tmpi = pl[i].ofs[j];
		XSetForeground(disp,gc, capcolor);
		XDrawLine(disp,layer[i],gc, tmpi+w,0,tmpi+w,BAR_SPACE);
		XSetForeground(disp,gc, rimcolor);
		XDrawLine(disp,layer[i],gc,tmpi+w-2,2,tmpi+w-2,BAR_HEIGHT+1);
		XDrawLine(disp,layer[i],gc,tmpi+2,BAR_HEIGHT+2,tmpi+w-2,BAR_HEIGHT+2);
		XSetForeground(disp,gc, ((j)? boxcolor:textbgcolor));
		XFillRectangle(disp,layer[i],gc,tmpi+2,2,w-4,BAR_HEIGHT);
	  }
	}
	initStatus();
	XFreeGC(disp,gc);
	voices_num_width = XTextWidth(tracefont,"Voices",6) +VOICES_NUM_OFS;
  }
  while (1) {
	a_pipe_read(local_buf,sizeof(local_buf));
	if (local_buf[0] < 'A') break;
	a_print_text(lyric_t,local_buf+2);
  }
  bsb=XtVaCreateManagedWidget("dummyfile",smeLineObjectClass,title_sm,NULL);
  max_files=atoi(local_buf);
  for (i=0;i<max_files;i++) {
    a_pipe_read(local_buf,sizeof(local_buf));
	addOneFile(max_files,i,local_buf);
  }
  for (i = 0; i < XtNumber(file_menu); i++) {
	bsb=XtVaCreateManagedWidget(file_menu[i].name,
			(file_menu[i].trap ? smeBSBObjectClass:smeLineObjectClass),
			file_sm,XtNleftBitmap, None,XtNleftMargin,24,NULL);
	XtAddCallback(bsb,XtNcallback,filemenuCB,(XtPointer)&file_menu[i].id);
	file_menu[i].widget = bsb;
  }
  if(!ctl->trace_playing) {
	Dimension w2,h2,h;
	XtVaSetValues(file_menu[ID_HIDETRACE-100].widget,XtNsensitive,False,NULL);
	XtVaSetValues(lyric_t,XtNwidth,TRACE_WIDTH_SHORT -12,NULL);
	XtVaGetValues(toplevel,XtNheight,&h,NULL);
	XtMakeResizeRequest(toplevel,TRACE_WIDTH_SHORT,h,&w2,&h2);
  }
  /* Please sleep here to make widgets arrange by Form Widget,
   * otherwise the widget geometry is broken.
   * Strange!!
   */
  if(Cfg.hidetext || !Cfg.disptrace) {
	XSync(disp, False);
	usleep(10000);
  }
  if(Cfg.autostart)
    filemenuCB(file_menu[ID_AUTOSTART-100].widget,
	           &file_menu[ID_AUTOSTART-100].id,NULL);
  if(Cfg.autoexit) {
    filemenuCB(file_menu[ID_AUTOQUIT-100].widget,
	           &file_menu[ID_AUTOQUIT-100].id,NULL);
  }
  if(Cfg.hidetext)
    filemenuCB(file_menu[ID_HIDETXT-100].widget,
		&file_menu[ID_HIDETXT-100].id,NULL);
  if(!Cfg.disptrace)
    filemenuCB(file_menu[ID_HIDETRACE-100].widget,
	           &file_menu[ID_HIDETRACE-100].id,NULL);

  if(Cfg.repeat) repeatCB(NULL,&Cfg.repeat,NULL);
  if(Cfg.shuffle) randomCB(NULL,&Cfg.shuffle,NULL);
  if(Cfg.autostart)
	playCB(NULL,NULL,NULL);
  else
	stopCB(NULL,NULL,NULL);
  if(ctl->trace_playing) initStatus();
  XtAppMainLoop(app_con);
}
