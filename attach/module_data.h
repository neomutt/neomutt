/**
 * @file
 * Attach private Module data
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

#ifndef MUTT_ATTACH_MODULE_DATA_H
#define MUTT_ATTACH_MODULE_DATA_H

#include "mutt/lib.h"

/**
 * struct AttachModuleData - Attach private Module data
 */
struct AttachModuleData
{
  struct ListHead attach_allow;          ///< List of attachment types to be counted
  struct ListHead attach_exclude;        ///< List of attachment types to be ignored
  struct ListHead inline_allow;          ///< List of inline types to counted
  struct ListHead inline_exclude;        ///< List of inline types to ignore
  struct Notify  *attachments_notify;    ///< Notifications: #NotifyAttach

  struct ListHead mime_lookup_list;      ///< List of mime types that that shouldn't use the mailcap entry
  struct ListHead temp_attachments_list; ///< List of temporary files for displaying attachments
};

#endif /* MUTT_ATTACH_MODULE_DATA_H */
