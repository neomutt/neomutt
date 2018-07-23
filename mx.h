/**
 * @file
 * API for mailboxes
 *
 * @authors
 * Copyright (C) 1996-2002,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2002 Thomas Roessler <roessler@does-not-exist.org>
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

#ifndef _MUTT_MX_H
#define _MUTT_MX_H

#include <stdbool.h>
#include <stdio.h>
#include "where.h"
#ifdef USE_HCACHE
#include "hcache/hcache.h"
#endif

struct Header;
struct Context;
struct Message;
struct stat;

/* These Config Variables are only used in mx.c */
extern unsigned char CatchupNewsgroup;
extern bool          KeepFlagged;
extern unsigned char Move;
extern char *        Trash;

/**
 * struct MxOps - The Mailbox API
 *
 * Each backend provides a set of functions through which the mailbox, messages
 * and tags are manipulated.
 */
struct MxOps
{
  /**
   * mbox_open - Open a mailbox
   * @param ctx Mailbox to open
   * @retval  0 Success
   * @retval -1 Error
   */
  int (*mbox_open)       (struct Context *ctx);
  /**
   * mbox_open_append - Open a mailbox for appending
   * @param ctx   Mailbox to open
   * @param flags e.g. #MUTT_READONLY
   * @retval  0 Success
   * @retval -1 Failure
   */
  int (*mbox_open_append)(struct Context *ctx, int flags);
  /**
   * mbox_check - Check for new mail
   * @param ctx Mailbox
   * @param index_hint Remember our place in the index
   * @retval >0 Success, e.g. #MUTT_REOPENED
   * @retval -1 Error
   */
  int (*mbox_check)      (struct Context *ctx, int *index_hint);
  /**
   * mbox_sync - Save changes to the mailbox
   * @param ctx        Mailbox to sync
   * @param index_hint Remember our place in the index
   * @retval  0 Success
   * @retval -1 Failure
   */
  int (*mbox_sync)       (struct Context *ctx, int *index_hint);
  /**
   * mbox_close - Close a mailbox
   * @param ctx Mailbox to close
   * @retval  0 Success
   * @retval -1 Failure
   */
  int (*mbox_close)      (struct Context *ctx);
  /**
   * msg_open - Open an email message in mailbox
   * @param ctx   Mailbox
   * @param msg   Message to open
   * @param msgno Index of message to open
   * @retval 0 Success
   * @retval 1 Error
   */
  int (*msg_open)        (struct Context *ctx, struct Message *msg, int msgno);
  /**
   * msg_open_new - Open a new message in a mailbox
   * @param ctx Mailbox
   * @param msg  Message to open
   * @param hdr Email header
   * @retval  0 Success
   * @retval -1 Failure
   */
  int (*msg_open_new)    (struct Context *ctx, struct Message *msg, struct Header *hdr);
  /**
   * msg_commit - Save changes to an email
   * @param ctx Mailbox
   * @param msg Message to commit
   * @retval  0 Success
   * @retval -1 Failure
   */
  int (*msg_commit)      (struct Context *ctx, struct Message *msg);
  /**
   * msg_close - Close an email
   * @param ctx Mailbox
   * @param msg Message to close
   * @retval  0 Success
   * @retval -1 Failure
   */
  int (*msg_close)       (struct Context *ctx, struct Message *msg);
  /**
   * tags_commit - Save the tags to a message
   * @param ctx Mailbox
   * @param hdr Email Header
   * @param buf Buffer containing tags
   * @retval  0 Success
   * @retval -1 Failure
   */
  int (*tags_edit)       (struct Context *ctx, const char *tags, char *buf, size_t buflen);
  /**
   * tags_edit - Prompt and validate new messages tags
   * @param ctx    Mailbox
   * @param tags   Existing tags
   * @param buf    Buffer to store the tags
   * @param buflen Length of buffer
   * @retval -1 Error
   * @retval  0 No valid user input
   * @retval  1 Buf set
   */
  int (*tags_commit)     (struct Context *ctx, struct Header *hdr, char *buf);
};

#define MMDF_SEP "\001\001\001\001\n"

void mbox_reset_atime(struct Context *ctx, struct stat *st);

#ifdef USE_NOTMUCH
bool mx_is_notmuch(const char *p);
#endif

bool mx_tags_is_supported(struct Context *ctx);

void mx_alloc_memory(struct Context *ctx);
void mx_update_context(struct Context *ctx, int new_messages);
void mx_update_tables(struct Context *ctx, bool committing);

struct MxOps *mx_get_ops(int magic);

/* This variable is backing for a config item */
WHERE short MboxType;

#endif /* _MUTT_MX_H */
