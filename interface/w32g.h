#ifndef ___W32G_H_
#define ___W32G_H_

#include <windows.h>
#include <windowsx.h>	/* There is no <windowsx.h> on CYGWIN.
			 * Edit_* and ListBox_* are defined in
			 * <windowsx.h>
			 */

#ifndef MAXPATH
#define MAXPATH 256
#endif /* MAXPATH */

extern CHAR *INI_INVALID;
extern CHAR *INI_SEC_PLAYER;
extern CHAR *INI_SEC_TIMIDITY;

#endif
