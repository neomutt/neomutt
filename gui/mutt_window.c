/**
 * @file
 * Window management
 *
 * @authors
 * Copyright (C) 2018-2020 Richard Russon <rich@flatcap.org>
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
 * @page gui_window Window management
 *
 * Window management
 */

#include "config.h"
#include <stdarg.h>
#include <string.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "mutt_window.h"
#include "helpbar/lib.h"
#include "curs_lib.h"
#include "msgwin.h"
#include "mutt_curses.h"
#include "options.h"
#include "reflow.h"
#ifdef USE_DEBUG_WINDOW
#include "debug/lib.h"
#endif

struct MuttWindow *RootWindow = NULL;       ///< Parent of all Windows
struct MuttWindow *AllDialogsWindow = NULL; ///< Parent of all Dialogs
struct MuttWindow *MessageWindow = NULL;    ///< Message Window, ":set", etc

/// Lookups for Window Names
static const struct Mapping WindowNames[] = {
  // clang-format off
  { "WT_ALL_DIALOGS",     WT_ALL_DIALOGS },
  { "WT_CONTAINER",       WT_CONTAINER },
  { "WT_CUSTOM",          WT_CUSTOM },
  { "WT_DLG_ALIAS",       WT_DLG_ALIAS },
  { "WT_DLG_ATTACH",      WT_DLG_ATTACH },
  { "WT_DLG_AUTOCRYPT",   WT_DLG_AUTOCRYPT },
  { "WT_DLG_BROWSER",     WT_DLG_BROWSER },
  { "WT_DLG_CERTIFICATE", WT_DLG_CERTIFICATE },
  { "WT_DLG_COMPOSE",     WT_DLG_COMPOSE },
  { "WT_DLG_CRYPT_GPGME", WT_DLG_CRYPT_GPGME },
  { "WT_DLG_DO_PAGER",    WT_DLG_DO_PAGER },
  { "WT_DLG_HISTORY",     WT_DLG_HISTORY },
  { "WT_DLG_INDEX",       WT_DLG_INDEX },
  { "WT_DLG_PGP",         WT_DLG_PGP },
  { "WT_DLG_POSTPONE",    WT_DLG_POSTPONE },
  { "WT_DLG_QUERY",       WT_DLG_QUERY },
  { "WT_DLG_REMAILER",    WT_DLG_REMAILER },
  { "WT_DLG_SMIME",       WT_DLG_SMIME },
  { "WT_HELP_BAR",        WT_HELP_BAR },
  { "WT_INDEX",           WT_INDEX },
  { "WT_MENU",            WT_MENU },
  { "WT_MESSAGE",         WT_MESSAGE },
  { "WT_PAGER",           WT_PAGER },
  { "WT_ROOT",            WT_ROOT },
  { "WT_SIDEBAR",         WT_SIDEBAR },
  { "WT_STATUS_BAR",      WT_STATUS_BAR },
  // clang-format off
  { NULL, 0 },
};

/**
 * window_was_visible - Was the Window visible?
 * @param win Window
 * @retval true The Window was visible
 *
 * Using the `WindowState old`, check if a Window used to be visible.
 * For a Window to be visible, *it* must have been visible and it's parent and
 * grandparent, etc.
 */
static bool window_was_visible(struct MuttWindow *win)
{
  if (!win)
    return false;

  for (; win; win = win->parent)
  {
    if (!win->old.visible)
      return false;
  }

  return true;
}

/**
 * window_notify - Notify observers of changes to a Window
 * @param win Window
 */
