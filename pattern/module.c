/**
 * @file
 * Definition of the Pattern Module
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
 * @page pattern_module Definition of the Pattern Module
 *
 * Definition of the Pattern Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "module_data.h"

extern struct ConfigDef PatternVars[];

/**
 * pattern_init - Initialise a Module - Implements Module::init()
 */
static bool pattern_init(struct NeoMutt *n)
{
  // struct PatternModuleData *mod_data = MUTT_MEM_CALLOC(1, struct PatternModuleData);
  // neomutt_set_module_data(n, MODULE_ID_PATTERN, mod_data);

  return true;
}

/**
 * pattern_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool pattern_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  return cs_register_variables(cs, PatternVars);
}

/**
 * pattern_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool pattern_cleanup(struct NeoMutt *n)
{
  // struct PatternModuleData *mod_data = neomutt_get_module_data(n, MODULE_ID_PATTERN);
  // ASSERT(mod_data);

  // FREE(&mod_data);
  return true;
}

/**
 * ModulePattern - Module for the Pattern library
 */
const struct Module ModulePattern = {
  MODULE_ID_PATTERN,
  "pattern",
  pattern_init,
  NULL, // config_define_types
  pattern_config_define_variables,
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  pattern_cleanup,
};
