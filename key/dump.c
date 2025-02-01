/**
 * @file
 * Dump key bindings
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
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
 * @page key_dump Dump key bindings
 *
 * Dump key bindings
 */

#include "config.h"
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "color/lib.h"
#include "menu/lib.h"
#include "parse/lib.h"
#include "pfile/lib.h"
#include "spager/lib.h"

/**
 * print_bind - Display the bindings for one menu
 * @param menu Menu type
 * @param pf   PagedFile to write to
 * @retval num Number of bindings
 */
static int print_bind(enum MenuType menu, struct PagedFile *pf)
{
  struct BindingInfoArray bia_bind = ARRAY_HEAD_INITIALIZER;
  struct PagedRow *pr = NULL;

  gather_menu(menu, &bia_bind, NULL);
  if (ARRAY_EMPTY(&bia_bind))
    return 0;

  ARRAY_SORT(&bia_bind, binding_sort, NULL);
  const int wb0 = measure_column(&bia_bind, 0);
  const int wb1 = measure_column(&bia_bind, 1);
  int len0;
  int len1;

  const char *menu_name = mutt_map_get_name(menu, MenuNames);

  struct Buffer *buf = buf_pool_get();
  struct BindingInfo *bi = NULL;
  ARRAY_FOREACH(bi, &bia_bind)
  {
    pr = paged_file_new_row(pf);

    // bind menu
    paged_row_add_colored_text(pr, MT_COLOR_FUNCTION, "bind");
    paged_row_add_text(pr, " ");
    paged_row_add_colored_text(pr, MT_COLOR_ENUM, menu_name);
    paged_row_add_text(pr, " ");

    // keybinding
    len0 = paged_row_add_colored_text(pr, MT_COLOR_OPERATOR, bi->a[0]);
    buf_printf(buf, "%*s", wb0 - len0, "");
    paged_row_add_text(pr, buf_string(buf));
    paged_row_add_text(pr, " ");

    // function
    len1 = paged_row_add_colored_text(pr, MT_COLOR_FUNCTION, bi->a[1]);
    buf_printf(buf, "%*s", wb1 - len1, "");
    paged_row_add_text(pr, buf_string(buf));

    // function description
    paged_row_add_text(pr, " ");
    buf_printf(buf, "# %s\n", bi->a[2]);
    paged_row_add_colored_text(pr, MT_COLOR_COMMENT, buf_string(buf));
  }
  buf_pool_release(&buf);

  // Apply striping
  ARRAY_FOREACH(pr, &pf->rows)
  {
    if ((ARRAY_FOREACH_IDX_pr % 2) == 0)
      pr->cid = MT_COLOR_STRIPE_ODD;
    else
      pr->cid = MT_COLOR_STRIPE_EVEN;
  }

  const int count = ARRAY_SIZE(&bia_bind);
  ARRAY_FOREACH(bi, &bia_bind)
  {
    // we only need to free the keybinding
    FREE(&bi->a[0]);
  }

  return count;
}

/**
 * colon_bind - Dump the key bindings
 * @param menu Menu type
 * @param pf   PagedFile to write to
 */
static void colon_bind(enum MenuType menu, struct PagedFile *pf)
{
  if (menu == MENU_MAX)
  {
    struct PagedRow *pr = NULL;
    for (enum MenuType i = 1; i < MENU_MAX; i++)
    {
      if (print_bind(i, pf) > 0)
        fprintf(fp, "\n");
    }
  }
  else
  {
    print_bind(menu, pf);
  }
}

/**
 * print_macro - Display the macros for one menu
 * @param menu Menu type
 * @param pf   PagedFile to write to
 * @retval num Number of macros
 */
