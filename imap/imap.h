/**
 * @file
 * IMAP network mailbox
 *
 * @authors
 * Copyright (C) 1996-1998 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2000-2007 Brendan Cully <brendan@kublai.com>
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

#ifndef _MUTT_IMAP_H
#define _MUTT_IMAP_H

#include <stddef.h>
#include <sys/types.h>
#include "account.h"

struct Header;
struct Pattern;
struct Context;
struct Message;
struct BrowserState;

/**
 * struct ImapMbox - An IMAP mailbox
 */
struct ImapMbox
{
  struct Account account;
  char *mbox;
};

/* imap.c */
int imap_access(const char *path);
int imap_check_mailbox(struct Context *ctx, int force);
int imap_delete_mailbox(struct Context *ctx, struct ImapMbox *mx);
int imap_sync_mailbox(struct Context *ctx, int expunge);
int imap_close_mailbox(struct Context *ctx);
int imap_buffy_check(int force, int check_stats);
int imap_status(char *path, int queue);
int imap_search(struct Context *ctx, const struct Pattern *pat);
int imap_subscribe(char *path, int subscribe);
int imap_complete(char *dest, size_t dlen, char *path);
int imap_fast_trash(struct Context *ctx, char *dest);

void imap_allow_reopen(struct Context *ctx);
void imap_disallow_reopen(struct Context *ctx);

extern struct MxOps mx_imap_ops;

/* browse.c */
int imap_browse(char *path, struct BrowserState *state);
int imap_mailbox_create(const char *folder);
int imap_mailbox_rename(const char *mailbox);

/* message.c */
int imap_append_message(struct Context *ctx, struct Message *msg);
int imap_copy_messages(struct Context *ctx, struct Header *h, char *dest, int delete);

/* socket.c */
void imap_logout_all(void);

/* util.c */
int imap_expand_path(char *path, size_t len);
int imap_parse_path(const char *path, struct ImapMbox *mx);
void imap_pretty_mailbox(char *path);

int imap_wait_keepalive(pid_t pid);
void imap_keepalive(void);

int imap_account_match(const struct Account *a1, const struct Account *a2);
void imap_get_parent(char *output, const char *mbox, size_t olen, char delim);
void imap_get_parent_path(char *output, const char *path, size_t olen);
void imap_clean_path(char *path, size_t plen);

#endif /* _MUTT_IMAP_H */
