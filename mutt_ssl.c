/*
 * Copyright (C) 1999-2001 Tommi Komulainen <Tommi.Komulainen@iki.fi>
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

/* for SSL NO_* defines */
#include "config.h"

#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#undef _

#include <string.h>

#include "mutt.h"
#include "mutt_socket.h"
#include "mutt_menu.h"
#include "mutt_curses.h"
#include "mutt_ssl.h"

#if OPENSSL_VERSION_NUMBER >= 0x00904000L
#define READ_X509_KEY(fp, key)	PEM_read_X509(fp, key, NULL, NULL)
#else
#define READ_X509_KEY(fp, key)	PEM_read_X509(fp, key, NULL)
#endif

/* Just in case OpenSSL doesn't define DEVRANDOM */
#ifndef DEVRANDOM
#define DEVRANDOM "/dev/urandom"
#endif

/* This is ugly, but as RAND_status came in on OpenSSL version 0.9.5
 * and the code has to support older versions too, this is seemed to
 * be cleaner way compared to having even uglier #ifdefs all around.
 */
#ifdef HAVE_RAND_STATUS
#define HAVE_ENTROPY()	(RAND_status() == 1)
#else
static int entropy_byte_count = 0;
/* OpenSSL fills the entropy pool from /dev/urandom if it exists */
#define HAVE_ENTROPY()	(!access(DEVRANDOM, R_OK) || entropy_byte_count >= 16)
#endif

typedef struct _sslsockdata
{
  SSL_CTX *ctx;
  SSL *ssl;
  X509 *cert;
}
sslsockdata;

/* local prototypes */
int ssl_init (void);
static int add_entropy (const char *file);
static int ssl_check_certificate (sslsockdata * data);
static int ssl_socket_read (CONNECTION* conn, char* buf, size_t len);
static int ssl_socket_write (CONNECTION* conn, const char* buf, size_t len);
static int ssl_socket_open (CONNECTION * conn);
static int ssl_socket_close (CONNECTION * conn);
static int tls_close (CONNECTION* conn);
int ssl_negotiate (sslsockdata*);

/* mutt_ssl_starttls: Negotiate TLS over an already opened connection.
 *   TODO: Merge this code better with ssl_socket_open. */
int mutt_ssl_starttls (CONNECTION* conn)
{
  sslsockdata* ssldata;
  int maxbits;

  if (ssl_init())
    goto bail;

  ssldata = (sslsockdata*) safe_calloc (1, sizeof (sslsockdata));
  /* the ssl_use_xxx protocol options don't apply. We must use TLS in TLS. */
  if (! (ssldata->ctx = SSL_CTX_new (TLSv1_client_method ())))
  {
    dprint (1, (debugfile, "mutt_ssl_starttls: Error allocating SSL_CTX\n"));
    goto bail_ssldata;
  }

  if (! (ssldata->ssl = SSL_new (ssldata->ctx)))
  {
    dprint (1, (debugfile, "mutt_ssl_starttls: Error allocating SSL\n"));
    goto bail_ctx;
  }

  if (SSL_set_fd (ssldata->ssl, conn->fd) != 1)
  {
    dprint (1, (debugfile, "mutt_ssl_starttls: Error setting fd\n"));
    goto bail_ssl;
  }

  if (ssl_negotiate (ssldata))
    goto bail_ssl;

  /* hmm. watch out if we're starting TLS over any method other than raw. */
  conn->sockdata = ssldata;
  conn->read = ssl_socket_read;
  conn->write = ssl_socket_write;
  conn->close = tls_close;

  conn->ssf = SSL_CIPHER_get_bits (SSL_get_current_cipher (ssldata->ssl),
    &maxbits);

  return 0;

 bail_ssl:
  FREE (&ssldata->ssl);
 bail_ctx:
  FREE (&ssldata->ctx);
 bail_ssldata:
  FREE (&ssldata);
 bail:
  return -1;
}

