

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef USE_TWSYN_BRIDGE

#define _DLLC	
#define STRICT
#include <stdlib.h>
#include <io.h>
#include <windows.h>
#include <process.h>
#include <windows.h>
#include <winuser.h>
#include <windef.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <tchar.h>
#pragma comment(lib, "shlwapi.lib")
#include <shlwapi.h>
#include <mmsystem.h>
#include "timidity.h"
#include "common.h"
#include "w32g_res.h"
#include "twsyn_bridge_common.h"
#include "twsyn_bridge_host.h"
#pragma hdrstop

int opt_use_twsyn_bridge = 0;

// bridge
static DWORD PrcsId = 0;
static DWORD PrcsVer = 0;
static HWND hControlWnd = NULL;
static UINT uControlMess = 0;
// host
static HWND hControlWndHost = NULL;
static UINT uControlMessHost = 0;
static TCHAR ExePath[FILEPATH_MAX + 4] = _T("");
static HANDLE hFileMap = NULL;
static fm_bridge_t *shared_data = NULL;
static PROCESS_INFORMATION pi;
static int run_bridge = 0;
static int error_bridge = 0;

static void ErrorMessageBox(const char *text,DWORD errorcode)
{
	const TCHAR title[] = _T("twsyn_bridge(host)");
	char buf[0x800];

	wsprintfA(buf,"%s (code:%d)", text, errorcode);
	TCHAR *t = char_to_tchar(buf);
	MessageBox(NULL, t, title, MB_OK | MB_ICONEXCLAMATION);
	safe_free(t);
}

static void uninit_bridge(void)
{
	if(hControlWndHost){
		EndDialog(hControlWndHost, FALSE);
		hControlWndHost = NULL;
	}
	if(shared_data != NULL) {		
		UnmapViewOfFile(shared_data);
		shared_data = NULL;
	}
	if(hFileMap != NULL) {
		CloseHandle(hFileMap);
		hFileMap = NULL;
	}
	if(pi.hProcess){
		WaitForInputIdle(pi.hProcess, 100); 
		TerminateProcess(pi.hProcess, 0); 
		CloseHandle(pi.hProcess);
	}
	run_bridge = 0;
	error_bridge = 0;
}

static int check_bridge(void)
{
	if(!run_bridge)
		return 1;
	if(GetProcessVersion(PrcsId) == PrcsVer)
		return 0;
	ErrorMessageBox("bridge host error : lost twsyn bridge process.", 0);
	uninit_bridge();
	return 1;
}

LRESULT APIENTRY CALLBACK CtrlWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	if(!run_bridge)
		return FALSE;
	if(uMess == uControlMessHost){
		switch(wParam){
		case WMC_MIM_DATA:	
		case WMC_MIM_LONGDATA:	
			MidiInProc(NULL, shared_data->wMsg, shared_data->dwInstance, shared_data->dwParam1, shared_data->dwParam2);
			return TRUE;
		}
	}else switch (uMess){
	case WM_COMMAND:
		switch(LOWORD(wParam)){
		case IDOK:	
			return TRUE;
		case IDCANCEL:	
			return TRUE;
		}
		break;
	case WM_INITDIALOG:
		return TRUE;
	default:
	  break;
	}
	return FALSE;
}

int get_bridge_midi_devs(void)
{
	if(check_bridge())
		return 0;
	SendMessage(hControlWnd, uControlMess, WMC_GET_MIDI_DEVS, (LPARAM)NULL);
	return shared_data->midi_dev_num;
}

char *get_bridge_midi_dev_name(int num)
{	
	if(!run_bridge)
		return NULL;
	if(num < 0)
		num = 0;
	if(num > shared_data->midi_dev_num)
		num = shared_data->midi_dev_num;
	return shared_data->midi_devs[num];
}

int get_bridge_mim_databytes(int num)
{
	if(!run_bridge)
		return 0;
	return shared_data->dwBytes[num];
}

char *get_bridge_mim_longdata(int num)
{
	if(!run_bridge)
		return NULL;
	return shared_data->lpData[num];
}

void open_bridge_midi_dev(int portnumber, unsigned int *portID)
{
	int i;
	if(check_bridge())
		return;
	shared_data->portnumber = portnumber;
	for(i = 0; i < BRIDGE_MAX_PORT; i++)
		shared_data->portID[i] = portID[i];
	SendMessage(hControlWnd, uControlMess, WMC_OPEN_MIDI_DEVS, (LPARAM)NULL);
}

void close_bridge_midi_dev(void)
{
	int i;
	if(!run_bridge)
		return;
	SendMessage(hControlWnd, uControlMess, WMC_CLOSE_MIDI_DEVS, (LPARAM)NULL);
}

void close_bridge(void)
{
	if(!run_bridge)
		return;		
	SendMessage(hControlWnd, uControlMess, WMC_CLOSE_BRIDGE, (LPARAM)NULL); // close_bridge
	uninit_bridge();
}

