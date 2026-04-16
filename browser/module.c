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
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "module_data.h"

extern struct ConfigDef BrowserVars[];

/**
 * browser_init - Initialise a Module - Implements Module::init()
 */
static bool browser_init(struct NeoMutt *n)
{
  struct BrowserModuleData *mod_data = MUTT_MEM_CALLOC(1, struct BrowserModuleData);
  neomutt_set_module_data(n, MODULE_ID_BROWSER, mod_data);

  mod_data->notify = notify_new();
  notify_set_parent(mod_data->notify, n->notify);

  buf_alloc(&mod_data->last_dir, PATH_MAX);
  buf_alloc(&mod_data->last_dir_backup, PATH_MAX);

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

  notify_free(&mod_data->notify);

  buf_dealloc(&mod_data->last_dir);
  buf_dealloc(&mod_data->last_dir_backup);
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
  NULL, // gui_cleanup
  browser_cleanup,
};
