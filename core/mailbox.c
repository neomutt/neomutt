/**
 * @file
 * Representation of a mailbox
 *
 * @authors
 * Copyright (C) 2016-2017 Kevin J. McCarthy <kevin@8t8.us>
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020-2022 Pietro Cerutti <gahr@gahr.ch>
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
 * @page core_mailbox Mailbox object
 *
 * Representation of a Mailbox
 */

#include "config.h"
#include <sys/stat.h>
#include "config/lib.h"
#include "email/lib.h"
#include "mailbox.h"
#include "neomutt.h"

/// Lookups for Mailbox types
static const struct Mapping MailboxTypes[] = {
  // clang-format off
  { "compressed", MUTT_COMPRESSED },
  { "imap",       MUTT_IMAP },
  { "maildir",    MUTT_MAILDIR },
  { "mbox",       MUTT_MBOX },
  { "mh",         MUTT_MH },
  { "mmdf",       MUTT_MMDF },
  { "nntp",       MUTT_NNTP },
  { "notmuch",    MUTT_NOTMUCH },
  { "pop",        MUTT_POP },
  { NULL, 0 },
  // clang-format on
};

/**
 * mailbox_gen - Get the next generation number
 * @retval num Unique number
 */
int mailbox_gen(void)
{
  static int gen = 0;
  return gen++;
}

/**
 * mailbox_new - Create a new Mailbox
 * @retval ptr New Mailbox
 */
struct Mailbox *mailbox_new(void)
{
  struct Mailbox *m = MUTT_MEM_CALLOC(1, struct Mailbox);

  buf_init(&m->pathbuf);
  m->notify = notify_new();

  m->email_max = 25;
  m->emails = MUTT_MEM_CALLOC(m->email_max, struct Email *);
  m->v2r = MUTT_MEM_CALLOC(m->email_max, int);
  m->gen = mailbox_gen();
  m->notify_user = true;
  m->poll_new_mail = true;

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

  const bool do_free = (m->opened == 0) && !m->visible;

  mutt_debug(LL_DEBUG3, "%sfreeing %s mailbox %s with refcount %d\n",
             do_free ? "" : "not ", m->visible ? "visible" : "invisible",
             buf_string(&m->pathbuf), m->opened);

  if (!do_free)
  {
    return;
  }

  mutt_debug(LL_NOTIFY, "NT_MAILBOX_DELETE: %s %p\n",
             mailbox_get_type_name(m->type), (void *) m);
  struct EventMailbox ev_m = { m };
  notify_send(m->notify, NT_MAILBOX, NT_MAILBOX_DELETE, &ev_m);

  mutt_debug(LL_NOTIFY, "NT_EMAIL_DELETE_ALL\n");
  struct EventEmail ev_e = { 0, NULL };
  notify_send(m->notify, NT_EMAIL, NT_EMAIL_DELETE_ALL, &ev_e);

  for (size_t i = 0; i < m->email_max; i++)
    email_free(&m->emails[i]);

  m->email_max = 0;
  m->msg_count = 0;
  m->msg_deleted = 0;
  m->msg_flagged = 0;
  m->msg_new = 0;
  m->msg_tagged = 0;
  m->msg_unread = 0;

  if (m->mdata_free && m->mdata)
    m->mdata_free(&m->mdata);

  buf_dealloc(&m->pathbuf);
  cs_subset_free(&m->sub);
  FREE(&m->name);
  FREE(&m->realpath);
  FREE(&m->emails);
  FREE(&m->v2r);
  notify_free(&m->notify);
  mailbox_gc_run();

  /* The NT_MAILBOX_DELETE notification might already have caused *ptr to be NULL,
   * so call free() on the m pointer */
  *ptr = NULL;
  FREE(&m);
}

/**
 * mailbox_find - Find the mailbox with a given path
 * @param path Path to match
 * @retval ptr Matching Mailbox
 */
struct Mailbox *mailbox_find(const char *path)
{
  if (!path)
    return NULL;

  struct stat st = { 0 };
  struct stat st_tmp = { 0 };

  if (stat(path, &st) != 0)
    return NULL;

  struct MailboxList ml = STAILQ_HEAD_INITIALIZER(ml);
  neomutt_mailboxlist_get_all(&ml, NeoMutt, MUTT_MAILBOX_ANY);
  struct MailboxNode *np = NULL;
  struct Mailbox *m = NULL;
  STAILQ_FOREACH(np, &ml, entries)
  {
    if ((stat(mailbox_path(np->mailbox), &st_tmp) == 0) &&
        (st.st_dev == st_tmp.st_dev) && (st.st_ino == st_tmp.st_ino))
    {
      m = np->mailbox;
      break;
    }
  }
  neomutt_mailboxlist_clear(&ml);

