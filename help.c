/**
 * @file
 * Generate the help-line and help-page and GUI display them
 *
 * @authors
 * Copyright (C) 1996-2000,2009 Michael R. Elkins <me@mutt.org>
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
#include <stddef.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "mutt/mutt.h"
#include "globals.h"
#include "keymap.h"
#include "mutt_curses.h"
#include "opcodes.h"
#include "options.h"
#include "pager.h"
#include "protos.h"

static const char *HelpStrings[] = {
#define DEFINE_HELP_MESSAGE(opcode, help_string) help_string,
  OPS(DEFINE_HELP_MESSAGE)
#undef DEFINE_HELP_MESSAGE
      NULL,
};

static const struct Binding *help_lookup_function(int op, int menu)
{
  const struct Binding *map = NULL;

  if (menu != MENU_PAGER)
  {
    /* first look in the generic map for the function */
    for (int i = 0; OpGeneric[i].name; i++)
      if (OpGeneric[i].op == op)
        return (&OpGeneric[i]);
  }

  map = km_get_table(menu);
  if (map)
  {
    for (int i = 0; map[i].name; i++)
      if (map[i].op == op)
        return (&map[i]);
  }

  return NULL;
}

void mutt_make_help(char *d, size_t dlen, const char *txt, int menu, int op)
{
  char buf[SHORT_STRING];

  if (km_expand_key(buf, sizeof(buf), km_find_func(menu, op)) ||
      km_expand_key(buf, sizeof(buf), km_find_func(MENU_GENERIC, op)))
  {
    snprintf(d, dlen, "%s:%s", buf, txt);
  }
  else
  {
    d[0] = '\0';
  }
}

char *mutt_compile_help(char *buf, size_t buflen, int menu, const struct Mapping *items)
{
  char *pbuf = buf;

  for (int i = 0; items[i].name && buflen > 2; i++)
  {
    if (i)
    {
      *pbuf++ = ' ';
      *pbuf++ = ' ';
      buflen -= 2;
    }
    mutt_make_help(pbuf, buflen, _(items[i].name), menu, items[i].value);
    const size_t len = mutt_str_strlen(pbuf);
    pbuf += len;
    buflen -= len;
  }
  return buf;
}

static int print_macro(FILE *f, int maxwidth, const char **macro)
{
  int n = maxwidth;
  wchar_t wc;
  size_t k;
  size_t len = mutt_str_strlen(*macro);
  mbstate_t mbstate1, mbstate2;

  memset(&mbstate1, 0, sizeof(mbstate1));
  memset(&mbstate2, 0, sizeof(mbstate2));
  for (; len && (k = mbrtowc(&wc, *macro, len, &mbstate1)); *macro += k, len -= k)
  {
    if (k == (size_t)(-1) || k == (size_t)(-2))
    {
      if (k == (size_t)(-1))
        memset(&mbstate1, 0, sizeof(mbstate1));
      k = (k == (size_t)(-1)) ? 1 : len;
      wc = ReplacementChar;
    }
    /* glibc-2.1.3's wcwidth() returns 1 for unprintable chars! */
    const int w = wcwidth(wc);
    if (IsWPrint(wc) && w >= 0)
    {
      if (w > n)
        break;
      n -= w;
      {
        char buf[MB_LEN_MAX * 2];
        size_t n1, n2;
        if ((n1 = wcrtomb(buf, wc, &mbstate2)) != (size_t)(-1) &&
            (n2 = wcrtomb(buf + n1, 0, &mbstate2)) != (size_t)(-1))
        {
          fputs(buf, f);
        }
      }
    }
    else if (wc < 0x20 || wc == 0x7f)
    {
      if (n < 2)
        break;
      n -= 2;
      if (wc == '\033')
        fprintf(f, "\\e");
      else if (wc == '\n')
        fprintf(f, "\\n");
      else if (wc == '\r')
        fprintf(f, "\\r");
      else if (wc == '\t')
        fprintf(f, "\\t");
      else
        fprintf(f, "^%c", (char) ((wc + '@') & 0x7f));
    }
    else
    {
      if (n < 1)
        break;
      n -= 1;
      fprintf(f, "?");
    }
  }
  return (maxwidth - n);
}

static int get_wrapped_width(const char *t, size_t wid)
{
  wchar_t wc;
  size_t k;
  size_t m, n;
  size_t len = mutt_str_strlen(t);
  const char *s = t;
  mbstate_t mbstate;

  memset(&mbstate, 0, sizeof(mbstate));
  for (m = wid, n = 0; len && (k = mbrtowc(&wc, s, len, &mbstate)) && (n <= wid);
       s += k, len -= k)
  {
    if (*s == ' ')
      m = n;
    if (k == (size_t)(-1) || k == (size_t)(-2))
    {
      if (k == (size_t)(-1))
        memset(&mbstate, 0, sizeof(mbstate));
      k = (k == (size_t)(-1)) ? 1 : len;
      wc = ReplacementChar;
    }
    if (!IsWPrint(wc))
      wc = '?';
    n += wcwidth(wc);
  }
  if (n > wid)
    n = m;
  else
    n = wid;
  return n;
}

