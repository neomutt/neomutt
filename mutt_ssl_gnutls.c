/* Copyright (C) 2001 Marco d'Itri <md@linux.it>
 * Copyright (C) 2001-2004 Andrew McDonald <andrew@mcdonald.org.uk>
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

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#ifdef HAVE_GNUTLS_OPENSSL_H
#include <gnutls/openssl.h>
#endif

#include "mutt.h"
#include "mutt_socket.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "mutt_ssl.h"
#include "mutt_regex.h"

typedef struct _tlssockdata
{
  gnutls_session state;
  gnutls_certificate_credentials xcred;
}
tlssockdata;

/* local prototypes */
static int tls_socket_read (CONNECTION* conn, char* buf, size_t len);
static int tls_socket_write (CONNECTION* conn, const char* buf, size_t len);
static int tls_socket_open (CONNECTION* conn);
static int tls_socket_close (CONNECTION* conn);
static int tls_starttls_close (CONNECTION* conn);

static int tls_init (void);
static int tls_negotiate (CONNECTION* conn);
static int tls_check_certificate (CONNECTION* conn);


static int tls_init (void)
{
  static unsigned char init_complete = 0;
  int err;

  if (init_complete)
    return 0;

  err = gnutls_global_init();
  if (err < 0)
  {
    mutt_error ("gnutls_global_init: %s", gnutls_strerror(err));
    mutt_sleep (2);
    return -1;
  }

  init_complete = 1;
  return 0;
}

int mutt_ssl_socket_setup (CONNECTION* conn)
{
  if (tls_init() < 0)
    return -1;

  conn->conn_open	= tls_socket_open;
  conn->conn_read	= tls_socket_read;
  conn->conn_write	= tls_socket_write;
  conn->conn_close	= tls_socket_close;
  conn->conn_poll       = raw_socket_poll;

  return 0;
}

static int tls_socket_read (CONNECTION* conn, char* buf, size_t len)
{
  tlssockdata *data = conn->sockdata;
  int ret;

  if (!data)
  {
    mutt_error (_("Error: no TLS socket open"));
    mutt_sleep (2);
    return -1;
  }

  ret = gnutls_record_recv (data->state, buf, len);
  if (gnutls_error_is_fatal(ret) == 1)
  {
    mutt_error ("tls_socket_read (%s)", gnutls_strerror (ret));
    mutt_sleep (4);
    return -1;
  }
  return ret;
}

static int tls_socket_write (CONNECTION* conn, const char* buf, size_t len)
{
  tlssockdata *data = conn->sockdata;
  int ret;

  if (!data)
  {
    mutt_error (_("Error: no TLS socket open"));
    mutt_sleep (2);
    return -1;
  }

  ret = gnutls_record_send (data->state, buf, len);
  if (gnutls_error_is_fatal(ret) == 1)
  {
    mutt_error ("tls_socket_write (%s)", gnutls_strerror (ret));
    mutt_sleep (4);
    return -1;
  }
  return ret;
}

static int tls_socket_open (CONNECTION* conn)
{
  if (raw_socket_open (conn) < 0)
    return -1;

  if (tls_negotiate (conn) < 0)
  {
    tls_socket_close (conn);
    return -1;
  }

  return 0;
}

int mutt_ssl_starttls (CONNECTION* conn)
{
  if (tls_init() < 0)
    return -1;

  if (tls_negotiate (conn) < 0)
    return -1;

  conn->conn_read	= tls_socket_read;
  conn->conn_write	= tls_socket_write;
  conn->conn_close	= tls_starttls_close;

  return 0;
}

static int protocol_priority[] = {GNUTLS_TLS1, GNUTLS_SSL3, 0};

/* tls_negotiate: After TLS state has been initialised, attempt to negotiate
 *   TLS over the wire, including certificate checks. */
