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

#ifndef _CONN_GLOBALS_H
#define _CONN_GLOBALS_H

#include <stdbool.h>

/* These variables are backing for config items */
extern short ConnectTimeout;

#ifdef USE_SSL
extern const char *CertificateFile;
extern const char *EntropyFile;
extern const char *SslCiphers;
extern const char *SslClientCert;
#ifdef USE_SSL_GNUTLS
extern const char *SslCaCertificatesFile;
extern short SslMinDhPrimeBits;
#endif
#endif

#ifdef USE_SOCKET
extern const char *Preconnect;
extern const char *Tunnel;
#endif

/* These Config Variables are only used in conn/conn_raw.c */
extern bool UseIpv6;

/* These Config Variables are only used in conn/ssl_gnutls.c */
extern bool SslUseSslv3;
extern bool SslUseTlsv1;
extern bool SslUseTlsv11;
extern bool SslUseTlsv12;
extern bool SslVerifyDates;
extern bool SslVerifyHost;

#endif /* _CONN_GLOBALS_H */
