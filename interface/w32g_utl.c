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

    w32g_utl.c: written by Daisuke Aoki <dai@y7.net>
*/


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>
#include <stdio.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <mem.h>
#include <ctype.h>
//#include <dir.h>

#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "output.h"
#include "controls.h"
#include "tables.h"
#include "miditrace.h"
#include "reverb.h"
#ifdef SUPPORT_SOUNDSPEC
#include "soundspec.h"
#endif /* SUPPORT_SOUNDSPEC */
#include "recache.h"
#include "arc.h"
#include "strtab.h"
#include "wrd.h"
#include "mid.defs"

#include "w32g.h"
//#include <windowsx.h>
#include "w32g_main.h"
// #include "w32g_res.h"
#include "w32g_utl.h"
#include "w32g_c.h"


extern int effect_lr_mode;
extern int effect_lr_delay_msec;


//***************************************************************************/
// Play list utility

PLAYLIST *
new_playlist(void)
{
  PLAYLIST *playlist = (PLAYLIST *)safe_malloc(sizeof(PLAYLIST));
  playlist->prev = NULL;
  playlist->next = NULL;
  playlist->filename[0] = '\0';
  playlist->title[0] = '\0';
  playlist->opt = 0;
  return playlist;
}

PLAYLIST *
del_playlist(PLAYLIST *playlist)
{
  PLAYLIST *ret;
  if(!playlist)
    return NULL;
  if(playlist->prev && playlist->next){
    (playlist->prev)->next = (playlist->next)->prev;
    ret = playlist->prev;
  } else if(playlist->prev && !playlist->next){
    (playlist->prev)->next = NULL;
    ret = playlist->prev;
  } else if(!playlist->prev && playlist->next){
    (playlist->next)->prev = NULL;
    ret = playlist->next;
  } else
    ret = NULL;
  free((void *)playlist);
  return ret;
}

void
del_all_playlist(PLAYLIST *playlist)
{
  PLAYLIST *res;
  while((res = del_playlist(playlist))!=NULL)
    playlist = res;
}

PLAYLIST *
first_playlist(PLAYLIST *playlist)
{
  if(!playlist)
    return NULL;
  while(playlist->prev)
    playlist = playlist->prev;
  return playlist;
}

PLAYLIST *
last_playlist(PLAYLIST *playlist)
{
  if(!playlist)
    return NULL;
  while(playlist->next)
    playlist = playlist->next;
  return playlist;
}

PLAYLIST *
insert_playlist(PLAYLIST *playlist, PLAYLIST *prev, PLAYLIST *next)
{
  PLAYLIST *first, *last;
  if(playlist==NULL)
    return NULL;
  first = first_playlist(playlist);
  last = last_playlist(playlist);
  first->prev = prev;
  last->next = next;
  if(prev)
    prev->next = first;
  if(next)
    next->prev = last;
  return playlist;
}

/* insert p1 into p2 */
void
prev_insert_playlist(PLAYLIST *p1, PLAYLIST *p2)
{
  PLAYLIST *p3;
  if(p2)
    p3 = p2->prev;
  else
    p3 = NULL;
  insert_playlist(p1,p3,p2);
}

void
next_insert_playlist(PLAYLIST *p1, PLAYLIST *p2)
{
  PLAYLIST *p3;
  if(p2)
    p3 = p2->next;
  else
    p3 = NULL;
  insert_playlist(p1,p2,p3);
}

void
first_insert_playlist(PLAYLIST *p1, PLAYLIST *p2)
{
  PLAYLIST *p = first_playlist(p2);
  insert_playlist(p1,NULL,p);
}

void
last_insert_playlist(PLAYLIST *p1, PLAYLIST *p2)
{
  PLAYLIST *p = last_playlist(p2);
  insert_playlist(p1,p,NULL);
}

PLAYLIST *
last_insert_playlist_filename(PLAYLIST *playlist, char *filename)
{
  PLAYLIST *p = new_playlist();
  strncpy((char *)p->filename,filename,PLAYLIST_DATAMAX-1);
  p->filename[PLAYLIST_DATAMAX] = '\0';
  last_insert_playlist(p,playlist);
  return p;
}

PLAYLIST *
first_insert_playlist_filename(PLAYLIST *playlist, char *filename)
{
  PLAYLIST *p = new_playlist();
  strncpy((char *)p->filename,filename,PLAYLIST_DATAMAX-1);
  p->filename[PLAYLIST_DATAMAX] = '\0';
  first_insert_playlist(p,playlist);
  return p;
}

PLAYLIST *
dup_a_playlist_merely(PLAYLIST *playlist)
{
  PLAYLIST *p = new_playlist();
  memcpy((void *)p,(void *)playlist,sizeof(PLAYLIST));
  return p;
}

PLAYLIST *
dup_a_playlist(PLAYLIST *playlist)
{
  PLAYLIST *p = dup_a_playlist(playlist);
  p->prev = NULL;
  p->next = NULL;
  return p;
}

