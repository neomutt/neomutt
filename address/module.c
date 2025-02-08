/**
 * @file
 * Definition of the Address Module
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
 * @page addr_module Definition of the Address Module
 *
 * Definition of the Address Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "config/lib.h"
#include "core/lib.h"
#include "group.h"

extern const struct ConfigSetType CstAddress;

/**
 * address_init - Initialise a Module - Implements Module::init()
 */
static bool address_init(struct NeoMutt *n)
{
  mutt_grouplist_init();
  return true;
}

/**
 * address_config_define_types - Set up Config Types - Implements Module::config_define_types()
 */
static bool address_config_define_types(struct NeoMutt *n, struct ConfigSet *cs)
{
  return cs_register_type(cs, &CstAddress);
}

/**
 * address_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static void address_cleanup(struct NeoMutt *n)
{
  mutt_grouplist_cleanup();
}

/**
 * ModuleAddress - Module for the Address library
 */
const struct Module ModuleAddress = {
  "address",
  address_init,
  address_config_define_types,
  NULL, // config_define_variables
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  address_cleanup,
  NULL, // mod_data
};
