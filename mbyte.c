/*
 * Copyright (C) 2000 Edmund Grimley Evans <edmundo@rano.org>
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */

/*
 * Japanese support by TAKIZAWA Takashi.
 */

#include "mutt.h"
#include "mbyte.h"
#include "charset.h"

#include <errno.h>

#include <ctype.h>

#ifndef EILSEQ
#define EILSEQ EINVAL
#endif

int Charset_is_utf8 = 0;
#ifndef HAVE_WC_FUNCS
static int charset_is_ja = 0;
static iconv_t charset_to_utf8 = (iconv_t)(-1);
static iconv_t charset_from_utf8 = (iconv_t)(-1);
#endif

void mutt_set_charset (char *charset)
{
  char buffer[8];

  mutt_canonical_charset (buffer, sizeof (buffer), charset);

  Charset_is_utf8 = 0;
#ifndef HAVE_WC_FUNCS
  charset_is_ja = 0;
  if (charset_to_utf8 != (iconv_t)(-1))
  {
    iconv_close (charset_to_utf8);
    charset_to_utf8 = (iconv_t)(-1);
  }
  if (charset_from_utf8 != (iconv_t)(-1))
  {
    iconv_close (charset_from_utf8);
    charset_from_utf8 = (iconv_t)(-1);
  }
#endif

  if (!strcmp(buffer, "utf-8"))
    Charset_is_utf8 = 1;
#ifndef HAVE_WC_FUNCS
  else if (!strcmp(buffer, "euc-jp") || !strcmp(buffer, "shift_jis"))
  {
    charset_is_ja = 1;
    charset_to_utf8 = iconv_open ("UTF-8", charset);
    charset_from_utf8 = iconv_open (charset, "UTF-8");
  }
#endif
}

#ifndef HAVE_WC_FUNCS

/*
 * For systems that don't have them, we provide here our own
 * implementations of wcrtomb(), mbrtowc(), iswprint() and wcwidth().
 * Instead of using the locale, as these functions normally would,
 * we use Mutt's Charset variable. We support 3 types of charset:
 * (1) For 8-bit charsets, wchar_t uses the same encoding as char.
 * (2) For UTF-8, wchar_t uses UCS.
 * (3) For stateless Japanese encodings, we use UCS and convert
 *     via UTF-8 using iconv.
 * Unfortunately, we can't handle non-stateless encodings.
 */

static size_t wcrtomb_iconv (char *s, wchar_t wc, iconv_t cd)
{
  char buf[MB_LEN_MAX];
  const char *ib;
  char *ob;
  size_t ibl, obl, r;

  if (s)
  {
    ibl = mutt_wctoutf8 (buf, wc);
    if (ibl == (size_t)(-1))
      return (size_t)(-1);
    ib = buf;
    ob = s;
    obl = MB_LEN_MAX;
    r = iconv (cd, &ib, &ibl, &ob, &obl);
  }
  else
  {
    ib = "";
    ibl = 1;
    ob = buf;
    obl = sizeof (buf);
    r = iconv (cd, &ib, &ibl, &ob, &obl);
  }
  return ob - s;
}

size_t wcrtomb (char *s, wchar_t wc, mbstate_t *ps)
{
  /* We only handle stateless encodings, so we can ignore ps. */

  if (Charset_is_utf8)
    return mutt_wctoutf8 (s, wc);
  else if (charset_from_utf8 != (iconv_t)(-1))
    return wcrtomb_iconv (s, wc, charset_from_utf8);
  else
  {
    if (!s)
      return 1;
    if (wc < 0x100)
    {
      *s = wc;
      return 1;
    }
    errno = EILSEQ;
    return (size_t)(-1);
  }
}

size_t mbrtowc_iconv (wchar_t *pwc, const char *s, size_t n,
		      mbstate_t *ps, iconv_t cd)
{
  static mbstate_t mbstate;
  const char *ib, *ibmax;
  char *ob, *t;
  size_t ibl, obl, k, r;
  char bufi[8], bufo[6];

  if (!n)
    return (size_t)(-2);

  t = memchr (ps, 0, sizeof (*ps));
  k = t ? (t - (char *)ps) : sizeof (*ps);
  if (k > sizeof (bufi))
    k = 0;
  if (k)
  {
    /* use the buffer for input */
    memcpy (bufi, ps, k);
    ib = bufi;
    ibmax = bufi + (k + n < sizeof (bufi) ? k + n : sizeof (bufi));
    memcpy (bufi + k, s, ibmax - bufi - k);
  }
  else
  {
    /* use the real input */
    ib = s;
    ibmax = s + n;
  }

  ob = bufo;
  obl = sizeof (bufo);
  ibl = 1;

  for (;;)
  {
    r = iconv (cd, &ib, &ibl, &ob, &obl);
    if (ob > bufo && (!k || ib > bufi + k))
    {
      /* we have a character */
      memset (ps, 0, sizeof (*ps));
      utf8rtowc (pwc, bufo, ob - bufo, &mbstate);
      return *pwc ? (ib - (k ? bufi + k : s)) : 0;
    }
    else if (!r || (r == (size_t)(-1) && errno == EINVAL))
    {
      if (ib + ibl < ibmax)
	/* try using more input */
	++ibl;
      else if (k && ib > bufi + k && bufi + k + n > ibmax)
      {
	/* switch to using real input */
	ib = s + (ib - bufi - k);
	ibmax = s + n;
	k = 0;
	++ibl;
      }
      else
      {
	/* save the state and give up */
	memset (ps, 0, sizeof (*ps));
	if (ibl <= sizeof (mbstate_t)) /* need extra condition here! */
	  memcpy (ps, ib, ibl);
	return (size_t)(-2);
      }
    }
    else
    {
      /* bad input */
      errno = EILSEQ;
      return (size_t)(-1);
    }
  }
}

