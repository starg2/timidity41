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
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */
#include <stdarg.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "timidity.h"
#include "common.h"
#include "url.h"

/* #define DEBUG 1 */

int url_errno;
static struct URL_module *url_mod_list = NULL;
char *user_mailaddr = NULL;
char *url_user_agent = NULL;
int url_newline_code = '\n';
char *url_lib_version = URL_LIB_VERSION;
int uudecode_unquote_html = 0;

void url_add_module(struct URL_module *m)
{
    m->chain = url_mod_list;
    url_mod_list = m;
}

void url_add_modules(struct URL_module *m, ...)
{
    va_list ap;
    struct URL_module *mod;

    if (!m)
	return;
    url_add_module(m);
    va_start(ap, m);
    while ((mod = va_arg(ap, struct URL_module*)) != NULL)
	url_add_module(mod);
}

static int url_init_nop(void)
{
    /* Do nothing any more */
    return 1;
}

int url_check_type(const char *s)
{
    struct URL_module *m;

    for (m = url_mod_list; m; m = m->chain)
	if (m->type != URL_none_t && m->name_check && m->name_check(s))
	    return m->type;
    return -1;
}

URL url_open(const char *s)
{
    struct URL_module *m;

    for (m = url_mod_list; m; m = m->chain)
    {
#ifdef DEBUG
	fprintf(stderr, "Check URL type=%d\n", m->type);
#endif /* DEBUG */
	if (m->type != URL_none_t && m->name_check && m->name_check(s))
	{
#ifdef DEBUG
	    fprintf(stderr, "open url (type=%d, name=%s)\n", m->type, s);
#endif /* DEBUG */
	    if (m->url_init != url_init_nop)
	    {
		if (m->url_init && m->url_init() < 0)
		    return NULL;
		m->url_init = url_init_nop;
	    }

	    url_errno = URLERR_NONE;
	    _set_errno(0);
	    return m->url_open(s);
	}
    }

    url_errno = URLERR_NOURL;
    _set_errno(ENOENT);
    return NULL;
}

ptr_size_t url_read(URL url, void *buff, ptr_size_t n)
{
    if (n <= 0)
	return 0;
    url_errno = URLERR_NONE;
    _set_errno(0);
    if (url->nread >= url->readlimit) {
        url->eof = 1;
	return 0;
    }
    if (url->nread + n > url->readlimit)
	n = (ptr_size_t)(url->readlimit - url->nread);
    n = url->url_read(url, buff, n);
    if (n > 0)
	url->nread += n;
    return n;
}

ptr_size_t url_safe_read(URL url, void *buff, ptr_size_t n)
{
    ptr_size_t i;
    if (n <= 0)
	return 0;

    do /* Ignore signal intruption */
    {
	_set_errno(0);
	i = url_read(url, buff, n);
    } while (i == -1 && errno == EINTR);
#if 0
    /* Already done in url_read!! */
    if (i > 0)
	url->nread += i;
#endif
    return i;
}

ptr_size_t url_nread(URL url, void *buff, ptr_size_t n)
{
    ptr_size_t insize = 0;
    char *s = (char*)buff;

    do
    {
	ptr_size_t i;
	i = url_safe_read(url, s + insize, n - insize);
	if (i <= 0)
	{
	    if (insize == 0)
		return i;
	    break;
	}
	insize += i;
    } while (insize < n);

    return insize;
}

char *url_gets(URL url, char *buff, ptr_size_t n)
{
    if (url->nread >= url->readlimit)
	return NULL;

    if (!url->url_gets)
    {
	int maxlen, i, c;
	int newline = url_newline_code;

	maxlen = n - 1;
	if (maxlen == 0)
	    *buff = '\0';
	if (maxlen <= 0)
	    return buff;
	i = 0;

	do
	{
	    if ((c = url_getc(url)) == EOF)
		break;
	    buff[i++] = c;
	} while (c != newline && i < maxlen);

	if (i == 0)
	    return NULL; /* EOF */
	buff[i] = '\0';
	return buff;
    }

    url_errno = URLERR_NONE;
    _set_errno(0);

    if (url->nread + n > url->readlimit)
	n = (ptr_size_t)(url->readlimit - url->nread) + 1;

    buff = url->url_gets(url, buff, n);
    if (buff)
	url->nread += strlen(buff);
    return buff;
}

ptr_size_t url_readline(URL url, char *buff, ptr_size_t n)
{
    ptr_size_t maxlen, i, c;

    maxlen = n - 1;
    if (maxlen == 0)
	*buff = '\0';
    if (maxlen <= 0)
	return 0;
    do
    {
	i = 0;
	do
	{
	    if ((c = url_getc(url)) == EOF)
		break;
	    buff[i++] = c;
	} while (c != '\r' && c != '\n' && i < maxlen);
	if (i == 0)
	    return 0; /* EOF */
    } while (i == 1 && (c == '\r' || c == '\n'));

    if (c == '\r' || c == '\n')
	i--;
    buff[i] = '\0';
    return i;
}

