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

#ifndef _MUTT_BUFFY_H
#define _MUTT_BUFFY_H

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
 * struct Buffy - A mailbox
 */
struct Buffy
{
  char path[PATH_MAX];
  char realpath[PATH_MAX]; /**< used for duplicate detection, context
                            * comparison, and the sidebar */
  char *desc;
  off_t size;
  bool new; /**< mailbox has new mail */

  /* These next three are only set when MailCheckStats is set */
  int msg_count;             /**< total number of messages */
  int msg_unread;            /**< number of unread messages */
  int msg_flagged;           /**< number of flagged messages */

  bool notified;             /**< user has been notified */
  short magic;               /**< mailbox type */
  bool newly_created;        /**< mbox or mmdf just popped into existence */
  time_t last_visited;       /**< time of last exit from this mailbox */
  time_t stats_last_checked; /**< mtime of mailbox the last time stats where checked. */
};

struct BuffyNode
{
  struct Buffy *b;
  STAILQ_ENTRY(BuffyNode) entries;
};

STAILQ_HEAD(BuffyList, BuffyNode);

extern struct BuffyList BuffyList;

#ifdef USE_NOTMUCH
void mutt_buffy_vfolder(char *buf, size_t buflen);
#endif

extern time_t BuffyDoneTime; /**< last time we knew for sure how much mail there was */

struct Buffy *mutt_find_mailbox(const char *path);
void mutt_update_mailbox(struct Buffy *b);

void mutt_buffy_cleanup(const char *path, struct stat *st);

/** mark mailbox just left as already notified */
void mutt_buffy_setnotified(const char *path);

/* force flags passed to mutt_buffy_check() */
#define MUTT_BUFFY_CHECK_FORCE       (1 << 0)
#define MUTT_BUFFY_CHECK_FORCE_STATS (1 << 1)

void mutt_buffy(char *s, size_t slen);
bool mutt_buffy_list(void);
int mutt_buffy_check(int force);
bool mutt_buffy_notify(void);
int mutt_parse_mailboxes(struct Buffer *path, struct Buffer *s, unsigned long data, struct Buffer *err);
int mutt_parse_unmailboxes(struct Buffer *path, struct Buffer *s, unsigned long data, struct Buffer *err);

#endif /* _MUTT_BUFFY_H */
