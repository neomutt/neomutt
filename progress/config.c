/**
 * @file
 * Config used by the Progress Bar
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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
 * @page progress_config Config used by the Progress Bar
 *
 * Config used by the Progress Bar
 */

#include "config.h"
#include <stddef.h>
#include "config/lib.h"

/**
 * ProgressVars - Config definitions for the Progress Bar
 */
struct ConfigDef ProgressVars[] = {
  // clang-format off
  { "net_inc", DT_NUMBER|D_INTEGER_NOT_NEGATIVE, 10, 0, NULL,
    "(socket) Update the progress bar after this many KB sent/received (0 to disable)"
  },
  { "read_inc", DT_NUMBER|D_INTEGER_NOT_NEGATIVE, 10, 0, NULL,
    "Update the progress bar after this many records read (0 to disable)"
  },
  { "time_inc", DT_NUMBER|D_INTEGER_NOT_NEGATIVE, 0, 0, NULL,
    "Frequency of progress bar updates (milliseconds)"
  },
  { "write_inc", DT_NUMBER|D_INTEGER_NOT_NEGATIVE, 10, 0, NULL,
    "Update the progress bar after this many records written (0 to disable)"
  },
  { NULL },
  // clang-format on
};
