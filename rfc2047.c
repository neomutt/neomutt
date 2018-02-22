/**
 * @file
 * RFC2047 MIME extensions routines
 *
 * @authors
 * Copyright (C) 1996-2000,2010 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2000-2002 Edmund Grimley Evans <edmundo@rano.org>
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
#include "mutt/charset.h"
#include "mutt/rfc2047.h"
#include "address.h"
#include "globals.h"

#include <string.h>

void rfc2047_encode_addrlist(struct Address *addr, const char *tag)
{
  struct Address *ptr = addr;
  int col = tag ? strlen(tag) + 2 : 32;

  while (ptr)
  {
    if (ptr->personal)
      mutt_rfc2047_encode(&ptr->personal, AddressSpecials, col, SendCharset);
    else if (ptr->group && ptr->mailbox)
      mutt_rfc2047_encode(&ptr->mailbox, AddressSpecials, col, SendCharset);
    ptr = ptr->next;
  }
}

void rfc2047_decode_addrlist(struct Address *a)
{
  while (a)
  {
    if (a->personal &&
        ((strstr(a->personal, "=?") != NULL) || (AssumedCharset && *AssumedCharset)))
    {
      mutt_rfc2047_decode(&a->personal);
    }
    else if (a->group && a->mailbox && (strstr(a->mailbox, "=?") != NULL))
      mutt_rfc2047_decode(&a->mailbox);
    a = a->next;
  }
}
