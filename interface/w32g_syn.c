/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2018 Masanao Izumo <iz@onicos.co.jp>
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

    w32g_syn.c: Written by Daisuke Aoki <dai@y7.net>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */


#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */
#include <process.h>
#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif /* HAVE_STDDEF_H */
#include <windows.h>
#undef RC_NONE
#include <shlobj.h>
#include <commctrl.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <shlobj.h>
#include <windowsx.h>   /* There is no <windowsx.h> on CYGWIN.
                         * Edit_* and ListBox_* are defined in
                         * <windowsx.h>
                         */

#ifdef TWSYNSRV
#include <winsvc.h>
#ifdef __DMC__
#define SERVICE_ACCEPT_PARAMCHANGE    8
#endif
//#include <lmcons.h>
#include <stdarg.h>
#endif /* TWSYNSRV */

#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "output.h"
#include "controls.h"
#include "rtsyn.h"

#ifdef WIN32GCC
WINAPI void InitCommonControls(void);
#endif

#include "w32g.h"
#include "w32g_utl.h"
#include "w32g_pref.h"
#include "w32g_res.h"
#include "w32g_int_synth_editor.h"

#ifdef IA_W32G_SYN

#ifdef USE_TWSYN_BRIDGE
#include "twsyn_bridge_common.h"
#include "twsyn_bridge_host.h"
#endif

typedef struct w32g_syn_t_ {
	UINT nid_uID;
#ifndef TWSYNSRV
	HWND nid_hWnd;
	HICON hIcon;
	HICON hIconStart;
	HICON hIconPause;
#endif /* !TWSYNSRV */
	int argc;
	char **argv;
	HANDLE gui_hThread;
	DWORD gui_dwThreadId;
	HANDLE syn_hThread;
	DWORD syn_dwThreadId;
//	int syn_ThreadPriority;
	HANDLE hMutex;
	int volatile quit_state;
} w32g_syn_t;
static w32g_syn_t w32g_syn;

// 各種変数 (^^;;;
HINSTANCE hInst = NULL;
extern int RestartTimidity;

extern void CmdLineToArgv(LPSTR lpCmdLine, int *argc, CHAR ***argv);

static int start_syn_thread(void);
static void WINAPI syn_thread(void);


#ifndef TWSYNSRV

// Task tray version here

static LRESULT CALLBACK SynWinProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static void VersionWnd(HWND hParentWnd);
static void TiMidityWnd(HWND hParentWnd);

static int w32g_syn_create_win(void);

#define HAVE_SYN_CONSOLE 1
#define HAVE_SYN_SOUNDSPEC 1

#define MYWM_NOTIFYICON (WM_USER + 501)
#define MYWM_QUIT (WM_USER + 502)
#define W32G_SYNWIN_CLASSNAME "TWSYNTH GUI"
#define W32G_SYN_TIP "TWSYNTH GUI"

///r
// ポップアップメニュー
#define IDM_NOTHING                            400
#define IDM_QUIT                               401
#define IDM_START                              402
// #define IDM_STOP                            403
#define IDM_RESTART                            404

#define IDM_VERSION                            405
#define IDM_TIMIDITY                           406

#define IDM_PREFERENCE                         410
#define IDM_CONSOLE_WND                        411
#define IDM_VSTMGR_WND                         412
#define IDM_ISEDITOR_WND                       413

#define IDM_SYSTEM_RESET                       415

#define IDM_SYSTEM_RESET_X                     420 // <= IDM_SYSTEM_RESET_X + system_mode_list_num - 1
// reserve ~429
#define IDM_CHANGE_SYSTEM_X                    430 // <= IDM_CHANGE_SYSTEM_X_X + system_mode_list_num - 1
// reserve ~439
#define IDM_PROCESS_PRIORITY                   440 // <= IDM_PROCESS_PRIORITY + process_priority_list_num - 1
// reserve ~449
#define IDM_SYN_THREAD_PRIORITY                450 // <= IDM_SYN_THREAD_PRIORITY + syn_thread_priority_list_num - 1
// reserve ~459
#define IDM_MODULE                             500 // <= IDM_MODULE + module_list_num - 1
// reserve ~549


#ifdef HAVE_SYN_CONSOLE
HWND hConsoleWnd;
void InitConsoleWnd(HWND hParentWnd);
#endif /* HAVE_SYN_CONSOLE */
#ifdef HAVE_SYN_SOUNDSPEC
HWND hSoundSpecWnd;
void InitSoundSpecWnd(HWND hParentWnd);
#endif /* HAVE_SYN_SOUNDSPEC */

#else  // !TWSYNSRV

// Windows service version here

#undef HAVE_SYN_CONSOLE
#undef HAVE_SYN_SOUNDSPEC

static SERVICE_STATUS_HANDLE serviceStatusHandle;
static DWORD currentServiceStatus;
static const char *serviceName = "Timidity";
static const char *serviceDescription = "Realtime synthesize midi message";
static const char *regKeyTwSynSrv = "SYSTEM\\CurrentControlSet\\Services\\Timidity";

static BOOL quietInstaller = FALSE;
static BOOL InstallService();
static BOOL UninstallService();

#endif	// !TWSYNSRV


#define W32G_SYN_NID_UID 12301
#define W32G_MUTEX_NAME TEXT("TWSYNTH MUTEX")

#define W32G_SYN_MESSAGE_MAX              100
#define W32G_SYN_NONE                     0
#define W32G_SYN_QUIT	                  10
#define W32G_SYN_START	                  11		// 演奏状態へ移行
#define W32G_SYN_STOP	                  12		// 演奏停止状態へ移行
///r
#define W32G_SYN_SYSTEM_RESET             15
#define W32G_SYN_SYSTEM_RESET_X           20 // IDM_SYSTEM_RESET_X と同じ並びであること
// reserve ~29
#define W32G_SYN_CHANGE_SYSTEM_X          30 // IDM_CHANGE_SYSTEM_X と同じ並びであること
// reserve ~39
#define W32G_SYN_MODULE                   50 // IDM_MODULE と同じ並びであること
// reserve ~99



typedef struct w32g_syn_message_t_ {
	int cmd;
} w32g_syn_message_t;
static volatile enum { stop, run, quit, none } w32g_syn_status, w32g_syn_status_prev;
#ifndef MAX_PORT
#define MAX_PORT 4
#endif
int w32g_syn_id_port[MAX_PORT];
int w32g_syn_port_num = 2;

extern int win_main(int argc, char **argv);
extern int ctl_pass_playing_list2(int n, char *args[]);
extern void winplaymidi(void);

w32g_syn_message_t msg_loopbuf[W32G_SYN_MESSAGE_MAX];
int msg_loopbuf_start = -1;
int msg_loopbuf_end = -1;
extern int rtsyn_system_mode;
HANDLE msg_loopbuf_hMutex = NULL; // 排他処理用
int syn_AutoStart;                // シンセ自動起動
extern DWORD processPriority;            // プロセスのプライオリティ
extern DWORD syn_ThreadPriority;         // シンセスレッドのプライオリティ

extern int volatile stream_max_compute;	// play_event() の compute_data() で計算を許す最大時間。

static int w32g_syn_main(void);
static int start_syn_thread(void);
static void WINAPI syn_thread(void);
static void terminate_syn_thread(void); // w32g_pref.c
static int wait_for_termination_of_syn_thread(void);
extern int w32g_message_set(int cmd);
extern int w32g_message_get(w32g_syn_message_t *msg);
extern int w32g_syn_ctl_pass_playing_list(int n_, char *args_[]);
extern int w32g_syn_do_before_pref_apply(void);
extern int w32g_syn_do_after_pref_apply(void);


/*
  構造
	　メインスレッド：GUIのメッセージループ
	　シンセサイザースレッド：発音部分
*/
int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
				LPSTR lpCmdLine, int nCmdShow)
{
	HANDLE hMutex;
	int i;

	
	Sleep(100); // Restartで前プロセスの終了待機
#ifdef TIMIDITY_LEAK_CHECK
	_CrtSetDbgFlag(CRTDEBUGFLAGS);
#endif

	// 今のところ２重起動はできないようにしとく。
	hMutex = OpenMutex(0, FALSE, W32G_MUTEX_NAME);
	if (hMutex) {
		CloseHandle(hMutex);
		return 0;
	}
	w32g_syn.hMutex = CreateMutex(NULL, TRUE, W32G_MUTEX_NAME);
	if (!w32g_syn.hMutex) {
		return 0;
	}

	CmdLineToArgv(lpCmdLine, &w32g_syn.argc, &w32g_syn.argv);

#ifdef TWSYNSRV
	// Service install and uninstall handling
	for (i = 1; i < w32g_syn.argc; i++)
	{
		if (!stricmp(w32g_syn.argv[i], "/INSTALL"))
		{
			InstallService();

			ReleaseMutex(w32g_syn.hMutex);
			CloseHandle(w32g_syn.hMutex);

			return 0;
		}
		else if (!stricmp(w32g_syn.argv[i], "/UNINSTALL"))
		{
			UninstallService();

			ReleaseMutex(w32g_syn.hMutex);
			CloseHandle(w32g_syn.hMutex);

			return 0;
		}
	}
#endif /* TWSYNSRV */

//	wrdt = wrdt_list[0];

	hInst = hInstance;
	w32g_syn.gui_hThread = GetCurrentThread();
	w32g_syn.gui_dwThreadId = GetCurrentThreadId();
	w32g_syn.quit_state = 0;

	w32g_syn_main();

	for (i = 0; i < w32g_syn.argc; i++) {
		safe_free(w32g_syn.argv[i]);
	}
	safe_free(w32g_syn.argv);
	w32g_syn.argv = NULL;
	w32g_syn.argc = 0;

	ReleaseMutex(w32g_syn.hMutex);
	CloseHandle(w32g_syn.hMutex);
	
	if(RestartTimidity){
		PROCESS_INFORMATION pi;
		STARTUPINFO si;
		CHAR path[FILEPATH_MAX] = "";
		RestartTimidity = 0;
		memset(&si, 0, sizeof(si));
		si.cb  = sizeof(si);
		GetModuleFileName(hInstance, path, MAX_PATH);
		if(CreateProcess(path, lpCmdLine, NULL, NULL, TRUE, CREATE_DEFAULT_ERROR_MODE, NULL, NULL, &si, &pi) == FALSE)
			MessageBox(NULL, "Restart Error.", "TiMidity++ Synth Win32GUI", MB_OK | MB_ICONEXCLAMATION);
	}
	return 0;
}

