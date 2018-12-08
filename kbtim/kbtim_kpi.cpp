#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <windows.h>
#include <stdio.h>
#include "kmp_pi.h"
#include "kbtim_common.h"
#include "kbstr.h"
#include "common.h"

#ifdef _MSC_VER
#define HAVE_SNPRINTF
#define snprintf _snprintf
#endif

#ifndef _DEBUG
//#pragma comment(linker, "/opt:nowin98")
//#pragma comment(linker, "/nodefaultlib")
//#pragma comment(linker, "/entry:\"DllMain\"")
#endif

#ifdef __cplusplus
extern "C" {
#endif

BOOL APIENTRY DllMain(HANDLE, DWORD, LPVOID);

#ifdef __GNUC__
__declspec(dllexport) KMPMODULE* WINAPI kmp_GetTestModule(void);
__declspec(dllexport) DWORD WINAPI kmp_Config(HWND, DWORD, DWORD);
#else
KMPMODULE* WINAPI kmp_GetTestModule(void);
DWORD WINAPI kmp_Config(HWND, DWORD, DWORD);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

typedef KMPMODULE* (WINAPI *pfnTimGetModule)(HINSTANCE hInstance, DWORD dwVersion);
static HINSTANCE g_hDll1;    //kbtim.kpi �̃C���X�^���X�n���h��
static HINSTANCE g_hDll2;    //kbtim.dll �̃C���X�^���X�n���h��
static KMPMODULE *g_pModule;//kbtim.dll ���Ԃ��� KMPMODULE

static void __fastcall kbReplaceFileName(char *szDest, const char *cszReplace)
{
    char *p = szDest;
    char *lastPathDelimiter = szDest;
    while (*p) {
        if (IsDBCSLeadByte((BYTE) *p)) {
            p += 2;
            continue;
        }
        else if (*p == '\\') {
            lastPathDelimiter = p+1;
        }
        p++;
    }
    strcpy(lastPathDelimiter, cszReplace);
}
BOOL APIENTRY DllMain(HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:{
            g_hDll1 = (HINSTANCE) hModule;
            DisableThreadLibraryCalls((HMODULE) hModule);
            kbStr_Initialize();
            break;
        }
        case DLL_PROCESS_DETACH:{
            break;
        }
    }
    return TRUE;
}

static void WINAPI kmp_Init(void)
{
    if (g_pModule) {
        if (g_pModule->Init) {
            g_pModule->Init();
        }
    }
}
static void WINAPI kmp_Deinit(void)
{
    if (g_pModule) {
        if (g_pModule->Deinit) {
            g_pModule->Deinit();
        }
        g_pModule = NULL;
    }
    if (g_hDll2) {
        FreeLibrary(g_hDll2);
        g_hDll2 = NULL;
    }
}

KMPMODULE* WINAPI kmp_GetTestModule(void)
{
    static KMPMODULE Module;
    static KMPMODULE *pModule = NULL;
    if (!pModule) {
        //kbtim_GetModule �� 1 �񂵂��Ă�ł͂����Ȃ�
        char szDll1[FILEPATH_MAX];
        char szDll2[FILEPATH_MAX];
        GetModuleFileName(g_hDll1, szDll1, sizeof(szDll1));
        kbExtractFilePath(szDll2, szDll1, sizeof(szDll2));
        kbStrLCat(szDll2, "kbtim.dll", sizeof(szDll2));
        HINSTANCE hDll = LoadLibrary(szDll2);
        if (!hDll) {
            return NULL;
        }
        pfnTimGetModule kbtim_GetModule = (pfnTimGetModule) GetProcAddress(hDll, "kbtim_GetModule");
        if (!kbtim_GetModule) {
            FreeLibrary(hDll);
            return NULL;
        }
        pModule = kbtim_GetModule(g_hDll1, KBTIM_GETMODULE_VERSION);
        if (!pModule) {
            FreeLibrary(hDll);
        } else {
            memcpy(&Module, pModule, sizeof(KMPMODULE));
            Module.Init = kmp_Init;
            Module.Deinit = kmp_Deinit;
            if (pModule->dwReentrant != 0xFFFFFFFF) {
                //kbtim.dll �����g�p
                g_pModule = pModule;
            }
            pModule = &Module;
            g_hDll2 = hDll;
        }
    }
    return pModule;
}

