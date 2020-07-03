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

/**
 * @page string String manipulation functions
 *
 * Lots of commonly-used string manipulation routines.
 */

#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "exit.h"
#include "logging.h"
#include "memory.h"
#include "message.h"
#include "string2.h"
#ifdef HAVE_SYSEXITS_H
#include <sysexits.h>
#endif

/**
 * typedef char_cmp - Pointer to a function taking two characters and returning bool
 */
typedef bool (*char_cmp)(char, char);

/**
 * struct SysExits - Lookup table of error messages
 */
struct SysExits
{
  int err_num;
  const char *err_str;
};

static const struct SysExits sysexits[] = {
#ifdef EX_USAGE
  { 0xff & EX_USAGE, "Bad usage." },
#endif
#ifdef EX_DATAERR
  { 0xff & EX_DATAERR, "Data format error." },
#endif
#ifdef EX_NOINPUT
  { 0xff & EX_NOINPUT, "Can't open input." },
#endif
#ifdef EX_NOUSER
  { 0xff & EX_NOUSER, "User unknown." },
#endif
#ifdef EX_NOHOST
  { 0xff & EX_NOHOST, "Host unknown." },
#endif
#ifdef EX_UNAVAILABLE
  { 0xff & EX_UNAVAILABLE, "Service unavailable." },
#endif
#ifdef EX_SOFTWARE
  { 0xff & EX_SOFTWARE, "Internal error." },
#endif
#ifdef EX_OSERR
  { 0xff & EX_OSERR, "Operating system error." },
#endif
#ifdef EX_OSFILE
  { 0xff & EX_OSFILE, "System file missing." },
#endif
#ifdef EX_CANTCREAT
  { 0xff & EX_CANTCREAT, "Can't create output." },
#endif
#ifdef EX_IOERR
  { 0xff & EX_IOERR, "I/O error." },
#endif
#ifdef EX_TEMPFAIL
  { 0xff & EX_TEMPFAIL, "Deferred." },
#endif
#ifdef EX_PROTOCOL
  { 0xff & EX_PROTOCOL, "Remote protocol error." },
#endif
#ifdef EX_NOPERM
  { 0xff & EX_NOPERM, "Insufficient permission." },
#endif
#ifdef EX_CONFIG
  { 0xff & EX_NOPERM, "Local configuration error." },
#endif
  { S_ERR, "Exec error." },
};

/**
 * mutt_str_sysexit - Return a string matching an error code
 * @param err_num Error code, e.g. EX_NOPERM
 * @retval ptr string representing the error code
 */
const char *mutt_str_sysexit(int err_num)
{
  for (size_t i = 0; i < mutt_array_size(sysexits); i++)
  {
    if (err_num == sysexits[i].err_num)
      return sysexits[i].err_str;
  }

  return NULL;
}

/**
 * startswith - Check whether a string starts with a prefix
 * @param str String to check
 * @param prefix Prefix to match
 * @param match_case True if case needs to match
 * @retval num Length of prefix if str starts with prefix
 * @retval 0 if str does not start with prefix
 */
static size_t startswith(const char *str, const char *prefix, bool match_case)
{
  if (!str || (str[0] == '\0') || !prefix || (prefix[0] == '\0'))
  {
    return 0;
  }

  const char *saved_prefix = prefix;
  for (; *str && *prefix; str++, prefix++)
  {
    if (*str == *prefix)
      continue;

    if (!match_case && tolower(*str) == tolower(*prefix))
      continue;

    return 0;
  }

  return (*prefix == '\0') ? (prefix - saved_prefix) : 0;
}

/**
 * mutt_str_startswith - Check whether a string starts with a prefix
 * @param str String to check
 * @param prefix Prefix to match
 * @retval num Length of prefix if str starts with prefix
 * @retval 0 if str does not start with prefix
 */
size_t mutt_str_startswith(const char *str, const char *prefix)
{
  return startswith(str, prefix, true);
}

/**
 * mutt_istr_startswith - Check whether a string starts with a prefix, ignoring case
 * @param str String to check
 * @param prefix Prefix to match
 * @retval num Length of prefix if str starts with prefix
 * @retval 0 if str does not start with prefix
 */
size_t mutt_istr_startswith(const char *str, const char *prefix)
{
  return startswith(str, prefix, false);
}

