/**
 * @file
 * Window reflowing
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
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
 * @page reflow Window reflowing
 *
 * Window reflowing
 */

#include "config.h"
#include <stddef.h>
#include "mutt/mutt.h"
#include "reflow.h"
#include "mutt_window.h"

/**
 * window_reflow_horiz - Reflow Windows using all the available horizontal space
 * @param win Window
 */
void window_reflow_horiz(struct MuttWindow *win)
{
  if (!win)
    return;

  int max_count = 0;
  int space = win->state.cols;

  // Pass one - minimal allocation
  struct MuttWindow *np = NULL;
  TAILQ_FOREACH(np, &win->children, entries)
  {
    if (!np->state.visible)
      continue;

    np->old = np->state; // Save the old state for later notifications

    switch (np->size)
    {
      case MUTT_WIN_SIZE_FIXED:
      {
        const int avail = MIN(space, np->req_cols);
        np->state.cols = avail;
        np->state.rows = win->state.rows;
        space -= avail;
        break;
      }
      case MUTT_WIN_SIZE_MAXIMISE:
      {
        np->state.cols = 1;
        np->state.rows = win->state.rows;
        max_count++;
        space -= 1;
        break;
      }
      case MUTT_WIN_SIZE_MINIMISE:
      {
        np->state.rows = win->state.rows;
        np->state.cols = win->state.cols;
        np->state.row_offset = win->state.row_offset;
        np->state.col_offset = win->state.col_offset;
        window_reflow(np);
        space -= np->state.cols;
        break;
      }
    }
  }

  // Pass two - sharing
  if ((max_count > 0) && (space > 0))
  {
    int alloc = (space + max_count - 1) / max_count;
    TAILQ_FOREACH(np, &win->children, entries)
    {
      if (space == 0)
        break;
      if (!np->state.visible)
        continue;
      if (np->size != MUTT_WIN_SIZE_MAXIMISE)
        continue;

      alloc = MIN(space, alloc);
      np->state.cols += alloc;
      space -= alloc;
    }
  }

  // Pass three - position and recursion
  int col = win->state.col_offset;
  TAILQ_FOREACH(np, &win->children, entries)
  {
    if (!np->state.visible)
      continue;

    np->state.col_offset = col;
    np->state.row_offset = win->state.row_offset;
    col += np->state.cols;

    if (np->size != MUTT_WIN_SIZE_MINIMISE)
      window_reflow(np);
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
void window_reflow_vert(struct MuttWindow *win)
{
  if (!win)
    return;

  int max_count = 0;
  int space = win->state.rows;

  // Pass one - minimal allocation
  struct MuttWindow *np = NULL;
  TAILQ_FOREACH(np, &win->children, entries)
  {
    if (!np->state.visible)
      continue;

    np->old = np->state; // Save the old state for later notifications

    switch (np->size)
    {
      case MUTT_WIN_SIZE_FIXED:
      {
        const int avail = MIN(space, np->req_rows);
        np->state.rows = avail;
        np->state.cols = win->state.cols;
        space -= avail;
        break;
      }
      case MUTT_WIN_SIZE_MAXIMISE:
      {
        np->state.rows = 1;
        np->state.cols = win->state.cols;
        max_count++;
        space -= 1;
        break;
      }
      case MUTT_WIN_SIZE_MINIMISE:
      {
        np->state.rows = win->state.rows;
        np->state.cols = win->state.cols;
        np->state.row_offset = win->state.row_offset;
        np->state.col_offset = win->state.col_offset;
        window_reflow(np);
        space -= np->state.rows;
        break;
      }
    }
  }

  // Pass two - sharing
  if ((max_count > 0) && (space > 0))
  {
    int alloc = (space + max_count - 1) / max_count;
    TAILQ_FOREACH(np, &win->children, entries)
    {
      if (space == 0)
        break;
      if (!np->state.visible)
        continue;
      if (np->size != MUTT_WIN_SIZE_MAXIMISE)
        continue;

      alloc = MIN(space, alloc);
      np->state.rows += alloc;
      space -= alloc;
    }
  }

  // Pass three - position and recursion
  int row = win->state.row_offset;
  TAILQ_FOREACH(np, &win->children, entries)
  {
    if (!np->state.visible)
      continue;

    np->state.row_offset = row;
    np->state.col_offset = win->state.col_offset;
    row += np->state.rows;

    if (np->size != MUTT_WIN_SIZE_MINIMISE)
      window_reflow(np);
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
