/**
 * @file
 * Change the Menu's position/selection
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
 * @page menu_move Change the Menu's position/selection
 *
 * There are two types of movement function:
 *
 * - Those that change the selection:
 *   - menu_top_page()
 *   - menu_middle_page()
 *   - menu_bottom_page()
 *   - menu_prev_entry()
 *   - menu_next_entry()
 *   - menu_first_entry()
 *   - menu_last_entry()
 *
 * - Those that change the view:
 *   - menu_current_top()
 *   - menu_current_middle()
 *   - menu_current_bottom()
 *   - menu_half_up()
 *   - menu_half_down()
 *   - menu_prev_line()
 *   - menu_next_line()
 *   - menu_prev_page()
 *   - menu_next_page()
 *
 * Changing the selection may cause the view to move and vice versa.
 */

#include "config.h"
#include <stdbool.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "lib.h"

/**
 * menu_length_jump - Calculate the destination of a jump
 * @param menu    Current Menu
 * @param jumplen Number of lines to jump
 *
 * * pageup:   jumplen == -pagelen
 * * pagedown: jumplen == pagelen
 * * halfup:   jumplen == -pagelen/2
 * * halfdown: jumplen == pagelen/2
 */
static void menu_length_jump(struct Menu *menu, int jumplen)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return;
  }

  const short c_menu_context = cs_subset_number(menu->sub, "menu_context");
  const bool c_menu_move_off = cs_subset_bool(menu->sub, "menu_move_off");

  const int neg = (jumplen >= 0) ? 0 : -1;
  const int c = MIN(c_menu_context, (menu->pagelen / 2));
  const int direction = ((neg * 2) + 1);

  /* possible to scroll? */
  int tmp;
  int index = menu->current;
  if ((direction * menu->top) <
      (tmp = (neg ? 0 : (menu->max /* -1 */) - (menu->pagelen /* -1 */))))
  {
    menu->top += jumplen;

    /* jumped too long? */
    if ((neg || !c_menu_move_off) && ((direction * menu->top) > tmp))
      menu->top = tmp;

    /* need to move the cursor? */
    if ((direction *
         (tmp = (menu->current - (menu->top + (neg ? (menu->pagelen - 1) - c : c))))) < 0)
    {
      index -= tmp;
    }

    menu->redraw = MENU_REDRAW_INDEX;
  }
  else if ((menu->current != (neg ? 0 : menu->max - 1)) && ARRAY_EMPTY(&menu->dialog))
  {
    index += jumplen;
  }
  else
  {
    mutt_message(neg ? _("You are on the first page") : _("You are on the last page"));
  }

  // Range check
  index = MIN(index, menu->max - 1);
  index = MAX(index, 0);
  menu_set_index(menu, index);
}

// These functions move the selection (and may cause the view to move)
/**
 * menu_top_page - Move the focus to the top of the page
 * @param menu Current Menu
 */
void menu_top_page(struct Menu *menu)
{
  if (menu->current == menu->top)
    return;

  menu_set_index(menu, menu->top);
}

/**
 * menu_middle_page - Move the focus to the centre of the page
 * @param menu Current Menu
 */
void menu_middle_page(struct Menu *menu)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return;
  }

  int i = menu->top + menu->pagelen;
  if (i > (menu->max - 1))
    i = menu->max - 1;

  menu_set_index(menu, menu->top + (i - menu->top) / 2);
}

/**
 * menu_bottom_page - Move the focus to the bottom of the page
 * @param menu Current Menu
 */
void menu_bottom_page(struct Menu *menu)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return;
  }

  int index = menu->top + menu->pagelen - 1;
  if (index > (menu->max - 1))
    index = menu->max - 1;
  menu_set_index(menu, index);
}

/**
 * menu_prev_entry - Move the focus to the previous item in the menu
 * @param menu Current Menu
 */
void menu_prev_entry(struct Menu *menu)
{
  if (menu->current)
  {
    menu_set_index(menu, menu->current - 1);
  }
  else
    mutt_message(_("You are on the first entry"));
}

/**
 * menu_next_entry - Move the focus to the next item in the menu
 * @param menu Current Menu
 */
