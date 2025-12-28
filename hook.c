/**
 * @file
 * Parse and execute user-defined hooks
 *
 * @authors
 * Copyright (C) 1996-2002,2004,2007 Michael R. Elkins <me@mutt.org>, and others
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
 * @page neo_hook Parse and execute user-defined hooks
 *
 * Parse and execute user-defined hooks
 */

#include "config.h"
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "hook.h"
#include "attach/lib.h"
#include "compmbox/lib.h"
#include "expando/lib.h"
#include "index/lib.h"
#include "parse/lib.h"
#include "pattern/lib.h"
#include "commands.h"
#include "globals.h"
#include "muttlib.h"
#include "mx.h"

extern const struct ExpandoDefinition IndexFormatDef[];

/**
 * struct Hook - A list of user hooks
 */
struct Hook
{
  enum CommandId id;           ///< Hook CommandId, e.g. #CMD_FOLDER_HOOK
  struct Regex regex;          ///< Regular expression
  char *command;               ///< Filename, command or pattern to execute
  char *source_file;           ///< Used for relative-directory source
  struct PatternList *pattern; ///< Used for fcc,save,send-hook
  struct Expando *expando;     ///< Used for format hooks
  TAILQ_ENTRY(Hook) entries;   ///< Linked list
};
TAILQ_HEAD(HookList, Hook);

/// All simple hooks, e.g. CMD_FOLDER_HOOK
static struct HookList Hooks = TAILQ_HEAD_INITIALIZER(Hooks);

/// All Index Format hooks
static struct HashTable *IdxFmtHooks = NULL;

/// The ID of the Hook currently being executed, e.g. #CMD_SAVE_HOOK
static enum CommandId CurrentHookId = CMD_NONE;

/**
 * hook_free - Free a Hook
 * @param ptr Hook to free
 */
static void hook_free(struct Hook **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Hook *h = *ptr;

  FREE(&h->command);
  FREE(&h->source_file);
  FREE(&h->regex.pattern);
  if (h->regex.regex)
  {
    regfree(h->regex.regex);
    FREE(&h->regex.regex);
  }
  mutt_pattern_free(&h->pattern);
  expando_free(&h->expando);
  FREE(ptr);
}

/**
 * hook_new - Create a Hook
 * @retval ptr New Hook
 */
static struct Hook *hook_new(void)
{
  return MUTT_MEM_CALLOC(1, struct Hook);
}

/**
 * parse_hook_charset - Parse charset Hook commands - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `charset-hook <alias>   <charset>`
 * - `iconv-hook   <charset> <local-charset>`
 */
enum CommandResult parse_hook_charset(const struct Command *cmd,
                                      struct Buffer *line, struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *alias = buf_pool_get();
  struct Buffer *charset = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  if (parse_extract_token(alias, line, TOKEN_NO_FLAGS) < 0)
    goto done;
  if (parse_extract_token(charset, line, TOKEN_NO_FLAGS) < 0)
    goto done;

  const enum LookupType type = (cmd->id == CMD_ICONV_HOOK) ? MUTT_LOOKUP_ICONV :
                                                             MUTT_LOOKUP_CHARSET;

  if (buf_is_empty(alias) || buf_is_empty(charset))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
  }
  else if (MoreArgs(line))
  {
    buf_printf(err, _("%s: too many arguments"), cmd->name);
    buf_reset(line); // clean up buffer to avoid a mess with further rcfile processing
    rc = MUTT_CMD_WARNING;
  }
  else if (mutt_ch_lookup_add(type, buf_string(alias), buf_string(charset), err))
  {
    rc = MUTT_CMD_SUCCESS;
  }

done:
  buf_pool_release(&alias);
  buf_pool_release(&charset);

  return rc;
}

/**
 * parse_hook_global - Parse global Hook commands - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `shutdown-hook <command>`
 * - `startup-hook  <command>`
 * - `timeout-hook  <command>`
 */
enum CommandResult parse_hook_global(const struct Command *cmd,
                                     struct Buffer *line, struct Buffer *err)
{
  struct Hook *hook = NULL;
  enum CommandResult rc = MUTT_CMD_ERROR;

  struct Buffer *command = buf_pool_get();

  // TOKEN_SPACE allows the command to contain whitespace, without quoting
  parse_extract_token(command, line, TOKEN_SPACE);

  if (buf_is_empty(command))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
    goto cleanup;
  }

  if (MoreArgs(line))
  {
    buf_printf(err, _("%s: too many arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
    goto cleanup;
  }

  /* check to make sure that a matching Hook doesn't already exist */
  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    /* Ignore duplicate global Hooks */
    if ((hook->id == cmd->id) && mutt_str_equal(hook->command, buf_string(command)))
    {
      rc = MUTT_CMD_SUCCESS;
      goto cleanup;
    }
  }

  hook = hook_new();
  hook->id = cmd->id;
  hook->command = buf_strdup(command);
  hook->source_file = mutt_get_sourced_cwd();
  hook->pattern = NULL;
  hook->regex.pattern = NULL;
  hook->regex.regex = NULL;
  hook->regex.pat_not = false;
  hook->expando = NULL;

  TAILQ_INSERT_TAIL(&Hooks, hook, entries);
  rc = MUTT_CMD_SUCCESS;

cleanup:
  buf_pool_release(&command);
  return rc;
}

/**
 * parse_hook_pattern - Parse pattern-based Hook commands - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `message-hook <pattern> <command>`
 * - `reply-hook   <pattern> <command>`
 * - `send-hook    <pattern> <command>`
 * - `send2-hook   <pattern> <command>`
 */
