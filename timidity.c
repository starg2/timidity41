/*

    TiMidity -- Experimental MIDI to WAVE converter
    Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

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
#include <stdio.h>
#include <stdlib.h>

#if defined(SOLARIS) || defined(__WIN32__)
#include <string.h>
#else
#include <strings.h>
#endif

#ifdef __WIN32__
#include <windows.h>
#define rindex strrchr
extern int optind;
extern char *optarg;
int getopt(int, char **, char *);
#else
#include <unistd.h>
#endif

#include "config.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "output.h"
#include "controls.h"
#include "tables.h"

int free_instruments_afterwards=0;
static char def_instr_name[256]="";

#ifdef __WIN32__
int intr;
CRITICAL_SECTION critSect;

#pragma argsused
static BOOL WINAPI handler (DWORD dw)
	{
	printf ("***BREAK\n");
	intr = TRUE;
	play_mode->purge_output ();
	return TRUE;
	}
#endif

static void help(void)
{
  PlayMode **pmp=play_mode_list;
  ControlMode **cmp=ctl_list;
  printf(" TiMidity version " TIMID_VERSION " (C) 1995 Tuukka Toivonen "
	 "<toivonen@clinet.fi>\n"
	 " TiMidity is free software and comes with ABSOLUTELY NO WARRANTY.\n"
	 "\n"
#ifdef __WIN32__
	 " Win32 version by Davide Moretti <dmoretti@iper.net>\n"
	 "\n"
#endif
	 "Usage:\n"
	 "  %s [options] filename [...]\n"
	 "\n"
#ifndef __WIN32__		/*does not work in Win32 */
	 "  Use \"-\" as filename to read a MIDI file from stdin\n"
	 "\n"
#endif
	 "Options:\n"
#if defined(AU_HPUX)
	 "  -o file Output to another file (or audio server) (Use \"-\" for stdout)\n"
#elif defined (AU_LINUX)
	 "  -o file Output to another file (or device) (Use \"-\" for stdout)\n"
#else
	 "  -o file Output to another file (Use \"-\" for stdout)\n"
#endif
	 "  -O mode Select output mode and format (see below for list)\n"
	 "  -s f    Set sampling frequency to f (Hz or kHz)\n"
	 "  -a      Enable the antialiasing filter\n"
	 "  -f      "
#ifdef FAST_DECAY
					"Disable"
#else
					"Enable"
#endif
							  " fast decay mode\n"
	 "  -p n    Allow n-voice polyphony\n"
	 "  -A n    Amplify volume by n percent (may cause clipping)\n"
	 "  -C n    Set ratio of sampling and control frequencies\n"
	 "\n"
	 "  -L dir  Append dir to search path\n"
	 "  -c file Read extra configuration file\n"
	 "  -I n    Use program n as the default\n"
	 "  -P file Use patch file for all programs\n"
	 "  -D n    Play drums on channel n\n"
	 "  -Q n    Ignore channel n\n"
	 "  -F      Enable fast panning\n"
	 "  -U      Unload instruments from memory between MIDI files\n"
	 "\n"
	 "  -i mode Select user interface (see below for list)\n"
#if defined(AU_LINUX) || defined(AU_WIN32)
	 "  -B n    Set number of buffer fragments\n"
#endif
#ifdef __WIN32__
	 "  -e      Increase thread priority (evil) - be careful!\n"
#endif
	 "  -h      Display this help message\n"
	 "\n"
	 , program_name);

  printf("Available output modes (-O option):\n\n");
  while (*pmp)
	 {
		printf("  -O%c     %s\n", (*pmp)->id_character, (*pmp)->id_name);
		pmp++;
	 }
  printf("\nOutput format options (append to -O? option):\n\n"
	 "   `8'    8-bit sample width\n"
	 "   `1'    16-bit sample width\n"
	 "   `U'    uLaw encoding\n"
	 "   `l'    linear encoding\n"
	 "   `M'    monophonic\n"
	 "   `S'    stereo\n"
	 "   `s'    signed output\n"
	 "   `u'    unsigned output\n"
	 "   `x'    byte-swapped output\n");

  printf("\nAvailable interfaces (-i option):\n\n");
  while (*cmp)
	 {
      printf("  -i%c     %s\n", (*cmp)->id_character, (*cmp)->id_name);
      cmp++;
	 }
  printf("\nInterface options (append to -i? option):\n\n"
	 "   `v'    more verbose (cumulative)\n"
	 "   `q'    quieter (cumulative)\n"
	 "   `t'    trace playing\n\n");
}

