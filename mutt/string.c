/**
 * @file
 * String manipulation functions
 *
 * @authors
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2018-2020 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2021 Austin Ray <austin@austinray.io>
 * Copyright (C) 2022 Claes Nästén <pekdon@gmail.com>
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
 * @page mutt_string String manipulation functions
 *
 * Lots of commonly-used string manipulation routines.
 */

#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <stdarg.h> // IWYU pragma: keep
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "exit.h"
#include "logging2.h"
#include "memory.h"
#include "string2.h"
#ifdef HAVE_SYSEXITS_H
#include <sysexits.h>
#endif

#ifndef HAVE_STRCASESTR
/**
 * strcasestr - Find the first occurrence of needle in haystack, ignoring case
 * @param haystack String to search
 * @param needle   String to find
 * @retval ptr Matched string, or NULL on failure
 */
static char *strcasestr(const char *haystack, const char *needle)
{
  size_t haystackn = strlen(haystack);
  size_t needlen = strlen(needle);

  const char *p = haystack;
  while (haystackn >= needlen)
  {
    if (strncasecmp(p, needle, needlen) == 0)
      return (char *) p;
    p++;
    haystackn--;
  }
  return NULL;
}
#endif /* HAVE_STRCASESTR */

#ifndef HAVE_STRSEP
/**
 * strsep - Extract a token from a string
 * @param stringp String to be split up
 * @param delim   Characters to split stringp at
 * @retval ptr Next token, or NULL if the no more tokens
 *
 * @note The pointer stringp will be moved and NULs inserted into it
 */
static char *strsep(char **stringp, const char *delim)
{
  if (!*stringp)
    return NULL;

  char *start = *stringp;
  for (char *p = *stringp; *p != '\0'; p++)
  {
    for (const char *s = delim; *s != '\0'; s++)
    {
      if (*p == *s)
      {
        *p = '\0';
        *stringp = p + 1;
        return start;
      }
    }
  }
  *stringp = NULL;
  return start;
}
#endif /* HAVE_STRSEP */

/**
 * struct SysExits - Lookup table of error messages
 */
struct SysExits
{
  int err_num;         ///< Error number, see errno(3)
  const char *err_str; ///< Human-readable string for error
};

/// Lookup table of error messages
static const struct SysExits SysExits[] = {
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
  for (size_t i = 0; i < mutt_array_size(SysExits); i++)
  {
    if (err_num == SysExits[i].err_num)
      return SysExits[i].err_str;
  }

  return NULL;
}

/**
 * mutt_str_sep - Find first occurrence of any of delim characters in *stringp
 * @param stringp Pointer to string to search for delim, updated with position of after delim if found else NULL
 * @param delim   String with characters to search for in *stringp
 * @retval ptr Input value of *stringp
 */
char *mutt_str_sep(char **stringp, const char *delim)
{
  if (!stringp || !*stringp || !delim)
    return NULL;
  return strsep(stringp, delim);
}

/**
 * startswith - Check whether a string starts with a prefix
 * @param str String to check
 * @param prefix Prefix to match
 * @param match_case True if case needs to match
 * @retval num Length of prefix if str starts with prefix
 * @retval 0   str does not start with prefix
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
 * @retval 0   str does not start with prefix
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
 * @retval 0   str does not start with prefix
 */
size_t mutt_istr_startswith(const char *str, const char *prefix)
{
  return startswith(str, prefix, false);
}

/**
 * mutt_str_dup - Copy a string, safely
 * @param str String to copy
 * @retval ptr  Copy of the string
 * @retval NULL str was NULL or empty
 */
