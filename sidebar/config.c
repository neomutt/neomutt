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
#include <stdbool.h>

// clang-format off
short C_SidebarComponentDepth;      ///< Config: (sidebar) Strip leading path components from sidebar folders
char *C_SidebarDelimChars;          ///< Config: (sidebar) Characters that separate nested folders
char *C_SidebarDividerChar;         ///< Config: (sidebar) Character to draw between the sidebar and index
bool  C_SidebarFolderIndent;        ///< Config: (sidebar) Indent nested folders
char *C_SidebarFormat;              ///< Config: (sidebar) printf-like format string for the sidebar panel
char *C_SidebarIndentString;        ///< Config: (sidebar) Indent nested folders using this string
bool  C_SidebarNewMailOnly;         ///< Config: (sidebar) Only show folders with new/flagged mail
bool  C_SidebarNextNewWrap;         ///< Config: (sidebar) Wrap around when searching for the next mailbox with new mail
bool  C_SidebarNonEmptyMailboxOnly; ///< Config: (sidebar) Only show folders with a non-zero number of mail
bool  C_SidebarOnRight;             ///< Config: (sidebar) Display the sidebar on the right
bool  C_SidebarShortPath;           ///< Config: (sidebar) Abbreviate the paths using the #C_Folder variable
short C_SidebarSortMethod;          ///< Config: (sidebar) Method to sort the sidebar
bool  C_SidebarVisible;             ///< Config: (sidebar) Show the sidebar
short C_SidebarWidth;               ///< Config: (sidebar) Width of the sidebar
// clang-format on

struct ConfigDef SidebarVars[] = {
  // clang-format off
  { "sidebar_component_depth", DT_NUMBER|R_SIDEBAR, &C_SidebarComponentDepth, 0, 0, NULL,
    "(sidebar) Strip leading path components from sidebar folders"
  },
  { "sidebar_delim_chars", DT_STRING|R_SIDEBAR, &C_SidebarDelimChars, IP "/.", 0, NULL,
    "(sidebar) Characters that separate nested folders"
  },
  { "sidebar_divider_char", DT_STRING|R_SIDEBAR, &C_SidebarDividerChar, 0, 0, NULL,
    "(sidebar) Character to draw between the sidebar and index"
  },
  { "sidebar_folder_indent", DT_BOOL|R_SIDEBAR, &C_SidebarFolderIndent, false, 0, NULL,
    "(sidebar) Indent nested folders"
  },
  { "sidebar_format", DT_STRING|DT_NOT_EMPTY|R_SIDEBAR, &C_SidebarFormat, IP "%D%*  %n", 0, NULL,
    "(sidebar) printf-like format string for the sidebar panel"
  },
  { "sidebar_indent_string", DT_STRING|R_SIDEBAR, &C_SidebarIndentString, IP "  ", 0, NULL,
    "(sidebar) Indent nested folders using this string"
  },
  { "sidebar_new_mail_only", DT_BOOL|R_SIDEBAR, &C_SidebarNewMailOnly, false, 0, NULL,
    "(sidebar) Only show folders with new/flagged mail"
  },
  { "sidebar_next_new_wrap", DT_BOOL, &C_SidebarNextNewWrap, false, 0, NULL,
    "(sidebar) Wrap around when searching for the next mailbox with new mail"
  },
  { "sidebar_non_empty_mailbox_only", DT_BOOL|R_SIDEBAR, &C_SidebarNonEmptyMailboxOnly, false, 0, NULL,
    "(sidebar) Only show folders with a non-zero number of mail"
  },
  { "sidebar_on_right", DT_BOOL|R_INDEX|R_PAGER|R_REFLOW, &C_SidebarOnRight, false, 0, NULL,
    "(sidebar) Display the sidebar on the right"
  },
  { "sidebar_short_path", DT_BOOL|R_SIDEBAR, &C_SidebarShortPath, false, 0, NULL,
    "(sidebar) Abbreviate the paths using the #C_Folder variable"
  },
  { "sidebar_sort_method", DT_SORT|DT_SORT_SIDEBAR|R_SIDEBAR, &C_SidebarSortMethod, SORT_ORDER, 0, NULL,
    "(sidebar) Method to sort the sidebar"
  },
  { "sidebar_visible", DT_BOOL|R_REFLOW, &C_SidebarVisible, false, 0, NULL,
    "(sidebar) Show the sidebar"
  },
  { "sidebar_width", DT_NUMBER|DT_NOT_NEGATIVE|R_REFLOW, &C_SidebarWidth, 30, 0, NULL,
    "(sidebar) Width of the sidebar"
  },
  { NULL, 0, NULL, 0, 0, NULL, NULL },
  // clang-format on
};

/**
 * config_init_sidebar - Register sidebar config variables
 */
bool config_init_sidebar(struct ConfigSet *cs)
{
  return cs_register_variables(cs, SidebarVars, 0);
}
