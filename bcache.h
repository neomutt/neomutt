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
 * mutt_bcache_open - Open an Email-Body Cache
 * @param account current mailbox' account (required)
 * @param mailbox path to the mailbox of the account (optional)
 * @retval NULL on failure
 *
 * The driver using it is responsible for ensuring that hierarchies are
 * separated by '/' (if it knows of such a concepts like mailboxes or
 * hierarchies)
 */
struct BodyCache *mutt_bcache_open(struct Account *account, const char *mailbox);

/**
 * mutt_bcache_close - Close an Email-Body Cache
 * @param bcache Body cache
 *
 * Free all memory of bcache and finally FREE() it, too.
 */
void mutt_bcache_close(struct BodyCache **bcache);

/**
 * mutt_bcache_get - Open a file in the Body Cache
 * @param bcache Body Cache from mutt_bcache_open()
 * @param id     Per-mailbox unique identifier for the message
 * @retval FILE* on success
 * @retval NULL  on failure
 */
FILE *mutt_bcache_get(struct BodyCache *bcache, const char *id);

/**
 * mutt_bcache_put - Create a file in the Body Cache
 * @param bcache Body Cache from mutt_bcache_open()
 * @param id     Per-mailbox unique identifier for the message
 * @param tmp    Returned FILE* is in a temporary location
 *               If set, use mutt_bcache_commit to put it into place
 * @retval FILE* on success
 * @retval NULL on failure
 */
FILE *mutt_bcache_put(struct BodyCache *bcache, const char *id, int tmp);

/**
 * mutt_bcache_commit - Move a temporary file into the Body Cache
 * @param bcache Body Cache from mutt_bcache_open()
 * @param id     Per-mailbox unique identifier for the message
 * @retval 0 on success
 * @retval -1 on failure
 */
int mutt_bcache_commit(struct BodyCache *bcache, const char *id);

/**
 * mutt_bcache_del - Delete a file from the Body Cache
 * @param bcache Body Cache from mutt_bcache_open()
 * @param id     Per-mailbox unique identifier for the message
 * @retval 0 on success
 * @retval -1 on failure
 */
int mutt_bcache_del(struct BodyCache *bcache, const char *id);

/**
 * mutt_bcache_exists - Check if a file exists in the Body Cache
 * @param bcache Body Cache from mutt_bcache_open()
 * @param id     Per-mailbox unique identifier for the message
 * @retval 0 on success
 * @retval -1 on failure
 */
int mutt_bcache_exists(struct BodyCache *bcache, const char *id);

/**
 * mutt_bcache_list - Find matching entries in the Body Cache
 * @param bcache Body Cache from mutt_bcache_open()
 * @param want_id Callback function called for each match
 * @param data    Data to pass to the callback function
 * @retval -1  on failure
 * @retval >=0 count of matching items
 *
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
 */
int mutt_bcache_list(struct BodyCache *bcache,
                     int (*want_id)(const char *id, struct BodyCache *bcache, void *data),
                     void *data);

#endif /* _MUTT_BCACHE_H */
