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

#ifndef MUTT_CONN_GLOBALS_H
#define MUTT_CONN_GLOBALS_H

#include <stdbool.h>

/* These variables are backing for config items */
extern short       C_ConnectTimeout;

#ifdef USE_SSL
extern const char *C_CertificateFile;
extern const char *C_EntropyFile;
extern const char *C_SslCiphers;
extern const char *C_SslClientCert;
extern bool        C_SslUseSslv3;
extern bool        C_SslUseTlsv11;
extern bool        C_SslUseTlsv12;
extern bool        C_SslUseTlsv13;
extern bool        C_SslUseTlsv1;
extern bool        C_SslVerifyDates;
extern bool        C_SslVerifyHost;
#endif

#ifdef USE_SSL_GNUTLS
extern const char *C_SslCaCertificatesFile;
extern short       C_SslMinDhPrimeBits;
#endif

#ifdef USE_SOCKET
extern const char *C_Preconnect;
extern const char *C_Tunnel;
extern bool C_TunnelIsSecure;
#endif

#ifdef HAVE_GETADDRINFO
extern bool        C_UseIpv6;
#endif

#endif /* MUTT_CONN_GLOBALS_H */
