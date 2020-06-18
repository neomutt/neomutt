/**
 * @file
 * Handling of OpenSSL encryption
 *
 * @authors
 * Copyright (C) 1999-2001 Tommi Komulainen <Tommi.Komulainen@iki.fi>
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
 * @page conn_openssl Handling of OpenSSL encryption
 *
 * Handling of OpenSSL encryption
 */

#include "config.h"
#include <errno.h>
#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/opensslv.h>
#include <openssl/ossl_typ.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/safestack.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "private.h"
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "lib.h"
#include "mutt_globals.h"
#include "mutt_logging.h"
#include "ssl.h"

/* LibreSSL defines OPENSSL_VERSION_NUMBER but sets it to 0x20000000L.
 * So technically we don't need the defined(OPENSSL_VERSION_NUMBER) check.  */
#if (defined(OPENSSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER < 0x10100000L)) || \
    (defined(LIBRESSL_VERSION_NUMBER) && (LIBRESSL_VERSION_NUMBER < 0x2070000fL))
#define X509_get0_notBefore X509_get_notBefore
#define X509_get0_notAfter X509_get_notAfter
#define X509_getm_notBefore X509_get_notBefore
#define X509_getm_notAfter X509_get_notAfter
#define X509_STORE_CTX_get0_chain X509_STORE_CTX_get_chain
#define SSL_has_pending SSL_pending
#endif

/* Unimplemented OpenSSL 1.1 api calls */
#if (defined(LIBRESSL_VERSION_NUMBER) && (LIBRESSL_VERSION_NUMBER >= 0x2070000fL))
#define SSL_has_pending SSL_pending
#endif

/* index for storing hostname as application specific data in SSL structure */
static int HostExDataIndex = -1;

/* Index for storing the "skip mode" state in SSL structure.  When the
 * user skips a certificate in the chain, the stored value will be
 * non-null. */
static int SkipModeExDataIndex = -1;

/* keep a handle on accepted certificates in case we want to
 * open up another connection to the same server in this session */
static STACK_OF(X509) *SslSessionCerts = NULL;

static int ssl_socket_close(struct Connection *conn);

/**
 * struct SslSockData - SSL socket data
 */
struct SslSockData
{
  SSL_CTX *sctx;
  SSL *ssl;
  unsigned char isopen;
};

/**
 * ssl_load_certificates - Load certificates and filter out the expired ones
 * @param ctx SSL context
 * @retval 1 Success
 * @retval 0 Error
 *
 * ssl certificate verification can behave strangely if there are expired certs
 * loaded into the trusted store.  This function filters out expired certs.
 *
 * Previously the code used this form:
 *     SSL_CTX_load_verify_locations (ssldata->ctx, #C_CertificateFile, NULL);
 */
static int ssl_load_certificates(SSL_CTX *ctx)
{
  int rc = 1;

  mutt_debug(LL_DEBUG2, "loading trusted certificates\n");
  X509_STORE *store = SSL_CTX_get_cert_store(ctx);
  if (!store)
  {
    store = X509_STORE_new();
    SSL_CTX_set_cert_store(ctx, store);
  }

  FILE *fp = mutt_file_fopen(C_CertificateFile, "rt");
  if (!fp)
    return 0;

  X509 *cert = NULL;
  while (NULL != PEM_read_X509(fp, &cert, NULL, NULL))
  {
    if ((X509_cmp_current_time(X509_get0_notBefore(cert)) >= 0) ||
        (X509_cmp_current_time(X509_get0_notAfter(cert)) <= 0))
    {
      char buf[256];
      mutt_debug(LL_DEBUG2, "filtering expired cert: %s\n",
                 X509_NAME_oneline(X509_get_subject_name(cert), buf, sizeof(buf)));
    }
    else
    {
      X509_STORE_add_cert(store, cert);
    }
  }
  /* PEM_read_X509 sets the error NO_START_LINE on eof */
  if (ERR_GET_REASON(ERR_peek_last_error()) != PEM_R_NO_START_LINE)
    rc = 0;
  ERR_clear_error();

  X509_free(cert);
  mutt_file_fclose(&fp);

  return rc;
}

/**
 * ssl_set_verify_partial - Allow verification using partial chains (with no root)
 * @param ctx SSL context
 * @retval  0 Success
 * @retval -1 Error
 */
static int ssl_set_verify_partial(SSL_CTX *ctx)
{
  int rc = 0;
#ifdef HAVE_SSL_PARTIAL_CHAIN
  X509_VERIFY_PARAM *param = NULL;

  if (C_SslVerifyPartialChains)
  {
    param = X509_VERIFY_PARAM_new();
    if (param)
    {
      X509_VERIFY_PARAM_set_flags(param, X509_V_FLAG_PARTIAL_CHAIN);
      if (SSL_CTX_set1_param(ctx, param) == 0)
      {
        mutt_debug(LL_DEBUG2, "SSL_CTX_set1_param() failed\n");
        rc = -1;
      }
      X509_VERIFY_PARAM_free(param);
    }
    else
    {
      mutt_debug(LL_DEBUG2, "X509_VERIFY_PARAM_new() failed\n");
      rc = -1;
    }
  }
#endif
  return rc;
}

/**
 * add_entropy - Add a source of random numbers
 * @param file Random device
 * @retval >0 Success, number of bytes read from the source
 * @retval -1 Error
 */
