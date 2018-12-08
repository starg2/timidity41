#ifndef KBTIM_H
#define KBTIM_H

class KbTimDecoder
{
private:
    PlayMode    m_pm;
    ControlMode m_cm;

    CRITICAL_SECTION m_cs;
    HANDLE m_hThread;    //timidity �̊֐����Ăяo���X���b�h
    DWORD  m_dwThreadId; //���̃X���b�h ID
    HANDLE m_hEventOpen; //�X���b�h�����������҂��p
                         //�t�@�C���I�[�v���҂��p
                         //�V�[�N�����҂��p
                         //���� output_data() �Ăяo���҂��p
    HANDLE m_hEventWrite;    //�K�v�ȃo�C�g�������f�R�[�h������V�O�i�����
                             //���t�I�������ꍇ���V�O�i�����
    HANDLE m_hEventCanWrite; //�f�R�[�h�v������������V�O�i�����
                             //���t�I�������ꍇ���V�O�i�����

    KbRingBuffer m_PreloadBuffer;//��ǂ݃f�R�[�h�o�b�t�@
    BYTE* m_pRequest;    //��ǂ݂��Ԃɍ���Ȃ��ꍇ�͂��̃o�b�t�@�ɒ��ڏ�������
    int   m_nRequest;    //m_pRequest �̃o�b�t�@�T�C�Y
///r
    DWORD cdwPreloadMS; // ��ǂݎ��� (�����O�o�b�t�@

    BOOL  m_bEOF;  //���t�I�������� TRUE
                   //�I���v�����������ꍇ�� TRUE
    int   m_nStart;//output_data() ���Ă΂ꂽ�� 1, ����� Render ���Ă΂ꂽ�� 2
    DWORD m_dwSeek;//�V�[�N(ms)�i�V�[�N�v������Ă��Ȃ��ꍇ�� 0xFFFFFFFF�j

    KbTimSetting m_Setting;
    char m_szIniFile[FILEPATH_MAX];

    SOUNDINFO m_SoundInfo;
    char m_szFileName[FILEPATH_MAX];
//
//ControlMode/static
    static int  s_ctl_open(int using_stdin, int using_stdout) {
        return g_pDecoder->ctl_open(using_stdin,using_stdout);
    }
    static void s_ctl_close(void) {
        g_pDecoder->ctl_close();
    }
    static int s_ctl_pass_playing_list(int number_of_files, char *list_of_files[]) {
        return 0;
        //return g_pDecoder->ctl_pass_playing_list(number_of_files,list_of_files);
    }
    static void s_ctl_event(CtlEvent *e) {
        g_pDecoder->ctl_event(e);
    }
    static int  s_ctl_read(ptr_size_t *valp) {
        return g_pDecoder->ctl_read(valp);
    }
    static int  s_ctl_write(const uint8 *buf, int32 size) { return 0; }
    static int  s_ctl_cmsg(int type, int verbosity_level, const char *fmt, ...) {
#if 0
        int ret;
        va_list ap;
        va_start(ap, fmt);
        ret = g_pDecoder->ctl_cmsg(type, verbosity_level, fmt, ap);
        va_end(ap);
        return ret;
#else
        return 0;
#endif
    }
//ControlMode
    int  __fastcall ctl_open(int using_stdin, int using_stdout) { return 0; }
    void __fastcall ctl_close(void) {}
    int  __fastcall ctl_pass_playing_list(int number_of_files, char *list_of_files[]) { return 0; }
    void __fastcall ctl_event(CtlEvent *e);
    int  __fastcall ctl_read(ptr_size_t *valp);
    int32 __fastcall ctl_write(const uint8 *buf, int32 size) { return 0; }
    int  __cdecl ctl_cmsg(int type, int verbosity_level, const char *fmt, ...);
//PlayMode/static
    static int  s_open_output(void) {
        return g_pDecoder->open_output();
    }
    static void s_close_output(void) {
        g_pDecoder->close_output();
    }
    static int32  s_output_data(const uint8 * Data, int32 Size) {
        return g_pDecoder->output_data(Data, Size);
    }
    static int  s_acntl(int request, void * arg) {
        return g_pDecoder->acntl(request, arg);
    }
    static int  s_detect(void) {
        return g_pDecoder->detect();
    }
//PlayMode
    int  __fastcall open_output (void);
    void __fastcall close_output(void);
    int32  __fastcall output_data (const uint8 * Data, int32 Size);
    int  __fastcall acntl       (int request, void * arg);
    int  __fastcall detect(void) { return 1; }
///r
    void __fastcall calc_preload_time(void);
//
    static unsigned __stdcall ThreadProc(void* pv);
    unsigned __fastcall ThreadProc(void);
    DWORD __fastcall InternalSetPosition(DWORD dwPos);
public:
    BOOL  __fastcall Open(const char *cszFileName, SOUNDINFO *pInfo);
    DWORD __fastcall Render(BYTE *Buffer, DWORD dwSize);
    DWORD __fastcall SetPosition(DWORD dwPos);
    void  __fastcall Close(void);
    KbTimDecoder(void);
    ~KbTimDecoder(void);
    static KbTimDecoder* g_pDecoder;
};

#endif
