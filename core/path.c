/**
 * @file
 * Mailbox path
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * @page core_path Mailbox path
 *
 * Mailbox path
 */

#include "config.h"
#include "mutt/lib.h"
#include "path.h"

/**
 * mutt_path_free - Free a Path
 * @param ptr Path to free
 *
 * Some of the paths may be shared, so check for dupes before freeing.
 */
void mutt_path_free(struct Path **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Path *path = *ptr;

  if ((path->desc != path->orig) && (path->desc != path->canon))
    FREE(&path->desc);

  if (path->orig != path->canon)
    FREE(&path->orig);

  FREE(&path->canon);
  FREE(&path->pretty);
  FREE(ptr);
}

/**
 * mutt_path_new - Create a Path
 * @retval ptr New Path
 */
struct Path *mutt_path_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct Path));
}

/**
 * mutt_path_dup - Duplicate a Path
 * @param p Path
 * @retval ptr New Path
 */
struct Path *mutt_path_dup(struct Path *p)
{
  if (!p)
    return NULL;

  struct Path *p_new = mutt_path_new();

  p_new->desc = mutt_str_strdup(p->desc);
  p_new->orig = mutt_str_strdup(p->orig);
  p_new->canon = mutt_str_strdup(p->canon);
  p_new->type = p->type;
  p_new->flags = p->flags;

  return p_new;
}

/**
 * path_partial_match_string - Compare two strings, allowing for missing values
 * @param str1 First string
 * @param str2 Second string
 * @retval true Strings match
 *
 * @note If both strings are present, they must be identical.
 *       If either one of the strings is missing, that is also a match.
 */
bool path_partial_match_string(const char *str1, const char *str2)
{
  if (str1 && (str1[0] == '\0'))
    str1 = NULL;
  if (str2 && (str2[0] == '\0'))
    str2 = NULL;
  if (!str1 && !str2) // both empty
    return true;
  if (!str1 ^ !str2) // one is empty, but not the other
    return true;
  return (mutt_str_strcmp(str1, str2) == 0);
}

/**
 * path_partial_match_number - Compare two numbers, allowing for missing values
 * @param num1 First number
 * @param num2 Second number
 * @retval true Numbers match
 *
 * @note If both numbers are non-zero, they must be identical.
 *       If either one of the numbers is zero, that is also a match.
 */
bool path_partial_match_number(int num1, int num2)
{
  if ((num1 == 0) && (num2 == 0)) // both zero
    return true;
  if ((num1 == 0) ^ (num2 == 0)) // one is zero, but not the other
    return true;
  return (num1 == num2);
}
