/**
 * @file
 * Definition of the Key Module
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
 * @page key_module Definition of the Key Module
 *
 * Definition of the Key Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "init.h"
#include "module_data.h"

extern const struct Command KeyCommands[];

/**
 * key_init - Initialise a Module - Implements Module::init()
 */
static bool key_init(struct NeoMutt *n)
{
  struct KeyModuleData *mod_data = MUTT_MEM_CALLOC(1, struct KeyModuleData);
  neomutt_set_module_data(n, MODULE_ID_KEY, mod_data);

  mod_data->notify = notify_new();
  notify_set_parent(mod_data->notify, n->notify);

  return true;
}

/**
 * key_commands_register - Register NeoMutt Commands - Implements Module::commands_register()
 */
static bool key_commands_register(struct NeoMutt *n, struct CommandArray *ca)
{
  return commands_register(ca, KeyCommands);
}

/**
 * key_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool key_cleanup(struct NeoMutt *n)
{
  struct KeyModuleData *mod_data = neomutt_get_module_data(n, MODULE_ID_KEY);
  ASSERT(mod_data);

  notify_free(&mod_data->notify);

  FREE(&mod_data);
  return true;
}

/**
 * key_gui_init - Initialise the GUI - Implements Module::gui_init()
 */
static bool key_gui_init(struct NeoMutt *n)
{
  km_set_abort_key();
  return true;
}

/**
 * key_gui_cleanup - Clean up the GUI - Implements Module::gui_cleanup()
 */
static void key_gui_cleanup(struct NeoMutt *n)
{
  km_cleanup();
}

/**
 * ModuleKey - Module for the Key library
 */
const struct Module ModuleKey = {
  MODULE_ID_KEY,
  "key",
  key_init,
  NULL, // config_define_types
  NULL, // config_define_variables
  key_commands_register,
  key_gui_init,
  key_gui_cleanup,
  key_cleanup,
};
