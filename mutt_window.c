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
#include "config/lib.h"
#include "core/lib.h"
#include "mutt_window.h"
#include "context.h"
#include "globals.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "options.h"
#include "pager.h"
#include "reflow.h"

struct MuttWindow *RootWindow = NULL;        ///< Parent of all Windows
struct MuttWindow *MuttDialogWindow = NULL;  ///< Parent of all Dialogs
struct MuttWindow *MuttHelpWindow = NULL;    ///< Help Window
struct MuttWindow *MuttMessageWindow = NULL; ///< Message Window

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
  win->state.visible = true;
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
 * mutt_dlg_rootwin_observer - Listen for config changes affecting the Root Window - Implements ::observer_t()
 */
int mutt_dlg_rootwin_observer(struct NotifyCallback *nc)
{
  if (!nc)
    return -1;

  struct EventConfig *ec = (struct EventConfig *) nc->event;
  if (!ec)
    return -1;

  struct MuttWindow *root_win = (struct MuttWindow *) nc->data;
  if (!root_win)
    return -1;

  if (mutt_str_strcmp(ec->name, "help") == 0)
  {
    MuttHelpWindow->state.visible = C_Help;
    goto reflow;
  }

  if (mutt_str_strcmp(ec->name, "status_on_top") == 0)
  {
    struct MuttWindow *first = TAILQ_FIRST(&root_win->children);
    if (!first)
      return -1;

    if ((C_StatusOnTop && (first->type == WT_HELP_BAR)) ||
        (!C_StatusOnTop && (first->type != WT_HELP_BAR)))
    {
      // Swap the HelpLine and the Dialogs Container
      struct MuttWindow *next = TAILQ_NEXT(first, entries);
      if (!next)
        return -1;
      TAILQ_REMOVE(&root_win->children, next, entries);
      TAILQ_INSERT_HEAD(&root_win->children, next, entries);
    }
  }

reflow:
  mutt_window_reflow(root_win);
  return 0;
}

/**
 * mutt_window_free_all - Free all the default Windows
 */
void mutt_window_free_all(void)
{
  if (Config)
    notify_observer_remove(Config->notify, mutt_dlg_rootwin_observer, (intptr_t) RootWindow);
  MuttDialogWindow = NULL;
  MuttHelpWindow = NULL;
  MuttMessageWindow = NULL;
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

  RootWindow = mutt_window_new(MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_FIXED, 0, 0);
  RootWindow->type = WT_ROOT;
  MuttHelpWindow = mutt_window_new(MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_FIXED,
                                   1, MUTT_WIN_SIZE_UNLIMITED);
  MuttHelpWindow->type = WT_HELP_BAR;
  MuttHelpWindow->state.visible = C_Help;
  MuttDialogWindow = mutt_window_new(MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                                     MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  MuttDialogWindow->type = WT_ALL_DIALOGS;
  MuttMessageWindow = mutt_window_new(MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_FIXED,
                                      1, MUTT_WIN_SIZE_UNLIMITED);
  MuttMessageWindow->type = WT_MESSAGE;

  if (C_StatusOnTop)
  {
    mutt_window_add_child(RootWindow, MuttDialogWindow);
    mutt_window_add_child(RootWindow, MuttHelpWindow);
  }
  else
  {
    mutt_window_add_child(RootWindow, MuttHelpWindow);
    mutt_window_add_child(RootWindow, MuttDialogWindow);
  }

  mutt_window_add_child(RootWindow, MuttMessageWindow);
  notify_observer_add(Config->notify, NT_CONFIG, 0, mutt_dlg_rootwin_observer,
                      (intptr_t) RootWindow);
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
 * mutt_window_reflow - Resize a Window and its children
 * @param win Window to resize
 */
void mutt_window_reflow(struct MuttWindow *win)
{
  if (OptNoCurses)
    return;

  mutt_debug(LL_DEBUG2, "entering\n");
  window_reflow(win ? win : RootWindow);

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
  MuttMessageWindow->req_rows = mw_rows;
  mutt_window_reflow(MuttMessageWindow->parent);

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
 * mutt_window_set_root - Set the dimensions of the Root Window
 * @param rows
 * @param cols
 */
void mutt_window_set_root(int rows, int cols)
{
  if (!RootWindow)
    return;

  bool changed = false;

  if (RootWindow->state.rows != rows)
  {
    RootWindow->state.rows = rows;
    changed = true;
  }

  if (RootWindow->state.cols != cols)
  {
    RootWindow->state.cols = cols;
    changed = true;
  }

  if (changed)
  {
    mutt_window_reflow(RootWindow);
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

/**
 * dialog_push - Display a Window to the user
 * @param dlg Window to display
 *
 * The Dialog Windows are kept in a stack.
 * The topmost is visible to the user, whilst the others are hidden.
 *
 * When a Window is pushed, the old Window is marked as not visible.
 */
void dialog_push(struct MuttWindow *dlg)
{
  if (!dlg || !MuttDialogWindow)
    return;

  struct MuttWindow *last = TAILQ_LAST(&MuttDialogWindow->children, MuttWindowList);
  if (last)
    last->state.visible = false;

  TAILQ_INSERT_TAIL(&MuttDialogWindow->children, dlg, entries);
  dlg->state.visible = true;
  mutt_window_reflow(MuttDialogWindow);
}

/**
 * dialog_pop - Hide a Window from the user
 *
 * The topmost (visible) Window is removed from the stack and the next Window
 * is marked as visible.
 */
void dialog_pop(void)
{
  if (!MuttDialogWindow)
    return;

  struct MuttWindow *last = TAILQ_LAST(&MuttDialogWindow->children, MuttWindowList);
  if (!last)
    return;

  last->state.visible = false;
  TAILQ_REMOVE(&MuttDialogWindow->children, last, entries);

  last = TAILQ_LAST(&MuttDialogWindow->children, MuttWindowList);
  if (last)
  {
    last->state.visible = true;
    mutt_window_reflow(MuttDialogWindow);
  }
}
