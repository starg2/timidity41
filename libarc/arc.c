#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdlib.h>

#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include "timidity.h"
#include "arc.h"
#include "mblock.h"
#include "zip.h"
#include "unlzh.h"
#include "explode.h"
#include "strtab.h"

char *arc_lib_version = ARC_LIB_VERSION;
ArchiveFileList *last_archive_file_list = NULL;

#define GZIP_ASCIIFLAG		(1u<<0)
#define GZIP_MULTIPARTFLAG	(1u<<1)
#define GZIP_EXTRAFLAG		(1u<<2)
#define GZIP_FILEFLAG		(1u<<3)
#define GZIP_COMMFLAG		(1u<<4)
#define GZIP_ENCFLAG		(1u<<5)

#ifndef TRUE
#define TRUE			1
#endif /* TRUE */
#ifndef FALSE
#define FALSE			0
#endif /* FALSE */
#define ABORT			-1

static void fix_path_sep(char *s);
static long url_arc_read(URL url, void *buff, long n);
static long url_arc_tell(URL url);
static void url_arc_close(URL url);
static char *archive_base_name(MBlockList *pool, char *name);
static int DoMatch(char *text, char *p);
static int DoCaseMatch(char *text, char *p);

struct archive_ext_type_t archive_ext_list[] =
{
    {".tar",	ARCHIVE_TAR},
    {".tar.gz",	ARCHIVE_TGZ},
    {".tgz",	ARCHIVE_TGZ},
    {".zip",	ARCHIVE_ZIP},
    {".lzh",	ARCHIVE_LZH},
    {".lha",	ARCHIVE_LZH},
    {".mime",	ARCHIVE_MIME},
    {PATH_STRING, ARCHIVE_DIR},
    {NULL, -1}
};

int skip_gzip_header(URL url)
{
    unsigned char flags;
    int m1, method;

    /* magic */
    m1 = url_getc(url);
    if(m1 == 0)
    {
	url_skip(url, 128 - 1);
	m1 = url_getc(url);
    }
    if(m1 != 0x1f)
	return -1;
    if(url_getc(url) != 0x8b)
	return -1;

    /* method */
    method = url_getc(url);
    switch(method)
    {
      case 8: /* deflated */
	method = ARCHIVEC_DEFLATED;
	break;
      default:
	return -1;
    }
    /* flags */
    flags = url_getc(url);
    if(flags & GZIP_ENCFLAG)
	return -1;
    /* time */
    url_getc(url); url_getc(url); url_getc(url); url_getc(url);

    url_getc(url); /* extra flags */
    url_getc(url); /* OS type */

    if(flags & GZIP_MULTIPARTFLAG)
    {
	/* part number */
	url_getc(url); url_getc(url);
    }

    if(flags & GZIP_EXTRAFLAG)
    {
	unsigned short len;
	int i;

	/* extra field */

	len = url_getc(url);
	len |= ((unsigned short)url_getc(url)) << 8;
	for(i = 0; i < len; i++)
	    url_getc(url);
    }

    if(flags & GZIP_FILEFLAG)
    {
	/* file name */
	int c;

	do
	{
	    c = url_getc(url);
	    if(c == EOF)
		return -1;
	} while(c != '\0');
    }

    if(flags & GZIP_COMMFLAG)
    {
	/* comment */
	int c;

	do
	{
	    c = url_getc(url);
	    if(c == EOF)
		return -1;
	} while(c != '\0');
    }
    return method;
}

ArchiveEntryNode *new_entry_node(MBlockList *pool, char *filename, int len)
{
    ArchiveEntryNode *p;

    p = (ArchiveEntryNode *)
	new_segment(pool, sizeof(ArchiveEntryNode) + len + 1);
    memset(p, 0, sizeof(ArchiveEntryNode));
    if(filename != NULL)
    {
	memcpy(p->filename, filename, len);
	p->filename[len] = '\0';
    }
    p->compsize = p->origsize = -1;
    return p;
}

