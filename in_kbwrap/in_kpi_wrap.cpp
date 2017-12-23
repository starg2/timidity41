

/*
Winamp input plug-in
KbMedia Player Plugin wrapper

based
Example Winamp .RAW input plug-in
*/

#include <windows.h>
#include <stdio.h>


#include "../Winamp/in2.h"

#include "kmp_pi.h"
#include "kbtim_common.h"
#include "kbstr.h"


#ifdef _MSC_VER
#define HAVE_SNPRINTF
#define snprintf _snprintf
#endif


// avoid CRT. Evil. Big. Bloated. Only uncomment this code if you are using 
// 'ignore default libraries' in VC++. Keeps DLL size way down.
// /*
BOOL WINAPI _DllMainCRTStartup(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
	return TRUE;
}
// */

// post this to the main window at end of file (after playback as stopped)
#define WM_WA_MPEG_EOF WM_USER+2
#define FILEPATH_MAX 32000
#define EXT_INFO_MAX 32000

// def configuration.
#define NCH 2
#define SAMPLERATE 44100
#define BPS 16


char lastfn[MAX_PATH];	// currently playing file (used for getting info on the current file)
int file_length;		// file length, in bytes
int decode_pos_ms;		// current decoding position, in milliseconds. 
						// Used for correcting DSP plug-in pitch changes
int paused;				// are we paused?
volatile int seek_needed; // if != -1, it is the point that the decode 
						  // thread should seek to, in ms.
HANDLE input_file=INVALID_HANDLE_VALUE; // input file handle
volatile int killDecodeThread=0;			// the kill switch for the decode thread
HANDLE thread_handle=INVALID_HANDLE_VALUE;	// the handle to the decode thread
DWORD WINAPI DecodeThread(LPVOID b); // the decode thread procedure

#define RENDER_SAMPLES 576 // winamp plugin default
#define MAX_RENDER_SAMPLES 4096
static char sample_buffer[MAX_RENDER_SAMPLES * 2 * (32 / 8) * 2]; // sample buffer. twice as // big as the blocksize
int first_render_play;
int first_render_byte;
int play_load = 0;
int kpi_wrapper = 0; // 0:in_tim 1: in_kpi

static HINSTANCE g_hDll1;    //dll のインスタンスハンドル
static HINSTANCE g_hDll2;    //kpi のインスタンスハンドル
static KMPMODULE *g_pModule;//kpi が返した KMPMODULE
static HKMP hkmp = NULL;
static char szDll1[FILEPATH_MAX];
static char szIni1[FILEPATH_MAX];
static char szDll2[FILEPATH_MAX];
static char szDll2File[FILEPATH_MAX];
static char szDll2Ext[EXT_INFO_MAX];
static char szDll2Dsc[EXT_INFO_MAX];

static SOUNDINFO InfoKPI =
{	// init param
    SAMPLERATE, // DWORD dwSamplesPerSec サンプリング周波数(44100, 22050 など)
    NCH,        // DWORD dwChannels チャンネル数( mono = 1, stereo = 2)
    BPS,        // DWORD dwBitsPerSample 量子化ビット数( 8 or 16)
    0xFFFFFFFF, // DWORD dwLength 曲の長さ（SPC のように計算不可能な場合は 0xFFFFFFFF）
    1,          // DWORD dwSeekable シークをサポートしている場合は 1、しない場合は 0
    0,          // DWORD dwUnitRender Render関数の第３引数はこの値の倍数が渡される（どんな値でも良い場合は 0）
    0,          // DWORD dwReserved1 常に 0
    0,          // DWORD dwReserved2 常に 0
};

// module definition.
void config(HWND hwndParent);
void about(HWND hwndParent);
void init(void);
void quit(void);
void getfileinfo(const char *filename, char *title, int *length_in_ms);
int infoDlg(const char *fn, HWND hwnd);
int isourfile(const char *fn);
int play(const char *fn);
void pause(void);
void unpause(void);
int ispaused(void);
void stop(void);
int getlength(void);
int getoutputtime(void);
void setoutputtime(int time_in_ms);
void setvolume(int volume);
void setpan(int pan);
void eq_set(int on, char data[10], int preamp);