DWORD WINAPI kmp_Config(HWND hWnd, DWORD dwVersion, DWORD dwReserved)
{
    if (dwVersion == 0) {
        //�v���O�C���Ɠ����t�H���_�ɂ��� kbtimsetup.exe ���N������
        //�R�}���h���C���p�����[�^�Ƃ��āA�������g(kbtim(_xxx).kpi) �̃t���p�X��n��
        char szDll1[FILEPATH_MAX];       //X:\...\kbtim(_xxx).kpi
        char szKbTimSetupExe[FILEPATH_MAX];  //X:\...\kbtimsetup.exe
        char szKbTimSetupParam[FILEPATH_MAX];//"X:\...\kbtim(_xxx).kpi"
        //*.kpi
        GetModuleFileName(g_hDll1, szDll1, sizeof(szDll1));
        //kbtimsetup.exe
        kbExtractFilePath(szKbTimSetupExe, szDll1, sizeof(szKbTimSetupExe));
#ifdef _WIN64
        kbStrLCat(szKbTimSetupExe, "kbtimsetup_x64.exe", sizeof(szKbTimSetupExe));
#else
        kbStrLCat(szKbTimSetupExe, "kbtimsetup.exe", sizeof(szKbTimSetupExe));
#endif
        //�R�}���h���C���p�����[�^("" �Ŋ���)
        szKbTimSetupParam[0] = '"';
        szKbTimSetupParam[1] = 0;
        kbStrLCat(szKbTimSetupParam, szDll1, sizeof(szKbTimSetupParam));
        kbStrLCat(szKbTimSetupParam, "\"", sizeof(szKbTimSetupParam));
        ShellExecute(NULL, "open", szKbTimSetupExe, szKbTimSetupParam, NULL, SW_SHOWNORMAL);
    }
    return 0;
}




#ifdef WINAMP_PLUGIN
/*
Winamp TiMidity++ input plug-in


based
Example Winamp .RAW input plug-in
*/

#include "../Winamp/in2.h"


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

// def configuration.
#define NCH 2
#define SAMPLERATE 44100
#define BPS 16

char lastfn[MAX_PATH];  // currently playing file (used for getting info on the current file)
int file_length;                // file length, in bytes
int decode_pos_ms;              // current decoding position, in milliseconds.
                                                // Used for correcting DSP plug-in pitch changes
int paused;                             // are we paused?
volatile int seek_needed; // if != -1, it is the point that the decode
                                                  // thread should seek to, in ms.
HANDLE input_file=INVALID_HANDLE_VALUE; // input file handle
volatile int killDecodeThread=0;                        // the kill switch for the decode thread
HANDLE thread_handle=INVALID_HANDLE_VALUE;      // the handle to the decode thread
DWORD WINAPI DecodeThread(LPVOID b); // the decode thread procedure

#define RENDER_SAMPLES 576 // winamp plugin default
#define MAX_RENDER_SAMPLES 4096
static char *sample_buffer = NULL; // sample buffer. twice as // big as the blocksize
int first_render_play;
int first_render_byte;
int play_load = 0;
int kpi_wrapper = 0; // 0:in_tim 1: in_kpi

typedef KMPMODULE* (WINAPI *pfnTimGetModule)(HINSTANCE hInstance, DWORD dwVersion);
static HKMP hkmp = NULL;
static char szDll1[FILEPATH_MAX];
static char szDll2[FILEPATH_MAX];
static char szIni1[FILEPATH_MAX];
static char szDll2File[FILEPATH_MAX];
#define EXT_INFO_MAX 32000
static char szDll2Ext[EXT_INFO_MAX];
static char szDll2Dsc[EXT_INFO_MAX];

