/**
 * @file
 * Conn private Module data
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_CONN_MODULE_DATA_H
#define MUTT_CONN_MODULE_DATA_H

#include "config.h"
#ifdef USE_SASL_GNU
#include <gsasl.h>
#endif
#ifdef USE_SASL_CYRUS
#include <sasl/sasl.h>
#endif

/**
 * struct ConnModuleData - Conn private Module data
 */
struct ConnModuleData
{
  struct Notify   *notify;                   ///< Notifications

#ifdef USE_SSL_GNUTLS
  int              protocol_priority[5];     ///< GnuTLS protocol priority
#endif
#ifdef USE_SASL_GNU
  Gsasl           *mutt_gsasl_ctx;           ///< GNU SASL context
#endif
#ifdef USE_SSL_OPENSSL
  int              host_ex_data_index;       ///< OpenSSL host extra data index
  int              skip_mode_ex_data_index;  ///< OpenSSL skip mode extra data index
#endif
#ifdef USE_SASL_CYRUS
  sasl_callback_t *mutt_sasl_callbacks;      ///< Cyrus SASL callbacks
  sasl_secret_t   *secret_ptr;               ///< Cyrus SASL secret
#endif
};

#endif /* MUTT_CONN_MODULE_DATA_H */
