

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#ifdef __POCC__
#include <sys/types.h>
#endif // for off_t

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <math.h>
#ifdef __W32__
#include <windows.h>
#endif /* __W32__ */
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif /* HAVE_PTHREAD_H */
#ifdef HAVE_PTHREADS_H
#include <pthreads.h>
#endif /* HAVE_PTHREADS_H */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include "timidity.h"
#include "common.h"
#include "optcode.h"
#include "instrum.h"
#include "playmidi.h"
#include "controls.h"

#include "thread.h"
#ifdef __W32__
#include "w32g.h"
#endif /* __W32__ */



/*
win以外のunix/linux/mac等ではposix

Winでのスレッド構造テスト
test1 毎回beginthread 非常に遅い
test2 毎回ResumeThread/SuspendThread 多スレッドで遅い Sleepループによる不要な空回りがでCPU負荷高い
test3 ずっとスレッド回しっぱなし やや遅い 動作が怪しい ボツ
test4 再生中まわしっぱなし一定時間後スレッド終了 CPUスレッド数のとき遅くなる (コントロール用スレッド追加の分 CPUスレッド数より実行スレッドが多くなる
test5 Eventでスレッドをコントロール 多スレッドで速い Sleepループによる不要な空回りがないのでCPU負荷少ない スレッド終了イベント待機のWaitForMultiが遅いような・・？
test6 test5のスレッド終了イベント待機を_mm_pause()ループにしたもの test5より速い メインスレッドの負荷は増える
test7 test6+load_balance 細かく分割したjobを空いてるスレッドに割り当て スレッド間の処理量の差を減らす mutex使用 test6より速い
test8 test7のmutexをCRITICAL_SECTIONに変更  test7より速い
test9 test8+reload 一度実行したスレッド割り当てを記録し 一定時間同期なしで実行 test7くらいに遅くなった ボツ
*/


int compute_thread_num = 0;
int compute_thread_ready = 0;

void go_compute_thread(thread_func_t fnc, int num);
void terminate_compute_thread(void);
void begin_compute_thread(void);
void reset_compute_thread(void);
static void do_compute_null(int thread_num){}
static thread_func_t do_compute_func = do_compute_null;
static int compute_thread_job = 0, compute_thread_job_cnt = 0;

#if MULTI_THREAD_COMPUTE2
static thread_func_t mtcs_func0 = do_compute_null;
static thread_func_t mtcs_func1 = do_compute_null;
static int mtcs_job_num0 = 0, mtcs_job_cnt0 = 0;
static int mtcs_job_num1 = 0, mtcs_job_cnt1 = 0;
static int8 mtcs_job_flg0[MTC2_JOB_MAX] = {0};
static int8 mtcs_job_flg1[MTC2_JOB_MAX] = {0};
#endif

#if defined(MULTI_THREAD_COMPUTE) && defined(__W32__)

static int thread_exit = 0;
static HANDLE hComputeThread[MAX_THREADS - 1];
VOLATILE DWORD ComputeThreadID[MAX_THREADS - 1];
static DWORD ComputeThreadPriority = THREAD_PRIORITY_NORMAL;
static HANDLE hEventTcv[MAX_THREADS - 1];
static ALIGN uint8 thread_finish_all[MAX_THREADS]; // byte*16=64bit*2=128bit
static ALIGN uint8 thread_finish[MAX_THREADS]; // byte*16=64bit*2=128bit
CRITICAL_SECTION critThread;

void set_compute_thread_priority(DWORD var)
{
	ComputeThreadPriority = var;
}

#if (USE_X86_EXT_INTRIN >= 3)
#define THREAD_WAIT_MAIN _mm_pause(); // SSE2
#define THREAD_WAIT_SUB Sleep(0);
#else
#define THREAD_WAIT_MAIN Sleep(0);
#define THREAD_WAIT_SUB Sleep(0);
// 他のスレッドに処理を渡す(最小時間) これがないと負荷増加
// OSデフォルトでは1~15の指定時 15.6ms 
#endif


