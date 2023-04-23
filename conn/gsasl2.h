/**
 * @file
 * GNU SASL authentication support
 *
 * @authors
 * Copyright (C) 2022 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_CONN_GSASL2_H
#define MUTT_CONN_GSASL2_H

#include <gsasl.h>

struct Connection;

void        mutt_gsasl_client_finish(Gsasl_session **sctx);
int         mutt_gsasl_client_new   (struct Connection *conn, const char *mech, Gsasl_session **sctx);
void        mutt_gsasl_done         (void);
const char *mutt_gsasl_get_mech     (const char *requested_mech, const char *server_mechlist);

#endif /* MUTT_CONN_GSASL2_H */
