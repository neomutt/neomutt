/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2000-2002 Edmund Grimley Evans <edmundo@rano.org>
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
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "mime.h"
#include "charset.h"
#include "rfc2047.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* If you are debugging this file, comment out the following line. */
/*#define NDEBUG*/

#ifdef NDEBUG
#define assert(x)
#else
#include <assert.h>
#endif

#define ENCWORD_LEN_MAX 75
#define ENCWORD_LEN_MIN 9 /* strlen ("=?.?.?.?=") */

#define HSPACE(x) ((x) == '\0' || (x) == ' ' || (x) == '\t')

#define CONTINUATION_BYTE(c) (((c) & 0xc0) == 0x80)

extern char RFC822Specials[];

typedef size_t (*encoder_t) (char *, ICONV_CONST char *, size_t,
			     const char *);

static size_t convert_string (ICONV_CONST char *f, size_t flen,
			      const char *from, const char *to,
			      char **t, size_t *tlen)
{
  iconv_t cd;
  char *buf, *ob;
  size_t obl, n;
  int e;

  cd = mutt_iconv_open (to, from, 0);
  if (cd == (iconv_t)(-1))
    return (size_t)(-1);
  obl = 4 * flen + 1;
  ob = buf = safe_malloc (obl);
  n = iconv (cd, &f, &flen, &ob, &obl);
  if (n == (size_t)(-1) || iconv (cd, 0, 0, &ob, &obl) == (size_t)(-1))
  {
    e = errno;
    FREE (&buf);
    iconv_close (cd);
    errno = e;
    return (size_t)(-1);
  }
  *ob = '\0';
  
  *tlen = ob - buf;

  safe_realloc (&buf, ob - buf + 1);
  *t = buf;
  iconv_close (cd);

  return n;
}

int convert_nonmime_string (char **ps)
{
  const char *c, *c1;

  for (c = AssumedCharset; c; c = c1 ? c1 + 1 : 0)
  {
    char *u = *ps;
    char *s;
    char *fromcode;
    size_t m, n;
    size_t ulen = mutt_strlen (*ps);
    size_t slen;

    if (!u || !*u)
      return 0;

    c1 = strchr (c, ':');
    n = c1 ? c1 - c : mutt_strlen (c);
    if (!n)
      return 0;
    fromcode = safe_malloc (n + 1);
    strfcpy (fromcode, c, n + 1);
    m = convert_string (u, ulen, fromcode, Charset, &s, &slen);
    FREE (&fromcode);
    if (m != (size_t)(-1))
    {
      FREE (ps); /* __FREE_CHECKED__ */
      *ps = s;
      return 0;
    }
  }
  mutt_convert_string (ps,
      (const char *)mutt_get_default_charset (),
      Charset, M_ICONV_HOOK_FROM);
  return -1;
}

char *mutt_choose_charset (const char *fromcode, const char *charsets,
		      char *u, size_t ulen, char **d, size_t *dlen)
{
  char canonical_buff[LONG_STRING];
  char *e = 0, *tocode = 0;
  size_t elen = 0, bestn = 0;
  const char *p, *q;

  for (p = charsets; p; p = q ? q + 1 : 0)
  {
    char *s, *t;
    size_t slen, n;

    q = strchr (p, ':');

    n = q ? q - p : strlen (p);

    if (!n ||
	/* Assume that we never need more than 12 characters of
	   encoded-text to encode a single character. */
	n > (ENCWORD_LEN_MAX - ENCWORD_LEN_MIN + 2 - 12))
      continue;

    t = safe_malloc (n + 1);
    memcpy (t, p, n);
    t[n] = '\0';

    n = convert_string (u, ulen, fromcode, t, &s, &slen);
    if (n == (size_t)(-1))
      continue;

    if (!tocode || n < bestn)
    {
      bestn = n;
      FREE (&tocode);
      tocode = t;
      if (d)
      {
	FREE (&e);
	e = s;
      }
      else
	FREE (&s);
      elen = slen;
      if (!bestn)
	break;
    }
    else
    {
      FREE (&t);
      FREE (&s);
    }
  }
  if (tocode)
  {
    if (d)
      *d = e;
    if (dlen)
      *dlen = elen;
    
    mutt_canonical_charset (canonical_buff, sizeof (canonical_buff), tocode);
    mutt_str_replace (&tocode, canonical_buff);
  }
  return tocode;
}

