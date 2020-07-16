/**
 * @file
 * Display a guide to Patterns
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
 * @page pattern_dlgpattern Display a guide to Patterns
 *
 * Display a guide to Patterns
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
#include "mutt.h"
#include "format_flags.h"
#include "keymap.h"
#include "mutt_globals.h"
#include "mutt_menu.h"
#include "muttlib.h"
#include "opcodes.h"

/**
 * struct PatternEntry - A line in the Pattern Completion menu
 */
struct PatternEntry
{
  int num;           ///< Index number
  const char *tag;   ///< Copied to buffer if selected
  const char *expr;  ///< Displayed in the menu
  const char *descr; ///< Description of pattern
};

static const struct Mapping PatternHelp[] = {
  { N_("Exit"), OP_EXIT },
  { N_("Select"), OP_GENERIC_SELECT_ENTRY },
  { N_("Help"), OP_HELP },
  { NULL, 0 },
};

/**
 * pattern_format_str - Format a string for the pattern completion menu - Implements ::format_t
 *
 * | Expando | Description
 * |:--------|:----------------------
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
      mutt_format_s(buf, buflen, prec, NONULL(entry->descr));
      break;
    case 'e':
      mutt_format_s(buf, buflen, prec, NONULL(entry->expr));
      break;
    case 'n':
    {
      char tmp[32];
      snprintf(tmp, sizeof(tmp), "%%%sd", prec);
      snprintf(buf, buflen, tmp, entry->num);
      break;
    }
  }

  return src;
}

/**
 * make_pattern_entry - Create a line for the Pattern Completion menu
 * @param buf    Buffer for the result
 * @param buflen Length of buffer
 * @param menu   Menu
 * @param num    Index number
 */
static void make_pattern_entry(char *buf, size_t buflen, struct Menu *menu, int num)
{
  struct PatternEntry *entry = &((struct PatternEntry *) menu->mdata)[num];

  mutt_expando_format(buf, buflen, 0, menu->win_index->state.cols,
                      NONULL(C_PatternFormat), pattern_format_str,
                      (intptr_t) entry, MUTT_FORMAT_ARROWCURSOR);
}

/**
 * create_pattern_menu - Create the Pattern Completion menu
 * @retval ptr New Menu
 */
static struct Menu *create_pattern_menu(void)
{
  struct PatternEntry *entries = NULL;
  int num_entries = 0, i = 0;
  struct Buffer *entrybuf = NULL;

  while (Flags[num_entries].tag)
    num_entries++;
  /* Add three more hard-coded entries */
  num_entries += 3;

  struct Menu *menu = mutt_menu_new(MENU_GENERIC);

  menu->make_entry = make_pattern_entry;

  // L10N: Pattern completion menu title
  menu->title = _("Patterns");
  char *helpstr = mutt_mem_malloc(256);
  menu->help = mutt_compile_help(helpstr, 256, MENU_GENERIC, PatternHelp);

  menu->mdata = entries = mutt_mem_calloc(num_entries, sizeof(struct PatternEntry));
  menu->max = num_entries;

