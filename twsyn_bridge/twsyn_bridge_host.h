
#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef USE_TWSYN_BRIDGE

#include <windows.h>
#include <mmsystem.h>

#define EXE_NAME32 "twsyn_bridge_x86.exe"
#define EXE_NAME64 "twsyn_bridge_x64.exe"

#ifdef _WIN64
#define EXE_NAME EXE_NAME32
#define BRD_EXE_NAME EXE_NAME64
#define FILE_HANDLE ((HANDLE)0xffffffffffffffff)
#else
#define EXE_NAME EXE_NAME64
#define BRD_EXE_NAME EXE_NAME32
#define FILE_HANDLE ((HANDLE)0xffffffff)
#endif

#define FILEPATH_MAX 32000


extern int opt_use_twsyn_bridge;
extern void close_bridge(void);
extern void init_bridge(void);
extern int get_bridge_midi_devs(void);
extern char *get_bridge_midi_dev_name(int num);
extern void open_bridge_midi_dev(int portnumber, unsigned int *portID);
extern void close_bridge_midi_dev(void);
extern int get_bridge_mim_databytes(int num);
extern char *get_bridge_mim_longdata(int num);

extern void CALLBACK MidiInProc(HMIDIIN hMidiInL, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2);

#endif // USE_BRIDGE





