/*

    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999 Masanao Izumo <mo@goice.co.jp>
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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdlib.h>
#ifndef NO_STRING_H
#include <string.h>
#endif
#include <fcntl.h>
#include <sys/ioctl.h>
#ifndef	__NetBSD__
#include <stropts.h>
#endif
#include <sys/types.h>
#include <sys/filio.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#ifdef SUNAUDIO_BUG_FIX
#include "timer.h"
static double play_start_time = 0.0;
static long play_sample_size = 1;
static long play_counter, reset_samples;
#endif

#include "timidity.h"
#include "audriv.h"
#include "aenc.h"

#if defined(__NetBSD__)		/* for NetBSD */
#include <sys/audioio.h>
#ifdef LITTLE_ENDIAN
#define AUDRIV_LINEAR_TAG AUDIO_ENCODING_SLINEAR_LE
#define AUDRIV_AENC_SIGWORD AENC_SIGWORDL
#else
#define AUDRIV_LINEAR_TAG AUDIO_ENCODING_SLINEAR_BE
#define AUDRIV_AENC_SIGWORD AENC_SIGWORDB
#endif
#elif defined(SOLARIS)		/* for Solaris 2.x */
#include <sys/audioio.h>
#define AUDRIV_LINEAR_TAG AUDIO_ENCODING_LINEAR
#ifdef LITTLE_ENDIAN
#define AUDRIV_AENC_SIGWORD AENC_SIGWORDL
#else
#define AUDRIV_AENC_SIGWORD AENC_SIGWORDB
int usleep(unsigned int useconds); /* shut gcc warning up */
#endif
#else				/* for SunOS 4.x */
#include <sun/audioio.h>
#define AUDRIV_LINEAR_TAG AUDIO_ENCODING_LINEAR
#define AUDRIV_AENC_SIGWORD AENC_SIGWORDB
#endif

#define AUDIO_DEV    "/dev/audio"
#define AUDIO_CTLDEV "/dev/audioctl"
void (* audriv_error_handler)(const char *errmsg) = NULL;

static int audriv_open_internal(int open_flag, Bool noblock);
static int sun_stop_playing(void);

static int audio_play_fd   = -1;
static int audioctl_fd     = -1;
static Bool audio_write_noblocking = False;
static long estimated_audio_buffer_size = 0;

static long internal_play_sample_rate = 8000;
static long internal_play_encoding = AUDIO_ENCODING_ULAW;
static int internal_play_channels = 1;

char audriv_errmsg[BUFSIZ];
#ifndef	__NetBSD__
extern char *sys_errlist[];
#endif

static const long available_sample_rates[] =
{
    8000, 9600, 11025, 16000, 18900, 22050, 32000, 37800, 44100, 48000
};
#define NALL_AVAIL_SAMPLE_RATE (sizeof(available_sample_rates)/sizeof(long))
static int navail_sample_rate = NALL_AVAIL_SAMPLE_RATE;

static const long available_encodings[] =
{
    AENC_G711_ULAW,
    AENC_G711_ALAW,
    AUDRIV_AENC_SIGWORD
};
#define NALL_AVAIL_ENCODING (sizeof(available_encodings)/sizeof(long))
static int navail_encoding = NALL_AVAIL_ENCODING;

static const long available_channels[] =
{
    1, 2
};
#define NALL_AVAIL_CHANNELS (sizeof(available_channels)/sizeof(long))
static int navail_channels = NALL_AVAIL_CHANNELS;

enum sun_audriv_error_codes
{
    AUDRIV_ERROR_DEVICE_BUSY,
    AUDRIV_ERROR_NOT_AUDIO_DEV,
    AUDRIV_ERROR_INVALID_AUDIO_FORMAT,
    AUDRIV_ERROR_INVALID_OUTPUT
};

static const char *sun_audriv_error_text[] =
{
    "/dev/audio device is busy",
    "/dev/audio not a audio device",
    "Invalid audio format",
    "Invalid audio output"
};

static const char *sun_audriv_error_jtext[] =
{
    "/dev/audio は他のプロセスによって使用されています",
    "/dev/audio はオーディオデバイスではありません．",
    "指定された音声形式はデバイスがサポートしていません",
    "指定外の出力ポートが指定されました"
};