enum CommandResult parse_hook_pattern(const struct Command *cmd,
                                      struct Buffer *line, struct Buffer *err)
{
  struct Hook *hook = NULL;
  enum CommandResult rc = MUTT_CMD_ERROR;
  bool pat_not = false;
  struct PatternList *pat = NULL;

  struct Buffer *command = buf_pool_get();
  struct Buffer *pattern = buf_pool_get();

  if (*line->dptr == '!')
  {
    line->dptr++;
    SKIPWS(line->dptr);
    pat_not = true;
  }

  parse_extract_token(pattern, line, TOKEN_NO_FLAGS);

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
    goto cleanup;
  }

  // TOKEN_SPACE allows the command to contain whitespace, without quoting
  parse_extract_token(command, line, TOKEN_SPACE);

  if (buf_is_empty(command))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
    goto cleanup;
  }

  if (MoreArgs(line))
  {
    buf_printf(err, _("%s: too many arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
    goto cleanup;
  }

  const char *const c_default_hook = cs_subset_string(NeoMutt->sub, "default_hook");
  if (c_default_hook)
  {
    mutt_check_simple(pattern, c_default_hook);
  }

  /* check to make sure that a matching hook doesn't already exist */
  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if ((hook->id == cmd->id) && (hook->regex.pat_not == pat_not) &&
        mutt_str_equal(buf_string(pattern), hook->regex.pattern))
    {
      /* these hooks allow multiple commands with the same pattern,
       * so if we've already seen this pattern/command pair,
       * just ignore it instead of creating a duplicate */
      if (mutt_str_equal(hook->command, buf_string(command)))
      {
        rc = MUTT_CMD_SUCCESS;
        goto cleanup;
      }
    }
  }

  PatternCompFlags comp_flags;
  if (cmd->id == CMD_SEND2_HOOK)
    comp_flags = MUTT_PC_SEND_MODE_SEARCH;
  else if (cmd->id == CMD_SEND_HOOK)
    comp_flags = MUTT_PC_NO_FLAGS;
  else
    comp_flags = MUTT_PC_FULL_MSG;

  struct MailboxView *mv_cur = get_current_mailbox_view();
  pat = mutt_pattern_comp(mv_cur, buf_string(pattern), comp_flags, err);
  if (!pat)
    goto cleanup;

  hook = hook_new();
  hook->id = cmd->id;
  hook->command = buf_strdup(command);
  hook->source_file = mutt_get_sourced_cwd();
  hook->pattern = pat;
  hook->regex.pattern = buf_strdup(pattern);
  hook->regex.regex = NULL;
  hook->regex.pat_not = pat_not;
  hook->expando = NULL;

  TAILQ_INSERT_TAIL(&Hooks, hook, entries);
  rc = MUTT_CMD_SUCCESS;

cleanup:
  buf_pool_release(&command);
  buf_pool_release(&pattern);
  return rc;
}

/**
 * add_mailbox_hook - Add a Mailbox Hook
 * @param id      CommandId, e.g. CMD_FCC_SAVE_HOOK
 * @param mailbox Mailbox
 * @param pattern Pattern to match
 * @param pat_not true of Pattern is inverted
 * @param err     Buffer for error message
 * @retval enum CommandResult, e.g. MUTT_CMD_SUCCESS
 */
enum CommandResult add_mailbox_hook(enum CommandId id, struct Buffer *mailbox,
                                    struct Buffer *pattern, bool pat_not, struct Buffer *err)
{
  struct Hook *hook = NULL;
  struct PatternList *pat = NULL;

  /* check to make sure that a matching hook doesn't already exist */
  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if ((hook->id == id) && (hook->regex.pat_not == pat_not) &&
        mutt_str_equal(buf_string(pattern), hook->regex.pattern))
    {
      // Update an existing hook
      FREE(&hook->command);
      hook->command = buf_strdup(mailbox);
      FREE(&hook->source_file);
      hook->source_file = mutt_get_sourced_cwd();

      expando_free(&hook->expando);
      hook->expando = expando_parse(buf_string(mailbox), IndexFormatDef, err);

      return MUTT_CMD_SUCCESS;
    }
  }

  PatternCompFlags comp_flags;
  if (id == CMD_FCC_HOOK)
    comp_flags = MUTT_PC_NO_FLAGS;
  else
    comp_flags = MUTT_PC_FULL_MSG;

  struct MailboxView *mv_cur = get_current_mailbox_view();
  pat = mutt_pattern_comp(mv_cur, buf_string(pattern), comp_flags, err);
  if (!pat)
    return MUTT_CMD_ERROR;

  struct Expando *exp = expando_parse(buf_string(mailbox), IndexFormatDef, err);

  hook = hook_new();
  hook->id = id;
  hook->command = buf_strdup(mailbox);
  hook->source_file = mutt_get_sourced_cwd();
  hook->pattern = pat;
  hook->regex.pattern = buf_strdup(pattern);
  hook->regex.regex = NULL;
  hook->regex.pat_not = pat_not;
  hook->expando = exp;

  TAILQ_INSERT_TAIL(&Hooks, hook, entries);
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_hook_mailbox - Parse mailbox pattern hook commands - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `fcc-hook      <pattern> <mailbox>`
 * - `fcc-save-hook <pattern> <mailbox>`
 * - `save-hook     <pattern> <mailbox>`
 */
enum CommandResult parse_hook_mailbox(const struct Command *cmd,
                                      struct Buffer *line, struct Buffer *err)
{
  enum CommandResult rc = MUTT_CMD_ERROR;
  bool pat_not = false;

  struct Buffer *pattern = buf_pool_get();
  struct Buffer *mailbox = buf_pool_get();

