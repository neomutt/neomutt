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
 *
 * Each type lists the associated Event object
 */
enum NotifyType
{
  NT_ALL = 0,       ///< Register for all notifications
  NT_ACCOUNT,       ///< Account has changed,         #NotifyAccount, #EventAccount
  NT_COLOR,         ///< Colour has changed,          #ColorId,       #EventColor
  NT_COMMAND,       ///< A Command has been executed, #Command
  NT_CONFIG,        ///< Config has changed,          #NotifyConfig,  #EventConfig
  NT_CONTEXT,       ///< Context has changed,         #NotifyContext, #EventContext
  NT_EMAIL,         ///< Email has changed,           #NotifyEmail,   #EventEmail
  NT_GLOBAL,        ///< Not object-related,          #NotifyGlobal
  NT_HEADER,        ///< A header has changed,        #NotifyHeader   #EventHeader
  NT_MAILBOX,       ///< Mailbox has changed,         #NotifyMailbox, #EventMailbox
  NT_WINDOW,        ///< MuttWindow has changed,      #NotifyWindow,  #EventWindow
  NT_ALIAS,         ///< Alias has changed,           #NotifyAlias,   #EventAlias
  NT_BINDING,       ///< Key binding has changed,     #NotifyBinding, #EventBinding
  NT_USER_INDEX,    ///< User action on the index,    #NotifyIndex,   #IndexEvent
};

#endif /* MUTT_LIB_NOTIFY_TYPE_H */
