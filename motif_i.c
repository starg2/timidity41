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
   
   motif_interface.c: written by Vincent Pagel (pagel@loria.fr) 10/4/95
   
   Policy : I choose to make a separate process for a TIMIDITY motif 
   interface for TIMIDITY (if the interface was in the same process
   X redrawings would interfere with the audio computation. Besides 
   XtAppAddWorkProc mechanism is not easily controlable)
   
   The solution : I create a pipe between Timidity and the forked interface
   and use XtAppAddInput to watch the data arriving on the pipe.

   10/4/95 
     - Initial working version with prev, next, and quit button and a
       text display

   17/5/95 
     - Add timidity icon with filename displaying to play midi while 
       I work without having that big blue window in the corner of 
       my screen :)
     - Solve the problem of concurent scale value modification

   21/5/95
     - Add menus, file selection box
   
   14/6/95
     - Make the visible part of file list follow the selection

   */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>

#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Text.h>
#include <Xm/PushB.h>
#include <Xm/Label.h>
#include <Xm/Scale.h>
#include <Xm/List.h>
#include <Xm/Frame.h>
#include <Xm/RowColumn.h>
#include <Xm/CascadeB.h>
#include <Xm/FileSB.h>
#include <Xm/FileSB.h>
#include <Xm/ToggleB.h>

#include "config.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "output.h"
#include "controls.h"
#include "motif.h"

XtAppContext context;
XmStringCharSet char_set=XmSTRING_DEFAULT_CHARSET;

Widget toplevel;

Widget text;
static XmTextPosition wpr_position=0;

Widget mainForm; 

Widget menu_bar, open_option, quit_option, auto_next_option;
Widget open_dialog , add_all_button;

Widget btnForm, backBtn,fwdBtn, restartBtn, pauseBtn, quitBtn,
       nextBtn, prevBtn;

Pixmap backPixmap, fwdPixmap, pausePixmap , restartPixmap, 
       playPixmap, prevPixmap, nextPixmap, quitPixmap,
       timidityPixmap;

Widget countFrame, countForm, counterlbl, totlbl , count_headlbl;
int last_sec=0;
int max_sec=0;

Widget file_namelbl, file_headlbl;

Widget volume_scale , locator_scale ;
Boolean locator_scale_button= ButtonRelease;

Widget file_list;
int file_number_to_play; /* Number of the file we're playing in the list */

/*
 * CREATE PIXMAPS FOR THE BUTONS
 */
void CreatePixmaps(Widget parent)
{
  /*
   * 
   * BITMAPS 
   */
    #include "BITMAPS/back.xbm"
    #include "BITMAPS/next.xbm"
    #include "BITMAPS/prev.xbm"
    #include "BITMAPS/restart.xbm"
    #include "BITMAPS/fwd.xbm"
    #include "BITMAPS/pause.xbm"
    #include "BITMAPS/quit.xbm"
    #include "BITMAPS/timidity.xbm"

    Display *disp;
    Drawable d;
    Pixel fg,bg;
    int ac;
    Arg al[20];
    unsigned int depth=DefaultDepthOfScreen(XtScreen(toplevel));

    ac = 0;
    XtSetArg(al[ac], XmNbackground, &bg); ac++;
    XtSetArg(al[ac], XmNforeground, &fg); ac++;
    XtGetValues(parent, al, ac);

    disp=XtDisplay(toplevel);
    d=RootWindowOfScreen(XtScreen(toplevel));
    
    backPixmap = XCreatePixmapFromBitmapData(disp, d,
					     back_bits, back_width, back_height,
					     fg, bg,depth);
    fwdPixmap = XCreatePixmapFromBitmapData( disp, d,
					     fwd_bits, fwd_width, fwd_height,
					     fg, bg,depth);
    pausePixmap = XCreatePixmapFromBitmapData(disp, d,
					pause_bits, pause_width, pause_height,
					fg, bg,depth);
    
    restartPixmap = XCreatePixmapFromBitmapData(disp, d,
					  restart_bits, restart_width, restart_height,
					  fg, bg,depth);
    
    nextPixmap  = XCreatePixmapFromBitmapData(disp, d,
					next_bits, next_width, next_height,
					fg, bg,depth);
    
    prevPixmap  = XCreatePixmapFromBitmapData(disp, d,
					prev_bits, prev_width, prev_height,
					fg, bg,depth);
    
    quitPixmap  = XCreatePixmapFromBitmapData(disp, d,
					quit_bits, quit_width, quit_height,
					fg, bg,depth);

    timidityPixmap  = XCreatePixmapFromBitmapData(disp, d,
						  timidity_bits, timidity_width, timidity_height,
						  WhitePixelOfScreen(XtScreen(toplevel)),
						  BlackPixelOfScreen(XtScreen(toplevel)),depth);
}


/************************************
 *                                  *
 * ALL THE INTERFACE'S CALLBACKS    *
 *                                  *
 ************************************/

/*
 * Generic buttons callbacks ( Transport Buttons )
 */
void  GenericCB(Widget widget, int data, XtPointer call_data)
{
    pipe_int_write( data );
}

/*
 *  Generic scales callbacks : VOLUME and LOCATOR
 */
void Generic_scaleCB(Widget widget, int data, XtPointer call_data)
{ 
    XmScaleCallbackStruct *cbs = (XmScaleCallbackStruct *) call_data;
    
    pipe_int_write(  data );
    pipe_int_write(cbs->value);
}

/* 
 * Detect when a mouse button is pushed or released in a scale area to
 * avoid concurent scale value modification while holding it with the mouse
 */