  if (*line->dptr == '!')
  {
    line->dptr++;
    SKIPWS(line->dptr);
    pat_not = true;
  }

  parse_extract_token(pattern, line, TOKEN_NO_FLAGS);

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
    goto cleanup;
  }

  parse_extract_token(mailbox, line, TOKEN_NO_FLAGS);

  if (buf_is_empty(mailbox))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
    goto cleanup;
  }

  if (MoreArgs(line))
  {
    buf_printf(err, _("%s: too many arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
    goto cleanup;
  }

  const char *const c_default_hook = cs_subset_string(NeoMutt->sub, "default_hook");
  if (c_default_hook)
  {
    mutt_check_simple(pattern, c_default_hook);
  }

  buf_expand_path(mailbox);

  if ((cmd->id == CMD_FCC_HOOK) || (cmd->id == CMD_FCC_SAVE_HOOK))
  {
    rc = add_mailbox_hook(CMD_FCC_HOOK, mailbox, pattern, pat_not, err);
    if (rc != MUTT_CMD_SUCCESS)
      goto cleanup;
  }

  if ((cmd->id == CMD_SAVE_HOOK) || (cmd->id == CMD_FCC_SAVE_HOOK))
  {
    rc = add_mailbox_hook(CMD_SAVE_HOOK, mailbox, pattern, pat_not, err);
    if (rc != MUTT_CMD_SUCCESS)
      goto cleanup;
  }

  rc = MUTT_CMD_SUCCESS;

cleanup:
  buf_pool_release(&pattern);
  buf_pool_release(&mailbox);
  return rc;
}

/**
 * parse_hook_regex - Parse regex-based hook command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `account-hook <regex> <command>`
 */
enum CommandResult parse_hook_regex(const struct Command *cmd,
                                    struct Buffer *line, struct Buffer *err)
{
  struct Hook *hook = NULL;
  enum CommandResult rc = MUTT_CMD_ERROR;
  bool pat_not = false;
  regex_t *rx = NULL;

  struct Buffer *regex = buf_pool_get();
  struct Buffer *command = buf_pool_get();

  if (*line->dptr == '!')
  {
    line->dptr++;
    SKIPWS(line->dptr);
    pat_not = true;
  }

  parse_extract_token(regex, line, TOKEN_NO_FLAGS);

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
    goto cleanup;
  }

  parse_extract_token(command, line, TOKEN_SPACE);

  if (buf_is_empty(command))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
    goto cleanup;
  }

  if (MoreArgs(line))
  {
    buf_printf(err, _("%s: too many arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
    goto cleanup;
  }

  /* check to make sure that a matching hook doesn't already exist */
  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if ((hook->id == cmd->id) && (hook->regex.pat_not == pat_not) &&
        mutt_str_equal(buf_string(regex), hook->regex.pattern))
    {
      // Ignore duplicate hooks
      if (mutt_str_equal(hook->command, buf_string(command)))
      {
        rc = MUTT_CMD_SUCCESS;
        goto cleanup;
      }
    }
  }

  /* Hooks not allowing full patterns: Check syntax of regex */
  rx = MUTT_MEM_CALLOC(1, regex_t);
  int rc2 = REG_COMP(rx, buf_string(regex), 0);
  if (rc2 != 0)
  {
    regerror(rc2, rx, err->data, err->dsize);
    FREE(&rx);
    goto cleanup;
  }

  hook = hook_new();
  hook->id = cmd->id;
  hook->command = buf_strdup(command);
  hook->source_file = mutt_get_sourced_cwd();
  hook->pattern = NULL;
  hook->regex.pattern = buf_strdup(regex);
  hook->regex.regex = rx;
  hook->regex.pat_not = pat_not;
  hook->expando = NULL;

  TAILQ_INSERT_TAIL(&Hooks, hook, entries);
  rc = MUTT_CMD_SUCCESS;

cleanup:
  buf_pool_release(&regex);
  buf_pool_release(&command);
  return rc;
}

/**
 * parse_hook_folder - Parse folder hook command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `folder-hook [ -noregex ] <regex> <command>`
 */
enum CommandResult parse_hook_folder(const struct Command *cmd,
                                     struct Buffer *line, struct Buffer *err)
{
  struct Hook *hook = NULL;
  enum CommandResult rc = MUTT_CMD_ERROR;
  bool pat_not = false;
  bool use_regex = true;
  regex_t *rx = NULL;

  struct Buffer *regex = buf_pool_get();
  struct Buffer *command = buf_pool_get();

  if (*line->dptr == '!')
  {
    line->dptr++;
    SKIPWS(line->dptr);
    pat_not = true;
  }

