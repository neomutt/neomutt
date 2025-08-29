/**
 * @file
 * POP authentication
 *
 * @authors
 * Copyright (C) 2000-2001 Vsevolod Volkov <vvv@mutt.org.ua>
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2018-2020 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020 Yousef Akbar <yousef@yhakbar.com>
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
#include "config/lib.h"
#include "core/lib.h"
#include "conn/lib.h"
#include "adata.h"
#ifdef USE_SASL_CYRUS
#include <sasl/sasl.h>
#include <sasl/saslutil.h>
#endif
#ifdef USE_SASL_GNU
#include <gsasl.h>
#endif

#ifdef USE_SASL_GNU
/**
 * pop_auth_gsasl - POP GNU SASL authenticator - Implements PopAuth::authenticate() - @ingroup pop_authenticate
 */
static enum PopAuthRes pop_auth_gsasl(struct PopAccountData *adata, const char *method)
{
  Gsasl_session *gsasl_session = NULL;
  struct Buffer *output_buf = NULL;
  struct Buffer *input_buf = NULL;
  int rc = POP_A_FAILURE;
  int gsasl_rc = GSASL_OK;

  const char *chosen_mech = mutt_gsasl_get_mech(method, buf_string(&adata->auth_list));
  if (!chosen_mech)
  {
    mutt_debug(LL_DEBUG2, "returned no usable mech\n");
    return POP_A_UNAVAIL;
  }

  mutt_debug(LL_DEBUG2, "using mech %s\n", chosen_mech);

  if (mutt_gsasl_client_new(adata->conn, chosen_mech, &gsasl_session) < 0)
  {
    mutt_debug(LL_DEBUG1, "Error allocating GSASL connection\n");
    return POP_A_UNAVAIL;
  }

  mutt_message(_("Authenticating (%s)..."), chosen_mech);

  output_buf = buf_pool_get();
  input_buf = buf_pool_get();
  buf_printf(output_buf, "AUTH %s\r\n", chosen_mech);

  do
  {
    if (mutt_socket_send(adata->conn, buf_string(output_buf)) < 0)
    {
      adata->status = POP_DISCONNECTED;
      rc = POP_A_SOCKET;
      goto fail;
    }

    if (mutt_socket_buffer_readln(input_buf, adata->conn) < 0)
    {
      adata->status = POP_DISCONNECTED;
      rc = POP_A_SOCKET;
      goto fail;
    }

    if (!mutt_strn_equal(buf_string(input_buf), "+ ", 2))
      break;

    const char *pop_auth_data = buf_string(input_buf) + 2;
    char *gsasl_step_output = NULL;
    gsasl_rc = gsasl_step64(gsasl_session, pop_auth_data, &gsasl_step_output);
    if ((gsasl_rc == GSASL_NEEDS_MORE) || (gsasl_rc == GSASL_OK))
    {
      buf_strcpy(output_buf, gsasl_step_output);
      buf_addstr(output_buf, "\r\n");
      gsasl_free(gsasl_step_output);
    }
    else
    {
      mutt_debug(LL_DEBUG1, "gsasl_step64() failed (%d): %s\n", gsasl_rc,
                 gsasl_strerror(gsasl_rc));
    }
  } while ((gsasl_rc == GSASL_NEEDS_MORE) || (gsasl_rc == GSASL_OK));

  if (mutt_strn_equal(buf_string(input_buf), "+ ", 2))
  {
    mutt_socket_send(adata->conn, "*\r\n");
    goto fail;
  }

  if (mutt_strn_equal(buf_string(input_buf), "+OK", 3) && (gsasl_rc == GSASL_OK))
    rc = POP_A_SUCCESS;

fail:
  buf_pool_release(&input_buf);
  buf_pool_release(&output_buf);
  mutt_gsasl_client_finish(&gsasl_session);

  if (rc == POP_A_FAILURE)
  {
    mutt_debug(LL_DEBUG2, "%s failed\n", chosen_mech);
    mutt_error(_("SASL authentication failed"));
  }

  return rc;
}
#endif

#ifdef USE_SASL_CYRUS
/**
 * pop_auth_sasl - POP SASL authenticator - Implements PopAuth::authenticate() - @ingroup pop_authenticate
 */
static enum PopAuthRes pop_auth_sasl(struct PopAccountData *adata, const char *method)
{
  sasl_conn_t *saslconn = NULL;
  sasl_interact_t *interaction = NULL;
  int rc;
  char inbuf[1024] = { 0 };
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
    mutt_debug(LL_DEBUG1, "Failure starting authentication exchange. No shared mechanisms?\n");

