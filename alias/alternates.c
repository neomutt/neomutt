/**
 * @file
 * Parse Alternate Commands
 *
 * @authors
 * Copyright (C) 2021-2025 Richard Russon <rich@flatcap.org>
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
 * @page alias_alternates Parse Alternate Commands
 *
 * Parse Alternate Commands
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "alternates.h"
#include "parse/lib.h"
#include "module_data.h"

/**
 * mutt_alternates_reset - Clear the recipient valid flag of all emails
 * @param mv Mailbox view
 */
void mutt_alternates_reset(struct MailboxView *mv)
{
  if (!mv || !mv->mailbox)
    return;

  struct Mailbox *m = mv->mailbox;

  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e)
      break;
    e->recip_valid = false;
  }
}

/**
 * parse_alternates - Parse the 'alternates' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `alternates [ -group <name> ... ] <regex> [ <regex> ... ]`
 */
enum CommandResult parse_alternates(const struct Command *cmd, struct Buffer *line,
                                    const struct ParseContext *pc, struct ParseError *pe)
{
  struct Buffer *err = pe->message;

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct GroupList gl = STAILQ_HEAD_INITIALIZER(gl);
  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  struct AliasModuleData *md = neomutt_get_module_data(NeoMutt, MODULE_ID_ALIAS);
  ASSERT(md);

  do
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);

    if (parse_grouplist(&gl, token, line, err, NeoMutt->groups) == -1)
      goto done;

    mutt_regexlist_remove(&md->unalternates, buf_string(token));

    if (mutt_regexlist_add(&md->alternates, buf_string(token), REG_ICASE, err) != 0)
      goto done;

    if (grouplist_add_regex(&gl, buf_string(token), REG_ICASE, err) != 0)
      goto done;
  } while (MoreArgs(line));

  mutt_debug(LL_NOTIFY, "NT_ALTERN_ADD: %s\n", buf_string(token));
  notify_send(md->alternates_notify, NT_ALTERN, NT_ALTERN_ADD, NULL);

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  grouplist_destroy(&gl);
  return rc;
}

/**
 * parse_unalternates - Parse the 'unalternates' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `unalternates { * | <regex> ... }`
 */
enum CommandResult parse_unalternates(const struct Command *cmd, struct Buffer *line,
                                      const struct ParseContext *pc, struct ParseError *pe)
{
  struct Buffer *err = pe->message;

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  struct AliasModuleData *md = neomutt_get_module_data(NeoMutt, MODULE_ID_ALIAS);
  ASSERT(md);

  do
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);
    mutt_regexlist_remove(&md->alternates, buf_string(token));

    if (!mutt_str_equal(buf_string(token), "*") &&
        (mutt_regexlist_add(&md->unalternates, buf_string(token), REG_ICASE, err) != 0))
    {
      goto done;
    }

  } while (MoreArgs(line));

  mutt_debug(LL_NOTIFY, "NT_ALTERN_DELETE: %s\n", buf_string(token));
  notify_send(md->alternates_notify, NT_ALTERN, NT_ALTERN_DELETE, NULL);

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  return rc;
}

/**
 * mutt_alternates_match - Compare an Address to the un/alternates lists
 * @param addr Address to check
 * @retval true Address matches
 */
bool mutt_alternates_match(const char *addr)
{
  if (!addr)
    return false;

  struct AliasModuleData *md = neomutt_get_module_data(NeoMutt, MODULE_ID_ALIAS);
  ASSERT(md);

  if (mutt_regexlist_match(&md->alternates, addr))
  {
    mutt_debug(LL_DEBUG5, "yes, %s matched by alternates\n", addr);
    if (mutt_regexlist_match(&md->unalternates, addr))
      mutt_debug(LL_DEBUG5, "but, %s matched by unalternates\n", addr);
    else
      return true;
  }

  return false;
}
