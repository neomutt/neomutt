/*
 * Copyright (C) 2000-1 Brendan Cully <brendan@kublai.com>
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
#include <sys/socket.h>
#include <netinet/in.h>

/* arbitrary. SASL will probably use a smaller buffer anyway. OTOH it's
 * been a while since I've had access to an SASL server which negotiated
 * a protection buffer. */ 
#define M_SASL_MAXBUF 65536

static sasl_callback_t mutt_sasl_callbacks[5];

static int mutt_sasl_start (void);

/* callbacks */
static int mutt_sasl_cb_log (void* context, int priority, const char* message);
static int mutt_sasl_cb_authname (void* context, int id, const char** result,
  unsigned int* len);
static int mutt_sasl_cb_pass (sasl_conn_t* conn, void* context, int id,
  sasl_secret_t** psecret);

/* socket wrappers for a SASL security layer */
static int mutt_sasl_conn_open (CONNECTION* conn);
static int mutt_sasl_conn_close (CONNECTION* conn);
static int mutt_sasl_conn_read (CONNECTION* conn, char* buf, size_t len);
static int mutt_sasl_conn_write (CONNECTION* conn, const char* buf,
  size_t count);

/* mutt_sasl_start: called before doing a SASL exchange - initialises library
 *   (if neccessary). */
int mutt_sasl_start (void)
{
  static unsigned char sasl_init = 0;

  sasl_callback_t* callback, callbacks[2];
  int rc;

  if (sasl_init)
    return SASL_OK;

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

  return SASL_OK;
}

/* mutt_sasl_client_new: wrapper for sasl_client_new which also sets various
 * security properties. If this turns out to be fine for POP too we can
 * probably stop exporting mutt_sasl_get_callbacks(). */
int mutt_sasl_client_new (CONNECTION* conn, sasl_conn_t** saslconn)
{
  sasl_security_properties_t secprops;
  sasl_external_properties_t extprops;
  const char* service;
  int rc;

  if (mutt_sasl_start () != SASL_OK)
    return -1;

  switch (conn->account.type)
  {
    case M_ACCT_TYPE_IMAP:
      service = "imap";
      break;
    case M_ACCT_TYPE_POP:
      service = "pop-3";
      break;
    default:
      dprint (1, (debugfile, "mutt_sasl_client_new: account type unset\n"));
      return -1;
  }
  
  rc = sasl_client_new (service, conn->account.host,
    mutt_sasl_get_callbacks (&conn->account), SASL_SECURITY_LAYER, saslconn);
  
  if (rc != SASL_OK)
  {
    dprint (1, (debugfile,
      "mutt_sasl_client_new: Error allocating SASL connection\n"));
    return -1;
  }

  /*** set sasl IP properties, necessary for use with krb4 ***/
  /* Do we need to fail if this fails? I would assume having these unset
   * would just disable KRB4. Who wrote this code? I'm not sure how this
   * interacts with the NSS code either, since that mucks with the fd. */
  {
    struct sockaddr_in local, remote;
    socklen_t size;

    size = sizeof (local);
    if (getsockname (conn->fd, (struct sockaddr*) &local, &size))
      return -1;

    size = sizeof(remote);
    if (getpeername(conn->fd, (struct sockaddr*) &remote, &size))
      return -1;

#ifdef SASL_IP_LOCAL
    if (sasl_setprop(*saslconn, SASL_IP_LOCAL, &local) != SASL_OK)
    {
      dprint (1, (debugfile,
	"mutt_sasl_client_new: Error setting local IP address\n"));
      return -1;
    }
#endif

#ifdef SASL_IP_REMOTE
    if (sasl_setprop(*saslconn, SASL_IP_REMOTE, &remote) != SASL_OK)
    {
      dprint (1, (debugfile,
	"mutt_sasl_client_new: Error setting remote IP address\n"));
      return -1;
    }
#endif
  }

  /* set security properties. We use NOPLAINTEXT globally, since we can
   * just fall back to LOGIN in the IMAP case anyway. If that doesn't
   * work for POP, we can make it a flag or move this code into
   * imap/auth_sasl.c */
  memset (&secprops, 0, sizeof (secprops));
  /* Work around a casting bug in the SASL krb4 module */
  secprops.max_ssf = 0x7fff;
  secprops.maxbufsize = M_SASL_MAXBUF;
  secprops.security_flags |= SASL_SEC_NOPLAINTEXT;
  if (sasl_setprop (*saslconn, SASL_SEC_PROPS, &secprops) != SASL_OK)
  {
    dprint (1, (debugfile,
      "mutt_sasl_client_new: Error setting security properties\n"));
    return -1;
  }

  /* we currently don't have an SSF finder for NSS (I don't know the API).
   * If someone does it'd probably be trivial to write mutt_nss_get_ssf().
   * I have a feeling more SSL code could be shared between those two files,
   * but I haven't looked into it yet, since I still don't know the APIs. */
#if defined(USE_SSL) && !defined(USE_NSS)
  if (conn->account.flags & M_ACCT_SSL)
  {
    memset (&extprops, 0, sizeof (extprops));
    extprops.ssf = conn->ssf;
    dprint (2, (debugfile, "External SSF: %d\n", extprops.ssf));
    if (sasl_setprop (*saslconn, SASL_SSF_EXTERNAL, &extprops) != SASL_OK)
    {
      dprint (1, (debugfile, "mutt_sasl_client_new: Error setting external properties\n"));
      return -1;
    }
  }
#endif

  return 0;
}

