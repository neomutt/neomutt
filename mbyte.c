
/*
 * This file provides functions that are just like the C library ones,
 * except that they behave according to mutt's Charset instead of
 * according to the locale.
 */

#include <errno.h>

#include "mutt.h"
#include "mbyte.h"
#include "charset.h"

#ifndef EILSEQ
#define EILSEQ EINVAL
#endif

int Charset_is_utf8 = 0;

void mutt_set_charset (char *charset)
{
  Charset_is_utf8 = mutt_is_utf8 (charset);
}

int wctomb (char *s, wchar_t wc)
{
  if (Charset_is_utf8)
    return mutt_wctoutf8(s, wc);
  else if (wc < 0x100)
  {
    if (s)
      *s = wc;
    return 1;
  }
  else
    return -1;
}

int mbtowc (wchar_t *pwc, const char *s, size_t n)
{
  mbstate_t state = 0;
  int result = mbrtowc (pwc, s, n, &state);
  if (result >= 0)
    return result;
  else
    return -1;
}

size_t utf8rtowc (wchar_t *pwc, const char *s, size_t n, mbstate_t *ps)
{
  static mbstate_t mbstate = 0;
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

size_t mbrtowc (wchar_t *pwc, const char *s, size_t n, mbstate_t *ps)
{
  static mbstate_t mbstate = 0;

  if (!ps)
    ps = &mbstate;

  if (Charset_is_utf8)
    return utf8rtowc (pwc, s, n, ps);
  else
  {
    if (!s)
    {
      *ps = 0;
      return 0;
    }
    if (!n)
      return (size_t)-2;
    if (pwc)
      *pwc = (wchar_t)(unsigned char)*s;
    return (*s != 0);
  }
}

int iswprint (wchar_t wc)
{
  return ((0x20 <= wc && wc < 0x7f) || 0xa0 <= wc);
}

wchar_t replacement_char ()
{
  return Charset_is_utf8 ? 0xfffd : '?';
}
