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
#include "index/lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "pager/lib.h"
#include "protos.h"

/**
 * struct HelpLine - One line of Help text
 */
struct HelpLine
{
  const char *first;  ///< First  column
  const char *second; ///< Second column
  const char *third;  ///< Third  column
};
ARRAY_HEAD(HelpLineArray, struct HelpLine);

/**
 * help_lookup_function - Find a keybinding for an operation
 * @param op   Operation, e.g. OP_DELETE
 * @param menu Current Menu, e.g. #MENU_PAGER
 * @retval ptr  Key binding
 * @retval NULL No key binding found
 */
static const struct MenuFuncOp *help_lookup_function(int op, enum MenuType menu)
{
  if ((menu != MENU_PAGER) && (menu != MENU_GENERIC))
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
 * help_sort_alpha - Compare two Help Lines by their first entry - Implements ::sort_t - @ingroup sort_api
 */
static int help_sort_alpha(const void *a, const void *b, void *sdata)
{
  const struct HelpLine *x = (const struct HelpLine *) a;
  const struct HelpLine *y = (const struct HelpLine *) b;

  return mutt_str_cmp(x->first, y->first);
}

/**
 * escape_macro - Escape any special characters in a macro
 * @param[in]  macro Macro string
 * @param[out] buf   Buffer for the result
 *
 * Replace characters, such as `<Enter>` with the literal "\n"
 */
static void escape_macro(const char *macro, struct Buffer *buf)
{
  wchar_t wc = 0;
  size_t k;
  size_t len = mutt_str_len(macro);
  mbstate_t mbstate1 = { 0 };
  mbstate_t mbstate2 = { 0 };

  for (; (len > 0) && (k = mbrtowc(&wc, macro, MB_LEN_MAX, &mbstate1)); macro += k, len -= k)
  {
    if ((k == ICONV_ILLEGAL_SEQ) || (k == ICONV_BUF_TOO_SMALL))
    {
      if (k == ICONV_ILLEGAL_SEQ)
        memset(&mbstate1, 0, sizeof(mbstate1));
      k = (k == ICONV_ILLEGAL_SEQ) ? 1 : len;
      wc = ReplacementChar;
    }

    const int w = wcwidth(wc);
    if (IsWPrint(wc) && (w >= 0))
    {
      char tmp[MB_LEN_MAX * 2] = { 0 };
      if (wcrtomb(tmp, wc, &mbstate2) != ICONV_ILLEGAL_SEQ)
      {
        buf_addstr(buf, tmp);
      }
    }
    else if ((wc < 0x20) || (wc == 0x7f))
    {
      if (wc == '\033') // Escape
        buf_addstr(buf, "\\e");
      else if (wc == '\n')
        buf_addstr(buf, "\\n");
      else if (wc == '\r')
        buf_addstr(buf, "\\r");
      else if (wc == '\t')
        buf_addstr(buf, "\\t");
      else
        buf_add_printf(buf, "^%c", (char) ((wc + '@') & 0x7f));
    }
    else
    {
      buf_addch(buf, '?');
    }
  }
}

/**
 * dump_menu - Write all the key bindings to a HelpLine Array
 * @param menu      Menu type
 * @param hla_menu  HelpLine Array of key bindings
 * @param hla_macro HelpLine Array of macros
 *
 * For bind:  { key-string, function-name, description }
 * For macro: { key-string, macro-text,    optional-description }
 */
static void dump_menu(enum MenuType menu, struct HelpLineArray *hla_menu,
                      struct HelpLineArray *hla_macro)
{
  struct Keymap *map = NULL;
  struct Buffer *buf = buf_pool_get();

  STAILQ_FOREACH(map, &Keymaps[menu], entries)
  {
    if (map->op == OP_NULL)
      continue;

    buf_reset(buf);
    km_expand_key(map, buf);

    struct HelpLine hl = { 0 };

    hl.first = buf_strdup(buf);

    if (map->op == OP_MACRO)
    {
      hl.second = map->macro;
      hl.third = map->desc;
      ARRAY_ADD(hla_macro, hl);
    }
    else
    {
      const struct MenuFuncOp *funcs = help_lookup_function(map->op, menu);
      ASSERT(funcs);
      hl.second = funcs->name;
      hl.third = _(opcodes_get_description(funcs->op));
      ARRAY_ADD(hla_menu, hl);
    }
  }

  buf_pool_release(&buf);
}

/**
 * dump_bound - Dump the bound keys to a file
 * @param menu Menu type
 * @param fp   File to write to
 *
 * Collect all the function bindings and write them to a file.
 *
 * The output will be in three columns: binding, function, description.
 */
static void dump_bound(enum MenuType menu, FILE *fp)
{
  struct HelpLineArray hla_gen = ARRAY_HEAD_INITIALIZER;
  struct HelpLineArray hla_macro = ARRAY_HEAD_INITIALIZER;
  struct HelpLineArray hla_menu = ARRAY_HEAD_INITIALIZER;

  dump_menu(menu, &hla_menu, &hla_macro);
  if ((menu != MENU_EDITOR) && (menu != MENU_PAGER) && (menu != MENU_GENERIC))
  {
    dump_menu(MENU_GENERIC, &hla_gen, &hla_macro);
  }

  struct HelpLine *hl = NULL;
  int w1 = 0;
  int w2 = 0;
  ARRAY_FOREACH(hl, &hla_menu)
  {
    w1 = MAX(w1, mutt_str_len(hl->first));
    w2 = MAX(w2, mutt_str_len(hl->second));
  }

  ARRAY_FOREACH(hl, &hla_gen)
  {
    w1 = MAX(w1, mutt_str_len(hl->first));
    w2 = MAX(w2, mutt_str_len(hl->second));
  }

  ARRAY_FOREACH(hl, &hla_macro)
  {
    w1 = MAX(w1, mutt_str_len(hl->first));
  }

  const char *desc = mutt_map_get_name(menu, MenuNames);
  fprintf(fp, _("%s bindings:"), desc);
  fputs("\n\n", fp);

  ARRAY_FOREACH(hl, &hla_menu)
  {
    fprintf(fp, "%*s  ", -w1, hl->first);
    fprintf(fp, "%*s  %s\n", -w2, hl->second, hl->third);
  }

  fprintf(fp, "\n%s\n\n", _("Generic bindings:"));

  ARRAY_FOREACH(hl, &hla_gen)
  {
    fprintf(fp, "%*s  ", -w1, hl->first);
    fprintf(fp, "%*s  %s\n", -w2, hl->second, hl->third);
  }

  fprintf(fp, "\n%s\n\n", _("macros:"));
  struct Buffer *macro = buf_pool_get();
  ARRAY_FOREACH(hl, &hla_macro)
  {
    fprintf(fp, "%*s  ", -w1, hl->first);

    buf_reset(macro);
    escape_macro(hl->second, macro);
    fprintf(fp, "%s\n", buf_string(macro));

    if (hl->third)
      fprintf(fp, "%*s  %s\n\n", -w1, "", hl->third);
  }
  buf_pool_release(&macro);

  ARRAY_FREE(&hla_gen);
  ARRAY_FREE(&hla_macro);
  ARRAY_FREE(&hla_menu);
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
 * dump_unbound_menu - Write the operations with no key bindings to a HelpLine Array
 * @param[in]  funcs   All the bindings for the current menu
 * @param[in]  km_list First key map to consider
 * @param[in]  aux     Second key map to consider
 * @param[out] hla     HelpLine Array
 *
 * The output will be in two columns: { function-name, description }
 */
static void dump_unbound_menu(const struct MenuFuncOp *funcs, struct KeymapList *km_list,
                              struct KeymapList *aux, struct HelpLineArray *hla)
{
  for (int i = 0; funcs[i].name; i++)
  {
    if (!is_bound(km_list, funcs[i].op) && (!aux || !is_bound(aux, funcs[i].op)))
    {
      struct HelpLine hl = { 0 };
      hl.first = funcs[i].name;
      hl.second = _(opcodes_get_description(funcs[i].op));
      ARRAY_ADD(hla, hl);
    }
  }
}

/**
 * dump_unbound - Dump the unbound keys to a file
 * @param menu Menu type
 * @param fp   File to write to
 *
 * The output will be in two columns: { function-name, description }
 */
static void dump_unbound(enum MenuType menu, FILE *fp)
{
  fprintf(fp, "\n%s\n\n", _("Unbound functions:"));

  struct HelpLineArray hla = ARRAY_HEAD_INITIALIZER;

  const struct MenuFuncOp *funcs = km_get_table(menu);
  if (funcs)
    dump_unbound_menu(funcs, &Keymaps[menu], NULL, &hla);

  if ((menu != MENU_EDITOR) && (menu != MENU_PAGER) && (menu != MENU_GENERIC))
    dump_unbound_menu(OpGeneric, &Keymaps[MENU_GENERIC], &Keymaps[menu], &hla);

  struct HelpLine *hl = NULL;
  int w1 = 0;
  ARRAY_FOREACH(hl, &hla)
  {
    w1 = MAX(w1, mutt_str_len(hl->first));
  }

  ARRAY_SORT(&hla, help_sort_alpha, NULL);
  ARRAY_FOREACH(hl, &hla)
  {
    fprintf(fp, "%*s  %s\n", -w1, hl->first, hl->second);
  }

  ARRAY_FREE(&hla);
}

/**
 * show_flag_if_present - Write out a message flag if exists
 * @param fp    File to write to
 * @param table Table containing the flag characters
 * @param index Index of flag character int the table
 * @param desc  Description of flag
 */
static void show_flag_if_present(FILE *fp, const struct MbTable *table, int index, char *desc)
{
  const char *flag = mbtable_get_nth_wchar(table, index);
  if ((strlen(flag) < 1) || (*flag == ' '))
    return;

  const int cols = mutt_strwidth(flag);

  fprintf(fp, "    %s%*s  %s\n", flag, 4 - cols, "", desc);
}

/**
 * dump_message_flags - Write out all the message flags
 * @param menu Menu type
 * @param fp   File to write to
 *
 * Display a quick reminder of all the flags in the config options:
 * - $crypt_chars
 * - $flag_chars
 * - $to_chars
 */
static void dump_message_flags(enum MenuType menu, FILE *fp)
{
  if (menu != MENU_INDEX)
    return;

  fprintf(fp, "\n%s\n\n", _("Message flags:"));

  const struct MbTable *c_flag_chars = cs_subset_mbtable(NeoMutt->sub, "flag_chars");
  const struct MbTable *c_crypt_chars = cs_subset_mbtable(NeoMutt->sub, "crypt_chars");
  const struct MbTable *c_to_chars = cs_subset_mbtable(NeoMutt->sub, "to_chars");

  fputs("$flag_chars:\n", fp);
  show_flag_if_present(fp, c_flag_chars, FLAG_CHAR_TAGGED, _("message is tagged"));
  show_flag_if_present(fp, c_flag_chars, FLAG_CHAR_IMPORTANT, _("message is flagged"));
  show_flag_if_present(fp, c_flag_chars, FLAG_CHAR_DELETED, _("message is deleted"));
  show_flag_if_present(fp, c_flag_chars, FLAG_CHAR_DELETED_ATTACH, _("attachment is deleted"));
  show_flag_if_present(fp, c_flag_chars, FLAG_CHAR_REPLIED, _("message has been replied to"));
  show_flag_if_present(fp, c_flag_chars, FLAG_CHAR_OLD, _("message has been read"));
  show_flag_if_present(fp, c_flag_chars, FLAG_CHAR_NEW, _("message is new"));
  show_flag_if_present(fp, c_flag_chars, FLAG_CHAR_OLD_THREAD, _("thread has been read"));
  show_flag_if_present(fp, c_flag_chars, FLAG_CHAR_NEW_THREAD,
                       _("thread has at least one new message"));
  show_flag_if_present(fp, c_flag_chars, FLAG_CHAR_SEMPTY,
                       _("message has been read (%S expando)"));
  show_flag_if_present(fp, c_flag_chars, FLAG_CHAR_ZEMPTY,
                       _("message has been read (%Z expando)"));

  fputs("\n$crypt_chars:\n", fp);
  show_flag_if_present(fp, c_crypt_chars, FLAG_CHAR_CRYPT_GOOD_SIGN,
                       _("message signed with a verified key"));
  show_flag_if_present(fp, c_crypt_chars, FLAG_CHAR_CRYPT_ENCRYPTED,
                       _("message is PGP-encrypted"));
  show_flag_if_present(fp, c_crypt_chars, FLAG_CHAR_CRYPT_SIGNED, _("message is signed"));
  show_flag_if_present(fp, c_crypt_chars, FLAG_CHAR_CRYPT_CONTAINS_KEY,
                       _("message contains a PGP key"));
  show_flag_if_present(fp, c_crypt_chars, FLAG_CHAR_CRYPT_NO_CRYPTO,
                       _("message has no cryptography information"));

  fputs("\n$to_chars:\n", fp);
  show_flag_if_present(fp, c_to_chars, FLAG_CHAR_TO_NOT_IN_THE_LIST,
                       _("message is not To: you"));
  show_flag_if_present(fp, c_to_chars, FLAG_CHAR_TO_UNIQUE,
                       _("message is To: you and only you"));
  show_flag_if_present(fp, c_to_chars, FLAG_CHAR_TO_TO, _("message is To: you"));
  show_flag_if_present(fp, c_to_chars, FLAG_CHAR_TO_CC, _("message is Cc: to you"));
  show_flag_if_present(fp, c_to_chars, FLAG_CHAR_TO_ORIGINATOR, _("message is From: you"));
  show_flag_if_present(fp, c_to_chars, FLAG_CHAR_TO_SUBSCRIBED_LIST,
                       _("message is sent to a subscribed mailing list"));
  show_flag_if_present(fp, c_to_chars, FLAG_CHAR_TO_REPLY_TO,
                       _("you are in the Reply-To: list"));
  fputs("\n", fp);
}

/**
 * mutt_help - Display the help menu
 * @param menu    Current Menu
 */
void mutt_help(enum MenuType menu)
{
  char banner[128] = { 0 };
  FILE *fp = NULL;

  struct Buffer *tempfile = buf_pool_get();
  buf_mktemp(tempfile);

  struct PagerData pdata = { 0 };
  struct PagerView pview = { &pdata };

  pview.mode = PAGER_MODE_HELP;
  pview.flags = MUTT_PAGER_MARKER | MUTT_PAGER_NOWRAP | MUTT_PAGER_STRIPES;

  fp = mutt_file_fopen(buf_string(tempfile), "w");
  if (!fp)
  {
    mutt_perror("%s", buf_string(tempfile));
    goto cleanup;
  }

  dump_bound(menu, fp);
  dump_unbound(menu, fp);
  dump_message_flags(menu, fp);

  mutt_file_fclose(&fp);

  const char *desc = mutt_map_get_name(menu, MenuNames);
  snprintf(banner, sizeof(banner), _("Help for %s"), desc);
  pdata.fname = buf_string(tempfile);
  pview.banner = banner;

  mutt_do_pager(&pview, NULL);

cleanup:
  buf_pool_release(&tempfile);
}
