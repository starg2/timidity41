#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdlib.h>
#include "timidity.h"
#include "arc.h"

#define LOCSIG    0x04034b50
#define EXTLOCSIG 0x08074b50L

static unsigned short get_short(char *s)
{
    unsigned char *p = (unsigned char *)s;
    return ((unsigned short)p[0] |
	    (unsigned short)p[1]<<8);
}

static unsigned long get_long(char *s)
{
    unsigned char *p = (unsigned char *)s;
    return ((unsigned long)p[0] |
	    (unsigned long)p[1]<<8 |
	    (unsigned long)p[2]<<16 |
	    (unsigned long)p[3]<<24);
}

ArchiveEntryNode *next_zip_entry(ArchiveHandler archiver)
{
    unsigned long magic;
    unsigned short flen, elen, hdrsiz;
    URL url;
    long compsize, origsize;
    char buff[BUFSIZ];
    ArchiveEntryNode *entry;
    int method;
    unsigned short flags;
    int macbin_check;

    url = archiver->decode_stream;
    macbin_check = (archiver->nfiles == 0);

  retry_read:
    if(url_read(url, buff, 4) != 4)
	return NULL;

    hdrsiz = 4;

    magic = get_long(buff);
    if(magic == EXTLOCSIG)
    {
	/* ignored */
	if(url_read(url, buff, 20) != 20)
	    return NULL;
	magic = get_long(buff + 16);
	hdrsiz += 20;
    }
    else if(macbin_check && buff[0] == '0')
    {
	macbin_check = 0;
	url_skip(url, 128-4);
	if(archiver->type == AHANDLER_SEEK)
	    archiver->pos += 128;
	goto retry_read;
    }

    if(magic != LOCSIG)
	return NULL;

    /* Version needed to extract */
    url_skip(url, 2);
    hdrsiz += 2;

    /* General purpose bit flag */
    if(url_read(url, buff, 2) != 2)
	return NULL;
    flags = get_short(buff);
    hdrsiz += 2;

    /* Compression method */
    if(url_read(url, buff, 2) != 2)
	return NULL;
    method = get_short(buff);
    hdrsiz += 2;

    switch(method)
    {
      case 0: /* The file is stored (no compression) */
	method = ARCHIVEC_STORED;
	break;
      case 1: /* The file is Shrunk */
	method = ARCHIVEC_SHRUNKED;
	break;
      case 2: /* The file is Reduced with compression factor 1 */
	method = ARCHIVEC_REDUCED1;
	break;
      case 3: /* The file is Reduced with compression factor 2 */
	method = ARCHIVEC_REDUCED2;
	break;
      case 4: /* The file is Reduced with compression factor 3 */
	method = ARCHIVEC_REDUCED3;
	break;
      case 5: /* The file is Reduced with compression factor 4 */
	method = ARCHIVEC_REDUCED4;
	break;
      case 6: /* The file is Imploded */
	if(flags & 4)
	{
	    if(flags & 2)
		method = ARCHIVEC_IMPLODED_LIT8;
	    else
		method = ARCHIVEC_IMPLODED_LIT4;
	}
	else if(flags & 2)
	    method = ARCHIVEC_IMPLODED_NOLIT8;
	else
	    method = ARCHIVEC_IMPLODED_NOLIT4;
	break;
      case 7: /* Reserved for Tokenizing compression algorithm */
	method = -1;
	break;
      case 8: /* The file is Deflated */
	method = ARCHIVEC_DEFLATED;
	break;
      default:
	return NULL;
    }

    /* Last mod file time */
    url_skip(url, 2);
    hdrsiz += 2;

    /* Last mod file date */
    url_skip(url, 2);
    hdrsiz += 2;

    /* CRC-32 */
    url_skip(url, 4);
    hdrsiz += 4;

    /* Compressed size */
    if(url_read(url, buff, 4) != 4)
	return NULL;
    hdrsiz += 4;
    compsize = (long)get_long(buff);

    /* Uncompressed size */
    if(url_read(url, buff, 4) != 4)
	return NULL;
    hdrsiz += 4;
    origsize = (long)get_long(buff);

    /* Filename length */
    if(url_read(url, buff, 2) != 2)
	return NULL;
    hdrsiz += 2;
    flen = get_short(buff);
    if(flen >= sizeof(buff)-1)
	return NULL;

    /* Extra field length */
    if(url_read(url, buff, 2) != 2)
	return NULL;
    hdrsiz += 2;
    elen = get_short(buff);

    /* filename */
    if(url_read(url, buff, flen) != flen)
	return NULL;
    hdrsiz += flen;
    buff[flen] = '\0';

    if(compsize == 0 && flen > 0 &&
       (buff[flen - 1] == '/' || buff[flen - 1] == '\\'))
    {
	url_skip(url, elen);
	hdrsiz += elen;
	if(archiver->type == AHANDLER_SEEK)
	    archiver->pos += hdrsiz;
	goto retry_read;
    }

    entry = new_entry_node(&archiver->pool, buff, flen);
    if(entry == NULL)
	return NULL;

    entry->comptype = method;
    entry->origsize = origsize;
    entry->compsize = compsize;

    /* Extra field */
    url_skip(url, elen);
    hdrsiz += elen;

    if(archiver->type == AHANDLER_SEEK)
    {
	entry->strmtype = ARCSTRM_SEEK_URL;
	archiver->pos += hdrsiz;
	entry->u.seek_start = archiver->pos;
	url_skip(url, compsize);
	archiver->pos += compsize;
    }
    else
    {
	char buff[BUFSIZ];
	long rest, n;

	entry->strmtype = ARCSTRM_MEMBUFFER;
	rest = compsize;
	while(rest > 0)
	{
	    n = rest;
	    if(n > sizeof(buff))
		n = sizeof(buff);
	    n = url_read(url, buff, n);
	    if(n <= 0)
		break;
	    push_memb(&entry->u.compdata, buff, n);
	    rest -= n;
	}
    }

    return entry;
}
