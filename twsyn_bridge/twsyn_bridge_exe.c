


#include <stdlib.h>
#include <io.h>
#include <windows.h>
#include <process.h>
#include <windows.h>
#include <windowsx.h>
#include <winuser.h>
#include <windef.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <tchar.h>
#include <mmsystem.h>

#include "twsyn_bridge_common.h"
#include "twsyn_bridge_exe_res.h"

#define FILEPATH_MAX 32000

#ifdef _WIN64
const char WinTitle[] = "twsyn bridge x64";
#else
const char WinTitle[] = "twsyn bridge x86";
#endif

// host
static DWORD PrcsIdHost = 0;
static DWORD PrcsVerHost = 0;
static HWND hControlWndHost = NULL;
static UINT uControlMessHost = 0;
// bridge
static HWND hControlWnd = NULL;
static UINT uControlMess = 0;
static HANDLE hMutex = NULL;
static HANDLE hFileMap = NULL;
static fm_bridge_t *shared_data = NULL;
static int thread_exit = 0;
static HANDLE hThread = NULL;
// midi in
HMIDIIN hMidiIn[BRIDGE_MAX_PORT];
MIDIHDR *IMidiHdr[BRIDGE_MAX_PORT][BRIDGE_MAX_EXBUF];
char sIMidiHdr[BRIDGE_MAX_PORT][BRIDGE_MAX_EXBUF][sizeof(MIDIHDR)];
char sImidiHdr_data[BRIDGE_MAX_PORT][BRIDGE_MAX_EXBUF][BRIDGE_BUFF_SIZE];
static int exbuf_count = 0;

static void ErrorMessageBox(const char *text)
{
	const char title[] = "twsyn_bridge(exe)";
	char buf[0x800];

	wsprintfA(buf,"%s", text);
	MessageBox(NULL, buf, title, MB_OK | MB_ICONEXCLAMATION);
}

static void uninit_bridge(void)
{
	thread_exit = 1;	
	if(hThread){
		switch(WaitForSingleObject(hThread, 500)) {
		case WAIT_OBJECT_0:
			break;
		default:
			TerminateThread(hThread, 0);
			break;
		}
		CloseHandle(hThread);
		hThread = NULL;
	}
	if(shared_data) {		
		UnmapViewOfFile(shared_data);
		shared_data = NULL;
	}
	if(hFileMap) {
		CloseHandle(hFileMap);
		hFileMap = NULL;
	}	
	if(hMutex){
		CloseHandle(hMutex);
		hMutex = NULL;
	}
}

static void get_midi_devs(void)
{
	int i, max;

	if(shared_data == NULL)
		return;	
	memset(shared_data->midi_devs, 0, sizeof(shared_data->midi_devs));	
	max = midiInGetNumDevs();
	if(max > 32)
		max = 32;
	shared_data->midi_dev_num = max;
#ifdef _WIN64
	_snprintf(shared_data->midi_devs[0], 255, "MIDI Mapper (x64)");
#else
	_snprintf(shared_data->midi_devs[0], 255, "MIDI Mapper (x86)");
#endif
	for(i = 1; i <= max; i++){
		MIDIINCAPS mic;
		if(midiInGetDevCaps(i - 1, &mic, sizeof(MIDIINCAPS)) == 0){
#ifdef _WIN64
			_snprintf(shared_data->midi_devs[i], 255, "%s (x64)", mic.szPname);
#else
			_snprintf(shared_data->midi_devs[i], 255, "%s (x86)", mic.szPname);
#endif
		} else {
#ifdef _WIN64
			_snprintf(shared_data->midi_devs[i], 255, "MIDI IN dev:%d (x64)", i - 1);
#else
			_snprintf(shared_data->midi_devs[i], 255, "MIDI IN dev:%d (x86)", i - 1);
#endif
		}
	}
}

