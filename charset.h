/*
 * Copyright (C) 1999-2000 Thomas Roessler <roessler@guug.de>
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
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */

#ifndef _CHARSET_H
#define _CHARSET_H

#include <iconv.h>

int mutt_convert_string (char **, const char *, const char *);

iconv_t mutt_iconv_open (const char *, const char *);
size_t mutt_iconv (iconv_t, const char **, size_t *, char **, size_t *, const char **, const char *);

typedef void * FGETCONV;

FGETCONV *fgetconv_open (FILE *, const char *, const char *);
int fgetconv (FGETCONV *);
void fgetconv_close (FGETCONV **);

void mutt_set_langinfo_charset (void);

#endif /* _CHARSET_H */
