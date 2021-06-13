/**
 * @file
 * Notmuch-specific Account data
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
 * @page nm_adata Notmuch-specific Account data
 *
 * Notmuch-specific Account data
 */

#include "config.h"
#include <stddef.h>
#include "private.h"
#include "mutt/lib.h"
#include "core/lib.h"
#include "adata.h"

/**
 * nm_adata_free - Free the private Account data - Implements Account::adata_free()
 */
void nm_adata_free(void **ptr)
{
  struct NmAccountData *adata = *ptr;
  if (adata->db)
  {
    nm_db_free(adata->db);
    adata->db = NULL;
  }

  FREE(ptr);
}

/**
 * nm_adata_new - Allocate and initialise a new NmAccountData structure
 * @retval ptr New NmAccountData
 */
struct NmAccountData *nm_adata_new(void)
{
  struct NmAccountData *adata = mutt_mem_calloc(1, sizeof(struct NmAccountData));

  return adata;
}

/**
 * nm_adata_get - Get the Notmuch Account data
 * @param m Mailbox
 * @retval ptr  Success
 * @retval NULL Failure, not a Notmuch mailbox
 */
struct NmAccountData *nm_adata_get(struct Mailbox *m)
{
  if (!m || (m->type != MUTT_NOTMUCH))
    return NULL;

  struct Account *a = m->account;
  if (!a)
    return NULL;

  return a->adata;
}
