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

	Macintosh interface for TiMidity
	by T.Nogami	<t-nogami@happy.email.ne.jp>
	
    mac_main.c
*/

#include	<Sound.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<Threads.h>
#include	<unix.mac.h>

#include	"timidity.h"
#include	"common.h"
#include	"instrum.h"
#include	"playmidi.h"
#include	"readmidi.h"
#include	"output.h"
#include	"controls.h"
#include	"tables.h"
#include	"wrd.h"
#ifdef SUPPORT_SOUNDSPEC
#include "soundspec.h"
#endif /* SUPPORT_SOUNDSPEC */

#include	"mac_main.h"
#include	"mac_c.h"
#include	"mac_util.h"

#define MAIN_INTERFACE  /* non-static */
MAIN_INTERFACE void	timidity_start_initialize(void);
MAIN_INTERFACE int	timidity_pre_load_configuration(void);
MAIN_INTERFACE int	timidity_post_load_configuration(void);
MAIN_INTERFACE void	timidity_init_player(void);
MAIN_INTERFACE int	timidity_play_main(int nfiles, char **files);
MAIN_INTERFACE int	read_config_file(char *name, int self);

char *timidity_version = TIMID_VERSION;
Boolean	skin_f_repeat, gQuit, gBusy, gShuffle;
int		mac_rc, skin_state=WAITING, mac_n_files, nPlaying;
long	gStartTick;
double	gSilentSec;
MidiFile	fileList[LISTSIZE];
int		evil_level;
int		do_initial_filling;

/*****************************************/

/* ************************************************** */

static pascal OSErr myHandleOAPP(AppleEvent* /*theAppleEvent*/, AppleEvent* /*reply*/, long /*handlerRefCon*/)
{
	return noErr;
}

static pascal OSErr myHandleODOC(AppleEvent *theAppleEvent, AppleEvent* /*reply*/, long /*handlerRefCon*/)
{
	int				oldListEnd;
	AEDescList		docList;
	AEKeyword		keyword;
	DescType		returnedType;
	FSSpec			theFSSpec;
	Size			actualSize;
	long			itemsInList;
	short			index;
	OSErr			err;
	KeyMap			keys;

	GetKeys(keys);
	if( keys[1] & 0x00000004 )
		if( mac_SetPlayOption()!=noErr ) return noErr; /* user cancel*/
		
	if ( (err=AEGetParamDesc(theAppleEvent, keyDirectObject, typeAEList, &docList))!=0 )
		return(err);

	if ( (err=AECountItems(&docList, &itemsInList))!=0 )
		return(err);

	oldListEnd=mac_n_files;
	for (index = 1; index <= itemsInList; index++) {
		if ( (err=AEGetNthPtr(&docList, index, typeFSS, &keyword,
								 &returnedType, (Ptr) &theFSSpec, sizeof(FSSpec), &actualSize))!=0 )
			return(err);
		mac_add_fsspec(&theFSSpec);
		//if( isArchiveFile(&theFSSpec) ){
		//	mac_add_archive_file(&theFSSpec);
		//}else  {
		//	mac_add_midi_file(&theFSSpec);
		//}
	}
	
	if( gShuffle ) ShuffleList( oldListEnd, mac_n_files);
	return noErr;
}

static pascal OSErr myHandleQUIT(AppleEvent* /*theEvent*/, AppleEvent* /*reply*/, long /*refCon*/)
{
	gQuit=true;
	return noErr;	/* don't ExitToShell() here, must return noErr */
}
/*******************************************************/

static void InitMenuBar()
{
	MenuHandle	synth;

	SetMenuBar(GetNewMBar(128));
	AppendResMenu(GetMenu(128),'DRVR');

	synth= GetMenu(mSynth);
	InsertMenu(synth, -1); //setup submenu
	CheckItem(synth, iTiMidity & 0x0000FFFF, 1);

	DrawMenuBar();
}

