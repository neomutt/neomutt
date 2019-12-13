/**
 * @file
 * Routines for querying and external address book
 *
 * @authors
 * Copyright (C) 1996-2000,2003,2013 Michael R. Elkins <me@mutt.org>
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
 * @page query Routines for querying and external address book
 *
 * Routines for querying and external address book
 */

#include "config.h"
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "mutt/mutt.h"
#include "address/lib.h"
#include "email/lib.h"
#include "mutt.h"
#include "query.h"
#include "alias.h"
#include "curs_lib.h"
#include "filter.h"
#include "format_flags.h"
#include "globals.h"
#include "keymap.h"
#include "mutt_logging.h"
#include "mutt_menu.h"
#include "mutt_window.h"
#include "muttlib.h"
#include "opcodes.h"
#include "send.h"

/* These Config Variables are only used in query.c */
char *C_QueryCommand; ///< Config: External command to query and external address book
char *C_QueryFormat; ///< Config: printf-like format string for the query menu (address book)

/**
 * struct Query - An entry from an external address-book
 */
struct Query
{
  int num;
  struct AddressList addr;
  char *name;
  char *other;
  struct Query *next;
};

/**
 * struct QueryEntry - An entry in a selectable list of Query's
 */
struct QueryEntry
{
  bool tagged;
  struct Query *data;
};

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
 * result_to_addr - Turn a Query into an AddressList
 * @param al AddressList to fill (must be empty)
 * @param r Query to use
 * @retval bool True on success
 */
static bool result_to_addr(struct AddressList *al, struct Query *r)
{
  if (!al || !TAILQ_EMPTY(al) || !r)
    return false;

  mutt_addrlist_copy(al, &r->addr, false);
  if (!TAILQ_EMPTY(al))
  {
    struct Address *first = TAILQ_FIRST(al);
    struct Address *second = TAILQ_NEXT(first, entries);
    if (!second && !first->personal)
      first->personal = mutt_str_strdup(r->name);

    mutt_addrlist_to_intl(al, NULL);
  }

  return true;
}

/**
 * query_new - Create a new query
 * @retval A newly allocated query
 */
static struct Query *query_new(void)
{
  struct Query *query = mutt_mem_calloc(1, sizeof(struct Query));
  TAILQ_INIT(&query->addr);
  return query;
}

/**
 * query_free - Free a Query
 * @param[out] query Query to free
 */
static void query_free(struct Query **query)
{
  if (!query)
    return;

  struct Query *p = NULL;

  while (*query)
  {
    p = *query;
    *query = (*query)->next;

    mutt_addrlist_clear(&p->addr);
    FREE(&p->name);
    FREE(&p->other);
    FREE(&p);
  }
}

/**
 * run_query - Run an external program to find Addresses
 * @param s     String to match
 * @param quiet If true, don't print progress messages
 * @retval ptr Query List of results
 */
static struct Query *run_query(char *s, int quiet)
{
  FILE *fp = NULL;
  struct Query *first = NULL;
  struct Query *cur = NULL;
  char *buf = NULL;
  size_t buflen;
  int dummy = 0;
  char *msg = NULL;
  size_t msglen = 0;
  char *p = NULL;
  pid_t pid;
  struct Buffer *cmd = mutt_buffer_pool_get();

  mutt_buffer_file_expand_fmt_quote(cmd, C_QueryCommand, s);

  pid = mutt_create_filter(mutt_b2s(cmd), NULL, &fp, NULL);
  if (pid < 0)
  {
    mutt_debug(LL_DEBUG1, "unable to fork command: %s\n", mutt_b2s(cmd));
    mutt_buffer_pool_release(&cmd);
    return 0;
  }
  mutt_buffer_pool_release(&cmd);

  if (!quiet)
    mutt_message(_("Waiting for response..."));

