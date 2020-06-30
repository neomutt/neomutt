/**
 * @file
 * Address book handling aliases
 *
 * @authors
 * Copyright (C) 1996-2000,2007 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * @page alias_dlgalias Address book handling aliases
 *
 * Address book handling aliases
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "lib.h"
#include "alias.h"
#include "format_flags.h"
#include "gui.h"
#include "keymap.h"
#include "mutt_globals.h"
#include "mutt_menu.h"
#include "muttlib.h"
#include "opcodes.h"

/* These Config Variables are only used in dlgalias.c */
char *C_AliasFormat; ///< Config: printf-like format string for the alias menu
short C_SortAlias;   ///< Config: Sort method for the alias menu

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
 * | \%c     | Comments
 * | \%f     | Flags - currently, a 'd' for an alias marked for deletion
 * | \%n     | Index number
 * | \%r     | Address which alias expands to
 * | \%t     | Character which indicates if the alias is tagged for inclusion
 */
static const char *alias_format_str(char *buf, size_t buflen, size_t col, int cols,
                                    char op, const char *src, const char *prec,
                                    const char *if_str, const char *else_str,
                                    intptr_t data, MuttFormatFlags flags)
{
  char fmt[128], addr[1024];
  struct AliasView *av = (struct AliasView *) data;
  struct Alias *alias = av->alias;

  switch (op)
  {
    case 'a':
      mutt_format_s(buf, buflen, prec, alias->name);
      break;
    case 'c':
      mutt_format_s(buf, buflen, prec, alias->comment);
      break;
    case 'f':
      snprintf(fmt, sizeof(fmt), "%%%ss", prec);
      snprintf(buf, buflen, fmt, av->is_deleted ? "D" : " ");
      break;
    case 'n':
      snprintf(fmt, sizeof(fmt), "%%%sd", prec);
      snprintf(buf, buflen, fmt, av->num + 1);
      break;
    case 'r':
      addr[0] = '\0';
      mutt_addrlist_write(&alias->addr, addr, sizeof(addr), true);
      mutt_format_s(buf, buflen, prec, addr);
      break;
    case 't':
      buf[0] = av->is_tagged ? '*' : ' ';
      buf[1] = '\0';
      break;
  }

  return src;
}

/**
 * alias_make_entry - Format a menu item for the alias list - Implements Menu::make_entry()
 */
static void alias_make_entry(char *buf, size_t buflen, struct Menu *menu, int line)
{
  const struct AliasMenuData *mdata = menu->mdata;
  const struct AliasView *av = mdata->av[line];

  mutt_expando_format(buf, buflen, 0, menu->win_index->state.cols, NONULL(C_AliasFormat),
                      alias_format_str, IP av, MUTT_FORMAT_ARROWCURSOR);
}

/**
 * alias_tag - Tag some aliases - Implements Menu::tag()
 */
static int alias_tag(struct Menu *menu, int sel, int act)
{
  struct AliasMenuData *mdata = (struct AliasMenuData *) menu->mdata;
  struct AliasView *av = mdata->av[sel];

  bool ot = av->is_tagged;

  av->is_tagged = ((act >= 0) ? act : !av->is_tagged);

  return av->is_tagged - ot;
}

/**
 * alias_data_observer - Listen for data changes affecting the Alias menu - Implements ::observer_t
 */
static int alias_data_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_ALIAS)
    return 0;

  struct EventAlias *ea = nc->event_data;
  struct Menu *menu = nc->global_data;
  struct AliasMenuData *mdata = menu->mdata;
  struct Alias *alias = ea->alias;

  if (nc->event_subtype == NT_ALIAS_NEW)
  {
    menu_data_alias_add(mdata, alias);
  }
  else if (nc->event_subtype == NT_ALIAS_DELETED)
  {
    menu_data_alias_delete(mdata, alias);
  }
  menu_data_sort(mdata);

  menu->max = mdata->num_views;
  menu->redraw = REDRAW_FULL;
  return 0;
}

/**
 * alias_config_observer - Listen for config changes affecting the Alias menu - Implements ::observer_t
 */
static int alias_config_observer(struct NotifyCallback *nc)
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
 * alias_menu - Display a menu of Aliases
 * @param buf    Buffer for expanded aliases
 * @param buflen Length of buffer
 * @param mdata  Menu data holding Aliases
 */
