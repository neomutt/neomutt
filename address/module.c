/**
 * @file
 * Definition of the Address Module
 *
 * @authors
 * Copyright (C) 2025-2026 Richard Russon <rich@flatcap.org>
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
 * @page address_module Definition of the Address Module
 *
 * Definition of the Address Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "config/lib.h"
#include "core/lib.h"

extern const struct ConfigSetType CstAddress;
#if defined(HAVE_LIBIDN)
extern struct ConfigDef AddressVarsIdn[];
#endif

/**
 * address_config_define_types - Set up Config Types - Implements Module::config_define_types()
 */
static bool address_config_define_types(struct NeoMutt *n, struct ConfigSet *cs)
{
  return cs_register_type(cs, &CstAddress);
}

/**
 * address_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool address_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  bool rc = true;

#if defined(HAVE_LIBIDN)
  rc &= cs_register_variables(cs, AddressVarsIdn);
#endif

  return rc;
}

/**
 * ModuleAddress - Module for the Address library
 */
const struct Module ModuleAddress = {
  MODULE_ID_ADDRESS,
  "address",
  NULL, // init
  address_config_define_types,
  address_config_define_variables,
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  NULL, // cleanup
};
