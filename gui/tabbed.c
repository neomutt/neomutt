/**
 * @file
 * Tabbed Container Window
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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
 * @page gui_tabbed Tabbed Container Window
 *
 * Tabbed Container Window
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "tabbed.h"
#include "color/lib.h"
#include "key/lib.h"
#include "mutt_curses.h"
#include "mutt_window.h"
#include "opcodes.h"
#ifdef USE_DEBUG_WINDOW
#include "debug/lib.h"
#endif

/**
 * tabbar_recalc - Recalculate the display of the Tabbed Window - Implements MuttWindow::recalc() - @ingroup window_recalc
 */
static int tabbar_recalc(struct MuttWindow *win)
{
  win->actions |= WA_REPAINT;
  mutt_debug(LL_DEBUG5, "recalc done, request WA_REPAINT\n");
  return 0;
}

/**
 * tabbar_repaint - Redraw the Tabbed Window - Implements MuttWindow::repaint() - @ingroup window_repaint
 */
static int tabbar_repaint(struct MuttWindow *win)
{
  struct TabWinWindowData *wdata = win->wdata;

  const char **pstr = NULL;
  struct MuttWindow **pwin = NULL;

  mutt_window_move(win, 0, 0);
  for (int i = 0; i < win->state.cols; i++)
    mutt_window_addstr(win, "⎽");

  mutt_window_move(win, 0, 1);
  ARRAY_FOREACH(pwin, &wdata->tabs)
  {
    pstr = ARRAY_GET(&wdata->names, ARRAY_FOREACH_IDX_pwin);

    int num = ARRAY_FOREACH_IDX_pwin + 1;

    if (num == wdata->active_tab)
    {
      mutt_curses_set_color_by_id(MT_COLOR_BOLD);
      mutt_window_printf(win, "◤ %d: %s ◥", num, *pstr);
    }
    else
    {
      mutt_curses_set_color_by_id(MT_COLOR_NORMAL);
      mutt_window_printf(win, "╱ %d: %s ╲", num, *pstr);
    }
  }

  mutt_curses_set_color_by_id(MT_COLOR_NORMAL);

  mutt_debug(LL_DEBUG5, "win repaint done\n");
  return 0;
}

/**
 * tabwin_recalc - Recalculate the display of the Tabbed Window - Implements MuttWindow::recalc() - @ingroup window_recalc
 */
static int tabwin_recalc(struct MuttWindow *win)
{
  struct TabWinWindowData *wdata = win->wdata;

  if (ARRAY_SIZE(&wdata->tabs) == 0)
    return 0;

  wdata->active_tab = CLAMP(wdata->active_tab, 1, ARRAY_SIZE(&wdata->tabs));

  struct MuttWindow **ptr = NULL;
  ARRAY_FOREACH(ptr, &wdata->tabs)
  {
    struct MuttWindow *tab = *ptr;

    bool active = ((ARRAY_FOREACH_IDX_ptr + 1) == wdata->active_tab);
    window_set_visible(tab, active);
  }

  win->actions |= WA_REPAINT;
  mutt_debug(LL_DEBUG5, "recalc done, request WA_REPAINT\n");

  return 0;
}

/**
 * tabwin_repaint - Redraw the Tabbed Window - Implements MuttWindow::repaint() - @ingroup window_repaint
 */
static int tabwin_repaint(struct MuttWindow *win)
{
  mutt_debug(LL_DEBUG5, "win repaint done\n");
  return 0;
}

/**
 * cont_recalc - Recalculate the display of the Tabbed Window - Implements MuttWindow::recalc() - @ingroup window_recalc
 */
static int cont_recalc(struct MuttWindow *win)
{
  win->actions |= WA_REPAINT;
  mutt_debug(LL_DEBUG5, "recalc done, request WA_REPAINT\n");

  return 0;
}

/**
 * cont_repaint - Redraw the Tabbed Window - Implements MuttWindow::repaint() - @ingroup window_repaint
 */
static int cont_repaint(struct MuttWindow *win)
{
  mutt_debug(LL_DEBUG5, "win repaint done\n");
  return 0;
}

/**
 * tabwin_wdata_new - Create private data for a Tabbed Window
 * @retval ptr New private data
 */
static struct TabWinWindowData *tabwin_wdata_new(void)
{
  struct TabWinWindowData *wdata = MUTT_MEM_CALLOC(1, struct TabWinWindowData);

  wdata->wrap_around = true;

  return wdata;
}

/**
 * tabwin_wdata_free - Free the private data - Implements MuttWindow::wdata_free() - @ingroup window_wdata_free
 */
static void tabwin_wdata_free(struct MuttWindow *win, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct TabWinWindowData *wdata = *ptr;

  const char **pstr = NULL;
  ARRAY_FOREACH(pstr, &wdata->names)
  {
    FREE(pstr);
  }

  ARRAY_FREE(&wdata->names);
  ARRAY_FREE(&wdata->tabs);

  FREE(ptr);
}

