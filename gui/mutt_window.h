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

#ifndef MUTT_MUTT_WINDOW_H
#define MUTT_MUTT_WINDOW_H

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include "mutt/lib.h"

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
  short cols;       ///< Number of columns, can be #MUTT_WIN_SIZE_UNLIMITED
  short rows;       ///< Number of rows, can be #MUTT_WIN_SIZE_UNLIMITED
  short col_offset; ///< Absolute on-screen column
  short row_offset; ///< Absolute on-screen row
};

/**
 * enum WindowType - Type of Window
 */
enum WindowType
{
  // Structural Windows
  WT_ROOT,            ///< Parent of All Windows
  WT_CONTAINER,       ///< Invisible shaping container Window
  WT_ALL_DIALOGS,     ///< Container for All Dialogs (nested Windows)

  // Dialogs (nested Windows) displayed to the user
  WT_DLG_ALIAS,       ///< Alias Dialog,       dlg_select_alias()
  WT_DLG_ATTACH,      ///< Attach Dialog,      dlg_select_attachment()
  WT_DLG_AUTOCRYPT,   ///< Autocrypt Dialog,   dlg_select_autocrypt_account()
  WT_DLG_BROWSER,     ///< Browser Dialog,     mutt_buffer_select_file()
  WT_DLG_CERTIFICATE, ///< Certificate Dialog, dlg_verify_certificate()
  WT_DLG_COMPOSE,     ///< Compose Dialog,     mutt_compose_menu()
  WT_DLG_CRYPT_GPGME, ///< Crypt-GPGME Dialog, dlg_select_gpgme_key()
  WT_DLG_DO_PAGER,    ///< Pager Dialog,       mutt_do_pager()
  WT_DLG_HISTORY,     ///< History Dialog,     dlg_select_history()
  WT_DLG_INDEX,       ///< Index Dialog,       index_pager_init()
  WT_DLG_PATTERN,     ///< Pattern Dialog,     create_pattern_menu()
  WT_DLG_PGP,         ///< Pgp Dialog,         dlg_select_pgp_key()
  WT_DLG_POSTPONE,    ///< Postpone Dialog,    dlg_select_postponed_email()
  WT_DLG_QUERY,       ///< Query Dialog,       dlg_select_query()
  WT_DLG_REMAILER,    ///< Remailer Dialog,    dlg_select_mixmaster_chain()
  WT_DLG_SMIME,       ///< Smime Dialog,       dlg_select_smime_key()

  // Common Windows
  WT_CUSTOM,          ///< Window with a custom drawing function
  WT_HELP_BAR,        ///< Help Bar containing list of useful key bindings
  WT_INDEX,           ///< An Index Window containing a selection list
  WT_INDEX_BAR,       ///< Index Bar containing status info about the Index
  WT_MESSAGE,         ///< Window for messages/errors and command entry
  WT_PAGER,           ///< Window containing paged free-form text
  WT_PAGER_BAR,       ///< Pager Bar containing status info about the Pager
  WT_SIDEBAR,         ///< Side panel containing Accounts or groups of data
};

TAILQ_HEAD(MuttWindowList, MuttWindow);

typedef uint8_t WindowActionFlags; ///< Actions waiting to be performed on a MuttWindow
#define WA_NO_FLAGS        0       ///< No flags are set
#define WA_REFLOW    (1 << 0)      ///< Reflow the Window and its children
#define WA_RECALC    (1 << 1)      ///< Recalculate the contents of the Window
#define WA_REPAINT   (1 << 2)      ///< Redraw the contents of the Window

/**
 * struct MuttWindow - A division of the screen
 *
 * Windows for different parts of the screen
 */
struct MuttWindow
{
  short req_cols;                    ///< Number of columns required
  short req_rows;                    ///< Number of rows required

  struct WindowState state;          ///< Current state of the Window
  struct WindowState old;            ///< Previous state of the Window

  enum MuttWindowOrientation orient; ///< Which direction the Window will expand
  enum MuttWindowSize size;          ///< Type of Window, e.g. #MUTT_WIN_SIZE_FIXED
  WindowActionFlags actions;         ///< Actions to be performed, e.g. #WA_RECALC

  TAILQ_ENTRY(MuttWindow) entries;   ///< Linked list
  struct MuttWindow *parent;         ///< Parent Window
  struct MuttWindowList children;    ///< Children Windows

  struct Notify *notify;             ///< Notifications system

  struct MuttWindow *focus;          ///< Focussed Window
  int help_menu;                     ///< Menu for key bindings, e.g. #MENU_PAGER
  const struct Mapping *help_data;   ///< Data for the Help Bar

