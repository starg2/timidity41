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

    w32g_c.c: written by Daisuke Aoki <dai@y7.net>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <math.h>
#if defined(__CYGWIN32__) || defined(__MINGW32__)
#include <sys/stat.h>
#include <fcntl.h>
#endif

#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "output.h"
#include "controls.h"
#include "miditrace.h"
#include "timer.h"
#include "bitset.h"
#include "arc.h"
#include "aq.h"

#include "w32g.h"
/* FIXME */
//#include <dir.h>
#include <process.h>
#include "w32g_main.h"
#include "w32g_res.h"
#include "w32g_utl.h"
#include "w32g_c.h"

static void InitCrbLoopBuffer(void);


#define RC_CONTROL_CLEAR		1000

#define PLAYERMODE_CONTINUE 0x0001
#define PLAYERMODE_REVERSE				0x0002
#define PLAYERMODE_SINGLE					0x0004
#define PLAYERMODE_TRUE_RANDOM		0x0008
#define PLAYERMODE_HALF_RANDOM		0x0010
#define PLAYERMODE_UNREPEAT				0x0100
static int playermode = PLAYERMODE_CONTINUE;

#define PLAYLISTMODE_LAST_ADD			0x0001
#define PLAYLISTMODE_FIRST_ADD		0x0002
#define PLAYLISTMODE_CLEAR				0x0100
#define PLAYLISTMODE_IGNORE_CUR		0x0200
#define PLAYLISTMODE_AUTOSTART		0x0400
//static int playlistmode = PLAYLISTMODE_LAST_ADD | PLAYLISTMODE_AUTOSTART;
static int playlistmode = PLAYLISTMODE_LAST_ADD;

volatile int player_status = PLAYERSTATUS_STOP;

#if 0 /* not used */
#define HISTORY_PLAYLIST_MAX 1024
static PLAYLIST *old_playlist, *history_playList;
#endif

PLAYLIST *playlist;
PLAYLIST *cur_pl = NULL;
int cur_pl_num = 0;
int playlist_num = 0;

static HANDLE hMutexPlaylist = NULL;
void LockPlaylist(void)
{
	if(hMutexPlaylist==NULL)
  	hMutexPlaylist = CreateMutex(NULL,FALSE,"TiMidityPlaylistMutex");
	WaitForSingleObject(hMutexPlaylist, INFINITE);
}

void UnLockPlaylist(void)
{
	if(hMutexPlaylist==NULL)
  	hMutexPlaylist = CreateMutex(NULL,FALSE,"TiMidityPlaylistMutex");
	ReleaseMutex(hMutexPlaylist);
}

void PlayerPlaylistNum(void)
{
	LockPlaylist();
  cur_pl_num = num_before_playlist(cur_pl);
  playlist_num = num_with_playlist(playlist);
	UnLockPlaylist();
}

PLAYLIST *PlayerPlaylistAddFilename(char *filename)
{
  PLAYLIST *pl;
  char *str;
	LockPlaylist();

  if(playlistmode & PLAYLISTMODE_CLEAR){
    del_all_playlist(playlist);
    playlist = NULL;
  }

  switch(playlistmode & 0x00ff){
  case PLAYLISTMODE_FIRST_ADD:
    pl = first_insert_playlist_filename(playlist,filename);
    playlist = first_playlist(pl);
    break;
  case PLAYLISTMODE_LAST_ADD:
  default:
    pl = last_insert_playlist_filename(playlist,filename);
    playlist = first_playlist(pl);
    break;
  }

  if(!(playlistmode & PLAYLISTMODE_IGNORE_CUR))
    cur_pl = pl;
  str = get_midi_title((char *)pl->filename);
  if(str!=NULL){
    while(*str==' ')
      str++;
    strncpy((char *)pl->title,str,PLAYLIST_DATAMAX);
    pl->title[PLAYLIST_DATAMAX] = '\0';
  }
  PlayerPlaylistNum();
	UnLockPlaylist();
  return pl;
}

static int isDirectory(char *path)
{
#if defined(__CYGWIN32__) || defined(__MINGW32__)
    struct stat st;
    if(stat(path, &st) != -1)
	return S_ISDIR(st.st_mode);
#endif
    return GetFileAttributes(path) == FILE_ATTRIBUTE_DIRECTORY;
}

PLAYLIST *PlayerPlaylistAddExpandFileArchives(char *filename)
{
    int backup_playlistmode = playlistmode;
    PLAYLIST *p, *ret = NULL, *cur_pl_old = cur_pl;
    int nfiles = 1, i;
    char *infiles[1];
    char **files;
    char *dirfile = NULL;

    LockPlaylist();

    if(isDirectory(filename))
    {
	dirfile = safe_malloc(strlen(filename) + 5); /*dir:*/
	strcpy(dirfile, "dir:");
	strcat(dirfile, filename);
	filename = dirfile;
    }

    infiles[0] = filename;
    if(playlistmode & PLAYLISTMODE_CLEAR){
	del_all_playlist(playlist);
	playlist = NULL;
    }
    playlistmode &= ~PLAYLISTMODE_CLEAR;
    files = expand_file_archives(infiles, &nfiles);
    ret = NULL;
    for(i = 0;i < nfiles; i++)
    {
	p = PlayerPlaylistAddFilename(files[i]);
	if(ret == NULL)
	    ret = p;
    }
    if(files != infiles)
	free(files);
    if(dirfile != NULL)
	free(dirfile);
    playlistmode = backup_playlistmode;
    if(playlistmode & PLAYLISTMODE_IGNORE_CUR)
	cur_pl = cur_pl_old;
    else
	cur_pl = ret;
    PlayerPlaylistNum();
    UnLockPlaylist();
    return ret;
}

