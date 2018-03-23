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
#include <stdio.h>

short ConnectTimeout = 0; /**< Config: Timeout for a network connection (for IMAP, POP or SMTP) */

#ifdef USE_SSL
const char *CertificateFile = NULL; /**< Config: File containing trusted certificates */
const char *EntropyFile = NULL; /**< Config: File containing random data to initialise SSL */
const char *SslCiphers = NULL; /**< Config: Ciphers to use when using SSL */
const char *SslClientCert = NULL; /**< Config: File containing client certificates */
#ifdef USE_SSL_GNUTLS
const char *SslCaCertificatesFile = NULL; /**< Config: File containing trusted CA certificates */
short SslMinDhPrimeBits = 0; /**< Config: Minimum keysize for Diffie-Hellman key exchange */
#endif
#endif

#ifdef USE_SOCKET
const char *Preconnect = NULL; /**< Config: Shell command to run before making a connection */
const char *Tunnel = NULL; /**< Config: Shell command to establish a tunnel */
#endif
