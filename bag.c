/*

    bag -- A quick hack to buffer audio data between TiMidity and an
           external player program.

    This program is in the Public Domain. It is distributed without
    much hope that it will be useful, and WITHOUT ANY WARRANTY;
    definitely without any implied warranty of MERCHANTABILITY or
    FITNESS FOR ANY PURPOSE.

    bag may enable you to play complex files at higher output rates,
    but it's a bit of a pain to use.

    Here's a usage example for Linux. splay is a program that (with
    this invocation) plays audio data from stdin. Your flag
    requirements may vary.

		timidity -id -Or1lsS -o- -s22 -p48 filenames |
		bag -b 2M -t 1M -c 4k |
		splay -b16 -s22 -S

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

static int bytes(char *s)
{
  int i;
  i=atoi(s);
  while (*s>='0' && *s<='9')
    s++;

  switch(*s)
    {
    case 'm':
    case 'M':
      i *= 1024;
    case 'k':
    case 'K':
      i *= 1024;
    }
  return i;
}

int main(int argc, char **argv)
{
  int bs=1024*1024, rs=4*1024, ws=4*1024, cs=4*1024, thresh=64*1024;
  int c, junk=0, verbosity=0, flush_and_go=0;
  char *bag, *head, *tail;
  extern char *optarg; /* for Sun's gcc */

  fd_set readfds, writefds, brokefds;
  struct timeval tmo;

  while ((c=getopt(argc, argv, "b:t:r:w:c:hv"))>0)
    switch(c)
      {
      case 'b': bs=bytes(optarg); break;
      case 't': thresh=bytes(optarg); break;
      case 'r': rs=bytes(optarg); break;
      case 'w': ws=bytes(optarg); break;
      case 'c': rs=ws=bytes(optarg); break;
      case 'v': verbosity++; break;
      case '?':
      case 'h':
	printf("\nBag usage:\n\n"
	       "  timidity -id -Or[format options] -o- [options and filenames] |\n"
	       "  bag [bag options] | audio_player [player options]\n\n"
	       "Bag options:\n\n"
	       "  -b size    Set buffer size (default %d)\n"
	       "  -r size    Set read chunk size (default %d)\n"
	       "  -w size    Set write chunk size (default %d)\n"
	       "  -c size    Set read/write chunk size\n"
	       "  -t size    Set start threshold for writing (default %d)\n"
	       "  -v         Verbose mode\n"
	       "  -h         This help screen\n\n"
	       ,bs,rs,ws,thresh);
	return 0;
      }

  if (ws>rs) cs=ws;
  else cs=rs;
  if (bs < ws+rs)
    {
      bs=ws+rs;
      fprintf(stderr, "bag: Buffer size increased to %d bytes\n", bs);
    }
  if (thresh > (bs-rs))
    {
      thresh=bs-rs;
      fprintf(stderr, "bag: Threshold lowered to %d bytes\n", bs);
    }

  head=tail=bag=malloc(bs);
  junk=0;
  while (!flush_and_go)
    {
      FD_ZERO(&readfds);
      FD_ZERO(&writefds);
      FD_ZERO(&brokefds);

      if (junk<(bs-rs))
	FD_SET(STDIN_FILENO, &readfds);

      if (junk>ws && junk>thresh)
	{
	  FD_SET(STDOUT_FILENO, &writefds);
	  thresh=0;
	}
      
      FD_SET(STDIN_FILENO, &brokefds);
      FD_SET(STDOUT_FILENO, &brokefds);
      
      tmo.tv_sec=1;
      tmo.tv_usec=0;
#ifdef HPUX
      if ((c=select(FD_SETSIZE,(int *) &readfds,(int *) &writefds, (int *) &brokefds, &tmo))<0)
#else
      if ((c=select(FD_SETSIZE, &readfds, &writefds, &brokefds, &tmo))<0)
#endif
	perror("\nbag: select");

      if (verbosity)
	fprintf(stderr, "\rbag: %8d bytes", junk);

      if (FD_ISSET(STDOUT_FILENO, &brokefds))
	{
	  fprintf(stderr, "\nbag: Elvis has left the building\n");
	  return 1;
	}

      if (FD_ISSET(STDIN_FILENO, &readfds))
	{
	  if ((c=read(STDIN_FILENO, head, 
		      (head+rs >= bag+bs) ? (bag+bs-head) : rs))<=0)
	    {
	      if (c==0 || errno!=EAGAIN)
		flush_and_go=1;
	    }
	  else
	    {
	      junk+=c;
	      head+=c;
	      if (head>=bag+bs)
		head-=bs;
	    }
	}
      else if (FD_ISSET(STDIN_FILENO, &brokefds))
	flush_and_go=1;

      if (flush_and_go || FD_ISSET(STDOUT_FILENO, &writefds))
	do
	  {
	    c=(tail+ws >= bag+bs) ? (bag+bs-tail) : ws;
	    if (c>junk) c=junk;
	    
	    if (flush_and_go && verbosity)
	      fprintf(stderr, "\rbag: Flushing, %8d bytes", junk);
	    
	    c=write(STDOUT_FILENO, tail, c);
	    junk -= c;
	    tail += c;
	    if (tail>=bag+bs)
	      tail-=bs;
	  } while (flush_and_go && junk);
    }
  if (verbosity)
    fprintf(stderr, "\nbag: All done\n");
  return 0;
}
