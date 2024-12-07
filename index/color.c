/**
 * @file
 * Colour used by libindex
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
 * @page index_color Colour used by libindex
 *
 * Colour used by libindex
 */

#include "config.h"
#include "color.h"
#include "color/lib.h"

/**
 * IndexColorDefs - Mapping of colour names to their IDs
 */
static const struct ColorDefinition IndexColorDefs[] = {
  // clang-format off
  { "index",           CD_IND_INDEX,     CDT_PATTERN },
  { "index_author",    CD_IND_AUTHOR,    CDT_PATTERN },
  { "index_collapsed", CD_IND_COLLAPSED, CDT_PATTERN },
  { "index_date",      CD_IND_DATE,      CDT_PATTERN },
  { "index_flags",     CD_IND_FLAGS,     CDT_PATTERN },
  { "index_label",     CD_IND_LABEL,     CDT_PATTERN },
  { "index_number",    CD_IND_NUMBER,    CDT_PATTERN },
  { "index_size",      CD_IND_SIZE,      CDT_PATTERN },
  { "index_subject",   CD_IND_SUBJECT,   CDT_PATTERN },
  { "index_tag",       CD_IND_TAG,       CDT_PATTERN },
  { "index_tags",      CD_IND_TAGS,      CDT_PATTERN },
  { NULL, 0 },
  // clang-format on
};

/**
 * index_colors_init - Initialise the Index Colours
 */
void index_colors_init(void)
{
  color_register_domain("index", CD_INDEX);
  color_register_colors(CD_INDEX, IndexColorDefs);
}