  /* The query protocol first reads one NL-terminated line. If an error
   * occurs, this is assumed to be an error message. Otherwise it's ignored. */
  msg = mutt_file_read_line(msg, &msglen, fp, &dummy, 0);
  while ((buf = mutt_file_read_line(buf, &buflen, fp, &dummy, 0)))
  {
    p = strtok(buf, "\t\n");
    if (p)
    {
      if (first)
      {
        cur->next = query_new();
        cur = cur->next;
      }
      else
      {
        first = query_new();
        cur = first;
      }

      mutt_addrlist_parse(&cur->addr, p);
      p = strtok(NULL, "\t\n");
      if (p)
      {
        cur->name = mutt_str_strdup(p);
        p = strtok(NULL, "\t\n");
        if (p)
          cur->other = mutt_str_strdup(p);
      }
    }
  }
  FREE(&buf);
  mutt_file_fclose(&fp);
  if (mutt_wait_filter(pid))
  {
    mutt_debug(LL_DEBUG1, "Error: %s\n", msg);
    if (!quiet)
      mutt_error("%s", msg);
  }
  else
  {
    if (!quiet)
      mutt_message("%s", msg);
  }
  FREE(&msg);

  return first;
}

/**
 * query_search - Search a Address menu item - Implements Menu::menu_search()
 *
 * Try to match various Address fields.
 */
