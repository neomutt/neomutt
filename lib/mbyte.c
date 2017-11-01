/**
 * @file
 * Multi-byte String manipulation functions
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
 * @page mbyte Multi-byte String manipulation functions
 *
 * Some commonly-used multi-byte string manipulation routines.
 *
 * | Data             | Description
 * | :--------------- | :--------------------------------------------------
 * | #ReplacementChar | When a Unicode character can't be displayed, use this instead
 *
 * | Function        | Description
 * | :-------------- | :---------------------------------------------------------
 * | get_initials()  | Turn a name into initials
 * | is_shell_char() | Is character not typically part of a pathname
 * | mutt_charlen()  | Count the bytes in a (multibyte) character
 * | my_mbstowcs()   | Convert a string from multibyte to wide characters
 * | my_wcstombs()   | Convert a string from wide to multibyte characters
 * | my_wcswidth()   | Measure the screen width of a string
 * | my_wcwidth()    | Measure the screen width of a character
 * | my_width()      | Measure a string's display width (in screen columns)
 * | width_ceiling() | Keep the end of the string on-screen
 */

#include "config.h"
#include <stddef.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "mbyte.h"
#include "memory.h"
#include "string2.h"

bool OPT_LOCALES; /**< (pseudo) set if user has valid locale definition */

/**
 * ReplacementChar - When a Unicode character can't be displayed, use this instead
 */
wchar_t ReplacementChar = '?';

/**
 * mutt_charlen - Count the bytes in a (multibyte) character
 * @param[in]  s     String to be examined
 * @param[out] width Number of screen columns the character would use
 * @retval n  Number of bytes in the first (multibyte) character of input consumes
 * @retval <0 Conversion error
 * @retval =0 End of input
 * @retval >0 Length (bytes)
 */
int mutt_charlen(const char *s, int *width)
{
  wchar_t wc;
  mbstate_t mbstate;
  size_t k, n;

  if (!s || !*s)
    return 0;

  n = mutt_strlen(s);
  memset(&mbstate, 0, sizeof(mbstate));
  k = mbrtowc(&wc, s, n, &mbstate);
  if (width)
    *width = wcwidth(wc);
  return (k == (size_t)(-1) || k == (size_t)(-2)) ? -1 : k;
}

/**
 * get_initials - Turn a name into initials
 * @param name   String to be converted
 * @param buf    Buffer for the result
 * @param buflen Size of the buffer
 * @retval 1 on Success
 * @retval 0 on Failure
 *
 * Take a name, e.g. "John F. Kennedy" and reduce it to initials "JFK".
 * The function saves the first character from each word.  Words are delimited
 * by whitespace, or hyphens (so "Jean-Pierre" becomes "JP").
 */
bool get_initials(const char *name, char *buf, int buflen)
{
  if (!name || !buf)
    return false;

  while (*name)
  {
    /* Char's length in bytes */
    int clen = mutt_charlen(name, NULL);
    if (clen < 1)
      return false;

    /* Ignore punctuation at the beginning of a word */
    if ((clen == 1) && ispunct(*name))
    {
      name++;
      continue;
    }

    if (clen >= buflen)
      return false;

    /* Copy one multibyte character */
    buflen -= clen;
    while (clen--)
      *buf++ = *name++;

    /* Skip to end-of-word */
    for (; *name; name += clen)
    {
      clen = mutt_charlen(name, NULL);
      if (clen < 1)
        return false;
      else if ((clen == 1) && (isspace(*name) || (*name == '-')))
        break;
    }

    /* Skip any whitespace, or hyphens */
    while (*name && (isspace(*name) || (*name == '-')))
      name++;
  }

  *buf = 0;
  return true;
}

/**
 * my_width - Measure a string's display width (in screen columns)
 * @param str     String to measure
 * @param col     Display column (used for expanding tabs)
 * @param display will this be displayed to the user?
 * @retval int Strings width in screen columns
 *
 * This is like wcwidth(), but gets const char* not wchar_t*.
 */
int my_width(const char *str, int col, bool display)
{
  wchar_t wc;
  int l, w = 0, nl = 0;
  const char *p = str;

  while (p && *p)
  {
    if (mbtowc(&wc, p, MB_CUR_MAX) >= 0)
    {
      l = wcwidth(wc);
      if (l < 0)
        l = 1;
      /* correctly calc tab stop, even for sending as the
       * line should look pretty on the receiving end */
      if (wc == L'\t' || (nl && wc == L' '))
      {
        nl = 0;
        l = 8 - (col % 8);
      }
      /* track newlines for display-case: if we have a space
       * after a newline, assume 8 spaces as for display we
       * always tab-fold */
      else if (display && (wc == '\n'))
        nl = 1;
    }
    else
      l = 1;
    w += l;
    p++;
  }
  return w;
}

