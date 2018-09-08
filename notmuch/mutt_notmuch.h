/**
 * @file
 * Notmuch virtual mailbox type
 *
 * @authors
 * Copyright (C) 2011 Karel Zak <kzak@redhat.com>
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
 * @page notmuch NOTMUCH: Virtual mailbox type
 *
 * Notmuch virtual mailbox type
 *
 * | File                   | Description         |
 * | :--------------------- | :------------------ |
 * | notmuch/mutt_notmuch.c | @subpage nm_notmuch |
 */

#ifndef MUTT_NOTMUCH_MUTT_NOTMUCH_H
#define MUTT_NOTMUCH_MUTT_NOTMUCH_H

#include <stddef.h>
#include <stdbool.h>
#include "mx.h"

struct Context;
struct Header;

/* These Config Variables are only used in notmuch/mutt_notmuch.c */
extern int   NmDbLimit;
extern char *NmDefaultUri;
extern char *NmExcludeTags;
extern int   NmOpenTimeout;
extern char *NmQueryType;
extern int   NmQueryWindowCurrentPosition;
extern char *NmQueryWindowTimebase;
extern char *NmRecordTags;
extern char *NmUnreadTag;

int nm_read_entire_thread(struct Context *ctx, struct Header *h);

char *nm_header_get_folder(struct Header *h);
int nm_update_filename(struct Mailbox *mailbox, const char *old, const char *new, struct Header *h);
bool nm_normalize_uri(const char *uri, char *buf, size_t buflen);
char *nm_uri_from_query(struct Mailbox *mailbox, char *buf, size_t buflen);
bool nm_message_is_still_queried(struct Mailbox *mailbox, struct Header *hdr);

void nm_query_window_backward(void);
void nm_query_window_forward(void);

void nm_longrun_init(struct Mailbox *mailbox, bool writable);
void nm_longrun_done(struct Mailbox *mailbox);

char *nm_get_description(struct Mailbox *mailbox);
int nm_description_to_path(const char *desc, char *buf, size_t buflen);

int nm_record_message(struct Mailbox *mailbox, char *path, struct Header *h);

void nm_debug_check(struct Mailbox *mailbox);
int nm_get_all_tags(struct Mailbox *mailbox, char **tag_list, int *tag_count);

/*
 * functions usable outside of notmuch
 */
int nm_nonctx_get_count(char *path, int *all, int *new);
int nm_path_probe(const char *path, const struct stat *st);

extern struct MxOps mx_notmuch_ops;

#endif /* MUTT_NOTMUCH_MUTT_NOTMUCH_H */
