/**
 * @file
 * Generic menu format data wrapper
 *
 * @authors
 * Copyright (C) 2026 Copilot <223556219+Copilot@users.noreply.github.com>
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

#ifndef MUTT_MENU_FORMAT_DATA_H
#define MUTT_MENU_FORMAT_DATA_H

#include "lib.h"

/**
 * @defgroup menu_format_data Menu Format Data
 *
 * Data structures for passing menu context to format callbacks
 */

/**
 * struct MenuFormatData - Wrapper for menu item format data
 *
 * Contains a menu reference and current line number, allowing callbacks
 * to compute relative positioning
 */
struct MenuFormatData
{
  struct Menu *menu;  ///< Current menu
  int line;           ///< Current line being formatted
  void *data;         ///< Item-specific data (e.g., AliasView, Email)
};

/**
 * menu_relative_number - Calculate Vim-style relative numbering in a menu
 * @param mfd MenuFormatData wrapper
 * @retval num Current line number for the selection, otherwise absolute offset
 *
 * Returns the relative position of the current line from the selected item,
 * matching Vim's relative number display.
 * For example:
 * - Current selection at line 5: returns 6
 * - Line 3: returns 2
 * - Line 7: returns 2
 */
static inline long menu_relative_number(const struct MenuFormatData *mfd)
{
  if (!mfd || !mfd->menu)
    return 0;

  if (mfd->line == mfd->menu->current)
    return mfd->line + 1;

  return labs(mfd->line - mfd->menu->current);
}

#endif /* MUTT_MENU_FORMAT_DATA_H */
