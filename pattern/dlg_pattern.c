/**
 * @file
 * Pattern Selection Dialog
 *
 * @authors
 * Copyright (C) 1996-2000,2006-2007,2010 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
 * @page pattern_dlg_pattern Pattern Selection Dialog
 *
 * The Pattern Selection Dialog lets the user select a pattern.
 *
 * This is a @ref gui_simple
 *
 * ## Windows
 *
 * | Name                     | Type           | See Also      |
 * | :----------------------- | :------------- | :------------ |
 * | Pattern Selection Dialog | WT_DLG_PATTERN | dlg_pattern() |
 *
 * **Parent**
 * - @ref gui_dialog
 *
 * **Children**
 * - See: @ref gui_simple
 *
 * ## Data
 * - #Menu
 * - #Menu::mdata
 * - #PatternEntry
 *
 * The @ref gui_simple holds a Menu.  The Pattern Selection Dialog stores its
 * data (#PatternEntry) in Menu::mdata.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type  | Handler                   |
 * | :---------- | :------------------------ |
 * | #NT_CONFIG  | pattern_config_observer() |
 * | #NT_WINDOW  | pattern_window_observer() |
 *
 * The Pattern Selection Dialog doesn't have any specific colours, so it
 * doesn't need to support #NT_COLOR.
 *
 * The Pattern Selection Dialog does not implement MuttWindow::recalc() or
 * MuttWindow::repaint().
 *
 * Some other events are handled by the @ref gui_simple.
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "format_flags.h"
#include "functions.h"
#include "mutt_logging.h"
#include "muttlib.h"

/// Help Bar for the Pattern selection dialog
static const struct Mapping PatternHelp[] = {
  // clang-format off
  { N_("Exit"),   OP_EXIT },
  { N_("Select"), OP_GENERIC_SELECT_ENTRY },
  { N_("Help"),   OP_HELP },
  { NULL, 0 },
  // clang-format on
};

/**
 * pattern_format_str - Format a string for the pattern completion menu - Implements ::format_t - @ingroup expando_api
 *
 * | Expando | Description
 * | :------ | :---------------------
 * | \%d     | Pattern description
 * | \%e     | Pattern expression
 * | \%n     | Index number
 */
static const char *pattern_format_str(char *buf, size_t buflen, size_t col, int cols,
                                      char op, const char *src, const char *prec,
                                      const char *if_str, const char *else_str,
                                      intptr_t data, MuttFormatFlags flags)
{
  struct PatternEntry *entry = (struct PatternEntry *) data;

  switch (op)
  {
    case 'd':
      mutt_format_s(buf, buflen, prec, NONULL(entry->desc));
      break;
    case 'e':
      mutt_format_s(buf, buflen, prec, NONULL(entry->expr));
      break;
    case 'n':
    {
      char tmp[32] = { 0 };
      snprintf(tmp, sizeof(tmp), "%%%sd", prec);
      snprintf(buf, buflen, tmp, entry->num);
      break;
    }
  }

  return src;
}

/**
 * make_pattern_entry - Create a Pattern for the Menu - Implements Menu::make_entry() - @ingroup menu_make_entry
 *
 * @sa $pattern_format, pattern_format_str()
 */
static void make_pattern_entry(struct Menu *menu, char *buf, size_t buflen, int num)
{
  struct PatternEntry *entry = &((struct PatternEntry *) menu->mdata)[num];

  const char *const c_pattern_format = cs_subset_string(NeoMutt->sub, "pattern_format");
  mutt_expando_format(buf, buflen, 0, menu->win->state.cols, NONULL(c_pattern_format),
                      pattern_format_str, (intptr_t) entry, MUTT_FORMAT_ARROWCURSOR);
}

/**
 * free_pattern_menu - Free the Pattern Completion menu - Implements Menu::mdata_free() - @ingroup menu_mdata_free
 */
static void free_pattern_menu(struct Menu *menu, void **ptr)
{
  struct PatternEntry *entries = *ptr;

  for (size_t i = 0; i < menu->max; i++)
  {
    FREE(&entries[i].tag);
    FREE(&entries[i].expr);
    FREE(&entries[i].desc);
  }

  FREE(ptr);
}

/**
 * create_pattern_menu - Create the Pattern Completion menu
 * @param dlg Dialog holding the Menu
 * @retval ptr New Menu
 */
static struct Menu *create_pattern_menu(struct MuttWindow *dlg)
{
  int num_entries = 0, i = 0;
  struct Buffer *entrybuf = NULL;

  while (Flags[num_entries].tag)
    num_entries++;
  /* Add three more hard-coded entries */
  num_entries += 3;
  struct PatternEntry *entries = mutt_mem_calloc(num_entries, sizeof(struct PatternEntry));

  struct Menu *menu = dlg->wdata;
  menu->make_entry = make_pattern_entry;
  menu->mdata = entries;
  menu->mdata_free = free_pattern_menu;
  menu->max = num_entries;

  struct MuttWindow *sbar = window_find_child(dlg, WT_STATUS_BAR);
  // L10N: Pattern completion menu title
  sbar_set_title(sbar, _("Patterns"));

