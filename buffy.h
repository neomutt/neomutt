/* 
 * Copyright (C) 1996-2000,2010,2013 Michael R. Elkins <me@mutt.org>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _BUFFY_H
#define _BUFFY_H

/*parameter to mutt_parse_mailboxes*/
#define MUTT_MAILBOXES   1
#define MUTT_UNMAILBOXES 2 

typedef struct buffy_t
{
  char path[_POSIX_PATH_MAX];
  char realpath[_POSIX_PATH_MAX]; /* used for duplicate detection, context comparison,
                                     and the sidebar */
  off_t size;
  struct buffy_t *next;
  short new;			/* mailbox has new mail */

  /* These next three are only set when OPTMAILCHECKSTATS is set */
  int msg_count;		/* total number of messages */
  int msg_unread;		/* number of unread messages */
  int msg_flagged;		/* number of flagged messages */

  short notified;		/* user has been notified */
  short magic;			/* mailbox type */
  short newly_created;		/* mbox or mmdf just popped into existence */
  time_t last_visited;		/* time of last exit from this mailbox */
  time_t stats_last_checked;	/* mtime of mailbox the last time stats where checked. */
}
BUFFY;

WHERE BUFFY *Incoming INITVAL (0);
WHERE short BuffyTimeout INITVAL (3);
WHERE short BuffyCheckStatsInterval INITVAL (60);

extern time_t BuffyDoneTime;	/* last time we knew for sure how much mail there was */

BUFFY *mutt_find_mailbox (const char *path);
void mutt_update_mailbox (BUFFY * b);

/* fixes up atime + mtime after mbox/mmdf mailbox was modified
   according to stat() info taken before a modification */
void mutt_buffy_cleanup (const char *buf, struct stat *st);

/* mark mailbox just left as already notified */
void mutt_buffy_setnotified (const char *path);

int mh_buffy (BUFFY *, int);

#endif /* _BUFFY_H */
