/**
 * @file
 * POP authentication
 *
 * @authors
 * Copyright (C) 2000-2001 Vsevolod Volkov <vvv@mutt.org.ua>
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
 * @page pop_auth POP authentication
 *
 * POP authentication
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "pop_private.h"
#include "mutt/mutt.h"
#include "email/lib.h"
#include "conn/conn.h"
#include "mutt_account.h"
#include "mutt_logging.h"
#include "mutt_socket.h"
#ifdef USE_SASL
#include <sasl/sasl.h>
#include <sasl/saslutil.h>
#endif

/* These Config Variables are only used in pop/pop_auth.c */
char *PopAuthenticators; ///< Config: (pop) List of allowed authentication methods
bool PopAuthTryAll; ///< Config: (pop) Try all available authentication methods

#ifdef USE_SASL
/**
 * pop_auth_sasl - POP SASL authenticator
 * @param mdata POP Mailbox data
 * @param method   Authentication method
 * @retval num Result, e.g. #POP_A_SUCCESS
 */
static enum PopAuthRes pop_auth_sasl(struct PopMboxData *mdata, const char *method)
{
  sasl_conn_t *saslconn = NULL;
  sasl_interact_t *interaction = NULL;
  int rc;
  char inbuf[LONG_STRING];
  const char *mech = NULL;
  const char *pc = NULL;
  unsigned int len = 0, olen = 0, client_start;

  if (mutt_account_getpass(&mdata->conn->account) || !mdata->conn->account.pass[0])
    return POP_A_FAILURE;

  if (mutt_sasl_client_new(mdata->conn, &saslconn) < 0)
  {
    mutt_debug(1, "Error allocating SASL connection.\n");
    return POP_A_FAILURE;
  }

  if (!method)
    method = mdata->auth_list;

  while (true)
  {
    rc = sasl_client_start(saslconn, method, &interaction, &pc, &olen, &mech);
    if (rc != SASL_INTERACT)
      break;
    mutt_sasl_interact(interaction);
  }

  if (rc != SASL_OK && rc != SASL_CONTINUE)
  {
    mutt_debug(
        1, "Failure starting authentication exchange. No shared mechanisms?\n");

    /* SASL doesn't support suggested mechanisms, so fall back */
    return POP_A_UNAVAIL;
  }

  /* About client_start:  If sasl_client_start() returns data via pc/olen,
   * the client is expected to send this first (after the AUTH string is sent).
   * sasl_client_start() may in fact return SASL_OK in this case.
   */
  client_start = olen;

  mutt_message(_("Authenticating (SASL)..."));

  size_t bufsize = ((olen * 2) > LONG_STRING) ? (olen * 2) : LONG_STRING;
  char *buf = mutt_mem_malloc(bufsize);

  snprintf(buf, bufsize, "AUTH %s", mech);
  olen = strlen(buf);

  /* looping protocol */
  while (true)
  {
    mutt_str_strfcpy(buf + olen, "\r\n", bufsize - olen);
    mutt_socket_send(mdata->conn, buf);
    if (mutt_socket_readln(inbuf, sizeof(inbuf), mdata->conn) < 0)
    {
      sasl_dispose(&saslconn);
      mdata->status = POP_DISCONNECTED;
      FREE(&buf);
      return POP_A_SOCKET;
    }

    /* Note we don't exit if rc==SASL_OK when client_start is true.
     * This is because the first loop has only sent the AUTH string, we
     * need to loop at least once more to send the pc/olen returned
     * by sasl_client_start().
     */
    if (!client_start && rc != SASL_CONTINUE)
      break;

    if ((mutt_str_strncmp(inbuf, "+ ", 2) == 0) &&
        sasl_decode64(inbuf + 2, strlen(inbuf + 2), buf, bufsize - 1, &len) != SASL_OK)
    {
      mutt_debug(1, "error base64-decoding server response.\n");
      goto bail;
    }

    if (!client_start)
    {
      while (true)
      {
        rc = sasl_client_step(saslconn, buf, len, &interaction, &pc, &olen);
        if (rc != SASL_INTERACT)
          break;
        mutt_sasl_interact(interaction);
      }
    }
    else
    {
      olen = client_start;
      client_start = 0;
    }

    /* Even if sasl_client_step() returns SASL_OK, we should send at
     * least one more line to the server.  See #3862.
     */
    if (rc != SASL_CONTINUE && rc != SASL_OK)
      break;

    /* send out response, or line break if none needed */
    if (pc)
    {
      if ((olen * 2) > bufsize)
      {
        bufsize = olen * 2;
        mutt_mem_realloc(&buf, bufsize);
      }
      if (sasl_encode64(pc, olen, buf, bufsize, &olen) != SASL_OK)
      {
        mutt_debug(1, "error base64-encoding client response.\n");
        goto bail;
      }
    }
  }

  if (rc != SASL_OK)
    goto bail;

