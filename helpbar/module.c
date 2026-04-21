/**
 * @file
 * Definition of the Helpbar Module
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
 * @page helpbar_module Definition of the Helpbar Module
 *
 * Definition of the Helpbar Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "module_data.h"

extern struct ConfigDef HelpbarVars[];

/**
 * helpbar_init - Initialise a Module - Implements Module::init()
 */
static bool helpbar_init(struct NeoMutt *n)
{
  struct HelpbarModuleData *mod_data = MUTT_MEM_CALLOC(1, struct HelpbarModuleData);
  neomutt_set_module_data(n, MODULE_ID_HELPBAR, mod_data);

  mod_data->notify = notify_new();
  notify_set_parent(mod_data->notify, n->notify);

  return true;
}

/**
 * helpbar_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool helpbar_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  return cs_register_variables(cs, HelpbarVars);
}

/**
 * helpbar_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool helpbar_cleanup(struct NeoMutt *n, void *data)
{
  struct HelpbarModuleData *mod_data = data;

  notify_free(&mod_data->notify);

  FREE(&mod_data);
  return true;
}

/**
 * ModuleHelpbar - Module for the Helpbar library
 */
const struct Module ModuleHelpbar = {
  MODULE_ID_HELPBAR,
  "helpbar",
  helpbar_init,
  NULL, // config_define_types
  helpbar_config_define_variables,
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  helpbar_cleanup,
};