PLAYLIST *PlayerPlaylistAddFiles
(int nfiles, char **files, int expand_file_archives_flag)
{
  int backup_playlistmode = playlistmode, i;
  PLAYLIST *p, *ret = NULL, *cur_pl_old = cur_pl;
	LockPlaylist();
  if(playlistmode & PLAYLISTMODE_CLEAR){
    del_all_playlist(playlist);
    playlist = NULL;
  }
  playlistmode &= ~PLAYLISTMODE_CLEAR;
  for(i=0;i<nfiles;i++){
    if(expand_file_archives_flag)
      p = PlayerPlaylistAddExpandFileArchives(files[i]);
    else
      p = PlayerPlaylistAddFilename(files[i]);
    if(ret == NULL)
      ret = p;
  }
  playlistmode = backup_playlistmode;
  if(playlistmode & PLAYLISTMODE_IGNORE_CUR)
    cur_pl = cur_pl_old;
  else
    cur_pl = ret;
  PlayerPlaylistNum();
	UnLockPlaylist();
  return ret;
}

PLAYLIST *PlayerPlaylistAddDropfilesEx(HDROP hDrop, int expand_file_archives_flag)
{
  int backup_playlistmode = playlistmode, i;
  int nfiles;
  char buffer[1024];
  PLAYLIST *p, *ret = NULL, *cur_pl_old = cur_pl;
	LockPlaylist();
  if(playlistmode & PLAYLISTMODE_CLEAR){
    del_all_playlist(playlist);
    playlist = NULL;
  }
  playlistmode &= ~PLAYLISTMODE_CLEAR;
  nfiles = DragQueryFile(hDrop,0xffffffffL, NULL, 0);
  for(i=0;i<nfiles;i++){
    DragQueryFile(hDrop,i,buffer,1000);
    if(expand_file_archives_flag){
      p = PlayerPlaylistAddExpandFileArchives(buffer);
    } else
      p = PlayerPlaylistAddFilename(buffer);
    if(ret == NULL)
      ret = p;
  }
  playlistmode = backup_playlistmode;
  if(playlistmode & PLAYLISTMODE_IGNORE_CUR)
    cur_pl = cur_pl_old;
  else
    cur_pl = ret;
#ifdef WIN32GUI_DEBUG
  {
    PLAYLIST *pl = first_playlist(playlist);
    while(pl){
      if(pl == playlist)
	PutsDebugWnd("+");
      if(pl == cur_pl)
	PutsDebugWnd("*");
      PutsDebugWnd(pl->filename);
      PutsDebugWnd("\n");
      pl = pl->next;
    }
  }
#endif
  PlayerPlaylistNum();
	UnLockPlaylist();
  return ret;
}

static int dropfiles_expand_file_archives_flag = 1;
PLAYLIST *PlayerPlaylistAddDropfiles(HDROP hDrop)
{
  return PlayerPlaylistAddDropfilesEx(hDrop, dropfiles_expand_file_archives_flag);
}

#if 0 /* not used */
static void CheckPlaylist(PLAYLIST *p)
{
  return;
}

static void PlayerCheckPlaylist(void)
{
  CheckPlaylist(playlist);
}
#endif

#define XOR(a,b) (((a) && !(b)) || (!(a) && (b)))

static int progress_direction = 1;
static PLAYLIST *Player_1progress_playlist(PLAYLIST *p, int i)
{
  PLAYLIST *res;
  int direction;
  if(p==NULL)
    return p;
  if(i==0){
    direction = progress_direction;
  } else {
    if(XOR((playermode & PLAYERMODE_UNREPEAT),(i < 0))) /* reverse */
      direction = progress_direction = -1;
    else
      direction = progress_direction = 1;
  }
  if(direction < 0){
    res = p->prev;
    if(res == NULL){
      if(playermode & PLAYERMODE_UNREPEAT)
      	res = p;
      else
	res = last_playlist(p);
    }
  } else {
    res = p->next;
    if(res == NULL){
      if(playermode & PLAYERMODE_UNREPEAT)
      	res = p;
      else
	res = first_playlist(p);
    }
  }
  return res;
}

static PLAYLIST *Player_progress_playlist(PLAYLIST *p, int n)
{
  PLAYLIST *res = p;
  int i;
  if(p==NULL)
    return p;
  if(n==0){
    res = Player_1progress_playlist(p, 0);
  } else if(n>0){
    for(i=n;i>0;i--)
      res = Player_1progress_playlist(p, 1);
  } else {
    for(i=n;i<0;i++)
      res = Player_1progress_playlist(p, -1);
  }
  return res;
}

static int IsPlayer1ProgressAblePlaylist(int n)
{
  PLAYLIST *p;
  if(cur_pl == NULL)
    return 0;
  p = Player_1progress_playlist(cur_pl, n);
  if(p==cur_pl)
    return 0;
  else
    return 1;
}

void SetCur_pl(PLAYLIST *pl)
{
	LockPlaylist();
	cur_pl = pl;
	UnLockPlaylist();
}

void SetCur_plNum(int num)
{
	int i;
	PlayerPlaylistNum();
	LockPlaylist();
	if(num >= 0 && num < playlist_num){
		PLAYLIST *pl = playlist;
  	for(i=0;i<num;i++){
			if(pl!=NULL){
				pl = pl->next;
      } else
      	break;
    }
		cur_pl = pl;
  }
	UnLockPlaylist();
	PlayerPlaylistNum();
}

#if 0
static void Player1ProgressCur_pl(int n)
{
	LockPlaylist();
	cur_pl = Player_1progress_playlist(cur_pl, n);
	UnLockPlaylist();
}
#endif 

static void PlayerProgressCur_pl(int n)
{
	LockPlaylist();
	cur_pl = Player_progress_playlist(cur_pl, n);
	UnLockPlaylist();
}

static volatile int AutoPlayNum = 0;

static int TmCanvasResetOK = 0;
static int TmPanelResetOK = 0;
static int MainWndScrollbarProgressResetOK = 0;
static int MainWndScrollbarVolumeResetOK = 0;