static int tls_negotiate (CONNECTION * conn)
{
  tlssockdata *data;
  int err;

  data = (tlssockdata *) safe_calloc (1, sizeof (tlssockdata));
  conn->sockdata = data;
  err = gnutls_certificate_allocate_credentials (&data->xcred);
  if (err < 0)
  {
    FREE(&conn->sockdata);
    mutt_error ("gnutls_certificate_allocate_credentials: %s", gnutls_strerror(err));
    mutt_sleep (2);
    return -1;
  }

  gnutls_certificate_set_x509_trust_file (data->xcred, SslCertFile,
					  GNUTLS_X509_FMT_PEM);
  /* ignore errors, maybe file doesn't exist yet */

  if (SslCACertFile)
  {
    gnutls_certificate_set_x509_trust_file (data->xcred, SslCACertFile,
                                            GNUTLS_X509_FMT_PEM);
  }

/*
  gnutls_set_x509_client_key (data->xcred, "", "");
  gnutls_set_x509_cert_callback (data->xcred, cert_callback);
*/

  gnutls_init(&data->state, GNUTLS_CLIENT);

  /* set socket */
  gnutls_transport_set_ptr (data->state, (gnutls_transport_ptr)conn->fd);

  /* disable TLS/SSL protocols as needed */
  if (!option(OPTTLSV1) && !option(OPTSSLV3))
  {
    mutt_error (_("All available protocols for TLS/SSL connection disabled"));
    goto fail;
  }
  else if (!option(OPTTLSV1))
  {
    protocol_priority[0] = GNUTLS_SSL3;
    protocol_priority[1] = 0;
  }
  else if (!option(OPTSSLV3))
  {
    protocol_priority[0] = GNUTLS_TLS1;
    protocol_priority[1] = 0;
  }
  /*
  else
    use the list set above
  */

  /* We use default priorities (see gnutls documentation),
     except for protocol version */
  gnutls_set_default_priority (data->state);
  gnutls_protocol_set_priority (data->state, protocol_priority);

  if (SslDHPrimeBits > 0)
  {
    gnutls_dh_set_prime_bits (data->state, SslDHPrimeBits);
  }

/*
  gnutls_set_cred (data->state, GNUTLS_ANON, NULL);
*/

  gnutls_credentials_set (data->state, GNUTLS_CRD_CERTIFICATE, data->xcred);

  err = gnutls_handshake(data->state);

  while (err == GNUTLS_E_AGAIN || err == GNUTLS_E_INTERRUPTED)
  {
    err = gnutls_handshake(data->state);
  }
  if (err < 0) {
    if (err == GNUTLS_E_FATAL_ALERT_RECEIVED)
    {
      mutt_error("gnutls_handshake: %s(%s)", gnutls_strerror(err),
		 gnutls_alert_get_name(gnutls_alert_get(data->state)));
    }
    else
    {
      mutt_error("gnutls_handshake: %s", gnutls_strerror(err));
    }
    mutt_sleep (2);
    goto fail;
  }

  if (!tls_check_certificate(conn))
    goto fail;

  /* set Security Strength Factor (SSF) for SASL */
  /* NB: gnutls_cipher_get_key_size() returns key length in bytes */
  conn->ssf = gnutls_cipher_get_key_size (gnutls_cipher_get (data->state)) * 8;

  mutt_message (_("SSL/TLS connection using %s (%s/%s/%s)"),
		gnutls_protocol_get_name (gnutls_protocol_get_version (data->state)),
		gnutls_kx_get_name (gnutls_kx_get (data->state)),
		gnutls_cipher_get_name (gnutls_cipher_get (data->state)),
		gnutls_mac_get_name (gnutls_mac_get (data->state)));
  mutt_sleep (0);

  return 0;

 fail:
  gnutls_certificate_free_credentials (data->xcred);
  gnutls_deinit (data->state);
  FREE(&conn->sockdata);
  return -1;
}

static int tls_socket_close (CONNECTION* conn)
{
  tlssockdata *data = conn->sockdata;
  if (data)
  {
    gnutls_bye (data->state, GNUTLS_SHUT_RDWR);

    gnutls_certificate_free_credentials (data->xcred);
    gnutls_deinit (data->state);
    FREE (&conn->sockdata);
  }

  return raw_socket_close (conn);
}

static int tls_starttls_close (CONNECTION* conn)
{
  int rc;

  rc = tls_socket_close (conn);
  conn->conn_read = raw_socket_read;
  conn->conn_write = raw_socket_write;
  conn->conn_close = raw_socket_close;

  return rc;
}

#define CERT_SEP "-----BEGIN"