#ifndef TWSYNSRV

// Task tray version here

static int w32g_syn_create_win(void)
{
	WNDCLASSEX wndclass;
	wndclass.cbSize        = sizeof(WNDCLASSEX);
	wndclass.style         = CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc   = SynWinProc;
	wndclass.cbClsExtra    = 0;
	wndclass.cbWndExtra    = 0;
	wndclass.hInstance     = hInst;
	wndclass.hIcon         = w32g_syn.hIcon;
	wndclass.hIconSm       = w32g_syn.hIcon;
	wndclass.hCursor       = LoadCursor(0, IDC_ARROW);
	wndclass.hbrBackground = (HBRUSH)(COLOR_SCROLLBAR + 1);
	wndclass.lpszMenuName  = NULL;
	wndclass.lpszClassName =  W32G_SYNWIN_CLASSNAME;
	RegisterClassEx(&wndclass);
	w32g_syn.nid_hWnd = CreateWindowEx(WS_EX_TOOLWINDOW, W32G_SYNWIN_CLASSNAME, 0,
		WS_CLIPCHILDREN,
		CW_USEDEFAULT, 0, 10, 10, 0, 0, hInst, 0);
	if (!w32g_syn.nid_hWnd) {
		return -1;
	}
	ShowWindow(w32g_syn.nid_hWnd, SW_HIDE);
	UpdateWindow(w32g_syn.nid_hWnd);		// 必要ないと思うんだけど。
	return 0;
}

