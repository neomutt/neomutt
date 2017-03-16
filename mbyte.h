/**
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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


void mutt_set_charset(char *charset);
extern int Charset_is_utf8;
wchar_t replacement_char(void);
int is_display_corrupting_utf8(wchar_t wc);

#endif /* _MBYTE_H */