static void mac_init()
{
	long	gestaltResponse;

	InitGraf( &qd.thePort );
	InitFonts();
	FlushEvents( everyEvent,0 );
	InitWindows();
	InitMenus();
	TEInit();
	InitDialogs( 0 );
	InitCursor();
	MaxApplZone();
	ReadDateTime( (unsigned long*)&qd.randSeed );
	
	if( !((Gestalt(gestaltSystemVersion, &gestaltResponse)==noErr )
			&& gestaltResponse>=0x0750))
				mac_ErrorExit("\pThis program can run on System 7.5 or later!");
	
	if ((Gestalt(gestaltDragMgrAttr, &gestaltResponse) == noErr)
		&& (gestaltResponse & (1 << gestaltDragMgrPresent)))
			gHasDragMgr=true;
	else gHasDragMgr=false;
	
#if (__MC68K__ && __MC68881__)
	if (!((Gestalt(gestaltFPUType, &gestaltResponse) == noErr)
		&& (gestaltResponse!=gestaltNoFPU )))
			mac_ErrorExit("\pSorry, No FPU.");
#endif
	
	AEInstallEventHandler(kCoreEventClass, kAEOpenApplication,
					NewAEEventHandlerProc(myHandleOAPP), 0, false);
	AEInstallEventHandler(kCoreEventClass, kAEOpenDocuments,
					NewAEEventHandlerProc(myHandleODOC), 0, false);
	AEInstallEventHandler(kCoreEventClass, kAEQuitApplication,
					NewAEEventHandlerProc(myHandleQUIT), 0, false);	
	InitMenuBar();
}

int  main()
{
	EventRecord	event;
	int32	output_rate=DEFAULT_RATE;
	int		err;
	
	mac_init();	
	
	nPlaying=mac_n_files=0; skin_state=WAITING;
	
	mac_DefaultOption();
	mac_GetPreference();
	
	
	timidity_start_initialize();
    if((err = timidity_pre_load_configuration()) != 0)
		return err;
    err += timidity_post_load_configuration();
    if( err )
    {
		//ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		//	  "Try %s -h for help", program_name);
		return 1; /* problems with command line */
    }
    timidity_init_player();
	ctl->open(0, 0);
	play_mode->open_output();
	init_load_soundfont();
	wrdt=wrdt_list[0];  //dirty!!
	if(wrdt->open("d"))
	{	//some errors
		fprintf(stderr, "Couldn't open WRD Tracer: %s (`%c')" NLS,
			wrdt->name, wrdt->id);
		play_mode->close_output();
		ctl->close();
		return 1;
	}
	//amplification=20;

	gQuit=false;
	while(!gQuit)
	{
		WaitNextEvent(everyEvent, &event, 1,0);
		mac_HandleEvent(&event);
	}	
	DoQuit();
	return 0;
}

static pascal void* StartPlay(void *)
{
	//OSErr	err;
	SndCommand		theCmd;
	int		rc;
		
	//for( i=0; i<nfiles; i++)
	{
		skin_state=PLAYING; gBusy=true; DrawButton();
		gCursorIsWatch=true; SetCursor(*GetCursor(watchCursor));
		DisableItem(GetMenu(mFile), iSaveAs & 0x0000FFFF);
		DisableItem(GetMenu(mFile), iPref & 0x0000FFFF);
			rc=play_midi_file( fileList[nPlaying].filename );
			gStartTick=0;
			theCmd.cmd=waitCmd;
			theCmd.param1=2000*gSilentSec;
			SndDoCommand(gSndCannel, &theCmd, 1);
		EnableItem(GetMenu(mFile), iSaveAs & 0x0000FFFF);
		EnableItem(GetMenu(mFile), iPref & 0x0000FFFF);
		skin_state=(rc==RC_QUIT? STOP:WAITING);
		
		if( rc==RC_REALLY_PREVIOUS && nPlaying>0)
				nPlaying--;
		else if(rc==RC_RESTART)	/*noting*/;
		else if(rc==RC_QUIT) /*noting*/;
		else if(rc==RC_LOAD_FILE) /*noting*/;
		else if(rc==RC_NEXT) nPlaying++;
		else if(!skin_f_repeat) nPlaying++;
	}
	DrawButton();
	if( gCursorIsWatch ){
		InitCursor();	gCursorIsWatch=false;
	}
	return 0;
}