static size_t b_encoder (char *s, ICONV_CONST char *d, size_t dlen,
			 const char *tocode)
{
  char *s0 = s;

  memcpy (s, "=?", 2), s += 2;
  memcpy (s, tocode, strlen (tocode)), s += strlen (tocode);
  memcpy (s, "?B?", 3), s += 3;
  for (;;)
  {
    if (!dlen)
      break;
    else if (dlen == 1)
    {
      *s++ = B64Chars[(*d >> 2) & 0x3f];
      *s++ = B64Chars[(*d & 0x03) << 4];
      *s++ = '=';
      *s++ = '=';
      break;
    }
    else if (dlen == 2)
    {
      *s++ = B64Chars[(*d >> 2) & 0x3f];
      *s++ = B64Chars[((*d & 0x03) << 4) | ((d[1] >> 4) & 0x0f)];
      *s++ = B64Chars[(d[1] & 0x0f) << 2];
      *s++ = '=';
      break;
    }
    else
    {
      *s++ = B64Chars[(*d >> 2) & 0x3f];
      *s++ = B64Chars[((*d & 0x03) << 4) | ((d[1] >> 4) & 0x0f)];
      *s++ = B64Chars[((d[1] & 0x0f) << 2) | ((d[2] >> 6) & 0x03)];
      *s++ = B64Chars[d[2] & 0x3f];
      d += 3, dlen -= 3;
    }
  }
  memcpy (s, "?=", 2), s += 2;
  return s - s0;
}

static size_t q_encoder (char *s, ICONV_CONST char *d, size_t dlen,
			 const char *tocode)
{
  static const char hex[] = "0123456789ABCDEF";
  char *s0 = s;

  memcpy (s, "=?", 2), s += 2;
  memcpy (s, tocode, strlen (tocode)), s += strlen (tocode);
  memcpy (s, "?Q?", 3), s += 3;
  while (dlen--)
  {
    unsigned char c = *d++;
    if (c == ' ')
      *s++ = '_';
    else if (c >= 0x7f || c < 0x20 || c == '_' ||  strchr (MimeSpecials, c))
    {
      *s++ = '=';
      *s++ = hex[(c & 0xf0) >> 4];
      *s++ = hex[c & 0x0f];
    }
    else
      *s++ = c;
  }
  memcpy (s, "?=", 2), s += 2;
  return s - s0;
}

/*
 * Return 0 if and set *encoder and *wlen if the data (d, dlen) could
 * be converted to an encoded word of length *wlen using *encoder.
 * Otherwise return an upper bound on the maximum length of the data
 * which could be converted.
 * The data is converted from fromcode (which must be stateless) to
 * tocode, unless fromcode is 0, in which case the data is assumed to
 * be already in tocode, which should be 8-bit and stateless.
 */
static size_t try_block (ICONV_CONST char *d, size_t dlen,
			 const char *fromcode, const char *tocode,
			 encoder_t *encoder, size_t *wlen)
{
  char buf1[ENCWORD_LEN_MAX - ENCWORD_LEN_MIN + 1];
  iconv_t cd;
  ICONV_CONST char *ib;
  char *ob, *p;
  size_t ibl, obl;
  int count, len, len_b, len_q;

  if (fromcode)
  {
    cd = mutt_iconv_open (tocode, fromcode, 0);
    assert (cd != (iconv_t)(-1));
    ib = d, ibl = dlen, ob = buf1, obl = sizeof (buf1) - strlen (tocode);
    if (iconv (cd, &ib, &ibl, &ob, &obl) == (size_t)(-1) ||
	iconv (cd, 0, 0, &ob, &obl) == (size_t)(-1))
    {
      assert (errno == E2BIG);
      iconv_close (cd);
      assert (ib > d);
      return (ib - d == dlen) ? dlen : ib - d + 1;
    }
    iconv_close (cd);
  }
  else
  {
    if (dlen > sizeof (buf1) - strlen (tocode))
      return sizeof (buf1) - strlen (tocode) + 1;
    memcpy (buf1, d, dlen);
    ob = buf1 + dlen;
  }

  count = 0;
  for (p = buf1; p < ob; p++)
  {
    unsigned char c = *p;
    assert (strchr (MimeSpecials, '?'));
    if (c >= 0x7f || c < 0x20 || *p == '_' ||
	(c != ' ' && strchr (MimeSpecials, *p)))
      ++count;
  }

  len = ENCWORD_LEN_MIN - 2 + strlen (tocode);
  len_b = len + (((ob - buf1) + 2) / 3) * 4;
  len_q = len + (ob - buf1) + 2 * count;

  /* Apparently RFC 1468 says to use B encoding for iso-2022-jp. */
  if (!ascii_strcasecmp (tocode, "ISO-2022-JP"))
    len_q = ENCWORD_LEN_MAX + 1;

  if (len_b < len_q && len_b <= ENCWORD_LEN_MAX)
  {
    *encoder = b_encoder;
    *wlen = len_b;
    return 0;
  }
  else if (len_q <= ENCWORD_LEN_MAX)
  {
    *encoder = q_encoder;
    *wlen = len_q;
    return 0;
  }
  else
    return dlen;
}

