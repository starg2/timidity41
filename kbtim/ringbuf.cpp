#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <windows.h>
#include "ringbuf.h"

KbRingBuffer::KbRingBuffer(DWORD dwInitialSize/*=0*/)
{
    if ((int) dwInitialSize <= 0) {
        dwInitialSize = 4*1024;
    }
    m_hHeap = NULL;
    m_pBuffer = NULL;
    m_dwReadPos = m_dwWritePos = m_dwWritten = 0;
    m_dwBufferSize = dwInitialSize;
}
KbRingBuffer::~KbRingBuffer(void)
{
    if (m_hHeap) {
        HeapDestroy(m_hHeap);
    }
}
void __fastcall KbRingBuffer::SetSize(DWORD dwNewSize)
{//リングバッファのサイズを拡大・縮小する
 //サイズを縮小する場合は、縮小前のデータはすべて失われる
 //サイズを拡大する場合は、拡大前後でデータは失われない
    //OutputDebugString("Expand\n");
    BYTE *pNewBuf;
    if (dwNewSize < m_dwBufferSize) {//縮小する
        m_dwReadPos = m_dwWritePos = m_dwWritten = 0;//縮小前のデータはすべて失われる
    }
    //排他処理は呼び出し側で管理するので HEAP_NO_SERIALIZE で問題なし
#if 1
    if (!m_hHeap) {
        m_hHeap = ::HeapCreate(HEAP_NO_SERIALIZE, dwNewSize+4096, 0);
        pNewBuf = (BYTE*) HeapAlloc(m_hHeap, HEAP_NO_SERIALIZE, dwNewSize);
    }
    else {
        pNewBuf = (BYTE*) HeapReAlloc(m_hHeap, HEAP_NO_SERIALIZE, m_pBuffer, dwNewSize);
    }
    if (m_dwReadPos > m_dwWritePos ||
       (m_dwReadPos == m_dwWritePos && m_dwWritten != 0)) {
        DWORD dwExpandSize = dwNewSize-m_dwBufferSize;
        //memmove(pNewBuf + m_dwReadPos + dwExpandSize, pNewBuf+m_dwReadPos,
        //            m_dwBufferSize-m_dwReadPos);
        const DWORD *end4 = (DWORD*)(pNewBuf+m_dwReadPos);
        DWORD *src4 = (DWORD*)(pNewBuf+m_dwBufferSize-4);
        DWORD *dst4 = (DWORD*)((BYTE*) src4+dwExpandSize);
        while (src4 >= end4) {
            *dst4-- = *src4--;
        }
        BYTE *src = (BYTE*) src4+3;
        BYTE *dst = (BYTE*) dst4;
        while (src >= (const BYTE*) end4) {
            *dst-- = *src--;
        }
        m_dwReadPos += dwExpandSize;
    }
#else
    DWORD dwWritten = m_dwWritten;
    if (!m_hHeap) {
        m_hHeap = ::HeapCreate(HEAP_NO_SERIALIZE, dwNewSize+4096, 0);
    }
    pNewBuf = (BYTE*) HeapAlloc(m_hHeap, HEAP_NO_SERIALIZE, dwNewSize);
    Read(pNewBuf, dwWritten);
    HeapFree(m_hHeap, 0, m_pBuffer);
    m_dwReadPos = 0;
    m_dwWritePos = m_dwWritten = dwWritten;
#endif
    m_pBuffer = pNewBuf;
    m_dwBufferSize = dwNewSize;
}

DWORD __fastcall KbRingBuffer::Read(BYTE* buf, DWORD dwSize)
{//リングバッファの内容を読み取る
 //実際に読み取ったバイト数を返す
    if (dwSize > m_dwWritten) {
        dwSize = m_dwWritten;
    }
    DWORD dwCopyFirst;//先頭部分からのコピーサイズ
    DWORD dwCopyTail; //終端部分からのコピーサイズ
    dwCopyTail = m_dwBufferSize-m_dwReadPos;
    if (dwCopyTail > dwSize) {
        dwCopyTail = dwSize;
    }
    dwCopyFirst = dwSize-dwCopyTail;
    memcpy(buf,m_pBuffer+m_dwReadPos,dwCopyTail);
        memcpy(buf+dwCopyTail,m_pBuffer,dwCopyFirst);
    m_dwReadPos += dwSize;
    if (m_dwReadPos > m_dwBufferSize) {
        m_dwReadPos -= m_dwBufferSize;
    }
    m_dwWritten -= dwSize;
    return dwSize;
}
void __fastcall KbRingBuffer::Write(const BYTE* buf, DWORD dwSize)
{//リングバッファに書き込む
 //バッファサイズが足りない場合は拡大する
    if (m_dwBufferSize-m_dwWritten < dwSize || !m_pBuffer) {
        SetSize(m_dwWritten + dwSize);
    }
    DWORD dwCopyFirst;//先頭部分へのコピーサイズ
    DWORD dwCopyTail; //終端部分へのコピーサイズ
    dwCopyTail = m_dwBufferSize-m_dwWritePos;
    if (dwCopyTail > dwSize) {
        dwCopyTail = dwSize;
    }
    dwCopyFirst = dwSize-dwCopyTail;
    memcpy(m_pBuffer+m_dwWritePos, buf,dwCopyTail);
        memcpy(m_pBuffer,buf+dwCopyTail,dwCopyFirst);
    m_dwWritePos += dwSize;
    if (m_dwWritePos > m_dwBufferSize) {
        m_dwWritePos -= m_dwBufferSize;
    }
    m_dwWritten += dwSize;
}
