#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NON_CONFORMING_SWPRINTFS
#endif
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#ifdef __W32__
#include <windows.h>
#endif
#include "timidity.h"
#include "common.h"
//#include <cstdio>
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif /* HAVE_MEMORY_H */
#include <locale.h>

#include <windows.h>
#include <tchar.h>
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "winmm.lib")
#ifdef _DEBUG
//#	define _CRTDBG_MAP_ALLOC
#	include <crtdbg.h>
#endif

#include "myini.h"

//extern struct tagKey;
//extern struct tagSection;

/*
typedef struct tagKey {
	uint32 Hash;
	char *Name;
	char *Data;
	struct tagKey *Next;
} INIKEY, *LPINIKEY;

typedef struct tagSection {
	uint32 Hash;
	char *Name;
	LPINIKEY Key;
	struct tagSection *Next;
} INISEC, *LPINISEC;

typedef struct tagini {
//	int i;
	LPINISEC Sec;
} INIDATA;
*/

/*
  ハッシュを作成する
  引数:
    str: 文字列
  戻り値
    ハッシュ
*/
static UINT32 CreateHash(const char *str)
{
	const UINT8 *p = (const UINT8*)str;
	uint32 Key = 0;
	while (*p) {
		if (*p < 0x81)		Key += *p++;
		else if (*p < 0xA0)	Key += *(const UINT16*)p++;
		else if (*p < 0xE0)	Key += *p++;
		else if (*p < 0xFF)	Key += *(const UINT16*)p++;
		else				Key += *p++;
	}
	return Key;
}
/*
  strlen
*/
static size_t MyStrLen(const char *str)
{
	const UINT8 *p = (const UINT8*)str;
	uint32 r = 0;
	while (*p) {
		if (*p < 0x81u)		++p, ++r;
		else if (*p < 0xA0u)	(void)*(const UINT16*)p++, r += 2;
		else if (*p < 0xE0u) ++p, ++r;
		else if (*p < 0xFFu)	(void)*(const UINT16*)p++, r += 2;
		else				++p, ++r;
	}
	return r;
}

extern int MyIni_DeleteSection(INIDATA *Ini, const char *SecName);
extern LPINISEC SearchSection(INIDATA *Ini, const char *Section, int AutoCreateEnable);
extern LPINIKEY SearchKey(LPINISEC Sec, const char *KeyName, int AutoCreateEnable);

extern char *MyIni_GetString(INISEC *Sec, const char *KeyName, char *Buf, size_t Size, const char *DefParam);
extern void MyIni_SetString2(INIDATA *Ini, const char *SecName, const char *KeyName, const char *Data);

#define TRUE 1
#define FALSE 0

UINT32 g_memref = 0;

UINT32 MyIni_CheckMemLeak()
{
	return g_memref;
}

void *myMemAlloc(size_t s)
{
#if defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(WIN64)
	void *p = HeapAlloc(GetProcessHeap(), 0, s);
#else
	void *p = malloc(s);
#endif
	if (!p) {
		printf("error: malloc failed.\n");
		return NULL;
	}
	++g_memref;
//	printf("MemAlloc: %02d %p\n", g_memref, p);
	return p;
}

void *myMemCalloc(size_t n, size_t s)
{
#if defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(WIN64)
	void *p = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, s * n);
#else
	void *p = calloc(n, s);
#endif
	if (!p) {
		printf("error: calloc failed.\n");
		return NULL;
	}
	++g_memref;
//	printf("MemAlloc: %02d %p\n", g_memref, p);
	return p;
}

void myMemFree(void **p)
{
	if (!*p) return;
//	printf("MemFree : %02d %p\n", g_memref, *p);
#if defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(WIN64)
	HeapFree(GetProcessHeap(), 0, *p);
#else
	safe_free(*p);
#endif
	*p = NULL;
	--g_memref;
}

/*
void myMemRealloc(void **p, size_t s)
{
	if (!p) {
		*p = malloc(s);
		++g_memref;
		return;
	}
	safe_free(*p);
	*p = malloc(s);
}
*/


static LPINISEC PushSection(const char *Section)
{
	LPINISEC Sec = (LPINISEC) myMemCalloc(1, sizeof(INISEC));
	Sec->Name = (char*) myMemAlloc((MyStrLen(Section) + 4) * sizeof(char));
	strcpy(Sec->Name, Section);
	Sec->Hash = CreateHash(Sec->Name);
	return Sec;
}

/*-------------------------------------------------------------
  公開関数
  指定した Section を検索する
  引数:
    Ini: INIDATA のぽいんた
	SecName: 検索したいセクション名
	AutoCreateEnable: Sectionがなかった場合自動的に作成する
  戻り値
    見つかった Section の ポインタ
---------------------------------------------------------------*/
LPINISEC SearchSection(INIDATA *Ini, const char *Section, int AutoCreateEnable)
{
	LPINISEC Sec = Ini->Sec;
	uint32 SearchHash = 0;

	if (!Ini->Sec) {
		if (AutoCreateEnable) {
			Ini->Sec = PushSection(Section);
			return Ini->Sec;
		}
		return NULL;
	}

	SearchHash = CreateHash(Section);

	do {
		if (Sec->Hash == SearchHash) {
			if (strcmp(Sec->Name, Section) == 0) {
				return Sec;
			}
		}
		if (!Sec->Next) break;
		Sec = Sec->Next;
	} while (1);

	if (AutoCreateEnable) {
		Sec->Next = PushSection(Section);
		return Sec->Next;
	}
	return NULL;
}