static void interesting_message(void)
{
  printf(
"\n"
" TiMidity version " TIMID_VERSION " -- Experimental MIDI to WAVE converter\n"
" Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>\n"
" \n"
#ifdef __WIN32__
" Win32 version by Davide Moretti <dmoretti@iper.net>\n"
"\n"
#endif
" This program is free software; you can redistribute it and/or modify\n"
" it under the terms of the GNU General Public License as published by\n"
" the Free Software Foundation; either version 2 of the License, or\n"
" (at your option) any later version.\n"
" \n"
" This program is distributed in the hope that it will be useful,\n"
" but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
" MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
" GNU General Public License for more details.\n"
" \n"
" You should have received a copy of the GNU General Public License\n"
" along with this program; if not, write to the Free Software\n"
" Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.\n"
"\n"
);

}

static int set_channel_flag(int32 *flags, int32 i, char *name)
{
  if (i==0) *flags=0;
  else if ((i<1 || i>16) && (i<-16 || i>-1))
    {
		fprintf(stderr,
	      "%s must be between 1 and 16, or between -1 and -16, or 0\n", 
	      name);
      return -1;
	 }
  else
    {
      if (i>0) *flags |= (1<<(i-1));
		else *flags &= ~ ((1<<(-1-i)));
    }
  return 0;
}

static int set_value(int32 *param, int32 i, int32 low, int32 high, char *name)
{
  if (i<low || i > high)
    {
      fprintf(stderr, "%s must be between %ld and %ld\n", name, low, high);
      return -1;
    }
  else *param=i;
  return 0;
}

static int set_play_mode(char *cp)
{
  PlayMode *pmp, **pmpp=play_mode_list;

  while ((pmp=*pmpp++))
    {
      if (pmp->id_character == *cp)
	{
	  play_mode=pmp;
	  while (*(++cp))
		 switch(*cp)
	      {
	      case 'U': pmp->encoding |= PE_ULAW; break; /* uLaw */
			case 'l': pmp->encoding &= ~PE_ULAW; break; /* linear */

			case '1': pmp->encoding |= PE_16BIT; break; /* 1 for 16-bit */
	      case '8': pmp->encoding &= ~PE_16BIT; break;

	      case 'M': pmp->encoding |= PE_MONO; break;
	      case 'S': pmp->encoding &= ~PE_MONO; break; /* stereo */

	      case 's': pmp->encoding |= PE_SIGNED; break;
	      case 'u': pmp->encoding &= ~PE_SIGNED; break;

	      case 'x': pmp->encoding ^= PE_BYTESWAP; break; /* toggle */

			default:
		fprintf(stderr, "Unknown format modifier `%c'\n", *cp);
		return 1;
	      }
	  return 0;
	}
	 }
  
  fprintf(stderr, "Playmode `%c' is not compiled in.\n", *cp);
  return 1;
}

static int set_ctl(char *cp)
{
  ControlMode *cmp, **cmpp=ctl_list;

  while ((cmp=*cmpp++))
    {
      if (cmp->id_character == *cp)
	{
	  ctl=cmp;
	  while (*(++cp))
	    switch(*cp)
			{
			case 'v': cmp->verbosity++; break;
	      case 'q': cmp->verbosity--; break;
			case 't': /* toggle */
		cmp->trace_playing= (cmp->trace_playing) ? 0 : 1; 
		break;

	      default:
		fprintf(stderr, "Unknown interface option `%c'\n", *cp);
		return 1;
	      }
	  return 0;
	}
	 }
  
  fprintf(stderr, "Interface `%c' is not compiled in.\n", *cp);
  return 1;
}

#define MAXWORDS 10

