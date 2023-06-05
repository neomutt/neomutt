/**
 * @file
 * Pop-specific Email data
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
 * @page pop_edata Pop-specific Email data
 *
 * Pop-specific Email data
 */

#include "config.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "edata.h"

/**
 * pop_edata_free - Free the private Email data - Implements Email::edata_free()
 *
 * Each email has an attached PopEmailData, which contains things like the tags
 * (labels).
 */
void pop_edata_free(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct PopEmailData *edata = *ptr;
  FREE(&edata->uid);
  FREE(ptr);
}

/**
 * pop_edata_new - Create a new PopEmailData for an email
 * @param uid Email UID
 * @retval ptr New PopEmailData struct
 */
struct PopEmailData *pop_edata_new(const char *uid)
{
  struct PopEmailData *edata = mutt_mem_calloc(1, sizeof(struct PopEmailData));
  edata->uid = mutt_str_dup(uid);
  return edata;
}

/**
 * pop_edata_get - Get the private data for this Email
 * @param e Email
 * @retval ptr Private Email data
 */
struct PopEmailData *pop_edata_get(struct Email *e)
{
  if (!e)
    return NULL;
  return e->edata;
}