ArchiveHandler open_archive_handler(URL url, int archive_type)
{
    ArchiveHandler archiver;
    ArchiveEntryNode *entry;
    ArchiveEntryNode *(* next_header_entry)(ArchiveHandler archiver);
    URL gzip_stream = NULL;
    int gzip_method;

    switch(archive_type)
    {
      case ARCHIVE_TAR:
	next_header_entry = next_tar_entry;
	break;
      case ARCHIVE_TGZ:
	gzip_stream = url;
	gzip_method = skip_gzip_header(gzip_stream);
	if(gzip_method != ARCHIVEC_DEFLATED)
	{
	    url_close(url);
	    return NULL;
	}
	if((url = url_inflate_open(gzip_stream, -1, 1)) == NULL)
	    return NULL;
	next_header_entry = next_tar_entry;
	break;
      case ARCHIVE_ZIP:
	next_header_entry = next_zip_entry;
	break;
      case ARCHIVE_LZH:
	next_header_entry = next_lzh_entry;
	break;
      case ARCHIVE_DIR:
	if(url->type != URL_dir_t)
	{
	    url_close(url);
	    return NULL;
	}
	next_header_entry = next_dir_entry;
	break;
      case ARCHIVE_MIME:
	if(!IS_URL_SEEK_SAFE(url))
	{
	    if((url = url_cache_open(url, 1)) == NULL)
		return NULL;
	}
	next_header_entry = next_mime_entry;
	break;
#ifdef SUPPORT_SOCKET
      case ARCHIVE_NEWSGROUP:
	if(url->type != URL_newsgroup_t)
	{
	    url_close(url);
	    return NULL;
	}
	next_header_entry = next_newsgroup_entry;
	break;
#endif /* SUPPORT_SOCKET */
      default:
	return NULL;
    }

    archiver = (ArchiveHandler)malloc(sizeof(struct _ArchiveHandler));
    if(archiver == NULL)
    {
	url_close(url);
	return NULL;
    }
    memset(archiver, 0, sizeof(struct _ArchiveHandler));
    archiver->decode_stream = url;

    if(archive_type == ARCHIVE_DIR)
	archiver->type = AHANDLER_DIR;
#ifdef SUPPORT_SOCKET
    else if(archive_type == ARCHIVE_NEWSGROUP)
	archiver->type = AHANDLER_NEWSGROUP;
#endif /* SUPPORT_SOCKET */
    else if(!IS_URL_SEEK_SAFE(url))
	archiver->type = AHANDLER_CACHED;
    else
    {
	archiver->type = AHANDLER_SEEK;
	url_rewind(url);
    }

    if((entry = next_header_entry(archiver)) == NULL)
    {
	url_close(archiver->decode_stream);
	archiver->decode_stream = NULL;
	close_archive_handler(archiver);
	return NULL;
    }
    fix_path_sep(entry->filename);
    archiver->entry_head = archiver->entry_tail = entry;
    archiver->nfiles = 1;

    if(archive_type == ARCHIVE_MIME)
    {
	while(archiver->entry_tail->next != NULL)
	{
	    archiver->entry_tail = archiver->entry_tail->next;
	    fix_path_sep(archiver->entry_tail->filename);
	    archiver->nfiles++;
	}
    }
    else
    {
	while((entry = next_header_entry(archiver)) != NULL)
	{
	    fix_path_sep(entry->filename);
	    archiver->entry_tail = archiver->entry_tail->next = entry;
	    archiver->nfiles++;
	}
    }

    if(archiver->type == AHANDLER_SEEK)
    {
	archiver->seek_stream = archiver->decode_stream;
	if(archive_type == ARCHIVE_MIME)
	    url_cache_detach(archiver->seek_stream);
    }
    else
	url_close(archiver->decode_stream);
    archiver->decode_stream = NULL;
    archiver->pos = -1;

    return archiver;
}

ArchiveHandler open_archive_handler_name(char *name)
{
    URL url;
    int type;

    if((type = get_archive_type(name)) == -1)
	return NULL;

    /* open archiver stream */
    if(type != ARCHIVE_MIME)
    {
	if((url = url_open(name)) == NULL)
	    return NULL;
    }
    else
    {
	int len, method;
	len = strlen(name);

	if(strncmp(name, "mail:", 5) == 0 || strncmp(name, "mime:", 5) == 0)
	    url = url_open(name + 5);
	else
	    url = url_open(name);
	if(url == NULL)
	    return NULL;

	if(len >= 3 && strcmp(name + len - 3, ".gz") == 0)
	{
	    method = skip_gzip_header(url);
	    if(method != ARCHIVEC_DEFLATED)
	    {
		url_close(url);
		return NULL;
	    }
	    if((url = url_inflate_open(url, -1, 1)) == NULL)
		return NULL;
	}
    }

    return open_archive_handler(url, type);
}

#ifndef __MACOS__
static void fix_path_sep(char *s)
{
    int from, to;

    if(PATH_SEP == '/')
    {
	from = '\\';
	to   = '/';
    }
    else
    {
	from = '/';
	to   = '\\';
    }

    while(*s)
    {
	if(*s == from)
	    *s = to;
	s++;
    }
}
#else
static void fix_path_sep(char *s)
{
    while(*s)
    {
	if(*s == '/' || *s == '\\')
	    *s = ':';
	s++;
    }
}
#endif /* __MACOS__ */

long archiver_read_func(char *buff, long buff_size, void *v)
{
    ArchiveHandler archiver;
    long n;

    archiver = (ArchiveHandler)v;
    n = archiver->pos;
    if(n <= 0 || archiver->decode_stream == NULL)
	return 0;
    if(n > buff_size)
	n = buff_size;
    n = url_read(archiver->decode_stream, buff, n);
    if(n <= 0)
	return n;
    archiver->pos -= n;

    return n;
}

