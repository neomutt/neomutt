/**
 * @file
 * Execute user-defined Hooks
 *
 * @authors
 * Copyright (C) 1996-2007 Michael R. Elkins <me@mutt.org>, and others
 * Copyright (C) 2016 Thomas Adam <thomas@xteddy.org>
 * Copyright (C) 2016-2025 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2017-2021 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2019 Federico Kircheis <federico.kircheis@gmail.com>
 * Copyright (C) 2019 Naveen Nathan <naveen@lastninja.net>
 * Copyright (C) 2022 Oliver Bandel <oliver@first.in-berlin.de>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
 * Copyright (C) 2023-2024 Tóth János <gomba007@gmail.com>
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
 * @page hooks_exec Execute user-defined Hooks
 *
 * Execute user-defined Hooks
 */

#include "config.h"
#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "attach/lib.h"
#include "commands/lib.h"
#include "expando/lib.h"
#include "index/lib.h"
#include "parse/lib.h"
#include "pattern/lib.h"
#include "hook.h"
#include "muttlib.h"
#include "mx.h"
#include "parse.h"

/**
 * exec_folder_hook - Perform a folder hook
 * @param path Path to potentially match
 * @param desc Description to potentially match
 */
void exec_folder_hook(const char *path, const char *desc)
{
  if (!path && !desc)
    return;

  struct Hook *hook = NULL;
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();

  CurrentHookId = CMD_FOLDER_HOOK;

  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if (!hook->command)
      continue;

    if (!(hook->id == CMD_FOLDER_HOOK))
      continue;

    const char *match = NULL;
    if (mutt_regex_match(&hook->regex, path))
      match = path;
    else if (mutt_regex_match(&hook->regex, desc))
      match = desc;

    if (match)
    {
      mutt_debug(LL_DEBUG1, "folder-hook '%s' matches '%s'\n", hook->regex.pattern, match);
      mutt_debug(LL_DEBUG5, "    %s\n", hook->command);
      if (parse_rc_line_cwd(hook->command, hook->source_file, pc, pe) == MUTT_CMD_ERROR)
      {
        mutt_error("%s", buf_string(pe->message));
        break;
      }
    }
  }

  CurrentHookId = CMD_NONE;
  parse_context_free(&pc);
  parse_error_free(&pe);
}

/**
 * mutt_find_hook - Find a matching hook
 * @param id  Hook CommandId, e.g #CMD_FOLDER_HOOK
 * @param pat Pattern to match
 * @retval ptr Command string
 *
 * @note The returned string must not be freed.
 */
char *mutt_find_hook(enum CommandId id, const char *pat)
{
  struct Hook *hook = NULL;

  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if (hook->id == id)
    {
      if (mutt_regex_match(&hook->regex, pat))
        return hook->command;
    }
  }
  return NULL;
}

/**
 * exec_message_hook - Perform a message hook
 * @param m  Mailbox
 * @param e  Email
 * @param id Hook CommandId, e.g #CMD_FOLDER_HOOK
 */
void exec_message_hook(struct Mailbox *m, struct Email *e, enum CommandId id)
{
  struct Hook *hook = NULL;
  struct PatternCache cache = { 0 };
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();

  CurrentHookId = id;

  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if (!hook->command)
      continue;

    if (hook->id == id)
    {
      if ((mutt_pattern_exec(SLIST_FIRST(hook->pattern), 0, m, e, &cache) > 0) ^
          hook->regex.pat_not)
      {
        if (parse_rc_line_cwd(hook->command, hook->source_file, pc, pe) == MUTT_CMD_ERROR)
        {
          mutt_error("%s", buf_string(pe->message));
          goto done;
        }
        /* Executing arbitrary commands could affect the pattern results,
         * so the cache has to be wiped */
        memset(&cache, 0, sizeof(cache));
      }
    }
  }

done:
  CurrentHookId = CMD_NONE;
  parse_context_free(&pc);
  parse_error_free(&pe);
}

/**
 * addr_hook - Perform an address hook (get a path)
 * @param path Buffer for path
 * @param id   Hook CommandId, e.g. #CMD_FOLDER_HOOK
 * @param m    Mailbox
 * @param e    Email
 * @retval  0 Success
 * @retval -1 Failure
 */
