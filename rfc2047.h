/**
 * @file
 * RFC2047 MIME extensions routines
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
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

#ifndef _MUTT_RFC2047_H
#define _MUTT_RFC2047_H

#include <stddef.h>

struct Address;

char *mutt_choose_charset(const char *fromcode, const char *charsets, char *u,
                          size_t ulen, char **d, size_t *dlen);
int convert_nonmime_string(char **ps);

void _rfc2047_encode_string(char **pd, int encode_specials, int col);
void rfc2047_encode_adrlist(struct Address *addr, const char *tag);

#define rfc2047_encode_string(a) _rfc2047_encode_string(a, 0, 32);

void rfc2047_decode(char **pd);
void rfc2047_decode_adrlist(struct Address *a);

#endif /* _MUTT_RFC2047_H */