/*
 * Encode the data (d, dlen) into s using the encoder.
 * Return the length of the encoded word.
 */
static size_t encode_block (char *s, char *d, size_t dlen,
			    const char *fromcode, const char *tocode,
			    encoder_t encoder)
{
  char buf1[ENCWORD_LEN_MAX - ENCWORD_LEN_MIN + 1];
  iconv_t cd;
  ICONV_CONST char *ib;
  char *ob;
  size_t ibl, obl, n1, n2;

  if (fromcode)
  {
    cd = mutt_iconv_open (tocode, fromcode, 0);
    assert (cd != (iconv_t)(-1));
    ib = d, ibl = dlen, ob = buf1, obl = sizeof (buf1) - strlen (tocode);
    n1 = iconv (cd, &ib, &ibl, &ob, &obl);
    n2 = iconv (cd, 0, 0, &ob, &obl);
    assert (n1 != (size_t)(-1) && n2 != (size_t)(-1));
    iconv_close (cd);
    return (*encoder) (s, buf1, ob - buf1, tocode);
  }
  else
    return (*encoder) (s, d, dlen, tocode);
}

/*
 * Discover how much of the data (d, dlen) can be converted into
 * a single encoded word. Return how much data can be converted,
 * and set the length *wlen of the encoded word and *encoder.
 * We start in column col, which limits the length of the word.
 */
static size_t choose_block (char *d, size_t dlen, int col,
			    const char *fromcode, const char *tocode,
			    encoder_t *encoder, size_t *wlen)
{
  size_t n, nn;
  int utf8 = fromcode && !ascii_strcasecmp (fromcode, "utf-8");

  n = dlen;
  for (;;)
  {
    assert (d + n > d);
    nn = try_block (d, n, fromcode, tocode, encoder, wlen);
    if (!nn && (col + *wlen <= ENCWORD_LEN_MAX + 1 || n <= 1))
      break;
    n = (nn ? nn : n) - 1;
    assert (n > 0);
    if (utf8)
      while (n > 1 && CONTINUATION_BYTE(d[n]))
	--n;
  }
  return n;
}

/*
 * Place the result of RFC-2047-encoding (d, dlen) into the dynamically
 * allocated buffer (e, elen). The input data is in charset fromcode
 * and is converted into a charset chosen from charsets.
 * Return 1 if the conversion to UTF-8 failed, 2 if conversion from UTF-8
 * failed, otherwise 0. If conversion failed, fromcode is assumed to be
 * compatible with us-ascii and the original data is used.
 * The input data is assumed to be a single line starting at column col;
 * if col is non-zero, the preceding character was a space.
 */
