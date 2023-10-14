/**
 * @file
 * Test code for ANSI Colour Parsing
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

static struct ConfigDef Vars[] = {
  // clang-format off
  { "color_directcolor", DT_BOOL, true, 0, NULL, },
  { NULL },
  // clang-format on
};

void test_ansi_color(void)
{
  // int ansi_color_parse(const char *str, struct AnsiColor *ansi, struct AttrColorList *acl, bool dry_run);

  TEST_CHECK(cs_register_variables(NeoMutt->sub->cs, Vars, DT_NO_FLAGS));

  curses_colors_init();
  COLOR_PAIRS = 256;

  const char *str = NULL;
  int rc;

  struct AttrColorList acl = TAILQ_HEAD_INITIALIZER(acl);
  struct AnsiColor ansi = { 0 };
  ansi.fg.color = COLOR_DEFAULT;
  ansi.bg.color = COLOR_DEFAULT;

  str = "\033[1;31m";
  rc = ansi_color_parse(str, &ansi, NULL, false);
  TEST_CHECK(rc == 7);

  str = "\033[4;31m";
  rc = ansi_color_parse(str, &ansi, &acl, false);
  TEST_CHECK(rc == 7);

  str = "\033[7;38;5;207m";
  rc = ansi_color_parse(str, &ansi, &acl, false);
  TEST_CHECK(rc == 13);

  str = "\033[3;38;2;0;0;6m";
  ansi.attrs = A_NORMAL;
  rc = ansi_color_parse(str, &ansi, &acl, false);
  TEST_CHECK(rc == 15);

  str = "\033[3;38;2;0;0;6m";
  rc = ansi_color_parse(str, &ansi, &acl, false);
  TEST_CHECK(rc == 15);

  str = "\033[48;2;0;0;6m";
  rc = ansi_color_parse(str, &ansi, &acl, false);
  TEST_CHECK(rc == 13);

  memset(&ansi, 0, sizeof(ansi));
  ansi.fg.color = COLOR_DEFAULT;
  ansi.bg.color = COLOR_DEFAULT;

  str = "";
  rc = ansi_color_parse(str, &ansi, &acl, false);
  TEST_CHECK(rc == 0);

  str = "\033[1m";
  ansi.attrs = A_NORMAL;
  rc = ansi_color_parse(str, &ansi, &acl, false);
  TEST_CHECK(rc == 4);

  str = "\033[3m";
  ansi.attrs = A_NORMAL;
  rc = ansi_color_parse(str, &ansi, &acl, false);
  TEST_CHECK(rc == 4);

  str = "\033[4m";
  ansi.attrs = A_NORMAL;
  rc = ansi_color_parse(str, &ansi, &acl, false);
  TEST_CHECK(rc == 4);

  attr_color_list_clear(&acl);
}