  parse_extract_token(regex, line, TOKEN_NO_FLAGS);
  if (mutt_str_equal(buf_string(regex), "-noregex"))
  {
    use_regex = false;
    if (!MoreArgs(line))
    {
      buf_printf(err, _("%s: too few arguments"), cmd->name);
      rc = MUTT_CMD_WARNING;
      goto cleanup;
    }
    parse_extract_token(regex, line, TOKEN_NO_FLAGS);
  }

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
    goto cleanup;
  }

  parse_extract_token(command, line, TOKEN_SPACE);

  if (buf_is_empty(command))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
    goto cleanup;
  }

  if (MoreArgs(line))
  {
    buf_printf(err, _("%s: too many arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
    goto cleanup;
  }

  /* Accidentally using the ^ mailbox shortcut in the .neomuttrc is a
   * common mistake */
  if ((buf_at(regex, 0) == '^') && !CurrentFolder)
  {
    buf_strcpy(err, _("current mailbox shortcut '^' is unset"));
    goto cleanup;
  }

  struct Buffer *tmp = buf_pool_get();
  buf_copy(tmp, regex);
  buf_expand_path_regex(tmp, use_regex);

  /* Check for other mailbox shortcuts that expand to the empty string.
   * This is likely a mistake too */
  if (buf_is_empty(tmp) && !buf_is_empty(regex))
  {
    buf_strcpy(err, _("mailbox shortcut expanded to empty regex"));
    buf_pool_release(&tmp);
    goto cleanup;
  }

  if (use_regex)
  {
    buf_copy(regex, tmp);
  }
  else
  {
    mutt_file_sanitize_regex(regex, buf_string(tmp));
  }
  buf_pool_release(&tmp);

  /* check to make sure that a matching hook doesn't already exist */
  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if ((hook->id == cmd->id) && (hook->regex.pat_not == pat_not) &&
        mutt_str_equal(buf_string(regex), hook->regex.pattern))
    {
      // Ignore duplicate hooks
      if (mutt_str_equal(hook->command, buf_string(command)))
      {
        rc = MUTT_CMD_SUCCESS;
        goto cleanup;
      }
    }
  }

  /* Hooks not allowing full patterns: Check syntax of regex */
  rx = MUTT_MEM_CALLOC(1, regex_t);
  int rc2 = REG_COMP(rx, buf_string(regex), 0);
  if (rc2 != 0)
  {
    regerror(rc2, rx, err->data, err->dsize);
    FREE(&rx);
    goto cleanup;
  }

  hook = hook_new();
  hook->id = cmd->id;
  hook->command = buf_strdup(command);
  hook->source_file = mutt_get_sourced_cwd();
  hook->pattern = NULL;
  hook->regex.pattern = buf_strdup(regex);
  hook->regex.regex = rx;
  hook->regex.pat_not = pat_not;
  hook->expando = NULL;

  TAILQ_INSERT_TAIL(&Hooks, hook, entries);
  rc = MUTT_CMD_SUCCESS;

cleanup:
  buf_pool_release(&regex);
  buf_pool_release(&command);
  return rc;
}

/**
 * parse_hook_crypt - Parse crypt hook commands - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `crypt-hook <regex> <keyid>`
 * - `pgp-hook` is a deprecated synonym for `crypt-hook`
 */
enum CommandResult parse_hook_crypt(const struct Command *cmd,
                                    struct Buffer *line, struct Buffer *err)
{
  struct Hook *hook = NULL;
  enum CommandResult rc = MUTT_CMD_ERROR;
  bool pat_not = false;
  regex_t *rx = NULL;

  struct Buffer *regex = buf_pool_get();
  struct Buffer *keyid = buf_pool_get();

  if (*line->dptr == '!')
  {
    line->dptr++;
    SKIPWS(line->dptr);
    pat_not = true;
  }

  parse_extract_token(regex, line, TOKEN_NO_FLAGS);

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
    goto cleanup;
  }

  parse_extract_token(keyid, line, TOKEN_NO_FLAGS);

  if (buf_is_empty(keyid))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
    goto cleanup;
  }

  if (MoreArgs(line))
  {
    buf_printf(err, _("%s: too many arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
    goto cleanup;
  }

  /* check to make sure that a matching hook doesn't already exist */
  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if ((hook->id == cmd->id) && (hook->regex.pat_not == pat_not) &&
        mutt_str_equal(buf_string(regex), hook->regex.pattern))
    {
      // Ignore duplicate hooks
      if (mutt_str_equal(hook->command, buf_string(keyid)))
      {
        rc = MUTT_CMD_SUCCESS;
        goto cleanup;
      }
    }
  }

  /* Hooks not allowing full patterns: Check syntax of regex */
  rx = MUTT_MEM_CALLOC(1, regex_t);
  int rc2 = REG_COMP(rx, buf_string(regex), REG_ICASE);
  if (rc2 != 0)
  {
    regerror(rc2, rx, err->data, err->dsize);
    FREE(&rx);
    goto cleanup;
  }

  hook = hook_new();
  hook->id = cmd->id;
  hook->command = buf_strdup(keyid);
  hook->source_file = mutt_get_sourced_cwd();
  hook->pattern = NULL;
  hook->regex.pattern = buf_strdup(regex);
  hook->regex.regex = rx;
  hook->regex.pat_not = pat_not;
  hook->expando = NULL;

  TAILQ_INSERT_TAIL(&Hooks, hook, entries);
  rc = MUTT_CMD_SUCCESS;

cleanup:
  buf_pool_release(&regex);
  buf_pool_release(&keyid);
  return rc;
}

/**
 * parse_hook_mbox - Parse mbox hook command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `mbox-hook [ -noregex ] <regex> <mailbox>`
 */
enum CommandResult parse_hook_mbox(const struct Command *cmd,
                                   struct Buffer *line, struct Buffer *err)
{
  struct Hook *hook = NULL;
  enum CommandResult rc = MUTT_CMD_ERROR;
  bool pat_not = false;
  bool use_regex = true;
  regex_t *rx = NULL;

  struct Buffer *regex = buf_pool_get();
  struct Buffer *command = buf_pool_get();

  if (*line->dptr == '!')
  {
    line->dptr++;
    SKIPWS(line->dptr);
    pat_not = true;
  }

