/**
 * @file
 * Config used by Help Bar
 *
 * @authors
 * Copyright (C) 2020-2021 Richard Russon <rich@flatcap.org>
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
 * @page helpbar_config Config used by Help Bar
 *
 * Config used by Help Bar
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "config/lib.h"

/**
 * HelpbarVars - Config definitions for the Helpbar
 */
struct ConfigDef HelpbarVars[] = {
  // clang-format off
  { "help", DT_BOOL, true, 0, NULL,
    "Display a help line with common key bindings"
  },
  { NULL },
  // clang-format on
};
