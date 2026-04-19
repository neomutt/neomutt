/**
 * @file
 * Simple colour
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
 * @page color_simple Simple colour
 *
 * Manage the colours of the 'simple' graphical objects -- those that can only
 * have one colour, plus attributes.
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "attr.h"
#include "color.h"
#include "commands.h"
#include "debug.h"
#include "module_data.h"
#include "notify2.h"
#include "simple2.h"

/**
 * simple_colors_init - Initialise the simple colour definitions
 * @param simple_colors Array of simple colours to initialise
 */
void simple_colors_init(struct AttrColor *simple_colors)
{
  for (int i = 0; i < MT_COLOR_MAX; i++)
  {
    simple_colors[i].fg.color = COLOR_DEFAULT;
    simple_colors[i].bg.color = COLOR_DEFAULT;
  }

  // Set some defaults
  color_debug(LL_DEBUG5, "init indicator, markers, etc\n");
  simple_colors[MT_COLOR_BOLD].attrs = A_BOLD;
  simple_colors[MT_COLOR_INDICATOR].attrs = A_REVERSE;
  simple_colors[MT_COLOR_ITALIC].attrs = A_ITALIC;
  simple_colors[MT_COLOR_MARKERS].attrs = A_REVERSE;
  simple_colors[MT_COLOR_SEARCH].attrs = A_REVERSE;
  simple_colors[MT_COLOR_STATUS].attrs = A_REVERSE;
  simple_colors[MT_COLOR_STRIPE_EVEN].attrs = A_BOLD;
  simple_colors[MT_COLOR_UNDERLINE].attrs = A_UNDERLINE;
}

/**
 * simple_colors_reset - Reset the simple colour definitions
 * @param simple_colors Array of simple colours to reset
 */
void simple_colors_reset(struct AttrColor *simple_colors)
{
  color_debug(LL_DEBUG5, "reset defs\n");
  for (size_t i = 0; i < MT_COLOR_MAX; i++)
  {
    attr_color_clear(&simple_colors[i]);
  }
  simple_colors_init(simple_colors);
}

/**
 * simple_colors_cleanup - Cleanup the simple colour definitions
 * @param simple_colors Array of simple colours to cleanup
 */
void simple_colors_cleanup(struct AttrColor *simple_colors)
{
  simple_colors_reset(simple_colors);
}

/**
 * simple_color_get - Get the colour of an object by its ID
 * @param cid Colour ID, e.g. #MT_COLOR_SEARCH
 * @retval ptr AttrColor of the object
 *
 * @note Do not free the returned object
 */
struct AttrColor *simple_color_get(enum ColorId cid)
{
  if (cid >= MT_COLOR_MAX)
  {
    mutt_debug(LL_DEBUG1, "color overflow %d/%d\n", cid, MT_COLOR_MAX);
    return NULL;
  }
  if (cid <= MT_COLOR_NONE)
  {
    mutt_debug(LL_DEBUG1, "color underflow %d/%d\n", cid, MT_COLOR_NONE);
    return NULL;
  }

  struct ColorModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_COLOR);
  return &mod_data->simple_colors[cid];
}

/**
 * simple_color_is_set - Is the object coloured?
 * @param cid Colour ID, e.g. #MT_COLOR_SEARCH
 * @retval true Yes, a 'color' command has been used on this object
 */
bool simple_color_is_set(enum ColorId cid)
{
  return attr_color_is_set(simple_color_get(cid));
}

/**
 * simple_color_set - Set the colour of a simple object
 * @param cid    Colour ID, e.g. #MT_COLOR_SEARCH
 * @param ac_val Colour value to use
 * @retval ptr Colour
 */
struct AttrColor *simple_color_set(enum ColorId cid, struct AttrColor *ac_val)
{
  struct AttrColor *ac = simple_color_get(cid);
  if (!ac)
    return NULL;

  attr_color_overwrite(ac, ac_val);

  struct Buffer *buf = buf_pool_get();
  get_colorid_name(cid, buf);
  color_debug(LL_DEBUG5, "NT_COLOR_SET: %s\n", buf_string(buf));
  buf_pool_release(&buf);

  struct ColorModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_COLOR);
  struct EventColor ev_c = { cid, NULL };
  notify_send(mod_data->colors_notify, NT_COLOR, NT_COLOR_SET, &ev_c);

  return ac;
}

/**
 * simple_color_reset - Clear the colour of a simple object
 * @param cid Colour ID, e.g. #MT_COLOR_SEARCH
 */
void simple_color_reset(enum ColorId cid)
{
  struct AttrColor *ac = simple_color_get(cid);
  if (!ac)
    return;

  struct Buffer *buf = buf_pool_get();
  get_colorid_name(cid, buf);
  color_debug(LL_DEBUG5, "NT_COLOR_RESET: %s\n", buf_string(buf));
  buf_pool_release(&buf);

  attr_color_clear(ac);

  struct ColorModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_COLOR);
  struct EventColor ev_c = { cid, ac };
  notify_send(mod_data->colors_notify, NT_COLOR, NT_COLOR_RESET, &ev_c);
}
