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

    vorbis_a.c

    Functions to output Ogg Vorbis (*.ogg).
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <string.h>

//#define VORBIS_DLL_UNICODE 1 // undef multibyte

#ifdef AU_VORBIS

#ifdef AU_VORBIS_DLL
#include <stdlib.h>
#include <io.h>
#include <ctype.h>
extern int load_ogg_dll(void);
extern void free_ogg_dll(void);
extern int load_vorbis_dll(void);
extern void free_vorbis_dll(void);
#ifndef VORBIS_DLL_INCLUDE_VORBISENC
extern int load_vorbisenc_dll(void);
extern void free_vorbisenc_dll(void);
#endif
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef STDC_HEADERS
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#elif HAVE_STRINGS_H
#include <strings.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef __W32__
#include <windows.h>
#include <winnls.h>
#endif

#include <vorbis/vorbisenc.h>
#include "../vorbis-tools/vorbiscomment/vcedit.h"

#include "timidity.h"
#include "common.h"
#include "output.h"
#include "controls.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"

static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static int output_data(const uint8 *buf, size_t bytes);
static int acntl(int request, void *arg);

static int insert_loop_tags(void);

/* export the playback mode */
#define dpm vorbis_play_mode

PlayMode dpm = {
    44100, PE_16BIT|PE_SIGNED, PF_PCM_STREAM|PF_FILE_OUTPUT,
    -1,
    {0,0,0,0,0},
    "Ogg Vorbis", 'v',
    NULL,
    open_output,
    close_output,
    output_data,
    acntl
};
static char *tag_title = NULL;

static	ogg_stream_state os; /* take physical pages, weld into a logical
				stream of packets */
static	vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
static	vorbis_block	 vb; /* local working space for packet->PCM decode */
static	vorbis_info	 vi; /* struct that stores all the static vorbis bitstream
				settings */
static	vorbis_comment	 vc; /* struct that stores all the user comments */

static int has_loopinfo = 0;
static int32 loopstart;
static int32 looplength;

#if defined ( IA_W32GUI ) || defined ( IA_W32G_SYN )
extern char *w32g_output_dir;
extern int w32g_auto_output_mode;
extern int vorbis_ConfigDialogInfoApply(void);
int ogg_vorbis_mode = 8;	/* initial mode. */
#endif
int ogg_vorbis_embed_loop = 0;

/*************************************************************************/

#if defined ( IA_W32GUI ) || defined ( IA_W32G_SYN )
static int
choose_bitrate(int nch, int rate)
{
  int bitrate;

#if 0
  /* choose an encoding mode */
  /* (mode 0: -> mode2 */
  /* (mode 1: 44kHz stereo uncoupled, N/A\n */
  /* (mode 2: 44kHz stereo uncoupled, roughly 128kbps VBR) */
  /* (mode 3: 44kHz stereo uncoupled, roughly 160kbps VBR) */
  /* (mode 4: 44kHz stereo uncoupled, roughly 192kbps VBR) */
  /* (mode 5: 44kHz stereo uncoupled, roughly 256kbps VBR) */
  /* (mode 6: 44kHz stereo uncoupled, roughly 350kbps VBR) */

  switch (ogg_vorbis_mode) {
  case 0:
    bitrate = 128 * 1000; break;
  case 1:
    bitrate = 112 * 1000; break;
  case 2:
    bitrate = 128 * 1000; break;
  case 3:
    bitrate = 160 * 1000; break;
  case 4:
    bitrate = 192 * 1000; break;
  case 5:
    bitrate = 256 * 1000; break;
  case 6:
    bitrate = 350 * 1000; break;
  default:
    bitrate = 160 * 1000; break;
  }
  return bitrate;
#else
	if (ogg_vorbis_mode < 1 || ogg_vorbis_mode > 1000)
		bitrate = 8;
	else
		bitrate = ogg_vorbis_mode;
	return bitrate;
#endif
  return (int)(nch * rate * (128000.0 / (2.0 * 44100.0)) + 0.5); /* +0.5 for rounding */
}
#else
static int
choose_bitrate(int nch, int rate)
{
  int target;

  /* 44.1kHz 2ch --> 128kbps */
  target = (int)(nch * rate * (128000.0 / (2.0 * 44100.0)) + 0.5); /* +0.5 for rounding */

  return target;
}
#endif


