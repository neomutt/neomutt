/**
 * @file
 * Handling of GnuTLS encryption
 *
 * @authors
 * Copyright (C) 2001 Marco d'Itri <md@linux.it>
 * Copyright (C) 2001-2004 Andrew McDonald <andrew@mcdonald.org.uk>
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
 * @page conn_gnutls Handling of GnuTLS encryption
 *
 * Handling of GnuTLS encryption
 */

#include "config.h"
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "lib.h"
#include "muttlib.h"
#include "options.h"
#include "ssl.h"

// clang-format off
/* certificate error bitmap values */
#define CERTERR_VALID              0
#define CERTERR_EXPIRED      (1 << 0)
#define CERTERR_NOTYETVALID  (1 << 1)
#define CERTERR_REVOKED      (1 << 2)
#define CERTERR_NOTTRUSTED   (1 << 3)
#define CERTERR_HOSTNAME     (1 << 4)
#define CERTERR_SIGNERNOTCA  (1 << 5)
#define CERTERR_INSECUREALG  (1 << 6)
#define CERTERR_OTHER        (1 << 7)
// clang-format on

const int dialog_row_len = 128;

#define CERT_SEP "-----BEGIN"

#ifndef HAVE_GNUTLS_PRIORITY_SET_DIRECT
/* This array needs to be large enough to hold all the possible values support
 * by NeoMutt.  The initialized values are just placeholders--the array gets
 * overwrriten in tls_negotiate() depending on the $ssl_use_* options.
 *
 * Note: gnutls_protocol_set_priority() was removed in GnuTLS version
 * 3.4 (2015-04).  TLS 1.3 support wasn't added until version 3.6.5.
 * Therefore, no attempt is made to support $ssl_use_tlsv1_3 in this code.
 */
static int protocol_priority[] = { GNUTLS_TLS1_2, GNUTLS_TLS1_1, GNUTLS_TLS1,
                                   GNUTLS_SSL3, 0 };
#endif

/**
 * struct TlsSockData - TLS socket data
 */
struct TlsSockData
{
  gnutls_session_t state;
  gnutls_certificate_credentials_t xcred;
};

/**
 * tls_init - Set up Gnu TLS
 * @retval  0 Success
 * @retval -1 Error
 */
static int tls_init(void)
{
  static bool init_complete = false;
  int err;

  if (init_complete)
    return 0;

  err = gnutls_global_init();
  if (err < 0)
  {
    mutt_error("gnutls_global_init: %s", gnutls_strerror(err));
    return -1;
  }

  init_complete = true;
  return 0;
}

/**
 * tls_verify_peers - Wrapper for gnutls_certificate_verify_peers()
 * @param tlsstate TLS state
 * @param certstat Certificate state, e.g. GNUTLS_CERT_INVALID
 * @retval  0 Success If certstat was set. note: this does not mean success
 * @retval >0 Error
 *
 * Wrapper with sanity-checking.
 *
 * certstat is technically a bitwise-or of gnutls_certificate_status_t values.
 */
static int tls_verify_peers(gnutls_session_t tlsstate, gnutls_certificate_status_t *certstat)
{
  /* gnutls_certificate_verify_peers2() chains to
   * gnutls_x509_trust_list_verify_crt2().  That function's documentation says:
   *
   *   When a certificate chain of cert_list_size with more than one
   *   certificates is provided, the verification status will apply to
   *   the first certificate in the chain that failed
   *   verification. The verification process starts from the end of
   *   the chain(from CA to end certificate). The first certificate
   *   in the chain must be the end-certificate while the rest of the
   *   members may be sorted or not.
   *
   * This is why tls_check_certificate() loops from CA to host in that order,
   * calling the menu, and recalling tls_verify_peers() for each approved
   * cert in the chain.
   */
  int rc = gnutls_certificate_verify_peers2(tlsstate, certstat);

  /* certstat was set */
  if (rc == 0)
    return 0;

  if (rc == GNUTLS_E_NO_CERTIFICATE_FOUND)
    mutt_error(_("Unable to get certificate from peer"));
  else
    mutt_error(_("Certificate verification error (%s)"), gnutls_strerror(rc));

  return rc;
}

/**
 * tls_fingerprint - Create a fingerprint of a TLS Certificate
 * @param algo   Fingerprint algorithm, e.g. GNUTLS_MAC_SHA256
 * @param buf    Buffer for the fingerprint
 * @param buflen Length of the buffer
 * @param data Certificate
 */
