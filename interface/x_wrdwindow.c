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

    x_wrdwindow.c - MIMPI WRD for X Window written by Takanori Watanabe.
*/

/*
 * PC98 Screen Emulator
 * By Takanori Watanabe.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "timidity.h"
#include "common.h"
#include "x_mag.h"
#include "x_wrdwindow.h"
#include "VTparse.h"
#include "wrd.h"
#include "controls.h"

#define SIZEX WRD_GSCR_WIDTH
#define SIZEY WRD_GSCR_HEIGHT
#define MBCS 1
#define DEFAULT -1
#define JISX0201 "-*-fixed-*-r-normal--16-*-*-*-*-*-jisx0201.1976-*"
#define JISX0208 "-*-fixed-*-r-normal--16-*-*-*-*-*-jisx0208.1983-*"
#define TXTCOLORS 8
#define LINES WRD_TSCR_HEIGHT
#define COLS WRD_TSCR_WIDTH
#define TAB_SET 8
#define CSIZEX ((SIZEX)/(COLS))
#define CSIZEY ((SIZEY)/(LINES))
#define min(a,b) ((a) <= (b) ? (a) : (b))
#define max(a,b) ((a) >= (b) ? (a) : (b))
typedef struct{
  long attr;/*if attr<0 this attr is invalid*/
#define CATTR_LPART (1)
#define CATTR_16FONT (1<<1)
#define CATTR_COLORED (1<<2)
#define CATTR_BGCOLORED (1<<3) 
#define CATTR_TXTCOL_MASK_SHIFT 4
#define CATTR_TXTCOL_MASK (7<<CATTR_TXTCOL_MASK_SHIFT)
#define CATTR_INVAL (1<<31)
  char c;
}Linbuf;
/*VTParse Table externals*/
extern int groundtable[];
extern int csitable[];
extern int dectable[];
extern int eigtable[];
extern int esctable[];
extern int iestable[];
extern int igntable[];
extern int scrtable[];
extern int scstable[];
extern int mbcstable[];
extern int smbcstable[];

static struct MyWin{
  Display *d;
  Window w;
  XFontStruct *f8;/*8bit Font*/
  XFontStruct *f16;/*16bit Font*/
  
  GC gc,	/* For any screens */
     gcgr,	/* For Graphic screens */
     gcbmp;	/* For bitmap */
#define NUMVSCREEN 2
  Pixmap screens[NUMVSCREEN];/*Screen buffers*/
  Pixmap gscreen,appearscreen; 
  Pixmap palscreen; /*pallet screen for truecolor*/
  Pixmap workbmp;
  Pixmap offscr;
  Linbuf **scrnbuf;
  int curline;
  int curcol;
  long curattr;
#define NUMPLANE 4
#define NUMPXL 16
#define MAXPAL 20
  XColor txtcolor[TXTCOLORS],curcoltab[NUMPXL],
  gcolor[MAXPAL][NUMPXL],gcolorbase[NUMPXL];
  unsigned long basepix[1],pmask[NUMPLANE];
  int ton;
  int gmode;
#define TON_NOSHOW 0
  Colormap cmap;
#define COLOR_BG 0
#define COLOR_DEFAULT 7
  int redrawflag;
}mywin;

static struct VGVRAM{
  Pixmap *vpix;
  int num;
}vgvram;

const static char *TXTCOLORNAME[8]={
    WRD_TEXT_COLOR0,
    WRD_TEXT_COLOR1,
    WRD_TEXT_COLOR2,
    WRD_TEXT_COLOR3,
    WRD_TEXT_COLOR4,
    WRD_TEXT_COLOR5,
    WRD_TEXT_COLOR6,
    WRD_TEXT_COLOR7
};
const static int colval12[16]={
  0x000,0xf00,0x0f0,0xff0,0x00f,0xf0f,0x0ff,0xfff,
  0x444,0xc44,0x4c4,0xcc4,0x44c,0xc4c,0x4cc,0xccc
};

static char *image_buffer; /* Used for @MAG or @PLOAD */
static unsigned long gscreen_plane_mask;

static int Parse(int);
static void Redraw(int,int,int,int,int);
static void RedrawInject(int x,int y,int width,int height,int flag);

/* for truecolor */
static Bool truecolor;
static unsigned long pallets[MAXPAL][NUMPXL];
#define rgb2pixel(xcolorptr) XAllocColor(mywin.d,mywin.cmap,xcolorptr)

  /* ppixel : pseudo pixel value (each bit presents each pallet)*/
/*#define pal2ppixel(palnum) ((unsigned long) (1L << (palnum)))*/
static unsigned long pal2ppixel(int palnum) {
  return(unsigned long) (1L << (palnum));
}
#if 0
static int ppixel2pal(unsigned long ppixel){
  int i=0;
  if (!ppixel) return 0;
  i++;
  while(ppixel!=1){
    i++;
    ppixel >> 1; // statement with no effect??
  }
  return i;
}
static int pixel2pal(int pal,unsigned long pixel)
{
  int i;
  for(i=0; i<NUMPXL; i++)
    if(pallets[pal][i]==pixel)
      return i;
  return -1;
}
static unsigned long pal2pixel(int pal,int i)
{
  return pallets[pal][i];
}
static void gscreen2palscreen(pallet)
{
  XImage *simg, *timg;
  int x, y;
  
  simg = XGetImage(mywin.d, mywin.gscreen,
		   0, 0, SIZEX, SIZEY, gscreen_plane_mask, ZPixmap);
  timg = XGetImage(mywin.d, mywin.palscreen,
		   0, 0, SIZEX, SIZEY, gscreen_plane_mask, ZPixmap);
  if(!simg) return;
  for(y = 0; y < SIZEY; y++){
    for(x = 0; x < SIZEX; x++){
      int p;
      p = pixel2pal(pallet, XGetPixel(simg, x, y));
      XPutPixel(timg, x, y, p);
    }
  }
  XPutImage(mywin.d, mywin.gscreen, mywin.gcgr, timg,
	    0, 0, 0, 0, SIZEX, SIZEY);
  XFree(simg);
  XFree(timg);
}
#endif
static void palscreen2gscreen(pallet)
{
  int i;
  Pixmap colorpix;

  colorpix = XCreatePixmap(mywin.d, mywin.palscreen, SIZEX, SIZEY,
			   DefaultDepth(mywin.d, DefaultScreen(mywin.d)));

  XSetFunction(mywin.d,mywin.gcgr,GXclear);
  XFillRectangle(mywin.d, mywin.gscreen, mywin.gcgr, 0, 0, SIZEX, SIZEY);
  XSetFunction(mywin.d, mywin.gcgr, GXand);
  for(i=0; i<NUMPXL; i++){
    /* clear color base */
    XSetFunction(mywin.d, mywin.gcgr, GXcopy);
    XSetForeground(mywin.d, mywin.gcgr, pal2ppixel(i));
    XFillRectangle(mywin.d, colorpix, mywin.gcgr, 0, 0, SIZEX, SIZEY);
    /* mask with pallet i */
    XSetFunction(mywin.d, mywin.gcgr, GXand);
    XCopyArea(mywin.d, mywin.palscreen, colorpix,
	      mywin.gcgr, 0, 0, SIZEX, SIZEY, 0, 0);
    XSetForeground(mywin.d, mywin.gcgr, pallets[pallet][i]);
    XFillRectangle(mywin.d, colorpix, mywin.gcgr, 0, 0, SIZEX, SIZEY);
    XCopyArea(mywin.d, colorpix, mywin.gscreen,
	      mywin.gcgr, 0, 0, SIZEX, SIZEY, 0, 0);
  }
  XFreePixmap(mywin.d, colorpix);
}

/*12bit color value to color member of XColor structure */
static void col12toXColor(int val,XColor *set)
{
  set->red=((val>>8)&0xf)*0x1111;
  set->green=((val>>4)&0xf)*0x1111;
  set->blue=(0xf&val)*0x1111;
}

static int InitColor(Colormap cmap, Bool allocate)
{
  int i,j;
  XColor dummy;
  if(allocate) {
    for(i=0;i<TXTCOLORS;i++)
      XAllocNamedColor(mywin.d,cmap,TXTCOLORNAME[i],&mywin.txtcolor[i],&dummy);
    if (!truecolor)
      if(!XAllocColorCells(mywin.d,cmap,True,mywin.pmask,NUMPLANE,mywin.basepix,1))
	return 1;
    gscreen_plane_mask = 0;
    for(i = 0; i < NUMPLANE; i++)
      gscreen_plane_mask |= mywin.pmask[i];
    gscreen_plane_mask |= mywin.basepix[0];
  }

  for(i=0;i<NUMPXL;i++){
    int k;
    unsigned long pvalue=mywin.basepix[0];
    k=i;
    for(j=0;j<NUMPLANE;j++){
      pvalue|=(((k&1)==1)?mywin.pmask[j]:0);
      k=k>>1;
    }
    col12toXColor(colval12[i],&mywin.curcoltab[i]);
    mywin.curcoltab[i].pixel=pvalue;
    mywin.curcoltab[i].flags=DoRed|DoGreen|DoBlue;
    if(truecolor) rgb2pixel(&mywin.curcoltab[i]);
  }
  if(!truecolor) XStoreColors(mywin.d,cmap,mywin.curcoltab,NUMPXL);
  for(i=0;i<MAXPAL;i++)
    memcpy(mywin.gcolor[i],mywin.curcoltab,sizeof(mywin.curcoltab));
  if(truecolor)
    for(i=0;i<MAXPAL;i++)
      for(j=0;j<NUMPXL;j++)
	pallets[i][j]=mywin.gcolor[i][j].pixel;
  memcpy(mywin.gcolorbase,mywin.curcoltab,sizeof(mywin.curcoltab));
  return 0;
}

