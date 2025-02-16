/**
 * @file
 * SASL authentication support
 *
 * @authors
 * Copyright (C) 2017-2024 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019 Ian Zimmerman <itz@no-use.mooo.com>
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
 * @page conn_sasl SASL authentication
 *
 * SASL can stack a protection layer on top of an existing connection.  To
 * handle this, we store a saslconn_t in conn->sockdata, and write wrappers
 * which en/decode the read/write stream, then replace sockdata with an
 * embedded copy of the old sockdata and call the underlying functions (which
 * we've also preserved). I thought about trying to make a general stackable
 * connection system, but it seemed like overkill. Something is wrong if we
 * have 15 filters on top of a socket. Anyway, anything else which wishes to
 * stack can use the same method. The only disadvantage is we have to write
 * wrappers for all the socket methods, even if we only stack over read and
 * write. Thinking about it, the abstraction problem is that there is more in
 * Connection than there needs to be. Ideally it would have only (void*)data
 * and methods.
 */

#include "config.h"
#include <errno.h>
#include <netdb.h>
#include <sasl/sasl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "mutt/lib.h"
#include "mutt.h"
#include "sasl.h"
#include "editor/lib.h"
#include "history/lib.h"
#include "connaccount.h"
#include "connection.h"
#include "globals.h"

/**
 * struct SaslSockData - SASL authentication API - @extends Connection
 */
struct SaslSockData
{
  sasl_conn_t *saslconn;
  const sasl_ssf_t *ssf;
  const unsigned int *pbufsize;

  /* read buffer */
  const char *buf;   ///< Buffer for data read from the connection
  unsigned int blen; ///< Size of the read buffer
  unsigned int bpos; ///< Current read position

  void *sockdata; ///< Underlying socket data

  /**
   * open - Open a socket Connection - Implements Connection::open() - @ingroup connection_open
   */
  int (*open)(struct Connection *conn);

  /**
   * read - Read from a socket Connection - Implements Connection::read() - @ingroup connection_read
   */
  int (*read)(struct Connection *conn, char *buf, size_t count);

  /**
   * write - Write to a socket Connection - Implements Connection::write() - @ingroup connection_write
   */
  int (*write)(struct Connection *conn, const char *buf, size_t count);

  /**
   * poll - Check if any data is waiting on a socket - Implements Connection::poll() - @ingroup connection_poll
   */
  int (*poll)(struct Connection *conn, time_t wait_secs);

  /**
   * close - Close a socket Connection - Implements Connection::close() - @ingroup connection_close
   */
  int (*close)(struct Connection *conn);
};

/**
 * SaslAuthenticators - Authentication methods supported by Cyrus SASL
 */
static const char *const SaslAuthenticators[] = {
  "ANONYMOUS",     "CRAM-MD5",       "DIGEST-MD5",    "EXTERNAL",
  "GS2-IAKERB",    "GS2-KRB5",       "GSS-SPNEGO",    "GSSAPI",
  "LOGIN",         "NTLM",           "OTP-MD4",       "OTP-MD5",
  "OTP-SHA1",      "PASSDSS-3DES-1", "PLAIN",         "SCRAM-SHA-1",
  "SCRAM-SHA-224", "SCRAM-SHA-256",  "SCRAM-SHA-384", "SCRAM-SHA-512",
  "SRP",
};

/* arbitrary. SASL will probably use a smaller buffer anyway. OTOH it's
 * been a while since I've had access to an SASL server which negotiated
 * a protection buffer. */
#define MUTT_SASL_MAXBUF 65536

/* used to hold a string "host;port"
 * where host is size NI_MAXHOST-1 and port is size NI_MAXSERV-1
 * plus two bytes for the ';' and trailing \0 */
#define IP_PORT_BUFLEN (NI_MAXHOST + NI_MAXSERV)

/// SASL callback functions, e.g. mutt_sasl_cb_authname(), mutt_sasl_cb_pass()
static sasl_callback_t MuttSaslCallbacks[5];

/// SASL secret, to store the password
static sasl_secret_t *SecretPtr = NULL;

/**
 * sasl_auth_validator - Validate an auth method against Cyrus SASL methods
 * @param authenticator Name of the authenticator to validate
 * @retval true Argument matches an accepted auth method
 */
