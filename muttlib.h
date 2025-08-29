/**
 * @file
 * Some miscellaneous functions
 *
 * @authors
 * Copyright (C) 2018-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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

#ifndef MUTT_MUTTLIB_H
#define MUTT_MUTTLIB_H

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include "attach/lib.h"

struct Address;
struct Body;
struct Buffer;
struct ListHead;
struct passwd;
struct stat;

void        mutt_adv_mktemp(struct Buffer *buf);
void        buf_expand_path(struct Buffer *buf);
void        buf_expand_path_regex(struct Buffer *buf, bool regex);
void        buf_pretty_mailbox(struct Buffer *s);
void        buf_sanitize_filename (struct Buffer *buf, const char *path, short slash);
void        buf_save_path(struct Buffer *dest, const struct Address *a);
int         mutt_check_overwrite(const char *attname, const char *path, struct Buffer *fname, enum SaveAttach *opt, char **directory);
void        mutt_encode_path(struct Buffer *buf, const char *src);
char *      mutt_gecos_name(char *dest, size_t destlen, struct passwd *pw);
void        mutt_get_parent_path(const char *path, char *buf, size_t buflen);
bool        mutt_is_text_part(const struct Body *b);
const char *mutt_make_version(void);
bool        mutt_needs_mailcap(struct Body *b);
FILE *      mutt_open_read(const char *path, pid_t *thepid);
void        mutt_pretty_mailbox(char *buf, size_t buflen);
void        mutt_safe_path(struct Buffer *dest, const struct Address *a);
int         mutt_save_confirm(const char *s, struct stat *st);
void        mutt_save_path(char *d, size_t dsize, const struct Address *a);
void        mutt_sleep(short s);
void        mutt_str_pretty_size(char *buf, size_t buflen, size_t num);

void add_to_stailq     (struct ListHead *head, const char *str);
void remove_from_stailq(struct ListHead *head, const char *str);

#endif /* MUTT_MUTTLIB_H */
