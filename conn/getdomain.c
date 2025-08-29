/**
 * @file
 * DNS lookups
 *
 * @authors
 * Copyright (C) 2017 Mehdi Abaakouk <sileht@sileht.net>
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2021 Ihor Antonov <ihor@antonovs.family>
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
 * @page conn_getdomain DNS lookups
 *
 * DNS lookups
 */

#include "config.h"
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "lib.h"

#ifdef HAVE_GETADDRINFO_A
/**
 * mutt_getaddrinfo_a - Lookup the host's name using getaddrinfo_a()
 * @param node  Hostname, got by gethostname()
 * @param hints Flags to pass to getaddrinfo_a()
 * @retval ptr  Address info
 * @retval NULL Error
 *
 * @note Caller must free result
 */
static struct addrinfo *mutt_getaddrinfo_a(const char *node, const struct addrinfo *hints)
{
  ASSERT(node);
  ASSERT(hints);
  struct addrinfo *result = NULL;

  /* Allow 0.1 seconds to get the FQDN (fully-qualified domain name).
   * If it takes longer, the system is mis-configured and the network is not
   * working properly, so...  */
  struct timespec timeout = { 0, 100000000 };
  struct gaicb req = { 0 };
  req.ar_name = node;
  req.ar_request = hints;
  struct gaicb *reqs[1] = { &req };
  if (getaddrinfo_a(GAI_NOWAIT, reqs, 1, NULL) == 0)
  {
    gai_suspend((const struct gaicb *const *) reqs, 1, &timeout);
    const int status = gai_error(reqs[0]);
    if (status == 0)
    {
      result = reqs[0]->ar_result;
    }
    else if (status == EAI_INPROGRESS)
    {
      mutt_debug(LL_DEBUG1, "timeout\n");
      /* request is not finished, cancel it to free it safely */
      if (gai_cancel(reqs[0]) == EAI_NOTCANCELED)
      {
        // try once more for half-a-second, then bail out
        timeout.tv_nsec = 50000000;
        gai_suspend((const struct gaicb *const *) reqs, 1, &timeout);
      }
    }
    else
    {
      mutt_debug(LL_DEBUG1, "fail: (%d) %s\n", status, gai_strerror(status));
    }
  }
  return result;
}

#elif defined(HAVE_GETADDRINFO)
/**
 * mutt_getaddrinfo - Lookup the host's name using getaddrinfo()
 * @param node  Hostname, got by gethostname()
 * @param hints Flags to pass to getaddrinfo()
 * @retval ptr  Address info
 * @retval NULL Error
 *
 * @note Caller must free result
 */
static struct addrinfo *mutt_getaddrinfo(const char *node, const struct addrinfo *hints)
{
  ASSERT(node);
  ASSERT(hints);
  struct addrinfo *result = NULL;
  mutt_debug(LL_DEBUG3, "before getaddrinfo\n");
  int rc = getaddrinfo(node, NULL, hints, &result);
  mutt_debug(LL_DEBUG3, "after getaddrinfo\n");

  if (rc != 0)
    result = NULL;

  return result;
}
#endif

/**
 * getdnsdomainname - Lookup the host's name using DNS
 * @param result Buffer were result of the domain name lookup will be stored
 * @retval  0 Success
 * @retval -1 Error
 */
int getdnsdomainname(struct Buffer *result)
{
  ASSERT(result);
  int rc = -1;

#if defined(HAVE_GETADDRINFO) || defined(HAVE_GETADDRINFO_A)
  char node[256] = { 0 };
  if (gethostname(node, sizeof(node)) != 0)
    return rc;

  struct addrinfo *lookup_result = NULL;
  struct addrinfo hints = { 0 };

  buf_reset(result);
  hints.ai_flags = AI_CANONNAME;
  hints.ai_family = AF_UNSPEC;

#ifdef HAVE_GETADDRINFO_A
  lookup_result = mutt_getaddrinfo_a(node, &hints);
#else
  lookup_result = mutt_getaddrinfo(node, &hints);
#endif

  if (lookup_result && lookup_result->ai_canonname)
  {
    const char *hostname = strchr(lookup_result->ai_canonname, '.');
    if (hostname && hostname[1] != '\0')
    {
      buf_strcpy(result, ++hostname);
      rc = 0;
    }
  }

  if (lookup_result)
    freeaddrinfo(lookup_result);
#endif

  return rc;
}