static void compute_thread_core(int thread_num)
{	
#if MULTI_THREAD_COMPUTE2
	for(;;){
		int job_num, job_nums0 = 0, job_nums1 = 0;
		EnterCriticalSection(&critThread); // single thread ~
		job_num = (compute_thread_job_cnt++);
		LeaveCriticalSection(&critThread); // ~ single thread
		if(job_num < compute_thread_job)
			do_compute_func(job_num);
		if(mtcs_job_num0){
			for(;;){
				EnterCriticalSection(&critThread); // single thread ~
				job_nums0 = (mtcs_job_cnt0++);
				LeaveCriticalSection(&critThread); // ~ single thread
				if(job_nums0 >= mtcs_job_num0) break;
				mtcs_func0(job_nums0);
				mtcs_job_flg0[job_nums0] = 0;
			}
		}
		if(mtcs_job_num1){
			for(;;){
				EnterCriticalSection(&critThread); // single thread ~
				job_nums1 = (mtcs_job_cnt1++);
				LeaveCriticalSection(&critThread); // ~ single thread
				if(job_nums1 >= mtcs_job_num1) break;
				mtcs_func1(job_nums1);
				mtcs_job_flg1[job_nums1] = 0;
			}
		}
		if(job_num >= compute_thread_job && job_nums0 >= mtcs_job_num0 && job_nums1 >= mtcs_job_num1)
			break;
	}
#elif 1 // load_balance // 空いてるスレッドにジョブ割り当て
	for(;;){
		int job_num;
		EnterCriticalSection(&critThread); // single thread ~
		job_num = (compute_thread_job_cnt++);
		LeaveCriticalSection(&critThread); // ~ single thread
		if(job_num >= compute_thread_job) break;
		do_compute_func(job_num);
	}
#else // normal // スレッドに均等にジョブ割り当て
	int i;
	for (i = thread_num; i < compute_thread_job; i += compute_thread_ready)
		do_compute_func(i);
#endif
}

static void WINAPI ComputeThread(void *arglist)
{
	const int thread_num = (int)arglist;
	
	for(;;){	
		int num;	
		WaitForSingleObject(hEventTcv[thread_num], INFINITE); // スレッド開始イベント待機
		if(thread_exit) break;		
		compute_thread_core(thread_num + 1); // 1~15
		ResetEvent(hEventTcv[thread_num]); // スレッド開始イベントリセット
		thread_finish[thread_num] = 1; // スレッド終了フラグセット
	}
	crt_endthread();
}

#ifdef MULTI_THREAD_COMPUTE2
static inline void compute_thread_sub_wait(int8 *ptr)
{
// (MTC2_JOB_MAX == 16)
	// byte*8を64bit単位で比較 
	uint64 *ptr1 = (uint64 *)ptr;
	while(ptr1[0] || ptr1[1])
		THREAD_WAIT_MAIN
}

void go_compute_thread_sub0(thread_func_t fnc, int num)
{
	int i;
	if(!compute_thread_job)
		return; // error
	if(mtcs_job_num0)
		return; // error
	if(!fnc || num < 1)
		return; // error
	for(i = 0; i < num; i++)
		mtcs_job_flg0[i] = 1;
	mtcs_func0 = fnc;
	mtcs_job_cnt0 = 0;
	mtcs_job_num0 = num; // start flag
	compute_thread_core(0);
	compute_thread_sub_wait(mtcs_job_flg0);
	mtcs_job_num0 = 0; // end flag
	mtcs_func0 = do_compute_null;
}

void go_compute_thread_sub1(thread_func_t fnc, int num)
{
	int i;
	if(!compute_thread_job)
		return; // error
	if(mtcs_job_num1)
		return; // error
	if(!fnc || num < 1)
		return; // error
	for(i = 0; i < num; i++)
		mtcs_job_flg1[i] = 1;
	mtcs_func1 = fnc;
	mtcs_job_cnt1 = 0;
	mtcs_job_num1 = num; // start flag
	compute_thread_core(0);
	compute_thread_sub_wait(mtcs_job_flg1);
	mtcs_job_num1 = 0; // end flag
	mtcs_func1 = do_compute_null;
}
#endif // MULTI_THREAD_COMPUTE2

static inline void compute_thread_wait(void)
{	
// (MAX_THREADS == 16)
#if (USE_X86_EXT_INTRIN >= 6)
	// byte*8を128bit単位で比較
	__m128i vec = _mm_load_si128((__m128i *)thread_finish_all);
	while(!_mm_testc_si128(_mm_load_si128((__m128i *)thread_finish), vec)) // SSE4.1 !(finish == finish_all)
		THREAD_WAIT_MAIN
#else
	// byte*8を64bit単位で比較 
	uint64 *ptr1 = (uint64 *)&thread_finish, *ptr2 = (uint64 *)&thread_finish_all;
	while(ptr1[0] != ptr2[0] || ptr1[1] != ptr2[1])
		THREAD_WAIT_MAIN
#endif
}