void Locator_btn(Widget w,XtPointer client_data,XEvent *event,Boolean *cont)
{ 
    /* Type = ButtonPress or ButtonRelease */
    locator_scale_button= event->xbutton.type;
}

/*
 * File List selection CALLBACK
 */
void File_ListCB(Widget widget, int data, XtPointer call_data)
{
    XmListCallbackStruct *cbs= (XmListCallbackStruct *) call_data;
    char *text;
    int nbvisible, first_visible ;
    Arg al[10];
    int ac;
    
    /* First, check that the selected file is really visible in the list */
    ac=0;
    XtSetArg(al[ac],XmNtopItemPosition,&first_visible); ac++;
    XtSetArg(al[ac],XmNvisibleItemCount,&nbvisible); ac++;
    XtGetValues(widget, al, ac);
     
    if ( ( first_visible > cbs->item_position) ||
         ((first_visible+nbvisible) <= cbs->item_position))
	XmListSetPos(widget, cbs->item_position);

    /* Tell the application to play the requested file */
    XmStringGetLtoR(cbs->item,char_set,&text);
    pipe_int_write(MOTIF_PLAY_FILE);
    pipe_string_write(text);
    file_number_to_play=cbs->item_position;
    XtFree(text);
}

/*
 * Generic menu callback
 */
void menuCB(Widget w,int client_data,XmAnyCallbackStruct *call_data)
{
    switch (client_data)
	{
	case MENU_OPEN: 
	{
	    XtManageChild(open_dialog);
	}
	break;
	
	case MENU_QUIT : {
	    pipe_int_write(MOTIF_QUIT);
	}
	    break;
	case MENU_TOGGLE : {
	    /* Toggle modified : for the moment , nothing to do ! */
	    /* if (XmToggleButtonGetState(w)) TRUE else FALSE */
	    }
	    break;
	}
}

/* 
 * File selection box callback 
 */
void openCB(Widget w,int client_data,XmFileSelectionBoxCallbackStruct *call_data)
{ 
    if (client_data==DIALOG_CANCEL)
	{ /* do nothing if cancel is selected. */
	    XtUnmanageChild(open_dialog);
	    return;
	}
    else if (client_data==DIALOG_ALL)
	{ /* Add all the listed files  */
	    Arg al[10];
	    int ac;
	    Widget the_list;
	    int nbfile;
	    XmStringTable files;
	    int i;
	   	    
	    the_list=XmFileSelectionBoxGetChild(open_dialog,XmDIALOG_LIST);
	    if (!XmIsList(the_list))
		{
		    printf("PANIC: List are not what they used to be\n");
		    exit;
		}
	    
	    ac=0;
	    XtSetArg(al[ac], XmNitemCount, &nbfile); ac++;
	    XtSetArg(al[ac], XmNitems, &files); ac++;
	    XtGetValues(the_list, al, ac);
	    
	    for (i=0;i<nbfile;i++)
		XmListAddItemUnselected(file_list,files[i],0);
	}
    else
	{   /* get filename from file selection box and add it to the list*/
	    XmListAddItemUnselected(file_list,call_data->value,0);
	    XtUnmanageChild(open_dialog);
	}
}


/********************************************************
 *                                                      *
 * Receive DATA sent by the application on the pipe     *
 *                                                      *
 ********************************************************/
