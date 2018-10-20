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
 * | File         | Description              |
 * | :----------- | :----------------------- |
 * | maildir/mh.c | @subpage maildir_maildir |
 */

#ifndef MUTT_MAILDIR_MAILDIR_H
#define MUTT_MAILDIR_MAILDIR_H

#include <stdbool.h>
#include <stdio.h>
#include "mx.h"
#ifdef USE_HCACHE
#include "hcache/hcache.h"
#endif

struct Mailbox;
struct Context;
struct Email;

/* These Config Variables are only used in maildir/mh.c */
extern bool  CheckNew;
extern bool  MaildirHeaderCacheVerify;
extern bool  MhPurge;
extern char *MhSeqFlagged;
extern char *MhSeqReplied;
extern char *MhSeqUnseen;

extern struct MxOps mx_maildir_ops;
extern struct MxOps mx_mh_ops;

int           maildir_check_empty(const char *path);
void          maildir_gen_flags(char *dest, size_t destlen, struct Email *e);
FILE *        maildir_open_find_message(const char *folder, const char *msg, char **newname);
void          maildir_parse_flags(struct Email *e, const char *path);
struct Email *maildir_parse_message(enum MailboxType magic, const char *fname, bool is_old, struct Email *e);
struct Email *maildir_parse_stream(enum MailboxType magic, FILE *f, const char *fname, bool is_old, struct Email *e);
bool          maildir_update_flags(struct Context *ctx, struct Email *o, struct Email *n);

bool          mh_mailbox(struct Mailbox *m, bool check_stats);
int           mh_check_empty(const char *path);

int           maildir_path_probe(const char *path, const struct stat *st);
int           mh_path_probe(const char *path, const struct stat *st);

#ifdef USE_HCACHE
int           mh_sync_mailbox_message(struct Context *ctx, int msgno, header_cache_t *hc);
#else
int           mh_sync_mailbox_message(struct Context *ctx, int msgno);
#endif

#endif /* MUTT_MAILDIR_MAILDIR_H */
