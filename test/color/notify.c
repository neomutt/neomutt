/**
 * @file
 * Test code for Colour Notifications
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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

#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include <stddef.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "color/lib.h"

int color_observer(struct NotifyCallback *nc)
{
  if (!nc || !nc->event_data)
    return -1;

  struct EventColor *ev_c = nc->event_data;
  TEST_CHECK(ev_c->cid == MT_COLOR_INDICATOR);

  return 0;
}

void test_color_notify(void)
{
  color_notify_init();

  mutt_color_observer_add(color_observer, NULL);

  struct EventColor ev_c = { MT_COLOR_INDICATOR, NULL };
  notify_send(ColorsNotify, NT_COLOR, NT_COLOR_SET, &ev_c);

  mutt_color_observer_remove(color_observer, NULL);

  color_notify_cleanup();
}
