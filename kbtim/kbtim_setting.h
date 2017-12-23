#ifndef KBTIM_SETTING_H
#define KBTIM_SETTING_H

class KbTimSetting
{
    SETTING_TIMIDITY m_st;
    char m_szIniFile[MAX_PATH*2];
    char m_szCfgFile[MAX_PATH*2];
    
    FILETIME m_ftIni;
    FILETIME m_ftCfg;
    HANDLE   m_hNotification;

    int __fastcall IniGetKeyInt(char *section, char *key,int *n);
    int __fastcall IniGetKeyInt32(char *section, char *key,int32 *n){
        return IniGetKeyInt(section, key, (int*)n);
    }
    int __fastcall IniGetKeyInt8(char *section, char *key,int8 *n);
    int __fastcall IniGetKeyFloat(char *section, char *key, FLOAT_T *n);
    int __fastcall IniGetKeyIntArray(char *section, char *key, int *n, int arraysize);
    int __fastcall IniGetKeyStringN(char *section, char *key, char *str, int size);

    void __fastcall GetIniCfgFileTime(FILETIME* pftIni, FILETIME* pftCfg);
///r
	void __fastcall KbTimOverrideSFSettingLoad(void);
public:
    const char* __fastcall GetIniFileName(void)const{return m_szIniFile;}
    const char* __fastcall GetCfgFileName(void)const{return m_szCfgFile;}
    BOOL __fastcall LoadIniFile(const char *cszIniFile);
    void __fastcall Close(void);
    void __fastcall Apply(void);
    BOOL __fastcall IsUpdated(void);
    KbTimSetting(void);
    ~KbTimSetting(void);
};

#endif