static int ogg_output_open(const char *fname, const char *comment)
{
  int fd;
  int nch;
#if !defined ( IA_W32GUI ) && !defined ( IA_W32G_SYN )
  int bitrate;
#endif

#ifdef AU_VORBIS_DLL
  {
	  int flag = 0;
		if(!load_ogg_dll())
			if(!load_vorbis_dll())
#ifndef VORBIS_DLL_INCLUDE_VORBISENC
				if(!load_vorbisenc_dll())
#endif
					flag = 1;
		if(!flag){
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				  "DLL load failed: %s", "vorbis.dll, vorbisenc.dll, ogg.dll");
			free_ogg_dll();
			free_vorbis_dll();
#ifndef VORBIS_DLL_INCLUDE_VORBISENC
			free_vorbisenc_dll();
#endif
			return -1;
		}
  }
#endif

  if(strcmp(fname, "-") == 0) {
    fd = 1; /* data to stdout */
    if(comment == NULL)
      comment = "(stdout)";
  } else {
    /* Open the audio file */
#ifdef __W32__
	  TCHAR *t = char_to_tchar(fname);
	  fd = _topen(t, FILE_UPDATE_MODE);
	  safe_free(t);
#else
	  fd = open(fname, FILE_UPDATE_MODE);
#endif
	  if(fd < 0) {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s",
		fname, strerror(errno));
      return -1;
    }
    if(comment == NULL)
      comment = fname;
  }

  has_loopinfo = 0;

#if defined ( IA_W32GUI ) || defined ( IA_W32G_SYN )
  vorbis_ConfigDialogInfoApply();
#endif

  nch = (dpm.encoding & PE_MONO) ? 1 : 2;

  /* choose an encoding mode */
  vorbis_info_init(&vi);
#if !defined ( IA_W32GUI ) && !defined ( IA_W32G_SYN )
  bitrate = choose_bitrate(nch, dpm.rate);
  ctl->cmsg(CMSG_INFO,VERB_NOISY,"Target encoding bitrate: %dbps", bitrate);
  vorbis_encode_init(&vi, nch, dpm.rate, -1, bitrate, -1);
#else
  {
	  float bitrate_f = (float)choose_bitrate(nch, dpm.rate);
	  if (bitrate_f <= 10.0 )
		  bitrate_f /= 10.0;
	  if (bitrate_f > 10 )
		  bitrate_f /= 1000.0;
	  ctl->cmsg(CMSG_INFO,VERB_NOISY,"Target encoding VBR quality: %d", bitrate_f);
	  vorbis_encode_init_vbr(&vi, nch, dpm.rate, bitrate_f);
  }
#endif

  {
    /* add a comment */
    char *location_string;

    vorbis_comment_init(&vc);

    location_string =
      (char *)safe_malloc(strlen(comment) + sizeof("LOCATION=") + 2);
    strcpy(location_string, "LOCATION=");
    strcat(location_string, comment);
#if defined(__W32__) && (defined(VORBIS_DLL_UNICODE) && (defined(_UNICODE) || defined(UNICODE)))
	{
		char* location_string_utf8 = w32_mbs_to_utf8 ( location_string );
		if ( location_string_utf8 == NULL ) {
		vorbis_comment_add(&vc, (char *)location_string);
		} else {
		vorbis_comment_add(&vc, (char *)location_string_utf8);
			if ( location_string_utf8 != location_string )
				free ( location_string_utf8 );
		}
		free(location_string);
	}
#else
    vorbis_comment_add(&vc, (char *)location_string);
    free(location_string);
#endif
  }
  /* add default tag */
  if (tag_title != NULL) {
#if defined(__W32__) && (defined(VORBIS_DLL_UNICODE) && (defined(_UNICODE) || defined(UNICODE)))
	{
		char* tag_title_utf8 = w32_mbs_to_utf8 ( tag_title );
		if ( tag_title_utf8 == NULL ) {
			vorbis_comment_add_tag(&vc, "title", (char *)tag_title);
		} else {
			vorbis_comment_add_tag(&vc, "title", (char *)tag_title_utf8);
			if ( tag_title_utf8 != tag_title )
				free ( tag_title_utf8 );
		}
	}
#else
	vorbis_comment_add_tag(&vc, "title", (char *)tag_title);
#endif
  }
	
  /* set up the analysis state and auxiliary encoding storage */
  vorbis_analysis_init(&vd, &vi);
  vorbis_block_init(&vd, &vb);

  /* set up our packet->stream encoder */
  /* pick a random serial number; that way we can more likely build
     chained streams just by concatenation */
  srand(time(NULL));
  ogg_stream_init(&os, rand());

  /* Vorbis streams begin with three headers; the initial header (with
     most of the codec setup parameters) which is mandated by the Ogg
     bitstream spec.  The second header holds any comment fields.  The
     third header holds the bitstream codebook.  We merely need to
     make the headers, then pass them to libvorbis one at a time;
     libvorbis handles the additional Ogg bitstream constraints */

  {
    ogg_packet header;
    ogg_packet header_comm;
    ogg_packet header_code;

    vorbis_analysis_headerout(&vd, &vc, &header, &header_comm, &header_code);
    ogg_stream_packetin(&os, &header); /* automatically placed in its own
					  page */
    ogg_stream_packetin(&os, &header_comm);
    ogg_stream_packetin(&os, &header_code);

    /* no need to write out here.  We'll get to that in the main loop */
  }

  return fd;
}