int url_fgetc(URL url)
{
    if (url->nread >= url->readlimit)
	return EOF;

    url->nread++;
    if (!url->url_fgetc)
    {
	unsigned char c;
	if (url_read(url, &c, 1) <= 0)
	    return EOF;
	return (int)c;
    }
    url_errno = URLERR_NONE;
    _set_errno(0);
    return url->url_fgetc(url);
}

off_size_t url_seek(URL url, off_size_t offset, int whence)
{
    off_size_t pos, savelimit;

    if (!url->url_seek)
    {
		if (whence == SEEK_CUR && offset >= 0)
		{
			pos = url_tell(url);
			if (offset == 0)
			return pos;
			savelimit = (off_size_t)url->readlimit;
			url->readlimit = URL_MAX_READLIMIT;
			url_skip(url, offset);
			url->readlimit = savelimit;
			url->nread = 0;
			return pos;
		}

		if (whence == SEEK_SET)
		{
			pos = url_tell(url);
			if (pos != -1 && pos <= offset)
			{
			if (pos == offset)
				return pos;
			savelimit = (off_size_t)url->readlimit;
			url->readlimit = URL_MAX_READLIMIT;
			url_skip(url, offset - pos);
			url->readlimit = savelimit;
			url->nread = 0;
			return pos;
			}
		}

		_set_errno(url_errno = EPERM);
		return -1;
    }
    url_errno = URLERR_NONE;
    _set_errno(0);
    url->nread = 0;
    return url->url_seek(url, offset, whence);
}

///r
// must tf_seek_uint64()
off_size_t url_seek_uint64(URL url, uint64 offset, int whence)
{
    off_size_t pos, savelimit;

    if (!url->url_seek)
    {
		if (whence == SEEK_CUR)
		{
			pos = url_tell(url);
			if (offset == 0)
			return pos;
			savelimit = (off_size_t)url->readlimit;
			url->readlimit = URL_MAX_READLIMIT;
			if (url->url_seek)
				url_skip(url, offset);
			url->readlimit = savelimit;
			url->nread = 0;
			return pos;
		}

		if (whence == SEEK_SET)
		{
			pos = url_tell(url);
			if (pos != -1 && pos <= offset)
			{
			if (pos == offset)
				return pos;
			savelimit = (off_size_t)url->readlimit;
			url->readlimit = URL_MAX_READLIMIT;
			if (url->url_seek)
				url_skip(url, offset - pos);
			url->readlimit = savelimit;
			url->nread = 0;
			return pos;
			}
		}

		_set_errno(url_errno = EPERM);
		return -1;
    }
    url_errno = URLERR_NONE;
    _set_errno(0);
    url->nread = 0;
    return url->url_seek(url, offset, whence);
}

off_size_t url_tell(URL url)
{
    url_errno = URLERR_NONE;
    _set_errno(0);
    if (!url->url_tell)
	return (off_size_t)url->nread;
    return url->url_tell(url);
}

void url_skip(URL url, off_size_t n)
{
    char tmp[BUFSIZ];

    if (url->url_seek)
    {
	off_size_t savenread;

	savenread = (off_size_t)url->nread;
	if (savenread >= url->readlimit)
	    return;
	if (savenread + n > url->readlimit)
	    n = (off_size_t)(url->readlimit - savenread);
	if (url->url_seek(url, n, SEEK_CUR) != -1)
	{
	    url->nread = savenread + n;
	    return;
	}
	url->nread = savenread;
    }

    while (n > 0)
    {
		off_size_t c;

	c = n;
	if (c > sizeof(tmp))
	    c = sizeof(tmp);
	c = url_read(url, tmp, c);
	if (c <= 0)
	    break;
	n -= c;
    }
}

void url_rewind(URL url)
{
    if (url->url_seek)
	url->url_seek(url, 0, SEEK_SET);
    url->nread = 0;
}

void url_set_readlimit(URL url, ptr_size_t readlimit)
{
    if (readlimit < 0)
	url->readlimit = URL_MAX_READLIMIT;
    else
	url->readlimit = (off_size_t)readlimit;
    url->nread = 0;
}

URL alloc_url(ptr_size_t size)
{
    URL url;
#ifdef HAVE_SAFE_MALLOC
    url = (URL) safe_malloc(size);
    memset(url, 0, size);
#else
    url = (URL) malloc(size);
    if (url)
	memset(url, 0, size);
    else
	url_errno = errno;
#endif /* HAVE_SAFE_MALLOC */

    url->nread = 0;
    url->readlimit = URL_MAX_READLIMIT;
    url->eof = 0;
    return url;
}

