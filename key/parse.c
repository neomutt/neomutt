/**
 * @file
 * Parse key bindings
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
 * @page key_parse Parse key bindings
 *
 * Parse key bindings
 */

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "parse/lib.h"

/// Maximum length of a key binding sequence used for buffer in km_bindkey
#define MAX_SEQ 8

/**
 * km_bind_err - Set up a key binding
 * @param s     Key string
 * @param mtype Menu type, e.g. #MENU_EDITOR
 * @param op    Operation, e.g. OP_DELETE
 * @param macro Macro string
 * @param desc Description of macro (OPTIONAL)
 * @param err   Buffer for error message
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * Insert a key sequence into the specified map.
 * The map is sorted by ASCII value (lowest to highest)
 */
static enum CommandResult km_bind_err(const char *s, enum MenuType mtype, int op,
                                      char *macro, char *desc, struct Buffer *err)
{
  enum CommandResult rc = MUTT_CMD_SUCCESS;
  struct Keymap *last = NULL, *np = NULL, *compare = NULL;
  keycode_t buf[MAX_SEQ];
  size_t pos = 0, lastpos = 0;

  size_t len = parsekeys(s, buf, MAX_SEQ);

  struct Keymap *map = alloc_keys(len, buf);
  map->op = op;
  map->macro = mutt_str_dup(macro);
  map->desc = mutt_str_dup(desc);

  /* find position to place new keymap */
  STAILQ_FOREACH(np, &Keymaps[mtype], entries)
  {
    compare = km_compare_keys(map, np, &pos);

    if (compare == map) /* map's keycode is bigger */
    {
      last = np;
      lastpos = pos;
      if (pos > np->eq)
        pos = np->eq;
    }
    else if (compare == np) /* np's keycode is bigger, found insert location */
    {
      map->eq = pos;
      break;
    }
    else /* equal keycodes */
    {
      /* Don't warn on overwriting a 'noop' binding */
      if ((np->len != len) && (np->op != OP_NULL))
      {
        static const char *guide_link = "https://neomutt.org/guide/configuration.html#bind-warnings";
        /* Overwrite with the different lengths, warn */
        char old_binding[128] = { 0 };
        char new_binding[128] = { 0 };
        km_expand_key(old_binding, sizeof(old_binding), map);
        km_expand_key(new_binding, sizeof(new_binding), np);
        char *err_msg = _("Binding '%s' will alias '%s'  Before, try: 'bind %s %s noop'");
        if (err)
        {
          /* err was passed, put the string there */
          buf_printf(err, err_msg, old_binding, new_binding,
                     mutt_map_get_name(mtype, MenuNames), new_binding);
          buf_add_printf(err, "  %s", guide_link);
        }
        else
        {
          struct Buffer *tmp = buf_pool_get();
          buf_printf(tmp, err_msg, old_binding, new_binding,
                     mutt_map_get_name(mtype, MenuNames), new_binding);
          buf_add_printf(tmp, "  %s", guide_link);
          mutt_error("%s", buf_string(tmp));
          buf_pool_release(&tmp);
        }
        rc = MUTT_CMD_WARNING;
      }

      map->eq = np->eq;
      STAILQ_REMOVE(&Keymaps[mtype], np, Keymap, entries);
      mutt_keymap_free(&np);
      break;
    }
  }

  if (map->op == OP_NULL)
  {
    mutt_keymap_free(&map);
  }
  else
  {
    if (last) /* if queue has at least one entry */
    {
      if (STAILQ_NEXT(last, entries))
        STAILQ_INSERT_AFTER(&Keymaps[mtype], last, map, entries);
      else /* last entry in the queue */
        STAILQ_INSERT_TAIL(&Keymaps[mtype], map, entries);
      last->eq = lastpos;
    }
    else /* queue is empty, so insert from head */
    {
      STAILQ_INSERT_HEAD(&Keymaps[mtype], map, entries);
    }
  }

  return rc;
}

