/**
 * @file
 * RFC2047 MIME extensions encoding / decoding routines
 *
 * @authors
 * Copyright (C) 1996-2000,2010 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2000-2002 Edmund Grimley Evans <edmundo@rano.org>
 * Copyright (C) 2018 Pietro Cerutti <gahr@gahr.ch>
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
 * @page rfc2047 RFC2047 encoding / decoding functions
 *
 * RFC2047 MIME extensions encoding / decoding routines.
 *
 * | Function                      | Description
 * | :---------------------------- | :-----------------------------------------
 * | mutt_rfc2047_choose_charset() | Figure the best charset to encode a string
 * | mutt_rfc2047_decode()         | Decode any RFC2047-encoded header fields
 * | mutt_rfc2047_encode()         | RFC-2047-encode a string
 */

#include "config.h"
#include <assert.h>
#include <errno.h>
#include <string.h>
#include "rfc2047.h"
#include "base64.h"
#include "buffer.h"
#include "charset.h"
#include "mbyte.h"
#include "memory.h"
#include "mime.h"
#include "regex3.h"
#include "string2.h"

#define ENCWORD_LEN_MAX 75
#define ENCWORD_LEN_MIN 9 /* strlen ("=?.?.?.?=") */

#define HSPACE(x) (((x) == '\0') || ((x) == ' ') || ((x) == '\t'))

#define CONTINUATION_BYTE(c) (((c) &0xc0) == 0x80)

typedef size_t (*encoder_t)(char *s, const char *d, size_t dlen, const char *tocode);

/**
 * b_encoder - Base64 Encode a string
 * @param s      String to encode
 * @param d      Buffer for result
 * @param dlen   Length of buffer
 * @param tocode Character encoding
 * @retval num Number of bytes written to buffer
 */
static size_t b_encoder(char *s, const char *d, size_t dlen, const char *tocode)
{
  char *s0 = s;

  memcpy(s, "=?", 2);
  s += 2;
  memcpy(s, tocode, strlen(tocode));
  s += strlen(tocode);
  memcpy(s, "?B?", 3);
  s += 3;

  while (dlen)
  {
    char encoded[11];
    size_t ret;
    size_t in_len = MIN(3, dlen);

    ret = mutt_b64_encode(encoded, d, in_len, sizeof(encoded));
    for (size_t i = 0; i < ret; i++)
      *s++ = encoded[i];

    dlen -= in_len;
    d += in_len;
  }

  memcpy(s, "?=", 2);
  s += 2;
  return s - s0;
}

/**
 * q_encoder - Quoted-printable Encode a string
 * @param s      String to encode
 * @param d      Buffer for result
 * @param dlen   Length of buffer
 * @param tocode Character encoding
 * @retval num Number of bytes written to buffer
 */
static size_t q_encoder(char *s, const char *d, size_t dlen, const char *tocode)
{
  static const char hex[] = "0123456789ABCDEF";
  char *s0 = s;

  memcpy(s, "=?", 2);
  s += 2;
  memcpy(s, tocode, strlen(tocode));
  s += strlen(tocode);
  memcpy(s, "?Q?", 3);
  s += 3;
  while (dlen--)
  {
    unsigned char c = *d++;
    if (c == ' ')
      *s++ = '_';
    else if (c >= 0x7f || c < 0x20 || c == '_' || strchr(MimeSpecials, c))
    {
      *s++ = '=';
      *s++ = hex[(c & 0xf0) >> 4];
      *s++ = hex[c & 0x0f];
    }
    else
      *s++ = c;
  }
  memcpy(s, "?=", 2);
  s += 2;
  return s - s0;
}

/**
 * parse_encoded_word - Parse a string and report RFC2047 elements
 * @param[in]  s          String to parse
 * @param[out] enc        Content encoding found in the first RFC2047 word
 * @param[out] charset    Charset found in the first RFC2047 word
 * @param[out] charsetlen Lenght of the charset string found
 * @param[out] text       Start of the first RFC2047 encoded text
 * @param[out] textlen    Length of the encoded text found
 * @retval ptr Start of the RFC2047 encoded word
 * @retval NULL None was found
 */
