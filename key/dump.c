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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "pager/lib.h"
#include "parse/lib.h"
#ifdef ENABLE_NLS
#include <libintl.h>
#endif

/**
 * print_bind - Display the bindings for one menu
 * @param menu Menu type
 * @param fp   File to write to
 * @retval num Number of bindings
 */
static int print_bind(enum MenuType menu, FILE *fp)
{
  struct BindingInfoArray bia_bind = ARRAY_HEAD_INITIALIZER;

  gather_menu(menu, &bia_bind, NULL);
  if (ARRAY_EMPTY(&bia_bind))
    return 0;

  ARRAY_SORT(&bia_bind, binding_sort, NULL);
  const int wb0 = measure_column(&bia_bind, 0);
  const int wb1 = measure_column(&bia_bind, 1);

  const char *menu_name = mutt_map_get_name(menu, MenuNames);

  struct BindingInfo *bi = NULL;
  ARRAY_FOREACH(bi, &bia_bind)
  {
    //XXX use description?
    fprintf(fp, "bind %s %*s  %*s  # %s\n", menu_name, -wb0, bi->a[0], -wb1,
            bi->a[1], bi->a[2]);
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
 * @param fp   File to write to
 */
static void colon_bind(enum MenuType menu, FILE *fp)
{
  if (menu == MENU_MAX)
  {
    for (enum MenuType i = 1; i < MENU_MAX; i++)
    {
      print_bind(i, fp);

      //XXX need to elide last blank line
      fprintf(fp, "\n");
    }
  }
  else
  {
    print_bind(menu, fp);
  }
}

/**
 * print_macro - Display the macros for one menu
 * @param menu Menu type
 * @param fp   File to write to
 * @retval num Number of macros
 */
static int print_macro(enum MenuType menu, FILE *fp)
{
  struct BindingInfoArray bia_macro = ARRAY_HEAD_INITIALIZER;

  gather_menu(menu, NULL, &bia_macro);
  if (ARRAY_EMPTY(&bia_macro))
    return 0;

  ARRAY_SORT(&bia_macro, binding_sort, NULL);
  const int wm0 = measure_column(&bia_macro, 0);

  const char *menu_name = mutt_map_get_name(menu, MenuNames);

  struct BindingInfo *bi = NULL;
  ARRAY_FOREACH(bi, &bia_macro)
  {
    if (bi->a[2]) // description
    {
      fprintf(fp, "macro %s %*s  \"%s\"  \"%s\"\n", menu_name, -wm0, bi->a[0],
              bi->a[1], bi->a[2]);
    }
    else
    {
      fprintf(fp, "macro %s %*s  \"%s\"\n", menu_name, -wm0, bi->a[0], bi->a[1]);
    }
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
 * @param fp   File to write to
 */
static void colon_macro(enum MenuType menu, FILE *fp)
{
  if (menu == MENU_MAX)
  {
    for (enum MenuType i = 1; i < MENU_MAX; i++)
    {
      if (print_macro(i, fp) > 0)
      {
        //XXX need to elide last blank line
        fprintf(fp, "\n");
      }
    }
  }
  else
  {
    print_macro(menu, fp);
  }
}

/**
 * dump_bind_macro - Parse 'bind' and 'macro' commands - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult dump_bind_macro(struct Buffer *buf, struct Buffer *s,
                                   intptr_t data, struct Buffer *err)
{
  FILE *fp = NULL;
  struct Buffer *tempfile = NULL;
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

  tempfile = buf_pool_get();
  buf_mktemp(tempfile);
  fp = mutt_file_fopen(buf_string(tempfile), "w");
  if (!fp)
  {
    // L10N: '%s' is the file name of the temporary file
    buf_printf(err, _("Could not create temporary file %s"), buf_string(tempfile));
    goto done;
  }

  if (dump_all || mutt_istr_equal(buf_string(buf), "all"))
  {
    if (bind)
      colon_bind(MENU_MAX, fp);
    else
      colon_macro(MENU_MAX, fp);
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
      colon_bind(menu, fp);
    else
      colon_macro(menu, fp);
  }

  if (ftello(fp) == 0)
  {
    // L10N: '%s' is the name of the menu, e.g. 'index' or 'pager',
    //       it might also be 'all' when all menus are affected.
    buf_printf(err, bind ? _("%s: no binds for this menu") : _("%s: no macros for this menu"),
               dump_all ? "all" : buf_string(buf));
    goto done;
  }
  mutt_file_fclose(&fp);

  struct PagerData pdata = { 0 };
  struct PagerView pview = { &pdata };

  pdata.fname = buf_string(tempfile);

  pview.banner = bind ? "bind" : "macro";
  pview.flags = MUTT_PAGER_NO_FLAGS;
  pview.mode = PAGER_MODE_OTHER;

  mutt_do_pager(&pview, NULL);
  rc = MUTT_CMD_SUCCESS;

done:
  mutt_file_fclose(&fp);
  buf_pool_release(&tempfile);

  return rc;
}
