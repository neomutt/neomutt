/*
 * Copyright (C) 2000-3 Brendan Cully <brendan@kublai.com>
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

#ifdef USE_SASL2
#include <sasl/sasl.h>
#include <sasl/saslutil.h>
#else
#include <sasl.h>
#include <saslutil.h>
#endif

/* imap_auth_sasl: Default authenticator if available. */
imap_auth_res_t imap_auth_sasl (IMAP_DATA* idata, const char* method)
{
  sasl_conn_t* saslconn;
  sasl_interact_t* interaction = NULL;
  int rc, irc;
  char buf[HUGE_STRING];
  const char* mech;
#ifdef USE_SASL2
  const char *pc = NULL;
#else
  char* pc = NULL;
#endif
  unsigned int len, olen;
  unsigned char client_start;

  if (mutt_sasl_client_new (idata->conn, &saslconn) < 0)
  {
    dprint (1, (debugfile,
      "imap_auth_sasl: Error allocating SASL connection.\n"));
    return IMAP_AUTH_FAILURE;
  }

  rc = SASL_FAIL;

  /* If the user hasn't specified a method, use any available */
  if (!method)
  {
    method = idata->capstr;

    /* hack for SASL ANONYMOUS support:
     * 1. Fetch username. If it's "" or "anonymous" then
     * 2. attempt sasl_client_start with only "AUTH=ANONYMOUS" capability
     * 3. if sasl_client_start fails, fall through... */

    if (mutt_account_getuser (&idata->conn->account))
      return IMAP_AUTH_FAILURE;

    if (mutt_bit_isset (idata->capabilities, AUTH_ANON) &&
	(!idata->conn->account.user[0] ||
	 !ascii_strncmp (idata->conn->account.user, "anonymous", 9)))
#ifdef USE_SASL2
      rc = sasl_client_start (saslconn, "AUTH=ANONYMOUS", NULL, &pc, &olen, 
                              &mech);
#else
      rc = sasl_client_start (saslconn, "AUTH=ANONYMOUS", NULL, NULL, &pc, &olen,
			      &mech);
#endif
  }
  
  if (rc != SASL_OK && rc != SASL_CONTINUE)
    do
    {
#ifdef USE_SASL2
      rc = sasl_client_start (saslconn, method, &interaction,
        &pc, &olen, &mech);
#else
      rc = sasl_client_start (saslconn, method, NULL, &interaction,
        &pc, &olen, &mech);
#endif
      if (rc == SASL_INTERACT)
	mutt_sasl_interact (interaction);
    }
    while (rc == SASL_INTERACT);

  client_start = (olen > 0);

  if (rc != SASL_OK && rc != SASL_CONTINUE)
  {
    if (method)
      dprint (2, (debugfile, "imap_auth_sasl: %s unavailable\n", method));
    else
      dprint (1, (debugfile, "imap_auth_sasl: Failure starting authentication exchange. No shared mechanisms?\n"));
    /* SASL doesn't support LOGIN, so fall back */

    return IMAP_AUTH_UNAVAIL;
  }

  mutt_message (_("Authenticating (%s)..."), mech);

  snprintf (buf, sizeof (buf), "AUTHENTICATE %s", mech);
  imap_cmd_start (idata, buf);
  irc = IMAP_CMD_CONTINUE;

  /* looping protocol */
  while (rc == SASL_CONTINUE)
  {
    do
      irc = imap_cmd_step (idata);
    while (irc == IMAP_CMD_CONTINUE);

    if (irc == IMAP_CMD_BAD || irc == IMAP_CMD_NO)
      goto bail;

    if (irc == IMAP_CMD_RESPOND)
    {
#ifdef USE_SASL2
      if (sasl_decode64 (idata->cmd.buf+2, strlen (idata->cmd.buf+2), buf, LONG_STRING-1,
#else
      if (sasl_decode64 (idata->cmd.buf+2, strlen (idata->cmd.buf+2), buf,
#endif
			 &len) != SASL_OK)
      {
	dprint (1, (debugfile, "imap_auth_sasl: error base64-decoding server response.\n"));
	goto bail;
      }
    }

    if (!client_start)
    {
      do
      {
	rc = sasl_client_step (saslconn, buf, len, &interaction, &pc, &olen);
	if (rc == SASL_INTERACT)
	  mutt_sasl_interact (interaction);
      }
      while (rc == SASL_INTERACT);
    }
    else
      client_start = 0;

    /* send out response, or line break if none needed */
    if (olen)
    {
      if (sasl_encode64 (pc, olen, buf, sizeof (buf), &olen) != SASL_OK)
      {
	dprint (1, (debugfile, "imap_auth_sasl: error base64-encoding client response.\n"));
	goto bail;
      }

      /* sasl_client_st(art|ep) allocate pc with malloc, expect me to 
       * free it */
#ifndef USE_SASL2
      FREE (&pc);
#endif
    }
    
    if (irc == IMAP_CMD_RESPOND)
    {
      strfcpy (buf + olen, "\r\n", sizeof (buf) - olen);
      mutt_socket_write (idata->conn, buf);
    }

    /* If SASL has errored out, send an abort string to the server */
    if (rc < 0)
    {
      mutt_socket_write (idata->conn, "*\r\n");
      dprint (1, (debugfile, "imap_auth_sasl: sasl_client_step error %d\n",rc));
    }
  }

  while (irc != IMAP_CMD_OK)
    if ((irc = imap_cmd_step (idata)) != IMAP_CMD_CONTINUE)
      break;

  if (rc != SASL_OK)
    goto bail;

  if (imap_code (idata->cmd.buf))
  {
    mutt_sasl_setup_conn (idata->conn, saslconn);
    return IMAP_AUTH_SUCCESS;
  }

 bail:
  mutt_error _("SASL authentication failed.");
  mutt_sleep(2);
  sasl_dispose (&saslconn);

  return IMAP_AUTH_FAILURE;
}
