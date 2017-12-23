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

#ifndef ___URL_H_
#define ___URL_H_

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#elif defined(HAVE_UNISTD_H)
#include <unistd.h>
#else
#include <stdio.h>
#endif

#include "sysdep.h"

/* This header file from liburl-1.8.3.
 * You can get full source from:
 * http://www.goice.co.jp/member/mo/release/index.html#liburl
 */


#define URL_LIB_VERSION "1.9.5"

/* Define if you want to enable pipe command scheme ("command|") */
#define PIPE_SCHEME_ENABLE 1

/* Define if you want to appended on a user's home directory if a filename
 * is beginning with '~'
 */
#if !defined(__MACOS__) && !defined(__W32__)
#define TILD_SCHEME_ENABLE 1
#endif

/* Define if you want to use soft directory cache */
#ifndef URL_DIR_CACHE_DISABLE
#define URL_DIR_CACHE_ENABLE 1
#endif /* URL_DIR_CACHE_DISABLE */

/* Define if you want to use XOVER command in NNTP */
/* #define URL_NEWS_XOVER_SUPPORT "XOVER", "XOVERVIEW" */

/* M:Must, O:Optional defined */
typedef struct _URL
{
    int   type;									/* M */

    ptr_size_t (*url_read)(struct _URL *url, void *buff, ptr_size_t n);		/* M */
    char *(*url_gets)(struct _URL *url, char *buff, ptr_size_t n);		/* O */
    int   (*url_fgetc)(struct _URL *url);					/* O */
    off_size_t (*url_seek)(struct _URL *url, off_size_t offset, int whence);	/* O */
    off_size_t (*url_tell)(struct _URL *url);					/* O */
    void  (*url_close)(struct _URL *url);					/* M */

    off_size_t nread;	/* Reset in url_seek, url_rewind,
				   url_set_readlimit */
    off_size_t readlimit;
    off_size_t eof;		/* Used in url_nread and others */
} *URL;
#define URLm(url, m) (((URL)url)->m)

#define url_eof(url) URLm((url), eof)

/* open URL stream */
extern URL url_open(const char *url_string);

/* close URL stream */
extern void url_close(URL url);

/* read n bytes */
extern ptr_size_t url_read(URL url, void *buff, ptr_size_t n);
extern ptr_size_t url_safe_read(URL url, void *buff, ptr_size_t n);
extern ptr_size_t url_nread(URL url, void *buff, ptr_size_t n);

/* read a line */
/* Like a fgets */
extern char *url_gets(URL url, char *buff, ptr_size_t n);

/* Allow termination by CR or LF or both. Ignored empty lines.
 * CR or LF is truncated.
 * Success: length of the line.
 * EOF or Error: EOF
 */
extern ptr_size_t url_readline(URL url, char *buff, ptr_size_t n);

/* read a byte */
extern int url_fgetc(URL url);
#define url_getc(url) \
    ((url)->nread >= (url)->readlimit ? ((url)->eof = 1, EOF) : \
     (url)->url_fgetc ? ((url)->nread++, (url)->url_fgetc(url)) : \
      url_fgetc(url))

/* seek position */
extern off_size_t url_seek(URL url, off_size_t offset, int whence);
extern off_size_t url_seek_uint64(URL url, uint64 offset, int whence);

/* get the current position */
extern off_size_t url_tell(URL url);

/* skip n bytes */
extern void url_skip(URL url, ptr_size_t n);

/* seek to first position */
extern void url_rewind(URL url);

/* dump */
void *url_dump(URL url, ptr_size_t nbytes, ptr_size_t *real_read);

/* set read limit */
void url_set_readlimit(URL url, ptr_size_t readlimit);

/* url_errno to error message */
extern const char *url_strerror(int no);

/* allocate URL structure */
extern URL alloc_url(ptr_size_t size);

/* Check URL type. */
extern int url_check_type(const char *url_string);

/* replace `~' to user directory */
extern const char *url_expand_home_dir(const char *filename);
extern const char *url_unexpand_home_dir(const char *filename);

extern int url_errno;
enum url_errtypes
{
    URLERR_NONE = 10000,	/* < 10000 represent system call's errno */
    URLERR_NOURL,		/* Unknown URL */
    URLERR_OPERM,		/* Operation not permitted */
    URLERR_CANTOPEN,		/* Can't open a URL */
    URLERR_IURLF,		/* Invalid URL form */
    URLERR_URLTOOLONG,		/* URL too long */
    URLERR_NOMAILADDR,		/* No mail address */
    URLERR_MAXNO
};

