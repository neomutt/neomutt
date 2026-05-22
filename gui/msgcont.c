/**
 * @file
 * Message Container
 *
 * @authors
 * Copyright (C) 2022-2023 Richard Russon <rich@flatcap.org>
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
 * @page gui_msgcont Message Container
 *
 * Message Container
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "msgcont.h"
#include "module_data.h"
#include "msgwin.h"
#include "mutt_window.h"
#ifdef USE_DEBUG_WINDOW
#include "debug/lib.h"
#endif

/**
 * msgcont_new - Create a new Message Container
 * @retval ptr New Container Window
 */
struct MuttWindow *msgcont_new(void)
{
  struct GuiModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_GUI);
  struct MuttWindow *win = mutt_window_new(WT_CONTAINER, MUTT_WIN_ORIENT_VERTICAL,
                                           MUTT_WIN_SIZE_MAXIMISE,
                                           MUTT_WIN_SIZE_UNLIMITED, 1);
  if (mod_data)
    mod_data->message_container = win;
  return win;
}

/**
 * msgcont_recalc_rows - Recalculate the Bottom Bar height
 *
 * Sum the req_rows of all visible children of the Message Container and
 * apply the result to the Bottom Bar's req_rows.  The caller is responsible
 * for triggering a reflow if needed.
 */
void msgcont_recalc_rows(void)
{
  struct GuiModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_GUI);
  if (!mod_data || !mod_data->message_container || !mod_data->bottom_bar)
    return;

  struct MuttWindow *mc = mod_data->message_container;
  int rows = 0;
  struct MuttWindow **wp = NULL;
  ARRAY_FOREACH(wp, &mc->children)
  {
    if (!(*wp)->state.visible)
      continue;
    rows += MAX(0, (*wp)->req_rows);
  }

  if (rows < 1)
    rows = 1;

  mod_data->bottom_bar->req_rows = rows;
}

/**
 * msgcont_num_windows - Count the Windows in the Message Container
 * @retval num Number of children windows
 */
int msgcont_num_windows(void)
{
  struct GuiModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_GUI);
  if (!mod_data || !mod_data->message_container)
    return 0;

  return ARRAY_SIZE(&mod_data->message_container->children);
}

/**
 * msgcont_pop_window - Remove the last Window from the Container Stack
 * @retval ptr Window removed from the stack
 */
struct MuttWindow *msgcont_pop_window(void)
{
  struct GuiModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_GUI);
  if (!mod_data || !mod_data->message_container)
    return NULL;

  struct MuttWindow *mc = mod_data->message_container;

  struct MuttWindow **wp_pop = ARRAY_LAST(&mc->children);
  if (!wp_pop)
    return NULL;

  struct MuttWindow *win_pop = *wp_pop;

  // Don't pop the last entry (check if there's a previous one)
  if (ARRAY_SIZE(&mc->children) <= 1)
    return NULL;

  // Hide the old window
  window_set_visible(win_pop, false);

  // Get the window that will become top of stack
  struct MuttWindow **wp_top = ARRAY_GET(&mc->children, ARRAY_SIZE(&mc->children) - 2);
  struct MuttWindow *win_top = wp_top ? *wp_top : NULL;

  ARRAY_REMOVE(&mc->children, wp_pop);

  if (win_top)
  {
    window_set_visible(win_top, true);
    win_top->actions |= WA_RECALC;
  }

  msgcont_recalc_rows();

  mutt_window_reflow(NULL);
  window_redraw(NULL);
#ifdef USE_DEBUG_WINDOW
  debug_win_dump();
#endif
  return win_pop;
}

/**
 * msgcont_push_window - Add a window to the Container Stack
 * @param win Window to add
 */
void msgcont_push_window(struct MuttWindow *win)
{
  struct GuiModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_GUI);
  if (!mod_data || !mod_data->message_container || !win)
    return;

  struct MuttWindow *mc = mod_data->message_container;
  const bool keep_msgwin = msgwin_has_text();

  // Hide all currently visible children: the new window will take over the
  // space.  If the Message Window currently holds any text, it should remain
  // visible so the user still sees the message.
  struct MuttWindow **wp = NULL;
  ARRAY_FOREACH(wp, &mc->children)
  {
    if (!(*wp)->state.visible)
      continue;
    if (keep_msgwin && ((*wp)->type == WT_MESSAGE))
      continue;
    window_set_visible(*wp, false);
  }

  mutt_window_add_child(mc, win);

  msgcont_recalc_rows();

  mutt_window_reflow(NULL);
  window_redraw(NULL);
#ifdef USE_DEBUG_WINDOW
  debug_win_dump();
#endif
}

/**
 * msgcont_get_msgwin - Get the Message Window
 * @retval ptr Message Window
 *
 * The Message Window is the first child of the MessageContainer and will have
 * type WT_MESSAGE.
 */
struct MuttWindow *msgcont_get_msgwin(void)
{
  struct GuiModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_GUI);
  if (!mod_data || !mod_data->message_container)
    return NULL;

  struct MuttWindow **wp = ARRAY_FIRST(&mod_data->message_container->children);
  if (!wp)
    return NULL;

  struct MuttWindow *win = *wp;
  if (win->type != WT_MESSAGE)
    return NULL;

  return win;
}