static void tls_fingerprint(gnutls_digest_algorithm_t algo, char *buf,
                            size_t buflen, const gnutls_datum_t *data)
{
  unsigned char md[64];
  size_t n;

  n = 64;

  if (gnutls_fingerprint(algo, data, (char *) md, &n) < 0)
  {
    snprintf(buf, buflen, _("[unable to calculate]"));
  }
  else
  {
    for (int i = 0; i < (int) n; i++)
    {
      char ch[8];
      snprintf(ch, 8, "%02X%s", md[i], ((i % 2) ? " " : ""));
      mutt_str_cat(buf, buflen, ch);
    }
    buf[2 * n + n / 2 - 1] = '\0'; /* don't want trailing space */
  }
}

/**
 * tls_check_stored_hostname - Does the hostname match a stored certificate?
 * @param cert     Certificate
 * @param hostname Hostname
 * @retval 1 Hostname match found
 * @retval 0 Error, or no match
 */
static int tls_check_stored_hostname(const gnutls_datum_t *cert, const char *hostname)
{
  char *linestr = NULL;
  size_t linestrsize = 0;

  /* try checking against names stored in stored certs file */
  FILE *fp = mutt_file_fopen(C_CertificateFile, "r");
  if (!fp)
    return 0;

  char buf[80];
  buf[0] = '\0';
  tls_fingerprint(GNUTLS_DIG_MD5, buf, sizeof(buf), cert);
  while ((linestr = mutt_file_read_line(linestr, &linestrsize, fp, NULL, 0)))
  {
    regmatch_t *match = mutt_prex_capture(PREX_GNUTLS_CERT_HOST_HASH, linestr);
    if (match)
    {
      regmatch_t *mhost = &match[PREX_GNUTLS_CERT_HOST_HASH_MATCH_HOST];
      regmatch_t *mhash = &match[PREX_GNUTLS_CERT_HOST_HASH_MATCH_HASH];
      linestr[mutt_regmatch_end(mhost)] = '\0';
      linestr[mutt_regmatch_end(mhash)] = '\0';
      if ((strcmp(linestr + mutt_regmatch_start(mhost), hostname) == 0) &&
          (strcmp(linestr + mutt_regmatch_start(mhash), buf) == 0))
      {
        FREE(&linestr);
        mutt_file_fclose(&fp);
        return 1;
      }
    }
  }

  mutt_file_fclose(&fp);

  /* not found a matching name */
  return 0;
}

/**
 * tls_compare_certificates - Compare certificates against #C_CertificateFile
 * @param peercert Certificate
 * @retval 1 Certificate matches file
 * @retval 0 Error, or no match
 */
static int tls_compare_certificates(const gnutls_datum_t *peercert)
{
  gnutls_datum_t cert = { 0 };
  unsigned char *ptr = NULL;
  gnutls_datum_t b64_data = { 0 };
  unsigned char *b64_data_data = NULL;
  struct stat filestat;

  if (stat(C_CertificateFile, &filestat) == -1)
    return 0;

  b64_data.size = filestat.st_size;
  b64_data_data = mutt_mem_calloc(1, b64_data.size + 1);
  b64_data.data = b64_data_data;

  FILE *fp = mutt_file_fopen(C_CertificateFile, "r");
  if (!fp)
    return 0;

  b64_data.size = fread(b64_data.data, 1, b64_data.size, fp);
  mutt_file_fclose(&fp);

  do
  {
    const int ret = gnutls_pem_base64_decode_alloc(NULL, &b64_data, &cert);
    if (ret != 0)
    {
      FREE(&b64_data_data);
      return 0;
    }

    /* find start of cert, skipping junk */
    ptr = (unsigned char *) strstr((char *) b64_data.data, CERT_SEP);
    if (!ptr)
    {
      gnutls_free(cert.data);
      FREE(&b64_data_data);
      return 0;
    }
    /* find start of next cert */
    ptr = (unsigned char *) strstr((char *) ptr + 1, CERT_SEP);

    b64_data.size = b64_data.size - (ptr - b64_data.data);
    b64_data.data = ptr;

    if (cert.size == peercert->size)
    {
      if (memcmp(cert.data, peercert->data, cert.size) == 0)
      {
        /* match found */
        gnutls_free(cert.data);
        FREE(&b64_data_data);
        return 1;
      }
    }

    gnutls_free(cert.data);
  } while (ptr);

  /* no match found */
  FREE(&b64_data_data);
  return 0;
}

/**
 * tls_check_preauth - Prepare a certificate for authentication
 * @param[in]  certdata  List of GnuTLS certificates
 * @param[in]  certstat  GnuTLS certificate status
 * @param[in]  hostname  Hostname
 * @param[in]  chainidx  Index in the certificate chain
 * @param[out] certerr   Result, e.g. #CERTERR_VALID
 * @param[out] savedcert 1 if certificate has been saved
 * @retval  0 Success
 * @retval -1 Error
 */