static int addr_hook(struct Buffer *path, enum CommandId id, struct Mailbox *m,
                     struct Email *e)
{
  struct Hook *hook = NULL;
  struct PatternCache cache = { 0 };

  /* determine if a matching hook exists */
  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if (!hook->command)
      continue;

    if (hook->id == id)
    {
      if ((mutt_pattern_exec(SLIST_FIRST(hook->pattern), 0, m, e, &cache) > 0) ^
          hook->regex.pat_not)
      {
        buf_alloc(path, PATH_MAX);
        mutt_make_string(path, -1, hook->expando, m, -1, e, MUTT_FORMAT_PLAIN, NULL);
        buf_fix_dptr(path);
        return 0;
      }
    }
  }

  return -1;
}

/**
 * mutt_default_save - Find the default save path for an email
 * @param path  Buffer for the path
 * @param e     Email
 */
void mutt_default_save(struct Buffer *path, struct Email *e)
{
  struct Mailbox *m_cur = get_current_mailbox();
  if (addr_hook(path, CMD_SAVE_HOOK, m_cur, e) == 0)
    return;

  struct Envelope *env = e->env;
  const struct Address *from = TAILQ_FIRST(&env->from);
  const struct Address *reply_to = TAILQ_FIRST(&env->reply_to);
  const struct Address *to = TAILQ_FIRST(&env->to);
  const struct Address *cc = TAILQ_FIRST(&env->cc);
  const struct Address *addr = NULL;
  bool from_me = mutt_addr_is_user(from);

  if (!from_me && reply_to && reply_to->mailbox)
    addr = reply_to;
  else if (!from_me && from && from->mailbox)
    addr = from;
  else if (to && to->mailbox)
    addr = to;
  else if (cc && cc->mailbox)
    addr = cc;
  else
    addr = NULL;
  if (addr)
  {
    struct Buffer *tmp = buf_pool_get();
    generate_save_path(tmp, addr);
    buf_add_printf(path, "=%s", buf_string(tmp));
    buf_pool_release(&tmp);
  }
}

/**
 * mutt_select_fcc - Select the FCC path for an email
 * @param path    Buffer for the path
 * @param e       Email
 */
void mutt_select_fcc(struct Buffer *path, struct Email *e)
{
  buf_alloc(path, PATH_MAX);

  if (addr_hook(path, CMD_FCC_HOOK, NULL, e) != 0)
  {
    const struct Address *to = TAILQ_FIRST(&e->env->to);
    const struct Address *cc = TAILQ_FIRST(&e->env->cc);
    const struct Address *bcc = TAILQ_FIRST(&e->env->bcc);
    const bool c_save_name = cs_subset_bool(NeoMutt->sub, "save_name");
    const bool c_force_name = cs_subset_bool(NeoMutt->sub, "force_name");
    const char *const c_record = cs_subset_string(NeoMutt->sub, "record");
    if ((c_save_name || c_force_name) && (to || cc || bcc))
    {
      const struct Address *addr = to ? to : (cc ? cc : bcc);
      struct Buffer *buf = buf_pool_get();
      generate_save_path(buf, addr);
      const char *const c_folder = cs_subset_string(NeoMutt->sub, "folder");
      buf_concat_path(path, NONULL(c_folder), buf_string(buf));
      buf_pool_release(&buf);
      if (!c_force_name && (mx_access(buf_string(path), W_OK) != 0))
        buf_strcpy(path, c_record);
    }
    else
    {
      buf_strcpy(path, c_record);
    }
  }
  else
  {
    buf_fix_dptr(path);
  }

  pretty_mailbox(path);
}

/**
 * list_hook - Find hook strings matching
 * @param[out] matches List of hook strings
 * @param[in]  match   String to match
 * @param[in]  id      Hook CommandId, e.g. #CMD_FOLDER_HOOK
 */
static void list_hook(struct ListHead *matches, const char *match, enum CommandId id)
{
  struct Hook *tmp = NULL;

  TAILQ_FOREACH(tmp, &Hooks, entries)
  {
    if ((tmp->id == id) && mutt_regex_match(&tmp->regex, match))
    {
      mutt_list_insert_tail(matches, mutt_str_dup(tmp->command));
    }
  }
}

