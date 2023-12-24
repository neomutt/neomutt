/**
 * @file
 * Test code for Simple Colours
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
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "color/lib.h"
#include "test_common.h"

static struct ConfigDef Vars[] = {
  // clang-format off
  { "color_directcolor", DT_BOOL, true, 0, NULL, },
  { NULL },
  // clang-format on
};

void test_simple_colors(void)
{
  MuttLogger = log_disp_null;

  TEST_CHECK(cs_register_variables(NeoMutt->sub->cs, Vars));

  simple_colors_init();

  struct AttrColor *ac = NULL;

  ac = simple_color_get(MT_COLOR_NONE - 10);
  TEST_CHECK(ac == NULL);

  ac = simple_color_get(MT_COLOR_PROMPT);
  TEST_CHECK(ac != NULL);

  bool set = simple_color_is_set(MT_COLOR_PROMPT);
  TEST_CHECK(!set);

  ac = simple_color_get(MT_COLOR_MAX + 10);
  TEST_CHECK(ac == NULL);

  TEST_CHECK(simple_color_is_header(MT_COLOR_HEADER));
  TEST_CHECK(!simple_color_is_header(MT_COLOR_QUOTED));

  ac = simple_color_set(MT_COLOR_MAX + 10, NULL);
  TEST_CHECK(ac == NULL);

  simple_color_reset(MT_COLOR_MAX + 10);

  struct AttrColor ac2 = { 0 };

  ac2.fg.color = COLOR_RED;
  ac2.fg.type = CT_SIMPLE;
  ac2.bg.color = COLOR_CYAN;
  ac2.bg.type = CT_SIMPLE;
  ac2.attrs = A_BOLD;
  ac2.fg.prefix = COLOR_PREFIX_ALERT;
  simple_color_set(MT_COLOR_INDICATOR, &ac2);

  simple_color_reset(MT_COLOR_INDICATOR);

  simple_colors_cleanup();
}