In_Module mod = 
{
	IN_VER,	// defined in IN2.H
	"KbMedia Player Plugin wrapper"
	// winamp runs on both alpha systems and x86 ones. :)
#ifdef __alpha
	"(AXP)"
#else
	"(x86)"
#endif
	,
	0,	// hMainWindow (filled in by winamp)
	0,  // hDllInstance (filled in by winamp)
	0, // this is a double-null limited list. "EXT\0Description\0EXT\0Description\0" etc.
	1,	// is_seekable
	1,	// uses output plug-in system
	config,
	about,
	init,
	quit,
	getfileinfo,
	infoDlg,
	isourfile,
	play,
	pause,
	unpause,
	ispaused,
	stop,	
	getlength,
	getoutputtime,
	setoutputtime,
	setvolume,
	setpan,
	0,0,0,0,0,0,0,0,0, // visualization calls filled in by winamp
	0,0, // dsp calls filled in by winamp
	eq_set,
	NULL,		// setinfo call filled in by winamp
	0 // out_mod filled in by winamp

};

static void load_wrapper_config(void)
{
	int i;
	int end = 0;
    static KMPMODULE Module;
    static KMPMODULE *pModule = NULL;
			
	GetModuleFileName(g_hDll1, szDll1, sizeof(szDll1));		
	kbExtractReplacedFileExt(szIni1, szDll1, ".ini");// ini取得 プラグインとファイル名が同じで拡張子が INI	
	kpi_wrapper = GetPrivateProfileInt("wrapper", "kpi_wrapper", 0, szIni1);	

	play_load = GetPrivateProfileInt("wrapper", "play_load", 0, szIni1);	
	InfoKPI.dwSamplesPerSec = GetPrivateProfileInt("wrapper", "output_rate", 0, szIni1);
	InfoKPI.dwBitsPerSample = GetPrivateProfileInt("wrapper", "output_bit", 0, szIni1);
	InfoKPI.dwChannels = GetPrivateProfileInt("wrapper", "output_channel", 0, szIni1);		
	InfoKPI.dwUnitRender = RENDER_SAMPLES * InfoKPI.dwChannels * InfoKPI.dwBitsPerSample / 8;
	GetPrivateProfileString("wrapper", "file", "", szDll2File, sizeof(szDll2File), szIni1); // kpiファイル名取得
	if(kbStrLen(szDll2File)){ // kpi取得 ファイル名指定
		kbExtractFilePath(szDll2, szDll1, sizeof(szDll2));
		kbStrLCat(szDll2, szDll2File, sizeof(szDll2));
	}else{// kpi取得 プラグインファイル名から"in_"削除 拡張子が KPI
		const char *file = kbExtractFileName(szDll1) + 3; // delete "in_"
		kbExtractFilePath(szDll2File, szDll1, sizeof(szDll2));
		kbStrLCat(szDll2File, file, sizeof(szDll2));
		kbExtractReplacedFileExt(szDll2, szDll2File, ".kpi");// 拡張子が KPI
	}
	// load kpi
	HINSTANCE hKpi = LoadLibrary(szDll2);
	if(!hKpi){
		return;
	}
	pfnGetKMPModule GetModule = (pfnGetKMPModule)GetProcAddress(hKpi, SZ_KMP_GETMODULE);
	if(!GetModule){
		FreeLibrary(hKpi);
		return;
	}
	pModule = GetModule();
	if(!pModule){
		FreeLibrary(hKpi);
		return;
	}		
	memcpy(&Module, pModule, sizeof(KMPMODULE));		
//    if(pModule->dwReentrant != 0xFFFFFFFF)
	g_pModule = pModule;
	pModule = &Module;
	g_hDll2 = hKpi;  
	
	// Description
	kbStrLCat(szDll2Dsc, g_pModule->pszDescription, EXT_INFO_MAX);
#ifdef __alpha
	kbStrLCat(szDll2Dsc, " (AXP)", EXT_INFO_MAX);
#else
	kbStrLCat(szDll2Dsc, " (x86)", EXT_INFO_MAX);
#endif
	mod.description = szDll2Dsc;
	// Extension Info
	for(i = 0; i < 16; i++){
		int tmp1, tmp2;
		char szext[EXT_INFO_MAX], szinf[EXT_INFO_MAX];
		char *str = (char *)g_pModule->ppszSupportExts[i];

		if(str == NULL)
			break;
		memset(szext, 0, sizeof(szext));
		memset(szinf, 0, sizeof(szinf));
		tmp1 = kbStrLen(str);
		--tmp1; 
		if(tmp1 <= 0)
			break;
		memcpy(szext, (char *)str + 1, tmp1); // delete piriod
		snprintf(szinf, sizeof(szinf), "%s file (*.%s)", szext, szext);	
		tmp1 = kbStrLen(szext);
		tmp2 = kbStrLen(szinf);
		if(tmp1 && tmp2){
			memcpy(szDll2Ext + end, szext, tmp1);
			end += tmp1;
			szDll2Ext[end] = '\0';
			end++;
			memcpy(szDll2Ext + end, szinf, tmp2);
			end += tmp2;
			szDll2Ext[end] = '\0';
			end++;
		}	
	}
	mod.FileExtensions = szDll2Ext;	
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call){
        case DLL_PROCESS_ATTACH:{
            g_hDll1 = (HINSTANCE)hModule;
            DisableThreadLibraryCalls((HMODULE)hModule);
            kbStr_Initialize();
            break;
        }
        case DLL_PROCESS_DETACH:{
            break;
        }
    }
    return TRUE;
}

