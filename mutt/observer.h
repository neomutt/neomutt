/**
 * @file
 * Observer of notifications
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

/**
 * @page observer Observer of notifications
 *
 * Observer of notifications
 */

#ifndef MUTT_LIB_OBSERVER_H
#define MUTT_LIB_OBSERVER_H

#include <stdbool.h>
#include "notify_type.h"
#include "queue.h"

/**
 * struct NotifyCallback - Data passed to a notification function
 */
struct NotifyCallback
{
  enum NotifyType event_type;    ///< Send: Event type, e.g. #NT_ACCOUNT
  int             event_subtype; ///< Send: Event subtype, e.g. #NT_ACCOUNT_ADD
  void           *event_data;    ///< Data from notify_send()
  void           *global_data;   ///< Data from notify_observer_add()
};

/**
 * typedef observer_t - Prototype for a notification callback function
 * @param[in] nc Callback data
 * @retval  0 Success
 * @retval -1 Error
 */
typedef int (*observer_t)(struct NotifyCallback *nc);

/**
 * struct Observer - An observer of notifications
 */
struct Observer
{
  observer_t callback;   ///< Callback function for events
  void *global_data;     ///< Private data to pass to callback
};

/**
 * struct ObserverNode - List of Observers
 */
struct ObserverNode
{
  struct Observer *observer;          ///< An Observer
  STAILQ_ENTRY(ObserverNode) entries; ///< Linked list
};
STAILQ_HEAD(ObserverList, ObserverNode);

#endif /* MUTT_LIB_OBSERVER_H */
