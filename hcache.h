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

void *mutt_hcache_open(const char *path, const char *folder);
void mutt_hcache_close(void *db);
HEADER *mutt_hcache_restore(const unsigned char *d, HEADER **oh);
void *mutt_hcache_fetch(void *db, const char *filename, size_t (*keylen)(const char *fn));
void *mutt_hcache_fetch_raw (void *db, const char *filename,
                             size_t (*keylen)(const char *fn));
int mutt_hcache_store(void *db, const char *filename, HEADER *h,
                      unsigned long uid_validity, size_t (*keylen)(const char *fn));
int mutt_hcache_store_raw (void* db, const char* filename, char* data,
                           size_t dlen, size_t(*keylen) (const char* fn));
int mutt_hcache_delete(void *db, const char *filename, size_t (*keylen)(const char *fn));

#endif /* _HCACHE_H_ */
