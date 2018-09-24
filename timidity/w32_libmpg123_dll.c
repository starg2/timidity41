
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_LIBMPG123

#include <string.h>

#include <mpg123.h>

#include <windows.h>

int load_mpg123_dll(void);
void free_mpg123_dll(void);

static HANDLE h_mpg123_dll;

typedef int (*type_mpg123_init)(void);
typedef mpg123_handle *(*type_mpg123_new)(const char *decoder, int *error);
typedef int (*type_mpg123_close)(mpg123_handle *mh);
typedef void (*type_mpg123_delete)(mpg123_handle *mh);
typedef int (*type_mpg123_format_none)(mpg123_handle *mh);
typedef int (*type_mpg123_format)(mpg123_handle *mh, long rate, int channels, int encodings);
typedef int (*type_mpg123_replace_reader_handle)(mpg123_handle *mh, ssize_t (*r_read)(void *, void *, size_t), off_t (*r_lseek)(void *, off_t, int), void (*cleanup)(void *));
typedef int (*type_mpg123_open_handle)(mpg123_handle *mh, void *iohandle);
typedef int (*type_mpg123_getformat)(mpg123_handle *mh, long *rate, int *channels, int *encoding);
typedef int (*type_mpg123_read)(mpg123_handle *mh, unsigned char *out, size_t size, size_t *done);

static struct {
	type_mpg123_init mpg123_init;
	type_mpg123_new mpg123_new;
	type_mpg123_close mpg123_close;
	type_mpg123_delete mpg123_delete;
	type_mpg123_format_none mpg123_format_none;
	type_mpg123_format mpg123_format;
	type_mpg123_replace_reader_handle mpg123_replace_reader_handle;
	type_mpg123_open_handle mpg123_open_handle;
	type_mpg123_getformat mpg123_getformat;
	type_mpg123_read mpg123_read;
} mpg123_dll;

int load_mpg123_dll(void)
{
	if (!h_mpg123_dll) {
		h_mpg123_dll = LoadLibrary(TEXT("mpg123.dll"));

		if (!h_mpg123_dll) {
			h_mpg123_dll = LoadLibrary(TEXT("libmpg123.dll"));
		}

		if (!h_mpg123_dll) {
			return -1;
		}

		mpg123_dll.mpg123_init = (type_mpg123_init)GetProcAddress(h_mpg123_dll, "mpg123_init");
		if (!mpg123_dll.mpg123_init) { free_mpg123_dll(); return -1; }
		mpg123_dll.mpg123_new = (type_mpg123_new)GetProcAddress(h_mpg123_dll, "mpg123_new");
		if (!mpg123_dll.mpg123_new) { free_mpg123_dll(); return -1; }
		mpg123_dll.mpg123_close = (type_mpg123_close)GetProcAddress(h_mpg123_dll, "mpg123_close");
		if (!mpg123_dll.mpg123_close) { free_mpg123_dll(); return -1; }
		mpg123_dll.mpg123_delete = (type_mpg123_delete)GetProcAddress(h_mpg123_dll, "mpg123_delete");
		if (!mpg123_dll.mpg123_delete) { free_mpg123_dll(); return -1; }
		mpg123_dll.mpg123_format_none = (type_mpg123_format_none)GetProcAddress(h_mpg123_dll, "mpg123_format_none");
		if (!mpg123_dll.mpg123_format_none) { free_mpg123_dll(); return -1; }
		mpg123_dll.mpg123_format = (type_mpg123_format)GetProcAddress(h_mpg123_dll, "mpg123_format");
		if (!mpg123_dll.mpg123_format) { free_mpg123_dll(); return -1; }
		mpg123_dll.mpg123_replace_reader_handle = (type_mpg123_replace_reader_handle)GetProcAddress(h_mpg123_dll, "mpg123_replace_reader_handle");
		if (!mpg123_dll.mpg123_replace_reader_handle) { free_mpg123_dll(); return -1; }
		mpg123_dll.mpg123_open_handle = (type_mpg123_open_handle)GetProcAddress(h_mpg123_dll, "mpg123_open_handle");
		if (!mpg123_dll.mpg123_open_handle) { free_mpg123_dll(); return -1; }
		mpg123_dll.mpg123_getformat = (type_mpg123_getformat)GetProcAddress(h_mpg123_dll, "mpg123_getformat");
		if (!mpg123_dll.mpg123_getformat) { free_mpg123_dll(); return -1; }
		mpg123_dll.mpg123_read = (type_mpg123_read)GetProcAddress(h_mpg123_dll, "mpg123_read");
		if (!mpg123_dll.mpg123_read) { free_mpg123_dll(); return -1; }
	}

	return 0;
}

void free_mpg123_dll(void)
{
	memset(&mpg123_dll, 0, sizeof(mpg123_dll));

	if (h_mpg123_dll) {
		FreeLibrary(h_mpg123_dll);
		h_mpg123_dll = NULL;
	}
}

int mpg123_init(void)
{
	if (mpg123_dll.mpg123_init) {
		return (*mpg123_dll.mpg123_init)();
	} else {
		return MPG123_ERR;
	}
}

mpg123_handle *mpg123_new(const char *decoder, int *error)
{
	if (mpg123_dll.mpg123_new) {
		return (*mpg123_dll.mpg123_new)(decoder, error);
	} else {
		return NULL;
	}
}

int mpg123_close(mpg123_handle *mh)
{
	if (mpg123_dll.mpg123_close) {
		return (*mpg123_dll.mpg123_close)(mh);
	} else {
		return MPG123_ERR;
	}
}

void mpg123_delete(mpg123_handle *mh)
{
	if (mpg123_dll.mpg123_delete) {
		(*mpg123_dll.mpg123_delete)(mh);
	}
}

int mpg123_format_none(mpg123_handle *mh)
{
	if (mpg123_dll.mpg123_format_none) {
		return (*mpg123_dll.mpg123_format_none)(mh);
	} else {
		return MPG123_ERR;
	}
}

int mpg123_format(mpg123_handle *mh, long rate, int channels, int encodings)
{
	if (mpg123_dll.mpg123_format) {
		return (*mpg123_dll.mpg123_format)(mh, rate, channels, encodings);
	} else {
		return MPG123_ERR;
	}
}

int mpg123_replace_reader_handle(mpg123_handle *mh, ssize_t (*r_read)(void *, void *, size_t), off_t (*r_lseek)(void *, off_t, int), void (*cleanup)(void *))
{
	if (mpg123_dll.mpg123_replace_reader_handle) {
		return (*mpg123_dll.mpg123_replace_reader_handle)(mh, r_read, r_lseek, cleanup);
	} else {
		return MPG123_ERR;
	}
}

int mpg123_open_handle(mpg123_handle *mh, void *iohandle)
{
	if (mpg123_dll.mpg123_open_handle) {
		return (*mpg123_dll.mpg123_open_handle)(mh, iohandle);
	} else {
		return MPG123_ERR;
	}
}

int mpg123_getformat(mpg123_handle *mh, long *rate, int *channels, int *encoding)
{
	if (mpg123_dll.mpg123_getformat) {
		return (*mpg123_dll.mpg123_getformat)(mh, rate, channels, encoding);
	} else {
		return MPG123_ERR;
	}
}

int mpg123_read(mpg123_handle *mh, unsigned char *out, size_t size, size_t *done)
{
	if (mpg123_dll.mpg123_read) {
		return (*mpg123_dll.mpg123_read)(mh, out, size, done);
	} else {
		return MPG123_ERR;
	}
}

#endif