void go_compute_thread(thread_func_t fnc, int num)
{
	const int thread = compute_thread_ready - 1;
	int i;

	do_compute_func = fnc;
	compute_thread_job = num;
	compute_thread_job_cnt = 0;
#ifdef MULTI_THREAD_COMPUTE2
	mtcs_func0 = do_compute_null;
	mtcs_job_num0 = 0;
	mtcs_job_cnt0 = 0;
	mtcs_func1 = do_compute_null;
	mtcs_job_num1 = 0;
	mtcs_job_cnt1 = 0;
	memset(mtcs_job_flg0, 0, sizeof(mtcs_job_flg0));
	memset(mtcs_job_flg1, 0, sizeof(mtcs_job_flg1));
#endif // MULTI_THREAD_COMPUTE2
#if (USE_X86_EXT_INTRIN >= 3) && (MAX_THREADS == 16)
	_mm_store_si128((__m128i *)thread_finish, _mm_setzero_si128());  // スレッド終了フラグリセット
	for(i = 0; i < thread; i++)
		SetEvent(hEventTcv[i]); // スレッド開始イベントセット (再開)
#else
	for(i = 0; i < thread; i++){
		thread_finish[i] = 0;  // スレッド終了フラグリセット
		SetEvent(hEventTcv[i]); // スレッド開始イベントセット (再開)
	}
#endif
	compute_thread_core(0);
	compute_thread_wait(); // 全スレッド終了待機
}

static void init_compute_thread_param(void)
{
	static int thread_init = 0;
	int i;

	if(thread_init)
		return;
	for(i = 0; i < (MAX_THREADS - 1); i++){
		hComputeThread[i] = NULL;
	}	
	memset(thread_finish, 0, MAX_THREADS);
	memset(thread_finish_all, 0, MAX_THREADS);	
	InitializeCriticalSection(&critThread);	
	compute_thread_ready = 0;
	thread_init = 1; // set init flag
}

static int check_compute_thread(void)
{	
	int i, flg = 0;

	for(i = 0; i < (MAX_THREADS - 1); i++){
		if(hComputeThread[i] != NULL)
			flg++;
	}
	if(flg){
		terminate_compute_thread();
		compute_thread_ready = 0;
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "ERROR ComputeThread : already thread exsist.");
		return 1;	
	}
	return 0;	
}

void terminate_compute_thread(void)
{
	int i;
	DWORD status;
	
	thread_exit = 1;
	compute_thread_job = 0; // デッドロック対策
#ifdef MULTI_THREAD_COMPUTE2
	mtcs_job_num0 = 0; // デッドロック対策
	memset(mtcs_job_flg0, 0, sizeof(mtcs_job_flg0));
	mtcs_job_num1 = 0; // デッドロック対策
	memset(mtcs_job_flg1, 0, sizeof(mtcs_job_flg1));
#endif // MULTI_THREAD_COMPUTE2
	for(i = 0; i < (MAX_THREADS - 1); i++){
		if(hComputeThread[i] == NULL)
			continue;		
		SetEvent(hEventTcv[i]);
		switch(WaitForSingleObject(hComputeThread[i], 10)) {
		case WAIT_OBJECT_0:
			break;
		case WAIT_TIMEOUT:
			status = WaitForSingleObject(hComputeThread[i], 1000);
			if(status == WAIT_TIMEOUT)
				TerminateThread(hComputeThread[i], 0);
			break;
		default:
			TerminateThread(hComputeThread[i], 0);
			break;
		}
		CloseHandle(hComputeThread[i]);
		hComputeThread[i] = NULL;
	}	
	for(i = 0; i < (MAX_THREADS - 1); i++){
		if(hEventTcv[i] != NULL){
			CloseHandle(hEventTcv[i]);
			hEventTcv[i] = NULL;
		}
	}
	memset(thread_finish, 0, MAX_THREADS);
	memset(thread_finish_all, 0, MAX_THREADS);	
	DeleteCriticalSection(&critThread);	
	compute_thread_ready = 0;
	thread_exit = 0;
	uninit_compute_data_midi_thread();
}

