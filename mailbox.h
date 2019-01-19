/**
 * @file
 * Representation of a mailbox
 *
 * @authors
 * Copyright (C) 1996-2000,2010,2013 Michael R. Elkins <me@mutt.org>
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

#ifndef MUTT_MAILBOX_H
#define MUTT_MAILBOX_H

#include <limits.h>
#include <stdbool.h>
#include <sys/types.h>
#include <time.h>
#include "mutt/mutt.h"
#include "mutt_commands.h"
#include "where.h"

struct Buffer;
struct Account;
struct stat;

/* These Config Variables are only used in mailbox.c */
extern short MailCheck;
extern bool  MailCheckStats;
extern short MailCheckStatsInterval;
extern bool  MaildirCheckCur;

/* parameter to mutt_parse_mailboxes */
#define MUTT_NAMED   (1 << 0)
#define MUTT_VIRTUAL (1 << 1)

#define MB_NORMAL 0
#define MB_HIDDEN 1

/**
 * enum MailboxNotification - Notifications about changes to a Mailbox
 */
enum MailboxNotification
{
  MBN_CLOSED = 1, ///< Mailbox was closed
  MBN_INVALID,    ///< Email list was changed
  MBN_RESORT,     ///< Email list needs resorting
  MBN_UPDATE,     ///< Update internal tables
  MBN_UNTAG,      ///< Clear the 'last-tagged' pointer
};

/**
 * enum AclRights - ACL Rights
 *
 * These show permission to...
 */
enum AclRights
{
  MUTT_ACL_ADMIN = 0, ///< administer the account (get/set permissions)
  MUTT_ACL_CREATE,    ///< create a mailbox
  MUTT_ACL_DELETE,    ///< delete a message
  MUTT_ACL_DELMX,     ///< delete a mailbox
  MUTT_ACL_EXPUNGE,   ///< expunge messages
  MUTT_ACL_INSERT,    ///< add/copy into the mailbox (used when editing a message)
  MUTT_ACL_LOOKUP,    ///< lookup mailbox (visible to 'list')
  MUTT_ACL_POST,      ///< post (submit messages to the server)
  MUTT_ACL_READ,      ///< read the mailbox
  MUTT_ACL_SEEN,      ///< change the 'seen' status of a message
  MUTT_ACL_WRITE,     ///< write to a message (for flagging, or linking threads)
  MUTT_ACL_MAX,
};

/**
 * struct Mailbox - A mailbox
 */
struct Mailbox
{
  char path[PATH_MAX];
  char realpath[PATH_MAX]; /**< used for duplicate detection, context
                            * comparison, and the sidebar */
  char *desc;
  off_t size;
  bool has_new; /**< mailbox has new mail */

  /* These next three are only set when MailCheckStats is set */
  int msg_count;             /**< total number of messages */
  int msg_unread;            /**< number of unread messages */
  int msg_flagged;           /**< number of flagged messages */
  int msg_new;               /**< number of new messages */
  int msg_deleted;           /**< number of deleted messages */
  int msg_tagged;            /**< how many messages are tagged? */

  struct Email **emails;
  int email_max;               /**< number of pointers in emails */
  int *v2r;                 /**< mapping from virtual to real msgno */
  int vcount;               /**< the number of virtual messages */

  bool notified;             /**< user has been notified */
  enum MailboxType magic;    /**< mailbox type */
  bool newly_created;        /**< mbox or mmdf just popped into existence */
  struct timespec mtime;
  struct timespec last_visited;       /**< time of last exit from this mailbox */
  struct timespec stats_last_checked; /**< mtime of mailbox the last time stats where checked. */

  const struct MxOps *mx_ops;

  bool append                 : 1; /**< mailbox is opened in append mode */
  bool changed                : 1; /**< mailbox has been modified */
  bool dontwrite              : 1; /**< don't write the mailbox on close */
  bool first_check_stats_done : 1; /**< true when the check have been done at least on time */
  bool peekonly               : 1; /**< just taking a glance, revert atime */
  bool quiet                  : 1; /**< inhibit status messages? */
  bool readonly               : 1; /**< don't allow changes to the mailbox */

  unsigned char rights[(MUTT_ACL_MAX + 7) / 8]; /**< ACL bits */

#ifdef USE_COMPRESSED
  void *compress_info; /**< compressed mbox module private data */
#endif

  struct Hash *id_hash;     /**< hash table by msg id */
  struct Hash *subj_hash;   /**< hash table by subject */
  struct Hash *label_hash;  /**< hash table for x-labels */

  struct Account *account;
  int opened;              /**< number of times mailbox is opened */

  int flags; /**< e.g. #MB_NORMAL */

  void *mdata;                 /**< driver specific data */
  void (*free_mdata)(void **); /**< driver-specific data free function */

  void (*notify)(struct Mailbox *m, enum MailboxNotification action); ///< Notification callback
  void *ndata; ///< Notification callback private data
};

/**
 * struct MailboxNode - List of Mailboxes
 */
struct MailboxNode
{
  struct Mailbox *m;
  STAILQ_ENTRY(MailboxNode) entries;
};

STAILQ_HEAD(MailboxList, MailboxNode);

extern struct MailboxList AllMailboxes; ///< List of all Mailboxes

struct Mailbox *mailbox_new(void);
void            mailbox_free(struct Mailbox **ptr);

struct Mailbox *mutt_find_mailbox(const char *path);
struct Mailbox *mutt_find_mailbox_desc(const char *desc);
void mutt_update_mailbox(struct Mailbox *m);

void mutt_mailbox_cleanup(const char *path, struct stat *st);

/** mark mailbox just left as already notified */
void mutt_mailbox_setnotified(struct Mailbox *m);

/* force flags passed to mutt_mailbox_check() */
#define MUTT_MAILBOX_CHECK_FORCE       (1 << 0)
#define MUTT_MAILBOX_CHECK_FORCE_STATS (1 << 1)

void mutt_mailbox(struct Mailbox *m_cur, char *s, size_t slen);
bool mutt_mailbox_list(void);
int mutt_mailbox_check(struct Mailbox *m_cur, int force);
bool mutt_mailbox_notify(struct Mailbox *m_cur);
enum CommandResult mutt_parse_mailboxes(struct Buffer *path, struct Buffer *s, unsigned long data, struct Buffer *err);
enum CommandResult mutt_parse_unmailboxes(struct Buffer *path, struct Buffer *s, unsigned long data, struct Buffer *err);
void mutt_mailbox_changed(struct Mailbox *m, enum MailboxNotification action);

#endif /* MUTT_MAILBOX_H */
