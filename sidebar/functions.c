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
 * @page sidebar_functions Sidebar functions
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
#include "functions.h"
#include "lib.h"
#include "editor/lib.h"
#include "fuzzy/lib.h"
#include "index/lib.h"
#include "key/lib.h"
#include "menu/lib.h"

/// Sidebar functions
static struct SubMenu *SmSidebar = NULL;

// clang-format off
/**
 * OpSidebar - Functions for the Sidebar Window
 */
static const struct MenuFuncOp OpSidebar[] = { /* map: sidebar */
  { "sidebar-first",          OP_SIDEBAR_FIRST          },
  { "sidebar-last",           OP_SIDEBAR_LAST           },
  { "sidebar-next",           OP_SIDEBAR_NEXT           },
  { "sidebar-next-new",       OP_SIDEBAR_NEXT_NEW       },
  { "sidebar-open",           OP_SIDEBAR_OPEN           },
  { "sidebar-page-down",      OP_SIDEBAR_PAGE_DOWN      },
  { "sidebar-page-up",        OP_SIDEBAR_PAGE_UP        },
  { "sidebar-prev",           OP_SIDEBAR_PREV           },
  { "sidebar-prev-new",       OP_SIDEBAR_PREV_NEW       },
  { "sidebar-start-search",   OP_SIDEBAR_START_SEARCH   },
  { "sidebar-toggle-virtual", OP_SIDEBAR_TOGGLE_VIRTUAL },
  { "sidebar-toggle-visible", OP_SIDEBAR_TOGGLE_VISIBLE },
  { NULL, 0 },
};

/**
 * SidebarDefaultBindings - Key bindings for the Sidebar Window
 */
const struct MenuOpSeq SidebarDefaultBindings[] = {
  { OP_EDITOR_BACKSPACE, "<backspace>" },
  { OP_SIDEBAR_NEXT,     "<down>"      },
  { OP_SIDEBAR_PREV,     "<up>"        },
  { 0, NULL },
};
// clang-format on

/**
 * sidebar_init_keys - Initialise the Sidebar Keybindings - Implements ::init_keys_api
 */
void sidebar_init_keys(struct SubMenu *sm_generic)
{
  struct MenuDefinition *md = NULL;
  struct SubMenu *sm = NULL;

  sm = km_register_submenu(OpSidebar);
  md = km_register_menu(MENU_SIDEBAR, "sidebar");
  km_menu_add_submenu(md, sm);
  km_menu_add_bindings(md, SidebarDefaultBindings);

  SmSidebar = sm;
}

/**
 * sidebar_get_submenu - Get the Sidebar SubMenu
 * @retval ptr Sidebar SubMenu
 */
