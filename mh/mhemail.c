/**
 * @file
 * Mh Email helper
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
 * @page mh_mdemail Mh Email helper
 *
 * Mh Email helper
 */

#include "config.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "mhemail.h"

/**
 * mh_entry_new - Create a new Mh entry
 * @retval ptr New Mh entry
 */
struct MhEmail *mh_entry_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct MhEmail));
}

/**
 * mh_entry_free - Free a Mh object
 * @param[out] ptr Mh to free
 */
void mh_entry_free(struct MhEmail **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct MhEmail *md = *ptr;
  FREE(&md->canon_fname);
  email_free(&md->email);

  FREE(ptr);
}

/**
 * mharray_clear - Free a Mh array
 * @param[out] mha Mh array to free
 */
void mharray_clear(struct MhEmailArray *mha)
{
  if (!mha)
    return;

  struct MhEmail **mdp = NULL;
  ARRAY_FOREACH(mdp, mha)
  {
    mh_entry_free(mdp);
  }

  ARRAY_FREE(mha);
}
