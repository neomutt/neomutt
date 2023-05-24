/**
 * @file
 * IMAP Message Sets
 *
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
 * @page imap_msg_set IMAP Message Sets
 *
 * Manage IMAP message sets, ordered compressed lists of Email UIDs.
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "msg_set.h"
#include "adata.h"
#include "edata.h"
#include "limits.h"

int imap_sort_email_uid(const void *a, const void *b);

/// Maximum length of IMAP commands before they must be split
#define IMAP_MAX_CMDLEN 1024

/**
 * imap_make_msg_set - Make a message set
 * @param[in]  m       Selected Imap Mailbox
 * @param[in]  buf     Buffer to store message set
 * @param[in]  flag    Flags to match, e.g. #MUTT_DELETED
 * @param[in]  changed Matched messages that have been altered
 * @param[in]  invert  Flag matches should be inverted
 * @param[out] pos     Cursor used for multiple calls to this function
 * @retval num Messages in the set
 *
 * @note Headers must be in #SORT_ORDER. See imap_exec_msg_set() for args.
 * Pos is an opaque pointer a la strtok(). It should be 0 at first call.
 */
int imap_make_msg_set(struct Mailbox *m, struct Buffer *buf,
                      enum MessageType flag, bool changed, bool invert, int *pos)
{
  int count = 0;             /* number of messages in message set */
  unsigned int setstart = 0; /* start of current message range */
  int n;
  bool started = false;

  struct ImapAccountData *adata = imap_adata_get(m);
  if (!adata || (adata->mailbox != m))
    return -1;

  for (n = *pos; (n < m->msg_count) && (buf_len(buf) < IMAP_MAX_CMDLEN); n++)
  {
    struct Email *e = m->emails[n];
    if (!e)
      break;
    bool match = false; /* whether current message matches flag condition */
    /* don't include pending expunged messages.
     *
     * TODO: can we unset active in cmd_parse_expunge() and
     * cmd_parse_vanished() instead of checking for index != INT_MAX. */
    if (e->active && (e->index != INT_MAX))
    {
      switch (flag)
      {
        case MUTT_DELETED:
          if (e->deleted != imap_edata_get(e)->deleted)
            match = invert ^ e->deleted;
          break;
        case MUTT_FLAG:
          if (e->flagged != imap_edata_get(e)->flagged)
            match = invert ^ e->flagged;
          break;
        case MUTT_OLD:
          if (e->old != imap_edata_get(e)->old)
            match = invert ^ e->old;
          break;
        case MUTT_READ:
          if (e->read != imap_edata_get(e)->read)
            match = invert ^ e->read;
          break;
        case MUTT_REPLIED:
          if (e->replied != imap_edata_get(e)->replied)
            match = invert ^ e->replied;
          break;
        case MUTT_TAG:
          if (e->tagged)
            match = true;
          break;
        case MUTT_TRASH:
          if (e->deleted && !e->purge)
            match = true;
          break;
        default:
          break;
      }
    }

    if (match && (!changed || e->changed))
    {
      count++;
      if (setstart == 0)
      {
        setstart = imap_edata_get(e)->uid;
        if (started)
        {
          buf_add_printf(buf, ",%u", imap_edata_get(e)->uid);
        }
        else
        {
          buf_add_printf(buf, "%u", imap_edata_get(e)->uid);
          started = true;
        }
      }
      else if (n == (m->msg_count - 1))
      {
        /* tie up if the last message also matches */
        buf_add_printf(buf, ":%u", imap_edata_get(e)->uid);
      }
    }
    else if (setstart)
    {
      /* End current set if message doesn't match. */
      if (imap_edata_get(m->emails[n - 1])->uid > setstart)
        buf_add_printf(buf, ":%u", imap_edata_get(m->emails[n - 1])->uid);
      setstart = 0;
    }
  }

  *pos = n;

  return count;
}

/**
 * imap_exec_msg_set - Prepare commands for all messages matching conditions
 * @param m       Selected Imap Mailbox
 * @param pre     prefix commands
 * @param post    postfix commands
 * @param flag    flag type on which to filter, e.g. #MUTT_REPLIED
 * @param changed include only changed messages in message set
 * @param invert  invert sense of flag, eg #MUTT_READ matches unread messages
 * @retval num Matched messages
 * @retval -1  Failure
 *
 * pre/post: commands are of the form "%s %s %s %s", tag, pre, message set, post
 * Prepares commands for all messages matching conditions
 * (must be flushed with imap_exec)
 */
int imap_exec_msg_set(struct Mailbox *m, const char *pre, const char *post,
                     enum MessageType flag, bool changed, bool invert)
{
  struct ImapAccountData *adata = imap_adata_get(m);
  if (!adata || (adata->mailbox != m))
    return -1;

  struct Email **emails = NULL;
  int pos;
  int rc;
  int count = 0;

  struct Buffer cmd = buf_make(0);

  /* We make a copy of the headers just in case resorting doesn't give
   exactly the original order (duplicate messages?), because other parts of
   the mv are tied to the header order. This may be overkill. */
  const enum SortType c_sort = cs_subset_sort(NeoMutt->sub, "sort");
  if (c_sort != SORT_ORDER)
  {
    emails = m->emails;
    if (m->msg_count != 0)
    {
      // We overcommit here, just in case new mail arrives whilst we're sync-ing
      m->emails = mutt_mem_malloc(m->email_max * sizeof(struct Email *));
      memcpy(m->emails, emails, m->email_max * sizeof(struct Email *));

      cs_subset_str_native_set(NeoMutt->sub, "sort", SORT_ORDER, NULL);
      qsort(m->emails, m->msg_count, sizeof(struct Email *), imap_sort_email_uid);
    }
  }

  pos = 0;

  do
  {
    buf_reset(&cmd);
    buf_add_printf(&cmd, "%s ", pre);
    rc = imap_make_msg_set(m, &cmd, flag, changed, invert, &pos);
    if (rc > 0)
    {
      buf_add_printf(&cmd, " %s", post);
      if (imap_exec(adata, cmd.data, IMAP_CMD_QUEUE) != IMAP_EXEC_SUCCESS)
      {
        rc = -1;
        goto out;
      }
      count += rc;
    }
  } while (rc > 0);

  rc = count;

out:
  buf_dealloc(&cmd);
  if (c_sort != SORT_ORDER)
  {
    cs_subset_str_native_set(NeoMutt->sub, "sort", c_sort, NULL);
    FREE(&m->emails);
    m->emails = emails;
  }

  return rc;
}