#ifdef DEBUG
int ioctl();
int fprintf(FILE *, char *, ...);
void perror(const char *);
unsigned ualarm(unsigned, unsigned);
#endif


static void audriv_syserr(const char *msg)
{
    sprintf(audriv_errmsg, "%s: %s", msg, sys_errlist[errno]);
    if(audriv_error_handler != NULL)
	audriv_error_handler(audriv_errmsg);
    else
	fprintf(stderr, "%s\n", audriv_errmsg);
}

static void audriv_err(long errcode)
{
    const char *msg;
    char *lang;

    if((lang = getenv("LANG")) != NULL &&
       (!strcmp(lang, "japanese") || !strcmp(lang, "ja")
                                  || !strcmp(lang, "ja_JP.EUC")))
	msg = sun_audriv_error_jtext[errcode];
    else
	msg = sun_audriv_error_text[errcode];
    strcpy(audriv_errmsg, msg);
    if(audriv_error_handler != NULL)
	audriv_error_handler(audriv_errmsg);
}

#ifdef SUNAUDIO_BUG_FIX
static long calculate_play_samples(void)
{
    double es;

    if(play_counter <= 0)
	return 0;

    es = (double)internal_play_sample_rate
	* (double)play_sample_size
	    * (get_current_calender_time() - play_start_time);
    if(es > play_counter)
	return play_counter / play_sample_size;
    return (long)(es / play_sample_size);
}
#endif

/* 使用可能なサンプルレートと符号化の簡易チェック */
static void check_audio_support(int fd)
{
    audio_info_t auinfo;

    AUDIO_INITINFO(&auinfo);

    /* SS5, SS10, SS20 は以下の設定で成功する．
     * 他の SPARC マシンは恐らく失敗する．
     */
    auinfo.play.sample_rate = 16000;
    auinfo.play.channels = 1;
    auinfo.play.encoding = AUDRIV_LINEAR_TAG;
    auinfo.play.precision = 16;
    if(ioctl(fd, AUDIO_SETINFO, &auinfo) < 0 && errno == EINVAL)
    {
	navail_sample_rate = 1;
	navail_encoding = 2;
    }

    auinfo.play.channels = 2;
    if(ioctl(fd, AUDIO_SETINFO, &auinfo) < 0)
	navail_channels = 1;
    else
	navail_channels = 2;
}

#define FRAGMSIZ 1024
static long fd_get_fillable(int fd)
{
    int arg;
    static char buff[FRAGMSIZ];
    long n, sum;

    arg = 1;
    if(ioctl(fd, FIONBIO, &arg) < 0)
	return FRAGMSIZ; /* ??? */
    sum = 0;
    while((n = write(fd, buff, FRAGMSIZ)) == FRAGMSIZ)
	sum += n;
    return sum;
}

Bool audriv_setup_audio(void)
{
    struct stat buf;

    /* まず最初に /dev/audioctl をオープンする．
     * /dev/audioctl は audriv_free_audio が呼び出されるまで
     * オープンしっぱなしにしておく．
     * 既にオープンされていた場合は何もしない．
     */
    if(audioctl_fd != -1)
	return True;
    if((audioctl_fd = open(AUDIO_CTLDEV, O_RDWR)) < 0)
    {
	audriv_syserr(AUDIO_CTLDEV);
	return False;
    }

    /* /dev/audio の簡易チェック */

    /* オープンできるかどうかのチェック */
    if((audio_play_fd = open(AUDIO_DEV, O_WRONLY | O_NDELAY)) < 0)
    {
	if(errno == EBUSY)
	{
	    audriv_err(AUDRIV_ERROR_DEVICE_BUSY);

	    /* O_NDELAY フラグを外して再度トライ */
	    if((audio_play_fd = open(AUDIO_DEV, O_WRONLY)) < 0)
	    {
		audriv_syserr(AUDIO_DEV);
		return False;
	    }
	}
	else
	{
	    audriv_syserr(AUDIO_DEV);
	    return False;
	}
    }

    /* キャラクタデバイスであるかどうかのチェック */
    if(stat(AUDIO_DEV, &buf) < 0)
    {
	audriv_syserr(AUDIO_DEV);
	close(audio_play_fd);
	close(audioctl_fd);
	audio_play_fd = -1;
	audioctl_fd   = -1;
	return False;
    }
    if(!S_ISCHR(buf.st_mode))
    {
	audriv_err(AUDRIV_ERROR_NOT_AUDIO_DEV);
	close(audio_play_fd);
	close(audioctl_fd);
	audio_play_fd = -1;
	audioctl_fd   = -1;
	return False;
    }

    /* 指定可能な符合化方式サンプルレートの簡易チェック */
    check_audio_support(audio_play_fd);

    /* estimated_audio_buffer_size の算出 */
    if(estimated_audio_buffer_size == 0)
	estimated_audio_buffer_size = fd_get_fillable(audio_play_fd);

    sun_stop_playing();
    audio_play_fd = -1;

    return True;
}

