/**
 * @file
 * Sidebar functions
 *
 * @authors
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020-2026 Richard Russon <rich@flatcap.org>
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
#include "functions_sidebar.h"
#include "lib.h"
#include "editor/lib.h"
#include "index/lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "functions_fuzzy.h"
#include "module_data.h"

// clang-format off
/**
 * OpSidebar - Functions for the Sidebar Window
 */
static const struct MenuFuncOp OpSidebar[] = { /* map: sidebar */
  { "sidebar-first",                       OP_SIDEBAR_FIRST },
  { "sidebar-last",                        OP_SIDEBAR_LAST },
  { "sidebar-next",                        OP_SIDEBAR_NEXT },
  { "sidebar-next-new",                    OP_SIDEBAR_NEXT_NEW },
  { "sidebar-open",                        OP_SIDEBAR_OPEN },
  { "sidebar-page-down",                   OP_SIDEBAR_PAGE_DOWN },
  { "sidebar-page-up",                     OP_SIDEBAR_PAGE_UP },
  { "sidebar-prev",                        OP_SIDEBAR_PREV },
  { "sidebar-prev-new",                    OP_SIDEBAR_PREV_NEW },
  { "sidebar-scroll-half-down",            OP_SIDEBAR_SCROLL_HALF_DOWN },
  { "sidebar-scroll-half-up",              OP_SIDEBAR_SCROLL_HALF_UP },
  { "sidebar-scroll-line-down",            OP_SIDEBAR_SCROLL_LINE_DOWN },
  { "sidebar-scroll-line-up",              OP_SIDEBAR_SCROLL_LINE_UP },
  { "sidebar-scroll-selection-to-bottom",  OP_SIDEBAR_SCROLL_SELECTION_TO_BOTTOM },
  { "sidebar-scroll-selection-to-middle",  OP_SIDEBAR_SCROLL_SELECTION_TO_MIDDLE },
  { "sidebar-scroll-selection-to-top",     OP_SIDEBAR_SCROLL_SELECTION_TO_TOP },
  { "sidebar-select-page-bottom",          OP_SIDEBAR_SELECT_PAGE_BOTTOM },
  { "sidebar-select-page-middle",          OP_SIDEBAR_SELECT_PAGE_MIDDLE },
  { "sidebar-select-page-top",             OP_SIDEBAR_SELECT_PAGE_TOP },
  { "sidebar-start-search",                OP_SIDEBAR_START_SEARCH },
  { "sidebar-toggle-virtual",              OP_SIDEBAR_TOGGLE_VIRTUAL },
  { "sidebar-toggle-visible",              OP_SIDEBAR_TOGGLE_VISIBLE },
  { NULL, 0 },
};

/**
 * SidebarDefaultBindings - Key bindings for the Sidebar Window
 */
const struct MenuOpSeq SidebarDefaultBindings[] = { /* map: sidebar */
  { 0, NULL },
};
// clang-format on

/**
 * sidebar_init_keys - Initialise the Sidebar Keybindings - Implements ::init_keys_api
 */
void sidebar_init_keys(struct NeoMutt *n, struct SubMenu *sm_generic)
{
  struct SidebarModuleData *mod_data = neomutt_get_module_data(n, MODULE_ID_SIDEBAR);
  ASSERT(mod_data);

  struct MenuDefinition *md = NULL;
  struct SubMenu *sm = NULL;
  struct SubMenu *sm_editor = editor_get_submenu();
  ASSERT(sm_editor);

  sm = km_register_submenu(OpSidebar);
  md = km_register_menu(MENU_SIDEBAR, "sidebar");
  km_menu_add_submenu(md, sm);
  km_menu_add_submenu(md, sm_editor);
  km_menu_add_bindings(md, SidebarDefaultBindings);

  mod_data->md_sidebar = md;
  mod_data->sm_sidebar = sm;
}

/**
 * sidebar_get_submenu - Get the Sidebar SubMenu
 * @retval ptr Sidebar SubMenu
 */
