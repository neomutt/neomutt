/*
 * Copyright (C) 2004 Thomas Glanzmann <sithglan@stud.uni-erlangen.de>
 * Copyright (C) 2004 Tobias Werth <sitowert@stud.uni-erlangen.de>
 * Copyright (C) 2004 Brian Fundakowski Feldman <green@FreeBSD.org>
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
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _HCACHE_H_
#define _HCACHE_H_ 1

struct header_cache;
typedef struct header_cache header_cache_t;

typedef int (*hcache_namer_t)(const char* path, char* dest, size_t dlen);

header_cache_t *mutt_hcache_open(const char *path, const char *folder,
  hcache_namer_t namer);
void mutt_hcache_close(header_cache_t *h);
HEADER *mutt_hcache_restore(const unsigned char *d, HEADER **oh);
void *mutt_hcache_fetch(header_cache_t *h, const char *filename, size_t (*keylen)(const char *fn));
void *mutt_hcache_fetch_raw (header_cache_t *h, const char *filename,
                             size_t (*keylen)(const char *fn));

typedef enum {
  M_GENERATE_UIDVALIDITY = 1 /* use gettimeofday() as value */
} mutt_hcache_store_flags_t;

/* uidvalidity is an IMAP-specific unsigned 32 bit number */
int mutt_hcache_store(header_cache_t *h, const char *filename, HEADER *header,
                      unsigned int uidvalidity, size_t (*keylen)(const char *fn), mutt_hcache_store_flags_t flags_t);
int mutt_hcache_store_raw (header_cache_t *h, const char* filename, void* data,
                           size_t dlen, size_t(*keylen) (const char* fn));
int mutt_hcache_delete(header_cache_t *h, const char *filename, size_t (*keylen)(const char *fn));

const char *mutt_hcache_backend (void);

#endif /* _HCACHE_H_ */