void handle_input(client_data, source, id)
    XtPointer client_data;
    int *source;
    XtInputId *id;
{
    int message;
     
    pipe_int_read(&message);

    switch (message)
	{
	case REFRESH_MESSAGE : {
	    printf("REFRESH MESSAGE IS OBSOLETE !!!\n");
	}
	    break;
	    
	case TOTALTIME_MESSAGE : { 
	    int cseconds;
	    int minutes,seconds;
	    char local_string[20];
	    Arg al[10];
	    int ac;

	    pipe_int_read(&cseconds);
	    
	    seconds=cseconds/100;
	    minutes=seconds/60;
	    seconds-=minutes*60;
	    sprintf(local_string,"/ %i:%02i",minutes,seconds);
	    ac=0;
	    XtSetArg(al[ac], XmNlabelString,
		     XmStringCreate(local_string, char_set)); ac++;
	    XtSetValues(totlbl, al, ac);
	    
	    /* Readjust the time scale */
	    XmScaleSetValue(locator_scale,0);
	    ac=0;
	    XtSetArg(al[ac], XmNmaximum, cseconds); ac++;
	    XtSetValues(locator_scale, al, ac);
	    max_sec=cseconds;
	}
	    break;
	    
	case MASTERVOL_MESSAGE: { 
	    int volume;
	    
	    pipe_int_read(&volume);
	    XmScaleSetValue(volume_scale,volume);
	}
	    break;
	    
	case FILENAME_MESSAGE : {
	    char filename[255], separator[255];
	    Arg al[10];
	    char *pc;
	    int ac, i;
	    short nbcol;
	    
	    pipe_string_read(filename);

	    /* Extract basename of the file */
	    pc=strrchr(filename,'/');
	    if (pc==NULL)
		pc=filename;
	    else 
		pc++;
	    
	    ac=0;
	    XtSetArg(al[ac], XmNlabelString,
		     XmStringCreate(pc, char_set)); ac++;
	    XtSetValues(file_namelbl, al, ac);
	    
	    /* Change the icon  */
	    ac=0;
	    XtSetArg(al[ac], XmNiconName,pc); ac++;
	    XtSetValues(toplevel,al,ac);

	    /* Put a separator in the text Window */
	    ac=0;
	    XtSetArg(al[ac], XmNcolumns,&nbcol); ac++;
	    XtGetValues(text,al,ac);
	    
	    for (i=0;i<nbcol;i++)
		separator[i]='*';
	    separator[i]='\0';

	    XmTextInsert(text,wpr_position, separator);
	    wpr_position+= strlen(separator);
	    XmTextInsert(text,wpr_position++,"\n");
	    XtVaSetValues(text,XmNcursorPosition, wpr_position,NULL);
	    XmTextShowPosition(text,wpr_position);
	}
	    break;
	
	case FILE_LIST_MESSAGE : {
	    char filename[255];
	    int i, number_of_files;
	    XmString s;
	    
	    /* reset the playing list : play from the start */
	    file_number_to_play=0;
	    
	    pipe_int_read(&number_of_files);
	    
	    for (i=0;i<number_of_files;i++)
		{
		    pipe_string_read(filename);
		    s=XmStringCreate(filename,char_set);
		    XmListAddItemUnselected(file_list,s,0);
		    XmStringFree(s);
		}
	}
	    break;
	    
	case NEXT_FILE_MESSAGE :
	case PREV_FILE_MESSAGE : 
	case TUNE_END_MESSAGE :{
	    Arg al[10];
	    int ac;
	    int nbfile;
	    
	    /* When a file ends, launch next if auto_next toggle */
	    if ((message==TUNE_END_MESSAGE) &&
		!XmToggleButtonGetState(auto_next_option))
		return;
	    
	    /* Total number of file to play in the list */
	    ac=0;
	    XtSetArg(al[ac], XmNitemCount, &nbfile); ac++;
	    XtGetValues(file_list, al, ac);
	    XmListDeselectAllItems(file_list); 
	    
	    if (message==PREV_FILE_MESSAGE)
		file_number_to_play--;
	    else 
		file_number_to_play++;

	    /* Do nothing if requested file is before first one */
	    if (file_number_to_play<0)
		{
		    file_number_to_play=1;
		    return;
		}

	    /* Stop after playing the last file */
	    if (file_number_to_play>nbfile)
		{ 
		    file_number_to_play=nbfile;
		    return;
		}
	    
	    XmListSelectPos(file_list,file_number_to_play,TRUE);
	}
	    break;

	case CURTIME_MESSAGE : { 
	    int cseconds;
	    int  sec,seconds, minutes;
	    int nbvoice;
	    char local_string[20];
	    Arg al[10];
	    int ac;

	    pipe_int_read(&cseconds);
	    pipe_int_read(&nbvoice);
	    
	    sec=seconds=cseconds/100;
	    
				/* To avoid blinking */
	    if (sec!=last_sec)
		{
		    minutes=seconds/60;
		    seconds-=minutes*60;
		    
		    sprintf(local_string,"%2d:%02d",
			    minutes, seconds);
	    
		    ac=0;
		    XtSetArg(al[ac], XmNlabelString,
			     XmStringCreate(local_string, char_set)); ac++;
		    XtSetValues(counterlbl, al, ac);
		}

	    last_sec=sec;
	    
	    /* Readjust the time scale if not dragging the scale */
	    if ( (locator_scale_button==ButtonRelease) &&
		 (cseconds<=max_sec))
		XmScaleSetValue(locator_scale, cseconds);
	}
	    break;
	    
	case NOTE_MESSAGE : {
	    int channel;
	    int note;
	    
	    pipe_int_read(&channel);
	    pipe_int_read(&note);
	    printf("NOTE chn%i %i\n",channel,note);
	}
	    break;
	    
	case    PROGRAM_MESSAGE :{
	    int channel;
	    int pgm;
	    
	    pipe_int_read(&channel);
	    pipe_int_read(&pgm);
	    printf("NOTE chn%i %i\n",channel,pgm);
	}
	    break;
	    
	case VOLUME_MESSAGE : { 
	    int channel;
	    int volume;
	    
	    pipe_int_read(&channel);
	    pipe_int_read(&volume);
	    printf("VOLUME= chn%i %i \n",channel, volume);
	}
	    break;
	    
	    
	case EXPRESSION_MESSAGE : { 
	    int channel;
	    int express;
	    
	    pipe_int_read(&channel);
	    pipe_int_read(&express);
	    printf("EXPRESSION= chn%i %i \n",channel, express);
	}
	    break;
	    
	case PANNING_MESSAGE : { 
	    int channel;
	    int pan;
	    
	    pipe_int_read(&channel);
	    pipe_int_read(&pan);
	    printf("PANNING= chn%i %i \n",channel, pan);
	}
	    break;
	    
	case  SUSTAIN_MESSAGE : { 
	    int channel;
	    int sust;
	    
	    pipe_int_read(&channel);
	    pipe_int_read(&sust);
	    printf("SUSTAIN= chn%i %i \n",channel, sust);
	}
	    break;
	    
	case  PITCH_MESSAGE : { 
	    int channel;
	    int bend;
	    
	    pipe_int_read(&channel);
	    pipe_int_read(&bend);
	    printf("PITCH BEND= chn%i %i \n",channel, bend);
	}
	    break;
	    
	case RESET_MESSAGE : {
	    printf("RESET_MESSAGE\n");
	}
	    break;   
	    
	case CLOSE_MESSAGE : {
	    printf("CLOSE_MESSAGE\n");
	    exit(0);
	}
	    break;
	    
	case CMSG_MESSAGE : { 
	    int type;
	    char message[1000];
	    
	    pipe_int_read(&type);
	    pipe_string_read(message);
	    
	    XmTextInsert(text,wpr_position, message);
	    wpr_position+= strlen(message);
	    XmTextInsert(text,wpr_position++,"\n");
	    XtVaSetValues(text,XmNcursorPosition, wpr_position,NULL);
	    XmTextShowPosition(text,wpr_position);
	}
	    break;
	default:    
	    fprintf(stderr,"UNKNOW MOTIF MESSAGE %i\n",message);
	}
    
}