static int read_config_file(char *name)
{
  FILE *fp;
  char tmp[1024], *w[MAXWORDS], *cp;
  ToneBank *bank=0;
  int i, j, k, line=0, words;
  static int rcf_count=0;

  if (rcf_count>50)
	 {
		fprintf(stderr, "Probable source loop in configuration files");
		return (-1);
	 }

  if (!(fp=open_file(name, 1, OF_VERBOSE)))
	 return -1;

  while (fgets(tmp, sizeof(tmp), fp))
	 {
      line++;
		w[words=0]=strtok(tmp, " \t\r\n\240");
		if (!w[0] || (*w[0]=='#')) continue;
      while (w[words] && (words < MAXWORDS))
	w[++words]=strtok(0," \t\r\n\240");
      if (!strcmp(w[0], "dir"))
	{
	  if (words < 2)
		 {
	      fprintf(stderr, "%s: line %d: No directory given\n", name, line);
	      return -2;
		 }
	  for (i=1; i<words; i++)
	    add_to_pathlist(w[i]);
	}
		else if (!strcmp(w[0], "source"))
	{
	  if (words < 2)
	    {
	      fprintf(stderr, "%s: line %d: No file name given\n", name, line);
	      return -2;
		 }
	  for (i=1; i<words; i++)
	    {
	      rcf_count++;
			read_config_file(w[i]);
	      rcf_count--;
	    }
	}
      else if (!strcmp(w[0], "default"))
	{
	  if (words != 2)
	    {
	      fprintf(stderr, 
				"%s: line %d: Must specify exactly one patch name\n",
		      name, line);
	      return -2;
	    }
	  strncpy(def_instr_name, w[1], 255);
	  def_instr_name[255]='\0';
	}
		else if (!strcmp(w[0], "drumset"))
	{
	  if (words < 2)
	    {
	      fprintf(stderr, "%s: line %d: No drum set number given\n", 
		      name, line);
			return -2;
	    }
	  i=atoi(w[1]);
	  if (i<0 || i>127)
		 {
	      fprintf(stderr, 
		      "%s: line %d: Drum set must be between 0 and 127\n",
				name, line);
	      return -2;
		 }
	  if (!drumset[i])
	    {
	      drumset[i]=safe_malloc(sizeof(ToneBank));
			memset(drumset[i], 0, sizeof(ToneBank));
		 }
	  bank=drumset[i];
	}
		else if (!strcmp(w[0], "bank"))
	{
	  if (words < 2)
		 {
	      fprintf(stderr, "%s: line %d: No bank number given\n", 
				name, line);
	      return -2;
		 }
	  i=atoi(w[1]);
	  if (i<0 || i>127)
	    {
	      fprintf(stderr, 
		      "%s: line %d: Tone bank must be between 0 and 127\n",
				name, line);
	      return -2;
	    }
	  if (!tonebank[i])
		 {
			tonebank[i]=safe_malloc(sizeof(ToneBank));
	      memset(tonebank[i], 0, sizeof(ToneBank));
	    }
	  bank=tonebank[i];
	}
      else {
	if ((words < 2) || (*w[0] < '0' || *w[0] > '9'))
	  {
		 fprintf(stderr, "%s: line %d: syntax error\n", name, line);
		 return -2;
	  }
	i=atoi(w[0]);
	if (i<0 || i>127)
	  {
	    fprintf(stderr, "%s: line %d: Program must be between 0 and 127\n",
		    name, line);
	    return -2;
	  }
	if (!bank)
	  {
	    fprintf(stderr, 
			 "%s: line %d: Must specify tone bank or drum set "
		    "before assignment\n",
		    name, line);
		 return -2;
	  }
	if (bank->tone[i].name)
	  free(bank->tone[i].name);
	strcpy((bank->tone[i].name=safe_malloc(strlen(w[1])+1)),w[1]);
	bank->tone[i].note=bank->tone[i].amp=bank->tone[i].pan=
	  bank->tone[i].strip_loop=bank->tone[i].strip_envelope=
	    bank->tone[i].strip_tail=-1;

	for (j=2; j<words; j++)
	  {
	    if (!(cp=strchr(w[j], '=')))
	      {
		fprintf(stderr, "%s: line %d: bad patch option %s\n",
			name, line, w[j]);
		return -2;
	      }
	    *cp++=0;
	    if (!strcmp(w[j], "amp"))
			{
		k=atoi(cp);
		if ((k<0 || k>MAX_AMPLIFICATION) || (*cp < '0' || *cp > '9'))
		  {
			 fprintf(stderr,
			    "%s: line %d: amplification must be between "
				 "0 and %d\n", name, line, MAX_AMPLIFICATION);
			 return -2;
		  }
		bank->tone[i].amp=k;
	      }
	    else if (!strcmp(w[j], "note"))
	      {
		k=atoi(cp);
		if ((k<0 || k>127) || (*cp < '0' || *cp > '9'))
		  {
			 fprintf(stderr,
				 "%s: line %d: note must be between 0 and 127\n",
			    name, line);
		    return -2;
		  }
		bank->tone[i].note=k;
			}
		 else if (!strcmp(w[j], "pan"))
			{
		if (!strcmp(cp, "center"))
		  k=64;
		else if (!strcmp(cp, "left"))
		  k=0;
		else if (!strcmp(cp, "right"))
		  k=127;
		else
		  k=((atoi(cp)+100) * 100) / 157;
		if ((k<0 || k>127) ||
			 (k==0 && *cp!='-' && (*cp < '0' || *cp > '9')))
		  {
			 fprintf(stderr,
				 "%s: line %d: panning must be left, right, "
				 "center, or between -100 and 100\n",
				 name, line);
			 return -2;
		  }
		bank->tone[i].pan=k;
			}
		 else if (!strcmp(w[j], "keep"))
			{
		if (!strcmp(cp, "env"))
		  bank->tone[i].strip_envelope=0;
		else if (!strcmp(cp, "loop"))
		  bank->tone[i].strip_loop=0;
		else
		  {
			 fprintf(stderr, "%s: line %d: keep must be env or loop\n",
				 name, line);
			 return -2;
		  }
			}
		 else if (!strcmp(w[j], "strip"))
			{
		if (!strcmp(cp, "env"))
		  bank->tone[i].strip_envelope=1;
		else if (!strcmp(cp, "loop"))
		  bank->tone[i].strip_loop=1;
		else if (!strcmp(cp, "tail"))
		  bank->tone[i].strip_tail=1;
		else
		  {
			 fprintf(stderr, "%s: line %d: strip must be env, loop, or tail\n",
				 name, line);
			 return -2;
		  }
			}
		 else
			{
		fprintf(stderr, "%s: line %d: bad patch option %s\n",
			name, line, w[j]);
		return -2;
			}
	  }
		}
	 }
  if (ferror(fp))
	 {
		fprintf(stderr, "Can't read %s: %s\n", name, sys_errlist[errno]);
		close_file(fp);
		return -2;
	 }
  close_file(fp);
  return 0;
}
#ifdef __WIN32__
int __cdecl main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
  int
	 c, cmderr=0, i, got_a_configuration=0, try_config_again=0,
	 need_stdin=0, need_stdout=0;

  int32
	 tmpi32, output_rate=DEFAULT_RATE;

  char
	 *output_name=0;

