/**
 * @file
 * Representation of a mailbox
 *
 * @authors
 * Copyright (C) 1996-2000,2010,2013 Michael R. Elkins <me@mutt.org>
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

#ifndef MUTT_CORE_MAILBOX_H
#define MUTT_CORE_MAILBOX_H

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "path.h"

struct ConfigSubset;
struct Email;

#define MB_NORMAL 0
#define MB_HIDDEN 1

/**
 * enum MailboxType - Supported mailbox formats
 */
enum MailboxType
{
  MUTT_MAILBOX_ANY = -2,   ///< Match any Mailbox type
  MUTT_MAILBOX_ERROR = -1, ///< Error occurred examining Mailbox
  MUTT_UNKNOWN = 0,        ///< Mailbox wasn't recognised
  MUTT_MBOX,               ///< 'mbox' Mailbox type
  MUTT_MMDF,               ///< 'mmdf' Mailbox type
  MUTT_MH,                 ///< 'MH' Mailbox type
  MUTT_MAILDIR,            ///< 'Maildir' Mailbox type
  MUTT_NNTP,               ///< 'NNTP' (Usenet) Mailbox type
  MUTT_IMAP,               ///< 'IMAP' Mailbox type
  MUTT_NOTMUCH,            ///< 'Notmuch' (virtual) Mailbox type
  MUTT_POP,                ///< 'POP3' Mailbox type
  MUTT_COMPRESSED,         ///< Compressed file Mailbox type
  MUTT_LOCAL_FILE,         ///< Local filesystem file
  MUTT_LOCAL_DIR,          ///< Local filesystem directory
};

/**
 * ACL Rights - These show permission to...
 */
typedef uint16_t AclFlags;          ///< Flags, e.g. #MUTT_ACL_ADMIN
#define MUTT_ACL_NO_FLAGS       0   ///< No flags are set
#define MUTT_ACL_ADMIN   (1 <<  0)  ///< Administer the account (get/set permissions)
#define MUTT_ACL_CREATE  (1 <<  1)  ///< Create a mailbox
#define MUTT_ACL_DELETE  (1 <<  2)  ///< Delete a message
#define MUTT_ACL_DELMX   (1 <<  3)  ///< Delete a mailbox
#define MUTT_ACL_EXPUNGE (1 <<  4)  ///< Expunge messages
#define MUTT_ACL_INSERT  (1 <<  5)  ///< Add/copy into the mailbox (used when editing a message)
#define MUTT_ACL_LOOKUP  (1 <<  6)  ///< Lookup mailbox (visible to 'list')
#define MUTT_ACL_POST    (1 <<  7)  ///< Post (submit messages to the server)
#define MUTT_ACL_READ    (1 <<  8)  ///< Read the mailbox
#define MUTT_ACL_SEEN    (1 <<  9)  ///< Change the 'seen' status of a message
#define MUTT_ACL_WRITE   (1 << 10)  ///< Write to a message (for flagging or linking threads)

#define MUTT_ACL_ALL    ((1 << 11) - 1)

/**
 * struct Mailbox - A mailbox
 */
struct Mailbox
{
  struct Path *path;                  ///< Path representing the Mailbox
  struct ConfigSubset *sub;           ///< Inherited config items
  off_t size;                         ///< Size of the Mailbox
  bool has_new;                       ///< Mailbox has new mail

  // These next three are only set when MailCheckStats is set
  int msg_count;                      ///< Total number of messages
  int msg_unread;                     ///< Number of unread messages
  int msg_flagged;                    ///< Number of flagged messages

  int msg_new;                        ///< Number of new messages
  int msg_deleted;                    ///< Number of deleted messages
  int msg_tagged;                     ///< How many messages are tagged?

  struct Email **emails;              ///< Array of Emails
  int email_max;                      ///< Number of pointers in emails
  int *v2r;                           ///< Mapping from virtual to real msgno
  int vcount;                         ///< The number of virtual messages

