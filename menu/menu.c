/**
 * @file
 * GUI present the user with a selectable list
 *
 * @authors
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020 R Primus <rprimus@gmail.com>
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
 * @page menu_menu GUI present the user with a selectable list
 *
 * GUI present the user with a selectable list
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "private.h"
#include "mutt/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "color/lib.h"
#include "expando/lib.h" // IWYU pragma: keep
#include "key/lib.h"
#include "key/get.h"
#include "type.h"

struct ConfigSubset;

/**
 * default_color - Get the default colour for a line of the menu - Implements Menu::color() - @ingroup menu_color
 */
static const struct AttrColor *default_color(struct Menu *menu, int line)
{
  return simple_color_get(MT_COLOR_NORMAL);
}

/**
 * generic_search - Search a menu for a item matching a regex - Implements Menu::search() - @ingroup menu_search
 */
static int generic_search(struct Menu *menu, regex_t *rx, int line)
{
  struct Buffer *buf = buf_pool_get();

  menu->make_entry(menu, line, -1, buf);
  int rc = regexec(rx, buf->data, 0, NULL, 0);
  buf_pool_release(&buf);

  return rc;
}

/**
 * menu_init2 - Initialise all the Menus
 * @param search_buffers Array of search buffer pointers to initialise
 */
void menu_init2(char **search_buffers)
{
  for (int i = 0; i < MENU_MAX; i++)
    search_buffers[i] = NULL;
}

/**
 * menu_get_current_type - Get the type of the current Window
 * @retval enum Menu Type, e.g. #MENU_PAGER
 */
enum MenuType menu_get_current_type(void)
{
  struct MuttWindow *win = window_get_focus();

  // This should only happen before the first window is created
  if (!win)
    return MENU_INDEX;

  if ((win->type == WT_CUSTOM) && (win->parent->type == WT_PAGER))
    return MENU_PAGER;

  if (win->type != WT_MENU)
    return MENU_GENERIC;

  struct Menu *menu = win->wdata;
  if (!menu)
    return MENU_GENERIC;

  return menu->md->id;
}

/**
 * menu_free - Free a Menu
 * @param ptr Menu to free
 */
void menu_free(struct Menu **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Menu *menu = *ptr;

  notify_free(&menu->notify);

  if (menu->mdata_free && menu->mdata)
    menu->mdata_free(menu, &menu->mdata); // Custom function to free private data

  FREE(ptr);
}

/**
 * menu_new - Create a new Menu
 * @param md  Menu Definition
 * @param win Parent Window
 * @param sub Config items
 * @retval ptr New Menu
 */
struct Menu *menu_new(const struct MenuDefinition *md, struct MuttWindow *win,
                      struct ConfigSubset *sub)
{
  struct Menu *menu = MUTT_MEM_CALLOC(1, struct Menu);

  menu->md = md;
  menu->redraw = MENU_REDRAW_FULL;
  menu->color = default_color;
  menu->search = generic_search;
  menu->notify = notify_new();
  menu->win = win;
  menu->page_len = win->state.rows;
  menu->sub = sub;
  menu->show_indicator = true;

  notify_set_parent(menu->notify, win->notify);
  menu_add_observers(menu);

  return menu;
}

/**
 * menu_get_index - Get the current selection in the Menu
 * @param menu Menu
 * @retval num Index of selection
 */
int menu_get_index(struct Menu *menu)
{
  if (!menu)
    return -1;

  return menu->current;
}

/**
 * menu_get_index_by_coords - Find a visible entry from screen coordinates
 * @param menu Menu
 * @param row  Screen row
 * @param col  Screen column
 * @retval num Index of entry
 * @retval  -1 Click is outside the visible Menu entries
 */
int menu_get_index_by_coords(const struct Menu *menu, int row, int col)
{
  if (!menu || !menu->win || !mutt_window_is_visible(menu->win))
    return -1;

  const struct WindowState *wstate = &menu->win->state;
  if ((row < wstate->row_offset) || (row >= (wstate->row_offset + wstate->rows)) ||
      (col < wstate->col_offset) || (col >= (wstate->col_offset + wstate->cols)))
  {
    return -1;
  }

  const int index = menu->top + row - wstate->row_offset;
  if ((index < 0) || (index >= menu->max))
    return -1;

  return index;
}

/**
 * menu_set_index - Set the current selection in the Menu
 * @param menu  Menu
 * @param index Item to select
 * @retval enum #MenuRedrawFlags, e.g. #MENU_REDRAW_INDEX
 */
MenuRedrawFlags menu_set_index(struct Menu *menu, int index)
{
  return menu_move_selection(menu, index);
}

/**
 * menu_mouse_translate - Translate a mouse event for a Menu window
 * @param win           Menu Window
 * @param event         KeyEvent (may be a mouse event)
 * @retval out consumed_only Set true if the event was a menu click that only
 *                           moved the highlight (caller should consume it)
 *
 * A single click that only moves the highlight returns #OP_NULL with
 * @a consumed_only set, so the caller can consume the event without performing
 * any further movement.  Wheel events are translated into page scrolls.  A
 * double-click or a click on the already-selected entry (the "open" action) is
 * left to the upstream dialog dispatcher and returns #OP_NULL with
 * @a consumed_only clear.  Returns #OP_NULL (and leaves @a consumed_only clear)
 * when the event is not a Menu mouse event or the click fell outside the menu.
 */
bool menu_mouse_translate(struct MuttWindow *win, struct KeyEvent *event)
{
  if (!win || !event)
    return false;

  /* Only mouse events are handled here. */
  if ((event->op < OP_MOUSE_CLICK) || (event->op > OP_MOUSE_WHEEL_DOWN))
    return false;

  struct Menu *menu = win->wdata;
  if (!menu || !menu->win)
    return false;

  /* Wheel scrolling is handled directly by the Menu. */
  if (event->op == OP_MOUSE_WHEEL_UP)
  {
    event->op = OP_PREV_PAGE;
    return false;
  }
  if (event->op == OP_MOUSE_WHEEL_DOWN)
  {
    event->op = OP_NEXT_PAGE;
    return false;
  }

  /* Click / double-click: hit-test against the visible entries. */
  const int new_index = menu_get_index_by_coords(menu, event->mouse_row, event->mouse_col);
  if (new_index < 0)
    return false; /* click outside this Menu */

  const int old_index = menu_get_index(menu);
  menu_set_index(menu, new_index);

  if (new_index == old_index)
    /* Clicked the already-selected entry: the "open" action belongs to the
     * upstream dialog dispatcher, not the generic Menu. */
    return false;

  /* Single click on a different entry: the highlight has been moved, consume. */
  return true;
}

/**
 * menu_queue_redraw - Queue a request for a redraw
 * @param menu  Menu
 * @param redraw Item to redraw, e.g. #MENU_REDRAW_CURRENT
 */
void menu_queue_redraw(struct Menu *menu, MenuRedrawFlags redraw)
{
  if (!menu)
    return;

  menu->redraw |= redraw;
  menu->win->actions |= WA_RECALC;
}
