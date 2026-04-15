/**
 * @file
 * Gui private Module data
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

#ifndef MUTT_GUI_MODULE_DATA_H
#define MUTT_GUI_MODULE_DATA_H

#include <stdbool.h>

struct MenuDefinition;
struct MuttWindow;
struct SubMenu;

/**
 * struct GuiModuleData - Gui private Module data
 */
struct GuiModuleData
{
  struct MenuDefinition *md_generic;     ///< Generic Menu Definition
  struct MenuDefinition *md_dialog;      ///< Dialog Menu Definition
  struct SubMenu *sm_generic;            ///< Generic functions
  struct SubMenu *sm_dialog;             ///< Dialog functions
  struct MuttWindow *all_dialogs_window; ///< Parent of all Dialogs
  struct MuttWindow *message_container;  ///< Message Container Window
  struct MuttWindow *root_window;        ///< Parent of all Windows
  bool ts_supported;                     ///< Terminal Setting is supported
};

#endif /* MUTT_GUI_MODULE_DATA_H */
