#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <windows.h>
#include <stdio.h>
#include <shlwapi.h>
#include <process.h>
#include <float.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <io.h>
#include <time.h>
//#pragma comment(lib, "winmm")

#ifdef _DEBUG
#define KBTIM_LEAK_CHECK //���������[�N���o�i���Ȃ��ꍇ�̓R�����g�A�E�g�j
#define KMPMODULE_PLUGIN_VERSION 0x7FFFFFFF
#else
#define KMPMODULE_PLUGIN_VERSION 23
#endif

static const char g_cszKbTimCopyright[] = "Copyright (C) 2004-2012 Tuukka Toivonen, Masanao Izumo, Saito, Kobarin, etc...";
static const char g_cszKbTimDescription[] = "TiMidity++ MIDI Decoder Plugin for KbMedia Player based on TiMidity++ 2.14.0(2012/06/29)";
static const char *g_cppszSupportExts[] = {".mid", ".midi", ".smf", ".rmi", ".rcp", ".r36", ".g36", ".g18", NULL};

#ifdef KBTIM_LEAK_CHECK
#include <crtdbg.h>
#endif

extern "C"{

#include "timidity.h"
#include "instrum.h"
#include "playmidi.h"
#include "output.h"
#include "controls.h"

#include "w32g.h"
#include "w32g_utl.h"

extern void kbtim_initialize(const char *cszCfgFile);
extern void kbtim_uninitialize(void);
extern void kbtim_play_midi_file(char *szFileName);


extern char *opt_aq_max_buff;
extern char *opt_aq_fill_buff;
}

#include "kmp_pi.h"
#include "ringbuf.h"
#include "kbtim_setting.h"
#include "kbtim.h"
#include "kbtim_common.h"
#include "kbstr.h"

//�萔
enum{
    WM_KBTIM_INIT = WM_USER, //������
    WM_KBTIM_QUIT,           //��n��
    WM_KBTIM_PLAY,           //���t�J�n�FwParam = (char*)szFileName
                             //�i���t���I���܂Ŗ߂�Ȃ��j
    WM_KBTIM_STOP,           //���t��~�FwParam = (HANDLE)hEvent
                             //�i�Ă΂ꂽ�� hEvent ���V�O�i����ԂɃZ�b�g�j
};

static CRITICAL_SECTION g_cs;
static HINSTANCE g_hKpi = NULL; //kbtim.kpi �̃C���X�^���X�n���h��
static HINSTANCE g_hDll = NULL; //kbtim.dll �̃C���X�^���X�n���h��
static LONG g_lCanUse = -1; //InterlocedIncrement(&g_lCanUse) == 0 �̂Ƃ��g�p�\
static const int UNIT_SAMPLE = 1024;
static const int PRELOAD_MS = 3500; //�ʏ펞
static const int PRELOAD_MS_REALTIME_PLAYING = 5500;//���t���ɉ��F�Ǎ��L����
    //��ǂ݃o�b�t�@�̃T�C�Y�i�~���b�j
    //�l���傫���قǁA
    //�E���Ƃт��ɂ����Ȃ�
    //�E�������g�p�ʂ��傫���Ȃ�
    //�E���t�J�n����A�V�[�N����̓��삪�d���Ȃ�
    //KbMedia Player �{�̂���ǂݏ������s���Ă���̂ŁA�l��傫�����Ă������b�g�͏��Ȃ�
    //���t���ɉ��F�Ǎ����L���ȂƂ�������ǂ݃o�b�t�@��傫������
KbTimDecoder* KbTimDecoder::g_pDecoder = NULL;

///r
void __fastcall KbTimDecoder::calc_preload_time(void)
{
    double time1 = atof(opt_aq_max_buff); /* max buffer */
	double time2 = atof(opt_aq_fill_buff); /* init filled */
	double base = opt_realtime_playing ? PRELOAD_MS_REALTIME_PLAYING : PRELOAD_MS;  /* default preload ms */

  //  if(strchr(opt_aq_max_buff, '%'))
  //  {
		//time1 = base * (time1 - 100) * DIV_100;
		//if(time1 < 0) time1 = 0;
  //  }
	if(time2 <= 0)
		time2 = base;
    else if(strchr(opt_aq_fill_buff, '%'))
		time2 = base * time2 * DIV_100;
	else 
		time2 = time2 * 1000; // sec to ms
	cdwPreloadMS = time2;
}

