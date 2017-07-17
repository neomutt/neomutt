/**
 * @file
 * Conversion between different character encodings
 *
 * @authors
 * Copyright (C) 1999-2003 Thomas Roessler <roessler@does-not-exist.org>
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

#ifndef _MUTT_CHARSET_H
#define _MUTT_CHARSET_H

#include <iconv.h>
#include <stdbool.h>
#include <stdio.h>

int mutt_convert_string(char **ps, const char *from, const char *to, int flags);

iconv_t mutt_iconv_open(const char *tocode, const char *fromcode, int flags);
size_t mutt_iconv(iconv_t cd, ICONV_CONST char **inbuf, size_t *inbytesleft,
                  char **outbuf, size_t *outbytesleft,
                  ICONV_CONST char **inrepls, const char *outrepl);

typedef void *FGETCONV;

FGETCONV *fgetconv_open(FILE *file, const char *from, const char *to, int flags);
int fgetconv(FGETCONV *_fc);
char *fgetconvs(char *buf, size_t l, FGETCONV *_fc);
void fgetconv_close(FGETCONV **_fc);

void mutt_set_langinfo_charset(void);
char *mutt_get_default_charset(void);

/* flags for charset.c:mutt_convert_string(), fgetconv_open(), and
 * mutt_iconv_open(). Note that applying charset-hooks to tocode is
 * never needed, and sometimes hurts: Hence there is no MUTT_ICONV_HOOK_TO
 * flag.
 */
#define MUTT_ICONV_HOOK_FROM 1 /* apply charset-hooks to fromcode */

/* Check if given character set is valid (either officially assigned or
 * known to local iconv implementation). If strict is non-zero, check
 * against iconv only. Returns 0 if known and negative otherwise.
 */
bool mutt_check_charset(const char *s, bool strict);

#endif /* _MUTT_CHARSET_H */