struct URL_module
{
    /* url type */
    int type;

    /* URL checker */
    int (*name_check)(const char *url_string);

    /* Once call just before url_open(). */
    int (*url_init)(void);

    /* Open specified URL */
    URL (*url_open)(const char *url_string);

    /* chain next modules */
    struct URL_module *chain;
};

extern void url_add_module(struct URL_module *m);
extern void url_add_modules(struct URL_module *m, ...);

extern URL url_file_open(const char *filename);
extern URL url_dir_open(const char *directory_name);
extern URL url_http_open(const char *url_string);
extern URL url_ftp_open(const char *url_string);
extern URL url_newsgroup_open(const char *url_string);
extern URL url_news_open(const char *url_string);
extern URL url_pipe_open(const char *command);

/* No URL_module */
extern URL url_mem_open(char *memory, ptr_size_t memsiz, int autofree);
extern URL url_constmem_open(const char *memory, ptr_size_t memsiz);
extern URL url_inflate_open(URL instream, ptr_size_t compsize, int autoclose);
extern URL url_buff_open(URL url, int autoclose);
extern URL url_cache_open(URL url, int autoclose);
extern void url_cache_detach(URL url);
extern void url_cache_disable(URL url);
extern URL url_uudecode_open(URL reader, int autoclose);
extern URL url_b64decode_open(URL reader, int autoclose);
extern URL url_hqxdecode_open(URL reader, int dataonly, int autoclose);
extern URL url_qsdecode_open(URL reader, int autoclose);
extern URL url_cgi_escape_open(URL reader, int autoclose);
extern URL url_cgi_unescape_open(URL reader, int autoclose);

extern char *url_dir_name(URL url);
extern char *url_newsgroup_name(URL url);
extern int url_news_connection_cache(int flag);

extern char *url_lib_version;
extern char *user_mailaddr;
extern char *url_user_agent;
extern char *url_http_proxy_host;
extern unsigned short url_http_proxy_port;
extern char *url_ftp_proxy_host;
extern unsigned short url_ftp_proxy_port;
extern int url_newline_code;
extern int uudecode_unquote_html;

enum url_types
{
    URL_none_t,			/* Undefined URL */
    URL_file_t,			/* File system */
    URL_dir_t,			/* Directory entry */
    URL_http_t,			/* HTTP */
    URL_ftp_t,			/* FTP */
    URL_news_t,			/* NetNews article */
    URL_newsgroup_t,		/* NetNews group */
    URL_pipe_t,			/* Pipe */
    URL_mem_t,			/* On memory */
    URL_buff_t,			/* Buffered stream */
    URL_cache_t,		/* Cached stream */
    URL_uudecode_t,		/* UU decoder */
    URL_b64decode_t,		/* Base64 decoder */
    URL_qsdecode_t,		/* Quoted-string decoder */
    URL_hqxdecode_t,		/* HQX decoder */
    URL_cgi_escape_t,		/* WWW CGI Escape */
    URL_cgi_unescape_t,		/* WWW CGI Unescape */
    URL_arc_t,			/* arc stream */
    URL_constmem_t,		/* On memory (const pointer) */

    URL_inflate_t = 99,		/* LZ77 decode stream */

    URL_extension_t = 100	/* extentional stream >= 100 */
};

enum url_news_conn_type
{
    URL_NEWS_CONN_NO_CACHE,
    URL_NEWS_CONN_CACHE,
    URL_NEWS_CLOSE_CACHE,
    URL_NEWS_GET_FLAG
};

#define IS_URL_SEEK_SAFE(url) ((url)->url_seek && \
			       (url)->type != URL_buff_t)

#if -1L != (-1L >> 1)
#define URL_MAX_READLIMIT ((~(off_size_t)0L) >> 1)
#elif _FILE_OFFSET_BITS > 32
#define URL_MAX_READLIMIT ((off_size_t)0x7fffffffffffffffLL)
#else
#define URL_MAX_READLIMIT ((off_size_t)0x7fffffffL)
#endif
#endif /* ___URL_H_ */