static char *parse_encoded_word(char *s, enum ContentEncoding *enc, char **charset,
                                size_t *charsetlen, char **text, size_t *textlen)
{
  static struct Regex *re = NULL;
  regmatch_t match[4];
  size_t nmatch = 4;

  if (re == NULL)
  {
    re = mutt_regex_compile("=\\?"
                            "([^][()<>@,;:\\\"/?. =]+)" /* charset */
                            "\\?"
                            "([qQbB])" /* encoding */
                            "\\?"
                            "([^? ]+)" /* encoded text */
                            "\\?=",
                            REG_EXTENDED);
    assert(re && "Something is wrong with your RE engine.");
  }

  int rc = regexec(re->regex, s, nmatch, match, 0);
  if (rc != 0)
  {
    return NULL;
  }

  /* Charset */
  *charset = s + match[1].rm_so;
  *charsetlen = match[1].rm_eo - match[1].rm_so;

  /* Encoding: either Q or B */
  *enc = (s[match[2].rm_so] == 'Q' || s[match[2].rm_so] == 'q') ? ENCQUOTEDPRINTABLE : ENCBASE64;

  *text = s + match[3].rm_so;
  *textlen = match[3].rm_eo - match[3].rm_so;
  return s + match[0].rm_so;
}

/**
 * try_block - Attempt to convert a block of text
 * @param d        String to convert
 * @param dlen     Length of string
 * @param fromcode Original encoding
 * @param tocode   New encoding
 * @param encoder  Encoding function
 * @param wlen     Number of characters converted
 * @retval  0 Success, string converted
 * @retval >0 Error, number of bytes that could be converted
 *
 * If the data could be conveted using encoder, then set *encoder and *wlen.
 * Otherwise return an upper bound on the maximum length of the data which
 * could be converted.
 *
 * The data is converted from fromcode (which must be stateless) to tocode,
 * unless fromcode is NULL, in which case the data is assumed to be already in
 * tocode, which should be 8-bit and stateless.
 */
