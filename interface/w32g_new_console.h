
#pragma once

#ifdef TIMW32G_USE_NEW_CONSOLE

void ClearNewConsoleBuffer(void);
void NewConsoleBufferWriteCMsg(int type, int verbosity_level, const char *str);

void InitializeNewConsole(void);
void NewConsoleClear(HWND hwnd);
void NewConsoleWrite(HWND hwnd, const char *str);
void NewConsoleWriteV(HWND hwnd, const char *format, va_list args);

#endif // TIMW32G_USE_NEW_CONSOLE