#if defined(AU_LINUX) || defined(AU_WIN32)
  int buffer_fragments=-1;
#endif

#ifdef __WIN32__
	int evil=0;
#endif

#ifdef DANGEROUS_RENICE
#include <sys/resource.h>
  int u_uid=getuid();
  if (setpriority(PRIO_PROCESS, 0, DANGEROUS_RENICE) < 0)
	 fprintf(stderr, "Couldn't set priority to %d.\n", DANGEROUS_RENICE);
  setreuid(u_uid, u_uid);
#endif

  if ((program_name=rindex(argv[0], '/'))) program_name++;
  else program_name=argv[0];
  if (argc==1)
	 {
		interesting_message();
		return 0;
	 }

  if (!read_config_file(CONFIG_FILE))
	 got_a_configuration=1;

  while ((c=getopt(argc, argv, "UI:P:L:c:A:C:ap:fo:O:s:Q:FD:hi:"
#if defined(AU_LINUX) || defined(AU_WIN32)
			"B:" /* buffer fragments */
#endif
#ifdef __WIN32__
			"e"	/* evil (be careful) */
#endif
			))>0)
	 switch(c)
		{
		case 'U': free_instruments_afterwards=1; break;
		case 'L': add_to_pathlist(optarg); try_config_again=1; break;
		case 'c':
	if (read_config_file(optarg)) cmderr++;
	else got_a_configuration=1;
	break;

		case 'Q':
	if (set_channel_flag(&quietchannels, atoi(optarg), "Quiet channel"))
	  cmderr++;
	break;

		case 'D':
	if (set_channel_flag(&drumchannels, atoi(optarg), "Drum channel"))
	  cmderr++;
	break;

		case 'O': /* output mode */
	if (set_play_mode(optarg))
	  cmderr++;
	break;

		case 'o':	output_name=optarg; break;

		case 'a': antialiasing_allowed=1; break;

		case 'f': fast_decay=(fast_decay) ? 0 : 1; break;

		case 'F': adjust_panning_immediately=1; break;

		case 's': /* sampling rate */
	i=atoi(optarg);
	if (i < 100) i *= 1000;
	if (set_value(&output_rate, i, MIN_OUTPUT_RATE, MAX_OUTPUT_RATE,
				"Resampling frequency")) cmderr++;
	break;

		case 'P': /* set overriding instrument */
	strncpy(def_instr_name, optarg, 255);
	def_instr_name[255]='\0';
	break;

		case 'I':
	if (set_value(&tmpi32, atoi(optarg), 0, 127,
				"Default program")) cmderr++;
	else default_program=tmpi32;
	break;
		case 'A':
	if (set_value(&amplification, atoi(optarg), 1, MAX_AMPLIFICATION,
				"Amplification")) cmderr++;
	break;
		case 'C':
	if (set_value(&control_ratio, atoi(optarg), 1, MAX_CONTROL_RATIO,
				"Control ratio")) cmderr++;
	break;
		case 'p':
	if (set_value(&tmpi32, atoi(optarg), 1, MAX_VOICES,
				"Polyphony")) cmderr++;
	else voices=tmpi32;
	break;

		case 'i':
	if (set_ctl(optarg))
	  cmderr++;
#if defined(AU_LINUX) || defined(AU_WIN32)
	else if (buffer_fragments==-1 && ctl->trace_playing)
	  /* user didn't specify anything, so use 2 for real-time response */
#if defined(AU_LINUX)
	  buffer_fragments=2;
#else
		buffer_fragments=3;		/* On Win32 2 is chunky */
#endif
#endif
	break;

#if defined(AU_LINUX) || defined(AU_WIN32)
		case 'B':
	if (set_value(&tmpi32, atoi(optarg), 0, 1000,
				"Buffer fragments")) cmderr++;
	else buffer_fragments=tmpi32;
	break;
#endif
#ifdef __WIN32__
		case 'e': /* evil */
			evil = 1;
			break;
#endif
		case 'h':
	help();
	return 0;

		default:
	cmderr++; break;
		}

  if (!got_a_configuration)
	 {
		if (!try_config_again || read_config_file(CONFIG_FILE))
	cmderr++;
	 }

  /* If there were problems, give up now */
  if (cmderr || optind >= argc)
	 {
		fprintf(stderr, "Try %s -h for help\n", program_name);
		return 1; /* problems with command line */
	 }

  /* Set play mode parameters */
  play_mode->rate=output_rate;
  if (output_name)
	 {
		play_mode->name=output_name;
		if (!strcmp(output_name, "-"))
	need_stdout=1;
	 }
