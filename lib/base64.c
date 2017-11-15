/**
 * @file
 * Conversion to/from base64 encoding
 *
 * @authors
 * Copyright (C) 1993,1995 Carl Harris
 * Copyright (C) 1997 Eric S. Raymond
 * Copyright (C) 1999 Brendan Cully <brendan@kublai.com>
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
 * @page base64 Conversion to/from base64 encoding
 *
 * Convert between binary data and base64 text, according to RFC2045.
 *
 * @note RFC3548 obsoletes RFC2045.
 * @note RFC4648 obsoletes RFC3548.
 *
 * | Data               | Description
 * | :----------------- | :--------------------------------------------------
 * | #Index_64          | Lookup table for Base64 encoding characters
 *
 * | Function           | Description
 * | :----------------- | :-------------------------------------------------
 * | mutt_from_base64() | convert null-terminated base64 string to raw bytes
 * | mutt_to_base64()   | convert raw bytes to null-terminated base64 string
 */

#include "config.h"
#include "base64.h"

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

// clang-format off
/**
 * Index_64 - Lookup table for Base64 encoding characters
 *
 * @note This is very similar to the table in imap/utf7.c
 *
 * Encoding chars:
 * * utf7 A-Za-z0-9+,
 * * mime A-Za-z0-9+/
 */
const int Index_64[128] = {
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,
    52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1,-1,-1,-1,
    -1, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
    15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1,
    -1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
    41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1
};
// clang-format on

/**
 * mutt_to_base64 - convert raw bytes to null-terminated base64 string
 * @param out  Output buffer for the base64 encoded string
 * @param cin  Input  buffer for the raw bytes
 * @param len  Length of the input buffer
 * @param olen Length of the output buffer
 * @retval n Length of the string written to the output buffer
 *
 * This function performs base64 encoding. The resulting string is guaranteed
 * to be null-terminated. The number of characters up to the terminating
 * null-byte is returned (equivalent to calling strlen() on the output buffer
 * after this function returns).
 */
size_t mutt_to_base64(char *out, const char *cin, size_t len, size_t olen)
{
  unsigned char *begin = (unsigned char *) out;
  const unsigned char *in = (const unsigned char *) cin;

  while ((len >= 3) && (olen > 10))
  {
    *out++ = B64Chars[in[0] >> 2];
    *out++ = B64Chars[((in[0] << 4) & 0x30) | (in[1] >> 4)];
    *out++ = B64Chars[((in[1] << 2) & 0x3c) | (in[2] >> 6)];
    *out++ = B64Chars[in[2] & 0x3f];
    olen -= 4;
    len -= 3;
    in += 3;
  }

  /* clean up remainder */
  if ((len > 0) && (olen > 4))
  {
    unsigned char fragment;

    *out++ = B64Chars[in[0] >> 2];
    fragment = (in[0] << 4) & 0x30;
    if (len > 1)
      fragment |= in[1] >> 4;
    *out++ = B64Chars[fragment];
    *out++ = (len < 2) ? '=' : B64Chars[(in[1] << 2) & 0x3c];
    *out++ = '=';
  }
  *out = '\0';
  return out - (char *) begin;
}

/**
 * mutt_from_base64 - convert null-terminated base64 string to raw bytes
 * @param out Output buffer for the raw bytes
 * @param in  Input  buffer for the null-terminated base64-encoded string
 * @retval n Number of bytes written on success
 * @retval -1 on error
 *
 * This function performs base64 decoding. The resulting buffer is NOT
 * null-terminated. If the input buffer contains invalid base64 characters,
 * this function returns -1.
 */
int mutt_from_base64(char *out, const char *in)
{
  int len = 0;
  unsigned char digit1, digit2, digit3, digit4;

  do
  {
    digit1 = in[0];
    if ((digit1 > 127) || (base64val(digit1) == BAD))
      return -1;
    digit2 = in[1];
    if ((digit2 > 127) || (base64val(digit2) == BAD))
      return -1;
    digit3 = in[2];
    if ((digit3 > 127) || ((digit3 != '=') && (base64val(digit3) == BAD)))
      return -1;
    digit4 = in[3];
    if ((digit4 > 127) || ((digit4 != '=') && (base64val(digit4) == BAD)))
      return -1;
    in += 4;

    /* digits are already sanity-checked */
    *out++ = (base64val(digit1) << 2) | (base64val(digit2) >> 4);
    len++;
    if (digit3 != '=')
    {
      *out++ = ((base64val(digit2) << 4) & 0xf0) | (base64val(digit3) >> 2);
      len++;
      if (digit4 != '=')
      {
        *out++ = ((base64val(digit3) << 6) & 0xc0) | base64val(digit4);
        len++;
      }
    }
  } while (*in && digit4 != '=');

  return len;
}