static int tls_check_preauth(const gnutls_datum_t *certdata,
                             gnutls_certificate_status_t certstat, const char *hostname,
                             int chainidx, int *certerr, int *savedcert)
{
  gnutls_x509_crt_t cert;

  *certerr = CERTERR_VALID;
  *savedcert = 0;

  if (gnutls_x509_crt_init(&cert) < 0)
  {
    mutt_error(_("Error initialising gnutls certificate data"));
    return -1;
  }

  if (gnutls_x509_crt_import(cert, certdata, GNUTLS_X509_FMT_DER) < 0)
  {
    mutt_error(_("Error processing certificate data"));
    gnutls_x509_crt_deinit(cert);
    return -1;
  }

  /* Note: tls_negotiate() contains a call to
   * gnutls_certificate_set_verify_flags() with a flag disabling
   * GnuTLS checking of the dates.  So certstat shouldn't have the
   * GNUTLS_CERT_EXPIRED and GNUTLS_CERT_NOT_ACTIVATED bits set. */
  if (C_SslVerifyDates != MUTT_NO)
  {
    if (gnutls_x509_crt_get_expiration_time(cert) < mutt_date_epoch())
      *certerr |= CERTERR_EXPIRED;
    if (gnutls_x509_crt_get_activation_time(cert) > mutt_date_epoch())
      *certerr |= CERTERR_NOTYETVALID;
  }

  if ((chainidx == 0) && (C_SslVerifyHost != MUTT_NO) &&
      !gnutls_x509_crt_check_hostname(cert, hostname) &&
      !tls_check_stored_hostname(certdata, hostname))
  {
    *certerr |= CERTERR_HOSTNAME;
  }

  if (certstat & GNUTLS_CERT_REVOKED)
  {
    *certerr |= CERTERR_REVOKED;
    certstat ^= GNUTLS_CERT_REVOKED;
  }

  /* see whether certificate is in our cache (certificates file) */
  if (tls_compare_certificates(certdata))
  {
    *savedcert = 1;

    /* We check above for certs with bad dates or that are revoked.
     * These must be accepted manually each time.  Otherwise, we
     * accept saved certificates as valid. */
    if (*certerr == CERTERR_VALID)
    {
      gnutls_x509_crt_deinit(cert);
      return 0;
    }
  }

  if (certstat & GNUTLS_CERT_INVALID)
  {
    *certerr |= CERTERR_NOTTRUSTED;
    certstat ^= GNUTLS_CERT_INVALID;
  }

  if (certstat & GNUTLS_CERT_SIGNER_NOT_FOUND)
  {
    /* NB: already cleared if cert in cache */
    *certerr |= CERTERR_NOTTRUSTED;
    certstat ^= GNUTLS_CERT_SIGNER_NOT_FOUND;
  }

  if (certstat & GNUTLS_CERT_SIGNER_NOT_CA)
  {
    /* NB: already cleared if cert in cache */
    *certerr |= CERTERR_SIGNERNOTCA;
    certstat ^= GNUTLS_CERT_SIGNER_NOT_CA;
  }

  if (certstat & GNUTLS_CERT_INSECURE_ALGORITHM)
  {
    /* NB: already cleared if cert in cache */
    *certerr |= CERTERR_INSECUREALG;
    certstat ^= GNUTLS_CERT_INSECURE_ALGORITHM;
  }

  /* we've been zeroing the interesting bits in certstat -
   * don't return OK if there are any unhandled bits we don't
   * understand */
  if (certstat != 0)
    *certerr |= CERTERR_OTHER;

  gnutls_x509_crt_deinit(cert);

  if (*certerr == CERTERR_VALID)
    return 0;

  return -1;
}

/**
 * add_cert - Look up certificate info and save it to a list
 * @param title  Title for this block of certificate info
 * @param cert   Certificate
 * @param issuer If true, look up the issuer rather than owner details
 * @param list   List to save info to
 */
static void add_cert(const char *title, gnutls_x509_crt_t cert, bool issuer,
                     struct ListHead *list)
{
  static const char *part[] = {
    GNUTLS_OID_X520_COMMON_NAME,              // CN
    GNUTLS_OID_PKCS9_EMAIL,                   // Email
    GNUTLS_OID_X520_ORGANIZATION_NAME,        // O
    GNUTLS_OID_X520_ORGANIZATIONAL_UNIT_NAME, // OU
    GNUTLS_OID_X520_LOCALITY_NAME,            // L
    GNUTLS_OID_X520_STATE_OR_PROVINCE_NAME,   // ST
    GNUTLS_OID_X520_COUNTRY_NAME,             // C
  };

  char buf[128];
  int rc;

