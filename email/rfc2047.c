/**
 * @file
 * RFC2047 MIME extensions encoding / decoding routines
 *
 * @authors
 * Copyright (C) 1996-2000,2010 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2000-2002 Edmund Grimley Evans <edmundo@rano.org>
 * Copyright (C) 2018-2019 Pietro Cerutti <gahr@gahr.ch>
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
 * @page email_rfc2047 RFC2047 encoding / decoding functions
 *
 * RFC2047 MIME extensions encoding / decoding routines.
 */

#include "config.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <iconv.h>
#include <stdbool.h>
#include <string.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "rfc2047.h"
#include "envelope.h"
#include "globals.h"
#include "mime.h"
#include "mutt_globals.h"

#define ENCWORD_LEN_MAX 75
#define ENCWORD_LEN_MIN 9 /* strlen ("=?.?.?.?=") */

#define HSPACE(ch) (((ch) == '\0') || ((ch) == ' ') || ((ch) == '\t'))

#define CONTINUATION_BYTE(ch) (((ch) &0xc0) == 0x80)

/**
 * typedef encoder_t - Prototype for an encoding function
 * @param str    String to encode
 * @param buf    Buffer for result
 * @param buflen Length of buffer
 * @param tocode Character encoding
 * @retval num Bytes written to buffer
 */
typedef size_t (*encoder_t)(char *str, const char *buf, size_t buflen, const char *tocode);

/**
 * b_encoder - Base64 Encode a string - Implements ::encoder_t
 */
static size_t b_encoder(char *str, const char *buf, size_t buflen, const char *tocode)
{
  char *s0 = str;

  memcpy(str, "=?", 2);
  str += 2;
  memcpy(str, tocode, strlen(tocode));
  str += strlen(tocode);
  memcpy(str, "?B?", 3);
  str += 3;

  while (buflen)
  {
    char encoded[11];
    size_t ret;
    size_t in_len = MIN(3, buflen);

    ret = mutt_b64_encode(buf, in_len, encoded, sizeof(encoded));
    for (size_t i = 0; i < ret; i++)
      *str++ = encoded[i];

    buflen -= in_len;
    buf += in_len;
  }

  memcpy(str, "?=", 2);
  str += 2;
  return str - s0;
}

/**
 * q_encoder - Quoted-printable Encode a string - Implements ::encoder_t
 */
static size_t q_encoder(char *str, const char *buf, size_t buflen, const char *tocode)
{
  static const char hex[] = "0123456789ABCDEF";
  char *s0 = str;

  memcpy(str, "=?", 2);
  str += 2;
  memcpy(str, tocode, strlen(tocode));
  str += strlen(tocode);
  memcpy(str, "?Q?", 3);
  str += 3;
  while (buflen--)
  {
    unsigned char c = *buf++;
    if (c == ' ')
      *str++ = '_';
    else if ((c >= 0x7f) || (c < 0x20) || (c == '_') || strchr(MimeSpecials, c))
    {
      *str++ = '=';
      *str++ = hex[(c & 0xf0) >> 4];
      *str++ = hex[c & 0x0f];
    }
    else
      *str++ = c;
  }
  memcpy(str, "?=", 2);
  str += 2;
  return str - s0;
}

/**
 * parse_encoded_word - Parse a string and report RFC2047 elements
 * @param[in]  str        String to parse
 * @param[out] enc        Content encoding found in the first RFC2047 word
 * @param[out] charset    Charset found in the first RFC2047 word
 * @param[out] charsetlen Length of the charset string found
 * @param[out] text       Start of the first RFC2047 encoded text
 * @param[out] textlen    Length of the encoded text found
 * @retval ptr Start of the RFC2047 encoded word
 * @retval NULL None was found
 */
