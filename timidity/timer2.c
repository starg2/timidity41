

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */
#ifdef __W32__
#include <process.h>
#endif
#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif /* HAVE_STDDEF_H */
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"

#ifdef __W32__
#if defined(IA_W32GUI)
#include "w32g.h"
#endif

#else // other
 // HAVE_SIGNAL ?


#endif



#include "timer2.h"


/*
他の用途でも使えるようにしたが・・

win以外のunix/linux/mac等ではposixで?
winCUI window handle ? わからん

configの整備もしてない HAVE_TIMERとか?
posix部分はただの推測
*/

#ifdef __W32__
#ifdef IA_W32GUI
timer_func_t timer_func[MAX_TIMER_ID];

static inline void timer_event(UINT id)
{	
	if(timer_func[id])
		timer_func[id]();
}

LRESULT CALLBACK TimerProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	if(wParam < (UINT)MAX_TIMER_ID){
		timer_event((UINT)wParam);
		return 0; // ok ?
	}else
		return 1; // error ?
}

int start_timer(uint32 id, timer_func_t fnc, uint32 ms)
{	
	if(timer_func[id] != NULL)
		return 0; // already start timer
	if(SetTimer(hMainWnd, id, ms, (TIMERPROC)TimerProc)){
		timer_func[id] = fnc;
		return 0; // ok
	}else{
		timer_func[id] = NULL;
		return 1; // error
	}
}

void stop_timer(uint32 id)
{
	if(timer_func[id] == NULL)
		return; // not start timer
	if(KillTimer(hMainWnd, (UINT_PTR)id)){
		timer_func[id] = NULL;
		return; // ok
	}else{
		timer_func[id] = NULL;
		return; // error
	}
}

#else /* IA_W32GUI */

int start_timer(uint32 id, timer_func_t fnc, uint32 ms){return 1;}
void stop_timer(uint32 id){}

#endif /* IA_W32GUI */

#else /* other */

#if 0 // 

timer_func_t timer_func[MAX_TIMER_ID];

int init_timer_flg = 0;
struct sigaction sa[MAX_TIMER_ID];
timer_t timerID[MAX_TIMER_ID];
struct itimerspec timer_spec[MAX_TIMER_ID];

static inline void timer_event(UINT id)
{	
	if(timer_func[id])
		timer_func[id]();
}

// signo にTIMER_ID設定できるならハンドラは1個でいいのかも

#define CREATE_HANDLER_FUNCTION(id) \
	void TimerHandler##id(int signo){timer_event(##id);}

CREATE_HANDLER_FUNCTION(0)
CREATE_HANDLER_FUNCTION(1)

typedef void (*timer_handler_t)(int signo);

timer_handler_t timer_handler[MAX_TIMER_ID] = {
	TimerHandler0,
//	TimerHandler1,
}

static int init_timer(void) // 0:ok 1:error
{
	// ハンドラ登録/設定
	for(i = 0; i < MAX_TIMER_ID; i++){
		sa[id].sa_handler = timer_handler[id];
		sa[id].sa_flags = 0;
		sigemptyset(&(sa[id].sa_mask));
		if(sigaction(SIGALRM, &sa[id], NULL) == -1){			
			return 1;
		}
	}
	init_timer_flg = 1;
	return 0;
}

int start_timer(uint32 id, timer_func_t fnc, uint32 ms)
{	
	int nsec;

	if(!init_timer_flg)
		if(init_timer())
			return 1;
	if(timer_func[id] != NULL)
		return 0; // already start timer
	// タイマー作成/設定
	nsec = ms * 1000;
	timer_spec[id].it_interval.tv_sec = 0;
    timer_spec[id].it_interval.tv_nsec = nsec;
    timer_spec[id].it_value.tv_sec = 0;
    timer_spec[id].it_value.tv_nsec = nsec;
	if(timer_create(CLOCK_REALTIME,NULL,&timerID[id]) == -1){
		timer_func[id] = NULL;
		return 1; // error
	}
	if(timer_settime(timerID[id],0,&timer_spec[id],NULL) == -1){
		timer_func[id] = NULL;
		return 1; // error
    }else{
		timer_func[id] = fnc;
		return 0; // ok
	}
}

void stop_timer(uint32 id)
{
	if(timer_func[id] == NULL)
		return; // not start timer
	// タイマー削除
	if(timer_delete(timerID[id]) == -1)	{
		timer_func[id] = NULL;
		return; // error
	}else{
		timer_func[id] = NULL;
		return; // ok
	}
}

#else

int start_timer(uint32 id, void *fnc, uint32 ms){return 1;}
void stop_timer(uint32 id){}

#endif
#endif /* other */



