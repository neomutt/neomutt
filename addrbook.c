/**
 * @file
 * Address book handling aliases
 *
 * @authors
 * Copyright (C) 1996-2000,2007 Michael R. Elkins <me@mutt.org>
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
 * @page addrbook Address book handling aliases
 *
 * Address book handling aliases
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "mutt/mutt.h"
#include "address/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "addrbook.h"
#include "alias.h"
#include "format_flags.h"
#include "globals.h"
#include "keymap.h"
#include "mutt_menu.h"
#include "muttlib.h"
#include "opcodes.h"

/* These Config Variables are only used in addrbook.c */
char *C_AliasFormat; ///< Config: printf-like format string for the alias menu
short C_SortAlias;   ///< Config: Sort method for the alias menu

#define RSORT(num) ((C_SortAlias & SORT_REVERSE) ? -num : num)

static const struct Mapping AliasHelp[] = {
  { N_("Exit"), OP_EXIT },      { N_("Del"), OP_DELETE },
  { N_("Undel"), OP_UNDELETE }, { N_("Select"), OP_GENERIC_SELECT_ENTRY },
  { N_("Help"), OP_HELP },      { NULL, 0 },
};

/**
 * alias_format_str - Format a string for the alias list - Implements ::format_t
 *
 * | Expando | Description
 * |:--------|:--------------------------------------------------------
 * | \%a     | Alias name
 * | \%f     | Flags - currently, a 'd' for an alias marked for deletion
 * | \%n     | Index number
 * | \%r     | Address which alias expands to
 * | \%t     | Character which indicates if the alias is tagged for inclusion
 */
static const char *alias_format_str(char *buf, size_t buflen, size_t col, int cols,
                                    char op, const char *src, const char *prec,
                                    const char *if_str, const char *else_str,
                                    unsigned long data, MuttFormatFlags flags)
{
  char fmt[128], addr[128];
  struct Alias *alias = (struct Alias *) data;

  switch (op)
  {
    case 'a':
      mutt_format_s(buf, buflen, prec, alias->name);
      break;
    case 'f':
      snprintf(fmt, sizeof(fmt), "%%%ss", prec);
      snprintf(buf, buflen, fmt, alias->del ? "D" : " ");
      break;
    case 'n':
      snprintf(fmt, sizeof(fmt), "%%%sd", prec);
      snprintf(buf, buflen, fmt, alias->num + 1);
      break;
    case 'r':
      addr[0] = '\0';
      mutt_addrlist_write(&alias->addr, addr, sizeof(addr), true);
      snprintf(fmt, sizeof(fmt), "%%%ss", prec);
      snprintf(buf, buflen, fmt, addr);
      break;
    case 't':
      buf[0] = alias->tagged ? '*' : ' ';
      buf[1] = '\0';
      break;
  }

  return src;
}

/**
 * alias_make_entry - Format a menu item for the alias list - Implements Menu::menu_make_entry()
 */
static void alias_make_entry(char *buf, size_t buflen, struct Menu *menu, int line)
{
  mutt_expando_format(buf, buflen, 0, menu->win_index->state.cols,
                      NONULL(C_AliasFormat), alias_format_str,
                      (unsigned long) ((struct Alias **) menu->data)[line],
                      MUTT_FORMAT_ARROWCURSOR);
}

/**
 * alias_tag - Tag some aliases - Implements Menu::menu_tag()
 */
static int alias_tag(struct Menu *menu, int sel, int act)
{
  struct Alias *cur = ((struct Alias **) menu->data)[sel];
  bool ot = cur->tagged;

  cur->tagged = ((act >= 0) ? act : !cur->tagged);

  return cur->tagged - ot;
}

/**
 * alias_sort_alias - Compare two Aliases
 * @param a First  Alias to compare
 * @param b Second Alias to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int alias_sort_alias(const void *a, const void *b)
{
  const struct Alias *pa = *(struct Alias const *const *) a;
  const struct Alias *pb = *(struct Alias const *const *) b;
  int r = mutt_str_strcasecmp(pa->name, pb->name);

  return RSORT(r);
}

/**
 * alias_sort_address - Compare two Addresses
 * @param a First  Address to compare
 * @param b Second Address to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int alias_sort_address(const void *a, const void *b)
{
  const struct AddressList *pal = &(*(struct Alias const *const *) a)->addr;
  const struct AddressList *pbl = &(*(struct Alias const *const *) b)->addr;
  int r;

  if (pal == pbl)
    r = 0;
  else if (!pal)
    r = -1;
  else if (!pbl)
    r = 1;
  else
  {
    const struct Address *pa = TAILQ_FIRST(pal);
    const struct Address *pb = TAILQ_FIRST(pbl);
    if (pa->personal)
    {
      if (pb->personal)
        r = mutt_str_strcasecmp(pa->personal, pb->personal);
      else
        r = 1;
    }
    else if (pb->personal)
      r = -1;
    else
      r = mutt_str_strcasecmp(pa->mailbox, pb->mailbox);
  }
  return RSORT(r);
}

/**
 * mutt_dlg_alias_observer - Listen for config changes affecting the Alias menu - Implements ::observer_t()
 */
