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

#ifndef MUTT_MAILDIR_MAILDIR_PRIVATE_H
#define MUTT_MAILDIR_MAILDIR_PRIVATE_H

#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include "config/lib.h"

struct Account;
struct Buffer;
struct Context;
struct Email;
struct Mailbox;
struct Message;
struct Progress;

/**
 * struct MaildirMboxData - Maildir-specific Mailbox data - @extends Mailbox
 */
struct MaildirMboxData
{
  struct timespec mtime_cur;
  mode_t mh_umask;
};

/**
 * struct Maildir - A Maildir mailbox
 */
struct Maildir
{
  struct Email *email;
  char *canon_fname;
  bool header_parsed : 1;
  ino_t inode;
  struct Maildir *next;
};

/**
 * struct MhSequences - Set of MH sequence numbers
 */
struct MhSequences
{
  int max;
  short *flags;
};

#define MH_SEQ_UNSEEN (1 << 0)
#define MH_SEQ_REPLIED (1 << 1)
#define MH_SEQ_FLAGGED (1 << 2)

/* MXAPI shared functions */
int             maildir_ac_add     (struct Account *a, struct Mailbox *m);
struct Account *maildir_ac_find    (struct Account *a, const char *path);
int             maildir_mbox_check (struct Mailbox *m, int *index_hint);
int             maildir_path_canon (char *buf, size_t buflen);
int             maildir_path_parent(char *buf, size_t buflen);
int             maildir_path_pretty(char *buf, size_t buflen, const char *folder);
int             mh_mbox_check      (struct Mailbox *m, int *index_hint);
int             mh_mbox_close      (struct Mailbox *m);
int             mh_mbox_sync       (struct Mailbox *m, int *index_hint);
int             mh_msg_close       (struct Mailbox *m, struct Message *msg);

/* Maildir/MH shared functions */
void                    maildir_canon_filename (struct Buffer *dest, const char *src);
void                    maildir_delayed_parsing(struct Mailbox *m, struct Maildir **md, struct Progress *progress);
struct MaildirMboxData *maildir_mdata_get      (struct Mailbox *m);
int                     maildir_mh_open_message(struct Mailbox *m, struct Message *msg, int msgno, bool is_maildir);
int                     maildir_move_to_context(struct Mailbox *m, struct Maildir **md);
int                     maildir_parse_dir      (struct Mailbox *m, struct Maildir ***last, const char *subdir, int *count, struct Progress *progress);
void                    maildir_parse_flags    (struct Email *e, const char *path);
struct Email *          maildir_parse_message  (enum MailboxType magic, const char *fname, bool is_old, struct Email *e);
bool                    maildir_update_flags   (struct Mailbox *m, struct Email *o, struct Email *n);
void                    maildir_update_tables  (struct Context *ctx, int *index_hint);
int                     md_commit_message      (struct Mailbox *m, struct Message *msg, struct Email *e);
int                     mh_commit_msg          (struct Mailbox *m, struct Message *msg, struct Email *e, bool updseq);
int                     mh_mkstemp             (struct Mailbox *m, FILE **fp, char **tgt);
int                     mh_read_dir            (struct Mailbox *m, const char *subdir);
int                     mh_read_sequences      (struct MhSequences *mhs, const char *path);
short                   mhs_check              (struct MhSequences *mhs, int i);
void                    mhs_free_sequences     (struct MhSequences *mhs);
short                   mhs_set                (struct MhSequences *mhs, int i, short f);
mode_t                  mh_umask               (struct Mailbox *m);
void                    mh_update_maildir      (struct Maildir *md, struct MhSequences *mhs);
void                    mh_update_sequences    (struct Mailbox *m);
bool                    mh_valid_message       (const char *s);

int mh_sync_message(struct Mailbox *m, int msgno);
int maildir_sync_message(struct Mailbox *m, int msgno);
int mh_rewrite_message(struct Mailbox *m, int msgno);

#endif /* MUTT_MAILDIR_MAILDIR_PRIVATE_H */