  // Allocate formatted strings and let the ListHead take ownership
  mutt_list_insert_tail(list, mutt_str_dup(title));

  for (size_t i = 0; i < mutt_array_size(part); i++)
  {
    size_t buflen = sizeof(buf);
    if (issuer)
      rc = gnutls_x509_crt_get_issuer_dn_by_oid(cert, part[i], 0, 0, buf, &buflen);
    else
      rc = gnutls_x509_crt_get_dn_by_oid(cert, part[i], 0, 0, buf, &buflen);
    if (rc != 0)
      continue;

    char *line = NULL;
    mutt_str_asprintf(&line, "   %s", buf);
    mutt_list_insert_tail(list, line);
  }
}

/**
 * tls_check_one_certificate - Check a GnuTLS certificate
 * @param certdata List of GnuTLS certificates
 * @param certstat GnuTLS certificate status
 * @param hostname Hostname
 * @param idx      Index into certificate list
 * @param len      Length of certificate list
 * @retval 1 Success
 * @retval 0 Failure
 */
static int tls_check_one_certificate(const gnutls_datum_t *certdata,
                                     gnutls_certificate_status_t certstat,
                                     const char *hostname, int idx, size_t len)
{
  struct ListHead list = STAILQ_HEAD_INITIALIZER(list);
  int certerr, savedcert;
  gnutls_x509_crt_t cert;
  char fpbuf[128];
  time_t t;
  char datestr[30];
  char title[256];
  gnutls_datum_t pemdata = { 0 };

  if (tls_check_preauth(certdata, certstat, hostname, idx, &certerr, &savedcert) == 0)
    return 1;

  /* interactive check from user */
  if (gnutls_x509_crt_init(&cert) < 0)
  {
    mutt_error(_("Error initialising gnutls certificate data"));
    return 0;
  }

  if (gnutls_x509_crt_import(cert, certdata, GNUTLS_X509_FMT_DER) < 0)
  {
    mutt_error(_("Error processing certificate data"));
    gnutls_x509_crt_deinit(cert);
    return 0;
  }

  add_cert(_("This certificate belongs to:"), cert, false, &list);
  mutt_list_insert_tail(&list, NULL);
  add_cert(_("This certificate was issued by:"), cert, true, &list);

  mutt_list_insert_tail(&list, NULL);
  mutt_list_insert_tail(&list, mutt_str_dup(_("This certificate is valid")));

  char *line = NULL;
  t = gnutls_x509_crt_get_activation_time(cert);
  mutt_date_make_tls(datestr, sizeof(datestr), t);
  mutt_str_asprintf(&line, _("   from %s"), datestr);
  mutt_list_insert_tail(&list, line);

  t = gnutls_x509_crt_get_expiration_time(cert);
  mutt_date_make_tls(datestr, sizeof(datestr), t);
  mutt_str_asprintf(&line, _("     to %s"), datestr);
  mutt_list_insert_tail(&list, line);
  mutt_list_insert_tail(&list, NULL);

  fpbuf[0] = '\0';
  tls_fingerprint(GNUTLS_DIG_SHA, fpbuf, sizeof(fpbuf), certdata);
  mutt_str_asprintf(&line, _("SHA1 Fingerprint: %s"), fpbuf);
  mutt_list_insert_tail(&list, line);
  fpbuf[0] = '\0';
  fpbuf[40] = '\0'; /* Ensure the second printed line is null terminated */
  tls_fingerprint(GNUTLS_DIG_SHA256, fpbuf, sizeof(fpbuf), certdata);
  fpbuf[39] = '\0'; /* Divide into two lines of output */
  mutt_str_asprintf(&line, "%s%s", _("SHA256 Fingerprint: "), fpbuf);
  mutt_list_insert_tail(&list, line);
  mutt_str_asprintf(&line, "%*s%s", (int) mutt_str_len(_("SHA256 Fingerprint: ")),
                    "", fpbuf + 40);
  mutt_list_insert_tail(&list, line);

  if (certerr)
    mutt_list_insert_tail(&list, NULL);

  if (certerr & CERTERR_NOTYETVALID)
  {
    mutt_list_insert_tail(
        &list, mutt_str_dup(_("WARNING: Server certificate is not yet valid")));
  }
  if (certerr & CERTERR_EXPIRED)
  {
    mutt_list_insert_tail(
        &list, mutt_str_dup(_("WARNING: Server certificate has expired")));
  }
  if (certerr & CERTERR_REVOKED)
  {
    mutt_list_insert_tail(
        &list, mutt_str_dup(_("WARNING: Server certificate has been revoked")));
  }
  if (certerr & CERTERR_HOSTNAME)
  {
    mutt_list_insert_tail(
        &list,
        mutt_str_dup(_("WARNING: Server hostname does not match certificate")));
  }
  if (certerr & CERTERR_SIGNERNOTCA)
  {
    mutt_list_insert_tail(
        &list,
        mutt_str_dup(_("WARNING: Signer of server certificate is not a CA")));
  }
  if (certerr & CERTERR_INSECUREALG)
  {
    mutt_list_insert_tail(
        &list, mutt_str_dup(_("Warning: Server certificate was signed using "
                              "an insecure algorithm")));
  }

  snprintf(title, sizeof(title),
           _("SSL Certificate check (certificate %zu of %zu in chain)"), len - idx, len);

  const bool allow_always =
      (C_CertificateFile && !savedcert &&
       !(certerr & (CERTERR_EXPIRED | CERTERR_NOTYETVALID | CERTERR_REVOKED)));
  int rc = dlg_verify_cert(title, &list, allow_always, false);
  if (rc == 3) // Accept always
  {
    bool saved = false;
    FILE *fp = mutt_file_fopen(C_CertificateFile, "a");
    if (fp)
    {
      if (certerr & CERTERR_HOSTNAME) // Save hostname if necessary
      {
        fpbuf[0] = '\0';
        tls_fingerprint(GNUTLS_DIG_MD5, fpbuf, sizeof(fpbuf), certdata);
        fprintf(fp, "#H %s %s\n", hostname, fpbuf);
        saved = true;
      }
      if (certerr ^ CERTERR_HOSTNAME) // Save the cert for all other errors
      {
        int rc2 = gnutls_pem_base64_encode_alloc("CERTIFICATE", certdata, &pemdata);
        if (rc2 == 0)
        {
          if (fwrite(pemdata.data, pemdata.size, 1, fp) == 1)
          {
            saved = true;
          }
          gnutls_free(pemdata.data);
        }
      }
      mutt_file_fclose(&fp);
    }
    if (!saved)
      mutt_message(_("Certificate saved"));
    else
      mutt_error(_("Warning: Couldn't save certificate"));
  }

  mutt_list_free(&list);
  gnutls_x509_crt_deinit(cert);
  return (rc > 1);
}

