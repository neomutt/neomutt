/**
 * @file
 * Definition of the Store Module
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
 * @page store_module Definition of the Store Module
 *
 * Definition of the Store Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "module_data.h" // IWYU pragma: keep

/**
 * store_init - Initialise a Module - Implements Module::init()
 */
static bool store_init(struct NeoMutt *n)
{
  // struct StoreModuleData *mod_data = MUTT_MEM_CALLOC(1, struct StoreModuleData);
  // neomutt_set_module_data(n, MODULE_ID_STORE, mod_data);

  return true;
}

/**
 * store_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool store_cleanup(struct NeoMutt *n)
{
  // struct StoreModuleData *mod_data = neomutt_get_module_data(n, MODULE_ID_STORE);
  // ASSERT(mod_data);

  // FREE(&mod_data);
  return true;
}

/**
 * ModuleStore - Module for the Store library
 */
const struct Module ModuleStore = {
  MODULE_ID_STORE,
  "store",
  store_init,
  NULL, // config_define_types
  NULL, // config_define_variables
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  store_cleanup,
};