/**
 * tabwin_new - Create a Tabbed Window
 * @retval ptr New Tabbed Window
 */
struct MuttWindow *tabwin_new(void)
{
  struct MuttWindow *win = mutt_window_new(WT_TABBED, MUTT_WIN_ORIENT_VERTICAL,
                                           MUTT_WIN_SIZE_MAXIMISE, MUTT_WIN_SIZE_UNLIMITED,
                                           MUTT_WIN_SIZE_UNLIMITED);

  struct TabWinWindowData *wdata = tabwin_wdata_new();

  win->wdata = wdata;
  win->wdata_free = tabwin_wdata_free;
  win->recalc = tabwin_recalc;
  win->repaint = tabwin_repaint;

  struct MuttWindow *tabbar = mutt_window_new(WT_CUSTOM, MUTT_WIN_ORIENT_VERTICAL,
                                              MUTT_WIN_SIZE_FIXED,
                                              MUTT_WIN_SIZE_UNLIMITED, 1);
  tabbar->wdata = wdata;
  tabbar->recalc = tabbar_recalc;
  tabbar->repaint = tabbar_repaint;
  mutt_window_add_child(win, tabbar);

  struct MuttWindow *cont = mutt_window_new(WT_CONTAINER, MUTT_WIN_ORIENT_VERTICAL,
                                            MUTT_WIN_SIZE_MAXIMISE, MUTT_WIN_SIZE_UNLIMITED,
                                            MUTT_WIN_SIZE_UNLIMITED);

  cont->wdata = wdata;
  cont->recalc = cont_recalc;
  cont->repaint = cont_repaint;
  mutt_window_add_child(win, cont);

  wdata->tabbar = tabbar;
  wdata->cont = cont;

  return win;
}

/**
 * tabwin_add_child - Add a child to a Tabbed Window
 * @param win   Tabbed Window
 * @param name  Tab name
 * @param child Window to add
 * @retval -1 Always
 */
int tabwin_add_child(struct MuttWindow *win, const char *name, struct MuttWindow *child)
{
  struct TabWinWindowData *wdata = win->wdata;

  if (ARRAY_EMPTY(&wdata->tabs))
  {
    wdata->active_tab = 1;
    window_set_visible(child, true);
  }
  else
    window_set_visible(child, false);

  mutt_window_add_child(wdata->cont, child);
  ARRAY_ADD(&wdata->tabs, child);
  ARRAY_ADD(&wdata->names, mutt_str_dup(name));

  mutt_window_reflow(win);

  return -1;
}

/**
 * tabwin_select_tab - Select a Tab
 * @param win   Tabbed Window
 * @param wdata Window private data
 * @param num   Tab number, starting at one
 * @retval true  Selection changed
 * @retval false Selection unchanged
 */
static bool tabwin_select_tab(struct MuttWindow *win, struct TabWinWindowData *wdata, int num)
{
  if ((num < 1) || (num > ARRAY_SIZE(&wdata->tabs)))
    return false;

  if (num == wdata->active_tab)
    return false;

  struct MuttWindow *old_win = *ARRAY_GET(&wdata->tabs, wdata->active_tab - 1);
  struct MuttWindow *new_win = *ARRAY_GET(&wdata->tabs, num - 1);

  window_set_visible(old_win, false);
  window_set_visible(new_win, true);

  struct MuttWindow *focus = new_win->focus ? new_win->focus : new_win;
  window_set_focus(focus);
  mutt_window_reflow(wdata->cont);

  wdata->active_tab = num;
  new_win->actions |= WA_RECALC | WA_REPAINT;

  wdata->tabbar->actions |= WA_RECALC | WA_REPAINT;
  wdata->cont->actions |= WA_RECALC | WA_REPAINT;

#ifdef USE_DEBUG_WINDOW
  debug_win_dump();
#endif
  return true;
}

/**
 * tabwin_select_tab_first - Select the first Tab
 * @param win Tabbed Window
 * @retval true  Selection changed
 * @retval false Selection unchanged
 */
bool tabwin_select_tab_first(struct MuttWindow *win)
{
  struct TabWinWindowData *wdata = win->wdata;

  return tabwin_select_tab(win, wdata, 1);
}

/**
 * tabwin_select_tab_last - Select the last Tab
 * @param win Tabbed Window
 * @retval true  Selection changed
 * @retval false Selection unchanged
 */
bool tabwin_select_tab_last(struct MuttWindow *win)
{
  struct TabWinWindowData *wdata = win->wdata;

  return tabwin_select_tab(win, wdata, ARRAY_SIZE(&wdata->tabs));
}

/**
 * tabwin_select_tab_num - Select a Tab by number
 * @param win Tabbed Window
 * @param num Tab number, starting at one
 * @retval true  Selection changed
 * @retval false Selection unchanged
 */
bool tabwin_select_tab_num(struct MuttWindow *win, int num)
{
  struct TabWinWindowData *wdata = win->wdata;

  return tabwin_select_tab(win, wdata, num);
}

