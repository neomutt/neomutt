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
#include "list.h"
#include "logging.h"
#include "memory.h"
#include "message.h"
#include "string2.h"
#ifdef HAVE_SYSEXITS_H
#include <sysexits.h>
#endif

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
 * char_cmp_identity - Compare two characters
 * @param a First character to compare
 * @param b Second character to compare
 * @retval true If (a == b)
 */
static inline bool char_cmp_identity(char a, char b)
{
  return a == b;
}

/**
 * char_cmp_lower - Compare two characters ignoring case
 * @param a First character to compare
 * @param b Second character to compare
 * @retval true If (a == b), ignoring case
 */
static inline bool char_cmp_lower(char a, char b)
{
  return tolower((unsigned char) a) == tolower((unsigned char) b);
}

/**
 * char_cmp - Pointer to a function taking two characters and returning bool
 */
typedef bool (*char_cmp)(char, char);

/**
 * get_char_cmp - Retrieve the correct function to compare characters according to a case sensitivity setting
 * @param cs Case sensitivity setting
 * @retval ptr char_cmp function pointer
 */
static char_cmp get_char_cmp(enum CaseSensitivity cs)
{
  return (cs == CASE_IGNORE) ? char_cmp_lower : char_cmp_identity;
}

/**
 * mutt_str_startswith - Check whether a string starts with a prefix
 * @param str String to check
 * @param prefix Prefix to match
 * @param cs Case sensitivity setting
 * @retval num Length of prefix if str starts with prefix
 * @retval 0 if str does not start with prefix
 */
