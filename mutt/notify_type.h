/**
 * @file
 * Notification Types
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_LIB_NOTIFY_TYPE_H
#define MUTT_LIB_NOTIFY_TYPE_H

/**
 * enum NotifyType - Notification Types
 */
enum NotifyType
{
  NT_ACCOUNT, ///< Account has changed
  NT_COLOR,   ///< Colour has changed
  NT_COMMAND, ///< A Command has been executed
  NT_CONFIG,  ///< Config has changed
  NT_CONTEXT, ///< Context has changed
  NT_EMAIL,   ///< Email has changed
  NT_GLOBAL,  ///< Not object-related
  NT_MAILBOX, ///< Mailbox has changed
};

#endif /* MUTT_LIB_NOTIFY_TYPE_H */