  entrybuf = buf_pool_get();
  while (Flags[i].tag)
  {
    entries[i].num = i + 1;

    buf_printf(entrybuf, "~%c", (char) Flags[i].tag);
    entries[i].tag = mutt_str_dup(buf_string(entrybuf));

    switch (Flags[i].eat_arg)
    {
      case EAT_REGEX:
        /* L10N:
           Pattern Completion Menu argument type: a regular expression
        */
        buf_add_printf(entrybuf, " %s", _("EXPR"));
        break;
      case EAT_RANGE:
      case EAT_MESSAGE_RANGE:
        /* L10N:
           Pattern Completion Menu argument type: a numeric range.
           Used by ~m, ~n, ~X, ~z.
        */
        buf_add_printf(entrybuf, " %s", _("RANGE"));
        break;
      case EAT_DATE:
        /* L10N:
           Pattern Completion Menu argument type: a date range
           Used by ~d, ~r.
        */
        buf_add_printf(entrybuf, " %s", _("DATERANGE"));
        break;
      case EAT_QUERY:
        /* L10N:
           Pattern Completion Menu argument type: a query
           Used by ~I.
        */
        buf_add_printf(entrybuf, " %s", _("QUERY"));
        break;
      default:
        break;
    }
    entries[i].expr = mutt_str_dup(buf_string(entrybuf));
    entries[i].desc = mutt_str_dup(_(Flags[i].desc));

    i++;
  }

  /* Add struct MuttThread patterns manually.
   * Note we allocated 3 extra slots for these above. */

  /* L10N:
     Pattern Completion Menu argument type: a nested pattern.
     Used by ~(), ~<(), ~>().
  */
  const char *patternstr = _("PATTERN");

  entries[i].num = i + 1;
  entries[i].tag = mutt_str_dup("~()");
  buf_printf(entrybuf, "~(%s)", patternstr);
  entries[i].expr = mutt_str_dup(buf_string(entrybuf));
  // L10N: Pattern Completion Menu description for ~()
  entries[i].desc = mutt_str_dup(_("messages in threads containing messages matching PATTERN"));
  i++;

  entries[i].num = i + 1;
  entries[i].tag = mutt_str_dup("~<()");
  buf_printf(entrybuf, "~<(%s)", patternstr);
  entries[i].expr = mutt_str_dup(buf_string(entrybuf));
  // L10N: Pattern Completion Menu description for ~<()
  entries[i].desc = mutt_str_dup(_("messages whose immediate parent matches PATTERN"));
  i++;

  entries[i].num = i + 1;
  entries[i].tag = mutt_str_dup("~>()");
  buf_printf(entrybuf, "~>(%s)", patternstr);
  entries[i].expr = mutt_str_dup(buf_string(entrybuf));
  // L10N: Pattern Completion Menu description for ~>()
  entries[i].desc = mutt_str_dup(_("messages having an immediate child matching PATTERN"));

  buf_pool_release(&entrybuf);

  return menu;
}

/**
 * pattern_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 *
 * The Address Book Window is affected by changes to `$sort_pattern`.
 */
static int pattern_config_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_CONFIG)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;

  if (!mutt_str_equal(ev_c->name, "pattern_format"))
    return 0;

  struct Menu *menu = nc->global_data;
  menu_queue_redraw(menu, MENU_REDRAW_FULL);
  mutt_debug(LL_DEBUG5, "config done, request WA_RECALC, MENU_REDRAW_FULL\n");

  return 0;
}

/**
 * pattern_window_observer - Notification that a Window has changed - Implements ::observer_t - @ingroup observer_api
 *
 * This function is triggered by changes to the windows.
 *
 * - Delete (this window): clean up the resources held by the Help Bar
 */
static int pattern_window_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_WINDOW)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;
  if (nc->event_subtype != NT_WINDOW_DELETE)
    return 0;

  struct MuttWindow *win_menu = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != win_menu)
    return 0;

  struct Menu *menu = win_menu->wdata;

  notify_observer_remove(NeoMutt->sub->notify, pattern_config_observer, menu);
  notify_observer_remove(win_menu->notify, pattern_window_observer, win_menu);

  mutt_debug(LL_DEBUG5, "window delete done\n");
  return 0;
}

/**
 * dlg_pattern - Show menu to select a Pattern - @ingroup gui_dlg
 * @param buf    Buffer for the selected Pattern
 * @param buflen Length of buffer
 * @retval true A selection was made
 *
 * The Select Pattern Dialog shows the user a help page of Patterns.
 * They can select one to auto-complete some functions, e.g. `<limit>`
 */
bool dlg_pattern(char *buf, size_t buflen)
{
  struct MuttWindow *dlg = simple_dialog_new(MENU_GENERIC, WT_DLG_PATTERN, PatternHelp);
  struct Menu *menu = create_pattern_menu(dlg);

  struct PatternData pd = { false, false, buf, buflen, menu };
  dlg->wdata = &pd;

  // NT_COLOR is handled by the SimpleDialog
  notify_observer_add(NeoMutt->sub->notify, NT_CONFIG, pattern_config_observer, menu);
  notify_observer_add(menu->win->notify, NT_WINDOW, pattern_window_observer, menu->win);

  struct MuttWindow *old_focus = window_set_focus(menu->win);
  // ---------------------------------------------------------------------------
  // Event Loop
  int op = OP_NULL;
  do
  {
    menu_tagging_dispatcher(menu->win, op);
    window_redraw(NULL);

    op = km_dokey(MENU_DIALOG, GETCH_NO_FLAGS);
    mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", opcodes_get_name(op), op);
    if (op < 0)
      continue;
    if (op == OP_NULL)
    {
      km_error_key(MENU_GENERIC);
      continue;
    }
    mutt_clear_error();

    int rc = pattern_function_dispatcher(dlg, op);
    if (rc == FR_UNKNOWN)
      rc = menu_function_dispatcher(menu->win, op);
    if (rc == FR_UNKNOWN)
      rc = global_function_dispatcher(NULL, op);
  } while (!pd.done);
  // ---------------------------------------------------------------------------

  window_set_focus(old_focus);
  simple_dialog_free(&dlg);
  return pd.selection;
}