static int add_entropy(const char *file)
{
  if (!file)
    return 0;

  struct stat st;
  int n = -1;

  if (stat(file, &st) == -1)
    return (errno == ENOENT) ? 0 : -1;

  mutt_message(_("Filling entropy pool: %s..."), file);

  /* check that the file permissions are secure */
  if ((st.st_uid != getuid()) || ((st.st_mode & (S_IWGRP | S_IRGRP)) != 0) ||
      ((st.st_mode & (S_IWOTH | S_IROTH)) != 0))
  {
    mutt_error(_("%s has insecure permissions"), file);
    return -1;
  }

#ifdef HAVE_RAND_EGD
  n = RAND_egd(file);
#endif
  if (n <= 0)
    n = RAND_load_file(file, -1);

  return n;
}

/**
 * ssl_err - Display an SSL error message
 * @param data SSL socket data
 * @param err  SSL error code
 */
static void ssl_err(struct SslSockData *data, int err)
{
  int e = SSL_get_error(data->ssl, err);
  switch (e)
  {
    case SSL_ERROR_NONE:
      return;
    case SSL_ERROR_ZERO_RETURN:
      data->isopen = 0;
      break;
    case SSL_ERROR_SYSCALL:
      data->isopen = 0;
      break;
  }

  const char *errmsg = NULL;
  unsigned long sslerr;

  switch (e)
  {
    case SSL_ERROR_SYSCALL:
      errmsg = "I/O error";
      break;
    case SSL_ERROR_WANT_ACCEPT:
      errmsg = "retry accept";
      break;
    case SSL_ERROR_WANT_CONNECT:
      errmsg = "retry connect";
      break;
    case SSL_ERROR_WANT_READ:
      errmsg = "retry read";
      break;
    case SSL_ERROR_WANT_WRITE:
      errmsg = "retry write";
      break;
    case SSL_ERROR_WANT_X509_LOOKUP:
      errmsg = "retry x509 lookup";
      break;
    case SSL_ERROR_ZERO_RETURN:
      errmsg = "SSL connection closed";
      break;
    case SSL_ERROR_SSL:
      sslerr = ERR_get_error();
      switch (sslerr)
      {
        case 0:
          switch (err)
          {
            case 0:
              errmsg = "EOF";
              break;
            default:
              errmsg = strerror(errno);
          }
          break;
        default:
          errmsg = ERR_error_string(sslerr, NULL);
      }
      break;
    default:
      errmsg = "unknown error";
  }

  mutt_debug(LL_DEBUG1, "SSL error: %s\n", errmsg);
}

/**
 * ssl_dprint_err_stack - Dump the SSL error stack
 */
static void ssl_dprint_err_stack(void)
{
  BIO *bio = BIO_new(BIO_s_mem());
  if (!bio)
    return;
  ERR_print_errors(bio);

  char *buf = NULL;
  long buflen = BIO_get_mem_data(bio, &buf);
  if (buflen > 0)
  {
    char *output = mutt_mem_malloc(buflen + 1);
    memcpy(output, buf, buflen);
    output[buflen] = '\0';
    mutt_debug(LL_DEBUG1, "SSL error stack: %s\n", output);
    FREE(&output);
  }
  BIO_free(bio);
}

/**
 * ssl_passwd_cb - Callback to get a password
 * @param buf      Buffer for the password
 * @param buflen   Length of the buffer
 * @param rwflag   0 if writing, 1 if reading (UNUSED)
 * @param userdata ConnAccount whose password is requested
 * @retval >0 Success, number of chars written to buf
 * @retval  0 Error
 */
static int ssl_passwd_cb(char *buf, int buflen, int rwflag, void *userdata)
{
  struct ConnAccount *cac = userdata;

  if (mutt_account_getuser(cac) < 0)
    return 0;

  mutt_debug(LL_DEBUG2, "getting password for %s@%s:%u\n", cac->user, cac->host, cac->port);

  if (mutt_account_getpass(cac) < 0)
    return 0;

  return snprintf(buf, buflen, "%s", cac->pass);
}

/**
 * ssl_socket_open_err - Error callback for opening an SSL connection - Implements Connection::open()
 * @retval -1 Always
 */
static int ssl_socket_open_err(struct Connection *conn)
{
  mutt_error(_("SSL disabled due to the lack of entropy"));
  return -1;
}

/**
 * x509_get_part - Retrieve from X509 data
 * @param name Name of data to retrieve
 * @param nid  ID of the item to retrieve
 * @retval ptr Retrieved data
 *
 * The returned pointer is to a static buffer, so it must not be free()'d.
 */
static char *x509_get_part(X509_NAME *name, int nid)
{
  static char data[128];

  if (!name || (X509_NAME_get_text_by_NID(name, nid, data, sizeof(data)) < 0))
    return NULL;

  return data;
}

/**
 * x509_fingerprint - Generate a fingerprint for an X509 certificate
 * @param s        Buffer for fingerprint
 * @param l        Length of buffer
 * @param cert     Certificate
 * @param hashfunc Hashing function
 */
static void x509_fingerprint(char *s, int l, X509 *cert, const EVP_MD *(*hashfunc)(void) )
{
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int n;

  if (X509_digest(cert, hashfunc(), md, &n) == 0) // Failure
  {
    snprintf(s, l, "%s", _("[unable to calculate]"));
  }
  else
  {
    for (unsigned int i = 0; i < n; i++)
    {
      char ch[8];
      snprintf(ch, sizeof(ch), "%02X%s", md[i], ((i % 2) ? " " : ""));
      mutt_str_cat(s, l, ch);
    }
  }
}

