/**
 * @file
 * Colour notifications
 *
 * @authors
 * Copyright (C) 2021-2023 Richard Russon <rich@flatcap.org>
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
#include "mutt/lib.h"
#include "core/lib.h"
#include "module_data.h"
#include "notify2.h"

struct Notify;

/**
 * color_notify_init - Initialise the Colour notification
 * @param colors_notify Pointer to the notification object
 * @param parent        Parent notification object
 */
void color_notify_init(struct Notify **colors_notify, struct Notify *parent)
{
  *colors_notify = notify_new();
  notify_set_parent(*colors_notify, parent);
}

/**
 * color_notify_cleanup - Free the Colour notification
 * @param colors_notify Pointer to the notification object
 */
void color_notify_cleanup(struct Notify **colors_notify)
{
  notify_free(colors_notify);
}

/**
 * mutt_color_observer_add - Add an observer
 * @param callback The callback
 * @param global_data The data
 */
void mutt_color_observer_add(observer_t callback, void *global_data)
{
  struct ColorModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_COLOR);
  notify_observer_add(mod_data->colors_notify, NT_COLOR, callback, global_data);
}

/**
 * mutt_color_observer_remove - Remove an observer
 * @param callback The callback
 * @param global_data The data
 */
void mutt_color_observer_remove(observer_t callback, void *global_data)
{
  struct ColorModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_COLOR);
  notify_observer_remove(mod_data->colors_notify, callback, global_data);
}
