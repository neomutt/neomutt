/**
 * @file
 * Definition of the Imap Module
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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
 * @page imap_module Definition of the Imap Module
 *
 * Definition of the Imap Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "config/lib.h"
#include "core/lib.h"

extern struct ConfigDef ImapVars[];
extern struct ConfigDef ImapVarsZlib[];

extern const struct Command ImapCommands[];

/**
 * imap_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool imap_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  bool rc = cs_register_variables(cs, ImapVars);

#if defined(USE_ZLIB)
  rc |= cs_register_variables(cs, ImapVarsZlib);
#endif

  return rc;
}

/**
 * imap_commands_register - Register NeoMutt Commands - Implements Module::commands_register()
 */
static bool imap_commands_register(struct NeoMutt *n, struct CommandArray *ca)
{
  return commands_register(ca, ImapCommands);
}

/**
 * ModuleImap - Module for the Imap library
 */
const struct Module ModuleImap = {
  "imap",
  NULL, // init
  NULL, // config_define_types
  imap_config_define_variables,
  imap_commands_register,
  NULL, // gui_init
  NULL, // gui_cleanup
  NULL, // cleanup
  NULL, // mod_data
};
