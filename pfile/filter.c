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
void filter_ansi_apply(void)
{
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
