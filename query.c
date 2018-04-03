/**
 * @file
 * Routines for querying and external address book
 *
 * @authors
 * Copyright (C) 1996-2000,2003,2013 Michael R. Elkins <me@mutt.org>
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
#include <limits.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "mutt/mutt.h"
#include "mutt.h"
#include "alias.h"
#include "envelope.h"
#include "filter.h"
#include "format_flags.h"
#include "globals.h"
#include "header.h"
#include "keymap.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "mutt_window.h"
#include "opcodes.h"
#include "protos.h"

/**
 * struct Query - An entry from an external address-book
 */
struct Query
{
  int num;
  struct Address *addr;
  char *name;
  char *other;
  struct Query *next;
};

/**
 * struct Entry - An entry in a selectable list of Query's
 */
struct Entry
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

static struct Address *result_to_addr(struct Query *r)
{
  static struct Address *tmp = NULL;

  tmp = mutt_addr_copy_list(r->addr, false);
  if (!tmp)
    return NULL;

  if (!tmp->next && !tmp->personal)
    tmp->personal = mutt_str_strdup(r->name);

  mutt_addrlist_to_intl(tmp, NULL);
  return tmp;
}

static void free_query(struct Query **query)
{
  struct Query *p = NULL;

  if (!query)
    return;

  while (*query)
  {
    p = *query;
    *query = (*query)->next;

    mutt_addr_free(&p->addr);
    FREE(&p->name);
    FREE(&p->other);
    FREE(&p);
  }
}

static struct Query *run_query(char *s, int quiet)
{
  FILE *fp = NULL;
  struct Query *first = NULL;
  struct Query *cur = NULL;
  char cmd[_POSIX_PATH_MAX];
  char *buf = NULL;
  size_t buflen;
  int dummy = 0;
  char msg[STRING];
  char *p = NULL;
  pid_t thepid;

  mutt_expand_file_fmt(cmd, sizeof(cmd), QueryCommand, s);

