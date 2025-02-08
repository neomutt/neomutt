/**
 * @file
 * Definition of the Notmuch Module
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
 * @page nm_module Definition of the Notmuch Module
 *
 * Definition of the Notmuch Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "config/lib.h"
#include "core/lib.h"

extern struct ConfigDef NotmuchVars[];

extern const struct Command NmCommands[];

/**
 * notmuch_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool notmuch_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  bool rc = false;

#if defined(USE_NOTMUCH)
  rc |= cs_register_variables(cs, NotmuchVars);
#endif

  return rc;
}

/**
 * notmuch_commands_register - Register NeoMutt Commands - Implements Module::commands_register()
 */
static bool notmuch_commands_register(struct NeoMutt *n, struct CommandArray *ca)
{
  return commands_register(ca, NmCommands);
}

/**
 * ModuleNotmuch - Module for the Notmuch library
 */
const struct Module ModuleNotmuch = {
  "notmuch",
  NULL, // init
  NULL, // config_define_types
  notmuch_config_define_variables,
  notmuch_commands_register,
  NULL, // gui_init
  NULL, // gui_cleanup
  NULL, // cleanup
  NULL, // mod_data
};
