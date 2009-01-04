/*
 * Copyright (C) 1999-2001,2005 Brendan Cully <brendan@kublai.com>
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

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "imap_private.h"
#include "auth.h"

/* this is basically a stripped-down version of the cram-md5 method. */
imap_auth_res_t imap_auth_anon (IMAP_DATA* idata, const char* method)
{
  int rc;

  if (!mutt_bit_isset (idata->capabilities, AUTH_ANON))
    return IMAP_AUTH_UNAVAIL;

  if (mutt_account_getuser (&idata->conn->account))
    return IMAP_AUTH_FAILURE;

  if (idata->conn->account.user[0] != '\0')
    return IMAP_AUTH_UNAVAIL;

  mutt_message _("Authenticating (anonymous)...");

  imap_cmd_start (idata, "AUTHENTICATE ANONYMOUS");

  do
    rc = imap_cmd_step (idata);
  while (rc == IMAP_CMD_CONTINUE);

  if (rc != IMAP_CMD_RESPOND)
  {
    dprint (1, (debugfile, "Invalid response from server.\n"));
    goto bail;
  }

  mutt_socket_write (idata->conn, "ZHVtbXkK\r\n"); /* base64 ("dummy") */

  do
    rc = imap_cmd_step (idata);
  while (rc == IMAP_CMD_CONTINUE);
  
  if (rc != IMAP_CMD_OK)
  {
    dprint (1, (debugfile, "Error receiving server response.\n"));
    goto bail;
  }

  if (imap_code (idata->buf))
    return IMAP_AUTH_SUCCESS;

 bail:
  mutt_error _("Anonymous authentication failed.");
  mutt_sleep (2);
  return IMAP_AUTH_FAILURE;
}