typedef struct _URL_arc
{
    char common[sizeof(struct _URL)];
    ArchiveHandler archiver;
    long pos;
} URL_arc;

URL archive_extract_open(ArchiveHandler archiver, int idx)
{
    int i;
    ArchiveEntryNode *p;
    URL_arc *url;
    URL fileurl;

    archiver->pos = -1;
    if(idx == -1)
    {
	errno = ENOENT;
	return NULL;
    }
    for(i = 0, p = archiver->entry_head; i < idx && p; i++, p = p->next)
	;
    if(p == NULL)
    {
	errno = ENOENT;
	return NULL;
    }

    archiver->pos = p->compsize;
    if(archiver->pos == 0)
    {
	archiver->pos = -1;
	return NULL;
    }

    if(p->comptype == -1)
	return NULL;

    archiver->entry_cur = p;
    archiver->decoder = NULL;

    switch(p->strmtype)
    {
      case ARCSTRM_MEMBUFFER:
	if((archiver->decode_stream = memb_open_stream(&p->u.compdata, 0))
	   == NULL)
	    return NULL;
	break;

      case ARCSTRM_SEEK_URL:
	archiver->decode_stream = archiver->seek_stream;
	url_seek(archiver->decode_stream, p->u.seek_start, SEEK_SET);
	break;

      case ARCSTRM_PATHNAME:
	if((fileurl = url_file_open(p->u.pathname)) == NULL)
	    return NULL;
	if(p->compsize != -1)
	    url_set_readlimit(fileurl, p->compsize);
	return fileurl;

      case ARCSTRM_URL:
	archiver->decode_stream = p->u.aurl.url;
	url_seek(archiver->decode_stream, p->u.aurl.seek_start, SEEK_SET);
	break;
    }

    url = (URL_arc *)alloc_url(sizeof(URL_arc));
    if(url == NULL)
    {
	int save_errno = errno;

	if(p->strmtype == ARCSTRM_MEMBUFFER && archiver->decode_stream != NULL)
	    url_close(archiver->decode_stream);
	archiver->decode_stream = NULL;
	archiver->pos = -1;
	errno = save_errno;

	return NULL;
    }

    /* common members */
    URLm(url, type)      = URL_arc_stream_t;
    URLm(url, url_read)  = url_arc_read;
    URLm(url, url_gets)  = NULL;
    URLm(url, url_fgetc) = NULL;
    URLm(url, url_seek)  = NULL;
    URLm(url, url_tell)  = url_arc_tell;
    URLm(url, url_close) = url_arc_close;

    /* private members */
    url->archiver = archiver;
    url->pos = 0;

    /* open decoder */
    switch(p->comptype)
    {
      case ARCHIVEC_STORED:	/* No compression */
      case ARCHIVEC_LZHED_LH0:	/* -lh0- */
      case ARCHIVEC_LZHED_LZ4:	/* -lz4- */
	p->comptype = ARCHIVEC_STORED;
	archiver->decoder = NULL;

      case ARCHIVEC_DEFLATED:	/* deflate */
	archiver->decoder =
	    (void *)open_inflate_handler(archiver_read_func, archiver);
	if(archiver->decoder == NULL)
	{
	    url_arc_close((URL)url);
	    return NULL;
	}
	break;

      case ARCHIVEC_IMPLODED_LIT8:
      case ARCHIVEC_IMPLODED_LIT4:
      case ARCHIVEC_IMPLODED_NOLIT8:
      case ARCHIVEC_IMPLODED_NOLIT4:
	archiver->decoder =
	    (void *)open_explode_handler(archiver_read_func,
					 p->comptype - ARCHIVEC_IMPLODED - 1,
					 p->compsize, p->origsize, archiver);
	if(archiver->decoder == NULL)
	{
	    url_arc_close((URL)url);
	    return NULL;
	}
	break;

      case ARCHIVEC_LZHED_LH1:	/* -lh1- */
      case ARCHIVEC_LZHED_LH2:	/* -lh2- */
      case ARCHIVEC_LZHED_LH3:	/* -lh3- */
      case ARCHIVEC_LZHED_LH4:	/* -lh4- */
      case ARCHIVEC_LZHED_LH5:	/* -lh5- */
      case ARCHIVEC_LZHED_LZS:	/* -lzs- */
      case ARCHIVEC_LZHED_LZ5:	/* -lz5- */
      case ARCHIVEC_LZHED_LHD:	/* -lhd- */
      case ARCHIVEC_LZHED_LH6:	/* -lh6- */
      case ARCHIVEC_LZHED_LH7:	/* -lh7- */
	archiver->decoder =
	    (void *)open_unlzh_handler(
		archiver_read_func,
		lzh_methods[p->comptype - ARCHIVEC_LZHED - 1],
		p->compsize, p->origsize, archiver);
	if(archiver->decoder == NULL)
	{
	    url_arc_close((URL)url);
	    return NULL;
	}
	break;

      case ARCHIVEC_UU:		/* uu encoded */
	if((archiver->decoder =
	    (void *)url_uudecode_open(archiver->decode_stream, 0)) == NULL)
	{
	    url_arc_close((URL)url);
	    return NULL;
	}
	break;

      case ARCHIVEC_B64:	/* base64 encoded */
	if((archiver->decoder =
	    (void *)url_b64decode_open(archiver->decode_stream, 0)) == NULL)
	{
	    url_arc_close((URL)url);
	    return NULL;
	}
	break;

      case ARCHIVEC_QS:		/* quoted string encoded */
	if((archiver->decoder =
	    (void *)url_qsdecode_open(archiver->decode_stream, 0)) == NULL)
	{
	    url_arc_close((URL)url);
	    return NULL;
	}
	break;

      case ARCHIVEC_HQX:	/* HQX encoded */
	if((archiver->decoder =
	    (void *)url_hqxdecode_open(archiver->decode_stream, 1, 0)) == NULL)
	{
	    url_arc_close((URL)url);
	    return NULL;
	}
	break;

      default:
	url_arc_close((URL)url);
	return NULL;
    }

    return (URL)url;
}

