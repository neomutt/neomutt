/**
 * @file
 * IMAP network mailbox
 *
 * @authors
 * Copyright (C) 1996-1998 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2000-2007,2017 Brendan Cully <brendan@kublai.com>
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

/**
 * @page imap IMAP Network Mailbox
 *
 * IMAP network mailbox
 *
 * -# @subpage imap_imap
 * -# @subpage imap_auth_anon
 * -# @subpage imap_auth
 * -# @subpage imap_auth_crap
 * -# @subpage imap_auth_gss
 * -# @subpage imap_auth_login
 * -# @subpage imap_auth_plain
 * -# @subpage imap_auth_sasl
 * -# @subpage imap_browse
 * -# @subpage imap_command
 * -# @subpage imap_message
 * -# @subpage imap_utf7
 * -# @subpage imap_util
 */

#ifndef _IMAP_IMAP_H
#define _IMAP_IMAP_H

#include "conn/conn.h"
#include "mutt_account.h"
#include <stddef.h>
#include <sys/types.h>

struct BrowserState;
struct Context;
struct Header;
struct Message;
struct Pattern;

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
int imap_buffy_check(int check_stats);
int imap_status(char *path, int queue);
int imap_search(struct Context *ctx, const struct Pattern *pat);
int imap_subscribe(char *path, bool subscribe);
int imap_complete(char *dest, size_t dlen, char *path);
int imap_fast_trash(struct Context *ctx, char *dest);

extern struct MxOps mx_imap_ops;

/* browse.c */
int imap_browse(char *path, struct BrowserState *state);
int imap_mailbox_create(const char *folder);
int imap_mailbox_rename(const char *mailbox);

/* message.c */
int imap_copy_messages(struct Context *ctx, struct Header *h, char *dest, int delete);

/* socket.c */
void imap_logout_all(void);

/* util.c */
int imap_expand_path(char *path, size_t len);
int imap_parse_path(const char *path, struct ImapMbox *mx);
void imap_pretty_mailbox(char *path);

int imap_wait_keepalive(pid_t pid);
void imap_keepalive(void);

void imap_get_parent_path(char *output, const char *path, size_t olen);
void imap_clean_path(char *path, size_t plen);

#endif /* _IMAP_IMAP_H */
