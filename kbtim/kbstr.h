#ifndef kbstrH
#define kbstrH

extern BYTE g_byLeadByteTable[256];
extern BYTE g_byToLowerTable[256];
extern BYTE g_byToUpperTable[256];

#define kbIsLeadByte(X) g_byLeadByteTable[(BYTE)(X)]
#define kbToLower(X)    g_byToLowerTable[(BYTE)(X)]
#define kbToUpper(X)    g_byToUpperTable[(BYTE)(X)]

//テーブル初期化
//使用前に必ず１度だけ呼び出す
void __fastcall kbStr_Initialize(void);
//
//文字列関係

int __fastcall kbStrLen(const char *cszString);
//
// cszSrc を szDest に nBufSize バイト分コピー
// 文字列の長さが足りない場合は nBufSize-1 バイトコピーして終端に \0 を付加
// 終端文字が途切れているような場合にも対応
// cszSrc の長さ(strlen(cszSrc)を返す
// 戻り値 >= nBufSize の場合、文字列を切り捨てたことを意味する
int __fastcall kbStrLCpy(char *szDest, const char *cszSrc, int nBufSize);

// cszSrc を szDest に連結
// nBufSize は szDest のサイズ（終端の '\0' を含む）
// 終端文字が途切れているような場合にも対応
// strlen(cszSrc) + min(strlen(szDest) + nBufSize) を返す
// 戻り値 >= nBufSize の場合、連結文字列を切り捨てたことを意味する
int __fastcall kbStrLCat(char *szDest, const char *cszSrc, int nBufSize);

char* __fastcall kbStrTrimRight(char* szString);//終端の空白文字を除去
char* __fastcall kbStrTrimLeft(char* szString); //先頭の空白文字を除去
char* __fastcall kbStrTrim(char* szString);     //先頭と終端の空白文字を除去
char* __fastcall kbCRLFtoSpace(char* szString); //改行文字とタブ文字をスペースに変換

int __fastcall kbStrCmp(const char *cszStr1, const char *cszStr2);
int __fastcall kbStrCmpI(const char *cszStr1, const char *cszStr2);
int __fastcall kbStrNCmp(const char *cszStr1, const char *cszStr2, int nLen);
int __fastcall kbStrNCmpI(const char *cszStr1, const char *cszStr2, int nLen);

//文字列 cszString から 文字 cFind を検索し、最初に見つかった文字へのポインタを返す
//見つからない場合は NULL を返す
const char* __fastcall kbStrChr(const char *cszString, char cFind);
inline char *__fastcall kbStrChr(char *szString, char cFind)
{
    return (char*)kbStrChr((const char*)szString, cFind);
}
//文字列 cszString から 文字 cFind を検索し、最後に見つかった文字へのポインタを返す
//見つからない場合は NULL を返す
const char* __fastcall kbStrRChr(const char *cszString, char cFind);
inline char *__fastcall kbStrRChr(char *szString, char cFind)
{
    return (char*)kbStrRChr((const char*)szString, cFind);
}

const char* __fastcall kbStrStr(const char *cszString, const char *cszSearch);
inline char *__fastcall kbStrStr(char *szString, const char *cszSearch)
{
    return (char*)kbStrStr((const char*)szString, cszSearch);
}
const char* __fastcall kbStrStrI(const char *cszString, const char *cszSearch);
inline char *__fastcall kbStrStrI(char *szString, const char *cszSearch)
{
    return (char*)kbStrStrI((const char*)szString, cszSearch);
}

char* __fastcall kbStrUpr(char *pszString);
char* __fastcall kbStrLwr(char *pszString);

char* __fastcall kbStrTok(char *pszString, const char *cszDelimiter, char **ppszNext);

int  __fastcall  kbStrToIntDef(const char* cszStr, int nDefault);
double __fastcall kbStrToDoubleDef(const char* cszStr, double nDefault);

//文字列 src の old_pattern を new_pattern に置換する
//size は dst のバッファサイズ
//old_pattern == "" （空文字列）の場合、kbStrLCat(dst, size, new_pattern) と同じ
//バッファサイズが足りない場合は、size-1 バイトまで置換する
//完全置換されたとしたときの文字列の長さを返す
//戻り値 >= size の場合、置換後の文字列が一部切り捨てられたことを意味する
int __fastcall kbStringReplace(char *dst,
                               int size,
                               const char *src,
                               const char *old_pattern,
                               const char *new_pattern);

int __fastcall kbMultiByteToWideChar(const char *cszMultiByte,
                                     wchar_t* wszWideChar,
                                     int cchWideChar);
int __fastcall kbWideCharToMultiByte(const wchar_t *wcszWideChar,
                                     char *szMultiByte,
                                     int cchMultiByte);
//
//ファイル名関係
//
//cszSrc 内の拡張子部分へのポインタを返す
//拡張子がない場合は空文字列を返す
//（戻り値は NULL でないことが保証される）
const char* __fastcall kbExtractFileExt(const char* cszSrc);

//cszFileName からパスを取り除いたもの（ファイル名部分）のポインタを返す
const char* __fastcall kbExtractFileName(const char* cszFileName);

//cszFileName からパスを取得してポインタを返す
char* __fastcall kbExtractFilePath(char *szDest, const char* cszFileName, int nSize);

//書庫ファイル内のファイル名からパスを取り除いたものを返す
//例１：C:\hoge\hoge.lzh>test.mid の戻り値 = test.mid
//例２：C:\hoge\hoge.lzh>test/test2.mid の戻り値＝test2.mid
//例３：C:\hoge\hoge.lzh>test/testzip.zip>test3.mid の戻り値 = test3.mid
//例４：C:\hoge\hoge.lzh>test/testzip.zip>test/test4.mid の戻り値 = test4.mid
const char* __fastcall kbExtractFileNameInArc(const char *cszFileName);

//書庫ファイル内のファイル名からファイル名を取り除いたものを返す
//例１：C:\hoge\hoge.lzh>test.mid の戻り値 = C:\hoge\hoge.lzh>
//例２：C:\hoge\hoge.lzh>test/test2.mid の戻り値 = C:\hoge\hoge.lzh>test/
//例３：C:\hoge\hoge.lzh>test/testzip.zip>test3.mid の戻り値 = C:\hoge\hoge.lzh>test/testzip.zip>
//例４：C:\hoge\hoge.lzh>test/testzip.zip>test/test4.mid の戻り値 = C:\hoge\hoge.lzh>test/testzip.zip>test/
const char* __fastcall kbExtractFilePathInArc(char *szDest, const char *cszFileName);

//cszFileName の拡張子を cszExt に置き換えて szDest にコピーする
char* __fastcall kbExtractReplacedFileExt(char *szDest,
                                          const char *cszFileName,
                                          const char *cszExt);

void __fastcall  kbAddPathDelimiter(char *szPath);
void __fastcall  kbRemovePathDelimiter(char *szPath);
void __fastcall  kbCombinePath(char *szDest, const char *cszSrc,
                               const char *cszAdd, int nDestSize);

#endif

