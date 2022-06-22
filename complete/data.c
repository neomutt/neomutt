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

#ifdef USE_NOTMUCH
/**
 * completion_data_free_nm_list - Free the Notmuch Completion strings
 * @param cd Completion Data
 */
void completion_data_free_nm_list(struct CompletionData *cd)
{
  if (!cd || !cd->nm_tags)
    return;

  for (int i = 0; cd->nm_tags[i]; i++)
    FREE(&cd->nm_tags[i]);
  FREE(&cd->nm_tags);
}
#endif

/**
 * completion_data_free - Free the Completion Data
 * @param ptr CompletionData to free
 */
void completion_data_free(struct CompletionData **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct CompletionData *cd = *ptr;

  FREE(&cd->Matches);
#ifdef USE_NOTMUCH
  completion_data_free_nm_list(cd);
#endif

  FREE(ptr);
}

/**
 * completion_data_new - Create new Completion Data
 * @retval ptr New Completion Data
 */
struct CompletionData *completion_data_new(void)
{
  struct CompletionData *cd = mutt_mem_calloc(1, sizeof(struct CompletionData));

  cd->MatchesListsize = 512;
  cd->Matches = mutt_mem_calloc(cd->MatchesListsize, sizeof(char *));

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

  memset(cd->UserTyped, 0, sizeof(cd->UserTyped));
  memset(cd->Completed, 0, sizeof(cd->Completed));
  memset(cd->Matches, 0, cd->MatchesListsize * sizeof(char *));
  cd->NumMatched = 0;
#ifdef USE_NOTMUCH
  completion_data_free_nm_list(cd);
#endif
}