/*Initialize Window subsystem*/
/* return:
 * -1: error
 *  0: success
 *  1: already initialized
 */
static int InitWin(char *opt)
{
  XSizeHints *sh;
  XGCValues gcv;
  int i;
  XVisualInfo visualTmpl;
  XVisualInfo *visualList;
  int nvisuals;
  static int init_flag = 0;

  if(init_flag)
      return init_flag;

  /*Initialize Charactor buffer and attr */
  mywin.curline=0;
  mywin.curcol=0;
  mywin.ton=1;
  mywin.gmode=-1;
  mywin.curattr=0;/*Attribute Ground state*/
  mywin.scrnbuf=(Linbuf **)calloc(LINES,sizeof(Linbuf *));
  mywin.redrawflag=1;
  if((mywin.d=XOpenDisplay(NULL))==NULL){
    ctl->cmsg(CMSG_ERROR,VERB_NORMAL,"WRD: Can't Open Display");
    init_flag = -1;
    return -1;
  }

  if(strchr(opt, 'd')) {
      /* For debug */
      fprintf(stderr,"Entering -Wx Debug mode\n");
      XSynchronize(mywin.d, True);
  }

  /* check truecolor */
  visualList = XGetVisualInfo(mywin.d, 0, &visualTmpl, &nvisuals);
  truecolor = False;
  for (i=0;i<nvisuals;i++){
    if (visualList[i].visual==DefaultVisual(mywin.d,DefaultScreen(mywin.d))){
      if (visualList[i].class==TrueColor || visualList[i].class==StaticColor){
	truecolor=True;
	break;
      }
    }
  }
  XFree(visualList);

  if((mywin.f8=XLoadQueryFont(mywin.d,JISX0201))==NULL){
    ctl->cmsg(CMSG_ERROR,VERB_NORMAL,"%s: Can't load font",JISX0201);
    /* Can't load font JISX0201 */
    XCloseDisplay(mywin.d);
    mywin.d=NULL;
    init_flag = -1;
    return -1;
  }
  if((mywin.f16=XLoadQueryFont(mywin.d,JISX0208))==NULL){
    ctl->cmsg(CMSG_ERROR,VERB_NORMAL,"%s: Can't load font",JISX0208);
    XCloseDisplay(mywin.d);
    mywin.d=NULL;
    init_flag = -1;
    return -1;
  }
  mywin.w=XCreateSimpleWindow(mywin.d,DefaultRootWindow(mywin.d)
			      ,0,0,SIZEX,SIZEY
			      ,10,
			      BlackPixel(mywin.d,DefaultScreen(mywin.d)),
			      WhitePixel(mywin.d,DefaultScreen(mywin.d)));
  mywin.cmap=DefaultColormap(mywin.d,DefaultScreen(mywin.d));
  /*This block initialize Colormap*/
  if(InitColor(mywin.cmap, True)!=0){
    mywin.cmap=XCopyColormapAndFree(mywin.d,mywin.cmap);
    if(InitColor(mywin.cmap, True)!=0){
      ctl->cmsg(CMSG_ERROR,VERB_NORMAL,"WRD: Can't initialize colormap");
      XCloseDisplay(mywin.d);
      mywin.d=NULL;
      init_flag = -1;
      return -1;
    }
    XSetWindowColormap(mywin.d,mywin.w,mywin.cmap);
  }
  /*Tell Window manager not to allow resizing
   * And set Application name
   */
  sh=XAllocSizeHints();
  sh->flags=PMinSize|PMaxSize;
  sh->min_width=SIZEX;
  sh->min_height=SIZEY;
  sh->max_width=SIZEX;
  sh->max_height=SIZEY;
  XSetWMNormalHints(mywin.d,mywin.w,sh);
  XStoreName(mywin.d,mywin.w,"timidity");
  XSetIconName(mywin.d,mywin.w,"TiMidity");
  XFree(sh);

  /* Alloc background pixmap(Graphic plane)*/
  {
    int depth;
    depth=DefaultDepth(mywin.d,DefaultScreen(mywin.d));
    for(i=0;i<NUMVSCREEN;i++)
      mywin.screens[i]=XCreatePixmap(mywin.d,mywin.w,SIZEX,SIZEY,depth);
    mywin.offscr=XCreatePixmap(mywin.d,mywin.w,SIZEX,SIZEY,depth);
    mywin.workbmp=XCreatePixmap(mywin.d,mywin.w,SIZEX,CSIZEY,1);
    mywin.gscreen=mywin.screens[0];
    if(truecolor)
      mywin.palscreen=XCreatePixmap(mywin.d,mywin.w,SIZEX,SIZEY,depth);
    XSetWindowBackgroundPixmap(mywin.d,mywin.w,mywin.screens[0]);
    mywin.appearscreen=mywin.screens[0];
  }

  gcv.graphics_exposures = False;
  mywin.gc=XCreateGC(mywin.d,mywin.w,GCGraphicsExposures, &gcv);
  mywin.gcgr=XCreateGC(mywin.d,mywin.w,GCGraphicsExposures, &gcv);
  mywin.gcbmp=XCreateGC(mywin.d,mywin.workbmp,0,0);

  XSetForeground(mywin.d,mywin.gcbmp,0);
  XSetBackground(mywin.d,mywin.gcbmp,1);

  /*This Initialize background pixmap(Graphic plane)*/
  XSetForeground(mywin.d,mywin.gcgr,mywin.curcoltab[0].pixel);
  XFillRectangle(mywin.d,mywin.gscreen,mywin.gcgr,0,0,SIZEX,SIZEY);
  for(i=0;i<NUMVSCREEN;i++)
      XFillRectangle(mywin.d, mywin.screens[i], mywin.gcgr,
		     0, 0, SIZEX, SIZEY);
  XFillRectangle(mywin.d, mywin.offscr, mywin.gcgr, 0, 0, SIZEX, SIZEY);

  XSetForeground(mywin.d,mywin.gc,mywin.txtcolor[COLOR_DEFAULT].pixel);
  XSelectInput(mywin.d,mywin.w,ExposureMask|ButtonPressMask);
  image_buffer=(char *)safe_malloc(SIZEX*SIZEY*
	(DefaultDepth(mywin.d,DefaultScreen(mywin.d))+7)/8);
  init_flag = 1;
  return 0;
}

/***************************************************
 Redraw Routine 
  This redraw Text screen.
  Graphic Screen is treated as background bitmap
 ***************************************************/
static int DrawReverseString16(Display *disp,Drawable d,GC gc,int x,int y,
		  XChar2b *string,int length)
{
  int lbearing,ascent;
  lbearing=mywin.f16->min_bounds.lbearing;
  ascent=mywin.f16->max_bounds.ascent;
  XSetFont(disp,mywin.gcbmp,mywin.f16->fid);
  XDrawImageString16(disp,mywin.workbmp,mywin.gcbmp,-lbearing,ascent,
		     string,length);
  XSetClipMask(disp,gc,mywin.workbmp);
  XSetClipOrigin(disp,gc,x+lbearing,y-ascent);
  XFillRectangle(disp,d,gc,x+lbearing,y-ascent,CSIZEX*length*2,CSIZEY);
  XSetClipMask(disp,gc,None);
  return 0;
}

static int DrawReverseString(Display *disp,Drawable d,GC gc,int x,int y,
		  char *string,int length)
{
  int lbearing,ascent;
  lbearing=mywin.f16->min_bounds.lbearing;
  ascent=mywin.f16->max_bounds.ascent;
  XSetFont(disp,mywin.gcbmp,mywin.f8->fid);
  XDrawImageString(disp,mywin.workbmp,mywin.gcbmp,-lbearing,ascent,
		   string,length);
  XSetClipMask(disp,gc,mywin.workbmp);
  XSetClipOrigin(disp,gc,x+lbearing,y-ascent);
  XFillRectangle(disp,d,gc,x+lbearing,y-ascent,CSIZEX*length,CSIZEY);
  XSetClipMask(disp,gc,None);
  return 0;
}