static int rfc2047_encode (ICONV_CONST char *d, size_t dlen, int col,
			   const char *fromcode, const char *charsets,
			   char **e, size_t *elen, char *specials)
{
  int ret = 0;
  char *buf;
  size_t bufpos, buflen;
  char *u, *t0, *t1, *t;
  char *s0, *s1;
  size_t ulen, r, n, wlen;
  encoder_t encoder;
  char *tocode1 = 0;
  const char *tocode;
  char *icode = "utf-8";

  /* Try to convert to UTF-8. */
  if (convert_string (d, dlen, fromcode, icode, &u, &ulen))
  {
    ret = 1; 
    icode = 0;
    u = safe_malloc ((ulen = dlen) + 1);
    memcpy (u, d, dlen);
    u[ulen] = 0;
  }

  /* Find earliest and latest things we must encode. */
  s0 = s1 = t0 = t1 = 0;
  for (t = u; t < u + ulen; t++)
  {
    if ((*t & 0x80) || 
	(*t == '=' && t[1] == '?' && (t == u || HSPACE(*(t-1)))))
    {
      if (!t0) t0 = t;
      t1 = t;
    }
    else if (specials && strchr (specials, *t))
    {
      if (!s0) s0 = t;
      s1 = t;
    }
  }

  /* If we have something to encode, include RFC822 specials */
  if (t0 && s0 && s0 < t0)
    t0 = s0;
  if (t1 && s1 && s1 > t1)
    t1 = s1;

  if (!t0)
  {
    /* No encoding is required. */
    *e = u;
    *elen = ulen;
    return ret;
  }

  /* Choose target charset. */
  tocode = fromcode;
  if (icode)
  {
    if ((tocode1 = mutt_choose_charset (icode, charsets, u, ulen, 0, 0)))
      tocode = tocode1;
    else
      ret = 2, icode = 0;
  }

  /* Hack to avoid labelling 8-bit data as us-ascii. */
  if (!icode && mutt_is_us_ascii (tocode))
    tocode = "unknown-8bit";
  
  /* Adjust t0 for maximum length of line. */
  t = u + (ENCWORD_LEN_MAX + 1) - col - ENCWORD_LEN_MIN;
  if (t < u)  t = u;
  if (t < t0) t0 = t;
  

  /* Adjust t0 until we can encode a character after a space. */
  for (; t0 > u; t0--)
  {
    if (!HSPACE(*(t0-1)))
      continue;
    t = t0 + 1;
    if (icode)
      while (t < u + ulen && CONTINUATION_BYTE(*t))
	++t;
    if (!try_block (t0, t - t0, icode, tocode, &encoder, &wlen) &&
	col + (t0 - u) + wlen <= ENCWORD_LEN_MAX + 1)
      break;
  }

  /* Adjust t1 until we can encode a character before a space. */
  for (; t1 < u + ulen; t1++)
  {
    if (!HSPACE(*t1))
      continue;
    t = t1 - 1;
    if (icode)
      while (CONTINUATION_BYTE(*t))
	--t;
    if (!try_block (t, t1 - t, icode, tocode, &encoder, &wlen) &&
	1 + wlen + (u + ulen - t1) <= ENCWORD_LEN_MAX + 1)
      break;
  }

  /* We shall encode the region [t0,t1). */

  /* Initialise the output buffer with the us-ascii prefix. */
  buflen = 2 * ulen;
  buf = safe_malloc (buflen);
  bufpos = t0 - u;
  memcpy (buf, u, t0 - u);

  col += t0 - u;

  t = t0;
  for (;;)
  {
    /* Find how much we can encode. */
    n = choose_block (t, t1 - t, col, icode, tocode, &encoder, &wlen);
    if (n == t1 - t)
    {
      /* See if we can fit the us-ascii suffix, too. */
      if (col + wlen + (u + ulen - t1) <= ENCWORD_LEN_MAX + 1)
	break;
      n = t1 - t - 1;
      if (icode)
	while (CONTINUATION_BYTE(t[n]))
	  --n;
      assert (t + n >= t);
      if (!n)
      {
	/* This should only happen in the really stupid case where the
	   only word that needs encoding is one character long, but
	   there is too much us-ascii stuff after it to use a single
	   encoded word. We add the next word to the encoded region
	   and try again. */
	assert (t1 < u + ulen);
	for (t1++; t1 < u + ulen && !HSPACE(*t1); t1++)
	  ;
	continue;
      }
      n = choose_block (t, n, col, icode, tocode, &encoder, &wlen);
    }

    /* Add to output buffer. */
#define LINEBREAK "\n\t"
    if (bufpos + wlen + strlen (LINEBREAK) > buflen)
    {
      buflen = bufpos + wlen + strlen (LINEBREAK);
      safe_realloc (&buf, buflen);
    }
    r = encode_block (buf + bufpos, t, n, icode, tocode, encoder);
    assert (r == wlen);
    bufpos += wlen;
    memcpy (buf + bufpos, LINEBREAK, strlen (LINEBREAK));
    bufpos += strlen (LINEBREAK);
#undef LINEBREAK

    col = 1;

    t += n;
  }

  /* Add last encoded word and us-ascii suffix to buffer. */
  buflen = bufpos + wlen + (u + ulen - t1);
  safe_realloc (&buf, buflen + 1);
  r = encode_block (buf + bufpos, t, t1 - t, icode, tocode, encoder);
  assert (r == wlen);
  bufpos += wlen;
  memcpy (buf + bufpos, t1, u + ulen - t1);

  FREE (&tocode1);
  FREE (&u);

  buf[buflen] = '\0';
  
  *e = buf;
  *elen = buflen + 1;
  return ret;
}

