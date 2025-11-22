/**
 * @file
 * Window reflowing
 *
 * @authors
 * Copyright (C) 2019-2022 Richard Russon <rich@flatcap.org>
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
 * @page gui_reflow Window reflowing
 *
 * Window reflowing
 */

#include "config.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "reflow.h"
#include "mutt_window.h"

/**
 * window_reflow_horiz - Reflow Windows using all the available horizontal space
 * @param win Window
 */
static void window_reflow_horiz(struct MuttWindow *win)
{
  if (!win)
    return;

  int max_count = 0;
  int space = win->state.cols;

  // Pass one - minimal allocation
  struct MuttWindow **wp = NULL;
  ARRAY_FOREACH(wp, &win->children)
  {
    if (!(*wp)->state.visible)
      continue;

    switch ((*wp)->size)
    {
      case MUTT_WIN_SIZE_FIXED:
      {
        const int avail = MIN(space, (*wp)->req_cols);
        (*wp)->state.cols = avail;
        (*wp)->state.rows = win->state.rows;
        space -= avail;
        break;
      }
      case MUTT_WIN_SIZE_MAXIMISE:
      {
        (*wp)->state.cols = 1;
        (*wp)->state.rows = win->state.rows;
        max_count++;
        space -= 1;
        break;
      }
      case MUTT_WIN_SIZE_MINIMISE:
      {
        (*wp)->state.rows = win->state.rows;
        (*wp)->state.cols = win->state.cols;
        (*wp)->state.row_offset = win->state.row_offset;
        (*wp)->state.col_offset = win->state.col_offset;
        window_reflow(*wp);
        space -= (*wp)->state.cols;
        break;
      }
    }
  }

  // Pass two - sharing
  if ((max_count > 0) && (space > 0))
  {
    int alloc = (space + max_count - 1) / max_count;
    ARRAY_FOREACH(wp, &win->children)
    {
      if (space == 0)
        break;
      if (!(*wp)->state.visible)
        continue;
      if ((*wp)->size != MUTT_WIN_SIZE_MAXIMISE)
        continue;

      alloc = MIN(space, alloc);
      (*wp)->state.cols += alloc;
      space -= alloc;
    }
  }

  // Pass three - position and recursion
  int col = win->state.col_offset;
  ARRAY_FOREACH(wp, &win->children)
  {
    if (!(*wp)->state.visible)
      continue;

    (*wp)->state.col_offset = col;
    (*wp)->state.row_offset = win->state.row_offset;
    col += (*wp)->state.cols;

    window_reflow(*wp);
  }

  if ((space > 0) && (win->size == MUTT_WIN_SIZE_MINIMISE))
  {
    win->state.cols -= space;
  }
}

/**
 * window_reflow_vert - Reflow Windows using all the available vertical space
 * @param win Window
 */
static void window_reflow_vert(struct MuttWindow *win)
{
  if (!win)
    return;

  int max_count = 0;
  int space = win->state.rows;

  // Pass one - minimal allocation
  struct MuttWindow **wp = NULL;
  ARRAY_FOREACH(wp, &win->children)
  {
    if (!(*wp)->state.visible)
      continue;

    switch ((*wp)->size)
    {
      case MUTT_WIN_SIZE_FIXED:
      {
        const int avail = MIN(space, (*wp)->req_rows);
        (*wp)->state.rows = avail;
        (*wp)->state.cols = win->state.cols;
        space -= avail;
        break;
      }
      case MUTT_WIN_SIZE_MAXIMISE:
      {
        (*wp)->state.rows = 1;
        (*wp)->state.cols = win->state.cols;
        max_count++;
        space -= 1;
        break;
      }
      case MUTT_WIN_SIZE_MINIMISE:
      {
        (*wp)->state.rows = win->state.rows;
        (*wp)->state.cols = win->state.cols;
        (*wp)->state.row_offset = win->state.row_offset;
        (*wp)->state.col_offset = win->state.col_offset;
        window_reflow(*wp);
        space -= (*wp)->state.rows;
        break;
      }
    }
  }

  // Pass two - sharing
  if ((max_count > 0) && (space > 0))
  {
    int alloc = (space + max_count - 1) / max_count;
    ARRAY_FOREACH(wp, &win->children)
    {
      if (space == 0)
        break;
      if (!(*wp)->state.visible)
        continue;
      if ((*wp)->size != MUTT_WIN_SIZE_MAXIMISE)
        continue;

      alloc = MIN(space, alloc);
      (*wp)->state.rows += alloc;
      space -= alloc;
    }
  }

  // Pass three - position and recursion
  int row = win->state.row_offset;
  ARRAY_FOREACH(wp, &win->children)
  {
    if (!(*wp)->state.visible)
      continue;

    (*wp)->state.row_offset = row;
    (*wp)->state.col_offset = win->state.col_offset;
    row += (*wp)->state.rows;

    window_reflow(*wp);
  }

  if ((space > 0) && (win->size == MUTT_WIN_SIZE_MINIMISE))
  {
    win->state.rows -= space;
  }
}

/**
 * window_reflow - Reflow Windows
 * @param win Root Window
 *
 * Using the rules coded into the Windows, such as Fixed or Maximise, allocate
 * space to a set of nested Windows.
 */
void window_reflow(struct MuttWindow *win)
{
  if (!win)
    return;
  if (win->orient == MUTT_WIN_ORIENT_VERTICAL)
    window_reflow_vert(win);
  else
    window_reflow_horiz(win);
}
