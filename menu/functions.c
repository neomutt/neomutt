/**
 * @file
 * Menu functions
 *
 * @authors
 * Copyright (C) 2022 Richard Russon <rich@flatcap.org>
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
 * @page menu_functions Menu functions
 *
 * Menu functions
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "functions.h"
#include "lib.h"
#include "enter/lib.h"
#include "opcodes.h"
#include "protos.h"
#include "type.h"

extern char *SearchBuffers[];

#define MUTT_SEARCH_UP 1
#define MUTT_SEARCH_DOWN 2

/**
 * search - Search a menu
 * @param menu Menu to search
 * @param op   Search operation, e.g. OP_SEARCH_NEXT
 * @retval >=0 Index of matching item
 * @retval -1  Search failed, or was cancelled
 */
static int search(struct Menu *menu, int op)
{
  int rc = -1;
  int wrap = 0;
  int search_dir;
  regex_t re = { 0 };
  struct Buffer *buf = buf_pool_get();

  char *search_buf = ((menu->type < MENU_MAX)) ? SearchBuffers[menu->type] : NULL;

  if (!(search_buf && *search_buf) || ((op != OP_SEARCH_NEXT) && (op != OP_SEARCH_OPPOSITE)))
  {
    buf_strcpy(buf, search_buf && (search_buf[0] != '\0') ? search_buf : "");
    if ((mw_get_field(((op == OP_SEARCH) || (op == OP_SEARCH_NEXT)) ? _("Search for: ") : _("Reverse search for: "),
                      buf, MUTT_COMP_CLEAR, false, NULL, NULL, NULL) != 0) ||
        buf_is_empty(buf))
    {
      goto done;
    }
    if (menu->type < MENU_MAX)
    {
      mutt_str_replace(&SearchBuffers[menu->type], buf_string(buf));
      search_buf = SearchBuffers[menu->type];
    }
    menu->search_dir = ((op == OP_SEARCH) || (op == OP_SEARCH_NEXT)) ?
                           MUTT_SEARCH_DOWN :
                           MUTT_SEARCH_UP;
  }

  search_dir = (menu->search_dir == MUTT_SEARCH_UP) ? -1 : 1;
  if (op == OP_SEARCH_OPPOSITE)
    search_dir = -search_dir;

  if (search_buf)
  {
    uint16_t flags = mutt_mb_is_lower(search_buf) ? REG_ICASE : 0;
    rc = REG_COMP(&re, search_buf, REG_NOSUB | flags);
  }

  if (rc != 0)
  {
    regerror(rc, &re, buf->data, buf->dsize);
    mutt_error("%s", buf_string(buf));
    rc = -1;
    goto done;
  }

  rc = menu->current + search_dir;
search_next:
  if (wrap)
    mutt_message(_("Search wrapped to top"));
  while ((rc >= 0) && (rc < menu->max))
  {
    if (menu->search(menu, &re, rc) == 0)
    {
      regfree(&re);
      goto done;
    }

    rc += search_dir;
  }

  const bool c_wrap_search = cs_subset_bool(menu->sub, "wrap_search");
  if (c_wrap_search && (wrap++ == 0))
  {
    rc = (search_dir == 1) ? 0 : menu->max - 1;
    goto search_next;
  }
  regfree(&re);
  mutt_error(_("Not found"));
  rc = -1;

done:
  buf_pool_release(&buf);
  return rc;
}

// -----------------------------------------------------------------------------

/**
 * menu_movement - Handle all the common Menu movements - Implements ::menu_function_t - @ingroup menu_function_api
 */
static int menu_movement(struct Menu *menu, int op)
{
  switch (op)
  {
    case OP_BOTTOM_PAGE:
      menu_bottom_page(menu);
      return FR_SUCCESS;

    case OP_CURRENT_BOTTOM:
      menu_current_bottom(menu);
      return FR_SUCCESS;

    case OP_CURRENT_MIDDLE:
      menu_current_middle(menu);
      return FR_SUCCESS;

    case OP_CURRENT_TOP:
      menu_current_top(menu);
      return FR_SUCCESS;

    case OP_FIRST_ENTRY:
      menu_first_entry(menu);
      return FR_SUCCESS;

    case OP_HALF_DOWN:
      menu_half_down(menu);
      return FR_SUCCESS;

    case OP_HALF_UP:
      menu_half_up(menu);
      return FR_SUCCESS;

    case OP_LAST_ENTRY:
      menu_last_entry(menu);
      return FR_SUCCESS;

    case OP_MIDDLE_PAGE:
      menu_middle_page(menu);
      return FR_SUCCESS;

    case OP_NEXT_ENTRY:
      menu_next_entry(menu);
      return FR_SUCCESS;

    case OP_NEXT_LINE:
      menu_next_line(menu);
      return FR_SUCCESS;

    case OP_NEXT_PAGE:
      menu_next_page(menu);
      return FR_SUCCESS;

    case OP_PREV_ENTRY:
      menu_prev_entry(menu);
      return FR_SUCCESS;

    case OP_PREV_LINE:
      menu_prev_line(menu);
      return FR_SUCCESS;

    case OP_PREV_PAGE:
      menu_prev_page(menu);
      return FR_SUCCESS;

    case OP_TOP_PAGE:
      menu_top_page(menu);
      return FR_SUCCESS;

    default:
      return FR_UNKNOWN;
  }
}