PLAYLIST *
dup_playlist(PLAYLIST *playlist)
{
  PLAYLIST *src = first_playlist(playlist);
  PLAYLIST *new = NULL, *ret = NULL, *p;
  while(src){
    p = dup_a_playlist(src);
    last_insert_playlist(new,p);
    new = p;
    if(!ret)
      ret = new;
    src = src->next;
  }
  return ret;
}

void
reverse_playlist(PLAYLIST *playlist)
{
  PLAYLIST *p = first_playlist(playlist);
  PLAYLIST *next = p;
  while(next){
    next = p->next;
    p->next = p->prev;
    p->prev = next;
  }
}

int
num_before_playlist(PLAYLIST *pl)
{
  PLAYLIST *p = pl;
  int i = 0;
  while(p!=NULL){
    p=p->prev;
    i++;
  }
  return i;
}

int
num_after_playlist(PLAYLIST *pl)
{
  PLAYLIST *p = pl;
  int i = 0;
  while(p!=NULL){
    p=p->next;
    i++;
  }
  return i;
}

int
num_with_playlist(PLAYLIST *pl)
{
  PLAYLIST *p = first_playlist(pl);
  return num_after_playlist(p);
}









// **************************************************************************
// misc utility

int
isinteger(char *str)
{
  int digit_flag;
  while(*str == ' ' && *str == '\t')
    str++;
  if(*str == '+' && *str == '-')
    str++;
  while(isdigit(*str)){
    digit_flag = 1;
    str++;
  }
  while(*str == ' ' && *str == '\t')
    str++;
  if(*str || !digit_flag)
    return 0;
  return 1;
}

#define LotationBufferNumMAX 256
int LotationBufferNum = 0;
char LotationBuffer[LotationBufferNumMAX][LotationBufferMAX];
char *
GetLotationBuffer(void)
{
  char *buffer = LotationBuffer[LotationBufferNum];
  if(LotationBufferNum<LotationBufferNumMAX-1)
    LotationBufferNum++;
  else
    LotationBufferNum = 0;
  return buffer;
}












//*****************************************************************************/
// Windows Utility


//*****************************************************************************/
// ini

// INI file
CHAR *INI_INVALID = "INVALID PARAMETER";
CHAR *INI_SEC_PLAYER = "PLAYER";
CHAR *INI_SEC_TIMIDITY = "TIMIDITY";
#define INI_MAXLEN 1024

int
IniGetKeyInt32(char *section, char *key,int32 *n)
{
  CHAR buffer[INI_MAXLEN];
  GetPrivateProfileString
    (section,key,INI_INVALID,buffer,INI_MAXLEN-1,IniFile);
  if(strcasecmp(buffer,INI_INVALID)){
    *n =atol(buffer);
    return 0;
  } else
    return 1;
}

int
IniGetKeyInt32Array(char *section, char *key, int32 *n, int arraysize)
{
  int i;
  int ret = 0;
  CHAR buffer[INI_MAXLEN];
  char keybuffer[INI_MAXLEN];
  for(i=0;i<arraysize;i++){
    sprintf(keybuffer,"%s%d",key,i);
    GetPrivateProfileString
      (section,keybuffer,INI_INVALID,buffer,INI_MAXLEN-1,IniFile);
    if(strcasecmp(buffer,INI_INVALID))
      *n =atol(buffer);
    else
      ret++;
  }
  return ret;
}

int
IniGetKeyInt(char *section, char *key, int *n)
{
  return IniGetKeyInt32(section, key, (int32 *)n);
}

int
IniGetKeyFloat(char *section, char *key, FLOAT_T *n)
{
    CHAR buffer[INI_MAXLEN];
    GetPrivateProfileString(section, key, INI_INVALID, buffer,
			    INI_MAXLEN-1, IniFile);
    if(strcasecmp(buffer, INI_INVALID))
    {
	*n = (FLOAT_T)atof(buffer);
	return 0;
    }
    else
	return 1;
}


int
IniGetKeyChar(char *section, char *key, char *c)
{
  char buffer[64];
  if(IniGetKeyStringN(section,key,buffer,60))
    return 1;
  else {
    *c = buffer[0];
    return 0;
  }
}

int
IniGetKeyIntArray(char *section, char *key, int *n, int arraysize)
{
  return IniGetKeyInt32Array(section,key,(int32 *)n, arraysize);
}

int
IniGetKeyString(char *section, char *key,char *str)
{
  CHAR buffer[INI_MAXLEN];
  GetPrivateProfileString
    (section,key,INI_INVALID,buffer,INI_MAXLEN-1,IniFile);
  if(strcasecmp(buffer,INI_INVALID)){
    strcpy(str,buffer);
    return 0;
  } else
    return 1;
}

int
IniGetKeyStringN(char *section, char *key,char *str, int size)
{
  CHAR buffer[INI_MAXLEN];
  GetPrivateProfileString
    (section,key,INI_INVALID,buffer,size-1,IniFile);
  if(strcasecmp(buffer,INI_INVALID)){
    strcpy(str,buffer);
    return 0;
  }	else
    return 1;
}

