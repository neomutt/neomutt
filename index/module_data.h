/**
 * @file
 * Index private Module data
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

#ifndef MUTT_INDEX_MODULE_DATA_H
#define MUTT_INDEX_MODULE_DATA_H

#include "mutt/lib.h"

/**
 * struct IndexModuleData - Index private Module data
 */
struct IndexModuleData
{
  struct ReplaceList subject_rx_list;      /// List of subject-regex rules for modifying the Subject:
  struct Notify     *subject_rx_notify;    ///< Notifications: #NotifySubjectRx
};

#endif /* MUTT_INDEX_MODULE_DATA_H */
