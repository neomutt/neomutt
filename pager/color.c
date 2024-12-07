/**
 * @file
 * Colour used by libpager
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
 * @page pager_color Colour used by libpager
 *
 * Colour used by libpager
 */

#include "config.h"
#include "color.h"
#include "color/lib.h"

/**
 * PagerColorDefs - Mapping of colour names to their IDs
 */
static const struct ColorDefinition PagerColorDefs[] = {
  // clang-format off
  { "attachment",     CD_PAG_ATTACHMENT,     CDT_SIMPLE },
  { "attach_headers", CD_PAG_ATTACH_HEADERS, CDT_REGEX  },
  { "body",           CD_PAG_BODY,           CDT_REGEX  },
  { "hdrdefault",     CD_PAG_HDRDEFAULT,     CDT_SIMPLE },
  { "header",         CD_PAG_HEADER,         CDT_REGEX  },
  { "markers",        CD_PAG_MARKERS,        CDT_SIMPLE },
  { "search",         CD_PAG_SEARCH,         CDT_SIMPLE },
  { "signature",      CD_PAG_SIGNATURE,      CDT_SIMPLE },
  { "tilde",          CD_PAG_TILDE,          CDT_SIMPLE },
  { NULL, 0 },
  // clang-format on
};

/**
 * QuotedColorDefs - Mapping of colour names to their IDs
 */
static const struct ColorDefinition QuotedColorDefs[] = {
  // clang-format off
  { "quoted0", CD_QUO_LEVEL0, CDT_SIMPLE  },
  { "quoted1", CD_QUO_LEVEL1, CDT_SIMPLE  },
  { "quoted2", CD_QUO_LEVEL2, CDT_SIMPLE  },
  { "quoted3", CD_QUO_LEVEL3, CDT_SIMPLE  },
  { "quoted4", CD_QUO_LEVEL4, CDT_SIMPLE  },
  { "quoted5", CD_QUO_LEVEL5, CDT_SIMPLE  },
  { "quoted6", CD_QUO_LEVEL6, CDT_SIMPLE  },
  { "quoted7", CD_QUO_LEVEL7, CDT_SIMPLE  },
  { "quoted8", CD_QUO_LEVEL8, CDT_SIMPLE  },
  { "quoted9", CD_QUO_LEVEL9, CDT_SIMPLE  },
  // Deprecated
  { "quoted",  CD_QUO_LEVEL0, CDT_SIMPLE, CDF_SYNONYM },
  { NULL, 0 },
  // clang-format on
};

/**
 * pager_colors_init - Initialise the Pager Colours
 */
void pager_colors_init(void)
{
  color_register_domain("pager", CD_PAGER);
  color_register_colors(CD_PAGER, PagerColorDefs);

  color_register_domain("quoted", CD_QUOTED);
  color_register_colors(CD_QUOTED, QuotedColorDefs);
}
