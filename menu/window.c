/**
 * @file
 * Window wrapper around a Menu
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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
 * @page menu_window Window wrapper around a Menu
 *
 * The Menu Window is an interactive window that allows a user to work with a
 * list of items.  The Menu can be configured to allow single or multiple
 * selections and it can handle arbitrary data, sorting, custom colouring and
 * searching.
 *
 * ## Windows
 *
 * | Name | Type     | Constructor   |
 * | :--- | :------- | :------------ |
 * | Menu | #WT_MENU | menu_window() |
 *
 * **Parent**
 *
 * The Menu Window has many possible parents, e.g.
 *
 * - @ref index_dlg_index
 * - @ref compose_dialog
 * - ...
 *
 * **Children**
 * - None
 *
 * ## Data
 * - #Menu
 * - Menu::mdata
 *
 * The Menu Window stores it state info in struct Menu.
 * Users of the Menu Window can store custom data in Menu::mdata.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type            | Handler                 |
 * | :-------------------- | :---------------------- |
 * | #NT_COLOR             | menu_color_observer()   |
 * | #NT_CONFIG            | menu_config_observer()  |
 * | #NT_WINDOW            | menu_window_observer()  |
 * | MuttWindow::recalc()  | menu_recalc()           |
 * | MuttWindow::repaint() | menu_repaint()          |
 */

#include "config.h"
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "type.h"

struct ConfigSubset;

/**
 * menu_recalc - Recalculate the Window data - Implements MuttWindow::recalc() - @ingroup window_recalc
 */
static int menu_recalc(struct MuttWindow *win)
{
  if (win->type != WT_MENU)
    return 0;

  // struct Menu *menu = win->wdata;

  win->actions |= WA_REPAINT;
  mutt_debug(LL_DEBUG5, "recalc done, request WA_REPAINT\n");
  return 0;
}

/**
 * menu_repaint - Repaint the Window - Implements MuttWindow::repaint() - @ingroup window_repaint
 */
static int menu_repaint(struct MuttWindow *win)
{
  if (win->type != WT_MENU)
    return 0;

  struct Menu *menu = win->wdata;
  menu->redraw |= MENU_REDRAW_FULL;
  menu_redraw(menu);
  menu->redraw = MENU_REDRAW_NO_FLAGS;

  const bool c_arrow_cursor = cs_subset_bool(menu->sub, "arrow_cursor");
  const bool c_braille_friendly = cs_subset_bool(menu->sub, "braille_friendly");

  /* move the cursor out of the way */
  if (c_arrow_cursor)
    mutt_window_move(menu->win, 2, menu->current - menu->top);
  else if (c_braille_friendly)
    mutt_window_move(menu->win, 0, menu->current - menu->top);
  else
  {
    mutt_window_move(menu->win, menu->win->state.cols - 1, menu->current - menu->top);
  }

  mutt_debug(LL_DEBUG5, "repaint done\n");
  return 0;
}

/**
 * menu_wdata_free - Destroy a Menu Window - Implements MuttWindow::wdata_free() - @ingroup window_wdata_free
 */
static void menu_wdata_free(struct MuttWindow *win, void **ptr)
{
  menu_free((struct Menu **) ptr);
}

/**
 * menu_new_window - Create a new Menu Window
 * @param type Menu type, e.g. #MENU_PAGER
 * @param sub  Config items
 * @retval ptr New MuttWindow wrapping a Menu
 */
struct MuttWindow *menu_new_window(enum MenuType type, struct ConfigSubset *sub)
{
  struct MuttWindow *win =
      mutt_window_new(WT_MENU, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);

  struct Menu *menu = menu_new(type, win, sub);

  win->recalc = menu_recalc;
  win->repaint = menu_repaint;
  win->wdata = menu;
  win->wdata_free = menu_wdata_free;
  win->actions |= WA_RECALC;

  return win;
}
