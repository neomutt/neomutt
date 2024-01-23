/**
 * @file
 * Test code for Curses Colours
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
#include <stdlib.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "color/lib.h"
#include "test_common.h"

ARRAY_HEAD(CursesColorArray, struct CursesColor *);

void test_curses_colors(void)
{
  MuttLogger = log_disp_null;

  COLOR_PAIRS = 32;
  curses_colors_init();

  {
    // Degenerate test -- no colour
    struct CursesColor *cc = curses_color_new(COLOR_DEFAULT, COLOR_DEFAULT);
    TEST_CHECK(cc == NULL);
  }

  {
    // Create too many colours
    struct CursesColorArray cca = ARRAY_HEAD_INITIALIZER;

    for (int i = 0; i < 50; i++)
    {
      color_t fg = rand() % (1 << 24); // Make up some arbitrary colours
      color_t bg = rand() % (1 << 24);

      struct CursesColor *cc = curses_color_new(fg, bg);
      if (!cc)
        continue;

      ARRAY_ADD(&cca, cc);
    }

    TEST_CHECK(ARRAY_SIZE(&cca) == 16);

    struct CursesColor **ccp = NULL;
    ARRAY_FOREACH(ccp, &cca)
    {
      struct CursesColor *cc = *ccp;
      curses_color_free(&cc);
    }
    ARRAY_FREE(&cca);
  }

  {
    // Create and find colours
    struct CursesColor *cc = NULL;
    color_t fg = 0x800000;
    color_t bg = 0x008000;

    cc = curses_color_new(fg, bg);
    TEST_CHECK(cc != NULL);

    struct CursesColor *cc_copy = NULL;
    cc_copy = curses_color_new(fg, bg);
    TEST_CHECK(cc == cc_copy);

    struct CursesColor *cc_find = NULL;
    cc_find = curses_colors_find(bg, fg);
    TEST_CHECK(cc_find == NULL);

    cc_find = curses_colors_find(fg, bg);
    TEST_CHECK(cc_find == cc);

    curses_color_free(&cc_copy);
    curses_color_free(&cc);
  }

  {
    // Check the insertion / freeing of colours
    struct CursesColor *cc1 = curses_color_new(1, 1);
    struct CursesColor *cc2 = curses_color_new(2, 2);
    struct CursesColor *cc3 = curses_color_new(3, 3);
    struct CursesColor *cc4 = curses_color_new(4, 4);
    struct CursesColor *cc5 = curses_color_new(5, 5);

    TEST_CHECK(cc1 != NULL);
    TEST_CHECK(cc2 != NULL);
    TEST_CHECK(cc3 != NULL);
    TEST_CHECK(cc4 != NULL);
    TEST_CHECK(cc5 != NULL);

    curses_color_free(&cc2);
    curses_color_free(&cc4);

    cc2 = curses_color_new(22, 22);
    cc4 = curses_color_new(44, 44);

    TEST_CHECK(cc2 != NULL);
    TEST_CHECK(cc4 != NULL);

    curses_color_free(&cc1);
    curses_color_free(&cc2);
    curses_color_free(&cc3);
    curses_color_free(&cc4);
    curses_color_free(&cc5);
  }
}
