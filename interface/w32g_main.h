#ifndef ___W32G_MAIN_H_
#define ___W32G_MAIN_H_

extern HWND hStartWnd;
extern HWND hMainWnd;
extern HWND hDebugWnd;
extern HWND hConsoleWnd;
extern HWND hTraceWnd;
extern HWND hDocWnd;
extern HWND hSoundSpecWnd;
extern HWND hListWnd;
extern HWND hWrdWnd;

extern HINSTANCE hInst;

extern void InitStartWnd(int nCmdShow);
extern void InitMainWnd(HWND hParentWnd);
extern void UpdateListWnd(void);

// flags
extern int InitMinimizeFlag;
extern int DebugWndStartFlag;
extern int ConsoleWndStartFlag;
extern int ListWndStartFlag;
extern int TracerWndStartFlag;
extern int DocWndStartFlag;
extern int WrdWndStartFlag;
extern int DebugWndFlag;
extern int ConsoleWndFlag;
extern int ListWndFlag;
extern int TracerWndFlag;
extern int DocWndFlag;
extern int WrdWndFlag;
extern int SoundSpecWndFlag;

extern int SubWindowMax;

extern char *IniFile;
extern char *ConfigFile;
extern char *PlaylistFile;
extern char *PlaylistHistoryFile;
extern char *MidiFileOpenDir;
extern char *ConfigFileOpenDir;
extern char *PlaylistFileOpenDir;

extern int ProcessPriority;
extern int PlayerThreadPriority;
extern int MidiPlayerThreadPriority;
extern int MainThreadPriority;
extern int TracerThreadPriority;
extern int WrdThreadPriority;

extern int WrdGraphicFlag;
extern int TraceGraphicFlag;
extern int DocMaxSize;
extern char *DocFileExt;


// Main Window
extern int MainWndScrollbarProgressNotUpdate;
extern void MainWndScrollbarProgressUpdate(void);
extern void MainWndScrollbarProgressReset(void);
extern void MainWndScrollbarProgressInit(void);
extern void MainWndScrollbarProgressApply(int pos);
extern HWND hMainWndScrollbarProgressWnd;

extern int MainWndScrollbarVolumeNotUpdate;
extern void MainWndScrollbarVolumeUpdate(void);
extern void MainWndScrollbarVolumeReset(void);
extern void MainWndScrollbarVolumeInit(void);
extern void MainWndScrollbarVolumeApply(int pos);
extern HWND hMainWndScrollbarVolumeWnd;

// Canvas Window
#define TMCM_SLEEP			0
#define TMCM_CHANNEL		1
#define TMCM_32CHANNEL	2
#define TMCM_FREQUENCY	3
#define TMCM_TRACER			4
extern int TmCanvasMode;
extern void TmCanvasUpdate(void);
extern void TmCanvasFullReset(void);
extern void TmCanvasReset(void);
extern void TmCanvasPartReset(void);
extern void TmCanvasSet(void);
extern int TmPanelMode;
extern void TmPanelFullReset(void);
extern void TmPanelReset(void);
extern void TmPanelPartReset(void);
extern void TmPanelUpdate(void);
extern void TmPanelSet(void);

// Debug Window
#ifdef WIN32GUI_DEBUG
extern void InitDebugWnd(HWND hParentWnd);
extern void PutsDebugWnd(char *str);
extern void PrintfDebugWnd(char *fmt, ...);
extern void ClearDebugWnd(void);
#endif

// Console Window
extern void InitConsoleWnd(HWND hParentWnd);
extern void PutsConsoleWnd(char *str);
extern void PrintfConsoleWnd(char *fmt, ...);
extern void ClearConsoleWnd(void);

// Tracer Window
extern void InitTracerWnd(HWND hParentWnd);

// List Window
extern void InitListWnd(HWND hParentWnd);

// Doc Window
extern void InitDocWnd(HWND hParentWnd);
extern void PutsDocWnd(char *str);
extern void PrintfDocWnd(char *fmt, ...);
extern void ClearDocWnd(void);

// Wrd Window
extern void InitWrdWnd(HWND hParentWnd);

// SoundSpec Window
extern void InitSoundSpecWnd(HWND hParentWnd);

// 	Misc Window

#define WM_TM_CANVAS_UPDATE (WM_USER + 101)

// Edit Ctl
extern void PutsEditCtlWnd(HWND hwnd, char *str);
extern void VprintfEditCtlWnd(HWND hwnd, char *fmt, va_list argList);
extern void PrintfEditCtlWnd(HWND hwnd, char *fmt, ...);
extern void ClearEditCtlWnd(HWND hwnd);

// misc util.

extern void DlgMidiFileOpen(void);

#endif /* ___W32G_MAIN_H_ */
