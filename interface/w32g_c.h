#ifndef ___W32G_C_H_
#define ___W32G_C_H_


#define PLAYERSTATUS_NONE 		0x0000
#define PLAYERSTATUS_STOP		0x0001
#define PLAYERSTATUS_PAUSE		0x0002
#define PLAYERSTATUS_PLAY		0x0003
#define PLAYERSTATUS_PLAYSTART		0x0004
#define PLAYERSTATUS_DEMANDPLAY		0x0005
#define PLAYERSTATUS_PLAYEND		0x0006
#define PLAYERSTATUS_PLAYERROREND	0x0007
#define PLAYERSTATUS_QUIT		0x0008
#define PLAYERSTATUS_ERROR 		0x0009

extern volatile int player_status;

void PlayerOnPlay(void);
void PlayerOnQuit(void);
void PlayerOnStop(void);
void PlayerOnNextPlay(void);
void PlayerOnPrevPlay(void);
void PlayerOnPlayLoadFile(void);
int PlayerOnPause(void);
void PlayerOnKill(void);
void PlayerPlaylistNum(void);
PLAYLIST *PlayerPlaylistAddFilename(char *filename);
PLAYLIST *PlayerPlaylistAddExpandFileArchives(char *filename);
PLAYLIST *PlayerPlaylistAddFiles(int nfiles, char **files, int expand_file_archives_flag);
PLAYLIST *PlayerPlaylistAddDropfiles(HDROP hDrop);
PLAYLIST *PlayerPlaylistAddDropfilesEx(HDROP hDrop, int expand_file_archives_flag);

void LockPlaylist(void);
void UnLockPlaylist(void);
void SetCur_pl(PLAYLIST *pl);
void SetCur_plNum(int num);

extern PLAYLIST *playlist, *old_playlist, *history_playList;
extern PLAYLIST *cur_pl;
extern int cur_pl_num;
extern int playlist_num;

#define CTLREADBUFFERMAX 128
typedef struct CRBLOOPBUFFER_ {
	int cmd;
	int32 val;
} CRBLOOPBUFFER;

void PutCrbLoopBuffer(int cmd, int32 val);
CRBLOOPBUFFER *GetCrbLoopBuffer(void);

#define MAX_W32G_MIDI_CHANNELS	32

#define FLAG_NOTE_OFF	1
#define FLAG_NOTE_ON	2

#define FLAG_BANK	0x0001
#define FLAG_PROG	0x0002
#define FLAG_PAN	0x0004
#define FLAG_SUST	0x0008

typedef struct {
	int reset_panel;
	int wait_reset;
	int multi_part;

	char v_flags[MAX_W32G_MIDI_CHANNELS];
	int16 cnote[MAX_W32G_MIDI_CHANNELS];
	int16 cvel[MAX_W32G_MIDI_CHANNELS];
	int16 ctotal[MAX_W32G_MIDI_CHANNELS];
	char c_flags[MAX_W32G_MIDI_CHANNELS];
	Channel channel[MAX_W32G_MIDI_CHANNELS];

	int32 total_time;
	int total_time_h;
	int total_time_m;
	int total_time_s;
	int total_time_ss;
	int32 cur_time;
	int cur_time_h;
	int cur_time_m;
	int cur_time_s;
	int cur_time_ss;
	int cur_voices;
	int voices;
  int upper_voices;
	char filename[MAXPATH + 64];
	char titlename[MAXPATH + 64];
  int filename_setflag;
  int titlename_setflag;
	int32 master_volume;
	int32 master_volume_max;
	int invalid_flag;

	int32 xnote[MAX_W32G_MIDI_CHANNELS][4];

	char dummy[1024];
} VOLATILE PanelInfo;

extern PanelInfo *Panel;

void PanelReset(void);
void PanelPartReset(void);

extern void Player_loop(int init_flag);
extern void PlayerOnPlayEx(int rc);

#endif /* ___W32G_C_H_ */