struct SubMenu *sidebar_get_submenu(void)
{
  struct SidebarModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_SIDEBAR);
  ASSERT(mod_data);

  return mod_data->sm_sidebar;
}

/**
 * sb_next - Find the next unhidden Mailbox
 * @param wdata Sidebar data
 * @retval true Mailbox found
 */
bool sb_next(struct SidebarWindowData *wdata)
{
  struct SbEntry **sbep = NULL;
  ARRAY_FOREACH_FROM(sbep, &wdata->entries, wdata->hil_index + 1)
  {
    if (!(*sbep)->is_hidden)
    {
      wdata->hil_index = ARRAY_FOREACH_IDX_sbep;
      return true;
    }
  }

  return false;
}

/**
 * sb_next_n - Move down N unhidden Mailboxes, capping at the last
 * @param wdata Sidebar data
 * @param count Number of entries to move
 * @retval true  Moved at least one entry
 * @retval false Already at the last unhidden entry
 */
static bool sb_next_n(struct SidebarWindowData *wdata, int count)
{
  int orig = wdata->hil_index;
  for (int i = 0; i < count; i++)
  {
    if (!sb_next(wdata))
      break;
  }
  return (wdata->hil_index != orig);
}

/**
 * sb_prev_n - Move up N unhidden Mailboxes, capping at the first
 * @param wdata Sidebar data
 * @param count Number of entries to move
 * @retval true  Moved at least one entry
 * @retval false Already at the first unhidden entry
 */
static bool sb_prev_n(struct SidebarWindowData *wdata, int count)
{
  int orig = wdata->hil_index;
  for (int i = 0; i < count; i++)
  {
    if (!sb_prev(wdata))
      break;
  }
  return (wdata->hil_index != orig);
}

/**
 * sb_next_new - Return the next mailbox with new messages
 * @param wdata Sidebar data
 * @param begin Starting index for searching
 * @param end   Ending index for searching
 * @retval ptr  Pointer to the first entry with new messages
 * @retval NULL None could be found
 */
static struct SbEntry **sb_next_new(struct SidebarWindowData *wdata, size_t begin, size_t end)
{
  struct SbEntry **sbep = NULL;
  ARRAY_FOREACH_FROM_TO(sbep, &wdata->entries, begin, end)
  {
    if ((*sbep)->mailbox->has_new || ((*sbep)->mailbox->msg_unread != 0))
      return sbep;
  }
  return NULL;
}

/**
 * sb_prev - Find the previous unhidden Mailbox
 * @param wdata Sidebar data
 * @retval true Mailbox found
 */
bool sb_prev(struct SidebarWindowData *wdata)
{
  struct SbEntry **sbep = NULL, **prev = NULL;
  ARRAY_FOREACH_TO(sbep, &wdata->entries, wdata->hil_index)
  {
    if (!(*sbep)->is_hidden)
      prev = sbep;
  }

  if (prev)
  {
    wdata->hil_index = ARRAY_IDX(&wdata->entries, prev);
    return true;
  }

  return false;
}

/**
 * sb_prev_new - Return the previous mailbox with new messages
 * @param wdata Sidebar data
 * @param begin Starting index for searching
 * @param end   Ending index for searching
 * @retval ptr  Pointer to the first entry with new messages
 * @retval NULL None could be found
 */
static struct SbEntry **sb_prev_new(struct SidebarWindowData *wdata, size_t begin, size_t end)
{
  struct SbEntry **sbep = NULL, **prev = NULL;
  ARRAY_FOREACH_FROM_TO(sbep, &wdata->entries, begin, end)
  {
    if ((*sbep)->mailbox->has_new || ((*sbep)->mailbox->msg_unread != 0))
      prev = sbep;
  }

  return prev;
}

// -----------------------------------------------------------------------------

/**
 * op_sidebar_first - Selects the first unhidden mailbox - Implements ::sidebar_function_t - @ingroup sidebar_function_api
 */