// return
// 0 : OK
// -1 : FATAL ERROR
static int w32g_syn_main(void)
{
	int i;
	MSG msg;

	InitCommonControls();

	w32g_syn.nid_uID = W32G_SYN_NID_UID;
	w32g_syn.nid_hWnd = NULL;
	w32g_syn.hIcon = LoadImage(hInst, MAKEINTRESOURCE(IDI_ICON_TIMIDITY), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
	w32g_syn.hIconStart = LoadImage(hInst, MAKEINTRESOURCE(IDI_ICON_SERVER_START), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
	w32g_syn.hIconPause = LoadImage(hInst, MAKEINTRESOURCE(IDI_ICON_SERVER_PAUSE), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
//	processPriority = NORMAL_PRIORITY_CLASS;
//	syn_ThreadPriority = THREAD_PRIORITY_NORMAL;
	for (i = 0; i < MAX_PORT; i++) {
		w32g_syn_id_port[i] = i + 1;
	}

	if (w32g_syn_create_win()) {
		MessageBox(NULL, TEXT("Fatal Error"), TEXT("ERROR"), MB_OK);
		return -1;
	}

	while (GetMessage(&msg, NULL, 0, 0)) {
		if (msg.message == MYWM_QUIT) {
			if (w32g_syn.quit_state < 1) w32g_syn.quit_state = 1;
			if (hConsoleWnd) {
				DestroyWindow(hConsoleWnd);
				hConsoleWnd = NULL;
			}
			DestroyWindow(w32g_syn.nid_hWnd);
			w32g_syn.nid_hWnd = NULL;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	while (w32g_syn.quit_state < 2) {
		Sleep(300);
	}
	
	return 0;
}

static VOID CALLBACK forced_exit(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	exit(0);
}

// Add the icon into the status area of the task bar.
BOOL AddTasktrayIcon(HWND hwnd)
{
	BOOL bRes;
	NOTIFYICONDATAA nid;
	int i;
	nid.cbSize           = sizeof(NOTIFYICONDATAA);
	nid.hWnd             = w32g_syn.nid_hWnd = hwnd;
	nid.uID              = w32g_syn.nid_uID;
	nid.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	nid.uCallbackMessage = MYWM_NOTIFYICON;
	nid.hIcon            = w32g_syn_status == run ? w32g_syn.hIconStart : w32g_syn.hIconPause;
	strcpy(nid.szTip, W32G_SYN_TIP);
	for (i = 1; i <= 20; i++) {
		bRes = Shell_NotifyIconA(NIM_ADD, &nid);
		if (bRes != FALSE || GetLastError() != ERROR_TIMEOUT) {
			break;
		}
		if (Shell_NotifyIconA(NIM_MODIFY, &nid)) {
			break;
		}
		Sleep(100);
	}
	return bRes != FALSE;
}

// Delete the icon from the status area of the task bar.
void DeleteTasktrayIcon(HWND hwnd)
{
	BOOL bRes;
	NOTIFYICONDATAA nid;
	int i;
	nid.cbSize = sizeof(NOTIFYICONDATAA);
	nid.hWnd   = w32g_syn.nid_hWnd;
	nid.uID    = w32g_syn.nid_uID;
	nid.uFlags = 0;
	for (i = 1; i <= 20; i++) {
		bRes = Shell_NotifyIconA(NIM_DELETE, &nid);
		if (bRes == TRUE)
			break;
		if (i >= 20) {
			MessageBox(NULL, TEXT("Fatal Error"), TEXT("ERROR"), MB_OK);
		}
		Sleep(100);
	}
}

void ChangeTasktrayIcon(HWND hwnd)
{
	BOOL bRes;
	NOTIFYICONDATAA nid;
	int i;
	nid.cbSize = sizeof(NOTIFYICONDATAA);
	nid.hWnd   = w32g_syn.nid_hWnd = hwnd;
	nid.uID    = w32g_syn.nid_uID;
	nid.uFlags = NIF_ICON;
	nid.hIcon  = w32g_syn_status == run ? w32g_syn.hIconStart : w32g_syn.hIconPause;
	strcpy(nid.szTip, W32G_SYN_TIP);
	for (i = 1; i <= 20; i++) {
		bRes = Shell_NotifyIconA(NIM_MODIFY, &nid);
		if (bRes != FALSE || GetLastError() != ERROR_TIMEOUT) {
			break;
		}
		Sleep(100);
	}
	return;
}

static const int system_mode_list_num = 8;
static const int system_mode_num[] = {
    GM_SYSTEM_MODE,
    GS_SYSTEM_MODE,
    XG_SYSTEM_MODE,
    GM2_SYSTEM_MODE,
    SD_SYSTEM_MODE,
    KG_SYSTEM_MODE,
    CM_SYSTEM_MODE,
    DEFAULT_SYSTEM_MODE
};
static const TCHAR *system_mode_name_reset_jp[] = {
    TEXT("GM リセット"),
    TEXT("GS リセット"),
    TEXT("XG リセット"),
    TEXT("GM2 リセット"),
    TEXT("SD リセット"),
    TEXT("KG リセット"),
    TEXT("CM リセット"),
    TEXT("デフォルトシステム リセット")
};
static const TCHAR *system_mode_name_change_jp[] = {
    TEXT("GM システムへ変更"),
    TEXT("GS システムへ変更"),
    TEXT("XG システムへ変更"),
    TEXT("GM2 システムへ変更"),
    TEXT("SD システムへ変更"),
    TEXT("KG システムへ変更"),
    TEXT("CM システムへ変更"),
    TEXT("デフォルトシステムへ変更")
};
static const TCHAR *system_mode_name_reset_en[] = {
    TEXT("GM Reset"),
    TEXT("GS Reset"),
    TEXT("XG Reset"),
    TEXT("GM2 Reset"),
    TEXT("SD Reset"),
    TEXT("KG Reset"),
    TEXT("CM Reset"),
    TEXT("Default system Reset")
};
static const TCHAR *system_mode_name_change_en[] = {
    TEXT("Change GM system"),
    TEXT("Change GS system"),
    TEXT("Change XG system"),
    TEXT("Change GM2 system"),
    TEXT("Change SD system"),
    TEXT("Change KG system"),
    TEXT("Change CM system"),
    TEXT("Change default system")
};

static const int process_priority_list_num = 6;
static const int process_priority_num[] = {
    IDLE_PRIORITY_CLASS,
    BELOW_NORMAL_PRIORITY_CLASS,
    NORMAL_PRIORITY_CLASS,
    ABOVE_NORMAL_PRIORITY_CLASS,
    HIGH_PRIORITY_CLASS,
    REALTIME_PRIORITY_CLASS
};
static const TCHAR *process_priority_name_jp[] = {
    TEXT("低い"),
    TEXT("少し低い"),
    TEXT("普通"),
    TEXT("少し高い"),
    TEXT("高い"),
    TEXT("リアルタイム")
};
static const TCHAR *process_priority_name_en[] = {
    TEXT("lowest"),
    TEXT("below normal"),
    TEXT("normal"),
    TEXT("above normal"),
    TEXT("highest"),
    TEXT("realtime")
};

static const int syn_thread_priority_list_num = 7;
static const int syn_thread_priority_num[] = {
    THREAD_PRIORITY_IDLE,
    THREAD_PRIORITY_LOWEST,
    THREAD_PRIORITY_BELOW_NORMAL,
    THREAD_PRIORITY_NORMAL,
    THREAD_PRIORITY_ABOVE_NORMAL,
    THREAD_PRIORITY_HIGHEST,
    THREAD_PRIORITY_TIME_CRITICAL
};
static const TCHAR *syn_thread_priority_name_jp[] = {
	TEXT("アイドル"),
    TEXT("低い"),
    TEXT("少し低い"),
    TEXT("普通"),
    TEXT("少し高い"),
    TEXT("高い"),
    TEXT("タイムクリティカル")
};
static const TCHAR *syn_thread_priority_name_en[] = {
	TEXT("idle"),
    TEXT("lowest"),
    TEXT("below normal"),
    TEXT("normal"),
    TEXT("above normal"),
    TEXT("highest"),
    TEXT("time critical")
};

static LRESULT CALLBACK
SynWinProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	static int have_popupmenu = 0;
	switch (uMess) {
	case WM_CREATE:
		if (AddTasktrayIcon(hwnd) == FALSE) {
			MessageBox(NULL, TEXT("Fatal Error"), TEXT("ERROR"), MB_OK);
			DestroyWindow(hwnd);
			PostQuitMessage(0);
			return -1;
		}
#ifdef USE_TWSYN_BRIDGE
		init_bridge();
#endif
#ifdef VST_LOADER_ENABLE
		if (!hVSTHost) {
#ifdef _WIN64
			hVSTHost = LoadLibrary(TEXT("timvstwrap_x64.dll"));
#else
			hVSTHost = LoadLibrary(TEXT("timvstwrap.dll"));
#endif
			if (hVSTHost) {
				((vst_open) GetProcAddress(hVSTHost, "vstOpen"))();
	//			((vst_open_config_all) GetProcAddress(hVSTHost, "openEffectEditorAll"))(hwnd);
			}
		}
#endif /* VST_LOADER_ENABLE */
		start_syn_thread();
		break;
	case WM_DESTROY:
		{
		int i;
		terminate_syn_thread();
#ifndef NDEBUG
		while(!wait_for_termination_of_syn_thread());
#else
		/* wait 16s (0.2s * 10loop * 8loop) */
		for (i = 0; i < 8; i++) {
			if (wait_for_termination_of_syn_thread())
				break;
		}
#endif
#ifdef VST_LOADER_ENABLE
		if (hVSTHost != NULL) {
			((vst_close)GetProcAddress(hVSTHost,"vstClose"))();
			FreeLibrary(hVSTHost);
			hVSTHost = NULL;
		}
#endif
#ifdef USE_TWSYN_BRIDGE
		close_bridge();
#endif
		DeleteTasktrayIcon(hwnd);
		PostQuitMessage(0);
		break;
		}
	case MYWM_NOTIFYICON:
		if ((UINT)wParam == w32g_syn.nid_uID) {
			if ((UINT)lParam == WM_RBUTTONDOWN || (UINT)lParam == WM_LBUTTONDOWN) {
				SetTimer(hwnd, (UINT_PTR)lParam, GetDoubleClickTime(), NULL);
			}
			else if ((UINT)lParam == WM_LBUTTONDBLCLK) {
				KillTimer(hwnd, WM_LBUTTONDOWN);
				PostMessage(hwnd, (WPARAM)lParam, 0, 0);
			}
			else if ((UINT)lParam == WM_RBUTTONDBLCLK) {
				KillTimer(hwnd, WM_RBUTTONDOWN);
				PostMessage(hwnd, (WPARAM)lParam, 0, 0);
			}
		}
		break;
	case WM_LBUTTONDBLCLK:
		PostMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_PREFERENCE, 0), 0);
		break;
	case WM_RBUTTONDBLCLK:
		break;
	case WM_TIMER:
		KillTimer(hwnd, (UINT_PTR)wParam);
		if ((UINT)wParam == WM_LBUTTONDOWN || (UINT)wParam == WM_RBUTTONDOWN)
///r
		{
			UINT uMess = (UINT)wParam;
			{
				int i, check, flag;
				POINT point;
				HMENU hMenu, hMenuModule, hMenuReset, hMenuChange, hMenuProcessPriority, hMenuSynPriority;
				GetCursorPos(&point);

				if (have_popupmenu)
					break;
				have_popupmenu = 1;

				hMenu = CreatePopupMenu();
				hMenuModule = CreateMenu();
				hMenuReset = CreateMenu();
				hMenuChange = CreateMenu();
				hMenuProcessPriority = CreateMenu();
				hMenuSynPriority = CreateMenu();
				if (PlayerLanguage == LANGUAGE_JAPANESE) {
					if (w32g_syn_status == run) {
						AppendMenu(hMenu, MF_STRING, IDM_STOP, TEXT("シンセ停止(&S)"));
					} else if (w32g_syn_status == stop) {
						AppendMenu(hMenu, MF_STRING, IDM_START, TEXT("シンセ開始(&S)"));
					} else if (w32g_syn_status == quit) {
						AppendMenu(hMenu, MF_STRING | MF_GRAYED, IDM_START, TEXT("終了中……"));
					}
					AppendMenu(hMenu, MF_STRING, IDM_SYSTEM_RESET, TEXT("システムリセット(&R)"));
///r
					for (i = 0; i < module_list_num && module_list[i].name; i++) {
						flag = MF_STRING;
						flag |= (i > 0 && i % (module_list_num / 2) == 0) ? MF_MENUBARBREAK : 0;
						check = opt_default_module == module_list[i].num ? MF_CHECKED : 0;
						AppendMenuA(hMenuModule, flag | check, IDM_MODULE + i, module_list[i].name);
					}

					for (i = 0; i < system_mode_list_num; i++) {
						check = rtsyn_system_mode == system_mode_num[i] ? MF_CHECKED : 0;
						if (i < system_mode_list_num - 1)
							AppendMenu(hMenuReset, MF_STRING | check, IDM_SYSTEM_RESET_X + i, system_mode_name_reset_jp[i]);
						AppendMenu(hMenuChange, MF_STRING | check, IDM_CHANGE_SYSTEM_X + i, system_mode_name_change_jp[i]);
					}

					for (i = 0; i < process_priority_list_num; i++) {
						check = processPriority == process_priority_num[i] ? MF_CHECKED : 0;
						AppendMenu(hMenuProcessPriority, MF_STRING | check, IDM_PROCESS_PRIORITY + i, process_priority_name_jp[i]);
						if (i == 4)
							AppendMenu(hMenuProcessPriority, MF_SEPARATOR, 0, 0);
					}

					for (i = 0; i < syn_thread_priority_list_num; i++) {
						check = syn_ThreadPriority == syn_thread_priority_num[i] ? MF_CHECKED : 0;
						AppendMenu(hMenuSynPriority, MF_STRING | check, IDM_SYN_THREAD_PRIORITY + i, syn_thread_priority_name_jp[i]);
						if (i == 4)
							AppendMenu(hMenuSynPriority, MF_SEPARATOR, 0, 0);
					}

					AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
					AppendMenu(hMenu, MF_POPUP, (UINT)hMenuReset, TEXT("各種システムリセット"));
					AppendMenu(hMenu, MF_POPUP, (UINT)hMenuChange, TEXT("特定のシステムへ変更"));
					AppendMenu(hMenu, MF_POPUP, (UINT)hMenuModule, TEXT("特定の音源モードへ変更"));

					if (uMess != WM_LBUTTONDOWN) {
						AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
						AppendMenu(hMenu, MF_POPUP, (UINT)hMenuProcessPriority, TEXT("プロセスプライオリティ設定"));
						AppendMenu(hMenu, MF_POPUP, (UINT)hMenuSynPriority, TEXT("シンセスレッドプライオリティ設定"));
						AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
						AppendMenu(hMenu, MF_STRING, IDM_PREFERENCE, TEXT("設定(&P)..."));
#ifdef HAVE_SYN_CONSOLE
						AppendMenu(hMenu, MF_STRING, IDM_CONSOLE_WND, TEXT("コン\x83\x5Cール(&C)"));   // コンソール
#endif /* HAVE_SYN_CONSOLE */
///r
#ifdef VST_LOADER_ENABLE
						AppendMenu(hMenu, MF_STRING, IDM_VSTMGR_WND, TEXT("VSTマネージャ(&V)"));
#endif /* VST_LOADER_ENABLE */
#ifdef HAVE_SYN_SOUNDSPEC
						AppendMenu(hMenu, MF_STRING, IDM_MWSOUNDSPEC, TEXT("スペクトログラム(&O)"));
#endif /* HAVE_SYN_SOUNDSPEC */
#ifdef INT_SYNTH
						AppendMenu(hMenu, MF_STRING, IDM_ISEDITOR_WND, TEXT("内蔵シンセエディタ"));
#endif /* INT_SYNTH */
						AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
						AppendMenu(hMenu, MF_STRING, IDM_VERSION, TEXT("バージョン情報"));
						AppendMenu(hMenu, MF_STRING, IDM_TIMIDITY, TEXT("TiMidity++ について(&A)"));
						AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
						AppendMenu(hMenu, MF_STRING, IDM_RESTART, TEXT("再起動(&R)"));
						AppendMenu(hMenu, MF_STRING, IDM_QUIT, TEXT("終了(&X)"));
					}
				} else {
					if (w32g_syn_status == run) {
						AppendMenu(hMenu, MF_STRING, IDM_STOP, TEXT("&Stop synthesizer"));
					} else if (w32g_syn_status == stop) {
						AppendMenu(hMenu, MF_STRING, IDM_START, TEXT("&Start synthesizer"));
					} else if (w32g_syn_status == quit) {
						AppendMenu(hMenu, MF_STRING | MF_GRAYED, IDM_START, TEXT("Quitting..."));
					}
					AppendMenu(hMenu, MF_STRING, IDM_SYSTEM_RESET, TEXT("System &Reset"));
///r
					for (i = 0; i < module_list_num && module_list[i].name; i++) {
						flag = MF_STRING;
						flag |= (i > 0 && i % (module_list_num / 2) == 0) ? MF_MENUBARBREAK : 0;
						check = opt_default_module == module_list[i].num ? MF_CHECKED : 0;
						AppendMenuA(hMenuModule, flag | check, IDM_MODULE + i, module_list[i].name);
					}

					for (i = 0; i < system_mode_list_num; i++) {
						check = rtsyn_system_mode == system_mode_num[i] ? MF_CHECKED : 0;
						if (i < system_mode_list_num - 1)
							AppendMenu(hMenuReset, MF_STRING | check, IDM_SYSTEM_RESET_X + i, system_mode_name_reset_en[i]);
						AppendMenu(hMenuChange, MF_STRING | check, IDM_CHANGE_SYSTEM_X + i, system_mode_name_change_en[i]);
					}

					for (i = 0; i < process_priority_list_num; i++) {
						check = processPriority == process_priority_num[i] ? MF_CHECKED : 0;
						AppendMenu(hMenuProcessPriority, MF_STRING | check, IDM_PROCESS_PRIORITY + i, process_priority_name_en[i]);
						if (i == 4)
							AppendMenu(hMenuProcessPriority, MF_SEPARATOR, 0, 0);
					}

					for (i = 0; i < syn_thread_priority_list_num; i++) {
						check = syn_ThreadPriority == syn_thread_priority_num[i] ? MF_CHECKED : 0;
						AppendMenu(hMenuSynPriority, MF_STRING | check, IDM_SYN_THREAD_PRIORITY + i, syn_thread_priority_name_en[i]);
						if (i == 4)
							AppendMenu(hMenuSynPriority, MF_SEPARATOR, 0, 0);
					}

					AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
					AppendMenu(hMenu, MF_POPUP, (UINT)hMenuReset, TEXT("Specific system reset"));
					AppendMenu(hMenu, MF_POPUP, (UINT)hMenuChange, TEXT("Change Specific system"));
					AppendMenu(hMenu, MF_POPUP, (UINT)hMenuModule, TEXT("Change Specific Module"));

					if (uMess != WM_LBUTTONDOWN) {
						AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
						AppendMenu(hMenu, MF_POPUP, (UINT)hMenuProcessPriority, TEXT("Change process priority"));
						AppendMenu(hMenu, MF_POPUP, (UINT)hMenuSynPriority, TEXT("Change synthesizer thread priority"));
						AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
						AppendMenu(hMenu, MF_STRING, IDM_PREFERENCE, TEXT("&Preferences..."));
#ifdef HAVE_SYN_CONSOLE
						AppendMenu(hMenu, MF_STRING, IDM_CONSOLE_WND, TEXT("&Console"));
#endif /* HAVE_SYN_CONSOLE */
///r
#ifdef VST_LOADER_ENABLE
						AppendMenu(hMenu, MF_STRING, IDM_VSTMGR_WND, TEXT("VST Manager(&V)"));
#endif /* VST_LOADER_ENABLE */
#ifdef HAVE_SYN_SOUNDSPEC
						AppendMenu(hMenu, MF_STRING, IDM_MWSOUNDSPEC, TEXT("S&ound Spectrogram"));
#endif /* HAVE_SYN_SOUNDSPEC */
#ifdef INT_SYNTH
						AppendMenu(hMenu, MF_STRING, IDM_ISEDITOR_WND, TEXT("Internal Synthesizer Edior"));
#endif /* INT_SYNTH */
						AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
						AppendMenu(hMenu, MF_STRING, IDM_VERSION, TEXT("Version Info"));
						AppendMenu(hMenu, MF_STRING, IDM_TIMIDITY, TEXT("&About TiMidity++"));
						AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
						AppendMenu(hMenu, MF_STRING, IDM_RESTART, TEXT("Restart(&R)"));
						AppendMenu(hMenu, MF_STRING, IDM_QUIT, TEXT("E&xit"));
					}
				}
				// ポップアップメニューがきちんと消えるための操作。
				// http://support.microsoft.com/default.aspx?scid = KB;EN-US;Q135788& 参照
#if 0		// Win 98/2000 以降用？
				{
					DWORD dwThreadID = GetWindowThreadProcessId(hwnd, NULL);
					if (dwThreadID != w32g_syn.gui_dwThreadId) {
						AttachThreadInput(w32g_syn.gui_dwThreadId, dwThreadID, TRUE);
						SetForegroundWindow(hwnd);
						AttachThreadInput(w32g_syn.gui_dwThreadId, dwThreadID, FALSE);
					} else {
						SetForegroundWindow(hwnd);
					}
				}
#else	// これでいいらしい？
				SetForegroundWindow(hwnd);
#endif
				TrackPopupMenu(hMenu, TPM_TOPALIGN | TPM_LEFTALIGN,
					point.x, point.y, 0, hwnd, NULL);
				PostMessage(hwnd, WM_NULL, 0, 0);	// これもポップアップメニューのテクニックらしい。
				DestroyMenu(hMenu);
				have_popupmenu = 0;
				return 0;
			}
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDM_RESTART:
			RestartTimidity = 1; // WinMain()
			// thru quit
		case IDM_QUIT:
#if 1/* 強制終了 */
			/* Exit after 20 sec. */
			SetTimer(NULL, 0, 20000, forced_exit);
#endif
			w32g_message_set(W32G_SYN_QUIT);
			break;
		case IDM_START:
			w32g_message_set(W32G_SYN_START);
			break;
		case IDM_STOP:
			w32g_message_set(W32G_SYN_STOP);
			break;
		case IDM_SYSTEM_RESET:
			w32g_message_set(W32G_SYN_SYSTEM_RESET);
			break;
		case IDM_PREFERENCE:
			PrefWndCreate(w32g_syn.nid_hWnd, 0);
			break;
		case IDM_VERSION:
			VersionWnd(w32g_syn.nid_hWnd);
			break;
		case IDM_TIMIDITY:
			TiMidityWnd(w32g_syn.nid_hWnd);
			break;
#ifdef HAVE_SYN_CONSOLE
		case IDM_CONSOLE_WND:
			if (!hConsoleWnd)
				InitConsoleWnd(w32g_syn.nid_hWnd);
			if (IsWindowVisible(hConsoleWnd))
				ShowWindow(hConsoleWnd, SW_HIDE);
			else
				ShowWindow(hConsoleWnd, SW_SHOW);
			break;
#endif /* HAVE_SYN_CONSOLE */
///r
#ifdef VST_LOADER_ENABLE
		case IDM_VSTMGR_WND:
			if (hVSTHost)
				((open_vst_mgr) GetProcAddress(hVSTHost, "openVSTManager"))(hwnd);
			break;
#endif /* VST_LOADER_ENABLE */
#ifdef HAVE_SYN_SOUNDSPEC
		case IDM_MWSOUNDSPEC:
			if (!hSoundSpecWnd)
				InitSoundSpecWnd(w32g_syn.nid_hWnd);
			if (IsWindowVisible(hSoundSpecWnd))
				ShowWindow(hSoundSpecWnd, SW_HIDE);
			else
				ShowWindow(hSoundSpecWnd, SW_SHOW);
			break;
#endif /* HAVE_SYN_SOUNDSPEC */
#ifdef INT_SYNTH
		case IDM_ISEDITOR_WND:
			ISEditorWndCreate(w32g_syn.nid_hWnd);
			break;
#endif /* INT_SYNTH */
		default:
///r
			// IDM_MODULE
			if (IDM_MODULE <= LOWORD(wParam) && LOWORD(wParam) < (IDM_MODULE + module_list_num))
				w32g_message_set(LOWORD(wParam) + W32G_SYN_MODULE - IDM_MODULE);
			// IDM_SYSTEM_RESET_X
			if (IDM_SYSTEM_RESET_X <= LOWORD(wParam) && LOWORD(wParam) < (IDM_SYSTEM_RESET_X + system_mode_list_num))
				w32g_message_set(LOWORD(wParam) + W32G_SYN_SYSTEM_RESET_X - IDM_SYSTEM_RESET_X);
			// IDM_SYSTEM_RESET_X
			if (IDM_CHANGE_SYSTEM_X <= LOWORD(wParam) && LOWORD(wParam) < (IDM_CHANGE_SYSTEM_X + system_mode_list_num))
				w32g_message_set(LOWORD(wParam) + W32G_SYN_CHANGE_SYSTEM_X - IDM_CHANGE_SYSTEM_X);

			// IDM_PROCESS_PRIORITY
			if (IDM_PROCESS_PRIORITY <= LOWORD(wParam) && LOWORD(wParam) < (IDM_PROCESS_PRIORITY + process_priority_list_num)) {
				processPriority = process_priority_num[LOWORD(wParam) - IDM_PROCESS_PRIORITY];
				if (w32g_syn_status == run)
					SetPriorityClass(GetCurrentProcess(), processPriority);
			}
 			// IDM_SYN_THREAD_PRIORITY
			if (IDM_SYN_THREAD_PRIORITY <= LOWORD(wParam) && LOWORD(wParam) < (IDM_SYN_THREAD_PRIORITY + syn_thread_priority_list_num)) {
				syn_ThreadPriority = syn_thread_priority_num[LOWORD(wParam) - IDM_SYN_THREAD_PRIORITY];
				if (w32g_syn_status == run)
					SetThreadPriority(w32g_syn.syn_hThread, syn_ThreadPriority);
			}
			break;
		}
		break;
	default:
		if (uMess == RegisterWindowMessage(TEXT("TaskbarCreated"))) {
			AddTasktrayIcon(hwnd);
			return 0;
		}
	  return DefWindowProc(hwnd, uMess, wParam, lParam);
	}
	return 0L;
}

static int volatile syn_thread_started = 0;
static int start_syn_thread(void)
{
	w32g_syn.syn_hThread = crt_beginthreadex(NULL, 0,
				    (LPTHREAD_START_ROUTINE) syn_thread,
				    NULL, 0, & w32g_syn.syn_dwThreadId);
	if (!w32g_syn.syn_hThread)
		return -1;
	for (;;) {
		if (syn_thread_started == 1)
			break;
		if (syn_thread_started == 2)
			return -1;
		Sleep(200);
	}
	if (syn_thread_started == 2)
		return -1;
	return 0;
}

static void WINAPI syn_thread(void)
{
	syn_thread_started = 1;
	win_main(w32g_syn.argc, w32g_syn.argv);
	syn_thread_started = 2;
}

static void terminate_syn_thread(void)
{
	w32g_message_set(W32G_SYN_QUIT);
}

static int wait_for_termination_of_syn_thread(void)
{
	int i;
	int ok = 0;
	for (i = 0; i < 10; i++) {
		if (WaitForSingleObject(w32g_syn.syn_hThread, 200) == WAIT_TIMEOUT)
			w32g_message_set(W32G_SYN_QUIT);
		else {
			ok = 1;
			break;
		}
	}
	return ok;
}

#else // !TWSYNSRV

// Windows service version here

// To debug output (Require attached debugger)
static void OutputString(const char *format, ...)
{
	char temp[256];
	va_list va;

	va_start(va, format);
	vsnprintf(temp, sizeof(temp), format, va);
	OutputDebugStringA(temp);
	va_end(va);
}

void PutsConsoleWnd(const char *str)
{
	OutputString("%s", str);
}

// To MessageBox Window (Require grant access windowstation)
static void OutputWindow(const char *format, ...)
{
	char temp[256];
	va_list va;

	va_start(va, format);
	vsnprintf(temp, sizeof(temp), format, va);
	MessageBoxA(NULL, temp, serviceName, MB_OK | MB_ICONEXCLAMATION);
	va_end(va);
}

static void OutputLastError(const char *message)
{
	LPVOID buffer;

	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&buffer, 0, NULL);
	OutputDebugString(message);
	OutputDebugString(" : ");
	OutputDebugString(buffer);
	OutputDebugString("\n");

	LocalFree(buffer);
}

static void OutputWindowLastError(const char *message)
{
	LPVOID buffer;
	char *temp;

	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&buffer, 0, NULL);

	temp = (char*) safe_malloc(strlen((const char*)buffer) + strlen(message) + 10);
	sprintf(temp, "%s : %s\n", message, buffer);

	MessageBoxA(NULL, temp, serviceName, MB_OK | MB_ICONEXCLAMATION);

	safe_free(temp);
	LocalFree(buffer);
}

