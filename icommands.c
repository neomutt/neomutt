/**
 * @file
 * Information commands
 *
 * @authors
 * Copyright (C) 2016 Christopher John Czettel <chris@meicloud.at>
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019 Victor Fernandes <criw@pm.me>
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
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include "mutt/mutt.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "icommands.h"
#include "globals.h"
#include "keymap.h"
#include "muttlib.h"
#include "opcodes.h"
#include "pager.h"
#include "version.h"

/**
 * @page icommands Information commands
 *
 * Information commands
 */

// clang-format off
static enum CommandResult icmd_bind   (struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
static enum CommandResult icmd_set    (struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
static enum CommandResult icmd_version(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);

/**
 * ICommandList - All available informational commands
 *
 * @note These commands take precedence over conventional NeoMutt rc-lines
 */
const struct ICommand ICommandList[] = {
  { "bind",     icmd_bind,     0 },
  { "macro",    icmd_bind,     1 },
  { "set",      icmd_set,      0 },
  { "version",  icmd_version,  0 },
  { NULL,       NULL,          0 },
};
// clang-format on

/**
 * mutt_parse_icommand - Parse an informational command
 * @param line Command to execute
 * @param err  Buffer for error messages
 * @retval #MUTT_CMD_SUCCESS Success
 * @retval #MUTT_CMD_ERROR   Error (no message): command not found
 * @retval #MUTT_CMD_ERROR   Error with message: command failed
 * @retval #MUTT_CMD_WARNING Warning with message: command failed
 */
enum CommandResult mutt_parse_icommand(/* const */ char *line, struct Buffer *err)
{
  if (!line || !*line || !err)
    return MUTT_CMD_ERROR;

  enum CommandResult rc = MUTT_CMD_ERROR;

  struct Buffer *token = mutt_buffer_pool_get();
  struct Buffer expn = mutt_buffer_make(0);
  mutt_buffer_addstr(&expn, line);
  expn.dptr = expn.data;

  mutt_buffer_reset(err);

  SKIPWS(expn.dptr);
  mutt_extract_token(token, &expn, MUTT_TOKEN_NO_FLAGS);
  for (size_t i = 0; ICommandList[i].name; i++)
  {
    if (mutt_str_strcmp(token->data, ICommandList[i].name) != 0)
      continue;

    rc = ICommandList[i].func(token, &expn, ICommandList[i].data, err);
    if (rc != 0)
      goto finish;

    break; /* Continue with next command */
  }

finish:
  mutt_buffer_pool_release(&token);
  mutt_buffer_dealloc(&expn);
  return rc;
}

/**
 * dump_bind - Dump a bind map to a buffer
 * @param buf  Output buffer
 * @param menu Map menu
 * @param map  Bind keymap
 */
static void dump_bind(struct Buffer *buf, struct Mapping *menu, struct Keymap *map)
{
  char key_binding[32];
  const char *fn_name = NULL;

  km_expand_key(key_binding, sizeof(key_binding), map);
  if (map->op == OP_NULL)
  {
    mutt_buffer_add_printf(buf, "bind %s %s noop\n", menu->name, key_binding);
    return;
  }

  /* The pager and editor menus don't use the generic map,
   * however for other menus try generic first. */
  if ((menu->value != MENU_PAGER) && (menu->value != MENU_EDITOR) && (menu->value != MENU_GENERIC))
  {
    fn_name = mutt_get_func(OpGeneric, map->op);
  }

  /* if it's one of the menus above or generic doesn't find
   * the function, try with its own menu. */
  if (!fn_name)
  {
    const struct Binding *bindings = km_get_table(menu->value);
    if (!bindings)
      return;

    fn_name = mutt_get_func(bindings, map->op);
  }

  mutt_buffer_add_printf(buf, "bind %s %s %s\n", menu->name, key_binding, fn_name);
}

/**
 * dump_macro - Dump a macro map to a buffer
 * @param buf  Output buffer
 * @param menu Map menu
 * @param map  Macro keymap
 */
static void dump_macro(struct Buffer *buf, struct Mapping *menu, struct Keymap *map)
{
  char key_binding[MAX_SEQ];
  km_expand_key(key_binding, MAX_SEQ, map);

  struct Buffer tmp = mutt_buffer_make(0);
  escape_string(&tmp, map->macro);

  if (map->desc)
  {
    mutt_buffer_add_printf(buf, "macro %s %s \"%s\" \"%s\"\n", menu->name,
                           key_binding, tmp.data, map->desc);
  }
  else
  {
    mutt_buffer_add_printf(buf, "macro %s %s \"%s\"\n", menu->name, key_binding, tmp.data);
  }

  mutt_buffer_dealloc(&tmp);
}

/**
 * dump_menu - Dumps all the binds or macros maps of a menu into a buffer
 * @param buf   Output buffer
 * @param menu  Menu to dump
 * @param bind  If true it's :bind, else :macro
 * @retval bool true if menu is empty, false if not
 */
static bool dump_menu(struct Buffer *buf, struct Mapping *menu, bool bind)
{
  bool empty = true;
  struct Keymap *map = NULL, *next = NULL;

  for (map = Keymaps[menu->value]; map; map = next)
  {
    next = map->next;

    if (bind && (map->op != OP_MACRO))
    {
      empty = false;
      dump_bind(buf, menu, map);
    }
    else if (!bind && (map->op == OP_MACRO))
    {
      empty = false;
      dump_macro(buf, menu, map);
    }
  }

  return empty;
}

/**
 * dump_all_menus - Dumps all the binds or macros inside every menu
 * @param buf  Output buffer
 * @param bind If true it's :bind, else :macro
 */
static void dump_all_menus(struct Buffer *buf, bool bind)
{
  for (int i = 0; i < MENU_MAX; i++)
  {
    const char *menu_name = mutt_map_get_name(i, Menus);
    struct Mapping menu = { menu_name, i };

    const bool empty = dump_menu(buf, &menu, bind);

    /* Add a new line for readability between menus. */
    if (!empty && (i < (MENU_MAX - 1)))
      mutt_buffer_addch(buf, '\n');
  }
}

/**
 * icmd_bind - Parse 'bind' and 'macro' commands - Implements ::icommand_t
 */
static enum CommandResult icmd_bind(struct Buffer *buf, struct Buffer *s,
                                    unsigned long data, struct Buffer *err)
{
  FILE *fp_out = NULL;
  char tempfile[PATH_MAX];
  bool dump_all = false, bind = (data == 0);

  if (!MoreArgs(s))
    dump_all = true;
  else
    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

  if (MoreArgs(s))
  {
    /* More arguments potentially means the user is using the
     * ::command_t :bind command thus we delegate the task. */
    return MUTT_CMD_ERROR;
  }

  struct Buffer filebuf = mutt_buffer_make(4096);
  if (dump_all || (mutt_str_strcasecmp(buf->data, "all") == 0))
  {
    dump_all_menus(&filebuf, bind);
  }
  else
  {
    const int menu_index = mutt_map_get_value(buf->data, Menus);
    if (menu_index == -1)
    {
      // L10N: '%s' is the (misspelled) name of the menu, e.g. 'index' or 'pager'
      mutt_buffer_printf(err, _("%s: no such menu"), buf->data);
      mutt_buffer_dealloc(&filebuf);
      return MUTT_CMD_ERROR;
    }

    struct Mapping menu = { buf->data, menu_index };
    dump_menu(&filebuf, &menu, bind);
  }

  if (mutt_buffer_is_empty(&filebuf))
  {
    // L10N: '%s' is the name of the menu, e.g. 'index' or 'pager', it might
    // L10N: also be 'all' when all menus are affected.
    mutt_buffer_printf(err, bind ? _("%s: no binds for this menu") : _("%s: no macros for this menu"),
                       dump_all ? "all" : buf->data);
    mutt_buffer_dealloc(&filebuf);
    return MUTT_CMD_ERROR;
  }

  mutt_mktemp(tempfile, sizeof(tempfile));
  fp_out = mutt_file_fopen(tempfile, "w");
  if (!fp_out)
  {
    // L10N: '%s' is the file name of the temporary file
    mutt_buffer_printf(err, _("Could not create temporary file %s"), tempfile);
    mutt_buffer_dealloc(&filebuf);
    return MUTT_CMD_ERROR;
  }
  fputs(filebuf.data, fp_out);

  mutt_file_fclose(&fp_out);
  mutt_buffer_dealloc(&filebuf);

  if (mutt_do_pager((bind) ? "bind" : "macro", tempfile, MUTT_PAGER_NO_FLAGS, NULL) == -1)
  {
    // L10N: '%s' is the file name of the temporary file
    mutt_buffer_printf(err, _("Could not create temporary file %s"), tempfile);
    return MUTT_CMD_ERROR;
  }

  return MUTT_CMD_SUCCESS;
}

/**
 * icmd_set - Parse 'set' command to display config - Implements ::icommand_t
 */
static enum CommandResult icmd_set(struct Buffer *buf, struct Buffer *s,
                                   unsigned long data, struct Buffer *err)
{
  char tempfile[PATH_MAX];
  mutt_mktemp(tempfile, sizeof(tempfile));

  FILE *fp_out = mutt_file_fopen(tempfile, "w");
  if (!fp_out)
  {
    // L10N: '%s' is the file name of the temporary file
    mutt_buffer_printf(err, _("Could not create temporary file %s"), tempfile);
    return MUTT_CMD_ERROR;
  }

  if (mutt_str_strcmp(s->data, "set all") == 0)
  {
    dump_config(NeoMutt->sub->cs, CS_DUMP_NO_FLAGS, fp_out);
  }
  else if (mutt_str_strcmp(s->data, "set") == 0)
  {
    dump_config(NeoMutt->sub->cs, CS_DUMP_ONLY_CHANGED, fp_out);
  }
  else
  {
    mutt_file_fclose(&fp_out);
    return MUTT_CMD_ERROR;
  }

  mutt_file_fclose(&fp_out);

  if (mutt_do_pager("set", tempfile, MUTT_PAGER_NO_FLAGS, NULL) == -1)
  {
    // L10N: '%s' is the file name of the temporary file
    mutt_buffer_printf(err, _("Could not create temporary file %s"), tempfile);
    return MUTT_CMD_ERROR;
  }

  return MUTT_CMD_SUCCESS;
}

/**
 * icmd_version - Parse 'version' command - Implements ::icommand_t
 */
static enum CommandResult icmd_version(struct Buffer *buf, struct Buffer *s,
                                       unsigned long data, struct Buffer *err)
{
  char tempfile[PATH_MAX];
  mutt_mktemp(tempfile, sizeof(tempfile));

  FILE *fp_out = mutt_file_fopen(tempfile, "w");
  if (!fp_out)
  {
    // L10N: '%s' is the file name of the temporary file
    mutt_buffer_printf(err, _("Could not create temporary file %s"), tempfile);
    return MUTT_CMD_ERROR;
  }

  print_version(fp_out);
  mutt_file_fclose(&fp_out);

  if (mutt_do_pager("version", tempfile, MUTT_PAGER_NO_FLAGS, NULL) == -1)
  {
    // L10N: '%s' is the file name of the temporary file
    mutt_buffer_printf(err, _("Could not create temporary file %s"), tempfile);
    return MUTT_CMD_ERROR;
  }

  return MUTT_CMD_SUCCESS;
}
