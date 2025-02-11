/**
 * @file
 * Definition of the Nntp Module
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
 * @page nntp_module Definition of the Nntp Module
 *
 * Definition of the Nntp Module
 */

#include "config.h"
#include <stddef.h>
#include "core/lib.h"

/**
 * ModuleNntp - Module for the Nntp library
 */
const struct Module ModuleNntp = {
  "nntp",
  NULL, // init
  NULL, // config_define_types
  NULL, // config_define_variables
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  NULL, // cleanup
  NULL, // mod_data
};
