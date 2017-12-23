# Microsoft Developer Studio Project File - Name="kbtim_dll" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** 編集しないでください **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=kbtim_dll - Win32 Debug
!MESSAGE これは有効なﾒｲｸﾌｧｲﾙではありません。 このﾌﾟﾛｼﾞｪｸﾄをﾋﾞﾙﾄﾞするためには NMAKE を使用してください。
!MESSAGE [ﾒｲｸﾌｧｲﾙのｴｸｽﾎﾟｰﾄ] ｺﾏﾝﾄﾞを使用して実行してください
!MESSAGE 
!MESSAGE NMAKE /f "kbtim_dll.mak".
!MESSAGE 
!MESSAGE NMAKE の実行時に構成を指定できます
!MESSAGE ｺﾏﾝﾄﾞ ﾗｲﾝ上でﾏｸﾛの設定を定義します。例:
!MESSAGE 
!MESSAGE NMAKE /f "kbtim_dll.mak" CFG="kbtim_dll - Win32 Debug"
!MESSAGE 
!MESSAGE 選択可能なﾋﾞﾙﾄﾞ ﾓｰﾄﾞ:
!MESSAGE 
!MESSAGE "kbtim_dll - Win32 Release" ("Win32 (x86) Dynamic-Link Library" 用)
!MESSAGE "kbtim_dll - Win32 Debug" ("Win32 (x86) Dynamic-Link Library" 用)
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "kbtim_dll - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "kbtim_dll___Win32_Release"
# PROP BASE Intermediate_Dir "kbtim_dll___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "kbtim_dll_Release"
# PROP Intermediate_Dir "kbtim_dll_Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "KBTIM_DLL_EXPORTS" /YX /FD /c
# ADD CPP /nologo /G6 /MD /W3 /GX /O2 /I "..\interface" /I "..\libarc" /I "..\libunimod" /I "..\timidity" /I "..\utils" /I ".." /I "..\..\include" /I "..\portaudio\pa_common" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "KBTIM_DLL_EXPORTS" /D "HAVE_CONFIG_H" /D "_MT" /D "KBTIM" /D "MYINI_LIBRARY_DEFIND_VAR" /FR /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x411 /d "NDEBUG"
# ADD RSC /l 0x411 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386 /out:"../../kbtim.dll"

!ELSEIF  "$(CFG)" == "kbtim_dll - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "kbtim_dll___Win32_Debug"
# PROP BASE Intermediate_Dir "kbtim_dll___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "kbtim_dll_Debug"
# PROP Intermediate_Dir "kbtim_dll_Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "KBTIM_DLL_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "..\interface" /I "..\libarc" /I "..\libunimod" /I "..\timidity" /I "..\utils" /I ".." /I "..\..\include" /I "..\portaudio\pa_common" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "KBTIM_DLL_EXPORTS" /D "HAVE_CONFIG_H" /D "_MT" /D "KBTIM" /D "MYINI_LIBRARY_DEFIND_VAR" /FR /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x411 /d "_DEBUG"
# ADD RSC /l 0x411 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /out:"../../kbtim.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "kbtim_dll - Win32 Release"
# Name "kbtim_dll - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "timidity"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\timidity\aq.c
# End Source File
# Begin Source File

SOURCE=..\timidity\audio_cnv.c
# End Source File
# Begin Source File

SOURCE=..\timidity\common.c
# End Source File
# Begin Source File

SOURCE=..\timidity\effect.c
# End Source File
# Begin Source File

SOURCE=..\timidity\envelope.c
# End Source File
# Begin Source File

SOURCE=..\timidity\filter.c
# End Source File
# Begin Source File

SOURCE=..\timidity\freq.c
# End Source File
# Begin Source File

SOURCE=..\timidity\instrum.c
# End Source File
# Begin Source File

SOURCE=..\timidity\int_synth.c
# End Source File
# Begin Source File

SOURCE=..\timidity\loadtab.c
# End Source File
# Begin Source File

SOURCE=..\timidity\mfi.c
# End Source File
# Begin Source File

SOURCE=..\timidity\miditrace.c
# End Source File
# Begin Source File

SOURCE=..\timidity\mix.c
# End Source File
# Begin Source File

SOURCE=..\timidity\mt19937ar.c
# End Source File
# Begin Source File

SOURCE=..\timidity\myini.c
# End Source File
# Begin Source File

SOURCE=..\timidity\optcode.c
# End Source File
# Begin Source File

