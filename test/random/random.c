/**
 * @file
 * Test code for the libmutt random functions
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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

#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include <stddef.h>
#include "mutt/lib.h"

void test_mutt_rand32(void)
{
  // uint32_t mutt_rand32(void);

  {
    // Every return value is valid, so there's no point checking it
    mutt_rand32();
  }
}

void test_mutt_rand64(void)
{
  // uint64_t mutt_rand64(void);

  {
    // Every return value is valid, so there's no point checking it
    mutt_rand64();
  }
}

void test_mutt_rand_base32(void)
{
  // void mutt_rand_base32(char *buf, size_t buflen);

  {
    // Degenerate tests
    mutt_rand_base32(NULL, 64);
    mutt_rand_base32(NULL, 0);

    // This causes mutt_randbuf() to fail and mutt_exit() to be called
    // char buf[64] = { 0 };
    // mutt_rand_base32(buf, 10485760);
  }

  {
    char buf[64] = { 0 };
    mutt_rand_base32(buf, sizeof(buf));
  }
}
