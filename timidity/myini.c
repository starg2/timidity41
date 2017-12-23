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

#ifdef _MSC_VER
#	include <windows.h>
#	include <tchar.h>
#	pragma comment(lib, "kernel32.lib")
#	pragma comment(lib, "winmm.lib")
#	ifdef _DEBUG
//#		define _CRTDBG_MAP_ALLOC
#		include <crtdbg.h>
#	endif
#else

#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <stdarg.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif /* HAVE_STDINT_H */
#include "common.h"
#define CONST const

#ifdef __GNUC__
/**
 * C++ version 0.4 char *style "itoa":
 * Written by Lukas Chmela
 * Released under GPLv3.
 */
char *itoa(int value, char *result, int base)
{
	char *ptr = result, *ptr1 = result, tmp_char;
	int tmp_value;

	// check that the base if valid
	if (base < 2 || base > 36) { *result = '\0'; return result; }

	do {
		tmp_value = value;
		value /= base;
		*ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
	} while (value);

	// Apply negative sign
	if (tmp_value < 0) *ptr++ = '-';
	*ptr-- = '\0';
	while (ptr1 < ptr) {
		tmp_char = *ptr;
		*ptr-- = *ptr1;
		*ptr1++ = tmp_char;
	}
	return result;
}
char *uitoa(unsigned int value, char *result, int base)
{
	char *ptr = result, *ptr1 = result, tmp_char;
	unsigned int tmp_value;

	// check that the base if valid
	if (base < 2 || base > 36) { *result = '\0'; return result; }

	do {
		tmp_value = value;
		value /= base;
		*ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
	} while (value);

	// Apply negative sign
	//if (tmp_value < 0) *ptr++ = '-';
	*ptr-- = '\0';
	while (ptr1 < ptr) {
		tmp_char = *ptr;
		*ptr-- = *ptr1;
		*ptr1++ = tmp_char;
	}
	return result;
}
#define _i64toa itoa
#define _ui64toa uitoa
#define _strtoui64 strtoull
#define _atoi64 atoll
#endif

#define _tcscmp strcmp
#define _tcslen strlen
#define _tcsncmp strncmp
#define _tcscpy strcpy
#define _tcsncpy strncpy
#define _tcscat strcat
#define _tcsncat strncat
#define _stprintf sprintf
#define _tcstok strtok
#define _vstprintf vsprintf
#define _tcstol strtol
#define _tcstoul strtoul
#define _tcstoui64 _strtoui64
#define _tcstod strtod
#define _ttol atol
#define _ttod atod
#define _ttoi64 _atoi64
#define _ui64tot _ui64toa
#define _i64tot _i64toa

#define _ftprintf fprintf
#define _tfopen fopen
#define _fgetts fgets
#define _fgettc fgetc
#define _tcschr strchr
#define _tcsstr strstr

#ifdef UNICODE
#define _T(x) L ## x
#else
#define _T(x) x
#endif

#define __int64 long long
#define INT8 int8_t
#define UINT8 uint8_t
#define INT16 int16_t
#define UINT16 uint16_t
#define INT32 int32_t
#define UINT32 uint32_t
#define INT64 int64_t
#define UINT64 uint64_t

#endif

#include "myini.h"

//extern struct tagKey;
//extern struct tagSection;

/*
typedef struct tagKey {
	uint32 Hash;
	TCHAR *Name;
	TCHAR *Data;
	struct tagKey *Next;
} INIKEY, *LPINIKEY;

typedef struct tagSection {
	uint32 Hash;
	TCHAR *Name;
	LPINIKEY Key;
	struct tagSection *Next;
} INISEC, *LPINISEC;

typedef struct tagini {
//	int i;
	LPINISEC Sec;
} INIDATA;
*/