static DWORD PlayerLoopSleepTimeMsecDefault = 1000;
static DWORD PlayerLoopSleepTimeMsecFast = 50;
void Player_loop(int init_flag)
{
  PLAYLIST *p;
  DWORD PlayerLoopSleepTimeMsec = PlayerLoopSleepTimeMsecDefault;
  AutoPlayNum = 0;
  if(init_flag){
      PLAYLIST *p = first_playlist(playlist);
      SetCur_pl(p);
      PlayerPlaylistNum();
      if(cur_pl!=NULL)
	  player_status = PLAYERSTATUS_DEMANDPLAY;
      else
	  player_status = PLAYERSTATUS_STOP;
  }
  for(;;){
    volatile int cur_player_status = player_status;
    p = cur_pl;
    switch(cur_player_status){
    case PLAYERSTATUS_PLAYERROREND:
      PlayerLoopSleepTimeMsec = PlayerLoopSleepTimeMsecFast;
#ifdef W32GUI_DEBUG1
      PrintfDebugWnd("PlayerLoop : PLAYERROREND\n");
#endif
      if(IsPlayer1ProgressAblePlaylist(0)){
	PlayerProgressCur_pl(0);
//	p = Player_progress_playlist(p,0);
//	SetCur_pl(p);
	PlayerPlaylistNum();
	player_status = PLAYERSTATUS_DEMANDPLAY;
      } else
	player_status = PLAYERSTATUS_STOP;
      break;
    case PLAYERSTATUS_PLAYEND:
      PlayerLoopSleepTimeMsec = PlayerLoopSleepTimeMsecFast;
#ifdef W32GUI_DEBUG1
      PrintfDebugWnd("PlayerLoop : PLAYEND\n");
#endif
      if(playermode & (PLAYERMODE_CONTINUE | PLAYERMODE_REVERSE)) {
	if(IsPlayer1ProgressAblePlaylist(1)){
		PlayerProgressCur_pl(1);
//	  p = Player_progress_playlist(p,1);
//		SetCur_pl(p);
	  PlayerPlaylistNum();
	  player_status = PLAYERSTATUS_DEMANDPLAY;
	} else if(!IsPlayer1ProgressAblePlaylist(-1)){
	  player_status = PLAYERSTATUS_DEMANDPLAY;
	} else
	  player_status = PLAYERSTATUS_STOP;
      } else
	player_status = PLAYERSTATUS_STOP;
      /*
	 if(playermode & (PLAYERMODE_CONTINUE | PLAYERMODE_REVERSE))
	 p = Player_progress_playlist(p,1);
	 if((p == cur_pl && AutoPlayNum >= 1)&& playermode & PLAYERMODE_SINGLE)
	 player_status = PLAYERSTATUS_STOP;
	 else
	 player_status = PLAYERSTATUS_DEMANDPLAY;
	 */
      break;
    case PLAYERSTATUS_DEMANDPLAY:
      PlayerLoopSleepTimeMsec = PlayerLoopSleepTimeMsecFast;
#ifdef W32GUI_DEBUG1
      PrintfDebugWnd("PlayerLoop : DEMANDPLAY\n");
#endif
      AutoPlayNum++;
      player_status = PLAYERSTATUS_PLAYSTART;
      PlayerOnPlay();
      break;
    case PLAYERSTATUS_QUIT:
#ifdef W32GUI_DEBUG1
      PrintfDebugWnd("PlayerLoop : QUIT\n");
#endif
      PlayerLoopSleepTimeMsec = PlayerLoopSleepTimeMsecDefault;
      AutoPlayNum = 0;
      return;
    case PLAYERSTATUS_STOP:
#ifdef W32GUI_DEBUG1
      PrintfDebugWnd("PlayerLoop : STOP\n");
#endif
      if(!TmCanvasResetOK){
	PanelPartReset();
	TmCanvasReset();
	TmCanvasSet();
	TmCanvasUpdate();
	TmCanvasResetOK = 1;
      }
      if(!TmPanelResetOK){
	PanelPartReset();
	TmPanelPartReset();
	TmPanelSet();
	TmPanelUpdate();
	TmPanelResetOK = 1;
      }
      if(!MainWndScrollbarProgressResetOK){
	MainWndScrollbarProgressUpdate();
	MainWndScrollbarProgressResetOK = 1;
      }
      PlayerLoopSleepTimeMsec = PlayerLoopSleepTimeMsecDefault;
      AutoPlayNum = 0;
      break;
    case PLAYERSTATUS_PLAYSTART:
#ifdef W32GUI_DEBUG1
      PrintfDebugWnd("PlayerLoop : PLAYSTART\n");
#endif
      PlayerLoopSleepTimeMsec = PlayerLoopSleepTimeMsecDefault;
      break;
    case PLAYERSTATUS_PAUSE:
#ifdef W32GUI_DEBUG1
      PrintfDebugWnd("PlayerLoop : PAUSE\n");
#endif
      PlayerLoopSleepTimeMsec = PlayerLoopSleepTimeMsecDefault;
      break;
    case PLAYERSTATUS_PLAY:
#ifdef W32GUI_DEBUG1
      PrintfDebugWnd("PlayerLoop : PLAY\n");
#endif
      if(TmCanvasMode != TMCM_SLEEP){
	TmCanvasSet();
	TmCanvasResetOK = 0;
      }
      TmPanelSet();
      TmPanelResetOK = 0;
      MainWndScrollbarProgressUpdate();
      MainWndScrollbarProgressResetOK = 0;
      MainWndScrollbarVolumeUpdate();
      MainWndScrollbarVolumeResetOK = 0;
      PlayerLoopSleepTimeMsec = PlayerLoopSleepTimeMsecFast;
      break;
    case PLAYERSTATUS_ERROR:
#ifdef W32GUI_DEBUG1
      PrintfDebugWnd("PlayerLoop : ERROR\n");
#endif
      PlayerLoopSleepTimeMsec = PlayerLoopSleepTimeMsecDefault;
      break;
    default:
#ifdef W32GUI_DEBUG1
      PrintfDebugWnd("PlayerLoop : default\n");
#endif
      PlayerLoopSleepTimeMsec = PlayerLoopSleepTimeMsecDefault;
      break;
    }
    Sleep(PlayerLoopSleepTimeMsec);
  }
}













