/**
 * @file
 * SASL authentication support
 *
 * @authors
 * Copyright (C) 2000-2008,2012,2014 Brendan Cully <brendan@kublai.com>
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
 * @page conn_sasl SASL authentication support
 *
 * SASL can stack a protection layer on top of an existing connection.  To
 * handle this, we store a saslconn_t in conn->sockdata, and write wrappers
 * which en/decode the read/write stream, then replace sockdata with an
 * embedded copy of the old sockdata and call the underlying functions (which
 * we've also preserved). I thought about trying to make a general stackable
 * connection system, but it seemed like overkill - something is wrong if we
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
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include "mutt/logging.h"
#include "mutt/memory.h"
#include "mutt/message.h"
#include "mutt/string2.h"
#include "sasl.h"
#include "account.h"
#include "connection.h"
#include "mutt_account.h"
#include "options.h"
#include "protos.h"

/* arbitrary. SASL will probably use a smaller buffer anyway. OTOH it's
 * been a while since I've had access to an SASL server which negotiated
 * a protection buffer. */
#define MUTT_SASL_MAXBUF 65536

#define IP_PORT_BUFLEN 1024

static sasl_callback_t MuttSaslCallbacks[5];

static sasl_secret_t *secret_ptr = NULL;

/**
 * getnameinfo_err - Convert a getaddrinfo() error code into an SASL error code
 * @param ret getaddrinfo() error code, e.g. EAI_AGAIN
 * @retval int SASL error code, e.g. SASL_FAIL
 */
