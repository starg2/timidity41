/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2009 Masanao Izumo <iz@onicos.co.jp>
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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    thread_effect.c -- random stuff in need of rearrangement
*/


#include "thread.h"



// thread_playmidi
extern int cdmt_buf_i;
extern int cdmt_buf_o;
extern int cdmt_ofs_0;
extern int cdmt_ofs_1;
extern int cdmt_ofs_2;
extern int cdmt_ofs_3;

static ALIGN DATA_T eq_buffer_thread[4][AUDIO_BUFFER_SIZE * 2];
static ALIGN DATA_T chorus_effect_buffer_thread[4][AUDIO_BUFFER_SIZE * 2];
static ALIGN DATA_T delay_effect_buffer_thread[4][AUDIO_BUFFER_SIZE * 2];
static ALIGN DATA_T reverb_effect_buffer_thread[4][AUDIO_BUFFER_SIZE * 2];

static ALIGN DATA_T insertion_effect_buffer_sub[2][AUDIO_BUFFER_SIZE * 2];
static ALIGN DATA_T mfx_effect_buffer_sub[2][SD_MFX_EFFECT_NUM][AUDIO_BUFFER_SIZE * 2];
static ALIGN DATA_T eq_effect_buffer_sub[2][AUDIO_BUFFER_SIZE * 2];
static ALIGN DATA_T chorus_effect_buffer_sub[2][AUDIO_BUFFER_SIZE * 2];
static ALIGN DATA_T delay_effect_buffer_sub[2][AUDIO_BUFFER_SIZE * 2];
static ALIGN DATA_T reverb_effect_buffer_sub[2][AUDIO_BUFFER_SIZE * 2];