// load ini, load config , init_playmidi()... の後にする
void begin_compute_thread(void)
{
	int i, pnum, error = 0;			
	SYSTEM_INFO sys_info;

	GetSystemInfo(&sys_info);
	pnum = sys_info.dwNumberOfProcessors;
	ctl->cmsg(CMSG_INFO, VERB_NORMAL, "NumberOfProcessors : %d", pnum);

	init_compute_thread_param(); // init thread param	
	if(check_compute_thread()) // check thread exist
		return;	
	if(compute_thread_num < 2){ // check thread num
		compute_thread_num = 0;
		ctl->cmsg(CMSG_INFO, VERB_NORMAL, "SetComputeThread : OFF");
	}else{
		if(compute_thread_num > MAX_THREADS){ // check thread num
			compute_thread_num = MAX_THREADS;
			ctl->cmsg(CMSG_INFO, VERB_NORMAL, "ERROR ComputeThread : Limit MAX_THREADS:%d", MAX_THREADS);
		}
		ctl->cmsg(CMSG_INFO, VERB_NORMAL, "SetComputeThread : %d", compute_thread_num);
	}
	if(compute_thread_num == 0) // check multi thread
		return;		
	// beginthread after CreateEvent
	InitializeCriticalSection(&critThread);	
	for(i = 0; i < (compute_thread_num - 1); i++){	
		char thread_desc[64] = "";

		hEventTcv[i] = CreateEvent(NULL,FALSE,FALSE,NULL); // reset manual
		thread_finish_all[i] = 1; // 1byte full bit
		if(hEventTcv[i] == NULL){
			++error;
			ctl->cmsg(CMSG_INFO, VERB_NORMAL, "ERROR ComputeThread: CreateEvent(%d) error.", i);
			break;
		}
		hComputeThread[i] = crt_beginthreadex(
			NULL, 
			0,
			(LPTHREAD_START_ROUTINE)ComputeThread, 
			(LPVOID)i, /* void *arglist = thread_num */
			0, /* initflag = NULL or CREATE_SUSPENDED */
			&(ComputeThreadID[i])
		);
		if(hComputeThread[i] == NULL){
			++error;
			ctl->cmsg(CMSG_INFO, VERB_NORMAL, "ERROR ComputeThread : beginthread(%d) error.", i);
			break;
		}

		snprintf(thread_desc, sizeof(thread_desc) / sizeof(thread_desc[0]), "Compute Thread #%d", i);
		set_thread_description((ptr_size_t)hComputeThread[i], thread_desc);
	}	
	if(error){
		terminate_compute_thread();
		compute_thread_ready = 0;
		ctl->cmsg(CMSG_INFO, VERB_NORMAL, "SetComputeThread : OFF");
		return;
	}
	compute_thread_ready = compute_thread_num;
	switch(ComputeThreadPriority){
	case THREAD_PRIORITY_LOWEST:
	case THREAD_PRIORITY_BELOW_NORMAL:
	case THREAD_PRIORITY_NORMAL:
	case THREAD_PRIORITY_ABOVE_NORMAL:
	case THREAD_PRIORITY_HIGHEST:
	case THREAD_PRIORITY_TIME_CRITICAL:
		for(i = 0; i < (compute_thread_ready - 1); i++)
			if(!SetThreadPriority(hComputeThread[i], ComputeThreadPriority)){
				ctl->cmsg(CMSG_INFO, VERB_NORMAL, "ERROR ComputeThread : Invalid priority");
			}
		break;
	default:
		ctl->cmsg(CMSG_INFO, VERB_NORMAL, "ERROR ComputeThread : Invalid priority");
		break;
	}	
	init_compute_data_midi_thread();
    return;
}

void reset_compute_thread(void)
{
	if(compute_thread_num == compute_thread_ready)
		return;
	terminate_compute_thread();
	begin_compute_thread();
}