void audriv_free_audio(void)
{
    audriv_play_close();
    if(audioctl_fd != -1)
    {
	close(audioctl_fd);
	audioctl_fd = -1;
    }
}

static long sun_audio_precision(long encoding)
{
    if(encoding == AUDIO_ENCODING_ULAW || encoding == AUDIO_ENCODING_ALAW)
	return 8;
    return 16;			/* AUDRIV_LINEAR_TAG */
}

static Bool sun_audio_set_parameter(audio_info_t *auinfo)
{
    if(ioctl(audio_play_fd, AUDIO_SETINFO, auinfo) < 0)
    {
	if(errno == EINVAL)
	    audriv_err(AUDRIV_ERROR_INVALID_AUDIO_FORMAT);
	else
	    audriv_syserr("ioctl:AUDIO_SETINFO");
	return False;
    }
    return True;
}

static Bool set_noblock_internal(int fd, Bool noblock)
{
    int arg;
    if(noblock)
	arg = 1;
    else
	arg = 0;
    if(ioctl(fd, FIONBIO, &arg) < 0)
	return False;
    return True;
}

static int audriv_open_internal(int open_flag, Bool noblock)
{
    int fd;

    if((fd = open(AUDIO_DEV, open_flag | O_NDELAY)) < 0)
    {
	if(errno == EBUSY)
	    audriv_err(AUDRIV_ERROR_DEVICE_BUSY);
	else
	    audriv_syserr(AUDIO_DEV);
	return -1;
    }

    if(noblock)
    {
	if(!set_noblock_internal(fd, noblock))
	{
	    close(fd);
	    return -1;
	}
    }

    return fd;
}

Bool audriv_play_open(void)
{
    audio_info_t auinfo;

    if(audioctl_fd == -1)
    {
	fprintf(stderr, "%s がオープンされていないため，%s のオープンを拒否しました．", AUDIO_CTLDEV, AUDIO_DEV);
	exit(1);
    }

    if(audio_play_fd != -1)
	return True;

    if((audio_play_fd = audriv_open_internal(O_WRONLY,
					     audio_write_noblocking)) < 0)
	return False;

    AUDIO_INITINFO(&auinfo);
    auinfo.play.sample_rate = internal_play_sample_rate;
    auinfo.play.channels = internal_play_channels;
    auinfo.play.encoding = internal_play_encoding;
    auinfo.play.precision = sun_audio_precision(internal_play_encoding);

    return sun_audio_set_parameter(&auinfo);
}

void audriv_play_close(void)
{
    if(audio_play_fd != -1)
    {
	close(audio_play_fd);
	audio_play_fd = -1;
#ifdef SUNAUDIO_BUG_FIX
	play_counter = 0;
	reset_samples = 0;
#endif /* SUNAUDIO_BUG_FIX */
    }
}

#if !defined(I_FLUSH) || !defined(FLUSHW)
static void null_proc(){}
#endif

