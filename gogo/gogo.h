/* -*- TABSIZE = 4 -*- */
/*
 *	for GOGO-no-coda
 *	Copyright (C) 1999, 2000, 2001 gogo develop team
 */


#ifndef GOGO_H_
#define GOGO_H_

typedef int MERET;

/* for only compatibility */
#define MUPARAM UPARAM

typedef	unsigned long			UPARAM;

#define	ME_NOERR					(0)		// return normally;正常終了
#define	ME_EMPTYSTREAM				(1)		// stream becomes empty;ストリームが最後に達した
#define	ME_HALTED					(2)		// stopped by user;(ユーザーの手により)中断された
#define	ME_MOREDATA					(3)	
#define	ME_INTERNALERROR			(10)	// internal error; 内部エラー
#define	ME_PARAMERROR				(11)	// parameters error;設定でパラメーターエラー
#define	ME_NOFPU					(12)	// no FPU;FPUを装着していない!!
#define	ME_INFILE_NOFOUND			(13)	// can't open input file;入力ファイルを正しく開けない
#define	ME_OUTFILE_NOFOUND			(14)	// can't open output file;出力ファイルを正しく開けない
#define	ME_FREQERROR				(15)	// frequency is not good;入出力周波数が正しくない
#define	ME_BITRATEERROR				(16)	// bitrate is not good;出力ビットレートが正しくない
#define	ME_WAVETYPE_ERR				(17)	// WAV format is not good;ウェーブタイプが正しくない
#define	ME_CANNOT_SEEK				(18)	// can't seek;正しくシーク出来ない
#define	ME_BITRATE_ERR				(19)	// only for compatibility;ビットレート設定が正しくない
#define	ME_BADMODEORLAYER			(20)	// mode/layer not good;モード・レイヤの設定異常
#define	ME_NOMEMORY					(21)	// fail to allocate memory;メモリアローケーション失敗
#define	ME_CANNOT_SET_SCOPE			(22)	// thread error;スレッド属性エラー(pthread only)
#define	ME_CANNOT_CREATE_THREAD		(23)	// fail to create thear;スレッド生成エラー
#define	ME_WRITEERROR				(24)	// lock of capacity of disk;記憶媒体の容量不足


// definition of call-back function for user;ユーザーのコールバック関数定義
typedef	MERET	(*MPGE_USERFUNC)(void *buf, unsigned long nLength );
#define MPGE_NULL_FUNC (MPGE_USERFUNC)NULL

///////////////////////////////////////////////////////////////////////////
// Configuration
///////////////////////////////////////////////////////////////////////////
// for INPUT
#define		MC_INPUTFILE			(1)
	// para1 choice of input device
	#define		MC_INPDEV_FILE		(0)	// input device is file;入力デバイスはファイル
	#define		MC_INPDEV_STDIO		(1)	//                 stdin;入力デバイスは標準入力
	#define		MC_INPDEV_USERFUNC	(2)	//       defined by user;入力デバイスはユーザー定義
	#define		MC_INPDEV_LIBSND	(3) // input device is file via LIBSND ;入力デバイスは LIBSND 経由でファイル
	// para2 (必要であれば)ファイル名。ポインタを指定する
	// メモリよりエンコードの時は以下の構造体のポインタを指定する.
	struct MCP_INPDEV_USERFUNC {
		MPGE_USERFUNC	pUserFunc;		// pointer to user-function for call-back or MPGE_NULL_FUNC if none
											// コールバック対象のユーザー関数。未定義時MPGE_NULL_FUNCを代入
		unsigned int	nSize;			// size of file or MC_INPDEV_MEMORY_NOSIZE if unknown
											// ファイルサイズ。不定の時は MC_INPDEV_MEMORY_NOSIZEを指定
		int				nBit;	// nBit = 8 or 16 ; PCMビット深度を指定
		int				nFreq;	// input frequency ; 入力周波数の指定
		int				nChn;	// number of channel(1 or 2) ; チャネル数
	};
	#define		MC_INPDEV_MEMORY_NOSIZE		(0xffffffff)
