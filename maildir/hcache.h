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

#include "config.h"
#include <stdlib.h>
#include "core/lib.h"

struct Email;
struct EmailArray;
struct FilenameArray;
struct HeaderCache;
struct Progress;

const char *maildir_hcache_key   (struct Email *e);
size_t      maildir_hcache_keylen(const char *fn);

#ifdef USE_HCACHE

void                maildir_hcache_close (struct HeaderCache **ptr);
enum MxOpenReturns  maildir_hcache_delete(struct HeaderCache *hc, struct EmailArray *ea, const char *mbox_path, struct Progress *progress);
struct HeaderCache *maildir_hcache_open  (struct Mailbox *m);
enum MxOpenReturns  maildir_hcache_read  (struct HeaderCache *hc, const char *mbox_path, struct FilenameArray *fa, struct EmailArray *ea, struct Progress *progress);
enum MxOpenReturns  maildir_hcache_store (struct HeaderCache *hc, struct EmailArray *ea, size_t skip, const char *path, struct Progress *progress);

#else

static inline void                maildir_hcache_close (struct HeaderCache **ptr) {}
static inline enum MxOpenReturns  maildir_hcache_delete(struct HeaderCache *hc, struct EmailArray *ea, const char *mbox_path, struct Progress *progress) { return MX_OPEN_OK; }
static inline struct HeaderCache *maildir_hcache_open  (struct Mailbox *m) { return NULL; }
static inline int                 maildir_hcache_read  (struct HeaderCache *hc, const char *mbox_path, struct FilenameArray *fa, struct EmailArray *ea, struct Progress *progress) { return -1; }
static inline enum MxOpenReturns  maildir_hcache_store (struct HeaderCache *hc, struct EmailArray *ea, size_t skip, const char *path, struct Progress *progress) { return MX_OPEN_OK; }

#endif

#endif /* MUTT_MAILDIR_HCACHE_H */
