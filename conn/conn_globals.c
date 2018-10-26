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

short ConnectTimeout = 0; ///< Config: Timeout for making network connections (-1 to wait indefinitely)

#ifdef USE_SSL
const char *CertificateFile = NULL; ///< Config: File containing trusted certificates
const char *EntropyFile = NULL; ///< Config: (ssl) File/device containing random data to initialise SSL
const char *SslCiphers = NULL;    ///< Config: Ciphers to use when using SSL
const char *SslClientCert = NULL; ///< Config: File containing client certificates
#ifdef USE_SSL_GNUTLS
const char *SslCaCertificatesFile = NULL; ///< Config: File containing trusted CA certificates
short SslMinDhPrimeBits = 0; ///< Config: Minimum keysize for Diffie-Hellman key exchange
#endif
#endif

#ifdef USE_SOCKET
const char *Preconnect = NULL; ///< Config: (socket) External command to run prior to opening a socket
const char *Tunnel = NULL; ///< Config: Shell command to establish a tunnel
#endif

#ifdef USE_SSL
bool SslUseSslv3;    ///< Config: (ssl) INSECURE: Use SSLv3 for authentication
bool SslUseTlsv1;    ///< Config: (ssl) Use TLSv1 for authentication
bool SslUseTlsv11;   ///< Config: (ssl) Use TLSv1.1 for authentication
bool SslUseTlsv12;   ///< Config: (ssl) Use TLSv1.2 for authentication
bool SslVerifyDates; ///< Config: (ssl) Verify the dates on the server certificate
bool SslVerifyHost; ///< Config: (ssl) Verify the server's hostname against the certificate
#endif
