/*

    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999,2000 Masanao Izumo <mo@goice.co.jp>
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
#include "timidity.h"
#ifdef JAPANESE
/** Network Kanji Filter. (PDS Version)
************************************************************************
** Copyright (C) 1987, Fujitsu LTD. (Itaru ICHIKAWA)
** 連絡先： （株）富士通研究所　ソフト３研　市川　至
** （E-Mail Address: ichikawa@flab.fujitsu.co.jp）
**    営利を目的としない限り、このソースのいかなる
**    複写，改変，修正も許諾します。その際には、この部分を残すこと。
**    このプログラムについては特に何の保証もしない、悪しからず。
**    Everyone is permitted to do anything on this program
**    including copying, modifying, improving
**    as long as you don't try to make money off it,
**    or pretend that you wrote it.
**    i.e., the above copyright notice has to appear in all copies.
**    THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE.
***********************************************************************/

/* 以下のソースは、nkf を文字列操作できるよう改造したライブラリである。

   nkf_conv(元文字列,出力文字列、out モード)
     出力文字列を NULL としたときは、元文字列を操作する。
     バグ : 変換され出力される文字列のための領域はある程度とっておくこと。
	    さもないと、バグを生じる。
   nkf_convert(元文字列、出力文字列、出力文字列の最大の大きさ、
               in モード、out モード)
     kanji_conv に準じる。出力文字列の最大の大きさが指定できる。
     その大きさ以上になったときはそれ以上の文字の出力は打ち切られる。
   モード
     nkf の convert に与えるオプションを与える文字列。空白で区切って指定する。
     各オプション:

   このプログラムに関しての著作権がらみのことは nkf に準じるものとする。
   無保証であるので、使用の場合は自らの責任をもってすること。
   改変者 青木大輔	1997.02
*/

/* 無駄なところを削除した．
   他で用いられないインターフェースは static にした．
   コンパイラの Warning メッセージを抑制するように ANSI C の形式にした．
   文字を unsigned char * で SFILE に蓄えるようにした．
   SFILE を簡単化．
   input_f == FALSE で convert すると，半角カタカナ SJIS が EUC
         と判断されてしまうバグ(仕様だった？)を直した．
	 しかしながら，SJIS の半角カタカナ 2 文字と EUC は区別できない
	 場合がある．このときは SJIS として変換することにした．
   EUC_STRICT_CHECK を定義すると EUC-Japan の定義コードを完全にチェックする
   ようにした．
   読み込み文字を指定できるようにした．
   改変者 出雲正尚 1997
*/

/* もし，EUC-Japan の完全なチェックをする場合は EUC_STRICT_CHECK を定義
 * してください．ただし，1 バイトでも EUC-Japan の未定義文字が含まれていると
 * EUC とみなされなくなってしまいます．他のプログラムで漢字コードを EUC に変換
 * した場合，EUC の未定義域へマップされる可能性があります．
 */
/* #define EUC_STRICT_CHECK */

#if 0
static char *CopyRight =
      "Copyright (C) 1987, FUJITSU LTD. (I.Ichikawa)";
static char *Version =
      "1.5";
static char *Patchlevel =
      "1/9503/Shinji Kono";
#endif

#if (defined(__TURBOC__) || defined(LSI_C)) && !defined(MSDOS)
#define MSDOS
#endif

#include <stdio.h>
#include <stdlib.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "common.h"
#include "nkflib.h"

#ifndef FALSE
#define	FALSE	0
#endif /* FALSE */

#ifndef TRUE
#define	TRUE	1
#endif /* TRUE */

/* state of output_mode and input_mode	*/

#define		ASCII		0
#define		X0208		1
#define		X0201		2
#define		NO_X0201	3
#define		JIS_INPUT	4
#define		SJIS_INPUT	5
#define		LATIN1_INPUT	6
#define		EUC_INPUT	7

#define		ESC	0x1b
#define		SP	0x20
#define		AT	0x40
#define		SSP	0xa0
#define		DEL	0x7f
#define		SI	0x0f
#define		SO	0x0e
#define		SSO	0x8e

#define		HOLD_SIZE	32

#define		DEFAULT_J	'B'
#define		DEFAULT_R	'B'

#define		SJ0162	0x00e1		/* 01 - 62 ku offset */
#define		SJ6394	0x0161		/* 63 - 94 ku offset */

/* SFILE begin */
/* 文字列 を FILE みたいに扱う小細工 */

/*
   これは nkf の漢字コード変換がファイルに対してのみ対応しているのでそれを
   文字列操作で使えるようにするためのインターフェースである。ただし、
   対応している機能は少ないし、必要なものしか作っていない。したがって、
   これらは nkf の中でしか意味のないものであろう。

   SFILE は FILE みたいなもので文字列をファイルみたいに扱えるようにする。
   SFILE を使うためには必ずオープンすること。sopen で mode=="new" または
   "auto" 指定していなければクローズする必要はない。SFILE の中を直接操作
   した場合はいろいろ問題が出てくるであろう。

   SEOF は EOF みたいなもの。

   sopen は open みたいな関数で、
      sf : SFILE 型の変数
      st : 文字列
      maxsize : 文字列が許容できる最大の大きさ。sputc 時に制限を入れるもの。
		maxsize に -1 を指定するとこの処理を無視するようになる。
		そのときは、必要以上の文字を sputc しないように気をつけなけれ
		ばならない。
      mode : newstr、stdout、stdin の文字列を指定できる。
	     例えば mode="new stdout"
	     newstr は自動的に文字列のメモリを maxsize だけ獲得する。
	     ただし、maxsize < 1 のときはディフォルトの値を獲得する。
	     stdout は SFILE の標準出力 stdout となる文字列を指定する。
	     stdin は SFILE の標準入力 stdin となる文字列を指定する。

   sclose は close みたいな関数で、newstr でオープンされていたときは、
   文字列も free で消去する。

   sgetc、sungetc、sputc、sputchar はそれぞれ getc、ungetc、putc、putchar
   に相当する。引数の sf が NULL の時は SEOF を返す。
*/

typedef struct __SFILE {
  unsigned char *pointer;      /* 文字列現在のポインタ */
  unsigned char *head;	       /* 文字列の最初の位置 */
  unsigned char *tail;	       /* 文字列の許容の最後の位置 */
  char mode[20];	       /* 文字列オープンモード newstr,stdout,stdin */
				/* "newstr stdin" の組合わせはない */
} SFILE;
#define SEOF -1

static SFILE *sstdout=NULL;
static SFILE *sstdin=NULL; /* Never used ? */
#ifndef BUFSIZ
#define BUFSIZ 1024
#endif /* BUFSIZ */
static char sfile_buffer[BUFSIZ];
#ifndef SAFE_CONVERT_LENGTH
#define SAFE_CONVERT_LENGTH(len) (2 * (len) + 7)
#endif /* SAFE_CONVERT_LENGTH */