static void window_notify(struct MuttWindow *win)
{
  if (!win->notify)
    return;

  const struct WindowState *old = &win->old;
  const struct WindowState *state = &win->state;
  WindowNotifyFlags flags = WN_NO_FLAGS;

  const bool was_visible = window_was_visible(win);
  const bool is_visible = mutt_window_is_visible(win);
  if (was_visible != is_visible)
    flags |= is_visible ? WN_VISIBLE : WN_HIDDEN;

  if ((state->row_offset != old->row_offset) || (state->col_offset != old->col_offset))
    flags |= WN_MOVED;

  if (state->rows > old->rows)
    flags |= WN_TALLER;
  else if (state->rows < old->rows)
    flags |= WN_SHORTER;

  if (state->cols > old->cols)
    flags |= WN_WIDER;
  else if (state->cols < old->cols)
    flags |= WN_NARROWER;

  if (flags == WN_NO_FLAGS)
    return;

  mutt_debug(LL_NOTIFY, "NT_WINDOW_STATE: %s, %p\n", mutt_window_win_name(win), win);
  struct EventWindow ev_w = { win, flags };
  notify_send(win->notify, NT_WINDOW, NT_WINDOW_STATE, &ev_w);
}

/**
 * window_notify_all - Notify observers of changes to a Window and its children
 * @param win Window
 */
void window_notify_all(struct MuttWindow *win)
{
  if (!win)
    win = RootWindow;

  window_notify(win);

  struct MuttWindow *np = NULL;
  TAILQ_FOREACH(np, &win->children, entries)
  {
    window_notify_all(np);
  }
  win->old = win->state;
}

/**
 * window_set_visible - Set a Window visible or hidden
 * @param win     Window
 * @param visible If true, make Window visible, otherwise hidden
 */
void window_set_visible(struct MuttWindow *win, bool visible)
{
  if (!win)
    win = RootWindow;

  win->state.visible = visible;
}

/**
 * mutt_window_new - Create a new Window
 * @param type   Window type, e.g. #WT_ROOT
 * @param orient Window orientation, e.g. #MUTT_WIN_ORIENT_VERTICAL
 * @param size   Window size, e.g. #MUTT_WIN_SIZE_MAXIMISE
 * @param cols   Initial number of columns to allocate, can be #MUTT_WIN_SIZE_UNLIMITED
 * @param rows   Initial number of rows to allocate, can be #MUTT_WIN_SIZE_UNLIMITED
 * @retval ptr New Window
 */
struct MuttWindow *mutt_window_new(enum WindowType type, enum MuttWindowOrientation orient,
                                   enum MuttWindowSize size, int cols, int rows)
{
  struct MuttWindow *win = mutt_mem_calloc(1, sizeof(struct MuttWindow));

  win->type = type;
  win->orient = orient;
  win->size = size;
  win->req_rows = rows;
  win->req_cols = cols;
  win->state.visible = true;
  win->notify = notify_new();
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

  if (win->parent && (win->parent->focus == win))
    win->parent->focus = NULL;

  mutt_debug(LL_NOTIFY, "NT_WINDOW_DELETE: %s, %p\n", mutt_window_win_name(win), win);
  struct EventWindow ev_w = { win, WN_NO_FLAGS };
  notify_send(win->notify, NT_WINDOW, NT_WINDOW_DELETE, &ev_w);

  mutt_winlist_free(&win->children);

  if (win->wdata && win->wdata_free)
    win->wdata_free(win, &win->wdata); // Custom function to free private data

  notify_free(&win->notify);

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
  mutt_window_move(win, 0, row);
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

  if ((win->state.col_offset + win->state.cols) == COLS)
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
 * rootwin_config_observer - Listen for config changes affecting the Root Window - Implements ::observer_t
 */
static int rootwin_config_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_CONFIG)
    return 0;

  struct EventConfig *ev_c = nc->event_data;
  struct MuttWindow *root_win = nc->global_data;

  if (mutt_str_equal(ev_c->name, "status_on_top"))
  {
    struct MuttWindow *first = TAILQ_FIRST(&root_win->children);
    if (!first)
      return -1;

    mutt_debug(LL_DEBUG5, "config: '%s'\n", ev_c->name);
    const bool c_status_on_top = cs_subset_bool(NeoMutt->sub, "status_on_top");
    if ((c_status_on_top && (first->type == WT_HELP_BAR)) ||
        (!c_status_on_top && (first->type != WT_HELP_BAR)))
    {
      // Swap the HelpBar and the AllDialogsWindow
      struct MuttWindow *next = TAILQ_NEXT(first, entries);
      if (!next)
        return -1;
      TAILQ_REMOVE(&root_win->children, next, entries);
      TAILQ_INSERT_HEAD(&root_win->children, next, entries);

      mutt_debug(LL_DEBUG5, "config done, request WA_REFLOW\n");
    }
  }

  mutt_window_reflow(root_win);
  return 0;
}