/* 
 * OpenSSL library needs to be fed with sufficient entropy. On systems
 * with /dev/urandom, this is done transparently by the library itself,
 * on other systems we need to fill the entropy pool ourselves.
 *
 * Even though only OpenSSL 0.9.5 and later will complain about the
 * lack of entropy, we try to our best and fill the pool with older
 * versions also. (That's the reason for the ugly #ifdefs and macros,
 * otherwise I could have simply #ifdef'd the whole ssl_init funcion)
 */
int ssl_init (void)
{
  char path[_POSIX_PATH_MAX];
  static unsigned char init_complete = 0;

  if (init_complete)
    return 0;

  if (! HAVE_ENTROPY())
  {
    /* load entropy from files */
    add_entropy (SslEntropyFile);
    add_entropy (RAND_file_name (path, sizeof (path)));
  
    /* load entropy from egd sockets */
#ifdef HAVE_RAND_EGD
    add_entropy (getenv ("EGDSOCKET"));
    snprintf (path, sizeof(path), "%s/.entropy", NONULL(Homedir));
    add_entropy (path);
    add_entropy ("/tmp/entropy");
#endif

    /* shuffle $RANDFILE (or ~/.rnd if unset) */
    RAND_write_file (RAND_file_name (path, sizeof (path)));
    mutt_clear_error ();
    if (! HAVE_ENTROPY())
    {
      mutt_error (_("Failed to find enough entropy on your system"));
      mutt_sleep (2);
      return -1;
    }
  }

  /* I don't think you can do this just before reading the error. The call
   * itself might clobber the last SSL error. */
  SSL_load_error_strings();
  SSL_library_init();
  init_complete = 1;
  return 0;
}

static int add_entropy (const char *file)
{
  struct stat st;
  int n = -1;

  if (!file) return 0;

  if (stat (file, &st) == -1)
    return errno == ENOENT ? 0 : -1;

  mutt_message (_("Filling entropy pool: %s...\n"),
		file);
  
  /* check that the file permissions are secure */
  if (st.st_uid != getuid () || 
      ((st.st_mode & (S_IWGRP | S_IRGRP)) != 0) ||
      ((st.st_mode & (S_IWOTH | S_IROTH)) != 0))
  {
    mutt_error (_("%s has insecure permissions!"), file);
    mutt_sleep (2);
    return -1;
  }

#ifdef HAVE_RAND_EGD
  n = RAND_egd (file);
#endif
  if (n <= 0)
    n = RAND_load_file (file, -1);

#ifndef HAVE_RAND_STATUS
  if (n > 0) entropy_byte_count += n;
#endif
  return n;
}

static int ssl_socket_open_err (CONNECTION *conn)
{
  mutt_error (_("SSL disabled due the lack of entropy"));
  mutt_sleep (2);
  return -1;
}


int ssl_socket_setup (CONNECTION * conn)
{
  if (ssl_init() < 0)
  {
    conn->open = ssl_socket_open_err;
    return -1;
  }

  conn->open	= ssl_socket_open;
  conn->read	= ssl_socket_read;
  conn->write	= ssl_socket_write;
  conn->close	= ssl_socket_close;

  return 0;
}

static int ssl_socket_read (CONNECTION* conn, char* buf, size_t len)
{
  sslsockdata *data = conn->sockdata;
  return SSL_read (data->ssl, buf, len);
}

static int ssl_socket_write (CONNECTION* conn, const char* buf, size_t len)
{
  sslsockdata *data = conn->sockdata;
  return SSL_write (data->ssl, buf, len);
}