/* Functions */
static SFILE *sopen(SFILE *, char *string,signed int maxsize,char *md);
static void sclose(SFILE *sf);
static int sgetc(SFILE *sf);
static int sungetc(int c,SFILE *sf);
static int sputc(int c,SFILE *sf);
#define sputchar(c) sputc(c,sstdout)

/* nkf 漢字コンバート */
char *nkf_convert(char *si,char *so,int maxsize,char *in_mode,char *out_mode);
char *nkf_conv(char *si,char *so,char *out_mode);

/* mime related */
static int	base64decode(int c);
static int	mime_getc(SFILE *f);
static int	mime_begin(SFILE *f);
static int	mime_ungetc(unsigned int c);
#define VOIDVOID 0

/* MIME preprocessor */
#define GETC(p)	((!mime_mode)?sgetc(p):mime_getc(p))
#define UNGETC(c,p)	((!mime_mode)?sungetc(c,p):mime_ungetc(c))

/* buffers */
static unsigned char   hold_buf[HOLD_SIZE*2];
static int	       hold_count;

/* MIME preprocessor fifo */
#define MIME_BUF_SIZE	4    /* 2^n ring buffer */
#define MIME_BUF_MASK	(MIME_BUF_SIZE-1)
#define Fifo(n)		mime_buf[(n)&MIME_BUF_MASK]
static unsigned char	mime_buf[MIME_BUF_SIZE];
static unsigned int	mime_top = 0;
static unsigned int	mime_last = 0;

/* flags */
static int	unbuf_f = FALSE,
		estab_f = FALSE;
static int	rot_f = FALSE;		/* rot14/43 mode */
static int	input_f = FALSE;	/* non fixed input code	 */
static int	alpha_f = FALSE;	/* convert JIx0208 alphbet to ASCII */
static int	mime_f = FALSE;		/* convert MIME base64 */
static int	broken_f = FALSE;	/* convert ESC-less broken JIS */
static int	iso8859_f = FALSE;	/* ISO8859 through */
#ifdef MSDOS
static int	x0201_f = TRUE;		/* Assume JISX0201 kana */
#else
static int	x0201_f = NO_X0201;	/* Assume NO JISX0201 */
#endif

/* X0208 -> ASCII converter */

static int	c1_return;

/* fold parameter */
static int line = 0;	/* chars in line */
static int prev = 0;
static int fold_f  = FALSE;
static int fold_len  = 0;

/* options */
static char	kanji_intro = DEFAULT_J,
		ascii_intro = DEFAULT_R;

/* Folding */
static int fold(int c2, int c1);
#define FOLD_MARGIN  10

/* converters */
static int	s_iconv (int c2, int c1);
static int	e_oconv (int c2, int c1);
static int	j_oconv (int c2, int c1);
static int	s_oconv (int c2, int c1);
static int	convert (SFILE *f);
static int	h_conv (SFILE *f, int c1, int c2);
static int	push_hold_buf (int c2, int c1);
static int	(*iconv) (int c2, int c1);   /* s_iconv or oconv */
static int	(*oconv) (int c2, int c1);   /* [ejs]_oconv */
static int	pre_convert(int c1, int c2);/* rot13 or JISX0201 -> JISX0208 */
static int	check_kanji_code(unsigned char *p);

/* Global states */
static int	output_mode = ASCII,	/* output kanji mode */
		input_mode = ASCII,	/* input kanji mode */
		shift_mode = FALSE;	/* TRUE shift out, or X0201  */
static int	mime_mode = FALSE;	/* MIME mode B base64, Q hex */

/* X0208 -> ASCII translation table */
/* X0201 / X0208 conversion tables */

/* X0201 kana conversion table */
/* 90-9F A0-DF */
static unsigned char cv[]= {
0x21,0x21,0x21,0x23,0x21,0x56,0x21,0x57,
0x21,0x22,0x21,0x26,0x25,0x72,0x25,0x21,
0x25,0x23,0x25,0x25,0x25,0x27,0x25,0x29,
0x25,0x63,0x25,0x65,0x25,0x67,0x25,0x43,
0x21,0x3c,0x25,0x22,0x25,0x24,0x25,0x26,
0x25,0x28,0x25,0x2a,0x25,0x2b,0x25,0x2d,
0x25,0x2f,0x25,0x31,0x25,0x33,0x25,0x35,
0x25,0x37,0x25,0x39,0x25,0x3b,0x25,0x3d,
0x25,0x3f,0x25,0x41,0x25,0x44,0x25,0x46,
0x25,0x48,0x25,0x4a,0x25,0x4b,0x25,0x4c,
0x25,0x4d,0x25,0x4e,0x25,0x4f,0x25,0x52,
0x25,0x55,0x25,0x58,0x25,0x5b,0x25,0x5e,
0x25,0x5f,0x25,0x60,0x25,0x61,0x25,0x62,
0x25,0x64,0x25,0x66,0x25,0x68,0x25,0x69,
0x25,0x6a,0x25,0x6b,0x25,0x6c,0x25,0x6d,
0x25,0x6f,0x25,0x73,0x21,0x2b,0x21,0x2c,
0x00,0x00};

/* X0201 kana conversion table for daguten */
/* 90-9F A0-DF */
static unsigned char dv[]= {
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x25,0x2c,0x25,0x2e,
0x25,0x30,0x25,0x32,0x25,0x34,0x25,0x36,
0x25,0x38,0x25,0x3a,0x25,0x3c,0x25,0x3e,
0x25,0x40,0x25,0x42,0x25,0x45,0x25,0x47,
0x25,0x49,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x25,0x50,0x25,0x53,
0x25,0x56,0x25,0x59,0x25,0x5c,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00};

/* X0201 kana conversion table for han-daguten */
/* 90-9F A0-DF */
static unsigned char ev[]= {
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x25,0x51,0x25,0x54,
0x25,0x57,0x25,0x5a,0x25,0x5d,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00};

/* X0208 kigou conversion table */
/* 0x8140 - 0x819e */
static unsigned char fv[] = {
0x00,0x00,0x00,0x00,0x2c,0x2e,0x00,0x3a,
0x3b,0x3f,0x21,0x00,0x00,0x27,0x60,0x00,
0x5e,0x00,0x5f,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x2d,0x00,0x2f,
0x5c,0x7e,0x00,0x7c,0x00,0x00,0x60,0x27,
0x22,0x22,0x28,0x29,0x00,0x00,0x5b,0x5d,
0x7b,0x7d,0x3c,0x3e,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x2b,0x2d,0x00,0x00,
0x00,0x3d,0x00,0x3c,0x3e,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x24,0x00,0x00,0x25,0x23,0x26,0x2a,0x40,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};


/* SFILE 関連関数 */

static SFILE *
sopen(SFILE *sf, char *string, signed int maxsize, char *md)
{
  char *st;
  strcpy(sf->mode,md);
  if (strstr(sf->mode,"newstr"))
  {
      if(maxsize <= sizeof(sfile_buffer))
	  st = sfile_buffer;
      else
	  st = (char *)malloc(maxsize);
  }
  else
    st=string;
  sf->pointer=sf->head=(unsigned char *)st;
  if (strstr(sf->mode,"stdout"))
    sstdout=sf;
  else if (strstr(sf->mode,"stdin"))
  {
    sstdin=sf;
    maxsize=strlen((char *)st);
  }
  sf->tail=sf->head+maxsize;
  return sf;
}

