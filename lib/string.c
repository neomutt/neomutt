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

/**
 * @page string String manipulation functions
 *
 * Lots of commonly-used string manipulation routines.
 *
 * | Function                  | Description
 * | :------------------------ | :---------------------------------------------------------
 * | is_email_wsp()            | Is this a whitespace character (for an email header)
 * | lwslen()                  | Measure the linear-white-space at the beginning of a string
 * | lwsrlen()                 | Measure the linear-white-space at the end of a string
 * | mutt_atoi()               | Convert ASCII string to an integer
 * | mutt_atos()               | Convert ASCII string to a short
 * | mutt_remove_trailing_ws() | Trim trailing whitespace from a string
 * | mutt_skip_whitespace()    | Find the first non-whitespace character in a string
 * | mutt_strcasecmp()         | Compare two strings ignoring case, safely
 * | mutt_strchrnul()          | Find first occurrence of character in string
 * | mutt_strcmp()             | Compare two strings, safely
 * | mutt_strcoll()            | Collate two strings (compare using locale), safely
 * | mutt_stristr()            | Find first occurrence of string (ignoring case)
 * | mutt_strlen()             | Calculate the length of a string, safely
 * | mutt_strlower()           | convert all characters in the string to lowercase
 * | mutt_strncasecmp()        | Compare two strings ignoring case (to a maximum), safely
 * | mutt_strncmp()            | Compare two strings (to a maximum), safely
 * | mutt_str_adjust()         | Shrink-to-fit a string
 * | mutt_str_replace()        | Replace one string with another
 * | mutt_substrcpy()          | Copy a sub-string into a buffer
 * | mutt_substrdup()          | Duplicate a sub-string
 * | rfc822_dequote_comment()  | Un-escape characters in an email address comment
 * | safe_strcat()             | Concatenate two strings
 * | safe_strdup()             | Copy a string, safely
 * | safe_strncat()            | Concatenate two strings
 * | skip_email_wsp()          | Skip over whitespace as defined by RFC5322
 * | strfcpy()                 | Copy a string into a buffer (guaranteeing NUL-termination)
 * | strnfcpy()                | Copy a limited string into a buffer (guaranteeing NUL-termination)
 */

#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "memory.h"
#include "string2.h"

/**
 * mutt_atol - Convert ASCII string to a long
 * @param[in]  str String to read
 * @param[out] dst Store the result
 * @retval  0 Success
 * @retval -1 Error
 *
 * This is a strtol() wrapper with range checking.
 * errno may be set on error, e.g. ERANGE
 */
static int mutt_atol(const char *str, long *dst)
{
  long r;
  long *res = dst ? dst : &r;
  char *e = NULL;

  /* no input: 0 */
  if (!str || !*str)
  {
    *res = 0;
    return 0;
  }

  *res = strtol(str, &e, 10);
  if (((*res == LONG_MAX) && (errno == ERANGE)) || (e && (*e != '\0')))
    return -1;
  return 0;
}

/**
 * mutt_atos - Convert ASCII string to a short
 * @param[in]  str String to read
 * @param[out] dst Store the result
 * @retval  0 Success
 * @retval -1 Error
 * @retval -2 Error, overflow
 *
 * This is a strtol() wrapper with range checking.
 * If @a dst is NULL, the string will be tested only (without conversion).
 *
 * errno may be set on error, e.g. ERANGE
 */
int mutt_atos(const char *str, short *dst)
{
  int rc;
  long res;
  short tmp;
  short *t = dst ? dst : &tmp;

  *t = 0;

  rc = mutt_atol(str, &res);
  if (rc < 0)
    return rc;
  if ((short) res != res)
    return -2;

  *t = (short) res;
  return 0;
}

/**
 * mutt_atoi - Convert ASCII string to an integer
 * @param[in]  str String to read
 * @param[out] dst Store the result
 * @retval  0 Success
 * @retval -1 Error
 * @retval -2 Error, overflow
 *
 * This is a strtol() wrapper with range checking.
 * If @a dst is NULL, the string will be tested only (without conversion).
 * errno may be set on error, e.g. ERANGE
 */
