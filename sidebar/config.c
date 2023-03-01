/**
 * @file
 * Config used by libsidebar
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * @page sidebar_config Config used by libsidebar
 *
 * Config used by libsidebar
 */

#include "config.h"
#include <stddef.h>
#include <config/lib.h>
#include <mutt/lib.h>
#include <stdbool.h>

/**
 * SortSidebarMethods - Sort methods for the sidebar
 */
const struct Mapping SortSidebarMethods[] = {
  // clang-format off
  { "path",          SORT_PATH },
  { "alpha",         SORT_PATH },
  { "name",          SORT_PATH },
  { "count",         SORT_COUNT },
  { "desc",          SORT_DESC },
  { "flagged",       SORT_FLAGGED },
  { "unsorted",      SORT_ORDER },
  { "mailbox-order", SORT_ORDER },
  { "unread",        SORT_UNREAD },
  { "new",           SORT_UNREAD },
  { NULL, 0 },
  // clang-format on
};

/**
 * SidebarVars - Config definitions for the sidebar
 */
static struct ConfigDef SidebarVars[] = {
  // clang-format off
  { "sidebar_component_depth", DT_NUMBER, 0, 0, NULL,
    "(sidebar) Strip leading path components from sidebar folders"
  },
  { "sidebar_delim_chars", DT_STRING, IP "/.", 0, NULL,
    "(sidebar) Characters that separate nested folders"
  },
  { "sidebar_divider_char", DT_STRING, IP "\342\224\202", 0, NULL, // Box Drawings Light Vertical, U+2502
    "(sidebar) Character to draw between the sidebar and index"
  },
  { "sidebar_folder_indent", DT_BOOL, false, 0, NULL,
    "(sidebar) Indent nested folders"
  },
  { "sidebar_format", DT_STRING|DT_NOT_EMPTY, IP "%D%*  %n", 0, NULL,
    "(sidebar) printf-like format string for the sidebar panel"
  },
  { "sidebar_indent_string", DT_STRING, IP "  ", 0, NULL,
    "(sidebar) Indent nested folders using this string"
  },
  { "sidebar_new_mail_only", DT_BOOL, false, 0, NULL,
    "(sidebar) Only show folders with new/flagged mail"
  },
  { "sidebar_next_new_wrap", DT_BOOL, false, 0, NULL,
    "(sidebar) Wrap around when searching for the next mailbox with new mail"
  },
  { "sidebar_non_empty_mailbox_only", DT_BOOL, false, 0, NULL,
    "(sidebar) Only show folders with a non-zero number of mail"
  },
  { "sidebar_on_right", DT_BOOL, false, 0, NULL,
    "(sidebar) Display the sidebar on the right"
  },
  { "sidebar_short_path", DT_BOOL, false, 0, NULL,
    "(sidebar) Abbreviate the paths using the `$folder` variable"
  },
  { "sidebar_sort_method", DT_SORT, SORT_ORDER, IP SortSidebarMethods, NULL,
    "(sidebar) Method to sort the sidebar"
  },
  { "sidebar_visible", DT_BOOL, false, 0, NULL,
    "(sidebar) Show the sidebar"
  },
  { "sidebar_width", DT_NUMBER|DT_NOT_NEGATIVE, 30, 0, NULL,
    "(sidebar) Width of the sidebar"
  },
  { NULL },
  // clang-format on
};

/**
 * config_init_sidebar - Register sidebar config variables - Implements ::module_init_config_t - @ingroup cfg_module_api
 */
bool config_init_sidebar(struct ConfigSet *cs)
{
  return cs_register_variables(cs, SidebarVars, 0);
}
