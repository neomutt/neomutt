/**
 * @file
 * Alias functions
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
 * @page alias_functions Alias functions
 *
 * Alias functions
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "functions.h"
#include "lib.h"
#include "enter/lib.h"
#include "menu/lib.h"
#include "pattern/lib.h"
#include "question/lib.h"
#include "alias.h"
#include "gui.h"
#include "opcodes.h"

/**
 * op_create_alias - create an alias from a message sender - Implements ::alias_function_t - @ingroup alias_function_api
 */
static int op_create_alias(struct AliasMenuData *mdata, int op)
{
  struct Menu *menu = mdata->menu;

  if (menu->tag_prefix)
  {
    struct AddressList naddr = TAILQ_HEAD_INITIALIZER(naddr);

    struct AliasView *avp = NULL;
    ARRAY_FOREACH(avp, &mdata->ava)
    {
      if (!avp->is_tagged)
        continue;

      struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
      if (alias_to_addrlist(&al, avp->alias))
      {
        mutt_addrlist_copy(&naddr, &al, false);
        mutt_addrlist_clear(&al);
      }
    }

    alias_create(&naddr, mdata->sub);
    mutt_addrlist_clear(&naddr);
  }
  else
  {
    struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
    if (alias_to_addrlist(&al, ARRAY_GET(&mdata->ava, menu_get_index(menu))->alias))
    {
      alias_create(&al, mdata->sub);
      mutt_addrlist_clear(&al);
    }
  }
  return FR_SUCCESS;
}

/**
 * op_delete - delete the current entry - Implements ::alias_function_t - @ingroup alias_function_api
 */
static int op_delete(struct AliasMenuData *mdata, int op)
{
  struct Menu *menu = mdata->menu;

  if (menu->tag_prefix)
  {
    struct AliasView *avp = NULL;
    ARRAY_FOREACH(avp, &mdata->ava)
    {
      if (avp->is_tagged)
        avp->is_deleted = (op == OP_DELETE);
    }
    menu_queue_redraw(menu, MENU_REDRAW_INDEX);
  }
  else
  {
    int index = menu_get_index(menu);
    ARRAY_GET(&mdata->ava, index)->is_deleted = (op == OP_DELETE);
    menu_queue_redraw(menu, MENU_REDRAW_CURRENT);
    const bool c_resolve = cs_subset_bool(mdata->sub, "resolve");
    if (c_resolve && (index < (menu->max - 1)))
    {
      menu_set_index(menu, index + 1);
      menu_queue_redraw(menu, MENU_REDRAW_INDEX);
    }
  }
  return FR_SUCCESS;
}

/**
 * op_exit - exit this menu - Implements ::alias_function_t - @ingroup alias_function_api
 */
static int op_exit(struct AliasMenuData *mdata, int op)
{
  return FR_DONE;
}

/**
 * op_generic_select_entry - select the current entry - Implements ::alias_function_t - @ingroup alias_function_api
 *
 * This function handles:
 * - OP_GENERIC_SELECT_ENTRY
 * - OP_MAIL
 *
 * @note AliasMenuData.is_tagged will show the user's selection
 */
static int op_generic_select_entry(struct AliasMenuData *mdata, int op)
{
  struct Menu *menu = mdata->menu;
  if (menu->tag_prefix)
  {
    // Untag any non-visible aliases
    struct AliasView *avp = NULL;
    ARRAY_FOREACH(avp, &mdata->ava)
    {
      if (avp->is_tagged && !avp->is_visible)
        avp->is_tagged = false;
    }
  }
  else
  {
    // Untag all but the current alias
    struct AliasView *avp = NULL;
    const int idx = menu_get_index(menu);
    ARRAY_FOREACH(avp, &mdata->ava)
    {
      avp->is_tagged = (ARRAY_FOREACH_IDX == idx);
    }
  }

  return FR_CONTINUE;
}

/**
 * op_main_limit - show only messages matching a pattern - Implements ::alias_function_t - @ingroup alias_function_api
 */
static int op_main_limit(struct AliasMenuData *mdata, int op)
{
  struct Menu *menu = mdata->menu;
  int rc = mutt_pattern_alias_func(_("Limit to addresses matching: "), mdata, menu);
  if (rc != 0)
    return FR_NO_ACTION;

  alias_array_sort(&mdata->ava, mdata->sub);
  alias_set_title(mdata->sbar, mdata->title, mdata->limit);
  menu_queue_redraw(menu, MENU_REDRAW_FULL);
  window_redraw(NULL);

  return FR_SUCCESS;
}

/**
 * op_query - query external program for addresses - Implements ::alias_function_t - @ingroup alias_function_api
 *
 * This function handles:
 * - OP_QUERY
 * - OP_QUERY_APPEND
 */
