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
#ifdef __POCC__
#include <sys/types.h>
#endif /* for off_t */
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include "timidity.h"
#include "common.h"
#include "url.h"

typedef struct _URL_constmem
{
    char common[sizeof(struct _URL)];
    const char *memory;
    off_size_t  memsiz;
    off_size_t  mempos;
} URL_constmem;

static ptr_size_t url_constmem_read(URL url, void *buff, ptr_size_t n);
static char *url_constmem_gets(URL url, char *buff, ptr_size_t n);
static int url_constmem_fgetc(URL url);
static off_size_t url_constmem_seek(URL url, off_size_t offset, int whence);
static off_size_t url_constmem_tell(URL url);
static void url_constmem_close(URL url);

URL url_constmem_open(const char *memory, ptr_size_t memsiz)
{
    URL_constmem *url;

    url = (URL_constmem*)alloc_url(sizeof(URL_constmem));
    if (!url)
    {
	return NULL;
    }

    /* common members */
    URLm(url, type)      = URL_constmem_t;
    URLm(url, url_read)  = url_constmem_read;
    URLm(url, url_gets)  = url_constmem_gets;
    URLm(url, url_fgetc) = url_constmem_fgetc;
    URLm(url, url_seek)  = url_constmem_seek;
    URLm(url, url_tell)  = url_constmem_tell;
    URLm(url, url_close) = url_constmem_close;

    /* private members */
    url->memory = memory;
    url->memsiz = memsiz;
    url->mempos = 0;

    return (URL)url;
}

static ptr_size_t url_constmem_read(URL url, void *buff, ptr_size_t n)
{
    URL_constmem *urlp = (URL_constmem*)url;
    off_size_t s;
    char *p = (char*)buff;

    s = urlp->memsiz - urlp->mempos;
    if (s > n)
	s = n;
    if (s <= 0)
	return 0;
    memcpy(p, urlp->memory + urlp->mempos, s);
    urlp->mempos += s;
    return s;
}

static char *url_constmem_gets(URL url, char *buff, ptr_size_t n)
{
    URL_constmem *urlp = (URL_constmem*)url;
    off_size_t s;
    char *nlp;
    const char *p;

    if (urlp->memsiz == urlp->mempos)
	return NULL;
    if (n <= 0)
	return buff;
    if (n == 1)
    {
	*buff = '\0';
	return buff;
    }
    n--; /* for '\0' */
    s = urlp->memsiz - urlp->mempos;
    if (s > n)
	s = n;
    p = urlp->memory + urlp->mempos;
    nlp = (char*)memchr(p, url_newline_code, s);
    if (nlp)
	s = nlp - p + 1;
    memcpy(buff, p, s);
    buff[s] = '\0';
    urlp->mempos += s;
    return buff;
}

static int url_constmem_fgetc(URL url)
{
    URL_constmem *urlp = (URL_constmem*)url;

    if (urlp->memsiz == urlp->mempos)
	return EOF;
    return (int)(unsigned char)urlp->memory[urlp->mempos++];
}

static off_size_t url_constmem_seek(URL url, off_size_t offset, int whence)
{
    URL_constmem *urlp = (URL_constmem*)url;
    off_size_t ret;

    ret = urlp->mempos;
    switch (whence)
    {
      case SEEK_SET:
	urlp->mempos = offset;
	break;
      case SEEK_CUR:
	urlp->mempos += offset;
	break;
      case SEEK_END:
	urlp->mempos = urlp->memsiz + offset;
	break;
    }
    if (urlp->mempos > urlp->memsiz)
	urlp->mempos = urlp->memsiz;
    else if (urlp->mempos < 0)
	urlp->mempos = 0;

    return ret;
}

static off_size_t url_constmem_tell(URL url)
{
    return ((URL_constmem*)url)->mempos;
}

static void url_constmem_close(URL url)
{
    int save_errno = errno;
    URL_constmem *urlp = (URL_constmem*)url;
    safe_free(url);
    _set_errno(save_errno);
}
