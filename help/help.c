/**
 * @file
 * Help system
 *
 * @authors
 * Copyright (C) 2018-2019 Richard Russon <rich@flatcap.org>
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

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include "mutt/mutt.h"
#include "config/lib.h"
#include "core/lib.h"
#include "mx.h"

struct Context;
struct Header;
struct Message;

/**
 * help_mbox_open - Open a Mailbox -- Implements MxOps::mbox_open
 */
static int help_mbox_open(struct Mailbox *m)
{
  mutt_debug(1, "entering help_mbox_open\n");
  return -1;
}

/**
 * help_ac_find - Find an Account that matches a Mailbox path -- Implements MxOps::ac_find
 */
static struct Account *help_ac_find(struct Account *a, const char *path)
{
  if (!a || !path)
    return NULL;

  return a;
}

/**
 * help_ac_add - Add a Mailbox to an Account -- Implements MxOps::ac_add
 */
static int help_ac_add(struct Account *a, struct Mailbox *m)
{
  if (!a || !m)
    return -1;

  if (m->magic != MUTT_HELP)
    return -1;

  m->account = a;

  struct MailboxNode *np = mutt_mem_calloc(1, sizeof(*np));
  np->mailbox = m;
  STAILQ_INSERT_TAIL(&a->mailboxes, np, entries);
  return 0;
}

/**
 * help_mbox_open_append - Open a Mailbox for appending -- Implements MxOps::mbox_open_append
 */
static int help_mbox_open_append(struct Mailbox *m, OpenMailboxFlags flags)
{
  mutt_debug(1, "entering help_mbox_open_append\n");
  return -1;
}

/**
 * help_mbox_check - Check for new mail -- Implements MxOps::mbox_check
 */
static int help_mbox_check(struct Mailbox *m, int *index_hint)
{
  mutt_debug(1, "entering help_mbox_check\n");
  return -1;
}

/**
 * help_mbox_sync - Save changes to the Mailbox -- Implements MxOps::mbox_sync
 */
static int help_mbox_sync(struct Mailbox *m, int *index_hint)
{
  mutt_debug(1, "entering help_mbox_sync\n");
  return -1;
}

/**
 * help_mbox_close - Close a Mailbox -- Implements MxOps::mbox_close
 */
static int help_mbox_close(struct Mailbox *m)
{
  mutt_debug(1, "entering help_mbox_close\n");
  return -1;
}

/**
 * help_msg_open - Open an email message in a Mailbox -- Implements MxOps::msg_open
 */
static int help_msg_open(struct Mailbox *m, struct Message *msg, int msgno)
{
  mutt_debug(1, "entering help_msg_open\n");
  return -1;
}

/**
 * help_msg_open_new - Open a new message in a Mailbox -- Implements MxOps::msg_open_new
 */
static int help_msg_open_new(struct Mailbox *m, struct Message *msg, struct Email *e)
{
  mutt_debug(1, "entering help_msg_open_new\n");
  return -1;
}

/**
 * help_msg_commit - Save changes to an email -- Implements MxOps::msg_commit
 */
static int help_msg_commit(struct Mailbox *m, struct Message *msg)
{
  mutt_debug(1, "entering help_msg_commit\n");
  return -1;
}

/**
 * help_msg_close - Close an email -- Implements MxOps::msg_close
 */
static int help_msg_close(struct Mailbox *m, struct Message *msg)
{
  mutt_debug(1, "entering help_msg_close\n");
  return -1;
}

/**
 * help_msg_padding_size - Bytes of padding between messages -- Implements MxOps::msg_padding_size
 */
static int help_msg_padding_size(struct Mailbox *m)
{
  mutt_debug(1, "entering help_msg_padding_size\n");
  return -1;
}

/**
 * help_tags_edit - Prompt and validate new messages tags -- Implements MxOps::tags_edit
 */
static int help_tags_edit(struct Mailbox *m, const char *tags, char *buf, size_t buflen)
{
  mutt_debug(1, "entering help_tags_edit\n");
  return -1;
}

/**
 * help_tags_commit - Save the tags to a message -- Implements MxOps::tags_commit
 */
static int help_tags_commit(struct Mailbox *m, struct Email *e, char *buf)
{
  mutt_debug(1, "entering help_tags_commit\n");
  return -1;
}

/**
 * help_path_probe - Is this a Help Mailbox? -- Implements MxOps::path_probe
 */
static enum MailboxType help_path_probe(const char *path, const struct stat *st)
{
  if (!path)
    return MUTT_UNKNOWN;

  if (mutt_str_strncasecmp(path, "help://", 7) == 0)
    return MUTT_HELP;

  return MUTT_UNKNOWN;
}

/**
 * help_path_canon - Canonicalise a Mailbox path -- Implements MxOps::path_canon
 */
static int help_path_canon(char *buf, size_t buflen)
{
  mutt_debug(1, "entering help_path_canon\n");
  return 0;
}

/**
 * help_path_pretty - Abbreviate a Mailbox path -- Implements MxOps::path_pretty
 */
static int help_path_pretty(char *buf, size_t buflen, const char *folder)
{
  mutt_debug(1, "entering help_path_pretty\n");
  return -1;
}

/**
 * help_path_parent - Find the parent of a Mailbox path -- Implements MxOps::path_parent
 */
static int help_path_parent(char *buf, size_t buflen)
{
  mutt_debug(1, "entering help_path_parent\n");
  return -1;
}

// clang-format off
/**
 * MxHelpOps - Help Mailbox - Implements ::MxOps
 */
struct MxOps MxHelpOps = {
  .magic            = MUTT_HELP,
  .name             = "help",
  .ac_find          = help_ac_find,
  .ac_add           = help_ac_add,
  .mbox_open        = help_mbox_open,
  .mbox_open_append = help_mbox_open_append,
  .mbox_check       = help_mbox_check,
  .mbox_sync        = help_mbox_sync,
  .mbox_close       = help_mbox_close,
  .msg_open         = help_msg_open,
  .msg_open_new     = help_msg_open_new,
  .msg_commit       = help_msg_commit,
  .msg_close        = help_msg_close,
  .msg_padding_size = help_msg_padding_size,
  .tags_edit        = help_tags_edit,
  .tags_commit      = help_tags_commit,
  .path_probe       = help_path_probe,
  .path_canon       = help_path_canon,
  .path_pretty      = help_path_pretty,
  .path_parent      = help_path_parent,
};
// clang-format on
