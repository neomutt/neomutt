/**
 * @file
 * Routines for querying and external address book
 *
 * @authors
 * Copyright (C) 1996-2000,2003,2013 Michael R. Elkins <me@mutt.org>
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
 * @page alias_dlgquery Routines for querying and external address book
 *
 * Routines for querying and external address book
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "lib.h"
#include "alias.h"
#include "format_flags.h"
#include "gui.h"
#include "keymap.h"
#include "mutt_globals.h"
#include "mutt_logging.h"
#include "mutt_menu.h"
#include "muttlib.h"
#include "opcodes.h"
#include "send/lib.h"

/* These Config Variables are only used in dlgquery.c */
char *C_QueryCommand; ///< Config: External command to query and external address book
char *C_QueryFormat; ///< Config: printf-like format string for the query menu (address book)

static const struct Mapping QueryHelp[] = {
  { N_("Exit"), OP_EXIT },
  { N_("Mail"), OP_MAIL },
  { N_("New Query"), OP_QUERY },
  { N_("Make Alias"), OP_CREATE_ALIAS },
  { N_("Search"), OP_SEARCH },
  { N_("Help"), OP_HELP },
  { NULL, 0 },
};

/**
 * alias_to_addrlist - Turn an Alias into an AddressList
 * @param al    AddressList to fill (must be empty)
 * @param alias Alias to use
 * @retval bool True on success
 */
static bool alias_to_addrlist(struct AddressList *al, struct Alias *alias)
{
  if (!al || !TAILQ_EMPTY(al) || !alias)
    return false;

  mutt_addrlist_copy(al, &alias->addr, false);
  if (!TAILQ_EMPTY(al))
  {
    struct Address *first = TAILQ_FIRST(al);
    struct Address *second = TAILQ_NEXT(first, entries);
    if (!second && !first->personal)
      first->personal = mutt_str_dup(alias->name);

    mutt_addrlist_to_intl(al, NULL);
  }

  return true;
}

/**
 * query_search - Search a Address menu item - Implements Menu::search()
 *
 * Try to match various Address fields.
 */
static int query_search(struct Menu *menu, regex_t *rx, int line)
{
  struct AliasMenuData *mdata = menu->mdata;
  struct AliasView *av = mdata->av[line];
  struct Alias *alias = av->alias;

  if (alias->name && (regexec(rx, alias->name, 0, NULL, 0) == 0))
    return 0;
  if (alias->comment && (regexec(rx, alias->comment, 0, NULL, 0) == 0))
    return 0;
  if (!TAILQ_EMPTY(&alias->addr))
  {
    struct Address *addr = TAILQ_FIRST(&alias->addr);
    if (addr->personal && (regexec(rx, addr->personal, 0, NULL, 0) == 0))
    {
      return 0;
    }
    if (addr->mailbox && (regexec(rx, addr->mailbox, 0, NULL, 0) == 0))
    {
      return 0;
    }
  }

  return REG_NOMATCH;
}

/**
 * query_format_str - Format a string for the query menu - Implements ::format_t
 *
 * | Expando | Description
 * |:--------|:--------------------------------------------------------
 * | \%a     | Destination address
 * | \%c     | Current entry number
 * | \%e     | Extra information
 * | \%n     | Destination name
 * | \%t     | `*` if current entry is tagged, a space otherwise
 */