void init_bridge(void)
{	
	STARTUPINFO si;
	WNDCLASSEX wc;
	HINSTANCE hInstance = NULL;
	HANDLE hfile = NULL;
	char *errortext;
	int result, count, error = 0;
	
	if(run_bridge)
		return;
	if(error_bridge)
		return;	
	// get instance
	hInstance = GetModuleHandle(0);
	// bridge exe path
    if(GetModuleFileName(hInstance, ExePath, FILEPATH_MAX - 1)){
		PathRemoveFileSpec(ExePath);
		_tcscat(ExePath,_T("\\"));
	}else{
		ExePath[0] = _T('.');
		ExePath[1] = _T('\\');
		ExePath[2] = _T('\0');
    }
	_tcscat(ExePath, _T(EXE_NAME));
	// check bridge exe
	hfile = CreateFile(ExePath, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);	
	if(hfile == INVALID_HANDLE_VALUE){
	//	errortext = "host error : bridge.exe not exist.";
	//	goto error;
		error_bridge = 1;
		return;
	}
	CloseHandle(hfile);
	hfile = NULL;
	// CreateFileMapping
	hFileMap = CreateFileMapping(FILE_HANDLE, NULL, PAGE_READWRITE, 0, sizeof(fm_bridge_t), _T(COMMON_FM_NAME));
	if(hFileMap == NULL){
		errortext = "bridge host error : CreateFileMapping.";
		goto error;
	}
	if(GetLastError() == ERROR_ALREADY_EXISTS){ // 二重起動防止
		errortext = "bridge host error : CreateFileMapping ALREADY_EXISTS.";
		goto error;
	}
	// open FileMap
	shared_data = (fm_bridge_t *)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if(shared_data == NULL){
		errortext = "bridge host error : MapViewOfFile.";
		goto error;
	}
	memset(shared_data, 0, sizeof(shared_data));
	// send exsit flag
	shared_data->exit = 1; // 二重起動防止 (古いブリッジを終了 ブリッジ側でスレッド起動前に解除
	// send processID	
	shared_data->PrcsIdHost = GetCurrentProcessId();
	// send process Version	
	shared_data->PrcsVerHost = GetProcessVersion(shared_data->PrcsIdHost);
	// resister ctrl message
	uControlMessHost = RegisterWindowMessage(_T("twsyn_bridge_host"));
	if(!uControlMessHost){
		errortext = "bridge host error : RegisterWindowMessage.";
		goto error;	
	}
	// send ctrl message	
	shared_data->uControlMessHost = uControlMessHost;
	// create window
	hControlWndHost = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DIALOG_TWSYN_BRIDGE), NULL, (DLGPROC)CtrlWndProc);
	if(!hControlWndHost){
		errortext = "bridge host error : CreateDialog.";
		goto error;	
	}	
	ShowWindow(hControlWndHost, SW_HIDE);
//	ShowWindow(hControlWndHost, SW_SHOW);
	// send win handle	
	shared_data->hControlWndHost = hControlWndHost;
	// run bridge
	si.cb			= sizeof(si);
	si.lpReserved	= NULL;
	si.lpDesktop	= NULL;
	si.lpTitle		= NULL;
	si.dwFlags		= STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	si.cbReserved2	= 0;
	si.lpReserved2	= NULL;
	si.wShowWindow	= SW_HIDE;
	si.hStdError	= GetStdHandle(STD_ERROR_HANDLE);
	if(CreateProcess(ExePath,NULL,NULL,NULL,TRUE,
		CREATE_DEFAULT_ERROR_MODE,NULL,NULL,&si,&pi) == FALSE ){
		errortext = "host error : run bridge.exe.";
		goto error;
	}
	// recieve bridge hControlWnd
	count = 0;
	while(!shared_data->hControlWnd){
		Sleep(100);
		if((count++) > 50){ // wait max 5sec
			errortext = "host error : hControlWnd timeout.";
			goto error;
		}
	}
	uControlMess = (UINT)shared_data->uControlMess;
	hControlWnd = (HWND)shared_data->hControlWnd;
	// recieve processID
	PrcsId = shared_data->PrcsId;
	// recieve  process Version	
	PrcsVer = shared_data->PrcsVer;
	run_bridge = 1;
	return;
error:
	result = GetLastError();
	CloseWindow(hControlWndHost);
	if(shared_data != NULL) {		
		UnmapViewOfFile(shared_data);
		shared_data = NULL;
	}
	if(hFileMap != NULL) {
		CloseHandle(hFileMap);
		hFileMap = NULL;
	}
	if(pi.hProcess){
		WaitForInputIdle(pi.hProcess, 100); 
		TerminateProcess(pi.hProcess, 0); 
		CloseHandle(pi.hProcess);
	}
	ErrorMessageBox(errortext,result);
	run_bridge = 0;
	error_bridge = 1;
	return;
}

#endif // USE_BRIDGE