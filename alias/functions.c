/**
 * @file
 * Alias functions
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
 * @page alias_functions Alias functions
 *
 * Alias functions
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "functions.h"
#include "lib.h"
#include "editor/lib.h"
#include "history/lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "pattern/lib.h"
#include "question/lib.h"
#include "alias.h"
#include "gui.h"
#include "module_data.h"
#include "sort.h"

// clang-format off
/**
 * OpAlias - Functions for the Alias Menu
 */
static const struct MenuFuncOp OpAlias[] = { /* map: alias */
  { "delete-entry",                  OP_DELETE },
  { "limit",                         OP_MAIN_LIMIT },
  { "mail",                          OP_MAIL },
  { "sort",                          OP_SORT },
  { "sort-reverse",                  OP_SORT_REVERSE },
  { "tag-pattern",                   OP_MAIN_TAG_PATTERN },
  { "undelete-entry",                OP_UNDELETE },
  { "untag-pattern",                 OP_MAIN_UNTAG_PATTERN },

  // Deprecated
  { "sort-alias",                    OP_SORT,         MFF_DEPRECATED },
  { "sort-alias-reverse",            OP_SORT_REVERSE, MFF_DEPRECATED },
  { NULL, 0 },
};

/**
 * OpQuery - Functions for the external Query Menu
 */
static const struct MenuFuncOp OpQuery[] = { /* map: query */
  { "create-alias",                  OP_CREATE_ALIAS },
  { "limit",                         OP_MAIN_LIMIT },
  { "mail",                          OP_MAIL },
  { "query",                         OP_QUERY },
  { "query-append",                  OP_QUERY_APPEND },
  { "sort",                          OP_SORT },
  { "sort-reverse",                  OP_SORT_REVERSE },
  { "tag-pattern",                   OP_MAIN_TAG_PATTERN },
  { "untag-pattern",                 OP_MAIN_UNTAG_PATTERN },
  { NULL, 0 },
};

/**
 * AliasDefaultBindings - Key bindings for the Alias Menu
 */
static const struct MenuOpSeq AliasDefaultBindings[] = { /* map: alias */
  { OP_DELETE,                             "d" },
  { OP_MAIL,                               "m" },
  { OP_MAIN_LIMIT,                         "l" },
  { OP_MAIN_TAG_PATTERN,                   "T" },
  { OP_MAIN_UNTAG_PATTERN,                 "\024" },           // <Ctrl-T>
  { OP_SORT,                               "o" },
  { OP_SORT_REVERSE,                       "O" },
  { OP_TAG,                                "<space>" },
  { OP_UNDELETE,                           "u" },
  { 0, NULL },
};

/**
 * QueryDefaultBindings - Key bindings for the external Query Menu
 */
static const struct MenuOpSeq QueryDefaultBindings[] = { /* map: query */
  { OP_CREATE_ALIAS,                       "a" },
  { OP_MAIL,                               "m" },
  { OP_MAIN_LIMIT,                         "l" },
  { OP_MAIN_TAG_PATTERN,                   "T" },
  { OP_MAIN_UNTAG_PATTERN,                 "\024" },           // <Ctrl-T>
  { OP_QUERY,                              "Q" },
  { OP_QUERY_APPEND,                       "A" },
  { OP_SORT,                               "o" },
  { OP_SORT_REVERSE,                       "O" },
  { OP_TAG,                                "<space>" },
  { 0, NULL },
};
// clang-format on

/**
 * alias_init_keys - Initialise the Alias Keybindings - Implements ::init_keys_api
 */
void alias_init_keys(struct NeoMutt *n, struct SubMenu *sm_generic)
{
  struct AliasModuleData *mod_data = neomutt_get_module_data(n, MODULE_ID_ALIAS);
  ASSERT(mod_data);

  struct MenuDefinition *md = NULL;
  struct SubMenu *sm = NULL;

  sm = km_register_submenu(OpAlias);
  md = km_register_menu(MENU_ALIAS, "alias");
  km_menu_add_submenu(md, sm);
  km_menu_add_submenu(md, sm_generic);
  km_menu_add_bindings(md, AliasDefaultBindings);

  mod_data->menu_alias = md;

  sm = km_register_submenu(OpQuery);
  md = km_register_menu(MENU_QUERY, "query");
  km_menu_add_submenu(md, sm);
  km_menu_add_submenu(md, sm_generic);
  km_menu_add_bindings(md, QueryDefaultBindings);

  mod_data->menu_query = md;
}

/**
 * op_create_alias - create an alias from a message sender - Implements ::alias_function_t - @ingroup alias_function_api
 */
