
#include <stddef.h>

wchar_t *wmemchr(const wchar_t *p, wchar_t ch, size_t count)
{
	for (size_t i = 0; i < count; i++) {
		if (p[i] == ch) {
			return (wchar_t *)&p[i];
		}
	}

	return NULL;
}
