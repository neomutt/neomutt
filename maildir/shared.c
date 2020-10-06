/**
 * @file
 * Maildir/MH local mailbox type
 *
 * @authors
 * Copyright (C) 1996-2002,2007,2009 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2005 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2010,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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
 * @page maildir_shared Maildir/MH local mailbox type
 *
 * Maildir/MH local mailbox type
 */

#include "config.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "hcache/lib.h"
#include "maildir/lib.h"
#include "copy.h"
#include "edata.h"
#include "mdata.h"
#include "mdemail.h"
#include "mutt_globals.h"
#include "mx.h"
#include "progress.h"
#include "protos.h"
#include "sort.h"
#ifdef USE_NOTMUCH
#include "notmuch/lib.h"
#endif

#define INS_SORT_THRESHOLD 6

/**
 * mh_umask - Create a umask from the mailbox directory
 * @param  m   Mailbox
 * @retval num Umask
 */
mode_t mh_umask(struct Mailbox *m)
{
  struct MaildirMboxData *mdata = maildir_mdata_get(m);
  if (mdata && mdata->mh_umask)
    return mdata->mh_umask;

  struct stat st;
  if (stat(mailbox_path(m), &st) != 0)
  {
    mutt_debug(LL_DEBUG1, "stat failed on %s\n", mailbox_path(m));
    return 077;
  }

  return 0777 & ~st.st_mode;
}

/**
 * maildir_free - Free a Maildir list
 * @param[out] md Maildir list to free
 */
void maildir_free(struct MdEmail **md)
{
  if (!md || !*md)
    return;

  struct MdEmail *p = NULL, *q = NULL;

  for (p = *md; p; p = q)
  {
    q = p->next;
    maildir_entry_free(&p);
  }
}

/**
 * maildir_move_to_mailbox - Copy the Maildir list to the Mailbox
 * @param[in]  m   Mailbox
 * @param[out] ptr Maildir list to copy, then free
 * @retval num Number of new emails
 * @retval 0   Error
 */
int maildir_move_to_mailbox(struct Mailbox *m, struct MdEmail **ptr)
{
  if (!m)
    return 0;

  struct MdEmail *md = *ptr;
  int oldmsgcount = m->msg_count;

  for (; md; md = md->next)
  {
    mutt_debug(LL_DEBUG2, "Considering %s\n", NONULL(md->canon_fname));
    if (!md->email)
      continue;

    mutt_debug(LL_DEBUG2, "Adding header structure. Flags: %s%s%s%s%s\n",
               md->email->flagged ? "f" : "", md->email->deleted ? "D" : "",
               md->email->replied ? "r" : "", md->email->old ? "O" : "",
               md->email->read ? "R" : "");
    if (m->msg_count == m->email_max)
      mx_alloc_memory(m);

    m->emails[m->msg_count] = md->email;
    m->emails[m->msg_count]->index = m->msg_count;
    mailbox_size_add(m, md->email);

    md->email = NULL;
    m->msg_count++;
  }

  int num = 0;
  if (m->msg_count > oldmsgcount)
    num = m->msg_count - oldmsgcount;

  maildir_free(ptr);
  return num;
}

/**
 * md_cmp_inode - Compare two Maildirs by inode number
 * @param a First  Maildir
 * @param b Second Maildir
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int md_cmp_inode(struct MdEmail *a, struct MdEmail *b)
{
  return a->inode - b->inode;
}

/**
 * maildir_merge_lists - Merge two maildir lists
 * @param left    First  Maildir to merge
 * @param right   Second Maildir to merge
 * @param cmp     Comparison function for sorting
 * @retval ptr Merged Maildir
 */
static struct MdEmail *maildir_merge_lists(struct MdEmail *left, struct MdEmail *right,
                                           int (*cmp)(struct MdEmail *, struct MdEmail *))
{
  struct MdEmail *head = NULL;
  struct MdEmail *tail = NULL;

  if (left && right)
  {
    if (cmp(left, right) < 0)
    {
      head = left;
      left = left->next;
    }
    else
    {
      head = right;
      right = right->next;
    }
  }
  else
  {
    if (left)
      return left;
    return right;
  }

  tail = head;

  while (left && right)
  {
    if (cmp(left, right) < 0)
    {
      tail->next = left;
      left = left->next;
    }
    else
    {
      tail->next = right;
      right = right->next;
    }
    tail = tail->next;
  }

  if (left)
  {
    tail->next = left;
  }
  else
  {
    tail->next = right;
  }

  return head;
}

/**
 * maildir_ins_sort - Sort maildirs using an insertion sort
 * @param list    Maildir list to sort
 * @param cmp     Comparison function for sorting
 * @retval ptr Sort Maildir list
 */
