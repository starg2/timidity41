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

    support.c - Define missing function
                Written by Masanao Izumo <mo@goice.co.jp>
*/


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/types.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#ifdef __WIN32__
#include <windows.h>
#endif /* __WIN32__ */

#include "timidity.h"
#include "mblock.h"

#ifndef HAVE_VSNPRINTF
/* From glib-1.1.13:gstrfuncs.c
 * Modified by Masanao Izumo <mo@goice.co.jp>
 */
static int printf_string_upper_bound (const char* format,
			     va_list      args)
{
  int len = 1;

  while (*format)
    {
      int long_int = 0;
      int extra_long = 0;
      char c;
      
      c = *format++;
      
      if (c == '%')
	{
	  int done = 0;
	  
	  while (*format && !done)
	    {
	      switch (*format++)
		{
		  char *string_arg;
		  
		case '*':
		  len += va_arg (args, int);
		  break;
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		  /* add specified format length, since it might exceed the
		   * size we assume it to have.
		   */
		  format -= 1;
		  len += strtol (format, (char**) &format, 10);
		  break;
		case 'h':
		  /* ignore short int flag, since all args have at least the
		   * same size as an int
		   */
		  break;
		case 'l':
		  if (long_int)
		    extra_long = 1; /* linux specific */
		  else
		    long_int = 1;
		  break;
		case 'q':
		case 'L':
		  long_int = 1;
		  extra_long = 1;
		  break;
		case 's':
		  string_arg = va_arg (args, char *);
		  if (string_arg)
		    len += strlen (string_arg);
		  else
		    {
		      /* add enough padding to hold "(null)" identifier */
		      len += 16;
		    }
		  done = 1;
		  break;
		case 'd':
		case 'i':
		case 'o':
		case 'u':
		case 'x':
		case 'X':
#ifdef	G_HAVE_GINT64
		  if (extra_long)
		    (void) va_arg (args, gint64);
		  else
#endif	/* G_HAVE_GINT64 */
		    {
		      if (long_int)
			(void) va_arg (args, long);
		      else
			(void) va_arg (args, int);
		    }
		  len += extra_long ? 64 : 32;
		  done = 1;
		  break;
		case 'D':
		case 'O':
		case 'U':
		  (void) va_arg (args, long);
		  len += 32;
		  done = 1;
		  break;
		case 'e':
		case 'E':
		case 'f':
		case 'g':
#ifdef HAVE_LONG_DOUBLE
		  if (extra_long)
		    (void) va_arg (args, long double);
		  else
#endif	/* HAVE_LONG_DOUBLE */
		    (void) va_arg (args, double);
		  len += extra_long ? 64 : 32;
		  done = 1;
		  break;
		case 'c':
		  (void) va_arg (args, int);
		  len += 1;
		  done = 1;
		  break;
		case 'p':
		case 'n':
		  (void) va_arg (args, void*);
		  len += 32;
		  done = 1;
		  break;
		case '%':
		  len += 1;
		  done = 1;
		  break;
		default:
		  /* ignore unknow/invalid flags */
		  break;
		}
	    }
	}
      else
	len += 1;
    }

  return len;
}

void vsnprintf(char *buff, size_t bufsiz, const char *fmt, va_list ap)
{
    MBlockList pool;
    char *tmpbuf = buff;

    init_mblock(&pool);
    tmpbuf = new_segment(&pool, printf_string_upper_bound(fmt, ap));
    vsprintf(tmpbuf, fmt, ap);
    strncpy(buff, tmpbuf, bufsiz);
    reuse_mblock(&pool);
}
#endif /* HAVE_VSNPRINTF */

#ifndef HAVE_SNPRINTF
void snprintf(char *buff, size_t bufsiz, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buff, bufsiz, fmt, ap);
    va_end(ap);
}
#endif /* HAVE_VSNPRINTF */


#ifndef HAVE_GETOPT
/*
	Copyright (c) 1986,1992 by Borland International Inc.
	All Rights Reserved.
*/
#ifdef HAVE_DOS_H
#include <dos.h>
#endif /* HAVE_DOS_H */

int	optind	= 1;	/* index of which argument is next	*/
char   *optarg;		/* pointer to argument of current option */
int	opterr	= 1;	/* allow error message	*/

static	char   *letP = NULL;	/* remember next option char's location */

