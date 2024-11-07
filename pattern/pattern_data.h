/**
 * @file
 * Private Pattern Data
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

#ifndef MUTT_PATTERN_PATTERN_DATA_H
#define MUTT_PATTERN_PATTERN_DATA_H

#include <stdbool.h>
#include "mutt/lib.h"

struct Menu;

/**
 * struct PatternEntry - A line in the Pattern Completion menu
 */
struct PatternEntry
{
  int num;          ///< Index number
  const char *tag;  ///< Copied to buffer if selected
  const char *expr; ///< Displayed in the menu
  const char *desc; ///< Description of pattern
};
ARRAY_HEAD(PatternEntryArray, struct PatternEntry);

/**
 * struct PatternData - Data to pass to the Pattern Functions
 */
struct PatternData
{
  bool                      done;       ///< Should we close the Dialog?
  bool                      selection;  ///< Was a selection made?
  struct Buffer            *buf;        ///< Buffer for the results
  struct Menu              *menu;       ///< Pattern Menu
  struct PatternEntryArray  entries;    ///< Patterns for the Menu
};

struct PatternData *pattern_data_new(void);
void                pattern_data_free(struct Menu *menu, void **ptr);

#endif /* MUTT_PATTERN_PATTERN_DATA_H */
