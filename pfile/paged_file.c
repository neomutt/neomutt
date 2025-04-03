/**
 * @file
 * Backing File for the Simple Pager
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
 * @page pfile_paged_file Backing File for the Simple Pager
 *
 * Backing File for the Simple Pager
 */

#include "config.h"
#include "mutt/lib.h"
#include "core/lib.h"
#include "paged_file.h"
#include "paged_row.h"
#include "source.h"

/**
 * paged_file_free - Free a PagedFile
 * @param pptr PagedFile to free
 */
void paged_file_free(struct PagedFile **pptr)
{
  if (!pptr || !*pptr)
    return;

  struct PagedFile *pf = *pptr;

  struct PagedRow *pr = NULL;
  ARRAY_FOREACH(pr, &pf->rows)
  {
    paged_row_clear(pr);
  }
  ARRAY_FREE(&pf->rows);

  struct Filter **pfil = NULL;
  ARRAY_FOREACH(pfil, &pf->filters)
  {
    (*pfil)->fdata_free(NULL);
  }

  FREE(pptr);
}

/**
 * paged_file_new - Create a new PagedFile
 * @param fp File to use (OPTIONAL)
 * @retval ptr New PagedFile
 *
 * @note If fp is supplied, the caller must close it
 */
struct PagedFile *paged_file_new(FILE *fp)
{
  struct PagedFile *pf = MUTT_MEM_CALLOC(1, struct PagedFile);

  pf->source = source_new(fp);

  ARRAY_INIT(&pf->rows);

  return pf;
}

/**
 * paged_file_new_row - Create a new PagedRow in the PagedFile
 * @param pf PagedFile
 * @retval ptr New PagedRow
 *
 * @note Do not free the PagedRow, it's owned by the PagedFile
 */
struct PagedRow *paged_file_new_row(struct PagedFile *pf)
{
  if (!pf)
    return NULL;

  struct PagedRow pr = { 0 };
  pr.paged_file = pf;

  struct PagedRow *pr_prev = ARRAY_LAST(&pf->rows);
  if (pr_prev)
  {
    pr.offset = pr_prev->offset + pr_prev->num_bytes;
  }

  ARRAY_ADD(&pf->rows, pr);

  return ARRAY_LAST(&pf->rows);
}

/**
 * paged_file_add_filter - XXX
 */
void paged_file_add_filter(struct PagedFile *pf, struct Filter *fil)
{
  if (!pf || !fil)
    return;

  ARRAY_ADD(&pf->filters, fil);
}

/**
 * paged_file_get_row_from_source - XXX
 */
void paged_file_get_row_from_source(struct PagedFile *pf)
{
}

/**
 * paged_file_apply_filters - XXX
 */
void paged_file_apply_filters(struct PagedRow *pr)
{
  if (!pr || pr->valid)
    return;

  struct PagedFile *pf = pr->paged_file;

  struct Filter **pfil = NULL;
  ARRAY_FOREACH(pfil, &pf->filters)
  {
    struct Filter *fil = *pfil;

    fil->apply(fil, pr);
  }

  pr->valid = true;
}
