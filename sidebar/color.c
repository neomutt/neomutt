/**
 * @file
 * Colour used by libsidebar
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
 * @page sidebar_color Colour used by libsidebar
 *
 * Colour used by libsidebar
 */

#include "config.h"
#include "color.h"
#include "color/lib.h"

/**
 * SidebarColorDefs - Mapping of colour names to their IDs
 */
static const struct ColorDefinition SidebarColorDefs[] = {
  // clang-format off
  { "sidebar_background", CD_SID_BACKGROUND, CDT_SIMPLE  },
  { "sidebar_divider",    CD_SID_DIVIDER,    CDT_SIMPLE  },
  { "sidebar_flagged",    CD_SID_FLAGGED,    CDT_SIMPLE  },
  { "sidebar_highlight",  CD_SID_HIGHLIGHT,  CDT_SIMPLE  },
  { "sidebar_indicator",  CD_SID_INDICATOR,  CDT_SIMPLE  },
  { "sidebar_new",        CD_SID_NEW,        CDT_SIMPLE  },
  { "sidebar_ordinary",   CD_SID_ORDINARY,   CDT_SIMPLE  },
  { "sidebar_spool_file", CD_SID_SPOOL_FILE, CDT_SIMPLE  },
  { "sidebar_unread",     CD_SID_UNREAD,     CDT_SIMPLE  },
  // Deprecated
  { "sidebar_spoolfile",  CD_SID_SPOOL_FILE, CDT_SIMPLE, CDF_SYNONYM },
  { NULL, 0 },
  // clang-format on
};

/**
 * sidebar_colors_init - Initialise the Sidebar Colours
 */
void sidebar_colors_init(void)
{
  color_register_domain("sidebar", CD_SIDEBAR);
  color_register_colors(CD_SIDEBAR, SidebarColorDefs);
}