static void
sclose(SFILE *sf)
{
  if (sf==NULL)
    return;
  if (strstr(sf->mode,"stdout"))
      sstdout=NULL;
  if (strstr(sf->mode,"stdin"))
      sstdin=NULL;
  if (strstr(sf->mode,"newstr") && sf->head != (unsigned char *)sfile_buffer)
      free(sf->head);
}

static int
sgetc(SFILE *sf)
{
  if (sf==NULL)
    return SEOF;
  if(sf->pointer<sf->tail)
      return (int)*sf->pointer++;
  return SEOF;
}

static int
sungetc(int c, SFILE *sf)
{
  if (sf==NULL)
    return SEOF;
  if (sf->head<sf->pointer) {
    *--sf->pointer=(unsigned char)c;
    return c;
  } else
    return SEOF;
}

static int
sputc(int c, SFILE *sf)
{
  if (sf==NULL)
    return SEOF;
  if (sf->pointer<sf->tail)
      return (int)(*sf->pointer++=(unsigned char)c);
  return SEOF;
}

/* public 関数 start */

/* nkf 漢字コンバート関数 */
/* si must be terminated with '\0' */
char *
nkf_convert(char *si, char *so, int maxsize, char *in_mode, char *out_mode)
{
/* 前処理 */
  SFILE *fi,*fo;
  SFILE xfi,xfo;
  int a;

  if(maxsize == -1)
    maxsize = SAFE_CONVERT_LENGTH(strlen(si));
  else if(maxsize == 0)
    return si;

  fi = &xfi;
  fo = &xfo;
  if (so!=NULL) {
    sopen(fi,si,0,"stdin");
    sopen(fo,so,maxsize,"stdout");
  } else {
    sopen(fi,si,0,"stdin");
    sopen(fo,so,maxsize,"newstr stdout");
  }

/* 変数をデフォルト設定 */
  unbuf_f = FALSE;
  estab_f = FALSE;
  rot_f = FALSE;	/* rot14/43 mode */
  input_f = FALSE;	/* non fixed input code	 */
  alpha_f = FALSE;	/* convert JIx0208 alphbet to ASCII */
  mime_f = FALSE;	/* convert MIME base64 */
  broken_f = FALSE;	/* convert ESC-less broken JIS */
  iso8859_f = FALSE;	/* ISO8859 through */
#ifdef MSDOS
  x0201_f = TRUE;	/* Assume JISX0201 kana */
#else
  x0201_f = NO_X0201;	/* Assume NO JISX0201 */
#endif
  line = 0;	/* chars in line */
  prev = 0;
  fold_f  = FALSE;
  fold_len  = 0;
  kanji_intro = DEFAULT_J;
  ascii_intro = DEFAULT_R;
  output_mode = ASCII;	/* output kanji mode */
  input_mode = ASCII;	/* input kanji mode */
  shift_mode = FALSE;	/* TRUE shift out, or X0201  */
  mime_mode = FALSE;	/* MIME mode B base64, Q hex */
  
#if	0
/* No X0201->X0208 conversion 半角カナを有効に*/
  x0201_f = FALSE;
#else
/* 半角カナを全角にする */
  x0201_f = TRUE;
#endif

/* オプション mode 解析 */
  oconv=e_oconv;
  if (strstr(out_mode,"EUCK")||strstr(out_mode,"euck")||strstr(out_mode,"ujisk")){
    /*Hankaku Enable (For WRD File )*/
    oconv=e_oconv; 
    /* No X0201->X0208 conversion 半角カナを有効に*/
    x0201_f = FALSE;
  }
  else if (strstr(out_mode,"SJISK")||strstr(out_mode,"sjisk")){
    /*Hankaku Enable (For WRD File )*/
    oconv=s_oconv; 
    /* No X0201->X0208 conversion 半角カナを有効に*/
    x0201_f = FALSE;
  }
  else if (strstr(out_mode,"JISK")||strstr(out_mode,"jisk")){
    /*Hankaku Enable (For WRD File )*/
    oconv=j_oconv; 
    /* No X0201->X0208 conversion 半角カナを有効に*/
    x0201_f = FALSE;
  }
  else if (strstr(out_mode,"EUC")||strstr(out_mode,"euc")||strstr(out_mode,"ujis"))
    oconv=e_oconv;
  else if (strstr(out_mode,"SJIS")||strstr(out_mode,"sjis"))
    oconv=s_oconv;
  else if (strstr(out_mode,"JIS")||strstr(out_mode,"jis"))
    oconv=j_oconv;
  /* 読み込みコードのチェック */
  input_f = -1;
  if(in_mode != NULL)
  {
      if(strstr(in_mode,"EUC")||strstr(in_mode,"euc")||strstr(in_mode,"ujis"))
	  input_f = JIS_INPUT;
      else if (strstr(in_mode,"SJIS")||strstr(in_mode,"sjis"))
	  input_f = SJIS_INPUT;
      else if (strstr(in_mode,"JIS")||strstr(in_mode,"jis"))
	  input_f = JIS_INPUT;
  }
  if(input_f == -1)
  {
      /* Auto detect */
      input_f = check_kanji_code((unsigned char *)si);
      if(input_f == -1)
	  input_f = SJIS_INPUT;
      else if(input_f == EUC_INPUT)
	  input_f = JIS_INPUT;
      if(input_f == SJIS_INPUT && x0201_f == NO_X0201)
	  x0201_f = TRUE;
  }


  /* コンバート */
  convert(fi);

/* 後処理 */
  sputchar('\0');
  if (so==NULL) {
    /* Copy `fo' buffer to `si' */

    a = fo->pointer - fo->head; /* Stored length */
    if(a > maxsize)
	a = maxsize;
    memcpy(si, fo->head, a); /* Do copy */
    so = si;
  }
  sclose(fi);
  sclose(fo);
  return so;
}

char *
nkf_conv(char *si, char *so, char *mode)
{
  return nkf_convert(si,so,-1,NULL,mode);
}

/* public 関数 end */

#define IS_SJIS_HANKAKU(c)	(0xa0 <= (c) && (c) <= 0xdf)
#define IS_SJIS_BYTE1(c)	((0x81 <= (c) && (c) <= 0x9f) ||\
				 (0xe0 <= (c) && (c) <= 0xfc))
#define IS_SJIS_BYTE2(c)	((0x40 <= (c) && (c) <= 0x7e) ||\
				 (0x80 <= (c) && (c) <= 0xfc))

#define IS_EUC_BYTE1(c)		(0xa1 <= (c) && (c) <= 0xf4)
#ifdef EUC_STRICT_CHECK
#define IS_EUC_BYTE2(c)		(0xa1 <= (c) && (c) <= 0xfe)
#else
#define IS_EUC_BYTE2(c)		(0xa0 <= (c) && (c) <= 0xff)
#endif /* EUC_STRICT_CHECK */