int mutt_atoi(const char *str, int *dst)
{
  int rc;
  long res;
  int tmp;
  int *t = dst ? dst : &tmp;

  *t = 0;

  rc = mutt_atol(str, &res);
  if (rc < 0)
    return rc;
  if ((int) res != res)
    return -2;

  *t = (int) res;
  return 0;
}

/**
 * safe_strdup - Copy a string, safely
 * @param s String to copy
 * @retval ptr  Copy of the string
 * @retval NULL if s was NULL
 */
char *safe_strdup(const char *s)
{
  char *p = NULL;
  size_t l;

  if (!s || !*s)
    return 0;
  l = strlen(s) + 1;
  p = safe_malloc(l);
  memcpy(p, s, l);
  return p;
}

/**
 * safe_strcat - Concatenate two strings
 * @param d Buffer containing source string
 * @param l Length of buffer
 * @param s String to add
 * @retval ptr Start of joined string
 */
char *safe_strcat(char *d, size_t l, const char *s)
{
  char *p = d;

  if (!l)
    return d;

  l--; /* Space for the trailing '\0'. */

  for (; *d && l; l--)
    d++;
  for (; *s && l; l--)
    *d++ = *s++;

  *d = '\0';

  return p;
}

/**
 * safe_strncat - Concatenate two strings
 * @param d  Buffer containing source string
 * @param l  Length of buffer
 * @param s  String to add
 * @param sl Maximum amount of string to add
 * @retval ptr Start of joined string
 *
 * Add a string to a maximum of @a sl bytes.
 */
char *safe_strncat(char *d, size_t l, const char *s, size_t sl)
{
  char *p = d;

  if (!l)
    return d;

  l--; /* Space for the trailing '\0'. */

  for (; *d && l; l--)
    d++;
  for (; *s && l && sl; l--, sl--)
    *d++ = *s++;

  *d = '\0';

  return p;
}

/**
 * mutt_str_replace - Replace one string with another
 * @param p String to replace
 * @param s New string
 *
 * This function free()s the original string, strdup()s the new string and
 * overwrites the pointer to the first string.
 *
 * This function alters the pointer of the caller.
 */
void mutt_str_replace(char **p, const char *s)
{
  FREE(p);
  *p = safe_strdup(s);
}

/**
 * mutt_str_adjust - Shrink-to-fit a string
 * @param p String to alter
 *
 * Take a string which is allocated on the heap, find its length and reallocate
 * the memory to be exactly the right size.
 *
 * This function alters the pointer of the caller.
 */
void mutt_str_adjust(char **p)
{
  if (!p || !*p)
    return;
  safe_realloc(p, strlen(*p) + 1);
}

/**
 * mutt_strlower - convert all characters in the string to lowercase
 * @param s String to lowercase
 * @retval ptr Lowercase string
 *
 * The string is transformed in place.
 */
char *mutt_strlower(char *s)
{
  char *p = s;

  while (*p)
  {
    *p = tolower((unsigned char) *p);
    p++;
  }

  return s;
}

/**
 * mutt_strchrnul - Find first occurrence of character in string
 * @param s Haystack
 * @param c Needle
 * @retval ptr Success, first occurrence of the character
 * @retval ptr Failure, pointer to the terminating NUL character
 *
 * This function is like GNU's strchrnul, which is similar to the standard
 * strchr function: it looks for the c character in the NULL-terminated string
 * s and returns a pointer to its location. If c is not in s, instead of
 * returning NULL like its standard counterpart, this function returns a
 * pointer to the terminating NUL character.
 */
const char *mutt_strchrnul(const char *s, char c)
{
  for (; *s && (*s != c); s++)
    ;
  return s;
}

/**
 * mutt_substrcpy - Copy a sub-string into a buffer
 * @param dest    Buffer for the result
 * @param begin     Start of the string to copy
 * @param end     End of the string to copy
 * @param destlen Length of buffer
 * @retval ptr Destination buffer
 */