/*-------------------------------------------------------------
  非公開関数
  Key を生成する
  引数:
    KeyName: 生成名
  戻り値
    確保したポインタ
-------------------------------------------------------------*/
LPINIKEY PushKey(const char *KeyName)
{
	LPINIKEY Key;
	Key = (LPINIKEY) myMemCalloc(1, sizeof(INIKEY));
	Key->Name = (char*) myMemAlloc((MyStrLen(KeyName) + 4) * sizeof(char));
	strcpy(Key->Name, KeyName);
	Key->Hash = CreateHash(Key->Name);

	return Key;
}

/*-------------------------------------------------------------
  公開関数
  指定した Key を検索する
  引数:
    Sec: Section のぽいんた
	KeyName: 検索したいキー名
	AutoCreateEnable: Key がなかった場合自動的に作成する
  戻り値
    見つかった Key の ポインタ
-------------------------------------------------------------*/
LPINIKEY SearchKey(LPINISEC Sec, const char *KeyName, int AutoCreateEnable)
{
	LPINIKEY Key = Sec->Key;
	uint32 SearchHash = 0;

	if (!Sec->Key) {
		if (AutoCreateEnable) {
			Sec->Key = PushKey(KeyName);
			return Sec->Key;
		}
		return NULL;
	}

	SearchHash = CreateHash(KeyName);

	do {
		if (Key->Hash == SearchHash) {
			if (strcmp(Key->Name, KeyName) == 0) {
				return Key;
			}
		}

		if (!Key->Next) break;
		Key = Key->Next;
	} while (1);

	if (AutoCreateEnable) {
		Key->Next = PushKey(KeyName);
		return (LPINIKEY)Key->Next;
	}
	return NULL;
}

/*-------------------------------------------------------------
  非公開関数
  KeyData を生成する
  引数:
    KeyName: Key名
	Data: data
  戻り値
    確保したポインタ
-------------------------------------------------------------*/
void CreateKeyData(LPINIKEY Key, const char *Data)
{
	if (!Key->Data) {
		if (Data) {
			Key->Data = (char*) myMemAlloc((strlen(Data) + 4) * sizeof(char));
			strcpy(Key->Data, Data);
		} else {
			Key->Data = NULL;
		}
		return;
	}

	myMemFree((void**)&Key->Data);
	if (Data) {
		Key->Data = (char*) myMemAlloc((strlen(Data) + 4) * sizeof(char));
		strcpy(Key->Data, Data);
	} else {
		Key->Data = NULL;
	}
}

/*-------------------------------------------------------------
  公開関数
  Ini をLoad する
-------------------------------------------------------------*/
static void MyIni_LoadF(INIDATA *Ini, FILE *fp)
{
	char *ptok = NULL;
	char buf[2048] = "";
	LPINISEC nowSec = NULL;
	LPINIKEY Key = NULL;
	char *pbf = NULL;

	if (!fp) {
		return;
	}

	while (fgets(buf, 2000, fp)) {

		pbf = buf;

		while (*pbf == ' ' || *pbf == '\t') { ++pbf; }
		if (*pbf == '[') {
			*strtok(pbf, "]") = 0;
			nowSec = SearchSection(Ini, pbf + 1, TRUE);
			continue;
		}

		if ((ptok = strstr(pbf, "\n"))!= NULL)
			*ptok = 0;

		if (*pbf == ';') {
			char *pcm = strtok(pbf, "; ");
//			Key = SearchKey(nowSec, pcm, TRUE);
//			CreateKeyData(Key, NULL);
			if (pcm) {
				Key = SearchKey(nowSec, pcm, TRUE);
				CreateKeyData(Key, NULL);
			}
			continue;
		}

		if (strcmp(pbf, "") == 0)
			continue;

		ptok = strtok(pbf, "=");
		Key = SearchKey(nowSec, ptok, TRUE);
		ptok = strtok(NULL, "=");

		CreateKeyData(Key, ptok);
	}

	fclose(fp);
}

void MyIni_Load(INIDATA *Ini, const char *str)
{
	TCHAR *tstr = char_to_tchar(str);
	MyIni_LoadF(Ini, _tfopen(tstr, _T("r")));
	safe_free(tstr);
}

void MyIni_LoadT(INIDATA *Ini, const TCHAR *str)
{
	MyIni_LoadF(Ini, _tfopen(str, _T("r")));
}

