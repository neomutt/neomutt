/**
 * @file
 * Connection Global Variables
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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
 * @page conn_globals Connection Global Variables
 *
 * These global variables are private to the connection library.
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>

// clang-format off
short       C_ConnectTimeout = 0;           ///< Config: Timeout for making network connections (-1 to wait indefinitely)

#ifdef USE_SSL
const char *C_CertificateFile = NULL;       ///< Config: File containing trusted certificates
const char *C_EntropyFile = NULL;           ///< Config: (ssl) File/device containing random data to initialise SSL
const char *C_SslCiphers = NULL;            ///< Config: Ciphers to use when using SSL
const char *C_SslClientCert = NULL;         ///< Config: File containing client certificates
bool        C_SslUseSslv3;                  ///< Config: (ssl) INSECURE: Use SSLv3 for authentication
bool        C_SslUseTlsv1;                  ///< Config: (ssl) Use TLSv1 for authentication
bool        C_SslUseTlsv11;                 ///< Config: (ssl) Use TLSv1.1 for authentication
bool        C_SslUseTlsv12;                 ///< Config: (ssl) Use TLSv1.2 for authentication
bool        C_SslUseTlsv13;                 ///< Config: (ssl) Use TLSv1.3 for authentication
bool        C_SslVerifyDates;               ///< Config: (ssl) Verify the dates on the server certificate
bool        C_SslVerifyHost;                ///< Config: (ssl) Verify the server's hostname against the certificate
#endif

#ifdef USE_SSL_GNUTLS
const char *C_SslCaCertificatesFile = NULL; ///< Config: File containing trusted CA certificates
short       C_SslMinDhPrimeBits = 0;        ///< Config: Minimum keysize for Diffie-Hellman key exchange
#endif

#ifdef USE_SOCKET
const char *C_Preconnect = NULL;            ///< Config: (socket) External command to run prior to opening a socket
const char *C_Tunnel = NULL;                ///< Config: Shell command to establish a tunnel
bool C_TunnelIsSecure = true;               ///< Config: Assume a tunneled connection is secure
#endif
// clang-format on
