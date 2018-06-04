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

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include "mutt.h"
#include "browser.h"

struct Context;
struct Pager;

/* These Config Variables are only used in curs_lib.c */
extern bool MetaKey; /**< interpret ALT-x as ESC-x */

/* For mutt_simple_format() justifications */
#define FMT_LEFT   -1
#define FMT_CENTER 0
#define FMT_RIGHT  1

bool         message_is_tagged(struct Context *ctx, int index);
bool         message_is_visible(struct Context *ctx, int index);
int          mutt_addwch(wchar_t wc);
int          mutt_any_key_to_continue(const char *s);
int          mutt_do_pager(const char *banner, const char *tempfile, int do_color, struct Pager *info);
void         mutt_edit_file(const char *editor, const char *file);
void         mutt_endwin(void);
int          mutt_enter_fname_full(const char *prompt, char *buf, size_t blen, bool mailbox, bool multiple, char ***files, int *numfiles, int flags);
void         mutt_flushinp(void);
void         mutt_flush_macro_to_endcond(void);
void         mutt_flush_unget_to_endcond(void);
void         mutt_format_s(char *buf, size_t buflen, const char *prec, const char *s);
void         mutt_format_s_tree(char *buf, size_t buflen, const char *prec, const char *s);
struct Event mutt_getch(void);
void         mutt_getch_timeout(int);
int          mutt_get_field_full(const char *field, char *buf, size_t buflen, int complete, bool multiple, char ***files, int *numfiles);
int          mutt_get_field_unbuffered(char *msg, char *buf, size_t buflen, int flags);
int          mutt_multi_choice(const char *prompt, const char *letters);
void         mutt_need_hard_redraw(void);
void         mutt_paddstr(int n, const char *s);
void         mutt_perror_debug(const char *s);
void         mutt_push_macro_event(int ch, int op);
void         mutt_query_exit(void);
void         mutt_refresh(void);
void         mutt_show_error(void);
void         mutt_simple_format(char *buf, size_t buflen, int min_width, int max_width, int justify, char pad_char, const char *s, size_t n, int arboreal);
int          mutt_strwidth(const char *s);
void         mutt_unget_event(int ch, int op);
void         mutt_unget_string(const char *s);
size_t       mutt_wstr_trunc(const char *src, size_t maxlen, size_t maxwid, size_t *width);
int          mutt_yesorno(const char *msg, int def);

#define mutt_enter_fname(A, B, C, D)   mutt_enter_fname_full(A, B, C, D, false, NULL, NULL, 0)
#define mutt_enter_vfolder(A, B, C, D) mutt_enter_fname_full(A, B, C, D, false, NULL, NULL, MUTT_SEL_VFOLDER)
#define mutt_get_field(A, B, C, D)     mutt_get_field_full(A, B, C, D, false, NULL, NULL)
#define mutt_get_password(A, B, C)     mutt_get_field_unbuffered(A, B, C, MUTT_PASS)

#endif /* MUTT_CURS_LIB_H */
