

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <string.h>



#ifdef AU_LAME

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef STDC_HEADERS
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#elif defined(HAVE_STRINGS_H)
#include <strings.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef __W32__
#include <windows.h>
#include <winnls.h>
#endif

#include "timidity.h"
#include "common.h"
#include "output.h"
#include "controls.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"

#if defined(IA_W32GUI) || defined(IA_W32G_SYN)
#include "w32g.h"
#endif /* IA_W32GUI || IA_W32G_SYN */

///r
#include "w32g_res.h"

static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static int32 output_data(const uint8 *buf, size_t bytes);
static int acntl(int request, void *arg);

#include "LameEnc.h"
#include "BladeMP3EncDLL.h"

/* export the playback mode */
#define dpm lame_play_mode

PlayMode dpm = {
    44100, PE_16BIT | PE_SIGNED, PF_PCM_STREAM | PF_FILE_OUTPUT,
    -1,
    { 0, 0, 0, 0, 0 },
    "Lame", 'L',
    NULL,
    open_output,
    close_output,
    output_data,
    acntl
};
static char *tag_title = NULL;

/*************************************************************************/

#if defined(IA_W32GUI) || defined(IA_W32G_SYN)
/*
static int
choose_bitrate(int nch, int rate)
{
  int bitrate;

	if (ogg_vorbis_mode < 1 || ogg_vorbis_mode > 1000)
		bitrate = 8;
	else
		bitrate = ogg_vorbis_mode;
	return bitrate;

  return (int)(nch * rate * (128000.0 / (2.0 * 44100.0)) + 0.5);
}
*/
#else
/*
static int
choose_bitrate(int nch, int rate)
{
  int target;

  target = (int)(nch * rate * (128000.0 / (2.0 * 44100.0)) + 0.5);

  return target;
}
*/
#endif

HMODULE hLame;
BEINITSTREAM		beInitStream = NULL;
BEENCODECHUNK		beEncodeChunk = NULL;
BEDEINITSTREAM		beDeinitStream = NULL;
BECLOSESTREAM		beCloseStream = NULL;
BEVERSION			beVersion = NULL;
BEWRITEVBRHEADER	beWriteVBRHeader = NULL;
BEWRITEINFOTAG		beWriteInfoTag = NULL;

static DWORD dwSamples = 0;
static HBE_STREAM	hbeStream = 0;
static FILE *lame_fpout = NULL;
unsigned char *lame_buffer = NULL;
unsigned char *lame_work_buffer = NULL;
static int lame_buffer_cur = 0;

typedef enum  {
	INSANE,
	FAST_EXTREME,
	EXTREME,
	FAST_STANDARD,
	STANDARD,
	FAST_MEDIUM,
	MEDIUM,
	CBR320,
	CBR256,
	CBR224,
	CBR192,
	CBR160,
	CBR128,
	CBR112,
	CBR96,
	CBR80,
	CBR64,

	QUALITY_SETTING_LAST = MEDIUM,
	CBR_SETTING_LAST = CBR64,
} LAME_PRESET_TIM;

int lame_encode_preset = 4; /* STANDARD */