static void Redraw(int x,int y,int width,int height,int flag)
{
  int i,yfrom,yto,xfrom,xto;
  int drawflag;

  if(!mywin.redrawflag)
    return;
  if(mywin.ton==TON_NOSHOW){
      XClearArea(mywin.d,mywin.w,x,y,width,height,False);
    return;
  }
  if(!flag){
    XCopyArea(mywin.d,mywin.offscr,mywin.w,mywin.gc,x,y,width,height,x,y);
    return;
  }
  XCopyArea(mywin.d,mywin.appearscreen,mywin.offscr,mywin.gc,
	    x,y,width,height,x,y);

  xfrom=x/CSIZEX;
  xfrom=max(xfrom,0);
  xto=(x+width-1)/CSIZEX;
  xto=(xto<COLS-1)?xto:COLS-1;
  yfrom=y/CSIZEY;
  yfrom=max(yfrom,0);
  yto=(y+height-1)/CSIZEY;
  yto=(yto<LINES-1)?yto:LINES-1;

  drawflag=0;
  for(i=yfrom;i<=yto;i++){
    if(mywin.scrnbuf[i]!=NULL){
      long prevattr,curattr;
      char line[COLS+1];
      int pos,s_x,e_x;
      int j,jfrom,jto;

      jfrom=xfrom;
      jto=xto;

      /* Check multibyte boudary */
      if(jfrom > 0 && (mywin.scrnbuf[i][jfrom-1].attr&CATTR_LPART))
	  jfrom--;
      if(jto < COLS-1 && (mywin.scrnbuf[i][jto].attr&CATTR_LPART))
	  jto++;

      pos=0;
      prevattr=CATTR_INVAL;
      s_x=e_x=jfrom*CSIZEX;
      for(j=jfrom;j<=jto+1;j++){
	if(j==jto+1 || mywin.scrnbuf[i][j].c==0) {
	  curattr=CATTR_INVAL;
	}
	else
	  curattr=mywin.scrnbuf[i][j].attr;
	if((prevattr&~CATTR_LPART)!=(curattr&~CATTR_LPART)){
	  XFontStruct *f=NULL;
	  int lbearing,ascent;
	  int (*DrawStringFunc)();
	  DrawStringFunc=XDrawString;
	  line[pos]=0;
	  if(prevattr<0){
	    DrawStringFunc=NULL;
	  }else if(prevattr&CATTR_16FONT){
	    f=mywin.f16;
	    DrawStringFunc=XDrawString16;
	    pos/=2;
	  }else
	    f=mywin.f8;
	  if(DrawStringFunc!=NULL){
	    XSetFont(mywin.d,mywin.gc,f->fid);
	    lbearing=f->min_bounds.lbearing;
	    ascent=f->max_bounds.ascent;
	    if(prevattr&CATTR_COLORED){
	      int tcol;
	      tcol=(prevattr&CATTR_TXTCOL_MASK)>>CATTR_TXTCOL_MASK_SHIFT;
	      XSetForeground(mywin.d,mywin.gc,
			     mywin.txtcolor[tcol].pixel);
  	    }else if(prevattr&CATTR_BGCOLORED){
  	      int tcol;
  	      tcol=(prevattr&CATTR_TXTCOL_MASK)>>CATTR_TXTCOL_MASK_SHIFT;
	      DrawStringFunc=(DrawStringFunc==XDrawString)?DrawReverseString:DrawReverseString16;
	      XSetForeground(mywin.d,mywin.gc,
			     mywin.txtcolor[tcol].pixel);
	    }
	    (*DrawStringFunc)(mywin.d,mywin.offscr,mywin.gc,
			      s_x-lbearing,i*CSIZEY+ascent,line,pos);
	    drawflag=1;
	    if((prevattr&CATTR_COLORED)||(prevattr&CATTR_BGCOLORED))
	      XSetForeground(mywin.d,mywin.gc,mywin.txtcolor[COLOR_DEFAULT].pixel);
	  }
	  prevattr=curattr;
	  s_x=e_x;
	  pos=0;
	}
	line[pos++]=mywin.scrnbuf[i][j].c;
	e_x+=CSIZEX;
      }
    }
  }
  if(drawflag)
    XCopyArea(mywin.d,mywin.offscr,mywin.w,mywin.gc,x,y,width,height,x,y);
  else
    XClearArea(mywin.d,mywin.w,x,y,width,height,False);
}


static void RedrawPallet(int pallet)
{
  int i,yfrom,yto,xfrom,xto;
  int drawflag;

  if(!mywin.redrawflag)
    return;
  if(mywin.ton==TON_NOSHOW){
    XClearArea(mywin.d,mywin.w,0,0,SIZEX,SIZEY,False);
    return;
  }

  /*gscreen2palscreen(pallet);*/

  xfrom=0/CSIZEX;
  xto=(0+SIZEX-1)/CSIZEX;
  xto=(xto<COLS-1)?xto:COLS-1;
  yfrom=0/CSIZEY;
  yto=(0+SIZEY-1)/CSIZEY;
  yto=(yto<LINES-1)?yto:LINES-1;

  drawflag=0;
  for(i=yfrom;i<=yto;i++){
    if(mywin.scrnbuf[i]!=NULL){
      long prevattr,curattr;
      char line[COLS+1];
      int pos,s_x,e_x;
      int j,jfrom,jto;

      jfrom=xfrom;
      jto=xto;

      /* Check multibyte boudary */
      if(jfrom > 0 && (mywin.scrnbuf[i][jfrom-1].attr&CATTR_LPART))
	  jfrom--;
      if(jto < COLS-1 && (mywin.scrnbuf[i][jto].attr&CATTR_LPART))
	  jto++;

      pos=0;
      prevattr=CATTR_INVAL;
      s_x=e_x=jfrom*CSIZEX;
      for(j=jfrom;j<=jto+1;j++){
	if(j==jto+1 || mywin.scrnbuf[i][j].c==0) {
	  curattr=CATTR_INVAL;
	}else
	  curattr=mywin.scrnbuf[i][j].attr;
	if((prevattr&~CATTR_LPART)!=(curattr&~CATTR_LPART)){
	  XFontStruct *f=NULL;
	  int lbearing,ascent;
	  int (*DrawStringFunc)();
	  DrawStringFunc=XDrawString;
	  line[pos]=0;
	  if(prevattr<0){
	    DrawStringFunc=NULL;
	  }else if(prevattr&CATTR_16FONT){
	    f=mywin.f16;
	    DrawStringFunc=XDrawString16;
	    pos/=2;
	  }else
	    f=mywin.f8;
	  if(DrawStringFunc!=NULL){
	    XSetFont(mywin.d,mywin.gc,f->fid);
	    lbearing=f->min_bounds.lbearing;
	    ascent=f->max_bounds.ascent;
	    if(prevattr&CATTR_COLORED){
	      int tcol;
	      tcol=(prevattr&CATTR_TXTCOL_MASK)>>CATTR_TXTCOL_MASK_SHIFT;
	      XSetForeground(mywin.d,mywin.gc,
			     mywin.gcolor[pallet][tcol].pixel);
	      /*pal2pixel(pallet,tcol));*/
  	    }else if(prevattr&CATTR_BGCOLORED){
  	      int tcol;
  	      tcol=(prevattr&CATTR_TXTCOL_MASK)>>CATTR_TXTCOL_MASK_SHIFT;
	      DrawStringFunc=(DrawStringFunc==XDrawString)?DrawReverseString:DrawReverseString16;
	      XSetForeground(mywin.d,mywin.gc,
			     mywin.gcolor[pallet][tcol].pixel);
	      /*pal2pixel(pallet,tcol));*/
	    }
	    (*DrawStringFunc)(mywin.d,mywin.offscr,mywin.gc,
			      s_x-lbearing,i*CSIZEY+ascent,line,pos);
	    drawflag=1;
	    if((prevattr&CATTR_COLORED)||(prevattr&CATTR_BGCOLORED))
	      XSetForeground(mywin.d,mywin.gc,
			     mywin.gcolor[pallet][COLOR_DEFAULT].pixel);
	    /*pal2pixel(pallet,COLOR_DEFAULT));*/

	  }
	  prevattr=curattr;
	  s_x=e_x;
	  pos=0;
	}
	line[pos++]=mywin.scrnbuf[i][j].c;
	e_x+=CSIZEX;
      }
    }
  }
  if(drawflag)
    XCopyArea(mywin.d,mywin.offscr,mywin.w,mywin.gc,0,0,SIZEX,SIZEY,0,0);
  else
    XClearArea(mywin.d,mywin.w,0,0,SIZEX,SIZEY,False);

  palscreen2gscreen(pallet);
  XSetWindowBackgroundPixmap(mywin.d, mywin.w, mywin.gscreen);
}


/*******************************************************
 *  Utilities for VT Parser
 ********************************************************/

