/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    ao_a.c
	Written by Iwata <b6330015@kit.jp>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <ao/ao.h>

#include "timidity.h"
#include "output.h"
#include "controls.h"
#include "timer.h"
#include "instrum.h"
#include "playmidi.h"
#include "miditrace.h"
#include "common.h"

static int opt_ao_device_id = -2;

static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static int output_data(char *buf, int32 nbytes);
static int acntl(int request, void *arg);
static int detect(void);
static void ao_set_options(int, ao_option **);
static void safe_ao_append_option(ao_option **, const char *, const char *);

/* export the playback mode */

#define dpm ao_play_mode

PlayMode dpm = {
  DEFAULT_RATE, PE_SIGNED|PE_16BIT, PF_PCM_STREAM,
  -1,
  {0}, /* default: get all the buffer fragments you can */
  "Libao mode", 'O',
  NULL, /* edit your ~/.libao */
  open_output,
  close_output,
  output_data,
  acntl,
  detect
};

static ao_device *ao_device_ctx;

static void safe_ao_append_option(ao_option **options, const char *key,
                                  const char *value)
{
  if (ao_append_option(options, key, value) == 1) return;
  else {
    ctl->cmsg(CMSG_FATAL, VERB_NORMAL,
              "Fatal error: ao_append_option has failed to allocate memory");
#ifdef ABORT_AT_FATAL
    abort();
#endif /* ABORT_AT_FATAL */
    safe_exit(10);
    /*NOTREACHED*/
  }
}

static void ao_set_options(int driver_id, ao_option **options)
{
  char *opt_string, *p, *token, *value;
  const char *env_ao_opts = getenv ("TIMIDITY_AO_OPTIONS");

  if (env_ao_opts == NULL) return;

  opt_string = safe_strdup(env_ao_opts);
  p = opt_string;

  while (p) {
    token = p;
    p = strchr(token, ',');
    if (p != NULL) *p++ = '\0';
    value = strchr(token, '=');
    if ((value == NULL) || (value == token)) continue;
    *value = '\0';
    safe_ao_append_option(options, token, value+1);
  }

  free(opt_string);
}

static ao_sample_format ao_sample_format_ctx;

static void show_ao_device_info(FILE *fp)
{
  int driver_count;
  ao_info **devices;
  int i;

  ao_initialize();

  devices  = ao_driver_info_list(&driver_count);
  if (driver_count < 1) {
	  fputs("*no device found*" NLS, fp);
  }
  else {
	  for(i = 0; i < driver_count; i++) {
		  if (devices[i]->type == AO_TYPE_LIVE)
			  fprintf(fp, "%d %s \n", ao_driver_id(devices[i]->short_name), devices[i]->short_name);
	  }
  }
  ao_shutdown();
}


static int open_output(void)
{
  int driver_id, ret = 0;

  int driver_count;
  ao_info **devices;
  ao_option *options = NULL;
  int i;

  ao_initialize();

  opt_ao_device_id = -2;
  devices  = ao_driver_info_list(&driver_count);
  if ((driver_count > 0) && (dpm.name != NULL)) {
    for(i = 0; i < driver_count; i++) {
      if(  (devices[i]->type == AO_TYPE_LIVE) 
        && (strcmp(dpm.name, devices[i]->short_name) == 0)  ){
        opt_ao_device_id = ao_driver_id(dpm.name);
      }
    }
  }

  if (opt_ao_device_id == -2){
    if(dpm.name != NULL)
      ret = sscanf(dpm.name, "%d", &opt_ao_device_id);
    if ( dpm.name == NULL || ret == 0 || ret == EOF)
      opt_ao_device_id = -2;
  }

  if (opt_ao_device_id == -1){
    ao_shutdown();
    show_ao_device_info(stdout);
    return -1;
  }

  if (opt_ao_device_id==-2) {
    driver_id = ao_default_driver_id();
  }
  else {
    ao_info *device;

    driver_id = opt_ao_device_id;
    if ((device = ao_driver_info(driver_id)) == NULL) {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: driver is not supported.",
		dpm.name);
      return -1;
    }
    if (device->type == AO_TYPE_FILE) {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: file output is not supported.",
		dpm.name);
      return -1;
    }
  }

  if (driver_id == -1) {
    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s",
	      dpm.name, strerror(errno));
    return -1;
  }

  /* They can't mean these */
  dpm.encoding &= ~(PE_ULAW|PE_ALAW|PE_BYTESWAP);

  ao_sample_format_ctx.channels = (dpm.encoding & PE_MONO) ? 1 : 2;
  ao_sample_format_ctx.rate = dpm.rate;
  ao_sample_format_ctx.byte_format = AO_FMT_NATIVE;
  ao_sample_format_ctx.bits = (dpm.encoding & PE_24BIT) ? 24 : 0;
  ao_sample_format_ctx.bits = (dpm.encoding & PE_16BIT) ? 16 : 0;
  if (ao_sample_format_ctx.bits == 0)
    ao_sample_format_ctx.bits = 8;

  ao_set_options(driver_id, &options);

  if ((ao_device_ctx = ao_open_live(driver_id, &ao_sample_format_ctx, options)) == NULL) {
    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s",
	      dpm.name, strerror(errno));
    ao_free_options(options);
    return -1;
  }
  ao_free_options(options);
  return 0;
}

static int output_data(char *buf, int32 nbytes)
{
  if (ao_play(ao_device_ctx, buf, nbytes) == 0) {
    ctl->cmsg(CMSG_WARNING, VERB_VERBOSE, "%s: %s",
	      dpm.name, strerror(errno));
    return -1;
  }
  return 0;
}

static void close_output(void)
{
  if (ao_device_ctx != NULL) {
    ao_close(ao_device_ctx);
    ao_device_ctx = NULL;
    ao_shutdown();
  }
}

static int acntl(int request, void *arg)
{
  switch(request) {
  case PM_REQ_DISCARD:
  case PM_REQ_PLAY_START: /* Called just before playing */
  case PM_REQ_PLAY_END: /* Called just after playing */
    return 0;
  }
  return -1;
}

static int detect(void)
{
  int driver_id, result = 0;
  ao_sample_format ao_sample_format_ctx;
  ao_device *ao_device_ctx;

  ao_initialize();

  /* Only succeed in autodetect mode when pulseaudio is available! */
  driver_id = ao_driver_id("pulse");

  ao_sample_format_ctx.rate = 44100;
  ao_sample_format_ctx.bits = 16;
  ao_sample_format_ctx.channels = 2;
  ao_sample_format_ctx.byte_format = AO_FMT_NATIVE;
  ao_sample_format_ctx.matrix = NULL;

  if ((ao_device_ctx = ao_open_live(driver_id, &ao_sample_format_ctx, NULL))) {
    result = 1;
    ao_close(ao_device_ctx);
  }

  ao_shutdown();

  return result;
}
