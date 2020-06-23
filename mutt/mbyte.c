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
 */

#include "config.h"
#include <stddef.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include "mbyte.h"
#include "buffer.h"
#include "charset.h"
#include "memory.h"
#include "string2.h"

bool OptLocales; ///< (pseudo) set if user has valid locale definition

/**
 * mutt_mb_charlen - Count the bytes in a (multibyte) character
 * @param[in]  s     String to be examined
 * @param[out] width Number of screen columns the character would use
 * @retval num Bytes in the first (multibyte) character of input consumes
 * @retval <0  Conversion error
 * @retval =0  End of input
 * @retval >0  Length (bytes)
 */
int mutt_mb_charlen(const char *s, int *width)
{
  if (!s || (*s == '\0'))
    return 0;

  wchar_t wc;
  mbstate_t mbstate;
  size_t k, n;

  n = mutt_str_len(s);
  memset(&mbstate, 0, sizeof(mbstate));
  k = mbrtowc(&wc, s, n, &mbstate);
  if (width)
    *width = wcwidth(wc);
  return ((k == (size_t)(-1)) || (k == (size_t)(-2))) ? -1 : k;
}

/**
 * mutt_mb_get_initials - Turn a name into initials
 * @param name   String to be converted
 * @param buf    Buffer for the result
 * @param buflen Size of the buffer
 * @retval 1 Success
 * @retval 0 Failure
 *
 * Take a name, e.g. "John F. Kennedy" and reduce it to initials "JFK".
 * The function saves the first character from each word.  Words are delimited
 * by whitespace, or hyphens (so "Jean-Pierre" becomes "JP").
 */
bool mutt_mb_get_initials(const char *name, char *buf, size_t buflen)
{
  if (!name || !buf)
    return false;

  while (*name)
  {
    /* Char's length in bytes */
    int clen = mutt_mb_charlen(name, NULL);
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
      clen = mutt_mb_charlen(name, NULL);
      if (clen < 1)
        return false;
      if ((clen == 1) && (isspace(*name) || (*name == '-')))
        break;
    }

    /* Skip any whitespace, or hyphens */
    while (*name && (isspace(*name) || (*name == '-')))
      name++;
  }

  *buf = '\0';
  return true;
}

/**
 * mutt_mb_width - Measure a string's display width (in screen columns)
 * @param str     String to measure
 * @param col     Display column (used for expanding tabs)
 * @param display will this be displayed to the user?
 * @retval num Strings width in screen columns
 *
 * This is like wcwidth(), but gets const char* not wchar_t*.
 */
int mutt_mb_width(const char *str, int col, bool display)
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
      if ((wc == L'\t') || (nl && (wc == L' ')))
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
 * mutt_mb_wcwidth - Measure the screen width of a character
 * @param wc Character to examine
 * @retval num Width in screen columns
 */
int mutt_mb_wcwidth(wchar_t wc)
{
  int n = wcwidth(wc);
  if (IsWPrint(wc) && (n > 0))
    return n;
  if (!(wc & ~0x7f))
    return 2;
  if (!(wc & ~0xffff))
    return 6;
  return 10;
}

/**
 * mutt_mb_wcswidth - Measure the screen width of a string
 * @param s String to measure
 * @param n Length of string in characters
 * @retval num Width in screen columns
 */
int mutt_mb_wcswidth(const wchar_t *s, size_t n)
{
  if (!s)
    return 0;

  int w = 0;
  while (n--)
    w += mutt_mb_wcwidth(*s++);
  return w;
}

/**
 * mutt_mb_width_ceiling - Keep the end of the string on-screen
 * @param s String being displayed
 * @param n Length of string in characters
 * @param w1 Width limit
 * @retval num Chars to skip
 *
 * Given a string and a width, determine how many characters from the
 * beginning of the string should be skipped so that the string fits.
 */
size_t mutt_mb_width_ceiling(const wchar_t *s, size_t n, int w1)
{
  if (!s)
    return 0;

  const wchar_t *s0 = s;
  int w = 0;
  for (; n; s++, n--)
    if ((w += mutt_mb_wcwidth(*s)) > w1)
      break;
  return s - s0;
}

/**
 * mutt_mb_wcstombs - Convert a string from wide to multibyte characters
 * @param dest Buffer for the result
 * @param dlen Length of the result buffer
 * @param src Source string to convert
 * @param slen Length of the source string
 */
