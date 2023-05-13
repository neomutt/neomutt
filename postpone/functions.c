/**
 * @file
 * Postponed Emails Functions
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
 * @page postpone_functions Postponed Emails Functions
 *
 * Postponed Emails Functions
 */

#include "config.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "functions.h"
#include "menu/lib.h"
#include "pattern/lib.h"
#include "mview.h"
#include "opcodes.h"
#include "protos.h"

struct Email;

/**
 * op_delete - Delete the current entry - Implements ::postpone_function_t - @ingroup postpone_function_api
 */
static int op_delete(struct PostponeData *pd, int op)
{
  struct Menu *menu = pd->menu;
  struct MailboxView *mv = pd->mailbox_view;
  struct Mailbox *m = mv->mailbox;

  const int index = menu_get_index(menu);
  /* should deleted draft messages be saved in the trash folder? */
  mutt_set_flag(m, m->emails[index], MUTT_DELETE, (op == OP_DELETE), true);
  PostCount = m->msg_count - m->msg_deleted;
  const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
  if (c_resolve && (index < (menu->max - 1)))
  {
    menu_set_index(menu, index + 1);
    if (index >= (menu->top + menu->page_len))
    {
      menu->top = index;
      menu_queue_redraw(menu, MENU_REDRAW_INDEX);
    }
  }
  else
  {
    menu_queue_redraw(menu, MENU_REDRAW_CURRENT);
  }

  return FR_SUCCESS;
}

/**
 * op_exit - Exit this menu - Implements ::postpone_function_t - @ingroup postpone_function_api
 */
static int op_exit(struct PostponeData *pd, int op)
{
  pd->done = true;
  return FR_SUCCESS;
}

/**
 * op_generic_select_entry - Select the current entry - Implements ::postpone_function_t - @ingroup postpone_function_api
 */
static int op_generic_select_entry(struct PostponeData *pd, int op)
{
  int index = menu_get_index(pd->menu);
  struct MailboxView *mv = pd->mailbox_view;
  struct Mailbox *m = mv->mailbox;
  pd->email = m->emails[index];
  pd->done = true;
  return FR_SUCCESS;
}

/**
 * op_search - Search for a regular expression - Implements ::postpone_function_t - @ingroup postpone_function_api
 */
static int op_search(struct PostponeData *pd, int op)
{
  int index = menu_get_index(pd->menu);
  struct MailboxView *mv = pd->mailbox_view;
  index = mutt_search_command(mv->mailbox, pd->menu, index, op);
  if (index != -1)
    menu_set_index(pd->menu, index);

  return FR_SUCCESS;
}

// -----------------------------------------------------------------------------

/**
 * PostponeFunctions - All the NeoMutt functions that the Postpone supports
 */
static const struct PostponeFunction PostponeFunctions[] = {
  // clang-format off
  { OP_DELETE,                 op_delete },
  { OP_EXIT,                   op_exit },
  { OP_GENERIC_SELECT_ENTRY,   op_generic_select_entry },
  { OP_SEARCH,                 op_search },
  { OP_SEARCH_NEXT,            op_search },
  { OP_SEARCH_OPPOSITE,        op_search },
  { OP_SEARCH_REVERSE,         op_search },
  { OP_UNDELETE,               op_delete },
  { 0, NULL },
  // clang-format on
};

/**
 * postpone_function_dispatcher - Perform a Postpone function - Implements ::function_dispatcher_t - @ingroup dispatcher_api
 */
int postpone_function_dispatcher(struct MuttWindow *win, int op)
{
  if (!win || !win->wdata)
    return FR_UNKNOWN;

  struct MuttWindow *dlg = dialog_find(win);
  if (!dlg)
    return FR_ERROR;

  struct PostponeData *pd = dlg->wdata;

  int rc = FR_UNKNOWN;
  for (size_t i = 0; PostponeFunctions[i].op != OP_NULL; i++)
  {
    const struct PostponeFunction *fn = &PostponeFunctions[i];
    if (fn->op == op)
    {
      rc = fn->function(pd, op);
      break;
    }
  }

  if (rc == FR_UNKNOWN) // Not our function
    return rc;

  const char *result = dispacher_get_retval_name(rc);
  mutt_debug(LL_DEBUG1, "Handled %s (%d) -> %s\n", opcodes_get_name(op), op, NONULL(result));

  return rc;
}

/**
 * postponed_get_mailbox_view - Extract the Mailbox from the Postponed Dialog
 * @param dlg Postponed Dialog
 * @retval ptr Mailbox view
 */
struct MailboxView *postponed_get_mailbox_view(struct MuttWindow *dlg)
{
  if (!dlg)
    return NULL;

  struct PostponeData *pd = dlg->wdata;
  if (!pd)
    return NULL;

  return pd->mailbox_view;
}