static int ssl_socket_open (CONNECTION * conn)
{
  sslsockdata *data;
  int maxbits;

  if (raw_socket_open (conn) < 0)
    return -1;

  data = (sslsockdata *) safe_calloc (1, sizeof (sslsockdata));
  conn->sockdata = data;

  data->ctx = SSL_CTX_new (SSLv23_client_method ());

  /* disable SSL protocols as needed */
  if (!option(OPTTLSV1)) 
  {
    SSL_CTX_set_options(data->ctx, SSL_OP_NO_TLSv1);
  }
  if (!option(OPTSSLV2)) 
  {
    SSL_CTX_set_options(data->ctx, SSL_OP_NO_SSLv2);
  }
  if (!option(OPTSSLV3)) 
  {
    SSL_CTX_set_options(data->ctx, SSL_OP_NO_SSLv3);
  }

  data->ssl = SSL_new (data->ctx);
  SSL_set_fd (data->ssl, conn->fd);

  if (ssl_negotiate(data))
  {
    mutt_socket_close (conn);
    return -1;
  }
  
  conn->ssf = SSL_CIPHER_get_bits (SSL_get_current_cipher (data->ssl),
    &maxbits);

  return 0;
}

/* ssl_negotiate: After SSL state has been initialised, attempt to negotiate
 *   SSL over the wire, including certificate checks. */
int ssl_negotiate (sslsockdata* ssldata)
{
  int err;
  const char* errmsg;

#if OPENSSL_VERSION_NUMBER >= 0x00906000L
  /* This only exists in 0.9.6 and above. Without it we may get interrupted
   *   reads or writes. Bummer. */
  SSL_set_mode (ssldata->ssl, SSL_MODE_AUTO_RETRY);
#endif

  if ((err = SSL_connect (ssldata->ssl)) != 1)
  {
    switch (SSL_get_error (ssldata->ssl, err))
    {
    case SSL_ERROR_SYSCALL:
      errmsg = _("I/O error");
      break;
    case SSL_ERROR_SSL:
      errmsg = _("unspecified protocol error");
      break;
    default:
      errmsg = _("unknown error");
    }
    
    mutt_error (_("SSL failed: %s"), errmsg);
    mutt_sleep (1);

    return -1;
  }

  ssldata->cert = SSL_get_peer_certificate (ssldata->ssl);
  if (!ssldata->cert)
  {
    mutt_error (_("Unable to get certificate from peer"));
    mutt_sleep (1);
    return -1;
  }

  if (!ssl_check_certificate (ssldata))
    return -1;

  mutt_message (_("SSL connection using %s (%s)"), 
    SSL_get_cipher_version (ssldata->ssl), SSL_get_cipher_name (ssldata->ssl));
  mutt_sleep (0);

  return 0;
}

static int ssl_socket_close (CONNECTION * conn)
{
  sslsockdata *data = conn->sockdata;
  if (data)
  {
    SSL_shutdown (data->ssl);

    X509_free (data->cert);
    SSL_free (data->ssl);
    SSL_CTX_free (data->ctx);
    FREE (&conn->sockdata);
  }

  return raw_socket_close (conn);
}

static int tls_close (CONNECTION* conn)
{
  int rc;

  rc = ssl_socket_close (conn);
  conn->read = raw_socket_read;
  conn->write = raw_socket_write;
  conn->close = raw_socket_close;

  return rc;
}

static char *x509_get_part (char *line, const char *ndx)
{
  static char ret[SHORT_STRING];
  char *c, *c2;

  strfcpy (ret, _("Unknown"), sizeof (ret));

  c = strstr (line, ndx);
  if (c)
  {
    c += strlen (ndx);
    c2 = strchr (c, '/');
    if (c2)
      *c2 = '\0';
    strfcpy (ret, c, sizeof (ret));
    if (c2)
      *c2 = '/';
  }

  return ret;
}

static void x509_fingerprint (char *s, int l, X509 * cert)
{
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int n;
  int j;

  if (!X509_digest (cert, EVP_md5 (), md, &n))
  {
    snprintf (s, l, _("[unable to calculate]"));
  }
  else
  {
    for (j = 0; j < (int) n; j++)
    {
      char ch[8];
      snprintf (ch, 8, "%02X%s", md[j], (j % 2 ? " " : ""));
      strncat (s, ch, l);
    }
  }
}

