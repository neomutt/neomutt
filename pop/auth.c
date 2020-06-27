/**
 * @file
 * POP authentication
 *
 * @authors
 * Copyright (C) 2000-2001 Vsevolod Volkov <vvv@mutt.org.ua>
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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
#include "private.h"
#include "mutt/lib.h"
#include "address/lib.h"
#include "conn/lib.h"
#include "lib.h"
#include "mutt_logging.h"
#include "mutt_socket.h"
#ifdef USE_SASL
#include <sasl/sasl.h>
#include <sasl/saslutil.h>
#endif

/* These Config Variables are only used in pop/pop_auth.c */
struct Slist *C_PopAuthenticators; ///< Config: (pop) List of allowed authentication methods
bool C_PopAuthTryAll; ///< Config: (pop) Try all available authentication methods

#ifdef USE_SASL
/**
 * pop_auth_sasl - POP SASL authenticator
 * @param adata  POP Account data
 * @param method Authentication method
 * @retval num Result, e.g. #POP_A_SUCCESS
 */
static enum PopAuthRes pop_auth_sasl(struct PopAccountData *adata, const char *method)
{
  sasl_conn_t *saslconn = NULL;
  sasl_interact_t *interaction = NULL;
  int rc;
  char inbuf[1024];
  const char *mech = NULL;
  const char *pc = NULL;
  unsigned int len = 0, olen = 0;

  if (mutt_account_getpass(&adata->conn->account) || !adata->conn->account.pass[0])
    return POP_A_FAILURE;

  if (mutt_sasl_client_new(adata->conn, &saslconn) < 0)
  {
    mutt_debug(LL_DEBUG1, "Error allocating SASL connection\n");
    return POP_A_FAILURE;
  }

  if (!method)
    method = adata->auth_list.data;

  while (true)
  {
    rc = sasl_client_start(saslconn, method, &interaction, &pc, &olen, &mech);
    if (rc != SASL_INTERACT)
      break;
    mutt_sasl_interact(interaction);
  }

  if ((rc != SASL_OK) && (rc != SASL_CONTINUE))
  {
    mutt_debug(
        LL_DEBUG1,
        "Failure starting authentication exchange. No shared mechanisms?\n");

    /* SASL doesn't support suggested mechanisms, so fall back */
    sasl_dispose(&saslconn);
    return POP_A_UNAVAIL;
  }

  /* About client_start:  If sasl_client_start() returns data via pc/olen,
   * the client is expected to send this first (after the AUTH string is sent).
   * sasl_client_start() may in fact return SASL_OK in this case.  */
  unsigned int client_start = olen;

  mutt_message(_("Authenticating (%s)..."), "SASL");

  size_t bufsize = MAX((olen * 2), 1024);
  char *buf = mutt_mem_malloc(bufsize);

  snprintf(buf, bufsize, "AUTH %s", mech);
  olen = strlen(buf);

  /* looping protocol */
  while (true)
  {
    mutt_str_copy(buf + olen, "\r\n", bufsize - olen);
    mutt_socket_send(adata->conn, buf);
    if (mutt_socket_readln_d(inbuf, sizeof(inbuf), adata->conn, MUTT_SOCK_LOG_FULL) < 0)
    {
      sasl_dispose(&saslconn);
      adata->status = POP_DISCONNECTED;
      FREE(&buf);
      return POP_A_SOCKET;
    }

    /* Note we don't exit if rc==SASL_OK when client_start is true.
     * This is because the first loop has only sent the AUTH string, we
     * need to loop at least once more to send the pc/olen returned
     * by sasl_client_start().  */
    if (!client_start && (rc != SASL_CONTINUE))
      break;

    if (mutt_str_startswith(inbuf, "+ ") &&
        (sasl_decode64(inbuf + 2, strlen(inbuf + 2), buf, bufsize - 1, &len) != SASL_OK))
    {
      mutt_debug(LL_DEBUG1, "error base64-decoding server response\n");
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
     * least one more line to the server.  */
    if ((rc != SASL_CONTINUE) && (rc != SASL_OK))
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
        mutt_debug(LL_DEBUG1, "error base64-encoding client response\n");
        goto bail;
      }
    }
  }

  if (rc != SASL_OK)
    goto bail;

  if (mutt_str_startswith(inbuf, "+OK"))
  {
    mutt_sasl_setup_conn(adata->conn, saslconn);
    FREE(&buf);
    return POP_A_SUCCESS;
  }

