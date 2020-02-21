/**
 * @file
 * IMAP network mailbox
 *
 * @authors
 * Copyright (C) 1996-1998 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2000-2007,2017 Brendan Cully <brendan@kublai.com>
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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
 * @page imap IMAP: Network Mailbox
 *
 * IMAP network mailbox
 *
 * | File              | Description              |
 * | :---------------- | :----------------------- |
 * | imap/imap.c       | @subpage imap_imap       |
 * | imap/auth_anon.c  | @subpage imap_auth_anon  |
 * | imap/auth.c       | @subpage imap_auth       |
 * | imap/auth_cram.c  | @subpage imap_auth_cram  |
 * | imap/auth_gss.c   | @subpage imap_auth_gss   |
 * | imap/auth_login.c | @subpage imap_auth_login |
 * | imap/auth_oauth.c | @subpage imap_auth_oauth |
 * | imap/auth_plain.c | @subpage imap_auth_plain |
 * | imap/auth_sasl.c  | @subpage imap_auth_sasl  |
 * | imap/browse.c     | @subpage imap_browse     |
 * | imap/command.c    | @subpage imap_command    |
 * | imap/message.c    | @subpage imap_message    |
 * | imap/path.c       | @subpage imap_path       |
 * | imap/search.c     | @subpage imap_search     |
 * | imap/utf7.c       | @subpage imap_utf7       |
 * | imap/util.c       | @subpage imap_util       |
 */

#ifndef MUTT_IMAP_LIB_H
#define MUTT_IMAP_LIB_H

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>
#include "core/lib.h"
#include "mx.h"
#include "path.h"

struct BrowserState;
struct Buffer;
struct ConnAccount;
struct EmailList;
struct Path;
struct PatternList;
struct stat;

/* These Config Variables are only used in imap/auth.c */
extern struct Slist *C_ImapAuthenticators;

/* These Config Variables are only used in imap/imap.c */
#ifdef USE_ZLIB
extern bool C_ImapDeflate;
#endif
extern bool C_ImapIdle;
extern bool C_ImapRfc5161;

/* These Config Variables are only used in imap/message.c */
extern char *C_ImapHeaders;
extern long C_ImapFetchChunkSize;

/* These Config Variables are only used in imap/command.c */
extern bool C_ImapServernoise;

/* These Config Variables are only used in imap/util.c */
extern char *C_ImapDelimChars;
extern char *C_ImapLogin;
extern char *C_ImapOauthRefreshCommand;
extern char *C_ImapPass;
extern short C_ImapPipelineDepth;

/* imap.c */
int imap_access(const char *path);
int imap_check_mailbox(struct Mailbox *m, bool force);
int imap_delete_mailbox(struct Mailbox *m, char *path);
int imap_sync_mailbox(struct Mailbox *m, bool expunge, bool close);
int imap_path_status(const char *path, bool queue);
int imap_mailbox_status(struct Mailbox *m, bool queue);
int imap_subscribe(char *path, bool subscribe);
int imap_complete(char *buf, size_t buflen, const char *path);
int imap_fast_trash(struct Mailbox *m, char *dest);
enum MailboxType imap_path_probe(const char *path, const struct stat *st);
int imap_path_canon(char *buf, size_t buflen);

extern struct MxOps MxImapOps;

/* browse.c */
int imap_browse(const char *path, struct BrowserState *state);
int imap_mailbox_create(const char *folder);
int imap_mailbox_rename(const char *path);

/* message.c */
int imap_copy_messages(struct Mailbox *m, struct EmailList *el, const char *dest, bool delete_original);

/* socket.c */
void imap_logout_all(void);

/* util.c */
int imap_expand_path(struct Buffer *buf);
int imap_parse_path(const char *path, struct ConnAccount *cac, char *mailbox, size_t mailboxlen);
void imap_pretty_mailbox(char *path, size_t pathlen, const char *folder);
int imap_mxcmp(const char *mx1, const char *mx2);

int imap_wait_keepalive(pid_t pid);
void imap_keepalive(void);

void imap_get_parent_path(const char *path, char *buf, size_t buflen);
void imap_clean_path(char *path, size_t plen);

/* search.c */
bool imap_search(struct Mailbox *m, const struct PatternList *pat);

#endif /* MUTT_IMAP_LIB_H */