  bool notified;                      ///< User has been notified
  enum MailboxType type;              ///< Mailbox type
  bool newly_created;                 ///< Mbox or mmdf just popped into existence
  struct timespec mtime;              ///< Time Mailbox was last changed
  struct timespec last_visited;       ///< Time of last exit from this mailbox
  struct timespec stats_last_checked; ///< Mtime of mailbox the last time stats where checked.

  const struct MxOps *mx_ops;         ///< MXAPI callback functions

  bool append                 : 1;    ///< Mailbox is opened in append mode
  bool changed                : 1;    ///< Mailbox has been modified
  bool dontwrite              : 1;    ///< Don't write the mailbox on close
  bool first_check_stats_done : 1;    ///< True when the check have been done at least on time
  bool peekonly               : 1;    ///< Just taking a glance, revert atime
  bool verbose                : 1;    ///< Display status messages?
  bool readonly               : 1;    ///< Don't allow changes to the mailbox

  AclFlags rights;                    ///< ACL bits, see #AclFlags

#ifdef USE_COMP_MBOX
  void *compress_info;                ///< Compressed mbox module private data
#endif

  struct HashTable *id_hash;          ///< Hash Table by msg id
  struct HashTable *subj_hash;        ///< Hash Table by subject
  struct HashTable *label_hash;       ///< Hash Table for x-labels

  struct Account *account;            ///< Account that owns this Mailbox
  int opened;                         ///< Number of times mailbox is opened

  int flags;                          ///< e.g. #MB_NORMAL

  void *mdata;                        ///< Driver specific data
  void (*mdata_free)(void **ptr);     ///< Driver-specific data free function

  struct Notify *notify;              ///< Notifications handler
};

/**
 * struct MailboxNode - List of Mailboxes
 */
struct MailboxNode
{
  struct Mailbox *mailbox;           ///< Mailbox in the list
  STAILQ_ENTRY(MailboxNode) entries; ///< Linked list
};
STAILQ_HEAD(MailboxList, MailboxNode);

/**
 * enum NotifyMailbox - Types of Mailbox Event
 *
 * Observers of #NT_MAILBOX will be passed an #EventMailbox.
 */
enum NotifyMailbox
{
  NT_MAILBOX_ADD = 1, ///< A new Mailbox has been created
  NT_MAILBOX_REMOVE,  ///< A Mailbox is about to be destroyed

  /* These don't really belong here as they are tied to GUI operations.
   * Eventually, they'll be eliminated. */
  NT_MAILBOX_CLOSED,  ///< Mailbox was closed
  NT_MAILBOX_INVALID, ///< Email list was changed
  NT_MAILBOX_RESORT,  ///< Email list needs resorting
  NT_MAILBOX_UPDATE,  ///< Update internal tables
  NT_MAILBOX_UNTAG,   ///< Clear the 'last-tagged' pointer
};

/**
 * struct EventMailbox - An Event that happened to a Mailbox
 */
struct EventMailbox
{
  struct Mailbox *mailbox; ///< The Mailbox this Event relates to
};

void            mailbox_changed   (struct Mailbox *m, enum NotifyMailbox action);
struct Mailbox *mailbox_find      (const char *path);
struct Mailbox *mailbox_find_name (const char *name);
void            mailbox_free      (struct Mailbox **ptr);
struct Mailbox *mailbox_new       (struct Path *path);
bool            mailbox_set_subset(struct Mailbox *m, struct ConfigSubset *sub);
void            mailbox_size_add  (struct Mailbox *m, const struct Email *e);
void            mailbox_size_sub  (struct Mailbox *m, const struct Email *e);
void            mailbox_update    (struct Mailbox *m);

/**
 * mailbox_path - Get the Mailbox's path string
 * @param m Mailbox
 * @retval ptr Path string
 */
static inline const char *mailbox_path(const struct Mailbox *m) // LCOV_EXCL_LINE
{
  return m->path->orig; // LCOV_EXCL_LINE
}

#endif /* MUTT_CORE_MAILBOX_H */
