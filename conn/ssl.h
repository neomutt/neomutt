/**
 * @file
 * Handling of SSL encryption
 *
 * @authors
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_CONN_SSL_H
#define MUTT_CONN_SSL_H

#include "config.h"
#include <stdbool.h>
#include "mutt/lib.h"

struct Connection;

#ifdef USE_SSL
/// Array of text making up a Certificate
ARRAY_HEAD(CertArray, const char *);

void cert_array_clear(struct CertArray *carr);

/**
 * struct CertMenuData - Certificate data to use in the Menu
 */
struct CertMenuData
{
  struct CertArray *carr;      ///< Lines of the Certificate
  char *prompt;                ///< Prompt for user, similar to mw_multi_choice
  char *keys;                  ///< Keys used in the prompt
};

int mutt_ssl_socket_setup(struct Connection *conn);
int dlg_certificate(const char *title, struct CertArray *carr, bool allow_always, bool allow_skip);
#else
/**
 * [Dummy] Set up the socket multiplexor
 * @retval -1 Failure
 */
static inline int mutt_ssl_socket_setup(struct Connection *conn)
{
  return -1;
}
#endif

#endif /* MUTT_CONN_SSL_H */
