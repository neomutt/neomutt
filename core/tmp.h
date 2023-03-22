/**
 * @file
 * Create Temporary Files
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_CORE_TMP_H
#define MUTT_CORE_TMP_H

#include <stdio.h>

struct Buffer;

void  mutt_buffer_mktemp_full(struct Buffer *buf, const char *prefix, const char *suffix, const char *src, int line);
FILE *mutt_file_mkstemp_full (const char *file, int line, const char *func);
void  mutt_mktemp_full       (char *s, size_t slen, const char *prefix, const char *suffix, const char *src, int line);

#define mutt_mktemp(buf, buflen)                         mutt_mktemp_pfx_sfx(buf, buflen, "neomutt", NULL)
#define mutt_mktemp_pfx_sfx(buf, buflen, prefix, suffix) mutt_mktemp_full(buf, buflen, prefix, suffix, __FILE__, __LINE__)

#define mutt_buffer_mktemp(buf)                         mutt_buffer_mktemp_pfx_sfx(buf, "neomutt", NULL)
#define mutt_buffer_mktemp_pfx_sfx(buf, prefix, suffix) mutt_buffer_mktemp_full(buf, prefix, suffix, __FILE__, __LINE__)

#define mutt_file_mkstemp() mutt_file_mkstemp_full(__FILE__, __LINE__, __func__)

#endif /* MUTT_CORE_TMP_H */
