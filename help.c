/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
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
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */ 

#define HELP_C

#include "mutt.h"
#include "mutt_curses.h"
#include "keymap.h"
#include "pager.h"
#include "mapping.h"

#include <ctype.h>
#include <string.h>

static struct binding_t *help_lookupFunction (int op, int menu)
{
  int i;
  struct binding_t *map;

  if (menu != MENU_PAGER)
  {
    /* first look in the generic map for the function */
    for (i = 0; OpGeneric[i].name; i++)
      if (OpGeneric[i].op == op)
	return (&OpGeneric[i]);    
  }

  if ((map = km_get_table(menu)))
  {
    for (i = 0; map[i].name; i++)
      if (map[i].op == op)
	return (&map[i]);
  }
  
  return (NULL);
}

void mutt_make_help (char *d, size_t dlen, char *txt, int menu, int op)
{
  char buf[SHORT_STRING];

  if (km_expand_key (buf, sizeof (buf), km_find_func (menu, op)) ||
      km_expand_key (buf, sizeof (buf), km_find_func (MENU_GENERIC, op)))
    snprintf (d, dlen, "%s:%s", buf, txt);
  else
    d[0] = 0;
}

char *
mutt_compile_help (char *buf, size_t buflen, int menu, struct mapping_t *items)
{
  int i;
  size_t len;
  char *pbuf = buf;
  
  for (i = 0; items[i].name && buflen > 2; i++)
  {
    if (i)
    {
      *pbuf++ = ' ';
      *pbuf++ = ' ';
      buflen -= 2;
    }
    mutt_make_help (pbuf, buflen, items[i].name, menu, items[i].value);
    len = strlen (pbuf);
    pbuf += len;
    buflen -= len;
  }
  return buf;
}

static void print_macro (FILE *f, const char *macro)
{
  int i;

  for (i = 0; *macro && i < COLS - 34; macro++, i++)
  {
    switch (*macro)
    {
      case '\033':
	fputs ("\\e", f);
	i++;
	break;
      case '\n':
	fputs ("\\n", f);
	i++;
	break;
      case '\r':
	fputs ("\\r", f);
	i++;
	break;
      case '\t':
	fputs ("\\t", f);
	i++;
	break;
      default:
	fputc (*macro, f);
	break;
    }
  }
}

static void dump_menu (FILE *f, int menu)
{
  struct keymap_t *map;
  struct binding_t *b;
  char buf[SHORT_STRING];

  /* browse through the keymap table */
  for (map = Keymaps[menu]; map; map = map->next)
  {
    km_expand_key (buf, sizeof (buf), map);

    if (map->op == OP_MACRO)
    {
      fprintf (f, "%s\t%-20s\t", buf, "macro");
      print_macro (f, map->macro);
      fputc ('\n', f);
    }
    else if (map->op != OP_NULL)
    {
      b = help_lookupFunction (map->op, menu);
      fprintf (f, "%s\t%-20s\t%s\n", buf,
	      b ? b->name : "UNKNOWN",
	      b ? HelpStrings[b->op] : "ERROR: please report this bug");
    }
  }
}

static int is_bound (struct keymap_t *map, int op)
{
  for (; map; map = map->next)
    if (map->op == op)
      return 1;
  return 0;
}

static void dump_unbound (FILE *f,
			  struct binding_t *funcs,
			  struct keymap_t *map,
			  struct keymap_t *aux)
{
  int i;

  for (i = 0; funcs[i].name; i++)
  {
    if (! is_bound (map, funcs[i].op) &&
	(!aux || ! is_bound (aux, funcs[i].op)))
      fprintf (f, "%s\t\t%s\n", funcs[i].name, HelpStrings[funcs[i].op]);
  }
}

void mutt_help (int menu)
{
  char t[_POSIX_PATH_MAX];
  char buf[SHORT_STRING];
  char *desc;
  FILE *f;
  struct binding_t *funcs;

  mutt_mktemp (t);
  if ((f = safe_fopen (t, "w")) == NULL)
  {
    mutt_perror (t);
    return;
  }

  funcs = km_get_table (menu);
  desc = mutt_getnamebyvalue (menu, Menus);
  if (!desc)
    desc = "<UNKNOWN>";
  
  dump_menu (f, menu);
  if (menu != MENU_EDITOR && menu != MENU_PAGER)
  {
    fputs ("\nGeneric bindings:\n\n", f);
    dump_menu (f, MENU_GENERIC);
  }

  fputs ("\nUnbound functions:\n\n", f);
  if (funcs)
    dump_unbound (f, funcs, Keymaps[menu], NULL);
  if (menu != MENU_PAGER)
    dump_unbound (f, OpGeneric, Keymaps[MENU_GENERIC], Keymaps[menu]);

  fclose (f);

  snprintf (buf, sizeof (buf), "Help for %s", desc);
  mutt_do_pager (buf, t, 0, NULL);
}
