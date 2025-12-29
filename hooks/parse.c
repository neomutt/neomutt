/**
 * @file
 * Parse user-defined Hooks
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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
 * @page hooks_parse Parse user-defined Hooks
 *
 * Parse user-defined Hooks
 */

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "commands/lib.h"
#include "compmbox/lib.h"
#include "expando/lib.h"
#include "index/lib.h"
#include "parse/lib.h"
#include "pattern/lib.h"
#include "globals.h"
#include "hook.h"
#include "muttlib.h"

extern const struct ExpandoDefinition IndexFormatDef[];

/// All simple hooks, e.g. CMD_FOLDER_HOOK
struct HookList Hooks = TAILQ_HEAD_INITIALIZER(Hooks);

/// All Index Format hooks
struct HashTable *IdxFmtHooks = NULL;

/// The ID of the Hook currently being executed, e.g. #CMD_SAVE_HOOK
enum CommandId CurrentHookId = CMD_NONE;

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
