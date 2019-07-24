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

/**
 * @page window Window management
 *
 * Window management
 */

#include "config.h"
#include <stdarg.h>
#include <string.h>
#include "mutt/mutt.h"
#include "mutt_window.h"
#include "globals.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "options.h"

struct MuttWindow *RootWindow = NULL;         ///< Parent of all Windows
struct MuttWindow *MuttHelpWindow = NULL;     ///< Help Window
struct MuttWindow *MuttIndexWindow = NULL;    ///< Index Window
struct MuttWindow *MuttMessageWindow = NULL;  ///< Message Window
struct MuttWindow *MuttPagerBarWindow = NULL; ///< Pager Status Window
struct MuttWindow *MuttPagerWindow = NULL;    ///< Pager Window
#ifdef USE_SIDEBAR
struct MuttWindow *MuttSidebarWindow = NULL; ///< Sidebar Window
#endif
struct MuttWindow *MuttStatusWindow = NULL; ///< Status Window

/**
 * mutt_window_new - Create a new Window
 * @param orient Window orientation, e.g. #MUTT_WIN_ORIENT_VERTICAL
 * @param size   Window size, e.g. #MUTT_WIN_SIZE_MAXIMISE
 * @param rows   Initial number of rows to allocate, can be #MUTT_WIN_SIZE_UNLIMITED
 * @param cols   Initial number of columns to allocate, can be #MUTT_WIN_SIZE_UNLIMITED
 * @retval ptr New Window
 */
struct MuttWindow *mutt_window_new(enum MuttWindowOrientation orient,
                                   enum MuttWindowSize size, int rows, int cols)
{
  struct MuttWindow *win = mutt_mem_calloc(1, sizeof(struct MuttWindow));

  win->orient = orient;
  win->size = size;
  win->req_rows = rows;
  win->req_cols = cols;
  TAILQ_INIT(&win->children);
  return win;
}

/**
 * mutt_window_free - Free a Window and its children
 * @param ptr Window to free
 */
void mutt_window_free(struct MuttWindow **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct MuttWindow *win = *ptr;

  if (win->wdata && win->free_wdata)
    win->free_wdata(win, &win->wdata); // Custom function to free private data

  mutt_winlist_free(&win->children);

  FREE(ptr);
}

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
  char buf[1024];

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
 * mutt_window_clrtobot - Clear to the bottom of the Window
 *
 * @note Assumes the cursor has already been positioned within the Window.
 */
void mutt_window_clrtobot(void)
{
  clrtobot();
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

  if (win->state.col_offset + win->state.cols == COLS)
    clrtoeol();
  else
  {
    int row = 0;
    int col = 0;
    getyx(stdscr, row, col);
    int curcol = col;
    while (curcol < (win->state.col_offset + win->state.cols))
    {
      addch(' ');
      curcol++;
    }
    move(row, col);
  }
}

/**
 * mutt_window_free_all - Free all the default Windows
 */
void mutt_window_free_all(void)
{
  MuttHelpWindow = NULL;
  MuttIndexWindow = NULL;
  MuttMessageWindow = NULL;
  MuttPagerBarWindow = NULL;
  MuttPagerWindow = NULL;
#ifdef USE_SIDEBAR
  MuttSidebarWindow = NULL;
#endif
  MuttStatusWindow = NULL;
  mutt_window_free(&RootWindow);
}

/**
 * mutt_window_get_coords - Get the cursor position in the Window
 * @param[in]  win Window
 * @param[out] row Row in Window
 * @param[out] col Column in Window
 *
 * Assumes the current position is inside the window.  Otherwise it will
 * happily return negative or values outside the window boundaries
 */
void mutt_window_get_coords(struct MuttWindow *win, int *row, int *col)
{
  int x = 0;
  int y = 0;

  getyx(stdscr, y, x);
  if (col)
    *col = x - win->state.col_offset;
  if (row)
    *row = y - win->state.row_offset;
}

/**
 * mutt_window_init - Create the default Windows
 *
 * Create the Help, Index, Status, Message and Sidebar Windows.
 */