  parse_extract_token(regex, line, TOKEN_NO_FLAGS);
  if (mutt_str_equal(buf_string(regex), "-noregex"))
  {
    use_regex = false;
    if (!MoreArgs(line))
    {
      buf_printf(err, _("%s: too few arguments"), cmd->name);
      rc = MUTT_CMD_WARNING;
      goto cleanup;
    }
    parse_extract_token(regex, line, TOKEN_NO_FLAGS);
  }

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
    goto cleanup;
  }

  parse_extract_token(command, line, TOKEN_NO_FLAGS);

  if (buf_is_empty(command))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
    goto cleanup;
  }

  if (MoreArgs(line))
  {
    buf_printf(err, _("%s: too many arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
    goto cleanup;
  }

  /* Accidentally using the ^ mailbox shortcut in the .neomuttrc is a
   * common mistake */
  if ((buf_at(regex, 0) == '^') && !CurrentFolder)
  {
    buf_strcpy(err, _("current mailbox shortcut '^' is unset"));
    goto cleanup;
  }

  struct Buffer *tmp = buf_pool_get();
  buf_copy(tmp, regex);
  buf_expand_path_regex(tmp, use_regex);

  /* Check for other mailbox shortcuts that expand to the empty string.
   * This is likely a mistake too */
  if (buf_is_empty(tmp) && !buf_is_empty(regex))
  {
    buf_strcpy(err, _("mailbox shortcut expanded to empty regex"));
    buf_pool_release(&tmp);
    goto cleanup;
  }

  if (use_regex)
  {
    buf_copy(regex, tmp);
  }
  else
  {
    mutt_file_sanitize_regex(regex, buf_string(tmp));
  }
  buf_pool_release(&tmp);

  buf_expand_path(command);

  /* check to make sure that a matching hook doesn't already exist */
  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if ((hook->id == cmd->id) && (hook->regex.pat_not == pat_not) &&
        mutt_str_equal(buf_string(regex), hook->regex.pattern))
    {
      // Update an existing hook
      FREE(&hook->command);
      hook->command = buf_strdup(command);
      FREE(&hook->source_file);
      hook->source_file = mutt_get_sourced_cwd();

      expando_free(&hook->expando);
      hook->expando = expando_parse(buf_string(command), IndexFormatDef, err);

      rc = MUTT_CMD_SUCCESS;
      goto cleanup;
    }
  }

  /* Hooks not allowing full patterns: Check syntax of regex */
  rx = MUTT_MEM_CALLOC(1, regex_t);
  int rc2 = REG_COMP(rx, buf_string(regex), 0);
  if (rc2 != 0)
  {
    regerror(rc2, rx, err->data, err->dsize);
    FREE(&rx);
    goto cleanup;
  }

  struct Expando *exp = expando_parse(buf_string(command), IndexFormatDef, err);

  hook = hook_new();
  hook->id = cmd->id;
  hook->command = buf_strdup(command);
  hook->source_file = mutt_get_sourced_cwd();
  hook->pattern = NULL;
  hook->regex.pattern = buf_strdup(regex);
  hook->regex.regex = rx;
  hook->regex.pat_not = pat_not;
  hook->expando = exp;

  TAILQ_INSERT_TAIL(&Hooks, hook, entries);
  rc = MUTT_CMD_SUCCESS;

cleanup:
  buf_pool_release(&regex);
  buf_pool_release(&command);
  return rc;
}

/**
 * parse_hook_compress - Parse compress hook commands - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `append-hook <regex> <shell-command>`
 * - `close-hook  <regex> <shell-command>`
 * - `open-hook   <regex> <shell-command>`
 */
enum CommandResult parse_hook_compress(const struct Command *cmd,
                                       struct Buffer *line, struct Buffer *err)
{
  struct Hook *hook = NULL;
  enum CommandResult rc = MUTT_CMD_ERROR;
  bool pat_not = false;
  regex_t *rx = NULL;

  struct Buffer *regex = buf_pool_get();
  struct Buffer *command = buf_pool_get();

  if (*line->dptr == '!')
  {
    line->dptr++;
    SKIPWS(line->dptr);
    pat_not = true;
  }

  parse_extract_token(regex, line, TOKEN_NO_FLAGS);

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
    goto cleanup;
  }

  // TOKEN_SPACE allows the command to contain whitespace, without quoting
  parse_extract_token(command, line, TOKEN_SPACE);

  if (buf_is_empty(command))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
    goto cleanup;
  }

  if (MoreArgs(line))
  {
    buf_printf(err, _("%s: too many arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
    goto cleanup;
  }

  if (mutt_comp_valid_command(buf_string(command)) == 0)
  {
    buf_strcpy(err, _("badly formatted command string"));
    goto cleanup;
  }

  /* check to make sure that a matching hook doesn't already exist */
  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if ((hook->id == cmd->id) && (hook->regex.pat_not == pat_not) &&
        mutt_str_equal(buf_string(regex), hook->regex.pattern))
    {
      // Update an existing hook
      FREE(&hook->command);
      hook->command = buf_strdup(command);
      FREE(&hook->source_file);
      hook->source_file = mutt_get_sourced_cwd();

      rc = MUTT_CMD_SUCCESS;
      goto cleanup;
    }
  }

  /* Hooks not allowing full patterns: Check syntax of regex */
  rx = MUTT_MEM_CALLOC(1, regex_t);
  int rc2 = REG_COMP(rx, buf_string(regex), 0);
  if (rc2 != 0)
  {
    regerror(rc2, rx, err->data, err->dsize);
    FREE(&rx);
    goto cleanup;
  }

  hook = hook_new();
  hook->id = cmd->id;
  hook->command = buf_strdup(command);
  hook->source_file = mutt_get_sourced_cwd();
  hook->pattern = NULL;
  hook->regex.pattern = buf_strdup(regex);
  hook->regex.regex = rx;
  hook->regex.pat_not = pat_not;
  hook->expando = NULL;

  TAILQ_INSERT_TAIL(&Hooks, hook, entries);
  rc = MUTT_CMD_SUCCESS;

cleanup:
  buf_pool_release(&regex);
  buf_pool_release(&command);
  return rc;
}