/*-------------------------------------------------------------
  公開関数
  Ini をLoad する
  timidity_file対応版 , CFG dir から検索
  saveのときはabs_path使用するので loadのみでいいはず
  tf_gets()が_fgets()と同じ動作かは不明
-------------------------------------------------------------*/
void MyIni_Load_timidity(INIDATA *Ini, const char *str, int decompress, int noise_mode)
{
	char *ptok = NULL;
	char buf[2048] = "";
	LPINISEC nowSec = NULL;
	LPINIKEY Key = NULL;
	char *pbf = NULL;
	struct timidity_file *tf = open_file((char *)str, decompress, noise_mode);

	if (tf == NULL) {
		return;
	}

	while (tf_gets(buf, 2000, tf)) {

		pbf = buf;

		while (*pbf == ' ' || *pbf == '\t') { ++pbf; }
		if (*pbf == '[') {
			*strtok(pbf, "]") = 0;
			nowSec = SearchSection(Ini, pbf + 1, TRUE);
			continue;
		}

		if ((ptok = strstr(pbf, "\n"))!= NULL)
			*ptok = 0;

		if (*pbf == ';') {
			char *pcm = strtok(pbf, "; ");
//			Key = SearchKey(nowSec, pcm, TRUE);
//			CreateKeyData(Key, NULL);
			if (pcm) {
				Key = SearchKey(nowSec, pcm, TRUE);
				CreateKeyData(Key, NULL);
			}
			continue;
		}

		if (strcmp(pbf, "") == 0)
			continue;

		ptok = strtok(pbf, "=");
		Key = SearchKey(nowSec, ptok, TRUE);
		ptok = strtok(NULL, "=");

		CreateKeyData(Key, ptok);
	}
	close_file(tf);
}

/*-------------------------------------------------------------
  公開関数
  Ini を Save する
-------------------------------------------------------------*/
static void MyIni_SaveF(INIDATA *Ini, FILE *fp)
{
	LPINISEC Sec = Ini->Sec;
	LPINIKEY Key;
	if (!fp) return;

	while (Sec) {
		Key = Sec->Key;
		fprintf(fp, "[%s]\n", Sec->Name);
		Sec = Sec->Next;


		while (Key) {
			if (Key->Data) {
				fprintf(fp, "%s=%s\n", Key->Name, Key->Data);
			} else { fprintf(fp, "; %s\n", Key->Name); }
			Key = Key->Next;
		}
		fprintf(fp, "\n");
	}
	fclose(fp);
}

void MyIni_Save(INIDATA *Ini, const char *fn)
{
	TCHAR *tfn = char_to_tchar(fn);
	MyIni_SaveF(Ini, _tfopen(tfn, _T("w")));
	safe_free(tfn);
}

void MyIni_SaveT(INIDATA *Ini, const TCHAR *fn)
{
	MyIni_SaveF(Ini, _tfopen(fn, _T("w")));
}

/*-------------------------------------------------------------
  公開関数
  指定したSection の Key をすべて消去する
-------------------------------------------------------------*/
void KeyAllClear(LPINISEC Sec)
{
	LPINIKEY Key;
	LPINIKEY SaveKey;
	SaveKey = Key = Sec->Key;
	while (Key) {
		SaveKey = Key;
		Key = Key->Next;

		myMemFree((void**)&SaveKey->Name);
		myMemFree((void**)&SaveKey->Data);
		myMemFree((void**)&SaveKey);
	}
}

/*-------------------------------------------------------------
  公開関数
  すべての Section をメモリー上から削除する
  引数:
    Ini: INIDATA のぽいんた
	SecName: 消去したいセクション名
-------------------------------------------------------------*/
void MyIni_SectionAllClear(INIDATA *Ini)
{
	LPINISEC Sec;
	LPINISEC SaveSec;
	SaveSec = Sec = Ini->Sec;
	while (Sec) {
		SaveSec = Sec;
		Sec = Sec->Next;

		KeyAllClear(SaveSec);
		myMemFree((void**)&SaveSec->Name);
		myMemFree((void**)&SaveSec);
	}
	Ini->Sec = NULL;
}

/*-------------------------------------------------------------
  公開関数
  指定した Section をメモリー上から削除する
  引数:
    Ini: INIDATA のぽいんた
	SecName: 消去したいセクション名
-------------------------------------------------------------*/
int MyIni_DeleteSection(INIDATA *Ini, const char *SecName)
{
	LPINISEC Sec = Ini->Sec;
	LPINISEC SaveSec = Ini->Sec;
	LPINISEC BackSec = Ini->Sec;

	while (Sec) {
		if (strcmp(Sec->Name, SecName) == 0) {
			SaveSec = Sec->Next;
			KeyAllClear(Sec);
			myMemFree((void**)&Sec->Name);
			myMemFree((void**)&Sec);
			BackSec->Next = SaveSec;

			return 1;
		}
		BackSec = Sec;
		Sec = Sec->Next;
	}
	return 0;
}

