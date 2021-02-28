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

    resample.c
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "output.h"
#include "controls.h"
#include "tables.h"
#include "resample.h"
#include "recache.h"

#include "thread.h"

///r
int opt_resample_type = DEFAULT_RESAMPLATION_NUM;
int opt_resample_param = 0; //DEFAULT ORDER
int opt_resample_filter = 0; //DEFAULT ORDER
int opt_pre_resamplation = DEFAULT_PRE_RESAMPLATION;
int opt_resample_over_sampling = 0;

const int32 mlt_fraction = (1L << FRACTION_BITS);
const int32 ml2_fraction = (2L << FRACTION_BITS);
const FLOAT_T div_fraction = (FLOAT_T)1.0 / (FLOAT_T)(1L << FRACTION_BITS);

#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
#define OUT_INT16  DIV_15BIT
#define OUT_INT32  DIV_31BIT
#define OUT_FLOAT  1.0
#define OUT_DOUBLE 1.0
#else // DATA_T int32
#define OUT_INT16  1
#define OUT_INT32  1
#define OUT_FLOAT  M_15BIT
#define OUT_DOUBLE M_15BIT
#endif


/*
リサンプル部分限定
実用範囲ではFRACTION_BITSを含まないサンプル点にはint64不要なので
length 30[bit]=1073741824[samples] 22369[sec]@48[kHz] 
length*2まで扱えれば十分
*/
#if (SAMPLE_LENGTH_BITS <= 32)
#define spos_t int32
#else
#define spos_t splen_t
#endif

/*
リサンプル部分限定
サンプル小数点部分は frac*4まで扱えれば十分
*/
#if (FRACTION_BITS <= 28)
#define fract_t int32
#else
#define fract_t int64
#endif

#define USE_PERMUTEX2

static inline int32 imuldiv_fraction(int32 a, int32 b) {
#if (OPT_MODE == 1) && defined(SUPPORT_ASM_INTEL) /* fixed-point implementation */
#if defined(SUPPORT_ASM_INTEL)
	_asm {
		mov eax, a
		mov edx, b
		imul edx
		shr eax, FRACTION_BITS
		shl edx, (32 - FRACTION_BITS)
		or  eax, edx
	}
#else	
	return (a * b) >> FRACTION_BITS;
#endif	
#else	
	return (a * b) >> FRACTION_BITS;
#endif // (OPT_MODE == 1)
}

static inline int32 imuldiv_fraction_int32(int32 a, int32 b) {
#if (OPT_MODE == 1) && defined(SUPPORT_ASM_INTEL) /* fixed-point implementation */
#if defined(SUPPORT_ASM_INTEL)
	_asm {
		mov eax, a
		mov edx, b
		imul edx
		shr eax, FRACTION_BITS
		shl edx, (32 - FRACTION_BITS)
		or  eax, edx
	}
#else	
	return (int32)(((int64)a * b) >> FRACTION_BITS);
#endif	
#else	
	return (int32)(((int64)a * b) >> FRACTION_BITS);
#endif // (OPT_MODE == 1)
}

/* No interpolation -- Earplugs recommended for maximum listening enjoyment */
static DATA_T resample_none(const sample_t *src, splen_t ofs, resample_rec_t *rec)
{
    return (DATA_T)src[ofs >> FRACTION_BITS] * OUT_INT16;
}

static DATA_T resample_none_int32(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const int32 *src = (const int32*)srci;
    return (DATA_T)src[ofs >> FRACTION_BITS] * OUT_INT32;
}

static DATA_T resample_none_float(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const float *src = (const float*)srci;
    return (DATA_T)src[ofs >> FRACTION_BITS] * OUT_FLOAT;
}

static DATA_T resample_none_double(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const double *src = (const double*)srci;
	return (DATA_T)src[ofs >> FRACTION_BITS] * OUT_DOUBLE;
}


/* Simple linear interpolation */
static inline DATA_T resample_linear(const sample_t *src, splen_t ofs, resample_rec_t *rec)
{
	const spos_t ofsi = ofs >> FRACTION_BITS;
	fract_t ofsf = ofs & FRACTION_MASK;
    int32 v1 = src[ofsi], v2 = src[ofsi + 1];
// sample_t int8
#if defined(LOOKUP_HACK) && defined(LOOKUP_INTERPOLATION)
    return (sample_t)(v1 + (iplookup[(((v2 - v1) << 5) & 0x03FE0) | (ofsf >> (FRACTION_BITS-5))]));
// sample_t int16
#elif defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
    return ((FLOAT_T)v1 + (FLOAT_T)(v2 - v1) * (FLOAT_T)ofsf * div_fraction) * OUT_INT16; // FLOAT_T
#else // DATA_T_IN32
//	return v1 + (((v2 - v1) * ofsf) >> FRACTION_BITS);
	return v1 + imuldiv_fraction(v2 - v1, ofsf);
#endif
}

static DATA_T resample_linear_int32(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
	const int32 *src = (const int32*)srci;
	const spos_t ofsi = ofs >> FRACTION_BITS;
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
    FLOAT_T v1 = src[ofsi], v2 = src[ofsi + 1], fp = (ofs & FRACTION_MASK);
    return (v1 + (v2 - v1) * fp * div_fraction) * OUT_INT32; // FLOAT_T
#else // DATA_T_IN32
	fract_t ofsf = ofs & FRACTION_MASK;
    int32 v1 = src[ofsi] >> 16, v2 = src[ofsi + 1] >> 16;
	return v1 + imuldiv_fraction(v2 - v1, ofsf);
#endif
}

static inline DATA_T resample_linear_float(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const float *src = (const float*)srci;
	const spos_t ofsi = ofs >> FRACTION_BITS;
    FLOAT_T v1 = src[ofsi], v2 = src[ofsi + 1], fp = (ofs & FRACTION_MASK);
    return (v1 + (v2 - v1) * fp * div_fraction) * OUT_FLOAT; // FLOAT_T
}

static DATA_T resample_linear_double(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const double *src = (const double*)srci;
	const spos_t ofsi = ofs >> FRACTION_BITS;
    FLOAT_T v1 = src[ofsi], v2 = src[ofsi + 1], fp = (ofs & FRACTION_MASK);
    return (v1 + (v2 - v1) * fp * div_fraction) * OUT_DOUBLE; // FLOAT_T
}


/* 4-point interpolation by cubic spline curve. */

static DATA_T resample_cspline(const sample_t *src, splen_t ofs, resample_rec_t *rec)
{
    const spos_t ofsi = ofs >> FRACTION_BITS;
    fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofsls = rec->loop_start >> FRACTION_BITS;
    const spos_t ofsle = rec->loop_end >> FRACTION_BITS;
	spos_t ofstmp, len;
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT) || (FRACTION_BITS > 12)
	FLOAT_T v[4], temp1, temp2;
#else // DATA_T_IN32
	int32 v[4], temp1, temp2;
#endif
	int32 i, dir;

	switch(rec->mode){
	case RESAMPLE_MODE_PLAIN:
		if(ofsi < 1)
			goto do_linear;
		break; // normal
	case RESAMPLE_MODE_LOOP:
		if(ofsi < ofsls){
			if(ofsi < 1)
				goto do_linear;
			if((ofsi + 2) < ofsle)
				break; // normal
		}else if(((ofsi + 2) < ofsle) && ((ofsi - 1) >= ofsls))
			break; // normal		
		len = ofsle - ofsls; // loop_length
		ofstmp = ofsi - 1;
		if(ofstmp < ofsls) {ofstmp += len;} // if loop_length == data_length need			
		for(i = 0; i < 4; i++){
			v[i] = src[ofstmp];			
			if((++ofstmp) > ofsle) {ofstmp -= len;} // -= loop_length , jump loop_start
		}
		goto loop_ofs;
		break;
	case RESAMPLE_MODE_BIDIR_LOOP:		
		if(rec->increment >= 0){ // normal dir
			if(ofsi < ofsls){
				if(ofsi < 1)
					goto do_linear;
				if((ofsi + 2) < ofsle)
					break; // normal
			}else if(((ofsi + 2) < ofsle) && ((ofsi - 1) >= ofsls))
				break; // normal
			dir = 1;
			ofstmp = ofsi - 1;
			if(ofstmp < ofsls){ // if loop_length == data_length need				
				ofstmp = (ofsls << 1) - ofstmp;
				dir = -1;
			}				
		}else{ // reverse dir
			dir = -1;
			ofstmp = ofsi + 1;
			if(ofstmp > ofsle){ // if loop_length == data_length need				
				ofstmp = (ofsle << 1) - ofstmp;
				dir = 1;
			}
			ofsf = mlt_fraction - ofsf;
		}
		for(i = 0; i < 4; i++){
			v[i] = src[ofstmp];			
			ofstmp += dir;
			if(dir < 0){ // -
				if(ofstmp <= ofsls) {dir = 1;}
			}else{ // +
				if(ofstmp >= ofsle) {dir = -1;}
			}
		}
		goto loop_ofs;
		break;
	}
normal_ofs:
	v[0] = src[ofsi - 1];
    v[1] = src[ofsi];
    v[2] = src[ofsi + 1];	
	v[3] = src[ofsi + 2];
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT) || (FRACTION_BITS > 12)
loop_ofs:	
	temp1 = v[1];
	temp2 = v[2];
	v[1] = 5.0 * v[0] - 11.0 * temp1 + 7.0 * temp2 - v[3];
	v[2] = 5.0 * v[3] - 11.0 * temp2 + 7.0 * temp1 - v[0];
	v[1] *= DIV_4;
	v[2] *= DIV_4;
	v[1] *= (FLOAT_T)ofsf * div_fraction;
	v[2] *= (FLOAT_T)(ofsf + mlt_fraction) * div_fraction;
	v[1] *= (FLOAT_T)(ofsf - ml2_fraction) * div_fraction;
	v[2] *= (FLOAT_T)(ofsf - mlt_fraction) * div_fraction;
	v[1] += 6.0 * temp1;
	v[2] += 6.0 * temp2;
	v[1] *= (FLOAT_T)(mlt_fraction - ofsf);
	v[2] *= ofsf;
	temp1 = (v[1] + v[2]) * DIV_6 * div_fraction;
	return temp1 * OUT_INT16; // FLOAT_T
do_linear:
    v[1] = src[ofsi];
	v[2] = (int32)(src[ofsi + 1]) - (int32)(src[ofsi]);
    return (v[1] + v[2] * (FLOAT_T)ofsf * div_fraction) * OUT_INT16; // FLOAT_T
#else // DATA_T_IN32
loop_ofs:
	temp1 = v[1];
	temp2 = v[2];
	//v[2] = (6 * v[2] + ((((5 * v[3] - 11 * v[2] + 7 * v[1] - v[0]) >> 2)
	//	* (ofsf + (1L << FRACTION_BITS)) >> FRACTION_BITS)
	//	* (ofsf - (1L << FRACTION_BITS)) >> FRACTION_BITS))
	//    * ofsf;
	//v[1] = ((6 * v[1]+((((5 * v[0] - 11 * v[1] + 7 * temp2 - v[3]) >> 2)
	//	* ofsf >> FRACTION_BITS)
	//	* (ofsf - (2L << FRACTION_BITS)) >> FRACTION_BITS))
	//	* ((1L << FRACTION_BITS) - ofsf));
	//temp1 = (v[1] + v[2]) / (6L << FRACTION_BITS);
	v[1] = (5 * v[0] - 11 * temp1 + 7 * temp2 - v[3]) >> 2;
	v[2] = (5 * v[3] - 11 * temp2 + 7 * temp1 - v[0]) >> 2;
	v[1] = imuldiv_fraction(v[1], ofsf);
	v[2] = imuldiv_fraction(v[2], ofsf + mlt_fraction);
	v[1] = imuldiv_fraction(v[1], ofsf - ml2_fraction);
	v[2] = imuldiv_fraction(v[2], ofsf - mlt_fraction);
	v[1] = (6 * temp1 + v[1]) * (mlt_fraction - ofsf);
	v[2] = (6 * temp2 + v[2]) * ofsf;
	temp1 = (v[1] + v[2]) / (6L << FRACTION_BITS);
	return temp1;
do_linear:
    v[1] = src[ofsi];
	v[2] = src[ofsi + 1];
//	return v[1] + ((v[2] - v[1]) * ofsf) >> FRACTION_BITS;
	return v[1] + imuldiv_fraction(v[2] - v[1], ofsf);
#endif
}

static DATA_T resample_cspline_int32(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const int32 *src = (const int32*)srci;
    const spos_t ofsi = ofs >> FRACTION_BITS;
    fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofsls = rec->loop_start >> FRACTION_BITS;
    const spos_t ofsle = rec->loop_end >> FRACTION_BITS;
    spos_t ofstmp, len;
	FLOAT_T v[4], temp1, temp2;
	int32 i, dir;
	
	switch(rec->mode){
	case RESAMPLE_MODE_PLAIN:
		if(ofsi < 1)
			goto do_linear;
		break; // normal
	case RESAMPLE_MODE_LOOP:
		if(ofsi < ofsls){
			if(ofsi < 1)
				goto do_linear;
			if((ofsi + 2) < ofsle)
				break; // normal
		}else if(((ofsi + 2) < ofsle) && ((ofsi - 1) >= ofsls))
			break; // normal		
		len = ofsle - ofsls; // loop_length
		ofstmp = ofsi - 1;
		if(ofstmp < ofsls) {ofstmp += len;} // if loop_length == data_length need			
		for(i = 0; i < 4; i++){
			v[i] = src[ofstmp];			
			if((++ofstmp) > ofsle) {ofstmp -= len;} // -= loop_length , jump loop_start
		}
		goto loop_ofs;
		break;
	case RESAMPLE_MODE_BIDIR_LOOP:		
		if(rec->increment >= 0){ // normal dir
			if(ofsi < ofsls){
				if(ofsi < 1)
					goto do_linear;
				if((ofsi + 2) < ofsle)
					break; // normal
			}else if(((ofsi + 2) < ofsle) && ((ofsi - 1) >= ofsls))
				break; // normal
			dir = 1;
			ofstmp = ofsi - 1;
			if(ofstmp < ofsls){ // if loop_length == data_length need				
				ofstmp = (ofsls << 1) - ofstmp;
				dir = -1;
			}			
		}else{ // reverse dir
			dir = -1;
			ofstmp = ofsi + 1;
			if(ofstmp > ofsle){ // if loop_length == data_length need				
				ofstmp = (ofsle << 1) - ofstmp;
				dir = 1;
			}
			ofsf = mlt_fraction - ofsf;
		}
		for(i = 0; i < 4; i++){
			v[i] = src[ofstmp];			
			ofstmp += dir;
			if(dir < 0){ // -
				if(ofstmp <= ofsls) {dir = 1;}
			}else{ // +
				if(ofstmp >= ofsle) {dir = -1;}
			}
		}
		goto loop_ofs;
		break;
	}
normal_ofs:
	v[0] = src[ofsi - 1];
    v[1] = src[ofsi];
    v[2] = src[ofsi + 1];	
	v[3] = src[ofsi + 2];
loop_ofs:
	temp1 = v[1];
	temp2 = v[2];
	v[2] = 5.0 * v[3] - 11.0 * temp2 + 7.0 * temp1 - v[0];
	v[2] *= DIV_4;
	v[2] *= (FLOAT_T)(ofsf + mlt_fraction) * div_fraction;
	v[2] *= (FLOAT_T)(ofsf - mlt_fraction) * div_fraction;
	v[2] += 6.0 * temp2;
	v[2] *= ofsf;
	v[1] = 5.0 * v[0] - 11.0 * temp1 + 7.0 * temp2 - v[3];
	v[1] *= DIV_4;
	v[1] *= (FLOAT_T)ofsf * div_fraction;
	v[1] *= (FLOAT_T)(ofsf - ml2_fraction) * div_fraction;
	v[1] += 6.0 * temp1;
	v[1] *= (FLOAT_T)(mlt_fraction - ofsf);
	temp1 = (v[1] + v[2]) *  DIV_6 * div_fraction;
	return temp1 * OUT_INT32; // FLOAT_T
do_linear:
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
    v[1] = src[ofsi];
	v[2] = src[ofsi + 1];
    return (v[1] + (v[2] - v[1]) * (FLOAT_T)ofsf * div_fraction) * OUT_INT32; // FLOAT_T
#else // DATA_T_IN32
	v[1] = src[ofsi];
	v[2] = src[ofsi + 1];	
	return v[1] + imuldiv_fraction_int32(v[2] - v[1], ofsf);
#endif
}

static DATA_T resample_cspline_float(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const float *src = (const float*)srci;
    const spos_t ofsi = ofs >> FRACTION_BITS;
    fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofsls = rec->loop_start >> FRACTION_BITS;
    const spos_t ofsle = rec->loop_end >> FRACTION_BITS;
    spos_t ofstmp, len;
	FLOAT_T v[4], temp1, temp2;
	int32 i, dir;
	
	switch(rec->mode){
	case RESAMPLE_MODE_PLAIN:
		if(ofsi < 1)
			goto do_linear;
		break; // normal
	case RESAMPLE_MODE_LOOP:
		if(ofsi < ofsls){
			if(ofsi < 1)
				goto do_linear;
			if((ofsi + 2) < ofsle)
				break; // normal
		}else if(((ofsi + 2) < ofsle) && ((ofsi - 1) >= ofsls))
			break; // normal		
		len = ofsle - ofsls; // loop_length
		ofstmp = ofsi - 1;
		if(ofstmp < ofsls) {ofstmp += len;} // if loop_length == data_length need			
		for(i = 0; i < 4; i++){
			v[i] = src[ofstmp];			
			if((++ofstmp) > ofsle) {ofstmp -= len;} // -= loop_length , jump loop_start
		}
		goto loop_ofs;
		break;
	case RESAMPLE_MODE_BIDIR_LOOP:		
		if(rec->increment >= 0){ // normal dir
			if(ofsi < ofsls){
				if(ofsi < 1)
					goto do_linear;
				if((ofsi + 2) < ofsle)
					break; // normal
			}else if(((ofsi + 2) < ofsle) && ((ofsi - 1) >= ofsls))
				break; // normal
			dir = 1;
			ofstmp = ofsi - 1;
			if(ofstmp < ofsls){ // if loop_length == data_length need				
				ofstmp = (ofsls << 1) - ofstmp;
				dir = -1;
			}			
		}else{ // reverse dir
			dir = -1;
			ofstmp = ofsi + 1;
			if(ofstmp > ofsle){ // if loop_length == data_length need				
				ofstmp = (ofsle << 1) - ofstmp;
				dir = 1;
			}
			ofsf = mlt_fraction - ofsf;
		}
		for(i = 0; i < 4; i++){
			v[i] = src[ofstmp];			
			ofstmp += dir;
			if(dir < 0){ // -
				if(ofstmp <= ofsls) {dir = 1;}
			}else{ // +
				if(ofstmp >= ofsle) {dir = -1;}
			}
		}
		goto loop_ofs;
		break;
	}
normal_ofs:
	v[0] = src[ofsi - 1];
    v[1] = src[ofsi];
    v[2] = src[ofsi + 1];	
	v[3] = src[ofsi + 2];
loop_ofs:
	temp1 = v[1];
	temp2 = v[2];
	v[2] = 5.0 * v[3] - 11.0 * temp2 + 7.0 * temp1 - v[0];
	v[2] *= DIV_4;
	v[2] *= (FLOAT_T)(ofsf + mlt_fraction) * div_fraction;
	v[2] *= (FLOAT_T)(ofsf - mlt_fraction) * div_fraction;
	v[2] += 6.0 * temp2;
	v[2] *= ofsf;
	v[1] = 5.0 * v[0] - 11.0 * temp1 + 7.0 * temp2 - v[3];
	v[1] *= DIV_4;
	v[1] *= (FLOAT_T)ofsf * div_fraction;
	v[1] *= (FLOAT_T)(ofsf - ml2_fraction) * div_fraction;
	v[1] += 6.0 * temp1;
	v[1] *= (FLOAT_T)(mlt_fraction - ofsf);
	temp1 = (v[1] + v[2]) *  DIV_6 * div_fraction;	
	return temp1 * OUT_FLOAT;
do_linear:
    v[1] = src[ofsi];
    v[2] = src[ofsi + 1];	
	return (v[1] + (v[2] - v[1]) * (FLOAT_T)ofsf * div_fraction) * OUT_FLOAT;
}

static DATA_T resample_cspline_double(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const double *src = (const double*)srci;
    const spos_t ofsi = ofs >> FRACTION_BITS;
    fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofsls = rec->loop_start >> FRACTION_BITS;
    const spos_t ofsle = rec->loop_end >> FRACTION_BITS;
    spos_t ofstmp, len;
	FLOAT_T v[4], temp1, temp2;
	int32 i, dir;
	
	switch(rec->mode){
	case RESAMPLE_MODE_PLAIN:
		if(ofsi < 1)
			goto do_linear;
		break; // normal
	case RESAMPLE_MODE_LOOP:
		if(ofsi < ofsls){
			if(ofsi < 1)
				goto do_linear;
			if((ofsi + 2) < ofsle)
				break; // normal
		}else if(((ofsi + 2) < ofsle) && ((ofsi - 1) >= ofsls))
			break; // normal		
		len = ofsle - ofsls; // loop_length
		ofstmp = ofsi - 1;
		if(ofstmp < ofsls) {ofstmp += len;} // if loop_length == data_length need			
		for(i = 0; i < 4; i++){
			v[i] = src[ofstmp];			
			if((++ofstmp) > ofsle) {ofstmp -= len;} // -= loop_length , jump loop_start
		}
		goto loop_ofs;
		break;
	case RESAMPLE_MODE_BIDIR_LOOP:		
		if(rec->increment >= 0){ // normal dir
			if(ofsi < ofsls){
				if(ofsi < 1)
					goto do_linear;
				if((ofsi + 2) < ofsle)
					break; // normal
			}else if(((ofsi + 2) < ofsle) && ((ofsi - 1) >= ofsls))
				break; // normal
			dir = 1;
			ofstmp = ofsi - 1;
			if(ofstmp < ofsls){ // if loop_length == data_length need				
				ofstmp = (ofsls << 1) - ofstmp;
				dir = -1;
			}			
		}else{ // reverse dir
			dir = -1;
			ofstmp = ofsi + 1;
			if(ofstmp > ofsle){ // if loop_length == data_length need				
				ofstmp = (ofsle << 1) - ofstmp;
				dir = 1;
			}
			ofsf = mlt_fraction - ofsf;
		}
		for(i = 0; i < 4; i++){
			v[i] = src[ofstmp];			
			ofstmp += dir;
			if(dir < 0){ // -
				if(ofstmp <= ofsls) {dir = 1;}
			}else{ // +
				if(ofstmp >= ofsle) {dir = -1;}
			}
		}
		goto loop_ofs;
		break;
	}
normal_ofs:
	v[0] = src[ofsi - 1];
    v[1] = src[ofsi];
    v[2] = src[ofsi + 1];	
	v[3] = src[ofsi + 2];
loop_ofs:
	temp1 = v[1];
	temp2 = v[2];
	v[2] = 5.0 * v[3] - 11.0 * temp2 + 7.0 * temp1 - v[0];
	v[2] *= DIV_4;
	v[2] *= (FLOAT_T)(ofsf + mlt_fraction) * div_fraction;
	v[2] *= (FLOAT_T)(ofsf - mlt_fraction) * div_fraction;
	v[2] += 6.0 * temp2;
	v[2] *= ofsf;
	v[1] = 5.0 * v[0] - 11.0 * temp1 + 7.0 * temp2 - v[3];
	v[1] *= DIV_4;
	v[1] *= (FLOAT_T)ofsf * div_fraction;
	v[1] *= (FLOAT_T)(ofsf - ml2_fraction) * div_fraction;
	v[1] += 6.0 * temp1;
	v[1] *= (FLOAT_T)(mlt_fraction - ofsf);
	temp1 = (v[1] + v[2]) *  DIV_6 * div_fraction;
	return temp1 * OUT_DOUBLE;
do_linear:
    v[1] = src[ofsi];
    v[2] = src[ofsi + 1];	
	return (v[1] + (v[2] - v[1]) * (FLOAT_T)ofsf * div_fraction) * OUT_DOUBLE;
}

/* 4-point interpolation by Lagrange method.
   Lagrange is now faster than C-spline.  Both have about the same accuracy,
   so choose Lagrange over C-spline, since it is faster.  Technically, it is
   really a 3rd order Newton polynomial (whereas the old Lagrange truely was
   the Lagrange form of the polynomial).  Both Newton and Lagrange forms
   yield the same numerical results, but the Newton form is faster.  Since
   n'th order Newton interpolaiton is resample_newton(), it made sense to
   just keep this labeled as resample_lagrange(), even if it really is the
   Newton form of the polynomial. */

static inline DATA_T resample_lagrange(const sample_t *src, splen_t ofs, resample_rec_t *rec)
{
    const spos_t ofsi = ofs >> FRACTION_BITS;
    fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofsls = rec->loop_start >> FRACTION_BITS;
    const spos_t ofsle = rec->loop_end >> FRACTION_BITS;
    spos_t ofstmp, len;
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
    FLOAT_T v[4], tmp;
#else // DATA_T_IN32
	int32 v[4], tmp;
#endif
	int32 i, dir;

	switch(rec->mode){
	case RESAMPLE_MODE_PLAIN:
		if(ofsi < 1)
			goto do_linear;
		break; // normal
	case RESAMPLE_MODE_LOOP:
		if(ofsi < ofsls){
			if(ofsi < 1)
				goto do_linear;
			if((ofsi + 2) < ofsle)
				break; // normal
		}else if(((ofsi + 2) < ofsle) && ((ofsi - 1) >= ofsls))
			break; // normal		
		len = ofsle - ofsls; // loop_length
		ofstmp = ofsi - 1;
		if(ofstmp < ofsls) {ofstmp += len;} // if loop_length == data_length need			
		for(i = 0; i < 4; i++){
			v[i] = src[ofstmp];			
			if((++ofstmp) > ofsle) {ofstmp -= len;} // -= loop_length , jump loop_start
		}
		goto loop_ofs;
		break;
	case RESAMPLE_MODE_BIDIR_LOOP:			
		if(rec->increment >= 0){ // normal dir
			if(ofsi < ofsls){
				if(ofsi < 1)
					goto do_linear;
				if((ofsi + 2) < ofsle)
					break; // normal
			}else if(((ofsi + 2) < ofsle) && ((ofsi - 1) >= ofsls))
				break; // normal
			dir = 1;
			ofstmp = ofsi - 1;
			if(ofstmp < ofsls){ // if loop_length == data_length need				
				ofstmp = (ofsls << 1) - ofstmp;
				dir = -1;
			}			
		}else{ // reverse dir
			dir = -1;
			ofstmp = ofsi + 1;
			if(ofstmp > ofsle){ // if loop_length == data_length need				
				ofstmp = (ofsle << 1) - ofstmp;
				dir = 1;
			}
			ofsf = mlt_fraction - ofsf;
		}
		for(i = 0; i < 4; i++){
			v[i] = src[ofstmp];			
			ofstmp += dir;
			if(dir < 0){ // -
				if(ofstmp <= ofsls) {dir = 1;}
			}else{ // +
				if(ofstmp >= ofsle) {dir = -1;}
			}
		}
		goto loop_ofs;
		break;
	}
normal_ofs:
	v[0] = src[ofsi - 1];
    v[1] = src[ofsi];
    v[2] = src[ofsi + 1];	
	v[3] = src[ofsi + 2];
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
loop_ofs:
	ofsf += mlt_fraction;
	tmp = v[1] - v[0];
	v[3] += -3 * v[2] + 3 * v[1] - v[0];
	v[3] *= (FLOAT_T)(ofsf - ml2_fraction) * DIV_6 * div_fraction;
	v[3] += v[2] - v[1] - tmp;
	v[3] *= (FLOAT_T)(ofsf - mlt_fraction) * DIV_2 * div_fraction;
	v[3] += tmp;
	v[3] *= (FLOAT_T)ofsf * div_fraction;
	v[3] += v[0];
	return v[3] * OUT_INT16;
do_linear:
    v[1] = src[ofsi];
	v[2] = (int32)(src[ofsi + 1]) - (int32)(src[ofsi]);
    return (v[1] + v[2] * (FLOAT_T)ofsf * div_fraction) * OUT_INT16; // FLOAT_T
#else // DATA_T_IN32
loop_ofs:
	ofsf += mlt_fraction;
	tmp = v[1] - v[0];
	//v[3] += -3*v[2] + 3*v[1] - v[0];
	//v[3] *= (ofsf - ml2_fraction) / 6;
	//v[3] >>= FRACTION_BITS;
	//v[3] += v[2] - v[1] - tmp;
	//v[3] *= (ofsf - mlt_fraction) >> 1;
	//v[3] >>= FRACTION_BITS;
	//v[3] += tmp;
	//v[3] *= ofsf;
	//v[3] >>= FRACTION_BITS;
	//v[3] += v[0];
	v[3] += -3*v[2] + 3*v[1] - v[0];
	v[3] = imuldiv_fraction(v[3], (ofsf - ml2_fraction) / 6);
	v[3] += v[2] - v[1] - tmp;
	v[3] = imuldiv_fraction(v[3], (ofsf - mlt_fraction) >> 1);
	v[3] += tmp;
	v[3] = imuldiv_fraction(v[3], ofsf);
	v[3] += v[0];
	return v[3];
do_linear:
    v[1] = src[ofsi];
	v[2] = src[ofsi + 1];
//	return v[1] + ((v[2] - v[1]) * ofsf >> FRACTION_BITS) >> FRACTION_BITS;
	return v[1] + imuldiv_fraction(v[2] - v[1], ofsf);
#endif
}

static DATA_T resample_lagrange_int32(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const int32 *src = (const int32*)srci;
    const spos_t ofsi = ofs >> FRACTION_BITS;
    fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofsls = rec->loop_start >> FRACTION_BITS;
    const spos_t ofsle = rec->loop_end >> FRACTION_BITS;
    spos_t ofstmp, len;
	FLOAT_T v[4], tmp;
	int32 i, dir;

	switch(rec->mode){
	case RESAMPLE_MODE_PLAIN:
		if(ofsi < 1)
			goto do_linear;
		break; // normal
	case RESAMPLE_MODE_LOOP:
		if(ofsi < ofsls){
			if(ofsi < 1)
				goto do_linear;
			if((ofsi + 2) < ofsle)
				break; // normal
		}else if(((ofsi + 2) < ofsle) && ((ofsi - 1) >= ofsls))
			break; // normal		
		len = ofsle - ofsls; // loop_length
		ofstmp = ofsi - 1;
		if(ofstmp < ofsls) {ofstmp += len;} // if loop_length == data_length need			
		for(i = 0; i < 4; i++){
			v[i] = src[ofstmp];			
			if((++ofstmp) > ofsle) {ofstmp -= len;} // -= loop_length , jump loop_start
		}	
		goto loop_ofs;
		break;
	case RESAMPLE_MODE_BIDIR_LOOP:		
		if(rec->increment >= 0){ // normal dir
			if(ofsi < ofsls){
				if(ofsi < 1)
					goto do_linear;
				if((ofsi + 2) < ofsle)
					break; // normal
			}else if(((ofsi + 2) < ofsle) && ((ofsi - 1) >= ofsls))
				break; // normal
			dir = 1;
			ofstmp = ofsi - 1;
			if(ofstmp < ofsls){ // if loop_length == data_length need				
				ofstmp = (ofsls << 1) - ofstmp;
				dir = -1;
			}				
		}else{ // reverse dir
			dir = -1;
			ofstmp = ofsi + 1;
			if(ofstmp > ofsle){ // if loop_length == data_length need				
				ofstmp = (ofsle << 1) - ofstmp;
				dir = 1;
			}
			ofsf = mlt_fraction - ofsf;
		}
		for(i = 0; i < 4; i++){
			v[i] = src[ofstmp];			
			ofstmp += dir;
			if(dir < 0){ // -
				if(ofstmp <= ofsls) {dir = 1;}
			}else{ // +
				if(ofstmp >= ofsle) {dir = -1;}
			}
		}
		goto loop_ofs;
		break;
	}
normal_ofs:
	v[0] = src[ofsi - 1];
    v[1] = src[ofsi];
    v[2] = src[ofsi + 1];	
	v[3] = src[ofsi + 2];
loop_ofs:
	ofsf += mlt_fraction;
	tmp = v[1] - v[0];
	v[3] += -3 * v[2] + 3 * v[1] - v[0];
	v[3] *= (FLOAT_T)(ofsf - ml2_fraction) * DIV_6 * div_fraction;
	v[3] += v[2] - v[1] - tmp;
	v[3] *= (FLOAT_T)(ofsf - mlt_fraction) * DIV_2 * div_fraction;
	v[3] += tmp;
	v[3] *= (FLOAT_T)ofsf * div_fraction;
	v[3] += v[0];
	return v[3] * OUT_INT32;
do_linear:
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
    v[1] = src[ofsi];
	v[2] = src[ofsi + 1];
    return (v[1] + (v[2] - v[1]) * (FLOAT_T)ofsf * div_fraction) * OUT_INT32; // FLOAT_T
#else // DATA_T_IN32
	v[1] = src[ofsi];
	v[2] = src[ofsi + 1];	
	return v[1] + imuldiv_fraction_int32(v[2] - v[1], ofsf);
#endif
}

static inline DATA_T resample_lagrange_float(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const float *src = (const float*)srci;
    const spos_t ofsi = ofs >> FRACTION_BITS;
    fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofsls = rec->loop_start >> FRACTION_BITS;
    const spos_t ofsle = rec->loop_end >> FRACTION_BITS;
    spos_t ofstmp, len;
	FLOAT_T v[4], tmp;
	int32 i, dir;

	switch(rec->mode){
	case RESAMPLE_MODE_PLAIN:
		if(ofsi < 1)
			goto do_linear;
		break; // normal
	case RESAMPLE_MODE_LOOP:
		if(ofsi < ofsls){
			if(ofsi < 1)
				goto do_linear;
			if((ofsi + 2) < ofsle)
				break; // normal
		}else if(((ofsi + 2) < ofsle) && ((ofsi - 1) >= ofsls))
			break; // normal		
		len = ofsle - ofsls; // loop_length
		ofstmp = ofsi - 1;
		if(ofstmp < ofsls) {ofstmp += len;} // if loop_length == data_length need			
		for(i = 0; i < 4; i++){
			v[i] = src[ofstmp];			
			if((++ofstmp) > ofsle) {ofstmp -= len;} // -= loop_length , jump loop_start
		}
		goto loop_ofs;
		break;
	case RESAMPLE_MODE_BIDIR_LOOP:			
		if(rec->increment >= 0){ // normal dir
			if(ofsi < ofsls){
				if(ofsi < 1)
					goto do_linear;
				if((ofsi + 2) < ofsle)
					break; // normal
			}else if(((ofsi + 2) < ofsle) && ((ofsi - 1) >= ofsls))
				break; // normal
			dir = 1;
			ofstmp = ofsi - 1;
			if(ofstmp < ofsls){ // if loop_length == data_length need				
				ofstmp = (ofsls << 1) - ofstmp;
				dir = -1;
			}					
		}else{ // reverse dir
			dir = -1;
			ofstmp = ofsi + 1;
			if(ofstmp > ofsle){ // if loop_length == data_length need				
				ofstmp = (ofsle << 1) - ofstmp;
				dir = 1;
			}
			ofsf = mlt_fraction - ofsf;
		}
		for(i = 0; i < 4; i++){
			v[i] = src[ofstmp];			
			ofstmp += dir;
			if(dir < 0){ // -
				if(ofstmp <= ofsls) {dir = 1;}
			}else{ // +
				if(ofstmp >= ofsle) {dir = -1;}
			}
		}
		goto loop_ofs;
		break;
	}
normal_ofs:
	v[0] = src[ofsi - 1];
    v[1] = src[ofsi];
    v[2] = src[ofsi + 1];	
	v[3] = src[ofsi + 2];
loop_ofs:
	ofsf += mlt_fraction;
	tmp = v[1] - v[0];
	v[3] += -3 * v[2] + 3 * v[1] - v[0];
	v[3] *= (FLOAT_T)(ofsf - ml2_fraction) * DIV_6 * div_fraction;
	v[3] += v[2] - v[1] - tmp;
	v[3] *= (FLOAT_T)(ofsf - mlt_fraction) * DIV_2 * div_fraction;
	v[3] += tmp;
	v[3] *= (FLOAT_T)ofsf * div_fraction;
	v[3] += v[0];
	return v[3] * OUT_FLOAT;
do_linear:
    v[1] = src[ofsi];
    v[2] = src[ofsi + 1];	
	return (v[1] + (v[2] - v[1]) * (FLOAT_T)ofsf * div_fraction) * OUT_FLOAT;
}

static DATA_T resample_lagrange_double(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const double *src = (const double*)srci;
    const spos_t ofsi = ofs >> FRACTION_BITS;
    fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofsls = rec->loop_start >> FRACTION_BITS;
    const spos_t ofsle = rec->loop_end >> FRACTION_BITS;
    spos_t ofstmp, len;
	FLOAT_T v[4], tmp;
	int32 i, dir;

	switch(rec->mode){
	case RESAMPLE_MODE_PLAIN:
		if(ofsi < 1)
			goto do_linear;
		break; // normal
	case RESAMPLE_MODE_LOOP:
		if(ofsi < ofsls){
			if(ofsi < 1)
				goto do_linear;
			if((ofsi + 2) < ofsle)
				break; // normal
		}else if(((ofsi + 2) < ofsle) && ((ofsi - 1) >= ofsls))
			break; // normal		
		len = ofsle - ofsls; // loop_length
		ofstmp = ofsi - 1;
		if(ofstmp < ofsls) {ofstmp += len;} // if loop_length == data_length need			
		for(i = 0; i < 4; i++){
			v[i] = src[ofstmp];			
			if((++ofstmp) > ofsle) {ofstmp -= len;} // -= loop_length , jump loop_start
		}
		goto loop_ofs;
		break;
	case RESAMPLE_MODE_BIDIR_LOOP:		
		if(rec->increment >= 0){ // normal dir
			if(ofsi < ofsls){
				if(ofsi < 1)
					goto do_linear;
				if((ofsi + 2) < ofsle)
					break; // normal
			}else if(((ofsi + 2) < ofsle) && ((ofsi - 1) >= ofsls))
				break; // normal
			dir = 1;
			ofstmp = ofsi - 1;
			if(ofstmp < ofsls){ // if loop_length == data_length need				
				ofstmp = (ofsls << 1) - ofstmp;
				dir = -1;
			}				
		}else{ // reverse dir
			dir = -1;
			ofstmp = ofsi + 1;
			if(ofstmp > ofsle){ // if loop_length == data_length need				
				ofstmp = (ofsle << 1) - ofstmp;
				dir = 1;
			}
			ofsf = mlt_fraction - ofsf;
		}
		for(i = 0; i < 4; i++){
			v[i] = src[ofstmp];			
			ofstmp += dir;
			if(dir < 0){ // -
				if(ofstmp <= ofsls) {dir = 1;}
			}else{ // +
				if(ofstmp >= ofsle) {dir = -1;}
			}
		}
		goto loop_ofs;
		break;
	}
normal_ofs:
	v[0] = src[ofsi - 1];
    v[1] = src[ofsi];
    v[2] = src[ofsi + 1];	
	v[3] = src[ofsi + 2];	
loop_ofs:
	ofsf += mlt_fraction;
	tmp = v[1] - v[0];
	v[3] += -3 * v[2] + 3 * v[1] - v[0];
	v[3] *= (FLOAT_T)(ofsf - ml2_fraction) * DIV_6 * div_fraction;
	v[3] += v[2] - v[1] - tmp;
	v[3] *= (FLOAT_T)(ofsf - mlt_fraction) * DIV_2 * div_fraction;
	v[3] += tmp;
	v[3] *= (FLOAT_T)ofsf * div_fraction;
	v[3] += v[0];
	return v[3] * OUT_DOUBLE;
do_linear:
    v[1] = src[ofsi];
    v[2] = src[ofsi + 1];	
	return (v[1] + (v[2] - v[1]) * (FLOAT_T)ofsf * div_fraction) * OUT_DOUBLE;
}

/* (at least) n+1 point interpolation using Newton polynomials.
   n can be set with a command line option, and
   must be an odd number from 1 to 57 (57 is as high as double precision
   can go without precision errors).  Default n = 11 is good for a 1.533 MHz
   Athlon.  Larger values for n require very fast processors for real time
   playback.  Some points will be interpolated at orders > n to both increase
   accuracy and save CPU. */

/* for start/end of samples */
static double newt_coeffs[58][58] = {
#include "newton_table.c"
};

#define DEFAULT_NEWTON_ORDER 11
#define DEFAULT_NEWTON_MAX 13
static int newt_n = DEFAULT_NEWTON_ORDER;
static int newt_max = DEFAULT_NEWTON_MAX;
static FLOAT_T newt_recip[65] = { 0, 1, 1.0/2, 1.0/3, 1.0/4, 1.0/5, 1.0/6, 1.0/7,
			1.0/8, 1.0/9, 1.0/10, 1.0/11, 1.0/12, 1.0/13, 1.0/14,
			1.0/15, 1.0/16, 1.0/17, 1.0/18, 1.0/19, 1.0/20, 1.0/21,
			1.0/22, 1.0/23, 1.0/24, 1.0/25, 1.0/26, 1.0/27, 1.0/28,
			1.0/29, 1.0/30, 1.0/31, 1.0/32, 1.0/33, 1.0/34, 1.0/35,
			1.0/36, 1.0/37, 1.0/38, 1.0/39, 1.0/40, 1.0/41, 1.0/42,
			1.0/43, 1.0/44, 1.0/45, 1.0/46, 1.0/47, 1.0/48, 1.0/49,
			1.0/50, 1.0/51, 1.0/52, 1.0/53, 1.0/54, 1.0/55, 1.0/56,
			1.0/57, 1.0/58, 1.0/59 };
#ifndef RESAMPLE_NEWTON_VOICE
static int32 newt_old_trunc_x = -1;
static int newt_grow = -1;
static void *newt_old_src = NULL;
static double newt_divd[60][60];
#endif

#if 0 /* NOT USED */
/* the was calculated statically in newton_table.c */
static void initialize_newton_coeffs(void)
{
    int i, j, n = 57;
    int sign;

    newt_coeffs[0][0] = 1;
    for (i = 0; i <= n; i++)
    {
    	newt_coeffs[i][0] = 1;
    	newt_coeffs[i][i] = 1;

	if (i > 1)
	{
	    newt_coeffs[i][0] = newt_coeffs[i-1][0] / i;
	    newt_coeffs[i][i] = newt_coeffs[i-1][0] / i;
	}

    	for (j = 1; j < i; j++)
    	{
    	    newt_coeffs[i][j] = newt_coeffs[i-1][j-1] + newt_coeffs[i-1][j];

	    if (i > 1)
	    	newt_coeffs[i][j] /= i;
	}
    }
    for (i = 0; i <= n; i++)
    	for (j = 0, sign = pow(-1, i); j <= i; j++, sign *= -1)
    	    newt_coeffs[i][j] *= sign;
}
#endif /* NOT USED */

#ifdef RESAMPLE_NEWTON_VOICE
#define NEWT_TRNC	rec->newt_old_trunc_x
#define NEWT_GROW 	rec->newt_grow
#define NEWT_SRC	rec->newt_old_src
#define NEWT_DIVD	rec->newt_divd

#else // ! RESAMPLE_NEWTON_VOICE
#define NEWT_TRNC	newt_old_trunc_x
#define NEWT_GROW 	newt_grow
#define NEWT_SRC	newt_old_src
#define NEWT_DIVD	newt_divd

static int32 newt_old_trunc_x = -1;
static int newt_grow = -1;
static void *newt_old_src = NULL;
static double newt_divd[60][60];

#endif // RESAMPLE_NEWTON_VOICE

static DATA_T resample_newton(const sample_t *src, splen_t ofs, resample_rec_t *rec)
{
    const spos_t ofsi = ofs >> FRACTION_BITS;
    const fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofso = (rec->data_length >> FRACTION_BITS) - ofsi - 1;
    const sample_t *sptr;
	spos_t temp_n, ii, jj;
    int32 v1, v2, diff = 0;
    int n_new, n_old;
    FLOAT_T y, xd;

	if(rec->mode == RESAMPLE_MODE_BIDIR_LOOP){
		//int32 v1 = src[ofsi];
		//int32 v2 = src[ofsi + 1];	
		//return (DATA_T)(v1 + ((int32)((v2 - v1) * ofsf) >> FRACTION_BITS)) * OUT_INT16;
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
		FLOAT_T v1 = src[ofsi];
		FLOAT_T v2 = src[ofsi + 1];	
		return (v1 + (v2 - v1) * (FLOAT_T)ofsf * div_fraction) * OUT_INT16;
#else // DATA_T_IN32		
		int32 v1 = src[ofsi];
		int32 v2 = src[ofsi + 1];	
		return v1 + imuldiv_fraction(v2 - v1, ofsf);
#endif
	}
    temp_n = (ofso<<1)-1;
    if (temp_n <= 0)
		temp_n = 1;
    if (temp_n > (ofsi<<1)+1)
		temp_n = (ofsi<<1)+1;
    if (temp_n < newt_n) {
		xd = ofsf;
		xd *= div_fraction;
		xd += temp_n>>1;
		y = 0;
		sptr = src + ofsi - (temp_n>>1);
		for (ii = temp_n; ii;) {
			for (jj = 0; jj <= ii; jj++)
				y += sptr[jj] * newt_coeffs[ii][jj];
			y *= xd - --ii;
		} y += *sptr;
    }else{
		if (NEWT_GROW >= 0 && src == NEWT_SRC && (diff = ofsi - NEWT_TRNC) > 0){
			n_new = newt_n + ((NEWT_GROW + diff)<<1);
			if (n_new <= newt_max){
				n_old = newt_n + (NEWT_GROW<<1);
				NEWT_GROW += diff;
				for (v1=ofsi+(n_new>>1)+1,v2=n_new;
					 v2 > n_old; --v1, --v2){
					NEWT_DIVD[0][v2] = src[v1];
				}for (v1 = 1; v1 <= n_new; v1++)
					for (v2 = n_new; v2 > n_old; --v2)
					NEWT_DIVD[v1][v2] = (NEWT_DIVD[v1-1][v2] - NEWT_DIVD[v1-1][v2-1]) * newt_recip[v1];
			}else
				NEWT_GROW = -1;
		}
		if (NEWT_GROW < 0 || src != NEWT_SRC || diff < 0){
			NEWT_GROW = 0;
			for (v1=ofsi-(newt_n>>1),v2=0; v2 <= newt_n; v1++, v2++){
				NEWT_DIVD[0][v2] = src[v1];
			}for (v1 = 1; v1 <= newt_n; v1++)
				for (v2 = newt_n; v2 >= v1; --v2)
					NEWT_DIVD[v1][v2] = (NEWT_DIVD[v1-1][v2] - NEWT_DIVD[v1-1][v2-1]) * newt_recip[v1];
		}
		n_new = newt_n + (NEWT_GROW<<1);
		v2 = n_new;
		y = NEWT_DIVD[v2][v2];
		xd = (FLOAT_T)ofsf * div_fraction + (newt_n>>1) + NEWT_GROW;
		for (--v2; v2; --v2){
			y *= xd - v2;
			y += NEWT_DIVD[v2][v2];
		}y = y*xd + **NEWT_DIVD;
		NEWT_SRC = src;
		NEWT_TRNC = ofsi;
    }
	return y * OUT_INT16;
}

static DATA_T resample_newton_int32(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const int32 *src = (const int32*)srci;
    const spos_t ofsi = ofs >> FRACTION_BITS;
    const fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofso = (rec->data_length >> FRACTION_BITS) - ofsi - 1;
    const int32 *sptr;
	spos_t temp_n, ii, jj;
    int32 v1, v2, diff = 0;
    int n_new, n_old;
    FLOAT_T y, xd;
	
	if(rec->mode == RESAMPLE_MODE_BIDIR_LOOP){
		//FLOAT_T v1 = src[ofsi];
		//FLOAT_T v2 = src[ofsi + 1];	
		//return (v1 + (v2 - v1) * (FLOAT_T)ofsf * div_fraction) * OUT_INT32;
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
		FLOAT_T v1 = src[ofsi];
		FLOAT_T v2 = src[ofsi + 1];	
		return (v1 + (v2 - v1) * (FLOAT_T)ofsf * div_fraction) * OUT_INT32;
#else // DATA_T_IN32	
		int32 v1 = src[ofsi];
		int32 v2 = src[ofsi + 1];	
		return v1 + imuldiv_fraction_int32(v2 - v1, ofsf);
#endif
	}
    temp_n = (ofso<<1)-1;
    if (temp_n <= 0)
		temp_n = 1;
    if (temp_n > (ofsi<<1)+1)
		temp_n = (ofsi<<1)+1;
    if (temp_n < newt_n) {
		xd = ofsf;
		xd *= div_fraction;
		xd += temp_n>>1;
		y = 0;
		sptr = src + ofsi - (temp_n>>1);
		for (ii = temp_n; ii;) {
			for (jj = 0; jj <= ii; jj++)
				y += sptr[jj] * newt_coeffs[ii][jj];
			y *= xd - --ii;
		} y += *sptr;
    }else{
		if (NEWT_GROW >= 0 && src == NEWT_SRC && (diff = ofsi - NEWT_TRNC) > 0){
			n_new = newt_n + ((NEWT_GROW + diff)<<1);
			if (n_new <= newt_max){
				n_old = newt_n + (NEWT_GROW<<1);
				NEWT_GROW += diff;
				for (v1=ofsi+(n_new>>1)+1,v2=n_new;
					 v2 > n_old; --v1, --v2){
					NEWT_DIVD[0][v2] = src[v1];
				}for (v1 = 1; v1 <= n_new; v1++)
					for (v2 = n_new; v2 > n_old; --v2)
					NEWT_DIVD[v1][v2] = (NEWT_DIVD[v1-1][v2] - NEWT_DIVD[v1-1][v2-1]) * newt_recip[v1];
			}else
				NEWT_GROW = -1;
		}
		if (NEWT_GROW < 0 || src != NEWT_SRC || diff < 0){
			NEWT_GROW = 0;
			for (v1=ofsi-(newt_n>>1),v2=0; v2 <= newt_n; v1++, v2++){
				NEWT_DIVD[0][v2] = src[v1];
			}for (v1 = 1; v1 <= newt_n; v1++)
				for (v2 = newt_n; v2 >= v1; --v2)
					NEWT_DIVD[v1][v2] = (NEWT_DIVD[v1-1][v2] - NEWT_DIVD[v1-1][v2-1]) * newt_recip[v1];
		}
		n_new = newt_n + (NEWT_GROW<<1);
		v2 = n_new;
		y = NEWT_DIVD[v2][v2];
		xd = (FLOAT_T)ofsf * div_fraction + (newt_n>>1) + NEWT_GROW;
		for (--v2; v2; --v2){
			y *= xd - v2;
			y += NEWT_DIVD[v2][v2];
		}y = y*xd + **NEWT_DIVD;
		NEWT_SRC = src;
		NEWT_TRNC = ofsi;
    }
	return y * OUT_INT32;
}

static DATA_T resample_newton_float(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const float *src = (const float*)srci;
    const spos_t ofsi = ofs >> FRACTION_BITS;
    const fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofso = (rec->data_length >> FRACTION_BITS) - ofsi - 1;
    const float *sptr;
	spos_t temp_n, ii, jj;
    int32 v1, v2, diff = 0;
    int n_new, n_old;
    FLOAT_T y, xd;
	
	if(rec->mode == RESAMPLE_MODE_BIDIR_LOOP){
		FLOAT_T v1 = src[ofsi];
		FLOAT_T v2 = src[ofsi + 1];	
		return (v1 + (v2 - v1) * (FLOAT_T)ofsf * div_fraction) * OUT_FLOAT;
	}
    temp_n = (ofso<<1)-1;
    if (temp_n <= 0)
		temp_n = 1;
    if (temp_n > (ofsi<<1)+1)
		temp_n = (ofsi<<1)+1;
    if (temp_n < newt_n) {
		xd = ofsf;
		xd *= div_fraction;
		xd += temp_n>>1;
		y = 0;
		sptr = src + ofsi - (temp_n>>1);
		for (ii = temp_n; ii;) {
			for (jj = 0; jj <= ii; jj++)
				y += sptr[jj] * newt_coeffs[ii][jj];
			y *= xd - --ii;
		} y += *sptr;
    }else{
		if (NEWT_GROW >= 0 && src == NEWT_SRC && (diff = ofsi - NEWT_TRNC) > 0){
			n_new = newt_n + ((NEWT_GROW + diff)<<1);
			if (n_new <= newt_max){
				n_old = newt_n + (NEWT_GROW<<1);
				NEWT_GROW += diff;
				for (v1=ofsi+(n_new>>1)+1,v2=n_new;
					 v2 > n_old; --v1, --v2){
					NEWT_DIVD[0][v2] = src[v1];
				}for (v1 = 1; v1 <= n_new; v1++)
					for (v2 = n_new; v2 > n_old; --v2)
					NEWT_DIVD[v1][v2] = (NEWT_DIVD[v1-1][v2] - NEWT_DIVD[v1-1][v2-1]) * newt_recip[v1];
			}else
				NEWT_GROW = -1;
		}
		if (NEWT_GROW < 0 || src != NEWT_SRC || diff < 0){
			NEWT_GROW = 0;
			for (v1=(ofs>>FRACTION_BITS)-(newt_n>>1),v2=0; v2 <= newt_n; v1++, v2++){
				NEWT_DIVD[0][v2] = src[v1];
			}for (v1 = 1; v1 <= newt_n; v1++)
				for (v2 = newt_n; v2 >= v1; --v2)
					NEWT_DIVD[v1][v2] = (NEWT_DIVD[v1-1][v2] - NEWT_DIVD[v1-1][v2-1]) * newt_recip[v1];
		}
		n_new = newt_n + (NEWT_GROW<<1);
		v2 = n_new;
		y = NEWT_DIVD[v2][v2];
		xd = (FLOAT_T)ofsf * div_fraction + (newt_n>>1) + NEWT_GROW;
		for (--v2; v2; --v2){
			y *= xd - v2;
			y += NEWT_DIVD[v2][v2];
		}y = y*xd + **NEWT_DIVD;
		NEWT_SRC = src;
		NEWT_TRNC = ofsi;
    }
	return y * OUT_FLOAT;
}

static DATA_T resample_newton_double(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const double *src = (const double*)srci;
    const spos_t ofsi = ofs >> FRACTION_BITS;
    const fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofso = (rec->data_length >> FRACTION_BITS) - ofsi - 1;
    const double *sptr;
	spos_t temp_n, ii, jj;
    int32 v1, v2, diff = 0;
    int n_new, n_old;
    FLOAT_T y, xd;
	
	if(rec->mode == RESAMPLE_MODE_BIDIR_LOOP){
		FLOAT_T v1 = src[ofsi];
		FLOAT_T v2 = src[ofsi + 1];	
		return (v1 + (v2 - v1) * (FLOAT_T)ofsf * div_fraction) * OUT_DOUBLE;
	}
    temp_n = (ofso<<1)-1;
    if (temp_n <= 0)
		temp_n = 1;
    if (temp_n > (ofsi<<1)+1)
		temp_n = (ofsi<<1)+1;
    if (temp_n < newt_n) {
		xd = ofsf;
		xd *= div_fraction;
		xd += temp_n>>1;
		y = 0;
		sptr = src + ofsi - (temp_n>>1);
		for (ii = temp_n; ii;) {
			for (jj = 0; jj <= ii; jj++)
				y += sptr[jj] * newt_coeffs[ii][jj];
			y *= xd - --ii;
		} y += *sptr;
    }else{
		if (NEWT_GROW >= 0 && src == NEWT_SRC && (diff = ofsi - NEWT_TRNC) > 0){
			n_new = newt_n + ((NEWT_GROW + diff)<<1);
			if (n_new <= newt_max){
				n_old = newt_n + (NEWT_GROW<<1);
				NEWT_GROW += diff;
				for (v1=ofsi+(n_new>>1)+1,v2=n_new;
					 v2 > n_old; --v1, --v2){
					NEWT_DIVD[0][v2] = src[v1];
				}for (v1 = 1; v1 <= n_new; v1++)
					for (v2 = n_new; v2 > n_old; --v2)
					NEWT_DIVD[v1][v2] = (NEWT_DIVD[v1-1][v2] - NEWT_DIVD[v1-1][v2-1]) * newt_recip[v1];
			}else
				NEWT_GROW = -1;
		}
		if (NEWT_GROW < 0 || src != NEWT_SRC || diff < 0){
			NEWT_GROW = 0;
			for (v1=ofsi-(newt_n>>1),v2=0; v2 <= newt_n; v1++, v2++){
				NEWT_DIVD[0][v2] = src[v1];
			}for (v1 = 1; v1 <= newt_n; v1++)
				for (v2 = newt_n; v2 >= v1; --v2)
					NEWT_DIVD[v1][v2] = (NEWT_DIVD[v1-1][v2] - NEWT_DIVD[v1-1][v2-1]) * newt_recip[v1];
		}
		n_new = newt_n + (NEWT_GROW<<1);
		v2 = n_new;
		y = NEWT_DIVD[v2][v2];
		xd = (FLOAT_T)ofsf * div_fraction + (newt_n>>1) + NEWT_GROW;
		for (--v2; v2; --v2){
			y *= xd - v2;
			y += NEWT_DIVD[v2][v2];
		}y = y*xd + **NEWT_DIVD;
		NEWT_SRC = src;
		NEWT_TRNC = ofsi;
    }
	return y * OUT_DOUBLE;
}


/* Very fast and accurate table based interpolation.  Better speed and higher
   accuracy than Newton.  This isn't *quite* true Gauss interpolation; it's
   more a slightly modified Gauss interpolation that I accidently stumbled
   upon.  Rather than normalize all x values in the window to be in the range
   [0 to 2*PI], it simply divides them all by 2*PI instead.  I don't know why
   this works, but it does.  Gauss should only work on periodic data with the
   window spanning exactly one period, so it is no surprise that regular Gauss
   interpolation doesn't work too well on general audio data.  But dividing
   the x values by 2*PI magically does.  Any other scaling produces degraded
   results or total garbage.  If anyone can work out the theory behind why
   this works so well (at first glance, it shouldn't ??), please contact me
   (Eric A. Welsh, ewelsh@ccb.wustl.edu), as I would really like to have some
   mathematical justification for doing this.  Despite the lack of any sound
   theoretical basis, this method DOES result in highly accurate interpolation
   (or possibly approximaton, not sure yet if it truly interpolates, but it
   looks like it does).  -N 34 is as high as it can go before errors start
   appearing.  But even at -N 34, it is more accurate than Newton at -N 57.
   -N 34 has no problem running in realtime on my system, but -N 25 is the
   default, since that is the optimal compromise between speed and accuracy.
   I strongly recommend using Gauss interpolation.  It is the highest
   quality interpolation option available, and is much faster than using
   Newton polynomials. */
///r
#define DEFAULT_GAUSS_ORDER	24
static FLOAT_T *gauss_table[(1<<FRACTION_BITS)] = {0};	/* don't need doubles */
static float *gauss_table_float[(1<<FRACTION_BITS)] = {0};	/* don't need doubles */
static int32 *gauss_table_int32[(1<<FRACTION_BITS)] = {0};	/* don't need doubles */
static int gauss_n = DEFAULT_GAUSS_ORDER;

static void initialize_gauss_table(int n)
{
    int m, i, k, n_half = (n>>1);
    double ck;
    double x, xz;
    double z[35], zsin_[34 + 35], *zsin, xzsin[35];
    FLOAT_T *gptr;

    for (i = 0; i <= n; i++)
    	z[i] = i / (4*M_PI);
    zsin = &zsin_[34];
    for (i = -n; i <= n; i++)
    	zsin[i] = sin(i / (4*M_PI));

    gptr = (FLOAT_T *)safe_realloc(gauss_table[0], (n + 1) * sizeof(FLOAT_T) * mlt_fraction);
    for (m = 0, x = 0.0; m < mlt_fraction; m++, x += div_fraction)
    {
    	xz = (x + n_half) / (4*M_PI);
    	for (i = 0; i <= n; i++)
	    xzsin[i] = sin(xz - z[i]);
    	gauss_table[m] = gptr;
    	for (k = 0; k <= n; k++)
    	{
    	    ck = 1.0;
    	    for (i = 0; i <= n; i++)
    	    {
    	    	if (i == k)  continue;
     	    	ck *= xzsin[i] / zsin[k - i];
    	    }
    	    *gptr++ = ck;
    	}
    }
}

static void free_gauss_table(void)
{
	int i;
    if(gauss_table[0]){
        free(gauss_table[0]);
    }
    for(i = 0; i < (1<<FRACTION_BITS); i++){
        gauss_table[i] = NULL;
    }
	//if(gauss_table[0] != 0)
	//  free(gauss_table[0]);
	//gauss_table[0] = NULL;
}

static DATA_T resample_gauss(const sample_t *src, splen_t ofs, resample_rec_t *rec)
{
    const spos_t ofsi = ofs >> FRACTION_BITS;
    const fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofso = (rec->data_length >> FRACTION_BITS) - ofsi - 1;
    const sample_t *sptr;
	spos_t temp_n, temp_l;
	FLOAT_T *gptr;
	FLOAT_T y = 0, xd;
    int32 i, j;
	
	if(rec->mode == RESAMPLE_MODE_BIDIR_LOOP){
	//	int32 v1 = src[ofsi];
	//	int32 v2 = src[ofsi + 1];	
	//	return (DATA_T)(v1 + ((int32)((v2 - v1) * ofsf) >> FRACTION_BITS)) * OUT_INT16;
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
		FLOAT_T v1 = src[ofsi];
		FLOAT_T v2 = src[ofsi + 1];	
		return (v1 + (v2 - v1) * (FLOAT_T)ofsf * div_fraction) * OUT_INT16;
#else // DATA_T_IN32		
		int32 v1 = src[ofsi];
		int32 v2 = src[ofsi + 1];	
		return v1 + imuldiv_fraction(v2 - v1, ofsf);
#endif
	}
    temp_n = (ofso<<1)-1;
	temp_l = (ofsi<<1)+1;
    if (temp_n > temp_l)
		temp_n = temp_l;
    if (temp_n < gauss_n) { // gauss_n
		if (temp_n < 1)
			temp_n = 1;
		xd = (FLOAT_T)ofsf * div_fraction + (temp_n>>1);
		sptr = src + ofsi - (temp_n>>1);
		for (i = temp_n; i;) {
			for (j = 0; j <= i; j++)
				y += sptr[j] * newt_coeffs[i][j];
			y *= xd - --i;
		}
		y += *sptr;
		return y * OUT_INT16;
	} else	switch(gauss_n) {
// optimization gauss_n=32,24,16,8
#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE) && defined(FLOAT_T_DOUBLE)
	case 32:
	case 24:
	case 16:
	case 8:
		sptr = src + ofsi - (gauss_n >> 1); // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		{
		__m256d sum = _mm256_set_pd(0, 0, 0, (FLOAT_T)(*(sptr++)) * (*gptr++));
		__m128d sum1, sum2;	
		double tmp;
		for (i = 0; i < gauss_n; i += 8){
#if (USE_X86_EXT_INTRIN >= 9)
			__m256i vec32 = _mm256_cvtepi16_epi32(_mm_loadu_si128((__m128i *)&sptr[i])); // low i16*8 > i32*8
			__m128i vec1 = _mm256_extracti128_si256(vec32, 0x0);
			__m128i vec2 = _mm256_extracti128_si256(vec32, 0x1);
#else
			__m128i vec16a = _mm_loadu_si128((__m128i *)&sptr[i]); // i16*8 (low			
			__m128i vec1 = _mm_cvtepi16_epi32(vec16a); // low i16*4 > i32*4 > d*4
			__m128i vec2 = _mm_cvtepi16_epi32(_mm_shuffle_epi32(vec16a, 0x4e)); // hi i16*4 > i32*4 > d*4
#endif
			sum = MM256_FMA_PD(_mm256_cvtepi32_pd(vec1), _mm256_loadu_pd(&gptr[i]), sum);
			sum = MM256_FMA_PD(_mm256_cvtepi32_pd(vec2), _mm256_loadu_pd(&gptr[i + 4]), sum);
		}
		sum1 = _mm256_extractf128_pd(sum, 0x0); // v0,v1
		sum2 = _mm256_extractf128_pd(sum, 0x1); // v2,v3
		sum1 = _mm_add_pd(sum1, sum2); // v0=v0+v2 v1=v1+v3
		sum1 = _mm_add_pd(sum1, _mm_shuffle_pd(sum1, sum1, 0x1)); // v0=v0+v1 v1=v1+v0
		_mm_store_sd(&tmp, sum1);
		return tmp * OUT_INT16;
		}
#elif (USE_X86_EXT_INTRIN >= 6) && defined(DATA_T_DOUBLE) && defined(FLOAT_T_DOUBLE)
	case 32:
	case 24:
	case 16:
	case 8:
		sptr = src + ofsi - (gauss_n >> 1); // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		{
		__m128d sum = _mm_set_pd(0, (FLOAT_T)(*(sptr++)) * (*gptr++));
		double tmp;
		for (i = 0; i < gauss_n; i += 8){
			__m128i vec16a = _mm_loadu_si128((__m128i *)&sptr[i]);
			__m128i vec32l = _mm_cvtepi16_epi32(vec16a); // low i16*4 > i32*4
			__m128i vec32h = _mm_cvtepi16_epi32(_mm_shuffle_epi32(vec16a, 0x4e)); // hi i16*4 > i32*4
			__m128d vecd0 = _mm_cvtepi32_pd(vec32l); // low low i32*2 > d*2
			__m128d vecd2 = _mm_cvtepi32_pd(_mm_shuffle_epi32(vec32l, 0x4e)); // low hi i32*2 > d*2
			__m128d vecd4 = _mm_cvtepi32_pd(vec32h); // hi low i32*2 > d*2
			__m128d vecd6 = _mm_cvtepi32_pd(_mm_shuffle_epi32(vec32h, 0x4e)); // hi hi i32*2 > d*2
			sum = MM_FMA_PD(vecd0, _mm_loadu_pd(&gptr[i]), sum);
			sum = MM_FMA_PD(vecd2, _mm_loadu_pd(&gptr[i + 2]), sum);
			sum = MM_FMA_PD(vecd4, _mm_loadu_pd(&gptr[i + 4]), sum);
			sum = MM_FMA_PD(vecd6, _mm_loadu_pd(&gptr[i + 6]), sum);
		}
		sum = _mm_add_pd(sum, _mm_shuffle_pd(sum, sum, 0x1)); // v0=v0+v1 v1=v1+v0
		_mm_store_sd(&tmp, sum);
		return tmp * OUT_INT16;
		}
		
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE) && defined(FLOAT_T_DOUBLE)
	case 32:
	case 24:
	case 16:
	case 8:
		sptr = src + ofsi - (gauss_n >> 1); // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		{
		__m128d sum = _mm_set_pd(0, (FLOAT_T)(*(sptr++)) * (*gptr++));
		double tmp;
		for (i = 0; i < gauss_n; i += 8){
			__m128i vec16a = _mm_loadu_si128((__m128i *)&sptr[i]);
			__m128i vec16h = _mm_shuffle_epi32(vec16a, 0x4e); // vec16a hi 64bit to low 64bit	
			__m128i vec32l = _mm_unpacklo_epi16(vec16a, _mm_cmpgt_epi16(_mm_setzero_si128(), vec16a)); // low i16*4 > i32*4
			__m128i vec32h = _mm_unpacklo_epi16(vec16h, _mm_cmpgt_epi16(_mm_setzero_si128(), vec16h)); // hi i16*4 > i32*4
			__m128d vecd0 = _mm_cvtepi32_pd(vec32l); // low low i32*2 > d*2
			__m128d vecd2 = _mm_cvtepi32_pd(_mm_shuffle_epi32(vec32l, 0x4e)); // low hi i32*2 > d*2
			__m128d vecd4 = _mm_cvtepi32_pd(vec32h); // hi low i32*2 > d*2
			__m128d vecd6 = _mm_cvtepi32_pd(_mm_shuffle_epi32(vec32h, 0x4e)); // hi hi i32*2 > d*2
			sum = MM_FMA_PD(vecd0, _mm_loadu_pd(&gptr[i]), sum);
			sum = MM_FMA_PD(vecd2, _mm_loadu_pd(&gptr[i + 2]), sum);
			sum = MM_FMA_PD(vecd4, _mm_loadu_pd(&gptr[i + 4]), sum);
			sum = MM_FMA_PD(vecd6, _mm_loadu_pd(&gptr[i + 6]), sum);
		}	
		sum = _mm_add_pd(sum, _mm_shuffle_pd(sum, sum, 0x1)); // v0=v0+v1 v1=v1+v0
		_mm_store_sd(&tmp, sum);
		return tmp * OUT_INT16;
		}
#else
	case 32:
		sptr = src + ofsi - 16; // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		y = *sptr * *gptr;
		y = *sptr * *gptr;
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +2
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +4
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +6
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +8
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +10
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +12
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +14
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +16
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +18
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +20
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +22
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +24
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +26
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +28
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +30
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +32
		return y * OUT_INT16;
	case 24:
		sptr = src + ofsi - 12; // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		y = *sptr * *gptr;
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +2
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +4
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +6
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +8
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +10
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +12
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +14
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +16
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +18
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +20
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +22
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +24
		return y * OUT_INT16;
	case 16:
		sptr = src + ofsi - 8; // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		y = *sptr * *gptr;
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +2
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +4
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +6
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +8
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +10
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +12
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +14
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +16
		return y * OUT_INT16;
	case 8:
		sptr = src + ofsi - 4; // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		y = *sptr * *gptr;
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +2
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +4
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +6
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +8
		return y * OUT_INT16;
#endif
	case 4:
		sptr = src + ofsi - 2; // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		y = *sptr * *gptr;
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +2
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +4
		return y * OUT_INT16;
	case 2:
		sptr = src + ofsi - 1; // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		y = *sptr * *gptr;
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +2
		return y * OUT_INT16;
	default:
		return 0;
	}
}

static DATA_T resample_gauss_int32(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const int32 *src = (const int32*)srci;
    const spos_t ofsi = ofs >> FRACTION_BITS;
    const fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofso = (rec->data_length >> FRACTION_BITS) - ofsi - 1;
    const int32 *sptr;
	spos_t temp_n, temp_l;
	FLOAT_T *gptr;
	FLOAT_T y = 0, xd;
    int32 i, j;
	
	if(rec->mode == RESAMPLE_MODE_BIDIR_LOOP){
		//FLOAT_T v1 = src[ofsi];
		//FLOAT_T v2 = src[ofsi + 1];	
		//return (v1 + (v2 - v1) * (FLOAT_T)ofsf * div_fraction) * OUT_INT32;		
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
		FLOAT_T v1 = src[ofsi];
		FLOAT_T v2 = src[ofsi + 1];	
		return (v1 + (v2 - v1) * (FLOAT_T)ofsf * div_fraction) * OUT_INT32;
#else // DATA_T_IN32	
		int32 v1 = src[ofsi];
		int32 v2 = src[ofsi + 1];	
		return v1 + imuldiv_fraction_int32(v2 - v1, ofsf);
#endif
	}
    temp_n = (ofso<<1)-1;
	temp_l = (ofsi<<1)+1;
    if (temp_n > temp_l)
		temp_n = temp_l;
    if (temp_n < gauss_n) { // gauss_n
		if (temp_n < 1)
			temp_n = 1;
		xd = (FLOAT_T)ofsf * div_fraction + (temp_n>>1);
		sptr = src + ofsi - (temp_n>>1);
		for (i = temp_n; i;) {
			for (j = 0; j <= i; j++)
				y += sptr[j] * newt_coeffs[i][j];
			y *= xd - --i;
		}
		y += *sptr;
		return y * OUT_INT32;
	} else switch(gauss_n) {
#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE) && defined(FLOAT_T_DOUBLE)
	case 32:
	case 24:
	case 16:
	case 8:
		sptr = src + ofsi - (gauss_n >> 1); // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		{
		__m256d sum = _mm256_set_pd(0, 0, 0, (FLOAT_T)(*(sptr++)) * (*gptr++));
		__m128d sum1, sum2;	
		double tmp;
		for (i = 0; i < gauss_n; i += 8){
#if (USE_X86_EXT_INTRIN >= 9)
			__m256i vec32 = _mm256_loadu_si256((const __m256i *)&sptr[i]);
			__m128i vec1 = _mm256_extracti128_si256(vec32, 0x0);
			__m128i vec2 = _mm256_extracti128_si256(vec32, 0x1);
#else
			__m128i vec1 = _mm_loadu_si128((__m128i *)&sptr[i]);
			__m128i vec2 = _mm_loadu_si128((__m128i *)&sptr[i + 4]);
#endif
			sum = MM256_FMA_PD(_mm256_cvtepi32_pd(vec1), _mm256_loadu_pd(&gptr[i]), sum);
			sum = MM256_FMA_PD(_mm256_cvtepi32_pd(vec2), _mm256_loadu_pd(&gptr[i + 4]), sum);
		}
		sum1 = _mm256_extractf128_pd(sum, 0x0); // v0,v1
		sum2 = _mm256_extractf128_pd(sum, 0x1); // v2,v3
		sum1 = _mm_add_pd(sum1, sum2); // v0=v0+v2 v1=v1+v3
		sum1 = _mm_add_pd(sum1, _mm_shuffle_pd(sum1, sum1, 0x1)); // v0=v0+v1 v1=v1+v0
		_mm_store_sd(&tmp, sum1);
		return tmp * OUT_INT32;
		}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE) && defined(FLOAT_T_DOUBLE)
	case 32:
	case 24:
	case 16:
	case 8:
		sptr = src + ofsi - (gauss_n >> 1); // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		{
		__m128d sum = _mm_set_pd(0, (FLOAT_T)(*(sptr++)) * (*gptr++));
		double tmp;
		for (i = 0; i < gauss_n; i += 8){
			__m128i vec32i0 = _mm_loadu_si128((__m128i *)&sptr[i]);
			__m128i vec32i4 = _mm_loadu_si128((__m128i *)&sptr[i + 4]);
			sum = MM_FMA_PD(_mm_cvtepi32_pd(vec32i0), _mm_loadu_pd(&gptr[i]), sum);
			sum = MM_FMA_PD(_mm_cvtepi32_pd(_mm_shuffle_epi32(vec32i0, 0x4e)), _mm_loadu_pd(&gptr[i + 2]), sum);
			sum = MM_FMA_PD(_mm_cvtepi32_pd(vec32i4), _mm_loadu_pd(&gptr[i + 4]), sum);
			sum = MM_FMA_PD(_mm_cvtepi32_pd(_mm_shuffle_epi32(vec32i4, 0x4e)), _mm_loadu_pd(&gptr[i + 6]), sum);
		}
		sum = _mm_add_pd(sum, _mm_shuffle_pd(sum, sum, 0x1)); // v0=v0+v1 v1=v1+v0
		_mm_store_sd(&tmp, sum);
		return tmp * OUT_INT32;
		}
#else
	case 32:
// optimization gauss_n=32,24,16,8
		sptr = src + ofsi - 16; // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		y = *sptr * *gptr;
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +2
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +4
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +6
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +8
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +10
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +12
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +14
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +16
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +18
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +20
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +22
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +24
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +26
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +28
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +30
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +32
		return y * OUT_INT32;
	case 24:
		sptr = src + ofsi - 12; // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		y = *sptr * *gptr;
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +2
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +4
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +6
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +8
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +10
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +12
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +14
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +16
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +18
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +20
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +22
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +24
		return y * OUT_INT32;
	case 16:
		sptr = src + ofsi - 8; // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		y = *sptr * *gptr;
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +2
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +4
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +6
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +8
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +10
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +12
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +14
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +16
		return y * OUT_INT32;
	case 8:
		sptr = src + ofsi - 4; // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		y = *sptr * *gptr;
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +2
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +4
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +6
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +8
		return y * OUT_INT32;
#endif
	case 4:
		sptr = src + ofsi - 2; // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		y = *sptr * *gptr;
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +2
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +4
		return y * OUT_INT32;
	case 2:
		sptr = src + ofsi - 1; // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		y = *sptr * *gptr;
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +2
		return y * OUT_INT32;
	default:
		return 0;
	}
}


static DATA_T resample_gauss_float(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const float *src = (const float*)srci;
    const spos_t ofsi = ofs >> FRACTION_BITS;
    const fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofso = (rec->data_length >> FRACTION_BITS) - ofsi - 1;
    const float *sptr;
	spos_t temp_n, temp_l;
	FLOAT_T *gptr;
	FLOAT_T y = 0, xd;
    int32 i, j;
	
	if(rec->mode == RESAMPLE_MODE_BIDIR_LOOP){
		FLOAT_T v1 = src[ofsi];
		FLOAT_T v2 = src[ofsi + 1];	
		return (v1 + (v2 - v1) * (FLOAT_T)ofsf * div_fraction) * OUT_FLOAT;
	}
    temp_n = (ofso<<1)-1;
	temp_l = (ofsi<<1)+1;
    if (temp_n > temp_l)
		temp_n = temp_l;
    if (temp_n < gauss_n) { // gauss_n
		if (temp_n < 1)
			temp_n = 1;
		xd = (FLOAT_T)ofsf * div_fraction + (temp_n>>1);
		sptr = src + ofsi - (temp_n>>1);
		for (i = temp_n; i;) {
			for (j = 0; j <= i; j++)
				y += sptr[j] * newt_coeffs[i][j];
			y *= xd - --i;
		}
		y += *sptr;
		return y;
	} else switch(gauss_n) {
// optimization gauss_n=32,24,16,8
#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE) && defined(FLOAT_T_DOUBLE)
	case 32:
	case 24:
	case 16:
	case 8:
		sptr = src + ofsi - (gauss_n >> 1); // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		{
		__m256d sum = _mm256_set_pd(0, 0, 0, (FLOAT_T)(*(sptr++)) * (*gptr++));
		__m128d sum1, sum2;	
		double tmp;
		for (i = 0; i < gauss_n; i += 8){
			__m256 vecf = _mm256_loadu_ps(&sptr[i]);
			__m128 vec1 = _mm256_extractf128_ps(vecf, 0x0);
			__m128 vec2 = _mm256_extractf128_ps(vecf, 0x1);
			sum = MM256_FMA_PD(_mm256_cvtps_pd(vec1), _mm256_loadu_pd(&gptr[i]), sum);
			sum = MM256_FMA_PD(_mm256_cvtps_pd(vec2), _mm256_loadu_pd(&gptr[i + 4]), sum);
		}
		sum1 = _mm256_extractf128_pd(sum, 0x0); // v0,v1
		sum2 = _mm256_extractf128_pd(sum, 0x1); // v2,v3
		sum1 = _mm_add_pd(sum1, sum2); // v0=v0+v2 v1=v1+v3
		sum1 = _mm_add_pd(sum1, _mm_shuffle_pd(sum1, sum1, 0x1)); // v0=v0+v1 v1=v1+v0
		_mm_store_sd(&tmp, sum1);
		return tmp * OUT_FLOAT;
		}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE) && defined(FLOAT_T_DOUBLE)
	case 32:
	case 24:
	case 16:
	case 8:
		sptr = src + ofsi - (gauss_n >> 1); // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		{
		__m128d sum = _mm_set_pd(0, (FLOAT_T)(*(sptr++)) * (*gptr++));
		double tmp;
		for (i = 0; i < gauss_n; i += 8){
			__m128 vecf0 = _mm_loadu_ps(&sptr[i]);
			__m128 vecf4 = _mm_loadu_ps(&sptr[i + 4]);
			sum = MM_FMA_PD(_mm_cvtps_pd(vecf0), _mm_loadu_pd(&gptr[i]), sum);
			sum = MM_FMA_PD(_mm_cvtps_pd(_mm_shuffle_ps(vecf0, vecf0, 0x4e)), _mm_loadu_pd(&gptr[i + 2]), sum);
			sum = MM_FMA_PD(_mm_cvtps_pd(vecf4), _mm_loadu_pd(&gptr[i + 4]), sum);
			sum = MM_FMA_PD(_mm_cvtps_pd(_mm_shuffle_ps(vecf4, vecf4, 0x4e)), _mm_loadu_pd(&gptr[i + 6]), sum);
		}
		sum = _mm_add_pd(sum, _mm_shuffle_pd(sum, sum, 0x1)); // v0=v0+v1 v1=v1+v0
		_mm_store_sd(&tmp, sum);
		return tmp * OUT_FLOAT;
		}
#else
	case 32:
		sptr = src + ofsi - 16; // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		y = *sptr * *gptr;
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +2
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +4
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +6
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +8
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +10
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +12
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +14
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +16
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +18
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +20
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +22
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +24
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +26
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +28
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +30
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +32
		return y * OUT_FLOAT;
	case 24:
		sptr = src + ofsi - 12; // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		y = *sptr * *gptr;
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +2
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +4
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +6
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +8
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +10
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +12
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +14
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +16
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +18
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +20
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +22
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +24
		return y * OUT_FLOAT;
	case 16:
		sptr = src + ofsi - 8; // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		y = *sptr * *gptr;
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +2
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +4
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +6
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +8
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +10
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +12
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +14
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +16
		return y * OUT_FLOAT;
	case 8:
		sptr = src + ofsi - 4; // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		y = *sptr * *gptr;
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +2
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +4
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +6
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +8
		return y * OUT_FLOAT;
#endif
	case 4:
		sptr = src + ofsi - 2; // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		y = *sptr * *gptr;
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +2
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +4
		return y * OUT_FLOAT;
	case 2:
		sptr = src + ofsi - 1; // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		y = *sptr * *gptr;
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +2
		return y * OUT_FLOAT;
	default:
		return 0;
	}
}

static DATA_T resample_gauss_double(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const double *src = (const double*)srci;
    const spos_t ofsi = ofs >> FRACTION_BITS;
    const fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofso = (rec->data_length >> FRACTION_BITS) - ofsi - 1;
    const double *sptr;
	spos_t temp_n, temp_l;
	FLOAT_T *gptr;
	FLOAT_T y = 0, xd;
    int32 i, j;
	
	if(rec->mode == RESAMPLE_MODE_BIDIR_LOOP){
		FLOAT_T v1 = src[ofsi];
		FLOAT_T v2 = src[ofsi + 1];	
		return (v1 + (v2 - v1) * (FLOAT_T)ofsf * div_fraction) * OUT_DOUBLE;
	}
    temp_n = (ofso<<1)-1;
	temp_l = (ofsi<<1)+1;
    if (temp_n > temp_l)
		temp_n = temp_l;
    if (temp_n < gauss_n) { // gauss_n
		if (temp_n < 1)
			temp_n = 1;
		xd = (FLOAT_T)ofsf * div_fraction + (temp_n>>1);
		sptr = src + ofsi - (temp_n>>1);
		for (i = temp_n; i;) {
			for (j = 0; j <= i; j++)
				y += sptr[j] * newt_coeffs[i][j];
			y *= xd - --i;
		}
		y += *sptr;
		return y * OUT_DOUBLE;
	} else switch(gauss_n) {
// optimization gauss_n=32,24,16,8
#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE) && defined(FLOAT_T_DOUBLE)
	case 32:
	case 24:
	case 16:
	case 8:
		sptr = src + ofsi - (gauss_n >> 1); // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		{
		__m256d sum = _mm256_set_pd(0, 0, 0, (FLOAT_T)(*(sptr++)) * (*gptr++));
		__m128d sum1, sum2;	
		double tmp;
		for (i = 0; i < gauss_n; i += 8){
			sum = MM256_FMA_PD(_mm256_loadu_pd(&sptr[i]), _mm256_loadu_pd(&gptr[i]), sum);
			sum = MM256_FMA_PD(_mm256_loadu_pd(&sptr[i + 4]), _mm256_loadu_pd(&gptr[i + 4]), sum);
		}
		sum1 = _mm256_extractf128_pd(sum, 0x0); // v0,v1
		sum2 = _mm256_extractf128_pd(sum, 0x1); // v2,v3
		sum1 = _mm_add_pd(sum1, sum2); // v0=v0+v2 v1=v1+v3
		sum1 = _mm_add_pd(sum1, _mm_shuffle_pd(sum1, sum1, 0x1)); // v0=v0+v1 v1=v1+v0
		_mm_store_sd(&tmp, sum1);
		return tmp * OUT_DOUBLE;
		}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE) && defined(FLOAT_T_DOUBLE)
	case 32:
	case 24:
	case 16:
	case 8:
		sptr = src + ofsi - (gauss_n >> 1); // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		{
		__m128d sum = _mm_set_pd(0, (FLOAT_T)(*(sptr++)) * (*gptr++));
		double tmp;
		for (i = 0; i < gauss_n; i += 8){
			sum = MM_FMA_PD(_mm_loadu_pd(&sptr[i]), _mm_loadu_pd(&gptr[i]), sum);
			sum = MM_FMA_PD(_mm_loadu_pd(&sptr[i + 2]), _mm_loadu_pd(&gptr[i + 2]), sum);
			sum = MM_FMA_PD(_mm_loadu_pd(&sptr[i + 4]), _mm_loadu_pd(&gptr[i + 4]), sum);
			sum = MM_FMA_PD(_mm_loadu_pd(&sptr[i + 6]), _mm_loadu_pd(&gptr[i + 6]), sum);
		}
		sum = _mm_add_pd(sum, _mm_shuffle_pd(sum, sum, 0x1)); // v0=v0+v1 v1=v1+v0
		_mm_store_sd(&tmp, sum);
		return tmp * OUT_DOUBLE;
		}
#else
	case 32:
		sptr = src + ofsi - 16; // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		y = *sptr * *gptr;
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +2
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +4
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +6
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +8
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +10
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +12
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +14
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +16
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +18
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +20
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +22
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +24
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +26
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +28
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +30
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +32
		return y * OUT_DOUBLE;
	case 24:
		sptr = src + ofsi - 12; // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		y = *sptr * *gptr;
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +2
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +4
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +6
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +8
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +10
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +12
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +14
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +16
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +18
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +20
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +22
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +24
		return y * OUT_DOUBLE;
	case 16:
		sptr = src + ofsi - 8; // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		y = *sptr * *gptr;
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +2
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +4
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +6
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +8
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +10
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +12
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +14
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +16
		return y * OUT_DOUBLE;
	case 8:
		sptr = src + ofsi - 4; // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		y = *sptr * *gptr;
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +2
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +4
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +6
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +8
		return y * OUT_DOUBLE;
#endif
	case 4:
		sptr = src + ofsi - 2; // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		y = *sptr * *gptr;
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +2
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +4
		return y * OUT_DOUBLE;
	case 2:
		sptr = src + ofsi - 1; // gauss_n>>1
		gptr = gauss_table[ofs&FRACTION_MASK];
		y = *sptr * *gptr;
		y += *(++sptr) * *(++gptr); y += *(++sptr) * *(++gptr); // +2
		return y * OUT_DOUBLE;
	default:
		return 0;
	}
}

///r 
#define DEFAULT_SHARP_ORDER	2
static int sharp_n = DEFAULT_SHARP_ORDER;
static FLOAT_T sharp_recip[20] = { 10.00, -1.0, -1.0/2, -1.0/3, -1.0/4, -1.0/5, -1.0/6, -1.0/7, -1.0/8, -1.0/9,
								-1.0/10, -1.0/11, -1.0/12, -1.0/13, -1.0/14, -1.0/15, -1.0/16, -1.0/17, -1.0/18, -1.0/19,};

static DATA_T resample_sharp(const sample_t *src, splen_t ofs, resample_rec_t *rec)
{
    const spos_t ofsi = ofs >> FRACTION_BITS;
    const fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofso = (rec->data_length >> FRACTION_BITS) - ofsi - 2;
	const FLOAT_T fp = (FLOAT_T)ofsf * div_fraction;
	const sample_t *v1, *v2;
    int32 i, tmp;
	FLOAT_T c,s = 0.0, va = 0.0, vb = 0.0;
	
	if(rec->mode == RESAMPLE_MODE_BIDIR_LOOP){
	//	int32 v1 = src[ofsi];
	//	int32 v2 = src[ofsi + 1];	
	//	return (v1 + (FLOAT_T)(v2 - v1) * fp) * OUT_INT16;
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
		FLOAT_T v1 = src[ofsi];
		FLOAT_T v2 = src[ofsi + 1];	
		return (v1 + (v2 - v1) * fp) * OUT_INT16;
#else // DATA_T_IN32		
		int32 v1 = src[ofsi];
		int32 v2 = src[ofsi + 1];	
		return v1 + imuldiv_fraction(v2 - v1, ofsf);
#endif
	}
	tmp = sharp_n;
	if(ofso < tmp) tmp = ofso;
	if(tmp < 1) return (FLOAT_T)src[ofsi] * OUT_INT16;
	v1 = src + ofsi, v2 = v1 + 1;
	for(i = 0; i < tmp; i++){
		va += v1[i] * sharp_recip[i];
		vb += v2[i] * sharp_recip[i];
		s += fabs(sharp_recip[i]);
	}
	c = 1.0 / s;
	va *= c;
	vb *= c;
	return (va + (vb - va) * fp) * OUT_INT16;
}

static DATA_T resample_sharp_int32(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const int32 *src = (const int32*)srci;
    const spos_t ofsi = ofs >> FRACTION_BITS;
    const fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofso = (rec->data_length >> FRACTION_BITS) - ofsi - 2;
	const FLOAT_T fp = (FLOAT_T)ofsf * div_fraction;
	const int32 *v1, *v2;
    int32 i, tmp;
	FLOAT_T c,s = 0.0, va = 0.0, vb = 0.0;
	
	if(rec->mode == RESAMPLE_MODE_BIDIR_LOOP){
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
		FLOAT_T v1 = src[ofsi];
		FLOAT_T v2 = src[ofsi + 1];	
		return (v1 + (v2 - v1) * fp) * OUT_INT32;	
#else // DATA_T_IN32	
		int32 v1 = src[ofsi];
		int32 v2 = src[ofsi + 1];	
		return v1 + imuldiv_fraction_int32(v2 - v1, ofsf);
#endif
	}
	tmp = sharp_n;
	if(ofso < tmp) tmp = ofso;
	if(tmp < 1) return (FLOAT_T)src[ofsi] * OUT_INT32;
	v1 = src + ofsi, v2 = v1 + 1;
	for(i = 0; i < tmp; i++){
		va += v1[i] * sharp_recip[i];
		vb += v2[i] * sharp_recip[i];
		s += fabs(sharp_recip[i]);
	}
	c = 1.0 / s;
	va *= c;
	vb *= c;
	return (va + (vb - va) * fp) * OUT_INT32;
}

static DATA_T resample_sharp_float(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const float *src = (const float*)srci;
    const spos_t ofsi = ofs >> FRACTION_BITS;
    const fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofso = (rec->data_length >> FRACTION_BITS) - ofsi - 2;
	const FLOAT_T fp = (FLOAT_T)ofsf * div_fraction;
	const float *v1, *v2;
    int32 i, tmp;
	FLOAT_T c,s = 0.0, va = 0.0, vb = 0.0;
	
	if(rec->mode == RESAMPLE_MODE_BIDIR_LOOP){
		FLOAT_T v1 = src[ofsi];
		FLOAT_T v2 = src[ofsi + 1];	
		return (v1 + (v2 - v1) * fp) * OUT_FLOAT;
	}
	tmp = sharp_n;
	if(ofso < tmp) tmp = ofso;
	if(tmp < 1) return (FLOAT_T)src[ofsi] * OUT_FLOAT;
	v1 = src + ofsi, v2 = v1 + 1;
	for(i = 0; i < tmp; i++){
		va += v1[i] * sharp_recip[i];
		vb += v2[i] * sharp_recip[i];
		s += fabs(sharp_recip[i]);
	}
	c = 1.0 / s;
	va *= c;
	vb *= c;
	return (va + (vb - va) * fp) * OUT_FLOAT;
}

static DATA_T resample_sharp_double(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const double *src = (const double*)srci;
    const spos_t ofsi = ofs >> FRACTION_BITS;
    const fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofso = (rec->data_length >> FRACTION_BITS) - ofsi - 2;
	const FLOAT_T fp = (FLOAT_T)ofsf * div_fraction;
	const double *v1, *v2;
    int32 i, tmp;
	FLOAT_T c,s = 0.0, va = 0.0, vb = 0.0;
	
	if(rec->mode == RESAMPLE_MODE_BIDIR_LOOP){
		FLOAT_T v1 = src[ofsi];
		FLOAT_T v2 = src[ofsi + 1];	
		return (v1 + (v2 - v1) * fp) * OUT_DOUBLE;
	}
	tmp = sharp_n;
	if(ofso < tmp) tmp = ofso;
	if(tmp < 1) return (FLOAT_T)src[ofsi] * OUT_DOUBLE;
	v1 = src + ofsi, v2 = v1 + 1;
	for(i = 0; i < tmp; i++){
		va += v1[i] * sharp_recip[i];
		vb += v2[i] * sharp_recip[i];
		s += fabs(sharp_recip[i]);
	}
	c = 1.0 / s;
	va *= c;
	vb *= c;
	return (va + (vb - va) * fp) * OUT_DOUBLE;
}

///r 
#define DEFAULT_LINEAR_P_ORDER 100
static int linear_n = DEFAULT_LINEAR_P_ORDER;

static DATA_T resample_linear_p(const sample_t *src, splen_t ofs, resample_rec_t *rec)
{
    const spos_t ofsi = ofs >> FRACTION_BITS;
    const fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofsls = rec->loop_start >> FRACTION_BITS;
    const spos_t ofsle = rec->loop_end >> FRACTION_BITS;
	spos_t ofsi2 = ofsi + 1;
    FLOAT_T fp;
	int32 v1, v2;

	switch(rec->mode){
	case RESAMPLE_MODE_PLAIN:
		// safe end+128 sample
		break;
	case RESAMPLE_MODE_LOOP:
		if(ofsi2 >= ofsle)
			ofsi2 = ofsi2 - (ofsle - ofsls);
		break;
	case RESAMPLE_MODE_BIDIR_LOOP:	
		if(rec->increment >= 0){
			if(ofsi2 >= ofsle)
				ofsi2 = (ofsle << 1) - ofsi2;
		}
		break;
	}
	v1 = src[ofsi];
	v2 = src[ofsi2] - v1;	
	fp = (FLOAT_T)ofsf * div_fraction;
	fp *= linear_n * 0.01; // parcent // angle
    return ((FLOAT_T)v1 + (FLOAT_T)v2 * fp) * OUT_INT16;

}

static DATA_T resample_linear_p_int32(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const int32 *src = (const int32*)srci;
    const spos_t ofsi = ofs >> FRACTION_BITS;
    const fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofsls = rec->loop_start >> FRACTION_BITS;
    const spos_t ofsle = rec->loop_end >> FRACTION_BITS;
	spos_t ofsi2 = ofsi + 1;
    FLOAT_T v1, v2, fp;
	
	switch(rec->mode){
	case RESAMPLE_MODE_PLAIN:
		// safe end+128 sample
		break;
	case RESAMPLE_MODE_LOOP:
		if(ofsi2 >= ofsle)
			ofsi2 = ofsi2 - (ofsle - ofsls);
		break;
	case RESAMPLE_MODE_BIDIR_LOOP:	
		if(rec->increment >= 0){
			if(ofsi2 >= ofsle)
				ofsi2 = (ofsle << 1) - ofsi2;
		}
		break;
	}
	v1 = src[ofsi];
	v2 = src[ofsi2] - v1;	
	fp = (FLOAT_T)ofsf * div_fraction;
	fp *= linear_n * 0.01; // parcent // angle
    return (v1 + v2 * fp) * OUT_INT32; // FLOAT_T
}

static DATA_T resample_linear_p_float(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const float *src = (const float*)srci;
    const spos_t ofsi = ofs >> FRACTION_BITS;
    const fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofsls = rec->loop_start >> FRACTION_BITS;
    const spos_t ofsle = rec->loop_end >> FRACTION_BITS;
	spos_t ofsi2 = ofsi + 1;
    FLOAT_T v1, v2, fp;
	
	switch(rec->mode){
	case RESAMPLE_MODE_PLAIN:
		// safe end+128 sample
		break;
	case RESAMPLE_MODE_LOOP:
		if(ofsi2 >= ofsle)
			ofsi2 = ofsi2 - (ofsle - ofsls);
		break;
	case RESAMPLE_MODE_BIDIR_LOOP:	
		if(rec->increment >= 0){
			if(ofsi2 >= ofsle)
				ofsi2 = (ofsle << 1) - ofsi2;
		}
		break;
	}
	v1 = src[ofsi];
	v2 = src[ofsi2] - v1;	
	fp = (FLOAT_T)ofsf * div_fraction;
	fp *= linear_n * 0.01; // parcent // angle
    return (v1 + v2 * fp) * OUT_FLOAT; // FLOAT_T
}

static DATA_T resample_linear_p_double(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const double *src = (const double*)srci;
    const spos_t ofsi = ofs >> FRACTION_BITS;
    const fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofsls = rec->loop_start >> FRACTION_BITS;
    const spos_t ofsle = rec->loop_end >> FRACTION_BITS;
	spos_t ofsi2 = ofsi + 1;
    FLOAT_T v1, v2, fp;
	
	switch(rec->mode){
	case RESAMPLE_MODE_PLAIN:
		// safe end+128 sample
		break;
	case RESAMPLE_MODE_LOOP:
		if(ofsi2 >= ofsle)
			ofsi2 = ofsi2 - (ofsle - ofsls);
		break;
	case RESAMPLE_MODE_BIDIR_LOOP:		
		if(rec->increment >= 0){
			if(ofsi2 >= ofsle)
				ofsi2 = (ofsle << 1) - ofsi2;
		}
		break;
	}
	v1 = src[ofsi];
	v2 = src[ofsi2] - v1;	
	fp = (FLOAT_T)ofsf * div_fraction;
	fp *= linear_n * 0.01; // parcent // angle
    return (v1 + v2 * fp) * OUT_DOUBLE; // FLOAT_T
}


///   sine
static DATA_T resample_sine(const sample_t *src, splen_t ofs, resample_rec_t *rec)
{
    const spos_t ofsi = ofs >> FRACTION_BITS;
    const fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofsls = rec->loop_start >> FRACTION_BITS;
    const spos_t ofsle = rec->loop_end >> FRACTION_BITS;
	spos_t ofsi2 = ofsi + 1;
    FLOAT_T fp;
	int32 v1, v2;

	switch(rec->mode){
	case RESAMPLE_MODE_PLAIN:
		// safe end+128 sample
		break;
	case RESAMPLE_MODE_LOOP:
		if(ofsi2 >= ofsle)
			ofsi2 = ofsi2 - (ofsle - ofsls);
		break;
	case RESAMPLE_MODE_BIDIR_LOOP:	
		if(rec->increment >= 0){
			if(ofsi2 >= ofsle)
				ofsi2 = (ofsle << 1) - ofsi2;
		}
		break;
	}
	v1 = src[ofsi];
	v2 = src[ofsi2] - v1;	
	fp = (FLOAT_T)ofsf * div_fraction;
	fp = 0.5 + sin((fp - 0.5) * M_PI) * DIV_2;
    return ((FLOAT_T)v1 + (FLOAT_T)v2 * fp)  * OUT_INT16; // FLOAT_T
}

static DATA_T resample_sine_int32(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const int32 *src = (const int32*)srci;
    const spos_t ofsi = ofs >> FRACTION_BITS;
    const fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofsls = rec->loop_start >> FRACTION_BITS;
    const spos_t ofsle = rec->loop_end >> FRACTION_BITS;
	spos_t ofsi2 = ofsi + 1;
    FLOAT_T v1, v2, fp;
	
	switch(rec->mode){
	case RESAMPLE_MODE_PLAIN:
		// safe end+128 sample
		break;
	case RESAMPLE_MODE_LOOP:
		if(ofsi2 >= ofsle)
			ofsi2 = ofsi2 - (ofsle - ofsls);
		break;
	case RESAMPLE_MODE_BIDIR_LOOP:	
		if(rec->increment >= 0){
			if(ofsi2 >= ofsle)
				ofsi2 = (ofsle << 1) - ofsi2;
		}
		break;
	}
	v1 = src[ofsi];
	v2 = src[ofsi2] - v1;	
	fp = (FLOAT_T)ofsf * div_fraction;
	fp = 0.5 + sin((fp - 0.5) * M_PI) * DIV_2;
    return (v1 + v2 * fp) * OUT_INT32; // FLOAT_T
}

static DATA_T resample_sine_float(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const float *src = (const float*)srci;
    const spos_t ofsi = ofs >> FRACTION_BITS;
    const fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofsls = rec->loop_start >> FRACTION_BITS;
    const spos_t ofsle = rec->loop_end >> FRACTION_BITS;
	spos_t ofsi2 = ofsi + 1;
    FLOAT_T v1, v2, fp;
	
	switch(rec->mode){
	case RESAMPLE_MODE_PLAIN:
		// safe end+128 sample
		break;
	case RESAMPLE_MODE_LOOP:
		if(ofsi2 >= ofsle)
			ofsi2 = ofsi2 - (ofsle - ofsls);
		break;
	case RESAMPLE_MODE_BIDIR_LOOP:	
		if(rec->increment >= 0){
			if(ofsi2 >= ofsle)
				ofsi2 = (ofsle << 1) - ofsi2;
		}
		break;
	}
	v1 = src[ofsi];
	v2 = src[ofsi2] - v1;	
	fp = (FLOAT_T)ofsf * div_fraction;
	fp = 0.5 + sin((fp - 0.5) * M_PI) * DIV_2;
    return (v1 + v2 * fp)  * OUT_FLOAT; // FLOAT_T
}

static DATA_T resample_sine_double(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const double *src = (const double*)srci;
    const spos_t ofsi = ofs >> FRACTION_BITS;
    const fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofsls = rec->loop_start >> FRACTION_BITS;
    const spos_t ofsle = rec->loop_end >> FRACTION_BITS;
	spos_t ofsi2 = ofsi + 1;
    FLOAT_T v1, v2, fp;
	
	switch(rec->mode){
	case RESAMPLE_MODE_PLAIN:
		// safe end+128 sample
		break;
	case RESAMPLE_MODE_LOOP:
		if(ofsi2 >= ofsle)
			ofsi2 = ofsi2 - (ofsle - ofsls);
		break;
	case RESAMPLE_MODE_BIDIR_LOOP:	
		if(rec->increment >= 0){
			if(ofsi2 >= ofsle)
				ofsi2 = (ofsle << 1) - ofsi2;
		}
		break;
	}
	v1 = src[ofsi];
	v2 = src[ofsi2] - v1;	
	fp = (FLOAT_T)ofsf * div_fraction;
	fp = 0.5 + sin((fp - 0.5) * M_PI) * DIV_2;
    return (v1 + v2 * fp) * OUT_DOUBLE; // FLOAT_T
}


///   square
static DATA_T resample_square(const sample_t *src, splen_t ofs, resample_rec_t *rec)
{
    const spos_t ofsi = ofs >> FRACTION_BITS;
    const fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofsls = rec->loop_start >> FRACTION_BITS;
    const spos_t ofsle = rec->loop_end >> FRACTION_BITS;
	spos_t ofsi2 = ofsi + 1;
    FLOAT_T fp;
	int32 v1, v2;

	switch(rec->mode){
	case RESAMPLE_MODE_PLAIN:
		// safe end+128 sample
		break;
	case RESAMPLE_MODE_LOOP:
		if(ofsi2 >= ofsle)
			ofsi2 = ofsi2 - (ofsle - ofsls);
		break;
	case RESAMPLE_MODE_BIDIR_LOOP:		
		if(rec->increment >= 0){
			if(ofsi2 >= ofsle)
				ofsi2 = (ofsle << 1) - ofsi2;
		}
		break;
	}
	v1 = src[ofsi];
	v2 = src[ofsi2] - v1;	
	fp = (FLOAT_T)ofsf * div_fraction;
	fp *= fp;
    return ((FLOAT_T)v1 + (FLOAT_T)v2 * fp)  * OUT_INT16; // FLOAT_T
}

static DATA_T resample_square_int32(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const int32 *src = (const int32*)srci;
    const spos_t ofsi = ofs >> FRACTION_BITS;
    const fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofsls = rec->loop_start >> FRACTION_BITS;
    const spos_t ofsle = rec->loop_end >> FRACTION_BITS;
	spos_t ofsi2 = ofsi + 1;
    FLOAT_T v1, v2, fp;
	
	switch(rec->mode){
	case RESAMPLE_MODE_PLAIN:
		// safe end+128 sample
		break;
	case RESAMPLE_MODE_LOOP:
		if(ofsi2 >= ofsle)
			ofsi2 = ofsi2 - (ofsle - ofsls);
		break;
	case RESAMPLE_MODE_BIDIR_LOOP:		
		if(rec->increment >= 0){
			if(ofsi2 >= ofsle)
				ofsi2 = (ofsle << 1) - ofsi2;
		}
		break;
	}
	v1 = src[ofsi];
	v2 = src[ofsi2] - v1;	
	fp = (FLOAT_T)ofsf * div_fraction;
	fp *= fp;
    return (v1 + v2 * fp)  * OUT_INT32; // FLOAT_T
}

static DATA_T resample_square_float(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const float *src = (const float*)srci;
    const spos_t ofsi = ofs >> FRACTION_BITS;
    const fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofsls = rec->loop_start >> FRACTION_BITS;
    const spos_t ofsle = rec->loop_end >> FRACTION_BITS;
	spos_t ofsi2 = ofsi + 1;
    FLOAT_T v1, v2, fp;
	
	switch(rec->mode){
	case RESAMPLE_MODE_PLAIN:
		// safe end+128 sample
		break;
	case RESAMPLE_MODE_LOOP:
		if(ofsi2 >= ofsle)
			ofsi2 = ofsi2 - (ofsle - ofsls);
		break;
	case RESAMPLE_MODE_BIDIR_LOOP:		
		if(rec->increment >= 0){
			if(ofsi2 >= ofsle)
				ofsi2 = (ofsle << 1) - ofsi2;
		}
		break;
	}
	v1 = src[ofsi];
	v2 = src[ofsi2] - v1;	
	fp = (FLOAT_T)ofsf * div_fraction;
	fp *= fp;
    return (v1 + v2 * fp)  * OUT_FLOAT; // FLOAT_T
}

static DATA_T resample_square_double(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const double *src = (const double*)srci;
    const spos_t ofsi = ofs >> FRACTION_BITS;
    const fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofsls = rec->loop_start >> FRACTION_BITS;
    const spos_t ofsle = rec->loop_end >> FRACTION_BITS;
	spos_t ofsi2 = ofsi + 1;
    FLOAT_T v1, v2, fp;
	
	switch(rec->mode){
	case RESAMPLE_MODE_PLAIN:
		// safe end+128 sample
		break;
	case RESAMPLE_MODE_LOOP:
		if(ofsi2 >= ofsle)
			ofsi2 = ofsi2 - (ofsle - ofsls);
		break;
	case RESAMPLE_MODE_BIDIR_LOOP:		
		if(rec->increment >= 0){
			if(ofsi2 >= ofsle)
				ofsi2 = ofsle + (ofsle - ofsi2); // (ofsle<<1) over spos_t 31bit
		}
		break;
	}
	v1 = src[ofsi];
	v2 = src[ofsi2] - v1;	
	fp = (FLOAT_T)ofsf * div_fraction;
	fp *= fp;
    return (v1 + v2 * fp) * OUT_DOUBLE; // FLOAT_T
}

///r 
#define DEFAULT_LANCZOS_ORDER 16
#define DEFAULT_LANCZOS_ORDER_MAX 96
static int lanczos_n = DEFAULT_LANCZOS_ORDER;
static int lanczos_samples = DEFAULT_LANCZOS_ORDER * (1 << FRACTION_BITS);
static FLOAT_T *lanczos_table = NULL;

static void free_lanczos_table(void)
{
    if(lanczos_table){
#ifdef ALIGN_SIZE
		aligned_free(lanczos_table);
#else
		free(lanczos_table);
#endif
		lanczos_table = NULL;
    }
}

static void initialize_lanczos_table(int n)
{
	int i, width = n;
	double inc, sum = 0.0;

	free_lanczos_table();
	lanczos_samples = width * mlt_fraction;	
#ifdef ALIGN_SIZE
	lanczos_table = (FLOAT_T *)aligned_malloc((lanczos_samples + 1) * sizeof(FLOAT_T), ALIGN_SIZE);
#else
	lanczos_table = (FLOAT_T *)safe_malloc((lanczos_samples + 1) * sizeof(FLOAT_T));
#endif
	memset(lanczos_table, 0, (lanczos_samples + 1) * sizeof(FLOAT_T));
	inc = (double)width / (double)lanczos_samples;
	for (i = 0; i < lanczos_samples + 1; ++i, sum += inc){
		if(fabs(sum) < width){
			double tmp1 = sum, tmp2 = sum / width;
			tmp1 = (fabs(tmp1) < 1.0e-8) ? 1.0 : (sin(tmp1 * M_PI) / (tmp1 * M_PI));
			tmp2 = (fabs(tmp2) < 1.0e-8) ? 1.0 : (sin(tmp2 * M_PI) / (tmp2 * M_PI));
			lanczos_table[i] = tmp1 * tmp2;
		}else
			lanczos_table[i] = 0.0;
	}
}

static DATA_T resample_lanczos(const sample_t *src, splen_t ofs, resample_rec_t *rec)
{
    const spos_t ofsi = ofs >> FRACTION_BITS;
    fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofsls = rec->loop_start >> FRACTION_BITS;
    const spos_t ofsle = rec->loop_end >> FRACTION_BITS;
	const sample_t *v1;
	spos_t ofstmp, len;
	fract_t incr;
    int32 i, lanczos_n2 = lanczos_n >> 1, width = lanczos_n2, dir;
    ALIGN FLOAT_T coef[DEFAULT_LANCZOS_ORDER_MAX * 2];
	FLOAT_T coef_sum = 0.0, sample_sum = 0.0;
	
	switch(rec->mode){
	case RESAMPLE_MODE_PLAIN:
		if(ofsi < 1)
			return (FLOAT_T)src[ofsi] * OUT_INT16; // FLOAT_T	
		if(ofsi < width)
			width = ofsi;
		break;
	case RESAMPLE_MODE_LOOP:
		if(ofsi < ofsls){
			if(ofsi < 1)
				return (FLOAT_T)src[ofsi] * OUT_INT16; // FLOAT_T
			if(ofsi < lanczos_n2)
				width = ofsi;
			if((ofsi + width) < ofsle)
				break; // normal
		}else if(((ofsi + width) < ofsle) && ((ofsi - width) >= ofsls))
			break; // normal
		incr = rec->increment > mlt_fraction ? (mlt_fraction * mlt_fraction / rec->increment) : mlt_fraction;
		for (i = width; i >= -width + 1; --i)
			coef_sum += coef[i + width - 1] = lanczos_table[abs(ofsf - i * incr)];	
		len = ofsle - ofsls; // loop_length
		ofstmp = ofsi - width;
		if(ofstmp < ofsls) {ofstmp += len;} // if loop_length == data_length need
		for (i = 0; i < width * 2; ++i){
			sample_sum += (FLOAT_T)src[ofstmp] * coef[i];
			if((++ofstmp) > ofsle) {ofstmp -= len;} // -= loop_length , jump loop_start
		}
		return (sample_sum / coef_sum) * OUT_INT16; // FLOAT_T	
		break;
	case RESAMPLE_MODE_BIDIR_LOOP:	
		if(rec->increment >= 0){ // normal dir
			if(ofsi < ofsls){
				if(ofsi < 1)
					return (FLOAT_T)src[ofsi] * OUT_INT16; // FLOAT_T
				if(ofsi < lanczos_n2)
					width = ofsi;
				if((ofsi + width) < ofsle)
					break; // normal
			}else if(((ofsi + width) < ofsle) && ((ofsi - width) >= ofsls))
				break; // normal
			dir = 1;
			ofstmp = ofsi - width;
			if(ofstmp < ofsls){ // if loop_length == data_length need				
				ofstmp = (ofsls << 1) - ofstmp;
				dir = -1;
			}				
			incr = rec->increment > mlt_fraction ? (mlt_fraction * mlt_fraction / rec->increment) : mlt_fraction;
		}else{ // reverse dir
			dir = -1;
			ofstmp = ofsi + width;
			if(ofstmp > ofsle){ // if loop_length == data_length need				
				ofstmp = (ofsle << 1) - ofstmp;
				dir = 1;
			}
			ofsf = mlt_fraction - ofsf;
			incr = -rec->increment;
			incr = incr > mlt_fraction ? (mlt_fraction * mlt_fraction / incr) : mlt_fraction;
		}
		for (i = width; i >= -width + 1; --i)
			coef_sum += coef[i + width - 1] = lanczos_table[abs(ofsf - i * incr)];
		for (i = 0; i < width * 2; ++i){
			sample_sum += (FLOAT_T)src[ofstmp] * coef[i];
			ofstmp += dir;
			if(dir < 0){ // -
				if(ofstmp <= ofsls) {dir = 1;}
			}else{ // +
				if(ofstmp >= ofsle) {dir = -1;}
			}
		}
		return (sample_sum / coef_sum) * OUT_INT16; // FLOAT_T
		break;
	}
	incr = rec->increment > mlt_fraction ? (mlt_fraction * mlt_fraction / rec->increment) : mlt_fraction;
	for (i = width; i >= -width + 1; --i)
		coef_sum += coef[i + width - 1] = lanczos_table[abs(ofsf - i * incr)];
	v1 = src + ofsi - width;
	width *= 2;
#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE) && defined(FLOAT_T_DOUBLE)
	if(width >= 16 && !(width & 0x7)){
		__m256d sum = _mm256_setzero_pd();
		__m128d sum1, sum2;	
		for (i = 0; i < width; i += 8){
#if (USE_X86_EXT_INTRIN >= 9)
			__m256i vec32 = _mm256_cvtepi16_epi32(_mm_loadu_si128((__m128i *)&v1[i])); // low i16*8 > i32*8
			__m128i vec1 = _mm256_extracti128_si256(vec32, 0x0);
			__m128i vec2 = _mm256_extracti128_si256(vec32, 0x1);
#else
			__m128i vec16a = _mm_loadu_si128((__m128i *)&v1[i]); // i16*8 (low			
			__m128i vec1 = _mm_cvtepi16_epi32(vec16a); // low i16*4 > i32*4 > d*4
			__m128i vec2 = _mm_cvtepi16_epi32(_mm_shuffle_epi32(vec16a, 0x4e)); // hi i16*4 > i32*4 > d*4
#endif
			sum = MM256_FMA_PD(_mm256_cvtepi32_pd(vec1), _mm256_load_pd(&coef[i]), sum);
			sum = MM256_FMA_PD(_mm256_cvtepi32_pd(vec2), _mm256_load_pd(&coef[i + 4]), sum);
		}
		sum1 = _mm256_extractf128_pd(sum, 0x0); // v0,v1
		sum2 = _mm256_extractf128_pd(sum, 0x1); // v2,v3
		sum1 = _mm_add_pd(sum1, sum2); // v0=v0+v2 v1=v1+v3
		sum1 = _mm_add_pd(sum1, _mm_shuffle_pd(sum1, sum1, 0x1)); // v0=v0+v1 v1=v1+v0	
		_mm_store_sd(&sample_sum, sum1);
	}else
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE) && defined(FLOAT_T_DOUBLE)
	if(width >= 16 && !(width & 0x3)){
		__m128d sum1 = _mm_setzero_pd();
		__m128d sum2 = _mm_setzero_pd();
		for (i = 0; i < width; i += 4){
#if (USE_X86_EXT_INTRIN >= 6) // sse4.1 , _mm_ cvtepi16_epi32()
			__m128i vi16 = _mm_loadu_si128((__m128i *)&v1[i]);
			__m128i vi32 = _mm_cvtepi16_epi32(vi16);
#else
			__m128i vi32 = _mm_set_epi32(v1[i + 3], v1[i + 2], v1[i + 1], v1[i]);
#endif
			__m128d vecd0 = _mm_cvtepi32_pd(vi32);
			__m128d vecd2 = _mm_cvtepi32_pd(_mm_shuffle_epi32(vi32, 0x4E)); // swap lo64 hi64
			sum1 = MM_FMA_PD(vecd0, _mm_load_pd(&coef[i]), sum1);
			sum2 = MM_FMA_PD(vecd2, _mm_load_pd(&coef[i + 2]), sum2);
		}
		sum1 = _mm_add_pd(sum1, sum2);
		sum1 = _mm_add_pd(sum1, _mm_shuffle_pd(sum1, sum1, 0x1)); // v0=v0+v1 v1=v1+v0
		_mm_store_sd(&sample_sum, sum1);
	}else
#endif
	{
		for (i = 0; i < width; ++i)
			sample_sum += (FLOAT_T)v1[i] * coef[i];
	}
	return (sample_sum / coef_sum) * OUT_INT16; // FLOAT_T	
}

static DATA_T resample_lanczos_int32(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const int32 *src = (const int32*)srci;
    const spos_t ofsi = ofs >> FRACTION_BITS;
    fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofsls = rec->loop_start >> FRACTION_BITS;
    const spos_t ofsle = rec->loop_end >> FRACTION_BITS;
	const int32 *v1;
	spos_t ofstmp, len;
	fract_t incr;
    int32 i, lanczos_n2 = lanczos_n >> 1, width = lanczos_n2, dir;
    ALIGN FLOAT_T coef[DEFAULT_LANCZOS_ORDER_MAX * 2];
	FLOAT_T coef_sum = 0.0, sample_sum = 0.0;
	
	switch(rec->mode){
	case RESAMPLE_MODE_PLAIN:
		if(ofsi < 1)
			return (FLOAT_T)src[ofsi] * OUT_INT32; // FLOAT_T	
		if(ofsi < lanczos_n2)
			width = ofsi;
		break;
	case RESAMPLE_MODE_LOOP:
		if(ofsi < ofsls){
			if(ofsi < 1)
				return (FLOAT_T)src[ofsi] * OUT_INT32; // FLOAT_T
			if(ofsi < lanczos_n2)
				width = ofsi;
			if((ofsi + width) < ofsle)
				break; // normal
		}else if(((ofsi + width) < ofsle) && ((ofsi - width) >= ofsls))
			break; // normal
		incr = rec->increment > mlt_fraction ? (mlt_fraction * mlt_fraction / rec->increment) : mlt_fraction;
		for (i = width; i >= -width + 1; --i)
			coef_sum += coef[i + width - 1] = lanczos_table[abs(ofsf - i * incr)];	
		len = ofsle - ofsls; // loop_length
		ofstmp = ofsi - width;
		if(ofstmp < ofsls) {ofstmp += len;} // if loop_length == data_length need
		for (i = 0; i < width * 2; ++i){
			sample_sum += (FLOAT_T)src[ofstmp] * coef[i];
			if((++ofstmp) > ofsle) {ofstmp -= len;} // -= loop_length , jump loop_start
		}
		return (sample_sum / coef_sum) * OUT_INT32; // FLOAT_T	
		break;
	case RESAMPLE_MODE_BIDIR_LOOP:
		if(rec->increment >= 0){ // normal dir	
			if(ofsi < ofsls){
				if(ofsi < 1)
					return (FLOAT_T)src[ofsi] * OUT_INT32; // FLOAT_T
				if(ofsi < lanczos_n2)
					width = ofsi;
				if((ofsi + width) < ofsle)
					break; // normal
			}else if(((ofsi + width) < ofsle) && ((ofsi - width) >= ofsls))
				break; // normal
			dir = 1;
			ofstmp = ofsi - width;
			if(ofstmp < ofsls){ // if loop_length == data_length need				
				ofstmp = (ofsls << 1) - ofstmp;
				dir = -1;
			}	
			incr = rec->increment > mlt_fraction ? (mlt_fraction * mlt_fraction / rec->increment) : mlt_fraction;
		}else{ // reverse dir
			dir = -1;
			ofstmp = ofsi + width;
			if(ofstmp > ofsle){ // if loop_length == data_length need				
				ofstmp = (ofsle << 1) - ofstmp;
				dir = 1;
			}
			ofsf = mlt_fraction - ofsf;
			incr = -rec->increment;
			incr = incr > mlt_fraction ? (mlt_fraction * mlt_fraction / incr) : mlt_fraction;
		}
		for (i = width; i >= -width + 1; --i)
			coef_sum += coef[i + width - 1] = lanczos_table[abs(ofsf - i * incr)];
		for (i = 0; i < width * 2; ++i){
			sample_sum += (FLOAT_T)src[ofstmp] * coef[i];
			ofstmp += dir;
			if(dir < 0){ // -
				if(ofstmp <= ofsls) {dir = 1;}
			}else{ // +
				if(ofstmp >= ofsle) {dir = -1;}
			}
		}
		return (sample_sum / coef_sum) * OUT_INT32; // FLOAT_T
		break;
	}
	incr = rec->increment > mlt_fraction ? (mlt_fraction * mlt_fraction / rec->increment) : mlt_fraction;
	for (i = width; i >= -width + 1; --i)
		coef_sum += coef[i + width - 1] = lanczos_table[abs(ofsf - i * incr)];
	v1 = src + ofsi - width;
	width *= 2;
#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE) && defined(FLOAT_T_DOUBLE)
	if(width >= 16 && !(width & 0x7)){
		__m256d sum = _mm256_setzero_pd();
		__m128d sum1, sum2;	
		for (i = 0; i < width; i += 8){
#if (USE_X86_EXT_INTRIN >= 9)
			__m256i vec32 = _mm256_loadu_si256((const __m256i *)&v1[i]);
			__m128i vec1 = _mm256_extracti128_si256(vec32, 0x0);
			__m128i vec2 = _mm256_extracti128_si256(vec32, 0x1);
#else
			__m128i vec1 = _mm_loadu_si128((__m128i *)&v1[i]);
			__m128i vec2 = _mm_loadu_si128((__m128i *)&v1[i + 4]);
#endif
			sum = MM256_FMA_PD(_mm256_cvtepi32_pd(vec1), _mm256_load_pd(&coef[i]), sum);
			sum = MM256_FMA_PD(_mm256_cvtepi32_pd(vec2), _mm256_load_pd(&coef[i + 4]), sum);
		}
		sum1 = _mm256_extractf128_pd(sum, 0x0); // v0,v1
		sum2 = _mm256_extractf128_pd(sum, 0x1); // v2,v3
		sum1 = _mm_add_pd(sum1, sum2); // v0=v0+v2 v1=v1+v3
		sum1 = _mm_add_pd(sum1, _mm_shuffle_pd(sum1, sum1, 0x1)); // v0=v0+v1 v1=v1+v0
		_mm_store_sd(&sample_sum, sum1);
	}else
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE) && defined(FLOAT_T_DOUBLE)
	if(width >= 16 && !(width & 0x3)){
		__m128d sum1 = _mm_setzero_pd();
		__m128d sum2 = _mm_setzero_pd();
		for (i = 0; i < width; i += 4){
			__m128i vec32i0 = _mm_loadu_si128((__m128i *)&v1[i]);
			sum1 = MM_FMA_PD(_mm_cvtepi32_pd(vec32i0), _mm_load_pd(&coef[i]), sum1);
			sum2 = MM_FMA_PD(_mm_cvtepi32_pd(_mm_shuffle_epi32(vec32i0, 0x4e)), _mm_load_pd(&coef[i + 2]), sum2);
		}
		sum1 = _mm_add_pd(sum1, sum2);
		sum1 = _mm_add_pd(sum1, _mm_shuffle_pd(sum1, sum1, 0x1)); // v0=v0+v1 v1=v1+v0
		_mm_store_sd(&sample_sum, sum1);
	}else
#endif
	{
		for (i = 0; i < width; ++i)
			sample_sum += (FLOAT_T)v1[i] * coef[i];
	}
	return (sample_sum / coef_sum) * OUT_INT32; // FLOAT_T	
}

static DATA_T resample_lanczos_float(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const float *src = (const float*)srci;
    const spos_t ofsi = ofs >> FRACTION_BITS;
    fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofsls = rec->loop_start >> FRACTION_BITS;
    const spos_t ofsle = rec->loop_end >> FRACTION_BITS;
	const float *v1;
	spos_t ofstmp, len;
	fract_t incr;
    int32 i, lanczos_n2 = lanczos_n >> 1, width = lanczos_n2, dir;
    ALIGN FLOAT_T coef[DEFAULT_LANCZOS_ORDER_MAX * 2];
	FLOAT_T coef_sum = 0.0, sample_sum = 0.0;
	
	switch(rec->mode){
	case RESAMPLE_MODE_PLAIN:
		if(ofsi < 1)
			return (FLOAT_T)src[ofsi] * OUT_FLOAT; // FLOAT_T	
		if(ofsi < lanczos_n2)
			width = ofsi;
		break;
	case RESAMPLE_MODE_LOOP:
		if(ofsi < ofsls){
			if(ofsi < 1)
				return (FLOAT_T)src[ofsi] * OUT_FLOAT; // FLOAT_T
			if(ofsi < lanczos_n2)
				width = ofsi;
			if((ofsi + width) < ofsle)
				break; // normal
		}else if(((ofsi + width) < ofsle) && ((ofsi - width) >= ofsls))
			break; // normal
		incr = rec->increment > mlt_fraction ? (mlt_fraction * mlt_fraction / rec->increment) : mlt_fraction;
		for (i = width; i >= -width + 1; --i)
			coef_sum += coef[i + width - 1] = lanczos_table[abs(ofsf - i * incr)];	
		len = ofsle - ofsls; // loop_length
		ofstmp = ofsi - width;
		if(ofstmp < ofsls) {ofstmp += len;} // if loop_length == data_length need
		for (i = 0; i < width * 2; ++i){
			sample_sum += (FLOAT_T)src[ofstmp] * coef[i];
			if((++ofstmp) > ofsle) {ofstmp -= len;} // -= loop_length , jump loop_start
		}
		return (sample_sum / coef_sum) * OUT_FLOAT; // FLOAT_T	
		break;
	case RESAMPLE_MODE_BIDIR_LOOP:	
		if(rec->increment >= 0){ // normal dir
			if(ofsi < ofsls){
				if(ofsi < 1)
					return (FLOAT_T)src[ofsi] * OUT_FLOAT; // FLOAT_T
				if(ofsi < lanczos_n2)
					width = ofsi;
				if((ofsi + width) < ofsle)
					break; // normal
			}else if(((ofsi + width) < ofsle) && ((ofsi - width) >= ofsls))
				break; // normal
			dir = 1;
			ofstmp = ofsi - width;
			if(ofstmp < ofsls){ // if loop_length == data_length need				
				ofstmp = (ofsls << 1) - ofstmp;
				dir = -1;
			}	
			incr = rec->increment > mlt_fraction ? (mlt_fraction * mlt_fraction / rec->increment) : mlt_fraction;
		}else{ // reverse dir
			dir = -1;
			ofstmp = ofsi + width;
			if(ofstmp > ofsle){ // if loop_length == data_length need				
				ofstmp = (ofsle << 1) - ofstmp;
				dir = 1;
			}
			ofsf = mlt_fraction - ofsf;
			incr = -rec->increment;
			incr = incr > mlt_fraction ? (mlt_fraction * mlt_fraction / incr) : mlt_fraction;
		}
		for (i = width; i >= -width + 1; --i)
			coef_sum += coef[i + width - 1] = lanczos_table[abs(ofsf - i * incr)];
		for (i = 0; i < width * 2; ++i){
			sample_sum += (FLOAT_T)src[ofstmp] * coef[i];
			ofstmp += dir;
			if(dir < 0){ // -
				if(ofstmp <= ofsls) {dir = 1;}
			}else{ // +
				if(ofstmp >= ofsle) {dir = -1;}
			}
		}
		return (sample_sum / coef_sum) * OUT_FLOAT; // FLOAT_T
		break;
	}
	incr = rec->increment > mlt_fraction ? (mlt_fraction * mlt_fraction / rec->increment) : mlt_fraction;
	for (i = width; i >= -width + 1; --i)
		coef_sum += coef[i + width - 1] = lanczos_table[abs(ofsf - i * incr)];
	v1 = src + ofsi - width;
	width *= 2;
#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE) && defined(FLOAT_T_DOUBLE)
	if(width >= 16 && !(width & 0x7)){
		__m256d sum = _mm256_setzero_pd();
		__m128d sum1, sum2;	
		for (i = 0; i < width; i += 8){
			__m256 vecf = _mm256_loadu_ps(&v1[i]);
			__m128 vec1 = _mm256_extractf128_ps(vecf, 0x0);
			__m128 vec2 = _mm256_extractf128_ps(vecf, 0x1);
			sum = MM256_FMA_PD(_mm256_cvtps_pd(vec1), _mm256_load_pd(&coef[i]), sum);
			sum = MM256_FMA_PD(_mm256_cvtps_pd(vec2), _mm256_load_pd(&coef[i + 4]), sum);
		}
		sum1 = _mm256_extractf128_pd(sum, 0x0); // v0,v1
		sum2 = _mm256_extractf128_pd(sum, 0x1); // v2,v3
		sum1 = _mm_add_pd(sum1, sum2); // v0=v0+v2 v1=v1+v3
		sum1 = _mm_add_pd(sum1, _mm_shuffle_pd(sum1, sum1, 0x1)); // v0=v0+v1 v1=v1+v0
		_mm_store_sd(&sample_sum, sum1);
	}else
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE) && defined(FLOAT_T_DOUBLE)
	if(width >= 16 && !(width & 0x3)){
		__m128d sum1 = _mm_setzero_pd();
		__m128d sum2 = _mm_setzero_pd();
		for (i = 0; i < width; i += 4){
			__m128 vecf0 = _mm_loadu_ps(&v1[i]);
			sum1 = MM_FMA_PD(_mm_cvtps_pd(vecf0), _mm_load_pd(&coef[i]), sum1);
			sum2 = MM_FMA_PD(_mm_cvtps_pd(_mm_shuffle_ps(vecf0, vecf0, 0x4e)), _mm_load_pd(&coef[i + 2]), sum2);
		}
		sum1 = _mm_add_pd(sum1, sum2);
		sum1 = _mm_add_pd(sum1, _mm_shuffle_pd(sum1, sum1, 0x1)); // v0=v0+v1 v1=v1+v0
		_mm_store_sd(&sample_sum, sum1);
	}else
#endif
	{
		for (i = 0; i < width; ++i)
			sample_sum += (FLOAT_T)v1[i] * coef[i];
	}
	return (sample_sum / coef_sum) * OUT_FLOAT; // FLOAT_T	
}

static DATA_T resample_lanczos_double(const sample_t *srci, splen_t ofs, resample_rec_t *rec)
{
    const double *src = (const double*)srci;
    const spos_t ofsi = ofs >> FRACTION_BITS;
    fract_t ofsf = ofs & FRACTION_MASK;
    const spos_t ofsls = rec->loop_start >> FRACTION_BITS;
    const spos_t ofsle = rec->loop_end >> FRACTION_BITS;
	const double *v1;
	spos_t ofstmp, len;
	fract_t incr;
    int32 i, lanczos_n2 = lanczos_n >> 1, width = lanczos_n2, dir;
    ALIGN FLOAT_T coef[DEFAULT_LANCZOS_ORDER_MAX * 2];
	FLOAT_T coef_sum = 0.0, sample_sum = 0.0;
	
	switch(rec->mode){
	case RESAMPLE_MODE_PLAIN:
		if(ofsi < 1)
			return (FLOAT_T)src[ofsi] * OUT_DOUBLE; // FLOAT_T	
		if(ofsi < lanczos_n2)
			width = ofsi;
		break;
	case RESAMPLE_MODE_LOOP:
		if(ofsi < ofsls){
			if(ofsi < 1)
				return (FLOAT_T)src[ofsi] * OUT_DOUBLE; // FLOAT_T
			if(ofsi < lanczos_n2)
				width = ofsi;
			if((ofsi + width) < ofsle)
				break; // normal
		}else if(((ofsi + width) < ofsle) && ((ofsi - width) >= ofsls))
			break; // normal
		incr = rec->increment > mlt_fraction ? (mlt_fraction * mlt_fraction / rec->increment) : mlt_fraction;
		for (i = width; i >= -width + 1; --i)
			coef_sum += coef[i + width - 1] = lanczos_table[abs(ofsf - i * incr)];	
		len = ofsle - ofsls; // loop_length
		ofstmp = ofsi - width;
		if(ofstmp < ofsls) {ofstmp += len;} // if loop_length == data_length need
		for (i = 0; i < width * 2; ++i){
			sample_sum += (FLOAT_T)src[ofstmp] * coef[i];
			if((++ofstmp) > ofsle) {ofstmp -= len;} // -= loop_length , jump loop_start
		}
		return (sample_sum / coef_sum) * OUT_DOUBLE; // FLOAT_T	
		break;
	case RESAMPLE_MODE_BIDIR_LOOP:	
		if(rec->increment >= 0){ // normal dir
			if(ofsi < ofsls){
				if(ofsi < 1)
					return (FLOAT_T)src[ofsi] * OUT_DOUBLE; // FLOAT_T
				if(ofsi < lanczos_n2)
					width = ofsi;
				if((ofsi + width) < ofsle)
					break; // normal
			}else if(((ofsi + width) < ofsle) && ((ofsi - width) >= ofsls))
				break; // normal
			dir = 1;
			ofstmp = ofsi - width;
			if(ofstmp < ofsls){ // if loop_length == data_length need				
				ofstmp = (ofsls << 1) - ofstmp;
				dir = -1;
			}	
			incr = rec->increment > mlt_fraction ? (mlt_fraction * mlt_fraction / rec->increment) : mlt_fraction;
		}else{ // reverse dir
			dir = -1;
			ofstmp = ofsi + width;
			if(ofstmp > ofsle){ // if loop_length == data_length need				
				ofstmp = (ofsle << 1) - ofstmp;
				dir = 1;
			}
			ofsf = mlt_fraction - ofsf;
			incr = -rec->increment;
			incr = incr > mlt_fraction ? (mlt_fraction * mlt_fraction / incr) : mlt_fraction;
		}
		for (i = width; i >= -width + 1; --i)
			coef_sum += coef[i + width - 1] = lanczos_table[abs(ofsf - i * incr)];
		for (i = 0; i < width * 2; ++i){
			sample_sum += (FLOAT_T)src[ofstmp] * coef[i];
			ofstmp += dir;
			if(dir < 0){ // -
				if(ofstmp <= ofsls) {dir = 1;}
			}else{ // +
				if(ofstmp >= ofsle) {dir = -1;}
			}
		}
		return (sample_sum / coef_sum) * OUT_DOUBLE; // FLOAT_T
		break;
	}
	incr = rec->increment > mlt_fraction ? (mlt_fraction * mlt_fraction / rec->increment) : mlt_fraction;
	for (i = width; i >= -width + 1; --i)
		coef_sum += coef[i + width - 1] = lanczos_table[abs(ofsf - i * incr)];
	v1 = src + ofsi - width;
	width *= 2;
#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE) && defined(FLOAT_T_DOUBLE)
	if(width >= 16 && !(width & 0x7)){
		__m256d sum = _mm256_setzero_pd();
		__m128d sum1, sum2;	
		for (i = 0; i < width; i += 8){
			sum = MM256_FMA_PD(_mm256_loadu_pd(&v1[i]), _mm256_load_pd(&coef[i]), sum);
			sum = MM256_FMA_PD(_mm256_loadu_pd(&v1[i + 4]), _mm256_load_pd(&coef[i + 4]), sum);
		}
		sum1 = _mm256_extractf128_pd(sum, 0x0); // v0,v1
		sum2 = _mm256_extractf128_pd(sum, 0x1); // v2,v3
		sum1 = _mm_add_pd(sum1, sum2); // v0=v0+v2 v1=v1+v3
		sum1 = _mm_add_pd(sum1, _mm_shuffle_pd(sum1, sum1, 0x1)); // v0=v0+v1 v1=v1+v0
		_mm_store_sd(&sample_sum, sum1);
	}else
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE) && defined(FLOAT_T_DOUBLE)
	if(width >= 16 && !(width & 0x3)){
		__m128d sum1 = _mm_setzero_pd();
		__m128d sum2 = _mm_setzero_pd();
		for (i = 0; i < width; i += 4){		
			sum1 = MM_FMA_PD(_mm_loadu_pd(&v1[i]), _mm_load_pd(&coef[i]), sum1);
			sum2 = MM_FMA_PD(_mm_loadu_pd(&v1[i + 2]), _mm_load_pd(&coef[i + 2]), sum2);
		}
		sum1 = _mm_add_pd(sum1, sum2);
		sum1 = _mm_add_pd(sum1, _mm_shuffle_pd(sum1, sum1, 0x1)); // v0=v0+v1 v1=v1+v0
		_mm_store_sd(&sample_sum, sum1);
	}else
#endif
	{
		for (i = 0; i < width; ++i)
			sample_sum += v1[i] * coef[i];
	}
	return (sample_sum / coef_sum) * OUT_DOUBLE; // FLOAT_T
}



typedef DATA_T (*resampler_t)(const sample_t*, splen_t, resample_rec_t *);

static resampler_t resamplers[4][RESAMPLE_MAX] = {{
// sample_t
// cfg sort
    resample_none,
	resample_linear,
    resample_cspline,
    resample_lagrange,
	resample_newton,
    resample_gauss,
    resample_sharp,
    resample_linear_p,
    resample_sine,
    resample_square,
    resample_lanczos,
},{
// int32
// cfg sort
    resample_none_int32,
	resample_linear_int32,
    resample_cspline_int32,
    resample_lagrange_int32,
	resample_newton_int32,
    resample_gauss_int32,
    resample_sharp_int32,
    resample_linear_p_int32,
    resample_sine_int32,
    resample_square_int32,
    resample_lanczos_int32,
},{
// float
// cfg sort
    resample_none_float,
	resample_linear_float,
    resample_cspline_float,
    resample_lagrange_float,
	resample_newton_float,
    resample_gauss_float,
    resample_sharp_float,
    resample_linear_p_float,
    resample_sine_float,
    resample_square_float,
    resample_lanczos_float,
},{
// double
// cfg sort
    resample_none_double,
	resample_linear_double,
    resample_cspline_double,
    resample_lagrange_double,
	resample_newton_double,
    resample_gauss_double,
    resample_sharp_double,
    resample_linear_p_double,
    resample_sine_double,
    resample_square_double,
    resample_lanczos_double,
}};




	
#define RESAMPLATION *dest++ = vp->resrc.current_resampler(src, ofs, &vp->resrc);

// for pre_resample() and recache.c
static resampler_t current_resampler;

#define PRE_RESAMPLATION *dest++ = current_resampler(src, ofs, &resrc);

/* exported for recache.c */
void set_resamplation(int data_type)
{ 
	current_resampler = resamplers[data_type][opt_resample_type];
}

/* exported for recache.c */
DATA_T do_resamplation(const sample_t *src, splen_t ofs, resample_rec_t *rec)
{ 
	// DATA_T_FLOAT or DATA_T_DOUBLE -1.0 ~ 1.0 
	// DATA_T_IBT16 or DATA_T_IBT32 -32768 ~ 32767
	return current_resampler(src, ofs, rec); // filter none
}

/* return the current resampling algorithm */
int get_current_resampler(void)
{
	return opt_resample_type;
}

/* set the current resampling algorithm */
int set_current_resampler(int type)
{
#ifdef FIXED_RESAMPLATION
    return -1;
#else
///r
    if (type < 0 || type > (RESAMPLE_MAX - 1)){
		opt_resample_type = DEFAULT_RESAMPLATION_NUM;
		return -1;
	}
	opt_resample_type = type;
	return 0;
#endif
}

/* #define FINALINTERP if (ofs < le) *dest++=src[(ofs>>FRACTION_BITS)-1]/2; */
#define FINALINTERP /* Nothing to do after TiMidity++ 2.9.0 */
/* So it isn't interpolation. At least it's final. */


///r
#ifdef PRECALC_LOOPS
#define PRECALC_LOOP_COUNT(start, end, incr) (int32)(((splen_t)((end) - (start) + (incr) - 1)) / (incr))
#endif /* PRECALC_LOOPS */


///r
/* change the parameter for the current resampling algorithm */
int set_resampler_parm(int val)
{
	if (opt_resample_type != RESAMPLE_GAUSS)
		gauss_n = 2;
	if (opt_resample_type != RESAMPLE_LANCZOS)
		lanczos_n = 4;

	if (opt_resample_type == RESAMPLE_LINEAR_P) {
		if (val < 0)
		  return -1;
		else if (val == 0)
			linear_n = DEFAULT_LINEAR_P_ORDER;
		else if (val >= 100)
			linear_n = 100;
		else 
			linear_n = val;
	} else if (opt_resample_type == RESAMPLE_SHARP) {
		if (val < 0)
		  return -1;
		else if (val == 0)
			sharp_n = DEFAULT_SHARP_ORDER;
		else if (val >= 20)
			sharp_n = 20;
		else if (val >= 2)
			sharp_n = val;
		else 
			sharp_n = 2;
    } else if (opt_resample_type == RESAMPLE_GAUSS) {
		if (val < 0)
		  return -1;
		else if (val == 0)
			gauss_n = DEFAULT_GAUSS_ORDER;
		else if (val >= 32)
			gauss_n = 32;
		else if (val >= 24)
			gauss_n = 24;
		else if (val >= 16)
			gauss_n = 16;
		else if (val >= 8)
			gauss_n = 8;
		else if (val >= 4)
			gauss_n = 4;
		else 
			gauss_n = 2;
    } else if (opt_resample_type == RESAMPLE_NEWTON) {
		if (val < 0)
		  return -1;
		else if (val == 0)
			newt_n = DEFAULT_NEWTON_ORDER;
		else if(val > 57) // 45
			newt_n = 57;
		else if ((val % 2) == 0)
			newt_n = val + 1;
		else 
			newt_n = val;
		/* set optimal value for newt_max */
		newt_max = newt_n * 1.57730263158 - 1.875328947;
		if (newt_max < newt_n)
			newt_max = newt_n;
		if (newt_max > 57)
			newt_max = 57;
    } else if (opt_resample_type == RESAMPLE_LANCZOS) {
		if (val < 0)
		  return -1;
		else if (val == 0)
			lanczos_n = DEFAULT_LANCZOS_ORDER;
		else if (val >= 96)
			lanczos_n = 96;
		else if (val >= 64)
			lanczos_n = 64;
		else if (val >= 48)
			lanczos_n = 48;
		else if (val >= 32)
			lanczos_n = 32;
		else if (val >= 24)
			lanczos_n = 24;
		else if (val >= 16)
			lanczos_n = 16;
		else if (val >= 12)
			lanczos_n = 12;
		else if (val >= 8)
			lanczos_n = 8;
		else
			lanczos_n = 4;
	}
    return 0;
}


/*************** over_sampling *****************/

FLOAT_T div_over_sampling_ratio = 1.0;

void set_resampler_over_sampling(int val)
{
	if (val >= RESAMPLE_OVER_SAMPLING_MAX)
		opt_resample_over_sampling = RESAMPLE_OVER_SAMPLING_MAX;
#if 1 // optimize
	else if (val >= 8)
		opt_resample_over_sampling = 8;
	else if (val >= 4)
		opt_resample_over_sampling = 4;
	else if (val >= 2)
		opt_resample_over_sampling = 2;
#else
	else if (val >= 2)
		opt_resample_over_sampling = val;
#endif
	else 
		opt_resample_over_sampling = 0;	
	if(opt_resample_over_sampling){
		div_over_sampling_ratio = (double)1.0 / (double)opt_resample_over_sampling;
	}else{
		div_over_sampling_ratio = 1.0;
	}
}

void resample_down_sampling(DATA_T *ptr, int32 count)
{
	int32 i;
	
#if (USE_X86_EXT_INTRIN >= 2)
	_mm_prefetch((const char *)&ptr, _MM_HINT_T0);
#endif
	switch(opt_resample_over_sampling){
	case 0:
		break;
#if 1 // optimize
	case 2:
		for(i = 0; i < count; i++){
			int32 ofs = i * 2;
			FLOAT_T tmp = ptr[ofs + 0] + ptr[ofs + 1];
			ptr[i] = tmp * DIV_2;
		}
		break;
	case 4:
#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
		{ // 8samples
			const double divnv = DIV_4;
			const __m256d divn = _mm256_broadcast_sd(&divnv);
			for(i = 0; i < count; i += 8){
				int32 ofs = i * 4;
				__m256d	sum1 = _mm256_load_pd(&ptr[ofs + 0]); // v0,v1,v2,v3
				__m256d	sum2 = _mm256_load_pd(&ptr[ofs + 4]); // v4,v5,v6,v7
				__m256d	sum3 = _mm256_load_pd(&ptr[ofs + 8]); // v8,v9,v10,v11
				__m256d	sum4 = _mm256_load_pd(&ptr[ofs + 12]); // v12,v13,v14,v15
				__m256d	sum5 = _mm256_load_pd(&ptr[ofs + 16]); // v16,....
				__m256d	sum6 = _mm256_load_pd(&ptr[ofs + 20]); // v20,....
				__m256d	sum7 = _mm256_load_pd(&ptr[ofs + 24]); // v24,....
				__m256d	sum8 = _mm256_load_pd(&ptr[ofs + 28]); // v28,....			
				//_MM_TRANSPOSE4_PS(sum1, sum2, sum3, sum4)								
				__m256d tmp0 = _mm256_shuffle_pd(sum1, sum2, 0x00); // v0,v4,v2,v6
				__m256d tmp1 = _mm256_shuffle_pd(sum1, sum2, 0x0F); // v1,v5,v3,v7
				__m256d tmp2 = _mm256_shuffle_pd(sum3, sum4, 0x00); // v8,v12,v10,v14
				__m256d tmp3 = _mm256_shuffle_pd(sum3, sum4, 0x0F); // v9,v13,v11,v15
				sum1 = _mm256_permute2f128_pd(tmp0, tmp2, (0|(2<<4))); // v0,v4,v8,v12
				sum2 = _mm256_permute2f128_pd(tmp1, tmp3, (0|(2<<4))); // v1,v5,v9,v13
				sum3 = _mm256_permute2f128_pd(tmp0, tmp2, (1|(3<<4))); // v2,v6,10,v14
				sum4 = _mm256_permute2f128_pd(tmp1, tmp3, (1|(3<<4))); // v3,v7,v11,v15				
				//_MM_TRANSPOSE4_PS(sum5, sum6, sum7, sum8)
				tmp0 = _mm256_shuffle_pd(sum5, sum6, 0x00); // v16,....
				tmp1 = _mm256_shuffle_pd(sum5, sum6, 0x0F); // v17,....
				tmp2 = _mm256_shuffle_pd(sum7, sum8, 0x00); // v24,....
				tmp3 = _mm256_shuffle_pd(sum7, sum8, 0x0F); // v25,....
				sum5 = _mm256_permute2f128_pd(tmp0, tmp2, (0|(2<<4))); // v16,....
				sum6 = _mm256_permute2f128_pd(tmp1, tmp3, (0|(2<<4))); // v17,....
				sum7 = _mm256_permute2f128_pd(tmp0, tmp2, (1|(3<<4))); // v18,....
				sum8 = _mm256_permute2f128_pd(tmp1, tmp3, (1|(3<<4))); // v19,....			
				sum1 = _mm256_add_pd(sum1, sum2);
				sum3 = _mm256_add_pd(sum3, sum4);
				sum5 = _mm256_add_pd(sum5, sum6);
				sum7 = _mm256_add_pd(sum7, sum8);
				sum1 = _mm256_add_pd(sum1, sum3);
				sum5 = _mm256_add_pd(sum5, sum7);
				sum1 = _mm256_mul_pd(sum1, divn);
				sum5 = _mm256_mul_pd(sum5, divn);
				_mm256_store_pd(&ptr[i    ], sum1);
				_mm256_store_pd(&ptr[i + 4], sum5);
			}
		}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
		{ // 4samples
			const __m128d divn = _mm_set1_pd(DIV_4);
			for(i = 0; i < count; i += 4){
				int32 ofs = i * 4;
				__m128d	sum1 = _mm_load_pd(&ptr[ofs + 0]); // v0,v1
				__m128d	sum2 = _mm_load_pd(&ptr[ofs + 2]); // v2,v3
				__m128d	sum3 = _mm_load_pd(&ptr[ofs + 4]); // v4,v5
				__m128d	sum4 = _mm_load_pd(&ptr[ofs + 6]); // v6,v7
				__m128d	sum5 = _mm_load_pd(&ptr[ofs + 8]); // v8,v9
				__m128d	sum6 = _mm_load_pd(&ptr[ofs + 10]); // v10,v11
				__m128d	sum7 = _mm_load_pd(&ptr[ofs + 12]); // v12,v13
				__m128d	sum8 = _mm_load_pd(&ptr[ofs + 14]); // v14,v15
				// ([1,2] [3,4]) ([5,6] [7,8]) ...
				sum1 = _mm_add_pd(sum1, sum2); // (v0+v2) (v1+v3)
				sum3 = _mm_add_pd(sum3, sum4); // (v4+v6) (v5+v7)
				sum5 = _mm_add_pd(sum5, sum6); // (v8+v10) (v9+v11)
				sum7 = _mm_add_pd(sum7, sum8); // (v12+v14) (v13+v15)
				sum2 = _mm_shuffle_pd(sum1, sum3, 0x0); // (v0+v2) (v4+v6)
				sum4 = _mm_shuffle_pd(sum1, sum3, 0x3); // (v1+v3) (v5+v7)
				sum6 = _mm_shuffle_pd(sum5, sum7, 0x0); // (v8+v10) (v12+v14)
				sum8 = _mm_shuffle_pd(sum5, sum7, 0x3); // (v9+v11) (v13+v15)				
				sum2 = _mm_add_pd(sum2, sum4); // ((v0+v2)+(v1+v3)) ((v4+v6)+(v5+v7))
				sum6 = _mm_add_pd(sum6, sum8); // ((v8+v10)+(v9+v11)) ((v12+v14)+(v13+v15))
				sum2 = _mm_mul_pd(sum2, divn);
				sum6 = _mm_mul_pd(sum6, divn);
				_mm_store_pd(&ptr[i    ], sum2);
				_mm_store_pd(&ptr[i + 2], sum6);
			}
		}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
		{ // 8samples
			const float divnv = DIV_4;
			const __m128 divn = _mm_load1_ps(&divnv);
			for(i = 0; i < count; i += 8){
				int32 ofs = i * 4;
				__m128	sum1 = _mm_load_ps(&ptr[ofs + 0]); // v0,v1,v2,v3
				__m128	sum2 = _mm_load_ps(&ptr[ofs + 4]); // v4,v5,v6,v7
				__m128	sum3 = _mm_load_ps(&ptr[ofs + 8]); // v8,v9,v10,v11
				__m128	sum4 = _mm_load_ps(&ptr[ofs + 12]); // v12,v13,v14,v15
				__m128	sum5 = _mm_load_ps(&ptr[ofs + 16]); // v16,....
				__m128	sum6 = _mm_load_ps(&ptr[ofs + 20]); // v20,....
				__m128	sum7 = _mm_load_ps(&ptr[ofs + 24]); // v24,....
				__m128	sum8 = _mm_load_ps(&ptr[ofs + 28]); // v28,....			
				//_MM_TRANSPOSE4_PS(sum1, sum2, sum3, sum4)				
				__m128 tmp0 = _mm_shuffle_ps(sum1, sum2, 0x44); // v0,v1,v4,v5
				__m128 tmp2 = _mm_shuffle_ps(sum1, sum2, 0xEE); // v2,v3,v6,v7
				__m128 tmp1 = _mm_shuffle_ps(sum3, sum4, 0x44); // v8,v9,v12,v13
				__m128 tmp3 = _mm_shuffle_ps(sum3, sum4, 0xEE); // v10,v11,v14,v5
				sum1 = _mm_shuffle_ps(tmp0, tmp1, 0x88); // v0,v4,v8,v12
				sum2 = _mm_shuffle_ps(tmp0, tmp1, 0xDD); // v1,v5,v9,v13
				sum3 = _mm_shuffle_ps(tmp2, tmp3, 0x88); // v2,v6,10,v15
				sum4 = _mm_shuffle_ps(tmp2, tmp3, 0xDD); // v3,v7,v11,v16				
				//_MM_TRANSPOSE4_PS(sum5, sum6, sum7, sum8)
				tmp0 = _mm_shuffle_ps(sum5, sum6, 0x44); // v16,....
				tmp2 = _mm_shuffle_ps(sum5, sum6, 0xEE); // v18,....
				tmp1 = _mm_shuffle_ps(sum7, sum8, 0x44); // v24,....
				tmp3 = _mm_shuffle_ps(sum7, sum8, 0xEE); // v26,....
				sum5 = _mm_shuffle_ps(tmp0, tmp1, 0x88); // v16,....
				sum6 = _mm_shuffle_ps(tmp0, tmp1, 0xDD); // v17,....
				sum7 = _mm_shuffle_ps(tmp2, tmp3, 0x88); // v18,....
				sum8 = _mm_shuffle_ps(tmp2, tmp3, 0xDD); // v19,....
				sum1 = _mm_add_ps(sum1, sum2);
				sum3 = _mm_add_ps(sum3, sum4);
				sum5 = _mm_add_ps(sum5, sum6);
				sum7 = _mm_add_ps(sum7, sum8);
				sum1 = _mm_add_ps(sum1, sum3);
				sum5 = _mm_add_ps(sum5, sum7);
				sum1 = _mm_mul_ps(sum1, divn);
				sum5 = _mm_mul_ps(sum5, divn);
				_mm_store_ps(&ptr[i    ], sum1);
				_mm_store_ps(&ptr[i + 4], sum5);
			}
		}
#else
		for(i = 0; i < count; i++){
			int32 ofs = i * 4;
			FLOAT_T tmp = ptr[ofs + 0] + ptr[ofs + 1] + ptr[ofs + 2] + ptr[ofs + 3];
			ptr[i] = tmp * DIV_4;
		}
#endif
		break;
	case 8:
#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
		{ // 8samples
			const double divnv = DIV_8;
			const __m256d divn = _mm256_broadcast_sd(&divnv);
			for(i = 0; i < count; i += 8){
				int32 ofs = i * 8;
				__m256d	sum1 = _mm256_load_pd(&ptr[ofs + 0]); // v0,v1,v2,v3
				__m256d	sum2 = _mm256_load_pd(&ptr[ofs + 4]); // v4,v5,v6,v7
				__m256d	sum3 = _mm256_load_pd(&ptr[ofs + 8]); // v8,v9,v10,v11
				__m256d	sum4 = _mm256_load_pd(&ptr[ofs + 12]); // v12,v13,v14,v15
				__m256d	sum5 = _mm256_load_pd(&ptr[ofs + 16]); // v16,....
				__m256d	sum6 = _mm256_load_pd(&ptr[ofs + 20]); // v20,....
				__m256d	sum7 = _mm256_load_pd(&ptr[ofs + 24]); // v24,....
				__m256d	sum8 = _mm256_load_pd(&ptr[ofs + 28]); // v28,....				
				__m256d	sum9 = _mm256_load_pd(&ptr[ofs + 32]); // v32,....
				__m256d	sum10 = _mm256_load_pd(&ptr[ofs + 36]); // v36,....
				__m256d	sum11 = _mm256_load_pd(&ptr[ofs + 40]); // v40,....
				__m256d	sum12 = _mm256_load_pd(&ptr[ofs + 44]); // v44,....
				__m256d	sum13 = _mm256_load_pd(&ptr[ofs + 48]); // v48,....
				__m256d	sum14 = _mm256_load_pd(&ptr[ofs + 52]); // v52,....
				__m256d	sum15 = _mm256_load_pd(&ptr[ofs + 56]); // v56,....
				__m256d	sum16 = _mm256_load_pd(&ptr[ofs + 60]); // v60,....
				sum1 = _mm256_add_pd(sum1, sum2); // v0~v7
				sum3 = _mm256_add_pd(sum3, sum4); // v8~v15
				sum5 = _mm256_add_pd(sum5, sum6); // v16~v23
				sum7 = _mm256_add_pd(sum7, sum8); // v24~v31
				sum9 = _mm256_add_pd(sum9, sum10); // v32~
				sum11 = _mm256_add_pd(sum11, sum12); // v40~
				sum13 = _mm256_add_pd(sum13, sum14); // v48~
				sum15 = _mm256_add_pd(sum15, sum16); // v52~
				//_mm256_TRANSPOSE4_pd(sum1, sum3, sum5, sum7)	
				sum2 = _mm256_shuffle_pd(sum1, sum3, 0x00); // v0,v4,v2,v6
				sum4 = _mm256_shuffle_pd(sum1, sum3, 0x0F); // v1,v5,v3,v7
				sum6 = _mm256_shuffle_pd(sum5, sum7, 0x00); // v8,v12,v10,v14
				sum8 = _mm256_shuffle_pd(sum5, sum7, 0x0F); // v9,v13,v11,v15
				sum1 = _mm256_permute2f128_pd(sum2, sum6, (0|(2<<4))); // v0,v4,v8,v12
				sum3 = _mm256_permute2f128_pd(sum4, sum8, (0|(2<<4))); // v1,v5,v9,v13
				sum5 = _mm256_permute2f128_pd(sum2, sum6, (1|(3<<4))); // v2,v6,10,v14
				sum7 = _mm256_permute2f128_pd(sum4, sum8, (1|(3<<4))); // v3,v7,v11,v15				
				//_mm256_TRANSPOSE4_pd(sum9, sum11, sum13, sum15)
				sum10 = _mm256_shuffle_pd(sum9, sum11, 0x00); // v0,v4,v2,v6
				sum12 = _mm256_shuffle_pd(sum9, sum11, 0x0F); // v1,v5,v3,v7
				sum14 = _mm256_shuffle_pd(sum13, sum15, 0x00); // v8,v12,v10,v14
				sum16 = _mm256_shuffle_pd(sum13, sum15, 0x0F); // v9,v13,v11,v15
				sum9 = _mm256_permute2f128_pd(sum10, sum14, (0|(2<<4))); // v0,v4,v8,v12
				sum11 = _mm256_permute2f128_pd(sum12, sum16, (0|(2<<4))); // v1,v5,v9,v13
				sum13 = _mm256_permute2f128_pd(sum10, sum14, (1|(3<<4))); // v2,v6,10,v14
				sum15 = _mm256_permute2f128_pd(sum12, sum16, (1|(3<<4))); // v3,v7,v11,v15
				sum1 = _mm256_add_pd(sum1, sum3);
				sum5 = _mm256_add_pd(sum5, sum7);
				sum9 = _mm256_add_pd(sum9, sum11);
				sum13 = _mm256_add_pd(sum13, sum15);
				sum1 = _mm256_add_pd(sum1, sum5);
				sum9 = _mm256_add_pd(sum9, sum13);
				sum1 = _mm256_mul_pd(sum1, divn);
				sum9 = _mm256_mul_pd(sum9, divn);
				_mm256_store_pd(&ptr[i    ], sum1);
				_mm256_store_pd(&ptr[i + 4], sum9);
			}
		}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
		{ // 4samples
			const __m128d divn = _mm_set1_pd(DIV_8);
			for(i = 0; i < count; i += 4){
				int32 ofs = i * 8;
				__m128d	sum1 = _mm_load_pd(&ptr[ofs + 0]); // v0,v1
				__m128d	sum2 = _mm_load_pd(&ptr[ofs + 2]); // v2,v3
				__m128d	sum3 = _mm_load_pd(&ptr[ofs + 4]); // v4,v5
				__m128d	sum4 = _mm_load_pd(&ptr[ofs + 6]); // v6,v7
				__m128d	sum5 = _mm_load_pd(&ptr[ofs + 8]); // v8,v9
				__m128d	sum6 = _mm_load_pd(&ptr[ofs + 10]); // v10,v11
				__m128d	sum7 = _mm_load_pd(&ptr[ofs + 12]); // v12,v13
				__m128d	sum8 = _mm_load_pd(&ptr[ofs + 14]); // v14,v15
				__m128d	sum9 = _mm_load_pd(&ptr[ofs + 16]);
				__m128d	sum10 = _mm_load_pd(&ptr[ofs + 18]);
				__m128d	sum11 = _mm_load_pd(&ptr[ofs + 20]);
				__m128d	sum12 = _mm_load_pd(&ptr[ofs + 22]);
				__m128d	sum13 = _mm_load_pd(&ptr[ofs + 24]);
				__m128d	sum14 = _mm_load_pd(&ptr[ofs + 26]);
				__m128d	sum15 = _mm_load_pd(&ptr[ofs + 28]);
				__m128d	sum16 = _mm_load_pd(&ptr[ofs + 30]);
				// ([1,2,3,4] [5,6,7,8]) ([9,10,11,12] [13,14,15,16])				
				sum1 = _mm_add_pd(sum1, sum2); // (v0+v2) (v1+v3)
				sum3 = _mm_add_pd(sum3, sum4); // (v4+v6) (v5+v7)
				sum5 = _mm_add_pd(sum5, sum6); // (v8+v10) (v9+v11)
				sum7 = _mm_add_pd(sum7, sum8); // (v12+v14) (v13+v15)
				sum9 = _mm_add_pd(sum9, sum10);
				sum11 = _mm_add_pd(sum11, sum12);
				sum13 = _mm_add_pd(sum13, sum14);
				sum15 = _mm_add_pd(sum15, sum16);
				sum1 = _mm_add_pd(sum1, sum3); // ((v0+v2)+(v4+v6)) ((v1+v3)+(v5+v7))
				sum5 = _mm_add_pd(sum5, sum7); // ((v8+v10)+(v12+v14)) ((v9+v11)+(v13+v15))
				sum9 = _mm_add_pd(sum9, sum11);
				sum13 = _mm_add_pd(sum13, sum15);
				sum2 = _mm_shuffle_pd(sum1, sum5, 0x0); // ((v0+v2)+(v4+v6)) ((v8+v10)+(v12+v14))
				sum6 = _mm_shuffle_pd(sum1, sum5, 0x3); // ((v1+v3)+(v5+v7)) ((v9+v11)+(v13+v15))
				sum10 = _mm_shuffle_pd(sum9, sum13, 0x0);
				sum14 = _mm_shuffle_pd(sum9, sum13, 0x3);
				sum2 = _mm_add_pd(sum2, sum6); // (((v0+v2)+(v4+v6))+((v1+v3)+(v5+v7))) (((v8+v10)+(v12+v14))+((v9+v11)+(v13+v15)))
				sum10 = _mm_add_pd(sum10, sum14); // sum(16~23) sum(24~31)
				sum2 = _mm_mul_pd(sum2, divn);
				sum10 = _mm_mul_pd(sum10, divn);
				_mm_store_pd(&ptr[i    ], sum2);
				_mm_store_pd(&ptr[i + 2], sum10);
			}
		}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
		{ // 8samples
			const float divnv = DIV_8;
			const __m128 divn = _mm_load1_ps(&divnv);
			for(i = 0; i < count; i += 8){
				int32 ofs = i * 8;
				__m128	sum1 = _mm_load_ps(&ptr[ofs + 0]); // v0,v1,v2,v3
				__m128	sum2 = _mm_load_ps(&ptr[ofs + 4]); // v4,v5,v6,v7
				__m128	sum3 = _mm_load_ps(&ptr[ofs + 8]); // v8,v9,v10,v11
				__m128	sum4 = _mm_load_ps(&ptr[ofs + 12]); // v12,v13,v14,v15
				__m128	sum5 = _mm_load_ps(&ptr[ofs + 16]); // v16,....
				__m128	sum6 = _mm_load_ps(&ptr[ofs + 20]); // v20,....
				__m128	sum7 = _mm_load_ps(&ptr[ofs + 24]); // v24,....
				__m128	sum8 = _mm_load_ps(&ptr[ofs + 28]); // v28,....				
				__m128	sum9 = _mm_load_ps(&ptr[ofs + 32]); // v32,....
				__m128	sum10 = _mm_load_ps(&ptr[ofs + 36]); // v36,....
				__m128	sum11 = _mm_load_ps(&ptr[ofs + 40]); // v40,....
				__m128	sum12 = _mm_load_ps(&ptr[ofs + 44]); // v44,....
				__m128	sum13 = _mm_load_ps(&ptr[ofs + 48]); // v48,....
				__m128	sum14 = _mm_load_ps(&ptr[ofs + 52]); // v52,....
				__m128	sum15 = _mm_load_ps(&ptr[ofs + 56]); // v56,....
				__m128	sum16 = _mm_load_ps(&ptr[ofs + 60]); // v60,....
				sum1 = _mm_add_pd(sum1, sum2); // v0~v7
				sum3 = _mm_add_pd(sum3, sum4); // v8~v15
				sum5 = _mm_add_pd(sum5, sum6); // v16~v23
				sum7 = _mm_add_pd(sum7, sum8); // v24~v31
				sum9 = _mm_add_pd(sum9, sum10); // v32~
				sum11 = _mm_add_pd(sum11, sum12); // v40~
				sum13 = _mm_add_pd(sum13, sum14); // v48~
				sum15 = _mm_add_pd(sum15, sum16); // v52~
				//_MM_TRANSPOSE4_PS(sum1, sum3, sum5, sum7)				
				sum2 = _mm_shuffle_ps(sum1, sum3, 0x44); // v0,v1,v4,v5
				sum4 = _mm_shuffle_ps(sum1, sum3, 0xEE); // v2,v3,v6,v7
				sum6 = _mm_shuffle_ps(sum5, sum7, 0x44); // v8,v9,v12,v13
				sum8 = _mm_shuffle_ps(sum5, sum7, 0xEE); // v10,v11,v14,v5
				sum1 = _mm_shuffle_ps(sum2, sum4, 0x88); // v0,v4,v8,v12
				sum3 = _mm_shuffle_ps(sum2, sum4, 0xDD); // v1,v5,v9,v13
				sum5 = _mm_shuffle_ps(sum6, sum8, 0x88); // v2,v6,10,v15
				sum7 = _mm_shuffle_ps(sum6, sum8, 0xDD); // v3,v7,v11,v16				
				//_MM_TRANSPOSE4_PS(sum9, sum11, sum13, sum15)				
				sum10 = _mm_shuffle_ps(sum9, sum11, 0x44); // v0,v1,v4,v5
				sum12 = _mm_shuffle_ps(sum9, sum11, 0xEE); // v2,v3,v6,v7
				sum14 = _mm_shuffle_ps(sum13, sum15, 0x44); // v8,v9,v12,v13
				sum16 = _mm_shuffle_ps(sum13, sum15, 0xEE); // v10,v11,v14,v5
				sum9 = _mm_shuffle_ps(sum10, sum12, 0x88); // v0,v4,v8,v12
				sum11 = _mm_shuffle_ps(sum10, sum12, 0xDD); // v1,v5,v9,v13
				sum13 = _mm_shuffle_ps(sum14, sum16, 0x88); // v2,v6,10,v15
				sum15 = _mm_shuffle_ps(sum14, sum16, 0xDD); // v3,v7,v11,v16
				sum1 = _mm_add_ps(sum1, sum3);
				sum5 = _mm_add_ps(sum5, sum7);
				sum9 = _mm_add_ps(sum9, sum11);
				sum13 = _mm_add_ps(sum13, sum15);
				sum1 = _mm_add_ps(sum1, sum5);
				sum9 = _mm_add_ps(sum9, sum13);
				sum1 = _mm_mul_ps(sum1, divn);
				sum9 = _mm_mul_ps(sum9, divn);
				_mm_store_ps(&ptr[i    ], sum1);
				_mm_store_ps(&ptr[i + 4], sum9);
			}
		}
#else
		for(i = 0; i < count; i++){
			int32 ofs = i * 8;
			FLOAT_T tmp = ptr[ofs + 0] + ptr[ofs + 1] + ptr[ofs + 2] + ptr[ofs + 3]
				+ ptr[ofs + 4] + ptr[ofs + 5] + ptr[ofs + 6] + ptr[ofs + 7];
			ptr[i] = tmp * DIV_8;
		}
#endif
		break;
	case 16:
#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
		{ // 4samples
			const double divnv = DIV_16;
			const __m256d divn = _mm256_broadcast_sd(&divnv);
			for(i = 0; i < count; i += 4){
				int32 ofs = i * 16;
				__m256d	sum1 = _mm256_load_pd(&ptr[ofs + 0]); // v0,v1,v2,v3
				__m256d	sum2 = _mm256_load_pd(&ptr[ofs + 4]); // v4,v5,v6,v7
				__m256d	sum3 = _mm256_load_pd(&ptr[ofs + 8]); // v8,v9,v10,v11
				__m256d	sum4 = _mm256_load_pd(&ptr[ofs + 12]); // v12,v13,v14,v15
				__m256d	sum5 = _mm256_load_pd(&ptr[ofs + 16]); // v16,....
				__m256d	sum6 = _mm256_load_pd(&ptr[ofs + 20]); // v20,....
				__m256d	sum7 = _mm256_load_pd(&ptr[ofs + 24]); // v24,....
				__m256d	sum8 = _mm256_load_pd(&ptr[ofs + 28]); // v28,....	
				__m256d	sum9 = _mm256_load_pd(&ptr[ofs + 32]); // v32,....
				__m256d	sum10 = _mm256_load_pd(&ptr[ofs + 36]); // v36,....
				__m256d	sum11 = _mm256_load_pd(&ptr[ofs + 40]); // v40,....
				__m256d	sum12 = _mm256_load_pd(&ptr[ofs + 44]); // v44,....
				__m256d	sum13 = _mm256_load_pd(&ptr[ofs + 48]); // v48,....
				__m256d	sum14 = _mm256_load_pd(&ptr[ofs + 52]); // v52,....
				__m256d	sum15 = _mm256_load_pd(&ptr[ofs + 56]); // v56,....
				__m256d	sum16 = _mm256_load_pd(&ptr[ofs + 60]); // v60,....
				sum1 = _mm256_add_pd(sum1, sum2); // v0~v7
				sum3 = _mm256_add_pd(sum3, sum4); // v8~v15
				sum5 = _mm256_add_pd(sum5, sum6); // v16~v23
				sum7 = _mm256_add_pd(sum7, sum8); // v24~v31
				sum9 = _mm256_add_pd(sum9, sum10); // v32~
				sum11 = _mm256_add_pd(sum11, sum12); // v40~
				sum13 = _mm256_add_pd(sum13, sum14); // v48~
				sum15 = _mm256_add_pd(sum15, sum16); // v52~
				sum1 = _mm256_add_pd(sum1, sum3); // v0~v15
				sum5 = _mm256_add_pd(sum5, sum7); // v16~v31
				sum9 = _mm256_add_pd(sum9, sum11); // v32~v47
				sum13 = _mm256_add_pd(sum13, sum15); // v48~v63
				//_mm256_TRANSPOSE4_pd(sum1, sum5, sum9, sum13)
				sum2 = _mm256_shuffle_pd(sum1, sum5, 0x00); // v0,v4,v2,v6
				sum6 = _mm256_shuffle_pd(sum1, sum5, 0x0F); // v1,v5,v3,v7
				sum10 = _mm256_shuffle_pd(sum9, sum13, 0x00); // v8,v12,v10,v14
				sum14 = _mm256_shuffle_pd(sum9, sum13, 0x0F); // v9,v13,v11,v15
				sum1 = _mm256_permute2f128_pd(sum2, sum10, (0|(2<<4))); // v0,v4,v8,v12
				sum5 = _mm256_permute2f128_pd(sum6, sum14, (0|(2<<4))); // v1,v5,v9,v13
				sum9 = _mm256_permute2f128_pd(sum2, sum10, (1|(3<<4))); // v2,v6,10,v14
				sum13 = _mm256_permute2f128_pd(sum6, sum14, (1|(3<<4))); // v3,v7,v11,v15
				sum1 = _mm256_add_pd(sum1, sum3);
				sum5 = _mm256_add_pd(sum5, sum7);
				sum9 = _mm256_add_pd(sum9, sum11);
				sum13 = _mm256_add_pd(sum13, sum15);
				sum1 = _mm256_add_pd(sum1, sum5);
				sum9 = _mm256_add_pd(sum9, sum13);
				sum1 = _mm256_add_pd(sum1, sum9);
				sum1 = _mm256_mul_pd(sum1, divn);
				_mm256_store_pd(&ptr[i    ], sum1);
			}
		}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
		{ // 2sample
			const __m128d divn = _mm_set1_pd(DIV_16);
			for(i = 0; i < count; i += 2){
				int32 ofs = i * 16;
				__m128d	sum1 = _mm_load_pd(&ptr[ofs + 0]);
				__m128d	sum2 = _mm_load_pd(&ptr[ofs + 2]);
				__m128d	sum3 = _mm_load_pd(&ptr[ofs + 4]);
				__m128d	sum4 = _mm_load_pd(&ptr[ofs + 6]);
				__m128d	sum5 = _mm_load_pd(&ptr[ofs + 8]);
				__m128d	sum6 = _mm_load_pd(&ptr[ofs + 10]);
				__m128d	sum7 = _mm_load_pd(&ptr[ofs + 12]);
				__m128d	sum8 = _mm_load_pd(&ptr[ofs + 14]);
				__m128d	sum9 = _mm_load_pd(&ptr[ofs + 16]);
				__m128d	sum10 = _mm_load_pd(&ptr[ofs + 18]);
				__m128d	sum11 = _mm_load_pd(&ptr[ofs + 20]);
				__m128d	sum12 = _mm_load_pd(&ptr[ofs + 22]);
				__m128d	sum13 = _mm_load_pd(&ptr[ofs + 24]);
				__m128d	sum14 = _mm_load_pd(&ptr[ofs + 26]);
				__m128d	sum15 = _mm_load_pd(&ptr[ofs + 28]);
				__m128d	sum16 = _mm_load_pd(&ptr[ofs + 30]);
				// ([1,2,3,4,5,6,7,8] [9,10,11,12,13,14,15,16])
				sum1 = _mm_add_pd(sum1, sum2);
				sum3 = _mm_add_pd(sum3, sum4);
				sum5 = _mm_add_pd(sum5, sum6);
				sum7 = _mm_add_pd(sum7, sum8);
				sum9 = _mm_add_pd(sum9, sum10);
				sum11 = _mm_add_pd(sum11, sum12);
				sum13 = _mm_add_pd(sum13, sum14);
				sum15 = _mm_add_pd(sum15, sum16);
				sum1 = _mm_add_pd(sum1, sum3);
				sum5 = _mm_add_pd(sum5, sum7);				
				sum9 = _mm_add_pd(sum9, sum11);
				sum13 = _mm_add_pd(sum13, sum15);				
				sum1 = _mm_add_pd(sum1, sum5);
				sum9 = _mm_add_pd(sum9, sum13);					
				sum2 = _mm_shuffle_pd(sum1, sum9, 0x0); // x10x90 
				sum3 = _mm_shuffle_pd(sum1, sum9, 0x3); // x11x91
				sum2 = _mm_add_pd(sum2, sum3); // v0=x10+x11 v1=x90+91
				sum2 = _mm_mul_pd(sum2, divn);
				_mm_store_pd(&ptr[i], sum2);
			}
		}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
		{ // 4samples
			const float divnv = DIV_16;
			const __m128 divn = _mm_load1_ps(&divnv);
			for(i = 0; i < count; i += 4){
				int32 ofs = i * 16;
				__m128	sum1 = _mm_load_ps(&ptr[ofs + 0]); // v0,v1,v2,v3
				__m128	sum2 = _mm_load_ps(&ptr[ofs + 4]); // v4,v5,v6,v7
				__m128	sum3 = _mm_load_ps(&ptr[ofs + 8]); // v8,v9,v10,v11
				__m128	sum4 = _mm_load_ps(&ptr[ofs + 12]); // v12,v13,v14,v15
				__m128	sum5 = _mm_load_ps(&ptr[ofs + 16]); // v16,....
				__m128	sum6 = _mm_load_ps(&ptr[ofs + 20]); // v20,....
				__m128	sum7 = _mm_load_ps(&ptr[ofs + 24]); // v24,....
				__m128	sum8 = _mm_load_ps(&ptr[ofs + 28]); // v28,....	
				__m128	sum9 = _mm_load_ps(&ptr[ofs + 32]); // v32,....
				__m128	sum10 = _mm_load_ps(&ptr[ofs + 36]); // v36,....
				__m128	sum11 = _mm_load_ps(&ptr[ofs + 40]); // v40,....
				__m128	sum12 = _mm_load_ps(&ptr[ofs + 44]); // v44,....
				__m128	sum13 = _mm_load_ps(&ptr[ofs + 48]); // v48,....
				__m128	sum14 = _mm_load_ps(&ptr[ofs + 52]); // v52,....
				__m128	sum15 = _mm_load_ps(&ptr[ofs + 56]); // v56,....
				__m128	sum16 = _mm_load_ps(&ptr[ofs + 60]); // v60,....
				sum1 = _mm_add_pd(sum1, sum2); // v0~v7
				sum3 = _mm_add_pd(sum3, sum4); // v8~v15
				sum5 = _mm_add_pd(sum5, sum6); // v16~v23
				sum7 = _mm_add_pd(sum7, sum8); // v24~v31
				sum9 = _mm_add_pd(sum9, sum10); // v32~
				sum11 = _mm_add_pd(sum11, sum12); // v40~
				sum13 = _mm_add_pd(sum13, sum14); // v48~
				sum15 = _mm_add_pd(sum15, sum16); // v52~				
				sum1 = _mm_add_pd(sum1, sum3); // v0~v15
				sum5 = _mm_add_pd(sum5, sum7); // v16~v31
				sum9 = _mm_add_pd(sum9, sum11); // v32~v47
				sum13 = _mm_add_pd(sum13, sum15); // v48~v63
				//_MM_TRANSPOSE4_PS(sum1, sum5, sum9, sum13)				
				sum2 = _mm_shuffle_ps(sum1, sum5, 0x44); // v0,v1,v4,v5
				sum6 = _mm_shuffle_ps(sum1, sum5, 0xEE); // v2,v3,v6,v7
				sum10 = _mm_shuffle_ps(sum9, sum13, 0x44); // v8,v9,v12,v13
				sum14 = _mm_shuffle_ps(sum9, sum13, 0xEE); // v10,v11,v14,v5
				sum1 = _mm_shuffle_ps(sum2, sum6, 0x88); // v0,v4,v8,v12
				sum5 = _mm_shuffle_ps(sum2, sum6, 0xDD); // v1,v5,v9,v13
				sum9 = _mm_shuffle_ps(sum10, sum14, 0x88); // v2,v6,10,v15
				sum13 = _mm_shuffle_ps(sum10, sum14, 0xDD); // v3,v7,v11,v16
				sum1 = _mm_add_ps(sum1, sum3);
				sum5 = _mm_add_ps(sum5, sum7);
				sum9 = _mm_add_ps(sum9, sum11);
				sum13 = _mm_add_ps(sum13, sum15);
				sum1 = _mm_add_ps(sum1, sum5);
				sum9 = _mm_add_ps(sum9, sum13);
				sum1 = _mm_add_ps(sum1, sum9);
				sum1 = _mm_mul_ps(sum1, divn);
				_mm_store_ps(&ptr[i    ], sum1);
			}
		}
#else
		for(i = 0; i < count; i++){
			int32 ofs = i * 16;
			FLOAT_T tmp = ptr[ofs + 0] + ptr[ofs + 1] + ptr[ofs + 2] + ptr[ofs + 3]
				+ ptr[ofs + 4] + ptr[ofs + 5] + ptr[ofs + 6] + ptr[ofs + 7]
				+ ptr[ofs + 8] + ptr[ofs + 9] + ptr[ofs + 10] + ptr[ofs + 11]
				+ ptr[ofs + 12] + ptr[ofs + 13] + ptr[ofs + 14] + ptr[ofs + 15];
			ptr[i] = tmp * DIV_16;
		}
#endif
		break;
#else
	default:
		for(i = 0; i < count; i++){
			int32 ofs = i * opt_resample_over_sampling;
			FLOAT_T tmp = 0.0;
			int32 j;
			for(j = 0; j < opt_resample_over_sampling; j++){
				tmp += ptr[ofs + j];
			}
			ptr[i] = tmp * div_over_sampling_ratio;
		}
		break;
#endif

	}

}



/*************** resample.c initialize uninitialize *****************/
///r
/* initialize the coefficients of the current resampling algorithm */
void initialize_resampler_coeffs(void)
{
	set_current_resampler(opt_resample_type);
	set_resampler_parm(opt_resample_param);
    /* initialize_newton_coeffs(); */
    initialize_gauss_table(gauss_n);
    /* we don't have to initialize newton table any more */	
	initialize_lanczos_table(lanczos_n);	
	/* over_sampling */
	set_resampler_over_sampling(opt_resample_over_sampling);
}

///r
// kobarin
void uninitialize_resampler_coeffs(void)
{
	free_gauss_table();
	free_lanczos_table();
}



/*************** optimize linear resample *****************/
#if defined(PRECALC_LOOPS)
#define LO_OPTIMIZE_INCREMENT

static inline DATA_T resample_linear_single(Voice *vp)
{	
	sample_t *src = vp->sample->data;
    const fract_t ofsf = vp->resrc.offset & FRACTION_MASK;
	const spos_t ofsi = vp->resrc.offset >> FRACTION_BITS;
	const int32 v1 = src[ofsi];
	const int32 v2 = src[ofsi + 1];	
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
    return ((FLOAT_T)v1 + (FLOAT_T)(v2 - v1) * (FLOAT_T)ofsf * div_fraction) * OUT_INT16;
#else // DATA_T_IN32
    return (v1 + imuldiv_fraction((v2 - v1), ofsf));
#endif
}

#if (USE_X86_EXT_INTRIN >= 10)
// offset:int32*16, resamp:float*16
static inline DATA_T *resample_linear_multi(Voice *vp, DATA_T *dest, int32 req_count, int32 *out_count)
{
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0;
	const int32 count = req_count & ~15;
	splen_t prec_offset = resrc->offset & INTEGER_MASK;
	sample_t *src = vp->sample->data + (prec_offset >> FRACTION_BITS);
	int32 start_offset = (int32)(resrc->offset - prec_offset); // (offset計算をint32値域にする(SIMD用
	int32 inc = resrc->increment;

	__m512i vinit = _mm512_mullo_epi32(_mm512_set_epi32(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0), _mm512_set1_epi32(inc));
	__m512i vofs = _mm512_add_epi32(_mm512_set1_epi32(start_offset), vinit);
	__m512i vinc = _mm512_set1_epi32(inc * 16), vfmask = _mm512_set1_epi32((int32)FRACTION_MASK);
	__m512 vec_divo = _mm512_set1_ps(DIV_15BIT), vec_divf = _mm512_set1_ps(div_fraction);

#ifdef LO_OPTIMIZE_INCREMENT
#ifdef USE_PERMUTEX2
	const int32 opt_inc1 = (1 << FRACTION_BITS) * (32 - 1 - 1) / 16; // (float*16) * 1セット
#else
	const int32 opt_inc1 = (1 << FRACTION_BITS) * (16 - 1 - 1) / 16; // (float*16) * 1セット
#endif
	const __m512i vvar1 = _mm512_set1_epi32(1);
	if (inc < opt_inc1) {
		for (i = 0; i < count; i+= 16) {
			__m512i vofsi1 = _mm512_srli_epi32(vofs, FRACTION_BITS);
			__m512i vofsi2 = _mm512_add_epi32(vofsi1, vvar1);
			int32 ofs0 = _mm_cvtsi128_si32(_mm512_castsi512_si128(vofsi1));
			__m256i vin1 = _mm256_loadu_si256((__m256i *)&src[ofs0]); // int16*16
#ifdef USE_PERMUTEX2
			__m256i vin2 = _mm256_loadu_si256((__m256i *)&src[ofs0 + 16]); // int16*16
#endif
			__m512i vofsib = _mm512_broadcastd_epi32(_mm512_castsi512_si128(vofsi1));
			__m512i vofsub1 = _mm512_sub_epi32(vofsi1, vofsib);
			__m512i vofsub2 = _mm512_sub_epi32(vofsi2, vofsib);
			__m512 vvf1 = _mm512_cvtepi32_ps(_mm512_cvtepi16_epi32(vin1));
#ifdef USE_PERMUTEX2
			__m512 vvf2 = _mm512_cvtepi32_ps(_mm512_cvtepi16_epi32(vin2));
			__m512 vv1 = _mm512_permutex2var_ps(vvf1, vofsub1, vvf2); // v1 ofsi
			__m512 vv2 = _mm512_permutex2var_ps(vvf1, vofsub2, vvf2); // v2 ofsi+1
#else
			__m512 vv1 = _mm512_permutexvar_ps(vofsub1, vvf1); // v1 ofsi
			__m512 vv2 = _mm512_permutexvar_ps(vofsub2, vvf1); // v2 ofsi+1
#endif
			// あとは通常と同じ
			__m512 vfp = _mm512_mul_ps(_mm512_cvtepi32_ps(_mm512_and_epi32(vofs, vfmask)), vec_divf);
#if defined(DATA_T_DOUBLE)
			__m512 vec_out = _mm512_mul_ps(_mm512_fmadd_ps(_mm512_sub_ps(vv2, vv1), vfp, vv1), vec_divo);
			_mm512_storeu_pd(dest, _mm512_cvtps_pd(_mm512_castps512_ps256(vec_out)));
			dest += 8;
			_mm512_storeu_pd(dest, _mm512_cvtps_pd(_mm512_extractf32x8_ps(vec_out, 1)));
			dest += 8;
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT 
			__m512 vec_out = _mm512_mul_ps(_mm512_fmadd_ps(_mm512_sub_ps(vv2, vv1), vfp, vv1), vec_divo);
			_mm512_storeu_ps(dest, vec_out);
			dest += 16;
#else // DATA_T_IN32
			__m512 vec_out = _mm512_fmadd_ps(_mm512_sub_ps(vv2, vv1), vfp, vv1);
			_mm512_storeu_epi32((__m512i *)dest, _mm512_cvtps_epi32(vec_out));
			dest += 16;
#endif
			vofs = _mm512_add_epi32(vofs, vinc);
		}
	}
#endif // LO_OPTIMIZE_INCREMENT
	for (; i < count; i += 16) {
		__m512i vofsi = _mm512_srli_epi32(vofs, FRACTION_BITS);
#if 1
		__m512i vsrc01 = _mm512_i32gather_epi32(vofsi, (const int*)src, 2);
		__m512i vsrc0 = _mm512_srai_epi32(_mm512_slli_epi32(vsrc01, 16), 16);
		__m512i vsrc1 = _mm512_srai_epi32(vsrc01, 16);
		__m512 vv1 = _mm512_cvtepi32_ps(vsrc0);
		__m512 vv2 = _mm512_cvtepi32_ps(vsrc1);
#else
		__m128i vin1 = _mm_loadu_si128((__m128i*) & src[MM256_EXTRACT_I32(vofsi, 0)]); // ofsiとofsi+1をロード
		__m128i vin2 = _mm_loadu_si128((__m128i*) & src[MM256_EXTRACT_I32(vofsi, 1)]); // 次周サンプルも同じ
		__m128i vin3 = _mm_loadu_si128((__m128i*) & src[MM256_EXTRACT_I32(vofsi, 2)]); // 次周サンプルも同じ
		__m128i vin4 = _mm_loadu_si128((__m128i*) & src[MM256_EXTRACT_I32(vofsi, 3)]); // 次周サンプルも同じ
		__m128i vin5 = _mm_loadu_si128((__m128i*) & src[MM256_EXTRACT_I32(vofsi, 4)]); // 次周サンプルも同じ
		__m128i vin6 = _mm_loadu_si128((__m128i*) & src[MM256_EXTRACT_I32(vofsi, 5)]); // 次周サンプルも同じ
		__m128i vin7 = _mm_loadu_si128((__m128i*) & src[MM256_EXTRACT_I32(vofsi, 6)]); // 次周サンプルも同じ
		__m128i vin8 = _mm_loadu_si128((__m128i*) & src[MM256_EXTRACT_I32(vofsi, 7)]); // 次周サンプルも同じ
		__m128i vin12 = _mm_unpacklo_epi16(vin1, vin2); // [v11v21]e96,[v12v22]e96 to [v11v12v21v22]e64
		__m128i vin34 = _mm_unpacklo_epi16(vin3, vin4); // [v13v23]e96,[v14v24]e96 to [v13v14v23v24]e64
		__m128i vin56 = _mm_unpacklo_epi16(vin5, vin6); // 同じ
		__m128i vin78 = _mm_unpacklo_epi16(vin7, vin8); // 同じ
		__m128i vin1234 = _mm_unpacklo_epi32(vin12, vin34); // [v11v12,v21v22]e64,[v13v14,v23v24]e64 to [v11v12v13v14,v21v22v23v24]e0
		__m128i vin5678 = _mm_unpacklo_epi32(vin56, vin78); // [v15v16,v25v26]e64,[v17v18,v27v28]e64 to [v15v16v17v18,v25v26v27v28]e0
		__m256i viall = MM256_SET2X_SI256(vin1234, vin5678); // 256bit =128bit+128bit	
		__m256i vsi16_1 = _mm256_permute4x64_epi64(viall, 0xD8); // v1をL128bitにまとめ
		__m256i vsi16_2 = _mm256_permute4x64_epi64(viall, 0x8D); // v2をL128bitにまとめ
		__m256 vv1 = _mm256_cvtepi32_ps(_mm256_cvtepi16_epi32(_mm256_extracti128_si256(vsi16_1, 0))); // int16 to float (float変換でH128bitは消える
		__m256 vv2 = _mm256_cvtepi32_ps(_mm256_cvtepi16_epi32(_mm256_extracti128_si256(vsi16_2, 0))); // int16 to float (float変換でH128bitは消える
#endif
		__m512 vfp = _mm512_mul_ps(_mm512_cvtepi32_ps(_mm512_and_epi32(vofs, vfmask)), vec_divf);
#if defined(DATA_T_DOUBLE)
		__m512 vec_out = _mm512_mul_ps(_mm512_fmadd_ps(_mm512_sub_ps(vv2, vv1), vfp, vv1), vec_divo);
		_mm512_storeu_pd(dest, _mm512_cvtps_pd(_mm512_castps512_ps256(vec_out)));
		dest += 8;
		_mm512_storeu_pd(dest, _mm512_cvtps_pd(_mm512_extractf32x8_ps(vec_out, 1)));
		dest += 8;
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT
		__m512 vec_out = _mm512_mul_ps(_mm512_fmadd_ps(_mm512_sub_ps(vv2, vv1), vfp, vv1), vec_divo);
		_mm512_storeu_ps(dest, vec_out);
		dest += 16;
#else // DATA_T_IN32
		__m512 vec_out = _mm512_fmadd_ps(_mm512_sub_ps(vv2, vv1), vfp, vv1);
		_mm512_storeu_epi32((__m512i *)dest, _mm512_cvtps_epi32(vec_out));
		dest += 16;
#endif
		vofs = _mm512_add_epi32(vofs, vinc);
	}
	resrc->offset = prec_offset + (splen_t)(_mm_cvtsi128_si32(_mm512_castsi512_si128(vofs)));
	*out_count = i;
	return dest;
}
#elif (USE_X86_EXT_INTRIN >= 9)
// offset:int32*8, resamp:float*8
// ループ内部のoffset計算をint32値域にする , (sample_increment * (req_count+1)) < int32 max
static inline DATA_T *resample_linear_multi(Voice *vp, DATA_T *dest, int32 req_count, int32 *out_count)
{
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0;
	const int32 req_count_mask = ~(0x7);
	const int32 count = req_count & req_count_mask;
	splen_t prec_offset = resrc->offset & INTEGER_MASK;
	sample_t *src = vp->sample->data + (prec_offset >> FRACTION_BITS);
	int32 start_offset = (int32)(resrc->offset - prec_offset); // (offset計算をint32値域にする(SIMD用
	int32 inc = resrc->increment;
	__m256i vinit = _mm256_set_epi32(inc * 7, inc * 6, inc * 5, inc * 4, inc * 3, inc * 2, inc, 0);
	__m256i vofs = _mm256_add_epi32(_mm256_set1_epi32(start_offset), vinit);
	__m256i vinc = _mm256_set1_epi32(inc * 8), vfmask = _mm256_set1_epi32((int32)FRACTION_MASK);
	__m256 vec_divo = _mm256_set1_ps(DIV_15BIT), vec_divf = _mm256_set1_ps(div_fraction);

#ifdef LO_OPTIMIZE_INCREMENT
	// 最適化レート = (ロードデータ数 - 初期オフセット小数部の最大値(1未満) - 補間ポイント数(linearは1) ) / オフセットデータ数
	// ロードデータ数はint16用permutevarがないので変換後の32bit(int32/float)の8セットになる
	// 256bitロードデータ(int16*16セット)を全て使うにはSIMD2セットで対応
	const int32 opt_inc1 = (1 << FRACTION_BITS) * (8 - 1 - 1) / 8; // (float*8) * 1セット
	const int32 opt_inc2 = (1 << FRACTION_BITS) * (16 - 1 - 1) / 8; // (float*8) * 2セット
	const __m256i vvar1 = _mm256_set1_epi32(1);
	if(inc < opt_inc1){	// 1セット	
	for(i = 0; i < count; i += 8) {
	__m256i vofsi1 = _mm256_srli_epi32(vofs, FRACTION_BITS);
	__m256i vofsi2 = _mm256_add_epi32(vofsi1, vvar1);
	int32 ofs0 = _mm_cvtsi128_si32(_mm256_castsi256_si128(vofsi1));
	__m128i vin1 = _mm_loadu_si128((__m128i *)&src[ofs0]); // int16*16
	__m256i vofsib = _mm256_broadcastd_epi32(_mm256_castsi256_si128(vofsi1));
	__m256i vofsub1 = _mm256_sub_epi32(vofsi1, vofsib); 
	__m256i vofsub2 = _mm256_sub_epi32(vofsi2, vofsib); 
	__m256 vvf1 = _mm256_cvtepi32_ps(_mm256_cvtepi16_epi32(vin1)); // int16 to float (float変換でH128bitは消える
	__m256 vv1 = _mm256_permutevar8x32_ps(vvf1, vofsub1); // v1 ofsi
	__m256 vv2 = _mm256_permutevar8x32_ps(vvf1, vofsub2); // v2 ofsi+1
	// あとは通常と同じ
	__m256 vfp = _mm256_mul_ps(_mm256_cvtepi32_ps(_mm256_and_si256(vofs, vfmask)), vec_divf);
#if defined(DATA_T_DOUBLE)
	__m256 vec_out = _mm256_mul_ps(MM256_FMA_PS(_mm256_sub_ps(vv2, vv1), vfp, vv1), vec_divo);
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(_mm256_castps256_ps128(vec_out)));
	dest += 4;
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(_mm256_extractf128_ps(vec_out, 1)));	
	dest += 4;
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT 
	__m256 vec_out = _mm256_mul_ps(MM256_FMA_PS(_mm256_sub_ps(vv2, vv1), vfp, vv1), vec_divo);
	_mm256_storeu_ps(dest, vec_out);
	dest += 8;
#else // DATA_T_IN32
	__m256 vec_out = MM256_FMA_PS(_mm256_sub_ps(vv2, vv1), vfp, vv1);
	_mm256_storeu_si256(__m256i *)dest, _mm256_cvtps_epi32(vec_out));
	dest += 8;
#endif
	vofs = _mm256_add_epi32(vofs, vinc);
	}
	}else
#if 0 // 2set
	if(inc < opt_inc2){ // 2セット
	const __m256i vvar7 = _mm256_set1_epi32(7);
	for(i = 0; i < count; i += 8) {
	__m256i vofsi1 = _mm256_srli_epi32(vofs, FRACTION_BITS); // ofsi
	__m256i vofsi2 = _mm256_add_epi32(vofsi1, vadd1); // ofsi+1
	int32 ofs0 = _mm_extract_epi32(_mm256_extracti128si256(vofsi1, 0x0), 0x0);
	__m256i vin1 = _mm256_loadu_si256((__m256i *)&src[ofs0]); // int16*16
	__m256i vin2 = _mm256_permutevar8x32_epi32(vin1, _mm256_set_epi32(3,2,1,0,7,6,5,4)); // H128bitをL128bitに移動
	__m256 vvf1 = _mm256_cvtepi32_ps(_mm256_cvtepi16_epi32(vin1)); // int16 to float (float変換でH128bitは消える
	__m256 vvf2 = _mm256_cvtepi32_ps(_mm256_cvtepi16_epi32(vin2)); // int16 to float (float変換でH128bitは消える	
	__m256i vofsib = _mm256_permutevar8x32_epi32(vofsi, _mm256_setzero_epi32()); // ofsi[0]
	__m256i vofsub1 = _mm256_sub_epi32(vofsi1, vofsib); // v1 ofsi
	__m256i vofsub2 = _mm256_sub_epi32(vofsi2, vofsib); // v2 ofsi+1
	__m256i vrm1 = _mm256_cmpgt_epi32(vofsub1, vvar7); // オフセット差が8以上の条件でマスク作成
	__m256i vrm2 = _mm256_cmpgt_epi32(vofsub2, vvar7); // オフセット差が8以上の条件でマスク作成
	// src2 offsetが下位3bitのみ有効であれば8を超える部分にマスク不要のはず
	__m256 vv11 = _mm256_permutevar8x32_ps(vvf1, vofsub1);
	__m256 vv12 = _mm256_permutevar8x32_ps(vvf2, vofsub1);
	__m256 vv21 = _mm256_permutevar8x32_ps(vvf1, vofsub2);
	__m256 vv22 = _mm256_permutevar8x32_ps(vvf2, vofsub2);	
	__m256 vv1 = _mm256_blendv_ps(vv11, vv12, vrm1); // v1 ofsi
	__m256 vv2 = _mm256_blendv_ps(vv21, vv22, vrm2); // v2 ofsi+1
	// あとは通常と同じ
	__m256 vfp = _mm256_mul_ps(_mm256_cvtepi32_ps(_mm256_and_si256(vofs, vfmask)), vec_divf);
#if defined(DATA_T_DOUBLE)
	__m256 vec_out = _mm256_mul_ps(MM256_FMA_PS(_mm256_sub_ps(vv2, vv1), vfp, vv1), vec_divo);
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(_mm256_extractf128_ps(vec_out, 0)));
	dest += 4;
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(_mm256_extractf128_ps(vec_out, 1)));	
	dest += 4;
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT
	__m256 vec_out = _mm256_mul_ps(MM256_FMA_PS(_mm256_sub_ps(vv2, vv1), vfp, vv1), vec_divo);
	_mm256_storeu_ps(dest, vec_out);
	dest += 8;
#else // DATA_T_IN32
	__m256 vec_out = MM256_FMA_PS(_mm256_sub_ps(vv2, vv1), vfp, vv1);
	_mm256_storeu_si256((__m256i *)dest, _mm256_cvtps_epi32(vec_out));
	dest += 8;
#endif
	vofs = _mm256_add_epi32(vofs, vinc);
	}
	}else
#endif // 2set
#endif // LO_OPTIMIZE_INCREMENT

	for(; i < count; i += 8) {
	__m256i vofsi = _mm256_srli_epi32(vofs, FRACTION_BITS);
#if 0
	__m256i vsrc01 = _mm256_i32gather_epi32((const int*)src, vofsi, 2);
	__m256i vsrc0 = _mm256_srai_epi32(_mm256_slli_epi32(vsrc01, 16), 16);
	__m256i vsrc1 = _mm256_srai_epi32(vsrc01, 16);
	__m256 vv1 = _mm256_cvtepi32_ps(vsrc0);
	__m256 vv2 = _mm256_cvtepi32_ps(vsrc1);
#else
	__m128i vin1 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,0)]); // ofsiとofsi+1をロード
	__m128i vin2 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,1)]); // 次周サンプルも同じ
	__m128i vin3 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,2)]); // 次周サンプルも同じ
	__m128i vin4 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,3)]); // 次周サンプルも同じ
	__m128i vin5 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,4)]); // 次周サンプルも同じ
	__m128i vin6 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,5)]); // 次周サンプルも同じ
	__m128i vin7 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,6)]); // 次周サンプルも同じ
	__m128i vin8 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,7)]); // 次周サンプルも同じ
	__m128i vin12 =	_mm_unpacklo_epi16(vin1, vin2); // [v11v21]e96,[v12v22]e96 to [v11v12v21v22]e64
	__m128i vin34 =	_mm_unpacklo_epi16(vin3, vin4); // [v13v23]e96,[v14v24]e96 to [v13v14v23v24]e64
	__m128i vin56 =	_mm_unpacklo_epi16(vin5, vin6); // 同じ
	__m128i vin78 =	_mm_unpacklo_epi16(vin7, vin8); // 同じ
	__m128i vin1234 = _mm_unpacklo_epi32(vin12, vin34); // [v11v12,v21v22]e64,[v13v14,v23v24]e64 to [v11v12v13v14,v21v22v23v24]e0
	__m128i vin5678 = _mm_unpacklo_epi32(vin56, vin78); // [v15v16,v25v26]e64,[v17v18,v27v28]e64 to [v15v16v17v18,v25v26v27v28]e0
	__m256i viall = MM256_SET2X_SI256(vin1234, vin5678); // 256bit =128bit+128bit	
	__m256i vsi16_1 = _mm256_permute4x64_epi64(viall, 0xD8); // v1をL128bitにまとめ
	__m256i vsi16_2 = _mm256_permute4x64_epi64(viall, 0x8D); // v2をL128bitにまとめ
	__m256 vv1 = _mm256_cvtepi32_ps(_mm256_cvtepi16_epi32(_mm256_extracti128_si256(vsi16_1, 0))); // int16 to float (float変換でH128bitは消える
	__m256 vv2 = _mm256_cvtepi32_ps(_mm256_cvtepi16_epi32(_mm256_extracti128_si256(vsi16_2, 0))); // int16 to float (float変換でH128bitは消える
#endif
	__m256 vfp = _mm256_mul_ps(_mm256_cvtepi32_ps(_mm256_and_si256(vofs, vfmask)), vec_divf);
#if defined(DATA_T_DOUBLE)
	__m256 vec_out = _mm256_mul_ps(MM256_FMA_PS(_mm256_sub_ps(vv2, vv1), vfp, vv1), vec_divo);
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(_mm256_castps256_ps128(vec_out)));
	dest += 4;
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(_mm256_extractf128_ps(vec_out, 1)));	
	dest += 4;
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT
	__m256 vec_out = _mm256_mul_ps(MM256_FMA_PS(_mm256_sub_ps(vv2, vv1), vfp, vv1), vec_divo);
	_mm256_storeu_ps(dest, vec_out);
	dest += 8;
#else // DATA_T_IN32
	__m256 vec_out = MM256_FMA_PS(_mm256_sub_ps(vv2, vv1), vfp, vv1);
	_mm256_storeu_si256((__m256i *)dest, _mm256_cvtps_epi32(vec_out));
	dest += 8;
#endif
	vofs = _mm256_add_epi32(vofs, vinc);
	}
	resrc->offset = prec_offset + (splen_t)(MM256_EXTRACT_I32(vofs, 0));
	*out_count = i;
    return dest;
}

#elif (USE_X86_EXT_INTRIN >= 3)
// offset:int32*4, resamp:float*4
// ループ内部のoffset計算をint32値域にする , (sample_increment * (req_count+1)) < int32 max
static inline DATA_T *resample_linear_multi(Voice *vp, DATA_T *dest, int32 req_count, int32 *out_count)
{
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0;
	const uint32 req_count_mask = ~(0x3);
	const int32 count = req_count & req_count_mask;
	splen_t prec_offset = resrc->offset & INTEGER_MASK;
	sample_t *src = vp->sample->data + (prec_offset >> FRACTION_BITS);
	const int32 start_offset = (int32)(resrc->offset - prec_offset); // offset計算をint32値域にする(SIMD用
	const int32 inc = resrc->increment;
	__m128i vofs = _mm_add_epi32(_mm_set1_epi32(start_offset), _mm_set_epi32(inc * 3, inc * 2, inc, 0));
	const __m128i vinc = _mm_set1_epi32(inc * 4), vfmask = _mm_set1_epi32((int32)FRACTION_MASK);
	const __m128 vec_divf = _mm_set1_ps(div_fraction);
		
#ifdef LO_OPTIMIZE_INCREMENT
// AVXではopt_incのときは速いが 範囲が狭く低いサンプレートで外れやすい 2セットでは負荷高い
#if (USE_X86_EXT_INTRIN >= 8)
	// 最適化レート = (ロードデータ数 - 初期オフセット小数部の最大値(1未満) - 補間ポイント数(linearは1) ) / オフセットデータ数
	// ロードデータ数は_mm_permutevar_psの変換後の(float)の4セットになる
	// 128bitロードデータ(int16*8セット)を全て使うにはSIMD2セットで対応
	const int32 opt_inc1 = (1 << FRACTION_BITS) * (4 - 1 - 1) / 4; // (float*4) * 1セット
	const int32 opt_inc2 = (1 << FRACTION_BITS) * (8 - 1 - 1) / 4; // (float*4) * 2セット
	const __m128i vvar1 = _mm_set1_epi32(1);
	const __m128 vec_divo = _mm_set1_ps(DIV_15BIT);
	if(inc < opt_inc1){	// 1セット	
	for(i = 0; i < count; i += 4) {
	__m128i vofsi1 = _mm_srli_epi32(vofs, FRACTION_BITS);
	__m128i vofsi2 = _mm_add_epi32(vofsi1, vvar1);
	int32 ofs0 = _mm_cvtsi128_si32(vofsi1);
	__m128i vin1 = _mm_loadu_si128((__m128i *)&src[ofs0]); // int16*8
	__m128 vvf1 = _mm_cvtepi32_ps(_mm_cvtepi16_epi32(vin1)); // int16 to float (float変換でH64bitは消える
	__m128i vofsib = _mm_set1_epi32(ofs0); 
	__m128i vofsub1 = _mm_sub_epi32(vofsi1, vofsib); 
	__m128i vofsub2 = _mm_sub_epi32(vofsi2, vofsib); 
	__m128 vv1 = _mm_permutevar_ps(vvf1, vofsub1); // v1 ofsi
	__m128 vv2 = _mm_permutevar_ps(vvf1, vofsub2); // v2 ofsi+1
	// あとは通常と同じ
	__m128 vfp = _mm_mul_ps(_mm_cvtepi32_ps(_mm_and_si128(vofs, vfmask)), vec_divf);
#if defined(DATA_T_DOUBLE)
	__m128 vec_out = _mm_mul_ps(MM_FMA_PS(_mm_sub_ps(vv2, vv1), vfp, vv1), vec_divo);
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(vec_out));
	dest += 4;
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT 
	__m128 vec_out = _mm_mul_ps(MM_FMA_PS(_mm_sub_ps(vv2, vv1), vfp, vv1), vec_divo);
	_mm_storeu_ps(dest, vec_out);
	dest += 4;
#else // DATA_T_IN32
	__m128 vec_out = MM_FMA_PS(_mm_sub_ps(vv2, vv1), vfp, vv1);
	_mm_storeu_si128(__m128i *)dest, _mm_cvtps_epi32(vec_out));
	dest += 4;
#endif
	vofs = _mm_add_epi32(vofs, vinc);
	}
	}else
#if 0 // 2set
	if(inc < opt_inc2){ // 2セット
	const __m128i vvar3 = _mm_set1_epi32(3);
	for(i = 0; i < count; i += 4) {
	__m128i vofsi1 = _mm_srli_epi32(vofs, FRACTION_BITS);
	__m128i vofsi2 = _mm_add_epi32(vofsi1, vvar1);
	int32 ofs0 = _mm_extract_epi32(vofsi1, 0x0);
	__m128i vin1 = _mm_loadu_si128((__m128i *)&src[ofs0]); // int16*8
	__m128i vin2 = _mm_shuffle_epi32(vin1, 0x4E); // H128bitをL128bitに移動
	__m128 vvf1 = _mm_cvtepi32_ps(_mm_cvtepi16_epi32(vin1)); // int16 to float (float変換でH64bitは消える
	__m128 vvf2 = _mm_cvtepi32_ps(_mm_cvtepi16_epi32(vin2)); // int16 to float (float変換でH64bitは消える	
	__m128i vofsib = _mm_shuffle_epi32(vofsi1, 0x0); 
	__m128i vofsub1 = _mm_sub_epi32(vofsi1, vofsib); 
	__m128i vofsub2 = _mm_sub_epi32(vofsi2, vofsib); 
	__m128i vrm1 = _mm_cmpgt_epi32(vofsub1, vvar3); // オフセット差が4以上の条件でマスク作成
	__m128i vrm2 = _mm_cmpgt_epi32(vofsub2, vvar3); // オフセット差が4以上の条件でマスク作成
	// src2 offsetが下位2bitのみ有効であれば4を超える部分にマスク不要のはず
	__m128 vv11 = _mm_permutevar_ps(vvf1, vofsub1); // v1 ofsi
	__m128 vv12 = _mm_permutevar_ps(vvf2, vofsub1); // v1 ofsi
	__m128 vv21 = _mm_permutevar_ps(vvf1, vofsub2); // v2 ofsi+1
	__m128 vv22 = _mm_permutevar_ps(vvf2, vofsub2); // v2 ofsi+1
	__m128 vv1 = _mm_blendv_ps(vv11, vv12, *((__m128 *)&vrm1)); // v1 ofsi
	__m128 vv2 = _mm_blendv_ps(vv21, vv22, *((__m128 *)&vrm2)); // v2 ofsi+1
	// あとは通常と同じ
	__m128 vfp = _mm_mul_ps(_mm_cvtepi32_ps(_mm_and_si128(vofs, vfmask)), vec_divf);
#if defined(DATA_T_DOUBLE)
	__m128 vec_out = _mm_mul_ps(MM_FMA_PS(_mm_sub_ps(vv2, vv1), vfp, vv1), vec_divo);
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(vec_out));
	dest += 4;
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT 
	__m128 vec_out = _mm_mul_ps(MM_FMA_PS(_mm_sub_ps(vv2, vv1), vfp, vv1), vec_divo);
	_mm256_storeu_ps(dest, vec_out);
	dest += 4;
#else // DATA_T_IN32
	__m128 vec_out = MM_FMA_PS(_mm_sub_ps(vv2, vv1), vfp, vv1);
	_mm_storeu_si128(__m128i *)dest, _mm_cvtps_epi32(vec_out));
	dest += 4;
#endif
	vofs = _mm_add_epi32(vofs, vinc);
	}
	}else
#endif // 2set
		
// x86だとほとんど変わらない x64だとやや速い 1.5%・・
#elif (USE_X86_EXT_INTRIN >= 5) && defined(IX64CPU)
	// 最適化レート = (ロードデータ数 - 初期オフセット小数部の最大値(1未満) - 補間ポイント数(linearは1) ) / オフセットデータ数
	// ロードデータ数は_mm_shuffle_epi8扱えるのint16の8セットになる (=int8*16)
	// 128bitロードデータ(int16*8セット)を全て使用できる
	const int32 opt_inc1 = (1 << FRACTION_BITS) * (8 - 1 - 1) / 4; // (int32*4) * 1セット
	const __m128i vvar1 = _mm_set1_epi32(1);
	const __m128i vshuf = _mm_set_epi8(0x0C,0x0C,0xFF,0xFF,0x08,0x08,0xFF,0xFF, 0x04,0x04,0xFF,0xFF,0x00,0x00,0xFF,0xFF);
	const __m128i vadd = _mm_set_epi8(1,0,0,0,1,0,0,0, 1,0,0,0,1,0,0,0); // 
#if defined(DATA_T_DOUBLE) || defined(DATA_T_DOUBLE)
	const __m128 vec_divo = _mm_set1_ps(DIV_15BIT * DIV_16BIT);
#else
	const __m128 vec_divo = _mm_set1_ps(DIV_16BIT);
#endif
	if(inc < opt_inc1){	
	for(i = 0; i < count; i += 4) {
	__m128i vofsi1 = _mm_srli_epi32(vofs, FRACTION_BITS);
	__m128i vofsi2 = _mm_add_epi32(vofsi1, vvar1);
	int32 ofs0 = _mm_cvtsi128_si32(vofsi1);
	__m128i vin1 = _mm_loadu_si128((__m128i *)&src[ofs0]); // int16*8
	__m128i vofsib = _mm_shuffle_epi32(vofsi1, 0x0); 
	__m128i vofsub1 = _mm_sub_epi32(vofsi1, vofsib); // 0~6 の値になる ハズ
	__m128i vofsub2 = _mm_sub_epi32(vofsi2, vofsib); // 1~7 の値になる ハズ
	__m128i vofmul1 = _mm_slli_epi32(vofsub1, 1); // 2byte単位なのでofsetを2倍 (乗算だと64bitになるのでシフトで
	__m128i vofmul2 = _mm_slli_epi32(vofsub2, 1); // 2byte単位なのでofsetを2倍 (乗算だと64bitになるのでシフトで
	__m128i vofshf1 = _mm_shuffle_epi8(vofmul1, vshuf); // ***4***3***2***1 to 44**33**22**11** (2byte単位なので 4byte上位2byteにセット
	__m128i vofshf2 = _mm_shuffle_epi8(vofmul2, vshuf); // ***8***7***6***5 to 88**77**66**55** (2byte単位なので 4byte上位2byteにセット
	__m128i vofadd1 = _mm_add_epi8(vofshf1, vadd); // 4byte単位(最上位byteに+1 オフセット
	__m128i vofadd2 = _mm_add_epi8(vofshf2, vadd); // 4byte単位(最上位byteに+1 オフセット
	__m128i vi32_1 = _mm_shuffle_epi8(vin1, vofadd1); // v1 ofsi   オフセットによってint16データ移動と同時にint32化
	__m128i vi32_2 = _mm_shuffle_epi8(vin1, vofadd2); // v2 ofsi+1
	__m128 vv1 = _mm_cvtepi32_ps(vi32_1); // int32 to float (float変換でH64bitは消える
	__m128 vv2 = _mm_cvtepi32_ps(vi32_2); // int32 to float (float変換でH64bitは消える
	// あとは通常と同じ (float変換がint16ではなくint32なのでレベル変換が必要 vec_divoに変換係数を追加
	__m128 vfp = _mm_mul_ps(_mm_cvtepi32_ps(_mm_and_si128(vofs, vfmask)), vec_divf);
	__m128 vec_out = _mm_mul_ps(MM_FMA_PS(_mm_sub_ps(vv2, vv1), vfp, vv1), vec_divo);
#if defined(DATA_T_DOUBLE)
#if (USE_X86_EXT_INTRIN >= 8)
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(vec_out));
	dest += 4;
#else
	_mm_storeu_pd(dest, _mm_cvtps_pd(vec_out));
	dest += 2;
	_mm_storeu_pd(dest, _mm_cvtps_pd(_mm_movehl_ps(vec_out, vec_out)));
	dest += 2;
#endif
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT 
	_mm256_storeu_ps(dest, vec_out);
	dest += 4;
#else // DATA_T_IN32
	_mm_storeu_si128(__m128i *)dest, _mm_cvtps_epi32(vec_out));
	dest += 4;
#endif
	vofs = _mm_add_epi32(vofs, vinc);
	}
	}else
#endif 
#endif // LO_OPTIMIZE_INCREMENT
		
	{		
	const __m128 vec_divo = _mm_set1_ps(DIV_15BIT);
	for(; i < count; i += 4) {
	__m128i vofsi = _mm_srli_epi32(vofs, FRACTION_BITS);
	__m128i vin1 = _mm_loadu_si128((__m128i *)&src[MM_EXTRACT_I32(vofsi,0)]); // ofsiとofsi+1をロード
	__m128i vin2 = _mm_loadu_si128((__m128i *)&src[MM_EXTRACT_I32(vofsi,1)]); // 次周サンプルも同じ
	__m128i vin3 = _mm_loadu_si128((__m128i *)&src[MM_EXTRACT_I32(vofsi,2)]); // 次周サンプルも同じ
	__m128i vin4 = _mm_loadu_si128((__m128i *)&src[MM_EXTRACT_I32(vofsi,3)]); // 次周サンプルも同じ	
	__m128i vin12 =	_mm_unpacklo_epi16(vin1, vin2); // [v11v21]e96,[v12v22]e96 to [v11v12v21v22]e64
	__m128i vin34 =	_mm_unpacklo_epi16(vin3, vin4); // [v13v23]e96,[v14v24]e96 to [v13v14v23v24]e64
	__m128i vi16 = _mm_unpacklo_epi32(vin12, vin34); // [v11v12,v21v22]e64,[v13v14,v23v24]e64 to [v11v12v13v14,v21v22v23v24]e0
#if (USE_X86_EXT_INTRIN >= 6) // sse4.1 , _mm_ cvtepi16_epi32()
	__m128i vi16_2 = _mm_shuffle_epi32(vi16, 0x4e); // ofsi+1はL64bitへ
	__m128 vv1 = _mm_cvtepi32_ps(_mm_cvtepi16_epi32(vi16)); // int16 to float
	__m128 vv2 = _mm_cvtepi32_ps(_mm_cvtepi16_epi32(vi16_2)); // int16 to float
#else
	__m128i sign = _mm_cmpgt_epi16(_mm_setzero_si128(), vi16);
	__m128 vv1 = _mm_cvtepi32_ps(_mm_unpacklo_epi16(vi16, sign)); // int16 to float
	__m128 vv2 = _mm_cvtepi32_ps(_mm_unpackhi_epi16(vi16, sign)); // int16 to float
#endif	
	__m128 vfp = _mm_mul_ps(_mm_cvtepi32_ps(_mm_and_si128(vofs, vfmask)), vec_divf);
#if defined(DATA_T_DOUBLE)
	__m128 vec_out = _mm_mul_ps(MM_FMA_PS(_mm_sub_ps(vv2, vv1), vfp, vv1), vec_divo);
#if (USE_X86_EXT_INTRIN >= 8)
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(vec_out));
	dest += 4;
#else
	_mm_storeu_pd(dest, _mm_cvtps_pd(vec_out));
	dest += 2;
	_mm_storeu_pd(dest, _mm_cvtps_pd(_mm_movehl_ps(vec_out, vec_out)));
	dest += 2;
#endif
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT
	__m128 vec_out = _mm_mul_ps(MM_FMA_PS(_mm_sub_ps(vv2, vv1), vfp, vv1), vec_divo);
	_mm_storeu_ps(dest, vec_out);
	dest += 4;
#else // DATA_T_IN32
	__m128 vec_out = MM_FMA_PS(_mm_sub_ps(vv2, vv1), vfp, vv1);
	_mm_storeu_si128((__m128i *)dest, _mm_cvtps_epi32(vec_out));
	dest += 4;
#endif
	vofs = _mm_add_epi32(vofs, vinc);
	}
	}
	resrc->offset = prec_offset + (splen_t)(MM_EXTRACT_I32(vofs,0));
	*out_count = i;
    return dest;
}

#elif (USE_X86_EXT_INTRIN >= 2) // SSE (not use MMX
// offset:int32*4, resamp:float*4
// ループ内部のoffset計算をint32値域にする , (sample_increment * (req_count+1)) < int32 max
static inline DATA_T *resample_linear_multi(Voice *vp, DATA_T *dest, int32 req_count, int32 *out_count)
{	
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0;
	const uint32 req_count_mask = ~(0x3);
	const int32 count = req_count & req_count_mask;
	splen_t prec_offset = resrc->offset & INTEGER_MASK;
	sample_t *src = vp->sample->data + (prec_offset >> FRACTION_BITS);
	int32 ofs = (int32)(resrc->offset & FRACTION_MASK);
	int32 inc = resrc->increment;
	__m128 vec_divo = _mm_set1_ps((float)DIV_15BIT), vec_divf = _mm_set1_ps((float)div_fraction);
	for(i = 0; i < count; i += 4) {
		int32 ofsi;		
		__m128 vv1 = _mm_setzero_ps(), vv2 = _mm_setzero_ps(), vfp = _mm_setzero_ps(), vec_out;
		ofsi = ofs >> FRACTION_BITS; 
		vfp = _mm_cvt_si2ss(vfp, ofs & FRACTION_MASK), vfp = _mm_shuffle_ps(vfp, vfp, 0x0); ofs += inc;
		vv1 = _mm_cvt_si2ss(vv1, src[ofsi]), vv1 = _mm_shuffle_ps(vv1, vv1, 0x0); // [0,0,0,0]
		vv2 = _mm_cvt_si2ss(vv2, src[++ofsi]), vv2 = _mm_shuffle_ps(vv2, vv2, 0x0);
		ofsi = ofs >> FRACTION_BITS; 
		vfp = _mm_cvt_si2ss(vfp, ofs & FRACTION_MASK), vfp = _mm_shuffle_ps(vfp, vfp, 0xc0); ofs += inc;
		vv1 = _mm_cvt_si2ss(vv1, src[ofsi]), vv1 = _mm_shuffle_ps(vv1, vv1, 0xc0); // [1,1,1,0]
		vv2 = _mm_cvt_si2ss(vv2, src[++ofsi]), vv2 = _mm_shuffle_ps(vv2, vv2, 0xc0);
		ofsi = ofs >> FRACTION_BITS; 
		vfp = _mm_cvt_si2ss(vfp, ofs & FRACTION_MASK), vfp = _mm_shuffle_ps(vfp, vfp, 0xe0); ofs += inc;
		vv1 = _mm_cvt_si2ss(vv1, src[ofsi]), vv1 = _mm_shuffle_ps(vv1, vv1, 0xe0); // [2,2,1,0]
		vv2 = _mm_cvt_si2ss(vv2, src[++ofsi]), vv2 = _mm_shuffle_ps(vv2, vv2, 0xe0);
		ofsi = ofs >> FRACTION_BITS; 
		vfp = _mm_cvt_si2ss(vfp, ofs & FRACTION_MASK), vfp = _mm_shuffle_ps(vfp, vfp, 0x1b); ofs += inc;
		vv1 = _mm_cvt_si2ss(vv1, src[ofsi]), vv1 = _mm_shuffle_ps(vv1, vv1, 0x1b); // [3,2,1,0] to [1,2,3,4]
		vv2 = _mm_cvt_si2ss(vv2, src[++ofsi]), vv2 = _mm_shuffle_ps(vv2, vv2, 0x1b);			
#if defined(DATA_T_DOUBLE)
		vec_out = _mm_mul_ps(MM_FMA_PS(_mm_sub_ps(vv2, vv1), _mm_mul_ps(vfp, vec_divf), vv1), vec_divo);
		*dest++ = (DATA_T)MM_EXTRACT_F32(vec_out,0);
		*dest++ = (DATA_T)MM_EXTRACT_F32(vec_out,1);
		*dest++ = (DATA_T)MM_EXTRACT_F32(vec_out,2);
		*dest++ = (DATA_T)MM_EXTRACT_F32(vec_out,3);
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT
		_mm_storeu_ps(dest, _mm_mul_ps(MM_FMA_PS(_mm_sub_ps(vv2, vv1), _mm_mul_ps(vfp, vec_divf), vv1), vec_divo));
		dest += 4;
#else // DATA_T_IN32
		vec_out = MM_FMA_PS(_mm_sub_ps(vv2, vv1), _mm_mul_ps(vfp, vec_divf), vv1);		
		*dest++ = _mm_cvt_ss2si(vec_out);
		*dest++ = _mm_cvt_ss2si(_mm_shuffle_ps(vec_out, vec_out, 0xe5));
		*dest++ = _mm_cvt_ss2si(_mm_shuffle_ps(vec_out, vec_out, 0xea));
		*dest++ = _mm_cvt_ss2si(_mm_shuffle_ps(vec_out, vec_out, 0xff));
#endif
	}
	resrc->offset = prec_offset + (splen_t)ofs;
	*out_count = i;
    return dest;
}

#elif 1 // not use MMX/SSE/AVX 
// offset:int32*4, resamp:float*4
// ループ内部のoffset計算をint32値域にする , (sample_increment * (req_count+1)) < int32 max
static inline DATA_T *resample_linear_multi(Voice *vp, DATA_T *dest, int32 req_count, int32 *out_count)
{
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0;
	const uint32 req_count_mask = ~(0x3);
	const int32 count = req_count & req_count_mask;
	splen_t prec_offset = resrc->offset & INTEGER_MASK;
	sample_t *src = vp->sample->data + (prec_offset >> FRACTION_BITS);
	int32 ofs = (int32)(resrc->offset & FRACTION_MASK);
	int32 inc = resrc->increment;
	for(i = 0; i < count; i += 4) {
		int32 v1, v2, ofsi, fp;
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
		ofsi = ofs >> FRACTION_BITS, fp = ofs & FRACTION_MASK; ofs += inc;
		v1 = src[ofsi], v2 = src[++ofsi];
		*dest++ = ((FLOAT_T)v1 + (FLOAT_T)(v2 - v1)	* (FLOAT_T)fp * div_fraction) * OUT_INT16;
		ofsi = ofs >> FRACTION_BITS, fp = ofs & FRACTION_MASK; ofs += inc;
		v1 = src[ofsi], v2 = src[++ofsi];
		*dest++ = ((FLOAT_T)v1 + (FLOAT_T)(v2 - v1)	* (FLOAT_T)fp * div_fraction) * OUT_INT16;
		ofsi = ofs >> FRACTION_BITS, fp = ofs & FRACTION_MASK; ofs += inc;
		v1 = src[ofsi], v2 = src[++ofsi];
		*dest++ = ((FLOAT_T)v1 + (FLOAT_T)(v2 - v1)	* (FLOAT_T)fp * div_fraction) * OUT_INT16;
		ofsi = ofs >> FRACTION_BITS, fp = ofs & FRACTION_MASK; ofs += inc;
		v1 = src[ofsi], v2 = src[++ofsi];
		*dest++ = ((FLOAT_T)v1 + (FLOAT_T)(v2 - v1)	* (FLOAT_T)fp * div_fraction) * OUT_INT16;
#else // DATA_T_IN32
		ofsi = ofs >> FRACTION_BITS, fp = ofs & FRACTION_MASK; ofs += inc;
		v1 = src[ofsi], v2 = src[++ofsi];
	//	*dest++ = v1 + (((v2 - v1) * fp) >> FRACTION_BITS);
		*dest++ = v1 + imuldiv_fraction((v2 - v1), fp);
		ofsi = ofs >> FRACTION_BITS, fp = ofs & FRACTION_MASK; ofs += inc;
		v1 = src[ofsi], v2 = src[++ofsi];
	//	*dest++ = v1 + (((v2 - v1) * fp) >> FRACTION_BITS);
		*dest++ = v1 + imuldiv_fraction((v2 - v1), fp);
		ofsi = ofs >> FRACTION_BITS, fp = ofs & FRACTION_MASK; ofs += inc;
		v1 = src[ofsi], v2 = src[++ofsi];
	//	*dest++ = v1 + (((v2 - v1) * fp) >> FRACTION_BITS);
		*dest++ = v1 + imuldiv_fraction((v2 - v1), fp);
		ofsi = ofs >> FRACTION_BITS, fp = ofs & FRACTION_MASK; ofs += inc;
		v1 = src[ofsi], v2 = src[++ofsi];
	//	*dest++ = v1 + (((v2 - v1) * fp) >> FRACTION_BITS);
		*dest++ = v1 + imuldiv_fraction((v2 - v1), fp);
#endif
	}
	resrc->offset = prec_offset + (splen_t)ofs;
	*out_count = i;
    return dest;
}

#else // normal
// ループ内部のoffset計算をint32値域にする , (sample_increment * (req_count+1)) < int32 max
static inline DATA_T *resample_linear_multi(Voice *vp, DATA_T *dest, int32 req_count, int32 *out_count)
{
	int32 i;
	resample_rec_t *resrc = &vp->resrc;	
	splen_t prec_offset = resrc->offset & INTEGER_MASK;
	sample_t *src = vp->sample->data + (prec_offset >> FRACTION_BITS);
	const int32 start_offset = (int32)(resrc->offset - prec_offset); // offset計算をint32値域にする(SIMD用
	int32 ofs = (int32)(resrc->offset & FRACTION_MASK);
	const int32 inc = resrc->increment;

	for(i = 0; i < req_count; i++) {	
		int32 ofsi = ofs >> FRACTION_BITS;
		int32 ofsf = ofs & FRACTION_MASK;	
		int32 v1 = src[ofsi];
		int32 v2 = src[ofsi + 1];	
	//	*dest++ = ((FLOAT_T)v1 + (FLOAT_T)(v2 - v1) * (FLOAT_T)ofsf * div_fraction) * OUT_INT16;
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
		*dest++ = ((FLOAT_T)v1 + (FLOAT_T)(v2 - v1) * (FLOAT_T)ofsf * div_fraction) * OUT_INT16;
#else
		*dest++ = (v1 + imuldiv_fraction((v2 - v1), ofsf);
#endif
		ofs += inc;
	}
	resrc->offset = prec_offset + (splen_t)ofs;
	*out_count = i;
    return dest;
}
#endif

static void lo_rs_plain(Voice *vp, DATA_T *dest, int32 count)
{
	/* Play sample until end, then free the voice. */
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0, j;

	if (resrc->increment < 0) resrc->increment = -resrc->increment; /* In case we're coming out of a bidir loop */
	j = PRECALC_LOOP_COUNT(resrc->offset, resrc->data_length, resrc->increment) + 1; // safe end+128 sample
	if (j > count) {j = count;}
	else if(j < 0) {j = 0;}	
	dest = resample_linear_multi(vp, dest, j, &i);
	for(; i < j; i++) {
		*dest++ = resample_linear_single(vp);
		resrc->offset += resrc->increment;
	}
	for(; i < count; i++) { *dest++ = 0; }
	if (resrc->offset >= resrc->data_length)
		vp->finish_voice = 1;
}

static void lo_rs_loop(Voice *vp, DATA_T *dest, int32 count)
{
	/* Play sample until end-of-loop, skip back and continue. */
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0, j;
	
	j = PRECALC_LOOP_COUNT(resrc->offset, resrc->loop_end, resrc->increment) - 2; // 2point interpolation
	if (j > count) {j = count;}
	else if(j < 0) {j = 0;}
	dest = resample_linear_multi(vp, dest, j, &i);
	for(; i < count; i++) {
		*dest++ = resample_linear_single(vp);
		resrc->offset += resrc->increment;
		while(resrc->offset >= resrc->loop_end)
			resrc->offset -= resrc->loop_end - resrc->loop_start;
		/* The loop may not be longer than an increment. */
	}
}

static void lo_rs_bidir(Voice *vp, DATA_T *dest, int32 count)
{
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0, j = 0;	

	if (resrc->increment > 0){
		j = PRECALC_LOOP_COUNT(resrc->offset, resrc->loop_end, resrc->increment) - 2; // 2point interpolation
		if (j > count) {j = count;}
		else if(j < 0) {j = 0;}
		dest = resample_linear_multi(vp, dest, j, &i);
	}
	for(; i < count; i++) {
		*dest++ = resample_linear_single(vp);
		resrc->offset += resrc->increment;
		if(resrc->increment > 0){
			if(resrc->offset >= resrc->loop_end){
				resrc->offset = (resrc->loop_end << 1) - resrc->offset;
				resrc->increment = -resrc->increment;
			}
		}else{
			if(resrc->offset <= resrc->loop_start){
				resrc->offset = (resrc->loop_start << 1) - resrc->offset;
				resrc->increment = -resrc->increment;
			}
		}
	}
}

static inline void resample_voice_linear_optimize(Voice *vp, DATA_T *ptr, int32 count)
{
    int mode = vp->sample->modes;
	
	if(vp->resrc.plain_flag){ /* no loop */ /* else then loop */ 
		lo_rs_plain(vp, ptr, count);	/* no loop */
	}else if(!(mode & MODES_ENVELOPE) && (vp->status & (VOICE_OFF | VOICE_DIE))){ /* no env */
		vp->resrc.plain_flag = 1; /* lock no loop */
		lo_rs_plain(vp, ptr, count);	/* no loop */
	}else if(mode & MODES_RELEASE && (vp->status & VOICE_OFF)){ /* release sample */
		vp->resrc.plain_flag = 1; /* lock no loop */
		lo_rs_plain(vp, ptr, count);	/* no loop */
	}else if(mode & MODES_PINGPONG){ /* Bidirectional */
		lo_rs_bidir(vp, ptr, count);	/* Bidirectional loop */
	}else {
		lo_rs_loop(vp, ptr, count);	/* loop */
	}		
}
#endif /* optimize linear resample */

/*************** optimize linear int32 resample *****************/
#if defined(PRECALC_LOOPS)
#define LO_OPTIMIZE_INCREMENT

static inline DATA_T resample_linear_int32_single(Voice *vp)
{	
    const int32 *src = (const int32*)vp->sample->data;
    const fract_t ofsf = vp->resrc.offset & FRACTION_MASK;
	const spos_t ofsi = vp->resrc.offset >> FRACTION_BITS;
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
    FLOAT_T v1 = src[ofsi], v2 = src[ofsi + 1], fp = ofsf;
    return (v1 + (v2 - v1) * fp * div_fraction) * OUT_INT32; // FLOAT_T
#else // DATA_T_IN32
    int32 v1 = src[ofsi], v2 = src[ofsi + 1];
	return v1 + imuldiv_fraction_int32(v2 - v1, ofsf);
#endif
}

#if (USE_X86_EXT_INTRIN >= 10)
// offset:int32*16, resamp:float*16
static inline DATA_T *resample_linear_int32_multi(Voice *vp, DATA_T *dest, int32 req_count, int32 *out_count)
{
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0;
	const int32 count = req_count & ~15;
	splen_t prec_offset = resrc->offset & INTEGER_MASK;
	int32 *src = (int32 *)vp->sample->data + (prec_offset >> FRACTION_BITS);
	int32 start_offset = (int32)(resrc->offset - prec_offset); // (offset計算をint32値域にする(SIMD用
	int32 inc = resrc->increment;

	__m512i vinit = _mm512_mullo_epi32(_mm512_set_epi32(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0), _mm512_set1_epi32(inc));
	__m512i vofs = _mm512_add_epi32(_mm512_set1_epi32(start_offset), vinit);
	__m512i vinc = _mm512_set1_epi32(inc * 16), vfmask = _mm512_set1_epi32((int32)FRACTION_MASK);
	__m512 vec_divo = _mm512_set1_ps(DIV_31BIT), vec_divf = _mm512_set1_ps(div_fraction);

#ifdef LO_OPTIMIZE_INCREMENT
#ifdef USE_PERMUTEX2
	const int32 opt_inc1 = (1 << FRACTION_BITS) * (32 - 1 - 1) / 16; // (float*16) * 1セット
#else
	const int32 opt_inc1 = (1 << FRACTION_BITS) * (16 - 1 - 1) / 16; // (float*16) * 1セット
#endif
	const __m512i vvar1 = _mm512_set1_epi32(1);
	if (inc < opt_inc1) {
		for (i = 0; i < count; i+= 16) {
			__m512i vofsi1 = _mm512_srli_epi32(vofs, FRACTION_BITS);
			__m512i vofsi2 = _mm512_add_epi32(vofsi1, vvar1);
			int32 ofs0 = _mm_cvtsi128_si32(_mm512_castsi512_si128(vofsi1));
			__m512i vin1 = _mm512_loadu_epi32(&src[ofs0]); // int32*16
#ifdef USE_PERMUTEX2
			__m512i vin2 = _mm512_loadu_epi32(&src[ofs0 + 16]); // int32*16
#endif
			__m512i vofsib = _mm512_broadcastd_epi32(_mm512_castsi512_si128(vofsi1));
			__m512i vofsub1 = _mm512_sub_epi32(vofsi1, vofsib);
			__m512i vofsub2 = _mm512_sub_epi32(vofsi2, vofsib);
			__m512 vvf1 = _mm512_cvtepi32_ps(vin1);
#ifdef USE_PERMUTEX2
			__m512 vvf2 = _mm512_cvtepi32_ps(vin2);
			__m512 vv1 = _mm512_permutex2var_ps(vvf1, vofsub1, vvf2); // v1 ofsi
			__m512 vv2 = _mm512_permutex2var_ps(vvf1, vofsub2, vvf2); // v2 ofsi+1
#else
			__m512 vv1 = _mm512_permutexvar_ps(vofsub1, vvf1); // v1 ofsi
			__m512 vv2 = _mm512_permutexvar_ps(vofsub2, vvf1); // v2 ofsi+1
#endif
			// あとは通常と同じ
			__m512 vfp = _mm512_mul_ps(_mm512_cvtepi32_ps(_mm512_and_epi32(vofs, vfmask)), vec_divf);
#if defined(DATA_T_DOUBLE)
			__m512 vec_out = _mm512_mul_ps(_mm512_fmadd_ps(_mm512_sub_ps(vv2, vv1), vfp, vv1), vec_divo);
			_mm512_storeu_pd(dest, _mm512_cvtps_pd(_mm512_castps512_ps256(vec_out)));
			dest += 8;
			_mm512_storeu_pd(dest, _mm512_cvtps_pd(_mm512_extractf32x8_ps(vec_out, 1)));
			dest += 8;
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT 
			__m512 vec_out = _mm512_mul_ps(_mm512_fmadd_ps(_mm512_sub_ps(vv2, vv1), vfp, vv1), vec_divo);
			_mm512_storeu_ps(dest, vec_out);
			dest += 16;
#else // DATA_T_IN32
			__m512 vec_out = _mm512_fmadd_ps(_mm512_sub_ps(vv2, vv1), vfp, vv1);
			_mm512_storeu_epi32((__m512i *)dest, _mm512_cvtps_epi32(vec_out));
			dest += 16;
#endif
			vofs = _mm512_add_epi32(vofs, vinc);
		}
	}
#endif // LO_OPTIMIZE_INCREMENT
	for (; i < count; i += 16) {
		__m512i vofsi = _mm512_srli_epi32(vofs, FRACTION_BITS);
#if 1
		__m512 vv1 = _mm512_cvtepi32_ps(_mm512_i32gather_epi32(vofsi, src, 4));
		__m512 vv2 = _mm512_cvtepi32_ps(_mm512_i32gather_epi32(_mm512_add_epi32(vofsi, _mm512_set1_epi32(1)), src, 4));
#else
		__m128i vin1 = _mm_loadu_si128((__m128i *)&src[MM512_EXTRACT_I32(vofsi, 0)]);
		__m128i vin2 = _mm_loadu_si128((__m128i *)&src[MM512_EXTRACT_I32(vofsi, 1)]);
		__m128i vin3 = _mm_loadu_si128((__m128i *)&src[MM512_EXTRACT_I32(vofsi, 2)]);
		__m128i vin4 = _mm_loadu_si128((__m128i *)&src[MM512_EXTRACT_I32(vofsi, 3)]);
		__m128i vin5 = _mm_loadu_si128((__m128i *)&src[MM512_EXTRACT_I32(vofsi, 4)]);
		__m128i vin6 = _mm_loadu_si128((__m128i *)&src[MM512_EXTRACT_I32(vofsi, 5)]);
		__m128i vin7 = _mm_loadu_si128((__m128i *)&src[MM512_EXTRACT_I32(vofsi, 6)]);
		__m128i vin8 = _mm_loadu_si128((__m128i *)&src[MM512_EXTRACT_I32(vofsi, 7)]);
		__m128i vin9 = _mm_loadu_si128((__m128i *)&src[MM512_EXTRACT_I32(vofsi, 8)]);
		__m128i vin10 = _mm_loadu_si128((__m128i *)&src[MM512_EXTRACT_I32(vofsi, 9)]);
		__m128i vin11 = _mm_loadu_si128((__m128i *)&src[MM512_EXTRACT_I32(vofsi, 10)]);
		__m128i vin12 = _mm_loadu_si128((__m128i *)&src[MM512_EXTRACT_I32(vofsi, 11)]);
		__m128i vin13 = _mm_loadu_si128((__m128i *)&src[MM512_EXTRACT_I32(vofsi, 12)]);
		__m128i vin14 = _mm_loadu_si128((__m128i *)&src[MM512_EXTRACT_I32(vofsi, 13)]);
		__m128i vin15 = _mm_loadu_si128((__m128i *)&src[MM512_EXTRACT_I32(vofsi, 14)]);
		__m128i vin16 = _mm_loadu_si128((__m128i *)&src[MM512_EXTRACT_I32(vofsi, 15)]);
		__m256i vin1_5 = _mm256_inserti32x4(_mm256_castsi128_si256(vin1), vin5, 1);
		__m256i vin2_6 = _mm256_inserti32x4(_mm256_castsi128_si256(vin2), vin6, 1);
		__m256i vin3_7 = _mm256_inserti32x4(_mm256_castsi128_si256(vin3), vin7, 1);
		__m256i vin4_8 = _mm256_inserti32x4(_mm256_castsi128_si256(vin4), vin8, 1);
		__m256i vin9_13 = _mm256_inserti32x4(_mm256_castsi128_si256(vin9), vin13, 1);
		__m256i vin10_14 = _mm256_inserti32x4(_mm256_castsi128_si256(vin10), vin14, 1);
		__m256i vin11_15 = _mm256_inserti32x4(_mm256_castsi128_si256(vin11), vin15, 1);
		__m256i vin12_16 = _mm256_inserti32x4(_mm256_castsi128_si256(vin12), vin16, 1);
		__m512i vin1_5_9_13 = _mm512_inserti32x8(_mm512_castsi256_si512(vin1_5), vin9_13, 1);
		__m512i vin2_6_10_14 = _mm512_inserti32x8(_mm512_castsi256_si512(vin2_6), vin10_14, 1);
		__m512i vin3_7_11_15 = _mm512_inserti32x8(_mm512_castsi256_si512(vin3_7), vin11_15, 1);
		__m512i vin4_8_12_16 = _mm512_inserti32x8(_mm512_castsi256_si512(vin4_8), vin12_16, 1);
		__m512 vin1_2_5_6_9_10_13_14 = _mm512_cvtepi32_ps(_mm512_unpacklo_epi32(vin1_5_9_13, vin2_6_10_14));
		__m512 vin3_4_7_8_11_12_15_16 = _mm512_cvtepi32_ps(_mm512_unpacklo_epi32(vin3_7_11_15, vin4_8_12_16));
		__m512 vv1 = _mm512_shuffle_ps(vin1_2_5_6_9_10_13_14, vin3_4_7_8_11_12_15_16, _MM_SHUFFLE(1, 0, 1, 0));
		__m512 vv2 = _mm512_shuffle_ps(vin1_2_5_6_9_10_13_14, vin3_4_7_8_11_12_15_16, _MM_SHUFFLE(3, 2, 3, 2));
#endif
		__m512 vfp = _mm512_mul_ps(_mm512_cvtepi32_ps(_mm512_and_epi32(vofs, vfmask)), vec_divf);
#if defined(DATA_T_DOUBLE)
		__m512 vec_out = _mm512_mul_ps(_mm512_fmadd_ps(_mm512_sub_ps(vv2, vv1), vfp, vv1), vec_divo);
		_mm512_storeu_pd(dest, _mm512_cvtps_pd(_mm512_castps512_ps256(vec_out)));
		dest += 8;
		_mm512_storeu_pd(dest, _mm512_cvtps_pd(_mm512_extractf32x8_ps(vec_out, 1)));
		dest += 8;
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT
		__m512 vec_out = _mm512_mul_ps(_mm512_fmadd_ps(_mm512_sub_ps(vv2, vv1), vfp, vv1), vec_divo);
		_mm512_storeu_ps(dest, vec_out);
		dest += 16;
#else // DATA_T_IN32
		__m512 vec_out = _mm512_fmadd_ps(_mm512_sub_ps(vv2, vv1), vfp, vv1);
		_mm512_storeu_epi32((__m512i *)dest, _mm512_cvtps_epi32(vec_out));
		dest += 16;
#endif
		vofs = _mm512_add_epi32(vofs, vinc);
	}
	resrc->offset = prec_offset + (splen_t)(_mm_cvtsi128_si32(_mm512_castsi512_si128(vofs)));
	*out_count = i;
	return dest;
}
#elif (USE_X86_EXT_INTRIN >= 9)
// offset:int32*8, resamp:float*8
// ループ内部のoffset計算をint32値域にする , (sample_increment * (req_count+1)) < int32 max
static inline DATA_T *resample_linear_int32_multi(Voice *vp, DATA_T *dest, int32 req_count, int32 *out_count)
{
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0;
	const int32 req_count_mask = ~(0x7);
	const int32 count = req_count & req_count_mask;
	splen_t prec_offset = resrc->offset & INTEGER_MASK;
	int32 *src = (int32 *)vp->sample->data + (prec_offset >> FRACTION_BITS);
	int32 start_offset = (int32)(resrc->offset - prec_offset); // (offset計算をint32値域にする(SIMD用
	int32 inc = resrc->increment;
	__m256i vinit = _mm256_set_epi32(inc * 7, inc * 6, inc * 5, inc * 4, inc * 3, inc * 2, inc, 0);
	__m256i vofs = _mm256_add_epi32(_mm256_set1_epi32(start_offset), vinit);
	__m256i vinc = _mm256_set1_epi32(inc * 8), vfmask = _mm256_set1_epi32((int32)FRACTION_MASK);
	__m256 vec_divo = _mm256_set1_ps(DIV_31BIT), vec_divf = _mm256_set1_ps(div_fraction);

#ifdef LO_OPTIMIZE_INCREMENT
	// 最適化レート = (ロードデータ数 - 初期オフセット小数部の最大値(1未満) - 補間ポイント数(linearは1) ) / オフセットデータ数
	// ロードデータ数はint16用permutevarがないので変換後の32bit(int32/float)の8セットになる
	// 256bitロードデータ(int16*16セット)を全て使うにはSIMD2セットで対応
	const int32 opt_inc1 = (1 << FRACTION_BITS) * (8 - 1 - 1) / 8; // (float*8) * 1セット
	const int32 opt_inc2 = (1 << FRACTION_BITS) * (16 - 1 - 1) / 8; // (float*8) * 2セット
	const __m256i vvar1 = _mm256_set1_epi32(1);
	if(inc < opt_inc1){	// 1セット	
	for(i = 0; i < count; i += 8) {
	__m256i vofsi1 = _mm256_srli_epi32(vofs, FRACTION_BITS);
	__m256i vofsi2 = _mm256_add_epi32(vofsi1, vvar1);
	int32 ofs0 = _mm_cvtsi128_si32(_mm256_castsi256_si128(vofsi1));
	__m256i vin1 = _mm256_loadu_si256((__m256i *)&src[ofs0]); // int32*8
	__m256i vofsib = _mm256_broadcastd_epi32(_mm256_castsi256_si128(vofsi1));
	__m256i vofsub1 = _mm256_sub_epi32(vofsi1, vofsib); 
	__m256i vofsub2 = _mm256_sub_epi32(vofsi2, vofsib); 
	__m256 vvf1 = _mm256_cvtepi32_ps(vin1);
	__m256 vv1 = _mm256_permutevar8x32_ps(vvf1, vofsub1); // v1 ofsi
	__m256 vv2 = _mm256_permutevar8x32_ps(vvf1, vofsub2); // v2 ofsi+1
	// あとは通常と同じ
	__m256 vfp = _mm256_mul_ps(_mm256_cvtepi32_ps(_mm256_and_si256(vofs, vfmask)), vec_divf);
#if defined(DATA_T_DOUBLE)
	__m256 vec_out = _mm256_mul_ps(MM256_FMA_PS(_mm256_sub_ps(vv2, vv1), vfp, vv1), vec_divo);
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(_mm256_castps256_ps128(vec_out)));
	dest += 4;
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(_mm256_extractf128_ps(vec_out, 1)));	
	dest += 4;
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT 
	__m256 vec_out = _mm256_mul_ps(MM256_FMA_PS(_mm256_sub_ps(vv2, vv1), vfp, vv1), vec_divo);
	_mm256_storeu_ps(dest, vec_out);
	dest += 8;
#else // DATA_T_IN32
	__m256 vec_out = MM256_FMA_PS(_mm256_sub_ps(vv2, vv1), vfp, vv1);
	_mm256_storeu_si256(__m256i *)dest, _mm256_cvtps_epi32(vec_out));
	dest += 8;
#endif
	vofs = _mm256_add_epi32(vofs, vinc);
	}
	}else
#if 0 // 2set
	if(inc < opt_inc2){ // 2セット
	const __m256i vvar7 = _mm256_set1_epi32(7);
	for(i = 0; i < count; i += 8) {
	__m256i vofsi1 = _mm256_srli_epi32(vofs, FRACTION_BITS); // ofsi
	__m256i vofsi2 = _mm256_add_epi32(vofsi1, vadd1); // ofsi+1
	int32 ofs0 = _mm_extract_epi32(_mm256_extracti128si256(vofsi1, 0x0), 0x0);
	__m256i vin1 = _mm256_loadu_si256((__m256i *)&src[ofs0]); // int16*16
	__m256i vin2 = _mm256_permutevar8x32_epi32(vin1, _mm256_set_epi32(3,2,1,0,7,6,5,4)); // H128bitをL128bitに移動
	__m256 vvf1 = _mm256_cvtepi32_ps(_mm256_cvtepi16_epi32(vin1)); // int16 to float (float変換でH128bitは消える
	__m256 vvf2 = _mm256_cvtepi32_ps(_mm256_cvtepi16_epi32(vin2)); // int16 to float (float変換でH128bitは消える	
	__m256i vofsib = _mm256_permutevar8x32_epi32(vofsi, _mm256_setzero_epi32()); // ofsi[0]
	__m256i vofsub1 = _mm256_sub_epi32(vofsi1, vofsib); // v1 ofsi
	__m256i vofsub2 = _mm256_sub_epi32(vofsi2, vofsib); // v2 ofsi+1
	__m256i vrm1 = _mm256_cmpgt_epi32(vofsub1, vvar7); // オフセット差が8以上の条件でマスク作成
	__m256i vrm2 = _mm256_cmpgt_epi32(vofsub2, vvar7); // オフセット差が8以上の条件でマスク作成
	// src2 offsetが下位3bitのみ有効であれば8を超える部分にマスク不要のはず
	__m256 vv11 = _mm256_permutevar8x32_ps(vvf1, vofsub1);
	__m256 vv12 = _mm256_permutevar8x32_ps(vvf2, vofsub1);
	__m256 vv21 = _mm256_permutevar8x32_ps(vvf1, vofsub2);
	__m256 vv22 = _mm256_permutevar8x32_ps(vvf2, vofsub2);	
	__m256 vv1 = _mm256_blendv_ps(vv11, vv12, vrm1); // v1 ofsi
	__m256 vv2 = _mm256_blendv_ps(vv21, vv22, vrm2); // v2 ofsi+1
	// あとは通常と同じ
	__m256 vfp = _mm256_mul_ps(_mm256_cvtepi32_ps(_mm256_and_si256(vofs, vfmask)), vec_divf);
#if defined(DATA_T_DOUBLE)
	__m256 vec_out = _mm256_mul_ps(MM256_FMA_PS(_mm256_sub_ps(vv2, vv1), vfp, vv1), vec_divo);
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(_mm256_extractf128_ps(vec_out, 0)));
	dest += 4;
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(_mm256_extractf128_ps(vec_out, 1)));	
	dest += 4;
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT
	__m256 vec_out = _mm256_mul_ps(MM256_FMA_PS(_mm256_sub_ps(vv2, vv1), vfp, vv1), vec_divo);
	_mm256_storeu_ps(dest, vec_out);
	dest += 8;
#else // DATA_T_IN32
	__m256 vec_out = MM256_FMA_PS(_mm256_sub_ps(vv2, vv1), vfp, vv1);
	_mm256_storeu_si256((__m256i *)dest, _mm256_cvtps_epi32(vec_out));
	dest += 8;
#endif
	vofs = _mm256_add_epi32(vofs, vinc);
	}
	}else
#endif // 2set
#endif // LO_OPTIMIZE_INCREMENT

	for(; i < count; i += 8) {
	__m256i vofsi = _mm256_srli_epi32(vofs, FRACTION_BITS);
#if 0
	__m256 vv1 = _mm256_cvtepi32_ps(_mm256_i32gather_epi32(src, vofsi, 4));
	__m256 vv2 = _mm256_cvtepi32_ps(_mm256_i32gather_epi32(src, _mm256_add_epi32(vofsi, _mm256_set1_epi32(1)), 4));
#else
	__m128i vin1 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,0)]);
	__m128i vin2 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,1)]);
	__m128i vin3 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,2)]);
	__m128i vin4 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,3)]);
	__m128i vin5 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,4)]);
	__m128i vin6 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,5)]);
	__m128i vin7 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,6)]);
	__m128i vin8 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,7)]);
	__m256i vin15 = _mm256_inserti128_si256(_mm256_castsi128_si256(vin1), vin5, 1);
	__m256i vin26 = _mm256_inserti128_si256(_mm256_castsi128_si256(vin2), vin6, 1);
	__m256i vin37 = _mm256_inserti128_si256(_mm256_castsi128_si256(vin3), vin7, 1);
	__m256i vin48 = _mm256_inserti128_si256(_mm256_castsi128_si256(vin4), vin8, 1);
	__m256 vin1256 = _mm256_cvtepi32_ps(_mm256_unpacklo_epi32(vin15, vin26));
	__m256 vin3478 = _mm256_cvtepi32_ps(_mm256_unpacklo_epi32(vin37, vin48));
	__m256 vv1 = _mm256_shuffle_ps(vin1256, vin3478, _MM_SHUFFLE(1, 0, 1, 0));
	__m256 vv2 = _mm256_shuffle_ps(vin1256, vin3478, _MM_SHUFFLE(3, 2, 3, 2));
#endif
	__m256 vfp = _mm256_mul_ps(_mm256_cvtepi32_ps(_mm256_and_si256(vofs, vfmask)), vec_divf);
#if defined(DATA_T_DOUBLE)
	__m256 vec_out = _mm256_mul_ps(MM256_FMA_PS(_mm256_sub_ps(vv2, vv1), vfp, vv1), vec_divo);
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(_mm256_castps256_ps128(vec_out)));
	dest += 4;
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(_mm256_extractf128_ps(vec_out, 1)));	
	dest += 4;
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT
	__m256 vec_out = _mm256_mul_ps(MM256_FMA_PS(_mm256_sub_ps(vv2, vv1), vfp, vv1), vec_divo);
	_mm256_storeu_ps(dest, vec_out);
	dest += 8;
#else // DATA_T_IN32
	__m256 vec_out = MM256_FMA_PS(_mm256_sub_ps(vv2, vv1), vfp, vv1);
	_mm256_storeu_si256((__m256i *)dest, _mm256_cvtps_epi32(vec_out));
	dest += 8;
#endif
	vofs = _mm256_add_epi32(vofs, vinc);
	}
	resrc->offset = prec_offset + (splen_t)(MM256_EXTRACT_I32(vofs, 0));
	*out_count = i;
    return dest;
}
#elif (USE_X86_EXT_INTRIN >= 3)
// offset:int32*4, resamp:float*4
// ループ内部のoffset計算をint32値域にする , (sample_increment * (req_count+1)) < int32 max
static inline DATA_T *resample_linear_int32_multi(Voice *vp, DATA_T *dest, int32 req_count, int32 *out_count)
{
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0;
	const uint32 req_count_mask = ~(0x3);
	const int32 count = req_count & req_count_mask;
	splen_t prec_offset = resrc->offset & INTEGER_MASK;
	int32 *src = (int32 *)vp->sample->data + (prec_offset >> FRACTION_BITS);
	const int32 start_offset = (int32)(resrc->offset - prec_offset); // offset計算をint32値域にする(SIMD用
	const int32 inc = resrc->increment;
	__m128i vofs = _mm_add_epi32(_mm_set1_epi32(start_offset), _mm_set_epi32(inc * 3, inc * 2, inc, 0));
	const __m128i vinc = _mm_set1_epi32(inc * 4), vfmask = _mm_set1_epi32((int32)FRACTION_MASK);
	const __m128 vec_divf = _mm_set1_ps(div_fraction);
	const __m128 vec_divo = _mm_set1_ps(DIV_31BIT);
	for(; i < count; i += 4) {
	__m128i vofsi = _mm_srli_epi32(vofs, FRACTION_BITS);
	__m128i vin1 = _mm_loadu_si128((__m128i *)&src[MM_EXTRACT_I32(vofsi,0)]); // ofsiとofsi+1をロード [v11v12v13v14]
	__m128i vin2 = _mm_loadu_si128((__m128i *)&src[MM_EXTRACT_I32(vofsi,1)]); // 次周サンプルも同じ [v21v22v23v24]
	__m128i vin3 = _mm_loadu_si128((__m128i *)&src[MM_EXTRACT_I32(vofsi,2)]); // 次周サンプルも同じ [v31v32v33v34]
	__m128i vin4 = _mm_loadu_si128((__m128i *)&src[MM_EXTRACT_I32(vofsi,3)]); // 次周サンプルも同じ [v41v42v43v44]	
	__m128 vin12 = _mm_shuffle_ps(_mm_castsi128_ps(vin1), _mm_castsi128_ps(vin2), 0x44); // [v11,v12,v21,v22]
	__m128 vin34 = _mm_shuffle_ps(_mm_castsi128_ps(vin3), _mm_castsi128_ps(vin4), 0x44); // [v31,v32,v41,v42]
	__m128 vv1 = _mm_cvtepi32_ps(_mm_castps_si128(_mm_shuffle_ps(vin12, vin34, 0x88))); // [v11,v21,v31,v41]
	__m128 vv2 = _mm_cvtepi32_ps(_mm_castps_si128(_mm_shuffle_ps(vin12, vin34, 0xDD))); // [v12,v22,v32,v42]
	__m128 vfp = _mm_mul_ps(_mm_cvtepi32_ps(_mm_and_si128(vofs, vfmask)), vec_divf);
	__m128 vec_out = MM_FMA_PS(_mm_sub_ps(vv2, vv1), vfp, vv1);
#if defined(DATA_T_DOUBLE)
	vec_out = _mm_mul_ps(vec_out, vec_divo);
#if (USE_X86_EXT_INTRIN >= 8)
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(vec_out));
	dest += 4;
#else
	_mm_storeu_pd(dest, _mm_cvtps_pd(vec_out));
	dest += 2;
	_mm_storeu_pd(dest, _mm_cvtps_pd(_mm_movehl_ps(vec_out, vec_out)));
	dest += 2;
#endif
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT
	vec_out = _mm_mul_ps(vec_out, vec_divo);
	_mm_storeu_ps(dest, vec_out);
	dest += 4;
#else // DATA_T_IN32
	_mm_storeu_si128((__m128i *)dest, _mm_cvtps_epi32(vec_out));
	dest += 4;
#endif
	vofs = _mm_add_epi32(vofs, vinc);
	}
	resrc->offset = prec_offset + (splen_t)(MM_EXTRACT_I32(vofs,0));
	*out_count = i;
    return dest;
}

#else // normal
// ループ内部のoffset計算をint32値域にする , (sample_increment * (req_count+1)) < int32 max
static inline DATA_T *resample_linear_int32_multi(Voice *vp, DATA_T *dest, int32 req_count, int32 *out_count)
{
	int32 i;
	resample_rec_t *resrc = &vp->resrc;
	splen_t prec_offset = resrc->offset & INTEGER_MASK;
	int32 *src = (int32 *)vp->sample->data + (prec_offset >> FRACTION_BITS);
	const int32 start_offset = (int32)(resrc->offset - prec_offset); // offset計算をint32値域にする(SIMD用
	int32 ofs = (int32)(resrc->offset & FRACTION_MASK);
	const int32 inc = resrc->increment;

	for(i = 0; i < req_count; i++) {	
		int32 ofsi = ofs >> FRACTION_BITS;
		int32 ofsf = ofs & FRACTION_MASK;		
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
		FLOAT_T v1 = src[ofsi], v2 = src[ofsi + 1], fp = (ofsf & FRACTION_MASK);
		*dest++ = (v1 + (v2 - v1) * fp * div_fraction) * OUT_INT32; // FLOAT_T
#else
		int32 v1 = src[ofsi], v2 = src[ofsi + 1];
		*dest++ = v1 + imuldiv_fraction_int32(v2 - v1, ofsf);
#endif
		ofs += inc;
	}
	resrc->offset = prec_offset + (splen_t)ofs;
	*out_count = i;
    return dest;
}
#endif

static void lo_rs_plain_int32(Voice *vp, DATA_T *dest, int32 count)
{
	/* Play sample until end, then free the voice. */
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0, j;

	if (resrc->increment < 0) resrc->increment = -resrc->increment; /* In case we're coming out of a bidir loop */
	j = PRECALC_LOOP_COUNT(resrc->offset, resrc->data_length, resrc->increment) + 1; // safe end+128 sample
	if (j > count) {j = count;}
	else if(j < 0) {j = 0;}	
	dest = resample_linear_int32_multi(vp, dest, j, &i);
	for(; i < j; i++) {
		*dest++ = resample_linear_int32_single(vp);
		resrc->offset += resrc->increment;
	}
	for(; i < count; i++) { *dest++ = 0; }
	if (resrc->offset >= resrc->data_length)
		vp->finish_voice = 1;
}

static void lo_rs_loop_int32(Voice *vp, DATA_T *dest, int32 count)
{
	/* Play sample until end-of-loop, skip back and continue. */
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0, j;
	
	j = PRECALC_LOOP_COUNT(resrc->offset, resrc->loop_end, resrc->increment) - 2; // 2point interpolation
	if (j > count) {j = count;}
	else if(j < 0) {j = 0;}
	dest = resample_linear_int32_multi(vp, dest, j, &i);
	for(; i < count; i++) {
		*dest++ = resample_linear_int32_single(vp);
		resrc->offset += resrc->increment;
		while(resrc->offset >= resrc->loop_end)
			resrc->offset -= resrc->loop_end - resrc->loop_start;
		/* The loop may not be longer than an increment. */
	}
}

static void lo_rs_bidir_int32(Voice *vp, DATA_T *dest, int32 count)
{
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0, j = 0;	

	if (resrc->increment > 0){
		j = PRECALC_LOOP_COUNT(resrc->offset, resrc->loop_end, resrc->increment) - 2; // 2point interpolation
		if (j > count) {j = count;}
		else if(j < 0) {j = 0;}
		dest = resample_linear_int32_multi(vp, dest, j, &i);
	}
	for(; i < count; i++) {
		*dest++ = resample_linear_int32_single(vp);
		resrc->offset += resrc->increment;
		if(resrc->increment > 0){
			if(resrc->offset >= resrc->loop_end){
				resrc->offset = (resrc->loop_end << 1) - resrc->offset;
				resrc->increment = -resrc->increment;
			}
		}else{
			if(resrc->offset <= resrc->loop_start){
				resrc->offset = (resrc->loop_start << 1) - resrc->offset;
				resrc->increment = -resrc->increment;
			}
		}
	}
}

static inline void resample_voice_linear_int32_optimize(Voice *vp, DATA_T *ptr, int32 count)
{
    int mode = vp->sample->modes;
	
	if(vp->resrc.plain_flag){ /* no loop */ /* else then loop */ 
		lo_rs_plain_int32(vp, ptr, count);	/* no loop */
	}else if(!(mode & MODES_ENVELOPE) && (vp->status & (VOICE_OFF | VOICE_DIE))){ /* no env */
		vp->resrc.plain_flag = 1; /* lock no loop */
		lo_rs_plain_int32(vp, ptr, count);	/* no loop */
	}else if(mode & MODES_RELEASE && (vp->status & VOICE_OFF)){ /* release sample */
		vp->resrc.plain_flag = 1; /* lock no loop */
		lo_rs_plain_int32(vp, ptr, count);	/* no loop */
	}else if(mode & MODES_PINGPONG){ /* Bidirectional */
		lo_rs_bidir_int32(vp, ptr, count);	/* Bidirectional loop */
	}else {
		lo_rs_loop_int32(vp, ptr, count);	/* loop */
	}		
}
#endif /* optimize linear int32 resample */

/*************** optimize linear float resample *****************/
#if defined(PRECALC_LOOPS)
#define LO_OPTIMIZE_INCREMENT

static inline DATA_T resample_linear_float_single(Voice *vp)
{	
    const float *src = (const float*)vp->sample->data;
    const fract_t ofsf = vp->resrc.offset & FRACTION_MASK;
	const spos_t ofsi = vp->resrc.offset >> FRACTION_BITS;
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
    FLOAT_T v1 = src[ofsi], v2 = src[ofsi + 1], fp = ofsf;
    return (v1 + (v2 - v1) * fp * div_fraction); // FLOAT_T
#else // DATA_T_IN32
    int32 v1 = (int32)(src[ofsi] * M_16BIT), v2 = (int32)(src[ofsi + 1] * M_16BIT);
	return v1 + imuldiv_fraction(v2 - v1, ofsf);
#endif
}

#if (USE_X86_EXT_INTRIN >= 10)
// offset:int32*16, resamp:float*16
static inline DATA_T *resample_linear_float_multi(Voice *vp, DATA_T *dest, int32 req_count, int32 *out_count)
{
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0;
	const int32 count = req_count & ~15;
	splen_t prec_offset = resrc->offset & INTEGER_MASK;
	float *src = (float *)vp->sample->data + (prec_offset >> FRACTION_BITS);
	int32 start_offset = (int32)(resrc->offset - prec_offset); // (offset計算をint32値域にする(SIMD用
	int32 inc = resrc->increment;

	__m512i vinit = _mm512_mullo_epi32(_mm512_set_epi32(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0), _mm512_set1_epi32(inc));
	__m512i vofs = _mm512_add_epi32(_mm512_set1_epi32(start_offset), vinit);
	__m512i vinc = _mm512_set1_epi32(inc * 16), vfmask = _mm512_set1_epi32((int32)FRACTION_MASK);
	__m512 vec_divo = _mm512_set1_ps(M_15BIT), vec_divf = _mm512_set1_ps(div_fraction);

#ifdef LO_OPTIMIZE_INCREMENT
#ifdef USE_PERMUTEX2
	const int32 opt_inc1 = (1 << FRACTION_BITS) * (32 - 1 - 1) / 16; // (float*16) * 1セット
#else
	const int32 opt_inc1 = (1 << FRACTION_BITS) * (16 - 1 - 1) / 16; // (float*16) * 1セット
#endif
	const __m512i vvar1 = _mm512_set1_epi32(1);
	if (inc < opt_inc1) {
		for (i = 0; i < count; i+= 16) {
			__m512i vofsi1 = _mm512_srli_epi32(vofs, FRACTION_BITS);
			__m512i vofsi2 = _mm512_add_epi32(vofsi1, vvar1);
			int32 ofs0 = _mm_cvtsi128_si32(_mm512_castsi512_si128(vofsi1));
			__m512 vin1 = _mm512_loadu_ps(&src[ofs0]); // float*16
#ifdef USE_PERMUTEX2
			__m512 vin2 = _mm512_loadu_ps(&src[ofs0 + 16]); // float*16
#endif
			__m512i vofsib = _mm512_broadcastd_epi32(_mm512_castsi512_si128(vofsi1));
			__m512i vofsub1 = _mm512_sub_epi32(vofsi1, vofsib);
			__m512i vofsub2 = _mm512_sub_epi32(vofsi2, vofsib);
			__m512 vvf1 = vin1;
#ifdef USE_PERMUTEX2
			__m512 vvf2 = vin2;
			__m512 vv1 = _mm512_permutex2var_ps(vvf1, vofsub1, vvf2); // v1 ofsi
			__m512 vv2 = _mm512_permutex2var_ps(vvf1, vofsub2, vvf2); // v2 ofsi+1
#else
			__m512 vv1 = _mm512_permutexvar_ps(vofsub1, vvf1); // v1 ofsi
			__m512 vv2 = _mm512_permutexvar_ps(vofsub2, vvf1); // v2 ofsi+1
#endif
			// あとは通常と同じ
			__m512 vfp = _mm512_mul_ps(_mm512_cvtepi32_ps(_mm512_and_epi32(vofs, vfmask)), vec_divf);
#if defined(DATA_T_DOUBLE)
			__m512 vec_out = _mm512_fmadd_ps(_mm512_sub_ps(vv2, vv1), vfp, vv1);
			_mm512_storeu_pd(dest, _mm512_cvtps_pd(_mm512_castps512_ps256(vec_out)));
			dest += 8;
			_mm512_storeu_pd(dest, _mm512_cvtps_pd(_mm512_extractf32x8_ps(vec_out, 1)));
			dest += 8;
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT 
			__m512 vec_out = _mm512_fmadd_ps(_mm512_sub_ps(vv2, vv1), vfp, vv1);
			_mm512_storeu_ps(dest, vec_out);
			dest += 16;
#else // DATA_T_IN32
			__m512 vec_out = _mm512_fmadd_ps(_mm512_sub_ps(vv2, vv1), vfp, vv1);
			_mm512_storeu_epi32((__m512i *)dest, _mm512_cvtps_epi32(_mm512_mul_ps(vec_out, vec_divo)));
			dest += 16;
#endif
			vofs = _mm512_add_epi32(vofs, vinc);
		}
	}
#endif // LO_OPTIMIZE_INCREMENT
	for (; i < count; i += 16) {
		__m512i vofsi = _mm512_srli_epi32(vofs, FRACTION_BITS);
#if 1
		__m512 vv1 = _mm512_i32gather_ps(vofsi, src, 4);
		__m512 vv2 = _mm512_i32gather_ps(_mm512_add_epi32(vofsi, _mm512_set1_epi32(1)), src, 4);
#else
		__m128 vin1 = _mm_loadu_ps(&src[MM512_EXTRACT_I32(vofsi, 0)]);
		__m128 vin2 = _mm_loadu_ps(&src[MM512_EXTRACT_I32(vofsi, 1)]);
		__m128 vin3 = _mm_loadu_ps(&src[MM512_EXTRACT_I32(vofsi, 2)]);
		__m128 vin4 = _mm_loadu_ps(&src[MM512_EXTRACT_I32(vofsi, 3)]);
		__m128 vin5 = _mm_loadu_ps(&src[MM512_EXTRACT_I32(vofsi, 4)]);
		__m128 vin6 = _mm_loadu_ps(&src[MM512_EXTRACT_I32(vofsi, 5)]);
		__m128 vin7 = _mm_loadu_ps(&src[MM512_EXTRACT_I32(vofsi, 6)]);
		__m128 vin8 = _mm_loadu_ps(&src[MM512_EXTRACT_I32(vofsi, 7)]);
		__m128 vin9 = _mm_loadu_ps(&src[MM512_EXTRACT_I32(vofsi, 8)]);
		__m128 vin10 = _mm_loadu_ps(&src[MM512_EXTRACT_I32(vofsi, 9)]);
		__m128 vin11 = _mm_loadu_ps(&src[MM512_EXTRACT_I32(vofsi, 10)]);
		__m128 vin12 = _mm_loadu_ps(&src[MM512_EXTRACT_I32(vofsi, 11)]);
		__m128 vin13 = _mm_loadu_ps(&src[MM512_EXTRACT_I32(vofsi, 12)]);
		__m128 vin14 = _mm_loadu_ps(&src[MM512_EXTRACT_I32(vofsi, 13)]);
		__m128 vin15 = _mm_loadu_ps(&src[MM512_EXTRACT_I32(vofsi, 14)]);
		__m128 vin16 = _mm_loadu_ps(&src[MM512_EXTRACT_I32(vofsi, 15)]);
		__m256 vin1_5 = _mm256_insertf32x4(_mm256_castps128_ps256(vin1), vin5, 1);
		__m256 vin2_6 = _mm256_insertf32x4(_mm256_castps128_ps256(vin2), vin6, 1);
		__m256 vin3_7 = _mm256_insertf32x4(_mm256_castps128_ps256(vin3), vin7, 1);
		__m256 vin4_8 = _mm256_insertf32x4(_mm256_castps128_ps256(vin4), vin8, 1);
		__m256 vin9_13 = _mm256_insertf32x4(_mm256_castps128_ps256(vin9), vin13, 1);
		__m256 vin10_14 = _mm256_insertf32x4(_mm256_castps128_ps256(vin10), vin14, 1);
		__m256 vin11_15 = _mm256_insertf32x4(_mm256_castps128_ps256(vin11), vin15, 1);
		__m256 vin12_16 = _mm256_insertf32x4(_mm256_castps128_ps256(vin12), vin16, 1);
		__m512 vin1_5_9_13 = _mm512_insertf32x8(_mm512_castps256_ps512(vin1_5), vin9_13, 1);
		__m512 vin2_6_10_14 = _mm512_insertf32x8(_mm512_castps256_ps512(vin2_6), vin10_14, 1);
		__m512 vin3_7_11_15 = _mm512_insertf32x8(_mm512_castps256_ps512(vin3_7), vin11_15, 1);
		__m512 vin4_8_12_16 = _mm512_insertf32x8(_mm512_castps256_ps512(vin4_8), vin12_16, 1);
		__m512 vin1_2_5_6_9_10_13_14 = _mm512_unpacklo_ps(vin1_5_9_13, vin2_6_10_14);
		__m512 vin3_4_7_8_11_12_15_16 = _mm512_unpacklo_ps(vin3_7_11_15, vin4_8_12_16);
		__m512 vv1 = _mm512_shuffle_ps(vin1_2_5_6_9_10_13_14, vin3_4_7_8_11_12_15_16, _MM_SHUFFLE(1, 0, 1, 0));
		__m512 vv2 = _mm512_shuffle_ps(vin1_2_5_6_9_10_13_14, vin3_4_7_8_11_12_15_16, _MM_SHUFFLE(3, 2, 3, 2));
#endif
		__m512 vfp = _mm512_mul_ps(_mm512_cvtepi32_ps(_mm512_and_epi32(vofs, vfmask)), vec_divf);
#if defined(DATA_T_DOUBLE)
		__m512 vec_out = _mm512_fmadd_ps(_mm512_sub_ps(vv2, vv1), vfp, vv1);
		_mm512_storeu_pd(dest, _mm512_cvtps_pd(_mm512_castps512_ps256(vec_out)));
		dest += 8;
		_mm512_storeu_pd(dest, _mm512_cvtps_pd(_mm512_extractf32x8_ps(vec_out, 1)));
		dest += 8;
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT
		__m512 vec_out = _mm512_fmadd_ps(_mm512_sub_ps(vv2, vv1), vfp, vv1);
		_mm512_storeu_ps(dest, vec_out);
		dest += 16;
#else // DATA_T_IN32
		__m512 vec_out = _mm512_fmadd_ps(_mm512_sub_ps(vv2, vv1), vfp, vv1);
		_mm512_storeu_epi32((__m512i *)dest, _mm512_cvtps_epi32(_mm512_mul_ps(vec_out, vec_divo)));
		dest += 16;
#endif
		vofs = _mm512_add_epi32(vofs, vinc);
	}
	resrc->offset = prec_offset + (splen_t)(_mm_cvtsi128_si32(_mm512_castsi512_si128(vofs)));
	*out_count = i;
	return dest;
}
#elif (USE_X86_EXT_INTRIN >= 9)
// offset:int32*8, resamp:float*8
// ループ内部のoffset計算をint32値域にする , (sample_increment * (req_count+1)) < int32 max
static inline DATA_T *resample_linear_float_multi(Voice *vp, DATA_T *dest, int32 req_count, int32 *out_count)
{
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0;
	const int32 req_count_mask = ~(0x7);
	const int32 count = req_count & req_count_mask;
	splen_t prec_offset = resrc->offset & INTEGER_MASK;
	float *src = (float *)vp->sample->data + (prec_offset >> FRACTION_BITS);
	int32 start_offset = (int32)(resrc->offset - prec_offset); // (offset計算をint32値域にする(SIMD用
	int32 inc = resrc->increment;
	__m256i vinit = _mm256_set_epi32(inc * 7, inc * 6, inc * 5, inc * 4, inc * 3, inc * 2, inc, 0);
	__m256i vofs = _mm256_add_epi32(_mm256_set1_epi32(start_offset), vinit);
	__m256i vinc = _mm256_set1_epi32(inc * 8), vfmask = _mm256_set1_epi32((int32)FRACTION_MASK);
	__m256 vec_divo = _mm256_set1_ps(M_15BIT), vec_divf = _mm256_set1_ps(div_fraction);

#ifdef LO_OPTIMIZE_INCREMENT
	// 最適化レート = (ロードデータ数 - 初期オフセット小数部の最大値(1未満) - 補間ポイント数(linearは1) ) / オフセットデータ数
	// ロードデータ数はint16用permutevarがないので変換後の32bit(int32/float)の8セットになる
	// 256bitロードデータ(int16*16セット)を全て使うにはSIMD2セットで対応
	const int32 opt_inc1 = (1 << FRACTION_BITS) * (8 - 1 - 1) / 8; // (float*8) * 1セット
	const int32 opt_inc2 = (1 << FRACTION_BITS) * (16 - 1 - 1) / 8; // (float*8) * 2セット
	const __m256i vvar1 = _mm256_set1_epi32(1);
	if(inc < opt_inc1){	// 1セット	
	for(i = 0; i < count; i += 8) {
	__m256i vofsi1 = _mm256_srli_epi32(vofs, FRACTION_BITS);
	__m256i vofsi2 = _mm256_add_epi32(vofsi1, vvar1);
	int32 ofs0 = _mm_cvtsi128_si32(_mm256_castsi256_si128(vofsi1));
	__m256 vin1 = _mm256_loadu_ps(&src[ofs0]); // float*8
	__m256i vofsib = _mm256_broadcastd_epi32(_mm256_castsi256_si128(vofsi1));
	__m256i vofsub1 = _mm256_sub_epi32(vofsi1, vofsib); 
	__m256i vofsub2 = _mm256_sub_epi32(vofsi2, vofsib); 
	__m256 vvf1 = vin1;
	__m256 vv1 = _mm256_permutevar8x32_ps(vvf1, vofsub1); // v1 ofsi
	__m256 vv2 = _mm256_permutevar8x32_ps(vvf1, vofsub2); // v2 ofsi+1
	// あとは通常と同じ
	__m256 vfp = _mm256_mul_ps(_mm256_cvtepi32_ps(_mm256_and_si256(vofs, vfmask)), vec_divf);
#if defined(DATA_T_DOUBLE)
	__m256 vec_out = MM256_FMA_PS(_mm256_sub_ps(vv2, vv1), vfp, vv1);
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(_mm256_castps256_ps128(vec_out)));
	dest += 4;
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(_mm256_extractf128_ps(vec_out, 1)));	
	dest += 4;
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT 
	__m256 vec_out = MM256_FMA_PS(_mm256_sub_ps(vv2, vv1), vfp, vv1);
	_mm256_storeu_ps(dest, vec_out);
	dest += 8;
#else // DATA_T_IN32
	__m256 vec_out = MM256_FMA_PS(_mm256_sub_ps(vv2, vv1), vfp, vv1);
	_mm256_storeu_si256((__m256i *)dest, _mm256_cvtps_epi32(_mm256_mul_ps(vec_out, vec_divo)));
	dest += 8;
#endif
	vofs = _mm256_add_epi32(vofs, vinc);
	}
	}else
#if 0 // 2set
	if(inc < opt_inc2){ // 2セット
	const __m256i vvar7 = _mm256_set1_epi32(7);
	for(i = 0; i < count; i += 8) {
	__m256i vofsi1 = _mm256_srli_epi32(vofs, FRACTION_BITS); // ofsi
	__m256i vofsi2 = _mm256_add_epi32(vofsi1, vadd1); // ofsi+1
	int32 ofs0 = _mm_extract_epi32(_mm256_extracti128si256(vofsi1, 0x0), 0x0);
	__m256i vin1 = _mm256_loadu_si256((__m256i *)&src[ofs0]); // int16*16
	__m256i vin2 = _mm256_permutevar8x32_epi32(vin1, _mm256_set_epi32(3,2,1,0,7,6,5,4)); // H128bitをL128bitに移動
	__m256 vvf1 = _mm256_cvtepi32_ps(_mm256_cvtepi16_epi32(vin1)); // int16 to float (float変換でH128bitは消える
	__m256 vvf2 = _mm256_cvtepi32_ps(_mm256_cvtepi16_epi32(vin2)); // int16 to float (float変換でH128bitは消える	
	__m256i vofsib = _mm256_permutevar8x32_epi32(vofsi, _mm256_setzero_epi32()); // ofsi[0]
	__m256i vofsub1 = _mm256_sub_epi32(vofsi1, vofsib); // v1 ofsi
	__m256i vofsub2 = _mm256_sub_epi32(vofsi2, vofsib); // v2 ofsi+1
	__m256i vrm1 = _mm256_cmpgt_epi32(vofsub1, vvar7); // オフセット差が8以上の条件でマスク作成
	__m256i vrm2 = _mm256_cmpgt_epi32(vofsub2, vvar7); // オフセット差が8以上の条件でマスク作成
	// src2 offsetが下位3bitのみ有効であれば8を超える部分にマスク不要のはず
	__m256 vv11 = _mm256_permutevar8x32_ps(vvf1, vofsub1);
	__m256 vv12 = _mm256_permutevar8x32_ps(vvf2, vofsub1);
	__m256 vv21 = _mm256_permutevar8x32_ps(vvf1, vofsub2);
	__m256 vv22 = _mm256_permutevar8x32_ps(vvf2, vofsub2);	
	__m256 vv1 = _mm256_blendv_ps(vv11, vv12, vrm1); // v1 ofsi
	__m256 vv2 = _mm256_blendv_ps(vv21, vv22, vrm2); // v2 ofsi+1
	// あとは通常と同じ
	__m256 vfp = _mm256_mul_ps(_mm256_cvtepi32_ps(_mm256_and_si256(vofs, vfmask)), vec_divf);
#if defined(DATA_T_DOUBLE)
	__m256 vec_out = MM256_FMA_PS(_mm256_sub_ps(vv2, vv1), vfp, vv1);
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(_mm256_extractf128_ps(vec_out, 0)));
	dest += 4;
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(_mm256_extractf128_ps(vec_out, 1)));	
	dest += 4;
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT
	__m256 vec_out = MM256_FMA_PS(_mm256_sub_ps(vv2, vv1), vfp, vv1);
	_mm256_storeu_ps(dest, vec_out);
	dest += 8;
#else // DATA_T_IN32
	__m256 vec_out = MM256_FMA_PS(_mm256_sub_ps(vv2, vv1), vfp, vv1);
	_mm256_storeu_si256((__m256i *)dest, _mm256_cvtps_epi32(_mm256_mul_ps(vec_out, vec_divo)));
	dest += 8;
#endif
	vofs = _mm256_add_epi32(vofs, vinc);
	}
	}else
#endif // 2set
#endif // LO_OPTIMIZE_INCREMENT

	for(; i < count; i += 8) {
	__m256i vofsi = _mm256_srli_epi32(vofs, FRACTION_BITS);
#if 0
	__m256 vv1 = _mm256_i32gather_ps(src, vofsi, 4);
	__m256 vv2 = _mm256_i32gather_ps(src, _mm256_add_epi32(vofsi, _mm256_set1_epi32(1)), 4);
#else
	__m128 vin1 = _mm_loadu_ps(&src[MM256_EXTRACT_I32(vofsi,0)]); // ofsiとofsi+1をロード
	__m128 vin2 = _mm_loadu_ps(&src[MM256_EXTRACT_I32(vofsi,1)]); // 次周サンプルも同じ
	__m128 vin3 = _mm_loadu_ps(&src[MM256_EXTRACT_I32(vofsi,2)]); // 次周サンプルも同じ
	__m128 vin4 = _mm_loadu_ps(&src[MM256_EXTRACT_I32(vofsi,3)]); // 次周サンプルも同じ
	__m128 vin5 = _mm_loadu_ps(&src[MM256_EXTRACT_I32(vofsi,4)]); // 次周サンプルも同じ
	__m128 vin6 = _mm_loadu_ps(&src[MM256_EXTRACT_I32(vofsi,5)]); // 次周サンプルも同じ
	__m128 vin7 = _mm_loadu_ps(&src[MM256_EXTRACT_I32(vofsi,6)]); // 次周サンプルも同じ
	__m128 vin8 = _mm_loadu_ps(&src[MM256_EXTRACT_I32(vofsi,7)]); // 次周サンプルも同じ
	__m256 vin15 = _mm256_insertf128_ps(_mm256_castps128_ps256(vin1), vin5, 1);
	__m256 vin26 = _mm256_insertf128_ps(_mm256_castps128_ps256(vin2), vin6, 1);
	__m256 vin37 = _mm256_insertf128_ps(_mm256_castps128_ps256(vin3), vin7, 1);
	__m256 vin48 = _mm256_insertf128_ps(_mm256_castps128_ps256(vin4), vin8, 1);
	__m256 vin1256 = _mm256_unpacklo_ps(vin15, vin26);
	__m256 vin3478 = _mm256_unpacklo_ps(vin37, vin48);
	__m256 vv1 = _mm256_shuffle_ps(vin1256, vin3478, _MM_SHUFFLE(1, 0, 1, 0));
	__m256 vv2 = _mm256_shuffle_ps(vin1256, vin3478, _MM_SHUFFLE(3, 2, 3, 2));
#endif
	__m256 vfp = _mm256_mul_ps(_mm256_cvtepi32_ps(_mm256_and_si256(vofs, vfmask)), vec_divf);
#if defined(DATA_T_DOUBLE)
	__m256 vec_out = MM256_FMA_PS(_mm256_sub_ps(vv2, vv1), vfp, vv1);
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(_mm256_castps256_ps128(vec_out)));
	dest += 4;
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(_mm256_extractf128_ps(vec_out, 1)));	
	dest += 4;
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT
	__m256 vec_out = MM256_FMA_PS(_mm256_sub_ps(vv2, vv1), vfp, vv1);
	_mm256_storeu_ps(dest, vec_out);
	dest += 8;
#else // DATA_T_IN32
	__m256 vec_out = MM256_FMA_PS(_mm256_sub_ps(vv2, vv1), vfp, vv1);
	_mm256_storeu_si256((__m256i *)dest, _mm256_cvtps_epi32(_mm256_mul_ps(vec_out, vec_divo)));
	dest += 8;
#endif
	vofs = _mm256_add_epi32(vofs, vinc);
	}
	resrc->offset = prec_offset + (splen_t)(MM256_EXTRACT_I32(vofs, 0));
	*out_count = i;
    return dest;
}
#elif (USE_X86_EXT_INTRIN >= 3)
// offset:int32*4, resamp:float*4
// ループ内部のoffset計算をint32値域にする , (sample_increment * (req_count+1)) < int32 max
static inline DATA_T *resample_linear_float_multi(Voice *vp, DATA_T *dest, int32 req_count, int32 *out_count)
{
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0;
	const uint32 req_count_mask = ~(0x3);
	const int32 count = req_count & req_count_mask;
	splen_t prec_offset = resrc->offset & INTEGER_MASK;
	float *src = (float *)vp->sample->data + (prec_offset >> FRACTION_BITS);
	const int32 start_offset = (int32)(resrc->offset - prec_offset); // offset計算をint32値域にする(SIMD用
	const int32 inc = resrc->increment;
	__m128i vofs = _mm_add_epi32(_mm_set1_epi32(start_offset), _mm_set_epi32(inc * 3, inc * 2, inc, 0));
	const __m128i vinc = _mm_set1_epi32(inc * 4), vfmask = _mm_set1_epi32((int32)FRACTION_MASK);
	const __m128 vec_divf = _mm_set1_ps(div_fraction);
	const __m128 vec_divo = _mm_set1_ps(M_15BIT);
	for(; i < count; i += 4) {
	__m128i vofsi = _mm_srli_epi32(vofs, FRACTION_BITS);
	__m128 vin1 = _mm_loadu_ps(&src[MM_EXTRACT_I32(vofsi,0)]); // ofsiとofsi+1をロード [v11v12v13v14]
	__m128 vin2 = _mm_loadu_ps(&src[MM_EXTRACT_I32(vofsi,1)]); // 次周サンプルも同じ [v21v22v23v24]
	__m128 vin3 = _mm_loadu_ps(&src[MM_EXTRACT_I32(vofsi,2)]); // 次周サンプルも同じ [v31v32v33v34]
	__m128 vin4 = _mm_loadu_ps(&src[MM_EXTRACT_I32(vofsi,3)]); // 次周サンプルも同じ [v41v42v43v44]	
    __m128 vin12 = _mm_shuffle_ps(vin1, vin2, 0x44); // [v11,v12,v21,v22]
    __m128 vin34 = _mm_shuffle_ps(vin3, vin4, 0x44); // [v31,v32,v41,v42]
    __m128 vv1 = _mm_shuffle_ps(vin12, vin34, 0x88); // [v11,v21,v31,v41]
    __m128 vv2 = _mm_shuffle_ps(vin12, vin34, 0xDD); // [v12,v22,v32,v42]
	__m128 vfp = _mm_mul_ps(_mm_cvtepi32_ps(_mm_and_si128(vofs, vfmask)), vec_divf);
	__m128 vec_out = MM_FMA_PS(_mm_sub_ps(vv2, vv1), vfp, vv1);
#if defined(DATA_T_DOUBLE)
#if (USE_X86_EXT_INTRIN >= 8)
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(vec_out));
	dest += 4;
#else
	_mm_storeu_pd(dest, _mm_cvtps_pd(vec_out));
	dest += 2;
	_mm_storeu_pd(dest, _mm_cvtps_pd(_mm_movehl_ps(vec_out, vec_out)));
	dest += 2;
#endif
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT
	_mm_storeu_ps(dest, vec_out);
	dest += 4;
#else // DATA_T_IN32
	_mm_storeu_si128((__m128i *)dest, _mm_cvtps_epi32(_mm_mul_ps(vec_out, vec_divo)));
	dest += 4;
#endif
	vofs = _mm_add_epi32(vofs, vinc);
	}
	resrc->offset = prec_offset + (splen_t)(MM_EXTRACT_I32(vofs,0));
	*out_count = i;
    return dest;
}

#else // normal
// ループ内部のoffset計算をint32値域にする , (sample_increment * (req_count+1)) < int32 max
static inline DATA_T *resample_linear_float_multi(Voice *vp, DATA_T *dest, int32 req_count, int32 *out_count)
{
	int32 i;
	resample_rec_t *resrc = &vp->resrc;
	splen_t prec_offset = resrc->offset & INTEGER_MASK;
	float *src = (float *)vp->sample->data + (prec_offset >> FRACTION_BITS);
	const int32 start_offset = (int32)(resrc->offset - prec_offset); // offset計算をint32値域にする(SIMD用
	int32 ofs = (int32)(resrc->offset & FRACTION_MASK);
	const int32 inc = resrc->increment;

	for(i = 0; i < req_count; i++) {	
		int32 ofsi = ofs >> FRACTION_BITS;
		int32 ofsf = ofs & FRACTION_MASK;		
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
		FLOAT_T v1 = src[ofsi], v2 = src[ofsi + 1], fp = (ofsf & FRACTION_MASK);
		*dest++ = (v1 + (v2 - v1) * fp * div_fraction); // FLOAT_T
#else
		int32 v1 = (int32)(src[ofsi] * M_16BIT), v2 = (int32)(src[ofsi + 1] * M_16BIT);
		*dest++ = v1 + imuldiv_fraction(v2 - v1, ofsf);
#endif
		ofs += inc;
	}
	resrc->offset = prec_offset + (splen_t)ofs;
	*out_count = i;
    return dest;
}
#endif

static void lo_rs_plain_float(Voice *vp, DATA_T *dest, int32 count)
{
	/* Play sample until end, then free the voice. */
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0, j;

	if (resrc->increment < 0) resrc->increment = -resrc->increment; /* In case we're coming out of a bidir loop */
	j = PRECALC_LOOP_COUNT(resrc->offset, resrc->data_length, resrc->increment) + 1; // safe end+128 sample
	if (j > count) {j = count;}
	else if(j < 0) {j = 0;}	
	dest = resample_linear_float_multi(vp, dest, j, &i);
	for(; i < j; i++) {
		*dest++ = resample_linear_float_single(vp);
		resrc->offset += resrc->increment;
	}
	for(; i < count; i++) { *dest++ = 0; }
	if (resrc->offset >= resrc->data_length)
		vp->finish_voice = 1;
}

static void lo_rs_loop_float(Voice *vp, DATA_T *dest, int32 count)
{
	/* Play sample until end-of-loop, skip back and continue. */
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0, j;
	
	j = PRECALC_LOOP_COUNT(resrc->offset, resrc->loop_end, resrc->increment) - 2; // 2point interpolation
	if (j > count) {j = count;}
	else if(j < 0) {j = 0;}
	dest = resample_linear_float_multi(vp, dest, j, &i);
	for(; i < count; i++) {
		*dest++ = resample_linear_float_single(vp);
		resrc->offset += resrc->increment;
		while(resrc->offset >= resrc->loop_end)
			resrc->offset -= resrc->loop_end - resrc->loop_start;
		/* The loop may not be longer than an increment. */
	}
}

static void lo_rs_bidir_float(Voice *vp, DATA_T *dest, int32 count)
{
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0, j = 0;	

	if (resrc->increment > 0){
		j = PRECALC_LOOP_COUNT(resrc->offset, resrc->loop_end, resrc->increment) - 2; // 2point interpolation
		if (j > count) {j = count;}
		else if(j < 0) {j = 0;}
		dest = resample_linear_float_multi(vp, dest, j, &i);
	}
	for(; i < count; i++) {
		*dest++ = resample_linear_float_single(vp);
		resrc->offset += resrc->increment;
		if(resrc->increment > 0){
			if(resrc->offset >= resrc->loop_end){
				resrc->offset = (resrc->loop_end << 1) - resrc->offset;
				resrc->increment = -resrc->increment;
			}
		}else{
			if(resrc->offset <= resrc->loop_start){
				resrc->offset = (resrc->loop_start << 1) - resrc->offset;
				resrc->increment = -resrc->increment;
			}
		}
	}
}

static inline void resample_voice_linear_float_optimize(Voice *vp, DATA_T *ptr, int32 count)
{
    int mode = vp->sample->modes;
	
	if(vp->resrc.plain_flag){ /* no loop */ /* else then loop */ 
		lo_rs_plain_float(vp, ptr, count);	/* no loop */
	}else if(!(mode & MODES_ENVELOPE) && (vp->status & (VOICE_OFF | VOICE_DIE))){ /* no env */
		vp->resrc.plain_flag = 1; /* lock no loop */
		lo_rs_plain_float(vp, ptr, count);	/* no loop */
	}else if(mode & MODES_RELEASE && (vp->status & VOICE_OFF)){ /* release sample */
		vp->resrc.plain_flag = 1; /* lock no loop */
		lo_rs_plain_float(vp, ptr, count);	/* no loop */
	}else if(mode & MODES_PINGPONG){ /* Bidirectional */
		lo_rs_bidir_float(vp, ptr, count);	/* Bidirectional loop */
	}else {
		lo_rs_loop_float(vp, ptr, count);	/* loop */
	}		
}
#endif /* optimize linear float resample */

/*************** optimize lagrange resample ***********************/
#if defined(PRECALC_LOOPS)
#define LAO_OPTIMIZE_INCREMENT

#if 0 // timidity41-eddb86e
#if USE_X86_EXT_INTRIN >= 8

// caller must check offsets to ensure lagrange interpolation is applicable
// TODO: use newton interpolation
static DATA_T *resample_multi_lagrange_m256(Voice *vp, DATA_T *dest, int32 *i, int32 count)
{
	resample_rec_t *resrc = &vp->resrc;
	spos_t ofsls = resrc->loop_start >> FRACTION_BITS;
	spos_t ofsle = resrc->loop_end >> FRACTION_BITS;
	spos_t ofsend = resrc->data_length >> FRACTION_BITS;

	splen_t prec_offset = (resrc->offset & INTEGER_MASK) - (1 << FRACTION_BITS);
	sample_t *src = vp->sample->data + (prec_offset >> FRACTION_BITS);
	int32 start_offset = (int32)(resrc->offset - prec_offset); // (offset計算をint32値域にする(SIMD用

	__m256i vindices = _mm256_set_epi32(7, 6, 5, 4, 3, 2, 1, 0);
	__m256i vofs = _mm256_add_epi32(_mm256_set1_epi32(start_offset), _mm256_mullo_epi32(vindices, _mm256_set1_epi32(resrc->increment)));
	__m256i vofsi = _mm256_srai_epi32(vofs, FRACTION_BITS);

	// src[ofsi-1], src[ofsi]
	__m256i vinm10 = MM256_I32GATHER_I32((const int *)src, _mm256_sub_epi32(vofsi, _mm256_set1_epi32(1)), 2);
	// src[ofsi+1], src[ofsi+2]
	__m256i vin12 = MM256_I32GATHER_I32((const int *)src, _mm256_add_epi32(vofsi, _mm256_set1_epi32(1)), 2);

	// (int32)src[ofsi-1]
	__m256i vinm1 = _mm256_srai_epi32(_mm256_slli_epi32(vinm10, 16), 16);
	// (int32)src[ofsi]
	__m256i vin0 = _mm256_srai_epi32(vinm10, 16);
	// (int32)src[ofsi+1]
	__m256i vin1 = _mm256_srai_epi32(_mm256_slli_epi32(vin12, 16), 16);
	// (int32)src[ofsi+2]
	__m256i vin2 = _mm256_srai_epi32(vin12, 16);

	__m256 vec_divf = _mm256_set1_ps(div_fraction);

	// (float)(ofs - ofsi)
	__m256 vfofsf = _mm256_mul_ps(_mm256_cvtepi32_ps(_mm256_and_si256(vofs, _mm256_set1_epi32(FRACTION_MASK))), vec_divf);

	// (float)(int32)src[ofsi-1]
	__m256 vfinm1 = _mm256_cvtepi32_ps(vinm1);
	// (float)(int32)src[ofsi]
	__m256 vfin0 = _mm256_cvtepi32_ps(vin0);
	// (float)(int32)src[ofsi+1]
	__m256 vfin1 = _mm256_cvtepi32_ps(vin1);
	// (float)(int32)src[ofsi+2]
	__m256 vfin2 = _mm256_cvtepi32_ps(vin2);

	__m256 v1 = _mm256_set1_ps(1.0f);

	// x - x1
	__m256 vfofsfm1 = _mm256_add_ps(vfofsf, v1);
	// x - x2
	// __m256 vfofsf0 = vfofsf;

	// x - x3
	__m256 vfofsf1 = _mm256_sub_ps(vfofsf, v1);
	// x - x4
	__m256 vfofsf2 = _mm256_sub_ps(vfofsf1, v1);

	//   (x - x2)(x - x3)(x - x4) / (x1 - x2)(x1 - x3)(x1 - x4)
	// = (x - x2)(x - x3)(x - x4) * (-1/6)
	__m256 vfcoefm1 = _mm256_mul_ps(_mm256_mul_ps(vfofsf, vfofsf1), _mm256_mul_ps(vfofsf2, _mm256_set1_ps(-1.0f / 6.0f)));

	//   (x - x1)(x - x3)(x - x4) / (x2 - x1)(x2 - x3)(x2 - x4)
	// = (x - x1)(x - x3)(x - x4) * (1/2)
	__m256 vfcoef0 = _mm256_mul_ps(_mm256_mul_ps(vfofsfm1, vfofsf1), _mm256_mul_ps(vfofsf2, _mm256_set1_ps(1.0f / 2.0f)));

	//   (x - x1)(x - x2)(x - x4) / (x3 - x1)(x3 - x2)(x3 - x4)
	// = (x - x1)(x - x2)(x - x4) * (-1/2)
	__m256 vfcoef1 = _mm256_mul_ps(_mm256_mul_ps(vfofsfm1, vfofsf), _mm256_mul_ps(vfofsf2, _mm256_set1_ps(-1.0f / 2.0f)));

	//   (x - x1)(x - x2)(x - x3) / (x4 - x1)(x4 - x2)(x4 - x3)
	// = (x - x1)(x - x2)(x - x3) * (1/6)
	__m256 vfcoef2 = _mm256_mul_ps(_mm256_mul_ps(vfofsfm1, vfofsf), _mm256_mul_ps(vfofsf1, _mm256_set1_ps(1.0f / 6.0f)));

#if USE_X86_EXT_INTRIN >= 9
	__m256 vresult = _mm256_add_ps(
		_mm256_fmadd_ps(vfinm1, vfcoefm1, _mm256_mul_ps(vfin0, vfcoef0)),
		_mm256_fmadd_ps(vfin1, vfcoef1, _mm256_mul_ps(vfin2, vfcoef2))
	);
#else
	__m256 vresult = _mm256_add_ps(
		_mm256_add_ps(_mm256_mul_ps(vfinm1, vfcoefm1), _mm256_mul_ps(vfin0, vfcoef0)),
		_mm256_add_ps(_mm256_mul_ps(vfin1, vfcoef1), _mm256_mul_ps(vfin2, vfcoef2))
	);
#endif

#if defined(DATA_T_DOUBLE)
	vresult = _mm256_mul_ps(vresult, _mm256_set1_ps(OUT_INT16));
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(_mm256_extractf128_ps(vresult, 0)));
	_mm256_storeu_pd(dest + 4, _mm256_cvtps_pd(_mm256_extractf128_ps(vresult, 1)));
#elif defined(DATA_T_FLOAT)
	vresult = _mm256_mul_ps(vresult, _mm256_set1_ps(OUT_INT16));
	_mm256_storeu_ps(dest, vresult);
#else
	_mm256_storeu_si256(dest, _mm256_cvtps_epi32(vresult));
#endif

	dest += 8;
	resrc->offset += resrc->increment * 8;
	*i += 8;
	return dest;
}

#endif

#if USE_X86_EXT_INTRIN >= 6

// caller must check offsets to ensure lagrange interpolation is applicable
// TODO: use newton interpolation
static DATA_T *resample_multi_lagrange_m128(Voice *vp, DATA_T *dest, int32 *i, int32 count)
{
	resample_rec_t *resrc = &vp->resrc;
	spos_t ofsls = resrc->loop_start >> FRACTION_BITS;
	spos_t ofsle = resrc->loop_end >> FRACTION_BITS;
	spos_t ofsend = resrc->data_length >> FRACTION_BITS;

	splen_t prec_offset = (resrc->offset & INTEGER_MASK) - (1 << FRACTION_BITS);
	sample_t *src = vp->sample->data + (prec_offset >> FRACTION_BITS);
	int32 start_offset = (int32)(resrc->offset - prec_offset); // (offset計算をint32値域にする(SIMD用

	__m128i vindices = _mm_set_epi32(3, 2, 1, 0);
	__m128i vofs = _mm_add_epi32(_mm_set1_epi32(start_offset), _mm_mullo_epi32(vindices, _mm_set1_epi32(resrc->increment)));
	__m128i vofsi = _mm_srai_epi32(vofs, FRACTION_BITS);

	// src[ofsi-1], src[ofsi]
	__m128i vinm10 = MM_I32GATHER_I32((const int *)src, _mm_sub_epi32(vofsi, _mm_set1_epi32(1)), 2);
	// src[ofsi+1], src[ofsi+2]
	__m128i vin12 = MM_I32GATHER_I32((const int *)src, _mm_add_epi32(vofsi, _mm_set1_epi32(1)), 2);

	// (int32)src[ofsi-1]
	__m128i vinm1 = _mm_srai_epi32(_mm_slli_epi32(vinm10, 16), 16);
	// (int32)src[ofsi]
	__m128i vin0 = _mm_srai_epi32(vinm10, 16);
	// (int32)src[ofsi+1]
	__m128i vin1 = _mm_srai_epi32(_mm_slli_epi32(vin12, 16), 16);
	// (int32)src[ofsi+2]
	__m128i vin2 = _mm_srai_epi32(vin12, 16);

	__m128 vec_divf = _mm_set1_ps(div_fraction);

	// (float)(ofs - ofsi)
	__m128 vfofsf = _mm_mul_ps(_mm_cvtepi32_ps(_mm_and_si128(vofs, _mm_set1_epi32(FRACTION_MASK))), vec_divf);

	// (float)(int32)src[ofsi-1]
	__m128 vfinm1 = _mm_cvtepi32_ps(vinm1);
	// (float)(int32)src[ofsi]
	__m128 vfin0 = _mm_cvtepi32_ps(vin0);
	// (float)(int32)src[ofsi+1]
	__m128 vfin1 = _mm_cvtepi32_ps(vin1);
	// (float)(int32)src[ofsi+2]
	__m128 vfin2 = _mm_cvtepi32_ps(vin2);

	__m128 v1 = _mm_set1_ps(1.0f);

	// x - x1
	__m128 vfofsfm1 = _mm_add_ps(vfofsf, v1);
	// x - x2
	// __m128 vfofsf0 = vfofsf;

	// x - x3
	__m128 vfofsf1 = _mm_sub_ps(vfofsf, v1);
	// x - x4
	__m128 vfofsf2 = _mm_sub_ps(vfofsf1, v1);

	//   (x - x2)(x - x3)(x - x4) / (x1 - x2)(x1 - x3)(x1 - x4)
	// = (x - x2)(x - x3)(x - x4) * (-1/6)
	__m128 vfcoefm1 = _mm_mul_ps(_mm_mul_ps(vfofsf, vfofsf1), _mm_mul_ps(vfofsf2, _mm_set1_ps(-1.0f / 6.0f)));

	//   (x - x1)(x - x3)(x - x4) / (x2 - x1)(x2 - x3)(x2 - x4)
	// = (x - x1)(x - x3)(x - x4) * (1/2)
	__m128 vfcoef0 = _mm_mul_ps(_mm_mul_ps(vfofsfm1, vfofsf1), _mm_mul_ps(vfofsf2, _mm_set1_ps(1.0f / 2.0f)));

	//   (x - x1)(x - x2)(x - x4) / (x3 - x1)(x3 - x2)(x3 - x4)
	// = (x - x1)(x - x2)(x - x4) * (-1/2)
	__m128 vfcoef1 = _mm_mul_ps(_mm_mul_ps(vfofsfm1, vfofsf), _mm_mul_ps(vfofsf2, _mm_set1_ps(-1.0f / 2.0f)));

	//   (x - x1)(x - x2)(x - x3) / (x4 - x1)(x4 - x2)(x4 - x3)
	// = (x - x1)(x - x2)(x - x3) * (1/6)
	__m128 vfcoef2 = _mm_mul_ps(_mm_mul_ps(vfofsfm1, vfofsf), _mm_mul_ps(vfofsf1, _mm_set1_ps(1.0f / 6.0f)));

#if USE_X86_EXT_INTRIN >= 9
	__m128 vresult = _mm_add_ps(
		_mm_fmadd_ps(vfinm1, vfcoefm1, _mm_mul_ps(vfin0, vfcoef0)),
		_mm_fmadd_ps(vfin1, vfcoef1, _mm_mul_ps(vfin2, vfcoef2))
	);
#else
	__m128 vresult = _mm_add_ps(
		_mm_add_ps(_mm_mul_ps(vfinm1, vfcoefm1), _mm_mul_ps(vfin0, vfcoef0)),
		_mm_add_ps(_mm_mul_ps(vfin1, vfcoef1), _mm_mul_ps(vfin2, vfcoef2))
	);
#endif

#if defined(DATA_T_DOUBLE)
	vresult = _mm_mul_ps(vresult, _mm_set1_ps(OUT_INT16));
	_mm_storeu_pd(dest, _mm_cvtps_pd(vresult));
	_mm_storeu_pd(dest + 2, _mm_cvtps_pd(_mm_movehl_ps(vresult, vresult)));
#elif defined(DATA_T_FLOAT)
	vresult = _mm_mul_ps(vresult, _mm_set1_ps(OUT_INT16));
	_mm_storeu_ps(dest, vresult);
#else
	_mm_storeu_si128(dest, _mm_cvtps_epi32(vresult));
#endif

	dest += 4;
	resrc->offset += resrc->increment * 4;
	*i += 4;
	return dest;
}

#endif

static void resample_lagrange_multi2(Voice *vp, DATA_T *dest, int32 count)
{
	const sample_t *src = vp->sample->data;
	resample_rec_t *resrc = &vp->resrc;
	spos_t ofsls = resrc->loop_start >> FRACTION_BITS;
	spos_t ofsle = resrc->loop_end >> FRACTION_BITS;
	spos_t ofsend = resrc->data_length >> FRACTION_BITS;
	int32 i = 0;

	if (resrc->mode == RESAMPLE_MODE_PLAIN) {
		if (resrc->increment < 0) {
			resrc->increment = -resrc->increment;
		}

		// interpolate [0, 1] linearly
		while (i < count && (resrc->offset >> FRACTION_BITS) < 1) {
			*dest++ = resample_linear(src, resrc->offset, resrc);
			resrc->offset += resrc->increment;
			i++;
		}

		// lagrange interpolation
#if USE_X86_EXT_INTRIN >= 8
		while (count - i >= 8) {
			// !(ofsi + 2 < ofsend)
			if (((resrc->offset + resrc->increment * 7) >> FRACTION_BITS) + 2 >= ofsend) {
				break;
			}

			dest = resample_multi_lagrange_m256(vp, dest, &i, count);
		}
#endif

#if USE_X86_EXT_INTRIN >= 6
		while (count - i >= 4) {
			// !(ofsi + 2 < ofsend)
			if (((resrc->offset + resrc->increment * 3) >> FRACTION_BITS) + 2 >= ofsend) {
				break;
			}

			dest = resample_multi_lagrange_m128(vp, dest, &i, count);
		}
#endif

		while (i < count && (resrc->offset >> FRACTION_BITS) + 2 < ofsend) {
			*dest++ = resample_lagrange(src, resrc->offset, resrc);
			resrc->offset += resrc->increment;
			i++;
		}

		// interpolate [ofsend - 2, ofsend - 1] linearly
		while (i < count && (resrc->offset >> FRACTION_BITS) < 1) {
			*dest++ = resample_linear(src, resrc->offset, resrc);
			resrc->offset += resrc->increment;
			i++;
		}

		if (i < count) {
			memset(dest, 0, (count - i) * sizeof(DATA_T));
			resrc->offset += resrc->increment * (count - i);
			vp->finish_voice = 1;
		}
	} else {
		while (i < count) {
			// interpolate [0, 1] linearly
			while (i < count && (resrc->offset >> FRACTION_BITS) < 1) {
				*dest++ = resample_linear(src, resrc->offset, resrc);
				resrc->offset += resrc->increment;
				i++;
			}

#if USE_X86_EXT_INTRIN >= 8
			while (count - i >= 8) {
				spos_t ofs0i = resrc->offset >> FRACTION_BITS;
				spos_t ofs7i = (resrc->offset + resrc->increment * 7) >> FRACTION_BITS;

				if (resrc->increment > 0 ? ofsle <= ofs7i + 2 : ofs7i - 1 < ofsls || ofsle <= ofs0i + 2) {
					break;
				}

				dest = resample_multi_lagrange_m256(vp, dest, &i, count);
			}
#endif

#if USE_X86_EXT_INTRIN >= 6
			while (count - i >= 4) {
				spos_t ofs0i = resrc->offset >> FRACTION_BITS;
				spos_t ofs3i = (resrc->offset + resrc->increment * 3) >> FRACTION_BITS;

				if (resrc->increment > 0 ? ofsle <= ofs3i + 2 : ofs3i - 1 < ofsls || ofsle <= ofs0i + 2) {
					break;
				}

				dest = resample_multi_lagrange_m128(vp, dest, &i, count);
			}
#endif

			while (i < count) {
				spos_t ofsi = resrc->offset >> FRACTION_BITS;

				if (resrc->increment > 0 ? ofsle <= ofsi + 2 : ofsi - 1 < ofsls || ofsle <= ofsi + 2) {
					break;
				}

				*dest++ = resample_lagrange(src, resrc->offset, resrc);
				resrc->offset += resrc->increment;
				i++;
			}

			while (i < count) {
				spos_t ofsi = resrc->offset >> FRACTION_BITS;

				if (resrc->increment > 0 ? ofsi + 2 < ofsle : ofsls <= ofsi - 1 && ofsi + 2 < ofsle) {
					break;
				}

				*dest++ = resample_lagrange(src, resrc->offset, resrc);
				resrc->offset += resrc->increment;
				i++;

				if (resrc->loop_end < resrc->offset) {
					if (resrc->mode == RESAMPLE_MODE_LOOP) {
						resrc->offset -= resrc->loop_end - resrc->loop_start;
					} else if (resrc->mode == RESAMPLE_MODE_BIDIR_LOOP && resrc->increment > 0) {
						resrc->increment = -resrc->increment;
					}
				} else if (resrc->mode == RESAMPLE_MODE_BIDIR_LOOP && resrc->increment < 0 && resrc->offset < resrc->loop_start) {
					resrc->increment = -resrc->increment;
				}
			}
		}
	}
}
#endif // timidity41-eddb86e

static inline DATA_T resample_lagrange_single(Voice *vp)
{		
	sample_t *src = vp->sample->data;
	const resample_rec_t *resrc = &vp->resrc;
    fract_t ofsf = resrc->offset & FRACTION_MASK;
    const spos_t ofsls = resrc->loop_start >> FRACTION_BITS;
    const spos_t ofsle = resrc->loop_end >> FRACTION_BITS;
	const spos_t ofsi = resrc->offset >> FRACTION_BITS;
    spos_t ofstmp, len;
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
    FLOAT_T v[4], tmp;
#else // DATA_T_IN32
	int32 v[4], tmp;
#endif
	int32 i, dir;

	switch(resrc->mode){
	case RESAMPLE_MODE_PLAIN:
		if(ofsi < 1)
			goto do_linear;
		break; // normal
	case RESAMPLE_MODE_LOOP:
		if(ofsi < ofsls){
			if(ofsi < 1)
				goto do_linear;
			if((ofsi + 2) < ofsle)
				break; // normal
		}else if(((ofsi + 2) < ofsle) && ((ofsi - 1) >= ofsls))
			break; // normal		
		len = ofsle - ofsls; // loop_length
		ofstmp = ofsi - 1;
		if(ofstmp < ofsls) {ofstmp += len;} // if loop_length == data_length need			
		for(i = 0; i < 4; i++){
			v[i] = src[ofstmp];			
			if((++ofstmp) > ofsle) {ofstmp -= len;} // -= loop_length , jump loop_start
		}
		goto loop_ofs;
		break;
	case RESAMPLE_MODE_BIDIR_LOOP:			
		if(resrc->increment >= 0){ // normal dir
			if(ofsi < ofsls){
				if(ofsi < 1)
					goto do_linear;
				if((ofsi + 2) < ofsle)
					break; // normal
			}else if(((ofsi + 2) < ofsle) && ((ofsi - 1) >= ofsls))
				break; // normal
			dir = 1;
			ofstmp = ofsi - 1;
			if(ofstmp < ofsls){ // if loop_length == data_length need				
				ofstmp = (ofsls << 1) - ofstmp;
				dir = -1;
			}			
		}else{ // reverse dir
			dir = -1;
			ofstmp = ofsi + 1;
			if(ofstmp > ofsle){ // if loop_length == data_length need				
				ofstmp = (ofsle << 1) - ofstmp;
				dir = 1;
			}
			ofsf = mlt_fraction - ofsf;
		}
		for(i = 0; i < 4; i++){
			v[i] = src[ofstmp];			
			ofstmp += dir;
			if(dir < 0){ // -
				if(ofstmp <= ofsls) {dir = 1;}
			}else{ // +
				if(ofstmp >= ofsle) {dir = -1;}
			}
		}
		goto loop_ofs;
		break;
	}
normal_ofs:
	v[0] = src[ofsi - 1];
    v[1] = src[ofsi];
    v[2] = src[ofsi + 1];	
	v[3] = src[ofsi + 2];
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
loop_ofs:
	ofsf += mlt_fraction;
	tmp = v[1] - v[0];
	v[3] += -3 * v[2] + 3 * v[1] - v[0];
	v[3] *= (FLOAT_T)(ofsf - ml2_fraction) * DIV_6 * div_fraction;
	v[3] += v[2] - v[1] - tmp;
	v[3] *= (FLOAT_T)(ofsf - mlt_fraction) * DIV_2 * div_fraction;
	v[3] += tmp;
	v[3] *= (FLOAT_T)ofsf * div_fraction;
	v[3] += v[0];
	return v[3] * OUT_INT16;
do_linear:
    v[1] = src[ofsi];
	v[2] = (int32)(src[ofsi + 1]) - (int32)(src[ofsi]);
    return (v[1] + v[2] * (FLOAT_T)ofsf * div_fraction) * OUT_INT16; // FLOAT_T
#else // DATA_T_IN32
loop_ofs:
	ofsf += mlt_fraction;
	tmp = v[1] - v[0];
	v[3] += -3*v[2] + 3*v[1] - v[0];
	v[3] = imuldiv_fraction(v[3], (ofsf - ml2_fraction) / 6);
	v[3] += v[2] - v[1] - tmp;
	v[3] = imuldiv_fraction(v[3], (ofsf - mlt_fraction) >> 1);
	v[3] += tmp;
	v[3] = imuldiv_fraction(v[3], ofsf);
	v[3] += v[0];
	return v[3];
do_linear:
    v[1] = src[ofsi];
	v[2] = src[ofsi + 1];
	return v[1] + imuldiv_fraction(v[2] - v[1], ofsf);
#endif
}

#if (USE_X86_EXT_INTRIN >= 10)
// offset:int32*16, resamp:float*16
// ループ内部のoffset計算をint32値域にする , (sample_increment * (req_count+1)) < int32 max
static inline DATA_T *resample_lagrange_multi(Voice *vp, DATA_T *dest, int32 req_count, int32 *out_count)
{
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0;
	const int32 req_count_mask = ~15;
	const int32 count = req_count & req_count_mask;
	splen_t prec_offset = resrc->offset & INTEGER_MASK;
	sample_t *src = vp->sample->data + (prec_offset >> FRACTION_BITS);
	const int32 start_offset = (int32)(resrc->offset - prec_offset); // offset計算をint32値域にする(SIMD用
	const int32 inc = resrc->increment;
	const __m512i vinc = _mm512_set1_epi32(inc * 16), vfmask = _mm512_set1_epi32((int32)FRACTION_MASK);
	__m512i vofs = _mm512_add_epi32(_mm512_set1_epi32(start_offset), _mm512_mullo_epi32(_mm512_set_epi32(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0), _mm512_set1_epi32(inc)));
	const __m512 vdivf = _mm512_set1_ps(div_fraction);	
	const __m512 vfrac_6 = _mm512_set1_ps(div_fraction * DIV_6);
	const __m512 vfrac_2 = _mm512_set1_ps(div_fraction * DIV_2);
	//const __m512 v3n = _mm512_set1_ps(-3);
	const __m512 v3p = _mm512_set1_ps(3);
	const __m512i vfrac = _mm512_set1_epi32(mlt_fraction);
	const __m512i vfrac2 = _mm512_set1_epi32(ml2_fraction);
	const __m512 vec_divo = _mm512_set1_ps(DIV_15BIT);
#ifdef LAO_OPTIMIZE_INCREMENT
	// 最適化レート = (ロードデータ数 - 初期オフセット小数部の最大値(1未満) - 補間ポイント数(lagrangeは3) ) / オフセットデータ数
#ifdef USE_PERMUTEX2
	const int32 opt_inc1 = (1 << FRACTION_BITS) * (32 - 1 - 3) / 16; // (float*16) * 1セット
#else
	const int32 opt_inc1 = (1 << FRACTION_BITS) * (16 - 1 - 3) / 16; // (float*16) * 1セット
#endif
	if(inc < opt_inc1){	// 1セット
	const __m512i vvar1n = _mm512_set1_epi32(-1);
	const __m512i vvar1 = _mm512_set1_epi32(1);
	const __m512i vvar2 = _mm512_set1_epi32(2);
	for(i = 0; i < count; i += 16) {
	__m512i vofsi2 = _mm512_srli_epi32(vofs, FRACTION_BITS); // ofsi
	__m512i vofsi1 = _mm512_add_epi32(vofsi2, vvar1n); // ofsi-1
	__m512i vofsi3 = _mm512_add_epi32(vofsi2, vvar1); // ofsi+1
	__m512i vofsi4 = _mm512_add_epi32(vofsi2, vvar2); // ofsi+2
	int32 ofs0 = _mm_cvtsi128_si32(_mm512_castsi512_si128(vofsi1));
	__m256i vin1 = _mm256_loadu_si256((__m256i *)&src[ofs0]); // int16*16
#ifdef USE_PERMUTEX2
	__m256i vin2 = _mm256_loadu_si256((__m256i *)&src[ofs0 + 16]); // int16*6
#endif
	__m512i vofsib = _mm512_broadcastd_epi32(_mm512_castsi512_si128(vofsi1));
	__m512i vofsub1 = _mm512_sub_epi32(vofsi1, vofsib); 
	__m512i vofsub2 = _mm512_sub_epi32(vofsi2, vofsib);  
	__m512i vofsub3 = _mm512_sub_epi32(vofsi3, vofsib); 
	__m512i vofsub4 = _mm512_sub_epi32(vofsi4, vofsib);
	__m512 vvf1 = _mm512_cvtepi32_ps(_mm512_cvtepi16_epi32(vin1)); // int16 to float (i16*16->i32*16->f32*16
#ifdef USE_PERMUTEX2
	__m512 vvf2 = _mm512_cvtepi32_ps(_mm512_cvtepi16_epi32(vin2));
	__m512 vv0 = _mm512_permutex2var_ps(vvf1, vofsub1, vvf2);
	__m512 vv1 = _mm512_permutex2var_ps(vvf1, vofsub2, vvf2);
	__m512 vv2 = _mm512_permutex2var_ps(vvf1, vofsub3, vvf2);
	__m512 vv3 = _mm512_permutex2var_ps(vvf1, vofsub4, vvf2);
#else
	__m512 vv0 = _mm512_permutexvar_ps(vofsub1, vvf1); // v1 ofsi-1
	__m512 vv1 = _mm512_permutexvar_ps(vofsub2, vvf1); // v2 ofsi
	__m512 vv2 = _mm512_permutexvar_ps(vofsub3, vvf1); // v2 ofsi+1
	__m512 vv3 = _mm512_permutexvar_ps(vofsub4, vvf1); // v2 ofsi+2
#endif
	// あとは通常と同じ
	__m512i vofsf = _mm512_add_epi32(_mm512_and_epi32(vofs, vfmask), vfrac); // ofsf = (ofs & FRACTION_MASK) + mlt_fraction;
	__m512 vtmp = _mm512_sub_ps(vv1, vv0); // tmp = v[1] - v[0];
	__m512 vtmp1, vtmp2, vtmp3, vtmp4;
	//vv3 = _mm512_add_ps(vv3, _mm512_sub_ps(_mm512_fmadd_ps(vv2, v3n, _mm512_mul_ps(vv1, v3p)), vv0)); // v[3] += -3 * v[2] + 3 * v[1] - v[0];
	vv3 = _mm512_add_ps(vv3, _mm512_fmsub_ps(v3p, _mm512_sub_ps(vv1, vv2), vv0)); // v[3] += 3 * (v[1] - v[2]) - v[0];
	vtmp1 = _mm512_mul_ps(_mm512_cvtepi32_ps(_mm512_sub_epi32(vofsf, vfrac2)), vfrac_6); // tmp1 = (float)(ofsf - ml2_fraction) * DIV_6 * div_fraction;
	vtmp2 = _mm512_sub_ps(_mm512_sub_ps(vv2, vv1), vtmp); // tmp2 = v[2] - v[1] - tmp);
	vtmp3 = _mm512_mul_ps(_mm512_cvtepi32_ps(_mm512_sub_epi32(vofsf, vfrac)), vfrac_2); // tmp3 = (FLOAT_T)(ofsf - mlt_fraction) * DIV_2 * div_fraction;
	vtmp4 = _mm512_mul_ps(_mm512_cvtepi32_ps(vofsf), vdivf); // tmp4 = (FLOAT_T)ofsf * div_fraction;
	vv3 = _mm512_fmadd_ps(vv3, vtmp1, vtmp2); // v[3] = v[3] * tmp1 + tmp2
	vv3 = _mm512_fmadd_ps(vv3, vtmp3, vtmp); // v[3] = v[3] * tmp3 + tmp;
	vv3 = _mm512_fmadd_ps(vv3, vtmp4, vv0); // v[3] = v[3] * tmp4 + vv0;
#if defined(DATA_T_DOUBLE)
	vv3 = _mm512_mul_ps(vv3, vec_divo);
	_mm512_storeu_pd(dest, _mm512_cvtps_pd(_mm512_castps512_ps256(vv3)));
	dest += 8;
	_mm512_storeu_pd(dest, _mm512_cvtps_pd(_mm512_extractf32x8_ps(vv3, 0x1)));
	dest += 8;
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT
	_mm512_storeu_ps(dest, _mm512_mul_ps(vv3, vec_divo));
	dest += 16;
#else // DATA_T_IN32
	_mm512_storeu_si512((__m512i *)dest, _mm512_cvtps_epi32(vv3));
	dest += 16;
#endif
	vofs = _mm512_add_epi32(vofs, vinc); // ofs += inc;
	}
	}else
#endif // LAO_OPTIMIZE_INCREMENT
	for(; i < count; i += 16) {
	__m512i vofsi = _mm512_srli_epi32(vofs, FRACTION_BITS); // ofsi = ofs >> FRACTION_BITS
#if 1
	__m512i vsrc0123_0123 = _mm512_i32gather_epi64(_mm512_castsi512_si256(vofsi), (const int*)(src - 1), 2);
	__m512i vsrc4567_0123 = _mm512_i32gather_epi64(_mm512_extracti32x8_epi32(vofsi, 1), (const int*)(src - 1), 2);
	const __m512i vpermi2mask = _mm512_set_epi32(30, 28, 26, 24, 22, 20, 18, 16, 14, 12 ,10, 8, 6, 4, 2, 0);
	__m512i vsrc01 = _mm512_permutex2var_epi32(vsrc0123_0123, vpermi2mask, vsrc4567_0123);
	__m512i vsrc23 = _mm512_permutex2var_epi32(vsrc0123_0123, _mm512_add_epi32(vpermi2mask, _mm512_set1_epi32(1)), vsrc4567_0123);
	__m512i vsrc0 = _mm512_srai_epi32(_mm512_slli_epi32(vsrc01, 16), 16);
	__m512i vsrc1 = _mm512_srai_epi32(vsrc01, 16);
	__m512i vsrc2 = _mm512_srai_epi32(_mm512_slli_epi32(vsrc23, 16), 16);
	__m512i vsrc3 = _mm512_srai_epi32(vsrc23, 16);
	__m512 vv0 = _mm512_cvtepi32_ps(vsrc0);
	__m512 vv1 = _mm512_cvtepi32_ps(vsrc1);
	__m512 vv2 = _mm512_cvtepi32_ps(vsrc2);
	__m512 vv3 = _mm512_cvtepi32_ps(vsrc3);
#else
	__m128i vin1 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,0) - 1]); // ofsi-1~ofsi+2をロード
	__m128i vin2 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,1) - 1]); // 次周サンプルも同じ
	__m128i vin3 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,2) - 1]); // 次周サンプルも同じ
	__m128i vin4 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,3) - 1]); // 次周サンプルも同じ
	__m128i vin5 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,4) - 1]); // 次周サンプルも同じ
	__m128i vin6 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,5) - 1]); // 次周サンプルも同じ
	__m128i vin7 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,6) - 1]); // 次周サンプルも同じ
	__m128i vin8 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,7) - 1]); // 次周サンプルも同じ
	__m128i vin12 = _mm_unpacklo_epi16(vin1, vin2); // [v11v21v31v41],[v12v22v32v42] to [v11v12v21v22v31v32v41v42]
	__m128i vin34 =	_mm_unpacklo_epi16(vin3, vin4); // [v13v23v33v43],[v14v24v34v44] to [v13v14v23v24v33v34v43v44]
	__m128i vin56 =	_mm_unpacklo_epi16(vin5, vin6); // [v15v25v35v45],[v16v26v36v46] to [v15v16v25v26v35v36v45v46]
	__m128i vin78 =	_mm_unpacklo_epi16(vin7, vin8); // [v17v27v37v47],[v18v28v38v48] to [v17v18v27v28v37v38v47v48]
	__m128i vin1121 = _mm_unpacklo_epi32(vin12, vin34); // [v11v12,v21v22],[v13v14,v23v24] to [v11v12v13v14,v21v22v23v24]
	__m128i vin3141 = _mm_unpackhi_epi32(vin12, vin34); // [v31v32,v41v42],[v33v34v,43v44] to [v31v32v33v34,v41v42v43v44]
	__m128i vin1525 = _mm_unpacklo_epi32(vin56, vin78); // [v15v16,v25v26],[v17v18,v27v28] to [v15v16v17v18,v25v26v27v28]
	__m128i vin3545 = _mm_unpackhi_epi32(vin56, vin78); // [v35v36,v45v46],[v37v38v,47v48] to [v35v36v37v38,v45v46v47v48]
	__m128i vi16_1 = _mm_unpacklo_epi64(vin1121, vin1525); // [v11v12v13v14,v21v22v23v24],[v15v16v17v18,v25v26v27v28] to [v11v12v13v14v15v16v17v18]
	__m128i vi16_2 = _mm_unpackhi_epi64(vin1121, vin1525); // [v11v12v13v14,v21v22v23v24],[v15v16v17v18,v25v26v27v28] to [v21v22v23v24v25v26v27v28]
	__m128i vi16_3 = _mm_unpacklo_epi64(vin3141, vin3545); // [v31v32v33v34,v41v42v43v44],[v35v36v37v38,v45v46v47v48] to [v31v32v33v34v35v36v37v38]
	__m128i vi16_4 = _mm_unpackhi_epi64(vin3141, vin3545); // [v31v32v33v34,v41v42v43v44],[v35v36v37v38,v45v46v47v48] to [v41v42v43v44v45v46v47v48]
	__m256 vv0 = _mm256_cvtepi32_ps(_mm256_cvtepi16_epi32(vi16_1)); // int16 to float (16bit*8 -> 32bit*8 > float*8
	__m256 vv1 = _mm256_cvtepi32_ps(_mm256_cvtepi16_epi32(vi16_2)); // int16 to float (16bit*8 -> 32bit*8 > float*8
	__m256 vv2 = _mm256_cvtepi32_ps(_mm256_cvtepi16_epi32(vi16_3)); // int16 to float (16bit*8 -> 32bit*8 > float*8
	__m256 vv3 = _mm256_cvtepi32_ps(_mm256_cvtepi16_epi32(vi16_4)); // int16 to float (16bit*8 -> 32bit*8 > float*8
#endif
	__m512i vofsf = _mm512_add_epi32(_mm512_and_epi32(vofs, vfmask), vfrac); // ofsf = (ofs & FRACTION_MASK) + mlt_fraction;
	__m512 vtmp = _mm512_sub_ps(vv1, vv0); // tmp = v[1] - v[0];
	__m512 vtmp1, vtmp2, vtmp3, vtmp4;
	//vv3 = _mm512_add_ps(vv3, _mm512_sub_ps(_mm512_fmadd_ps(vv2, v3n, _mm512_mul_ps(vv1, v3p)), vv0)); // v[3] += -3 * v[2] + 3 * v[1] - v[0];
	vv3 = _mm512_add_ps(vv3, _mm512_fmsub_ps(v3p, _mm512_sub_ps(vv1, vv2), vv0)); // v[3] += 3 * (v[1] - v[2]) - v[0];
	vtmp1 = _mm512_mul_ps(_mm512_cvtepi32_ps(_mm512_sub_epi32(vofsf, vfrac2)), vfrac_6); // tmp1 = (float)(ofsf - ml2_fraction) * DIV_6 * div_fraction;
	vtmp2 = _mm512_sub_ps(_mm512_sub_ps(vv2, vv1), vtmp); // tmp2 = v[2] - v[1] - tmp);
	vtmp3 = _mm512_mul_ps(_mm512_cvtepi32_ps(_mm512_sub_epi32(vofsf, vfrac)), vfrac_2); // tmp3 = (FLOAT_T)(ofsf - mlt_fraction) * DIV_2 * div_fraction;
	vtmp4 = _mm512_mul_ps(_mm512_cvtepi32_ps(vofsf), vdivf); // tmp4 = (FLOAT_T)ofsf * div_fraction;
	vv3 = _mm512_fmadd_ps(vv3, vtmp1, vtmp2); // v[3] = v[3] * tmp1 + tmp2
	vv3 = _mm512_fmadd_ps(vv3, vtmp3, vtmp); // v[3] = v[3] * tmp3 + tmp;
	vv3 = _mm512_fmadd_ps(vv3, vtmp4, vv0); // v[3] = v[3] * tmp4 + vv0;
#if defined(DATA_T_DOUBLE)
	vv3 = _mm512_mul_ps(vv3, vec_divo);
	_mm512_storeu_pd(dest, _mm512_cvtps_pd(_mm512_castps512_ps256(vv3)));
	dest += 8;
	_mm512_storeu_pd(dest, _mm512_cvtps_pd(_mm512_extractf32x8_ps(vv3, 0x1)));
	dest += 8;
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT
	_mm512_storeu_ps(dest, _mm512_mul_ps(vv3, vec_divo));
	dest += 16;
#else // DATA_T_IN32
	_mm512_storeu_si256((__m512i *)dest, _mm512_cvtps_epi32(vv3));
	dest += 8;
#endif
	vofs = _mm512_add_epi32(vofs, vinc); // ofs += inc;
	}
	resrc->offset = prec_offset + (splen_t)(_mm_cvtsi128_si32(_mm512_castsi512_si128(vofs)));
	*out_count = i;
    return dest;
}

#elif (USE_X86_EXT_INTRIN >= 9)
// offset:int32*8, resamp:float*8
// ループ内部のoffset計算をint32値域にする , (sample_increment * (req_count+1)) < int32 max
static inline DATA_T *resample_lagrange_multi(Voice *vp, DATA_T *dest, int32 req_count, int32 *out_count)
{
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0;
	const int32 req_count_mask = ~(0x7);
	const int32 count = req_count & req_count_mask;
	splen_t prec_offset = resrc->offset & INTEGER_MASK;
	sample_t *src = vp->sample->data + (prec_offset >> FRACTION_BITS);
	const int32 start_offset = (int32)(resrc->offset - prec_offset); // offset計算をint32値域にする(SIMD用
	const int32 inc = resrc->increment;
	const __m256i vinc = _mm256_set1_epi32(inc * 8), vfmask = _mm256_set1_epi32((int32)FRACTION_MASK);
	__m256i vofs = _mm256_add_epi32(_mm256_set1_epi32(start_offset), _mm256_set_epi32(inc*7,inc*6,inc*5,inc*4,inc*3,inc*2,inc,0));
	const __m256 vdivf = _mm256_set1_ps(div_fraction);	
	const __m256 vfrac_6 = _mm256_set1_ps(div_fraction * DIV_6);
	const __m256 vfrac_2 = _mm256_set1_ps(div_fraction * DIV_2);
	//const __m256 v3n = _mm256_set1_ps(-3);
	const __m256 v3p = _mm256_set1_ps(3);
	const __m256i vfrac = _mm256_set1_epi32(mlt_fraction);
	const __m256i vfrac2 = _mm256_set1_epi32(ml2_fraction);
	const __m256 vec_divo = _mm256_set1_ps(DIV_15BIT);
#ifdef LAO_OPTIMIZE_INCREMENT
	// 最適化レート = (ロードデータ数 - 初期オフセット小数部の最大値(1未満) - 補間ポイント数(lagrangeは3) ) / オフセットデータ数
	// ロードデータ数はint16用permutevarがないので変換後の32bit(int32/float)の8セットになる
	const int32 opt_inc1 = (1 << FRACTION_BITS) * (8 - 1 - 3) / 8; // (float*8) * 1セット
	if(inc < opt_inc1){	// 1セット
	const __m256i vvar1n = _mm256_set1_epi32(-1);
	const __m256i vvar1 = _mm256_set1_epi32(1);
	const __m256i vvar2 = _mm256_set1_epi32(2);
	for(i = 0; i < count; i += 8) {
	__m256i vofsi2 = _mm256_srli_epi32(vofs, FRACTION_BITS); // ofsi
	__m256i vofsi1 = _mm256_add_epi32(vofsi2, vvar1n); // ofsi-1
	__m256i vofsi3 = _mm256_add_epi32(vofsi2, vvar1); // ofsi+1
	__m256i vofsi4 = _mm256_add_epi32(vofsi2, vvar2); // ofsi+2
	int32 ofs0 = _mm_cvtsi128_si32(_mm256_extracti128_si256(vofsi1, 0x0));
	__m128i vin1 = _mm_loadu_si128((__m128i *)&src[ofs0]); // int16*8
	__m256i vofsib = _mm256_permutevar8x32_epi32(vofsi1, _mm256_setzero_si256()); 
	__m256i vofsub1 = _mm256_sub_epi32(vofsi1, vofsib); 
	__m256i vofsub2 = _mm256_sub_epi32(vofsi2, vofsib);  
	__m256i vofsub3 = _mm256_sub_epi32(vofsi3, vofsib); 
	__m256i vofsub4 = _mm256_sub_epi32(vofsi4, vofsib);
	__m256 vvf1 = _mm256_cvtepi32_ps(_mm256_cvtepi16_epi32(vin1)); // int16 to float (i16*8->i32*8->f32*8
	__m256 vv0 = _mm256_permutevar8x32_ps(vvf1, vofsub1); // v1 ofsi-1
	__m256 vv1 = _mm256_permutevar8x32_ps(vvf1, vofsub2); // v2 ofsi
	__m256 vv2 = _mm256_permutevar8x32_ps(vvf1, vofsub3); // v2 ofsi+1
	__m256 vv3 = _mm256_permutevar8x32_ps(vvf1, vofsub4); // v2 ofsi+2
	// あとは通常と同じ
	__m256i vofsf = _mm256_add_epi32(_mm256_and_si256(vofs, vfmask), vfrac); // ofsf = (ofs & FRACTION_MASK) + mlt_fraction;
	__m256 vtmp = _mm256_sub_ps(vv1, vv0); // tmp = v[1] - v[0];
	__m256 vtmp1, vtmp2, vtmp3, vtmp4;
	//vv3 = _mm256_add_ps(vv3, _mm256_sub_ps(MM256_FMA2_PS(vv2, v3n, vv1, v3p), vv0)); // v[3] += -3 * v[2] + 3 * v[1] - v[0];
#if (USE_X86_EXT_INTRIN >= 9)
	vv3 = _mm256_add_ps(vv3, _mm256_fmsub_ps(v3p, _mm256_sub_ps(vv1, vv2), vv0)); // v[3] += 3 * (v[1] - v[2]) - v[0];
#else
	vv3 = _mm256_add_ps(vv3, _mm256_sub_ps(_mm256_mul_ps(v3p, _mm256_sub_ps(vv1, vv2)), vv0)); // v[3] += 3 * (v[1] - v[2]) - v[0];
#endif
	vtmp1 = _mm256_mul_ps(_mm256_cvtepi32_ps(_mm256_sub_epi32(vofsf, vfrac2)), vfrac_6); // tmp1 = (float)(ofsf - ml2_fraction) * DIV_6 * div_fraction;
	vtmp2 = _mm256_sub_ps(_mm256_sub_ps(vv2, vv1), vtmp); // tmp2 = v[2] - v[1] - tmp);
	vtmp3 = _mm256_mul_ps(_mm256_cvtepi32_ps(_mm256_sub_epi32(vofsf, vfrac)), vfrac_2); // tmp3 = (FLOAT_T)(ofsf - mlt_fraction) * DIV_2 * div_fraction;
	vtmp4 = _mm256_mul_ps(_mm256_cvtepi32_ps(vofsf), vdivf); // tmp4 = (FLOAT_T)ofsf * div_fraction;
	vv3 = MM256_FMA_PS(vv3, vtmp1, vtmp2); // v[3] = v[3] * tmp1 + tmp2
	vv3 = MM256_FMA_PS(vv3, vtmp3, vtmp); // v[3] = v[3] * tmp3 + tmp;
	vv3 = MM256_FMA_PS(vv3, vtmp4, vv0); // v[3] = v[3] * tmp4 + vv0;
#if defined(DATA_T_DOUBLE)
	vv3 = _mm256_mul_ps(vv3, vec_divo);
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(_mm256_castps256_ps128(vv3)));
	dest += 4;
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(_mm256_extractf128_ps(vv3, 0x1)));
	dest += 4;
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT
	_mm256_storeu_ps(dest, _mm256_mul_ps(vv3, vec_divo));
	dest += 8;
#else // DATA_T_IN32
	_mm_storeu_si128((__m128i *)dest, _mm_cvtps_epi32(_mm256_extractf128_ps(vv3, 0x0)));
	dest += 4;
	_mm_storeu_si128((__m128i *)dest, _mm_cvtps_epi32(_mm256_extractf128_ps(vv3, 0x1)));
	dest += 4;
#endif
	vofs = _mm256_add_epi32(vofs, vinc); // ofs += inc;
	}
	}else
#endif // LAO_OPTIMIZE_INCREMENT
	for(; i < count; i += 8) {
	__m256i vofsi = _mm256_srli_epi32(vofs, FRACTION_BITS); // ofsi = ofs >> FRACTION_BITS
	__m128i vin1 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,0) - 1]); // ofsi-1~ofsi+2をロード
	__m128i vin2 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,1) - 1]); // 次周サンプルも同じ
	__m128i vin3 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,2) - 1]); // 次周サンプルも同じ
	__m128i vin4 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,3) - 1]); // 次周サンプルも同じ
	__m128i vin5 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,4) - 1]); // 次周サンプルも同じ
	__m128i vin6 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,5) - 1]); // 次周サンプルも同じ
	__m128i vin7 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,6) - 1]); // 次周サンプルも同じ
	__m128i vin8 = _mm_loadu_si128((__m128i *)&src[MM256_EXTRACT_I32(vofsi,7) - 1]); // 次周サンプルも同じ
	__m128i vin12 = _mm_unpacklo_epi16(vin1, vin2); // [v11v21v31v41],[v12v22v32v42] to [v11v12v21v22v31v32v41v42]
	__m128i vin34 =	_mm_unpacklo_epi16(vin3, vin4); // [v13v23v33v43],[v14v24v34v44] to [v13v14v23v24v33v34v43v44]
	__m128i vin56 =	_mm_unpacklo_epi16(vin5, vin6); // [v15v25v35v45],[v16v26v36v46] to [v15v16v25v26v35v36v45v46]
	__m128i vin78 =	_mm_unpacklo_epi16(vin7, vin8); // [v17v27v37v47],[v18v28v38v48] to [v17v18v27v28v37v38v47v48]
	__m128i vin1121 = _mm_unpacklo_epi32(vin12, vin34); // [v11v12,v21v22],[v13v14,v23v24] to [v11v12v13v14,v21v22v23v24]
	__m128i vin3141 = _mm_unpackhi_epi32(vin12, vin34); // [v31v32,v41v42],[v33v34v,43v44] to [v31v32v33v34,v41v42v43v44]
	__m128i vin1525 = _mm_unpacklo_epi32(vin56, vin78); // [v15v16,v25v26],[v17v18,v27v28] to [v15v16v17v18,v25v26v27v28]
	__m128i vin3545 = _mm_unpackhi_epi32(vin56, vin78); // [v35v36,v45v46],[v37v38v,47v48] to [v35v36v37v38,v45v46v47v48]
	__m128i vi16_1 = _mm_unpacklo_epi64(vin1121, vin1525); // [v11v12v13v14,v21v22v23v24],[v15v16v17v18,v25v26v27v28] to [v11v12v13v14v15v16v17v18]
	__m128i vi16_2 = _mm_unpackhi_epi64(vin1121, vin1525); // [v11v12v13v14,v21v22v23v24],[v15v16v17v18,v25v26v27v28] to [v21v22v23v24v25v26v27v28]
	__m128i vi16_3 = _mm_unpacklo_epi64(vin3141, vin3545); // [v31v32v33v34,v41v42v43v44],[v35v36v37v38,v45v46v47v48] to [v31v32v33v34v35v36v37v38]
	__m128i vi16_4 = _mm_unpackhi_epi64(vin3141, vin3545); // [v31v32v33v34,v41v42v43v44],[v35v36v37v38,v45v46v47v48] to [v41v42v43v44v45v46v47v48]
	__m256 vv0 = _mm256_cvtepi32_ps(_mm256_cvtepi16_epi32(vi16_1)); // int16 to float (16bit*8 -> 32bit*8 > float*8
	__m256 vv1 = _mm256_cvtepi32_ps(_mm256_cvtepi16_epi32(vi16_2)); // int16 to float (16bit*8 -> 32bit*8 > float*8
	__m256 vv2 = _mm256_cvtepi32_ps(_mm256_cvtepi16_epi32(vi16_3)); // int16 to float (16bit*8 -> 32bit*8 > float*8
	__m256 vv3 = _mm256_cvtepi32_ps(_mm256_cvtepi16_epi32(vi16_4)); // int16 to float (16bit*8 -> 32bit*8 > float*8
	__m256i vofsf = _mm256_add_epi32(_mm256_and_si256(vofs, vfmask), vfrac); // ofsf = (ofs & FRACTION_MASK) + mlt_fraction;
	__m256 vtmp = _mm256_sub_ps(vv1, vv0); // tmp = v[1] - v[0];
	__m256 vtmp1, vtmp2, vtmp3, vtmp4;
	//vv3 = _mm256_add_ps(vv3, _mm256_sub_ps(MM256_FMA2_PS(vv2, v3n, vv1, v3p), vv0)); // v[3] += -3 * v[2] + 3 * v[1] - v[0];
#if (USE_X86_EXT_INTRIN >= 9)
	vv3 = _mm256_add_ps(vv3, _mm256_fmsub_ps(v3p, _mm256_sub_ps(vv1, vv2), vv0)); // v[3] += 3 * (v[1] - v[2]) - v[0];
#else
	vv3 = _mm256_add_ps(vv3, _mm256_sub_ps(_mm256_mul_ps(v3p, _mm256_sub_ps(vv1, vv2)), vv0)); // v[3] += 3 * (v[1] - v[2]) - v[0];
#endif
	vtmp1 = _mm256_mul_ps(_mm256_cvtepi32_ps(_mm256_sub_epi32(vofsf, vfrac2)), vfrac_6); // tmp1 = (float)(ofsf - ml2_fraction) * DIV_6 * div_fraction;
	vtmp2 = _mm256_sub_ps(_mm256_sub_ps(vv2, vv1), vtmp); // tmp2 = v[2] - v[1] - tmp);
	vtmp3 = _mm256_mul_ps(_mm256_cvtepi32_ps(_mm256_sub_epi32(vofsf, vfrac)), vfrac_2); // tmp3 = (FLOAT_T)(ofsf - mlt_fraction) * DIV_2 * div_fraction;
	vtmp4 = _mm256_mul_ps(_mm256_cvtepi32_ps(vofsf), vdivf); // tmp4 = (FLOAT_T)ofsf * div_fraction;
	vv3 = MM256_FMA_PS(vv3, vtmp1, vtmp2); // v[3] = v[3] * tmp1 + tmp2
	vv3 = MM256_FMA_PS(vv3, vtmp3, vtmp); // v[3] = v[3] * tmp3 + tmp;
	vv3 = MM256_FMA_PS(vv3, vtmp4, vv0); // v[3] = v[3] * tmp4 + vv0;
#if defined(DATA_T_DOUBLE)
	vv3 = _mm256_mul_ps(vv3, vec_divo);
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(_mm256_castps256_ps128(vv3)));
	dest += 4;
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(_mm256_extractf128_ps(vv3, 0x1)));
	dest += 4;
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT
	_mm256_storeu_ps(dest, _mm256_mul_ps(vv3, vec_divo));
	dest += 8;
#else // DATA_T_IN32
	_mm256_storeu_si256((__m256i *)dest, _mm256_cvtps_epi32(vv3));
	dest += 8;
#endif
	vofs = _mm256_add_epi32(vofs, vinc); // ofs += inc;
	}
	resrc->offset = prec_offset + (splen_t)(MM256_EXTRACT_I32(vofs,0));
	*out_count = i;
    return dest;
}

#elif (USE_X86_EXT_INTRIN >= 3)
// offset:int32*4*2, resamp:float*4*2 2set 15.51s (1set 16.08s
// ループ内部のoffset計算をint32値域にする , (sample_increment * (req_count+1)) < int32 max
static inline DATA_T *resample_lagrange_multi(Voice *vp, DATA_T *dest, int32 req_count, int32 *out_count)
{
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0;
	const int32 req_count_mask = ~(0x7);
	const int32 count = req_count & req_count_mask;
	splen_t prec_offset = resrc->offset & INTEGER_MASK;
	sample_t *src = vp->sample->data + (prec_offset >> FRACTION_BITS);
	const int32 start_offset = (int32)(resrc->offset - prec_offset); // offset計算をint32値域にする(SIMD用
	const int32 inc = resrc->increment;
	const __m128i vinc = _mm_set1_epi32(inc * 8), vfmask = _mm_set1_epi32((int32)FRACTION_MASK);
	__m128i vofs1 = _mm_add_epi32(_mm_set1_epi32(start_offset), _mm_set_epi32(inc * 3, inc * 2, inc, 0));
	__m128i vofs2 = _mm_add_epi32(vofs1, _mm_set1_epi32(inc * 4));
	const __m128 vdivf = _mm_set1_ps(div_fraction);	
	const __m128 vfrac_6 = _mm_set1_ps(div_fraction * DIV_6);
	const __m128 vfrac_2 = _mm_set1_ps(div_fraction * DIV_2);
	const __m128 v3n = _mm_set1_ps(-3);
	const __m128 v3p = _mm_set1_ps(3);
	const __m128i vfrac = _mm_set1_epi32(mlt_fraction);
	const __m128i vfrac2 = _mm_set1_epi32(ml2_fraction);
	const __m128 vec_divo = _mm_set1_ps(DIV_15BIT);
	for(; i < count; i += 8) {
	__m128i vofsi1 = _mm_srli_epi32(vofs1, FRACTION_BITS); // ofsi = ofs >> FRACTION_BITS
	__m128i vofsi2 = _mm_srli_epi32(vofs2, FRACTION_BITS); // ofsi = ofs >> FRACTION_BITS
	__m128i vin1 = _mm_loadu_si128((__m128i *)&src[MM_EXTRACT_I32(vofsi1,0) - 1]); // ofsi-1~ofsi+2をロード
	__m128i vin2 = _mm_loadu_si128((__m128i *)&src[MM_EXTRACT_I32(vofsi1,1) - 1]); // 次周サンプルも同じ
	__m128i vin3 = _mm_loadu_si128((__m128i *)&src[MM_EXTRACT_I32(vofsi1,2) - 1]); // 次周サンプルも同じ
	__m128i vin4 = _mm_loadu_si128((__m128i *)&src[MM_EXTRACT_I32(vofsi1,3) - 1]); // 次周サンプルも同じ
	__m128i vin5 = _mm_loadu_si128((__m128i *)&src[MM_EXTRACT_I32(vofsi2,0) - 1]); // 次周サンプルも同じ
	__m128i vin6 = _mm_loadu_si128((__m128i *)&src[MM_EXTRACT_I32(vofsi2,1) - 1]); // 次周サンプルも同じ
	__m128i vin7 = _mm_loadu_si128((__m128i *)&src[MM_EXTRACT_I32(vofsi2,2) - 1]); // 次周サンプルも同じ
	__m128i vin8 = _mm_loadu_si128((__m128i *)&src[MM_EXTRACT_I32(vofsi2,3) - 1]); // 次周サンプルも同じ
	__m128i vin12 = _mm_unpacklo_epi16(vin1, vin2); // [v11v21v31v41],[v12v22v32v42] to [v11v12v21v22v31v32v41v42]
	__m128i vin34 =	_mm_unpacklo_epi16(vin3, vin4); // [v13v23v33v43],[v14v24v34v44] to [v13v14v23v24v33v34v43v44]
	__m128i vin56 =	_mm_unpacklo_epi16(vin5, vin6); // [v15v25v35v45],[v16v26v36v46] to [v15v16v25v26v35v36v45v46]
	__m128i vin78 =	_mm_unpacklo_epi16(vin7, vin8); // [v17v27v37v47],[v18v28v38v48] to [v17v18v27v28v37v38v47v48]
	__m128i vi16_1 = _mm_unpacklo_epi32(vin12, vin34); // [v11v12,v21v22],[v13v14,v23v24] to [v11v12v13v14,v21v22v23v24]
	__m128i vi16_2 = _mm_unpackhi_epi32(vin12, vin34); // [v31v32,v41v42],[v33v34v,43v44] to [v31v32v33v34,v41v42v43v44]
	__m128i vi16_3 = _mm_unpacklo_epi32(vin56, vin78); // [v15v16,v25v26],[v17v18,v27v28] to [v15v16v17v18,v25v26v27v28]
	__m128i vi16_4 = _mm_unpackhi_epi32(vin56, vin78); // [v35v36,v45v46],[v37v38v,47v48] to [v35v36v37v38,v45v46v47v48]
#if (USE_X86_EXT_INTRIN >= 6) // sse4.1 , _mm_ cvtepi16_epi32()
	__m128i vi16_1_2 = _mm_shuffle_epi32(vi16_1, 0x4e); // ofsi+0はL64bitへ
	__m128i vi16_2_2 = _mm_shuffle_epi32(vi16_2, 0x4e); // ofsi+2はL64bitへ
	__m128i vi16_3_2 = _mm_shuffle_epi32(vi16_3, 0x4e); // ofsi+0はL64bitへ
	__m128i vi16_4_2 = _mm_shuffle_epi32(vi16_4, 0x4e); // ofsi+2はL64bitへ
	__m128 vv01 = _mm_cvtepi32_ps(_mm_cvtepi16_epi32(vi16_1)); // int16 to float
	__m128 vv11 = _mm_cvtepi32_ps(_mm_cvtepi16_epi32(vi16_1_2)); // int16 to float
	__m128 vv21 = _mm_cvtepi32_ps(_mm_cvtepi16_epi32(vi16_2)); // int16 to float
	__m128 vv31 = _mm_cvtepi32_ps(_mm_cvtepi16_epi32(vi16_2_2)); // int16 to float
	__m128 vv02 = _mm_cvtepi32_ps(_mm_cvtepi16_epi32(vi16_3)); // int16 to float
	__m128 vv12 = _mm_cvtepi32_ps(_mm_cvtepi16_epi32(vi16_3_2)); // int16 to float
	__m128 vv22 = _mm_cvtepi32_ps(_mm_cvtepi16_epi32(vi16_4)); // int16 to float
	__m128 vv32 = _mm_cvtepi32_ps(_mm_cvtepi16_epi32(vi16_4_2)); // int16 to float
#else
	__m128i sign1 = _mm_cmpgt_epi16(_mm_setzero_si128(), vi16_1);
	__m128i sign2 = _mm_cmpgt_epi16(_mm_setzero_si128(), vi16_2);
	__m128i sign3 = _mm_cmpgt_epi16(_mm_setzero_si128(), vi16_3);
	__m128i sign4 = _mm_cmpgt_epi16(_mm_setzero_si128(), vi16_4);
	__m128 vv01 = _mm_cvtepi32_ps(_mm_unpacklo_epi16(vi16_1, sign1)); // int16 to float
	__m128 vv11 = _mm_cvtepi32_ps(_mm_unpackhi_epi16(vi16_1, sign1)); // int16 to float
	__m128 vv21 = _mm_cvtepi32_ps(_mm_unpacklo_epi16(vi16_2, sign2)); // int16 to float
	__m128 vv31 = _mm_cvtepi32_ps(_mm_unpackhi_epi16(vi16_2, sign2)); // int16 to float
	__m128 vv02 = _mm_cvtepi32_ps(_mm_unpacklo_epi16(vi16_3, sign3)); // int16 to float
	__m128 vv12 = _mm_cvtepi32_ps(_mm_unpackhi_epi16(vi16_3, sign3)); // int16 to float
	__m128 vv22 = _mm_cvtepi32_ps(_mm_unpacklo_epi16(vi16_4, sign4)); // int16 to float
	__m128 vv32 = _mm_cvtepi32_ps(_mm_unpackhi_epi16(vi16_4, sign4)); // int16 to float
#endif
	__m128i vofsf1 = _mm_add_epi32(_mm_and_si128(vofs1, vfmask), vfrac); // ofsf = (ofs & FRACTION_MASK) + mlt_fraction;
	__m128i vofsf2 = _mm_add_epi32(_mm_and_si128(vofs2, vfmask), vfrac); // ofsf = (ofs & FRACTION_MASK) + mlt_fraction;
	__m128 vtmp1 = _mm_sub_ps(vv11, vv01); // tmp = v[1] - v[0];
	__m128 vtmp2 = _mm_sub_ps(vv12, vv02); // tmp = v[1] - v[0];
	__m128 vtmpx11, vtmpx12, vtmpx21, vtmpx22, vtmpx31, vtmpx32, vtmpx41, vtmpx42;
	__m128 vtmpi1, vtmpi2;
	vv31 = _mm_add_ps(vv31, _mm_sub_ps(MM_FMA2_PS(vv21, v3n, vv11, v3p), vv01)); // v[3] += -3 * v[2] + 3 * v[1] - v[0];
	vv32 = _mm_add_ps(vv32, _mm_sub_ps(MM_FMA2_PS(vv22, v3n, vv12, v3p), vv02)); // v[3] += -3 * v[2] + 3 * v[1] - v[0];
	vtmpi1 = _mm_cvtepi32_ps(_mm_sub_epi32(vofsf1, vfrac2));
	vtmpi2 = _mm_cvtepi32_ps(_mm_sub_epi32(vofsf2, vfrac2));
	vtmpx11 = _mm_mul_ps(vtmpi1, vfrac_6); // tmpx1 = (float)(ofsf - ml2_fraction) * DIV_6 * div_fraction;
	vtmpx12 = _mm_mul_ps(vtmpi2, vfrac_6); // tmpx1 = (float)(ofsf - ml2_fraction) * DIV_6 * div_fraction;
	vtmpx21 = _mm_sub_ps(_mm_sub_ps(vv21, vv11), vtmp1); // tmpx2 = v[2] - v[1] - tmp);
	vtmpx22 = _mm_sub_ps(_mm_sub_ps(vv22, vv12), vtmp2); // tmpx2 = v[2] - v[1] - tmp);
	vtmpi1 = _mm_cvtepi32_ps(_mm_sub_epi32(vofsf1, vfrac));
	vtmpi2 = _mm_cvtepi32_ps(_mm_sub_epi32(vofsf2, vfrac));
	vtmpx31 = _mm_mul_ps(vtmpi1, vfrac_2); // tmpx3 = (FLOAT_T)(ofsf - mlt_fraction) * DIV_2 * div_fraction;
	vtmpx32 = _mm_mul_ps(vtmpi2, vfrac_2); // tmpx3 = (FLOAT_T)(ofsf - mlt_fraction) * DIV_2 * div_fraction;
	vtmpi1 = _mm_cvtepi32_ps(vofsf1);
	vtmpi2 = _mm_cvtepi32_ps(vofsf2);
	vtmpx41 = _mm_mul_ps(vtmpi1, vdivf); // tmpx4 = (FLOAT_T)ofsf * div_fraction;
	vtmpx42 = _mm_mul_ps(vtmpi2, vdivf); // tmpx4 = (FLOAT_T)ofsf * div_fraction;
	vv31 = MM_FMA_PS(vv31, vtmpx11, vtmpx21); // v[3] = v[3] * tmpx1 + tmpx2
	vv32 = MM_FMA_PS(vv32, vtmpx12, vtmpx22); // v[3] = v[3] * tmp1 + tmp2
	vv31 = MM_FMA_PS(vv31, vtmpx31, vtmp1); // v[3] = v[3] * tmpx3 + tmp;
	vv32 = MM_FMA_PS(vv32, vtmpx32, vtmp2); // v[3] = v[3] * tmpx3 + tmp;
	vv31 = MM_FMA_PS(vv31, vtmpx41, vv01); // v[3] = v[3] * tmpx4 + vv0;
	vv32 = MM_FMA_PS(vv32, vtmpx42, vv02); // v[3] = v[3] * tmpx4 + vv0;
#if defined(DATA_T_DOUBLE)
	vv31 = _mm_mul_ps(vv31, vec_divo);
	vv32 = _mm_mul_ps(vv32, vec_divo);
#if (USE_X86_EXT_INTRIN >= 8)	
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(vv31));
	dest += 4;
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(vv32));
	dest += 4;
#else
	_mm_storeu_pd(dest, _mm_cvtps_pd(vv31));
	dest += 2;
	_mm_storeu_pd(dest, _mm_cvtps_pd(_mm_movehl_ps(vv31, vv31)));
	dest += 2;
	_mm_storeu_pd(dest, _mm_cvtps_pd(vv32));
	dest += 2;
	_mm_storeu_pd(dest, _mm_cvtps_pd(_mm_movehl_ps(vv32, vv32)));
	dest += 2;
#endif
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT
	_mm_storeu_ps(dest, _mm_mul_ps(vv31, vec_divo));
	dest += 4;
	_mm_storeu_ps(dest, _mm_mul_ps(vv32, vec_divo));
	dest += 4;
#else // DATA_T_IN32
	_mm_storeu_si128((__m128i *)dest, _mm_cvtps_epi32(vv31));
	dest += 4;
	_mm_storeu_si128((__m128i *)dest, _mm_cvtps_epi32(vv32));
	dest += 4;
#endif
	vofs1 = _mm_add_epi32(vofs1, vinc); // ofs += inc;
	vofs2 = _mm_add_epi32(vofs2, vinc); // ofs += inc;
	}
	resrc->offset = prec_offset + (splen_t)(MM_EXTRACT_I32(vofs1,0));
	*out_count = i;
    return dest;
}

#else // not use MMX/SSE/AVX 
// ループ内部のoffset計算をint32値域にする , (sample_increment * (req_count+1)) < int32 max
static inline DATA_T *resample_lagrange_multi(Voice *vp, DATA_T *dest, int32 req_count, int32 *out_count)
{
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0;
	splen_t prec_offset = resrc->offset & INTEGER_MASK;
	sample_t *src = vp->sample->data + (prec_offset >> FRACTION_BITS);
	int32 ofs = (int32)(resrc->offset & FRACTION_MASK);
	int32 inc = resrc->increment;

	for(i = 0; i < req_count; i++) {
		int32 ofsi, ofsf;
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
		FLOAT_T v[4], tmp;
		ofsi = ofs >> FRACTION_BITS, ofsf = ofs & FRACTION_MASK; ofs += inc;		
		v[0] = src[ofsi - 1]; 
		v[1] = src[ofsi];
		v[2] = src[ofsi + 1];	
		v[3] = src[ofsi + 2];		
		ofsf += mlt_fraction;
		tmp = v[1] - v[0];
		v[3] += -3 * v[2] + 3 * v[1] - v[0];
		v[3] *= (FLOAT_T)(ofsf - ml2_fraction) * DIV_6 * div_fraction;
		v[3] += v[2] - v[1] - tmp;
		v[3] *= (FLOAT_T)(ofsf - mlt_fraction) * DIV_2 * div_fraction;
		v[3] += tmp;
		v[3] *= (FLOAT_T)ofsf * div_fraction;
		v[3] += v[0];
		*dest++ = v[3] * OUT_INT16;
#else // DATA_T_IN32
		int32 v[4], tmp;
		ofsi = ofs >> FRACTION_BITS, ofsf = ofs & FRACTION_MASK; ofs += inc;
		v[0] = src[ofsi - 1];
		v[1] = src[ofsi];
		v[2] = src[ofsi + 1];	
		v[3] = src[ofsi + 2];			
		ofsf += mlt_fraction;
		tmp = v[1] - v[0];
		v[3] += -3*v[2] + 3*v[1] - v[0];
		v[3] = imuldiv_fraction(v[3], (ofsf - ml2_fraction) / 6);
		v[3] += v[2] - v[1] - tmp;
		v[3] = imuldiv_fraction(v[3], (ofsf - mlt_fraction) >> 1);
		v[3] += tmp;
		v[3] = imuldiv_fraction(v[3], ofsf);
		v[3] += v[0];
		*dest++ = v[3];		
#endif
	}
	resrc->offset = prec_offset + (splen_t)ofs;
	*out_count = i;
    return dest;
}
#endif

static void lao_rs_plain(Voice *vp, DATA_T *dest, int32 count)
{
	/* Play sample until end, then free the voice. */
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0, j = 0;	
	
	if (resrc->increment < 0) resrc->increment = -resrc->increment; /* In case we're coming out of a bidir loop */
	j = PRECALC_LOOP_COUNT(resrc->offset, resrc->data_length, resrc->increment) + 1; // safe end+128 sample
	if (j > count) {j = count;}
	else if(j < 0) {j = 0;}	
	if((resrc->offset >> FRACTION_BITS) >= 1)
		dest = resample_lagrange_multi(vp, dest, j, &i);
	for(; i < j; i++) {
		*dest++ = resample_lagrange_single(vp);
		resrc->offset += resrc->increment;
	}
	for(; i < count; i++) { *dest++ = 0; }
	if (resrc->offset >= resrc->data_length)
		vp->finish_voice = 1;
}

static void lao_rs_loop(Voice *vp, DATA_T *dest, int32 count)
{
	/* Play sample until end-of-loop, skip back and continue. */
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0, j = 0;
	
	if((resrc->offset >> FRACTION_BITS) >= 1){
		j = PRECALC_LOOP_COUNT(resrc->offset, resrc->loop_end, resrc->increment) - 4; // 4point interpolation
		if (j > count) {j = count;}
		else if(j < 0) {j = 0;}
		dest = resample_lagrange_multi(vp, dest, j, &i);
	}
	for(; i < count; i++) {
		*dest++ = resample_lagrange_single(vp);
		resrc->offset += resrc->increment;
		while(resrc->offset >= resrc->loop_end)
			resrc->offset -= resrc->loop_end - resrc->loop_start;
		/* The loop may not be longer than an increment. */
	}
}

static void lao_rs_bidir(Voice *vp, DATA_T *dest, int32 count)
{
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0, j = 0;	

	if ((resrc->offset >> FRACTION_BITS) >= 1 && resrc->increment > 0){
		j = PRECALC_LOOP_COUNT(resrc->offset, resrc->loop_end, resrc->increment) - 4; // 4point interpolation
		if (j > count) {j = count;}
		else if(j < 0) {j = 0;}
		dest = resample_lagrange_multi(vp, dest, j, &i);
	}
	for(; i < count; i++) {
		*dest++ = resample_lagrange_single(vp);
		resrc->offset += resrc->increment;
		if(resrc->increment > 0){
			if(resrc->offset >= resrc->loop_end){
				resrc->offset = (resrc->loop_end << 1) - resrc->offset;
				resrc->increment = -resrc->increment;
			}
		}else{
			if(resrc->offset <= resrc->loop_start){
				resrc->offset = (resrc->loop_start << 1) - resrc->offset;
				resrc->increment = -resrc->increment;
			}
		}
	}
}

static inline void resample_voice_lagrange_optimize(Voice *vp, DATA_T *ptr, int32 count)
{
    int mode = vp->sample->modes;
	
	if(vp->resrc.plain_flag){ /* no loop */ /* else then loop */ 
		vp->resrc.mode = RESAMPLE_MODE_PLAIN;	/* no loop */
		lao_rs_plain(vp, ptr, count);	/* no loop */
	}else if(!(mode & MODES_ENVELOPE) && (vp->status & (VOICE_OFF | VOICE_DIE))){ /* no env */
		vp->resrc.plain_flag = 1; /* lock no loop */
		vp->resrc.mode = RESAMPLE_MODE_PLAIN;	/* no loop */
		lao_rs_plain(vp, ptr, count);	/* no loop */
	}else if(mode & MODES_RELEASE && (vp->status & VOICE_OFF)){ /* release sample */
		vp->resrc.plain_flag = 1; /* lock no loop */
		vp->resrc.mode = RESAMPLE_MODE_PLAIN;	/* no loop */
		lao_rs_plain(vp, ptr, count);	/* no loop */
	}else if(mode & MODES_PINGPONG){ /* Bidirectional */
		vp->resrc.mode = RESAMPLE_MODE_BIDIR_LOOP;	/* Bidirectional loop */
		lao_rs_bidir(vp, ptr, count);	/* Bidirectional loop */
	}else {
		vp->resrc.mode = RESAMPLE_MODE_LOOP;	/* loop */
		lao_rs_loop(vp, ptr, count);	/* loop */
	}		
}
#endif /* optimize lagrange resample */


/*************** optimize lagrange float resample ***********************/
#if defined(PRECALC_LOOPS)

static inline DATA_T resample_lagrange_float_single(Voice *vp)
{		
	float *src = (float *)vp->sample->data;
	const resample_rec_t *resrc = &vp->resrc;
    fract_t ofsf = resrc->offset & FRACTION_MASK;
    const spos_t ofsls = resrc->loop_start >> FRACTION_BITS;
    const spos_t ofsle = resrc->loop_end >> FRACTION_BITS;
	const spos_t ofsi = resrc->offset >> FRACTION_BITS;
    spos_t ofstmp, len;
    FLOAT_T v[4], tmp;
	int32 vi[4], tmpi;
	int32 i, dir;

	switch(resrc->mode){
	case RESAMPLE_MODE_PLAIN:
		if(ofsi < 1)
			goto do_linear;
		break; // normal
	case RESAMPLE_MODE_LOOP:
		if(ofsi < ofsls){
			if(ofsi < 1)
				goto do_linear;
			if((ofsi + 2) < ofsle)
				break; // normal
		}else if(((ofsi + 2) < ofsle) && ((ofsi - 1) >= ofsls))
			break; // normal		
		len = ofsle - ofsls; // loop_length
		ofstmp = ofsi - 1;
		if(ofstmp < ofsls) {ofstmp += len;} // if loop_length == data_length need			
		for(i = 0; i < 4; i++){
			v[i] = src[ofstmp];			
			if((++ofstmp) > ofsle) {ofstmp -= len;} // -= loop_length , jump loop_start
		}
		goto loop_ofs;
		break;
	case RESAMPLE_MODE_BIDIR_LOOP:			
		if(resrc->increment >= 0){ // normal dir
			if(ofsi < ofsls){
				if(ofsi < 1)
					goto do_linear;
				if((ofsi + 2) < ofsle)
					break; // normal
			}else if(((ofsi + 2) < ofsle) && ((ofsi - 1) >= ofsls))
				break; // normal
			dir = 1;
			ofstmp = ofsi - 1;
			if(ofstmp < ofsls){ // if loop_length == data_length need				
				ofstmp = (ofsls << 1) - ofstmp;
				dir = -1;
			}			
		}else{ // reverse dir
			dir = -1;
			ofstmp = ofsi + 1;
			if(ofstmp > ofsle){ // if loop_length == data_length need				
				ofstmp = (ofsle << 1) - ofstmp;
				dir = 1;
			}
			ofsf = mlt_fraction - ofsf;
		}
		for(i = 0; i < 4; i++){
			v[i] = src[ofstmp];			
			ofstmp += dir;
			if(dir < 0){ // -
				if(ofstmp <= ofsls) {dir = 1;}
			}else{ // +
				if(ofstmp >= ofsle) {dir = -1;}
			}
		}
		goto loop_ofs;
		break;
	}
normal_ofs:
	v[0] = src[ofsi - 1];
    v[1] = src[ofsi];
    v[2] = src[ofsi + 1];	
	v[3] = src[ofsi + 2];
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
loop_ofs:
	ofsf += mlt_fraction;
	tmp = v[1] - v[0];
	v[3] += -3 * v[2] + 3 * v[1] - v[0];
	v[3] *= (FLOAT_T)(ofsf - ml2_fraction) * DIV_6 * div_fraction;
	v[3] += v[2] - v[1] - tmp;
	v[3] *= (FLOAT_T)(ofsf - mlt_fraction) * DIV_2 * div_fraction;
	v[3] += tmp;
	v[3] *= (FLOAT_T)ofsf * div_fraction;
	v[3] += v[0];
	return v[3] * OUT_INT16;
do_linear:
    v[1] = src[ofsi];
	v[2] = (int32)(src[ofsi + 1]) - (int32)(src[ofsi]);
    return (v[1] + v[2] * (FLOAT_T)ofsf * div_fraction) * OUT_INT16; // FLOAT_T
#else // DATA_T_INT32
loop_ofs:
	vi[0] = v[0] * M_15BIT;
    vi[1] = v[1] * M_15BIT;
    vi[2] = v[2] * M_15BIT;
	vi[3] = v[3] * M_15BIT;
	ofsf += mlt_fraction;
	tmpi = vi[1] - vi[0];
	vi[3] += -3*vi[2] + 3*vi[1] - vi[0];
	vi[3] = imuldiv_fraction(vi[3], (ofsf - ml2_fraction) / 6);
	vi[3] += vi[2] - vi[1] - tmpi;
	vi[3] = imuldiv_fraction(vi[3], (ofsf - mlt_fraction) >> 1);
	vi[3] += tmpi;
	vi[3] = imuldiv_fraction(vi[3], ofsf);
	vi[3] += vi[0];
	return vi[3];
do_linear:
    v[1] = src[ofsi];
	v[2] = src[ofsi + 1];
	vi[0] = v[0] * M_15BIT;
    vi[1] = v[1] * M_15BIT;
	return v[1] + imuldiv_fraction(vi[2] - vi[1], ofsf);
#endif
}

#if (USE_X86_EXT_INTRIN >= 3)
// offset:int32*4*2, resamp:float*4*2 2set
// ループ内部のoffset計算をint32値域にする , (sample_increment * (req_count+1)) < int32 max
static inline DATA_T *resample_lagrange_float_multi(Voice *vp, DATA_T *dest, int32 req_count, int32 *out_count)
{
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0;
	const int32 req_count_mask = ~(0x7);
	const int32 count = req_count & req_count_mask;
	splen_t prec_offset = resrc->offset & INTEGER_MASK;
	float *src = (float *)vp->sample->data + (prec_offset >> FRACTION_BITS);
	const int32 start_offset = (int32)(resrc->offset - prec_offset); // offset計算をint32値域にする(SIMD用
	const int32 inc = resrc->increment;
	const __m128i vinc = _mm_set1_epi32(inc * 8), vfmask = _mm_set1_epi32((int32)FRACTION_MASK);
	__m128i vofs1 = _mm_add_epi32(_mm_set1_epi32(start_offset), _mm_set_epi32(inc * 3, inc * 2, inc, 0));
	__m128i vofs2 = _mm_add_epi32(vofs1, _mm_set1_epi32(inc * 4));
	const __m128 vdivf = _mm_set1_ps(div_fraction);	
	const __m128 vfrac_6 = _mm_set1_ps(div_fraction * DIV_6);
	const __m128 vfrac_2 = _mm_set1_ps(div_fraction * DIV_2);
	const __m128 v3n = _mm_set1_ps(-3);
	const __m128 v3p = _mm_set1_ps(3);
	const __m128i vfrac = _mm_set1_epi32(mlt_fraction);
	const __m128i vfrac2 = _mm_set1_epi32(ml2_fraction);
	const __m128 vec_divo = _mm_set1_ps(M_15BIT);
	for(; i < count; i += 8) {
	__m128i vofsi1 = _mm_srli_epi32(vofs1, FRACTION_BITS); // ofsi = ofs >> FRACTION_BITS
	__m128i vofsi2 = _mm_srli_epi32(vofs2, FRACTION_BITS); // ofsi = ofs >> FRACTION_BITS
	__m128 vin1 = _mm_loadu_ps(&src[MM_EXTRACT_I32(vofsi1,0) - 1]); // ofsi-1~ofsi+2をロード [v11v12v13v14]
	__m128 vin2 = _mm_loadu_ps(&src[MM_EXTRACT_I32(vofsi1,1) - 1]); // 次周サンプルも同じ [v21v22v23v24]
	__m128 vin3 = _mm_loadu_ps(&src[MM_EXTRACT_I32(vofsi1,2) - 1]); // 次周サンプルも同じ [v31v32v33v34]
	__m128 vin4 = _mm_loadu_ps(&src[MM_EXTRACT_I32(vofsi1,3) - 1]); // 次周サンプルも同じ [v41v42v43v44]	
	__m128 vin5 = _mm_loadu_ps(&src[MM_EXTRACT_I32(vofsi2,0) - 1]); // 次周サンプルも同じ [v51v52v53v54]
	__m128 vin6 = _mm_loadu_ps(&src[MM_EXTRACT_I32(vofsi2,1) - 1]); // 次周サンプルも同じ [v61v62v63v64]
	__m128 vin7 = _mm_loadu_ps(&src[MM_EXTRACT_I32(vofsi2,2) - 1]); // 次周サンプルも同じ [v71v72v73v74]
	__m128 vin8 = _mm_loadu_ps(&src[MM_EXTRACT_I32(vofsi2,3) - 1]); // 次周サンプルも同じ [v81v82v83v84]	
    __m128 vin12a = _mm_shuffle_ps(vin1, vin2, 0x44); // [v11,v12,v21,v22]
    __m128 vin12b = _mm_shuffle_ps(vin1, vin2, 0xEE); // [v13,v14,v23,v24]
    __m128 vin34a = _mm_shuffle_ps(vin3, vin4, 0x44); // [v31,v32,v41,v42]
    __m128 vin34b = _mm_shuffle_ps(vin3, vin4, 0xEE); // [v33,v34,v43,v44]
    __m128 vin56a = _mm_shuffle_ps(vin5, vin6, 0x44); // [v51,v52,v61,v62]
    __m128 vin56b = _mm_shuffle_ps(vin5, vin6, 0xEE); // [v53,v54,v63,v64]
    __m128 vin78a = _mm_shuffle_ps(vin7, vin8, 0x44); // [v71,v72,v81,v82]
    __m128 vin78b = _mm_shuffle_ps(vin7, vin8, 0xEE); // [v73,v74,v83,v84]
    __m128 vv01 = _mm_shuffle_ps(vin12a, vin34a, 0x88); // [v11,v21,v31,v41]
    __m128 vv11 = _mm_shuffle_ps(vin12a, vin34a, 0xDD); // [v12,v22,v32,v42]
    __m128 vv21 = _mm_shuffle_ps(vin12b, vin34b, 0x88); // [v13,v23,v33,v43]
    __m128 vv31 = _mm_shuffle_ps(vin12b, vin34b, 0xDD); // [v14,v24,v34,v44]
    __m128 vv02 = _mm_shuffle_ps(vin56a, vin78a, 0x88); // [v51,v61,v71,v81]
    __m128 vv12 = _mm_shuffle_ps(vin56a, vin78a, 0xDD); // [v52,v62,v72,v82]
    __m128 vv22 = _mm_shuffle_ps(vin56b, vin78b, 0x88); // [v53,v63,v73,v83]
    __m128 vv32 = _mm_shuffle_ps(vin56b, vin78b, 0xDD); // [v54,v64,v74,v84]
	__m128i vofsf1 = _mm_add_epi32(_mm_and_si128(vofs1, vfmask), vfrac); // ofsf = (ofs & FRACTION_MASK) + mlt_fraction;
	__m128i vofsf2 = _mm_add_epi32(_mm_and_si128(vofs2, vfmask), vfrac); // ofsf = (ofs & FRACTION_MASK) + mlt_fraction;
	__m128 vtmp1 = _mm_sub_ps(vv11, vv01); // tmp = v[1] - v[0];
	__m128 vtmp2 = _mm_sub_ps(vv12, vv02); // tmp = v[1] - v[0];
	__m128 vtmpx11, vtmpx12, vtmpx21, vtmpx22, vtmpx31, vtmpx32, vtmpx41, vtmpx42;
	__m128 vtmpi1, vtmpi2;
	vv31 = _mm_add_ps(vv31, _mm_sub_ps(MM_FMA2_PS(vv21, v3n, vv11, v3p), vv01)); // v[3] += -3 * v[2] + 3 * v[1] - v[0];
	vv32 = _mm_add_ps(vv32, _mm_sub_ps(MM_FMA2_PS(vv22, v3n, vv12, v3p), vv02)); // v[3] += -3 * v[2] + 3 * v[1] - v[0];
	vtmpi1 = _mm_cvtepi32_ps(_mm_sub_epi32(vofsf1, vfrac2));
	vtmpi2 = _mm_cvtepi32_ps(_mm_sub_epi32(vofsf2, vfrac2));
	vtmpx11 = _mm_mul_ps(vtmpi1, vfrac_6); // tmpx1 = (float)(ofsf - ml2_fraction) * DIV_6 * div_fraction;
	vtmpx12 = _mm_mul_ps(vtmpi2, vfrac_6); // tmpx1 = (float)(ofsf - ml2_fraction) * DIV_6 * div_fraction;
	vtmpx21 = _mm_sub_ps(_mm_sub_ps(vv21, vv11), vtmp1); // tmpx2 = v[2] - v[1] - tmp);
	vtmpx22 = _mm_sub_ps(_mm_sub_ps(vv22, vv12), vtmp2); // tmpx2 = v[2] - v[1] - tmp);
	vtmpi1 = _mm_cvtepi32_ps(_mm_sub_epi32(vofsf1, vfrac));
	vtmpi2 = _mm_cvtepi32_ps(_mm_sub_epi32(vofsf2, vfrac));
	vtmpx31 = _mm_mul_ps(vtmpi1, vfrac_2); // tmpx3 = (FLOAT_T)(ofsf - mlt_fraction) * DIV_2 * div_fraction;
	vtmpx32 = _mm_mul_ps(vtmpi2, vfrac_2); // tmpx3 = (FLOAT_T)(ofsf - mlt_fraction) * DIV_2 * div_fraction;
	vtmpi1 = _mm_cvtepi32_ps(vofsf1);
	vtmpi2 = _mm_cvtepi32_ps(vofsf2);
	vtmpx41 = _mm_mul_ps(vtmpi1, vdivf); // tmpx4 = (FLOAT_T)ofsf * div_fraction;
	vtmpx42 = _mm_mul_ps(vtmpi2, vdivf); // tmpx4 = (FLOAT_T)ofsf * div_fraction;
	vv31 = MM_FMA_PS(vv31, vtmpx11, vtmpx21); // v[3] = v[3] * tmpx1 + tmpx2
	vv32 = MM_FMA_PS(vv32, vtmpx12, vtmpx22); // v[3] = v[3] * tmp1 + tmp2
	vv31 = MM_FMA_PS(vv31, vtmpx31, vtmp1); // v[3] = v[3] * tmpx3 + tmp;
	vv32 = MM_FMA_PS(vv32, vtmpx32, vtmp2); // v[3] = v[3] * tmpx3 + tmp;
	vv31 = MM_FMA_PS(vv31, vtmpx41, vv01); // v[3] = v[3] * tmpx4 + vv0;
	vv32 = MM_FMA_PS(vv32, vtmpx42, vv02); // v[3] = v[3] * tmpx4 + vv0;
#if defined(DATA_T_DOUBLE)
#if (USE_X86_EXT_INTRIN >= 8)	
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(vv31));
	dest += 4;
	_mm256_storeu_pd(dest, _mm256_cvtps_pd(vv32));
	dest += 4;
#else
	_mm_storeu_pd(dest, _mm_cvtps_pd(vv31));
	dest += 2;
	_mm_storeu_pd(dest, _mm_cvtps_pd(_mm_movehl_ps(vv31, vv31)));
	dest += 2;
	_mm_storeu_pd(dest, _mm_cvtps_pd(vv32));
	dest += 2;
	_mm_storeu_pd(dest, _mm_cvtps_pd(_mm_movehl_ps(vv32, vv32)));
	dest += 2;
#endif
#elif defined(DATA_T_FLOAT) // DATA_T_FLOAT
	_mm_storeu_ps(dest, vv31);
	dest += 4;
	_mm_storeu_ps(dest, vv32);
	dest += 4;
#else // DATA_T_IN32
	vv31 = _mm_mul_ps(vv31, vdivo);
	vv32 = _mm_mul_ps(vv32, vdivo);
	_mm_storeu_si128((__m128i *)dest, _mm_cvtps_epi32(vv31));
	dest += 4;
	_mm_storeu_si128((__m128i *)dest, _mm_cvtps_epi32(vv32));
	dest += 4;
#endif
	vofs1 = _mm_add_epi32(vofs1, vinc); // ofs += inc;
	vofs2 = _mm_add_epi32(vofs2, vinc); // ofs += inc;
	}
	resrc->offset = prec_offset + (splen_t)(MM_EXTRACT_I32(vofs1,0));
	*out_count = i;
    return dest;
}

#else // not use MMX/SSE/AVX 
// ループ内部のoffset計算をint32値域にする , (sample_increment * (req_count+1)) < int32 max
static inline DATA_T *resample_lagrange_float_multi(Voice *vp, DATA_T *dest, int32 req_count, int32 *out_count)
{
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0;
	splen_t prec_offset = resrc->offset & INTEGER_MASK;
	float *src = (float *)vp->sample->data + (prec_offset >> FRACTION_BITS);
	int32 ofs = (int32)(resrc->offset & FRACTION_MASK);
	int32 inc = resrc->increment;

	for(i = 0; i < req_count; i++) {
		int32 ofsi, ofsf;
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
		FLOAT_T v[4], tmp;
		ofsi = ofs >> FRACTION_BITS, ofsf = ofs & FRACTION_MASK; ofs += inc;		
		v[0] = src[ofsi - 1]; 
		v[1] = src[ofsi];
		v[2] = src[ofsi + 1];	
		v[3] = src[ofsi + 2];		
		ofsf += mlt_fraction;
		tmp = v[1] - v[0];
		v[3] += -3 * v[2] + 3 * v[1] - v[0];
		v[3] *= (FLOAT_T)(ofsf - ml2_fraction) * DIV_6 * div_fraction;
		v[3] += v[2] - v[1] - tmp;
		v[3] *= (FLOAT_T)(ofsf - mlt_fraction) * DIV_2 * div_fraction;
		v[3] += tmp;
		v[3] *= (FLOAT_T)ofsf * div_fraction;
		v[3] += v[0];
		*dest++ = v[3];
#else // DATA_T_IN32
		int32 v[4], tmp;
		ofsi = ofs >> FRACTION_BITS, ofsf = ofs & FRACTION_MASK; ofs += inc;
		v[0] = src[ofsi - 1] * M_15BIT;
		v[1] = src[ofsi] * M_15BIT;
		v[2] = src[ofsi + 1] * M_15BIT;	
		v[3] = src[ofsi + 2] * M_15BIT;			
		ofsf += mlt_fraction;
		tmp = v[1] - v[0];
		v[3] += -3*v[2] + 3*v[1] - v[0];
		v[3] = imuldiv_fraction(v[3], (ofsf - ml2_fraction) / 6);
		v[3] += v[2] - v[1] - tmp;
		v[3] = imuldiv_fraction(v[3], (ofsf - mlt_fraction) >> 1);
		v[3] += tmp;
		v[3] = imuldiv_fraction(v[3], ofsf);
		v[3] += v[0];
		*dest++ = v[3];		
#endif
	}
	resrc->offset = prec_offset + (splen_t)ofs;
	*out_count = i;
    return dest;
}
#endif

static void lao_rs_plain_float(Voice *vp, DATA_T *dest, int32 count)
{
	/* Play sample until end, then free the voice. */
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0, j = 0;	
	
	if (resrc->increment < 0) resrc->increment = -resrc->increment; /* In case we're coming out of a bidir loop */
	j = PRECALC_LOOP_COUNT(resrc->offset, resrc->data_length, resrc->increment) + 1; // safe end+128 sample
	if (j > count) {j = count;}
	else if(j < 0) {j = 0;}	
	if((resrc->offset >> FRACTION_BITS) >= 1)
		dest = resample_lagrange_float_multi(vp, dest, j, &i);
	for(; i < j; i++) {
		*dest++ = resample_lagrange_float_single(vp);
		resrc->offset += resrc->increment;
	}
	for(; i < count; i++) { *dest++ = 0; }
	if (resrc->offset >= resrc->data_length)
		vp->finish_voice = 1;
}

static void lao_rs_loop_float(Voice *vp, DATA_T *dest, int32 count)
{
	/* Play sample until end-of-loop, skip back and continue. */
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0, j = 0;
	
	if((resrc->offset >> FRACTION_BITS) >= 1){
		j = PRECALC_LOOP_COUNT(resrc->offset, resrc->loop_end, resrc->increment) - 4; // 4point interpolation
		if (j > count) {j = count;}
		else if(j < 0) {j = 0;}
		dest = resample_lagrange_float_multi(vp, dest, j, &i);
	}
	for(; i < count; i++) {
		*dest++ = resample_lagrange_float_single(vp);
		resrc->offset += resrc->increment;
		while(resrc->offset >= resrc->loop_end)
			resrc->offset -= resrc->loop_end - resrc->loop_start;
		/* The loop may not be longer than an increment. */
	}
}

static void lao_rs_bidir_float(Voice *vp, DATA_T *dest, int32 count)
{
	resample_rec_t *resrc = &vp->resrc;
	int32 i = 0, j = 0;	

	if ((resrc->offset >> FRACTION_BITS) >= 1 && resrc->increment > 0){
		j = PRECALC_LOOP_COUNT(resrc->offset, resrc->loop_end, resrc->increment) - 4; // 4point interpolation
		if (j > count) {j = count;}
		else if(j < 0) {j = 0;}
		dest = resample_lagrange_float_multi(vp, dest, j, &i);
	}
	for(; i < count; i++) {
		*dest++ = resample_lagrange_float_single(vp);
		resrc->offset += resrc->increment;
		if(resrc->increment > 0){
			if(resrc->offset >= resrc->loop_end){
				resrc->offset = (resrc->loop_end << 1) - resrc->offset;
				resrc->increment = -resrc->increment;
			}
		}else{
			if(resrc->offset <= resrc->loop_start){
				resrc->offset = (resrc->loop_start << 1) - resrc->offset;
				resrc->increment = -resrc->increment;
			}
		}
	}
}

static inline void resample_voice_lagrange_float_optimize(Voice *vp, DATA_T *ptr, int32 count)
{
    int mode = vp->sample->modes;
	
	if(vp->resrc.plain_flag){ /* no loop */ /* else then loop */ 
		vp->resrc.mode = RESAMPLE_MODE_PLAIN;	/* no loop */
		lao_rs_plain_float(vp, ptr, count);	/* no loop */
	}else if(!(mode & MODES_ENVELOPE) && (vp->status & (VOICE_OFF | VOICE_DIE))){ /* no env */
		vp->resrc.plain_flag = 1; /* lock no loop */
		vp->resrc.mode = RESAMPLE_MODE_PLAIN;	/* no loop */
		lao_rs_plain_float(vp, ptr, count);	/* no loop */
	}else if(mode & MODES_RELEASE && (vp->status & VOICE_OFF)){ /* release sample */
		vp->resrc.plain_flag = 1; /* lock no loop */
		vp->resrc.mode = RESAMPLE_MODE_PLAIN;	/* no loop */
		lao_rs_plain_float(vp, ptr, count);	/* no loop */
	}else if(mode & MODES_PINGPONG){ /* Bidirectional */
		vp->resrc.mode = RESAMPLE_MODE_BIDIR_LOOP;	/* Bidirectional loop */
		lao_rs_bidir_float(vp, ptr, count);	/* Bidirectional loop */
	}else {
		vp->resrc.mode = RESAMPLE_MODE_LOOP;	/* loop */
		lao_rs_loop_float(vp, ptr, count);	/* loop */
	}		
}
#endif /* optimize lagrange float resample */



/*************** resampling with fixed increment *****************/
///r
static void rs_plain_c(int v, DATA_T *ptr, int32 count)
{
    Voice *vp = &voice[v];
    DATA_T *dest = ptr + vp->resrc.buffer_offset;
	cache_t *src = (cache_t *)vp->sample->data;
	int32 count2 = count;
    splen_t ofs, i, le;
	
    le = vp->sample->loop_end >> FRACTION_BITS;
    ofs = vp->resrc.offset >> FRACTION_BITS;

    i = ofs + count2;
    if(i > le)
		i = le;
	count2 = i - ofs;

	for (i = 0; i < count2; i++) {
		dest[i] = src[i + ofs];
	}
	for (; i < count; i++) {
		vp->finish_voice = 1;
		dest[i] = 0;
	}	
	ofs += count2;
	vp->resrc.offset = ofs << FRACTION_BITS;
}
///r
static void rs_plain(int v, DATA_T *ptr, int32 count)
{
  /* Play sample until end, then free the voice. */
  Voice *vp = &voice[v];
  DATA_T *dest = ptr;
	sample_t *src = vp->sample->data;
	int data_type = vp->sample->data_type;
  splen_t
    ofs = vp->resrc.offset,
    ls = 0,
    le = vp->sample->data_length;
  int32 incr = vp->resrc.increment;
#ifdef PRECALC_LOOPS
  int32 i = 0, j;
#endif

	if(vp->cache && incr == (1 << FRACTION_BITS)){
		rs_plain_c(v, ptr, count);
		return;
	}	

#ifdef PRECALC_LOOPS
	if (incr < 0) incr = -incr; /* In case we're coming out of a bidir loop */
  /* Precalc how many times we should go through the loop.
     NOTE: Assumes that incr > 0 and that ofs <= le */
	j = PRECALC_LOOP_COUNT(ofs, le, incr);
  	if (j > count) {j = count;}
	else if(j < 0) {j = 0;}	
	for(i = 0; i < j; i++) {
      RESAMPLATION;
      ofs += incr;
    }
	for (; i < count; i++) {
		*dest++ = 0;
		vp->finish_voice = 1;
	}	
#else /* PRECALC_LOOPS */
	while (count--)
	{
		if (ofs >= le){
			*dest++ = 0;
			vp->finish_voice = 1;
		}else {
			RESAMPLATION;
			ofs += incr;
		}
	}
#endif /* PRECALC_LOOPS */

  vp->resrc.offset = ofs; /* Update offset */
}
static void rs_loop_c(Voice *vp, DATA_T *ptr, int32 count)
{
  splen_t
		ofs = vp->resrc.offset >> FRACTION_BITS,
		le = vp->sample->loop_end >> FRACTION_BITS,
		ll = le - (vp->sample->loop_start >> FRACTION_BITS);

	DATA_T *dest = ptr;
	cache_t *src = (cache_t *)vp->sample->data;
	int32 i, j;

// ERROR loop_start = 4215529472 
	if(ll < 0)
	{	
		vp->sample->loop_start = 0;
		ll = le - (vp->sample->loop_start >> FRACTION_BITS);
	}	

	while(count){
		while(ofs >= le)
			ofs -= ll;
		/* Precalc how many times we should go through the loop */
		i = le - ofs;
		if(i > count)
			i = count;
		count -= i;
		for (j = 0; j < i; j++) {
			dest[j] = src[j + ofs];
		}
		dest += i;
		ofs += i;
	}

	vp->resrc.offset = ofs << FRACTION_BITS;
}
///r
static void rs_loop(Voice *vp, DATA_T *ptr, int32 count)
{
  /* Play sample until end-of-loop, skip back and continue. */
  splen_t
    ofs = vp->resrc.offset,
    ls, le, ll;
  DATA_T *dest = ptr;
	sample_t *src = vp->sample->data;
	int data_type = vp->sample->data_type;
#ifdef PRECALC_LOOPS
  int32 i, j;
#endif
  int32 incr = vp->resrc.increment;

	if(vp->cache && incr == (1 << FRACTION_BITS)){
		rs_loop_c(vp, ptr, count);
		return;
	}
	
	ls = vp->sample->loop_start;
	le = vp->sample->loop_end;
	ll = le - ls;

#ifdef PRECALC_LOOPS
	while (count)
    {
		while (ofs >= le)	{ofs -= ll;}
		/* Precalc how many times we should go through the loop */
		i = PRECALC_LOOP_COUNT(ofs, le, incr);
		if (i > count) {
			i = count;
			count = 0;
		}else{
			count -= i;
		}
		for(j = 0; j < i; j++) {
			RESAMPLATION;
			ofs += incr;
		}
    }
#else
	while (count--)
	{
		RESAMPLATION;
		ofs += incr;
		if (ofs >= le)
			ofs -= ll; /* Hopefully the loop is longer than an increment. */
	}
#endif

  vp->resrc.offset = ofs; /* Update offset */
}
///r
static void rs_bidir(Voice *vp, DATA_T *ptr, int32 count)
{
  splen_t
    ofs = vp->resrc.offset,
    le = vp->sample->loop_end,
    ls = vp->sample->loop_start;
  DATA_T *dest = ptr;
	sample_t *src = vp->sample->data;
	int data_type = vp->sample->data_type;
  int32 incr = vp->resrc.increment;

#ifdef PRECALC_LOOPS
  splen_t
    le2 = le << 1,
    ls2 = ls << 1;
  int32 i, j;

  /* Play normally until inside the loop region */
  

	if (incr > 0 && ofs < ls){
		/* NOTE: Assumes that incr > 0, which is NOT always the case
		when doing bidirectional looping.  I have yet to see a case
		where both ofs <= ls AND incr < 0, however. */
		i = PRECALC_LOOP_COUNT(ofs, ls, incr);
		if (i > count)
		{
			i = count;
			count = 0;
		}else
			count -= i;
		for(j = 0; j < i; j++){
			RESAMPLATION;
			ofs += incr;
		}
	}

  /* Then do the bidirectional looping */

	while(count){
		/* Precalc how many times we should go through the loop */
		i = PRECALC_LOOP_COUNT(ofs, incr > 0 ? le : ls, incr);
		if (i > count){
			i = count;
			count = 0;
		}
		else
			count -= i;
		for(j = 0; j < i; j++){
			RESAMPLATION;
			ofs += incr;
		}
		if(ofs >= 0 && ofs >= le){
			/* fold the overshoot back in */
			ofs = le2 - ofs;
			incr *= -1;
			vp->resrc.increment = incr;
		}else if (ofs <= 0 || ofs <= ls){
			ofs = ls2 - ofs;
			incr *= -1;
			vp->resrc.increment = incr;
		}
	}

#else /* PRECALC_LOOPS */
  /* Play normally until inside the loop region */

  if (ofs < ls)
    {
      while (count--)
	{
	  RESAMPLATION;
	  ofs += incr;
	  if (ofs >= ls)
	    break;
	}
    }

  /* Then do the bidirectional looping */

  if (count > 0)
    while (count--)
      {
	RESAMPLATION;
	ofs += incr;
	if (ofs >= le)
	  {
	    /* fold the overshoot back in */
	    ofs = le - (ofs - le);
	    incr = -incr;
		vp->resrc.increment = incr;
	  }
	else if (ofs <= ls)
	  {
	    ofs = ls + (ls - ofs);
	    incr = -incr;
		vp->resrc.increment = incr;
	  }
      }
#endif /* PRECALC_LOOPS */
  vp->resrc.increment = incr;
  vp->resrc.offset = ofs; /* Update offset */
}


/* interface function */
///r
void resample_voice(int v, DATA_T *ptr, int32 count)
{
    Voice *vp = &voice[v];
    int mode;
	int32 i = 0;
	int32 a;	

	if(!opt_resample_over_sampling && vp->sample->sample_rate == play_mode->rate &&
       vp->sample->root_freq == get_note_freq(vp->sample, vp->sample->note_to_use) &&
       vp->frequency == vp->orig_frequency)
    {
		int32 count2 = count;
		splen_t ofs = vp->resrc.offset >> FRACTION_BITS; /* Kind of silly to use FRACTION_BITS here... */

		/* Pre-resampled data -- just update the offset and check if
		   we're out of data. */
		if(count2 >= (vp->sample->data_length >> FRACTION_BITS) - ofs){
			/* Note finished. Free the voice. */
			vp->finish_voice = 1;
			/* Let the caller know how much data we had left */
			count2 = (int32)((vp->sample->data_length >> FRACTION_BITS) - ofs);
		}else
			vp->resrc.offset += ((splen_t)count2 << FRACTION_BITS);

		switch(vp->sample->data_type){
		case SAMPLE_TYPE_INT16:
			for (i = 0; i < count2; i++)
				ptr[i] = vp->sample->data[i + ofs] * OUT_INT16; // data[i+ofs]
			break;
		case SAMPLE_TYPE_INT32:
			for (i = 0; i < count2; i++)
				ptr[i] = *((int32 *)vp->sample->data + i + ofs) * OUT_INT32; // data[i+ofs]
			break;
		case SAMPLE_TYPE_FLOAT:
			for (i = 0; i < count2; i++)
				ptr[i] = *((float *)vp->sample->data + i + ofs) * OUT_FLOAT; // data[i+ofs]
			break;
		case SAMPLE_TYPE_DOUBLE:
			for (i = 0; i < count2; i++)
				ptr[i] = *((double *)vp->sample->data + i + ofs) * OUT_DOUBLE; // data[i+ofs]
			break;
		default:
			ctl->cmsg(CMSG_INFO, VERB_NORMAL, "invalid cache or pre_resample data_type %d", vp->sample->data_type);
			break;
		}
		for (; i < count; i++)
			ptr[i] = 0;
		return;
    }	
	
	// recalc increment
	a = ((double)vp->sample->sample_rate * (double)vp->frequency)
			/ (double)vp->sample->root_freq * div_playmode_rate * div_over_sampling_ratio * mlt_fraction + 0.5;
	/* need to preserve the loop direction */
	vp->resrc.increment = (vp->resrc.increment >= 0) ? a : -a;
	
#if defined(PRECALC_LOOPS)
	if(opt_resample_type == RESAMPLE_LINEAR){
		if(vp->sample->data_type == SAMPLE_TYPE_INT16){
			resample_voice_linear_optimize(vp, ptr, count);
			return;
		}else if(vp->sample->data_type == SAMPLE_TYPE_INT32 && !opt_pre_resamplation){
			resample_voice_linear_int32_optimize(vp, ptr, count);
			return;
		}else if(vp->sample->data_type == SAMPLE_TYPE_FLOAT && !opt_pre_resamplation){
			resample_voice_linear_float_optimize(vp, ptr, count);
			return;
		}
	} else if (opt_resample_type == RESAMPLE_LAGRANGE){
		if(vp->sample->data_type == SAMPLE_TYPE_INT16){
			resample_voice_lagrange_optimize(vp, ptr, count);
			return;
		}else if(vp->sample->data_type == SAMPLE_TYPE_FLOAT && !opt_pre_resamplation){
			resample_voice_lagrange_float_optimize(vp, ptr, count);
			return;
		}
	}
#endif
	
    mode = vp->sample->modes;
	if(vp->resrc.plain_flag){ /* no loop */ /* else then loop */ 		
		vp->resrc.mode = RESAMPLE_MODE_PLAIN;	/* no loop */
		rs_plain(v, ptr, count);
	}else if(!(mode & MODES_ENVELOPE) && (vp->status & (VOICE_OFF | VOICE_DIE))){ /* no env */
		vp->resrc.plain_flag = 1; /* lock no loop */
		vp->cache = NULL;
		vp->resrc.mode = RESAMPLE_MODE_PLAIN;	/* no loop */
		rs_plain(v, ptr, count);
	}else if(mode & MODES_RELEASE && (vp->status & VOICE_OFF)){ /* release sample */
		vp->resrc.plain_flag = 1; /* lock no loop */
		vp->cache = NULL;
		vp->resrc.mode = RESAMPLE_MODE_PLAIN;	/* no loop , release sample */
		rs_plain(v, ptr, count);
	}else if(mode & MODES_PINGPONG){ /* Bidirectional */
		vp->cache = NULL;
		vp->resrc.mode = RESAMPLE_MODE_BIDIR_LOOP;	/* Bidir loop */
		rs_bidir(vp, ptr, count);
	}else {
		vp->resrc.mode = RESAMPLE_MODE_LOOP;	/* loop */
		rs_loop(vp, ptr, count);
	}
}

void init_voice_resample(int v)
{
	Voice *vp = voice + v; 
	vp->resrc.offset = vp->reserve_offset + vp->sample->offset;
	vp->resrc.increment = 0; /* make sure it isn't negative */
	vp->resrc.loop_start = vp->sample->loop_start;
	vp->resrc.loop_end = vp->sample->loop_end;
	vp->resrc.data_length = vp->sample->data_length;	
	vp->resrc.mode = RESAMPLE_MODE_PLAIN; // change resample_voice()	
	vp->resrc.plain_flag = !(vp->sample->modes & MODES_LOOPING) ? 1 : 0;
	// newton
#ifdef RESAMPLE_NEWTON_VOICE
	vp->resrc.newt_old_trunc_x = -1;
	vp->resrc.newt_grow = -1;
	vp->resrc.newt_old_src = NULL;
#endif
	// set_resamplation
    if (reduce_quality_flag) {
#ifndef FIXED_RESAMPLATION
	if (opt_resample_type != RESAMPLE_NONE)
	    vp->resrc.current_resampler = resamplers[vp->sample->data_type][RESAMPLE_LINEAR];
	else
	    vp->resrc.current_resampler = resamplers[vp->sample->data_type][RESAMPLE_NONE];
#else
		vp->resrc.current_resampler = resamplers[vp->sample->data_type][RESAMPLE_NONE];
#endif
    } else {
		vp->resrc.current_resampler = resamplers[vp->sample->data_type][opt_resample_type];
    }
}

///r
void pre_resample(Sample * sp)
{
	double ratio;
	splen_t ofs, newlen;
	int32 i, count, incr, freq;
	resample_rec_t resrc;
	pre_resample_t *newdata, *dest;
	sample_t *src = sp->data;
	int data_type = sp->data_type;
	int32 bytes;

	ctl->cmsg(CMSG_INFO, VERB_DEBUG, " * pre-resampling for note %d (%s%d)",
		sp->note_to_use, note_name[sp->note_to_use % 12], (sp->note_to_use & 0x7F) / 12);

	freq = get_note_freq(sp, sp->note_to_use);
	ratio = (double)sp->root_freq * (double)play_mode->rate / ((double)sp->sample_rate * (double)freq);

	if((int64)sp->data_length * ratio >= 0x7fffffffL)
	{
		/* Too large to compute */
		ctl->cmsg(CMSG_INFO, VERB_DEBUG, " *** Can't pre-resampling for note %d", sp->note_to_use);
		return;
	}
	newlen = (splen_t)(sp->data_length * ratio);
	count = (newlen >> FRACTION_BITS);
	ofs = incr = (sp->data_length - 1) / (count - 1);

	if((double)newlen + ofs >= 0x7fffffffL)
	{
		/* Too large to compute */
		ctl->cmsg(CMSG_INFO, VERB_DEBUG, " *** Can't pre-resampling for note %d", sp->note_to_use);
		return;
	}

	bytes = sizeof(pre_resample_t) * (count + 128); // def +1 noise ?
	dest = newdata = (pre_resample_t *)safe_large_malloc(bytes);
	memset(newdata, 0, bytes);

	resrc.loop_start = 0;
	resrc.loop_end = sp->data_length;
	resrc.data_length = sp->data_length;
	resrc.increment = incr;
	resrc.mode = RESAMPLE_MODE_PLAIN; // plain
#ifdef RESAMPLE_NEWTON_VOICE
	resrc.newt_old_trunc_x = -1;
	resrc.newt_grow = -1;
	resrc.newt_old_src = NULL;
#endif

	// set_resamplation
	current_resampler = resamplers[data_type][opt_resample_type];
	
	/* Since we're pre-processing and this doesn't have to be done in
		real-time, we go ahead and do the higher order interpolation. */
	switch(data_type){
	case SAMPLE_TYPE_INT16:
		*dest++ = (FLOAT_T)(*sp->data) * OUT_INT16; // data[0] // -1.0 ~ 1.0
		break;
	case SAMPLE_TYPE_INT32: // DATA_T_INT32 not use
		*dest++ = (FLOAT_T)(*(int32 *)sp->data) * OUT_INT32; // data[0] // -1.0 ~ 1.0
		break;
	case SAMPLE_TYPE_FLOAT:
		*dest++ = *(float *)sp->data * OUT_FLOAT; // data[0] // -1.0 ~ 1.0
		break;
	case SAMPLE_TYPE_DOUBLE:
		*dest++ = *(double *)sp->data * OUT_DOUBLE; // data[0] // -1.0 ~ 1.0
		break;
	default:
		ctl->cmsg(CMSG_INFO, VERB_NORMAL, "invalid data_type %d", data_type);
		break;
	}
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
	for(i = 1; i < count; i++) { PRE_RESAMPLATION; ofs += incr;}
#else // DATA_T_INT32
#if defined(LOOKUP_HACK)
	for(i = 1; i < count; i++) {
		int32 x = resamplers[data_type][opt_resample_type](src, ofs, &resrc);
		*dest++ = CLIP_INT8(x);
		ofs += incr;
	}
#elif 0 // pre_resample_t int16
	for(i = 1; i < count; i++) {
		int32 x = resamplers[data_type][opt_resample_type](src, ofs, &resrc);
		*dest++ = CLIP_INT16(x);
		ofs += incr;
	}
#else // pre_resample_t int32
	for(i = 1; i < count; i++) { PRE_RESAMPLATION; ofs += incr;}
#endif
#endif
	safe_free(sp->data);
	sp->data = (sample_t *) newdata;
	sp->data_type = PRE_RESAMPLE_DATA_TYPE;
	sp->data_length = newlen;
	sp->loop_start = (splen_t)(sp->loop_start * ratio);
	sp->loop_end = (splen_t)(sp->loop_end * ratio);
///r
	sp->root_freq_org = sp->root_freq;
	sp->sample_rate_org = sp->sample_rate;
	sp->root_freq = freq;
	sp->sample_rate = play_mode->rate;
//	sp->low_freq = freq_table[0];
//	sp->high_freq = freq_table[127];
}