// Report service status to service control manager
static BOOL ReportStatusToSCM(DWORD newServiceStatus, DWORD checkPoint, DWORD waitHint,
	DWORD win32ExitCode, DWORD serviceSpecificExitCode)
{
	BOOL result;
	SERVICE_STATUS serviceStatus;

	serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	currentServiceStatus = newServiceStatus;
	serviceStatus.dwCurrentState = newServiceStatus;
	serviceStatus.dwCheckPoint = checkPoint;
	serviceStatus.dwWaitHint = waitHint;
	serviceStatus.dwWin32ExitCode = win32ExitCode;
	serviceStatus.dwServiceSpecificExitCode = serviceSpecificExitCode;
	if (newServiceStatus == SERVICE_START_PENDING)
	{
		serviceStatus.dwControlsAccepted = 0;
	}
	else
	{
		serviceStatus.dwControlsAccepted =
			SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE | SERVICE_ACCEPT_PARAMCHANGE;
	}
	result = SetServiceStatus(serviceStatusHandle, &serviceStatus);
	if (result == FALSE)
	{
		OutputLastError("ReportStatusToSCM() == FALSE");
	}
	return result;
}

// Report service status to service control manager (Alternate version)
static BOOL PingStatusToSCM(DWORD checkPoint, DWORD waitHint)
{
	return ReportStatusToSCM(currentServiceStatus, checkPoint, waitHint, NO_ERROR, NO_ERROR);
}

