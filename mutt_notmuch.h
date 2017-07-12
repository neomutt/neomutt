/**
 * @file
 * NotMuch virtual mailbox type
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

#ifndef _MUTT_NOTMUCH_H
#define _MUTT_NOTMUCH_H

#include <stddef.h>
#include <stdbool.h>
#include "mx.h"

struct Context;
struct Header;

int nm_read_entire_thread(struct Context *ctx, struct Header *h);

char *nm_header_get_folder(struct Header *h);
int nm_update_filename(struct Context *ctx, const char *old, const char *new, struct Header *h);
bool nm_normalize_uri(char *new_uri, const char *orig_uri, size_t new_uri_sz);
char *nm_uri_from_query(struct Context *ctx, char *buf, size_t bufsz);
int nm_modify_message_tags(struct Context *ctx, struct Header *hdr, char *buf);
bool nm_message_is_still_queried(struct Context *ctx, struct Header *hdr);

void nm_query_window_backward(void);
void nm_query_window_forward(void);

void nm_longrun_init(struct Context *ctx, int writable);
void nm_longrun_done(struct Context *ctx);

char *nm_get_description(struct Context *ctx);
int nm_description_to_path(const char *desc, char *buf, size_t bufsz);

int nm_record_message(struct Context *ctx, char *path, struct Header *h);

void nm_debug_check(struct Context *ctx);
int nm_get_all_tags(struct Context *ctx, char **tag_list, int *tag_count);

/*
 * functions usable outside notmuch Context
 */
int nm_nonctx_get_count(char *path, int *all, int *new);

char *nm_header_get_tag_transformed(char *tag, struct Header *h);
char *nm_header_get_tags_transformed(struct Header *h);
char *nm_header_get_tags(struct Header *h);

extern struct MxOps mx_notmuch_ops;

#endif /* _MUTT_NOTMUCH_H */
