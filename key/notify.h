/**
 * @file
 * Key binding notifications
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

#ifndef MUTT_KEY_NOTIFY_H
#define MUTT_KEY_NOTIFY_H

#include "menu/lib.h"

/**
 * struct EventBinding - A key binding Event
 */
struct EventBinding
{
  enum MenuType menu; ///< Menu, e.g. #MENU_PAGER
  const char *key;    ///< Key string being bound (for new bind/macro)
  int op;             ///< Operation the key's bound to (for bind), e.g. OP_DELETE
};

/**
 * enum NotifyBinding - Key Binding notification types
 *
 * Observers of #NT_BINDING will be passed an #EventBinding.
 *
 * @note Notifications are sent **after** the event.
 */
enum NotifyBinding
{
  NT_BINDING_ADD = 1,    ///< Key binding has been added
  NT_BINDING_DELETE,     ///< Key binding has been deleted
  NT_MACRO_ADD,          ///< Key macro has been added
  NT_MACRO_DELETE,       ///< Key macro has been deleted
};

#endif /* MUTT_KEY_NOTIFY_H */
