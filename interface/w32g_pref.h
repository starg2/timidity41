/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef __W32G2_PREF_H__
#define __W32G2_PREF_H__


extern void	tmdy_free_config(void);
extern void	timidity_start_initialize(void);
extern int read_config_file(char *name, int self, int allow_missing_file);

extern volatile int PrefWndDoing;
void PrefWndCreate(HWND hwnd, UINT cid);

typedef struct wave_ConfigDialogInfo_t {
	char optIDC_EDIT_RIFFWAVE_UPDATE_STEP[1024 + 1];
	int optIDC_CHECKBOX_RIFFWAVE_EXTENSIBLE;
} wave_ConfigDialogInfo_t;

extern int wave_ConfigDialogInfoInit(void);
extern int wave_ConfigDialogInfoApply(void);
extern int wave_ConfigDialogInfoSaveINI(void);
extern int wave_ConfigDialogInfoLoadINI(void);

extern int waveConfigDialog(void);

#ifdef AU_GOGO

// ダイアログの情報をほとんどそのまま保存する。
// コンボボックスについては Index は無意味なので値を保存する。
typedef struct gogo_ConfigDialogInfo_t_ {
	int optIDC_CHECK_DEFAULT;
	int optIDC_CHECK_COMMANDLINE_OPTS;
	char optIDC_EDIT_COMMANDLINE_OPTION[1024+1];
	int optIDC_CHECK_OUTPUT_FORMAT;
	int optIDC_COMBO_OUTPUT_FORMAT;
	int optIDC_CHECK_MPEG1AUDIOBITRATE;
	int optIDC_COMBO_MPEG1_AUDIO_BITRATE;
	int optIDC_CHECK_MPEG2AUDIOBITRATE;
	int optIDC_COMBO_MPEG2_AUDIO_BITRATE;
	int optIDC_CHECK_ENHANCED_LOW_PASS_FILTER;
	char optIDC_EDIT_LPF_PARA1[4+1];
	char optIDC_EDIT_LPF_PARA2[4+1];
	int optIDC_CHECK_ENCODE_MODE;
	int optIDC_COMBO_ENCODE_MODE;
	int optIDC_CHECK_EMPHASIS_TYPE;
	int optIDC_COMBO_EMPHASIS_TYPE;
	int optIDC_CHECK_OUTFREQ;
	char optIDC_EDIT_OUTFREQ[6+1];
	int optIDC_CHECK_MSTHRESHOLD;
	char optIDC_EDIT_MSTHRESHOLD_THRESHOLD[4+1];
	char optIDC_EDIT_MSTHRESHOLD_MSPOWER[4+1];
	int optIDC_CHECK_USE_CPU_OPTS;
	int optIDC_CHECK_CPUMMX;
	int optIDC_CHECK_CPUSSE;
	int optIDC_CHECK_CPU3DNOW;
	int optIDC_CHECK_CPUE3DNOW;
	int optIDC_CHECK_CPUCMOV;
	int optIDC_CHECK_CPUEMMX;
	int optIDC_CHECK_CPUSSE2;
	int optIDC_CHECK_VBR;
	int optIDC_COMBO_VBR;
	int optIDC_CHECK_VBR_BITRATE;
	int optIDC_COMBO_VBR_BITRATE_LOW;
	int optIDC_COMBO_VBR_BITRATE_HIGH;
	int optIDC_CHECK_USEPSY;
	int optIDC_CHECK_VERIFY;
	int optIDC_CHECK_16KHZ_LOW_PASS_FILTER;
} gogo_ConfigDialogInfo_t;

extern volatile gogo_ConfigDialogInfo_t gogo_ConfigDialogInfo;

extern int gogo_ConfigDialogInfoInit(void);
extern int gogo_ConfigDialogInfoApply(void);
extern int gogo_ConfigDialogInfoSaveINI(void);
extern int gogo_ConfigDialogInfoLoadINI(void);

#endif // AU_GOGO


#ifdef AU_VORBIS

typedef struct vorbis_ConfigDialogInfo_t_ {
	int optIDC_CHECK_DEFAULT;
	int optIDC_COMBO_MODE;
	int optIDC_CHECK_USE_TAG;
	char optIDC_EDIT_TITLE[256];
	char optIDC_EDIT_ARTIST[256];
	char optIDC_EDIT_ALBUM[256];
	int optIDC_CHECK_EMBED_LOOP;
} vorbis_ConfigDialogInfo_t;

extern int vorbis_ConfigDialogInfoInit(void);
extern int vorbis_ConfigDialogInfoApply(void);
extern int vorbis_ConfigDialogInfoSaveINI(void);
extern int vorbis_ConfigDialogInfoLoadINI(void);

#endif // AU_VORBIS


#ifdef AU_LAME

typedef struct lame_ConfigDialogInfo_t_ {
	int optIDC_LAME_CBPRESET;
} lame_ConfigDialogInfo_t;

extern int lame_encode_preset;

extern int lame_ConfigDialogInfoInit(void);
extern int lame_ConfigDialogInfoApply(void);
extern int lame_ConfigDialogInfoSaveINI(void);
extern int lame_ConfigDialogInfoLoadINI(void);

#endif // AU_LAME


#ifdef AU_FLAC

typedef struct flac_ConfigDialogInfo_t_ {
	int optIDC_COMBO_ENCODE_MODE;
	int optIDC_CHECKBOX_OGGFLAC_CONTAINER;
} flac_ConfigDialogInfo_t;

extern int flac_ConfigDialogInfoInit(void);
extern int flac_ConfigDialogInfoApply(void);
extern int flac_ConfigDialogInfoSaveINI(void);
extern int flac_ConfigDialogInfoLoadINI(void);

#endif // AU_FLAC


#ifdef AU_OPUS

typedef struct opus_ConfigDialogInfo_t_ {
	char unused;
} opus_ConfigDialogInfo_t;

extern int opus_ConfigDialogInfoInit(void);
extern int opus_ConfigDialogInfoApply(void);
extern int opus_ConfigDialogInfoSaveINI(void);
extern int opus_ConfigDialogInfoLoadINI(void);

#endif // AU_OPUS


#ifdef AU_SPEEX

typedef struct speex_ConfigDialogInfo_t_ {
	char unused;
} speex_ConfigDialogInfo_t;

extern int speex_ConfigDialogInfoInit(void);
extern int speex_ConfigDialogInfoApply(void);
extern int speex_ConfigDialogInfoSaveINI(void);
extern int speex_ConfigDialogInfoLoadINI(void);

#endif // AU_SPEEX


#endif /* __W32G2_PREF_H__ */
