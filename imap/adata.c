/**
 * @file
 * Imap-specific Account data
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
 * @page imap_adata Account data
 *
 * Imap-specific Account data
 */

#include "config.h"
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "conn/lib.h"
#include "adata.h"
#include "lib.h"

/**
 * imap_timeout_observer - Notification that a timeout has occurred - Implements ::observer_t - @ingroup observer_api
 *
 * This function is triggered by SIGWINCH.
 */
static int imap_timeout_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_TIMEOUT)
    return 0;
  if (!nc->global_data)
    return -1;

  struct ImapAccountData *adata = nc->global_data;
  mutt_debug(LL_DEBUG5, "imap timeout start\n");

  time_t now = mutt_date_now();
  const short c_imap_keep_alive = cs_subset_number(NeoMutt->sub, "imap_keep_alive");

  if ((adata->state >= IMAP_AUTHENTICATED) && (now >= (adata->lastread + c_imap_keep_alive)))
  {
    mutt_debug(LL_DEBUG5, "imap_keep_alive\n");
    imap_check_mailbox(adata->mailbox, true);
  }

  mutt_debug(LL_DEBUG5, "imap timeout done\n");
  return 0;
}

/**
 * imap_adata_free - Free the private Account data - Implements Account::adata_free() - @ingroup account_adata_free
 */
void imap_adata_free(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct ImapAccountData *adata = *ptr;

  notify_observer_remove(NeoMutt->notify_timeout, imap_timeout_observer, adata);

  FREE(&adata->capstr);
  buf_dealloc(&adata->cmdbuf);
  FREE(&adata->buf);
  FREE(&adata->cmds);

  if (adata->conn)
  {
    if (adata->conn->close)
      adata->conn->close(adata->conn);
    FREE(&adata->conn);
  }

  FREE(ptr);
}

/**
 * imap_adata_new - Allocate and initialise a new ImapAccountData structure
 * @param a Account
 * @retval ptr New ImapAccountData
 */
struct ImapAccountData *imap_adata_new(struct Account *a)
{
  struct ImapAccountData *adata = mutt_mem_calloc(1, sizeof(struct ImapAccountData));
  adata->account = a;

  static unsigned char new_seqid = 'a';

  adata->seqid = new_seqid;
  const short c_imap_pipeline_depth = cs_subset_number(NeoMutt->sub, "imap_pipeline_depth");
  adata->cmdslots = c_imap_pipeline_depth + 2;
  adata->cmds = mutt_mem_calloc(adata->cmdslots, sizeof(*adata->cmds));

  if (++new_seqid > 'z')
    new_seqid = 'a';

  notify_observer_add(NeoMutt->notify_timeout, NT_TIMEOUT, imap_timeout_observer, adata);

  return adata;
}

/**
 * imap_adata_get - Get the Account data for this mailbox
 * @param m Mailbox
 * @retval ptr Private data
 */
struct ImapAccountData *imap_adata_get(struct Mailbox *m)
{
  if (!m || (m->type != MUTT_IMAP) || !m->account)
    return NULL;
  return m->account->adata;
}