static void alias_menu(char *buf, size_t buflen, struct AliasMenuData *mdata)
{
  if (mdata->num_views == 0)
  {
    mutt_warning(_("You have no aliases"));
    return;
  }

  int t = -1;
  bool done = false;
  char helpstr[1024];

  struct MuttWindow *dlg =
      mutt_window_new(WT_DLG_ALIAS, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
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

  notify_observer_add(NeoMutt->notify, alias_config_observer, dlg);
  dialog_push(dlg);

  struct Menu *menu = mutt_menu_new(MENU_ALIAS);

  menu->pagelen = index->state.rows;
  menu->win_index = index;
  menu->win_ibar = ibar;

  menu->make_entry = alias_make_entry;
  menu->tag = alias_tag;
  menu->title = _("Aliases");
  menu->help = mutt_compile_help(helpstr, sizeof(helpstr), MENU_ALIAS, AliasHelp);
  mutt_menu_push_current(menu);

  menu->max = mdata->num_views;
  menu->mdata = mdata;
  notify_observer_add(NeoMutt->notify, alias_data_observer, menu);

  if ((C_SortAlias & SORT_MASK) != SORT_ORDER)
  {
    qsort(mdata->av, mdata->num_views, sizeof(struct AliasView *),
          ((C_SortAlias & SORT_MASK) == SORT_ADDRESS) ? alias_sort_address : alias_sort_name);
  }

  for (int i = 0; i < menu->max; i++)
    mdata->av[i]->num = i;

  while (!done)
  {
    int op = mutt_menu_loop(menu);
    switch (op)
    {
      case OP_DELETE:
      case OP_UNDELETE:
        if (menu->tagprefix)
        {
          for (int i = 0; i < menu->max; i++)
            if (mdata->av[i]->is_tagged)
              mdata->av[i]->is_deleted = (op == OP_DELETE);
          menu->redraw |= REDRAW_INDEX;
        }
        else
        {
          mdata->av[menu->current]->is_deleted = (op == OP_DELETE);
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
        if (t >= mdata->num_views)
          t = -1;
        done = true;
        break;
      case OP_EXIT:
        done = true;
        break;
    }
  }

  for (int i = 0; i < menu->max; i++)
  {
    if (mdata->av[i]->is_tagged)
    {
      mutt_addrlist_write(&mdata->av[i]->alias->addr, buf, buflen, true);
      t = -1;
    }
  }

  if (t != -1)
  {
    mutt_addrlist_write(&mdata->av[t]->alias->addr, buf, buflen, true);
  }

  notify_observer_remove(NeoMutt->notify, alias_data_observer, menu);
  mutt_menu_pop_current(menu);
  mutt_menu_free(&menu);
  dialog_pop();
  notify_observer_remove(NeoMutt->notify, alias_config_observer, dlg);
  mutt_window_free(&dlg);
}

/**
 * alias_complete - alias completion routine
 * @param buf    Partial Alias to complete
 * @param buflen Length of buffer
 * @retval 1 Success
 * @retval 0 Error
 *
 * Given a partial alias, this routine attempts to fill in the alias
 * from the alias list as much as possible. if given empty search string
 * or found nothing, present all aliases
 */
int alias_complete(char *buf, size_t buflen)
{
  struct Alias *np = NULL;
  char bestname[8192] = { 0 };
  struct AliasMenuData *mdata = NULL;

  if (buf[0] != '\0')
  {
    TAILQ_FOREACH(np, &Aliases, entries)
    {
      if (np->name && mutt_strn_equal(np->name, buf, strlen(buf)))
      {
        if (bestname[0] == '\0') /* init */
        {
          mutt_str_copy(bestname, np->name,
                        MIN(mutt_str_len(np->name) + 1, sizeof(bestname)));
        }
        else
        {
          int i;
          for (i = 0; np->name[i] && (np->name[i] == bestname[i]); i++)
            ; // do nothing

          bestname[i] = '\0';
        }
      }
    }

    if (bestname[0] != '\0')
    {
      if (!mutt_str_equal(bestname, buf))
      {
        /* we are adding something to the completion */
        mutt_str_copy(buf, bestname, mutt_str_len(bestname) + 1);
        return 1;
      }

      /* build alias list and show it */
      mdata = menu_data_new();
      TAILQ_FOREACH(np, &Aliases, entries)
      {
        if (np->name && mutt_strn_equal(np->name, buf, strlen(buf)))
        {
          menu_data_alias_add(mdata, np);
        }
      }
    }
  }

  if (!mdata)
  {
    mdata = menu_data_new();
    TAILQ_FOREACH(np, &Aliases, entries)
    {
      menu_data_alias_add(mdata, np);
    }
  }
  menu_data_sort(mdata);

  bestname[0] = '\0';
  alias_menu(bestname, sizeof(bestname), mdata);
  if (bestname[0] != '\0')
    mutt_str_copy(buf, bestname, buflen);

  for (int i = 0; i < mdata->num_views; i++)
  {
    struct AliasView *av = mdata->av[i];
    if (!av->is_deleted)
      continue;

    TAILQ_REMOVE(&Aliases, av->alias, entries);
    alias_free(&av->alias);
  }

  menu_data_free(&mdata);

  return 0;
}