static int sun_stop_playing(void)
{
#if !defined(I_FLUSH) || !defined(FLUSHW)
    void (* orig_alarm_handler)();

    orig_alarm_handler = signal(SIGALRM, null_proc);
    ualarm(10000, 10000);
    close(audio_play_fd);
    ualarm(0, 0);
    signal(SIGALRM, orig_alarm_handler);
#else
    if(ioctl(audio_play_fd, I_FLUSH, FLUSHW) < 0)
    {
	audriv_syserr("ioctl");
	close(audio_play_fd);
	audio_play_fd = -1;
	return -1;
    }
    close(audio_play_fd);
#endif
    return 0;
}

long audriv_play_stop(void)
{
    long samples;
    int isactive;

    if(audio_play_fd == -1)
	return 0;

    if((samples = audriv_play_samples()) < 0)
	return -1;

    if((isactive = audriv_play_active()) == -1)
	return -1;

    if(isactive)
	sun_stop_playing();
    else
	close(audio_play_fd);
    audio_play_fd = -1;
#ifdef SUNAUDIO_BUG_FIX
    play_counter = 0;
    reset_samples = 0;
#endif /* SUNAUDIO_BUG_FIX */


    return samples;
}

Bool audriv_is_play_open(void)
{
    return audio_play_fd != -1;
}

Bool audriv_set_play_volume(int volume)
{
    audio_info_t auinfo;

    if(volume < 0)
	volume = 0;
    else if(volume > 255)
	volume = 255;

    AUDIO_INITINFO(&auinfo);
    auinfo.play.gain = volume;
    if(ioctl(audioctl_fd, AUDIO_SETINFO, &auinfo) < 0)
    {
	audriv_syserr("ioctl:AUDIO_SETINFO");
	return False;
    }

    return True;
}

static int audriv_audio_getinfo(audio_info_t *auinfo)
{
    AUDIO_INITINFO(auinfo);
    return ioctl(audioctl_fd, AUDIO_GETINFO, auinfo);
}

int audriv_get_play_volume(void)
{
    audio_info_t auinfo;

    if(audriv_audio_getinfo(&auinfo) < 0)
    {
	audriv_syserr("ioctl:AUDIO_GETINFO");
	return -1;
    }

    return auinfo.play.gain;
}

Bool audriv_set_play_output(int port)
{
    audio_info_t auinfo;

    AUDIO_INITINFO(&auinfo);
    switch(port)
    {
      case AUDRIV_OUTPUT_SPEAKER:
	auinfo.play.port = AUDIO_SPEAKER;
	break;
      case AUDRIV_OUTPUT_HEADPHONE:
	auinfo.play.port = AUDIO_HEADPHONE;
	break;
      case AUDRIV_OUTPUT_LINE_OUT:
	auinfo.play.port = AUDIO_LINE_OUT;
	break;
      default:
	audriv_err(AUDRIV_ERROR_INVALID_OUTPUT);
	return False;
    }

    if(ioctl(audioctl_fd, AUDIO_SETINFO, &auinfo) < 0)
    {
	audriv_syserr("ioctl:AUDIO_SETINFO");
	return False;
    }
    return True;
}

int audriv_get_play_output(void)
{
    audio_info_t auinfo;

    if(audriv_audio_getinfo(&auinfo) < 0)
    {
	audriv_syserr("ioctl:AUDIO_GETINFO");
	return -1;
    }

    switch(auinfo.play.port)
    {
      case AUDIO_SPEAKER:
	return AUDRIV_OUTPUT_SPEAKER;
      case AUDIO_HEADPHONE:
	return AUDRIV_OUTPUT_HEADPHONE;
      case AUDIO_LINE_OUT:
	return AUDRIV_OUTPUT_LINE_OUT;
    }
    return AUDRIV_OUTPUT_SPEAKER;
}

int audriv_write(char *buff, int nbytes)
{
    int n = nbytes;
#ifdef SUNAUDIO_BUG_FIX
    if(audriv_play_active() == 0)
    {
	reset_samples += play_counter / play_sample_size;
	play_counter = 0;
    }
    if(play_counter == 0)
	play_start_time = get_current_calender_time();
#endif

  retry_write:
    n = write(audio_play_fd, buff, n);
    if(n < 0)
    {
	if(audio_write_noblocking && errno == EWOULDBLOCK)
	    return 0;
	if(!audio_write_noblocking && errno == EAGAIN) {
	    n = nbytes;
	    audriv_wait_play();
	    goto retry_write;
	}

	audriv_syserr("write:" AUDIO_DEV);
	return -1;
    }
#ifdef SUNAUDIO_BUG_FIX
    play_counter += n;
#endif /* SUNAUDIO_BUG_FIX */

    return n;
}

