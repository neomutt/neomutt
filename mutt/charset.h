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

#ifndef MUTT_LIB_CHARSET_H
#define MUTT_LIB_CHARSET_H

#include <iconv.h>
#include <stdbool.h>
#include <stdio.h>
#include <wchar.h>

struct Buffer;

extern struct Slist *C_AssumedCharset;
extern char *C_Charset;
extern bool CharsetIsUtf8;
extern wchar_t ReplacementChar;

/**
 * struct FgetConv - Cursor for converting a file's encoding
 */
struct FgetConv
{
  FILE *fp;
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
  FILE *fp;
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

/**
 * enum LookupType - Types of character set lookups
 */
enum LookupType
{
  MUTT_LOOKUP_CHARSET, ///< Alias for another character set
  MUTT_LOOKUP_ICONV,   ///< Character set conversion
};

#define MUTT_ICONV_HOOK_FROM 1 ///< apply charset-hooks to fromcode

extern const struct MimeNames PreferredMimeNames[];

void             mutt_ch_canonical_charset(char *buf, size_t buflen, const char *name);
const char *     mutt_ch_charset_lookup(const char *chs);
int              mutt_ch_check(const char *s, size_t slen, const char *from, const char *to);
bool             mutt_ch_check_charset(const char *cs, bool strict);
char *           mutt_ch_choose(const char *fromcode, const struct Slist *charsets, const char *u, size_t ulen, char **d, size_t *dlen);
bool             mutt_ch_chscmp(const char *cs1, const char *cs2);
int              mutt_ch_convert_nonmime_string(char **ps);
int              mutt_ch_convert_string(char **ps, const char *from, const char *to, int flags);
int              mutt_ch_fgetconv(struct FgetConv *fc);
void             mutt_ch_fgetconv_close(struct FgetConv **fc);
struct FgetConv *mutt_ch_fgetconv_open(FILE *fp, const char *from, const char *to, int flags);
char *           mutt_ch_fgetconvs(char *buf, size_t buflen, struct FgetConv *fc);
char *           mutt_ch_get_default_charset(void);
char *           mutt_ch_get_langinfo_charset(void);
size_t           mutt_ch_iconv(iconv_t cd, const char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft, const char **inrepls, const char *outrepl, int *iconverrno);
const char *     mutt_ch_iconv_lookup(const char *chs);
iconv_t          mutt_ch_iconv_open(const char *tocode, const char *fromcode, int flags);
bool             mutt_ch_lookup_add(enum LookupType type, const char *pat, const char *replace, struct Buffer *err);
void             mutt_ch_lookup_remove(void);
void             mutt_ch_set_charset(const char *charset);

#define mutt_ch_is_utf8(str)     mutt_ch_chscmp(str, "utf-8")
#define mutt_ch_is_us_ascii(str) mutt_ch_chscmp(str, "us-ascii")

#endif /* MUTT_LIB_CHARSET_H */