static const char *query_format_str(char *buf, size_t buflen, size_t col, int cols,
                                    char op, const char *src, const char *prec,
                                    const char *if_str, const char *else_str,
                                    intptr_t data, MuttFormatFlags flags)
{
  struct AliasView *av = (struct AliasView *) data;
  struct Alias *alias = av->alias;
  char fmt[128];
  char tmp[256] = { 0 };
  bool optional = (flags & MUTT_FORMAT_OPTIONAL);

  switch (op)
  {
    case 'a':
      tmp[0] = '<';
      mutt_addrlist_write(&alias->addr, tmp + 1, sizeof(tmp) - 1, true);
      const size_t len = strlen(tmp);
      if (len < (sizeof(tmp) - 1))
      {
        tmp[len] = '>';
        tmp[len + 1] = '\0';
      }
      mutt_format_s(buf, buflen, prec, tmp);
      break;
    case 'c':
      snprintf(fmt, sizeof(fmt), "%%%sd", prec);
      snprintf(buf, buflen, fmt, av->num + 1);
      break;
    case 'e':
      if (!optional)
        mutt_format_s(buf, buflen, prec, NONULL(alias->comment));
      else if (!alias->comment || (*alias->comment == '\0'))
        optional = false;
      break;
    case 'n':
      mutt_format_s(buf, buflen, prec, NONULL(alias->name));
      break;
    case 't':
      snprintf(fmt, sizeof(fmt), "%%%sc", prec);
      snprintf(buf, buflen, fmt, av->is_tagged ? '*' : ' ');
      break;
    default:
      snprintf(fmt, sizeof(fmt), "%%%sc", prec);
      snprintf(buf, buflen, fmt, op);
      break;
  }

  if (optional)
  {
    mutt_expando_format(buf, buflen, col, cols, if_str, query_format_str, data,
                        MUTT_FORMAT_NO_FLAGS);
  }
  else if (flags & MUTT_FORMAT_OPTIONAL)
  {
    mutt_expando_format(buf, buflen, col, cols, else_str, query_format_str,
                        data, MUTT_FORMAT_NO_FLAGS);
  }

  return src;
}

/**
 * query_make_entry - Format a menu item for the query list - Implements Menu::make_entry()
 */
static void query_make_entry(char *buf, size_t buflen, struct Menu *menu, int line)
{
  struct AliasMenuData *mdata = menu->mdata;
  struct AliasView *av = mdata->av[line];

  mutt_expando_format(buf, buflen, 0, menu->win_index->state.cols, NONULL(C_QueryFormat),
                      query_format_str, IP av, MUTT_FORMAT_ARROWCURSOR);
}

/**
 * query_tag - Tag an entry in the Query Menu - Implements Menu::tag()
 */
static int query_tag(struct Menu *menu, int sel, int act)
{
  struct AliasMenuData *mdata = menu->mdata;
  struct AliasView *av = mdata->av[sel];

  bool ot = av->is_tagged;

  av->is_tagged = ((act >= 0) ? act : !av->is_tagged);
  return av->is_tagged - ot;
}

/**
 * query_run - Run an external program to find Addresses
 * @param s       String to match
 * @param verbose If true, print progress messages
 * @param al      Alias list to fill
 * @retval  0 Success
 * @retval -1 Error
 */
static int query_run(char *s, bool verbose, struct AliasList *al)
{
  FILE *fp = NULL;
  char *buf = NULL;
  size_t buflen;
  char *msg = NULL;
  size_t msglen = 0;
  char *p = NULL;
  struct Buffer *cmd = mutt_buffer_pool_get();

  mutt_buffer_file_expand_fmt_quote(cmd, C_QueryCommand, s);

  pid_t pid = filter_create(mutt_b2s(cmd), NULL, &fp, NULL);
  if (pid < 0)
  {
    mutt_debug(LL_DEBUG1, "unable to fork command: %s\n", mutt_b2s(cmd));
    mutt_buffer_pool_release(&cmd);
    return -1;
  }
  mutt_buffer_pool_release(&cmd);

  if (verbose)
    mutt_message(_("Waiting for response..."));

  /* The query protocol first reads one NL-terminated line. If an error
   * occurs, this is assumed to be an error message. Otherwise it's ignored. */
  msg = mutt_file_read_line(msg, &msglen, fp, NULL, 0);
  while ((buf = mutt_file_read_line(buf, &buflen, fp, NULL, 0)))
  {
    p = strtok(buf, "\t\n");
    if (p)
    {
      struct Alias *alias = alias_new();

      mutt_addrlist_parse(&alias->addr, p);
      p = strtok(NULL, "\t\n");
      if (p)
      {
        alias->name = mutt_str_dup(p);
        p = strtok(NULL, "\t\n");
        if (p)
          alias->comment = mutt_str_dup(p);
      }
      TAILQ_INSERT_TAIL(al, alias, entries);
    }
  }
  FREE(&buf);
  mutt_file_fclose(&fp);
  if (filter_wait(pid))
  {
    mutt_debug(LL_DEBUG1, "Error: %s\n", msg);
    if (verbose)
      mutt_error("%s", msg);
  }
  else
  {
    if (verbose)
      mutt_message("%s", msg);
  }
  FREE(&msg);

  return 0;
}

