/**
 * @file
 * IMAP login authentication method
 *
 * @authors
 * Copyright (C) 1999-2001,2005,2009 Brendan Cully <brendan@kublai.com>
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
 * @page imap_auth_login IMAP login authentication method
 *
 * IMAP login authentication method
 */

#include "config.h"
#include <stdio.h>
#include "imap_private.h"
#include "mutt/mutt.h"
#include "conn/conn.h"
#include "mutt.h"
#include "auth.h"
#include "globals.h"
#include "mutt_account.h"
#include "mutt_logging.h"
#include "mutt_socket.h"
#include "options.h"
#include "protos.h"

/**
 * imap_auth_login - Plain LOGIN support
 * @param idata  Server data
 * @param method Name of this authentication method
 * @retval enum Result, e.g. #IMAP_AUTH_SUCCESS
 */
enum ImapAuthRes imap_auth_login(struct ImapData *idata, const char *method)
{
  char q_user[SHORT_STRING], q_pass[SHORT_STRING];
  char buf[STRING];
  int rc;

  if (mutt_bit_isset(idata->capabilities, LOGINDISABLED))
  {
    mutt_message(_("LOGIN disabled on this server."));
    return IMAP_AUTH_UNAVAIL;
  }

  if (mutt_account_getuser(&idata->conn->account) < 0)
    return IMAP_AUTH_FAILURE;
  if (mutt_account_getpass(&idata->conn->account) < 0)
    return IMAP_AUTH_FAILURE;

  mutt_message(_("Logging in..."));

  imap_quote_string(q_user, sizeof(q_user), idata->conn->account.user, false);
  imap_quote_string(q_pass, sizeof(q_pass), idata->conn->account.pass, false);

  /* don't print the password unless we're at the ungodly debugging level
   * of 5 or higher */

  if (DebugLevel < IMAP_LOG_PASS)
    mutt_debug(2, "Sending LOGIN command for %s...\n", idata->conn->account.user);

  snprintf(buf, sizeof(buf), "LOGIN %s %s", q_user, q_pass);
  rc = imap_exec(idata, buf, IMAP_CMD_FAIL_OK | IMAP_CMD_PASS);

  if (!rc)
  {
    mutt_clear_error(); /* clear "Logging in...".  fixes #3524 */
    return IMAP_AUTH_SUCCESS;
  }

  mutt_error(_("Login failed."));
  return IMAP_AUTH_FAILURE;
}
