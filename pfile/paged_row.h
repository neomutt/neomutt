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

#ifndef MUTT_PFILE_PAGED_ROW_H
#define MUTT_PFILE_PAGED_ROW_H

#include <stdbool.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "paged_text.h"

struct AttrColor;
struct PagedFile;

/**
 * typedef RowWrapFlags - Flags controlling the wrapping of text
 */
typedef uint8_t RowWrapFlags;        ///< Flags, e.g. #RW_NO_FLAGS
#define RW_NO_FLAGS               0   ///< No flags are set
#define RW_MARKERS         (1 <<  0)  ///< Display markers '+' at the beginning of wrapped rows
#define RW_WRAP            (1 <<  1)  ///< Should text be wrapped?
#define RW_SMART_WRAP      (1 <<  2)  ///< Should text be smart-wrapped? (wrapped at word boundaries)

/**
 * struct Segment - Part of a row of text
 */
struct Segment
{
  int offset_bytes;                     ///< Number of bytes into the row
  int offset_cols;                      ///< Number of screen columns into the row
};
ARRAY_HEAD(SegmentArray, struct Segment);

/**
 * struct PagedRow - A Row of text with markup
 *
 * One row of text with markup.
 */
struct PagedRow
{
  struct PagedFile *paged_file;         ///< Parent of the PagedRow
  long offset;                          ///< Offset into file (PagedFile.fp)

  int cid;                              ///< Default row colour, e.g. #MT_COLOR_SIGNATURE
  const struct AttrColor *ac_row;      ///< Curses colour of text
  const struct AttrColor *ac_merged;    ///< Default colour for the entire Window

  struct PagedTextMarkupArray text;     ///< Array of text with markup in the row
  struct PagedTextMarkupArray search;   ///< Array of search matches in the row

  char *cached_text;                    ///< Cached copy of the text
  int num_bytes;                        ///< Number of bytes (including newline)
  int num_cols;                         ///< Number of screen columns
  struct SegmentArray segments;         ///< Lengths of wrapped parts of the Row
};
ARRAY_HEAD(PagedRowArray, struct PagedRow);

void paged_row_clear(struct PagedRow *pr);

int paged_row_add_colored_text(struct PagedRow *pr, int cid, const char *text);
int paged_row_add_ac_text(struct PagedRow *pr, struct AttrColor *ac, const char *text);
int paged_row_add_newline     (struct PagedRow *pr);
int paged_row_add_raw_text(struct PagedRow *pr, const char *text);
int paged_row_add_text        (struct PagedRow *pr, const char *text);
int paged_row_add_multirow(struct PagedFile *pf, const char *text);

void paged_row_add_search     (struct PagedRow *pr, int first, int last);

void        paged_row_cache   (struct PagedRow *pr);
const char *paged_row_get_text(struct PagedRow *pr);
void        paged_row_wrap    (struct PagedRow *pr, int width, RowWrapFlags flags);
const char *paged_row_get_virtual_text(struct PagedRow *pr, struct Segment *seg);

int  paged_rows_count_virtual_rows(struct PagedRowArray *pra);
bool paged_rows_find_virtual_row  (struct PagedRowArray *pra, int virt_row, int *pr_index, int *seg_index);
void paged_rows_wrap              (struct PagedRowArray *pra, int width, RowWrapFlags flags);

#endif /* MUTT_PFILE_PAGED_ROW_H */
