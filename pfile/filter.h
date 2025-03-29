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

#ifndef MUTT_PFILE_FILTER_H
#define MUTT_PFILE_FILTER_H

#include "mutt/lib.h"
#include "color/lib.h"

struct PagedRow;

/**
 * struct Filter - XXX
 */
struct Filter
{
  struct Source *source;              ///< XXX

  void *fdata;                        ///< Filter specific data

  /**
   * @defgroup filter_fdata_free Filter Private Data API
   *
   * fdata_free - Free the private data attached to the Filter
   * @param pptr Private data to be freed
   *
   * @pre  pptr is not NULL
   * @pre *pptr is not NULL
   */
  void (*fdata_free)(void **pptr);

  /**
   * @defgroup apply Apply a Filter XXX
   *
   * apply - Apply a Filter XXX
   */
  void (*apply)(struct Filter *fil, struct PagedRow *row);
};
ARRAY_HEAD(FilterArray, struct Filter *);

/**
 * struct AnsiFilterData - XXX
 */
struct AnsiFilterData
{
  struct AnsiColor     ansi;         ///< Current ANSI colour
  struct AttrColorList ansi_list;    ///< List of ANSI colours used by the Filter
};

struct Filter *filter_ansi_new(void);
void           filter_ansi_fdata_free(void **pptr);
void           filter_ansi_apply(struct Filter *fil, struct PagedRow *pr);

#endif /* MUTT_PFILE_FILTER_H */
