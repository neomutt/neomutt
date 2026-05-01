/**
 * @file
 * Postponed Emails Functions
 *
 * @authors
 * Copyright (C) 2022-2026 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
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
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "functions.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "pattern/lib.h"
#include "module_data.h"

// clang-format off
/**
 * OpPostponed - Functions for the Postpone Menu
 */
static const struct MenuFuncOp OpPostponed[] = { /* map: postpone */
  { "exit",                          OP_EXIT },
  { "delete-entry",                  OP_DELETE },
  { "undelete-entry",                OP_UNDELETE },
  { "tag-entry",                     OP_TAG },
  { NULL, 0 },
};

/**
 * PostponedDefaultBindings - Key bindings for the Postpone Menu
 */
static const struct MenuOpSeq PostponedDefaultBindings[] = { /* map: postpone */
  { OP_DELETE,                             "d" },
  { OP_EXIT,                               "q" },
  { OP_TAG,                                "t" },
  { OP_UNDELETE,                           "u" },
  { 0, NULL },
};
// clang-format on

/**
 * postponed_init_keys - Initialise the Postponed Keybindings - Implements ::init_keys_api
 */
void postponed_init_keys(struct NeoMutt *n, struct SubMenu *sm_generic)
{
  struct MenuDefinition *md = NULL;
  struct SubMenu *sm = NULL;

  sm = km_register_submenu(OpPostponed);
  md = km_register_menu(MENU_POSTPONE, "postpone");
  km_menu_add_submenu(md, sm);
  km_menu_add_submenu(md, sm_generic);
  km_menu_add_bindings(md, PostponedDefaultBindings);

  struct PostponeModuleData *mod_data = neomutt_get_module_data(n, MODULE_ID_POSTPONE);
  ASSERT(mod_data);
  mod_data->menu_postpone = md;
}

/**
 * postpone_add_selection - Build a working set of Emails for an action
 * @param ea     Empty EmailArray to populate
 * @param menu   Postpone Menu
 * @param m      Mailbox
 * @param tagged Use tagged emails (tag-prefix)
 * @param count  Repeat-count (0 or 1 == just the current selection)
 * @retval num Number of emails added
 *
 * If @a tagged is true, the array is filled with the tagged emails and
 * @a count is ignored.
 *
 * Otherwise the array is filled with the current selection and the next
 * @a count - 1 emails.  Overruns are silently capped at the end of the list.
 */
static int postpone_add_selection(struct EmailArray *ea, struct Menu *menu,
                                  struct Mailbox *m, bool tagged, int count)
{
  if (!ea || !menu || !m || !m->emails)
    return 0;

  if (tagged)
  {
    for (int i = 0; i < m->msg_count; i++)
    {
      struct Email *e = m->emails[i];
      if (e && e->tagged)
        ARRAY_ADD(ea, e);
    }
  }
  else
  {
    const int index = menu_get_index(menu);
    if ((index < 0) || (index >= m->msg_count))
      return 0;

    int n = (count > 1) ? count : 1;
    if ((index + n) > m->msg_count)
      n = m->msg_count - index;

    for (int i = 0; i < n; i++)
    {
      struct Email *e = m->emails[index + i];
      if (e)
        ARRAY_ADD(ea, e);
    }
  }

  return ARRAY_SIZE(ea);
}

/**
 * postpone_apply_set_deleted - Apply the deleted flag to a working set of Emails
 * @param m       Mailbox
 * @param ea      Working set of Emails
 * @param deleted true to mark as deleted, false to undelete
 *
 * Also updates the postponed message count.
 */
static void postpone_apply_set_deleted(struct Mailbox *m, struct EmailArray *ea, bool deleted)
{
  if (!m || !ea)
    return;

  struct Email **ep = NULL;
  ARRAY_FOREACH(ep, ea)
  {
    if (*ep)
      mutt_set_flag(m, *ep, MUTT_DELETE, deleted, true);
  }

  struct PostponeModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_POSTPONE);
  if (mod_data)
    mod_data->post_count = m->msg_count - m->msg_deleted;
}

/**
 * op_delete - Delete the current entry - Implements ::postpone_function_t - @ingroup postpone_function_api
 *
 * This function handles:
 * - OP_DELETE
 * - OP_UNDELETE
 *
 * Supports repeat-count: `5<delete-entry>` deletes the current entry and the
 * next 4. Overruns are silently capped at the end of the list.
 */
