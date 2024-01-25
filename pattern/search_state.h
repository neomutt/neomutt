/**
 * @file
 * Holds state of a search
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
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

#ifndef MUTT_PATTERN_SEARCH_STATE_H
#define MUTT_PATTERN_SEARCH_STATE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * struct SearchState - Holds state of a search
 *
 * This data is kept to allow operations like OP_SEARCH_NEXT.
 */
struct SearchState
{
  struct PatternList *pattern;     ///< compiled search pattern
  struct Buffer      *string;      ///< search string
  struct Buffer      *string_expn; ///< expanded search string
  bool                reverse;     ///< search backwards
};

typedef uint8_t SearchFlags;       ///< Flags for a specific search, e.g. #SEARCH_PROMPT
#define SEARCH_NO_FLAGS        0   ///< No flags are set
#define SEARCH_PROMPT    (1 << 0)  ///< Ask for search input
#define SEARCH_OPPOSITE  (1 << 1)  ///< Search in the opposite direction

struct SearchState *search_state_new(void);
void search_state_free(struct SearchState **search);

#endif /* MUTT_PATTERN_SEARCH_STATE_H */