#ifndef _UNICODE
/*
  ハッシュを作成する
  引数:
    str: 文字列
  戻り値
    ハッシュ
*/
static UINT32 CreateHash(CONST TCHAR *str)
{
	CONST UINT8 *p = (CONST UINT8*)str;
	uint32 Key = 0;
	while (*p) {
		if (*p < 0x81)		Key += *p++;
		else if (*p < 0xA0)	Key += *(CONST UINT16*)p++;
		else if (*p < 0xE0)	Key += *p++;
		else if (*p < 0xFF)	Key += *(CONST UINT16*)p++;
		else				Key += *p++;
	}
	return Key;
}
/*
  strlen
*/
static size_t MyStrLen(CONST TCHAR *str)
{
	CONST UINT8 *p = (CONST UINT8*)str;
	uint32 r = 0;
	while (*p) {
		if (*p < 0x81u)		++p, ++r;
		else if (*p < 0xA0u)	(void)*(CONST UINT16*)p++, r += 2;
		else if (*p < 0xE0u) ++p, ++r;
		else if (*p < 0xFFu)	(void)*(CONST UINT16*)p++, r += 2;
		else				++p, ++r;
	}
	return r;
}
#else
/*
  ハッシュを作成する
  引数:
    str: 文字列
  戻り値
    ハッシュ
*/
static DWORD CreateHash(CONST TCHAR *str)
{
	BYTE *p = (BYTE*)str;
	uint32 Key = 0;
	while (*p) {
		Key += *p++;
	}
	return Key;
}
/*
  strlen
*/
#define MyStrLen _tcslen
#endif

/*
  VC Debug Window にログを流す.
  sprintf と同様。
*/
#ifdef _MSC_VER
static void OutputDebugStrFmt(TCHAR *fmt, ...)
{
	TCHAR szBuffer[2048] = _T("");
	va_list marker;
	va_start(marker, fmt);
	_vstprintf(szBuffer, fmt, marker);
	va_end(marker);
	OutputDebugString(szBuffer);
}
#endif

extern int MyIni_DeleteSection(INIDATA *Ini, CONST TCHAR *SecName);
extern LPINISEC SearchSection(INIDATA *Ini, CONST TCHAR *Section, int AutoCreateEnable);
extern LPINIKEY SearchKey(LPINISEC Sec, CONST TCHAR *KeyName, int AutoCreateEnable);

extern TCHAR *MyIni_GetString(INISEC *Sec, CONST TCHAR *KeyName, TCHAR *Buf, size_t Size, CONST TCHAR *DefParam);
extern void MyIni_SetString2(INIDATA *Ini, CONST TCHAR *SecName, CONST TCHAR *KeyName, CONST TCHAR *Data);

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
//	_tprintf(_T("MemAlloc: %02d %p\n"), g_memref, p);
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
//	_tprintf(_T("MemAlloc: %02d %p\n"), g_memref, p);
	return p;
}

