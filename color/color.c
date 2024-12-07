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
#include "domain.h"
#include "merged.h"
#include "notify2.h"
#include "pattern.h"
#include "quoted.h"
#include "regex4.h"
#include "simple2.h"

void compose_colors_init(void);
void index_colors_init(void);
void pager_colors_init(void);
void sidebar_colors_init(void);

/**
 * CoreColorDefs - Mapping of colour names to their IDs
 */
static const struct ColorDefinition CoreColorDefs[] = {
  // clang-format off
  { "bold",        CD_COR_BOLD,        CDT_SIMPLE },
  { "error",       CD_COR_ERROR,       CDT_SIMPLE },
  { "indicator",   CD_COR_INDICATOR,   CDT_SIMPLE },
  { "italic",      CD_COR_ITALIC,      CDT_SIMPLE },
  { "message",     CD_COR_MESSAGE,     CDT_SIMPLE },
  { "normal",      CD_COR_NORMAL,      CDT_SIMPLE },
  { "options",     CD_COR_OPTIONS,     CDT_SIMPLE },
  { "progress",    CD_COR_PROGRESS,    CDT_SIMPLE },
  { "prompt",      CD_COR_PROMPT,      CDT_SIMPLE },
  { "status",      CD_COR_STATUS,      CDT_REGEX, CDF_BACK_REF },
  { "stripe_even", CD_COR_STRIPE_EVEN, CDT_SIMPLE },
  { "stripe_odd",  CD_COR_STRIPE_ODD,  CDT_SIMPLE },
  { "tree",        CD_COR_TREE,        CDT_SIMPLE },
  { "underline",   CD_COR_UNDERLINE,   CDT_SIMPLE },
  { "warning",     CD_COR_WARNING,     CDT_SIMPLE },
  { NULL, 0 },
  // clang-format on
};

/**
 * core_colors_init - Initialise the Core Colours
 */
void core_colors_init(void)
{
  color_register_domain("core", CD_CORE);
  color_register_colors(CD_CORE, CoreColorDefs);
}

/**
 * colors_init - Initialize colours
 */
void colors_init(void)
{
  color_debug(LL_DEBUG5, "init\n");
  color_notify_init(NeoMutt->notify);

  curses_colors_init();
  merged_colors_init();
  quoted_colors_init();
  pattern_colors_init();
  regex_colors_init();
  simple_colors_init();

  start_color();
  use_default_colors();
  color_debug(LL_DEBUG5, "COLORS = %d, COLOR_PAIRS = %d\n", COLORS, COLOR_PAIRS);

  core_colors_init();
  compose_colors_init();
  index_colors_init();
  pager_colors_init();
  sidebar_colors_init();
}

/**
 * colors_reset - Reset all the simple, quoted and regex colours
 */
void colors_reset(void)
{
  color_debug(LL_DEBUG5, "reset\n");
  mutt_debug(LL_NOTIFY, "NT_COLOR_RESET: [ALL]\n");

  simple_colors_reset();
  quoted_colors_reset();
  pattern_colors_reset();
  regex_colors_reset();

  struct EventColor ev_c = { MT_COLOR_MAX, NULL };
  notify_send(ColorsNotify, NT_COLOR, NT_COLOR_RESET, &ev_c);
}

/**
 * colors_cleanup - Cleanup all the colours
 */
void colors_cleanup(void)
{
  simple_colors_cleanup();
  quoted_colors_cleanup();
  pattern_colors_cleanup();
  regex_colors_cleanup();
  merged_colors_cleanup();
  color_notify_cleanup();
}

/**
 * mutt_color_has_pattern - Check if a color object supports a NeoMutt pattern
 * @param cid   Object type, e.g. #MT_COLOR_INDEX
 * @retval true The color object supports patterns
 */
bool mutt_color_has_pattern(enum ColorId cid)
{
  return (cid == MT_COLOR_INDEX) || (cid == MT_COLOR_INDEX_AUTHOR) ||
         (cid == MT_COLOR_INDEX_COLLAPSED) || (cid == MT_COLOR_INDEX_DATE) ||
         (cid == MT_COLOR_INDEX_FLAGS) || (cid == MT_COLOR_INDEX_LABEL) ||
         (cid == MT_COLOR_INDEX_NUMBER) || (cid == MT_COLOR_INDEX_SIZE) ||
         (cid == MT_COLOR_INDEX_SUBJECT) || (cid == MT_COLOR_INDEX_TAG) ||
         (cid == MT_COLOR_INDEX_TAGS);
}

/**
 * mutt_color_has_regex - Check if a color object supports a regex
 * @param cid   Object type, e.g. #MT_COLOR_BODY
 * @retval true The color object supports regexes
 */
bool mutt_color_has_regex(enum ColorId cid)
{
  return (cid == MT_COLOR_ATTACH_HEADERS) || (cid == MT_COLOR_BODY) ||
         (cid == MT_COLOR_HEADER) || (cid == MT_COLOR_STATUS);
}
