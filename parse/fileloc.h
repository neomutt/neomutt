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

#ifndef MUTT_PARSE_FILELOC_H
#define MUTT_PARSE_FILELOC_H

/**
 * struct FileLocation - Represents one config file being processed
 *
 * This structure tracks a single file location during config parsing,
 * containing the filename and the current line number being processed.
 */
struct FileLocation
{
  char *filename;  ///< Full path to the config file
  int lineno;      ///< Line number being processed (1-based)
};
ARRAY_HEAD(FileLocationArray, struct FileLocation);

void file_location_init (struct FileLocation *fl, const char *filename, int lineno);
void file_location_clear(struct FileLocation *fl);

#endif /* MUTT_PARSE_FILELOC_H */