char *mutt_str_dup(const char *str)
{
  if (!str || (*str == '\0'))
    return NULL;

  char *p = strdup(str);
  if (!p)
  {
    mutt_error("%s", strerror(errno)); // LCOV_EXCL_LINE
    mutt_exit(1);                      // LCOV_EXCL_LINE
  }
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
 * mutt_str_adjust - Shrink-to-fit a string
 * @param[out] ptr String to alter
 *
 * Take a string which is allocated on the heap, find its length and reallocate
 * the memory to be exactly the right size.
 *
 * This function alters the pointer of the caller.
 */
void mutt_str_adjust(char **ptr)
{
  if (!ptr || !*ptr)
    return;
  MUTT_MEM_REALLOC(ptr, strlen(*ptr) + 1, char);
}

/**
 * mutt_str_lower - Convert all characters in the string to lowercase
 * @param str String to lowercase
 * @retval ptr Lowercase string
 *
 * The string is transformed in place.
 */
char *mutt_str_lower(char *str)
{
  if (!str)
    return NULL;

  char *p = str;

  while (*p)
  {
    *p = tolower((unsigned char) *p);
    p++;
  }

  return str;
}

/**
 * mutt_str_upper - Convert all characters in the string to uppercase
 * @param str String to uppercase
 * @retval ptr Uppercase string
 *
 * The string is transformed in place.
 */
char *mutt_str_upper(char *str)
{
  if (!str)
    return NULL;

  char *p = str;

  while (*p)
  {
    *p = toupper((unsigned char) *p);
    p++;
  }

  return str;
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

  char *p = MUTT_MEM_MALLOC(len + 1, char);
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
 * @param a   First string to compare
 * @param b   Second string to compare
 * @param num Maximum number of bytes to compare
 * @retval true First num chars of both strings are equal
 * @retval false First num chars of both strings not equal
 */
bool mutt_strn_equal(const char *a, const char *b, size_t num)
{
  return strncmp(NONULL(a), NONULL(b), num) == 0;
}

/**
 * mutt_istrn_cmp - Compare two strings ignoring case (to a maximum), safely
 * @param a   First string to compare
 * @param b   Second string to compare
 * @param num Maximum number of bytes to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int mutt_istrn_cmp(const char *a, const char *b, size_t num)
{
  return strncasecmp(NONULL(a), NONULL(b), num);
}

/**
 * mutt_istrn_equal - Check for equality of two strings ignoring case (to a maximum), safely
 * @param a   First string to compare
 * @param b   Second string to compare
 * @param num Maximum number of bytes to compare
 * @retval -1 a precedes b
 * @retval true First num chars of both strings are equal, ignoring case
 * @retval false First num chars of both strings not equal, ignoring case
 */
bool mutt_istrn_equal(const char *a, const char *b, size_t num)
{
  return strncasecmp(NONULL(a), NONULL(b), num) == 0;
}

/**
 * mutt_istrn_rfind - Find last instance of a substring, ignoring case
 * @param haystack        String to search through
 * @param haystack_length Length of the string
 * @param needle          String to find
 * @retval NULL String not found
 * @retval ptr  Location of string
 *
 * Return the last instance of needle in the haystack, or NULL.
 * Like strcasestr(), only backwards, and for a limited haystack length.
 */
const char *mutt_istrn_rfind(const char *haystack, size_t haystack_length, const char *needle)
{
  if (!haystack || (haystack_length == 0) || !needle)
    return NULL;

  int needle_length = strlen(needle);
  const char *haystack_end = haystack + haystack_length - needle_length;

  for (const char *p = haystack_end; p >= haystack; --p)
  {
    for (size_t i = 0; i < needle_length; i++)
    {
      if ((tolower((unsigned char) p[i]) != tolower((unsigned char) needle[i])))
        goto next;
    }
    return p;

  next:;
  }
  return NULL;
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
 * @retval <0 a precedes b
 * @retval  0 a and b are identical
 * @retval >0 b precedes a
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

  for (char *p = s + mutt_str_len(s) - 1; (p >= s) && isspace(*p); p--)
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
    mutt_error("%s", strerror(errno)); /* LCOV_EXCL_LINE */
    mutt_exit(1);                      /* LCOV_EXCL_LINE */
  }

  if (n == 0)
  {
    /* NeoMutt convention is to use NULL for 0-length strings */
    FREE(strp); /* LCOV_EXCL_LINE */
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

  *strp = MUTT_MEM_MALLOC(rlen, char);
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
        MUTT_MEM_REALLOC(strp, n + 1, char);
      return n;
    }
    /* increase size and try again */
    rlen = n + 1;
    MUTT_MEM_REALLOC(strp, rlen, char);
  }
  /* not reached */
}
#endif /* HAVE_ASPRINTF */

