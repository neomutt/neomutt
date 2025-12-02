/**
 * @file
 * Parse key binding commands
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
 * @page key_commands Parse key binding commands
 *
 * Parse key binding commands
 */

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "debug/lib.h"
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
 * parse_dump - Parse 'bind' and 'macro' commands - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult parse_dump(struct Buffer *buf, struct Buffer *s,
                              intptr_t data, struct Buffer *err)
{
  int mtype = MENU_MAX;
  bool bind = (data == 0);

  if (MoreArgs(s))
  {
    parse_extract_token(buf, s, TOKEN_NO_FLAGS);

    if (!mutt_str_equal(buf_string(buf), "all"))
    {
      mtype = km_get_menu_id(buf_string(buf));
      if (mtype == -1)
      {
        // L10N: '%s' is the (misspelled) name of the menu, e.g. 'index' or 'pager'
        buf_printf(err, _("%s: no such menu"), buf_string(buf));
        return MUTT_CMD_ERROR;
      }
    }
  }

  if (MoreArgs(s))
  {
    /* More arguments potentially means the user is using the
     * ::command_t :bind command thus we delegate the task. */
    return MUTT_CMD_ERROR;
  }

  dump_bind_macro(bind, mtype, buf, err);

  return MUTT_CMD_SUCCESS;
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
char *parse_keymap(enum MenuType *mtypes, struct Buffer *s, int max_menus,
                   int *num_menus, struct Buffer *err, bool bind)
{
  struct Buffer *buf = buf_pool_get();
  int i = 0;
  char *q = NULL;
  char *result = NULL;

  /* menu name */
  parse_extract_token(buf, s, TOKEN_NO_FLAGS);
  char *p = buf->data;
  if (MoreArgs(s))
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
    parse_extract_token(buf, s, TOKEN_NO_FLAGS);

    if (buf_at(buf, 0) == '\0')
    {
      buf_printf(err, _("%s: null key sequence"), bind ? "bind" : "macro");
    }
    else if (MoreArgs(s))
    {
      result = buf_strdup(buf);
      goto done;
    }
  }
  else
  {
    buf_printf(err, _("%s: too few arguments"), bind ? "bind" : "macro");
  }
done:
  buf_pool_release(&buf);
  return result;
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
void *parse_menu(bool *menus, const char *s, struct Buffer *err)
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
  return NULL;
}

/**
 * parse_push - Parse the 'push' command - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult parse_push(struct Buffer *buf, struct Buffer *s,
                              intptr_t data, struct Buffer *err)
{
  parse_extract_token(buf, s, TOKEN_CONDENSE);
  if (MoreArgs(s))
  {
    buf_printf(err, _("%s: too many arguments"), "push");
    return MUTT_CMD_ERROR;
  }

  generic_tokenize_push_string(buf->data);
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_bind - Parse the 'bind' command - Implements Command::parse() - @ingroup command_parse
 *
 * bind menu-name `<key_sequence>` function-name
 */
enum CommandResult parse_bind(struct Buffer *buf, struct Buffer *s,
                              intptr_t data, struct Buffer *err)
{
  if (StartupComplete)
  {
    // Save and restore the offset in `s` because parse_dump() might change it
    char *dptr = s->dptr;
    if (parse_dump(buf, s, data, err) == MUTT_CMD_SUCCESS)
      return MUTT_CMD_SUCCESS;
    if (!buf_is_empty(err))
      return MUTT_CMD_ERROR;
    s->dptr = dptr;
  }

  enum MenuType mtypes[MENU_MAX];
  int num_menus = 0;
  enum CommandResult rc = MUTT_CMD_SUCCESS;

  char *key = parse_keymap(mtypes, s, countof(mtypes), &num_menus, err, true);
  if (!key)
    return MUTT_CMD_ERROR;

  /* function to execute */
  parse_extract_token(buf, s, TOKEN_NO_FLAGS);
  if (MoreArgs(s))
  {
    buf_printf(err, _("%s: too many arguments"), "bind");
    rc = MUTT_CMD_ERROR;
  }
  else if (mutt_istr_equal("noop", buf_string(buf)))
  {
    struct Buffer *keystr = buf_pool_get();
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

      int op = km_get_op_menu(mtypes[i], buf_string(buf));

      struct EventBinding ev_b = { mtypes[i], key, op };
      notify_send(NeoMutt->notify, NT_BINDING, NT_BINDING_DELETE, &ev_b);
    }
    buf_pool_release(&keystr);
  }
  else
  {
    struct Buffer *keystr = buf_pool_get();
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
          if (mutt_str_equal(buf_string(buf), mfo->name))
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
      // dump_menu_binds(false);
    }
    buf_pool_release(&keystr);
  }

  FREE(&key);
  return rc;
}

