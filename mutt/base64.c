/**
 * @file
 * Conversion to/from base64 encoding
 *
 * @authors
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2022 Pietro Cerutti <gahr@gahr.ch>
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
 * @page mutt_base64 Conversion to/from base64 encoding
 *
 * Convert between binary data and base64 text, according to RFC2045.
 *
 * @note RFC3548 obsoletes RFC2045.
 * @note RFC4648 obsoletes RFC3548.
 */

#include "config.h"
#include "base64.h"
#include "buffer.h"
#include "memory.h"
#include "string2.h"

#define BAD -1

/**
 * B64Chars - Characters of the Base64 encoding
 */
static const char B64Chars[64] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
  'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/',
};

/**
 * Index64 - Lookup table for Base64 encoding characters
 *
 * @note This is very similar to the table in imap/utf7.c
 *
 * Encoding chars:
 * * utf7 A-Za-z0-9+,
 * * mime A-Za-z0-9+/
 */
const int Index64[128] = {
  // clang-format off
  -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
  -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
  -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,
  52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1,-1,-1,-1,
  -1, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
  15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1,
  -1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
  41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1
  // clang-format on
};

/**
 * mutt_b64_encode - Convert raw bytes to null-terminated base64 string
 * @param in     Input buffer for the raw bytes
 * @param inlen  Length of the input buffer
 * @param out    Output buffer for the base64 encoded string
 * @param outlen Length of the output buffer
 * @retval num Length of the string written to the output buffer
 *
 * This function performs base64 encoding. The resulting string is guaranteed
 * to be null-terminated. The number of characters up to the terminating
 * NUL-byte is returned (equivalent to calling strlen() on the output buffer
 * after this function returns).
 */
size_t mutt_b64_encode(const char *in, size_t inlen, char *out, size_t outlen)
{
  if (!in || !out)
    return 0;

  unsigned char *begin = (unsigned char *) out;
  const unsigned char *inu = (const unsigned char *) in;

  while ((inlen >= 3) && (outlen > 4))
  {
    *out++ = B64Chars[inu[0] >> 2];
    *out++ = B64Chars[((inu[0] << 4) & 0x30) | (inu[1] >> 4)];
    *out++ = B64Chars[((inu[1] << 2) & 0x3c) | (inu[2] >> 6)];
    *out++ = B64Chars[inu[2] & 0x3f];
    outlen -= 4;
    inlen -= 3;
    inu += 3;
  }

  /* clean up remainder */
  if ((inlen > 0) && (outlen > 4))
  {
    unsigned char fragment;

    *out++ = B64Chars[inu[0] >> 2];
    fragment = (inu[0] << 4) & 0x30;
    if (inlen > 1)
      fragment |= inu[1] >> 4;
    *out++ = B64Chars[fragment];
    *out++ = (inlen < 2) ? '=' : B64Chars[(inu[1] << 2) & 0x3c];
    *out++ = '=';
  }
  *out = '\0';
  return out - (char *) begin;
}

/**
 * mutt_b64_decode - Convert null-terminated base64 string to raw bytes
 * @param in   Input  buffer for the null-terminated base64-encoded string
 * @param out  Output buffer for the raw bytes
 * @param olen Length of the output buffer
 * @retval num Success, bytes written
 * @retval -1  Error
 *
 * This function performs base64 decoding. The resulting buffer is NOT
 * null-terminated. If the input buffer contains invalid base64 characters,
 * this function returns -1.
 */
int mutt_b64_decode(const char *in, char *out, size_t olen)
{
  if (!in || !*in || !out)
    return -1;

  int len = 0;

  for (; *in; in += 4)
  {
    const unsigned char digit1 = in[0];
    if ((digit1 > 127) || (base64val(digit1) == BAD))
      return -1;
    const unsigned char digit2 = in[1];
    if ((digit2 > 127) || (base64val(digit2) == BAD))
      return -1;

    /* The 3rd and 4th bytes can be terminating padding chars ('='). Some
     * mailers don't properly terminate base64-encoded strings, so we allow for
     * the input string to terminate without padding. */
    const unsigned char digit3 = in[2] ? in[2] : '=';
    if ((digit3 > 127) || ((digit3 != '=') && (base64val(digit3) == BAD)))
      return -1;
    const unsigned char digit4 = (digit3 == '=') ? '=' : in[3];
    if ((digit4 > 127) || ((digit4 != '=') && (base64val(digit4) == BAD)))
      return -1;

    /* digits are already sanity-checked */
    if (len == olen)
      return len;
    *out++ = (base64val(digit1) << 2) | (base64val(digit2) >> 4);
    len++;
    if (digit3 != '=')
    {
      if (len == olen)
        return len;
      *out++ = ((base64val(digit2) << 4) & 0xf0) | (base64val(digit3) >> 2);
      len++;
      if (digit4 != '=')
      {
        if (len == olen)
          return len;
        *out++ = ((base64val(digit3) << 6) & 0xc0) | base64val(digit4);
        len++;
      }
    }

    /* did we reach the end? */
    if (digit4 == '=')
    {
      break;
    }
  }

  return len;
}

/**
 * mutt_b64_buffer_encode - Convert raw bytes to null-terminated base64 string
 * @param buf    Buffer for the result
 * @param in     Input buffer for the raw bytes
 * @param len    Length of the input buffer
 * @retval num Length of the string written to the output buffer
 */
size_t mutt_b64_buffer_encode(struct Buffer *buf, const char *in, size_t len)
{
  if (!buf)
    return 0;

  buf_alloc(buf, MAX((len * 2), 1024));
  size_t num = mutt_b64_encode(in, len, buf->data, buf->dsize);
  buf_fix_dptr(buf);
  return num;
}

/**
 * mutt_b64_buffer_decode - Convert null-terminated base64 string to raw bytes
 * @param buf  Buffer for the result
 * @param in   Input  buffer for the null-terminated base64-encoded string
 * @retval num Success, bytes written
 * @retval -1  Error
 */
int mutt_b64_buffer_decode(struct Buffer *buf, const char *in)
{
  if (!buf)
    return -1;

  buf_alloc(buf, mutt_str_len(in));
  int olen = mutt_b64_decode(in, buf->data, buf->dsize);
  // mutt_b64_decode returns raw bytes, so don't terminate the buffer either
  if (olen > 0)
    buf_seek(buf, olen);
  else
    buf_seek(buf, 0);

  return olen;
}