static void DelChar(int line,int col)
{
  int rx1,ry1,rx2,ry2;
  rx1=(col)*CSIZEX;
  rx2=(col+1)*CSIZEX;
  ry1=(line)*CSIZEY;
  ry2=(line+1)*CSIZEY;
  if(mywin.scrnbuf[line][col].attr&CATTR_16FONT){
    if(mywin.scrnbuf[line][col].attr&CATTR_LPART){
      mywin.scrnbuf[line][col+1].c=0;
      mywin.scrnbuf[line][col+1].attr=0;
      rx2+=CSIZEX;
    }
    else{
      mywin.scrnbuf[line][col-1].c=0;
      mywin.scrnbuf[line][col-1].attr=0;
      rx1-=CSIZEX;
    }
  }
  RedrawInject(rx1,ry1,rx2-rx1,ry2-ry1,False);
  mywin.scrnbuf[line][col].c=0;
  mywin.scrnbuf[line][col].attr=0;
}

static void ClearLine(int j)
{
  free(mywin.scrnbuf[j]);
  mywin.scrnbuf[j]=NULL;
  RedrawInject(0,j*CSIZEY,SIZEX,CSIZEY,False);
}
static void ClearLeft(void)
{
  memset(mywin.scrnbuf[mywin.curline],0,sizeof(Linbuf)*mywin.curcol);
  if(!(mywin.scrnbuf[mywin.curline][mywin.curcol+1].attr
       &(CATTR_LPART))){
    mywin.scrnbuf[mywin.curline][mywin.curcol+1].attr=0;
    mywin.scrnbuf[mywin.curline][mywin.curcol+1].c=0;
  }
  RedrawInject(0,(mywin.curline)*CSIZEY,SIZEX,CSIZEY,False);
}
static void ClearRight(void)
{
  /*Erase Right*/
  memset(mywin.scrnbuf[mywin.curline]+mywin.curcol,0,
	 sizeof(Linbuf)*(COLS-mywin.curcol));
  if((mywin.scrnbuf[mywin.curline][mywin.curcol-1].attr
       &(CATTR_LPART))){
    mywin.scrnbuf[mywin.curline][mywin.curcol-1].attr=0;
    mywin.scrnbuf[mywin.curline][mywin.curcol-1].c=0;
  }
  RedrawInject(0,(mywin.curline)*CSIZEY,SIZEX,CSIZEY,False);
}
static void RedrawInject(int x,int y,int width,int height,int flag){
  static int xfrom,yfrom,xto,yto;
  int x2,y2;
  x2=x+width;
  y2=y+height;
  if(x==-1){
    xfrom=yfrom=xto=yto=-1;
    return;
  }
  if(flag==False){
    if(xfrom==-1){
      xfrom=x;
      yfrom=y;
      xto=x2;
      yto=y2;
    }
    else{
      xfrom=(xfrom<x)?xfrom:x;
      yfrom=(yfrom<y)?yfrom:y;
      xto=(xto>x2)?xto:x2;
      yto=(yto>y2)?yto:y2;
    }
  }
  else if(xfrom!=-1)
    Redraw(xfrom,yfrom,xto-xfrom,yto-yfrom,True);
}
/************************************************************
 *   Graphic Command Functions
 *
 *
 *
 **************************************************************/
void x_GMode(int mode)
{
  int i;
  unsigned long  mask=0;

  if(mode == -1)
  {
      /* Initialize plane mask */
      mywin.gmode = -1;
      XSetPlaneMask(mywin.d,mywin.gcgr,AllPlanes);
      return;
  }

  mode&=15;
  mywin.gmode = mode;
  mode = (mode&8)|wrd_plane_remap[mode&7];
  for(i=0;i<NUMPLANE;i++){
    mask|=(((mode&1)==1)?mywin.pmask[i]:0);
    mode=mode>>1;
  }
  XSetPlaneMask(mywin.d,mywin.gcgr,mask);
}
void x_GMove(int xorig,int yorig,int xend,int yend,int xdist,int ydist,
	     int srcp,int endp,int swflag)
{
  int w, h;

  w=xend-xorig+1;
  h=yend-yorig+1;
  if((srcp<2)&&(endp<2)){
    if(swflag==1){
      XSetFunction(mywin.d,mywin.gcgr,GXxor);
      XCopyArea(mywin.d,mywin.screens[endp],mywin.screens[srcp],mywin.gcgr
		,xdist,ydist,w,h,xorig,yorig);
      XCopyArea(mywin.d,mywin.screens[srcp],mywin.screens[endp],mywin.gcgr
		,xorig,yorig,w,h,xdist,ydist);
      XCopyArea(mywin.d,mywin.screens[endp],mywin.screens[srcp],mywin.gcgr
		,xdist,ydist,w,h,xorig,yorig);
      XSetFunction(mywin.d,mywin.gcgr,GXcopy);
      if(mywin.screens[srcp]==mywin.appearscreen)
	Redraw(xorig,yorig,w,h,True);
    }
    else
      XCopyArea(mywin.d,mywin.screens[srcp],mywin.screens[endp],mywin.gcgr
		,xorig,yorig,w,h,xdist,ydist);
    if(mywin.screens[endp]==mywin.appearscreen)
      Redraw(xdist,ydist,w,h,True);
  }
}
void x_VSget(int *params,int nparam)
{
  int numalloc,depth;
  depth=DefaultDepth(mywin.d,DefaultScreen(mywin.d));
  if(vgvram.vpix!=NULL)
    x_VRel();
  vgvram.vpix=safe_malloc(sizeof(Pixmap)*params[0]);
  for(numalloc=0;numalloc<params[0];numalloc++){
    vgvram.vpix[numalloc]=XCreatePixmap(mywin.d,mywin.w,SIZEX,SIZEY,depth);
  }
  vgvram.num=numalloc;
}
void x_VRel()
{
  int i;
  if(mywin.d == NULL)
      return;
  if(vgvram.vpix==NULL)
    return;
  for(i=0;i<vgvram.num;i++)
    XFreePixmap(mywin.d,vgvram.vpix[i]);
  free(vgvram.vpix);
  vgvram.vpix=NULL;
  vgvram.num=0;
}
 
void x_VCopy(int sx1,int sy1,int sx2,int sy2,int tx,int ty
	     ,int ss,int ts,int mode)
{
  int vpg,rpg,w,h;
  Pixmap srcpage,distpage,tmp;

  w=sx2-sx1+1;
  h=sy2-sy1+1;
  if(mode!=0){
    vpg=ss;
    rpg=ts;
  }else{
    vpg=ts;
    rpg=ss;
  }
  if(vpg<vgvram.num)
    srcpage=vgvram.vpix[vpg];
  else
    return;
  if(rpg<2)
    distpage=mywin.screens[rpg];
  else
    return;
  if(mode==0){
    tmp=srcpage;
    srcpage=distpage;
    distpage=tmp;
  }
  XCopyArea(mywin.d,srcpage,distpage,mywin.gc
	    ,sx1,sy1,w,h,tx,ty);
  if(distpage==mywin.appearscreen)
    Redraw(tx,ty,w,h,True);
}

