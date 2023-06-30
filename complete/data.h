/**
 * @file
 * String auto-completion data
 *
 * @authors
 * Copyright (C) 2022 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_COMPLETE_DATA_H
#define MUTT_COMPLETE_DATA_H

#include "config.h"
#include <stdbool.h>

/**
 * struct CompletionData - State data for auto-completion
 */
struct CompletionData
{
  char user_typed[1024];       ///< Initial string that starts completion
  int num_matched;             ///< Number of matches for completion
  char completed[256];         ///< Completed string (command or variable)
  const char **match_list;     ///< Matching strings
  int match_list_len;          ///< Enough space for all of the config items
  bool free_match_strings;     ///< Should the strings in match_list be freed?
};

void                   completion_data_free(struct CompletionData **ptr);
void                   completion_data_free_match_strings(struct CompletionData *cd);
struct CompletionData *completion_data_new(void);
void                   completion_data_reset(struct CompletionData *cd);

#endif /* MUTT_COMPLETE_DATA_H */
