/*
 * Copyright (C) 1996-2002,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2002 Thomas Roessler <roessler@does-not-exist.org>
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

/*
 * This header file contains prototypes for internal functions used by the
 * generic mailbox api.  None of these functions should be called directly.
 */

#ifndef _MX_H
#define _MX_H

#include "mailbox.h"
#include "buffy.h"

/* supported mailbox formats */
enum
{
  MUTT_MBOX = 1,
  MUTT_MMDF,
  MUTT_MH,
  MUTT_MAILDIR,
  MUTT_NNTP,
  MUTT_IMAP,
  MUTT_NOTMUCH,
  MUTT_POP,
  MUTT_COMPRESSED,
};

WHERE short DefaultMagic INITVAL (MUTT_MBOX);

#define MMDF_SEP "\001\001\001\001\n"
#define MAXLOCKATTEMPT 5

int mbox_check_empty(const char *path);
void mbox_reset_atime(CONTEXT *ctx, struct stat *st);

int mh_check_empty(const char *path);

int maildir_check_empty(const char *path);

HEADER *maildir_parse_message(int magic, const char *fname, int is_old, HEADER *h);
HEADER *maildir_parse_stream(int magic, FILE *f, const char *fname, int is_old, HEADER *_h);
void maildir_parse_flags(HEADER *h, const char *path);
void maildir_update_flags(CONTEXT *ctx, HEADER *o, HEADER *n);
void maildir_flags(char *dest, size_t destlen, HEADER *hdr);

#ifdef USE_HCACHE
#include "hcache.h"
int mh_sync_mailbox_message (CONTEXT * ctx, int msgno, header_cache_t *hc);
#else
int mh_sync_mailbox_message (CONTEXT * ctx, int msgno);
#endif

#ifdef USE_NOTMUCH
int mx_is_notmuch(const char *p);
#endif

FILE *maildir_open_find_message (const char *folder, const char *msg,
                                  char **newname);

int mbox_strict_cmp_headers(const HEADER *h1, const HEADER *h2);

void mx_alloc_memory(CONTEXT *ctx);
void mx_update_context(CONTEXT *ctx, int new_messages);
void mx_update_tables(CONTEXT *ctx, int committing);


int mx_lock_file(const char *path, int fd, int excl, int dot, int timeout);
int mx_unlock_file(const char *path, int fd, int dot);

struct mx_ops* mx_get_ops (int magic);
extern struct mx_ops mx_maildir_ops;
extern struct mx_ops mx_mbox_ops;
extern struct mx_ops mx_mh_ops;
extern struct mx_ops mx_mmdf_ops;

#endif