bail:
  sasl_dispose(&saslconn);

  /* terminate SASL session if the last response is not +OK nor -ERR */
  if (mutt_str_startswith(inbuf, "+ "))
  {
    snprintf(buf, bufsize, "*\r\n");
    if (pop_query(adata, buf, bufsize) == -1)
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
 * @param adata POP Account data
 * @param buf   Timestamp string
 */
void pop_apop_timestamp(struct PopAccountData *adata, char *buf)
{
  char *p1 = NULL, *p2 = NULL;

  FREE(&adata->timestamp);

  if ((p1 = strchr(buf, '<')) && (p2 = strchr(p1, '>')))
  {
    p2[1] = '\0';
    adata->timestamp = mutt_str_dup(p1);
  }
}

/**
 * pop_auth_apop - APOP authenticator
 * @param adata  POP Account data
 * @param method Authentication method
 * @retval num Result, e.g. #POP_A_SUCCESS
 */
static enum PopAuthRes pop_auth_apop(struct PopAccountData *adata, const char *method)
{
  struct Md5Ctx md5ctx;
  unsigned char digest[16];
  char hash[33];
  char buf[1024];

  if (mutt_account_getpass(&adata->conn->account) || !adata->conn->account.pass[0])
    return POP_A_FAILURE;

  if (!adata->timestamp)
    return POP_A_UNAVAIL;

  if (!mutt_addr_valid_msgid(adata->timestamp))
  {
    mutt_error(_("POP timestamp is invalid"));
    return POP_A_UNAVAIL;
  }

  mutt_message(_("Authenticating (%s)..."), "APOP");

  /* Compute the authentication hash to send to the server */
  mutt_md5_init_ctx(&md5ctx);
  mutt_md5_process(adata->timestamp, &md5ctx);
  mutt_md5_process(adata->conn->account.pass, &md5ctx);
  mutt_md5_finish_ctx(&md5ctx, digest);
  mutt_md5_toascii(digest, hash);

  /* Send APOP command to server */
  snprintf(buf, sizeof(buf), "APOP %s %s\r\n", adata->conn->account.user, hash);

  switch (pop_query(adata, buf, sizeof(buf)))
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
 * @param adata  POP Account data
 * @param method Authentication method
 * @retval num Result, e.g. #POP_A_SUCCESS
 */
static enum PopAuthRes pop_auth_user(struct PopAccountData *adata, const char *method)
{
  if (!adata->cmd_user)
    return POP_A_UNAVAIL;

  if (mutt_account_getpass(&adata->conn->account) || !adata->conn->account.pass[0])
    return POP_A_FAILURE;

  mutt_message(_("Logging in..."));

  char buf[1024];
  snprintf(buf, sizeof(buf), "USER %s\r\n", adata->conn->account.user);
  int ret = pop_query(adata, buf, sizeof(buf));

  if (adata->cmd_user == 2)
  {
    if (ret == 0)
    {
      adata->cmd_user = 1;

      mutt_debug(LL_DEBUG1, "set USER capability\n");
    }

    if (ret == -2)
    {
      adata->cmd_user = 0;

      mutt_debug(LL_DEBUG1, "unset USER capability\n");
      snprintf(adata->err_msg, sizeof(adata->err_msg), "%s",
               _("Command USER is not supported by server"));
    }
  }

  if (ret == 0)
  {
    snprintf(buf, sizeof(buf), "PASS %s\r\n", adata->conn->account.pass);
    ret = pop_query_d(adata, buf, sizeof(buf),
                      /* don't print the password unless we're at the ungodly debugging level */
                      (C_DebugLevel < MUTT_SOCK_LOG_FULL) ? "PASS *\r\n" : NULL);
  }

  switch (ret)
  {
    case 0:
      return POP_A_SUCCESS;
    case -1:
      return POP_A_SOCKET;
  }

  mutt_error("%s %s", _("Login failed"), adata->err_msg);

  return POP_A_FAILURE;
}

/**
 * pop_auth_oauth - Authenticate a POP connection using OAUTHBEARER
 * @param adata  POP Account data
 * @param method Name of this authentication method (UNUSED)
 * @retval num Result, e.g. #POP_A_SUCCESS
 */
static enum PopAuthRes pop_auth_oauth(struct PopAccountData *adata, const char *method)
{
  /* If they did not explicitly request or configure oauth then fail quietly */
  if (!method && !C_PopOauthRefreshCommand)
    return POP_A_UNAVAIL;

  mutt_message(_("Authenticating (%s)..."), "OAUTHBEARER");

  char *oauthbearer = mutt_account_getoauthbearer(&adata->conn->account);
  if (!oauthbearer)
    return POP_A_FAILURE;

  size_t auth_cmd_len = strlen(oauthbearer) + 30;
  char *auth_cmd = mutt_mem_malloc(auth_cmd_len);
  snprintf(auth_cmd, auth_cmd_len, "AUTH OAUTHBEARER %s\r\n", oauthbearer);
  FREE(&oauthbearer);

  int ret = pop_query_d(adata, auth_cmd, strlen(auth_cmd),
#ifdef DEBUG
                        /* don't print the bearer token unless we're at the ungodly debugging level */
                        (C_DebugLevel < MUTT_SOCK_LOG_FULL) ?
                            "AUTH OAUTHBEARER *\r\n" :
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
  mutt_socket_send(adata->conn, "\001");

  char *err = adata->err_msg;
  char decoded_err[1024];
  int len = mutt_b64_decode(adata->err_msg, decoded_err, sizeof(decoded_err) - 1);
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
 * @param adata POP Account data
 * @retval num Result, e.g. #POP_A_SUCCESS
 * @retval  0 Successful
 * @retval -1 Connection lost
 * @retval -2 Login failed
 * @retval -3 Authentication cancelled
 */
int pop_authenticate(struct PopAccountData *adata)
{
  struct ConnAccount *cac = &adata->conn->account;
  const struct PopAuth *authenticator = NULL;
  int attempts = 0;
  int ret = POP_A_UNAVAIL;

  if ((mutt_account_getuser(cac) < 0) || (cac->user[0] == '\0'))
  {
    return -3;
  }

  if (C_PopAuthenticators)
  {
    /* Try user-specified list of authentication methods */
    struct ListNode *np = NULL;
    STAILQ_FOREACH(np, &C_PopAuthenticators->head, entries)
    {
      mutt_debug(LL_DEBUG2, "Trying method %s\n", np->data);
      authenticator = pop_authenticators;

      while (authenticator->authenticate)
      {
        if (!authenticator->method || mutt_istr_equal(authenticator->method, np->data))
        {
          ret = authenticator->authenticate(adata, np->data);
          if (ret == POP_A_SOCKET)
          {
            switch (pop_connect(adata))
            {
              case 0:
              {
                ret = authenticator->authenticate(adata, np->data);
                break;
              }
              case -2:
                ret = POP_A_FAILURE;
            }
          }

          if (ret != POP_A_UNAVAIL)
            attempts++;
          if ((ret == POP_A_SUCCESS) || (ret == POP_A_SOCKET) ||
              ((ret == POP_A_FAILURE) && !C_PopAuthTryAll))
          {
            break;
          }
        }
        authenticator++;
      }
    }
  }
  else
  {
    /* Fall back to default: any authenticator */
    mutt_debug(LL_DEBUG2, "Using any available method\n");
    authenticator = pop_authenticators;

    while (authenticator->authenticate)
    {
      ret = authenticator->authenticate(adata, NULL);
      if (ret == POP_A_SOCKET)
      {
        switch (pop_connect(adata))
        {
          case 0:
          {
            ret = authenticator->authenticate(adata, NULL);
            break;
          }
          case -2:
            ret = POP_A_FAILURE;
        }
      }

      if (ret != POP_A_UNAVAIL)
        attempts++;
      if ((ret == POP_A_SUCCESS) || (ret == POP_A_SOCKET) ||
          ((ret == POP_A_FAILURE) && !C_PopAuthTryAll))
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
      if (attempts == 0)
        mutt_error(_("No authenticators available"));
  }

  return -2;
}