/**
 * my_wcwidth - Measure the screen width of a character
 * @param wc Character to examine
 * @retval int Width in screen columns
 */
int my_wcwidth(wchar_t wc)
{
  int n = wcwidth(wc);
  if (IsWPrint(wc) && n > 0)
    return n;
  if (!(wc & ~0x7f))
    return 2;
  if (!(wc & ~0xffff))
    return 6;
  return 10;
}

/**
 * my_wcswidth - Measure the screen width of a string
 * @param s String to measure
 * @param n Length of string in characters
 * @retval int Width in screen columns
 */
int my_wcswidth(const wchar_t *s, size_t n)
{
  int w = 0;
  while (n--)
    w += my_wcwidth(*s++);
  return w;
}

/**
 * width_ceiling - Keep the end of the string on-screen
 * @param s String being displayed
 * @param n Length of string in characters
 * @param w1 Width limit
 * @retval size_t Number of chars to skip
 *
 * Given a string and a width, determine how many characters from the
 * beginning of the string should be skipped so that the string fits.
 */
size_t width_ceiling(const wchar_t *s, size_t n, int w1)
{
  const wchar_t *s0 = s;
  int w = 0;
  for (; n; s++, n--)
    if ((w += my_wcwidth(*s)) > w1)
      break;
  return s - s0;
}

/**
 * my_wcstombs - Convert a string from wide to multibyte characters
 * @param dest Buffer for the result
 * @param dlen Length of the result buffer
 * @param src Source string to convert
 * @param slen Length of the source string
 */
void my_wcstombs(char *dest, size_t dlen, const wchar_t *src, size_t slen)
{
  mbstate_t st;
  size_t k;

  /* First convert directly into the destination buffer */
  memset(&st, 0, sizeof(st));
  for (; slen && dlen >= MB_LEN_MAX; dest += k, dlen -= k, src++, slen--)
    if ((k = wcrtomb(dest, *src, &st)) == (size_t)(-1))
      break;

  /* If this works, we can stop now */
  if (dlen >= MB_LEN_MAX)
  {
    wcrtomb(dest, 0, &st);
    return;
  }

  /* Otherwise convert any remaining data into a local buffer */
  {
    char buf[3 * MB_LEN_MAX];
    char *p = buf;

    for (; slen && p - buf < dlen; p += k, src++, slen--)
      if ((k = wcrtomb(p, *src, &st)) == (size_t)(-1))
        break;
    p += wcrtomb(p, 0, &st);

    /* If it fits into the destination buffer, we can stop now */
    if (p - buf <= dlen)
    {
      memcpy(dest, buf, p - buf);
      return;
    }

    /* Otherwise we truncate the string in an ugly fashion */
    memcpy(dest, buf, dlen);
    dest[dlen - 1] = '\0'; /* assume original dlen > 0 */
  }
}

/**
 * my_mbstowcs - Convert a string from multibyte to wide characters
 * @param pwbuf    Buffer for the result
 * @param pwbuflen Length of the result buffer
 * @param i        Starting index into the result buffer
 * @param buf      String to convert
 * @retval size_t First character after the result
 */
size_t my_mbstowcs(wchar_t **pwbuf, size_t *pwbuflen, size_t i, char *buf)
{
  wchar_t wc;
  mbstate_t st;
  size_t k;
  wchar_t *wbuf = NULL;
  size_t wbuflen;

  wbuf = *pwbuf;
  wbuflen = *pwbuflen;

  while (*buf)
  {
    memset(&st, 0, sizeof(st));
    for (; (k = mbrtowc(&wc, buf, MB_LEN_MAX, &st)) && k != (size_t)(-1) &&
           k != (size_t)(-2);
         buf += k)
    {
      if (i >= wbuflen)
      {
        wbuflen = i + 20;
        safe_realloc(&wbuf, wbuflen * sizeof(*wbuf));
      }
      wbuf[i++] = wc;
    }
    if (*buf && (k == (size_t) -1 || k == (size_t) -2))
    {
      if (i >= wbuflen)
      {
        wbuflen = i + 20;
        safe_realloc(&wbuf, wbuflen * sizeof(*wbuf));
      }
      wbuf[i++] = ReplacementChar;
      buf++;
    }
  }
  *pwbuf = wbuf;
  *pwbuflen = wbuflen;
  return i;
}

/**
 * is_shell_char - Is character not typically part of a pathname
 * @param ch Character to examine
 * @retval true  Character is not typically part of a pathname
 * @retval false Character is typically part of a pathname
 *
 * @note The name is very confusing.
 */
bool is_shell_char(wchar_t ch)
{
  static const wchar_t shell_chars[] = L"<>&()$?*;{}| "; /* ! not included because it can be part of a pathname in NeoMutt */
  return wcschr(shell_chars, ch) != NULL;
}