void x_XCopy(int sx1,
	     int sy1,
	     int sx2,
	     int sy2,
	     int tx,
	     int ty,
	     int ss,
	     int ts,
	     int method,
	     int *opts,
	     int nopts)
{
    XImage *simg, *timg;
    int i, x, y, w, h;
    int gmode_save;

    w = sx2 - sx1 + 1;
    h = sy2 - sy1 + 1;
    gmode_save = mywin.gmode;

    if(w <= 0 || w > SIZEX ||
       h <= 0 || h > SIZEY ||
       ss < 0 || ss >= NUMVSCREEN ||
       ts < 0 || ts >= NUMVSCREEN)
	return;

    simg = timg = NULL;
    x_GMode(-1);
    switch(method)
    {
      default:
      case 0: /* copy */
	x_GMove(sx1, sy1, sx2, sy2, tx, ty, ss, ts, 0);
	break;

      case 1: /* copy except pallet No.0 */
	simg = XGetImage(mywin.d, mywin.screens[ss],
			 sx1, sy1, w, h, gscreen_plane_mask, ZPixmap);
	if(!simg) break;
	timg = XGetImage(mywin.d, mywin.screens[ts],
			 tx, ty, w, h, gscreen_plane_mask, ZPixmap);
	if(!timg) break;
	for(y = 0; y < h; y++)
	    for(x = 0; x < w; x++)
	    {
		int pixel = XGetPixel(simg, x, y);
		if(pixel != mywin.curcoltab[0].pixel)
		    XPutPixel(timg, x, y, pixel);
	    }
	XPutImage(mywin.d, mywin.screens[ts], mywin.gcgr, timg,
		  0, 0, tx, ty, w, h);
	if(mywin.screens[ts] == mywin.appearscreen)
	    Redraw(tx, ty, w, h, True);
	break;

      case 2: /* xor */
	XSetFunction(mywin.d,mywin.gcgr,GXxor);
	x_GMove(sx1, sy1, sx2, sy2, tx, ty, ss, ts, 0);
	XSetFunction(mywin.d,mywin.gcgr,GXcopy);
	break;

      case 3: /* and */
	XSetFunction(mywin.d,mywin.gcgr,GXand);
	x_GMove(sx1, sy1, sx2, sy2, tx, ty, ss, ts, 0);
	XSetFunction(mywin.d,mywin.gcgr,GXcopy);
	break;

      case 4: /* or */
	XSetFunction(mywin.d,mywin.gcgr,GXor);
	x_GMove(sx1, sy1, sx2, sy2, tx, ty, ss, ts, 0);
	XSetFunction(mywin.d,mywin.gcgr,GXcopy);
	break;

      case 5: /* reverse x */
	simg = XGetImage(mywin.d, mywin.screens[ss],
			 sx1, sy1, w, h, gscreen_plane_mask, ZPixmap);
	if(!simg)
	    break;
	for(y = 0; y < h; y++)
	{
	    for(x = 0; x < w/2; x++)
	    {
		int p1, p2;
		p1 = XGetPixel(simg, x, y);
		p2 = XGetPixel(simg, w-x-1, y);
		XPutPixel(simg, x, y, p2);
		XPutPixel(simg, w-x-1, y, p1);
	    }
	}
	XPutImage(mywin.d, mywin.screens[ts], mywin.gcgr, simg,
		  0, 0, tx, ty, w, h);
	if(mywin.screens[ts] == mywin.appearscreen)
	    Redraw(tx, ty, w, h, True);
	break;

      case 6: /* reverse y */
	simg = XGetImage(mywin.d, mywin.screens[ss],
			 sx1, sy1, w, h, gscreen_plane_mask, ZPixmap);
	if(!simg)
	    break;
	for(y = 0; y < h/2; y++)
	{
	    for(x = 0; x < w; x++)
	    {
		int p1, p2;
		p1 = XGetPixel(simg, x, y);
		p2 = XGetPixel(simg, x, h-y-1);
		XPutPixel(simg, x, y, p2);
		XPutPixel(simg, x, h-y-1, p1);
	    }
	}
	XPutImage(mywin.d, mywin.screens[ts], mywin.gcgr, simg,
		  0, 0, tx, ty, w, h);
	if(mywin.screens[ts] == mywin.appearscreen)
	    Redraw(tx, ty, w, h, True);
	break;

      case 7: /* reverse x-y */
	simg = XGetImage(mywin.d, mywin.screens[ss],
			 sx1, sy1, w, h, gscreen_plane_mask, ZPixmap);
	if(!simg)
	    break;
	for(i = 0; i < w*h/2; i++)
	{
	    int p1, p2;
	    p1 = simg->data[i];
	    p2 = simg->data[w*h-i-1];
	    simg->data[i] = p2;
	    simg->data[w*h-i-1] = p1;
	}
	XPutImage(mywin.d, mywin.screens[ts], mywin.gcgr, simg,
		  0, 0, tx, ty, w, h);
	if(mywin.screens[ts] == mywin.appearscreen)
	    Redraw(tx, ty, w, h, True);
	break;

      case 8: /* copy except pallet No.0 (type2) */
	if(nopts < 2)
	    break;
	simg = XGetImage(mywin.d, mywin.screens[ss],
			 sx1, sy1, w, h, gscreen_plane_mask, ZPixmap);
	if(!simg) break;
	timg = XGetImage(mywin.d, mywin.screens[ts],
			 opts[0], opts[1], w, h, gscreen_plane_mask, ZPixmap);
	if(!timg) break;
	for(y = 0; y < h; y++)
	    for(x = 0; x < w; x++)
	    {
		int pixel = XGetPixel(simg, x, y);
		if(pixel != mywin.curcoltab[0].pixel)
		    XPutPixel(timg, x, y, pixel);
	    }
	XPutImage(mywin.d, mywin.screens[ts], mywin.gcgr, timg,
		  0, 0, tx, ty, w, h);
	if(mywin.screens[ts] == mywin.appearscreen)
	    Redraw(tx, ty, w, h, True);
	break;

      case 9: { /* Mask copy */
	  int m, opt5, c;
	  if(nopts < 5)
	    break;
	  opt5 = opts[4];
	  simg = XGetImage(mywin.d, mywin.screens[ss],
			   sx1, sy1, w, h, gscreen_plane_mask, ZPixmap);
	  if(!simg) break;
	  timg = XGetImage(mywin.d, mywin.screens[ts],
			   tx, ty, w, h, gscreen_plane_mask, ZPixmap);
	  if(!timg) break;
	  for(y = 0; y < h; y++)
	  {
	      m = opts[y & 3] & 0xff;
	      for(x = 0; x < w; x++)
	      {
		  if((1 << (x&7)) & m)
		  {
		      if(opt5 == 16)
			  continue;
		      c = mywin.curcoltab[opt5 & 0xf].pixel;
		  }
		  else
		      c = XGetPixel(simg, x, y);
		  XPutPixel(timg, x, y, c);
	      }
	  }
	  XPutImage(mywin.d, mywin.screens[ts], mywin.gcgr, timg,
		    0, 0, tx, ty, w, h);
	  if(mywin.screens[ts] == mywin.appearscreen)
	      Redraw(tx, ty, w, h, True);
	}
	break;

      case 10: { /* line copy */
	  int cp, sk, i;
	  if(nopts < 2)
	      break;
	  if((cp = opts[0]) < 0)
	      break;
	  if((sk = opts[1]) < 0)
	      break;
	  if(cp + sk == 0)
	      break;
	  simg = XGetImage(mywin.d, mywin.screens[ss],
			   sx1, sy1, w, h, gscreen_plane_mask, ZPixmap);
	  if(!simg) break;
	  timg = XGetImage(mywin.d, mywin.screens[ts],
			   tx, ty, w, h, gscreen_plane_mask, ZPixmap);
	  if(!timg) break;
	  y = 0;
	  while(y < h)
	  {
	      for(i = 0; i < cp && y < h; i++, y++)
	      {
		  for(x = 0; x < w; x++)
		      XPutPixel(timg, x, y, XGetPixel(simg, x, y));
	      }
	      y += sk;
	  }
	}
	if(mywin.screens[ts] == mywin.appearscreen)
	    Redraw(tx, ty, w, h, True);
	break;

      case 11: {
	  int etx, ety;
	  while(tx < 0) tx += SIZEX;
	  tx %= SIZEX;
	  while(ty < 0) ty += SIZEY;
	  ty %= SIZEY;
	  etx = tx + w;
	  ety = ty + h;

	  XCopyArea(mywin.d,mywin.screens[ss],mywin.screens[ts],mywin.gcgr,
		    sx1, sx2, w, h, tx, ty);
	  if(etx > SIZEX)
	      XCopyArea(mywin.d,mywin.screens[ss],mywin.screens[ts],mywin.gcgr,
			sx1 + (etx - SIZEX),
			sy1,
			w - (etx - SIZEX),
			h,
			0, ty);
	  if(ety > SIZEY)
	      XCopyArea(mywin.d,mywin.screens[ss],mywin.screens[ts],mywin.gcgr,
			sx1,
			sy1 + (ety - SIZEY),
			w,
			h - (ety - SIZEY),
			tx, 0);
	  if(etx > SIZEX && ety > SIZEY)
	  {
	      XCopyArea(mywin.d,mywin.screens[ss],mywin.screens[ts],mywin.gcgr,
			sx1 + (etx - SIZEX),
			sy1 + (ety - SIZEY),
			w - (etx - SIZEX),
			h - (ety - SIZEY),
			0, 0);
	  }
	  if(mywin.screens[ts] == mywin.appearscreen)
	  {
	      if(etx < SIZEX && ety < SIZEY)
		  Redraw(tx, ty, w, h, True);
	      else
		  Redraw(0, 0, SIZEX, SIZEY, True);
	  }
	}
	break;

      case 12: {
	  unsigned long psm, ptm;
	  int plane_map[4] = {2, 0, 1, 3};
	  psm = mywin.pmask[plane_map[opts[0] & 3]];
	  ptm = mywin.pmask[plane_map[opts[1] & 3]];

	  simg = XGetImage(mywin.d, mywin.screens[ss],
			   sx1, sy1, w, h, gscreen_plane_mask, ZPixmap);
	  if(!simg) break;
	  timg = XGetImage(mywin.d, mywin.screens[ts],
			   tx, ty, w, h, gscreen_plane_mask, ZPixmap);
	  if(!timg) break;
	  for(y = 0; y < h; y++)
	      for(x = 0; x < w; x++)
	      {
		  int p1, p2;
		  p1 = XGetPixel(simg, x, y);
		  p2 = XGetPixel(timg, x, y);
		  if(p1 & psm)
		      p2 |= ptm;
		  else
		      p2 &= ~ptm;
		  XPutPixel(timg, x, y, p2);
	      }
	  if(mywin.screens[ts] == mywin.appearscreen)
	      Redraw(tx, ty, w, h, True);
	}
	break;
    }
    if(simg != NULL)
	XDestroyImage(simg);
    if(timg != NULL)
	XDestroyImage(timg);
    x_GMode(gmode_save);
}