bool sasl_auth_validator(const char *authenticator)
{
  for (size_t i = 0; i < mutt_array_size(SaslAuthenticators); i++)
  {
    const char *auth = SaslAuthenticators[i];
    if (mutt_istr_equal(auth, authenticator))
      return true;
  }

  return false;
}

/**
 * getnameinfo_err - Convert a getaddrinfo() error code into an SASL error code
 * @param rc getaddrinfo() error code, e.g. EAI_AGAIN
 * @retval num SASL error code, e.g. SASL_FAIL
 */
static int getnameinfo_err(int rc)
{
  int err;
  mutt_debug(LL_DEBUG1, "getnameinfo: ");
  switch (rc)
  {
    case EAI_AGAIN:
      mutt_debug(LL_DEBUG1, "The name could not be resolved at this time.  Future attempts may succeed\n");
      err = SASL_TRYAGAIN;
      break;
    case EAI_BADFLAGS:
      mutt_debug(LL_DEBUG1, "The flags had an invalid value\n");
      err = SASL_BADPARAM;
      break;
    case EAI_FAIL:
      mutt_debug(LL_DEBUG1, "A non-recoverable error occurred\n");
      err = SASL_FAIL;
      break;
    case EAI_FAMILY:
      mutt_debug(LL_DEBUG1, "The address family was not recognized or the address length was invalid for the specified family\n");
      err = SASL_BADPROT;
      break;
    case EAI_MEMORY:
      mutt_debug(LL_DEBUG1, "There was a memory allocation failure\n");
      err = SASL_NOMEM;
      break;
    case EAI_NONAME:
      mutt_debug(LL_DEBUG1, "The name does not resolve for the supplied parameters.  "
                            "NI_NAMEREQD is set and the host's name can't be located, or both nodename and servname were null.\n");
      err = SASL_FAIL; /* no real equivalent */
      break;
    case EAI_SYSTEM:
      mutt_debug(LL_DEBUG1, "A system error occurred.  The error code can be found in errno(%d,%s))\n",
                 errno, strerror(errno));
      err = SASL_FAIL; /* no real equivalent */
      break;
    default:
      mutt_debug(LL_DEBUG1, "Unknown error %d\n", rc);
      err = SASL_FAIL; /* no real equivalent */
      break;
  }
  return err;
}

/**
 * iptostring - Convert IP Address to string
 * @param addr    IP address
 * @param addrlen Size of addr struct
 * @param out     Buffer for result
 * @param outlen  Length of buffer
 * @retval num SASL error code, e.g. SASL_BADPARAM
 *
 * utility function, copied from sasl2 sample code
 */
static int iptostring(const struct sockaddr *addr, socklen_t addrlen, char *out,
                      unsigned int outlen)
{
  char hbuf[NI_MAXHOST], pbuf[NI_MAXSERV];
  int rc;

  if (!addr || !out)
    return SASL_BADPARAM;

  rc = getnameinfo(addr, addrlen, hbuf, sizeof(hbuf), pbuf, sizeof(pbuf),
                   NI_NUMERICHOST |
#ifdef NI_WITHSCOPEID
                       NI_WITHSCOPEID |
#endif
                       NI_NUMERICSERV);
  if (rc != 0)
    return getnameinfo_err(rc);

  if (outlen < strlen(hbuf) + strlen(pbuf) + 2)
    return SASL_BUFOVER;

  snprintf(out, outlen, "%s;%s", hbuf, pbuf);

  return SASL_OK;
}

/**
 * mutt_sasl_cb_log - Callback to log SASL messages
 * @param context  Supplied context, always NULL
 * @param priority Debug level
 * @param message  Message
 * @retval num SASL_OK, always
 */
static int mutt_sasl_cb_log(void *context, int priority, const char *message)
{
  if (priority == SASL_LOG_NONE)
    return SASL_OK;

  int mutt_priority = 0;
  switch (priority)
  {
    case SASL_LOG_TRACE:
    case SASL_LOG_PASS:
      mutt_priority = 5;
      break;
    case SASL_LOG_DEBUG:
    case SASL_LOG_NOTE:
      mutt_priority = 3;
      break;
    case SASL_LOG_FAIL:
    case SASL_LOG_WARN:
      mutt_priority = 2;
      break;
    case SASL_LOG_ERR:
      mutt_priority = 1;
      break;
    default:
      mutt_debug(LL_DEBUG1, "SASL unknown log priority: %s\n", message);
      return SASL_OK;
  }
  mutt_debug(mutt_priority, "SASL: %s\n", message);
  return SASL_OK;
}

