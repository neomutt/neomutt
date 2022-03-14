/**
 * @file
 * GUI present the user with a selectable list
 *
 * @authors
 * Copyright (C) 1996-2000,2002,2012 Michael R. Elkins <me@mutt.org>
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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "color/lib.h"
#include "index/lib.h"
#include "menu/lib.h"
#include "commands.h"
#include "context.h"
#include "keymap.h"
#include "mutt_globals.h"
#include "mutt_logging.h"
#include "mutt_mailbox.h"
#include "opcodes.h"
#include "protos.h"
#include "type.h"

char *SearchBuffers[MENU_MAX];

/**
 * default_color - Get the default colour for a line of the menu - Implements Menu::color() - @ingroup menu_color
 */
static struct AttrColor *default_color(struct Menu *menu, int line)
{
  return simple_color_get(MT_COLOR_NORMAL);
}

/**
 * generic_search - Search a menu for a item matching a regex - Implements Menu::search() - @ingroup menu_search
 */
static int generic_search(struct Menu *menu, regex_t *rx, int line)
{
  char buf[1024];

  menu->make_entry(menu, buf, sizeof(buf), line);
  return regexec(rx, buf, 0, NULL, 0);
}

/**
 * menu_cleanup - Free the saved Menu searches
 */
void menu_cleanup(void)
{
  for (int i = 0; i < MENU_MAX; i++)
    FREE(&SearchBuffers[i]);
}

/**
 * menu_init - Initialise all the Menus
 */
void menu_init(void)
{
  for (int i = 0; i < MENU_MAX; i++)
    SearchBuffers[i] = NULL;
}

/**
 * menu_get_current_type - Get the type of the current Window
 * @retval enum Menu Type, e.g. #MENU_PAGER
 */
enum MenuType menu_get_current_type(void)
{
  struct MuttWindow *win = alldialogs_get_current();
  while (win && win->focus)
    win = win->focus;

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

  return menu->type;
}

/**
 * menu_free - Free a Menu
 * @param ptr Menu to free
 */
void menu_free(struct Menu **ptr)
{
  struct Menu *menu = *ptr;

  notify_free(&menu->notify);

  if (menu->mdata_free && menu->mdata)
    menu->mdata_free(menu, &menu->mdata); // Custom function to free private data

  FREE(ptr);
}

/**
 * menu_new - Create a new Menu
 * @param type Menu type, e.g. #MENU_ALIAS
 * @param win  Parent Window
 * @param sub  Config items
 * @retval ptr New Menu
 */
struct Menu *menu_new(enum MenuType type, struct MuttWindow *win, struct ConfigSubset *sub)
{
  struct Menu *menu = mutt_mem_calloc(1, sizeof(struct Menu));

  menu->type = type;
  menu->redraw = MENU_REDRAW_FULL;
  menu->color = default_color;
  menu->search = generic_search;
  menu->notify = notify_new();
  menu->win = win;
  menu->page_len = win->state.rows;
  menu->sub = sub;

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
 * menu_set_index - Set the current selection in the Menu
 * @param menu  Menu
 * @param index Item to select
 * @retval num #MenuRedrawFlags, e.g. #MENU_REDRAW_INDEX
 */
MenuRedrawFlags menu_set_index(struct Menu *menu, int index)
{
  return menu_move_selection(menu, index);
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
