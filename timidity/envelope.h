


#ifndef ___ENVELOPE_H_
#define ___ENVELOPE_H_



enum { // curve
	LINEAR_CURVE = 0,
	SF2_CONCAVE,
	SF2_CONVEX,
	DEF_VOL_CURVE,
	GS_VOL_CURVE,
	XG_VOL_CURVE,
	SF2_VOL_CURVE,
	MODENV_VOL_CURVE,
};


enum { // stage
	ENV0_END_STAGE = 0,
	ENV0_ATTACK_STAGE,	 // GUS_ATTACK
	ENV0_HOLD_STAGE,	 // GUS_DECAY1
	ENV0_DECAY_STAGE,	 // GUS_DECAY2
	ENV0_SUSTAIN_STAGE,	 // GUS_SUSTAIN
	ENV0_RELEASE1_STAGE, // SF2_RELEASE
	ENV0_RELEASE2_STAGE, // SF2_NULL
	ENV0_RELEASE3_STAGE, // SF2_NULL
	ENV0_RELEASE4_STAGE, // extend stage
	ENV0_STAGE_LIST_MAX, // last
};

enum { // note
	ENV0_NOTE_NULL = 0,
	ENV0_NOTE_ON_WAIT,
	ENV0_NOTE_ON,
	ENV0_NOTE_OFF_WAIT,
	ENV0_NOTE_OFF,
	ENV0_NOTE_CUT_WAIT,
	ENV0_NOTE_CUT,
};

typedef struct {
	int stage, status, curve[ENV0_STAGE_LIST_MAX];
	int32 delay, offdelay, count[ENV0_STAGE_LIST_MAX];
	FLOAT_T volume;
	double rate[ENV0_STAGE_LIST_MAX], offset[ENV0_STAGE_LIST_MAX], follow[ENV0_STAGE_LIST_MAX];
	double vol, add_vol[ENV0_STAGE_LIST_MAX], init_vol[ENV0_STAGE_LIST_MAX], target_vol[ENV0_STAGE_LIST_MAX];
} Envelope0;

#define ENV0_KEEP (0)
#define ENV0_OFFSET_MAX (0x3FFFFFFFL)
#define DIV_ENV0_OFFSET_MAX ((double)9.3132257548284025442119711490459e-10) // 1 / (double)OFFSET_MAX;
#define DIV_RELEASE4_TIME (100.0) // 1/(10ms/1000.0)

enum {
	ENV1_END_STAGE = 0,
	ENV1_DELAY_STAGE = 1,
	ENV1_ATTACK_STAGE,
	ENV1_HOLD_STAGE,
	ENV1_DECAY_STAGE,
	ENV1_SUSTAIN_STAGE,
	ENV1_OFFDELAY_STAGE,
	ENV1_RELEASE_STAGE,
	ENV1_STAGE_LIST_MAX, // last
};

#define MAX_ENV1_LENGTH (1.0e30)
#define MIN_ENV1_LENGTH (1.0e-30)
#define DAMPER_SUSTAIN_TIME (30000) //ms
#define ENVELOPE_KEEP (-1)

typedef struct {
	int curve;
	FLOAT_T length, length_b;
	FLOAT_T div_length; // 1.0 / length
	FLOAT_T start;
	FLOAT_T end; 
	FLOAT_T init_vol;
	FLOAT_T target_vol;
} Envelope1Stage;

typedef struct {
	int stage, curve;
	FLOAT_T volume, vol;
	int32 count;
	Envelope1Stage stage_p[ENV1_STAGE_LIST_MAX]; //delay, attack, hold, decay, sustain, offdelay, release;
} Envelope1;

typedef struct {
	int stage;
	FLOAT_T vol[2];
	int32 count;
	FLOAT_T length, div_length;
	FLOAT_T init_vol[2];
	FLOAT_T target_vol[2];
	FLOAT_T mlt_vol[2];
} Envelope2;

typedef struct {
	int stage;
	FLOAT_T vol;
	int32 count;
	FLOAT_T length, div_length;
	FLOAT_T init_vol;
	FLOAT_T target_vol;
	FLOAT_T mlt_vol;
} Envelope3;

enum {
	ENV4_END_STAGE = 0,
	ENV4_ATTACK_STAGE,
	ENV4_DECAY_STAGE,
	ENV4_SUSTAIN_STAGE,
	ENV4_RELEASE_STAGE,
	ENV4_STAGE_LIST_MAX, // last
};

typedef struct {
	int status, stage;
	FLOAT_T vol;
	int32 count;
	int32 delay, offdelay;
	FLOAT_T length;
	FLOAT_T init_vol, target_vol;
	FLOAT_T mlt_vol;
	FLOAT_T stage_vol[ENV4_STAGE_LIST_MAX];
	FLOAT_T stage_len[ENV4_STAGE_LIST_MAX];
} Envelope4;

#endif // ___ENVELOPE_H_


extern void init_envelope0(Envelope0 *env);
extern void apply_envelope0_param(Envelope0 *env);
extern void reset_envelope0_release(Envelope0 *env, int32 rate);
extern void reset_envelope0_damper(Envelope0 *env, int8 damper);
extern int compute_envelope0(Envelope0 *env, int32 cnt);
extern int compute_envelope0_delay(Envelope0 *env, int32 cnt);
extern int check_envelope0(Envelope0 *env);

extern void init_envelope1(Envelope1 *env, FLOAT_T delay_cnt, FLOAT_T attack_cnt, FLOAT_T hold_cnt, FLOAT_T decay_cnt,
	FLOAT_T sustain_vol, FLOAT_T sustain_cnt, FLOAT_T release_cnt, int up_curve, int curve);
extern void reset_envelope1_release(Envelope1 *env, double release_length);
extern void reset_envelope1_decay(Envelope1 *env);
extern void reset_envelope1_sustain(Envelope1 *env);
extern void reset_envelope1_half_damper(Envelope1 *env, int8 damper);
extern int compute_envelope1(Envelope1 *env, int32 cnt);
extern int check_envelope1(Envelope1 *env);

extern void set_envelope_noteoff(int v);
extern void set_envelope_damper(int v, int8 damper);

extern void init_envelope2(Envelope2 *env, FLOAT_T vol1, FLOAT_T vol2, FLOAT_T time_cnt);
extern void reset_envelope2(Envelope2 *env, FLOAT_T target_vol1, FLOAT_T target_vol2, FLOAT_T time_cnt);
extern void compute_envelope2(Envelope2 *env, int32 cnt);
extern int check_envelope2(Envelope2 *env);

extern void init_envelope3(Envelope3 *env, FLOAT_T vol, FLOAT_T time_cnt);
extern void reset_envelope3(Envelope3 *env, FLOAT_T target_vol, FLOAT_T time_cnt);
extern void compute_envelope3(Envelope3 *env, int32 count);
extern int check_envelope3(Envelope3 *env);

extern void init_envelope4(Envelope4 *env, 
	FLOAT_T delay_cnt, FLOAT_T offdelay_cnt,
	FLOAT_T ini_lv, 
	FLOAT_T atk_lv, FLOAT_T atk_cnt, 
	FLOAT_T dcy_lv, FLOAT_T dcy_cnt, 
	FLOAT_T sus_lv, FLOAT_T sus_cnt, 
	FLOAT_T rls_lv, FLOAT_T rls_cnt );
extern void reset_envelope4_noteoff(Envelope4 *env);
extern void compute_envelope4(Envelope4 *env, int32 count);
extern int check_envelope4(Envelope4 *env);