/**
 * mutt_sasl_start - Initialise SASL library
 * @retval num SASL error code, e.g. SASL_OK
 *
 * Call before doing an SASL exchange (initialises library if necessary).
 */
int mutt_sasl_start(void)
{
  static bool sasl_init = false;

  static sasl_callback_t callbacks[2];
  int rc;

  if (sasl_init)
    return SASL_OK;

  /* set up default logging callback */
  callbacks[0].id = SASL_CB_LOG;
  callbacks[0].proc = (int (*)(void))(intptr_t) mutt_sasl_cb_log;
  callbacks[0].context = NULL;

  callbacks[1].id = SASL_CB_LIST_END;
  callbacks[1].proc = NULL;
  callbacks[1].context = NULL;

  rc = sasl_client_init(callbacks);

  if (rc != SASL_OK)
  {
    mutt_debug(LL_DEBUG1, "libsasl initialisation failed\n");
    return SASL_FAIL;
  }

  sasl_init = true;

  return SASL_OK;
}

/**
 * mutt_sasl_cb_authname - Callback to retrieve authname or user from ConnAccount
 * @param[in]  context ConnAccount
 * @param[in]  id      Field to get.  SASL_CB_USER or SASL_CB_AUTHNAME
 * @param[out] result  Resulting string
 * @param[out] len     Length of result
 * @retval num SASL error code, e.g. SASL_FAIL
 */
static int mutt_sasl_cb_authname(void *context, int id, const char **result, unsigned int *len)
{
  if (!result)
    return SASL_FAIL;

  struct ConnAccount *cac = context;

  *result = NULL;
  if (len)
    *len = 0;

  if (!cac)
    return SASL_BADPARAM;

  mutt_debug(LL_DEBUG2, "getting %s for %s:%u\n",
             (id == SASL_CB_AUTHNAME) ? "authname" : "user", cac->host, cac->port);

  if (id == SASL_CB_AUTHNAME)
  {
    if (mutt_account_getlogin(cac) < 0)
      return SASL_FAIL;
    *result = cac->login;
  }
  else
  {
    if (mutt_account_getuser(cac) < 0)
      return SASL_FAIL;
    *result = cac->user;
  }

  if (len)
    *len = strlen(*result);

  return SASL_OK;
}

/**
 * mutt_sasl_cb_pass - SASL callback function to get password
 * @param[in]  conn    Connection to a server
 * @param[in]  context ConnAccount
 * @param[in]  id      SASL_CB_PASS
 * @param[out] psecret SASL secret
 * @retval num SASL error code, e.g SASL_FAIL
 */
static int mutt_sasl_cb_pass(sasl_conn_t *conn, void *context, int id, sasl_secret_t **psecret)
{
  struct ConnAccount *cac = context;
  int len;

  if (!cac || !psecret)
    return SASL_BADPARAM;

  mutt_debug(LL_DEBUG2, "getting password for %s@%s:%u\n", cac->login, cac->host, cac->port);

  if (mutt_account_getpass(cac) < 0)
    return SASL_FAIL;

  len = strlen(cac->pass);

  mutt_mem_realloc(&SecretPtr, sizeof(sasl_secret_t) + len);
  memcpy((char *) SecretPtr->data, cac->pass, (size_t) len);
  SecretPtr->len = len;
  *psecret = SecretPtr;

  return SASL_OK;
}

/**
 * mutt_sasl_get_callbacks - Get the SASL callback functions
 * @param cac ConnAccount to associate with callbacks
 * @retval ptr Array of callback functions
 */
