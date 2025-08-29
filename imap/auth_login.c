/**
 * @file
 * IMAP login authentication method
 *
 * @authors
 * Copyright (C) 1999-2001,2005,2009 Brendan Cully <brendan@kublai.com>
 * Copyright (C) 2017-2022 Richard Russon <rich@flatcap.org>
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
 * @page imap_auth_login Login authentication
 *
 * IMAP login authentication method
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "conn/lib.h"
#include "adata.h"
#include "auth.h"
#include "mutt_logging.h"

/**
 * imap_auth_login - Plain LOGIN support - Implements ImapAuth::authenticate() - @ingroup imap_authenticate
 */
enum ImapAuthRes imap_auth_login(struct ImapAccountData *adata, const char *method)
{
  char q_user[256] = { 0 };
  char q_pass[256] = { 0 };
  char buf[1024] = { 0 };

  if ((adata->capabilities & IMAP_CAP_LOGINDISABLED))
  {
    mutt_message(_("LOGIN disabled on this server"));
    return IMAP_AUTH_UNAVAIL;
  }

  if (mutt_account_getuser(&adata->conn->account) < 0)
    return IMAP_AUTH_FAILURE;
  if (mutt_account_getpass(&adata->conn->account) < 0)
    return IMAP_AUTH_FAILURE;

  mutt_message(_("Logging in..."));

  imap_quote_string(q_user, sizeof(q_user), adata->conn->account.user, false);
  imap_quote_string(q_pass, sizeof(q_pass), adata->conn->account.pass, false);

  /* don't print the password unless we're at the ungodly debugging level
   * of 5 or higher */

  const short c_debug_level = cs_subset_number(NeoMutt->sub, "debug_level");
  if (c_debug_level < IMAP_LOG_PASS)
    mutt_debug(LL_DEBUG2, "Sending LOGIN command for %s\n", adata->conn->account.user);

  snprintf(buf, sizeof(buf), "LOGIN %s %s", q_user, q_pass);
  if (imap_exec(adata, buf, IMAP_CMD_PASS) == IMAP_EXEC_SUCCESS)
  {
    mutt_clear_error();
    return IMAP_AUTH_SUCCESS;
  }

  mutt_error(_("Login failed"));
  return IMAP_AUTH_FAILURE;
}