sasl_callback_t* mutt_sasl_get_callbacks (ACCOUNT* account)
{
  sasl_callback_t* callback;

  callback = mutt_sasl_callbacks;

  callback->id = SASL_CB_AUTHNAME;
  callback->proc = mutt_sasl_cb_authname;
  callback->context = account;
  callback++;

  callback->id = SASL_CB_USER;
  callback->proc = mutt_sasl_cb_authname;
  callback->context = account;
  callback++;

  callback->id = SASL_CB_PASS;
  callback->proc = mutt_sasl_cb_pass;
  callback->context = account;
  callback++;

  callback->id = SASL_CB_GETREALM;
  callback->proc = NULL;
  callback->context = NULL;
  callback++;

  callback->id = SASL_CB_LIST_END;
  callback->proc = NULL;
  callback->context = NULL;

  return mutt_sasl_callbacks;
}

int mutt_sasl_interact (sasl_interact_t* interaction)
{
  char prompt[SHORT_STRING];
  char resp[SHORT_STRING];

  while (interaction->id != SASL_CB_LIST_END)
  {
    dprint (2, (debugfile, "mutt_sasl_interact: filling in SASL interaction %ld.\n", interaction->id));

    snprintf (prompt, sizeof (prompt), "%s: ", interaction->prompt);
    resp[0] = '\0';
    if (mutt_get_field (prompt, resp, sizeof (resp), 0))
      return SASL_FAIL;

    interaction->len = mutt_strlen (resp)+1;
    interaction->result = safe_malloc (interaction->len);
    memcpy (interaction->result, resp, interaction->len);

    interaction++;
  }

  return SASL_OK;
}

/* SASL can stack a protection layer on top of an existing connection.
 * To handle this, we store a saslconn_t in conn->sockdata, and write
 * wrappers which en/decode the read/write stream, then replace sockdata
 * with an embedded copy of the old sockdata and call the underlying
 * functions (which we've also preserved). I thought about trying to make
 * a general stackable connection system, but it seemed like overkill -
 * something is wrong if we have 15 filters on top of a socket. Anyway,
 * anything else which wishes to stack can use the same method. The only
 * disadvantage is we have to write wrappers for all the socket methods,
 * even if we only stack over read and write. Thinking about it, the
 * abstraction problem is that there is more in CONNECTION than there
 * needs to be. Ideally it would have only (void*)data and methods. */

/* mutt_sasl_setup_conn: replace connection methods, sockdata with 
 *   SASL wrappers, for protection layers. Also get ssf, as a fastpath
 *   for the read/write methods. */
void mutt_sasl_setup_conn (CONNECTION* conn, sasl_conn_t* saslconn)
{
  SASL_DATA* sasldata = (SASL_DATA*) safe_malloc (sizeof (SASL_DATA));

  sasldata->saslconn = saslconn;
  /* get ssf so we know whether we have to (en|de)code read/write */
  sasl_getprop (saslconn, SASL_SSF, (void**) &sasldata->ssf);
  dprint (3, (debugfile, "SASL protection strength: %u\n", *sasldata->ssf));
  /* Add SASL SSF to transport SSF */
  conn->ssf += *sasldata->ssf;
  sasl_getprop (saslconn, SASL_MAXOUTBUF, (void**) &sasldata->pbufsize);
  dprint (3, (debugfile, "SASL protection buffer size: %u\n", *sasldata->pbufsize));

  /* clear input buffer */
  sasldata->buf = NULL;
  sasldata->bpos = 0;
  sasldata->blen = 0;

  /* preserve old functions */
  sasldata->sockdata = conn->sockdata;
  sasldata->open = conn->open;
  sasldata->close = conn->close;
  sasldata->read = conn->read;
  sasldata->write = conn->write;

  /* and set up new functions */
  conn->sockdata = sasldata;
  conn->open = mutt_sasl_conn_open;
  conn->close = mutt_sasl_conn_close;
  conn->read = mutt_sasl_conn_read;
  conn->write = mutt_sasl_conn_write;
}

/* mutt_sasl_cb_log: callback to log SASL messages */
static int mutt_sasl_cb_log (void* context, int priority, const char* message)
{
  dprint (priority, (debugfile, "SASL: %s\n", message));

  return SASL_OK;
}

/* mutt_sasl_cb_authname: callback to retrieve authname or user (mutt
 *   doesn't distinguish, even if some SASL plugins do) from ACCOUNT */
