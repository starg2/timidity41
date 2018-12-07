#include <windows.h>
#include <stdio.h>
#include <process.h>

//#pragma comment( lib, "winmm" )

#ifdef _DEBUG
//#define KBTIM_LEAK_CHECK //メモリリーク検出（しない場合はコメントアウト）
#define KMPMODULE_PLUGIN_VERSION 0x7FFFFFFF
#else
#define KMPMODULE_PLUGIN_VERSION 16
#endif

#ifdef KBTIM_LEAK_CHECK
#include <crtdbg.h>
#endif


extern "C"{

#include "config.h"
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

}

#include "kmp_pi.h"
#include "ringbuf.h"
#include "kbtim_setting.h"
#include "kbtim.h"
#include "kbtim_common.h"

//定数
enum{
    WM_KBTIM_INIT = WM_USER, //初期化
    WM_KBTIM_QUIT,           //後始末
    WM_KBTIM_PLAY,           //演奏開始：wParam = (char*) szFileName
                             //（演奏が終わるまで戻らない）
    WM_KBTIM_STOP,           //演奏停止：wParam = (HANDLE) hEvent
                             //（呼ばれたら hEvent をシグナル状態にセット）
};

static CRITICAL_SECTION g_cs;
static HINSTANCE  g_hKpi = NULL; //kbtim.kpi のインスタンスハンドル
static const int UNIT_SAMPLE = 512;
static const int PRELOAD_MS = 300;
    //先読みバッファのサイズ（ミリ秒）
    //値が大きいほど、
    //・音とびしにくくなる
    //・メモリ使用量が大きくなる
    //・演奏開始直後、シーク直後の動作が重くなる
    //KbMedia Player 本体が先読み処理を行っているので、値を大きくしてもメリットは少ない

KbTimDecoder* KbTimDecoder::g_pDecoder = NULL;

