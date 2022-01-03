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
#include <stddef.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "gui/lib.h"
#include "lib.h"

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
      flags |= MENU_REDRAW_MOTION;
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
 * @retval num Top line
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
 * @retval num Index
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
 * @retval num Top line
 * @retval   0 Error
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
 * menu_move_selection - Move the selection, keeping within between [0, menu->max]
 * @param menu  Menu
 * @param index New selection
 * @retval num #MenuRedrawFlags, e.g. #MENU_REDRAW_CURRENT
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
 * @retval num #MenuRedrawFlags, e.g. #MENU_REDRAW_CURRENT
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
 * @retval num #MenuRedrawFlags, e.g. #MENU_REDRAW_CURRENT
 */
MenuRedrawFlags menu_top_page(struct Menu *menu)
{
  return menu_move_selection(menu, menu->top);
}

/**
 * menu_middle_page - Move the focus to the centre of the page
 * @param menu Current Menu
 * @retval num #MenuRedrawFlags, e.g. #MENU_REDRAW_CURRENT
 */
MenuRedrawFlags menu_middle_page(struct Menu *menu)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return MENU_REDRAW_NO_FLAGS;
  }

  int i = menu->top + menu->pagelen;
  if (i > (menu->max - 1))
    i = menu->max - 1;

  return menu_move_selection(menu, menu->top + (i - menu->top) / 2);
}

/**
 * menu_bottom_page - Move the focus to the bottom of the page
 * @param menu Current Menu
 * @retval num #MenuRedrawFlags, e.g. #MENU_REDRAW_CURRENT
 */
MenuRedrawFlags menu_bottom_page(struct Menu *menu)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return MENU_REDRAW_NO_FLAGS;
  }

  int index = menu->top + menu->pagelen - 1;
  if (index > (menu->max - 1))
    index = menu->max - 1;
  return menu_move_selection(menu, index);
}

/**
 * menu_prev_entry - Move the focus to the previous item in the menu
 * @param menu Current Menu
 * @retval num #MenuRedrawFlags, e.g. #MENU_REDRAW_CURRENT
 */
MenuRedrawFlags menu_prev_entry(struct Menu *menu)
{
  if (menu->current > 0)
    return menu_move_selection(menu, menu->current - 1);

  mutt_message(_("You are on the first entry"));
  return MENU_REDRAW_NO_FLAGS;
}

/**
 * menu_next_entry - Move the focus to the next item in the menu
 * @param menu Current Menu
 * @retval num #MenuRedrawFlags, e.g. #MENU_REDRAW_CURRENT
 */
MenuRedrawFlags menu_next_entry(struct Menu *menu)
{
  if (menu->current < (menu->max - 1))
    return menu_move_selection(menu, menu->current + 1);

  mutt_message(_("You are on the last entry"));
  return MENU_REDRAW_NO_FLAGS;
}

/**
 * menu_first_entry - Move the focus to the first entry in the menu
 * @param menu Current Menu
 * @retval num #MenuRedrawFlags, e.g. #MENU_REDRAW_CURRENT
 */
MenuRedrawFlags menu_first_entry(struct Menu *menu)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return MENU_REDRAW_NO_FLAGS;
  }

  return menu_move_selection(menu, 0);
}

/**
 * menu_last_entry - Move the focus to the last entry in the menu
 * @param menu Current Menu
 * @retval num #MenuRedrawFlags, e.g. #MENU_REDRAW_CURRENT
 */
MenuRedrawFlags menu_last_entry(struct Menu *menu)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return MENU_REDRAW_NO_FLAGS;
  }

  return menu_move_selection(menu, menu->max - 1);
}

// These functions move the view (and may cause the selection to move)
/**
 * menu_current_top - Move the current selection to the top of the window
 * @param menu Current Menu
 * @retval num #MenuRedrawFlags, e.g. #MENU_REDRAW_CURRENT
 */
