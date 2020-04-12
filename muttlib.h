/**
 * @file
 * Some miscellaneous functions
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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
#include <stdint.h>
#include <stdio.h>
#include "format_flags.h"
#include "mutt_attach.h"

struct Address;
struct Body;
struct Buffer;
struct ListHead;
struct passwd;
struct stat;

/* These Config Variables are only used in muttlib.c */
extern struct Regex *C_GecosMask;

void        mutt_adv_mktemp(struct Buffer *buf);
void        mutt_buffer_mktemp_full(struct Buffer *buf, const char *prefix, const char *suffix, const char *src, int line);
void        mutt_buffer_expand_path(struct Buffer *buf);
void        mutt_buffer_expand_path_regex(struct Buffer *buf, bool regex);
void        mutt_buffer_pretty_mailbox(struct Buffer *s);
void        mutt_buffer_sanitize_filename (struct Buffer *buf, const char *path, short slash);
void        mutt_buffer_save_path(struct Buffer *dest, const struct Address *a);
void        mutt_buffer_strip_formatting(struct Buffer *dest, const char *src);
int         mutt_check_overwrite(const char *attname, const char *path, struct Buffer *fname, enum SaveAttach *opt, char **directory);
void        mutt_encode_path(struct Buffer *buf, const char *src);
void        mutt_expando_format(char *buf, size_t buflen, size_t col, int cols, const char *src, format_t *callback, intptr_t data, MuttFormatFlags flags);
char *      mutt_expand_path(char *s, size_t slen);
char *      mutt_expand_path_regex(char *buf, size_t buflen, bool regex);
char *      mutt_gecos_name(char *dest, size_t destlen, struct passwd *pw);
void        mutt_get_parent_path(const char *path, char *buf, size_t buflen);
int         mutt_inbox_cmp(const char *a, const char *b);
bool        mutt_is_text_part(struct Body *b);
const char *mutt_make_version(void);
void        mutt_mktemp_full(char *s, size_t slen, const char *prefix, const char *suffix, const char *src, int line);
bool        mutt_needs_mailcap(struct Body *m);
FILE *      mutt_open_read(const char *path, pid_t *thepid);
void        mutt_pretty_mailbox(char *buf, size_t buflen);
void        mutt_safe_path(struct Buffer *dest, const struct Address *a);
int         mutt_save_confirm(const char *s, struct stat *st);
void        mutt_save_path(char *d, size_t dsize, const struct Address *a);
void        mutt_sleep(short s);
void        mutt_str_pretty_size(char *buf, size_t buflen, size_t num);

void add_to_stailq     (struct ListHead *head, const char *str);
void remove_from_stailq(struct ListHead *head, const char *str);

#define mutt_mktemp(buf, buflen)                         mutt_mktemp_pfx_sfx(buf, buflen, "neomutt", NULL)
#define mutt_mktemp_pfx_sfx(buf, buflen, prefix, suffix) mutt_mktemp_full(buf, buflen, prefix, suffix, __FILE__, __LINE__)

#define mutt_buffer_mktemp(buf)                         mutt_buffer_mktemp_pfx_sfx(buf, "neomutt", NULL)
#define mutt_buffer_mktemp_pfx_sfx(buf, prefix, suffix) mutt_buffer_mktemp_full(buf, prefix, suffix, __FILE__, __LINE__)

#endif /* MUTT_MUTTLIB_H */