/*
  Using userfunction input;
  ユーザー関数利用時の挙動
  ^^^^^^^^^^^^^^^^^^^^^^^^

  ユーザーが登録した関数 UsefFuncに対して、DLLより読み込み要求が行われる。
  MERET	UserFunc_input(void *buf, unsigned long nLength );

  要求を処理する際に
     ・void *buf には nLength バイト分のデータを格納、 return ME_NOERRで抜ける
	 ・ファイルの最後に達して、nLength分読み込めない(かつ少なくとも1バイト以上読み込める)場合、
	    memset( buf + 読み込んだデータbyte, 0, nLength - 読み込んだデータサイズ) ;
	   として return ME_NOERR する。
	 ・１バイトも読めない場合は、何もせず return ME_EMPTYSTREAM; で抜ける
*/

///////////////////////////////////////////////////////////////////////////
// for OUTPUT ( now stdout is not support )
#define		MC_OUTPUTFILE			(2)
// para1 choice of output device
	#define		MC_OUTDEV_FILE		(0)	// output device is file;出力デバイスはファイル
	#define		MC_OUTDEV_STDOUT	(1)	//                  stdout; 出力デバイスは標準出力
	#define		MC_OUTDEV_USERFUNC	(2)	//        defined by user;出力デバイスはユーザー定義
	#define		MC_OUTDEV_USERFUNC_WITHVBRTAG	(3)	//       defined by user;入力デバイスはユーザー定義/VBRタグ書き出し
// para2 pointer to file if necessary ;(必要であれば)ファイル名。ポインタ指定

/*
  Using userfunction output
  ユーザー関数利用時の挙動
  ^^^^^^^^^^^^^^^^^^^^^^^^

  ユーザーが登録した関数 UsefFuncに対して、DLLより書込み要求が行われる。
  MERET	UserFunc_output(void *buf, unsigned long nLength );

  要求を処理する際に
     ・void *buf には nLength バイト分のデータが格納されているので 
	   fwrite( buf, 1, nLength, fp );の様にして書き出しreturn ME_NOERRで抜ける.
	   書き出しに失敗した時は、return ME_WRITEERROR;で抜ける.
  エンコード終了の通知/処理はMC_OUTDEV_USERFUNCとMC_OUTDEV_USERFUNC_WITHVBRTAGで異なる
	(MC_OUTDEV_USERFUNCの場合)
	 ・最後に buf == NULLで1度呼び出される. return 値は何でも良い。
	(MC_OUTDEV_USERFUNC_WITHVBRTAGの場合)
	 ・buf == NULLで呼び出される.この際にファイルの先頭へシークし、
	   ファイル全体のサイズを returnの値とする。filesize<=0の時は終了。
	   (誤って return ME_NOERR; で抜けない様に注意!! )
	 ・XING-VBRタグデータが bufに、XINGVBRタグのサイズが nLengthに格納されて呼び出されるので
	   ファイル先頭からそれを書き込む.
	 ・最後にもう一度buf == NULLで呼び出される. return 値は何でも良い。
*/

///////////////////////////////////////////////////////////////////////////
// mode of encoding ;エンコードタイプ
#define		MC_ENCODEMODE			(3)
// para1 mode;モード設定
	#define		MC_MODE_MONO		(0)		// mono;モノラル
	#define		MC_MODE_STEREO		(1)		// stereo;ステレオ
	#define		MC_MODE_JOINT		(2)		// joint-stereo;ジョイント
	#define		MC_MODE_MSSTEREO	(3)		// mid/side stereo;ミッドサイド
	#define		MC_MODE_DUALCHANNEL	(4)		// dual channel;デュアルチャネル

///////////////////////////////////////////////////////////////////////////
// bitrate;ビットレート設定
#define		MC_BITRATE				(4)
// para1 bitrate;ビットレート 即値指定


///////////////////////////////////////////////////////////////////////////
// frequency of input file (force);入力で用いるサンプル周波数の強制指定
#define		MC_INPFREQ				(5)
// para1 frequency;入出力で用いるデータ

///////////////////////////////////////////////////////////////////////////
// frequency of output mp3 (force);出力で用いるサンプル周波数の強制指定
#define		MC_OUTFREQ				(6)
// para1 frequency;入出力で用いるデータ

///////////////////////////////////////////////////////////////////////////
// size ofheader if you ignore WAV-header (for example cda);エンコード開始位置の強制指定(ヘッダを無視する時)
#define		MC_STARTOFFSET			(7)

///////////////////////////////////////////////////////////////////////////
// psycho-acoustics ON/OFF;心理解析 ON/OFF
#define		MC_USEPSY				(8)
// PARA1 boolean(TRUE/FALSE)

