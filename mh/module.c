/**
 * @file
 * Definition of the Mh Module
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
 * @page mh_module Definition of the Mh Module
 *
 * Definition of the Mh Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "module_data.h"

extern struct ConfigDef MhVars[];

/**
 * mh_init - Initialise a Module - Implements Module::init()
 */
static bool mh_init(struct NeoMutt *n)
{
  struct MhModuleData *mod_data = MUTT_MEM_CALLOC(1, struct MhModuleData);
  neomutt_set_module_data(n, MODULE_ID_MH, mod_data);

  mod_data->notify = notify_new();
  notify_set_parent(mod_data->notify, n->notify);

  return true;
}

/**
 * mh_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool mh_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  return cs_register_variables(cs, MhVars);
}

/**
 * mh_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool mh_cleanup(struct NeoMutt *n, void *data)
{
  struct MhModuleData *mod_data = data;

  notify_free(&mod_data->notify);

  FREE(&mod_data);
  return true;
}

/**
 * ModuleMh - Module for the Mh library
 */
const struct Module ModuleMh = {
  MODULE_ID_MH,
  "mh",
  mh_init,
  NULL, // config_define_types
  mh_config_define_variables,
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  mh_cleanup,
};
