/**
 * @file
 * Maildir Header Cache
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_MAILDIR_HCACHE_H
#define MUTT_MAILDIR_HCACHE_H

#include <stdlib.h>

struct Email;
struct HeaderCache;
struct Mailbox;

#ifdef USE_HCACHE

void                maildir_hcache_close (struct HeaderCache **ptr);
int                 maildir_hcache_delete(struct HeaderCache *hc, struct Email *e);
struct HeaderCache *maildir_hcache_open  (struct Mailbox *m);
struct Email *      maildir_hcache_read  (struct HeaderCache *hc, struct Email *e, const char *fn);
int                 maildir_hcache_store (struct HeaderCache *hc, struct Email *e);

#else

static inline void                maildir_hcache_close (struct HeaderCache **ptr) {}
static inline int                 maildir_hcache_delete(struct HeaderCache *hc, struct Email *e) { return 0; }
static inline struct HeaderCache *maildir_hcache_open  (struct Mailbox *m) { return NULL; }
static inline struct Email *      maildir_hcache_read  (struct HeaderCache *hc, struct Email *e, const char *fn) { return NULL; }
static inline int                 maildir_hcache_store (struct HeaderCache *hc, struct Email *e) { return 0; }

#endif

#endif /* MUTT_MAILDIR_HCACHE_H */
