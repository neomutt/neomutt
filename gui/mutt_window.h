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

#ifndef MUTT_GUI_MUTT_WINDOW_H
#define MUTT_GUI_MUTT_WINDOW_H

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include "mutt/lib.h"

struct ConfigSubset;

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
  WT_INDEX,           ///< A panel containing the Index Window
  WT_MENU,            ///< An Window containing a Menu
  WT_MESSAGE,         ///< Window for messages/errors and command entry
  WT_PAGER,           ///< A panel containing the Pager Window
  WT_SIDEBAR,         ///< Side panel containing Accounts or groups of data
  WT_STATUS_BAR,      ///< Status Bar containing extra info about the Index/Pager/etc
};

TAILQ_HEAD(MuttWindowList, MuttWindow);

typedef uint8_t WindowActionFlags; ///< Flags for Actions waiting to be performed on a MuttWindow, e.g. #WA_REFLOW
#define WA_NO_FLAGS        0       ///< No flags are set
#define WA_REFLOW    (1 << 0)      ///< Reflow the Window and its children
#define WA_RECALC    (1 << 1)      ///< Recalculate the contents of the Window
#define WA_REPAINT   (1 << 2)      ///< Redraw the contents of the Window

/**
 * @defgroup window_api Window API
 *
 * The Window API
 *
 * A division of the screen.
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

  struct Notify *notify;             ///< Notifications: #NotifyWindow, #EventWindow

  struct MuttWindow *focus;          ///< Focused Window
  int help_menu;                     ///< Menu for key bindings, e.g. #MENU_PAGER
  const struct Mapping *help_data;   ///< Data for the Help Bar

  enum WindowType type;              ///< Window type, e.g. #WT_SIDEBAR
  void *wdata;                       ///< Private data

  /**
   * @defgroup window_wdata_free wdata_free()
   * @ingroup window_api
   *
   * wdata_free - Free the private data attached to the MuttWindow
   * @param win Window
   * @param ptr Window data to free
   *
   * **Contract**
   * - @a win  is not NULL
   * - @a ptr  is not NULL
   * - @a *ptr is not NULL
   */
  void (*wdata_free)(struct MuttWindow *win, void **ptr);

  /**
   * @defgroup window_recalc recalc()
   * @ingroup window_api
   *
   * recalc - Recalculate the Window data
   * @param win Window
   * @retval  0 Success
   * @retval -1 Error
   */
  int (*recalc)(struct MuttWindow *win);

  /**
   * @defgroup window_repaint repaint()
   * @ingroup window_api
   *
   * repaint - Repaint the Window
   * @param win Window
   * @retval  0 Success
   * @retval -1 Error
   */
  int (*repaint)(struct MuttWindow *win);
};

typedef uint8_t WindowNotifyFlags; ///< Flags for Changes to a MuttWindow, e.g. #WN_TALLER
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
 *
 * @note Delete notifications are sent **before** the object is deleted.
 * @note Other notifications are sent **after** the event.
 */
enum NotifyWindow
{
  NT_WINDOW_ADD = 1, ///< New Window has been added
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

// Functions that deal with the Window
void               mutt_window_add_child          (struct MuttWindow *parent, struct MuttWindow *child);
void               mutt_window_free               (struct MuttWindow **ptr);
void               mutt_window_get_coords         (struct MuttWindow *win, int *col, int *row);
struct MuttWindow *mutt_window_new                (enum WindowType type, enum MuttWindowOrientation orient, enum MuttWindowSize size, int cols, int rows);
void               mutt_window_reflow             (struct MuttWindow *win);
struct MuttWindow *mutt_window_remove_child       (struct MuttWindow *parent, struct MuttWindow *child);
int                mutt_window_wrap_cols          (int width, short wrap);

// Functions for drawing on the Window
int  mutt_window_addch    (struct MuttWindow *win, int ch);
int  mutt_window_addnstr  (struct MuttWindow *win, const char *str, int num);
int  mutt_window_addstr   (struct MuttWindow *win, const char *str);
void mutt_window_clearline(struct MuttWindow *win, int row);
void mutt_window_clear    (struct MuttWindow *win);
void mutt_window_clrtoeol (struct MuttWindow *win);
int  mutt_window_move     (struct MuttWindow *win, int col, int row);
int  mutt_window_mvaddstr (struct MuttWindow *win, int col, int row, const char *str);
int  mutt_window_mvprintw (struct MuttWindow *win, int col, int row, const char *fmt, ...);
int  mutt_window_printf   (struct MuttWindow *win, const char *format, ...);
bool mutt_window_is_visible(struct MuttWindow *win);

void               mutt_winlist_free (struct MuttWindowList *head);
struct MuttWindow *window_find_child (struct MuttWindow *win, enum WindowType type);
struct MuttWindow *window_find_parent(struct MuttWindow *win, enum WindowType type);
void               window_notify_all (struct MuttWindow *win);
void               window_set_visible(struct MuttWindow *win, bool visible);
struct MuttWindow *window_set_focus  (struct MuttWindow *win);
struct MuttWindow *window_get_focus  (void);
bool               window_is_focused (struct MuttWindow *win);

void window_redraw(struct MuttWindow *win);
void window_invalidate_all(void);
const char *mutt_window_win_name(const struct MuttWindow *win);
bool window_status_on_top(struct MuttWindow *panel, struct ConfigSubset *sub);

#endif /* MUTT_GUI_MUTT_WINDOW_H */
