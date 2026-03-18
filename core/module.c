/**
 * @file
 * Definition of the Core Module
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
 * @page core_module Definition of the Core Module
 *
 * Definition of the Core Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "module_api.h"
#include "module_data.h"
#include "neomutt.h"

/**
 * core_init - Initialise a Module - Implements Module::init()
 */
static bool core_init(struct NeoMutt *n)
{
  // struct CoreModuleData *md = MUTT_MEM_CALLOC(1, struct CoreModuleData);
  // neomutt_set_module_data(n, MODULE_ID_CORE, md);

  return true;
}

/**
 * core_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool core_cleanup(struct NeoMutt *n)
{
  // struct CoreModuleData *md = neomutt_get_module_data(n, MODULE_ID_CORE);
  // ASSERT(md);

  // FREE(&md);
  return true;
}

/**
 * ModuleCore - Module for the Core library
 */
const struct Module ModuleCore = {
  MODULE_ID_CORE, "core", core_init,
  NULL, // config_define_types
  NULL, // config_define_variables
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  core_cleanup,
};
