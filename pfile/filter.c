/**
 * @file
 * Filter XXX
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
 * @page pfile_filter_ansi XXX
 *
 * XXX
 */

#include "mutt/lib.h"
#include "filter.h"
#include "color/lib.h"
#include "paged_row.h"

/**
 * filter_new - XXX
 */
struct Filter *filter_new(void)
{
  struct Filter *fil = MUTT_MEM_CALLOC(1, struct Filter);

  return fil;
}

/**
 * filter_free - XXX
 */
void filter_free(struct Filter **pptr)
{
  if (!pptr || !*pptr)
    return;

  struct Filter *fil = *pptr;

  if (fil->fdata_free)
    fil->fdata_free(&fil->fdata);

  FREE(pptr);
}

/**
 * filter_ansi_fdata_free - XXX
 */
void filter_ansi_fdata_free(void **pptr)
{
  FREE(pptr);
}

/**
 * filter_ansi_apply - XXX
 */
void filter_ansi_apply(struct Filter *fil, struct PagedRow *pr)
{
#if 0
  struct PagedTextMarkup *ptm = ARRAY_GET(&pr->text, 2);
  ARRAY_REMOVE(&pr->text, ptm);
  ptm = ARRAY_GET(&pr->text, 0);
  ARRAY_REMOVE(&pr->text, ptm);
  pr->text.entries[0].cid = MT_COLOR_INDICATOR;
  pr->num_bytes -= 11;
#endif

#if 0
  // markup_dump(&pr->text, -1, -1);
  markup_delete(&pr->text, 0, 7);
  // markup_dump(&pr->text, -1, -1);
  markup_apply(&pr->text, 0, 33, MT_COLOR_INDICATOR);
  // markup_dump(&pr->text, -1, -1);
  markup_delete(&pr->text, 33, 4);
  // markup_dump(&pr->text, -1, -1);
  mutt_debug(LL_DEBUG1, "FILTER: %ld\n", pr->offset);
  pr->num_bytes -= 11;
#endif

  const char *plain = paged_row_get_plain(pr);
  mutt_debug(LL_DEBUG1, "Plain: %s\n", plain);

  struct AnsiFilterData *afd = fil->fdata;
  const bool c_allow_ansi = true;

  for (int i = 0; plain[i] != '\0'; i++)
  {
    if (plain[i] != '\033') // Escape
      continue;

    int len = ansi_color_parse(plain + i, afd->ansi, &afd->ansi_list, !c_allow_ansi);
    mutt_debug(LL_DEBUG1, "ANSI: off %d, len %d\n", i, len);
    markup_delete(&pr->text, i, len);
    i += len;
    pr->num_bytes -= len;
  }

  FREE(&plain);
}

/**
 * filter_ansi_new - XXX
 */
struct Filter *filter_ansi_new(void)
{
  struct Filter *fil = filter_new();

  fil->fdata = MUTT_MEM_CALLOC(1, struct AnsiFilterData);
  fil->fdata_free = filter_ansi_fdata_free;
  fil->apply = filter_ansi_apply;

  return fil;
}