static int auto_ogg_output_open(const char *input_filename, const char *title)
{
  char *output_filename;

#if !defined ( IA_W32GUI ) && !defined ( IA_W32G_SYN )
  output_filename = create_auto_output_name(input_filename,"ogg",NULL,0);
#else
  output_filename = create_auto_output_name(input_filename,"ogg",w32g_output_dir,w32g_auto_output_mode);
#endif
  if(output_filename==NULL){
	  return -1;
  }
  if (tag_title != NULL) {
	free(tag_title);
	tag_title = NULL;
  }
  if (title != NULL) {
	tag_title = (char *)safe_malloc(sizeof(char)*(strlen(title)+1));
	strcpy(tag_title, title);
  }
  if((dpm.fd = ogg_output_open(output_filename, input_filename)) == -1) {
    free(output_filename);
    return -1;
  }
  if(dpm.name != NULL)
    free(dpm.name);
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

#if !defined ( IA_W32GUI ) && !defined ( IA_W32G_SYN )
  if(dpm.name == NULL) {
    dpm.flag |= PF_AUTO_SPLIT_FILE;
  } else {
    dpm.flag &= ~PF_AUTO_SPLIT_FILE;
    if((dpm.fd = ogg_output_open(dpm.name, NULL)) == -1)
      return -1;
  }
#else
	if(w32g_auto_output_mode>0){
      dpm.flag |= PF_AUTO_SPLIT_FILE;
      dpm.name = NULL;
    } else {
      dpm.flag &= ~PF_AUTO_SPLIT_FILE;
      if((dpm.fd = ogg_output_open(dpm.name,NULL)) == -1)
		return -1;
    }
#endif

  return 0;
}

static int output_data(const uint8 *readbuffer, size_t bytes)
{
  int i, j, ch = ((dpm.encoding & PE_MONO) ? 1 : 2);
  float **buffer;
  int16 *samples = (int16 *)readbuffer;
  int nsamples = bytes / (2 * ch);
  ogg_page   og; /* one Ogg bitstream page.  Vorbis packets are inside */
  ogg_packet op; /* one raw packet of data for decode */

  if (dpm.fd<0)
    return 0;

  /* data to encode */

  /* expose the buffer to submit data */
  buffer = vorbis_analysis_buffer(&vd, nsamples);
      
  /* uninterleave samples */
  for(j = 0; j < ch; j++)
    for(i = 0; i < nsamples; i++)
      buffer[j][i] = (float)(samples[i*ch+j] * (1.0/32768.0));

  /* tell the library how much we actually submitted */
  vorbis_analysis_wrote(&vd, nsamples);

  /* vorbis does some data preanalysis, then divvies up blocks for
     more involved (potentially parallel) processing.  Get a single
     block for encoding now */
  while(vorbis_analysis_blockout(&vd, &vb) == 1) {

    /* analysis */
    vorbis_analysis(&vb, NULL);
	vorbis_bitrate_addblock(&vb);

	while (vorbis_bitrate_flushpacket(&vd, &op)) {
		/* weld the packet into the bitstream */
		ogg_stream_packetin(&os, &op);

		/* write out pages (if any) */
		while(ogg_stream_pageout(&os, &og) != 0) {
		  std_write(dpm.fd, og.header, og.header_len);
		  std_write(dpm.fd, og.body, og.body_len);
		}
	}
  }
  return 0;
}

