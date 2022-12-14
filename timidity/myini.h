
#ifndef __MYINI_LIBRARY__
#define __MYINI_LIBRARY__

#include <stdio.h>


#ifdef MYINI_LIBRARY_DEFIND_VAR
#ifdef _WIN32
#include <windows.h>
#else

#ifndef INT8
typedef char INT8;
#endif

#ifndef INT16
typedef short INT16;
#endif

#ifndef INT32
typedef long INT32;
#endif

#ifndef INT64
#ifdef _MSC_VER
typedef __int64 INT64;
#else
typedef long long INT64;
#endif
#endif

#ifndef UINT8
typedef unsigned char UINT8;
#endif

#ifndef UINT16
typedef unsigned short UINT16;
#endif

#ifndef UINT32
typedef unsigned long UINT32;
#endif

#ifndef UINT64
#ifdef _MSC_VER
typedef unsigned __int64 UINT64;
#else
typedef unsigned long long UINT64;
#endif
#endif

#ifndef CONST
	#define CONST const
#endif

#endif
#endif


#ifndef _TCHAR_DEFINED
	#ifdef _UNICODE
		#define TCHAR wchar_t
	#else
		#define TCHAR char
	#endif
#define _TCHAR_DEFINED
#endif

typedef enum Enum_MyIni_DataType
{ 
	EMYDTYPE_INT, 

	EMYDTYPE_UINT, 

	EMYDTYPE_DOUBLE, 

	EMYDTYPE_ADDR, 
	EMYDTYPE_BIN,

	EMYDTYPE_STR, 
}MyIni_DataType;

typedef struct tagKey {
	unsigned long Hash;
	TCHAR *Name;
	TCHAR *Data;
	struct tagKey *Next;
}INIKEY, *LPINIKEY;

typedef struct tagSection {
	unsigned long Hash;
	TCHAR *Name;
	LPINIKEY Key;
	struct tagSection *Next;
}INISEC, *LPINISEC;

typedef struct tagini {
//	int i;
	LPINISEC Sec;
}INIDATA;

/*------------------------------------------------*
** ファイルロード
**------------------------------------------------*/
extern void MyIni_Load(INIDATA *Ini, CONST TCHAR *str);
extern void MyIni_Load_timidity(INIDATA *Ini, CONST TCHAR *str, int decompress, int noise_mode);

/*------------------------------------------------*
** ファイルセーブ
**------------------------------------------------*/
extern void MyIni_Save(INIDATA *Ini, CONST TCHAR *fn);

/*------------------------------------------------*
** キークリア
**------------------------------------------------*/
extern void MyIni_KeyAllClear(LPINISEC Sec);

/*------------------------------------------------*
** セクションクリア
**------------------------------------------------*/
extern void MyIni_SectionAllClear(INIDATA *Ini);

/*------------------------------------------------*
** セクション/キー削除関数
**------------------------------------------------*/
extern int MyIni_DeleteSection(INIDATA *Ini, CONST TCHAR *SecName);
extern int MyIni_DeleteKey(INIDATA *Ini, CONST TCHAR *SecName, CONST TCHAR *KeyName);

/*------------------------------------------------*
** セクション/キー検索関数
**------------------------------------------------*/
extern int MyIni_SectionExists(INIDATA *Ini, CONST TCHAR *SecName);
extern int MyIni_KeyExists(INIDATA *Ini, CONST TCHAR *SecName, CONST TCHAR *KeyName);

/*------------------------------------------------*
** 取得関数
**------------------------------------------------*/
extern TCHAR *MyIni_GetString2(INIDATA *Ini, CONST TCHAR *Section, CONST TCHAR *KeyName, TCHAR *Buf, size_t Size, CONST TCHAR *DefParam);
extern TCHAR *MyIni_GetString(INISEC *Sec, CONST TCHAR *KeyName, TCHAR *Buf, size_t Size, CONST TCHAR *DefParam);

extern INT8 MyIni_GetInt8(INISEC *Sec, const TCHAR *KeyName, INT8 def);
extern INT16 MyIni_GetInt16(INISEC *Sec, const TCHAR *KeyName, INT16 def);
extern INT32 MyIni_GetInt32(INISEC *Sec, const TCHAR *KeyName, INT32 def);
extern INT64 MyIni_GetInt64(INISEC *Sec, const TCHAR *KeyName, INT64 def);

extern UINT8 MyIni_GetUint8(INISEC *Sec, const TCHAR *KeyName, UINT8 def);
extern UINT16 MyIni_GetUint16(INISEC *Sec, const TCHAR *KeyName, UINT16 def);
extern UINT32 MyIni_GetUint32(INISEC *Sec, const TCHAR *KeyName, UINT32 def);
extern UINT64 MyIni_GetUint64(INISEC *Sec, const TCHAR *KeyName, UINT64 def);

extern float MyIni_GetFloat32(INISEC *Sec, const TCHAR *KeyName, float def);
extern double MyIni_GetFloat64(INISEC *Sec, const TCHAR *KeyName, double def);

extern INT32 MyIni_GetBool(INISEC *Sec, const TCHAR *KeyName, INT32 def);

/*------------------------------------------------*
** 代入関数
**------------------------------------------------*/
extern void MyIni_SetString2(INIDATA *Ini, CONST TCHAR *SecName, CONST TCHAR *KeyName, CONST TCHAR *Data);
extern void MyIni_SetString(INISEC *Sec, CONST TCHAR *KeyName, CONST TCHAR *Data);

extern void MyIni_SetInt8(INISEC *Sec, const TCHAR *KeyName, INT8 prm);
extern void MyIni_SetInt16(INISEC *Sec, const TCHAR *KeyName, INT16 prm);
extern void MyIni_SetInt32(INISEC *Sec, const TCHAR *KeyName, INT32 prm);
extern void MyIni_SetInt64(INISEC *Sec, const TCHAR *KeyName, INT64 prm);

extern void MyIni_SetUint8(INISEC *Sec, const TCHAR *KeyName, UINT8 prm);
extern void MyIni_SetUint16(INISEC *Sec, const TCHAR *KeyName, UINT16 prm);
extern void MyIni_SetUint32(INISEC *Sec, const TCHAR *KeyName, UINT32 prm);
extern void MyIni_SetUint64(INISEC *Sec, const TCHAR *KeyName, UINT64 prm);

extern void MyIni_SetFloat32(INISEC *Sec, const TCHAR *KeyName, float prm);
extern void MyIni_SetFloat64(INISEC *Sec, const TCHAR *KeyName, double prm);

extern void MyIni_SetBool(INISEC *Sec, const TCHAR *KeyName, INT32 prm);

extern void MyIni_SetParam(INISEC *Sec, CONST TCHAR *KeyName, UINT64 Param, size_t Size, MyIni_DataType DataType);
extern CONST void *MyIni_GetParam(INISEC *Sec, CONST TCHAR *KeyName, void *bf, size_t Size, MyIni_DataType DataType);
extern LPINISEC MyIni_GetSection(INIDATA *Ini, CONST TCHAR *Sec, int AutoCreateEnable);
extern UINT64 MyIni_DoubleToInt64(double v);
extern void MyIni_CreateIniCFile(CONST TCHAR *fn);

extern const TCHAR *MyIni_PathFindFileName(CONST TCHAR *str);



#endif
