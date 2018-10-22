/**
 * @file
 * Representation of a mailbox
 *
 * @authors
 * Copyright (C) 1996-2000,2010,2013 Michael R. Elkins <me@mutt.org>
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
#include "where.h"

struct Buffer;
struct Context;
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

  RIGHTSMAX
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

  struct Email **hdrs;
  int hdrmax;               /**< number of pointers in hdrs */
  int *v2r;                 /**< mapping from virtual to real msgno */
  int vcount;               /**< the number of virtual messages */

  bool notified;             /**< user has been notified */
  enum MailboxType magic;    /**< mailbox type */
  bool newly_created;        /**< mbox or mmdf just popped into existence */
  struct timespec mtime;
  struct timespec last_visited;       /**< time of last exit from this mailbox */
  struct timespec stats_last_checked; /**< mtime of mailbox the last time stats where checked. */

  const struct MxOps *mx_ops;

  bool changed : 1;   /**< mailbox has been modified */
  bool readonly : 1;  /**< don't allow changes to the mailbox */
  bool quiet : 1;     /**< inhibit status messages? */
  bool closing : 1;   /**< mailbox is being closed */

  unsigned char rights[(RIGHTSMAX + 7) / 8]; /**< ACL bits */

#ifdef USE_COMPRESSED
  void *compress_info; /**< compressed mbox module private data */
#endif

  struct Hash *id_hash;     /**< hash table by msg id */
  struct Hash *subj_hash;   /**< hash table by subject */
  struct Hash *label_hash;  /**< hash table for x-labels */

  int flags; /**< e.g. #MB_NORMAL */

  void *mdata;                 /**< driver specific data */
  void (*free_mdata)(void **); /**< driver-specific data free function */
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

extern struct MailboxList AllMailboxes;

#ifdef USE_NOTMUCH
void mutt_mailbox_vfolder(char *buf, size_t buflen);
#endif

struct Mailbox *mailbox_new(const char *path);
void            mailbox_free(struct Mailbox **m);
void            mutt_context_free(struct Context **ctx);

struct Mailbox *mutt_find_mailbox(const char *path);
void mutt_update_mailbox(struct Mailbox *m);

void mutt_mailbox_cleanup(const char *path, struct stat *st);

/** mark mailbox just left as already notified */
void mutt_mailbox_setnotified(const char *path);

/* force flags passed to mutt_mailbox_check() */
#define MUTT_MAILBOX_CHECK_FORCE       (1 << 0)
#define MUTT_MAILBOX_CHECK_FORCE_STATS (1 << 1)

void mutt_mailbox(char *s, size_t slen);
bool mutt_mailbox_list(void);
int mutt_mailbox_check(int force);
bool mutt_mailbox_notify(void);
int mutt_parse_mailboxes(struct Buffer *path, struct Buffer *s, unsigned long data, struct Buffer *err);
int mutt_parse_unmailboxes(struct Buffer *path, struct Buffer *s, unsigned long data, struct Buffer *err);

#endif /* MUTT_MAILBOX_H */
