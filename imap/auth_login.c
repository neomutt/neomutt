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

/* plain LOGIN support */

#include "config.h"
#include <stdio.h>
#include "imap_private.h"
#include "account.h"
#include "auth.h"
#include "globals.h"
#include "lib.h"
#include "mutt_socket.h"
#include "options.h"
#include "protos.h"

/**
 * imap_auth_login - Plain LOGIN support
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

  if (mutt_account_getuser(&idata->conn->account))
    return IMAP_AUTH_FAILURE;
  if (mutt_account_getpass(&idata->conn->account))
    return IMAP_AUTH_FAILURE;

  mutt_message(_("Logging in..."));

  imap_quote_string(q_user, sizeof(q_user), idata->conn->account.user);
  imap_quote_string(q_pass, sizeof(q_pass), idata->conn->account.pass);

#ifdef DEBUG
  /* don't print the password unless we're at the ungodly debugging level
   * of 5 or higher */

  if (debuglevel < IMAP_LOG_PASS)
    mutt_debug(2, "Sending LOGIN command for %s...\n", idata->conn->account.user);
#endif

  snprintf(buf, sizeof(buf), "LOGIN %s %s", q_user, q_pass);
  rc = imap_exec(idata, buf, IMAP_CMD_FAIL_OK | IMAP_CMD_PASS);

  if (!rc)
  {
    mutt_clear_error(); /* clear "Logging in...".  fixes #3524 */
    return IMAP_AUTH_SUCCESS;
  }

  mutt_error(_("Login failed."));
  mutt_sleep(2);
  return IMAP_AUTH_FAILURE;
}
