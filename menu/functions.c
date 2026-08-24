/**
 * @file
 * Menu functions
 *
 * @authors
 * Copyright (C) 2022 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2022-2023 Richard Russon <rich@flatcap.org>
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
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "functions.h"
#include "lib.h"
#include "editor/lib.h"
#include "history/lib.h"
#include "index/lib.h"
#include "key/lib.h"
#include "pager/lib.h"
#include "module_data.h"
#include "mutt_logging.h"
#include "pager/module_data.h"
#include "pager/private_data.h"
#include "type.h"

#define MUTT_SEARCH_UP 1
#define MUTT_SEARCH_DOWN 2

/**
 * search - Search a menu
 * @param menu Menu to search
 * @param op   Search operation, e.g. OP_SEARCH_NEXT
 * @param[out] match Index of matching item, or -1 on failure
 * @retval FR_SUCCESS   Match found
 * @retval FR_NO_ACTION Search was cancelled
 * @retval FR_ERROR     Search failed
 */
static int search(struct Menu *menu, int op, int *match)
{
  struct MenuModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_MENU);
  int reg_rc = 0;
  int wrap = 0;
  int search_dir;
  int rc = FR_ERROR;
  regex_t re = { 0 };
  struct Buffer *buf = buf_pool_get();

  *match = -1;

  char *search_buf = (menu->md && (menu->md->id < MENU_MAX)) ?
                         mod_data->search_buffers[menu->md->id] :
                         NULL;

  if (!(search_buf && *search_buf) || ((op != OP_SEARCH_NEXT) && (op != OP_SEARCH_OPPOSITE)))
  {
    buf_strcpy(buf, search_buf && (search_buf[0] != '\0') ? search_buf : "");
    if ((mw_get_field(((op == OP_SEARCH) || (op == OP_SEARCH_NEXT)) ? _("Search for: ") : _("Reverse search for: "),
                      buf, MUTT_COMP_CLEAR, HC_OTHER, NULL, NULL) != 0) ||
        buf_is_empty(buf))
    {
      rc = FR_NO_ACTION;
      goto done;
    }
    if (menu->md && (menu->md->id < MENU_MAX))
    {
      mutt_str_replace(&mod_data->search_buffers[menu->md->id], buf_string(buf));
      search_buf = mod_data->search_buffers[menu->md->id];
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
    reg_rc = REG_COMP(&re, search_buf, REG_NOSUB | flags);
  }

  if (reg_rc != 0)
  {
    regerror(reg_rc, &re, buf->data, buf->dsize);
    mutt_error("%s", buf_string(buf));
    goto done;
  }

  *match = menu->current + search_dir;
search_next:
  if (wrap)
    mutt_message(_("Search wrapped to top"));
  while ((*match >= 0) && (*match < menu->max))
  {
    if (menu->search(menu, &re, *match) == 0)
    {
      regfree(&re);
      rc = FR_SUCCESS;
      goto done;
    }

    *match += search_dir;
  }

  const bool c_wrap_search = cs_subset_bool(menu->sub, "wrap_search");
  if (c_wrap_search && (wrap++ == 0))
  {
    *match = (search_dir == 1) ? 0 : menu->max - 1;
    goto search_next;
  }
  regfree(&re);
  mutt_error(_("Not found"));

done:
  buf_pool_release(&buf);
  return rc;
}

// -----------------------------------------------------------------------------

/**
 * menu_movement - Handle all the common Menu movements - Implements ::menu_function_t - @ingroup menu_function_api
 *
 * This function handles:
 * - OP_BOTTOM_PAGE
 * - OP_CURRENT_BOTTOM
 * - OP_CURRENT_MIDDLE
 * - OP_CURRENT_TOP
 * - OP_FIRST_ENTRY
 * - OP_HALF_DOWN
 * - OP_HALF_UP
 * - OP_LAST_ENTRY
 * - OP_MIDDLE_PAGE
 * - OP_NEXT_ENTRY
 * - OP_NEXT_LINE
 * - OP_NEXT_PAGE
 * - OP_PREV_ENTRY
 * - OP_PREV_LINE
 * - OP_PREV_PAGE
 * - OP_TOP_PAGE
 */
