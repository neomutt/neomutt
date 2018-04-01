/**
 * @file
 * Address book handling aliases
 *
 * @authors
 * Copyright (C) 1996-2000,2007 Michael R. Elkins <me@mutt.org>
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

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "mutt/mutt.h"
#include "mutt.h"
#include "alias.h"
#include "format_flags.h"
#include "globals.h"
#include "keymap.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "mutt_window.h"
#include "opcodes.h"
#include "options.h"
#include "protos.h"
#include "sort.h"

#define RSORT(x) (SortAlias & SORT_REVERSE) ? -x : x

static const struct Mapping AliasHelp[] = {
  { N_("Exit"), OP_EXIT },      { N_("Del"), OP_DELETE },
  { N_("Undel"), OP_UNDELETE }, { N_("Select"), OP_GENERIC_SELECT_ENTRY },
  { N_("Help"), OP_HELP },      { NULL, 0 },
};

/**
 * alias_format_str - Format a string for the alias list
 * @param[out] buf      Buffer in which to save string
 * @param[in]  buflen   Buffer length
 * @param[in]  col      Starting column
 * @param[in]  cols     Number of screen columns
 * @param[in]  op       printf-like operator, e.g. 't'
 * @param[in]  src      printf-like format string
 * @param[in]  prec     Field precision, e.g. "-3.4"
 * @param[in]  if_str   If condition is met, display this string
 * @param[in]  else_str Otherwise, display this string
 * @param[in]  data     Pointer to the mailbox Context
 * @param[in]  flags    Format flags
 * @retval src (unchanged)
 *
 * alias_format_str() is a callback function for mutt_expando_format().
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
                                    unsigned long data, enum FormatFlag flags)
{
  char fmt[SHORT_STRING], addr[SHORT_STRING];
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
      mutt_addr_write(addr, sizeof(addr), alias->addr, true);
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
 * alias_entry - Format a menu item for the alias list
 * @param[out] buf    Buffer in which to save string
 * @param[in]  buflen Buffer length
 * @param[in]  menu   Menu containing aliases
 * @param[in]  num    Index into the menu
 */
static void alias_entry(char *buf, size_t buflen, struct Menu *menu, int num)
{
  mutt_expando_format(buf, buflen, 0, MuttIndexWindow->cols, NONULL(AliasFormat), alias_format_str,
                      (unsigned long) ((struct Alias **) menu->data)[num],
                      MUTT_FORMAT_ARROWCURSOR);
}

static int alias_tag(struct Menu *menu, int n, int m)
{
  struct Alias *cur = ((struct Alias **) menu->data)[n];
  bool ot = cur->tagged;

  cur->tagged = (m >= 0 ? m : !cur->tagged);

  return cur->tagged - ot;
}

static int alias_sort_alias(const void *a, const void *b)
{
  struct Alias *pa = *(struct Alias **) a;
  struct Alias *pb = *(struct Alias **) b;
  int r = mutt_str_strcasecmp(pa->name, pb->name);

  return (RSORT(r));
}

static int alias_sort_address(const void *a, const void *b)
{
  struct Address *pa = (*(struct Alias **) a)->addr;
  struct Address *pb = (*(struct Alias **) b)->addr;
  int r;

  if (pa == pb)
    r = 0;
  else if (!pa)
    r = -1;
  else if (!pb)
    r = 1;
  else if (pa->personal)
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
  return (RSORT(r));
}

void mutt_alias_menu(char *buf, size_t buflen, struct Alias *aliases)
{
  struct Alias *aliasp = NULL;
  struct Menu *menu = NULL;
  struct Alias **AliasTable = NULL;
  int t = -1;
  int i;
  bool done = false;
  int op;
  char helpstr[LONG_STRING];

  int omax;

  if (!aliases)
  {
    mutt_error(_("You have no aliases!"));
    return;
  }

  menu = mutt_menu_new(MENU_ALIAS);
  menu->make_entry = alias_entry;
  menu->tag = alias_tag;
  menu->title = _("Aliases");
  menu->help = mutt_compile_help(helpstr, sizeof(helpstr), MENU_ALIAS, AliasHelp);
  mutt_menu_push_current(menu);

new_aliases:

  omax = menu->max;

  /* count the number of aliases */
  for (aliasp = aliases; aliasp; aliasp = aliasp->next)
  {
    aliasp->del = false;
    aliasp->tagged = false;
    menu->max++;
  }

  mutt_mem_realloc(&AliasTable, menu->max * sizeof(struct Alias *));
  menu->data = AliasTable;
  if (!AliasTable)
    return;

  for (i = omax, aliasp = aliases; aliasp; aliasp = aliasp->next, i++)
  {
    AliasTable[i] = aliasp;
    aliases = aliasp;
  }

  if ((SortAlias & SORT_MASK) != SORT_ORDER)
  {
    qsort(AliasTable, i, sizeof(struct Alias *),
          (SortAlias & SORT_MASK) == SORT_ADDRESS ? alias_sort_address : alias_sort_alias);
  }

  for (i = 0; i < menu->max; i++)
    AliasTable[i]->num = i;

  while (!done)
  {
    if (aliases->next)
    {
      menu->redraw |= REDRAW_FULL;
      aliases = aliases->next;
      goto new_aliases;
    }

    switch ((op = mutt_menu_loop(menu)))
    {
      case OP_DELETE:
      case OP_UNDELETE:
        if (menu->tagprefix)
        {
          for (i = 0; i < menu->max; i++)
            if (AliasTable[i]->tagged)
              AliasTable[i]->del = (op == OP_DELETE);
          menu->redraw |= REDRAW_INDEX;
        }
        else
        {
          AliasTable[menu->current]->del = (op == OP_DELETE);
          menu->redraw |= REDRAW_CURRENT;
          if (Resolve && menu->current < menu->max - 1)
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
    if (AliasTable[i]->tagged)
    {
      mutt_addr_write(buf, buflen, AliasTable[i]->addr, true);
      t = -1;
    }
  }

  if (t != -1)
  {
    mutt_addr_write(buf, buflen, AliasTable[t]->addr, true);
  }

  mutt_menu_pop_current(menu);
  mutt_menu_destroy(&menu);
  FREE(&AliasTable);
}
