/*
 * Copyright (C) 1996-2002 Michael R. Elkins <me@mutt.org>
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

#ifndef _MAILBOX_H
#define _MAILBOX_H

/* flags for mutt_open_mailbox() */
#define MUTT_NOSORT     (1<<0) /* do not sort the mailbox after opening it */
#define MUTT_APPEND     (1<<1) /* open mailbox for appending messages */
#define MUTT_READONLY   (1<<2) /* open in read-only mode */
#define MUTT_QUIET      (1<<3) /* do not print any messages */
#define MUTT_NEWFOLDER  (1<<4) /* create a new folder - same as MUTT_APPEND, but uses
                                * safe_fopen() with mode "w" for mbox-style folders.
                                * This will truncate an existing file. */
#define MUTT_PEEK       (1<<5) /* revert atime back after taking a look (if applicable) */
#define MUTT_APPENDNEW  (1<<6) /* set in mx_open_mailbox_append if the mailbox doesn't
                                * exist. used by maildir/mh to create the mailbox. */

/* mx_open_new_message() */
#define MUTT_ADD_FROM   (1<<0)  /* add a From_ line */
#define MUTT_SET_DRAFT  (1<<1)  /* set the message draft flag */

/* return values from mx_check_mailbox() */
enum
{
  MUTT_NEW_MAIL = 1,    /* new mail received in mailbox */
  MUTT_LOCKED,          /* couldn't lock the mailbox */
  MUTT_REOPENED,        /* mailbox was reopened */
  MUTT_FLAGS            /* nondestructive flags change (IMAP) */
};

typedef struct _message
{
  FILE *fp;	/* pointer to the message data */
  char *path;	/* path to temp file */
  short write;	/* nonzero if message is open for writing */
  struct {
    unsigned read : 1;
    unsigned flagged : 1;
    unsigned replied : 1;
    unsigned draft : 1;
  } flags;
  time_t received;	/* the time at which this message was received */
} MESSAGE;

CONTEXT *mx_open_mailbox (const char *, int, CONTEXT *);

MESSAGE *mx_open_message (CONTEXT *, int);
MESSAGE *mx_open_new_message (CONTEXT *, HEADER *, int);

void mx_fastclose_mailbox (CONTEXT *);

int mx_close_mailbox (CONTEXT *, int *);
int mx_sync_mailbox (CONTEXT *, int *);
int mx_commit_message (MESSAGE *, CONTEXT *);
int mx_close_message (CONTEXT *, MESSAGE **);
int mx_get_magic (const char *);
int mx_set_magic (const char *);
int mx_check_mailbox (CONTEXT *, int *);
#ifdef USE_IMAP
int mx_is_imap (const char *);
#endif
#ifdef USE_POP
int mx_is_pop (const char *);
#endif

int mx_access (const char*, int);
int mx_check_empty (const char *);

int mx_is_maildir (const char *);
int mx_is_mh (const char *);

#endif
