#ifndef _ICONV_H
#define _ICONV_H

#include <unistd.h>

typedef void * iconv_t;

iconv_t iconv_open (const char *, const char *);
size_t iconv (iconv_t, const char **, size_t *, char **, size_t *);
int iconv_close (iconv_t);

#endif /* _ICONV_H */
