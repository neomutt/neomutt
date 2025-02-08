/**
 * @file
 * Definition of the Conn Module
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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
 * @page conn_module Definition of the Conn Module
 *
 * Definition of the Conn Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "config/lib.h"
#include "core/lib.h"

extern struct ConfigDef ConnVars[];
extern struct ConfigDef ConnVarsGetaddr[];
extern struct ConfigDef ConnVarsGnutls[];
extern struct ConfigDef ConnVarsOpenssl[];
extern struct ConfigDef ConnVarsPartial[];
extern struct ConfigDef ConnVarsSsl[];

/**
 * conn_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool conn_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  bool rc = true;

#if defined(USE_SSL)
  rc &= cs_register_variables(cs, ConnVarsSsl);
#endif

#if defined(USE_SSL_GNUTLS)
  rc &= cs_register_variables(cs, ConnVarsGnutls);
#endif

#if defined(USE_SSL_OPENSSL)
  rc &= cs_register_variables(cs, ConnVarsOpenssl);
#endif

#if defined(HAVE_SSL_PARTIAL_CHAIN)
  rc &= cs_register_variables(cs, ConnVarsPartial);
#endif

#if defined(HAVE_GETADDRINFO)
  rc &= cs_register_variables(cs, ConnVarsGetaddr);
#endif

  return rc;
}

/**
 * ModuleConn - Module for the Conn library
 */
const struct Module ModuleConn = {
  "conn",
  NULL, // init
  NULL, // config_define_types
  conn_config_define_variables,
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  NULL, // cleanup
  NULL, // mod_data
};