typedef DWORD (WINAPI *pfnGetKMPConfig)(HWND hWnd, DWORD dwVersion, DWORD dwReserved);
void config(HWND hwndParent)
{
    if(!g_hDll2)
        return;
	pfnGetKMPConfig Config = (pfnGetKMPConfig)GetProcAddress(g_hDll2, "kmp_Config");
	if(Config)
		Config(hwndParent, 0, 0);
	else
		MessageBox(hwndParent, "No configuration.", "Configuration",MB_OK);
	// if we had a configuration box we'd want to write it here (using DialogBox, etc)
}

void about(HWND hwndParent)
{
	MessageBox(hwndParent,"KbMedia Player Plugin wrapper", "About Plugin",MB_OK);
}

void init(void) { 
	if(g_pModule){
        if(g_pModule->Init)
            g_pModule->Init();
    }	
}

void quit(void) { 
    if(g_pModule){
        if(g_pModule->Deinit){
            g_pModule->Deinit();
        }
        g_pModule = NULL;
    }
    if(g_hDll2){
        FreeLibrary(g_hDll2);
        g_hDll2 = NULL;
    }
}

int isourfile(const char *fn) { 
// used for detecting URL streams.. unused here. 
// return !strncmp(fn,"http://",7); to detect HTTP streams, etc
	return 0; 
} 

