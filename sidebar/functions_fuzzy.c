/**
 * @file
 * Sidebar fuzzy search functions
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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
 * @page sidebar_functions_sidebar Sidebar functions
 *
 * Sidebar functions
 */

#include "config.h"
#include <stdbool.h>
#include <string.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "editor/lib.h"
#include "fuzzy/lib.h"
#include "history/lib.h"
#include "index/lib.h"
#include "key/lib.h"
#include "functions_sidebar.h"

/**
 * sb_fuzzy_init_menu - Initialise the Fuzzy Search Menu - Implements ::init_keys_api
 * @retval ptr MenuDefinition for the fuzzy search
 */
struct MenuDefinition *sb_fuzzy_init_menu(void)
{
  struct MenuDefinition *md = menudef_new();

  struct SubMenu *sm_fuzzy = fuzzy_get_submenu();
  ASSERT(sm_fuzzy);
  struct SubMenu *sm_editor = editor_get_submenu();
  ASSERT(sm_editor);

  km_menu_add_submenu(md, sm_fuzzy);
  km_menu_add_submenu(md, sm_editor);

  return md;
}

/**
 * FuzzyFunctions - All the NeoMutt functions that the Sidebar Fuzzy Search supports
 */
static const struct SidebarFunction FuzzyFunctions[] = {
  // clang-format off
  { OP_SELECT_FIRST_ENTRY,          op_sidebar_first },
  { OP_SELECT_LAST_ENTRY,           op_sidebar_last },
  { OP_SELECT_NEXT_ENTRY,           op_sidebar_next },
  { OP_SCROLL_PAGE_DOWN,            op_sidebar_scroll_page_down },
  { OP_SELECT_PREVIOUS_ENTRY,           op_sidebar_prev },
  { OP_SCROLL_PAGE_UP,            op_sidebar_scroll_page_up },
  { 0, NULL },
  // clang-format on
};

/**
 * sb_fuzzy_function_dispatcher - Perform a Fuzzy Search function - Implements ::function_dispatcher_t - @ingroup dispatcher_api
 */
int sb_fuzzy_function_dispatcher(struct MuttWindow *win, const struct KeyEvent *event)
{
  if (!event || !win || !win->wdata)
    return FR_UNKNOWN;

  const int op = event->op;

  struct SidebarModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_SIDEBAR);

  struct SidebarFunctionData fdata = {
    .n = NeoMutt,
    .mod_data = mod_data,
    .wdata = win->wdata,
  };

  int rc = FR_UNKNOWN;
  for (size_t i = 0; FuzzyFunctions[i].op != OP_NULL; i++)
  {
    const struct SidebarFunction *fn = &FuzzyFunctions[i];
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

/**
 * sidebar_matcher_cb - React to keys as they are entered - Implements ::get_field_callback_t
 */
static void sidebar_matcher_cb(const char *text, void *data)
{
  struct MuttWindow *win = data;
  struct SidebarWindowData *wdata = win->wdata;
  wdata->hil_index = -1;
  wdata->repage = true;

  struct SbEntry **sbep = NULL;

  if (text[0] == '\0')
  {
    ARRAY_FOREACH(sbep, &wdata->entries)
    {
      struct SbEntry *sbe = *sbep;
      sbe->mailbox->visible = true;
      sbe->score = -1;
      if (wdata->hil_index == -1)
        wdata->hil_index = ARRAY_FOREACH_IDX_sbep;
    }
    wdata->win->actions |= WA_RECALC;
    return;
  }

  struct FuzzyOptions opts = { .smart_case = true };
  struct FuzzyResult result = { 0 };
  int best_score = -1;
  int best_index = -1;
  struct Buffer *buf = buf_pool_get();

  ARRAY_FOREACH(sbep, &wdata->entries)
  {
    struct SbEntry *sbe = *sbep;
    buf_printf(buf, "%s %s", sbe->box, sbe->display);
    int score = fuzzy_match(text, buf_string(buf), FUZZY_ALGO_SUBSEQ, &opts, &result);
    if (score > 0)
    {
      // Extra 2 points for new (unseen) mail
      //       1 point  for old (seen)   mail
      score += (2 * sbe->mailbox->msg_new);
      score += (sbe->mailbox->msg_unread - sbe->mailbox->msg_new);
    }
    sbe->score = score;
    if (score >= 0)
    {
      if ((best_score == -1) || (score > best_score))
      {
        best_score = score;
        best_index = ARRAY_FOREACH_IDX_sbep;
      }
      sbe->mailbox->visible = true;
    }
    else
    {
      sbe->mailbox->visible = false;
    }
  }

  wdata->hil_index = best_index;
  wdata->win->actions |= WA_RECALC;
  buf_pool_release(&buf);
}

/**
 * op_sidebar_start_search - Selects the last unhidden mailbox - Implements ::sidebar_function_t - @ingroup sidebar_function_api
 */
int op_sidebar_start_search(struct SidebarFunctionData *fdata, const struct KeyEvent *event)
{
  struct SidebarWindowData *wdata = fdata->wdata;
  if (ARRAY_EMPTY(&wdata->entries) || (wdata->hil_index < 0))
  {
    mutt_warning(_("There are no mailboxes"));
    return FR_ERROR;
  }

  const bool was_visible = cs_subset_bool(fdata->n->sub, "sidebar_visible");
  if (!was_visible)
  {
    cs_subset_str_native_set(fdata->n->sub, "sidebar_visible", true, NULL);
    mutt_window_reflow(NULL);
  }

  struct MenuDefinition *md = NULL;
  struct Buffer *buf = buf_pool_get();
  const int old_hil_index = wdata->hil_index;

  struct SbEntry **sbep = NULL;
  ARRAY_FOREACH(sbep, &wdata->entries)
  {
    struct SbEntry *sbe = *sbep;
    if (sbe->box[0] == '\0')
      sb_entry_set_display_name(sbe);
  }

  int rc = FR_NO_ACTION;
  wdata->search_active = true;

  md = sb_fuzzy_init_menu();

  if (mw_get_field_notify(_("Sidebar search: "), buf, MUTT_COMP_UNBUFFERED,
                          HC_NONE, NULL, NULL, sidebar_matcher_cb, wdata->win,
                          md, sb_fuzzy_function_dispatcher) != 0)
  {
    wdata->hil_index = old_hil_index;
    goto done;
  }

  if (!buf || buf_is_empty(buf) || (wdata->hil_index == wdata->opn_index))
  {
    wdata->hil_index = old_hil_index;
    goto done;
  }

  ARRAY_FOREACH(sbep, &wdata->entries)
  {
    struct SbEntry *sbe = *sbep;
    sbe->score = -1;
  }
  rc = FR_SUCCESS;

done:
  ARRAY_FOREACH(sbep, &wdata->entries)
  {
    (*sbep)->mailbox->visible = true;
  }
  wdata->search_active = false;
  wdata->repage = false;
  wdata->win->actions |= WA_RECALC;

  if (rc == FR_SUCCESS)
  {
    struct MuttWindow *dlg = dialog_find(wdata->win);
    index_change_folder(dlg, sb_get_highlight(wdata->win));
  }

  if (!was_visible)
  {
    cs_subset_str_native_set(fdata->n->sub, "sidebar_visible", false, NULL);
    mutt_window_reflow(NULL);
  }

  menudef_free(&md);
  buf_pool_release(&buf);
  return rc;
}