typedef volatile struct MIDIPLAYTHREAD_ {
  HANDLE hThread;
  DWORD dwThreadID;
  int endflag;
} MIDIPLAYTHREAD;

static MIDIPLAYTHREAD mMidiPlayThread[] = {{ 0, 0, 1 },{ 0, 0, 1 }};
static volatile int CurMidiPlayThreadNum = 0;

static MIDIPLAYTHREAD *GetCurMidiPlayThread(void)
{
  return (MIDIPLAYTHREAD *)&(mMidiPlayThread[CurMidiPlayThreadNum]);
}

static MIDIPLAYTHREAD *GetNotCurMidiPlayThread(void)
{
  if(CurMidiPlayThreadNum == 0)
    return (MIDIPLAYTHREAD *)&(mMidiPlayThread[1]);
  else
    return (MIDIPLAYTHREAD *)&(mMidiPlayThread[0]);
}

static void ExchangeMidiPlayThread(void)
{
  if(CurMidiPlayThreadNum == 0)
    CurMidiPlayThreadNum = 1;
  else
    CurMidiPlayThreadNum = 0;
}

void MidiPlayThread(void *none);

void PlayerOnStopEx(int rc)
{
  MIDIPLAYTHREAD *mpt;
  if(rc == RC_QUIT || rc == RC_LOAD_FILE || rc == RC_REALLY_PREVIOUS || rc == RC_NEXT)
    ;
  else
    rc = RC_LOAD_FILE;
  PutCrbLoopBuffer(rc, 0);
  mpt = GetCurMidiPlayThread();
  if(mpt->endflag)
    player_status = PLAYERSTATUS_STOP;
}

void PlayerOnStop(void)
{
  //	PlayerOnStopEx(RC_QUIT);
  PlayerOnStopEx(RC_LOAD_FILE);
}

void PlayerOnPlayEx(int rc)
{
  MIDIPLAYTHREAD *mpt = GetNotCurMidiPlayThread();
#if !(defined(__CYGWIN32__) || defined(__MINGW32__))
  SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES),NULL,TRUE};
#endif

  if(!mpt->endflag)
    return;
  mpt = GetCurMidiPlayThread();
  if(!mpt->endflag)
    PlayerOnStopEx(rc);
  mpt = GetNotCurMidiPlayThread();
	UpdateListWnd();

#if defined(__CYGWIN32__) || defined(__MINGW32__)
  mpt->hThread =
      CreateThread(NULL, 0,
		   (LPTHREAD_START_ROUTINE)MidiPlayThread,
		   NULL, 0, (LPDWORD)&mpt->dwThreadID);
#else
  mpt->hThread = (HANDLE) _beginthreadNT
    (MidiPlayThread,0,(void *)0,&sa,CREATE_SUSPENDED,&(mpt->dwThreadID));
#endif

  mpt->endflag = 0;
  ResumeThread(mpt->hThread);
}

void PlayerOnKill(void)
{
  MIDIPLAYTHREAD *mpt;
  mpt = GetNotCurMidiPlayThread();
  if(!mpt->endflag)
    TerminateThread(mpt->hThread,0);
	mpt = GetCurMidiPlayThread();
  if(!mpt->endflag)
    TerminateThread(mpt->hThread,0);
}

void PlayerCanselPause(void);
void PlayerOnPlay(void)
{
  MIDIPLAYTHREAD *mpt1;
  MIDIPLAYTHREAD *mpt2;
  if(player_status==PLAYERSTATUS_PLAY){
    PlayerCanselPause();
  } else {
    mpt1 = GetCurMidiPlayThread();
    mpt2 = GetNotCurMidiPlayThread();
    if(mpt1->endflag || mpt2->endflag)
      PlayerOnPlayEx(RC_LOAD_FILE);
  }
}

void PlayerOnQuit(void)
{
  DWORD dwRes;
  MIDIPLAYTHREAD *mpt = GetCurMidiPlayThread();
  if(!mpt->endflag)
    if(GetExitCodeThread(mpt->hThread,&dwRes)==STILL_ACTIVE)
      ;
  PlayerOnStop();
}

void PlayerOnNextPlay(void)
{
  if(cur_pl){
//		PLAYLIST *p = Player_progress_playlist(cur_pl,1);;
//		SetCur_pl(p);
		PlayerProgressCur_pl(1);
    PlayerPlaylistNum();
  }
  PlayerOnPlayEx(RC_NEXT);
}

void PlayerOnPrevPlay(void)
{
  if(cur_pl){
//		PLAYLIST *p = Player_progress_playlist(cur_pl,-1);;
//		SetCur_pl(p);
		PlayerProgressCur_pl(-1);
    PlayerPlaylistNum();
  }
  PlayerOnPlayEx(RC_REALLY_PREVIOUS);
}

static int PlayerOnPauseStatus = 0;
int PlayerOnPause(void)
{
  if(player_status==PLAYERSTATUS_PLAY){
    if(PlayerOnPauseStatus)
      PlayerOnPauseStatus = 0;
    else
      PlayerOnPauseStatus = 1;
    PutCrbLoopBuffer(RC_TOGGLE_PAUSE, 0);
  } else
    PlayerOnPauseStatus = 0;
  return PlayerOnPauseStatus;
}

void PlayerCanselPause(void)
{
  if(player_status==PLAYERSTATUS_PLAY){
    if(PlayerOnPauseStatus){
      PlayerOnPauseStatus = 0;
      PutCrbLoopBuffer(RC_TOGGLE_PAUSE, 0);
    }
  }
}

void PlayerOnPlayLoadFile(void)
{
  PlayerOnPlayEx(RC_LOAD_FILE);
}