/**
 * asn1time_to_string - Convert a time to a string
 * @param tm Time to convert
 * @retval ptr Time string
 *
 * The returned pointer is to a static buffer, so it must not be free()'d.
 */
static char *asn1time_to_string(ASN1_UTCTIME *tm)
{
  static char buf[64];
  BIO *bio = NULL;

  mutt_str_copy(buf, _("[invalid date]"), sizeof(buf));

  bio = BIO_new(BIO_s_mem());
  if (bio)
  {
    if (ASN1_TIME_print(bio, tm))
      (void) BIO_read(bio, buf, sizeof(buf));
    BIO_free(bio);
  }

  return buf;
}

/**
 * compare_certificates - Compare two X509 certificated
 * @param cert      Certificate
 * @param peercert  Peer certificate
 * @param peermd    Peer certificate message digest
 * @param peermdlen Length of peer certificate message digest
 * @retval true  Certificates match
 * @retval false Certificates differ
 */
static bool compare_certificates(X509 *cert, X509 *peercert,
                                 unsigned char *peermd, unsigned int peermdlen)
{
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int mdlen;

  /* Avoid CPU-intensive digest calculation if the certificates are
   * not even remotely equal.  */
  if ((X509_subject_name_cmp(cert, peercert) != 0) ||
      (X509_issuer_name_cmp(cert, peercert) != 0))
    return false;

  if (!X509_digest(cert, EVP_sha256(), md, &mdlen) || (peermdlen != mdlen))
    return false;

  if (memcmp(peermd, md, mdlen) != 0)
    return false;

  return true;
}

/**
 * check_certificate_expiration - Check if a certificate has expired
 * @param peercert Certificate to check
 * @param silent   If true, don't notify the user if the certificate has expired
 * @retval true  Certificate is valid
 * @retval false Certificate has expired (or hasn't yet become valid)
 */
static bool check_certificate_expiration(X509 *peercert, bool silent)
{
  if (C_SslVerifyDates == MUTT_NO)
    return true;

  if (X509_cmp_current_time(X509_get0_notBefore(peercert)) >= 0)
  {
    if (!silent)
    {
      mutt_debug(LL_DEBUG2, "Server certificate is not yet valid\n");
      mutt_error(_("Server certificate is not yet valid"));
    }
    return false;
  }

  if (X509_cmp_current_time(X509_get0_notAfter(peercert)) <= 0)
  {
    if (!silent)
    {
      mutt_debug(LL_DEBUG2, "Server certificate has expired\n");
      mutt_error(_("Server certificate has expired"));
    }
    return false;
  }

  return true;
}

/**
 * hostname_match - Does the hostname match the certificate
 * @param hostname Hostname
 * @param certname Certificate
 * @retval true Hostname matches the certificate
 */
static bool hostname_match(const char *hostname, const char *certname)
{
  const char *cmp1 = NULL, *cmp2 = NULL;

  if (mutt_strn_equal(certname, "*.", 2))
  {
    cmp1 = certname + 2;
    cmp2 = strchr(hostname, '.');
    if (!cmp2)
      return false;

    cmp2++;
  }
  else
  {
    cmp1 = certname;
    cmp2 = hostname;
  }

  if ((*cmp1 == '\0') || (*cmp2 == '\0'))
  {
    return false;
  }

  if (strcasecmp(cmp1, cmp2) != 0)
  {
    return false;
  }

  return true;
}

/**
 * ssl_init - Initialise the SSL library
 * @retval  0 Success
 * @retval -1 Error
 *
 * OpenSSL library needs to be fed with sufficient entropy. On systems with
 * /dev/urandom, this is done transparently by the library itself, on other
 * systems we need to fill the entropy pool ourselves.
 *
 * Even though only OpenSSL 0.9.5 and later will complain about the lack of
 * entropy, we try to our best and fill the pool with older versions also.
 * (That's the reason for the ugly ifdefs and macros, otherwise I could have
 * simply ifdef'd the whole ssl_init function)
 */
static int ssl_init(void)
{
  static bool init_complete = false;

  if (init_complete)
    return 0;

  if (RAND_status() != 1)
  {
    /* load entropy from files */
    struct Buffer *path = mutt_buffer_pool_get();
    add_entropy(C_EntropyFile);
    add_entropy(RAND_file_name(path->data, path->dsize));

/* load entropy from egd sockets */
#ifdef HAVE_RAND_EGD
    add_entropy(mutt_str_getenv("EGDSOCKET"));
    mutt_buffer_printf(path, "%s/.entropy", NONULL(HomeDir));
    add_entropy(mutt_b2s(path));
    add_entropy("/tmp/entropy");
#endif

    /* shuffle $RANDFILE (or ~/.rnd if unset) */
    RAND_write_file(RAND_file_name(path->data, path->dsize));
    mutt_buffer_pool_release(&path);

    mutt_clear_error();
    if (RAND_status() != 1)
    {
      mutt_error(_("Failed to find enough entropy on your system"));
      return -1;
    }
  }

/* OpenSSL performs automatic initialization as of 1.1.
 * However LibreSSL does not (as of 2.8.3). */
#if (defined(OPENSSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER < 0x10100000L)) || \
    (defined(LIBRESSL_VERSION_NUMBER))
  /* I don't think you can do this just before reading the error. The call
   * itself might clobber the last SSL error. */
  SSL_load_error_strings();
  SSL_library_init();
#endif
  init_complete = true;
  return 0;
}

