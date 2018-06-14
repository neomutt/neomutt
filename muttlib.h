/**
 * @file
 * Some miscellaneous functions
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

#ifndef MUTT_MUTTLIB_H
#define MUTT_MUTTLIB_H

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt.h"
#include "format_flags.h"

struct Address;
struct Body;

struct passwd;
struct stat;

/* These Config Variables are only used in muttlib.c */
extern struct Regex *GecosMask;

#define MUTT_RANDTAG_LEN 16

void        mutt_adv_mktemp(char *s, size_t l);
int         mutt_check_overwrite(const char *attname, const char *path, char *fname, size_t flen, int *append, char **directory);
void        mutt_encode_path(char *dest, size_t dlen, const char *src);
void        mutt_expando_format(char *buf, size_t buflen, size_t col, int cols, const char *src, format_t *callback, unsigned long data, enum FormatFlag flags);
char *      mutt_expand_path(char *s, size_t slen);
char *      mutt_expand_path_regex(char *s, size_t slen, bool regex);
char *      mutt_gecos_name(char *dest, size_t destlen, struct passwd *pw);
void        mutt_get_parent_path(char *path, char *buf, size_t buflen);
int         mutt_inbox_cmp(const char *a, const char *b);
bool        mutt_is_text_part(struct Body *b);
const char *mutt_make_version(void);
bool        mutt_matches_ignore(const char *s);
void        mutt_mktemp_full(char *s, size_t slen, const char *prefix, const char *suffix, const char *src, int line);
bool        mutt_needs_mailcap(struct Body *m);
FILE *      mutt_open_read(const char *path, pid_t *thepid);
void        mutt_pretty_mailbox(char *s, size_t buflen);
uint32_t    mutt_rand32(void);
uint64_t    mutt_rand64(void);
void        mutt_rand_base32(void *out, size_t len);
int         mutt_randbuf(void *out, size_t len);
void        mutt_safe_path(char *s, size_t l, struct Address *a);
int         mutt_save_confirm(const char *s, struct stat *st);
void        mutt_save_path(char *d, size_t dsize, struct Address *a);
void        mutt_sleep(short s);

int mutt_timespec_compare(struct timespec *a, struct timespec *b);
void mutt_get_stat_timespec(struct timespec *dest, struct stat *sb, enum MuttStatType type);
int mutt_stat_timespec_compare(struct stat *sba, enum MuttStatType type, struct timespec *b);
int mutt_stat_compare(struct stat *sba, enum MuttStatType sba_type, struct stat *sbb, enum MuttStatType sbb_type);

#define mutt_mktemp(a, b)               mutt_mktemp_pfx_sfx(a, b, "neomutt", NULL)
#define mutt_mktemp_pfx_sfx(a, b, c, d) mutt_mktemp_full(a, b, c, d, __FILE__, __LINE__)

#endif /* MUTT_MUTTLIB_H */