URL archive_extract_open_name(ArchiveHandler archiver, char *entry_name)
{
    int i;
    ArchiveEntryNode *entry;

    if(entry_name == NULL || *entry_name == '\0')
	return archive_extract_open(archiver, 0);

    i = 0;
    for(entry = archiver->entry_head; entry; entry = entry->next)
    {
	if(arc_case_wildmat(entry->filename, entry_name))
	    return archive_extract_open(archiver, i);
	i++;
    }
    errno = ENOENT;
    return NULL;
}

URL archive_file_extract_open(ArchiveFileList *list, char *name)
{
    int idx;
    ArchiveHandler archiver;
    ArchiveFileList *lp;

    if((lp = find_archiver(list, name, &idx)) == NULL)
	return NULL;
    if((archiver = lp->archiver) == NULL)
	return NULL;
    if(idx == -1)
	return NULL;
    return archive_extract_open(archiver, idx);
}

static long url_arc_read(URL url, void *vp, long bufsiz)
{
    URL_arc *urlp = (URL_arc *)url;
    ArchiveHandler archiver = urlp->archiver;
    long n = 0;
    int comptype;
    void *decoder;
    char *buff = (char *)vp;

    if(archiver->pos == -1)
	return 0;

    comptype = archiver->entry_cur->comptype;
    decoder = archiver->decoder;
    switch(comptype)
    {
      case ARCHIVEC_STORED:
	n = archiver_read_func(buff, bufsiz, (void *)archiver);
	break;

      case ARCHIVEC_DEFLATED:
	n = zip_inflate((InflateHandler)decoder, buff, bufsiz);
	break;

      case ARCHIVEC_IMPLODED_LIT8:
      case ARCHIVEC_IMPLODED_LIT4:
      case ARCHIVEC_IMPLODED_NOLIT8:
      case ARCHIVEC_IMPLODED_NOLIT4:
	n = explode((ExplodeHandler)decoder, buff, bufsiz);
	break;

      case ARCHIVEC_LZHED_LH1:	/* -lh1- */
      case ARCHIVEC_LZHED_LH2:	/* -lh2- */
      case ARCHIVEC_LZHED_LH3:	/* -lh3- */
      case ARCHIVEC_LZHED_LH4:	/* -lh4- */
      case ARCHIVEC_LZHED_LH5:	/* -lh5- */
      case ARCHIVEC_LZHED_LZS:	/* -lzs- */
      case ARCHIVEC_LZHED_LZ5:	/* -lz5- */
      case ARCHIVEC_LZHED_LHD:	/* -lhd- */
      case ARCHIVEC_LZHED_LH6:	/* -lh6- */
      case ARCHIVEC_LZHED_LH7:	/* -lh7- */
	n = unlzh((UNLZHHandler)decoder, buff, bufsiz);
	break;

      case ARCHIVEC_UU:		/* uu encoded */
      case ARCHIVEC_B64:	/* base64 encoded */
      case ARCHIVEC_QS:		/* quoted string encoded */
      case ARCHIVEC_HQX:	/* HQX encoded */
	n = url_read((URL)decoder, buff, bufsiz);
	break;
    }

    if(n > 0)
	urlp->pos += n;
    return n;
}

static long url_arc_tell(URL url)
{
    return ((URL_arc *)url)->pos;
}

