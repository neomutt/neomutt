/**
 * @file
 * GUI miscellaneous curses (window drawing) routines
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

#ifndef MUTT_CURS_LIB_H
#define MUTT_CURS_LIB_H

#include <stdbool.h>
#include <stdio.h>
#include <wchar.h>

struct Context;

bool   message_is_tagged(struct Context *ctx, int index);
bool   message_is_visible(struct Context *ctx, int index);
int    mutt_addwch(wchar_t wc);
int    mutt_any_key_to_continue(const char *s);
void   mutt_edit_file(const char *editor, const char *data);
int    mutt_enter_fname_full(const char *prompt, char *buf, size_t blen, int buffy, int multiple, char ***files, int *numfiles, int flags);
void   mutt_format_s(char *buf, size_t buflen, const char *prec, const char *s);
void   mutt_format_s_tree(char *buf, size_t buflen, const char *prec, const char *s);
int    mutt_get_field_full(const char *field, char *buf, size_t buflen, int complete, int multiple, char ***files, int *numfiles);
int    mutt_get_field_unbuffered(char *msg, char *buf, size_t buflen, int flags);
int    mutt_multi_choice(char *prompt, char *letters);
void   mutt_paddstr(int n, const char *s);
void   mutt_perror_debug(const char *s);
void   mutt_query_exit(void);
void   mutt_show_error(void);
void   mutt_simple_format(char *buf, size_t buflen, int min_width, int max_width, int justify, char pad_char, const char *s, size_t n, int arboreal);
int    mutt_strwidth(const char *s);
size_t mutt_wstr_trunc(const char *src, size_t maxlen, size_t maxwid, size_t *width);
int    mutt_yesorno(const char *msg, int def);

#endif /* MUTT_CURS_LIB_H */