SOURCE=..\timidity\oscillator.c
# End Source File
# Begin Source File

SOURCE=..\timidity\output.c
# End Source File
# Begin Source File

SOURCE=..\timidity\playmidi.c
# End Source File
# Begin Source File

SOURCE=..\timidity\quantity.c
# End Source File
# Begin Source File

SOURCE=..\timidity\rcp.c
# End Source File
# Begin Source File

SOURCE=..\timidity\readmidi.c
# End Source File
# Begin Source File

SOURCE=..\timidity\recache.c
# End Source File
# Begin Source File

SOURCE=..\timidity\resample.c
# End Source File
# Begin Source File

SOURCE=..\timidity\sbkconv.c
# End Source File
# Begin Source File

SOURCE=..\timidity\sffile.c
# End Source File
# Begin Source File

SOURCE=..\timidity\sfitem.c
# End Source File
# Begin Source File

SOURCE=..\timidity\smfconv.c
# End Source File
# Begin Source File

SOURCE=..\timidity\smplfile.c
# End Source File
# Begin Source File

SOURCE=..\timidity\sndfont.c
# End Source File
# Begin Source File

SOURCE=..\timidity\sndfontini.c
# End Source File
# Begin Source File

SOURCE=..\timidity\tables.c
# End Source File
# Begin Source File

SOURCE=..\timidity\thread.c
# End Source File
# Begin Source File

SOURCE=..\timidity\timidity.c
# End Source File
# Begin Source File

SOURCE=..\timidity\version.c
# End Source File
# Begin Source File

SOURCE=..\timidity\voice_effect.c
# End Source File
# End Group
# Begin Group "url"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\libarc\arc.c
# End Source File
# Begin Source File

SOURCE=..\libarc\arc_lzh.c
# End Source File
# Begin Source File

SOURCE=..\libarc\arc_tar.c
# End Source File
# Begin Source File

SOURCE=..\libarc\arc_zip.c
# End Source File
# Begin Source File

SOURCE=..\libarc\deflate.c
# End Source File
# Begin Source File

SOURCE=..\libarc\explode.c
# End Source File
# Begin Source File

SOURCE=..\libarc\inflate.c
# End Source File
# Begin Source File

SOURCE=..\libarc\unlzh.c
# End Source File
# Begin Source File

SOURCE=..\libarc\url.c
# End Source File
# Begin Source File

SOURCE=..\libarc\url_dir.c
# End Source File
# Begin Source File

SOURCE=..\libarc\url_file.c
# End Source File
# Begin Source File

SOURCE=..\libarc\url_inflate.c
# End Source File
# Begin Source File

SOURCE=..\libarc\url_mem.c
# End Source File
# End Group
# Begin Group "utils"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\utils\fft4g.c
# End Source File
# Begin Source File

SOURCE=..\utils\getopt.c
# End Source File
# Begin Source File

SOURCE=..\utils\mblock.c
# End Source File
# Begin Source File

SOURCE=..\utils\nkflib.c
# End Source File
# Begin Source File

SOURCE=..\utils\readdir_win.c
# End Source File
# Begin Source File

SOURCE=..\utils\strtab.c
# End Source File
# Begin Source File

SOURCE=..\utils\support.c
# End Source File
# Begin Source File

SOURCE=..\utils\timer.c
# End Source File
# End Group
# Begin Source File

SOURCE=.\_timidity_.c
# End Source File
# Begin Source File

SOURCE=.\kbtim_dll.cpp
# End Source File
# Begin Source File

SOURCE=.\kbtim_globals.c
# End Source File
# Begin Source File

SOURCE=.\kbtim_setting.cpp
# End Source File
# Begin Source File

SOURCE=.\ringbuf.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "timidity_h"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\timidity\myini.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\config.h
# End Source File
# Begin Source File

SOURCE=..\interface.h
# End Source File
# Begin Source File

SOURCE=.\kbtim.h
# End Source File
# Begin Source File

SOURCE=.\kbtim_common.h
# End Source File
# Begin Source File

SOURCE=.\kbtim_config.h
# End Source File
# Begin Source File

SOURCE=.\kbtim_interface.h
# End Source File
# Begin Source File

SOURCE=.\kbtim_setting.h
# End Source File
# Begin Source File

SOURCE=.\ringbuf.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=.\kbtim.txt
# End Source File
# Begin Source File

SOURCE=.\kbtim_dll.def
# End Source File
# Begin Source File

SOURCE=.\kbtim_memo.txt
# End Source File
# End Target
# End Project
