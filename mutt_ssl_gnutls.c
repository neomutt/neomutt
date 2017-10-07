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

#include "config.h"
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "lib/lib.h"
#include "mutt.h"
#include "account.h"
#include "globals.h"
#include "keymap.h"
#include "mutt_menu.h"
#include "mutt_regex.h"
#include "mutt_socket.h"
#include "opcodes.h"
#include "options.h"
#include "protos.h"

/* certificate error bitmap values */
#define CERTERR_VALID 0
#define CERTERR_EXPIRED 1
#define CERTERR_NOTYETVALID 2
#define CERTERR_REVOKED 4
#define CERTERR_NOTTRUSTED 8
#define CERTERR_HOSTNAME 16
#define CERTERR_SIGNERNOTCA 32
#define CERTERR_INSECUREALG 64

#define CERT_SEP "-----BEGIN"

/**
 * struct TlsSockData - TLS socket data
 */
struct TlsSockData
{
  gnutls_session_t state;
  gnutls_certificate_credentials_t xcred;
};

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
    mutt_sleep(2);
    return -1;
  }

  init_complete = true;
  return 0;
}

static int tls_socket_read(struct Connection *conn, char *buf, size_t len)
{
  struct TlsSockData *data = conn->sockdata;
  int ret;

  if (!data)
  {
    mutt_error(_("Error: no TLS socket open"));
    mutt_sleep(2);
    return -1;
  }

  do
  {
    ret = gnutls_record_recv(data->state, buf, len);
    if ((ret < 0 && gnutls_error_is_fatal(ret) == 1) || ret == GNUTLS_E_INTERRUPTED)
    {
      mutt_error("tls_socket_read (%s)", gnutls_strerror(ret));
      mutt_sleep(2);
      return -1;
    }
  } while (ret == GNUTLS_E_AGAIN);

  return ret;
}

static int tls_socket_write(struct Connection *conn, const char *buf, size_t len)
{
  struct TlsSockData *data = conn->sockdata;
  int ret;
  size_t sent = 0;

  if (!data)
  {
    mutt_error(_("Error: no TLS socket open"));
    mutt_sleep(2);
    return -1;
  }

  do
  {
    ret = gnutls_record_send(data->state, buf + sent, len - sent);
    if (ret < 0)
    {
      if (gnutls_error_is_fatal(ret) == 1 || ret == GNUTLS_E_INTERRUPTED)
      {
        mutt_error("tls_socket_write (%s)", gnutls_strerror(ret));
        mutt_sleep(4);
        return -1;
      }
      return ret;
    }
    sent += ret;
  } while (sent < len);

  return sent;
}

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
     * connection.
     */
    gnutls_bye(data->state, GNUTLS_SHUT_WR);

    gnutls_certificate_free_credentials(data->xcred);
    gnutls_deinit(data->state);
    FREE(&conn->sockdata);
  }

  return raw_socket_close(conn);
}

static int tls_starttls_close(struct Connection *conn)
{
  int rc;

  rc = tls_socket_close(conn);
  conn->conn_read = raw_socket_read;
  conn->conn_write = raw_socket_write;
  conn->conn_close = raw_socket_close;

  return rc;
}

/**
 * tls_verify_peers - wrapper for gnutls_certificate_verify_peers
 *
 * wrapper with sanity-checking
 */
static gnutls_certificate_status_t tls_verify_peers(gnutls_session_t tlsstate)
{
  int verify_ret;
  unsigned int status;

  verify_ret = gnutls_certificate_verify_peers2(tlsstate, &status);
  if (!verify_ret)
    return status;

  if (status == GNUTLS_E_NO_CERTIFICATE_FOUND)
  {
    mutt_error(_("Unable to get certificate from peer"));
    mutt_sleep(2);
    return 0;
  }
  if (verify_ret < 0)
  {
    mutt_error(_("Certificate verification error (%s)"), gnutls_strerror(status));
    mutt_sleep(2);
    return 0;
  }

  /* We only support X.509 certificates (not OpenPGP) at the moment */
  if (gnutls_certificate_type_get(tlsstate) != GNUTLS_CRT_X509)
  {
    mutt_error(_("Certificate is not X.509"));
    mutt_sleep(2);
    return 0;
  }

  return status;
}

