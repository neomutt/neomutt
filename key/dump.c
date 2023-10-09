/**
 * @file
 * Dump key bindings
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "pager/lib.h"
#include "parse/lib.h"

/**
 * dump_bind - Dumps all the binds maps of a menu into a buffer
 * @param buf  Output buffer
 * @param menu Menu to dump
 * @param name Menu name
 * @retval true  Menu is empty
 * @retval false Menu is not empty
 */
static bool dump_bind(struct Buffer *buf, enum MenuType menu, const char *name)
{
  bool empty = true;
  struct Keymap *map = NULL;

  STAILQ_FOREACH(map, &Keymaps[menu], entries)
  {
    if (map->op == OP_MACRO)
      continue;

    char key_binding[128] = { 0 };
    const char *fn_name = NULL;

    km_expand_key(key_binding, sizeof(key_binding), map);
    if (map->op == OP_NULL)
    {
      buf_add_printf(buf, "bind %s %s noop\n", name, key_binding);
      continue;
    }

    /* The pager and editor menus don't use the generic map,
     * however for other menus try generic first. */
    if ((menu != MENU_PAGER) && (menu != MENU_EDITOR) && (menu != MENU_GENERIC))
    {
      fn_name = mutt_get_func(OpGeneric, map->op);
    }

    /* if it's one of the menus above or generic doesn't find
     * the function, try with its own menu. */
    if (!fn_name)
    {
      const struct MenuFuncOp *funcs = km_get_table(menu);
      if (!funcs)
        continue;

      fn_name = mutt_get_func(funcs, map->op);
    }

    buf_add_printf(buf, "bind %s %s %s\n", name, key_binding, fn_name);
    empty = false;
  }

  return empty;
}

/**
 * dump_all_binds - Dumps all the binds inside every menu
 * @param buf  Output buffer
 */
static void dump_all_binds(struct Buffer *buf)
{
  for (enum MenuType i = 1; i < MENU_MAX; i++)
  {
    const bool empty = dump_bind(buf, i, mutt_map_get_name(i, MenuNames));

    /* Add a new line for readability between menus. */
    if (!empty && (i < (MENU_MAX - 1)))
      buf_addch(buf, '\n');
  }
}

/**
 * dump_macro - Dumps all the macros maps of a menu into a buffer
 * @param buf  Output buffer
 * @param menu Menu to dump
 * @param name Menu name
 * @retval true  Menu is empty
 * @retval false Menu is not empty
 */
static bool dump_macro(struct Buffer *buf, enum MenuType menu, const char *name)
{
  bool empty = true;
  struct Keymap *map = NULL;

  STAILQ_FOREACH(map, &Keymaps[menu], entries)
  {
    if (map->op != OP_MACRO)
      continue;

    char key_binding[128] = { 0 };
    km_expand_key(key_binding, sizeof(key_binding), map);

    struct Buffer tmp = buf_make(0);
    escape_string(&tmp, map->macro);

    if (map->desc)
    {
      buf_add_printf(buf, "macro %s %s \"%s\" \"%s\"\n", name, key_binding,
                     tmp.data, map->desc);
    }
    else
    {
      buf_add_printf(buf, "macro %s %s \"%s\"\n", name, key_binding, tmp.data);
    }

    buf_dealloc(&tmp);
    empty = false;
  }

  return empty;
}

/**
 * dump_all_macros - Dumps all the macros inside every menu
 * @param buf  Output buffer
 */
static void dump_all_macros(struct Buffer *buf)
{
  for (enum MenuType i = 1; i < MENU_MAX; i++)
  {
    const bool empty = dump_macro(buf, i, mutt_map_get_name(i, MenuNames));

    /* Add a new line for legibility between menus. */
    if (!empty && (i < (MENU_MAX - 1)))
      buf_addch(buf, '\n');
  }
}

/**
 * dump_bind_macro - Parse 'bind' and 'macro' commands - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult dump_bind_macro(struct Buffer *buf, struct Buffer *s,
                                   intptr_t data, struct Buffer *err)
{
  FILE *fp_out = NULL;
  bool dump_all = false, bind = (data == 0);

  if (!MoreArgs(s))
    dump_all = true;
  else
    parse_extract_token(buf, s, TOKEN_NO_FLAGS);

  if (MoreArgs(s))
  {
    /* More arguments potentially means the user is using the
     * ::command_t :bind command thus we delegate the task. */
    return MUTT_CMD_ERROR;
  }

  struct Buffer filebuf = buf_make(4096);
  if (dump_all || mutt_istr_equal(buf_string(buf), "all"))
  {
    if (bind)
      dump_all_binds(&filebuf);
    else
      dump_all_macros(&filebuf);
  }
  else
  {
    const int menu_index = mutt_map_get_value(buf_string(buf), MenuNames);
    if (menu_index == -1)
    {
      // L10N: '%s' is the (misspelled) name of the menu, e.g. 'index' or 'pager'
      buf_printf(err, _("%s: no such menu"), buf_string(buf));
      buf_dealloc(&filebuf);
      return MUTT_CMD_ERROR;
    }

    if (bind)
      dump_bind(&filebuf, menu_index, buf_string(buf));
    else
      dump_macro(&filebuf, menu_index, buf_string(buf));
  }

  if (buf_is_empty(&filebuf))
  {
    // L10N: '%s' is the name of the menu, e.g. 'index' or 'pager',
    //       it might also be 'all' when all menus are affected.
    buf_printf(err, bind ? _("%s: no binds for this menu") : _("%s: no macros for this menu"),
               dump_all ? "all" : buf_string(buf));
    buf_dealloc(&filebuf);
    return MUTT_CMD_ERROR;
  }

  struct Buffer *tempfile = buf_pool_get();
  buf_mktemp(tempfile);
  fp_out = mutt_file_fopen(buf_string(tempfile), "w");
  if (!fp_out)
  {
    // L10N: '%s' is the file name of the temporary file
    buf_printf(err, _("Could not create temporary file %s"), buf_string(tempfile));
    buf_dealloc(&filebuf);
    buf_pool_release(&tempfile);
    return MUTT_CMD_ERROR;
  }
  fputs(filebuf.data, fp_out);

  mutt_file_fclose(&fp_out);
  buf_dealloc(&filebuf);

  struct PagerData pdata = { 0 };
  struct PagerView pview = { &pdata };

  pdata.fname = buf_string(tempfile);

  pview.banner = (bind) ? "bind" : "macro";
  pview.flags = MUTT_PAGER_NO_FLAGS;
  pview.mode = PAGER_MODE_OTHER;

  mutt_do_pager(&pview, NULL);
  buf_pool_release(&tempfile);

  return MUTT_CMD_SUCCESS;
}
