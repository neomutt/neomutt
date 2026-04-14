/**
 * @file
 * Config used by Color
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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
 * @page color_config Config used by Color
 *
 * Config used by Color
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "config/lib.h"

/**
 * ColorVars - Config definitions for the Color module
 */
struct ConfigDef ColorVars[] = {
  // clang-format off
  { "color_directcolor", DT_BOOL|D_ON_STARTUP, false, 0, NULL,
    "Use 24bit colors (aka truecolor aka directcolor)"
  },

  { NULL },
  // clang-format on
};