/**
 * ssl_get_client_cert - Get the client certificate for an SSL connection
 * @param ssldata SSL socket data
 * @param conn    Connection to a server
 */
static void ssl_get_client_cert(struct SslSockData *ssldata, struct Connection *conn)
{
  if (!C_SslClientCert)
    return;

  mutt_debug(LL_DEBUG2, "Using client certificate %s\n", C_SslClientCert);
  SSL_CTX_set_default_passwd_cb_userdata(ssldata->sctx, &conn->account);
  SSL_CTX_set_default_passwd_cb(ssldata->sctx, ssl_passwd_cb);
  SSL_CTX_use_certificate_file(ssldata->sctx, C_SslClientCert, SSL_FILETYPE_PEM);
  SSL_CTX_use_PrivateKey_file(ssldata->sctx, C_SslClientCert, SSL_FILETYPE_PEM);

  /* if we are using a client cert, SASL may expect an external auth name */
  if (mutt_account_getuser(&conn->account) < 0)
    mutt_debug(LL_DEBUG1, "Couldn't get user info\n");
}

/**
 * ssl_socket_close_and_restore - Close an SSL Connection and restore Connection callbacks - Implements Connection::close()
 */
static int ssl_socket_close_and_restore(struct Connection *conn)
{
  int rc = ssl_socket_close(conn);
  conn->read = raw_socket_read;
  conn->write = raw_socket_write;
  conn->close = raw_socket_close;
  conn->poll = raw_socket_poll;

  return rc;
}

/**
 * check_certificate_cache - Is the X509 Certificate in the cache?
 * @param peercert Certificate
 * @retval true Certificate is in the cache
 */
static bool check_certificate_cache(X509 *peercert)
{
  unsigned char peermd[EVP_MAX_MD_SIZE];
  unsigned int peermdlen;
  X509 *cert = NULL;

  if (!X509_digest(peercert, EVP_sha256(), peermd, &peermdlen) || !SslSessionCerts)
  {
    return false;
  }

  for (int i = sk_X509_num(SslSessionCerts); i-- > 0;)
  {
    cert = sk_X509_value(SslSessionCerts, i);
    if (compare_certificates(cert, peercert, peermd, peermdlen))
    {
      return true;
    }
  }

  return false;
}

/**
 * check_certificate_file - Read and check a certificate file
 * @param peercert Certificate
 * @retval true  Certificate is valid
 * @retval false Error, or certificate is invalid
 */
static bool check_certificate_file(X509 *peercert)
{
  unsigned char peermd[EVP_MAX_MD_SIZE];
  unsigned int peermdlen;
  X509 *cert = NULL;
  int pass = false;
  FILE *fp = NULL;

  fp = mutt_file_fopen(C_CertificateFile, "rt");
  if (!fp)
    return false;

  if (!X509_digest(peercert, EVP_sha256(), peermd, &peermdlen))
  {
    mutt_file_fclose(&fp);
    return false;
  }

  while (PEM_read_X509(fp, &cert, NULL, NULL))
  {
    if (compare_certificates(cert, peercert, peermd, peermdlen) &&
        check_certificate_expiration(cert, true))
    {
      pass = true;
      break;
    }
  }
  /* PEM_read_X509 sets an error on eof */
  if (!pass)
    ERR_clear_error();
  X509_free(cert);
  mutt_file_fclose(&fp);

  return pass;
}

/**
 * check_host - Check the host on the certificate
 * @param x509cert Certificate
 * @param hostname Hostname
 * @param err      Buffer for error message
 * @param errlen   Length of buffer
 * @retval 1 Hostname matches the certificate
 * @retval 0 Error
 */
static int check_host(X509 *x509cert, const char *hostname, char *err, size_t errlen)
{
  int rc = 0;
  /* hostname in ASCII format: */
  char *hostname_ascii = NULL;
  /* needed to get the common name: */
  X509_NAME *x509_subject = NULL;
  char *buf = NULL;
  int bufsize;
  /* needed to get the DNS subjectAltNames: */
  STACK_OF(GENERAL_NAME) * subj_alt_names;
  int subj_alt_names_count;
  GENERAL_NAME *subj_alt_name = NULL;
  /* did we find a name matching hostname? */
  bool match_found;

  /* Check if 'hostname' matches the one of the subjectAltName extensions of
   * type DNS or the Common Name (CN). */

#ifdef HAVE_LIBIDN
  if (mutt_idna_to_ascii_lz(hostname, &hostname_ascii, 0) != 0)
  {
    hostname_ascii = mutt_str_dup(hostname);
  }
#else
  hostname_ascii = mutt_str_dup(hostname);
#endif

  /* Try the DNS subjectAltNames. */
  match_found = false;
  subj_alt_names = X509_get_ext_d2i(x509cert, NID_subject_alt_name, NULL, NULL);
  if (subj_alt_names)
  {
    subj_alt_names_count = sk_GENERAL_NAME_num(subj_alt_names);
    for (int i = 0; i < subj_alt_names_count; i++)
    {
      subj_alt_name = sk_GENERAL_NAME_value(subj_alt_names, i);
      if (subj_alt_name->type == GEN_DNS)
      {
        if ((subj_alt_name->d.ia5->length >= 0) &&
            (mutt_str_len((char *) subj_alt_name->d.ia5->data) ==
             (size_t) subj_alt_name->d.ia5->length) &&
            (match_found = hostname_match(hostname_ascii,
                                          (char *) (subj_alt_name->d.ia5->data))))
        {
          break;
        }
      }
    }
    GENERAL_NAMES_free(subj_alt_names);
  }

  if (!match_found)
  {
    /* Try the common name */
    x509_subject = X509_get_subject_name(x509cert);
    if (!x509_subject)
    {
      if (err && errlen)
        mutt_str_copy(err, _("can't get certificate subject"), errlen);
      goto out;
    }

    /* first get the space requirements */
    bufsize = X509_NAME_get_text_by_NID(x509_subject, NID_commonName, NULL, 0);
    if (bufsize == -1)
    {
      if (err && errlen)
        mutt_str_copy(err, _("can't get certificate common name"), errlen);
      goto out;
    }
    bufsize++; /* space for the terminal nul char */
    buf = mutt_mem_malloc((size_t) bufsize);
    if (X509_NAME_get_text_by_NID(x509_subject, NID_commonName, buf, bufsize) == -1)
    {
      if (err && errlen)
        mutt_str_copy(err, _("can't get certificate common name"), errlen);
      goto out;
    }
    /* cast is safe since bufsize is incremented above, so bufsize-1 is always
     * zero or greater.  */
    if (mutt_str_len(buf) == (size_t) bufsize - 1)
    {
      match_found = hostname_match(hostname_ascii, buf);
    }
  }

  if (!match_found)
  {
    if (err && errlen)
      snprintf(err, errlen, _("certificate owner does not match hostname %s"), hostname);
    goto out;
  }

  rc = 1;

out:
  FREE(&buf);
  FREE(&hostname_ascii);

  return rc;
}