/**
 * mutt_delete_hooks - Delete matching hooks
 * @param id Hook CommandId to delete, e.g. #CMD_SEND_HOOK
 *
 * If CMD_NONE is passed, all the hooks will be deleted.
 */
void mutt_delete_hooks(enum CommandId id)
{
  struct Hook *h = NULL;
  struct Hook *tmp = NULL;

  TAILQ_FOREACH_SAFE(h, &Hooks, entries, tmp)
  {
    if ((id == CMD_NONE) || (id == h->id))
    {
      TAILQ_REMOVE(&Hooks, h, entries);
      hook_free(&h);
    }
  }
}

/**
 * idxfmt_hashelem_free - Free our hash table data - Implements ::hash_hdata_free_t - @ingroup hash_hdata_free_api
 */
static void idxfmt_hashelem_free(int type, void *obj, intptr_t data)
{
  struct HookList *hl = obj;
  struct Hook *h = NULL;
  struct Hook *tmp = NULL;

  TAILQ_FOREACH_SAFE(h, hl, entries, tmp)
  {
    TAILQ_REMOVE(hl, h, entries);
    hook_free(&h);
  }

  FREE(&hl);
}

/**
 * delete_idxfmt_hooks - Delete all the index-format-hooks
 */
static void delete_idxfmt_hooks(void)
{
  mutt_hash_free(&IdxFmtHooks);
}

/**
 * parse_hook_index - Parse the index format hook command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `index-format-hook <name> [ ! ]<pattern> <format-string>`
 */
enum CommandResult parse_hook_index(const struct Command *cmd,
                                    struct Buffer *line, struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  enum CommandResult rc = MUTT_CMD_ERROR;
  bool pat_not = false;

  struct Buffer *name = buf_pool_get();
  struct Buffer *pattern = buf_pool_get();
  struct Buffer *fmt = buf_pool_get();
  struct Expando *exp = NULL;

  if (!IdxFmtHooks)
  {
    IdxFmtHooks = mutt_hash_new(30, MUTT_HASH_STRDUP_KEYS);
    mutt_hash_set_destructor(IdxFmtHooks, idxfmt_hashelem_free, 0);
  }

  parse_extract_token(name, line, TOKEN_NO_FLAGS);
  struct HookList *hl = mutt_hash_find(IdxFmtHooks, buf_string(name));

  if (*line->dptr == '!')
  {
    line->dptr++;
    SKIPWS(line->dptr);
    pat_not = true;
  }
  parse_extract_token(pattern, line, TOKEN_NO_FLAGS);

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    goto out;
  }
  parse_extract_token(fmt, line, TOKEN_NO_FLAGS);

  exp = expando_parse(buf_string(fmt), IndexFormatDef, err);
  if (!exp)
    goto out;

  if (MoreArgs(line))
  {
    buf_printf(err, _("%s: too many arguments"), cmd->name);
    goto out;
  }

  const char *const c_default_hook = cs_subset_string(NeoMutt->sub, "default_hook");
  if (c_default_hook)
    mutt_check_simple(pattern, c_default_hook);

  /* check to make sure that a matching hook doesn't already exist */
  struct Hook *hook = NULL;
  if (hl)
  {
    TAILQ_FOREACH(hook, hl, entries)
    {
      // Update an existing hook
      if ((hook->regex.pat_not == pat_not) &&
          mutt_str_equal(buf_string(pattern), hook->regex.pattern))
      {
        expando_free(&hook->expando);
        hook->expando = exp;
        exp = NULL;
        rc = MUTT_CMD_SUCCESS;
        goto out;
      }
    }
  }

  /* MUTT_PC_PATTERN_DYNAMIC sets so that date ranges are regenerated during
   * matching.  This of course is slower, but index-format-hook is commonly
   * used for date ranges, and they need to be evaluated relative to "now", not
   * the hook compilation time.  */
  struct MailboxView *mv_cur = get_current_mailbox_view();
  struct PatternList *pat = mutt_pattern_comp(mv_cur, buf_string(pattern),
                                              MUTT_PC_FULL_MSG | MUTT_PC_PATTERN_DYNAMIC,
                                              err);
  if (!pat)
    goto out;

  hook = hook_new();
  hook->id = CMD_INDEX_FORMAT_HOOK;
  hook->command = NULL;
  hook->source_file = mutt_get_sourced_cwd();
  hook->pattern = pat;
  hook->regex.pattern = buf_strdup(pattern);
  hook->regex.regex = NULL;
  hook->regex.pat_not = pat_not;
  hook->expando = exp;
  exp = NULL;

  if (!hl)
  {
    hl = MUTT_MEM_CALLOC(1, struct HookList);
    TAILQ_INIT(hl);
    mutt_hash_insert(IdxFmtHooks, buf_string(name), hl);
  }

  TAILQ_INSERT_TAIL(hl, hook, entries);
  rc = MUTT_CMD_SUCCESS;

out:
  buf_pool_release(&name);
  buf_pool_release(&pattern);
  buf_pool_release(&fmt);
  expando_free(&exp);

  return rc;
}

/**
 * parse_unhook - Parse the unhook command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `unhook { * | <hook-type> }`
 */
