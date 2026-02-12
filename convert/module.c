/**
 * @file
 * Definition of the Convert Module
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
 * @page convert_module Definition of the Convert Module
 *
 * Definition of the Convert Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "module_data.h"

/**
 * convert_init - Initialise a Module - Implements Module::init()
 */
static bool convert_init(struct NeoMutt *n)
{
  // struct ConvertModuleData *md = MUTT_MEM_CALLOC(1, struct ConvertModuleData);
  // neomutt_set_module_data(n, MODULE_ID_CONVERT, md);

  return true;
}

/**
 * convert_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool convert_cleanup(struct NeoMutt *n)
{
  // struct ConvertModuleData *md = neomutt_get_module_data(n, MODULE_ID_CONVERT);
  // ASSERT(md);

  // FREE(&md);
  return true;
}

/**
 * ModuleConvert - Module for the Convert library
 */
const struct Module ModuleConvert = {
  MODULE_ID_CONVERT,
  "convert",
  convert_init,
  NULL, // config_define_types
  NULL, // config_define_variables
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  convert_cleanup,
};
