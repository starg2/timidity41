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
#define WIN32_LEAN_AND_MEAN
// Defines from windows needed for this function only. Can't include full
//  Cygwin32 windows headers because of problems with CONTEXT redefinition
//  Removed logic to tell not dynamically load static modules. It is assumed that all
//   modules are dynamically built. This should be similar to the behavoir on sunOS.
//   Leaving in the logic would have required changes to the standard perlmain.c code
//
// // Includes call a dll function to initialize it's impure_ptr.
#include <stdio.h>
void (*impure_setupptr)(struct _reent *);  // pointer to the impure_setup routine

//#include <windows.h>
#define LOAD_WITH_ALTERED_SEARCH_PATH	(8)
typedef void *HANDLE;
typedef HANDLE HINSTANCE;
#define STDCALL     __attribute__ ((stdcall))
typedef int STDCALL (*FARPROC)();

HINSTANCE
STDCALL
LoadLibraryExA(
	       char *lpLibFileName,
	       HANDLE hFile,
	       unsigned int dwFlags
	       );
unsigned int
STDCALL
GetLastError(
	     void
	     );
FARPROC
STDCALL
GetProcAddress(
	       HINSTANCE hModule,
	       char *lpProcName
	       );

#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "timidity.h"
#include "dlutils.c"

/*ARGSUSED*/
void dl_init(int argc, char **argv)
{
}

void *dl_load_file(char *filename)
{
    int flags = 0;
    void *RETVAL;

    RETVAL = (void*) LoadLibraryExA(filename, NULL,
				    LOAD_WITH_ALTERED_SEARCH_PATH);
    if(RETVAL == NULL)
	fprintf(stderr, "%d", GetLastError());
    else
    {
	// setup the dll's impure_ptr:
	impure_setupptr = GetProcAddress(RETVAL, "impure_setup");
	if(impure_setupptr == NULL)
	{
	    fprintf(stderr,
    "Cygwin32 dynaloader error: could not load impure_setup symbol\n");
	    RETVAL = NULL;
	}
	else{
	    // setup the DLLs impure_ptr:
	    (*impure_setupptr)(_impure_ptr);
	}
    }

    return RETVAL;
}

void *dl_find_symbol(void *libhandle, char *symbolname)
{
    RETVAL = (void*) GetProcAddress((HINSTANCE) libhandle, symbolname);
    if(RETVAL == NULL)
	fprintf(stderr, "%d", GetLastError());
    return RETVAL;
}