static int pad(FILE *f, int col, int i)
{
  if (col < i)
  {
    char fmt[32] = "";
    snprintf(fmt, sizeof(fmt), "%%-%ds", i - col);
    fprintf(f, fmt, "");
    return i;
  }
  fputc(' ', f);
  return (col + 1);
}

static void format_line(FILE *f, int ismacro, const char *t1, const char *t2, const char *t3)
{
  int col;
  int col_b;
  bool split;

  fputs(t1, f);

  /* don't try to press string into one line with less than 40 characters. */
  split = (MuttIndexWindow->cols < 40);
  if (split)
  {
    col = 0;
    col_b = LONG_STRING;
    fputc('\n', f);
  }
  else
  {
    const int col_a = MuttIndexWindow->cols > 83 ? (MuttIndexWindow->cols - 32) >> 2 : 12;
    col_b = MuttIndexWindow->cols > 49 ? (MuttIndexWindow->cols - 10) >> 1 : 19;
    col = pad(f, mutt_strwidth(t1), col_a);
  }

  if (ismacro > 0)
  {
    if (mutt_str_strcmp(Pager, "builtin") == 0)
      fputs("_\010", f);
    fputs("M ", f);
    col += 2;

    if (!split)
    {
      col += print_macro(f, col_b - col - 4, &t2);
      if (mutt_strwidth(t2) > col_b - col)
        t2 = "...";
    }
  }

  col += print_macro(f, col_b - col - 1, &t2);
  if (split)
    fputc('\n', f);
  else
    col = pad(f, col, col_b);

  if (split)
  {
    print_macro(f, LONG_STRING, &t3);
    fputc('\n', f);
  }
  else
  {
    while (*t3)
    {
      int n = MuttIndexWindow->cols - col;

      if (ismacro >= 0)
      {
        SKIPWS(t3);
        n = get_wrapped_width(t3, n);
      }

      n = print_macro(f, n, &t3);

      if (*t3)
      {
        if (mutt_str_strcmp(Pager, "builtin") != 0)
        {
          fputc('\n', f);
          n = 0;
        }
        else
        {
          n += col - MuttIndexWindow->cols;
          if (Markers)
            n++;
        }
        col = pad(f, n, col_b);
      }
    }
  }

  fputc('\n', f);
}

static void dump_menu(FILE *f, int menu)
{
  struct Keymap *map = NULL;
  const struct Binding *b = NULL;
  char buf[SHORT_STRING];

  /* browse through the keymap table */
  for (map = Keymaps[menu]; map; map = map->next)
  {
    if (map->op != OP_NULL)
    {
      km_expand_key(buf, sizeof(buf), map);

      if (map->op == OP_MACRO)
      {
        if (!map->descr)
          format_line(f, -1, buf, "macro", map->macro);
        else
          format_line(f, 1, buf, map->macro, map->descr);
      }
      else
      {
        b = help_lookup_function(map->op, menu);
        format_line(f, 0, buf, b ? b->name : "UNKNOWN",
                    b ? _(HelpStrings[b->op]) : _("ERROR: please report this bug"));
      }
    }
  }
}

static bool is_bound(struct Keymap *map, int op)
{
  for (; map; map = map->next)
    if (map->op == op)
      return true;
  return false;
}

static void dump_unbound(FILE *f, const struct Binding *funcs,
                         struct Keymap *map, struct Keymap *aux)
{
  for (int i = 0; funcs[i].name; i++)
  {
    if (!is_bound(map, funcs[i].op) && (!aux || !is_bound(aux, funcs[i].op)))
      format_line(f, 0, funcs[i].name, "", _(HelpStrings[funcs[i].op]));
  }
}

void mutt_help(int menu)
{
  char t[_POSIX_PATH_MAX];
  char buf[SHORT_STRING];
  const char *desc = NULL;
  FILE *f = NULL;
  const struct Binding *funcs = NULL;

  mutt_mktemp(t, sizeof(t));

  funcs = km_get_table(menu);
  desc = mutt_map_get_name(menu, Menus);
  if (!desc)
    desc = _("<UNKNOWN>");

  do
  {
    f = mutt_file_fopen(t, "w");
    if (!f)
    {
      mutt_perror(t);
      return;
    }

    dump_menu(f, menu);
    if (menu != MENU_EDITOR && menu != MENU_PAGER)
    {
      fprintf(f, "\n%s\n\n", _("Generic bindings:"));
      dump_menu(f, MENU_GENERIC);
    }

    fprintf(f, "\n%s\n\n", _("Unbound functions:"));
    if (funcs)
      dump_unbound(f, funcs, Keymaps[menu], NULL);
    if (menu != MENU_PAGER)
      dump_unbound(f, OpGeneric, Keymaps[MENU_GENERIC], Keymaps[menu]);

    mutt_file_fclose(&f);

    snprintf(buf, sizeof(buf), _("Help for %s"), desc);
  } while (mutt_do_pager(buf, t, MUTT_PAGER_RETWINCH | MUTT_PAGER_MARKER | MUTT_PAGER_NSKIP | MUTT_PAGER_NOWRAP,
                         NULL) == OP_REFORMAT_WINCH);
}
