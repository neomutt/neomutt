/**
 * @file
 * Multi-byte String manipulation functions
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
 *
 * @copyright
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

#ifndef _MUTT_MBYTE_H
#define _MUTT_MBYTE_H

#include <stddef.h>
#include <ctype.h>
#include <stdbool.h>
#include <wchar.h>
#include <wctype.h>

extern bool OptLocales;

#ifdef LOCALES_HACK
#define IsPrint(c) (isprint((unsigned char) (c)) || ((unsigned char) (c) >= 0xa0))
#define IsWPrint(wc) (iswprint(wc) || wc >= 0xa0)
#else
#define IsPrint(c)                                                             \
  (isprint((unsigned char) (c)) ||                                             \
   (OptLocales ? 0 : ((unsigned char) (c) >= 0xa0)))
#define IsWPrint(wc) (iswprint(wc) || (OptLocales ? 0 : (wc >= 0xa0)))
#endif

int    mutt_mb_charlen(const char *s, int *width);
bool   mutt_mb_get_initials(const char *name, char *buf, int buflen);
bool   mutt_mb_is_lower(const char *s);
bool   mutt_mb_is_shell_char(wchar_t ch);
size_t mutt_mb_mbstowcs(wchar_t **pwbuf, size_t *pwbuflen, size_t i, char *buf);
void   mutt_mb_wcstombs(char *dest, size_t dlen, const wchar_t *src, size_t slen);
int    mutt_mb_wcswidth(const wchar_t *s, size_t n);
int    mutt_mb_wcwidth(wchar_t wc);
size_t mutt_mb_width_ceiling(const wchar_t *s, size_t n, int w1);
int    mutt_mb_width(const char *str, int col, bool display);
int    mutt_mb_filter_unprintable(char **s);
bool   mutt_mb_is_display_corrupting_utf8(wchar_t wc);

#endif /* _MUTT_MBYTE_H */