void CALLBACK MidiInProc(HMIDIIN hMidiIn_, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{	
	MIDIHDR *IIMidiHdr;
	unsigned int port, bytes;
	const int total_exbuf = BRIDGE_TOTAL_EXBUF;
	
	if(shared_data == NULL)
		return;	
	switch(wMsg){
	case MIM_DATA:		
		shared_data->wMsg = wMsg;
		shared_data->dwInstance = dwInstance;
		shared_data->dwParam1 = dwParam1;
		shared_data->dwParam2 = dwParam2;
		SendMessage(hControlWndHost, uControlMessHost, WMC_MIM_DATA, (LPARAM)NULL);
		break;
	case MIM_LONGDATA:
		IIMidiHdr = (MIDIHDR *) dwParam1;		
		shared_data->wMsg = wMsg;
		shared_data->dwInstance = dwInstance;
		shared_data->dwParam1 = exbuf_count;
		shared_data->dwParam2 = dwParam2;
		bytes = (UINT)IIMidiHdr->dwBytesRecorded;	
		shared_data->dwBytes[exbuf_count] = bytes;
		memcpy(shared_data->lpData[exbuf_count], IIMidiHdr->lpData, bytes);
		if(++exbuf_count >= total_exbuf)
			exbuf_count -= total_exbuf;
		port = (UINT)dwInstance;
		midiInUnprepareHeader(hMidiIn[port], IIMidiHdr, sizeof(MIDIHDR));
		midiInPrepareHeader(hMidiIn[port], IIMidiHdr, sizeof(MIDIHDR));
		midiInAddBuffer(hMidiIn[port], IIMidiHdr, sizeof(MIDIHDR));
		SendMessage(hControlWndHost, uControlMessHost, WMC_MIM_LONGDATA, (LPARAM)NULL);
		break;
	case MIM_OPEN:
	case MIM_CLOSE:
	case MIM_LONGERROR:
	case MIM_ERROR:
	case MIM_MOREDATA:
		break;
	}
}

static void close_midi_devs(void)
{	
	int i;
	
	for(i = 0; i < shared_data->portnumber; i++){
		midiInStop(hMidiIn[i]);
		midiInReset(hMidiIn[i]);
		midiInClose(hMidiIn[i]);
	}
	shared_data->open_midi_dev = 0;
}

static void open_midi_devs(void)
{
	int i, port;
	
	if(shared_data == NULL)
		return;	
	exbuf_count = 0;
	shared_data->wMsg;
	shared_data->dwInstance;
	shared_data->dwParam1;
	shared_data->dwParam2;
	memset(shared_data->dwBytes, 0, sizeof(shared_data->dwBytes));
	memset(shared_data->lpData, 0, sizeof(shared_data->lpData));
	for(port = 0; port < shared_data->portnumber; port++){
		for(i = 0; i < BRIDGE_MAX_EXBUF; i++){
			IMidiHdr[port][i] = (MIDIHDR *)sIMidiHdr[port][i];
			memset(IMidiHdr[port][i], 0, sizeof(MIDIHDR));
			IMidiHdr[port][i]->lpData = sImidiHdr_data[port][i];
			memset(IMidiHdr[port][i]->lpData, 0, BRIDGE_BUFF_SIZE);
			IMidiHdr[port][i]->dwBufferLength = BRIDGE_BUFF_SIZE;
		}
	}
	for(port = 0; port < shared_data->portnumber; port++){
		midiInOpen(&hMidiIn[port], shared_data->portID[port], (DWORD_PTR)MidiInProc, (DWORD_PTR)port, CALLBACK_FUNCTION);
		for (i = 0; i < BRIDGE_MAX_EXBUF; i++){
			midiInUnprepareHeader(hMidiIn[port], IMidiHdr[port][i], sizeof(MIDIHDR));
			midiInPrepareHeader(hMidiIn[port], IMidiHdr[port][i], sizeof(MIDIHDR));
			midiInAddBuffer(hMidiIn[port], IMidiHdr[port][i], sizeof(MIDIHDR));
		}
	}
	for(port = 0; port < shared_data->portnumber; port++){
		if(midiInStart(hMidiIn[port]) != MMSYSERR_NOERROR)
			goto winmmerror;
	}
	shared_data->open_midi_dev = 1;
	return;
winmmerror:
	close_midi_devs();
	return;
}

LRESULT APIENTRY CALLBACK CtrlWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	if(uMess == uControlMess){
		switch(wParam){
		case WMC_CLOSE_BRIDGE:	
			uninit_bridge();
			EndDialog(hwnd, FALSE);
			PostQuitMessage(0);
			return TRUE;
		case WMC_GET_MIDI_DEVS:	
			get_midi_devs();
			return TRUE;
		case WMC_OPEN_MIDI_DEVS:
			open_midi_devs();
			return TRUE;
		case WMC_CLOSE_MIDI_DEVS:
			close_midi_devs();
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

static unsigned int WINAPI CheckProcessThread(void *args)
{	
	for(;;){
		DWORD ver = 0;
		Sleep(500);
		if(thread_exit) break;
		if(shared_data->exit){
		//	ErrorMessageBox("recieve exit process.");
			PostMessage(hControlWnd, uControlMess, WMC_CLOSE_BRIDGE, (LPARAM)NULL);
			break;	
		}
		else if(GetProcessVersion(PrcsIdHost) != PrcsVerHost){
		//	ErrorMessageBox("lost host process.");
			PostMessage(hControlWnd, uControlMess, WMC_CLOSE_BRIDGE, (LPARAM)NULL);
			break;	
		}
	}
	_endthread();
	return 0;
}

// Create Window & init bridge
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	char buffer[FILEPATH_MAX] = {0};
	char *errortext;
	HICON hIcon;
	unsigned int dwThreadID = 0;
	
	// resister ctrl message
	uControlMess = RegisterWindowMessage("twsyn_bridge_exe");
	if(!uControlMess){
		errortext = "bridge.exe error : RegisterWindowMessage.";
		goto error;	
	}
	// create window
	hControlWnd = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DIALOG_DUMMY), NULL, (DLGPROC)CtrlWndProc);
	if(!hControlWnd){
		errortext = "bridge.exe error : CreateDialog.";
		goto error;	
	}	
	// create mutex
	hMutex = CreateMutex(NULL, TRUE, COMMON_MUTEX_NAME);
	if(!hMutex){
		errortext = "bridge.exe error : CreateMutex.";
		goto error;	
	}	
	if(GetLastError() == ERROR_ALREADY_EXISTS){ // 二重起動防止 この時点で古いブリッジは終了しているかも？
		errortext = "bridge.exe error : CreateMutex ALREADY_EXISTS.";
		goto error;
	}
	// set icon
	hIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON_BRIDGE16), IMAGE_ICON, 16, 16, 0);
	if(hIcon != NULL) 
		SendMessage(hControlWnd, WM_SETICON, FALSE, (LPARAM)hIcon);
	ShowWindow(hControlWnd, SW_HIDE);