static void tls_fingerprint(gnutls_digest_algorithm_t algo, char *s, int l,
                            const gnutls_datum_t *data)
{
  unsigned char md[36];
  size_t n;

  n = 36;

  if (gnutls_fingerprint(algo, data, (char *) md, &n) < 0)
  {
    snprintf(s, l, _("[unable to calculate]"));
  }
  else
  {
    for (int i = 0; i < (int) n; i++)
    {
      char ch[8];
      snprintf(ch, 8, "%02X%s", md[i], (i % 2 ? " " : ""));
      safe_strcat(s, l, ch);
    }
    s[2 * n + n / 2 - 1] = '\0'; /* don't want trailing space */
  }
}

static int tls_check_stored_hostname(const gnutls_datum_t *cert, const char *hostname)
{
  char buf[80];
  FILE *fp = NULL;
  char *linestr = NULL;
  size_t linestrsize;
  int linenum = 0;
  regex_t preg;
  regmatch_t pmatch[3];

  /* try checking against names stored in stored certs file */
  if ((fp = fopen(CertificateFile, "r")))
  {
    if (REGCOMP(&preg,
                "^#H ([a-zA-Z0-9_\\.-]+) ([0-9A-F]{4}( [0-9A-F]{4}){7})[ \t]*$",
                REG_ICASE) != 0)
    {
      safe_fclose(&fp);
      return 0;
    }

    buf[0] = '\0';
    tls_fingerprint(GNUTLS_DIG_MD5, buf, sizeof(buf), cert);
    while ((linestr = mutt_read_line(linestr, &linestrsize, fp, &linenum, 0)) != NULL)
    {
      if (linestr[0] == '#' && linestr[1] == 'H')
      {
        if (regexec(&preg, linestr, 3, pmatch, 0) == 0)
        {
          linestr[pmatch[1].rm_eo] = '\0';
          linestr[pmatch[2].rm_eo] = '\0';
          if ((strcmp(linestr + pmatch[1].rm_so, hostname) == 0) &&
              (strcmp(linestr + pmatch[2].rm_so, buf) == 0))
          {
            regfree(&preg);
            FREE(&linestr);
            safe_fclose(&fp);
            return 1;
          }
        }
      }
    }

    regfree(&preg);
    safe_fclose(&fp);
  }

  /* not found a matching name */
  return 0;
}

/**
 * tls_compare_certificates - Compare certificates
 */