/**
 * parse_unbind - Parse the 'unbind' command - Implements Command::parse() - @ingroup command_parse
 *
 * Command unbinds:
 * - one binding in one or many menu-names
 *   - `unbind index       j`
 *   - `unbind index,pager j`
 * - all bindings in one or many menu-names
 *   - `unbind index *`
 *   - `unbind index`
 *   - `unbind index,pager *`
 *   - `unbind index,pager`
 * - all bindings in all menu-names
 *   - `unbind *`
 *
 * unbind `<menu-name[,...]|*>` [`<key_sequence>`]
 */
enum CommandResult parse_unbind(struct Buffer *buf, struct Buffer *s,
                                intptr_t data, struct Buffer *err)
{
  bool menu_matches[MENU_MAX] = { 0 };
  bool all_keys = false;
  const char *key = NULL;
  // enum CommandResult rc = MUTT_CMD_SUCCESS;

  if (!MoreArgs(s))
  {
    const char *cmd = (data & MUTT_UNMACRO) ? "unmacro" : "unbind";
    buf_printf(err, _("%s: too few arguments"), cmd);
    return MUTT_CMD_ERROR;
  }

  parse_extract_token(buf, s, TOKEN_NO_FLAGS);
  if (mutt_str_equal(buf_string(buf), "*"))
  {
    km_unbind_all_menus(&MenuDefs);
    // XXX reset defaults
    return MUTT_CMD_SUCCESS;
  }

  parse_menu(menu_matches, buf_string(buf), err);

  if (MoreArgs(s))
  {
    parse_extract_token(buf, s, TOKEN_NO_FLAGS);
    key = buf_string(buf);
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
      struct MenuDefinition *md = NULL;

      km_unbind_all_menus(&MenuDefs);

      md = menu_find(MENU_GENERIC);
      km_bind(md, "<enter>", OP_GENERIC_SELECT_ENTRY, NULL, NULL, NULL);
      km_bind(md, "<return>", OP_GENERIC_SELECT_ENTRY, NULL, NULL, NULL);
      km_bind(md, ":", OP_ENTER_COMMAND, NULL, NULL, NULL);
      km_bind(md, "?", OP_HELP, NULL, NULL, NULL);
      km_bind(md, "q", OP_EXIT, NULL, NULL, NULL);

      md = menu_find(MENU_INDEX);
      km_bind(md, "<enter>", OP_DISPLAY_MESSAGE, NULL, NULL, NULL);
      km_bind(md, "<return>", OP_DISPLAY_MESSAGE, NULL, NULL, NULL);

      md = menu_find(MENU_EDITOR);
      km_bind(md, "<backspace>", OP_EDITOR_BACKSPACE, NULL, NULL, NULL);
      km_bind(md, "\177", OP_EDITOR_BACKSPACE, NULL, NULL, NULL);

      md = menu_find(MENU_PAGER);
      km_bind(md, ":", OP_ENTER_COMMAND, NULL, NULL, NULL);
      km_bind(md, "?", OP_HELP, NULL, NULL, NULL);
      km_bind(md, "q", OP_EXIT, NULL, NULL, NULL);

      const char *mname = km_get_menu_name(i);
      mutt_debug(LL_NOTIFY, "NT_MACRO_DELETE_ALL: %s\n", mname);

      struct EventBinding ev_b = { i, NULL, OP_NULL };
      notify_send(NeoMutt->notify, NT_BINDING,
                  (data & MUTT_UNMACRO) ? NT_MACRO_DELETE_ALL : NT_BINDING_DELETE_ALL,
                  &ev_b);
    }
    else
    {
      struct Buffer *keystr = buf_pool_get();
      keymap_expand_string(key, keystr);
      const char *mname = km_get_menu_name(i);
      mutt_debug(LL_NOTIFY, "NT_MACRO_DELETE: %s %s\n", mname, buf_string(keystr));
      buf_pool_release(&keystr);

      struct MenuDefinition *md = menu_find(i);
      km_bind(md, key, OP_NULL, NULL, NULL, NULL);
      struct EventBinding ev_b = { i, key, OP_NULL };
      notify_send(NeoMutt->notify, NT_BINDING,
                  (data & MUTT_UNMACRO) ? NT_MACRO_DELETE : NT_BINDING_DELETE, &ev_b);
    }
  }

  // done:
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_macro - Parse the 'macro' command - Implements Command::parse() - @ingroup command_parse
 *
 * macro `<menu>` `<key>` `<macro>` `<description>`
 */
