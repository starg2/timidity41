#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include "timidity.h"
#include "output.h"

static int open_output(void);
static void close_output(void);
static int output_data(char *buf, int32 bytes);
static int acntl(int request, void *arg);

PlayMode modmidi_play_mode = {
    DEFAULT_RATE,
    PE_16BIT|PE_SIGNED,
    PF_PCM_STREAM,
    -1,
    {0,0,0,0,0},
    "MOD -> MIDI file conversion", 'm',
    NULL,
    open_output,
    close_output,
    output_data,
    acntl
};

static int open_output(void)
{
    modmidi_play_mode.fd = 0;
    return 0;
}

static void close_output(void)
{
    modmidi_play_mode.fd = -1;
}

static int output_data(char *buf, int32 bytes)
{
    return bytes;
}

static int acntl(int request, void *arg)
{
    switch(request)
    {
      case PM_REQ_DISCARD:
	return 0;
    }
    return -1;
}