static void HandleNullEvent()
{	
	YieldToAnyThread();	
	if( Button() ){DoVolume();}
	if( skin_state==WAITING && mac_rc==RC_PREVIOUS && nPlaying>0)
		{nPlaying--; mac_rc=0; }
	if( skin_state==WAITING && nPlaying<mac_n_files )
	{
		NewThread(kCooperativeThread, StartPlay,
		         0, 0, kCreateIfNeeded, 0, 0);
	}
}

void mac_HandleEvent(EventRecord *event)
{	
	switch(event->what)
	{
	case nullEvent:
		HandleNullEvent();
		break;
	case mouseDown:
		HandleMouseDown(event);
		break;
	case keyDown:
		if( event->modifiers&cmdKey )
		{
			mac_HandleMenuSelect(MenuKey(event->message&charCodeMask));
			HiliteMenu(0);
			break;
		}else{ //no command key
			HandleSpecKeydownEvent( event->message, event->modifiers);
		}
		break;
	case updateEvt:
		BeginUpdate((WindowRef)event->message);
		DoUpdate((WindowRef)event->message);
		EndUpdate((WindowRef)event->message);
		break;
	case kHighLevelEvent:
		AEProcessAppleEvent(event);
		break;
	default:
		break;
	}
	
}

void mac_HandleControl()
{
	SndCommand		theCmd;

	switch(mac_rc)
	{
	case RC_QUIT:
			if( skin_state==PAUSE )
			{
				play_mode->purge_output();
				theCmd.cmd=resumeCmd; SndDoImmediate(gSndCannel, &theCmd);
				skin_state=PLAYING;
			}
		break;/*and wait ctl_read*/
	case RC_NEXT:
		if( skin_state==STOP )
			{ skin_state=WAITING; mac_rc=0; if( nPlaying<mac_n_files ) nPlaying++; }
		else if( skin_state==WAITING )
			{ skin_state=WAITING; mac_rc=0; if( nPlaying<mac_n_files ) nPlaying++; }
		else if( skin_state==PAUSE )
			{ mac_rc=RC_CONTINUE; mac_HandleControl(); mac_rc=RC_NEXT;}
		break;
	case RC_PREVIOUS:
		if( skin_state==PAUSE ){ mac_rc=RC_CONTINUE; mac_HandleControl(); mac_rc=RC_PREVIOUS;}
		else if( skin_state==STOP ) {skin_state=WAITING; if( nPlaying>0 ) nPlaying--; mac_rc=0; }
		else if( skin_state==WAITING  && nPlaying>0)
			{nPlaying--; mac_rc=0; }
		break;			/*and wait ctl_read*/
	case RC_FORWARD:
		if( skin_state==STOP ){ skin_state=WAITING; mac_rc=0; }
		else if( skin_state==PAUSE ){ mac_rc=RC_CONTINUE; mac_HandleControl(); }
		break;			/*and wait ctl_read*/
	case RC_TOGGLE_PAUSE:
		if( skin_state==PAUSE ){ mac_rc=RC_CONTINUE; mac_HandleControl(); }
		else if( skin_state==PLAYING ){ mac_rc=RC_PAUSE; mac_HandleControl(); }
		break;
	case RC_RESTART:
		break;			/*and wait ctl_read*/
	case RC_LOAD_FILE:
		if( skin_state==PAUSE )
			{ mac_rc=RC_CONTINUE; mac_HandleControl(); mac_rc=RC_LOAD_FILE;}
		skin_state=WAITING;
		break;
	case RC_PAUSE:
		if( skin_state==PLAYING ){theCmd.cmd=pauseCmd; SndDoImmediate(gSndCannel, &theCmd); skin_state=PAUSE;}
		break;
	case RC_CONTINUE:
		if( skin_state==PAUSE ){ theCmd.cmd=resumeCmd; SndDoImmediate(gSndCannel, &theCmd); skin_state=PLAYING;}
		else if( skin_state==STOP ){ skin_state=WAITING; mac_rc=0; }
		break;
	case RC_REALLY_PREVIOUS:
		break;			/*and wait ctl_read*/
	}
	DrawButton();
}


