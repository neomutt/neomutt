/**
 * @file
 * File Location
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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
 * @page parse_fileloc File Location
 *
 * File Location
 */

#include "config.h"
#include <stdio.h>
#include "mutt/lib.h"
#include "fileloc.h"

/**
 * file_location_init - Initialize a FileLocation
 * @param fl       FileLocation to initialize
 * @param filename Path to the file
 * @param lineno   Line number (1-based)
 */
void file_location_init(struct FileLocation *fl, const char *filename, int lineno)
{
  if (!fl)
    return;

  fl->filename = mutt_str_dup(filename);
  fl->lineno = lineno;
}

/**
 * file_location_clear - Free a FileLocation's contents
 * @param fl FileLocation to clear
 */
void file_location_clear(struct FileLocation *fl)
{
  if (!fl)
    return;

  FREE(&fl->filename);
}
