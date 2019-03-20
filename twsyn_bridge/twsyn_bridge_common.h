
#pragma once





#define COMMON_FM_NAME     "twsyn_bridge_file_mapping"
#define COMMON_MUTEX_NAME  "twsyn_bridge_mutex"

#define BRIDGE_MAX_PORT 4 // see rtsyn.h MAX_PORT
#define BRIDGE_MAX_EXBUF 32
#define BRIDGE_TOTAL_EXBUF (BRIDGE_MAX_PORT * BRIDGE_MAX_EXBUF)
#define BRIDGE_BUFF_SIZE 512

typedef struct _fm_bridge_t {
	// exit flag
	long exit;
	// host
	long PrcsIdHost; // processID
	long PrcsVerHost; // processVersion
	unsigned long long hControlWndHost; // window handle
	unsigned long uControlMessHost; // window message number
	// bridge
	long PrcsId; // processID
	long PrcsVer; // processVersion
	unsigned long long hControlWnd; // window handle
	unsigned long uControlMess; // window message number
	// midi in
	unsigned long long hMidiWnd[BRIDGE_MAX_PORT]; // window handle
	long midi_dev_num;
	char midi_devs[33][256];
	long portnumber;
	unsigned int portID[BRIDGE_MAX_PORT];
	long open_midi_dev;
	unsigned long wMsg;
	unsigned long long dwInstance;
	unsigned long long dwParam1;
	unsigned long long dwParam2;
	unsigned long dwBytes[BRIDGE_TOTAL_EXBUF];
	char lpData[BRIDGE_TOTAL_EXBUF][BRIDGE_BUFF_SIZE];
} fm_bridge_t;

enum {
	WMC_CLOSE_BRIDGE = 4096, // 0でもいいけど
	WMC_GET_MIDI_DEVS,
	WMC_OPEN_MIDI_DEVS,
	WMC_CLOSE_MIDI_DEVS,
	WMC_MIM_DATA,
	WMC_MIM_LONGDATA,
};
