/**
 * @file
 * Definition of the Pop Module
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
 * @page pop_module Definition of the Pop Module
 *
 * Definition of the Pop Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "module_data.h"

extern struct ConfigDef PopVars[];

/**
 * pop_init - Initialise a Module - Implements Module::init()
 */
static bool pop_init(struct NeoMutt *n)
{
  // struct PopModuleData *mod_data = MUTT_MEM_CALLOC(1, struct PopModuleData);
  // neomutt_set_module_data(n, MODULE_ID_POP, mod_data);

  return true;
}

/**
 * pop_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool pop_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  return cs_register_variables(cs, PopVars);
}

/**
 * pop_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool pop_cleanup(struct NeoMutt *n)
{
  // struct PopModuleData *mod_data = neomutt_get_module_data(n, MODULE_ID_POP);
  // ASSERT(mod_data);

  // FREE(&mod_data);
  return true;
}

/**
 * ModulePop - Module for the Pop library
 */
const struct Module ModulePop = {
  MODULE_ID_POP,
  "pop",
  pop_init,
  NULL, // config_define_types
  pop_config_define_variables,
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  pop_cleanup,
};
