/*

    Wav2pat -- simple WAVE to GUS patch converter
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

    This is a little tool to convert RIFF WAVEs to GUS-compatible
    patches. A WAVE is read from stdin, and a GUS patch is written to
    stdout.

*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef SOLARIS
#include <string.h>
#else
#include <strings.h>
#endif

#include "config.h"
#include "instrum.h"

#define DEFAULT_NOTE 60
#define DEFAULT_ROOT 261626

int32 freq_table[128]=
{
 8176, 8662, 9177, 9723, 
 10301, 10913, 11562, 12250, 
 12978, 13750, 14568, 15434,
 
 16352, 17324, 18354, 19445,
 20602, 21827, 23125, 24500, 
 25957, 27500, 29135, 30868, 

 32703, 34648, 36708, 38891,
 41203, 43654, 46249, 48999,
 51913, 55000, 58270, 61735,

 65406, 69296, 73416, 77782,
 82407, 87307, 92499, 97999,
 103826, 110000, 116541, 123471,

 130813, 138591, 146832, 155563,
 164814, 174614, 184997, 195998,
 207652, 220000, 233082, 246942,

 261626, 277183, 293665, 311127,
 329628, 349228, 369994, 391995,
 415305, 440000, 466164, 493883,

 523251, 554365, 587330, 622254,
 659255, 698456, 739989, 783991,
 830609, 880000, 932328, 987767,

 1046502, 1108731, 1174659, 1244508,
 1318510, 1396913, 1479978, 1567982,
 1661219, 1760000, 1864655, 1975533,

 2093005, 2217461, 2349318, 2489016,
 2637020, 2793826, 2959955, 3135963,
 3322438, 3520000, 3729310, 3951066,

 4186009, 4434922, 4698636, 4978032,
 5274041, 5587652, 5919911, 6271927,
 6644875, 7040000, 7458620, 7902133,

 8372018, 8869844, 9397273, 9956063, 
 10548082, 11175303, 11839822, 12543854
};

int main(int argc, char **argv)
{
  int32
    note=DEFAULT_NOTE,
    root=DEFAULT_ROOT,
    verbosity=0,
    samplerate=-1,
    datasize=-1,
    junklength, c;

  int
    infd=STDIN_FILENO;

  unsigned char 
    thing[4096],
    *point, 
    modes=0;

  while ((c=getopt(argc, argv, "r:n:v"))>0)
    switch(c)
      {
      case 'r': root=atol(optarg); break;
      case 'n': root=freq_table[atoi(optarg) & 127]; break;
      case 'u': note=atol(optarg); break;
      case 'v': verbosity++; break;
      default: return 1;
      }

  if (optind==argc-1)
    {
      if ((infd=open(argv[optind], O_RDONLY))<0)
	{
	  perror("wav2pat: I can't open your WAVE");
	  return 3;
	}
    }
  else if (optind!=argc)
    {
      fprintf(stderr, 
	      "Usage: wav2pat [-r RootFreq | -n Note] [-u Note] [-v] [filename]\n");
      return 1;
    }

  /* check out the putative RIFF WAVE on stdin */

  if (read(infd, thing, 12)<=0)
    {
      perror("wav2pat: I can't read your WAVE");
      return 3;
    }

  if (memcmp(thing, "RIFF", 4) || memcmp(thing+8, "WAVE", 4))
    {
      fprintf(stderr, "wav2pat: I want a RIFF WAVE on stdin!\n");
      return 2;
    }

  while (datasize==-1)
    {
      if (read(infd, thing, 8)!=8)
	{
	  perror("wav2pat: "
		 "Your WAVE ran out before I got to the interesting bits");
	  return 3;
	}
      junklength=LE_LONG(*((int32 *)(thing+4)));

      /* This Microsoft file format is designed to be impossible to
	 parse correctly if one doesn't have the full specification.
	 If you have a wave with an INFO "chunk", you lose. Thank you
	 for playing. */

      if (!memcmp(thing, "fmt ", 4))
	{
	  if (junklength > 4096)
	    {
	      fprintf(stderr, "wav2pat: "
		      "WAVEs with %ld-byte format blocks make me throw up!\n",
		      junklength);
	      return 2;
	    }
	  if (read(infd, thing, junklength) != junklength)
	    {
	      perror("wav2pat: Your WAVE is mangled");
	      return 3;
	    }
	  if (LE_SHORT(*((int16 *)(thing))) != 1)
	    {
	      fprintf(stderr, "wav2pat: I don't understand your WAVE. "
		      "It has a type %d format chunk!\n",
		      LE_SHORT(*((int16 *)(thing))));
	      return 2;
	    }
	  if (LE_SHORT(*((int16 *)(thing + 2))) != 1)
	    {
	      fprintf(stderr, "wav2pat: This WAVE has %d channels! " 
		      "There can be only one!\n",
		      LE_SHORT(*((int16 *)(thing + 2))));
	      return 2;
	    }
	  samplerate=LE_LONG(*((int32 *)(thing + 4)));
	  switch(LE_SHORT(*((int16 *)(thing + 14))))
	    {
	    case 8: modes |= MODES_UNSIGNED; break;
	    case 16: modes |= MODES_16BIT; break;
	    default:
	      fprintf(stderr, "wav2pat: Ack! Ppthbth! %d-bit samples!",
		      LE_SHORT(*((int16 *)(thing + 14))));
	      return 2;
	    }
	  if (verbosity)
	    fprintf(stderr, "wav2pat: This is a %d-bit, %ld Hz WAVE\n",
		    (LE_SHORT(*((int16 *)(thing + 14)))),
		    samplerate);
	}
      else if (!memcmp(thing, "data", 4))
	{
	  if (samplerate==-1)
	    {
	      fprintf(stderr, "wav2pat: Your WAVE has no format information before data!\n");
	      return 2;
	    }
	  if (verbosity)
	    fprintf(stderr, "wav2pat: It has %ld bytes of data\n", junklength);
	  datasize=junklength;
	}
      else
	{
	  if (verbosity)
	    fprintf(stderr, "wav2pat: "
		    "Your WAVE has a %ld-byte chunk called `%4.4s'\n",
		    junklength, thing);

	  /* It's cool to pad chunks with NULs to align them on
             half-word boundaries. */
	  if (junklength & 1)
	    junklength++;

	  while (junklength>0)
	    {
	      if ((c=read(infd, thing, (junklength>4096) ? 4096 : junklength)) 
		  <= 0)
		{
		  perror("wav2pat: Now your WAVE has run out of data");
		  return 3;
		}
	      junklength -= c;
	    }
	}
    }

  /* hammer together something that looks like a GUS patch header */