int op_sidebar_first(struct SidebarFunctionData *fdata, const struct KeyEvent *event)
{
  struct SidebarWindowData *wdata = fdata->wdata;
  if (!mutt_window_is_visible(wdata->win))
    return FR_NO_ACTION;

  if (ARRAY_EMPTY(&wdata->entries) || (wdata->hil_index < 0))
    return FR_NO_ACTION;

  int orig_hil_index = wdata->hil_index;

  wdata->hil_index = 0;
  if ((*ARRAY_GET(&wdata->entries, wdata->hil_index))->is_hidden)
    if (!sb_next(wdata))
      wdata->hil_index = orig_hil_index;

  if (orig_hil_index == wdata->hil_index)
    return FR_NO_ACTION;

  wdata->win->actions |= WA_RECALC;
  return FR_SUCCESS;
}

/**
 * op_sidebar_last - Selects the last unhidden mailbox - Implements ::sidebar_function_t - @ingroup sidebar_function_api
 */
int op_sidebar_last(struct SidebarFunctionData *fdata, const struct KeyEvent *event)
{
  struct SidebarWindowData *wdata = fdata->wdata;
  if (!mutt_window_is_visible(wdata->win))
    return FR_NO_ACTION;

  if (ARRAY_EMPTY(&wdata->entries) || (wdata->hil_index < 0))
    return FR_NO_ACTION;

  int orig_hil_index = wdata->hil_index;

  wdata->hil_index = ARRAY_SIZE(&wdata->entries);
  if (!sb_prev(wdata))
    wdata->hil_index = orig_hil_index;

  if (orig_hil_index == wdata->hil_index)
    return FR_NO_ACTION;

  wdata->win->actions |= WA_RECALC;
  return FR_SUCCESS;
}

/**
 * op_sidebar_next - Selects the next unhidden mailbox - Implements ::sidebar_function_t - @ingroup sidebar_function_api
 */
int op_sidebar_next(struct SidebarFunctionData *fdata, const struct KeyEvent *event)
{
  struct SidebarWindowData *wdata = fdata->wdata;
  if (!mutt_window_is_visible(wdata->win))
    return FR_NO_ACTION;

  if (ARRAY_EMPTY(&wdata->entries) || (wdata->hil_index < 0))
    return FR_NO_ACTION;

  const int count = event->count;
  if (count > 0)
  {
    if (!sb_next_n(wdata, count))
      return FR_NO_ACTION;
  }
  else
  {
    if (!sb_next(wdata))
    {
      mutt_message(_("You are on the last entry"));
      return FR_ERROR;
    }
  }

  wdata->win->actions |= WA_RECALC;
  return FR_SUCCESS;
}

/**
 * op_sidebar_next_new - Selects the next new mailbox - Implements ::sidebar_function_t - @ingroup sidebar_function_api
 *
 * Search down the list of mail folders for one containing new mail.
 */
static int op_sidebar_next_new(struct SidebarFunctionData *fdata, const struct KeyEvent *event)
{
  struct SidebarWindowData *wdata = fdata->wdata;
  if (!mutt_window_is_visible(wdata->win))
    return FR_NO_ACTION;

  const size_t max_entries = ARRAY_SIZE(&wdata->entries);
  if ((max_entries == 0) || (wdata->hil_index < 0))
    return FR_NO_ACTION;

  const int count = MAX(event->count, 1);
  const bool c_sidebar_next_new_wrap = cs_subset_bool(fdata->n->sub, "sidebar_next_new_wrap");
  int orig_hil_index = wdata->hil_index;

  for (int i = 0; i < count; i++)
  {
    struct SbEntry **sbep = NULL;
    if ((sbep = sb_next_new(wdata, wdata->hil_index + 1, max_entries)) ||
        (c_sidebar_next_new_wrap && (sbep = sb_next_new(wdata, 0, wdata->hil_index))))
    {
      wdata->hil_index = ARRAY_IDX(&wdata->entries, sbep);
    }
    else
    {
      break;
    }
  }

  if (wdata->hil_index == orig_hil_index)
    return FR_NO_ACTION;

  wdata->win->actions |= WA_RECALC;
  return FR_SUCCESS;
}