// Service control message from management interface (Callback from SCM)
static void WINAPI ServiceCtrlHandler(DWORD state)
{
 	switch (state)
	{
	case SERVICE_CONTROL_STOP:
		ReportStatusToSCM(SERVICE_STOP_PENDING, 1, 0, NO_ERROR, NO_ERROR);
		w32g_message_set(W32G_SYN_QUIT);
		break;
	case SERVICE_CONTROL_PAUSE:
		ReportStatusToSCM(SERVICE_PAUSE_PENDING, 1, 0, NO_ERROR, NO_ERROR);
		w32g_message_set(W32G_SYN_STOP);
		ReportStatusToSCM(SERVICE_PAUSED, 1, 0, NO_ERROR, NO_ERROR);
		break;
	case SERVICE_CONTROL_CONTINUE:
		ReportStatusToSCM(SERVICE_CONTINUE_PENDING, 1, 0, NO_ERROR, NO_ERROR);
		w32g_message_set(W32G_SYN_START);
		ReportStatusToSCM(SERVICE_RUNNING, 1, 0, NO_ERROR, NO_ERROR);
		break;
	case SERVICE_CONTROL_INTERROGATE:
		OutputString("ServiceCtrlHandler(), SERVICE_CONTROL_INTERROGATE : oops.\n");
		break;
	case SERVICE_CONTROL_SHUTDOWN:
		OutputString("ServiceCtrlHandler(), SERVICE_CONTROL_SHUTDOWN : oops.\n");
		break;
	default:
		OutputString("ServiceCtrlHandler(), default handler (%d) : oops.\n", state);
		break;
	}
	PingStatusToSCM(0, 0);
}

// Register service control handler
static SERVICE_STATUS_HANDLE RegisterCtrlHandler()
{
	SERVICE_STATUS_HANDLE ssh = RegisterServiceCtrlHandlerA(
		serviceName, ServiceCtrlHandler);
	if (ssh == 0)
	{
		OutputLastError("RegisterServiceCtrlHandler() == 0");
		return NULL;
	}
	return ssh;
}

// Service entry function (Callback from SCM)
static void WINAPI ServiceMain(DWORD argc, LPTSTR *argv)
{
	serviceStatusHandle = RegisterCtrlHandler();
	ReportStatusToSCM(SERVICE_RUNNING, 1, 0, NO_ERROR, NO_ERROR);

	w32g_syn.syn_hThread = GetCurrentThread();
	win_main(w32g_syn.argc, w32g_syn.argv);

	ReportStatusToSCM(SERVICE_STOPPED, 1, 0, NO_ERROR, NO_ERROR);
}

// return
// 0 : OK
// -1 : FATAL ERROR
static int w32g_syn_main(void)
{
	int i;
	BOOL result;
	SERVICE_TABLE_ENTRYA ServiceTable[2];

	w32g_syn.nid_uID = W32G_SYN_NID_UID;
//	processPriority = NORMAL_PRIORITY_CLASS;
//	syn_ThreadPriority = THREAD_PRIORITY_NORMAL;
	for (i = 0; i < MAX_PORT; i++) {
		w32g_syn_id_port[i] = i + 1;
	}

	ServiceTable[0].lpServiceName = (LPSTR)serviceName;
	ServiceTable[0].lpServiceProc = ServiceMain;
	ServiceTable[1].lpServiceName = 0;
	ServiceTable[1].lpServiceProc = 0;
	
    result = StartServiceCtrlDispatcherA(ServiceTable);
    if (result == FALSE) {
#ifdef _DEBUG
        OutputWindowLastError("StartServiceCtrlDispatcher() == FALSE");
#else
        OutputLastError("StartServiceCtrlDispatcher() == FALSE");
#endif
#if 0
        // デバッグ用
        // シンセが動作するか確認するための関数呼び出し
        // 停止方法はタスクマネージャかtaskkillからの強制終了
        ServiceMain(0, 0);
#endif
        return -1;
    }
	return 0;
}

