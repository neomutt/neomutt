/**
 * @file
 * Maildir local mailbox type
 *
 * @authors
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
 * @page maildir MAILDIR: Local mailbox type
 *
 * Maildir local mailbox type
 *
 * | File              | Description              |
 * | :---------------- | :----------------------- |
 * | maildir/maildir.c | @subpage maildir_maildir |
 * | maildir/mh.c      | @subpage maildir_mh      |
 * | maildir/path.c    | @subpage maildir_path    |
 * | maildir/shared.c  | @subpage maildir_shared  |
 */

#ifndef MUTT_MAILDIR_LIB_H
#define MUTT_MAILDIR_LIB_H

#include <stdbool.h>
#include <stdio.h>
#include "core/lib.h"
#include "mx.h"
#include "path.h"
#include "hcache/lib.h"

struct Email;

/* These Config Variables are only used in maildir/maildir.c */
extern bool  C_MaildirCheckCur;

/* These Config Variables are only used in maildir/mh.c */
extern bool  C_CheckNew;
extern bool  C_MaildirHeaderCacheVerify;
extern bool  C_MhPurge;
extern char *C_MhSeqFlagged;
extern char *C_MhSeqReplied;
extern char *C_MhSeqUnseen;

extern struct MxOps MxMaildirOps;
extern struct MxOps MxMhOps;

int           maildir_check_empty      (const char *path);
void          maildir_gen_flags        (char *dest, size_t destlen, struct Email *e);
int           maildir_msg_open_new     (struct Mailbox *m, struct Message *msg, struct Email *e);
FILE *        maildir_open_find_message(const char *folder, const char *msg, char **newname);
void          maildir_parse_flags      (struct Email *e, const char *path);
struct Email *maildir_parse_message    (enum MailboxType type, const char *fname, bool is_old, struct Email *e);
struct Email *maildir_parse_stream     (enum MailboxType type, FILE *fp, const char *fname, bool is_old, struct Email *e);
bool          maildir_update_flags     (struct Mailbox *m, struct Email *e_old, struct Email *e_new);
int           mh_check_empty           (const char *path);
int           mh_sync_mailbox_message  (struct Mailbox *m, int msgno, header_cache_t *hc);

#endif /* MUTT_MAILDIR_LIB_H */
