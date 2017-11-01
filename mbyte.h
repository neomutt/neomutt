/**
 * @file
 * Convert strings between multibyte and utf8 encodings
 *
 * @authors
 * Copyright (C) 2000 Edmund Grimley Evans <edmundo@rano.org>
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

#ifndef _MUTT_MBYTE_H
#define _MUTT_MBYTE_H

#include <stdbool.h>
#include <stddef.h>
#include <wchar.h>

void mutt_set_charset(char *charset);
extern bool Charset_is_utf8;
bool is_display_corrupting_utf8(wchar_t wc);
int mutt_filter_unprintable(char **s);

#endif /* _MUTT_MBYTE_H */