/**
 * mutt_crypt_hook - Find crypto hooks for an Address
 * @param[out] list List of keys
 * @param[in]  addr Address to match
 *
 * The crypt-hook associates keys with addresses.
 */
void mutt_crypt_hook(struct ListHead *list, struct Address *addr)
{
  list_hook(list, buf_string(addr->mailbox), CMD_CRYPT_HOOK);
}

/**
 * exec_account_hook - Perform an account hook
 * @param url Account URL to match
 */
void exec_account_hook(const char *url)
{
  /* parsing commands with URLs in an account hook can cause a recursive
   * call. We just skip processing if this occurs. Typically such commands
   * belong in a folder-hook -- perhaps we should warn the user. */
  static bool inhook = false;
  if (inhook)
    return;

  struct Hook *hook = NULL;
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();

  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if (!(hook->command && (hook->id == CMD_ACCOUNT_HOOK)))
      continue;

    if (mutt_regex_match(&hook->regex, url))
    {
      inhook = true;
      mutt_debug(LL_DEBUG1, "account-hook '%s' matches '%s'\n", hook->regex.pattern, url);
      mutt_debug(LL_DEBUG5, "    %s\n", hook->command);

      if (parse_rc_line_cwd(hook->command, hook->source_file, pc, pe) == MUTT_CMD_ERROR)
      {
        mutt_error("%s", buf_string(pe->message));
        inhook = false;
        goto done;
      }

      inhook = false;
    }
  }

done:
  parse_context_free(&pc);
  parse_error_free(&pe);
}

/**
 * exec_timeout_hook - Execute any timeout hooks
 *
 * The user can configure hooks to be run on timeout.
 * This function finds all the matching hooks and executes them.
 */
void exec_timeout_hook(void)
{
  struct Hook *hook = NULL;
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();

  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if (!(hook->command && (hook->id == CMD_TIMEOUT_HOOK)))
      continue;

    if (parse_rc_line_cwd(hook->command, hook->source_file, pc, pe) == MUTT_CMD_ERROR)
    {
      mutt_error("%s", buf_string(pe->message));
      parse_error_reset(pe);

      /* The hooks should be independent of each other, so even though this one
       * failed, we'll carry on with the others. */
    }
  }

  /* Delete temporary attachment files */
  mutt_temp_attachments_cleanup();

  parse_context_free(&pc);
  parse_error_free(&pe);
}

/**
 * exec_startup_shutdown_hook - Execute any startup/shutdown hooks
 * @param id Hook CommandId: #CMD_STARTUP_HOOK or #CMD_SHUTDOWN_HOOK
 *
 * The user can configure hooks to be run on startup/shutdown.
 * This function finds all the matching hooks and executes them.
 */
void exec_startup_shutdown_hook(enum CommandId id)
{
  struct Hook *hook = NULL;
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();

  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if (!(hook->command && (hook->id == id)))
      continue;

    if (parse_rc_line_cwd(hook->command, hook->source_file, pc, pe) == MUTT_CMD_ERROR)
    {
      mutt_error("%s", buf_string(pe->message));
      parse_error_reset(pe);
    }
  }

  parse_context_free(&pc);
  parse_error_free(&pe);
}

/**
 * mutt_idxfmt_hook - Get index-format-hook format string
 * @param name Hook name
 * @param m    Mailbox
 * @param e    Email
 * @retval ptr  Expando
 * @retval NULL No matching hook
 */
const struct Expando *mutt_idxfmt_hook(const char *name, struct Mailbox *m, struct Email *e)
{
  if (!IdxFmtHooks)
    return NULL;

  struct HookList *hl = mutt_hash_find(IdxFmtHooks, name);
  if (!hl)
    return NULL;

  CurrentHookId = CMD_INDEX_FORMAT_HOOK;

  struct PatternCache cache = { 0 };
  const struct Expando *exp = NULL;
  struct Hook *hook = NULL;

  TAILQ_FOREACH(hook, hl, entries)
  {
    struct Pattern *pat = SLIST_FIRST(hook->pattern);
    if ((mutt_pattern_exec(pat, 0, m, e, &cache) > 0) ^ hook->regex.pat_not)
    {
      exp = hook->expando;
      break;
    }
  }

  CurrentHookId = CMD_NONE;

  return exp;
}
