/**
 * @file
 * Definition of the Main Module
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
 * @page main_module Definition of the Main Module
 *
 * Definition of the Main Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "config/lib.h"
#include "core/lib.h"

extern struct ConfigDef MainVars[];

/**
 * main_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool main_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  return cs_register_variables(cs, MainVars);
}

/**
 * main_gui_init - Initialise the GUI - Implements Module::gui_init()
 */
static bool main_gui_init(struct NeoMutt *n)
{
  return true;
}

/**
 * main_gui_cleanup - Clean up the GUI - Implements Module::gui_cleanup()
 */
void main_gui_cleanup(struct NeoMutt *n)
{
}

/**
 * ModuleMain - Module for the Main library
 */
const struct Module ModuleMain = {
  MODULE_ID_MAIN,
  "main",
  NULL, // init
  NULL, // config_define_types
  main_config_define_variables,
  NULL, // commands_register
  main_gui_init,
  main_gui_cleanup,
  NULL, // cleanup
};
