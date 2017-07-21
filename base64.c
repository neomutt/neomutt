/**
 * @file
 * Conversion to/from base64 encoding
 *
 * @authors
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

/*
 * Base64 handling elsewhere in mutt should be modified to call
 * these routines. These routines were written because IMAP's
 * AUTHENTICATE protocol required them, and base64 handling
 * elsewhere wasn't sufficiently generic.
 */

/*
 * This code is heavily modified from fetchmail (also GPL'd, of
 * course) by Brendan Cully <brendan@kublai.com>.
 *
 * Original copyright notice:
 *
 * The code in the fetchmail distribution is Copyright 1997 by Eric
 * S. Raymond.  Portions are also copyrighted by Carl Harris, 1993
 * and 1995.  Copyright retained for the purpose of protecting free
 * redistribution of source.
 */

#include "config.h"
#include <stddef.h>
#include "mime.h"

#define BAD -1

static const char B64Chars[64] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
  'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/',
};

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
  while (len >= 3 && olen > 10)
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
  if (len > 0 && olen > 4)
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
  register unsigned char digit1, digit2, digit3, digit4;

  do
  {
    digit1 = in[0];
    if (digit1 > 127 || base64val(digit1) == BAD)
      return -1;
    digit2 = in[1];
    if (digit2 > 127 || base64val(digit2) == BAD)
      return -1;
    digit3 = in[2];
    if (digit3 > 127 || ((digit3 != '=') && (base64val(digit3) == BAD)))
      return -1;
    digit4 = in[3];
    if (digit4 > 127 || ((digit4 != '=') && (base64val(digit4) == BAD)))
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