// Service installer
static BOOL InstallService(void)
{
    char twSynSrvPath[FILEPATH_MAX], serviceLongName[40];
    SC_HANDLE scm, sv;
    HKEY srvKey;

    twSynSrvPath[0] = '\0';
    GetModuleFileNameA(NULL, twSynSrvPath, FILEPATH_MAX);

    if (strchr(twSynSrvPath, ' ') != 0) {
        int len, siz;
        strcat(twSynSrvPath, "\"");

        len = (int) strlen(twSynSrvPath);
        siz = sizeof(twSynSrvPath[0]);
        MoveMemory(twSynSrvPath + siz,
                   twSynSrvPath, siz * (len + 1));
        twSynSrvPath[0] = '\"';
    }

    scm = OpenSCManagerA(
            NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_CREATE_SERVICE);
    if (!scm) {
        if (quietInstaller)
            OutputLastError("OpenSCManager() == NULL");
        else
            OutputWindowLastError("OpenSCManager() == NULL");
        return FALSE;
    }

    strcpy(serviceLongName, serviceName);
    strcat(serviceLongName, (!strstr(timidity_version, "current"))
                        ? " version " : " ");
    strcat(serviceLongName, timidity_version);
    sv = CreateServiceA(scm, serviceName, serviceLongName,
                SERVICE_CHANGE_CONFIG | SERVICE_START | SERVICE_STOP,
                SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS,
                SERVICE_DEMAND_START,
                SERVICE_ERROR_IGNORE, twSynSrvPath, NULL, NULL, NULL, NULL, NULL);
    if (!sv) {
        if (quietInstaller)
            OutputLastError("CreateService() == NULL");
        else
            OutputWindowLastError("CreateService() == NULL");
        CloseServiceHandle(scm);
        return FALSE;
    }

    CloseServiceHandle(sv);
    CloseServiceHandle(scm);

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, regKeyTwSynSrv,
                0, KEY_WRITE | KEY_READ, &srvKey) == ERROR_SUCCESS)
    {
        if (RegSetValueExA(srvKey, "Description", (unsigned long) NULL, REG_SZ,
                        (const BYTE*) serviceDescription, strlen(serviceDescription)) != ERROR_SUCCESS)
        {
            if (quietInstaller)
                OutputLastError("RegSetValueEx() != ERROR_SUCCESS");
            else
                OutputWindowLastError("RegSetValueEx() != ERROR_SUCCESS");
            RegCloseKey(srvKey);
            return FALSE;
        }
        RegCloseKey(srvKey);
    }

    if (quietInstaller)
        OutputString("%s : Service install successful.", serviceLongName);
    else
        OutputWindow("%s : Service install successful.", serviceLongName);

    return TRUE;
}

