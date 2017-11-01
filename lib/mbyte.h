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

#ifndef _LIB_MBYTE_H
#define _LIB_MBYTE_H

#include <stddef.h>
#include <ctype.h>
#include <stdbool.h>
#include <wctype.h>

extern bool OPT_LOCALES;
extern wchar_t ReplacementChar;

#ifdef LOCALES_HACK
#define IsPrint(c) (isprint((unsigned char) (c)) || ((unsigned char) (c) >= 0xa0))
#define IsWPrint(wc) (iswprint(wc) || wc >= 0xa0)
#else
#define IsPrint(c)                                                             \
  (isprint((unsigned char) (c)) ||                                             \
   (OPT_LOCALES ? 0 : ((unsigned char) (c) >= 0xa0)))
#define IsWPrint(wc) (iswprint(wc) || (OPT_LOCALES ? 0 : (wc >= 0xa0)))
#endif

bool   get_initials(const char *name, char *buf, int buflen);
bool   is_shell_char(wchar_t ch);
int    mutt_charlen(const char *s, int *width);
size_t my_mbstowcs(wchar_t **pwbuf, size_t *pwbuflen, size_t i, char *buf);
void   my_wcstombs(char *dest, size_t dlen, const wchar_t *src, size_t slen);
int    my_wcswidth(const wchar_t *s, size_t n);
int    my_wcwidth(wchar_t wc);
int    my_width(const char *str, int col, bool display);
size_t width_ceiling(const wchar_t *s, size_t n, int w1);

#endif /* _LIB_MBYTE_H */
