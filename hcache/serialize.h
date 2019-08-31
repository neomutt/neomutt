/**
 * @file
 * Email-object serialiser
 *
 * @authors
 * Copyright (C) 2004 Thomas Glanzmann <sithglan@stud.uni-erlangen.de>
 * Copyright (C) 2004 Tobias Werth <sitowert@stud.uni-erlangen.de>
 * Copyright (C) 2004 Brian Fundakowski Feldman <green@FreeBSD.org>
 * Copyright (C) 2016 Pietro Cerutti <gahr@gahr.ch>
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

#ifndef MUTT_HCACHE_SERIALIZE_H
#define MUTT_HCACHE_SERIALIZE_H

#include <stdbool.h>
#include <sys/types.h>
#include "hcache.h"

struct Address;
struct Body;
struct Buffer;
struct Envelope;
struct Email;
struct ListHead;
struct ParameterList;

unsigned char *serial_dump_address(struct Address *a, unsigned char *d, int *off, bool convert);
unsigned char *serial_dump_body(struct Body *c, unsigned char *d, int *off, bool convert);
unsigned char *serial_dump_buffer(struct Buffer *buf, unsigned char *d, int *off, bool convert);
unsigned char *serial_dump_char(char *c, unsigned char *d, int *off, bool convert);
unsigned char *serial_dump_char_size(char *c, unsigned char *d, int *off, ssize_t size, bool convert);
unsigned char *serial_dump_envelope(struct Envelope *e, unsigned char *d, int *off, bool convert);
unsigned char *serial_dump_int(unsigned int i, unsigned char *d, int *off);
unsigned char *serial_dump_size_t(size_t s, unsigned char *d, int *off);
unsigned char *serial_dump_parameter(struct ParameterList *pl, unsigned char *d, int *off, bool convert);
unsigned char *serial_dump_stailq(struct ListHead *l, unsigned char *d, int *off, bool convert);

void           serial_restore_address(struct Address **a, const unsigned char *d, int *off, bool convert);
void           serial_restore_body(struct Body *c, const unsigned char *d, int *off, bool convert);
void           serial_restore_buffer(struct Buffer *buf, const unsigned char *d, int *off, bool convert);
void           serial_restore_char(char **c, const unsigned char *d, int *off, bool convert);
void           serial_restore_envelope(struct Envelope *e, const unsigned char *d, int *off, bool convert);
void           serial_restore_int(unsigned int *i, const unsigned char *d, int *off);
void           serial_restore_parameter(struct ParameterList *pl, const unsigned char *d, int *off, bool convert);
void           serial_restore_stailq(struct ListHead *l, const unsigned char *d, int *off, bool convert);

void *        mutt_hcache_dump(header_cache_t *hc, const struct Email *e, int *off, size_t uidvalidity);
struct Email *mutt_hcache_restore(const unsigned char *d);

#endif /* MUTT_HCACHE_SERIALIZE_H */