int
IniPutKeyInt32(char *section, char *key,int32 *n)
{
  CHAR buffer[INI_MAXLEN];
  sprintf(buffer,"%ld",*n);
  WritePrivateProfileString
    (section,key,buffer,IniFile);
  return 0;
}

int
IniPutKeyInt32Array(char *section, char *key, int32 *n, int arraysize)
{
  int i;
  CHAR buffer[INI_MAXLEN];
  CHAR keybuffer[INI_MAXLEN];
  for(i=0;i<arraysize;i++){
    sprintf(buffer,"%ld",n[i]);
    sprintf(keybuffer,"%s%d",key,i);
    WritePrivateProfileString(section,keybuffer,buffer,IniFile);
  }
  return 0;
}

int
IniPutKeyInt(char *section, char *key, int *n)
{
  return IniPutKeyInt32(section, key, (int32 *)n);
}

int
IniPutKeyChar(char *section, char *key, char *c)
{
  char buffer[64];
  sprintf(buffer,"%c",*c);
  return IniPutKeyStringN(section,key,buffer,60);
}

int
IniPutKeyIntArray(char *section, char *key, int *n, int arraysize)
{
  return IniPutKeyInt32Array(section,key,(int32 *)n, arraysize);
}

int
IniPutKeyString(char *section, char *key, char *str)
{
  WritePrivateProfileString(section,key,str,IniFile);
  return 0;
}

int
IniPutKeyStringN(char *section, char *key, char *str, int size)
{
  WritePrivateProfileString(section,key,str,IniFile);
  return 0;
}

int
IniPutKeyFloat(char *section, char *key,FLOAT_T n)
{
    CHAR buffer[INI_MAXLEN];
    sprintf(buffer,"%f", (double)n);
    WritePrivateProfileString(section, key, buffer, IniFile);
    return 0;
}

// LoadIniFile() , SaveIniFile()

//#include "w32g_ini.c"

// ***************************************************************************
// Setting

int
SetFlag(int flag)
{
  return flag?1:0;
}

long
SetValue(int32 value, int32 min, int32 max)
{
  int32 v = value;
  if(v < min) v = min;
  else if( v > max) v = max;
  return v;
}

void
ApplySettingPlayer(SETTING_PLAYER *sp)
{
  InitMinimizeFlag = SetFlag(sp->InitMinimizeFlag);
  DebugWndStartFlag = SetFlag(sp->DebugWndStartFlag);
  ConsoleWndStartFlag = SetFlag(sp->ConsoleWndStartFlag);
  ListWndStartFlag = SetFlag(sp->ListWndStartFlag);
  TracerWndStartFlag = SetFlag(sp->TracerWndStartFlag);
  DocWndStartFlag = SetFlag(sp->DocWndStartFlag);
  WrdWndStartFlag = SetFlag(sp->WrdWndStartFlag);
  DebugWndFlag = SetFlag(sp->DebugWndFlag);
  ConsoleWndFlag = SetFlag(sp->ConsoleWndFlag);
  ListWndFlag = SetFlag(sp->ListWndFlag);
  TracerWndFlag = SetFlag(sp->TracerWndFlag);
  DocWndFlag = SetFlag(sp->DocWndFlag);
  WrdWndFlag = SetFlag(sp->WrdWndFlag);
  SoundSpecWndFlag = SetFlag(sp->SoundSpecWndFlag);
  SubWindowMax = SetValue(sp->SubWindowMax,1,10);
  strncpy(ConfigFile,sp->ConfigFile,MAXPATH + 31);
  ConfigFile[MAXPATH + 31] = '\0';
  strncpy(PlaylistFile,sp->PlaylistFile,MAXPATH + 31);
  PlaylistFile[MAXPATH + 31] = '\0';
  strncpy(PlaylistHistoryFile,sp->PlaylistHistoryFile,MAXPATH + 31);
  PlaylistHistoryFile[MAXPATH + 31] = '\0';
  strncpy(MidiFileOpenDir,sp->MidiFileOpenDir,MAXPATH + 31);
  MidiFileOpenDir[MAXPATH + 31] = '\0';
  strncpy(ConfigFileOpenDir,sp->ConfigFileOpenDir,MAXPATH + 31);
  ConfigFileOpenDir[MAXPATH + 31] = '\0';
  strncpy(PlaylistFileOpenDir,sp->PlaylistFileOpenDir,MAXPATH + 31);
  PlaylistFileOpenDir[MAXPATH + 31] = '\0';
  ProcessPriority = sp->ProcessPriority;
  PlayerThreadPriority = sp->PlayerThreadPriority;
  MidiPlayerThreadPriority = sp->MidiPlayerThreadPriority;
  MainThreadPriority = sp->MainThreadPriority;
  TracerThreadPriority = sp->TracerThreadPriority;
  WrdThreadPriority = sp->WrdThreadPriority;
  WrdGraphicFlag = SetFlag(sp->WrdGraphicFlag);
  TraceGraphicFlag = SetFlag(sp->TraceGraphicFlag);
  // fonts ...
  SystemFontSize = sp->SystemFontSize;
  PlayerFontSize = sp->PlayerFontSize;
  WrdFontSize = sp->WrdFontSize;
  DocFontSize = sp->DocFontSize;
  ListFontSize = sp->ListFontSize;
  TracerFontSize = sp->TracerFontSize;
  strncpy(SystemFont,sp->SystemFont,255);
  SystemFont[255] = '\0';
  strncpy(PlayerFont,sp->PlayerFont,255);
  PlayerFont[255] = '\0';
  strncpy(WrdFont,sp->WrdFont,255);
  WrdFont[255] = '\0';
  strncpy(DocFont,sp->DocFont,255);
  DocFont[255] = '\0';
  strncpy(ListFont,sp->ListFont,255);
  ListFont[255] = '\0';
  strncpy(TracerFont,sp->TracerFont,255);
  TracerFont[255] = '\0';
  // Apply font functions ...
}