static int MainPlayThreadSleepTimeMsec = 200;
void MidiPlayThread(void *none)
{
  int rc;
  MIDIPLAYTHREAD *mpt = GetNotCurMidiPlayThread();
#ifdef W32GUI_DEBUG
  PrintfDebugWnd("MidiPlayThread : Start.\n",rc);
#endif
 mpt->endflag = 0;
  if(cur_pl==NULL){
    player_status = PLAYERSTATUS_STOP;
    mpt->endflag = 1;
    return;
  }
  mpt = GetCurMidiPlayThread();
  while(!mpt->endflag){
    PlayerOnStop();
    Sleep(MainPlayThreadSleepTimeMsec);
  }
  Sleep(MainPlayThreadSleepTimeMsec);
  player_status = PLAYERSTATUS_PLAYSTART;
  ExchangeMidiPlayThread();
  InitCrbLoopBuffer();

  if(target_play_mode)
      playmidi_output_changed(2);

  if(play_mode->open_output() < 0){
    ctl->cmsg(CMSG_FATAL, VERB_NORMAL,
	      "Couldn't open %s (`%c')",
	      play_mode->id_name, play_mode->id_character);
    player_status = PLAYERSTATUS_STOP;
  } else {
      extern void timidity_init_aq_buff(void);
      aq_setup();
      timidity_init_aq_buff();

    player_status = PLAYERSTATUS_PLAY;
    PlayerCanselPause();
    PanelReset();
    TmCanvasReset();
    TmCanvasSet();
    TmPanelReset();
    TmPanelSet();
    rc = play_midi_file((char *)cur_pl->filename);
    play_mode->close_output();
#ifdef W32GUI_DEBUG
    PrintfDebugWnd("MidiPlayThread : rc %d \n",rc);
#endif
    switch(rc){
    case RC_ERROR:
      	player_status = PLAYERSTATUS_PLAYERROREND;
	break;
      case RC_LOAD_FILE:
      case RC_QUIT:
      	player_status = PLAYERSTATUS_STOP;
	break;
      case RC_NEXT:
      case RC_REALLY_PREVIOUS:
	player_status = PLAYERSTATUS_STOP;
	break;
      default:
	player_status = PLAYERSTATUS_PLAYEND;
	break;
      }
  }
  mpt = GetCurMidiPlayThread();
  mpt->endflag = 1;
#ifdef W32GUI_DEBUG
  PrintfDebugWnd("MidiPlayThread : End.\n");
#endif
#if defined(__CYGWIN32__) || defined(__MINGW32__)
	ExitThread(0);
#else
  _endthread();
#endif
}



































#define CRBLOOPBUFFERMAX 64
static CRBLOOPBUFFER CrbLoopBuffer[CRBLOOPBUFFERMAX];
static int CrbLoopBuffer_first = 0;
static int CrbLoopBuffer_last = 0;

static void InitCrbLoopBuffer(void)
{
  CrbLoopBuffer_first = 0;
  CrbLoopBuffer_last = 0;
}

static int LoopNum(int n, int digit)
{
  int ret = n;
  for(;;){
    if(ret >= digit) ret -= digit;
    else if(ret < 0) ret += digit;
    else break;
  }
  return ret;
}

static int IsRangeLoopNum(int n, int min, int max, int digit)
{
  int n_=LoopNum(n,digit),min_=LoopNum(min,digit),max_=LoopNum(max,digit),i;
  if(min_==max_)
    return (min_==n_)?1:0;
  if(n_==min_ || n_==max_)
    return 1;
  for(i=n_;;i=LoopNum(i+1,digit)){
    if(i==max_) break;
    if(i==min_) return 0;
  }
  for(i=n_;;i=LoopNum(i-1,digit)){
    if(i==min_) break;
    if(i==max_) return 0;
  }
  return 1;
}

// move n from start between min & max
static int MoveRangeLoopNum(int start, int n, int min, int max, int digit)
{
  int start_=LoopNum(start,digit);
  int min_=LoopNum(min,digit),max_=LoopNum(max,digit);
  int i;
  if(!IsRangeLoopNum(start,min,max,digit))
    return -1;
  if(n==0){
    return start_;
  } else if(n>0){
    for(i=start_;n--;i=LoopNum(i+1,digit))
      if(i== max_)
	break;
  } else if(n<0){
    for(i=start_;n++;i=LoopNum(i-1,digit))
      if(i== min_)
	break;
  }
  return i;
}

static int CrbLoopNum(int n)
{
  return LoopNum(n,CRBLOOPBUFFERMAX);
}

#if 0 /* not used */
static int CrbIsRangeLoopNum(int n, int min, int max)
{
  return IsRangeLoopNum(n, min, max, CRBLOOPBUFFERMAX);
}
#endif

static int CrbMoveRangeLoopNum(int start, int n, int min, int max)
{
  return MoveRangeLoopNum(start,n,min,max,CRBLOOPBUFFERMAX);
}

static CRBLOOPBUFFER *ScanCrbLoopBuffer(int n)
{
  // n is
  // -1 : last, -2 : before last, ... , ? : first
  //	1 : first, 2 : next, ... , ? : last
  int start,i;
  if(n==0)
    return NULL;
  if(n>0){
    start = CrbLoopBuffer_first;
    n--;
  } else {
    start = CrbLoopBuffer_last;
    n++;
  }
  i = CrbMoveRangeLoopNum(start,n,CrbLoopBuffer_first,CrbLoopBuffer_last);
  return (CRBLOOPBUFFER *)&(CrbLoopBuffer[i]);
}

CRBLOOPBUFFER *GetCrbLoopBuffer(void)
{
  CRBLOOPBUFFER *ret;
  if(CrbLoopBuffer_first==CrbLoopBuffer_last)
    return NULL;
  ret = ScanCrbLoopBuffer(2);
  CrbLoopBuffer_first = CrbLoopNum(CrbLoopBuffer_first+1);
  return ret;
}

static void CheckCrbLoopBuffer(void);

