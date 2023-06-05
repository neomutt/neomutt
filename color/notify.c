/**
 * @file
 * Colour notifications
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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
 * @page color_notify Colour notifications
 *
 * Manage the notifications of Colour changes.
 */

#include "config.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "lib.h"

struct Notify *ColorsNotify = NULL; ///< Notifications: #ColorId, #EventColor

/**
 * color_notify_init - Initialise the Colour notification
 */
void color_notify_init(void)
{
  ColorsNotify = notify_new();
  notify_set_parent(ColorsNotify, NeoMutt->notify);
}

/**
 * color_notify_cleanup - Free the Colour notification
 */
void color_notify_cleanup(void)
{
  notify_free(&ColorsNotify);
}

/**
 * mutt_color_observer_add - Add an observer
 * @param callback The callback
 * @param global_data The data
 */
void mutt_color_observer_add(observer_t callback, void *global_data)
{
  notify_observer_add(ColorsNotify, NT_COLOR, callback, global_data);
}

/**
 * mutt_color_observer_remove - Remove an observer
 * @param callback The callback
 * @param global_data The data
 */
void mutt_color_observer_remove(observer_t callback, void *global_data)
{
  notify_observer_remove(ColorsNotify, callback, global_data);
}