/**
 * op_sidebar_open - Open highlighted mailbox - Implements ::sidebar_function_t - @ingroup sidebar_function_api
 */
int op_sidebar_open(struct SidebarFunctionData *fdata, const struct KeyEvent *event)
{
  struct SidebarWindowData *wdata = fdata->wdata;
  struct MuttWindow *win_sidebar = wdata->win;
  if (!mutt_window_is_visible(win_sidebar))
    return FR_NO_ACTION;

  struct MuttWindow *dlg = dialog_find(win_sidebar);
  index_change_folder(dlg, sb_get_highlight(win_sidebar));
  return FR_SUCCESS;
}

/**
 * op_sidebar_page_down - Selects the first entry in the next page of mailboxes - Implements ::sidebar_function_t - @ingroup sidebar_function_api
 */
int op_sidebar_page_down(struct SidebarFunctionData *fdata, const struct KeyEvent *event)
{
  struct SidebarWindowData *wdata = fdata->wdata;
  if (!mutt_window_is_visible(wdata->win))
    return FR_NO_ACTION;

  if (ARRAY_EMPTY(&wdata->entries) || (wdata->bot_index < 0))
    return FR_NO_ACTION;

  int orig_hil_index = wdata->hil_index;
  const int page_size = wdata->win->state.rows;
  const int count = MAX(event->count, 1);

  if (!sb_next_n(wdata, count * page_size))
    return FR_NO_ACTION;

  /* If we landed on a hidden entry, go up to the last unhidden one */
  if ((*ARRAY_GET(&wdata->entries, wdata->hil_index))->is_hidden)
    sb_prev(wdata);

  if (orig_hil_index == wdata->hil_index)
    return FR_NO_ACTION;

  wdata->repage = true;
  wdata->win->actions |= WA_RECALC;
  return FR_SUCCESS;
}

/**
 * op_sidebar_page_up - Selects the last entry in the previous page of mailboxes - Implements ::sidebar_function_t - @ingroup sidebar_function_api
 */
int op_sidebar_page_up(struct SidebarFunctionData *fdata, const struct KeyEvent *event)
{
  struct SidebarWindowData *wdata = fdata->wdata;
  if (!mutt_window_is_visible(wdata->win))
    return FR_NO_ACTION;

  if (ARRAY_EMPTY(&wdata->entries) || (wdata->top_index < 0))
    return FR_NO_ACTION;

  int orig_hil_index = wdata->hil_index;
  const int page_size = wdata->win->state.rows;
  const int count = MAX(event->count, 1);

  if (!sb_prev_n(wdata, count * page_size))
    return FR_NO_ACTION;

  /* If we landed on a hidden entry, go down to the last unhidden one */
  if ((*ARRAY_GET(&wdata->entries, wdata->hil_index))->is_hidden)
    sb_next(wdata);

  if (orig_hil_index == wdata->hil_index)
    return FR_NO_ACTION;

  wdata->repage = true;
  wdata->win->actions |= WA_RECALC;
  return FR_SUCCESS;
}

/**
 * op_sidebar_prev - Selects the previous unhidden mailbox - Implements ::sidebar_function_t - @ingroup sidebar_function_api
 */
int op_sidebar_prev(struct SidebarFunctionData *fdata, const struct KeyEvent *event)
{
  struct SidebarWindowData *wdata = fdata->wdata;
  if (!mutt_window_is_visible(wdata->win))
    return FR_NO_ACTION;

  if (ARRAY_EMPTY(&wdata->entries) || (wdata->hil_index < 0))
    return FR_NO_ACTION;

  const int count = event->count;
  if (count > 0)
  {
    if (!sb_prev_n(wdata, count))
      return FR_NO_ACTION;
  }
  else
  {
    if (!sb_prev(wdata))
    {
      mutt_message(_("You are on the first entry"));
      return FR_ERROR;
    }
  }

  wdata->win->actions |= WA_RECALC;
  return FR_SUCCESS;
}

