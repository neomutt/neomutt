/**
 * @file
 * Representation of an email address
 *
 * @authors
 * Copyright (C) 1996-2000,2011-2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#include "config.h"
#include <stdio.h>
#include "mutt/mutt.h"
#include "address.h"

/**
 * mutt_addrlist_to_intl - Convert an Address list to Punycode
 * @param[in]  a   Address list to modify
 * @param[out] err Pointer for failed addresses
 * @retval 0  Success, all addresses converted
 * @retval -1 Error, err will be set to the failed address
 */
int mutt_addrlist_to_intl(struct Address *a, char **err)
{
  char *user = NULL, *domain = NULL;
  char *intl_mailbox = NULL;
  int rc = 0;

  if (err)
    *err = NULL;

  for (; a; a = a->next)
  {
    if (!a->mailbox || mutt_addr_is_intl(a))
      continue;

    if (mutt_addr_mbox_to_udomain(a->mailbox, &user, &domain) == -1)
      continue;

    intl_mailbox = mutt_idna_local_to_intl(user, domain);
    if (!intl_mailbox)
    {
      rc = -1;
      if (err && !*err)
        *err = mutt_str_strdup(a->mailbox);
      continue;
    }

    mutt_addr_set_intl(a, intl_mailbox);
  }

  return rc;
}

/**
 * mutt_addrlist_to_local - Convert an Address list from Punycode
 * @param a Address list to modify
 * @retval 0 Always
 */
int mutt_addrlist_to_local(struct Address *a)
{
  char *user = NULL, *domain = NULL;
  char *local_mailbox = NULL;

  for (; a; a = a->next)
  {
    if (!a->mailbox || mutt_addr_is_local(a))
      continue;

    if (mutt_addr_mbox_to_udomain(a->mailbox, &user, &domain) == -1)
      continue;

    local_mailbox = mutt_idna_intl_to_local(user, domain, 0);
    if (local_mailbox)
      mutt_addr_set_local(a, local_mailbox);
  }

  return 0;
}