#elif 0 // defined(MULTI_THREAD_COMPUTE) && (defined(HAVE_PTHREAD_H) || defined(HAVE_PTHREADS_H)) && defined(HAVE_PTHREAD_CREATE)
/*
一応動いてはいるが非常に遅い・・CPU使用率が全然上がらない・・
cond_wait/cond_signalが遅いのでは・・？
Linuxでは高速なスレッド機能はないのか？ 他に何かあるのか？

使用する場合は各ファンクションの仕様を確認した上で動作テストする
参考にしたのは PTHREADS-WIN32 RELEASE 2.8.0 (2006-12-22) で POSIXそのものではないので・・
Win専用部分をPTHREADファンクションに置き換えたもの

CRITICAL_SECTIONはpthread_mutex_ , EVENTはpthread_cond_等で代替

pthread_create()		以前から使用してるのでたぶんOK
pthread_join()			以前から使用してるのでたぶんOK
pthread_????			スレッドがある/なしの状態取得 不明 (なくても動く
pthread_mutex_init()	引数があやしい 共有フラグ？？
pthread_mutex_destroy()	CloseHandle()と同じ 
pthread_mutex_lock()	WaitForSingleObject()と同じ
pthread_mutex_unlock()	ReleaseMutex()と同じ
pthread_cond_init()		引数があやしい 共有フラグ？？
pthread_cond_destroy()	CloseHandle()と同じ 
pthread_cond_wait()		WaitForSingleObject()と同じ？？ CPU時間消費するのかどうか？ mutex_lockが必要らしい
pthread_cond_signal()	SetEvent()と同じ？？
pthread_????			ResetEvent()相当のもの無い？ シグナルは自動リセット？ 
*/

static int thread_exit = 0;
static pthread_t handle_thread[MAX_THREADS - 1];
pthread_cond_t cond_thread[MAX_THREADS - 1];
static ALIGN int8 thread_finish_all[MAX_THREADS]; // byte*16=64bit*2=128bit
static ALIGN int8 thread_finish[MAX_THREADS]; // byte*16=64bit*2=128bit
pthread_mutex_t	mutex_job;
pthread_mutex_t	mutex_thread[MAX_THREADS];

#if (USE_X86_EXT_INTRIN >= 3)
#define THREAD_WAIT_MAIN _mm_pause(); // SSE2
#else
#define THREAD_WAIT_MAIN usleep(0);
#endif

static void compute_thread_core(int thread_num)
{
	if(compute_thread_job <= compute_thread_ready){
		if(thread_num < compute_thread_job)
			do_compute_func(thread_num);
	}else{	
#if 1 // load_balancer	// 空いてるスレッドにジョブ割り当て
		for(;;){
			int job_num;
			pthread_mutex_lock(&mutex_job); // ミューテックス取得
			job_num = (compute_thread_job_cnt++);
			pthread_mutex_unlock(&mutex_job); // ミューテックス解放			
			if(job_num >= compute_thread_job) break;
			do_compute_func(job_num);
		}
#else // normal // スレッドに均等にジョブ割り当て
		int i;
		for (i = thread_num; i < compute_thread_job; i += compute_thread_ready)
			do_compute_func(i);
#endif
	}
}

static void *ComputeThread(void *arglist)
{
	const int thread_num = (int)arglist;
	
	for(;;){
	//	pthread_mutex_lock(&mutex_thread[thread_num]); // ミューテックス取得
		pthread_cond_wait(&cond_thread[thread_num], &mutex_thread[thread_num]); // スレッド開始シグナル待機 . シグナル後待機状態に戻る？
	//	pthread_mutex_unlock(&mutex_thread[thread_num]);
		if(thread_exit) break;		
		compute_thread_core(thread_num + 1); // 1~15
		// ロック用のシグナルない？ cond_waitは自動リセット？
		thread_finish[thread_num] = 1;
	}
	return 0;
}

static inline void compute_thread_wait(void)
{	
// (MAX_THREADS == 16)
#if (USE_X86_EXT_INTRIN >= 6)
	 // byte*8を128bit単位で比較
	__m128i vec = _mm_load_si128((__m128i *)thread_finish_all);
	while(!_mm_testc_si128(_mm_load_si128((__m128i *)thread_finish), vec)) // SSE4.1 !(finish == finish_all)
		THREAD_WAIT_MAIN
#else // byte*8を64bit単位で比較
	uint64 *ptr1 = (uint64 *)&thread_finish, *ptr2 = (uint64 *)&thread_finish_all;
	while(ptr1[0] != ptr2[0] || ptr1[1] != ptr2[1])
		THREAD_WAIT_MAIN
#endif
}

