/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */ 

#include "mutt.h"
#include "mutt_menu.h"
#include "mapping.h"
#include "sort.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

typedef struct query
{
  ADDRESS *addr;
  char *name;
  char *other;
  struct query *next;
} QUERY;

typedef struct entry
{
  int tagged;
  QUERY *data;
} ENTRY;

static struct mapping_t QueryHelp[] = {
  { N_("Exit"),   OP_EXIT },
  { N_("Mail"),   OP_MAIL },
  { N_("New Query"),  OP_QUERY },
  { N_("Make Alias"), OP_CREATE_ALIAS },
  { N_("Search"), OP_SEARCH },
  { N_("Help"),   OP_HELP },
  { NULL }
};

/* Variables for outsizing output format */
static int FirstColumn;
static int SecondColumn;

static void query_menu (char *buf, size_t buflen, QUERY *results, int retbuf);

static ADDRESS *result_to_addr (QUERY *r)
{
  static ADDRESS tmp;
  
  tmp = *r->addr;
  
  if(!tmp.next && !tmp.personal)
    tmp.personal = r->name;
  
  return &tmp;
}

static QUERY *run_query (char *s, int quiet)
{
  FILE *fp;
  QUERY *first = NULL;
  QUERY *cur = NULL;
  char cmd[_POSIX_PATH_MAX];
  char *buf = NULL;
  size_t buflen;
  int dummy = 0;
  char msg[STRING];
  char *p;
  pid_t thepid;
  int l;


  mutt_expand_file_fmt (cmd, sizeof(cmd), QueryCmd, s);

  if ((thepid = mutt_create_filter (cmd, NULL, &fp, NULL)) < 0)
  {
    dprint (1, (debugfile, "unable to fork command: %s", cmd));
    return 0;
  }
  if (!quiet)
    mutt_message _("Waiting for response...");
  fgets (msg, sizeof (msg), fp);
  if ((p = strrchr (msg, '\n')))
    *p = '\0';
  while ((buf = mutt_read_line (buf, &buflen, fp, &dummy)) != NULL)
  {
    if ((p = strtok(buf, "\t\n")))
    {
      if (first == NULL)
      {
	FirstColumn = 0;
	SecondColumn = 0;
	first = (QUERY *) safe_calloc (1, sizeof (QUERY));
	cur = first;
      }
      else
      {
	cur->next = (QUERY *) safe_calloc (1, sizeof (QUERY));
	cur = cur->next;
      }

      l = mutt_strwidth (p);
      if (l > SecondColumn)
	SecondColumn = l;
	
      cur->addr = rfc822_parse_adrlist (cur->addr, p);
      p = strtok(NULL, "\t\n");
      if (p)
      {
	l = mutt_strwidth (p);
	if (l > FirstColumn)
	  FirstColumn = l;
	cur->name = safe_strdup (p);
	p = strtok(NULL, "\t\n");
	if (p)
	{
	  cur->other = safe_strdup (p);
	}
      }
    }
  }
  FREE (&buf);
  fclose (fp);
  if (mutt_wait_filter (thepid))
  {
    dprint (1, (debugfile, "Error: %s\n", msg));
    if (!quiet)  mutt_error ("%s", msg);
  }
  else
  {
    if (!quiet)
      mutt_message ("%s", msg);
  }
  
  return first;
}

static int query_search (MUTTMENU *m, regex_t *re, int n)
{
  ENTRY *table = (ENTRY *) m->data;

  if (table[n].data->name && !regexec (re, table[n].data->name, 0, NULL, 0))
    return 0;
  if (table[n].data->other && !regexec (re, table[n].data->other, 0, NULL, 0))
    return 0;
  if (table[n].data->addr)
  {
    if (table[n].data->addr->personal && 
	!regexec (re, table[n].data->addr->personal, 0, NULL, 0))
      return 0;
    if (table[n].data->addr->mailbox &&
	!regexec (re, table[n].data->addr->mailbox, 0, NULL, 0))
      return 0;
#ifdef EXACT_ADDRESS
    if (table[n].data->addr->val &&
	!regexec (re, table[n].data->addr->val, 0, NULL, 0))
      return 0;
#endif
  }
  
  return REG_NOMATCH;
}

/* This is the callback routine from mutt_menuLoop() which is used to generate
 * a menu entry for the requested item number.
 */
