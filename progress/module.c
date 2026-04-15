/**
 * @file
 * Definition of the Progress Module
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
 * @page progress_module Definition of the Progress Module
 *
 * Definition of the Progress Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "module_data.h"

extern struct ConfigDef ProgressVars[];

/**
 * progress_init - Initialise a Module - Implements Module::init()
 */
static bool progress_init(struct NeoMutt *n)
{
  // struct ProgressModuleData *mod_data = MUTT_MEM_CALLOC(1, struct ProgressModuleData);
  // neomutt_set_module_data(n, MODULE_ID_PROGRESS, mod_data);

  return true;
}

/**
 * progress_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool progress_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  return cs_register_variables(cs, ProgressVars);
}

/**
 * progress_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool progress_cleanup(struct NeoMutt *n)
{
  // struct ProgressModuleData *mod_data = neomutt_get_module_data(n, MODULE_ID_PROGRESS);
  // ASSERT(mod_data);

  // FREE(&mod_data);
  return true;
}

/**
 * ModuleProgress - Module for the Progress library
 */
const struct Module ModuleProgress = {
  MODULE_ID_PROGRESS,
  "progress",
  progress_init,
  NULL, // config_define_types
  progress_config_define_variables,
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  progress_cleanup,
};
