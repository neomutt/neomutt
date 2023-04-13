/**
 * @file
 * Test code for mutt_mktemp_full()
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
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "test_common.h"

void test_mutt_mktemp_full(void)
{
  // void mutt_mktemp_full(char *buf, size_t buflen, const char *prefix, const char *suffix, const char *src, int line);

  test_neomutt_create();

  {
    char buf[256] = { 0 };
    mutt_mktemp_full(buf, sizeof(buf), NULL, NULL, __FILE__, __LINE__);
  }

  test_neomutt_destroy();
}