/*******************************************/
#pragma mark -

extern PlayMode wave_play_mode;
extern PlayMode aiff_play_mode;
extern PlayMode mac_quicktime_play_mode;
extern PlayMode mac_play_mode;

static pascal void* ConvertToAiffFile(void*)
{
	OSErr	err;
	int		tmp;
	char	newfile[256];
	StandardFileReply	stdReply;
	Str255	fullPath;

#ifdef __MWERKS__
	_fcreator='TVOD';     //Movie Player
	_ftype='AIFF';
#endif
	StandardGetFile(0, 0, 0, &stdReply);
	if (stdReply.sfGood)
	{
		play_mode=&aiff_play_mode;
			if( play_mode->open_output()==-1 ) return 0;
			tmp=skin_state;
			skin_state=PLAYING;
			err=GetFullPath( &stdReply.sfFile, fullPath);
			if( err==noErr )
				play_midi_file( p2cstr(fullPath) );
			skin_state=tmp;
			play_mode->close_output();
		play_mode=&mac_play_mode;
		p2cstrcpy(newfile, stdReply.sfFile.name); strcat(newfile,".aiff");
		rename("output.aiff", newfile);
	}
	DrawButton();
	return 0;
}

static void mac_AboutBox()
{
	short		item;
	DialogRef	dialog, theDialog;
	EventRecord	event;
	
	dialog=GetNewDialog(200,0,(WindowRef)-1);
	if( dialog==0 ) return;
	SetDialogDefaultItem(dialog, 1);
	ParamText(TIMID_VERSION_PASCAL, "\p", "\p", "\p");
	
	ShowWindow(dialog);
	for(;;){
		WaitNextEvent(everyEvent, &event, 10, 0);
		if( ! IsDialogEvent(&event) )	continue;
		if( StdFilterProc(dialog, &event, &item) ) /**/;
			else DialogSelect(&event, &theDialog, &item);
		if( theDialog!=dialog ) continue;
		if( item==1 ) break;
		YieldToAnyThread();
	}
	DisposeDialog(dialog);
}

static void CloseFrontWindow()
{
	WindowRef window;
	MacWindow   *macwin;
	
	window=FrontWindow();
	if( ! window ) return;
	macwin= (MacWindow*)GetWRefCon(window);
	if( ! macwin ) return;
	macwin->goaway(macwin);
}