static char *asn1time_to_string (ASN1_UTCTIME *tm)
{
  static char buf[64];
  BIO *bio;

  strfcpy (buf, _("[invalid date]"), sizeof (buf));
  
  bio = BIO_new (BIO_s_mem());
  if (bio)
  {
    if (ASN1_TIME_print (bio, tm))
      (void) BIO_read (bio, buf, sizeof (buf));
    BIO_free (bio);
  }

  return buf;
}

static int check_certificate_by_signer (X509 *peercert)
{
  X509_STORE_CTX xsc;
  X509_STORE *ctx;
  int pass = 0;

  ctx = X509_STORE_new ();
  if (ctx == NULL) return 0;

  if (option (OPTSSLSYSTEMCERTS))
  {
    if (X509_STORE_set_default_paths (ctx))
      pass++;
    else
      dprint (2, (debugfile, "X509_STORE_set_default_paths failed\n"));
  }

  if (X509_STORE_load_locations (ctx, SslCertFile, NULL))
    pass++;
  else
    dprint (2, (debugfile, "X509_STORE_load_locations_failed\n"));

  if (pass == 0)
  {
    /* nothing to do */
    X509_STORE_free (ctx);
    return 0;
  }

  X509_STORE_CTX_init (&xsc, ctx, peercert, NULL);

  pass = (X509_verify_cert (&xsc) > 0);
#ifdef DEBUG
  if (! pass)
  {
    char buf[SHORT_STRING];
    int err;

    err = X509_STORE_CTX_get_error (&xsc);
    snprintf (buf, sizeof (buf), "%s (%d)", 
	X509_verify_cert_error_string(err), err);
    dprint (2, (debugfile, "X509_verify_cert: %s\n", buf));
  }
#endif
  X509_STORE_CTX_cleanup (&xsc);
  X509_STORE_free (ctx);

  return pass;
}

static int check_certificate_by_digest (X509 *peercert)
{
  unsigned char peermd[EVP_MAX_MD_SIZE];
  unsigned int peermdlen;
  X509 *cert = NULL;
  int pass = 0;
  FILE *fp;

  /* expiration check */
  if (X509_cmp_current_time (X509_get_notBefore (peercert)) >= 0)
  {
    dprint (2, (debugfile, "Server certificate is not yet valid\n"));
    mutt_error (_("Server certificate is not yet valid"));
    mutt_sleep (2);
    return 0;
  }
  if (X509_cmp_current_time (X509_get_notAfter (peercert)) <= 0)
  {
    dprint (2, (debugfile, "Server certificate has expired"));
    mutt_error (_("Server certificate has expired"));
    mutt_sleep (2);
    return 0;
  }

  if ((fp = fopen (SslCertFile, "rt")) == NULL)
    return 0;

  if (!X509_digest (peercert, EVP_sha1(), peermd, &peermdlen))
  {
    fclose (fp);
    return 0;
  }
  
  while ((cert = READ_X509_KEY (fp, &cert)) != NULL)
  {
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int mdlen;

    /* Avoid CPU-intensive digest calculation if the certificates are
     * not even remotely equal.
     */
    if (X509_subject_name_cmp (cert, peercert) != 0 ||
	X509_issuer_name_cmp (cert, peercert) != 0)
      continue;

    if (!X509_digest (cert, EVP_sha1(), md, &mdlen) || peermdlen != mdlen)
      continue;
    
    if (memcmp(peermd, md, mdlen) != 0)
      continue;

    pass = 1;
    break;
  }
  X509_free (cert);
  fclose (fp);

  return pass;
}

