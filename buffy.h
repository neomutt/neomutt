/* 
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
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

/*parameter to mutt_parse_mailboxes*/
#define M_MAILBOXES   1
#define M_UNMAILBOXES 2 

typedef struct buffy_t
{
  char path[_POSIX_PATH_MAX];
  char realpath[_POSIX_PATH_MAX];
  off_t size;
  struct buffy_t *next;
  struct buffy_t *prev;
  short new;			/* mailbox has new mail */
  int msgcount;			/* total number of messages */
  int msg_unread;		/* number of unread messages */
  int msg_flagged;		/* number of flagged messages */
  short notified;		/* user has been notified */
  short magic;			/* mailbox type */
  short newly_created;		/* mbox or mmdf just popped into existence */
  time_t last_visited;		/* time of last exit from this mailbox */
  time_t sb_last_checked;      /* time of last buffy check from sidebar */
}
BUFFY;

WHERE BUFFY *Incoming INITVAL (0);
WHERE short BuffyTimeout INITVAL (3);

extern time_t BuffyDoneTime;	/* last time we knew for sure how much mail there was */

BUFFY *mutt_find_mailbox (const char *path);
void mutt_update_mailbox (BUFFY * b);

/* fixes up atime + mtime after mbox/mmdf mailbox was modified
   according to stat() info taken before a modification */
void mutt_buffy_cleanup (const char *buf, struct stat *st);

/* mark mailbox just left as already notified */
void mutt_buffy_setnotified (const char *path);

void mh_buffy (BUFFY *);
