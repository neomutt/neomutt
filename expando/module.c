/**
 * @file
 * Definition of the Expando Module
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
 * @page expando_module Definition of the Expando Module
 *
 * Definition of the Expando Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "module_data.h"

extern const struct ConfigSetType CstExpando;

/**
 * expando_init - Initialise a Module - Implements Module::init()
 */
static bool expando_init(struct NeoMutt *n)
{
  // struct ExpandoModuleData *md = MUTT_MEM_CALLOC(1, struct ExpandoModuleData);
  // neomutt_set_module_data(n, MODULE_ID_EXPANDO, md);

  return true;
}

/**
 * expando_config_define_types - Set up Config Types - Implements Module::config_define_types()
 */
static bool expando_config_define_types(struct NeoMutt *n, struct ConfigSet *cs)
{
  return cs_register_type(cs, &CstExpando);
}

/**
 * expando_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool expando_cleanup(struct NeoMutt *n)
{
  // struct ExpandoModuleData *md = neomutt_get_module_data(n, MODULE_ID_EXPANDO);
  // ASSERT(md);

  // FREE(&md);
  return true;
}

/**
 * ModuleExpando - Module for the Expando library
 */
const struct Module ModuleExpando = {
  MODULE_ID_EXPANDO,
  "expando",
  expando_init,
  expando_config_define_types,
  NULL, // config_define_variables
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  expando_cleanup,
};