void __fastcall KbTimDecoder::ctl_event(CtlEvent *e)
{
    if(e->type == CTLE_PLAY_START){
        EnterCriticalSection(&m_cs);
        DWORD dwBps;   //�r�b�g��
        DWORD dwCh = 2;//�`�����l���i�X�e���I�j
        DWORD dwBytesPerSample;//1 �T���v��������̃o�C�g��
        DWORD dwPreloadBytes;  //��ǂ݃o�b�t�@�̃T�C�Y�i�o�C�g���j
        m_SoundInfo.dwLength = MulDiv(e->v1, 1000, m_pm.rate);//�Ȃ̒���
        if((m_pm.encoding & PE_F64BIT)){//64bit FLOAT
            dwBps = -64;
        }
        else if((m_pm.encoding & PE_F32BIT)){//32bit FLOAT
            dwBps = -32;
        }
        else if((m_pm.encoding & PE_32BIT)){//32bit INT
            dwBps = 32;
        }
        else if((m_pm.encoding & PE_24BIT)){//24bit
            dwBps = 24;
        }
        else{//16bit
            dwBps = 16;
        }
        dwBytesPerSample = (abs((int)dwBps)/8)*dwCh;
        m_SoundInfo.dwUnitRender = UNIT_SAMPLE*dwBytesPerSample;
        m_SoundInfo.dwSamplesPerSec = m_pm.rate;
        m_SoundInfo.dwBitsPerSample = dwBps;
        m_SoundInfo.dwChannels = dwCh;
        m_SoundInfo.dwSeekable = 1;
        //��ǂ݃o�b�t�@�̃T�C�Y��ݒ�
        //������x�̑傫���͊m�ۂ��Ă����K�v������
        //���t���ɉ��F�Ǎ����L���ȂƂ�������ǂ݃o�b�t�@��傫������
///r
		calc_preload_time();
 //       const DWORD cdwPreloadMS = opt_realtime_playing ? PRELOAD_MS_REALTIME_PLAYING : PRELOAD_MS;
        dwPreloadBytes = ((m_pm.rate * cdwPreloadMS) / 1000) * dwBytesPerSample;
        if(dwPreloadBytes < m_SoundInfo.dwUnitRender*2){
            //���Ȃ��Ƃ� Render �P�񕪂͐�ǂ݂���
            dwPreloadBytes = m_SoundInfo.dwUnitRender*2;
        }
        while(dwPreloadBytes % dwBytesPerSample){
            dwPreloadBytes++;//dwBytesPerSample �̔{���ɂ���
        }
        m_PreloadBuffer.SetSize(dwPreloadBytes);
        SetEvent(m_hEventOpen);
        LeaveCriticalSection(&m_cs);
        //�f�R�[�h�v��������܂ő҂�
        WaitForSingleObject(m_hEventCanWrite, INFINITE);
    }
}
int __fastcall KbTimDecoder::ctl_read(ptr_size_t *valp) ///r
{
    if(m_bEOF){
        return RC_QUIT;
    }
    else if(m_dwSeek != 0xFFFFFFFF){
        //�V�[�N
        //�܂� output_data() ���Ă΂�Ă��Ȃ���Ԃł́A
        //������ RC_JUMP ��Ԃ��Ă���������Ă��܂�
        //output_data() ���Ă΂ꂽ��ł���� RC_JUMP �����������
        if(m_Setting.IsUpdated()){
            //OutputDebugString("m_Setting.IsUpdated()==TRUE\n");
            return RC_QUIT;
        }
        EnterCriticalSection(&m_cs);
        *valp = MulDiv(m_dwSeek, m_pm.rate, 1000);
        m_dwSeek = 0xFFFFFFFF;
        m_PreloadBuffer.Reset();
        ResetEvent(m_hEventCanWrite);
        SetEvent(m_hEventOpen);
        LeaveCriticalSection(&m_cs);
        WaitForSingleObject(m_hEventCanWrite, INFINITE);
        //OutputDebugString("RC_JUMP\n");
        return RC_JUMP;
    }
    return 0;
}
int __cdecl KbTimDecoder::ctl_cmsg(int type, int verbosity_level, const char *fmt, ...)
{
#if 0
    //kbtim_config.h �� #define cmsg(X) ���R�����g�A�E�g���Ȃ���
    //���̊֐����Ă΂�邱�Ƃ͂Ȃ�
    char szMessage[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(szMessage, sizeof(szMessage)-2, fmt, ap);
    va_end(ap);
    szMessage[sizeof(szMessage)-2] = 0;
    strcat(szMessage, "\n");
    OutputDebugString(szMessage);
#endif
    return 0;
}
int __fastcall KbTimDecoder::open_output(void)
{
    m_pm.encoding &= ~(PE_ULAW | PE_ALAW | PE_BYTESWAP);
    if((m_pm.encoding & PE_16BIT) || 
       (m_pm.encoding & PE_24BIT) ||
       (m_pm.encoding & PE_32BIT) ||
       (m_pm.encoding & PE_F32BIT) ||
       (m_pm.encoding & PE_F64BIT) ||
       0 )
    {
        m_pm.encoding |= PE_SIGNED;
    }
    else{
        m_pm.encoding &= ~PE_SIGNED;
    }
    m_pm.fd = STDOUT_FILENO;
    return 0;
}
void __fastcall KbTimDecoder::close_output(void)
{
    m_pm.fd = -1;
}
int32 __fastcall KbTimDecoder::output_data(const uint8 *buf, size_t len)
{
    EnterCriticalSection(&m_cs);
    if(!m_nStart){
        //�t�@�C���I�[�v����Aoutput_data ���P�x�ł��Ă΂�Ă���΁A
        //ctl_read �� RC_JUMP ��Ԃ����Ƃ��Ɋm���ɃV�[�N�����������
        m_nStart = 1;
        ResetEvent(m_hEventCanWrite);
        SetEvent(m_hEventOpen);
        LeaveCriticalSection(&m_cs);
        WaitForSingleObject(m_hEventCanWrite, INFINITE);
        EnterCriticalSection(&m_cs);
    }
    while(!m_bEOF && (m_dwSeek == 0xFFFFFFFF) && len > 0){
        DWORD dwRemain;//��ǂ݃o�b�t�@�̋󂫃T�C�Y
        DWORD dwWrite; //��ǂ݃o�b�t�@�ւ̏������݃T�C�Y
        while((dwRemain = m_PreloadBuffer.GetRemain()) == 0){
            //�o�b�t�@�ɋ󂫂��Ȃ�
            ResetEvent(m_hEventCanWrite);
            LeaveCriticalSection(&m_cs);
            //char sz[256];
            //DWORD dwWaitStart = timeGetTime();
            //DWORD dwElapsed;
            //�o�b�t�@���󂭂܂ő҂�
            WaitForSingleObject(m_hEventCanWrite, INFINITE);
            /*dwElapsed = timeGetTime()-dwWaitStart;
            if(dwElapsed > 200){
                wsprintf(sz, "output_data::WaitForSingleObject, waittime=%d\n", dwElapsed);
                OutputDebugString(sz);
            }*/
            EnterCriticalSection(&m_cs);
            /*dwRemain = m_PreloadBuffer.GetRemain();
            if(dwRemain == 0 ||
               dwRemain > UNIT_SAMPLE*(m_SoundInfo.dwBitsPerSample/8)*m_SoundInfo.dwChannels){
                wsprintf(sz, "output_data::dwRemain=%d\n", dwRemain);
                OutputDebugString(sz);
            }*/
            if(m_bEOF){//���t�I��
                SetEvent(m_hEventWrite);
                LeaveCriticalSection(&m_cs);
                return 0;
            }
            else if(m_dwSeek != 0xFFFFFFFF){//�V�[�N�v��
                LeaveCriticalSection(&m_cs);
                return 0;
            }
        }
        if((DWORD)len > dwRemain){
            dwWrite = dwRemain;
        }
        else{
            dwWrite = len;
        }
        if(m_nRequest){
            //��ǂ݂��Ԃɍ���Ȃ��ꍇ
            //��ǂ݃o�b�t�@�o�R�ł͂Ȃ��ARender �֐��ŗv�����ꂽ�o�b�t�@�ɒ��ڏ�������
            //���̕����͏\���ɍ����� PC �Ȃ�ʏ�͖ő��Ɏ��s����Ȃ��͂�
            //���t�J�n����A�V�[�N���キ�炢�������s����邱�Ƃ͂Ȃ�
            int nCopy = dwWrite;
            if(nCopy > m_nRequest){
                nCopy = m_nRequest;
            }
            memcpy(m_pRequest, buf, nCopy);
            m_nRequest -= nCopy;
            m_pRequest += nCopy;
            //char sz[256];
            //wsprintf(sz, "output_data::nCopy = %d, dwWrite = %d\n", nCopy, dwWrite);
            //OutputDebugString(sz);
            if(m_nRequest == 0){
                //�c����ǂ݃o�b�t�@�ɏ�������
                m_PreloadBuffer.Write((BYTE*)buf+nCopy, dwWrite-nCopy);
                SetEvent(m_hEventWrite);
                //wsprintf(sz, "output_data::dwWrite-nCopy = %d\n", dwWrite-nCopy);
                //OutputDebugString(sz);
            }
        }
        else{//��ǂ݃o�b�t�@�ɏ�������
            m_PreloadBuffer.Write((BYTE*)buf, dwWrite);
        }
        len -= dwWrite;
        buf += dwWrite;
    }
    if(m_bEOF){
        SetEvent(m_hEventWrite);
    }
    LeaveCriticalSection(&m_cs);
    return 0;
}
int __fastcall KbTimDecoder::acntl(int request, void *arg)
{
    switch(request)
    {
        case PM_REQ_GETQSIZ:
            return -1;
        case PM_REQ_GETFRAGSIZ:
            return -1;
        case PM_REQ_FLUSH:
            return 0;
        case PM_REQ_PLAY_START: /* Called just before playing */
        case PM_REQ_PLAY_END: /* Called just after playing */
    	    return 0;
    }
    return -1;
}
unsigned __stdcall KbTimDecoder::ThreadProc(void* pv)
{
    KbTimDecoder *pDecoder = (KbTimDecoder*)pv;
    return pDecoder->ThreadProc();
}
unsigned __fastcall KbTimDecoder::ThreadProc(void)
{
    MSG msg;
    PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);//���b�Z�[�W�L���[�̍쐬
    SetEvent(m_hEventOpen);
    while(GetMessage(&msg, NULL, 0, 0) > 0){
        switch(msg.message){
            case WM_KBTIM_INIT:{
                //_controlfp(_RC_CHOP, _MCW_RC);
                m_Setting.LoadIniFile(m_szIniFile);
                m_Setting.Apply();			
                kbtim_initialize(m_Setting.GetCfgFileName());	
#ifdef VST_LOADER_ENABLE
				if (hVSTHost == NULL) {
					// ���kbtim.dll�̃f�B���N�g�����猟��
					char szWrapDll[FILEPATH_MAX];
					GetModuleFileName(g_hDll, szWrapDll, FILEPATH_MAX);
					PathRemoveFileSpec(szWrapDll);
#ifdef _WIN64
					strcat(szWrapDll, "\\timvstwrap_x64.dll");
#else
					strcat(szWrapDll, "\\timvstwrap.dll");
#endif
					hVSTHost = LoadLibrary(szWrapDll);
					// �Ȃ��ꍇ LoadLibrary�̌���
					if (hVSTHost == NULL){
#ifdef _WIN64
						hVSTHost = LoadLibrary("timvstwrap_x64.dll");
#else
						hVSTHost = LoadLibrary("timvstwrap.dll");
#endif
					}
					if (hVSTHost != NULL) {
						((vst_open)GetProcAddress(hVSTHost, "vstOpen"))();
					}
				}
#endif
                break;
            }
            case WM_KBTIM_QUIT:{
                kbtim_uninitialize();
#ifdef VST_LOADER_ENABLE
				if (hVSTHost != NULL) {
				// only load , no save
				//	((vst_close)GetProcAddress(hVSTHost,"vstClose"))();
					FreeLibrary(hVSTHost);
					hVSTHost = NULL;
				}
#endif
                PostQuitMessage(0);
                break;
            }
            case WM_KBTIM_PLAY:{
                char szFileName[FILEPATH_MAX];
                kbStrLCpy(szFileName, (const char*)msg.wParam, sizeof(szFileName) / sizeof(szFileName[0]));
                if(m_Setting.IsUpdated())
				{
                    kbtim_uninitialize();
                    m_Setting.LoadIniFile(m_szIniFile);
                    m_Setting.Apply();
                    kbtim_initialize(m_Setting.GetCfgFileName());
                }
                kbtim_play_midi_file(szFileName);//���t�I������܂Ŗ߂��Ă��Ȃ�
                EnterCriticalSection(&m_cs);
                //OutputDebugString("kbtim_play_midi_file::return\n");
                //if(m_bEOF){
                //OutputDebugString("m_bEOF == TRUE\n");
                //}
                m_bEOF = TRUE;
                SetEvent(m_hEventWrite);
                SetEvent(m_hEventOpen);
                LeaveCriticalSection(&m_cs);
                break;
            }
            case WM_KBTIM_STOP:{
                HANDLE hEvent = (HANDLE)msg.wParam;
                SetEvent(hEvent);
                break;
            }
        }
    }
    return 0;
}