static int print_macro(enum MenuType menu, struct PagedFile *pf)
{
  struct BindingInfoArray bia_macro = ARRAY_HEAD_INITIALIZER;
  struct PagedRow *pr = NULL;

  gather_menu(menu, NULL, &bia_macro);
  if (ARRAY_EMPTY(&bia_macro))
    return 0;

  ARRAY_SORT(&bia_macro, binding_sort, NULL);
  const int wm0 = measure_column(&bia_macro, 0);
  int len0;

  const char *menu_name = mutt_map_get_name(menu, MenuNames);

  struct Buffer *buf = buf_pool_get();
  struct BindingInfo *bi = NULL;
  ARRAY_FOREACH(bi, &bia_macro)
  {
    pr = paged_file_new_row(pf);

    // macro menu
    paged_row_add_colored_text(pr, MT_COLOR_FUNCTION, "macro");
    paged_row_add_text(pr, " ");
    paged_row_add_colored_text(pr, MT_COLOR_ENUM, menu_name);
    paged_row_add_text(pr, " ");

    // keybinding
    len0 = paged_row_add_colored_text(pr, MT_COLOR_OPERATOR, bi->a[0]);
    buf_printf(buf, "%*s", wm0 - len0, "");
    paged_row_add_text(pr, buf_string(buf));
    paged_row_add_text(pr, " ");

    // macro text
    buf_printf(buf, "\"%s\"", bi->a[1]);
    paged_row_add_colored_text(pr, MT_COLOR_STRING, buf_string(buf));

    // description
    if (bi->a[2])
    {
      paged_row_add_text(pr, " ");
      buf_printf(buf, "\"%s\"", bi->a[2]);
      paged_row_add_colored_text(pr, MT_COLOR_STRING, buf_string(buf));
    }

    paged_row_add_newline(pr);
  }
  buf_pool_release(&buf);

  // Apply striping
  ARRAY_FOREACH(pr, &pf->rows)
  {
    if ((ARRAY_FOREACH_IDX_pr % 2) == 0)
      pr->cid = MT_COLOR_STRIPE_ODD;
    else
      pr->cid = MT_COLOR_STRIPE_EVEN;
  }

  const int count = ARRAY_SIZE(&bia_macro);
  ARRAY_FOREACH(bi, &bia_macro)
  {
    // free the keybinding and the macro text
    FREE(&bi->a[0]);
    FREE(&bi->a[1]);
  }

  ARRAY_FREE(&bia_macro);
  return count;
}

/**
 * colon_macro - Dump the macros
 * @param menu Menu type
 * @param pf   PagedFile to write to
 */
static void colon_macro(enum MenuType menu, struct PagedFile *pf)
{
  if (menu == MENU_MAX)
  {
    struct PagedRow *pr = NULL;
    for (enum MenuType i = 1; i < MENU_MAX; i++)
    {
      if (print_macro(i, pf) > 0)
      {
        //XXX need to elide last blank line
        pr = paged_file_new_row(pf);
        paged_row_add_newline(pr);
      }
    }
  }
  else
  {
    print_macro(menu, pf);
  }
}

/**
 * dump_bind_macro - Parse 'bind' and 'macro' commands - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult dump_bind_macro(struct Buffer *buf, struct Buffer *s,
                                   intptr_t data, struct Buffer *err)
{
  struct PagedFile *pf = NULL;
  bool dump_all = false;
  bool bind = (data == 0);
  int rc = MUTT_CMD_ERROR;

  if (!MoreArgs(s))
    dump_all = true;
  else
    parse_extract_token(buf, s, TOKEN_NO_FLAGS);

  if (MoreArgs(s))
  {
    /* More arguments potentially means the user is using the
     * ::command_t :bind command thus we delegate the task. */
    goto done;
  }

  pf = paged_file_new(NULL);
  if (!pf)
    goto done;

  if (dump_all || mutt_istr_equal(buf_string(buf), "all"))
  {
    if (bind)
      colon_bind(MENU_MAX, pf);
    else
      colon_macro(MENU_MAX, pf);
  }
  else
  {
    const int menu = mutt_map_get_value(buf_string(buf), MenuNames);
    if (menu == -1)
    {
      // L10N: '%s' is the (misspelled) name of the menu, e.g. 'index' or 'pager'
      buf_printf(err, _("%s: no such menu"), buf_string(buf));
      goto done;
    }

    if (bind)
      colon_bind(menu, pf);
    else
      colon_macro(menu, pf);
  }

  // XXX Check reval from colon_bind/macro
  // if (ftello(fp) == 0)
  // {
  //   // L10N: '%s' is the name of the menu, e.g. 'index' or 'pager',
  //   //       it might also be 'all' when all menus are affected.
  //   buf_printf(err, bind ? _("%s: no binds for this menu") : _("%s: no macros for this menu"),
  //              dump_all ? "all" : buf_string(buf));
  //   goto done;
  // }

  const char *banner = bind ? "bind" : "macro";
  dlg_spager(pf, banner, NeoMutt->sub);

  rc = MUTT_CMD_SUCCESS;

