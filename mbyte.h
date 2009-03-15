#ifndef _MBYTE_H
# define _MBYTE_H

# ifdef HAVE_WC_FUNCS
#  ifdef HAVE_WCHAR_H
#   include <wchar.h>
#  endif
#  ifdef HAVE_WCTYPE_H
#   include <wctype.h>
#  endif
# endif

# ifndef HAVE_WC_FUNCS
#ifdef towupper
# undef towupper
#endif
#ifdef towlower
# undef towlower
#endif
#ifdef iswprint
# undef iswprint
#endif
#ifdef iswspace
# undef iswspace
#endif
#ifdef iswalnum
# undef iswalnum
#endif
#ifdef iswalpha
# undef iswalpha
#endif
#ifdef iswupper
# undef iswupper
#endif
size_t wcrtomb (char *s, wchar_t wc, mbstate_t *ps);
size_t mbrtowc (wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);
int iswprint (wint_t wc);
int iswspace (wint_t wc);
int iswalnum (wint_t wc);
int iswalpha (wint_t wc);
int iswupper (wint_t wc);
wint_t towupper (wint_t wc);
wint_t towlower (wint_t wc);
int wcwidth (wchar_t wc);
# endif /* !HAVE_WC_FUNCS */


void mutt_set_charset (char *charset);
extern int Charset_is_utf8;
size_t utf8rtowc (wchar_t *pwc, const char *s, size_t n, mbstate_t *_ps);
wchar_t replacement_char (void);

#endif /* _MBYTE_H */
