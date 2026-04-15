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

/**
 * struct ConnModuleData - Conn private Module data
 */
struct ConnModuleData
{
  int protocol_priority[5]; ///< GnuTLS protocol priority (USE_SSL_GNUTLS)
  void *mutt_gsasl_ctx;     ///< GNU SASL context (Gsasl*, USE_SASL_GNU)
  int host_ex_data_index;   ///< OpenSSL host extra data index (USE_SSL_OPENSSL)
  int skip_mode_ex_data_index; ///< OpenSSL skip mode extra data index (USE_SSL_OPENSSL)
  void *mutt_sasl_callbacks; ///< Cyrus SASL callbacks (sasl_callback_t[], USE_SASL_CYRUS)
  void *secret_ptr;          ///< Cyrus SASL secret (sasl_secret_t*, USE_SASL_CYRUS)
};

#endif /* MUTT_CONN_MODULE_DATA_H */