static int mutt_sasl_cb_authname (void* context, int id, const char** result,
  unsigned* len)
{
  ACCOUNT* account = (ACCOUNT*) context;

  *result = NULL;
  if (len)
    *len = 0;

  if (!account)
    return SASL_BADPARAM;

  dprint (2, (debugfile, "mutt_sasl_cb_authname: getting %s for %s:%u\n",
	      id == SASL_CB_AUTHNAME ? "authname" : "user",
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

  *psecret = (sasl_secret_t*) safe_malloc (sizeof (sasl_secret_t) + len);
  (*psecret)->len = len;
  strcpy ((*psecret)->data, account->pass);	/* __STRCPY_CHECKED__ */

  return SASL_OK;
}

/* mutt_sasl_conn_open: empty wrapper for underlying open function. We
 *   don't know in advance that a connection will use SASL, so we
 *   replace conn's methods with sasl methods when authentication
 *   is successful, using mutt_sasl_setup_conn */
static int mutt_sasl_conn_open (CONNECTION* conn)
{
  SASL_DATA* sasldata;
  int rc;

  sasldata = (SASL_DATA*) conn->sockdata;
  conn->sockdata = sasldata->sockdata;
  rc = (sasldata->open) (conn);
  conn->sockdata = sasldata;

  return rc;
}

/* mutt_sasl_conn_close: calls underlying close function and disposes of
 *   the sasl_conn_t object, then restores connection to pre-sasl state */
static int mutt_sasl_conn_close (CONNECTION* conn)
{
  SASL_DATA* sasldata;
  int rc;

  sasldata = (SASL_DATA*) conn->sockdata;

  /* restore connection's underlying methods */
  conn->sockdata = sasldata->sockdata;
  conn->open = sasldata->open;
  conn->close = sasldata->close;
  conn->read = sasldata->read;
  conn->write = sasldata->write;

  /* release sasl resources */
  sasl_dispose (&sasldata->saslconn);
  FREE (&sasldata->buf);
  FREE (&sasldata);

  /* call underlying close */
  rc = (conn->close) (conn);

  return rc;
}

static int mutt_sasl_conn_read (CONNECTION* conn, char* buf, size_t len)
{
  SASL_DATA* sasldata;
  int rc;

  unsigned int olen;

  sasldata = (SASL_DATA*) conn->sockdata;

  /* if we still have data in our read buffer, copy it into buf */
  if (sasldata->blen > sasldata->bpos)
  {
    olen = (sasldata->blen - sasldata->bpos > len) ? len :
      sasldata->blen - sasldata->bpos;

    memcpy (buf, sasldata->buf+sasldata->bpos, olen);
    sasldata->bpos += olen;

    return olen;
  }
  
  conn->sockdata = sasldata->sockdata;

  FREE (&sasldata->buf);
  sasldata->bpos = 0;
  sasldata->blen = 0;

  /* and decode the result, if necessary */
  if (*sasldata->ssf)
  {
    do
    {
      /* call the underlying read function to fill the buffer */
      rc = (sasldata->read) (conn, buf, len);
      if (rc <= 0)
	goto out;

      rc = sasl_decode (sasldata->saslconn, buf, rc, &sasldata->buf,
        &sasldata->blen);
      if (rc != SASL_OK)
      {
	dprint (1, (debugfile, "SASL decode failed: %s\n",
          sasl_errstring (rc, NULL, NULL)));
	goto out;
      }
    }
    while (!sasldata->blen);

    olen = (sasldata->blen - sasldata->bpos > len) ? len :
      sasldata->blen - sasldata->bpos;

    memcpy (buf, sasldata->buf, olen);
    sasldata->bpos += olen;

    rc = olen;
  }
  else
    rc = (sasldata->read) (conn, buf, len);

  out:
    conn->sockdata = sasldata;

    return rc;
}

static int mutt_sasl_conn_write (CONNECTION* conn, const char* buf,
  size_t len)
{
  SASL_DATA* sasldata;
  int rc;

  char* pbuf;
  unsigned int olen, plen;

  sasldata = (SASL_DATA*) conn->sockdata;
  conn->sockdata = sasldata->sockdata;

  /* encode data, if necessary */
  if (*sasldata->ssf)
  {
    /* handle data larger than MAXOUTBUF */
    do
    {
      olen = (len > *sasldata->pbufsize) ? *sasldata->pbufsize : len;

      rc = sasl_encode (sasldata->saslconn, buf, olen, &pbuf, &plen);
      if (rc != SASL_OK)
      {
	dprint (1, (debugfile, "SASL encoding failed: %s\n",
          sasl_errstring (rc, NULL, NULL)));
	goto fail;
      }

      rc = (sasldata->write) (conn, pbuf, plen);
      FREE (&pbuf);
      if (rc != plen)
	goto fail;

      len -= olen;
      buf += olen;
    }
    while (len > *sasldata->pbufsize);
  }
  else
  /* just write using the underlying socket function */
    rc = (sasldata->write) (conn, buf, len);
  
  conn->sockdata = sasldata;

  return rc;

 fail:
  conn->sockdata = sasldata;
  return -1;
}