#ifdef EUC_STRICT_CHECK
#define EUC_GAP_LIST_SIZE (16*2)
static unsigned int euc_gap_list[EUC_GAP_LIST_SIZE] =
{
    0xa2af, 0xa2b9,
    0xa2c2, 0xa2c9,
    0xa2d1, 0xa2db,
    0xa2eb, 0xa2f1,
    0xa2fa, 0xa2fd,
    0xa3a1, 0xa3af,
    0xa3ba, 0xa3c0,
    0xa3db, 0xa3e0,
    0xa3fb, 0xa3fe,
    0xa4f4, 0xa4fe,
    0xa5f7, 0xa5fe,
    0xa6b9, 0xa6c0,
    0xa6d9, 0xa6fe,
    0xa7c2, 0xa7d0,
    0xa7f2, 0xa7fe,
    0xa8c1, 0xaffe
};
#endif /* EUC_STRICT_CHECK */

static int check_kanji_code(unsigned char *p)
{
    int c1, c2, mode;
    int noteuc;

    /* check JIS or ASCII code */
    mode = ASCII;
    while(*p)
    {
	if(*p < SP || *p >= DEL)
	{
	    if(*p == ESC)
		return JIS_INPUT;
	    mode = -1; /* None ASCII */
	    break;
	}
	p++;
    }
    if(mode == ASCII)
	return ASCII;

    /* EUC or SJIS */
    noteuc = 0;
    while(*p)
    {
        /* skip ASCII */
        while(*p && *p <= DEL)
	    p++;

	if(!*p)
	    return -1;
	c1 = p[0];
	c2 = p[1];
	if(c2 == 0)
	{
	    if(IS_SJIS_HANKAKU(c1))
		return SJIS_INPUT;
	    return -1;
	}

	if(IS_SJIS_HANKAKU(c1))
	{
#ifdef EUC_STRICT_CHECK
	    unsigned int c;
#endif /* EUC_STRICT_CHECK */
/*
            0xa0   0xa1              0xdf   0xf4   0xfe
             |<-----+---- SH -------->|      |      |     SH: SJIS-HANKAKU
                    |<------- E1 ----------->|      |     E1: EUC (MSB)
                    |<--------E2------------------->|     E2: EUC (LSB)
*/
	    if(!IS_EUC_BYTE1(c1) || !IS_EUC_BYTE2(c2))
		return SJIS_INPUT;
	    if(!IS_SJIS_HANKAKU(c2)) /* (0xdf..0xfe] */
		return EUC_INPUT;
#ifdef EUC_STRICT_CHECK
	    if(!noteuc)
	    {
		int i;
		/* Checking more strictly */
		c = (((unsigned int)c1)<<8 | (unsigned int)c2);
		for(i = 0; i < EUC_GAP_LIST_SIZE; i += 2)
		    if(euc_gap_list[i] <= c && c <= euc_gap_list[i + 1])
		    {
			noteuc = 1;
			break;
		    }
	    }
#endif /* EUC_STRICT_CHECK */
	    p += 2;
	}
	else if(IS_SJIS_BYTE1(c1) && IS_SJIS_BYTE2(c2))
	{
	    if(!(IS_EUC_BYTE1(c1) && IS_EUC_BYTE2(c2)))
		return SJIS_INPUT;
	    p += 2;
	}
	else if(IS_EUC_BYTE1(c1) && IS_EUC_BYTE2(c2))
	{
	    return EUC_INPUT;
	}
	else
	    p++; /* What?  Is this japanese?  Try check again. */
    }
    if(noteuc)
	return SJIS_INPUT;
    return -1;
}

#ifdef EUC_STRICT_CHECK
static void fix_euc_code(unsigned char *s, int len)
{
    int i, j, c;

    for(i = 0; i < len - 1; i++)
    {
	if(s[i] & 0x80)
	{
	    c = (((unsigned int)s[i])<<8 | (unsigned int)s[i + 1]);
	    for(j = 0; j < EUC_GAP_LIST_SIZE; j += 2)
		if(euc_gap_list[j] <= c && c <= euc_gap_list[j + 1])
		{
		    s[i] = 0xa1;
		    s[i + 1] = 0xa1;
		    break;
		}
	    i++;
	}
    }
}
#endif /* EUC_STRICT_CHECK */

