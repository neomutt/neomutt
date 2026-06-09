/**
 * @file
 * Definition of the Mlist Module
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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
 * @page mlist_module Definition of the Mlist Module
 *
 * Definition of the Mlist Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "module_data.h"

extern struct ConfigDef MlistVars[];

/**
 * mlist_init - Initialise a Module - Implements Module::init()
 */
static bool mlist_init(struct NeoMutt *n)
{
  struct MlistModuleData *mod_data = MUTT_MEM_CALLOC(1, struct MlistModuleData);
  neomutt_set_module_data(n, MODULE_ID_MLIST, mod_data);

  return true;
}

/**
 * mlist_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool mlist_cleanup(struct NeoMutt *n, void *data)
{
  struct MlistModuleData *mod_data = data;

  FREE(&mod_data);
  return true;
}

/**
 * ModuleMlist - Module for the Mlist library
 */
const struct Module ModuleMlist = {
  MODULE_ID_MLIST,
  "mlist",
  mlist_init,
  NULL, // config_define_types
  NULL, // config_define_variables
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  mlist_cleanup,
};