static int op_delete(struct PostponeData *pd, const struct KeyEvent *event)
{
  struct Menu *menu = pd->menu;
  struct MailboxView *mv = pd->mailbox_view;
  struct Mailbox *m = mv->mailbox;

  const int index = menu_get_index(menu);
  const bool bf = (event->op == OP_DELETE);

  /* should deleted draft messages be saved in the trash folder? */
  struct EmailArray ea = ARRAY_HEAD_INITIALIZER;
  postpone_add_selection(&ea, menu, m, menu->tag_prefix, event->count);
  const int num = ARRAY_SIZE(&ea);
  postpone_apply_set_deleted(m, &ea, bf);
  ARRAY_FREE(&ea);

  const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
  if (!menu->tag_prefix && c_resolve && ((index + num) < menu->max))
  {
    const int new_index = index + num;
    menu_set_index(menu, new_index);
    if (new_index >= (menu->top + menu->page_len))
    {
      menu->top = new_index;
      menu_queue_redraw(menu, MENU_REDRAW_INDEX);
    }
  }
  else
  {
    menu_queue_redraw(menu, (menu->tag_prefix || (num > 1)) ? MENU_REDRAW_INDEX :
                                                              MENU_REDRAW_CURRENT);
  }

  return FR_SUCCESS;
}

/**
 * op_exit - Exit this menu - Implements ::postpone_function_t - @ingroup postpone_function_api
 */
static int op_exit(struct PostponeData *pd, const struct KeyEvent *event)
{
  pd->done = true;
  return FR_SUCCESS;
}

/**
 * op_generic_select_entry - Select the current entry - Implements ::postpone_function_t - @ingroup postpone_function_api
 */
static int op_generic_select_entry(struct PostponeData *pd, const struct KeyEvent *event)
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
 *
 * This function handles:
 * - OP_SEARCH
 * - OP_SEARCH_NEXT
 * - OP_SEARCH_OPPOSITE
 * - OP_SEARCH_REVERSE
 */
static int op_search(struct PostponeData *pd, const struct KeyEvent *event)
{
  SearchFlags flags = SEARCH_NO_FLAGS;
  switch (event->op)
  {
    case OP_SEARCH:
      flags |= SEARCH_PROMPT;
      pd->search_state->reverse = false;
      break;
    case OP_SEARCH_REVERSE:
      flags |= SEARCH_PROMPT;
      pd->search_state->reverse = true;
      break;
    case OP_SEARCH_NEXT:
      break;
    case OP_SEARCH_OPPOSITE:
      flags |= SEARCH_OPPOSITE;
      break;
  }

  int index = menu_get_index(pd->menu);
  struct MailboxView *mv = pd->mailbox_view;
  index = mutt_search_command(mv, pd->menu, index, pd->search_state, flags);
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
int postpone_function_dispatcher(struct MuttWindow *win, const struct KeyEvent *event)
{
  // The Dispatcher may be called on any Window in the Dialog
  struct MuttWindow *dlg = dialog_find(win);
  if (!event || !dlg || !dlg->wdata)
  {
    dispatcher_flush_on_error(FR_ERROR);
    return FR_ERROR;
  }

  const int op = event->op;
  struct Menu *menu = dlg->wdata;
  struct PostponeData *pd = menu->mdata;
  if (!pd)
  {
    dispatcher_flush_on_error(FR_ERROR);
    return FR_ERROR;
  }

  int rc = FR_UNKNOWN;
  for (size_t i = 0; PostponeFunctions[i].op != OP_NULL; i++)
  {
    const struct PostponeFunction *fn = &PostponeFunctions[i];
    if (fn->op == op)
    {
      rc = fn->function(pd, event);
      break;
    }
  }

  if (rc == FR_UNKNOWN) // Not our function
    return rc;

  const char *result = dispatcher_get_retval_name(rc);
  mutt_debug(LL_DEBUG1, "Handled %s (%d) -> %s\n", opcodes_get_name(op), op, NONULL(result));

  dispatcher_flush_on_error(rc);
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

  struct Menu *menu = dlg->wdata;
  struct PostponeData *pd = menu->mdata;
  if (!pd)
    return NULL;

  return pd->mailbox_view;
}