  enum WindowType type;              ///< Window type, e.g. #WT_SIDEBAR
  void *wdata;                       ///< Private data

  /**
   * wdata_free - Free the private data attached to the MuttWindow
   * @param win Window
   * @param ptr Window data to free
   */
  void (*wdata_free)(struct MuttWindow *win, void **ptr);

  /**
   * recalc - Recalculate the Window data
   * @param win Window
   * @retval  0 Success
   * @retval -1 Error
   */
  int (*recalc)(struct MuttWindow *win);

  /**
   * repaint - Repaint the Window
   * @param win Window
   * @retval  0 Success
   * @retval -1 Error
   */
  int (*repaint)(struct MuttWindow *win);
};

typedef uint8_t WindowNotifyFlags; ///< Changes to a MuttWindow
#define WN_NO_FLAGS        0       ///< No flags are set
#define WN_TALLER    (1 << 0)      ///< Window became taller
#define WN_SHORTER   (1 << 1)      ///< Window became shorter
#define WN_WIDER     (1 << 2)      ///< Window became wider
#define WN_NARROWER  (1 << 3)      ///< Window became narrower
#define WN_MOVED     (1 << 4)      ///< Window moved
#define WN_VISIBLE   (1 << 5)      ///< Window became visible
#define WN_HIDDEN    (1 << 6)      ///< Window became hidden

/**
 * enum NotifyWindow - Window notification types
 *
 * Observers of #NT_WINDOW will be passed an #EventWindow.
 */
enum NotifyWindow
{
  NT_WINDOW_NEW = 1, ///< New Window has been added
  NT_WINDOW_DELETE,  ///< Window is about to be deleted
  NT_WINDOW_STATE,   ///< Window state has changed, e.g. #WN_VISIBLE
  NT_WINDOW_DIALOG,  ///< A new Dialog Window has been created, e.g. #WT_DLG_INDEX
  NT_WINDOW_FOCUS,   ///< Window focus has changed
};

/**
 * struct EventWindow - An Event that happened to a Window
 */
struct EventWindow
{
  struct MuttWindow *win;  ///< Window that changed
  WindowNotifyFlags flags; ///< Attributes of Window that changed
};

extern struct MuttWindow *RootWindow;
extern struct MuttWindow *AllDialogsWindow;
extern struct MuttWindow *MessageWindow;

// Functions that deal with the Window
void               mutt_window_add_child          (struct MuttWindow *parent, struct MuttWindow *child);
void               mutt_window_free               (struct MuttWindow **ptr);
void               mutt_window_free_all           (void);
void               mutt_window_get_coords         (struct MuttWindow *win, int *col, int *row);
void               mutt_window_init               (void);
struct MuttWindow *mutt_window_new                (enum WindowType type, enum MuttWindowOrientation orient, enum MuttWindowSize size, int cols, int rows);
void               mutt_window_reflow             (struct MuttWindow *win);
void               mutt_window_reflow_message_rows(int mw_rows);
struct MuttWindow *mutt_window_remove_child       (struct MuttWindow *parent, struct MuttWindow *child);
void               mutt_window_set_root           (int cols, int rows);
int                mutt_window_wrap_cols          (int width, short wrap);

// Functions for drawing on the Window
int  mutt_window_addch    (int ch);
int  mutt_window_addnstr  (const char *str, int num);
int  mutt_window_addstr   (const char *str);
void mutt_window_clearline(struct MuttWindow *win, int row);
void mutt_window_clear    (struct MuttWindow *win);
void mutt_window_clrtoeol (struct MuttWindow *win);
int  mutt_window_move     (struct MuttWindow *win, int col, int row);
void mutt_window_move_abs (int col, int row);
int  mutt_window_mvaddstr (struct MuttWindow *win, int col, int row, const char *str);
int  mutt_window_mvprintw (struct MuttWindow *win, int col, int row, const char *fmt, ...);
int  mutt_window_printf   (const char *format, ...);
bool mutt_window_is_visible(struct MuttWindow *win);

void               mutt_winlist_free (struct MuttWindowList *head);
struct MuttWindow *mutt_window_find  (struct MuttWindow *root, enum WindowType type);
void               window_notify_all (struct MuttWindow *win);
void               window_set_visible(struct MuttWindow *win, bool visible);
void               window_set_focus  (struct MuttWindow *win);
struct MuttWindow *window_get_focus  (void);

void window_redraw(struct MuttWindow *win, bool force);

#endif /* MUTT_MUTT_WINDOW_H */