KbTimDecoder::KbTimDecoder(void)
{
    static PlayMode pm = {
        DEFAULT_RATE,         //rate
        PE_16BIT | PE_SIGNED, //encoding
        PF_PCM_STREAM,        //flag
        -1,                   //fd: file descriptor for the audio device
        {0,0,0,0,0},          //extra_param[5]: System depended parameters
                              //e.g. buffer fragments, ...
        "kbtim_play_mode",    //id_name
        0,                    //id_character
        "kbtim_play_mode",    //default device or file name
        KbTimDecoder::s_open_output, //0=success, 1=warning, -1=fatal error 
        KbTimDecoder::s_close_output,
        KbTimDecoder::s_output_data,
        KbTimDecoder::s_acntl,
        KbTimDecoder::s_detect
    };
    static ControlMode cm = {
        "kbtim_conrol_mode",//id_name
        0,                  //id_character
        "",                 //id_short_name
        0,                  //verbosity
        0,                  //trace_playing
        0,                  //opened
        CTLF_AUTOSTART,     //flags
        KbTimDecoder::s_ctl_open,
        KbTimDecoder::s_ctl_close,
        KbTimDecoder::s_ctl_pass_playing_list,
        KbTimDecoder::s_ctl_read,
        KbTimDecoder::s_ctl_write,
        KbTimDecoder::s_ctl_cmsg,
        KbTimDecoder::s_ctl_event
    };
    memcpy(&m_pm, &pm, sizeof(PlayMode));
    memcpy(&m_cm, &cm, sizeof(ControlMode));
    play_mode = &m_pm;
    ctl =       &m_cm;
    m_szFileName[0] = 0;
    ZeroMemory(&m_SoundInfo, sizeof(m_SoundInfo));
    m_pRequest = NULL;
    m_nRequest = 0;
    m_bEOF = FALSE;
    m_nStart = 0;
    m_dwSeek = 0xFFFFFFFF;
    {//INI �t�@�C�������擾
     //�v���O�C���ƃt�@�C�����������Ŋg���q�� INI
        char szKpi[FILEPATH_MAX];
        GetModuleFileName(g_hKpi, szKpi, sizeof(szKpi) - (4 + 1));
        kbExtractReplacedFileExt(m_szIniFile, szKpi, ".ini");
    }
    InitializeCriticalSection(&m_cs);
    //�C�x���g�̍쐬
    m_hEventOpen =     CreateEvent(NULL, FALSE, FALSE, NULL);
    m_hEventWrite =    CreateEvent(NULL, FALSE, FALSE, NULL);
    m_hEventCanWrite = CreateEvent(NULL, TRUE,  FALSE, NULL);
    //�X���b�h�̍쐬
    m_dwThreadId = 0;
    m_hThread = (HANDLE)_beginthreadex(NULL,// pointer to security attributes
                                       0,   // initial thread stack size
                                       ThreadProc,
                                       (void*)this,
                                       0,
                                       (unsigned*)&m_dwThreadId);
    //�X���b�h�̃��b�Z�[�W�L���[���쐬�����܂ő҂�
    WaitForSingleObject(m_hEventOpen, INFINITE);
    PostThreadMessage(m_dwThreadId, WM_KBTIM_INIT, 0, 0);//�������I���܂ő҂K�v�͂Ȃ�
    SetThreadPriority(m_hThread, THREAD_PRIORITY_IDLE);
}
KbTimDecoder::~KbTimDecoder(void)
{
    Close();
    SetThreadPriority(m_hThread, THREAD_PRIORITY_NORMAL);
    PostThreadMessage(m_dwThreadId, WM_KBTIM_QUIT, 0, 0);
    WaitForSingleObject(m_hThread, INFINITE);
    CloseHandle(m_hThread);
    CloseHandle(m_hEventOpen);
    CloseHandle(m_hEventWrite);
    CloseHandle(m_hEventCanWrite);
    DeleteCriticalSection(&m_cs);
}
void __fastcall KbTimDecoder::Close(void)
{
    if(!m_szFileName[0]){
        return;
    }
    EnterCriticalSection(&m_cs);
    m_bEOF = TRUE;
    SetEvent(m_hEventCanWrite);
    LeaveCriticalSection(&m_cs);
    HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    PostThreadMessage(m_dwThreadId, WM_KBTIM_STOP, (WPARAM)hEvent, 0);
    WaitForSingleObject(hEvent, INFINITE);
    CloseHandle(hEvent);
    ResetEvent(m_hEventOpen);
    ResetEvent(m_hEventWrite);
    ResetEvent(m_hEventCanWrite);
    m_bEOF = FALSE;
    m_nStart = 0;
    m_dwSeek = 0xFFFFFFFF;
    m_PreloadBuffer.Reset();
    m_szFileName[0] = 0;
    ZeroMemory(&m_SoundInfo, sizeof(m_SoundInfo));
    SetThreadPriority(m_hThread, THREAD_PRIORITY_IDLE);
}
BOOL __fastcall KbTimDecoder::Open(const char *cszFileName, 
                                   SOUNDINFO *pInfo)
{
    if(!pInfo)return FALSE;

    if(m_szFileName[0]){//���łɊJ���Ă���
        ZeroMemory(pInfo, sizeof(SOUNDINFO));
        return FALSE;
    }
    SetThreadPriority(m_hThread, THREAD_PRIORITY_NORMAL);
    //�Đ����g���v��
    //384, 352.8, 256, 192, 176.4, 128, 96, 88.2, 64, 48, 44.1, 32, 22.5 kHz �̂ݗv���ɉ�����
    if(pInfo->dwSamplesPerSec == 384000 ||
       pInfo->dwSamplesPerSec == 352800 ||
       pInfo->dwSamplesPerSec == 256000 ||
       pInfo->dwSamplesPerSec == 192000 ||
       pInfo->dwSamplesPerSec == 176400 ||
       pInfo->dwSamplesPerSec == 128000 ||
       pInfo->dwSamplesPerSec == 96000 ||
       pInfo->dwSamplesPerSec == 88200 ||
       pInfo->dwSamplesPerSec == 64000 ||
       pInfo->dwSamplesPerSec == 48000 ||
       pInfo->dwSamplesPerSec == 44100 ||
       pInfo->dwSamplesPerSec == 32000 ||
       pInfo->dwSamplesPerSec == 24000 ||
       pInfo->dwSamplesPerSec == 22050 ){
        m_pm.rate = pInfo->dwSamplesPerSec;
    }else{
        m_pm.rate = DEFAULT_RATE;
    }
    //�r�b�g���v��
    m_pm.encoding &= ~(PE_16BIT | PE_24BIT | PE_32BIT | PE_F32BIT | PE_F64BIT);
    if(pInfo->dwBitsPerSample == -64){
        m_pm.encoding |= PE_F64BIT;
    }
    else if(pInfo->dwBitsPerSample == -32){
        m_pm.encoding |= PE_F32BIT;
    }
    else if(pInfo->dwBitsPerSample >= 32){
        m_pm.encoding |= PE_32BIT;
    }
    else if(pInfo->dwBitsPerSample == 24){
        m_pm.encoding |= PE_24BIT;
    }
    else{
        m_pm.encoding |= PE_16BIT;
    }
    PostThreadMessage(m_dwThreadId, WM_KBTIM_PLAY, (WPARAM)cszFileName, 0);
    WaitForSingleObject(m_hEventOpen, INFINITE);
    kbStrLCpy(m_szFileName, cszFileName, sizeof(m_szFileName) / sizeof(m_szFileName[0]));
    if(!m_SoundInfo.dwUnitRender){
        Close();
    }
    memcpy(pInfo, &m_SoundInfo, sizeof(SOUNDINFO));
    return m_SoundInfo.dwUnitRender != 0;
}
DWORD __fastcall KbTimDecoder::Render(BYTE *pBuffer, DWORD dwSize)
{
    DWORD dwRet;
    EnterCriticalSection(&m_cs);
    if(!m_nStart && !m_bEOF){
        //�t�@�C���I�[�v������ł܂� output_data() ���Ă΂�Ă��Ȃ�
        //output_data ���Ă΂��܂ő҂�
        //OutputDebugString("!m_bStart\n");
        SetEvent(m_hEventCanWrite);
        LeaveCriticalSection(&m_cs);
        WaitForSingleObject(m_hEventOpen, INFINITE);
        EnterCriticalSection(&m_cs);
    }
    if(m_nStart == 1 && !m_bEOF){
        SetEvent(m_hEventCanWrite);
        while(m_PreloadBuffer.GetRemain() > 0){
        //���� Render ���̂݁A��ǂ݃o�b�t�@�����S�ăf�R�[�h�����܂ő҂�
            if(!m_bEOF){
                LeaveCriticalSection(&m_cs);
                Sleep(1);
                EnterCriticalSection(&m_cs);
            }
            else{
                break;
            }
        }
        m_nStart = 2;
    }
    dwRet = m_PreloadBuffer.Read(pBuffer, dwSize);    
    SetEvent(m_hEventCanWrite);
    if(dwRet < dwSize && !m_bEOF){
        //��ǂ݂����ł͊Ԃɍ���Ȃ��ꍇ
        //��ǂ݃o�b�t�@���o�R�����ɒ��ڏ�������
        m_pRequest = pBuffer + dwRet;
        m_nRequest = dwSize - dwRet;
        //char sz[256];
        //wsprintf(sz, "Render::dwRet = %d, g_nRequest = %d\n", dwRet, m_nRequest);
        //OutputDebugString(sz);
        SetEvent(m_hEventCanWrite);
        LeaveCriticalSection(&m_cs);
        WaitForSingleObject(m_hEventWrite, INFINITE);
        EnterCriticalSection(&m_cs);
        dwRet = m_pRequest-pBuffer;
        m_pRequest = NULL;
        m_nRequest = 0;
        //wsprintf(sz, "Render::dwRet = %d\n", dwRet);
        //OutputDebugString(sz);
    }
    //if(dwRet < dwSize){
    //    char sz[256];
    //    wsprintf(sz, "Render::dwRet < dwSize, dwRet = %d, dwSize = %d, m_bEOF = %d\n", 
    //                    dwRet, dwSize, m_bEOF);
    //    OutputDebugString(sz);
    //}
    LeaveCriticalSection(&m_cs);
    return dwRet;
}
DWORD __fastcall KbTimDecoder::InternalSetPosition(DWORD dwPos)
{
    if(!m_szFileName[0]){//�t�@�C�����J���Ă��Ȃ�
        return 0;
    }
    if(dwPos == 0xFFFFFFFF){
        dwPos = 0;
    }
    if(dwPos == 0 && !m_nStart){
        //�擪�ւ̃V�[�N�ł܂� output_data() ���Ă΂�Ă��Ȃ�
        return 0;
    }
    DWORD dwRet = 0;
    EnterCriticalSection(&m_cs);
    if(!m_nStart && !m_bEOF){
        //�t�@�C���I�[�v������ł܂� output_data() ���Ă΂�Ă��Ȃ�
        //���̒i�K�ł́Actl_read �� RC_JUMP ��Ԃ��Ă���������Ă��܂����Ƃ�
        //����̂ŁAoutput_data ���Ă΂��܂ő҂�
        SetEvent(m_hEventCanWrite);
        LeaveCriticalSection(&m_cs);
        WaitForSingleObject(m_hEventOpen, INFINITE);
        EnterCriticalSection(&m_cs);
    }
    if(m_bEOF){//���łɉ��t�I�����Ă���
        LeaveCriticalSection(&m_cs);
    }
    else{
        m_dwSeek = dwPos;
        SetEvent(m_hEventCanWrite);
        LeaveCriticalSection(&m_cs);
        WaitForSingleObject(m_hEventOpen, INFINITE);
        dwRet = dwPos;
    }
    return dwRet;
}
DWORD __fastcall KbTimDecoder::SetPosition(DWORD dwPos)
{
    DWORD dwRet = InternalSetPosition(dwPos);
    if(m_bEOF){
        //���łɉ��t�I�����Ă���ꍇ�̓t�@�C�����J������
        //OutputDebugString("SetPosition::m_bEOF == TRUE\n");
        SOUNDINFO info;
        char szFileName[sizeof(m_szFileName)];
        kbStrLCpy(szFileName, m_szFileName, sizeof(szFileName) / sizeof(szFileName[0]));
        memcpy(&info, &m_SoundInfo, sizeof(info));
        Close();
        Open(szFileName, &info);
        dwRet = InternalSetPosition(dwPos);
    }
    return dwRet;
}
static void WINAPI kmp_Init(void)
{
    if(!KbTimDecoder::g_pDecoder){
#ifdef KBTIM_LEAK_CHECK
        _CrtSetDbgFlag(CRTDEBUGFLAGS);
#endif
        kbStr_Initialize();
		KbTimDecoder::g_pDecoder = new KbTimDecoder;
    }
}
static void WINAPI kmp_Deinit(void)
{
    if(KbTimDecoder::g_pDecoder){
        delete KbTimDecoder::g_pDecoder;
        KbTimDecoder::g_pDecoder = NULL;
    }
    g_hKpi = NULL;
}
static HKMP WINAPI kmp_Open(const char *cszFileName, SOUNDINFO *pInfo)
{//���ɍĐ����ɌĂ΂ꂽ�� NULL ��Ԃ�
    if(InterlockedIncrement(&g_lCanUse) == 0){
        if(KbTimDecoder::g_pDecoder->Open(cszFileName, pInfo)){
            return (HKMP)KbTimDecoder::g_pDecoder;
        }
    }
    InterlockedDecrement(&g_lCanUse);
    return NULL;
}

