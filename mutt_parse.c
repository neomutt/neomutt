/**
 * @file
 * Miscellaneous email parsing routines
 *
 * @authors
 * Copyright (C) 1996-2000,2012-2013 Michael R. Elkins <me@mutt.org>
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
 * @page mutt_parse Miscellaneous email parsing routines
 *
 * Miscellaneous email parsing routines
 */

#include "config.h"
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include "mutt/mutt.h"
#include "email/lib.h"
#include "mutt.h"
#include "mutt_parse.h"
#include "globals.h"
#include "mx.h"
#include "ncrypt/ncrypt.h"

#define MUTT_PARTS_TOPLEVEL (1 << 0) /* is the top-level part */

/**
 * mutt_parse_mime_message - Parse a MIME email
 * @param m Mailbox
 * @param e Email
 */
void mutt_parse_mime_message(struct Mailbox *m, struct Email *e)
{
  do
  {
    if ((e->content->type != TYPE_MESSAGE) && (e->content->type != TYPE_MULTIPART))
      break; /* nothing to do */

    if (e->content->parts)
      break; /* The message was parsed earlier. */

    struct Message *msg = mx_msg_open(m, e->msgno);
    if (msg)
    {
      mutt_parse_part(msg->fp, e->content);

      if (WithCrypto)
        e->security = crypt_query(e->content);

      mx_msg_close(m, &msg);
    }
  } while (false);

  e->attach_valid = false;
}

/**
 * count_body_parts_check - Compares mime types to the ok and except lists
 * @param checklist List of AttachMatch
 * @param b         Email Body
 * @param dflt      Log whether the matches are OK, or Excluded
 * @retval true Attachment should be counted
 */
static bool count_body_parts_check(struct ListHead *checklist, struct Body *b, bool dflt)
{
  struct AttachMatch *a = NULL;

  /* If list is null, use default behavior. */
  if (!checklist || STAILQ_EMPTY(checklist))
  {
    return false;
  }

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, checklist, entries)
  {
    a = (struct AttachMatch *) np->data;
    mutt_debug(LL_DEBUG3, "%s %d/%s ?? %s/%s [%d]... ", dflt ? "[OK]   " : "[EXCL] ",
               b->type, b->subtype ? b->subtype : "*", a->major, a->minor, a->major_int);
    if (((a->major_int == TYPE_ANY) || (a->major_int == b->type)) &&
        (!b->subtype || !regexec(&a->minor_regex, b->subtype, 0, NULL, 0)))
    {
      mutt_debug(LL_DEBUG3, "yes\n");
      return true;
    }
    else
    {
      mutt_debug(LL_DEBUG3, "no\n");
    }
  }

  return false;
}

/**
 * count_body_parts - Count the MIME Body parts
 * @param body  Body of email
 * @param flags Flags, e.g. #MUTT_PARTS_TOPLEVEL
 * @retval num Number of MIME Body parts
 */
static int count_body_parts(struct Body *body, int flags)
{
  if (!body)
    return 0;

  int count = 0;

  for (struct Body *bp = body; bp; bp = bp->next)
  {
    /* Initial disposition is to count and not to recurse this part. */
    bool shallcount = true; /* default */
    bool shallrecurse = false;

    mutt_debug(LL_DEBUG5, "desc=\"%s\"; fn=\"%s\", type=\"%d/%s\"\n",
               bp->description ? bp->description : ("none"),
               bp->filename ? bp->filename : bp->d_filename ? bp->d_filename : "(none)",
               bp->type, bp->subtype ? bp->subtype : "*");

    if (bp->type == TYPE_MESSAGE)
    {
      shallrecurse = true;

      /* If it's an external body pointer, don't recurse it. */
      if (mutt_str_strcasecmp(bp->subtype, "external-body") == 0)
        shallrecurse = false;

      /* Don't count containers if they're top-level. */
      if (flags & MUTT_PARTS_TOPLEVEL)
        shallcount = false; // top-level message/*
    }
    else if (bp->type == TYPE_MULTIPART)
    {
      /* Always recurse multiparts, except multipart/alternative. */
      shallrecurse = true;
      if (mutt_str_strcasecmp(bp->subtype, "alternative") == 0)
        shallrecurse = false;

      /* Don't count containers if they're top-level. */
      if (flags & MUTT_PARTS_TOPLEVEL)
        shallcount = false; /* top-level multipart */
    }

    if ((bp->disposition == DISP_INLINE) && (bp->type != TYPE_MULTIPART) &&
        (bp->type != TYPE_MESSAGE) && (bp == body))
    {
      shallcount = false; /* ignore fundamental inlines */
    }

    /* If this body isn't scheduled for enumeration already, don't bother
     * profiling it further.  */
    if (shallcount)
    {
      /* Turn off shallcount if message type is not in ok list,
       * or if it is in except list. Check is done separately for
       * inlines vs. attachments.  */

      if (bp->disposition == DISP_ATTACH)
      {
        if (!count_body_parts_check(&AttachAllow, bp, true))
          shallcount = false; /* attach not allowed */
        if (count_body_parts_check(&AttachExclude, bp, false))
          shallcount = false; /* attach excluded */
      }
      else
      {
        if (!count_body_parts_check(&InlineAllow, bp, true))
          shallcount = false; /* inline not allowed */
        if (count_body_parts_check(&InlineExclude, bp, false))
          shallcount = false; /* excluded */
      }
    }

    if (shallcount)
      count++;
    bp->attach_qualifies = shallcount;

    mutt_debug(LL_DEBUG3, "%p shallcount = %d\n", (void *) bp, shallcount);

    if (shallrecurse)
    {
      mutt_debug(LL_DEBUG3, "%p pre count = %d\n", (void *) bp, count);
      bp->attach_count = count_body_parts(bp->parts, flags & ~MUTT_PARTS_TOPLEVEL);
      count += bp->attach_count;
      mutt_debug(LL_DEBUG3, "%p post count = %d\n", (void *) bp, count);
    }
  }

  mutt_debug(LL_DEBUG3, "return %d\n", (count < 0) ? 0 : count);
  return (count < 0) ? 0 : count;
}

/**
 * mutt_count_body_parts - Count the MIME Body parts
 * @param m Mailbox
 * @param e Email
 * @retval num Number of MIME Body parts
 */
int mutt_count_body_parts(struct Mailbox *m, struct Email *e)
{
  bool keep_parts = false;

  if (e->attach_valid)
    return e->attach_total;

  if (e->content->parts)
    keep_parts = true;
  else
    mutt_parse_mime_message(m, e);

  if (!STAILQ_EMPTY(&AttachAllow) || !STAILQ_EMPTY(&AttachExclude) ||
      !STAILQ_EMPTY(&InlineAllow) || !STAILQ_EMPTY(&InlineExclude))
  {
    e->attach_total = count_body_parts(e->content, MUTT_PARTS_TOPLEVEL);
  }
  else
    e->attach_total = 0;

  e->attach_valid = true;

  if (!keep_parts)
    mutt_body_free(&e->content->parts);

  return e->attach_total;
}
