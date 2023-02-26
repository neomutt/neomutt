/**
 * @file
 * Config used by libpager
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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
 * @page pager_config Config used by libpager
 *
 * Config used by libpager
 */

#include "config.h"
#include <stddef.h>
#include <config/lib.h>
#include <stdbool.h>

/**
 * PagerVars - Config definitions for the Pager
 */
static struct ConfigDef PagerVars[] = {
  // clang-format off
  { "allow_ansi", DT_BOOL, false, 0, NULL,
    "Allow ANSI colour codes in rich text messages"
  },
  { "display_filter", DT_STRING|DT_COMMAND|R_PAGER, 0, 0, NULL,
    "External command to pre-process an email before display"
  },
  { "header_color_partial", DT_BOOL|R_PAGER_FLOW, false, 0, NULL,
    "Only colour the part of the header matching the regex"
  },
  { "pager_context", DT_NUMBER|DT_NOT_NEGATIVE, 0, 0, NULL,
    "Number of lines of overlap when changing pages in the pager"
  },
  { "pager_format", DT_STRING|R_PAGER, IP "-%Z- %C/%m: %-20.20n   %s%*  -- (%P)", 0, NULL,
    "printf-like format string for the pager's status bar"
  },
  { "pager_index_lines", DT_NUMBER|DT_NOT_NEGATIVE|R_PAGER, 0, 0, NULL,
    "Number of index lines to display above the pager"
  },
  { "pager_read_delay", DT_NUMBER|DT_NOT_NEGATIVE, 0, 0, NULL,
    "Number of seconds to wait before marking a message read"
  },
  { "pager_skip_quoted_context", DT_NUMBER|DT_NOT_NEGATIVE, 0, 0, NULL,
    "Lines of context to show when skipping quoted text"
  },
  { "pager_stop", DT_BOOL, false, 0, NULL,
    "Don't automatically open the next message when at the end of a message"
  },
  { "prompt_after", DT_BOOL, true, 0, NULL,
    "Pause after running an external pager"
  },
  { "search_context", DT_NUMBER|DT_NOT_NEGATIVE, 0, 0, NULL,
    "Context to display around search matches"
  },
  { "smart_wrap", DT_BOOL|R_PAGER_FLOW, true, 0, NULL,
    "Wrap text at word boundaries"
  },
  { "smileys", DT_REGEX|R_PAGER, IP "(>From )|(:[-^]?[][)(><}{|/DP])", 0, NULL,
    "Regex to match smileys to prevent mistakes when quoting text"
  },
  { "tilde", DT_BOOL|R_PAGER, false, 0, NULL,
    "Display '~' in the pager after the end of the email"
  },
  { "toggle_quoted_show_levels", DT_NUMBER|DT_NOT_NEGATIVE, 0, 0, NULL,
    "Number of quote levels to show with toggle-quoted"
  },

  { "skip_quoted_offset", DT_SYNONYM, IP "pager_skip_quoted_context", IP "2021-06-18" },

  { NULL },
  // clang-format on
};

/**
 * config_init_pager - Register pager config variables - Implements ::module_init_config_t - @ingroup cfg_module_api
 */
bool config_init_pager(struct ConfigSet *cs)
{
  return cs_register_variables(cs, PagerVars, 0);
}