static int ssl_check_certificate (sslsockdata * data)
{
  char *part[] =
  {"/CN=", "/Email=", "/O=", "/OU=", "/L=", "/ST=", "/C="};
  char helpstr[SHORT_STRING];
  char buf[SHORT_STRING];
  MUTTMENU *menu;
  int done, row, i;
  FILE *fp;
  char *name = NULL, *c;

  if (check_certificate_by_signer (data->cert))
  {
    dprint (1, (debugfile, "ssl_check_certificate: signer check passed\n"));
    return 1;
  }

  /* automatic check from user's database */
  if (SslCertFile && check_certificate_by_digest (data->cert))
  {
    dprint (1, (debugfile, "ssl_check_certificate: digest check passed\n"));
    return 1;
  }

  /* interactive check from user */
  menu = mutt_new_menu ();
  menu->max = 19;
  menu->dialog = (char **) safe_calloc (1, menu->max * sizeof (char *));
  for (i = 0; i < menu->max; i++)
    menu->dialog[i] = (char *) safe_calloc (1, SHORT_STRING * sizeof (char));

  row = 0;
  strfcpy (menu->dialog[row], _("This certificate belongs to:"), SHORT_STRING);
  row++;
  name = X509_NAME_oneline (X509_get_subject_name (data->cert),
			    buf, sizeof (buf));
  for (i = 0; i < 5; i++)
  {
    c = x509_get_part (name, part[i]);
    snprintf (menu->dialog[row++], SHORT_STRING, "   %s", c);
  }

  row++;
  strfcpy (menu->dialog[row], _("This certificate was issued by:"), SHORT_STRING);
  row++;
  name = X509_NAME_oneline (X509_get_issuer_name (data->cert),
			    buf, sizeof (buf));
  for (i = 0; i < 5; i++)
  {
    c = x509_get_part (name, part[i]);
    snprintf (menu->dialog[row++], SHORT_STRING, "   %s", c);
  }

  row++;
  snprintf (menu->dialog[row++], SHORT_STRING, _("This certificate is valid"));
  snprintf (menu->dialog[row++], SHORT_STRING, _("   from %s"), 
      asn1time_to_string (X509_get_notBefore (data->cert)));
  snprintf (menu->dialog[row++], SHORT_STRING, _("     to %s"), 
      asn1time_to_string (X509_get_notAfter (data->cert)));

  row++;
  buf[0] = '\0';
  x509_fingerprint (buf, sizeof (buf), data->cert);
  snprintf (menu->dialog[row++], SHORT_STRING, _("Fingerprint: %s"), buf);

  menu->title = _("SSL Certificate check");
  if (SslCertFile)
  {
    menu->prompt = _("(r)eject, accept (o)nce, (a)ccept always");
    menu->keys = _("roa");
  }
  else
  {
    menu->prompt = _("(r)eject, accept (o)nce");
    menu->keys = _("ro");
  }
  
  helpstr[0] = '\0';
  mutt_make_help (buf, sizeof (buf), _("Exit  "), MENU_GENERIC, OP_EXIT);
  strncat (helpstr, buf, sizeof (helpstr));
  mutt_make_help (buf, sizeof (buf), _("Help"), MENU_GENERIC, OP_HELP);
  strncat (helpstr, buf, sizeof (helpstr));
  menu->help = helpstr;

  done = 0;
  set_option(OPTUNBUFFEREDINPUT);
  while (!done)
  {
    switch (mutt_menuLoop (menu))
    {
      case -1:			/* abort */
      case OP_MAX + 1:		/* reject */
      case OP_EXIT:
        done = 1;
        break;
      case OP_MAX + 3:		/* accept always */
        done = 0;
        if ((fp = fopen (SslCertFile, "a")))
	{
	  if (PEM_write_X509 (fp, data->cert))
	    done = 1;
	  fclose (fp);
	}
	if (!done)
        {
	  mutt_error (_("Warning: Couldn't save certificate"));
	  mutt_sleep (2);
	}
	else
        {
	  mutt_message (_("Certificate saved"));
	  mutt_sleep (0);
	}
        /* fall through */
      case OP_MAX + 2:		/* accept once */
        done = 2;
        break;
    }
  }
  unset_option(OPTUNBUFFEREDINPUT);
  mutt_menuDestroy (&menu);
  return (done == 2);
}
