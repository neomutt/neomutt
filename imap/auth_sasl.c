/*
 * Copyright (C) 2000 Brendan Cully <brendan@kublai.com>
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
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */ 

/* SASL login/authentication code */

#include "mutt.h"
#include "mutt_sasl.h"
#include "imap_private.h"
#include "auth.h"

#include <sasl.h>
#include <saslutil.h>

/* imap_auth_sasl: Default authenticator if available. */
imap_auth_res_t imap_auth_sasl (IMAP_DATA* idata)
{
  sasl_conn_t* saslconn;
  int rc;
  char buf[LONG_STRING];
  const char* mech;
  char* pc;
  unsigned int len, olen;
  unsigned char client_start;

  if (mutt_sasl_start () != SASL_OK)
    return IMAP_AUTH_FAILURE;

  /* TODO: set fourth option to SASL_SECURITY_LAYER once we have a wrapper
   *  (ie more than auth code) for SASL. */
  rc = sasl_client_new ("imap", idata->conn->account.host,
    mutt_sasl_get_callbacks (&idata->conn->account), 0, &saslconn);

  if (rc != SASL_OK)
  {
    dprint (1, (debugfile, "imap_auth_sasl: Error allocating SASL connection.\n"));
    return IMAP_AUTH_FAILURE;
  }

  /* hack for SASL ANONYMOUS support:
   * 1. Fetch username. If it's "" or "anonymous" then
   * 2. attempt sasl_client_start with only "AUTH=ANONYMOUS" capability
   * 3. if sasl_client_start fails, fall through... */
  rc = SASL_FAIL;

  if (mutt_account_getuser (&idata->conn->account))
    return IMAP_AUTH_FAILURE;

  if (mutt_bit_isset (idata->capabilities, AUTH_ANON) &&
      (!idata->conn->account.user[0] ||
       !mutt_strncmp (idata->conn->account.user, "anonymous", 9)))
    rc = sasl_client_start (saslconn, "AUTH=ANONYMOUS", NULL, NULL, &pc, &olen,
      &mech);

  if (rc != SASL_OK && rc != SASL_CONTINUE)
    rc = sasl_client_start (saslconn, idata->capstr, NULL, NULL, &pc, &olen,
      &mech);

  client_start = (olen > 0);

  if (rc != SASL_OK && rc != SASL_CONTINUE)
  {
    dprint (1, (debugfile, "imap_auth_sasl: Failure starting authentication exchange. No shared mechanisms?\n"));
    /* SASL doesn't support LOGIN, so fall back */

    return IMAP_AUTH_UNAVAIL;
  }

  mutt_message _("Authenticating (SASL)...");

  snprintf (buf, sizeof (buf), "AUTHENTICATE %s", mech);
  imap_cmd_start (idata, buf);

  /* looping protocol */
  while (rc == SASL_CONTINUE)
  {
    if (mutt_socket_readln (buf, sizeof (buf), idata->conn) < 0)
      goto bail;

    if (!mutt_strncmp (buf, "+ ", 2))
    {
      if (sasl_decode64 (buf+2, strlen (buf+2), buf, &len) != SASL_OK)
      {
	dprint (1, (debugfile, "imap_auth_sasl: error base64-decoding server response.\n"));
	goto bail;
      }
    }
    else if ((buf[0] == '*'))
    {
      if (imap_handle_untagged (idata, buf))
	goto bail;
      else continue;
    }

    if (!client_start)
      rc = sasl_client_step (saslconn, buf, len, NULL, &pc, &olen);
    else
      client_start = 0;

    /* send out response, or line break if none needed */
    if (olen && sasl_encode64 (pc, olen, buf, sizeof (buf), &olen) != SASL_OK)
    {
      dprint (1, (debugfile, "imap_auth_sasl: error base64-encoding client response.\n"));
      goto bail;
    }

    if (olen || rc == SASL_CONTINUE)
    {
      strfcpy (buf + olen, "\r\n", sizeof (buf) - olen);
      mutt_socket_write (idata->conn, buf);
    }
  }

  while (mutt_strncmp (buf, idata->seq, SEQLEN))
    if (mutt_socket_readln (buf, sizeof (buf), idata->conn) < 0)
      goto bail;

  if (rc != SASL_OK)
    goto bail;

  if (imap_code (buf))
  {
    /* later we'll want to keep saslconn, when we support a protection layer.
     * For now it shouldn't hurt to dispose of it at this point. */
    sasl_dispose (&saslconn);
    return IMAP_AUTH_SUCCESS;
  }

 bail:
  mutt_error _("SASL authentication failed.");
  sleep(2);
  sasl_dispose (&saslconn);

  return IMAP_AUTH_FAILURE;
}