/**
 * tls_check_certificate - Check a connection's certificate
 * @param conn Connection to a server
 * @retval 1 Certificate is valid
 * @retval 0 Error, or certificate is invalid
 */
static int tls_check_certificate(struct Connection *conn)
{
  struct TlsSockData *data = conn->sockdata;
  gnutls_session_t state = data->state;
  const gnutls_datum_t *cert_list = NULL;
  unsigned int cert_list_size = 0;
  gnutls_certificate_status_t certstat;
  int certerr, savedcert, rc = 0;
  int max_preauth_pass = -1;

  /* tls_verify_peers() calls gnutls_certificate_verify_peers2(),
   * which verifies the auth_type is GNUTLS_CRD_CERTIFICATE
   * and that get_certificate_type() for the server is GNUTLS_CRT_X509.
   * If it returns 0, certstat will be set with failure codes for the first
   * cert in the chain(from CA to host) with an error.
   */
  if (tls_verify_peers(state, &certstat) != 0)
    return 0;

  cert_list = gnutls_certificate_get_peers(state, &cert_list_size);
  if (!cert_list)
  {
    mutt_error(_("Unable to get certificate from peer"));
    return 0;
  }

  /* tls_verify_peers doesn't check hostname or expiration, so walk
   * from most specific to least checking these. If we see a saved certificate,
   * its status short-circuits the remaining checks. */
  int preauthrc = 0;
  for (int i = 0; i < cert_list_size; i++)
  {
    rc = tls_check_preauth(&cert_list[i], certstat, conn->account.host, i,
                           &certerr, &savedcert);
    preauthrc += rc;
    if (!preauthrc)
      max_preauth_pass = i;

    if (savedcert)
    {
      if (preauthrc == 0)
        return 1;
      break;
    }
  }

  /* then check interactively, starting from chain root */
  for (int i = cert_list_size - 1; i >= 0; i--)
  {
    rc = tls_check_one_certificate(&cert_list[i], certstat, conn->account.host,
                                   i, cert_list_size);

    /* Stop checking if the menu cert is aborted or rejected. */
    if (rc == 0)
      break;

    /* add signers to trust set, then reverify */
    if (i)
    {
      int rcsettrust = gnutls_certificate_set_x509_trust_mem(
          data->xcred, &cert_list[i], GNUTLS_X509_FMT_DER);
      if (rcsettrust != 1)
        mutt_debug(LL_DEBUG1, "error trusting certificate %d: %d\n", i, rcsettrust);

      if (tls_verify_peers(state, &certstat) != 0)
        return 0;

      /* If the cert chain now verifies, and all lower certs already
       * passed preauth, we are done. */
      if (!certstat && (max_preauth_pass >= (i - 1)))
        return 1;
    }
  }

  return rc;
}

