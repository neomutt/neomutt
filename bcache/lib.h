/**
 * @file
 * Body Caching - local copies of email bodies
 *
 * @authors
 * Copyright (C) 2006-2007 Brendan Cully <brendan@kublai.com>
 * Copyright (C) 2006 Rocco Rutte <pdmef@gmx.net>
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

#ifndef MUTT_BCACHE_LIB_H
#define MUTT_BCACHE_LIB_H

#include <stdio.h>

struct ConnAccount;
struct BodyCache;

/* These Config Variables are only used in bcache.c */
extern char *C_MessageCachedir;

/**
 * typedef bcache_list_t - Prototype for mutt_bcache_list() callback
 * @param bcache  Body Cache from mutt_bcache_open()
 * @param want_id Callback function called for each match
 * @param data    Data to pass to the callback function
 * @retval -1  Failure
 * @retval >=0 count of matching items
 *
 * mutt_bcache_list() will call this function once for each item in the cache.
 */
typedef int (*bcache_list_t)(const char *id, struct BodyCache *bcache, void *data);

void              mutt_bcache_close (struct BodyCache **bcache);
int               mutt_bcache_commit(struct BodyCache *bcache, const char *id);
int               mutt_bcache_del   (struct BodyCache *bcache, const char *id);
int               mutt_bcache_exists(struct BodyCache *bcache, const char *id);
FILE *            mutt_bcache_get   (struct BodyCache *bcache, const char *id);
int               mutt_bcache_list  (struct BodyCache *bcache, bcache_list_t want_id, void *data);
struct BodyCache *mutt_bcache_open  (struct ConnAccount *account, const char *mailbox);
FILE *            mutt_bcache_put   (struct BodyCache *bcache, const char *id);

#endif /* MUTT_BCACHE_LIB_H */