static int
convert(SFILE *f)
{
    register int    c1,
		    c2;

    c2 = 0;

    if (input_f == JIS_INPUT || input_f == LATIN1_INPUT) {
	estab_f = TRUE; iconv = oconv;
    } else if (input_f == SJIS_INPUT) {
	estab_f = TRUE;	 iconv = s_iconv;
    } else {
	estab_f = FALSE; iconv = oconv;
    }
    input_mode = ASCII;
    output_mode = ASCII;
    shift_mode = FALSE;
    mime_mode = FALSE;

#define NEXT continue	/* no output, get next */
#define SEND  ;		/* output c1 and c2, get next */
#define LAST break	/* end of loop, go closing  */

    while ((c1 = GETC (f)) != SEOF) {
	if (!c2 && !input_mode && c1<DEL && !mime_mode && !output_mode
		&& !shift_mode && !fold_f && !rot_f) {
	    /* plain ASCII loop, no conversion and no fold  */
	    while(c1!='=' && c1!=SO && c1!=SEOF &&
		    c1!=ESC && c1!='$' && c1<DEL) {
		sputchar(c1);
		c1 = sgetc(f);
	    }
	    if(c1==SEOF) LAST;
	}
	if (c2) {
	    /* second byte */
	    if (c2 > DEL) {
		/* in case of 8th bit is on */
		if (!estab_f) {
		    /* in case of not established yet */
		    if (c1 > SSP) {
			/* It is still ambiguious */
			h_conv (f, c2, c1);
			c2 = 0;
			NEXT;
		    } else if (c1 < AT) {
			/* ignore bogus code */
			c2 = 0;
			NEXT;
		    } else {
			/* established */
			/* it seems to be MS Kanji */
			estab_f = TRUE;
			iconv = s_iconv;
			SEND;
		    }
		} else
		    /* in case of already established */
		    if (c1 < AT) {
			/* ignore bogus code */
			c2 = 0;
			NEXT;
		    } else
			SEND;
	    } else
		/* 7 bit code */
		/* it might be kanji shitfted */
		if ((c1 == DEL) || (c1 <= SP)) {
		    /* ignore bogus first code */
		    c2 = 0;
		    NEXT;
		} else
		    SEND;
	} else {
	    /* first byte */
	    if (c1 > DEL) {
		/* 8 bit code */
		if (!estab_f && !iso8859_f) {
		    /* not established yet */
		    if (c1 < SSP) {
			/* it seems to be MS Kanji */
			estab_f = TRUE;
			iconv = s_iconv;
		    } else if (c1 < 0xe0) {
			/* it seems to be EUC */
			estab_f = TRUE;
			iconv = oconv;
		    } else {
			/* still ambiguious */
		    }
		    c2 = c1;
		    NEXT;
		} else { /* estab_f==TRUE */
		    if(iso8859_f) {
			SEND;
		    } else if(SSP<=c1 && c1<0xe0 && iconv == s_iconv) {
			/* SJIS X0201 Case... */
			/* This is too arrogant, but ... */
			if(x0201_f==NO_X0201) {
			    iconv = oconv;
			    c2 = c1;
			    NEXT;
			} else
			if(x0201_f) {
			    if(dv[(c1-SSP)*2]||ev[(c1-SSP)*2]) {
			    /* look ahead for X0201/X0208conversion */
				if ((c2 = GETC (f)) == SEOF) {
				    (*oconv)(cv[(c1-SSP)*2],cv[(c1-SSP)*2+1]);
				    LAST;
				} else if (c2==(0xde)) { /* 濁点 */
				    (*oconv)(dv[(c1-SSP)*2],dv[(c1-SSP)*2+1]);
				    c2=0;
				    NEXT;
				} else if (c2==(0xdf)&&ev[(c1-SSP)*2]) {
				    /* 半濁点 */
				    (*oconv)(ev[(c1-SSP)*2],ev[(c1-SSP)*2+1]);
				    c2=0;
				    NEXT;
				}
				UNGETC(c2,f); c2 = 0;
			    }
			    (*oconv)(cv[(c1-SSP)*2],cv[(c1-SSP)*2+1]);
			    NEXT;
			} else
			    SEND;
		    } else if(c1==SSO && iconv != s_iconv) {
			/* EUC X0201 Case */
			/* This is too arrogant
			if(x0201_f == NO_X0201) {
			    estab_f = FALSE;
			    c2 = 0;
			    NEXT;
			} */
			c1 = GETC (f);	/* skip SSO */
			euc_1byte_check:
			if(x0201_f && SSP<=c1 && c1<0xe0) {
			    if(dv[(c1-SSP)*2]||ev[(c1-SSP)*2]) {
				if ((c2 = GETC (f)) == SEOF) {
				    (*oconv)(cv[(c1-SSP)*2],cv[(c1-SSP)*2+1]);
				    LAST;
				}
				/* forward lookup 濁点/半濁点 */
				if (c2 != SSO) {
				    UNGETC(c2,f); c2 = 0;
				    (*oconv)(cv[(c1-SSP)*2],cv[(c1-SSP)*2+1]);
				    NEXT;
				} else if ((c2 = GETC (f)) == SEOF) {
				    (*oconv)(cv[(c1-SSP)*2],cv[(c1-SSP)*2+1]);
				    (*oconv)(0,SSO);
				    LAST;
				} else if (c2==(0xde)) { /* 濁点 */
				    (*oconv)(dv[(c1-SSP)*2],dv[(c1-SSP)*2+1]);
				    c2=0;
				    NEXT;
				} else if (c2==(0xdf)&&ev[(c1-SSP)*2]) {
				    /* 半濁点 */
				    (*oconv)(ev[(c1-SSP)*2],ev[(c1-SSP)*2+1]);
				    c2=0;
				    NEXT;
				} else {
				    (*oconv)(cv[(c1-SSP)*2],cv[(c1-SSP)*2+1]);
				    /* we have to check this c2 */
				    /* and no way to push back SSO */
				    c1 = c2; c2 = 0;
				    goto euc_1byte_check;
				}
			    }
			    (*oconv)(cv[(c1-SSP)*2],cv[(c1-SSP)*2+1]);
			    NEXT;
			} else
			    SEND;
		    } else if(c1 < SSP && iconv != s_iconv) {
			/* strange code in EUC */
			iconv = s_iconv;  /* try SJIS */
			c2 = c1;
			NEXT;
		    } else {
		       /* already established */
		       c2 = c1;
		       NEXT;
		    }
		}
	    } else if ((c1 > SP) && (c1 != DEL)) {
		/* in case of Roman characters */
		if (shift_mode) {
		    c1 |= 0x80;
		    /* output 1 shifted byte */
		    if(x0201_f && (!iso8859_f||input_mode==X0201) &&
			    SSP<=c1 && c1<0xe0 ) {
			if(dv[(c1-SSP)*2]||ev[(c1-SSP)*2]) {
			    if ((c2 = GETC (f)) == SEOF) {
				(*oconv)(cv[(c1-SSP)*2],cv[(c1-SSP)*2+1]);
				LAST;
			    } else if (c2==(0xde&0x7f)) { /* 濁点 */
				(*oconv)(dv[(c1-SSP)*2],dv[(c1-SSP)*2+1]);
				c2=0;
				NEXT;
			    } else if (c2==(0xdf&0x7f)&&ev[(c1-SSP)*2]) {
				/* 半濁点 */
				(*oconv)(ev[(c1-SSP)*2],ev[(c1-SSP)*2+1]);
				c2=0;
				NEXT;
			    }
			    UNGETC(c2,f); c2 = 0;
			}
			(*oconv)(cv[(c1-SSP)*2],cv[(c1-SSP)*2+1]);
			NEXT;
		    } else
			SEND;
		} else if(c1 == '(' && broken_f && input_mode == X0208
			&& !mime_mode ) {
		    /* Try to recover missing escape */
		    if ((c1 = GETC (f)) == SEOF) {
			(*oconv) (0, '(');
			LAST;
		    } else {
			if (c1 == 'B' || c1 == 'J' || c1 == 'H') {
			    input_mode = ASCII; shift_mode = FALSE;
			    NEXT;
			} else {
			    (*oconv) (0, '(');
			    /* do not modify various input_mode */
			    /* It can be vt100 sequence */
			    SEND;
			}
		    }
		} else if (input_mode == X0208) {
		    /* in case of Kanji shifted */
		    c2 = c1;
		    NEXT;
		    /* goto next_byte */
		} else if(mime_f && !mime_mode && c1 == '=') {
		    if ((c1 = sgetc (f)) == SEOF) {
			(*oconv) (0, '=');
			LAST;
		    } else if (c1 == '?') {
			/* =? is mime conversion start sequence */
			if(mime_begin(f) == SEOF) /* check in detail */
			    LAST;
			else
			    NEXT;
		    } else {
			(*oconv) (0, '=');
			sungetc(c1,f);
			NEXT;
		    }
		} else if(c1 == '$' && broken_f && !mime_mode) {
		    /* try to recover missing escape */
		    if ((c1 = GETC (f)) == SEOF) {
			(*oconv) (0, '$');
			LAST;
		    } else if(c1 == '@'|| c1 == 'B') {
			/* in case of Kanji in ESC sequence */
			input_mode = X0208;
			shift_mode = FALSE;
			NEXT;
		    } else {
			/* sorry */
			(*oconv) (0, '$');
			(*oconv) (0, c1);
			NEXT;
		    }
		} else
		    SEND;
	    } else if (c1 == SI) {
		shift_mode = FALSE;
		NEXT;
	    } else if (c1 == SO) {
		shift_mode = TRUE;
		NEXT;
	    } else if (c1 == ESC ) {
		if ((c1 = GETC (f)) == SEOF) {
		    (*oconv) (0, ESC);
		    LAST;
		} else if (c1 == '$') {
		    if ((c1 = GETC (f)) == SEOF) {
			(*oconv) (0, ESC);
			(*oconv) (0, '$');
			LAST;
		    } else if(c1 == '@'|| c1 == 'B') {
			/* This is kanji introduction */
			input_mode = X0208;
			shift_mode = FALSE;
			NEXT;
		    } else {
			(*oconv) (0, ESC);
			(*oconv) (0, '$');
			(*oconv) (0, c1);
			NEXT;
		    }
		} else if (c1 == '(') {
		    if ((c1 = GETC (f)) == SEOF) {
			(*oconv) (0, ESC);
			(*oconv) (0, '(');
			LAST;
		    } else {
			if (c1 == 'I') {
			    /* This is X0201 kana introduction */
			    input_mode = X0201; shift_mode = X0201;
			    NEXT;
			} else if (c1 == 'B' || c1 == 'J' || c1 == 'H') {
			    /* This is X0208 kanji introduction */
			    input_mode = ASCII; shift_mode = FALSE;
			    NEXT;
			} else {
			    (*oconv) (0, ESC);
			    (*oconv) (0, '(');
			    /* maintain various input_mode here */
			    SEND;
			}
		    }
		} else {
		    /* lonely ESC  */
		    (*oconv) (0, ESC);
		    SEND;
		}
	    } else
		SEND;
	}
	/* send: */
	if (input_mode == X0208)
	    (*oconv) (c2, c1);	/* this is JIS, not SJIS/EUC case */
	else
	    (*iconv) (c2, c1);	/* can be EUC/SJIS */
	c2 = 0;
	continue;
	/* goto next_word */
    }

    /* epilogue */
    (*iconv) (SEOF, 0);

#ifdef EUC_STRICT_CHECK
    if(oconv == e_oconv)
	fix_euc_code(sstdout->pointer, sstdout->tail - sstdout->head);
#endif /* EUC_STRICT_CHECK */
    return 1;
}