static sasl_callback_t *mutt_sasl_get_callbacks(struct ConnAccount *cac)
{
  sasl_callback_t *callback = MuttSaslCallbacks;

  callback->id = SASL_CB_USER;
  callback->proc = (int (*)(void))(intptr_t) mutt_sasl_cb_authname;
  callback->context = cac;
  callback++;

  callback->id = SASL_CB_AUTHNAME;
  callback->proc = (int (*)(void))(intptr_t) mutt_sasl_cb_authname;
  callback->context = cac;
  callback++;

  callback->id = SASL_CB_PASS;
  callback->proc = (int (*)(void))(intptr_t) mutt_sasl_cb_pass;
  callback->context = cac;
  callback++;

  callback->id = SASL_CB_GETREALM;
  callback->proc = NULL;
  callback->context = NULL;
  callback++;

  callback->id = SASL_CB_LIST_END;
  callback->proc = NULL;
  callback->context = NULL;

  return MuttSaslCallbacks;
}

/**
 * mutt_sasl_conn_open - Empty wrapper for underlying open function - Implements Connection::open() - @ingroup connection_open
 *
 * We don't know in advance that a connection will use SASL, so we replace
 * conn's methods with sasl methods when authentication is successful, using
 * mutt_sasl_setup_conn
 */
static int mutt_sasl_conn_open(struct Connection *conn)
{
  struct SaslSockData *sasldata = conn->sockdata;
  conn->sockdata = sasldata->sockdata;
  int rc = sasldata->open(conn);
  conn->sockdata = sasldata;

  return rc;
}

/**
 * mutt_sasl_conn_close - Close SASL connection - Implements Connection::close() - @ingroup connection_close
 *
 * Calls underlying close function and disposes of the sasl_conn_t object, then
 * restores connection to pre-sasl state
 */
static int mutt_sasl_conn_close(struct Connection *conn)
{
  struct SaslSockData *sasldata = conn->sockdata;

  /* restore connection's underlying methods */
  conn->sockdata = sasldata->sockdata;
  conn->open = sasldata->open;
  conn->read = sasldata->read;
  conn->write = sasldata->write;
  conn->poll = sasldata->poll;
  conn->close = sasldata->close;

  /* release sasl resources */
  sasl_dispose(&sasldata->saslconn);
  FREE(&sasldata);

  /* call underlying close */
  int rc = conn->close(conn);

  return rc;
}

/**
 * mutt_sasl_conn_read - Read data from an SASL connection - Implements Connection::read() - @ingroup connection_read
 */
static int mutt_sasl_conn_read(struct Connection *conn, char *buf, size_t count)
{
  int rc;
  unsigned int olen;

  struct SaslSockData *sasldata = conn->sockdata;

  /* if we still have data in our read buffer, copy it into buf */
  if (sasldata->blen > sasldata->bpos)
  {
    olen = ((sasldata->blen - sasldata->bpos) > count) ?
               count :
               sasldata->blen - sasldata->bpos;

    memcpy(buf, sasldata->buf + sasldata->bpos, olen);
    sasldata->bpos += olen;

    return olen;
  }

  conn->sockdata = sasldata->sockdata;

  sasldata->bpos = 0;
  sasldata->blen = 0;

  /* and decode the result, if necessary */
  if (*sasldata->ssf != 0)
  {
    do
    {
      /* call the underlying read function to fill the buffer */
      rc = sasldata->read(conn, buf, count);
      if (rc <= 0)
        goto out;

      rc = sasl_decode(sasldata->saslconn, buf, rc, &sasldata->buf, &sasldata->blen);
      if (rc != SASL_OK)
      {
        mutt_debug(LL_DEBUG1, "SASL decode failed: %s\n", sasl_errstring(rc, NULL, NULL));
        goto out;
      }
    } while (sasldata->blen == 0);

    olen = ((sasldata->blen - sasldata->bpos) > count) ?
               count :
               sasldata->blen - sasldata->bpos;

    memcpy(buf, sasldata->buf, olen);
    sasldata->bpos += olen;

    rc = olen;
  }
  else
  {
    rc = sasldata->read(conn, buf, count);
  }

out:
  conn->sockdata = sasldata;

  return rc;
}

/**
 * mutt_sasl_conn_write - Write to an SASL connection - Implements Connection::write() - @ingroup connection_write
 */