struct SubMenu *sidebar_get_submenu(void)
{
  return SmSidebar;
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
static int op_sidebar_first(struct SidebarWindowData *wdata, const struct KeyEvent *event)
{
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
static int op_sidebar_last(struct SidebarWindowData *wdata, const struct KeyEvent *event)
{
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
static int op_sidebar_next(struct SidebarWindowData *wdata, const struct KeyEvent *event)
{
  if (!mutt_window_is_visible(wdata->win))
    return FR_NO_ACTION;

  if (ARRAY_EMPTY(&wdata->entries) || (wdata->hil_index < 0))
    return FR_NO_ACTION;

  if (!sb_next(wdata))
    return FR_NO_ACTION;

  wdata->win->actions |= WA_RECALC;
  return FR_SUCCESS;
}

/**
 * op_sidebar_next_new - Selects the next new mailbox - Implements ::sidebar_function_t - @ingroup sidebar_function_api
 *
 * Search down the list of mail folders for one containing new mail.
 */
static int op_sidebar_next_new(struct SidebarWindowData *wdata, const struct KeyEvent *event)
{
  if (!mutt_window_is_visible(wdata->win))
    return FR_NO_ACTION;

  const size_t max_entries = ARRAY_SIZE(&wdata->entries);
  if ((max_entries == 0) || (wdata->hil_index < 0))
    return FR_NO_ACTION;

  const bool c_sidebar_next_new_wrap = cs_subset_bool(NeoMutt->sub, "sidebar_next_new_wrap");
  struct SbEntry **sbep = NULL;
  if ((sbep = sb_next_new(wdata, wdata->hil_index + 1, max_entries)) ||
      (c_sidebar_next_new_wrap && (sbep = sb_next_new(wdata, 0, wdata->hil_index))))
  {
    wdata->hil_index = ARRAY_IDX(&wdata->entries, sbep);
    wdata->win->actions |= WA_RECALC;
    return FR_SUCCESS;
  }

  return FR_NO_ACTION;
}

/**
 * op_sidebar_open - Open highlighted mailbox - Implements ::sidebar_function_t - @ingroup sidebar_function_api
 */
static int op_sidebar_open(struct SidebarWindowData *wdata, const struct KeyEvent *event)
{
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
static int op_sidebar_page_down(struct SidebarWindowData *wdata, const struct KeyEvent *event)
{
  if (!mutt_window_is_visible(wdata->win))
    return FR_NO_ACTION;

  if (ARRAY_EMPTY(&wdata->entries) || (wdata->bot_index < 0))
    return FR_NO_ACTION;

  int orig_hil_index = wdata->hil_index;

  wdata->hil_index = wdata->bot_index;
  sb_next(wdata);
  /* If the rest of the entries are hidden, go up to the last unhidden one */
  if ((*ARRAY_GET(&wdata->entries, wdata->hil_index))->is_hidden)
    sb_prev(wdata);

  if (orig_hil_index == wdata->hil_index)
    return FR_NO_ACTION;

  wdata->win->actions |= WA_RECALC;
  return FR_SUCCESS;
}

/**
 * op_sidebar_page_up - Selects the last entry in the previous page of mailboxes - Implements ::sidebar_function_t - @ingroup sidebar_function_api
 */
static int op_sidebar_page_up(struct SidebarWindowData *wdata, const struct KeyEvent *event)
{
  if (!mutt_window_is_visible(wdata->win))
    return FR_NO_ACTION;

  if (ARRAY_EMPTY(&wdata->entries) || (wdata->top_index < 0))
    return FR_NO_ACTION;

  int orig_hil_index = wdata->hil_index;

  wdata->hil_index = wdata->top_index;
  sb_prev(wdata);
  /* If the rest of the entries are hidden, go down to the last unhidden one */
  if ((*ARRAY_GET(&wdata->entries, wdata->hil_index))->is_hidden)
    sb_next(wdata);

  if (orig_hil_index == wdata->hil_index)
    return FR_NO_ACTION;

  wdata->win->actions |= WA_RECALC;
  return FR_SUCCESS;
}

/**
 * op_sidebar_prev - Selects the previous unhidden mailbox - Implements ::sidebar_function_t - @ingroup sidebar_function_api
 */
static int op_sidebar_prev(struct SidebarWindowData *wdata, const struct KeyEvent *event)
{
  if (!mutt_window_is_visible(wdata->win))
    return FR_NO_ACTION;

  if (ARRAY_EMPTY(&wdata->entries) || (wdata->hil_index < 0))
    return FR_NO_ACTION;

  if (!sb_prev(wdata))
    return FR_NO_ACTION;

  wdata->win->actions |= WA_RECALC;
  return FR_SUCCESS;
}

/**
 * op_sidebar_prev_new - Selects the previous new mailbox - Implements ::sidebar_function_t - @ingroup sidebar_function_api
 *
 * Search up the list of mail folders for one containing new mail.
 */
static int op_sidebar_prev_new(struct SidebarWindowData *wdata, const struct KeyEvent *event)
{
  if (!mutt_window_is_visible(wdata->win))
    return FR_NO_ACTION;

  const size_t max_entries = ARRAY_SIZE(&wdata->entries);
  if ((max_entries == 0) || (wdata->hil_index < 0))
    return FR_NO_ACTION;

  const bool c_sidebar_next_new_wrap = cs_subset_bool(NeoMutt->sub, "sidebar_next_new_wrap");
  struct SbEntry **sbep = NULL;
  if ((sbep = sb_prev_new(wdata, 0, wdata->hil_index)) ||
      (c_sidebar_next_new_wrap &&
       (sbep = sb_prev_new(wdata, wdata->hil_index + 1, max_entries))))
  {
    wdata->hil_index = ARRAY_IDX(&wdata->entries, sbep);
    wdata->win->actions |= WA_RECALC;
    return FR_SUCCESS;
  }

  return FR_NO_ACTION;
}

/**
 * op_sidebar_toggle_visible - Make the sidebar (in)visible - Implements ::sidebar_function_t - @ingroup sidebar_function_api
 */
static int op_sidebar_toggle_visible(struct SidebarWindowData *wdata,
                                     const struct KeyEvent *event)
{
  bool_str_toggle(NeoMutt->sub, "sidebar_visible", NULL);
  mutt_window_reflow(NULL);
  return FR_SUCCESS;
}

/**
 * op_sidebar_toggle_virtual - Deprecated - Implements ::sidebar_function_t - @ingroup sidebar_function_api
 */
static int op_sidebar_toggle_virtual(struct SidebarWindowData *wdata,
                                     const struct KeyEvent *event)
{
  return FR_SUCCESS;
}

/**
 * sidebar_matcher_cb - Callback to filter sidebar entries by pattern
 * @param data    Sidebar window
 * @param pattern Search pattern to match against mailbox names
 */
static void sidebar_matcher_cb(void *data, const char *pattern)
{
  struct MuttWindow *win = data;
  struct SidebarWindowData *wdata = win->wdata;
  wdata->hil_index = -1;
  wdata->repage = true;

  struct SbEntry **sbep = NULL;

  if (!pattern || (pattern[0] == '\0'))
  {
    ARRAY_FOREACH(sbep, &wdata->entries)
    {
      struct SbEntry *sbe = *sbep;
      sbe->mailbox->visible = true;
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
    int score = fuzzy_match(pattern, buf_string(buf), FUZZY_ALGO_SUBSEQ, &opts, &result);
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

  if (best_index != -1)
    wdata->hil_index = best_index;

  wdata->win->actions |= WA_RECALC;
  buf_pool_release(&buf);
}

/**
 * op_sidebar_start_search - Selects the last unhidden mailbox - Implements ::sidebar_function_t - @ingroup sidebar_function_api
 */
static int op_sidebar_start_search(struct SidebarWindowData *wdata, const struct KeyEvent *event)
{
  const bool was_visible = cs_subset_bool(NeoMutt->sub, "sidebar_visible");
  if (!was_visible)
  {
    cs_subset_str_native_set(NeoMutt->sub, "sidebar_visible", true, NULL);
    mutt_window_reflow(NULL);
  }

  struct Buffer *buf = buf_pool_get();
  buf_alloc(buf, 128);
  int orghlidx = wdata->hil_index;

  struct SbEntry **sbep = NULL;
  ARRAY_FOREACH(sbep, &wdata->entries)
  {
    struct SbEntry *sbe = *sbep;
    if (sbe->box[0] == '\0')
      sb_entry_set_display_name(sbe);
  }

  int rc = FR_NO_ACTION;
  if (mw_get_field_notify(_("Sidebar search: "), buf, sidebar_matcher_cb, wdata->win) != 0)
  {
    wdata->hil_index = orghlidx;
    goto done;
  }

  if (!buf || buf_is_empty(buf) || (wdata->hil_index == -1))
  {
    wdata->hil_index = orghlidx;
    goto done;
  }

  rc = FR_SUCCESS;

done:
  ARRAY_FOREACH(sbep, &wdata->entries)
  (*sbep)->mailbox->visible = true;
  wdata->repage = false;
  wdata->win->actions |= WA_RECALC;

  if (rc == FR_SUCCESS)
  {
    struct MuttWindow *dlg = dialog_find(wdata->win);
    index_change_folder(dlg, sb_get_highlight(wdata->win));
  }

  if (!was_visible)
  {
    cs_subset_str_native_set(NeoMutt->sub, "sidebar_visible", false, NULL);
    mutt_window_reflow(NULL);
  }

  buf_pool_release(&buf);
  return rc;
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
  { OP_SIDEBAR_TOGGLE_VIRTUAL, op_sidebar_toggle_virtual },
  { OP_SIDEBAR_TOGGLE_VISIBLE, op_sidebar_toggle_visible },
  { OP_SIDEBAR_START_SEARCH,   op_sidebar_start_search },
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
  struct SidebarWindowData *wdata = win->wdata;
  int rc = FR_UNKNOWN;
  for (size_t i = 0; SidebarFunctions[i].op != OP_NULL; i++)
  {
    const struct SidebarFunction *fn = &SidebarFunctions[i];
    if (fn->op == op)
    {
      rc = fn->function(wdata, event);
      break;
    }
  }

  if (rc == FR_UNKNOWN) // Not our function
    return rc;

  const char *result = dispatcher_get_retval_name(rc);
  mutt_debug(LL_DEBUG1, "Handled %s (%d) -> %s\n", opcodes_get_name(op), op, NONULL(result));

  return FR_SUCCESS; // Whatever the outcome, we handled it
}