/*-------------------------------------------------------------
  公開関数
  キーを削除する
-------------------------------------------------------------*/
int MyIni_DeleteKey(INIDATA *Ini, const char *SecName, const char *KeyName)
{
	LPINISEC Sec = NULL;
	LPINIKEY Key = NULL;
	LPINIKEY SaveKey = NULL;
	LPINIKEY BackKey = NULL;
	if ((Sec = SearchSection(Ini, SecName, FALSE)) == NULL)
		return 0;

	if (!Sec->Key) return 0;

//	BackKey = Sec->Key;
	Key = Sec->Key;

	while (Key) {
		if (strcmp(Key->Name, KeyName) == 0) {
			SaveKey = Key->Next;
			myMemFree((void**)&Key->Data);
			myMemFree((void**)&Key->Name);
			myMemFree((void**)&Key);
			BackKey->Next = SaveKey;
			return 1;
		}
		BackKey = Key;
		Key = Key->Next;
	}

	return 0;
}

/*-------------------------------------------------------------
  公開関数
  セクションを検索する
-------------------------------------------------------------*/
int MyIni_SectionExists(INIDATA *Ini, const char *SecName)
{
	return SearchSection(Ini, SecName, FALSE) ? 1 : 0;
}

/*-------------------------------------------------------------
  公開関数
  キーを検索する
-------------------------------------------------------------*/
int MyIni_KeyExists(INIDATA *Ini, const char *SecName, const char *KeyName)
{
	LPINISEC Sec = NULL;

	if ((Sec = SearchSection(Ini, SecName, FALSE)) == NULL)
		return 0;
	return SearchKey(Sec, KeyName, FALSE) ? 1 : 0;
}


char *MyIni_GetString2(INIDATA *Ini, const char *Section, const char *KeyName, char *Buf, size_t Size, const char *DefParam)
{
	LPINIKEY Key = NULL;
	INISEC *Sec = SearchSection(Ini, Section, FALSE);

	if (!Sec)
		return strncpy(Buf, DefParam, Size);
	if (!Sec->Key)
		return strncpy(Buf, DefParam, Size);

	if ((Key = SearchKey(Sec, KeyName, FALSE)) == NULL)
		return strncpy(Buf, DefParam, Size);

	return strncpy(Buf, Key->Data, Size);
}

char *MyIni_GetString(INISEC *Sec, const char *KeyName, char *Buf, size_t Size, const char *DefParam)
{
	LPINIKEY Key = NULL;

	if (!Sec)
		return strncpy(Buf, DefParam, Size);
	if (!Sec->Key)
		return strncpy(Buf, DefParam, Size);

	if ((Key = SearchKey(Sec, KeyName, FALSE)) == NULL)
		return strncpy(Buf, DefParam, Size);

	return strncpy(Buf, Key->Data, Size);
}

void MyIni_SetString2(INIDATA *Ini, const char *SecName, const char *KeyName, const char *Data)
{
	LPINIKEY Key = NULL;
	LPINISEC Sec = Ini->Sec;

	if (!Sec) {
		Ini->Sec = Sec = PushSection(SecName);
		Sec->Key = PushKey(KeyName);
		CreateKeyData(Sec->Key, Data);
		return;
	}

	Sec = SearchSection(Ini, SecName, TRUE);
	Key = SearchKey(Sec, KeyName, TRUE);
	CreateKeyData(Key, Data);
}

void MyIni_SetString(INISEC *Sec, const char *KeyName, const char *Data)
{
	LPINIKEY Key = NULL;

	if (!Sec) {
		Sec->Key = PushKey(KeyName);
		CreateKeyData(Sec->Key, Data);
		return;
	}

	Key = SearchKey(Sec, KeyName, TRUE);
	CreateKeyData(Key, Data);
}
/*
typedef enum Enum_MyIni_DataType
{
	EMYDTYPE_INT,

	EMYDTYPE_UINT,

	EMYDTYPE_DOUBLE,

	EMYDTYPE_ADDR,
	EMYDTYPE_BIN,

	EMYDTYPE_STR,
} MyIni_DataType;
*/
void MyIni_SetParam(INISEC *Sec, const char *KeyName, UINT64 Param, size_t Size, MyIni_DataType DataType)
{
	char bf[2048] = "";

	switch (DataType) {
		case EMYDTYPE_INT:
			_i64toa(Param, bf, 10);
			MyIni_SetString(Sec, KeyName, bf);
			return;
		case EMYDTYPE_UINT:
			_ui64toa(Param, bf, 10);
			MyIni_SetString(Sec, KeyName, bf);
			return;

		case EMYDTYPE_DOUBLE:
			sprintf(bf, "%f", *((double*)&Param));
			MyIni_SetString(Sec, KeyName, bf);
			return;

		case EMYDTYPE_ADDR:
			_ui64toa(Param, bf, 16);
			MyIni_SetString(Sec, KeyName, bf);
			return;

		case EMYDTYPE_BIN: {
			size_t i = 0;
			char *hexc = { "0123456789ABCDEF" };
			unsigned char *p = (unsigned char*)((u_ptr_size_t)Param);
			char *bfm = (char*) myMemAlloc((Size + 4) * 2 * sizeof(char));
			for (i = 0; i < Size; i++) {
				bfm[(i << 1)] = hexc[p[i] / 16];
				bfm[(i << 1) + 1] = hexc[p[i] % 16];
			}
			bfm[i] = 0;
			MyIni_SetString(Sec, KeyName, bfm);
			myMemFree((void**)&bfm);
			return;
		}
		case EMYDTYPE_STR:
			MyIni_SetString(Sec, KeyName, (char*)((u_ptr_size_t)Param));
			return;
	}
}