void mutt_window_init(void)
{
  if (RootWindow)
    return;

  struct MuttWindow *w1 =
      mutt_window_new(MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  struct MuttWindow *w2 =
      mutt_window_new(MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  struct MuttWindow *w3 =
      mutt_window_new(MUTT_WIN_ORIENT_HORIZONTAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  struct MuttWindow *w4 =
      mutt_window_new(MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  struct MuttWindow *w5 =
      mutt_window_new(MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  struct MuttWindow *w6 =
      mutt_window_new(MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  w6->state.visible = false; // The Pager and Pager Bar are initially hidden

  MuttHelpWindow = mutt_window_new(MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_FIXED,
                                   1, MUTT_WIN_SIZE_UNLIMITED);
  MuttIndexWindow = mutt_window_new(MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                                    MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  MuttMessageWindow = mutt_window_new(MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_FIXED,
                                      1, MUTT_WIN_SIZE_UNLIMITED);
  MuttPagerBarWindow = mutt_window_new(MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_FIXED,
                                       1, MUTT_WIN_SIZE_UNLIMITED);
  MuttPagerWindow = mutt_window_new(MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                                    MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  MuttSidebarWindow = mutt_window_new(MUTT_WIN_ORIENT_HORIZONTAL, MUTT_WIN_SIZE_FIXED,
                                      MUTT_WIN_SIZE_UNLIMITED, 20);
  MuttStatusWindow = mutt_window_new(MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_FIXED,
                                     1, MUTT_WIN_SIZE_UNLIMITED);

  RootWindow = w1;

  mutt_window_add_child(w1, w2);
  mutt_window_add_child(w1, MuttMessageWindow);

  mutt_window_add_child(w2, MuttHelpWindow);
  mutt_window_add_child(w2, w3);

  mutt_window_add_child(w3, MuttSidebarWindow);
  mutt_window_add_child(w3, w4);

  mutt_window_add_child(w4, w5);
  mutt_window_add_child(w5, MuttIndexWindow);
  mutt_window_add_child(w5, MuttStatusWindow);

  mutt_window_add_child(w4, w6);
  mutt_window_add_child(w6, MuttPagerWindow);
  mutt_window_add_child(w6, MuttPagerBarWindow);
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
  return move(win->state.row_offset + row, win->state.col_offset + col);
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
#ifdef USE_SLANG_CURSES
  return mvaddstr(win->state.row_offset + row, win->state.col_offset + col, (char *) str);
#else
  return mvaddstr(win->state.row_offset + row, win->state.col_offset + col, str);
#endif
}

/**
 * mutt_window_mvprintw - Move the cursor and write a formatted string to a Window
 * @param win Window to write to
 * @param row Row to move to
 * @param col Column to move to
 * @param fmt printf format string
 * @param ... printf arguments
 * @retval num Success, characters written
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
 * mutt_window_copy_size - Copy the size of one Window to another
 * @param win_src Window to copy
 * @param win_dst Window to resize
 */
void mutt_window_copy_size(const struct MuttWindow *win_src, struct MuttWindow *win_dst)
{
  if (!win_src || !win_dst)
    return;

  win_dst->state.rows = win_src->state.rows;
  win_dst->state.cols = win_src->state.cols;
  win_dst->state.row_offset = win_src->state.row_offset;
  win_dst->state.col_offset = win_src->state.col_offset;
}

/**
 * mutt_window_reflow - Resize the Windows to fit the screen
 */
void mutt_window_reflow(void)
{
  if (OptNoCurses)
    return;

  mutt_debug(LL_DEBUG2, "entering\n");

  MuttStatusWindow->state.rows = 1;
  MuttStatusWindow->state.cols = COLS;
  MuttStatusWindow->state.row_offset = C_StatusOnTop ? 0 : LINES - 2;
  MuttStatusWindow->state.col_offset = 0;

  mutt_window_copy_size(MuttStatusWindow, MuttHelpWindow);
  if (C_Help)
    MuttHelpWindow->state.row_offset = C_StatusOnTop ? LINES - 2 : 0;
  else
    MuttHelpWindow->state.rows = 0;

  mutt_window_copy_size(MuttStatusWindow, MuttMessageWindow);
  MuttMessageWindow->state.row_offset = LINES - 1;

  mutt_window_copy_size(MuttStatusWindow, MuttIndexWindow);
  MuttIndexWindow->state.rows =
      MAX(LINES - MuttStatusWindow->state.rows - MuttHelpWindow->state.rows -
              MuttMessageWindow->state.rows,
          0);
  MuttIndexWindow->state.row_offset =
      C_StatusOnTop ? MuttStatusWindow->state.rows : MuttHelpWindow->state.rows;

#ifdef USE_SIDEBAR
  if (C_SidebarVisible)
  {
    mutt_window_copy_size(MuttIndexWindow, MuttSidebarWindow);
    MuttSidebarWindow->state.cols = C_SidebarWidth;
    MuttIndexWindow->state.cols -= C_SidebarWidth;

    if (C_SidebarOnRight)
    {
      MuttSidebarWindow->state.col_offset = COLS - C_SidebarWidth;
    }
    else
    {
      MuttIndexWindow->state.col_offset += C_SidebarWidth;
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
  MuttMessageWindow->state.rows = mw_rows;
  MuttMessageWindow->state.row_offset = LINES - mw_rows;

  MuttStatusWindow->state.row_offset = C_StatusOnTop ? 0 : LINES - mw_rows - 1;

  if (C_Help)
    MuttHelpWindow->state.row_offset = C_StatusOnTop ? LINES - mw_rows - 1 : 0;

  MuttIndexWindow->state.rows =
      MAX(LINES - MuttStatusWindow->state.rows - MuttHelpWindow->state.rows -
              MuttMessageWindow->state.rows,
          0);

#ifdef USE_SIDEBAR
  if (C_SidebarVisible)
    MuttSidebarWindow->state.rows = MuttIndexWindow->state.rows;
#endif

  /* We don't also set REDRAW_FLOW because this function only
   * changes rows and is a temporary adjustment. */
  mutt_menu_set_current_redraw_full();
}

/**
 * mutt_window_wrap_cols - Calculate the wrap column for a given screen width
 * @param width Screen width
 * @param wrap  Wrap config
 * @retval num Column that text should be wrapped at
 *
 * The wrap variable can be negative, meaning there should be a right margin.
 */
int mutt_window_wrap_cols(int width, short wrap)
{
  if (wrap < 0)
    return (width > -wrap) ? (width + wrap) : width;
  if (wrap)
    return (wrap < width) ? wrap : width;
  return width;
}

/**
 * mutt_window_addch - Write one character to a Window
 * @param ch  Character to write
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_window_addch(int ch)
{
  return addch(ch);
}

/**
 * mutt_window_addnstr - Write a partial string to a Window
 * @param str String
 * @param num Maximum number of characters to write
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_window_addnstr(const char *str, int num)
{
  if (!str)
    return -1;

#ifdef USE_SLANG_CURSES
  return addnstr((char *) str, num);
#else
  return addnstr(str, num);
#endif
}

/**
 * mutt_window_addstr - Write a string to a Window
 * @param str String
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_window_addstr(const char *str)
{
  if (!str)
    return -1;

#ifdef USE_SLANG_CURSES
  return addstr((char *) str);
#else
  return addstr(str);
#endif
}

/**
 * mutt_window_move_abs - Move the cursor to an absolute screen position
 * @param row Screen row (0-based)
 * @param col Screen column (0-based)
 */
void mutt_window_move_abs(int row, int col)
{
  move(row, col);
}

/**
 * mutt_window_printf - Write a formatted string to a Window
 * @param fmt Format string
 * @param ... Arguments
 * @retval num Number of characters written
 */
int mutt_window_printf(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int rc = vw_printw(stdscr, fmt, ap);
  va_end(ap);

  return rc;
}

/**
 * mutt_window_add_child - Add a child to Window
 * @param parent Window to add to
 * @param child  Window to add
 */
void mutt_window_add_child(struct MuttWindow *parent, struct MuttWindow *child)
{
  if (!parent || !child)
    return;

  TAILQ_INSERT_TAIL(&parent->children, child, entries);
  child->parent = parent;
}

/**
 * mutt_winlist_free - Free a tree of Windows
 * @param head WindowList to free
 */
void mutt_winlist_free(struct MuttWindowList *head)
{
  if (!head)
    return;

  struct MuttWindow *np = NULL;
  struct MuttWindow *tmp = NULL;
  TAILQ_FOREACH_SAFE(np, head, entries, tmp)
  {
    TAILQ_REMOVE(head, np, entries);
    mutt_winlist_free(&np->children);
    FREE(&np);
  }
}

/**
 * mutt_window_is_visible - Is the Window visible?
 * @param win Window
 * @retval true If the Window is visible
 *
 * For a Window to be visible, *it* must be visible and it's parent and
 * grandparent, etc.
 */
bool mutt_window_is_visible(struct MuttWindow *win)
{
  if (!win)
    return false;

  for (; win; win = win->parent)
  {
    if (!win->state.visible)
      return false;
  }

  return true;
}

/**
 * mutt_window_dialog - Find the parent Dialog of a Window
 * @param win Window
 * @retval ptr Dialog
 *
 * Windows may be nested under a MuttWindow of type #WT_DIALOG.
 */
struct MuttWindow *mutt_window_dialog(struct MuttWindow *win)
{
  if (!win)
    return NULL;
  if (win->type == WT_DIALOG)
    return win;

  return mutt_window_dialog(win->parent);
}

/**
 * mutt_window_find - Find a Window of a given type
 * @param root Window to start searching
 * @param type Window type to find, e.g. #WT_INDEX_BAR
 * @retval ptr  Matching Window
 * @retval NULL No match
 */
struct MuttWindow *mutt_window_find(struct MuttWindow *root, enum WindowType type)
{
  if (!root)
    return NULL;
  if (root->type == type)
    return root;

  struct MuttWindow *np = NULL;
  struct MuttWindow *match = NULL;
  TAILQ_FOREACH(np, &root->children, entries)
  {
    match = mutt_window_find(np, type);
    if (match)
      return match;
  }

  return NULL;
}
