/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999,2000 Masanao Izumo <mo@goice.co.jp>
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

*/

#ifndef ___MOD_H_
#define ___MOD_H_

/*
                Clock
SampleRate := ----------
                Period
*/

#define NTSC_CLOCK 3579545.25
#define NTSC_RATE (NTSC_CLOCK/428)

#define PAL_CLOCK 3546894.6
#define PAL_RATE (PAL_CLOCK/428)


#define MIDI_CLOCK 3499241.93589551  /* for freq_table[]
				      * freq_table[0] := MIDI_CLOCK/428
				      */
#define NTSC_MIDI_FINETUNE -101
#define PAL_MIDI_FINETUNE -60

#define MOD_BASE_NOTE 60
#define MOD_NOTE_OFFSET 36
#define MOD_BEND_SENSITIVE 60

#define MOD_EFX_PORTAMENT_UP		0x01
#define MOD_EFX_PORTAMENT_DOWN		0x02
#define MOD_EFX_PORTAMENT		0x03
#define MOD_EFX_VIBRATO			0x04
#define MOD_EFX_PORTANOTEVOLSLIDE	0x05
#define MOD_EFX_VIBRATOVOLSLIDE		0x06
#define MOD_EFX_TREMOLO			0x07
#define MOD_EFX_SAMPOFFS		0x09
#define MOD_EFX_VOLSLIDE		0x0a
#define MOD_EFX_VOLCHNG			0x0c
#define MOD_EFX_E			0x0e

/* MOD_EFX_E */
#define MOD_EFXE_FILTER			0x00
#define MOD_EFXE_FINESLIDE_UP		0x01
#define MOD_EFXE_FINESLIDE_DOWN		0x02
#define MOD_EFXE_GLISSANDO		0x03
#define MOD_EFXE_VIBWAVEFORM		0x04
#define MOD_EFXE_FINETUNE		0x05
#define MOD_EFXE_LOOP			0x06
#define MOD_EFXE_TREMWAVEFORM		0x07
#define MOD_EFXE_RETRIGGER		0x09
#define MOD_EFXE_FINEVOLSLIDE_UP	0x0a
#define MOD_EFXE_FINEVOLSLIDE_DOWN	0x0b
#define MOD_EFXE_NOTECUT		0x0c
#define MOD_EFXE_NOTEDELAY 		0x0d
#define MOD_EFXE_PATTERNDELAY 		0x0e
#define MOD_EFXE_INVERTEDLOOP 		0x0f


typedef struct _ModVoice
{
    int sample;			/* current sample ID */
    int noteon;			/* (-1 means OFF status) */
    int period;
    int efx, arg;		/* Effect & Argument */
    int tune, tuneon;		/* note fine tune */
    int vol, lastvol;		/* current volume, last volume */
    int retrig;			/* retrigger */
    int skipon;
    int32 start, starttmp;	/* sample offset */
    int period_start, period_todo, period_step; /* For portamento */
} ModVoice;

extern int get_module_type(char *fn);
extern int load_module_file(struct timidity_file *tf, int mod_type);

extern void mod_change_tempo(int32 at, int bpm);
extern void mod_new_effect(int v, int period, int efx, int arg);
extern void mod_update_effect(int32 at, int v, int mul);
extern void mod_pitchbend(int at, int v, int tune);
extern void mod_start_note(int32 at, int v, int period);
extern void mod_period_move(int32 at, int v, int diff);
extern int  period2note(int period, int *finetune);

extern ModVoice *ModV;

#endif /* ___MOD_H_ */