char CharToNum(char x)
{
	if (x >= '0' && x <= '9') return x - '0';
	if (x >= 'A' && x <= 'Z') return x - 'A' + 10;
	if (x >= 'a' && x <= 'z') return x - 'a' + 10;
	return 0;
}

const void *MyIni_GetParam(INISEC *Sec, const char *KeyName, void *bf, size_t Size, MyIni_DataType DataType)
{
	switch (DataType) {
		case EMYDTYPE_UINT:{
			char tbf[258] = "";
			MyIni_GetString(Sec, KeyName, tbf, 256, "");
			if (strcmp(tbf, "") == 0) return NULL;
			switch (Size) {
			case 1:
				*((char*)bf) = (char)strtoul(tbf, NULL, 10);
				return bf;
			case 2:
				*((short*)bf) = (short)strtoul(tbf, NULL, 10);
				return bf;
			case 4:
				*((long*)bf) = (long)strtoul(tbf, NULL, 10);
				return bf;
			case 8:
				*((__int64*)bf) = _strtoui64(tbf, NULL, 10);
				return bf;
			}
			return NULL;
		}
		case EMYDTYPE_INT: {
			char tbf[258] = "";
			MyIni_GetString(Sec, KeyName, tbf, 256, "");
			if (strcmp(tbf, "") == 0) return NULL;
			switch (Size) {
			case 1:
				*((char*)bf) = (char)atol(tbf);
				return bf;
			case 2:
				*((short*)bf) = (short)atol(tbf);
				return bf;
			case 4:
				*((long*)bf) = (long)atol(tbf);
				return bf;
			case 8:
				*((__int64*)bf) = _atoi64(tbf);
				return bf;
			}
			return NULL;
		}

		case EMYDTYPE_DOUBLE: {
			char tbf[258] = "";
			MyIni_GetString(Sec, KeyName, tbf, 256, "");
			if (strcmp(tbf, "") == 0) return NULL;
			switch (Size) {
			case 4:
				*((float*)bf) = (float)strtod(tbf, NULL);
				return bf;
			case 8:
				*((double*)bf) = (double)strtod(tbf, NULL);
				return bf;
			}
			return NULL;
		}
		case EMYDTYPE_ADDR:{
			char tbf[258] = "";
			MyIni_GetString(Sec, KeyName, tbf, 256, "");
			if (strcmp(tbf, "") == 0) return NULL;
			switch (Size) {
			case 1:
				*((char*)bf) = (char)strtoul(tbf, NULL, 16);
				return bf;
			case 2:
				*((short*)bf) = (short)strtoul(tbf, NULL, 16);
				return bf;
			case 4:
				*((long*)bf) = (long)strtoul(tbf, NULL, 16);
				return bf;
			case 8:
				*((__int64*)bf) = _strtoui64(tbf, NULL, 16);
				return bf;
			}
			return NULL;
		}
		case EMYDTYPE_STR: {
			char *p = (char*)bf;
			MyIni_GetString(Sec, KeyName, p, Size, "");
			if (strcmp(p, "") == 0) return NULL;
			return bf;
		}
		case EMYDTYPE_BIN: {
			size_t i = 0;
			char *hexc = { "0123456789ABCDEF" };
			unsigned char *p = (unsigned char*)(bf);
			char *bfm = (char*) myMemAlloc((Size + 4) * 2 * sizeof(char));

			MyIni_GetString(Sec, KeyName, bfm, Size * 2, "");
			if (strcmp(bfm, "") == 0) {
				myMemFree((void**)&bfm);
				return NULL;
			}

			for (i = 0; i < Size; i++) {
				p[i] = (CharToNum(bfm[i << 1]) << 4)+ CharToNum(bfm[(i << 1) + 1]);
			}
			myMemFree((void**)&bfm);
			return bf;
		}
	}
	return NULL;
}

LPINISEC MyIni_GetSection(INIDATA *Ini, const char *Sec, int AutoCreateEnable)
{
	return SearchSection(Ini, Sec, AutoCreateEnable);
}

UINT64 MyIni_DoubleToInt64(double v)
{
	return *((UINT64*)&v);
}

const char *MyIni_PathFindFileName(const char *str)
{
	const char *p = str;
	const char *ret = str;
	while (*p) {
		if (*p == '\\') {
				ret = ++p;
		} else ++p;
	}
	return ret;
}