void menu_next_entry(struct Menu *menu)
{
  if (menu->current < (menu->max - 1))
  {
    menu_set_index(menu, menu->current + 1);
  }
  else
    mutt_message(_("You are on the last entry"));
}

/**
 * menu_first_entry - Move the focus to the first entry in the menu
 * @param menu Current Menu
 */
void menu_first_entry(struct Menu *menu)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return;
  }

  menu_set_index(menu, 0);
}

/**
 * menu_last_entry - Move the focus to the last entry in the menu
 * @param menu Current Menu
 */
void menu_last_entry(struct Menu *menu)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return;
  }

  menu_set_index(menu, menu->max - 1);
}

// These functions move the view (and may cause the selection to move)
/**
 * menu_current_top - Move the current selection to the top of the window
 * @param menu Current Menu
 */
void menu_current_top(struct Menu *menu)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return;
  }

  menu->top = menu->current;
  menu->redraw = MENU_REDRAW_INDEX;
}

/**
 * menu_current_middle - Move the current selection to the centre of the window
 * @param menu Current Menu
 */
void menu_current_middle(struct Menu *menu)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return;
  }

  menu->top = menu->current - (menu->pagelen / 2);
  if (menu->top < 0)
    menu->top = 0;
  menu->redraw = MENU_REDRAW_INDEX;
}

/**
 * menu_current_bottom - Move the current selection to the bottom of the window
 * @param menu Current Menu
 */
void menu_current_bottom(struct Menu *menu)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return;
  }

  menu->top = menu->current - menu->pagelen + 1;
  if (menu->top < 0)
    menu->top = 0;
  menu->redraw = MENU_REDRAW_INDEX;
}

/**
 * menu_half_up - Move the focus up half a page in the menu
 * @param menu Current Menu
 */
void menu_half_up(struct Menu *menu)
{
  menu_length_jump(menu, 0 - (menu->pagelen / 2));
}

/**
 * menu_half_down - Move the focus down half a page in the menu
 * @param menu Current Menu
 */
void menu_half_down(struct Menu *menu)
{
  menu_length_jump(menu, (menu->pagelen / 2));
}

/**
 * menu_prev_line - Move the view up one line, keeping the selection the same
 * @param menu Current Menu
 */
void menu_prev_line(struct Menu *menu)
{
  if (menu->top < 1)
  {
    mutt_message(_("You can't scroll up farther"));
    return;
  }

  const short c_menu_context = cs_subset_number(menu->sub, "menu_context");
  int c = MIN(c_menu_context, (menu->pagelen / 2));

  menu->top--;
  if ((menu->current >= (menu->top + menu->pagelen - c)) && (menu->current > 1))
    menu_set_index(menu, menu->current - 1);
  menu->redraw = MENU_REDRAW_INDEX;
}

/**
 * menu_next_line - Move the view down one line, keeping the selection the same
 * @param menu Current Menu
 */
void menu_next_line(struct Menu *menu)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return;
  }

  const short c_menu_context = cs_subset_number(menu->sub, "menu_context");
  const bool c_menu_move_off = cs_subset_bool(menu->sub, "menu_move_off");
  int c = MIN(c_menu_context, (menu->pagelen / 2));

  if (((menu->top + 1) < (menu->max - c)) &&
      (c_menu_move_off ||
       ((menu->max > menu->pagelen) && (menu->top < (menu->max - menu->pagelen)))))
  {
    menu->top++;
    if ((menu->current < (menu->top + c)) && (menu->current < (menu->max - 1)))
      menu_set_index(menu, menu->current + 1);
    menu->redraw = MENU_REDRAW_INDEX;
  }
  else
    mutt_message(_("You can't scroll down farther"));
}

/**
 * menu_prev_page - Move the focus to the previous page in the menu
 * @param menu Current Menu
 */
void menu_prev_page(struct Menu *menu)
{
  menu_length_jump(menu, 0 - MAX(menu->pagelen /* - MenuOverlap */, 0));
}

/**
 * menu_next_page - Move the focus to the next page in the menu
 * @param menu Current Menu
 */
void menu_next_page(struct Menu *menu)
{
  menu_length_jump(menu, MAX(menu->pagelen /* - MenuOverlap */, 0));
}