/**
 * mutt_window_free_all - Free all the default Windows
 */
void mutt_window_free_all(void)
{
  if (NeoMutt)
    notify_observer_remove(NeoMutt->notify, rootwin_config_observer, RootWindow);
  AllDialogsWindow = NULL;
  MessageWindow = NULL;
  mutt_window_free(&RootWindow);
}

/**
 * mutt_window_get_coords - Get the cursor position in the Window
 * @param[in]  win Window
 * @param[out] col Column in Window
 * @param[out] row Row in Window
 *
 * Assumes the current position is inside the window.  Otherwise it will
 * happily return negative or values outside the window boundaries
 */
void mutt_window_get_coords(struct MuttWindow *win, int *col, int *row)
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

  RootWindow =
      mutt_window_new(WT_ROOT, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_FIXED, 0, 0);
  notify_set_parent(RootWindow->notify, NeoMutt->notify);

  struct MuttWindow *win_helpbar = helpbar_create();

  AllDialogsWindow = mutt_window_new(WT_ALL_DIALOGS, MUTT_WIN_ORIENT_VERTICAL,
                                     MUTT_WIN_SIZE_MAXIMISE, MUTT_WIN_SIZE_UNLIMITED,
                                     MUTT_WIN_SIZE_UNLIMITED);

  MessageWindow = msgwin_create();

  const bool c_status_on_top = cs_subset_bool(NeoMutt->sub, "status_on_top");
  if (c_status_on_top)
  {
    mutt_window_add_child(RootWindow, AllDialogsWindow);
    mutt_window_add_child(RootWindow, win_helpbar);
  }
  else
  {
    mutt_window_add_child(RootWindow, win_helpbar);
    mutt_window_add_child(RootWindow, AllDialogsWindow);
  }

  mutt_window_add_child(RootWindow, MessageWindow);
  notify_observer_add(NeoMutt->notify, NT_CONFIG, rootwin_config_observer, RootWindow);
}

/**
 * mutt_window_move - Move the cursor in a Window
 * @param win Window
 * @param col Column to move to
 * @param row Row to move to
 * @retval OK  Success
 * @retval ERR Error
 */
int mutt_window_move(struct MuttWindow *win, int col, int row)
{
  return move(win->state.row_offset + row, win->state.col_offset + col);
}

/**
 * mutt_window_mvaddstr - Move the cursor and write a fixed string to a Window
 * @param win Window to write to
 * @param col Column to move to
 * @param row Row to move to
 * @param str String to write
 * @retval OK  Success
 * @retval ERR Error
 */
int mutt_window_mvaddstr(struct MuttWindow *win, int col, int row, const char *str)
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
 * @param col Column to move to
 * @param row Row to move to
 * @param fmt printf format string
 * @param ... printf arguments
 * @retval num Success, characters written
 * @retval ERR Error, move failed
 */
int mutt_window_mvprintw(struct MuttWindow *win, int col, int row, const char *fmt, ...)
{
  int rc = mutt_window_move(win, col, row);
  if (rc == ERR)
    return rc;

  va_list ap;
  va_start(ap, fmt);
  rc = vw_printw(stdscr, fmt, ap);
  va_end(ap);

  return rc;
}

/**
 * mutt_window_reflow - Resize a Window and its children
 * @param win Window to resize
 */
void mutt_window_reflow(struct MuttWindow *win)
{
  if (OptNoCurses)
    return;

  if (!win)
    win = RootWindow;

  mutt_debug(LL_DEBUG2, "entering\n");
  window_reflow(win);
  window_notify_all(win);

#ifdef USE_DEBUG_WINDOW
  debug_win_dump();
#endif
}

/**
 * mutt_window_reflow_message_rows - Resize the Message Window
 * @param mw_rows Number of rows required
 *
 * Resize the other Windows to allow a multi-line message to be displayed.
 */
