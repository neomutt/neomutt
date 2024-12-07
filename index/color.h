/**
 * @file
 * Colour used by libindex
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_INDEX_COLOR_H
#define MUTT_INDEX_COLOR_H

/**
 * enum ColorIndex - Index Dialog Colours
 *
 * @sa CD_INDEX, ColorDomain
 */
enum ColorIndex
{
  CD_IND_INDEX = 1,                ///< Default colour
  CD_IND_AUTHOR,                   ///< Author field
  CD_IND_COLLAPSED,                ///< Number of messages in collapsed thread
  CD_IND_DATE,                     ///< Date field
  CD_IND_FLAGS,                    ///< Flags field
  CD_IND_LABEL,                    ///< Label field
  CD_IND_NUMBER,                   ///< Index number
  CD_IND_SIZE,                     ///< Size field
  CD_IND_SUBJECT,                  ///< Subject field
  CD_IND_TAG,                      ///< Tag field (%G)
  CD_IND_TAGS,                     ///< Tags field (%g, %J)
};

void index_colors_init(void);

#endif /* MUTT_INDEX_COLOR_H */
