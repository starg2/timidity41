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


 *****************************************************************************
 * REVERB EFFECT FOR TIMIDITY-0.2i+X (Version 0.06b  1998/2/27)
 *                      Copyright (C) 1997,1998  Masaki Kiryu <mkiryu@usa.net>
 *                                   (http://www.netq.or.jp/~user/hid/masaki/)
 *
 * reverb.c  -- main reverb engine.
 ****************************************************************************
*/

/*
 * [CHANGES]
 *   0.06b (1998/2/27)
 *             - Improved presence. (NOTE: speed down reverb functions)
 *   0.06a (1998/2/15)
 *             - Speed up reverb functions.
 *             - Support for mono.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "timidity.h"
#include "controls.h"

/* Delay Buffer @65kHz */
#define REV_MAX_BUF0         344
#define REV_MAX_BUF1         682
#define REV_MAX_BUF2        2275
#define REV_MAX_BUF3        1332

#define REV_BASE_VAL0        5.3
#define REV_BASE_VAL1       10.5
#define REV_BASE_VAL2       35.0
#define REV_BASE_VAL3       20.5

#define REV_IN_LEVEL         0.7
#define REV_FBK_LEVEL        0.15
#define REV_MIX_LEVEL        0.8
#define REV_CH_MIX_LEVEL     0.9
#define REV_MONO_IN_LEVEL    0.8

#define REV_HPF_LEVEL        0.5
#define REV_LPF_LEVEL        0.45
#define REV_LPF_INPUT        0.55

int do_reverb_flag = 0;
static int  def_bufp0, bufp0;
static int  def_bufp1, bufp1;
static int  def_bufp2, bufp2;
static int  def_bufp3, bufp3;
static unsigned int  sdp0, sdp1, sdp2, sdp3;
static int32  HPF_L, HPF_R, LPF_L, LPF_R;
static int32  buf0_L[REV_MAX_BUF0], buf0_R[REV_MAX_BUF0];
static int32  buf1_L[REV_MAX_BUF1], buf1_R[REV_MAX_BUF1];
static int32  buf2_L[REV_MAX_BUF2], buf2_R[REV_MAX_BUF2];
static int32  buf3_L[REV_MAX_BUF2], buf3_R[REV_MAX_BUF2];
static int32  effect_buffer[AUDIO_BUFFER_SIZE*2];
static int32  direct_buffer[AUDIO_BUFFER_SIZE*2];
static int32  effect_buffer_size=sizeof(effect_buffer);
static int32  direct_buffer_size=sizeof(direct_buffer);
static int32  ta, tb;

/* macro functions */
#define LPF_FUNC_L  LPF_L=LPF_L*REV_LPF_LEVEL+(buf2_L[sdp2]+tb)*REV_LPF_INPUT;\
                    ta=buf3_L[sdp3];s=buf3_L[sdp3]=buf0_L[sdp0];\
                    buf0_L[sdp0]=-LPF_L;
#define LPF_FUNC_R  LPF_R=LPF_R*REV_LPF_LEVEL+(buf2_R[sdp2]+tb)*REV_LPF_INPUT;\
                    ta=buf3_R[sdp3];s=buf3_R[sdp3]=buf0_R[sdp0];\
                    buf0_R[sdp0]= LPF_R;
#define HPF_FUNC_L  t=(HPF_L+fixp)*REV_HPF_LEVEL;HPF_L=t-fixp;
#define HPF_FUNC_R  t=(HPF_R+fixp)*REV_HPF_LEVEL;HPF_R=t-fixp;
#define FUNC_L(XX)  LPF_FUNC_L;HPF_FUNC_L;buf2_L[sdp2]=(s-fixp*REV_FBK_LEVEL)\
                    *XX;tb=buf1_L[sdp1];buf1_L[sdp1]=t;
#define FUNC_R(XX)  LPF_FUNC_R;HPF_FUNC_R;buf2_R[sdp2]=(s-fixp*REV_FBK_LEVEL)\
                    *XX;tb=buf1_R[sdp1];buf1_R[sdp1]=t;
#define FUNC_INC    sdp0++;if(sdp0==bufp0)sdp0=0;sdp1++;if(sdp1==bufp1)sdp1=0;\
                    sdp2++;if(sdp2==bufp2)sdp2=0;sdp3++;if(sdp3==bufp3)sdp3=0;

/* reverb send level table data (test version) */
static FLOAT_T send_level_table[128] = {
0.00, 0.01, 0.02, 0.04, 0.05, 0.06, 0.07, 0.08,
0.10, 0.11, 0.12, 0.13, 0.14, 0.16, 0.17, 0.18,
0.19, 0.20, 0.22, 0.23, 0.24, 0.25, 0.26, 0.27,
0.29, 0.30, 0.31, 0.32, 0.33, 0.34, 0.35, 0.37,
0.38, 0.39, 0.40, 0.41, 0.42, 0.43, 0.44, 0.45,
0.46, 0.48, 0.49, 0.50, 0.51, 0.52, 0.53, 0.54,
0.55, 0.56, 0.57, 0.58, 0.59, 0.60, 0.61, 0.62,
0.63, 0.64, 0.64, 0.65, 0.66, 0.67, 0.68, 0.69,
0.70, 0.71, 0.72, 0.72, 0.73, 0.74, 0.75, 0.76,
0.76, 0.77, 0.78, 0.79, 0.79, 0.80, 0.81, 0.82,
0.82, 0.83, 0.84, 0.84, 0.85, 0.86, 0.86, 0.87,
0.87, 0.88, 0.89, 0.89, 0.90, 0.90, 0.91, 0.91,
0.92, 0.92, 0.93, 0.93, 0.94, 0.94, 0.94, 0.95,
0.95, 0.95, 0.96, 0.96, 0.96, 0.97, 0.97, 0.97,
0.98, 0.98, 0.98, 0.98, 0.99, 0.99, 0.99, 0.99,
0.99, 0.99, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00
};

void init_reverb(int32 output_rate)
{
    tb = 0;
    HPF_L = 0; HPF_R = 0;
    LPF_L = 0; LPF_R = 0;
    sdp0 = 0; sdp1 = 0; sdp2 = 0; sdp3 = 0;
    memset(buf0_L, 0, sizeof(buf0_L)); memset(buf0_R, 0, sizeof(buf0_R));
    memset(buf1_L, 0, sizeof(buf1_L)); memset(buf1_R, 0, sizeof(buf1_R));
    memset(buf2_L, 0, sizeof(buf2_L)); memset(buf2_R, 0, sizeof(buf2_R));
    memset(buf3_L, 0, sizeof(buf3_L)); memset(buf3_R, 0, sizeof(buf3_R));
    memset(effect_buffer, 0, effect_buffer_size);
    memset(direct_buffer, 0, direct_buffer_size);

    if(output_rate > 0)
    {
	def_bufp0 = bufp0 = REV_BASE_VAL0 * output_rate / 1000;
	def_bufp1 = bufp1 = REV_BASE_VAL1 * output_rate / 1000;
	def_bufp2 = bufp2 = REV_BASE_VAL2 * output_rate / 1000;
	def_bufp3 = bufp3 = REV_BASE_VAL3 * output_rate / 1000;
    }
}

void set_ch_reverb(register int32 *sbuffer, int32 n, int level)
{
    register int32  i;
    FLOAT_T send_level = send_level_table[level];

    for(i = 0; i < n; i++)
        direct_buffer[i] += sbuffer[i];
    for(i = 0; i < n; i++)
	effect_buffer[i] += sbuffer[i] * send_level;
}

void do_ch_reverb(int32 *comp, int32 n)
{
    register int32  fixp, s, t, i;

    for(i = 0; i < n; i++)
    {
	fixp = effect_buffer[i] * REV_IN_LEVEL;
	FUNC_L(REV_CH_MIX_LEVEL);
	comp[i] = ta + direct_buffer[i] * REV_IN_LEVEL;

	fixp = effect_buffer[++i] * REV_IN_LEVEL;
        FUNC_R(REV_CH_MIX_LEVEL);
	comp[i] = ta + direct_buffer[i] * REV_IN_LEVEL;

	FUNC_INC;
    }
    memset(effect_buffer, 0, effect_buffer_size);
    memset(direct_buffer, 0, direct_buffer_size);
}

void do_reverb(int32 *comp, int32 n)
{
    register int32  fixp, s, t, i;

    for(i = 0; i < n; i++)
    {
	fixp = comp[i] * REV_IN_LEVEL;
	FUNC_L(REV_MIX_LEVEL);
	comp[i] = ta + fixp;

        fixp = comp[++i] * REV_IN_LEVEL;
	FUNC_R(REV_MIX_LEVEL);
	comp[i] = ta + fixp;

	FUNC_INC;
    }
}

void do_mono_reverb(int32 *comp, int32 n)
{
    register int32  fixp, s, t, i;

    for(i = 0; i < n; i++)
    {
	fixp = comp[i] * REV_MONO_IN_LEVEL;
	FUNC_L(REV_MIX_LEVEL);
	FUNC_R(REV_MIX_LEVEL);
	comp[i] = ta + fixp;

	FUNC_INC;
    }
}

/* dummy */
void reverb_rc_event(int rc, int32 val)
{
    switch(rc)
    {
      case RC_CHANGE_REV_EFFB:
        break;
      case RC_CHANGE_REV_TIME:
        break;
    }
}

/*
void make_reverb_send_level_table()
{

}
*/
