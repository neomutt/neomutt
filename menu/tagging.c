/**
 * @file
 * Tagging support
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
 * @page menu_tagging Tagging support
 *
 * Tagging support
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "color/lib.h"
#include "key/lib.h"

/**
 * menu_set_prefix - Set tag_prefix on $auto_tag
 * @param menu  Menu
 */
static void menu_set_prefix(struct Menu *menu)
{
  const bool c_auto_tag = cs_subset_bool(menu->sub, "auto_tag");
  if ((menu->num_tagged != 0) && c_auto_tag)
    menu->tag_prefix = true;

  mutt_debug(LL_DEBUG1, "tag_prefix = %d\n", menu->tag_prefix);

  // Don't overwrite error messages
  const char *msg_text = msgwin_get_text(NULL);
  if (msg_text && !mutt_str_equal(msg_text, "tag-"))
    return;

  if (menu->tag_prefix)
  {
    msgwin_set_text(NULL, "tag-", MT_COLOR_NORMAL);
  }
  else
  {
    msgwin_clear_text(NULL);
  }
}

/**
 * op_end_cond - End of conditional execution (noop)
 * @param menu Menu
 * @param op   Operation to perform, e.g. OP_END_COND
 * @retval enum #FunctionRetval
 */
static int op_end_cond(struct Menu *menu, int op)
{
  menu->tag_prefix = false;
  menu_set_prefix(menu);
  return FR_SUCCESS;
}

/**
 * op_tag - Tag the current entry
 * @param menu Menu
 * @param op   Operation to perform, e.g. OP_TAG
 * @retval enum #FunctionRetval
 */
static int op_tag(struct Menu *menu, int op)
{
  const bool c_auto_tag = cs_subset_bool(menu->sub, "auto_tag");
  int rc = FR_SUCCESS;

  if ((menu->num_tagged != 0) && c_auto_tag)
    menu->tag_prefix = true;

  if (!menu->tag)
  {
    mutt_error(_("Tagging is not supported"));
    return FR_ERROR;
  }

  if (menu->tag_prefix && !c_auto_tag)
  {
    for (int i = 0; i < menu->max; i++)
      menu->num_tagged += menu->tag(menu, i, 0);

    menu->redraw |= MENU_REDRAW_INDEX;
  }
  else if (menu->max != 0)
  {
    int num = menu->tag(menu, menu->current, -1);
    menu->num_tagged += num;

    const bool c_resolve = cs_subset_bool(menu->sub, "resolve");
    if ((num != 0) && c_resolve && (menu->current < (menu->max - 1)))
    {
      menu_set_index(menu, menu->current + 1);
    }
    else
    {
      menu->redraw |= MENU_REDRAW_CURRENT;
    }
  }
  else
  {
    mutt_error(_("No entries"));
    rc = FR_ERROR;
  }

  menu->tag_prefix = ((menu->num_tagged != 0) && c_auto_tag);

  /* give visual indication that the next command is a tag- command */
  if (menu->tag_prefix)
  {
    msgwin_set_text(NULL, "tag-", MT_COLOR_NORMAL);
  }

  menu->win->actions |= WA_REPAINT;
  return rc;
}

/**
 * op_tag_prefix - Apply next function to tagged messages
 * @param menu Menu
 * @param op   Operation to perform, e.g. OP_TAG_PREFIX
 * @retval enum #FunctionRetval
 */
static int op_tag_prefix(struct Menu *menu, int op)
{
  if (menu->tag_prefix)
  {
    menu->tag_prefix = false;
    menu_set_prefix(menu);
  }
  else if (menu->num_tagged == 0)
  {
    mutt_warning(_("No tagged entries"));
  }
  else
  {
    menu->tag_prefix = true;
    menu_set_prefix(menu);
  }

  return FR_SUCCESS;
}

/**
 * op_tag_prefix_cond - Apply next function ONLY to tagged messages
 * @param menu Menu
 * @param op   Operation to perform, e.g. OP_TAG_PREFIX_COND
 * @retval enum #FunctionRetval
 */
static int op_tag_prefix_cond(struct Menu *menu, int op)
{
  if (menu->tag_prefix)
  {
    menu->tag_prefix = false;
  }
  else if (menu->num_tagged == 0)
  {
    mutt_flush_macro_to_endcond();
    mutt_debug(LL_DEBUG1, "nothing to do\n");
  }
  else
  {
    menu->tag_prefix = true;
  }

  menu_set_prefix(menu);
  return FR_SUCCESS;
}

/**
 * menu_abort - User aborted an operation
 * @param menu Menu
 * @retval enum #FunctionRetval
 */
static int menu_abort(struct Menu *menu)
{
  menu->tag_prefix = false;
  menu_set_prefix(menu);
  return FR_SUCCESS;
}

/**
 * menu_timeout - Timeout waiting for a keypress
 * @param menu Menu
 * @retval enum #FunctionRetval
 */
static int menu_timeout(struct Menu *menu)
{
  menu_set_prefix(menu);
  return FR_SUCCESS;
}

/**
 * menu_other - Some non-tagging operation occurred
 * @param menu Menu
 * @retval enum #FunctionRetval
 */
static int menu_other(struct Menu *menu)
{
  menu->tag_prefix = false;
  menu_set_prefix(menu);
  return FR_SUCCESS;
}

/**
 * menu_tagging_dispatcher - Perform tagging operations on the Menu - Implements ::function_dispatcher_t - @ingroup dispatcher_api
 */
int menu_tagging_dispatcher(struct MuttWindow *win, int op)
{
  struct Menu *menu = win->wdata;

  switch (op)
  {
    case OP_END_COND:
      return op_end_cond(menu, op);
    case OP_TAG:
      return op_tag(menu, op);
    case OP_TAG_PREFIX:
      return op_tag_prefix(menu, op);
    case OP_TAG_PREFIX_COND:
      return op_tag_prefix_cond(menu, op);
    case OP_ABORT:
      return menu_abort(menu);
    case OP_TIMEOUT:
      return menu_timeout(menu);
    default:
      return menu_other(menu);
  }
}