static SOUNDINFO InfoKPI =
{       // init param
    SAMPLERATE, // DWORD dwSamplesPerSec �T���v�����O���g��(44100, 22050 �Ȃ�)
    NCH,        // DWORD dwChannels �`�����l����( mono = 1, stereo = 2)
    BPS,        // DWORD dwBitsPerSample �ʎq���r�b�g��( 8 or 16)
    0xFFFFFFFF, // DWORD dwLength �Ȃ̒����iSPC �̂悤�Ɍv�Z�s�\�ȏꍇ�� 0xFFFFFFFF�j
    1,          // DWORD dwSeekable �V�[�N���T�|�[�g���Ă���ꍇ�� 1�A���Ȃ��ꍇ�� 0
    0,          // DWORD dwUnitRender Render�֐��̑�R�����͂��̒l�̔{�����n�����i�ǂ�Ȓl�ł��ǂ��ꍇ�� 0�j
    0,          // DWORD dwReserved1 ��� 0
    0,          // DWORD dwReserved2 ��� 0
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
        IN_VER, // defined in IN2.H
        "Timidity++ MIDI Plugin"
        // winamp runs on both alpha systems and x86 ones. :)
#ifdef __alpha
        " (AXP)"
#else
        " (x86)"
#endif
        ,
        0,      // hMainWindow (filled in by winamp)
        0,  // hDllInstance (filled in by winamp)
        "MID\0SMF File (*.MID)\0"
        "MIDI\0SMF File (*.MID)\0"
        "SMF\0SMF File (*.SMF)\0"
        "RMI\0RMID File (*.RMI)\0"
        "RCP\0RCP File (*.RCP)\0"
        "R36\0RCP File (*.R36)\0"
        "G18\0RCP File (*.G18)\0"
        "G36\0RCP File (*.G36)\0"
        // this is a double-null limited list. "EXT\0Description\0EXT\0Description\0" etc.
        ,
        1,      // is_seekable
        1,      // uses output plug-in system
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
        NULL,           // setinfo call filled in by winamp
        0 // out_mod filled in by winamp

};

static void create_sample_buffer(void)
{
        // sample buffer. twice as // big as the blocksize
        // max = MAX_RENDER_SAMPLES * 2ch * 32bit/byte * twice;
        if (!sample_buffer)
                sample_buffer = (char*) malloc(MAX_RENDER_SAMPLES * 2 * (32 / 8) * 2);
}

static void free_sample_buffer(void)
{
        safe_free(sample_buffer);
}

static void get_playmode(void)
{
        // kbtim�ƈႢ �T���v�����[�g����ini�Őݒ肷��
        char opt_playmode[16], *arg;
        int size = sizeof(opt_playmode) - 1;

        InfoKPI.dwSamplesPerSec = GetPrivateProfileInt("TIMIDITY", "output_rate", 0, szIni1);
    GetPrivateProfileString("TIMIDITY", "opt_playmode", "", opt_playmode, size, szIni1);
    opt_playmode[size - 1] = 0;
        arg = opt_playmode;
        while (*(++arg))
                switch (*arg) {
                case 'S':       /* stereo */
                        InfoKPI.dwChannels = 2;
                        break;
                case 'M':
                        InfoKPI.dwChannels = 1;
                        break;
                //case 'D':     /* D for float 64-bit */
                //      InfoKPI.dwBitsPerSample = -64;
                //      break;
                //case '6':     /* 6 for 64-bit */
                //      InfoKPI.dwBitsPerSample = 64;
                //      break;
                //case 'f':     /* f for float 32-bit */
                //      InfoKPI.dwBitsPerSample = -32;
                //      break;
                case '3':       /* 3 for 32-bit */
                        InfoKPI.dwBitsPerSample = 32;
                        break;
                case '2':       /* 2 for 24-bit */
                        InfoKPI.dwBitsPerSample = 24;
                        break;
                case '1':       /* 1 for 16-bit */
                        InfoKPI.dwBitsPerSample = 16;
                        break;
                case '8':
                        InfoKPI.dwBitsPerSample = 8;
                        break;
                default:
                        break;
                }
        InfoKPI.dwUnitRender = RENDER_SAMPLES * InfoKPI.dwChannels * InfoKPI.dwBitsPerSample / 8;
}

