/**
 * @file
 * Definition of the Attach Module
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
 * @page attach_module Definition of the Attach Module
 *
 * Definition of the Attach Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "core/lib.h"
#include "attachments.h"

/**
 * attach_init - Initialise a Module - Implements Module::init()
 */
static bool attach_init(struct NeoMutt *n)
{
  attachlist_init();
  return true;
}

/**
 * attach_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static void attach_cleanup(struct NeoMutt *n)
{
  attachlist_cleanup();
}

/**
 * ModuleAttach - Module for the Attach library
 */
const struct Module ModuleAttach = {
  "attach",       attach_init,
  NULL, // config_define_types
  NULL, // config_define_variables
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  attach_cleanup,
  NULL, // mod_data
};
