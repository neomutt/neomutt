/*
 * Copyright (C) 2000-2008,2012,2014 Brendan Cully <brendan@kublai.com>
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

/* common SASL helper routines */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "account.h"
#include "mutt_sasl.h"
#include "mutt_socket.h"

#include <errno.h>
#include <netdb.h>
#include <sasl/sasl.h>
#include <sys/socket.h>
#include <netinet/in.h>

static int getnameinfo_err(int ret)
{
  int err;
  dprint (1, (debugfile, "getnameinfo: "));
  switch(ret)
  {
     case EAI_AGAIN:
       dprint (1, (debugfile, "The name could not be resolved at this time.  Future attempts may succeed.\n"));
       err=SASL_TRYAGAIN;
       break;
     case EAI_BADFLAGS:
       dprint (1, (debugfile, "The flags had an invalid value.\n"));
       err=SASL_BADPARAM;
       break;
     case EAI_FAIL:
       dprint (1, (debugfile, "A non-recoverable error occurred.\n"));
       err=SASL_FAIL;
       break;
     case EAI_FAMILY:
       dprint (1, (debugfile, "The address family was not recognized or the address length was invalid for the specified family.\n"));
       err=SASL_BADPROT;
       break;
     case EAI_MEMORY:
       dprint (1, (debugfile, "There was a memory allocation failure.\n"));
       err=SASL_NOMEM;
       break;
     case EAI_NONAME:
       dprint (1, (debugfile, "The name does not resolve for the supplied parameters.  NI_NAMEREQD is set and the host's name cannot be located, or both nodename and servname were null.\n"));
       err=SASL_FAIL; /* no real equivalent */
       break;
     case EAI_SYSTEM:
       dprint (1, (debugfile, "A system error occurred.  The error code can be found in errno(%d,%s)).\n",errno,strerror(errno)));
       err=SASL_FAIL; /* no real equivalent */
       break;
     default:
       dprint (1, (debugfile, "Unknown error %d\n",ret));
       err=SASL_FAIL; /* no real equivalent */
       break;
  }
  return err;
}

/* arbitrary. SASL will probably use a smaller buffer anyway. OTOH it's
 * been a while since I've had access to an SASL server which negotiated
 * a protection buffer. */ 
#define MUTT_SASL_MAXBUF 65536

#define IP_PORT_BUFLEN 1024

static sasl_callback_t mutt_sasl_callbacks[5];

static sasl_secret_t *secret_ptr = NULL;

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
static int mutt_sasl_conn_poll (CONNECTION* conn);

/* utility function, stolen from sasl2 sample code */
static int iptostring(const struct sockaddr *addr, socklen_t addrlen,
                      char *out, unsigned outlen)
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

/* mutt_sasl_start: called before doing a SASL exchange - initialises library
 *   (if necessary). */
int mutt_sasl_start (void)
{
  static unsigned char sasl_init = 0;

  static sasl_callback_t callbacks[2];
  int rc;

  if (sasl_init)
    return SASL_OK;

  /* set up default logging callback */
  callbacks[0].id = SASL_CB_LOG;
  callbacks[0].proc = (int (*)(void))mutt_sasl_cb_log;
  callbacks[0].context = NULL;

  callbacks[1].id = SASL_CB_LIST_END;
  callbacks[1].proc = NULL;
  callbacks[1].context = NULL;

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
  struct sockaddr_storage local, remote;
  socklen_t size;
  char iplocalport[IP_PORT_BUFLEN], ipremoteport[IP_PORT_BUFLEN];
  char* plp = NULL;
  char* prp = NULL;
  const char* service;
  int rc;

  if (mutt_sasl_start () != SASL_OK)
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
    default:
      mutt_error (_("Unknown SASL profile"));
      return -1;
  }

  size = sizeof (local);
  if (!getsockname (conn->fd, (struct sockaddr *)&local, &size)) {
    if (iptostring((struct sockaddr *)&local, size, iplocalport,
                   IP_PORT_BUFLEN) == SASL_OK)
      plp = iplocalport;
    else
      dprint (2, (debugfile, "SASL failed to parse local IP address\n"));
  }
  else
    dprint (2, (debugfile, "SASL failed to get local IP address\n"));
  
  size = sizeof (remote);
  if (!getpeername (conn->fd, (struct sockaddr *)&remote, &size)){
    if (iptostring((struct sockaddr *)&remote, size, ipremoteport,
                   IP_PORT_BUFLEN) == SASL_OK)
      prp = ipremoteport;
    else
      dprint (2, (debugfile, "SASL failed to parse remote IP address\n"));
  }
  else
    dprint (2, (debugfile, "SASL failed to get remote IP address\n"));

  dprint (2, (debugfile, "SASL local ip: %s, remote ip:%s\n", NONULL(plp),
	      NONULL(prp)));
  
  rc = sasl_client_new (service, conn->account.host, plp, prp,
    mutt_sasl_get_callbacks (&conn->account), 0, saslconn);

  if (rc != SASL_OK)
  {
    mutt_error (_("Error allocating SASL connection"));
    mutt_sleep (2);
    return -1;
  }

  memset (&secprops, 0, sizeof (secprops));
  /* Work around a casting bug in the SASL krb4 module */
  secprops.max_ssf = 0x7fff;
  secprops.maxbufsize = MUTT_SASL_MAXBUF;
  if (sasl_setprop (*saslconn, SASL_SEC_PROPS, &secprops) != SASL_OK)
  {
    mutt_error (_("Error setting SASL security properties"));
    return -1;
  }

  if (conn->ssf)
  {
    /* I'm not sure this actually has an effect, at least with SASLv2 */
    dprint (2, (debugfile, "External SSF: %d\n", conn->ssf));
    if (sasl_setprop (*saslconn, SASL_SSF_EXTERNAL, &(conn->ssf)) != SASL_OK)
    {
      mutt_error (_("Error setting SASL external security strength"));
      return -1;
    }
  }
  if (conn->account.user[0])
  {
    dprint (2, (debugfile, "External authentication name: %s\n", conn->account.user));
    if (sasl_setprop (*saslconn, SASL_AUTH_EXTERNAL, conn->account.user) != SASL_OK)
    {
      mutt_error (_("Error setting SASL external user name"));
      return -1;
    }
  }

  return 0;
}