/**
 * tabwin_select_tab_next - Select the next Tab
 * @param win Tabbed Window
 * @retval true  Selection changed
 * @retval false Selection unchanged
 */
bool tabwin_select_tab_next(struct MuttWindow *win)
{
  struct TabWinWindowData *wdata = win->wdata;

  if (wdata->active_tab < ARRAY_SIZE(&wdata->tabs))
    return tabwin_select_tab(win, wdata, wdata->active_tab + 1);

  if (wdata->wrap_around)
    return tabwin_select_tab(win, wdata, 1);

  return false;
}

/**
 * tabwin_select_tab_prev - Select the previous Tab
 * @param win Tabbed Window
 * @retval true  Selection changed
 * @retval false Selection unchanged
 */
bool tabwin_select_tab_prev(struct MuttWindow *win)
{
  struct TabWinWindowData *wdata = win->wdata;

  if (wdata->active_tab > 1)
    return tabwin_select_tab(win, wdata, wdata->active_tab - 1);

  if (wdata->wrap_around)
    return tabwin_select_tab(win, wdata, ARRAY_SIZE(&wdata->tabs));

  return false;
}

// -----------------------------------------------------------------------------

/**
 * op_exit - Close the Tabbed Window - Implements ::tabbed_function_t - @ingroup tabbed_function_api
 */
static int op_exit(struct MuttWindow *win, int op)
{
  return FR_DONE;
}

/**
 * op_tab_switch - Select a different Tab - Implements ::tabbed_function_t - @ingroup tabbed_function_api
 */
static int op_tab_switch(struct MuttWindow *win, int op)
{
  bool rc = false;

  switch (op)
  {
    case OP_TAB_SWITCH_1:
    case OP_TAB_SWITCH_2:
    case OP_TAB_SWITCH_3:
    case OP_TAB_SWITCH_4:
    case OP_TAB_SWITCH_5:
    case OP_TAB_SWITCH_6:
    case OP_TAB_SWITCH_7:
    case OP_TAB_SWITCH_8:
    case OP_TAB_SWITCH_9:
    case OP_TAB_SWITCH_0:
      rc = tabwin_select_tab_num(win, op - OP_TAB_SWITCH_1 + 1);
      break;

    case OP_TAB_SWITCH_NEXT:
      rc = tabwin_select_tab_next(win);
      break;

    case OP_TAB_SWITCH_PREV:
      rc = tabwin_select_tab_prev(win);
      break;

    case OP_TAB_SWITCH_FIRST:
      rc = tabwin_select_tab_first(win);
      break;

    case OP_TAB_SWITCH_LAST:
      rc = tabwin_select_tab_last(win);
      break;
  }

  return rc ? FR_SUCCESS : FR_NO_ACTION;
}

/**
 * TabbedFunctions - All the NeoMutt functions that the Tabbed Window supports
 */
static const struct TabbedFunction TabbedFunctions[] = {
  // clang-format off
  { OP_EXIT,                  op_exit },
  { OP_TAB_SWITCH_1,          op_tab_switch },
  { OP_TAB_SWITCH_2,          op_tab_switch },
  { OP_TAB_SWITCH_3,          op_tab_switch },
  { OP_TAB_SWITCH_4,          op_tab_switch },
  { OP_TAB_SWITCH_5,          op_tab_switch },
  { OP_TAB_SWITCH_6,          op_tab_switch },
  { OP_TAB_SWITCH_7,          op_tab_switch },
  { OP_TAB_SWITCH_8,          op_tab_switch },
  { OP_TAB_SWITCH_9,          op_tab_switch },
  { OP_TAB_SWITCH_0,          op_tab_switch },
  { OP_TAB_SWITCH_NEXT,       op_tab_switch },
  { OP_TAB_SWITCH_PREV,       op_tab_switch },
  { OP_TAB_SWITCH_FIRST,      op_tab_switch },
  { OP_TAB_SWITCH_LAST,       op_tab_switch },
  { 0, NULL },
  // clang-format on
};

/**
 * tabbed_function_dispatcher - Perform a Tabbed Window function - Implements ::function_dispatcher_t - @ingroup dispatcher_api
 *
 * @note @a win is not used
 */
int tabbed_function_dispatcher(struct MuttWindow *win, const struct KeyEvent *event)
{
  if (!win || !event)
    return FR_ERROR;

  int rc = FR_UNKNOWN;
  const int op = event->op;

  for (size_t i = 0; TabbedFunctions[i].op != OP_NULL; i++)
  {
    const struct TabbedFunction *fn = &TabbedFunctions[i];
    if (fn->op == op)
    {
      rc = fn->function(win, op);
      break;
    }
  }

  if (rc == FR_UNKNOWN) // Not our function
    return rc;

  const char *result = dispatcher_get_retval_name(rc);
  mutt_debug(LL_DEBUG1, "Handled %s (%d) -> %s\n", opcodes_get_name(op), op, NONULL(result));

  return rc;
}