MenuRedrawFlags menu_current_top(struct Menu *menu)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return MENU_REDRAW_NO_FLAGS;
  }

  short context = cs_subset_number(menu->sub, "menu_context");
  if (context > (menu->pagelen / 2))
    return MENU_REDRAW_NO_FLAGS;

  context = MIN(context, (menu->pagelen / 2));
  return menu_move_view_relative(menu, menu->current - menu->top - context);
}

/**
 * menu_current_middle - Move the current selection to the centre of the window
 * @param menu Current Menu
 * @retval num #MenuRedrawFlags, e.g. #MENU_REDRAW_CURRENT
 */
MenuRedrawFlags menu_current_middle(struct Menu *menu)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return MENU_REDRAW_NO_FLAGS;
  }

  short context = cs_subset_number(menu->sub, "menu_context");
  if (context > (menu->pagelen / 2))
    return MENU_REDRAW_NO_FLAGS;

  return menu_move_view_relative(menu, menu->current - (menu->top + (menu->pagelen / 2)));
}

/**
 * menu_current_bottom - Move the current selection to the bottom of the window
 * @param menu Current Menu
 * @retval num #MenuRedrawFlags, e.g. #MENU_REDRAW_CURRENT
 */
MenuRedrawFlags menu_current_bottom(struct Menu *menu)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return MENU_REDRAW_NO_FLAGS;
  }

  short context = cs_subset_number(menu->sub, "menu_context");
  if (context > (menu->pagelen / 2))
    return MENU_REDRAW_NO_FLAGS;

  context = MIN(context, (menu->pagelen / 2));
  return menu_move_view_relative(
      menu, 0 - (menu->top + menu->pagelen - 1 - menu->current - context));
}

/**
 * menu_half_up - Move the focus up half a page in the menu
 * @param menu Current Menu
 * @retval num #MenuRedrawFlags, e.g. #MENU_REDRAW_CURRENT
 */
MenuRedrawFlags menu_half_up(struct Menu *menu)
{
  return menu_move_view_relative(menu, 0 - (menu->pagelen / 2));
}

/**
 * menu_half_down - Move the focus down half a page in the menu
 * @param menu Current Menu
 * @retval num #MenuRedrawFlags, e.g. #MENU_REDRAW_CURRENT
 */
MenuRedrawFlags menu_half_down(struct Menu *menu)
{
  return menu_move_view_relative(menu, (menu->pagelen / 2));
}

/**
 * menu_prev_line - Move the view up one line, keeping the selection the same
 * @param menu Current Menu
 * @retval num #MenuRedrawFlags, e.g. #MENU_REDRAW_CURRENT
 */
MenuRedrawFlags menu_prev_line(struct Menu *menu)
{
  MenuRedrawFlags flags = menu_move_view_relative(menu, -1);
  if (flags == MENU_REDRAW_NO_FLAGS)
    mutt_message(_("You can't scroll up farther"));
  return flags;
}

/**
 * menu_next_line - Move the view down one line, keeping the selection the same
 * @param menu Current Menu
 * @retval num #MenuRedrawFlags, e.g. #MENU_REDRAW_CURRENT
 */
MenuRedrawFlags menu_next_line(struct Menu *menu)
{
  MenuRedrawFlags flags = menu_move_view_relative(menu, 1);
  if (flags == MENU_REDRAW_NO_FLAGS)
    mutt_message(_("You can't scroll down farther"));
  return flags;
}

/**
 * menu_prev_page - Move the focus to the previous page in the menu
 * @param menu Current Menu
 * @retval num #MenuRedrawFlags, e.g. #MENU_REDRAW_CURRENT
 */
MenuRedrawFlags menu_prev_page(struct Menu *menu)
{
  return menu_move_view_relative(menu, 0 - menu->pagelen);
}

/**
 * menu_next_page - Move the focus to the next page in the menu
 * @param menu Current Menu
 * @retval num #MenuRedrawFlags, e.g. #MENU_REDRAW_CURRENT
 */
MenuRedrawFlags menu_next_page(struct Menu *menu)
{
  return menu_move_view_relative(menu, menu->pagelen);
}