void
SaveSettingPlayer(SETTING_PLAYER *sp)
{
  sp->InitMinimizeFlag = SetFlag(InitMinimizeFlag);
  sp->DebugWndStartFlag = SetFlag(DebugWndStartFlag);
  sp->ConsoleWndStartFlag = SetFlag(ConsoleWndStartFlag);
  sp->ListWndStartFlag = SetFlag(ListWndStartFlag);
  sp->TracerWndStartFlag = SetFlag(TracerWndStartFlag);
  sp->DocWndStartFlag = SetFlag(DocWndStartFlag);
  sp->WrdWndStartFlag = SetFlag(WrdWndStartFlag);
  sp->DebugWndFlag = SetFlag(DebugWndFlag);
  sp->ConsoleWndFlag = SetFlag(ConsoleWndFlag);
  sp->ListWndFlag = SetFlag(ListWndFlag);
  sp->TracerWndFlag = SetFlag(TracerWndFlag);
  sp->DocWndFlag = SetFlag(DocWndFlag);
  sp->WrdWndFlag = SetFlag(WrdWndFlag);
  sp->SoundSpecWndFlag = SetFlag(SoundSpecWndFlag);
  sp->SubWindowMax = SetValue(SubWindowMax,1,10);
  strncpy(sp->ConfigFile,ConfigFile,MAXPATH + 31);
  (sp->ConfigFile)[MAXPATH + 31] = '\0';
  strncpy(sp->PlaylistFile,PlaylistFile,MAXPATH + 31);
  (sp->PlaylistFile)[MAXPATH + 31] = '\0';
  strncpy(sp->PlaylistHistoryFile,PlaylistHistoryFile,MAXPATH + 31);
  (sp->PlaylistHistoryFile)[MAXPATH + 31] = '\0';
  strncpy(sp->MidiFileOpenDir,MidiFileOpenDir,MAXPATH + 31);
  (sp->MidiFileOpenDir)[MAXPATH + 31] = '\0';
  strncpy(sp->ConfigFileOpenDir,ConfigFileOpenDir,MAXPATH + 31);
  (sp->ConfigFileOpenDir)[MAXPATH + 31] = '\0';
  strncpy(sp->PlaylistFileOpenDir,PlaylistFileOpenDir,MAXPATH + 31);
  (sp->PlaylistFileOpenDir)[MAXPATH + 31] = '\0';
  sp->ProcessPriority = ProcessPriority;
  sp->PlayerThreadPriority = PlayerThreadPriority;
  sp->MidiPlayerThreadPriority = MidiPlayerThreadPriority;
  sp->MainThreadPriority = MainThreadPriority;
  sp->TracerThreadPriority = TracerThreadPriority;
  sp->WrdThreadPriority = WrdThreadPriority;
  sp->WrdGraphicFlag = SetFlag(WrdGraphicFlag);
  sp->TraceGraphicFlag = SetFlag(TraceGraphicFlag);
  // fonts ...
  sp->SystemFontSize = SystemFontSize;
  sp->PlayerFontSize = PlayerFontSize;
  sp->WrdFontSize = WrdFontSize;
  sp->DocFontSize = DocFontSize;
  sp->ListFontSize = ListFontSize;
  sp->TracerFontSize = TracerFontSize;
  strncpy(sp->SystemFont,SystemFont,255);
  sp->SystemFont[255] = '\0';
  strncpy(sp->PlayerFont,PlayerFont,255);
  sp->PlayerFont[255] = '\0';
  strncpy(sp->WrdFont,WrdFont,255);
  sp->WrdFont[255] = '\0';
  strncpy(sp->DocFont,DocFont,255);
  DocFont[255] = '\0';
  strncpy(sp->ListFont,ListFont,255);
  sp->ListFont[255] = '\0';
  strncpy(sp->TracerFont,TracerFont,255);
  sp->TracerFont[255] = '\0';
}