/**
 * check_certificate_by_digest - Validate a certificate by its digest
 * @param peercert Certificate
 * @retval true  Certificate is valid
 * @retval false Error
 */
static bool check_certificate_by_digest(X509 *peercert)
{
  return check_certificate_expiration(peercert, false) && check_certificate_file(peercert);
}

/**
 * ssl_cache_trusted_cert - Cache a trusted certificate
 * @param c Certificate
 * @retval >0 Number of elements in the cache
 * @retval  0 Error
 */
static int ssl_cache_trusted_cert(X509 *c)
{
  mutt_debug(LL_DEBUG1, "trusted\n");
  if (!SslSessionCerts)
    SslSessionCerts = sk_X509_new_null();
  return sk_X509_push(SslSessionCerts, X509_dup(c));
}

/**
 * add_cert - Look up certificate info and save it to a list
 * @param title  Title for this block of certificate info
 * @param cert   Certificate
 * @param issuer If true, look up the issuer rather than owner details
 * @param list   List to save info to
 */
static void add_cert(const char *title, X509 *cert, bool issuer, struct ListHead *list)
{
  static const int part[] = {
    NID_commonName,             // CN
    NID_pkcs9_emailAddress,     // Email
    NID_organizationName,       // O
    NID_organizationalUnitName, // OU
    NID_localityName,           // L
    NID_stateOrProvinceName,    // ST
    NID_countryName,            // C
  };

  X509_NAME *x509 = NULL;
  if (issuer)
    x509 = X509_get_issuer_name(cert);
  else
    x509 = X509_get_subject_name(cert);

  // Allocate formatted strings and let the ListHead take ownership
  mutt_list_insert_tail(list, mutt_str_dup(title));

  char *line = NULL;
  char *text = NULL;
  for (size_t i = 0; i < mutt_array_size(part); i++)
  {
    text = x509_get_part(x509, part[i]);
    if (text)
    {
      mutt_str_asprintf(&line, "   %s", text);
      mutt_list_insert_tail(list, line);
    }
  }
}

/**
 * interactive_check_cert - Ask the user if a certificate is valid
 * @param cert         Certificate
 * @param idx          Place of certificate in the chain
 * @param len          Length of the certificate chain
 * @param ssl          SSL state
 * @param allow_always If certificate may be always allowed
 * @retval true  User selected 'skip'
 * @retval false Otherwise
 */
