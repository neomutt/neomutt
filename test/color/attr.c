/**
 * @file
 * Test code for Attr Colours
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
#include <stdlib.h>
#include <string.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "color/lib.h"
#include "test_common.h"

color_t color_xterm256_to_24bit(const color_t color);
void modify_color_by_prefix(enum ColorPrefix prefix, bool is_fg, color_t *col, int *attrs);

static struct ConfigDef Vars[] = {
  // clang-format off
  { "color_directcolor", DT_BOOL, true, 0, NULL, },
  { NULL },
  // clang-format on
};

struct ModifyTest
{
  enum ColorPrefix prefix;
  bool is_fg;
  color_t col_before;
  int attrs_before;
  color_t col_after;
  int attrs_after;
};

void test_attr_colors(void)
{
  COLOR_PAIRS = 32;
  curses_colors_init();
  TEST_CHECK(cs_register_variables(NeoMutt->sub->cs, Vars, DT_NO_FLAGS));

  {
    attr_color_free(NULL);
  }

  {
    struct AttrColor *ac = attr_color_new();
    TEST_CHECK(ac != NULL);
    attr_color_free(&ac);
  }

  {
    struct AttrColor *ac = attr_color_new();
    TEST_CHECK(ac != NULL);

    struct AttrColor *ac_copy = ac;
    ac_copy->ref_count++;

    attr_color_free(&ac_copy);
    attr_color_free(&ac);
  }

  {
    attr_color_clear(NULL);
  }

  {
    color_t fg = rand() % (1 << 24); // Make up some arbitrary colours
    color_t bg = rand() % (1 << 24);

    struct CursesColor *cc = curses_color_new(fg, bg);
    struct AttrColor ac = { 0 };
    ac.attrs = A_BOLD;
    ac.curses_color = cc;

    TEST_CHECK(attr_color_is_set(NULL) == false);
    TEST_CHECK(attr_color_is_set(&ac) == true);

    struct AttrColor ac_copy = attr_color_copy(&ac);

    TEST_CHECK(memcmp(&ac, &ac_copy, sizeof(ac_copy)) == 0);

    attr_color_clear(&ac);
  }

  {
    attr_color_list_clear(NULL);

    struct AttrColorList acl = TAILQ_HEAD_INITIALIZER(acl);

    struct AttrColor *ac = attr_color_list_find(&acl, COLOR_RED, COLOR_RED, A_BOLD);
    TEST_CHECK(ac == NULL);

    color_t fg_find = COLOR_DEFAULT;
    color_t bg_find = COLOR_DEFAULT;
    int attrs_find = A_UNDERLINE;

    for (int i = 0; i < 10; i++)
    {
      color_t fg = rand(); // Make up some arbitrary colours
      color_t bg = rand();

      struct CursesColor *cc = NULL;
      if (i != 3)
        cc = curses_color_new(fg, bg);

      ac = attr_color_new();
      ac->curses_color = cc;
      ac->attrs = A_BOLD | A_ITALIC;

      if (i == 3)
        ac->attrs = attrs_find;

      if (i == 7)
      {
        fg_find = fg;
        bg_find = bg;
        ac->attrs = attrs_find;
      }

      TAILQ_INSERT_TAIL(&acl, ac, entries);
    }

    ac = attr_color_list_find(NULL, fg_find, bg_find, attrs_find);
    TEST_CHECK(ac == NULL);

    ac = attr_color_list_find(&acl, fg_find, bg_find, attrs_find);
    TEST_CHECK(ac != NULL);
    TEST_CHECK(ac->attrs == attrs_find);
    TEST_CHECK(ac->curses_color->fg == fg_find);
    TEST_CHECK(ac->curses_color->bg == bg_find);

    attr_color_list_clear(&acl);
  }

  {
    COLORS = 256;

    struct ModifyTest Tests[] = {
      // clang-format off
      { COLOR_PREFIX_NONE,   true,  COLOR_RED, A_NORMAL, COLOR_RED,     A_NORMAL         },
      { COLOR_PREFIX_ALERT,  true,  COLOR_RED, A_NORMAL, COLOR_RED,     A_BOLD | A_BLINK },
      { COLOR_PREFIX_LIGHT,  true,  COLOR_RED, A_NORMAL, COLOR_RED + 8, A_NORMAL         },
      { COLOR_PREFIX_LIGHT,  true,  123,       A_NORMAL, 123,           A_NORMAL         },
      { COLOR_PREFIX_BRIGHT, true,  COLOR_RED, A_NORMAL, COLOR_RED,     A_BOLD           },
      { COLOR_PREFIX_LIGHT,  false, COLOR_RED, A_NORMAL, COLOR_RED + 8, A_NORMAL         },
      { COLOR_PREFIX_LIGHT,  false, 123,       A_NORMAL, 123,           A_NORMAL         },
      // clang-format on
    };

    for (int i = 0; i < mutt_array_size(Tests); i++)
    {
      color_t col = Tests[i].col_before;
      int attrs = Tests[i].attrs_before;

      modify_color_by_prefix(Tests[i].prefix, Tests[i].is_fg, &col, &attrs);

      TEST_CHECK(col == Tests[i].col_after);
      TEST_MSG("[%d] Color Expected: 0x%06x, Got: 0x%06x", i, Tests[i].col_after, col);

      TEST_CHECK(attrs == Tests[i].attrs_after);
      TEST_MSG("[%d] Attrs Expected: 0x%06x, Got: 0x%06x", i, Tests[i].attrs_after, attrs);
    }
  }

  {
    COLORS = 8;

    struct ModifyTest Tests[] = {
      // clang-format off
      { COLOR_PREFIX_LIGHT,  true,  COLOR_RED, A_NORMAL, COLOR_RED, A_BOLD   },
      { COLOR_PREFIX_LIGHT,  true,  123,       A_NORMAL, 123,       A_BOLD   },
      { COLOR_PREFIX_BRIGHT, true,  COLOR_RED, A_NORMAL, COLOR_RED, A_BOLD   },
      { COLOR_PREFIX_LIGHT,  false, COLOR_RED, A_NORMAL, COLOR_RED, A_NORMAL },
      { COLOR_PREFIX_LIGHT,  false, 123,       A_NORMAL, 123,       A_NORMAL },
      // clang-format on
    };

    for (int i = 0; i < mutt_array_size(Tests); i++)
    {
      color_t col = Tests[i].col_before;
      int attrs = Tests[i].attrs_before;

      modify_color_by_prefix(Tests[i].prefix, Tests[i].is_fg, &col, &attrs);

      TEST_CHECK(col == Tests[i].col_after);
      TEST_MSG("[%d] Color Expected: 0x%06x, Got: 0x%06x", i, Tests[i].col_after, col);

      TEST_CHECK(attrs == Tests[i].attrs_after);
      TEST_MSG("[%d] Attrs Expected: 0x%06x, Got: 0x%06x", i, Tests[i].attrs_after, attrs);
    }
  }

  {
    color_t Colors[][2] = {
      // clang-format off
      { COLOR_DEFAULT, COLOR_DEFAULT },
      {   0, 0x000000 }, // Basic Colours
      {   1, 0x800000 },
      {  14, 0x00ffff },
      {  15, 0xffffff },
      {  16, 0x000000 }, // Palette Colours
      {  17, 0x00005f },
      { 230, 0xffffd7 },
      { 231, 0xffffff },
      { 232, 0x080808 }, // Greyscale Colours
      { 233, 0x121212 },
      { 254, 0xe4e4e4 },
      { 255, 0xeeeeee },
      // clang-format on
    };

    for (int i = 0; i < mutt_array_size(Colors); i++)
    {
      color_t col = color_xterm256_to_24bit(Colors[i][0]);
      TEST_CHECK(col == Colors[i][1]);
      TEST_MSG("[%d] %d", i, Colors[i][0]);
      TEST_MSG("Expected: 0x%06x, Got: 0x%06x", Colors[i][1], col);
    }
  }

  {
    cs_str_native_set(NeoMutt->sub->cs, "color_directcolor", false, NULL);
    color_t col = color_xterm256_to_24bit(123);
    TEST_CHECK(col == 123);
  }

  {
    struct CursesColor *cc = curses_color_new(123, 207);
    struct AttrColor ac1 = { 0 };
    ac1.fg.color = 0x800000;
    ac1.fg.type = CT_RGB;
    ac1.attrs = A_UNDERLINE;
    ac1.curses_color = cc;

    struct AttrColor ac2 = { 0 };
    ac2.fg.color = 0x800000;
    ac2.fg.type = CT_RGB;
    ac2.attrs = A_BOLD;
    ac2.curses_color = cc;

    TEST_CHECK(attr_color_match(NULL, NULL));
    TEST_CHECK(!attr_color_match(NULL, &ac2));
    TEST_CHECK(!attr_color_match(&ac1, NULL));
    TEST_CHECK(!attr_color_match(&ac1, &ac2));

    ac2.attrs = A_UNDERLINE;
    TEST_CHECK(attr_color_match(&ac1, &ac2));

    attr_color_overwrite(NULL, NULL);

    ac1.fg.color = 0x000004;
    ac1.bg.color = 0x000006;
    ac1.bg.type = CT_RGB;
    ac2.fg.color = 0x000005;
    ac2.bg.color = 0x000007;
    ac2.bg.type = CT_RGB;
    attr_color_overwrite(&ac1, &ac2);
  }
}
