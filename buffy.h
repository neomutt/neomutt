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
#include "where.h"

struct stat;

/* parameter to mutt_parse_mailboxes */
#define MUTT_NAMED   1
#define MUTT_VIRTUAL 2

/**
 * struct Buffy - A mailbox
 */
struct Buffy
{
  char path[_POSIX_PATH_MAX];
  char realpath[_POSIX_PATH_MAX]; /**< used for duplicate detection, context
                                   * comparison, and the sidebar */
  char *desc;
  off_t size;
  struct Buffy *next;
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

WHERE struct Buffy *Incoming;
WHERE short MailCheck;
WHERE short MailCheckStatsInterval;

#ifdef USE_NOTMUCH
void mutt_buffy_vfolder(char *s, size_t slen);
#endif

extern time_t BuffyDoneTime; /**< last time we knew for sure how much mail there was */

struct Buffy *mutt_find_mailbox(const char *path);
void mutt_update_mailbox(struct Buffy *b);

/** fixes up atime + mtime after mbox/mmdf mailbox was modified
 * according to stat() info taken before a modification */
void mutt_buffy_cleanup(const char *buf, struct stat *st);

/** mark mailbox just left as already notified */
void mutt_buffy_setnotified(const char *path);

bool mh_buffy(struct Buffy *mailbox, bool check_stats);

#endif /* _MUTT_BUFFY_H */
