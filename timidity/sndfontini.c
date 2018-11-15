#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "timidity.h"
#include "instrum.h"
#include "common.h"
#define MYINI_LIBRARY_DEFIND_VAR
#include "myini.h"
#include "sndfontini.h"


Sample OverrideSample = {0};
OVERRIDETIMIDITYDATA otd = {0};

#if defined(__W32__)

#include <windows.h>

#if defined(WINDRV) || defined(WINDRV_SETUP)

void timdrvOverrideSFSettingLoad(void)
{
	INIDATA ini={0};
	LPINISEC sec = NULL;

	char fn[FILEPATH_MAX] = "";
	GetWindowsDirectory(fn, FILEPATH_MAX - 1);
	if (IS_PATH_SEP(fn[strlen(fn) - 1]))
		fn[strlen(fn) - 1] = 0;
	strlcat(fn, "\\", FILEPATH_MAX);
	strlcat(fn, "timdrv_soundfont.ini", FILEPATH_MAX);

#include "loadsndfontini.h"
}

#elif defined(KBTIM) || defined(WINVSTI)

#pragma comment(lib, "shlwapi.lib")
#include <shlwapi.h>

void OverrideSFSettingLoad(const char *kbini, int size)
{
	INIDATA ini={0};
	LPINISEC sec = NULL;
	char fn[FILEPATH_MAX] = "";

#if 1 // directory kbtim.kpi
	lstrcpyn(fn, (char *)kbini, size);
	PathRemoveFileSpec(fn);
	strcat(fn,"\\");
#else // directory Kbmplay.exe
    if(GetModuleFileName(GetModuleHandle(0), fn, FILEPATH_MAX - 1)){
		PathRemoveFileSpec(fn);
		strcat(fn,"\\");
	}else{
		fn[0] = '.';
		fn[1] = PATH_SEP;
		fn[2] = '\0';
    }
#endif
    strlcat(fn,"soundfont.ini",FILEPATH_MAX);
#include "loadsndfontini.h"
}

#elif defined(IA_W32GUI) || defined(IA_W32G_SYN)

#pragma comment(lib, "shlwapi.lib")
#include <shlwapi.h>

void OverrideSFSettingLoad()
{
	INIDATA ini={0};
	LPINISEC sec = NULL;

	TCHAR fn[FILEPATH_MAX] = {0};
    if(GetModuleFileName(GetModuleHandle(0), fn, FILEPATH_MAX - 1)){
		PathRemoveFileSpec(fn);
		_tcscat(fn, _T("\\"));
	}else{
		fn[0] = _T('.');
		fn[1] = _T(PATH_SEP);
		fn[2] = _T('\0');
    }
	_tcsncat(fn, _T("soundfont.ini"), FILEPATH_MAX);
#include "loadsndfontini.h"
}

#elif !defined(__W32G__)

void OverrideSFSettingLoad()
{
	INIDATA ini={0};
	LPINISEC sec = NULL;

	char fn[FILEPATH_MAX] = "";
	char *p = NULL;
    if(GetModuleFileName(GetModuleHandle(0), fn, FILEPATH_MAX - 1)){
		if((p = pathsep_strrchr(fn)) != NULL){
			p++;
			*p = '\0';
		}else{
			fn[0] = '.';
			fn[1] = PATH_SEP;
			fn[2] = '\0';
		}
	}else{
		fn[0] = '.';
		fn[1] = PATH_SEP;
		fn[2] = '\0';
    }

    strlcat(fn,"soundfont.ini",FILEPATH_MAX);

#include "loadsndfontini.h"

	printf("\n**********************************\nsoundfont.ini load ... ok\n");
}

#endif

#else /* ! defined(__W32__) */ 

void OverrideSFSettingLoad()
{
	INIDATA ini={0};
	LPINISEC sec = NULL;
#ifdef DEFAULT_PATH
	char fn[FILEPATH_MAX] = DEFAULT_PATH;
#else
	char fn[FILEPATH_MAX] = "usr/local/share/timidity";
#endif
	strlcat(fn, PATH_STRING, FILEPATH_MAX);
    strlcat(fn, "soundfont.ini", FILEPATH_MAX);
#include "loadsndfontini.h"
}

#endif /* defined(__W32__) */
