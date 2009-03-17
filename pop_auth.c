/*
 * Copyright (C) 2000-2001 Vsevolod Volkov <vvv@mutt.org.ua>
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

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "mx.h"
#include "md5.h"
#include "pop.h"

#include <string.h>
#include <unistd.h>

#ifdef USE_SASL
#include <sasl/sasl.h>
#include <sasl/saslutil.h>

#include "mutt_sasl.h"
#endif

#ifdef USE_SASL
/* SASL authenticator */
static pop_auth_res_t pop_auth_sasl (POP_DATA *pop_data, const char *method)
{
  sasl_conn_t *saslconn;
  sasl_interact_t *interaction = NULL;
  int rc;
  char buf[LONG_STRING];
  char inbuf[LONG_STRING];
  const char* mech;
  const char *pc = NULL;
  unsigned int len, olen, client_start;

  if (mutt_sasl_client_new (pop_data->conn, &saslconn) < 0)
  {
    dprint (1, (debugfile, "pop_auth_sasl: Error allocating SASL connection.\n"));
    return POP_A_FAILURE;
  }

  if (!method)
    method = pop_data->auth_list;

  FOREVER
  {
    rc = sasl_client_start(saslconn, method, &interaction, &pc, &olen, &mech);
    if (rc != SASL_INTERACT)
      break;
    mutt_sasl_interact (interaction);
  }

  if (rc != SASL_OK && rc != SASL_CONTINUE)
  {
    dprint (1, (debugfile, "pop_auth_sasl: Failure starting authentication exchange. No shared mechanisms?\n"));

    /* SASL doesn't support suggested mechanisms, so fall back */
    return POP_A_UNAVAIL;
  }

  client_start = olen;

  mutt_message _("Authenticating (SASL)...");

  snprintf (buf, sizeof (buf), "AUTH %s", mech);
  olen = strlen (buf);

  /* looping protocol */
  FOREVER
  {
    strfcpy (buf + olen, "\r\n", sizeof (buf) - olen);
    mutt_socket_write (pop_data->conn, buf);
    if (mutt_socket_readln (inbuf, sizeof (inbuf), pop_data->conn) < 0)
    {
      sasl_dispose (&saslconn);
      pop_data->status = POP_DISCONNECTED;
      return POP_A_SOCKET;
    }

    if (!client_start && rc != SASL_CONTINUE)
      break;

    if (!mutt_strncmp (inbuf, "+ ", 2)
        && sasl_decode64 (inbuf+2, strlen (inbuf+2), buf, LONG_STRING-1, &len) != SASL_OK)
    {
      dprint (1, (debugfile, "pop_auth_sasl: error base64-decoding server response.\n"));
      goto bail;
    }

    if (!client_start)
      FOREVER
      {
	rc = sasl_client_step (saslconn, buf, len, &interaction, &pc, &olen);
	if (rc != SASL_INTERACT)
	  break;
	mutt_sasl_interact (interaction);
      }
    else
    {
      olen = client_start;
      client_start = 0;
    }

    if (rc != SASL_CONTINUE && (olen == 0 || rc != SASL_OK))
      break;

    /* send out response, or line break if none needed */
    if (pc)
    {
      if (sasl_encode64 (pc, olen, buf, sizeof (buf), &olen) != SASL_OK)
      {
	dprint (1, (debugfile, "pop_auth_sasl: error base64-encoding client response.\n"));
	goto bail;
      }
    }
  }

  if (rc != SASL_OK)
    goto bail;

  if (!mutt_strncmp (inbuf, "+OK", 3))
  {
    mutt_sasl_setup_conn (pop_data->conn, saslconn);
    return POP_A_SUCCESS;
  }

bail:
  sasl_dispose (&saslconn);

  /* terminate SASL sessoin if the last responce is not +OK nor -ERR */
  if (!mutt_strncmp (inbuf, "+ ", 2))
  {
    snprintf (buf, sizeof (buf), "*\r\n");
    if (pop_query (pop_data, buf, sizeof (buf)) == -1)
      return POP_A_SOCKET;
  }

  mutt_error _("SASL authentication failed.");
  mutt_sleep (2);

  return POP_A_FAILURE;
}
#endif

/* Get the server timestamp for APOP authentication */
void pop_apop_timestamp (POP_DATA *pop_data, char *buf)
{
  char *p1, *p2;

  FREE (&pop_data->timestamp);

  if ((p1 = strchr (buf, '<')) && (p2 = strchr (p1, '>')))
  {
    p2[1] = '\0';
    pop_data->timestamp = safe_strdup (p1);
  }
}

/* APOP authenticator */
static pop_auth_res_t pop_auth_apop (POP_DATA *pop_data, const char *method)
{
  struct md5_ctx ctx;
  unsigned char digest[16];
  char hash[33];
  char buf[LONG_STRING];
  int i;

  if (!pop_data->timestamp)
    return POP_A_UNAVAIL;

  if (rfc822_valid_msgid (pop_data->timestamp) < 0)
  {
    mutt_error _("POP timestamp is invalid!");
    mutt_sleep (2);
    return POP_A_UNAVAIL;
  }

  mutt_message _("Authenticating (APOP)...");

  /* Compute the authentication hash to send to the server */
  md5_init_ctx (&ctx);
  md5_process_bytes (pop_data->timestamp, strlen (pop_data->timestamp), &ctx);
  md5_process_bytes (pop_data->conn->account.pass,
		     strlen (pop_data->conn->account.pass), &ctx);
  md5_finish_ctx (&ctx, digest);

  for (i = 0; i < sizeof (digest); i++)
    sprintf (hash + 2 * i, "%02x", digest[i]);

  /* Send APOP command to server */
  snprintf (buf, sizeof (buf), "APOP %s %s\r\n", pop_data->conn->account.user, hash);

  switch (pop_query (pop_data, buf, sizeof (buf)))
  {
    case 0:
      return POP_A_SUCCESS;
    case -1:
      return POP_A_SOCKET;
  }

  mutt_error _("APOP authentication failed.");
  mutt_sleep (2);

  return POP_A_FAILURE;
}