    /* SASL doesn't support suggested mechanisms, so fall back */
    sasl_dispose(&saslconn);
    return POP_A_UNAVAIL;
  }

  /* About client_start:  If sasl_client_start() returns data via pc/olen,
   * the client is expected to send this first (after the AUTH string is sent).
   * sasl_client_start() may in fact return SASL_OK in this case.  */
  unsigned int client_start = olen;

  // L10N: (%s) is the method name, e.g. Anonymous, CRAM-MD5, GSSAPI, SASL
  mutt_message(_("Authenticating (%s)..."), "SASL");

  size_t bufsize = MAX((olen * 2), 1024);
  char *buf = MUTT_MEM_MALLOC(bufsize, char);

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

    if (client_start)
    {
      olen = client_start;
      client_start = 0;
    }
    else
    {
      while (true)
      {
        rc = sasl_client_step(saslconn, buf, len, &interaction, &pc, &olen);
        if (rc != SASL_INTERACT)
          break;
        mutt_sasl_interact(interaction);
      }
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
        MUTT_MEM_REALLOC(&buf, bufsize, char);
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
  // L10N: %s is the method name, e.g. Anonymous, CRAM-MD5, GSSAPI, SASL
  mutt_error(_("%s authentication failed"), "SASL");

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
 * pop_auth_apop - APOP authenticator - Implements PopAuth::authenticate() - @ingroup pop_authenticate
 */
static enum PopAuthRes pop_auth_apop(struct PopAccountData *adata, const char *method)
{
  struct Md5Ctx md5ctx = { 0 };
  unsigned char digest[16];
  char hash[33] = { 0 };
  char buf[1024] = { 0 };

  if (mutt_account_getpass(&adata->conn->account) || !adata->conn->account.pass[0])
    return POP_A_FAILURE;

  if (!adata->timestamp)
    return POP_A_UNAVAIL;

  if (!mutt_addr_valid_msgid(adata->timestamp))
  {
    mutt_error(_("POP timestamp is invalid"));
    return POP_A_UNAVAIL;
  }

  // L10N: (%s) is the method name, e.g. Anonymous, CRAM-MD5, GSSAPI, SASL
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

  // L10N: %s is the method name, e.g. Anonymous, CRAM-MD5, GSSAPI, SASL
  mutt_error(_("%s authentication failed"), "APOP");

  return POP_A_FAILURE;
}

/**
 * pop_auth_user - USER authenticator - Implements PopAuth::authenticate() - @ingroup pop_authenticate
 */
static enum PopAuthRes pop_auth_user(struct PopAccountData *adata, const char *method)
{
  if (!adata->cmd_user)
    return POP_A_UNAVAIL;

  if (mutt_account_getpass(&adata->conn->account) || !adata->conn->account.pass[0])
    return POP_A_FAILURE;

  mutt_message(_("Logging in..."));

  char buf[1024] = { 0 };
  snprintf(buf, sizeof(buf), "USER %s\r\n", adata->conn->account.user);
  int rc = pop_query(adata, buf, sizeof(buf));

  if (adata->cmd_user == 2)
  {
    if (rc == 0)
    {
      adata->cmd_user = 1;

      mutt_debug(LL_DEBUG1, "set USER capability\n");
    }

    if (rc == -2)
    {
      adata->cmd_user = 0;

      mutt_debug(LL_DEBUG1, "unset USER capability\n");
      snprintf(adata->err_msg, sizeof(adata->err_msg), "%s",
               _("Command USER is not supported by server"));
    }
  }

  if (rc == 0)
  {
    snprintf(buf, sizeof(buf), "PASS %s\r\n", adata->conn->account.pass);
    const short c_debug_level = cs_subset_number(NeoMutt->sub, "debug_level");
    rc = pop_query_d(adata, buf, sizeof(buf),
                     /* don't print the password unless we're at the ungodly debugging level */
                     (c_debug_level < MUTT_SOCK_LOG_FULL) ? "PASS *\r\n" : NULL);
  }

  switch (rc)
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
 * pop_auth_oauth - Authenticate a POP connection using OAUTHBEARER - Implements PopAuth::authenticate() - @ingroup pop_authenticate
 */
static enum PopAuthRes pop_auth_oauth(struct PopAccountData *adata, const char *method)
{
  /* If they did not explicitly request or configure oauth then fail quietly */
  const char *const c_pop_oauth_refresh_command = cs_subset_string(NeoMutt->sub, "pop_oauth_refresh_command");
  if (!method && !c_pop_oauth_refresh_command)
    return POP_A_UNAVAIL;

  // L10N: (%s) is the method name, e.g. Anonymous, CRAM-MD5, GSSAPI, SASL
  mutt_message(_("Authenticating (%s)..."), "OAUTHBEARER");

  char *oauthbearer = mutt_account_getoauthbearer(&adata->conn->account, false);
  if (!oauthbearer)
    return POP_A_FAILURE;

  char *auth_cmd = NULL;
  mutt_str_asprintf(&auth_cmd, "AUTH OAUTHBEARER %s\r\n", oauthbearer);
  FREE(&oauthbearer);

  int rc = pop_query_d(adata, auth_cmd, strlen(auth_cmd),
#ifdef DEBUG
                       /* don't print the bearer token unless we're at the ungodly debugging level */
                       (cs_subset_number(NeoMutt->sub, "debug_level") < MUTT_SOCK_LOG_FULL) ?
                           "AUTH OAUTHBEARER *\r\n" :
#endif
                           NULL);
  FREE(&auth_cmd);

  switch (rc)
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
  char decoded_err[1024] = { 0 };
  int len = mutt_b64_decode(adata->err_msg, decoded_err, sizeof(decoded_err) - 1);
  if (len >= 0)
  {
    decoded_err[len] = '\0';
    err = decoded_err;
  }
  mutt_error("%s %s", _("Authentication failed"), err);

  return POP_A_FAILURE;
}

/**
 * PopAuthenticators - Accepted authentication methods
 */
static const struct PopAuth PopAuthenticators[] = {
  // clang-format off
  { pop_auth_oauth, "oauthbearer" },
#ifdef USE_SASL_CYRUS
  { pop_auth_sasl, NULL },
#endif
#ifdef USE_SASL_GNU
  { pop_auth_gsasl, NULL },
#endif
  { pop_auth_apop, "apop" },
  { pop_auth_user, "user" },
  { NULL, NULL },
  // clang-format on
};

/**
 * pop_auth_is_valid - Check if string is a valid pop authentication method
 * @param authenticator Authenticator string to check
 * @retval true Argument is a valid auth method
 *
 * Validate whether an input string is an accepted pop authentication method as
 * defined by #PopAuthenticators.
 */
bool pop_auth_is_valid(const char *authenticator)
{
  for (size_t i = 0; i < mutt_array_size(PopAuthenticators); i++)
  {
    const struct PopAuth *auth = &PopAuthenticators[i];
    if (auth->method && mutt_istr_equal(auth->method, authenticator))
      return true;
  }

  return false;
}

/**
 * pop_authenticate - Authenticate with a POP server
 * @param adata POP Account data
 * @retval num Result, e.g. #POP_A_SUCCESS
 * @retval   0 Successful
 * @retval  -1 Connection lost
 * @retval  -2 Login failed
 * @retval  -3 Authentication cancelled
 */
int pop_authenticate(struct PopAccountData *adata)
{
  struct ConnAccount *cac = &adata->conn->account;
  const struct PopAuth *authenticator = NULL;
  int attempts = 0;
  int rc = POP_A_UNAVAIL;

  if ((mutt_account_getuser(cac) < 0) || (cac->user[0] == '\0'))
  {
    return -3;
  }

  const struct Slist *c_pop_authenticators = cs_subset_slist(NeoMutt->sub, "pop_authenticators");
  const bool c_pop_auth_try_all = cs_subset_bool(NeoMutt->sub, "pop_auth_try_all");
  if (c_pop_authenticators && (c_pop_authenticators->count > 0))
  {
    /* Try user-specified list of authentication methods */
    struct ListNode *np = NULL;
    STAILQ_FOREACH(np, &c_pop_authenticators->head, entries)
    {
      mutt_debug(LL_DEBUG2, "Trying method %s\n", np->data);
      authenticator = PopAuthenticators;

      while (authenticator->authenticate)
      {
        if (!authenticator->method || mutt_istr_equal(authenticator->method, np->data))
        {
          rc = authenticator->authenticate(adata, np->data);
          if (rc == POP_A_SOCKET)
          {
            switch (pop_connect(adata))
            {
              case 0:
              {
                rc = authenticator->authenticate(adata, np->data);
                break;
              }
              case -2:
                rc = POP_A_FAILURE;
            }
          }

          if (rc != POP_A_UNAVAIL)
            attempts++;
          if ((rc == POP_A_SUCCESS) || (rc == POP_A_SOCKET) ||
              ((rc == POP_A_FAILURE) && !c_pop_auth_try_all))
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
    authenticator = PopAuthenticators;

    while (authenticator->authenticate)
    {
      rc = authenticator->authenticate(adata, NULL);
      if (rc == POP_A_SOCKET)
      {
        switch (pop_connect(adata))
        {
          case 0:
          {
            rc = authenticator->authenticate(adata, NULL);
            break;
          }
          case -2:
            rc = POP_A_FAILURE;
        }
      }

      if (rc != POP_A_UNAVAIL)
        attempts++;
      if ((rc == POP_A_SUCCESS) || (rc == POP_A_SOCKET) ||
          ((rc == POP_A_FAILURE) && !c_pop_auth_try_all))
      {
        break;
      }

      authenticator++;
    }
  }

  switch (rc)
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