  thepid = mutt_create_filter(cmd, NULL, &fp, NULL);
  if (thepid < 0)
  {
    mutt_debug(1, "unable to fork command: %s\n", cmd);
    return 0;
  }
  if (!quiet)
    mutt_message(_("Waiting for response..."));
  fgets(msg, sizeof(msg), fp);
  p = strrchr(msg, '\n');
  if (p)
    *p = '\0';
  while ((buf = mutt_file_read_line(buf, &buflen, fp, &dummy, 0)) != NULL)
  {
    p = strtok(buf, "\t\n");
    if (p)
    {
      if (!first)
      {
        first = mutt_mem_calloc(1, sizeof(struct Query));
        cur = first;
      }
      else
      {
        cur->next = mutt_mem_calloc(1, sizeof(struct Query));
        cur = cur->next;
      }

      cur->addr = mutt_addr_parse_list(cur->addr, p);
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
  if (mutt_wait_filter(thepid))
  {
    mutt_debug(1, "Error: %s\n", msg);
    if (!quiet)
      mutt_error("%s", msg);
  }
  else
  {
    if (!quiet)
      mutt_message("%s", msg);
  }

  return first;
}

static int query_search(struct Menu *m, regex_t *re, int n)
{
  struct Entry *table = (struct Entry *) m->data;

  if (table[n].data->name && !regexec(re, table[n].data->name, 0, NULL, 0))
    return 0;
  if (table[n].data->other && !regexec(re, table[n].data->other, 0, NULL, 0))
    return 0;
  if (table[n].data->addr)
  {
    if (table[n].data->addr->personal &&
        !regexec(re, table[n].data->addr->personal, 0, NULL, 0))
    {
      return 0;
    }
    if (table[n].data->addr->mailbox &&
        !regexec(re, table[n].data->addr->mailbox, 0, NULL, 0))
    {
      return 0;
    }
  }

  return REG_NOMATCH;
}

/**
 * query_format_str - Format a string for the query menu
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
 * query_format_str() is a callback function for mutt_expando_format().
 *
 * | Expando | Description
 * |:--------|:--------------------------------------------------------
 * | \%a     | Destination address
 * | \%c     | Current entry number
 * | \%e     | Extra information
 * | \%n     | Destination name
 * | \%t     | '*' if current entry is tagged, a space otherwise
 */
static const char *query_format_str(char *buf, size_t buflen, size_t col, int cols,
                                    char op, const char *src, const char *prec,
                                    const char *if_str, const char *else_str,
                                    unsigned long data, enum FormatFlag flags)
{
  struct Entry *entry = (struct Entry *) data;
  struct Query *query = entry->data;
  char fmt[SHORT_STRING];
  char tmp[STRING] = "";
  int optional = (flags & MUTT_FORMAT_OPTIONAL);

  switch (op)
  {
    case 'a':
      mutt_addr_write(tmp, sizeof(tmp), query->addr, true);
      snprintf(fmt, sizeof(fmt), "%%%ss", prec);
      snprintf(buf, buflen, fmt, tmp);
      break;
    case 'c':
      snprintf(fmt, sizeof(fmt), "%%%sd", prec);
      snprintf(buf, buflen, fmt, query->num + 1);
      break;
    case 'e':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, NONULL(query->other));
      }
      else if (!query->other || !*query->other)
        optional = 0;
      break;
    case 'n':
      snprintf(fmt, sizeof(fmt), "%%%ss", prec);
      snprintf(buf, buflen, fmt, NONULL(query->name));
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
    mutt_expando_format(buf, buflen, col, cols, if_str, query_format_str, data, 0);
  else if (flags & MUTT_FORMAT_OPTIONAL)
    mutt_expando_format(buf, buflen, col, cols, else_str, query_format_str, data, 0);

  return src;
}

/**
 * query_entry - Format a menu item for the query list
 * @param[out] buf    Buffer in which to save string
 * @param[in]  buflen Buffer length
 * @param[in]  menu   Menu containing aliases
 * @param[in]  num    Index into the menu
 */
static void query_entry(char *buf, size_t buflen, struct Menu *menu, int num)
{
  struct Entry *entry = &((struct Entry *) menu->data)[num];

  entry->data->num = num;
  mutt_expando_format(buf, buflen, 0, MuttIndexWindow->cols, NONULL(QueryFormat),
                      query_format_str, (unsigned long) entry, MUTT_FORMAT_ARROWCURSOR);
}

static int query_tag(struct Menu *menu, int n, int m)
{
  struct Entry *cur = &((struct Entry *) menu->data)[n];
  bool ot = cur->tagged;

  cur->tagged = m >= 0 ? m : !cur->tagged;
  return cur->tagged - ot;
}

static void query_menu(char *buf, size_t buflen, struct Query *results, int retbuf)
{
  struct Menu *menu = NULL;
  struct Header *msg = NULL;
  struct Entry *QueryTable = NULL;
  struct Query *queryp = NULL;
  char title[STRING];

  if (!results)
  {
    /* Prompt for Query */
    if (mutt_get_field(_("Query: "), buf, buflen, 0) == 0 && buf[0])
    {
      results = run_query(buf, 0);
    }
  }

  if (results)
  {
    snprintf(title, sizeof(title), _("Query '%s'"), buf);

    menu = mutt_menu_new(MENU_QUERY);
    menu->make_entry = query_entry;
    menu->search = query_search;
    menu->tag = query_tag;
    menu->title = title;
    char helpstr[LONG_STRING];
    menu->help = mutt_compile_help(helpstr, sizeof(helpstr), MENU_QUERY, QueryHelp);
    mutt_menu_push_current(menu);

    /* count the number of results */
    for (queryp = results; queryp; queryp = queryp->next)
      menu->max++;

    menu->data = QueryTable = mutt_mem_calloc(menu->max, sizeof(struct Entry));

    int i;
    for (i = 0, queryp = results; queryp; queryp = queryp->next, i++)
      QueryTable[i].data = queryp;

    int done = 0;
    while (!done)
    {
      const int op = mutt_menu_loop(menu);
      switch (op)
      {
        case OP_QUERY_APPEND:
        case OP_QUERY:
          if (mutt_get_field(_("Query: "), buf, buflen, 0) == 0 && buf[0])
          {
            struct Query *newresults = run_query(buf, 0);

            menu->redraw = REDRAW_FULL;
            if (newresults)
            {
              snprintf(title, sizeof(title), _("Query '%s'"), buf);

              if (op == OP_QUERY)
              {
                free_query(&results);
                results = newresults;
                FREE(&QueryTable);
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
              mutt_menu_destroy(&menu);
              menu = mutt_menu_new(MENU_QUERY);
              menu->make_entry = query_entry;
              menu->search = query_search;
              menu->tag = query_tag;
              menu->title = title;
              menu->help = mutt_compile_help(helpstr, sizeof(helpstr), MENU_QUERY, QueryHelp);
              mutt_menu_push_current(menu);

              /* count the number of results */
              for (queryp = results; queryp; queryp = queryp->next)
                menu->max++;

              if (op == OP_QUERY)
              {
                menu->data = QueryTable =
                    mutt_mem_calloc(menu->max, sizeof(struct Entry));

                for (i = 0, queryp = results; queryp; queryp = queryp->next, i++)
                  QueryTable[i].data = queryp;
              }
              else
              {
                bool clear = false;

                /* append */
                mutt_mem_realloc(&QueryTable, menu->max * sizeof(struct Entry));

                menu->data = QueryTable;

                for (i = 0, queryp = results; queryp; queryp = queryp->next, i++)
                {
                  /* once we hit new entries, clear/init the tag */
                  if (queryp == newresults)
                    clear = true;

                  QueryTable[i].data = queryp;
                  if (clear)
                    QueryTable[i].tagged = false;
                }
              }
            }
          }
          break;

        case OP_CREATE_ALIAS:
          if (menu->tagprefix)
          {
            struct Address *naddr = NULL;

            for (i = 0; i < menu->max; i++)
            {
              if (QueryTable[i].tagged)
              {
                struct Address *a = result_to_addr(QueryTable[i].data);
                mutt_addr_append(&naddr, a, false);
                mutt_addr_free(&a);
              }
            }

            mutt_alias_create(NULL, naddr);
            mutt_addr_free(&naddr);
          }
          else
          {
            struct Address *a = result_to_addr(QueryTable[menu->current].data);
            mutt_alias_create(NULL, a);
            mutt_addr_free(&a);
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
          msg = mutt_header_new();
          msg->env = mutt_env_new();
          if (!menu->tagprefix)
          {
            msg->env->to = result_to_addr(QueryTable[menu->current].data);
          }
          else
          {
            for (i = 0; i < menu->max; i++)
            {
              if (QueryTable[i].tagged)
              {
                struct Address *a = result_to_addr(QueryTable[i].data);
                mutt_addr_append(&msg->env->to, a, false);
                mutt_addr_free(&a);
              }
            }
          }
          ci_send_message(0, msg, NULL, Context, NULL);
          menu->redraw = REDRAW_FULL;
          break;

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
      for (i = 0; i < menu->max; i++)
      {
        if (QueryTable[i].tagged)
        {
          if (curpos == 0)
          {
            struct Address *tmpa = result_to_addr(QueryTable[i].data);
            mutt_addrlist_to_local(tmpa);
            tagged = true;
            mutt_addr_write(buf, buflen, tmpa, false);
            curpos = mutt_str_strlen(buf);
            mutt_addr_free(&tmpa);
          }
          else if (curpos + 2 < buflen)
          {
            struct Address *tmpa = result_to_addr(QueryTable[i].data);
            mutt_addrlist_to_local(tmpa);
            strcat(buf, ", ");
            mutt_addr_write((char *) buf + curpos + 1, buflen - curpos - 1, tmpa, false);
            curpos = mutt_str_strlen(buf);
            mutt_addr_free(&tmpa);
          }
        }
      }
      /* then enter current message */
      if (!tagged)
      {
        struct Address *tmpa = result_to_addr(QueryTable[menu->current].data);
        mutt_addrlist_to_local(tmpa);
        mutt_addr_write(buf, buflen, tmpa, false);
        mutt_addr_free(&tmpa);
      }
    }

    free_query(&results);
    FREE(&QueryTable);
    mutt_menu_pop_current(menu);
    mutt_menu_destroy(&menu);
  }
}

int mutt_query_complete(char *buf, size_t buflen)
{
  struct Query *results = NULL;
  struct Address *tmpa = NULL;

  if (!QueryCommand)
  {
    mutt_error(_("Query command not defined."));
    return 0;
  }

  results = run_query(buf, 1);
  if (results)
  {
    /* only one response? */
    if (!results->next)
    {
      tmpa = result_to_addr(results);
      mutt_addrlist_to_local(tmpa);
      buf[0] = '\0';
      mutt_addr_write(buf, buflen, tmpa, false);
      mutt_addr_free(&tmpa);
      free_query(&results);
      mutt_clear_error();
      return 0;
    }
    /* multiple results, choose from query menu */
    query_menu(buf, buflen, results, 1);
  }
  return 0;
}

void mutt_query_menu(char *buf, size_t buflen)
{
  if (!QueryCommand)
  {
    mutt_error(_("Query command not defined."));
    return;
  }

  if (!buf)
  {
    char buffer[STRING] = "";

    query_menu(buffer, sizeof(buffer), NULL, 0);
  }
  else
  {
    query_menu(buf, buflen, NULL, 1);
  }
}