Bool audriv_set_noblock_write(Bool noblock)
{
    if((noblock && audio_write_noblocking) ||
       (!noblock && !audio_write_noblocking))
	return True;

    if(audio_play_fd >= 0)
    {
	if(!set_noblock_internal(audio_play_fd, noblock))
	    return False;
    }
    audio_write_noblocking = noblock;
    return True;
}

#ifdef SUNAUDIO_BUG_FIX
int audriv_play_active(void)
{
    double es;

    if(play_counter <= 0)
	return 0;
    es = (double)internal_play_sample_rate
	* (double)play_sample_size
	    * (get_current_calender_time() - play_start_time);
    if(es > play_counter)
	return 0;
    return 1;
}
#else
int audriv_play_active(void)
{
    audio_info_t auinfo;

    if(audio_play_fd == -1)
	return False;

    if(audriv_audio_getinfo(&auinfo) < 0)
    {
	audriv_syserr("ioctl:AUDIO_GETINFO");
	return -1;
    }

    return !!auinfo.play.active;
}
#endif

long audriv_play_samples(void)
{
    audio_info_t auinfo;
    int status;
    int samples;

    status = audriv_audio_getinfo(&auinfo);

#ifdef DEBUGALL
    if(status < 0)
	perror("audriv:audriv_play_samples:" AUDIO_CTLDEV);
#endif
    samples = auinfo.play.samples;

    if(samples < 0)
	return 0;

#ifdef SUNAUDIO_BUG_FIX
    if(play_counter > 0 && samples == 0)
	return reset_samples + calculate_play_samples();
#endif

    return samples;
}

long audriv_get_filled(void)
/* オーディオバッファ内のバイト数を返します。
 * エラーの場合は -1 を返します。
 */
{
#ifdef SUNAUDIO_BUG_FIX
    double es;

    if(play_counter <= 0)
	return 0;
    es = (double)internal_play_sample_rate
	* (double)play_sample_size
	    * (get_current_calender_time() - play_start_time);
    if(es > play_counter)
	return 0;
    return play_counter - (long)es;
#else
    fprintf(stderr, "audriv_get_filled() is undefined.\n");
    exit(1);
#endif /* SUNAUDIO_BUG_FIX */
}

const long *audriv_available_encodings(int *n_ret)
{
    *n_ret = navail_encoding;
    return available_encodings;
}

const long *audriv_available_sample_rates(int *n_ret)
{
    *n_ret = navail_sample_rate;
    return available_sample_rates;
}

const long *audriv_available_channels(int *n_ret)
{
    *n_ret = navail_channels;
    return available_channels;
}

static long sun_audio_device_encoding_value(long encoding)
{
    switch(encoding)
    {
      case AENC_G711_ULAW:
	return AUDIO_ENCODING_ULAW; /* ISDN u-law */
      case AENC_G711_ALAW:
	return AUDIO_ENCODING_ALAW; /* ISDN A-law */
      case AUDRIV_AENC_SIGWORD:
	return AUDRIV_LINEAR_TAG; /* PCM 2's-complement (0-center) */
    }
    return AUDIO_ENCODING_ULAW;

/*
 * Solaris 2.x ?
 * AUDIO_ENCODING_FLOAT    (100) IEEE float (-1. <-> +1.)
 * AUDIO_ENCODING_G721     (101) CCITT g.721 ADPCM
 * AUDIO_ENCODING_G722     (102) CCITT g.722 ADPCM
 * AUDIO_ENCODING_G723     (103) CCITT g.723 ADPCM
 * AUDIO_ENCODING_DVI      (104) DVI ADPCM
 * AUDIO_ENCODING_LINEAR8  (105) 8 bit UNSIGNED
 *
 */
}