static int lame_output_open(const char *fname, const char *comment)
{
  int fd = 1;
  int nch;
	FILE *pFileIn			=NULL;
	FILE *pFileOut		=NULL;
	BE_VERSION	Version			= { 0, };
	BE_CONFIG	beConfig		= { 0, };

	CHAR		strFileIn[FILEPATH_MAX]	= { '0', };
	CHAR		strFileOut[FILEPATH_MAX]	= { '0', };

	DWORD		dwMP3Buffer		=0;
	BE_ERR		err				=0;

	PBYTE		pMP3Buffer		=NULL;
	PSHORT		pWAVBuffer		=NULL;

#if !defined(IA_W32GUI) && !defined(IA_W32G_SYN)
  int bitrate;
#endif

	w32_reset_dll_directory();
	hLame = LoadLibraryA("lame_enc.dll");
  	if (NULL == hLame) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "DLL load failed: %s", "lame_enc.dll");
		return -1;
	}


	beInitStream	= (BEINITSTREAM) GetProcAddress(hLame, TEXT_BEINITSTREAM);
	beEncodeChunk	= (BEENCODECHUNK) GetProcAddress(hLame, TEXT_BEENCODECHUNK);
	beDeinitStream	= (BEDEINITSTREAM) GetProcAddress(hLame, TEXT_BEDEINITSTREAM);
	beCloseStream	= (BECLOSESTREAM) GetProcAddress(hLame, TEXT_BECLOSESTREAM);
	beVersion		= (BEVERSION) GetProcAddress(hLame, TEXT_BEVERSION);
	beWriteVBRHeader = (BEWRITEVBRHEADER) GetProcAddress(hLame, TEXT_BEWRITEVBRHEADER);
	beWriteInfoTag  = (BEWRITEINFOTAG) GetProcAddress(hLame, TEXT_BEWRITEINFOTAG);

	if (!beInitStream || !beEncodeChunk || !beDeinitStream || !beCloseStream || !beVersion || !beWriteVBRHeader) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			"DLL load failed: %s", "lame_enc.dll");
		return -1;
	}

	nch = (dpm.encoding & PE_MONO) ? 1 : 2;

	beVersion(&Version);

#if defined(IA_W32GUI) || defined(IA_W32G_SYN)
//  lame_ConfigDialogInfoApply();
#endif
	memset(&beConfig, 0, sizeof(beConfig));					// clear all fields

	// use the LAME config structure
	beConfig.dwConfig = BE_CONFIG_LAME;

	// this are the default settings for testcase.wav
	beConfig.format.LHV1.dwStructVersion	= 1;
	beConfig.format.LHV1.dwStructSize		= sizeof(beConfig);
	beConfig.format.LHV1.dwSampleRate		= play_mode->rate;				// INPUT FREQUENCY
	beConfig.format.LHV1.dwReSampleRate		= 0;					// DON"T RESAMPLE
	beConfig.format.LHV1.nMode				= (nch == 1) ? BE_MP3_MODE_MONO : BE_MP3_MODE_JSTEREO;	// OUTPUT IN STREO

	if (lame_encode_preset <= QUALITY_SETTING_LAST) {
		switch (lame_encode_preset) {
		case INSANE:
			beConfig.format.LHV1.nPreset = LQP_INSANE;
			break;
		case FAST_EXTREME:
			beConfig.format.LHV1.nPreset = LQP_FAST_EXTREME;
			break;
		case EXTREME:
			beConfig.format.LHV1.nPreset = LQP_EXTREME;
			break;
		case FAST_STANDARD:
			beConfig.format.LHV1.nPreset = LQP_FAST_STANDARD;
			break;
		case STANDARD:
			beConfig.format.LHV1.nPreset = LQP_STANDARD;
			break;
		case FAST_MEDIUM:
			beConfig.format.LHV1.nPreset = LQP_FAST_MEDIUM;
			break;
		case MEDIUM:
			beConfig.format.LHV1.nPreset = LQP_MEDIUM;
			break;
		}
		beConfig.format.LHV1.bWriteVBRHeader	= TRUE;
		beConfig.format.LHV1.nVbrMethod			= VBR_METHOD_NEW;
		beConfig.format.LHV1.bEnableVBR			= TRUE;
	} else { // cbr
		switch (lame_encode_preset) {
		case CBR320:
			beConfig.format.LHV1.dwBitrate = beConfig.format.LHV1.dwMaxBitrate = 320;
			break;
		case CBR256:
			beConfig.format.LHV1.dwBitrate = beConfig.format.LHV1.dwMaxBitrate = 256;
			break;
		case CBR224:
			beConfig.format.LHV1.dwBitrate = beConfig.format.LHV1.dwMaxBitrate = 224;
			break;
		case CBR192:
			beConfig.format.LHV1.dwBitrate = beConfig.format.LHV1.dwMaxBitrate = 192;
			break;
		case CBR160:
			beConfig.format.LHV1.dwBitrate = beConfig.format.LHV1.dwMaxBitrate = 160;
			break;
		case CBR128:
			beConfig.format.LHV1.dwBitrate = beConfig.format.LHV1.dwMaxBitrate = 128;
			break;
		case CBR112:
			beConfig.format.LHV1.dwBitrate = beConfig.format.LHV1.dwMaxBitrate = 112;
			break;
		case CBR96:
			beConfig.format.LHV1.dwBitrate = beConfig.format.LHV1.dwMaxBitrate = 96;
			break;
		case CBR80:
			beConfig.format.LHV1.dwBitrate = beConfig.format.LHV1.dwMaxBitrate = 80;
			break;
		case CBR64:
			beConfig.format.LHV1.dwBitrate = beConfig.format.LHV1.dwMaxBitrate = 64;
			break;
		default:
			beConfig.format.LHV1.dwBitrate = beConfig.format.LHV1.dwMaxBitrate = 128;
			break;
		}
	}

	beConfig.format.LHV1.dwMpegVersion		= MPEG1;				// MPEG VERSION (I or II)
	beConfig.format.LHV1.dwPsyModel			= 0;					// USE DEFAULT PSYCHOACOUSTIC MODEL
	beConfig.format.LHV1.dwEmphasis			= 0;					// NO EMPHASIS TURNED ON
	beConfig.format.LHV1.bOriginal			= TRUE;					// SET ORIGINAL FLAG
	beConfig.format.LHV1.bWriteVBRHeader	= TRUE;					// Write INFO tag

	beConfig.format.LHV1.bCRC				= TRUE;					// INSERT CRC