/* ***************************************
 *                                       *
 * Convenience function to create menus  *
 *                                       *
 *****************************************/

/* adds an accelerator to a menu option. */
void add_accelerator(Widget w,char *acc_text,char *key)
{
    int ac;
    Arg al[10];
    
    ac=0;
    XtSetArg(al[ac],XmNacceleratorText,
	     XmStringCreate(acc_text,char_set)); ac++;
    XtSetArg(al[ac],XmNaccelerator,key); ac++;
    XtSetValues(w,al,ac);
}

/* Adds a toggle option to an existing menu. */
Widget make_menu_toggle(char *item_name, int client_data, Widget menu)
{
    int ac;
    Arg al[10];
    Widget item;

    ac = 0;
    XtSetArg(al[ac],XmNlabelString, XmStringCreateLtoR(item_name,
						       char_set)); ac++;
    item=XmCreateToggleButton(menu,item_name,al,ac);
    XtManageChild(item);
    XtAddCallback(item, XmNvalueChangedCallback, 
		  (XtCallbackProc) menuCB,(XtPointer) client_data);
    XtSetSensitive(item, True);
    return(item);
}

/* Adds an option to an existing menu. */
Widget make_menu_option(char *option_name, KeySym mnemonic,
			int client_data, Widget menu)
{
    int ac;
    Arg al[10];
    Widget b;
    
    ac = 0;
    XtSetArg(al[ac], XmNlabelString,
	     XmStringCreateLtoR(option_name, char_set)); ac++;
    XtSetArg (al[ac], XmNmnemonic, mnemonic); ac++;
    b=XtCreateManagedWidget(option_name,xmPushButtonWidgetClass,
			    menu,al,ac);
    XtAddCallback (b, XmNactivateCallback,
		   (XtCallbackProc) menuCB, (XtPointer) client_data);
    return(b);
}

/* Creates a new menu on the menu bar. */
Widget make_menu(char *menu_name,KeySym  mnemonic, Widget menu_bar)
{
    int ac;
    Arg al[10];
    Widget menu, cascade;

    ac = 0;
    menu = XmCreatePulldownMenu (menu_bar, menu_name, al, ac);

    ac = 0;
    XtSetArg (al[ac], XmNsubMenuId, menu); ac++;
    XtSetArg (al[ac], XmNmnemonic, mnemonic); ac++;
    XtSetArg(al[ac], XmNlabelString,
        XmStringCreateLtoR(menu_name, char_set)); ac++;
    cascade = XmCreateCascadeButton (menu_bar, menu_name, al, ac);
    XtManageChild (cascade); 

    return(menu);
}

/* *******************************************
 *                                           *
 * Interface initialisation before launching *
 *                                           *
 *********************************************/

void create_menus(Widget menu_bar)
{
    Widget menu;
    
    menu=make_menu("File",'F',menu_bar);
    open_option = make_menu_option("Open", 'O', MENU_OPEN, menu);
    add_accelerator(open_option, "meta+o", "Meta<Key>o:");
    
    quit_option = make_menu_option("Exit", 'E', MENU_QUIT, menu);
    add_accelerator(quit_option, "meta+q", "Meta<Key>q:");

    menu=make_menu("Options",'O',menu_bar);
    auto_next_option= make_menu_toggle("Auto Next", MENU_TOGGLE, menu);
    XmToggleButtonSetState( auto_next_option , True , False );


}

void create_dialog_boxes()
{
    Arg al[10];
    int ac;
    XmString add_all = XmStringCreateLocalized("ADD ALL");
  
    /* create the file selection box used by MENU_OPEN */
    ac=0;
    XtSetArg(al[ac],XmNmustMatch,True); ac++;
    XtSetArg(al[ac],XmNautoUnmanage,False); ac++;
    XtSetArg(al[ac],XmNdialogTitle,
	     XmStringCreateLtoR("TIMIDITY: Open",char_set)); ac++;
    open_dialog=XmCreateFileSelectionDialog(toplevel,"open_dialog",al,ac);
    XtAddCallback(open_dialog, XmNokCallback, 
		  (XtCallbackProc) openCB, (XtPointer) DIALOG_OK);
    XtAddCallback(open_dialog, XmNcancelCallback, 
		  (XtCallbackProc) openCB, (XtPointer) DIALOG_CANCEL);
    XtUnmanageChild(XmFileSelectionBoxGetChild(open_dialog, XmDIALOG_HELP_BUTTON));
    
    ac = 0;
    XtSetArg(al[ac], XmNleftOffset, 10); ac++;
    XtSetArg(al[ac], XmNrightOffset, 10); ac++;
    XtSetArg(al[ac], XmNtopOffset, 10); ac++;
    XtSetArg(al[ac], XmNbottomOffset, 10); ac++;
    XtSetArg(al[ac], XmNlabelString, add_all); ac++;
    add_all_button = XmCreatePushButton(open_dialog, "add_all",al, ac);
    XtManageChild(add_all_button); 
    XtAddCallback(add_all_button, XmNactivateCallback,
		  (XtCallbackProc) openCB, (XtPointer) DIALOG_ALL);
}

