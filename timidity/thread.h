



#ifndef ___THREAD_H_
#define ___THREAD_H_


#ifdef __W32__
#if defined(ENABLE_THREAD) // && (defined(IA_W32GUI) || defined(IA_W32G_SYN) || defined(KBTIM) || defined(TIM_CUI))
#define MULTI_THREAD_COMPUTE 1
#endif
#endif /* __W32__ */

#if defined(__linux__) || defined(__unix__) || \
    defined(__SUNOS__) || defined(SOLARIS) || \
    defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || \
    defined(__MACOS__) || defined(__MACOSX__)
#if defined(ENABLE_THREAD) && (defined(HAVE_PTHREAD_H) || defined(HAVE_PTHREADS_H)) && defined(HAVE_PTHREAD_CREATE)
#define MULTI_THREAD_COMPUTE 1
#endif
#endif /* pthread platform */

typedef void (*thread_func_t)(int thread_num);

#else


#endif //  ___THREAD_H_

#define MAX_THREADS 16
// optimize 16 , thread.c compute_thread_wait()
// 8 <= threads , thread_paymidi.c cdm_job_num
// thread_mix.c voice_buffer_thread[
// thread_int_synth.c is_resample_buffer_thread[

extern int compute_thread_num;
extern int compute_thread_ready;

#ifdef MULTI_THREAD_COMPUTE
extern void begin_compute_thread(void);
extern void terminate_compute_thread(void);
extern void reset_compute_thread(void);
extern void go_compute_thread(thread_func_t fnc, int num);

#define CDM_JOB_NUM 16
// 13 <= threads , thread_paymidi.c cdm_job_num
// thread_mix.c voice_buffer_thread[
// thread_int_synth.c is_resample_buffer_thread[

extern void init_compute_data_midi_thread(void);
extern void uninit_compute_data_midi_thread(void);
static void do_compute_data_midi_thread(int32 count);

extern void mix_voice_thread(DATA_T *buf, int v, int32 c, int thread);
extern void compute_voice_scc_thread(int v, DATA_T *ptr, int32 count, int thread);
extern void compute_voice_mms_thread(int v, DATA_T *ptr, int32 count, int thread);


#if 0 // test
extern void do_compute_effect_thread1(int thread_num);
#endif



#endif // MULTI_THREAD_COMPUTE