///////////////////////////////////////////////////////////////////////////
// 16kHz low-pass filter ON/OFF;16KHz低帯域フィルタ ON/OFF
#define		MC_USELPF16				(9)
// PARA1 boolean(TRUE/FALSE)

///////////////////////////////////////////////////////////////////////////
// use special UNIT, para1:boolean; ユニット指定 para1:BOOL値
#define		MC_USEMMX				(10)	// MMX
#define		MC_USE3DNOW				(11)	// 3DNow!
#define		MC_USESSE				(12)	// SSE(KNI)
#define		MC_USEKNI				MC_USESSE	// 午後2との互換性
#define		MC_USEE3DNOW			(13)	// Enhanced 3D Now!
#define		MC_USECMOV				(38)	// CMOV
#define		MC_USEEMMX				(39)	// EMMX
#define		MC_USESSE2				(40)	// SSE2
/* #define 	MC_USECLFLUSH				(43)	// CLFLUSH */
#define 	MC_USEALTIVEC				(44)	// ALTIVEC
#define		MC_USESPC1				(14)	// special switch for debug
#define		MC_USESPC2				(15)	// special switch for debug

///////////////////////////////////////////////////////////////////////////
// addition of TAG; ファイルタグ情報付加
#define		MC_ADDTAG				(16)
// dwPara1  length of TAG;タグ長  
// dwPara2  pointer to TAG;タグデータのポインタ

///////////////////////////////////////////////////////////////////////////
// emphasis;エンファシスタイプの設定
#define		MC_EMPHASIS				(17)	
// para1 type of emphasis;エンファシスタイプの設定
	#define		MC_EMP_NONE			(0)		// no empahsis;エンファシスなし(dflt)
	#define		MC_EMP_5015MS		(1)		// 50/15ms    ;エンファシス50/15ms
	#define		MC_EMP_CCITT		(3)		// CCITT      ;エンファシスCCITT

///////////////////////////////////////////////////////////////////////////
// use VBR;VBRタイプの設定
#define		MC_VBR					(18)

///////////////////////////////////////////////////////////////////////////
// SMP support para1: interger
#define		MC_CPU					(19)

///////////////////////////////////////////////////////////////////////////
// for RAW-PCM; 以下4つはRAW-PCMの設定のため
// byte swapping for 16bitPCM; PCM入力時のlow, high bit 変換
#define		MC_BYTE_SWAP			(20)

///////////////////////////////////////////////////////////////////////////
// for 8bit PCM
#define		MC_8BIT_PCM				(21)

///////////////////////////////////////////////////////////////////////////
// for mono PCM
#define		MC_MONO_PCM				(22)

///////////////////////////////////////////////////////////////////////////
// for Towns SND
#define		MC_TOWNS_SND			(23)

///////////////////////////////////////////////////////////////////////////
// BeOS & Win32 Encode thread priority
#define		MC_THREAD_PRIORITY		(24)
// (WIN32) dwPara1 MULTITHREAD Priority (THREAD_PRIORITY_**** at WinBASE.h )

///////////////////////////////////////////////////////////////////////////
// BeOS Read thread priority
//#if	defined(USE_BTHREAD)
#define		MC_READTHREAD_PRIORITY	(25)
//#endif

///////////////////////////////////////////////////////////////////////////
// output format 
#define		MC_OUTPUT_FORMAT		(26)
// para1 
	#define		MC_OUTPUT_NORMAL	(0)		// mp3+TAG(see MC_ADDTAG)
	#define		MC_OUTPUT_RIFF_WAVE	(1)		// RIFF/WAVE
	#define		MC_OUTPUT_RIFF_RMP	(2)		// RIFF/RMP

///////////////////////////////////////////////////////////////////////////
// LIST/INFO chunk of RIFF/WAVE or RIFF/RMP 
#define		MC_RIFF_INFO			(27)
// para1 size of info(include info name)
// para2 pointer to info
//   byte offset       contents
//   0..3              info name
//   4..size of info-1 info

///////////////////////////////////////////////////////////////////////////
// verify and overwrite
#define		MC_VERIFY				(28)

///////////////////////////////////////////////////////////////////////////
// output directory
#define		MC_OUTPUTDIR			(29)

///////////////////////////////////////////////////////////////////////////
// VBRの最低/最高ビットレートの設定
#define		MC_VBRBITRATE			(30)
// para1 最低ビットレート (kbps)
// para2 最高ビットレート (kbps)


