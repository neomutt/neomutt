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

/**
 * @page help Generate the help-line and help-page and GUI display them
 *
 * Generate the help-line and help-page and GUI display them
 */

#include "config.h"
#include <stddef.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "functions.h"
#include "keymap.h"
#include "mutt_globals.h"
#include "muttlib.h"
#include "opcodes.h"
#include "pager.h"
#include "protos.h" // IWYU pragma: keep

/**
 * help_lookup_function - Find a keybinding for an operation
 * @param op   Operation, e.g. OP_DELETE
 * @param menu Current Menu, e.g. #MENU_PAGER
 * @retval ptr  Key binding
 * @retval NULL If none
 */
static const struct Binding *help_lookup_function(int op, enum MenuType menu)
{
  const struct Binding *map = NULL;

  if (menu != MENU_PAGER)
  {
    /* first look in the generic map for the function */
    for (int i = 0; OpGeneric[i].name; i++)
      if (OpGeneric[i].op == op)
        return &OpGeneric[i];
  }

  map = km_get_table(menu);
  if (map)
  {
    for (int i = 0; map[i].name; i++)
      if (map[i].op == op)
        return &map[i];
  }

  return NULL;
}

/**
 * mutt_make_help - Create one entry for the help bar
 * @param buf    Buffer for the result
 * @param buflen Length of buffer
 * @param txt    Text part, e.g. "delete"
 * @param menu   Current Menu, e.g. #MENU_PAGER
 * @param op     Operation, e.g. OP_DELETE
 *
 * This will return something like: "d:delete"
 */
void mutt_make_help(char *buf, size_t buflen, const char *txt, enum MenuType menu, int op)
{
  char tmp[128];

  if (km_expand_key(tmp, sizeof(tmp), km_find_func(menu, op)) ||
      km_expand_key(tmp, sizeof(tmp), km_find_func(MENU_GENERIC, op)))
  {
    snprintf(buf, buflen, "%s:%s", tmp, txt);
  }
  else
  {
    buf[0] = '\0';
  }
}

/**
 * mutt_compile_help - Create the text for the help menu
 * @param buf    Buffer for the result
 * @param buflen Length of buffer
 * @param menu   Current Menu, e.g. #MENU_PAGER
 * @param items  Map of functions to display in the help bar
 * @retval ptr Buffer containing result
 */
char *mutt_compile_help(char *buf, size_t buflen, enum MenuType menu,
                        const struct Mapping *items)
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
    const size_t len = mutt_str_len(pbuf);
    pbuf += len;
    buflen -= len;
  }
  return buf;
}

/**
 * print_macro - Print a macro string to a file
 * @param[in]  fp       File to write to
 * @param[in]  maxwidth Maximum width in screen columns
 * @param[out] macro    Macro string
 * @retval num Number of screen columns used
 *
 * The `macro` pointer is move past the string we've printed
 */
static int print_macro(FILE *fp, int maxwidth, const char **macro)
{
  int n = maxwidth;
  wchar_t wc;
  size_t k;
  size_t len = mutt_str_len(*macro);
  mbstate_t mbstate1, mbstate2;

  memset(&mbstate1, 0, sizeof(mbstate1));
  memset(&mbstate2, 0, sizeof(mbstate2));
  for (; len && (k = mbrtowc(&wc, *macro, len, &mbstate1)); *macro += k, len -= k)
  {
    if ((k == (size_t)(-1)) || (k == (size_t)(-2)))
    {
      if (k == (size_t)(-1))
        memset(&mbstate1, 0, sizeof(mbstate1));
      k = (k == (size_t)(-1)) ? 1 : len;
      wc = ReplacementChar;
    }
    /* glibc-2.1.3's wcwidth() returns 1 for unprintable chars! */
    const int w = wcwidth(wc);
    if (IsWPrint(wc) && (w >= 0))
    {
      if (w > n)
        break;
      n -= w;
      {
        char buf[MB_LEN_MAX * 2];
        size_t n1, n2;
        if (((n1 = wcrtomb(buf, wc, &mbstate2)) != (size_t)(-1)) &&
            ((n2 = wcrtomb(buf + n1, 0, &mbstate2)) != (size_t)(-1)))
        {
          fputs(buf, fp);
        }
      }
    }
    else if ((wc < 0x20) || (wc == 0x7f))
    {
      if (n < 2)
        break;
      n -= 2;
      if (wc == '\033') // Escape
        fprintf(fp, "\\e");
      else if (wc == '\n')
        fprintf(fp, "\\n");
      else if (wc == '\r')
        fprintf(fp, "\\r");
      else if (wc == '\t')
        fprintf(fp, "\\t");
      else
        fprintf(fp, "^%c", (char) ((wc + '@') & 0x7f));
    }
    else
    {
      if (n < 1)
        break;
      n -= 1;
      fprintf(fp, "?");
    }
  }
  return maxwidth - n;
}