static int op_create_alias(struct AliasFunctionData *fdata, const struct KeyEvent *event)
{
  struct AliasMenuData *mdata = fdata->wdata;
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
    struct AliasView *av = ARRAY_GET(&mdata->ava, menu_get_index(menu));
    if (!av)
      return FR_NO_ACTION;

    struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
    if (alias_to_addrlist(&al, av->alias))
    {
      alias_create(&al, mdata->sub);
      mutt_addrlist_clear(&al);
    }
  }
  return FR_SUCCESS;
}

/**
 * alias_add_selection - Build a working set of AliasView pointers for an action
 * @param avpa   AliasView Array to populate
 * @param mdata  Alias Menu data
 * @param tagged Use tagged AliasViews (tag-prefix)
 * @param count   Repeat-count (0 or 1 == just the current selection)
 * @retval num Number of AliasViews added
 *
 * If @a tagged is true, the array is filled with the tagged AliasViews and
 * @a count is ignored.
 *
 * Otherwise the array is filled with the current selection and the next
 * @a count - 1 visible AliasViews. Overruns are silently capped at the end
 * of the visible list.
 */
static int alias_add_selection(struct AliasViewPtrArray *avpa,
                               struct AliasMenuData *mdata, bool tagged, int count)
{
  if (!avpa || !mdata)
    return 0;

  if (tagged)
  {
    struct AliasView *avp = NULL;
    ARRAY_FOREACH(avp, &mdata->ava)
    {
      if (avp->is_tagged)
        ARRAY_ADD(avpa, avp);
    }
  }
  else
  {
    struct Menu *menu = mdata->menu;
    const int index = menu_get_index(menu);
    if ((index < 0) || (index >= menu->max))
      return 0;

    int n = (count > 1) ? count : 1;
    if ((index + n) > menu->max)
      n = menu->max - index;

    for (int i = 0; i < n; i++)
    {
      struct AliasView *av = ARRAY_GET(&mdata->ava, index + i);
      if (av)
        ARRAY_ADD(avpa, av);
    }
  }

  return ARRAY_SIZE(avpa);
}

/**
 * alias_apply_set_deleted - Apply the deleted flag to a working set of AliasViews
 * @param avpa Working set of AliasView pointers
 * @param deleted true to mark as deleted, false to undelete
 */
static void alias_apply_set_deleted(struct AliasViewPtrArray *avpa, bool deleted)
{
  if (!avpa)
    return;

  struct AliasView **avpp = NULL;
  ARRAY_FOREACH(avpp, avpa)
  {
    if (*avpp)
      (*avpp)->is_deleted = deleted;
  }
}

/**
 * op_delete - delete the current entry - Implements ::alias_function_t - @ingroup alias_function_api
 *
 * This function handles:
 * - OP_DELETE
 * - OP_UNDELETE
 *
 * Supports repeat-count: `5<delete-entry>` deletes the current entry and the
 * next 4. Overruns are silently capped at the end of the visible list.
 */
static int op_delete(struct AliasFunctionData *fdata, const struct KeyEvent *event)
{
  struct AliasMenuData *mdata = fdata->wdata;
  struct Menu *menu = mdata->menu;
  const bool deleted = (event->op == OP_DELETE);

  struct AliasViewPtrArray avpa = ARRAY_HEAD_INITIALIZER;
  alias_add_selection(&avpa, mdata, menu->tag_prefix, event->count);
  if (ARRAY_EMPTY(&avpa))
  {
    ARRAY_FREE(&avpa);
    return FR_NO_ACTION;
  }
  alias_apply_set_deleted(&avpa, deleted);
  const int num = ARRAY_SIZE(&avpa);
  ARRAY_FREE(&avpa);

  if (menu->tag_prefix)
  {
    menu_queue_redraw(menu, MENU_REDRAW_INDEX);
  }
  else
  {
    const int index = menu_get_index(menu);
    menu_queue_redraw(menu, (num > 1) ? MENU_REDRAW_INDEX : MENU_REDRAW_CURRENT);
    const bool c_resolve = cs_subset_bool(mdata->sub, "resolve");
    if (c_resolve && ((index + num) < menu->max))
    {
      menu_set_index(menu, index + num);
      menu_queue_redraw(menu, MENU_REDRAW_INDEX);
    }
  }
  return FR_SUCCESS;
}

/**
 * op_quit - Save changes and exit this dialog - Implements ::alias_function_t - @ingroup alias_function_api
 */
static int op_quit(struct AliasFunctionData *fdata, const struct KeyEvent *event)
{
  return FR_DONE;
}

/**
 * op_jump - Jump to an index number - Implements ::alias_function_t - @ingroup alias_function_api
 */
static int op_jump(struct AliasFunctionData *fdata, const struct KeyEvent *event)
{
  int num = event->count;

  if (num == 0)
    return FR_NO_ACTION;

  struct AliasMenuData *mdata = fdata->wdata;
  struct Menu *menu = mdata->menu;

  if ((num < 1) || (num > menu->max))
  {
    mutt_warning(_("Invalid message number"));
    return FR_ERROR;
  }

  menu_set_index(menu, num - 1); // Num is 1-based
  return FR_SUCCESS;
}

