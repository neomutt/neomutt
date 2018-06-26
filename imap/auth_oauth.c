/*
 * Copyright (C) 1999-2001,2005 Brendan Cully <brendan@kublai.com>
 * Copyright (C) 2018 Brandon Long <blong@fiction.net>
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

/* IMAP login/authentication code */

#include "config.h"
#include "imap_private.h"
#include "mutt/mutt.h"
#include "conn/conn.h"
#include "mutt.h"
#include "auth.h"
#include "mutt_account.h"
#include "mutt_logging.h"
#include "mutt_socket.h"
#include "muttlib.h"

/* imap_auth_oauth: AUTH=OAUTHBEARER support. See RFC 7628 */
enum ImapAuthRes imap_auth_oauth(struct ImapData *idata, const char *method)
{
  char *ibuf = NULL;
  char *oauthbearer = NULL;
  int ilen;
  int rc;

  /* For now, we only support SASL_IR also and over TLS */
  if (!mutt_bit_isset(idata->capabilities, AUTH_OAUTHBEARER) ||
      !mutt_bit_isset(idata->capabilities, SASL_IR) || !idata->conn->ssf)
    return IMAP_AUTH_UNAVAIL;

  mutt_message(_("Authenticating (OAUTHBEARER)..."));

  /* We get the access token from the imap_oauth_refresh_command */
  oauthbearer = mutt_account_getoauthbearer(&idata->conn->account);
  if (oauthbearer == NULL)
    return IMAP_AUTH_FAILURE;

  ilen = mutt_str_strlen(oauthbearer) + 30;
  ibuf = mutt_mem_malloc(ilen);
  snprintf(ibuf, ilen, "AUTHENTICATE OAUTHBEARER %s", oauthbearer);

  /* This doesn't really contain a password, but the token is good for
   * an hour, so suppress it anyways.
   */
  rc = imap_exec(idata, ibuf, IMAP_CMD_FAIL_OK | IMAP_CMD_PASS);

  FREE(&oauthbearer);
  FREE(&ibuf);

  if (rc)
  {
    /* The error response was in SASL continuation, so continue the SASL
     * to cause a failure and exit SASL input.  See RFC 7628 3.2.3
     */
    mutt_socket_send(idata->conn, "\001");
    rc = imap_exec(idata, ibuf, IMAP_CMD_FAIL_OK);
  }

  if (!rc)
  {
    mutt_clear_error();
    return IMAP_AUTH_SUCCESS;
  }

  mutt_error(_("OAUTHBEARER authentication failed."));
  mutt_sleep(2);
  return IMAP_AUTH_FAILURE;
}
