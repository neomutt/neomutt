/*
 * Copyright (C) 2009,2013,2016 Derek Martin <code@pizzashack.org>
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "mutt.h"


int getdnsdomainname (char *d, size_t len)
{
  /* A DNS name can actually be only 253 octets, string is 256 */
  char *node;
  long node_len;
  struct addrinfo hints;
  struct addrinfo *h;
  char *p;
  int ret;

  *d = '\0';
  memset(&hints, 0, sizeof (struct addrinfo));
  hints.ai_flags = AI_CANONNAME;
  hints.ai_family = AF_UNSPEC;

  if ((node_len = sysconf(_SC_HOST_NAME_MAX)) == -1)
    node_len = STRING;
  node = safe_malloc(node_len + 1);
  if (gethostname(node, node_len))
    ret = -1;
  else if (getaddrinfo(node, NULL, &hints, &h))
    ret = -1;
  else
  {
    if (!(p = strchr(h->ai_canonname, '.')))
      ret = -1;
    else
    {
      strfcpy(d, ++p, len);
      ret = 0;
      dprint(1, (debugfile, "getdnsdomainname(): %s\n", d));
    }
    freeaddrinfo(h);
  }
  FREE (&node);
  return ret;
}