/**
 * tls_get_client_cert - Get the client certificate for a TLS connection
 * @param conn Connection to a server
 *
 * @note This function grabs the CN out of the client cert but appears to do
 *       nothing with it.  It does contain a call to mutt_account_getuser().
 */
static void tls_get_client_cert(struct Connection *conn)
{
  struct TlsSockData *data = conn->sockdata;
  gnutls_x509_crt_t clientcrt;
  char *cn = NULL;
  size_t cnlen = 0;
  int rc;

  /* get our cert CN if we have one */
  const gnutls_datum_t *crtdata = gnutls_certificate_get_ours(data->state);
  if (!crtdata)
    return;

  if (gnutls_x509_crt_init(&clientcrt) < 0)
  {
    mutt_debug(LL_DEBUG1, "Failed to init gnutls crt\n");
    return;
  }

  if (gnutls_x509_crt_import(clientcrt, crtdata, GNUTLS_X509_FMT_DER) < 0)
  {
    mutt_debug(LL_DEBUG1, "Failed to import gnutls client crt\n");
    goto err;
  }

  /* get length of CN, then grab it. */
  rc = gnutls_x509_crt_get_dn_by_oid(clientcrt, GNUTLS_OID_X520_COMMON_NAME, 0,
                                     0, NULL, &cnlen);
  if (((rc >= 0) || (rc == GNUTLS_E_SHORT_MEMORY_BUFFER)) && (cnlen > 0))
  {
    cn = mutt_mem_calloc(1, cnlen);
    if (gnutls_x509_crt_get_dn_by_oid(clientcrt, GNUTLS_OID_X520_COMMON_NAME, 0,
                                      0, cn, &cnlen) < 0)
    {
      goto err;
    }
    mutt_debug(LL_DEBUG2, "client certificate CN: %s\n", cn);

    /* if we are using a client cert, SASL may expect an external auth name */
    if (mutt_account_getuser(&conn->account) < 0)
      mutt_debug(LL_DEBUG1, "Couldn't get user info\n");
  }

err:
  FREE(&cn);
  gnutls_x509_crt_deinit(clientcrt);
}

#ifdef HAVE_GNUTLS_PRIORITY_SET_DIRECT
/**
 * tls_set_priority - Set TLS algorithm priorities
 * @param data TLS socket data
 * @retval  0 Success
 * @retval -1 Error
 */
static int tls_set_priority(struct TlsSockData *data)
{
  size_t nproto = 5;
  int rv = -1;

  struct Buffer *priority = mutt_buffer_pool_get();

  if (C_SslCiphers)
    mutt_buffer_strcpy(priority, C_SslCiphers);
  else
    mutt_buffer_strcpy(priority, "NORMAL");

  if (!C_SslUseTlsv13)
  {
    nproto--;
    mutt_buffer_addstr(priority, ":-VERS-TLS1.3");
  }
  if (!C_SslUseTlsv12)
  {
    nproto--;
    mutt_buffer_addstr(priority, ":-VERS-TLS1.2");
  }
  if (!C_SslUseTlsv11)
  {
    nproto--;
    mutt_buffer_addstr(priority, ":-VERS-TLS1.1");
  }
  if (!C_SslUseTlsv1)
  {
    nproto--;
    mutt_buffer_addstr(priority, ":-VERS-TLS1.0");
  }
  if (!C_SslUseSslv3)
  {
    nproto--;
    mutt_buffer_addstr(priority, ":-VERS-SSL3.0");
  }

  if (nproto == 0)
  {
    mutt_error(_("All available protocols for TLS/SSL connection disabled"));
    goto cleanup;
  }

  int err = gnutls_priority_set_direct(data->state, mutt_b2s(priority), NULL);
  if (err < 0)
  {
    mutt_error("gnutls_priority_set_direct(%s): %s", mutt_b2s(priority),
               gnutls_strerror(err));
    goto cleanup;
  }

  rv = 0;

cleanup:
  mutt_buffer_pool_release(&priority);
  return rv;
}

#else
/**
 * tls_set_priority - Set the priority of various protocols
 * @param data TLS socket data
 * @retval  0 Success
 * @retval -1 Error
 */