void mutt_mb_wcstombs(char *dest, size_t dlen, const wchar_t *src, size_t slen)
{
  if (!dest || !src)
    return;

  mbstate_t st;
  size_t k;

  /* First convert directly into the destination buffer */
  memset(&st, 0, sizeof(st));
  for (; slen && dlen >= MB_LEN_MAX; dest += k, dlen -= k, src++, slen--)
  {
    k = wcrtomb(dest, *src, &st);
    if (k == (size_t)(-1))
      break;
  }

  /* If this works, we can stop now */
  if (dlen >= MB_LEN_MAX)
  {
    dest += wcrtomb(dest, 0, &st);
    return;
  }

  /* Otherwise convert any remaining data into a local buffer */
  {
    char buf[3 * MB_LEN_MAX];
    char *p = buf;

    for (; slen && p - buf < dlen; p += k, src++, slen--)
    {
      k = wcrtomb(p, *src, &st);
      if (k == (size_t)(-1))
        break;
    }
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
 * mutt_mb_mbstowcs - Convert a string from multibyte to wide characters
 * @param[out] pwbuf    Buffer for the result
 * @param[out] pwbuflen Length of the result buffer
 * @param[in]  i        Starting index into the result buffer
 * @param[in]  buf      String to convert
 * @retval num First character after the result
 */
size_t mutt_mb_mbstowcs(wchar_t **pwbuf, size_t *pwbuflen, size_t i, char *buf)
{
  if (!pwbuf || !pwbuflen || !buf)
    return 0;

  wchar_t wc;
  mbstate_t st;
  size_t k;
  wchar_t *wbuf = *pwbuf;
  size_t wbuflen = *pwbuflen;

  while (*buf != '\0')
  {
    memset(&st, 0, sizeof(st));
    for (; (k = mbrtowc(&wc, buf, MB_LEN_MAX, &st)) && k != (size_t)(-1) &&
           k != (size_t)(-2);
         buf += k)
    {
      if (i >= wbuflen)
      {
        wbuflen = i + 20;
        mutt_mem_realloc(&wbuf, wbuflen * sizeof(*wbuf));
      }
      wbuf[i++] = wc;
    }
    if ((*buf != '\0') && ((k == (size_t) -1) || (k == (size_t) -2)))
    {
      if (i >= wbuflen)
      {
        wbuflen = i + 20;
        mutt_mem_realloc(&wbuf, wbuflen * sizeof(*wbuf));
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
 * mutt_mb_is_shell_char - Is character not typically part of a pathname
 * @param ch Character to examine
 * @retval true  Character is not typically part of a pathname
 * @retval false Character is typically part of a pathname
 *
 * @note The name is very confusing.
 */
bool mutt_mb_is_shell_char(wchar_t ch)
{
  static const wchar_t shell_chars[] = L"<>&()$?*;{}| "; /* ! not included because it can be part of a pathname in NeoMutt */
  return wcschr(shell_chars, ch);
}

/**
 * mutt_mb_is_lower - Does a multi-byte string contain only lowercase characters?
 * @param s String to check
 * @retval true  String contains no uppercase characters
 * @retval false Error, or contains some uppercase characters
 *
 * Non-alphabetic characters are considered lowercase.
 */
bool mutt_mb_is_lower(const char *s)
{
  if (!s)
    return false;

  wchar_t w;
  mbstate_t mb;
  size_t l;

  memset(&mb, 0, sizeof(mb));

  for (; (l = mbrtowc(&w, s, MB_CUR_MAX, &mb)) != 0; s += l)
  {
    if (l == (size_t) -2)
      continue; /* shift sequences */
    if (l == (size_t) -1)
      return false;
    if (iswalpha((wint_t) w) && iswupper((wint_t) w))
      return false;
  }

  return true;
}

/**
 * mutt_mb_is_display_corrupting_utf8 - Will this character corrupt the display?
 * @param wc Character to examine
 * @retval true  Character would corrupt the display
 * @retval false Character is safe to display
 *
 * @note This list isn't complete.
 */
bool mutt_mb_is_display_corrupting_utf8(wchar_t wc)
{
  if ((wc == (wchar_t) 0x00ad) || /* soft hyphen */
      (wc == (wchar_t) 0x200e) || /* left-to-right mark */
      (wc == (wchar_t) 0x200f) || /* right-to-left mark */
      (wc == (wchar_t) 0xfeff))   /* zero width no-break space */
  {
    return true;
  }

  /* left-to-right isolate, right-to-left isolate, first strong isolate,
   * pop directional isolate */
  if ((wc >= (wchar_t) 0x2066) && (wc <= (wchar_t) 0x2069))
    return true;

  /* left-to-right embedding, right-to-left embedding, pop directional formatting,
   * left-to-right override, right-to-left override */
  if ((wc >= (wchar_t) 0x202a) && (wc <= (wchar_t) 0x202e))
    return true;

  return false;
}

/**
 * mutt_mb_filter_unprintable - Replace unprintable characters
 * @param[in,out] s String to modify
 * @retval  0 Success
 * @retval -1 Error
 *
 * Unprintable characters will be replaced with #ReplacementChar.
 *
 * @note The source string will be freed and a newly allocated string will be
 * returned in its place.  The caller should free the returned string.
 */
int mutt_mb_filter_unprintable(char **s)
{
  if (!s || !*s)
    return -1;

  wchar_t wc;
  size_t k, k2;
  char scratch[MB_LEN_MAX + 1];
  char *p = *s;
  mbstate_t mbstate1, mbstate2;

  struct Buffer buf = mutt_buffer_make(0);
  memset(&mbstate1, 0, sizeof(mbstate1));
  memset(&mbstate2, 0, sizeof(mbstate2));
  for (; (k = mbrtowc(&wc, p, MB_LEN_MAX, &mbstate1)); p += k)
  {
    if ((k == (size_t) -1) || (k == (size_t) -2))
    {
      k = 1;
      memset(&mbstate1, 0, sizeof(mbstate1));
      wc = ReplacementChar;
    }
    if (!IsWPrint(wc))
      wc = '?';
    else if (CharsetIsUtf8 && mutt_mb_is_display_corrupting_utf8(wc))
      continue;
    k2 = wcrtomb(scratch, wc, &mbstate2);
    scratch[k2] = '\0';
    mutt_buffer_addstr(&buf, scratch);
  }
  FREE(s);
  *s = buf.data ? buf.data : mutt_mem_calloc(1, 1);
  return 0;
}
