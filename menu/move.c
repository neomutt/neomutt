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
#include "gui/lib.h"
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

/**
 * menu_set_and_notify - Set the Menu selection/view and notify others
 * @param menu  Menu
 * @param top   Index of item at the top of the view
 * @param index Selected item
 * @retval num #MenuRedrawFlags, e.g. #MENU_REDRAW_CURRENT
 */
MenuRedrawFlags menu_set_and_notify(struct Menu *menu, int top, int index)
{
  MenuRedrawFlags flags = MENU_REDRAW_NO_FLAGS;

  if (top != menu->top)
  {
    menu->top = top;
    flags |= MENU_REDRAW_FULL;
  }

  if (index != menu->current)
  {
    menu->oldcurrent = menu->current;
    menu->current = index;

    if (menu->redraw == MENU_REDRAW_NO_FLAGS)
    {
      // If this is the only change
      flags |= MENU_REDRAW_CURRENT;
    }
    else
    {
      // otherwise, redraw completely
      flags |= MENU_REDRAW_FULL;
    }
  }

  menu->redraw |= flags;
  menu->win->actions |= WA_REPAINT;

  mutt_debug(LL_NOTIFY, "NT_MENU\n");
  notify_send(menu->notify, NT_MENU, flags, NULL);
  return flags;
}

/**
 * menu_drag_view - Move the view around the selection
 * @param menu  Menu
 * @param top   Index of item at the top of the view
 * @param index Current selection
 */
static int menu_drag_view(struct Menu *menu, int top, int index)
{
  if (menu->max <= menu->pagelen) // fewer entries than lines
    return 0;

  const int page = menu->pagelen;

  short context = cs_subset_number(menu->sub, "menu_context");
  context = MIN(context, (page / 2));

  const bool c_menu_scroll = cs_subset_bool(menu->sub, "menu_scroll");
  if (c_menu_scroll)
  {
    int bottom = top + page;
    // Scroll the view to make the selection visible
    if (index < top + context) // scroll=YES, moving UP
      top = index - context;
    else if (index >= (bottom - context)) // scroll=YES, moving DOWN
      top = index - page + context + 1;
  }
  else
  {
    if ((index < top) || (index >= (top + page)))
      top = (index / page) * page; // Round down to a page size
    int bottom = top + page;

    // Page up/down to make the selection visible
    if (index < (top + context)) // scroll=NO, moving UP
      top = index - page + context + 1;
    else if (index >= (bottom - context)) // scroll=NO, moving DOWN
      top = index - context;
  }

  if (top < 0)
    top = 0;

  // Tie the last entry to the bottom of the screen
  const bool c_menu_move_off = cs_subset_bool(menu->sub, "menu_move_off");
  if (!c_menu_move_off && (top >= (menu->max - page)))
  {
    top = menu->max - page;
  }

  return top;
}

/**
 * calc_fit_selection_to_view - Move the selection into the view
 * @param menu  Menu
 * @param top   First entry visible in the view
 * @param index Current selection
 */
static int calc_fit_selection_to_view(struct Menu *menu, int top, int index)
{
  short context = cs_subset_number(menu->sub, "menu_context");
  context = MIN(context, (menu->pagelen / 2));

  int min = top;
  if (top != 0)
    min += context;

  int max = top + menu->pagelen - 1;
  if (max < (menu->max - 1))
    max -= context;
  else
    max = menu->max - 1;

  if (index < min)
    index = min;
  else if (index > max)
    index = max;

  return index;
}

/**
 * calc_move_view - Move the view
 * @param menu     Menu
 * @param relative Relative number of lines to move
 */
static int calc_move_view(struct Menu *menu, int relative)
{
  if (menu->max <= menu->pagelen) // fewer entries than lines
    return 0;

  short context = cs_subset_number(menu->sub, "menu_context");
  context = MIN(context, (menu->pagelen / 2));

  int index = menu->current;
  if (index < context)
    return 0;

  int top = menu->top + relative;
  if (top < 0)
    return 0;

  if ((menu->top + menu->pagelen) < menu->max)
    return top;

  int max = menu->max - 1;
  const bool c_menu_move_off = cs_subset_bool(menu->sub, "menu_move_off");
  if (c_menu_move_off)
  {
    max -= context;
  }
  else
  {
    max -= menu->pagelen - 1;
  }

  if (top > max)
    top = max;

  return top;
}

/**
 * menu_move_selection - Move the selection
 * @param menu  Menu
 * @param index New selection
 */
MenuRedrawFlags menu_move_selection(struct Menu *menu, int index)
{
  if (index < 0)
    index = 0;
  else if (index >= menu->max)
    index = menu->max - 1;

  int top = menu_drag_view(menu, menu->top, index);

  return menu_set_and_notify(menu, top, index);
}

/**
 * menu_move_view_relative - Move the view relatively
 * @param menu     Menu
 * @param relative Relative number of lines to move
 */
MenuRedrawFlags menu_move_view_relative(struct Menu *menu, int relative)
{
  const bool c_menu_move_off = cs_subset_bool(menu->sub, "menu_move_off");

  short context = cs_subset_number(menu->sub, "menu_context");
  context = MIN(context, (menu->pagelen / 2));

  // Move and range-check the view
  int top = menu->top + relative;
  if (top < 0)
  {
    top = 0;
  }
  else if (c_menu_move_off && (top >= (menu->max - context)))
  {
    top = menu->max - context - 1;
  }
  else if (!c_menu_move_off && ((top + menu->pagelen) >= menu->max))
  {
    top = menu->max - menu->pagelen;
  }

  // Move the selection on-screen
  int index = menu->current;
  if (index < top)
    index = top;
  else if (index >= (top + menu->pagelen))
    index = top + menu->pagelen - 1;

  // Check for top/bottom limits
  if (index < context)
  {
    top = 0;
    index = menu->current;
  }
  else if (!c_menu_move_off && (index > (menu->max - context)))
  {
    top = menu->max - menu->pagelen;
    index = menu->current;
  }

  if (top == menu->top)
  {
    // Can't move the view; move the selection
    index = calc_fit_selection_to_view(menu, top, index + relative);
  }
  else if (index > (top + menu->pagelen - context - 1))
  {
    index = calc_fit_selection_to_view(menu, top, index + relative);
  }
  else
  {
    // Drag the selection into the view
    index = calc_fit_selection_to_view(menu, top, index);
  }

  return menu_set_and_notify(menu, top, index);
}

/**
 * menu_adjust - Reapply the config to the Menu
 * @param menu Menu
 */
void menu_adjust(struct Menu *menu)
{
  int top = calc_move_view(menu, 0);
  top = menu_drag_view(menu, top, menu->current);

  menu_set_and_notify(menu, top, menu->current);
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
