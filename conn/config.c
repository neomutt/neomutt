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

/**
 * ConnVars - Config definitions for the connection library
 */
static struct ConfigDef ConnVars[] = {
  // clang-format off
  { "account_command", DT_STRING|DT_COMMAND, 0, 0, NULL,
    "Shell command to retrieve account credentials"
  },
  { "preconnect", DT_STRING, 0, 0, NULL,
    "(socket) External command to run prior to opening a socket"
  },
  { "socket_timeout", DT_NUMBER, 30, 0, NULL,
    "Timeout for socket connect/read/write operations (-1 to wait indefinitely)"
  },
  { "tunnel", DT_STRING|DT_COMMAND, 0, 0, NULL,
    "Shell command to establish a tunnel"
  },
  { "tunnel_is_secure", DT_BOOL, true, 0, NULL,
    "Assume a tunneled connection is secure"
  },

  { "connect_timeout", DT_SYNONYM, IP "socket_timeout", IP "2023-02-15" },
  { NULL },
  // clang-format on
};

#if defined(USE_SSL)
/**
 * ConnVarsSsl - General SSL Config definitions for the conn library
 */
static struct ConfigDef ConnVarsSsl[] = {
  // clang-format off
  { "certificate_file", DT_PATH|DT_PATH_FILE, IP "~/.mutt_certificates", 0, NULL,
    "File containing trusted certificates"
  },
  { "ssl_ciphers", DT_STRING, 0, 0, NULL,
    "Ciphers to use when using SSL"
  },
  { "ssl_client_cert", DT_PATH|DT_PATH_FILE, 0, 0, NULL,
    "File containing client certificates"
  },
  { "ssl_force_tls", DT_BOOL, true, 0, NULL,
    "(ssl) Require TLS encryption for all connections"
  },
  { "ssl_starttls", DT_QUAD, MUTT_YES, 0, NULL,
    "(ssl) Use STARTTLS on servers advertising the capability"
  },
  { "ssl_use_sslv3", DT_BOOL, false, 0, NULL,
    "(ssl) INSECURE: Use SSLv3 for authentication"
  },
  { "ssl_use_tlsv1", DT_BOOL, false, 0, NULL,
    "(ssl) Use TLSv1 for authentication"
  },
  { "ssl_use_tlsv1_1", DT_BOOL, false, 0, NULL,
    "(ssl) Use TLSv1.1 for authentication"
  },
  { "ssl_use_tlsv1_2", DT_BOOL, true, 0, NULL,
    "(ssl) Use TLSv1.2 for authentication"
  },
  { "ssl_use_tlsv1_3", DT_BOOL, true, 0, NULL,
    "(ssl) Use TLSv1.3 for authentication"
  },
  { "ssl_verify_dates", DT_BOOL, true, 0, NULL,
    "(ssl) Verify the dates on the server certificate"
  },
  { "ssl_verify_host", DT_BOOL, true, 0, NULL,
    "(ssl) Verify the server's hostname against the certificate"
  },
  { NULL },
  // clang-format on
};
#endif

#if defined(USE_SSL_GNUTLS)
/**
 * ConnVarsGnutls - GnuTLS Config definitions for the connection library
 */
static struct ConfigDef ConnVarsGnutls[] = {
  // clang-format off
  { "ssl_ca_certificates_file", DT_PATH|DT_PATH_FILE, 0, 0, NULL,
    "File containing trusted CA certificates"
  },
  { "ssl_min_dh_prime_bits", DT_NUMBER|DT_NOT_NEGATIVE, 0, 0, NULL,
    "Minimum keysize for Diffie-Hellman key exchange"
  },
  { NULL },
  // clang-format on
};
#endif

#if defined(USE_SSL_OPENSSL)
/**
 * ConnVarsOpenssl - OpenSSL Config definitions for the connection library
 */
static struct ConfigDef ConnVarsOpenssl[] = {
  // clang-format off
  { "entropy_file", DT_PATH|DT_PATH_FILE, 0, 0, NULL,
    "(ssl) File/device containing random data to initialise SSL"
  },
  { "ssl_use_sslv2", DT_BOOL, false, 0, NULL,
    "(ssl) INSECURE: Use SSLv2 for authentication"
  },
  { "ssl_use_system_certs", DT_BOOL, true, 0, NULL,
    "(ssl) Use CA certificates in the system-wide store"
  },
  { "ssl_usesystemcerts", DT_SYNONYM, IP "ssl_use_system_certs", IP "2021-02-11" },
  { NULL },
  // clang-format on
};
#endif

#if defined(HAVE_SSL_PARTIAL_CHAIN)
/**
 * ConnVarsPartial - SSL partial chains Config definitions for the connection library
 */
static struct ConfigDef ConnVarsPartial[] = {
  // clang-format off
  { "ssl_verify_partial_chains", DT_BOOL, false, 0, NULL,
    "(ssl) Allow verification using partial certificate chains"
  },
  { NULL },
  // clang-format on
};
#endif

#if defined(HAVE_GETADDRINFO)
/**
 * ConnVarsGetaddr - GetAddrInfo Config definitions for the connection library
 */
static struct ConfigDef ConnVarsGetaddr[] = {
  // clang-format off
  { "use_ipv6", DT_BOOL, true, 0, NULL,
    "Lookup IPv6 addresses when making connections"
  },
  { NULL },
  // clang-format on
};
#endif

/**
 * config_init_conn - Register conn config variables - Implements ::module_init_config_t - @ingroup cfg_module_api
 */
bool config_init_conn(struct ConfigSet *cs)
{
  bool rc = cs_register_variables(cs, ConnVars, 0);

#if defined(USE_SSL)
  rc |= cs_register_variables(cs, ConnVarsSsl, 0);
#endif

#if defined(USE_SSL_GNUTLS)
  rc |= cs_register_variables(cs, ConnVarsGnutls, 0);
#endif

#if defined(USE_SSL_OPENSSL)
  rc |= cs_register_variables(cs, ConnVarsOpenssl, 0);
#endif

#if defined(HAVE_SSL_PARTIAL_CHAIN)
  rc |= cs_register_variables(cs, ConnVarsPartial, 0);
#endif

#if defined(HAVE_GETADDRINFO)
  rc |= cs_register_variables(cs, ConnVarsGetaddr, 0);
#endif

  return rc;
}
