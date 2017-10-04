/**
 * @file
 * Manipulate the flags in an email header
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
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
#include "lib/lib.h"
#include "mutt.h"
#include "context.h"
#include "globals.h"
#include "header.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "mx.h"
#include "options.h"
#include "protos.h"
#include "sort.h"
#include "thread.h"

void _mutt_set_flag(struct Context *ctx, struct Header *h, int flag, int bf, int upd_ctx)
{
  if (!ctx || !h)
    return;

  bool changed = h->changed;
  int deleted = ctx->deleted;
  int tagged = ctx->tagged;
  int flagged = ctx->flagged;
  int update = false;

  if (ctx->readonly && flag != MUTT_TAG)
    return; /* don't modify anything if we are read-only */

  switch (flag)
  {
    case MUTT_DELETE:

      if (!mutt_bit_isset(ctx->rights, MUTT_ACL_DELETE))
        return;

      if (bf)
      {
        if (!h->deleted && !ctx->readonly && (!h->flagged || !option(OPT_FLAG_SAFE)))
        {
          h->deleted = true;
          update = true;
          if (upd_ctx)
            ctx->deleted++;
#ifdef USE_IMAP
          /* deleted messages aren't treated as changed elsewhere so that the
           * purge-on-sync option works correctly. This isn't applicable here */
          if (ctx && ctx->magic == MUTT_IMAP)
          {
            h->changed = true;
            if (upd_ctx)
              ctx->changed = true;
          }
#endif
        }
      }
      else if (h->deleted)
      {
        h->deleted = false;
        update = true;
        if (upd_ctx)
          ctx->deleted--;
#ifdef USE_IMAP
        /* see my comment above */
        if (ctx->magic == MUTT_IMAP)
        {
          h->changed = true;
          if (upd_ctx)
            ctx->changed = true;
        }
#endif
        /*
         * If the user undeletes a message which is marked as
         * "trash" in the maildir folder on disk, the folder has
         * been changed, and is marked accordingly.  However, we do
         * _not_ mark the message itself changed, because trashing
         * is checked in specific code in the maildir folder
         * driver.
         */
        if (ctx->magic == MUTT_MAILDIR && upd_ctx && h->trash)
          ctx->changed = true;
      }
      break;

    case MUTT_PURGE:

      if (!mutt_bit_isset(ctx->rights, MUTT_ACL_DELETE))
        return;

      if (bf)
      {
        if (!h->purge && !ctx->readonly)
          h->purge = true;
      }
      else if (h->purge)
        h->purge = false;
      break;

    case MUTT_NEW:

      if (!mutt_bit_isset(ctx->rights, MUTT_ACL_SEEN))
        return;

      if (bf)
      {
        if (h->read || h->old)
        {
          update = true;
          h->old = false;
          if (upd_ctx)
            ctx->new ++;
          if (h->read)
          {
            h->read = false;
            if (upd_ctx)
              ctx->unread++;
          }
          h->changed = true;
          if (upd_ctx)
            ctx->changed = true;
        }
      }
      else if (!h->read)
      {
        update = true;
        if (!h->old)
          if (upd_ctx)
            ctx->new --;
        h->read = true;
        if (upd_ctx)
          ctx->unread--;
        h->changed = true;
        if (upd_ctx)
          ctx->changed = true;
      }
      break;

    case MUTT_OLD:

      if (!mutt_bit_isset(ctx->rights, MUTT_ACL_SEEN))
        return;

      if (bf)
      {
        if (!h->old)
        {
          update = true;
          h->old = true;
          if (!h->read)
            if (upd_ctx)
              ctx->new --;
          h->changed = true;
          if (upd_ctx)
            ctx->changed = true;
        }
      }
      else if (h->old)
      {
        update = true;
        h->old = false;
        if (!h->read)
          if (upd_ctx)
            ctx->new ++;
        h->changed = true;
        if (upd_ctx)
          ctx->changed = true;
      }
      break;

    case MUTT_READ:

      if (!mutt_bit_isset(ctx->rights, MUTT_ACL_SEEN))
        return;

      if (bf)
      {
        if (!h->read)
        {
          update = true;
          h->read = true;
          if (upd_ctx)
            ctx->unread--;
          if (!h->old)
            if (upd_ctx)
              ctx->new --;
          h->changed = true;
          if (upd_ctx)
            ctx->changed = true;
        }
      }
      else if (h->read)
      {
        update = true;
        h->read = false;
        if (upd_ctx)
          ctx->unread++;
        if (!h->old)
          if (upd_ctx)
            ctx->new ++;
        h->changed = true;
        if (upd_ctx)
          ctx->changed = true;
      }
      break;

    case MUTT_REPLIED:

      if (!mutt_bit_isset(ctx->rights, MUTT_ACL_WRITE))
        return;

      if (bf)
      {
        if (!h->replied)
        {
          update = true;
          h->replied = true;
          if (!h->read)
          {
            h->read = true;
            if (upd_ctx)
              ctx->unread--;
            if (!h->old)
              if (upd_ctx)
                ctx->new --;
          }
          h->changed = true;
          if (upd_ctx)
            ctx->changed = true;
        }
      }
      else if (h->replied)
      {
        update = true;
        h->replied = false;
        h->changed = true;
        if (upd_ctx)
          ctx->changed = true;
      }
      break;

    case MUTT_FLAG:

      if (!mutt_bit_isset(ctx->rights, MUTT_ACL_WRITE))
        return;

      if (bf)
      {
        if (!h->flagged)
        {
          update = true;
          h->flagged = bf;
          if (upd_ctx)
            ctx->flagged++;
          h->changed = true;
          if (upd_ctx)
            ctx->changed = true;
        }
      }
      else if (h->flagged)
      {
        update = true;
        h->flagged = false;
        if (upd_ctx)
          ctx->flagged--;
        h->changed = true;
        if (upd_ctx)
          ctx->changed = true;
      }
      break;

    case MUTT_TAG:
      if (bf)
      {
        if (!h->tagged)
        {
          update = true;
          h->tagged = true;
          if (upd_ctx)
            ctx->tagged++;
        }
      }
      else if (h->tagged)
      {
        update = true;
        h->tagged = false;
        if (upd_ctx)
          ctx->tagged--;
      }
      break;
  }

  if (update)
  {
    mutt_set_header_color(ctx, h);
#ifdef USE_SIDEBAR
    mutt_set_current_menu_redraw(REDRAW_SIDEBAR);
#endif
  }

  /* if the message status has changed, we need to invalidate the cached
   * search results so that any future search will match the current status
   * of this message and not what it was at the time it was last searched.
   */
  if (h->searched && (changed != h->changed || deleted != ctx->deleted ||
                      tagged != ctx->tagged || flagged != ctx->flagged))
    h->searched = false;
}