#define QUERY_MIN_COLUMN_LENGHT 20 /* Must be < 70/2 */
static void query_entry (char *s, size_t slen, MUTTMENU *m, int num)
{
  ENTRY *table = (ENTRY *) m->data;
  char buf2[SHORT_STRING], buf[SHORT_STRING] = "";

  /* need a query format ... hard coded constants are not good */
  while (FirstColumn + SecondColumn > 70)
  {
    FirstColumn = FirstColumn * 3 / 4;
    SecondColumn = SecondColumn * 3 / 4;
    if (FirstColumn < QUERY_MIN_COLUMN_LENGHT)
      FirstColumn = QUERY_MIN_COLUMN_LENGHT;
    if (SecondColumn < QUERY_MIN_COLUMN_LENGHT)
      SecondColumn = QUERY_MIN_COLUMN_LENGHT;
  }

  rfc822_write_address (buf, sizeof (buf), table[num].data->addr);

  mutt_format_string (buf2, sizeof (buf2),
		      FirstColumn + 2, FirstColumn + 2,
		      0, ' ', table[num].data->name,
		      mutt_strlen (table[num].data->name), 0);

  snprintf (s, slen, " %c %3d %s %-*.*s %s", 
	    table[num].tagged ? '*':' ',
	    num+1,
	    buf2,
	    SecondColumn+2,
	    SecondColumn+2,
	    buf,
	    NONULL (table[num].data->other));
}

static int query_tag (MUTTMENU *menu, int n, int m)
{
  ENTRY *cur = &((ENTRY *) menu->data)[n];
  int ot = cur->tagged;
  
  cur->tagged = m >= 0 ? m : !cur->tagged;
  return cur->tagged - ot;
}

int mutt_query_complete (char *buf, size_t buflen)
{
  QUERY *results = NULL;

  if (!QueryCmd)
  {
    mutt_error _("Query command not defined.");
    return 0;
  }

  results = run_query (buf, 1);
  if (results)
  {
    /* only one response? */
    if (results->next == NULL)
    {
      buf[0] = '\0';
      rfc822_write_address (buf, buflen, result_to_addr(results));
      mutt_clear_error ();
      return (0);
    }
    /* multiple results, choose from query menu */
    query_menu (buf, buflen, results, 1);
  }
  return (0);
}

void mutt_query_menu (char *buf, size_t buflen)
{
  if (!QueryCmd)
  {
    mutt_error _("Query command not defined.");
    return;
  }

  if (buf == NULL)
  {
    char buffer[STRING] = "";

    query_menu (buffer, sizeof (buffer), NULL, 0);
  }
  else
  {
    query_menu (buf, buflen, NULL, 1);
  }
}

