/**
 * @file
 * Maildir Email helper
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * @page maildir_mdemail Maildir Email helper
 *
 * Maildir Email helper
 */

#include "config.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "mdemail.h"

/**
 * maildir_entry_new - Create a new Maildir entry
 * @retval ptr New Maildir entry
 */
struct MdEmail *maildir_entry_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct MdEmail));
}

/**
 * maildir_entry_free - Free a Maildir object
 * @param[out] ptr Maildir to free
 */
void maildir_entry_free(struct MdEmail **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct MdEmail *md = *ptr;
  FREE(&md->canon_fname);
  email_free(&md->email);

  FREE(ptr);
}

/**
 * maildirarray_clear - Free a Maildir array
 * @param[out] mda Maildir array to free
 */
void maildirarray_clear(struct MdEmailArray *mda)
{
  if (!mda)
    return;

  struct MdEmail **mdp = NULL;
  ARRAY_FOREACH(mdp, mda)
  {
    maildir_entry_free(mdp);
  }

  ARRAY_FREE(mda);
}