static int menu_movement(struct MenuFunctionData *fdata, const struct KeyEvent *event)
{
  struct Menu *menu = fdata->menu;
  const int old_top = menu->top;
  const int old_current = menu->current;
  MenuRedrawFlags flags = MENU_REDRAW_NONE;

  const int count = event->count;
  switch (event->op)
  {
    case OP_BOTTOM_PAGE:
      flags = menu_bottom_page(menu);
      return (menu->max == 0) ? FR_ERROR : FR_SUCCESS;

    case OP_CURRENT_BOTTOM:
      flags = menu_current_bottom(menu);
      return (menu->max == 0) ? FR_ERROR : FR_SUCCESS;

    case OP_CURRENT_MIDDLE:
      flags = menu_current_middle(menu);
      return (menu->max == 0) ? FR_ERROR : FR_SUCCESS;

    case OP_CURRENT_TOP:
      flags = menu_current_top(menu);
      return (menu->max == 0) ? FR_ERROR : FR_SUCCESS;

    case OP_FIRST_ENTRY:
      flags = menu_first_entry(menu, count);
      return (menu->max == 0) ? FR_ERROR : FR_SUCCESS;

    case OP_HALF_DOWN:
      menu_half_down(menu, count);
      return FR_SUCCESS;

    case OP_HALF_UP:
      menu_half_up(menu, count);
      return FR_SUCCESS;

    case OP_LAST_ENTRY:
      flags = menu_last_entry(menu, count);
      return (menu->max == 0) ? FR_ERROR : FR_SUCCESS;

    case OP_MIDDLE_PAGE:
      flags = menu_middle_page(menu);
      return (menu->max == 0) ? FR_ERROR : FR_SUCCESS;

    case OP_NEXT_ENTRY:
      flags = menu_next_entry(menu, count);
      return ((flags == MENU_REDRAW_NONE) && (menu->top == old_top) &&
              (menu->current == old_current)) ?
                 FR_ERROR :
                 FR_SUCCESS;

    case OP_NEXT_LINE:
      flags = menu_next_line(menu, count);
      return ((flags == MENU_REDRAW_NONE) && (menu->top == old_top) &&
              (menu->current == old_current)) ?
                 FR_ERROR :
                 FR_SUCCESS;

    case OP_NEXT_PAGE:
      menu_next_page(menu, count);
      return FR_SUCCESS;

    case OP_PREV_ENTRY:
      flags = menu_prev_entry(menu, count);
      return ((flags == MENU_REDRAW_NONE) && (menu->top == old_top) &&
              (menu->current == old_current)) ?
                 FR_ERROR :
                 FR_SUCCESS;

    case OP_PREV_LINE:
      flags = menu_prev_line(menu, count);
      return ((flags == MENU_REDRAW_NONE) && (menu->top == old_top) &&
              (menu->current == old_current)) ?
                 FR_ERROR :
                 FR_SUCCESS;

    case OP_PREV_PAGE:
      menu_prev_page(menu, count);
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
 *
 * This function handles:
 * - OP_SEARCH
 * - OP_SEARCH_NEXT
 * - OP_SEARCH_OPPOSITE
 * - OP_SEARCH_REVERSE
 */
static int menu_search(struct MenuFunctionData *fdata, const struct KeyEvent *event)
{
  struct Menu *menu = fdata->menu;
  if (menu->search)
  {
    int index = -1;
    const int rc = search(menu, event->op, &index);
    if ((rc == FR_SUCCESS) && (index != -1))
      menu_set_index(menu, index);
    return rc;
  }
  return FR_SUCCESS;
}

/**
 * struct SampleTab - Sample pager tab state
 */
struct SampleTab
{
  const char *name;
  struct IndexSharedData *shared;
  struct Buffer *tempfile;
  struct PagerData pdata;
  struct PagerView pview;
  struct MuttWindow *panel;
};

/**
 * sample_data_file_new - Create sample data for a tab
 * @param name Name of the tab
 * @retval ptr Sample file path
 */
static struct Buffer *sample_data_file_new(const char *name)
{
  struct Buffer *tempfile = buf_pool_get();
  buf_mktemp(tempfile);

  FILE *fp = mutt_file_fopen(buf_string(tempfile), "w");
  if (!fp)
  {
    mutt_error(_("Could not create temporary file %s"), buf_string(tempfile));
    buf_pool_release(&tempfile);
    return NULL;
  }

  fprintf(fp, "%s\n\n", name);
  for (int i = 1; i <= 100; i++)
    fprintf(fp, "%s sample line %d\n", name, i);

  mutt_file_fclose(&fp);
  return tempfile;
}

/**
 * sample_tab_free - Release a sample tab
 * @param tab Tab state
 */
static void sample_tab_free(struct SampleTab *tab)
{
  if (!tab)
    return;

  pager_cleanup(&tab->pview);
  buf_pool_release(&tab->tempfile);
}

/**
 * op_help - Show the help screen - Implements ::menu_function_t - @ingroup menu_function_api
 */
static int op_help(struct MenuFunctionData *fdata, const struct KeyEvent *event)
{
  struct PagerModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_PAGER);
  ASSERT(mod_data);

  struct Menu *menu = fdata->menu;

  static const char *const names[] = { "Apple",  "Banana", "Cherry",
                                       "Damson", "Endive", "Fig" };
  enum
  {
    SAMPLE_TAB_COUNT = sizeof(names) / sizeof(names[0]),
  };
  struct MuttWindow *tabbed = tabwin_new();
  dialog_push(tabbed);

  struct SampleTab tabs[SAMPLE_TAB_COUNT] = { 0 };
  bool setup_ok = true;
  for (size_t i = 0; i < SAMPLE_TAB_COUNT; i++)
  {
    tabs[i].name = names[i];
    tabs[i].shared = index_shared_data_new();
    if (!tabs[i].shared)
    {
      setup_ok = false;
      break;
    }

    tabs[i].tempfile = sample_data_file_new(names[i]);
    if (!tabs[i].tempfile)
    {
      setup_ok = false;
      break;
    }

    tabs[i].panel = ppanel_new(false, tabs[i].shared);
    tabs[i].pdata.fname = buf_string(tabs[i].tempfile);
    tabs[i].pview.pdata = &tabs[i].pdata;
    tabs[i].pview.mode = PAGER_MODE_OTHER;
    tabs[i].pview.flags = MUTT_PAGER_NOWRAP | MUTT_PAGER_STRIPES;
    tabs[i].pview.banner = tabs[i].name;
    tabs[i].pview.win_index = NULL;
    tabs[i].pview.win_pbar = window_find_child(tabs[i].panel, WT_STATUS_BAR);
    tabs[i].pview.win_pager = window_find_child(tabs[i].panel, WT_CUSTOM);

    tabwin_add_child(tabbed, tabs[i].name, tabs[i].panel);
    if (pager_setup(&tabs[i].pview) != 0)
    {
      setup_ok = false;
      break;
    }
  }

  if (!setup_ok)
  {
    for (size_t i = 0; i < SAMPLE_TAB_COUNT; i++)
      sample_tab_free(&tabs[i]);
    dialog_pop();
    mutt_window_free(&tabbed);
    for (size_t i = 0; i < SAMPLE_TAB_COUNT; i++)
    {
      if (tabs[i].shared)
      {
        void *ptr = tabs[i].shared;
        index_shared_data_free(NULL, &ptr);
      }
    }
    return FR_ERROR;
  }

  struct MuttWindow *focus = tabs[0].pview.win_pager;
  struct MuttWindow *old_focus = window_set_focus(focus);

  int rc = 0;
  int op = OP_NULL;
  do
  {
    window_redraw(NULL);

    struct KeyEvent event2 = km_dokey(mod_data->md_pager, GETCH_NONE);
    op = event2.op;

    mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", opcodes_get_name(op), op);
    if (op < 0)
      continue;
    if (op == OP_NULL)
    {
      km_error_key(mod_data->md_pager);
      continue;
    }
    mutt_clear_error();

    focus = window_get_focus();
    if (!focus)
      focus = tabs[0].pview.win_pager;
    rc = pager_function_dispatcher(focus, &event2);
    if (rc == FR_UNKNOWN)
      rc = tabbed_function_dispatcher(tabbed, &event2);
    if (rc == FR_UNKNOWN)
      rc = global_function_dispatcher(tabbed, &event2);
  } while (rc != FR_DONE);

  for (size_t i = 0; i < SAMPLE_TAB_COUNT; i++)
    sample_tab_free(&tabs[i]);

  window_set_focus(old_focus);
  dialog_pop();
  mutt_window_free(&tabbed);

  for (size_t i = 0; i < SAMPLE_TAB_COUNT; i++)
  {
    if (tabs[i].shared)
    {
      void *ptr = tabs[i].shared;
      index_shared_data_free(NULL, &ptr);
    }
  }

  menu->redraw = MENU_REDRAW_FULL;
  return FR_SUCCESS;
}

/**
 * op_jump - Jump to an index number - Implements ::menu_function_t - @ingroup menu_function_api
 */
static int op_jump(struct MenuFunctionData *fdata, const struct KeyEvent *event)
{
  struct Menu *menu = fdata->menu;
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return FR_ERROR;
  }

  struct Buffer *buf = buf_pool_get();
  int rc = FR_ERROR;

  int num = event->count;
  if (num == 0)
  {
    if ((mw_get_field(_("Jump to: "), buf, MUTT_COMP_NONE, HC_OTHER, NULL, NULL) != 0) ||
        buf_is_empty(buf))
    {
      rc = FR_NO_ACTION;
      goto done;
    }

    if (!mutt_str_atoi_full(buf_string(buf), &num) || (num < 1) || (num > menu->max))
    {
      mutt_error(_("Invalid index number"));
      goto done;
    }
  }

  menu_set_index(menu, num - 1); // msg numbers are 0-based
  rc = FR_SUCCESS;

done:
  buf_pool_release(&buf);
  return rc;
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
int menu_function_dispatcher(struct MuttWindow *win, const struct KeyEvent *event)
{
  if (!event || !win || !win->wdata)
    return FR_UNKNOWN;

  const int op = event->op;
  struct Menu *menu = win->wdata;

  struct MenuFunctionData fdata = {
    .n = NeoMutt,
    .menu = menu,
  };

  int rc = FR_UNKNOWN;
  for (size_t i = 0; MenuFunctions[i].op != OP_NULL; i++)
  {
    const struct MenuFunction *fn = &MenuFunctions[i];
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