// Service uninstaller
static BOOL UninstallService(void)
{
    char serviceLongName[40];
    SC_HANDLE scm, sv;

    scm = OpenSCManagerA(
                NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_CONNECT);
    if (!scm) {
        if (quietInstaller)
            OutputLastError("OpenSCManager() == NULL");
        else
            OutputWindowLastError("OpenSCManager() == NULL");
        return FALSE;
    }

    sv = OpenServiceA(scm, serviceName, DELETE | SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (!sv) {
        if (quietInstaller)
            OutputLastError("OpenSCManager() == NULL");
        else
            OutputWindowLastError("OpenService() == NULL");
        CloseServiceHandle(scm);
        return FALSE;
    }

    if (DeleteService(sv) == FALSE) {
        if (quietInstaller)
            OutputLastError("DeleteService() == FALSE");
        else
            OutputWindowLastError("DeleteService() == FALSE");
        CloseServiceHandle(sv);
        CloseServiceHandle(scm);
        return FALSE;
    }

    CloseServiceHandle(sv);
    CloseServiceHandle(scm);

    strcpy(serviceLongName, serviceName);
    strcat(serviceLongName, (!strstr(timidity_version, "current"))
                        ? " version " : " ");
    strcat(serviceLongName, timidity_version);

    if (quietInstaller)
        OutputString("%s : Service uninstall successful.", serviceLongName);
    else
        OutputWindow("%s : Service uninstall successful.", serviceLongName);

    return TRUE;
}

#endif	// !TWSYNSRV


// 可変長引数にする予定……
// 0: 成功、1: 追加できなかった
int w32g_message_set(int cmd)
{
	int res = 0, i;
	if (!msg_loopbuf_hMutex) {
		msg_loopbuf_hMutex = CreateMutex(NULL, TRUE, NULL);
	} else {
		WaitForSingleObject(msg_loopbuf_hMutex, INFINITE);
	}
	if (cmd == W32G_SYN_QUIT){	// 優先するメッセージ。
		for(i = 0; i < W32G_SYN_MESSAGE_MAX; i++)
			msg_loopbuf[i].cmd = cmd;
		msg_loopbuf_start = 0;
		msg_loopbuf_end = 0;
		ReleaseMutex(msg_loopbuf_hMutex);
		return res;
	} else if (cmd == W32G_SYN_START || cmd == W32G_SYN_STOP) {	// 優先するメッセージ。
		msg_loopbuf_start = 0;
		msg_loopbuf_end = 0;
		msg_loopbuf[msg_loopbuf_end].cmd = cmd;
		ReleaseMutex(msg_loopbuf_hMutex);
		return res;
	} else if (cmd != W32G_SYN_NONE) {
		if (msg_loopbuf_end < 0) {
			msg_loopbuf_start = 0;
			msg_loopbuf_end = 0;
		} else if (msg_loopbuf_start <= msg_loopbuf_end) {
			if (msg_loopbuf_end < W32G_SYN_MESSAGE_MAX - 1)
				msg_loopbuf_end++;
			else
				res = 1;
		} else if (msg_loopbuf_end < msg_loopbuf_start - 1) {
			msg_loopbuf_end++;
		} else {
			res = 1;
		}
		if (res == 0) {
			msg_loopbuf[msg_loopbuf_end].cmd = cmd;
		}
	}
	ReleaseMutex(msg_loopbuf_hMutex);
	Sleep(100);
	return res;
}

int w32g_message_get(w32g_syn_message_t *msg)
{
	int have_msg = 0;
	if (!msg_loopbuf_hMutex) {
		msg_loopbuf_hMutex = CreateMutex(NULL, TRUE, NULL);
	} else {
		WaitForSingleObject(msg_loopbuf_hMutex, INFINITE);
	}
	if (msg_loopbuf_start >= 0) {
		CopyMemory(msg, &msg_loopbuf[msg_loopbuf_start], sizeof(w32g_syn_message_t));
		have_msg = 1;
		msg_loopbuf_start++;
		if (msg_loopbuf_end < msg_loopbuf_start) {
			msg_loopbuf_start = msg_loopbuf_end = -1;
		} else if (msg_loopbuf_start >= W32G_SYN_MESSAGE_MAX) {
			msg_loopbuf_start = 0;
		}
	}
	ReleaseMutex(msg_loopbuf_hMutex);
	return have_msg;
}

extern int seq_quit;
extern void rtsyn_play_event(MidiEvent*);
extern void rtsyn_server_reset();
void w32g_syn_doit(void)
{
	w32g_syn_message_t msg;
	MidiEvent ev;
	DWORD sleep_time;
	while (seq_quit == 0) {
		int have_msg = 0;
		sleep_time = 0;
		have_msg = w32g_message_get(&msg);
		if (have_msg) {
			switch (msg.cmd) {
			case W32G_SYN_QUIT:
				seq_quit = ~0;
				w32g_syn_status = quit;
				sleep_time = 100;
				break;
			case W32G_SYN_START:
				seq_quit = ~0;
				w32g_syn_status = run;
#ifndef TWSYNSRV
				ChangeTasktrayIcon(w32g_syn.nid_hWnd);
#endif /* !TWSYNSRV */
				sleep_time = 100;
				break;
			case W32G_SYN_STOP:
				seq_quit = ~0;
				w32g_syn_status = stop;
#ifndef TWSYNSRV
				ChangeTasktrayIcon(w32g_syn.nid_hWnd);
#endif /* !TWSYNSRV */
				sleep_time = 100;
				break;
///r
			default:
#ifndef TWSYNSRV
				// W32G_SYN_MODULE
				if (W32G_SYN_MODULE <= msg.cmd && msg.cmd  < (W32G_SYN_MODULE + module_list_num))
				{
					opt_default_module = module_list[msg.cmd - W32G_SYN_MODULE].num;
					rtsyn_server_reset();
					ev.type = ME_RESET;
					ev.a = rtsyn_system_mode;
					rtsyn_play_event(&ev);
					change_system_mode(rtsyn_system_mode);
					sleep_time = 100;
				}
				// W32G_SYN_SYSTEM_RESET_X
				if (W32G_SYN_SYSTEM_RESET_X <= msg.cmd && msg.cmd  < (W32G_SYN_SYSTEM_RESET_X + system_mode_list_num))
				{
					rtsyn_server_reset();
					ev.type = ME_RESET;
					ev.a = system_mode_num[msg.cmd - W32G_SYN_SYSTEM_RESET_X];
					rtsyn_play_event(&ev);
					sleep_time = 100;
				}
				// W32G_SYN_CHANGE_SYSTEM_X
				if (W32G_SYN_CHANGE_SYSTEM_X  <= msg.cmd && msg.cmd  < (W32G_SYN_CHANGE_SYSTEM_X  + system_mode_list_num))
				{
					rtsyn_system_mode = system_mode_num[msg.cmd - W32G_SYN_CHANGE_SYSTEM_X];
					rtsyn_server_reset();
					ev.type = ME_RESET;
					ev.a = system_mode_num[msg.cmd - W32G_SYN_CHANGE_SYSTEM_X];
					rtsyn_play_event(&ev);
					change_system_mode(rtsyn_system_mode);
					sleep_time = 100;
					break;
				}
#endif /* !TWSYNSRV */
				break;
			}
		}

		winplaymidi();
		Sleep(sleep_time);
	}
}

int w32g_syn_ctl_pass_playing_list(int n_, char *args_[])
{
	int i;
#ifndef TWSYNSRV
	w32g_syn_status = syn_AutoStart ? run : stop;
#else
	w32g_syn_status = run;
#endif /* !TWSYNSRV */
	for (;;) {
		int breakflag = 0;
		switch (w32g_syn_status) {
		default:
		case quit:
			breakflag = 1;
			break;
		case run:
			{
				int result;
				char args_[MAX_PORT][10];
				char *args[MAX_PORT];
				if (w32g_syn_port_num <= 0) {
					w32g_syn_status = stop;
					break;
				} else if (w32g_syn_port_num > MAX_PORT) {
					w32g_syn_port_num = MAX_PORT;
				}
				for (i = 0; i < MAX_PORT; i++) {
					args[i] = args_[i];
					sprintf(args[i], "%d", w32g_syn_id_port[i]);
				}
#ifndef TWSYNSRV
				ChangeTasktrayIcon(w32g_syn.nid_hWnd);
#endif /* !TWSYNSRV */
				SetPriorityClass(GetCurrentProcess(), processPriority);
				SetThreadPriority(w32g_syn.syn_hThread, syn_ThreadPriority);
				result = ctl_pass_playing_list2(w32g_syn_port_num, args);
				SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
				SetThreadPriority(w32g_syn.syn_hThread, THREAD_PRIORITY_NORMAL);
				if (result == 2) {
					w32g_syn_status = stop;
				}
#ifndef TWSYNSRV
				ChangeTasktrayIcon(w32g_syn.nid_hWnd);
#endif /* !TWSYNSRV */
			}
			break;
		case stop:
			{
			w32g_syn_message_t msg;
			if (w32g_message_get(&msg)) {
				if (msg.cmd == W32G_SYN_START) {
					w32g_syn_status = run;
					break;
				} else {
					if (msg.cmd == W32G_SYN_QUIT) {
						w32g_syn_status = quit;
						break;
					}
				}
			}
			Sleep(500);
			}
			break;
		}
		if (breakflag)
			break;
	}
#ifndef TWSYNSRV
	while (w32g_syn.quit_state < 1) {
		PostThreadMessage(w32g_syn.gui_dwThreadId, MYWM_QUIT, 0, 0);
		Sleep(300);
	}
#endif /* !TWSYNSRV */
	if (w32g_syn.quit_state < 2) w32g_syn.quit_state = 2;
	return 0;
}

int w32g_syn_do_before_pref_apply(void)
{
	w32g_syn_status_prev = none;
	for (;;) {
		if (w32g_syn_status == quit)
			return -1;
		if (!msg_loopbuf_hMutex) {
			msg_loopbuf_hMutex = CreateMutex(NULL, TRUE, NULL);
		} else {
			WaitForSingleObject(msg_loopbuf_hMutex, INFINITE);
		}
		if (w32g_syn_status_prev == none)
			w32g_syn_status_prev = w32g_syn_status;
		if (w32g_syn_status == stop) {
			return 0;
		}
		ReleaseMutex(msg_loopbuf_hMutex);
		w32g_message_set(W32G_SYN_STOP);
		Sleep(100);
	}
}

int w32g_syn_do_after_pref_apply(void)
{
	ReleaseMutex(msg_loopbuf_hMutex);
	if (w32g_syn_status_prev == run) {
		w32g_message_set(W32G_SYN_START);
		Sleep(100);
	}
	return 0;
}

///r
int w32g_syn_do_after_pref_save_restart(void)
{
	ReleaseMutex(msg_loopbuf_hMutex);
	w32g_message_set(W32G_SYN_QUIT);
	Sleep(100);
	return 0;
}

#ifdef HAVE_SYN_CONSOLE

// ****************************************************************************
// Edit Ctl.

static void VersionWnd(HWND hParentWnd)
{
	char VersionText[2024];
  sprintf(VersionText,
"TiMidity++ %s%s %s" NLS NLS
"TiMidity-0.2i by Tuukka Toivonen <tt@cgs.fi>." NLS
"TiMidity Win32 version by Davide Moretti <dave@rimini.com>." NLS
"TiMidity Windows 95 port by Nicolas Witczak." NLS
"Twsynth by Keishi Suenaga <s_keishi@mutt.freemail.ne.jp>." NLS
"Twsynth GUI by Daisuke Aoki <dai@y7.net>." NLS
" Japanese menu, dialog, etc by Saito <timidity@flashmail.com>." NLS
"TiMidity++ by Masanao Izumo <mo@goice.co.jp>." NLS
, (strcmp(timidity_version, "current")) ? "version " : "", timidity_version, arch_string);
	MessageBoxA(hParentWnd, VersionText, "Version", MB_OK);
}

static void TiMidityWnd(HWND hParentWnd)
{
	char TiMidityText[2024];
  sprintf(TiMidityText,
" TiMidity++ %s%s %s -- MIDI to WAVE converter and player" NLS
" Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>" NLS
" Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>" NLS
NLS
" Win32 version by Davide Moretti <dmoretti@iper.net>" NLS
" GUI by Daisuke Aoki <dai@y7.net>." NLS
" Modified by Masanao Izumo <mo@goice.co.jp>." NLS
NLS
" This program is free software; you can redistribute it and/or modify" NLS
" it under the terms of the GNU General Public License as published by" NLS
" the Free Software Foundation; either version 2 of the License, or" NLS
" (at your option) any later version." NLS
NLS
" This program is distributed in the hope that it will be useful, " NLS
" but WITHOUT ANY WARRANTY; without even the implied warranty of"NLS
" MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the" NLS
" GNU General Public License for more details." NLS
NLS
" You should have received a copy of the GNU General Public License" NLS
" along with this program; if not, write to the Free Software" NLS
" Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA" NLS
,
(strcmp(timidity_version, "current")) ? "version " : "", timidity_version, arch_string
	);
	MessageBoxA(hParentWnd, TiMidityText, "TiMidity", MB_OK);
}


// ***************************************************************************
//
// Console Window
//
// ***************************************************************************

// ---------------------------------------------------------------------------
// variables
static int ConsoleWndMaxSize = 64 * 1024;
static HFONT hFontConsoleWnd = NULL;

// ---------------------------------------------------------------------------
// prototypes of functions
static LRESULT CALLBACK ConsoleWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static void ConsoleWndAllUpdate(void);
static void ConsoleWndVerbosityUpdate(void);
static void ConsoleWndVerbosityApply(void);
static void ConsoleWndValidUpdate(void);
static void ConsoleWndValidApply(void);
static void ConsoleWndVerbosityApplySet(int num);
static int ConsoleWndInfoReset(HWND hwnd);
static int ConsoleWndInfoApply(void);

void ClearConsoleWnd(void);

// ---------------------------------------------------------------------------
// Global Functions

// Window Procedure
static LRESULT CALLBACK
ConsoleWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess) {
	case WM_INITDIALOG:
		PutsConsoleWnd("Console Window\n");
		ConsoleWndAllUpdate();
		return FALSE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCLOSE:
			ShowWindow(hwnd, SW_HIDE);
			break;
		case IDCLEAR:
			ClearConsoleWnd();
			break;
		case IDC_CHECKBOX_VALID:
			ConsoleWndValidApply();
			break;
		case IDC_BUTTON_VERBOSITY:
			ConsoleWndVerbosityApply();
			break;
		case IDC_BUTTON_INC:
			{
				int n = (int)GetDlgItemInt(hwnd, IDC_EDIT_VERBOSITY, NULL, TRUE);
				n++;
				ConsoleWndVerbosityApplySet(n);
			}
			break;
		case IDC_BUTTON_DEC:
			{
				int n = (int)GetDlgItemInt(hwnd, IDC_EDIT_VERBOSITY, NULL, TRUE);
				n--;
				ConsoleWndVerbosityApplySet(n);
			}
			break;
		default:
			break;
		}
		switch (HIWORD(wParam)) {
		case EN_ERRSPACE:
			ClearConsoleWnd();
			PutsConsoleWnd("### EN_ERRSPACE -> Clear! ###\n");
			break;
		default:
			break;
		}
		break;
	case WM_SIZE:
		ConsoleWndAllUpdate();
		switch(wParam){
		case SIZE_MAXIMIZED:
		case SIZE_RESTORED:
			{	// くそめんどーー
			int x,y,cx,cy;
			int max = 0;
			int width;
			RECT rcParent;
			RECT rcBUTTON_VERBOSITY, rcEDIT_VERBOSITY, rcBUTTON_DEC, rcBUTTON_INC, rcCHECKBOX_VALID, rcCLEAR, rcEDIT;
			HWND hwndBUTTON_VERBOSITY, hwndEDIT_VERBOSITY, hwndBUTTON_DEC, hwndBUTTON_INC, hwndCHECKBOX_VALID, hwndCLEAR, hwndEDIT;
			int nWidth = LOWORD(lParam);
			int nHeight = HIWORD(lParam);				
			hwndEDIT = GetDlgItem(hwnd,IDC_EDIT);
			hwndBUTTON_VERBOSITY = GetDlgItem(hwnd,IDC_BUTTON_VERBOSITY);
			hwndEDIT_VERBOSITY = GetDlgItem(hwnd,IDC_EDIT_VERBOSITY);
			hwndBUTTON_DEC = GetDlgItem(hwnd,IDC_BUTTON_DEC);
			hwndBUTTON_INC = GetDlgItem(hwnd,IDC_BUTTON_INC);
			hwndCHECKBOX_VALID = GetDlgItem(hwnd,IDC_CHECKBOX_VALID);
			hwndCLEAR = GetDlgItem(hwnd,IDCLEAR);
			GetWindowRect(hwndEDIT,&rcEDIT); // x0y0 基準
			GetWindowRect(hwndBUTTON_VERBOSITY,&rcBUTTON_VERBOSITY);
			GetWindowRect(hwndEDIT_VERBOSITY,&rcEDIT_VERBOSITY);
			GetWindowRect(hwndBUTTON_DEC,&rcBUTTON_DEC);
			GetWindowRect(hwndBUTTON_INC,&rcBUTTON_INC);
			GetWindowRect(hwndCHECKBOX_VALID,&rcCHECKBOX_VALID);
			GetWindowRect(hwndCLEAR,&rcCLEAR);					
			GetClientRect(hwnd,&rcParent);
			// IDC_BUTTON_VERBOSITY
			cx = rcBUTTON_VERBOSITY.right-rcBUTTON_VERBOSITY.left;
			cy = rcBUTTON_VERBOSITY.bottom-rcBUTTON_VERBOSITY.top;
			x = rcBUTTON_VERBOSITY.left - rcEDIT.left;
			y = rcParent.bottom - cy - 4;
			MoveWindow(hwndBUTTON_VERBOSITY,x,y,cx,cy,TRUE);
			if(cy>max) max = cy;
			// IDC_EDIT_VERBOSITY
			cx = rcEDIT_VERBOSITY.right-rcEDIT_VERBOSITY.left;
			cy = rcEDIT_VERBOSITY.bottom-rcEDIT_VERBOSITY.top;
			x = rcEDIT_VERBOSITY.left - rcEDIT.left;
			y = rcParent.bottom - cy - 4;
			MoveWindow(hwndEDIT_VERBOSITY,x,y,cx,cy,TRUE);
			if(cy>max) max = cy;
			// IDC_BUTTON_DEC
			cx = rcBUTTON_DEC.right-rcBUTTON_DEC.left;
			cy = rcBUTTON_DEC.bottom-rcBUTTON_DEC.top;
			x = rcBUTTON_DEC.left - rcEDIT.left;
			y = rcParent.bottom - cy - 4;
			MoveWindow(hwndBUTTON_DEC,x,y,cx,cy,TRUE);
			if(cy>max) max = cy;
			// IDC_BUTTON_INC
			cx = rcBUTTON_INC.right-rcBUTTON_INC.left;
			cy = rcBUTTON_INC.bottom-rcBUTTON_INC.top;
			x = rcBUTTON_INC.left - rcEDIT.left;
			y = rcParent.bottom - cy - 4;
			MoveWindow(hwndBUTTON_INC,x,y,cx,cy,TRUE);
			if(cy>max) max = cy;
			// IDC_CHECKBOX_VALID
			cx = rcCHECKBOX_VALID.right-rcCHECKBOX_VALID.left;
			cy = rcCHECKBOX_VALID.bottom-rcCHECKBOX_VALID.top;
			x = rcCHECKBOX_VALID.left - rcEDIT.left;
			y = rcParent.bottom - cy - 4;
			MoveWindow(hwndCHECKBOX_VALID,x,y,cx,cy,TRUE);
			if(cy>max) max = cy;
			// IDCLEAR
			cx = rcCLEAR.right-rcCLEAR.left;
			cy = rcCLEAR.bottom-rcCLEAR.top;
			x = rcCLEAR.left - rcEDIT.left;
			y = rcParent.bottom - cy - 4;
			MoveWindow(hwndCLEAR,x,y,cx,cy,TRUE);
			if(cy>max) max = cy;
			// IDC_EDIT
			cx = rcParent.right - rcParent.left;
			cy = rcParent.bottom - rcParent.top - max - 8;
			x  = rcParent.left;
			y = rcParent.top;
			MoveWindow(hwndEDIT,x,y,cx,cy,TRUE);
			// 
			InvalidateRect(hwnd,&rcParent,FALSE);
			UpdateWindow(hwnd);
			GetWindowRect(hwnd,&rcParent);
			break;
			}
		case SIZE_MINIMIZED:
		case SIZE_MAXHIDE:
		case SIZE_MAXSHOW:
		default:
			break;
		}
		return FALSE;
	case WM_MOVE:
		break;
	// See PreDispatchMessage() in w32g2_main.c
	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
	{
		int nVirtKey = (int)wParam;
		switch (nVirtKey) {
			case VK_ESCAPE:
				SendMessage(hwnd, WM_CLOSE, 0, 0);
				break;
		}
	}
		break;
	case WM_DESTROY:
		break;
	case WM_CLOSE:
		ShowWindow(hConsoleWnd, SW_HIDE);
		break;
	case WM_SETFOCUS:
		HideCaret(hwnd);
		break;
	case WM_KILLFOCUS:
		ShowCaret(hwnd);
		break;
	default:
		return FALSE;
	}
	return FALSE;
}

