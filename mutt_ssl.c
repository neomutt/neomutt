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
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#undef _

#include <string.h>

#include "mutt.h"
#include "mutt_socket.h"
#include "mutt_menu.h"
#include "mutt_curses.h"
#include "mutt_ssl.h"
#include "mutt_idna.h"

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

/* keep a handle on accepted certificates in case we want to
 * open up another connection to the same server in this session */
static STACK_OF(X509) *SslSessionCerts = NULL;

typedef struct _sslsockdata
{
  SSL_CTX *ctx;
  SSL *ssl;
  X509 *cert;
}
sslsockdata;

/* local prototypes */
static int ssl_init (void);
static int add_entropy (const char *file);
static int ssl_socket_read (CONNECTION* conn, char* buf, size_t len);
static int ssl_socket_write (CONNECTION* conn, const char* buf, size_t len);
static int ssl_socket_open (CONNECTION * conn);
static int ssl_socket_close (CONNECTION * conn);
static int tls_close (CONNECTION* conn);
static int ssl_cache_trusted_cert (X509 *cert);
static int ssl_check_certificate (CONNECTION *conn, sslsockdata * data);
static int interactive_check_cert (X509 *cert, int idx, int len);
static void ssl_get_client_cert(sslsockdata *ssldata, CONNECTION *conn);
static int ssl_passwd_cb(char *buf, int size, int rwflag, void *userdata);
static int ssl_negotiate (CONNECTION *conn, sslsockdata*);

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

  ssl_get_client_cert(ssldata, conn);

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

  if (ssl_negotiate (conn, ssldata))
    goto bail_ssl;

  /* hmm. watch out if we're starting TLS over any method other than raw. */
  conn->sockdata = ssldata;
  conn->conn_read = ssl_socket_read;
  conn->conn_write = ssl_socket_write;
  conn->conn_close = tls_close;

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
static int ssl_init (void)
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


int mutt_ssl_socket_setup (CONNECTION * conn)
{
  if (ssl_init() < 0)
  {
    conn->conn_open = ssl_socket_open_err;
    return -1;
  }

  conn->conn_open	= ssl_socket_open;
  conn->conn_read	= ssl_socket_read;
  conn->conn_write	= ssl_socket_write;
  conn->conn_close	= ssl_socket_close;
  conn->conn_poll       = raw_socket_poll;

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

  ssl_get_client_cert(data, conn);

  data->ssl = SSL_new (data->ctx);
  SSL_set_fd (data->ssl, conn->fd);

  if (ssl_negotiate(conn, data))
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
static int ssl_negotiate (CONNECTION *conn, sslsockdata* ssldata)
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
      errmsg = ERR_error_string (ERR_get_error (), NULL);
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

  if (!ssl_check_certificate (conn, ssldata))
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

    /* hold onto this for the life of mutt, in case we want to reconnect.
     * The purist in me wants a mutt_exit hook. */
#if 0
    X509_free (data->cert);
#endif
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
  conn->conn_read = raw_socket_read;
  conn->conn_write = raw_socket_write;
  conn->conn_close = raw_socket_close;

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
      safe_strcat (s, l, ch);
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
  int pass = 0, i;

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

  for (i = 0; i < sk_X509_num (SslSessionCerts); i++)
    pass += (X509_STORE_add_cert (ctx, sk_X509_value (SslSessionCerts, i)) != 0);

  if (pass == 0)
  {
    /* nothing to do */
    X509_STORE_free (ctx);
    return 0;
  }

  X509_STORE_CTX_init (&xsc, ctx, peercert, SslSessionCerts);

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
    dprint (2, (debugfile, " [%s]\n", peercert->name));
  }
#endif
  X509_STORE_CTX_cleanup (&xsc);
  X509_STORE_free (ctx);

  return pass;
}

static int compare_certificates (X509 *cert, X509 *peercert,
  unsigned char *peermd, unsigned int peermdlen)
{
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int mdlen;

  /* Avoid CPU-intensive digest calculation if the certificates are
    * not even remotely equal.
    */
  if (X509_subject_name_cmp (cert, peercert) != 0 ||
      X509_issuer_name_cmp (cert, peercert) != 0)
    return -1;

  if (!X509_digest (cert, EVP_sha1(), md, &mdlen) || peermdlen != mdlen)
    return -1;

  if (memcmp(peermd, md, mdlen) != 0)
    return -1;

  return 0;
}