  return m;
}

/**
 * mailbox_find_name - Find the mailbox with a given name
 * @param name Name to match
 * @retval ptr Matching Mailbox
 * @retval NULL No matching mailbox found
 *
 * @note This searches across all Accounts
 */
struct Mailbox *mailbox_find_name(const char *name)
{
  if (!name)
    return NULL;

  struct MailboxList ml = STAILQ_HEAD_INITIALIZER(ml);
  neomutt_mailboxlist_get_all(&ml, NeoMutt, MUTT_MAILBOX_ANY);
  struct MailboxNode *np = NULL;
  struct Mailbox *m = NULL;
  STAILQ_FOREACH(np, &ml, entries)
  {
    if (mutt_str_equal(np->mailbox->name, name))
    {
      m = np->mailbox;
      break;
    }
  }
  neomutt_mailboxlist_clear(&ml);

  return m;
}

/**
 * mailbox_update - Get the mailbox's current size
 * @param m Mailbox to check
 *
 * @note Only applies to local Mailboxes
 */
void mailbox_update(struct Mailbox *m)
{
  struct stat st = { 0 };

  if (!m)
    return;

  if (stat(mailbox_path(m), &st) == 0)
    m->size = (off_t) st.st_size;
  else
    m->size = 0;
}

/**
 * mailbox_changed - Notify observers of a change to a Mailbox
 * @param m      Mailbox
 * @param action Change to Mailbox
 */
void mailbox_changed(struct Mailbox *m, enum NotifyMailbox action)
{
  if (!m)
    return;

  mutt_debug(LL_NOTIFY, "NT_MAILBOX_CHANGE: %s %p\n",
             mailbox_get_type_name(m->type), (void *) m);
  struct EventMailbox ev_m = { m };
  notify_send(m->notify, NT_MAILBOX, action, &ev_m);
}

/**
 * mailbox_size_add - Add an email's size to the total size of a Mailbox
 * @param m Mailbox
 * @param e Email
 */
void mailbox_size_add(struct Mailbox *m, const struct Email *e)
{
  m->size += email_size(e);
}

/**
 * mailbox_size_sub - Subtract an email's size from the total size of a Mailbox
 * @param m Mailbox
 * @param e Email
 */
void mailbox_size_sub(struct Mailbox *m, const struct Email *e)
{
  m->size -= email_size(e);
}

/**
 * mailbox_set_subset - Set a Mailbox's Config Subset
 * @param m   Mailbox
 * @param sub Parent Config Subset
 * @retval true Success
 */
bool mailbox_set_subset(struct Mailbox *m, struct ConfigSubset *sub)
{
  if (!m || m->sub || !sub)
    return false;

  m->sub = cs_subset_new(m->name, sub, m->notify);
  m->sub->scope = SET_SCOPE_MAILBOX;
  return true;
}

/**
 * struct EmailGarbageCollector - Email garbage collection
 */
struct EmailGarbageCollector
{
  struct Email *arr[10]; ///< Array of Emails to be deleted
  size_t idx;            ///< Current position
};

/// Set of Emails to be deleted
static struct EmailGarbageCollector GC = { 0 };

/**
 * mailbox_gc_add - Add an Email to the garbage-collection set
 * @param e Email
 *
 * @pre e is not NULL
 */
void mailbox_gc_add(struct Email *e)
{
  ASSERT(e);
  if (GC.idx == mutt_array_size(GC.arr))
  {
    mailbox_gc_run();
  }
  GC.arr[GC.idx] = e;
  GC.idx++;
}

/**
 * mailbox_gc_run - Run the garbage-collection
 */
void mailbox_gc_run(void)
{
  for (size_t i = 0; i < GC.idx; i++)
  {
    email_free(&GC.arr[i]);
  }
  GC.idx = 0;
}

/**
 * mailbox_get_type_name - Get the type of a Mailbox
 * @param type Mailbox type, e.g. #MUTT_IMAP
 * @retval ptr  String describing Mailbox type
 */
const char *mailbox_get_type_name(enum MailboxType type)
{
  const char *name = mutt_map_get_name(type, MailboxTypes);
  if (name)
    return name;
  return "UNKNOWN";
}
