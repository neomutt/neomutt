#ifndef _MBYTE_H
#define _MBYTE_H

void mutt_set_charset (char *charset);

size_t utf8rtowc (wchar_t *pwc, const char *s, size_t n, mbstate_t *_ps);

int wctomb (char *s, wchar_t wc);
int mbtowc (wchar_t *pwc, const char *s, size_t n);
size_t mbrtowc (wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);
int iswprint (wint_t wc);
int wcwidth (wchar_t wc);

wchar_t replacement_char (void);

#endif /* _MBYTE_H */
