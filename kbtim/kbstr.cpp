//#include "StdAfx.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <windows.h>
#pragma hdrstop

#include "kbstr.h"
/*
    ������֌W
*/
BYTE g_byLeadByteTable[256];
BYTE g_byToLowerTable[256];
BYTE g_byToUpperTable[256];

static const char gcszEmpty[] = "";//�󕶎���
///////////////////////////////////////////////////////////////////////////////
void __fastcall kbStr_Initialize(void)
{//�e�[�u��������
 //�g�p�O�ɂP�x�����Ă΂Ȃ���΂Ȃ�Ȃ�
    static LONG uInit = -1; //�������J�n��� 0
    if (InterlockedIncrement(&uInit) == 0) {
        char szLowerUpper[4] = {0};
        int i;
        for (i = 0; i < 256; i++) {
            g_byLeadByteTable[i] = (BYTE) IsDBCSLeadByte((BYTE) i);
            szLowerUpper[0] = i;
            CharLower(szLowerUpper);
            g_byToLowerTable[i] = szLowerUpper[0];
            CharUpper(szLowerUpper);
            g_byToUpperTable[i] = szLowerUpper[0];
        }
    }
    else {
        InterlockedDecrement(&uInit);
    }
}
///////////////////////////////////////////////////////////////////////////////
int __fastcall kbStrLen(const char *cszString)
{//cszString �̒�����Ԃ�
 //cszString �̏I�[���r���œr�؂�Ă���ꍇ�́A�L���ȕ����܂ł̒�����Ԃ�
    const char *s = cszString;
    while (*s) {
        if (kbIsLeadByte(*s)) {
            if (!*(s+1)) {
                break;
            }
            s += 2;
        }
        else {
            s++;
        }
    }
    return s - cszString;
}
///////////////////////////////////////////////////////////////////////////////
int __fastcall kbStrLCpy(char *szDest, const char *cszSrc, int nBufSize)
{// cszSrc �� szDest �� nBufSize �o�C�g���R�s�[
 // ������̒���������Ȃ��ꍇ�� nBufSize-1 �o�C�g�R�s�[���ďI�[�� \0 ��t��
 // �I�[�������r�؂�Ă���悤�ȏꍇ�ɂ��Ή�
 // cszSrc �̗L���Ȓ���(kbStrLen(cszSrc))��Ԃ�
 // �߂�l >= nBufSize �̏ꍇ�A�������؂�̂Ă����Ƃ��Ӗ�����
    int p = 0;
    while (*cszSrc) {
        if (kbIsLeadByte(*cszSrc)) {
            if (!*(cszSrc+1)) {
                break;
            }
            p += 2;
            if (p < nBufSize) {
                *(WORD*) szDest = *(const WORD*) cszSrc;
                szDest += 2;
            }
            cszSrc += 2;
        }
        else if (++p < nBufSize) {
            *szDest++ = *cszSrc++;
        }
        else {
            cszSrc++;
        }
    }
    if (nBufSize > 0) {
        *szDest = 0;
    }
    return p;
}
///////////////////////////////////////////////////////////////////////////////
int __fastcall kbStrLCat(char *szDest, const char *cszSrc, int nBufSize)
{// cszSrc �� szDest �ɘA��
 // nBufSize �� szDest �̃T�C�Y�i�I�[�� '\0' ���܂ށj
 // �I�[�������r�؂�Ă���悤�ȏꍇ�ɂ��Ή�
 // kbStrLen(cszSrc) + min(kbStrLen(szDest), nBufSize) ��Ԃ�
 // �߂�l�� >= nBufSize �̏ꍇ�A�A���������؂�̂Ă����Ƃ��Ӗ�����
 //
    //min(kbStrLen(szDest) + nBufSize) �ƃR�s�[��o�b�t�@�ւ̃|�C���^�𓾂�
    int nDestLen = 0;
    while (*szDest && nDestLen < nBufSize) {
        if (kbIsLeadByte(*szDest)) {
            if (!*(szDest+1) || nDestLen >= nBufSize-2) {
                break;
            }
            else {
                szDest += 2;
                nDestLen += 2;
            }
        }
        else {
            szDest++;
            nDestLen++;
        }
    }
    return kbStrLCpy(szDest, cszSrc, nBufSize - nDestLen) + nDestLen;
}
///////////////////////////////////////////////////////////////////////////////
//static const WORD wWSpace = 0x4081;//�S�p�X�y�[�X
char* __fastcall kbStrTrimRight(char* szString)
{//������lpszString�̏I�[����󔒕�������菜��
    register char *p = szString;
    if (p) {
        char *Space = NULL;
        while (*p) {
            if (kbIsLeadByte(*p)) {//�S�p
                /*if (*(WORD*) p == wWSpace) {
                //�S�p�X�y�[�X����������
                    if (!Space) {
                        Space = p;
                    }
                }
                else*/ if (p[1]) {
                    Space = NULL;//�󔒈ȊO����������
                }
                else {//�����񂪓r���œr�؂�Ă��܂��Ă���H
                    *p = 0;//�S�p�P�o�C�g�ڂ̕����͐؂�̂Ă�
                    break;
                }
                p += 2;
            }
            else {//���p
                if (*p == ' ') {
                //���p�X�y�[�X
                    if (!Space) {
                        Space = p;
                    }
                }
                else {
                    Space = NULL;//�󔒈ȊO����������
                }
                p++;
            }
        }
        if (Space) {
            *Space = 0;
        }
    }
    return szString;
}
///////////////////////////////////////////////////////////////////////////////
char* __fastcall kbStrTrimLeft(char* szString)
{//������lpszString�̐擪����󔒕�������菜��
    register char *src = szString;
    if (src) {
        while (*src) {
            if (*src == ' ') {//���p�X�y�[�X
                src++;
            }
            else {
                if (src != szString) {
                    register char *dst = szString;
                    do{
                        *dst++ = *src++;
                    }while (*src);
                    *dst = 0;
                }
                return szString;
            }
        }
        *szString = 0;//���ׂċ�
    }
    return szString;
}
///////////////////////////////////////////////////////////////////////////////
char* __fastcall kbStrTrim(char *szString)
{//������lpszString�̐擪�ƏI�[����󔒕�������菜��
    return kbStrTrimLeft(kbStrTrimRight(szString));
}
///////////////////////////////////////////////////////////////////////////////
char* __fastcall kbCRLFtoSpace(char *szString)
{//���s�E���A�����ƃ^�u�����𔼊p�X�y�[�X�ɕϊ�����
    register char* p = szString;
    if (p) {
        while (*p) {
            if (kbIsLeadByte(*p)) {//�S�p
                if (!p[1]) {//�����񂪓r���œr�؂�Ă���
                    *p = 0;
                    break;
                }
                p += 2;
            }
            else {//���p
                if (*p == '\n' || *p == '\r' || *p == '\t') {
                    *p = ' ';
                }
                p++;
            }
        }
    }
    return szString;
}
///////////////////////////////////////////////////////////////////////////////
int __fastcall kbStrCmp(const char *cszStr1, const char *cszStr2)
{
    while (*cszStr1) {
        if (!*cszStr2) {
            break;
        }
        if (kbIsLeadByte(*cszStr1)) {
            if (kbIsLeadByte(*cszStr2)) {
                int cmp = *(WORD*) cszStr1 - *(WORD*) cszStr2;
                if (cmp == 0 && cszStr1[1]) {
                    cszStr1 += 2;
                    cszStr2 += 2;
                }
                else {
                    if (*cszStr1 == *cszStr2) {
                        return (BYTE) cszStr1[1] - (BYTE) cszStr2[1];
                    }
                    return (BYTE) cszStr1[0] - (BYTE) cszStr2[0];
                }
            }
            else {
                break;
            }
        }
        else {
            if (kbIsLeadByte(*cszStr2)) {
                break;
            }
            int cmp = (BYTE)(*cszStr1) - (BYTE)(*cszStr2);
            if (cmp) {
                return cmp;
            }
            cszStr1++;
            cszStr2++;
        }
    }
    return (BYTE) *cszStr1 - (BYTE) *cszStr2;
}
///////////////////////////////////////////////////////////////////////////////
//#define KBSTRCMPI_DEBUG
#ifdef KBSTRCMPI_DEBUG
static int __fastcall kbStrCmpI_(const char *cszStr1, const char *cszStr2);
int __fastcall kbStrCmpI(const char *cszStr1, const char *cszStr2)
{
    int ret;
    int ret1  = kbStrCmpI_(cszStr1, cszStr2);
    int ret2 = strcmpi(cszStr1, cszStr2);
    ret = ret1;
    if (ret1) {
        ret1 /= abs(ret1);
    }
    if (ret2) {
        ret2 /= abs(ret2);
    }
    if (ret1 != ret2) {
        if (!ret1 || !ret2) {
        //�v���I�G���[
            int t = 0;
        }
    }
    return ret;
}
///////////////////////////////////////////////////////////////////////////////
static int __fastcall kbStrCmpI_(const char *cszStr1, const char *cszStr2)
#else
int __fastcall kbStrCmpI(const char *cszStr1, const char *cszStr2)
#endif
{
    while (*cszStr1) {
        if (!*cszStr2) {
            break;
        }
        if (kbIsLeadByte(*cszStr1)) {
            if (kbIsLeadByte(*cszStr2)) {
                int cmp = *(WORD*) cszStr1 - *(WORD*) cszStr2;
                if (cmp == 0 && cszStr1[1]) {
                    cszStr1 += 2;
                    cszStr2 += 2;
                }
                else {
                    if (*cszStr1 == *cszStr2) {
                        return (BYTE) cszStr1[1] - (BYTE) cszStr2[1];
                    }
                    return (BYTE) cszStr1[0] - (BYTE) cszStr2[0];
                }
            }
            else {
                break;
            }
        }
        else {
            if (kbIsLeadByte(*cszStr2)) {
                break;
            }
            int cmp = kbToLower(*cszStr1) - kbToLower(*cszStr2);
            //int cmp = tolower(*cszStr1) - tolower(*cszStr2);
            if (cmp) {
                return cmp;
            }
            cszStr1++;
            cszStr2++;
        }
    }
    return (BYTE) *cszStr1 - (BYTE) *cszStr2;
}
///////////////////////////////////////////////////////////////////////////////
int __fastcall kbStrNCmp(const char *cszStr1, const char *cszStr2, int nLen)
{
    while (nLen > 0 && *cszStr1) {
        if (!*cszStr2) {
            return 0;
        }
        if (kbIsLeadByte(*cszStr1)) {
            if (kbIsLeadByte(*cszStr2)) {
                int cmp = *(WORD*) cszStr1 - *(WORD*) cszStr2;
                if (cmp == 0 && cszStr1[1]) {
                    cszStr1 += 2;
                    cszStr2 += 2;
                    nLen -= 2;
                }
                else {
                    if (*cszStr1 == *cszStr2) {
                        return (BYTE) cszStr1[1] - (BYTE) cszStr2[1];
                    }
                    return (BYTE) cszStr1[0] - (BYTE) cszStr2[0];
                }
            }
            else {
                break;
            }
        }
        else {
            if (kbIsLeadByte(*cszStr2)) {
                break;
            }
            int cmp = (BYTE)(*cszStr1) - (BYTE)(*cszStr2);
            //int cmp = tolower(*cszStr1) - tolower(*cszStr2);
            if (cmp) {
                return cmp;
            }
            cszStr1++;
            cszStr2++;
            nLen--;
        }
    }
    if (nLen > 0) {
        return (BYTE) *cszStr1 - (BYTE) *cszStr2;
    }
    else {
        return 0;
    }
}
///////////////////////////////////////////////////////////////////////////////
int __fastcall kbStrNCmpI(const char *cszStr1, const char *cszStr2, int nLen)
{
    while (nLen > 0 && *cszStr1) {
        if (!*cszStr2) {
            return 0;
        }
        if (kbIsLeadByte(*cszStr1)) {
            if (kbIsLeadByte(*cszStr2)) {
                int cmp = *(WORD*) cszStr1 - *(WORD*) cszStr2;
                if (cmp == 0 && cszStr1[1]) {
                    cszStr1 += 2;
                    cszStr2 += 2;
                    nLen -= 2;
                }
                else {
                    if (*cszStr1 == *cszStr2) {
                        return (BYTE) cszStr1[1] - (BYTE) cszStr2[1];
                    }
                    return (BYTE) cszStr1[0] - (BYTE) cszStr2[0];
                }
            }
            else {
                break;
            }
        }
        else {
            if (kbIsLeadByte(*cszStr2)) {
                break;
            }
            int cmp = kbToLower(*cszStr1) - kbToLower(*cszStr2);
            //int cmp = tolower(*cszStr1) - tolower(*cszStr2);
            if (cmp) {
                return cmp;
            }
            cszStr1++;
            cszStr2++;
            nLen--;
        }
    }
    if (nLen > 0) {
        return (BYTE) *cszStr1 - (BYTE) *cszStr2;
    }
    else {
        return 0;
    }
}
///////////////////////////////////////////////////////////////////////////////
const char* __fastcall kbStrChr(const char *cszString, char cFind)
{//������ cszString ���� ���� cFind ���������A�ŏ��Ɍ������������ւ̃|�C���^��Ԃ�
 //������Ȃ��ꍇ�� NULL ��Ԃ�
 //cFind �� '\0' ���L��
 //cFind == '\0' �ŕ����񂪓r�؂�Ă���Ƃ��A�r�؂ꂽ�ŏ��̕����ւ̃|�C���^��Ԃ�
    if (!cszString) return NULL;
    while (1) {
        if ((BYTE) *cszString == (BYTE) cFind) {
            return cszString;
        }
        else if (kbIsLeadByte((BYTE) *cszString)) {
            if (cszString[1]) {
                cszString += 2;
            }
            else if (cFind) {//�����񂪓r�؂�Ă���
                return NULL;
            }
            else {//�r�؂ꂽ�ŏ��̕����ւ̃|�C���^
                return cszString;
            }
        }
        else if (*cszString) {
            cszString++;
        }
        else {
            return NULL;
        }
    }
}
///////////////////////////////////////////////////////////////////////////////
const char* __fastcall kbStrRChr(const char *cszString, char cFind)
{//������ cszString ���� ���� cFind ���������A�Ō�Ɍ������������ւ̃|�C���^��Ԃ�
 //������Ȃ��ꍇ�� NULL ��Ԃ�
 //cFind �� '\0' ���L��
 //cFind == '\0' �ŕ����񂪓r�؂�Ă���Ƃ��A�r�؂ꂽ�ŏ��̕����ւ̃|�C���^��Ԃ�
    if (!cszString) return NULL;
    const char *cszRet = NULL;
    while (1) {
        if ((BYTE) *cszString == (BYTE) cFind) {
            cszRet = cszString;
        }
        else if (kbIsLeadByte((BYTE) *cszString)) {
            if (cszString[1]) {
                cszString += 2;
                continue;
            }
            else if (cFind) {//�����񂪓r�؂�Ă���
                return cszRet;
            }
            else {//�r�؂ꂽ�ŏ��̕����ւ̃|�C���^
                return cszString;
            }
        }
        if (*cszString) {
            cszString++;
        }
        else {
            break;
        }
    }
    return cszRet;
}
///////////////////////////////////////////////////////////////////////////////
const char* __fastcall kbStrStr(const char *cszString, const char *cszSearch)
{
    int nSearchLen = -1;
    while (*cszString) {
        if (*cszString == *cszSearch) {
            if (nSearchLen < 0) {
                nSearchLen = kbStrLen(cszSearch);
            }
            //if (strncmp(cszString, cszSearch, nSearchLen) == 0) {
            if (kbStrNCmp(cszString, cszSearch, nSearchLen) == 0) {
                return cszString;
            }
        }
        if (kbIsLeadByte(*cszString)) {
            if (!cszString[1]) {
                break;
            }
            cszString += 2;
        }
        else {
            cszString++;
        }
    }
    return NULL;
}
///////////////////////////////////////////////////////////////////////////////
const char* __fastcall kbStrStrI(const char *cszString, const char *cszSearch)
{
    int nSearchLen = kbStrLen(cszSearch);
    while (*cszString) {
        //if (strnicmp(cszString, cszSearch, nSearchLen) == 0) {
        //    return cszString;
        //}
        if (kbStrNCmpI(cszString, cszSearch, nSearchLen) == 0) {
            return cszString;
        }
        if (kbIsLeadByte(*cszString)) {
            if (!cszString[1]) {
                break;
            }
            cszString += 2;
        }
        else {
            cszString++;
        }
    }
    return NULL;
}
///////////////////////////////////////////////////////////////////////////////
char* __fastcall kbStrUpr(char *pszString)
{
    char *p = pszString;
    while (*p) {
        *p = kbToUpper(*p);
        p++;
    }
    return pszString;
}
///////////////////////////////////////////////////////////////////////////////
char* __fastcall kbStrLwr(char *pszString)
{
    char *p = pszString;
    while (*p) {
        *p = kbToLower(*p);
        p++;
    }
    return pszString;
}
///////////////////////////////////////////////////////////////////////////////
char* __fastcall kbStrTok(char *pszString, const char *cszDelimiter, char **ppszNext)
{
    *ppszNext = NULL;
    char *p = pszString;
    if (p) {
        while (*p) {
            if (kbIsLeadByte(*p)) {
                if (!p[1]) {//�����񂪓r�؂�Ă���
                    *p = 0;
                    return pszString;
                }
                p += 2;
                continue;
            }
            const char *delim = cszDelimiter;
            while (*delim) {
                if (*p == *delim++) {
                    *p++ = 0;
                    *ppszNext = p;
                    return pszString;
                }
            }
            p++;
        }
        if (*pszString) {
            return pszString;
        }
    }
    return NULL;
}
///////////////////////////////////////////////////////////////////////////////
#if 1
static inline bool __fastcall is_digit(char c)
{
    return ('0' <= c && c <= '9');
}
int __fastcall kbStrToIntDef(const char* p, int nDefault)
{//������𐮐��ɕϊ��ł����ꍇ�͂��̒l��Ԃ�
 //�ϊ��ł��Ȃ��ꍇ�� nDefault ��Ԃ�
    int nSign = 1;//����
    //�󔒂��Ƃ΂�
        while (*p == ' ') {
        p++;
    }
    //����
        switch (*p) {
                case '-':
                        nSign = -1;
            p++;
            break;
                case '+':
                        p++;
            break;
        }
    if (is_digit(*p)) {
        //��������
        int nReturn = (*p++ - '0');
        while (is_digit(*p)) {
            nReturn *= 10;
            nReturn += (*p++ - '0');
        }
        //��������
        if (*p == '.') {
            while (is_digit(*++p));//���������͂��ׂĖ���
        }
        //���l�̕����ȍ~�ɋ󔒈ȊO�̕��������������ꍇ�� nDefault ��Ԃ�
        while (*p == ' ') {
            p++;
        }
        if (!*p) {
            return nSign*nReturn;
        }
    }
    return nDefault;
}
double __fastcall kbStrToDoubleDef(const char* p, double nDefault)
{//������������ɕϊ��ł����ꍇ�͂��̒l��Ԃ�
 //�ϊ��ł��Ȃ��ꍇ�� nDefault ��Ԃ�
    int nSign = 1;//����
    //�󔒂��Ƃ΂�
        while (*p == ' ') {
        p++;
    }
    //����
        switch (*p) {
                case '-':
                        nSign = -1;
            p++;
            break;
                case '+':
                        p++;
            break;
        }
    if (is_digit(*p)) {
        //��������
        double nReturn = (*p++ - '0');
        while (is_digit(*p)) {
            nReturn *= 10;
            nReturn += (*p++ - '0');
        }
        //��������
        if (*p == '.') {
            int multi = 1;
            ++p;
            while (is_digit(*p)) {
              multi *= 10;
              nReturn += (double)(*p++ - '0') / multi;
            }
        }
        //���l�̕����ȍ~�ɋ󔒈ȊO�̕��������������ꍇ�� nDefault ��Ԃ�
        while (*p == ' ') {
            p++;
        }
        if (!*p) {
            return nSign*nReturn;
        }
    }
    return nDefault;
}
#else
int __fastcall kbStrToIntDef(const char* cszStr, int nDefault)
{//cszStr �𐮐��ɕϊ�����
 //�ϊ��ł��Ȃ��ꍇ�� nDefault ��Ԃ�
    if (!cszStr) {
        return nDefault;
    }
    const int cMaxLen = 1024;
    const int len = kbStrLen(cszStr) + 1;
    if (len > cMaxLen) {
        return nDefault;//�����񂪒�������
    }
    char szCopy[cMaxLen];
    memcpy(szCopy, cszStr, len);
    kbStrTrim(szCopy);
    int ret;
    if (szCopy[0]) {
        char *szEndStr = NULL;
        ret = strtol(szCopy, &szEndStr, 10);
        if (szEndStr && szEndStr[0]) {
            ret = nDefault;
        }
    }
    else {
        ret = nDefault;
    }
    return ret;
}
double __fastcall kbStrToDoubleDef(const char* cszStr, double nDefault)
{//cszStr �������ɕϊ�����
 //�ϊ��ł��Ȃ��ꍇ�� nDefault ��Ԃ�
    if (!cszStr) {
        return nDefault;
    }
    const int cMaxLen = 1024;
    const int len = kbStrLen(cszStr) + 1;
    if (len > cMaxLen) {
        return nDefault;//�����񂪒�������
    }
    char szCopy[cMaxLen];
    memcpy(szCopy, cszStr, len);
    kbStrTrim(szCopy);
    int ret;
    if (szCopy[0]) {
        char *szEndStr = NULL;
        ret = strtof(szCopy, &szEndStr, 10);
        if (szEndStr && szEndStr[0]) {
            ret = nDefault;
        }
    }
    else {
        ret = nDefault;
    }
    return ret;
}
#endif
///////////////////////////////////////////////////////////////////////////////
int __fastcall kbStringReplace(char *dst,
                               int   size,
                               const char *src,
                               const char *old_pattern,
                               const char *new_pattern)
{//������ src �� old_pattern �� new_pattern �ɒu������
 //size �� dst �̃o�b�t�@�T�C�Y
 //old_pattern == "" �i�󕶎���j�̏ꍇ�AkbStrLCat(dst, size, new_pattern) �Ɠ���
 //�o�b�t�@�T�C�Y������Ȃ��ꍇ�́Asize-1 �o�C�g�܂Œu������
 //���S�u�����ꂽ�Ƃ����Ƃ��̕�����̒�����Ԃ�
 //�߂�l >= size �̏ꍇ�A�u����̕����񂪈ꕔ�؂�̂Ă�ꂽ���Ƃ��Ӗ�����
    int ret = 0;
    const int old_pattern_len = kbStrLen(old_pattern);
    while (1) {
        if (ret < size &&
           *src == *old_pattern &&
           kbStrNCmp(src, old_pattern, old_pattern_len) == 0) {
            const int new_pattern_len = kbStrLCpy(dst, new_pattern, size - ret);
            if (new_pattern_len < size - ret) {
                dst += new_pattern_len;
            }
            else {
                dst += (size - ret);
            }
            ret += new_pattern_len;
            if (!*src) {
                break;
            }
            src += old_pattern_len;
            continue;
        }
        if (kbIsLeadByte(*src)) {
            if (src[1]) {
                ret += 2;
                if (ret < size) {
                    *((WORD*) dst) = *((WORD*) src);
                    dst += 2;
                }
                src += 2;
            }
            else {
                break;
            }
        }
        else if (*src) {
            if (++ret < size) {
                *dst++ = *src++;
            }
            else {
                src++;
            }
        }
        else {
            break;
        }
    }
    if (size > 0) {
        *dst = 0;
    }
    return ret;
}
///////////////////////////////////////////////////////////////////////////////
int __fastcall kbMultiByteToWideChar(const char *cszMultiByte,
                                     wchar_t* wszWideChar,
                                     int cchWideChar)
{//cchWideChar == 0 �̂Ƃ��͕K�v�ȃo�b�t�@�̃T�C�Y(�������P��)��Ԃ�
    int ret = MultiByteToWideChar(CP_ACP, 0, cszMultiByte, -1,
                                      wszWideChar, cchWideChar);
    //�o�b�t�@�T�C�Y������Ȃ��ꍇ�AwszWideChar �̏I�[������ \0 �ɂȂ�Ȃ�
    if (cchWideChar) {
        wszWideChar[cchWideChar-1] = 0;//�I�[�� \0 �ɂ���
    }
    return ret;
}
///////////////////////////////////////////////////////////////////////////////
int __fastcall kbWideCharToMultiByte(const wchar_t *wcszWideChar,
                                     char *szMultiByte,
                                     int cchMultiByte)
{//cchMultiByte �� 0 �̂Ƃ��͕K�v�ȃo�b�t�@�̃T�C�Y��Ԃ�
    int ret = WideCharToMultiByte(CP_ACP, 0, wcszWideChar, -1,
                                szMultiByte, cchMultiByte, NULL, NULL);
    //�o�b�t�@�T�C�Y������Ȃ��ꍇ�AszMultiByte �̏I�[������ \0 �ɂȂ�Ȃ�
    if (cchMultiByte) {
        szMultiByte[cchMultiByte-1] = 0;//�I�[�� \0 �ɂ���
        if (ret == 0) {//�T�C�Y������Ȃ�(���ŏI�[�������}���`�o�C�g��2�o�C�g�ڂ������ꍇ�ɑΏ��j
            int nLen = kbStrLen(szMultiByte);//�����񂪓r�؂�Ă�ꍇ�͗L���ȕ����܂ł̒�����Ԃ�
            szMultiByte[nLen] = 0;           //�r�؂�Ă�ꍇ�͓r�؂ꂽ���������������
        }
    }
    return ret;
}
///////////////////////////////////////////////////////////////////////////////
//
//�t�@�C�����֌W
//
///////////////////////////////////////////////////////////////////////////////
const char* __fastcall kbExtractFileExt(const char* cszSrc)
{//cszSrc ���̊g���q�����̃|�C���^��Ԃ�
 //������Ȃ��ꍇ�͋󕶎����Ԃ��iNULL �ł͂Ȃ��j
    if (!cszSrc) return gcszEmpty;
    const BYTE* LastPeriod = NULL;
    const BYTE* LastPathDelimiter = NULL;
    while (*cszSrc) {
        if (kbIsLeadByte((*((const BYTE*) cszSrc)))) {
            if (cszSrc[1]) {
                cszSrc+=2;
                continue;
            }
            else {//�����񂪓r���œr�؂�Ă���
                break;
            }
        }
        if ((BYTE) *cszSrc == (BYTE)'.') {
            LastPeriod = (BYTE*) cszSrc;
        }
        else if ((BYTE) *cszSrc == (BYTE)'\\') {
            LastPathDelimiter = (BYTE*) cszSrc;
        }
        cszSrc++;
    }
    if (LastPeriod && (DWORD) LastPeriod > (DWORD) LastPathDelimiter) {
        //�s���I�h���������Ă��A�g���q�Ƃ͌���Ȃ�
        //��FC:\\hoge.hoge\\hogehoge
        return (const char*) LastPeriod;
    }
    return gcszEmpty;
}
///////////////////////////////////////////////////////////////////////////////
const char* __fastcall kbExtractFileName(const char* cszFileName)
{//cszFileName ����p�X����菜�������́i�t�@�C���������j�̃|�C���^��Ԃ�
    const char *cszLastDelimiter = kbStrRChr(cszFileName, '\\');
    if (cszLastDelimiter) {
        return cszLastDelimiter + 1;
    }
    if (cszFileName) return cszFileName;
    return gcszEmpty;
}
///////////////////////////////////////////////////////////////////////////////
char* __fastcall kbExtractFilePath(char *szDest, const char *cszFileName, int nSize)
{//cszFileName ����p�X���擾���ă|�C���^��Ԃ�
 //szDest == cszFileName �ł����Ă͂Ȃ�Ȃ�
    szDest[0] = 0;
    const char *cszLastPathDelimiter = kbStrRChr(cszFileName, '\\');
    if (cszLastPathDelimiter) {
        int nLen = cszLastPathDelimiter - cszFileName + 1;
        if (nLen < nSize) {
            memcpy(szDest, cszFileName, nLen);
            szDest[nLen] =0;
        }
    }
    return szDest;
}
///////////////////////////////////////////////////////////////////////////////
const char* __fastcall kbExtractFileNameInArc(const char *cszFileName)
{//���Ƀt�@�C�����̃t�@�C��������p�X����菜�������̂�Ԃ�
 //��P�FC:\hoge\hoge.lzh>test.mid �̖߂�l = test.mid
 //��Q�FC:\hoge\hoge.lzh>test/test2.mid �̖߂�l��test2.mid
 //��R�FC:\hoge\hoge.lzh>test/testzip.zip>test3.mid �̖߂�l = test3.mid
 //��S�FC:\hoge\hoge.lzh>test/testzip.zip>test/test4.mid �̖߂�l = test4.mid
    if (!cszFileName) return gcszEmpty;
    const char *cszDelimiter = kbStrRChr(cszFileName, '>');
    if (cszDelimiter) {
        cszFileName = cszDelimiter + 1;
    }
    cszDelimiter = kbStrRChr(cszFileName, '/');
    if (cszDelimiter) {
        return cszDelimiter + 1;
    }
    return cszFileName;
}
///////////////////////////////////////////////////////////////////////////////
const char* __fastcall kbExtractFilePathInArc(char *szDest, const char *cszFileName)
{//���Ƀt�@�C�����̃t�@�C��������t�@�C��������菜�������̂�Ԃ�
 //��P�FC:\hoge\hoge.lzh>test.mid �̖߂�l = C:\hoge\hoge.lzh>
 //��Q�FC:\hoge\hoge.lzh>test/test2.mid �̖߂�l = C:\hoge\hoge.lzh>test/
 //��R�FC:\hoge\hoge.lzh>test/testzip.zip>test3.mid �̖߂�l = C:\hoge\hoge.lzh>test/testzip.zip>
 //��S�FC:\hoge\hoge.lzh>test/testzip.zip>test/test4.mid �̖߂�l = C:\hoge\hoge.lzh>test/testzip.zip>test/
    if (!szDest) return NULL;
    szDest[0] = 0;
    if (!cszFileName) return szDest;
    const char *cszFirst = cszFileName;
    const char *cszDelimiter = kbStrRChr(cszFileName, '>');
    if (cszDelimiter) {
        cszFileName = cszDelimiter + 1;
    }
    cszDelimiter = kbStrRChr(cszFileName, '/');
    if (cszDelimiter) {
        cszFileName = cszDelimiter + 1;
    }
    int nCopySize = cszFileName - cszFirst;
    if (nCopySize > 0) {
        memcpy(szDest, cszFirst, nCopySize);
        szDest[nCopySize] = 0;
    }
    return szDest;
}
///////////////////////////////////////////////////////////////////////////////
char* __fastcall kbExtractReplacedFileExt(char *szDest,
                                          const char *cszFileName,
                                          const char *cszExt)
{//cszFileName �̊g���q�� cszExt �ɒu�������� szDest �ɃR�s�[����
 //szDest �̃T�C�Y�� cszFileName �� cszExt �̍��v���傫�����Ƃ��O��
    if (!szDest) {
        return NULL;
    }
    if (!cszExt) {
        cszExt = gcszEmpty;
    }
    if (!cszFileName) {
        strcpy(szDest, cszExt);
        return szDest;
    }
    const char *ext =  kbStrRChr(cszFileName, '.');
    const char *path = kbStrRChr(cszFileName, '\\');
    if (ext && ext > path) {
        const int len = ext - cszFileName;
        memcpy(szDest, cszFileName, len);
        strcpy(szDest + len, cszExt);
    }
    else {
        strcpy(szDest, cszFileName);
        strcat(szDest, cszExt);
    }

    return szDest;
}
///////////////////////////////////////////////////////////////////////////////
void __fastcall kbAddPathDelimiter(char *szPath)
{//������̏I�[�� \ ��ǉ�
 //���łɏI�[�� \ �̏ꍇ�͉������Ȃ�
    char *p = szPath;
    char *last = p;
    //�I�[������T��
    while (*p) {
        last = p;
        if (kbIsLeadByte(*p)) {
            if (!p[1]) {//�����񂪓r�؂�Ă���
                *p = 0;
                p = last = szPath;
                continue;//�ŏ������蒼��
            }
            p += 2;
        }
        else {
            p++;
        }
    }
    if (*last != '\\') {
        p[0] = '\\';
        p[1] = 0;
    }
}
///////////////////////////////////////////////////////////////////////////////
void __fastcall kbCombinePath(char *szDest, const char *cszSrc,
                              const char *cszAdd, int nDestSize)
{
    kbStrLCpy(szDest, cszSrc, nDestSize);
    char *p = kbStrRChr(szDest, '\\');
    if (!p || p[1]) {//\ ���܂܂�Ȃ� or �܂ނ��I�[�ł͂Ȃ�
        kbStrLCat(szDest, "\\", nDestSize);//kbAddPathDelimiter ���ƃo�b�t�@�T�C�Y�̃`�F�b�N�����Ȃ��̂Ŏg�p�s��
    }
    kbStrLCat(szDest, cszAdd, nDestSize);
}
///////////////////////////////////////////////////////////////////////////////
void __fastcall kbRemovePathDelimiter(char *szPath)
{   //�h���C�u�̃��[�g�łȂ��ꍇ�͏I�[�� \ ������
    char *p = kbStrRChr(szPath, '\\');
    if (p && !p[1] && (p - szPath >= 3)) {
        *p = 0;
    }
}
///////////////////////////////////////////////////////////////////////////////

