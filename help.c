/**
 * @file
 * Generate the help-page and GUI display it
 *
 * @authors
 * Copyright (C) 1996-2000,2009 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2018-2020 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020 Yousef Akbar <yousef@yhakbar.com>
 * Copyright (C) 2021 Ihor Antonov <ihor@antonovs.family>
 * Copyright (C) 2023 Tóth János <gomba007@gmail.com>
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
 * @page neo_help Generate the help-page and GUI display it
 *
 * Generate and help-page and GUI display it
 */

#include "config.h"
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "pager/lib.h"
#include "hdrline.h"
#include "protos.h"

/**
 * help_lookup_function - Find a keybinding for an operation
 * @param op   Operation, e.g. OP_DELETE
 * @param menu Current Menu, e.g. #MENU_PAGER
 * @retval ptr  Key binding
 * @retval NULL No key binding found
 */
static const struct MenuFuncOp *help_lookup_function(int op, enum MenuType menu)
{
  if (menu != MENU_PAGER && (menu != MENU_GENERIC))
  {
    /* first look in the generic map for the function */
    for (int i = 0; OpGeneric[i].name; i++)
      if (OpGeneric[i].op == op)
        return &OpGeneric[i];
  }

  const struct MenuFuncOp *funcs = km_get_table(menu);
  if (funcs)
  {
    for (int i = 0; funcs[i].name; i++)
      if (funcs[i].op == op)
        return &funcs[i];
  }

  return NULL;
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
  wchar_t wc = 0;
  size_t k;
  size_t len = mutt_str_len(*macro);
  mbstate_t mbstate1 = { 0 };
  mbstate_t mbstate2 = { 0 };

  for (; len && (k = mbrtowc(&wc, *macro, len, &mbstate1)); *macro += k, len -= k)
  {
    if ((k == ICONV_ILLEGAL_SEQ) || (k == ICONV_BUF_TOO_SMALL))
    {
      if (k == ICONV_ILLEGAL_SEQ)
        memset(&mbstate1, 0, sizeof(mbstate1));
      k = (k == ICONV_ILLEGAL_SEQ) ? 1 : len;
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
        if (((n1 = wcrtomb(buf, wc, &mbstate2)) != ICONV_ILLEGAL_SEQ) &&
            ((n2 = wcrtomb(buf + n1, 0, &mbstate2)) != ICONV_ILLEGAL_SEQ))
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
  wchar_t wc = 0;
  size_t k;
  size_t m, n;
  size_t len = mutt_str_len(t);
  const char *s = t;
  mbstate_t mbstate = { 0 };

  for (m = wid, n = 0; len && (k = mbrtowc(&wc, s, len, &mbstate)) && (n <= wid);
       s += k, len -= k)
  {
    if (*s == ' ')
      m = n;
    if ((k == ICONV_ILLEGAL_SEQ) || (k == ICONV_BUF_TOO_SMALL))
    {
      if (k == ICONV_ILLEGAL_SEQ)
        memset(&mbstate, 0, sizeof(mbstate));
      k = (k == ICONV_ILLEGAL_SEQ) ? 1 : len;
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

  const char *const c_pager = pager_get_pager(NeoMutt->sub);
  if (ismacro > 0)
  {
    if (!c_pager)
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
        if (c_pager)
        {
          fputc('\n', fp);
          n = 0;
        }
        else
        {
          n += col - wraplen;
          const bool c_markers = cs_subset_bool(NeoMutt->sub, "markers");
          if (c_markers)
            n++;
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
  char buf[128] = { 0 };

  STAILQ_FOREACH(map, &Keymaps[menu], entries)
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
        const struct MenuFuncOp *funcs = help_lookup_function(map->op, menu);
        format_line(fp, 0, buf, funcs ? funcs->name : "UNKNOWN",
                    funcs ? _(opcodes_get_description(funcs->op)) :
                            _("ERROR: please report this bug"),
                    wraplen);
      }
    }
  }
}

/**
 * is_bound - Does a function have a keybinding?
 * @param km_list Keymap to examine
 * @param op      Operation, e.g. OP_DELETE
 * @retval true A key is bound to that operation
 */
static bool is_bound(struct KeymapList *km_list, int op)
{
  struct Keymap *map = NULL;
  STAILQ_FOREACH(map, km_list, entries)
  {
    if (map->op == op)
      return true;
  }
  return false;
}

/**
 * dump_unbound - Write out all the operations with no key bindings
 * @param fp      File to write to
 * @param funcs   All the bindings for the current menu
 * @param km_list First key map to consider
 * @param aux     Second key map to consider
 * @param wraplen Width to wrap to
 */
static void dump_unbound(FILE *fp, const struct MenuFuncOp *funcs,
                         struct KeymapList *km_list, struct KeymapList *aux, int wraplen)
{
  for (int i = 0; funcs[i].name; i++)
  {
    if (!is_bound(km_list, funcs[i].op) && (!aux || !is_bound(aux, funcs[i].op)))
      format_line(fp, 0, funcs[i].name, "", _(opcodes_get_description(funcs[i].op)), wraplen);
  }
}

/**
 * show_flag_if_present - Write out a message flag if exists
 * @param fp              File to write to
 * @param wraplen         Width to wrap to
 * @param table           Table containing the flag characters
 * @param index           Index of flag character int the table
 * @param description     Description of flag
 */
static void show_flag_if_present(FILE *fp, int wraplen, const struct MbTable *table,
                                 int index, char *description)
{
  const char *flag = mbtable_get_nth_wchar(table, index);
  if ((strlen(flag) < 1) || (*flag == ' '))
  {
    return;
  }

  format_line(fp, 0, flag, "", description, wraplen);
}

/**
 * dump_message_flags - Write out all the message flags
 * @param fp            File to write to
 * @param wraplen       Width to wrap to
 */
static void dump_message_flags(FILE *fp, int wraplen)
{
  const struct MbTable *c_flag_chars = cs_subset_mbtable(NeoMutt->sub, "flag_chars");
  const struct MbTable *c_crypt_chars = cs_subset_mbtable(NeoMutt->sub, "crypt_chars");
  const struct MbTable *c_to_chars = cs_subset_mbtable(NeoMutt->sub, "to_chars");

  format_line(fp, 0, "$flag_chars:", "", "", wraplen);
  show_flag_if_present(fp, wraplen, c_flag_chars, FLAG_CHAR_TAGGED, _("message is tagged"));
  show_flag_if_present(fp, wraplen, c_flag_chars, FLAG_CHAR_IMPORTANT,
                       _("message is flagged"));
  show_flag_if_present(fp, wraplen, c_flag_chars, FLAG_CHAR_DELETED, _("message is deleted"));
  show_flag_if_present(fp, wraplen, c_flag_chars, FLAG_CHAR_DELETED_ATTACH,
                       _("attachment is deleted"));
  show_flag_if_present(fp, wraplen, c_flag_chars, FLAG_CHAR_REPLIED,
                       _("message has been replied to"));
  show_flag_if_present(fp, wraplen, c_flag_chars, FLAG_CHAR_OLD, _("message has been read"));
  show_flag_if_present(fp, wraplen, c_flag_chars, FLAG_CHAR_NEW, _("message is new"));
  show_flag_if_present(fp, wraplen, c_flag_chars, FLAG_CHAR_OLD_THREAD,
                       _("thread has been read"));
  show_flag_if_present(fp, wraplen, c_flag_chars, FLAG_CHAR_NEW_THREAD,
                       _("thread has at least one new message"));
  show_flag_if_present(fp, wraplen, c_flag_chars, FLAG_CHAR_SEMPTY,
                       _("message has been read (%S expando)"));
  show_flag_if_present(fp, wraplen, c_flag_chars, FLAG_CHAR_ZEMPTY,
                       _("message has been read (%Z expando)"));

  format_line(fp, 0, "\n$crypt_chars:", "", "", wraplen);
  show_flag_if_present(fp, wraplen, c_crypt_chars, FLAG_CHAR_CRYPT_GOOD_SIGN,
                       _("message signed with a verified key"));
  show_flag_if_present(fp, wraplen, c_crypt_chars, FLAG_CHAR_CRYPT_ENCRYPTED,
                       _("message is PGP-encrypted"));
  show_flag_if_present(fp, wraplen, c_crypt_chars, FLAG_CHAR_CRYPT_SIGNED,
                       _("message is signed"));
  show_flag_if_present(fp, wraplen, c_crypt_chars, FLAG_CHAR_CRYPT_CONTAINS_KEY,
                       _("message contains a PGP key"));
  show_flag_if_present(fp, wraplen, c_crypt_chars, FLAG_CHAR_CRYPT_NO_CRYPTO,
                       _("message has no cryptography information"));

  format_line(fp, 0, "\n$to_chars:", "", "", wraplen);
  show_flag_if_present(fp, wraplen, c_to_chars, FLAG_CHAR_TO_NOT_IN_THE_LIST,
                       _("message is not To: you"));
  show_flag_if_present(fp, wraplen, c_to_chars, FLAG_CHAR_TO_UNIQUE,
                       _("message is To: you and only you"));
  show_flag_if_present(fp, wraplen, c_to_chars, FLAG_CHAR_TO_TO, _("message is To: you"));
  show_flag_if_present(fp, wraplen, c_to_chars, FLAG_CHAR_TO_CC, _("message is Cc: to you"));
  show_flag_if_present(fp, wraplen, c_to_chars, FLAG_CHAR_TO_ORIGINATOR,
                       _("message is From: you"));
  show_flag_if_present(fp, wraplen, c_to_chars, FLAG_CHAR_TO_SUBSCRIBED_LIST,
                       _("message is sent to a subscribed mailing list"));
  show_flag_if_present(fp, wraplen, c_to_chars, FLAG_CHAR_TO_REPLY_TO,
                       _("you are in the Reply-To: list"));
}

/**
 * mutt_help - Display the help menu
 * @param menu    Current Menu
 */
void mutt_help(enum MenuType menu)
{
  char banner[128] = { 0 };
  FILE *fp = NULL;

  struct Buffer *tmp_file = buf_pool_get();
  buf_mktemp(tmp_file);

  const struct MenuFuncOp *funcs = km_get_table(menu);
  const char *desc = mutt_map_get_name(menu, MenuNames);
  if (!desc)
    desc = _("<UNKNOWN>");

  struct PagerData pdata = { 0 };
  struct PagerView pview = { &pdata };

  pview.mode = PAGER_MODE_HELP;
  pview.flags = MUTT_PAGER_RETWINCH | MUTT_PAGER_MARKER | MUTT_PAGER_NSKIP |
                MUTT_PAGER_NOWRAP | MUTT_PAGER_STRIPES;

  do
  {
    fp = mutt_file_fopen(buf_string(tmp_file), "w");
    if (!fp)
    {
      mutt_perror("%s", buf_string(tmp_file));
      goto cleanup;
    }

    const int wraplen = AllDialogsWindow->state.cols;
    dump_menu(fp, menu, wraplen);
    if ((menu != MENU_EDITOR) && (menu != MENU_PAGER) && (menu != MENU_GENERIC))
    {
      fprintf(fp, "\n%s\n\n", _("Generic bindings:"));
      dump_menu(fp, MENU_GENERIC, wraplen);
    }

    fprintf(fp, "\n%s\n\n", _("Unbound functions:"));
    if (funcs)
      dump_unbound(fp, funcs, &Keymaps[menu], NULL, wraplen);
    if ((menu != MENU_EDITOR) && (menu != MENU_PAGER) && (menu != MENU_GENERIC))
      dump_unbound(fp, OpGeneric, &Keymaps[MENU_GENERIC], &Keymaps[menu], wraplen);

    if (menu == MENU_INDEX)
    {
      fprintf(fp, "\n%s\n\n", _("Message flags:"));
      dump_message_flags(fp, wraplen);
    }

    mutt_file_fclose(&fp);

    snprintf(banner, sizeof(banner), _("Help for %s"), desc);
    pdata.fname = buf_string(tmp_file);
    pview.banner = banner;
  } while (mutt_do_pager(&pview, NULL) == OP_REFORMAT_WINCH);

cleanup:
  buf_pool_release(&tmp_file);
}