static size_t try_block(const char *d, size_t dlen, const char *fromcode,
                        const char *tocode, encoder_t *encoder, size_t *wlen)
{
  char buf1[ENCWORD_LEN_MAX - ENCWORD_LEN_MIN + 1];
  iconv_t cd;
  const char *ib = NULL;
  char *ob = NULL;
  size_t ibl, obl;
  int count, len, len_b, len_q;

  if (fromcode)
  {
    cd = mutt_ch_iconv_open(tocode, fromcode, 0);
    assert(cd != (iconv_t)(-1));
    ib = d;
    ibl = dlen;
    ob = buf1;
    obl = sizeof(buf1) - strlen(tocode);
    if (iconv(cd, (ICONV_CONST char **) &ib, &ibl, &ob, &obl) == (size_t)(-1) ||
        iconv(cd, NULL, NULL, &ob, &obl) == (size_t)(-1))
    {
      assert(errno == E2BIG);
      iconv_close(cd);
      assert(ib > d);
      return (ib - d == dlen) ? dlen : ib - d + 1;
    }
    iconv_close(cd);
  }
  else
  {
    if (dlen > sizeof(buf1) - strlen(tocode))
      return sizeof(buf1) - strlen(tocode) + 1;
    memcpy(buf1, d, dlen);
    ob = buf1 + dlen;
  }

  count = 0;
  for (char *p = buf1; p < ob; p++)
  {
    unsigned char c = *p;
    assert(strchr(MimeSpecials, '?'));
    if (c >= 0x7f || c < 0x20 || *p == '_' || (c != ' ' && strchr(MimeSpecials, *p)))
      count++;
  }

  len = ENCWORD_LEN_MIN - 2 + strlen(tocode);
  len_b = len + (((ob - buf1) + 2) / 3) * 4;
  len_q = len + (ob - buf1) + 2 * count;

  /* Apparently RFC1468 says to use B encoding for iso-2022-jp. */
  if (mutt_str_strcasecmp(tocode, "ISO-2022-JP") == 0)
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

/**
 * encode_block - Encode a block of text using an encoder
 * @param s        String to convert
 * @param d        Buffer for result
 * @param dlen     Buffer length
 * @param fromcode Original encoding
 * @param tocode   New encoding
 * @param encoder  Encoding function
 * @retval num Length of the encoded word
 *
 * Encode the data (d, dlen) into s using the encoder.
 */
static size_t encode_block(char *s, char *d, size_t dlen, const char *fromcode,
                           const char *tocode, encoder_t encoder)
{
  char buf1[ENCWORD_LEN_MAX - ENCWORD_LEN_MIN + 1];
  iconv_t cd;
  const char *ib = NULL;
  char *ob = NULL;
  size_t ibl, obl, n1, n2;

  if (fromcode)
  {
    cd = mutt_ch_iconv_open(tocode, fromcode, 0);
    assert(cd != (iconv_t)(-1));
    ib = d;
    ibl = dlen;
    ob = buf1;
    obl = sizeof(buf1) - strlen(tocode);
    n1 = iconv(cd, (ICONV_CONST char **) &ib, &ibl, &ob, &obl);
    n2 = iconv(cd, NULL, NULL, &ob, &obl);
    assert(n1 != (size_t)(-1) && n2 != (size_t)(-1));
    iconv_close(cd);
    return (*encoder)(s, buf1, ob - buf1, tocode);
  }
  else
    return (*encoder)(s, d, dlen, tocode);
}

/**
 * choose_block - Calculate how much data can be converted
 * @param d        String to convert
 * @param dlen     Length of string
 * @param col      Starting column to convert
 * @param fromcode Original encoding
 * @param tocode   New encoding
 * @param encoder  Encoding function
 * @param wlen     Number of characters converted
 * @retval num Number of bytes that can be converted
 *
 * Discover how much of the data (d, dlen) can be converted into a single
 * encoded word. Return how much data can be converted, and set the length
 * *wlen of the encoded word and *encoder.  We start in column col, which
 * limits the length of the word.
 */
static size_t choose_block(char *d, size_t dlen, int col, const char *fromcode,
                           const char *tocode, encoder_t *encoder, size_t *wlen)
{
  size_t n, nn;
  int utf8 = fromcode && (mutt_str_strcasecmp(fromcode, "utf-8") == 0);

  n = dlen;
  while (true)
  {
    assert(d + n > d);
    nn = try_block(d, n, fromcode, tocode, encoder, wlen);
    if (!nn && (col + *wlen <= ENCWORD_LEN_MAX + 1 || n <= 1))
      break;
    n = (nn ? nn : n) - 1;
    assert(n > 0);
    if (utf8)
      while (n > 1 && CONTINUATION_BYTE(d[n]))
        n--;
  }
  return n;
}

/**
 * finalize_chunk - Perform charset conversion and filtering
 * @param[out] res        Buffer where the resulting string is appended
 * @param[in]  buf        Buffer with the input string
 * @param[in]  charset    Charset to use for the conversion
 * @param[in]  charsetlen Length of the charset parameter
 *
 * The buffer buf is reinitialized at the end of this function.
 */
static void finalize_chunk(struct Buffer *res, struct Buffer *buf, char *charset, size_t charsetlen)
{
  char end = charset[charsetlen];
  charset[charsetlen] = '\0';
  mutt_ch_convert_string(&buf->data, charset, Charset, MUTT_ICONV_HOOK_FROM);
  charset[charsetlen] = end;
  mutt_mb_filter_unprintable(&buf->data);
  mutt_buffer_addstr(res, buf->data);
  FREE(&buf->data);
  mutt_buffer_init(buf);
}

/**
 * rfc2047_decode_word - Decode an RFC2047-encoded string
 * @param s   String to decode
 * @param len Lenfth of the string
 * @param enc Encoding type
 * @retval ptr Decoded string
 *
 * @note The caller must free the returned string
 */
static char *rfc2047_decode_word(const char *s, size_t len, enum ContentEncoding enc)
{
  struct Buffer buf = { 0 };
  const char *it = s;
  const char *end = s + len;

  if (enc == ENCQUOTEDPRINTABLE)
  {
    for (; it < end; ++it)
    {
      if (*it == '_')
      {
        mutt_buffer_addch(&buf, ' ');
      }
      else if (*it == '=' && (!(it[1] & ~127) && hexval(it[1]) != -1) &&
               (!(it[2] & ~127) && hexval(it[2]) != -1))
      {
        mutt_buffer_addch(&buf, (hexval(it[1]) << 4) | hexval(it[2]));
        it += 2;
      }
      else
      {
        mutt_buffer_addch(&buf, *it);
      }
    }
  }
  else if (enc == ENCBASE64)
  {
    int c, b = 0, k = 0;

    for (; it < end; ++it)
    {
      if (*it == '=')
        break;
      if ((*it & ~127) || (c = base64val(*it)) == -1)
        continue;
      if (k + 6 >= 8)
      {
        k -= 2;
        mutt_buffer_addch(&buf, b | (c >> k));
        b = c << (8 - k);
      }
      else
      {
        b |= c << (k + 2);
        k += 6;
      }
    }
  }

  mutt_buffer_addch(&buf, '\0');
  return buf.data;
}

/**
 * rfc2047_encode - RFC2047-encode a string
 * @param d        String to convert
 * @param dlen     Length of string
 * @param col      Starting column to convert
 * @param fromcode Original encoding
 * @param charsets List of allowable encodings (colon separated)
 * @param e        Encoded string
 * @param elen     Length of encoded string
 * @param specials Special characters to be encoded
 * @retval 0 Success
 */
static int rfc2047_encode(const char *d, size_t dlen, int col, const char *fromcode,
                          const char *charsets, char **e, size_t *elen, const char *specials)
{
  int rc = 0;
  char *buf = NULL;
  size_t bufpos, buflen;
  char *u = NULL, *t0 = NULL, *t1 = NULL, *t = NULL;
  char *s0 = NULL, *s1 = NULL;
  size_t ulen, r, n, wlen;
  encoder_t encoder;
  char *tocode1 = NULL;
  const char *tocode = NULL;
  char *icode = "utf-8";

  /* Try to convert to UTF-8. */
  u = mutt_str_substr_dup(d, d + dlen);
  if (mutt_ch_convert_string(&u, fromcode, icode, 0))
  {
    rc = 1;
    icode = 0;
  }
  ulen = mutt_str_strlen(u);

  /* Find earliest and latest things we must encode. */
  s0 = s1 = t0 = t1 = 0;
  for (t = u; t < u + ulen; t++)
  {
    if ((*t & 0x80) || (*t == '=' && t[1] == '?' && (t == u || HSPACE(*(t - 1)))))
    {
      if (!t0)
        t0 = t;
      t1 = t;
    }
    else if (specials && *t && strchr(specials, *t))
    {
      if (!s0)
        s0 = t;
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
    return rc;
  }

  /* Choose target charset. */
  tocode = fromcode;
  if (icode)
  {
    tocode1 = mutt_rfc2047_choose_charset(icode, charsets, u, ulen, 0, 0);
    if (tocode1)
      tocode = tocode1;
    else
    {
      rc = 2;
      icode = 0;
    }
  }

  /* Hack to avoid labelling 8-bit data as us-ascii. */
  if (!icode && mutt_ch_is_us_ascii(tocode))
    tocode = "unknown-8bit";

  /* Adjust t0 for maximum length of line. */
  t = u + (ENCWORD_LEN_MAX + 1) - col - ENCWORD_LEN_MIN;
  if (t < u)
    t = u;
  if (t < t0)
    t0 = t;

  /* Adjust t0 until we can encode a character after a space. */
  for (; t0 > u; t0--)
  {
    if (!HSPACE(*(t0 - 1)))
      continue;
    t = t0 + 1;
    if (icode)
      while (t < u + ulen && CONTINUATION_BYTE(*t))
        t++;
    if (!try_block(t0, t - t0, icode, tocode, &encoder, &wlen) &&
        col + (t0 - u) + wlen <= ENCWORD_LEN_MAX + 1)
    {
      break;
    }
  }

  /* Adjust t1 until we can encode a character before a space. */
  for (; t1 < u + ulen; t1++)
  {
    if (!HSPACE(*t1))
      continue;
    t = t1 - 1;
    if (icode)
      while (CONTINUATION_BYTE(*t))
        t--;
    if (!try_block(t, t1 - t, icode, tocode, &encoder, &wlen) &&
        1 + wlen + (u + ulen - t1) <= ENCWORD_LEN_MAX + 1)
    {
      break;
    }
  }

  /* We shall encode the region [t0,t1). */

  /* Initialise the output buffer with the us-ascii prefix. */
  buflen = 2 * ulen;
  buf = mutt_mem_malloc(buflen);
  bufpos = t0 - u;
  memcpy(buf, u, t0 - u);

  col += t0 - u;

  t = t0;
  while (true)
  {
    /* Find how much we can encode. */
    n = choose_block(t, t1 - t, col, icode, tocode, &encoder, &wlen);
    if (n == t1 - t)
    {
      /* See if we can fit the us-ascii suffix, too. */
      if (col + wlen + (u + ulen - t1) <= ENCWORD_LEN_MAX + 1)
        break;
      n = t1 - t - 1;
      if (icode)
        while (CONTINUATION_BYTE(t[n]))
          n--;
      assert(t + n >= t);
      if (!n)
      {
        /* This should only happen in the really stupid case where the
           only word that needs encoding is one character long, but
           there is too much us-ascii stuff after it to use a single
           encoded word. We add the next word to the encoded region
           and try again. */
        assert(t1 < u + ulen);
        for (t1++; t1 < u + ulen && !HSPACE(*t1); t1++)
          ;
        continue;
      }
      n = choose_block(t, n, col, icode, tocode, &encoder, &wlen);
    }

    /* Add to output buffer. */
    const char *line_break = "\n\t";
    const int lb_len = 2; /* strlen(line_break) */

    if ((bufpos + wlen + lb_len) > buflen)
    {
      buflen = bufpos + wlen + lb_len;
      mutt_mem_realloc(&buf, buflen);
    }
    r = encode_block(buf + bufpos, t, n, icode, tocode, encoder);
    assert(r == wlen);
    bufpos += wlen;
    memcpy(buf + bufpos, line_break, lb_len);
    bufpos += lb_len;

    col = 1;

    t += n;
  }

  /* Add last encoded word and us-ascii suffix to buffer. */
  buflen = bufpos + wlen + (u + ulen - t1);
  mutt_mem_realloc(&buf, buflen + 1);
  r = encode_block(buf + bufpos, t, t1 - t, icode, tocode, encoder);
  assert(r == wlen);
  bufpos += wlen;
  memcpy(buf + bufpos, t1, u + ulen - t1);

  FREE(&tocode1);
  FREE(&u);

  buf[buflen] = '\0';

  *e = buf;
  *elen = buflen + 1;
  return rc;
}

/**
 * mutt_rfc2047_choose_charset - Figure the best charset to encode a string
 * @param[in] fromcode Original charset of the string
 * @param[in] charsets Colon-separated list of potential charsets to use
 * @param[in] u        String to encode
 * @param[in] ulen     Length of the string to encode
 * @param[out] d       If not NULL, point it to the converted string
 * @param[out] dlen    If not NULL, point it to the length of the d string
 * @retval ptr  Best performing charset
 * @retval NULL None could be found
 */
char *mutt_rfc2047_choose_charset(const char *fromcode, const char *charsets,
                                  char *u, size_t ulen, char **d, size_t *dlen)
{
  char canonical_buf[LONG_STRING];
  char *e = NULL, *tocode = NULL;
  size_t elen = 0, bestn = 0;
  const char *q = NULL;

  for (const char *p = charsets; p; p = q ? q + 1 : 0)
  {
    char *s = NULL, *t = NULL;
    size_t slen, n;

    q = strchr(p, ':');

    n = q ? q - p : strlen(p);
    if (!n)
      continue;

    t = mutt_mem_malloc(n + 1);
    memcpy(t, p, n);
    t[n] = '\0';

    s = mutt_str_substr_dup(u, u + ulen);
    if (mutt_ch_convert_string(&s, fromcode, t, 0))
    {
      FREE(&t);
      FREE(&s);
      continue;
    }
    slen = mutt_str_strlen(s);

    if (!tocode || n < bestn)
    {
      bestn = n;
      FREE(&tocode);
      tocode = t;
      if (d)
      {
        FREE(&e);
        e = s;
      }
      else
        FREE(&s);
      elen = slen;
    }
    else
    {
      FREE(&t);
      FREE(&s);
    }
  }
  if (tocode)
  {
    if (d)
      *d = e;
    if (dlen)
      *dlen = elen;

    mutt_ch_canonical_charset(canonical_buf, sizeof(canonical_buf), tocode);
    mutt_str_replace(&tocode, canonical_buf);
  }
  return tocode;
}

/**
 * mutt_rfc2047_encode - RFC-2047-encode a string
 * @param[in,out] pd       String to be encoded, and resulting encoded string
 * @param[in]     specials Special characters to be encoded
 * @param[in]     col      Starting index in string
 * @param[in]     charsets List of charsets to choose from
 */
void mutt_rfc2047_encode(char **pd, const char *specials, int col, const char *charsets)
{
  char *e = NULL;
  size_t elen;

  if (!Charset || !*pd)
    return;

  if (!charsets || !*charsets)
    charsets = "utf-8";

  rfc2047_encode(*pd, strlen(*pd), col, Charset, charsets, &e, &elen, specials);

  FREE(pd);
  *pd = e;
}

/**
 * mutt_rfc2047_decode - Decode any RFC2047-encoded header fields
 * @param[in,out] pd  String to be decoded, and resulting decoded string
 *
 * Try to decode anything that looks like a valid RFC2047 encoded header field,
 * ignoring RFC822 parsing rules
 */
void mutt_rfc2047_decode(char **pd)
{
  struct Buffer buf = { 0 }; /* Output buffer                          */
  char *s = *pd;             /* Read pointer                           */
  char *beg;                 /* Begin of encoded word                  */
  enum ContentEncoding enc;  /* ENCBASE64 or ENCQUOTEDPRINTABLE        */
  char *charset;             /* Which charset                          */
  size_t charsetlen;         /* Lenght of the charset                  */
  char *text;                /* Encoded text                           */
  size_t textlen;            /* Length of encoded text                 */

  /* Keep some state in case the next decoded word is using the same charset
   * and it happens to be split in the middle of a multibyte character. See
   * https://github.com/neomutt/neomutt/issues/1015
   */
  struct Buffer prev = { 0 }; /* Previously decoded word                */
  char *prev_charset = NULL;  /* Previously used charset                */
  size_t prev_charsetlen = 0; /* Length of the previously used charset  */

  while (*s)
  {
    beg = parse_encoded_word(s, &enc, &charset, &charsetlen, &text, &textlen);
    if (beg != s)
    {
      /* Some non-encoded text was found */
      size_t holelen = beg ? beg - s : mutt_str_strlen(s);

      /* Ignore whitespace between encoded words */
      if (beg && mutt_str_lws_len(s, holelen) == holelen)
      {
        s = beg;
        continue;
      }

      /* If we have some previously decoded text, add it now */
      if (prev.data)
      {
        finalize_chunk(&buf, &prev, prev_charset, prev_charsetlen);
      }

      /* Add non-encoded part */
      {
        if (AssumedCharset && *AssumedCharset)
        {
          char *conv = mutt_str_substr_dup(s, s + holelen);
          mutt_ch_convert_nonmime_string(&conv);
          mutt_buffer_addstr(&buf, conv);
          FREE(&conv);
        }
        else
        {
          mutt_buffer_add(&buf, s, holelen);
        }
      }
      s += holelen;
    }
    if (beg)
    {
      /* Some encoded text was found */
      char *decoded = rfc2047_decode_word(text, textlen, enc);
      if (prev.data && (prev_charsetlen != charsetlen ||
                        strncmp(prev_charset, charset, charsetlen) != 0))
      {
        /* Different charset, convert the previous chunk and add it to the
         * final result */
        finalize_chunk(&buf, &prev, prev_charset, prev_charsetlen);
      }

      mutt_buffer_addstr(&prev, decoded);
      prev_charset = charset;
      prev_charsetlen = charsetlen;
      s = text + textlen + 2; /* Skip final ?= */
    }
  }

  /* Save the last chunk */
  if (prev.data)
  {
    finalize_chunk(&buf, &prev, prev_charset, prev_charsetlen);
  }

  mutt_buffer_addch(&buf, '\0');
  *pd = buf.data;
  return;
}