static int mutt_sasl_conn_write(struct Connection *conn, const char *buf, size_t count)
{
  int rc;
  const char *pbuf = NULL;
  unsigned int olen, plen;

  struct SaslSockData *sasldata = conn->sockdata;
  conn->sockdata = sasldata->sockdata;

  /* encode data, if necessary */
  if (*sasldata->ssf != 0)
  {
    /* handle data larger than MAXOUTBUF */
    do
    {
      olen = (count > *sasldata->pbufsize) ? *sasldata->pbufsize : count;

      rc = sasl_encode(sasldata->saslconn, buf, olen, &pbuf, &plen);
      if (rc != SASL_OK)
      {
        mutt_debug(LL_DEBUG1, "SASL encoding failed: %s\n", sasl_errstring(rc, NULL, NULL));
        goto fail;
      }

      rc = sasldata->write(conn, pbuf, plen);
      if (rc != plen)
        goto fail;

      count -= olen;
      buf += olen;
    } while (count > *sasldata->pbufsize);
  }
  else
  {
    /* just write using the underlying socket function */
    rc = sasldata->write(conn, buf, count);
  }

  conn->sockdata = sasldata;

  return rc;

fail:
  conn->sockdata = sasldata;
  return -1;
}

/**
 * mutt_sasl_conn_poll - Check if any data is waiting on a socket - Implements Connection::poll() - @ingroup connection_poll
 */
static int mutt_sasl_conn_poll(struct Connection *conn, time_t wait_secs)
{
  struct SaslSockData *sasldata = conn->sockdata;
  int rc;

  conn->sockdata = sasldata->sockdata;
  rc = sasldata->poll(conn, wait_secs);
  conn->sockdata = sasldata;

  return rc;
}

/**
 * mutt_sasl_client_new - Wrapper for sasl_client_new()
 * @param[in]  conn     Connection to a server
 * @param[out] saslconn SASL connection
 * @retval  0 Success
 * @retval -1 Error
 *
 * which also sets various security properties. If this turns out to be fine
 * for POP too we can probably stop exporting mutt_sasl_get_callbacks().
 */
int mutt_sasl_client_new(struct Connection *conn, sasl_conn_t **saslconn)
{
  if (mutt_sasl_start() != SASL_OK)
    return -1;

  if (!conn->account.service)
  {
    mutt_error(_("Unknown SASL profile"));
    return -1;
  }

  socklen_t size;

  struct sockaddr_storage local = { 0 };
  char iplocalport[IP_PORT_BUFLEN] = { 0 };
  char *plp = NULL;
  size = sizeof(local);
  if (getsockname(conn->fd, (struct sockaddr *) &local, &size) == 0)
  {
    if (iptostring((struct sockaddr *) &local, size, iplocalport, IP_PORT_BUFLEN) == SASL_OK)
      plp = iplocalport;
    else
      mutt_debug(LL_DEBUG2, "SASL failed to parse local IP address\n");
  }
  else
  {
    mutt_debug(LL_DEBUG2, "SASL failed to get local IP address\n");
  }

  struct sockaddr_storage remote = { 0 };
  char ipremoteport[IP_PORT_BUFLEN] = { 0 };
  char *prp = NULL;
  size = sizeof(remote);
  if (getpeername(conn->fd, (struct sockaddr *) &remote, &size) == 0)
  {
    if (iptostring((struct sockaddr *) &remote, size, ipremoteport, IP_PORT_BUFLEN) == SASL_OK)
      prp = ipremoteport;
    else
      mutt_debug(LL_DEBUG2, "SASL failed to parse remote IP address\n");
  }
  else
  {
    mutt_debug(LL_DEBUG2, "SASL failed to get remote IP address\n");
  }

  mutt_debug(LL_DEBUG2, "SASL local ip: %s, remote ip:%s\n", NONULL(plp), NONULL(prp));

  int rc = sasl_client_new(conn->account.service, conn->account.host, plp, prp,
                           mutt_sasl_get_callbacks(&conn->account), 0, saslconn);
  if (rc != SASL_OK)
  {
    mutt_error(_("Error allocating SASL connection"));
    return -1;
  }

  /* Work around a casting bug in the SASL krb4 module */
  sasl_security_properties_t secprops = { 0 };
  secprops.max_ssf = 0x7fff;
  secprops.maxbufsize = MUTT_SASL_MAXBUF;
  if (sasl_setprop(*saslconn, SASL_SEC_PROPS, &secprops) != SASL_OK)
  {
    mutt_error(_("Error setting SASL security properties"));
    sasl_dispose(saslconn);
    return -1;
  }

  if (conn->ssf != 0)
  {
    /* I'm not sure this actually has an effect, at least with SASLv2 */
    mutt_debug(LL_DEBUG2, "External SSF: %d\n", conn->ssf);
    if (sasl_setprop(*saslconn, SASL_SSF_EXTERNAL, &conn->ssf) != SASL_OK)
    {
      mutt_error(_("Error setting SASL external security strength"));
      sasl_dispose(saslconn);
      return -1;
    }
  }
  if (conn->account.user[0])
  {
    mutt_debug(LL_DEBUG2, "External authentication name: %s\n", conn->account.user);
    if (sasl_setprop(*saslconn, SASL_AUTH_EXTERNAL, conn->account.user) != SASL_OK)
    {
      mutt_error(_("Error setting SASL external user name"));
      sasl_dispose(saslconn);
      return -1;
    }
  }

  return 0;
}