static void query_menu (char *buf, size_t buflen, QUERY *results, int retbuf)
{
  MUTTMENU *menu;
  HEADER *msg = NULL;
  ENTRY *QueryTable = NULL;
  QUERY *queryp = NULL;
  int i, done = 0;
  int op;
  char helpstr[SHORT_STRING];
  char title[STRING];

  snprintf (title, sizeof (title), _("Query")); /* FIXME */

  menu = mutt_new_menu ();
  menu->make_entry = query_entry;
  menu->search = query_search;
  menu->tag = query_tag;
  menu->menu = MENU_QUERY;
  menu->title = title;
  menu->help = mutt_compile_help (helpstr, sizeof (helpstr), MENU_QUERY, QueryHelp);

  if (results == NULL)
  {
    /* Prompt for Query */
    if (mutt_get_field (_("Query: "), buf, buflen, 0) == 0 && buf[0])
    {
      results = run_query (buf, 0);
    }
  }

  if (results)
  {
    snprintf (title, sizeof (title), _("Query '%s'"), buf);

    /* count the number of results */
    for (queryp = results; queryp; queryp = queryp->next)
      menu->max++;

    menu->data = QueryTable = (ENTRY *) safe_calloc (menu->max, sizeof (ENTRY));

    for (i = 0, queryp = results; queryp; queryp = queryp->next, i++)
      QueryTable[i].data = queryp;

    while (!done)
    {
      switch ((op = mutt_menuLoop (menu)))
      {
	case OP_QUERY_APPEND:
	case OP_QUERY:
	  if (mutt_get_field (_("Query: "), buf, buflen, 0) == 0 && buf[0])
	  {
	    QUERY *newresults = NULL;

	    newresults = run_query (buf, 0);

	    menu->redraw = REDRAW_FULL;
	    if (newresults)
	    {
	      snprintf (title, sizeof (title), _("Query '%s'"), buf);

	      if (op == OP_QUERY)
	      {
		queryp = results;
		while (queryp)
		{
		  rfc822_free_address (&queryp->addr);
		  FREE (&queryp->name);
		  FREE (&queryp->other);
		  results = queryp->next;
		  FREE (&queryp);
		  queryp = results;
		}
		results = newresults;
		FREE (&QueryTable);
	      }
	      else
	      {
		/* append */
		for (queryp = results; queryp->next; queryp = queryp->next);

		queryp->next = newresults;
	      }


	      menu->current = 0;
	      mutt_menuDestroy (&menu);
	      menu = mutt_new_menu ();
	      menu->make_entry = query_entry;
	      menu->search = query_search;
	      menu->tag = query_tag;
	      menu->menu = MENU_QUERY;
	      menu->title = title;
	      menu->help = mutt_compile_help (helpstr, sizeof (helpstr), MENU_QUERY, QueryHelp);

	      /* count the number of results */
	      for (queryp = results; queryp; queryp = queryp->next)
		menu->max++;

	      if (op == OP_QUERY)
	      {
		menu->data = QueryTable = 
		  (ENTRY *) safe_calloc (menu->max, sizeof (ENTRY));

		for (i = 0, queryp = results; queryp; 
		     queryp = queryp->next, i++)
		  QueryTable[i].data = queryp;
	      }
	      else
	      {
		int clear = 0;

		/* append */
		safe_realloc ((void **)&QueryTable, menu->max * sizeof (ENTRY));

		menu->data = QueryTable;

		for (i = 0, queryp = results; queryp; 
		     queryp = queryp->next, i++)
		{
		  /* once we hit new entries, clear/init the tag */
		  if (queryp == newresults)
		    clear = 1;

		  QueryTable[i].data = queryp;
		  if (clear)
		    QueryTable[i].tagged = 0;
		}
	      }
	    }
	  }
	  break;

	case OP_CREATE_ALIAS:
	  if (menu->tagprefix)
	  {
	    ADDRESS *naddr = NULL;

	    for (i = 0; i < menu->max; i++)
	      if (QueryTable[i].tagged)
		rfc822_append (&naddr, result_to_addr(QueryTable[i].data));

	    mutt_create_alias (NULL, naddr);
	  }
	  else
	  {
	    mutt_create_alias (NULL, result_to_addr(QueryTable[menu->current].data));
	  }
	  break;

	case OP_GENERIC_SELECT_ENTRY:
	  if (retbuf)
	  {
	    done = 2;
	    break;
	  }
	  /* fall through to OP_MAIL */

	case OP_MAIL:
	  msg = mutt_new_header ();
	  msg->env = mutt_new_envelope ();
	  if (!menu->tagprefix)
	  {
	    msg->env->to = 
	      rfc822_cpy_adr (result_to_addr(QueryTable[menu->current].data));
	  }
	  else
	  {
	    for (i = 0; i < menu->max; i++)
	      if (QueryTable[i].tagged)
		rfc822_append (&msg->env->to, result_to_addr(QueryTable[i].data));
	  }
	  ci_send_message (0, msg, NULL, Context, NULL);
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
      int tagged = 0;
      size_t curpos = 0;

      memset (buf, 0, buflen); 

      /* check for tagged entries */
      for (i = 0; i < menu->max; i++)
      {
	if (QueryTable[i].tagged)
	{
	  if (curpos == 0)
	  {
	    tagged = 1;
	    rfc822_write_address (buf, buflen, result_to_addr(QueryTable[i].data));
	    curpos = mutt_strlen (buf);
	  }
	  else if (curpos + 2 < buflen)
	  {
	    strcat (buf, ", ");	/* __STRCAT_CHECKED__ */
	    rfc822_write_address ((char *) buf + curpos + 1, buflen - curpos - 1,
				  result_to_addr(QueryTable[i].data));
	    curpos = mutt_strlen (buf);
	  }
	}
      }
      /* then enter current message */
      if (!tagged)
      {
	rfc822_write_address (buf, buflen, result_to_addr(QueryTable[menu->current].data));
      }
      
    }

    queryp = results;
    while (queryp)
    {
      rfc822_free_address (&queryp->addr);
      FREE (&queryp->name);
      FREE (&queryp->other);
      results = queryp->next;
      FREE (&queryp);
      queryp = results;
    }
    FREE (&QueryTable);
    
    /* tell whoever called me to redraw the screen when I return */
    set_option (OPTNEEDREDRAW);
  }

  mutt_menuDestroy (&menu);
}