char *mutt_substrcpy(char *dest, const char *begin, const char *end, size_t destlen)
{
  size_t len;

  len = end - begin;
  if (len > (destlen - 1))
    len = destlen - 1;
  memcpy(dest, begin, len);
  dest[len] = 0;
  return dest;
}

/**
 * mutt_substrdup - Duplicate a sub-string
 * @param begin Start of the string to copy
 * @param end   End of the string to copy
 * @retval ptr New string
 *
 * If end is NULL, then the rest of the string from begin will be copied.
 *
 * The caller must free the returned string.
 */
char *mutt_substrdup(const char *begin, const char *end)
{
  size_t len;
  char *p = NULL;

  if (end)
    len = end - begin;
  else
    len = strlen(begin);

  p = safe_malloc(len + 1);
  memcpy(p, begin, len);
  p[len] = 0;
  return p;
}

/**
 * mutt_strcmp - Compare two strings, safely
 * @param a First string to compare
 * @param b Second string to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int mutt_strcmp(const char *a, const char *b)
{
  return strcmp(NONULL(a), NONULL(b));
}

/**
 * mutt_strcasecmp - Compare two strings ignoring case, safely
 * @param a First string to compare
 * @param b Second string to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int mutt_strcasecmp(const char *a, const char *b)
{
  return strcasecmp(NONULL(a), NONULL(b));
}

/**
 * mutt_strncmp - Compare two strings (to a maximum), safely
 * @param a First string to compare
 * @param b Second string to compare
 * @param l Maximum number of bytes to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int mutt_strncmp(const char *a, const char *b, size_t l)
{
  return strncmp(NONULL(a), NONULL(b), l);
}

/**
 * mutt_strncasecmp - Compare two strings ignoring case (to a maximum), safely
 * @param a First string to compare
 * @param b Second string to compare
 * @param l Maximum number of bytes to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int mutt_strncasecmp(const char *a, const char *b, size_t l)
{
  return strncasecmp(NONULL(a), NONULL(b), l);
}

/**
 * mutt_strlen - Calculate the length of a string, safely
 * @param a String to measure
 * @retval num Length in bytes
 */
size_t mutt_strlen(const char *a)
{
  return a ? strlen(a) : 0;
}

/**
 * mutt_strcoll - Collate two strings (compare using locale), safely
 * @param a First string to compare
 * @param b Second string to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int mutt_strcoll(const char *a, const char *b)
{
  return strcoll(NONULL(a), NONULL(b));
}

/**
 * mutt_stristr - Find first occurrence of string (ignoring case)
 * @param haystack String to search through
 * @param needle   String to find
 * @retval ptr  First match of the search string
 * @retval NULL No match, or an error
 */
const char *mutt_stristr(const char *haystack, const char *needle)
{
  const char *p = NULL, *q = NULL;

  if (!haystack)
    return NULL;
  if (!needle)
    return haystack;

  while (*(p = haystack))
  {
    for (q = needle;
         *p && *q && tolower((unsigned char) *p) == tolower((unsigned char) *q); p++, q++)
      ;
    if (!*q)
      return haystack;
    haystack++;
  }
  return NULL;
}

/**
 * mutt_skip_whitespace - Find the first non-whitespace character in a string
 * @param p String to search
 * @retval ptr First non-whitespace character
 * @retval ptr Terminating NUL character, if the string was entirely whitespace
 */
char *mutt_skip_whitespace(char *p)
{
  SKIPWS(p);
  return p;
}

/**
 * mutt_remove_trailing_ws - Trim trailing whitespace from a string
 * @param s String to trim
 *
 * The string is modified in place.
 */
void mutt_remove_trailing_ws(char *s)
{
  char *p = NULL;

  for (p = s + mutt_strlen(s) - 1; p >= s && ISSPACE(*p); p--)
    *p = 0;
}

