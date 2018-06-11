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

#ifndef _MUTT_BCACHE_H
#define _MUTT_BCACHE_H

#include <stdio.h>

struct Account;
struct BodyCache;

/**
 * Callback function for mutt_bcache_list
 */
typedef int bcache_list_t(const char *id, struct BodyCache *bcache, void *data);

void              mutt_bcache_close(struct BodyCache **bcache);
int               mutt_bcache_commit(struct BodyCache *bcache, const char *id);
int               mutt_bcache_del(struct BodyCache *bcache, const char *id);
int               mutt_bcache_exists(struct BodyCache *bcache, const char *id);
FILE *            mutt_bcache_get(struct BodyCache *bcache, const char *id);
int               mutt_bcache_list(struct BodyCache *bcache, bcache_list_t *want_id, void *data);
struct BodyCache *mutt_bcache_open(struct Account *account, const char *mailbox);
FILE *            mutt_bcache_put(struct BodyCache *bcache, const char *id);

#endif /* _MUTT_BCACHE_H */
