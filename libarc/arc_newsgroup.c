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

ArchiveEntryNode *next_newsgroup_entry(ArchiveHandler archiver)
{
    char name[BUFSIZ];
    URL url;
    char *urlname, *p;
    int len, i;
    ArchiveEntryNode *entry;

    url = archiver->decode_stream;

    if((urlname = url_newsgroup_name(url)) == NULL)
	return NULL;

    p = urlname;
    if(strncmp(p, "news://", 7) == 0)
	p += 7;
    if((p = strchr(p, '/')) == NULL)
	return NULL;
    p++;
    len = p - urlname;
    p = name + len;
    if(url_gets(url, p, sizeof(name) - len) == NULL)
	return NULL;

    i = 0;
    while('0' <= p[i] && p[i] <= '9')
	i++;
    if(p[i] != ' ')
	return NULL;
    i++;
    memcpy(name + i, urlname, len);

    entry = new_entry_node(&archiver->pool, name + i, strlen(name + i));
    if(entry == NULL)
	return NULL;
    entry->comptype = ARCHIVEC_STORED;
    entry->strmtype = ARCSTRM_NEWSGROUP;

    return entry;
}