#if defined(AU_LINUX) || defined(AU_WIN32)
  if (buffer_fragments != -1)
	 play_mode->extra_param[0]=buffer_fragments;
#endif

  init_tables();

  if (optind<argc)
	 {
		int orig_optind=optind;

		while (optind<argc)
	if (!strcmp(argv[optind++], "-"))
	  need_stdin=1;
		optind=orig_optind;

		if (ctl->open(need_stdin, need_stdout))
	{
	  fprintf(stderr, "Couldn't open %s\n", ctl->id_name);
	  play_mode->close_output();
	  return 3;
	}
		/* Open output device */
		if (play_mode->open_output()<0)
	{
	  fprintf(stderr, "Couldn't open %s\n", play_mode->id_name);
	  ctl->close();
	  return 2;
	}

		if (!control_ratio)
	{
	  control_ratio = play_mode->rate / CONTROLS_PER_SECOND;
	  if(control_ratio<1)
		 control_ratio=1;
	  else if (control_ratio > MAX_CONTROL_RATIO)
		 control_ratio=MAX_CONTROL_RATIO;
	}

		if (*def_instr_name)
	set_default_instrument(def_instr_name);

#ifdef __WIN32__
		SetConsoleCtrlHandler (handler, TRUE);
		InitializeCriticalSection (&critSect);
		if(evil)
			if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL))
				fprintf(stderr, "Error raising process priority.\n");
#endif

		/* Return only when quitting */
		ctl->pass_playing_list(argc-optind, &argv[orig_optind]);

		play_mode->close_output();
		ctl->close();
#ifdef __WIN32__
			DeleteCriticalSection (&critSect);
#endif
		return 0;

	 }
  return 0;
}