void mutt_window_reflow_message_rows(int mw_rows)
{
  MessageWindow->req_rows = mw_rows;
  mutt_window_reflow(MessageWindow->parent);

  /* We don't also set MENU_REDRAW_FLOW because this function only
   * changes rows and is a temporary adjustment. */
  window_redraw(RootWindow);
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
 * @param win Window
 * @param ch  Character to write
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_window_addch(struct MuttWindow *win, int ch)
{
  return addch(ch);
}

/**
 * mutt_window_addnstr - Write a partial string to a Window
 * @param win Window
 * @param str String
 * @param num Maximum number of characters to write
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_window_addnstr(struct MuttWindow *win, const char *str, int num)
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
 * @param win Window
 * @param str String
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_window_addstr(struct MuttWindow *win, const char *str)
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
 * mutt_window_printf - Write a formatted string to a Window
 * @param win Window
 * @param fmt Format string
 * @param ... Arguments
 * @retval num Number of characters written
 */
int mutt_window_printf(struct MuttWindow *win, const char *fmt, ...)
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

  notify_set_parent(child->notify, parent->notify);

  mutt_debug(LL_NOTIFY, "NT_WINDOW_NEW: %s, %p\n", mutt_window_win_name(child), child);
  struct EventWindow ev_w = { child, WN_NO_FLAGS };
  notify_send(child->notify, NT_WINDOW, NT_WINDOW_ADD, &ev_w);
}

/**
 * mutt_window_remove_child - Remove a child from a Window
 * @param parent Window to remove from
 * @param child  Window to remove
 */
struct MuttWindow *mutt_window_remove_child(struct MuttWindow *parent, struct MuttWindow *child)
{
  if (!parent || !child)
    return NULL;

  mutt_debug(LL_NOTIFY, "NT_WINDOW_DELETE: %s, %p\n", mutt_window_win_name(child), child);
  struct EventWindow ev_w = { child, WN_NO_FLAGS };
  notify_send(child->notify, NT_WINDOW, NT_WINDOW_DELETE, &ev_w);

  TAILQ_REMOVE(&parent->children, child, entries);
  child->parent = NULL;

  notify_set_parent(child->notify, NULL);

  return child;
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
    mutt_window_free(&np);
  }
}

/**
 * mutt_window_set_root - Set the dimensions of the Root Window
 * @param rows
 * @param cols
 */
void mutt_window_set_root(int cols, int rows)
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
 * @retval true The Window is visible
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
 * mutt_window_find - Find a Window of a given type
 * @param root Window to start searching
 * @param type Window type to find, e.g. #WT_STATUS_BAR
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
 * window_recalc - Recalculate a tree of Windows
 * @param win Window to start at
 */
static void window_recalc(struct MuttWindow *win)
{
  if (!win)
    return;

  if (win->recalc)
    win->recalc(win);
  win->actions &= ~WA_RECALC;

  struct MuttWindow *np = NULL;
  TAILQ_FOREACH(np, &win->children, entries)
  {
    window_recalc(np);
  }
}

/**
 * window_repaint - Repaint a tree of Windows
 * @param win   Window to start at
 */
static void window_repaint(struct MuttWindow *win)
{
  if (!win)
    return;

  if (win->repaint && (win->actions & WA_REPAINT))
    win->repaint(win);
  win->actions &= ~WA_REPAINT;

  struct MuttWindow *np = NULL;
  TAILQ_FOREACH(np, &win->children, entries)
  {
    window_repaint(np);
  }
}

/**
 * window_redraw - Reflow, recalc and repaint a tree of Windows
 * @param win   Window to start at
 */
void window_redraw(struct MuttWindow *win)
{
  if (!win)
    return;

  window_reflow(win);
  window_notify_all(win);

  window_recalc(win);
  window_repaint(win);
  mutt_refresh();
}

/**
 * window_get_focus - Get the currently focussed Window
 * @retval ptr Window with focus
 */
struct MuttWindow *window_get_focus(void)
{
  struct MuttWindow *win = RootWindow;

  while (win && win->focus)
    win = win->focus;

