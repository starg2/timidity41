#ifndef ___SUPPORT_H_
#define ___SUPPORT_H_


#ifndef HAVE_VSNPRINTF
#include <stdarg.h> /* for va_list */
extern void vsnprintf(char *buff, size_t bufsiz, const char *fmt, va_list ap);
#endif

#ifndef HAVE_SNPRINTF
extern void snprintf(char *buff, size_t bufsiz, const char *fmt, ...);
#endif /* HAVE_SNPRINTF */

#ifndef HAVE_GETOPT
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern int getopt(int argc, char *argv[], char *optionS);
#endif
#endif /* HAVE_GETOPT */

#ifndef HAVE_STRERROR
extern char *strerror(int errnum);
#endif /* HAVE_STRERROR */

/* There is no prototype of usleep() on Solaris. Why? */
#if !defined(HAVE_USLEEP) || defined(SOLARIS)
extern int usleep(unsigned int usec);
#endif

#ifndef HAVE_SLEEP
#define sleep(s) usleep((s) * 1000000)
#endif /* HAVE_SLEEP */

#ifndef HAVE_STRDUP
extern char *strdup(const char *s);
#endif /* HAVE_STRDUP */

#ifndef HAVE_GETCWD
extern char *getcwd(char *buf, size_t size);
#endif /* HAVE_GETCWD */

#ifndef HAVE_STRSTR
#define strstr(s,c)	index(s,c)
#endif /* HAVE_STRSTR */

#ifndef HAVE_STRNCASECMP
int strncasecmp(char *s1, char *s2, unsigned int len);
#endif /* HAVE_STRNCASECMP */


#endif /* ___SUPPORT_H_ */