void _rfc2047_encode_string (char **pd, int encode_specials, int col)
{
  char *e;
  size_t elen;
  char *charsets;

  if (!Charset || !*pd)
    return;

  charsets = SendCharset;
  if (!charsets || !*charsets)
    charsets = "utf-8";

  rfc2047_encode (*pd, strlen (*pd), col,
		  Charset, charsets, &e, &elen,
		  encode_specials ? RFC822Specials : NULL);

  FREE (pd);		/* __FREE_CHECKED__ */
  *pd = e;
}

void rfc2047_encode_adrlist (ADDRESS *addr, const char *tag)
{
  ADDRESS *ptr = addr;
  int col = tag ? strlen (tag) + 2 : 32;
  
  while (ptr)
  {
    if (ptr->personal)
      _rfc2047_encode_string (&ptr->personal, 1, col);
    else if (ptr->group && ptr->mailbox)
      _rfc2047_encode_string (&ptr->mailbox, 1, col);
#ifdef EXACT_ADDRESS
    if (ptr->val)
      _rfc2047_encode_string (&ptr->val, 1, col);
#endif
    ptr = ptr->next;
  }
}

static int rfc2047_decode_word (char *d, const char *s, size_t len)
{
  const char *pp, *pp1;
  char *pd, *d0;
  const char *t, *t1;
  int enc = 0, count = 0;
  char *charset = NULL;
  int rv = -1;

  pd = d0 = safe_malloc (strlen (s));

  for (pp = s; (pp1 = strchr (pp, '?')); pp = pp1 + 1)
  {
    count++;

    /* hack for non-compliant MUAs that allow unquoted question marks in encoded-text */
    if (count == 4)
    {
      while (pp1 && *(pp1 + 1) != '=')
	pp1 = strchr(pp1 + 1, '?');
      if (!pp1)
	  goto error_out_0;
    }

    switch (count)
    {
      case 2:
	/* ignore language specification a la RFC 2231 */        
	t = pp1;
        if ((t1 = memchr (pp, '*', t - pp)))
	  t = t1;
	charset = mutt_substrdup (pp, t);
	break;
      case 3:
	if (toupper ((unsigned char) *pp) == 'Q')
	  enc = ENCQUOTEDPRINTABLE;
	else if (toupper ((unsigned char) *pp) == 'B')
	  enc = ENCBASE64;
	else
	  goto error_out_0;
	break;
      case 4:
	if (enc == ENCQUOTEDPRINTABLE)
	{
	  for (; pp < pp1; pp++)
	  {
	    if (*pp == '_')
	      *pd++ = ' ';
	    else if (*pp == '=' &&
		     (!(pp[1] & ~127) && hexval(pp[1]) != -1) &&
		     (!(pp[2] & ~127) && hexval(pp[2]) != -1))
	    {
	      *pd++ = (hexval(pp[1]) << 4) | hexval(pp[2]);
	      pp += 2;
	    }
	    else
	      *pd++ = *pp;
	  }
	  *pd = 0;
	}
	else if (enc == ENCBASE64)
	{
	  int c, b = 0, k = 0;

	  for (; pp < pp1; pp++)
	  {
	    if (*pp == '=')
	      break;
	    if ((*pp & ~127) || (c = base64val(*pp)) == -1)
	      continue;
	    if (k + 6 >= 8)
	    {
	      k -= 2;
	      *pd++ = b | (c >> k);
	      b = c << (8 - k);
	    }
	    else
	    {
	      b |= c << (k + 2);
	      k += 6;
	    }
	  }
	  *pd = 0;
	}
	break;
    }
  }
  
  if (charset)
    mutt_convert_string (&d0, charset, Charset, M_ICONV_HOOK_FROM);
  mutt_filter_unprintable (&d0);
  strfcpy (d, d0, len);
  rv = 0;
error_out_0:
  FREE (&charset);
  FREE (&d0);
  return rv;
}

/*
 * Find the start and end of the first encoded word in the string.
 * We use the grammar in section 2 of RFC 2047, but the "encoding"
 * must be B or Q. Also, we don't require the encoded word to be
 * separated by linear-white-space (section 5(1)).
 */
static const char *find_encoded_word (const char *s, const char **x)
{
  const char *p, *q;

  q = s;
  while ((p = strstr (q, "=?")))
  {
    for (q = p + 2;
	 0x20 < *q && *q < 0x7f && !strchr ("()<>@,;:\"/[]?.=", *q);
	 q++)
      ;
    if (q[0] != '?' || !strchr ("BbQq", q[1]) || q[2] != '?')
      continue;
    /* non-strict check since many MUAs will not encode spaces and question marks */
    for (q = q + 3; 0x20 <= *q && *q < 0x7f && (*q != '?' || q[1] != '='); q++)
      ;
    if (q[0] != '?' || q[1] != '=')
    {
      --q;
      continue;
    }

    *x = q + 2;
    return p;
  }

  return 0;
}