static int getnameinfo_err(int ret)
{
  int err;
  mutt_debug(1, "getnameinfo: ");
  switch (ret)
  {
    case EAI_AGAIN:
      mutt_debug(1, "The name could not be resolved at this time.  Future "
                    "attempts may succeed.\n");
      err = SASL_TRYAGAIN;
      break;
    case EAI_BADFLAGS:
      mutt_debug(1, "The flags had an invalid value.\n");
      err = SASL_BADPARAM;
      break;
    case EAI_FAIL:
      mutt_debug(1, "A non-recoverable error occurred.\n");
      err = SASL_FAIL;
      break;
    case EAI_FAMILY:
      mutt_debug(1, "The address family was not recognized or the address "
                    "length was invalid for the specified family.\n");
      err = SASL_BADPROT;
      break;
    case EAI_MEMORY:
      mutt_debug(1, "There was a memory allocation failure.\n");
      err = SASL_NOMEM;
      break;
    case EAI_NONAME:
      mutt_debug(1, "The name does not resolve for the supplied parameters.  "
                    "NI_NAMEREQD is set and the host's name cannot be located, "
                    "or both nodename and servname were null.\n");
      err = SASL_FAIL; /* no real equivalent */
      break;
    case EAI_SYSTEM:
      mutt_debug(1,
                 "A system error occurred.  The error code can be found in "
                 "errno(%d,%s)).\n",
                 errno, strerror(errno));
      err = SASL_FAIL; /* no real equivalent */
      break;
    default:
      mutt_debug(1, "Unknown error %d\n", ret);
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
 * @retval int SASL error code, e.g. SASL_BADPARAM
 *
 * utility function, copied from sasl2 sample code
 */
static int iptostring(const struct sockaddr *addr, socklen_t addrlen, char *out,
                      unsigned int outlen)
{
  char hbuf[NI_MAXHOST], pbuf[NI_MAXSERV];
  int ret;

  if (!addr || !out)
    return SASL_BADPARAM;

  ret = getnameinfo(addr, addrlen, hbuf, sizeof(hbuf), pbuf, sizeof(pbuf),
                    NI_NUMERICHOST |
#ifdef NI_WITHSCOPEID
                        NI_WITHSCOPEID |
#endif
                        NI_NUMERICSERV);
  if (ret)
    return getnameinfo_err(ret);

  if (outlen < strlen(hbuf) + strlen(pbuf) + 2)
    return SASL_BUFOVER;

  snprintf(out, outlen, "%s;%s", hbuf, pbuf);

  return SASL_OK;
}

/**
 * mutt_sasl_cb_log - callback to log SASL messages
 * @param context  Supplied context, always NULL
 * @param priority Debug level
 * @param message  Message
 * @retval int SASL_OK, always
 */
static int mutt_sasl_cb_log(void *context, int priority, const char *message)
{
  mutt_debug(priority, "SASL: %s\n", message);

  return SASL_OK;
}

/**
 * mutt_sasl_start - Initialise SASL library
 * @retval int SASL error code, e.g. SASL_OK
 *
 * Call before doing an SASL exchange - initialises library (if necessary).
 */
static int mutt_sasl_start(void)
{
  static bool sasl_init = false;

  static sasl_callback_t callbacks[2];
  int rc;

  if (sasl_init)
    return SASL_OK;

  /* set up default logging callback */
  callbacks[0].id = SASL_CB_LOG;
  callbacks[0].proc = (int (*)(void)) mutt_sasl_cb_log;
  callbacks[0].context = NULL;

  callbacks[1].id = SASL_CB_LIST_END;
  callbacks[1].proc = NULL;
  callbacks[1].context = NULL;

  rc = sasl_client_init(callbacks);

  if (rc != SASL_OK)
  {
    mutt_debug(1, "libsasl initialisation failed.\n");
    return SASL_FAIL;
  }

  sasl_init = true;

  return SASL_OK;
}

/**
 * mutt_sasl_cb_authname - callback to retrieve authname or user from Account
 * @param[in]  context Account
 * @param[in]  id      Field to get.  SASL_CB_USER or SASL_CB_AUTHNAME
 * @param[out] result  Resulting string
 * @param[out] len     Length of result
 * @retval int SASL error code, e.g. SASL_FAIL
 */
static int mutt_sasl_cb_authname(void *context, int id, const char **result, unsigned int *len)
{
  struct Account *account = (struct Account *) context;

  if (!result)
    return SASL_FAIL;

  *result = NULL;
  if (len)
    *len = 0;

  if (!account)
    return SASL_BADPARAM;

  mutt_debug(2, "getting %s for %s:%u\n", id == SASL_CB_AUTHNAME ? "authname" : "user",
             account->host, account->port);

  if (id == SASL_CB_AUTHNAME)
  {
    if (mutt_account_getlogin(account) < 0)
      return SASL_FAIL;
    *result = account->login;
  }
  else
  {
    if (mutt_account_getuser(account) < 0)
      return SASL_FAIL;
    *result = account->user;
  }

  if (len)
    *len = strlen(*result);

  return SASL_OK;
}

/**
 * mutt_sasl_cb_pass - SASL callback function to get password
 * @param[in]  conn    Connection to a server
 * @param[in]  context Account
 * @param[in]  id      SASL_CB_PASS
 * @param[out] psecret SASL secret
 * @retval int SASL error code, e.g SASL_FAIL
 */
static int mutt_sasl_cb_pass(sasl_conn_t *conn, void *context, int id, sasl_secret_t **psecret)
{
  struct Account *account = (struct Account *) context;
  int len;

  if (!account || !psecret)
    return SASL_BADPARAM;

  mutt_debug(2, "getting password for %s@%s:%u\n", account->login,
             account->host, account->port);

  if (mutt_account_getpass(account) < 0)
    return SASL_FAIL;

  len = strlen(account->pass);

  mutt_mem_realloc(&secret_ptr, sizeof(sasl_secret_t) + len);
  memcpy((char *) secret_ptr->data, account->pass, (size_t) len);
  secret_ptr->len = len;
  *psecret = secret_ptr;

  return SASL_OK;
}

/**
 * mutt_sasl_get_callbacks - Get the SASL callback functions
 * @param account Account to associate with callbacks
 * @retval ptr Array of callback functions
 */
static sasl_callback_t *mutt_sasl_get_callbacks(struct Account *account)
{
  sasl_callback_t *callback = MuttSaslCallbacks;

  callback->id = SASL_CB_USER;
  callback->proc = (int (*)(void)) mutt_sasl_cb_authname;
  callback->context = account;
  callback++;

  callback->id = SASL_CB_AUTHNAME;
  callback->proc = (int (*)(void)) mutt_sasl_cb_authname;
  callback->context = account;
  callback++;

  callback->id = SASL_CB_PASS;
  callback->proc = (int (*)(void)) mutt_sasl_cb_pass;
  callback->context = account;
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
 * mutt_sasl_conn_open - empty wrapper for underlying open function
 * @param conn Connection to the server
 * @retval  0 Success
 * @retval -1 Error
 *
 * We don't know in advance that a connection will use SASL, so we replace
 * conn's methods with sasl methods when authentication is successful, using
 * mutt_sasl_setup_conn
 */
static int mutt_sasl_conn_open(struct Connection *conn)
{
  struct SaslData *sasldata = (struct SaslData *) conn->sockdata;
  conn->sockdata = sasldata->sockdata;
  int rc = (sasldata->msasl_open)(conn);
  conn->sockdata = sasldata;

  return rc;
}

/**
 * mutt_sasl_conn_close - close SASL connection
 * @param conn Connection to a server
 * @retval  0 Success
 * @retval -1 Error
 *
 * Calls underlying close function and disposes of the sasl_conn_t object, then
 * restores connection to pre-sasl state
 */
static int mutt_sasl_conn_close(struct Connection *conn)
{
  struct SaslData *sasldata = (struct SaslData *) conn->sockdata;

  /* restore connection's underlying methods */
  conn->sockdata = sasldata->sockdata;
  conn->conn_open = sasldata->msasl_open;
  conn->conn_close = sasldata->msasl_close;
  conn->conn_read = sasldata->msasl_read;
  conn->conn_write = sasldata->msasl_write;
  conn->conn_poll = sasldata->msasl_poll;

  /* release sasl resources */
  sasl_dispose(&sasldata->saslconn);
  FREE(&sasldata);

  /* call underlying close */
  int rc = (conn->conn_close)(conn);

  return rc;
}

/**
 * mutt_sasl_conn_read - Read data from an SASL connection
 * @param conn Connection to a server
 * @param buf Buffer to store the data
 * @param len Number of bytes to read
 * @retval >0 Success, number of bytes read
 * @retval -1 Error, see errno
 */
static int mutt_sasl_conn_read(struct Connection *conn, char *buf, size_t len)
{
  int rc;
  unsigned int olen;

  struct SaslData *sasldata = (struct SaslData *) conn->sockdata;

  /* if we still have data in our read buffer, copy it into buf */
  if (sasldata->blen > sasldata->bpos)
  {
    olen = (sasldata->blen - sasldata->bpos > len) ? len :
                                                     sasldata->blen - sasldata->bpos;

    memcpy(buf, sasldata->buf + sasldata->bpos, olen);
    sasldata->bpos += olen;

    return olen;
  }

  conn->sockdata = sasldata->sockdata;

  sasldata->bpos = 0;
  sasldata->blen = 0;

  /* and decode the result, if necessary */
  if (*sasldata->ssf)
  {
    do
    {
      /* call the underlying read function to fill the buffer */
      rc = (sasldata->msasl_read)(conn, buf, len);
      if (rc <= 0)
        goto out;

      rc = sasl_decode(sasldata->saslconn, buf, rc, &sasldata->buf, &sasldata->blen);
      if (rc != SASL_OK)
      {
        mutt_debug(1, "SASL decode failed: %s\n", sasl_errstring(rc, NULL, NULL));
        goto out;
      }
    } while (sasldata->blen == 0);

    olen = (sasldata->blen - sasldata->bpos > len) ? len :
                                                     sasldata->blen - sasldata->bpos;

    memcpy(buf, sasldata->buf, olen);
    sasldata->bpos += olen;

    rc = olen;
  }
  else
    rc = (sasldata->msasl_read)(conn, buf, len);

out:
  conn->sockdata = sasldata;

  return rc;
}

/**
 * mutt_sasl_conn_write - Write to an SASL connection
 * @param conn Connection to a server
 * @param buf Buffer to store the data
 * @param len Number of bytes to read
 * @retval >0 Success, number of bytes read
 * @retval -1 Error, see errno
 */
static int mutt_sasl_conn_write(struct Connection *conn, const char *buf, size_t len)
{
  int rc;
  const char *pbuf = NULL;
  unsigned int olen, plen;

  struct SaslData *sasldata = (struct SaslData *) conn->sockdata;
  conn->sockdata = sasldata->sockdata;

  /* encode data, if necessary */
  if (*sasldata->ssf)
  {
    /* handle data larger than MAXOUTBUF */
    do
    {
      olen = (len > *sasldata->pbufsize) ? *sasldata->pbufsize : len;

      rc = sasl_encode(sasldata->saslconn, buf, olen, &pbuf, &plen);
      if (rc != SASL_OK)
      {
        mutt_debug(1, "SASL encoding failed: %s\n", sasl_errstring(rc, NULL, NULL));
        goto fail;
      }

      rc = (sasldata->msasl_write)(conn, pbuf, plen);
      if (rc != plen)
        goto fail;

      len -= olen;
      buf += olen;
    } while (len > *sasldata->pbufsize);
  }
  else
    /* just write using the underlying socket function */
    rc = (sasldata->msasl_write)(conn, buf, len);

  conn->sockdata = sasldata;

  return rc;

fail:
  conn->sockdata = sasldata;
  return -1;
}

/**
 * mutt_sasl_conn_poll - Check an SASL connection for data
 * @param conn Connection to a server
 * @param wait_secs How long to wait for a response
 * @retval >0 There is data to read
 * @retval  0 Read would block
 * @retval -1 Connection doesn't support polling
 */
static int mutt_sasl_conn_poll(struct Connection *conn, time_t wait_secs)
{
  struct SaslData *sasldata = conn->sockdata;
  int rc;

  conn->sockdata = sasldata->sockdata;
  rc = sasldata->msasl_poll(conn, wait_secs);
  conn->sockdata = sasldata;

  return rc;
}

/**
 * mutt_sasl_client_new - wrapper for sasl_client_new
 * @param conn     Connection to a server
 * @param saslconn SASL connection
 * @retval  0 Success
 * @retval -1 Error
 *
 * which also sets various security properties. If this turns out to be fine
 * for POP too we can probably stop exporting mutt_sasl_get_callbacks().
 */
int mutt_sasl_client_new(struct Connection *conn, sasl_conn_t **saslconn)
{
  sasl_security_properties_t secprops;
  struct sockaddr_storage local, remote;
  socklen_t size;
  char iplocalport[IP_PORT_BUFLEN], ipremoteport[IP_PORT_BUFLEN];
  char *plp = NULL;
  char *prp = NULL;
  const char *service = NULL;
  int rc;

  if (mutt_sasl_start() != SASL_OK)
    return -1;

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
#ifdef USE_NNTP
    case MUTT_ACCT_TYPE_NNTP:
      service = "nntp";
      break;
#endif
    default:
      mutt_error(_("Unknown SASL profile"));
      return -1;
  }

  size = sizeof(local);
  if (!getsockname(conn->fd, (struct sockaddr *) &local, &size))
  {
    if (iptostring((struct sockaddr *) &local, size, iplocalport, IP_PORT_BUFLEN) == SASL_OK)
      plp = iplocalport;
    else
      mutt_debug(2, "SASL failed to parse local IP address\n");
  }
  else
    mutt_debug(2, "SASL failed to get local IP address\n");

  size = sizeof(remote);
  if (!getpeername(conn->fd, (struct sockaddr *) &remote, &size))
  {
    if (iptostring((struct sockaddr *) &remote, size, ipremoteport, IP_PORT_BUFLEN) == SASL_OK)
      prp = ipremoteport;
    else
      mutt_debug(2, "SASL failed to parse remote IP address\n");
  }
  else
    mutt_debug(2, "SASL failed to get remote IP address\n");

  mutt_debug(2, "SASL local ip: %s, remote ip:%s\n", NONULL(plp), NONULL(prp));

  rc = sasl_client_new(service, conn->account.host, plp, prp,
                       mutt_sasl_get_callbacks(&conn->account), 0, saslconn);

  if (rc != SASL_OK)
  {
    mutt_error(_("Error allocating SASL connection"));
    return -1;
  }

  memset(&secprops, 0, sizeof(secprops));
  /* Work around a casting bug in the SASL krb4 module */
  secprops.max_ssf = 0x7fff;
  secprops.maxbufsize = MUTT_SASL_MAXBUF;
  if (sasl_setprop(*saslconn, SASL_SEC_PROPS, &secprops) != SASL_OK)
  {
    mutt_error(_("Error setting SASL security properties"));
    return -1;
  }

  if (conn->ssf)
  {
    /* I'm not sure this actually has an effect, at least with SASLv2 */
    mutt_debug(2, "External SSF: %d\n", conn->ssf);
    if (sasl_setprop(*saslconn, SASL_SSF_EXTERNAL, &(conn->ssf)) != SASL_OK)
    {
      mutt_error(_("Error setting SASL external security strength"));
      return -1;
    }
  }
  if (conn->account.user[0])
  {
    mutt_debug(2, "External authentication name: %s\n", conn->account.user);
    if (sasl_setprop(*saslconn, SASL_AUTH_EXTERNAL, conn->account.user) != SASL_OK)
    {
      mutt_error(_("Error setting SASL external user name"));
      return -1;
    }
  }

  return 0;
}

/**
 * mutt_sasl_interact - Perform an SASL interaction with the user
 * @param interaction Details of interaction
 * @retval int SASL error code: SASL_OK or SASL_FAIL
 *
 * An example interaction might be asking the user for a password.
 */
int mutt_sasl_interact(sasl_interact_t *interaction)
{
  char prompt[SHORT_STRING];
  char resp[SHORT_STRING];

  while (interaction->id != SASL_CB_LIST_END)
  {
    mutt_debug(2, "filling in SASL interaction %ld.\n", interaction->id);

    snprintf(prompt, sizeof(prompt), "%s: ", interaction->prompt);
    resp[0] = '\0';
    if (OptNoCurses || mutt_get_field(prompt, resp, sizeof(resp), 0))
      return SASL_FAIL;

    interaction->len = mutt_str_strlen(resp) + 1;
    interaction->result = mutt_mem_malloc(interaction->len);
    memcpy((char *) interaction->result, resp, interaction->len);

    interaction++;
  }

  return SASL_OK;
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
  struct SaslData *sasldata = mutt_mem_malloc(sizeof(struct SaslData));
  /* work around sasl_getprop aliasing issues */
  const void *tmp = NULL;

  sasldata->saslconn = saslconn;
  /* get ssf so we know whether we have to (en|de)code read/write */
  sasl_getprop(saslconn, SASL_SSF, &tmp);
  sasldata->ssf = tmp;
  mutt_debug(3, "SASL protection strength: %u\n", *sasldata->ssf);
  /* Add SASL SSF to transport SSF */
  conn->ssf += *sasldata->ssf;
  sasl_getprop(saslconn, SASL_MAXOUTBUF, &tmp);
  sasldata->pbufsize = tmp;
  mutt_debug(3, "SASL protection buffer size: %u\n", *sasldata->pbufsize);

  /* clear input buffer */
  sasldata->buf = NULL;
  sasldata->bpos = 0;
  sasldata->blen = 0;

  /* preserve old functions */
  sasldata->sockdata = conn->sockdata;
  sasldata->msasl_open = conn->conn_open;
  sasldata->msasl_close = conn->conn_close;
  sasldata->msasl_read = conn->conn_read;
  sasldata->msasl_write = conn->conn_write;
  sasldata->msasl_poll = conn->conn_poll;

  /* and set up new functions */
  conn->sockdata = sasldata;
  conn->conn_open = mutt_sasl_conn_open;
  conn->conn_close = mutt_sasl_conn_close;
  conn->conn_read = mutt_sasl_conn_read;
  conn->conn_write = mutt_sasl_conn_write;
  conn->conn_poll = mutt_sasl_conn_poll;
}

/**
 * mutt_sasl_done - Invoke when processing is complete.
 *
 * This is a cleanup function, used to free all memory used by the library.
 * Invoke when processing is complete.
 */
void mutt_sasl_done(void)
{
#ifdef HAVE_SASL_CLIENT_DONE
  /* As we never use the server-side, the silently ignore the return value */
  sasl_client_done();
#else
  sasl_done();
#endif
}