/**
 * km_bind - Bind a key to a macro
 * @param s     Key string
 * @param mtype Menu type, e.g. #MENU_EDITOR
 * @param op    Operation, e.g. OP_DELETE
 * @param macro Macro string
 * @param desc Description of macro (OPTIONAL)
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
enum CommandResult km_bind(char *s, enum MenuType mtype, int op, char *macro, char *desc)
{
  return km_bind_err(s, mtype, op, macro, desc, NULL);
}

/**
 * km_bindkey_err - Bind a key in a Menu to an operation (with error message)
 * @param s     Key string
 * @param mtype Menu type, e.g. #MENU_PAGER
 * @param op    Operation, e.g. OP_DELETE
 * @param err   Buffer for error message
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
static enum CommandResult km_bindkey_err(const char *s, enum MenuType mtype,
                                         int op, struct Buffer *err)
{
  return km_bind_err(s, mtype, op, NULL, NULL, err);
}

/**
 * km_bindkey - Bind a key in a Menu to an operation
 * @param s     Key string
 * @param mtype Menu type, e.g. #MENU_PAGER
 * @param op    Operation, e.g. OP_DELETE
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
enum CommandResult km_bindkey(const char *s, enum MenuType mtype, int op)
{
  return km_bindkey_err(s, mtype, op, NULL);
}

/**
 * km_unbind_all - Free all the keys in the supplied Keymap
 * @param km_list Keymap mapping
 * @param mode    Undo bind or macro, e.g. #MUTT_UNBIND, #MUTT_UNMACRO
 *
 * Iterate through Keymap and free keys defined either by "macro" or "bind".
 */
static void km_unbind_all(struct KeymapList *km_list, unsigned long mode)
{
  struct Keymap *np = NULL, *tmp = NULL;

  STAILQ_FOREACH_SAFE(np, km_list, entries, tmp)
  {
    if (((mode & MUTT_UNBIND) && !np->macro) || ((mode & MUTT_UNMACRO) && np->macro))
    {
      STAILQ_REMOVE(km_list, np, Keymap, entries);
      mutt_keymap_free(&np);
    }
  }
}

/**
 * parse_keymap - Parse a user-config key binding
 * @param mtypes    Array for results
 * @param s         Buffer containing config string
 * @param max_menus Total number of menus
 * @param num_menus Number of menus this config applies to
 * @param err       Buffer for error messages
 * @param bind      If true 'bind', otherwise 'macro'
 * @retval ptr Key string for the binding
 *
 * Expects to see: <menu-string>,<menu-string>,... <key-string>
 *
 * @note Caller needs to free the returned string
 */
static char *parse_keymap(enum MenuType *mtypes, struct Buffer *s, int max_menus,
                          int *num_menus, struct Buffer *err, bool bind)
{
  struct Buffer buf;
  int i = 0;
  char *q = NULL;

  buf_init(&buf);

  /* menu name */
  parse_extract_token(&buf, s, TOKEN_NO_FLAGS);
  char *p = buf.data;
  if (MoreArgs(s))
  {
    while (i < max_menus)
    {
      q = strchr(p, ',');
      if (q)
        *q = '\0';

      int val = mutt_map_get_value(p, MenuNames);
      if (val == -1)
      {
        buf_printf(err, _("%s: no such menu"), p);
        goto error;
      }
      mtypes[i] = val;
      i++;
      if (q)
        p = q + 1;
      else
        break;
    }
    *num_menus = i;
    /* key sequence */
    parse_extract_token(&buf, s, TOKEN_NO_FLAGS);

    if (buf.data[0] == '\0')
    {
      buf_printf(err, _("%s: null key sequence"), bind ? "bind" : "macro");
    }
    else if (MoreArgs(s))
    {
      return buf.data;
    }
  }
  else
  {
    buf_printf(err, _("%s: too few arguments"), bind ? "bind" : "macro");
  }
error:
  FREE(&buf.data);
  return NULL;
}

/**
 * parse_menu - Parse menu-names into an array
 * @param menus    Array for results
 * @param s        String containing menu-names
 * @param err      Buffer for error messages
 * @retval NULL Always
 *
 * Expects to see: <menu-string>[,<menu-string>]
 */
static void *parse_menu(bool *menus, char *s, struct Buffer *err)
{
  char *menu_names_dup = mutt_str_dup(s);
  char *marker = menu_names_dup;
  char *menu_name = NULL;

  while ((menu_name = mutt_str_sep(&marker, ",")))
  {
    int value = mutt_map_get_value(menu_name, MenuNames);
    if (value == -1)
    {
      buf_printf(err, _("%s: no such menu"), menu_name);
      break;
    }
    else
    {
      menus[value] = true;
    }
  }

  FREE(&menu_names_dup);
  return NULL;
}