void mac_HandleMenuSelect(long select)
{
	StandardFileReply	stdReply;
	Str255	str;

	switch(select)
	{
	case iAbout:
		mac_AboutBox();
		return;
	case iOpen:
		StandardGetFile(0, -1, 0, &stdReply);
		if (stdReply.sfGood)
		{
			mac_add_fsspec(&stdReply.sfFile);
		}
		return;
	case iClose:
		CloseFrontWindow();
		return;

	case iLogWindow:	SHOW_WINDOW(mac_LogWindow);		return;
	case iListWindow:	SHOW_WINDOW(mac_ListWindow);	return;
	case iWrdWindow:	SHOW_WINDOW(mac_WrdWindow);		return;
	case iDocWindow:	SHOW_WINDOW(mac_DocWindow);		return;
	case iSpecWindow:
#ifdef SUPPORT_SOUNDSPEC
		if(!mac_SpecWindow.show)
		{
			mac_SpecWindow.show=true;
		    open_soundspec();
  			soundspec_update_wave(NULL, 0);
		}
		SelectWindow(mac_SpecWindow.ref);
#endif /* SUPPORT_SOUNDSPEC */
		return;
	case iTraceWindow:	SHOW_WINDOW(mac_TraceWindow);	return;
	case iSkinWindow: SHOW_WINDOW(mac_SkinWindow); return;
	case iSaveAs:
		NewThread(kCooperativeThread, ConvertToAiffFile,
	         0, 0, kCreateIfNeeded, 0, 0);
		return;
		
	case iPref:	mac_SetPlayOption(); return;
	case iQuit:	DoQuit();			 return;
	
	//Play menu
	case iPlay:	SKIN_ACTION_PLAY(); break;
	case iStop:	SKIN_ACTION_STOP();	break;
	case iPause:	SKIN_ACTION_PAUSE(); break;
	case iPrev:	SKIN_ACTION_PREV(); break;
	case iNext:	SKIN_ACTION_NEXT(); break;
	
		//Synth menu
	case iTiMidity:{
		MenuHandle menu=GetMenu(mSynth);
		CheckItem(menu, iTiMidity, 1);
		CheckItem(menu, iQuickTime, 0);
		play_mode=&mac_play_mode;
		}
		return;
	case iQuickTime:{
		MenuHandle menu=GetMenu(mSynth);
		
		if( ! mac_quicktime_play_mode.fd ){ //not opened yet
			if( mac_quicktime_play_mode.open_output()!=0 ){
				SysBeep(0);
				return;	//can't open device
			}
		}
		CheckItem(menu, iTiMidity & 0x0000FFFF, 0);
		CheckItem(menu, iQuickTime & 0x0000FFFF, 1);
		play_mode=&mac_quicktime_play_mode;
		}
		return;
	}
	
	if( (select>>16)==mApple )
	{
		GetMenuItemText(GetMenu(mApple), select&0x0000FFFF, str);
		OpenDeskAcc(str);
	}
}

void DoQuit()
{
	play_mode->close_output();
	ctl->close();
	mac_SetPreference();
	ExitToShell();
}

void mac_ErrorExit(Str255 s)
{
	StopAlertMessage(s);
	SndDisposeChannel(gSndCannel, 1); gSndCannel=0;
	ExitToShell();
}

/* ****************************** */
#pragma mark -

void	ShuffleList(int start, int end)
{
	int 	i, newFile;
	MidiFile	tmpItem;
	
	for( i=start; i<end; i++){ /*Shuffle target is start..end-1*/
		newFile= i+(end-i-1)*((32767+Random())/65535.0);		
					/*Random() returns between -32767 to 32767*/
		tmpItem=fileList[newFile];	/*Swapping*/
		fileList[newFile]=fileList[i];
		fileList[i]=tmpItem;
			//update list window
		change_ListRow( i, &fileList[i]);
	}
}

static int isMidiFilename(const char *fn)
{
	char	*p;
	
	p= strrchr(fn, '.'); if( p==0 ) return 0;
	if( strcasecmp(p, ".mid")==0 ||
		strcasecmp(p, ".rcp")==0 ||
		strcasecmp(p, ".r36")==0 ||
		strcasecmp(p, ".g18")==0 ||
		strcasecmp(p, ".g36")==0 ||
		strcasecmp(p, ".kar")==0 ||
		strcasecmp(p, ".mod")==0 ||
		strcasecmp(p, ".gz")==0
		){
		return 1;
	}else{
		return 0;
	}
}

static int isArchiveFilename(const char *fn)
{
	char	*p;
	
	p= strrchr(fn, '.'); if( p==0 ) return 0;
	if( strcasecmp(p, ".lzh")==0 ||
		strcasecmp(p, ".zip")==0
		){
		return 1;
	}else{
		return 0;
	}
}

static int isBMPFilename(const char *fn)
{
	char	*p;
	
	p= strrchr(fn, '.'); if( p==0 ) return 0;
	if( strcasecmp(p, ".BMP")==0 ){
		return 1;
	}else{
		return 0;
	}
}

