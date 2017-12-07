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
 * struct EgetConv - Cursor for converting a file's encoding
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

/**
 * struct MimeNames - MIME name lookup entry
 */
struct MimeNames
{
  const char *key;
  const char *pref;
};

extern const struct MimeNames PreferredMIMENames[];

void   mutt_cs_canonical_charset(char *dest, size_t dlen, const char *name);
int    mutt_cs_chscmp(const char *s, const char *chs);
void   mutt_cs_fgetconv_close(FGETCONV **handle);
int    mutt_cs_fgetconv(FGETCONV *handle);
char * mutt_cs_fgetconvs(char *buf, size_t l, FGETCONV *handle);
char * mutt_cs_get_default_charset(void);
size_t mutt_cs_iconv(iconv_t cd, const char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft, const char **inrepls, const char *outrepl);
void   mutt_cs_set_langinfo_charset(void);

#define mutt_cs_is_utf8(a)     mutt_cs_chscmp(a, "utf-8")
#define mutt_cs_is_us_ascii(a) mutt_cs_chscmp(a, "us-ascii")

#endif 
