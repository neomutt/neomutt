/**
 * @file
 * Maildir shared functions
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_MAILDIR_SHARED_H
#define MUTT_MAILDIR_SHARED_H

#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>

struct Buffer;
struct Email;
struct HeaderCache;
struct Mailbox;
struct Message;

// These are needed by Maildir
void          maildir_canon_filename      (struct Buffer *dest, const char *src);
mode_t        maildir_umask               (struct Mailbox *m);
bool          maildir_update_flags        (struct Mailbox *m, struct Email *e_old, struct Email *e_new);

// These are needed by Notmuch
struct Email *maildir_email_new           (void);
void          maildir_gen_flags           (char *dest, size_t destlen, struct Email *e);
bool          maildir_msg_open_new        (struct Mailbox *m, struct Message *msg, const struct Email *e);
FILE *        maildir_open_find_message   (const char *folder, const char *msg, char **newname);
void          maildir_parse_flags         (struct Email *e, const char *path);
bool          maildir_parse_message       (const char *fname, bool is_old, struct Email *e);
bool          maildir_parse_stream        (FILE *fp, const char *fname, bool is_old, struct Email *e);
int           maildir_path_is_empty       (struct Buffer *path);
int           maildir_rewrite_message     (struct Mailbox *m, struct Email *e);
bool          maildir_sync_mailbox_message(struct Mailbox *m, struct Email *e, struct HeaderCache *hc);
void          maildir_update_mtime        (struct Mailbox *m);

#endif /* MUTT_MAILDIR_SHARED_H */
