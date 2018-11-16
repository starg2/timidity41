
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

#ifndef const
	#define const const
#endif

#endif
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
	char *Name;
	char *Data;
	struct tagKey *Next;
}INIKEY, *LPINIKEY;

typedef struct tagSection {
	unsigned long Hash;
	char *Name;
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
extern void MyIni_Load(INIDATA *Ini, const char *str);
extern void MyIni_LoadT(INIDATA *Ini, const TCHAR *str);
extern void MyIni_Load_timidity(INIDATA *Ini, const char *str, int decompress, int noise_mode);

/*------------------------------------------------*
** ファイルセーブ
**------------------------------------------------*/
extern void MyIni_Save(INIDATA *Ini, const char *fn);
extern void MyIni_SaveT(INIDATA *Ini, const TCHAR *fn);

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
extern int MyIni_DeleteSection(INIDATA *Ini, const char *SecName);
extern int MyIni_DeleteKey(INIDATA *Ini, const char *SecName, const char *KeyName);

/*------------------------------------------------*
** セクション/キー検索関数
**------------------------------------------------*/
extern int MyIni_SectionExists(INIDATA *Ini, const char *SecName);
extern int MyIni_KeyExists(INIDATA *Ini, const char *SecName, const char *KeyName);

/*------------------------------------------------*
** 取得関数
**------------------------------------------------*/
extern char *MyIni_GetString2(INIDATA *Ini, const char *Section, const char *KeyName, char *Buf, size_t Size, const char *DefParam);
extern char *MyIni_GetString(INISEC *Sec, const char *KeyName, char *Buf, size_t Size, const char *DefParam);

extern INT8 MyIni_GetInt8(INISEC *Sec, const char *KeyName, INT8 def);
extern INT16 MyIni_GetInt16(INISEC *Sec, const char *KeyName, INT16 def);
extern INT32 MyIni_GetInt32(INISEC *Sec, const char *KeyName, INT32 def);
extern INT64 MyIni_GetInt64(INISEC *Sec, const char *KeyName, INT64 def);

extern UINT8 MyIni_GetUint8(INISEC *Sec, const char *KeyName, UINT8 def);
extern UINT16 MyIni_GetUint16(INISEC *Sec, const char *KeyName, UINT16 def);
extern UINT32 MyIni_GetUint32(INISEC *Sec, const char *KeyName, UINT32 def);
extern UINT64 MyIni_GetUint64(INISEC *Sec, const char *KeyName, UINT64 def);

extern float MyIni_GetFloat32(INISEC *Sec, const char *KeyName, float def);
extern double MyIni_GetFloat64(INISEC *Sec, const char *KeyName, double def);

extern INT32 MyIni_GetBool(INISEC *Sec, const char *KeyName, INT32 def);

/*------------------------------------------------*
** 代入関数
**------------------------------------------------*/
extern void MyIni_SetString2(INIDATA *Ini, const char *SecName, const char *KeyName, const char *Data);
extern void MyIni_SetString(INISEC *Sec, const char *KeyName, const char *Data);

extern void MyIni_SetInt8(INISEC *Sec, const char *KeyName, INT8 prm);
extern void MyIni_SetInt16(INISEC *Sec, const char *KeyName, INT16 prm);
extern void MyIni_SetInt32(INISEC *Sec, const char *KeyName, INT32 prm);
extern void MyIni_SetInt64(INISEC *Sec, const char *KeyName, INT64 prm);

extern void MyIni_SetUint8(INISEC *Sec, const char *KeyName, UINT8 prm);
extern void MyIni_SetUint16(INISEC *Sec, const char *KeyName, UINT16 prm);
extern void MyIni_SetUint32(INISEC *Sec, const char *KeyName, UINT32 prm);
extern void MyIni_SetUint64(INISEC *Sec, const char *KeyName, UINT64 prm);

extern void MyIni_SetFloat32(INISEC *Sec, const char *KeyName, float prm);
extern void MyIni_SetFloat64(INISEC *Sec, const char *KeyName, double prm);

extern void MyIni_SetBool(INISEC *Sec, const char *KeyName, INT32 prm);

extern void MyIni_SetParam(INISEC *Sec, const char *KeyName, UINT64 Param, size_t Size, MyIni_DataType DataType);
extern const void *MyIni_GetParam(INISEC *Sec, const char *KeyName, void *bf, size_t Size, MyIni_DataType DataType);
extern LPINISEC MyIni_GetSection(INIDATA *Ini, const char *Sec, int AutoCreateEnable);
extern UINT64 MyIni_DoubleToInt64(double v);
extern void MyIni_CreateIniCFile(const char *fn);

extern const char *MyIni_PathFindFileName(const char *str);



#endif