/**
 * query_menu - Get the user to enter an Address Query
 * @param buf    Buffer for the query
 * @param buflen Length of buffer
 * @param all    Alias List
 * @param retbuf If true, populate the results
 */
static void query_menu(char *buf, size_t buflen, struct AliasList *all, bool retbuf)
{
  struct AliasMenuData *mdata = menu_data_new();
  struct Alias *np = NULL;
  TAILQ_FOREACH(np, all, entries)
  {
    menu_data_alias_add(mdata, np);
  }
  menu_data_sort(mdata);

  struct Menu *menu = NULL;
  char title[256];

  snprintf(title, sizeof(title), _("Query '%s'"), buf);

  struct MuttWindow *dlg =
      mutt_window_new(WT_DLG_QUERY, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
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

  dialog_push(dlg);

  menu = mutt_menu_new(MENU_QUERY);

  menu->pagelen = index->state.rows;
  menu->win_index = index;
  menu->win_ibar = ibar;

  menu->make_entry = query_make_entry;
  menu->search = query_search;
  menu->tag = query_tag;
  menu->title = title;
  char helpstr[1024];
  menu->help = mutt_compile_help(helpstr, sizeof(helpstr), MENU_QUERY, QueryHelp);
  menu->max = mdata->num_views;
  menu->mdata = mdata;
  mutt_menu_push_current(menu);

  int done = 0;
  while (done == 0)
  {
    const int op = mutt_menu_loop(menu);
    switch (op)
    {
      case OP_QUERY_APPEND:
      case OP_QUERY:
      {
        if ((mutt_get_field(_("Query: "), buf, buflen, MUTT_COMP_NO_FLAGS) != 0) ||
            (buf[0] == '\0'))
        {
          break;
        }

        if (op == OP_QUERY)
        {
          menu_data_clear(mdata);
          aliaslist_free(all);
        }

        struct AliasList al = TAILQ_HEAD_INITIALIZER(al);
        query_run(buf, true, &al);
        menu->redraw = REDRAW_FULL;
        if (TAILQ_EMPTY(&al))
          break;

        snprintf(title, sizeof(title), _("Query '%s'"), buf);

        struct Alias *tmp = NULL;
        TAILQ_FOREACH_SAFE(np, &al, entries, tmp)
        {
          menu_data_alias_add(mdata, np);
          TAILQ_REMOVE(&al, np, entries);
          TAILQ_INSERT_TAIL(all, np, entries); // Transfer
        }
        menu_data_sort(mdata);
        menu->max = mdata->num_views;
        break;
      }

      case OP_CREATE_ALIAS:
        if (menu->tagprefix)
        {
          struct AddressList naddr = TAILQ_HEAD_INITIALIZER(naddr);

          for (int i = 0; i < menu->max; i++)
          {
            if (mdata->av[i]->is_tagged)
            {
              struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
              if (alias_to_addrlist(&al, mdata->av[i]->alias))
              {
                mutt_addrlist_copy(&naddr, &al, false);
                mutt_addrlist_clear(&al);
              }
            }
          }

          alias_create(&naddr);
          mutt_addrlist_clear(&naddr);
        }
        else
        {
          struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
          if (alias_to_addrlist(&al, mdata->av[menu->current]->alias))
          {
            alias_create(&al);
            mutt_addrlist_clear(&al);
          }
        }
        break;

      case OP_GENERIC_SELECT_ENTRY:
        if (retbuf)
        {
          done = 2;
          break;
        }
      /* fallthrough */
      case OP_MAIL:
      {
        struct Email *e = email_new();
        e->env = mutt_env_new();
        if (menu->tagprefix)
        {
          for (int i = 0; i < menu->max; i++)
          {
            if (mdata->av[i]->is_tagged)
            {
              struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
              if (alias_to_addrlist(&al, mdata->av[i]->alias))
              {
                mutt_addrlist_copy(&e->env->to, &al, false);
                mutt_addrlist_clear(&al);
              }
            }
          }
        }
        else
        {
          struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
          if (alias_to_addrlist(&al, mdata->av[menu->current]->alias))
          {
            mutt_addrlist_copy(&e->env->to, &al, false);
            mutt_addrlist_clear(&al);
          }
        }
        mutt_send_message(SEND_NO_FLAGS, e, NULL, Context, NULL, NeoMutt->sub);
        menu->redraw = REDRAW_FULL;
        break;
      }

      case OP_EXIT:
        done = 1;
        break;
    }
  }

  /* if we need to return the selected entries */
  if (retbuf && (done == 2))
  {
    bool tagged = false;
    size_t curpos = 0;

    memset(buf, 0, buflen);

    /* check for tagged entries */
    for (int i = 0; i < menu->max; i++)
    {
      if (!mdata->av[i]->is_tagged)
        continue;

      if (curpos == 0)
      {
        struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
        if (alias_to_addrlist(&al, mdata->av[i]->alias))
        {
          mutt_addrlist_to_local(&al);
          tagged = true;
          mutt_addrlist_write(&al, buf, buflen, false);
          curpos = mutt_str_len(buf);
          mutt_addrlist_clear(&al);
        }
      }
      else if (curpos + 2 < buflen)
      {
        struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
        if (alias_to_addrlist(&al, mdata->av[i]->alias))
        {
          mutt_addrlist_to_local(&al);
          strcat(buf, ", ");
          mutt_addrlist_write(&al, buf + curpos + 2, buflen - curpos - 2, false);
          curpos = mutt_str_len(buf);
          mutt_addrlist_clear(&al);
        }
      }
    }
    /* then enter current message */
    if (!tagged)
    {
      struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
      if (alias_to_addrlist(&al, mdata->av[menu->current]->alias))
      {
        mutt_addrlist_to_local(&al);
        mutt_addrlist_write(&al, buf, buflen, false);
        mutt_addrlist_clear(&al);
      }
    }
  }

  mutt_menu_pop_current(menu);
  mutt_menu_free(&menu);
  dialog_pop();
  mutt_window_free(&dlg);
  menu_data_free(&mdata);
}

/**
 * query_complete - Perform auto-complete using an Address Query
 * @param buf    Buffer for completion
 * @param buflen Length of buffer
 * @retval 0 Always
 */
int query_complete(char *buf, size_t buflen)
{
  if (!C_QueryCommand)
  {
    mutt_warning(_("Query command not defined"));
    return 0;
  }

  struct AliasList al = TAILQ_HEAD_INITIALIZER(al);
  query_run(buf, true, &al);
  if (TAILQ_EMPTY(&al))
    return 0;

  struct Alias *a_first = TAILQ_FIRST(&al);
  if (!TAILQ_NEXT(a_first, entries)) // only one response?
  {
    struct AddressList addr = TAILQ_HEAD_INITIALIZER(addr);
    if (alias_to_addrlist(&addr, a_first))
    {
      mutt_addrlist_to_local(&addr);
      buf[0] = '\0';
      mutt_addrlist_write(&addr, buf, buflen, false);
      mutt_addrlist_clear(&addr);
      aliaslist_free(&al);
      mutt_clear_error();
    }
    return 0;
  }

  /* multiple results, choose from query menu */
  query_menu(buf, buflen, &al, true);
  aliaslist_free(&al);
  return 0;
}

/**
 * query_index - Perform an Alias Query and display the results
 */
void query_index(void)
{
  if (!C_QueryCommand)
  {
    mutt_warning(_("Query command not defined"));
    return;
  }

  char buf[256] = { 0 };
  if ((mutt_get_field(_("Query: "), buf, sizeof(buf), MUTT_COMP_NO_FLAGS) != 0) ||
      (buf[0] == '\0'))
  {
    return;
  }

  struct AliasList al = TAILQ_HEAD_INITIALIZER(al);
  query_run(buf, false, &al);
  if (TAILQ_EMPTY(&al))
    return;

  query_menu(buf, sizeof(buf), &al, false);
  aliaslist_free(&al);
}
