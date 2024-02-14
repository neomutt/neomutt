/**
 * @file
 * Simple string formatting
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
 * @page gui_format Simple string formatting
 *
 * Simple string formatting
 */

#include "config.h"
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "mutt/lib.h"
#include "format.h"
#include "mutt_thread.h"
#ifdef HAVE_ISWBLANK
#include <wctype.h>
#endif

/**
 * mutt_simple_format - Format a string, like snprintf()
 * @param[out] buf       Buffer in which to save string
 * @param[in]  buflen    Buffer length
 * @param[in]  min_width Minimum width
 * @param[in]  max_width Maximum width
 * @param[in]  justify   Justification, e.g. #JUSTIFY_RIGHT
 * @param[in]  pad_char  Padding character
 * @param[in]  s         String to format
 * @param[in]  n         Number of bytes of string to format
 * @param[in]  arboreal  If true, string contains graphical tree characters
 *
 * This formats a string, a bit like snprintf(buf, buflen, "%-*.*s",
 * min_width, max_width, s), except that the widths refer to the number of
 * character cells when printed.
 */
void mutt_simple_format(char *buf, size_t buflen, int min_width, int max_width,
                        enum FormatJustify justify, char pad_char,
                        const char *s, size_t n, bool arboreal)
{
  wchar_t wc = 0;
  int w;
  size_t k;
  char scratch[MB_LEN_MAX] = { 0 };
  mbstate_t mbstate1 = { 0 };
  mbstate_t mbstate2 = { 0 };
  bool escaped = false;

  buflen--;
  char *p = buf;
  for (; n && (k = mbrtowc(&wc, s, n, &mbstate1)); s += k, n -= k)
  {
    if ((k == ICONV_ILLEGAL_SEQ) || (k == ICONV_BUF_TOO_SMALL))
    {
      if ((k == ICONV_ILLEGAL_SEQ) && (errno == EILSEQ))
        memset(&mbstate1, 0, sizeof(mbstate1));

      k = (k == ICONV_ILLEGAL_SEQ) ? 1 : n;
      wc = ReplacementChar;
    }
    if (escaped)
    {
      escaped = false;
      w = 0;
    }
    else if (arboreal && (wc == MUTT_SPECIAL_INDEX))
    {
      escaped = true;
      w = 0;
    }
    else if (arboreal && (wc < MUTT_TREE_MAX))
    {
      w = 1; /* hack */
    }
    else
    {
#ifdef HAVE_ISWBLANK
      if (iswblank(wc))
        wc = ' ';
      else
#endif
          if (!IsWPrint(wc))
        wc = '?';
      w = wcwidth(wc);
    }
    if (w >= 0)
    {
      if (w > max_width)
        continue;
      size_t k2 = wcrtomb(scratch, wc, &mbstate2);
      if ((k2 == ICONV_ILLEGAL_SEQ) || (k2 > buflen))
        continue;

      min_width -= w;
      max_width -= w;
      strncpy(p, scratch, k2);
      p += k2;
      buflen -= k2;
    }
  }
  w = ((int) buflen < min_width) ? buflen : min_width;
  if (w <= 0)
  {
    *p = '\0';
  }
  else if (justify == JUSTIFY_RIGHT) /* right justify */
  {
    p[w] = '\0';
    while (--p >= buf)
      p[w] = *p;
    while (--w >= 0)
      buf[w] = pad_char;
  }
  else if (justify == JUSTIFY_CENTER) /* center */
  {
    char *savedp = p;
    int half = (w + 1) / 2; /* half of cushion space */

    p[w] = '\0';

    /* move str to center of buffer */
    while (--p >= buf)
      p[half] = *p;

    /* fill rhs */
    p = savedp + half;
    while (--w >= half)
      *p++ = pad_char;

    /* fill lhs */
    while (half--)
      buf[half] = pad_char;
  }
  else /* left justify */
  {
    while (--w >= 0)
      *p++ = pad_char;
    *p = '\0';
  }
}

/**
 * mutt_format - Format a string like snprintf()
 * @param[out] buf      Buffer in which to save string
 * @param[in]  buflen   Buffer length
 * @param[in]  prec     Field precision, e.g. "-3.4"
 * @param[in]  s        String to format
 * @param[in]  arboreal  If true, string contains graphical tree characters
 *
 * This formats a string rather like:
 * - snprintf(fmt, sizeof(fmt), "%%%ss", prec);
 * - snprintf(buf, buflen, fmt, s);
 * except that the numbers in the conversion specification refer to
 * the number of character cells when printed.
 */
void mutt_format(char *buf, size_t buflen, const char *prec, const char *s, bool arboreal)
{
  enum FormatJustify justify = JUSTIFY_RIGHT;
  char *p = NULL;
  int min_width;
  int max_width = INT_MAX;

  if (*prec == '-')
  {
    prec++;
    justify = JUSTIFY_LEFT;
  }
  else if (*prec == '=')
  {
    prec++;
    justify = JUSTIFY_CENTER;
  }
  min_width = strtol(prec, &p, 10);
  if (*p == '.')
  {
    prec = p + 1;
    max_width = strtol(prec, &p, 10);
    if (p <= prec)
      max_width = INT_MAX;
  }

  mutt_simple_format(buf, buflen, min_width, max_width, justify, ' ', s,
                     mutt_str_len(s), arboreal);
}
