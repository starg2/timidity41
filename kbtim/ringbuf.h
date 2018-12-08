#ifndef _RINGBUF_H
#define _RINGBUF_H
//リングバッファ
//各メソッド呼び出しの排他処理は呼び出し側で管理する
//リングバッファのサイズは必要に応じて自動的に拡大するが、
//使い方を誤ると際限なくサイズが大きくなるので注意する
class KbRingBuffer
{
private:
    HANDLE m_hHeap;
    BYTE*  m_pBuffer;     //リングバッファ
    DWORD  m_dwReadPos;   //読み取り位置
    DWORD  m_dwWritePos;  //書き込み位置
    DWORD  m_dwWritten;   //書き込んだバッファのサイズ
    DWORD  m_dwBufferSize;//リングバッファのサイズ
public:
    KbRingBuffer(DWORD dwInitialSize = 0);
    ~KbRingBuffer(void);
    inline void __fastcall Reset(void) {
        m_dwReadPos = m_dwWritePos = m_dwWritten = 0;
    }
    inline DWORD __fastcall GetSize(void) const{//リングバッファのサイズ
        return m_dwBufferSize;
    }
    inline DWORD __fastcall GetWritten(void) const{//書き込んだバッファのサイズ
        return m_dwWritten;
    }
    inline DWORD __fastcall GetRemain(void) const{//書き込み可能な残りのバッファサイズ
        return m_dwBufferSize-m_dwWritten;
    }
    void  __fastcall SetSize(DWORD dwSize);//リングバッファのサイズを設定する
    DWORD __fastcall Read(BYTE* buf, DWORD dwSize);
    void  __fastcall Write(const BYTE* buf, DWORD dwSize);
};
#endif
