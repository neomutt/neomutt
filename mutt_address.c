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
#include <string.h>
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

void rfc822_write_address_single(char *buf, size_t buflen, struct Address *addr, int display)
{
  size_t len;
  char *pbuf = buf;
  char *pc = NULL;

  if (!addr)
    return;

  buflen--; /* save room for the terminal nul */

  if (addr->personal)
  {
    if (strpbrk(addr->personal, AddressSpecials))
    {
      if (!buflen)
        goto done;
      *pbuf++ = '"';
      buflen--;
      for (pc = addr->personal; *pc && buflen > 0; pc++)
      {
        if (*pc == '"' || *pc == '\\')
        {
          *pbuf++ = '\\';
          buflen--;
        }
        if (!buflen)
          goto done;
        *pbuf++ = *pc;
        buflen--;
      }
      if (!buflen)
        goto done;
      *pbuf++ = '"';
      buflen--;
    }
    else
    {
      if (!buflen)
        goto done;
      mutt_str_strfcpy(pbuf, addr->personal, buflen);
      len = mutt_str_strlen(pbuf);
      pbuf += len;
      buflen -= len;
    }

    if (!buflen)
      goto done;
    *pbuf++ = ' ';
    buflen--;
  }

  if (addr->personal || (addr->mailbox && *addr->mailbox == '@'))
  {
    if (!buflen)
      goto done;
    *pbuf++ = '<';
    buflen--;
  }

  if (addr->mailbox)
  {
    if (!buflen)
      goto done;
    if ((mutt_str_strcmp(addr->mailbox, "@") != 0) && !display)
    {
      mutt_str_strfcpy(pbuf, addr->mailbox, buflen);
      len = mutt_str_strlen(pbuf);
    }
    else if ((mutt_str_strcmp(addr->mailbox, "@") != 0) && display)
    {
      mutt_str_strfcpy(pbuf, mutt_addr_for_display(addr), buflen);
      len = mutt_str_strlen(pbuf);
    }
    else
    {
      *pbuf = '\0';
      len = 0;
    }
    pbuf += len;
    buflen -= len;

    if (addr->personal || (addr->mailbox && *addr->mailbox == '@'))
    {
      if (!buflen)
        goto done;
      *pbuf++ = '>';
      buflen--;
    }

    if (addr->group)
    {
      if (!buflen)
        goto done;
      *pbuf++ = ':';
      buflen--;
      if (!buflen)
        goto done;
      *pbuf++ = ' ';
      buflen--;
    }
  }
  else
  {
    if (!buflen)
      goto done;
    *pbuf++ = ';';
    buflen--;
  }
done:
  /* no need to check for length here since we already save space at the
     beginning of this routine */
  *pbuf = 0;
}

/**
 * rfc822_write_address - Write an address to a buffer
 *
 * Note: it is assumed that `buf' is nul terminated!
 */
size_t rfc822_write_address(char *buf, size_t buflen, struct Address *addr, int display)
{
  char *pbuf = buf;
  size_t len = mutt_str_strlen(buf);

  buflen--; /* save room for the terminal nul */

  if (len > 0)
  {
    if (len > buflen)
      return 0; /* safety check for bogus arguments */

    pbuf += len;
    buflen -= len;
    if (!buflen)
      goto done;
    *pbuf++ = ',';
    buflen--;
    if (!buflen)
      goto done;
    *pbuf++ = ' ';
    buflen--;
  }

  for (; addr && buflen > 0; addr = addr->next)
  {
    /* use buflen+1 here because we already saved space for the trailing
       nul char, and the subroutine can make use of it */
    rfc822_write_address_single(pbuf, buflen + 1, addr, display);

    /* this should be safe since we always have at least 1 char passed into
       the above call, which means `pbuf' should always be nul terminated */
    len = mutt_str_strlen(pbuf);
    pbuf += len;
    buflen -= len;

    /* if there is another address, and it's not a group mailbox name or
       group terminator, add a comma to separate the addresses */
    if (addr->next && addr->next->mailbox && !addr->group)
    {
      if (!buflen)
        goto done;
      *pbuf++ = ',';
      buflen--;
      if (!buflen)
        goto done;
      *pbuf++ = ' ';
      buflen--;
    }
  }
done:
  *pbuf = 0;
  return pbuf - buf;
}
