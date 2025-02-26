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
#include "gui/lib.h"
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
  const char *plain = paged_row_get_plain(pr);
  mutt_debug(LL_DEBUG1, "Plain: %s\n", plain);

  struct AnsiFilterData *afd = fil->fdata;
  const bool c_allow_ansi = true;

  int del = 0;
  int off = 0;
  int e_off = 0;
  while (plain && (plain[off] != '\0'))
  {
    const char *end = strchr(plain + off, '\033'); // Escape
    if (end)
      e_off = end - (plain + off);
    else
      e_off = strlen(plain + off);

    // Existing colour
    if ((e_off > 0) && afd->ansi.attr_color)
    {
      mutt_debug(LL_DEBUG1, "APPLY: off %d\n", e_off);
      markup_apply(&pr->text, off - del, e_off, MT_COLOR_NONE, afd->ansi.attr_color);
    }

    off += e_off;

    int len = ansi_color_parse(plain + off, &afd->ansi, &afd->ansi_list, !c_allow_ansi);
    mutt_debug(LL_DEBUG1, "ANSI: off %d, len %d\n", off, len);
    markup_delete(&pr->text, off - del, len);
    off += len;
    del += len;
  }

  int num_bytes = 0;
  int num_cols = 0;

  struct PagedTextMarkup *ptm = NULL;
  ARRAY_FOREACH(ptm, &pr->text)
  {
    num_bytes += ptm->bytes;
    num_cols += mutt_strnwidth(plain + ptm->first, ptm->bytes);
  }

  pr->num_bytes = num_bytes;
  pr->num_cols = num_cols;

  FREE(&plain);
}

/**
 * filter_ansi_new - XXX
 */
struct Filter *filter_ansi_new(void)
{
  struct Filter *fil = filter_new();

  struct AnsiFilterData *afd = MUTT_MEM_CALLOC(1, struct AnsiFilterData);
  TAILQ_INIT(&afd->ansi_list);

  fil->fdata = afd;
  fil->fdata_free = filter_ansi_fdata_free;
  fil->apply = filter_ansi_apply;

  return fil;
}
