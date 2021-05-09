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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "lib.h"
#include "menu/lib.h"
#include "pattern/lib.h"
#include "send/lib.h"
#include "alias.h"
#include "context.h"
#include "format_flags.h"
#include "gui.h"
#include "keymap.h"
#include "mutt_globals.h"
#include "mutt_logging.h"
#include "muttlib.h"
#include "opcodes.h"

/// Help Bar for the Address Query dialog
static const struct Mapping QueryHelp[] = {
  // clang-format off
  { N_("Exit"),       OP_EXIT },
  { N_("Mail"),       OP_MAIL },
  { N_("New Query"),  OP_QUERY },
  { N_("Make Alias"), OP_CREATE_ALIAS },
  { N_("Sort"),       OP_SORT },
  { N_("Rev-Sort"),   OP_SORT_REVERSE },
  { N_("Search"),     OP_SEARCH },
  { N_("Help"),       OP_HELP },
  { NULL, 0 },
  // clang-format on
};

/**
 * alias_to_addrlist - Turn an Alias into an AddressList
 * @param al    AddressList to fill (must be empty)
 * @param alias Alias to use
 * @retval true Success
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
  const struct AliasViewArray *ava = &((struct AliasMenuData *) menu->mdata)->ava;
  struct AliasView *av = ARRAY_GET(ava, line);
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

  /* We return the format string, unchanged */
  return src;
}

/**
 * query_make_entry - Format a menu item for the query list - Implements Menu::make_entry()
 */
static void query_make_entry(struct Menu *menu, char *buf, size_t buflen, int line)
{
  const struct AliasMenuData *mdata = menu->mdata;
  const struct AliasViewArray *ava = &mdata->ava;
  struct AliasView *av = ARRAY_GET(ava, line);

  const char *const query_format = cs_subset_string(mdata->sub, "query_format");

  mutt_expando_format(buf, buflen, 0, menu->win_index->state.cols, NONULL(query_format),
                      query_format_str, (intptr_t) av, MUTT_FORMAT_ARROWCURSOR);
}

/**
 * query_tag - Tag an entry in the Query Menu - Implements Menu::tag()
 */
static int query_tag(struct Menu *menu, int sel, int act)
{
  const struct AliasMenuData *mdata = menu->mdata;
  const struct AliasViewArray *ava = &mdata->ava;
  struct AliasView *av = ARRAY_GET(ava, sel);

  bool ot = av->is_tagged;

  av->is_tagged = ((act >= 0) ? act : !av->is_tagged);
  return av->is_tagged - ot;
}

/**
 * query_run - Run an external program to find Addresses
 * @param s       String to match
 * @param verbose If true, print progress messages
 * @param al      Alias list to fill
 * @param sub     Config items
 * @retval  0 Success
 * @retval -1 Error
 */
