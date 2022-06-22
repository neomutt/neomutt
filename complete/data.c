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

/**
 * @page complete_data String auto-completion data
 *
 * String auto-completion data
 */

#include "config.h"
#include "mutt/lib.h"
#include "data.h"

/**
 * completion_data_free_match_strings - Free the Completion strings
 * @param cd Completion Data
 */
void completion_data_free_match_strings(struct CompletionData *cd)
{
  if (!cd || !cd->free_match_strings)
    return;

  for (int i = 0; i < cd->num_matched; i++)
    FREE(&cd->match_list[i]);

  cd->free_match_strings = false;
}

/**
 * completion_data_free - Free the Completion Data
 * @param ptr CompletionData to free
 */
void completion_data_free(struct CompletionData **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct CompletionData *cd = *ptr;

  completion_data_free_match_strings(cd);

  FREE(&cd->match_list);

  FREE(ptr);
}

/**
 * completion_data_new - Create new Completion Data
 * @retval ptr New Completion Data
 */
struct CompletionData *completion_data_new(void)
{
  struct CompletionData *cd = mutt_mem_calloc(1, sizeof(struct CompletionData));

  cd->match_list_len = 512;
  cd->match_list = mutt_mem_calloc(cd->match_list_len, sizeof(char *));

  return cd;
}

/**
 * completion_data_reset - Wipe the stored Comletion Data
 * @param cd Completion Data
 */
void completion_data_reset(struct CompletionData *cd)
{
  if (!cd)
    return;

  completion_data_free_match_strings(cd);

  memset(cd->user_typed, 0, sizeof(cd->user_typed));
  memset(cd->completed, 0, sizeof(cd->completed));
  memset(cd->match_list, 0, cd->match_list_len * sizeof(char *));
  cd->num_matched = 0;
  cd->free_match_strings = false;
}