static void url_arc_close(URL url)
{
    ArchiveHandler archiver = ((URL_arc *)url)->archiver;
    void *decoder;
    ArchiveEntryNode *p;
    int save_errno = errno;

    /* 1. close decoder 
     * 2. close decode_stream
     * 3. free url
     */

    p = archiver->entry_cur;
    decoder = archiver->decoder;
    if(decoder != NULL)
    {
	switch(p->comptype)
	{
	  case ARCHIVEC_DEFLATED:
	    close_inflate_handler((InflateHandler)decoder);
	    break;

	  case ARCHIVEC_IMPLODED_LIT8:
	  case ARCHIVEC_IMPLODED_LIT4:
	  case ARCHIVEC_IMPLODED_NOLIT8:
	  case ARCHIVEC_IMPLODED_NOLIT4:
	    close_explode_handler((ExplodeHandler)decoder);
	    break;

	  case ARCHIVEC_LZHED_LH1:	/* -lh1- */
	  case ARCHIVEC_LZHED_LH2:	/* -lh2- */
	  case ARCHIVEC_LZHED_LH3:	/* -lh3- */
	  case ARCHIVEC_LZHED_LH4:	/* -lh4- */
	  case ARCHIVEC_LZHED_LH5:	/* -lh5- */
	  case ARCHIVEC_LZHED_LZS:	/* -lzs- */
	  case ARCHIVEC_LZHED_LZ5:	/* -lz5- */
	  case ARCHIVEC_LZHED_LHD:	/* -lhd- */
	  case ARCHIVEC_LZHED_LH6:	/* -lh6- */
	  case ARCHIVEC_LZHED_LH7:	/* -lh7- */
	    close_unlzh_handler((UNLZHHandler)decoder);
	    break;

	  case ARCHIVEC_UU:	/* uu encoded */
	  case ARCHIVEC_B64:	/* base64 encoded */
	  case ARCHIVEC_QS:	/* quoted string encoded */
	  case ARCHIVEC_HQX:	/* HQX encoded */
	    url_close((URL)decoder);
	    break;
	}
	archiver->decoder = NULL;
    }

    if(p->strmtype == ARCSTRM_MEMBUFFER && archiver->decode_stream != NULL)
	url_close(archiver->decode_stream);
    archiver->decode_stream = NULL;

    archiver->pos = -1;
    free(url);
    errno = save_errno;
}

void close_archive_handler(ArchiveHandler archiver)
{
    ArchiveEntryNode *entry;
    if(archiver == NULL)
	return;
    for(entry = archiver->entry_head; entry; entry = entry->next)
    {
	if(entry->strmtype == ARCSTRM_MEMBUFFER)
	    delete_memb(&entry->u.compdata);
	else if(entry->strmtype == ARCSTRM_URL && entry->u.aurl.idx == 0 &&
		entry->u.aurl.url != NULL) {
	    url_close(entry->u.aurl.url);
	    entry->u.aurl.url = NULL;
	}
    }

    if(archiver->decode_stream == archiver->seek_stream)
    {
	if(archiver->decode_stream != NULL)
	    url_close(archiver->decode_stream);
    }
    else
    {
	if(archiver->decode_stream != NULL)
	    url_close(archiver->decode_stream);
	if(archiver->seek_stream != NULL)
	    url_close(archiver->seek_stream);
    }
    reuse_mblock(&archiver->pool);
    free(archiver);
}

void close_archive_files(ArchiveFileList *list)
{
    while(list)
    {
	ArchiveFileList *p;

	p = list;
	list = list->next;
	free(p->archive_name);
	close_archive_handler(p->archiver);
	free(p);
    }
}

int get_archive_type(char *archive_name)
{
    int i, len;
    char *p;
    int archive_name_length, delim;

#ifdef SUPPORT_SOCKET
    int type = url_check_type(archive_name);
    if(type == URL_news_t)
	return ARCHIVE_MIME;
    if(type == URL_newsgroup_t)
	return ARCHIVE_NEWSGROUP;
#endif /* SUPPORT_SOCKET */

    if(strncmp(archive_name, "mail:", 5) == 0 ||
       strncmp(archive_name, "mime:", 5) == 0)
	return ARCHIVE_MIME;

    if((p = strchr(archive_name, '#')) != NULL)
    {
	archive_name_length = p - archive_name;
	delim = '#';
    }
    else
    {
	archive_name_length = strlen(archive_name);
	delim = '\0';
    }

    for(i = 0; archive_ext_list[i].ext; i++)
    {
	len = strlen(archive_ext_list[i].ext);
	if(len <= archive_name_length &&
	   strncasecmp(archive_name + archive_name_length - len,
		       archive_ext_list[i].ext, len) == 0 &&
	   archive_name[archive_name_length] == delim)
	    return archive_ext_list[i].type; /* Found */
    }

    if(url_check_type(archive_name) == URL_dir_t)
	return ARCHIVE_DIR;

    return -1; /* Not found */
}

static char *archive_base_name(MBlockList *pool, char *name)
{
    char *p;
    int len;

    if(*name == '\0')
	return NULL;
    if((p = strchr(name, '#')) == NULL)
	return strdup_mblock(pool, name);

    len = p - name;
    if((p = (char *)new_segment(pool, len + 1)) == NULL)
	return NULL;
    memcpy(p, name, len);
    p[len] = '\0';
    return p;
}

