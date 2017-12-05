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

#ifndef _MUTT_CHARSET2_H
#define _MUTT_CHARSET2_H

#include <iconv.h>
#include <stdbool.h>
#include <stdio.h>

int mutt_convert_string(char **ps, const char *from, const char *to, int flags);

iconv_t mutt_iconv_open(const char *tocode, const char *fromcode, int flags);
FGETCONV *fgetconv_open(FILE *file, const char *from, const char *to, int flags);

/* flags for charset.c:mutt_convert_string(), fgetconv_open(), and
 * mutt_iconv_open(). Note that applying charset-hooks to tocode is
 * never needed, and sometimes hurts: Hence there is no MUTT_ICONV_HOOK_TO
 * flag.
 */
#define MUTT_ICONV_HOOK_FROM 1 /* apply charset-hooks to fromcode */

bool mutt_check_charset(const char *s, bool strict);

#endif /* _MUTT_CHARSET2_H */