void go_compute_thread(thread_func_t fnc, int num)
{
	const int thread = compute_thread_ready - 1;
	int i;
		
	do_compute_func = fnc;
	compute_thread_job = num;
	compute_thread_job_cnt = 0;
#if (USE_X86_EXT_INTRIN >= 3) && (MAX_THREADS == 16)
	_mm_store_si128((__m128i *)thread_finish, _mm_setzero_si128());  // スレッド終了フラグリセット
	for(i = 0; i < thread; i++){		
		pthread_mutex_lock(&mutex_thread[i]); // ミューテックス取得
		pthread_cond_signal(&cond_thread[i]); // スレッド開始シグナルセット (再開)
		pthread_mutex_unlock(&mutex_thread[i]); // ミューテックス解放
	}
#else
	for (i = 0; i < thread; i++) {
		thread_finish[i] = 0;  // スレッド終了フラグリセット		
		pthread_mutex_lock(&mutex_thread[i]); // ミューテックス取得
		pthread_cond_signal(&cond_thread[i]); // スレッド開始シグナルセット (再開)
		pthread_mutex_unlock(&mutex_thread[i]); // ミューテックス解放
	}
#endif
	compute_thread_core(0); // thread 0
	compute_thread_wait(); // 全スレッド終了待機
}

static void init_compute_thread_param(void)
{
	static int thread_init = 0;
	int i;

	if (thread_init)
		return;
	//for (i = 0; i < (MAX_THREADS - 1); i++){
	//	// ハンドルかなにかの初期化
	//}
	memset(thread_finish, 0, MAX_THREADS);
	memset(thread_finish_all, 0, MAX_THREADS);	
	compute_thread_ready = 0;
	thread_init = 1; // set init flag
}

static int check_compute_thread(void)
{
	int i, flg = 0;
	
	// ここでスレッドが残ってることはないはず
	//for (i = 0; i < (MAX_THREADS - 1); i++) {
	//	if () // スレッドがあるの条件 ハンドルかなにかのチェック？
	//		flg++;
	//}
	//if(flg){
	//	terminate_compute_thread();
	//	compute_thread_ready = 0;
	//	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "ERROR ComputeThread : already thread exist.");
	//	return 1;	
	//}
	return 0;
}

void terminate_compute_thread(void)
{
	int i;
		
	thread_exit = 1;
	compute_thread_job = 0; // デッドロック対策
	for(i = 0; i < (MAX_THREADS - 1); i++){
		//if() // スレッドがないの条件
		//	continue;	
		pthread_mutex_lock(&mutex_thread[i]);
		pthread_cond_signal(&cond_thread[i]); // スレッド開始シグナルセット (再開)
		pthread_mutex_unlock(&mutex_thread[i]);
		pthread_join(handle_thread[i], NULL);
		handle_thread[i] = NULL;
	}	
	for(i = 0; i < (MAX_THREADS - 1); i++){
		pthread_mutex_destroy(&mutex_thread[i]);
		pthread_cond_destroy(&cond_thread[i]);
	}	
	pthread_mutex_destroy(&mutex_job);
	memset(thread_finish, 0, MAX_THREADS);
	memset(thread_finish_all, 0, MAX_THREADS);	
	compute_thread_ready = 0;
	thread_exit = 0;
	uninit_compute_data_midi_thread();
}

// load ini, load config , init_playmidi()... の後にする
void begin_compute_thread(void)
{
	int i, error = 0;

	init_compute_thread_param(); // init thread param
	if (check_compute_thread()) // check thread exist
		return;
	if (compute_thread_num > MAX_THREADS) // check thread num
		compute_thread_num = MAX_THREADS;
	else if (compute_thread_num < 2) // check thread num
		compute_thread_num = 0;
	if (compute_thread_num == 0) // check multi thread
		return;
	if(pthread_mutex_init(&mutex_job, NULL)){
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "ERROR ComputeThread : pthread_mutex_init mutex_job error.");
		error++;
	}else for(i = 0; i < (compute_thread_num - 1); i++){	
		if(pthread_mutex_init(&mutex_thread[i], NULL)){
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "ERROR ComputeThread : pthread_mutex_init %d error.", i);
			error++;
			break;
		}
		if(pthread_cond_init(&cond_thread[i], NULL)){ // attrは共有設定？ 
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "ERROR ComputeThread : pthread_cond_init %d error.", i);
			error++;
			break;
		}
		thread_finish_all[i] = 1;
		if(pthread_create(&handle_thread[i], NULL, (void*)ComputeThread, (void*)i)){
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "ERROR ComputeThread : pthread_create %d error.", i);
			error++;
			break;
		}
	}		
	if(error){
		terminate_compute_thread();
		compute_thread_ready = 0;
		ctl->cmsg(CMSG_INFO, VERB_NORMAL, "SetComputeThread : OFF");
		return;
	}
	compute_thread_ready = compute_thread_num;
	init_compute_data_midi_thread();
    return;
}