static ArchiveFileList *new_archive_list(char *filename)
{
    ArchiveFileList *node;
    char *basename;
    MBlockList pool;

    init_mblock(&pool);

    if((basename = archive_base_name(&pool, filename)) == NULL)
    {
	reuse_mblock(&pool);
	return NULL;
    }

    if(get_archive_type(basename) == -1)
    {
	reuse_mblock(&pool);
	return NULL;
    }

    node = (ArchiveFileList *)malloc(sizeof(ArchiveFileList));
    if(node == NULL)
    {
	reuse_mblock(&pool);
	return NULL;
    }

    node->archive_name = strdup(basename);
    if(node->archive_name == NULL)
    {
	free(node);
	reuse_mblock(&pool);
	return NULL;
    }

    /* open archive handler */
    if((node->archiver = open_archive_handler_name(basename)) == NULL)
    {
	if(errno == 0)
	    errno = ENOENT;
	node->errstatus = errno;
	reuse_mblock(&pool);
	return node;
    }

    node->errstatus = 0;
    reuse_mblock(&pool);
    return node;
}

ArchiveFileList *add_archive_list(ArchiveFileList *list, char *name)
{
    ArchiveFileList *node;
    ArchiveHandler archiver;
    ArchiveEntryNode *entry;

    name = url_expand_home_dir(name);
    if(find_archiver(list, name, NULL) != NULL)
	return list;
    if((node = new_archive_list(name)) == NULL)
	return list;

    node->next = list;
    archiver = node->archiver;
    list = node;

    if(archiver == NULL)
	return list;

    if(archiver->type == AHANDLER_DIR)
    {
	for(entry = archiver->entry_head; entry; entry = entry->next)
	{
	    if(find_archiver(list, entry->u.pathname, NULL) != NULL)
		continue;
	    if((node = new_archive_list(entry->u.pathname)) != NULL)
	    {
		node->next = list;
		list = node;
	    }
	}
    }
#ifdef SUPPORT_SOCKET
    else if(archiver->type == AHANDLER_NEWSGROUP)
    {
	for(entry = archiver->entry_head; entry; entry = entry->next)
	{
	    if(find_archiver(list, entry->filename, NULL) != NULL)
		continue;
	    if((node = new_archive_list(entry->filename)) != NULL)
	    {
		node->next = list;
		list = node;
	    }
	}
    }
#endif /* SUPPORT_SOCKET */
    return list;
}

ArchiveFileList *make_archive_list(ArchiveFileList *list,
				   int nfiles, char **files)
{
    int i;
    for(i = 0; i < nfiles; i++)
	list = add_archive_list(list, files[i]);
    return list;
}

ArchiveFileList* find_archiver(ArchiveFileList *list, char *name, int *idx)
{
    char *base_name;
    MBlockList pool;
    ArchiveFileList *lp;
    ArchiveHandler archiver = NULL;

    last_archive_file_list = NULL;
    init_mblock(&pool);
    name = url_expand_home_dir(name);
    if((base_name = archive_base_name(&pool, name)) == NULL)
    {
	reuse_mblock(&pool);
	return NULL;
    }

    for(lp = list; lp; lp = lp->next)
    {
	if(strcmp(lp->archive_name, base_name) == 0)
	{
	    last_archive_file_list = lp;
	    archiver = lp->archiver;
	    break;
	}
    }

    if(archiver == NULL || lp == NULL)
    {
	reuse_mblock(&pool);
	return lp;
    }

    if(idx != NULL)
    {
	name += strlen(base_name);
	if(*name == '#')
	    name++;
	if(*name == '\0')
	    *idx = 0;
	else
	{
	    ArchiveEntryNode *entry;
	    int i;

	    i = 0;
	    *idx = -1;
	    for(entry = archiver->entry_head; entry; entry = entry->next)
	    {
		if(arc_case_wildmat(entry->filename, name))
		{
		    *idx = i;
		    break;
		}
		i++;
	    }
	}
    }

    reuse_mblock(&pool);
    return lp;
}

static int put_arc_rep(StringTable *stab,
		       char *basename, int blen, char *filename, int flen)
{
    MBlockList pool;
    char *qp, *s;
    int qplen;
    StringTableNode *st;

    init_mblock(&pool);
    qp = (char *)new_segment(&pool, flen * 2 + 1);
    qplen = 0;
    while(*filename)
    {
	if(*filename == '\\' ||
	   *filename == '?' ||
	   *filename == '*' ||
	   *filename == '[')
	    qp[qplen++] = '\\';
	qp[qplen++] = *filename++;
    }
    qp[qplen++] = '\0';
    if((st = put_string_table(stab, NULL, blen + qplen + 1)) == NULL)
    {
	reuse_mblock(&pool);
	return 1;
    }
    s = st->string;
    memcpy(s, basename, blen);
    s[blen] = '#';
    memcpy(s + blen + 1, qp, qplen + 1);
    reuse_mblock(&pool);
    return 0;
}

