
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "interface.h"
#include "timidity.h"

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <windows.h>

///r
#define PA_DEVLIST_MAX 60
#define PA_DEVLIST_LEN 64

#define DEVLIST_MAX 20

#define PA_DEVLIST_DEFAULT_NAME "(default)"
#define PA_DEVLIST_ERROR_NAME "(null)"

typedef struct tag_PA_DEVICELIST{
	int  deviceID;
	char name[256];
} PA_DEVICELIST;


///r
extern int opt_pa_wmme_device_id;
extern int opt_pa_ds_device_id;
extern int opt_pa_asio_device_id;
extern int opt_pa_wdmks_device_id;
extern int opt_pa_wasapi_device_id;
extern int opt_pa_wasapi_flag;
extern int opt_pa_wasapi_stream_category;
extern int opt_pa_wasapi_stream_option;

#ifndef AU_PORTAUDIO_DLL
extern PlayMode portaudio_play_mode;
#else
extern PlayMode portaudio_asio_play_mode;
#ifdef PORTAUDIO_V19
extern PlayMode portaudio_win_wasapi_play_mode;
extern PlayMode portaudio_win_wdmks_play_mode;
#endif
extern PlayMode portaudio_win_ds_play_mode;
extern PlayMode portaudio_win_wmme_play_mode;
extern PlayMode * volatile portaudio_play_mode;
#endif

extern int pa_device_list(PA_DEVICELIST *device,int HostApiTypeId);
