/**
 * @file
 * Data shared between Index, Pager and Sidebar
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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

/**
 * @page index_shared_data Data shared between Index, Pager and Sidebar
 *
 * Data shared between Index, Pager and Sidebar
 */

#include "config.h"
#include <stdbool.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "shared_data.h"
#include "lib.h"
#include "context.h"
#include "mutt_globals.h"

/**
 * index_shared_context_observer - Notification that the Context has changed - Implements ::observer_t
 */
static int index_shared_context_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_CONTEXT) || !nc->global_data || !nc->event_data)
    return -1;

  struct EventContext *ev_c = nc->event_data;
  struct IndexSharedData *shared = nc->global_data;

  if (nc->event_subtype != NT_CONTEXT_DELETE)
    return 0;

  if (ev_c->ctx != shared->ctx)
    return 0;

  struct IndexSharedData old_shared = *shared;
  shared->ctx = NULL;

  // Relay the message
  mutt_debug(LL_NOTIFY, "NT_INDEX_CONTEXT: %p\n", shared->ctx);
  notify_send(shared->notify, NT_INDEX, NT_INDEX_CONTEXT, &old_shared);
  return 0;
}

/**
 * index_shared_account_observer - Notification that an Account has changed - Implements ::observer_t
 */
static int index_shared_account_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_ACCOUNT) || !nc->global_data || !nc->event_data)
    return -1;

  struct EventAccount *ev_a = nc->event_data;
  struct IndexSharedData *shared = nc->global_data;

  if (nc->event_subtype != NT_ACCOUNT_DELETE)
    return 0;

  if (ev_a->account != shared->account)
    return 0;

  struct IndexSharedData old_shared = *shared;
  shared->account = NULL;
  shared->mailbox = NULL;
  shared->email = NULL;

  // Relay the message
  mutt_debug(LL_NOTIFY, "NT_INDEX_ACCOUNT: %p, NT_INDEX_MAILBOX: %p, NT_INDEX_EMAIL: %p\n",
             shared->account, shared->mailbox, shared->email);
  notify_send(shared->notify, NT_INDEX,
              NT_INDEX_ACCOUNT | NT_INDEX_MAILBOX | NT_INDEX_EMAIL, &old_shared);
  return 0;
}

/**
 * index_shared_mailbox_observer - Notification that a Mailbox has changed - Implements ::observer_t
 */
static int index_shared_mailbox_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_MAILBOX) || !nc->global_data || !nc->event_data)
    return -1;

  struct EventMailbox *ev_m = nc->event_data;
  struct IndexSharedData *shared = nc->global_data;

  if (nc->event_subtype != NT_MAILBOX_DELETE)
    return 0;

  if (ev_m->mailbox != shared->mailbox)
    return 0;

  struct IndexSharedData old_shared = *shared;
  shared->mailbox = NULL;
  shared->email = NULL;

  // Relay the message
  mutt_debug(LL_NOTIFY, "NT_INDEX_MAILBOX: %p, NT_INDEX_EMAIL: %p\n",
             shared->mailbox, shared->email);
  notify_send(shared->notify, NT_INDEX, NT_INDEX_MAILBOX | NT_INDEX_EMAIL, &old_shared);
  return 0;
}

/**
 * index_shared_email_observer - Notification that an Email has changed - Implements ::observer_t
 */
static int index_shared_email_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_EMAIL) || !nc->global_data || !nc->event_data)
    return -1;

  struct EventEmail *ev_e = nc->event_data;
  struct IndexSharedData *shared = nc->global_data;

  if (nc->event_subtype != NT_EMAIL_DELETE)
    return 0;

  for (int i = 0; i < ev_e->num_emails; i++)
  {
    if (ev_e->emails[i] == shared->email)
    {
      struct IndexSharedData old_shared = *shared;
      shared->email = NULL;

      // Relay the message
      mutt_debug(LL_NOTIFY, "NT_INDEX_EMAIL: %p\n", shared->email);
      notify_send(shared->notify, NT_INDEX, NT_INDEX_EMAIL, &old_shared);
      return 0;
    }
  }

  return 0;
}

/**
 * index_shared_data_set_context - Set the Context for the Index and friends
 * @param shared Shared Index data
 * @param ctx    New Context, may be NULL
 */
