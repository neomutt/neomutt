/**
 * @file
 * Definition of the Color Module
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
 * @page color_module Definition of the Color Module
 *
 * Definition of the Color Module
 */

#include "config.h"
#include <stddef.h>
#include "core/lib.h"
#include "color.h"

/**
 * color_gui_init - Initialise the GUI - Implements Module::gui_init()
 */
static bool color_gui_init(struct NeoMutt *n)
{
  return true;
}

/**
 * color_gui_cleanup - Clean up the GUI - Implements Module::gui_cleanup()
 */
static void color_gui_cleanup(struct NeoMutt *n)
{
  colors_cleanup();
}

/**
 * ModuleColor - Module for the Color library
 */
const struct Module ModuleColor = {
  "color",
  NULL, // init
  NULL, // config_define_types
  NULL, // config_define_variables
  NULL, // commands_register
  color_gui_init, color_gui_cleanup,
  NULL, // cleanup
  NULL, // mod_data
};
