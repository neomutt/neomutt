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
   * @param ptr Private data to be freed
   *
   * @pre ptr  is not NULL
   * @pre *ptr is not NULL
   */
  void (*fdata_free)(void **ptr);

  /**
   * @defgroup filter_apply Apply a Filter XXX
   *
   * filter_apply - Apply a Filter XXX
   */
  void (*filter_apply)(void);
};

#endif /* MUTT_PFILE_FILTER_H */