static void MyDestroyImage(XImage *img)
{
    img->data = NULL; /* Don't free in XDestroyImage() */
    XDestroyImage(img);
}

void x_PLoad(char *filename){
  static XImage *image = NULL;
  if(image == NULL) {
    image=XCreateImage(mywin.d,
		       DefaultVisual(mywin.d,DefaultScreen(mywin.d)),
		       DefaultDepth(mywin.d,DefaultScreen(mywin.d)),
		       ZPixmap,0,None,SIZEX,SIZEY,8,0);
    image->data = image_buffer;
  }
  memset(image->data, 0, SIZEX * SIZEY);
  if(!pho_load_pixel(image,mywin.curcoltab,filename))
    return;
  XPutImage(mywin.d,mywin.gscreen,mywin.gc
	    ,image,0,0,0,0,SIZEX,SIZEY);
  if(mywin.gscreen==mywin.appearscreen)
    Redraw(0,0,SIZEX,SIZEY,True);
}

void x_Mag(magdata *mag,int32 x,int32 y,int32 s,int32 p)
{
  XImage *image;
  int pixsizex,pixsizey;
  if(mag==NULL){
    ctl->cmsg(CMSG_INFO,VERB_VERBOSE,"mag ERROR!\n");
    return;
  }
  x=(x==WRD_NOARG)?mag->xorig:x;
  y=(y==WRD_NOARG)?mag->yorig:y;
  p=(p==WRD_NOARG)?0:p;
  x=x+mag->xorig/8*8-mag->xorig;
  pixsizex=mag->xend-mag->xorig/8*8+1;
  pixsizey=mag->yend-mag->yorig+1;

  mag->pal[0]=17;
  x_Pal(mag->pal,16);
  if(mywin.gscreen==mywin.screens[0]){ /* Foreground screen */
    mag->pal[0]=18;
    if(!truecolor) x_Pal(mag->pal,16);
  } else {			/* Background screen */
    mag->pal[0]=19;
    if(!truecolor) x_Pal(mag->pal,16);
  }
  if((p&1)==0){
    mag->pal[0]=0;
    if(!truecolor) x_Pal(mag->pal,16);
  }
  if(p==2)
    return;
  image=XCreateImage(mywin.d,
		     DefaultVisual(mywin.d,DefaultScreen(mywin.d)),
		     DefaultDepth(mywin.d,DefaultScreen(mywin.d)),
		     ZPixmap,0,None,pixsizex,pixsizey,8,0);
  image->data=image_buffer;
  memset(image->data, 0, pixsizex*pixsizey);
  mag_load_pixel(image,mywin.curcoltab,mag);
  XPutImage(mywin.d,mywin.gscreen,mywin.gc
	    ,image,0,0,x,y,pixsizex,pixsizey);
  if(mywin.gscreen==mywin.appearscreen)
    Redraw(x,y,pixsizex,pixsizey,True);
  MyDestroyImage(image);
}
void x_Gcls(int mode)
{
  int gmode_save;
  gmode_save = mywin.gmode;

  if(mode==0)
    mode=15;
  x_GMode(mode);
  XSetFunction(mywin.d,mywin.gcgr,GXclear);
  if(truecolor)
    XFillRectangle(mywin.d,mywin.palscreen,mywin.gcgr,0,0,SIZEX,SIZEY);
  else
    XFillRectangle(mywin.d,mywin.gscreen,mywin.gcgr,0,0,SIZEX,SIZEY);
  XSetFunction(mywin.d,mywin.gcgr,GXcopy);
  x_GMode(gmode_save);
  Redraw(0,0,SIZEX,SIZEY,True);
}
void x_Ton(int param)
{
  mywin.ton=param;
  Redraw(0,0,SIZEX,SIZEY,True);
}
void x_RedrawControl(int flag)
{
  mywin.redrawflag = flag;
  if(flag)
  {
    Redraw(0,0,SIZEX,SIZEY,True);
    if(!truecolor) XStoreColors(mywin.d,mywin.cmap,mywin.curcoltab,NUMPXL);
  }
  XFlush(mywin.d);
}
void x_Gline(int *params,int nparam)
{
    int x, y, w, h; /* Update rectangle region */
    unsigned long color;
    Pixmap screen;

    x = min(params[0], params[2]);
    y = min(params[1], params[3]);
    w = max(params[0], params[2]) - x + 1;
    h = max(params[1], params[3]) - y + 1;

    if (truecolor)
      screen = mywin.palscreen;
    else
      screen = mywin.gscreen;

    switch(params[5])
    {
      default:
      case 0:
	if (truecolor)
	  color = (unsigned long) params[4];
	else
	  color = mywin.curcoltab[params[4]].pixel;
	XSetForeground(mywin.d,mywin.gcgr,color);
	XDrawLine(mywin.d,screen,mywin.gcgr,
		  params[0],params[1],params[2],params[3]);
	break;
      case 1:
	if (truecolor)
	  color = (unsigned long) params[4];
	else
	  color = mywin.curcoltab[params[4]].pixel;
	XSetForeground(mywin.d,mywin.gcgr,color);
	XDrawRectangle(mywin.d,screen,mywin.gcgr,x,y,w-1,h-1);

	break;
      case 2:
	if (truecolor)
	  color = (unsigned long) params[6];
	else
	  color = mywin.curcoltab[params[6]].pixel;
	XSetForeground(mywin.d,mywin.gcgr,color);
	XFillRectangle(mywin.d,screen,mywin.gcgr,x,y,w,h);

	break;
  }
  if(mywin.gscreen==mywin.appearscreen)
    Redraw(x,y,w,h,True);
}
void x_GCircle(int *params,int nparam)
{
  int pad=0;
  int (*Linefunc)();
  Linefunc=XDrawArc;
  if(nparam>=5){
    switch(params[4]){
    default:
    case 0:
    case 1:
      Linefunc=XDrawArc;
      if (truecolor)
	XSetForeground(mywin.d,mywin.gcgr,(unsigned long) params[3]);
      else
	XSetForeground(mywin.d,mywin.gcgr,mywin.curcoltab[params[3]].pixel);

      pad=-1;
      break;
    case 2:
      Linefunc=XFillArc;
      if (truecolor)
	XSetForeground(mywin.d,mywin.gcgr,(unsigned long) params[5]);
      else
	XSetForeground(mywin.d,mywin.gcgr,mywin.curcoltab[params[5]].pixel);
      break;
    }
  }
  if(nparam>=3){
    int xcorner,ycorner,width,height,angle;
    xcorner=params[0]-params[2];/*x_center-radius*/
    ycorner=params[1]-params[2];/*y_center-radius*/
    width=height=params[2]*2;/*radius*2*/
    angle=360*64;
    (*Linefunc)(mywin.d,mywin.gscreen,mywin.gcgr,xcorner,ycorner,
		width+pad,height+pad,
		0,angle);
    if(mywin.gscreen==mywin.appearscreen)
      Redraw(xcorner,ycorner,width,height,True);
  }
}


