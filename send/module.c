/**
 * @file
 * Definition of the Send Module
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
 * @page send_module Definition of the Send Module
 *
 * Definition of the Send Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "module_data.h"

extern struct ConfigDef SendVars[];

extern const struct Command SendCommands[];

/**
 * send_init - Initialise a Module - Implements Module::init()
 */
static bool send_init(struct NeoMutt *n)
{
  struct SendModuleData *md = MUTT_MEM_CALLOC(1, struct SendModuleData);
  neomutt_set_module_data(n, MODULE_ID_SEND, md);

  return true;
}

/**
 * send_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool send_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  return cs_register_variables(cs, SendVars);
}

/**
 * send_commands_register - Register NeoMutt Commands - Implements Module::commands_register()
 */
static bool send_commands_register(struct NeoMutt *n, struct CommandArray *ca)
{
  return commands_register(ca, SendCommands);
}

/**
 * send_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool send_cleanup(struct NeoMutt *n)
{
  struct SendModuleData *md = neomutt_get_module_data(n, MODULE_ID_SEND);
  ASSERT(md);

  FREE(&md);
  return true;
}

/**
 * ModuleSend - Module for the Send library
 */
const struct Module ModuleSend = {
  MODULE_ID_SEND,
  "send",
  send_init,
  NULL, // config_define_types
  send_config_define_variables,
  send_commands_register,
  NULL, // gui_init
  NULL, // gui_cleanup
  send_cleanup,
};
