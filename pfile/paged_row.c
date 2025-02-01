/**
 * @file
 * A Row of marked up text for the Simple Pager
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
 * @page pfile_paged_row Row of marked up text for the Simple Pager
 *
 * A Row of marked up text for the Simple Pager
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "paged_row.h"
#include "color/lib.h"
#include "paged_file.h"
#include "paged_text.h"

/**
 * paged_row_clear - Clear a PagedRow
 * @param pr PagedRow to clear
 *
 * Free the contents of a PagedRow, but not the object itself.
 * We don't own attr_color, so don't free it.
 */
void paged_row_clear(struct PagedRow *pr)
{
  if (!pr)
    return;

  FREE(&pr->cached_text);
  ARRAY_FREE(&pr->segments);
}

/**
 * paged_row_add_newline - Add a newline to a PagedRow
 * @param pr PagedRow to add to
 * @retval num Number of screen columns used
 */
int paged_row_add_newline(struct PagedRow *pr)
{
  if (!pr || !pr->paged_file)
    return 0;

  // The GUI doesn't care about newlines,
  // so add it to the file, but don't adjust the counters.
  fputs("\n", pr->paged_file->fp);

  return 0;
}

/**
 * paged_row_add_raw_text - Add raw text to a PagedRow
 * @param pr   PagedRow to add to
 * @param text Text to add
 * @retval num Number of screen columns used
 */
int paged_row_add_raw_text(struct PagedRow *pr, const char *text)
{
  if (!pr || !pr->paged_file || !text || (text[0] == '\0'))
    return 0;

  fputs(text, pr->paged_file->fp);

  // We don't alter num_cols as this text won't be displayed
  pr->num_bytes += mutt_str_len(text);

  return 0;
}

/**
 * paged_row_add_text - Add some plain text to a PagedRow
 * @param pr   PagedRow to add to
 * @param text Text to add
 * @retval num Number of screen columns used
 */
int paged_row_add_text(struct PagedRow *pr, const char *text)
{
  if (!pr || !pr->paged_file || !text)
    return 0;

  int bytes = mutt_str_len(text);
  int cols = mutt_strwidth(text);

  fputs(text, pr->paged_file->fp);

  pr->num_bytes += bytes;
  pr->num_cols += cols;

  return cols;
}

/**
 * paged_row_add_multirow - Add some multirow text to a PagedRow
 * @param pf   PagedFile to write to
 * @param text Text to add
 * @retval num Number of rows used
 */
int paged_row_add_multirow(struct PagedFile *pf, const char *text)
{
  if (!pf || !text)
    return 0;

  const char *ptr = NULL;
  struct PagedRow *pr = NULL;
  int count = 0;

  while ((ptr = strchr(text, '\n')))
  {
    pr = paged_file_new_row(pf);

    pr->num_bytes = ptr - text;
    pr->num_cols = mutt_strnwidth(text, pr->num_bytes);
    pr->num_bytes++; // Count the newline, but don't measure it

    if (fwrite(text, pr->num_bytes, 1, pf->fp) != 1)
      break;

    text = ptr + 1;
    count++;
  }

  if (text[0] != '\0')
  {
    pr = paged_file_new_row(pf);

    pr->num_bytes = mutt_str_len(text);
    pr->num_cols = mutt_strnwidth(text, pr->num_bytes);
    fputs(text, pf->fp);
    count++;
  }

  return count;
}

/**
 * paged_row_add_colored_text - Add some coloured text to a PagedRow
 * @param pr   PagedRow to add to
 * @param cid  Colour ID
 * @param text Text to add
 * @retval num Number of screen columns used
 */
int paged_row_add_colored_text(struct PagedRow *pr, int cid, const char *text)
{
  if (!pr || !pr->paged_file || !text)
    return 0;

  struct PagedTextMarkup *ptm = paged_text_markup_new(&pr->text);

  int bytes = mutt_str_len(text);
  int cols = mutt_strwidth(text);

  fputs(text, pr->paged_file->fp);

  ptm->first = pr->num_bytes;
  ptm->last = ptm->first + bytes;
  ptm->cid = cid;

  pr->num_bytes += bytes;
  pr->num_cols += cols;

  return cols;
}

/**
 * paged_row_add_ac_text - Add some AttrColor text to a PagedRow
 * @param pr   PagedRow to add to
 * @param ac   Colour
 * @param text Text to add
 * @retval num Number of screen columns used
 */
int paged_row_add_ac_text(struct PagedRow *pr, struct AttrColor *ac, const char *text)
{
  if (!pr || !pr->paged_file || !text)
    return 0;

  struct PagedTextMarkup *ptm = paged_text_markup_new(&pr->text);

  int bytes = mutt_str_len(text);
  int cols = mutt_strwidth(text);

  fputs(text, pr->paged_file->fp);

  ptm->first = pr->num_bytes;
  ptm->last = ptm->first + bytes;
  ptm->cid = MT_COLOR_NONE;
  ptm->ac_text = ac;

  pr->num_bytes += bytes;
  pr->num_cols += cols;

  return cols;
}

/**
 * paged_row_add_search - Add a search match to a PagedRow
 * @param pr   PagedRow to add to
 * @param first First matching character
 * @param last  Last matching character (not included)
 */
void paged_row_add_search(struct PagedRow *pr, int first, int last)
{
  if (!pr || !pr->paged_file)
    return;

  struct PagedTextMarkup *ptm = paged_text_markup_new(&pr->search);

  ptm->first = first;
  ptm->last = last;
  ptm->cid = MT_COLOR_SEARCH;
}

/**
 * paged_row_cache - Read and cache a Row of the File
 * @param pr PagedRow
 */
