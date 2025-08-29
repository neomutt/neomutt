/**
 * @file
 * Multi-byte String manipulation functions
 *
 * @authors
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_MUTT_MBYTE_H
#define MUTT_MUTT_MBYTE_H

#include "config.h"
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <wctype.h> // IWYU pragma: keep

struct Buffer;

extern bool OptLocales;

#ifdef LOCALES_HACK
#define IsPrint(ch) (isprint((unsigned char) (ch)) || ((unsigned char) (ch) >= 0xa0))
#define IsWPrint(wc) (iswprint(wc) || wc >= 0xa0)
#else
#define IsPrint(ch) (isprint((unsigned char) (ch)) || (OptLocales ? 0 : ((unsigned char) (ch) >= 0xa0)))
#define IsWPrint(wc) (iswprint(wc) || (OptLocales ? 0 : (wc >= 0xa0)))
#endif
#define IsBOM(wc) (wc == L'\xfeff')

int    mutt_mb_charlen(const char *s, int *width);
int    mutt_mb_filter_unprintable(char **s);
bool   mutt_mb_get_initials(const char *name, char *buf, size_t buflen);
bool   mutt_mb_is_display_corrupting_utf8(wchar_t wc);
bool   mutt_mb_is_lower(const char *s);
bool   mutt_mb_is_shell_char(wchar_t ch);
size_t mutt_mb_mbstowcs(wchar_t **pwbuf, size_t *pwbuflen, size_t i, const char *buf);
void   buf_mb_wcstombs(struct Buffer *dest, const wchar_t *wstr, size_t wlen);
int    mutt_mb_wcswidth(const wchar_t *s, size_t n);
int    mutt_mb_wcwidth(wchar_t wc);
int    mutt_mb_width(const char *str, int col, bool indent);
size_t mutt_mb_width_ceiling(const wchar_t *s, size_t n, int w1);

#endif /* MUTT_MUTT_MBYTE_H */
