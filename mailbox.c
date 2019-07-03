/**
 * @file
 * Representation of a mailbox
 *
 * @authors
 * Copyright (C) 1996-2000,2010,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2016-2017 Kevin J. McCarthy <kevin@8t8.us>
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
 * @page mailbox Representation of a mailbox
 *
 * Representation of a mailbox
 */

#include "config.h"
#include <sys/stat.h>
#include <utime.h>
#include "email/lib.h"
#include "mailbox.h"
#include "globals.h"
#include "maildir/lib.h"
#include "neomutt.h"

struct MailboxList AllMailboxes = STAILQ_HEAD_INITIALIZER(AllMailboxes);

/**
 * mailbox_new - Create a new Mailbox
 * @retval ptr New Mailbox
 */
struct Mailbox *mailbox_new(void)
{
  struct Mailbox *m = mutt_mem_calloc(1, sizeof(struct Mailbox));

  m->pathbuf = mutt_buffer_new();

  return m;
}

/**
 * mailbox_free - Free a Mailbox
 * @param[out] ptr Mailbox to free
 */
void mailbox_free(struct Mailbox **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Mailbox *m = *ptr;
  mutt_mailbox_changed(m, MBN_CLOSED);

  mutt_buffer_free(&m->pathbuf);
  FREE(&m->desc);
  if (m->mdata && m->free_mdata)
    m->free_mdata(&m->mdata);
  FREE(&m->realpath);
  FREE(ptr);
}

/**
 * mutt_mailbox_cleanup - Restore the timestamp of a mailbox
 * @param path Path to the mailbox
 * @param st   Timestamp info from stat()
 *
 * Fix up the atime and mtime after mbox/mmdf mailbox was modified according to
 * stat() info taken before a modification.
 */
void mutt_mailbox_cleanup(const char *path, struct stat *st)
{
#ifdef HAVE_UTIMENSAT
  struct timespec ts[2];
#else
  struct utimbuf ut;
#endif

  if (C_CheckMboxSize)
  {
    struct Mailbox *m = mutt_mailbox_find(path);
    if (m && !m->has_new)
      mutt_mailbox_update(m);
  }
  else
  {
    /* fix up the times so mailbox won't get confused */
    if (st->st_mtime > st->st_atime)
    {
#ifdef HAVE_UTIMENSAT
      ts[0].tv_sec = 0;
      ts[0].tv_nsec = UTIME_OMIT;
      ts[1].tv_sec = 0;
      ts[1].tv_nsec = UTIME_NOW;
      utimensat(0, buf, ts, 0);
#else
      ut.actime = st->st_atime;
      ut.modtime = time(NULL);
      utime(path, &ut);
#endif
    }
    else
    {
#ifdef HAVE_UTIMENSAT
      ts[0].tv_sec = 0;
      ts[0].tv_nsec = UTIME_NOW;
      ts[1].tv_sec = 0;
      ts[1].tv_nsec = UTIME_NOW;
      utimensat(0, buf, ts, 0);
#else
      utime(path, NULL);
#endif
    }
  }
}

/**
 * mutt_mailbox_find - Find the mailbox with a given path
 * @param path Path to match
 * @retval ptr Matching Mailbox
 */
struct Mailbox *mutt_mailbox_find(const char *path)
{
  if (!path)
    return NULL;

  struct stat sb;
  struct stat tmp_sb;

  if (stat(path, &sb) != 0)
    return NULL;

  struct MailboxList ml = neomutt_mailboxlist_get_all(NeoMutt, MUTT_MAILBOX_ANY);
  struct MailboxNode *np = NULL;
  struct Mailbox *m = NULL;
  STAILQ_FOREACH(np, &ml, entries)
  {
    if ((stat(mutt_b2s(np->mailbox->pathbuf), &tmp_sb) == 0) &&
        (sb.st_dev == tmp_sb.st_dev) && (sb.st_ino == tmp_sb.st_ino))
    {
      m = np->mailbox;
      break;
    }
  }
  neomutt_mailboxlist_clear(&ml);

  return m;
}

/**
 * mutt_mailbox_find_desc - Find the mailbox with a given description
 * @param desc Description to match
 * @retval ptr Matching Mailbox
 * @retval NULL No matching mailbox found
 */
struct Mailbox *mutt_mailbox_find_desc(const char *desc)
{
  if (!desc)
    return NULL;

  struct MailboxList ml = neomutt_mailboxlist_get_all(NeoMutt, MUTT_MAILBOX_ANY);
  struct MailboxNode *np = NULL;
  struct Mailbox *m = NULL;
  STAILQ_FOREACH(np, &ml, entries)
  {
    if (np->mailbox->desc && (mutt_str_strcmp(np->mailbox->desc, desc) == 0))
    {
      m = np->mailbox;
      break;
    }
  }
  neomutt_mailboxlist_clear(&ml);

  return m;
}

/**
 * mutt_mailbox_update - Get the mailbox's current size
 * @param m Mailbox to check
 */
void mutt_mailbox_update(struct Mailbox *m)
{
  struct stat sb;

  if (!m)
    return;

  if (stat(mutt_b2s(m->pathbuf), &sb) == 0)
    m->size = (off_t) sb.st_size;
  else
    m->size = 0;
}

/**
 * mutt_mailbox_changed - Notify observers of a change to a Mailbox
 * @param m      Mailbox
 * @param action Change to Mailbox
 */
void mutt_mailbox_changed(struct Mailbox *m, enum MailboxNotification action)
{
  if (!m || !m->notify2)
    return;

  m->notify2(m, action);
}

/**
 * mutt_mailbox_size_add - Add an email's size to the total size of a Mailbox
 * @param m Mailbox
 * @param e Email
 */
void mutt_mailbox_size_add(struct Mailbox *m, const struct Email *e)
{
  m->size += mutt_email_size(e);
}

/**
 * mutt_mailbox_size_sub - Subtract an email's size from the total size of a Mailbox
 * @param m Mailbox
 * @param e Email
 */
void mutt_mailbox_size_sub(struct Mailbox *m, const struct Email *e)
{
  m->size -= mutt_email_size(e);
}