enum CommandResult parse_unhook(const struct Command *cmd, struct Buffer *line,
                                struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_WARNING;

  while (MoreArgs(line))
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);
    if (mutt_str_equal("*", buf_string(token)))
    {
      if (CurrentHookId != TOKEN_NO_FLAGS)
      {
        buf_addstr(err, _("unhook: Can't do unhook * from within a hook"));
        goto done;
      }
      mutt_delete_hooks(CMD_NONE);
      delete_idxfmt_hooks();
      mutt_ch_lookup_remove();
    }
    else
    {
      const struct Command *hook = command_find_by_name(&NeoMutt->commands,
                                                        buf_string(token));
      if (!hook)
      {
        buf_printf(err, _("unhook: unknown hook type: %s"), buf_string(token));
        rc = MUTT_CMD_ERROR;
        goto done;
      }

      if ((hook->id == CMD_CHARSET_HOOK) || (hook->id == CMD_ICONV_HOOK))
      {
        mutt_ch_lookup_remove();
        rc = MUTT_CMD_SUCCESS;
        goto done;
      }
      if (CurrentHookId == hook->id)
      {
        buf_printf(err, _("unhook: Can't delete a %s from within a %s"),
                   buf_string(token), buf_string(token));
        rc = MUTT_CMD_WARNING;
        goto done;
      }
      if (hook->id == CMD_INDEX_FORMAT_HOOK)
        delete_idxfmt_hooks();
      else
        mutt_delete_hooks(hook->id);
    }
  }

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  return rc;
}

/**
 * mutt_folder_hook - Perform a folder hook
 * @param path Path to potentially match
 * @param desc Description to potentially match
 */
void mutt_folder_hook(const char *path, const char *desc)
{
  if (!path && !desc)
    return;

  struct Hook *hook = NULL;
  struct Buffer *err = buf_pool_get();

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
      if (parse_rc_line_cwd(hook->command, hook->source_file, err) == MUTT_CMD_ERROR)
      {
        mutt_error("%s", buf_string(err));
        break;
      }
    }
  }
  buf_pool_release(&err);

  CurrentHookId = CMD_NONE;
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
 * mutt_message_hook - Perform a message hook
 * @param m  Mailbox
 * @param e  Email
 * @param id Hook CommandId, e.g #CMD_FOLDER_HOOK
 */
void mutt_message_hook(struct Mailbox *m, struct Email *e, enum CommandId id)
{
  struct Hook *hook = NULL;
  struct PatternCache cache = { 0 };
  struct Buffer *err = buf_pool_get();

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
        if (parse_rc_line_cwd(hook->command, hook->source_file, err) == MUTT_CMD_ERROR)
        {
          mutt_error("%s", buf_string(err));
          CurrentHookId = CMD_NONE;
          buf_pool_release(&err);

          return;
        }
        /* Executing arbitrary commands could affect the pattern results,
         * so the cache has to be wiped */
        memset(&cache, 0, sizeof(cache));
      }
    }
  }
  buf_pool_release(&err);

  CurrentHookId = CMD_NONE;
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
    mutt_safe_path(tmp, addr);
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
      mutt_safe_path(buf, addr);
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

  buf_pretty_mailbox(path);
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
 * mutt_account_hook - Perform an account hook
 * @param url Account URL to match
 */
void mutt_account_hook(const char *url)
{
  /* parsing commands with URLs in an account hook can cause a recursive
   * call. We just skip processing if this occurs. Typically such commands
   * belong in a folder-hook -- perhaps we should warn the user. */
  static bool inhook = false;
  if (inhook)
    return;

  struct Hook *hook = NULL;
  struct Buffer *err = buf_pool_get();

  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if (!(hook->command && (hook->id == CMD_ACCOUNT_HOOK)))
      continue;

    if (mutt_regex_match(&hook->regex, url))
    {
      inhook = true;
      mutt_debug(LL_DEBUG1, "account-hook '%s' matches '%s'\n", hook->regex.pattern, url);
      mutt_debug(LL_DEBUG5, "    %s\n", hook->command);

      if (parse_rc_line_cwd(hook->command, hook->source_file, err) == MUTT_CMD_ERROR)
      {
        mutt_error("%s", buf_string(err));
        buf_pool_release(&err);

        inhook = false;
        goto done;
      }

      inhook = false;
    }
  }
done:
  buf_pool_release(&err);
}

/**
 * mutt_timeout_hook - Execute any timeout hooks
 *
 * The user can configure hooks to be run on timeout.
 * This function finds all the matching hooks and executes them.
 */
void mutt_timeout_hook(void)
{
  struct Hook *hook = NULL;
  struct Buffer *err = buf_pool_get();

  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if (!(hook->command && (hook->id == CMD_TIMEOUT_HOOK)))
      continue;

    if (parse_rc_line_cwd(hook->command, hook->source_file, err) == MUTT_CMD_ERROR)
    {
      mutt_error("%s", buf_string(err));
      buf_reset(err);

      /* The hooks should be independent of each other, so even though this on
       * failed, we'll carry on with the others. */
    }
  }
  buf_pool_release(&err);

  /* Delete temporary attachment files */
  mutt_temp_attachments_cleanup();
}

/**
 * mutt_startup_shutdown_hook - Execute any startup/shutdown hooks
 * @param id Hook CommandId: #CMD_STARTUP_HOOK or #CMD_SHUTDOWN_HOOK
 *
 * The user can configure hooks to be run on startup/shutdown.
 * This function finds all the matching hooks and executes them.
 */
void mutt_startup_shutdown_hook(enum CommandId id)
{
  struct Hook *hook = NULL;
  struct Buffer *err = buf_pool_get();

  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if (!(hook->command && (hook->id == id)))
      continue;

    if (parse_rc_line_cwd(hook->command, hook->source_file, err) == MUTT_CMD_ERROR)
    {
      mutt_error("%s", buf_string(err));
      buf_reset(err);
    }
  }
  buf_pool_release(&err);
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

