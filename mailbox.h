/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
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
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */ 

/* flags for mutt_open_mailbox() */
#define M_NOSORT	(1<<0) /* do not sort the mailbox after opening it */
#define M_APPEND	(1<<1) /* open mailbox for appending messages */
#define M_READONLY	(1<<2) /* open in read-only mode */
#define M_QUIET		(1<<3) /* do not print any messages */

/* mx_open_new_message() */
#define M_ADD_FROM	1	/* add a From_ line */

/* return values from mx_check_mailbox() */
enum
{
  M_NEW_MAIL = 1,	/* new mail received in mailbox */
  M_LOCKED,		/* couldn't lock the mailbox */
  M_REOPENED		/* mailbox was reopened */
};

typedef struct
{
  FILE *fp;	/* pointer to the message data */
#ifdef USE_IMAP
  CONTEXT *ctx;	/* context (mailbox) for this message */
  char *path;	/* path to temp file */
#endif /* USE_IMAP */
  short magic;	/* type of mailbox this message belongs to */
  short write;	/* nonzero if message is open for writing */
} MESSAGE;

CONTEXT *mx_open_mailbox (const char *, int, CONTEXT *);

MESSAGE *mx_open_message (CONTEXT *, int);
MESSAGE *mx_open_new_message (CONTEXT *, HEADER *, int);

void mx_fastclose_mailbox (CONTEXT *);

int mx_close_mailbox (CONTEXT *);
int mx_sync_mailbox (CONTEXT *);
int mx_close_message (MESSAGE **msg);
int mx_get_magic (const char *);
int mx_set_magic (const char *);
int mx_check_mailbox (CONTEXT *, int *);
#ifdef USE_IMAP
int mx_is_imap (const char *);
#endif

