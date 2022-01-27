/**
 * @file
 * Color and attribute parsing
 *
 * @authors
 * Copyright (C) 1996-2002,2012 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2020 R Primus <rprimus@gmail.com>
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
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "pattern/lib.h"
#include "context.h"
#include "mutt_globals.h"

const struct Mapping ColorNames[] = {
  // clang-format off
  { "black",   COLOR_BLACK },
  { "blue",    COLOR_BLUE },
  { "cyan",    COLOR_CYAN },
  { "green",   COLOR_GREEN },
  { "magenta", COLOR_MAGENTA },
  { "red",     COLOR_RED },
  { "white",   COLOR_WHITE },
  { "yellow",  COLOR_YELLOW },
  { "default", COLOR_DEFAULT },
  { 0, 0 },
  // clang-format on
};

/**
 * colors_clear - Reset all the simple, quoted and regex colours
 */
void colors_clear(void)
{
  mutt_debug(LL_NOTIFY, "NT_COLOR_RESET: [ALL]\n");
  struct EventColor ev_c = { MT_COLOR_MAX, NULL };
  notify_send(ColorsNotify, NT_COLOR, NT_COLOR_RESET, &ev_c);

  simple_colors_clear();
  quoted_colors_clear();
  regex_colors_clear();
}

/**
 * mutt_colors_cleanup - Cleanup all the colours
 */
void mutt_colors_cleanup(void)
{
  colors_clear();
  merged_colors_clear();
  color_notify_free();
}

/**
 * mutt_colors_init - Initialize colours
 */
void mutt_colors_init(void)
{
  color_notify_init();
  simple_colors_init();
  regex_colors_init();
  curses_colors_init();
  merged_colors_init();

  start_color();
  use_default_colors();

  notify_set_parent(ColorsNotify, NeoMutt->notify);
}
