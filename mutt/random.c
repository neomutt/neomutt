/**
 * @file
 * Random number/string functions
 *
 * @authors
 * Copyright (C) 2020-2023 Richard Russon <rich@flatcap.org>
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
 * @page mutt_random Random number/string functions
 *
 * Random number/string functions
 */

#include "config.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "random.h"
#include "exit.h"
#include "file.h"
#include "logging2.h"
#include "message.h"
#ifdef HAVE_SYS_RANDOM_H
#include <sys/random.h>
#endif

/// FILE pointer of the random source
static FILE *FpRandom = NULL;

/// Base 32 alphabet
static const unsigned char Base32[] = "abcdefghijklmnopqrstuvwxyz234567";

/**
 * mutt_randbuf - Fill a buffer with randomness
 * @param buf    Buffer for result
 * @param buflen Size of buffer
 * @retval  0 Success
 * @retval -1 Error
 */
static int mutt_randbuf(void *buf, size_t buflen)
{
  if (buflen > 1048576)
  {
    mutt_error(_("mutt_randbuf buflen=%zu"), buflen);
    return -1;
  }

#ifdef HAVE_GETRANDOM
  ssize_t rc;
  ssize_t count = 0;
  do
  {
    // getrandom() can return less than requested if there's insufficient
    // entropy or it's interrupted by a signal.
    rc = getrandom((char *) buf + count, buflen - count, 0);
    if (rc > 0)
      count += rc;
  } while (((rc >= 0) && (count < buflen)) || ((rc == -1) && (errno == EINTR)));
  if (count == buflen)
    return 0;
#endif
  /* let's try urandom in case we're on an old kernel, or the user has
   * configured selinux, seccomp or something to not allow getrandom */
  if (!FpRandom)
  {
    FpRandom = mutt_file_fopen("/dev/urandom", "rb");
    if (!FpRandom)
    {
      mutt_error(_("open /dev/urandom: %s"), strerror(errno));
      return -1;
    }
    setbuf(FpRandom, NULL);
  }
  if (fread(buf, 1, buflen, FpRandom) != buflen)
  {
    mutt_error(_("read /dev/urandom: %s"), strerror(errno));
    return -1;
  }

  return 0;
}

/**
 * mutt_rand_base32 - Fill a buffer with a base32-encoded random string
 * @param buf    Buffer for result
 * @param buflen Length of buffer
 */
void mutt_rand_base32(char *buf, size_t buflen)
{
  if (!buf || (buflen == 0))
    return;

  uint8_t *p = (uint8_t *) buf;

  if (mutt_randbuf(p, buflen) < 0)
    mutt_exit(1); // LCOV_EXCL_LINE
  for (size_t pos = 0; pos < buflen; pos++)
    p[pos] = Base32[p[pos] % 32];
}

/**
 * mutt_rand64 - Create a 64-bit random number
 * @retval num Random number
 */
uint64_t mutt_rand64(void)
{
  uint64_t num = 0;

  if (mutt_randbuf(&num, sizeof(num)) < 0)
    mutt_exit(1); // LCOV_EXCL_LINE
  return num;
}