#define FOREGROUND_PALLET 0
void x_Pal(int *param,int nparam){
  int pallet;

  if(nparam==NUMPXL){
    pallet=param[0];
    nparam--;
    param++;
  }
  else
    pallet=FOREGROUND_PALLET;

  if(nparam==NUMPXL-1){
    int i;
    for(i=0;i<NUMPXL;i++){
      col12toXColor(param[i],&mywin.gcolor[pallet][i]);
      if(truecolor){
	rgb2pixel(&mywin.gcolor[pallet][i]);
	pallets[pallet][i]=mywin.gcolor[pallet][i].pixel;
      }
    }
    if(pallet==FOREGROUND_PALLET){
      memcpy(mywin.curcoltab,mywin.gcolor[FOREGROUND_PALLET],sizeof(mywin.curcoltab));
      if(mywin.redrawflag) {
	if(truecolor) RedrawPallet(FOREGROUND_PALLET);
	else XStoreColors(mywin.d,mywin.cmap,mywin.curcoltab,NUMPXL);
      }
    }
  }
}
void x_Palrev(int pallet)
{
  int i;
  if(pallet < 0 || pallet > MAXPAL)
    return;
  for(i = 0; i < NUMPXL; i++){
    mywin.gcolor[pallet][i].red ^= 0xffff;
    mywin.gcolor[pallet][i].green ^= 0xffff;
    mywin.gcolor[pallet][i].blue ^= 0xffff;
    if(truecolor) rgb2pixel(&mywin.gcolor[pallet][i]);
  }

  if(pallet == FOREGROUND_PALLET){
    memcpy(mywin.curcoltab,
	   mywin.gcolor[FOREGROUND_PALLET],
	   sizeof(mywin.curcoltab));
    if(mywin.redrawflag) {
	if(truecolor) RedrawPallet(pallet);
	else XStoreColors(mywin.d,mywin.cmap,mywin.curcoltab,NUMPXL);
    }
  }
}
void x_Gscreen(int active,int appear)
{
  if(active<NUMVSCREEN)
     mywin.gscreen=mywin.screens[active];
  if((appear<NUMVSCREEN)&&(mywin.appearscreen!=mywin.screens[appear])){
    XSetWindowBackgroundPixmap(mywin.d,mywin.w,mywin.screens[appear]);
    mywin.appearscreen=mywin.screens[appear];
    Redraw(0,0,SIZEX,SIZEY,True);
  }
}
void x_Fade(int *params,int nparam,int step,int maxstep)
{
  static XColor *frompal=NULL,*topal=NULL;
  if(params==NULL){
    int i;
    if(frompal==NULL||topal==NULL)
      return;
    if(step==maxstep){
      memcpy(mywin.curcoltab,topal,sizeof(mywin.curcoltab));
      memcpy(mywin.gcolor[0],mywin.curcoltab,sizeof(mywin.curcoltab));
    }
    else{
      int tmp;
      if(!mywin.redrawflag)
	return;
      for(i=0;i<NUMPXL;i++){
	tmp=(topal[i].red-frompal[i].red)/maxstep;
	mywin.curcoltab[i].red=tmp*step+frompal[i].red;
	tmp=(topal[i].green-frompal[i].green)/maxstep;
	mywin.curcoltab[i].green=tmp*step+frompal[i].green;
	tmp=(topal[i].blue-frompal[i].blue)/maxstep;
	mywin.curcoltab[i].blue=tmp*step+frompal[i].blue;
	if(truecolor) rgb2pixel(&mywin.curcoltab[i]);
      }
    }
    if(mywin.redrawflag) {
      if(truecolor) Redraw(0,0,SIZEX,SIZEY,True);
      else XStoreColors(mywin.d,mywin.cmap,mywin.curcoltab,NUMPXL);
    }
  }
  else{
    if(params[2] == 0 && params[1] < MAXPAL) {
      memcpy(mywin.curcoltab,mywin.gcolor[params[1]],sizeof(mywin.curcoltab));
      memcpy(mywin.gcolor[0],mywin.curcoltab,sizeof(mywin.curcoltab));
      if(mywin.redrawflag) {
	if(truecolor) RedrawPallet(params[1]);
	else XStoreColors(mywin.d,mywin.cmap,mywin.curcoltab,NUMPXL);
      }
    }
    else if(params[0] < MAXPAL && params[1] < MAXPAL)
    {
      frompal=mywin.gcolor[params[0]];
      topal=mywin.gcolor[params[1]];
    }
    else
      frompal=topal=NULL;

    return;
  }
}
void x_Startup(int version)
{
    int i;
    Parse(-1);
    memset(mywin.scrnbuf, 0, LINES*sizeof(Linbuf *));
    mywin.curline = 0;
    mywin.curcol = 0;
    mywin.ton = 1;
    mywin.curattr = 0;
    x_VRel();
    x_GMode(-1);
    InitColor(mywin.cmap, False);
    mywin.gscreen = mywin.appearscreen = mywin.screens[0];

    XSetForeground(mywin.d, mywin.gcgr, mywin.curcoltab[0].pixel);
    XSetForeground(mywin.d, mywin.gc, mywin.txtcolor[COLOR_DEFAULT].pixel);
    for(i = 0; i < NUMVSCREEN; i++)
	XFillRectangle(mywin.d, mywin.screens[i], mywin.gcgr,
		       0, 0, SIZEX, SIZEY);
    XFillRectangle(mywin.d, mywin.offscr, mywin.gcgr, 0, 0, SIZEX, SIZEY);
    XSetWindowBackgroundPixmap(mywin.d, mywin.w, mywin.gscreen);
    XFillRectangle(mywin.d, mywin.w, mywin.gcgr, 0, 0, SIZEX, SIZEY);
}

/*Graphic Definition*/
#define GRPH_LINE_MODE 1
#define GRPH_CIRCLE_MODE 2
#define GRPH_PAL_CHANGE 3
#define GRPH_FADE 4
#define GRPH_FADE_STEP 5
static void GrphCMD(int *params,int nparam)
{
  switch(params[0]){
  case GRPH_LINE_MODE:
    x_Gline(params+1,nparam-1);
    break;
  case GRPH_CIRCLE_MODE:
    x_GCircle(params+1,nparam-1);
    break;
  case GRPH_PAL_CHANGE:
    x_Pal(params+1,nparam-1);
    break;
  case GRPH_FADE:
    x_Fade(params+1,nparam-1,-1,-1);
    break;
  case GRPH_FADE_STEP:
    x_Fade(NULL,0,params[1],params[2]);
    break;
  }
}
/*****************************************************
 * VT parser
 *
 *
 ******************************************************/