void __fastcall KbTimDecoder::ctl_event(CtlEvent *e)
{
    if (e->type == CTLE_PLAY_START) {
        EnterCriticalSection(&m_cs);
        DWORD dwBps;   //ビット数
        DWORD dwCh = 2;//チャンネル（ステレオ）
        DWORD dwBytesPerSample;//1 サンプルあたりのバイト数
        DWORD dwPreloadBytes;  //先読みバッファのサイズ（バイト数）
        m_SoundInfo.dwLength = MulDiv(e->v1, 1000, m_pm.rate);//曲の長さ
        //if ((m_pm.encoding & PE_32BIT)) {//32bit(未対応)
        //    dwBps = 32;
        //}
        //else if ((m_pm.encoding & PE_24BIT)) {//24bit
        if ((m_pm.encoding & PE_24BIT)) {//24bit
            dwBps = 24;
        }
        else {//16bit
            dwBps = 16;
        }
        dwBytesPerSample = (dwBps/8) *dwCh;
        m_SoundInfo.dwUnitRender = UNIT_SAMPLE*dwBytesPerSample;
        m_SoundInfo.dwSamplesPerSec = m_pm.rate;
        m_SoundInfo.dwBitsPerSample = dwBps;
        m_SoundInfo.dwChannels = dwCh;
        m_SoundInfo.dwSeekable = 1;
        //先読みバッファのサイズを設定
        //ある程度の大きさは確保しておく必要がある
        dwPreloadBytes = ((m_pm.rate * PRELOAD_MS) / 1000) * dwBytesPerSample;
        if (dwPreloadBytes < m_SoundInfo.dwUnitRender*2) {
            //少なくとも Render １回分は先読みする
            dwPreloadBytes = m_SoundInfo.dwUnitRender*2;
        }
        while (dwPreloadBytes % dwBytesPerSample) {
            dwPreloadBytes++;//dwBytesPerSample の倍数にする
        }
        m_PreloadBuffer.SetSize(dwPreloadBytes);
        SetEvent(m_hEventOpen);
        LeaveCriticalSection(&m_cs);
        //デコード要求があるまで待つ
        WaitForSingleObject(m_hEventCanWrite, INFINITE);
    }
}
int __fastcall KbTimDecoder::ctl_read(ptr_size_t *valp)
{
    if (m_bEOF) {
        return RC_QUIT;
    }
    else if (m_dwSeek != 0xFFFFFFFF) {
        //シーク
        //まだ output_data() が呼ばれていない状態では、
        //ここで RC_JUMP を返しても無視されてしまう
        //output_data() が呼ばれた後であれば RC_JUMP が処理される
        //if (kbtim_config_updated()) {
        if (m_Setting.IsUpdated()) {
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
    //kbtim_config.h の #define cmsg(X) をコメントアウトしないと
    //この関数が呼ばれることはない
    char szMessage[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(szMessage, sizeof(szMessage) -2, fmt, ap);
    va_end(ap);
    szMessage[sizeof(szMessage) -2] = 0;
    strcat(szMessage, "\n");
    OutputDebugString(szMessage);
#endif
    return 0;
}
int __fastcall KbTimDecoder::open_output(void)
{
    m_pm.encoding &= ~(PE_ULAW | PE_ALAW | PE_BYTESWAP);
    if ((m_pm.encoding & PE_16BIT) ||
       (m_pm.encoding & PE_24BIT) ||
     //(m_pm.encoding & PE_32BIT) ||
       0 )
    {
        m_pm.encoding |= PE_SIGNED;
    }
    else {
        m_pm.encoding &= ~PE_SIGNED;
    }
    m_pm.fd = 0;
    return 0;
}
void __fastcall KbTimDecoder::close_output(void)
{
    m_pm.fd = -1;
}
int __fastcall KbTimDecoder::output_data(const uint8 *buf, int32 len)
{
    EnterCriticalSection(&m_cs);
    if (!m_nStart) {
        //ファイルオープン後、output_data が１度でも呼ばれていれば、
        //ctl_read で RC_JUMP を返したときに確実にシークが処理される
        m_nStart = TRUE;
        ResetEvent(m_hEventCanWrite);
        SetEvent(m_hEventOpen);
        LeaveCriticalSection(&m_cs);
        WaitForSingleObject(m_hEventCanWrite, INFINITE);
        EnterCriticalSection(&m_cs);
    }
    while (!m_bEOF && (m_dwSeek == 0xFFFFFFFF) && len > 0) {
        DWORD dwRemain;//先読みバッファの空きサイズ
        DWORD dwWrite; //先読みバッファへの書き込みサイズ
        while ((dwRemain = m_PreloadBuffer.GetRemain()) == 0) {
            //バッファに空きがない
            ResetEvent(m_hEventCanWrite);
            LeaveCriticalSection(&m_cs);
            //char sz[256];
            //DWORD dwWaitStart = timeGetTime();
            //DWORD dwElapsed;
            //バッファが空くまで待つ
            WaitForSingleObject(m_hEventCanWrite, INFINITE);
            /*dwElapsed = timeGetTime() -dwWaitStart;
            if (dwElapsed > 200) {
                wsprintf(sz, "output_data::WaitForSingleObject, waittime=%d\n", dwElapsed);
                OutputDebugString(sz);
            }*/
            EnterCriticalSection(&m_cs);
            /*dwRemain = m_PreloadBuffer.GetRemain();
            if (dwRemain == 0 ||
               dwRemain > UNIT_SAMPLE*(m_SoundInfo.dwBitsPerSample/8) *m_SoundInfo.dwChannels) {
                wsprintf(sz, "output_data::dwRemain=%d\n", dwRemain);
                OutputDebugString(sz);
            }*/
            if (m_bEOF) {//演奏終了
                SetEvent(m_hEventWrite);
                LeaveCriticalSection(&m_cs);
                return 0;
            }
            else if (m_dwSeek != 0xFFFFFFFF) {//シーク要求
                LeaveCriticalSection(&m_cs);
                return 0;
            }
        }
        if ((DWORD) len > dwRemain) {
            dwWrite = dwRemain;
        }
        else {
            dwWrite = len;
        }
        if (m_nRequest) {
            //先読みが間に合わない場合
            //先読みバッファ経由ではなく、Render 関数で要求されたバッファに直接書き込む
            //この部分は十分に高速な PC なら通常は滅多に実行されないはず
            //演奏開始直後、シーク直後くらいしか実行されることはない
            int nCopy = dwWrite;
            if (nCopy > m_nRequest) {
                nCopy = m_nRequest;
            }
            memcpy(m_pRequest, buf, nCopy);
            m_nRequest -= nCopy;
            m_pRequest += nCopy;
            //char sz[256];
            //wsprintf(sz, "output_data::nCopy = %d, dwWrite = %d\n", nCopy, dwWrite);
            //OutputDebugString(sz);
            if (m_nRequest == 0) {
                //残りを先読みバッファに書き込む
                m_PreloadBuffer.Write((BYTE*) buf+nCopy, dwWrite-nCopy);
                SetEvent(m_hEventWrite);
                //wsprintf(sz, "output_data::dwWrite-nCopy = %d\n", dwWrite-nCopy);
                //OutputDebugString(sz);
            }
        }
        else {//先読みバッファに書き込む
            m_PreloadBuffer.Write((BYTE*) buf, dwWrite);
        }
        len -= dwWrite;
        buf += dwWrite;
    }
    if (m_bEOF) {
        SetEvent(m_hEventWrite);
    }
    LeaveCriticalSection(&m_cs);
    return 0;
}
int __fastcall KbTimDecoder::acntl(int request, void *arg)
{
    switch (request) {
        case PM_REQ_GETQSIZ:{
            int narg = UNIT_SAMPLE;
            if (!(m_pm.encoding & PE_MONO)) {
                narg *= 2;
            }
            if (m_pm.encoding & PE_24BIT) {
                narg *= 3;
            }
            else if (m_pm.encoding & PE_16BIT) {
                narg *= 2;
            }
            *(int*) arg = narg;
            return 0;
        }
        case PM_REQ_DISCARD:{
            return 0;
        }
        case PM_REQ_FLUSH:{
            return 0;
        }
    }
    return -1;
}

unsigned __stdcall KbTimDecoder::ThreadProc(void* pv)
{
    KbTimDecoder *pDecoder = (KbTimDecoder*) pv;
    return pDecoder->ThreadProc();
}
unsigned __fastcall KbTimDecoder::ThreadProc(void)
{
    MSG msg;
    PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);//メッセージキューの作成
    SetEvent(m_hEventOpen);
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        switch (msg.message) {
            case WM_KBTIM_INIT:{
                m_Setting.LoadIniFile(m_szIniFile);
                m_Setting.Apply();
                kbtim_initialize(m_Setting.GetCfgFileName());
                break;
            }
            case WM_KBTIM_QUIT:{
                kbtim_uninitialize();
                PostQuitMessage(0);
                break;
            }
            case WM_KBTIM_PLAY:{
                char szFileName[MAX_PATH*2];
                lstrcpyn(szFileName, (char*) msg.wParam, sizeof(szFileName));
                if (m_Setting.IsUpdated()) {
                    kbtim_uninitialize();
                    m_Setting.LoadIniFile(m_szIniFile);
                    m_Setting.Apply();
                    kbtim_initialize(m_Setting.GetCfgFileName());
                }
                kbtim_play_midi_file(szFileName);//演奏終了するまで戻ってこない
                EnterCriticalSection(&m_cs);
                //OutputDebugString("kbtim_play_midi_file::return\n");
                //if (m_bEOF) {
                //OutputDebugString("m_bEOF == TRUE\n");
                //}
                m_bEOF = TRUE;
                SetEvent(m_hEventWrite);
                SetEvent(m_hEventOpen);
                LeaveCriticalSection(&m_cs);
                break;
            }
            case WM_KBTIM_STOP:{
                HANDLE hEvent = (HANDLE) msg.wParam;
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
        KbTimDecoder::s_open_output,          //0=success, 1=warning, -1=fatal error
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
    m_nStart = FALSE;
    m_dwSeek = 0xFFFFFFFF;
    {//INI ファイル名を取得
     //プラグインとファイル名が同じで拡張子が INI
        GetModuleFileName(g_hKpi, m_szIniFile, sizeof(m_szIniFile));
        char *p = m_szIniFile;
        char *ext = NULL;
        while (*p) {
            if (IsDBCSLeadByte((BYTE) *p)) {
                p += 2;
                continue;
            }
            if (*p == '.') {
                ext = p;
            }
            else if (*p == '\\') {
                ext = NULL;
            }
            p++;
        }
        if (ext) {
            strcpy(ext, ".ini");
        }
        else {
            strcat(m_szIniFile, ".ini");
        }
    }
    InitializeCriticalSection(&m_cs);
    //イベントの作成
    m_hEventOpen =     CreateEvent(NULL, FALSE, FALSE, NULL);
    m_hEventWrite =    CreateEvent(NULL, FALSE, FALSE, NULL);
    m_hEventCanWrite = CreateEvent(NULL, TRUE,  FALSE, NULL);
    //スレッドの作成
    m_dwThreadId = 0;
    m_hThread = (HANDLE) _beginthreadex(NULL,// pointer to security attributes
                                       0,   // initial thread stack size
                                       ThreadProc,
                                       (void*) this,
                                       0,
                                       (unsigned*) &m_dwThreadId);
    //スレッドのメッセージキューが作成されるまで待つ
    WaitForSingleObject(m_hEventOpen, INFINITE);
    PostThreadMessage(m_dwThreadId, WM_KBTIM_INIT, 0, 0);//処理が終わるまで待つ必要はない
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
    if (!m_szFileName[0]) {
        return;
    }
    EnterCriticalSection(&m_cs);
    m_bEOF = TRUE;
    SetEvent(m_hEventCanWrite);
    LeaveCriticalSection(&m_cs);
    HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    PostThreadMessage(m_dwThreadId, WM_KBTIM_STOP, (WPARAM) hEvent, 0);
    WaitForSingleObject(hEvent, INFINITE);
    CloseHandle(hEvent);
    ResetEvent(m_hEventOpen);
    ResetEvent(m_hEventWrite);
    ResetEvent(m_hEventCanWrite);
    m_bEOF = FALSE;
    m_nStart = FALSE;
    m_dwSeek = 0xFFFFFFFF;
    m_PreloadBuffer.Reset();
    m_szFileName[0] = 0;
    ZeroMemory(&m_SoundInfo, sizeof(m_SoundInfo));
    SetThreadPriority(m_hThread, THREAD_PRIORITY_IDLE);
}
BOOL __fastcall KbTimDecoder::Open(const char *cszFileName,
                                   SOUNDINFO *pInfo)
{
    if (!pInfo) return FALSE;
    if (m_szFileName[0]) {//すでに開いている
        ZeroMemory(pInfo, sizeof(SOUNDINFO));
        return FALSE;
    }
    SetThreadPriority(m_hThread, THREAD_PRIORITY_NORMAL);
    //再生周波数要求
    //48000, 44100, 22050 Hz のみ要求に応じる
    if (pInfo->dwSamplesPerSec == 48000 ||
       pInfo->dwSamplesPerSec == 44100 ||
       pInfo->dwSamplesPerSec == 22050 ) {
        m_pm.rate = pInfo->dwSamplesPerSec;
    }
    else {
        m_pm.rate = DEFAULT_RATE;
    }
    //ビット数要求
    //32bit float, 32bit int の場合は 24bit で応じる
    //m_pm.encoding &= ~(PE_16BIT | PE_24BIT | PE_32BIT);
    m_pm.encoding &= ~(PE_16BIT | PE_24BIT);
    if (abs((int) pInfo->dwBitsPerSample) >= 32) {
        //m_pm.encoding |= PE_32BIT;
        m_pm.encoding |= PE_24BIT;
    }
    else if (abs((int) pInfo->dwBitsPerSample) >= 24) {
        m_pm.encoding |= PE_24BIT;
    }
    else {
        m_pm.encoding |= PE_16BIT;
    }
    PostThreadMessage(m_dwThreadId, WM_KBTIM_PLAY, (WPARAM) cszFileName, 0);
    WaitForSingleObject(m_hEventOpen, INFINITE);
    lstrcpyn(m_szFileName, cszFileName, sizeof(m_szFileName));
    if (!m_SoundInfo.dwUnitRender) {
        Close();
    }
    memcpy(pInfo, &m_SoundInfo, sizeof(SOUNDINFO));
    return m_SoundInfo.dwUnitRender != 0;
}
DWORD __fastcall KbTimDecoder::Render(BYTE *pBuffer, DWORD dwSize)
{
    DWORD dwRet;
    EnterCriticalSection(&m_cs);
    if (!m_nStart && !m_bEOF) {
        //ファイルオープン直後でまだ output_data() が呼ばれていない
        //output_data が呼ばれるまで待つ
        //OutputDebugString("!m_nStart\n");
        SetEvent(m_hEventCanWrite);
        LeaveCriticalSection(&m_cs);
        WaitForSingleObject(m_hEventOpen, INFINITE);
        EnterCriticalSection(&m_cs);
    }
    dwRet= m_PreloadBuffer.Read(pBuffer, dwSize);
    SetEvent(m_hEventCanWrite);
    if (dwRet < dwSize && !m_bEOF) {
        //先読みだけでは間に合わない場合
        //先読みバッファを経由せずに直接書き込む
        m_pRequest = pBuffer + dwRet;
        m_nRequest = dwSize - dwRet;
        //char sz[256];
        //wsprintf(sz, "Render::dwRet = %d, g_nRequest = %d\n", dwRet, m_nRequest);
        //OutputDebugString(sz);
        LeaveCriticalSection(&m_cs);
        WaitForSingleObject(m_hEventWrite, INFINITE);
        EnterCriticalSection(&m_cs);
        dwRet = m_pRequest-pBuffer;
        m_pRequest = NULL;
        m_nRequest = 0;
        //wsprintf(sz, "Render::dwRet = %d\n", dwRet);
        //OutputDebugString(sz);
    }
    //if (dwRet < dwSize) {
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
    if (!m_szFileName[0]) {//ファイルを開いていない
        return 0;
    }
    if (dwPos == 0xFFFFFFFF) {
        dwPos = 0;
    }
    if (dwPos == 0 && !m_nStart) {
        //先頭へのシークでまだ output_data() が呼ばれていない
        return 0;
    }
    DWORD dwRet = 0;
    EnterCriticalSection(&m_cs);
    if (!m_nStart && !m_bEOF) {
        //ファイルオープン直後でまだ output_data() が呼ばれていない
        //この段階では、ctl_read で RC_JUMP を返しても無視されてしまうことが
        //あるので、output_data が呼ばれるまで待つ
        SetEvent(m_hEventCanWrite);
        LeaveCriticalSection(&m_cs);
        WaitForSingleObject(m_hEventOpen, INFINITE);
        EnterCriticalSection(&m_cs);
    }
    if (m_bEOF) {//すでに演奏終了している
        LeaveCriticalSection(&m_cs);
    }
    else {
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
    if (m_bEOF) {
        //すでに演奏終了している場合はファイルを開き直す
        //OutputDebugString("SetPosition::m_bEOF == TRUE\n");
        SOUNDINFO info;
        char szFileName[sizeof(m_szFileName)];
        strcpy(szFileName, m_szFileName);
        memcpy(&info, &m_SoundInfo, sizeof(info));
        Close();
        Open(szFileName, &info);
        dwRet = InternalSetPosition(dwPos);
    }
    return dwRet;
}
static void WINAPI kmp_Init(void)
{
    if (!KbTimDecoder::g_pDecoder) {
        KbTimDecoder::g_pDecoder = new KbTimDecoder;
    }
}
static void WINAPI kmp_Deinit(void)
{
    if (KbTimDecoder::g_pDecoder) {
        delete KbTimDecoder::g_pDecoder;
        KbTimDecoder::g_pDecoder = NULL;
    }
    g_hKpi = NULL;
}
static HKMP WINAPI kmp_Open(const char *cszFileName, SOUNDINFO *pInfo)
{
    if (KbTimDecoder::g_pDecoder->Open(cszFileName, pInfo)) {
        return (HKMP) KbTimDecoder::g_pDecoder;
    }
    return NULL;
}

static void WINAPI kmp_Close(HKMP hKMP)
{
    KbTimDecoder *tim = (KbTimDecoder*) hKMP;
    if (tim) {
        tim->Close();
    }
}

static DWORD WINAPI kmp_Render(HKMP hKMP, BYTE* Buffer, DWORD dwSize)
{
    KbTimDecoder *tim = (KbTimDecoder*) hKMP;
    if (tim) {
        return tim->Render(Buffer, dwSize);
    }
    return 0;
}

static DWORD WINAPI kmp_SetPosition(HKMP hKMP, DWORD dwPos)
{
    KbTimDecoder *tim = (KbTimDecoder*) hKMP;
    if (tim) {
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
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:{
            InitializeCriticalSection(&g_cs);
            DisableThreadLibraryCalls((HMODULE) hModule);
            break;
        }
        case DLL_PROCESS_DETACH:{
            DeleteCriticalSection(&g_cs);
            break;
        }
    }
    return TRUE;
}

extern "C" KMPMODULE* WINAPI kbtim_GetModule(HINSTANCE hInstance, DWORD dwVersion)
{
    //return NULL;
#if 1
    if ((dwVersion == KBTIM_GETMODULE_VERSION) && hInstance) {
            InitializeCriticalSection(&g_cs);
        EnterCriticalSection(&g_cs);
        static const char *ppszSupportExts[] = {".mid", NULL};
        static KMPMODULE mod1 =
        {
            KMPMODULE_VERSION,
            KMPMODULE_PLUGIN_VERSION,
            "Copyright (C) 2004-2005 Tuukka Toivonen, Masanao Izumo, Saito, Kobarin, etc...",
            "TiMidity++ MIDI Decoder Plugin for KbMedia Player",
            ppszSupportExts,
            0, //not reentrant
            kmp_Init,
            kmp_Deinit,
            kmp_Open,
            NULL,
            kmp_Close,
            kmp_Render,
            kmp_SetPosition
        };
        static KMPMODULE mod2 =
        {
            mod1.dwVersion,
            mod1.dwPluginVersion,
            mod1.pszCopyright,
            mod1.pszDescription,
            mod1.ppszSupportExts,
            0xFFFFFFFF,
            kmp_NullInit,
            kmp_NullDeinit,
            kmp_NullOpen,
            NULL,
            kmp_NullClose,
            kmp_NullRender,
            kmp_NullSetPosition
        };
        KMPMODULE *pRet;
        if (!g_hKpi) {
            g_hKpi = hInstance;
            pRet = &mod1;
        }
        else {
            pRet = &mod2;
        }
        LeaveCriticalSection(&g_cs);
        return pRet;
    }
#ifdef KBTIM_LEAK_CHECK
    //メモリリーク検出
    static int t = 0;
    if (!t) {
        t = 1;
        int tmpDbgFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
        //tmpDbgFlag |= _CRTDBG_DELAY_FREE_MEM_DF;
        tmpDbgFlag |= _CRTDBG_LEAK_CHECK_DF;
        //tmpDbgFlag |= _CRTDBG_CHECK_ALWAYS_DF;
        _CrtSetDbgFlag(tmpDbgFlag);
    }
#endif
    return NULL;

#endif
}