/* return length of linear-white-space */
static size_t lwslen (const char *s, size_t n)
{
  const char *p = s;
  size_t len = n;

  if (n <= 0)
    return 0;

  for (; p < s + n; p++)
    if (!strchr (" \t\r\n", *p))
    {
      len = (size_t)(p - s);
      break;
    }
  if (strchr ("\r\n", *(p-1))) /* LWS doesn't end with CRLF */
    len = (size_t)0;
  return len;
}

/* return length of linear-white-space : reverse */
static size_t lwsrlen (const char *s, size_t n)
{
  const char *p = s + n - 1;
  size_t len = n;

  if (n <= 0)
    return 0;

  if (strchr ("\r\n", *p)) /* LWS doesn't end with CRLF */
    return (size_t)0;

  for (; p >= s; p--)
    if (!strchr (" \t\r\n", *p))
    {
      len = (size_t)(s + n - 1 - p);
      break;
    }
  return len;
}

/* try to decode anything that looks like a valid RFC2047 encoded
 * header field, ignoring RFC822 parsing rules
 */
void rfc2047_decode (char **pd)
{
  const char *p, *q;
  size_t m, n;
  int found_encoded = 0;
  char *d0, *d;
  const char *s = *pd;
  size_t dlen;

  if (!s || !*s)
    return;

  dlen = 4 * strlen (s); /* should be enough */
  d = d0 = safe_malloc (dlen + 1);

  while (*s && dlen > 0)
  {
    if (!(p = find_encoded_word (s, &q)))
    {
      /* no encoded words */
      if (option (OPTIGNORELWS))
      {
        n = mutt_strlen (s);
        if (found_encoded && (m = lwslen (s, n)) != 0)
        {
          if (m != n)
            *d = ' ', d++, dlen--;
          s += m;
        }
      }
      if (AssumedCharset && *AssumedCharset)
      {
	char *t;
	size_t tlen;

	n = mutt_strlen (s);
	t = safe_malloc (n + 1);
	strfcpy (t, s, n + 1);
	convert_nonmime_string (&t);
	tlen = mutt_strlen (t);
	strncpy (d, t, tlen);
	d += tlen;
	FREE (&t);
	break;
      }
      strncpy (d, s, dlen);
      d += dlen;
      break;
    }

    if (p != s)
    {
      n = (size_t) (p - s);
      /* ignore spaces between encoded word
       * and linear-white-space between encoded word and *text */
      if (option (OPTIGNORELWS))
      {
        if (found_encoded && (m = lwslen (s, n)) != 0)
        {
          if (m != n)
            *d = ' ', d++, dlen--;
          n -= m, s += m;
        }

        if ((m = n - lwsrlen (s, n)) != 0)
        {
          if (m > dlen)
            m = dlen;
          memcpy (d, s, m);
          d += m;
          dlen -= m;
          if (m != n)
            *d = ' ', d++, dlen--;
        }
      }
      else if (!found_encoded || strspn (s, " \t\r\n") != n)
      {
	if (n > dlen)
	  n = dlen;
	memcpy (d, s, n);
	d += n;
	dlen -= n;
      }
    }

    if (rfc2047_decode_word (d, p, dlen) == -1)
    {
      /* could not decode word, fall back to displaying the raw string */
      strfcpy(d, p, dlen);
    }
    found_encoded = 1;
    s = q;
    n = mutt_strlen (d);
    dlen -= n;
    d += n;
  }
  *d = 0;

  FREE (pd);		/* __FREE_CHECKED__ */
  *pd = d0;
  mutt_str_adjust (pd);
}

void rfc2047_decode_adrlist (ADDRESS *a)
{
  while (a)
  {
    if (a->personal && ((strstr (a->personal, "=?") != NULL) || 
			(AssumedCharset && *AssumedCharset)))
      rfc2047_decode (&a->personal);
    else if (a->group && a->mailbox && (strstr (a->mailbox, "=?") != NULL))
      rfc2047_decode (&a->mailbox);
#ifdef EXACT_ADDRESS
    if (a->val && strstr (a->val, "=?") != NULL)
      rfc2047_decode (&a->val);
#endif
    a = a->next;
  }
}