/**
 * alias_select_entries - Select entries to return from the Alias Dialog
 * @param mdata Alias Menu data
 * @param count Repeat-count (0 or 1 == just the current selection)
 * @retval enum #FunctionRetval
 * @note AliasMenuData.is_tagged will show the user's selection
 */
static int alias_select_entries(struct AliasMenuData *mdata, int count)
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
    struct AliasViewPtrArray avpa = ARRAY_HEAD_INITIALIZER;
    alias_add_selection(&avpa, mdata, false, count);
    if (ARRAY_EMPTY(&avpa))
    {
      ARRAY_FREE(&avpa);
      return FR_NO_ACTION;
    }

    // Untag all but the selected alias(es)
    struct AliasView *avp = NULL;
    ARRAY_FOREACH(avp, &mdata->ava)
    {
      avp->is_tagged = false;
    }

    struct AliasView **avpp = NULL;
    ARRAY_FOREACH(avpp, &avpa)
    {
      if (*avpp)
        (*avpp)->is_tagged = true;
    }
    ARRAY_FREE(&avpa);
  }

  return FR_CONTINUE;
}

/**
 * op_generic_select_entry - select the current entry - Implements ::alias_function_t - @ingroup alias_function_api
 */
static int op_generic_select_entry(struct AliasFunctionData *fdata, const struct KeyEvent *event)
{
  if (event->count > 0)
    return op_jump(fdata, event);

  return alias_select_entries(fdata->wdata, 0);
}

/**
 * op_mail - mail the selected entries - Implements ::alias_function_t - @ingroup alias_function_api
 */
static int op_mail(struct AliasFunctionData *fdata, const struct KeyEvent *event)
{
  return alias_select_entries(fdata->wdata, event->count);
}

/**
 * op_main_limit - show only messages matching a pattern - Implements ::alias_function_t - @ingroup alias_function_api
 */
static int op_main_limit(struct AliasFunctionData *fdata, const struct KeyEvent *event)
{
  struct AliasMenuData *mdata = fdata->wdata;
  struct Menu *menu = mdata->menu;
  int rc = mutt_pattern_alias_func(_("Limit to addresses matching: "), mdata,
                                   PAA_VISIBLE, menu);
  if (rc != 0)
    return FR_NO_ACTION;

  alias_array_sort(&mdata->ava, mdata->sub);
  alias_set_title(mdata->sbar, mdata->title, mdata->limit);
  menu_queue_redraw(menu, MENU_REDRAW_FULL);
  window_redraw(NULL);

  return FR_SUCCESS;
}

/**
 * op_main_tag_pattern - Tag messages matching a pattern - Implements ::alias_function_t - @ingroup alias_function_api
 */
static int op_main_tag_pattern(struct AliasFunctionData *fdata, const struct KeyEvent *event)
{
  struct AliasMenuData *mdata = fdata->wdata;
  struct Menu *menu = mdata->menu;
  int rc = mutt_pattern_alias_func(_("Tag addresses matching: "), mdata, PAA_TAG, menu);
  if (rc != 0)
    return FR_NO_ACTION;

  menu_queue_redraw(menu, MENU_REDRAW_FULL);
  window_redraw(NULL);

  return FR_SUCCESS;
}

/**
 * op_main_untag_pattern - Untag messages matching a pattern - Implements ::alias_function_t - @ingroup alias_function_api
 */
