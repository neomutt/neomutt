/**
 * @file
 * Search a Paged File
 *
 * @authors
 * Copyright (C) 2024-2025 Richard Russon <rich@flatcap.org>
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
 * @page spager_search Search a Paged File
 *
 * Search a Paged File
 */

#include "config.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "search.h"
#include "pfile/lib.h"

// static const int SEARCH_TOO_MANY_MATCHES = 10000;

/**
 * spager_search_clear - Reset a Search object
 * @param sps Search Object
 */
void spager_search_clear(struct SimplePagerSearch *sps)
{
  if (!sps)
    return;

  FREE(&sps->pattern);

  if (sps->compiled)
  {
    regfree(&sps->regex);
    sps->compiled = false;
  }

  sps->direction = SD_SEARCH_FORWARDS;
  sps->pra = NULL;
}

/**
 * spager_search_free - Free a Search object
 * @param ptr Search object to free
 */
void spager_search_free(struct SimplePagerSearch **ptr)
{
  if (!ptr || !*ptr)
    return;

  spager_search_clear(*ptr);

  FREE(ptr);
}

/**
 * spager_search_new - Create a new Search object
 * @retval ptr New Search object
 */
struct SimplePagerSearch *spager_search_new(void)
{
  struct SimplePagerSearch *sps = MUTT_MEM_CALLOC(1, struct SimplePagerSearch);

  sps->show_search = true;

  return sps;
}

/**
 * spager_search_set_rows - Associate the Search with the Array of Rows
 * @param sps Search Object
 * @param pra Array of Rows
 */
void spager_search_set_rows(struct SimplePagerSearch *sps, struct PagedRowArray *pra)
{
  if (!sps || (sps->pra == pra))
    return;

  spager_search_clear(sps);
  sps->pra = pra;
}

/**
 * spager_search_search - Perform a search
 * @param sps         Search Object
 * @param pattern     Pattern to search for
 * @param start_index Array index to start searching
 * @param direction   Direction in which to search, e.g. #SD_SEARCH_FORWARDS
 * @param err         Buffer for error messages
 * @retval enum SearchResult, e.g. #SR_SEARCH_MATCHES
 */
enum SearchResult spager_search_search(struct SimplePagerSearch *sps,
                                       const char *pattern, int start_index,
                                       enum SearchDirection direction, struct Buffer *err)
{
  if (!sps || !pattern || !sps->pra)
    return SR_SEARCH_ERROR;

  if (sps->compiled)
  {
    FREE(&sps->pattern);
    regfree(&sps->regex);
  }

  int rflags = REG_NEWLINE;
  if (mutt_mb_is_lower(pattern))
    rflags |= REG_ICASE;

  int rc = REG_COMP(&sps->regex, pattern, rflags);
  if (rc != 0)
  {
    regerror(rc, &sps->regex, err->data, err->dsize);
    return SR_SEARCH_ERROR;
  }

  sps->compiled = true;
  sps->pattern = mutt_str_dup(pattern);

  struct PagedRow *pr = NULL;
  ARRAY_FOREACH(pr, sps->pra)
  {
    ARRAY_SHRINK(&pr->search, ARRAY_SIZE(&pr->search));
  }

  ARRAY_FOREACH(pr, sps->pra)
  {
    const char *text = paged_row_get_virtual_text(pr, NULL);
    if (!text)
      continue;

    int offset = 0;
    int flags = REG_NOTBOL;
    regmatch_t pmatch = { 0 };

    while (true)
    {
      rc = regexec(&sps->regex, text + offset, 1, &pmatch, flags);
      if (rc != 0)
        break;

      mutt_debug(LL_DEBUG1, "match for %s, row %d, offset %d\n", sps->pattern,
                 ARRAY_FOREACH_IDX_pr, pmatch.rm_so);
      paged_row_add_search(pr, offset + pmatch.rm_so, pmatch.rm_eo - pmatch.rm_so + 1);

      if (pmatch.rm_eo == pmatch.rm_so)
        offset++; /* avoid degenerate cases */
      else
        offset += pmatch.rm_eo;
      if (text[offset] == '\0')
        break;
    }
  }

  return SR_SEARCH_ERROR;
}

/**
 * spager_search_next - Find the next match
 * @param[in]  sps       Search Object
 * @param[in]  start_row Row to start searching
 * @param[in]  direction Direction in which to search, e.g. #SD_SEARCH_FORWARDS
 * @param[out] next_index Row Index of next match
 * @param[out] next_seg   Segment of next match
 * @retval enum Result, e.g. #SR_SEARCH_MATCHES
 */
enum SearchResult spager_search_next(struct SimplePagerSearch *sps, int start_row,
                                     enum SearchDirection direction,
                                     int *next_index, int *next_seg)
{
  if (!sps || !sps->pattern || !sps->pra)
    return SR_SEARCH_ERROR;

  if (direction == SD_SEARCH_FORWARDS)
  {
    struct PagedRow *pr = NULL;
    ARRAY_FOREACH_FROM(pr, sps->pra, start_row + 1)
    {
      if (ARRAY_SIZE(&pr->search) == 0)
        continue;

      *next_index = ARRAY_FOREACH_IDX_pr;
      return SR_SEARCH_MATCHES;
    }

    ARRAY_FOREACH_TO(pr, sps->pra, start_row - 1)
    {
      if (ARRAY_SIZE(&pr->search) == 0)
        continue;

      *next_index = ARRAY_FOREACH_IDX_pr;
      return SR_SEARCH_MATCHES;
    }
  }
  else
  {
    struct PagedRow *pr = NULL;
    int count = ARRAY_SIZE(sps->pra);

    for (int i = start_row - 1; i >= 0; i--)
    {
      pr = ARRAY_GET(sps->pra, i);
      if (ARRAY_SIZE(&pr->search) == 0)
        continue;

      *next_index = i;
      return SR_SEARCH_MATCHES;
    }

    for (int i = count - 1; i > start_row; i--)
    {
      pr = ARRAY_GET(sps->pra, i);
      if (ARRAY_SIZE(&pr->search) == 0)
        continue;

      *next_index = i;
      return SR_SEARCH_MATCHES;
    }
  }

  return SR_SEARCH_ERROR;
}
