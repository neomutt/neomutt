/**
 * @file
 * For systems lacking wide character functions
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

#include "config.h"
#include <stddef.h>
#include <errno.h>

#ifndef EILSEQ
#define EILSEQ EINVAL
#endif

int mutt_wctoutf8(char *s, unsigned int c, size_t buflen)
{
  if (c < (1 << 7))
  {
    if (s && buflen >= 1)
      *s++ = c;
    return 1;
  }
  else if (c < (1 << 11))
  {
    if (s && buflen >= 2)
    {
      *s++ = 0xc0 | (c >> 6);
      *s++ = 0x80 | (c & 0x3f);
    }
    return 2;
  }
  else if (c < (1 << 16))
  {
    if (s && buflen >= 3)
    {
      *s++ = 0xe0 | (c >> 12);
      *s++ = 0x80 | ((c >> 6) & 0x3f);
      *s++ = 0x80 | (c & 0x3f);
    }
    return 3;
  }
  else if (c < (1 << 21))
  {
    if (s && buflen >= 4)
    {
      *s++ = 0xf0 | (c >> 18);
      *s++ = 0x80 | ((c >> 12) & 0x3f);
      *s++ = 0x80 | ((c >> 6) & 0x3f);
      *s++ = 0x80 | (c & 0x3f);
    }
    return 4;
  }
  else if (c < (1 << 26))
  {
    if (s && buflen >= 5)
    {
      *s++ = 0xf8 | (c >> 24);
      *s++ = 0x80 | ((c >> 18) & 0x3f);
      *s++ = 0x80 | ((c >> 12) & 0x3f);
      *s++ = 0x80 | ((c >> 6) & 0x3f);
      *s++ = 0x80 | (c & 0x3f);
    }
    return 5;
  }
  else if (c < (1 << 31))
  {
    if (s && buflen >= 6)
    {
      *s++ = 0xfc | (c >> 30);
      *s++ = 0x80 | ((c >> 24) & 0x3f);
      *s++ = 0x80 | ((c >> 18) & 0x3f);
      *s++ = 0x80 | ((c >> 12) & 0x3f);
      *s++ = 0x80 | ((c >> 6) & 0x3f);
      *s++ = 0x80 | (c & 0x3f);
    }
    return 6;
  }
  errno = EILSEQ;
  return -1;
}
