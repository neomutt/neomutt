/**
 * @file
 * IMAP anonymous authentication method
 *
 * @authors
 * Copyright (C) 1999-2001,2005 Brendan Cully <brendan@kublai.com>
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
 * @page imap_auth_anon IMAP anonymous authentication method
 *
 * IMAP anonymous authentication method
 */

#include "config.h"
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
 * imap_auth_anon - Authenticate anonymously
 * @param idata  Server data
 * @param method Name of this authentication method
 * @retval enum Result, e.g. #IMAP_AUTH_SUCCESS
 *
 * this is basically a stripped-down version of the cram-md5 method.
 */
enum ImapAuthRes imap_auth_anon(struct ImapData *idata, const char *method)
{
  int rc;

  if (!mutt_bit_isset(idata->capabilities, AUTH_ANON))
    return IMAP_AUTH_UNAVAIL;

  if (mutt_account_getuser(&idata->conn->account) < 0)
    return IMAP_AUTH_FAILURE;

  if (idata->conn->account.user[0] != '\0')
    return IMAP_AUTH_UNAVAIL;

  mutt_message(_("Authenticating (anonymous)..."));

  imap_cmd_start(idata, "AUTHENTICATE ANONYMOUS");

  do
    rc = imap_cmd_step(idata);
  while (rc == IMAP_CMD_CONTINUE);

  if (rc != IMAP_CMD_RESPOND)
  {
    mutt_debug(1, "Invalid response from server.\n");
    goto bail;
  }

  mutt_socket_write(idata->conn, "ZHVtbXkK\r\n"); /* base64 ("dummy") */

  do
    rc = imap_cmd_step(idata);
  while (rc == IMAP_CMD_CONTINUE);

  if (rc != IMAP_CMD_OK)
  {
    mutt_debug(1, "Error receiving server response.\n");
    goto bail;
  }

  if (imap_code(idata->buf))
    return IMAP_AUTH_SUCCESS;

bail:
  mutt_error(_("Anonymous authentication failed."));
  return IMAP_AUTH_FAILURE;
}
