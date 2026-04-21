/**
 * @file
 * Definition of the Commands Module
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
 * @page commands_module Definition of the Commands Module
 *
 * Definition of the Commands Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "module_data.h"

extern const struct Command CommandsCommands[];

/**
 * commands_init - Initialise a Module - Implements Module::init()
 */
static bool commands_init(struct NeoMutt *n)
{
  struct CommandsModuleData *mod_data = MUTT_MEM_CALLOC(1, struct CommandsModuleData);
  STAILQ_INIT(&mod_data->muttrc_stack);
  neomutt_set_module_data(n, MODULE_ID_COMMANDS, mod_data);

  mod_data->notify = notify_new();
  notify_set_parent(mod_data->notify, n->notify);

  return true;
}

/**
 * commands_commands_register - Register NeoMutt Commands - Implements Module::commands_register()
 */
static bool commands_commands_register(struct NeoMutt *n, struct CommandArray *ca)
{
  return commands_register(ca, CommandsCommands);
}

/**
 * commands_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool commands_cleanup(struct NeoMutt *n, void *data)
{
  struct CommandsModuleData *mod_data = data;

  notify_free(&mod_data->notify);

  mutt_list_free(&mod_data->muttrc_stack);
  FREE(&mod_data);
  return true;
}

/**
 * ModuleCommands - Module for the Commands library
 */
const struct Module ModuleCommands = {
  MODULE_ID_COMMANDS,
  "commands",
  commands_init,
  NULL, // config_define_types
  NULL, // config_define_variables
  commands_commands_register,
  NULL, // gui_init
  NULL, // gui_cleanup
  commands_cleanup,
};
