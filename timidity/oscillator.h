
#ifndef ___OSCILLATOR_H_
#define ___OSCILLATOR_H_



#include "envelope.h"


enum { // OSC_TYPE
	OSC_TYPE_SINE = 0,
	OSC_TYPE_TRIANGULAR,	
	OSC_TYPE_SAW1, 
	OSC_TYPE_SAW2,
	OSC_TYPE_SQUARE,
	OSC_TYPE_LIST_MAX, // last
};

typedef FLOAT_T (*osc_type_t)(FLOAT_T in);

typedef struct _Oscillator {
	int mode, wave_type;
	int32 delay;
	FLOAT_T freq, rate, init_phase, out;
	Envelope3 env;
	osc_type_t osc_ptr;
} Oscillator;

typedef struct _Oscillator2 {
	int mode, wave_type;
	FLOAT_T freq, rate, init_phase, phase_diff, out[2];
	osc_type_t osc_ptr;
} Oscillator2;

#if 0
#define OSC_MULTI_PHASE_MAX 8

typedef struct _OscillatorMulti {
	int mode, phase_num;
	FLOAT_T freq, rate, init_phase, phase_diff[OSC_MULTI_PHASE_MAX], out[OSC_MULTI_PHASE_MAX];
} OscillatorMulti;
#endif

#endif // ___OSCILLATOR_H_





extern void init_oscillator(Oscillator *osc, FLOAT_T freq, int32 delay_count, int32 attack_count, int wave_type, FLOAT_T init_phase);
extern void reset_oscillator(Oscillator *osc, FLOAT_T freq);
extern void compute_oscillator(Oscillator *osc, int32 count);

extern void init_oscillator2(Oscillator2 *osc, FLOAT_T freq, int wave_type, FLOAT_T init_phase, FLOAT_T phase_diff);
extern void reset_oscillator2(Oscillator2 *osc, FLOAT_T freq);
extern void compute_oscillator2(Oscillator2 *osc, int32 count);

#if 0
extern void init_oscillator_multi(Oscillator *osc, int phase_num, FLOAT_T freq, FLOAT_T *phase_diff);
extern void reset_oscillator_multi(Oscillator *osc, FLOAT_T freq);
extern void compute_oscillator_multi(Oscillator *osc, int32 count);
#endif

extern int check_osc_wave_type(int tmpi, int limit);
extern osc_type_t get_osc_ptr(int tmpi);