/**
 * strfcpy - Copy a string into a buffer (guaranteeing NUL-termination)
 * @param dest Buffer for the result
 * @param src  String to copy
 * @param dlen Length of buffer
 * @retval ptr Destination buffer
 */
char *strfcpy(char *dest, const char *src, size_t dlen)
{
  char *dest0 = dest;
  while ((--dlen > 0) && (*src != '\0'))
    *dest++ = *src++;

  *dest = '\0';
  return dest0;
}

/**
 * skip_email_wsp - Skip over whitespace as defined by RFC5322
 * @param s String to search
 * @retval ptr First non-whitespace character
 * @retval ptr Terminating NUL character, if the string was entirely whitespace
 *
 * This is used primarily for parsing header fields.
 */
char *skip_email_wsp(const char *s)
{
  if (s)
    return (char *) (s + strspn(s, EMAIL_WSP));
  return (char *) s;
}

/**
 * is_email_wsp - Is this a whitespace character (for an email header)
 * @param c Character to test
 * @retval boolean
 */
int is_email_wsp(char c)
{
  return c && (strchr(EMAIL_WSP, c) != NULL);
}

/**
 * strnfcpy - Copy a limited string into a buffer (guaranteeing NUL-termination)
 * @param dest Buffer for the result
 * @param src  String to copy
 * @param size Maximum number of characters to copy
 * @param dlen Length of buffer
 * @retval ptr Destination buffer
 */
char *strnfcpy(char *dest, char *src, size_t size, size_t dlen)
{
  if (dlen > size)
    dlen = size - 1;
  return strfcpy(dest, src, dlen);
}

/**
 * lwslen - Measure the linear-white-space at the beginning of a string
 * @param s String to check
 * @param n Maximum number of characters to check
 * @retval num Count of whitespace characters
 *
 * Count the number of whitespace characters at the beginning of a string.
 * They can be `<space>`, `<tab>`, `<cr>` or `<lf>`.
 */
size_t lwslen(const char *s, size_t n)
{
  const char *p = s;
  size_t len = n;

  if (n <= 0)
    return 0;

  for (; p < (s + n); p++)
  {
    if (!strchr(" \t\r\n", *p))
    {
      len = p - s;
      break;
    }
  }

  if (strchr("\r\n", *(p - 1))) /* LWS doesn't end with CRLF */
    len = 0;
  return len;
}

/**
 * lwsrlen - Measure the linear-white-space at the end of a string
 * @param s String to check
 * @param n Maximum number of characters to check
 * @retval num Count of whitespace characters
 *
 * Count the number of whitespace characters at the end of a string.
 * They can be `<space>`, `<tab>`, `<cr>` or `<lf>`.
 */
size_t lwsrlen(const char *s, size_t n)
{
  const char *p = s + n - 1;
  size_t len = n;

  if (n <= 0)
    return 0;

  if (strchr("\r\n", *p)) /* LWS doesn't end with CRLF */
    return 0;

  for (; p >= s; p--)
  {
    if (!strchr(" \t\r\n", *p))
    {
      len = s + n - 1 - p;
      break;
    }
  }

  return len;
}

/**
 * rfc822_dequote_comment - Un-escape characters in an email address comment
 * @param s String to the un-escaped
 *
 * @note The string is changed in-place
 */
void rfc822_dequote_comment(char *s)
{
  char *w = s;

  for (; *s; s++)
  {
    if (*s == '\\')
    {
      if (!*++s)
        break; /* error? */
      *w++ = *s;
    }
    else if (*s != '\"')
    {
      if (w != s)
        *w = *s;
      w++;
    }
  }
  *w = 0;
}

/**
 * next_word - Find the next word in a string
 * @param s String to examine
 * @retval ptr Next word
 *
 * If the s is pointing to a word (non-space) is is skipped over.
 * Then, any whitespace is skipped over.
 *
 * @note What is/isn't a word is determined by isspace()
 */
const char *next_word(const char *s)
{
  while (*s && !ISSPACE(*s))
    s++;
  SKIPWS(s);
  return s;
}
