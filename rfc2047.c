/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@cs.hmc.edu>
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

#include "mutt.h"
#include "mime.h"
#include "charset.h"
#include "rfc2047.h"

#include <ctype.h>
#include <errno.h>
#include <iconv.h>
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
#define ENCWORD_OVERHEAD 7 /* strlen ("=??X??=") */

#define HSPACE(x) ((x) == ' ' || (x) == '\t')

typedef size_t (*encoder_t) (char *, const char *, size_t,
			     const char *);

static size_t convert_string (const char *f, size_t flen,
			      const char *from, const char *to,
			      char **t, size_t *tlen)
{
  iconv_t cd;
  char *buf, *ob, *x;
  size_t obl, n;
  int e;

  cd = iconv_open (to, from);
  if (cd == (iconv_t)-1)
    return -1;
  obl = MB_LEN_MAX * flen;
  ob = buf = safe_malloc (obl);
  n = iconv (cd, &f, &flen, &ob, &obl);
  if (n == -1 || iconv (cd, 0, 0, &ob, &obl) == -1)
  {
    e = errno;
    free (buf);
    iconv_close (cd);
    errno = e;
    return -1;
  }
  x = realloc (buf, ob - buf);
  *t = x ? x : buf;
  *tlen = ob - buf;
  iconv_close (cd);
  return n;
}

static char *choose_charset (const char *charsets, char *u, size_t ulen)
{
  char *tocode = 0;
  size_t bestn = 0;
  const char *p, *q;

  for (p = charsets; p; p = q ? q + 1 : 0)
  {
    char *s, *t;
    size_t slen, n;

    q = strchr (p, ':');

    n = q ? q - p : strlen (p);

    if (!n ||
	n > (ENCWORD_LEN_MAX - ENCWORD_OVERHEAD - ((MB_LEN_MAX + 2) / 3) * 4))
      continue;

    t = safe_malloc (n + 1);
    memcpy (t, p, n), t[n] = '\0';
    n = convert_string (u, ulen, "UTF-8", t, &s, &slen);
    if (n == (size_t)(-1))
      continue;
    free (s);
    if (!tocode || n < bestn)
    {
      free (tocode), tocode = t, bestn = n;
      if (!bestn)
	break;
    }
    else
      free (t);
  }
  return tocode;
}