/**
 * try_bind - Try to make a key binding
 * @param key   Key name
 * @param mtype Menu type, e.g. #MENU_PAGER
 * @param func  Function name
 * @param funcs Functions table
 * @param err   Buffer for error message
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
static enum CommandResult try_bind(char *key, enum MenuType mtype, char *func,
                                   const struct MenuFuncOp *funcs, struct Buffer *err)
{
  for (int i = 0; funcs[i].name; i++)
  {
    if (mutt_str_equal(func, funcs[i].name))
    {
      return km_bindkey_err(key, mtype, funcs[i].op, err);
    }
  }
  if (err)
  {
    buf_printf(err, _("Function '%s' not available for menu '%s'"), func,
               mutt_map_get_name(mtype, MenuNames));
  }
  return MUTT_CMD_ERROR; /* Couldn't find an existing function with this name */
}

/**
 * mutt_parse_push - Parse the 'push' command - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult mutt_parse_push(struct Buffer *buf, struct Buffer *s,
                                   intptr_t data, struct Buffer *err)
{
  parse_extract_token(buf, s, TOKEN_CONDENSE);
  if (MoreArgs(s))
  {
    buf_printf(err, _("%s: too many arguments"), "push");
    return MUTT_CMD_ERROR;
  }

  generic_tokenize_push_string(buf->data, mutt_push_macro_event);
  return MUTT_CMD_SUCCESS;
}

/**
 * mutt_parse_bind - Parse the 'bind' command - Implements Command::parse() - @ingroup command_parse
 *
 * bind menu-name `<key_sequence>` function-name
 */
enum CommandResult mutt_parse_bind(struct Buffer *buf, struct Buffer *s,
                                   intptr_t data, struct Buffer *err)
{
  if (StartupComplete)
  {
    // Save and restore the offset in `s` because dump_bind_macro() might change it
    char *dptr = s->dptr;
    if (dump_bind_macro(buf, s, data, err) == MUTT_CMD_SUCCESS)
      return MUTT_CMD_SUCCESS;
    if (!buf_is_empty(err))
      return MUTT_CMD_ERROR;
    s->dptr = dptr;
  }

  const struct MenuFuncOp *funcs = NULL;
  enum MenuType mtypes[MenuNamesLen];
  int num_menus = 0;
  enum CommandResult rc = MUTT_CMD_SUCCESS;

  char *key = parse_keymap(mtypes, s, mutt_array_size(mtypes), &num_menus, err, true);
  if (!key)
    return MUTT_CMD_ERROR;

  /* function to execute */
  parse_extract_token(buf, s, TOKEN_NO_FLAGS);
  if (MoreArgs(s))
  {
    buf_printf(err, _("%s: too many arguments"), "bind");
    rc = MUTT_CMD_ERROR;
  }
  else if (mutt_istr_equal("noop", buf->data))
  {
    for (int i = 0; i < num_menus; i++)
    {
      km_bindkey(key, mtypes[i], OP_NULL); /* the 'unbind' command */
      funcs = km_get_table(mtypes[i]);
      if (funcs)
      {
        char keystr[32] = { 0 };
        km_expand_key_string(key, keystr, sizeof(keystr));
        const char *mname = mutt_map_get_name(mtypes[i], MenuNames);
        mutt_debug(LL_NOTIFY, "NT_BINDING_DELETE: %s %s\n", mname, keystr);

        int op = get_op(OpGeneric, buf->data, mutt_str_len(buf->data));
        struct EventBinding ev_b = { mtypes[i], key, op };
        notify_send(NeoMutt->notify, NT_BINDING, NT_BINDING_DELETE, &ev_b);
      }
    }
  }
  else
  {
    for (int i = 0; i < num_menus; i++)
    {
      /* The pager and editor menus don't use the generic map,
       * however for other menus try generic first. */
      if ((mtypes[i] != MENU_PAGER) && (mtypes[i] != MENU_EDITOR) && (mtypes[i] != MENU_GENERIC))
      {
        rc = try_bind(key, mtypes[i], buf->data, OpGeneric, err);
        if (rc == MUTT_CMD_SUCCESS)
        {
          char keystr[32] = { 0 };
          km_expand_key_string(key, keystr, sizeof(keystr));
          const char *mname = mutt_map_get_name(mtypes[i], MenuNames);
          mutt_debug(LL_NOTIFY, "NT_BINDING_NEW: %s %s\n", mname, keystr);

          int op = get_op(OpGeneric, buf->data, mutt_str_len(buf->data));
          struct EventBinding ev_b = { mtypes[i], key, op };
          notify_send(NeoMutt->notify, NT_BINDING, NT_BINDING_ADD, &ev_b);
          continue;
        }
        if (rc == MUTT_CMD_WARNING)
          break;
      }

      /* Clear any error message, we're going to try again */
      err->data[0] = '\0';
      funcs = km_get_table(mtypes[i]);
      if (funcs)
      {
        rc = try_bind(key, mtypes[i], buf->data, funcs, err);
        if (rc == MUTT_CMD_SUCCESS)
        {
          char keystr[32] = { 0 };
          km_expand_key_string(key, keystr, sizeof(keystr));
          const char *mname = mutt_map_get_name(mtypes[i], MenuNames);
          mutt_debug(LL_NOTIFY, "NT_BINDING_NEW: %s %s\n", mname, keystr);

          int op = get_op(funcs, buf->data, mutt_str_len(buf->data));
          struct EventBinding ev_b = { mtypes[i], key, op };
          notify_send(NeoMutt->notify, NT_BINDING, NT_BINDING_ADD, &ev_b);
          continue;
        }
      }
    }
  }
  FREE(&key);
  return rc;
}

