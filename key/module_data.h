/**
 * @file
 * Key private Module data
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

#ifndef MUTT_KEY_MODULE_DATA_H
#define MUTT_KEY_MODULE_DATA_H

#include "get.h"
#include "keymap.h"
#include "menu.h"

/**
 * struct KeyModuleData - Key private Module data
 */
struct KeyModuleData
{
  struct Notify              *notify;             ///< Notifications
  struct KeyEventArray        macro_events;       ///< Macro event buffer
  struct KeyEventArray        unget_key_events;   ///< Unget key event buffer
  keycode_t                   abort_key;          ///< Key to abort prompts, normally Ctrl-G
  struct MenuDefinitionArray  menu_defs;          ///< All registered Menus
  struct SubMenuArray         sub_menus;          ///< All registered SubMenus
  struct Mapping             *key_names;          ///< Key name lookup table
};

#endif /* MUTT_KEY_MODULE_DATA_H */