// called when winamp wants to play a file
int play(const char *fn) 
{ 	
	int maxlatency;
	DWORD thread_id;
	
	paused=0;
	decode_pos_ms=0;
	seek_needed=-1;	
	first_render_play = 0;
	first_render_byte = 0;
	memset(sample_buffer, 0, sizeof(sample_buffer));
	
    if(g_pModule){
        if(g_pModule->Open){
            hkmp = g_pModule->Open(fn, &InfoKPI);
        }
    }
	if(!hkmp)
		return 1;
	mod.is_seekable = InfoKPI.dwSeekable;

	// -1 and -1 are to specify buffer and prebuffer lengths.
	// -1 means to use the default, which all input plug-ins should
	// really do.
	maxlatency = mod.outMod->Open(InfoKPI.dwSamplesPerSec, InfoKPI.dwChannels, InfoKPI.dwBitsPerSample, -1, -1); 

	// maxlatency is the maxium latency between a outMod->Write() call and
	// when you hear those samples. In ms. Used primarily by the visualization
	// system.
	if (maxlatency < 0) // error opening device
		return 1;

	// dividing by 1000 for the first parameter of setinfo makes it
	// display 'H'... for hundred.. i.e. 14H Kbps.
	mod.SetInfo((InfoKPI.dwSamplesPerSec * InfoKPI.dwBitsPerSample * InfoKPI.dwChannels) / 1000,		
		InfoKPI.dwSamplesPerSec / 1000, InfoKPI.dwChannels >= 2 ? 1 : 0, 1);

	// initialize visualization stuff
	mod.SAVSAInit(maxlatency, InfoKPI.dwSamplesPerSec);
	mod.VSASetInfo(InfoKPI.dwSamplesPerSec, InfoKPI.dwChannels);

	// set the output plug-ins default volume.
	// volume is 0-255, -666 is a token for
	// current volume.
	mod.outMod->SetVolume(-666); 		 
	
	// for aimp3 timeout
	if(play_load){
		if(g_pModule){
			if(g_pModule->Render){
				first_render_byte = g_pModule->Render(hkmp, (BYTE*)sample_buffer, InfoKPI.dwUnitRender); // retrieve samples
			}
		}
	}else{
		first_render_play = 1;
	}

	// launch decode thread
	killDecodeThread=0;
	thread_handle = (HANDLE)CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) DecodeThread, NULL, 0, &thread_id);

	return 0; 
}

// standard pause implementation
void pause() { paused=1; mod.outMod->Pause(1); }
void unpause() { paused=0; mod.outMod->Pause(0); }
int ispaused() { return paused; }


// stop playing.
void stop() { 
	if (thread_handle != INVALID_HANDLE_VALUE)
	{
		killDecodeThread=1;
		if (WaitForSingleObject(thread_handle,10000) == WAIT_TIMEOUT)
		{
			MessageBox(mod.hMainWindow,"error asking thread to die!\n",
				"error killing decode thread",0);
			TerminateThread(thread_handle,0);
		}
		CloseHandle(thread_handle);
		thread_handle = INVALID_HANDLE_VALUE;
	}
	// close output system
	mod.outMod->Close();
	// deinitialize visualization
	mod.SAVSADeInit();	

    if(g_pModule){
        if(g_pModule->Close){
            g_pModule->Close(hkmp);
        }
    } 
}

// returns length of playing track
int getlength() {
	return InfoKPI.dwLength; 
}


// returns current output position, in ms.
// you could just use return mod.outMod->GetOutputTime(),
// but the dsp plug-ins that do tempo changing tend to make
// that wrong.
int getoutputtime() { 
	return decode_pos_ms + (mod.outMod->GetOutputTime() - mod.outMod->GetWrittenTime()); 
}


// called when the user releases the seek scroll bar.
// usually we use it to set seek_needed to the seek
// point (seek_needed is -1 when no seek is needed)
// and the decode thread checks seek_needed.
void setoutputtime(int time_in_ms) { 
	seek_needed = time_in_ms; 
}


// standard volume/pan functions
void setvolume(int volume) { }
void setpan(int pan) { }

// this gets called when the use hits Alt+3 to get the file info.
// if you need more info, ask me :)
int infoDlg(const char *fn, HWND hwnd)
{
	// CHANGEME! Write your own info dialog code here
	return 0;
}


// this is an odd function. it is used to get the title and/or
// length of a track.
// if filename is either NULL or of length 0, it means you should
// return the info of lastfn. Otherwise, return the information
// for the file in filename.
// if title is NULL, no title is copied into it.
// if length_in_ms is NULL, no length is copied into it.
void getfileinfo(const char *filename, char *title, int *length_in_ms)
{
	if (!filename || !*filename)  // currently playing file
	{
		if (length_in_ms) *length_in_ms = -1000; // the default is unknown file length (-1000).
		if (title) // get non-path portion.of filename
		{
			char *p=lastfn+strlen(lastfn);
			while (*p != '\\' && p >= lastfn) p--;
			strcpy(title,++p);
		}
	}
	else // some other file
	{
		if (length_in_ms) *length_in_ms = -1000; // the default is unknown file length (-1000).
		if (title) // get non path portion of filename
		{
			const char *p=filename+strlen(filename);
			while (*p != '\\' && p >= filename) p--;
			strcpy(title,++p);
		}
	}
}

