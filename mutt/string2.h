/**
 * @file
 * String manipulation functions
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

/* NOTE: This file is called string2.h so that other files can safely
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
#define EMAIL_WSP " \t\r\n"

/* Exit values */
#define S_ERR 127
#define S_BKG 126

/* this macro must check for (*c == 0) since isspace(0) has unreliable behavior
 * on some systems */
#define SKIPWS(ch)                                                             \
  while (*(ch) && isspace((unsigned char) *(ch)))                              \
    ch++;

#define terminate_string(str, strlen, buflen)                                  \
  do                                                                           \
  {                                                                            \
    if ((strlen) < (buflen))                                                   \
      str[(strlen)] = '\0';                                                    \
    else                                                                       \
      str[(buflen)] = '\0';                                                    \
  } while (false)

#define terminate_buffer(str, strlen) terminate_string(str, strlen, sizeof(str) - 1)

/**
 * enum CaseSensitivity - Should a string's case matter when matching?
 */
enum CaseSensitivity
{
  CASE_MATCH,  ///< Match case when comparing strings
  CASE_IGNORE, ///< Ignore case when comparing strings
};

void        mutt_str_adjust(char **p);
void        mutt_str_append_item(char **str, const char *item, char sep);
int         mutt_str_asprintf(char **strp, const char *fmt, ...);
int         mutt_str_atoi(const char *str, int *dst);
int         mutt_str_atol(const char *str, long *dst);
int         mutt_str_atos(const char *str, short *dst);
int         mutt_str_atoui(const char *str, unsigned int *dst);
int         mutt_str_atoul(const char *str, unsigned long *dst);
int         mutt_str_atoull(const char *str, unsigned long long *dst);
void        mutt_str_dequote_comment(char *s);
const char *mutt_str_find_word(const char *src);
const char *mutt_str_getenv(const char *name);
bool        mutt_str_inline_replace(char *buf, size_t buflen, size_t xlen, const char *rstr);
bool        mutt_str_is_ascii(const char *p, size_t len);
bool        mutt_str_is_email_wsp(char c);
size_t      mutt_str_lws_len(const char *s, size_t n);
size_t      mutt_str_lws_rlen(const char *s, size_t n);
const char *mutt_str_next_word(const char *s);
void        mutt_str_pretty_size(char *buf, size_t buflen, size_t num);
int         mutt_str_remall_strcasestr(char *str, const char *target);
void        mutt_str_remove_trailing_ws(char *s);
void        mutt_str_replace(char **p, const char *s);
const char *mutt_str_rstrnstr(const char *haystack, size_t haystack_length, const char *needle);
char *      mutt_str_skip_email_wsp(const char *s);
char *      mutt_str_skip_whitespace(const char *p);
struct ListHead mutt_str_split(const char *src, char sep);
int         mutt_str_strcasecmp(const char *a, const char *b);
size_t      mutt_str_startswith(const char *str, const char *prefix, enum CaseSensitivity cs);
const char *mutt_str_strcasestr(const char *haystack, const char *needle);
char *      mutt_str_strcat(char *buf, size_t buflen, const char *s);
const char *mutt_str_strchrnul(const char *s, char c);
int         mutt_str_strcmp(const char *a, const char *b);
int         mutt_str_strcoll(const char *a, const char *b);
char *      mutt_str_strdup(const char *str);
size_t      mutt_str_strfcpy(char *dest, const char *src, size_t dsize);
const char *mutt_str_stristr(const char *haystack, const char *needle);
size_t      mutt_str_strlen(const char *a);
char *      mutt_str_strlower(char *s);
int         mutt_str_strncasecmp(const char *a, const char *b, size_t l);
char *      mutt_str_strncat(char *d, size_t l, const char *s, size_t sl);
int         mutt_str_strncmp(const char *a, const char *b, size_t l);
size_t      mutt_str_strnfcpy(char *dest, const char *src, size_t n, size_t dsize);
char *      mutt_str_substr_cpy(char *dest, const char *begin, const char *end, size_t destlen);
char *      mutt_str_substr_dup(const char *begin, const char *end);
const char *mutt_str_sysexit(int e);
int         mutt_str_word_casecmp(const char *a, const char *b);

#endif /* MUTT_LIB_STRING_H */