#define pound(a) *point++=(a);
#define pounds(a) { *((int16 *)point)=LE_SHORT(a); point+=2; }
#define poundl(a) { *((int32 *)point)=LE_LONG(a); point+=4; }
#define bounce(a) point += a;
  
  memset(thing, 0, 335);
  point=thing;

  /* header */
  memcpy(point, "GF1PATCH110", 12);
  point += 12;

  /* Gravis ID */
  memcpy(point, "ID#000002", 10);
  point += 10;
  
  /* description */
  strcpy(point, "Copyleft 1995 EWE&U Conductions and one Retreated Gravi\032");
  point += 60;

  pound(1); /* instruments */
  pound(14); /* voices */
  pound(0); /* channels */
  pounds(1); /* waveforms */
  pounds(127); /* master volume */
  poundl(datasize); /* data size */
  bounce(36); /* reserved */

  pounds(1); /* instrument # */
  strcpy(point, "Bleahnoise"); /* instrument name */
  point += 16;
  poundl(datasize); /* instrument size */
  pound(1); /* layers */
  bounce(40); /* reserved */

  pound(0); /* layer duplicate */
  pound(0); /* layer */
  poundl(datasize); /* layer size */
  pound(1); /* samples */
  bounce(40); /* reserved */
  
  strcpy(point, "bleah"); /* wave name */
  point += 7;
  pound(0); /* fractions */
  poundl(datasize); /* wave size */
  poundl(0); /* loop start */
  poundl(datasize); /* loop end */
  pounds(samplerate); /* sample rate */
  poundl(8176); /* low freq */
  poundl(12543854); /* high freq */
  poundl(root); /* root freq */
  pounds(512); /* tune */
  pound(7);  /* balance */

  pound(63); /* envelope rates */
  pound(63);
  pound(63);
  pound(63);
  pound(63);
  pound(63);

  pound(240); /* envelope offsets */
  pound(240);
  pound(240);
  pound(240);
  pound(240);
  pound(240);

  pound(0); /* tremolo sweep */
  pound(0); /* tremolo rate */
  pound(0); /* tremolo depth */
  pound(0); /* vibrato sweep */
  pound(0); /* vibrato rate */
  pound(0); /* vibrato depth */

  pound(modes); /* modes */

  pounds(note); /* scale freq */
  pounds(1024); /* scale factor */
  bounce(36); /* reserved */

  write(STDOUT_FILENO, thing, 335);

  /* wave data */

  while (datasize>0)
    {
      if ((c=read(infd, thing, (datasize>4096) ? 4096 : datasize)) 
	  <= 0)
	{
	  perror("wav2pat: I can't read data");
	  return 3;
	}
      write(STDOUT_FILENO, thing, c);
      datasize -= c;
    }

  /* be courteous */
  if (infd != STDIN_FILENO)
    close(infd);

  return 0;
}
