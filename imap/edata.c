/**
 * @file
 * Imap-specific Email data
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2022 Pietro Cerutti <gahr@gahr.ch>
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
 * @page imap_edata Imap-specific Email data
 *
 * Imap-specific Email data
 */

#include "config.h"
#include <stddef.h>
#include <string.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "edata.h"

/**
 * imap_edata_free - Free the private Email data - Implements Email::edata_free() - @ingroup email_edata_free
 */
void imap_edata_free(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct ImapEmailData *edata = *ptr;
  /* this should be safe even if the list wasn't used */
  FREE(&edata->flags_system);
  FREE(&edata->flags_remote);

  FREE(ptr);
}

/**
 * imap_edata_new - Create a new ImapEmailData
 * @retval ptr New ImapEmailData
 */
struct ImapEmailData *imap_edata_new(void)
{
  return MUTT_MEM_CALLOC(1, struct ImapEmailData);
}

/**
 * imap_edata_get - Get the private data for this Email
 * @param e Email
 * @retval ptr Private Email data
 */
struct ImapEmailData *imap_edata_get(struct Email *e)
{
  if (!e)
    return NULL;
  return e->edata;
}

/**
 * imap_edata_clone - Clone an ImapEmailData
 * @param src The source ImapEmailData to clone
 * @retval ptr New ImapEmailData
 */
struct ImapEmailData *imap_edata_clone(struct ImapEmailData *src)
{
  struct ImapEmailData *dst = imap_edata_new();
  memcpy(dst, src, sizeof(*src));
  dst->flags_system = mutt_str_dup(src->flags_system);
  dst->flags_remote = mutt_str_dup(src->flags_remote);
  return dst;
}
