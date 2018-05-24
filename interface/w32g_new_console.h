
#pragma once

#ifdef TIMW32G_USE_NEW_CONSOLE

void ClearNewConsoleBuffer(void);
void NewConsoleBufferWriteCMsg(int type, int verbosity_level, LPCTSTR str);

void InitializeNewConsole(void);
void NewConsoleClear(HWND hwnd);
void NewConsoleWrite(HWND hwnd, LPCTSTR str);
void NewConsoleWriteV(HWND hwnd, LPCTSTR format, va_list args);

#endif // TIMW32G_USE_NEW_CONSOLE
