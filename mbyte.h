#ifndef _MBYTE_H
#define _MBYTE_H

/* This is necessary because we may be redefining wchar_t, etc */
#include <stdlib.h>

#define wchar_t mutt_wchar_t
#define mbstate_t mutt_mbstate_t

typedef unsigned int wchar_t;
typedef unsigned int mbstate_t;

#define wctomb mutt_wctomb
#define mbtowc mutt_mbtowc
#define mbrtowc mutt_mbrtowc
#define iswprint mutt_iswprint
#define wcwidth mutt_wcwidth

void mutt_set_charset (char *charset);

int wctomb (char *s, wchar_t wc);
int mbtowc (wchar_t *pwc, const char *s, size_t n);
size_t utf8rtowc (wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);
size_t mbrtowc (wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);
int iswprint (wchar_t wc);
int wcwidth (wchar_t wc);

wchar_t replacement_char (void);

#endif /* _MBYTE_H */
