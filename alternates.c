/**
 * @file
 * Alternate address handling
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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
 * @page neo_alternates Alternate address handling
 *
 * Alternate address handling
 */

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "alternates.h"
#include "commands.h"
#include "init.h"

struct RegexList Alternates = STAILQ_HEAD_INITIALIZER(Alternates); ///< List of regexes to match the user's alternate email addresses
struct RegexList UnAlternates = STAILQ_HEAD_INITIALIZER(UnAlternates); ///< List of regexes to exclude false matches in Alternates
static struct Notify *AlternatesNotify = NULL;

/**
 * alternates_free - Free the alternates lists
 */
void alternates_free(void)
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
 */
void mutt_alternates_reset(struct Mailbox *m)
{
  if (!m)
    return;

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
 */
enum CommandResult parse_alternates(struct Buffer *buf, struct Buffer *s,
                                    intptr_t data, struct Buffer *err)
{
  struct GroupList gl = STAILQ_HEAD_INITIALIZER(gl);

  do
  {
    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

    if (parse_grouplist(&gl, buf, s, err) == -1)
      goto bail;

    mutt_regexlist_remove(&UnAlternates, buf->data);

    if (mutt_regexlist_add(&Alternates, buf->data, REG_ICASE, err) != 0)
      goto bail;

    if (mutt_grouplist_add_regex(&gl, buf->data, REG_ICASE, err) != 0)
      goto bail;
  } while (MoreArgs(s));

  mutt_grouplist_destroy(&gl);

  mutt_debug(LL_NOTIFY, "NT_ALTERN_ADD: %s\n", buf->data);
  notify_send(AlternatesNotify, NT_ALTERN, NT_ALTERN_ADD, NULL);

  return MUTT_CMD_SUCCESS;

bail:
  mutt_grouplist_destroy(&gl);
  return MUTT_CMD_ERROR;
}

/**
 * parse_unalternates - Parse the 'unalternates' command - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult parse_unalternates(struct Buffer *buf, struct Buffer *s,
                                      intptr_t data, struct Buffer *err)
{
  do
  {
    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
    mutt_regexlist_remove(&Alternates, buf->data);

    if (!mutt_str_equal(buf->data, "*") &&
        (mutt_regexlist_add(&UnAlternates, buf->data, REG_ICASE, err) != 0))
    {
      return MUTT_CMD_ERROR;
    }

  } while (MoreArgs(s));

  mutt_debug(LL_NOTIFY, "NT_ALTERN_DELETE: %s\n", buf->data);
  notify_send(AlternatesNotify, NT_ALTERN, NT_ALTERN_DELETE, NULL);

  return MUTT_CMD_SUCCESS;
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
