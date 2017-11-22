/**
 * @file
 * Conversion between different character encodings
 *
 * @authors
 * Copyright (C) 1999-2002,2007 Thomas Roessler <roessler@does-not-exist.org>
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
#include <stdio.h>

extern char *AssumedCharset;
extern char *Charset;

typedef void *FGETCONV;

/**
 * struct FgetConv - Cursor for converting a file's encoding
 */
struct FgetConv
{
  FILE *file;
  iconv_t cd;
  char bufi[512];
  char bufo[512];
  char *p;
  char *ob;
  char *ib;
  size_t ibl;
  const char **inrepls;
};

/**
 * struct FgetConvNot - A dummy converter
 */
struct FgetConvNot
{
  FILE *file;
  iconv_t cd;
};

struct MimeNames
{
  const char *key;
  const char *pref;
};

extern const struct MimeNames PreferredMIMENames[];

char * fgetconvs(char *buf, size_t l, FGETCONV *_fc);
char * mutt_get_default_charset(void);
int    fgetconv(FGETCONV *_fc);
int    mutt_chscmp(const char *s, const char *chs);
size_t mutt_iconv(iconv_t cd, const char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft, const char **inrepls, const char *outrepl);
void   fgetconv_close(FGETCONV **_fc);
void   mutt_canonical_charset(char *dest, size_t dlen, const char *name);
void   mutt_set_langinfo_charset(void);

#define mutt_is_utf8(a)     mutt_chscmp(a, "utf-8")
#define mutt_is_us_ascii(a) mutt_chscmp(a, "us-ascii")

#endif 