static int add_archive_files(StringTable *stab,
			     ArchiveFileList *list,
			     char *pathname, char *pattern)
{
    int plen;
    ArchiveEntryNode *entry;
    ArchiveHandler archiver;
    ArchiveFileList* lp;

    plen = strlen(pathname);
    if((lp = find_archiver(list, pathname, NULL)) == NULL)
    {
	if(put_string_table(stab, pathname, plen) == NULL)
	    return 1;
	return 0;
    }

    if((archiver = lp->archiver) == NULL)
	return 0;

    for(entry = archiver->entry_head; entry; entry = entry->next)
    {
	if(*pattern == '\0' || arc_case_wildmat(entry->filename, pattern))
	{
	    if(put_arc_rep(stab, pathname, plen,
			   entry->filename, strlen(entry->filename)))
		return 1;
	}
    }
    return 0;
}

char **expand_archive_names(ArchiveFileList *list, int *nfiles, char **files)
{
    StringTable stab;
    int in_nfiles;
    char *infile_name, *name;
    ArchiveHandler archiver;
    ArchiveEntryNode *entry;
    int base_name_len, i;

    if(list == NULL)
	return files;

    init_string_table(&stab);
    in_nfiles = *nfiles;
    for(i = 0; i < in_nfiles; i++)
    {
	ArchiveFileList *lp;

	infile_name = url_expand_home_dir(files[i]);
	lp = find_archiver(list, infile_name, NULL);
	if(lp == NULL || lp->archiver == NULL)
	{
	    if(put_string_table(&stab, infile_name, strlen(infile_name))
	       == NULL)
	    {
		/* memory error */
		delete_string_table(&stab);
		return NULL;
	    }
	    continue;
	}

	archiver = lp->archiver;
	name = strchr(infile_name, '#');
	if(name == NULL)
	{
	    base_name_len = strlen(infile_name);
	    name = infile_name + base_name_len;
	}
	else
	{
	    base_name_len = name - infile_name;
	    name++;
	}

#ifdef SUPPORT_SOCKET
	if(archiver->type == AHANDLER_NEWSGROUP)
	{
	    for(entry = archiver->entry_head; entry; entry = entry->next)
	    {
		ArchiveHandler subarc;
		ArchiveEntryNode *subentry;
		ArchiveFileList *lp;

		if((lp = find_archiver(list, entry->filename, NULL)) == NULL)
		    continue;
		if((subarc = lp->archiver) == NULL)
		    continue;
		for(subentry = subarc->entry_head; subentry;
		    subentry = subentry->next)
		{
		    if(*name == '\0' ||
		       arc_case_wildmat(subentry->filename, name))
		    {

			if(put_arc_rep(&stab,
				       entry->filename,
				       strlen(entry->filename),
				       subentry->filename,
				       strlen(subentry->filename)))
			{
			    delete_string_table(&stab);
			    return NULL;
			}
		    }
		}
	    }
	    continue;
	}
#endif /* SUPPORT_SOCKET */

	for(entry = archiver->entry_head; entry; entry = entry->next)
	{
	    if(*name == '\0' || arc_case_wildmat(entry->filename, name))
	    {
		if(entry->strmtype == ARCSTRM_PATHNAME)
		{
		    if(add_archive_files(&stab, list,
					 entry->u.pathname, name))
		    {
			delete_string_table(&stab);
			return NULL;
		    }
		}
		else
		{
		    if(put_arc_rep(&stab,
				   infile_name, base_name_len,
				   entry->filename, strlen(entry->filename)))
		    {
			delete_string_table(&stab);
			return NULL;
		    }
		}
	    }
	}
    }
    *nfiles = stab.nstring;
    return make_string_array(&stab);
}

/************** wildmat ***************/
/* What character marks an inverted character class? */
#define NEGATE_CLASS		'!'

/* Is "*" a common pattern? */
#define OPTIMIZE_JUST_STAR

/* Do tar(1) matching rules, which ignore a trailing slash? */
#undef MATCH_TAR_PATTERN

/* Define if case is ignored */
#define MATCH_CASE_IGNORE

#include <ctype.h>
#define TEXT_CASE_CHAR(c) (toupper(c))
#define CHAR_CASE_COMP(a, b) (TEXT_CASE_CHAR(a) == TEXT_CASE_CHAR(b))

static char *ParseHex(char *p, int *val)
{
    int i, v;

    *val = 0;
    for(i = 0; i < 2; i++)
    {
	v = *p++;
	if('0' <= v && v <= '9')
	    v = v - '0';
	else if('A' <= v && v <= 'F')
	    v = v - 'A' + 10;
	else if('a' <= v && v <= 'f')
	    v = v - 'a' + 10;
	else
	    return NULL;
	*val = (*val << 4 | v);
    }
    return p;
}

