/**
 * @file
 * String manipulation functions
 *
 * @authors
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019-2020 Pietro Cerutti <gahr@gahr.ch>
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

#ifndef MUTT_MUTT_STRING2_H
#define MUTT_MUTT_STRING2_H

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>

#define STR_COMMAND 8192  ///< Enough space for a long command line

#define NONULL(x) ((x) ? (x) : "")

/* Exit values */
#define S_ERR 127
#define S_BKG 126

/* this macro must check for (*ch == 0) since isspace(0) has unreliable behavior
 * on some systems */
#define SKIPWS(ch)                                                             \
  while (*(ch) && isspace((unsigned char) *(ch)))                              \
    ch++;

#define terminate_string(str, strlen, buflen)                                  \
  (str)[MIN((strlen), (buflen))] = '\0'

#define terminate_buffer(str, strlen)                                          \
  terminate_string(str, strlen, sizeof(str) - 1)

void        mutt_str_adjust(char **ptr);
int         mutt_str_asprintf(char **strp, const char *fmt, ...)
                              __attribute__((__format__(__printf__, 2, 3)));
int         mutt_str_coll(const char *a, const char *b);
const char *mutt_str_find_word(const char *src);
const char *mutt_str_getenv(const char *name);
void        mutt_str_hyphenate(char *buf, size_t buflen, const char *str);
bool        mutt_str_is_ascii(const char *str, size_t len);
size_t      mutt_str_len(const char *a);
char *      mutt_str_lower(char *str);
size_t      mutt_str_lws_len(const char *s, size_t n);
void        mutt_str_remove_trailing_ws(char *s);
char *      mutt_str_replace(char **p, const char *s);
char *      mutt_str_sep(char **stringp, const char *delim);
char *      mutt_str_skip_email_wsp(const char *s);
char *      mutt_str_skip_whitespace(const char *p);
const char *mutt_str_sysexit(int e);
char *      mutt_str_upper(char *str);

/* case-sensitive flavours */
int         mutt_str_cmp(const char *a, const char *b);
size_t      mutt_str_copy(char *dest, const char *src, size_t dsize);
char *      mutt_str_dup(const char *str);
bool        mutt_str_equal(const char *a, const char *b);
size_t      mutt_str_startswith(const char *str, const char *prefix);

/* case-sensitive, length-bound flavours */
char *      mutt_strn_copy(char *dest, const char *src, size_t len, size_t dsize);
char *      mutt_strn_dup(const char *begin, size_t l);
bool        mutt_strn_equal(const char *a, const char *b, size_t num);

/* case-insensitive flavours */
int         mutt_istr_cmp(const char *a, const char *b);
bool        mutt_istr_equal(const char *a, const char *b);
const char *mutt_istr_find(const char *haystack, const char *needle);
int         mutt_istr_remall(char *str, const char *target);
size_t      mutt_istr_startswith(const char *str, const char *prefix);
int         mutt_str_inbox_cmp(const char *a, const char *b);

/* case-insensitive, length-bound flavours */
int         mutt_istrn_cmp(const char *a, const char *b, size_t num);
bool        mutt_istrn_equal(const char *a, const char *b, size_t num);
const char *mutt_istrn_rfind(const char *haystack, size_t haystack_length, const char *needle);

/**
 * mutt_str_is_email_wsp - Is this a whitespace character (for an email header)
 * @param c Character to test
 * @retval true It is whitespace
 */
static inline bool mutt_str_is_email_wsp(char c)
{
  return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n');
}

#endif /* MUTT_MUTT_STRING2_H */
