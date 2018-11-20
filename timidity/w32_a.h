
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
#include <mmsystem.h>

///r
#define DEVLIST_MAX 20
#define WMME_DEVICE_NAME_MAX_LENGTH 64

typedef struct tag_DEVICELIST {
	int  deviceID;
	char name[WMME_DEVICE_NAME_MAX_LENGTH];
} DEVICELIST;

//extern int data_block_bits;
//extern int data_block_num;
///r
extern int opt_wmme_buffer_bits;
extern int opt_wmme_buffer_num;
extern int opt_wave_format_ext;
extern int opt_wmme_device_id;
extern int wmme_device_list(DEVICELIST *device);