static int
h_conv(SFILE *f, int c2, int c1)
{
    register int    wc;


    /** it must NOT be in the kanji shifte sequence	 */
    /** it must NOT be written in JIS7			 */
    /** and it must be after 2 byte 8bit code		 */

    hold_count = 0;
    push_hold_buf (c2, c1);
    c2 = 0;

    while ((c1 = GETC (f)) != SEOF) {
	if (c2) {
	    /* second byte */
	    if (!estab_f) {
		/* not established */
		if (c1 > SSP) {
		    /* it is still ambiguious yet */
		    SEND;
		} else if (c1 < AT) {
		    /* ignore bogus first byte */
		    c2 = 0;
		    SEND;
		} else {
		    /* now established */
		    /* it seems to be MS Kanji */
		    estab_f = TRUE;
		    iconv = s_iconv;
		    SEND;
		}
	    } else
		SEND;
	} else {
	    /* First byte */
	    if (c1 > DEL) {
		/* 8th bit is on */
		if (c1 < SSP) {
		    /* it seems to be MS Kanji */
		    estab_f = TRUE;
		    iconv = s_iconv;
		} else if (c1 < 0xe0) {
		    /* it seems to be EUC */
		    estab_f = TRUE;
		    iconv = oconv;
		} else {
		    /* still ambiguious */
		}
		c2 = c1;
		NEXT;
	    } else
	    /* 7 bit code , then send without any process */
		SEND;
	}
	/* send: */
	if ((push_hold_buf (c2, c1) == SEOF) || estab_f)
	    break;
	c2 = 0;
	continue;
    }

    /** now,
     ** 1) EOF is detected, or
     ** 2) Code is established, or
     ** 3) Buffer is FULL (but last word is pushed)
     **
     ** in 1) and 3) cases, we continue to use
     ** Kanji codes by oconv and leave estab_f unchanged.
     **/

    for (wc = 0; wc < hold_count; wc += 2) {
	c2 = hold_buf[wc];
	c1 = hold_buf[wc+1];
	(*iconv) (c2, c1);
    }
    return VOIDVOID ;
}

static int
push_hold_buf(int c2, int c1)
{
    if (hold_count >= HOLD_SIZE*2)
	return (SEOF);
    hold_buf[hold_count++] = c2;
    hold_buf[hold_count++] = c1;
    return ((hold_count >= HOLD_SIZE*2) ? SEOF : hold_count);
}


static int
s_iconv(int c2, int c1)
{
    if ((c2 == SEOF) || (c2 == 0)) {
	/* NOP */
    } else {
	c2 = c2 + c2 - ((c2 <= 0x9f) ? SJ0162 : SJ6394);
	if (c1 < 0x9f)
	    c1 = c1 - ((c1 > DEL) ? SP : 0x1f);
	else {
	    c1 = c1 - 0x7e;
	    c2++;
	}
    }
    (*oconv) (c2, c1);
    return 1;
}

static int
e_oconv(int c2, int c1)
{
    c2 = pre_convert(c1,c2); c1 = c1_return;
    if(fold_f) {
	switch(fold(c2,c1)) {
	    case '\n': sputchar('\n');
		break;
	    case 0:    return VOIDVOID;
	    case '\r':
		c1 = '\n'; c2 = 0;
		break;
	    case '\t':
	    case ' ':
		c1 = ' '; c2 = 0;
		break;
	}
    }
    if (c2 == SEOF)
	return VOIDVOID;
    else if (c2 == 0 && (c1&0x80)) {
	sputchar(SSO); sputchar(c1);
    } else if (c2 == 0) {
	sputchar(c1);
    } else {
	if((c1<0x20 || 0x7e<c1) ||
	   (c2<0x20 || 0x7e<c2)) {
	    estab_f = FALSE;
	    return VOIDVOID; /* too late to rescue this char */
	}
	sputchar(c2 | 0x080);
	sputchar(c1 | 0x080);
    }
    return VOIDVOID;
}

static int
s_oconv(int c2, int c1)
{
    c2 = pre_convert(c1,c2); c1 = c1_return;
    if(fold_f) {
	switch(fold(c2,c1)) {
	    case '\n': sputchar('\n');
		break;
	    case '\r':
		c1 = '\n'; c2 = 0;
		break;
	    case 0:    return VOIDVOID;
	    case '\t':
	    case ' ':
		c1 = ' '; c2 = 0;
		break;
	}
    }
    if (c2 == SEOF)
	return VOIDVOID;
    else if (c2 == 0) {
	sputchar(c1);
    } else {
	if((c1<0x20 || 0x7e<c1) ||
	   (c2<0x20 || 0x7e<c2)) {
	    estab_f = FALSE;
	    return VOIDVOID; /* too late to rescue this char */
	}
	sputchar((((c2 - 1) >> 1) + ((c2 <= 0x5e) ? 0x71 : 0xb1)));
	sputchar((c1 + ((c2 & 1) ? ((c1 < 0x60) ? 0x1f : 0x20) : 0x7e)));
    }
    return VOIDVOID;
}

