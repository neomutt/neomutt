/**
 * @file
 * Notification API
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

#ifndef MUTT_LIB_NOTIFY_H
#define MUTT_LIB_NOTIFY_H

#include <stdbool.h>
#include "notify_type.h"
#include "observer.h"

/**
 * struct Notify - Notification API
 */
struct Notify
{
  struct Notify *parent;
  struct ObserverList observers;
};

struct Notify *notify_new(void);
void notify_free(struct Notify **ptr);
void notify_set_parent(struct Notify *notify, struct Notify *parent);

bool notify_send(struct Notify *notify, enum NotifyType event_type, int event_subtype, void *event_data);
bool notify_observer_add(struct Notify *notify, observer_t callback, void *global_data);
bool notify_observer_remove(struct Notify *notify, observer_t callback, void *global_data);

#endif /* MUTT_LIB_NOTIFY_H */
