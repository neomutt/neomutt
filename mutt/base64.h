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

#ifndef MUTT_LIB_BASE64_H
#define MUTT_LIB_BASE64_H

#include <stdio.h>

struct Buffer;

extern const int Index64[];

#define base64val(ch) Index64[(unsigned int) (ch)]

int    mutt_b64_decode(const char *in, char *out, size_t olen);
size_t mutt_b64_encode(const char *in, size_t inlen, char *out, size_t outlen);

int    mutt_b64_buffer_decode(struct Buffer *buf, const char *in);
size_t mutt_b64_buffer_encode(struct Buffer *buf, const char *in, size_t len);

#endif /* MUTT_LIB_BASE64_H */
