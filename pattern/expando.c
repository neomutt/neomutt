/**
 * @file
 * Pattern Expando definitions
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
 * @page pattern_expando Pattern Expando definitions
 *
 * Pattern Expando definitions
 */

#include <stdbool.h>
#include <stdio.h>
#include "private.h"
#include "mutt/lib.h"
#include "expando/lib.h"
#include "pattern_data.h"

/**
 * pattern_description - Pattern: pattern description - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void pattern_description(const struct ExpandoNode *node, void *data,
                                MuttFormatFlags flags, struct Buffer *buf)
{
  const struct PatternEntry *entry = data;

  const char *s = entry->desc;
  buf_strcpy(buf, s);
}

/**
 * pattern_expression - Pattern: pattern expression - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void pattern_expression(const struct ExpandoNode *node, void *data,
                               MuttFormatFlags flags, struct Buffer *buf)
{
  const struct PatternEntry *entry = data;

  const char *s = entry->expr;
  buf_strcpy(buf, s);
}

/**
 * pattern_number_num - Pattern: Index number - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long pattern_number_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct PatternEntry *entry = data;

  return entry->num;
}

/**
 * PatternRenderData - Callbacks for Pattern Expandos
 *
 * @sa PatternFormatDef, ExpandoDataGlobal, ExpandoDataPattern
 */
const struct ExpandoRenderData PatternRenderData[] = {
  // clang-format off
  { ED_PATTERN, ED_PAT_DESCRIPTION, pattern_description, NULL },
  { ED_PATTERN, ED_PAT_EXPRESSION,  pattern_expression,  NULL },
  { ED_PATTERN, ED_PAT_NUMBER,      NULL,                pattern_number_num },
  { -1, -1, NULL, NULL },
  // clang-format on
};