static int tls_compare_certificates(const gnutls_datum_t *peercert)
{
  gnutls_datum_t cert;
  unsigned char *ptr = NULL;
  FILE *fd1 = NULL;
  int ret;
  gnutls_datum_t b64_data;
  unsigned char *b64_data_data = NULL;
  struct stat filestat;

  if (stat(CertificateFile, &filestat) == -1)
    return 0;

  b64_data.size = filestat.st_size + 1;
  b64_data_data = safe_calloc(1, b64_data.size);
  b64_data_data[b64_data.size - 1] = '\0';
  b64_data.data = b64_data_data;

  fd1 = fopen(CertificateFile, "r");
  if (!fd1)
  {
    return 0;
  }

  b64_data.size = fread(b64_data.data, 1, b64_data.size, fd1);
  safe_fclose(&fd1);

  do
  {
    ret = gnutls_pem_base64_decode_alloc(NULL, &b64_data, &cert);
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
    mutt_sleep(2);
    return -1;
  }

  if (gnutls_x509_crt_import(cert, certdata, GNUTLS_X509_FMT_DER) < 0)
  {
    mutt_error(_("Error processing certificate data"));
    mutt_sleep(2);
    gnutls_x509_crt_deinit(cert);
    return -1;
  }

  if (option(OPT_SSL_VERIFY_DATES) != MUTT_NO)
  {
    if (gnutls_x509_crt_get_expiration_time(cert) < time(NULL))
      *certerr |= CERTERR_EXPIRED;
    if (gnutls_x509_crt_get_activation_time(cert) > time(NULL))
      *certerr |= CERTERR_NOTYETVALID;
  }

  if (chainidx == 0 && option(OPT_SSL_VERIFY_HOST) != MUTT_NO &&
      !gnutls_x509_crt_check_hostname(cert, hostname) &&
      !tls_check_stored_hostname(certdata, hostname))
    *certerr |= CERTERR_HOSTNAME;

  /* see whether certificate is in our cache (certificates file) */
  if (tls_compare_certificates(certdata))
  {
    *savedcert = 1;

    if (chainidx == 0 && (certstat & GNUTLS_CERT_INVALID))
    {
      /* doesn't matter - have decided is valid because server
       certificate is in our trusted cache */
      certstat ^= GNUTLS_CERT_INVALID;
    }

    if (chainidx == 0 && (certstat & GNUTLS_CERT_SIGNER_NOT_FOUND))
    {
      /* doesn't matter that we haven't found the signer, since
       certificate is in our trusted cache */
      certstat ^= GNUTLS_CERT_SIGNER_NOT_FOUND;
    }

    if (chainidx <= 1 && (certstat & GNUTLS_CERT_SIGNER_NOT_CA))
    {
      /* Hmm. Not really sure how to handle this, but let's say
       that we don't care if the CA certificate hasn't got the
       correct X.509 basic constraints if server or first signer
       certificate is in our cache. */
      certstat ^= GNUTLS_CERT_SIGNER_NOT_CA;
    }

    if (chainidx == 0 && (certstat & GNUTLS_CERT_INSECURE_ALGORITHM))
    {
      /* doesn't matter that it was signed using an insecure
         algorithm, since certificate is in our trusted cache */
      certstat ^= GNUTLS_CERT_INSECURE_ALGORITHM;
    }
  }

  if (certstat & GNUTLS_CERT_REVOKED)
  {
    *certerr |= CERTERR_REVOKED;
    certstat ^= GNUTLS_CERT_REVOKED;
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

  gnutls_x509_crt_deinit(cert);

  /* we've been zeroing the interesting bits in certstat -
   don't return OK if there are any unhandled bits we don't
   understand */
  if (*certerr == CERTERR_VALID && certstat == 0)
    return 0;

  return -1;
}

static char *tls_make_date(time_t t, char *s, size_t len)
{
  struct tm *l = gmtime(&t);

  if (l)
    snprintf(s, len, "%s, %d %s %d %02d:%02d:%02d UTC", Weekdays[l->tm_wday], l->tm_mday,
             Months[l->tm_mon], l->tm_year + 1900, l->tm_hour, l->tm_min, l->tm_sec);
  else
    strfcpy(s, _("[invalid date]"), len);

  return s;
}

/**
 * tls_check_one_certificate - Check a GnuTLS certificate
 * @param certdata List of GnuTLS certificates
 * @param certstat GnuTLS certificate status
 * @param hostname Hostname
 * @param idx      Index into certificate list
 * @param len      Length of certificate list
 * @retval 0  on failure
 * @retval >0 on success
 */
static int tls_check_one_certificate(const gnutls_datum_t *certdata,
                                     gnutls_certificate_status_t certstat,
                                     const char *hostname, int idx, int len)
{
  int certerr, savedcert;
  gnutls_x509_crt_t cert;
  char buf[SHORT_STRING];
  char fpbuf[SHORT_STRING];
  size_t buflen;
  char dn_common_name[SHORT_STRING];
  char dn_email[SHORT_STRING];
  char dn_organization[SHORT_STRING];
  char dn_organizational_unit[SHORT_STRING];
  char dn_locality[SHORT_STRING];
  char dn_province[SHORT_STRING];
  char dn_country[SHORT_STRING];
  time_t t;
  char datestr[30];
  struct Menu *menu = NULL;
  char helpstr[LONG_STRING];
  char title[STRING];
  FILE *fp = NULL;
  gnutls_datum_t pemdata;
  int row, done, ret;

  if (!tls_check_preauth(certdata, certstat, hostname, idx, &certerr, &savedcert))
    return 1;

  /* skip signers if insecure algorithm was used */
  if (idx && (certerr & CERTERR_INSECUREALG))
  {
    if (idx == 1)
    {
      mutt_error(_("Warning: Server certificate was signed using an insecure "
                   "algorithm"));
      mutt_sleep(2);
    }
    return 0;
  }

  /* interactive check from user */
  if (gnutls_x509_crt_init(&cert) < 0)
  {
    mutt_error(_("Error initialising gnutls certificate data"));
    mutt_sleep(2);
    return 0;
  }

  if (gnutls_x509_crt_import(cert, certdata, GNUTLS_X509_FMT_DER) < 0)
  {
    mutt_error(_("Error processing certificate data"));
    mutt_sleep(2);
    gnutls_x509_crt_deinit(cert);
    return 0;
  }

  menu = mutt_new_menu(MENU_GENERIC);
  menu->max = 25;
  menu->dialog = safe_calloc(1, menu->max * sizeof(char *));
  for (int i = 0; i < menu->max; i++)
    menu->dialog[i] = safe_calloc(1, SHORT_STRING * sizeof(char));
  mutt_push_current_menu(menu);

  row = 0;
  strfcpy(menu->dialog[row], _("This certificate belongs to:"), SHORT_STRING);
  row++;

  buflen = sizeof(dn_common_name);
  if (gnutls_x509_crt_get_dn_by_oid(cert, GNUTLS_OID_X520_COMMON_NAME, 0, 0,
                                    dn_common_name, &buflen) != 0)
    dn_common_name[0] = '\0';
  buflen = sizeof(dn_email);
  if (gnutls_x509_crt_get_dn_by_oid(cert, GNUTLS_OID_PKCS9_EMAIL, 0, 0, dn_email, &buflen) != 0)
    dn_email[0] = '\0';
  buflen = sizeof(dn_organization);
  if (gnutls_x509_crt_get_dn_by_oid(cert, GNUTLS_OID_X520_ORGANIZATION_NAME, 0,
                                    0, dn_organization, &buflen) != 0)
    dn_organization[0] = '\0';
  buflen = sizeof(dn_organizational_unit);
  if (gnutls_x509_crt_get_dn_by_oid(cert, GNUTLS_OID_X520_ORGANIZATIONAL_UNIT_NAME,
                                    0, 0, dn_organizational_unit, &buflen) != 0)
    dn_organizational_unit[0] = '\0';
  buflen = sizeof(dn_locality);
  if (gnutls_x509_crt_get_dn_by_oid(cert, GNUTLS_OID_X520_LOCALITY_NAME, 0, 0,
                                    dn_locality, &buflen) != 0)
    dn_locality[0] = '\0';
  buflen = sizeof(dn_province);
  if (gnutls_x509_crt_get_dn_by_oid(cert, GNUTLS_OID_X520_STATE_OR_PROVINCE_NAME,
                                    0, 0, dn_province, &buflen) != 0)
    dn_province[0] = '\0';
  buflen = sizeof(dn_country);
  if (gnutls_x509_crt_get_dn_by_oid(cert, GNUTLS_OID_X520_COUNTRY_NAME, 0, 0,
                                    dn_country, &buflen) != 0)
    dn_country[0] = '\0';

  snprintf(menu->dialog[row++], SHORT_STRING, "   %s  %s", dn_common_name, dn_email);
  snprintf(menu->dialog[row++], SHORT_STRING, "   %s", dn_organization);
  snprintf(menu->dialog[row++], SHORT_STRING, "   %s", dn_organizational_unit);
  snprintf(menu->dialog[row++], SHORT_STRING, "   %s  %s  %s", dn_locality,
           dn_province, dn_country);
  row++;

  strfcpy(menu->dialog[row], _("This certificate was issued by:"), SHORT_STRING);
  row++;

  buflen = sizeof(dn_common_name);
  if (gnutls_x509_crt_get_issuer_dn_by_oid(cert, GNUTLS_OID_X520_COMMON_NAME, 0,
                                           0, dn_common_name, &buflen) != 0)
    dn_common_name[0] = '\0';
  buflen = sizeof(dn_email);
  if (gnutls_x509_crt_get_issuer_dn_by_oid(cert, GNUTLS_OID_PKCS9_EMAIL, 0, 0,
                                           dn_email, &buflen) != 0)
    dn_email[0] = '\0';
  buflen = sizeof(dn_organization);
  if (gnutls_x509_crt_get_issuer_dn_by_oid(cert, GNUTLS_OID_X520_ORGANIZATION_NAME,
                                           0, 0, dn_organization, &buflen) != 0)
    dn_organization[0] = '\0';
  buflen = sizeof(dn_organizational_unit);
  if (gnutls_x509_crt_get_issuer_dn_by_oid(cert, GNUTLS_OID_X520_ORGANIZATIONAL_UNIT_NAME,
                                           0, 0, dn_organizational_unit, &buflen) != 0)
    dn_organizational_unit[0] = '\0';
  buflen = sizeof(dn_locality);
  if (gnutls_x509_crt_get_issuer_dn_by_oid(cert, GNUTLS_OID_X520_LOCALITY_NAME,
                                           0, 0, dn_locality, &buflen) != 0)
    dn_locality[0] = '\0';
  buflen = sizeof(dn_province);
  if (gnutls_x509_crt_get_issuer_dn_by_oid(cert, GNUTLS_OID_X520_STATE_OR_PROVINCE_NAME,
                                           0, 0, dn_province, &buflen) != 0)
    dn_province[0] = '\0';
  buflen = sizeof(dn_country);
  if (gnutls_x509_crt_get_issuer_dn_by_oid(cert, GNUTLS_OID_X520_COUNTRY_NAME,
                                           0, 0, dn_country, &buflen) != 0)
    dn_country[0] = '\0';

  snprintf(menu->dialog[row++], SHORT_STRING, "   %s  %s", dn_common_name, dn_email);
  snprintf(menu->dialog[row++], SHORT_STRING, "   %s", dn_organization);
  snprintf(menu->dialog[row++], SHORT_STRING, "   %s", dn_organizational_unit);
  snprintf(menu->dialog[row++], SHORT_STRING, "   %s  %s  %s", dn_locality,
           dn_province, dn_country);
  row++;

  snprintf(menu->dialog[row++], SHORT_STRING, _("This certificate is valid"));

  t = gnutls_x509_crt_get_activation_time(cert);
  snprintf(menu->dialog[row++], SHORT_STRING, _("   from %s"),
           tls_make_date(t, datestr, 30));

  t = gnutls_x509_crt_get_expiration_time(cert);
  snprintf(menu->dialog[row++], SHORT_STRING, _("     to %s"),
           tls_make_date(t, datestr, 30));

  fpbuf[0] = '\0';
  tls_fingerprint(GNUTLS_DIG_SHA, fpbuf, sizeof(fpbuf), certdata);
  snprintf(menu->dialog[row++], SHORT_STRING, _("SHA1 Fingerprint: %s"), fpbuf);
  fpbuf[0] = '\0';
  tls_fingerprint(GNUTLS_DIG_MD5, fpbuf, sizeof(fpbuf), certdata);
  snprintf(menu->dialog[row++], SHORT_STRING, _("MD5 Fingerprint: %s"), fpbuf);

  if (certerr & CERTERR_NOTYETVALID)
  {
    row++;
    strfcpy(menu->dialog[row],
            _("WARNING: Server certificate is not yet valid"), SHORT_STRING);
  }
  if (certerr & CERTERR_EXPIRED)
  {
    row++;
    strfcpy(menu->dialog[row], _("WARNING: Server certificate has expired"), SHORT_STRING);
  }
  if (certerr & CERTERR_REVOKED)
  {
    row++;
    strfcpy(menu->dialog[row],
            _("WARNING: Server certificate has been revoked"), SHORT_STRING);
  }
  if (certerr & CERTERR_HOSTNAME)
  {
    row++;
    strfcpy(menu->dialog[row],
            _("WARNING: Server hostname does not match certificate"), SHORT_STRING);
  }
  if (certerr & CERTERR_SIGNERNOTCA)
  {
    row++;
    strfcpy(menu->dialog[row],
            _("WARNING: Signer of server certificate is not a CA"), SHORT_STRING);
  }

  snprintf(title, sizeof(title),
           _("SSL Certificate check (certificate %d of %d in chain)"), len - idx, len);
  menu->title = title;
  /* certificates with bad dates, or that are revoked, must be
   accepted manually each and every time */
  if (CertificateFile && !savedcert &&
      !(certerr & (CERTERR_EXPIRED | CERTERR_NOTYETVALID | CERTERR_REVOKED)))
  {
    menu->prompt = _("(r)eject, accept (o)nce, (a)ccept always");
    /* L10N:
   * These three letters correspond to the choices in the string:
   * (r)eject, accept (o)nce, (a)ccept always.
   * This is an interactive certificate confirmation prompt for
   * a GNUTLS connection.
   */
    menu->keys = _("roa");
  }
  else
  {
    menu->prompt = _("(r)eject, accept (o)nce");
    /* L10N:
   * These two letters correspond to the choices in the string:
   * (r)eject, accept (o)nce.
   * These is an interactive certificate confirmation prompt for
   * a GNUTLS connection.
   */
    menu->keys = _("ro");
  }

  helpstr[0] = '\0';
  mutt_make_help(buf, sizeof(buf), _("Exit  "), MENU_GENERIC, OP_EXIT);
  safe_strcat(helpstr, sizeof(helpstr), buf);
  mutt_make_help(buf, sizeof(buf), _("Help"), MENU_GENERIC, OP_HELP);
  safe_strcat(helpstr, sizeof(helpstr), buf);
  menu->help = helpstr;

  done = 0;
  set_option(OPT_IGNORE_MACRO_EVENTS);
  while (!done)
  {
    switch (mutt_menu_loop(menu))
    {
      case -1:         /* abort */
      case OP_MAX + 1: /* reject */
      case OP_EXIT:
        done = 1;
        break;
      case OP_MAX + 3: /* accept always */
        done = 0;
        if ((fp = fopen(CertificateFile, "a")))
        {
          /* save hostname if necessary */
          if (certerr & CERTERR_HOSTNAME)
          {
            fprintf(fp, "#H %s %s\n", hostname, fpbuf);
            done = 1;
          }
          if (certerr & CERTERR_NOTTRUSTED)
          {
            done = 0;
            ret = gnutls_pem_base64_encode_alloc("CERTIFICATE", certdata, &pemdata);
            if (ret == 0)
            {
              if (fwrite(pemdata.data, pemdata.size, 1, fp) == 1)
              {
                done = 1;
              }
              gnutls_free(pemdata.data);
            }
          }
          safe_fclose(&fp);
        }
        if (!done)
        {
          mutt_error(_("Warning: Couldn't save certificate"));
          mutt_sleep(2);
        }
        else
        {
          mutt_message(_("Certificate saved"));
          mutt_sleep(0);
        }
      /* fall through */
      case OP_MAX + 2: /* accept once */
        done = 2;
        break;
    }
  }
  unset_option(OPT_IGNORE_MACRO_EVENTS);
  mutt_pop_current_menu(menu);
  mutt_menu_destroy(&menu);
  gnutls_x509_crt_deinit(cert);

  return (done == 2);
}

static int tls_check_certificate(struct Connection *conn)
{
  struct TlsSockData *data = conn->sockdata;
  gnutls_session_t state = data->state;
  const gnutls_datum_t *cert_list = NULL;
  unsigned int cert_list_size = 0;
  gnutls_certificate_status_t certstat;
  int certerr, preauthrc, savedcert, rc = 0;
  int rcpeer = -1; /* the result of tls_check_preauth() on the peer's EE cert */

  if (gnutls_auth_get_type(state) != GNUTLS_CRD_CERTIFICATE)
  {
    mutt_error(_("Unable to get certificate from peer"));
    mutt_sleep(2);
    return 0;
  }

  certstat = tls_verify_peers(state);

  cert_list = gnutls_certificate_get_peers(state, &cert_list_size);
  if (!cert_list)
  {
    mutt_error(_("Unable to get certificate from peer"));
    mutt_sleep(2);
    return 0;
  }

  /* tls_verify_peers doesn't check hostname or expiration, so walk
   * from most specific to least checking these. If we see a saved certificate,
   * its status short-circuits the remaining checks. */
  preauthrc = 0;
  for (int i = 0; i < cert_list_size; i++)
  {
    rc = tls_check_preauth(&cert_list[i], certstat, conn->account.host, i,
                           &certerr, &savedcert);
    preauthrc += rc;
    if (i == 0)
    {
      /* This is the peer's end-entity X.509 certificate.  Stash the result
       * to check later in this function.
       */
      rcpeer = rc;
    }

    if (savedcert)
    {
      if (!preauthrc)
        return 1;
      else
        break;
    }
  }

  /* then check interactively, starting from chain root */
  for (int i = cert_list_size - 1; i >= 0; i--)
  {
    rc = tls_check_one_certificate(&cert_list[i], certstat, conn->account.host,
                                   i, cert_list_size);

    /* add signers to trust set, then reverify */
    if (i && rc)
    {
      rc = gnutls_certificate_set_x509_trust_mem(data->xcred, &cert_list[i], GNUTLS_X509_FMT_DER);
      if (rc != 1)
        mutt_debug(1, "error trusting certificate %d: %d\n", i, rc);

      certstat = tls_verify_peers(state);
      /* If the cert chain now verifies, and the peer's cert was otherwise
       * valid (rcpeer==0), we are done.
       */
      if (!certstat && !rcpeer)
        return 1;
    }
  }

  return rc;
}

static void tls_get_client_cert(struct Connection *conn)
{
  struct TlsSockData *data = conn->sockdata;
  const gnutls_datum_t *crtdata = NULL;
  gnutls_x509_crt_t clientcrt;
  char *dn = NULL;
  char *cn = NULL;
  char *cnend = NULL;
  size_t dnlen;

  /* get our cert CN if we have one */
  crtdata = gnutls_certificate_get_ours(data->state);
  if (!crtdata)
    return;

  if (gnutls_x509_crt_init(&clientcrt) < 0)
  {
    mutt_debug(1, "Failed to init gnutls crt\n");
    return;
  }
  if (gnutls_x509_crt_import(clientcrt, crtdata, GNUTLS_X509_FMT_DER) < 0)
  {
    mutt_debug(1, "Failed to import gnutls client crt\n");
    goto err_crt;
  }
  /* get length of DN */
  dnlen = 0;
  gnutls_x509_crt_get_dn(clientcrt, NULL, &dnlen);
  dn = calloc(1, dnlen);
  if (!dn)
  {
    mutt_debug(1, "could not allocate DN\n");
    goto err_crt;
  }
  gnutls_x509_crt_get_dn(clientcrt, dn, &dnlen);
  mutt_debug(2, "client certificate DN: %s\n", dn);

  /* extract CN to use as external user name */
  cn = strstr(dn, "CN=");
  if (!cn)
  {
    mutt_debug(1, "no CN found in DN\n");
    goto err_dn;
  }
  cn += 3;

  if ((cnend = strstr(dn, ",EMAIL=")))
    *cnend = '\0';

  /* if we are using a client cert, SASL may expect an external auth name */
  mutt_account_getuser(&conn->account);

err_dn:
  FREE(&dn);
err_crt:
  gnutls_x509_crt_deinit(clientcrt);
}

#ifdef HAVE_GNUTLS_PRIORITY_SET_DIRECT
static int tls_set_priority(struct TlsSockData *data)
{
  size_t nproto = 4;
  char *priority = NULL;
  size_t priority_size;
  int err;

  priority_size = SHORT_STRING + mutt_strlen(SslCiphers);
  priority = safe_malloc(priority_size);

  priority[0] = 0;
  if (SslCiphers)
    safe_strcat(priority, priority_size, SslCiphers);
  else
    safe_strcat(priority, priority_size, "NORMAL");

  if (!option(OPT_SSL_USE_TLSV1_2))
  {
    nproto--;
    safe_strcat(priority, priority_size, ":-VERS-TLS1.2");
  }
  if (!option(OPT_SSL_USE_TLSV1_1))
  {
    nproto--;
    safe_strcat(priority, priority_size, ":-VERS-TLS1.1");
  }
  if (!option(OPT_SSL_USE_TLSV1))
  {
    nproto--;
    safe_strcat(priority, priority_size, ":-VERS-TLS1.0");
  }
  if (!option(OPT_SSL_USE_SSLV3))
  {
    nproto--;
    safe_strcat(priority, priority_size, ":-VERS-SSL3.0");
  }

  if (nproto == 0)
  {
    mutt_error(_("All available protocols for TLS/SSL connection disabled"));
    FREE(&priority);
    return -1;
  }

  if ((err = gnutls_priority_set_direct(data->state, priority, NULL)) < 0)
  {
    mutt_error("gnutls_priority_set_direct(%s): %s", priority, gnutls_strerror(err));
    mutt_sleep(2);
    FREE(&priority);
    return -1;
  }

  FREE(&priority);
  return 0;
}
#else
/* This array needs to be large enough to hold all the possible values support
 * by NeoMutt.  The initialized values are just placeholders--the array gets
 * overwritten in tls_negotiate() depending on the $ssl_use_* options.
 */
static int protocol_priority[] = { GNUTLS_TLS1_2, GNUTLS_TLS1_1, GNUTLS_TLS1,
                                   GNUTLS_SSL3, 0 };

static int tls_set_priority(struct TlsSockData *data)
{
  size_t nproto = 0; /* number of tls/ssl protocols */

  if (option(OPT_SSL_USE_TLSV1_2))
    protocol_priority[nproto++] = GNUTLS_TLS1_2;
  if (option(OPT_SSL_USE_TLSV1_1))
    protocol_priority[nproto++] = GNUTLS_TLS1_1;
  if (option(OPT_SSL_USE_TLSV1))
    protocol_priority[nproto++] = GNUTLS_TLS1;
  if (option(OPT_SSL_USE_SSLV3))
    protocol_priority[nproto++] = GNUTLS_SSL3;
  protocol_priority[nproto] = 0;

  if (nproto == 0)
  {
    mutt_error(_("All available protocols for TLS/SSL connection disabled"));
    return -1;
  }

  if (SslCiphers)
  {
    mutt_error(
        _("Explicit ciphersuite selection via $ssl_ciphers not supported"));
    mutt_sleep(2);
  }

  /* We use default priorities (see gnutls documentation),
     except for protocol version */
  gnutls_set_default_priority(data->state);
  gnutls_protocol_set_priority(data->state, protocol_priority);
  return 0;
}
#endif

/**
 * tls_negotiate - Negotiate TLS connection
 *
 * After TLS state has been initialized, attempt to negotiate TLS over the
 * wire, including certificate checks.
 */
static int tls_negotiate(struct Connection *conn)
{
  struct TlsSockData *data = NULL;
  int err;

  data = safe_calloc(1, sizeof(struct TlsSockData));
  conn->sockdata = data;
  err = gnutls_certificate_allocate_credentials(&data->xcred);
  if (err < 0)
  {
    FREE(&conn->sockdata);
    mutt_error("gnutls_certificate_allocate_credentials: %s", gnutls_strerror(err));
    mutt_sleep(2);
    return -1;
  }

  gnutls_certificate_set_x509_trust_file(data->xcred, CertificateFile, GNUTLS_X509_FMT_PEM);
  /* ignore errors, maybe file doesn't exist yet */

  if (SslCaCertificatesFile)
  {
    gnutls_certificate_set_x509_trust_file(data->xcred, SslCaCertificatesFile,
                                           GNUTLS_X509_FMT_PEM);
  }

  if (SslClientCert)
  {
    mutt_debug(2, "Using client certificate %s\n", SslClientCert);
    gnutls_certificate_set_x509_key_file(data->xcred, SslClientCert,
                                         SslClientCert, GNUTLS_X509_FMT_PEM);
  }

#ifdef HAVE_DECL_GNUTLS_VERIFY_DISABLE_TIME_CHECKS
  /* disable checking certificate activation/expiration times
     in gnutls, we do the checks ourselves */
  gnutls_certificate_set_verify_flags(data->xcred, GNUTLS_VERIFY_DISABLE_TIME_CHECKS);
#endif

  if ((err = gnutls_init(&data->state, GNUTLS_CLIENT)))
  {
    mutt_error("gnutls_handshake: %s", gnutls_strerror(err));
    mutt_sleep(2);
    goto fail;
  }

  /* set socket */
  gnutls_transport_set_ptr(data->state, (gnutls_transport_ptr_t)(long) conn->fd);

  if (gnutls_server_name_set(data->state, GNUTLS_NAME_DNS, conn->account.host,
                             mutt_strlen(conn->account.host)))
  {
    mutt_error(_("Warning: unable to set TLS SNI host name"));
    mutt_sleep(1);
  }

  if (tls_set_priority(data) < 0)
  {
    goto fail;
  }

  if (SslMinDhPrimeBits > 0)
  {
    gnutls_dh_set_prime_bits(data->state, SslMinDhPrimeBits);
  }

  /*
  gnutls_set_cred (data->state, GNUTLS_ANON, NULL);
*/

  gnutls_credentials_set(data->state, GNUTLS_CRD_CERTIFICATE, data->xcred);

  err = gnutls_handshake(data->state);

  while (err == GNUTLS_E_AGAIN)
  {
    err = gnutls_handshake(data->state);
  }
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
    mutt_sleep(2);
    goto fail;
  }

  if (!tls_check_certificate(conn))
    goto fail;

  /* set Security Strength Factor (SSF) for SASL */
  /* NB: gnutls_cipher_get_key_size() returns key length in bytes */
  conn->ssf = gnutls_cipher_get_key_size(gnutls_cipher_get(data->state)) * 8;

  tls_get_client_cert(conn);

  if (!option(OPT_NO_CURSES))
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

int mutt_ssl_socket_setup(struct Connection *conn)
{
  if (tls_init() < 0)
    return -1;

  conn->conn_open = tls_socket_open;
  conn->conn_read = tls_socket_read;
  conn->conn_write = tls_socket_write;
  conn->conn_close = tls_socket_close;
  conn->conn_poll = raw_socket_poll;

  return 0;
}

int mutt_ssl_starttls(struct Connection *conn)
{
  if (tls_init() < 0)
    return -1;

  if (tls_negotiate(conn) < 0)
    return -1;

  conn->conn_read = tls_socket_read;
  conn->conn_write = tls_socket_write;
  conn->conn_close = tls_starttls_close;

  return 0;
}
