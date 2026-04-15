/**
 * @file
 * Definition of the Browser Module
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
 * @page browser_module Definition of the Browser Module
 *
 * Definition of the Browser Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "lib.h"
#include "module_data.h"

extern struct ConfigDef BrowserVars[];

/**
 * browser_init - Initialise a Module - Implements Module::init()
 */
static bool browser_init(struct NeoMutt *n)
{
  struct BrowserModuleData *mod_data = MUTT_MEM_CALLOC(1, struct BrowserModuleData);
  neomutt_set_module_data(n, MODULE_ID_BROWSER, mod_data);

  return true;
}

/**
 * browser_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool browser_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  return cs_register_variables(cs, BrowserVars);
}

/**
 * browser_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool browser_cleanup(struct NeoMutt *n)
{
  struct BrowserModuleData *mod_data = neomutt_get_module_data(n, MODULE_ID_BROWSER);
  ASSERT(mod_data);

  FREE(&mod_data);
  return true;
}

/**
 * browser_gui_init - Initialise the GUI - Implements Module::gui_init()
 */
static bool browser_gui_init(struct NeoMutt *n)
{
  return true;
}

/**
 * browser_gui_cleanup - Clean up the GUI - Implements Module::gui_cleanup()
 */
static void browser_gui_cleanup(struct NeoMutt *n)
{
  mutt_browser_cleanup();
}

/**
 * ModuleBrowser - Module for the Browser library
 */
const struct Module ModuleBrowser = {
  MODULE_ID_BROWSER,
  "browser",
  browser_init,
  NULL, // config_define_types
  browser_config_define_variables,
  NULL, // commands_register
  browser_gui_init,
  browser_gui_cleanup,
  browser_cleanup,
};
