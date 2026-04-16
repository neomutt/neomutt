/**
 * @file
 * History private Module data
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

#ifndef MUTT_HISTORY_MODULE_DATA_H
#define MUTT_HISTORY_MODULE_DATA_H

#include "lib.h"

/**
 * struct History - Saved list of user-entered commands/searches
 *
 * This is a ring buffer of strings.
 */
struct History
{
  char  **hist;  ///< Array of history items
  short   cur;   ///< Current history item
  short   last;  ///< Last history item
};

/**
 * struct HistoryModuleData - History private Module data
 */
struct HistoryModuleData
{
  struct Notify  *notify;             ///< Notifications
  struct History histories[HC_MAX];   ///< Command histories, one for each #HistoryClass
  int            old_size;            ///< The previous number of history entries to save
};

#endif /* MUTT_HISTORY_MODULE_DATA_H */
