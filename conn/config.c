/**
 * @file
 * Connection Global Variables
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * @page conn_config Config used by libconn
 *
 * Config used by libconn
 */

#include "config.h"
#include <stddef.h>
#include <config/lib.h>
#include <stdbool.h>
#include "mutt/lib.h"

// clang-format off
short         C_ConnectTimeout;         ///< Config: Timeout for making network connections (-1 to wait indefinitely)
const char *  C_Preconnect;             ///< Config: External command to run prior to opening a socket
const char *  C_Tunnel;                 ///< Config: Shell command to establish a tunnel
bool          C_TunnelIsSecure;         ///< Config: Assume a tunneled connection is secure
#ifdef USE_SSL
const char *  C_CertificateFile;        ///< Config: (ssl) File containing trusted certificates
const char *  C_EntropyFile;            ///< Config: (ssl) File/device containing random data to initialise SSL
const char *  C_SslCiphers;             ///< Config: (ssl) Ciphers to use when using SSL
const char *  C_SslClientCert;          ///< Config: (ssl) File containing client certificates
bool          C_SslForceTls;            ///< Config: (ssl) Require TLS encryption for all connections
unsigned char C_SslStarttls;            ///< Config: (ssl) Use STARTTLS on servers advertising the capability
#ifndef USE_SSL_GNUTLS
bool          C_SslUsesystemcerts;      ///< Config: (ssl) Use CA certificates in the system-wide store
bool          C_SslUseSslv2;            ///< Config: (ssl) INSECURE: Use SSLv2 for authentication
#endif
bool          C_SslUseSslv3;            ///< Config: (ssl) INSECURE: Use SSLv3 for authentication
bool          C_SslUseTlsv1;            ///< Config: (ssl) Use TLSv1 for authentication
bool          C_SslUseTlsv11;           ///< Config: (ssl) Use TLSv1.1 for authentication
bool          C_SslUseTlsv12;           ///< Config: (ssl) Use TLSv1.2 for authentication
bool          C_SslUseTlsv13;           ///< Config: (ssl) Use TLSv1.3 for authentication
bool          C_SslVerifyDates;         ///< Config: (ssl) Verify the dates on the server certificate
bool          C_SslVerifyHost;          ///< Config: (ssl) Verify the server's hostname against the certificate
#if defined(USE_SSL_OPENSSL) && defined(HAVE_SSL_PARTIAL_CHAIN)
bool          C_SslVerifyPartialChains; ///< Config: (ssl) Allow verification using partial certificate chains
#endif
#endif
#ifdef USE_SSL_GNUTLS
const char *  C_SslCaCertificatesFile;  ///< Config: File containing trusted CA certificates
short         C_SslMinDhPrimeBits;      ///< Config: Minimum keysize for Diffie-Hellman key exchange
#endif
#ifdef HAVE_GETADDRINFO
bool          C_UseIpv6;                ///< Config: Lookup IPv6 addresses when making connections
#endif
// clang-format on

// clang-format off
struct ConfigDef ConnVars[] = {
#ifdef USE_SSL
  { "certificate_file",          DT_PATH|DT_PATH_FILE,      &C_CertificateFile,        IP "~/.mutt_certificates" },
#endif
  { "connect_timeout",           DT_NUMBER,                 &C_ConnectTimeout,         30 },
#ifdef USE_SSL_OPENSSL
  { "entropy_file",              DT_PATH|DT_PATH_FILE,      &C_EntropyFile,            0 },
#endif
  { "preconnect",                DT_STRING,                 &C_Preconnect,             0 },
#ifdef USE_SSL
#ifdef USE_SSL_GNUTLS
  { "ssl_ca_certificates_file",  DT_PATH|DT_PATH_FILE,      &C_SslCaCertificatesFile,  0 },
#endif
  { "ssl_ciphers",               DT_STRING,                 &C_SslCiphers,             0 },
  { "ssl_client_cert",           DT_PATH|DT_PATH_FILE,      &C_SslClientCert,          0 },
  { "ssl_force_tls",             DT_BOOL,                   &C_SslForceTls,            false },
#ifdef USE_SSL_GNUTLS
  { "ssl_min_dh_prime_bits",     DT_NUMBER|DT_NOT_NEGATIVE, &C_SslMinDhPrimeBits,      0 },
#endif
  { "ssl_starttls",              DT_QUAD,                   &C_SslStarttls,            MUTT_YES },
#ifdef USE_SSL_OPENSSL
  { "ssl_use_sslv2",             DT_BOOL,                   &C_SslUseSslv2,            false },
#endif
  { "ssl_use_sslv3",             DT_BOOL,                   &C_SslUseSslv3,            false },
  { "ssl_use_tlsv1",             DT_BOOL,                   &C_SslUseTlsv1,            false },
  { "ssl_use_tlsv1_1",           DT_BOOL,                   &C_SslUseTlsv11,           false },
  { "ssl_use_tlsv1_2",           DT_BOOL,                   &C_SslUseTlsv12,           true },
  { "ssl_use_tlsv1_3",           DT_BOOL,                   &C_SslUseTlsv13,           true },
#ifdef USE_SSL_OPENSSL
  { "ssl_usesystemcerts",        DT_BOOL,                   &C_SslUsesystemcerts,      true },
#endif
  { "ssl_verify_dates",          DT_BOOL,                   &C_SslVerifyDates,         true },
  { "ssl_verify_host",           DT_BOOL,                   &C_SslVerifyHost,          true },
#ifdef USE_SSL_OPENSSL
#ifdef HAVE_SSL_PARTIAL_CHAIN
  { "ssl_verify_partial_chains", DT_BOOL,                   &C_SslVerifyPartialChains, false },
#endif
#endif
#endif
  { "tunnel",                    DT_STRING|DT_COMMAND,      &C_Tunnel,                 0 },
  { "tunnel_is_secure",          DT_BOOL,                   &C_TunnelIsSecure,         true },
#ifdef HAVE_GETADDRINFO
  { "use_ipv6",                  DT_BOOL,                   &C_UseIpv6,                true },
#endif
  { NULL, 0, NULL, 0, 0, NULL },
};
// clang-format on

/**
 * config_init_conn - Register conn config variables
 */
bool config_init_conn(struct ConfigSet *cs)
{
  return cs_register_variables(cs, ConnVars, 0);
}
