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

static struct ConfigDef ConnVars[] = {
// clang-format off
#ifdef USE_SSL
  { "certificate_file", DT_PATH|DT_PATH_FILE, IP "~/.mutt_certificates", 0, NULL,
    "File containing trusted certificates"
  },
#endif
  { "connect_timeout", DT_NUMBER, 30, 0, NULL,
    "Timeout for making network connections (-1 to wait indefinitely)"
  },
#ifdef USE_SSL_OPENSSL
  { "entropy_file", DT_PATH|DT_PATH_FILE, 0, 0, NULL,
    "(ssl) File/device containing random data to initialise SSL"
  },
#endif
  { "preconnect", DT_STRING, 0, 0, NULL,
    "(socket) External command to run prior to opening a socket"
  },
#ifdef USE_SSL
#ifdef USE_SSL_GNUTLS
  { "ssl_ca_certificates_file", DT_PATH|DT_PATH_FILE, 0, 0, NULL,
    "File containing trusted CA certificates"
  },
#endif
  { "ssl_ciphers", DT_STRING, 0, 0, NULL,
    "Ciphers to use when using SSL"
  },
  { "ssl_client_cert", DT_PATH|DT_PATH_FILE, 0, 0, NULL,
    "File containing client certificates"
  },
  { "ssl_force_tls", DT_BOOL, false, 0, NULL,
    "(ssl) Require TLS encryption for all connections"
  },
#ifdef USE_SSL_GNUTLS
  { "ssl_min_dh_prime_bits", DT_NUMBER|DT_NOT_NEGATIVE, 0, 0, NULL,
    "Minimum keysize for Diffie-Hellman key exchange"
  },
#endif
  { "ssl_starttls", DT_QUAD, MUTT_YES, 0, NULL,
    "(ssl) Use STARTTLS on servers advertising the capability"
  },
#ifdef USE_SSL_OPENSSL
  { "ssl_use_sslv2", DT_BOOL, false, 0, NULL,
    "(ssl) INSECURE: Use SSLv2 for authentication"
  },
#endif
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
#ifdef USE_SSL_OPENSSL
  { "ssl_use_system_certs", DT_BOOL, true, 0, NULL,
    "(ssl) Use CA certificates in the system-wide store"
  },
#endif
  { "ssl_verify_dates", DT_BOOL, true, 0, NULL,
    "(ssl) Verify the dates on the server certificate"
  },
  { "ssl_verify_host", DT_BOOL, true, 0, NULL,
    "(ssl) Verify the server's hostname against the certificate"
  },
#ifdef USE_SSL_OPENSSL
#ifdef HAVE_SSL_PARTIAL_CHAIN
  { "ssl_verify_partial_chains", DT_BOOL, false, 0, NULL,
    "(ssl) Allow verification using partial certificate chains"
  },
#endif
#endif
#endif
  { "tunnel", DT_STRING|DT_COMMAND, 0, 0, NULL,
    "Shell command to establish a tunnel"
  },
  { "tunnel_is_secure", DT_BOOL, true, 0, NULL,
    "Assume a tunneled connection is secure"
  },
#ifdef HAVE_GETADDRINFO
  { "use_ipv6", DT_BOOL, true, 0, NULL,
    "Lookup IPv6 addresses when making connections"
  },
#endif

  { "ssl_usesystemcerts",        DT_SYNONYM, IP "ssl_use_system_certs",       },

  { NULL },
  // clang-format on
};

/**
 * config_init_conn - Register conn config variables - Implements ::module_init_config_t
 */
bool config_init_conn(struct ConfigSet *cs)
{
  return cs_register_variables(cs, ConnVars, 0);
}
