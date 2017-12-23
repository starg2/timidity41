#include "config.h"
#include "timidity.h"
#include "common.h"
#include "output.h"
#include "controls.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "wrd.h"
#include "strtab.h"
#include "arc.h"

//グローバル変数

//int audio_buffer_bits = DEFAULT_AUDIO_BUFFER_BITS;
//volatile int data_block_bits = DEFAULT_AUDIO_BUFFER_BITS;
//volatile int data_block_num = 64;

//PlayMode
//PlayMode *play_mode_list[] = {NULL};
//PlayMode *play_mode = NULL;
//PlayMode *target_play_mode = NULL;

//ControlMode
ControlMode *ctl_list[]=
{
  NULL
};
ControlMode *ctl=NULL;

//WRD
static int  null_wrdt_open(char *wrdt_opts) { return 0; }
static void null_wrdt_apply(int cmd, int argc, int args[]) { }
static void null_wrdt_update_events(void) { }
static void null_wrdt_end(void) { }
static void null_wrdt_close(void) { }

WRDTracer null_wrdt_mode =
{
    "No WRD trace", '-',
    0,
    null_wrdt_open,
    null_wrdt_apply,
    NULL,
    null_wrdt_update_events,
    NULL,
    null_wrdt_end,
    null_wrdt_close
};

WRDTracer *wrdt_list[] =
{
    &null_wrdt_mode,
    0
};

WRDTracer *wrdt = &null_wrdt_mode;

void wrd_midi_event(int cmd, int arg)
{
}

void wrd_sherry_event(int addr)
{
}


//リンカエラー回避のための苦肉の策（ぉ
StringTable wrd_read_opts;//timidity.c
int dumb_error_count;     //timidity.c

ArchiveEntryNode *next_mime_entry(void)
{
    return NULL;
}

void ML_RegisterAllLoaders (void)// timidity.c
{
}

URL url_cache_open(URL url, int autoclose)//common.c, readmidi.c, arc.c
{
    return NULL;
}

void url_cache_disable(URL url)//readmidi.c
{
}
int get_module_type (char *fn)//readmidi.c
{
    return IS_OTHER_FILE;
}
void convert_mod_to_midi_file(MidiEvent * ev)//playmidi.c
{
}

///r
void free_wrd(void)
{
}

void wave_set_option_extensible(int arg) { };
void wave_set_option_update_step(int arg) { };
//ここまで