extern int set_play_mode(char *cp);
extern int set_tim_opt(int c, char *optarg);
extern int set_ctl(char *cp);
extern int set_wrd(char *w);

#ifdef PRESENCE_HACK
extern int presence_balance;
extern double presence_delay_msec;
#ifndef atof
extern double atof(const char *);
#endif
#endif /* PRESENCE_HACK */

#if defined(__W32__) && defined(SMFCONV)
extern int opt_rcpcv_dll;
#endif /* SMFCONV */

extern int noise_sharp_type;
extern char *wrdt_open_opts;
/* FIXME */
//char *S_wrdt_open_opts[64];
char S_wrdt_open_opts[64];

void
ApplySettingTimidity(SETTING_TIMIDITY *st)
{
  int i;
  char buffer[256];
  free_instruments_afterwards = st->free_instruments_afterwards;

  effect_lr_mode = st->effect_lr_mode;
  effect_lr_delay_msec = st->effect_lr_delay_msec;
  quietchannels = st->quietchannels;
  drumchannels = st->drumchannels;
  drumchannel_mask = st->drumchannel_mask;
  default_drumchannels = st->default_drumchannels;
  default_drumchannel_mask = st->default_drumchannel_mask;
  sprintf(buffer,"%c",st->play_mode_id_character);
  set_play_mode(buffer);
  play_mode->encoding = st->play_mode_encoding;
  strncpy(OutputName,st->OutputName,MAXPATH + 31);
  OutputName[MAXPATH + 31] = '\0';
  if(OutputName[0]=='\0')
    play_mode->name = NULL;
  else
    play_mode->name = OutputName;
//  st->output_rate = SetValue
//    (st->output_rate,MIN_OUTPUT_RATE,MAX_OUTPUT_RATE);
  if(st->output_rate != 0)
    play_mode->rate = st->output_rate;
  else if(play_mode->rate == 0)
    play_mode->rate = DEFAULT_RATE;

#if 0 /* FIXME */
  if (st->buffer_fragments != -1)
    play_mode->extra_param[0]=SetValue(st->buffer_fragments, 3, 1000);
#endif

  antialiasing_allowed = SetFlag(st->antialiasing_allowed);
  fast_decay = SetFlag(st->fast_decay);
  adjust_panning_immediately =
    SetFlag(st->adjust_panning_immediately);
  for(i = 0; i < MAX_CHANNELS; i++)
    default_program[i] = (st->default_program)[i];
  amplification = SetValue
    (st->amplification, 0, MAX_AMPLIFICATION);
  if(!st->control_ratio){
    control_ratio = play_mode->rate / CONTROLS_PER_SECOND;
    control_ratio = SetValue(control_ratio, 1, MAX_CONTROL_RATIO);
  } else
    control_ratio = SetValue(st->control_ratio, 1, MAX_CONTROL_RATIO);
  voices = SetValue(st->voices, 1, MAX_VOICES);

  /* FIXME */
//  modify_release = SetValue(st->modify_release, 0, MRELS - 1);

#ifdef SUPPORT_SOUNDSPEC
  view_soundspec_flag = SetFlag(st->view_soundspec_flag);
  spectrigram_update_sec = st->spectrigram_update_sec;
#endif
  allocate_cache_size = st->allocate_cache_size;
// FIXME
//st->ctl_id_character='w';
  sprintf(buffer,"%c",st->ctl_id_character);
  set_ctl(buffer);
  ctl->verbosity = st->ctl_verbosity;
  ctl->id_character = st->ctl_id_character;
  ctl->trace_playing = SetFlag(st->ctl_trace_playing);
  trace_nodelay(!ctl->trace_playing);
  //	strncpy(output_text_code,st->output_text_code,MAXPATH + 63);
  //	output_text_code[63] = '\0';
  opt_modulation_wheel = SetFlag(st->opt_modulation_wheel);
  opt_portamento = SetFlag(st->opt_portamento);
  opt_nrpn_vibrato = SetFlag(st->opt_nrpn_vibrato);
  opt_reverb_control = st->opt_reverb_control;
  opt_chorus_control = st->opt_reverb_control;
  //	for(i=0;i<MAX_CHANNELS;i++)
  //		opt_chorus_control2[i] = (st->opt_chorus_control2)[i];
  opt_channel_pressure = SetFlag(st->opt_channel_pressure);
  opt_trace_text_meta_event = SetFlag(st->opt_trace_text_meta_event);
  opt_overlap_voice_allow = SetFlag(st->opt_overlap_voice_allow);
  opt_default_mid = st->opt_default_mid;
  if(strcmp((char *)st->opt_default_mid_str, "gs") == 0
     || strcmp((char *)st->opt_default_mid_str, "GS") == 0)
    opt_default_mid = 0x41;
  else if(strcmp((char *)st->opt_default_mid_str, "xg") == 0
	  || strcmp((char *)st->opt_default_mid_str, "XG") == 0)
    opt_default_mid = 0x43;
  else if(strcmp((char *)st->opt_default_mid_str, "gm") == 0
	  || strcmp((char *)st->opt_default_mid_str, "GM") == 0)
    opt_default_mid = 0x7e;
  default_tonebank = st->default_tonebank;
  special_tonebank = st->special_tonebank;
  opt_realtime_playing = SetFlag(st->opt_realtime_playing);
#if defined(__W32__) && defined(SMFCONV)
  opt_rcpcv_dll = st->opt_rcpcv_dll;
#endif /* SMFCONV */
  sprintf(buffer,"%c%s",st->wrdt_id,st->wrdt_open_opts);
  set_wrd(buffer);
  strncpy(S_wrdt_open_opts,st->wrdt_open_opts,63);
  S_wrdt_open_opts[63] = '\0';
  noise_sharp_type = st->noise_sharp_type;
#if 0
  for(i=0;i<4;i++)
    ns_tap[i] = (st->ns_tap)[i];
#endif
}

