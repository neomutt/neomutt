/**
 * @file
 * Parse key binding commands
 *
 * @authors
 * Copyright (C) 2025-2026 Richard Russon <rich@flatcap.org>
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
 * @page key_commands Parse key binding commands
 *
 * Parse key binding commands
 */

#include "config.h"
#include <stdbool.h>
#include <string.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "commands.h"
#include "menu/lib.h"
#include "parse/lib.h"
#include "dump.h"
#include "get.h"
#include "init.h"
#include "keymap.h"
#include "menu.h"
#include "notify.h"

/**
 * struct ParseUnbind - Parsed 'unbind' or 'unmacro' command
 */
struct ParseUnbind
{
  bool menus[MENU_MAX]; ///< Menus to work on
  bool all_menus;       ///< Command affects all Menus
  bool all_keys;        ///< Command affects all key bindings
  const char *key;      ///< Key string to be removed
};

/**
 * parse_dump - Parse 'bind' and 'macro' commands - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult parse_dump(const struct Command *cmd, struct Buffer *line,
                              struct Buffer *err)
{
  struct Buffer *token = buf_pool_get();
  int mtype = MENU_MAX;
  enum CommandResult rc = MUTT_CMD_ERROR;

  if (MoreArgs(line))
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);

    if (MoreArgs(line))
    {
      /* More arguments potentially means the user is using the
       * ::command_t :bind command thus we delegate the task. */
      goto done;
    }

    if (!mutt_str_equal(buf_string(token), "all"))
    {
      mtype = km_get_menu_id(buf_string(token));
      if (mtype == -1)
      {
        // L10N: '%s' is the (misspelled) name of the menu, e.g. 'index' or 'pager'
        buf_printf(err, _("%s: no such menu"), buf_string(token));
        goto done;
      }
    }
  }

  dump_bind_macro(cmd, mtype, token, err);
  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  return rc;
}

/**
 * parse_keymap - Parse a user-config key binding
 * @param cmd       Command being processed
 * @param mtypes    Array for results
 * @param line      Buffer containing config string
 * @param max_menus Total number of menus
 * @param num_menus Number of menus this config applies to
 * @param err       Buffer for error messages
 * @retval ptr Key string for the binding
 *
 * Expects to see: <menu-string>,<menu-string>,... <key-string>
 *
 * @note Caller needs to free the returned string
 */
