/**
 * @file
 * Definition of the Alias Module
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
 * @page alias_module Definition of the Alias Module
 *
 * Definition of the Alias Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "alias.h"
#include "reverse.h"

extern struct ConfigDef AliasVars[];

/**
 * alias_init - Initialise a Module - Implements Module::init()
 */
static bool alias_init(struct NeoMutt *n)
{
  alias_reverse_init();
  return true;
}

/**
 * alias_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool alias_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  return cs_register_variables(cs, AliasVars);
}

/**
 * alias_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static void alias_cleanup(struct NeoMutt *n)
{
  struct Alias *np = NULL;
  TAILQ_FOREACH(np, &Aliases, entries)
  {
    alias_reverse_delete(np);
  }
  aliaslist_clear(&Aliases);
  alias_reverse_shutdown();
}

/**
 * ModuleAlias - Module for the Alias library
 */
const struct Module ModuleAlias = {
  "alias",
  alias_init,
  NULL, // config_define_types
  alias_config_define_variables,
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  alias_cleanup,
  NULL, // mod_data
};