static char *parse_encoded_word(char *str, enum ContentEncoding *enc, char **charset,
                                size_t *charsetlen, char **text, size_t *textlen)
{
  regmatch_t *match = mutt_prex_capture(PREX_RFC2047_ENCODED_WORD, str);
  if (!match)
    return NULL;

  const regmatch_t *mfull = &match[PREX_RFC2047_ENCODED_WORD_MATCH_FULL];
  const regmatch_t *mcharset = &match[PREX_RFC2047_ENCODED_WORD_MATCH_CHARSET];
  const regmatch_t *mencoding = &match[PREX_RFC2047_ENCODED_WORD_MATCH_ENCODING];
  const regmatch_t *mtext = &match[PREX_RFC2047_ENCODED_WORD_MATCH_TEXT];

  /* Charset */
  *charset = str + mutt_regmatch_start(mcharset);
  *charsetlen = mutt_regmatch_len(mcharset);

  /* Encoding: either Q or B */
  *enc = (tolower(str[mutt_regmatch_start(mencoding)]) == 'q') ? ENC_QUOTED_PRINTABLE : ENC_BASE64;

  *text = str + mutt_regmatch_start(mtext);
  *textlen = mutt_regmatch_len(mtext);
  return str + mutt_regmatch_start(mfull);
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
 * If the data could be converted using encoder, then set *encoder and *wlen.
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
  char buf[ENCWORD_LEN_MAX - ENCWORD_LEN_MIN + 1];
  const char *ib = NULL;
  char *ob = NULL;
  size_t ibl, obl;
  int count, len, len_b, len_q;

  if (fromcode)
  {
    iconv_t cd = mutt_ch_iconv_open(tocode, fromcode, 0);
    assert(cd != (iconv_t)(-1));
    ib = d;
    ibl = dlen;
    ob = buf;
    obl = sizeof(buf) - strlen(tocode);
    if ((iconv(cd, (ICONV_CONST char **) &ib, &ibl, &ob, &obl) == (size_t)(-1)) ||
        (iconv(cd, NULL, NULL, &ob, &obl) == (size_t)(-1)))
    {
      assert(errno == E2BIG);
      iconv_close(cd);
      assert(ib > d);
      return ((ib - d) == dlen) ? dlen : ib - d + 1;
    }
    iconv_close(cd);
  }
  else
  {
    if (dlen > (sizeof(buf) - strlen(tocode)))
      return sizeof(buf) - strlen(tocode) + 1;
    memcpy(buf, d, dlen);
    ob = buf + dlen;
  }

  count = 0;
  for (char *p = buf; p < ob; p++)
  {
    unsigned char c = *p;
    assert(strchr(MimeSpecials, '?'));
    if ((c >= 0x7f) || (c < 0x20) || (*p == '_') ||
        ((c != ' ') && strchr(MimeSpecials, *p)))
    {
      count++;
    }
  }

  len = ENCWORD_LEN_MIN - 2 + strlen(tocode);
  len_b = len + (((ob - buf) + 2) / 3) * 4;
  len_q = len + (ob - buf) + 2 * count;

  /* Apparently RFC1468 says to use B encoding for iso-2022-jp. */
  if (mutt_istr_equal(tocode, "ISO-2022-JP"))
    len_q = ENCWORD_LEN_MAX + 1;

  if ((len_b < len_q) && (len_b <= ENCWORD_LEN_MAX))
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
 * @param str      String to convert
 * @param buf      Buffer for result
 * @param buflen   Buffer length
 * @param fromcode Original encoding
 * @param tocode   New encoding
 * @param encoder  Encoding function
 * @retval num Length of the encoded word
 *
 * Encode the data (buf, buflen) into str using the encoder.
 */
static size_t encode_block(char *str, char *buf, size_t buflen, const char *fromcode,
                           const char *tocode, encoder_t encoder)
{
  if (!fromcode)
  {
    return (*encoder)(str, buf, buflen, tocode);
  }

  const iconv_t cd = mutt_ch_iconv_open(tocode, fromcode, 0);
  assert(cd != (iconv_t)(-1));
  const char *ib = buf;
  size_t ibl = buflen;
  char tmp[ENCWORD_LEN_MAX - ENCWORD_LEN_MIN + 1];
  char *ob = tmp;
  size_t obl = sizeof(tmp) - strlen(tocode);
  const size_t n1 = iconv(cd, (ICONV_CONST char **) &ib, &ibl, &ob, &obl);
  const size_t n2 = iconv(cd, NULL, NULL, &ob, &obl);
  assert(n1 != (size_t)(-1) && n2 != (size_t)(-1));
  iconv_close(cd);
  return (*encoder)(str, tmp, ob - tmp, tocode);
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
 * @retval num Bytes that can be converted
 *
 * Discover how much of the data (d, dlen) can be converted into a single
 * encoded word. Return how much data can be converted, and set the length
 * *wlen of the encoded word and *encoder.  We start in column col, which
 * limits the length of the word.
 */
static size_t choose_block(char *d, size_t dlen, int col, const char *fromcode,
                           const char *tocode, encoder_t *encoder, size_t *wlen)
{
  const bool utf8 = fromcode && mutt_istr_equal(fromcode, "utf-8");

  size_t n = dlen;
  while (true)
  {
    assert(n > 0);
    const size_t nn = try_block(d, n, fromcode, tocode, encoder, wlen);
    if ((nn == 0) && (((col + *wlen) <= (ENCWORD_LEN_MAX + 1)) || (n <= 1)))
      break;
    n = ((nn != 0) ? nn : n) - 1;
    assert(n > 0);
    if (utf8)
      while ((n > 1) && CONTINUATION_BYTE(d[n]))
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
  if (!charset)
    return;
  char end = charset[charsetlen];
  charset[charsetlen] = '\0';
  mutt_ch_convert_string(&buf->data, charset, C_Charset, MUTT_ICONV_HOOK_FROM);
  charset[charsetlen] = end;
  mutt_mb_filter_unprintable(&buf->data);
  mutt_buffer_addstr(res, buf->data);
  FREE(&buf->data);
  mutt_buffer_init(buf);
}

/**
 * decode_word - Decode an RFC2047-encoded string
 * @param s   String to decode
 * @param len Length of the string
 * @param enc Encoding type
 * @retval ptr Decoded string
 *
 * @note The caller must free the returned string
 */
static char *decode_word(const char *s, size_t len, enum ContentEncoding enc)
{
  const char *it = s;
  const char *end = s + len;

  if (enc == ENC_QUOTED_PRINTABLE)
  {
    struct Buffer buf = mutt_buffer_make(0);
    for (; it < end; it++)
    {
      if (*it == '_')
      {
        mutt_buffer_addch(&buf, ' ');
      }
      else if ((it[0] == '=') && (!(it[1] & ~127) && (hexval(it[1]) != -1)) &&
               (!(it[2] & ~127) && (hexval(it[2]) != -1)))
      {
        mutt_buffer_addch(&buf, (hexval(it[1]) << 4) | hexval(it[2]));
        it += 2;
      }
      else
      {
        mutt_buffer_addch(&buf, *it);
      }
    }
    mutt_buffer_addch(&buf, '\0');
    return buf.data;
  }
  else if (enc == ENC_BASE64)
  {
    const int olen = 3 * len / 4 + 1;
    char *out = mutt_mem_malloc(olen);
    int dlen = mutt_b64_decode(it, out, olen);
    if (dlen == -1)
    {
      FREE(&out);
      return NULL;
    }
    out[dlen] = '\0';
    return out;
  }

  assert(0); /* The enc parameter has an invalid value */
  return NULL;
}

/**
 * encode - RFC2047-encode a string
 * @param[in]  d        String to convert
 * @param[in]  dlen     Length of string
 * @param[in]  col      Starting column to convert
 * @param[in]  fromcode Original encoding
 * @param[in]  charsets List of allowable encodings (colon separated)
 * @param[out] e        Encoded string
 * @param[out] elen     Length of encoded string
 * @param[in]  specials Special characters to be encoded
 * @retval 0 Success
 */
static int encode(const char *d, size_t dlen, int col, const char *fromcode,
                  const char *charsets, char **e, size_t *elen, const char *specials)
{
  int rc = 0;
  char *buf = NULL;
  size_t bufpos, buflen;
  char *t0 = NULL, *t1 = NULL, *t = NULL;
  char *s0 = NULL, *s1 = NULL;
  size_t ulen, r, wlen = 0;
  encoder_t encoder = NULL;
  char *tocode1 = NULL;
  const char *tocode = NULL;
  const char *icode = "utf-8";

  /* Try to convert to UTF-8. */
  char *u = mutt_strn_dup(d, dlen);
  if (mutt_ch_convert_string(&u, fromcode, icode, 0) != 0)
  {
    rc = 1;
    icode = 0;
  }
  ulen = mutt_str_len(u);

  /* Find earliest and latest things we must encode. */
  s0 = 0;
  s1 = 0;
  t0 = 0;
  t1 = 0;
  for (t = u; t < (u + ulen); t++)
  {
    if ((*t & 0x80) || ((*t == '=') && (t[1] == '?') && ((t == u) || HSPACE(*(t - 1)))))
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
  if (t0 && s0 && (s0 < t0))
    t0 = s0;
  if (t1 && s1 && (s1 > t1))
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
    tocode1 = mutt_ch_choose(icode, charsets, u, ulen, 0, 0);
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
      while ((t < (u + ulen)) && CONTINUATION_BYTE(*t))
        t++;
    if ((try_block(t0, t - t0, icode, tocode, &encoder, &wlen) == 0) &&
        ((col + (t0 - u) + wlen) <= (ENCWORD_LEN_MAX + 1)))
    {
      break;
    }
  }

  /* Adjust t1 until we can encode a character before a space. */
  for (; t1 < (u + ulen); t1++)
  {
    if (!HSPACE(*t1))
      continue;
    t = t1 - 1;
    if (icode)
      while (CONTINUATION_BYTE(*t))
        t--;
    if ((try_block(t, t1 - t, icode, tocode, &encoder, &wlen) == 0) &&
        ((1 + wlen + (u + ulen - t1)) <= (ENCWORD_LEN_MAX + 1)))
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
    size_t n = choose_block(t, t1 - t, col, icode, tocode, &encoder, &wlen);
    if (n == (t1 - t))
    {
      /* See if we can fit the us-ascii suffix, too. */
      if ((col + wlen + (u + ulen - t1)) <= (ENCWORD_LEN_MAX + 1))
        break;
      n = t1 - t - 1;
      if (icode)
        while (CONTINUATION_BYTE(t[n]))
          n--;
      if (n == 0)
      {
        /* This should only happen in the really stupid case where the
         * only word that needs encoding is one character long, but
         * there is too much us-ascii stuff after it to use a single
         * encoded word. We add the next word to the encoded region
         * and try again. */
        assert(t1 < (u + ulen));
        for (t1++; (t1 < (u + ulen)) && !HSPACE(*t1); t1++)
          ; // do nothing

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
 * rfc2047_encode - RFC-2047-encode a string
 * @param[in,out] pd       String to be encoded, and resulting encoded string
 * @param[in]     specials Special characters to be encoded
 * @param[in]     col      Starting index in string
 * @param[in]     charsets List of charsets to choose from
 */
void rfc2047_encode(char **pd, const char *specials, int col, const char *charsets)
{
  if (!C_Charset || !pd || !*pd)
    return;

  if (!charsets)
    charsets = "utf-8";

  char *e = NULL;
  size_t elen = 0;
  encode(*pd, strlen(*pd), col, C_Charset, charsets, &e, &elen, specials);

  FREE(pd);
  *pd = e;
}

/**
 * rfc2047_decode - Decode any RFC2047-encoded header fields
 * @param[in,out] pd  String to be decoded, and resulting decoded string
 *
 * Try to decode anything that looks like a valid RFC2047 encoded header field,
 * ignoring RFC822 parsing rules. If decoding fails, for example due to an
 * invalid base64 string, the original input is left untouched.
 */
void rfc2047_decode(char **pd)
{
  if (!pd || !*pd)
    return;

  struct Buffer buf = mutt_buffer_make(0); /* Output buffer            */
  char *s = *pd;            /* Read pointer                           */
  char *beg = NULL;         /* Begin of encoded word                  */
  enum ContentEncoding enc; /* ENC_BASE64 or ENC_QUOTED_PRINTABLE     */
  char *charset = NULL;     /* Which charset                          */
  size_t charsetlen;        /* Length of the charset                  */
  char *text = NULL;        /* Encoded text                           */
  size_t textlen;           /* Length of encoded text                 */

  /* Keep some state in case the next decoded word is using the same charset
   * and it happens to be split in the middle of a multibyte character.
   * See https://github.com/neomutt/neomutt/issues/1015 */
  struct Buffer prev = mutt_buffer_make(0); /* Previously decoded word  */
  char *prev_charset = NULL;  /* Previously used charset                */
  size_t prev_charsetlen = 0; /* Length of the previously used charset  */

  while (*s)
  {
    beg = parse_encoded_word(s, &enc, &charset, &charsetlen, &text, &textlen);
    if (beg != s)
    {
      /* Some non-encoded text was found */
      size_t holelen = beg ? beg - s : mutt_str_len(s);

      /* Ignore whitespace between encoded words */
      if (beg && (mutt_str_lws_len(s, holelen) == holelen))
      {
        s = beg;
        continue;
      }

      /* If we have some previously decoded text, add it now */
      if (!mutt_buffer_is_empty(&prev))
      {
        finalize_chunk(&buf, &prev, prev_charset, prev_charsetlen);
      }

      /* Add non-encoded part */
      {
        if (C_AssumedCharset)
        {
          char *conv = mutt_strn_dup(s, holelen);
          mutt_ch_convert_nonmime_string(&conv);
          mutt_buffer_addstr(&buf, conv);
          FREE(&conv);
        }
        else
        {
          mutt_buffer_addstr_n(&buf, s, holelen);
        }
      }
      s += holelen;
    }
    if (beg)
    {
      /* Some encoded text was found */
      text[textlen] = '\0';
      char *decoded = decode_word(text, textlen, enc);
      if (!decoded)
      {
        return;
      }
      if (prev.data && ((prev_charsetlen != charsetlen) ||
                        !mutt_strn_equal(prev_charset, charset, charsetlen)))
      {
        /* Different charset, convert the previous chunk and add it to the
         * final result */
        finalize_chunk(&buf, &prev, prev_charset, prev_charsetlen);
      }

      mutt_buffer_addstr(&prev, decoded);
      FREE(&decoded);
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
  FREE(pd);
  *pd = buf.data;
}

/**
 * rfc2047_encode_addrlist - Encode any RFC2047 headers, where required, in an Address list
 * @param al   AddressList
 * @param tag  Header tag (used for wrapping calculation)
 */
void rfc2047_encode_addrlist(struct AddressList *al, const char *tag)
{
  if (!al)
    return;

  int col = tag ? strlen(tag) + 2 : 32;
  struct Address *a = NULL;
  TAILQ_FOREACH(a, al, entries)
  {
    if (a->personal)
      rfc2047_encode(&a->personal, AddressSpecials, col, C_SendCharset);
    else if (a->group && a->mailbox)
      rfc2047_encode(&a->mailbox, AddressSpecials, col, C_SendCharset);
  }
}

/**
 * rfc2047_decode_addrlist - Decode any RFC2047 headers in an Address list
 * @param al AddressList
 */
void rfc2047_decode_addrlist(struct AddressList *al)
{
  if (!al)
    return;

  struct Address *a = NULL;
  TAILQ_FOREACH(a, al, entries)
  {
    if (a->personal && ((strstr(a->personal, "=?")) || C_AssumedCharset))
    {
      rfc2047_decode(&a->personal);
    }
    else if (a->group && a->mailbox && strstr(a->mailbox, "=?"))
      rfc2047_decode(&a->mailbox);
  }
}

/**
 * rfc2047_decode_envelope - Decode the fields of an Envelope
 * @param env Envelope
 */
void rfc2047_decode_envelope(struct Envelope *env)
{
  if (!env)
    return;
  rfc2047_decode_addrlist(&env->from);
  rfc2047_decode_addrlist(&env->to);
  rfc2047_decode_addrlist(&env->cc);
  rfc2047_decode_addrlist(&env->bcc);
  rfc2047_decode_addrlist(&env->reply_to);
  rfc2047_decode_addrlist(&env->mail_followup_to);
  rfc2047_decode_addrlist(&env->return_path);
  rfc2047_decode_addrlist(&env->sender);
  rfc2047_decode(&env->x_label);
  rfc2047_decode(&env->subject);
}

/**
 * rfc2047_encode_envelope - Encode the fields of an Envelope
 * @param env Envelope
 */
void rfc2047_encode_envelope(struct Envelope *env)
{
  if (!env)
    return;
  rfc2047_encode_addrlist(&env->from, "From");
  rfc2047_encode_addrlist(&env->to, "To");
  rfc2047_encode_addrlist(&env->cc, "Cc");
  rfc2047_encode_addrlist(&env->bcc, "Bcc");
  rfc2047_encode_addrlist(&env->reply_to, "Reply-To");
  rfc2047_encode_addrlist(&env->mail_followup_to, "Mail-Followup-To");
  rfc2047_encode_addrlist(&env->sender, "Sender");
  rfc2047_encode(&env->x_label, NULL, sizeof("X-Label:"), C_SendCharset);
  rfc2047_encode(&env->subject, NULL, sizeof("Subject:"), C_SendCharset);
}
