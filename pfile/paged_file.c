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

/**
 * paged_file_free - Free a PagedFile
 * @param pptr PagedFile to free
 */
void paged_file_free(struct PagedFile **pptr)
{
  if (!pptr || !*pptr)
    return;

  struct PagedFile *pf = *pptr;

  if (pf->close_fp)
    mutt_file_fclose(&pf->fp);

  struct PagedRow *pr = NULL;
  ARRAY_FOREACH(pr, &pf->rows)
  {
    paged_row_clear(pr);
  }
  ARRAY_FREE(&pf->rows);

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
  bool close_fp = false;

  if (fp)
    close_fp = true;
  else
    fp = mutt_file_mkstemp();

  if (!fp)
  {
    mutt_perror(_("Can't create temporary file"));
    return NULL;
  }

  struct PagedFile *pf = MUTT_MEM_CALLOC(1, struct PagedFile);

  pf->fp = fp;
  pf->close_fp = close_fp;
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
  if (!pf || !pf->fp)
    return NULL;

  struct PagedRow pr = { 0 };
  pr.paged_file = pf;
  pr.offset = ftell(pf->fp);

  ARRAY_ADD(&pf->rows, pr);

  return ARRAY_LAST(&pf->rows);
}
