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
#include <stdio.h>
#include <string.h>
#include "imap_private.h"
#include "mutt/mutt.h"
#include "conn/conn.h"
#include "auth.h"
#include "globals.h"
#include "mutt_account.h"
#include "mutt_socket.h"
#include "options.h"
#include "protos.h"

/**
 * imap_auth_sasl - Default authenticator if available
 * @param idata  Server data
 * @param method Name of this authentication method
 * @retval enum Result, e.g. #IMAP_AUTH_SUCCESS
 */
enum ImapAuthRes imap_auth_sasl(struct ImapData *idata, const char *method)
{
  sasl_conn_t *saslconn = NULL;
  sasl_interact_t *interaction = NULL;
  int rc, irc;
  char *buf = NULL;
  size_t bufsize = 0;
  const char *mech = NULL;
  const char *pc = NULL;
  unsigned int len, olen;
  bool client_start;

  if (mutt_sasl_client_new(idata->conn, &saslconn) < 0)
  {
    mutt_debug(1, "Error allocating SASL connection.\n");
    return IMAP_AUTH_FAILURE;
  }

  rc = SASL_FAIL;

  /* If the user hasn't specified a method, use any available */
  if (!method)
  {
    method = idata->capstr;

    /* hack for SASL ANONYMOUS support:
     * 1. Fetch username. If it's "" or "anonymous" then
     * 2. attempt sasl_client_start with only "AUTH=ANONYMOUS" capability
     * 3. if sasl_client_start fails, fall through... */

    if (mutt_account_getuser(&idata->conn->account) < 0)
      return IMAP_AUTH_FAILURE;

    if (mutt_bit_isset(idata->capabilities, AUTH_ANON) &&
        (!idata->conn->account.user[0] ||
         (mutt_str_strncmp(idata->conn->account.user, "anonymous", 9) == 0)))
    {
      rc = sasl_client_start(saslconn, "AUTH=ANONYMOUS", NULL, &pc, &olen, &mech);
    }
  }
  else if ((mutt_str_strcasecmp("login", method) == 0) &&
           !strstr(NONULL(idata->capstr), "AUTH=LOGIN"))
  {
    /* do not use SASL login for regular IMAP login (#3556) */
    return IMAP_AUTH_UNAVAIL;
  }

  if (rc != SASL_OK && rc != SASL_CONTINUE)
  {
    do
    {
      rc = sasl_client_start(saslconn, method, &interaction, &pc, &olen, &mech);
      if (rc == SASL_INTERACT)
        mutt_sasl_interact(interaction);
    } while (rc == SASL_INTERACT);
  }

  client_start = (olen > 0);

  if (rc != SASL_OK && rc != SASL_CONTINUE)
  {
    if (method)
      mutt_debug(2, "%s unavailable\n", method);
    else
      mutt_debug(
          1,
          "Failure starting authentication exchange. No shared mechanisms?\n");
    /* SASL doesn't support LOGIN, so fall back */

    return IMAP_AUTH_UNAVAIL;
  }

  mutt_message(_("Authenticating (%s)..."), mech);

  bufsize = ((olen * 2) > LONG_STRING) ? (olen * 2) : LONG_STRING;
  buf = mutt_mem_malloc(bufsize);

  snprintf(buf, bufsize, "AUTHENTICATE %s", mech);
  if (mutt_bit_isset(idata->capabilities, SASL_IR) && client_start)
  {
    len = mutt_str_strlen(buf);
    buf[len++] = ' ';
    if (sasl_encode64(pc, olen, buf + len, bufsize - len, &olen) != SASL_OK)
    {
      mutt_debug(1, "#1 error base64-encoding client response.\n");
      goto bail;
    }
    client_start = false;
    olen = 0;
  }
  imap_cmd_start(idata, buf);
  irc = IMAP_CMD_CONTINUE;

  /* looping protocol */
  while (rc == SASL_CONTINUE || olen > 0)
  {
    do
      irc = imap_cmd_step(idata);
    while (irc == IMAP_CMD_CONTINUE);

    if (irc == IMAP_CMD_BAD || irc == IMAP_CMD_NO)
      goto bail;

    if (irc == IMAP_CMD_RESPOND)
    {
      /* Exchange incorrectly returns +\r\n instead of + \r\n */
      if (idata->buf[1] == '\0')
      {
        buf[0] = '\0';
        len = 0;
      }
      else
      {
        len = strlen(idata->buf + 2);
        if (len > bufsize)
        {
          bufsize = len;
          mutt_mem_realloc(&buf, bufsize);
        }
        /* For sasl_decode64, the fourth parameter, outmax, doesn't
         * include space for the trailing null */
        if (sasl_decode64(idata->buf + 2, len, buf, bufsize - 1, &len) != SASL_OK)
        {
          mutt_debug(1, "error base64-decoding server response.\n");
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
        mutt_debug(1, "#2 error base64-encoding client response.\n");
        goto bail;
      }
    }

    if (irc == IMAP_CMD_RESPOND)
    {
      mutt_str_strfcpy(buf + olen, "\r\n", bufsize - olen);
      mutt_socket_write(idata->conn, buf);
    }

    /* If SASL has errored out, send an abort string to the server */
    if (rc < 0)
    {
      mutt_socket_write(idata->conn, "*\r\n");
      mutt_debug(1, "sasl_client_step error %d\n", rc);
    }

    olen = 0;
  }

  while (irc != IMAP_CMD_OK)
  {
    irc = imap_cmd_step(idata);
    if (irc != IMAP_CMD_CONTINUE)
      break;
  }

  if (rc != SASL_OK)
    goto bail;

  if (imap_code(idata->buf))
  {
    mutt_sasl_setup_conn(idata->conn, saslconn);
    FREE(&buf);
    return IMAP_AUTH_SUCCESS;
  }

bail:
  sasl_dispose(&saslconn);
  FREE(&buf);

  if (method)
  {
    mutt_debug(2, "%s failed\n", method);
    return IMAP_AUTH_UNAVAIL;
  }

  mutt_error(_("SASL authentication failed."));

  return IMAP_AUTH_FAILURE;
}