char *parse_keymap(const struct Command *cmd, enum MenuType *mtypes, struct Buffer *line,
                   int max_menus, int *num_menus, struct Buffer *err)
{
  struct Buffer *token = buf_pool_get();
  int i = 0;
  char *q = NULL;
  char *result = NULL;

  /* menu name */
  parse_extract_token(token, line, TOKEN_NO_FLAGS);
  char *p = token->data;
  if (MoreArgs(line))
  {
    while (i < max_menus)
    {
      q = strchr(p, ',');
      if (q)
        *q = '\0';

      int val = km_get_menu_id(p);
      if (val == -1)
      {
        buf_printf(err, _("%s: no such menu"), p);
        goto done;
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
    parse_extract_token(token, line, TOKEN_NO_FLAGS);

    if (buf_at(token, 0) == '\0')
    {
      buf_printf(err, _("%s: null key sequence"), cmd->name);
    }
    else if (MoreArgs(line))
    {
      result = buf_strdup(token);
      goto done;
    }
  }
  else
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
  }
done:
  buf_pool_release(&token);
  return result;
}

/**
 * parse_menu - Parse menu-names into an array
 * @param menus    Array for results
 * @param s        String containing menu-names
 * @param err      Buffer for error messages
 *
 * Expects to see: <menu-string>[,<menu-string>]
 */
void parse_menu(bool *menus, const char *s, struct Buffer *err)
{
  char *menu_names_dup = mutt_str_dup(s);
  char *marker = menu_names_dup;
  char *menu_name = NULL;

  while ((menu_name = mutt_str_sep(&marker, ",")))
  {
    int value = km_get_menu_id(menu_name);
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
}

/**
 * parse_push - Parse the 'push' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `push <string>`
 */
enum CommandResult parse_push(const struct Command *cmd, struct Buffer *line,
                              struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_WARNING;

  parse_extract_token(token, line, TOKEN_CONDENSE);
  if (MoreArgs(line))
  {
    buf_printf(err, _("%s: too many arguments"), cmd->name);
    goto done;
  }

  generic_tokenize_push_string(token->data);
  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  return rc;
}

/**
 * parse_bind - Parse the 'bind' command - Implements Command::parse() - @ingroup command_parse
 *
 * bind menu-name `<key_sequence>` function-name
 *
 * Parse:
 * - `bind <map>[,<map> ... ] <key> <function>`
 */
enum CommandResult parse_bind(const struct Command *cmd, struct Buffer *line,
                              struct Buffer *err)
{
  if (StartupComplete)
  {
    // Save and restore the offset in `line` because parse_dump() might change it
    char *dptr = line->dptr;
    if (parse_dump(cmd, line, err) == MUTT_CMD_SUCCESS)
    {
      return MUTT_CMD_SUCCESS;
    }
    if (!buf_is_empty(err))
    {
      return MUTT_CMD_ERROR;
    }
    line->dptr = dptr;
  }

  struct Buffer *token = buf_pool_get();
  struct Buffer *keystr = NULL;

  enum MenuType mtypes[MENU_MAX];
  int num_menus = 0;
  enum CommandResult rc = MUTT_CMD_ERROR;

  char *key = parse_keymap(cmd, mtypes, line, countof(mtypes), &num_menus, err);
  if (!key)
    goto done;

  /* function to execute */
  parse_extract_token(token, line, TOKEN_NO_FLAGS);
  if (MoreArgs(line))
  {
    buf_printf(err, _("%s: too many arguments"), cmd->name);
    goto done;
  }

  rc = MUTT_CMD_SUCCESS;

  if (mutt_istr_equal("noop", buf_string(token)))
  {
    keystr = buf_pool_get();
    for (int i = 0; i < num_menus; i++)
    {
      struct MenuDefinition *md = NULL;
      ARRAY_FOREACH(md, &MenuDefs)
      {
        if (md->id == mtypes[i])
          break;
      }

      km_bind(md, key, OP_NULL, NULL, NULL, NULL); /* the 'unbind' command */

      buf_reset(keystr);
      keymap_expand_string(key, keystr);
      const char *mname = km_get_menu_name(mtypes[i]);
      mutt_debug(LL_NOTIFY, "NT_BINDING_DELETE: %s %s\n", mname, buf_string(keystr));

      int op = km_get_op_menu(mtypes[i], buf_string(token));

      struct EventBinding ev_b = { mtypes[i], key, op };
      notify_send(NeoMutt->notify, NT_BINDING, NT_BINDING_DELETE, &ev_b);
    }
  }
  else
  {
    keystr = buf_pool_get();
    for (int i = 0; i < num_menus; i++)
    {
      struct MenuDefinition *md = NULL;
      ARRAY_FOREACH(md, &MenuDefs)
      {
        if (md->id == mtypes[i])
          break;
      }

      int op = OP_NULL;
      struct SubMenu **ptr = NULL;
      ARRAY_FOREACH(ptr, &md->submenus)
      {
        struct SubMenu *sm = *ptr;
        const struct MenuFuncOp *mfo = NULL;
        for (int j = 0; sm->functions[j].name; j++)
        {
          mfo = &sm->functions[j];
          if (mutt_str_equal(buf_string(token), mfo->name))
          {
            op = mfo->op;
            break;
          }
        }
      }

      rc = km_bind(md, key, op, NULL, NULL, err);
      if (rc == MUTT_CMD_SUCCESS)
      {
        buf_reset(keystr);
        keymap_expand_string(key, keystr);
        const char *mname = km_get_menu_name(mtypes[i]);
        mutt_debug(LL_NOTIFY, "NT_BINDING_NEW: %s %s\n", mname, buf_string(keystr));

        struct EventBinding ev_b = { mtypes[i], key, op };
        notify_send(NeoMutt->notify, NT_BINDING, NT_BINDING_ADD, &ev_b);
      }
    }
  }

done:
  FREE(&key);
  buf_pool_release(&keystr);
  buf_pool_release(&token);
  return rc;
}

/**
 * set_default_bindings - Set some safe default keybindings
 * @param menu Menu to update
 */
void set_default_bindings(enum MenuType menu)
{
  struct MenuDefinition *md = NULL;
  bool success = false;

  if ((menu == MENU_MAX) || (menu == MENU_GENERIC))
  {
    md = menu_find(MENU_GENERIC);
    km_bind(md, "<enter>", OP_GENERIC_SELECT_ENTRY, NULL, NULL, NULL);
    km_bind(md, "<return>", OP_GENERIC_SELECT_ENTRY, NULL, NULL, NULL);
    km_bind(md, ":", OP_ENTER_COMMAND, NULL, NULL, NULL);
    km_bind(md, "?", OP_HELP, NULL, NULL, NULL);
    km_bind(md, "q", OP_EXIT, NULL, NULL, NULL);
    success = true;
  }

  if ((menu == MENU_MAX) || (menu == MENU_INDEX))
  {
    md = menu_find(MENU_INDEX);
    km_bind(md, "<enter>", OP_DISPLAY_MESSAGE, NULL, NULL, NULL);
    km_bind(md, "<return>", OP_DISPLAY_MESSAGE, NULL, NULL, NULL);
    km_bind(md, "q", OP_EXIT, NULL, NULL, NULL);
    success = true;
  }

  if ((menu == MENU_MAX) || (menu == MENU_EDITOR))
  {
    md = menu_find(MENU_EDITOR);
    km_bind(md, "<backspace>", OP_EDITOR_BACKSPACE, NULL, NULL, NULL);
    km_bind(md, "\177", OP_EDITOR_BACKSPACE, NULL, NULL, NULL);
    success = true;
  }

  if ((menu == MENU_MAX) || (menu == MENU_PAGER))
  {
    md = menu_find(MENU_PAGER);
    km_bind(md, ":", OP_ENTER_COMMAND, NULL, NULL, NULL);
    km_bind(md, "?", OP_HELP, NULL, NULL, NULL);
    km_bind(md, "q", OP_EXIT, NULL, NULL, NULL);
    success = true;
  }

  if ((menu == MENU_MAX) || (menu == MENU_DIALOG))
  {
    md = menu_find(MENU_DIALOG);
    km_bind(md, ":", OP_ENTER_COMMAND, NULL, NULL, NULL);
    km_bind(md, "?", OP_HELP, NULL, NULL, NULL);
    km_bind(md, "q", OP_EXIT, NULL, NULL, NULL);
    success = true;
  }

  if (success)
  {
    mutt_debug(LL_NOTIFY, "NT_BINDING_ADD set defaults\n");
    struct EventBinding ev_b = { menu, NULL, OP_NULL };
    notify_send(NeoMutt->notify, NT_BINDING, NT_BINDING_ADD, &ev_b);
  }
}

/**
 * notify_binding_name - Get the name for a NotifyBinding
 * @param ev Event type
 * @retval str Name of the NotifyBinding
 */
const char *notify_binding_name(enum NotifyBinding ev)
{
  switch (ev)
  {
    case NT_BINDING_ADD:
      return "NT_BINDING_ADD";
    case NT_BINDING_DELETE:
      return "NT_BINDING_DELETE";
    case NT_MACRO_ADD:
      return "NT_MACRO_ADD";
    case NT_MACRO_DELETE:
      return "NT_MACRO_DELETE";
    default:
      return "UNKNOWN";
  }
}

/**
 * parse_unbind_args - Parse the 'unbind' and 'unmacro' commands
 * @param[in]  cmd   Command being parsed
 * @param[in]  line  Text to parse
 * @param[out] err   Buffer for error messages
 * @param[out] args  Parsed args
 * @retval true Success
 *
 * Command unbinds:
 * - all bindings in all menu-names
 *   - `unbind *`
 * - all bindings in one/many menu-names
 *   - `unbind index *`
 *   - `unbind index`
 *   - `unbind index,pager *`
 *   - `unbind index,pager`
 * - one binding in one/many/all menu-names
 *   - `unbind index       j`
 *   - `unbind index,pager j`
 *   - `unbind *           j`
 *
 * The same applies to `unmacro`.
 */
bool parse_unbind_args(const struct Command *cmd, struct Buffer *line,
                       struct Buffer *err, struct ParseUnbind *args)
{
  if (!cmd || !line || !err || !args)
    return false;

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return false;
  }

  struct Buffer *token = buf_pool_get();
  bool rc = false;

  parse_extract_token(token, line, TOKEN_NO_FLAGS);
  if (mutt_str_equal(buf_string(token), "*"))
  {
    args->all_menus = true;

    if (MoreArgs(line))
    {
      // `unbind * key`
      // `unmacro * key`
      parse_extract_token(token, line, TOKEN_NO_FLAGS);
      args->key = buf_strdup(token);
    }
    else
    {
      // `unbind *`
      // `unmacro *`
      args->all_keys = true;
    }
  }
  else
  {
    parse_menu(args->menus, buf_string(token), err);
    if (!buf_is_empty(err))
      goto done;

    if (MoreArgs(line))
    {
      parse_extract_token(token, line, TOKEN_NO_FLAGS);
      if (mutt_str_equal(buf_string(token), "*"))
      {
        // `unbind  menu *`
        // `unmacro menu *`
        args->all_keys = true;
      }
      else
      {
        // `unbind  menu key`
        // `unmacro menu key`
        args->key = buf_strdup(token);
      }
    }
    else
    {
      // `unbind  menu`
      // `unmacro menu`
      args->all_keys = true;
    }
  }

  if (MoreArgs(line))
  {
    buf_printf(err, _("%s: too many arguments"), cmd->name);
    goto done;
  }

  rc = true;

done:
  buf_pool_release(&token);
  return rc;
}

/**
 * parse_unbind_execute - Execute the 'unbind' or 'unmacro' command
 * @param[in]  cmd  Command being executed
 * @param[in]  args Parsed arguments
 * @param[out] err  Buffer for error messages
 * @retval true Success
 */
bool parse_unbind_execute(const struct Command *cmd, struct ParseUnbind *args,
                          struct Buffer *err)
{
  if (!cmd || !args || !err)
    return false;

  keycode_t key_bytes[MAX_SEQ] = { 0 };
  int key_len = 0;
  if (args->key)
  {
    key_len = parse_keys(args->key, key_bytes, MAX_SEQ);
    if (key_len == 0)
      return false;
  }

  bool success = false;
  struct Buffer *keystr = buf_pool_get();
  enum NotifyBinding nb = (cmd->id == CMD_UNBIND) ? NT_BINDING_DELETE : NT_MACRO_DELETE;
  struct MenuDefinition *md = NULL;
  ARRAY_FOREACH(md, &MenuDefs)
  {
    if (!args->all_menus && !args->menus[md->id])
      continue;

    struct SubMenu **smp = ARRAY_GET(&md->submenus, 0);
    if (!smp || !*smp)
      continue;

    struct SubMenu *sm = *smp;

    struct Keymap *km = NULL;
    struct Keymap *km_tmp = NULL;
    STAILQ_FOREACH_SAFE(km, &sm->keymaps, entries, km_tmp)
    {
      const bool is_macro = (km->op == OP_MACRO);
      const bool want_macro = (cmd->id == CMD_UNMACRO);

      // Is this the type we're looking for?
      if (is_macro ^ want_macro)
        continue;

      if (args->all_keys ||
          ((key_len == km->len) && (memcmp(km->keys, key_bytes, km->len) == 0)))
      {
        STAILQ_REMOVE(&sm->keymaps, km, Keymap, entries);
        keymap_free(&km);
        success = true;

        if (!args->all_keys && !args->all_menus)
        {
          buf_reset(keystr);
          keymap_expand_string(args->key, keystr);
          mutt_debug(LL_NOTIFY, "%s: %s %s\n", notify_binding_name(nb),
                     md->name, buf_string(keystr));

          struct EventBinding ev_b = { md->id, args->key, OP_NULL };
          notify_send(NeoMutt->notify, NT_BINDING, nb, &ev_b);
        }
      }
    }

    if (args->all_keys && !args->all_menus && success)
    {
      buf_reset(keystr);
      keymap_expand_string(args->key, keystr);
      mutt_debug(LL_NOTIFY, "%s: %s %s\n", notify_binding_name(nb),
                 md->name, buf_string(keystr));

      struct EventBinding ev_b = { md->id, args->key, OP_NULL };
      notify_send(NeoMutt->notify, NT_BINDING, nb, &ev_b);

      // restore some bindings for this menu
      set_default_bindings(md->id);
    }
  }

  if (args->all_menus && success)
  {
    buf_reset(keystr);
    keymap_expand_string(args->key, keystr);
    mutt_debug(LL_NOTIFY, "%s: ALL %s\n", notify_binding_name(nb),
               buf_string(keystr));

    struct EventBinding ev_b = { MENU_MAX, args->key, OP_NULL };
    notify_send(NeoMutt->notify, NT_BINDING, nb, &ev_b);

    // restore some bindings for all menus
    set_default_bindings(MENU_MAX);
  }

  buf_pool_release(&keystr);
  return true;
}

/**
 * parse_unbind - Parse the 'unbind' and 'unmacro' commands - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `unbind { * | <menu>[,<menu> ... ] } [ <key> ]`
 * - `unmacro { * | <menu>[,<menu> ... ] } [ <key> ]`
 */
enum CommandResult parse_unbind(const struct Command *cmd, struct Buffer *line,
                                struct Buffer *err)
{
  struct ParseUnbind args = { 0 };
  enum CommandResult rc = MUTT_CMD_WARNING;

  if (!parse_unbind_args(cmd, line, err, &args))
    goto done;

  rc = MUTT_CMD_ERROR;
  if (parse_unbind_execute(cmd, &args, err))
    rc = MUTT_CMD_SUCCESS;

done:
  FREE(&args.key);
  return rc;
}

/**
 * parse_macro - Parse the 'macro' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `macro <menu>[,<menu> ... ] <key> <sequence> [ <description> ]`
 */
enum CommandResult parse_macro(const struct Command *cmd, struct Buffer *line,
                               struct Buffer *err)
{
  if (StartupComplete)
  {
    // Save and restore the offset in `line` because parse_dump() might change it
    char *dptr = line->dptr;
    if (parse_dump(cmd, line, err) == MUTT_CMD_SUCCESS)
    {
      return MUTT_CMD_SUCCESS;
    }
    if (!buf_is_empty(err))
    {
      return MUTT_CMD_ERROR;
    }
    line->dptr = dptr;
  }

  enum MenuType mtypes[MENU_MAX];
  int num_menus = 0;
  struct Buffer *token = buf_pool_get();
  struct Buffer *keystr = NULL;
  enum CommandResult rc = MUTT_CMD_ERROR;

  char *key = parse_keymap(cmd, mtypes, line, countof(mtypes), &num_menus, err);
  if (!key)
    goto done;

  parse_extract_token(token, line, TOKEN_CONDENSE);
  /* make sure the macro sequence is not an empty string */
  if (buf_at(token, 0) == '\0')
  {
    buf_strcpy(err, _("macro: empty key sequence"));
  }
  else
  {
    if (MoreArgs(line))
    {
      char *seq = mutt_str_dup(buf_string(token));
      parse_extract_token(token, line, TOKEN_CONDENSE);

      if (MoreArgs(line))
      {
        buf_printf(err, _("%s: too many arguments"), cmd->name);
      }
      else
      {
        keystr = buf_pool_get();
        for (int i = 0; i < num_menus; i++)
        {
          struct MenuDefinition *md = menu_find(mtypes[i]);
          rc = km_bind(md, key, OP_MACRO, seq, token->data, NULL);
          if (rc == MUTT_CMD_SUCCESS)
          {
            buf_reset(keystr);
            keymap_expand_string(key, keystr);
            const char *mname = km_get_menu_name(mtypes[i]);
            mutt_debug(LL_NOTIFY, "NT_MACRO_NEW: %s %s\n", mname, buf_string(keystr));

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
      keystr = buf_pool_get();
      for (int i = 0; i < num_menus; i++)
      {
        struct MenuDefinition *md = menu_find(mtypes[i]);
        rc = km_bind(md, key, OP_MACRO, token->data, NULL, NULL);
        if (rc == MUTT_CMD_SUCCESS)
        {
          buf_reset(keystr);
          keymap_expand_string(key, keystr);
          const char *mname = km_get_menu_name(mtypes[i]);
          mutt_debug(LL_NOTIFY, "NT_MACRO_NEW: %s %s\n", mname, buf_string(keystr));

          struct EventBinding ev_b = { mtypes[i], key, OP_MACRO };
          notify_send(NeoMutt->notify, NT_BINDING, NT_MACRO_ADD, &ev_b);
          continue;
        }
      }
    }
  }

done:
  FREE(&key);
  buf_pool_release(&keystr);
  buf_pool_release(&token);
  return rc;
}

/**
 * parse_exec - Parse the 'exec' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `exec <function> [ <function> ... ]`
 */
enum CommandResult parse_exec(const struct Command *cmd, struct Buffer *line,
                              struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  int ops[128];
  int nops = 0;
  char *function = NULL;

  do
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);
    function = token->data;

    const enum MenuType mtype = menu_get_current_type();
    ops[nops] = km_get_op_menu(mtype, function);
    if (ops[nops] == OP_NULL)
    {
      mutt_flushinp();
      mutt_error(_("%s: no such function"), function);
      goto done;
    }
    nops++;
  } while (MoreArgs(line) && nops < countof(ops));

  while (nops)
    mutt_push_macro_event(0, ops[--nops]);

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  return rc;
}