static void load_wrapper_config(void)
{
        int i;
        int end = 0;
    static KMPMODULE Module;
    static KMPMODULE *pModule = NULL;

    GetModuleFileName(g_hDll1, szDll1, sizeof(szDll1));
        kbExtractReplacedFileExt(szIni1, szDll1, ".ini");// ini�擾 �v���O�C���ƃt�@�C�����������Ŋg���q�� INI
        kpi_wrapper = GetPrivateProfileInt("wrapper", "kpi_wrapper", 0, szIni1);
        play_load = GetPrivateProfileInt("wrapper", "play_load", 0, szIni1);
        if (!kpi_wrapper) {     // in_kbtim
                HINSTANCE hDll = NULL;
                get_playmode();
                kbExtractFilePath(szDll2, szDll1, sizeof(szDll1));
                kbStrLCat(szDll2, "kbtim.dll", sizeof(szDll2));
                hDll = LoadLibrary(szDll2);
                if (!hDll) {
                        return;
                }
                //kbtim_GetModule �� 1 �񂵂��Ă�ł͂����Ȃ�
                pfnTimGetModule kbtim_GetModule = (pfnTimGetModule) GetProcAddress(hDll, "kbtim_GetModule");
                if (!kbtim_GetModule) {
                        FreeLibrary(hDll);
                        return;
                }
                pModule = kbtim_GetModule(g_hDll1, KBTIM_GETMODULE_VERSION);
                if (!pModule) {
                        FreeLibrary(hDll);
                        return;
                }
                memcpy(&Module, pModule, sizeof(KMPMODULE));
                if (pModule->dwReentrant != 0xFFFFFFFF) //kbtim.dll �����g�p
                        g_pModule = pModule;
                pModule = &Module;
                g_hDll2 = hDll;
        } else { // in_kpi_wrapper
                // kbtim�ƈႢ �T���v�����[�g����ini�Őݒ肷��
                HINSTANCE hKpi = NULL;
                InfoKPI.dwSamplesPerSec = GetPrivateProfileInt("wrapper", "output_rate", 0, szIni1);
                InfoKPI.dwBitsPerSample = GetPrivateProfileInt("wrapper", "output_bit", 0, szIni1);
                InfoKPI.dwChannels = GetPrivateProfileInt("wrapper", "output_channel", 0, szIni1);
                InfoKPI.dwUnitRender = RENDER_SAMPLES * InfoKPI.dwChannels * InfoKPI.dwBitsPerSample / 8;
                GetPrivateProfileString("wrapper", "file", "", szDll2File, sizeof(szDll2File), szIni1); // kpi�t�@�C�����擾
                if (kbStrLen(szDll2File)) { // kpi�擾 �t�@�C�����w��
                        kbExtractFilePath(szDll2, szDll1, sizeof(szDll2));
                        kbStrLCat(szDll2, szDll2File, sizeof(szDll2));
                } else {// kpi�擾 �v���O�C���t�@�C��������"in_"�폜 �g���q�� KPI
                        const char *file = kbExtractFileName(szDll1) + 3; // delete "in_"
                        kbExtractFilePath(szDll2File, szDll1, sizeof(szDll2));
                        kbStrLCat(szDll2File, file, sizeof(szDll2));
                        kbExtractReplacedFileExt(szDll2, szDll2File, ".kpi");// �g���q�� KPI
                }
                // load kpi
                hKpi = LoadLibrary(szDll2);
                if (!hKpi) {
                        return;
                }
                pfnGetKMPModule GetModule = (pfnGetKMPModule) GetProcAddress(hKpi, SZ_KMP_GETMODULE);
                if (!GetModule) {
                        FreeLibrary(hKpi);
                        return;
                }
                pModule = GetModule();
                if (!pModule) {
                        FreeLibrary(hKpi);
                        return;
                }
                memcpy(&Module, pModule, sizeof(KMPMODULE));
            if (pModule->dwReentrant != 0xFFFFFFFF)
                        g_pModule = pModule;
                pModule = &Module;
                g_hDll2 = hKpi;
                // Description
                kbStrLCat(szDll2Dsc, g_pModule->pszDescription, EXT_INFO_MAX);
#ifdef __alpha
                kbStrLCat(szDll2Dsc, " (AXP)", EXT_INFO_MAX);
#else // windows
#ifdef _WIN64
                kbStrLCat(szDll2Dsc, " (x64)", EXT_INFO_MAX);
#else
                kbStrLCat(szDll2Dsc, " (x86)", EXT_INFO_MAX);
#endif
#endif
                mod.description = szDll2Dsc;
                // Extension Info
                for (i = 0; i < 16; i++) {
                        int tmp1, tmp2;
                        char szext[EXT_INFO_MAX], szinf[EXT_INFO_MAX];
                        char *str = (char*) g_pModule->ppszSupportExts[i];

                        if (str == NULL)
                                break;
                        memset(szext, 0, sizeof(szext));
                        memset(szinf, 0, sizeof(szinf));
                        tmp1 = kbStrLen(str);
                        --tmp1;
                        if (tmp1 <= 0)
                                break;
                        memcpy(szext, (char*) str + 1, tmp1); // delete piriod
                        snprintf(szinf, sizeof(szinf), "%s file (*.%s)", szext, szext);
                        tmp1 = kbStrLen(szext);
                        tmp2 = kbStrLen(szinf);
                        if (tmp1 && tmp2) {
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
}

typedef DWORD (WINAPI *kmp_Config_gui)(HWND hWnd, DWORD dwVersion, DWORD dwReserved);

void config(HWND hwndParent)
{
//      MessageBox(hwndParent, "No configuration.", "Configuration",MB_OK);
        if (!kpi_wrapper) {
                char szDll1[FILEPATH_MAX];       //X:\...\kbtim(_xxx).kpi
                char szKbTimSetupExe[FILEPATH_MAX];  //X:\...\kbtimsetup.exe
                char szKbTimSetupParam[FILEPATH_MAX];//"X:\...\kbtim(_xxx).kpi"
                //*.kpi
                GetModuleFileName(g_hDll1, szDll1, sizeof(szDll1));
                //kbtimsetup.exe
                kbExtractFilePath(szKbTimSetupExe, szDll1, sizeof(szKbTimSetupExe));
#ifdef _WIN64
                kbStrLCat(szKbTimSetupExe, "kbtimsetup_x64.exe", sizeof(szKbTimSetupExe));
#else
                kbStrLCat(szKbTimSetupExe, "kbtimsetup.exe", sizeof(szKbTimSetupExe));
#endif
                //�R�}���h���C���p�����[�^("" �Ŋ���)
                szKbTimSetupParam[0] = '"';
                szKbTimSetupParam[1] = 0;
                kbStrLCat(szKbTimSetupParam, szDll1, sizeof(szKbTimSetupParam));
                kbStrLCat(szKbTimSetupParam, "\"", sizeof(szKbTimSetupParam));
                ShellExecute(NULL, "open", szKbTimSetupExe, szKbTimSetupParam, NULL, SW_SHOWNORMAL);
        } else {
                ((kmp_Config_gui) GetProcAddress(g_hDll2, "kmp_Config"))(hwndParent, 0, 0);
        }
    return;
}

void about(HWND hwndParent)
{
        if (!kpi_wrapper)
                MessageBox(hwndParent,"Timidity++ MIDI Plugin", "About Plugin",MB_OK);
        else
                MessageBox(hwndParent,"KbMedia Player Plugin wrapper", "About Plugin",MB_OK);
}

void init(void)
{
        create_sample_buffer();
        if (g_pModule) {
        if (g_pModule->Init)
            g_pModule->Init();
    }
}

void quit(void)
{
    if (g_pModule) {
        if (g_pModule->Deinit) {
            g_pModule->Deinit();
        }
        g_pModule = NULL;
    }
    if (g_hDll2) {
        FreeLibrary(g_hDll2);
        g_hDll2 = NULL;
    }
        free_sample_buffer();
}

int isourfile(const char *fn)
{
// used for detecting URL streams.. unused here.
// return !strncmp(fn,"http://",7); to detect HTTP streams, etc
        return 0;
}

// called when winamp wants to play a file
int play(const char *fn)
{
        int maxlatency;
        DWORD thread_id;
        int tmp_byte = InfoKPI.dwUnitRender;

        paused=0;
        decode_pos_ms=0;
        seek_needed=-1;
        first_render_play = 0;
        first_render_byte = 0;
        memset(sample_buffer, 0, sizeof(sample_buffer));

    if (g_pModule) {
        if (g_pModule->Open) {
            hkmp = g_pModule->Open(fn, &InfoKPI);
        }
    }
        if (!hkmp)
                return 1;
        if (InfoKPI.dwUnitRender <= 0) { // �Ȃ���0�ɂ����ꍇ
                InfoKPI.dwUnitRender = tmp_byte;
        //      InfoKPI.dwUnitRender = RENDER_SAMPLES;
        }

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
        if (play_load) {
                if (g_pModule) {
                        if (g_pModule->Render)
                                first_render_byte = g_pModule->Render(hkmp, (BYTE*) sample_buffer, InfoKPI.dwUnitRender); // retrieve samples
                }
        } else {
                first_render_play = 1;
        }

        // launch decode thread
        killDecodeThread=0;
        thread_handle = (HANDLE) CreateThread(NULL,0,(LPTHREAD_START_ROUTINE) DecodeThread,NULL,0,&thread_id);

        return 0;
}

// standard pause implementation
void pause() { paused=1; mod.outMod->Pause(1); }
void unpause() { paused=0; mod.outMod->Pause(0); }
int ispaused() { return paused; }


// stop playing.
void stop(void)
{
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

    if (g_pModule) {
        if (g_pModule->Close) {
            g_pModule->Close(hkmp);
        }
    }
}

// returns length of playing track
int getlength(void)
{
        return InfoKPI.dwLength;
}


// returns current output position, in ms.
// you could just use return mod.outMod->GetOutputTime(),
// but the dsp plug-ins that do tempo changing tend to make
// that wrong.
int getoutputtime(void)
{
        return decode_pos_ms + (mod.outMod->GetOutputTime() - mod.outMod->GetWrittenTime());
}


// called when the user releases the seek scroll bar.
// usually we use it to set seek_needed to the seek
// point (seek_needed is -1 when no seek is needed)
// and the decode thread checks seek_needed.
void setoutputtime(int time_in_ms)
{
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

static void get_filename(const char *filename, char *title)
{
        char *p=lastfn+strlen(filename);
        while (*p != '\\' && p >= filename) p--;
        strcpy(title,++p);
}

#define BIN_SIZE (0xC0)

static void get_title(const char *filename, char *title)
{
        FILE *file = NULL;
        char bin[BIN_SIZE];
        int i = 0, j = 0, flg = 0;

        file = fopen(filename, "rb");
        if (file) {
                while (!feof(file)) {
                        if (i >= BIN_SIZE)
                                break;
                        bin[i] = fgetc(file);
                        i++;
                }
                fclose(file);
        }
        if (i < BIN_SIZE)
                flg = 1; // no title
        else if (memcmp(bin, "melo", 4) == 0) // mfi
                flg = 1; // no title
        else if (memcmp(bin, "RCM-", 4) == 0 || memcmp(bin, "COME", 4) == 0) { // rcp
                for (i = 0x20; i < BIN_SIZE; i++) {
                        if (bin[i] == (char) 0x00) {
                                title[j] = '\0';
                                break;
                        } else if (bin[i] == (char) 0x20 && bin[i + 1] == (char) 0x20 && bin[i + 2] == (char) 0x20) { // space*3
                                title[j] = '\0';
                                break;
                        }
                        title[j++] = bin[i];
                }
                title[j] = '\0';
        } else if (strncmp(bin, "RIFF", 4) == 0 || strncmp(bin, "MThd", 4) == 0) { // rmf or smf
                int stp = 0;
                i = 0;
                while (i <= BIN_SIZE - 4) {
                        if (memcmp(bin + i, "MTrk", 4) == 0) {
                                stp = 1;
                                break;
                        }
                        i++;
                }
                if (stp) {
                        stp = 0;
                        while (i <= BIN_SIZE - 4) {
                                if (bin[i] == (char) 0x00 && bin[i + 1] == (char) 0xFF && bin[i + 2] == (char) 0x03) {
                                        stp = 1;
                                        break;
                                }
                                i++;
                        }
                        if (stp) {
                                i += 4; // skip 00FFxxxx
                                for (; i < BIN_SIZE; i++) {
                                        if (bin[i] == (char) 0x00 || bin[i + 1] == (char) 0xFF) {
                                                title[j] = '\0';
                                                break;
                                        }
                                        title[j++] = bin[i];
                                }
                        } else
                                flg = 1; // no title
                } else
                        flg = 1; // no title
        } else // if (memcmp(bin, "melo", 4) == 0) // mfi
                flg = 1; // no title
        if (flg)
                get_filename(filename, title); // get non path portion of filename
}

#if defined(_MSC_VER)
#ifndef strncasecmp
#define strncasecmp(a,b,c)      _strnicmp((a),(b),(c))
#endif
#else
#undef strncasecmp
#endif

static void get_title_filename(const char *filename, char *title)
{
        const char *ext;

        if (filename[0] == 0)
                return;
        if (!kpi_wrapper) { // only midi ext
                get_title(filename, title); // title or get non path portion of filename
                return;
        }
        ext = kbExtractFileExt(filename);
#ifdef strncasecmp
        if (strncasecmp(ext, ".MID", 4) == 0
                || strncasecmp(ext, ".MIDI", 5) == 0
                || strncasecmp(ext, ".SMF", 4) == 0
                || strncasecmp(ext, ".RMI", 4) == 0
                || strncasecmp(ext, ".RCP", 4) == 0
                || strncasecmp(ext, ".R36", 4) == 0
                || strncasecmp(ext, ".G18", 4) == 0
                || strncasecmp(ext, ".G36", 4) == 0) // search midi ext
#else
        if (strncmp(ext, ".mid", 4) == 0
                || strncmp(ext, ".MID", 4) == 0
                || strncmp(ext, ".midi", 5) == 0
                || strncmp(ext, ".MIDI", 5) == 0
                || strncmp(ext, ".smf", 4) == 0
                || strncmp(ext, ".SMF", 4) == 0
                || strncmp(ext, ".rmi", 4) == 0
                || strncmp(ext, ".RMI", 4) == 0
                || strncmp(ext, ".rcp", 4) == 0
                || strncmp(ext, ".RCP", 4) == 0
                || strncmp(ext, ".r36", 4) == 0
                || strncmp(ext, ".R36", 4) == 0
                || strncmp(ext, ".g18", 4) == 0
                || strncmp(ext, ".G18", 4) == 0
                || strncmp(ext, ".g36", 4) == 0
                || strncmp(ext, ".G36", 4) == 0) // search midi ext
#endif
                get_title(filename, title); // title or get non path portion of filename
        else
                get_filename(filename, title); // get non path portion of filename
}

void getfileinfo(const char *filename, char *title, int *length_in_ms)
{
        if (!filename || !*filename)  // currently playing file
        {
                if (length_in_ms) *length_in_ms = -1000; // the default is unknown file length (-1000).
                if (title) get_title_filename(lastfn, title); // title or get non-path portion.of filename

        }
        else // some other file
        {
                if (length_in_ms) *length_in_ms = -1000; // the default is unknown file length (-1000).
                if (title)      get_title_filename(filename, title); // title or get non path portion of filename
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
                        if (g_pModule) {
                                if (g_pModule->SetPosition) {
                                        g_pModule->SetPosition(hkmp, decode_pos_ms);
                                }
                        }
                }
                if (done) // done was set to TRUE during decoding, signaling eof
                {
                        mod.outMod->CanWrite();         // some output drivers need CanWrite // to be called on a regular basis.
                        if (!mod.outMod->IsPlaying())
                        {
                                // we're done playing, so tell Winamp and quit the thread.
                                PostMessage(mod.hMainWindow,WM_WA_MPEG_EOF,0,0);
                                return 0;       // quit thread
                        }
                        Sleep(10);              // give a little CPU time back to the system.
                }
                else if (mod.outMod->CanWrite() >= (render_byte * (mod.dsp_isactive() ? 2 : 1)))
                        // CanWrite() returns the number of bytes you can write, so we check that
                        // to the block size. the reason we multiply the block size by two if
                        // mod.dsp_isactive() is that DSP plug-ins can change it by up to a
                        // factor of two (for tempo adjustment).
                {
                        int l = render_byte;  // block length in bytes
                        static char sample_buffer[RENDER_SAMPLES * 2 * (32 / 8) * 2]; // sample buffer. twice as // big as the blocksize

                        if (!first_render_play) { // first render play()
                                first_render_play = 1;
                                l = first_render_byte;
                        } else if (g_pModule) {
                                if (g_pModule->Render)
                                        l = g_pModule->Render(hkmp, (BYTE*) sample_buffer, render_byte); // retrieve samples
                        }
                        if (!l)                 // no samples means we're at eof
                        {
                                done=1;
                        }
                        else    // we got samples!
                        {
                                int samples = l / ch_byte;
                                // give the samples to the vis subsystems
                                mod.SAAddPCMData((char*) sample_buffer, InfoKPI.dwChannels, InfoKPI.dwBitsPerSample, decode_pos_ms);
                                mod.VSAAddPCMData((char*) sample_buffer, InfoKPI.dwChannels, InfoKPI.dwBitsPerSample, decode_pos_ms);
                                // adjust decode position variable
                                decode_pos_ms += (samples * 1000) / InfoKPI.dwSamplesPerSec;
                                // if we have a DSP plug-in, then call it on our samples
                                if (mod.dsp_isactive())
                                        l = mod.dsp_dosamples((short*) sample_buffer, samples, InfoKPI.dwBitsPerSample, InfoKPI.dwChannels, InfoKPI.dwSamplesPerSec)
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

__declspec( dllexport ) In_Module * winampGetInModule2(void)
{
        load_wrapper_config();
        return &mod;
}
#endif /* WINAMP_PLUGIN */
