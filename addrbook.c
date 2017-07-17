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
#include "address.h"
#include "alias.h"
#include "ascii.h"
#include "format_flags.h"
#include "globals.h"
#include "keymap.h"
#include "keymap_defs.h"
#include "lib.h"
#include "mapping.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "options.h"
#include "protos.h"
#include "rfc822.h"
#include "sort.h"

#define RSORT(x) (SortAlias & SORT_REVERSE) ? -x : x

static const struct Mapping AliasHelp[] = {
  { N_("Exit"), OP_EXIT },      { N_("Del"), OP_DELETE },
  { N_("Undel"), OP_UNDELETE }, { N_("Select"), OP_GENERIC_SELECT_ENTRY },
  { N_("Help"), OP_HELP },      { NULL, 0 },
};

static const char *alias_format_str(char *dest, size_t destlen, size_t col, int cols,
                                    char op, const char *src, const char *fmt,
                                    const char *ifstring, const char *elsestring,
                                    unsigned long data, enum FormatFlag flags)
{
  char tmp[SHORT_STRING], adr[SHORT_STRING];
  struct Alias *alias = (struct Alias *) data;

  switch (op)
  {
    case 'f':
      snprintf(tmp, sizeof(tmp), "%%%ss", fmt);
      snprintf(dest, destlen, tmp, alias->del ? "D" : " ");
      break;
    case 'a':
      mutt_format_s(dest, destlen, fmt, alias->name);
      break;
    case 'r':
      adr[0] = 0;
      rfc822_write_address(adr, sizeof(adr), alias->addr, 1);
      snprintf(tmp, sizeof(tmp), "%%%ss", fmt);
      snprintf(dest, destlen, tmp, adr);
      break;
    case 'n':
      snprintf(tmp, sizeof(tmp), "%%%sd", fmt);
      snprintf(dest, destlen, tmp, alias->num + 1);
      break;
    case 't':
      dest[0] = alias->tagged ? '*' : ' ';
      dest[1] = 0;
      break;
  }

  return src;
}

static void alias_entry(char *s, size_t slen, struct Menu *m, int num)
{
  mutt_expando_format(s, slen, 0, MuttIndexWindow->cols, NONULL(AliasFmt), alias_format_str,
                    (unsigned long) ((struct Alias **) m->data)[num],
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
  int r = mutt_strcasecmp(pa->name, pb->name);

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
      r = mutt_strcasecmp(pa->personal, pb->personal);
    else
      r = 1;
  }
  else if (pb->personal)
    r = -1;
  else
    r = ascii_strcasecmp(pa->mailbox, pb->mailbox);
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

  menu = mutt_new_menu(MENU_ALIAS);
  menu->make_entry = alias_entry;
  menu->tag = alias_tag;
  menu->title = _("Aliases");
  menu->help = mutt_compile_help(helpstr, sizeof(helpstr), MENU_ALIAS, AliasHelp);
  mutt_push_current_menu(menu);

new_aliases:

  omax = menu->max;

  /* count the number of aliases */
  for (aliasp = aliases; aliasp; aliasp = aliasp->next)
  {
    aliasp->self->del = false;
    aliasp->self->tagged = false;
    menu->max++;
  }

  safe_realloc(&AliasTable, menu->max * sizeof(struct Alias *));
  menu->data = AliasTable;
  if (!AliasTable)
    return;

  for (i = omax, aliasp = aliases; aliasp; aliasp = aliasp->next, i++)
  {
    AliasTable[i] = aliasp->self;
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
          AliasTable[menu->current]->self->del = (op == OP_DELETE);
          menu->redraw |= REDRAW_CURRENT;
          if (option(OPTRESOLVE) && menu->current < menu->max - 1)
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
      rfc822_write_address(buf, buflen, AliasTable[i]->addr, 1);
      t = -1;
    }
  }

  if (t != -1)
  {
    rfc822_write_address(buf, buflen, AliasTable[t]->addr, 1);
  }

  mutt_pop_current_menu(menu);
  mutt_menu_destroy(&menu);
  FREE(&AliasTable);
}
