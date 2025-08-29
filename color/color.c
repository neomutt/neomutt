/**
 * @file
 * Color and attribute parsing
 *
 * @authors
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020 R Primus <rprimus@gmail.com>
 * Copyright (C) 2021-2022 Pietro Cerutti <gahr@gahr.ch>
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
 * @page color_color Color and attribute parsing
 *
 * Color and attribute parsing
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "color.h"
#include "curses2.h"
#include "debug.h"
#include "merged.h"
#include "notify2.h"
#include "quoted.h"
#include "regex4.h"
#include "simple2.h"

/**
 * colors_cleanup - Reset all the simple, quoted and regex colours
 */
void colors_cleanup(void)
{
  color_debug(LL_DEBUG5, "clean up\n");
  mutt_debug(LL_NOTIFY, "NT_COLOR_RESET: [ALL]\n");
  struct EventColor ev_c = { MT_COLOR_MAX, NULL };
  notify_send(ColorsNotify, NT_COLOR, NT_COLOR_RESET, &ev_c);

  simple_colors_cleanup();
  quoted_colors_cleanup();
  regex_colors_cleanup();
}

/**
 * mutt_colors_cleanup - Cleanup all the colours
 */
void mutt_colors_cleanup(void)
{
  colors_cleanup();
  merged_colors_cleanup();
  color_notify_cleanup();
}

/**
 * mutt_colors_init - Initialize colours
 */
void mutt_colors_init(void)
{
  color_debug(LL_DEBUG5, "init\n");
  color_notify_init();

  curses_colors_init();
  merged_colors_init();
  quoted_colors_init();
  regex_colors_init();
  simple_colors_init();

  start_color();
  use_default_colors();
  color_debug(LL_DEBUG5, "COLORS = %d, COLOR_PAIRS = %d\n", COLORS, COLOR_PAIRS);

  notify_set_parent(ColorsNotify, NeoMutt->notify);
}

/**
 * mutt_color_has_pattern - Check if a color object supports a regex pattern
 * @param cid   Object type, e.g. #MT_COLOR_TILDE
 * @retval true The color object supports patterns
 */
bool mutt_color_has_pattern(enum ColorId cid)
{
  return (cid == MT_COLOR_ATTACH_HEADERS) || (cid == MT_COLOR_BODY) ||
         (cid == MT_COLOR_HEADER) || (cid == MT_COLOR_INDEX) ||
         (cid == MT_COLOR_INDEX_AUTHOR) || (cid == MT_COLOR_INDEX_COLLAPSED) ||
         (cid == MT_COLOR_INDEX_DATE) || (cid == MT_COLOR_INDEX_FLAGS) ||
         (cid == MT_COLOR_INDEX_LABEL) || (cid == MT_COLOR_INDEX_NUMBER) ||
         (cid == MT_COLOR_INDEX_SIZE) || (cid == MT_COLOR_INDEX_SUBJECT) ||
         (cid == MT_COLOR_INDEX_TAG) || (cid == MT_COLOR_INDEX_TAGS) ||
         (cid == MT_COLOR_STATUS);
}