static int
j_oconv(int c2, int c1)
{
    c2 = pre_convert(c1,c2); c1 = c1_return;
    if(fold_f) {
	switch(fold(c2,c1)) {
	    case '\n':
		if (output_mode) {
		    sputchar(ESC);
		    sputchar('(');
		    sputchar(ascii_intro);
		}
		sputchar('\n');
		output_mode = ASCII;
		break;
	    case '\r':
		c1 = '\n'; c2 = 0;
		break;
	    case '\t':
	    case ' ':
		c1 = ' '; c2 = 0;
		break;
	    case 0:    return VOIDVOID;
	}
     }
    if (c2 == SEOF) {
	if (output_mode) {
	    sputchar(ESC);
	    sputchar('(');
	    sputchar(ascii_intro);
	}
    } else if (c2 == 0 && (c1 & 0x80)) {
	if(input_mode==X0201 || !iso8859_f) {
	    if(output_mode!=X0201) {
		sputchar(ESC);
		sputchar('(');
		sputchar('I');
		output_mode = X0201;
	    }
	    c1 &= 0x7f;
	} else {
	    /* iso8859 introduction, or 8th bit on */
	    /* Can we convert in 7bit form using ESC-'-'-A ?
	       Is this popular? */
	}
	sputchar(c1);
    } else if (c2 == 0) {
	if (output_mode) {
	    sputchar(ESC);
	    sputchar('(');
	    sputchar(ascii_intro);
	    output_mode = ASCII;
	}
	sputchar(c1);
    } else {
	if (output_mode != X0208) {
	    sputchar(ESC);
	    sputchar('$');
	    sputchar(kanji_intro);
	    output_mode = X0208;
	}
	if(c1<0x20 || 0x7e<c1) return VOIDVOID;
	if(c2<0x20 || 0x7e<c2) return VOIDVOID;
	sputchar(c2);
	sputchar(c1);
    }
    return VOIDVOID;
}

#define rot13(c)  ( \
      ( c < 'A' ) ? c: \
      (c <= 'M')  ? (c + 13): \
      (c <= 'Z')  ? (c - 13): \
      (c < 'a')	  ? (c): \
      (c <= 'm')  ? (c + 13): \
      (c <= 'z')  ? (c - 13): \
      (c) \
)

#define	 rot47(c) ( \
      ( c < '!' ) ? c: \
      ( c <= 'O' ) ? (c + 47) : \
      ( c <= '~' ) ?  (c - 47) : \
      c \
)


/*
  Return value of fold()

       \n  add newline	and output char
       \r  add newline	and output nothing
       ' ' space
       0   skip
       1   (or else) normal output

  fold state in prev (previous character)

      >0x80 Japanese (X0208/X0201)
      <0x80 ASCII
      \n    new line
      ' '   space

  This fold algorthm does not conserve heading space in a line.
  This is the main difference from fmt.
*/

static int
fold(int c2, int c1)
{
    int prev0;
    if(c1=='\r')
	return 0;		/* ignore cr */
    if(c1== 8) {
	if(line>0) line--;
	return 1;
    }
    if(c2==SEOF && line != 0)	 /* close open last line */
	return '\n';
    /* new line */
    if(c1=='\n') {
	if(prev == c1) {	/* duplicate newline */
	    if(line) {
		line = 0;
		return '\n';	/* output two newline */
	    } else {
		line = 0;
		return 1;
	    }
	} else	{
	    if(prev&0x80) {	/* Japanese? */
		prev = c1;
		return 0;	/* ignore given single newline */
	    } else if(prev==' ') {
		return 0;
	    } else {
		prev = c1;
		if(++line<=fold_len)
		    return ' ';
		else {
		    line = 0;
		    return '\r';	/* fold and output nothing */
		}
	    }
	}
    }
    if(c1=='\f') {
	prev = '\n';
	if(line==0)
	    return 1;
	line = 0;
	return '\n';		/* output newline and clear */
    }
    /* X0208 kankaku or ascii space */
    if((c2==0&&c1==' ')||
	(c2==0&&c1=='\t')||
	(c2=='!'&& c1=='!')) {
	if(prev == ' ') {
	    return 0;		/* remove duplicate spaces */
	}
	prev = ' ';
	if(++line<=fold_len)
	    return ' ';		/* output ASCII space only */
	else {
	    prev = ' '; line = 0;
	    return '\r';	/* fold and output nothing */
	}
    }
    prev0 = prev; /* we still need this one... , but almost done */
    prev = c1;
    if (c2 || (SSP<=c1 && c1<=0xdf))
	prev |= 0x80;  /* this is Japanese */
    line += (c2==0)?1:2;
    if(line<=fold_len) {   /* normal case */
	return 1;
    }
    if(line>=fold_len+FOLD_MARGIN) { /* too many kinsou suspension */
	line = (c2==0)?1:2;
	return '\n';	   /* We can't wait, do fold now */
    }
    /* simple kinsoku rules  return 1 means no folding	*/
    if(c2==0) {
	if(c1==0xde) return 1; /* ゛*/
	if(c1==0xdf) return 1; /* ゜*/
	if(c1==0xa4) return 1; /* 。*/
	if(c1==0xa3) return 1; /* ，*/
	if(c1==0xa1) return 1; /* 」*/
	if(c1==0xb0) return 1; /* - */
	if(SSP<=c1 && c1<=0xdf) {		/* X0201 */
	    line = 1;
	    return '\n';/* add one new line before this character */
	}
	/* fold point in ASCII { [ ( */
	if(( c1!=')'&&
	     c1!=']'&&
	     c1!='}'&&
	     c1!='.'&&
	     c1!=','&&
	     c1!='!'&&
	     c1!='?'&&
	     c1!='/'&&
	     c1!=':'&&
	     c1!=';')&&
	    ((prev0=='\n')|| (prev0==' ')||	/* ignored new line */
	    (prev0&0x80))			/* X0208 - ASCII */
	    ) {
	    line = 1;
	    return '\n';/* add one new line before this character */
	}
	return 1;  /* default no fold in ASCII */
    } else {
	if(c2=='!') {
	    if(c1=='"')	 return 1; /* 、 */
	    if(c1=='#')	 return 1; /* 。 */
	    if(c1=='$')	 return 1; /* ， */
	    if(c1=='%')	 return 1; /* ． */
	    if(c1=='\'') return 1; /* ＋ */
	    if(c1=='(')	 return 1; /* ； */
	    if(c1==')')	 return 1; /* ？ */
	    if(c1=='*')	 return 1; /* ！ */
	    if(c1=='+')	 return 1; /* ゛ */
	    if(c1==',')	 return 1; /* ゜ */
	}
	line = 2;
	return '\n'; /* add one new line before this character */
    }
}

