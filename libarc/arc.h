#ifndef ___LIBARC_H_
#define ___LIBARC_H_

/* Archive library */

#include "memb.h"

#define ARC_LIB_VERSION "1.4.10"
#define ARC_DEFLATE_LEVEL 6	/* 1:Compress faster .. 9:Compress better */

typedef struct _ArchiveStreamURL
{
    int idx;
    long seek_start;
    URL url;
} ArchiveStreamURL;

typedef struct _ArchiveEntryNode
{
    struct _ArchiveEntryNode *next; /* next entry */
    int comptype;		/* Compression/Encoding type */
    int strmtype;		/* Archive stream type */
    long compsize;		/* Compressed size */
    long origsize;		/* Uncompressed size */

    union
    {
	/* Archive streams */
	MemBuffer compdata;	/* for ARCSTRM_MEMBUFFER */
	long seek_start;	/* for ARCSTRM_SEEK_URL */
	char *pathname;		/* for ARCSTRM_PATHNAME */
	ArchiveStreamURL aurl;	/* for ARCSTRM_URL */
    } u;
    char filename[1];		/* Archive file name (variable length) */
} ArchiveEntryNode;

typedef struct _ArchiveHandler
{
    /* Archive handler type */
    int type;

    /* Number of archive entryes */
    int nfiles;

    /* Archive entry */
    ArchiveEntryNode *entry_head;
    ArchiveEntryNode *entry_tail;

    /* current extract entry */
    ArchiveEntryNode *entry_cur;

    /* current decode handler */
    void *decoder;

    /* decode rest size */
    long pos;

    /* for decoder stream */
    URL decode_stream;

    /* for AHANDLER_SEEK */
    URL seek_stream;

    /* Memory pool */
    MBlockList pool;
} *ArchiveHandler;

typedef struct _ArchiveFileList
{
    char *archive_name;
    int errstatus;
    ArchiveHandler archiver;
    struct _ArchiveFileList *next;
} ArchiveFileList;

struct archive_ext_type_t
{
    char *ext;
    int type;
};

/* Archive handler type */
enum
{
    AHANDLER_CACHED,
    AHANDLER_SEEK,
    AHANDLER_DIR,
    AHANDLER_NEWSGROUP
};

/* Archive stream type */
enum
{
    ARCSTRM_MEMBUFFER,
    ARCSTRM_SEEK_URL,
    ARCSTRM_PATHNAME,
    ARCSTRM_URL,
    ARCSTRM_NEWSGROUP
};

/* Compression/Encoding type */
enum
{
    ARCHIVEC_STORED,		/* No compression */
    ARCHIVEC_PATHNAME,		/* Pathname */
    ARCHIVEC_COMPRESSED,	/* Compressed */
    ARCHIVEC_PACKED,		/* Packed */
    ARCHIVEC_DEFLATED,		/* Deflate */
    ARCHIVEC_SHRUNKED,		/* Shrunked */
    ARCHIVEC_REDUCED1,		/* Reduced with compression factor 1 */
    ARCHIVEC_REDUCED2,		/* Reduced with compression factor 2 */
    ARCHIVEC_REDUCED3,		/* Reduced with compression factor 3 */
    ARCHIVEC_REDUCED4,		/* Reduced with compression factor 4 */
    ARCHIVEC_IMPLODED,		/* Implode base-tag */
    ARCHIVEC_IMPLODED_LIT8,	/* 8K sliding window (coded) */
    ARCHIVEC_IMPLODED_LIT4,	/* 4K sliding window (coded) */
    ARCHIVEC_IMPLODED_NOLIT8,	/* 8K sliding window (uncoded) */
    ARCHIVEC_IMPLODED_NOLIT4,	/* 4K sliding window (uncoded) */
    ARCHIVEC_LZHED,		/* LZH base-tag */
    ARCHIVEC_LZHED_LH0,		/* -lh0- (ARCHIVE_STORED) */
    ARCHIVEC_LZHED_LH1,		/* -lh1- */
    ARCHIVEC_LZHED_LH2,		/* -lh2- */
    ARCHIVEC_LZHED_LH3,		/* -lh3- */
    ARCHIVEC_LZHED_LH4,		/* -lh4- */
    ARCHIVEC_LZHED_LH5,		/* -lh5- */
    ARCHIVEC_LZHED_LZS,		/* -lzs- */
    ARCHIVEC_LZHED_LZ5,		/* -lz5- */
    ARCHIVEC_LZHED_LZ4,		/* -lz4- (ARCHIVE_STORED) */
    ARCHIVEC_LZHED_LHD,		/* -lhd- (Directory, No compression data) */
    ARCHIVEC_LZHED_LH6,		/* -lh6- */
    ARCHIVEC_LZHED_LH7,		/* -lh7- */
    ARCHIVEC_UU,		/* uu encoded */
    ARCHIVEC_B64,		/* base64 encoded */
    ARCHIVEC_QS,		/* quoted string encoded */
    ARCHIVEC_HQX		/* HQX encoded */
};

/* archive_type */
enum
{
    ARCHIVE_TAR,
    ARCHIVE_TGZ,
    ARCHIVE_ZIP,
    ARCHIVE_LZH,
    ARCHIVE_DIR,
    ARCHIVE_MIME,
    ARCHIVE_NEWSGROUP
};

/* Internal archive library functions */
extern ArchiveEntryNode *new_entry_node(MBlockList *pool,
					char *filename, int flen);
extern long archiver_read_func(char *buff, long buff_size, void *v);
extern ArchiveEntryNode *next_tar_entry(ArchiveHandler archiver);
extern ArchiveEntryNode *next_zip_entry(ArchiveHandler archiver);
extern ArchiveEntryNode *next_lzh_entry(ArchiveHandler archiver);
extern ArchiveEntryNode *next_dir_entry(ArchiveHandler archiver);
extern ArchiveEntryNode *next_mime_entry(ArchiveHandler archiver);
extern ArchiveEntryNode *next_newsgroup_entry(ArchiveHandler archiver);
extern int skip_gzip_header(URL url);

/* Interface functions */
extern ArchiveHandler open_archive_handler(URL instream, int archive_type);
extern ArchiveHandler open_archive_handler_name(char *name);

extern URL archive_extract_open(ArchiveHandler archiver, int idx);
extern URL archive_extract_open_name(ArchiveHandler archiver,
				     char *entry_name);
extern void close_archive_handler(ArchiveHandler archiver);
extern int get_archive_type(char *archive_name);
extern ArchiveFileList *add_archive_list(ArchiveFileList *list,
					 char *filename);
extern ArchiveFileList *make_archive_list(ArchiveFileList *list,
					  int nfiles, char **files);
extern char **expand_archive_names(ArchiveFileList *list,
				   int *nfiles_in_out, char **files);
extern ArchiveFileList* find_archiver(ArchiveFileList *list,
				      char *archive_name, int *idx_ret);
extern URL archive_file_extract_open(ArchiveFileList *list, char *name);
extern void close_archive_files(ArchiveFileList *list);
extern int arc_wildmat(char *text, char *ptn);
extern int arc_case_wildmat(char *text, char *ptn);

extern struct archive_ext_type_t archive_ext_list[];
extern int arc_internal_prefixID;
extern int arc_news_mime_allow;
extern ArchiveFileList *last_archive_file_list;

#endif /* ___LIBARC_H_ */