  entrybuf = mutt_buffer_pool_get();
  while (Flags[i].tag)
  {
    entries[i].num = i + 1;

    mutt_buffer_printf(entrybuf, "~%c", (char) Flags[i].tag);
    entries[i].tag = mutt_str_dup(mutt_b2s(entrybuf));

    switch (Flags[i].eat_arg)
    {
      case EAT_REGEX:
        /* L10N:
           Pattern Completion Menu argument type: a regular expression
        */
        mutt_buffer_add_printf(entrybuf, " %s", _("EXPR"));
        break;
      case EAT_RANGE:
      case EAT_MESSAGE_RANGE:
        /* L10N:
           Pattern Completion Menu argument type: a numeric range.
           Used by ~m, ~n, ~X, ~z.
        */
        mutt_buffer_add_printf(entrybuf, " %s", _("RANGE"));
        break;
      case EAT_DATE:
        /* L10N:
           Pattern Completion Menu argument type: a date range
           Used by ~d, ~r.
        */
        mutt_buffer_add_printf(entrybuf, " %s", _("DATERANGE"));
        break;
      case EAT_QUERY:
        /* L10N:
           Pattern Completion Menu argument type: a query
           Used by ~I.
        */
        mutt_buffer_add_printf(entrybuf, " %s", _("QUERY"));
        break;
      default:
        break;
    }
    entries[i].expr = mutt_str_dup(mutt_b2s(entrybuf));
    entries[i].descr = mutt_str_dup(_(Flags[i].desc));

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
  mutt_buffer_printf(entrybuf, "~(%s)", patternstr);
  entries[i].expr = mutt_str_dup(mutt_b2s(entrybuf));
  // L10N: Pattern Completion Menu description for ~()
  entries[i].descr = mutt_str_dup(
      _("messages in threads containing messages matching PATTERN"));
  i++;

  entries[i].num = i + 1;
  entries[i].tag = mutt_str_dup("~<()");
  mutt_buffer_printf(entrybuf, "~<(%s)", patternstr);
  entries[i].expr = mutt_str_dup(mutt_b2s(entrybuf));
  // L10N: Pattern Completion Menu description for ~<()
  entries[i].descr =
      mutt_str_dup(_("messages whose immediate parent matches PATTERN"));
  i++;

  entries[i].num = i + 1;
  entries[i].tag = mutt_str_dup("~>()");
  mutt_buffer_printf(entrybuf, "~>(%s)", patternstr);
  entries[i].expr = mutt_str_dup(mutt_b2s(entrybuf));
  // L10N: Pattern Completion Menu description for ~>()
  entries[i].descr =
      mutt_str_dup(_("messages having an immediate child matching PATTERN"));

  mutt_menu_push_current(menu);

  mutt_buffer_pool_release(&entrybuf);

  return menu;
}

/**
 * free_pattern_menu - Free the Pattern Completion menu
 * @param ptr Menu to free
 */
static void free_pattern_menu(struct Menu **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Menu *menu = *ptr;
  mutt_menu_pop_current(menu);

  struct PatternEntry *entries = (struct PatternEntry *) menu->mdata;
  while (menu->max)
  {
    menu->max--;
    FREE(&entries[menu->max].tag);
    FREE(&entries[menu->max].expr);
    FREE(&entries[menu->max].descr);
  }
  FREE(&menu->mdata);

  FREE(&menu->help);
  mutt_menu_free(ptr);
}

/**
 * pattern_config_observer - Listen for config changes affecting the Pattern menu - Implements ::observer_t
 */
static int pattern_config_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_CONFIG)
    return 0;

  struct EventConfig *ec = nc->event_data;
  struct MuttWindow *dlg = nc->global_data;

  if (!mutt_str_equal(ec->name, "status_on_top"))
    return 0;

  struct MuttWindow *win_first = TAILQ_FIRST(&dlg->children);

  if ((C_StatusOnTop && (win_first->type == WT_INDEX)) ||
      (!C_StatusOnTop && (win_first->type != WT_INDEX)))
  {
    // Swap the Index and the IndexBar Windows
    TAILQ_REMOVE(&dlg->children, win_first, entries);
    TAILQ_INSERT_TAIL(&dlg->children, win_first, entries);
  }

  mutt_window_reflow(dlg);
  return 0;
}


/**
 * mutt_ask_pattern - Show menu to select a Pattern
 * @param buf    Buffer for the selected Pattern
 * @param buflen Length of buffer
 * @retval bool true, if a selection was made
 */
bool mutt_ask_pattern(char *buf, size_t buflen)
{
  struct MuttWindow *dlg =
      mutt_window_new(WT_DLG_PATTERN, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);

  struct MuttWindow *index =
      mutt_window_new(WT_INDEX, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);

  struct MuttWindow *ibar =
      mutt_window_new(WT_INDEX_BAR, MUTT_WIN_ORIENT_VERTICAL,
                      MUTT_WIN_SIZE_FIXED, MUTT_WIN_SIZE_UNLIMITED, 1);

  if (C_StatusOnTop)
  {
    mutt_window_add_child(dlg, ibar);
    mutt_window_add_child(dlg, index);
  }
  else
  {
    mutt_window_add_child(dlg, index);
    mutt_window_add_child(dlg, ibar);
  }

  notify_observer_add(NeoMutt->notify, pattern_config_observer, dlg);
  dialog_push(dlg);

  struct Menu *menu = create_pattern_menu();

  menu->pagelen = index->state.rows;
  menu->win_index = index;
  menu->win_ibar = ibar;

  bool rc = false;
  bool done = false;
  while (!done)
  {
    switch (mutt_menu_loop(menu))
    {
      case OP_GENERIC_SELECT_ENTRY:
      {
        struct PatternEntry *entry = (struct PatternEntry *) menu->mdata + menu->current;
        mutt_str_copy(buf, entry->tag, buflen);
        rc = true;
        done = true;
        break;
      }

      case OP_EXIT:
        done = true;
        break;
    }
  }

  free_pattern_menu(&menu);
  dialog_pop();
  notify_observer_remove(NeoMutt->notify, pattern_config_observer, dlg);
  mutt_window_free(&dlg);
  return rc;
}
