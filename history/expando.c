/**
 * @file
 * History Expando definitions
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
 * @page history_expando History Expando definitions
 *
 * History Expando definitions
 */

#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "expando.h"
#include "lib.h"
#include "expando/lib.h"

/**
 * history_number - History: Index number - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long history_number(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct HistoryEntry *entry = data;

  return entry->num + 1;
}

/**
 * history_match - History: History match - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void history_match(const struct ExpandoNode *node, void *data,
                          MuttFormatFlags flags, struct Buffer *buf)
{
  const struct HistoryEntry *entry = data;

  const char *s = entry->history;
  buf_strcpy(buf, s);
}

/**
 * HistoryRenderData - Callbacks for History Expandos
 *
 * @sa HistoryFormatDef, ExpandoDataGlobal, ExpandoDataHistory
 */
const struct ExpandoRenderData HistoryRenderData[] = {
  // clang-format off
  { ED_HISTORY, ED_HIS_NUMBER, NULL,          history_number },
  { ED_HISTORY, ED_HIS_MATCH,  history_match, NULL },
  { -1, -1, NULL, NULL },
  // clang-format on
};
