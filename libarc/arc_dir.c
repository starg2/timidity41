#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "timidity.h"
#include "arc.h"

ArchiveEntryNode *next_dir_entry(ArchiveHandler archiver)
{
    char filename[BUFSIZ];
    URL url;
    char *dirname;
    int flen, dlen;
    ArchiveEntryNode *entry;

    url = archiver->decode_stream;

    if((dirname = url_dir_name(url)) == NULL)
	return NULL;

    do
    {
	if(url_gets(url, filename, sizeof(filename)) == NULL)
	    return NULL;
    } while(filename[0] == '.'); /* skip special file */

    dlen = strlen(dirname);
    flen = strlen(filename);

    entry = new_entry_node(&archiver->pool, filename, flen);
    if(entry == NULL)
	return NULL;
    entry->comptype = ARCHIVEC_STORED;
    entry->strmtype = ARCSTRM_PATHNAME;
    entry->u.pathname = (char *)new_segment(&archiver->pool, dlen + flen + 2);
    if(entry->u.pathname == NULL)
	return NULL;
    memcpy(entry->u.pathname, dirname, dlen);
    if(dirname[dlen - 1] != PATH_SEP)
	entry->u.pathname[dlen++] = PATH_SEP;
    memcpy(entry->u.pathname + dlen, filename, flen + 1);

    return entry;
}
