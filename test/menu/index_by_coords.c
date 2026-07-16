/**
 * @file
 * Test code for menu_get_index_by_coords()
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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
#include "mutt/lib.h"
#include "gui/lib.h"
#include "menu/lib.h"
#include "test_common.h"

void test_menu_get_index_by_coords(void)
{
  // int menu_get_index_by_coords(const struct Menu *menu, int row, int col);

  TEST_CHECK_NUM_EQ(menu_get_index_by_coords(NULL, 0, 0), -1);

  struct MuttWindow *win = mutt_window_new(WT_MENU, MUTT_WIN_ORIENT_VERTICAL,
                                           MUTT_WIN_SIZE_FIXED, 80, 5);
  TEST_CHECK(win != NULL);
  if (!win)
    return;

  win->state.visible = true;
  win->state.cols = 80;
  win->state.rows = 5;
  win->state.col_offset = 3;
  win->state.row_offset = 7;

  struct Menu menu = { 0 };
  menu.win = win;
  menu.top = 10;
  menu.max = 20;

  TEST_CHECK_NUM_EQ(menu_get_index_by_coords(&menu, 7, 3), 10);
  TEST_CHECK_NUM_EQ(menu_get_index_by_coords(&menu, 9, 40), 12);
  TEST_CHECK_NUM_EQ(menu_get_index_by_coords(&menu, 11, 82), 14);

  TEST_CHECK_NUM_EQ(menu_get_index_by_coords(&menu, 6, 3), -1);
  TEST_CHECK_NUM_EQ(menu_get_index_by_coords(&menu, 12, 3), -1);
  TEST_CHECK_NUM_EQ(menu_get_index_by_coords(&menu, 7, 2), -1);
  TEST_CHECK_NUM_EQ(menu_get_index_by_coords(&menu, 7, 83), -1);

  menu.max = 12;
  TEST_CHECK_NUM_EQ(menu_get_index_by_coords(&menu, 10, 3), -1);

  mutt_window_free(&win);
}
