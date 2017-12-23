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
#include "timidity.h"
#include "common.h"
#include "url.h"
#include "mblock.h"
#include "zip.h"

typedef struct _URL_inflate
{
    char common[sizeof(struct _URL)];
    InflateHandler decoder;
    URL instream;
    off_size_t compsize;
    off_size_t pos;
    int autoclose;
} URL_inflate;

static ptr_size_t url_inflate_read_func(char *buf, ptr_size_t size, void *v);
static ptr_size_t url_inflate_read(URL url, void *buff, ptr_size_t n);
static off_size_t url_inflate_tell(URL url);
static void url_inflate_close(URL url);

URL url_inflate_open(URL instream, ptr_size_t compsize, int autoclose)
{
    URL_inflate *url;

    url = (URL_inflate*)alloc_url(sizeof(URL_inflate));
    if (!url)
    {
	if (autoclose)
	    url_close(instream);
	url_errno = errno;
	return NULL;
    }

    /* common members */
    URLm(url, type)      = URL_inflate_t;
    URLm(url, url_read)  = url_inflate_read;
    URLm(url, url_gets)  = NULL;
    URLm(url, url_fgetc) = NULL;
    URLm(url, url_seek)  = NULL;
    URLm(url, url_tell)  = url_inflate_tell;
    URLm(url, url_close) = url_inflate_close;

    /* private members */
    url->decoder = NULL;
    url->instream = instream;
    url->pos = 0;
    url->compsize = compsize;
    url->autoclose = autoclose;

    _set_errno(0);
    url->decoder = open_inflate_handler(url_inflate_read_func, url);
    if (!url->decoder)
    {
	if (autoclose)
	    url_close(instream);
	url_inflate_close((URL)url);
	url_errno = errno;
	return NULL;
    }

    return (URL)url;
}

static ptr_size_t url_inflate_read_func(char *buf, ptr_size_t size, void *v)
{
    URL_inflate *urlp = (URL_inflate*)v;
    off_size_t n;

    if (urlp->compsize == -1) /* size if unknown */
	return url_read(urlp->instream, buf, size);

    if (urlp->compsize == 0)
	return 0;
    n = size;
    if (n > urlp->compsize)
	n = urlp->compsize;
    n = url_read(urlp->instream, buf, n);
    if (n == -1)
	return -1;
    urlp->compsize -= n;
    return n;
}

static ptr_size_t url_inflate_read(URL url, void *buff, ptr_size_t n)
{
    URL_inflate *urlp = (URL_inflate*)url;

    n = zip_inflate(urlp->decoder, (char*)buff, n);
    if (n <= 0)
	return n;
    urlp->pos += n;
    return n;
}

static off_size_t url_inflate_tell(URL url)
{
    return ((URL_inflate*)url)->pos;
}

static void url_inflate_close(URL url)
{
    int save_errno = errno;
    URL_inflate *urlp = (URL_inflate*)url;
    if (urlp->decoder)
	close_inflate_handler(urlp->decoder);
    if (urlp->autoclose)
	url_close(urlp->instream);
    safe_free(url);
    _set_errno(save_errno);
}
