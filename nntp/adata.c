/**
 * @file
 * Nntp-specific Account data
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
 * @page nntp_adata Nntp-specific Account data
 *
 * Nntp-specific Account data
 */

#include "config.h"
#include "private.h"
#include "mutt/lib.h"
#include "adata.h"

struct Connection;

/**
 * nntp_adata_free - Free the private Account data - Implements Account::adata_free()
 *
 * The NntpAccountData struct stores global NNTP data, such as the connection to
 * the database.  This function will close the database, free the resources and
 * the struct itself.
 */
void nntp_adata_free(void **ptr)
{
  struct NntpAccountData *adata = *ptr;

  mutt_file_fclose(&adata->fp_newsrc);
  FREE(&adata->newsrc_file);
  FREE(&adata->authenticators);
  FREE(&adata->overview_fmt);
  FREE(&adata->conn);
  FREE(&adata->groups_list);
  mutt_hash_free(&adata->groups_hash);
  FREE(ptr);
}

/**
 * nntp_adata_new - Allocate and initialise a new NntpAccountData structure
 * @param conn Network connection
 * @retval ptr New NntpAccountData
 */
struct NntpAccountData *nntp_adata_new(struct Connection *conn)
{
  struct NntpAccountData *adata = mutt_mem_calloc(1, sizeof(struct NntpAccountData));
  adata->conn = conn;
  adata->groups_hash = mutt_hash_new(1009, MUTT_HASH_NO_FLAGS);
  mutt_hash_set_destructor(adata->groups_hash, nntp_hashelem_free, 0);
  adata->groups_max = 16;
  adata->groups_list = mutt_mem_malloc(adata->groups_max * sizeof(struct NntpMboxData *));
  return adata;
}

#if 0
/**
 * nntp_adata_get - Get the Account data for this mailbox
 * @retval ptr Private Account data
 */
struct NntpAccountData *nntp_adata_get(struct Mailbox *m)
{
  if (!m || (m->type != MUTT_NNTP))
    return NULL;
  struct Account *a = m->account;
  if (!a)
    return NULL;
  return a->adata;
}
#endif
