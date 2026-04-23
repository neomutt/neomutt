/**
 * @file
 * Utility Window
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

/**
 * @page gui_utilwin Utility Window
 *
 * The Utility Window is a small, fixed-width window displayed at the
 * bottom-right of the screen.  It is used to display short status text
 * (up to 16 characters).
 *
 * ## Windows
 *
 * | Name           | Type       | Constructor   |
 * | :------------- | :--------- | :------------ |
 * | Utility Window | #WT_CUSTOM | utilwin_new() |
 *
 * **Parent**
 * - Bottom Bar Container
 *
 * **Children**
 * - None
 *
 * ## Data
 * - #UtilWinData
 *
 * The Utility Window caches the display string.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type            | Handler                    |
 * | :-------------------- | :------------------------- |
 * | #NT_CONFIG            | utilwin_config_observer()  |
 * | #NT_WINDOW            | utilwin_window_observer()  |
 * | MuttWindow::recalc()  | utilwin_recalc()           |
 * | MuttWindow::repaint() | utilwin_repaint()          |
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "utilwin.h"
#include "color/lib.h"
#include "key/lib.h"
#include "module_data.h"
#include "mutt_curses.h"
#include "mutt_window.h"

/**
 * KeyPreviewPositionMap - Choices for '$key_preview_position'
 */
static const struct Mapping KeyPreviewPositionMap[] = {
  // clang-format off
  { "left",  KEY_PREVIEW_LEFT  },
  { "right", KEY_PREVIEW_RIGHT },
  { NULL, 0 },
  // clang-format on
};

/// Data for the $key_preview_position enumeration
const struct EnumDef KeyPreviewPositionDef = {
  "key_preview_position",
  2,
  (struct Mapping *) &KeyPreviewPositionMap,
};

/// Width of the Utility Window in columns
#define UTILWIN_COLS 10

/**
 * struct UtilWinData - Utility Window private data
 */
struct UtilWinData
{
  char *text; ///< Cached display string
};

/**
 * utilwin_recalc - Recalculate the Utility Window - Implements MuttWindow::recalc() - @ingroup window_recalc
 */
static int utilwin_recalc(struct MuttWindow *win)
{
  win->actions |= WA_REPAINT;
  mutt_debug(LL_DEBUG5, "recalc done, request WA_REPAINT\n");
  return 0;
}

/**
 * utilwin_repaint - Repaint the Utility Window - Implements MuttWindow::repaint() - @ingroup window_repaint
 *
 * Draw the text on the bottom row of the window, so that it aligns with the
 * last line of a multi-line Message Window.
 */
static int utilwin_repaint(struct MuttWindow *win)
{
  struct UtilWinData *priv = win->wdata;

  mutt_curses_set_color_by_id(MT_COLOR_NORMAL);

  // Blank any extra rows above the bottom line
  for (int r = 0; r < (win->state.rows - 1); r++)
  {
    mutt_window_move(win, r, 0);
    mutt_window_clrtoeol(win);
  }

  int row = win->state.rows - 1;
  mutt_window_move(win, row, 0);

  if (priv->text)
    mutt_window_addstr(win, priv->text);

  mutt_window_clrtoeol(win);

  mutt_debug(LL_DEBUG5, "utilwin repaint done\n");
  return 0;
}

/**
 * utilwin_wdata_free - Free the private data - Implements MuttWindow::wdata_free() - @ingroup window_wdata_free
 */
static void utilwin_wdata_free(struct MuttWindow *win, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct UtilWinData *priv = *ptr;
  FREE(&priv->text);
  FREE(ptr);
}

/**
 * utilwin_key_observer - Notification that key progress has changed - Implements ::observer_t - @ingroup observer_api
 *
 * Display key binding progress in the Utility Window.
 * Shows count prefix and/or multi-key sequences, e.g. "123ab".
 * Single-key bindings without a count prefix are not shown.
 */
static int utilwin_key_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_KEY)
    return 0;
  if (nc->event_subtype != NT_KEY_PROGRESS)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct MuttWindow *win = nc->global_data;
  struct EventKeyProgress *ev_k = nc->event_data;

  // Nothing to display — clear/hide the window
  if ((ev_k->count == 0) && (ev_k->key_len == 0))
  {
    utilwin_set_text(win, NULL);
    window_redraw(NULL);
    return 0;
  }

  struct Buffer *buf = buf_pool_get();

  if (ev_k->count > 0)
    buf_add_printf(buf, "%d", ev_k->count);

  for (int i = 0; i < ev_k->key_len; i++)
  {
    keymap_get_name(ev_k->keys[i], buf);
  }

  utilwin_set_text(win, buf_string(buf));
  buf_pool_release(&buf);
  window_redraw(NULL);

  return 0;
}

