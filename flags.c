/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "mutt_curses.h"
#include "sort.h"
#include "mx.h"

void _mutt_set_flag (CONTEXT *ctx, HEADER *h, int flag, int bf, int upd_ctx)
{
  int changed = h->changed;
  int deleted = ctx->deleted;
  int tagged = ctx->tagged;
  int flagged = ctx->flagged;
  int update = 0;

  if (ctx->readonly && flag != MUTT_TAG)
    return; /* don't modify anything if we are read-only */

  switch (flag)
  {
    case MUTT_DELETE:

      if (!mutt_bit_isset(ctx->rights,MUTT_ACL_DELETE))
	return;

      if (bf)
      {
	if (!h->deleted && !ctx->readonly)
	{
	  h->deleted = 1;
          update = 1;
	  if (upd_ctx) ctx->deleted++;
#ifdef USE_IMAP
          /* deleted messages aren't treated as changed elsewhere so that the
           * purge-on-sync option works correctly. This isn't applicable here */
          if (ctx && ctx->magic == MUTT_IMAP)
          {
            h->changed = 1;
	    if (upd_ctx) ctx->changed = 1;
          }
#endif
	}
      }
      else if (h->deleted)
      {
	h->deleted = 0;
        update = 1;
	if (upd_ctx) ctx->deleted--;
#ifdef USE_IMAP
        /* see my comment above */
	if (ctx->magic == MUTT_IMAP) 
	{
	  h->changed = 1;
	  if (upd_ctx) ctx->changed = 1;
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
	  ctx->changed = 1;
      }
      break;

    case MUTT_PURGE:

      if (!mutt_bit_isset(ctx->rights,MUTT_ACL_DELETE))
        return;

      if (bf)
      {
        if (!h->purge && !ctx->readonly)
          h->purge = 1;
      }
      else if (h->purge)
        h->purge = 0;
      break;

    case MUTT_NEW:

      if (!mutt_bit_isset(ctx->rights,MUTT_ACL_SEEN))
	return;

      if (bf)
      {
	if (h->read || h->old)
	{
          update = 1;
	  h->old = 0;
	  if (upd_ctx) ctx->new++;
	  if (h->read)
	  {
	    h->read = 0;
	    if (upd_ctx) ctx->unread++;
	  }
	  h->changed = 1;
	  if (upd_ctx) ctx->changed = 1;
	}
      }
      else if (!h->read)
      {
        update = 1;
	if (!h->old)
	  if (upd_ctx) ctx->new--;
	h->read = 1;
	if (upd_ctx) ctx->unread--;
	h->changed = 1;
	if (upd_ctx) ctx->changed = 1;
      }
      break;

    case MUTT_OLD:

      if (!mutt_bit_isset(ctx->rights,MUTT_ACL_SEEN))
	return;

      if (bf)
      {
	if (!h->old)
	{
          update = 1;
	  h->old = 1;
	  if (!h->read)
	    if (upd_ctx) ctx->new--;
	  h->changed = 1;
	  if (upd_ctx) ctx->changed = 1;
	}
      }
      else if (h->old)
      {
        update = 1;
	h->old = 0;
	if (!h->read)
	  if (upd_ctx) ctx->new++;
	h->changed = 1;
	if (upd_ctx) ctx->changed = 1;
      }
      break;

    case MUTT_READ:

      if (!mutt_bit_isset(ctx->rights,MUTT_ACL_SEEN))
	return;

      if (bf)
      {
	if (!h->read)
	{
          update = 1;
	  h->read = 1;
	  if (upd_ctx) ctx->unread--;
	  if (!h->old)
	    if (upd_ctx) ctx->new--;
	  h->changed = 1;
	  if (upd_ctx) ctx->changed = 1;
	}
      }
      else if (h->read)
      {
        update = 1;
	h->read = 0;
	if (upd_ctx) ctx->unread++;
	if (!h->old)
	  if (upd_ctx) ctx->new++;
	h->changed = 1;
	if (upd_ctx) ctx->changed = 1;
      }
      break;

    case MUTT_REPLIED:

      if (!mutt_bit_isset(ctx->rights,MUTT_ACL_WRITE))
	return;

      if (bf)
      {
	if (!h->replied)
	{
          update = 1;
	  h->replied = 1;
	  if (!h->read)
	  {
	    h->read = 1;
	    if (upd_ctx) ctx->unread--;
	    if (!h->old)
	      if (upd_ctx) ctx->new--;
	  }
	  h->changed = 1;
	  if (upd_ctx) ctx->changed = 1;
	}
      }
      else if (h->replied)
      {
        update = 1;
	h->replied = 0;
	h->changed = 1;
	if (upd_ctx) ctx->changed = 1;
      }
      break;

    case MUTT_FLAG:

      if (!mutt_bit_isset(ctx->rights,MUTT_ACL_WRITE))
	return;

      if (bf)
      {
	if (!h->flagged)
	{
          update = 1;
	  h->flagged = bf;
	  if (upd_ctx) ctx->flagged++;
	  h->changed = 1;
	  if (upd_ctx) ctx->changed = 1;
	}
      }
      else if (h->flagged)
      {
        update = 1;
	h->flagged = 0;
	if (upd_ctx) ctx->flagged--;
	h->changed = 1;
	if (upd_ctx) ctx->changed = 1;
      }
      break;

    case MUTT_TAG:
      if (bf)
      {
	if (!h->tagged)
	{
          update = 1;
	  h->tagged = 1;
	  if (upd_ctx) ctx->tagged++;
	}
      }
      else if (h->tagged)
      {
        update = 1;
	h->tagged = 0;
	if (upd_ctx) ctx->tagged--;
      }
      break;
  }

  if (update)
  {
    mutt_set_header_color(ctx, h);
#ifdef USE_SIDEBAR
    SidebarNeedsRedraw = 1;
#endif
  }

  /* if the message status has changed, we need to invalidate the cached
   * search results so that any future search will match the current status
   * of this message and not what it was at the time it was last searched.
   */
  if (h->searched && (changed != h->changed || deleted != ctx->deleted || tagged != ctx->tagged || flagged != ctx->flagged))
    h->searched = 0;
}

void mutt_tag_set_flag (int flag, int bf)
{
  int j;

  for (j = 0; j < Context->vcount; j++)
    if (Context->hdrs[Context->v2r[j]]->tagged)
      mutt_set_flag (Context, Context->hdrs[Context->v2r[j]], flag, bf);
}
int mutt_thread_set_flag (HEADER *hdr, int flag, int bf, int subthread)
{
  THREAD *start, *cur = hdr->thread;
  
  if ((Sort & SORT_MASK) != SORT_THREADS)
  {
    mutt_error _("Threading is not enabled.");
    return (-1);
  }

  if (!subthread)
    while (cur->parent)
      cur = cur->parent;
  start = cur;
  
  if (cur->message)
    mutt_set_flag (Context, cur->message, flag, bf);

  if ((cur = cur->child) == NULL)
    return (0);

  FOREVER
  {
    if (cur->message)
      mutt_set_flag (Context, cur->message, flag, bf);

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
	  return (0);
      }
      cur = cur->next;
    }
  }
  /* not reached */
}

int mutt_change_flag (HEADER *h, int bf)
{
  int i, flag;
  event_t event;

  mutt_window_mvprintw (MuttMessageWindow, 0, 0,
                        "%s? (D/N/O/r/*/!): ", bf ? _("Set flag") : _("Clear flag"));
  mutt_window_clrtoeol (MuttMessageWindow);

  event = mutt_getch();
  i = event.ch;
  if (i < 0)
  {
    mutt_window_clearline (MuttMessageWindow, 0);
    return (-1);
  }

  mutt_window_clearline (MuttMessageWindow, 0);

  switch (i)
  {
    case 'd':
    case 'D':
      if (!bf)
      {
        if (h)
          mutt_set_flag (Context, h, MUTT_PURGE, bf);
        else
          mutt_tag_set_flag (MUTT_PURGE, bf);
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
	mutt_set_flag (Context, h, MUTT_READ, !bf);
      else
	mutt_tag_set_flag (MUTT_READ, !bf);
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
      BEEP ();
      return (-1);
  }

  if (h)
    mutt_set_flag (Context, h, flag, bf);
  else
    mutt_tag_set_flag (flag, bf);

  return 0;
}