#if 0
static	char	SW = 0;		/* DOS switch character, either '-' or '/' */
#endif
#define SW '-' /* On Win32 can't call DOS! */
/*
  Parse the command line options, System V style.

  Standard option syntax is:

    option ::= SW [optLetter]* [argLetter space* argument]

  where
    - SW is either '/' or '-', according to the current setting
      of the MSDOS switchar (int 21h function 37h).
    - there is no space before any optLetter or argLetter.
    - opt/arg letters are alphabetic, not punctuation characters.
	 - optLetters, if present, must be matched in optionS.
    - argLetters, if present, are found in optionS followed by ':'.
    - argument is any white-space delimited string.  Note that it
      can include the SW character.
    - upper and lower case letters are distinct.

  There may be multiple option clusters on a command line, each
  beginning with a SW, but all must appear before any non-option
  arguments (arguments not introduced by SW).  Opt/arg letters may
  be repeated: it is up to the caller to decide if that is an error.

  The character SW appearing alone as the last argument is an error.
  The lead-in sequence SWSW ("--" or "//") causes itself and all the
  rest of the line to be ignored (allowing non-options which begin
  with the switch char).

  The string *optionS allows valid opt/arg letters to be recognized.
  argLetters are followed with ':'.  Getopt () returns the value of
  the option character found, or EOF if no more options are in the
  command line.	 If option is an argLetter then the global optarg is
  set to point to the argument string (having skipped any white-space).

  The global optind is initially 1 and is always left as the index
  of the next argument of argv[] which getopt has not taken.  Note
  that if "--" or "//" are used then optind is stepped to the next
  argument before getopt() returns EOF.

  If an error occurs, that is an SW char precedes an unknown letter,
  then getopt() will return a '?' character and normally prints an
  error message via perror().  If the global variable opterr is set
  to false (zero) before calling getopt() then the error message is
  not printed.

  For example, if the MSDOS switch char is '/' (the MSDOS norm) and

    *optionS == "A:F:PuU:wXZ:"

  then 'P', 'u', 'w', and 'X' are option letters and 'F', 'U', 'Z'
  are followed by arguments.  A valid command line may be:

    aCommand  /uPFPi /X /A L someFile

  where:
    - 'u' and 'P' will be returned as isolated option letters.
    - 'F' will return with "Pi" as its argument string.
    - 'X' is an isolated option.
    - 'A' will return with "L" as its argument.
    - "someFile" is not an option, and terminates getOpt.  The
      caller may collect remaining arguments using argv pointers.
*/

int	getopt(int argc, char *argv[], char *optionS)
{
	unsigned char ch;
	char *optP;

#if 0
	if (SW == 0) {
		/* get SW using dos call 0x37 */
		_AX = 0x3700;
		geninterrupt(0x21);
		SW = _DL;
	}
#endif
	if (argc > optind) {
		if (letP == NULL) {
			if ((letP = argv[optind]) == NULL ||
				*(letP++) != SW)  goto gopEOF;
			if (*letP == SW) {
				optind++;  goto gopEOF;
			}
		}
		if (0 == (ch = *(letP++))) {
			optind++;  goto gopEOF;
		}
		if (':' == ch  ||  (optP = strchr(optionS, ch)) == NULL)
			goto gopError;
		if (':' == *(++optP)) {
			optind++;
			if (0 == *letP) {
				if (argc <= optind)  goto  gopError;
				letP = argv[optind++];
			}
			optarg = letP;
			letP = NULL;
		} else {
			if (0 == *letP) {
				optind++;
				letP = NULL;
			}
			optarg = NULL;
		}
		return ch;
	}
gopEOF:
	optarg = letP = NULL;
	return EOF;

gopError:
        if (argc > optind)
                optind++;
        optarg = letP = NULL;
	errno  = EINVAL;
	if (opterr)
		perror ("get command line option");
	return ('?');
}
#endif /* HAVE_GETOPT */