///////////////////////////////////////////////////////////////////////////
// 拡張フィルタの使用 LPF1, LPF2
#define		MC_ENHANCEDFILTER		(31)
// para1 LPF1 (0-100)	, dflt=auto setting by outfreq
// para2 LPF2 (0-100)	, dflt=auto setting by outfreq


///////////////////////////////////////////////////////////////////////////
// Joint-stereoにおける、ステレオ/MSステレオの切り替えの閾値
#define		MC_MSTHRESHOLD	 		(32)
// para1 threshold  (0-100)	, dflt=auto setting by outfreq
// para2 mspower    (0-100) , dflt=auto setting by outfreq

///////////////////////////////////////////////////////////////////////////
// Language
#define		MC_LANG					(33)
// t_lang defined in message.h

///////////////////////////////////////////////////////////////////////////
// 読み込みデータの最大サイズ設定 / max data length ( byte )
#define		MC_MAXFILELENGTH		(34)
// para1 maxfilesize (PCM body length, not include wave heaher size.)
//       (0-0xfffffffd)		// as byte
		#define		MC_MAXFLEN_IGNORE		(ULONG_MAX)		// DEFAULT
		#define		MC_MAXFLEN_WAVEHEADER	(ULONG_MAX-1)	// WAVEヘッダの値を使用


///////////////////////////////////////////////////////////////////////////
// 出力ストリームのバッファリングフラグ
#define		MC_OUTSTREAM_BUFFERD	(35)
// para1  enable(=1) or disable(=0), dflt=enable
		#define		MC_OBUFFER_ENABLE		1				// DEFAULT
		#define		MC_OBUFFER_DISABLE		0

// 以下はぷち午後新設

///////////////////////////////////////////////////////////////////////////
// quality (same as lame-option `-q')
#define		MC_ENCODE_QUALITY		(36)
// 1(high quality) <= para1 <= 9(low quality)
// 2:-h
// 5:default
// 7:-f



///////////////////////////////////////////////////////////////////////////
// use ABR;ABRタイプの設定
#define		MC_ABR					(37)

///////////////////////////////////////////////////////////////////////////
// 増設されたCPUタイプの設定
// defined in `use special UNIT'
//#define		MC_USECMOV				(38)	// CMOV 上で定義済み
//#define		MC_USEEMMX				(39)	// EMMX 上で定義済み
//#define		MC_USESSE2				(40)	// SSE2 上で定義済み

///////////////////////////////////////////////////////////////////////////
// LAMEタグの出力設定 (併せてVBRタグの書き出しを有効にしてください)
#define		MC_WRITELAMETAG					(41)
/// para1: 0 = disable (default)
///        1 = enable

///////////////////////////////////////////////////////////////////////////
// VBRタグの出力設定 (CBRでなおかつlametag無効時は設定内容に関係なく無効です)
#define		MC_WRITEVBRTAG					(42)
/// para1: 0 = disable 
///        1 = enable (default)


///////////////////////////////////////////////////////////////////////////
//  Functions
///////////////////////////////////////////////////////////////////////////
#ifdef GOGO_DLL_EXPORTS
#define		EXPORT				__declspec(dllexport) __cdecl	
#define		EXPORT_VB			__declspec(dllexport) __stdcall	
#else
#define		EXPORT
#define		EXPORT_VB
#endif

MERET	EXPORT	MPGE_initializeWork(void);
MERET	EXPORT	MPGE_setConfigure(UPARAM mode, UPARAM dwPara1, UPARAM dwPara2);
MERET	EXPORT	MPGE_getConfigure(UPARAM mode, void *para1);
MERET	EXPORT	MPGE_detectConfigure(void);
MERET	EXPORT	MPGE_processFrame(void);
MERET	EXPORT	MPGE_closeCoder(void);
MERET	EXPORT	MPGE_endCoder(void);
MERET	EXPORT	MPGE_getUnitStates( unsigned long *unit );
MERET	EXPORT	MPGE_processTrack(void);

// This function is effective for gogo.dll;このファンクションはDLLバージョンのみ有効
MERET	EXPORT	MPGE_getVersion( unsigned long *vercode,  char *verstring );
#define MGV_BUFLENGTH 260
// vercode = 0x125 ->  version 1.25
// verstring       ->  "ver 1.25 1999/09/25" (allocate abobe 260bytes buffer)


