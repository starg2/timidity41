#ifndef ___W32G_SUBWIN_H_
#define ___W32G_SUBWIN_H_

// Console Window
void InitConsoleWnd(HWND hParentWnd);
void PutsConsoleWnd(char *str);
void PrintfConsoleWnd(char *fmt, ...);
void ClearConsoleWnd(void);

// Tracer Window
void InitTracerWnd(HWND hParentWnd);

// List Window
void InitListWnd(HWND hParentWnd);

// Doc Window
extern int DocWndIndependent;
void InitDocWnd(HWND hParentWnd);
void DocWndInfoReset(void);
void DocWndAddDocFile(char *filename);
void DocWndSetMidifile(char *filename);
void DocWndReadDoc(int num);
void DocWndReadDocNext(void);
void DocWndReadDocPrev(void);

void PutsDocWnd(char *str);
void PrintfDocWnd(char *fmt, ...);
void ClearDocWnd(void);

// Wrd Window
void InitWrdWnd(HWND hParentWnd);

// SoundSpec Window
void InitSoundSpecWnd(HWND hParentWnd);

void w32g_setup_doc(int idx);
void w32g_open_doc(int close_if_no_doc);

#endif /* ___W32G_SUBWIN_H_ */
