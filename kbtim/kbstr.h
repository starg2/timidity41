#ifndef kbstrH
#define kbstrH

extern BYTE g_byLeadByteTable[256];
extern BYTE g_byToLowerTable[256];
extern BYTE g_byToUpperTable[256];

#define kbIsLeadByte(X) g_byLeadByteTable[(BYTE)(X)]
#define kbToLower(X)    g_byToLowerTable[(BYTE)(X)]
#define kbToUpper(X)    g_byToUpperTable[(BYTE)(X)]

//�e�[�u��������
//�g�p�O�ɕK���P�x�����Ăяo��
void __fastcall kbStr_Initialize(void);
//
//������֌W

int __fastcall kbStrLen(const char *cszString);
//
// cszSrc �� szDest �� nBufSize �o�C�g���R�s�[
// ������̒���������Ȃ��ꍇ�� nBufSize-1 �o�C�g�R�s�[���ďI�[�� \0 ��t��
// �I�[�������r�؂�Ă���悤�ȏꍇ�ɂ��Ή�
// cszSrc �̒���(strlen(cszSrc)��Ԃ�
// �߂�l >= nBufSize �̏ꍇ�A�������؂�̂Ă����Ƃ��Ӗ�����
int __fastcall kbStrLCpy(char *szDest, const char *cszSrc, int nBufSize);

// cszSrc �� szDest �ɘA��
// nBufSize �� szDest �̃T�C�Y�i�I�[�� '\0' ���܂ށj
// �I�[�������r�؂�Ă���悤�ȏꍇ�ɂ��Ή�
// strlen(cszSrc) + min(strlen(szDest) + nBufSize) ��Ԃ�
// �߂�l >= nBufSize �̏ꍇ�A�A���������؂�̂Ă����Ƃ��Ӗ�����
int __fastcall kbStrLCat(char *szDest, const char *cszSrc, int nBufSize);

char* __fastcall kbStrTrimRight(char* szString);//�I�[�̋󔒕���������
char* __fastcall kbStrTrimLeft(char* szString); //�擪�̋󔒕���������
char* __fastcall kbStrTrim(char* szString);     //�擪�ƏI�[�̋󔒕���������
char* __fastcall kbCRLFtoSpace(char* szString); //���s�����ƃ^�u�������X�y�[�X�ɕϊ�

int __fastcall kbStrCmp(const char *cszStr1, const char *cszStr2);
int __fastcall kbStrCmpI(const char *cszStr1, const char *cszStr2);
int __fastcall kbStrNCmp(const char *cszStr1, const char *cszStr2, int nLen);
int __fastcall kbStrNCmpI(const char *cszStr1, const char *cszStr2, int nLen);

//������ cszString ���� ���� cFind ���������A�ŏ��Ɍ������������ւ̃|�C���^��Ԃ�
//������Ȃ��ꍇ�� NULL ��Ԃ�
const char* __fastcall kbStrChr(const char *cszString, char cFind);
inline char *__fastcall kbStrChr(char *szString, char cFind)
{
    return (char*) kbStrChr((const char*) szString, cFind);
}
//������ cszString ���� ���� cFind ���������A�Ō�Ɍ������������ւ̃|�C���^��Ԃ�
//������Ȃ��ꍇ�� NULL ��Ԃ�
const char* __fastcall kbStrRChr(const char *cszString, char cFind);
inline char *__fastcall kbStrRChr(char *szString, char cFind)
{
    return (char*) kbStrRChr((const char*) szString, cFind);
}

const char* __fastcall kbStrStr(const char *cszString, const char *cszSearch);
inline char *__fastcall kbStrStr(char *szString, const char *cszSearch)
{
    return (char*) kbStrStr((const char*) szString, cszSearch);
}
const char* __fastcall kbStrStrI(const char *cszString, const char *cszSearch);
inline char *__fastcall kbStrStrI(char *szString, const char *cszSearch)
{
    return (char*) kbStrStrI((const char*) szString, cszSearch);
}

char* __fastcall kbStrUpr(char *pszString);
char* __fastcall kbStrLwr(char *pszString);

char* __fastcall kbStrTok(char *pszString, const char *cszDelimiter, char **ppszNext);

int  __fastcall  kbStrToIntDef(const char* cszStr, int nDefault);
double __fastcall kbStrToDoubleDef(const char* cszStr, double nDefault);

//������ src �� old_pattern �� new_pattern �ɒu������
//size �� dst �̃o�b�t�@�T�C�Y
//old_pattern == "" �i�󕶎���j�̏ꍇ�AkbStrLCat(dst, size, new_pattern) �Ɠ���
//�o�b�t�@�T�C�Y������Ȃ��ꍇ�́Asize-1 �o�C�g�܂Œu������
//���S�u�����ꂽ�Ƃ����Ƃ��̕�����̒�����Ԃ�
//�߂�l >= size �̏ꍇ�A�u����̕����񂪈ꕔ�؂�̂Ă�ꂽ���Ƃ��Ӗ�����
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
//�t�@�C�����֌W
//
//cszSrc ���̊g���q�����ւ̃|�C���^��Ԃ�
//�g���q���Ȃ��ꍇ�͋󕶎����Ԃ�
//�i�߂�l�� NULL �łȂ����Ƃ��ۏ؂����j
const char* __fastcall kbExtractFileExt(const char* cszSrc);

//cszFileName ����p�X����菜�������́i�t�@�C���������j�̃|�C���^��Ԃ�
const char* __fastcall kbExtractFileName(const char* cszFileName);

//cszFileName ����p�X���擾���ă|�C���^��Ԃ�
char* __fastcall kbExtractFilePath(char *szDest, const char* cszFileName, int nSize);

//���Ƀt�@�C�����̃t�@�C��������p�X����菜�������̂�Ԃ�
//��P�FC:\hoge\hoge.lzh>test.mid �̖߂�l = test.mid
//��Q�FC:\hoge\hoge.lzh>test/test2.mid �̖߂�l��test2.mid
//��R�FC:\hoge\hoge.lzh>test/testzip.zip>test3.mid �̖߂�l = test3.mid
//��S�FC:\hoge\hoge.lzh>test/testzip.zip>test/test4.mid �̖߂�l = test4.mid
const char* __fastcall kbExtractFileNameInArc(const char *cszFileName);

//���Ƀt�@�C�����̃t�@�C��������t�@�C��������菜�������̂�Ԃ�
//��P�FC:\hoge\hoge.lzh>test.mid �̖߂�l = C:\hoge\hoge.lzh>
//��Q�FC:\hoge\hoge.lzh>test/test2.mid �̖߂�l = C:\hoge\hoge.lzh>test/
//��R�FC:\hoge\hoge.lzh>test/testzip.zip>test3.mid �̖߂�l = C:\hoge\hoge.lzh>test/testzip.zip>
//��S�FC:\hoge\hoge.lzh>test/testzip.zip>test/test4.mid �̖߂�l = C:\hoge\hoge.lzh>test/testzip.zip>test/
const char* __fastcall kbExtractFilePathInArc(char *szDest, const char *cszFileName);

//cszFileName �̊g���q�� cszExt �ɒu�������� szDest �ɃR�s�[����
char* __fastcall kbExtractReplacedFileExt(char *szDest,
                                          const char *cszFileName,
                                          const char *cszExt);

void __fastcall  kbAddPathDelimiter(char *szPath);
void __fastcall  kbRemovePathDelimiter(char *szPath);
void __fastcall  kbCombinePath(char *szDest, const char *cszSrc,
                               const char *cszAdd, int nDestSize);

#endif