static int query_search(struct Menu *menu, regex_t *rx, int line)
{
  struct QueryEntry *table = menu->data;
  struct Query *query = table[line].data;

  if (query->name && !regexec(rx, query->name, 0, NULL, 0))
    return 0;
  if (query->other && !regexec(rx, query->other, 0, NULL, 0))
    return 0;
  if (!TAILQ_EMPTY(&query->addr))
  {
    struct Address *addr = TAILQ_FIRST(&query->addr);
    if (addr->personal && !regexec(rx, addr->personal, 0, NULL, 0))
    {
      return 0;
    }
    if (addr->mailbox && !regexec(rx, addr->mailbox, 0, NULL, 0))
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
                                    unsigned long data, MuttFormatFlags flags)
{
  struct QueryEntry *entry = (struct QueryEntry *) data;
  struct Query *query = entry->data;
  char fmt[128];
  char tmp[256] = { 0 };
  bool optional = (flags & MUTT_FORMAT_OPTIONAL);

  switch (op)
  {
    case 'a':
      mutt_addrlist_write(tmp, sizeof(tmp), &query->addr, true);
      mutt_format_s(buf, buflen, prec, tmp);
      break;
    case 'c':
      snprintf(fmt, sizeof(fmt), "%%%sd", prec);
      snprintf(buf, buflen, fmt, query->num + 1);
      break;
    case 'e':
      if (!optional)
        mutt_format_s(buf, buflen, prec, NONULL(query->other));
      else if (!query->other || !*query->other)
        optional = false;
      break;
    case 'n':
      mutt_format_s(buf, buflen, prec, NONULL(query->name));
      break;
    case 't':
      snprintf(fmt, sizeof(fmt), "%%%sc", prec);
      snprintf(buf, buflen, fmt, entry->tagged ? '*' : ' ');
      break;
    default:
      snprintf(fmt, sizeof(fmt), "%%%sc", prec);
      snprintf(buf, buflen, fmt, op);
      break;
  }

  if (optional)
    mutt_expando_format(buf, buflen, col, cols, if_str, query_format_str, data,
                        MUTT_FORMAT_NO_FLAGS);
  else if (flags & MUTT_FORMAT_OPTIONAL)
    mutt_expando_format(buf, buflen, col, cols, else_str, query_format_str,
                        data, MUTT_FORMAT_NO_FLAGS);

  return src;
}

/**
 * query_make_entry - Format a menu item for the query list - Implements Menu::menu_make_entry()
 */
static void query_make_entry(char *buf, size_t buflen, struct Menu *menu, int line)
{
  struct QueryEntry *entry = &((struct QueryEntry *) menu->data)[line];

  entry->data->num = line;
  mutt_expando_format(buf, buflen, 0, menu->indexwin->cols, NONULL(C_QueryFormat),
                      query_format_str, (unsigned long) entry, MUTT_FORMAT_ARROWCURSOR);
}

/**
 * query_tag - Tag an entry in the Query Menu - Implements Menu::menu_tag()
 */
static int query_tag(struct Menu *menu, int sel, int act)
{
  struct QueryEntry *cur = &((struct QueryEntry *) menu->data)[sel];
  bool ot = cur->tagged;

  cur->tagged = ((act >= 0) ? act : !cur->tagged);
  return cur->tagged - ot;
}

/**
 * query_menu - Get the user to enter an Address Query
 * @param buf     Buffer for the query
 * @param buflen  Length of buffer
 * @param results Query List
 * @param retbuf  If true, populate the results
 */
static void query_menu(char *buf, size_t buflen, struct Query *results, bool retbuf)
{
  struct Menu *menu = NULL;
  struct QueryEntry *query_table = NULL;
  struct Query *queryp = NULL;
  char title[256];

  if (!results)
  {
    /* Prompt for Query */
    if ((mutt_get_field(_("Query: "), buf, buflen, 0) == 0) && (buf[0] != '\0'))
    {
      results = run_query(buf, 0);
      if (!results)
        return;
    }
  }

  snprintf(title, sizeof(title), _("Query '%s'"), buf);

  menu = mutt_menu_new(MENU_QUERY);
  menu->menu_make_entry = query_make_entry;
  menu->menu_search = query_search;
  menu->menu_tag = query_tag;
  menu->title = title;
  char helpstr[1024];
  menu->help = mutt_compile_help(helpstr, sizeof(helpstr), MENU_QUERY, QueryHelp);
  mutt_menu_push_current(menu);

  /* count the number of results */
  for (queryp = results; queryp; queryp = queryp->next)
    menu->max++;

  query_table = mutt_mem_calloc(menu->max, sizeof(struct QueryEntry));
  menu->data = query_table;

  queryp = results;
  for (int i = 0; queryp; queryp = queryp->next, i++)
    query_table[i].data = queryp;

  int done = 0;
  while (done == 0)
  {
    const int op = mutt_menu_loop(menu);
    switch (op)
    {
      case OP_QUERY_APPEND:
      case OP_QUERY:
        if ((mutt_get_field(_("Query: "), buf, buflen, 0) == 0) && (buf[0] != '\0'))
        {
          struct Query *newresults = run_query(buf, 0);

          menu->redraw = REDRAW_FULL;
          if (newresults)
          {
            snprintf(title, sizeof(title), _("Query '%s'"), buf);

            if (op == OP_QUERY)
            {
              query_free(&results);
              results = newresults;
              FREE(&query_table);
            }
            else
            {
              /* append */
              for (queryp = results; queryp->next; queryp = queryp->next)
                ;

              queryp->next = newresults;
            }

            menu->current = 0;
            mutt_menu_pop_current(menu);
            mutt_menu_free(&menu);
            menu = mutt_menu_new(MENU_QUERY);
            menu->menu_make_entry = query_make_entry;
            menu->menu_search = query_search;
            menu->menu_tag = query_tag;
            menu->title = title;
            menu->help = mutt_compile_help(helpstr, sizeof(helpstr), MENU_QUERY, QueryHelp);
            mutt_menu_push_current(menu);

            /* count the number of results */
            for (queryp = results; queryp; queryp = queryp->next)
              menu->max++;

            if (op == OP_QUERY)
            {
              menu->data = query_table =
                  mutt_mem_calloc(menu->max, sizeof(struct QueryEntry));

              queryp = results;
              for (int i = 0; queryp; queryp = queryp->next, i++)
                query_table[i].data = queryp;
            }
            else
            {
              bool clear = false;

              /* append */
              mutt_mem_realloc(&query_table, menu->max * sizeof(struct QueryEntry));

              menu->data = query_table;

              queryp = results;
              for (int i = 0; queryp; queryp = queryp->next, i++)
              {
                /* once we hit new entries, clear/init the tag */
                if (queryp == newresults)
                  clear = true;

                query_table[i].data = queryp;
                if (clear)
                  query_table[i].tagged = false;
              }
            }
          }
        }
        break;

      case OP_CREATE_ALIAS:
        if (menu->tagprefix)
        {
          struct AddressList naddr = TAILQ_HEAD_INITIALIZER(naddr);

          for (int i = 0; i < menu->max; i++)
          {
            if (query_table[i].tagged)
            {
              struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
              if (result_to_addr(&al, query_table[i].data))
              {
                mutt_addrlist_copy(&naddr, &al, false);
                mutt_addrlist_clear(&al);
              }
            }
          }

          mutt_alias_create(NULL, &naddr);
          mutt_addrlist_clear(&naddr);
        }
        else
        {
          struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
          if (result_to_addr(&al, query_table[menu->current].data))
          {
            mutt_alias_create(NULL, &al);
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
        if (!menu->tagprefix)
        {
          struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
          if (result_to_addr(&al, query_table[menu->current].data))
          {
            mutt_addrlist_copy(&e->env->to, &al, false);
            mutt_addrlist_clear(&al);
          }
        }
        else
        {
          for (int i = 0; i < menu->max; i++)
          {
            if (query_table[i].tagged)
            {
              struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
              if (result_to_addr(&al, query_table[i].data))
              {
                mutt_addrlist_copy(&e->env->to, &al, false);
                mutt_addrlist_clear(&al);
              }
            }
          }
        }
        ci_send_message(SEND_NO_FLAGS, e, NULL, Context, NULL);
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
      if (query_table[i].tagged)
      {
        if (curpos == 0)
        {
          struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
          if (result_to_addr(&al, query_table[i].data))
          {
            mutt_addrlist_to_local(&al);
            tagged = true;
            mutt_addrlist_write(buf, buflen, &al, false);
            curpos = mutt_str_strlen(buf);
            mutt_addrlist_clear(&al);
          }
        }
        else if (curpos + 2 < buflen)
        {
          struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
          if (result_to_addr(&al, query_table[i].data))
          {
            mutt_addrlist_to_local(&al);
            strcat(buf, ", ");
            mutt_addrlist_write(buf + curpos + 1, buflen - curpos - 1, &al, false);
            curpos = mutt_str_strlen(buf);
            mutt_addrlist_clear(&al);
          }
        }
      }
    }
    /* then enter current message */
    if (!tagged)
    {
      struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
      if (result_to_addr(&al, query_table[menu->current].data))
      {
        mutt_addrlist_to_local(&al);
        mutt_addrlist_write(buf, buflen, &al, false);
        mutt_addrlist_clear(&al);
      }
    }
  }

  query_free(&results);
  FREE(&query_table);
  mutt_menu_pop_current(menu);
  mutt_menu_free(&menu);
}

/**
 * mutt_query_complete - Perform auto-complete using an Address Query
 * @param buf    Buffer for completion
 * @param buflen Length of buffer
 * @retval 0 Always
 */
int mutt_query_complete(char *buf, size_t buflen)
{
  if (!C_QueryCommand)
  {
    mutt_error(_("Query command not defined"));
    return 0;
  }

  struct Query *results = run_query(buf, 1);
  if (results)
  {
    /* only one response? */
    if (!results->next)
    {
      struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
      if (result_to_addr(&al, results))
      {
        mutt_addrlist_to_local(&al);
        buf[0] = '\0';
        mutt_addrlist_write(buf, buflen, &al, false);
        mutt_addrlist_clear(&al);
        query_free(&results);
        mutt_clear_error();
      }
      return 0;
    }
    /* multiple results, choose from query menu */
    query_menu(buf, buflen, results, true);
  }
  return 0;
}

/**
 * mutt_query_menu - Show the user the results of a Query
 * @param buf    Buffer for the query
 * @param buflen Length of buffer
 */
void mutt_query_menu(char *buf, size_t buflen)
{
  if (!C_QueryCommand)
  {
    mutt_error(_("Query command not defined"));
    return;
  }

  if (!buf)
  {
    char tmp[256] = { 0 };

    query_menu(tmp, sizeof(tmp), NULL, false);
  }
  else
  {
    query_menu(buf, buflen, NULL, true);
  }
}
