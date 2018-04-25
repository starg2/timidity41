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

/***************************************************************
name: vorbisfile_dll  dll: vorbisfile.dll
***************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "common.h"

#ifndef VORBIS_DLL_INCLUDE_VORBISFILE

#ifdef AU_VORBIS_DLL

#include <windows.h>
#include <vorbis/vorbisfile.h>

extern int load_vorbisfile_dll(void);
extern void free_vorbisfile_dll(void);

typedef int (*type_ov_clear)(OggVorbis_File *vf);
typedef int (*type_ov_open_callbacks)(void *datasource, OggVorbis_File *vf,
	const char *initial, long ibytes, ov_callbacks callbacks);
typedef ogg_int64_t(*type_ov_pcm_total)(OggVorbis_File *vf, int i);
typedef vorbis_info *(*type_ov_info)(OggVorbis_File *vf, int link);
typedef long (*type_ov_read)(OggVorbis_File *vf, char *buffer, int length,
	int bigendianp, int word, int sgned, int *bitstream);

static struct vorbisfile_dll_ {
	type_ov_clear ov_clear;
	type_ov_open_callbacks ov_open_callbacks;
	type_ov_pcm_total ov_pcm_total;
	type_ov_info ov_info;
	type_ov_read ov_read;
} vorbisfile_dll;

static volatile HANDLE h_vorbisfile_dll = NULL;

void free_vorbisfile_dll(void)
{
	if (h_vorbisfile_dll) {
		FreeLibrary(h_vorbisfile_dll);
		h_vorbisfile_dll = NULL;
	}
}

int load_vorbisfile_dll(void)
{
	if (!h_vorbisfile_dll) {
		w32_reset_dll_directory();
		h_vorbisfile_dll = LoadLibrary(TEXT("vorbisfile.dll"));
		if (!h_vorbisfile_dll) h_vorbisfile_dll = LoadLibrary(TEXT("libvorbisfile-2.dll"));
		if (!h_vorbisfile_dll) h_vorbisfile_dll = LoadLibrary(TEXT("libvorbisfile.dll"));
		if (!h_vorbisfile_dll) return -1;
		vorbisfile_dll.ov_clear = (type_ov_clear)GetProcAddress(h_vorbisfile_dll, "ov_clear");
		if (!vorbisfile_dll.ov_clear) { free_vorbisfile_dll(); return -1; }
		vorbisfile_dll.ov_open_callbacks = (type_ov_open_callbacks)GetProcAddress(h_vorbisfile_dll, "ov_open_callbacks");
		if (!vorbisfile_dll.ov_open_callbacks) { free_vorbisfile_dll(); return -1; }
		vorbisfile_dll.ov_pcm_total = (type_ov_pcm_total) GetProcAddress(h_vorbisfile_dll, "ov_pcm_total");
		if (!vorbisfile_dll.ov_pcm_total) { free_vorbisfile_dll(); return -1; }
		vorbisfile_dll.ov_info = (type_ov_info)GetProcAddress(h_vorbisfile_dll, "ov_info");
		if (!vorbisfile_dll.ov_info) { free_vorbisfile_dll(); return -1; }
		vorbisfile_dll.ov_read = (type_ov_read)GetProcAddress(h_vorbisfile_dll, "ov_read");
		if (!vorbisfile_dll.ov_read) { free_vorbisfile_dll(); return -1; }
	}
	return 0;
}

int ov_clear(OggVorbis_File *vf)
{
	if (h_vorbisfile_dll) {
		return vorbisfile_dll.ov_clear(vf);
	}

	return -1;
}

int ov_open_callbacks(void *datasource, OggVorbis_File *vf,
	const char *initial, long ibytes, ov_callbacks callbacks)
{
	if (h_vorbisfile_dll) {
		return vorbisfile_dll.ov_open_callbacks(datasource, vf, initial, ibytes, callbacks);
	}

	return -1;
}

ogg_int64_t ov_pcm_total(OggVorbis_File *vf, int i)
{
	if (h_vorbisfile_dll) {
		return vorbisfile_dll.ov_pcm_total(vf, i);
	}

	return -1;
}

vorbis_info *ov_info(OggVorbis_File *vf, int link)
{
	if (h_vorbisfile_dll) {
		return vorbisfile_dll.ov_info(vf, link);
	}

	return NULL;
}

long ov_read(OggVorbis_File *vf, char *buffer, int length,
	int bigendianp, int word, int sgned, int *bitstream)
{
	if (h_vorbisfile_dll) {
		return vorbisfile_dll.ov_read(vf, buffer, length, bigendianp, word, sgned, bitstream);
	}

	return -1;
}

/***************************************************************/
#endif /* AU_VORBIS_DLL */
#endif /* ! VORBIS_DLL_INCLUDE_VORBISFILE */