size_t mutt_str_startswith(const char *str, const char *prefix, enum CaseSensitivity cs)
{
  if (!str || (str[0] == '\0') || !prefix || !prefix[0])
  {
    return 0;
  }

  const char *saved_prefix = prefix;
  for (char_cmp f = get_char_cmp(cs); *str && *prefix; str++, prefix++)
  {
    if (!f(*str, *prefix))
    {
      return 0;
    }
  }

  return (!*prefix) ? prefix - saved_prefix : 0;
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

  if (!str || !*str) /* no input: 0 */
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

  if (!str || !*str) /* no input: 0 */
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

  if (!str || !*str) /* no input: 0 */
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
 * mutt_str_strdup - Copy a string, safely
 * @param str String to copy
 * @retval ptr  Copy of the string
 * @retval NULL if str was NULL
 */
char *mutt_str_strdup(const char *str)
{
  if (!str || !*str)
    return NULL;

  const size_t len = strlen(str) + 1;
  char *copy = mutt_mem_malloc(len);
  memcpy(copy, str, len);
  return copy;
}

/**
 * mutt_str_strcat - Concatenate two strings
 * @param buf    Buffer containing source string
 * @param buflen Length of buffer
 * @param s      String to add
 * @retval ptr Start of joined string
 */
char *mutt_str_strcat(char *buf, size_t buflen, const char *s)
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
 * mutt_str_strncat - Concatenate two strings
 * @param d  Buffer containing source string
 * @param l  Length of buffer
 * @param s  String to add
 * @param sl Maximum amount of string to add
 * @retval ptr Start of joined string
 *
 * Add a string to a maximum of @a sl bytes.
 */
char *mutt_str_strncat(char *d, size_t l, const char *s, size_t sl)
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
 *
 * This function free()s the original string, strdup()s the new string and
 * overwrites the pointer to the first string.
 *
 * This function alters the pointer of the caller.
 */
void mutt_str_replace(char **p, const char *s)
{
  if (!p)
    return;
  FREE(p);
  *p = mutt_str_strdup(s);
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

  size_t sz = mutt_str_strlen(item);
  size_t ssz = mutt_str_strlen(*str);

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
 * mutt_str_strlower - convert all characters in the string to lowercase
 * @param s String to lowercase
 * @retval ptr Lowercase string
 *
 * The string is transformed in place.
 */
char *mutt_str_strlower(char *s)
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
 * mutt_str_strchrnul - Find first occurrence of character in string
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
const char *mutt_str_strchrnul(const char *s, char c)
{
  if (!s)
    return NULL;

  for (; *s && (*s != c); s++)
    ;
  return s;
}

/**
 * mutt_str_substr_cpy - Copy a sub-string into a buffer
 * @param dest    Buffer for the result
 * @param begin     Start of the string to copy
 * @param end     End of the string to copy
 * @param destlen Length of buffer
 * @retval ptr Destination buffer
 */
char *mutt_str_substr_cpy(char *dest, const char *begin, const char *end, size_t destlen)
{
  if (!dest || !begin || !end || (destlen == 0))
    return dest;

  size_t len = end - begin;
  if (len > (destlen - 1))
    len = destlen - 1;
  memcpy(dest, begin, len);
  dest[len] = '\0';
  return dest;
}

/**
 * mutt_str_substr_dup - Duplicate a sub-string
 * @param begin Start of the string to copy
 * @param end   End of the string to copy
 * @retval ptr New string
 *
 * If end is NULL, then the rest of the string from begin will be copied.
 *
 * The caller must free the returned string.
 */
char *mutt_str_substr_dup(const char *begin, const char *end)
{
  size_t len;
  char *p = NULL;

  if (!begin)
  {
    mutt_debug(LL_DEBUG1, "%s: ERROR: 'begin' is NULL\n", __func__);
    return NULL;
  }

  if (end)
  {
    if (begin > end)
      return NULL;
    len = end - begin;
  }
  else
  {
    len = strlen(begin);
  }

  p = mutt_mem_malloc(len + 1);
  memcpy(p, begin, len);
  p[len] = '\0';
  return p;
}

/**
 * mutt_str_strcmp - Compare two strings, safely
 * @param a First string to compare
 * @param b Second string to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int mutt_str_strcmp(const char *a, const char *b)
{
  return strcmp(NONULL(a), NONULL(b));
}

/**
 * mutt_str_strcasecmp - Compare two strings ignoring case, safely
 * @param a First string to compare
 * @param b Second string to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int mutt_str_strcasecmp(const char *a, const char *b)
{
  return strcasecmp(NONULL(a), NONULL(b));
}

/**
 * mutt_str_strncmp - Compare two strings (to a maximum), safely
 * @param a First string to compare
 * @param b Second string to compare
 * @param l Maximum number of bytes to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int mutt_str_strncmp(const char *a, const char *b, size_t l)
{
  return strncmp(NONULL(a), NONULL(b), l);
}

/**
 * mutt_str_strncasecmp - Compare two strings ignoring case (to a maximum), safely
 * @param a First string to compare
 * @param b Second string to compare
 * @param l Maximum number of bytes to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int mutt_str_strncasecmp(const char *a, const char *b, size_t l)
{
  return strncasecmp(NONULL(a), NONULL(b), l);
}

/**
 * mutt_str_strlen - Calculate the length of a string, safely
 * @param a String to measure
 * @retval num Length in bytes
 */
size_t mutt_str_strlen(const char *a)
{
  return a ? strlen(a) : 0;
}

/**
 * mutt_str_strcoll - Collate two strings (compare using locale), safely
 * @param a First string to compare
 * @param b Second string to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int mutt_str_strcoll(const char *a, const char *b)
{
  return strcoll(NONULL(a), NONULL(b));
}

/**
 * mutt_str_stristr - Find first occurrence of string (ignoring case)
 * @param haystack String to search through
 * @param needle   String to find
 * @retval ptr  First match of the search string
 * @retval NULL No match, or an error
 */
const char *mutt_str_stristr(const char *haystack, const char *needle)
{
  const char *p = NULL, *q = NULL;

  if (!haystack)
    return NULL;
  if (!needle)
    return haystack;

  while (*(p = haystack))
  {
    for (q = needle;
         *p && *q && (tolower((unsigned char) *p) == tolower((unsigned char) *q));
         p++, q++)
    {
    }
    if (!*q)
      return haystack;
    haystack++;
  }
  return NULL;
}

/**
 * mutt_str_skip_whitespace - Find the first non-whitespace character in a string
 * @param p String to search
 * @retval ptr First non-whitespace character
 * @retval ptr Terminating NUL character, if the string was entirely whitespace
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

  for (char *p = s + mutt_str_strlen(s) - 1; (p >= s) && IS_SPACE(*p); p--)
    *p = '\0';
}

/**
 * mutt_str_strfcpy - Copy a string into a buffer (guaranteeing NUL-termination)
 * @param dest  Buffer for the result
 * @param src   String to copy
 * @param dsize Destination buffer size
 * @retval num Destination string length
 */
size_t mutt_str_strfcpy(char *dest, const char *src, size_t dsize)
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
 * @retval ptr First non-whitespace character
 * @retval ptr Terminating NUL character, if the string was entirely whitespace
 *
 * This is used primarily for parsing header fields.
 */
char *mutt_str_skip_email_wsp(const char *s)
{
  if (!s)
    return NULL;
  return (char *) (s + strspn(s, EMAIL_WSP));
}

/**
 * mutt_str_is_email_wsp - Is this a whitespace character (for an email header)
 * @param c Character to test
 * @retval true It is whitespcae
 */
bool mutt_str_is_email_wsp(char c)
{
  return c && (strchr(EMAIL_WSP, c));
}

/**
 * mutt_str_strnfcpy - Copy a limited string into a buffer (guaranteeing NUL-termination)
 * @param dest  Buffer for the result
 * @param src   String to copy
 * @param n     Maximum number of characters to copy
 * @param dsize Destination buffer size
 * @retval num Destination string length
 */
size_t mutt_str_strnfcpy(char *dest, const char *src, size_t n, size_t dsize)
{
  return mutt_str_strfcpy(dest, src, MIN(n + 1, dsize));
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
 * @param s String to the un-escaped
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
 * mutt_str_rstrnstr - Find last instance of a substring
 * @param haystack        String to search through
 * @param haystack_length Length of the string
 * @param needle          String to find
 * @retval NULL String not found
 * @retval ptr  Location of string
 *
 * Return the last instance of needle in the haystack, or NULL.
 * Like strstr(), only backwards, and for a limited haystack length.
 */
const char *mutt_str_rstrnstr(const char *haystack, size_t haystack_length, const char *needle)
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
 * mutt_str_word_casecmp - Find word a in word list b
 * @param a Word to find
 * @param b String to check
 * @retval 0   Word was found
 * @retval !=0 Word was not found
 *
 * Given a word "apple", check if it exists at the start of a string of words,
 * e.g. "apple banana".  It must be an exact match, so "apple" won't match
 * "apples banana".
 *
 * The case of the words is ignored.
 */
int mutt_str_word_casecmp(const char *a, const char *b)
{
  if (!b)
  {
    if (a)
      return 1;
    else
      return 0;
  }

  char tmp[128] = { 0 };

  int i;
  for (i = 0; i < (sizeof(tmp) - 2); i++, b++)
  {
    if (!*b || IS_SPACE(*b))
    {
      tmp[i] = '\0';
      break;
    }
    tmp[i] = *b;
  }
  tmp[i + 1] = '\0';

  return mutt_str_strcasecmp(a, tmp);
}

/**
 * mutt_str_is_ascii - Is a string ASCII (7-bit)?
 * @param p   String to examine
 * @param len Length of string
 * @retval true There are no 8-bit chars
 */
bool mutt_str_is_ascii(const char *p, size_t len)
{
  const char *s = p;
  while (s && ((unsigned int) (s - p) < len))
  {
    if ((*s & 0x80) != 0)
      return false;
    s++;
  }
  return true;
}

/**
 * mutt_str_find_word - Find the next word (non-space)
 * @param src String to search
 * @retval ptr Beginning of the next word
 *
 * Skip to the end of the current word.
 * Skip past any whitespace characters.
 *
 * @note If there aren't any more words, this will return a pointer to the
 *       final NUL character.
 */
const char *mutt_str_find_word(const char *src)
{
  const char *p = src;

  while (p && *p && strchr(" \t\n", *p))
    p++;
  while (p && *p && !strchr(" \t\n", *p))
    p++;
  return p;
}

/**
 * mutt_str_pretty_size - Display an abbreviated size, like 3.4K
 * @param buf    Buffer for the result
 * @param buflen Length of the buffer
 * @param num    Number to abbreviate
 */
void mutt_str_pretty_size(char *buf, size_t buflen, size_t num)
{
  if (!buf)
    return;

  if (num < 1000)
  {
    snprintf(buf, buflen, "%dB", (int) num);
  }
  else if (num < 10189) /* 0.1K - 9.9K */
  {
    snprintf(buf, buflen, "%3.1fK", num / 1024.0);
  }
  else if (num < 1023949) /* 10K - 999K */
  {
    /* 51 is magic which causes 10189/10240 to be rounded up to 10 */
    snprintf(buf, buflen, "%zuK", (num + 51) / 1024);
  }
  else if (num < 10433332) /* 1.0M - 9.9M */
  {
    snprintf(buf, buflen, "%3.1fM", num / 1048576.0);
  }
  else /* 10M+ */
  {
    /* (10433332 + 52428) / 1048576 = 10 */
    snprintf(buf, buflen, "%zuM", (num + 52428) / 1048576);
  }
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

  size_t slen = mutt_str_strlen(buf + xlen);
  size_t rlen = mutt_str_strlen(rstr);

  memmove(buf + rlen, buf + xlen, slen + 1);
  memmove(buf, rstr, rlen);

  return true;
}

/**
 * mutt_str_remall_strcasestr - Remove all occurrences of substring, ignoring case
 * @param str     String containing the substring
 * @param target  Target substring for removal
 * @retval 0 String contained substring and substring was removed successfully
 * @retval 1 String did not contain substring
 */
int mutt_str_remall_strcasestr(char *str, const char *target)
{
  int rc = 1;

  // Look through an ensure all instances of the substring are gone.
  while ((str = (char *) mutt_str_strcasestr(str, target)))
  {
    size_t target_len = mutt_str_strlen(target);
    memmove(str, str + target_len, 1 + strlen(str + target_len));
    rc = 0; // If we got here, then a substring existed and has been removed.
  }

  return rc;
}

/**
 * mutt_str_strcasestr - Find a substring within a string without worrying about case
 * @param haystack String that may or may not contain the substring
 * @param needle   Substring we're looking for
 * @retval ptr  Beginning of substring
 * @retval NULL Substring is not in substring
 *
 * This performs a byte-to-byte check so it will return unspecified
 * results for multibyte locales.
 */
const char *mutt_str_strcasestr(const char *haystack, const char *needle)
{
  if (!needle)
    return NULL;

  size_t haystack_len = mutt_str_strlen(haystack);
  size_t needle_len = mutt_str_strlen(needle);

  // Empty string exists at the front of a string. Check strstr if you don't believe me.
  if (needle_len == 0)
    return haystack;

  // Check size conditions. No point wasting CPU cycles.
  if ((haystack_len == 0) || (haystack_len < needle_len))
    return NULL;

  // Only check space that needle could fit in.
  // Conditional has + 1 to handle when the haystack and needle are the same length.
  for (size_t i = 0; i < (haystack_len - needle_len) + 1; i++)
  {
    for (size_t j = 0; j < needle_len; j++)
    {
      if (tolower((unsigned char) haystack[i + j]) != tolower((unsigned char) needle[j]))
        break;

      // If this statement is true, the needle has been found.
      if (j == (needle_len - 1))
        return haystack + i;
    }
  }

  return NULL;
}

/**
 * mutt_str_split - return list of the words of the strings
 * @param src String to split
 * @param sep Word separator
 */
struct ListHead mutt_str_split(const char *src, char sep)
{
  struct ListHead head = STAILQ_HEAD_INITIALIZER(head);

  if (!src || !*src)
    return head;

  while (true)
  {
    const char *start = src;
    while (*src && (*src != sep))
      src++;

    mutt_list_insert_tail(&head, mutt_str_substr_dup(start, src));

    if (!*src)
      break;

    src++;
  }

  return head;
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
    mutt_error(_("Out of memory"));
    mutt_exit(1);
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