/*int isMidiFile(const FSSpec *spec)
{
	int len;

	//err= FSpGetFInfo(spec, &fndrInfo);
	len=spec->name[0];	//strlen
	if( memcmp( &spec->name[len-3], ".mid", 4)==0 ||
		memcmp( &spec->name[len-3], ".MID", 4)==0 ||
		memcmp( &spec->name[len-3], ".rcp", 4)==0 ||
		memcmp( &spec->name[len-3], ".RCP", 4)==0 ||
		memcmp( &spec->name[len-3], ".R36", 4)==0 ||
		memcmp( &spec->name[len-3], ".r36", 4)==0 ||
		memcmp( &spec->name[len-3], ".G18", 4)==0 ||		
		memcmp( &spec->name[len-3], ".g18", 4)==0 ||
		memcmp( &spec->name[len-3], ".G36", 4)==0 ||
		memcmp( &spec->name[len-3], ".g36", 4)==0 ||
		memcmp( &spec->name[len-2], ".gz",  3)==0 ||
		memcmp( &spec->name[len-3], ".mod", 4)==0 ||
		memcmp( &spec->name[len-3], ".hqx", 4)==0 ||
		memcmp( &spec->name[len-4], ".mime", 5)==0
		){
		return 1;
	}
	else{
		return 0;
	}
}*/

/*int isArchiveFile(const FSSpec *spec)
{
	int len;

	len=spec->name[0];	//strlen
	if( memcmp( &spec->name[len-3], ".lzh", 4)==0 ||
		memcmp( &spec->name[len-3], ".LZH", 4)==0 ||
		memcmp( &spec->name[len-3], ".zip", 4)==0 ||
		memcmp( &spec->name[len-3], ".ZIP", 4)==0
		//memcmp( &spec->name[len-3], ".hqx", 4)==0 ||
		//memcmp( &spec->name[len-4], ".mime", 5)==0		
		){
		return 1;
	}
	else{
		return 0;
	}
}*/

static void AddFolderFSSpec2PlayList(const FSSpec *spec)
{
	CInfoPBRec	cipb;
	Str32		theString;
	OSErr		err = noErr;	
	
	BlockMoveData(spec->name, theString, spec->name[0]+1);
	
	cipb.hFileInfo.ioCompletion 	= nil;
	cipb.hFileInfo.ioFDirIndex  	= 0;	/* this mean 'use ioNamePtr' */
	cipb.hFileInfo.ioDirID			= spec->parID;
	cipb.hFileInfo.ioVRefNum 		= spec->vRefNum;
	cipb.hFileInfo.ioNamePtr 		= (StringPtr)theString;
	
	if( noErr==PBGetCatInfoSync(&cipb) )
		AddFolder2PlayList(spec->vRefNum, cipb.hFileInfo.ioDirID);
}

static void mac_add_midi_file(const char *fullpath)
{
	if( mac_n_files<LISTSIZE )	//not full
	{
		fileList[mac_n_files].filename= (char*)safe_malloc(strlen(fullpath)+1);
		strcpy(fileList[mac_n_files].filename, fullpath);
		if( skin_state==STOP ){ skin_state=WAITING; nPlaying=mac_n_files; }
		add_ListWin(&fileList[mac_n_files]);
		mac_n_files++;
	}
}

static void mac_add_bmp_file(const char *fullpath)
{
	mac_SkinWindow.message(MW_SKIN_LOAD_BMP, (long)fullpath);
}

static void mac_add_nonarchive_file(const char *fullpath)
{
	if(isMidiFilename(fullpath)){
		mac_add_midi_file(fullpath);
	}else if(isBMPFilename(fullpath)){
		mac_add_bmp_file(fullpath);
	}else if( strtailcasecmp(fullpath, "viscolor.txt")==0 ){
		read_viscolor(fullpath);
	}
}