/*
**  Match text and p, return TRUE, FALSE, or ABORT.
*/
static int DoMatch(char *text, char *p)
{
    register int	last;
    register int	matched;
    register int	reverse;

    for ( ; *p; text++, p++) {
	if (*text == '\0' && *p != '*')
	    return ABORT;
	switch (*p) {
	case '\\':
	    p++;
	    if(*p == 'x')
	    {
		int c;
		if((p = ParseHex(++p, &c)) == NULL)
		    return ABORT;
		if(*text != c)
		    return FALSE;
		continue;
	    }
	    /* Literal match with following character. */

	    /* FALLTHROUGH */
	default:
	    if (*text != *p)
		return FALSE;
	    continue;
	case '?':
	    /* Match anything. */
	    continue;
	case '*':
	    while (*++p == '*')
		/* Consecutive stars act just like one. */
		continue;
	    if (*p == '\0')
		/* Trailing star matches everything. */
		return TRUE;
	    while (*text)
		if ((matched = DoMatch(text++, p)) != FALSE)
		    return matched;
	    return ABORT;
	case '[':
	    reverse = p[1] == NEGATE_CLASS ? TRUE : FALSE;
	    if (reverse)
		/* Inverted character class. */
		p++;
	    matched = FALSE;
	    if (p[1] == ']' || p[1] == '-')
		if (*++p == *text)
		    matched = TRUE;
	    for (last = *p; *++p && *p != ']'; last = *p)
		/* This next line requires a good C compiler. */
		if (*p == '-' && p[1] != ']'
		    ? *text <= *++p && *text >= last : *text == *p)
		    matched = TRUE;
	    if (matched == reverse)
		return FALSE;
	    continue;
	}
    }

#ifdef	MATCH_TAR_PATTERN
    if (*text == '/')
	return TRUE;
#endif	/* MATCH_TAR_ATTERN */
    return *text == '\0';
}

static int DoCaseMatch(char *text, char *p)
{
    register int	last;
    register int	matched;
    register int	reverse;

    for(; *p; text++, p++)
    {
	if(*text == '\0' && *p != '*')
	    return ABORT;
	switch (*p)
	{
	  case '\\':
	    p++;
	    if(*p == 'x')
	    {
		int c;
		if((p = ParseHex(++p, &c)) == NULL)
		    return ABORT;
		if(!CHAR_CASE_COMP(*text, c))
		    return FALSE;
		continue;
	    }
	    /* Literal match with following character. */

	    /* FALLTHROUGH */
	  default:
	    if(!CHAR_CASE_COMP(*text, *p))
		return FALSE;
	    continue;
	  case '?':
	    /* Match anything. */
	    continue;
	  case '*':
	    while(*++p == '*')
		/* Consecutive stars act just like one. */
		continue;
	    if(*p == '\0')
		/* Trailing star matches everything. */
		return TRUE;
	    while(*text)
		if((matched = DoCaseMatch(text++, p)) != FALSE)
		    return matched;
	    return ABORT;
	case '[':
	    reverse = p[1] == NEGATE_CLASS ? TRUE : FALSE;
	    if(reverse)
		/* Inverted character class. */
		p++;
	    matched = FALSE;
	    if(p[1] == ']' || p[1] == '-')
	    {
		if(*++p == *text)
		    matched = TRUE;
	    }
	    for(last = TEXT_CASE_CHAR(*p); *++p && *p != ']';
		last = TEXT_CASE_CHAR(*p))
	    {
		/* This next line requires a good C compiler. */
		if(*p == '-' && p[1] != ']')
		{
		    p++;
		    if(TEXT_CASE_CHAR(*text) <= TEXT_CASE_CHAR(*p) &&
		       TEXT_CASE_CHAR(*text) >= last)
			matched = TRUE;
		}
		else
		{
		    if(CHAR_CASE_COMP(*text, *p))
			matched = TRUE;
		}
	    }
	    if(matched == reverse)
		return FALSE;
	    continue;
	}
    }

#ifdef	MATCH_TAR_PATTERN
    if (*text == '/')
	return TRUE;
#endif	/* MATCH_TAR_ATTERN */
    return *text == '\0';
}

/*
**  User-level routine.  Returns TRUE or FALSE.
*/
int arc_wildmat(char *text, char *p)
{
#ifdef	OPTIMIZE_JUST_STAR
    if (p[0] == '*' && p[1] == '\0')
	return TRUE;
#endif	/* OPTIMIZE_JUST_STAR */
    return DoMatch(text, p) == TRUE;
}

int arc_case_wildmat(char *text, char *p)
{
#ifdef	OPTIMIZE_JUST_STAR
    if (p[0] == '*' && p[1] == '\0')
	return TRUE;
#endif	/* OPTIMIZE_JUST_STAR */
    return DoCaseMatch(text, p) == TRUE;
}