static int check_certificate_cache (X509 *peercert)
{
  unsigned char peermd[EVP_MAX_MD_SIZE];
  unsigned int peermdlen;
  X509 *cert;
  int i;

  if (!X509_digest (peercert, EVP_sha1(), peermd, &peermdlen)
      || !SslSessionCerts)
  {
    return 0;
  }

  for (i = sk_X509_num (SslSessionCerts); i-- > 0;)
  {
    cert = sk_X509_value (SslSessionCerts, i);
    if (!compare_certificates (cert, peercert, peermd, peermdlen))
    {
      return 1;
    }
  }

  return 0;
}

static int check_certificate_by_digest (X509 *peercert)
{
  unsigned char peermd[EVP_MAX_MD_SIZE];
  unsigned int peermdlen;
  X509 *cert = NULL;
  int pass = 0;
  FILE *fp;

  /* expiration check */
  if (option (OPTSSLVERIFYDATES) != M_NO)
  {
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
  }

  if ((fp = fopen (SslCertFile, "rt")) == NULL)
    return 0;

  if (!X509_digest (peercert, EVP_sha1(), peermd, &peermdlen))
  {
    safe_fclose (&fp);
    return 0;
  }

  while ((cert = READ_X509_KEY (fp, &cert)) != NULL)
  {
    pass = compare_certificates (cert, peercert, peermd, peermdlen) ? 0 : 1;

    if (pass)
      break;
  }
  X509_free (cert);
  safe_fclose (&fp);

  return pass;
}

/* port to mutt from msmtp's tls.c */
static int hostname_match (const char *hostname, const char *certname)
{
  const char *cmp1, *cmp2;

  if (strncmp(certname, "*.", 2) == 0)
  {
    cmp1 = certname + 2;
    cmp2 = strchr(hostname, '.');
    if (!cmp2)
    {
      return 0;
    }
    else
    {
      cmp2++;
    }
  }
  else
  {
    cmp1 = certname;
    cmp2 = hostname;
  }

  if (*cmp1 == '\0' || *cmp2 == '\0')
  {
    return 0;
  }

  if (strcasecmp(cmp1, cmp2) != 0)
  {
    return 0;
  }

  return 1;
}

/* port to mutt from msmtp's tls.c */
static int check_host (X509 *x509cert, const char *hostname, char *err, size_t errlen)
{
  int i, rc = 0;
  /* hostname in ASCII format: */
  char *hostname_ascii = NULL;
  /* needed to get the common name: */
  X509_NAME *x509_subject;
  char *buf = NULL;
  int bufsize;
  /* needed to get the DNS subjectAltNames: */
  STACK *subj_alt_names;
  int subj_alt_names_count;
  GENERAL_NAME *subj_alt_name;
  /* did we find a name matching hostname? */
  int match_found;

  /* Check if 'hostname' matches the one of the subjectAltName extensions of
   * type DNS or the Common Name (CN). */

#ifdef HAVE_LIBIDN
  if (idna_to_ascii_lz(hostname, &hostname_ascii, 0) != IDNA_SUCCESS)
  {
    hostname_ascii = safe_strdup(hostname);
  }
#else
  hostname_ascii = safe_strdup(hostname);
#endif

  /* Try the DNS subjectAltNames. */
  match_found = 0;
  if ((subj_alt_names = X509_get_ext_d2i(x509cert, NID_subject_alt_name,
					 NULL, NULL)))
  {
    subj_alt_names_count = sk_GENERAL_NAME_num(subj_alt_names);
    for (i = 0; i < subj_alt_names_count; i++)
    {
      subj_alt_name = sk_GENERAL_NAME_value(subj_alt_names, i);
      if (subj_alt_name->type == GEN_DNS)
      {
	if ((match_found = hostname_match(hostname_ascii,
					  (char *)(subj_alt_name->d.ia5->data))))
	{
	  break;
	}
      }
    }
  }

  if (!match_found)
  {
    /* Try the common name */
    if (!(x509_subject = X509_get_subject_name(x509cert)))
    {
      if (err && errlen)
	strfcpy (err, _("cannot get certificate subject"), errlen);
      goto out;
    }

    bufsize = X509_NAME_get_text_by_NID(x509_subject, NID_commonName,
					NULL, 0);
    bufsize++;
    buf = safe_malloc((size_t)bufsize);
    if (X509_NAME_get_text_by_NID(x509_subject, NID_commonName,
				  buf, bufsize) == -1)
    {
      if (err && errlen)
	strfcpy (err, _("cannot get certificate common name"), errlen);
      goto out;
    }
    match_found = hostname_match(hostname_ascii, buf);
  }

  if (!match_found)
  {
    if (err && errlen)
      snprintf (err, errlen, _("certificate owner does not match hostname %s"),
		hostname);
    goto out;
  }

  rc = 1;

out:
  FREE(&buf);
  FREE(&hostname_ascii);

  return rc;
}