void MyIni_CreateIniCFile(const char *fn)
{
	char outfn[FILEPATH_MAX] = "";
	FILE *fpi = NULL;
	FILE *fpo = NULL;
	char bf[4096] = "";
	LPINISEC nowSec = NULL;
	LPINISEC TestSec = NULL;
	LPINIKEY Key = NULL;
	LPINIKEY TestKey = NULL;
	char *pbf = NULL;
	char *ptok;


	strcpy(outfn, fn);
	strcat(outfn, ".c");
	TCHAR *tfn = char_to_tchar(fn);
	fpi = _tfopen(tfn, _T("r"));
	safe_free(tfn);
	if (!fpi) return;
	TCHAR *toutfn = char_to_tchar(outfn);
	fpo = _tfopen(toutfn, _T("w"));
	safe_free(toutfn);


	fprintf(fpo, "int LoadIniFile( \n{ \n");
	fprintf(fpo, "\tINIDATA ini; \n\tLPINISEC Sec; \n\tMyIni_Load(&ini, \"%s\"); \n", MyIni_PathFindFileName(fn));
	while (fgets(bf, 4000, fpi)) {
		pbf = bf;
		while (*pbf == ' ' || *pbf == '\t') { ++pbf; }
		if (*pbf == '[') {
			*strtok(pbf, "]") = 0;
//			nowSec = SearchSection(Ini, pbf + 1, TRUE);
			fprintf(fpo, "\n\tSec = MyIni_GetSection(&ini, \"%s\", TRUE);\n", pbf + 1);
			continue;
		}

		if ((ptok = strstr(pbf, "\n")) != NULL)
			*ptok = 0;

		if (strcmp(pbf, "") == 0)
			continue;



		{
			char *p1;
			char *p2;
			p1 = strtok(pbf, "=");
			p2 = strtok(NULL, "=");
			fprintf(fpo, "\t%s\t = MyIni_GetString2(Sec, \"%s\", \"%s\");\n", p1, p1, p2 ? p2 : "NULL");

		}
	}
	fprintf(fpo, "\n\treturn 0; \n}//LoadIniFile \n");


	fclose(fpi);
	tfn = char_to_tchar(fn);
	fpi = _tfopen(tfn, _T("r"));
	safe_free(tfn);

	fprintf(fpo, "int SaveIniFile() \n{ \n");
	fprintf(fpo, "\tINIDATA ini; \n\tLPINISEC Sec; \n\tMyIni_Save(&ini, \"%s\"); \n", MyIni_PathFindFileName(fn));
	while (fgets(bf, 4000, fpi)) {
		pbf = bf;
		while (*pbf == ' ' || *pbf == '\t') { ++pbf; }
		if (*pbf == '[') {
			*strtok(pbf, "]") = 0;
//			nowSec = SearchSection(Ini, pbf + 1, TRUE);
			fprintf(fpo, "\n\tSec = MyIni_GetSection(&ini, \"%s\", TRUE);\n", pbf + 1);
			continue;
		}

		if ((ptok = strstr(pbf, "\n")) != NULL)
			*ptok = 0;

		if (strcmp(pbf, "") == 0)
			continue;



		{
			char *p1;
			char *p2;
			p1 = strtok(pbf, "=");
			p2 = strtok(NULL, "=");
			fprintf(fpo, "\tMyIni_SetString2(Sec, \"%s\", %s);\n", p1, p1);

		}
	}
	fprintf(fpo, "\n\treturn 0; \n}//SaveIniFile \n");
	fclose(fpo);
	fclose(fpi);
}

//--------------------------
// Define を駆使した関数展開


#define MYINI_GETPARAMFUNC(MIVAR, FUNCNAME, CONVFUNC) \
MIVAR FUNCNAME(INISEC *Sec, const char *KeyName, MIVAR def) \
{ \
	char tbf[258] = ""; \
	MyIni_GetString(Sec, KeyName, tbf, 256, ""); \
	return (tbf[0] == '\0') ? def : (MIVAR)CONVFUNC; \
}

MYINI_GETPARAMFUNC(INT8, MyIni_GetInt8,  strtoul(tbf, NULL, 10))
MYINI_GETPARAMFUNC(INT16, MyIni_GetInt16, strtoul(tbf, NULL, 10))
MYINI_GETPARAMFUNC(INT32, MyIni_GetInt32, strtoul(tbf, NULL, 10))
MYINI_GETPARAMFUNC(INT64, MyIni_GetInt64, strtoul(tbf, NULL, 10))

MYINI_GETPARAMFUNC(UINT8, MyIni_GetUint8,  strtoul(tbf, NULL, 10))
MYINI_GETPARAMFUNC(UINT16, MyIni_GetUint16, strtoul(tbf, NULL, 10))
MYINI_GETPARAMFUNC(UINT32, MyIni_GetUint32, strtoul(tbf, NULL, 10))
MYINI_GETPARAMFUNC(UINT64, MyIni_GetUint64, strtoul(tbf, NULL, 10))

MYINI_GETPARAMFUNC(float, MyIni_GetFloat32, strtod(tbf, NULL))
MYINI_GETPARAMFUNC(double, MyIni_GetFloat64, strtod(tbf, NULL))

#undef MYINI_GETPARAMFUNC