#ifndef HAVE_STRERROR
#ifndef HAVE_ERRNO_H
char *strerror(int errnum) {
    static char s[32];
    sprintf(s, "ERROR %d", errnum);
    return s;
}
#else
char *strerror(int errnum)
{
    switch(errnum) {
#ifdef EPERM
      case EPERM: return "Not super-user";
#endif /* EPERM */
#ifdef ENOENT
      case ENOENT: return "No such file or directory";
#endif /* ENOENT */
#ifdef ESRCH
      case ESRCH: return "No such process";
#endif /* ESRCH */
#ifdef EINTR
      case EINTR: return "interrupted system call";
#endif /* EINTR */
#ifdef EIO
      case EIO: return "I/O error";
#endif /* EIO */
#ifdef ENXIO
      case ENXIO: return "No such device or address";
#endif /* ENXIO */
#ifdef E2BIG
      case E2BIG: return "Arg list too long";
#endif /* E2BIG */
#ifdef ENOEXEC
      case ENOEXEC: return "Exec format error";
#endif /* ENOEXEC */
#ifdef EBADF
      case EBADF: return "Bad file number";
#endif /* EBADF */
#ifdef ECHILD
      case ECHILD: return "No children";
#endif /* ECHILD */
#ifdef EAGAIN
      case EAGAIN: return "Resource temporarily unavailable";
#endif /* EAGAIN */
#ifdef EWOULDBLOCK
#if !defined(EAGAIN) || defined(EAGAIN) && EAGAIN != EWOULDBLOCK
      case EWOULDBLOCK: return "Resource temporarily unavailable";
#endif
#endif /* EWOULDBLOCK */
#ifdef ENOMEM
      case ENOMEM: return "Not enough core";
#endif /* ENOMEM */
#ifdef EACCES
      case EACCES: return "Permission denied";
#endif /* EACCES */
#ifdef EFAULT
      case EFAULT: return "Bad address";
#endif /* EFAULT */
#ifdef ENOTBLK
      case ENOTBLK: return "Block device required";
#endif /* ENOTBLK */
#ifdef EBUSY
      case EBUSY: return "Mount device busy";
#endif /* EBUSY */
#ifdef EEXIST
      case EEXIST: return "File exists";
#endif /* EEXIST */
#ifdef EXDEV
      case EXDEV: return "Cross-device link";
#endif /* EXDEV */
#ifdef ENODEV
      case ENODEV: return "No such device";
#endif /* ENODEV */
#ifdef ENOTDIR
      case ENOTDIR: return "Not a directory";
#endif /* ENOTDIR */
#ifdef EISDIR
      case EISDIR: return "Is a directory";
#endif /* EISDIR */
#ifdef EINVAL
      case EINVAL: return "Invalid argument";
#endif /* EINVAL */
#ifdef ENFILE
      case ENFILE: return "File table overflow";
#endif /* ENFILE */
#ifdef EMFILE
      case EMFILE: return "Too many open files";
#endif /* EMFILE */
#ifdef ENOTTY
      case ENOTTY: return "Inappropriate ioctl for device";
#endif /* ENOTTY */
#ifdef ETXTBSY
      case ETXTBSY: return "Text file busy";
#endif /* ETXTBSY */
#ifdef EFBIG
      case EFBIG: return "File too large";
#endif /* EFBIG */
#ifdef ENOSPC
      case ENOSPC: return "No space left on device";
#endif /* ENOSPC */
#ifdef ESPIPE
      case ESPIPE: return "Illegal seek";
#endif /* ESPIPE */
#ifdef EROFS
      case EROFS: return "Read only file system";
#endif /* EROFS */
#ifdef EMLINK
      case EMLINK: return "Too many links";
#endif /* EMLINK */
#ifdef EPIPE
      case EPIPE: return "Broken pipe";
#endif /* EPIPE */
#ifdef EDOM
      case EDOM: return "Math arg out of domain of func";
#endif /* EDOM */
#ifdef ERANGE
      case ERANGE: return "Math result not representable";
#endif /* ERANGE */
#ifdef ENOMSG
      case ENOMSG: return "No message of desired type";
#endif /* ENOMSG */
#ifdef EIDRM
      case EIDRM: return "Identifier removed";
#endif /* EIDRM */
#ifdef ECHRNG
      case ECHRNG: return "Channel number out of range";
#endif /* ECHRNG */
#ifdef EL2NSYNC
      case EL2NSYNC: return "Level 2 not synchronized";
#endif /* EL2NSYNC */
#ifdef EL3HLT
      case EL3HLT: return "Level 3 halted";
#endif /* EL3HLT */
#ifdef EL3RST
      case EL3RST: return "Level 3 reset";
#endif /* EL3RST */
#ifdef ELNRNG
      case ELNRNG: return "Link number out of range";
#endif /* ELNRNG */
#ifdef EUNATCH
      case EUNATCH: return "Protocol driver not attached";
#endif /* EUNATCH */
#ifdef ENOCSI
      case ENOCSI: return "No CSI structure available";
#endif /* ENOCSI */
#ifdef EL2HLT
      case EL2HLT: return "Level 2 halted";
#endif /* EL2HLT */
#ifdef EDEADLK
      case EDEADLK: return "Deadlock condition.";
#endif /* EDEADLK */
#ifdef ENOLCK
      case ENOLCK: return "No record locks available.";
#endif /* ENOLCK */
#ifdef ECANCELED
      case ECANCELED: return "Operation canceled";
#endif /* ECANCELED */
#ifdef ENOTSUP
      case ENOTSUP: return "Operation not supported";
#endif /* ENOTSUP */
#ifdef EDQUOT
      case EDQUOT: return "Disc quota exceeded";
#endif /* EDQUOT */
#ifdef EBADE
      case EBADE: return "invalid exchange";
#endif /* EBADE */
#ifdef EBADR
      case EBADR: return "invalid request descriptor";
#endif /* EBADR */
#ifdef EXFULL
      case EXFULL: return "exchange full";
#endif /* EXFULL */
#ifdef ENOANO
      case ENOANO: return "no anode";
#endif /* ENOANO */
#ifdef EBADRQC
      case EBADRQC: return "invalid request code";
#endif /* EBADRQC */
#ifdef EBADSLT
      case EBADSLT: return "invalid slot";
#endif /* EBADSLT */
#ifdef EDEADLOCK
      case EDEADLOCK: return "file locking deadlock error";
#endif /* EDEADLOCK */
#ifdef EBFONT
      case EBFONT: return "bad font file fmt";
#endif /* EBFONT */
#ifdef ENOSTR
      case ENOSTR: return "Device not a stream";
#endif /* ENOSTR */
#ifdef ENODATA
      case ENODATA: return "no data (for no delay io)";
#endif /* ENODATA */
#ifdef ETIME
      case ETIME: return "timer expired";
#endif /* ETIME */
#ifdef ENOSR
      case ENOSR: return "out of streams resources";
#endif /* ENOSR */
#ifdef ENONET
      case ENONET: return "Machine is not on the network";
#endif /* ENONET */
#ifdef ENOPKG
      case ENOPKG: return "Package not installed";
#endif /* ENOPKG */
#ifdef EREMOTE
      case EREMOTE: return "The object is remote";
#endif /* EREMOTE */
#ifdef ENOLINK
      case ENOLINK: return "the link has been severed";
#endif /* ENOLINK */
#ifdef EADV
      case EADV: return "advertise error";
#endif /* EADV */
#ifdef ESRMNT
      case ESRMNT: return "srmount error";
#endif /* ESRMNT */
#ifdef ECOMM
      case ECOMM: return "Communication error on send";
#endif /* ECOMM */
#ifdef EPROTO
      case EPROTO: return "Protocol error";
#endif /* EPROTO */
#ifdef EMULTIHOP
      case EMULTIHOP: return "multihop attempted";
#endif /* EMULTIHOP */
#ifdef EBADMSG
      case EBADMSG: return "trying to read unreadable message";
#endif /* EBADMSG */
#ifdef ENAMETOOLONG
      case ENAMETOOLONG: return "path name is too long";
#endif /* ENAMETOOLONG */
#ifdef EOVERFLOW
      case EOVERFLOW: return "value too large to be stored in data type";
#endif /* EOVERFLOW */
#ifdef ENOTUNIQ
      case ENOTUNIQ: return "given log. name not unique";
#endif /* ENOTUNIQ */
#ifdef EBADFD
      case EBADFD: return "f.d. invalid for this operation";
#endif /* EBADFD */
#ifdef EREMCHG
      case EREMCHG: return "Remote address changed";
#endif /* EREMCHG */
#ifdef ELIBACC
      case ELIBACC: return "Can't access a needed shared lib.";
#endif /* ELIBACC */
#ifdef ELIBBAD
      case ELIBBAD: return "Accessing a corrupted shared lib.";
#endif /* ELIBBAD */
#ifdef ELIBSCN
      case ELIBSCN: return ".lib section in a.out corrupted.";
#endif /* ELIBSCN */
#ifdef ELIBMAX
      case ELIBMAX: return "Attempting to link in too many libs.";
#endif /* ELIBMAX */
#ifdef ELIBEXEC
      case ELIBEXEC: return "Attempting to exec a shared library.";
#endif /* ELIBEXEC */
#ifdef EILSEQ
      case EILSEQ: return "Illegal byte sequence.";
#endif /* EILSEQ */
#ifdef ENOSYS
      case ENOSYS: return "Unsupported file system operation";
#endif /* ENOSYS */
#ifdef ELOOP
      case ELOOP: return "Symbolic link loop";
#endif /* ELOOP */
#ifdef ERESTART
      case ERESTART: return "Restartable system call";
#endif /* ERESTART */
#ifdef ESTRPIPE
      case ESTRPIPE: return "if pipe/FIFO, don't sleep in stream head";
#endif /* ESTRPIPE */
#ifdef ENOTEMPTY
      case ENOTEMPTY: return "directory not empty";
#endif /* ENOTEMPTY */
#ifdef EUSERS
      case EUSERS: return "Too many users (for UFS)";
#endif /* EUSERS */
#ifdef ENOTSOCK
      case ENOTSOCK: return "Socket operation on non-socket";
#endif /* ENOTSOCK */
#ifdef EDESTADDRREQ
      case EDESTADDRREQ: return "Destination address required";
#endif /* EDESTADDRREQ */
#ifdef EMSGSIZE
      case EMSGSIZE: return "Message too long";
#endif /* EMSGSIZE */
#ifdef EPROTOTYPE
      case EPROTOTYPE: return "Protocol wrong type for socket";
#endif /* EPROTOTYPE */
#ifdef ENOPROTOOPT
      case ENOPROTOOPT: return "Protocol not available";
#endif /* ENOPROTOOPT */
#ifdef EPROTONOSUPPORT
      case EPROTONOSUPPORT: return "Protocol not supported";
#endif /* EPROTONOSUPPORT */
#ifdef ESOCKTNOSUPPORT
      case ESOCKTNOSUPPORT: return "Socket type not supported";
#endif /* ESOCKTNOSUPPORT */
#ifdef EOPNOTSUPP
      case EOPNOTSUPP: return "Operation not supported on socket";
#endif /* EOPNOTSUPP */
#ifdef EPFNOSUPPORT
      case EPFNOSUPPORT: return "Protocol family not supported";
#endif /* EPFNOSUPPORT */
#ifdef EAFNOSUPPORT
      case EAFNOSUPPORT: return "Address family not supported by";
#endif /* EAFNOSUPPORT */
#ifdef EADDRINUSE
      case EADDRINUSE: return "Address already in use";
#endif /* EADDRINUSE */
#ifdef EADDRNOTAVAIL
      case EADDRNOTAVAIL: return "Can't assign requested address";
#endif /* EADDRNOTAVAIL */
#ifdef ENETDOWN
      case ENETDOWN: return "Network is down";
#endif /* ENETDOWN */
#ifdef ENETUNREACH
      case ENETUNREACH: return "Network is unreachable";
#endif /* ENETUNREACH */
#ifdef ENETRESET
      case ENETRESET: return "Network dropped connection because";
#endif /* ENETRESET */
#ifdef ECONNABORTED
      case ECONNABORTED: return "Software caused connection abort";
#endif /* ECONNABORTED */
#ifdef ECONNRESET
      case ECONNRESET: return "Connection reset by peer";
#endif /* ECONNRESET */
#ifdef ENOBUFS
      case ENOBUFS: return "No buffer space available";
#endif /* ENOBUFS */
#ifdef EISCONN
      case EISCONN: return "Socket is already connected";
#endif /* EISCONN */
#ifdef ENOTCONN
      case ENOTCONN: return "Socket is not connected";
#endif /* ENOTCONN */
#ifdef ESHUTDOWN
      case ESHUTDOWN: return "Can't send after socket shutdown";
#endif /* ESHUTDOWN */
#ifdef ETOOMANYREFS
      case ETOOMANYREFS: return "Too many references: can't splice";
#endif /* ETOOMANYREFS */
#ifdef ETIMEDOUT
      case ETIMEDOUT: return "Connection timed out";
#endif /* ETIMEDOUT */
#ifdef ECONNREFUSED
      case ECONNREFUSED: return "Connection refused";
#endif /* ECONNREFUSED */
#ifdef EHOSTDOWN
      case EHOSTDOWN: return "Host is down";
#endif /* EHOSTDOWN */
#ifdef EHOSTUNREACH
      case EHOSTUNREACH: return "No route to host";
#endif /* EHOSTUNREACH */
#ifdef EALREADY
      case EALREADY: return "operation already in progress";
#endif /* EALREADY */
#ifdef EINPROGRESS
      case EINPROGRESS: return "operation now in progress";
#endif /* EINPROGRESS */
#ifdef ESTALE
      case ESTALE: return "Stale NFS file handle";
#endif /* ESTALE */
      default: {
	  static char s[32];
	  sprintf(s, "ERROR %d", errnum);
	  return s;
      }
    }
    /*NOTREACHED*/
}
#endif /* HAVE_ERRNO_H */
#endif /* HAVE_STRERROR */


#ifndef HAVE_USLEEP
int usleep(unsigned int usec)
{
#if defined(HAVE_SELECT)
    struct timeval tv;
    tv.tv_sec  = usec / 1000000;
    tv.tv_usec = usec % 1000000;
    select(0, NULL, NULL, NULL, &tv);
#elif defined(__WIN32__)
    Sleep(usec / 1000);
#endif /* HAVE_SELECT */
    return 0;
}
#endif /* HAVE_USLEEP */
