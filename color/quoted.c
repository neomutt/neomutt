/**
 * @file
 * Quoted-Email colours
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
 * @page color_quote Quoted-Email colours
 *
 * Manage the colours of quoted emails.
 */

#include "config.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "quoted.h"
#include "color.h"
#include "notify2.h"
#include "simple2.h"

static int NumQuotedColors = 0; ///< Number of colours for quoted email text

/**
 * quoted_color_observer - Notification that a Color has changed - Implements ::observer_t - @ingroup observer_api
 */
static int quoted_color_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_COLOR)
    return 0;
  if (!nc->event_data)
    return -1;

  struct EventColor *ev_c = nc->event_data;
  enum ColorId cid = ev_c->cid;

  if (!COLOR_QUOTED(cid))
    return 0;

  // Find the highest-numbered quotedN in use
  for (int i = MT_COLOR_QUOTED9; i >= MT_COLOR_QUOTED0; i--)
  {
    if (simple_color_is_set(i))
    {
      NumQuotedColors = i - MT_COLOR_QUOTED0 + 1;
      break;
    }
  }

  return 0;
}

/**
 * quoted_colors_init - Initialise the Quoted colours
 */
void quoted_colors_init(void)
{
  mutt_color_observer_add(quoted_color_observer, NULL);
}

/**
 * quoted_colors_reset - Reset the quoted-email colours
 */
void quoted_colors_reset(void)
{
  NumQuotedColors = 0;
}

/**
 * quoted_colors_cleanup - Cleanup the quoted-email colours
 */
void quoted_colors_cleanup(void)
{
  mutt_color_observer_remove(quoted_color_observer, NULL);
  quoted_colors_reset();
}

/**
 * quoted_colors_get - Return the color of a quote, cycling through the used quotes
 * @param q Quote level
 * @retval enum #ColorId, e.g. #MT_COLOR_QUOTED3
 */
struct AttrColor *quoted_colors_get(int q)
{
  if (NumQuotedColors == 0)
    return NULL;

  // If we have too few colours, cycle around
  q %= NumQuotedColors;

  return simple_color_get(MT_COLOR_QUOTED0 + q);
}

/**
 * quoted_colors_num_used - Return the number of used quotes
 * @retval num Number of used quotes
 */
int quoted_colors_num_used(void)
{
  return NumQuotedColors;
}