void index_shared_data_set_context(struct IndexSharedData *shared, struct Context *ctx)
{
  if (!shared)
    return;

  struct IndexSharedData old_shared = *shared;

  NotifyIndex subtype = NT_INDEX_NO_FLAGS;

  if (shared->ctx != ctx)
  {
    shared->ctx = ctx;
    subtype |= NT_INDEX_CONTEXT;

    Context = ctx;
  }

  struct Mailbox *m = ctx_mailbox(ctx);
  if (shared->mailbox != m)
  {
    shared->mailbox = m;
    shared->email = NULL;
    shared->email_seq = 0;
    subtype |= NT_INDEX_MAILBOX | NT_INDEX_EMAIL;
  }

  struct Account *a = m ? m->account : NULL;
  if (shared->account != a)
  {
    shared->account = a;
    subtype |= NT_INDEX_ACCOUNT;
  }

  struct ConfigSubset *sub = NeoMutt->sub;
  if (m)
    sub = m->sub;
  else if (a)
    sub = a->sub;
  if (shared->sub != sub)
  {
    shared->sub = sub;
    subtype |= NT_INDEX_SUBSET;
  }

  if (subtype != NT_INDEX_NO_FLAGS)
  {
    mutt_debug(LL_NOTIFY, "NT_INDEX: %p\n", shared);
    notify_send(shared->notify, NT_INDEX, subtype, &old_shared);
  }
}

/**
 * index_shared_data_set_email - Set the current Email for the Index and friends
 * @param shared Shared Index data
 * @param e      Current Email, may be NULL
 */
void index_shared_data_set_email(struct IndexSharedData *shared, struct Email *e)
{
  if (!shared)
    return;

  struct IndexSharedData old_shared = *shared;

  size_t seq = e ? e->sequence : 0;
  if ((shared->email != e) || (shared->email_seq != seq))
  {
    shared->email = e;
    shared->email_seq = seq;

    mutt_debug(LL_NOTIFY, "NT_INDEX_EMAIL: %p\n", shared->email);
    notify_send(shared->notify, NT_INDEX, NT_INDEX_EMAIL, &old_shared);
  }
}

/**
 * index_shared_data_is_cur_email - Check whether an email is the currently selected Email
 * @param shared Shared Index data
 * @param e      Email to check
 * @retval true  e is current
 * @retval false e is not current
 */
bool index_shared_data_is_cur_email(const struct IndexSharedData *shared,
                                    const struct Email *e)
{
  if (!shared)
    return false;

  return shared->email_seq == e->sequence;
}

/**
 * index_shared_data_free - Free Index Data
 * @param win Window
 * @param ptr Index Data to free
 *
 * Only `notify` is owned by IndexSharedData and should be freed.
 */
void index_shared_data_free(struct MuttWindow *win, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct IndexSharedData *shared = *ptr;

  mutt_debug(LL_NOTIFY, "NT_INDEX_DELETE: %p\n", shared);
  notify_send(shared->notify, NT_INDEX, NT_INDEX_DELETE, shared);
  notify_free(&shared->notify);

  notify_observer_remove(NeoMutt->notify, index_shared_context_observer, shared);
  notify_observer_remove(NeoMutt->notify, index_shared_account_observer, shared);
  notify_observer_remove(NeoMutt->notify, index_shared_mailbox_observer, shared);
  notify_observer_remove(NeoMutt->notify, index_shared_email_observer, shared);

  FREE(ptr);
}

/**
 * index_shared_data_new - Create new Index Data
 * @retval ptr New IndexSharedData
 */
struct IndexSharedData *index_shared_data_new(void)
{
  struct IndexSharedData *shared = mutt_mem_calloc(1, sizeof(struct IndexSharedData));

  shared->notify = notify_new();
  shared->sub = NeoMutt->sub;

  notify_observer_add(NeoMutt->notify, NT_CONTEXT, index_shared_context_observer, shared);
  notify_observer_add(NeoMutt->notify, NT_ACCOUNT, index_shared_account_observer, shared);
  notify_observer_add(NeoMutt->notify, NT_MAILBOX, index_shared_mailbox_observer, shared);
  notify_observer_add(NeoMutt->notify, NT_EMAIL, index_shared_email_observer, shared);

  mutt_debug(LL_NOTIFY, "NT_INDEX_ADD: %p\n", shared);
  notify_send(shared->notify, NT_INDEX, NT_INDEX_ADD, shared);

  return shared;
}