/**
 * get_wrapped_width - Wrap a string at a sensible place
 * @param t   String to wrap
 * @param wid Maximum width
 * @retval num Break after this many characters
 *
 * If the string's too long, look for some whitespace to break at.
 */
static int get_wrapped_width(const char *t, size_t wid)
{
  wchar_t wc;
  size_t k;
  size_t m, n;
  size_t len = mutt_str_len(t);
  const char *s = t;
  mbstate_t mbstate;

  memset(&mbstate, 0, sizeof(mbstate));
  for (m = wid, n = 0; len && (k = mbrtowc(&wc, s, len, &mbstate)) && (n <= wid);
       s += k, len -= k)
  {
    if (*s == ' ')
      m = n;
    if ((k == (size_t)(-1)) || (k == (size_t)(-2)))
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

/**
 * pad - Write some padding to a file
 * @param fp  File to write to
 * @param col Current screen column
 * @param i   Screen column to pad until
 * @retval num
 * - `i` - Padding was added
 * - `col` - Content was already wider than col
 */
static int pad(FILE *fp, int col, int i)
{
  if (col < i)
  {
    char fmt[32] = { 0 };
    snprintf(fmt, sizeof(fmt), "%%-%ds", i - col);
    fprintf(fp, fmt, "");
    return i;
  }
  fputc(' ', fp);
  return col + 1;
}

/**
 * format_line - Write a formatted line to a file
 * @param fp      File to write to
 * @param ismacro Layout mode, see below
 * @param t1      Text part 1
 * @param t2      Text part 2
 * @param t3      Text part 3
 * @param wraplen Width to wrap to
 *
 * Assemble the three columns of text.
 *
 * `ismacro` can be:
 * *  1 : Macro with a description
 * *  0 : Non-macro
 * * -1 : Macro with no description
 */
static void format_line(FILE *fp, int ismacro, const char *t1, const char *t2,
                        const char *t3, int wraplen)
{
  int col;
  int col_b;

  fputs(t1, fp);

  /* don't try to press string into one line with less than 40 characters. */
  bool split = (wraplen < 40);
  if (split)
  {
    col = 0;
    col_b = 1024;
    fputc('\n', fp);
  }
  else
  {
    const int col_a = (wraplen > 83) ? (wraplen - 32) >> 2 : 12;
    col_b = (wraplen > 49) ? (wraplen - 10) >> 1 : 19;
    col = pad(fp, mutt_strwidth(t1), col_a);
  }

  if (ismacro > 0)
  {
    if (!C_Pager || mutt_str_equal(C_Pager, "builtin"))
      fputs("_\010", fp); // Ctrl-H (backspace)
    fputs("M ", fp);
    col += 2;

    if (!split)
    {
      col += print_macro(fp, col_b - col - 4, &t2);
      if (mutt_strwidth(t2) > col_b - col)
        t2 = "...";
    }
  }

  col += print_macro(fp, col_b - col - 1, &t2);
  if (split)
    fputc('\n', fp);
  else
    col = pad(fp, col, col_b);

  if (split)
  {
    print_macro(fp, 1024, &t3);
    fputc('\n', fp);
  }
  else
  {
    while (*t3)
    {
      int n = wraplen - col;

      if (ismacro >= 0)
      {
        SKIPWS(t3);
        n = get_wrapped_width(t3, n);
      }

      n = print_macro(fp, n, &t3);

      if (*t3)
      {
        if (mutt_str_equal(C_Pager, "builtin"))
        {
          n += col - wraplen;
          if (C_Markers)
            n++;
        }
        else
        {
          fputc('\n', fp);
          n = 0;
        }
        col = pad(fp, n, col_b);
      }
    }
  }

  fputc('\n', fp);
}

/**
 * dump_menu - Write all the key bindings to a file
 * @param fp      File to write to
 * @param menu    Current Menu, e.g. #MENU_PAGER
 * @param wraplen Width to wrap to
 */
static void dump_menu(FILE *fp, enum MenuType menu, int wraplen)
{
  struct Keymap *map = NULL;
  const struct Binding *b = NULL;
  char buf[128];

  /* browse through the keymap table */
  for (map = Keymaps[menu]; map; map = map->next)
  {
    if (map->op != OP_NULL)
    {
      km_expand_key(buf, sizeof(buf), map);

      if (map->op == OP_MACRO)
      {
        if (map->desc)
          format_line(fp, 1, buf, map->macro, map->desc, wraplen);
        else
          format_line(fp, -1, buf, "macro", map->macro, wraplen);
      }
      else
      {
        b = help_lookup_function(map->op, menu);
        format_line(fp, 0, buf, b ? b->name : "UNKNOWN",
                    b ? _(OpStrings[b->op][1]) : _("ERROR: please report this bug"), wraplen);
      }
    }
  }
}

/**
 * is_bound - Does a function have a keybinding?
 * @param map Keymap to examine
 * @param op  Operation, e.g. OP_DELETE
 * @retval true If a key is bound to that operation
 */
static bool is_bound(struct Keymap *map, int op)
{
  for (; map; map = map->next)
    if (map->op == op)
      return true;
  return false;
}

/**
 * dump_unbound - Write out all the operations with no key bindings
 * @param fp      File to write to
 * @param funcs   All the bindings for the current menu
 * @param map     First key map to consider
 * @param aux     Second key map to consider
 * @param wraplen Width to wrap to
 */
static void dump_unbound(FILE *fp, const struct Binding *funcs,
                         struct Keymap *map, struct Keymap *aux, int wraplen)
{
  for (int i = 0; funcs[i].name; i++)
  {
    if (!is_bound(map, funcs[i].op) && (!aux || !is_bound(aux, funcs[i].op)))
      format_line(fp, 0, funcs[i].name, "", _(OpStrings[funcs[i].op][1]), wraplen);
  }
}

/**
 * mutt_help - Display the help menu
 * @param menu    Current Menu
 * @param wraplen Width to wrap to
 */
void mutt_help(enum MenuType menu, int wraplen)
{
  char buf[128];
  FILE *fp = NULL;

  /* We don't use the buffer pool because of the extended lifetime of t */
  struct Buffer t = mutt_buffer_make(PATH_MAX);
  mutt_buffer_mktemp(&t);

  const struct Binding *funcs = km_get_table(menu);
  const char *desc = mutt_map_get_name(menu, Menus);
  if (!desc)
    desc = _("<UNKNOWN>");

  do
  {
    fp = mutt_file_fopen(mutt_b2s(&t), "w");
    if (!fp)
    {
      mutt_perror(mutt_b2s(&t));
      goto cleanup;
    }

    dump_menu(fp, menu, wraplen);
    if ((menu != MENU_EDITOR) && (menu != MENU_PAGER))
    {
      fprintf(fp, "\n%s\n\n", _("Generic bindings:"));
      dump_menu(fp, MENU_GENERIC, wraplen);
    }

    fprintf(fp, "\n%s\n\n", _("Unbound functions:"));
    if (funcs)
      dump_unbound(fp, funcs, Keymaps[menu], NULL, wraplen);
    if (menu != MENU_PAGER)
      dump_unbound(fp, OpGeneric, Keymaps[MENU_GENERIC], Keymaps[menu], wraplen);

    mutt_file_fclose(&fp);

    snprintf(buf, sizeof(buf), _("Help for %s"), desc);
  } while (mutt_do_pager(buf, mutt_b2s(&t),
                         MUTT_PAGER_RETWINCH | MUTT_PAGER_MARKER | MUTT_PAGER_NSKIP | MUTT_PAGER_NOWRAP,
                         NULL) == OP_REFORMAT_WINCH);

cleanup:
  mutt_buffer_dealloc(&t);
}
