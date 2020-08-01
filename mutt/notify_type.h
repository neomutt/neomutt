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

#include <stdint.h>

typedef uint16_t NotifyType;   ///< Notification Types - Each type lists the associated Event object
#define NT_ALL             0   ///< Register for all notifications
#define NT_ACCOUNT  (1 <<  0)  ///< Account has changed,         #NotifyAccount, #EventAccount
#define NT_ALIAS    (1 <<  1)  ///< Alias has changed,           #NotifyAlias,   #EventAlias
#define NT_BINDING  (1 <<  2)  ///< Key binding has changed,     #NotifyBinding, #EventBinding
#define NT_COLOR    (1 <<  3)  ///< Colour has changed,          #ColorId,       #EventColor
#define NT_COMMAND  (1 <<  4)  ///< A Command has been executed, #Command
#define NT_CONFIG   (1 <<  5)  ///< Config has changed,          #NotifyConfig,  #EventConfig
#define NT_CONTEXT  (1 <<  6)  ///< Context has changed,         #NotifyContext, #EventContext
#define NT_EMAIL    (1 <<  7)  ///< Email has changed,           #NotifyEmail,   #EventEmail
#define NT_GLOBAL   (1 <<  8)  ///< Not object-related,          #NotifyGlobal
#define NT_MAILBOX  (1 <<  9)  ///< Mailbox has changed,         #NotifyMailbox, #EventMailbox
#define NT_WINDOW   (1 << 10)  ///< MuttWindow has changed,      #NotifyWindow,  #EventWindow

#endif /* MUTT_LIB_NOTIFY_TYPE_H */