static int op_main_untag_pattern(struct AliasFunctionData *fdata, const struct KeyEvent *event)
{
  struct AliasMenuData *mdata = fdata->wdata;
  struct Menu *menu = mdata->menu;
  int rc = mutt_pattern_alias_func(_("Untag addresses matching: "), mdata, PAA_UNTAG, menu);
  if (rc != 0)
    return FR_NO_ACTION;

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
static int op_query(struct AliasFunctionData *fdata, const struct KeyEvent *event)
{
  struct AliasMenuData *mdata = fdata->wdata;
  struct Buffer *buf = mdata->query;
  if ((mw_get_field(_("Query: "), buf, MUTT_COMP_NONE, HC_OTHER, NULL, NULL) != 0) ||
      buf_is_empty(buf))
  {
    return FR_NO_ACTION;
  }

  const int op = event->op;
  if (op == OP_QUERY)
  {
    ARRAY_FREE(&mdata->ava);
    aliaslist_clear(mdata->aa);
  }

  struct Menu *menu = mdata->menu;
  struct AliasArray aa = ARRAY_HEAD_INITIALIZER;

  query_run(buf_string(buf), true, &aa, mdata->sub);
  menu_queue_redraw(menu, MENU_REDRAW_FULL);
  char title[256] = { 0 };
  snprintf(title, sizeof(title), "%s%s", _("Query: "), buf_string(buf));
  sbar_set_title(mdata->sbar, title);

  if (ARRAY_EMPTY(&aa))
  {
    if (op == OP_QUERY)
      menu->max = 0;
    return FR_NO_ACTION;
  }

  struct Alias **ap = NULL;
  ARRAY_FOREACH(ap, &aa)
  {
    alias_array_alias_add(&mdata->ava, *ap);
    ARRAY_ADD(mdata->aa, *ap); // Transfer
  }
  ARRAY_FREE(&aa); // Free the array structure but not the aliases
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
 * - OP_SEARCH_PREVIOUS
 * - OP_SEARCH_REVERSE
 */
static int op_search(struct AliasFunctionData *fdata, const struct KeyEvent *event)
{
  struct AliasMenuData *mdata = fdata->wdata;
  SearchFlags flags = SEARCH_NONE;
  switch (event->op)
  {
    case OP_SEARCH:
      flags |= SEARCH_PROMPT;
      mdata->search_state->reverse = false;
      break;
    case OP_SEARCH_REVERSE:
      flags |= SEARCH_PROMPT;
      mdata->search_state->reverse = true;
      break;
    case OP_SEARCH_NEXT:
      break;
    case OP_SEARCH_PREVIOUS:
      flags |= SEARCH_PREVIOUS;
      break;
  }

  struct Menu *menu = mdata->menu;
  int index = menu_get_index(menu);
  index = mutt_search_alias_command(menu, index, mdata->search_state, flags);
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
static int op_sort(struct AliasFunctionData *fdata, const struct KeyEvent *event)
{
  struct AliasMenuData *mdata = fdata->wdata;
  int sort = cs_subset_sort(mdata->sub, "alias_sort");
  bool resort = true;
  const int op = event->op;
  bool reverse = (op == OP_SORT_REVERSE);

  switch (mw_multi_choice(reverse ?
                              /* L10N: The highlighted letters must match the "Sort" options */
                              _("Rev-Sort (a)lias, (n)ame, (e)mail or (u)nsorted?") :
                              /* L10N: The highlighted letters must match the "Rev-Sort" options */
                              _("Sort (a)lias, (n)ame, (e)mail or (u)nsorted?"),
                          /* L10N: These must match the highlighted letters from "Sort" and "Rev-Sort" */
                          _("aneu")))
  {
    case -1: /* abort */
      resort = false;
      break;

    case 1: /* (a)lias */
      sort = ALIAS_SORT_ALIAS;
      break;

    case 2: /* (n)ame */
      sort = ALIAS_SORT_NAME;
      break;

    case 3: /* (e)mail */
      sort = ALIAS_SORT_EMAIL;
      break;

    case 4: /* (u)nsorted */
      sort = ALIAS_SORT_UNSORTED;
      break;
  }

  if (resort)
  {
    sort |= reverse ? SORT_REVERSE : 0;

    // This will trigger a WA_RECALC
    cs_subset_str_native_set(mdata->sub, "alias_sort", sort, NULL);
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
  { OP_EXIT,                   op_quit },
  { OP_GENERIC_SELECT_ENTRY,   op_generic_select_entry },
  { OP_MAIL,                   op_mail },
  { OP_MAIN_LIMIT,             op_main_limit },
  { OP_MAIN_TAG_PATTERN,       op_main_tag_pattern },
  { OP_MAIN_UNTAG_PATTERN,     op_main_untag_pattern },
  { OP_QUERY,                  op_query },
  { OP_QUERY_APPEND,           op_query },
  { OP_QUIT,                   op_quit },
  { OP_SEARCH,                 op_search },
  { OP_SEARCH_NEXT,            op_search },
  { OP_SEARCH_PREVIOUS,        op_search },
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
int alias_function_dispatcher(struct MuttWindow *win, const struct KeyEvent *event)
{
  // The Dispatcher may be called on any Window in the Dialog
  struct MuttWindow *dlg = dialog_find(win);
  if (!event || !dlg || !dlg->wdata)
    return FR_ERROR;

  struct Menu *menu = dlg->wdata;
  struct AliasMenuData *mdata = menu->mdata;
  if (!mdata)
    return FR_ERROR;

  const int op = event->op;

  struct AliasFunctionData fdata = {
    .n = NeoMutt,
    .wdata = mdata,
  };

  int rc = FR_UNKNOWN;
  for (size_t i = 0; AliasFunctions[i].op != OP_NULL; i++)
  {
    const struct AliasFunction *fn = &AliasFunctions[i];
    if (fn->op == op)
    {
      rc = fn->function(&fdata, event);
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