sasl_callback_t* mutt_sasl_get_callbacks (ACCOUNT* account)
{
  sasl_callback_t* callback;

  callback = mutt_sasl_callbacks;

  callback->id = SASL_CB_USER;
  callback->proc = (int (*)(void))mutt_sasl_cb_authname;
  callback->context = account;
  callback++;

  callback->id = SASL_CB_AUTHNAME;
  callback->proc = (int (*)(void))mutt_sasl_cb_authname;
  callback->context = account;
  callback++;

  callback->id = SASL_CB_PASS;
  callback->proc = (int (*)(void))mutt_sasl_cb_pass;
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
    if (option (OPTNOCURSES) || mutt_get_field (prompt, resp, sizeof (resp), 0))
      return SASL_FAIL;

    interaction->len = mutt_strlen (resp)+1;
    interaction->result = safe_malloc (interaction->len);
    memcpy ((char *)interaction->result, resp, interaction->len);

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
  /* work around sasl_getprop aliasing issues */
  const void* tmp;

  sasldata->saslconn = saslconn;
  /* get ssf so we know whether we have to (en|de)code read/write */
  sasl_getprop (saslconn, SASL_SSF, &tmp);
  sasldata->ssf = tmp;
  dprint (3, (debugfile, "SASL protection strength: %u\n", *sasldata->ssf));
  /* Add SASL SSF to transport SSF */
  conn->ssf += *sasldata->ssf;
  sasl_getprop (saslconn, SASL_MAXOUTBUF, &tmp);
  sasldata->pbufsize = tmp;
  dprint (3, (debugfile, "SASL protection buffer size: %u\n", *sasldata->pbufsize));

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

/* mutt_sasl_cb_log: callback to log SASL messages */
static int mutt_sasl_cb_log (void* context, int priority, const char* message)
{
  dprint (priority, (debugfile, "SASL: %s\n", message));

  return SASL_OK;
}

void mutt_sasl_done (void)
{
  sasl_done ();
}

/* mutt_sasl_cb_authname: callback to retrieve authname or user from ACCOUNT */
static int mutt_sasl_cb_authname (void* context, int id, const char** result,
  unsigned* len)
{
  ACCOUNT* account = (ACCOUNT*) context;

  if (!result)
    return SASL_FAIL;

  *result = NULL;
  if (len)
    *len = 0;

  if (!account)
    return SASL_BADPARAM;

  dprint (2, (debugfile, "mutt_sasl_cb_authname: getting %s for %s:%u\n",
	      id == SASL_CB_AUTHNAME ? "authname" : "user",
	      account->host, account->port));

  if (id == SASL_CB_AUTHNAME)
  {
    if (mutt_account_getlogin (account))
      return SASL_FAIL;
    *result = account->login;
  }
  else
  {
    if (mutt_account_getuser (account))
      return SASL_FAIL;
    *result = account->user;
  }
  
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
    "mutt_sasl_cb_pass: getting password for %s@%s:%u\n", account->login,
    account->host, account->port));

  if (mutt_account_getpass (account))
    return SASL_FAIL;

  len = strlen (account->pass);

  safe_realloc (&secret_ptr, sizeof (sasl_secret_t) + len);
  memcpy ((char *) secret_ptr->data, account->pass, (size_t) len);
  secret_ptr->len = len;
  *psecret = secret_ptr;

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
  rc = (sasldata->msasl_open) (conn);
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
  conn->conn_open = sasldata->msasl_open;
  conn->conn_close = sasldata->msasl_close;
  conn->conn_read = sasldata->msasl_read;
  conn->conn_write = sasldata->msasl_write;
  conn->conn_poll = sasldata->msasl_poll;

  /* release sasl resources */
  sasl_dispose (&sasldata->saslconn);
  FREE (&sasldata);

  /* call underlying close */
  rc = (conn->conn_close) (conn);

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

  sasldata->bpos = 0;
  sasldata->blen = 0;

  /* and decode the result, if necessary */
  if (*sasldata->ssf)
  {
    do
    {
      /* call the underlying read function to fill the buffer */
      rc = (sasldata->msasl_read) (conn, buf, len);
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
    rc = (sasldata->msasl_read) (conn, buf, len);

  out:
    conn->sockdata = sasldata;

    return rc;
}

static int mutt_sasl_conn_write (CONNECTION* conn, const char* buf,
  size_t len)
{
  SASL_DATA* sasldata;
  int rc;

  const char *pbuf;
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

      rc = (sasldata->msasl_write) (conn, pbuf, plen);
      if (rc != plen)
	goto fail;

      len -= olen;
      buf += olen;
    }
    while (len > *sasldata->pbufsize);
  }
  else
  /* just write using the underlying socket function */
    rc = (sasldata->msasl_write) (conn, buf, len);
  
  conn->sockdata = sasldata;

  return rc;

 fail:
  conn->sockdata = sasldata;
  return -1;
}

static int mutt_sasl_conn_poll (CONNECTION* conn)
{
  SASL_DATA* sasldata = conn->sockdata;
  int rc;

  conn->sockdata = sasldata->sockdata;
  rc = sasldata->msasl_poll (conn);
  conn->sockdata = sasldata;

  return rc;
}