static int op_query(struct AliasMenuData *mdata, int op)
{
  struct Buffer *buf = mdata->query;
  if ((mw_get_field(_("Query: "), buf, MUTT_COMP_NO_FLAGS, false, NULL, NULL, NULL) != 0) ||
      buf_is_empty(buf))
  {
    return FR_NO_ACTION;
  }

  if (op == OP_QUERY)
  {
    ARRAY_FREE(&mdata->ava);
    aliaslist_free(mdata->al);
  }

  struct Menu *menu = mdata->menu;
  struct AliasList al = TAILQ_HEAD_INITIALIZER(al);

  query_run(buf_string(buf), true, &al, mdata->sub);
  menu_queue_redraw(menu, MENU_REDRAW_FULL);
  char title[256] = { 0 };
  snprintf(title, sizeof(title), "%s%s", _("Query: "), buf_string(buf));
  sbar_set_title(mdata->sbar, title);

  if (TAILQ_EMPTY(&al))
  {
    if (op == OP_QUERY)
      menu->max = 0;
    return FR_NO_ACTION;
  }

  struct Alias *np = NULL;
  struct Alias *tmp = NULL;
  TAILQ_FOREACH_SAFE(np, &al, entries, tmp)
  {
    alias_array_alias_add(&mdata->ava, np);
    TAILQ_REMOVE(&al, np, entries);
    TAILQ_INSERT_TAIL(mdata->al, np, entries); // Transfer
  }
  alias_array_sort(&mdata->ava, mdata->sub);
  menu->max = ARRAY_SIZE(&mdata->ava);
  return FR_SUCCESS;
}

/**
 * op_search - search for a regular expression - Implements ::alias_function_t - @ingroup alias_function_api
 *
 * This function handles:
 * - OP_SEARCH
 * - OP_SEARCH_NEXT
 * - OP_SEARCH_OPPOSITE
 * - OP_SEARCH_REVERSE
 */
static int op_search(struct AliasMenuData *mdata, int op)
{
  struct Menu *menu = mdata->menu;
  int index = mutt_search_alias_command(menu, menu_get_index(menu), op);
  if (index == -1)
    return FR_NO_ACTION;

  menu_set_index(menu, index);
  return FR_SUCCESS;
}

/**
 * op_sort - sort aliases - Implements ::alias_function_t - @ingroup alias_function_api
 *
 * This function handles:
 * - OP_SORT
 * - OP_SORT_REVERSE
 */
static int op_sort(struct AliasMenuData *mdata, int op)
{
  int sort = cs_subset_sort(mdata->sub, "sort_alias");
  bool resort = true;
  bool reverse = (op == OP_SORT_REVERSE);

  switch (mw_multi_choice(reverse ?
                              /* L10N: The highlighted letters must match the "Sort" options */
                              _("Rev-Sort (a)lias, a(d)dress or (u)nsorted?") :
                              /* L10N: The highlighted letters must match the "Rev-Sort" options */
                              _("Sort (a)lias, a(d)dress or (u)nsorted?"),
                          /* L10N: These must match the highlighted letters from "Sort" and "Rev-Sort" */
                          _("adu")))
  {
    case -1: /* abort */
      resort = false;
      break;

    case 1: /* (a)lias */
      sort = SORT_ALIAS;
      break;

    case 2: /* a(d)dress */
      sort = SORT_ADDRESS;
      break;

    case 3: /* (u)nsorted */
      sort = SORT_ORDER;
      break;
  }

  if (resort)
  {
    sort |= reverse ? SORT_REVERSE : 0;

    // This will trigger a WA_RECALC
    cs_subset_str_native_set(mdata->sub, "sort_alias", sort, NULL);
  }

  return FR_SUCCESS;
}

// -----------------------------------------------------------------------------

/**
 * AliasFunctions - All the NeoMutt functions that the Alias supports
 */
static const struct AliasFunction AliasFunctions[] = {
  // clang-format off
  { OP_CREATE_ALIAS,           op_create_alias },
  { OP_DELETE,                 op_delete },
  { OP_EXIT,                   op_exit },
  { OP_GENERIC_SELECT_ENTRY,   op_generic_select_entry },
  { OP_MAIL,                   op_generic_select_entry },
  { OP_MAIN_LIMIT,             op_main_limit },
  { OP_QUERY,                  op_query },
  { OP_QUERY_APPEND,           op_query },
  { OP_SEARCH,                 op_search },
  { OP_SEARCH_NEXT,            op_search },
  { OP_SEARCH_OPPOSITE,        op_search },
  { OP_SEARCH_REVERSE,         op_search },
  { OP_SORT,                   op_sort },
  { OP_SORT_REVERSE,           op_sort },
  { OP_UNDELETE,               op_delete },
  { 0, NULL },
  // clang-format on
};

/**
 * alias_function_dispatcher - Perform a Alias function - Implements ::function_dispatcher_t - @ingroup dispatcher_api
 */
int alias_function_dispatcher(struct MuttWindow *win, int op)
{
  if (!win || !win->wdata)
    return FR_UNKNOWN;

  struct Menu *menu = win->wdata;
  struct AliasMenuData *mdata = menu->mdata;
  int rc = FR_UNKNOWN;
  for (size_t i = 0; AliasFunctions[i].op != OP_NULL; i++)
  {
    const struct AliasFunction *fn = &AliasFunctions[i];
    if (fn->op == op)
    {
      rc = fn->function(mdata, op);
      break;
    }
  }

  if (rc == FR_UNKNOWN) // Not our function
    return rc;

  const char *result = dispacher_get_retval_name(rc);
  mutt_debug(LL_DEBUG1, "Handled %s (%d) -> %s\n", opcodes_get_name(op), op, NONULL(result));

  return rc;
}