/**
 * mutt_parse_unbind - Parse the 'unbind' command - Implements Command::parse() - @ingroup command_parse
 *
 * Command unbinds:
 * - one binding in one menu-name
 * - one binding in all menu-names
 * - all bindings in all menu-names
 *
 * unbind `<menu-name[,...]|*>` [`<key_sequence>`]
 */
enum CommandResult mutt_parse_unbind(struct Buffer *buf, struct Buffer *s,
                                     intptr_t data, struct Buffer *err)
{
  bool menu_matches[MENU_MAX] = { 0 };
  bool all_keys = false;
  char *key = NULL;

  parse_extract_token(buf, s, TOKEN_NO_FLAGS);
  if (mutt_str_equal(buf->data, "*"))
  {
    for (enum MenuType i = 1; i < MENU_MAX; i++)
      menu_matches[i] = true;
  }
  else
  {
    parse_menu(menu_matches, buf->data, err);
  }

  if (MoreArgs(s))
  {
    parse_extract_token(buf, s, TOKEN_NO_FLAGS);
    key = buf->data;
  }
  else
  {
    all_keys = true;
  }

  if (MoreArgs(s))
  {
    const char *cmd = (data & MUTT_UNMACRO) ? "unmacro" : "unbind";

    buf_printf(err, _("%s: too many arguments"), cmd);
    return MUTT_CMD_ERROR;
  }

  for (enum MenuType i = 1; i < MENU_MAX; i++)
  {
    if (!menu_matches[i])
      continue;
    if (all_keys)
    {
      km_unbind_all(&Keymaps[i], data);
      km_bindkey("<enter>", MENU_GENERIC, OP_GENERIC_SELECT_ENTRY);
      km_bindkey("<return>", MENU_GENERIC, OP_GENERIC_SELECT_ENTRY);
      km_bindkey("<enter>", MENU_INDEX, OP_DISPLAY_MESSAGE);
      km_bindkey("<return>", MENU_INDEX, OP_DISPLAY_MESSAGE);
      km_bindkey("<backspace>", MENU_EDITOR, OP_EDITOR_BACKSPACE);
      km_bindkey("\177", MENU_EDITOR, OP_EDITOR_BACKSPACE);
      km_bindkey(":", MENU_GENERIC, OP_ENTER_COMMAND);
      km_bindkey(":", MENU_PAGER, OP_ENTER_COMMAND);
      if (i != MENU_EDITOR)
      {
        km_bindkey("?", i, OP_HELP);
        km_bindkey("q", i, OP_EXIT);
      }

      const char *mname = mutt_map_get_name(i, MenuNames);
      mutt_debug(LL_NOTIFY, "NT_MACRO_DELETE_ALL: %s\n", mname);

      struct EventBinding ev_b = { i, NULL, OP_NULL };
      notify_send(NeoMutt->notify, NT_BINDING,
                  (data & MUTT_UNMACRO) ? NT_MACRO_DELETE_ALL : NT_BINDING_DELETE_ALL,
                  &ev_b);
    }
    else
    {
      char keystr[32] = { 0 };
      km_expand_key_string(key, keystr, sizeof(keystr));
      const char *mname = mutt_map_get_name(i, MenuNames);
      mutt_debug(LL_NOTIFY, "NT_MACRO_DELETE: %s %s\n", mname, keystr);

      km_bindkey(key, i, OP_NULL);
      struct EventBinding ev_b = { i, key, OP_NULL };
      notify_send(NeoMutt->notify, NT_BINDING,
                  (data & MUTT_UNMACRO) ? NT_MACRO_DELETE : NT_BINDING_DELETE, &ev_b);
    }
  }

  return MUTT_CMD_SUCCESS;
}

/**
 * mutt_parse_macro - Parse the 'macro' command - Implements Command::parse() - @ingroup command_parse
 *
 * macro `<menu>` `<key>` `<macro>` `<description>`
 */
