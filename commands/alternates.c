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
 * @page commands_alternates Parse Alternate Commands
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
#include "alternates.h"
#include "parse/lib.h"
#include "group.h"
#include "mview.h"

static struct RegexList Alternates = STAILQ_HEAD_INITIALIZER(Alternates); ///< List of regexes to match the user's alternate email addresses
static struct RegexList UnAlternates = STAILQ_HEAD_INITIALIZER(UnAlternates); ///< List of regexes to exclude false matches in Alternates
static struct Notify *AlternatesNotify = NULL; ///< Notifications: #NotifyAlternates

/**
 * alternates_cleanup - Free the alternates lists
 */
void alternates_cleanup(void)
{
  notify_free(&AlternatesNotify);

  mutt_regexlist_free(&Alternates);
  mutt_regexlist_free(&UnAlternates);
}

/**
 * alternates_init - Set up the alternates lists
 */
void alternates_init(void)
{
  if (AlternatesNotify)
    return;

  AlternatesNotify = notify_new();
  notify_set_parent(AlternatesNotify, NeoMutt->notify);
}

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
enum CommandResult parse_alternates(const struct Command *cmd,
                                    struct Buffer *line, struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct GroupList gl = STAILQ_HEAD_INITIALIZER(gl);
  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  do
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);

    if (parse_grouplist(&gl, token, line, err, NeoMutt->groups) == -1)
      goto done;

    mutt_regexlist_remove(&UnAlternates, buf_string(token));

    if (mutt_regexlist_add(&Alternates, buf_string(token), REG_ICASE, err) != 0)
      goto done;

    if (grouplist_add_regex(&gl, buf_string(token), REG_ICASE, err) != 0)
      goto done;
  } while (MoreArgs(line));

  mutt_debug(LL_NOTIFY, "NT_ALTERN_ADD: %s\n", buf_string(token));
  notify_send(AlternatesNotify, NT_ALTERN, NT_ALTERN_ADD, NULL);

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
 * - `unalternates [ -group <name> ... ] { * | <regex> ... }`
 */
enum CommandResult parse_unalternates(const struct Command *cmd,
                                      struct Buffer *line, struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  do
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);
    mutt_regexlist_remove(&Alternates, buf_string(token));

    if (!mutt_str_equal(buf_string(token), "*") &&
        (mutt_regexlist_add(&UnAlternates, buf_string(token), REG_ICASE, err) != 0))
    {
      goto done;
    }

  } while (MoreArgs(line));

  mutt_debug(LL_NOTIFY, "NT_ALTERN_DELETE: %s\n", buf_string(token));
  notify_send(AlternatesNotify, NT_ALTERN, NT_ALTERN_DELETE, NULL);

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  return rc;
}

/**
 * mutt_alternates_match - Compare an Address to the Un/Alternates lists
 * @param addr Address to check
 * @retval true Address matches
 */
bool mutt_alternates_match(const char *addr)
{
  if (!addr)
    return false;

  if (mutt_regexlist_match(&Alternates, addr))
  {
    mutt_debug(LL_DEBUG5, "yes, %s matched by alternates\n", addr);
    if (mutt_regexlist_match(&UnAlternates, addr))
      mutt_debug(LL_DEBUG5, "but, %s matched by unalternates\n", addr);
    else
      return true;
  }

  return false;
}