void Launch_Motif_Process(int pipe_number)
{
    Arg al[20];
    int ac;
    int argc=0;

    /* create the toplevel shell */
    toplevel = XtAppInitialize(&context,"timidity",NULL,0,&argc,NULL,
			       NULL,NULL,0);
    
    /*******************/
    /* Main form       */
    /*******************/
    ac=0;
    XtSetArg(al[ac],XmNtopAttachment,XmATTACH_FORM); ac++;
    XtSetArg(al[ac],XmNbottomAttachment,XmATTACH_FORM); ac++;
    XtSetArg(al[ac],XmNrightAttachment,XmATTACH_FORM); ac++;
    XtSetArg(al[ac],XmNleftAttachment,XmATTACH_FORM); ac++;
    
    mainForm=XmCreateForm(toplevel,"form",al,ac);
    XtManageChild(mainForm);

    CreatePixmaps(mainForm); 
  

    /* create a menu bar and attach it to the form. */
    ac=0;
    XtSetArg(al[ac], XmNtopAttachment,   XmATTACH_FORM); ac++;
    XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    XtSetArg(al[ac], XmNleftAttachment,  XmATTACH_FORM); ac++;
    menu_bar=XmCreateMenuBar(mainForm,"menu_bar",al,ac);
    XtManageChild(menu_bar);
    
    create_dialog_boxes();
    create_menus(menu_bar);

    /*******************/
    /* Message window  */
    /*******************/
    
    ac=0;
    XtSetArg(al[ac], XmNleftOffset, 10); ac++;
    XtSetArg(al[ac], XmNrightOffset, 10); ac++;
    XtSetArg(al[ac], XmNtopOffset, 10); ac++;
    XtSetArg(al[ac], XmNbottomOffset, 10); ac++;
    XtSetArg(al[ac],XmNtopAttachment,XmATTACH_WIDGET); ac++;
    XtSetArg(al[ac],XmNtopWidget, menu_bar); ac++;
    XtSetArg(al[ac],XmNrightAttachment,XmATTACH_FORM); ac++;
    XtSetArg(al[ac],XmNleftAttachment,XmATTACH_FORM); ac++;
    XtSetArg(al[ac],XmNeditMode,XmMULTI_LINE_EDIT); ac++;
    XtSetArg(al[ac],XmNrows,10); ac++;
    XtSetArg(al[ac],XmNcolumns,10); ac++;
    XtSetArg(al[ac],XmNeditable, False); ac++;
    XtSetArg(al[ac],XmNwordWrap, True); ac++;
    XtSetArg(al[ac],XmNvalue, "TIMIDIY RUNNING...\n"); ac++;
    wpr_position+= strlen("TIMIDIY RUNNING...\n");
    
    text=XmCreateScrolledText(mainForm,"text",al,ac);
    XtManageChild(text);    
   
    /********************/ 
    /* File_name label  */
    /********************/
    ac = 0;
    XtSetArg(al[ac], XmNleftOffset, 20); ac++;
    XtSetArg(al[ac], XmNrightOffset, 10); ac++;
    XtSetArg(al[ac], XmNtopOffset, 20); ac++;
    XtSetArg(al[ac], XmNbottomOffset, 20); ac++;
    XtSetArg(al[ac], XmNlabelType, XmSTRING); ac++;
    XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(al[ac], XmNtopWidget, text); ac++;
    XtSetArg(al[ac], XmNleftAttachment,XmATTACH_FORM); ac++;
    XtSetArg(al[ac], XmNtraversalOn, False); ac++;
    XtSetArg(al[ac], XmNhighlightThickness,0); ac++;
    XtSetArg(al[ac], XmNalignment,XmALIGNMENT_END); ac++;
    XtSetArg(al[ac], XmNlabelString,
	     XmStringCreate("Playing:",char_set)); ac++;
    file_headlbl = XmCreateLabel(mainForm,"fileheadlbl",al,ac);
    XtManageChild(file_headlbl);
    

    ac = 0;
    XtSetArg(al[ac], XmNrightOffset, 10); ac++;
    XtSetArg(al[ac], XmNtopOffset, 20); ac++;
    XtSetArg(al[ac], XmNbottomOffset, 20); ac++;
    XtSetArg(al[ac], XmNlabelType, XmSTRING); ac++;
    XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(al[ac], XmNtopWidget, text); ac++;
    XtSetArg(al[ac], XmNleftAttachment,XmATTACH_WIDGET); ac++;
    XtSetArg(al[ac], XmNleftWidget,file_headlbl); ac++;
    XtSetArg(al[ac], XmNtraversalOn, False); ac++;
    XtSetArg(al[ac], XmNhighlightThickness,0); ac++;
    XtSetArg(al[ac], XmNalignment,XmALIGNMENT_BEGINNING); ac++;
    XtSetArg(al[ac], XmNlabelString,
	     XmStringCreate("NONE           ",char_set)); ac++;
    file_namelbl = XmCreateLabel(mainForm,"filenameLbl",al,ac);
    XtManageChild(file_namelbl);

    /*****************************/
    /* TIME LABELS IN A FORM     */
    /*****************************/
   
    /* Counters frame    */
    ac=0;
    XtSetArg(al[ac], XmNtopOffset, 10); ac++;
    XtSetArg(al[ac], XmNbottomOffset, 10); ac++;
    XtSetArg(al[ac], XmNleftOffset, 10); ac++;
    XtSetArg(al[ac], XmNrightOffset, 10); ac++;
    XtSetArg(al[ac],XmNtopAttachment,XmATTACH_WIDGET); ac++;
    XtSetArg(al[ac],XmNtopWidget,text); ac++;
    XtSetArg(al[ac],XmNrightAttachment,XmATTACH_FORM); ac++;
    /*
      XtSetArg(al[ac],XmNleftAttachment,XmATTACH_WIDGET); ac++;
      XtSetArg(al[ac],XmNleftWidget,file_namelbl); ac++;
      */
    XtSetArg(al[ac],XmNshadowType,XmSHADOW_OUT); ac++;
    countFrame=XmCreateFrame(mainForm,"countframe",al,ac);
    XtManageChild(countFrame);

    /* Counters form       */
    ac=0;
    XtSetArg(al[ac],XmNtopAttachment,XmATTACH_FORM); ac++;
    XtSetArg(al[ac],XmNbottomAttachment,XmATTACH_FORM); ac++;
    XtSetArg(al[ac],XmNrightAttachment,XmATTACH_FORM); ac++;
    XtSetArg(al[ac],XmNleftAttachment,XmATTACH_FORM); ac++;
    XtSetArg(al[ac],XmNleftAttachment,XmATTACH_FORM); ac++;
   
    countForm=XmCreateForm(countFrame,"countform",al,ac);
    XtManageChild(countForm);
     
    /* HEADER label       */
    ac = 0;
    XtSetArg(al[ac], XmNtopOffset, 10); ac++;
    XtSetArg(al[ac], XmNlabelType, XmSTRING); ac++;
    XtSetArg(al[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(al[ac], XmNleftAttachment,XmATTACH_FORM); ac++;
    XtSetArg(al[ac], XmNrightAttachment,XmATTACH_FORM); ac++;
    XtSetArg(al[ac], XmNtraversalOn, False); ac++;
    XtSetArg(al[ac], XmNhighlightThickness,0); ac++;
    XtSetArg(al[ac], XmNlabelString,
	     XmStringCreate("Tempus Fugit",char_set)); ac++;
    count_headlbl = XmCreateLabel(countForm,"countheadLbl",al,ac);
    XtManageChild(count_headlbl);

    /* current Time label       */
    ac = 0;
    XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(al[ac], XmNtopWidget, count_headlbl); ac++;
    XtSetArg(al[ac], XmNleftAttachment,XmATTACH_FORM); ac++;
    XtSetArg(al[ac], XmNbottomAttachment,XmATTACH_FORM); ac++;
    XtSetArg(al[ac], XmNtraversalOn, False); ac++;
    XtSetArg(al[ac], XmNhighlightThickness,0); ac++;
    XtSetArg(al[ac], XmNalignment,XmALIGNMENT_END); ac++;
    XtSetArg(al[ac], XmNlabelString,
	     XmStringCreate("00:00",char_set)); ac++;
    counterlbl = XmCreateLabel(countForm,"counterLbl",al,ac);
    XtManageChild(counterlbl);
    
    /* Total time label */
        
    ac = 0;
    XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(al[ac], XmNtopWidget, count_headlbl); ac++;
    XtSetArg(al[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(al[ac], XmNleftWidget, counterlbl); ac++;
    XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    XtSetArg(al[ac], XmNbottomAttachment,XmATTACH_FORM); ac++;
    XtSetArg(al[ac], XmNtraversalOn, False); ac++;
    XtSetArg(al[ac], XmNhighlightThickness,0); ac++;
    XtSetArg(al[ac], XmNalignment,XmALIGNMENT_BEGINNING); ac++;
    XtSetArg(al[ac], XmNlabelString,
	     XmStringCreate("/ 00:00",char_set)); ac++;
    totlbl = XmCreateLabel(countForm,"TotalTimeLbl",al,ac);
    XtManageChild(totlbl);
            
    /******************/ 
    /* Locator Scale  */
    /******************/
    {	/*
	 * We need to add an xevent manager for buton pressing since 
	 * locator_scale is a critical ressource that can be modified
	 * by shared by the handle input function
	 */

	WidgetList WList;
	Cardinal Card;

	ac = 0;
	XtSetArg(al[ac], XmNleftOffset, 10); ac++;
	XtSetArg(al[ac], XmNrightOffset, 10); ac++;
	XtSetArg(al[ac], XmNtopOffset, 10); ac++;
	XtSetArg(al[ac], XmNbottomOffset, 10); ac++;
	XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(al[ac], XmNtopWidget, countForm); ac++;
	XtSetArg(al[ac], XmNleftAttachment,XmATTACH_FORM); ac++;
	XtSetArg(al[ac], XmNrightAttachment,XmATTACH_FORM); ac++;
	XtSetArg(al[ac], XmNmaximum, 100); ac++;
	XtSetArg(al[ac], XmNminimum, 0); ac++;
	XtSetArg(al[ac], XmNshowValue, True); ac++;
	XtSetArg(al[ac], XmNdecimalPoints, 2); ac++;
	XtSetArg(al[ac], XmNorientation, XmHORIZONTAL); ac++;
	XtSetArg(al[ac], XmNtraversalOn, False); ac++;
	XtSetArg(al[ac], XmNhighlightThickness,0); ac++;
	locator_scale = XmCreateScale(mainForm,"locator",al,ac);
	XtManageChild(locator_scale);
	XtAddCallback(locator_scale,XmNvalueChangedCallback,
		      (XtCallbackProc) Generic_scaleCB, 
		      (XtPointer) MOTIF_CHANGE_LOCATOR);
	
	/* Reach the scrollbar child in the scale  */
	ac = 0;
	XtSetArg(al[ac], XtNchildren, &WList); ac++;	
	XtSetArg(al[ac], XtNnumChildren, &Card); ac++;	
	XtGetValues(locator_scale,al,ac);    
	if ((Card!=2)||
	    strcmp(XtName(WList[1]),"Scrollbar"))
	    fprintf(stderr,"PANIC: Scale has be redefined.. may cause bugs\n");
	
 	XtAddEventHandler(WList[1],ButtonPressMask|ButtonReleaseMask,
			  FALSE,Locator_btn,NULL);
    }

    /*****************************/
    /* Control buttons in a form */
    /*****************************/
    
    /* create form for the row of control buttons */
    ac = 0; 
    XtSetArg(al[ac], XmNtopOffset, 10); ac++;
    XtSetArg(al[ac], XmNbottomOffset, 10); ac++;
    XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    XtSetArg(al[ac],XmNtopAttachment,XmATTACH_WIDGET); ac++;
    XtSetArg(al[ac], XmNtopWidget, locator_scale); ac++;
    btnForm = XmCreateForm(mainForm,"btnForm", al, ac);
    XtManageChild(btnForm);
    
    /* Previous Button */
    ac = 0;
    XtSetArg(al[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(al[ac], XmNleftOffset, 10); ac++;
    XtSetArg(al[ac], XmNshadowType, XmSHADOW_OUT); ac++;
    XtSetArg(al[ac], XmNlabelType, XmPIXMAP); ac++;
    XtSetArg(al[ac], XmNlabelPixmap, prevPixmap); ac++;
    XtSetArg(al[ac], XmNhighlightThickness, 2); ac++;
    prevBtn = XmCreatePushButton(btnForm, "previous",al, ac);
    XtManageChild(prevBtn);
    XtAddCallback(prevBtn, XmNactivateCallback,
		  (XtCallbackProc) GenericCB, (XtPointer) MOTIF_PREV);


    /* Backward Button */
    ac = 0;
    XtSetArg(al[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(al[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(al[ac], XmNleftWidget, prevBtn); ac++;
    XtSetArg(al[ac], XmNleftOffset, 10); ac++;
    XtSetArg(al[ac], XmNshadowType, XmSHADOW_OUT); ac++;
    XtSetArg(al[ac], XmNlabelType, XmPIXMAP); ac++;
    XtSetArg(al[ac], XmNlabelPixmap, backPixmap); ac++;
    XtSetArg(al[ac], XmNhighlightThickness, 2); ac++;
    backBtn = XmCreatePushButton(btnForm, "backward",al, ac);
    XtManageChild(backBtn);
    XtAddCallback(backBtn, XmNactivateCallback,
		  (XtCallbackProc) GenericCB, (XtPointer) MOTIF_RWD);

    /* Restart Button */
    ac = 0;
    XtSetArg(al[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(al[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(al[ac], XmNleftWidget, backBtn); ac++;
    XtSetArg(al[ac], XmNleftOffset, 2); ac++;
    XtSetArg(al[ac], XmNshadowType, XmSHADOW_OUT); ac++;
    XtSetArg(al[ac], XmNshadowThickness, 2); ac++;
    XtSetArg(al[ac], XmNlabelType, XmPIXMAP); ac++;
    XtSetArg(al[ac], XmNlabelPixmap, restartPixmap); ac++;
    XtSetArg(al[ac], XmNhighlightThickness, 2); ac++;
    restartBtn = XmCreatePushButton(btnForm,"restartBtn", al, ac);
    XtManageChild(restartBtn);
    XtAddCallback(restartBtn, XmNactivateCallback,
		  (XtCallbackProc) GenericCB, (XtPointer) MOTIF_RESTART);

    /* Quit Button */
    ac = 0;
    XtSetArg(al[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(al[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(al[ac], XmNleftWidget, restartBtn); ac++;
    XtSetArg(al[ac], XmNleftOffset, 2); ac++;
    XtSetArg(al[ac], XmNshadowType, XmSHADOW_OUT); ac++;
    XtSetArg(al[ac], XmNshadowThickness, 2); ac++;
    XtSetArg(al[ac], XmNlabelType, XmPIXMAP); ac++;
    XtSetArg(al[ac], XmNlabelPixmap, quitPixmap); ac++;
    XtSetArg(al[ac], XmNhighlightThickness, 2); ac++;
    quitBtn = XmCreatePushButton(btnForm,"quitBtn", al, ac);
    XtManageChild(quitBtn);
    XtAddCallback(quitBtn, XmNactivateCallback,
		  (XtCallbackProc) GenericCB, (XtPointer) MOTIF_QUIT);

    /* Pause Button */

    ac = 0;
    XtSetArg(al[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(al[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(al[ac], XmNleftWidget, quitBtn); ac++;
    XtSetArg(al[ac], XmNshadowType, XmSHADOW_OUT); ac++;
    XtSetArg(al[ac], XmNshadowThickness, 2); ac++;
    XtSetArg(al[ac], XmNlabelType, XmPIXMAP); ac++;
    XtSetArg(al[ac], XmNlabelPixmap, pausePixmap); ac++;
    XtSetArg(al[ac], XmNhighlightThickness, 2); ac++;
    pauseBtn =  XmCreatePushButton(btnForm,"pauseBtn", al, ac);
    XtManageChild(pauseBtn);
    XtAddCallback(pauseBtn, XmNactivateCallback,
		  (XtCallbackProc) GenericCB,(XtPointer) MOTIF_PAUSE);
 
    /* Forward Button */

    ac = 0;
    XtSetArg(al[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(al[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(al[ac], XmNleftWidget,pauseBtn); ac++;
    XtSetArg(al[ac], XmNshadowType, XmSHADOW_OUT); ac++;
    XtSetArg(al[ac], XmNshadowThickness, 2); ac++;
    XtSetArg(al[ac], XmNlabelType, XmPIXMAP); ac++;
    XtSetArg(al[ac], XmNlabelPixmap, fwdPixmap); ac++;
    XtSetArg(al[ac], XmNhighlightThickness, 2); ac++;
    fwdBtn =  XmCreatePushButton(btnForm,"fwdBtn", al, ac);
    XtManageChild(fwdBtn);
    XtAddCallback(fwdBtn, XmNactivateCallback,
		  (XtCallbackProc) GenericCB, (XtPointer) MOTIF_FWD);
 
    /* Next Button */
    ac = 0;
    XtSetArg(al[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    XtSetArg(al[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(al[ac], XmNleftWidget, fwdBtn); ac++;
    XtSetArg(al[ac], XmNleftOffset, 2); ac++;
    XtSetArg(al[ac], XmNshadowType, XmSHADOW_OUT); ac++;
    XtSetArg(al[ac], XmNshadowThickness, 2); ac++;
    XtSetArg(al[ac], XmNlabelType, XmPIXMAP); ac++;
    XtSetArg(al[ac], XmNlabelPixmap, nextPixmap); ac++;
    XtSetArg(al[ac], XmNhighlightThickness, 2); ac++;
    nextBtn = XmCreatePushButton(btnForm,"nextBtn", al, ac);
    XtManageChild(nextBtn);
    XtAddCallback(nextBtn, XmNactivateCallback,
		  (XtCallbackProc) GenericCB, (XtPointer) MOTIF_NEXT);
    
    /********************/
    /* Volume scale     */
    /********************/
    ac = 0;
    XtSetArg(al[ac], XmNleftOffset, 10); ac++;
    XtSetArg(al[ac], XmNrightOffset, 10); ac++;
    XtSetArg(al[ac], XmNtopOffset, 10); ac++;
    XtSetArg(al[ac], XmNbottomOffset, 10); ac++;
    XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(al[ac], XmNtopWidget, btnForm); ac++;
    XtSetArg(al[ac], XmNleftAttachment,XmATTACH_FORM); ac++;
    XtSetArg(al[ac], XmNbottomAttachment,XmATTACH_FORM); ac++;

    XtSetArg(al[ac], XmNmaximum, MAX_AMPLIFICATION); ac++;
    XtSetArg(al[ac], XmNminimum, 0); ac++;
    XtSetArg(al[ac], XmNshowValue, True); ac++;

    XtSetArg(al[ac], XmNtraversalOn, False); ac++;
    XtSetArg(al[ac], XmNhighlightThickness,0); ac++;
    XtSetArg(al[ac], XmNtitleString,
	     XmStringCreate("VOL",char_set)); ac++;
    volume_scale = XmCreateScale(mainForm,"volscale",al,ac);
    XtManageChild(volume_scale);
    XtAddCallback(volume_scale, XmNvalueChangedCallback,
		  (XtCallbackProc) Generic_scaleCB,
		  (XtPointer) MOTIF_CHANGE_VOLUME);
  
  
    /********************/ 
    /* File list        */
    /********************/ 
    
    ac = 0;
    XtSetArg(al[ac], XmNtopOffset, 10); ac++;
    XtSetArg(al[ac], XmNbottomOffset, 10); ac++;
    XtSetArg(al[ac], XmNleftOffset, 10); ac++;
    XtSetArg(al[ac], XmNrightOffset, 10); ac++;
    XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(al[ac], XmNtopWidget, btnForm ); ac++;
    XtSetArg(al[ac], XmNleftAttachment,XmATTACH_WIDGET); ac++;
    XtSetArg(al[ac], XmNleftWidget, volume_scale); ac++;
    XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    XtSetArg(al[ac], XmNbottomAttachment,XmATTACH_FORM); ac++;

    XtSetArg(al[ac], XmNselectionPolicy ,XmSINGLE_SELECT); ac++;
    XtSetArg(al[ac], XmNscrollBarDisplayPolicy ,XmAS_NEEDED); ac++;
    XtSetArg(al[ac], XmNlistSizePolicy ,XmRESIZE_IF_POSSIBLE); ac++;

    XtSetArg(al[ac], XmNtraversalOn, False); ac++;
    XtSetArg(al[ac], XmNhighlightThickness,0); ac++;
   
    file_list = XmCreateScrolledList(mainForm,"File List",al,ac);
    XtManageChild(file_list);
    XtAddCallback(file_list, XmNsingleSelectionCallback,
		  (XtCallbackProc) File_ListCB,
		  NULL);

    /*
     * Last details on toplevel
     */
    ac=0;
    /*
      XtSetArg(al[ac],XmNwidth,400); ac++;
      XtSetArg(al[ac],XmNheight,800); ac++;
      */
    XtSetArg(al[ac], XmNtitle, "Timidity 6.4f"); ac++;
    XtSetArg(al[ac], XmNiconName, "NONE"); ac++;
    XtSetArg(al[ac], XmNiconPixmap, timidityPixmap); ac++; 
    XtSetValues(toplevel,al,ac);
    
        
  /*******************************************************/ 
  /* Plug the pipe ..... and heeere we go                */
  /*******************************************************/ 

    XtAppAddInput(context,pipe_number, 
		  (XtPointer) XtInputReadMask,handle_input,NULL);
    
    XtRealizeWidget(toplevel);
    XtAppMainLoop(context);
}
