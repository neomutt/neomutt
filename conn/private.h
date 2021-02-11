/**
 * @file
 * Shared functions that are private to Connections
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

#ifndef MUTT_CONN_PRIVATE_H
#define MUTT_CONN_PRIVATE_H

#include <stddef.h>
#include <stdbool.h>
#include <time.h>

struct Connection;

extern const char *  C_CertificateFile;
extern short         C_ConnectTimeout;
extern const char *  C_EntropyFile;
extern const char *  C_Preconnect;
extern const char *  C_SslCaCertificatesFile;
extern const char *  C_SslCiphers;
extern const char *  C_SslClientCert;
extern short         C_SslMinDhPrimeBits;
extern bool          C_SslUseSslv2;
extern bool          C_SslUseSslv3;
extern bool          C_SslUseTlsv11;
extern bool          C_SslUseTlsv12;
extern bool          C_SslUseTlsv13;
extern bool          C_SslUseTlsv1;
extern bool          C_SslUseSystemCerts;
extern bool          C_SslVerifyDates;
extern bool          C_SslVerifyHost;
extern bool          C_SslVerifyPartialChains;
extern bool          C_TunnelIsSecure;
extern bool          C_UseIpv6;

int raw_socket_close(struct Connection *conn);
int raw_socket_open (struct Connection *conn);
int raw_socket_poll (struct Connection *conn, time_t wait_secs);
int raw_socket_read (struct Connection *conn, char *buf, size_t len);
int raw_socket_write(struct Connection *conn, const char *buf, size_t count);

void mutt_tunnel_socket_setup(struct Connection *conn);

#endif /* MUTT_CONN_PRIVATE_H */
