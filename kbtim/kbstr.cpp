//#include "StdAfx.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <windows.h>
#pragma hdrstop

#include "kbstr.h"
/*
    文字列関係
*/
BYTE g_byLeadByteTable[256];
BYTE g_byToLowerTable[256];
BYTE g_byToUpperTable[256];

static const char gcszEmpty[] = "";//空文字列
///////////////////////////////////////////////////////////////////////////////
void __fastcall kbStr_Initialize(void)
{//テーブル初期化
 //使用前に１度だけ呼ばなければならない
    static LONG uInit = -1; //初期化開始後は 0
    if(InterlockedIncrement(&uInit) == 0){
        char szLowerUpper[4] = {0};
        int i;
        for(i = 0; i < 256; i++){
            g_byLeadByteTable[i] = (BYTE)IsDBCSLeadByte((BYTE)i);
            szLowerUpper[0] = i;
            CharLower(szLowerUpper);
            g_byToLowerTable[i] = szLowerUpper[0];
            CharUpper(szLowerUpper);
            g_byToUpperTable[i] = szLowerUpper[0];
        }
    }
    else{
        InterlockedDecrement(&uInit);
    }
}
///////////////////////////////////////////////////////////////////////////////
int __fastcall kbStrLen(const char *cszString)
{//cszString の長さを返す
 //cszString の終端が途中で途切れている場合は、有効な部分までの長さを返す
    const char *s = cszString;
    while(*s){
        if(kbIsLeadByte(*s)){
            if(!*(s+1)){
                break;
            }
            s += 2;
        }
        else{
            s++;
        }
    }
    return s - cszString;
}
///////////////////////////////////////////////////////////////////////////////
int __fastcall kbStrLCpy(char *szDest, const char *cszSrc, int nBufSize)
{// cszSrc を szDest に nBufSize バイト分コピー
 // 文字列の長さが足りない場合は nBufSize-1 バイトコピーして終端に \0 を付加
 // 終端文字が途切れているような場合にも対応
 // cszSrc の有効な長さ(kbStrLen(cszSrc))を返す
 // 戻り値 >= nBufSize の場合、文字列を切り捨てたことを意味する
    int p = 0;
    while(*cszSrc){
        if(kbIsLeadByte(*cszSrc)){
            if(!*(cszSrc+1)){
                break;
            }
            p += 2;
            if(p < nBufSize){
                *(WORD*)szDest = *(const WORD*)cszSrc;
                szDest += 2;
            }
            cszSrc += 2;
        }
        else if(++p < nBufSize){
            *szDest++ = *cszSrc++;
        }
        else{
            cszSrc++;
        }
    }
    if(nBufSize > 0){
        *szDest = 0;
    }
    return p;
}
///////////////////////////////////////////////////////////////////////////////
int __fastcall kbStrLCat(char *szDest, const char *cszSrc, int nBufSize)
{// cszSrc を szDest に連結
 // nBufSize は szDest のサイズ（終端の '\0' を含む）
 // 終端文字が途切れているような場合にも対応
 // kbStrLen(cszSrc) + min(kbStrLen(szDest), nBufSize) を返す
 // 戻り値が >= nBufSize の場合、連結文字列を切り捨てたことを意味する
 //
    //min(kbStrLen(szDest) + nBufSize) とコピー先バッファへのポインタを得る
    int nDestLen = 0;
    while(*szDest && nDestLen < nBufSize){
        if(kbIsLeadByte(*szDest)){
            if(!*(szDest+1) || nDestLen >= nBufSize-2){
                break;
            }
            else{
                szDest += 2;
                nDestLen += 2;
            }
        }
        else{
            szDest++;
            nDestLen++;
        }
    }
    return kbStrLCpy(szDest, cszSrc, nBufSize - nDestLen) + nDestLen;
}
///////////////////////////////////////////////////////////////////////////////
//static const WORD wWSpace = 0x4081;//全角スペース
char* __fastcall kbStrTrimRight(char* szString)
{//文字列lpszStringの終端から空白文字を取り除く
    register char *p = szString;
    if(p){
        char *Space = NULL;
        while(*p){
            if(kbIsLeadByte(*p)){//全角
                /*if(*(WORD*)p == wWSpace){
                //全角スペースが見つかった
                    if(!Space){
                        Space = p;
                    }
                }
                else*/ if(p[1]){
                    Space = NULL;//空白以外が見つかった
                }
                else{//文字列が途中で途切れてしまっている？
                    *p = 0;//全角１バイト目の部分は切り捨てる
                    break;
                }
                p += 2;
            }
            else{//半角
                if(*p == ' '){
                //半角スペース
                    if(!Space){
                        Space = p;
                    }
                }
                else{
                    Space = NULL;//空白以外が見つかった
                }
                p++;
            }
        }
        if(Space){
            *Space = 0;
        }
    }
    return szString;
}
///////////////////////////////////////////////////////////////////////////////
char* __fastcall kbStrTrimLeft(char* szString)
{//文字列lpszStringの先頭から空白文字を取り除く
    register char *src = szString;
    if(src){
        while(*src){
            if(*src == ' '){//半角スペース
                src++;
            }
            else{
                if(src != szString){
                    register char *dst = szString;
                    do{
                        *dst++ = *src++;
                    }while(*src);
                    *dst = 0;
                }
                return szString;
            }
        }
        *szString = 0;//すべて空白
    }
    return szString;
}
///////////////////////////////////////////////////////////////////////////////
char* __fastcall kbStrTrim(char *szString)
{//文字列lpszStringの先頭と終端から空白文字を取り除く
    return kbStrTrimLeft(kbStrTrimRight(szString));
}
///////////////////////////////////////////////////////////////////////////////
char* __fastcall kbCRLFtoSpace(char *szString)
{//改行・復帰文字とタブ文字を半角スペースに変換する
    register char* p = szString;
    if(p){
        while(*p){
            if(kbIsLeadByte(*p)){//全角
                if(!p[1]){//文字列が途中で途切れている
                    *p = 0;
                    break;
                }
                p += 2;
            }
            else{//半角
                if(*p == '\n' || *p == '\r' || *p == '\t'){
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
    while(*cszStr1){
        if(!*cszStr2){
            break;
        }
        if(kbIsLeadByte(*cszStr1)){
            if(kbIsLeadByte(*cszStr2)){
                int cmp = *(WORD*)cszStr1 - *(WORD*)cszStr2;
                if(cmp == 0 && cszStr1[1]){
                    cszStr1 += 2;
                    cszStr2 += 2;
                }
                else{
                    if(*cszStr1 == *cszStr2){
                        return (BYTE)cszStr1[1] - (BYTE)cszStr2[1];
                    }
                    return (BYTE)cszStr1[0] - (BYTE)cszStr2[0];
                }
            }
            else{
                break;
            }
        }
        else{
            if(kbIsLeadByte(*cszStr2)){
                break;
            }
            int cmp = (BYTE)(*cszStr1) - (BYTE)(*cszStr2);
            if(cmp){
                return cmp;
            }
            cszStr1++;
            cszStr2++;
        }
    }
    return (BYTE)*cszStr1 - (BYTE)*cszStr2;
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
    if(ret1){
        ret1 /= abs(ret1);
    }
    if(ret2){
        ret2 /= abs(ret2);
    }
    if(ret1 != ret2){
        if(!ret1 || !ret2){
        //致命的エラー
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
    while(*cszStr1){
        if(!*cszStr2){
            break;
        }
        if(kbIsLeadByte(*cszStr1)){
            if(kbIsLeadByte(*cszStr2)){
                int cmp = *(WORD*)cszStr1 - *(WORD*)cszStr2;
                if(cmp == 0 && cszStr1[1]){
                    cszStr1 += 2;
                    cszStr2 += 2;
                }
                else{
                    if(*cszStr1 == *cszStr2){
                        return (BYTE)cszStr1[1] - (BYTE)cszStr2[1];
                    }
                    return (BYTE)cszStr1[0] - (BYTE)cszStr2[0];
                }
            }
            else{
                break;
            }
        }
        else{
            if(kbIsLeadByte(*cszStr2)){
                break;
            }
            int cmp = kbToLower(*cszStr1) - kbToLower(*cszStr2);
            //int cmp = tolower(*cszStr1) - tolower(*cszStr2);
            if(cmp){
                return cmp;
            }
            cszStr1++;
            cszStr2++;
        }
    }
    return (BYTE)*cszStr1 - (BYTE)*cszStr2;
}
///////////////////////////////////////////////////////////////////////////////
int __fastcall kbStrNCmp(const char *cszStr1, const char *cszStr2, int nLen)
{
    while(nLen > 0 && *cszStr1){
        if(!*cszStr2){
            return 0;
        }
        if(kbIsLeadByte(*cszStr1)){
            if(kbIsLeadByte(*cszStr2)){
                int cmp = *(WORD*)cszStr1 - *(WORD*)cszStr2;
                if(cmp == 0 && cszStr1[1]){
                    cszStr1 += 2;
                    cszStr2 += 2;
                    nLen -= 2;
                }
                else{
                    if(*cszStr1 == *cszStr2){
                        return (BYTE)cszStr1[1] - (BYTE)cszStr2[1];
                    }
                    return (BYTE)cszStr1[0] - (BYTE)cszStr2[0];
                }
            }
            else{
                break;
            }
        }
        else{
            if(kbIsLeadByte(*cszStr2)){
                break;
            }
            int cmp = (BYTE)(*cszStr1) - (BYTE)(*cszStr2);
            //int cmp = tolower(*cszStr1) - tolower(*cszStr2);
            if(cmp){
                return cmp;
            }
            cszStr1++;
            cszStr2++;
            nLen--;
        }
    }
    if(nLen > 0){
        return (BYTE)*cszStr1 - (BYTE)*cszStr2;
    }
    else{
        return 0;
    }
}
///////////////////////////////////////////////////////////////////////////////
int __fastcall kbStrNCmpI(const char *cszStr1, const char *cszStr2, int nLen)
{
    while(nLen > 0 && *cszStr1){
        if(!*cszStr2){
            return 0;
        }
        if(kbIsLeadByte(*cszStr1)){
            if(kbIsLeadByte(*cszStr2)){
                int cmp = *(WORD*)cszStr1 - *(WORD*)cszStr2;
                if(cmp == 0 && cszStr1[1]){
                    cszStr1 += 2;
                    cszStr2 += 2;
                    nLen -= 2;
                }
                else{
                    if(*cszStr1 == *cszStr2){
                        return (BYTE)cszStr1[1] - (BYTE)cszStr2[1];
                    }
                    return (BYTE)cszStr1[0] - (BYTE)cszStr2[0];
                }
            }
            else{
                break;
            }
        }
        else{
            if(kbIsLeadByte(*cszStr2)){
                break;
            }
            int cmp = kbToLower(*cszStr1) - kbToLower(*cszStr2);
            //int cmp = tolower(*cszStr1) - tolower(*cszStr2);
            if(cmp){
                return cmp;
            }
            cszStr1++;
            cszStr2++;
            nLen--;
        }
    }
    if(nLen > 0){
        return (BYTE)*cszStr1 - (BYTE)*cszStr2;
    }
    else{
        return 0;
    }
}
///////////////////////////////////////////////////////////////////////////////
const char* __fastcall kbStrChr(const char *cszString, char cFind)
{//文字列 cszString から 文字 cFind を検索し、最初に見つかった文字へのポインタを返す
 //見つからない場合は NULL を返す
 //cFind は '\0' も有効
 //cFind == '\0' で文字列が途切れているとき、途切れた最初の文字へのポインタを返す
    if(!cszString)return NULL;
    while(1){
        if((BYTE)*cszString == (BYTE)cFind){
            return cszString;
        }
        else if(kbIsLeadByte((BYTE)*cszString)){
            if(cszString[1]){
                cszString += 2;
            }
            else if(cFind){//文字列が途切れている
                return NULL;
            }
            else{//途切れた最初の文字へのポインタ
                return cszString;
            }
        }
        else if(*cszString){
            cszString++;
        }
        else{
            return NULL;
        }
    }
}
///////////////////////////////////////////////////////////////////////////////
const char* __fastcall kbStrRChr(const char *cszString, char cFind)
{//文字列 cszString から 文字 cFind を検索し、最後に見つかった文字へのポインタを返す
 //見つからない場合は NULL を返す
 //cFind は '\0' も有効
 //cFind == '\0' で文字列が途切れているとき、途切れた最初の文字へのポインタを返す
    if(!cszString)return NULL;
    const char *cszRet = NULL;
    while(1){
        if((BYTE)*cszString == (BYTE)cFind){
            cszRet = cszString;
        }
        else if(kbIsLeadByte((BYTE)*cszString)){
            if(cszString[1]){
                cszString += 2;
                continue;
            }
            else if(cFind){//文字列が途切れている
                return cszRet;
            }
            else{//途切れた最初の文字へのポインタ
                return cszString;
            }
        }
        if(*cszString){
            cszString++;
        }
        else{
            break;
        }
    }
    return cszRet;
}
///////////////////////////////////////////////////////////////////////////////
const char* __fastcall kbStrStr(const char *cszString, const char *cszSearch)
{
    int nSearchLen = -1;
    while(*cszString){
        if(*cszString == *cszSearch){
            if(nSearchLen < 0){
                nSearchLen = kbStrLen(cszSearch);
            }
            //if(strncmp(cszString, cszSearch, nSearchLen) == 0){
            if(kbStrNCmp(cszString, cszSearch, nSearchLen) == 0){
                return cszString;
            }
        }
        if(kbIsLeadByte(*cszString)){
            if(!cszString[1]){
                break;
            }
            cszString += 2;
        }
        else{
            cszString++;
        }
    }
    return NULL;
}
///////////////////////////////////////////////////////////////////////////////
const char* __fastcall kbStrStrI(const char *cszString, const char *cszSearch)
{
    int nSearchLen = kbStrLen(cszSearch);
    while(*cszString){
        //if(strnicmp(cszString, cszSearch, nSearchLen) == 0){
        //    return cszString;
        //}
        if(kbStrNCmpI(cszString, cszSearch, nSearchLen) == 0){
            return cszString;
        }
        if(kbIsLeadByte(*cszString)){
            if(!cszString[1]){
                break;
            }
            cszString += 2;
        }
        else{
            cszString++;
        }
    }
    return NULL;
}
///////////////////////////////////////////////////////////////////////////////
char* __fastcall kbStrUpr(char *pszString)
{
    char *p = pszString;
    while(*p){
        *p = kbToUpper(*p);
        p++;
    }
    return pszString;
}
///////////////////////////////////////////////////////////////////////////////
char* __fastcall kbStrLwr(char *pszString)
{
    char *p = pszString;
    while(*p){
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
    if(p){
        while(*p){
            if(kbIsLeadByte(*p)){
                if(!p[1]){//文字列が途切れている
                    *p = 0;
                    return pszString;
                }
                p += 2;
                continue;
            }
            const char *delim = cszDelimiter;
            while(*delim){
                if(*p == *delim++){
                    *p++ = 0;
                    *ppszNext = p;
                    return pszString;
                }
            }
            p++;
        }
        if(*pszString){
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
{//文字列を整数に変換できた場合はその値を返す
 //変換できない場合は nDefault を返す
    int nSign = 1;//符号
    //空白をとばす
	while(*p == ' '){
        p++;
    }
    //符号
	switch(*p){
		case '-':
			nSign = -1;
            p++;
            break;
		case '+':
			p++;
            break;
	}
    if(is_digit(*p)){
        //整数部分
        int nReturn = (*p++ - '0');
        while(is_digit(*p)){
            nReturn *= 10;
            nReturn += (*p++ - '0');
        }
        //小数部分
        if(*p == '.'){
            while(is_digit(*++p));//小数部分はすべて無視
        }
        //数値の部分以降に空白以外の文字が見つかった場合は nDefault を返す
    	while(*p == ' '){
            p++;
        }
        if(!*p){
            return nSign*nReturn;
        }
    }
    return nDefault;
}
double __fastcall kbStrToDoubleDef(const char* p, double nDefault)
{//文字列を小数に変換できた場合はその値を返す
 //変換できない場合は nDefault を返す
    int nSign = 1;//符号
    //空白をとばす
	while(*p == ' '){
        p++;
    }
    //符号
	switch(*p){
		case '-':
			nSign = -1;
            p++;
            break;
		case '+':
			p++;
            break;
	}
    if(is_digit(*p)){
        //整数部分
        double nReturn = (*p++ - '0');
        while(is_digit(*p)){
            nReturn *= 10;
            nReturn += (*p++ - '0');
        }
        //小数部分
        if(*p == '.'){
            int multi = 1;
	    ++p;
	    while(is_digit(*p)){
	      multi *= 10;
	      nReturn += (double)(*p++ - '0') / multi;
	    }
        }
        //数値の部分以降に空白以外の文字が見つかった場合は nDefault を返す
    	while(*p == ' '){
            p++;
        }
        if(!*p){
            return nSign*nReturn;
        }
    }
    return nDefault;
}
#else
int __fastcall kbStrToIntDef(const char* cszStr, int nDefault)
{//cszStr を整数に変換する
 //変換できない場合は nDefault を返す
    if(!cszStr){
        return nDefault;
    }
    const int cMaxLen = 1024;
    const int len = kbStrLen(cszStr) + 1;
    if(len > cMaxLen){
        return nDefault;//文字列が長すぎる
    }
    char szCopy[cMaxLen];
    memcpy(szCopy, cszStr, len);
    kbStrTrim(szCopy);
    int ret;
    if(szCopy[0]){
        char *szEndStr = NULL;
        ret = strtol(szCopy, &szEndStr, 10);
        if(szEndStr && szEndStr[0]){
            ret = nDefault;
        }
    }
    else{
        ret = nDefault;
    }
    return ret;
}
double __fastcall kbStrToDoubleDef(const char* cszStr, double nDefault)
{//cszStr を小数に変換する
 //変換できない場合は nDefault を返す
    if(!cszStr){
        return nDefault;
    }
    const int cMaxLen = 1024;
    const int len = kbStrLen(cszStr) + 1;
    if(len > cMaxLen){
        return nDefault;//文字列が長すぎる
    }
    char szCopy[cMaxLen];
    memcpy(szCopy, cszStr, len);
    kbStrTrim(szCopy);
    int ret;
    if(szCopy[0]){
        char *szEndStr = NULL;
        ret = strtof(szCopy, &szEndStr, 10);
        if(szEndStr && szEndStr[0]){
            ret = nDefault;
        }
    }
    else{
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
{//文字列 src の old_pattern を new_pattern に置換する
 //size は dst のバッファサイズ
 //old_pattern == "" （空文字列）の場合、kbStrLCat(dst, size, new_pattern) と同じ
 //バッファサイズが足りない場合は、size-1 バイトまで置換する
 //完全置換されたとしたときの文字列の長さを返す
 //戻り値 >= size の場合、置換後の文字列が一部切り捨てられたことを意味する
    int ret = 0;
    const int old_pattern_len = kbStrLen(old_pattern);
    while(1){
        if(ret < size &&
           *src == *old_pattern &&
           kbStrNCmp(src, old_pattern, old_pattern_len) == 0){
            const int new_pattern_len = kbStrLCpy(dst, new_pattern, size - ret);
            if(new_pattern_len < size - ret){
                dst += new_pattern_len;
            }
            else{
                dst += (size - ret);
            }
            ret += new_pattern_len;
            if(!*src){
                break;
            }
            src += old_pattern_len;
            continue;
        }
        if(kbIsLeadByte(*src)){
            if(src[1]){
                ret += 2;
                if(ret < size){
                    *((WORD*)dst) = *((WORD*)src);
                    dst += 2;
                }
                src += 2;
            }
            else{
                break;
            }
        }
        else if(*src){
            if(++ret < size){
                *dst++ = *src++;
            }
            else{
                src++;
            }
        }
        else{
            break;
        }
    }
    if(size > 0){
        *dst = 0;
    }
    return ret;
}
///////////////////////////////////////////////////////////////////////////////
int __fastcall kbMultiByteToWideChar(const char *cszMultiByte,
                                     wchar_t* wszWideChar,
                                     int cchWideChar)
{//cchWideChar == 0 のときは必要なバッファのサイズ(文字数単位)を返す
    int ret = MultiByteToWideChar(CP_ACP, 0, cszMultiByte, -1,
                                      wszWideChar, cchWideChar);
    //バッファサイズが足りない場合、wszWideChar の終端文字は \0 にならない
    if(cchWideChar){
        wszWideChar[cchWideChar-1] = 0;//終端を \0 にする
    }
    return ret;
}
///////////////////////////////////////////////////////////////////////////////
int __fastcall kbWideCharToMultiByte(const wchar_t *wcszWideChar,
                                     char *szMultiByte,
                                     int cchMultiByte)
{//cchMultiByte が 0 のときは必要なバッファのサイズを返す
    int ret = WideCharToMultiByte(CP_ACP, 0, wcszWideChar, -1,
                                szMultiByte, cchMultiByte, NULL, NULL);
    //バッファサイズが足りない場合、szMultiByte の終端文字は \0 にならない
    if(cchMultiByte){
        szMultiByte[cchMultiByte-1] = 0;//終端を \0 にする
        if(ret == 0){//サイズが足りない(↑で終端文字がマルチバイトの2バイト目だった場合に対処）
            int nLen = kbStrLen(szMultiByte);//文字列が途切れてる場合は有効な部分までの長さを返す
            szMultiByte[nLen] = 0;           //途切れてる場合は途切れた部分が除去される
        }
    }
    return ret;
}
///////////////////////////////////////////////////////////////////////////////
//
//ファイル名関係
//
///////////////////////////////////////////////////////////////////////////////
const char* __fastcall kbExtractFileExt(const char* cszSrc)
{//cszSrc 内の拡張子部分のポインタを返す
 //見つからない場合は空文字列を返す（NULL ではない）
    if(!cszSrc)return gcszEmpty;
    const BYTE* LastPeriod = NULL;
    const BYTE* LastPathDelimiter = NULL;
    while(*cszSrc){
        if(kbIsLeadByte((*((const BYTE*)cszSrc)))){
            if(cszSrc[1]){
                cszSrc+=2;
                continue;
            }
            else{//文字列が途中で途切れている
                break;
            }
        }
        if((BYTE)*cszSrc == (BYTE)'.'){
            LastPeriod = (BYTE*)cszSrc;
        }
        else if((BYTE)*cszSrc == (BYTE)'\\'){
            LastPathDelimiter = (BYTE*)cszSrc;
        }
        cszSrc++;
    }
    if(LastPeriod && (DWORD)LastPeriod > (DWORD)LastPathDelimiter){
        //ピリオドが見つかっても、拡張子とは限らない
        //例：C:\\hoge.hoge\\hogehoge
        return (const char*)LastPeriod;
    }
    return gcszEmpty;
}
///////////////////////////////////////////////////////////////////////////////
const char* __fastcall kbExtractFileName(const char* cszFileName)
{//cszFileName からパスを取り除いたもの（ファイル名部分）のポインタを返す
    const char *cszLastDelimiter = kbStrRChr(cszFileName, '\\');
    if(cszLastDelimiter){
        return cszLastDelimiter + 1;
    }
    if(cszFileName)return cszFileName;
    return gcszEmpty;
}
///////////////////////////////////////////////////////////////////////////////
char* __fastcall kbExtractFilePath(char *szDest, const char *cszFileName, int nSize)
{//cszFileName からパスを取得してポインタを返す
 //szDest == cszFileName であってはならない
    szDest[0] = 0;
    const char *cszLastPathDelimiter = kbStrRChr(cszFileName, '\\');
    if(cszLastPathDelimiter){
        int nLen = cszLastPathDelimiter - cszFileName + 1;
        if(nLen < nSize){
            memcpy(szDest, cszFileName, nLen);
            szDest[nLen] =0;
        }
    }
    return szDest;
}
///////////////////////////////////////////////////////////////////////////////
const char* __fastcall kbExtractFileNameInArc(const char *cszFileName)
{//書庫ファイル内のファイル名からパスを取り除いたものを返す
 //例１：C:\hoge\hoge.lzh>test.mid の戻り値 = test.mid
 //例２：C:\hoge\hoge.lzh>test/test2.mid の戻り値＝test2.mid
 //例３：C:\hoge\hoge.lzh>test/testzip.zip>test3.mid の戻り値 = test3.mid
 //例４：C:\hoge\hoge.lzh>test/testzip.zip>test/test4.mid の戻り値 = test4.mid
    if(!cszFileName)return gcszEmpty;
    const char *cszDelimiter = kbStrRChr(cszFileName, '>');
    if(cszDelimiter){
        cszFileName = cszDelimiter + 1;
    }
    cszDelimiter = kbStrRChr(cszFileName, '/');
    if(cszDelimiter){
        return cszDelimiter + 1;
    }
    return cszFileName;
}
///////////////////////////////////////////////////////////////////////////////
const char* __fastcall kbExtractFilePathInArc(char *szDest, const char *cszFileName)
{//書庫ファイル内のファイル名からファイル名を取り除いたものを返す
 //例１：C:\hoge\hoge.lzh>test.mid の戻り値 = C:\hoge\hoge.lzh>
 //例２：C:\hoge\hoge.lzh>test/test2.mid の戻り値 = C:\hoge\hoge.lzh>test/
 //例３：C:\hoge\hoge.lzh>test/testzip.zip>test3.mid の戻り値 = C:\hoge\hoge.lzh>test/testzip.zip>
 //例４：C:\hoge\hoge.lzh>test/testzip.zip>test/test4.mid の戻り値 = C:\hoge\hoge.lzh>test/testzip.zip>test/
    if(!szDest)return NULL;
    szDest[0] = 0;
    if(!cszFileName)return szDest;
    const char *cszFirst = cszFileName;
    const char *cszDelimiter = kbStrRChr(cszFileName, '>');
    if(cszDelimiter){
        cszFileName = cszDelimiter + 1;
    }
    cszDelimiter = kbStrRChr(cszFileName, '/');
    if(cszDelimiter){
        cszFileName = cszDelimiter + 1;
    }
    int nCopySize = cszFileName - cszFirst;
    if(nCopySize > 0){
        memcpy(szDest, cszFirst, nCopySize);
        szDest[nCopySize] = 0;
    }
    return szDest;
}
///////////////////////////////////////////////////////////////////////////////
char* __fastcall kbExtractReplacedFileExt(char *szDest,
                                          const char *cszFileName,
                                          const char *cszExt)
{//cszFileName の拡張子を cszExt に置き換えて szDest にコピーする
 //szDest のサイズは cszFileName と cszExt の合計より大きいことが前提
    if(!szDest){
        return NULL;
    }
    if(!cszExt){
        cszExt = gcszEmpty;
    }
    if(!cszFileName){
        strcpy(szDest, cszExt);
        return szDest;
    }
    const char *ext =  kbStrRChr(cszFileName, '.');
    const char *path = kbStrRChr(cszFileName, '\\');
    if(ext && ext > path){
        const int len = ext - cszFileName;
        memcpy(szDest, cszFileName, len);
        strcpy(szDest + len, cszExt);
    }
    else{
        strcpy(szDest, cszFileName);
        strcat(szDest, cszExt);
    }

    return szDest;
}
///////////////////////////////////////////////////////////////////////////////
void __fastcall kbAddPathDelimiter(char *szPath)
{//文字列の終端に \ を追加
 //すでに終端が \ の場合は何もしない
    char *p = szPath;
    char *last = p;
    //終端文字を探す
    while(*p){
        last = p;
        if(kbIsLeadByte(*p)){
            if(!p[1]){//文字列が途切れている
                *p = 0;
                p = last = szPath;
                continue;//最初からやり直す
            }
            p += 2;
        }
        else{
            p++;
        }
    }
    if(*last != '\\'){
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
    if(!p || p[1]){//\ が含まれない or 含むが終端ではない
        kbStrLCat(szDest, "\\", nDestSize);//kbAddPathDelimiter だとバッファサイズのチェックをしないので使用不可
    }
    kbStrLCat(szDest, cszAdd, nDestSize);
}
///////////////////////////////////////////////////////////////////////////////
void __fastcall kbRemovePathDelimiter(char *szPath)
{   //ドライブのルートでない場合は終端の \ を除く
    char *p = kbStrRChr(szPath, '\\');
    if(p && !p[1] && (p - szPath >= 3)){
        *p = 0;
    }
}
///////////////////////////////////////////////////////////////////////////////