void
SaveSettingTimidity(SETTING_TIMIDITY *st)
{
  int i;
  st->free_instruments_afterwards = free_instruments_afterwards;
  st->quietchannels = quietchannels;
  st->effect_lr_mode = effect_lr_mode;
  st->effect_lr_delay_msec = effect_lr_delay_msec;
  st->drumchannels = drumchannels;
  st->drumchannel_mask = drumchannel_mask;
  st->default_drumchannels = drumchannels;
  st->default_drumchannel_mask = drumchannel_mask;
  st->play_mode_encoding = play_mode->encoding;
  st->play_mode_id_character = play_mode->id_character;
  strncpy(st->OutputName,OutputName,MAXPATH + 31);
  st->OutputName[MAXPATH + 31] = '\0';
  st->output_rate = SetValue
    (play_mode->rate,MIN_OUTPUT_RATE,MAX_OUTPUT_RATE);

  /* FIXME */
#if defined(AU_LINUX) || defined(AU_W32) || defined(AU_BSDI)
  st->buffer_fragments = play_mode->extra_param[0];
#endif

  st->antialiasing_allowed = SetFlag(antialiasing_allowed);
  st->fast_decay = SetFlag(fast_decay);
  st->adjust_panning_immediately =
    SetFlag(adjust_panning_immediately);
  for(i = 0; i < MAX_CHANNELS; i++)
    (st->default_program)[i] = default_program[i];
  st->amplification = SetValue
    (amplification, 0, MAX_AMPLIFICATION);
  if(!control_ratio){
    st->control_ratio = play_mode->rate / CONTROLS_PER_SECOND;
    st->control_ratio = SetValue(st->control_ratio, 1, MAX_CONTROL_RATIO);
  } else
    st->control_ratio = SetValue(control_ratio, 1, MAX_CONTROL_RATIO);
  st->voices = SetValue(voices, 1, MAX_VOICES);

  /* FIXME */
//  st->modify_release = SetValue(modify_release, 0, MRELS - 1);


#ifdef SUPPORT_SOUNDSPEC
  st->view_soundspec_flag = SetFlag(view_soundspec_flag);
  st->spectrigram_update_sec = spectrigram_update_sec;
#endif
  st->allocate_cache_size = allocate_cache_size;
  st->ctl_id_character = ctl->id_character;
#if 0
// FIXME
//st->ctl_id_character = 'w';
  sprintf(buffer,"%c",st->ctl_id_character);
  set_ctl(buffer);
#endif
  st->ctl_verbosity = ctl->verbosity;
  st->ctl_id_character = ctl->id_character;
  st->ctl_trace_playing = SetFlag(ctl->trace_playing);
  strncpy(st->output_text_code,output_text_code,MAXPATH + 63);
  (st->output_text_code)[63] = '\0';
  st->opt_modulation_wheel = SetFlag(opt_modulation_wheel);
  st->opt_portamento = SetFlag(opt_portamento);
  st->opt_nrpn_vibrato = SetFlag(opt_nrpn_vibrato);
  st->opt_reverb_control = opt_reverb_control;
  st->opt_chorus_control = opt_reverb_control;
  //	for(i=0;i<MAX_CHANNELS;i++)
  //		(st->opt_chorus_control2)[i] = opt_chorus_control2[i];
  st->opt_channel_pressure = SetFlag(opt_channel_pressure);
  st->opt_trace_text_meta_event = SetFlag(opt_trace_text_meta_event);
  st->opt_overlap_voice_allow = SetFlag(opt_overlap_voice_allow);
  st->opt_default_mid = opt_default_mid;
  if(opt_default_mid == 0x41)
    strcpy((char *)st->opt_default_mid_str,"gs");
  else if(opt_default_mid == 0x439)
    strcpy((char *)st->opt_default_mid_str,"xg");
  else if(opt_default_mid == 0x7e)
    strcpy((char *)st->opt_default_mid_str,"gm");
  else
    strcpy((char *)st->opt_default_mid_str,"");
  st->default_tonebank = default_tonebank;
  st->special_tonebank = special_tonebank;
  st->opt_realtime_playing = SetFlag(opt_realtime_playing);
#if defined(__W32__) && defined(SMFCONV)
  st->opt_rcpcv_dll = opt_rcpcv_dll;
#endif /* SMFCONV */
  st->wrdt_id = wrdt->id;
  //  strncpy(st->wrdt_open_opts,wrdt_open_opts,60);
  //	(st->wrdt_open_opts)[60] = '\0';
  strncpy(st->wrdt_open_opts,S_wrdt_open_opts,60);
  (st->wrdt_open_opts)[60] = '\0';
  st->noise_sharp_type = noise_sharp_type;
#if 0
  for(i=0;i<4;i++)
    (st->ns_tap)[i] = ns_tap[i];
#endif
}