static bool interactive_check_cert(X509 *cert, int idx, size_t len, SSL *ssl, bool allow_always)
{
  char buf[256];
  struct ListHead list = STAILQ_HEAD_INITIALIZER(list);

  add_cert(_("This certificate belongs to:"), cert, false, &list);
  mutt_list_insert_tail(&list, NULL);
  add_cert(_("This certificate was issued by:"), cert, true, &list);

  char *line = NULL;
  mutt_list_insert_tail(&list, NULL);
  mutt_list_insert_tail(&list, mutt_str_dup(_("This certificate is valid")));
  mutt_str_asprintf(&line, _("   from %s"), asn1time_to_string(X509_getm_notBefore(cert)));
  mutt_list_insert_tail(&list, line);
  mutt_str_asprintf(&line, _("     to %s"), asn1time_to_string(X509_getm_notAfter(cert)));
  mutt_list_insert_tail(&list, line);

  mutt_list_insert_tail(&list, NULL);
  buf[0] = '\0';
  x509_fingerprint(buf, sizeof(buf), cert, EVP_sha1);
  mutt_str_asprintf(&line, _("SHA1 Fingerprint: %s"), buf);
  mutt_list_insert_tail(&list, line);
  buf[0] = '\0';
  buf[40] = '\0'; /* Ensure the second printed line is null terminated */
  x509_fingerprint(buf, sizeof(buf), cert, EVP_sha256);
  buf[39] = '\0'; /* Divide into two lines of output */
  mutt_str_asprintf(&line, "%s%s", _("SHA256 Fingerprint: "), buf);
  mutt_list_insert_tail(&list, line);
  mutt_str_asprintf(&line, "%*s%s",
                    (int) mutt_str_len(_("SHA256 Fingerprint: ")), "", buf + 40);
  mutt_list_insert_tail(&list, line);

  bool allow_skip = false;
/* The leaf/host certificate can't be skipped. */
#ifdef HAVE_SSL_PARTIAL_CHAIN
  if ((idx != 0) && C_SslVerifyPartialChains)
    allow_skip = true;
#endif

  char title[256];
  snprintf(title, sizeof(title),
           _("SSL Certificate check (certificate %zu of %zu in chain)"), len - idx, len);

  /* Inside ssl_verify_callback(), this function is guarded by a call to
   * check_certificate_by_digest().  This means if check_certificate_expiration() is
   * true, then check_certificate_file() must be false.  Therefore we don't need
   * to also scan the certificate file here.  */
  allow_always = allow_always && C_CertificateFile &&
                 check_certificate_expiration(cert, true);

  int rc = dlg_verify_cert(title, &list, allow_always, allow_skip);
  if ((rc == 3) && !allow_always)
    rc = 4;

  switch (rc)
  {
    case 1: // Reject
      break;
    case 2: // Once
      SSL_set_ex_data(ssl, SkipModeExDataIndex, NULL);
      ssl_cache_trusted_cert(cert);
      break;
    case 3: // Always
    {
      bool saved = false;
      FILE *fp = mutt_file_fopen(C_CertificateFile, "a");
      if (fp)
      {
        if (PEM_write_X509(fp, cert))
          saved = true;
        mutt_file_fclose(&fp);
      }

      if (saved)
        mutt_message(_("Certificate saved"));
      else
        mutt_error(_("Warning: Couldn't save certificate"));

      SSL_set_ex_data(ssl, SkipModeExDataIndex, NULL);
      ssl_cache_trusted_cert(cert);
      break;
    }
    case 4: // Skip
      SSL_set_ex_data(ssl, SkipModeExDataIndex, &SkipModeExDataIndex);
      break;
  }

  mutt_list_free(&list);
  return (rc > 1);
}

/**
 * ssl_verify_callback - Certificate verification callback
 * @param preverify_ok If true, don't question the user if they skipped verification
 * @param ctx          X509 store context
 * @retval true  Certificate is valid
 * @retval false Error, or Certificate is invalid
 *
 * Called for each certificate in the chain sent by the peer, starting from the
 * root; returning true means that the given certificate is trusted, returning
 * false immediately aborts the SSL connection
 */
static int ssl_verify_callback(int preverify_ok, X509_STORE_CTX *ctx)
{
  char buf[256];

  SSL *ssl = X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
  if (!ssl)
  {
    mutt_debug(LL_DEBUG1,
               "failed to retrieve SSL structure from X509_STORE_CTX\n");
    return false;
  }
  const char *host = SSL_get_ex_data(ssl, HostExDataIndex);
  if (!host)
  {
    mutt_debug(LL_DEBUG1, "failed to retrieve hostname from SSL structure\n");
    return false;
  }

  /* This is true when a previous entry in the certificate chain did
   * not verify and the user manually chose to skip it via the
   * $ssl_verify_partial_chains option.
   * In this case, all following certificates need to be treated as non-verified
   * until one is actually verified.  */
  bool skip_mode = (SSL_get_ex_data(ssl, SkipModeExDataIndex));

  X509 *cert = X509_STORE_CTX_get_current_cert(ctx);
  int pos = X509_STORE_CTX_get_error_depth(ctx);
  size_t len = sk_X509_num(X509_STORE_CTX_get0_chain(ctx));

  mutt_debug(LL_DEBUG1, "checking cert chain entry %s (preverify: %d skipmode: %d)\n",
             X509_NAME_oneline(X509_get_subject_name(cert), buf, sizeof(buf)),
             preverify_ok, skip_mode);

#ifdef HAVE_SSL_PARTIAL_CHAIN
  /* Sometimes, when a certificate is (s)kipped, OpenSSL will pass it
   * a second time with preverify_ok = 1.  Don't show it or the user
   * will think their "s" key is broken.  */
  if (C_SslVerifyPartialChains)
  {
    static int last_pos = 0;
    static X509 *last_cert = NULL;
    if (skip_mode && preverify_ok && (pos == last_pos) && last_cert)
    {
      unsigned char last_cert_md[EVP_MAX_MD_SIZE];
      unsigned int last_cert_mdlen;
      if (X509_digest(last_cert, EVP_sha256(), last_cert_md, &last_cert_mdlen) &&
          compare_certificates(cert, last_cert, last_cert_md, last_cert_mdlen))
      {
        mutt_debug(LL_DEBUG2, "ignoring duplicate skipped certificate\n");
        return true;
      }
    }

    last_pos = pos;
    if (last_cert)
      X509_free(last_cert);
    last_cert = X509_dup(cert);
  }
#endif

  /* check session cache first */
  if (check_certificate_cache(cert))
  {
    mutt_debug(LL_DEBUG2, "using cached certificate\n");
    SSL_set_ex_data(ssl, SkipModeExDataIndex, NULL);
    return true;
  }

  /* check hostname only for the leaf certificate */
  buf[0] = '\0';
  if ((pos == 0) && (C_SslVerifyHost != MUTT_NO))
  {
    if (check_host(cert, host, buf, sizeof(buf)) == 0)
    {
      mutt_error(_("Certificate host check failed: %s"), buf);
      /* we disallow (a)ccept always in the prompt, because it will have no effect
       * for hostname mismatches. */
      return interactive_check_cert(cert, pos, len, ssl, false);
    }
    mutt_debug(LL_DEBUG2, "hostname check passed\n");
  }

  if (!preverify_ok || skip_mode)
  {
    /* automatic check from user's database */
    if (C_CertificateFile && check_certificate_by_digest(cert))
    {
      mutt_debug(LL_DEBUG2, "digest check passed\n");
      SSL_set_ex_data(ssl, SkipModeExDataIndex, NULL);
      return true;
    }

    /* log verification error */
    int err = X509_STORE_CTX_get_error(ctx);
    snprintf(buf, sizeof(buf), "%s (%d)", X509_verify_cert_error_string(err), err);
    mutt_debug(LL_DEBUG2, "X509_verify_cert: %s\n", buf);

    /* prompt user */
    return interactive_check_cert(cert, pos, len, ssl, true);
  }

  return true;
}

