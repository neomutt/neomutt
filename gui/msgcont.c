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
#include "msgcont.h"
#include "mutt_window.h"
#ifdef USE_DEBUG_WINDOW
#include "debug/lib.h"
#endif

/// Window acting as a stack for the message windows
struct MuttWindow *MessageContainer = NULL;

/**
 * msgcont_new - Create a new Message Container
 * @retval ptr New Container Window
 */
struct MuttWindow *msgcont_new(void)
{
  MessageContainer = mutt_window_new(WT_CONTAINER, MUTT_WIN_ORIENT_VERTICAL,
                                     MUTT_WIN_SIZE_MINIMISE, MUTT_WIN_SIZE_UNLIMITED, 1);
  return MessageContainer;
}

/**
 * msgcont_pop_window - Remove the last Window from the Container Stack
 * @retval ptr Window removed from the stack
 */
struct MuttWindow *msgcont_pop_window(void)
{
  if (!MessageContainer)
    return NULL;

  struct MuttWindow **wp_pop = ARRAY_LAST(&MessageContainer->children);
  if (!wp_pop)
    return NULL;

  struct MuttWindow *win_pop = *wp_pop;

  // Don't pop the last entry (check if there's a previous one)
  if (ARRAY_SIZE(&MessageContainer->children) <= 1)
    return NULL;

  // Hide the old window
  window_set_visible(win_pop, false);

  // Get the window that will become top of stack
  struct MuttWindow **wp_top = ARRAY_GET(&MessageContainer->children,
                                         ARRAY_SIZE(&MessageContainer->children) - 2);
  struct MuttWindow *win_top = wp_top ? *wp_top : NULL;

  ARRAY_REMOVE(&MessageContainer->children, wp_pop);

  if (win_top)
  {
    window_set_visible(win_top, true);
    win_top->actions |= WA_RECALC;
  }

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
  if (!MessageContainer || !win)
    return;

  // Hide the current top window
  struct MuttWindow **wp_top = ARRAY_LAST(&MessageContainer->children);
  if (wp_top)
    window_set_visible(*wp_top, false);

  mutt_window_add_child(MessageContainer, win);
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
  if (!MessageContainer)
    return NULL;

  struct MuttWindow **wp = ARRAY_FIRST(&MessageContainer->children);
  if (!wp)
    return NULL;

  struct MuttWindow *win = *wp;
  if (win->type != WT_MESSAGE)
    return NULL;

  return win;
}
