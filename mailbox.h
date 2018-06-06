/**
 * @file
 * Constants/structs for handling mailboxes
 *
 * @authors
 * Copyright (C) 1996-2002 Michael R. Elkins <me@mutt.org>
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

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

struct Header;
struct Context;

/* flags for mutt_open_mailbox() */
#define MUTT_NOSORT    (1 << 0) /**< do not sort the mailbox after opening it */
#define MUTT_APPEND    (1 << 1) /**< open mailbox for appending messages */
#define MUTT_READONLY  (1 << 2) /**< open in read-only mode */
#define MUTT_QUIET     (1 << 3) /**< do not print any messages */
#define MUTT_NEWFOLDER (1 << 4) /**< create a new folder - same as MUTT_APPEND, but uses
                                 * mutt_file_fopen() with mode "w" for mbox-style folders.
                                 * This will truncate an existing file. */
#define MUTT_PEEK      (1 << 5) /**< revert atime back after taking a look (if applicable) */
#define MUTT_APPENDNEW (1 << 6) /**< set in mx_open_mailbox_append if the mailbox doesn't
                                 * exist. used by maildir/mh to create the mailbox. */

/* mx_msg_open_new() */
#define MUTT_ADD_FROM  (1 << 0) /**< add a From_ line */
#define MUTT_SET_DRAFT (1 << 1) /**< set the message draft flag */

/**
 * enum MxCheckReturns - Return values from mx_mbox_check()
 */
enum MxCheckReturns
{
  MUTT_NEW_MAIL = 1, /**< new mail received in mailbox */
  MUTT_LOCKED,       /**< couldn't lock the mailbox */
  MUTT_REOPENED,     /**< mailbox was reopened */
  MUTT_FLAGS         /**< nondestructive flags change (IMAP) */
};

/**
 * struct Message - A local copy of an email
 */
struct Message
{
  FILE *fp;            /**< pointer to the message data */
  char *path;          /**< path to temp file */
  char *commited_path; /**< the final path generated by mx_msg_commit() */
  bool write;          /**< nonzero if message is open for writing */
  struct
  {
    bool read : 1;
    bool flagged : 1;
    bool replied : 1;
    bool draft : 1;
  } flags;
  time_t received; /**< the time at which this message was received */
};

/* The Mailbox API, see MxOps */
struct Context *mx_mbox_open   (const char *path, int flags, struct Context *pctx);
int             mx_mbox_check  (struct Context *ctx, int *index_hint);
int             mx_mbox_sync   (struct Context *ctx, int *index_hint);
int             mx_mbox_close  (struct Context *ctx, int *index_hint);
struct Message *mx_msg_open    (struct Context *ctx, int msgno);
struct Message *mx_msg_open_new(struct Context *ctx, struct Header *hdr, int flags);
int             mx_msg_commit  (struct Context *ctx, struct Message *msg);
int             mx_msg_close   (struct Context *ctx, struct Message **msg);
int             mx_tags_edit   (struct Context *ctx, const char *tags, char *buf, size_t buflen);
int             mx_tags_commit (struct Context *ctx, struct Header *hdr, char *tags);

void mx_fastclose_mailbox(struct Context *ctx);

int mx_get_magic(const char *path);
int mx_set_magic(const char *s);
#ifdef USE_IMAP
bool mx_is_imap(const char *p);
#endif
#ifdef USE_POP
bool mx_is_pop(const char *p);
#endif
#ifdef USE_NNTP
bool mx_is_nntp(const char *p);
#endif

int mx_access(const char *path, int flags);
int mx_check_empty(const char *path);

bool mx_is_maildir(const char *path);
bool mx_is_mh(const char *path);

#endif /* _MUTT_MAILBOX_H */