/* this bit is based on read_ca_file() in gnutls */
static int tls_compare_certificates (const gnutls_datum *peercert)
{
  gnutls_datum cert;
  unsigned char *ptr;
  FILE *fd1;
  int ret;
  gnutls_datum b64_data;
  unsigned char *b64_data_data;
  struct stat filestat;

  if (stat(SslCertFile, &filestat) == -1)
    return 0;

  b64_data.size = filestat.st_size+1;
  b64_data_data = (unsigned char *) safe_calloc (1, b64_data.size);
  b64_data_data[b64_data.size-1] = '\0';
  b64_data.data = b64_data_data;

  fd1 = fopen(SslCertFile, "r");
  if (fd1 == NULL) {
    return 0;
  }

  b64_data.size = fread(b64_data.data, 1, b64_data.size, fd1);
  fclose(fd1);

  do {
    ret = gnutls_pem_base64_decode_alloc(NULL, &b64_data, &cert);
    if (ret != 0)
    {
      FREE (&b64_data_data);
      return 0;
    }

    ptr = (unsigned char *)strstr((char*)b64_data.data, CERT_SEP) + 1;
    ptr = (unsigned char *)strstr((char*)ptr, CERT_SEP);

    b64_data.size = b64_data.size - (ptr - b64_data.data);
    b64_data.data = ptr;

    if (cert.size == peercert->size)
    {
      if (memcmp (cert.data, peercert->data, cert.size) == 0)
      {
	/* match found */
        gnutls_free(cert.data);
	FREE (&b64_data_data);
	return 1;
      }
    }

    gnutls_free(cert.data);
  } while (ptr != NULL);

  /* no match found */
  FREE (&b64_data_data);
  return 0;
}

static void tls_fingerprint (gnutls_digest_algorithm algo,
                             char* s, int l, const gnutls_datum* data)
{
  unsigned char md[36];
  size_t n;
  int j;

  n = 36;

  if (gnutls_fingerprint (algo, data, (char *)md, &n) < 0)
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
    s[2*n+n/2-1] = '\0'; /* don't want trailing space */
  }
}

static char *tls_make_date (time_t t, char *s, size_t len)
{
  struct tm *l = gmtime (&t);

  if (l)
    snprintf (s, len,  "%s, %d %s %d %02d:%02d:%02d UTC",
	      Weekdays[l->tm_wday], l->tm_mday, Months[l->tm_mon],
	      l->tm_year + 1900, l->tm_hour, l->tm_min, l->tm_sec);
  else
    strfcpy (s, _("[invalid date]"), len);

  return (s);
}

static int tls_check_stored_hostname (const gnutls_datum *cert,
                                      const char *hostname)
{
  char buf[80];
  FILE *fp;
  char *linestr = NULL;
  size_t linestrsize;
  int linenum = 0;
  regex_t preg;
  regmatch_t pmatch[3];

  /* try checking against names stored in stored certs file */
  if ((fp = fopen (SslCertFile, "r")))
  {
    if (regcomp(&preg, "^#H ([a-zA-Z0-9_\\.-]+) ([0-9A-F]{4}( [0-9A-F]{4}){7})[ \t]*$", REG_ICASE|REG_EXTENDED) != 0)
    {
       regfree(&preg);
       return 0;
    }

    buf[0] = '\0';
    tls_fingerprint (GNUTLS_DIG_MD5, buf, sizeof (buf), cert);
    while ((linestr = mutt_read_line(linestr, &linestrsize, fp, &linenum)) != NULL)
    {
      if(linestr[0] == '#' && linestr[1] == 'H')
      {
        if (regexec(&preg, linestr, 3, pmatch, 0) == 0)
        {
          linestr[pmatch[1].rm_eo] = '\0';
          linestr[pmatch[2].rm_eo] = '\0';
          if (strcmp(linestr + pmatch[1].rm_so, hostname) == 0 &&
              strcmp(linestr + pmatch[2].rm_so, buf) == 0)
          {
            regfree(&preg);
            FREE(&linestr);
            fclose(fp);
            return 1;
          }
        }
      }
    }

    regfree(&preg);
    fclose(fp);
  }

  /* not found a matching name */
  return 0;
}

