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
#include <stdint.h>
#include "notify_type.h"
#include "queue.h"

/**
 * struct NotifyCallback - Data passed to a notification function
 */
struct NotifyCallback
{
  void *obj;         ///< Notify: Event happened here
  int obj_type;      ///< Notify: type of object event happened on
  int event_type;    ///< Send: event type
  int event_subtype; ///< Send: event subtype
  intptr_t event;    ///< Send: event data
  int flags;         ///< Observer: determine event data
  intptr_t data;     ///< Observer: private to observer
};

/**
 * typedef observer_t - Prototype for a notification callback function
 * @param[in]  flags    Flags, see #MuttFormatFlags
 * @retval  0 Success
 * @retval -1 Error
 */
typedef int (*observer_t)(struct NotifyCallback *nc);

typedef uint8_t ObserverFlags;     ///< Flags, e.g. #OBSERVE_RECURSIVE
#define OBSERVE_NO_FLAGS        0  ///< No flags are set
#define OBSERVE_RECURSIVE (1 << 0) ///< Listen for events of children

/**
 * struct Observer - An observer of notifications
 */
struct Observer
{
  enum NotifyType type;  ///< Object to observe to, e.g. #NT_NEOMUTT
  ObserverFlags flags;   ///< Flags, e.g. #OBSERVE_RECURSIVE
  observer_t callback;   ///< Callback function for events
  intptr_t data;         ///< Private data to pass to callback
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