void PutCrbLoopBuffer(int cmd, int32 val)
{
  CRBLOOPBUFFER *b;
  if(CrbLoopNum(CrbLoopBuffer_last+1)==CrbLoopBuffer_first){
    CrbLoopBuffer_last = CrbLoopNum(CrbLoopBuffer_last+1);
    CrbLoopBuffer_first = CrbLoopNum(CrbLoopBuffer_first+1);
  } else
    CrbLoopBuffer_last = CrbLoopNum(CrbLoopBuffer_last+1);
  b = ScanCrbLoopBuffer(-1);
  b->cmd = cmd;
  b->val = val;
  CheckCrbLoopBuffer();
}

static int CrbLoopBufferLength(void)
{
  int i = CrbLoopBuffer_last - CrbLoopBuffer_first;
  if(i>=0)
    return i;
  else
    return CrbLoopNum(i + CRBLOOPBUFFERMAX);
}

static void CheckCrbLoopBuffer(void)
{
  CRBLOOPBUFFER *b;
  int i;
  int len = CrbLoopBufferLength();
  for(i = -1;i >= -len;i--){
    b = ScanCrbLoopBuffer(i);
    if(b->cmd == RC_QUIT){
      CrbLoopBuffer_last = CrbLoopNum(CrbLoopBuffer_last+i+1);
      CrbLoopBuffer_first = CrbLoopNum(CrbLoopBuffer_last-1);
      return;
    }
  }
  for(i = -1;i >= -len;i--){
    b = ScanCrbLoopBuffer(i);
    if(b->cmd == RC_CONTROL_CLEAR){
      CrbLoopBuffer_first = CrbLoopNum(CrbLoopBuffer_last+i+1);
      return;
    }
    if(b->cmd == RC_NEXT || b->cmd == RC_PREVIOUS || b->cmd == RC_TOGGLE_PAUSE
       || b->cmd == RC_CONTINUE || b->cmd == RC_REALLY_PREVIOUS || b->cmd == RC_RESTART
       || b->cmd == RC_RELOAD || b->cmd == RC_LOAD_FILE){
      CrbLoopBuffer_last = CrbLoopNum(CrbLoopBuffer_last+i+1);
      CrbLoopBuffer_first = CrbLoopNum(CrbLoopBuffer_last-1);
      return;
    }
  }
}







//****************************************************************************/
// Control funcitons

static int ctl_open(int using_stdin, int using_stdout);
static void ctl_close(void);
static void ctl_pass_playing_list(int number_of_files, char *list_of_files[]);
static void ctl_event(CtlEvent *e);
static int ctl_read(int32 *valp);
static void ctl_current_time(int secs, int nvoices);
static int cmsg(int type, int verbosity_level, char *fmt, ...);

#define ctl w32gui_control_mode

ControlMode ctl=
{
    "Win32 GUI interface", 'w',
    1,1,0,
    0,
    ctl_open,
    ctl_close,
    ctl_pass_playing_list,
    ctl_read,
    cmsg,
    ctl_event
};

PanelInfo *Panel = NULL;
/*
volatile static int PanelFlag = 0;
#define PANEL_BEGIN() {while(PanelFlag); PanelFlag = 1;}
#define PANEL_END() {PanelFlag = 0;}
*/
static HANDLE hPanelSemaphore = NULL;
#define PANEL_BEGIN() WaitForSingleObject(hPanelSemaphore, INFINITE)
#define PANEL_END() ReleaseSemaphore(hPanelSemaphore, 1, NULL)

static void ctl_refresh(void)
{
}

static void ctl_total_time(int tt)
{
  int32 centisecs = tt/(play_mode->rate/100);
	PANEL_BEGIN();
  Panel->total_time = centisecs;
  Panel->total_time_h = centisecs/100/60/60;
  centisecs %= 100*60*60;
  Panel->total_time_m = centisecs/100/60;
  centisecs %= 100*60;
  Panel->total_time_s = centisecs/100;
  centisecs %= 100;
  Panel->total_time_ss = centisecs;
	PANEL_END();
  ctl_current_time(0, 0);
}

static void ctl_master_volume(int mv)
{
	PANEL_BEGIN();
  Panel->master_volume = mv;
	PANEL_END();
}

static void ctl_file_name(char *name)
{
  strncpy((char *)Panel->filename,name,MAXPATH);
	PANEL_BEGIN();
  Panel->filename[MAXPATH] = '\0';
	PANEL_END();
}

#if 0 /* not used */
static void pseudo_ctl_voices(int x)
{
  int v = 0, i;
  for(i = 0; i < upper_voices; i++)
    if(voice[i].status != VOICE_FREE)
      v++;
	PANEL_BEGIN();
  Panel->cur_voices = v;
  Panel->voices = voices;
  Panel->upper_voices = upper_voices;
	PANEL_END();
}
#endif

static void ctl_current_time(int secs, int nvoices)
{
  int32 centisecs = secs * 100;

  PANEL_BEGIN();
  Panel->cur_time = centisecs;
  Panel->cur_time_h = centisecs/100/60/60;
  centisecs %= 100*60*60;
  Panel->cur_time_m = centisecs/100/60;
  centisecs %= 100*60;
  Panel->cur_time_s = centisecs/100;
  centisecs %= 100;
  Panel->cur_time_ss = centisecs;
  Panel->cur_voices = nvoices;
  PANEL_END();
//  pseudo_ctl_voices(0);
}

static void ctl_channel_note(int ch, int note, int vel)
{
    PANEL_BEGIN();
    if (vel == 0) {
	if (note == Panel->cnote[ch])
	    Panel->v_flags[ch] = FLAG_NOTE_OFF;
	Panel->cvel[ch] = 0;
    } else if (vel > Panel->cvel[ch]) {
	Panel->cvel[ch] = vel;
	Panel->cnote[ch] = note;
	Panel->ctotal[ch] = vel * Panel->channel[ch].volume *
	    Panel->channel[ch].expression / (127*127);
	Panel->v_flags[ch] = FLAG_NOTE_ON;
    }
    PANEL_END();
}