static int ssl_cache_trusted_cert (X509 *c)
{
  dprint (1, (debugfile, "trusted: %s\n", c->name));
  if (!SslSessionCerts)
    SslSessionCerts = sk_new_null();
  return (sk_X509_push (SslSessionCerts, X509_dup(c)));
}

/* check whether cert is preauthorized. If host is not null, verify that
 * it matches the certificate.
 * Return > 0: authorized, < 0: problems, 0: unknown validity */
static int ssl_check_preauth (X509 *cert, const char* host)
{
  char buf[SHORT_STRING];

  /* check session cache first */
  if (check_certificate_cache (cert))
  {
    dprint (2, (debugfile, "ssl_check_preauth: using cached certificate\n"));
    return 1;
  }

  buf[0] = 0;
  if (host && option (OPTSSLVERIFYHOST) != M_NO)
  {
    if (!check_host (cert, host, buf, sizeof (buf)))
    {
      mutt_error (_("Certificate host check failed: %s"), buf);
      mutt_sleep (2);
      return -1;
    }
    dprint (2, (debugfile, "ssl_check_preauth: hostname check passed\n"));
  }

  if (check_certificate_by_signer (cert))
  {
    dprint (2, (debugfile, "ssl_check_preauth: signer check passed\n"));
    return 1;
  }

  /* automatic check from user's database */
  if (SslCertFile && check_certificate_by_digest (cert))
  {
    dprint (2, (debugfile, "ssl_check_preauth: digest check passed\n"));
    return 1;
  }

  return 0;
}

static int ssl_check_certificate (CONNECTION *conn, sslsockdata *data)
{
  int i, preauthrc, chain_len;
  STACK_OF(X509) *chain;
  X509 *cert;

  if ((preauthrc = ssl_check_preauth (data->cert, conn->account.host)) > 0)
    return preauthrc;

  chain = SSL_get_peer_cert_chain (data->ssl);
  chain_len = sk_X509_num (chain);
  /* negative preauthrc means the certificate won't be accepted without
   * manual override. */
  if (preauthrc < 0 || !chain || (chain_len <= 1))
    return interactive_check_cert (data->cert, 0, 0);

  /* check the chain from root to peer. */
  for (i = chain_len-1; i >= 0; i--)
  {
    cert = sk_X509_value (chain, i);

    /* if the certificate validates or is manually accepted, then add it to
     * the trusted set and recheck the peer certificate */
    if (ssl_check_preauth (cert, NULL)
	|| interactive_check_cert (cert, i, chain_len))
    {
      ssl_cache_trusted_cert (cert);
      if (ssl_check_preauth (data->cert, conn->account.host))
	return 1;
    }
  }

  return 0;
}

