/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@cs.hmc.edu>
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
    mutt_make_help (pbuf, buflen, _(items[i].name), menu, items[i].value);
    len = mutt_strlen (pbuf);
    pbuf += len;
    buflen -= len;
  }
  return buf;
}

static int print_macro (FILE *f, int maxwidth, const char **macro)
{
  int n = maxwidth;
  wchar_t wc;
  int k, w;

  for (;;)
  {
    if ((k = mutt_mbtowc (&wc, *macro, -1)) <= 0)
      break;
    if ((w = mutt_wcwidth (wc)) >= 0)
    {
      if (w > n)
	break;
      n -= w;
      {
	char tb[7];
	int m = mutt_wctomb (tb, wc);
	if (0 < m && m < 7)
	  tb[m] = '\0', fprintf (f, "%s", tb);
      }
    }
    else if (wc < 0x20 || wc == 0x7f)
    {
      if (2 > n)
	break;
      n -= 2;
      if (wc == '\033')
	fprintf (f, "\\e");
      else if (wc == '\n')
	fprintf (f, "\\n");
      else if (wc == '\r')
	fprintf (f, "\\r");
      else if (wc == '\t')
	fprintf (f, "\\t");
      else
	fprintf (f, "^%c", (char)((wc + '@') & 0x7f));
    }
    else
    {
      if (1 > n)
	break;
      n -= 1;
      fprintf (f, "?");
    }
    *macro += k;
  }
  return (maxwidth - n);
}

static int pad (FILE *f, int col, int i)
{
  char fmt[8];

  if (col < i)
  {
    snprintf (fmt, sizeof(fmt), "%%-%ds", i - col);
    fprintf (f, fmt, "");
    return (i);
  }
  fputc (' ', f);
  return (col + 1);
}

static void format_line (FILE *f, int ismacro,
			 const char *t1, const char *t2, const char *t3)
{
  int col;
  int col_a, col_b;
  int split;
  int n;

  fputs (t1, f);

  /* don't try to press string into one line with less than 40 characters.
     The double paranthesis avoid a gcc warning, sigh ... */
  if ((split = COLS < 40))
  {
    col_a = col = 0;
    col_b = LONG_STRING;
    fputc ('\n', f);
  }
  else
  {
    col_a = COLS > 83 ? (COLS - 32) >> 2 : 12;
    col_b = COLS > 49 ? (COLS - 10) >> 1 : 19;
    col = pad (f, mutt_strlen(t1), col_a);
  }

  if (ismacro > 0)
  {
    if (!mutt_strcmp (Pager, "builtin"))
      fputs ("_\010", f);
    fputs ("M ", f);
    col += 2;

    if (!split)
    {
      col += print_macro (f, col_b - col - 4, &t2);
      if (mutt_strlen (t2) > col_b - col)
	t2 = "...";
    }
  }

  col += print_macro (f, col_b - col - 1, &t2);
  if (split)
    fputc ('\n', f);
  else
    col = pad (f, col, col_b);

  if (split)
  {
    print_macro (f, LONG_STRING, &t3);
    fputc ('\n', f);
  }
  else
  {
    while (*t3)
    {
      n = COLS - col;

      if (ismacro >= 0)
      {
	SKIPWS(t3);

	/* FIXME: this is completely wrong */
	if ((n = mutt_strlen (t3)) > COLS - col)
	{
	  n = COLS - col;
	  for (col_a = n; col_a > 0 && t3[col_a] != ' '; col_a--) ;
	  if (col_a)
	    n = col_a;
	}
      }

      print_macro (f, n, &t3);

      if (*t3)
      {
        if (mutt_strcmp (Pager, "builtin"))
	{
	  fputc ('\n', f);
	  n = 0;
	}
	else
	{
	  n += col - COLS;
	  if (option (OPTMARKERS))
	    ++n;
	}
	col = pad (f, n, col_b);
      }
    }
  }

  fputc ('\n', f);
}

static void dump_menu (FILE *f, int menu)
{
  struct keymap_t *map;
  struct binding_t *b;
  char buf[SHORT_STRING];

  /* browse through the keymap table */
  for (map = Keymaps[menu]; map; map = map->next)
  {
    if (map->op != OP_NULL)
    {
      km_expand_key (buf, sizeof (buf), map);

      if (map->op == OP_MACRO)
      {
	if (map->descr == NULL)
	  format_line (f, -1, buf, "macro", map->macro);
        else
	  format_line (f, 1, buf, map->macro, map->descr);
      }
      else
      {
	b = help_lookupFunction (map->op, menu);
	format_line (f, 0, buf, b ? b->name : "UNKNOWN",
	      b ? _(HelpStrings[b->op]) : _("ERROR: please report this bug"));
      }
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
      format_line (f, 0, funcs[i].name, "", _(HelpStrings[funcs[i].op]));
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

  funcs = km_get_table (menu);
  desc = mutt_getnamebyvalue (menu, Menus);
  if (!desc)
    desc = _("<UNKNOWN>");
  
  do {
    if ((f = safe_fopen (t, "w")) == NULL)
    {
      mutt_perror (t);
      return;
    }
  
    dump_menu (f, menu);
    if (menu != MENU_EDITOR && menu != MENU_PAGER)
    {
      fputs (_("\nGeneric bindings:\n\n"), f);
      dump_menu (f, MENU_GENERIC);
    }
  
    fputs (_("\nUnbound functions:\n\n"), f);
    if (funcs)
      dump_unbound (f, funcs, Keymaps[menu], NULL);
    if (menu != MENU_PAGER)
      dump_unbound (f, OpGeneric, Keymaps[MENU_GENERIC], Keymaps[menu]);
  
    fclose (f);
  
    snprintf (buf, sizeof (buf), _("Help for %s"), desc);
  }
  while
    (mutt_do_pager (buf, t,
		    M_PAGER_RETWINCH | M_PAGER_MARKER | M_PAGER_NSKIP,
		    NULL)
     == OP_REFORMAT_WINCH);
}