static void ctl_note(int status, int ch, int note, int vel)
{
    if(!ctl.trace_playing)
	return;
    if(ch < 0 || ch >= MAX_W32G_MIDI_CHANNELS)
	return;

    if(status != VOICE_ON)
	vel = 0;

    PANEL_BEGIN();
    switch(status)
    {
      case VOICE_DIE:
      case VOICE_FREE:
      case VOICE_OFF:
      case VOICE_SUSTAINED: {
	  int32 n = note;
	  int32 i = 0;
	  if(n<0) n = 0;
	  if(n>127) n = 127;
	  while(n >= 32)
	  {
	      n -= 32;
	      i++;
	  }
	  Panel->xnote[ch][i] &= ~(((int32)1) << n);
        }
        break;
      case VOICE_ON: {
      	int n = note;
	int i = 0;
	if(n<0) n = 0;
        if(n>127) n = 127;
        while(n >= 32)
	{
	    n -= 32;
	    i++;
        }
        Panel->xnote[ch][i] |= ((int32)1) << n;
      }
      break;
    }
    PANEL_END();
    ctl_channel_note(ch, note, vel);
}

static void ctl_program(int ch, int val)
{
    if(ch >= MAX_W32G_MIDI_CHANNELS)
	return;
    if (!ctl.trace_playing)
	return;
    if (ch < 0 || ch >= MAX_W32G_MIDI_CHANNELS) return;
    if(channel[ch].special_sample)
	val = channel[ch].special_sample;
    else
	val += progbase;

    PANEL_BEGIN();
    Panel->channel[ch].program = val;
    Panel->c_flags[ch] |= FLAG_PROG;
    PANEL_END();
}

static void ctl_volume(int ch, int val)
{
	if(ch >= MAX_W32G_MIDI_CHANNELS)
		return;
	if (!ctl.trace_playing)
		return;

	PANEL_BEGIN();
	Panel->channel[ch].volume = val;
	PANEL_END();
	ctl_channel_note(ch, Panel->cnote[ch], Panel->cvel[ch]);
}

static void ctl_expression(int ch, int val)
{
  if(ch >= MAX_W32G_MIDI_CHANNELS)
    return;
  if (!ctl.trace_playing)
    return;

	PANEL_BEGIN();
  Panel->channel[ch].expression = val;
	PANEL_END();
  ctl_channel_note(ch, Panel->cnote[ch], Panel->cvel[ch]);
}

static void ctl_panning(int ch, int val)
{
  if(ch >= MAX_W32G_MIDI_CHANNELS)
    return;
  if (!ctl.trace_playing)
    return;

	PANEL_BEGIN();
  Panel->channel[ch].panning = val;
  Panel->c_flags[ch] |= FLAG_PAN;
	PANEL_END();
}

static void ctl_sustain(int ch, int val)
{
  if(ch >= MAX_W32G_MIDI_CHANNELS)
    return;
  if (!ctl.trace_playing)
    return;

	PANEL_BEGIN();
  Panel->channel[ch].sustain = val;
  Panel->c_flags[ch] |= FLAG_SUST;
	PANEL_END();
}

/*ARGSUSED*/
static void ctl_pitch_bend(int channel, int val)
{
  /*
     if(ch >= MAX_W32G_MIDI_CHANNELS)
     return;
     if (!ctl.trace_playing)
     return;
     semaphore_P(semid);
     Panel->channel[ch].pitch_bend = val;
     Panel->c_flags[ch] |= FLAG_BENDT;
     semaphore_V(semid);
     */
}

static void ctl_lyric(uint16 lyricid)
{
    default_ctl_lyric(lyricid);
}


static void ctl_reset(void)
{
  int i;

  InitCrbLoopBuffer();

  if (!ctl.trace_playing)
    return;

  while(Panel->wait_reset)
    VOLATILE_TOUCH(Panel->wait_reset);
  
  Panel->wait_reset = 1;
  for (i = 0; i < MAX_W32G_MIDI_CHANNELS; i++) {
    if(ISDRUMCHANNEL(i))
      ctl_program(i, channel[i].bank);
    else
      ctl_program(i, channel[i].program);
    ctl_volume(i, channel[i].volume);
    ctl_expression(i, channel[i].expression);
    ctl_panning(i, channel[i].panning);
    ctl_sustain(i, channel[i].sustain);
    if(channel[i].pitchbend == 0x2000 &&
       channel[i].modulation_wheel > 0)
      ctl_pitch_bend(i, -1);
    else
      ctl_pitch_bend(i, channel[i].pitchbend);
    ctl_channel_note(i, Panel->cnote[i], 0);
  }
  Panel->wait_reset = 0;
}

void panel_init(void);

static int ctl_open(int using_stdin, int using_stdout)
{
  InitCrbLoopBuffer();
  panel_init();
  ctl.opened=1;
  return 0;
}

static void ctl_close(void)
{
  InitCrbLoopBuffer();
  if (ctl.opened) {
    ctl.opened=0;
  }
	if(hPanelSemaphore!=NULL)
		CloseHandle(hPanelSemaphore);
  return;
}

static int ctl_read(int32 *valp)
{
  CRBLOOPBUFFER *b;
  if((b = GetCrbLoopBuffer()) == NULL)
    return RC_NONE;
  *valp = b->val;
  return b->cmd;
}

static int cmsg(int type, int verbosity_level, char *fmt, ...)
{
  char buffer[10240];
  va_list ap;
  va_start(ap, fmt);
  vsprintf(buffer,fmt,ap);
  va_end(ap);

//printf("##cmsg<%s>\n", buffer); fflush(stdout);

#ifdef W32GUI_DEBUG3
  PutsDebugWnd(buffer);
  PutsDebugWnd("\n");
#endif

  if ((type==CMSG_TEXT || type==CMSG_INFO || type==CMSG_WARNING) &&
      ctl.verbosity<verbosity_level)
    return 0;
  PutsConsoleWnd(buffer);
  PutsConsoleWnd("\n");
  return 0;
}