  if (mutt_str_strncmp(inbuf, "+OK", 3) == 0)
  {
    mutt_sasl_setup_conn(mdata->conn, saslconn);
    FREE(&buf);
    return POP_A_SUCCESS;
  }

bail:
  sasl_dispose(&saslconn);

  /* terminate SASL session if the last response is not +OK nor -ERR */
  if (mutt_str_strncmp(inbuf, "+ ", 2) == 0)
  {
    snprintf(buf, bufsize, "*\r\n");
    if (pop_query(mdata, buf, bufsize) == -1)
    {
      FREE(&buf);
      return POP_A_SOCKET;
    }
  }

  FREE(&buf);
  mutt_error(_("SASL authentication failed"));

  return POP_A_FAILURE;
}
#endif

/**
 * pop_apop_timestamp - Get the server timestamp for APOP authentication
 * @param mdata POP Mailbox data
 * @param buf      Timestamp string
 */
void pop_apop_timestamp(struct PopMboxData *mdata, char *buf)
{
  char *p1 = NULL, *p2 = NULL;

  FREE(&mdata->timestamp);

  if ((p1 = strchr(buf, '<')) && (p2 = strchr(p1, '>')))
  {
    p2[1] = '\0';
    mdata->timestamp = mutt_str_strdup(p1);
  }
}

/**
 * pop_auth_apop - APOP authenticator
 * @param mdata POP Mailbox data
 * @param method   Authentication method
 * @retval num Result, e.g. #POP_A_SUCCESS
 */
static enum PopAuthRes pop_auth_apop(struct PopMboxData *mdata, const char *method)
{
  struct Md5Ctx ctx;
  unsigned char digest[16];
  char hash[33];
  char buf[LONG_STRING];

  if (mutt_account_getpass(&mdata->conn->account) || !mdata->conn->account.pass[0])
    return POP_A_FAILURE;

  if (!mdata->timestamp)
    return POP_A_UNAVAIL;

  if (!mutt_addr_valid_msgid(mdata->timestamp))
  {
    mutt_error(_("POP timestamp is invalid"));
    return POP_A_UNAVAIL;
  }

  mutt_message(_("Authenticating (APOP)..."));

  /* Compute the authentication hash to send to the server */
  mutt_md5_init_ctx(&ctx);
  mutt_md5_process(mdata->timestamp, &ctx);
  mutt_md5_process(mdata->conn->account.pass, &ctx);
  mutt_md5_finish_ctx(&ctx, digest);
  mutt_md5_toascii(digest, hash);

  /* Send APOP command to server */
  snprintf(buf, sizeof(buf), "APOP %s %s\r\n", mdata->conn->account.user, hash);

  switch (pop_query(mdata, buf, sizeof(buf)))
  {
    case 0:
      return POP_A_SUCCESS;
    case -1:
      return POP_A_SOCKET;
  }

  mutt_error(_("APOP authentication failed"));

  return POP_A_FAILURE;
}

/**
 * pop_auth_user - USER authenticator
 * @param mdata POP Mailbox data
 * @param method   Authentication method
 * @retval num Result, e.g. #POP_A_SUCCESS
 */
static enum PopAuthRes pop_auth_user(struct PopMboxData *mdata, const char *method)
{
  if (!mdata->cmd_user)
    return POP_A_UNAVAIL;

  if (mutt_account_getpass(&mdata->conn->account) || !mdata->conn->account.pass[0])
    return POP_A_FAILURE;

  mutt_message(_("Logging in..."));

  char buf[LONG_STRING];
  snprintf(buf, sizeof(buf), "USER %s\r\n", mdata->conn->account.user);
  int ret = pop_query(mdata, buf, sizeof(buf));

  if (mdata->cmd_user == 2)
  {
    if (ret == 0)
    {
      mdata->cmd_user = 1;

      mutt_debug(1, "set USER capability\n");
    }

    if (ret == -2)
    {
      mdata->cmd_user = 0;

      mutt_debug(1, "unset USER capability\n");
      snprintf(mdata->err_msg, sizeof(mdata->err_msg), "%s",
               _("Command USER is not supported by server"));
    }
  }

  if (ret == 0)
  {
    snprintf(buf, sizeof(buf), "PASS %s\r\n", mdata->conn->account.pass);
    ret = pop_query_d(mdata, buf, sizeof(buf),
                      /* don't print the password unless we're at the ungodly debugging level */
                      DebugLevel < MUTT_SOCK_LOG_FULL ? "PASS *\r\n" : NULL);
  }

  switch (ret)
  {
    case 0:
      return POP_A_SUCCESS;
    case -1:
      return POP_A_SOCKET;
  }

  mutt_error("%s %s", _("Login failed"), mdata->err_msg);

  return POP_A_FAILURE;
}

/**
 * pop_auth_oauth - Authenticate a POP connection using OAUTHBEARER
 * @param mdata POP Mailbox data
 * @param method Name of this authentication method (UNUSED)
 * @retval num Result, e.g. #POP_A_SUCCESS
 */
