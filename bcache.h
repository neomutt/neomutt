/*
 * Copyright (C) 2006-2007 Brendan Cully <brendan@kublai.com>
 * Copyright (C) 2006 Rocco Rutte <pdmef@gmx.net>
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
#ifndef _BCACHE_H_
#define _BCACHE_H_ 1

/*
 * support for body cache
 */

struct body_cache;
typedef struct body_cache body_cache_t;

/*
 * Parameters:
 *   - 'account' is the current mailbox' account (required)
 *   - 'mailbox' is the path to the mailbox of the account (optional):
 *     the driver using it is responsible for ensuring that hierarchies
 *     are separated by '/' (if it knows of such a concepts like
 *     mailboxes or hierarchies)
 * Returns NULL on failure.
 */
body_cache_t *mutt_bcache_open (ACCOUNT *account, const char *mailbox);

/* free all memory of bcache and finally FREE() it, too */
void mutt_bcache_close (body_cache_t **bcache);

/*
 * Parameters:
 *   - 'bcache' is the pointer returned by mutt_bcache_open() (required)
 *   - 'id' is a per-mailbox unique identifier for the message (required)
 * These return NULL/-1 on failure and FILE pointer/0 on success.
 */

FILE* mutt_bcache_get(body_cache_t *bcache, const char *id);
/* tmp: the returned FILE* is in a temporary location.
 *      if set, use mutt_bcache_commit to put it into place */
FILE* mutt_bcache_put(body_cache_t *bcache, const char *id, int tmp);
int mutt_bcache_commit(body_cache_t *bcache, const char *id);
int mutt_bcache_move(body_cache_t *bcache, const char *id, const char *newid);
int mutt_bcache_del(body_cache_t *bcache, const char *id);
int mutt_bcache_exists(body_cache_t *bcache, const char *id);

/*
 * This more or less "examines" the cache and calls a function with
 * each id it finds if given.
 *
 * The optional callback function gets the id of a message, the very same
 * body cache handle mutt_bcache_list() is called with (to, perhaps,
 * perform further operations on the bcache), and a data cookie which is
 * just passed as-is. If the return value of the callback is non-zero, the
 * listing is aborted and continued otherwise. The callback is optional
 * so that this function can be used to count the items in the cache
 * (see below for return value).
 *
 * This returns -1 on failure and the count (>=0) of items processed
 * otherwise.
 */
int mutt_bcache_list(body_cache_t *bcache,
		     int (*want_id)(const char *id, body_cache_t *bcache,
				    void *data), void *data);

#endif /* _BCACHE_H_ */