void paged_row_cache(struct PagedRow *pr)
{
  if (!pr || pr->cached_text)
    return;

  FILE *fp = pr->paged_file->fp;
  char *text = NULL;
  size_t size = 0;

  fseek(fp, pr->offset, SEEK_SET);
  pr->cached_text = mutt_file_read_line(text, &size, fp, NULL, MUTT_RL_NO_FLAGS);

  pr->num_bytes = mutt_str_len(pr->cached_text);
  pr->num_cols = mutt_strwidth(pr->cached_text);
}

/**
 * paged_row_wrap - Wrap the text of a Row
 * @param pr    PagedRow
 * @param width Width in screen columns
 * @param flags Control the wrapping
 */
void paged_row_wrap(struct PagedRow *pr, int width, RowWrapFlags flags)
{
  if (!pr)
    return;

  ARRAY_FREE(&pr->segments);
  if (pr->num_cols <= width)
    return;

  mutt_debug(LL_DEBUG1, "WRAP: %d into %d\n", pr->num_cols, width);

  paged_row_cache(pr);

  int total_bytes = 0;
  int total_cols = 0;

  size_t bytes = 0;
  size_t text_len = pr->num_bytes;
  size_t cols = 0;

  mutt_debug(LL_DEBUG1, "Wrapping: %s\n", pr->cached_text);
  mutt_debug(LL_DEBUG1, "%d bytes, %d cols\n", pr->num_bytes, pr->num_cols);

  while (total_bytes < text_len)
  {
    struct Segment seg = { total_bytes, total_cols };
    ARRAY_ADD(&pr->segments, seg);

    bytes = mutt_wstr_trunc(pr->cached_text + total_bytes,
                            text_len - total_bytes, width, &cols);

    if ((total_bytes == 0) && (flags & RW_MARKERS))
      width--;

    total_bytes += bytes;
    total_cols += cols;
  }

  struct Segment *seg = NULL;
  struct PagedFile *pf = pr->paged_file;
  struct PagedRowArray *pra = &pf->rows;
  ARRAY_FOREACH(seg, &pr->segments)
  {
    size_t pr_index = ARRAY_IDX(pra, pr);
    mutt_debug(LL_DEBUG1, "Row %ld -- Segment %d: %d bytes, %d cols\n", pr_index,
               ARRAY_FOREACH_IDX_seg, seg->offset_bytes, seg->offset_cols);
  }
}

/**
 * paged_row_get_text - Get the text for a Row
 * @param pr PagedRow
 */
const char *paged_row_get_text(struct PagedRow *pr)
{
  if (!pr)
    return NULL;

  paged_row_cache(pr);

  if (pr->cached_text)
    return pr->cached_text;

  return NULL;
}

/**
 * paged_row_get_virtual_text - Get the text for a Row
 * @param pr  PagedRow
 * @param seg Segment
 */
const char *paged_row_get_virtual_text(struct PagedRow *pr, struct Segment *seg)
{
  if (!pr)
    return NULL;

  paged_row_cache(pr);

  if (!pr->cached_text)
    return NULL;

  if (seg)
    return pr->cached_text + seg->offset_bytes;

  return pr->cached_text;
}

/**
 * paged_rows_count_virtual_rows - Count the number of Rows including wrapping
 * @param[in] pra Array of Rows
 */
int paged_rows_count_virtual_rows(struct PagedRowArray *pra)
{
  if (!pra)
    return 0;

  int count = 0;

  struct PagedRow *pr = NULL;
  ARRAY_FOREACH(pr, pra)
  {
    size_t num_segs = ARRAY_SIZE(&pr->segments);
    if (num_segs == 0)
      count++;
    else
      count += num_segs;
  }

  return count;
}

/**
 * paged_rows_find_virtual_row - Find one of an array of wrapped Rows
 * @param[in]  pra       Array of Rows
 * @param[in]  virt_row  Row number
 * @param[out] pr_index  Index of the Row
 * @param[out] seg_index Index of the Segment
 */
bool paged_rows_find_virtual_row(struct PagedRowArray *pra, int virt_row,
                                 int *pr_index, int *seg_index)
{
  if (!pra || !pr_index || !seg_index)
    return false;

  if (virt_row < 0)
  {
    // Give the user the first possible row
    *pr_index = 0;
    *seg_index = 0;
    return false;
  }

  int v = 0;
  struct PagedRow *pr = NULL;
  ARRAY_FOREACH(pr, pra)
  {
    size_t num_segs = ARRAY_SIZE(&pr->segments);
    if (num_segs == 0)
      num_segs++;

    if (virt_row < (v + num_segs))
    {
      *pr_index = ARRAY_FOREACH_IDX_pr;
      *seg_index = virt_row - v;
      return true;
    }

    v += num_segs;
  }

  // Give the user the last possible virtual row
  if (ARRAY_SIZE(pra) > 0)
    *pr_index = ARRAY_SIZE(pra) - 1;
  else
    *pr_index = -1;

  if (pr && (ARRAY_SIZE(&pr->segments) > 0))
    *seg_index = ARRAY_SIZE(&pr->segments) - 1;
  else
    *seg_index = -1;

  return false;
}

/**
 * paged_rows_wrap - Wrap the text of an array of Rows
 * @param pra   Array of Rows
 * @param width Width in screen columns
 * @param flags Control the wrapping
 */
void paged_rows_wrap(struct PagedRowArray *pra, int width, RowWrapFlags flags)
{
  if (!pra)
    return;

  struct PagedRow *pr = NULL;
  ARRAY_FOREACH(pr, pra)
  {
    paged_row_wrap(pr, width, flags);
  }
}
