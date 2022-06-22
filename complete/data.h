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

/**
 * struct CompletionData - State data for auto-completion
 */
struct CompletionData
{
  char UserTyped[1024];     ///< Initial string that starts completion
  int NumMatched;           ///< Number of matches for completion
  char Completed[256];      ///< Completed string (command or variable)
  const char **Matches;     ///< Matching strings
  int MatchesListsize;      ///< Enough space for all of the config items
#ifdef USE_NOTMUCH
  char **nm_tags;           ///< List of tags found by mutt_nm_query_complete()
#endif
};

void                   completion_data_free(struct CompletionData **ptr);
void                   completion_data_free_nm_list(struct CompletionData *cd);
struct CompletionData *completion_data_new(void);
void                   completion_data_reset(struct CompletionData *cd);

#endif /* MUTT_COMPLETE_DATA_H */