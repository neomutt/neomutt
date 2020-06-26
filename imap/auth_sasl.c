/**
 * @file
 * IMAP SASL authentication method
 *
 * @authors
 * Copyright (C) 2000-2006,2012 Brendan Cully <brendan@kublai.com>
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
 * @page imap_auth_sasl IMAP SASL authentication method
 *
 * IMAP SASL authentication method
 */

#include "config.h"
#include <stddef.h>
#include <sasl/sasl.h>
#include <sasl/saslutil.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "private.h"
#include "mutt/lib.h"
#include "conn/lib.h"
#include "auth.h"
#include "mutt_socket.h"

/**
 * imap_auth_sasl - Default authenticator if available - Implements ImapAuth::authenticate()
 * @param adata Imap Account data
 * @param method Name of this authentication method
 * @retval #ImapAuthRes Result, e.g. #IMAP_AUTH_SUCCESS
 */
enum ImapAuthRes imap_auth_sasl(struct ImapAccountData *adata, const char *method)
{
  sasl_conn_t *saslconn = NULL;
  sasl_interact_t *interaction = NULL;
  int rc, irc;
  char *buf = NULL;
  size_t bufsize = 0;
  const char *mech = NULL;
  const char *pc = NULL;
  unsigned int len = 0, olen = 0;
  bool client_start;

  if (mutt_sasl_client_new(adata->conn, &saslconn) < 0)
  {
    mutt_debug(LL_DEBUG1, "Error allocating SASL connection\n");
    return IMAP_AUTH_FAILURE;
  }

  rc = SASL_FAIL;

  /* If the user hasn't specified a method, use any available */
  if (!method)
  {
    method = adata->capstr;

    /* hack for SASL ANONYMOUS support:
     * 1. Fetch username. If it's "" or "anonymous" then
     * 2. attempt sasl_client_start with only "AUTH=ANONYMOUS" capability
     * 3. if sasl_client_start fails, fall through... */

    if (mutt_account_getuser(&adata->conn->account) < 0)
    {
      sasl_dispose(&saslconn);
      return IMAP_AUTH_FAILURE;
    }

    if ((adata->capabilities & IMAP_CAP_AUTH_ANONYMOUS) &&
        (!adata->conn->account.user[0] ||
         mutt_str_startswith(adata->conn->account.user, "anonymous")))
    {
      rc = sasl_client_start(saslconn, "AUTH=ANONYMOUS", NULL, &pc, &olen, &mech);
    }
  }
  else if (mutt_istr_equal("login", method) && !strstr(NONULL(adata->capstr), "AUTH=LOGIN"))
  {
    /* do not use SASL login for regular IMAP login */
    sasl_dispose(&saslconn);
    return IMAP_AUTH_UNAVAIL;
  }

  if ((rc != SASL_OK) && (rc != SASL_CONTINUE))
  {
    do
    {
      rc = sasl_client_start(saslconn, method, &interaction, &pc, &olen, &mech);
      if (rc == SASL_INTERACT)
        mutt_sasl_interact(interaction);
    } while (rc == SASL_INTERACT);
  }

  client_start = (olen > 0);

  if ((rc != SASL_OK) && (rc != SASL_CONTINUE))
  {
    if (method)
      mutt_debug(LL_DEBUG2, "%s unavailable\n", method);
    else
    {
      mutt_debug(
          LL_DEBUG1,
          "Failure starting authentication exchange. No shared mechanisms?\n");
    }
    /* SASL doesn't support LOGIN, so fall back */

    sasl_dispose(&saslconn);
    return IMAP_AUTH_UNAVAIL;
  }

  mutt_message(_("Authenticating (%s)..."), mech);

  bufsize = MAX((olen * 2), 1024);
  buf = mutt_mem_malloc(bufsize);

  snprintf(buf, bufsize, "AUTHENTICATE %s", mech);
  if ((adata->capabilities & IMAP_CAP_SASL_IR) && client_start)
  {
    len = mutt_str_len(buf);
    buf[len++] = ' ';
    if (sasl_encode64(pc, olen, buf + len, bufsize - len, &olen) != SASL_OK)
    {
      mutt_debug(LL_DEBUG1, "#1 error base64-encoding client response\n");
      goto bail;
    }
    client_start = false;
    olen = 0;
  }
  imap_cmd_start(adata, buf);
  irc = IMAP_RES_CONTINUE;

  /* looping protocol */
  while ((rc == SASL_CONTINUE) || (olen > 0))
  {
    do
    {
      irc = imap_cmd_step(adata);
    } while (irc == IMAP_RES_CONTINUE);

    if ((irc == IMAP_RES_BAD) || (irc == IMAP_RES_NO))
      goto bail;

    if (irc == IMAP_RES_RESPOND)
    {
      /* Exchange incorrectly returns +\r\n instead of + \r\n */
      if (adata->buf[1] == '\0')
      {
        buf[0] = '\0';
        len = 0;
      }
      else
      {
        len = strlen(adata->buf + 2);
        if (len > bufsize)
        {
          bufsize = len;
          mutt_mem_realloc(&buf, bufsize);
        }
        /* For sasl_decode64, the fourth parameter, outmax, doesn't
         * include space for the trailing null */
        if (sasl_decode64(adata->buf + 2, len, buf, bufsize - 1, &len) != SASL_OK)
        {
          mutt_debug(LL_DEBUG1, "error base64-decoding server response\n");
          goto bail;
        }
      }
    }

    /* client-start is only available with the SASL-IR extension, but
     * SASL 2.1 seems to want to use it regardless, at least for DIGEST
     * fast reauth. Override if the server sent an initial continuation */
    if (!client_start || buf[0])
    {
      do
      {
        rc = sasl_client_step(saslconn, buf, len, &interaction, &pc, &olen);
        if (rc == SASL_INTERACT)
          mutt_sasl_interact(interaction);
      } while (rc == SASL_INTERACT);
    }
    else
      client_start = false;

    /* send out response, or line break if none needed */
    if (olen)
    {
      if ((olen * 2) > bufsize)
      {
        bufsize = olen * 2;
        mutt_mem_realloc(&buf, bufsize);
      }
      if (sasl_encode64(pc, olen, buf, bufsize, &olen) != SASL_OK)
      {
        mutt_debug(LL_DEBUG1, "#2 error base64-encoding client response\n");
        goto bail;
      }
    }

    if (irc == IMAP_RES_RESPOND)
    {
      mutt_str_copy(buf + olen, "\r\n", bufsize - olen);
      mutt_socket_send(adata->conn, buf);
    }

    /* If SASL has errored out, send an abort string to the server */
    if (rc < 0)
    {
      mutt_socket_send(adata->conn, "*\r\n");
      mutt_debug(LL_DEBUG1, "sasl_client_step error %d\n", rc);
    }

    olen = 0;
  }

  while (irc != IMAP_RES_OK)
  {
    irc = imap_cmd_step(adata);
    if (irc != IMAP_RES_CONTINUE)
      break;
  }

  if (rc != SASL_OK)
    goto bail;

  if (imap_code(adata->buf))
  {
    mutt_sasl_setup_conn(adata->conn, saslconn);
    FREE(&buf);
    return IMAP_AUTH_SUCCESS;
  }

bail:
  sasl_dispose(&saslconn);
  FREE(&buf);

  if (method)
  {
    mutt_debug(LL_DEBUG2, "%s failed\n", method);
    return IMAP_AUTH_UNAVAIL;
  }

  mutt_error(_("SASL authentication failed"));

  return IMAP_AUTH_FAILURE;
}
