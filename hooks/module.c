/**
 * @file
 * Definition of the Hooks Module
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
 * @page hooks_module Definition of the Hooks Module
 *
 * Definition of the Hooks Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "exec.h"
#include "module_data.h"
#include "parse.h"

extern struct ConfigDef HooksVars[];
extern const struct Command HooksCommands[];

/**
 * hooks_init - Initialise a Module - Implements Module::init()
 */
static bool hooks_init(struct NeoMutt *n)
{
  struct HooksModuleData *mod_data = MUTT_MEM_CALLOC(1, struct HooksModuleData);
  TAILQ_INIT(&mod_data->hooks);
  neomutt_set_module_data(n, MODULE_ID_HOOKS, mod_data);

  mod_data->notify = notify_new();
  notify_set_parent(mod_data->notify, n->notify);

  return true;
}

/**
 * hooks_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool hooks_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  return cs_register_variables(cs, HooksVars);
}

/**
 * hooks_commands_register - Register NeoMutt Commands - Implements Module::commands_register()
 */
static bool hooks_commands_register(struct NeoMutt *n, struct CommandArray *ca)
{
  return commands_register(ca, HooksCommands);
}

/**
 * hooks_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool hooks_cleanup(struct NeoMutt *n, void *data)
{
  struct HooksModuleData *mod_data = data;

  notify_free(&mod_data->notify);

  mutt_delete_hooks(&mod_data->hooks, CMD_NONE);
  delete_idxfmt_hooks(&mod_data->idx_fmt_hooks);

  FREE(&mod_data);
  return true;
}

/**
 * ModuleHooks - Module for the Hooks library
 */
const struct Module ModuleHooks = {
  MODULE_ID_HOOKS,
  "hooks",
  hooks_init,
  NULL, // config_define_types
  hooks_config_define_variables,
  hooks_commands_register,
  NULL, // gui_init
  NULL, // gui_cleanup
  hooks_cleanup,
};
