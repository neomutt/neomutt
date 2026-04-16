/**
 * @file
 * Menu private Module data
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

#ifndef MUTT_MENU_MODULE_DATA_H
#define MUTT_MENU_MODULE_DATA_H

#include "type.h"

/**
 * struct MenuModuleData - Menu private Module data
 */
struct MenuModuleData
{
  struct Notify *notify;                    ///< Notifications
  char          *search_buffers[MENU_MAX];  ///< Previous search string, one for each #MenuType
};

#endif /* MUTT_MENU_MODULE_DATA_H */