#define MYINI_SETPARAMFUNC(MIVAR, FUNCNAME, CNV, FMT) \
void FUNCNAME(INISEC *Sec, const char *KeyName, MIVAR prm) \
{ \
	char tbf[258] = ""; \
	sprintf(tbf, FMT, (CNV)prm); \
	MyIni_SetString(Sec, KeyName, tbf); \
}

MYINI_SETPARAMFUNC(INT8, MyIni_SetInt8, INT32, "%d")
MYINI_SETPARAMFUNC(INT16, MyIni_SetInt16, INT32, "%d")
MYINI_SETPARAMFUNC(INT32, MyIni_SetInt32, INT32, "%d")
MYINI_SETPARAMFUNC(INT64, MyIni_SetInt64, INT64, "%lld")

MYINI_SETPARAMFUNC(UINT8, MyIni_SetUint8, UINT32, "%u")
MYINI_SETPARAMFUNC(UINT16, MyIni_SetUint16, UINT32, "%u")
MYINI_SETPARAMFUNC(UINT32, MyIni_SetUint32, UINT32, "%u")
MYINI_SETPARAMFUNC(UINT64, MyIni_SetUint64, UINT64, "%llu")

MYINI_SETPARAMFUNC(float, MyIni_SetFloat32, float, "%f")
MYINI_SETPARAMFUNC(double, MyIni_SetFloat64, double, "%lf")

#undef MYINI_SETPARAMFUNC
/*
UINT32 MyIni_GetUint32(INISEC *Sec, const char *KeyName, UINT32 def)
{
	char tbf[258] = "";
	MyIni_GetString(Sec, KeyName, tbf, 256, "");
	return (tbf[0] == '\0') ? def : strtoul(tbf, NULL, 10);
}*/

void MyIni_SetBool(INISEC *Sec, const char *KeyName, INT32 prm)
{
	MyIni_SetString(Sec, KeyName, ((prm == 0) ? "0" : "1"));
}

INT32 MyIni_GetBool(INISEC *Sec, const char *KeyName, INT32 def)
{
	char tbf[258] = "";
	MyIni_GetString(Sec, KeyName, tbf, 256, "");
	return ((tbf[0] == '\0') ? def : ((strtol(tbf, NULL, 10) == 0) ? 0 : 1));
}

//#define _CPP
#ifdef _CPP

#define CPP_MYINI_GETPARAMFUNC(MIVAR, FUNCNAME, CONVFUNC) \
MIVAR FUNCNAME(LPCTSTR KeyName, MIVAR def) \
{ \
	char tbf[258] = ""; \
	MyIni_GetString(m_Sec, KeyName, tbf, 256, ""); \
	return (tbf[0] == '\0') ? def : (MIVAR)CONVFUNC; \
}

#define CPP_MYINI_SETPARAMFUNC(MIVAR, FUNCNAME, CNV, FMT) \
void FUNCNAME(LPCTSTR KeyName, MIVAR prm) \
{ \
	char tbf[258] = ""; \
	sprintf(tbf, FMT, (CNV)prm); \
	MyIni_SetString(m_Sec, KeyName, tbf); \
}

class CIniMgr2
{
	INIDATA m_ini;
	INISEC *m_Sec;
	char m_strIniFileName[FILEPATH_MAX];

public:
	explicit CIniMgr2()
	{

	}

	virtual ~CIniMgr2()
	{
		MyIni_SectionAllClear(&m_ini);
	}

	void loadFile(LPCTSTR strFileName)
	{
		strcpy(m_strIniFileName, strFileName);
		MyIni_SectionAllClear(&m_ini);
		MyIni_Load(&m_ini, strFileName);
	}

	void free()
	{
		MyIni_SectionAllClear(&m_ini);
	}

	void openSection(LPCTSTR strSec)
	{
		m_Sec = MyIni_GetSection(&m_ini, strSec, TRUE);
	}

	LPCTSTR getStr(LPCTSTR strKey, LPTSTR strBuf, size_t bufSize, LPCTSTR defParam)
	{
		MyIni_GetString(m_Sec, strKey, strBuf, bufSize, defParam);
		return strBuf;
	}

	void saveFile(LPCTSTR strFileName = NULL)
	{
		if (!strFileName)
			MyIni_Save(&m_ini, m_strIniFileName);
		else MyIni_Save(&m_ini, strFileName);
	}

	CPP_MYINI_GETPARAMFUNC(INT8, getInt8,  strtoul(tbf, NULL, 10))
	CPP_MYINI_GETPARAMFUNC(INT16, getInt16, strtoul(tbf, NULL, 10))
	CPP_MYINI_GETPARAMFUNC(INT32, getInt32, strtoul(tbf, NULL, 10))
	CPP_MYINI_GETPARAMFUNC(INT64, getInt64, strtoul(tbf, NULL, 10))

	CPP_MYINI_GETPARAMFUNC(UINT8, getUint8,  strtoul(tbf, NULL, 10))
	CPP_MYINI_GETPARAMFUNC(UINT16, getUint16, strtoul(tbf, NULL, 10))
	CPP_MYINI_GETPARAMFUNC(UINT32, getUint32, strtoul(tbf, NULL, 10))
	CPP_MYINI_GETPARAMFUNC(UINT64, getUint64, strtoul(tbf, NULL, 10))

