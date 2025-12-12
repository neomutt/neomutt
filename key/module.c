/**
 * @file
 * Definition of the Key Module
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
 * @page key_module Definition of the Key Module
 *
 * Definition of the Key Module
 */

#include "config.h"
#include <stddef.h>
#include "core/lib.h"
#include "lib.h"

/**
 * key_gui_init - Initialise the GUI - Implements Module::gui_init()
 */
static bool key_gui_init(struct NeoMutt *n)
{
  return true;
}

/**
 * key_gui_cleanup - Clean up the GUI - Implements Module::gui_cleanup()
 */
static void key_gui_cleanup(struct NeoMutt *n)
{
  mutt_keys_cleanup();
}

/**
 * ModuleKey - Module for the Key library
 */
const struct Module ModuleKey = {
  "key",
  NULL, // init
  NULL, // config_define_types
  NULL, // config_define_variables
  NULL, // commands_register
  key_gui_init, key_gui_cleanup,
  NULL, // cleanup
  NULL, // mod_data
};
