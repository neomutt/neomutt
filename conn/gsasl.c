/**
 * @file
 * GNU SASL authentication support
 *
 * @authors
 * Copyright (C) 2022 Richard Russon <rich@flatcap.org>
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
 * @page conn_gsasl GNU SASL authentication support
 *
 * GNU SASL authentication support
 */

#include "config.h"
#include <gsasl.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "connaccount.h"
#include "connection.h"
#include "gsasl2.h"
#include "mutt_account.h"

static Gsasl *MuttGsaslCtx = NULL;

/**
 * mutt_gsasl_callback - Callback to retrieve authname or user from ConnAccount
 * @param ctx  GNU SASL context
 * @param sctx GNU SASL session
 * @param prop Property to get, e.g. GSASL_PASSWORD
 * @retval num GNU SASL error code, e.g. GSASL_OK
 */
static int mutt_gsasl_callback(Gsasl *ctx, Gsasl_session *sctx, Gsasl_property prop)
{
  int rc = GSASL_NO_CALLBACK;

  struct Connection *conn = gsasl_session_hook_get(sctx);
  if (!conn)
  {
    mutt_debug(LL_DEBUG1, "missing session hook data!\n");
    return rc;
  }

  switch (prop)
  {
    case GSASL_PASSWORD:
      if (mutt_account_getpass(&conn->account))
        return rc;
      gsasl_property_set(sctx, GSASL_PASSWORD, conn->account.pass);
      rc = GSASL_OK;
      break;

    case GSASL_AUTHID:
      /* whom the provided password belongs to: login */
      if (mutt_account_getlogin(&conn->account))
        return rc;
      gsasl_property_set(sctx, GSASL_AUTHID, conn->account.login);
      rc = GSASL_OK;
      break;

    case GSASL_AUTHZID:
      /* name of the user whose mail/resources you intend to access: user */
      if (mutt_account_getuser(&conn->account))
        return rc;
      gsasl_property_set(sctx, GSASL_AUTHZID, conn->account.user);
      rc = GSASL_OK;
      break;

    case GSASL_ANONYMOUS_TOKEN:
      gsasl_property_set(sctx, GSASL_ANONYMOUS_TOKEN, "dummy");
      rc = GSASL_OK;
      break;

    case GSASL_SERVICE:
    {
      const char *service = NULL;
      switch (conn->account.type)
      {
        case MUTT_ACCT_TYPE_IMAP:
          service = "imap";
          break;
        case MUTT_ACCT_TYPE_POP:
          service = "pop";
          break;
        case MUTT_ACCT_TYPE_SMTP:
          service = "smtp";
          break;
        default:
          return rc;
      }
      gsasl_property_set(sctx, GSASL_SERVICE, service);
      rc = GSASL_OK;
      break;
    }

    case GSASL_HOSTNAME:
      gsasl_property_set(sctx, GSASL_HOSTNAME, conn->account.host);
      rc = GSASL_OK;
      break;

    default:
      break;
  }

  return rc;
}

/**
 * mutt_gsasl_init - Initialise GNU SASL library
 * @retval true Success
 */
static bool mutt_gsasl_init(void)
{
  if (MuttGsaslCtx)
    return true;

  int rc = gsasl_init(&MuttGsaslCtx);
  if (rc != GSASL_OK)
  {
    MuttGsaslCtx = NULL;
    mutt_debug(LL_DEBUG1, "libgsasl initialisation failed (%d): %s.\n", rc,
               gsasl_strerror(rc));
    return false;
  }

  gsasl_callback_set(MuttGsaslCtx, mutt_gsasl_callback);
  return true;
}

/**
 * mutt_gsasl_done - Shutdown GNU SASL library
 */
void mutt_gsasl_done(void)
{
  if (!MuttGsaslCtx)
    return;

  gsasl_done(MuttGsaslCtx);
  MuttGsaslCtx = NULL;
}

/**
 * mutt_gsasl_get_mech - Pick a connection mechanism
 * @param requested_mech  Requested mechanism
 * @param server_mechlist Server's list of mechanisms
 * @retval ptr Selected mechanism string
 */
const char *mutt_gsasl_get_mech(const char *requested_mech, const char *server_mechlist)
{
  if (!mutt_gsasl_init())
    return NULL;

  /* libgsasl does not do case-independent string comparisons,
   * and stores its methods internally in uppercase. */
  char *uc_server_mechlist = mutt_str_dup(server_mechlist);
  if (uc_server_mechlist)
    mutt_str_upper(uc_server_mechlist);

  char *uc_requested_mech = mutt_str_dup(requested_mech);
  if (uc_requested_mech)
    mutt_str_upper(uc_requested_mech);

  const char *sel_mech = NULL;
  if (uc_requested_mech)
    sel_mech = gsasl_client_suggest_mechanism(MuttGsaslCtx, uc_requested_mech);
  else
    sel_mech = gsasl_client_suggest_mechanism(MuttGsaslCtx, uc_server_mechlist);

  FREE(&uc_requested_mech);
  FREE(&uc_server_mechlist);

  return sel_mech;
}

/**
 * mutt_gsasl_client_new - Create a new GNU SASL client
 * @param conn Connection to a server
 * @param mech Mechanisms to use
 * @param sctx GNU SASL Session
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_gsasl_client_new(struct Connection *conn, const char *mech, Gsasl_session **sctx)
{
  if (!mutt_gsasl_init())
    return -1;

  int rc = gsasl_client_start(MuttGsaslCtx, mech, sctx);
  if (rc != GSASL_OK)
  {
    *sctx = NULL;
    mutt_debug(LL_DEBUG1, "gsasl_client_start failed (%d): %s.\n", rc, gsasl_strerror(rc));
    return -1;
  }

  gsasl_session_hook_set(*sctx, conn);
  return 0;
}

/**
 * mutt_gsasl_client_finish - Free a GNU SASL client
 * @param sctx GNU SASL Session
 */
void mutt_gsasl_client_finish(Gsasl_session **sctx)
{
  gsasl_finish(*sctx);
  *sctx = NULL;
}