static struct MdEmail *maildir_ins_sort(struct MdEmail *list,
                                        int (*cmp)(struct MdEmail *, struct MdEmail *))
{
  struct MdEmail *tmp = NULL, *last = NULL, *back = NULL;

  struct MdEmail *ret = list;
  list = list->next;
  ret->next = NULL;

  while (list)
  {
    last = NULL;
    back = list->next;
    for (tmp = ret; tmp && cmp(tmp, list) <= 0; tmp = tmp->next)
      last = tmp;

    list->next = tmp;
    if (last)
      last->next = list;
    else
      ret = list;

    list = back;
  }

  return ret;
}

/**
 * maildir_sort - Sort Maildir list
 * @param list    Maildirs to sort
 * @param len     Number of Maildirs in the list
 * @param cmp     Comparison function for sorting
 * @retval ptr Sort Maildir list
 */
struct MdEmail *maildir_sort(struct MdEmail *list, size_t len,
                             int (*cmp)(struct MdEmail *, struct MdEmail *))
{
  struct MdEmail *left = list;
  struct MdEmail *right = list;
  size_t c = 0;

  if (!list || !list->next)
  {
    return list;
  }

  if ((len != (size_t)(-1)) && (len <= INS_SORT_THRESHOLD))
    return maildir_ins_sort(list, cmp);

  list = list->next;
  while (list && list->next)
  {
    right = right->next;
    list = list->next->next;
    c++;
  }

  list = right;
  right = right->next;
  list->next = 0;

  left = maildir_sort(left, c, cmp);
  right = maildir_sort(right, c, cmp);
  return maildir_merge_lists(left, right, cmp);
}

/**
 * mh_sort_natural - Sort a Maildir list into its natural order
 * @param[in]  m  Mailbox
 * @param[out] md Maildir list to sort
 *
 * Currently only defined for MH where files are numbered.
 */
void mh_sort_natural(struct Mailbox *m, struct MdEmail **md)
{
  if (!m || !md || !*md || (m->type != MUTT_MH) || (C_Sort != SORT_ORDER))
    return;
  mutt_debug(LL_DEBUG3, "maildir: sorting %s into natural order\n", mailbox_path(m));
  *md = maildir_sort(*md, (size_t) -1, mh_cmp_path);
}

/**
 * skip_duplicates - Find the first non-duplicate message
 * @param[in]  p    Maildir to start
 * @param[out] last Previous Maildir
 * @retval ptr Next Maildir
 */
struct MdEmail *skip_duplicates(struct MdEmail *p, struct MdEmail **last)
{
  /* Skip ahead to the next non-duplicate message.
   *
   * p should never reach NULL, because we couldn't have reached this point
   * unless there was a message that needed to be parsed.
   *
   * The check for p->header_parsed is likely unnecessary since the dupes will
   * most likely be at the head of the list.  but it is present for consistency
   * with the check at the top of the for() loop in maildir_delayed_parsing().
   */
  while (!p->email || p->header_parsed)
  {
    *last = p;
    p = p->next;
  }
  return p;
}

/**
 * maildir_update_flags - Update the mailbox flags
 * @param m     Mailbox
 * @param e_old Old Email
 * @param e_new New Email
 * @retval true  If the flags changed
 * @retval false Otherwise
 */
bool maildir_update_flags(struct Mailbox *m, struct Email *e_old, struct Email *e_new)
{
  if (!m)
    return false;

  /* save the global state here so we can reset it at the
   * end of list block if required.  */
  bool context_changed = m->changed;

  /* user didn't modify this message.  alter the flags to match the
   * current state on disk.  This may not actually do
   * anything. mutt_set_flag() will just ignore the call if the status
   * bits are already properly set, but it is still faster not to pass
   * through it */
  if (e_old->flagged != e_new->flagged)
    mutt_set_flag(m, e_old, MUTT_FLAG, e_new->flagged);
  if (e_old->replied != e_new->replied)
    mutt_set_flag(m, e_old, MUTT_REPLIED, e_new->replied);
  if (e_old->read != e_new->read)
    mutt_set_flag(m, e_old, MUTT_READ, e_new->read);
  if (e_old->old != e_new->old)
    mutt_set_flag(m, e_old, MUTT_OLD, e_new->old);

  /* mutt_set_flag() will set this, but we don't need to
   * sync the changes we made because we just updated the
   * context to match the current on-disk state of the
   * message.  */
  bool header_changed = e_old->changed;
  e_old->changed = false;

  /* if the mailbox was not modified before we made these
   * changes, unset the changed flag since nothing needs to
   * be synchronized.  */
  if (!context_changed)
    m->changed = false;

  return header_changed;
}
