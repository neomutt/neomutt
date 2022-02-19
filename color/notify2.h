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

#ifndef MUTT_COLOR_NOTIFY2_H
#define MUTT_COLOR_NOTIFY2_H

#include "config.h"
#include "mutt/lib.h"
#include "color.h"

extern struct Notify *ColorsNotify;

/**
 * enum NotifyColor - Types of Color Event
 *
 * Observers of #NT_COLOR will be passed an #EventColor.
 *
 * @note Notifications are sent **after** the event.
 */
enum NotifyColor
{
  NT_COLOR_SET = 1, ///< Color has been set
  NT_COLOR_RESET,   ///< Color has been reset/removed
};

/**
 * struct EventColor - An Event that happened to a Colour
 *
 * Observers will be passed a type of #NT_COLOR and a subtype of
 * #NT_COLOR_SET or #NT_COLOR_RESET with a struct which
 * describes the colour, e.g. #MT_COLOR_SIDEBAR_HIGHLIGHT.
 */
struct EventColor
{
  enum ColorId cid;             ///< Colour ID that has changed
  struct AttrColor *attr_color; ///< Colour object that has changed
};

void color_notify_init(void);
void color_notify_free(void);

void mutt_color_observer_add   (observer_t callback, void *global_data);
void mutt_color_observer_remove(observer_t callback, void *global_data);

#endif /* MUTT_COLOR_NOTIFY2_H */