/**
 * ssl_negotiate - Attempt to negotiate SSL over the wire
 * @param conn    Connection to a server
 * @param ssldata SSL socket data
 * @retval  0 Success
 * @retval -1 Error
 *
 * After SSL state has been initialized, attempt to negotiate SSL over the
 * wire, including certificate checks.
 */
static int ssl_negotiate(struct Connection *conn, struct SslSockData *ssldata)
{
  int err;
  const char *errmsg = NULL;

  HostExDataIndex = SSL_get_ex_new_index(0, "host", NULL, NULL, NULL);
  if (HostExDataIndex == -1)
  {
    mutt_debug(LL_DEBUG1,
               "#1 failed to get index for application specific data\n");
    return -1;
  }

  if (!SSL_set_ex_data(ssldata->ssl, HostExDataIndex, conn->account.host))
  {
    mutt_debug(LL_DEBUG1, "#2 failed to save hostname in SSL structure\n");
    return -1;
  }

  SkipModeExDataIndex = SSL_get_ex_new_index(0, "skip", NULL, NULL, NULL);
  if (SkipModeExDataIndex == -1)
  {
    mutt_debug(LL_DEBUG1,
               "#3 failed to get index for application specific data\n");
    return -1;
  }

  if (!SSL_set_ex_data(ssldata->ssl, SkipModeExDataIndex, NULL))
  {
    mutt_debug(LL_DEBUG1, "#4 failed to save skip mode in SSL structure\n");
    return -1;
  }

  SSL_set_verify(ssldata->ssl, SSL_VERIFY_PEER, ssl_verify_callback);
  SSL_set_mode(ssldata->ssl, SSL_MODE_AUTO_RETRY);

  if (!SSL_set_tlsext_host_name(ssldata->ssl, conn->account.host))
  {
    /* L10N: This is a warning when trying to set the host name for
     * TLS Server Name Indication (SNI).  This allows the server to present
     * the correct certificate if it supports multiple hosts. */
    mutt_error(_("Warning: unable to set TLS SNI host name"));
  }

  ERR_clear_error();

  err = SSL_connect(ssldata->ssl);
  if (err != 1)
  {
    switch (SSL_get_error(ssldata->ssl, err))
    {
      case SSL_ERROR_SYSCALL:
        errmsg = _("I/O error");
        break;
      case SSL_ERROR_SSL:
        errmsg = ERR_error_string(ERR_get_error(), NULL);
        break;
      default:
        errmsg = _("unknown error");
    }

    mutt_error(_("SSL failed: %s"), errmsg);

    return -1;
  }

  return 0;
}

/**
 * sockdata - Get a Connection's socket data
 * @param conn Connection
 * @retval ptr Socket data
 */
static inline struct SslSockData *sockdata(struct Connection *conn)
{
  return conn->sockdata;
}

/**
 * ssl_setup - Set up SSL on the Connection
 * @param conn Connection
 * @retval  0 Success
 * @retval -1 Failure
 */
