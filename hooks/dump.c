/**
 * @file
 * Dump the Hooks to the Pager
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
 * @page hooks_dump Dump the Hooks to the Pager
 *
 * Dump the Hooks to the Pager
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "commands/lib.h"
#include "expando/lib.h"
#include "pager/lib.h"
#include "parse/lib.h"
#include "hook.h"
#include "muttlib.h"
#include "parse.h"

/**
 * hooks_dump_one - Dump a single hook to the buffer
 * @param hook Hook to dump
 * @param cmd  Command defining the Hook
 * @param buf  Buffer for the results
 */
static void hooks_dump_one(struct Hook *hook, const struct Command *cmd, struct Buffer *buf)
{
  struct Buffer *pretty = buf_pool_get();

  buf_add_printf(buf, "%s ", cmd->name);

  if (hook->regex.pattern)
  {
    if (hook->regex.pat_not)
      buf_addch(pretty, '!');

    if ((hook->id == CMD_FOLDER_HOOK) || (hook->id == CMD_MBOX_HOOK))
    {
      buf_strcpy(pretty, hook->regex.pattern);
      pretty_mailbox(pretty);
      buf_add_printf(buf, "\"%s\" ", buf_string(pretty));
    }
    else
    {
      buf_addstr(pretty, hook->regex.pattern);
      pretty_var(buf_string(pretty), buf);
      buf_addch(buf, ' ');
    }
  }

  if ((hook->id == CMD_FCC_HOOK) || (hook->id == CMD_MBOX_HOOK) || (hook->id == CMD_SAVE_HOOK))
  {
    buf_strcpy(pretty, hook->command);
    pretty_mailbox(pretty);
    buf_add_printf(buf, "\"%s\"\n", buf_string(pretty));
  }
  else
  {
    pretty_var(hook->command, buf);
    buf_addch(buf, '\n');
  }

  buf_pool_release(&pretty);
}

/**
 * hooks_dump_simple - Dump the simple Hooks
 * @param buf Buffer for the results
 */
static void hooks_dump_simple(struct Buffer *buf)
{
  // Dump all the Hooks, except: CMD_CHARSET_HOOK, CMD_ICONV_HOOK, CMD_INDEX_FORMAT_HOOK
  static const enum CommandId hook_ids[] = {
    CMD_ACCOUNT_HOOK, CMD_APPEND_HOOK,   CMD_CLOSE_HOOK,   CMD_CRYPT_HOOK,
    CMD_FCC_HOOK,     CMD_FOLDER_HOOK,   CMD_MBOX_HOOK,    CMD_MESSAGE_HOOK,
    CMD_OPEN_HOOK,    CMD_REPLY_HOOK,    CMD_SAVE_HOOK,    CMD_SEND_HOOK,
    CMD_SEND2_HOOK,   CMD_SHUTDOWN_HOOK, CMD_STARTUP_HOOK, CMD_TIMEOUT_HOOK,
  };

  for (size_t i = 0; i < countof(hook_ids); i++)
  {
    const struct Command *hook_cmd = command_find_by_id(&NeoMutt->commands, hook_ids[i]);
    enum CommandId id = hook_ids[i];
    bool found_header = false;
    struct Hook *hook = NULL;

    TAILQ_FOREACH(hook, &Hooks, entries)
    {
      if (hook->id != id)
        continue;

      if (!found_header)
      {
        buf_add_printf(buf, "# %s\n", hook_cmd->help);
        found_header = true;
      }

      hooks_dump_one(hook, hook_cmd, buf);
    }

    if (found_header)
      buf_addstr(buf, "\n");
  }
}

/**
 * hooks_dump_index - Dump the Index Format Hooks
 * @param buf Buffer for the results
 */
