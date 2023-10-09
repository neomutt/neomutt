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
 * @page imap_auth_oauth OAUTH authentication
 *
 * IMAP OAUTH authentication method
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "conn/lib.h"
#include "auth.h"
#include "adata.h"
#include "mutt_logging.h"

/**
 * imap_auth_oauth_xoauth2 - Authenticate an IMAP connection using OAUTHBEARER or XOAUTH2
 * @param adata   Imap Account data
 * @param method  Use this named method, or any available method if NULL
 * @param xoauth2 Use xoauth2 token (if true) or oauthbearer token (if false)
 * @retval num ImapAuth::ImapAuthRes Result, e.g. IMAP_AUTH_SUCCESS
 */
static enum ImapAuthRes imap_auth_oauth_xoauth2(struct ImapAccountData *adata,
                                                const char *method, bool xoauth2)
{
  char *ibuf = NULL;
  char *oauthbearer = NULL;
  const char *authtype = xoauth2 ? "XOAUTH2" : "OAUTHBEARER";
  int rc;

  /* For now, we only support SASL_IR also and over TLS */
  if ((xoauth2 && !(adata->capabilities & IMAP_CAP_AUTH_XOAUTH2)) ||
      (!xoauth2 && !(adata->capabilities & IMAP_CAP_AUTH_OAUTHBEARER)) ||
      !(adata->capabilities & IMAP_CAP_SASL_IR) || (adata->conn->ssf == 0))
  {
    return IMAP_AUTH_UNAVAIL;
  }

  /* If they did not explicitly request or configure oauth then fail quietly */
  const char *const c_imap_oauth_refresh_command = cs_subset_string(NeoMutt->sub, "imap_oauth_refresh_command");
  if (!method && !c_imap_oauth_refresh_command)
    return IMAP_AUTH_UNAVAIL;

  // L10N: (%s) is the method name, e.g. Anonymous, CRAM-MD5, GSSAPI, SASL
  mutt_message(_("Authenticating (%s)..."), authtype);

  /* We get the access token from the imap_oauth_refresh_command */
  oauthbearer = mutt_account_getoauthbearer(&adata->conn->account, xoauth2);
  if (!oauthbearer)
    return IMAP_AUTH_FAILURE;

  mutt_str_asprintf(&ibuf, "AUTHENTICATE %s %s", authtype, oauthbearer);

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

  // L10N: %s is the method name, e.g. Anonymous, CRAM-MD5, GSSAPI, SASL
  mutt_error(_("%s authentication failed"), authtype);
  return IMAP_AUTH_FAILURE;
}

/**
 * imap_auth_oauth - Authenticate an IMAP connection using OAUTHBEARER - Implements ImapAuth::authenticate() - @ingroup imap_authenticate
 */
enum ImapAuthRes imap_auth_oauth(struct ImapAccountData *adata, const char *method)
{
  return imap_auth_oauth_xoauth2(adata, method, false);
}

/**
 * imap_auth_xoauth2 - Authenticate an IMAP connection using XOAUTH2 - Implements ImapAuth::authenticate() - @ingroup imap_authenticate
 */
enum ImapAuthRes imap_auth_xoauth2(struct ImapAccountData *adata, const char *method)
{
  return imap_auth_oauth_xoauth2(adata, method, true);
}