static int tls_set_priority(struct TlsSockData *data)
{
  size_t nproto = 0; /* number of tls/ssl protocols */

  if (C_SslUseTlsv12)
    protocol_priority[nproto++] = GNUTLS_TLS1_2;
  if (C_SslUseTlsv11)
    protocol_priority[nproto++] = GNUTLS_TLS1_1;
  if (C_SslUseTlsv1)
    protocol_priority[nproto++] = GNUTLS_TLS1;
  if (C_SslUseSslv3)
    protocol_priority[nproto++] = GNUTLS_SSL3;
  protocol_priority[nproto] = 0;

  if (nproto == 0)
  {
    mutt_error(_("All available protocols for TLS/SSL connection disabled"));
    return -1;
  }

  if (C_SslCiphers)
  {
    mutt_error(
        _("Explicit ciphersuite selection via $ssl_ciphers not supported"));
  }
  if (certerr & CERTERR_INSECUREALG)
  {
    row++;
    strfcpy(menu->dialog[row], _("Warning: Server certificate was signed using an insecure algorithm"),
            dialog_row_len);
  }

  /* We use default priorities (see gnutls documentation),
   * except for protocol version */
  gnutls_set_default_priority(data->state);
  gnutls_protocol_set_priority(data->state, protocol_priority);
  return 0;
}
#endif

/**
 * tls_negotiate - Negotiate TLS connection
 * @param conn Connection to a server
 * @retval  0 Success
 * @retval -1 Error
 *
 * After TLS state has been initialized, attempt to negotiate TLS over the
 * wire, including certificate checks.
 */
static int tls_negotiate(struct Connection *conn)
{
  struct TlsSockData *data = mutt_mem_calloc(1, sizeof(struct TlsSockData));
  conn->sockdata = data;
  int err = gnutls_certificate_allocate_credentials(&data->xcred);
  if (err < 0)
  {
    FREE(&conn->sockdata);
    mutt_error("gnutls_certificate_allocate_credentials: %s", gnutls_strerror(err));
    return -1;
  }

  gnutls_certificate_set_x509_trust_file(data->xcred, C_CertificateFile, GNUTLS_X509_FMT_PEM);
  /* ignore errors, maybe file doesn't exist yet */

  if (C_SslCaCertificatesFile)
  {
    gnutls_certificate_set_x509_trust_file(data->xcred, C_SslCaCertificatesFile,
                                           GNUTLS_X509_FMT_PEM);
  }

  if (C_SslClientCert)
  {
    mutt_debug(LL_DEBUG2, "Using client certificate %s\n", C_SslClientCert);
    gnutls_certificate_set_x509_key_file(data->xcred, C_SslClientCert,
                                         C_SslClientCert, GNUTLS_X509_FMT_PEM);
  }

#ifdef HAVE_DECL_GNUTLS_VERIFY_DISABLE_TIME_CHECKS
  /* disable checking certificate activation/expiration times
   * in gnutls, we do the checks ourselves */
  gnutls_certificate_set_verify_flags(data->xcred, GNUTLS_VERIFY_DISABLE_TIME_CHECKS);
#endif

  err = gnutls_init(&data->state, GNUTLS_CLIENT);
  if (err)
  {
    mutt_error("gnutls_init: %s", gnutls_strerror(err));
    goto fail;
  }

  /* set socket */
  gnutls_transport_set_ptr(data->state, (gnutls_transport_ptr_t)(long) conn->fd);

  if (gnutls_server_name_set(data->state, GNUTLS_NAME_DNS, conn->account.host,
                             mutt_str_len(conn->account.host)))
  {
    mutt_error(_("Warning: unable to set TLS SNI host name"));
  }

  if (tls_set_priority(data) < 0)
  {
    goto fail;
  }

  if (C_SslMinDhPrimeBits > 0)
  {
    gnutls_dh_set_prime_bits(data->state, C_SslMinDhPrimeBits);
  }

  gnutls_credentials_set(data->state, GNUTLS_CRD_CERTIFICATE, data->xcred);

  do
  {
    err = gnutls_handshake(data->state);
  } while ((err == GNUTLS_E_AGAIN) || (err == GNUTLS_E_INTERRUPTED));

  if (err < 0)
  {
    if (err == GNUTLS_E_FATAL_ALERT_RECEIVED)
    {
      mutt_error("gnutls_handshake: %s(%s)", gnutls_strerror(err),
                 gnutls_alert_get_name(gnutls_alert_get(data->state)));
    }
    else
    {
      mutt_error("gnutls_handshake: %s", gnutls_strerror(err));
    }
    goto fail;
  }

  if (tls_check_certificate(conn) == 0)
    goto fail;

  /* set Security Strength Factor (SSF) for SASL */
  /* NB: gnutls_cipher_get_key_size() returns key length in bytes */
  conn->ssf = gnutls_cipher_get_key_size(gnutls_cipher_get(data->state)) * 8;

  tls_get_client_cert(conn);

  if (!OptNoCurses)
  {
    mutt_message(_("SSL/TLS connection using %s (%s/%s/%s)"),
                 gnutls_protocol_get_name(gnutls_protocol_get_version(data->state)),
                 gnutls_kx_get_name(gnutls_kx_get(data->state)),
                 gnutls_cipher_get_name(gnutls_cipher_get(data->state)),
                 gnutls_mac_get_name(gnutls_mac_get(data->state)));
    mutt_sleep(0);
  }

  return 0;

fail:
  gnutls_certificate_free_credentials(data->xcred);
  gnutls_deinit(data->state);
  FREE(&conn->sockdata);
  return -1;
}