static int
pre_convert(int c1, int c2)
{
	if(c2) c1 &= 0x7f;
	c1_return = c1;
	if(c2==SEOF) return c2;
	c2 &= 0x7f;
	if(rot_f) {
	    if(c2) {
		c1 = rot47(c1);
		c2 = rot47(c2);
	    } else {
		if (!(c1 & 0x80))
		    c1 = rot13(c1);
	    }
	    c1_return = c1;
	}
	/* JISX0208 Alphabet */
	if (alpha_f && c2 == 0x23 ) return 0;
	/* JISX0208 Kigou */
	if (alpha_f && c2 == 0x21 ) {
	   if(0x20<c1 && c1<0x7f && fv[c1-0x20]) {
	       c1_return = fv[c1-0x20];
	       return 0;
	   }
	}
	return c2;
}



/* This ONLY converts  =?ISO-2022-JP?B?HOGEHOGE?= to adequate form */
/* Quickly programmed by I.Ichikawa (( )) 1994, Dec. */
/* You may freely use this, unless you pretend you build this */

static unsigned char *mime_pattern[] = {
   (unsigned char *)"=?ISO-8859-1?Q?",
   (unsigned char *)"=?ISO-2022-JP?B?",
   NULL
};

static int mime_encode[] = {
    'Q',
    'B',
    0
};

static int iso8859_f_save;

static int
mime_begin(SFILE *f)
{
    int c1=0;
    int i,j,k;
    unsigned char *p,*q;

    mime_mode = FALSE;
    /* =? has been checked */
    j = 0;
    p = mime_pattern[j];

    for(i=2;p[i]>' ';i++) {
	if( ((c1 = sgetc(f))==SEOF) || c1 != p[i] ) {
	    q = p;
/*	    while (p = mime_pattern[++j]) { */
	    while ((p = mime_pattern[++j])!=NULL) {
		for(k=2;k<i;k++) /* assume length(p) > i */
		    if(p[k]!=q[k]) break; if(k==i && c1==p[k]) break; }
		    if(p) continue; sungetc(c1,f); for(j=0;j<i;j++) {
		    (*oconv)(0,q[j]); } return c1; } } iso8859_f_save
		    = iso8859_f; if(j==0) { iso8859_f = TRUE; }
		    mime_mode = mime_encode[j]; return c1; }

static int
mime_getc(SFILE *f)
{
    int c1, c2, c3, c4, cc;
    int t1, t2, t3, t4, mode;

    if(mime_top != mime_last) {	 /* Something is in FIFO */
	return	Fifo(mime_top++);
    }

    if(mime_mode == 'Q') {
	if ((c1 = sgetc(f)) == SEOF) return (SEOF);
	if(c1!='=' && c1!='?') return c1;
	if ((c2 = sgetc(f)) == SEOF) return (SEOF);
	if(c1=='?'&&c2=='=') {
	    /* end Q encoding */
	    iso8859_f = iso8859_f_save;
	    mime_mode = 0; return sgetc(f);
	}
	if ((c3 = sgetc(f)) == SEOF) return (SEOF);
#define hex(c)	 (('0'<=c&&c<='9')?(c-'0'):\
     ('A'<=c&&c<='F')?(c-'A'+10):('a'<=c&&c<='f')?(c-'a'+10):0)
	return ((hex(c2)<<4) + hex(c3));
    }

    if(mime_mode != 'B')
	return sgetc(f);


    /* Base64 encoding */
    /*
	MIME allows line break in the middle of
	Base64, but we are very pessimistic in decoding
	in unbuf mode because MIME encoded code may broken by
	less or editor's control sequence (such as ESC-[-K in unbuffered
	mode. ignore incomplete MIME.
    */
    mode = mime_mode;
    mime_mode = FALSE;	/* prepare for quit */

    while ((c1 = sgetc(f))<=' ') {
	if(c1==SEOF)
	    return (SEOF);
	if(unbuf_f) {
	    input_mode = ASCII;	 return c1;
	}
    }
    while ((c2 = sgetc(f))<=' ') {
	if(c2==SEOF)
	    return (SEOF);
	if(unbuf_f) {
	    input_mode = ASCII;	 return c2;
	}
    }
    if ((c1 == '?') && (c2 == '=')) return sgetc(f);
    while ((c3 = sgetc(f))<=' ') {
	if(c3==SEOF)
	    return (SEOF);
	if(unbuf_f) {
	    input_mode = ASCII;	 return c3;
	}
    }
    while ((c4 = sgetc(f))<=' ') {
	if(c4==SEOF)
	    return (SEOF);
	if(unbuf_f) {
	    input_mode = ASCII;	 return c4;
	}
    }

    mime_mode = mode; /* still in MIME sigh... */

    /* BASE 64 decoding */

    t1 = 0x3f & base64decode(c1);
    t2 = 0x3f & base64decode(c2);
    t3 = 0x3f & base64decode(c3);
    t4 = 0x3f & base64decode(c4);
    cc = ((t1 << 2) & 0x0fc) | ((t2 >> 4) & 0x03);
    if (c2 != '=') {
	Fifo(mime_last++) = cc;
	cc = ((t2 << 4) & 0x0f0) | ((t3 >> 2) & 0x0f);
	if (c3 != '=') {
	    Fifo(mime_last++) = cc;
	    cc = ((t3 << 6) & 0x0c0) | (t4 & 0x3f);
	    if (c4 != '=')
		Fifo(mime_last++) = cc;
	}
    } else {
	return c1;
    }
    return  Fifo(mime_top++);
}

static int
mime_ungetc(unsigned int c)
{
    Fifo(mime_last++) = c;
    return c;
}

static int
base64decode(int c)
{
    int		    i;
    if (c > '@')
    {
	if (c < '[')
	    i = c - 'A';	/* A..Z 0-25 */
	else
	    i = c - 'G' /* - 'a' + 26 */ ;	/* a..z 26-51 */
    }
    else if (c > '/')
	i = c - '0' + '4' /* - '0' + 52 */ ;	/* 0..9 52-61 */
    else if (c == '+')
	i = '>' /* 62 */ ;	/* +  62 */
    else
	i = '?' /* 63 */ ;	/* / 63 */

    return (i);
}

/**
 ** パッチ制作者
 **  void@merope.pleiades.or.jp (Kusakabe Youichi)
 **  NIDE Naoyuki <nide@ics.nara-wu.ac.jp>
 **  ohta@src.ricoh.co.jp (Junn Ohta)
 **  inouet@strl.nhk.or.jp (Tomoyuki Inoue)
 **  kiri@pulser.win.or.jp (Tetsuaki Kiriyama)
 **  Kimihiko Sato <sato@sail.t.u-tokyo.ac.jp>
 **  a_kuroe@kuroe.aoba.yokohama.jp (Akihiko Kuroe)
 **  kono@csl.sony.co.jp (Shinji Kono)
 **
 ** 最終更新日
 **  1994.12.21
 **/

/* end */
#endif /* JAPANESE */
