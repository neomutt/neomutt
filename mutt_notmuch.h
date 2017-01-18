/*
 * Copyright (C) 2011 Karel Zak <kzak@redhat.com>
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
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */

#ifndef _MUTT_NOTMUCH_H
#define _MUTT_NOTMUCH_H

#include <stdbool.h>

int   nm_read_entire_thread(CONTEXT *ctx, HEADER *h);

char *nm_header_get_folder(HEADER *h);
int   nm_update_filename(CONTEXT *ctx, const char *old, const char *new, HEADER *h);
bool  nm_normalize_uri(char *new_uri, const char *orig_uri, size_t new_uri_sz);
char *nm_uri_from_query(CONTEXT *ctx, char *buf, size_t bufsz);
int   nm_modify_message_tags(CONTEXT *ctx, HEADER *hdr, char *buf);

void  nm_query_window_backward(void);
void  nm_query_window_forward(void);

void  nm_longrun_init(CONTEXT *ctx, int writable);
void  nm_longrun_done(CONTEXT *ctx);

char *nm_get_description(CONTEXT *ctx);
int   nm_description_to_path(const char *desc, char *buf, size_t bufsz);

int   nm_record_message(CONTEXT *ctx, char *path, HEADER *h);

void  nm_debug_check(CONTEXT *ctx);
int   nm_get_all_tags(CONTEXT *ctx, char **tag_list, int *tag_count);

/*
 * functions usable outside notmuch CONTEXT
 */
int   nm_nonctx_get_count(char *path, int *all, int *new);

char *nm_header_get_tag_transformed(char *tag, HEADER *h);
char *nm_header_get_tags_transformed(HEADER *h);
char *nm_header_get_tags(HEADER *h);

extern struct mx_ops mx_notmuch_ops;

#endif /* _MUTT_NOTMUCH_H */