/**
 * mutt_str_hyphenate - Hyphenate a snake-case string
 * @param buf    Buffer for the result
 * @param buflen Length of the buffer
 * @param str    String to convert
 *
 * Replace underscores (`_`) with hyphens -`).
 */
void mutt_str_hyphenate(char *buf, size_t buflen, const char *str)
{
  if (!buf || (buflen == 0) || !str)
    return;

  mutt_str_copy(buf, str, buflen);
  for (; *buf != '\0'; buf++)
  {
    if (*buf == '_')
      *buf = '-';
  }
}

/**
 * mutt_str_inbox_cmp - Do two folders share the same path and one is an inbox - @ingroup sort_api
 * @param a First path
 * @param b Second path
 * @retval -1 a is INBOX of b
 * @retval  0 None is INBOX
 * @retval  1 b is INBOX for a
 *
 * This function compares two folder paths. It first looks for the position of
 * the last common '/' character. If a valid position is found and it's not the
 * last character in any of the two paths, the remaining parts of the paths are
 * compared (case insensitively) with the string "INBOX" followed by a non
 * alpha character, e.g., '.' or '/'. If only one of the two paths matches,
 * it's reported as being less than the other and the function returns -1 (a <
 * b) or 1 (a > b).  If both or no paths match the requirements, the two paths
 * are considered equivalent and this function returns 0.
 *
 * Examples:
 * * mutt_str_inbox_cmp("/foo/bar",      "/foo/baz") --> 0
 * * mutt_str_inbox_cmp("/foo/bar/",     "/foo/bar/inbox") --> 0
 * * mutt_str_inbox_cmp("/foo/bar/sent", "/foo/bar/inbox") --> 1
 * * mutt_str_inbox_cmp("=INBOX",        "=Drafts") --> -1
 * * mutt_str_inbox_cmp("=INBOX",        "=INBOX.Foo") --> 0
 * * mutt_str_inbox_cmp("=INBOX.Foo",    "=Drafts") --> -1
 */
int mutt_str_inbox_cmp(const char *a, const char *b)
{
#define IS_INBOX(s) (mutt_istrn_equal(s, "inbox", 5) && !isalnum((s)[5]))
#define CMP_INBOX(a, b) (IS_INBOX(b) - IS_INBOX(a))

  /* fast-track in case the paths have been mutt_pretty_mailbox'ified */
  if ((a[0] == '+') && (b[0] == '+'))
  {
    return CMP_INBOX(a + 1, b + 1);
  }

  const char *a_end = strrchr(a, '/');
  const char *b_end = strrchr(b, '/');

  /* If one path contains a '/', but not the other */
  if ((!a_end) ^ (!b_end))
    return 0;

  /* If neither path contains a '/' */
  if (!a_end)
    return 0;

  /* Compare the subpaths */
  size_t a_len = a_end - a;
  size_t b_len = b_end - b;
  size_t min = MIN(a_len, b_len);
  int same = (a[min] == '/') && (b[min] == '/') && (a[min + 1] != '\0') &&
             (b[min + 1] != '\0') && mutt_istrn_equal(a, b, min);

  if (!same)
    return 0;

  return CMP_INBOX(a + 1 + min, b + 1 + min);

#undef CMP_INBOX
#undef IS_INBOX
}
