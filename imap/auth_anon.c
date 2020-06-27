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
#include "private.h"
#include "mutt/lib.h"
#include "conn/lib.h"
#include "auth.h"
#include "mutt_socket.h"

/**
 * imap_auth_anon - Authenticate anonymously - Implements ImapAuth::authenticate()
 * @param adata Imap Account data
 * @param method Name of this authentication method
 * @retval #ImapAuthRes Result, e.g. #IMAP_AUTH_SUCCESS
 *
 * this is basically a stripped-down version of the cram-md5 method.
 */
enum ImapAuthRes imap_auth_anon(struct ImapAccountData *adata, const char *method)
{
  int rc;

  if (!(adata->capabilities & IMAP_CAP_AUTH_ANONYMOUS))
    return IMAP_AUTH_UNAVAIL;

  if (mutt_account_getuser(&adata->conn->account) < 0)
    return IMAP_AUTH_FAILURE;

  if (adata->conn->account.user[0] != '\0')
    return IMAP_AUTH_UNAVAIL;

  mutt_message(_("Authenticating (%s)..."), "anonymous");

  imap_cmd_start(adata, "AUTHENTICATE ANONYMOUS");

  do
  {
    rc = imap_cmd_step(adata);
  } while (rc == IMAP_RES_CONTINUE);

  if (rc != IMAP_RES_RESPOND)
  {
    mutt_debug(LL_DEBUG1, "Invalid response from server\n");
    goto bail;
  }

  mutt_socket_send(adata->conn, "ZHVtbXkK\r\n"); /* base64 ("dummy") */

  do
  {
    rc = imap_cmd_step(adata);
  } while (rc == IMAP_RES_CONTINUE);

  if (rc != IMAP_RES_OK)
  {
    mutt_debug(LL_DEBUG1, "Error receiving server response\n");
    goto bail;
  }

  if (imap_code(adata->buf))
    return IMAP_AUTH_SUCCESS;

bail:
  mutt_error(_("Anonymous authentication failed"));
  return IMAP_AUTH_FAILURE;
}
