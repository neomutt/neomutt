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

#ifndef MUTT_MUTT_WINDOW_H
#define MUTT_MUTT_WINDOW_H

#include "config.h"

/**
 * enum MuttWindowOrientation - Which way does the Window expand?
 */
enum MuttWindowOrientation
{
  MUTT_WIN_ORIENT_VERTICAL = 1, ///< Window uses all available vertical space
  MUTT_WIN_ORIENT_HORIZONTAL,   ///< Window uses all available horizontal space
};

/**
 * enum MuttWindowSize - Control the allocation of Window space
 */
enum MuttWindowSize
{
  MUTT_WIN_SIZE_FIXED = 1, ///< Window has a fixed size
  MUTT_WIN_SIZE_MAXIMISE,  ///< Window wants as much space as possible
  MUTT_WIN_SIZE_MINIMISE,  ///< Window size depends on its children
};

#define MUTT_WIN_SIZE_UNLIMITED -1 ///< Use as much space as possible

/**
 * struct WindowState - The current, or old, state of a Window
 */
struct WindowState
{
  bool visible;     ///< Window is visible
  short rows;       ///< Number of rows, can be #MUTT_WIN_SIZE_UNLIMITED
  short cols;       ///< Number of columns, can be #MUTT_WIN_SIZE_UNLIMITED
  short row_offset; ///< Absolute on screen row
  short col_offset; ///< Absolute on screen column
};

/**
 * enum WindowType - Type of Window
 */
enum WindowType
{
  WT_ROOT,        ///< Parent of All Windows
  WT_ALL_DIALOGS, ///< Container for All Dialogs (nested Windows)
  WT_DIALOG,      ///< Dialog (nested Windows) displayed to the user
  WT_CONTAINER,   ///< Invisible shaping container Window
  WT_HELP_BAR,    ///< Help Bar containing list of useful key bindings
  WT_MESSAGE,     ///< Window for messages/errors and command entry
  WT_INDEX,       ///< An Index Window containing a selection list
  WT_INDEX_BAR,   ///< Index Bar containing status info about the Index
  WT_PAGER,       ///< Window containing paged free-form text
  WT_PAGER_BAR,   ///< Pager Bar containing status info about the Pager
  WT_SIDEBAR,     ///< Side panel containing Accounts or groups of data
};

TAILQ_HEAD(MuttWindowList, MuttWindow);

/**
 * struct MuttWindow - A division of the screen
 *
 * Windows for different parts of the screen
 */
struct MuttWindow
{
  short req_rows;                    ///< Number of rows required
  short req_cols;                    ///< Number of columns required

  struct WindowState state;          ///< Current state of the Window
  struct WindowState old;            ///< Previous state of the Window

  enum MuttWindowOrientation orient; ///< Which direction the Window will expand
  enum MuttWindowSize size;          ///< Type of Window, e.g. #MUTT_WIN_SIZE_FIXED

  TAILQ_ENTRY(MuttWindow) entries;   ///< Linked list
  struct MuttWindow *parent;         ///< Parent Window
  struct MuttWindowList children;    ///< Children Windows

  enum WindowType type;              ///< Window type, e.g. #WT_SIDEBAR
  void *wdata;                       ///< Private data
  void (*free_wdata)(struct MuttWindow *win, void **); ///< Callback function to free private data
};

extern struct MuttWindow *MuttHelpWindow;
extern struct MuttWindow *MuttIndexWindow;
extern struct MuttWindow *MuttMessageWindow;
extern struct MuttWindow *MuttPagerBarWindow;
extern struct MuttWindow *MuttPagerWindow;
#ifdef USE_SIDEBAR
extern struct MuttWindow *MuttSidebarWindow;
#endif
extern struct MuttWindow *MuttStatusWindow;

// Functions that deal with the Window
void               mutt_window_add_child          (struct MuttWindow *parent, struct MuttWindow *child);
void               mutt_window_copy_size          (const struct MuttWindow *win_src, struct MuttWindow *win_dst);
void               mutt_window_free               (struct MuttWindow **ptr);
void               mutt_window_free_all           (void);
void               mutt_window_get_coords         (struct MuttWindow *win, int *col, int *row);
void               mutt_window_init               (void);
struct MuttWindow *mutt_window_new                (enum MuttWindowOrientation orient, enum MuttWindowSize size, int rows, int cols);
void               mutt_window_reflow             (void);
void               mutt_window_reflow_message_rows(int mw_rows);
int                mutt_window_wrap_cols          (int width, short wrap);

// Functions for drawing on the Window
int  mutt_window_addch    (int ch);
int  mutt_window_addnstr  (const char *str, int num);
int  mutt_window_addstr   (const char *str);
void mutt_window_clearline(struct MuttWindow *win, int row);
void mutt_window_clrtobot (void);
void mutt_window_clrtoeol (struct MuttWindow *win);
int  mutt_window_move     (struct MuttWindow *win, int row, int col);
void mutt_window_move_abs (int row, int col);
int  mutt_window_mvaddstr (struct MuttWindow *win, int row, int col, const char *str);
int  mutt_window_mvprintw (struct MuttWindow *win, int row, int col, const char *fmt, ...);
int  mutt_window_printf   (const char *format, ...);
bool mutt_window_is_visible(struct MuttWindow *win);

void mutt_winlist_free       (struct MuttWindowList *head);
struct MuttWindow *mutt_window_find(struct MuttWindow *root, enum WindowType type);
struct MuttWindow *mutt_window_dialog(struct MuttWindow *win);

void mutt_winlist_free       (struct MuttWindowList *head);

#endif /* MUTT_MUTT_WINDOW_H */