void myMemFree(void **p)
{
	if (!*p) return;
//	_tprintf(_T("MemFree : %02d %p\n"), g_memref, *p);
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


static LPINISEC PushSection(CONST TCHAR *Section)
{
	LPINISEC Sec = (LPINISEC) myMemCalloc(1, sizeof(INISEC));
	Sec->Name = (TCHAR*) myMemAlloc((MyStrLen(Section) + 4) * sizeof(TCHAR));
	_tcscpy(Sec->Name, Section);
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
LPINISEC SearchSection(INIDATA *Ini, CONST TCHAR *Section, int AutoCreateEnable)
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
			if (_tcscmp(Sec->Name, Section) == 0) {
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
LPINIKEY PushKey(CONST TCHAR *KeyName)
{
	LPINIKEY Key;
	Key = (LPINIKEY) myMemCalloc(1, sizeof(INIKEY));
	Key->Name = (TCHAR*) myMemAlloc((MyStrLen(KeyName) + 4) * sizeof(TCHAR));
	_tcscpy(Key->Name, KeyName);
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
LPINIKEY SearchKey(LPINISEC Sec, CONST TCHAR *KeyName, int AutoCreateEnable)
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
			if (_tcscmp(Key->Name, KeyName) == 0) {
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
void CreateKeyData(LPINIKEY Key, CONST TCHAR *Data)
{
	if (!Key->Data) {
		if (Data) {
			Key->Data = (TCHAR*) myMemAlloc((_tcslen(Data) + 4) * sizeof(TCHAR));
			_tcscpy(Key->Data, Data);
		} else {
			Key->Data = NULL;
		}
		return;
	}

	myMemFree((void**)&Key->Data);
	if (Data) {
		Key->Data = (TCHAR*) myMemAlloc((_tcslen(Data) + 4) * sizeof(TCHAR));
		_tcscpy(Key->Data, Data);
	} else {
		Key->Data = NULL;
	}
}

/*-------------------------------------------------------------
  公開関数
  Ini をLoad する
-------------------------------------------------------------*/
void MyIni_Load(INIDATA *Ini, CONST TCHAR *str)
{
	TCHAR *ptok = NULL;
	TCHAR buf[2048] = _T("");
	FILE *fp = _tfopen(str, _T("r"));
	LPINISEC nowSec = NULL;
	LPINIKEY Key = NULL;
	TCHAR *pbf = NULL;

	if (!fp) {
		return;
	}

	while (_fgetts(buf, 2000, fp)) {

		pbf = buf;

		while (*pbf == ' ' || *pbf == '\t') { ++pbf; }
		if (*pbf == '[') {
			*_tcstok(pbf, _T("]")) = 0;
			nowSec = SearchSection(Ini, pbf + 1, TRUE);
			continue;
		}

		if ((ptok = _tcsstr(pbf, _T("\n")))!= NULL)
			*ptok = 0;

		if (*pbf == ';') {
			TCHAR *pcm = _tcstok(pbf, _T("; "));
//			Key = SearchKey(nowSec, pcm, TRUE);
//			CreateKeyData(Key, NULL);
			if (pcm) {
				Key = SearchKey(nowSec, pcm, TRUE);
				CreateKeyData(Key, NULL);
			}
			continue;
		}

		if (_tcscmp(pbf, _T("")) == 0)
			continue;

		ptok = _tcstok(pbf, _T("="));
		Key = SearchKey(nowSec, ptok, TRUE);
		ptok = _tcstok(NULL, _T("="));

		CreateKeyData(Key, ptok);
	}

	fclose(fp);
}

/*-------------------------------------------------------------
  公開関数
  Ini をLoad する
  timidity_file対応版 , CFG dir から検索
  saveのときはabs_path使用するので loadのみでいいはず
  tf_gets()が_fgets()と同じ動作かは不明
-------------------------------------------------------------*/
void MyIni_Load_timidity(INIDATA *Ini, CONST TCHAR *str, int decompress, int noise_mode)
{
	TCHAR *ptok = NULL;
	TCHAR buf[2048] = _T("");
	LPINISEC nowSec = NULL;
	LPINIKEY Key = NULL;
	TCHAR *pbf = NULL;
	struct timidity_file *tf = open_file((char *)str, decompress, noise_mode);

	if (tf == NULL) {
		return;
	}

	while (tf_gets(buf, 2000, tf)) {

		pbf = buf;

		while (*pbf == ' ' || *pbf == '\t') { ++pbf; }
		if (*pbf == '[') {
			*_tcstok(pbf, _T("]")) = 0;
			nowSec = SearchSection(Ini, pbf + 1, TRUE);
			continue;
		}

		if ((ptok = _tcsstr(pbf, _T("\n")))!= NULL)
			*ptok = 0;

		if (*pbf == ';') {
			TCHAR *pcm = _tcstok(pbf, _T("; "));
//			Key = SearchKey(nowSec, pcm, TRUE);
//			CreateKeyData(Key, NULL);
			if (pcm) {
				Key = SearchKey(nowSec, pcm, TRUE);
				CreateKeyData(Key, NULL);
			}
			continue;
		}

		if (_tcscmp(pbf, _T("")) == 0)
			continue;

		ptok = _tcstok(pbf, _T("="));
		Key = SearchKey(nowSec, ptok, TRUE);
		ptok = _tcstok(NULL, _T("="));

		CreateKeyData(Key, ptok);
	}
	close_file(tf);
}

/*-------------------------------------------------------------
  公開関数
  Ini を Save する
-------------------------------------------------------------*/
void MyIni_Save(INIDATA *Ini, CONST TCHAR *fn)
{
	LPINISEC Sec = Ini->Sec;
	LPINIKEY Key;
	FILE *fp = _tfopen(fn, _T("w"));
	if (!fp) return;

	while (Sec) {
		Key = Sec->Key;
		_ftprintf(fp, _T("[%s]\n"), Sec->Name);
		Sec = Sec->Next;


		while (Key) {
			if (Key->Data) {
				_ftprintf(fp, _T("%s=%s\n"), Key->Name, Key->Data);
			} else { _ftprintf(fp, _T("; %s\n"), Key->Name); }
			Key = Key->Next;
		}
		_ftprintf(fp, _T("\n"));
	}
	fclose(fp);
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
int MyIni_DeleteSection(INIDATA *Ini, CONST TCHAR *SecName)
{
	LPINISEC Sec = Ini->Sec;
	LPINISEC SaveSec = Ini->Sec;
	LPINISEC BackSec = Ini->Sec;

	while (Sec) {
		if (_tcscmp(Sec->Name, SecName) == 0) {
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
int MyIni_DeleteKey(INIDATA *Ini, CONST TCHAR *SecName, CONST TCHAR *KeyName)
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
		if (_tcscmp(Key->Name, KeyName) == 0) {
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
int MyIni_SectionExists(INIDATA *Ini, CONST TCHAR *SecName)
{
	return SearchSection(Ini, SecName, FALSE) ? 1 : 0;
}

/*-------------------------------------------------------------
  公開関数
  キーを検索する
-------------------------------------------------------------*/
int MyIni_KeyExists(INIDATA *Ini, CONST TCHAR *SecName, CONST TCHAR *KeyName)
{
	LPINISEC Sec = NULL;

	if ((Sec = SearchSection(Ini, SecName, FALSE)) == NULL)
		return 0;
	return SearchKey(Sec, KeyName, FALSE) ? 1 : 0;
}


TCHAR *MyIni_GetString2(INIDATA *Ini, CONST TCHAR *Section, CONST TCHAR *KeyName, TCHAR *Buf, size_t Size, CONST TCHAR *DefParam)
{
	LPINIKEY Key = NULL;
	INISEC *Sec = SearchSection(Ini, Section, FALSE);

	if (!Sec)
		return _tcsncpy(Buf, DefParam, Size);
	if (!Sec->Key)
		return _tcsncpy(Buf, DefParam, Size);

	if ((Key = SearchKey(Sec, KeyName, FALSE)) == NULL)
		return _tcsncpy(Buf, DefParam, Size);

	return _tcsncpy(Buf, Key->Data, Size);
}

TCHAR *MyIni_GetString(INISEC *Sec, CONST TCHAR *KeyName, TCHAR *Buf, size_t Size, CONST TCHAR *DefParam)
{
	LPINIKEY Key = NULL;

	if (!Sec)
		return _tcsncpy(Buf, DefParam, Size);
	if (!Sec->Key)
		return _tcsncpy(Buf, DefParam, Size);

	if ((Key = SearchKey(Sec, KeyName, FALSE)) == NULL)
		return _tcsncpy(Buf, DefParam, Size);

	return _tcsncpy(Buf, Key->Data, Size);
}

void MyIni_SetString2(INIDATA *Ini, CONST TCHAR *SecName, CONST TCHAR *KeyName, CONST TCHAR *Data)
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

void MyIni_SetString(INISEC *Sec, CONST TCHAR *KeyName, CONST TCHAR *Data)
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
void MyIni_SetParam(INISEC *Sec, CONST TCHAR *KeyName, UINT64 Param, size_t Size, MyIni_DataType DataType)
{
	TCHAR bf[2048] = _T("");

	switch (DataType) {
		case EMYDTYPE_INT:
			_i64tot(Param, bf, 10);
			MyIni_SetString(Sec, KeyName, bf);
			return;
		case EMYDTYPE_UINT:
			_ui64tot(Param, bf, 10);
			MyIni_SetString(Sec, KeyName, bf);
			return;

		case EMYDTYPE_DOUBLE:
#ifdef _UNICODE
			_stprintf(bf, _T("%lf"), *((double*)&Param));
#elif defined(__GNUC__)
			_stprintf(bf, _T("%f"), *((double*)&Param));
#else
			_gcvt(*((double*)&Param), 6, bf);
#endif
			MyIni_SetString(Sec, KeyName, bf);
			return;

		case EMYDTYPE_ADDR:
			_ui64tot(Param, bf, 16);
			MyIni_SetString(Sec, KeyName, bf);
			return;

		case EMYDTYPE_BIN: {
			size_t i = 0;
			char *hexc = { "0123456789ABCDEF" };
			unsigned char *p = (unsigned char*)((u_ptr_size_t)Param);
			TCHAR *bfm = (TCHAR*) myMemAlloc((Size + 4) * 2 * sizeof(TCHAR));
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
			MyIni_SetString(Sec, KeyName, (TCHAR*)((u_ptr_size_t)Param));
			return;
	}
}

char CharToNum(TCHAR x)
{
	if (x >= '0' && x <= '9') return x - '0';
	if (x >= 'A' && x <= 'Z') return x - 'A' + 10;
	if (x >= 'a' && x <= 'z') return x - 'a' + 10;
	return 0;
}

CONST void *MyIni_GetParam(INISEC *Sec, CONST TCHAR *KeyName, void *bf, size_t Size, MyIni_DataType DataType)
{
	switch (DataType) {
		case EMYDTYPE_UINT:{
			TCHAR tbf[258] = _T("");
			MyIni_GetString(Sec, KeyName, tbf, 256, _T(""));
			if (_tcscmp(tbf, _T("")) == 0) return NULL;
			switch (Size) {
			case 1:
				*((char*)bf) = (char)_tcstoul(tbf, NULL, 10);
				return bf;
			case 2:
				*((short*)bf) = (short)_tcstoul(tbf, NULL, 10);
				return bf;
			case 4:
				*((long*)bf) = (long)_tcstoul(tbf, NULL, 10);
				return bf;
			case 8:
				*((__int64*)bf) = _tcstoui64(tbf, NULL, 10);
				return bf;
			}
			return NULL;
		}
		case EMYDTYPE_INT: {
			TCHAR tbf[258] = _T("");
			MyIni_GetString(Sec, KeyName, tbf, 256, _T(""));
			if (_tcscmp(tbf, _T("")) == 0) return NULL;
			switch (Size) {
			case 1:
				*((char*)bf) = (char)_ttol(tbf);
				return bf;
			case 2:
				*((short*)bf) = (short)_ttol(tbf);
				return bf;
			case 4:
				*((long*)bf) = (long)_ttol(tbf);
				return bf;
			case 8:
				*((__int64*)bf) = _ttoi64(tbf);
				return bf;
			}
			return NULL;
		}

		case EMYDTYPE_DOUBLE: {
			TCHAR tbf[258] = _T("");
			MyIni_GetString(Sec, KeyName, tbf, 256, _T(""));
			if (_tcscmp(tbf, _T("")) == 0) return NULL;
			switch (Size) {
			case 4:
				*((float*)bf) = (float)_tcstod(tbf, NULL);
				return bf;
			case 8:
				*((double*)bf) = (double)_tcstod(tbf, NULL);
				return bf;
			}
			return NULL;
		}
		case EMYDTYPE_ADDR:{
			TCHAR tbf[258] = _T("");
			MyIni_GetString(Sec, KeyName, tbf, 256, _T(""));
			if (_tcscmp(tbf, _T("")) == 0) return NULL;
			switch (Size) {
			case 1:
				*((char*)bf) = (char)_tcstoul(tbf, NULL, 16);
				return bf;
			case 2:
				*((short*)bf) = (short)_tcstoul(tbf, NULL, 16);
				return bf;
			case 4:
				*((long*)bf) = (long)_tcstoul(tbf, NULL, 16);
				return bf;
			case 8:
				*((__int64*)bf) = _tcstoui64(tbf, NULL, 16);
				return bf;
			}
			return NULL;
		}
		case EMYDTYPE_STR: {
			char *p = (char*)bf;
			MyIni_GetString(Sec, KeyName, p, Size, _T(""));
			if (_tcscmp(p, _T("")) == 0) return NULL;
			return bf;
		}
		case EMYDTYPE_BIN: {
			size_t i = 0;
			char *hexc = { "0123456789ABCDEF" };
			unsigned char *p = (unsigned char*)(bf);
			TCHAR *bfm = (TCHAR*) myMemAlloc((Size + 4) * 2 * sizeof(TCHAR));

			MyIni_GetString(Sec, KeyName, bfm, Size * 2, _T(""));
			if (_tcscmp(bfm, _T("")) == 0) {
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

LPINISEC MyIni_GetSection(INIDATA *Ini, CONST TCHAR *Sec, int AutoCreateEnable)
{
	return SearchSection(Ini, Sec, AutoCreateEnable);
}

UINT64 MyIni_DoubleToInt64(double v)
{
	return *((UINT64*)&v);
}

const TCHAR *MyIni_PathFindFileName(CONST TCHAR *str)
{
#ifdef _UNICODE
	const wchar_t *p = str;
	const wchar_t *ret = str;
	while (*p) {
		if (*p == L'\\') {
				ret = ++p;
		} else ++p;
	}
	return ret;
#else
	const unsigned char *p = (const unsigned char*)str;
	const unsigned char *ret = p;
	while (*p) {
		if (*p < 0x81u)		{ if (*p == '\\') { ret = ++p; } else ++p; }
		else if (*p < 0xA0u)	(void)*(const UINT16*)p++;
		else if (*p < 0xE0u)	{ if (*p == '\\') { ret = ++p; } else ++p; }
		else if (*p < 0xFFu)	(void)*(const UINT16*)p++;
		else				{ if (*p == '\\') { ret = ++p; } else ++p; }
	}
	return (const char*)ret;
#endif
}

void MyIni_CreateIniCFile(CONST TCHAR *fn)
{
	TCHAR outfn[FILEPATH_MAX] = _T("");
	FILE *fpi = NULL;
	FILE *fpo = NULL;
	TCHAR bf[4096] = _T("");
	LPINISEC nowSec = NULL;
	LPINISEC TestSec = NULL;
	LPINIKEY Key = NULL;
	LPINIKEY TestKey = NULL;
	TCHAR *pbf = NULL;
	TCHAR *ptok;


	_tcscpy(outfn, fn);
	_tcscat(outfn, _T(".c"));
	fpi = _tfopen(fn, _T("r"));
	if (!fpi) return;
	fpo = _tfopen(outfn, _T("w"));




	_ftprintf(fpo, _T("int LoadIniFile() \n{ \n"));
	_ftprintf(fpo, _T("\tINIDATA ini; \n\tLPINISEC Sec; \n\tMyIni_Load(&ini, \"%s\"); \n"), MyIni_PathFindFileName(fn));
	while (_fgetts(bf, 4000, fpi)) {
		pbf = bf;
		while (*pbf == ' ' || *pbf == '\t') { ++pbf; }
		if (*pbf == '[') {
			*_tcstok(pbf, _T("]")) = 0;
//			nowSec = SearchSection(Ini, pbf + 1, TRUE);
			_ftprintf(fpo, _T("\n\tSec = MyIni_GetSection(&ini, \"%s\", TRUE);\n"), pbf + 1);
			continue;
		}

		if ((ptok = _tcsstr(pbf, _T("\n"))) != NULL)
			*ptok = 0;

		if (_tcscmp(pbf, _T("")) == 0)
			continue;



		{
			TCHAR *p1;
			TCHAR *p2;
			p1 = _tcstok(pbf, _T("="));
			p2 = _tcstok(NULL, _T("="));
			_ftprintf(fpo, _T("\t%s\t = MyIni_GetString2(Sec, \"%s\", \"%s\");\n"), p1, p1, p2 ? p2 : _T("NULL"));

		}
	}
	_ftprintf(fpo, _T("\n\treturn 0; \n}//LoadIniFile \n"));


	fclose(fpi);
	fpi = _tfopen(fn, _T("r"));

	_ftprintf(fpo, _T("int SaveIniFile() \n{ \n"));
	_ftprintf(fpo, _T("\tINIDATA ini; \n\tLPINISEC Sec; \n\tMyIni_Save(&ini, \"%s\"); \n"), MyIni_PathFindFileName(fn));
	while (_fgetts(bf, 4000, fpi)) {
		pbf = bf;
		while (*pbf == ' ' || *pbf == '\t') { ++pbf; }
		if (*pbf == '[') {
			*_tcstok(pbf, _T("]")) = 0;
//			nowSec = SearchSection(Ini, pbf + 1, TRUE);
			_ftprintf(fpo, _T("\n\tSec = MyIni_GetSection(&ini, \"%s\", TRUE);\n"), pbf + 1);
			continue;
		}

		if ((ptok = _tcsstr(pbf, _T("\n"))) != NULL)
			*ptok = 0;

		if (_tcscmp(pbf, _T("")) == 0)
			continue;



		{
			TCHAR *p1;
			TCHAR *p2;
			p1 = _tcstok(pbf, _T("="));
			p2 = _tcstok(NULL, _T("="));
			_ftprintf(fpo, _T("\tMyIni_SetString2(Sec, \"%s\", %s);\n"), p1, p1);

		}
	}
	_ftprintf(fpo, _T("\n\treturn 0; \n}//SaveIniFile \n"));
	fclose(fpo);
	fclose(fpi);
}

//--------------------------
// Define を駆使した関数展開


#define MYINI_GETPARAMFUNC(MIVAR, FUNCNAME, CONVFUNC) \
MIVAR FUNCNAME(INISEC *Sec, CONST TCHAR *KeyName, MIVAR def) \
{ \
	TCHAR tbf[258] = _T(""); \
	MyIni_GetString(Sec, KeyName, tbf, 256, _T("")); \
	return (tbf[0] == '\0') ? def : (MIVAR)CONVFUNC; \
}

MYINI_GETPARAMFUNC(INT8, MyIni_GetInt8,  _tcstoul(tbf, NULL, 10))
MYINI_GETPARAMFUNC(INT16, MyIni_GetInt16, _tcstoul(tbf, NULL, 10))
MYINI_GETPARAMFUNC(INT32, MyIni_GetInt32, _tcstoul(tbf, NULL, 10))
MYINI_GETPARAMFUNC(INT64, MyIni_GetInt64, _tcstoul(tbf, NULL, 10))

MYINI_GETPARAMFUNC(UINT8, MyIni_GetUint8,  _tcstoul(tbf, NULL, 10))
MYINI_GETPARAMFUNC(UINT16, MyIni_GetUint16, _tcstoul(tbf, NULL, 10))
MYINI_GETPARAMFUNC(UINT32, MyIni_GetUint32, _tcstoul(tbf, NULL, 10))
MYINI_GETPARAMFUNC(UINT64, MyIni_GetUint64, _tcstoul(tbf, NULL, 10))

MYINI_GETPARAMFUNC(float, MyIni_GetFloat32, _tcstod(tbf, NULL))
MYINI_GETPARAMFUNC(double, MyIni_GetFloat64, _tcstod(tbf, NULL))

#undef MYINI_GETPARAMFUNC

#define MYINI_SETPARAMFUNC(MIVAR, FUNCNAME, CNV, FMT) \
void FUNCNAME(INISEC *Sec, CONST TCHAR *KeyName, MIVAR prm) \
{ \
	TCHAR tbf[258] = _T(""); \
	_stprintf(tbf, _T(FMT), (CNV)prm); \
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
UINT32 MyIni_GetUint32(INISEC *Sec, CONST TCHAR *KeyName, UINT32 def)
{
	TCHAR tbf[258] = _T("");
	MyIni_GetString(Sec, KeyName, tbf, 256, _T(""));
	return (tbf[0] == '\0') ? def : _tcstoul(tbf, NULL, 10);
}*/

void MyIni_SetBool(INISEC *Sec, CONST TCHAR *KeyName, INT32 prm)
{
	MyIni_SetString(Sec, KeyName, ((prm == 0) ? "0" : "1"));
}

INT32 MyIni_GetBool(INISEC *Sec, CONST TCHAR *KeyName, INT32 def)
{
	TCHAR tbf[258] = _T("");
	MyIni_GetString(Sec, KeyName, tbf, 256, _T(""));
	return ((tbf[0] == '\0') ? def : ((_tcstol(tbf, NULL, 10) == 0) ? 0 : 1));
}

//#define _CPP
#ifdef _CPP

#define CPP_MYINI_GETPARAMFUNC(MIVAR, FUNCNAME, CONVFUNC) \
MIVAR FUNCNAME(LPCTSTR KeyName, MIVAR def) \
{ \
	TCHAR tbf[258] = _T(""); \
	MyIni_GetString(m_Sec, KeyName, tbf, 256, _T("")); \
	return (tbf[0] == '\0') ? def : (MIVAR)CONVFUNC; \
}

#define CPP_MYINI_SETPARAMFUNC(MIVAR, FUNCNAME, CNV, FMT) \
void FUNCNAME(LPCTSTR KeyName, MIVAR prm) \
{ \
	TCHAR tbf[258] = _T(""); \
	_stprintf(tbf, _T(FMT), (CNV)prm); \
	MyIni_SetString(m_Sec, KeyName, tbf); \
}

class CIniMgr2
{
	INIDATA m_ini;
	INISEC *m_Sec;
	TCHAR m_strIniFileName[FILEPATH_MAX];

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
		_tcscpy(m_strIniFileName, strFileName);
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

	CPP_MYINI_GETPARAMFUNC(INT8, getInt8,  _tcstoul(tbf, NULL, 10))
	CPP_MYINI_GETPARAMFUNC(INT16, getInt16, _tcstoul(tbf, NULL, 10))
	CPP_MYINI_GETPARAMFUNC(INT32, getInt32, _tcstoul(tbf, NULL, 10))
	CPP_MYINI_GETPARAMFUNC(INT64, getInt64, _tcstoul(tbf, NULL, 10))

	CPP_MYINI_GETPARAMFUNC(UINT8, getUint8,  _tcstoul(tbf, NULL, 10))
	CPP_MYINI_GETPARAMFUNC(UINT16, getUint16, _tcstoul(tbf, NULL, 10))
	CPP_MYINI_GETPARAMFUNC(UINT32, getUint32, _tcstoul(tbf, NULL, 10))
	CPP_MYINI_GETPARAMFUNC(UINT64, getUint64, _tcstoul(tbf, NULL, 10))

	CPP_MYINI_GETPARAMFUNC(float, getFloat32, _tcstod(tbf, NULL))
	CPP_MYINI_GETPARAMFUNC(double, getFloat64, _tcstod(tbf, NULL))

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

			TCHAR tbf[258] = _T("");
			MyIni_GetString(Sec, KeyName, tbf, 256, _T(""));
			if (_tcscmp(tbf, _T("")) == 0) return NULL;
			switch (Size) {
			case 1:
				*((char*)bf) = (char)_tcstoul(tbf, NULL, 10);
				return bf;
			case 2:
				*((short*)bf) = (short)_tcstoul(tbf, NULL, 10);
				return bf;
			case 4:
				*((long*)bf) = (long)_tcstoul(tbf, NULL, 10);
				return bf;
			case 8:
				*((__int64*)bf) = _tcstoui64(tbf, NULL, 10);
				return bf;
			}
			return NULL;
		}
		case EMYDTYPE_INT: {
			TCHAR tbf[258] = _T("");
			MyIni_GetString(Sec, KeyName, tbf, 256, _T(""));
			if (_tcscmp(tbf, _T("")) == 0) return NULL;
			switch (Size) {
			case 1:
				*((char*)bf) = (char)_ttol(tbf);
				return bf;
			case 2:
				*((short*)bf) = (short)_ttol(tbf);
				return bf;
			case 4:
				*((long*)bf) = (long)_ttol(tbf);
				return bf;
			case 8:
				*((__int64*)bf) = _ttoi64(tbf);
				return bf;
			}
			return NULL;
		}

		case EMYDTYPE_DOUBLE: {
			TCHAR tbf[258] = _T("");
			MyIni_GetString(Sec, KeyName, tbf, 256, _T(""));
			if (_tcscmp(tbf, _T("")) == 0) return NULL;
			switch (Size) {
			case 4:
				*((float*)bf) = (float)_tcstod(tbf, NULL);
				return bf;
			case 8:
				*((double*)bf) = (double)_tcstod(tbf, NULL);
				return bf;
			}
			return NULL;
		}
		case EMYDTYPE_ADDR:{
			TCHAR tbf[258] = _T("");
			MyIni_GetString(Sec, KeyName, tbf, 256, _T(""));
			if (_tcscmp(tbf, _T("")) == 0) return NULL;
			switch (Size) {
			case 1:
				*((char*)bf) = (char)_tcstoul(tbf, NULL, 16);
				return bf;
			case 2:
				*((short*)bf) = (short)_tcstoul(tbf, NULL, 16);
				return bf;
			case 4:
				*((long*)bf) = (long)_tcstoul(tbf, NULL, 16);
				return bf;
			case 8:
				*((__int64*)bf) = _tcstoui64(tbf, NULL, 16);
				return bf;
			}
			return NULL;
		}
		case EMYDTYPE_STR: {
			char *p = (char*)bf;
			MyIni_GetString(Sec, KeyName, p, Size, _T(""));
			if (_tcscmp(p, _T("")) == 0) return NULL;
			return bf;
		}
		case EMYDTYPE_BIN: {
			size_t i = 0;
			char *hexc = { "0123456789ABCDEF" };
			unsigned char *p = (unsigned char*)(bf);
			TCHAR *bfm = (TCHAR*) myMemAlloc((Size + 4) * 2 * sizeof(TCHAR));

			MyIni_GetString(Sec, KeyName, bfm, Size * 2, _T(""));
			if (_tcscmp(bfm, _T("")) == 0) {
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
