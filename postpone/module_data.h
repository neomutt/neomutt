/**
 * @file
 * Postpone private Module data
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

#ifndef MUTT_POSTPONE_MODULE_DATA_H
#define MUTT_POSTPONE_MODULE_DATA_H

#include <stdbool.h>

/**
 * struct PostponeModuleData - Postpone private Module data
 */
struct PostponeModuleData
{
  struct Notify         *notify;                ///< Notifications
  struct MenuDefinition *menu_postpone;         ///< Postpone menu definition
  short                  post_count;            ///< Number of postponed (draft) emails
  bool                   update_num_postponed;  ///< When true, force recount of drafts
};

#endif /* MUTT_POSTPONE_MODULE_DATA_H */
