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

/* common SASL helper routines */

#include "mutt.h"
#include "account.h"
#include "mutt_sasl.h"
#include "mutt_socket.h"

#include <sasl.h>

static sasl_callback_t mutt_sasl_callbacks[3];

/* callbacks */
static int mutt_sasl_cb_log (void* context, int priority, const char* message);
static int mutt_sasl_cb_authname (void* context, int id, const char** result,
  unsigned int* len);
static int mutt_sasl_cb_pass (sasl_conn_t* conn, void* context, int id,
  sasl_secret_t** psecret);

/* mutt_sasl_start: called before doing a SASL exchange - initialises library
 *   (if neccessary). */
int mutt_sasl_start (void)
{
  static unsigned char sasl_init = 0;

  sasl_callback_t* callback, callbacks[2];
  int rc;

  if (!sasl_init) {
    /* set up default logging callback */
    callback = callbacks;

    callback->id = SASL_CB_LOG;
    callback->proc = mutt_sasl_cb_log;
    callback->context = NULL;
    callback++;

    callback->id = SASL_CB_LIST_END;
    callback->proc = NULL;
    callback->context = NULL;

    rc = sasl_client_init (callbacks);

    if (rc != SASL_OK)
    {
      dprint (1, (debugfile, "mutt_sasl_start: libsasl initialisation failed.\n"));
      return SASL_FAIL;
    }

    sasl_init = 1;
  }

  return SASL_OK;
}

sasl_callback_t* mutt_sasl_get_callbacks (ACCOUNT* account)
{
  sasl_callback_t* callback;

  callback = mutt_sasl_callbacks;

  callback->id = SASL_CB_AUTHNAME;
  callback->proc = mutt_sasl_cb_authname;
  callback->context = account;
  callback++;

  callback->id = SASL_CB_PASS;
  callback->proc = mutt_sasl_cb_pass;
  callback->context = account;
  callback++;

  callback->id = SASL_CB_LIST_END;
  callback->proc = NULL;
  callback->context = NULL;

  return mutt_sasl_callbacks;
}

/* mutt_sasl_cb_log: callback to log SASL messages */
static int mutt_sasl_cb_log (void* context, int priority, const char* message)
{
  dprint (priority, (debugfile, "SASL: %s\n", message));

  return SASL_OK;
}

/* mutt_sasl_cb_authname: callback to retrieve authname from ACCOUNT */
static int mutt_sasl_cb_authname (void* context, int id, const char** result,
  unsigned* len)
{
  ACCOUNT* account = (ACCOUNT*) context;

  *result = NULL;
  if (len)
    *len = 0;

  if (!account)
    return SASL_BADPARAM;

  dprint (2, (debugfile, "mutt_sasl_cb_authname: getting user for %s:%u\n",
	      account->host, account->port));

  if (mutt_account_getuser (account))
    return SASL_FAIL;

  *result = account->user;

  if (len)
    *len = strlen (*result);

  return SASL_OK;
}

static int mutt_sasl_cb_pass (sasl_conn_t* conn, void* context, int id,
  sasl_secret_t** psecret)
{
  ACCOUNT* account = (ACCOUNT*) context;
  int len;

  if (!account || !psecret)
    return SASL_BADPARAM;

  dprint (2, (debugfile,
    "mutt_sasl_cb_pass: getting password for %s@%s:%u\n", account->user,
    account->host, account->port));

  if (mutt_account_getpass (account))
    return SASL_FAIL;

  len = strlen (account->pass);

  *psecret = (sasl_secret_t*) malloc (sizeof (sasl_secret_t) + len);
  (*psecret)->len = len;
  strcpy ((*psecret)->data, account->pass);

  return SASL_OK;
}