/**
 * mutt_sasl_interact - Perform an SASL interaction with the user
 * @param interaction Details of interaction
 * @retval num SASL error code: SASL_OK or SASL_FAIL
 *
 * An example interaction might be asking the user for a password.
 */
int mutt_sasl_interact(sasl_interact_t *interaction)
{
  int rc = SASL_OK;
  char prompt[128] = { 0 };
  struct Buffer *resp = buf_pool_get();

  while (interaction->id != SASL_CB_LIST_END)
  {
    mutt_debug(LL_DEBUG2, "filling in SASL interaction %ld\n", interaction->id);

    snprintf(prompt, sizeof(prompt), "%s: ", interaction->prompt);
    buf_reset(resp);

    if (!OptGui ||
        (mw_get_field(prompt, resp, MUTT_COMP_NO_FLAGS, HC_OTHER, NULL, NULL) != 0))
    {
      rc = SASL_FAIL;
      break;
    }

    interaction->len = buf_len(resp) + 1;
    interaction->result = buf_strdup(resp);
    interaction++;
  }

  buf_pool_release(&resp);
  return rc;
}

/**
 * mutt_sasl_setup_conn - Set up an SASL connection
 * @param conn Connection to a server
 * @param saslconn SASL connection
 *
 * Replace connection methods, sockdata with SASL wrappers, for protection
 * layers. Also get ssf, as a fastpath for the read/write methods.
 */
void mutt_sasl_setup_conn(struct Connection *conn, sasl_conn_t *saslconn)
{
  struct SaslSockData *sasldata = MUTT_MEM_MALLOC(1, struct SaslSockData);
  /* work around sasl_getprop aliasing issues */
  const void *tmp = NULL;

  sasldata->saslconn = saslconn;
  /* get ssf so we know whether we have to (en|de)code read/write */
  sasl_getprop(saslconn, SASL_SSF, &tmp);
  sasldata->ssf = tmp;
  mutt_debug(LL_DEBUG3, "SASL protection strength: %u\n", *sasldata->ssf);
  /* Add SASL SSF to transport SSF */
  conn->ssf += *sasldata->ssf;
  sasl_getprop(saslconn, SASL_MAXOUTBUF, &tmp);
  sasldata->pbufsize = tmp;
  mutt_debug(LL_DEBUG3, "SASL protection buffer size: %u\n", *sasldata->pbufsize);

  /* clear input buffer */
  sasldata->buf = NULL;
  sasldata->bpos = 0;
  sasldata->blen = 0;

  /* preserve old functions */
  sasldata->sockdata = conn->sockdata;
  sasldata->open = conn->open;
  sasldata->read = conn->read;
  sasldata->write = conn->write;
  sasldata->poll = conn->poll;
  sasldata->close = conn->close;

  /* and set up new functions */
  conn->sockdata = sasldata;
  conn->open = mutt_sasl_conn_open;
  conn->read = mutt_sasl_conn_read;
  conn->write = mutt_sasl_conn_write;
  conn->poll = mutt_sasl_conn_poll;
  conn->close = mutt_sasl_conn_close;
}

/**
 * mutt_sasl_cleanup - Invoke when processing is complete
 *
 * This is a cleanup function, used to free all memory used by the library.
 * Invoke when processing is complete.
 */
void mutt_sasl_cleanup(void)
{
  /* As we never use the server-side, the silently ignore the return value */
  sasl_client_done();
}
