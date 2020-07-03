/**
 * @file
 * String manipulation functions
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

/* @note This file is called string2.h so that other files can safely
 * #include <string.h>
 */

#ifndef MUTT_LIB_STRING_H
#define MUTT_LIB_STRING_H

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>

#define STR_COMMAND 8192  ///< Enough space for a long command line

#define NONULL(x) ((x) ? (x) : "")
#define IS_SPACE(ch) isspace((unsigned char) ch)

/* Exit values */
#define S_ERR 127
#define S_BKG 126

/* this macro must check for (*c == 0) since isspace(0) has unreliable behavior
 * on some systems */
#define SKIPWS(ch)                                                             \
  while (*(ch) && isspace((unsigned char) *(ch)))                              \
    ch++;

#define terminate_string(str, strlen, buflen)                                  \
  (str)[MIN((strlen), (buflen))] = '\0'

#define terminate_buffer(str, strlen)                                          \
  terminate_string(str, strlen, sizeof(str) - 1)

void        mutt_str_adjust(char **p);
void        mutt_str_append_item(char **str, const char *item, char sep);
int         mutt_str_asprintf(char **strp, const char *fmt, ...);
int         mutt_str_atoi(const char *str, int *dst);
int         mutt_str_atol(const char *str, long *dst);
int         mutt_str_atos(const char *str, short *dst);
int         mutt_str_atoui(const char *str, unsigned int *dst);
int         mutt_str_atoul(const char *str, unsigned long *dst);
int         mutt_str_atoull(const char *str, unsigned long long *dst);
int         mutt_str_coll(const char *a, const char *b);
void        mutt_str_dequote_comment(char *s);
const char *mutt_str_find_word(const char *src);
const char *mutt_str_getenv(const char *name);
bool        mutt_str_inline_replace(char *buf, size_t buflen, size_t xlen, const char *rstr);
bool        mutt_str_is_ascii(const char *str, size_t len);
bool        mutt_str_is_email_wsp(char c);
size_t      mutt_str_len(const char *a);
char *      mutt_str_lower(char *s);
size_t      mutt_str_lws_len(const char *s, size_t n);
size_t      mutt_str_lws_rlen(const char *s, size_t n);
const char *mutt_str_next_word(const char *s);
void        mutt_str_remove_trailing_ws(char *s);
char *      mutt_str_replace(char **p, const char *s);
char *      mutt_str_skip_email_wsp(const char *s);
char *      mutt_str_skip_whitespace(const char *p);
const char *mutt_str_sysexit(int e);

/* case-sensitive flavours */
char *      mutt_str_cat(char *buf, size_t buflen, const char *s);
int         mutt_str_cmp(const char *a, const char *b);
size_t      mutt_str_copy(char *dest, const char *src, size_t dsize);
char *      mutt_str_dup(const char *str);
bool        mutt_str_equal(const char *a, const char *b);
size_t      mutt_str_startswith(const char *str, const char *prefix);

/* case-sensitive, length-bound flavours */
char *      mutt_strn_cat(char *dest, size_t l, const char *s, size_t sl);
char *      mutt_strn_copy(char *dest, const char *src, size_t len, size_t dsize);
char *      mutt_strn_dup(const char *begin, size_t l);
bool        mutt_strn_equal(const char *a, const char *b, size_t l);
const char *mutt_strn_rfind(const char *haystack, size_t haystack_length, const char *needle);

/* case-insensitive flavours */
int         mutt_istr_cmp(const char *a, const char *b);
bool        mutt_istr_equal(const char *a, const char *b);
const char *mutt_istr_find(const char *haystack, const char *needle);
int         mutt_istr_remall(char *str, const char *target);
size_t      mutt_istr_startswith(const char *str, const char *prefix);

/* case-insensitive, length-bound flavours */
int         mutt_istrn_cmp(const char *a, const char *b, size_t l);
bool        mutt_istrn_equal(const char *a, const char *b, size_t l);

#endif /* MUTT_LIB_STRING_H */