static void ctl_pass_playing_list(int number_of_files, char *list_of_files[])
{
  PlayerPlaylistAddFiles(number_of_files, list_of_files, 1);
  Player_loop(1);
  return;
}

void PanelFullReset(void)
{
  int i,j;
	PANEL_BEGIN();
  Panel->reset_panel = 0;
  Panel->multi_part = 0;
  Panel->wait_reset = 0;
  Panel->cur_time = 0;
  Panel->cur_time_h = 0;
  Panel->cur_time_m = 0;
  Panel->cur_time_s = 0;
  Panel->cur_time_ss = 0;
  for(i=0;i<MAX_W32G_MIDI_CHANNELS;i++){
    Panel->v_flags[i] = 0;
    Panel->cnote[i] = 0;
    Panel->cvel[i] = 0;
    Panel->ctotal[i] = 0;
    Panel->c_flags[i] = 0;
		for(j=0;j<4;j++)
  		Panel->xnote[i][j] = 0;
  }
  Panel->titlename[0] = '\0';
  Panel->filename[0] = '\0';
  Panel->titlename_setflag = 0;
  Panel->filename_setflag = 0;
  Panel->cur_voices = 0;
  Panel->voices = 0;
  Panel->upper_voices = 0;
  Panel->master_volume = 0;
	PANEL_END();
}

void PanelReset(void)
{
  int i,j;
	PANEL_BEGIN();
  Panel->reset_panel = 0;
  Panel->multi_part = 0;
  Panel->wait_reset = 0;
  Panel->cur_time = 0;
  Panel->cur_time_h = 0;
  Panel->cur_time_m = 0;
  Panel->cur_time_s = 0;
  Panel->cur_time_ss = 0;
  for(i=0;i<MAX_W32G_MIDI_CHANNELS;i++){
    Panel->v_flags[i] = 0;
    Panel->cnote[i] = 0;
    Panel->cvel[i] = 0;
    Panel->ctotal[i] = 0;
    Panel->c_flags[i] = 0;
		for(j=0;j<4;j++)
  		Panel->xnote[i][j] = 0;
  }
  Panel->titlename[0] = '\0';
  Panel->filename[0] = '\0';
  Panel->titlename_setflag = 0;
  Panel->filename_setflag = 0;
  Panel->cur_voices = 0;
  //  Panel->voices = 0;
  Panel->upper_voices = 0;
  //  Panel->master_volume = 0;
	PANEL_END();
}

void PanelPartReset(void)
{
  int i,j;
	PANEL_BEGIN();
  Panel->reset_panel = 0;
  Panel->multi_part = 0;
  Panel->wait_reset = 0;
  Panel->cur_time = 0;
  Panel->cur_time_h = 0;
  Panel->cur_time_m = 0;
  Panel->cur_time_s = 0;
  Panel->cur_time_ss = 0;
  for(i=0;i<MAX_W32G_MIDI_CHANNELS;i++){
    Panel->v_flags[i] = 0;
    Panel->cnote[i] = 0;
    Panel->cvel[i] = 0;
    Panel->ctotal[i] = 0;
    Panel->c_flags[i] = 0;
		for(j=0;j<4;j++)
  		Panel->xnote[i][j] = 0;
  }
  Panel->cur_voices = 0;
  //	Panel->voices = 0;
  Panel->upper_voices = 0;
  //	Panel->master_volume = 0;
	PANEL_END();
}

void panel_init(void)
{
	if(hPanelSemaphore==NULL)
		hPanelSemaphore = CreateSemaphore(NULL, 1, 1, "TiMidityPanelSemaphore");
	PANEL_BEGIN();
	if(Panel==NULL){
  	Panel = (PanelInfo *)safe_malloc(sizeof(PanelInfo));
  	memset((void *)Panel,0,sizeof(PanelInfo));
	}
	PANEL_END();
  PanelFullReset();
}

static void ctl_event(CtlEvent *e)
{
    switch(e->type)
    {
      case CTLE_NOW_LOADING:
	ctl_file_name((char *)e->v1);
	break;
      case CTLE_LOADING_DONE:
	break;
      case CTLE_PLAY_START:
	ctl_total_time((int)e->v1);
	break;
      case CTLE_PLAY_END:
	break;
      case CTLE_TEMPO:
	break;
      case CTLE_METRONOME:
//	update_indicator();
	break;
      case CTLE_CURRENT_TIME:
	ctl_current_time((int)e->v1, (int)e->v2);
//	display_aq_ratio();
	break;
      case CTLE_NOTE:
	ctl_note((int)e->v1, (int)e->v2, (int)e->v3, (int)e->v4);
	break;
      case CTLE_MASTER_VOLUME:
	ctl_master_volume((int)e->v1);
	break;
      case CTLE_PROGRAM:
	ctl_program((int)e->v1, (int)e->v2);
	break;
      case CTLE_VOLUME:
	ctl_volume((int)e->v1, (int)e->v2);
	break;
      case CTLE_EXPRESSION:
	ctl_expression((int)e->v1, (int)e->v2);
	break;
      case CTLE_PANNING:
	ctl_panning((int)e->v1, (int)e->v2);
	break;
      case CTLE_SUSTAIN:
	ctl_sustain((int)e->v1, (int)e->v2);
	break;
      case CTLE_PITCH_BEND:
	ctl_pitch_bend((int)e->v1, (int)e->v2);
	break;
      case CTLE_MOD_WHEEL:
	ctl_pitch_bend((int)e->v1, (int)e->v2);
	break;
      case CTLE_CHORUS_EFFECT:
	break;
      case CTLE_REVERB_EFFECT:
	break;
      case CTLE_LYRIC:
	ctl_lyric((int)e->v1);
	break;
      case CTLE_REFRESH:
	ctl_refresh();
	break;
      case CTLE_RESET:
	ctl_reset();
	break;
      case CTLE_SPEANA:
	break;
    }
}
