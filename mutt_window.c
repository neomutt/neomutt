/**
 * @file
 * Window management
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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

#include "config.h"
#include "globals.h"
#include "mutt/logging.h"
#include "mutt/mutt.h"
#include "mutt_menu.h"
#include "mutt_window.h"
#include "options.h"
#include <stdarg.h>
#include <string.h>

struct MuttWindow *MuttHelpWindow = NULL;
struct MuttWindow *MuttIndexWindow = NULL;
struct MuttWindow *MuttStatusWindow = NULL;
struct MuttWindow *MuttMessageWindow = NULL;
#ifdef USE_SIDEBAR
struct MuttWindow *MuttSidebarWindow = NULL;
#endif

#ifdef USE_SLANG_CURSES
/**
 * vw_printw - Write a formatted string to a Window (function missing from Slang)
 * @param win Window
 * @param fmt printf format string
 * @param ap  printf arguments
 * @retval 0 Always
 */
static int vw_printw(SLcurses_Window_Type *win, const char *fmt, va_list ap)
{
  char buf[LONG_STRING];

  (void) SLvsnprintf(buf, sizeof(buf), (char *) fmt, ap);
  SLcurses_waddnstr(win, buf, -1);
  return 0;
}
#endif

/**
 * mutt_window_clearline - Clear a row of a Window
 * @param win Window
 * @param row Row to clear
 */
void mutt_window_clearline(struct MuttWindow *win, int row)
{
  mutt_window_move(win, row, 0);
  mutt_window_clrtoeol(win);
}

/**
 * mutt_window_clrtoeol - Clear to the end of the line
 * @param win Window
 *
 * @note Assumes the cursor has already been positioned within the window.
 */
void mutt_window_clrtoeol(struct MuttWindow *win)
{
  if (!win || !stdscr)
    return;

  if (win->col_offset + win->cols == COLS)
    clrtoeol();
  else
  {
    int row = 0;
    int col = 0;
    getyx(stdscr, row, col);
    int curcol = col;
    while (curcol < (win->col_offset + win->cols))
    {
      addch(' ');
      curcol++;
    }
    move(row, col);
  }
}

/**
 * mutt_window_free - Free the default Windows
 */
void mutt_window_free(void)
{
  FREE(&MuttHelpWindow);
  FREE(&MuttIndexWindow);
  FREE(&MuttStatusWindow);
  FREE(&MuttMessageWindow);
#ifdef USE_SIDEBAR
  FREE(&MuttSidebarWindow);
#endif
}

/**
 * mutt_window_getxy - Get the cursor position in the Window
 * @param[in]  win Window
 * @param[out] x   X-Coordinate
 * @param[out] y   Y-Coordinate
 *
 * Assumes the current position is inside the window.  Otherwise it will
 * happily return negative or values outside the window boundaries
 */
void mutt_window_getxy(struct MuttWindow *win, int *x, int *y)
{
  int row = 0;
  int col = 0;

  getyx(stdscr, row, col);
  if (x)
    *x = col - win->col_offset;
  if (y)
    *y = row - win->row_offset;
}

/**
 * mutt_window_init - Create the default Windows
 *
 * Create the Help, Index, Status, Message and Sidebar Windows.
 */
void mutt_window_init(void)
{
  MuttHelpWindow = mutt_mem_calloc(1, sizeof(struct MuttWindow));
  MuttIndexWindow = mutt_mem_calloc(1, sizeof(struct MuttWindow));
  MuttStatusWindow = mutt_mem_calloc(1, sizeof(struct MuttWindow));
  MuttMessageWindow = mutt_mem_calloc(1, sizeof(struct MuttWindow));
#ifdef USE_SIDEBAR
  MuttSidebarWindow = mutt_mem_calloc(1, sizeof(struct MuttWindow));
#endif
}

/**
 * mutt_window_move - Move the cursor in a Window
 * @param win Window
 * @param row Row to move to
 * @param col Column to move to
 * @retval OK  Success
 * @retval ERR Error
 */
int mutt_window_move(struct MuttWindow *win, int row, int col)
{
  return move(win->row_offset + row, win->col_offset + col);
}

/**
 * mutt_window_mvaddch - Move the cursor and write a character to a Window
 * @param win Window to write to
 * @param row Row to move to
 * @param col Column to move to
 * @param ch  Character to write
 * @retval OK  Success
 * @retval ERR Error
 */
int mutt_window_mvaddch(struct MuttWindow *win, int row, int col, const chtype ch)
{
  return mvaddch(win->row_offset + row, win->col_offset + col, ch);
}