/**
 * mutt_thread_set_flag - Set a flag on an entire thread
 */
void mutt_tag_set_flag(int flag, int bf)
{
  for (int i = 0; i < Context->msgcount; i++)
    if (message_is_tagged(Context, i))
      mutt_set_flag(Context, Context->hdrs[i], flag, bf);
}

int mutt_thread_set_flag(struct Header *hdr, int flag, int bf, int subthread)
{
  struct MuttThread *start = NULL, *cur = hdr->thread;

  if ((Sort & SORT_MASK) != SORT_THREADS)
  {
    mutt_error(_("Threading is not enabled."));
    return -1;
  }

  if (!subthread)
    while (cur->parent)
      cur = cur->parent;
  start = cur;

  if (cur->message && cur != hdr->thread)
    mutt_set_flag(Context, cur->message, flag, bf);

  cur = cur->child;
  if (!cur)
    goto done;

  while (true)
  {
    if (cur->message && cur != hdr->thread)
      mutt_set_flag(Context, cur->message, flag, bf);

    if (cur->child)
      cur = cur->child;
    else if (cur->next)
      cur = cur->next;
    else
    {
      while (!cur->next)
      {
        cur = cur->parent;
        if (cur == start)
          goto done;
      }
      cur = cur->next;
    }
  }
done:
  cur = hdr->thread;
  if (cur->message)
    mutt_set_flag(Context, cur->message, flag, bf);
  return 0;
}

int mutt_change_flag(struct Header *h, int bf)
{
  int i, flag;
  struct Event event;

  mutt_window_mvprintw(MuttMessageWindow, 0, 0, "%s? (D/N/O/r/*/!): ",
                       bf ? _("Set flag") : _("Clear flag"));
  mutt_window_clrtoeol(MuttMessageWindow);

  event = mutt_getch();
  i = event.ch;
  if (i < 0)
  {
    mutt_window_clearline(MuttMessageWindow, 0);
    return -1;
  }

  mutt_window_clearline(MuttMessageWindow, 0);

  switch (i)
  {
    case 'd':
    case 'D':
      if (!bf)
      {
        if (h)
          mutt_set_flag(Context, h, MUTT_PURGE, bf);
        else
          mutt_tag_set_flag(MUTT_PURGE, bf);
      }
      flag = MUTT_DELETE;
      break;

    case 'N':
    case 'n':
      flag = MUTT_NEW;
      break;

    case 'o':
    case 'O':
      if (h)
        mutt_set_flag(Context, h, MUTT_READ, !bf);
      else
        mutt_tag_set_flag(MUTT_READ, !bf);
      flag = MUTT_OLD;
      break;

    case 'r':
    case 'R':
      flag = MUTT_REPLIED;
      break;

    case '*':
      flag = MUTT_TAG;
      break;

    case '!':
      flag = MUTT_FLAG;
      break;

    default:
      BEEP();
      return -1;
  }

  if (h)
    mutt_set_flag(Context, h, flag, bf);
  else
    mutt_tag_set_flag(flag, bf);

  return 0;
}