	CPP_MYINI_GETPARAMFUNC(float, getFloat32, strtod(tbf, NULL))
	CPP_MYINI_GETPARAMFUNC(double, getFloat64, strtod(tbf, NULL))

	CPP_MYINI_SETPARAMFUNC(INT8, setInt8, INT32, "%d")
	CPP_MYINI_SETPARAMFUNC(INT16, setInt16, INT32, "%d")
	CPP_MYINI_SETPARAMFUNC(INT32, setInt32, INT32, "%d")
	CPP_MYINI_SETPARAMFUNC(INT64, setInt64, INT64, "%lld")

	CPP_MYINI_SETPARAMFUNC(UINT8, setUint8, UINT32, "%u")
	CPP_MYINI_SETPARAMFUNC(UINT16, setUint16, UINT32, "%u")
	CPP_MYINI_SETPARAMFUNC(UINT32, setUint32, UINT32, "%u")
	CPP_MYINI_SETPARAMFUNC(UINT64, setUint64, UINT64, "%llu")

	CPP_MYINI_SETPARAMFUNC(float, setFloat32, float, "%f")
	CPP_MYINI_SETPARAMFUNC(double, setFloat64, double, "%lf")

	#undef CPP_MYINI_GETPARAMFUNC
	#undef CPP_MYINI_SETPARAMFUNC
};

#endif
/*
template<typename T> T MyIni_GetParamEx(INISEC *Sec, const char *KeyName, T data, int mode)
{
	if (typeid(T).name == int) {

			char tbf[258] = "";
			MyIni_GetString(Sec, KeyName, tbf, 256, "");
			if (strcmp(tbf, "") == 0) return NULL;
			switch (Size) {
			case 1:
				*((char*)bf) = (char)strtoul(tbf, NULL, 10);
				return bf;
			case 2:
				*((short*)bf) = (short)strtoul(tbf, NULL, 10);
				return bf;
			case 4:
				*((long*)bf) = (long)strtoul(tbf, NULL, 10);
				return bf;
			case 8:
				*((__int64*)bf) = _strtoui64(tbf, NULL, 10);
				return bf;
			}
			return NULL;
		}
		case EMYDTYPE_INT: {
			char tbf[258] = "";
			MyIni_GetString(Sec, KeyName, tbf, 256, "");
			if (strcmp(tbf, "") == 0) return NULL;
			switch (Size) {
			case 1:
				*((char*)bf) = (char)atol(tbf);
				return bf;
			case 2:
				*((short*)bf) = (short)atol(tbf);
				return bf;
			case 4:
				*((long*)bf) = (long)atol(tbf);
				return bf;
			case 8:
				*((__int64*)bf) = _atoi64(tbf);
				return bf;
			}
			return NULL;
		}

		case EMYDTYPE_DOUBLE: {
			char tbf[258] = "";
			MyIni_GetString(Sec, KeyName, tbf, 256, "");
			if (strcmp(tbf, "") == 0) return NULL;
			switch (Size) {
			case 4:
				*((float*)bf) = (float)strtod(tbf, NULL);
				return bf;
			case 8:
				*((double*)bf) = (double)strtod(tbf, NULL);
				return bf;
			}
			return NULL;
		}
		case EMYDTYPE_ADDR:{
			char tbf[258] = "";
			MyIni_GetString(Sec, KeyName, tbf, 256, "");
			if (strcmp(tbf, "") == 0) return NULL;
			switch (Size) {
			case 1:
				*((char*)bf) = (char)strtoul(tbf, NULL, 16);
				return bf;
			case 2:
				*((short*)bf) = (short)strtoul(tbf, NULL, 16);
				return bf;
			case 4:
				*((long*)bf) = (long)strtoul(tbf, NULL, 16);
				return bf;
			case 8:
				*((__int64*)bf) = _strtoui64(tbf, NULL, 16);
				return bf;
			}
			return NULL;
		}
		case EMYDTYPE_STR: {
			char *p = (char*)bf;
			MyIni_GetString(Sec, KeyName, p, Size, "");
			if (strcmp(p, "") == 0) return NULL;
			return bf;
		}
		case EMYDTYPE_BIN: {
			size_t i = 0;
			char *hexc = { "0123456789ABCDEF" };
			unsigned char *p = (unsigned char*)(bf);
			char *bfm = (char*) myMemAlloc((Size + 4) * 2 * sizeof(char));

			MyIni_GetString(Sec, KeyName, bfm, Size * 2, "");
			if (strcmp(bfm, "") == 0) {
				myMemFree((void**)&bfm);
				return NULL;
			}

			for (i = 0; i < Size; i++) {
				p[i] = (CharToNum(bfm[i << 1]) << 4)+ CharToNum(bfm[(i << 1) + 1]);
			}
			myMemFree((void**)&bfm);
			return bf;
		}
	}
	return NULL;
}
*/
