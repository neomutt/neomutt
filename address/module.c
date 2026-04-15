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
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "module_data.h"

extern const struct ConfigSetType CstAddress;
#if defined(HAVE_LIBIDN)
extern struct ConfigDef AddressVarsIdn[];
#endif

/**
 * address_init - Initialise a Module - Implements Module::init()
 */
static bool address_init(struct NeoMutt *n)
{
  // struct AddressModuleData *mod_data = MUTT_MEM_CALLOC(1, struct AddressModuleData);
  // neomutt_set_module_data(n, MODULE_ID_ADDRESS, mod_data);

  return true;
}

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
 * address_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool address_cleanup(struct NeoMutt *n)
{
  // struct AddressModuleData *mod_data = neomutt_get_module_data(n, MODULE_ID_ADDRESS);
  // ASSERT(mod_data);

  // FREE(&mod_data);
  return true;
}

/**
 * ModuleAddress - Module for the Address library
 */
const struct Module ModuleAddress = {
  MODULE_ID_ADDRESS,
  "address",
  address_init,
  address_config_define_types,
  address_config_define_variables,
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  address_cleanup,
};