  return win;
}

/**
 * window_set_focus - Set the Window focus
 * @param win Window to focus
 * @retval ptr  Old focussed Window
 * @retval NULL Error, or focus not changed
 */
struct MuttWindow *window_set_focus(struct MuttWindow *win)
{
  if (!win)
    return NULL;

  struct MuttWindow *old_focus = window_get_focus();

  struct MuttWindow *parent = win->parent;
  struct MuttWindow *child = win;

  // Set the chain of focus, all the way to the root
  for (; parent; child = parent, parent = parent->parent)
    parent->focus = child;

  // Find the most focussed Window
  while (win && win->focus)
    win = win->focus;

  if (win == old_focus)
    return NULL;

  mutt_debug(LL_NOTIFY, "NT_WINDOW_FOCUS: %s, %p\n", mutt_window_win_name(win), win);
  struct EventWindow ev_w = { win, WN_NO_FLAGS };
  notify_send(win->notify, NT_WINDOW, NT_WINDOW_FOCUS, &ev_w);
#ifdef USE_DEBUG_WINDOW
  debug_win_dump();
#endif
  return old_focus;
}

/**
 * window_get_dialog - Get the currently active Dialog
 * @retval ptr Active Dialog
 */
struct MuttWindow *window_get_dialog(void)
{
  if (!AllDialogsWindow)
    return NULL;

  struct MuttWindow *np = NULL;
  TAILQ_FOREACH(np, &AllDialogsWindow->children, entries)
  {
    if (mutt_window_is_visible(np))
      return np;
  }

  return NULL;
}

/**
 * mutt_window_clear - Clear a Window
 * @param win Window
 *
 * If the Window isn't visible, it won't be cleared.
 */
void mutt_window_clear(struct MuttWindow *win)
{
  if (!mutt_window_is_visible(win))
    return;

  for (int i = 0; i < win->state.rows; i++)
    mutt_window_clearline(win, i);
}

/**
 * mutt_window_win_name - Get the name of a Window
 * @param win Window
 * @retval ptr  String describing Window
 * @retval NULL Error, or unknown
 */
const char *mutt_window_win_name(const struct MuttWindow *win)
{
  if (!win)
    return "UNKNOWN";

  const char *name = mutt_map_get_name(win->type, WindowNames);
  if (name)
    return name;
  return "UNKNOWN";
}

/**
 * window_invalidate - Mark a window as in need of repaint
 * @param win   Window to start at
 */
static void window_invalidate(struct MuttWindow *win)
{
  if (!win)
    return;

  win->actions |= WA_REPAINT;

  struct MuttWindow *np = NULL;
  TAILQ_FOREACH(np, &win->children, entries)
  {
    window_invalidate(np);
  }
}

/**
 * window_invalidate_all - Mark all windows as in need of repaint
 */
void window_invalidate_all(void)
{
  window_invalidate(RootWindow);
  clearok(stdscr, true);
  keypad(stdscr, true);
}

/**
 * window_status_on_top - Organise windows according to config variable
 * @param panel Window containing WT_MENU and WT_STATUS_BAR
 * @param sub   Config Subset
 * @retval true Window order was changed
 *
 * Set the positions of two Windows based on a config variable `$status_on_top`.
 *
 * @note The children are expected to have types: #WT_MENU, #WT_STATUS_BAR
 */
bool window_status_on_top(struct MuttWindow *panel, struct ConfigSubset *sub)
{
  const bool c_status_on_top = cs_subset_bool(sub, "status_on_top");

  struct MuttWindow *win_first = TAILQ_FIRST(&panel->children);

  if ((c_status_on_top && (win_first->type == WT_STATUS_BAR)) ||
      (!c_status_on_top && (win_first->type != WT_STATUS_BAR)))
  {
    return false;
  }

  TAILQ_REMOVE(&panel->children, win_first, entries);
  TAILQ_INSERT_TAIL(&panel->children, win_first, entries);

  mutt_window_reflow(panel);
  mutt_debug(LL_DEBUG5, "config done, request WA_REFLOW\n");
  return true;
}
