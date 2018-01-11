
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "interface.h"
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


#define WDMKS_DEVLIST_MAX 20
#define WDMKS_DEVLIST_LEN 64
#define WDMKS_MAX_STR_LEN 512

typedef struct tag_WDMKS_DEVICELIST {
	int  deviceID;
	char name[WDMKS_DEVLIST_LEN];
} WDMKS_DEVICELIST;

extern CRITICAL_SECTION critSect;

extern int opt_wdmks_device_id;
extern int opt_wdmks_format_ext;
extern int opt_wdmks_priority;
extern int wdmks_device_list(WDMKS_DEVICELIST *device);