static void mac_add_archive_file(const char *fullpath)
{	
	if( mac_n_files<LISTSIZE )	/*not full*/
	{
		const char  *arc_files[1];
		char		**new_files;
		int			nfiles=1,i;
		
		//fileList[mac_n_files].filename= (char*)safe_malloc(fullpath[0]+1);
		arc_files[0]= fullpath;
		
		new_files= expand_file_archives(arc_files, &nfiles);
		
		for( i=0; i<nfiles; i++ ){
			mac_add_nonarchive_file(new_files[i]);			
		}
		if( arc_files!=new_files ) free(new_files);
	}
}

static void mac_add_file(const char *fullpath)
{
	if( isArchiveFilename(fullpath) ){
		mac_add_archive_file(fullpath);
	}else{
		mac_add_nonarchive_file(fullpath);
	}
}

void mac_add_fsspec( FSSpec *spec )
{
	OSErr	err;
	FInfo	fndrInfo;
	Boolean	targetIsFolder,wasAliased;
	
	ResolveAliasFile(spec, true, &targetIsFolder, &wasAliased);
	
	err= FSpGetFInfo(spec, &fndrInfo);
	
	if( err ){
		if( err==fnfErr ){
			AddFolderFSSpec2PlayList(spec); // spec may be directory
			return;
		}else return;
	}
	
	//no error
	{ //spec is file, not folder or disk
		Str255	fullpath;
		
		GetFullPath(spec, fullpath);
		p2cstr(fullpath);
		
		if( fndrInfo.fdType=='Midi' ){
			mac_add_midi_file((char*)fullpath);
		}else{
			mac_add_file((char*)fullpath);
		}
	}
}

void	AddHFS2PlayList(HFSFlavor* item)
{
	if( item->fileType=='fold' || item->fileType=='disk')
		AddFolderFSSpec2PlayList(&item->fileSpec);
	//else if(item->fileType=='Midi'){}
	else mac_add_fsspec(&item->fileSpec);
}

#define kDirFlag (1<<4)

OSErr AddFolder2PlayList( short vRefNum, long dirID)
{
	CInfoPBRec	cipb;
	Str32		theString;
	StringPtr	saveString;
	short		saveIndex;
	OSErr		err = noErr;	
	
	// initialize ourselves
	cipb.hFileInfo.ioCompletion 	= nil;
	cipb.hFileInfo.ioFDirIndex  	= 1;
	cipb.hFileInfo.ioVRefNum 		= vRefNum;
	cipb.hFileInfo.ioNamePtr 		= (StringPtr)theString;
	
	while (err == noErr) {
		
		// always reset directory id
		cipb.hFileInfo.ioDirID = dirID;

		// get the info for the next catalog item
		err = PBGetCatInfoSync(&cipb);
		if (err) goto exit;
		
		// increment the count
		cipb.hFileInfo.ioFDirIndex++;
		
		// if we are a directory, recurse
		if (cipb.hFileInfo.ioFlAttrib & kDirFlag) {
			
			// save before recursing
			saveIndex = cipb.hFileInfo.ioFDirIndex;
			saveString = cipb.hFileInfo.ioNamePtr;

			// recurse within the directory
			err = AddFolder2PlayList( vRefNum, cipb.hFileInfo.ioDirID );

			// restore after recursion
			cipb.hFileInfo.ioFDirIndex = saveIndex;
			cipb.hFileInfo.ioNamePtr = saveString;
			
		} else {	// we are a file
		
			// call our scanProc if there is one specified
			/*if (sp != nil)
				err = (*sp)(cipbp, refCon);*/
			FSSpec	spec;
			if(noErr==FSMakeFSSpec(vRefNum, dirID,
						cipb.hFileInfo.ioNamePtr, &spec))
				mac_add_fsspec(&spec);
				
		}
	}
	
exit:
	
	// ignore fnfErr and afpAccessDenied errors since fnfErr is what we
	// get when there are no more files left to scan, and afpAccessDenied
	// can just ŽÒhappenŽÓ depending on what we are scanning (ie: network
	// volumes)
	if ((err == fnfErr) || (err == afpAccessDenied)) return noErr;
	else return err;
}

/*******************************************/
#pragma mark -