/**
 * mutt_str_atol - Convert ASCII string to a long
 * @param[in]  str String to read
 * @param[out] dst Store the result
 * @retval  0 Success
 * @retval -1 Error
 * @retval -2 Overflow
 *
 * This is a strtol() wrapper with range checking.
 * errno may be set on error, e.g. ERANGE
 */
int mutt_str_atol(const char *str, long *dst)
{
  if (dst)
    *dst = 0;

  if (!str || (*str == '\0')) /* no input: 0 */
    return 0;

  char *e = NULL;
  errno = 0;

  long res = strtol(str, &e, 10);
  if (dst)
    *dst = res;
  if (((res == LONG_MIN) || (res == LONG_MAX)) && (errno == ERANGE))
    return -2;
  if (e && (*e != '\0'))
    return -1;
  return 0;
}

/**
 * mutt_str_atos - Convert ASCII string to a short
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
int mutt_str_atos(const char *str, short *dst)
{
  if (dst)
    *dst = 0;

  long res = 0;
  int rc = mutt_str_atol(str, &res);
  if (rc < 0)
    return rc;
  if ((res < SHRT_MIN) || (res > SHRT_MAX))
    return -2;

  if (dst)
    *dst = (short) res;

  return 0;
}

/**
 * mutt_str_atoi - Convert ASCII string to an integer
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
int mutt_str_atoi(const char *str, int *dst)
{
  if (dst)
    *dst = 0;

  long res = 0;
  int rc = mutt_str_atol(str, &res);
  if (rc < 0)
    return rc;
  if ((res < INT_MIN) || (res > INT_MAX))
    return -2;

  if (dst)
    *dst = (int) res;

  return 0;
}

/**
 * mutt_str_atoui - Convert ASCII string to an unsigned integer
 * @param[in]  str String to read
 * @param[out] dst Store the result
 * @retval  1 Successful conversion, with trailing characters
 * @retval  0 Successful conversion
 * @retval -1 Invalid input
 * @retval -2 Input out of range
 *
 * @note This function's return value differs from the other functions.
 *       They return -1 if there is input beyond the number.
 */
int mutt_str_atoui(const char *str, unsigned int *dst)
{
  if (dst)
    *dst = 0;

  unsigned long res = 0;
  int rc = mutt_str_atoul(str, &res);
  if (rc < 0)
    return rc;
  if (res > UINT_MAX)
    return -2;

  if (dst)
    *dst = (unsigned int) res;

  return rc;
}

/**
 * mutt_str_atoul - Convert ASCII string to an unsigned long
 * @param[in]  str String to read
 * @param[out] dst Store the result
 * @retval  1 Successful conversion, with trailing characters
 * @retval  0 Successful conversion
 * @retval -1 Invalid input
 *
 * @note This function's return value differs from the other functions.
 *       They return -1 if there is input beyond the number.
 */
int mutt_str_atoul(const char *str, unsigned long *dst)
{
  if (dst)
    *dst = 0;

  if (!str || (*str == '\0')) /* no input: 0 */
    return 0;

  char *e = NULL;
  errno = 0;

  unsigned long res = strtoul(str, &e, 10);
  if (dst)
    *dst = res;
  if ((res == ULONG_MAX) && (errno == ERANGE))
    return -1;
  if (e && (*e != '\0'))
    return 1;
  return 0;
}

/**
 * mutt_str_atoull - Convert ASCII string to an unsigned long long
 * @param[in]  str String to read
 * @param[out] dst Store the result
 * @retval  1 Successful conversion, with trailing characters
 * @retval  0 Successful conversion
 * @retval -1 Invalid input
 *
 * @note This function's return value differs from the other functions.
 *       They return -1 if there is input beyond the number.
 */
int mutt_str_atoull(const char *str, unsigned long long *dst)
{
  if (dst)
    *dst = 0;

  if (!str || (*str == '\0')) /* no input: 0 */
    return 0;

  char *e = NULL;
  errno = 0;

  unsigned long long res = strtoull(str, &e, 10);
  if (dst)
    *dst = res;
  if ((res == ULLONG_MAX) && (errno == ERANGE))
    return -1;
  if (e && (*e != '\0'))
    return 1;
  return 0;
}

/**
 * mutt_str_dup - Copy a string, safely
 * @param str String to copy
 * @retval ptr  Copy of the string
 * @retval NULL if str was NULL or empty
 */
char *mutt_str_dup(const char *str)
{
  if (!str || (*str == '\0'))
    return NULL;

  return strdup(str);
}

