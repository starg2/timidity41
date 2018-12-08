#ifndef _RINGBUF_H
#define _RINGBUF_H
//�����O�o�b�t�@
//�e���\�b�h�Ăяo���̔r�������͌Ăяo�����ŊǗ�����
//�����O�o�b�t�@�̃T�C�Y�͕K�v�ɉ����Ď����I�Ɋg�傷�邪�A
//�g���������ƍی��Ȃ��T�C�Y���傫���Ȃ�̂Œ��ӂ���
class KbRingBuffer
{
private:
    HANDLE m_hHeap;
    BYTE*  m_pBuffer;     //�����O�o�b�t�@
    DWORD  m_dwReadPos;   //�ǂݎ��ʒu
    DWORD  m_dwWritePos;  //�������݈ʒu
    DWORD  m_dwWritten;   //�������񂾃o�b�t�@�̃T�C�Y
    DWORD  m_dwBufferSize;//�����O�o�b�t�@�̃T�C�Y
public:
    KbRingBuffer(DWORD dwInitialSize = 0);
    ~KbRingBuffer(void);
    inline void __fastcall Reset(void) {
        m_dwReadPos = m_dwWritePos = m_dwWritten = 0;
    }
    inline DWORD __fastcall GetSize(void) const{//�����O�o�b�t�@�̃T�C�Y
        return m_dwBufferSize;
    }
    inline DWORD __fastcall GetWritten(void) const{//�������񂾃o�b�t�@�̃T�C�Y
        return m_dwWritten;
    }
    inline DWORD __fastcall GetRemain(void) const{//�������݉\�Ȏc��̃o�b�t�@�T�C�Y
        return m_dwBufferSize-m_dwWritten;
    }
    void  __fastcall SetSize(DWORD dwSize);//�����O�o�b�t�@�̃T�C�Y��ݒ肷��
    DWORD __fastcall Read(BYTE* buf, DWORD dwSize);
    void  __fastcall Write(const BYTE* buf, DWORD dwSize);
};
#endif