enum CommandResult parse_macro(struct Buffer *buf, struct Buffer *s,
                               intptr_t data, struct Buffer *err)
{
  if (StartupComplete)
  {
    // Save and restore the offset in `s` because parse_dump() might change it
    char *dptr = s->dptr;
    if (parse_dump(buf, s, data, err) == MUTT_CMD_SUCCESS)
      return MUTT_CMD_SUCCESS;
    if (!buf_is_empty(err))
      return MUTT_CMD_ERROR;
    s->dptr = dptr;
  }

  enum MenuType mtypes[MENU_MAX];
  int num_menus = 0;
  enum CommandResult rc = MUTT_CMD_ERROR;

  char *key = parse_keymap(mtypes, s, countof(mtypes), &num_menus, err, false);
  if (!key)
    return MUTT_CMD_ERROR;

  parse_extract_token(buf, s, TOKEN_CONDENSE);
  /* make sure the macro sequence is not an empty string */
  if (buf_at(buf, 0) == '\0')
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
        struct Buffer *keystr = buf_pool_get();
        for (int i = 0; i < num_menus; i++)
        {
          struct MenuDefinition *md = menu_find(mtypes[i]);
          rc = km_bind(md, key, OP_MACRO, seq, buf->data, NULL);
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
        buf_pool_release(&keystr);
      }

      FREE(&seq);
    }
    else
    {
      struct Buffer *keystr = buf_pool_get();
      for (int i = 0; i < num_menus; i++)
      {
        struct MenuDefinition *md = menu_find(mtypes[i]);
        rc = km_bind(md, key, OP_MACRO, buf->data, NULL, NULL);
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
      buf_pool_release(&keystr);
    }
  }
  FREE(&key);
  return rc;
}

/**
 * parse_exec - Parse the 'exec' command - Implements Command::parse() - @ingroup command_parse
 *
 * XXX can handle multiple args
 * XXX convert to ARRAY
 * XXX comment pushes backwards because MacroEvents is a *stack*
 * XXX change no args msg to "too few" for consistency
 */
enum CommandResult parse_exec(struct Buffer *buf, struct Buffer *s,
                              intptr_t data, struct Buffer *err)
{
  int ops[128];
  int nops = 0;
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
    ops[nops] = km_get_op_menu(mtype, function);
    if (ops[nops] == OP_NULL)
    {
      mutt_flushinp();
      mutt_error(_("%s: no such function"), function);
      return MUTT_CMD_ERROR;
    }
    nops++;
  } while (MoreArgs(s) && nops < countof(ops));

  while (nops)
    mutt_push_macro_event(0, ops[--nops]);

  return MUTT_CMD_SUCCESS;
}
