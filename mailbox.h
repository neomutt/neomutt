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

#ifndef _MUTT_MAILBOX_H
#define _MUTT_MAILBOX_H

#include <limits.h>
#include <stdbool.h>
#include <sys/types.h>
#include <time.h>
#include "mutt/mutt.h"
#include "where.h"

struct Buffer;
struct stat;

/* These Config Variables are only used in mailbox.c */
extern short MailCheck;
extern bool  MailCheckStats;
extern short MailCheckStatsInterval;
extern bool  MaildirCheckCur;

/* parameter to mutt_parse_mailboxes */
#define MUTT_NAMED   1
#define MUTT_VIRTUAL 2

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
  bool new; /**< mailbox has new mail */

  /* These next three are only set when MailCheckStats is set */
  int msg_count;   /**< total number of messages */
  int msg_unread;  /**< number of unread messages */
  int msg_flagged; /**< number of flagged messages */

  bool notified;             /**< user has been notified */
  enum MailboxType magic;    /**< mailbox type */
  bool newly_created;        /**< mbox or mmdf just popped into existence */
  struct timespec last_visited; /**< time of last exit from this mailbox */
  struct timespec stats_last_checked; /**< mtime of mailbox the last time stats where checked. */
};

/**
 * struct MailboxNode - List of Mailboxes
 */
struct MailboxNode
{
  struct Mailbox *b;
  STAILQ_ENTRY(MailboxNode) entries;
};

STAILQ_HEAD(MailboxList, MailboxNode);

extern struct MailboxList AllMailboxes;

#ifdef USE_NOTMUCH
void mutt_mailbox_vfolder(char *buf, size_t buflen);
#endif

struct Mailbox *mutt_find_mailbox(const char *path);
void mutt_update_mailbox(struct Mailbox *b);

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

#endif /* _MUTT_MAILBOX_H */