void url_close(URL url)
{
    int save_errno = errno;

    if (!url)
    {
	fprintf(stderr, "URL stream structure is NULL?\n");
#ifdef ABORT_AT_FATAL
	abort();
#endif /* ABORT_AT_FATAL */
    }
    else if (!url->url_close)
    {
	fprintf(stderr, "URL Error: Already URL is closed (type=%d)\n",
		url->type);
#ifdef ABORT_AT_FATAL
	abort();
#endif /* ABORT_AT_FATAL */
    }
    else
    {
	url->url_close(url);
#if 0
	url->url_close = NULL;
#endif /* unix */
    }
    _set_errno(save_errno);
}

#ifdef TILD_SCHEME_ENABLE
#include <pwd.h>
const char *url_expand_home_dir(const char *fname)
{
    static char path[BUFSIZ];
    char *dir;
    int dirlen;

    if (fname[0] != '~')
	return fname;

    if (IS_PATH_SEP(fname[1])) /* ~/... */
    {
	fname++;
	if ((dir = getenv("HOME")) == NULL)
	    if ((dir = getenv("home")) == NULL)
		return fname;
    }
    else /* ~user/... */
    {
	struct passwd *pw;
	int i;

	fname++;
	for (i = 0; i < sizeof(path) - 1 && fname[i] && !IS_PATH_SEP(fname[i]); i++)
	    path[i] = fname[i];
	path[i] = '\0';
	if ((pw = getpwnam(path)) == NULL)
	    return fname - 1;
	fname += i;
	dir = pw->pw_dir;
    }
    dirlen = strlen(dir);
    strncpy(path, dir, sizeof(path) - 1);
    if (sizeof(path) > dirlen)
	strncat(path, fname, sizeof(path) - dirlen - 1);
    path[sizeof(path) - 1] = '\0';
    return path;
}
const char *url_unexpand_home_dir(const char *fname)
{
    static char path[BUFSIZ];
    char *dir, *p;
    int dirlen;

    if (!IS_PATH_SEP(fname[0]))
	return fname;

    if ((dir = getenv("HOME")) == NULL)
	if ((dir = getenv("home")) == NULL)
	    return fname;
    dirlen = strlen(dir);
    if (dirlen == 0 || dirlen >= sizeof(path) - 2)
	return fname;
    memcpy(path, dir, dirlen);
    if (!IS_PATH_SEP(path[dirlen - 1]))
	path[dirlen++] = PATH_SEP;

#ifndef __W32__
    if (strncmp(path, fname, dirlen))
#else
    if (strncasecmp(path, fname, dirlen))
#endif /* __W32__ */
	return fname;

    path[0] = '~';
    path[1] = '/';
    p = fname + dirlen;
    if (strlen(p) >= sizeof(path) - 3)
	return fname;
    path[2] = '\0';
    strcat(path, p);
    return path;
}
#else
const char *url_expand_home_dir(const char *fname)
{
    return fname;
}
const char *url_unexpand_home_dir(const char *fname)
{
    return fname;
}
#endif

static const char *url_strerror_txt[] =
{
    "",				/* URLERR_NONE */
    "Unknown URL",		/* URLERR_NOURL */
    "Operation not permitted",	/* URLERR_OPERM */
    "Can't open a URL",		/* URLERR_CANTOPEN */
    "Invalid URL form",		/* URLERR_IURLF */
    "URL too long",		/* URLERR_URLTOOLONG */
    "No mail address",		/* URLERR_NOMAILADDR */
    ""
};

const char *url_strerror(int no)
{
    if (no <= URLERR_NONE)
	return strerror(no);
    if (no >= URLERR_MAXNO)
	return "Internal error";
    return url_strerror_txt[no - URLERR_NONE];
}

void *url_dump(URL url, ptr_size_t nbytes, ptr_size_t *read_size)
{
    ptr_size_t allocated, offset, read_len;
    char *buff;

    if (read_size)
      *read_size = 0;
    if (nbytes == 0)
	return NULL;
    if (nbytes >= 0)
    {
	buff = (void*) safe_malloc(nbytes);
	if (nbytes == 0)
	    return buff;
	read_len = url_nread(url, buff, nbytes);
	if (read_size)
	  *read_size = read_len;
	if (read_len <= 0)
	{
	    safe_free(buff);
	    return NULL;
	}
	return buff;
    }

    allocated = 1024;
    buff = (char*) safe_malloc(allocated);
    offset = 0;
    read_len = allocated;
    while ((nbytes = url_read(url, buff + offset, read_len)) > 0)
    {
	offset += nbytes;
	read_len -= nbytes;
	if (offset == allocated)
	{
	    read_len = allocated;
	    allocated *= 2;
	    buff = (char*) safe_realloc(buff, allocated);
	}
    }
    if (offset == 0)
    {
	safe_free(buff);
	return NULL;
    }
    if (read_size)
      *read_size = offset;
    return buff;
}