#define MAXPARAM 20
static int Parse(int c)
{
  static int *prstbl=groundtable;
  static char mbcs;
  static int params[MAXPARAM],nparam=0;
  static int hankaku=0;
  static int savcol,savline;
  static long savattr;
  if(c==-1) {
    prstbl=groundtable;
    mbcs=0;
    nparam=0;
    hankaku=0;
    savcol=savline=0;
    return 0;
  }

  if(mbcs&&
     prstbl !=mbcstable&&
     prstbl !=scstable&&
     prstbl !=scstable){
    mbcs=0;
  }
  switch(prstbl[c]){
  case CASE_IGNORE_STATE:
    prstbl=igntable;
    break;
  case CASE_IGNORE_ESC:
    prstbl=iestable;
    break;
  case CASE_ESC:
    prstbl=esctable;
    break;
  case CASE_ESC_IGNORE:
    prstbl=eigtable;
    break;
  case CASE_ESC_DIGIT:
    if(nparam<MAXPARAM){
      if(params[nparam]==DEFAULT){
	params[nparam]=0;
      }
      params[nparam]*=10;
      params[nparam]+=c-'0';
    }
    break;
  case CASE_ESC_SEMI:
    nparam++;
    params[nparam]=DEFAULT;
    break;
  case CASE_TAB:
    mywin.curcol+=TAB_SET;
    mywin.curcol&=~(TAB_SET-1);
    break;
  case CASE_BS:
    if(mywin.curcol > 0)
      mywin.curcol--;
#if 0 /* ^H maybe work backward character in MIMPI's screen */
    DelChar(mywin.curline,mywin.curcol);
    mywin.scrnbuf[mywin.curline][mywin.curcol].c=0;
    mywin.scrnbuf[mywin.curline][mywin.curcol].attr=0;
#endif
    break;
  case CASE_CSI_STATE:
    nparam=0;
    params[0]=DEFAULT;
    prstbl=csitable;
    break;
  case CASE_SCR_STATE:
    prstbl=scrtable;
    mbcs=0;
    break;
  case CASE_MBCS:
    hankaku=0;
    prstbl=mbcstable;
    mbcs=MBCS;
    break;
  case CASE_SCS_STATE:
    if(mbcs)
      prstbl=smbcstable;
    else
      prstbl=scstable;
    break;
  case CASE_GSETS:
    mywin.curattr=(mbcs)?(mywin.curattr|CATTR_16FONT):
      (mywin.curattr&~(CATTR_16FONT));
    if(!mbcs){
      hankaku=(c=='I')?1:0;
    }
    prstbl=groundtable;
    break;
  case CASE_DEC_STATE:
    prstbl =dectable;
    break;
  case CASE_SS2:
  case CASE_SS3:
    /*These are ignored because this will not accept SS2 SS3 charset*/
  case CASE_GROUND_STATE:
    prstbl=groundtable;
    break;
  case CASE_CR:
    mywin.curcol=0;
    prstbl=groundtable;
    break;
  case CASE_IND:
  case CASE_VMOT:
    mywin.curline++;
    mywin.curcol=0;
    prstbl=groundtable;
    break;
  case CASE_CUP:
    mywin.curline=(params[0]<1)?0:params[0]-1;
    if(nparam>=1)
      mywin.curcol=(params[1]<1)?0:params[1]-1;
    else
      mywin.curcol=0;
    prstbl=groundtable;
    break;
  case CASE_PRINT:
    if(mywin.curcol==COLS){
      mywin.curcol++;
      return 1;
    }
    if(mywin.curattr&CATTR_16FONT){
      if(!(mywin.curattr&CATTR_LPART)&&(mywin.curcol==COLS-1)){
	mywin.curcol+=2;
	return 1;
      }
      mywin.curattr^=CATTR_LPART;
    }
    else
      mywin.curattr&=~CATTR_LPART;
    DelChar(mywin.curline,mywin.curcol);
    if(hankaku==1)
      c|=0x80;
    mywin.scrnbuf[mywin.curline][mywin.curcol].attr=mywin.curattr;
    mywin.scrnbuf[mywin.curline][mywin.curcol].c=c;  
    mywin.curcol++;
    break;
  case CASE_CUU:
    mywin.curline-=((params[0]<1)?1:params[0]);
    prstbl=groundtable;
    break;
  case CASE_CUD:
    mywin.curline+=((params[0]<1)?1:params[0]);
    prstbl=groundtable;
    break;
  case CASE_CUF:
    mywin.curcol+=((params[0]<1)?1:params[0]);
    prstbl=groundtable;
    break;
  case CASE_CUB:
    mywin.curcol-=((params[0]<1)?1:params[0]);
    prstbl=groundtable;
    break;
  case CASE_ED:
    switch(params[0]){
    case DEFAULT:
    case 1:
      {
	int j;
	if(mywin.scrnbuf[mywin.curline]!=NULL)
	  ClearLeft();
	for(j=0;j<mywin.curline;j++)
	  ClearLine(j);	
      }
      break;
    case 0:
      {
	int j;
	if(mywin.scrnbuf[mywin.curline]!=NULL){
	  ClearRight();
	}
	for(j=mywin.curline;j<LINES;j++)
	  ClearLine(j);	
      }
      break;
    case 2:
      {
	int j;
	for(j=0;j<LINES;j++){
	  free(mywin.scrnbuf[j]);
	  mywin.scrnbuf[j]=NULL;
	}
	mywin.curline=0;
	mywin.curcol=0;
	break;
      }
    }
    RedrawInject(0,0,SIZEX,SIZEY,False);
    prstbl=groundtable;
    break;
  case CASE_DECSC:
    savcol=mywin.curcol;
    savline=mywin.curline;
    savattr=mywin.curattr;
    prstbl=groundtable;
  case CASE_DECRC:
    mywin.curcol=savcol;
    mywin.curline=savline;
    mywin.curattr=savattr;
    prstbl=groundtable;
    break;
  case CASE_SGR:
    {
      int i;
      for(i=0;i<nparam+1;i++)
	switch(params[i]){
	default:
	  mywin.curattr&=~(CATTR_COLORED|CATTR_BGCOLORED|CATTR_TXTCOL_MASK);
	  break;
	case 16:
	case 17:
	case 18:
	case 19:
	case 20:
	case 21:
	case 22:
	case 23:
	  /* Remap 16-23 into 30-37 */
	  params[i] = wrd_color_remap[params[i] - 16] + 30;
	  /*FALLTHROUGH*/
	case 30:
	case 31:
	case 32:
	case 33:
	case 34:
	case 35:
	case 36:
	case 37:
	  mywin.curattr&=~CATTR_TXTCOL_MASK;
	  mywin.curattr|=(params[i]-30)<<CATTR_TXTCOL_MASK_SHIFT;
	  mywin.curattr|=CATTR_COLORED;
	  break;
	case 40:
	case 41:
	case 42:
	case 43:
	case 44:
	case 45:
	case 46:
	case 47:
	  mywin.curattr&=~CATTR_TXTCOL_MASK;
	  mywin.curattr&=~CATTR_COLORED;
	  mywin.curattr|=(params[i]-40)<<CATTR_TXTCOL_MASK_SHIFT;
	  mywin.curattr|=CATTR_BGCOLORED;
	  break;
	}
    }
    prstbl=groundtable;
    break;
  case CASE_EL:
    switch(params[0]){
    case DEFAULT:
    case 0:
      ClearRight();
      break;
    case 1:
      ClearLeft();
      break;
    case 2:
      ClearLine(mywin.curline);
      break;
    }
    prstbl=groundtable;
    break;
  case CASE_NEL:
    mywin.curline++;
    mywin.curcol=0;
    mywin.curline=(mywin.curline<LINES)?mywin.curline:LINES;
    break;
/*Graphic Commands*/
  case CASE_MY_GRAPHIC_CMD:
    GrphCMD(params,nparam);
    prstbl=groundtable;
    break;
/*Unimpremented Command*/
  case CASE_ICH:
  case CASE_IL:
  case CASE_DL:
  case CASE_DCH:
  case CASE_DECID:
  case CASE_DECKPAM:
  case CASE_DECKPNM:
  case CASE_HP_BUGGY_LL:
  case CASE_HTS:
  case CASE_RI:
  case CASE_DA1:
  case CASE_CPR:
  case CASE_DECSET:
  case CASE_RST:
  case CASE_DECSTBM:
  case CASE_DECREQTPARM:
  case CASE_OSC:
  case CASE_RIS:
  case CASE_HP_MEM_LOCK:
  case CASE_HP_MEM_UNLOCK:
  case CASE_LS2:
  case CASE_LS3:
  case CASE_LS3R:
  case CASE_LS2R:
  case CASE_LS1R:
    ctl->cmsg(CMSG_INFO,VERB_VERBOSE,"NOT IMPREMENTED:%d\n",prstbl[c]);
    prstbl=groundtable;
    break;
  case CASE_BELL:
  case CASE_IGNORE:
  default:
    break;
  }
  return 0;
}
void AddLine(const unsigned char *str,int len)
{
  Linbuf *ptr;
  int i,j;
  /*Initialize Redraw rectangle Manager*/
  RedrawInject(-1,-1,-1,-1,False);
  
  /*Allocate LineBuffer*/
  if(len==0)
    len=strlen(str);
  for(i=0;i<len;i++){
    if(mywin.scrnbuf[mywin.curline]==NULL){
      ptr=(Linbuf *)calloc(COLS,sizeof(Linbuf)+1);
      if(ptr==NULL)
	exit(-1);
      else
	mywin.scrnbuf[mywin.curline]=ptr;
    }
    /*
     * Proc Each Charactor
     * If >0 Returned unput current value
     */
    if(Parse(str[i])!=0){
      i--;
    }
    /*Wrapping Proc*/
    while(mywin.curcol>=COLS+1){
      mywin.curcol-=COLS;
      mywin.curline++;
    }
    while(mywin.curcol<0){
      mywin.curcol+=COLS;
      mywin.curline--;
    }
    /*Scroll Proc*/
    mywin.curline=(mywin.curline<0)?0:mywin.curline;
    while(mywin.curline>=LINES){
      mywin.curline--;
      free(mywin.scrnbuf[0]);
      mywin.scrnbuf[0]=NULL;
      for(j=1;j<LINES;j++){
	mywin.scrnbuf[j-1]=mywin.scrnbuf[j];
      }
      mywin.scrnbuf[LINES-1]=NULL;
      RedrawInject(0,0,SIZEX,SIZEY,False);
    }
  }
  RedrawInject(0,0,0,0,True);
}
void WinFlush(void){
  if(mywin.redrawflag)
    XFlush(mywin.d);
}
/*
 *This Function Dumps Charactor Screen buffer status
 *Purely Debugging Purpose this code should be desabled 
 *if you need not.
 */

#ifdef SCREENDEBUG
DebugDump(void)
{
  FILE *f;
  int i,j;
  f=fopen("screen","w+");
  for(i=0;i<LINES;i++){
    fprintf(f,"LINE %d \n",i);
    for(j=0;j<COLS;j++){
      if(mywin.scrnbuf[i]!=NULL){
	Linbuf *a=&mywin.scrnbuf[i][j];
	fprintf(f,"{%x %c}",a->attr,a->c);
      }
    }
   fprintf(f,"\n");
  }
  fclose(f);
}
#endif
void WinEvent(void)
{
  XEvent e;
  int rdx1, rdy1, rdx2, rdy2; 
  rdx1 = rdy1 = rdx2 = rdy2 = -1;
  XSync(mywin.d, False);
  while(QLength(mywin.d)>0){
    XNextEvent(mywin.d,&e);
    switch(e.type){
    case Expose:
      if(rdx1 == -1) {
	rdx1 = e.xexpose.x;
	rdy1 = e.xexpose.y;
	rdx2 = e.xexpose.x + e.xexpose.width;
	rdy2 = e.xexpose.y + e.xexpose.height;
      } else {
	rdx1 = min(rdx1, e.xexpose.x);
	rdy1 = min(rdy1, e.xexpose.y);
	rdx2 = max(rdx2, e.xexpose.x + e.xexpose.width);
	rdy2 = max(rdy2, e.xexpose.y + e.xexpose.height);
      }
    case ButtonPress:
      Redraw(0,0,SIZEX,SIZEY,True);
      rdx1=0;
      rdy1=0;
      rdx2=SIZEX;
      rdy2=SIZEY;
      if(e.xbutton.button==3){
#ifdef SCREENDEBUG
	DebugDump();
#endif
      }
    }
  }
  
  if(rdx1 != -1){
    Redraw(rdx1, rdy1, rdx2 - rdx1, rdy2 - rdy1,False);
    XFlush(mywin.d);
  }
}
void EndWin(void)
{
  if(mywin.d!=NULL)
  {
    XCloseDisplay(mywin.d);
    free(image_buffer);
  }
  mywin.d=NULL;
}  

int OpenWRDWindow(char *opt)
{
    if(InitWin(opt) == -1)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "WRD: Can't open WRD window becase of error");
	return -1;
    }
    XMapWindow(mywin.d, mywin.w);
    XSync(mywin.d, False);
    return 0;
}

void CloseWRDWindow(void)
{
    if(mywin.d != NULL)
    {
	XUnmapWindow(mywin.d, mywin.w);
	XSync(mywin.d, False);
    }
}