static void hooks_dump_index(struct Buffer *buf)
{
  if (!IdxFmtHooks)
    return;

  struct HashWalkState hws = { 0 };
  struct HashElem *he = NULL;
  bool found_header = false;

  while ((he = mutt_hash_walk(IdxFmtHooks, &hws)))
  {
    const char *name = he->key.strkey;
    struct HookList *hl = he->data;
    struct Hook *hook = NULL;

    TAILQ_FOREACH(hook, hl, entries)
    {
      if (!found_header)
      {
        const struct Command *hook_cmd = command_find_by_id(&NeoMutt->commands,
                                                            CMD_INDEX_FORMAT_HOOK);
        buf_add_printf(buf, "# %s\n", hook_cmd->help);
        found_header = true;
      }

      buf_add_printf(buf, "index-format-hook '%s' %s'%s' '%s'\n", name,
                     hook->regex.pat_not ? "! " : "",
                     NONULL(hook->regex.pattern), NONULL(hook->expando->string));
    }
  }

  if (found_header)
    buf_addstr(buf, "\n");
}

/**
 * hooks_dump_charset - Dump the Charset Hooks
 * @param buf Buffer for the results
 */
static void hooks_dump_charset(struct Buffer *buf)
{
  struct Lookup *l = NULL;
  struct Buffer *charset = buf_pool_get();
  struct Buffer *iconv = buf_pool_get();

  TAILQ_FOREACH(l, &Lookups, entries)
  {
    if (l->type == MUTT_LOOKUP_CHARSET)
    {
      buf_addstr(charset, "charset-hook ");
      pretty_var(l->regex.pattern, charset);
      buf_addch(charset, ' ');
      pretty_var(l->replacement, charset);
      buf_addch(charset, '\n');
    }
    else if (l->type == MUTT_LOOKUP_ICONV)
    {
      buf_addstr(iconv, "iconv-hook ");
      pretty_var(l->regex.pattern, iconv);
      buf_addch(iconv, ' ');
      pretty_var(l->replacement, iconv);
      buf_addch(iconv, '\n');
    }
  }

  const struct Command *cmd = NULL;
  if (!buf_is_empty(charset))
  {
    cmd = command_find_by_name(&NeoMutt->commands, "charset-hook");
    buf_add_printf(buf, "# %s\n", cmd->help);
    buf_add_printf(buf, "%s\n", buf_string(charset));
  }

  if (!buf_is_empty(iconv))
  {
    cmd = command_find_by_name(&NeoMutt->commands, "iconv-hook");
    buf_add_printf(buf, "# %s\n", cmd->help);
    buf_add_printf(buf, "%s\n", buf_string(iconv));
  }

  buf_pool_release(&iconv);
  buf_pool_release(&charset);
}

/**
 * parse_hooks - Parse the 'hooks' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `hooks`
 */
enum CommandResult parse_hooks(const struct Command *cmd, struct Buffer *line,
                               const struct ParseContext *pc, struct ParseError *pe)
{
  struct Buffer *err = pe->message;

  if (MoreArgs(line))
  {
    buf_printf(err, _("%s: too many arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  if (!StartupComplete)
    return MUTT_CMD_SUCCESS;

  if (TAILQ_EMPTY(&Hooks))
  {
    buf_printf(err, _("%s: No Hooks are configured"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *tempfile = buf_pool_get();
  buf_mktemp(tempfile);

  FILE *fp = mutt_file_fopen(buf_string(tempfile), "w");
  if (!fp)
  {
    mutt_error(_("Could not create temporary file %s"), buf_string(tempfile));
    buf_pool_release(&tempfile);
    return MUTT_CMD_ERROR;
  }

  struct Buffer *buf = buf_pool_get();

  hooks_dump_simple(buf);
  hooks_dump_index(buf);
  hooks_dump_charset(buf);

  mutt_file_save_str(fp, buf_string(buf));
  mutt_file_fclose(&fp);
  buf_pool_release(&buf);

  struct PagerData pdata = { 0 };
  struct PagerView pview = { &pdata };

  pdata.fname = buf_string(tempfile);

  pview.banner = "hooks";
  pview.flags = MUTT_PAGER_NO_FLAGS;
  pview.mode = PAGER_MODE_OTHER;

  mutt_do_pager(&pview, NULL);
  buf_pool_release(&tempfile);
  return MUTT_CMD_SUCCESS;
}