/**
 * mutt_str_cat - Concatenate two strings
 * @param buf    Buffer containing source string
 * @param buflen Length of buffer
 * @param s      String to add
 * @retval ptr Start of the buffer
 */
char *mutt_str_cat(char *buf, size_t buflen, const char *s)
{
  if (!buf || (buflen == 0) || !s)
    return buf;

  char *p = buf;

  buflen--; /* Space for the trailing '\0'. */

  for (; (*buf != '\0') && buflen; buflen--)
    buf++;
  for (; *s && buflen; buflen--)
    *buf++ = *s++;

  *buf = '\0';

  return p;
}

/**
 * mutt_strn_cat - Concatenate two strings
 * @param d  Buffer containing source string
 * @param l  Length of buffer
 * @param s  String to add
 * @param sl Maximum amount of string to add
 * @retval ptr Start of joined string
 *
 * Add a string to a maximum of @a sl bytes.
 */
char *mutt_strn_cat(char *d, size_t l, const char *s, size_t sl)
{
  if (!d || (l == 0) || !s)
    return d;

  char *p = d;

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
 * @param[out] p String to replace
 * @param[in]  s New string
 * @retval ptr Replaced string
 *
 * This function free()s the original string, strdup()s the new string and
 * overwrites the pointer to the first string.
 *
 * This function alters the pointer of the caller.
 *
 * @note Free *p afterwards to handle the case that *p and s reference the same memory
 */
char *mutt_str_replace(char **p, const char *s)
{
  if (!p)
    return NULL;
  const char *tmp = *p;
  *p = mutt_str_dup(s);
  FREE(&tmp);
  return *p;
}

/**
 * mutt_str_append_item - Add string to another separated by sep
 * @param[out] str  String appended
 * @param[in]  item String to append
 * @param[in]  sep separator between string item
 *
 * Append a string to another, separating them by sep if needed.
 *
 * This function alters the pointer of the caller.
 */
void mutt_str_append_item(char **str, const char *item, char sep)
{
  if (!str || !item)
    return;

  size_t sz = mutt_str_len(item);
  size_t ssz = mutt_str_len(*str);

  mutt_mem_realloc(str, ssz + (((ssz > 0) && (sep != '\0')) ? 1 : 0) + sz + 1);
  char *p = *str + ssz;
  if ((ssz > 0) && (sep != '\0'))
    *p++ = sep;
  memcpy(p, item, sz + 1);
}

/**
 * mutt_str_adjust - Shrink-to-fit a string
 * @param[out] p String to alter
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
  mutt_mem_realloc(p, strlen(*p) + 1);
}

/**
 * mutt_str_lower - Convert all characters in the string to lowercase
 * @param s String to lowercase
 * @retval ptr Lowercase string
 *
 * The string is transformed in place.
 */
char *mutt_str_lower(char *s)
{
  if (!s)
    return NULL;

  char *p = s;

  while (*p)
  {
    *p = tolower((unsigned char) *p);
    p++;
  }

  return s;
}

/**
 * mutt_strn_copy - Copy a sub-string into a buffer
 * @param dest   Buffer for the result
 * @param src    Start of the string to copy
 * @param len    Length of the string to copy
 * @param dsize  Destination buffer size
 * @retval ptr Destination buffer
 */
char *mutt_strn_copy(char *dest, const char *src, size_t len, size_t dsize)
{
  if (!src || !dest || (len == 0) || (dsize == 0))
    return dest;

  if (len > (dsize - 1))
    len = dsize - 1;
  memcpy(dest, src, len);
  dest[len] = '\0';
  return dest;
}

/**
 * mutt_strn_dup - Duplicate a sub-string
 * @param begin Start of the string to copy
 * @param len   Length of string to copy
 * @retval ptr New string
 *
 * The caller must free the returned string.
 */
char *mutt_strn_dup(const char *begin, size_t len)
{
  if (!begin)
    return NULL;

  char *p = mutt_mem_malloc(len + 1);
  memcpy(p, begin, len);
  p[len] = '\0';
  return p;
}

/**
 * mutt_str_cmp - Compare two strings, safely
 * @param a First string to compare
 * @param b Second string to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int mutt_str_cmp(const char *a, const char *b)
{
  return strcmp(NONULL(a), NONULL(b));
}

/**
 * mutt_istr_cmp - Compare two strings ignoring case, safely
 * @param a First string to compare
 * @param b Second string to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int mutt_istr_cmp(const char *a, const char *b)
{
  return strcasecmp(NONULL(a), NONULL(b));
}

/**
 * mutt_strn_equal - Check for equality of two strings (to a maximum), safely
 * @param a First string to compare
 * @param b Second string to compare
 * @param l Maximum number of bytes to compare
 * @retval true First l chars of both strings are equal
 * @retval false First l chars of both strings not equal
 */
bool mutt_strn_equal(const char *a, const char *b, size_t l)
{
  return strncmp(NONULL(a), NONULL(b), l) == 0;
}

/**
 * mutt_istrn_cmp - Compare two strings ignoring case (to a maximum), safely
 * @param a First string to compare
 * @param b Second string to compare
 * @param l Maximum number of bytes to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int mutt_istrn_cmp(const char *a, const char *b, size_t l)
{
  return strncasecmp(NONULL(a), NONULL(b), l);
}

/**
 * mutt_istrn_equal - Check for equality of two strings ignoring case (to a maximum), safely
 * @param a First string to compare
 * @param b Second string to compare
 * @param l Maximum number of bytes to compare
 * @retval -1 a precedes b
 * @retval true First l chars of both strings are equal, ignoring case
 * @retval false First l chars of both strings not equal, ignoring case
 */
bool mutt_istrn_equal(const char *a, const char *b, size_t l)
{
  return strncasecmp(NONULL(a), NONULL(b), l) == 0;
}

/**
 * mutt_str_len - Calculate the length of a string, safely
 * @param a String to measure
 * @retval num Length in bytes
 */
size_t mutt_str_len(const char *a)
{
  return a ? strlen(a) : 0;
}

/**
 * mutt_str_coll - Collate two strings (compare using locale), safely
 * @param a First string to compare
 * @param b Second string to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int mutt_str_coll(const char *a, const char *b)
{
  return strcoll(NONULL(a), NONULL(b));
}

/**
 * mutt_istr_find - Find first occurrence of string (ignoring case)
 * @param haystack String to search through
 * @param needle   String to find
 * @retval ptr  First match of the search string
 * @retval NULL No match, or an error
 */
const char *mutt_istr_find(const char *haystack, const char *needle)
{
  if (!haystack)
    return NULL;
  if (!needle)
    return haystack;

  const char *p = NULL, *q = NULL;

  while (*(p = haystack))
  {
    for (q = needle;
         *p && *q && (tolower((unsigned char) *p) == tolower((unsigned char) *q));
         p++, q++)
    {
    }
    if ((*q == '\0'))
      return haystack;
    haystack++;
  }
  return NULL;
}

/**
 * mutt_str_skip_whitespace - Find the first non-whitespace character in a string
 * @param p String to search
 * @retval ptr
 * - First non-whitespace character
 * - Terminating NUL character, if the string was entirely whitespace
 */
char *mutt_str_skip_whitespace(const char *p)
{
  if (!p)
    return NULL;
  SKIPWS(p);
  return (char *) p;
}

/**
 * mutt_str_remove_trailing_ws - Trim trailing whitespace from a string
 * @param s String to trim
 *
 * The string is modified in place.
 */
void mutt_str_remove_trailing_ws(char *s)
{
  if (!s)
    return;

  for (char *p = s + mutt_str_len(s) - 1; (p >= s) && IS_SPACE(*p); p--)
    *p = '\0';
}

/**
 * mutt_str_copy - Copy a string into a buffer (guaranteeing NUL-termination)
 * @param dest  Buffer for the result
 * @param src   String to copy
 * @param dsize Destination buffer size
 * @retval num Destination string length
 */
size_t mutt_str_copy(char *dest, const char *src, size_t dsize)
{
  if (!dest || (dsize == 0))
    return 0;
  if (!src)
  {
    dest[0] = '\0';
    return 0;
  }

  char *dest0 = dest;
  while ((--dsize > 0) && (*src != '\0'))
    *dest++ = *src++;

  *dest = '\0';
  return dest - dest0;
}

/**
 * mutt_str_skip_email_wsp - Skip over whitespace as defined by RFC5322
 * @param s String to search
 * @retval ptr
 * - First non-whitespace character
 * - Terminating NUL character, if the string was entirely whitespace
 *
 * This is used primarily for parsing header fields.
 */
char *mutt_str_skip_email_wsp(const char *s)
{
  if (!s)
    return NULL;

  for (; mutt_str_is_email_wsp(*s); s++)
    ; // Do nothing

  return (char *) s;
}

/**
 * mutt_str_is_email_wsp - Is this a whitespace character (for an email header)
 * @param c Character to test
 * @retval true It is whitespcae
 */
bool mutt_str_is_email_wsp(char c)
{
  return c && strchr(" \t\r\n", c);
}

/**
 * mutt_str_lws_len - Measure the linear-white-space at the beginning of a string
 * @param s String to check
 * @param n Maximum number of characters to check
 * @retval num Count of whitespace characters
 *
 * Count the number of whitespace characters at the beginning of a string.
 * They can be `<space>`, `<tab>`, `<cr>` or `<lf>`.
 */
size_t mutt_str_lws_len(const char *s, size_t n)
{
  if (!s)
    return 0;

  const char *p = s;
  size_t len = n;

  if (n == 0)
    return 0;

  for (; p < (s + n); p++)
  {
    if (!strchr(" \t\r\n", *p))
    {
      len = p - s;
      break;
    }
  }

  if ((len != 0) && strchr("\r\n", *(p - 1))) /* LWS doesn't end with CRLF */
    len = 0;
  return len;
}

/**
 * mutt_str_lws_rlen - Measure the linear-white-space at the end of a string
 * @param s String to check
 * @param n Maximum number of characters to check
 * @retval num Count of whitespace characters
 *
 * Count the number of whitespace characters at the end of a string.
 * They can be `<space>`, `<tab>`, `<cr>` or `<lf>`.
 */
size_t mutt_str_lws_rlen(const char *s, size_t n)
{
  if (!s)
    return 0;

  const char *p = s + n - 1;
  size_t len = n;

  if (n == 0)
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
 * mutt_str_dequote_comment - Un-escape characters in an email address comment
 * @param s String to be un-escaped
 *
 * @note The string is changed in-place
 */
void mutt_str_dequote_comment(char *s)
{
  if (!s)
    return;

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
  *w = '\0';
}

/**
 * mutt_str_equal - Compare two strings
 * @param a First string
 * @param b Second string
 * @retval true The strings are equal
 * @retval false The strings are not equal
 */
bool mutt_str_equal(const char *a, const char *b)
{
  return (a == b) || (mutt_str_cmp(a, b) == 0);
}

/**
 * mutt_istr_equal - Compare two strings, ignoring case
 * @param a First string
 * @param b Second string
 * @retval true The strings are equal
 * @retval false The strings are not equal
 */
bool mutt_istr_equal(const char *a, const char *b)
{
  return (a == b) || (mutt_istr_cmp(a, b) == 0);
}

/**
 * mutt_str_next_word - Find the next word in a string
 * @param s String to examine
 * @retval ptr Next word
 *
 * If the s is pointing to a word (non-space) is is skipped over.
 * Then, any whitespace is skipped over.
 *
 * @note What is/isn't a word is determined by isspace()
 */
const char *mutt_str_next_word(const char *s)
{
  if (!s)
    return NULL;

  while (*s && !IS_SPACE(*s))
    s++;
  SKIPWS(s);
  return s;
}

/**
 * mutt_strn_rfind - Find last instance of a substring
 * @param haystack        String to search through
 * @param haystack_length Length of the string
 * @param needle          String to find
 * @retval NULL String not found
 * @retval ptr  Location of string
 *
 * Return the last instance of needle in the haystack, or NULL.
 * Like strstr(), only backwards, and for a limited haystack length.
 */
const char *mutt_strn_rfind(const char *haystack, size_t haystack_length, const char *needle)
{
  if (!haystack || (haystack_length == 0) || !needle)
    return NULL;

  int needle_length = strlen(needle);
  const char *haystack_end = haystack + haystack_length - needle_length;

  for (const char *p = haystack_end; p >= haystack; --p)
  {
    for (size_t i = 0; i < needle_length; i++)
    {
      if (p[i] != needle[i])
        goto next;
    }
    return p;

  next:;
  }
  return NULL;
}

/**
 * mutt_str_is_ascii - Is a string ASCII (7-bit)?
 * @param str String to examine
 * @param len Length of string to examine
 * @retval true There are no 8-bit chars
 */
bool mutt_str_is_ascii(const char *str, size_t len)
{
  if (!str)
    return true;

  for (; (*str != '\0') && (len > 0); str++, len--)
    if ((*str & 0x80) != 0)
      return false;

  return true;
}

/**
 * mutt_str_find_word - Find the end of a word (non-space)
 * @param src String to search
 * @retval ptr End of the word
 *
 * Skip to the end of the current word.
 * Skip past any whitespace characters.
 *
 * @note If there aren't any more words, this will return a pointer to the
 *       final NUL character.
 */
const char *mutt_str_find_word(const char *src)
{
  if (!src)
    return NULL;

  while (*src && strchr(" \t\n", *src))
    src++;
  while (*src && !strchr(" \t\n", *src))
    src++;
  return src;
}

/**
 * mutt_str_getenv - Get an environment variable
 * @param name Environment variable to get
 * @retval ptr Value of variable
 * @retval NULL Variable isn't set, or is empty
 *
 * @warning The caller must not free the returned pointer.
 */
const char *mutt_str_getenv(const char *name)
{
  if (!name)
    return NULL;

  const char *val = getenv(name);
  if (val && (val[0] != '\0'))
    return val;

  return NULL;
}

/**
 * mutt_str_inline_replace - Replace the beginning of a string
 * @param buf    Buffer to modify
 * @param buflen Length of buffer
 * @param xlen   Length of string to overwrite
 * @param rstr   Replacement string
 * @retval true Success
 *
 * String (`XX<OOOOOO>......`, 16, 2, `RRRR`) becomes `RRRR<OOOOOO>....`
 */
bool mutt_str_inline_replace(char *buf, size_t buflen, size_t xlen, const char *rstr)
{
  if (!buf || !rstr || (xlen >= buflen))
    return false;

  size_t slen = mutt_str_len(buf + xlen);
  size_t rlen = mutt_str_len(rstr);

  if ((slen + rlen) >= buflen)
    return false;

  memmove(buf + rlen, buf + xlen, slen + 1);
  memmove(buf, rstr, rlen);

  return true;
}

/**
 * mutt_istr_remall - Remove all occurrences of substring, ignoring case
 * @param str     String containing the substring
 * @param target  Target substring for removal
 * @retval 0 String contained substring and substring was removed successfully
 * @retval 1 String did not contain substring
 */
int mutt_istr_remall(char *str, const char *target)
{
  int rc = 1;
  if (!str || !target)
    return rc;

  // Look through an ensure all instances of the substring are gone.
  while ((str = (char *) strcasestr(str, target)))
  {
    size_t target_len = mutt_str_len(target);
    memmove(str, str + target_len, 1 + strlen(str + target_len));
    rc = 0; // If we got here, then a substring existed and has been removed.
  }

  return rc;
}

#ifdef HAVE_VASPRINTF
/**
 * mutt_str_asprintf - Format a string, allocating space as necessary
 * @param[out] strp New string saved here
 * @param[in]  fmt  Format string
 * @param[in]  ...  Format arguments
 * @retval num Characters written
 * @retval -1  Error
 */
int mutt_str_asprintf(char **strp, const char *fmt, ...)
{
  if (!strp || !fmt)
    return -1;

  va_list ap;
  int n;

  va_start(ap, fmt);
  n = vasprintf(strp, fmt, ap);
  va_end(ap);

  /* GNU libc man page for vasprintf(3) states that the value of *strp
   * is undefined when the return code is -1.  */
  if (n < 0)
  {
    mutt_error(_("Out of memory")); /* LCOV_EXCL_LINE */
    mutt_exit(1);                   /* LCOV_EXCL_LINE */
  }

  if (n == 0)
  {
    /* NeoMutt convention is to use NULL for 0-length strings */
    FREE(strp);
  }

  return n;
}
#else
/* Allocate a C-string large enough to contain the formatted string.
 * This is essentially malloc+sprintf in one.
 */
int mutt_str_asprintf(char **strp, const char *fmt, ...)
{
  if (!strp || !fmt)
    return -1;

  int rlen = 256;

  *strp = mutt_mem_malloc(rlen);
  while (true)
  {
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(*strp, rlen, fmt, ap);
    va_end(ap);
    if (n < 0)
    {
      FREE(strp);
      return n;
    }

    if (n < rlen)
    {
      /* reduce space to just that which was used.  note that 'n' does not
       * include the terminal nul char.  */
      if (n == 0) /* convention is to use NULL for zero-length strings. */
        FREE(strp);
      else if (n != rlen - 1)
        mutt_mem_realloc(strp, n + 1);
      return n;
    }
    /* increase size and try again */
    rlen = n + 1;
    mutt_mem_realloc(strp, rlen);
  }
  /* not reached */
}
#endif /* HAVE_ASPRINTF */