/* USER authenticator */
static pop_auth_res_t pop_auth_user (POP_DATA *pop_data, const char *method)
{
  char buf[LONG_STRING];
  int ret;

  if (!pop_data->cmd_user)
    return POP_A_UNAVAIL;

  mutt_message _("Logging in...");

  snprintf (buf, sizeof (buf), "USER %s\r\n", pop_data->conn->account.user);
  ret = pop_query (pop_data, buf, sizeof (buf));

  if (pop_data->cmd_user == 2)
  {
    if (ret == 0)
    {
      pop_data->cmd_user = 1;

      dprint (1, (debugfile, "pop_auth_user: set USER capability\n"));
    }

    if (ret == -2)
    {
      pop_data->cmd_user = 0;

      dprint (1, (debugfile, "pop_auth_user: unset USER capability\n"));
      snprintf (pop_data->err_msg, sizeof (pop_data->err_msg),
              _("Command USER is not supported by server."));
    }
  }

  if (ret == 0)
  {
    snprintf (buf, sizeof (buf), "PASS %s\r\n", pop_data->conn->account.pass);
    ret = pop_query_d (pop_data, buf, sizeof (buf), 
#ifdef DEBUG
	/* don't print the password unless we're at the ungodly debugging level */
	debuglevel < M_SOCK_LOG_FULL ? "PASS *\r\n" :
#endif
	NULL);
  }

  switch (ret)
  {
    case 0:
      return POP_A_SUCCESS;
    case -1:
      return POP_A_SOCKET;
  }

  mutt_error ("%s %s", _("Login failed."), pop_data->err_msg);
  mutt_sleep (2);

  return POP_A_FAILURE;
}

static pop_auth_t pop_authenticators[] = {
#ifdef USE_SASL
  { pop_auth_sasl, NULL },
#endif
  { pop_auth_apop, "apop" },
  { pop_auth_user, "user" },
  { NULL,	   NULL }
};

/*
 * Authentication
 *  0 - successful,
 * -1 - conection lost,
 * -2 - login failed,
 * -3 - authentication canceled.
*/
int pop_authenticate (POP_DATA* pop_data)
{
  ACCOUNT *acct = &pop_data->conn->account;
  pop_auth_t* authenticator;
  char* methods;
  char* comma;
  char* method;
  int attempts = 0;
  int ret = POP_A_UNAVAIL;

  if (mutt_account_getuser (acct) || !acct->user[0] ||
      mutt_account_getpass (acct) || !acct->pass[0])
    return -3;

  if (PopAuthenticators && *PopAuthenticators)
  {
    /* Try user-specified list of authentication methods */
    methods = safe_strdup (PopAuthenticators);
    method = methods;

    while (method)
    {
      comma = strchr (method, ':');
      if (comma)
	*comma++ = '\0';
      dprint (2, (debugfile, "pop_authenticate: Trying method %s\n", method));
      authenticator = pop_authenticators;

      while (authenticator->authenticate)
      {
	if (!authenticator->method ||
	    !ascii_strcasecmp (authenticator->method, method))
	{
	  ret = authenticator->authenticate (pop_data, method);
	  if (ret == POP_A_SOCKET)
	    switch (pop_connect (pop_data))
	    {
	      case 0:
	      {
		ret = authenticator->authenticate (pop_data, method);
		break;
	      }
	      case -2:
		ret = POP_A_FAILURE;
	    }

	  if (ret != POP_A_UNAVAIL)
	    attempts++;
	  if (ret == POP_A_SUCCESS || ret == POP_A_SOCKET ||
	      (ret == POP_A_FAILURE && !option (OPTPOPAUTHTRYALL)))
	  {
	    comma = NULL;
	    break;
	  }
	}
	authenticator++;
      }

      method = comma;
    }

    FREE (&methods);
  }
  else
  {
    /* Fall back to default: any authenticator */
    dprint (2, (debugfile, "pop_authenticate: Using any available method.\n"));
    authenticator = pop_authenticators;

    while (authenticator->authenticate)
    {
      ret = authenticator->authenticate (pop_data, authenticator->method);
      if (ret == POP_A_SOCKET)
	switch (pop_connect (pop_data))
	{
	  case 0:
	  {
	    ret = authenticator->authenticate (pop_data, authenticator->method);
	    break;
	  }
	  case -2:
	    ret = POP_A_FAILURE;
	}

      if (ret != POP_A_UNAVAIL)
	attempts++;
      if (ret == POP_A_SUCCESS || ret == POP_A_SOCKET ||
	  (ret == POP_A_FAILURE && !option (OPTPOPAUTHTRYALL)))
	break;

      authenticator++;
    }
  }

  switch (ret)
  {
    case POP_A_SUCCESS:
      return 0;
    case POP_A_SOCKET:
      return -1;
    case POP_A_UNAVAIL:
      if (!attempts)
	mutt_error (_("No authenticators available"));
  }

  return -2;
}