static void WINAPI kmp_Close(HKMP hKMP)
{
    KbTimDecoder *tim = (KbTimDecoder*)hKMP;
    if(tim){
        tim->Close();
        InterlockedDecrement(&g_lCanUse);
    }
}

static DWORD WINAPI kmp_Render(HKMP hKMP, BYTE* Buffer, DWORD dwSize)
{
    KbTimDecoder *tim = (KbTimDecoder*)hKMP;
    if(tim){
        return tim->Render(Buffer, dwSize);
    }
    return 0;
}

static DWORD WINAPI kmp_SetPosition(HKMP hKMP, DWORD dwPos)
{
    KbTimDecoder *tim = (KbTimDecoder*)hKMP;
    if(tim){
        return tim->SetPosition(dwPos);
    }
    return 0;
}

static void WINAPI kmp_NullInit(void)
{
}
static void WINAPI kmp_NullDeinit(void)
{
}
static HKMP WINAPI kmp_NullOpen(const char *cszFileName, SOUNDINFO *pInfo)
{
    return NULL;
}
static void WINAPI kmp_NullClose(HKMP hKMP)
{
}
static DWORD WINAPI kmp_NullRender(HKMP hKMP, BYTE* Buffer, DWORD dwSize)
{
    return 0;
}
static DWORD WINAPI kmp_NullSetPosition(HKMP hKMP, DWORD dwPos)
{
    return 0;         
}

