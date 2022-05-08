/**
 * @file
 * Conversion between different character encodings
 *
 * @authors
 * Copyright (C) 2022 Michal Siedlaczek <michal@siedlaczek.me>
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

/**
 * @page lib_convert File Charset Conversion
 *
 * Convert files between different character encodings.
 *
 * | File                      | Description                     |
 * | :------------------------ | :------------------------------ |
 * | convert/convert.c         | @subpage convert_convert        |
 * | convert/content_info.c    | @subpage convert_content_info   |
 */

#ifndef MUTT_CONVERT_LIB_H
#define MUTT_CONVERT_LIB_H

#include <wchar.h>
#include "config/lib.h"

struct Body;
struct Content;
struct ContentState;
struct Slist;

size_t          mutt_convert_file_from_to(FILE *fp, const struct Slist *fromcodes, const struct Slist *tocodes, char **fromcode, char **tocode, struct Content *info);
size_t          mutt_convert_file_to(FILE *fp, const char *fromcode, struct Slist const *const tocodes, int *tocode, struct Content *info);
struct Content *mutt_get_content_info(const char *fname, struct Body *b, struct ConfigSubset *sub);
void            mutt_update_content_info(struct Content *info, struct ContentState *s, char *buf, size_t buflen);

#endif /* MUTT_CONVERT_LIB_H */