/**
 * op_sidebar_prev_new - Selects the previous new mailbox - Implements ::sidebar_function_t - @ingroup sidebar_function_api
 *
 * Search up the list of mail folders for one containing new mail.
 */
static int op_sidebar_prev_new(struct SidebarFunctionData *fdata, const struct KeyEvent *event)
{
  struct SidebarWindowData *wdata = fdata->wdata;
  if (!mutt_window_is_visible(wdata->win))
    return FR_NO_ACTION;

  const size_t max_entries = ARRAY_SIZE(&wdata->entries);
  if ((max_entries == 0) || (wdata->hil_index < 0))
    return FR_NO_ACTION;

  const int count = MAX(event->count, 1);
  const bool c_sidebar_next_new_wrap = cs_subset_bool(fdata->n->sub, "sidebar_next_new_wrap");
  int orig_hil_index = wdata->hil_index;

  for (int i = 0; i < count; i++)
  {
    struct SbEntry **sbep = NULL;
    if ((sbep = sb_prev_new(wdata, 0, wdata->hil_index)) ||
        (c_sidebar_next_new_wrap &&
         (sbep = sb_prev_new(wdata, wdata->hil_index + 1, max_entries))))
    {
      wdata->hil_index = ARRAY_IDX(&wdata->entries, sbep);
    }
    else
    {
      break;
    }
  }

  if (wdata->hil_index == orig_hil_index)
    return FR_NO_ACTION;

  wdata->win->actions |= WA_RECALC;
  return FR_SUCCESS;
}

/**
 * op_sidebar_toggle_visible - Make the sidebar (in)visible - Implements ::sidebar_function_t - @ingroup sidebar_function_api
 */
static int op_sidebar_toggle_visible(struct SidebarFunctionData *fdata,
                                     const struct KeyEvent *event)
{
  bool_str_toggle(fdata->n->sub, "sidebar_visible", NULL);
  mutt_window_reflow(NULL);
  return FR_SUCCESS;
}

/**
 * op_sidebar_toggle_virtual - Deprecated - Implements ::sidebar_function_t - @ingroup sidebar_function_api
 */
static int op_sidebar_toggle_virtual(struct SidebarFunctionData *fdata,
                                     const struct KeyEvent *event)
{
  return FR_SUCCESS;
}

// -----------------------------------------------------------------------------

/**
 * SidebarFunctions - All the NeoMutt functions that the Sidebar supports
 */
static const struct SidebarFunction SidebarFunctions[] = {
  // clang-format off
  { OP_SIDEBAR_FIRST,          op_sidebar_first },
  { OP_SIDEBAR_LAST,           op_sidebar_last },
  { OP_SIDEBAR_NEXT,           op_sidebar_next },
  { OP_SIDEBAR_NEXT_NEW,       op_sidebar_next_new },
  { OP_SIDEBAR_OPEN,           op_sidebar_open },
  { OP_SIDEBAR_PAGE_DOWN,      op_sidebar_page_down },
  { OP_SIDEBAR_PAGE_UP,        op_sidebar_page_up },
  { OP_SIDEBAR_PREV,           op_sidebar_prev },
  { OP_SIDEBAR_PREV_NEW,       op_sidebar_prev_new },
  { OP_SIDEBAR_START_SEARCH,   op_sidebar_start_search },
  { OP_SIDEBAR_TOGGLE_VIRTUAL, op_sidebar_toggle_virtual },
  { OP_SIDEBAR_TOGGLE_VISIBLE, op_sidebar_toggle_visible },
  { 0, NULL },
  // clang-format on
};

/**
 * sb_function_dispatcher - Perform a Sidebar function - Implements ::function_dispatcher_t - @ingroup dispatcher_api
 */
int sb_function_dispatcher(struct MuttWindow *win, const struct KeyEvent *event)
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
  for (size_t i = 0; SidebarFunctions[i].op != OP_NULL; i++)
  {
    const struct SidebarFunction *fn = &SidebarFunctions[i];
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
