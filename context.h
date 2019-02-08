/**
 * @file
 * The "currently-open" mailbox
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_CONTEXT_H
#define MUTT_CONTEXT_H

#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include "mailbox.h"

struct EmailList;

/**
 * struct Context - The "current" mailbox
 */
struct Context
{
  off_t vsize;
  char *pattern;                 /**< limit pattern string */
  struct Pattern *limit_pattern; /**< compiled limit pattern */
  struct Email *last_tag;  /**< last tagged msg. used to link threads */
  struct MuttThread *tree;  /**< top of thread tree */
  struct Hash *thread_hash; /**< hash table for threading */
  int msgnotreadyet;        /**< which msg "new" in pager, -1 if none */

  struct Menu *menu; /**< needed for pattern compilation */

  bool collapsed : 1; /**< are all threads collapsed? */

  struct Mailbox *mailbox;
};

void ctx_free(struct Context **ctx);
void ctx_mailbox_changed(struct Mailbox *m, enum MailboxNotification action);
void ctx_update(struct Context *ctx);
void ctx_update_tables(struct Context *ctx, bool committing);

bool message_is_tagged(struct Context *ctx, int index);
bool message_is_visible(struct Context *ctx, int index);

int  el_add_email(struct EmailList *el, struct Email *e);
int  el_add_tagged(struct EmailList *el, struct Context *ctx, struct Email *e, bool use_tagged);
void el_free(struct EmailList *el);

#endif /* MUTT_CONTEXT_H */