static int ssl_setup(struct Connection *conn)
{
  int maxbits = 0;

  conn->sockdata = mutt_mem_calloc(1, sizeof(struct SslSockData));

  sockdata(conn)->sctx = SSL_CTX_new(SSLv23_client_method());
  if (!sockdata(conn)->sctx)
  {
    /* L10N: an SSL context is a data structure returned by the OpenSSL
       function SSL_CTX_new().  In this case it returned NULL: an
       error condition.  */
    mutt_error(_("Unable to create SSL context"));
    ssl_dprint_err_stack();
    goto free_ssldata;
  }

  /* disable SSL protocols as needed */
#ifdef SSL_OP_NO_TLSv1_3
  if (!C_SslUseTlsv13)
    SSL_CTX_set_options(sockdata(conn)->sctx, SSL_OP_NO_TLSv1_3);
#endif

#ifdef SSL_OP_NO_TLSv1_2
  if (!C_SslUseTlsv12)
    SSL_CTX_set_options(sockdata(conn)->sctx, SSL_OP_NO_TLSv1_2);
#endif

#ifdef SSL_OP_NO_TLSv1_1
  if (!C_SslUseTlsv11)
    SSL_CTX_set_options(sockdata(conn)->sctx, SSL_OP_NO_TLSv1_1);
#endif

#ifdef SSL_OP_NO_TLSv1
  if (!C_SslUseTlsv1)
    SSL_CTX_set_options(sockdata(conn)->sctx, SSL_OP_NO_TLSv1);
#endif

  if (!C_SslUseSslv3)
    SSL_CTX_set_options(sockdata(conn)->sctx, SSL_OP_NO_SSLv3);

  if (!C_SslUseSslv2)
    SSL_CTX_set_options(sockdata(conn)->sctx, SSL_OP_NO_SSLv2);

  if (C_SslUsesystemcerts)
  {
    if (!SSL_CTX_set_default_verify_paths(sockdata(conn)->sctx))
    {
      mutt_debug(LL_DEBUG1, "Error setting default verify paths\n");
      goto free_ctx;
    }
  }

  if (C_CertificateFile && !ssl_load_certificates(sockdata(conn)->sctx))
    mutt_debug(LL_DEBUG1, "Error loading trusted certificates\n");

  ssl_get_client_cert(sockdata(conn), conn);

  if (C_SslCiphers)
  {
    SSL_CTX_set_cipher_list(sockdata(conn)->sctx, C_SslCiphers);
  }

  if (ssl_set_verify_partial(sockdata(conn)->sctx))
  {
    mutt_error(_("Warning: error enabling ssl_verify_partial_chains"));
  }

  sockdata(conn)->ssl = SSL_new(sockdata(conn)->sctx);
  SSL_set_fd(sockdata(conn)->ssl, conn->fd);

  if (ssl_negotiate(conn, sockdata(conn)))
    goto free_ssl;

  sockdata(conn)->isopen = 1;
  conn->ssf = SSL_CIPHER_get_bits(SSL_get_current_cipher(sockdata(conn)->ssl), &maxbits);

  return 0;

free_ssl:
  SSL_free(sockdata(conn)->ssl);
  sockdata(conn)->ssl = NULL;
free_ctx:
  SSL_CTX_free(sockdata(conn)->sctx);
  sockdata(conn)->sctx = NULL;
free_ssldata:
  FREE(&conn->sockdata);

  return -1;
}

/**
 * ssl_socket_poll - Check whether a socket read would block - Implements Connection::poll()
 */
static int ssl_socket_poll(struct Connection *conn, time_t wait_secs)
{
  if (!conn)
    return -1;

  if (SSL_has_pending(sockdata(conn)->ssl))
    return 1;

  return raw_socket_poll(conn, wait_secs);
}

/**
 * ssl_socket_open - Open an SSL socket - Implements Connection::open()
 */
static int ssl_socket_open(struct Connection *conn)
{
  if (raw_socket_open(conn) < 0)
    return -1;

  int rc = ssl_setup(conn);
  if (rc)
    raw_socket_close(conn);

  return rc;
}

/**
 * ssl_socket_read - Read data from an SSL socket - Implements Connection::read()
 */
static int ssl_socket_read(struct Connection *conn, char *buf, size_t count)
{
  struct SslSockData *data = sockdata(conn);
  int rc;

  rc = SSL_read(data->ssl, buf, count);
  if ((rc <= 0) || (errno == EINTR))
  {
    if (errno == EINTR)
    {
      rc = -1;
    }
    data->isopen = 0;
    ssl_err(data, rc);
  }

  return rc;
}

/**
 * ssl_socket_write - Write data to an SSL socket - Implements Connection::write()
 */
static int ssl_socket_write(struct Connection *conn, const char *buf, size_t count)
{
  if (!conn || !conn->sockdata || !buf || (count == 0))
    return -1;

  int rc = SSL_write(sockdata(conn)->ssl, buf, count);
  if ((rc <= 0) || (errno == EINTR))
  {
    if (errno == EINTR)
    {
      rc = -1;
    }
    ssl_err(sockdata(conn), rc);
  }

  return rc;
}

/**
 * ssl_socket_close - Close an SSL connection - Implements Connection::close()
 */
static int ssl_socket_close(struct Connection *conn)
{
  struct SslSockData *data = sockdata(conn);

  if (data)
  {
    if (data->isopen && (raw_socket_poll(conn, 0) >= 0))
      SSL_shutdown(data->ssl);

    SSL_free(data->ssl);
    data->ssl = NULL;
    SSL_CTX_free(data->sctx);
    data->sctx = NULL;
    FREE(&conn->sockdata);
  }

  return raw_socket_close(conn);
}

/**
 * mutt_ssl_starttls - Negotiate TLS over an already opened connection
 * @param conn Connection to a server
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_ssl_starttls(struct Connection *conn)
{
  if (ssl_init())
    return -1;

  int rc = ssl_setup(conn);

  /* hmm. watch out if we're starting TLS over any method other than raw. */
  conn->read = ssl_socket_read;
  conn->write = ssl_socket_write;
  conn->close = ssl_socket_close_and_restore;
  conn->poll = ssl_socket_poll;

  return rc;
}

/**
 * mutt_ssl_socket_setup - Set up SSL socket mulitplexor
 * @param conn Connection to a server
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_ssl_socket_setup(struct Connection *conn)
{
  if (ssl_init() < 0)
  {
    conn->open = ssl_socket_open_err;
    return -1;
  }

  conn->open = ssl_socket_open;
  conn->read = ssl_socket_read;
  conn->write = ssl_socket_write;
  conn->poll = ssl_socket_poll;
  conn->close = ssl_socket_close;

  return 0;
}
