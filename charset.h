/*
 * Copyright (C) 1999-2003 Thomas Roessler <roessler@does-not-exist.org>
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _CHARSET_H
#define _CHARSET_H

#ifdef HAVE_ICONV_H
#include <iconv.h>
#endif

#ifndef HAVE_ICONV_T_DEF
typedef void *iconv_t;
#endif

#ifndef HAVE_ICONV
#define ICONV_CONST /**/
iconv_t iconv_open (const char *, const char *);
size_t iconv (iconv_t, ICONV_CONST char **, size_t *, char **, size_t *);
int iconv_close (iconv_t);
#endif

int mutt_convert_string (char **, const char *, const char *, int);

iconv_t mutt_iconv_open (const char *, const char *, int);
size_t mutt_iconv (iconv_t, ICONV_CONST char **, size_t *, char **, size_t *, ICONV_CONST char **, const char *);

typedef void * FGETCONV;

FGETCONV *fgetconv_open (FILE *, const char *, const char *, int);
int fgetconv (FGETCONV *);
char * fgetconvs (char *, size_t, FGETCONV *);
void fgetconv_close (FGETCONV **);

void mutt_set_langinfo_charset (void);
char *mutt_get_default_charset ();

/* flags for charset.c:mutt_convert_string(), fgetconv_open(), and
 * mutt_iconv_open(). Note that applying charset-hooks to tocode is
 * never needed, and sometimes hurts: Hence there is no M_ICONV_HOOK_TO
 * flag.
 */
#define M_ICONV_HOOK_FROM 1	/* apply charset-hooks to fromcode */

/* Check if given character set is valid (either officially assigned or
 * known to local iconv implementation). If strict is non-zero, check
 * against iconv only. Returns 0 if known and negative otherwise.
 */
int mutt_check_charset (const char *s, int strict);

#endif /* _CHARSET_H */