//	beConfig.format.LHV1.bCopyright			= TRUE;					// SET COPYRIGHT FLAG
	beConfig.format.LHV1.bPrivate			= TRUE;					// SET PRIVATE FLAG
	beConfig.format.LHV1.bNoRes				= TRUE;					// No Bit resorvoir

	// Init the MP3 Stream
	err = beInitStream(&beConfig, &dwSamples, &dwMP3Buffer, &hbeStream);

	// Check result
	if (err != BE_ERR_SUCCESSFUL) {
		fprintf(stderr, "Error opening encoding stream (%lu)", err);
		return -1;
	}

#if !defined(IA_W32GUI) && !defined(IA_W32G_SYN)

#else

#endif


  /* add default tag */
    if (tag_title) {
#ifndef __W32__

#else

#endif
  }

	lame_fpout = fopen(fname, "wb");
	lame_buffer = (unsigned char*) safe_malloc(dwSamples * 8);
	lame_work_buffer = (unsigned char*) safe_malloc(dwSamples * 8);
	lame_buffer_cur = 0;

  return fd;
}

static int auto_lame_output_open(const char *input_filename, const char *title)
{
  char *output_filename;

  output_filename = create_auto_output_name(input_filename, "mp3", NULL, 0);
  if (!output_filename) {
	  return -1;
  }
  safe_free(tag_title);
  tag_title = NULL;
  if (title) {
	tag_title = (char*) safe_malloc(sizeof(char) * (strlen(title) + 1));
	strcpy(tag_title, title);
  }
  if ((dpm.fd = lame_output_open(output_filename, input_filename)) == -1) {
    safe_free(output_filename);
    return -1;
  }
  safe_free(dpm.name);
  dpm.name = output_filename;
  ctl->cmsg(CMSG_INFO, VERB_NORMAL, "Output %s", dpm.name);
  return 0;
}

static int open_output(void)
{
  int include_enc, exclude_enc;

  /********** Encode setup ************/
///r
  include_enc = exclude_enc = 0;
  /* only 16 bit is supported */
  include_enc |= PE_16BIT | PE_SIGNED;
  exclude_enc |= PE_BYTESWAP | PE_ULAW | PE_ALAW | PE_24BIT | PE_32BIT | PE_F32BIT | PE_64BIT | PE_F64BIT;
  dpm.encoding = validate_encoding(dpm.encoding, include_enc, exclude_enc);

  if (!dpm.name)
    dpm.flag |= PF_AUTO_SPLIT_FILE;
  else {
    dpm.flag &= ~PF_AUTO_SPLIT_FILE;
    if ((dpm.fd = lame_output_open(dpm.name, NULL)) == -1)
      return -1;
  }

  return 0;
}