done:
  paged_file_free(&pf);

  return rc;
}

/**
 * binding_sort - Compare two BindingInfo by their keybinding - Implements ::sort_t - @ingroup sort_api
 */
int binding_sort(const void *a, const void *b, void *sdata)
{
  const struct BindingInfo *x = (const struct BindingInfo *) a;
  const struct BindingInfo *y = (const struct BindingInfo *) b;

  int rc = mutt_str_cmp(x->a[0], y->a[0]);
  if (rc != 0)
    return rc;

  // No binding, sort by function instead
  return mutt_str_cmp(x->a[1], y->a[1]);
}

/**
 * escape_macro - Escape any special characters in a macro
 * @param[in]  macro Macro string
 * @param[out] buf   Buffer for the result
 *
 * Replace characters, such as `<Enter>` with the literal "\n"
 */
void escape_macro(const char *macro, struct Buffer *buf)
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
 * help_lookup_function - Find a keybinding for an operation
 * @param op   Operation, e.g. OP_DELETE
 * @param menu Current Menu, e.g. #MENU_PAGER
 * @retval str  Key binding
 * @retval NULL No key binding found
 */
static const char *help_lookup_function(int op, enum MenuType menu)
{
  if ((menu != MENU_PAGER) && (menu != MENU_EDITOR) && (menu != MENU_GENERIC))
  {
    /* first look in the generic map for the function */
    const char *fn_name = mutt_get_func(OpGeneric, op);
    if (fn_name)
      return fn_name;
  }

  const struct MenuFuncOp *funcs = km_get_table(menu);

  return mutt_get_func(funcs, op);
}

/**
 * gather_menu - Gather info about one menu
 * @param menu      Menu type
 * @param bia_bind  Array for bind  results (may be NULL)
 * @param bia_macro Array for macro results (may be NULL)
 */
void gather_menu(enum MenuType menu, struct BindingInfoArray *bia_bind,
                 struct BindingInfoArray *bia_macro)
{
  struct Buffer *key_binding = buf_pool_get();
  struct Buffer *macro = buf_pool_get();

  struct Keymap *map = NULL;
  STAILQ_FOREACH(map, &Keymaps[menu], entries)
  {
    struct BindingInfo bi = { 0 };

    buf_reset(key_binding);
    km_expand_key(map, key_binding);

    if (map->op == OP_MACRO)
    {
      if (!bia_macro || (map->op == OP_NULL))
        continue;

      buf_reset(macro);
      escape_macro(map->macro, macro);
      bi.a[0] = buf_strdup(key_binding);
      bi.a[1] = buf_strdup(macro);
      bi.a[2] = map->desc;
      ARRAY_ADD(bia_macro, bi);
    }
    else
    {
      if (!bia_bind)
        continue;

      if (map->op == OP_NULL)
      {
        bi.a[0] = buf_strdup(key_binding);
        bi.a[1] = "noop";
        ARRAY_ADD(bia_bind, bi);
        continue;
      }

      bi.a[0] = buf_strdup(key_binding);
      bi.a[1] = help_lookup_function(map->op, menu);
      bi.a[2] = _(opcodes_get_description(map->op));
      ARRAY_ADD(bia_bind, bi);
    }
  }

  buf_pool_release(&key_binding);
  buf_pool_release(&macro);
}

/**
 * measure_column - Measure one column of a table
 * @param bia Array of binding info
 * @param col Column to measure
 * @retval num Width of widest column
 */
int measure_column(struct BindingInfoArray *bia, int col)
{
  int max_width = 0;

  struct BindingInfo *bi = NULL;
  ARRAY_FOREACH(bi, bia)
  {
    const int col_width = mutt_strwidth(bi->a[col]);
    max_width = MAX(max_width, col_width);
  }

  return max_width;
}
