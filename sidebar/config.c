/**
 * @file
 * Config used by libsidebar
 *
 * @authors
 * Copyright (C) 2020 Aditya De Saha <adityadesaha@gmail.com>
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020-2024 Richard Russon <rich@flatcap.org>
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
#include <stdbool.h>
#include <stddef.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "expando/lib.h"
#include "sort.h"

/**
 * SidebarSortMethods - Sort methods for the sidebar
 */
static const struct Mapping SidebarSortMethods[] = {
  // clang-format off
  { "count",         SB_SORT_COUNT },
  { "desc",          SB_SORT_DESC },
  { "flagged",       SB_SORT_FLAGGED },
  { "path",          SB_SORT_PATH },
  { "unread",        SB_SORT_UNREAD },
  { "unsorted",      SB_SORT_UNSORTED },
  // Compatibility
  { "alpha",         SB_SORT_PATH },
  { "mailbox-order", SB_SORT_UNSORTED },
  { "name",          SB_SORT_PATH },
  { "new",           SB_SORT_UNREAD },
  { NULL, 0 },
  // clang-format on
};

/**
 * SidebarFormatDef - Expando definitions
 *
 * Config:
 * - $sidebar_format
 */
static const struct ExpandoDefinition SidebarFormatDef[] = {
  // clang-format off
  { "*", "padding-soft",  ED_GLOBAL,  ED_GLO_PADDING_SOFT,  node_padding_parse },
  { ">", "padding-hard",  ED_GLOBAL,  ED_GLO_PADDING_HARD,  node_padding_parse },
  { "|", "padding-eol",   ED_GLOBAL,  ED_GLO_PADDING_EOL,   node_padding_parse },
  { "!", "flagged",       ED_SIDEBAR, ED_SID_FLAGGED,       NULL },
  { "a", "notify",        ED_SIDEBAR, ED_SID_NOTIFY,        NULL },
  { "B", "name",          ED_SIDEBAR, ED_SID_NAME,          NULL },
  { "d", "deleted-count", ED_SIDEBAR, ED_SID_DELETED_COUNT, NULL },
  { "D", "description",   ED_SIDEBAR, ED_SID_DESCRIPTION,   NULL },
  { "F", "flagged-count", ED_SIDEBAR, ED_SID_FLAGGED_COUNT, NULL },
  { "L", "limited-count", ED_SIDEBAR, ED_SID_LIMITED_COUNT, NULL },
  { "n", "new-mail",      ED_SIDEBAR, ED_SID_NEW_MAIL,      NULL },
  { "N", "unread-count",  ED_SIDEBAR, ED_SID_UNREAD_COUNT,  NULL },
  { "o", "old-count",     ED_SIDEBAR, ED_SID_OLD_COUNT,     NULL },
  { "p", "poll",          ED_SIDEBAR, ED_SID_POLL,          NULL },
  { "r", "read-count",    ED_SIDEBAR, ED_SID_READ_COUNT,    NULL },
  { "S", "message-count", ED_SIDEBAR, ED_SID_MESSAGE_COUNT, NULL },
  { "t", "tagged-count",  ED_SIDEBAR, ED_SID_TAGGED_COUNT,  NULL },
  { "Z", "unseen-count",  ED_SIDEBAR, ED_SID_UNSEEN_COUNT,  NULL },
  { NULL, NULL, 0, -1, NULL }
  // clang-format on
};

/**
 * SidebarVars - Config definitions for the sidebar
 */
struct ConfigDef SidebarVars[] = {
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
  { "sidebar_format", DT_EXPANDO|D_NOT_EMPTY, IP "%D%*  %n", IP &SidebarFormatDef, NULL,
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
  { "sidebar_sort", DT_SORT, SB_SORT_UNSORTED, IP SidebarSortMethods, NULL,
    "(sidebar) Method to sort the sidebar"
  },
  { "sidebar_visible", DT_BOOL, false, 0, NULL,
    "(sidebar) Show the sidebar"
  },
  { "sidebar_width", DT_NUMBER|D_INTEGER_NOT_NEGATIVE, 30, 0, NULL,
    "(sidebar) Width of the sidebar"
  },

  { "sidebar_sort_method", DT_SYNONYM, IP "sidebar_sort", IP "2024-11-20" },

  { NULL },
  // clang-format on
};
