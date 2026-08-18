/**
 * @file
 * Test code for sb_select_by_coords()
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
#include "sidebar/lib.h"
#include "sidebar/private.h"
#include "test_common.h"

void test_sb_select_by_coords(void)
{
  // int sb_select_by_coords(struct MuttWindow *win, int row, int col);

  TEST_CHECK_NUM_EQ(sb_select_by_coords(NULL, 0, 0), -1);

  struct MuttWindow *win = mutt_window_new(WT_SIDEBAR, MUTT_WIN_ORIENT_VERTICAL,
                                           MUTT_WIN_SIZE_FIXED, 20, 4);
  TEST_CHECK(win != NULL);
  if (!win)
    return;

  win->state.visible = true;
  win->state.cols = 20;
  win->state.rows = 4;
  win->state.col_offset = 3;
  win->state.row_offset = 7;

  struct SidebarWindowData *wdata = sb_wdata_new(win, NULL);
  TEST_CHECK(wdata != NULL);
  if (!wdata)
  {
    mutt_window_free(&win);
    return;
  }

  win->wdata = wdata;
  wdata->top_index = 0;
  wdata->hil_index = 0;

  for (int i = 0; i < 4; i++)
  {
    struct SbEntry *entry = MUTT_MEM_CALLOC(1, struct SbEntry);
    TEST_CHECK(entry != NULL);
    if (!entry)
      break;

    entry->is_hidden = (i == 1);
    ARRAY_ADD(&wdata->entries, entry);
  }

  TEST_CHECK_NUM_EQ(sb_select_by_coords(win, 7, 3), 0);
  TEST_CHECK_NUM_EQ(wdata->hil_index, 0);

  TEST_CHECK_NUM_EQ(sb_select_by_coords(win, 8, 10), 1);
  TEST_CHECK_NUM_EQ(wdata->hil_index, 2);

  win->actions = WA_NONE;
  TEST_CHECK_NUM_EQ(sb_select_by_coords(win, 9, 20), 1);
  TEST_CHECK_NUM_EQ(wdata->hil_index, 3);
  TEST_CHECK((win->actions & WA_RECALC) != 0);

  win->actions = WA_NONE;
  TEST_CHECK_NUM_EQ(sb_select_by_coords(win, 10, 10), -1);
  TEST_CHECK_NUM_EQ(wdata->hil_index, 3);
  TEST_CHECK_NUM_EQ(win->actions, WA_NONE);

  TEST_CHECK_NUM_EQ(sb_select_by_coords(win, 6, 10), -1);
  TEST_CHECK_NUM_EQ(sb_select_by_coords(win, 7, 2), -1);
  TEST_CHECK_NUM_EQ(sb_select_by_coords(win, 7, 23), -1);

  sb_wdata_free(win, &win->wdata);
  mutt_window_free(&win);
}
