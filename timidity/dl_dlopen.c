#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
/* dl_dlopen.c */

#include <stdio.h>
#include "timidity.h"

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>	/* the dynamic linker include file for Sunos/Solaris */
#else
#include <nlist.h>
#include <link.h>
#endif

#ifndef RTLD_LAZY
# define RTLD_LAZY 1	/* Solaris 1 */
#endif

#ifdef __NetBSD__
# define dlerror() strerror(errno)
#endif


#include "dlutils.h"

/*ARGSUSED*/
void dl_init(int argc, char **argv)
{
    /* Do nothing */
}

void *dl_load_file(char *filename)
{
    int mode = RTLD_LAZY;
    void *RETVAL;

    RETVAL = dlopen(filename, mode) ;
    if (RETVAL == NULL)
	fprintf(stderr, "%s\n", dlerror());
    return RETVAL;
}

void *dl_find_symbol(void *libhandle, char *symbolname)
{
    void *RETVAL;

#ifdef DLSYM_NEEDS_UNDERSCORE
    char buff[BUFSIZ];
    sprintf(buff, "_%s", symbolname);
    symbolname = buff;
#endif

    RETVAL = dlsym(libhandle, symbolname);
    if (RETVAL == NULL)
	fprintf(stderr, "%s\n", dlerror());
    return RETVAL;
}