#if (OPT_MODE == 1) && !defined(DATA_T_DOUBLE) && !defined(DATA_T_FLOAT) /* fixed-point implementation */
#if defined(IX86CPU) && defined(SUPPORT_ASM_INTEL)
static inline void mix_buffer_thread(DATA_T *dest, DATA_T *src, int32 count)
{
	_asm {
		mov		ecx, [count]
		mov		esi, [src]
		test		ecx, ecx
		jz		short L2
		mov		edi, [dest]
L1:		mov		eax, [esi]
		mov		ebx, [edi]
		add		esi, 4
		add		ebx, eax
		mov		[edi], ebx
		add		edi, 4
		dec		ecx
		jnz		L1
L2:
	}
}
#elif defined(IX86CPU) && defined(SUPPORT_ASM_AT_AND_T)
static inline void mix_buffer_thread(DATA_T *dest, DATA_T *src, int32 count)
{
	__asm__("\
		testl		%%ecx, %%ecx;\
		jz		L2_EQGS;\
L1_EQGS:	movl		(%%esi), %%eax;\
		movl		(%%edi), %%ebx;\
		addl		$4, %%esi;\
		addl		%%eax, %%ebx;\
		movl		%%ebx, (%%edi);\
		addl		$4, %%edi;\
		decl		%%ecx;\
		jnz		L1_EQGS;\
L2_EQGS:	"
		: "=S"(src), "=c"(count), "=D"(dest)
		: "0"(src), "1"(count), "2"(dest)
		: "eax", "ebx", "memory"
	);
}
#else
static inline void mix_buffer_thread(DATA_T *dest, DATA_T *src, int32 count)
{
    register int32 i;

    for (i = count - 1; i >= 0; i--) {
        dest[i] += src[i];
    }
}
#endif /* IX86CPU */
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
static inline void mix_buffer_thread(DATA_T *dest, DATA_T *src, int32 count)
{
    int32  i;
	for(i = 0; i < count; i += 8){
		MM256_LS_ADD_PD(&dest[i], _mm256_load_pd(&src[i]));
		MM256_LS_ADD_PD(&dest[i + 4], _mm256_load_pd(&src[i + 4]));
	}
}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
static inline void mix_buffer_thread(DATA_T *dest, DATA_T *src, int32 count)
{
    int32  i;
	for(i = 0; i < count; i += 8){
		MM256_LS_ADD_PS(&dest[i], _mm256_load_ps(&src[i]));
	}
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
static inline void mix_buffer_thread(DATA_T *dest, DATA_T *src, int32 count)
{
    int32  i;
	for(i = 0; i < count; i += 8){
		MM_LS_ADD_PD(&dest[i], _mm_load_pd(&src[i]));
		MM_LS_ADD_PD(&dest[i + 2], _mm_load_pd(&src[i + 2]));
		MM_LS_ADD_PD(&dest[i + 4], _mm_load_pd(&src[i + 4]));
		MM_LS_ADD_PD(&dest[i + 6], _mm_load_pd(&src[i + 6]));
	}
}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
static inline void mix_buffer_thread(DATA_T *dest, DATA_T *src, int32 count)
{
    int32  i;
	for(i = 0; i < count; i += 8){
		MM_LS_ADD_PS(&dest[i], _mm_load_ps(&src[i]));
		MM_LS_ADD_PS(&dest[i + 4], _mm_load_ps(&src[i + 4]));
	}
}
#else
static inline void mix_buffer_thread(DATA_T *dest, DATA_T *src, int32 count)
{
	int32 i;
	for (i = 0; i < count; i++)
		dest[i] += src[i];
}
#endif /* OPT_MODE != 0 */



#if (OPT_MODE == 1) && !defined(DATA_T_DOUBLE) && !defined(DATA_T_FLOAT) /* fixed-point implementation */
#if defined(IX86CPU) && defined(SUPPORT_ASM_INTEL)
static inline void mix_level_buffer_thread(DATA_T *dest, DATA_T *src, int32 count, int32 level)
{
	if (!level) { return; }
	level = CONV_LV_24BIT(level);
	_asm {
		mov		ecx, [count]
		mov		esi, [src]
		mov		ebx, [level]
		test		ecx, ecx
		jz		short L2
		mov		edi, [dest]
L1:		mov		eax, [esi]
		imul		ebx
		shr		eax, 24
		shl		edx, 8
		or		eax, edx	/* u */
		mov		edx, [edi]	/* v */
		add		esi, 4		/* u */
		add		edx, eax	/* v */
		mov		[edi], edx	/* u */
		add		edi, 4		/* v */
		dec		ecx		/* u */
		jnz		L1		/* v */
L2:
	}
}
#elif defined(IX86CPU) && defined(SUPPORT_ASM_AT_AND_T)
static inline void mix_level_buffer_thread(DATA_T *dest, DATA_T *src, int32 count, int32 level)
{
	if (!level) { return; }
	level = CONV_LV_24BIT(level);
	__asm__("\
		testl		%%ecx, %%ecx;\
		jz		L2_REVRB;\
L1_REVRB:	movl		(%%esi), %%eax;\
		imull		%%ebx;\
		shrl		$24, %%eax;	/* 24.8 */\
		shll		$8, %%edx;\
		orl		%%edx, %%eax;	/* u */\
		movl		(%%edi), %%edx;	/* v */\
		addl		$4, %%esi;	/* u */\
		addl		%%eax, %%edx;	/* v */\
		movl		%%edx, (%%edi);	/* u */\
		addl		$4, %%edi;	/* v */\
		decl		%%ecx;		/* u */\
		jnz		L1_REVRB;	/* v */\
L2_REVRB:	"
		: "=S"(src), "=c"(count), "=D"(dest)
		: "0"(src), "1"(count), "2"(dest), "b"(level)
		: "eax", "edx", "memory"
	);
}
#else
static inline void mix_level_buffer_thread(DATA_T *dest, DATA_T *src, int32 count, int32 level)
{
	register int32 i;

	if (level)
	{
		const int32 send_leveli = CONV_LV_24BIT(level);
#if 1
		register const int32 * const ebuf = src + count;

		do
		{
			(*(dest++)) += imuldiv24((*(src++)), send_leveli);
		} while (src < ebuf);
#else
		for (i = count - 1; i >= 0; i--)
		{
			dest[i] += imuldiv24(src[i], send_leveli);
		}
#endif
	}
}
#endif /* IX86CPU */
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
static inline void mix_level_buffer_thread(DATA_T *dest, DATA_T *src, int32 count, int32 level)
{
    int32  i;

	if(!level) {return;}
	{
		__m256d send_level = _mm256_set1_pd((double)level * DIV_127);
		for(i = 0; i < count; i += 8){
			MM256_LS_FMA_PD(&dest[i], _mm256_load_pd(&src[i]), send_level);
			MM256_LS_FMA_PD(&dest[i + 4], _mm256_load_pd(&src[i + 4]), send_level);
		}
	}
}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
static inline void mix_level_buffer_thread(DATA_T *dest, DATA_T *src, int32 count, int32 level)
{
    int32  i;
	__m256 send_level = _mm256_set1_ps((float)level * DIV_127);
	for(i = 0; i < count; i += 8)
		MM256_LS_FMA_PS(&dest[i], _mm256_load_ps(&src[i]), send_level);
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
static inline void mix_level_buffer_thread(DATA_T *dest, DATA_T *src, int32 count, int32 level)
{
    int32  i;
	__m128d send_level = _mm_set1_pd((double)level * DIV_127);
	for(i = 0; i < count; i += 8){
		MM_LS_FMA_PD(&dest[i], _mm_load_pd(&src[i]), send_level);
		MM_LS_FMA_PD(&dest[i + 2], _mm_load_pd(&src[i + 2]), send_level);
		MM_LS_FMA_PD(&dest[i + 4], _mm_load_pd(&src[i + 4]), send_level);
		MM_LS_FMA_PD(&dest[i + 6], _mm_load_pd(&src[i + 6]), send_level);
	}
}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
static inline void mix_level_buffer_thread(DATA_T *dest, DATA_T *src, int32 count, int32 level)
{
    int32  i;
	__m128 send_level = _mm_set_ps1((float)level * DIV_127);
	for(i = 0; i < count; i += 8){
		MM_LS_FMA_PS(&dest[i], _mm_load_ps(&src[i]), send_level);
		MM_LS_FMA_PS(&dest[i + 4], _mm_load_ps(&src[i + 4]), send_level);
	}
}
#else
static inline void mix_level_buffer_thread(DATA_T *dest, DATA_T *src, int32 count, int32 level)
{
    int32  i;
	FLOAT_T send_level = (FLOAT_T)level * DIV_127;
	for(i = 0; i < count; i++)  
		dest[i] += src[i] * send_level;
}
#endif /* OPT_MODE != 0 */




/***  Send Effect  ***/ 

void set_ch_reverb_thread(DATA_T *buf, int32 count, int32 level)
{
	if (!level) { return; }
	mix_level_buffer_thread(reverb_effect_buffer_sub[cdmt_buf_i], buf, count, level);
}

void set_ch_delay_thread(DATA_T *buf, int32 count, int32 level)
{
	if (!level) { return; }
	mix_level_buffer_thread(delay_effect_buffer_sub[cdmt_buf_i], buf, count, level);
}

void set_ch_chorus_thread(DATA_T *buf, int32 count, int32 level)
{
	if (!level) { return; }
	mix_level_buffer_thread(chorus_effect_buffer_sub[cdmt_buf_i], buf, count, level);
}

void set_ch_eq_gs_thread(DATA_T *buf, int32 count)
{
	mix_buffer_thread(eq_effect_buffer_sub[cdmt_buf_i], buf, count);
}

void set_ch_insertion_gs_thread(DATA_T *buf, int32 count)
{
	mix_buffer_thread(insertion_effect_buffer_sub[cdmt_buf_i], buf, count);
}

void set_ch_mfx_sd_thread(DATA_T *buf, int32 count, int mfx_select, int32 level)
{
	if (!level) { return; }
	mix_level_buffer_thread(mfx_effect_buffer_sub[cdmt_buf_i][mfx_select], buf, count, level);
}




/***  GS  ***/ 

void do_insertion_effect_gs_thread(int32 count, int32 byte)
{
	do_effect_list(insertion_effect_buffer_sub[cdmt_buf_o], count, insertion_effect_gs.ef);
}

#if (OPT_MODE == 1) && !defined(DATA_T_DOUBLE) && !defined(DATA_T_FLOAT) /* fixed-point implementation */
void mix_insertion_effect_gs_thread(DATA_T *buf, int32 count, int32 byte, int eq_enable)
{
	int32 i;
	DATA_T x, *out_ptr;
	int32 send_chorus =  TIM_FSCALE((double)insertion_effect_gs.send_chorus * DIV_127, 24),
		send_reverb = TIM_FSCALE((double)insertion_effect_gs.send_reverb * DIV_127, 24),
		send_delay = TIM_FSCALE((double)insertion_effect_gs.send_delay * DIV_127, 24);
	
	if(insertion_effect_gs.send_eq_switch && eq_enable)
		out_ptr = eq_buffer_thread[cdmt_ofs_0];
	else if(!buf) // NULL
		out_ptr = direct_buffer;
	else
		out_ptr = buf;
	for (i = 0; i < count; i++) {
		x = insertion_effect_buffer_sub[cdmt_buf_o][i];
		out_ptr[i] += x;
		chorus_effect_buffer_thread[cdmt_ofs_0][i] += imuldiv24(x, send_chorus);
		delay_effect_buffer_thread[cdmt_ofs_0][i] += imuldiv24(x, send_delay);
		reverb_effect_buffer_thread[cdmt_ofs_0][i] += imuldiv24(x, send_reverb);
	}
	memset(insertion_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
void mix_insertion_effect_gs_thread(DATA_T *buf, int32 count, int32 byte, int eq_enable)
{
	int32 i;
	DATA_T x, *out_ptr;
	__m256d send_chorus = _mm256_set1_pd((double)insertion_effect_gs.send_chorus * DIV_127),
		send_reverb = _mm256_set1_pd((double)insertion_effect_gs.send_reverb * DIV_127),
		send_delay = _mm256_set1_pd((double)insertion_effect_gs.send_delay * DIV_127);
	
	if(insertion_effect_gs.send_eq_switch && eq_enable)
		out_ptr = eq_buffer_thread[cdmt_ofs_0];
	else if(!buf) // NULL
		out_ptr = direct_buffer;
	else
		out_ptr = buf;
	for(i = 0; i < count; i += 8){
		__m256d out = _mm256_load_pd(&insertion_effect_buffer_sub[cdmt_buf_o][i]);
		MM256_LS_ADD_PD(&out_ptr[i], out);
		MM256_LS_FMA_PD(&chorus_effect_buffer_thread[cdmt_ofs_0][i], out, send_chorus);
		MM256_LS_FMA_PD(&delay_effect_buffer_thread[cdmt_ofs_0][i], out, send_delay);
		MM256_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_0][i], out, send_reverb);
		out = _mm256_load_pd(&insertion_effect_buffer_sub[cdmt_buf_o][i + 4]);
		MM256_LS_ADD_PD(&out_ptr[i + 4], out);
		MM256_LS_FMA_PD(&chorus_effect_buffer_thread[cdmt_ofs_0][i + 4], out, send_chorus);
		MM256_LS_FMA_PD(&delay_effect_buffer_thread[cdmt_ofs_0][i + 4], out, send_delay);
		MM256_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_0][i + 4], out, send_reverb);
	}
	memset(insertion_effect_buffer, 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
void mix_insertion_effect_gs_thread(DATA_T *buf, int32 count, int32 byte, int eq_enable)
{
	int32 i;
	DATA_T x, *out_ptr;
	__m256 send_chorus = _mm256_set1_ps((double)insertion_effect_gs.send_chorus * DIV_127),
		send_reverb = _mm256_set1_ps((double)insertion_effect_gs.send_reverb * DIV_127),
		send_delay = _mm256_set1_ps((double)insertion_effect_gs.send_delay * DIV_127);
	
	if(insertion_effect_gs.send_eq_switch && eq_enable)
		out_ptr = eq_buffer_thread[cdmt_ofs_0];
	else if(!buf) // NULL
		out_ptr = direct_buffer;
	else
		out_ptr = buf;
	for(i = 0; i < count; i += 8){
		__m256 out = _mm256_load_ps(&insertion_effect_buffer_sub[cdmt_buf_o][i]);
		MM256_LS_ADD_PS(&out_ptr[i], out);
		MM256_LS_FMA_PS(&chorus_effect_buffer_thread[cdmt_ofs_0][i], out, send_chorus);
		MM256_LS_FMA_PS(&delay_effect_buffer_thread[cdmt_ofs_0][i], out, send_delay);
		MM256_LS_FMA_PS(&reverb_effect_buffer_thread[cdmt_ofs_0][i], out, send_reverb);
	}
	memset(insertion_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
void mix_insertion_effect_gs_thread(DATA_T *buf, int32 count, int32 byte, int eq_enable)
{
	int32 i;
	DATA_T x, *out_ptr;
	__m128d send_chorus = _mm_set1_pd((double)insertion_effect_gs.send_chorus * DIV_127),
		send_reverb = _mm_set1_pd((double)insertion_effect_gs.send_reverb * DIV_127),
		send_delay = _mm_set1_pd((double)insertion_effect_gs.send_delay * DIV_127);
	
	if(insertion_effect_gs.send_eq_switch && eq_enable)
		out_ptr = eq_buffer_thread[cdmt_ofs_0];
	else if(!buf) // NULL
		out_ptr = direct_buffer;
	else
		out_ptr = buf;
	for(i = 0; i < count; i += 4){
		__m128d out = _mm_load_pd(&insertion_effect_buffer_sub[cdmt_buf_o][i]);
		MM_LS_ADD_PD(&out_ptr[i], out);
		MM_LS_FMA_PD(&chorus_effect_buffer_thread[cdmt_ofs_0][i], out, send_chorus);
		MM_LS_FMA_PD(&delay_effect_buffer_thread[cdmt_ofs_0][i], out, send_delay);
		MM_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_0][i], out, send_reverb);
		out = _mm_load_pd(&insertion_effect_buffer_sub[cdmt_buf_o][i + 2]);
		MM_LS_ADD_PD(&out_ptr[i + 2], out);
		MM_LS_FMA_PD(&chorus_effect_buffer_thread[cdmt_ofs_0][i + 2], out, send_chorus);
		MM_LS_FMA_PD(&delay_effect_buffer_thread[cdmt_ofs_0][i + 2], out, send_delay);
		MM_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_0][i + 2], out, send_reverb);
	}
	memset(insertion_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
void mix_insertion_effect_gs_thread(DATA_T *buf, int32 count, int32 byte, int eq_enable)
{
	int32 i;
	DATA_T x, *out_ptr;
	__m128 send_chorus = _mm_set1_ps((double)insertion_effect_gs.send_chorus * DIV_127),
		send_reverb = _mm_set1_ps((double)insertion_effect_gs.send_reverb * DIV_127),
		send_delay = _mm_set1_ps((double)insertion_effect_gs.send_delay * DIV_127);
	
	if(insertion_effect_gs.send_eq_switch && eq_enable)
		out_ptr = eq_buffer_thread[cdmt_ofs_0];
	else if(!buf) // NULL
		out_ptr = direct_buffer;
	else
		out_ptr = buf;
	for(i = 0; i < count; i += 8){
		__m128 out = _mm_load_ps(&insertion_effect_buffer_sub[cdmt_buf_o][i]);
		MM_LS_ADD_PS(&out_ptr[i], out);
		MM_LS_FMA_PS(&chorus_effect_buffer_thread[cdmt_ofs_0][i], out, send_chorus);
		MM_LS_FMA_PS(&delay_effect_buffer_thread[cdmt_ofs_0][i], out, send_delay);
		MM_LS_FMA_PS(&reverb_effect_buffer_thread[cdmt_ofs_0][i], out, send_reverb);
		out = _mm_load_ps(&insertion_effect_buffer_sub[cdmt_buf_o][i + 4]);
		MM_LS_ADD_PS(&out_ptr[i], out);
		MM_LS_FMA_PS(&chorus_effect_buffer_thread[cdmt_ofs_0][i + 4], out, send_chorus);
		MM_LS_FMA_PS(&delay_effect_buffer_thread[cdmt_ofs_0][i + 4], out, send_delay);
		MM_LS_FMA_PS(&reverb_effect_buffer_thread[cdmt_ofs_0][i + 4], out, send_reverb);
	}
	memset(insertion_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#else
void mix_insertion_effect_gs_thread(DATA_T *buf, int32 count, int32 byte, int eq_enable)
{
	int32 i;
	DATA_T x, *out_ptr;
	FLOAT_T send_chorus =  (double)insertion_effect_gs.send_chorus * DIV_127,
		send_reverb = (double)insertion_effect_gs.send_reverb * DIV_127,
		send_delay = (double)insertion_effect_gs.send_delay * DIV_127;
	
	if(insertion_effect_gs.send_eq_switch && eq_enable)
		out_ptr = eq_buffer_thread[cdmt_ofs_0];
	else if(!buf) // NULL
		out_ptr = direct_buffer;
	else
		out_ptr = buf;
	for (i = 0; i < count; i++) {
		x = insertion_effect_buffer_sub[cdmt_buf_o][i];
		out_ptr[i] += x;
		chorus_effect_buffer_thread[cdmt_ofs_0][i] += x * send_chorus;
		delay_effect_buffer_thread[cdmt_ofs_0][i] += x * send_delay;
		reverb_effect_buffer_thread[cdmt_ofs_0][i] += x * send_reverb;
	}
	memset(insertion_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#endif /* OPT_MODE != 0 */

void do_ch_eq_gs_thread(int32 count, int32 byte)
{
	mix_buffer_thread(eq_effect_buffer_sub[cdmt_buf_o], eq_buffer_thread[cdmt_ofs_1], count);
	buffer_filter_stereo(&(eq_status_gs.lsf), eq_effect_buffer_sub[cdmt_buf_o], count);
	buffer_filter_stereo(&(eq_status_gs.hsf), eq_effect_buffer_sub[cdmt_buf_o], count);
	memset(eq_buffer_thread[cdmt_ofs_1], 0, byte);
}

#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
void mix_ch_eq_gs_thread(DATA_T* buf, int32 count, int32 byte)
{
	int32 i;
	
	for(i = 0; i < count; i += 8){
		MM256_LS_ADD_PD(&buf[i], _mm256_load_pd(&eq_effect_buffer_sub[cdmt_buf_o][i]));
		MM256_LS_ADD_PD(&buf[i + 4], _mm256_load_pd(&eq_effect_buffer_sub[cdmt_buf_o][i + 4]));
	}
	memset(eq_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
void mix_ch_eq_gs_thread(DATA_T* buf, int32 count, int32 byte)
{
	int32 i;
	
	for(i = 0; i < count; i += 8)
		MM256_LS_ADD_PS(&buf[i], _mm256_load_ps(&eq_effect_buffer_sub[cdmt_buf_o][i]));
	memset(eq_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
void mix_ch_eq_gs_thread(DATA_T* buf, int32 count, int32 byte)
{
	int32 i;
	
	for(i = 0; i < count; i += 8){
		MM_LS_ADD_PD(&buf[i], _mm_load_pd(&eq_effect_buffer_sub[cdmt_buf_o][i]));
		MM_LS_ADD_PD(&buf[i + 2], _mm_load_pd(&eq_effect_buffer_sub[cdmt_buf_o][i + 2]));
		MM_LS_ADD_PD(&buf[i + 4], _mm_load_pd(&eq_effect_buffer_sub[cdmt_buf_o][i + 4]));
		MM_LS_ADD_PD(&buf[i + 6], _mm_load_pd(&eq_effect_buffer_sub[cdmt_buf_o][i + 6]));
	}
	memset(eq_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
void mix_ch_eq_gs_thread(DATA_T* buf, int32 count, int32 byte)
{
	int32 i;
	
	for(i = 0; i < count; i += 8){
		MM_LS_ADD_PS(&buf[i], _mm_load_ps(&eq_effect_buffer_sub[cdmt_buf_o][i]));
		MM_LS_ADD_PS(&buf[i + 4], _mm_load_ps(&eq_effect_buffer_sub[cdmt_buf_o][i + 4]));
	}
	memset(eq_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#else
void mix_ch_eq_gs_thread(DATA_T* buf, int32 count, int32 byte)
{
	int32 i;
		
	for(i = 0; i < count; i++) {
		buf[i] += eq_effect_buffer_sub[cdmt_buf_o][i];
	}
	memset(eq_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#endif

void do_ch_chorus_thread(int32 count, int32 byte)
{
	mix_buffer_thread(chorus_effect_buffer_sub[cdmt_buf_o], chorus_effect_buffer_thread[cdmt_ofs_1], count);
#ifdef SYS_EFFECT_PRE_LPF
	if ((opt_reverb_control >= 3 || (opt_reverb_control < 0 && ! (opt_reverb_control & 0x100))) && chorus_status_gs.pre_lpf)
		do_pre_lpf(chorus_effect_buffer_sub[cdmt_buf_o], count, &(chorus_status_gs.lpf));
#endif /* SYS_EFFECT_PRE_LPF */	
	(*p_do_ch_chorus)(chorus_effect_buffer_sub[cdmt_buf_o], count, &(chorus_status_gs.info_stereo_chorus));
	memset(chorus_effect_buffer_thread[cdmt_ofs_1], 0, byte);
}

#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
void mix_ch_chorus_thread(DATA_T *buf, int32 count, int32 byte)
{
	InfoStereoChorus *info = &(chorus_status_gs.info_stereo_chorus);
    int32  i;
	__m256d cho_level = _mm256_set1_pd(info->level),
		send_reverb = _mm256_set1_pd(info->send_reverb),
		send_delay = _mm256_set1_pd(info->send_delay);
	
	for(i = 0; i < count; i += 8){
		__m256d out = _mm256_mul_pd(_mm256_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i]), cho_level);
		MM256_LS_ADD_PD(&buf[i], out);
		MM256_LS_FMA_PD(&delay_effect_buffer_thread[cdmt_ofs_1][i], out, send_delay);
		MM256_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_1][i], out, send_reverb);
		out = _mm256_mul_pd(_mm256_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i + 4]), cho_level);
		MM256_LS_ADD_PD(&buf[i + 4], out);
		MM256_LS_FMA_PD(&delay_effect_buffer_thread[cdmt_ofs_1][i + 4], out, send_delay);
		MM256_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_1][i + 4], out, send_reverb);
	}
	memset(chorus_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
void mix_ch_chorus_thread(DATA_T *buf, int32 count, int32 byte)
{
	InfoStereoChorus *info = &(chorus_status_gs.info_stereo_chorus);
    int32  i;
	__m256 cho_level = _mm256_set1_ps(info->level),
		send_reverb = _mm256_set1_ps(info->send_reverb),
		send_delay = _mm256_set1_ps(info->send_delay);
	
	for(i = 0; i < count; i += 8){
		__m256 out = _mm256_mul_ps(_mm256_load_ps(&chorus_effect_buffer_sub[cdmt_buf_o][i]), cho_level);
		MM256_LS_ADD_PS(&buf[i], out);
		MM256_LS_FMA_PS(&delay_effect_buffer_thread[cdmt_ofs_1][i], out, send_delay);
		MM256_LS_FMA_PS(&reverb_effect_buffer_thread[cdmt_ofs_1][i], out, send_reverb);
	}
	memset(chorus_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
void mix_ch_chorus_thread(DATA_T *buf, int32 count, int32 byte)
{
	InfoStereoChorus *info = &(chorus_status_gs.info_stereo_chorus);
    int32  i;
	__m128d cho_level = _mm_set1_pd(info->level),
		send_reverb = _mm_set1_pd(info->send_reverb),
		send_delay = _mm_set1_pd(info->send_delay);
	
	for(i = 0; i < count; i += 4){
		__m128d out = _mm_mul_pd(_mm_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i]), cho_level);
		MM_LS_ADD_PD(&buf[i], out);
		MM_LS_FMA_PD(&delay_effect_buffer_thread[cdmt_ofs_1][i], out, send_delay);
		MM_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_1][i], out, send_reverb);
		out = _mm_mul_pd(_mm_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i + 2]), cho_level);
		MM_LS_ADD_PD(&buf[i + 2], out);
		MM_LS_FMA_PD(&delay_effect_buffer_thread[cdmt_ofs_1][i + 2], out, send_delay);
		MM_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_1][i + 2], out, send_reverb);
	}
	memset(chorus_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
void mix_ch_chorus_thread(DATA_T *buf, int32 count, int32 byte)
{
	InfoStereoChorus *info = &(chorus_status_gs.info_stereo_chorus);
    int32  i;
	__m128 cho_level = _mm_set1_ps(info->level),
		send_reverb = _mm_set1_ps(info->send_reverb),
		send_delay = _mm_set1_ps(info->send_delay);
	
	for(i = 0; i < count; i += 8){
		__m128 out = _mm_mul_ps(_mm_load_ps(&chorus_effect_buffer_sub[cdmt_buf_o][i]), cho_level);
		MM_LS_ADD_PS(&buf[i], out);
		MM_LS_FMA_PS(&delay_effect_buffer_thread[cdmt_ofs_1][i], out, send_delay);
		MM_LS_FMA_PS(&reverb_effect_buffer_thread[cdmt_ofs_1][i], out, send_reverb);
		out = _mm_mul_ps(_mm_load_ps(&chorus_effect_buffer_sub[cdmt_buf_o][i + 4]), cho_level);
		MM_LS_ADD_PS(&buf[i + 4], out);
		MM_LS_FMA_PS(&delay_effect_buffer_thread[cdmt_ofs_1][i + 4], out, send_delay);
		MM_LS_FMA_PS(&reverb_effect_buffer_thread[cdmt_ofs_1][i + 4], out, send_reverb);
	}
	memset(chorus_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#else
void mix_ch_chorus_thread(DATA_T *buf, int32 count, int32 byte)
{
	InfoStereoChorus *info = &(chorus_status_gs.info_stereo_chorus);
	int32 i;
	
	for (i = 0; i < count; i++) {
		DATA_T out = chorus_effect_buffer_sub[cdmt_buf_o][i] * info->level;
		buf[i] += out;
		delay_effect_buffer_thread[cdmt_ofs_1][i] += out * info->send_delay;
		reverb_effect_buffer_thread[cdmt_ofs_1][i] += out * info->send_reverb;
	}
	memset(chorus_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#endif /* OPT_MODE != 0 */

void do_ch_delay_thread(int32 count, int32 byte)
{
	mix_buffer_thread(delay_effect_buffer_sub[cdmt_buf_o], delay_effect_buffer_thread[cdmt_ofs_2], count);
#ifdef SYS_EFFECT_PRE_LPF
	if ((opt_reverb_control >= 3	|| (opt_reverb_control < 0 && ! (opt_reverb_control & 0x100))) && delay_status_gs.pre_lpf)
		do_pre_lpf(delay_effect_buffer_sub[cdmt_buf_o], count, &(delay_status_gs.lpf));
#endif /* SYS_EFFECT_PRE_LPF */
	do_ch_3tap_delay(delay_effect_buffer_sub[cdmt_buf_o], count, &(delay_status_gs.info_delay));	
	memset(delay_effect_buffer_thread[cdmt_ofs_2], 0, byte);
}

#if (OPT_MODE == 1) && !defined(DATA_T_DOUBLE) && !defined(DATA_T_FLOAT) /* fixed-point implementation */
void mix_ch_delay_thread(DATA_T *buf, int32 count, int32 byte)
{
	InfoDelay3 *info = &(delay_status_gs.info_delay);
	int32 i;
	int32 send_reverbi = TIM_FSCALE(info->send_reverb, 24),
		delay_leveli = TIM_FSCALE(info->delay_level, 24);
	
	for (i = 0; i < count; i++) {
		DATA_T out = imuldiv24(delay_effect_buffer_sub[cdmt_buf_o][i], delay_leveli);
		buf[i] += out;
		reverb_effect_buffer_thread[cdmt_ofs_2][i] += imuldiv24(out, send_reverbi);
	}
	memset(delay_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
void mix_ch_delay_thread(DATA_T *buf, int32 count, int32 byte)
{
	InfoDelay3 *info = &(delay_status_gs.info_delay);
    int32  i;
	__m256d delay_level = _mm256_set1_pd(info->delay_level),
		send_reverb = _mm256_set1_pd(info->send_reverb);
	
	for(i = 0; i < count; i += 8)	{
		__m256d out = _mm256_mul_pd(_mm256_load_pd(&delay_effect_buffer_sub[cdmt_buf_o][i]), delay_level);
		MM256_LS_ADD_PD(&buf[i], out);
		MM256_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_2][i], out, send_reverb);
		out = _mm256_mul_pd(_mm256_load_pd(&delay_effect_buffer_sub[cdmt_buf_o][i + 4]), delay_level);
		MM256_LS_ADD_PD(&buf[i + 4], out);
		MM256_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_2][i + 4], out, send_reverb);
	}
	memset(delay_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
void mix_ch_delay_thread(DATA_T *buf, int32 count, int32 byte)
{
	InfoDelay3 *info = &(delay_status_gs.info_delay);
    int32  i;
	__m256 delay_level = _mm256_set1_ps(info->delay_level),
		send_reverb = _mm256_set1_ps(info->send_reverb);
	
	for(i = 0; i < count; i += 8)	{
		__m256 out = _mm256_mul_ps(_mm256_load_ps(&delay_effect_buffer_sub[cdmt_buf_o][i]), delay_level);
		MM256_LS_ADD_PS(&buf[i], out);
		MM256_LS_FMA_PS(&reverb_effect_buffer_thread[cdmt_ofs_2][i], out, send_reverb);
	}
	memset(delay_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
void mix_ch_delay_thread(DATA_T *buf, int32 count, int32 byte)
{
	InfoDelay3 *info = &(delay_status_gs.info_delay);
    int32  i;
	__m128d delay_level = _mm_set1_pd(info->delay_level),
		send_reverb = _mm_set1_pd(info->send_reverb);
	
	for(i = 0; i < count; i += 4)	{
		__m128d out = _mm_mul_pd(_mm_load_pd(&delay_effect_buffer_sub[cdmt_buf_o][i]), delay_level);
		MM_LS_ADD_PD(&buf[i], out);
		MM_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_2][i], out, send_reverb);
		out = _mm_mul_pd(_mm_load_pd(&delay_effect_buffer_sub[cdmt_buf_o][i + 2]), delay_level);
		MM_LS_ADD_PD(&buf[i + 2], out);
		MM_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_2][i + 2], out, send_reverb);
	}
	memset(delay_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
void mix_ch_delay_thread(DATA_T *buf, int32 count, int32 byte)
{
	InfoDelay3 *info = &(delay_status_gs.info_delay);
    int32  i;
	__m128 delay_level = _mm_set1_ps(info->delay_level),
		send_reverb = _mm_set1_ps(info->send_reverb);

	for(i = 0; i < count; i += 8)	{
		__m128 out = _mm_mul_ps(_mm_load_ps(&delay_effect_buffer_sub[cdmt_buf_o][i]), delay_level);
		MM_LS_ADD_PS(&buf[i], out);
		MM_LS_FMA_PS(&reverb_effect_buffer_thread[cdmt_ofs_2][i], out, send_reverb);
		out = _mm_mul_ps(_mm_load_ps(&delay_effect_buffer_sub[cdmt_buf_o][i + 4]), delay_level);
		MM_LS_ADD_PS(&buf[i + 4], out);
		MM_LS_FMA_PS(&reverb_effect_buffer_thread[cdmt_ofs_2][i + 4], out, send_reverb);
	}
	memset(delay_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#else
void mix_ch_delay_thread(DATA_T *buf, int32 count, int32 byte)
{
	InfoDelay3 *info = &(delay_status_gs.info_delay);
	int32 i;
	
	for (i = 0; i < count; i++) {
		DATA_T out = delay_effect_buffer_sub[cdmt_buf_o][i] * info->delay_level;
		buf[i] += out;
		reverb_effect_buffer_thread[cdmt_ofs_2][i] += out * info->send_reverb;
	}
	memset(delay_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#endif /* OPT_MODE != 0 */

void do_ch_reverb_thread(int32 count, int32 byte)
{
	mix_buffer_thread(reverb_effect_buffer_sub[cdmt_buf_o], reverb_effect_buffer_thread[cdmt_ofs_3], count);
	if (opt_reverb_control >= 3 || opt_reverb_control <= -256){
#ifdef SYS_EFFECT_PRE_LPF
		if(reverb_status_gs.pre_lpf)
			do_pre_lpf(reverb_effect_buffer_sub[cdmt_buf_o], count, &(reverb_status_gs.lpf));
#endif /* SYS_EFFECT_PRE_LPF */
	}
	(*p_do_ch_reverb)(reverb_effect_buffer_sub[cdmt_buf_o], count, &reverb_status_gs);
	memset(reverb_effect_buffer_thread[cdmt_ofs_3], 0, byte);
}

#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
void mix_ch_reverb_thread(DATA_T *buf, int32 count, int32 byte)
{
    int32  i;
	
	for(i = 0; i < count; i += 8){
		MM256_LS_ADD_PD(&buf[i], _mm256_load_pd(&reverb_effect_buffer_sub[cdmt_buf_o][i]));
		MM256_LS_ADD_PD(&buf[i + 4], _mm256_load_pd(&reverb_effect_buffer_sub[cdmt_buf_o][i + 4]));
	}
	memset(reverb_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
void mix_ch_reverb_thread(DATA_T *buf, int32 count, int32 byte)
{
    int32  i;
	
	for(i = 0; i < count; i += 8)
		MM256_LS_ADD_PS(&buf[i], _mm256_load_ps(&reverb_effect_buffer_sub[cdmt_buf_o][i]));
	memset(reverb_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
void mix_ch_reverb_thread(DATA_T *buf, int32 count, int32 byte)
{
    int32  i;
	
	for(i = 0; i < count; i += 8){
		MM_LS_ADD_PD(&buf[i], _mm_load_pd(&reverb_effect_buffer_sub[cdmt_buf_o][i]));
		MM_LS_ADD_PD(&buf[i + 2], _mm_load_pd(&reverb_effect_buffer_sub[cdmt_buf_o][i + 2]));
		MM_LS_ADD_PD(&buf[i + 4], _mm_load_pd(&reverb_effect_buffer_sub[cdmt_buf_o][i + 4]));
		MM_LS_ADD_PD(&buf[i + 6], _mm_load_pd(&reverb_effect_buffer_sub[cdmt_buf_o][i + 6]));
	}
	memset(reverb_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
void mix_ch_reverb_thread(DATA_T *buf, int32 count, int32 byte)
{
    int32  i;
	
	for(i = 0; i < count; i += 8){
		MM_LS_ADD_PS(&buf[i], _mm_load_ps(&reverb_effect_buffer_sub[cdmt_buf_o][i]));
		MM_LS_ADD_PS(&buf[i + 4], _mm_load_ps(&reverb_effect_buffer_sub[cdmt_buf_o][i + 4]));
	}
	memset(reverb_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#else
void mix_ch_reverb_thread(DATA_T *buf, int32 count, int32 byte)
{
	int32 i;
	
	for (i = 0; i < count; i++)
		buf[i] += reverb_effect_buffer_sub[cdmt_buf_o][i];
	memset(reverb_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#endif




/***  XG  ***/ 

void do_variation_effect1_xg_thread(int32 count, int32 byte)
{
	if (variation_effect_xg[0].connection == XG_CONN_SYSTEM)
		do_effect_list(delay_effect_buffer_sub[cdmt_buf_o], count, variation_effect_xg[0].ef);
	else
		memset(delay_effect_buffer_sub[cdmt_buf_o], 0, byte);
}

#if (OPT_MODE == 1) && !defined(DATA_T_DOUBLE) && !defined(DATA_T_FLOAT) /* fixed-point implementation */
void mix_variation_effect1_xg_thread(DATA_T *buf, int32 count, int32 byte)
{
	if (variation_effect_xg[0].connection == XG_CONN_SYSTEM) {	
		int32 i, x;
		int32 panli = TIM_FSCALE((double)variation_effect_xg[0].pan_level[0], 24);
		int32 panri = TIM_FSCALE((double)variation_effect_xg[0].pan_level[1], 24);
		int32 send_reverbi = TIM_FSCALE(variation_effect_xg[0].reverb_level, 24),
			send_chorusi = TIM_FSCALE(variation_effect_xg[0].chorus_level, 24),
			var_returni = TIM_FSCALE(variation_effect_xg[0].return_level, 24);
		for (i = 0; i < count; i++) {
			x = imuldiv24(delay_effect_buffer_sub[cdmt_buf_o][i], panli);
			buf[i] += imuldiv24(x, var_returni);
			chorus_effect_buffer_thread[cdmt_ofs_0][i] += imuldiv24(x, send_chorusi);
			reverb_effect_buffer_thread[cdmt_ofs_0][i] += imuldiv24(x, send_reverbi);
			++i;
			x = imuldiv24(delay_effect_buffer_sub[cdmt_buf_o][i], panri);
			buf[i] += imuldiv24(x, var_returni);
			chorus_effect_buffer_thread[cdmt_ofs_0][i] += imuldiv24(x, send_chorusi);
			reverb_effect_buffer_thread[cdmt_ofs_0][i] += imuldiv24(x, send_reverbi);
		}
	}
	memset(delay_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
void mix_variation_effect1_xg_thread(DATA_T *buf, int32 count, int32 byte)
{
	if (variation_effect_xg[0].connection == XG_CONN_SYSTEM) {	
		int32 i;
		FLOAT_T *pan = variation_effect_xg[0].pan_level;
		__m128d vpant = _mm_set_pd(pan[1], pan[0]);
		__m256d vpan = MM256_SET2X_PD(vpant, vpant);
		__m256d send_reverb = _mm256_set1_pd(variation_effect_xg[0].reverb_level),
			send_chorus = _mm256_set1_pd(variation_effect_xg[0].chorus_level),
			var_return = _mm256_set1_pd(variation_effect_xg[0].return_level);	
		var_return = _mm256_mul_pd(var_return, vpan);
		send_reverb = _mm256_mul_pd(send_reverb, vpan);
		send_chorus = _mm256_mul_pd(send_chorus, vpan);
		for (i = 0; i < count; i += 8) {
			__m256d out = _mm256_load_pd(&delay_effect_buffer_sub[cdmt_buf_o][i]);
			MM256_LS_FMA_PD(&buf[i], out, var_return);
			MM256_LS_FMA_PD(&chorus_effect_buffer_thread[cdmt_ofs_0][i], out, send_chorus);
			MM256_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_0][i], out, send_reverb);
			out = _mm256_load_pd(&delay_effect_buffer_sub[cdmt_buf_o][i + 4]);
			MM256_LS_FMA_PD(&buf[i + 4], out, var_return);
			MM256_LS_FMA_PD(&chorus_effect_buffer_thread[cdmt_ofs_0][i + 4], out, send_chorus);
			MM256_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_0][i + 4], out, send_reverb);
		}
	}
	memset(delay_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
void mix_variation_effect1_xg_thread(DATA_T *buf, int32 count, int32 byte)
{
	if (variation_effect_xg[0].connection == XG_CONN_SYSTEM) {	
		int32 i;
		FLOAT_T *pan = variation_effect_xg[0].pan_level;
		__m128 vpant = _mm_set_ps(pan[1], pan[0], pan[1], pan[0]);
		__m256 vpan = MM256_SET2X_PS(vpant, vpant);
		__m256 send_reverb = _mm256_set1_ps(variation_effect_xg[0].reverb_level),
			send_chorus = _mm256_set1_ps(variation_effect_xg[0].chorus_level),
			var_return = _mm256_set1_ps(variation_effect_xg[0].return_level);	
		var_return = _mm256_mul_ps(var_return, vpan);
		send_reverb = _mm256_mul_ps(send_reverb, vpan);
		send_chorus = _mm256_mul_ps(send_chorus, vpan);
		for (i = 0; i < count; i += 8) {
			__m256 out = _mm256_load_ps(&delay_effect_buffer_sub[cdmt_buf_o][i]);
			MM256_LS_FMA_PS(&buf[i], out, var_return);
			MM256_LS_FMA_PS(&chorus_effect_buffer_thread[cdmt_ofs_0][i], out, send_chorus);
			MM256_LS_FMA_PS(&reverb_effect_buffer_thread[cdmt_ofs_0][i], out, send_reverb);
		}
	}
	memset(delay_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
void mix_variation_effect1_xg_thread(DATA_T *buf, int32 count, int32 byte)
{
	if (variation_effect_xg[0].connection == XG_CONN_SYSTEM) {	
		int32 i;
		FLOAT_T *pan = variation_effect_xg[0].pan_level;
		__m128d vpan = _mm_set_pd(pan[1], pan[0]);
		__m128d send_reverb = _mm_set1_pd(variation_effect_xg[0].reverb_level),
			send_chorus = _mm_set1_pd(variation_effect_xg[0].chorus_level),
			var_return = _mm_set1_pd(variation_effect_xg[0].return_level);	
		var_return = _mm_mul_pd(var_return, vpan);
		send_reverb = _mm_mul_pd(send_reverb, vpan);
		send_chorus = _mm_mul_pd(send_chorus, vpan);
		for (i = 0; i < count; i += 4) {
			__m128d out = _mm_load_pd(&delay_effect_buffer_sub[cdmt_buf_o][i]);
			MM_LS_FMA_PD(&buf[i], out, var_return);
			MM_LS_FMA_PD(&chorus_effect_buffer_thread[cdmt_ofs_0][i], out, send_chorus);
			MM_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_0][i], out, send_reverb);
			out = _mm_load_pd(&delay_effect_buffer_sub[cdmt_buf_o][i + 2]);
			MM_LS_FMA_PD(&buf[i + 2], out, var_return);
			MM_LS_FMA_PD(&chorus_effect_buffer_thread[cdmt_ofs_0][i + 2], out, send_chorus);
			MM_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_0][i + 2], out, send_reverb);
		}
	}
	memset(delay_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
void mix_variation_effect1_xg_thread(DATA_T *buf, int32 count, int32 byte)
{
	if (variation_effect_xg[0].connection == XG_CONN_SYSTEM) {	
		int32 i;
		FLOAT_T *pan = variation_effect_xg[0].pan_level;
		__m128 vpan = _mm_set_ps(pan[1], pan[0], pan[1], pan[0]);
		__m128 send_reverb = _mm_set1_ps(variation_effect_xg[0].reverb_level),
			send_chorus = _mm_set1_ps(variation_effect_xg[0].chorus_level),
			var_return = _mm_set1_ps(variation_effect_xg[0].return_level);	
		var_return = _mm_mul_ps(var_return, vpan);
		send_reverb = _mm_mul_ps(send_reverb, vpan);
		send_chorus = _mm_mul_ps(send_chorus, vpan);
		for (i = 0; i < count; i += 8) {
			__m128 out = _mm_load_ps(&delay_effect_buffer_sub[cdmt_buf_o][i]);
			MM_LS_FMA_PS(&buf[i], out, var_return);
			MM_LS_FMA_PS(&chorus_effect_buffer_thread[cdmt_ofs_0][i], out, send_chorus);
			MM_LS_FMA_PS(&reverb_effect_buffer_thread[cdmt_ofs_0][i], out, send_reverb);
			out = _mm_load_ps(&delay_effect_buffer_sub[cdmt_buf_o][i + 4]);
			MM_LS_FMA_PS(&buf[i + 4], out, var_return);
			MM_LS_FMA_PS(&chorus_effect_buffer_thread[cdmt_ofs_0][i + 4], out, send_chorus);
			MM_LS_FMA_PS(&reverb_effect_buffer_thread[cdmt_ofs_0][i + 4], out, send_reverb);
		}
	}
	memset(delay_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#else /* floating-point implementation */
void mix_variation_effect1_xg_thread(DATA_T *buf, int32 count, int32 byte)
{
	if (variation_effect_xg[0].connection == XG_CONN_SYSTEM) {		
		int32 i;
		DATA_T x;
		FLOAT_T *pan = variation_effect_xg[0].pan_level;
		FLOAT_T send_reverb = variation_effect_xg[0].reverb_level;
		FLOAT_T send_chorus = variation_effect_xg[0].chorus_level;
		FLOAT_T var_return = variation_effect_xg[0].return_level;	
		for (i = 0; i < count; i++) {
			x = delay_effect_buffer_sub[cdmt_buf_o][i] * pan[0];
			buf[i] += x * var_return;
			chorus_effect_buffer_thread[cdmt_ofs_0][i] += x * send_chorus;
			reverb_effect_buffer_thread[cdmt_ofs_0][i] += x * send_reverb;
			++i;
			x = delay_effect_buffer_sub[cdmt_buf_o][i] * pan[1];
			buf[i] += x * var_return;
			chorus_effect_buffer_thread[cdmt_ofs_0][i] += x * send_chorus;
			reverb_effect_buffer_thread[cdmt_ofs_0][i] += x * send_reverb;
		}
	}
	memset(delay_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#endif /* OPT_MODE != 0 */


void do_ch_chorus_xg_thread(int32 count, int32 byte)
{
	mix_buffer_thread(chorus_effect_buffer_sub[cdmt_buf_o], chorus_effect_buffer_thread[cdmt_ofs_1], count);
#ifdef VST_LOADER_ENABLE
#if defined(VSTWRAP_EXT) // if support chorus VST
	if (hVSTHost != NULL && opt_normal_chorus_plus == 6){
		do_chorus_vst(chorus_effect_buffer_sub[cdmt_buf_o], count);
	}else
#endif
#endif
	do_effect_list(chorus_effect_buffer_sub[cdmt_buf_o], count, chorus_status_xg.ef);
	memset(chorus_effect_buffer_thread[cdmt_ofs_1], 0, byte);
}


#if (OPT_MODE == 1) && !defined(DATA_T_DOUBLE) && !defined(DATA_T_FLOAT) /* fixed-point implementation */
void mix_ch_chorus_xg_thread(DATA_T *buf, int32 count, int32 byte)
{
	int32 i, x;
	int32 panli = TIM_FSCALE((double)chorus_status_xg.pan_level[0], 24);
	int32 panri = TIM_FSCALE((double)chorus_status_xg.pan_level[1], 24);
	int32 send_reverbi = TIM_FSCALE(chorus_status_xg.reverb_level, 24),
		cho_returni = TIM_FSCALE(chorus_status_xg.return_level, 24);
	for (i = 0; i < count; i++) {
		x = imuldiv24(chorus_effect_buffer_sub[cdmt_buf_o][i], panli);
		buf[i] += imuldiv24(x, cho_returni);
		reverb_effect_buffer_thread[cdmt_ofs_1][i] += imuldiv24(x, send_reverbi);
		++i;
		x = imuldiv24(chorus_effect_buffer_sub[cdmt_buf_o][i], panri);
		buf[i] += imuldiv24(x, cho_returni);
		reverb_effect_buffer_thread[cdmt_ofs_1][i] += imuldiv24(x, send_reverbi);
	}
	memset(chorus_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
void mix_ch_chorus_xg_thread(DATA_T *buf, int32 count, int32 byte)
{
	int32 i;
	FLOAT_T *pan = chorus_status_xg.pan_level;
	__m128d vpanx = _mm_set_pd(pan[1], pan[0]);
	__m256d vpan = MM256_SET2X_PD(vpanx, vpanx);
	__m256d send_reverb = _mm256_set1_pd(chorus_status_xg.reverb_level),
		cho_return = _mm256_set1_pd(chorus_status_xg.return_level);
	cho_return = _mm256_mul_pd(cho_return, vpan);
	send_reverb = _mm256_mul_pd(send_reverb, vpan);	
	for (i = 0; i < count; i += 8) {
		__m256d out = _mm256_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i]);
		MM256_LS_FMA_PD(&buf[i], out, cho_return);
		MM256_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_1][i], out, send_reverb);
		out = _mm256_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i + 4]);
		MM256_LS_FMA_PD(&buf[i + 4], out, cho_return);
		MM256_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_1][i + 4], out, send_reverb);
	}
	memset(chorus_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
void mix_ch_chorus_xg_thread(DATA_T *buf, int32 count, int32 byte)
{
	int32 i;
	FLOAT_T *pan = chorus_status_xg.pan_level;
	__m128 vpanx = _mm_set_ps(pan[1], pan[0], pan[1], pan[0]);
	__m256 vpan = MM256_SET2X_PS(vpanx, vpanx);
	__m256 send_reverb = _mm256_set1_ps(chorus_status_xg.reverb_level),
		cho_return = _mm256_set1_ps(chorus_status_xg.return_level);	
	cho_return = _mm256_mul_ps(cho_return, vpan);
	send_reverb = _mm256_mul_ps(send_reverb, vpan);	
	for (i = 0; i < count; i += 8) {
		__m256 out = _mm256_load_ps(&chorus_effect_buffer_sub[cdmt_buf_o][i]);
		MM256_LS_FMA_PS(&buf[i], out, cho_return);
		MM256_LS_FMA_PS(&reverb_effect_buffer_thread[cdmt_ofs_1][i], out, send_reverb);
	}
	memset(chorus_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
void mix_ch_chorus_xg_thread(DATA_T *buf, int32 count, int32 byte)
{
	int32 i;
	FLOAT_T *pan = chorus_status_xg.pan_level;
	__m128d vpan = _mm_set_pd(pan[1], pan[0]);
	__m128d send_reverb = _mm_set1_pd(chorus_status_xg.reverb_level),
		cho_return = _mm_set1_pd(chorus_status_xg.return_level);	
	cho_return = _mm_mul_pd(cho_return, vpan);
	send_reverb = _mm_mul_pd(send_reverb, vpan);	
	for (i = 0; i < count; i += 4) {
		__m128d out = _mm_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i]);
		MM_LS_FMA_PD(&buf[i], out, cho_return);
		MM_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_1][i], out, send_reverb);
		out = _mm_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i + 2]);
		MM_LS_FMA_PD(&buf[i + 2], out, cho_return);
		MM_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_1][i + 2], out, send_reverb);
	}
	memset(chorus_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
void mix_ch_chorus_xg_thread(DATA_T *buf, int32 count, int32 byte)
{
	int32 i;
	FLOAT_T *pan = chorus_status_xg.pan_level;
	__m128 vpan = _mm_set_ps(pan[1], pan[0], pan[1], pan[0]);
	__m128 send_reverb = _mm_set1_ps(chorus_status_xg.reverb_level),
		cho_return = _mm_set1_ps(chorus_status_xg.return_level);	
	cho_return = _mm_mul_ps(cho_return, vpan);
	send_reverb = _mm_mul_ps(send_reverb, vpan);	
	for (i = 0; i < count; i += 8) {
		__m128 out = _mm_load_ps(&chorus_effect_buffer_sub[cdmt_buf_o][i]);
		MM_LS_FMA_PS(&buf[i], out, cho_return);
		MM_LS_FMA_PS(&reverb_effect_buffer_thread[cdmt_ofs_1][i], out, send_reverb);
		out = _mm_load_ps(&chorus_effect_buffer_sub[cdmt_buf_o][i + 4]);
		MM_LS_FMA_PS(&buf[i + 4], out, cho_return);
		MM_LS_FMA_PS(&reverb_effect_buffer_thread[cdmt_ofs_1][i + 4], out, send_reverb);
	}
	memset(chorus_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#else /* floating-point implementation */
void mix_ch_chorus_xg_thread(DATA_T *buf, int32 count, int32 byte)
{
	int32 i;
	DATA_T x;
	FLOAT_T *pan = chorus_status_xg.pan_level;	
	FLOAT_T send_reverb = chorus_status_xg.reverb_level;
	FLOAT_T cho_return = chorus_status_xg.return_level;
	for (i = 0; i < count; i++) {
		x = chorus_effect_buffer_sub[cdmt_buf_o][i] * pan[0];
		buf[i] += x * cho_return;
		reverb_effect_buffer_thread[cdmt_ofs_1][i] += x * send_reverb;
		++i;
		x = chorus_effect_buffer_sub[cdmt_buf_o][i] * pan[1];
		buf[i] += x * cho_return;
		reverb_effect_buffer_thread[cdmt_ofs_1][i] += x * send_reverb;
	}
	memset(chorus_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#endif /* OPT_MODE != 0 */

void do_ch_reverb_xg_thread(int32 count, int32 byte)
{
	mix_buffer_thread(reverb_effect_buffer_sub[cdmt_buf_o], reverb_effect_buffer_thread[cdmt_ofs_2], count);
#ifdef VST_LOADER_ENABLE
#if defined(VSTWRAP_EXT) // if support reverb VST
	if (hVSTHost != NULL && (opt_reverb_control >= 9 || opt_reverb_control <= -1024)){
		do_reverb_vst(reverb_effect_buffer_sub[cdmt_buf_o], count);
	}else
#endif
#endif
	do_effect_list(reverb_effect_buffer_sub[cdmt_buf_o], count, reverb_status_xg.ef);	
	memset(reverb_effect_buffer_thread[cdmt_ofs_2], 0, byte);
}

#if (OPT_MODE == 1) && !defined(DATA_T_DOUBLE) && !defined(DATA_T_FLOAT) /* fixed-point implementation */
void mix_ch_reverb_xg_thread(DATA_T *buf, int32 count, int32 byte)
{
	int32 i;
	int32 rev_return[2] = {
		TIM_FSCALE((double)reverb_status_xg.pan_level[0] * reverb_status_xg.return_level, 24),
		TIM_FSCALE((double)reverb_status_xg.pan_level[1] * reverb_status_xg.return_level, 24),
	};
	for (i = 0; i < count; i++) {
		buf[i] += imuldiv24(reverb_effect_buffer_sub[cdmt_buf_o][i], rev_return[0]);
		++i;
		buf[i] += imuldiv24(reverb_effect_buffer_sub[cdmt_buf_o], rev_return[1]);
	}
	memset(reverb_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
void mix_ch_reverb_xg_thread(DATA_T *buf, int32 count, int32 byte)
{
	int32 i;
	FLOAT_T *pan = reverb_status_xg.pan_level;
	__m128d vpanx = _mm_set_pd(pan[1], pan[0]);
	__m256d vpan = MM256_SET2X_PD(vpanx, vpanx);
	__m256d rev_return = _mm256_set1_pd(reverb_status_xg.return_level);
	rev_return = _mm256_mul_pd(rev_return, vpan);
	for (i = 0; i < count; i += 8){
		MM256_LS_FMA_PD(&buf[i], _mm256_load_pd(&reverb_effect_buffer_sub[cdmt_buf_o][i]), rev_return);
		MM256_LS_FMA_PD(&buf[i + 4], _mm256_load_pd(&reverb_effect_buffer_sub[cdmt_buf_o][i + 4]), rev_return);
	}
	memset(reverb_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
void mix_ch_reverb_xg_thread(DATA_T *buf, int32 count, int32 byte)
{
	int32 i;
	FLOAT_T *pan = reverb_status_xg.pan_level;
	__m128 vpanx = _mm_set_ps(pan[1], pan[0], pan[1], pan[0]);
	__m256 vpan = MM256_SET2X_PS(vpanx, vpanx);
	__m256 rev_return = _mm256_set1_ps(reverb_status_xg.return_level);
	rev_return = _mm256_mul_ps(rev_return, vpan);
	for (i = 0; i < count; i += 8)
		MM256_LS_FMA_PS(&buf[i], _mm256_load_ps(&reverb_effect_buffer_sub[cdmt_buf_o][i]), rev_return);
	memset(reverb_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
void mix_ch_reverb_xg_thread(DATA_T *buf, int32 count, int32 byte)
{
	int32 i;
	FLOAT_T *pan = reverb_status_xg.pan_level;
	__m128d vpan = _mm_set_pd(pan[1], pan[0]);
	__m128d rev_return = _mm_set1_pd(reverb_status_xg.return_level);
	rev_return = _mm_mul_pd(rev_return, vpan);
	for (i = 0; i < count; i += 8){
		MM_LS_FMA_PD(&buf[i], _mm_load_pd(&reverb_effect_buffer_sub[cdmt_buf_o][i]), rev_return);
		MM_LS_FMA_PD(&buf[i + 2], _mm_load_pd(&reverb_effect_buffer_sub[cdmt_buf_o][i + 2]), rev_return);
		MM_LS_FMA_PD(&buf[i + 4], _mm_load_pd(&reverb_effect_buffer_sub[cdmt_buf_o][i + 4]), rev_return);
		MM_LS_FMA_PD(&buf[i + 6], _mm_load_pd(&reverb_effect_buffer_sub[cdmt_buf_o][i + 6]), rev_return);
	}
	memset(reverb_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
void mix_ch_reverb_xg_thread(DATA_T *buf, int32 count, int32 byte)
{
	int32 i;
	FLOAT_T *pan = reverb_status_xg.pan_level;
	__m128 vpan = _mm_set_ps(pan[1], pan[0], pan[1], pan[0]);
	__m128 rev_return = _mm_set1_ps(reverb_status_xg.return_level);
	rev_return = _mm_mul_ps(rev_return, vpan);
	for (i = 0; i < count; i += 8){
		MM_LS_FMA_PS(&buf[i], _mm_load_ps(&reverb_effect_buffer_sub[cdmt_buf_o][i]), rev_return);
		MM_LS_FMA_PS(&buf[i + 4], _mm_load_ps(&reverb_effect_buffer_sub[cdmt_buf_o][i + 4]), rev_return);
	}
	memset(reverb_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#else /* floating-point implementation */
void mix_ch_reverb_xg_thread(DATA_T *buf, int32 count, int32 byte)
{
	int32 i;
	DATA_T x;
	FLOAT_T rev_return[2] = {
		reverb_status_xg.pan_level[0] * reverb_status_xg.return_level,
		reverb_status_xg.pan_level[1] * reverb_status_xg.return_level,
	};	
	for (i = 0; i < count; i++){
		buf[i] += reverb_effect_buffer_sub[cdmt_buf_o][i] * rev_return[0];
		++i;
		buf[i] += reverb_effect_buffer_sub[cdmt_buf_o][i] * rev_return[1];
	}
	memset(reverb_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#endif

void do_multi_eq_xg_thread(int32 count, int32 byte)
{
	do_multi_eq_xg(eq_effect_buffer_sub[cdmt_buf_o], count);
}

void mix_multi_eq_xg_thread(DATA_T *buf, int32 count, int32 byte)
{
	memcpy(eq_effect_buffer_sub[cdmt_buf_i], buf, byte);
	memcpy(buf, eq_effect_buffer_sub[cdmt_buf_o], byte);				
}




/***  GM2/SD  ***/ 


void do_ch_chorus_gm2_thread(int32 count, int32 byte)
{
	mix_buffer_thread(chorus_effect_buffer_sub[cdmt_buf_o], chorus_effect_buffer_thread[cdmt_ofs_1], count);
	(*p_do_ch_chorus)(chorus_effect_buffer_sub[cdmt_buf_o], count, &(chorus_status_gs.info_stereo_chorus));
	memset(chorus_effect_buffer_thread[cdmt_ofs_1], 0, byte);
}

#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
void mix_ch_chorus_gm2_thread(DATA_T *buf, int32 count, int32 byte)
{
	InfoStereoChorus *info = &(chorus_status_gs.info_stereo_chorus);
    int32  i;
	__m256d send_reverb = _mm256_set1_pd(info->send_reverb);
	
	for(i = 0; i < count; i += 8){
		__m256d out = _mm256_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i]);
		MM256_LS_ADD_PD(&buf[i], out);
		MM256_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_1][i], out, send_reverb);
		out = _mm256_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i + 4]);
		MM256_LS_ADD_PD(&buf[i + 4], out);
		MM256_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_1][i + 4], out, send_reverb);
	}
	memset(chorus_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
void mix_ch_chorus_gm2_thread(DATA_T *buf, int32 count, int32 byte)
{
	InfoStereoChorus *info = &(chorus_status_gs.info_stereo_chorus);
    int32  i;
	__m256 send_reverb = _mm256_set1_ps(info->send_reverb);
	
	for(i = 0; i < count; i += 8){
		__m256 out = _mm256_load_ps(&chorus_effect_buffer_sub[cdmt_buf_o][i]);
		MM256_LS_ADD_PS(&buf[i], out);
		MM256_LS_FMA_PS(&reverb_effect_buffer_thread[cdmt_ofs_1][i], out, send_reverb);
	}
	memset(chorus_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
void mix_ch_chorus_gm2_thread(DATA_T *buf, int32 count, int32 byte)
{
	InfoStereoChorus *info = &(chorus_status_gs.info_stereo_chorus);
    int32  i;
	__m128d send_reverb = _mm_set1_pd(info->send_reverb);
	
	for(i = 0; i < count; i += 4){
		__m128d out = _mm_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i]);
		MM_LS_ADD_PD(&buf[i], out);
		MM_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_1][i], out, send_reverb);
		out = _mm_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i + 2]);
		MM_LS_ADD_PD(&buf[i + 2], out);
		MM_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_1][i + 2], out, send_reverb);
	}
	memset(chorus_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
void mix_ch_chorus_gm2_thread(DATA_T *buf, int32 count, int32 byte)
{
	InfoStereoChorus *info = &(chorus_status_gs.info_stereo_chorus);
    int32  i;
	__m128 send_reverb = _mm_set1_ps(info->send_reverb);
	
	for(i = 0; i < count; i += 8){
		__m128 out =_mm_load_ps(&chorus_effect_buffer_sub[cdmt_buf_o][i]);
		MM_LS_ADD_PS(&buf[i], out);
		MM_LS_FMA_PS(&reverb_effect_buffer_thread[cdmt_ofs_1][i], out, send_reverb);
		out = _mm_load_ps(&chorus_effect_buffer_sub[cdmt_buf_o][i + 4]);
		MM_LS_ADD_PS(&buf[i + 4], out);
		MM_LS_FMA_PS(&reverb_effect_buffer_thread[cdmt_ofs_1][i + 4], out, send_reverb);
	}
	memset(chorus_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#else
void mix_ch_chorus_gm2_thread(DATA_T *buf, int32 count, int32 byte)
{
	InfoStereoChorus *info = &(chorus_status_gs.info_stereo_chorus);
	int32 i;
		
	for (i = 0; i < count; i++) {
		DATA_T out = chorus_effect_buffer_sub[cdmt_buf_o][i];
		buf[i] += out;
		reverb_effect_buffer_thread[cdmt_ofs_1][i] += out * info->send_reverb;
	}
	memset(chorus_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#endif /* OPT_MODE != 0 */

void do_ch_reverb_gm2_thread(int32 count, int32 byte)
{
	mix_buffer_thread(reverb_effect_buffer_sub[cdmt_buf_o], reverb_effect_buffer_thread[cdmt_ofs_2], count);
	(*p_do_ch_reverb)(reverb_effect_buffer_sub[cdmt_buf_o], count, &reverb_status_gs);
	memset(reverb_effect_buffer_thread[cdmt_ofs_2], 0, byte);
}

#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
void mix_ch_reverb_gm2_thread(DATA_T *buf, int32 count, int32 byte)
{
    int32  i;
	
	for(i = 0; i < count; i += 8){
		MM256_LS_ADD_PD(&buf[i], _mm256_load_pd(&reverb_effect_buffer_sub[cdmt_buf_o][i]));
		MM256_LS_ADD_PD(&buf[i + 4], _mm256_load_pd(&reverb_effect_buffer_sub[cdmt_buf_o][i + 4]));
	}
	memset(reverb_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
void mix_ch_reverb_gm2_thread(DATA_T *buf, int32 count, int32 byte)
{
    int32  i;
	
	for(i = 0; i < count; i += 8)
		MM256_LS_ADD_PS(&buf[i], _mm256_load_ps(&reverb_effect_buffer_sub[cdmt_buf_o][i]));
	memset(reverb_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
void mix_ch_reverb_gm2_thread(DATA_T *buf, int32 count, int32 byte)
{
    int32  i;
	
	for(i = 0; i < count; i += 8){
		MM_LS_ADD_PD(&buf[i], _mm_load_pd(&reverb_effect_buffer_sub[cdmt_buf_o][i]));
		MM_LS_ADD_PD(&buf[i + 2], _mm_load_pd(&reverb_effect_buffer_sub[cdmt_buf_o][i + 2]));
		MM_LS_ADD_PD(&buf[i + 4], _mm_load_pd(&reverb_effect_buffer_sub[cdmt_buf_o][i + 4]));
		MM_LS_ADD_PD(&buf[i + 6], _mm_load_pd(&reverb_effect_buffer_sub[cdmt_buf_o][i + 6]));
	}
	memset(reverb_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
void mix_ch_reverb_gm2_thread(DATA_T *buf, int32 count, int32 byte)
{
    int32  i;
	
	for(i = 0; i < count; i += 8){
		MM_LS_ADD_PS(&buf[i], _mm_load_ps(&reverb_effect_buffer_sub[cdmt_buf_o][i]));
		MM_LS_ADD_PS(&buf[i + 4], _mm_load_ps(&reverb_effect_buffer_sub[cdmt_buf_o][i + 4]));
	}
	memset(reverb_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#else
void mix_ch_reverb_gm2_thread(DATA_T *buf, int32 count, int32 byte)
{
	int32 i;
	
	for (i = 0; i < count; i++)
		buf[i] += reverb_effect_buffer_sub[cdmt_buf_o][i];
	memset(reverb_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#endif


void do_mfx_effect_sd_thread(int32 count, int32 byte, int mfx_select)
{
	do_effect_list(mfx_effect_buffer_sub[cdmt_buf_o][mfx_select], count, mfx_effect_sd[mfx_select].ef);
}


#if (OPT_MODE == 1) && !defined(DATA_T_DOUBLE) && !defined(DATA_T_FLOAT) /* fixed-point implementation */
void mix_mfx_effect_sd_thread(DATA_T *buf, int32 count, int32 byte, int mfx_select)
{
	int32 i;
	DATA_T x, *out_ptr;
	int32 send_chorus =  mfx_effect_sd[mfx_select].chorus_leveli,
		send_reverb = mfx_effect_sd[mfx_select].reverb_leveli;
	
	if(!buf) // NULL
		out_ptr = direct_buffer;
	else
		out_ptr = buf;
	for (i = 0; i < count; i++) {
		x = mfx_effect_buffer_sub[cdmt_buf_o][mfx_select][i];
		out_ptr[i] += x;
		chorus_effect_buffer_thread[cdmt_ofs_0][i] += imuldiv24(x, send_chorus);
		reverb_effect_buffer_thread[cdmt_ofs_0][i] += imuldiv24(x, send_reverb);
	}
	memset(mfx_effect_buffer_sub[cdmt_buf_o][mfx_select], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
void mix_mfx_effect_sd_thread(DATA_T *buf, int32 count, int32 byte, int mfx_select)
{
	int32 i;
	DATA_T x, *out_ptr;
	__m256d send_chorus = _mm256_set1_pd(mfx_effect_sd[mfx_select].chorus_level),
		send_reverb = _mm256_set1_pd(mfx_effect_sd[mfx_select].reverb_level);
	
	if(!buf) // NULL
		out_ptr = direct_buffer;
	else
		out_ptr = buf;
	for(i = 0; i < count; i += 8){
		__m256d out = _mm256_load_pd(&mfx_effect_buffer_sub[cdmt_buf_o][mfx_select][i]);
		MM256_LS_ADD_PD(&out_ptr[i], out);
		MM256_LS_FMA_PD(&chorus_effect_buffer_thread[cdmt_ofs_0][i], out, send_chorus);
		MM256_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_0][i], out, send_reverb);
		out = _mm256_load_pd(&mfx_effect_buffer_sub[cdmt_buf_o][mfx_select][i + 4]);
		MM256_LS_ADD_PD(&out_ptr[i + 4], out);
		MM256_LS_FMA_PD(&chorus_effect_buffer_thread[cdmt_ofs_0][i + 4], out, send_chorus);
		MM256_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_0][i + 4], out, send_reverb);
	}
	memset(mfx_effect_buffer_sub[cdmt_buf_o][mfx_select], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
void mix_mfx_effect_sd_thread(DATA_T *buf, int32 count, int32 byte, int mfx_select)
{
	int32 i;
	DATA_T x, *out_ptr;
	__m256 send_chorus = _mm256_set1_ps(mfx_effect_sd[mfx_select].chorus_level),
		send_reverb = _mm256_set1_ps(mfx_effect_sd[mfx_select].reverb_level);
	
	if(!buf) // NULL
		out_ptr = direct_buffer;
	else
		out_ptr = buf;
	for(i = 0; i < count; i += 8){
		__m256 out = _mm256_load_ps(&mfx_effect_buffer_sub[cdmt_buf_o][mfx_select][i]);
		MM256_LS_ADD_PS(&out_ptr[i], out);
		MM256_LS_FMA_PS(&chorus_effect_buffer_thread[cdmt_ofs_0][i], out, send_chorus);
		MM256_LS_FMA_PS(&reverb_effect_buffer_thread[cdmt_ofs_0][i], out, send_reverb);
	}
	memset(mfx_effect_buffer_sub[cdmt_buf_o][mfx_select], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
void mix_mfx_effect_sd_thread(DATA_T *buf, int32 count, int32 byte, int mfx_select)
{
	int32 i;
	DATA_T x, *out_ptr;
	__m128d send_chorus = _mm_set1_pd(mfx_effect_sd[mfx_select].chorus_level),
		send_reverb = _mm_set1_pd(mfx_effect_sd[mfx_select].reverb_level);
	
	if(!buf) // NULL
		out_ptr = direct_buffer;
	else
		out_ptr = buf;
	for(i = 0; i < count; i += 4){
		__m128d out = _mm_load_pd(&mfx_effect_buffer_sub[cdmt_buf_o][mfx_select][i]);
		MM_LS_ADD_PD(&out_ptr[i], out);
		MM_LS_FMA_PD(&chorus_effect_buffer_thread[cdmt_ofs_0][i], out, send_chorus);
		MM_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_0][i], out, send_reverb);
		out = _mm_load_pd(&mfx_effect_buffer_sub[cdmt_buf_o][mfx_select][i + 2]);
		MM_LS_ADD_PD(&out_ptr[i + 2], out);
		MM_LS_FMA_PD(&chorus_effect_buffer_thread[cdmt_ofs_0][i + 2], out, send_chorus);
		MM_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_0][i + 2], out, send_reverb);
	}
	memset(mfx_effect_buffer_sub[cdmt_buf_o][mfx_select], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
void mix_mfx_effect_sd_thread(DATA_T *buf, int32 count, int32 byte, int mfx_select)
{
	int32 i;
	DATA_T x, *out_ptr;
	__m128 send_chorus = _mm_set1_ps(mfx_effect_sd[mfx_select].chorus_level),
		send_reverb = _mm_set1_ps(mfx_effect_sd[mfx_select].reverb_level);
	
	if(!buf) // NULL
		out_ptr = direct_buffer;
	else
		out_ptr = buf;
	for(i = 0; i < count; i += 8){
		__m128 out = _mm_load_ps(&mfx_effect_buffer_sub[cdmt_buf_o][mfx_select][i]);
		MM_LS_ADD_PS(&out_ptr[i], out);
		MM_LS_FMA_PS(&chorus_effect_buffer_thread[cdmt_ofs_0][i], out, send_chorus);
		MM_LS_FMA_PS(&reverb_effect_buffer_thread[cdmt_ofs_0][i], out, send_reverb);
		out = _mm_load_ps(&mfx_effect_buffer_sub[cdmt_buf_o][mfx_select][i + 4]);
		MM_LS_ADD_PS(&out_ptr[i + 4], out);
		MM_LS_FMA_PS(&chorus_effect_buffer_thread[cdmt_ofs_0][i + 4], out, send_chorus);
		MM_LS_FMA_PS(&reverb_effect_buffer_thread[cdmt_ofs_0][i + 4], out, send_reverb);
	}
	memset(mfx_effect_buffer_sub[cdmt_buf_o][mfx_select], 0, byte);
}
#else
void mix_mfx_effect_sd_thread(DATA_T *buf, int32 count, int32 byte, int mfx_select)
{
	int32 i;
	DATA_T x, *out_ptr;
	FLOAT_T send_chorus =  mfx_effect_sd[mfx_select].chorus_level,
		send_reverb = mfx_effect_sd[mfx_select].reverb_level;
	
	if(!buf) // NULL
		out_ptr = direct_buffer;
	else
		out_ptr = buf;
	for (i = 0; i < count; i++) {
		x = mfx_effect_buffer_sub[cdmt_buf_o][mfx_select][i];
		out_ptr[i] += x;
		chorus_effect_buffer_thread[cdmt_ofs_0][i] += x * send_chorus;
		reverb_effect_buffer_thread[cdmt_ofs_0][i] += x * send_reverb;
	}
	memset(mfx_effect_buffer_sub[cdmt_buf_o][mfx_select], 0, byte);
}
#endif /* OPT_MODE != 0 */


void do_ch_chorus_sd_thread(int32 count, int32 byte)
{
	mix_buffer_thread(chorus_effect_buffer_sub[cdmt_buf_o], chorus_effect_buffer_thread[cdmt_ofs_1], count);
#ifdef VST_LOADER_ENABLE
#if defined(VSTWRAP_EXT) // if support chorus VST
	if (hVSTHost != NULL && opt_normal_chorus_plus == 6){
		do_chorus_vst(chorus_effect_buffer_sub[cdmt_buf_o], count);
	}else
#endif
#endif
	do_effect_list(chorus_effect_buffer_sub[cdmt_buf_o], count, chorus_status_sd.ef);
	memset(chorus_effect_buffer_thread[cdmt_ofs_1], 0, byte);
}

#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
void mix_ch_chorus_sd_thread(DATA_T *buf, int32 count, int32 byte)
{
    int32  i;
	__m256d cho_level = _mm256_set1_pd(chorus_status_sd.chorus_level);
	__m256d rev_level = _mm256_set1_pd(chorus_status_sd.reverb_level);
	switch(*chorus_status_sd.output_select){
	case 0: // main
		for(i = 0; i < count; i += 8){
			MM256_LS_FMA_PD(&buf[i], _mm256_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i]), cho_level);
			MM256_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_1][i], _mm256_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i]), rev_level);
			MM256_LS_FMA_PD(&buf[i + 4], _mm256_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i + 4]), cho_level);
			MM256_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_1][i + 4], _mm256_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i + 4]), rev_level);
		}
		break;
	case 1: // reverb
		for(i = 0; i < count; i += 8){
			MM256_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_1][i], _mm256_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i]), cho_level);
			MM256_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_1][i + 4], _mm256_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i + 4]), cho_level);
		}
		break;
	case 2: // main+reverb	
		for(i = 0; i < count; i += 8){
			__m256d out = _mm256_mul_pd(_mm256_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i]), cho_level);
			MM256_LS_ADD_PD(&buf[i], out);
			MM256_LS_ADD_PD(&reverb_effect_buffer_thread[cdmt_ofs_1][i], out);
			out = _mm256_mul_pd(_mm256_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i + 4]), cho_level);
			MM256_LS_ADD_PD(&buf[i + 4], out);
			MM256_LS_ADD_PD(&reverb_effect_buffer_thread[cdmt_ofs_1][i + 4], out);
		}
		break;
	}
	memset(chorus_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
void mix_ch_chorus_sd_thread(DATA_T *buf, int32 count, int32 byte)
{
    int32  i;
	__m256 cho_level = _mm256_set1_ps(chorus_status_sd.chorus_level);
	__m256 rev_level = _mm256_set1_ps(chorus_status_sd.reverb_level);
	switch(*chorus_status_sd.output_select){
	case 0: // main
		for(i = 0; i < count; i += 8){
			MM256_LS_FMA_PS(&buf[i], _mm256_load_ps(&chorus_effect_buffer_sub[cdmt_buf_o][i]), cho_level);
			MM256_LS_FMA_PS(&reverb_effect_buffer_thread[cdmt_ofs_1][i], _mm256_load_ps(&chorus_effect_buffer_sub[cdmt_buf_o][i]), rev_level);
		}	
		break;
	case 1: // reverb
		for(i = 0; i < count; i += 8){
			MM256_LS_FMA_PS(&reverb_effect_buffer_thread[cdmt_ofs_1][i], _mm256_load_ps(&chorus_effect_buffer_sub[cdmt_buf_o][i]), cho_level);
		}
		break;
	case 2: // main+reverb	
		for(i = 0; i < count; i += 8){
			__m256 out = _mm256_mul_ps(_mm256_load_ps(&chorus_effect_buffer_sub[cdmt_buf_o][i]), cho_level);
			MM256_LS_ADD_PS(&buf[i], out);
			MM256_LS_ADD_PS(&reverb_effect_buffer_thread[cdmt_ofs_1][i], out);
		}
		break;
	}
	memset(chorus_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
void mix_ch_chorus_sd_thread(DATA_T *buf, int32 count, int32 byte)
{
    int32  i;
	__m128d cho_level = _mm_set1_pd(chorus_status_sd.chorus_level);
	__m128d rev_level = _mm_set1_pd(chorus_status_sd.reverb_level);
	switch(*chorus_status_sd.output_select){
	case 0: // main
		for(i = 0; i < count; i += 4){
			MM_LS_FMA_PD(&buf[i], _mm_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i]), cho_level);
			MM_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_1][i], _mm_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i]), rev_level);
			MM_LS_FMA_PD(&buf[i + 2], _mm_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i + 2]), cho_level);
			MM_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_1][i + 2], _mm_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i + 2]), rev_level);
		}	
		break;
	case 1: // reverb
		for(i = 0; i < count; i += 4){
			MM_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_1][i], _mm_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i]), cho_level);
			MM_LS_FMA_PD(&reverb_effect_buffer_thread[cdmt_ofs_1][i + 2], _mm_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i + 2]), cho_level);
		}
		break;
	case 2: // main+reverb	
		for(i = 0; i < count; i += 4){
			__m128d out = _mm_mul_pd(_mm_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i]), cho_level);
			MM_LS_ADD_PD(&buf[i], out);
			MM_LS_ADD_PD(&reverb_effect_buffer_thread[cdmt_ofs_1][i], out);
			out = _mm_mul_pd(_mm_load_pd(&chorus_effect_buffer_sub[cdmt_buf_o][i + 2]), cho_level);
			MM_LS_ADD_PD(&buf[i + 2], out);
			MM_LS_ADD_PD(&reverb_effect_buffer_thread[cdmt_ofs_1][i + 2], out);
		}
		break;
	}
	memset(chorus_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
void mix_ch_chorus_sd_thread(DATA_T *buf, int32 count, int32 byte)
{
    int32  i;
	__m128 cho_level = _mm_set1_ps(chorus_status_sd.chorus_level);
	__m128 rev_level = _mm_set1_ps(chorus_status_sd.reverb_level);
	switch(*chorus_status_sd.output_select){
	case 0: // main
		for(i = 0; i < count; i += 8){
			MM_LS_FMA_PS(&buf[i], _mm_load_ps(&chorus_effect_buffer_sub[cdmt_buf_o][i]), cho_level);
			MM_LS_FMA_PS(&reverb_effect_buffer_thread[cdmt_ofs_1][i], _mm_load_ps(&chorus_effect_buffer_sub[cdmt_buf_o][i]), rev_level);
			MM_LS_FMA_PS(&buf[i + 4], _mm_load_ps(&chorus_effect_buffer_sub[cdmt_buf_o][i + 4]), cho_level);
			MM_LS_FMA_PS(&reverb_effect_buffer_thread[cdmt_ofs_1][i + 4], _mm_load_ps(&chorus_effect_buffer_sub[cdmt_buf_o][i + 4]), rev_level);
		}
		break;
	case 1: // reverb
		for(i = 0; i < count; i += 8){
			MM_LS_FMA_PS(&reverb_effect_buffer_thread[cdmt_ofs_1][i], _mm_load_ps(&chorus_effect_buffer_sub[cdmt_buf_o][i]), cho_level);
			MM_LS_FMA_PS(&reverb_effect_buffer_thread[cdmt_ofs_1][i + 4], _mm_load_ps(&chorus_effect_buffer_sub[cdmt_buf_o][i + 4]), cho_level);
		}
		break;
	case 2: // main+reverb	
		for(i = 0; i < count; i += 8){
			__m128 out = _mm_mul_ps(_mm_load_ps(&chorus_effect_buffer_sub[cdmt_buf_o][i]), cho_level);
			MM_LS_ADD_PS(&buf[i], out);
			MM_LS_ADD_PS(&reverb_effect_buffer_thread[cdmt_ofs_1][i], out);
			out = _mm_mul_ps(_mm_load_ps(&chorus_effect_buffer_sub[cdmt_buf_o][i + 4]), cho_level);
			MM_LS_ADD_PS(&buf[i + 4], out);
			MM_LS_ADD_PS(&reverb_effect_buffer_thread[cdmt_ofs_1][i + 4], out);
		}
		break;
	}
	memset(chorus_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#else
void mix_ch_chorus_sd_thread(DATA_T *buf, int32 count, int32 byte)
{
	int32 i;
	FLOAT_T cho_level = chorus_status_sd.chorus_level;
	FLOAT_T rev_level = chorus_status_sd.reverb_level;
	switch(*chorus_status_sd.output_select){
	case 0: // main
		for (i = 0; i < count; i++) {
			buf[i] += chorus_effect_buffer_sub[cdmt_buf_o][i] * cho_level;
			reverb_effect_buffer_thread[cdmt_ofs_1][i] += chorus_effect_buffer_sub[cdmt_buf_o][i] * rev_level;
		}		
		break;
	case 1: // reverb
		for (i = 0; i < count; i++) {
			reverb_effect_buffer_thread[cdmt_ofs_1][i] += chorus_effect_buffer_sub[cdmt_buf_o][i] * cho_level;
		}
		break;
	case 2: // main+reverb
		for (i = 0; i < count; i++) {
			DATA_T out = chorus_effect_buffer_sub[cdmt_buf_o][i] * cho_level;
			buf[i] += out;
			reverb_effect_buffer_thread[cdmt_ofs_1][i] += out;
		}
		break;
	}
	memset(chorus_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#endif /* OPT_MODE != 0 */

void do_ch_reverb_sd_thread(int32 count, int32 byte)
{	
	mix_buffer_thread(reverb_effect_buffer_sub[cdmt_buf_o], reverb_effect_buffer_thread[cdmt_ofs_2], count);
#ifdef VST_LOADER_ENABLE
#if defined(VSTWRAP_EXT) // if support reverb VST
	if (hVSTHost != NULL && (opt_reverb_control >= 9 || opt_reverb_control <= -1024)){
		do_reverb_vst(reverb_effect_buffer_sub[cdmt_buf_o], count);
	}else
#endif
#endif
	do_effect_list(reverb_effect_buffer_sub[cdmt_buf_o], count, reverb_status_sd.ef);	
	memset(reverb_effect_buffer_thread[cdmt_ofs_2], 0, byte);
}

#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
void mix_ch_reverb_sd_thread(DATA_T *buf, int32 count, int32 byte)
{
    int32  i;
	__m256d rev_level = _mm256_set1_pd(reverb_status_sd.reverb_level);	
	for(i = 0; i < count; i += 8){
		MM256_LS_FMA_PD(&buf[i], _mm256_load_pd(&reverb_effect_buffer_sub[cdmt_buf_o][i]), rev_level);
		MM256_LS_FMA_PD(&buf[i + 4], _mm256_load_pd(&reverb_effect_buffer_sub[cdmt_buf_o][i + 4]), rev_level);
	}
	memset(reverb_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
void mix_ch_reverb_sd_thread(DATA_T *buf, int32 count, int32 byte)
{
    int32  i;
	__m256 rev_level = _mm256_set1_ps(reverb_status_sd.reverb_level);	
	for(i = 0; i < count; i += 8)
		MM256_LS_FMA_PS(&buf[i], _mm256_load_ps(&reverb_effect_buffer_sub[cdmt_buf_o][i]), rev_level);
	memset(reverb_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
void mix_ch_reverb_sd_thread(DATA_T *buf, int32 count, int32 byte)
{
    int32  i;
	__m128d rev_level = _mm_set1_pd(reverb_status_sd.reverb_level);	
	for(i = 0; i < count; i += 8){
		MM_LS_FMA_PD(&buf[i], _mm_load_pd(&reverb_effect_buffer_sub[cdmt_buf_o][i]), rev_level);
		MM_LS_FMA_PD(&buf[i + 2], _mm_load_pd(&reverb_effect_buffer_sub[cdmt_buf_o][i + 2]), rev_level);
		MM_LS_FMA_PD(&buf[i + 4], _mm_load_pd(&reverb_effect_buffer_sub[cdmt_buf_o][i + 4]), rev_level);
		MM_LS_FMA_PD(&buf[i + 6], _mm_load_pd(&reverb_effect_buffer_sub[cdmt_buf_o][i + 6]), rev_level);
	}
	memset(reverb_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
void mix_ch_reverb_sd_thread(DATA_T *buf, int32 count, int32 byte)
{
    int32  i;	
	__m128 rev_level = _mm_set1_ps(reverb_status_sd.reverb_level);
	for(i = 0; i < count; i += 8){
		MM_LS_FMA_PS(&buf[i], _mm_load_ps(&reverb_effect_buffer_sub[cdmt_buf_o][i]), rev_level);
		MM_LS_FMA_PS(&buf[i + 4], _mm_load_ps(&reverb_effect_buffer_sub[cdmt_buf_o][i + 4]), rev_level);
	}
	memset(reverb_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#else
void mix_ch_reverb_sd_thread(DATA_T *buf, int32 count, int32 byte)
{
	int32 i;
	FLOAT_T rev_level = reverb_status_sd.reverb_level;	
	for (i = 0; i < count; i++)
		buf[i] += reverb_effect_buffer_sub[cdmt_buf_o][i] * rev_level;
	memset(reverb_effect_buffer_sub[cdmt_buf_o], 0, byte);
}
#endif

void do_multi_eq_sd_thread(int32 count, int32 byte)
{
	do_multi_eq_sd(eq_effect_buffer_sub[cdmt_buf_o], count);
}

void mix_multi_eq_sd_thread(DATA_T *buf, int32 count, int32 byte)
{
	memcpy(eq_effect_buffer_sub[cdmt_buf_i], buf, byte);
	memcpy(buf, eq_effect_buffer_sub[cdmt_buf_o], byte);				
}








/************************************ master_effect ***************************************/


typedef struct _me_thread_var{
	int mono;
	int32 byte, nsamples, count;
} me_thread_var;

me_thread_var me_cv[2];


static void	reset_effect_thread_var(void)
{
	memset(me_cv, 0, sizeof(me_cv));		
}


static ALIGN DATA_T master_effect_buffer_thread[2][AUDIO_BUFFER_SIZE * 2];

void do_master_effect_thread(void)
{
#ifdef TEST_FFT
	test_fft(master_effect_buffer_thread[cdmt_buf_o], me_cv[cdmt_buf_o].nsamples);
#endif
#ifdef TEST_FIR_EQ
	apply_fir_eq(&test_fir_eq, master_effect_buffer_thread[cdmt_buf_o], me_cv[cdmt_buf_o].nsamples);
#endif
#ifdef TEST_FX
	test_fx(master_effect_buffer_thread[cdmt_buf_o], me_cv[cdmt_buf_o].nsamples);
#endif
	// elion add.
	mix_compressor(master_effect_buffer_thread[cdmt_buf_o], me_cv[cdmt_buf_o].nsamples);
	if(noise_sharp_type)
		ns_shaping(master_effect_buffer_thread[cdmt_buf_o], me_cv[cdmt_buf_o].nsamples);	
	if (opt_limiter)
		do_limiter(master_effect_buffer_thread[cdmt_buf_o], me_cv[cdmt_buf_o].count);

#ifdef VST_LOADER_ENABLE
#ifndef MASTER_VST_EFFECT2
	do_master_vst(master_effect_buffer_thread[cdmt_buf_o], me_cv[cdmt_buf_o].nsamples, me_cv[cdmt_buf_o].mono);
#endif /* MASTER_VST_EFFECT2 */
#endif /* VST_LOADER_ENABLE */
}



void do_effect_thread(DATA_T *buf, int32 count, int32 byte)
{
	me_cv[cdmt_buf_i].byte  = byte;
	me_cv[cdmt_buf_i].count = count;
	me_cv[cdmt_buf_i].nsamples = count * 2;
	me_cv[cdmt_buf_i].mono = 0;

	memcpy(master_effect_buffer_thread[cdmt_buf_i], buf, me_cv[cdmt_buf_i].byte);
	memcpy(buf, master_effect_buffer_thread[cdmt_buf_o], me_cv[cdmt_buf_o].byte);	
}




/************************************ effect_sub_thread ***************************************/

#ifdef MULTI_THREAD_COMPUTE2

static effect_sub_thread_func_t est_func0 = NULL;
static effect_sub_thread_func_t est_func1 = NULL;
static void *est_ptr0 = NULL;
static void *est_ptr1 = NULL;

int set_effect_sub_thread(effect_sub_thread_func_t func, void *ptr, int num)
{
	//if(!compute_thread_ready) // single thread
	//	return 1;
	//if(compute_thread_ready < 4)
	//	return 1; // 
	if(!func)
		return 1; // error
	if(func == est_func0 && ptr == est_ptr0)
		return 0; // 
	if(func == est_func1 && ptr == est_ptr1)
		return 0; // 
	if(est_func0 && est_func1)
		return 1; // 
	if(est_func0 == NULL){
		est_func0 = func;
		est_ptr0 = ptr;
		return 0;
	}
	if(est_func1 == NULL){
		est_func1 = func;
		est_ptr1 = ptr;
		return 0;
	}
	return 1; // error
}

void reset_effect_sub_thread(effect_sub_thread_func_t func, void *ptr)
{
	if(func == est_func0 && ptr == est_ptr0){
		est_func0 = NULL;
		est_ptr0 = NULL;
	}else if(func == est_func1 && ptr == est_ptr1){
		est_func1 = NULL;
		est_ptr1 = NULL;
	}else if(func == NULL && ptr == NULL){
		est_func0 = NULL;
		est_ptr0 = NULL;
		est_func1 = NULL;
		est_ptr1 = NULL;
	}
}

static void effect_sub_thread0(int thread_num)
{
	if(est_func0)
		est_func0(thread_num, est_ptr0);
}

static void effect_sub_thread1(int thread_num)
{
	if(est_func1)
		est_func1(thread_num, est_ptr1);
}

void go_effect_sub_thread(effect_sub_thread_func_t func, void *ptr, int num)
{
	if(func == est_func0 && ptr == est_ptr0)
		go_compute_thread_sub0(effect_sub_thread0, num);
	else if(func == est_func1 && ptr == est_ptr1)
		go_compute_thread_sub1(effect_sub_thread1, num);
}

#endif // MULTI_THREAD_COMPUTE2

/************************************ inialize_effect ***************************************/


void init_effect_buffer_thread(void)
{
	memset(direct_buffer, 0, sizeof(direct_buffer));
	memset(reverb_effect_buffer, 0, sizeof(reverb_effect_buffer));
	memset(delay_effect_buffer, 0, sizeof(delay_effect_buffer));
	memset(chorus_effect_buffer, 0, sizeof(chorus_effect_buffer));
	memset(eq_buffer, 0, sizeof(eq_buffer));
	memset(insertion_effect_buffer, 0, sizeof(insertion_effect_buffer));

	memset(eq_buffer_thread, 0, sizeof(eq_buffer_thread));
	memset(chorus_effect_buffer_thread, 0, sizeof(chorus_effect_buffer_thread));
	memset(delay_effect_buffer_thread, 0, sizeof(delay_effect_buffer_thread));
	memset(reverb_effect_buffer_thread, 0, sizeof(reverb_effect_buffer_thread));	
	
	memset(insertion_effect_buffer_sub, 0, sizeof(insertion_effect_buffer_sub));
	memset(mfx_effect_buffer_sub, 0, sizeof(mfx_effect_buffer_sub));
	memset(eq_effect_buffer_sub, 0, sizeof(eq_effect_buffer_sub));
	memset(chorus_effect_buffer_sub, 0, sizeof(chorus_effect_buffer_sub));
	memset(delay_effect_buffer_sub, 0, sizeof(delay_effect_buffer_sub));
	memset(reverb_effect_buffer_sub, 0, sizeof(reverb_effect_buffer_sub));

	reset_effect_thread_var();
}