static int tls_check_certificate (CONNECTION* conn)
{
  tlssockdata *data = conn->sockdata;
  gnutls_session state = data->state;
  char helpstr[LONG_STRING];
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
  MUTTMENU *menu;
  int done, row, i, ret;
  FILE *fp;
  time_t t;
  const gnutls_datum *cert_list;
  unsigned int cert_list_size = 0;
  gnutls_certificate_status certstat;
  char datestr[30];
  gnutls_x509_crt cert;
  gnutls_datum pemdata;
  int certerr_expired = 0;
  int certerr_notyetvalid = 0;
  int certerr_hostname = 0;
  int certerr_nottrusted = 0;
  int certerr_revoked = 0;
  int certerr_signernotca = 0;

  if (gnutls_auth_get_type(state) != GNUTLS_CRD_CERTIFICATE)
  {
    mutt_error (_("Unable to get certificate from peer"));
    mutt_sleep (2);
    return 0;
  }

  certstat = gnutls_certificate_verify_peers(state);

  if (certstat == GNUTLS_E_NO_CERTIFICATE_FOUND)
  {
    mutt_error (_("Unable to get certificate from peer"));
    mutt_sleep (2);
    return 0;
  }
  if (certstat < 0)
  {
    mutt_error (_("Certificate verification error (%s)"), gnutls_strerror(certstat));
    mutt_sleep (2);
    return 0;
  }

  /* We only support X.509 certificates (not OpenPGP) at the moment */
  if (gnutls_certificate_type_get(state) != GNUTLS_CRT_X509)
  {
    mutt_error (_("Certificate is not X.509"));
    mutt_sleep (2);
    return 0;
  }

  if (gnutls_x509_crt_init(&cert) < 0)
  {
    mutt_error (_("Error initialising gnutls certificate data"));
    mutt_sleep (2);
    return 0;
  }

  cert_list = gnutls_certificate_get_peers(state, &cert_list_size);
  if (!cert_list)
  {
    mutt_error (_("Unable to get certificate from peer"));
    mutt_sleep (2);
    return 0;
  }

  /* FIXME: Currently only check first certificate in chain. */
  if (gnutls_x509_crt_import(cert, &cert_list[0], GNUTLS_X509_FMT_DER) < 0)
  {
    mutt_error (_("Error processing certificate data"));
    mutt_sleep (2);
    return 0;
  }

  if (gnutls_x509_crt_get_expiration_time(cert) < time(NULL))
  {
    certerr_expired = 1;
  }

  if (gnutls_x509_crt_get_activation_time(cert) > time(NULL))
  {
    certerr_notyetvalid = 1;
  }

  if (!gnutls_x509_crt_check_hostname(cert, conn->account.host) &&
      !tls_check_stored_hostname (&cert_list[0], conn->account.host))
  {
    certerr_hostname = 1;
  }

  /* see whether certificate is in our cache (certificates file) */
  if (tls_compare_certificates (&cert_list[0]))
  {
    if (certstat & GNUTLS_CERT_INVALID)
    {
      /* doesn't matter - have decided is valid because server
         certificate is in our trusted cache */
      certstat ^= GNUTLS_CERT_INVALID;
    }

    if (certstat & GNUTLS_CERT_SIGNER_NOT_FOUND)
    {
      /* doesn't matter that we haven't found the signer, since
         certificate is in our trusted cache */
      certstat ^= GNUTLS_CERT_SIGNER_NOT_FOUND;
    }

    if (certstat & GNUTLS_CERT_SIGNER_NOT_CA)
    {
      /* Hmm. Not really sure how to handle this, but let's say
         that we don't care if the CA certificate hasn't got the
         correct X.509 basic constraints if server certificate is
         in our cache. */
      certstat ^= GNUTLS_CERT_SIGNER_NOT_CA;
    }

  }

  if (certstat & GNUTLS_CERT_REVOKED)
  {
    certerr_revoked = 1;
    certstat ^= GNUTLS_CERT_REVOKED;
  }

  if (certstat & GNUTLS_CERT_INVALID)
  {
    certerr_nottrusted = 1;
    certstat ^= GNUTLS_CERT_INVALID;
  }

  if (certstat & GNUTLS_CERT_SIGNER_NOT_FOUND)
  {
    /* NB: already cleared if cert in cache */
    certerr_nottrusted = 1;
    certstat ^= GNUTLS_CERT_SIGNER_NOT_FOUND;
  }

  if (certstat & GNUTLS_CERT_SIGNER_NOT_CA)
  {
    /* NB: already cleared if cert in cache */
    certerr_signernotca = 1;
    certstat ^= GNUTLS_CERT_SIGNER_NOT_CA;
  }

  /* OK if signed by (or is) a trusted certificate */
  /* we've been zeroing the interesting bits in certstat - 
     don't return OK if there are any unhandled bits we don't
     understand */
  if (!(certerr_expired || certerr_notyetvalid || 
	certerr_hostname || certerr_nottrusted) && certstat == 0)
  {
    gnutls_x509_crt_deinit(cert);
    return 1;
  }


  /* interactive check from user */
  menu = mutt_new_menu ();
  menu->max = 25;
  menu->dialog = (char **) safe_calloc (1, menu->max * sizeof (char *));
  for (i = 0; i < menu->max; i++)
    menu->dialog[i] = (char *) safe_calloc (1, SHORT_STRING * sizeof (char));

  row = 0;
  strfcpy (menu->dialog[row], _("This certificate belongs to:"), SHORT_STRING);
  row++;

  buflen = sizeof(dn_common_name);
  if (gnutls_x509_crt_get_dn_by_oid(cert, GNUTLS_OID_X520_COMMON_NAME, 0, 0,
                                    dn_common_name, &buflen) != 0)
    dn_common_name[0] = '\0';
  buflen = sizeof(dn_email);
  if (gnutls_x509_crt_get_dn_by_oid(cert, GNUTLS_OID_PKCS9_EMAIL, 0, 0,
                                    dn_email, &buflen) != 0)
    dn_email[0] = '\0';
  buflen = sizeof(dn_organization);
  if (gnutls_x509_crt_get_dn_by_oid(cert, GNUTLS_OID_X520_ORGANIZATION_NAME, 0, 0,
                                    dn_organization, &buflen) != 0)
    dn_organization[0] = '\0';
  buflen = sizeof(dn_organizational_unit);
  if (gnutls_x509_crt_get_dn_by_oid(cert, GNUTLS_OID_X520_ORGANIZATIONAL_UNIT_NAME, 0, 0,
                                    dn_organizational_unit, &buflen) != 0)
    dn_organizational_unit[0] = '\0';
  buflen = sizeof(dn_locality);
  if (gnutls_x509_crt_get_dn_by_oid(cert, GNUTLS_OID_X520_LOCALITY_NAME, 0, 0,
                                    dn_locality, &buflen) != 0)
    dn_locality[0] = '\0';
  buflen = sizeof(dn_province);
  if (gnutls_x509_crt_get_dn_by_oid(cert, GNUTLS_OID_X520_STATE_OR_PROVINCE_NAME, 0, 0,
                                    dn_province, &buflen) != 0)
    dn_province[0] = '\0';
  buflen = sizeof(dn_country);
  if (gnutls_x509_crt_get_dn_by_oid(cert, GNUTLS_OID_X520_COUNTRY_NAME, 0, 0,
                                    dn_country, &buflen) != 0)
    dn_country[0] = '\0';

  snprintf (menu->dialog[row++], SHORT_STRING, "   %s  %s", dn_common_name, dn_email);
  snprintf (menu->dialog[row++], SHORT_STRING, "   %s", dn_organization);
  snprintf (menu->dialog[row++], SHORT_STRING, "   %s", dn_organizational_unit);
  snprintf (menu->dialog[row++], SHORT_STRING, "   %s  %s  %s",
            dn_locality, dn_province, dn_country);
  row++;
  
  strfcpy (menu->dialog[row], _("This certificate was issued by:"), SHORT_STRING);
  row++;

  buflen = sizeof(dn_common_name);
  if (gnutls_x509_crt_get_issuer_dn_by_oid(cert, GNUTLS_OID_X520_COMMON_NAME, 0, 0,
                                    dn_common_name, &buflen) != 0)
    dn_common_name[0] = '\0';
  buflen = sizeof(dn_email);
  if (gnutls_x509_crt_get_issuer_dn_by_oid(cert, GNUTLS_OID_PKCS9_EMAIL, 0, 0,
                                    dn_email, &buflen) != 0)
    dn_email[0] = '\0';
  buflen = sizeof(dn_organization);
  if (gnutls_x509_crt_get_issuer_dn_by_oid(cert, GNUTLS_OID_X520_ORGANIZATION_NAME, 0, 0,
                                    dn_organization, &buflen) != 0)
    dn_organization[0] = '\0';
  buflen = sizeof(dn_organizational_unit);
  if (gnutls_x509_crt_get_issuer_dn_by_oid(cert, GNUTLS_OID_X520_ORGANIZATIONAL_UNIT_NAME, 0, 0,
                                    dn_organizational_unit, &buflen) != 0)
    dn_organizational_unit[0] = '\0';
  buflen = sizeof(dn_locality);
  if (gnutls_x509_crt_get_issuer_dn_by_oid(cert, GNUTLS_OID_X520_LOCALITY_NAME, 0, 0,
                                    dn_locality, &buflen) != 0)
    dn_locality[0] = '\0';
  buflen = sizeof(dn_province);
  if (gnutls_x509_crt_get_issuer_dn_by_oid(cert, GNUTLS_OID_X520_STATE_OR_PROVINCE_NAME, 0, 0,
                                    dn_province, &buflen) != 0)
    dn_province[0] = '\0';
  buflen = sizeof(dn_country);
  if (gnutls_x509_crt_get_issuer_dn_by_oid(cert, GNUTLS_OID_X520_COUNTRY_NAME, 0, 0,
                                    dn_country, &buflen) != 0)
    dn_country[0] = '\0';

  snprintf (menu->dialog[row++], SHORT_STRING, "   %s  %s", dn_common_name, dn_email);
  snprintf (menu->dialog[row++], SHORT_STRING, "   %s", dn_organization);
  snprintf (menu->dialog[row++], SHORT_STRING, "   %s", dn_organizational_unit);
  snprintf (menu->dialog[row++], SHORT_STRING, "   %s  %s  %s",
            dn_locality, dn_province, dn_country);
  row++;

  snprintf (menu->dialog[row++], SHORT_STRING, _("This certificate is valid"));

  t = gnutls_x509_crt_get_activation_time(cert);
  snprintf (menu->dialog[row++], SHORT_STRING, _("   from %s"), 
	    tls_make_date(t, datestr, 30));

  t = gnutls_x509_crt_get_expiration_time(cert);
  snprintf (menu->dialog[row++], SHORT_STRING, _("     to %s"), 
	    tls_make_date(t, datestr, 30));

  fpbuf[0] = '\0';
  tls_fingerprint (GNUTLS_DIG_SHA, fpbuf, sizeof (fpbuf), &cert_list[0]);
  snprintf (menu->dialog[row++], SHORT_STRING, _("SHA1 Fingerprint: %s"), fpbuf);
  fpbuf[0] = '\0';
  tls_fingerprint (GNUTLS_DIG_MD5, fpbuf, sizeof (fpbuf), &cert_list[0]);
  snprintf (menu->dialog[row++], SHORT_STRING, _("MD5 Fingerprint: %s"), fpbuf);

  if (certerr_notyetvalid)
  {
    row++;
    strfcpy (menu->dialog[row], _("WARNING: Server certificate is not yet valid"), SHORT_STRING);
  }
  if (certerr_expired)
  {
    row++;
    strfcpy (menu->dialog[row], _("WARNING: Server certificate has expired"), SHORT_STRING);
  }
  if (certerr_revoked)
  {
    row++;
    strfcpy (menu->dialog[row], _("WARNING: Server certificate has been revoked"), SHORT_STRING);
  }
  if (certerr_hostname)
  {
    row++;
    strfcpy (menu->dialog[row], _("WARNING: Server hostname does not match certificate"), SHORT_STRING);
  }
  if (certerr_signernotca)
  {
    row++;
    strfcpy (menu->dialog[row], _("WARNING: Signer of server certificate is not a CA"), SHORT_STRING);
  }

  menu->title = _("TLS/SSL Certificate check");
  /* certificates with bad dates, or that are revoked, must be
     accepted manually each and every time */
  if (SslCertFile && !certerr_expired && !certerr_notyetvalid && !certerr_revoked)
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
  set_option (OPTUNBUFFEREDINPUT);
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
	  /* save hostname if necessary */
	  if (certerr_hostname)
	  {
	    fprintf(fp, "#H %s %s\n", conn->account.host, fpbuf);
	    done = 1;
	  }
	  if (certerr_nottrusted)
	  {
            done = 0;
	    ret = gnutls_pem_base64_encode_alloc ("CERTIFICATE", &cert_list[0],
                                                  &pemdata);
	    if (ret == 0)
	    {
	      if (fwrite(pemdata.data, pemdata.size, 1, fp) == 1)
	      {
		done = 1;
	      }
              gnutls_free(pemdata.data);
	    }
	  }
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
  unset_option (OPTUNBUFFEREDINPUT);
  mutt_menuDestroy (&menu);
  gnutls_x509_crt_deinit(cert);
  return (done == 2);
}