/**
 * tls_socket_poll - Check whether a socket read would block - Implements Connection::poll()
 */
static int tls_socket_poll(struct Connection *conn, time_t wait_secs)
{
  struct TlsSockData *data = conn->sockdata;
  if (!data)
    return -1;

  if (gnutls_record_check_pending(data->state))
    return 1;

  return raw_socket_poll(conn, wait_secs);
}

/**
 * tls_socket_close - Close a TLS socket - Implements Connection::close()
 */
static int tls_socket_close(struct Connection *conn)
{
  struct TlsSockData *data = conn->sockdata;
  if (data)
  {
    /* shut down only the write half to avoid hanging waiting for the remote to respond.
     *
     * RFC5246 7.2.1. "Closure Alerts"
     *
     * It is not required for the initiator of the close to wait for the
     * responding close_notify alert before closing the read side of the
     * connection.  */
    gnutls_bye(data->state, GNUTLS_SHUT_WR);

    gnutls_certificate_free_credentials(data->xcred);
    gnutls_deinit(data->state);
    FREE(&conn->sockdata);
  }

  return raw_socket_close(conn);
}

/**
 * tls_socket_open - Open a TLS socket - Implements Connection::open()
 */
static int tls_socket_open(struct Connection *conn)
{
  if (raw_socket_open(conn) < 0)
    return -1;

  if (tls_negotiate(conn) < 0)
  {
    tls_socket_close(conn);
    return -1;
  }

  return 0;
}

/**
 * tls_socket_read - Read data from a TLS socket - Implements Connection::read()
 */
static int tls_socket_read(struct Connection *conn, char *buf, size_t count)
{
  struct TlsSockData *data = conn->sockdata;
  if (!data)
  {
    mutt_error(_("Error: no TLS socket open"));
    return -1;
  }

  int rc;
  do
  {
    rc = gnutls_record_recv(data->state, buf, count);
  } while ((rc == GNUTLS_E_AGAIN) || (rc == GNUTLS_E_INTERRUPTED));

  if (rc < 0)
  {
    mutt_error("tls_socket_read (%s)", gnutls_strerror(rc));
    return -1;
  }

  return rc;
}

/**
 * tls_socket_write - Write data to a TLS socket - Implements Connection::write()
 */
static int tls_socket_write(struct Connection *conn, const char *buf, size_t count)
{
  struct TlsSockData *data = conn->sockdata;
  size_t sent = 0;

  if (!data)
  {
    mutt_error(_("Error: no TLS socket open"));
    return -1;
  }

  do
  {
    int ret;
    do
    {
      ret = gnutls_record_send(data->state, buf + sent, count - sent);
    } while ((ret == GNUTLS_E_AGAIN) || (ret == GNUTLS_E_INTERRUPTED));

    if (ret < 0)
    {
      mutt_error("tls_socket_write (%s)", gnutls_strerror(ret));
      return -1;
    }

    sent += ret;
  } while (sent < count);

  return sent;
}

/**
 * tls_starttls_close - Close a TLS connection - Implements Connection::close()
 */
static int tls_starttls_close(struct Connection *conn)
{
  int rc;

  rc = tls_socket_close(conn);
  conn->read = raw_socket_read;
  conn->write = raw_socket_write;
  conn->close = raw_socket_close;
  conn->poll = raw_socket_poll;

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
  if (tls_init() < 0)
    return -1;

  conn->open = tls_socket_open;
  conn->read = tls_socket_read;
  conn->write = tls_socket_write;
  conn->close = tls_socket_close;
  conn->poll = tls_socket_poll;

  return 0;
}

/**
 * mutt_ssl_starttls - Negotiate TLS over an already opened connection
 * @param conn Connection to a server
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_ssl_starttls(struct Connection *conn)
{
  if (tls_init() < 0)
    return -1;

  if (tls_negotiate(conn) < 0)
    return -1;

  conn->read = tls_socket_read;
  conn->write = tls_socket_write;
  conn->close = tls_starttls_close;
  conn->poll = tls_socket_poll;

  return 0;
}
