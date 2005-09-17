/*
 * Copyright (C) 1999-2000 Thomas Roessler <roessler@does-not-exist.org>
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

#define M_ICONV_HOOK_FROM 1
#define M_ICONV_HOOK_TO   2

#endif /* _CHARSET_H */