void reset_compute_thread(void)
{
	if (compute_thread_num == compute_thread_ready)
		return;
	terminate_compute_thread();
	begin_compute_thread();
}

#elif defined(MULTI_THREAD_COMPUTE) && (defined(HAVE_PTHREAD_H) || defined(HAVE_PTHREADS_H)) && defined(HAVE_PTHREAD_CREATE)
/*
一応動いてはいるが・・遅いような・・？

毎回pthread_create/pthread_join
load_balancer (mutex)  // CRITICAL_SECTIONがない？のでmutex使用
*/

static pthread_t handle_thread[MAX_THREADS - 1];
pthread_mutex_t	mutex_job;

static void compute_thread_core(int thread_num)
{	
	if(compute_thread_job <= compute_thread_ready){
		if(thread_num < compute_thread_job)
			do_compute_func(thread_num);
	}else{	
#if 1 // load_balancer	// 空いてるスレッドにジョブ割り当て
		for(;;){
			int job_num;
			pthread_mutex_lock(&mutex_job); // ミューテックス取得
			job_num = (compute_thread_job_cnt++);
			pthread_mutex_unlock(&mutex_job); // ミューテックス解放			
			if(job_num >= compute_thread_job) break;
			do_compute_func(job_num);
		}
#else // normal // スレッドに均等にジョブ割り当て
		int i;
		for (i = thread_num; i < compute_thread_job; i += compute_thread_ready)
			do_compute_func(i);
#endif
	}
}

static void *ComputeThread(void *arglist)
{
	compute_thread_core((int)arglist + 1); // 1~15
	return 0;
}

void go_compute_thread(thread_func_t fnc, int num)
{
	int i;
		
	do_compute_func = fnc;
	compute_thread_job = num;
	compute_thread_job_cnt = 0;
	for (i = 0; i < (compute_thread_ready - 1); i++)
		pthread_create(&handle_thread[i], NULL, (void*)ComputeThread, (void*)i);
	compute_thread_core(0);
	for (i = 0; i < (compute_thread_ready - 1); i++)
		pthread_join(handle_thread[i], NULL);
}

static void init_compute_thread_param(void)
{
	static int thread_init = 0;
	int i;

	if (thread_init)
		return;
	compute_thread_ready = 0;
	thread_init = 1; // set init flag
}

void terminate_compute_thread(void)
{
	pthread_mutex_destroy(&mutex_job);
	compute_thread_ready = 0;
	uninit_compute_data_midi_thread();
}

// load ini, load config , init_playmidi()... の後にする
void begin_compute_thread(void)
{
	int i, error = 0;

	init_compute_thread_param(); // init thread param
	if (compute_thread_num > MAX_THREADS) // check thread num
		compute_thread_num = MAX_THREADS;
	else if (compute_thread_num < 2) // check thread num
		compute_thread_num = 0;
	if (compute_thread_num == 0) // check multi thread
		return;	
	if(pthread_mutex_init(&mutex_job, NULL)){
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "ERROR ComputeThread : pthread_mutex_init thread_core error.");
		error++;
	}		
	if(error){
		terminate_compute_thread();
		compute_thread_ready = 0;
		ctl->cmsg(CMSG_INFO, VERB_NORMAL, "SetComputeThread : OFF");
		return;
	}
	compute_thread_ready = compute_thread_num;
	init_compute_data_midi_thread();
    return;
}

void reset_compute_thread(void)
{
	if (compute_thread_num == compute_thread_ready)
		return;
	terminate_compute_thread();
	begin_compute_thread();
}
#endif /* MULTI_THREAD_COMPUTE */