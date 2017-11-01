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

#ifndef _LIB_STRING_H
#define _LIB_STRING_H

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>

#define SHORT_STRING 128
#define STRING       256
#define LONG_STRING  1024
#define HUGE_STRING  8192

#define NONULL(x) x ? x : ""
#define ISSPACE(c) isspace((unsigned char) c)
#define EMAIL_WSP " \t\r\n"

/* this macro must check for (*c == 0) since isspace(0) has unreliable behavior
   on some systems */
#define SKIPWS(c)                                                              \
  while (*(c) && isspace((unsigned char) *(c)))                                \
    c++;

#define terminate_string(a, b, c)                                              \
  do                                                                           \
  {                                                                            \
    if ((b) < (c))                                                             \
      a[(b)] = 0;                                                              \
    else                                                                       \
      a[(c)] = 0;                                                              \
  } while (0)

#define terminate_buffer(a, b) terminate_string(a, b, sizeof(a) - 1)

const char *find_word(const char *src);
int         imap_wordcasecmp(const char *a, const char *b);
bool        is_ascii(const char *p, size_t len);
int         is_email_wsp(char c);
size_t      lwslen(const char *s, size_t n);
size_t      lwsrlen(const char *s, size_t n);
int         mutt_atoi(const char *str, int *dst);
int         mutt_atos(const char *str, short *dst);
void        mutt_remove_trailing_ws(char *s);
char *      mutt_skip_whitespace(char *p);
void        mutt_str_adjust(char **p);
void        mutt_str_append_item(char **p, const char *item, int sep);
int         mutt_strcasecmp(const char *a, const char *b);
const char *mutt_strchrnul(const char *s, char c);
int         mutt_strcmp(const char *a, const char *b);
int         mutt_strcoll(const char *a, const char *b);
const char *mutt_stristr(const char *haystack, const char *needle);
size_t      mutt_strlen(const char *a);
char *      mutt_strlower(char *s);
int         mutt_strncasecmp(const char *a, const char *b, size_t l);
int         mutt_strncmp(const char *a, const char *b, size_t l);
void        mutt_str_replace(char **p, const char *s);
char *      mutt_substrcpy(char *dest, const char *begin, const char *end, size_t destlen);
char *      mutt_substrdup(const char *begin, const char *end);
const char *next_word(const char *s);
void        rfc822_dequote_comment(char *s);
const char *rstrnstr(const char *haystack, size_t haystack_length, const char *needle);
char *      safe_strcat(char *d, size_t l, const char *s);
char *      safe_strdup(const char *s);
char *      safe_strncat(char *d, size_t l, const char *s, size_t sl);
char *      skip_email_wsp(const char *s);
char *      strfcpy(char *dest, const char *src, size_t dlen);
char *      strnfcpy(char *dest, char *src, size_t size, size_t dlen);

#endif /* _LIB_STRING_H */
