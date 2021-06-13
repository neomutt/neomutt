/**
 * @file
 * Pop-specific Account data
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
 * @page pop_adata Pop-specific Account data
 *
 * Pop-specific Account data
 */

#include "config.h"
#include "core/lib.h"
#include "adata.h"

/**
 * pop_adata_free - Free the private Account data - Implements Account::adata_free()
 *
 * The PopAccountData struct stores global POP data, such as the connection to
 * the database.  This function will close the database, free the resources and
 * the struct itself.
 */
void pop_adata_free(void **ptr)
{
  struct PopAccountData *adata = *ptr;
  FREE(&adata->auth_list.data);
  FREE(ptr);
}

/**
 * pop_adata_new - Create a new PopAccountData object
 * @retval ptr New PopAccountData struct
 */
struct PopAccountData *pop_adata_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct PopAccountData));
}

/**
 * pop_adata_get - Get the Account data for this mailbox
 * @param m Mailbox
 * @retval ptr PopAccountData
 */
struct PopAccountData *pop_adata_get(struct Mailbox *m)
{
  if (!m || (m->type != MUTT_POP))
    return NULL;
  struct Account *a = m->account;
  if (!a)
    return NULL;
  return a->adata;
}
