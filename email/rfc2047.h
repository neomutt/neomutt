/**
 * @file
 * RFC2047 MIME extensions encoding / decoding routines
 *
 * @authors
 * Copyright (C) 1996-2000,2010 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2000-2002 Edmund Grimley Evans <edmundo@rano.org>
 * Copyright (C) 2018-2019 Pietro Cerutti <gahr@gahr.ch>
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

#ifndef MUTT_EMAIL_RFC2047_H
#define MUTT_EMAIL_RFC2047_H

struct AddressList;
struct Envelope;
struct Slist;

void rfc2047_decode(char **pd);
void rfc2047_encode(char **pd, const char *specials, int col, const struct Slist *charsets);

void rfc2047_decode_addrlist(struct AddressList *al);
void rfc2047_encode_addrlist(struct AddressList *al, const char *tag);
void rfc2047_decode_envelope(struct Envelope *env);
void rfc2047_encode_envelope(struct Envelope *env);

#endif /* MUTT_EMAIL_RFC2047_H */
