/**
 * @file
 * Definition of the Notmuch Module
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
 * @page notmuch_module Definition of the Notmuch Module
 *
 * Definition of the Notmuch Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "module_data.h"

extern struct ConfigDef NotmuchVars[];

extern const struct Command NmCommands[];

/**
 * notmuch_init - Initialise a Module - Implements Module::init()
 */
static bool notmuch_init(struct NeoMutt *n)
{
  // struct NotmuchModuleData *md = MUTT_MEM_CALLOC(1, struct NotmuchModuleData);
  // neomutt_set_module_data(n, MODULE_ID_NOTMUCH, md);

  return true;
}

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
 * notmuch_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool notmuch_cleanup(struct NeoMutt *n)
{
  // struct NotmuchModuleData *md = neomutt_get_module_data(n, MODULE_ID_NOTMUCH);
  // ASSERT(md);

  // FREE(&md);
  return true;
}

/**
 * ModuleNotmuch - Module for the Notmuch library
 */
const struct Module ModuleNotmuch = {
  MODULE_ID_NOTMUCH,
  "notmuch",
  notmuch_init,
  NULL, // config_define_types
  notmuch_config_define_variables,
  notmuch_commands_register,
  NULL, // gui_init
  NULL, // gui_cleanup
  notmuch_cleanup,
};