void eq_set(int on, char data[10], int preamp) 
{ 
	// most plug-ins can't even do an EQ anyhow.. I'm working on writing
	// a generic PCM EQ, but it looks like it'll be a little too CPU 
	// consuming to be useful :)
	// if you _CAN_ do EQ with your format, each data byte is 0-63 (+20db <-> -20db)
	// and preamp is the same. 
}

DWORD WINAPI DecodeThread(LPVOID b)
{
	int done=0; // set to TRUE if decoding has finished
	int byte = InfoKPI.dwBitsPerSample / 8, ch_byte = InfoKPI.dwChannels * byte, render_byte = InfoKPI.dwUnitRender;//RENDER_SAMPLES * ch_byte;

	while (!killDecodeThread) 
	{
		if (seek_needed != -1) // seek is needed.
		{
			int offs;
			decode_pos_ms = seek_needed;
			seek_needed=-1;
			done=0;
			mod.outMod->Flush(decode_pos_ms); // flush output device and set // output position to the seek position			
			if(g_pModule){
				if(g_pModule->SetPosition){
					g_pModule->SetPosition(hkmp, decode_pos_ms);
				}
			} 
		}
		if (done) // done was set to TRUE during decoding, signaling eof
		{
			mod.outMod->CanWrite();		// some output drivers need CanWrite // to be called on a regular basis.
			if (!mod.outMod->IsPlaying()) 
			{
				// we're done playing, so tell Winamp and quit the thread.
				PostMessage(mod.hMainWindow,WM_WA_MPEG_EOF,0,0);
				return 0;	// quit thread
			}
			Sleep(10);		// give a little CPU time back to the system.
		}
		else if (mod.outMod->CanWrite() >= (render_byte * (mod.dsp_isactive() ? 2 : 1)))
			// CanWrite() returns the number of bytes you can write, so we check that
			// to the block size. the reason we multiply the block size by two if 
			// mod.dsp_isactive() is that DSP plug-ins can change it by up to a 
			// factor of two (for tempo adjustment).
		{	
			int l = render_byte;  // block length in bytes

			if(!first_render_play){ // first render play()
				first_render_play = 1;
				l = first_render_byte;
			}else if(g_pModule){
				if(g_pModule->Render){
					l = g_pModule->Render(hkmp, (BYTE*)sample_buffer, render_byte); // retrieve samples
				}
			} 
			if (!l)			// no samples means we're at eof
			{
				done=1;		 
			}
			else	// we got samples!
			{
				int samples = l / ch_byte;
				// give the samples to the vis subsystems
				mod.SAAddPCMData((char *)sample_buffer, InfoKPI.dwChannels, InfoKPI.dwBitsPerSample, decode_pos_ms);	
				mod.VSAAddPCMData((char *)sample_buffer, InfoKPI.dwChannels, InfoKPI.dwBitsPerSample, decode_pos_ms);
				// adjust decode position variable
				decode_pos_ms += (samples * 1000) / InfoKPI.dwSamplesPerSec;	
				// if we have a DSP plug-in, then call it on our samples
				if (mod.dsp_isactive()) 
					l = mod.dsp_dosamples((short *)sample_buffer, samples, InfoKPI.dwBitsPerSample, InfoKPI.dwChannels, InfoKPI.dwSamplesPerSec)
						* ch_byte; // dsp_dosamples
				// write the pcm data to the output system
				mod.outMod->Write(sample_buffer, l);
			}
		}
		else Sleep(20); 
		// if we can't write data, wait a little bit. Otherwise, continue 
		// through the loop writing more data (without sleeping)
	}
	return 0;
}



// exported symbol. Returns output module.

__declspec( dllexport ) In_Module * winampGetInModule2()
{	
	load_wrapper_config();
	return &mod;
}
