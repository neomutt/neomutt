/**
 * @file
 * Colour used by libpager
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

#ifndef MUTT_PAGER_COLOR_H
#define MUTT_PAGER_COLOR_H

/**
 * enum ColorPager - Pager Dialog Colours
 *
 * @sa CD_PAGER, ColorDomain
 */
enum ColorPager
{
  CD_PAG_ATTACHMENT = 1,           ///< MIME attachments text (entire line)
  CD_PAG_ATTACH_HEADERS,           ///< MIME attachment test (takes a pattern)
  CD_PAG_BODY,                     ///< Highlight body of message (takes a pattern)
  CD_PAG_HDRDEFAULT,               ///< Header default colour
  CD_PAG_HEADER,                   ///< Message headers (takes a pattern)
  CD_PAG_MARKERS,                  ///< Markers, line continuation
  CD_PAG_SEARCH,                   ///< Search matches
  CD_PAG_SIGNATURE,                ///< Signature lines
  CD_PAG_TILDE,                    ///< Empty lines after message
};

/**
 * enum ColorQuoted - Quoted Text Colours
 *
 * @sa CD_QUOTED, ColorDomain
 */
enum ColorQuoted
{
  CD_QUO_LEVEL0 = 1,               ///< Quoted text, level 0
  CD_QUO_LEVEL1,                   ///< Quoted text, level 1
  CD_QUO_LEVEL2,                   ///< Quoted text, level 2
  CD_QUO_LEVEL3,                   ///< Quoted text, level 3
  CD_QUO_LEVEL4,                   ///< Quoted text, level 4
  CD_QUO_LEVEL5,                   ///< Quoted text, level 5
  CD_QUO_LEVEL6,                   ///< Quoted text, level 6
  CD_QUO_LEVEL7,                   ///< Quoted text, level 7
  CD_QUO_LEVEL8,                   ///< Quoted text, level 8
  CD_QUO_LEVEL9,                   ///< Quoted text, level 9
};

void pager_colors_init(void);

#endif /* MUTT_PAGER_COLOR_H */