//****************************************************************************/
// ini & config

/*
   
   CHAR INI_free_instruments_afterwards = "free_instruments_afterwards";
   CHAR INI_config_file = "config_file";
   CHAR INI_channel_flag = "channel_flag";
   CHAR INI_presence_balance = "presence_balance";
   CHAR INI_options = "options";
   CHAR INI_ = "";
   CHAR INI_ = "";
   CHAR INI_ = "";
   CHAR INI_ = "";
   CHAR INI_ = "";
   CHAR INI_ = "";
   CHAR INI_ = "";
   int load_inifile(void){
   DWORD dwRes;
   CHAR buffer[INI_MAXLEN];
   if(IniGetKeyLong(INI_free_instruments_afterwards,dwRes))
   if(dwRes)
   free_instruments_afterwards = 1;
   else
   free_instruments_afterwards = 0;
   if(IniGetKeyString(INI_config_file,buffer))
   strcpy(config_file, buffer);
   if(IniGetKeyLong(INI_channel_flag,dwRes))
   set_channel_flag(&quietchannels, dwRes, "Quiet channel");
   if(IniGetKeyString(INI_config_file,buffer))
   strcpy(config_file, buffer);
   if(IniGetKeyLong(INI_presence_balance,dwRes))
   if(dwRes >= -1 && dwRes <= 2)
   presence_balance = dwRes;
   if(IniGetKeyLong(INI_,dwRes))
   if(IniGetKeyLong(INI_,dwRes))
   if(IniGetKeyLong(INI_,dwRes))
   if(IniGetKeyLong(INI_,dwRes))
	if(IniGetKeyLong(INI_,dwRes))
	if(IniGetKeyString(INI_,buffer))
	if(IniGetKeyString(INI_,buffer))
	if(IniGetKeyString(INI_,buffer))
	if(IniGetKeyString(INI_,buffer))
	if(IniGetKeyString(INI_,buffer))
	*/

static char S_IniFile[MAXPATH + 32];
static char S_ConfigFile[MAXPATH + 32];
static char S_PlaylistFile[MAXPATH + 32];
static char S_PlaylistHistoryFile[MAXPATH + 32];
static char S_MidiFileOpenDir[MAXPATH + 32];
static char S_ConfigFileOpenDir[MAXPATH + 32];
static char S_PlaylistFileOpenDir[MAXPATH + 32];
static char S_DocFileExt[256];
static char S_OutputName[MAXPATH + 32];
static char *OutputName;
static char S_SystemFont[256];
static char S_PlayerFont[256];
static char S_WrdFont[256];
static char S_DocFont[256];
static char S_ListFont[256];
static char S_TracerFont[256];
static char *SystemFont = S_SystemFont;
char *PlayerFont = S_PlayerFont;
static char *WrdFont = S_WrdFont;
static char *DocFont = S_DocFont;
static char *ListFont = S_ListFont;
static char *TracerFont = S_TracerFont;

//static HFONT hSystemFont = 0;
//static HFONT hPlayerFont = 0;
//static HFONT hWrdFont = 0;
//static HFONT hDocFont = 0;
//static HFONT hListFont = 0;
//static HFONT hTracerFont = 0;
static int SystemFontSize = 9;
static int PlayerFontSize = 16;
static int WrdFontSize = 16;
static int DocFontSize = 16;
static int ListFontSize = 16;
static int TracerFontSize = 16;

#define DEFAULT_DOCFILEEXT "doc;txt;hed"

SETTING_PLAYER *sp_default, *sp_current, *sp_temp;
SETTING_TIMIDITY *st_default, *st_current, *st_temp;

extern int read_config_file(char *name, int self);

