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
 * Manage IMAP message sets: Lists of Email UIDs, ordered and compressed.
 *
 * Every Email on an IMAP server has a unique id (UID).
 *
 * When NeoMutt can COPY, FETCH, SEARCH or STORE Emails using these UIDs.
 * To save bandwidth, lists of UIDs can be abbreviated.  Ranges are shortened
 * to 'start:end'.
 *
 * e.g. `1,2,3,4,6,8,9,10` becomes `1:4,6,8:10`
 */

#include "config.h"
#include <stdio.h>
#include "private.h"
#include "mutt/lib.h"
#include "msg_set.h"
#include "sort.h"

/**
 * ImapMaxCmdlen - Maximum length of IMAP commands before they must be split
 *
 * This is suggested in RFC7162 (dated 2014).
 * - https://datatracker.ietf.org/doc/html/rfc7162#section-4
 */
int ImapMaxCmdlen = 8192;

/**
 * imap_sort_uid - Compare two UIDs - Implements ::sort_t - @ingroup sort_api
 */
int imap_sort_uid(const void *a, const void *b, void *sdata)
{
  unsigned int ua = *(unsigned int *) a;
  unsigned int ub = *(unsigned int *) b;

  return mutt_numeric_cmp(ua, ub);
}

/**
 * imap_make_msg_set - Generate a compressed message set of UIDs
 * @param uida  Array of UIDs
 * @param buf   Buffer for message set
 * @param pos   Cursor used for multiple calls to this function
 * @retval num Number of UIDs processed
 *
 * Compress a sorted list of UIDs, e.g.
 * - `1,2,3,4,6,8,9,10` becomes `1:4,6,8:10`
 */
int imap_make_msg_set(struct UidArray *uida, struct Buffer *buf, int *pos)
{
  if (!uida || !buf || !pos)
    return 0;

  const size_t array_size = ARRAY_SIZE(uida);
  if ((array_size == 0) || (*pos >= array_size))
    return 0;

  int count = 1; // Number of UIDs added to the set
  size_t i = *pos;
  unsigned int start = *ARRAY_GET(uida, i);
  unsigned int prev = start;

  for (i++; (i < array_size) && (buf_len(buf) < ImapMaxCmdlen); i++, count++)
  {
    unsigned int uid = *ARRAY_GET(uida, i);

    // Keep adding to current set
    if (uid == (prev + 1))
    {
      prev = uid;
      continue;
    }

    // End the current set
    if (start == prev)
      buf_add_printf(buf, "%u,", start);
    else
      buf_add_printf(buf, "%u:%u,", start, prev);

    // Start a new set
    start = uid;
    prev = uid;
  }

  if (start == prev)
    buf_add_printf(buf, "%u", start);
  else
    buf_add_printf(buf, "%u:%u", start, prev);

  *pos = i;

  return count;
}

/**
 * imap_exec_msg_set - Execute a command using a set of UIDs
 * @param adata Imap Account data
 * @param pre   Prefix commands
 * @param post  Postfix commands
 * @param uida  Sorted array of UIDs
 * @retval num Number of messages sent
 * @retval  -1 Error
 *
 * Commands are of the form: TAG PRE MESSAGE-SET POST
 * e.g. `A01 UID COPY 1:4 MAILBOX`
 *
 * @note Must be flushed with imap_exec()
 */
int imap_exec_msg_set(struct ImapAccountData *adata, const char *pre,
                      const char *post, struct UidArray *uida)
{
  struct Buffer cmd = buf_make(ImapMaxCmdlen);

  int count = 0;
  int pos = 0;
  int rc = 0;

  do
  {
    buf_reset(&cmd);
    buf_add_printf(&cmd, "%s ", pre);
    rc = imap_make_msg_set(uida, &cmd, &pos);
    if (rc > 0)
    {
      buf_add_printf(&cmd, " %s", post);
      if (imap_exec(adata, buf_string(&cmd), IMAP_CMD_QUEUE) != IMAP_EXEC_SUCCESS)
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
  return rc;
}