/**
 * menu_search - Handle Menu searching - Implements ::menu_function_t - @ingroup menu_function_api
 */
static int menu_search(struct Menu *menu, int op)
{
  if (menu->search)
  {
    int index = search(menu, op);
    if (index != -1)
      menu_set_index(menu, index);
  }
  return FR_SUCCESS;
}

/**
 * op_help - Show the help screen - Implements ::menu_function_t - @ingroup menu_function_api
 */
static int op_help(struct Menu *menu, int op)
{
  mutt_help(menu->type);
  menu->redraw = MENU_REDRAW_FULL;
  return FR_SUCCESS;
}

/**
 * op_jump - Jump to an index number - Implements ::menu_function_t - @ingroup menu_function_api
 */
static int op_jump(struct Menu *menu, int op)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return FR_SUCCESS;
  }

  const int digit = op - OP_JUMP;
  if (digit > 0 && digit < 10)
  {
    mutt_unget_ch('0' + digit);
  }

  struct Buffer *buf = buf_pool_get();
  if ((mw_get_field(_("Jump to: "), buf, MUTT_COMP_NO_FLAGS, false, NULL, NULL, NULL) == 0) &&
      !buf_is_empty(buf))
  {
    int n = 0;
    if (mutt_str_atoi_full(buf_string(buf), &n) && (n > 0) && (n < (menu->max + 1)))
    {
      menu_set_index(menu, n - 1); // msg numbers are 0-based
    }
    else
    {
      mutt_error(_("Invalid index number"));
    }
  }

  buf_pool_release(&buf);
  return FR_SUCCESS;
}

// -----------------------------------------------------------------------------

/**
 * MenuFunctions - All the NeoMutt functions that the Menu supports
 */
static const struct MenuFunction MenuFunctions[] = {
  // clang-format off
  { OP_BOTTOM_PAGE,            menu_movement },
  { OP_CURRENT_BOTTOM,         menu_movement },
  { OP_CURRENT_MIDDLE,         menu_movement },
  { OP_CURRENT_TOP,            menu_movement },
  { OP_FIRST_ENTRY,            menu_movement },
  { OP_HALF_DOWN,              menu_movement },
  { OP_HALF_UP,                menu_movement },
  { OP_HELP,                   op_help },
  { OP_JUMP,                   op_jump },
  { OP_JUMP_1,                 op_jump },
  { OP_JUMP_2,                 op_jump },
  { OP_JUMP_3,                 op_jump },
  { OP_JUMP_4,                 op_jump },
  { OP_JUMP_5,                 op_jump },
  { OP_JUMP_6,                 op_jump },
  { OP_JUMP_7,                 op_jump },
  { OP_JUMP_8,                 op_jump },
  { OP_JUMP_9,                 op_jump },
  { OP_LAST_ENTRY,             menu_movement },
  { OP_MIDDLE_PAGE,            menu_movement },
  { OP_NEXT_ENTRY,             menu_movement },
  { OP_NEXT_LINE,              menu_movement },
  { OP_NEXT_PAGE,              menu_movement },
  { OP_PREV_ENTRY,             menu_movement },
  { OP_PREV_LINE,              menu_movement },
  { OP_PREV_PAGE,              menu_movement },
  { OP_SEARCH,                 menu_search },
  { OP_SEARCH_NEXT,            menu_search },
  { OP_SEARCH_OPPOSITE,        menu_search },
  { OP_SEARCH_REVERSE,         menu_search },
  { OP_TOP_PAGE,               menu_movement },
  { 0, NULL },
  // clang-format on
};

/**
 * menu_function_dispatcher - Perform a Menu function - Implements ::function_dispatcher_t - @ingroup dispatcher_api
 */
int menu_function_dispatcher(struct MuttWindow *win, int op)
{
  if (!win || !win->wdata)
    return FR_UNKNOWN;

  struct Menu *menu = win->wdata;

  int rc = FR_UNKNOWN;
  for (size_t i = 0; MenuFunctions[i].op != OP_NULL; i++)
  {
    const struct MenuFunction *fn = &MenuFunctions[i];
    if (fn->op == op)
    {
      rc = fn->function(menu, op);
      break;
    }
  }

  if (rc == FR_UNKNOWN) // Not our function
    return rc;

  const char *result = dispacher_get_retval_name(rc);
  mutt_debug(LL_DEBUG1, "Handled %s (%d) -> %s\n", opcodes_get_name(op), op, NONULL(result));

  return rc;
}