/**
 * HookCommands - Hook Commands
 */
static const struct Command HookCommands[] = {
  // clang-format off
  { "account-hook", CMD_ACCOUNT_HOOK, parse_hook_regex, CMD_NO_DATA,
        N_("Run a command when switching to a matching account"),
        N_("account-hook <regex> <command>"),
        "optionalfeatures.html#account-hook" },
  { "charset-hook", CMD_CHARSET_HOOK, parse_hook_charset, CMD_NO_DATA,
        N_("Define charset alias for languages"),
        N_("charset-hook <alias> <charset>"),
        "configuration.html#charset-hook" },
  { "crypt-hook", CMD_CRYPT_HOOK, parse_hook_crypt, CMD_NO_DATA,
        N_("Specify which keyid to use for recipients matching regex"),
        N_("crypt-hook <regex> <keyid>"),
        "configuration.html#crypt-hook" },
  { "fcc-hook", CMD_FCC_HOOK, parse_hook_mailbox, CMD_NO_DATA,
        N_("Pattern rule to set the save location for outgoing mail"),
        N_("fcc-hook <pattern> <mailbox>"),
        "configuration.html#default-save-mailbox" },
  { "fcc-save-hook", CMD_FCC_SAVE_HOOK, parse_hook_mailbox, CMD_NO_DATA,
        N_("Equivalent to both `fcc-hook` and `save-hook`"),
        N_("fcc-save-hook <pattern> <mailbox>"),
        "configuration.html#default-save-mailbox" },
  { "folder-hook", CMD_FOLDER_HOOK, parse_hook_folder, CMD_NO_DATA,
        N_("Run a command upon entering a folder matching regex"),
        N_("folder-hook [ -noregex ] <regex> <command>"),
        "configuration.html#folder-hook" },
  { "iconv-hook", CMD_ICONV_HOOK, parse_hook_charset, CMD_NO_DATA,
        N_("Define a system-specific alias for a character set"),
        N_("iconv-hook <charset> <local-charset>"),
        "configuration.html#charset-hook" },
  { "index-format-hook", CMD_INDEX_FORMAT_HOOK, parse_hook_index, CMD_NO_DATA,
        N_("Create dynamic index format strings"),
        N_("index-format-hook <name> [ ! ]<pattern> <format-string>"),
        "configuration.html#index-format-hook" },
  { "mbox-hook", CMD_MBOX_HOOK, parse_hook_mbox, CMD_NO_DATA,
        N_("On leaving a mailbox, move read messages matching a regex regex"),
        N_("mbox-hook [ -noregex ] <regex> <mailbox>"),
        "configuration.html#mbox-hook" },
  { "message-hook", CMD_MESSAGE_HOOK, parse_hook_pattern, CMD_NO_DATA,
        N_("Run a command when viewing a message matching patterns"),
        N_("message-hook <pattern> <command>"),
        "configuration.html#message-hook" },
  { "reply-hook", CMD_REPLY_HOOK, parse_hook_pattern, CMD_NO_DATA,
        N_("Run a command when replying to messages matching a pattern"),
        N_("reply-hook <pattern> <command>"),
        "configuration.html#send-hook" },
  { "save-hook", CMD_SAVE_HOOK, parse_hook_mailbox, CMD_NO_DATA,
        N_("Set default save folder for messages"),
        N_("save-hook <pattern> <mailbox>"),
        "configuration.html#default-save-mailbox" },
  { "send-hook", CMD_SEND_HOOK, parse_hook_pattern, CMD_NO_DATA,
        N_("Run a command when sending a message, new or reply, matching a pattern"),
        N_("send-hook <pattern> <command>"),
        "configuration.html#send-hook" },
  { "send2-hook", CMD_SEND2_HOOK, parse_hook_pattern, CMD_NO_DATA,
        N_("Run command whenever a composed message is edited"),
        N_("send2-hook <pattern> <command>"),
        "configuration.html#send-hook" },
  { "shutdown-hook", CMD_SHUTDOWN_HOOK, parse_hook_global, CMD_NO_DATA,
        N_("Run a command before NeoMutt exits"),
        N_("shutdown-hook <command>"),
        "optionalfeatures.html#global-hooks" },
  { "startup-hook", CMD_STARTUP_HOOK, parse_hook_global, CMD_NO_DATA,
        N_("Run a command when NeoMutt starts up"),
        N_("startup-hook <command>"),
        "optionalfeatures.html#global-hooks" },
  { "timeout-hook", CMD_TIMEOUT_HOOK, parse_hook_global, CMD_NO_DATA,
        N_("Run a command after a specified timeout or idle period"),
        N_("timeout-hook <command>"),
        "optionalfeatures.html#global-hooks" },
  { "unhook", CMD_UNHOOK, parse_unhook, CMD_NO_DATA,
        N_("Remove hooks of a given type"),
        N_("unhook { * | <hook-type> }"),
        "configuration.html#unhook" },

  // Deprecated
  { "pgp-hook", CMD_NONE, parse_hook_crypt, CMD_CRYPT_HOOK, NULL, NULL, NULL, CF_SYNONYM },

  { NULL, CMD_NONE, NULL, CMD_NO_DATA, NULL, NULL, NULL, CF_NO_FLAGS },
  // clang-format on
};

/**
 * hooks_init - Setup feature commands
 */
void hooks_init(void)
{
  commands_register(&NeoMutt->commands, HookCommands);
}
