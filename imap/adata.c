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
 * @page imap_adata Imap-specific Account data
 *
 * Imap-specific Account data
 */

#include "config.h"
#include "private.h"
#include "mutt/lib.h"
#include "core/lib.h"
#include "conn/lib.h"
#include "adata.h"

/**
 * imap_adata_free - Free the private Account data - Implements Account::adata_free()
 */
void imap_adata_free(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct ImapAccountData *adata = *ptr;

  FREE(&adata->capstr);
  mutt_buffer_dealloc(&adata->cmdbuf);
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
 * @retval ptr New ImapAccountData
 */
struct ImapAccountData *imap_adata_new(struct Account *a)
{
  struct ImapAccountData *adata = mutt_mem_calloc(1, sizeof(struct ImapAccountData));
  adata->account = a;

  static unsigned char new_seqid = 'a';

  adata->seqid = new_seqid;
  adata->cmdslots = C_ImapPipelineDepth + 2;
  adata->cmds = mutt_mem_calloc(adata->cmdslots, sizeof(*adata->cmds));

  if (++new_seqid > 'z')
    new_seqid = 'a';

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