static int32 output_data(const uint8 *readbuffer, size_t bytes)
{
	BE_ERR err;
	DWORD dwRead = 0, dwWrite;
	size_t read_size = 0;
	int32 sample_sz = dwSamples * 2;
	size_t total_pos = 0;
	int32 bytes_tmp = bytes; // “Ç‚Ýž‚Þ‚½‚Ñ‚ÉƒfƒNƒŠ
/*
	while ((bytes_tmp + lame_buffer_cur) > sample_sz) {
		read_size = abs(bytes_tmp - lame_buffer_cur);

		if (read_size > sample_sz) {
			read_size = abs((int)dwSamples - lame_buffer_cur);
			total_pos += read_size;
			bytes_tmp -= read_size;
		} else {
			total_pos += read_size;
			bytes_tmp -= read_size;
		}
		dwRead = read_size;

		memcpy(lame_buffer + lame_buffer_cur, readbuffer + total_pos, read_size);


		err = beEncodeChunk(hbeStream, dwRead / 2, (short*)lame_buffer, (short*)lame_work_buffer, &dwWrite);
		if (err != BE_ERR_SUCCESSFUL) {
			beCloseStream(hbeStream);
			return -1;
		}
		if (fwrite(lame_work_buffer, 1, dwWrite, lame_fpout) != dwWrite) {
			return -1;
		}

		lame_buffer_cur = 0;
	}

	read_size = abs((bytes - total_pos) - lame_buffer_cur);
	memcpy(lame_buffer + lame_buffer_cur, readbuffer, read_size);
	lame_buffer_cur += read_size;
*/
	if (!beEncodeChunk) {
		return -1;
	}

	err = beEncodeChunk(hbeStream, divi_2(bytes), (short*)readbuffer, (unsigned char*)lame_work_buffer, &dwWrite);
	if (err != BE_ERR_SUCCESSFUL) {
		beCloseStream(hbeStream);
		return -1;
	}
	if (fwrite(lame_work_buffer, 1, dwWrite, lame_fpout) != dwWrite) {
		return -1;
	}

	return 0;
}

static void close_output(void)
{
	BE_ERR err;
	DWORD dwRead, dwWrite;

	if (hbeStream) {

		err = beDeinitStream(hbeStream, lame_work_buffer, &dwWrite);

		// Check result
		if (err != BE_ERR_SUCCESSFUL) {
			return;
		}

		if (dwWrite) {
			if (fwrite(lame_work_buffer, 1, dwWrite, lame_fpout) != dwWrite) {
			}
		}

		beCloseStream(hbeStream);

		safe_free(lame_buffer);
		lame_buffer = NULL;
		safe_free(lame_work_buffer);
		lame_work_buffer = NULL;
		FreeLibrary(hLame);
		hLame = NULL;
		fclose(lame_fpout);
		lame_fpout = NULL;

	}

	hbeStream = 0;


	dpm.fd = -1;
}

static int acntl(int request, void *arg)
{
  switch (request) {
  case PM_REQ_PLAY_START:
    if (dpm.flag & PF_AUTO_SPLIT_FILE) {
      const char *filename = (current_file_info && current_file_info->filename) ?
			     current_file_info->filename : "Output.mid";
      const char *seq_name = (current_file_info && current_file_info->seq_name) ?
			     current_file_info->seq_name : NULL;
      return auto_lame_output_open(filename, seq_name);
    }
    return 0;
  case PM_REQ_PLAY_END:
    if (dpm.flag & PF_AUTO_SPLIT_FILE)
      close_output();
    return 0;
  case PM_REQ_DISCARD:
    return 0;
  }
  return -1;
}

#endif