BOOL WINAPI DllMain(HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call){
        case DLL_PROCESS_ATTACH:{
            InitializeCriticalSection(&g_cs);
#ifdef KBTIM_LEAK_CHECK
            //���������[�N���o
            int tmpDbgFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
            //tmpDbgFlag |= _CRTDBG_DELAY_FREE_MEM_DF;
            tmpDbgFlag |= _CRTDBG_LEAK_CHECK_DF;
            //tmpDbgFlag |= _CRTDBG_CHECK_ALWAYS_DF;
            _CrtSetDbgFlag(tmpDbgFlag);
#endif
			g_hDll = (HINSTANCE)hModule;
            DisableThreadLibraryCalls((HMODULE)hModule);
            break;
        }
        case DLL_PROCESS_DETACH:{
            DeleteCriticalSection(&g_cs);
            break;
        }
    }
    return TRUE;
}
static KMPMODULE g_Module = 
{
    KMPMODULE_VERSION,
    KMPMODULE_PLUGIN_VERSION,
    g_cszKbTimCopyright, 
    g_cszKbTimDescription,
    g_cppszSupportExts,
    0, //not reentrant
    kmp_Init,
    kmp_Deinit,
    kmp_Open,
    NULL,
    kmp_Close,
    kmp_Render,
    kmp_SetPosition
};
static KMPMODULE g_NullModule = 
{//�����I�Ɏg�p�s�\
 //2��ڈȍ~�� kbtim_GetModule ���Ă΂ꂽ��Ԃ�
    KMPMODULE_VERSION,
    KMPMODULE_PLUGIN_VERSION,
    g_cszKbTimCopyright,
    g_cszKbTimDescription,
    g_cppszSupportExts,
    0xFFFFFFFF,
    kmp_NullInit,
    kmp_NullDeinit,
    kmp_NullOpen,
    NULL,
    kmp_NullClose,
    kmp_NullRender,
    kmp_NullSetPosition
};
////////////////////////////////////////////////////////////////////////
//kbtim.dll �͂P�� kbtim(_xxx).kpi ���炵�����p�ł��Ȃ�
//1��ڂ� KMPMODULE::dwReentrant == 0 �ŗ��p�\�� KMPMODULE(g_Module) ��Ԃ�
//2��ڈȍ~�� KMPMODULE::dwReentrant == 0xFFFFFFFF �ɃZ�b�g���Ď����I�ɗ��p�s�\�� 
//KMPMODULE(g_NullModule) ��Ԃ��AKbMedia Player ����͏�� kbrunkpi.exe �o�R�ŗ��p���邱�Ƃ𑣂�
KMPMODULE* WINAPI kbtim_GetModule(HINSTANCE hInstance, DWORD dwVersion)
{
    if((dwVersion == KBTIM_GETMODULE_VERSION) && hInstance){
        EnterCriticalSection(&g_cs);
        KMPMODULE *pRet;
        if(!g_hKpi){
            g_hKpi = hInstance;
        }
        if(g_hKpi == hInstance){//���߂ČĂ΂ꂽ���A���� kpi �ɂ���ČĂ΂ꂽ�ꍇ
            pRet = &g_Module;
        }
        else{
            pRet = &g_NullModule;
        }
        LeaveCriticalSection(&g_cs);
        return pRet;
    }
    return NULL;
}
