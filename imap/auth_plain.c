/**
 * @file
 * IMAP plain authentication method
 *
 * @authors
 * Copyright (C) 1999-2001,2005,2009 Brendan Cully <brendan@kublai.com>
 * Copyright (C) 2016 Pietro Cerutti <gahr@gahr.ch>
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
 * @page imap_auth_plain IMAP plain authentication method
 *
 * IMAP plain authentication method
 */

#include "config.h"
#include "private.h"
#include "mutt/lib.h"
#include "conn/lib.h"
#include "auth.h"
#include "mutt_logging.h"
#include "mutt_socket.h"

/**
 * imap_auth_plain - SASL PLAIN support - Implements ImapAuth::authenticate()
 * @param adata Imap Account data
 * @param method Name of this authentication method
 * @retval #ImapAuthRes Result, e.g. #IMAP_AUTH_SUCCESS
 */
enum ImapAuthRes imap_auth_plain(struct ImapAccountData *adata, const char *method)
{
  int rc = IMAP_RES_CONTINUE;
  enum ImapAuthRes res = IMAP_AUTH_SUCCESS;
  static const char auth_plain_cmd[] = "AUTHENTICATE PLAIN";
  char buf[256] = { 0 };

  if (mutt_account_getuser(&adata->conn->account) < 0)
    return IMAP_AUTH_FAILURE;
  if (mutt_account_getpass(&adata->conn->account) < 0)
    return IMAP_AUTH_FAILURE;

  mutt_message(_("Logging in..."));

  /* Prepare full AUTHENTICATE PLAIN message */
  mutt_sasl_plain_msg(buf, sizeof(buf), auth_plain_cmd, adata->conn->account.user,
                      adata->conn->account.user, adata->conn->account.pass);

  if (adata->capabilities & IMAP_CAP_SASL_IR)
  {
    imap_cmd_start(adata, buf);
  }
  else
  {
    /* Split the message so we send AUTHENTICATE PLAIN first, and the
     * credentials after the first command continuation request */
    buf[sizeof(auth_plain_cmd) - 1] = '\0';
    imap_cmd_start(adata, buf);
    while (rc == IMAP_RES_CONTINUE)
    {
      rc = imap_cmd_step(adata);
    }
    if (rc == IMAP_RES_RESPOND)
    {
      mutt_str_cat(buf + sizeof(auth_plain_cmd),
                   sizeof(buf) - sizeof(auth_plain_cmd), "\r\n");
      mutt_socket_send(adata->conn, buf + sizeof(auth_plain_cmd));
      rc = IMAP_RES_CONTINUE;
    }
  }

  while (rc == IMAP_RES_CONTINUE)
  {
    rc = imap_cmd_step(adata);
  }

  if (rc == IMAP_RES_BAD)
  {
    res = IMAP_AUTH_UNAVAIL;
  }
  else if (rc == IMAP_RES_NO)
  {
    mutt_error(_("Login failed"));
    res = IMAP_AUTH_FAILURE;
  }

  mutt_clear_error();
  return res;
}