/**
 * mutt_window_mvaddstr - Move the cursor and write a fixed string to a Window
 * @param win Window to write to
 * @param row Row to move to
 * @param col Column to move to
 * @param str String to write
 * @retval OK  Success
 * @retval ERR Error
 */
int mutt_window_mvaddstr(struct MuttWindow *win, int row, int col, const char *str)
{
  return mvaddstr(win->row_offset + row, win->col_offset + col, str);
}

/**
 * mutt_window_mvprintw - Move the cursor and write a formatted string to a Window
 * @param win Window to write to
 * @param row Row to move to
 * @param col Column to move to
 * @param fmt printf format string
 * @param ... printf arguments
 * @retval num Success, number of characters written
 * @retval ERR Error, move failed
 */
int mutt_window_mvprintw(struct MuttWindow *win, int row, int col, const char *fmt, ...)
{
  int rc = mutt_window_move(win, row, col);
  if (rc == ERR)
    return rc;

  va_list ap;
  va_start(ap, fmt);
  rc = vw_printw(stdscr, fmt, ap);
  va_end(ap);

  return rc;
}

/**
 * mutt_window_reflow - Resize the Windows to fit the screen
 */
void mutt_window_reflow(void)
{
  if (OptNoCurses)
    return;

  mutt_debug(2, "entering\n");

  MuttStatusWindow->rows = 1;
  MuttStatusWindow->cols = COLS;
  MuttStatusWindow->row_offset = StatusOnTop ? 0 : LINES - 2;
  MuttStatusWindow->col_offset = 0;

  memcpy(MuttHelpWindow, MuttStatusWindow, sizeof(struct MuttWindow));
  if (!Help)
    MuttHelpWindow->rows = 0;
  else
    MuttHelpWindow->row_offset = StatusOnTop ? LINES - 2 : 0;

  memcpy(MuttMessageWindow, MuttStatusWindow, sizeof(struct MuttWindow));
  MuttMessageWindow->row_offset = LINES - 1;

  memcpy(MuttIndexWindow, MuttStatusWindow, sizeof(struct MuttWindow));
  MuttIndexWindow->rows = MAX(
      LINES - MuttStatusWindow->rows - MuttHelpWindow->rows - MuttMessageWindow->rows, 0);
  MuttIndexWindow->row_offset =
      StatusOnTop ? MuttStatusWindow->rows : MuttHelpWindow->rows;

#ifdef USE_SIDEBAR
  if (SidebarVisible)
  {
    memcpy(MuttSidebarWindow, MuttIndexWindow, sizeof(struct MuttWindow));
    MuttSidebarWindow->cols = SidebarWidth;
    MuttIndexWindow->cols -= SidebarWidth;

    if (SidebarOnRight)
    {
      MuttSidebarWindow->col_offset = COLS - SidebarWidth;
    }
    else
    {
      MuttIndexWindow->col_offset += SidebarWidth;
    }
  }
#endif

  mutt_menu_set_current_redraw_full();
  /* the pager menu needs this flag set to recalc line_info */
  mutt_menu_set_current_redraw(REDRAW_FLOW);
}

/**
 * mutt_window_reflow_message_rows - Resize the Message Window
 * @param mw_rows Number of rows required
 *
 * Resize the other Windows to allow a multi-line message to be displayed.
 */
void mutt_window_reflow_message_rows(int mw_rows)
{
  MuttMessageWindow->rows = mw_rows;
  MuttMessageWindow->row_offset = LINES - mw_rows;

  MuttStatusWindow->row_offset = StatusOnTop ? 0 : LINES - mw_rows - 1;

  if (Help)
    MuttHelpWindow->row_offset = StatusOnTop ? LINES - mw_rows - 1 : 0;

  MuttIndexWindow->rows = MAX(
      LINES - MuttStatusWindow->rows - MuttHelpWindow->rows - MuttMessageWindow->rows, 0);

#ifdef USE_SIDEBAR
  if (SidebarVisible)
    MuttSidebarWindow->rows = MuttIndexWindow->rows;
#endif

  /* We don't also set REDRAW_FLOW because this function only
   * changes rows and is a temporary adjustment. */
  mutt_menu_set_current_redraw_full();
}

/**
 * mutt_window_wrap_cols - Calculate the wrap column for a Window
 *
 * The wrap variable can be negative, meaning there should be a right margin.
 */
int mutt_window_wrap_cols(struct MuttWindow *win, short wrap)
{
  if (wrap < 0)
    return (win->cols > -wrap) ? (win->cols + wrap) : win->cols;
  else if (wrap)
    return (wrap < win->cols) ? wrap : win->cols;
  else
    return win->cols;
}