Bool audriv_set_play_encoding(long encoding)
{
    audio_info_t auinfo;
    int i, n, w;
    const long *enc;

    enc = audriv_available_encodings(&n);
    for(i = 0; i < n; i++)
	if(enc[i] == encoding)
	    break;
    if(i == n)
	return False;

    w = AENC_SAMPW(encoding);
    encoding = sun_audio_device_encoding_value(encoding);
    if(encoding == internal_play_encoding)
	return True;
    internal_play_encoding = encoding;
#ifdef SUNAUDIO_BUG_FIX
    play_sample_size = w * internal_play_channels;
#endif
    if(audio_play_fd >= 0)
    {
	AUDIO_INITINFO(&auinfo);
	auinfo.play.encoding = encoding;
	auinfo.play.precision = sun_audio_precision(encoding);
	return sun_audio_set_parameter(&auinfo);
    }

    return True;
}

Bool audriv_set_play_sample_rate(long sample_rate)
{
    audio_info_t auinfo;
    int i, n;
    const long *r;

    r = audriv_available_sample_rates(&n);
    for(i = 0; i < n; i++)
	if(r[i] == sample_rate)
	    break;
    if(i == n)
	return False;

    if(sample_rate == internal_play_sample_rate)
	return True;

#ifdef SUNAUDIO_BUG_FIX
/* 高いサンプルレートからいきなり 8000 にサンプルレートを下げると
 *	vmunix: zs3: silo overflow
 * というエラーがでることがある．16000 から 8000 へ変わる場合は，
 * このエラーがでないので，一旦 16000 に変更したのち，8000 に変更
 * するようにする．
 */
    if(sample_rate == 8000 && internal_play_sample_rate > 8000)
    {
	Bool open_flag;

#ifdef DEBUG
	printf("Fix sun audio sample-rate bug\n");
#endif /* DEBUG */
	open_flag = audriv_is_play_open();

	if(!open_flag)
	    if(!audriv_play_open())
		goto end_fix;

	AUDIO_INITINFO(&auinfo);
	auinfo.play.sample_rate = 16000;
	sun_audio_set_parameter(&auinfo);
	if(open_flag)
	    audriv_play_open();
    }
  end_fix:
#endif /* SUNAUDIO_BUG_FIX */

    internal_play_sample_rate = sample_rate;

    if(audio_play_fd >= 0)
    {
	AUDIO_INITINFO(&auinfo);
	auinfo.play.sample_rate = sample_rate;
	return sun_audio_set_parameter(&auinfo);
    }

    return True;
}

Bool audriv_set_play_channels(long channels)
{
    audio_info_t auinfo;
    int i, n;
    const long *c;

    c = audriv_available_channels(&n);
    for(i = 0; i < n; i++)
	if(c[i] == channels)
	    break;
    if(i == n)
	return False;

    if(channels == internal_play_channels)
	return True;
    internal_play_channels = channels;
#ifdef SUNAUDIO_BUG_FIX
    play_sample_size = channels;
    if(internal_play_encoding == AUDRIV_LINEAR_TAG)
	play_sample_size *= 2;
#endif
    if(audio_play_fd >= 0)
    {
	AUDIO_INITINFO(&auinfo);
	auinfo.play.channels = channels;
	return sun_audio_set_parameter(&auinfo);
    }

    return True;
}

#ifdef SUNAUDIO_BUG_FIX
static long play_remain_buffer_size()
{
    long ret;

    ret = play_counter - calculate_play_samples() * play_sample_size;
    if(ret < 0)
	return 0;
    return ret;
}
#endif /* SUNAUDIO_BUG_FIX */

void audriv_wait_play(void)
{
#ifdef SUNAUDIO_BUG_FIX
    long remain;
    extern int usleep(unsigned int useconds);

    remain = (play_remain_buffer_size() / play_sample_size) * 1000.0 /
	(double)internal_play_sample_rate;

    if(remain > 1500)
	usleep(remain * (1000 / 4));
#else
    usleep(100000);
#endif /* SUNAUDIO_BUG_FIX */
}
