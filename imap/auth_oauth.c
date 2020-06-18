/**
 * @file
 * IMAP OAUTH authentication method
 *
 * @authors
 * Copyright (C) 1999-2001,2005 Brendan Cully <brendan@kublai.com>
 * Copyright (C) 2018 Brandon Long <blong@fiction.net>
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
 * @page imap_auth_oauth IMAP OAUTH authentication method
 *
 * IMAP OAUTH authentication method
 */

#include "config.h"
#include <stdio.h>
#include "private.h"
#include "mutt/lib.h"
#include "conn/lib.h"
#include "auth.h"
#include "lib.h"
#include "mutt_logging.h"
#include "mutt_socket.h"

/**
 * imap_auth_oauth - Authenticate an IMAP connection using OAUTHBEARER - Implements ImapAuth::authenticate()
 * @param adata Imap Account data
 * @param method Name of this authentication method
 * @retval num Result, e.g. #IMAP_AUTH_SUCCESS
 */
enum ImapAuthRes imap_auth_oauth(struct ImapAccountData *adata, const char *method)
{
  char *ibuf = NULL;
  char *oauthbearer = NULL;
  int ilen;
  int rc;

  /* For now, we only support SASL_IR also and over TLS */
  if (!(adata->capabilities & IMAP_CAP_AUTH_OAUTHBEARER) ||
      !(adata->capabilities & IMAP_CAP_SASL_IR) || (adata->conn->ssf == 0))
  {
    return IMAP_AUTH_UNAVAIL;
  }

  /* If they did not explicitly request or configure oauth then fail quietly */
  if (!method && !C_ImapOauthRefreshCommand)
    return IMAP_AUTH_UNAVAIL;

  mutt_message(_("Authenticating (%s)..."), "OAUTHBEARER");

  /* We get the access token from the imap_oauth_refresh_command */
  oauthbearer = mutt_account_getoauthbearer(&adata->conn->account);
  if (!oauthbearer)
    return IMAP_AUTH_FAILURE;

  ilen = mutt_str_len(oauthbearer) + 30;
  ibuf = mutt_mem_malloc(ilen);
  snprintf(ibuf, ilen, "AUTHENTICATE OAUTHBEARER %s", oauthbearer);

  /* This doesn't really contain a password, but the token is good for
   * an hour, so suppress it anyways.  */
  rc = imap_exec(adata, ibuf, IMAP_CMD_PASS);

  FREE(&oauthbearer);
  FREE(&ibuf);

  if (rc != IMAP_EXEC_SUCCESS)
  {
    /* The error response was in SASL continuation, so continue the SASL
     * to cause a failure and exit SASL input.  See RFC7628 3.2.3 */
    mutt_socket_send(adata->conn, "\001");
    rc = imap_exec(adata, ibuf, IMAP_CMD_NO_FLAGS);
  }

  if (rc == IMAP_EXEC_SUCCESS)
  {
    mutt_clear_error();
    return IMAP_AUTH_SUCCESS;
  }

  mutt_error(_("OAUTHBEARER authentication failed"));
  return IMAP_AUTH_FAILURE;
}