////////////////////////////////////////////////////////////////////////////
// for getting configuration
////////////////////////////////////////////////////////////////////////////

#define		MG_INPUTFILE			(1)		// name of input file ;入力ファイル名取得
#define		MG_OUTPUTFILE			(2)		// name of output file;出力ファイル名取得
#define		MG_ENCODEMODE			(3)		// type of encoding   ;エンコードモード
#define		MG_BITRATE				(4)		// bitrate            ;ビットレート
#define		MG_INPFREQ				(5)		// input frequency    ;入力周波数
#define		MG_OUTFREQ				(6)		// output frequency   ;出力周波数
#define		MG_STARTOFFSET			(7)		// offset of input PCM;スタートオフセット
#define		MG_USEPSY				(8)		// psycho-acoustics   ;心理解析を使用する/しない
#define		MG_USEMMX				(9)		// MMX
#define		MG_USE3DNOW				(10)	// 3DNow!
#define		MG_USESSE				(11)	// SSE(KNI)
#define		MG_USEKNI				MG_USESSE	// 午後2との互換性
#define		MG_USEE3DNOW			(12)	// Enhanced 3DNow!
#define		MG_USECMOV				(20)	// CMOV
#define		MG_USEEMMX				(21)	// EMMX
#define		MG_USESSE2				(22)	// SSE2
#define 	MG_CLFLUSH				(23)	// CLFLUSH
#define 	MG_USEALTIVEC				(24)	// ALTIVEC
#define		MG_USESPC1				(13)	// special switch for debug
#define		MG_USESPC2				(14)	// special switch for debug
#define		MG_COUNT_FRAME			(15)	// amount of frame
#define		MG_NUM_OF_SAMPLES		(16)	// number of sample for 1 frame;1フレームあたりのサンプル数
#define		MG_MPEG_VERSION			(17)	// MPEG VERSION
#define		MG_READTHREAD_PRIORITY	(18)	// thread priority to read for BeOS
#define		MG_FRAME				(19)	// frame number
//#define		MG_USECMOV				(20)	// CMOV 上で定義済み
//#define		MG_USEEMMX				(21)	// EMMX 上で定義済み
//#define		MG_USESSE2				(22)	// SSE2 上で定義済み



////////////////////////////////////////////////////////////////////////////
//  for MPGE_getUnitStates()
////////////////////////////////////////////////////////////////////////////
// x86 - Spec
#define		MU_tFPU					(1<<0)
#define		MU_tMMX					(1<<1)
#define		MU_t3DN					(1<<2)
#define		MU_tSSE					(1<<3)
#define		MU_tCMOV				(1<<4)
#define		MU_tE3DN				(1<<5)	/* for Athlon(Externd 3D Now!) */
#define 	MU_tEMMX				(1<<6)  /* EMMX = E3DNow!_INT = SSE_INT  */
#define		MU_tSSE2				(1<<7)
#define 	MU_tCLFLUSH				(1<<18)
#define 	MU_tMULTI				(1<<12)	/* for Multi-threaded encoder. Never set on UP or in the binary linked w/o multithread lib. */

// x86 - Vendor
#define 	MU_tINTEL				(1<<8)
#define 	MU_tAMD					(1<<9)
#define 	MU_tCYRIX				(1<<10)
#define 	MU_tIDT					(1<<11)
#define 	MU_tUNKNOWN				(1<<15)	/* unknown vendor */

// x86 - Special
#define 	MU_tSPC1 				(1<<16)	/* special flag */
#define 	MU_tSPC2 				(1<<17)	/* freely available */
// x86 - CPU TYPES
#define 	MU_tFAMILY4				(1<<20)	/* 486 vendor maybe isn't correct */
#define 	MU_tFAMILY5				(1<<21)	/* 586 (P5, P5-MMX, K6, K6-2, K6-III) */
#define 	MU_tFAMILY6				(1<<22)	/* 686 above P-Pro, P-II, P-III, Athlon */
#define 	MU_tFAMILY7				(1<<23) /* Pentium IV ? */

// for PPC arc
#define 	MU_tPPC					(1<<0)
#define 	MU_tGRAP				(1<<1)	/* fres, frsqrte, fsel */
#define 	MU_tFSQRT				(1<<2)	/* fsqrt, fsqrts */
#define 	MU_tALTIVEC				(1<<3)	/* AltiVec */


#endif /* GOGO_H_ */
