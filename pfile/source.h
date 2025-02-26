/**
 * @file
 * Source XXX
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

#ifndef MUTT_PFILE_SOURCE_H
#define MUTT_PFILE_SOURCE_H

#include <stdbool.h>
#include <stdio.h>

/**
 * struct Source - XXX
 */
struct Source
{
  long  source_size;           ///< Total size of text stored

  char *cache;                 ///< Cache of the beginning of the file, @sa CacheMaxSize
  int   cache_size;            ///< Current size of the cache

  FILE *fp;                    ///< Temporary file for text
  bool  close_fp;              ///< Close the file on exit?
};

void           source_free(struct Source **pptr);
struct Source *source_new (FILE *fp);

long           source_add_text(struct Source *src, const char *text, int bytes);
const char *   source_get_text(struct Source *src, long offset);

#endif /* MUTT_PFILE_SOURCE_H */
