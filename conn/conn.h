/**
 * @file
 * Connection Library
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

#ifndef _CONN_CONN_H
#define _CONN_CONN_H

#include "account.h"
#include "conn_globals.h"
#include "connection.h"
#ifdef USE_SASL
#include "sasl.h"
#endif
#include "sasl_plain.h"
#include "socket.h"
#ifdef USE_SSL
#include "ssl.h"
#endif
#include "tunnel.h"

#endif /* _CONN_CONN_H */
