/**
 * @file
 * Conversion to/from base64 encoding
 *
 * @authors
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

#ifndef _LIB_BASE64_H
#define _LIB_BASE64_H

#include <stdio.h>

extern const int Index_64[];

#define base64val(c) Index_64[(unsigned int) (c)]

size_t mutt_to_base64(char *out, const char *cin, size_t len, size_t olen);
int mutt_from_base64(char *out, const char *in);

#endif /* _LIB_BASE64_H */