static int interactive_check_cert (X509 *cert, int idx, int len)
{
  char *part[] =
    {"/CN=", "/Email=", "/O=", "/OU=", "/L=", "/ST=", "/C="};
  char helpstr[LONG_STRING];
  char buf[STRING];
  char title[STRING];
  MUTTMENU *menu = mutt_new_menu (-1);
  int done, row, i;
  FILE *fp;
  char *name = NULL, *c;

  dprint (2, (debugfile, "interactive_check_cert: %s\n", cert->name));

  menu->max = 19;
  menu->dialog = (char **) safe_calloc (1, menu->max * sizeof (char *));
  for (i = 0; i < menu->max; i++)
    menu->dialog[i] = (char *) safe_calloc (1, SHORT_STRING * sizeof (char));

  row = 0;
  strfcpy (menu->dialog[row], _("This certificate belongs to:"), SHORT_STRING);
  row++;
  name = X509_NAME_oneline (X509_get_subject_name (cert),
			    buf, sizeof (buf));
  dprint (2, (debugfile, "oneline: %s\n", name));

  for (i = 0; i < 5; i++)
  {
    c = x509_get_part (name, part[i]);
    snprintf (menu->dialog[row++], SHORT_STRING, "   %s", c);
  }

  row++;
  strfcpy (menu->dialog[row], _("This certificate was issued by:"), SHORT_STRING);
  row++;
  name = X509_NAME_oneline (X509_get_issuer_name (cert),
			    buf, sizeof (buf));
  for (i = 0; i < 5; i++)
  {
    c = x509_get_part (name, part[i]);
    snprintf (menu->dialog[row++], SHORT_STRING, "   %s", c);
  }

  row++;
  snprintf (menu->dialog[row++], SHORT_STRING, _("This certificate is valid"));
  snprintf (menu->dialog[row++], SHORT_STRING, _("   from %s"),
      asn1time_to_string (X509_get_notBefore (cert)));
  snprintf (menu->dialog[row++], SHORT_STRING, _("     to %s"),
      asn1time_to_string (X509_get_notAfter (cert)));

  row++;
  buf[0] = '\0';
  x509_fingerprint (buf, sizeof (buf), cert);
  snprintf (menu->dialog[row++], SHORT_STRING, _("Fingerprint: %s"), buf);

  snprintf (title, sizeof (title),
	    _("SSL Certificate check (certificate %d of %d in chain)"),
	    len - idx, len);
  menu->title = title;
  if (SslCertFile
      && (option (OPTSSLVERIFYDATES) == M_NO
	  || (X509_cmp_current_time (X509_get_notAfter (cert)) >= 0
	      && X509_cmp_current_time (X509_get_notBefore (cert)) < 0)))
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
  safe_strcat (helpstr, sizeof (helpstr), buf);
  mutt_make_help (buf, sizeof (buf), _("Help"), MENU_GENERIC, OP_HELP);
  safe_strcat (helpstr, sizeof (helpstr), buf);
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
	  if (PEM_write_X509 (fp, cert))
	    done = 1;
	  safe_fclose (&fp);
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
	ssl_cache_trusted_cert (cert);
        break;
    }
  }
  unset_option(OPTUNBUFFEREDINPUT);
  mutt_menuDestroy (&menu);
  dprint (2, (debugfile, "ssl interactive_check_cert: done=%d\n", done));
  return (done == 2);
}

static void ssl_get_client_cert(sslsockdata *ssldata, CONNECTION *conn)
{
  if (SslClientCert)
  {
    dprint (2, (debugfile, "Using client certificate %s\n", SslClientCert));
    SSL_CTX_set_default_passwd_cb_userdata(ssldata->ctx, &conn->account);
    SSL_CTX_set_default_passwd_cb(ssldata->ctx, ssl_passwd_cb);
    SSL_CTX_use_certificate_file(ssldata->ctx, SslClientCert, SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(ssldata->ctx, SslClientCert, SSL_FILETYPE_PEM);

    /* if we are using a client cert, SASL may expect an external auth name */
    mutt_account_getuser (&conn->account);
  }
}

static int ssl_passwd_cb(char *buf, int size, int rwflag, void *userdata)
{
  ACCOUNT *account = (ACCOUNT*)userdata;

  if (mutt_account_getuser (account))
    return 0;

  dprint (2, (debugfile, "ssl_passwd_cb: getting password for %s@%s:%u\n",
	      account->user, account->host, account->port));

  if (mutt_account_getpass (account))
    return 0;

  return snprintf(buf, size, "%s", account->pass);
}