void
w32g_initialize(void)
{
  char buffer[MAXPATH + 1024];
  char *p;
  
  IniFile = S_IniFile;
  ConfigFile = S_ConfigFile;
  PlaylistFile = S_PlaylistFile;
  PlaylistHistoryFile = S_PlaylistHistoryFile;
  MidiFileOpenDir = S_MidiFileOpenDir;
  ConfigFileOpenDir = S_ConfigFileOpenDir;
  PlaylistFileOpenDir = S_PlaylistFileOpenDir;
  DocFileExt = S_DocFileExt;
  OutputName = S_OutputName;
  
  IniFile[0] = '\0';
  ConfigFile[0] = '\0';
  PlaylistFile[0] = '\0';
  PlaylistHistoryFile[0] = '\0';
  MidiFileOpenDir[0] = '\0';
  ConfigFileOpenDir[0] = '\0';
  PlaylistFileOpenDir[0] = '\0';
  OutputName[0] = '\0';
  strcpy(DocFileExt,DEFAULT_DOCFILEEXT);
  S_wrdt_open_opts[0] = '\0';
  strcpy(SystemFont,"‚l‚r –¾’©");
  strcpy(PlayerFont,"‚l‚r –¾’©");
  strcpy(WrdFont,"‚l‚r –¾’©");
  strcpy(DocFont,"‚l‚r –¾’©");
  strcpy(ListFont,"‚l‚r –¾’©");
  strcpy(TracerFont,"‚l‚r –¾’©");

  if(GetModuleFileName(hInst, buffer, MAXPATH)){
    if((p=strrchr(buffer,'\\'))!=NULL){
      p++;
      *p = '\0';
    } else {
      buffer[0] = '.';
      buffer[1] = '\\';
      buffer[2] = '\0';
    }
  } else {
    buffer[0] = '.';
    buffer[1] = '\\';
    buffer[2] = '\0';
  }
  strncpy(IniFile,buffer,MAXPATH);
  IniFile[MAXPATH] = '\0';
  strcat(IniFile,"timpp32g.ini");

//  strncpy(ConfigFile,buffer,MAXPATH);
//  ConfigFile[MAXPATH] = '\0';
//  strcat(ConfigFile,"timidity.cfg");

  strcpy(ConfigFile, "\\WINDOWS\\TIMIDITY.CFG");

//  printf("## IniFile: \"%s\"\n", IniFile);
//  printf("## ConfigFile: \"%s\"\n", ConfigFile);
  
  st_default = (SETTING_TIMIDITY *)safe_malloc(sizeof(SETTING_TIMIDITY));
  sp_default = (SETTING_PLAYER *)safe_malloc(sizeof(SETTING_PLAYER));
  st_current = (SETTING_TIMIDITY *)safe_malloc(sizeof(SETTING_TIMIDITY));
  sp_current = (SETTING_PLAYER *)safe_malloc(sizeof(SETTING_PLAYER));
  st_temp = (SETTING_TIMIDITY *)safe_malloc(sizeof(SETTING_TIMIDITY));
  sp_temp = (SETTING_PLAYER *)safe_malloc(sizeof(SETTING_PLAYER));
}

extern int set_wrd(char *w);
extern char *wrdt_open_opts;
extern CRITICAL_SECTION critSect;
extern char *def_instr_name;


static int InitializeCriticalSectionDone = 0;

void
w32g_init_ctl(void)
{
  int need_stdin = 0, need_stdout = 0;
  ctl->open(need_stdin, need_stdout);
  if(wrdt->open(wrdt_open_opts)){
#ifdef W32GUI_DEBUG
    PrintfDebugWnd
      ("Couldn't open WRD Tracer: %s (`%c')" NLS,wrdt->name, wrdt->id);
#endif /* W32GUI_DEBUG */
    set_wrd("-");
    wrdt->open(wrdt_open_opts);
  }
  
  /*
     if(!control_ratio){
     control_ratio = play_mode->rate / CONTROLS_PER_SECOND;
     if(control_ratio < 1)
     control_ratio = 1;
     else if (control_ratio > MAX_CONTROL_RATIO)
     control_ratio = MAX_CONTROL_RATIO;
     }
     */

  init_load_soundfont();
  if(allocate_cache_size > 0)
      resamp_cache_reset();

  if(def_instr_name && *def_instr_name)
    set_default_instrument(def_instr_name);

  //	SetConsoleCtrlHandler (handler, TRUE);
  InitializeCriticalSection (&critSect);
  InitializeCriticalSectionDone = 1;

  trace_nodelay(!ctl->trace_playing);
}

void
w32g_reset_ctl(void)
{
  int need_stdin = 0, need_stdout = 0;
  
  play_mode->close_output();
  ctl->close();
  wrdt->close();
  
  ctl->open(need_stdin, need_stdout);
  if(wrdt->open(wrdt_open_opts)){
#ifdef W32GUI_DEBUG
    PrintfDebugWnd
      ("Couldn't open WRD Tracer: %s (`%c')" NLS, wrdt->name, wrdt->id);
#endif /* W32GUI_DEBUG */
    set_wrd("-");
    wrdt->open(wrdt_open_opts);
  }
}

void
w32g_finish_ctl(void)
{
  play_mode->close_output();
  ctl->close();
  wrdt->close();
#if defined(__W32__)
  if(InitializeCriticalSectionDone){
    DeleteCriticalSection (&critSect);
    InitializeCriticalSectionDone = 0;
  }
#endif
}
