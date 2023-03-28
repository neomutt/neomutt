/**
 * @file
 * Handle mailing lists
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
 * @page neo_maillist Handle mailing lists
 *
 * Handle mailing lists
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "email/lib.h"
#include "maillist.h"
#include "muttlib.h"
#include "sort.h"

/**
 * mutt_is_mail_list - Is this the email address of a mailing list? - Implements ::addr_predicate_t - @ingroup addr_predicate_api
 * @param addr Address to test
 * @retval true It's a mailing list
 */
bool mutt_is_mail_list(const struct Address *addr)
{
  if (!mutt_regexlist_match(&UnMailLists, buf_string(addr->mailbox)))
    return mutt_regexlist_match(&MailLists, buf_string(addr->mailbox));
  return false;
}

/**
 * mutt_is_subscribed_list - Is this the email address of a user-subscribed mailing list? - Implements ::addr_predicate_t - @ingroup addr_predicate_api
 * @param addr Address to test
 * @retval true It's a subscribed mailing list
 */
bool mutt_is_subscribed_list(const struct Address *addr)
{
  if (!mutt_regexlist_match(&UnMailLists, buf_string(addr->mailbox)) &&
      !mutt_regexlist_match(&UnSubscribedLists, buf_string(addr->mailbox)))
  {
    return mutt_regexlist_match(&SubscribedLists, buf_string(addr->mailbox));
  }
  return false;
}

/**
 * check_for_mailing_list - Search list of addresses for a mailing list
 * @param al      AddressList to search
 * @param pfx     Prefix string
 * @param buf     Buffer to store results
 * @param buflen  Buffer length
 * @retval 1 Mailing list found
 * @retval 0 No list found
 *
 * Search for a mailing list in the list of addresses pointed to by addr.
 * If one is found, print pfx and the name of the list into buf.
 */
bool check_for_mailing_list(struct AddressList *al, const char *pfx, char *buf, int buflen)
{
  struct Address *a = NULL;
  TAILQ_FOREACH(a, al, entries)
  {
    if (mutt_is_subscribed_list(a))
    {
      if (pfx && buf && buflen)
        snprintf(buf, buflen, "%s%s", pfx, mutt_get_name(a));
      return true;
    }
  }
  return false;
}

/**
 * check_for_mailing_list_addr - Check an address list for a mailing list
 * @param al     AddressList
 * @param buf    Buffer for the result
 * @param buflen Length of buffer
 * @retval true Mailing list found
 *
 * If one is found, print the address of the list into buf.
 */
bool check_for_mailing_list_addr(struct AddressList *al, char *buf, int buflen)
{
  struct Address *a = NULL;
  TAILQ_FOREACH(a, al, entries)
  {
    if (mutt_is_subscribed_list(a))
    {
      if (buf && buflen)
        snprintf(buf, buflen, "%s", buf_string(a->mailbox));
      return true;
    }
  }
  return false;
}

/**
 * first_mailing_list - Get the first mailing list in the list of addresses
 * @param buf    Buffer for the result
 * @param buflen Length of buffer
 * @param al     AddressList
 * @retval true A mailing list was found
 */
bool first_mailing_list(char *buf, size_t buflen, struct AddressList *al)
{
  struct Address *a = NULL;
  TAILQ_FOREACH(a, al, entries)
  {
    if (mutt_is_subscribed_list(a))
    {
      mutt_save_path(buf, buflen, a);
      return true;
    }
  }
  return false;
}
