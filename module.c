/**
 * @file
 * Definition of the Main Module
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
 * @page main_module Definition of the Main Module
 *
 * Definition of the Main Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "alternates.h"
#include "commands.h"
#include "external.h"
#include "globals.h"
#include "hook.h"
#include "subjectrx.h"

extern struct ConfigDef MainVars[];
extern struct ConfigDef MainVarsIdn[];

extern const struct Command HookCommands[];
extern const struct Command LuaCommands[];
extern const struct Command MuttCommands[];

/**
 * main_init - Initialise a Module - Implements Module::init()
 */
static bool main_init(struct NeoMutt *n)
{
  alternates_init();
  subjrx_init();
  return true;
}

/**
 * main_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool main_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  bool rc = cs_register_variables(cs, MainVars);

#if defined(HAVE_LIBIDN)
  rc |= cs_register_variables(cs, MainVarsIdn);
#endif

  return rc;
}

/**
 * main_commands_register - Register NeoMutt Commands - Implements Module::commands_register()
 */
static bool main_commands_register(struct NeoMutt *n, struct CommandArray *ca)
{
  bool rc = true;

  rc &= commands_register(ca, MuttCommands);
  rc &= commands_register(ca, HookCommands);
#ifdef USE_LUA
  rc &= commands_register(ca, LuaCommands);
#endif

  return rc;
}

/**
 * main_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static void main_cleanup(struct NeoMutt *n)
{
  alternates_cleanup();
  subjrx_cleanup();

  /* Lists of strings */
  mutt_list_free(&AlternativeOrderList);
  mutt_list_free(&AutoViewList);
  mutt_list_free(&HeaderOrderList);
  mutt_list_free(&MimeLookupList);
  mutt_list_free(&UserHeader);

  FREE(&CurrentFolder);
  FREE(&LastFolder);
  FREE(&ShortHostname);

  external_cleanup();
  source_stack_cleanup();
  mutt_delete_hooks(MUTT_HOOK_NO_FLAGS);
}

/**
 * ModuleMain - Module for the Main library
 */
const struct Module ModuleMain = {
  "main",
  main_init,
  NULL, // config_define_types
  main_config_define_variables,
  main_commands_register,
  NULL, // gui_init
  NULL, // gui_cleanup
  main_cleanup,
  NULL, // mod_data
};
