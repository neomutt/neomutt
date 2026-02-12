/**
 * @file
 * Definition of the Index Module
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
 * @page index_module Definition of the Index Module
 *
 * Definition of the Index Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "module_data.h"

extern struct ConfigDef IndexVars[];

/**
 * index_init - Initialise a Module - Implements Module::init()
 */
static bool index_init(struct NeoMutt *n)
{
  struct IndexModuleData *md = MUTT_MEM_CALLOC(1, struct IndexModuleData);
  neomutt_set_module_data(n, MODULE_ID_INDEX, md);

  return true;
}

/**
 * index_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool index_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  return cs_register_variables(cs, IndexVars);
}

/**
 * index_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool index_cleanup(struct NeoMutt *n)
{
  struct IndexModuleData *md = neomutt_get_module_data(n, MODULE_ID_INDEX);
  ASSERT(md);

  FREE(&md);
  return true;
}

/**
 * ModuleIndex - Module for the Index library
 */
const struct Module ModuleIndex = {
  MODULE_ID_INDEX,
  "index",
  index_init,
  NULL, // config_define_types
  index_config_define_variables,
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  index_cleanup,
};
