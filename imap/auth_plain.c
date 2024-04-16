/**
 * @file
 * IMAP plain authentication method
 *
 * @authors
 * Copyright (C) 2016-2020 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2017-2019 Richard Russon <rich@flatcap.org>
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
 * @page imap_auth_plain Plain authentication
 *
 * IMAP plain authentication method
 */

#include "config.h"
#include <stddef.h>
#include "private.h"
#include "mutt/lib.h"
#include "conn/lib.h"
#include "adata.h"
#include "auth.h"
#include "mutt_logging.h"

/**
 * imap_auth_plain - SASL PLAIN support - Implements ImapAuth::authenticate() - @ingroup imap_authenticate
 */
enum ImapAuthRes imap_auth_plain(struct ImapAccountData *adata, const char *method)
{
  static const char auth_plain_cmd[] = "AUTHENTICATE PLAIN";
  static const size_t apc_len = sizeof(auth_plain_cmd);

  struct ConnAccount *cac = &adata->conn->account;

  if (mutt_account_getuser(cac) < 0)
    return IMAP_AUTH_FAILURE;
  if (mutt_account_getpass(cac) < 0)
    return IMAP_AUTH_FAILURE;

  mutt_message(_("Logging in..."));

  int rc_step = IMAP_RES_CONTINUE;
  enum ImapAuthRes rc = IMAP_AUTH_SUCCESS;
  char buf[256] = { 0 };

  /* Prepare full AUTHENTICATE PLAIN message */
  mutt_sasl_plain_msg(buf, sizeof(buf), auth_plain_cmd, cac->user, cac->user, cac->pass);

  if (adata->capabilities & IMAP_CAP_SASL_IR)
  {
    imap_cmd_start(adata, buf);
  }
  else
  {
    /* Split the message so we send AUTHENTICATE PLAIN first, and the
     * credentials after the first command continuation request */
    buf[apc_len - 1] = '\0';
    imap_cmd_start(adata, buf);
    while (rc_step == IMAP_RES_CONTINUE)
    {
      rc_step = imap_cmd_step(adata);
    }
    if (rc_step == IMAP_RES_RESPOND)
    {
      mutt_str_cat(buf + apc_len, sizeof(buf) - apc_len, "\r\n");
      mutt_socket_send(adata->conn, buf + apc_len);
      rc_step = IMAP_RES_CONTINUE;
    }
  }

  while (rc_step == IMAP_RES_CONTINUE)
  {
    rc_step = imap_cmd_step(adata);
  }

  if (rc_step == IMAP_RES_BAD)
  {
    rc = IMAP_AUTH_UNAVAIL;
  }
  else if (rc_step == IMAP_RES_NO)
  {
    mutt_error(_("Login failed"));
    rc = IMAP_AUTH_FAILURE;
  }

  mutt_clear_error();
  return rc;
}