//	ShowWindow(hControlWnd, SW_SHOW);
	// open file mapping
	hFileMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, TRUE, COMMON_FM_NAME);
	if(!hFileMap){
		errortext = "bridge.exe error : OpenFileMapping.";
		goto error;	
	}	
	// open shared_data
	shared_data = (fm_bridge_t *)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if(!shared_data){
		errortext = "bridge.exe error : MapViewOfFile.";
		goto error;	
	}
	// open shared_data
	if(shared_data->PrcsIdHost == 0){
		errortext = "bridge.exe error : recieve processID.";
		goto error;	
	}	
	// 
	PrcsIdHost = shared_data->PrcsIdHost;
	PrcsVerHost = shared_data->PrcsVerHost;
	hControlWndHost = (HWND)shared_data->hControlWndHost;
	uControlMessHost = shared_data->uControlMessHost;
	// create thread
	shared_data->exit = 0; // 解除 (ロードで時間かかるのでSleepは不要
	thread_exit = 0;
	hThread = (HANDLE)_beginthreadex(NULL, 0, CheckProcessThread, (void *)0, 0, &dwThreadID);
	if(!hThread){
		errortext = "bridge.exe error : CreateThread.";
		goto error;	
	}	
	// send processID	
	shared_data->PrcsId = GetCurrentProcessId();
	// send process Version	
	shared_data->PrcsVer = GetProcessVersion(shared_data->PrcsId);
	// send ctrl message
	shared_data->uControlMess = uControlMess;
	// send win handle	
	shared_data->hControlWnd = (unsigned long long)hControlWnd; // winhandleをhost側へ渡す 初期化完了フラグ
	return TRUE;
error:
	uninit_bridge();
	ErrorMessageBox(errortext);
	return FALSE;
}

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	static int init = 0;
	MSG msg;
	
	if(init++) return FALSE;	
	if (!InitInstance(hInstance, nCmdShow)){
		return FALSE;
	}
 //   PeekMessage(&msg, NULL, WM_COMMAND, WM_COMMAND, PM_NOREMOVE);
	while( GetMessage(&msg,NULL,0,0) ){ // message loop
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	uninit_bridge();
	return (int) msg.wParam;
}


