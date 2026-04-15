/**
 * @file
 * Browser private Module data
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

#ifndef MUTT_BROWSER_MODULE_DATA_H
#define MUTT_BROWSER_MODULE_DATA_H

#include "mutt/lib.h"

/**
 * struct BrowserModuleData - Browser private Module data
 */
struct BrowserModuleData
{
  struct MenuDefinition *menu_browser; ///< Browser menu definition
  struct Buffer last_dir;              ///< Browser: previous selected directory
  struct Buffer last_dir_backup;       ///< Browser: backup copy of the current directory
};

#endif /* MUTT_BROWSER_MODULE_DATA_H */