static int query_run(char *s, bool verbose, struct AliasList *al,
                     const struct ConfigSubset *sub)
{
  FILE *fp = NULL;
  char *buf = NULL;
  size_t buflen;
  char *msg = NULL;
  size_t msglen = 0;
  char *p = NULL;
  struct Buffer *cmd = mutt_buffer_pool_get();

  const char *const query_command = cs_subset_string(sub, "query_command");
  mutt_buffer_file_expand_fmt_quote(cmd, query_command, s);

  pid_t pid = filter_create(mutt_buffer_string(cmd), NULL, &fp, NULL);
  if (pid < 0)
  {
    mutt_debug(LL_DEBUG1, "unable to fork command: %s\n", mutt_buffer_string(cmd));
    mutt_buffer_pool_release(&cmd);
    return -1;
  }
  mutt_buffer_pool_release(&cmd);

  if (verbose)
    mutt_message(_("Waiting for response..."));

  /* The query protocol first reads one NL-terminated line. If an error
   * occurs, this is assumed to be an error message. Otherwise it's ignored. */
  msg = mutt_file_read_line(msg, &msglen, fp, NULL, MUTT_RL_NO_FLAGS);
  while ((buf = mutt_file_read_line(buf, &buflen, fp, NULL, MUTT_RL_NO_FLAGS)))
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
 * dlg_select_query - Get the user to enter an Address Query
 * @param buf    Buffer for the query
 * @param buflen Length of buffer
 * @param all    Alias List
 * @param retbuf If true, populate the results
 * @param sub    Config items
 */
static void dlg_select_query(char *buf, size_t buflen, struct AliasList *all,
                             bool retbuf, struct ConfigSubset *sub)
{
  struct AliasMenuData mdata = { NULL, ARRAY_HEAD_INITIALIZER, sub };

  struct Alias *np = NULL;
  TAILQ_FOREACH(np, all, entries)
  {
    alias_array_alias_add(&mdata.ava, np);
  }
  alias_array_sort(&mdata.ava, mdata.sub);

  char title[256];
  snprintf(title, sizeof(title), "%s%s", _("Query: "), buf);

  struct MuttWindow *dlg = dialog_create_simple_index(MENU_QUERY, WT_DLG_QUERY, QueryHelp);
  struct MuttWindow *sbar = mutt_window_find(dlg, WT_INDEX_BAR);

  struct Menu *menu = dlg->wdata;
  menu->make_entry = query_make_entry;
  menu->search = query_search;
  menu->custom_search = true;
  menu->tag = query_tag;
  menu->max = ARRAY_SIZE(&mdata.ava);
  menu->mdata = &mdata;

  notify_observer_add(NeoMutt->notify, NT_CONFIG, alias_config_observer, &mdata);
  notify_observer_add(NeoMutt->notify, NT_COLOR, alias_color_observer, menu);

  short sort_alias = cs_subset_sort(sub, "sort_alias");

  int done = 0;
  while (done == 0)
  {
    const int op = menu_loop(menu);
    switch (op)
    {
      case OP_QUERY_APPEND:
      case OP_QUERY:
      {
        if ((mutt_get_field(_("Query: "), buf, buflen, MUTT_COMP_NO_FLAGS,
                            false, NULL, NULL) != 0) ||
            (buf[0] == '\0'))
        {
          break;
        }

        if (op == OP_QUERY)
        {
          ARRAY_FREE(&mdata.ava);
          aliaslist_free(all);
        }

        struct AliasList al = TAILQ_HEAD_INITIALIZER(al);
        query_run(buf, true, &al, sub);
        menu_queue_redraw(menu, REDRAW_FULL);
        snprintf(title, sizeof(title), "%s%s", _("Query: "), buf);
        sbar_set_title(sbar, title);

        if (TAILQ_EMPTY(&al))
        {
          menu->max = 0;
          break;
        }

        struct Alias *tmp = NULL;
        TAILQ_FOREACH_SAFE(np, &al, entries, tmp)
        {
          alias_array_alias_add(&mdata.ava, np);
          TAILQ_REMOVE(&al, np, entries);
          TAILQ_INSERT_TAIL(all, np, entries); // Transfer
        }
        alias_array_sort(&mdata.ava, mdata.sub);
        menu->max = ARRAY_SIZE(&mdata.ava);
        break;
      }

      case OP_CREATE_ALIAS:
        if (menu->tagprefix)
        {
          struct AddressList naddr = TAILQ_HEAD_INITIALIZER(naddr);

          struct AliasView *avp = NULL;
          ARRAY_FOREACH(avp, &mdata.ava)
          {
            if (avp->is_tagged)
            {
              struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
              if (alias_to_addrlist(&al, avp->alias))
              {
                mutt_addrlist_copy(&naddr, &al, false);
                mutt_addrlist_clear(&al);
              }
            }
          }

          alias_create(&naddr, sub);
          mutt_addrlist_clear(&naddr);
        }
        else
        {
          struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
          if (alias_to_addrlist(&al, ARRAY_GET(&mdata.ava, menu_get_index(menu))->alias))
          {
            alias_create(&al, sub);
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
          struct AliasView *avp = NULL;
          ARRAY_FOREACH(avp, &mdata.ava)
          {
            if (avp->is_tagged)
            {
              struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
              if (alias_to_addrlist(&al, avp->alias))
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
          if (alias_to_addrlist(&al, ARRAY_GET(&mdata.ava, menu_get_index(menu))->alias))
          {
            mutt_addrlist_copy(&e->env->to, &al, false);
            mutt_addrlist_clear(&al);
          }
        }
        mutt_send_message(SEND_NO_FLAGS, e, NULL, ctx_mailbox(Context), NULL,
                          NeoMutt->sub);
        menu_queue_redraw(menu, REDRAW_FULL);
        break;
      }

      case OP_SORT:
      case OP_SORT_REVERSE:
      {
        int sort = sort_alias;
        bool resort = true;
        bool reverse = (op == OP_SORT_REVERSE);

        switch (mutt_multi_choice(
            reverse ?
                /* L10N: The highlighted letters must match the "Sort" options */
                _("Rev-Sort (a)lias, a(d)dress or (u)nsorted?") :
                /* L10N: The highlighted letters must match the "Rev-Sort" options */
                _("Sort (a)lias, a(d)dress or (u)nsorted?"),
            /* L10N: These must match the highlighted letters from "Sort" and "Rev-Sort" */
            _("adu")))
        {
          case -1: /* abort */
            resort = false;
            break;

          case 1: /* (a)lias */
            sort = SORT_ALIAS;
            break;

          case 2: /* a(d)dress */
            sort = SORT_ADDRESS;
            break;

          case 3: /* (u)nsorted */
            sort = SORT_ORDER;
            break;
        }

        if (resort)
        {
          sort |= reverse ? SORT_REVERSE : 0;

          cs_subset_str_native_set(sub, "sort_alias", sort, NULL);
          menu_queue_redraw(menu, REDRAW_FULL);
        }

        break;
      }

      case OP_SEARCH_REVERSE:
      case OP_SEARCH_NEXT:
      case OP_SEARCH_OPPOSITE:
      case OP_SEARCH:
      {
        int index = mutt_search_alias_command(menu, menu_get_index(menu), op);
        if (index == -1)
          break;

        menu_set_index(menu, index);
        break;
      }

      case OP_MAIN_LIMIT:
      {
        int rc = mutt_pattern_alias_func(MUTT_LIMIT, _("Limit to messages matching: "),
                                         &mdata, menu);
        if (rc == 0)
        {
          alias_array_sort(&mdata.ava, mdata.sub);
          char *title2 = menu_create_alias_title(_("Query"), mdata.str);
          sbar_set_title(sbar, title2);
          FREE(&title2);
          menu_queue_redraw(menu, REDRAW_FULL);
        }

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
    struct AliasView *avp = NULL;
    ARRAY_FOREACH(avp, &mdata.ava)
    {
      if (!avp->is_tagged)
        continue;

      if (curpos == 0)
      {
        struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
        if (alias_to_addrlist(&al, avp->alias))
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
        if (alias_to_addrlist(&al, avp->alias))
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
      if (alias_to_addrlist(&al, ARRAY_GET(&mdata.ava, menu_get_index(menu))->alias))
      {
        mutt_addrlist_to_local(&al);
        mutt_addrlist_write(&al, buf, buflen, false);
        mutt_addrlist_clear(&al);
      }
    }
  }

  notify_observer_remove(NeoMutt->notify, alias_config_observer, &mdata);
  notify_observer_remove(NeoMutt->notify, alias_color_observer, menu);

  dialog_destroy_simple_index(&dlg);
  ARRAY_FREE(&mdata.ava);
}

/**
 * query_complete - Perform auto-complete using an Address Query
 * @param buf    Buffer for completion
 * @param buflen Length of buffer
 * @param sub    Config item
 * @retval 0 Always
 */
int query_complete(char *buf, size_t buflen, struct ConfigSubset *sub)
{
  const char *const query_command = cs_subset_string(sub, "query_command");
  if (!query_command)
  {
    mutt_warning(_("Query command not defined"));
    return 0;
  }

  struct AliasList al = TAILQ_HEAD_INITIALIZER(al);
  query_run(buf, true, &al, sub);
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
  dlg_select_query(buf, buflen, &al, true, sub);
  aliaslist_free(&al);
  return 0;
}

/**
 * query_index - Perform an Alias Query and display the results
 * @param sub    Config item
 */
void query_index(struct ConfigSubset *sub)
{
  const char *const query_command = cs_subset_string(sub, "query_command");
  if (!query_command)
  {
    mutt_warning(_("Query command not defined"));
    return;
  }

  char buf[256] = { 0 };
  if ((mutt_get_field(_("Query: "), buf, sizeof(buf), MUTT_COMP_NO_FLAGS, false,
                      NULL, NULL) != 0) ||
      (buf[0] == '\0'))
  {
    return;
  }

  struct AliasList al = TAILQ_HEAD_INITIALIZER(al);
  query_run(buf, false, &al, sub);
  if (TAILQ_EMPTY(&al))
    return;

  dlg_select_query(buf, sizeof(buf), &al, false, sub);
  aliaslist_free(&al);
}