static void close_output(void)
{
  int eos = 0;
  ogg_page   og; /* one Ogg bitstream page.  Vorbis packets are inside */
  ogg_packet op; /* one raw packet of data for decode */

  if(dpm.fd < 0)
    return;

  /* end of file.  this can be done implicitly in the mainline,
     but it's easier to see here in non-clever fashion.
     Tell the library we're at end of stream so that it can handle
     the last frame and mark end of stream in the output properly */
  vorbis_analysis_wrote(&vd, 0);

  /* vorbis does some data preanalysis, then divvies up blocks for
     more involved (potentially parallel) processing.  Get a single
     block for encoding now */
  while(vorbis_analysis_blockout(&vd, &vb) == 1) {

    /* analysis */
    vorbis_analysis(&vb, NULL);
    vorbis_bitrate_addblock(&vb);

    while(vorbis_bitrate_flushpacket(&vd,&op)) { 

    /* weld the packet into the bitstream */
    ogg_stream_packetin(&os, &op);

    /* write out pages (if any) */
    while(!eos){
      int result = ogg_stream_pageout(&os,&og);
      if(result == 0)
	break;
      std_write(dpm.fd, og.header, og.header_len);
      std_write(dpm.fd, og.body, og.body_len);

      /* this could be set above, but for illustrative purposes, I do
	 it here (to show that vorbis does know where the stream ends) */

      if(ogg_page_eos(&og))
	eos = 1;
    }
	}
  }

  /* clean up and exit.  vorbis_info_clear() must be called last */

  ogg_stream_clear(&os);
  vorbis_block_clear(&vb);
  vorbis_dsp_clear(&vd);
  vorbis_comment_clear(&vc);
  vorbis_info_clear(&vi);

  if (ogg_vorbis_embed_loop && has_loopinfo)
	  insert_loop_tags();

  close(dpm.fd);

#ifdef AU_VORBIS_DLL
#ifndef VORBIS_DLL_INCLUDE_VORBISENC
  free_vorbisenc_dll();
#endif
  free_vorbis_dll();
  free_ogg_dll();
#endif

  dpm.fd = -1;
}

static int acntl(int request, void *arg)
{
  switch(request) {
  case PM_REQ_PLAY_START:
    if (dpm.flag & PF_AUTO_SPLIT_FILE) {
      const char *filename = (current_file_info && current_file_info->filename) ?
			     current_file_info->filename : "Output.mid";
      const char *seq_name = (current_file_info && current_file_info->seq_name) ?
			     current_file_info->seq_name : NULL;
      return auto_ogg_output_open(filename, seq_name);
    }
    return 0;
  case PM_REQ_PLAY_END:
    if(dpm.flag & PF_AUTO_SPLIT_FILE)
      close_output();
    return 0;
  case PM_REQ_DISCARD:
    return 0;
  case PM_REQ_LOOP_START:
	if (ogg_vorbis_embed_loop) {
		loopstart = (int32)arg;
		looplength = 0;
		has_loopinfo = 0;
		ctl->cmsg(CMSG_INFO, VERB_NOISY, "LOOPSTART=%d", loopstart);
	}
    return 0;
  case PM_REQ_LOOP_END:
	if (ogg_vorbis_embed_loop) {
		looplength = (int32)arg - loopstart;
		has_loopinfo = 1;
		ctl->cmsg(CMSG_INFO, VERB_NOISY, "LOOPLENGTH=%d", looplength);
	}
	return 0;
  }
  return -1;
}

static int insert_loop_tags(void)
{
	lseek(dpm.fd, 0, SEEK_SET);

	vcedit_state *state = vcedit_new_state();
	FILE *ftemp = tmpfile();
	FILE *fin = fdopen(dup(dpm.fd), "w+b");

	if (!ftemp) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "failed to insert loop info; tmpfile() failed");
		goto failed;
	}

	if (!fin) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "failed to insert loop info; fdopen() failed");
		return 0;
	}

	if (vcedit_open(state, fin) < 0) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "failed to insert loop info; vcedit_open() failed");
		goto failed;
	}

	vorbis_comment *vc = vcedit_comments(state);
	char buf[4096];
	snprintf(buf, _countof(buf), "LOOPSTART=%d", loopstart);
	vorbis_comment_add(vc, buf);
	snprintf(buf, _countof(buf), "LOOPLENGTH=%d", looplength);
	vorbis_comment_add(vc, buf);

	if (vcedit_write(state, ftemp) < 0) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "failed to insert loop info; vcedit_write() failed");
		goto failed;
	}

	vcedit_clear(state);
	state = NULL;
	fclose(fin);
	fin = NULL;

	lseek(dpm.fd, 0, SEEK_SET);
	fseek(ftemp, 0, SEEK_SET);

	while (1) {
		size_t len = fread(buf, 1, _countof(buf), ftemp);

		if (len == 0) {
			break;
		}

		write(dpm.fd, buf, len);
	}

	fclose(ftemp);
	ftemp = NULL;
	long fsize = lseek(dpm.fd, 0, SEEK_CUR);

#ifdef __W32__
	_chsize(dpm.fd, fsize);
#else
	ftruncate(dpm.fd, fsize);
#endif

	ctl->cmsg(CMSG_INFO, VERB_NORMAL, "Ogg Vorbis loop was inserted (LOOPSTART=%d, LOOPLENGTH=%d).", loopstart, looplength);
	return 1;

failed:
	fclose(ftemp);
	fclose(fin);
	vcedit_clear(state);
	return 0;
}

#endif
