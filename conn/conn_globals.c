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
 *
 * | Global Variable        | NeoMutt Config
 * | :--------------------- | :------------------------
 * | #CertificateFile       | $certificate_file
 * | #ConnectTimeout        | $connect_timeout
 * | #EntropyFile           | $entropy_file
 * | #Preconnect            | $preconnect
 * | #SslCaCertificatesFile | $ssl_ca_certificates_file
 * | #SslCiphers            | $ssl_ciphers
 * | #SslClientCert         | $ssl_client_cert
 * | #SslMinDhPrimeBits     | $ssl_min_dh_prime_bits
 * | #Tunnel                | $tunnel
 */

#include "config.h"

short ConnectTimeout; /**< Config: $connect_timeout */

#ifdef USE_SSL
const char *CertificateFile; /**< Config: $certificate_file */
const char *EntropyFile;     /**< Config: $entropy_file */
const char *SslCiphers;      /**< Config: $ssl_ciphers */
const char *SslClientCert;   /**< Config: $ssl_client_cert */
#ifdef USE_SSL_GNUTLS
const char *SslCaCertificatesFile; /**< Config: $ssl_ca_certificates_file */
short SslMinDhPrimeBits;           /**< Config: $ssl_min_dh_prime_bits */
#endif
#endif

#ifdef USE_SOCKET
const char *Preconnect; /**< Config: $preconnect */
const char *Tunnel;     /**< Config: $tunnel */
#endif                  /* USE_SOCKET */