static enum PopAuthRes pop_auth_oauth(struct PopMboxData *mdata, const char *method)
{
  mutt_message(_("Authenticating (OAUTHBEARER)..."));

  char *oauthbearer = mutt_account_getoauthbearer(&mdata->conn->account);
  if (!oauthbearer)
    return POP_A_FAILURE;

  size_t auth_cmd_len = strlen(oauthbearer) + 30;
  char *auth_cmd = mutt_mem_malloc(auth_cmd_len);
  snprintf(auth_cmd, auth_cmd_len, "AUTH OAUTHBEARER %s\r\n", oauthbearer);
  FREE(&oauthbearer);

  int ret =
      pop_query_d(mdata, auth_cmd, strlen(auth_cmd),
#ifdef DEBUG
                  /* don't print the bearer token unless we're at the ungodly debugging level */
                  (DebugLevel < MUTT_SOCK_LOG_FULL) ? "AUTH OAUTHBEARER *\r\n" :
#endif
                                                      NULL);
  FREE(&auth_cmd);

  switch (ret)
  {
    case 0:
      return POP_A_SUCCESS;
    case -1:
      return POP_A_SOCKET;
  }

  /* The error response was a SASL continuation, so "continue" it.
   * See RFC7628 3.2.3 */
  mutt_socket_send(mdata->conn, "\001");

  char *err = mdata->err_msg;
  char decoded_err[LONG_STRING];
  int len = mutt_b64_decode(mdata->err_msg, decoded_err, sizeof(decoded_err) - 1);
  if (len >= 0)
  {
    decoded_err[len] = '\0';
    err = decoded_err;
  }
  mutt_error("%s %s", _("Authentication failed"), err);

  return POP_A_FAILURE;
}

static const struct PopAuth pop_authenticators[] = {
  { pop_auth_oauth, "oauthbearer" },
#ifdef USE_SASL
  { pop_auth_sasl, NULL },
#endif
  { pop_auth_apop, "apop" },         { pop_auth_user, "user" }, { NULL, NULL },
};

/**
 * pop_authenticate - Authenticate with a POP server
 * @param mdata POP Mailbox data
 * @retval num Result, e.g. #POP_A_SUCCESS
 * @retval  0 Successful
 * @retval -1 Connection lost
 * @retval -2 Login failed
 * @retval -3 Authentication cancelled
 */
int pop_authenticate(struct PopMboxData *mdata)
{
  struct ConnAccount *acct = &mdata->conn->account;
  const struct PopAuth *authenticator = NULL;
  char *methods = NULL;
  char *comma = NULL;
  char *method = NULL;
  int attempts = 0;
  int ret = POP_A_UNAVAIL;

  if ((mutt_account_getuser(acct) < 0) || (acct->user[0] == '\0'))
  {
    return -3;
  }

  if (PopAuthenticators && *PopAuthenticators)
  {
    /* Try user-specified list of authentication methods */
    methods = mutt_str_strdup(PopAuthenticators);
    method = methods;

    while (method)
    {
      comma = strchr(method, ':');
      if (comma)
        *comma++ = '\0';
      mutt_debug(2, "Trying method %s\n", method);
      authenticator = pop_authenticators;

      while (authenticator->authenticate)
      {
        if (!authenticator->method ||
            (mutt_str_strcasecmp(authenticator->method, method) == 0))
        {
          ret = authenticator->authenticate(mdata, method);
          if (ret == POP_A_SOCKET)
          {
            switch (pop_connect(mdata))
            {
              case 0:
              {
                ret = authenticator->authenticate(mdata, method);
                break;
              }
              case -2:
                ret = POP_A_FAILURE;
            }
          }

          if (ret != POP_A_UNAVAIL)
            attempts++;
          if (ret == POP_A_SUCCESS || ret == POP_A_SOCKET ||
              (ret == POP_A_FAILURE && !PopAuthTryAll))
          {
            comma = NULL;
            break;
          }
        }
        authenticator++;
      }

      method = comma;
    }

    FREE(&methods);
  }
  else
  {
    /* Fall back to default: any authenticator */
    mutt_debug(2, "Using any available method.\n");
    authenticator = pop_authenticators;

    while (authenticator->authenticate)
    {
      ret = authenticator->authenticate(mdata, authenticator->method);
      if (ret == POP_A_SOCKET)
      {
        switch (pop_connect(mdata))
        {
          case 0:
          {
            ret = authenticator->authenticate(mdata, authenticator->method);
            break;
          }
          case -2:
            ret = POP_A_FAILURE;
        }
      }

      if (ret != POP_A_UNAVAIL)
        attempts++;
      if (ret == POP_A_SUCCESS || ret == POP_A_SOCKET || (ret == POP_A_FAILURE && !PopAuthTryAll))
      {
        break;
      }

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
        mutt_error(_("No authenticators available"));
  }

  return -2;
}