int mutt_dlg_alias_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_CONFIG)
    return 0;

  struct EventConfig *ec = nc->event_data;
  struct MuttWindow *dlg = nc->global_data;

  if (mutt_str_strcmp(ec->name, "status_on_top") != 0)
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
 * mutt_alias_menu - Display a menu of Aliases
 * @param buf    Buffer for expanded aliases
 * @param buflen Length of buffer
 * @param aliases Alias List
 */
void mutt_alias_menu(char *buf, size_t buflen, struct AliasList *aliases)
{
  struct Alias *a = NULL, *last = NULL;
  struct Menu *menu = NULL;
  struct Alias **alias_table = NULL;
  int t = -1;
  int i;
  bool done = false;
  char helpstr[1024];

  int omax;

  if (TAILQ_EMPTY(aliases))
  {
    mutt_error(_("You have no aliases"));
    return;
  }

  struct MuttWindow *dlg =
      mutt_window_new(MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  dlg->type = WT_DIALOG;
  struct MuttWindow *index =
      mutt_window_new(MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  index->type = WT_INDEX;
  struct MuttWindow *ibar = mutt_window_new(
      MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_FIXED, 1, MUTT_WIN_SIZE_UNLIMITED);
  ibar->type = WT_INDEX_BAR;

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

  notify_observer_add(NeoMutt->notify, mutt_dlg_alias_observer, dlg);
  dialog_push(dlg);

  menu = mutt_menu_new(MENU_ALIAS);

  menu->pagelen = index->state.rows;
  menu->win_index = index;
  menu->win_ibar = ibar;

  menu->menu_make_entry = alias_make_entry;
  menu->menu_tag = alias_tag;
  menu->title = _("Aliases");
  menu->help = mutt_compile_help(helpstr, sizeof(helpstr), MENU_ALIAS, AliasHelp);
  mutt_menu_push_current(menu);

new_aliases:
  omax = menu->max;

  /* count the number of aliases */
  TAILQ_FOREACH_FROM(a, aliases, entries)
  {
    a->del = false;
    a->tagged = false;
    menu->max++;
  }

  mutt_mem_realloc(&alias_table, menu->max * sizeof(struct Alias *));
  menu->data = alias_table;
  if (!alias_table)
    goto mam_done;

  if (last)
    a = TAILQ_NEXT(last, entries);

  i = omax;
  TAILQ_FOREACH_FROM(a, aliases, entries)
  {
    alias_table[i] = a;
    i++;
  }

  if ((C_SortAlias & SORT_MASK) != SORT_ORDER)
  {
    qsort(alias_table, menu->max, sizeof(struct Alias *),
          ((C_SortAlias & SORT_MASK) == SORT_ADDRESS) ? alias_sort_address : alias_sort_alias);
  }

  for (i = 0; i < menu->max; i++)
    alias_table[i]->num = i;

  last = TAILQ_LAST(aliases, AliasList);

  while (!done)
  {
    int op;
    if (TAILQ_NEXT(last, entries))
    {
      menu->redraw |= REDRAW_FULL;
      a = TAILQ_NEXT(last, entries);
      goto new_aliases;
    }
    switch ((op = mutt_menu_loop(menu)))
    {
      case OP_DELETE:
      case OP_UNDELETE:
        if (menu->tagprefix)
        {
          for (i = 0; i < menu->max; i++)
            if (alias_table[i]->tagged)
              alias_table[i]->del = (op == OP_DELETE);
          menu->redraw |= REDRAW_INDEX;
        }
        else
        {
          alias_table[menu->current]->del = (op == OP_DELETE);
          menu->redraw |= REDRAW_CURRENT;
          if (C_Resolve && (menu->current < menu->max - 1))
          {
            menu->current++;
            menu->redraw |= REDRAW_INDEX;
          }
        }
        break;
      case OP_GENERIC_SELECT_ENTRY:
        t = menu->current;
        done = true;
        break;
      case OP_EXIT:
        done = true;
        break;
    }
  }

  for (i = 0; i < menu->max; i++)
  {
    if (alias_table[i]->tagged)
    {
      mutt_addrlist_write(&alias_table[i]->addr, buf, buflen, true);
      t = -1;
    }
  }

  if (t != -1)
  {
    mutt_addrlist_write(&alias_table[t]->addr, buf, buflen, true);
  }

  FREE(&alias_table);

mam_done:
  mutt_menu_pop_current(menu);
  mutt_menu_free(&menu);
  dialog_pop();
  notify_observer_remove(NeoMutt->notify, mutt_dlg_alias_observer, dlg);
  mutt_window_free(&dlg);
}