static size_t b_encoder (char *s, const char *d, size_t dlen,
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

static size_t q_encoder (char *s, const char *d, size_t dlen,
			 const char *tocode)
{
  char hex[] = "0123456789ABCDEF";
  char *s0 = s;

  memcpy (s, "=?", 2), s += 2;
  memcpy (s, tocode, strlen (tocode)), s += strlen (tocode);
  memcpy (s, "?Q?", 3), s += 3;
  while (dlen--)
  {
    unsigned char c = *d++;
    if (c >= 0x7f || c < 0x20 || c == '_' ||  strchr (MimeSpecials, c))
    {
      *s++ = '=';
      *s++ = hex[(c & 0xf0) >> 4];
      *s++ = hex[c & 0x0f];
    }
    else if (c == ' ')
      *s++ = '_';
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
 */
static size_t try_block (const char *d, size_t dlen, const char *tocode,
			 encoder_t *encoder, size_t *wlen)
{
  char buf1[ENCWORD_LEN_MAX - ENCWORD_OVERHEAD];
  iconv_t cd;
  const char *ib;
  char *ob, *p;
  size_t ibl, obl;
  int count, len, len_b, len_q;

  cd = iconv_open (tocode, "UTF-8");
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

  count = 0;
  for (p = buf1; p < ob; p++)
  {
    unsigned char c = *p;
    assert (strchr (MimeSpecials, '?'));
    if (c >= 0x7f || c < 0x20 || *p == '_' || strchr (MimeSpecials, *p))
      ++count;
  }

  len = strlen (tocode) + ENCWORD_OVERHEAD;
  len_b = len + (((ob - buf1) + 2) / 3) * 4;
  len_q = len + (ob - buf1) + 2 * count;

  /* Apparently RFC 1468 says to use B encoding for iso-2022-jp. */
  if (!strcasecmp (tocode, "ISO-2022-JP"))
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
			    const char *tocode, encoder_t encoder)
{
  char buf1[ENCWORD_LEN_MAX - ENCWORD_OVERHEAD];
  iconv_t cd;
  const char *ib;
  char *ob;
  size_t ibl, obl, n1, n2;

  cd = iconv_open (tocode, "UTF-8");
  assert (cd != (iconv_t)(-1));
  ib = d, ibl = dlen, ob = buf1, obl = sizeof (buf1) - strlen (tocode);
  n1 = iconv (cd, &ib, &ibl, &ob, &obl);
  n2 = iconv (cd, 0, 0, &ob, &obl);
  assert (n1 != (size_t)(-1) && n2 != (size_t)(-1));
  iconv_close (cd);
  return (*encoder) (s, buf1, ob - buf1, tocode);
}

/*
 * Discover how much of the data (d, dlen) can be converted into
 * a single encoded word. Return how much data can be converted,
 * and set the length *wlen of the encoded word and *encoder.
 */
static size_t choose_block (char *d, size_t dlen, const char *tocode,
			    size_t *wlen, encoder_t *encoder)
{
  size_t n, nn;

  n = dlen;
  for (;;)
  {
    assert (n);
    nn = try_block (d, n, tocode, encoder, wlen);
    if (!nn)
      break;
    for (n = nn - 1; (d[n] & 0xc0) == 0x80; n--)
      assert (n);
  }
  return n;
}

/*
 * Place the result of RFC-2048-encoding (d, dlen) into the dynamically
 * allocated buffer (e, elen). The input data is in charset fromcode
 * and is converted into a charset chosen from charsets.
 * Return 1 if the input data is invalid, 2 if no conversion is possible,
 * otherwise 0 on success.
 */
static int rfc2047_encode (const char *d, size_t dlen,
			   const char *fromcode, const char *charsets,
			   char **e, size_t *elen)
{
  char *buf = 0;
  size_t bufpos = 0, buflen = 0;
  char *u0, *u;
  size_t ulen;
  char *tocode;
  int prev;

  /* Convert to UTF-8. */
  if (convert_string (d, dlen, fromcode, "UTF-8", &u, &ulen))
    return 1;
  u0 = u;

  /* Choose target charset. */
  tocode = choose_charset (charsets, u, ulen);
  if (!tocode)
  {
    free (u);
    return 2;
  }

  for (prev = 0; ulen; prev = 1) {
    char *t;
    size_t n, wlen, r;
    encoder_t encoder;

    /* Decide where to start encoding. */
    if (prev && ulen && !HSPACE (*u))
      t = u;
    else
    {
      /* Look for a non-us-ascii chararcter or "=?". */
      for (t = u; t < u + ulen - 1; t++)
	if ((*t & 0x80) || (*t == '=' && t[1] == '?'))
	  break;
      if (t == u + ulen - 1 && !(*t & 0x80))
	break;

      /* Find start of that word. */
      while (t > u && !HSPACE(*(t-1)))
	--t;
      if (prev) {
	/* Include preceding characters if they are all spaces. */
	const char *x;
	for (x = u; x < t && HSPACE(*x); x++)
	  ;
	if (x >= t)
	  t = u;
      }
    }

    /* Convert some data and add encoded word to buffer. */
    n = choose_block (t, (u + ulen) - t, tocode, &wlen, &encoder);
    buflen = bufpos + (t == u && prev) + (t - u) + wlen;
    safe_realloc ((void **) &buf, buflen);
    if (t == u && prev)
      buf[bufpos++] = ' ';
    memcpy (buf + bufpos, u, t - u);
    bufpos += t - u;
    r = encode_block (buf + bufpos, t, n, tocode, encoder);
    assert (r == wlen);
    bufpos += wlen;
    n += t - u;
    u += n;
    ulen -= n;
  }

  /* Add remaining us-ascii characters to buffer. */
  buflen = bufpos + ulen;
  safe_realloc ((void **) &buf, buflen);
  memcpy (buf + bufpos, u, ulen);

  free (tocode);
  free (u0);
  *e = buf;
  *elen = buflen;
  return 0;
}

#define MAX (ENCWORD_LEN_MAX + 1)

static char *rfc2047_fold_line (char *e, size_t elen)
{
  char *line, *p, *f;
  int col = MAX;

  p = line = safe_malloc (elen * 2); /* more than enough */

  while (elen)
  {
    if (elen > 2 && e[1] == '=' && e[2] == '?' && HSPACE(*e))
    {
    again:
      if (col + elen > MAX)
      {
	if (col >= MAX)
	  f = e;
	else
	  for (f = e + MAX - col; !HSPACE(*f); f--)
	    ;
	if (e == f)
	{
	  if (col)
	  {
	    *p++ = '\n', col = 0;
	    goto again;
	  }
	  for (f = e + MAX; f < e + elen && !HSPACE(*f); f++)
	    ;
	}
	memcpy (p, e, f - e), p += f - e;
	elen -= f - e, e = f;
	if (elen)
	  *p++ = '\n', col = 0;
	continue;
      }
    }
    *p++ = *e++, elen--, col++;
  }
  *p++ = '\0';
  safe_realloc ((void **) &line, p - line);
  return line;
}

void rfc2047_encode_string (char **pd)
{
  char *e;
  size_t elen;
  char *charsets;

  charsets = SendCharset;
  if (!charsets || !*charsets)
    charsets = Charset;
  if (!charsets || !*charsets)
    charsets = "UTF-8";

  if (!rfc2047_encode (*pd, strlen (*pd), Charset, charsets, &e, &elen))
  {
    free (*pd);
    *pd = rfc2047_fold_line (e, elen);
    free (e);
  }
}

void rfc2047_encode_adrlist (ADDRESS *addr)
{
  ADDRESS *ptr = addr;

  while (ptr)
  {
    if (ptr->personal)
      rfc2047_encode_string (&ptr->personal);
#ifdef EXACT_ADDRESS
    if (ptr->val)
      rfc2047_encode_string (&ptr->val);
#endif
    ptr = ptr->next;
  }
}

static int rfc2047_decode_word (char *d, const char *s, size_t len)
{
  const char *pp = s, *pp1;
  char *pd, *d0;
  const char *t, *t1;
  int enc = 0, count = 0, c1, c2, c3, c4;
  char *charset = NULL;

  pd = d0 = safe_malloc (strlen (s));

  for (pp = s; (pp1 = strchr (pp, '?')); pp = pp1 + 1)
  {
    count++;
    switch (count)
    {
      case 2:
	/* ignore language specification a la RFC 2231 */        
	t = pp1;
        if ((t1 = memchr (pp, '*', t - pp)))
	  t = t1;
	charset = safe_malloc (t - pp + 1);
	memcpy (charset, pp, t - pp);
	charset[t-pp] = '\0';
	break;
      case 3:
	if (toupper (*pp) == 'Q')
	  enc = ENCQUOTEDPRINTABLE;
	else if (toupper (*pp) == 'B')
	  enc = ENCBASE64;
	else
	{
	  safe_free ((void **) &charset);
	  safe_free ((void **) &d0);
	  return (-1);
	}
	break;
      case 4:
	if (enc == ENCQUOTEDPRINTABLE)
	{
	  while (pp < pp1 && len > 0)
	  {
	    if (*pp == '_')
	    {
	      *pd++ = ' ';
	      len--;
	    }
	    else if (*pp == '=')
	    {
	      if (pp[1] == 0 || pp[2] == 0)
		break;	/* something wrong */
	      *pd++ = (hexval(pp[1]) << 4) | hexval(pp[2]);
	      len--;
	      pp += 2;
	    }
	    else
	    {
	      *pd++ = *pp;
	      len--;
	    }
	    pp++;
	  }
	  *pd = 0;
	}
	else if (enc == ENCBASE64)
	{
	  while (pp < pp1 && len > 0)
	  {
	    if (pp[0] == '=' || pp[1] == 0 || pp[1] == '=')
	      break;  /* something wrong */
	    c1 = base64val(pp[0]);
	    c2 = base64val(pp[1]);
	    *pd++ = (c1 << 2) | ((c2 >> 4) & 0x3);
	    if (--len == 0) break;
	    
	    if (pp[2] == 0 || pp[2] == '=') break;

	    c3 = base64val(pp[2]);
	    *pd++ = ((c2 & 0xf) << 4) | ((c3 >> 2) & 0xf);
	    if (--len == 0)
	      break;

	    if (pp[3] == 0 || pp[3] == '=')
	      break;

	    c4 = base64val(pp[3]);
	    *pd++ = ((c3 & 0x3) << 6) | c4;
	    if (--len == 0)
	      break;

	    pp += 4;
	  }
	  *pd = 0;
	}
	break;
    }
  }
  
  if (charset)
    mutt_convert_string (&d0, charset, Charset);
  strfcpy (d, d0, len);
  safe_free ((void **) &charset);
  safe_free ((void **) &d0);
  return (0);
}

/* try to decode anything that looks like a valid RFC2047 encoded
 * header field, ignoring RFC822 parsing rules
 */
void rfc2047_decode (char **pd)
{
  const char *p, *q;
  size_t n;
  int found_encoded = 0;
  char *d0, *d;
  const char *s = *pd;
  size_t dlen;

  if (!*s)
    return;

  dlen = MB_LEN_MAX * strlen (s); /* should be enough */
  d = d0 = safe_malloc (dlen + 1);

  while (*s && dlen > 0)
  {
    if ((p = strstr (s, "=?")) == NULL ||
	(q = strchr (p + 2, '?')) == NULL ||
	(q = strchr (q + 1, '?')) == NULL ||
	(q = strstr (q + 1, "?=")) == NULL)
    {
      /* no encoded words */
      strncpy (d, s, dlen);
      d += dlen;
      break;
    }

    if (p != s)
    {
      n = (size_t) (p - s);
      /* ignore spaces between encoded words */
      if (!found_encoded || strspn (s, " \t\r\n") != n)
      {
	if (n > dlen)
	  n = dlen;
	if (d != s)
	  memcpy (d, s, n);
	d += n;
	dlen -= n;
      }
    }

    rfc2047_decode_word (d, p, dlen);
    found_encoded = 1;
    s = q + 2;
    n = mutt_strlen (d);
    dlen -= n;
    d += n;
  }
  *d = 0;

  safe_free ((void **) pd);
  *pd = d0;
  mutt_str_adjust (pd);
}

void rfc2047_decode_adrlist (ADDRESS *a)
{
  while (a)
  {
    if (a->personal && strstr (a->personal, "=?") != NULL)
      rfc2047_decode (&a->personal);
#ifdef EXACT_ADDRESS
    if (a->val && strstr (a->val, "=?") != NULL)
      rfc2047_decode (&a->val);
#endif
    a = a->next;
  }
}