size_t mbrtowc (wchar_t *pwc, const char *s, size_t n, mbstate_t *ps)
{
  static mbstate_t mbstate;

  if (!ps)
    ps = &mbstate;

  if (Charset_is_utf8)
    return utf8rtowc (pwc, s, n, ps);
  else if (charset_to_utf8 != (iconv_t)(-1))
    return mbrtowc_iconv (pwc, s, n, ps, charset_to_utf8);
  else
  {
    if (!s)
    {
      memset(ps, 0, sizeof(*ps));
      return 0;
    }
    if (!n)
      return (size_t)-2;
    if (pwc)
      *pwc = (wchar_t)(unsigned char)*s;
    return (*s != 0);
  }
}

int iswprint (wint_t wc)
{
  if (Charset_is_utf8 || charset_is_ja)
    return ((0x20 <= wc && wc < 0x7f) || 0xa0 <= wc);
  else
    return (0 <= wc && wc < 256) ? isprint (wc) : 0;
}

/*
 * l10n for Japanese:
 *   Symbols, Greek and Cyrillic in JIS X 0208, Japanese Kanji
 *   Character Set, have a column width of 2.
 */
int wcwidth_ja (wchar_t ucs)
{
  if (ucs >= 2e80)
    return -1; /* continue with the normal check */
  /* a rough range for quick check */
  if ((ucs >= 0x00a1 && ucs <= 0x00fe) || /* Latin-1 Supplement */
      (ucs >= 0x0391 && ucs <= 0x0451) || /* Greek and Cyrillic */
      (ucs >= 0x2010 && ucs <= 0x266f))   /* Symbols */
    return 2;
  else
    return -1;
}

int wcwidth_ucs(wchar_t ucs);

int wcwidth (wchar_t wc)
{
  if (!Charset_is_utf8)
  {
    if (!charset_is_ja)
    {
      /* 8-bit case */
      if (0 <= wc && wc < 256)
	return isprint (wc) ? 1 : -1;
      else
	return -1;
    }
    else
    {
      /* Japanese */
      int k = wcwidth_ja (wc);
      if (k != -1)
	return k;
    }
  }
  return wcwidth_ucs (wc);
}

size_t utf8rtowc (wchar_t *pwc, const char *s, size_t n, mbstate_t *_ps)
{
  static wchar_t mbstate;
  wchar_t *ps = (wchar_t *)_ps;
  size_t k = 1;
  unsigned char c;
  wchar_t wc;
  int count;

  if (!ps)
    ps = &mbstate;

  if (!s)
  {
    *ps = 0;
    return 0;
  }
  if (!n)
    return (size_t)-2;

  if (!*ps)
  {
    c = (unsigned char)*s;
    if (c < 0x80)
    {
      if (pwc)
	*pwc = c;
      return (c != 0);
    }
    else if (c < 0xc2)
    {
      errno = EILSEQ;
      return (size_t)-1;
    }
    else if (c < 0xe0)
      wc = ((c & 0x1f) << 6) + (count = 0);
    else if (c < 0xf0)
      wc = ((c & 0x0f) << 12) + (count = 1);
    else if (c < 0xf8)
      wc = ((c & 0x07) << 18) + (count = 2);
    else if (c < 0xfc)
      wc = ((c & 0x03) << 24) + (count = 3);
    else if (c < 0xfe)
      wc = ((c & 0x01) << 30) + (count = 4);
    else
    {
      errno = EILSEQ;
      return (size_t)-1;
    }
    ++s, --n, ++k;
  }
  else
  {
    wc = *ps & 0x7fffffff;
    count = wc & 7; /* if count > 4 it will be caught below */
  }

  for (; n; ++s, --n, ++k)
  {
    c = (unsigned char)*s;
    if (0x80 <= c && c < 0xc0)
    {
      wc |= (c & 0x3f) << (6 * count);
      if (!count)
      {
	if (pwc)
	  *pwc = wc;
	*ps = 0;
	return wc ? k : 0;
      }
      --count, --wc;
      if (!(wc >> (11+count*5)))
      {
	errno = count < 4 ? EILSEQ : EINVAL;
	return (size_t)-1;
      }
    }
    else
    {
      errno = EILSEQ;
      return (size_t)-1;
    }
  }
  *ps = wc;
  return (size_t)-2;
}

#endif /* !HAVE_WC_FUNCS */

wchar_t replacement_char ()
{
  return Charset_is_utf8 ? 0xfffd : '?';
}