// ---------------------------------------------------------------------------
// Static Functions

static void ConsoleWndAllUpdate(void)
{
	ConsoleWndVerbosityUpdate();
	ConsoleWndValidUpdate();
	Edit_LimitText(GetDlgItem(hConsoleWnd, IDC_EDIT_VERBOSITY), 3);
	Edit_LimitText(GetDlgItem(hConsoleWnd, IDC_EDIT), ConsoleWndMaxSize);
}

static void ConsoleWndValidUpdate(void)
{
	if (ConsoleWndFlag)
		CheckDlgButton(hConsoleWnd, IDC_CHECKBOX_VALID, 1);
	else
		CheckDlgButton(hConsoleWnd, IDC_CHECKBOX_VALID, 0);
}

static void ConsoleWndValidApply(void)
{
	if (IsDlgButtonChecked(hConsoleWnd, IDC_CHECKBOX_VALID))
		ConsoleWndFlag = 1;
	else
		ConsoleWndFlag = 0;
}

static void ConsoleWndVerbosityUpdate(void)
{
	SetDlgItemInt(hConsoleWnd, IDC_EDIT_VERBOSITY, (UINT)ctl->verbosity, TRUE);
}

static void ConsoleWndVerbosityApply(void)
{
	char buffer[64];
	HWND hwnd;
	hwnd = GetDlgItem(hConsoleWnd, IDC_EDIT_VERBOSITY);
	if (!IsWindow(hConsoleWnd)) return;
	if (Edit_GetText(hwnd, buffer, 60) <= 0) return;
	ctl->verbosity = atoi(buffer);
	ConsoleWndVerbosityUpdate();
}

static void ConsoleWndVerbosityApplySet(int num)
{
	if (!IsWindow(hConsoleWnd)) return;
	ctl->verbosity = num;
	RANGE(ctl->verbosity, -1, 4);
	ConsoleWndVerbosityUpdate();
}

#endif /* HAVE_SYN_CONSOLE */

#ifdef IA_W32G_SYN
static int winplaymidi_sleep_level = 2;
static DWORD winplaymidi_active_start_time = 0;

void winplaymidi(void) {

    if (winplaymidi_sleep_level < 1) {
        winplaymidi_sleep_level = 1;
    }
    if (0 != rtsyn_buf_check()) {
        winplaymidi_sleep_level = 0;
    }
    rtsyn_play_some_data();
    if (winplaymidi_sleep_level == 1) {
        DWORD ct = GetCurrentTime();
        if (winplaymidi_active_start_time == 0 || ct < winplaymidi_active_start_time) {
            winplaymidi_active_start_time = ct;
        }
        else if (ct - winplaymidi_active_start_time > 60000) {
            winplaymidi_sleep_level = 2;
        }
    }
    else if (winplaymidi_sleep_level == 0) {
        winplaymidi_active_start_time = 0;
    }

    rtsyn_play_calculate();

    if (winplaymidi_sleep_level >= 2) {
        Sleep(100);
    }
    else if (winplaymidi_sleep_level > 0) {
        Sleep(1);
    }
}
#endif /* IA_W32G_SYN */


#ifdef HAVE_SYN_SOUNDSPEC

// ***************************************************************************
//
// Sound Spec Window
//
// ***************************************************************************

// ---------------------------------------------------------------------------
// variables

// ---------------------------------------------------------------------------
// prototypes of functions
LRESULT CALLBACK SoundSpecWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
#ifdef SUPPORT_SOUNDSPEC
extern void TargetSpectrogramCanvas(HWND hwnd);
extern void HandleSpecKeydownEvent(long message, short modifiers);
extern void UpdateSpectrogramCanvas(void);
#endif /* SUPPORT_SOUNDSPEC */

// ---------------------------------------------------------------------------
// Global Functions

// ---------------------------------------------------------------------------
// Static Functions

#endif /* HAVE_SYN_SOUNDSPEC */

#endif /* IA_W32G_SYN */
