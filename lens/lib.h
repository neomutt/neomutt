#pragma once

#include "mutt/lib.h"
#include "email/email.h"
#include "email/thread.h"
#include "core/account.h"
#include "core/mailbox.h"

enum LensMailboxType
{
  LENS_TYPE_MAILDIR,
  LENS_TYPE_MBOX,
  LENS_TYPE_IMAP,
  LENS_TYPE_POP
};

struct LensEmail
{
  const struct Email *email;
  const struct Mailbox *mailbox;
  STAILQ_ENTRY(LensEmail) entries; ///< Linked list
};
STAILQ_HEAD(LensEmailList, LensEmail);

struct LensMailbox
{
  struct Buffer pathbuf; ///< Path of the Mailbox
  char *realpath; ///< Used for duplicate detection, context comparison, and the sidebar
  char *name;               ///< A short name for the Mailbox
  struct ConfigSubset *sub; ///< Inherited config items
  off_t size;               ///< Size of the Mailbox
  bool has_new;             ///< Mailbox has new mail

  // These next three are only set when MailCheckStats is set
  int msg_count;   ///< Total number of messages
  int msg_unread;  ///< Number of unread messages
  int msg_flagged; ///< Number of flagged messages

  int msg_new;     ///< Number of new messages
  int msg_deleted; ///< Number of deleted messages
  int msg_tagged;  ///< How many messages are tagged?

  struct LensEmailList emails; ///< Linked list of emails
  int *v2r;                    ///< Mapping from virtual to real msgno
  int vcount;                  ///< The number of virtual messages

  bool notified;                ///< User has been notified
  enum LensMailboxType type;    ///< Mailbox type
  bool newly_created;           ///< Mbox or mmdf just popped into existence
  struct timespec mtime;        ///< Time Mailbox was last changed
  struct timespec last_visited; ///< Time of last exit from this mailbox
  struct timespec stats_last_checked; ///< Mtime of mailbox the last time stats where checked.

  bool append : 1;                 ///< Mailbox is opened in append mode
  bool changed : 1;                ///< Mailbox has been modified
  bool dontwrite : 1;              ///< Don't write the mailbox on close
  bool first_check_stats_done : 1; ///< True when the check have been done at least on time
  bool peekonly : 1;               ///< Just taking a glance, revert atime
  bool verbose : 1;                ///< Display status messages?
  bool readonly : 1;               ///< Don't allow changes to the mailbox

  struct HashTable *id_hash;    ///< Hash Table by msg id
  struct HashTable *subj_hash;  ///< Hash Table by subject
  struct HashTable *label_hash; ///< Hash Table for x-labels

  struct AccountList accounts; ///< The accounts this LensMailbox is associated with

  int opened; ///< Number of times mailbox is opened

  uint8_t flags; ///< e.g. #MB_NORMAL

  struct Notify *notify; ///< Notifications: #NotifyMailbox, #EventMailbox

  int gen; ///< Generation number, for sorting
};