/**
 * utilwin_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 *
 * The Utility Window is affected by changes to `$key_preview_position`.
 */
static int utilwin_config_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_CONFIG)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;
  if (!mutt_str_equal(ev_c->name, "key_preview_position"))
    return 0;

  struct MuttWindow *win = nc->global_data;
  struct MuttWindow *parent = win->parent;
  if (!parent)
    return 0;

  struct GuiModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_GUI);
  if (!mod_data || !mod_data->message_container)
    return 0;

  const unsigned char c_pos = cs_subset_enum(NeoMutt->sub, "key_preview_position");
  struct MuttWindow **wp_first = ARRAY_FIRST(&parent->children);
  if (!wp_first)
    return 0;

  bool util_is_first = (*wp_first == win);
  if ((c_pos == KEY_PREVIEW_LEFT) && !util_is_first)
  {
    mutt_window_swap(parent, win, mod_data->message_container);
    mutt_window_reflow(parent);
  }
  else if ((c_pos == KEY_PREVIEW_RIGHT) && util_is_first)
  {
    mutt_window_swap(parent, win, mod_data->message_container);
    mutt_window_reflow(parent);
  }

  return 0;
}

/**
 * utilwin_window_observer - Notification that a Window has changed - Implements ::observer_t - @ingroup observer_api
 */
static int utilwin_window_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_WINDOW)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct MuttWindow *win = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != win)
    return 0;

  if (nc->event_subtype == NT_WINDOW_STATE)
  {
    win->actions |= WA_REPAINT;
    mutt_debug(LL_DEBUG5, "window state done, request WA_REPAINT\n");
  }
  else if (nc->event_subtype == NT_WINDOW_DELETE)
  {
    notify_observer_remove(NeoMutt->sub->notify, utilwin_config_observer, win);
    notify_observer_remove(NeoMutt->notify, utilwin_key_observer, win);
    notify_observer_remove(win->notify, utilwin_window_observer, win);
    mutt_debug(LL_DEBUG5, "window delete done\n");
  }

  return 0;
}

/**
 * utilwin_new - Create the Utility Window
 * @retval ptr New Utility Window
 *
 * The window starts hidden and becomes visible when text is set.
 */
struct MuttWindow *utilwin_new(void)
{
  struct MuttWindow *win = mutt_window_new(WT_CUSTOM, MUTT_WIN_ORIENT_VERTICAL,
                                           MUTT_WIN_SIZE_FIXED, UTILWIN_COLS, 1);

  struct UtilWinData *priv = MUTT_MEM_CALLOC(1, struct UtilWinData);

  win->wdata = priv;
  win->wdata_free = utilwin_wdata_free;
  win->recalc = utilwin_recalc;
  win->repaint = utilwin_repaint;

  window_set_visible(win, false);

  struct GuiModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_GUI);
  if (mod_data)
    mod_data->utility_window = win;

  notify_observer_add(NeoMutt->sub->notify, NT_CONFIG, utilwin_config_observer, win);
  notify_observer_add(NeoMutt->notify, NT_KEY, utilwin_key_observer, win);
  notify_observer_add(win->notify, NT_WINDOW, utilwin_window_observer, win);

  return win;
}

/**
 * utilwin_get_text - Get the text from the Utility Window
 * @param win Utility Window
 * @retval ptr  Text string
 * @retval NULL No text or invalid window
 *
 * @note Do not free the returned string
 */
const char *utilwin_get_text(struct MuttWindow *win)
{
  if (!win)
  {
    struct GuiModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_GUI);
    if (mod_data)
      win = mod_data->utility_window;
  }
  if (!win)
    return NULL;

  struct UtilWinData *priv = win->wdata;
  return priv->text;
}

/**
 * utilwin_set_text - Set the text for the Utility Window
 * @param win  Utility Window (may be NULL to use the default)
 * @param text Text to display (NULL or empty to hide)
 *
 * Setting text makes the window visible; clearing it hides the window.
 *
 * @note The text string will be copied
 */
void utilwin_set_text(struct MuttWindow *win, const char *text)
{
  if (!win)
  {
    struct GuiModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_GUI);
    if (mod_data)
      win = mod_data->utility_window;
  }
  if (!win)
    return;

  struct UtilWinData *priv = win->wdata;

  if (!text || (*text == '\0'))
  {
    // Clear the old on-screen contents before hiding the window.
    // Hidden windows are skipped by repaint, so they can't erase themselves.
    mutt_window_clear(win);
    FREE(&priv->text);
    window_set_visible(win, false);
    mutt_window_reflow(NULL);
    return;
  }

  mutt_str_replace(&priv->text, text);

  window_set_visible(win, true);
  win->actions |= WA_RECALC;
  mutt_window_reflow(NULL);
}