enum CommandResult mutt_parse_macro(struct Buffer *buf, struct Buffer *s,
                                    intptr_t data, struct Buffer *err)
{
  if (StartupComplete)
  {
    // Save and restore the offset in `s` because dump_bind_macro() might change it
    char *dptr = s->dptr;
    if (dump_bind_macro(buf, s, data, err) == MUTT_CMD_SUCCESS)
      return MUTT_CMD_SUCCESS;
    if (!buf_is_empty(err))
      return MUTT_CMD_ERROR;
    s->dptr = dptr;
  }

  enum MenuType mtypes[MenuNamesLen];
  int num_menus = 0;
  enum CommandResult rc = MUTT_CMD_ERROR;

  char *key = parse_keymap(mtypes, s, mutt_array_size(mtypes), &num_menus, err, false);
  if (!key)
    return MUTT_CMD_ERROR;

  parse_extract_token(buf, s, TOKEN_CONDENSE);
  /* make sure the macro sequence is not an empty string */
  if (buf->data[0] == '\0')
  {
    buf_strcpy(err, _("macro: empty key sequence"));
  }
  else
  {
    if (MoreArgs(s))
    {
      char *seq = mutt_str_dup(buf->data);
      parse_extract_token(buf, s, TOKEN_CONDENSE);

      if (MoreArgs(s))
      {
        buf_printf(err, _("%s: too many arguments"), "macro");
      }
      else
      {
        for (int i = 0; i < num_menus; i++)
        {
          rc = km_bind(key, mtypes[i], OP_MACRO, seq, buf->data);
          if (rc == MUTT_CMD_SUCCESS)
          {
            char keystr[32] = { 0 };
            km_expand_key_string(key, keystr, sizeof(keystr));
            const char *mname = mutt_map_get_name(mtypes[i], MenuNames);
            mutt_debug(LL_NOTIFY, "NT_MACRO_NEW: %s %s\n", mname, keystr);

            struct EventBinding ev_b = { mtypes[i], key, OP_MACRO };
            notify_send(NeoMutt->notify, NT_BINDING, NT_MACRO_ADD, &ev_b);
            continue;
          }
        }
      }

      FREE(&seq);
    }
    else
    {
      for (int i = 0; i < num_menus; i++)
      {
        rc = km_bind(key, mtypes[i], OP_MACRO, buf->data, NULL);
        if (rc == MUTT_CMD_SUCCESS)
        {
          char keystr[32] = { 0 };
          km_expand_key_string(key, keystr, sizeof(keystr));
          const char *mname = mutt_map_get_name(mtypes[i], MenuNames);
          mutt_debug(LL_NOTIFY, "NT_MACRO_NEW: %s %s\n", mname, keystr);

          struct EventBinding ev_b = { mtypes[i], key, OP_MACRO };
          notify_send(NeoMutt->notify, NT_BINDING, NT_MACRO_ADD, &ev_b);
          continue;
        }
      }
    }
  }
  FREE(&key);
  return rc;
}

/**
 * mutt_parse_exec - Parse the 'exec' command - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult mutt_parse_exec(struct Buffer *buf, struct Buffer *s,
                                   intptr_t data, struct Buffer *err)
{
  int ops[128];
  int nops = 0;
  const struct MenuFuncOp *funcs = NULL;
  char *function = NULL;

  if (!MoreArgs(s))
  {
    buf_strcpy(err, _("exec: no arguments"));
    return MUTT_CMD_ERROR;
  }

  do
  {
    parse_extract_token(buf, s, TOKEN_NO_FLAGS);
    function = buf->data;

    const enum MenuType mtype = menu_get_current_type();
    funcs = km_get_table(mtype);
    if (!funcs && (mtype != MENU_PAGER))
      funcs = OpGeneric;

    ops[nops] = get_op(funcs, function, mutt_str_len(function));
    if ((ops[nops] == OP_NULL) && (mtype != MENU_PAGER) && (mtype != MENU_GENERIC))
    {
      ops[nops] = get_op(OpGeneric, function, mutt_str_len(function));
    }

    if (ops[nops] == OP_NULL)
    {
      mutt_flushinp();
      mutt_error(_("%s: no such function"), function);
      return MUTT_CMD_ERROR;
    }
    nops++;
  } while (MoreArgs(s) && nops < mutt_array_size(ops));

  while (nops)
    mutt_push_macro_event(0, ops[--nops]);

  return MUTT_CMD_SUCCESS;
}
