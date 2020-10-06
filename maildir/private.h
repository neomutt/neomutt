/**
 * @file
 * Maildir/MH private types
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

#ifndef MUTT_MAILDIR_PRIVATE_H
#define MUTT_MAILDIR_PRIVATE_H

#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include "maildir/lib.h"
#include "sequence.h"

struct Account;
struct Buffer;
struct Email;
struct Mailbox;
struct MdEmail;
struct Message;
struct Progress;

extern bool  C_CheckNew;
extern bool  C_MaildirCheckCur;
extern bool  C_MaildirHeaderCacheVerify;
extern bool  C_MhPurge;
extern char *C_MhSeqFlagged;
extern char *C_MhSeqReplied;
extern char *C_MhSeqUnseen;

int             maildir_commit_message (struct Mailbox *m, struct Message *msg, struct Email *e);
void            maildir_free           (struct MdEmail **md);
size_t          maildir_hcache_keylen  (const char *fn);
int             maildir_mbox_check     (struct Mailbox *m);
int             maildir_move_to_mailbox(struct Mailbox *m, struct MdEmail **ptr);
struct MdEmail *maildir_sort           (struct MdEmail *list, size_t len, int (*cmp)(struct MdEmail *, struct MdEmail *));
int             maildir_sync_message   (struct Mailbox *m, int msgno);
int             md_cmp_inode           (struct MdEmail *a, struct MdEmail *b);
int             mh_cmp_path            (struct MdEmail *a, struct MdEmail *b);
int             mh_commit_msg          (struct Mailbox *m, struct Message *msg, struct Email *e, bool updseq);
void            mh_delayed_parsing     (struct Mailbox *m, struct MdEmail **md, struct Progress *progress);
int             mh_mbox_check          (struct Mailbox *m);
int             mh_mkstemp             (struct Mailbox *m, FILE **fp, char **tgt);
int             mh_parse_dir           (struct Mailbox *m, struct MdEmail ***last, const char *subdir, int *count, struct Progress *progress);
void            mh_sort_natural        (struct Mailbox *m, struct MdEmail **md);
int             mh_sync_message        (struct Mailbox *m, int msgno);
mode_t          mh_umask               (struct Mailbox *m);
void            mh_update_maildir      (struct MdEmail *md, struct MhSequences *mhs);
void            mh_update_mtime        (struct Mailbox *m);
bool            mh_valid_message       (const char *s);
struct MdEmail *skip_duplicates        (struct MdEmail *p, struct MdEmail **last);

#endif /* MUTT_MAILDIR_PRIVATE_H */
