
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "timidity.h"
#include "common.h"

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


#define WASAPI_DEVLIST_MAX 20
#define WASAPI_DEVLIST_LEN 64
#define WASAPI_MAX_STR_LEN 512

typedef struct tag_WASAPI_DEVICELIST {
	int  deviceID;
	char name[WASAPI_DEVLIST_LEN];
	int32 LatencyMax;
	int32 LatencyMin;
} WASAPI_DEVICELIST;

extern int32 opt_wasapi_device_id;
extern int32 opt_wasapi_latency;
extern int opt_wasapi_format_ext;
extern int opt_wasapi_exclusive;
extern int opt_wasapi_polling;
extern int opt_wasapi_priority;
extern int opt_wasapi_stream_category;
extern int opt_wasapi_stream_option;
extern int wasapi_device_list(WASAPI_DEVICELIST *device);
